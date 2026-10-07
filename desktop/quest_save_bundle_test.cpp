#include "quest_save_bundle.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using Bytes = std::vector<std::uint8_t>;
void Require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
struct FixtureDirectory {
    std::filesystem::path root;
    std::vector<std::filesystem::path> targets;
    FixtureDirectory() {
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned i = 0; i < 20u; ++i) {
            const auto path = parent / ("deusex-save-bundle-test-" + std::to_string(stamp) + '-' + std::to_string(i));
            if (std::filesystem::create_directory(path)) {
                root = std::filesystem::canonical(path);
                Require(root.parent_path() == parent && root.filename().string().rfind("deusex-save-bundle-test-", 0) == 0,
                        "Fixture escaped its newly created temporary parent");
                return;
            }
        }
        throw std::runtime_error("Could not create isolated bundle test directory");
    }
    QuestVr::SaveBundlePaths Paths(const char* prefix) {
        const auto paths = QuestVr::MakeSaveBundlePaths((root / prefix).string());
        for (const auto* path : {&paths.slot0, &paths.slot1, &paths.staging}) targets.emplace_back(*path);
        return paths;
    }
    std::string File(const char* filename) {
        const auto path = root / filename;
        targets.push_back(path);
        return path.string();
    }
    ~FixtureDirectory() {
        std::error_code ignored;
        // Remove only recorded, generated leaf targets; no recursive folder op.
        for (auto it = targets.rbegin(); it != targets.rend(); ++it) std::filesystem::remove(*it, ignored);
        std::filesystem::remove(root, ignored);
    }
};
Bytes Read(const std::string& path) {
    Bytes bytes;
    Require(QuestVr::ReadBoundedSaveFile(path, QuestVr::kMaximumSaveBundleBytes, bytes), "Generated file read failed");
    return bytes;
}
Bytes RawFixtureRead(const std::string& path) {
    // Used only while deliberately constructing a hard-link rejection fixture.
    std::ifstream stream(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
    Require(static_cast<bool>(stream), "Generated hard-link fixture read failed");
    const auto size = stream.tellg();
    Require(size >= 0 && static_cast<std::uint64_t>(size) <= QuestVr::kMaximumSaveBundleBytes, "Fixture read budget exceeded");
    Bytes bytes(static_cast<std::size_t>(size));
    stream.seekg(0); stream.read(reinterpret_cast<char*>(bytes.data()), size);
    Require(static_cast<bool>(stream), "Fixture read truncated");
    return bytes;
}
void FixHeaderChecksum(Bytes& bytes) {
    QuestVr::SaveBundleDetail::Put32(bytes, 40u, QuestVr::SaveBundleDetail::Crc32(bytes.data(), 40u));
}
void CodecTests() {
    const Bytes check{'1','2','3','4','5','6','7','8','9'};
    Require(QuestVr::SaveBundleDetail::Crc32(check.data(), check.size()) == 0xcbf43926u, "CRC32 known vector differs");
    const QuestVr::SaveBundleData source{0x0102030405060708ull, {'m','e','t','a'}, {'r','u','n','t','i','m','e'}, 1u};
    Bytes encoded;
    Require(QuestVr::EncodeSaveBundle(source, encoded), "Paired bundle encoding failed");
    Require(encoded[16] == 8u && encoded[23] == 1u && encoded[24] == 4u && encoded[28] == 7u &&
            encoded.size() == 48u + 4u + 7u, "Explicit little-endian header layout differs");
    QuestVr::SaveBundleData decoded;
    Require(QuestVr::DecodeSaveBundle(encoded, decoded) && decoded.generation == source.generation &&
            decoded.metadata == source.metadata && decoded.runtime == source.runtime && decoded.slotIndex == 0u,
            "Bundle roundtrip or nonserialized slot provenance differs");
    std::size_t rejected{};
    const auto reject = [&](const Bytes& bytes, const char* description) {
        QuestVr::SaveBundleData data;
        Require(!QuestVr::DecodeSaveBundle(bytes, data) && data.generation == 0u &&
                data.metadata.empty() && data.runtime.empty(), description);
        ++rejected;
    };
    for (std::size_t size = 0; size < encoded.size(); ++size)
        reject(Bytes(encoded.begin(), encoded.begin() + size), "Truncated bundle accepted");
    auto bad = encoded; bad.push_back(0); reject(bad, "Trailing bundle bytes accepted");
    bad = encoded; bad[0] ^= 1u; reject(bad, "Wrong magic accepted");
    bad = encoded; bad[16] ^= 1u; reject(bad, "Unchecksummed generation mutation accepted");
    bad = encoded; bad[48] ^= 1u; reject(bad, "Metadata checksum corruption accepted");
    bad = encoded; bad.back() ^= 1u; reject(bad, "Runtime checksum corruption accepted");
    for (const auto& pair : {std::pair<std::size_t, std::uint32_t>{8u, 2u}, {12u, 44u}, {24u, 0u},
            {28u, 0u}, {24u, static_cast<std::uint32_t>(QuestVr::kMaximumSaveMetadataBytes + 1u)},
            {28u, 0xffffffffu}, {24u, 3u}, {44u, 1u}}) {
        bad = encoded;
        QuestVr::SaveBundleDetail::Put32(bad, pair.first, pair.second);
        FixHeaderChecksum(bad);
        reject(bad, "Malformed version/header/length/reserved control accepted");
    }
    for (const auto generation : {0ull, std::numeric_limits<unsigned long long>::max()}) {
        bad = encoded;
        QuestVr::SaveBundleDetail::Put32(bad, 16u, static_cast<std::uint32_t>(generation));
        QuestVr::SaveBundleDetail::Put32(bad, 20u, static_cast<std::uint32_t>(generation >> 32u));
        FixHeaderChecksum(bad);
        reject(bad, "Zero/wrapped generation accepted");
    }
    auto invalid = source;
    for (const auto generation : {0ull, std::numeric_limits<unsigned long long>::max()}) {
        invalid.generation = generation;
        Require(!QuestVr::EncodeSaveBundle(invalid, bad) && bad.empty(), "Encoder accepted zero/wrapped generation");
    }
    invalid = source; invalid.metadata.clear();
    Require(!QuestVr::EncodeSaveBundle(invalid, bad), "Empty metadata encoded");
    invalid = source; invalid.runtime.clear();
    Require(!QuestVr::EncodeSaveBundle(invalid, bad), "Empty runtime encoded");
    invalid = source; invalid.metadata.resize(QuestVr::kMaximumSaveMetadataBytes + 1u);
    Require(!QuestVr::EncodeSaveBundle(invalid, bad), "Oversized metadata encoded");
    invalid = source; invalid.runtime.resize(QuestVr::kMaximumSaveRuntimeBytes + 1u);
    Require(!QuestVr::EncodeSaveBundle(invalid, bad), "Oversized runtime encoded");
    std::cout << "Bundle LE header/CRC32/paired payload roundtrip and " << rejected << " malformed decoder controls passed.\n";
}

struct InjectedIo { enum Mode { PartialFailure, CorruptSuccess, WriteThrow, PublishFailure, PublishThrow } mode; unsigned publications{}; };
bool InjectWrite(void* context, const std::string& path, const Bytes& bytes) {
    auto& io = *static_cast<InjectedIo*>(context);
    if (io.mode == InjectedIo::WriteThrow) throw std::runtime_error("Injected write exception");
    if (io.mode == InjectedIo::PartialFailure || io.mode == InjectedIo::CorruptSuccess) {
        Require(QuestVr::WriteDurableSaveFile(path, Bytes{1u, 2u, 3u}), "Partial stage fixture failed");
        return io.mode == InjectedIo::CorruptSuccess;
    }
    return QuestVr::WriteDurableSaveFile(path, bytes);
}
bool InjectPublish(void* context, const std::string&, const std::string&) {
    auto& io = *static_cast<InjectedIo*>(context);
    ++io.publications;
    if (io.mode == InjectedIo::PublishThrow) throw std::runtime_error("Injected publication exception");
    return false;
}

void StorageTests() {
    FixtureDirectory directory;
    const auto paths = directory.Paths("quest-save-0");
    const auto legacyMetadata = directory.File("quest-save-0.meta");
    const auto legacyRuntime = directory.File("quest-save-0.runtime");
    const Bytes legacyMeta{'o','l','d','m'}, legacyRun{'o','l','d','r'};
    Require(QuestVr::WriteDurableSaveFile(legacyMetadata, legacyMeta) &&
            QuestVr::WriteDurableSaveFile(legacyRuntime, legacyRun), "Legacy preservation fixture write failed");
    Require(QuestVr::LoadSaveBundleCandidates(paths).empty(), "Fresh storage has a fabricated save");
    const Bytes metadataA{'m','1'}, runtimeA{'r','1'}, metadataB{'m','2'}, runtimeB{'r','2'},
        metadataC{'m','3'}, runtimeC{'r','3'};
    auto result = QuestVr::PublishSaveBundle(paths, metadataA, runtimeA);
    Require(result.saved && result.generation == 1u && result.slotIndex == 0u, "First durable publication failed");
    result = QuestVr::PublishSaveBundle(paths, metadataB, runtimeB);
    Require(result.saved && result.generation == 2u && result.slotIndex == 1u, "Alternating publication failed");
    auto candidates = QuestVr::LoadSaveBundleCandidates(paths);
    Require(candidates.size() == 2u && candidates[0].generation == 2u && candidates[1].generation == 1u &&
            candidates[0].metadata == metadataB && candidates[0].runtime == runtimeB &&
            candidates[1].metadata == metadataA && candidates[1].runtime == runtimeA,
            "Candidate ordering mixed metadata/runtime generations");
    const auto slot0 = Read(paths.slot0), slot1 = Read(paths.slot1);
    for (const auto mode : {InjectedIo::PartialFailure, InjectedIo::CorruptSuccess, InjectedIo::WriteThrow,
            InjectedIo::PublishFailure, InjectedIo::PublishThrow}) {
        InjectedIo io{mode};
        result = QuestVr::PublishSaveBundle(paths, metadataC, runtimeC, {&io, InjectWrite, InjectPublish});
        Require(!result.saved && Read(paths.slot0) == slot0 && Read(paths.slot1) == slot1,
                "Injected stage/publication failure altered a committed generation");
        candidates = QuestVr::LoadSaveBundleCandidates(paths);
        Require(candidates.size() == 2u && candidates.front().generation == 2u,
                "Failed/unpublished stage became a load candidate");
        if (mode == InjectedIo::PartialFailure || mode == InjectedIo::CorruptSuccess || mode == InjectedIo::WriteThrow)
            Require(io.publications == 0u, "Unvalidated stage reached publication");
    }
    result = QuestVr::PublishSaveBundle(paths, metadataC, runtimeC);
    Require(result.saved && result.generation == 3u && result.slotIndex == 0u && Read(paths.slot1) == slot1,
            "Third publication overwrote newest prior committed generation");
    const auto slot3 = Read(paths.slot0);
    for (const bool truncate : {true, false}) {
        auto corrupt = slot3;
        if (truncate) corrupt.pop_back(); else corrupt.back() ^= 1u;
        Require(QuestVr::WriteDurableSaveFile(paths.slot0, corrupt), "Corrupt-latest fixture failed");
        candidates = QuestVr::LoadSaveBundleCandidates(paths);
        Require(candidates.size() == 1u && candidates.front().generation == 2u &&
                candidates.front().metadata == metadataB && candidates.front().runtime == runtimeB &&
                Read(paths.slot1) == slot1, "Corrupt latest did not fall back to prior complete pair");
    }
    Require(QuestVr::WriteDurableSaveFile(paths.slot0, slot3), "Valid latest restoration failed");
    Bytes orphan;
    Require(QuestVr::EncodeSaveBundle({999u, metadataA, runtimeA, 0u}, orphan) &&
            QuestVr::WriteDurableSaveFile(paths.staging, orphan), "Orphaned-stage fixture failed");
    Require(QuestVr::LoadSaveBundleCandidates(paths).front().generation == 3u, "Orphaned high-generation stage was loaded");
    std::filesystem::remove(std::filesystem::u8path(paths.staging));
    std::filesystem::create_hard_link(std::filesystem::u8path(paths.slot0), std::filesystem::u8path(paths.staging));
    Require(!QuestVr::WriteDurableSaveFile(paths.staging, runtimeA) &&
            !QuestVr::PublishSaveBundle(paths, metadataA, runtimeA).saved && RawFixtureRead(paths.slot0) == slot3,
            "Hard-linked staging path altered the newest committed save");
    std::filesystem::remove(std::filesystem::u8path(paths.staging));
    std::filesystem::create_directory(std::filesystem::u8path(paths.staging));
    Require(!QuestVr::PublishSaveBundle(paths, metadataA, runtimeA).saved &&
            QuestVr::LoadSaveBundleCandidates(paths).front().generation == 3u,
            "Directory staging target was overwritten or prevented safe loading");
    std::filesystem::remove(std::filesystem::u8path(paths.staging));
    auto unsafe = paths; unsafe.staging = legacyMetadata;
    Require(!QuestVr::PublishSaveBundle(unsafe, metadataA, runtimeA).saved, "Legacy file accepted as a bundle staging path");
    unsafe = paths; unsafe.slot1 = paths.slot0;
    Require(!QuestVr::PublishSaveBundle(unsafe, metadataA, runtimeA).saved, "Aliased storage slots accepted");
    Bytes bounded;
    Require(!QuestVr::ReadBoundedSaveFile(paths.slot0, 1u, bounded) && bounded.empty() &&
            !QuestVr::ReadBoundedSaveFile(paths.slot0, 0u, bounded) &&
            !QuestVr::WriteDurableSaveFile(directory.root.string(), metadataA), "Bounded/exact file-path guard failed");
    Require(Read(legacyMetadata) == legacyMeta && Read(legacyRuntime) == legacyRun,
            "Bundle publication changed legacy independent save files");
    const auto overflow = directory.Paths("generation-limit");
    Bytes last;
    const auto lastGeneration = std::numeric_limits<std::uint64_t>::max() - 1u;
    Require(QuestVr::EncodeSaveBundle({lastGeneration, metadataA, runtimeA, 0u}, last) &&
            QuestVr::WriteDurableSaveFile(overflow.slot0, last), "Generation-limit fixture failed");
    Require(!QuestVr::PublishSaveBundle(overflow, metadataB, runtimeB).saved && Read(overflow.slot0) == last &&
            QuestVr::LoadSaveBundleCandidates(overflow).front().generation == lastGeneration,
            "Generation overflow changed prior committed save");
    std::cout << "Real temporary-file durable alternating publication, five injected failures, orphan-stage ignore, "
              << "corrupt-latest fallback, hard-link/path guards, generation limit and legacy preservation passed.\n";
}
} // namespace

int main() {
    try {
        CodecTests();
        StorageTests();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Quest save bundle test failed: " << error.what() << '\n';
        return 1;
    }
}
