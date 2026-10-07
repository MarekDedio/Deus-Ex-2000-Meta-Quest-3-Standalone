#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace QuestVr {
inline constexpr std::size_t kMaximumSaveMetadataBytes = 64u * 1024u;
inline constexpr std::size_t kMaximumSaveRuntimeBytes = 16u * 1024u * 1024u;
inline constexpr std::size_t kSaveBundleHeaderBytes = 48u;
inline constexpr std::size_t kMaximumSaveBundleBytes =
    kSaveBundleHeaderBytes + kMaximumSaveMetadataBytes + kMaximumSaveRuntimeBytes;

struct SaveBundleData {
    std::uint64_t generation{};
    std::vector<std::uint8_t> metadata;
    std::vector<std::uint8_t> runtime;
    std::uint32_t slotIndex{}; // Storage provenance; not part of the wire format.
};
struct SaveBundlePaths { std::string slot0, slot1, staging; };
struct SaveBundlePublishResult { bool saved{}; std::uint64_t generation{}; std::uint32_t slotIndex{}; };
struct SaveBundleIo {
    void* context{};
    bool (*writeDurable)(void*, const std::string&, const std::vector<std::uint8_t>&){};
    bool (*publishAtomic)(void*, const std::string&, const std::string&){};
};

namespace SaveBundleDetail {
inline std::filesystem::path Utf8Path(const std::string& value) {
#if defined(__cpp_char8_t)
    return std::filesystem::path(reinterpret_cast<const char8_t*>(value.c_str()));
#elif defined(_WIN32)
    if (value.empty()) return {};
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::filesystem::filesystem_error("UTF-8 save path too long", std::make_error_code(std::errc::invalid_argument));
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) throw std::filesystem::filesystem_error("Invalid UTF-8 save path",
        std::make_error_code(std::errc::invalid_argument));
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), wide.data(), length) != length)
        throw std::filesystem::filesystem_error("Invalid UTF-8 save path",
            std::make_error_code(std::errc::invalid_argument));
    return std::filesystem::path(wide);
#else
    // POSIX uses UTF-8 bytes directly; avoid deprecated u8path on new compilers.
    return std::filesystem::path(value.begin(), value.end());
#endif
}
inline constexpr std::array<std::uint8_t, 8> magic{{'D','X','Q','S','V','B','0','1'}};
inline std::uint32_t Crc32(const std::uint8_t* data, const std::size_t size) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> values{};
        for (std::uint32_t i = 0; i < 256u; ++i) {
            auto crc = i;
            for (unsigned bit = 0; bit < 8u; ++bit) crc = (crc >> 1u) ^ ((crc & 1u) ? 0xedb88320u : 0u);
            values[i] = crc;
        }
        return values;
    }();
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i) crc = (crc >> 8u) ^ table[(crc ^ data[i]) & 255u];
    return crc ^ 0xffffffffu;
}
inline void Put32(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint32_t value) {
    for (unsigned i = 0; i < 4u; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
}
inline std::uint32_t Get32(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
    std::uint32_t value{};
    for (unsigned i = 0; i < 4u; ++i) value |= static_cast<std::uint32_t>(bytes[offset + i]) << (i * 8u);
    return value;
}
inline bool SafeFilePath(const std::string& path, std::filesystem::path* parent = nullptr) {
    if (path.empty() || path.size() > 32'000u || path.find('\0') != std::string::npos) return false;
    const auto absolute = std::filesystem::absolute(Utf8Path(path)).lexically_normal();
    if (absolute.filename().empty() || absolute.filename() == "." || absolute.filename() == "..") return false;
    const auto directory = std::filesystem::canonical(absolute.parent_path());
    if (!std::filesystem::is_directory(directory)) return false;
    std::error_code error;
    const auto status = std::filesystem::symlink_status(absolute, error);
    if (error && error != std::errc::no_such_file_or_directory) return false;
    if (!error && std::filesystem::exists(status)) {
        if (!std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status) ||
            std::filesystem::hard_link_count(absolute) != 1u) return false;
#ifdef _WIN32
        const auto attributes = GetFileAttributesW(absolute.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) return false;
#endif
    }
    if (parent != nullptr) *parent = directory;
    return true;
}
inline bool ValidBundlePaths(const SaveBundlePaths& paths) {
    static const std::string suffix = ".slot0.qsv";
    if (paths.slot0.size() <= suffix.size() ||
        paths.slot0.compare(paths.slot0.size() - suffix.size(), suffix.size(), suffix) != 0) return false;
    const auto prefix = paths.slot0.substr(0, paths.slot0.size() - suffix.size());
    if (paths.slot1 != prefix + ".slot1.qsv" || paths.staging != prefix + ".stage.qsv.tmp") return false;
    // Parent resolution catches junction/symlink ancestors; the actual targets
    // are application-owned leaf files only. No directory creation or deletion.
    std::filesystem::path parent0, parent1, stageParent;
    const auto parentFor = [](const std::string& path) {
        return std::filesystem::canonical(std::filesystem::absolute(Utf8Path(path)).parent_path());
    };
    parent0 = parentFor(paths.slot0); parent1 = parentFor(paths.slot1); stageParent = parentFor(paths.staging);
    return std::filesystem::equivalent(parent0, parent1) && std::filesystem::equivalent(parent0, stageParent);
}
} // namespace SaveBundleDetail

inline SaveBundlePaths MakeSaveBundlePaths(const std::string& prefix) noexcept {
    try { return {prefix + ".slot0.qsv", prefix + ".slot1.qsv", prefix + ".stage.qsv.tmp"}; }
    catch (...) { return {}; }
}

inline bool EncodeSaveBundle(const SaveBundleData& data, std::vector<std::uint8_t>& bytes) noexcept {
    try {
        bytes.clear();
        if (data.generation == 0u || data.generation == std::numeric_limits<std::uint64_t>::max() ||
            data.metadata.empty() || data.metadata.size() > kMaximumSaveMetadataBytes ||
            data.runtime.empty() || data.runtime.size() > kMaximumSaveRuntimeBytes) return false;
        bytes.resize(kSaveBundleHeaderBytes + data.metadata.size() + data.runtime.size(), 0u);
        std::copy(SaveBundleDetail::magic.begin(), SaveBundleDetail::magic.end(), bytes.begin());
        SaveBundleDetail::Put32(bytes, 8u, 1u);
        SaveBundleDetail::Put32(bytes, 12u, static_cast<std::uint32_t>(kSaveBundleHeaderBytes));
        SaveBundleDetail::Put32(bytes, 16u, static_cast<std::uint32_t>(data.generation));
        SaveBundleDetail::Put32(bytes, 20u, static_cast<std::uint32_t>(data.generation >> 32u));
        SaveBundleDetail::Put32(bytes, 24u, static_cast<std::uint32_t>(data.metadata.size()));
        SaveBundleDetail::Put32(bytes, 28u, static_cast<std::uint32_t>(data.runtime.size()));
        SaveBundleDetail::Put32(bytes, 32u, SaveBundleDetail::Crc32(data.metadata.data(), data.metadata.size()));
        SaveBundleDetail::Put32(bytes, 36u, SaveBundleDetail::Crc32(data.runtime.data(), data.runtime.size()));
        SaveBundleDetail::Put32(bytes, 40u, SaveBundleDetail::Crc32(bytes.data(), 40u));
        std::copy(data.metadata.begin(), data.metadata.end(), bytes.begin() + kSaveBundleHeaderBytes);
        std::copy(data.runtime.begin(), data.runtime.end(), bytes.begin() + kSaveBundleHeaderBytes + data.metadata.size());
        return true;
    } catch (...) { bytes.clear(); return false; }
}

inline bool DecodeSaveBundle(const std::vector<std::uint8_t>& bytes, SaveBundleData& data) noexcept {
    try {
        data = {};
        if (bytes.size() < kSaveBundleHeaderBytes || bytes.size() > kMaximumSaveBundleBytes ||
            !std::equal(SaveBundleDetail::magic.begin(), SaveBundleDetail::magic.end(), bytes.begin()) ||
            SaveBundleDetail::Get32(bytes, 8u) != 1u || SaveBundleDetail::Get32(bytes, 12u) != kSaveBundleHeaderBytes ||
            SaveBundleDetail::Get32(bytes, 44u) != 0u ||
            SaveBundleDetail::Get32(bytes, 40u) != SaveBundleDetail::Crc32(bytes.data(), 40u)) return false;
        const auto generation = static_cast<std::uint64_t>(SaveBundleDetail::Get32(bytes, 16u)) |
            (static_cast<std::uint64_t>(SaveBundleDetail::Get32(bytes, 20u)) << 32u);
        const auto metadataLength = SaveBundleDetail::Get32(bytes, 24u);
        const auto runtimeLength = SaveBundleDetail::Get32(bytes, 28u);
        if (generation == 0u || generation == std::numeric_limits<std::uint64_t>::max() ||
            metadataLength == 0u || metadataLength > kMaximumSaveMetadataBytes ||
            runtimeLength == 0u || runtimeLength > kMaximumSaveRuntimeBytes ||
            static_cast<std::uint64_t>(kSaveBundleHeaderBytes) + metadataLength + runtimeLength != bytes.size() ||
            SaveBundleDetail::Get32(bytes, 32u) != SaveBundleDetail::Crc32(bytes.data() + kSaveBundleHeaderBytes, metadataLength) ||
            SaveBundleDetail::Get32(bytes, 36u) != SaveBundleDetail::Crc32(bytes.data() + kSaveBundleHeaderBytes + metadataLength, runtimeLength)) return false;
        data.generation = generation;
        data.metadata.assign(bytes.begin() + kSaveBundleHeaderBytes, bytes.begin() + kSaveBundleHeaderBytes + metadataLength);
        data.runtime.assign(bytes.begin() + kSaveBundleHeaderBytes + metadataLength, bytes.end());
        return true;
    } catch (...) { data = {}; return false; }
}

// Call only for explicitly application-owned capture/load staging files.
inline bool ReadBoundedSaveFile(const std::string& path, const std::size_t maxBytes,
    std::vector<std::uint8_t>& bytes) noexcept {
    try {
        bytes.clear();
        if (maxBytes == 0u || maxBytes > kMaximumSaveBundleBytes || !SaveBundleDetail::SafeFilePath(path)) return false;
        std::ifstream stream(SaveBundleDetail::Utf8Path(path), std::ios::binary | std::ios::ate);
        if (!stream) return false;
        const auto size = stream.tellg();
        if (size < 0 || static_cast<std::uint64_t>(size) > maxBytes) return false;
        bytes.resize(static_cast<std::size_t>(size));
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!stream || stream.peek() != std::char_traits<char>::eof()) { bytes.clear(); return false; }
        return true;
    } catch (...) { bytes.clear(); return false; }
}

inline bool WriteDurableSaveFile(const std::string& path, const std::vector<std::uint8_t>& bytes) noexcept {
    try {
        if (bytes.empty() || bytes.size() > kMaximumSaveBundleBytes || !SaveBundleDetail::SafeFilePath(path)) return false;
#ifdef _WIN32
        const auto native = SaveBundleDetail::Utf8Path(path);
        const auto file = CreateFileW(native.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        BY_HANDLE_FILE_INFORMATION info{};
        bool ok = GetFileInformationByHandle(file, &info) != 0 && info.nNumberOfLinks == 1u &&
            (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0u;
        if (ok) ok = SetEndOfFile(file) != 0;
        std::size_t offset{};
        while (ok && offset < bytes.size()) {
            DWORD written{};
            const auto count = static_cast<DWORD>(std::min(bytes.size() - offset, static_cast<std::size_t>(1u << 30u)));
            ok = WriteFile(file, bytes.data() + offset, count, &written, nullptr) != 0 && written != 0u;
            offset += written;
        }
        if (ok) ok = FlushFileBuffers(file) != 0;
        const bool closed = CloseHandle(file) != 0;
        return ok && closed;
#else
        const int file = open(path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (file < 0) return false;
        struct stat info{};
        bool ok = fstat(file, &info) == 0 && S_ISREG(info.st_mode) && info.st_nlink == 1;
        if (ok) ok = ftruncate(file, 0) == 0;
        std::size_t offset{};
        while (ok && offset < bytes.size()) {
            const auto written = write(file, bytes.data() + offset, bytes.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) { ok = false; break; }
            offset += static_cast<std::size_t>(written);
        }
        if (ok) ok = fsync(file) == 0;
        const bool closed = close(file) == 0;
        return ok && closed;
#endif
    } catch (...) { return false; }
}

inline bool PublishSaveBundleFileAtomically(const std::string& staging, const std::string& target) noexcept {
    try {
        std::filesystem::path stageParent, targetParent;
        if (!SaveBundleDetail::SafeFilePath(staging, &stageParent) ||
            !SaveBundleDetail::SafeFilePath(target, &targetParent) ||
            !std::filesystem::equivalent(stageParent, targetParent) ||
            (std::filesystem::exists(SaveBundleDetail::Utf8Path(target)) &&
             std::filesystem::equivalent(SaveBundleDetail::Utf8Path(staging), SaveBundleDetail::Utf8Path(target)))) return false;
#ifdef _WIN32
        return MoveFileExW(SaveBundleDetail::Utf8Path(staging).c_str(), SaveBundleDetail::Utf8Path(target).c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        const int directory = open(stageParent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (directory < 0) return false;
        const bool renamed = rename(staging.c_str(), target.c_str()) == 0;
        // If fsync fails after rename, report failure but retain both complete
        // slots. The prior committed generation is never the replaced target.
        const bool durable = renamed && fsync(directory) == 0;
        const bool closed = close(directory) == 0;
        return durable && closed;
#endif
    } catch (...) { return false; }
}

inline std::vector<SaveBundleData> LoadSaveBundleCandidates(const SaveBundlePaths& paths) noexcept {
    try {
        std::vector<SaveBundleData> candidates;
        if (!SaveBundleDetail::ValidBundlePaths(paths)) return candidates;
        const std::string* slots[] = {&paths.slot0, &paths.slot1};
        for (std::uint32_t i = 0; i < 2u; ++i) {
            std::vector<std::uint8_t> bytes;
            SaveBundleData data;
            if (ReadBoundedSaveFile(*slots[i], kMaximumSaveBundleBytes, bytes) && DecodeSaveBundle(bytes, data)) {
                data.slotIndex = i;
                candidates.push_back(std::move(data));
            }
        }
        std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.generation > b.generation; });
        return candidates;
    } catch (...) { return {}; }
}

inline SaveBundlePublishResult PublishSaveBundle(const SaveBundlePaths& paths,
    const std::vector<std::uint8_t>& metadata, const std::vector<std::uint8_t>& runtime,
    const SaveBundleIo hooks = {}) noexcept {
    try {
        if (!SaveBundleDetail::ValidBundlePaths(paths) || !SaveBundleDetail::SafeFilePath(paths.staging)) return {};
        const auto candidates = LoadSaveBundleCandidates(paths);
        const auto previous = candidates.empty() ? 0u : candidates.front().generation;
        if (previous >= std::numeric_limits<std::uint64_t>::max() - 1u) return {};
        const auto slot = candidates.empty() ? 0u : (1u - candidates.front().slotIndex);
        const auto& target = slot == 0u ? paths.slot0 : paths.slot1;
        if (!SaveBundleDetail::SafeFilePath(target)) return {};
        std::vector<std::uint8_t> bytes;
        SaveBundleData data{previous + 1u, metadata, runtime, slot};
        if (!EncodeSaveBundle(data, bytes)) return {};
        const bool written = hooks.writeDurable != nullptr
            ? hooks.writeDurable(hooks.context, paths.staging, bytes) : WriteDurableSaveFile(paths.staging, bytes);
        if (!written) return {};
        // Verify staged bytes, including both payload checksums, before making
        // the inactive slot visible. Interrupted/unpublished stages are ignored.
        std::vector<std::uint8_t> stagedBytes;
        SaveBundleData staged;
        if (!ReadBoundedSaveFile(paths.staging, kMaximumSaveBundleBytes, stagedBytes) ||
            !DecodeSaveBundle(stagedBytes, staged) || stagedBytes != bytes) return {};
        const bool published = hooks.publishAtomic != nullptr
            ? hooks.publishAtomic(hooks.context, paths.staging, target) : PublishSaveBundleFileAtomically(paths.staging, target);
        if (!published) return {};
        return {true, data.generation, slot};
    } catch (...) { return {}; }
}
} // namespace QuestVr
