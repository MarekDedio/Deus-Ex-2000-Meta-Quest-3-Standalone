// Read-only host table audit. No actor/runtime initialization or asset export.
// SHA-256 uses the Windows system provider; this executable is host-only.
#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

void Require(const bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
class Sha256 final {
public:
    Sha256() {
        Check(BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_SHA256_ALGORITHM, nullptr, 0u));
        try {
            ULONG objectBytes{}, returned{}, digestBytes{};
            Check(BCryptGetProperty(algorithm_, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes),
                sizeof(objectBytes), &returned, 0u));
            Require(returned == sizeof(objectBytes) && objectBytes != 0u && objectBytes <= 65536u,
                "SHA-256 provider object bound failed");
            Check(BCryptGetProperty(algorithm_, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&digestBytes),
                sizeof(digestBytes), &returned, 0u));
            Require(returned == sizeof(digestBytes) && digestBytes == 32u, "SHA-256 provider digest width failed");
            object_.resize(objectBytes);
            Check(BCryptCreateHash(algorithm_, &hash_, object_.data(), objectBytes, nullptr, 0u, 0u));
        } catch (...) {
            if (hash_) { BCryptDestroyHash(hash_); hash_ = nullptr; }
            BCryptCloseAlgorithmProvider(algorithm_, 0u); algorithm_ = nullptr;
            throw;
        }
    }
    ~Sha256() {
        if (hash_) BCryptDestroyHash(hash_);
        if (algorithm_) BCryptCloseAlgorithmProvider(algorithm_, 0u);
    }
    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;
    void Bytes(const void* bytes, std::size_t count) {
        Require(!finished_, "SHA-256 input after finalization");
        Require(count == 0u || bytes != nullptr, "SHA-256 null nonempty input");
        const auto* source = static_cast<const std::uint8_t*>(bytes);
        while (count != 0u) {
            const auto copied = std::min(count, staging_.size() - staged_);
            std::memcpy(staging_.data() + staged_, source, copied);
            staged_ += copied; source += copied; count -= copied;
            if (staged_ == staging_.size()) Flush();
        }
    }
    void U8(const std::uint8_t value) { Bytes(&value, 1u); }
    void U16(const std::uint16_t value) {
        const std::array<std::uint8_t, 2> bytes{static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8u)};
        Bytes(bytes.data(), bytes.size());
    }
    void U32(const std::uint32_t value) {
        std::array<std::uint8_t, 4> bytes{};
        for (unsigned index = 0u; index < bytes.size(); ++index) bytes[index] = static_cast<std::uint8_t>(value >> (8u * index));
        Bytes(bytes.data(), bytes.size());
    }
    void U64(const std::uint64_t value) {
        std::array<std::uint8_t, 8> bytes{};
        for (unsigned index = 0u; index < bytes.size(); ++index) bytes[index] = static_cast<std::uint8_t>(value >> (8u * index));
        Bytes(bytes.data(), bytes.size());
    }
    void I32(const std::int32_t value) { U32(static_cast<std::uint32_t>(value)); }
    void String(const std::string& value) { U64(value.size()); Bytes(value.data(), value.size()); }
    std::string Finish() {
        Require(!finished_, "SHA-256 finalized twice"); Flush();
        std::array<std::uint8_t, 32> bytes{};
        Check(BCryptFinishHash(hash_, bytes.data(), static_cast<ULONG>(bytes.size()), 0u));
        finished_ = true;
        std::ostringstream text; text << std::hex << std::setfill('0');
        for (const auto byte : bytes) text << std::setw(2) << static_cast<unsigned>(byte);
        return text.str();
    }
private:
    BCRYPT_ALG_HANDLE algorithm_{};
    BCRYPT_HASH_HANDLE hash_{};
    std::vector<std::uint8_t> object_;
    std::array<std::uint8_t, 8192> staging_{};
    std::size_t staged_{};
    bool finished_{};
    static void Check(const NTSTATUS status) {
        Require(status >= 0, "Windows SHA-256 provider operation failed");
    }
    void Flush() {
        if (staged_ == 0u) return;
        Check(BCryptHashData(hash_, staging_.data(), static_cast<ULONG>(staged_), 0u));
        staged_ = 0u;
    }
};

// Complete pinned Package::ReadTables header, including fields the portable
// table API deliberately does not retain. Serialized object bodies are not read.
struct Header {
    std::uint32_t signature{};
    std::uint16_t version{}, licensee{};
    std::uint32_t flags{}, names{}, nameOffset{}, exports{}, exportOffset{}, imports{}, importOffset{};
    std::uint32_t heritageCount{}, heritageOffset{};
    std::array<std::uint8_t, 16> guid{};
    std::vector<std::pair<std::uint32_t, std::uint32_t>> generations;
};
std::uint32_t ReadLittle(std::ifstream& file, const unsigned width) {
    std::array<std::uint8_t, 4> bytes{};
    file.read(reinterpret_cast<char*>(bytes.data()), width);
    Require(file.gcount() == static_cast<std::streamsize>(width), "Package audit header is truncated");
    std::uint32_t value{};
    for (unsigned index = 0u; index < width; ++index) value |= std::uint32_t(bytes[index]) << (8u * index);
    return value;
}
Header ReadHeader(const fs::path& path, const std::uint64_t fileBytes) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open(), "Package audit could not open header read-only");
    Header header;
    header.signature = ReadLittle(file, 4u);
    header.version = static_cast<std::uint16_t>(ReadLittle(file, 2u));
    header.licensee = static_cast<std::uint16_t>(ReadLittle(file, 2u));
    Require(header.signature == 0x9e2a83c1u && header.version >= 60u && header.version < 100u,
        "Package audit header signature/version is unsupported");
    header.flags = ReadLittle(file, 4u);
    header.names = ReadLittle(file, 4u); header.nameOffset = ReadLittle(file, 4u);
    header.exports = ReadLittle(file, 4u); header.exportOffset = ReadLittle(file, 4u);
    header.imports = ReadLittle(file, 4u); header.importOffset = ReadLittle(file, 4u);
    if (header.version < 68u) {
        header.heritageCount = ReadLittle(file, 4u); header.heritageOffset = ReadLittle(file, 4u);
    } else {
        file.read(reinterpret_cast<char*>(header.guid.data()), header.guid.size());
        Require(file.gcount() == static_cast<std::streamsize>(header.guid.size()), "Package audit GUID is truncated");
        const auto count = ReadLittle(file, 4u);
        Require(fileBytes >= 56u && count <= 4096u && count <= (fileBytes - 56u) / 8u,
            "Package audit generation header exceeds bound");
        header.generations.reserve(count);
        for (std::uint32_t index = 0u; index < count; ++index) {
            const auto exports = ReadLittle(file, 4u), names = ReadLittle(file, 4u);
            header.generations.emplace_back(exports, names);
        }
    }
    return header;
}
std::string CanonicalDigest(const std::string& relativePath, const std::uint64_t fileBytes,
    const Header& header, const PortablePackageTables& table) {
    Sha256 hash;
    // v1 framing: domain/path are u64-byte-length UTF-8/byte strings; all
    // integers are fixed-width little endian; signed i32 uses modulo-2^32 bits.
    // Preserve table order and literal NameString spelling/flags, not global
    // process-local interning indices or locale/case-folded approximations.
    hash.String("QuestVR.PortablePackageTables.canonical.v1");
    hash.String(relativePath); hash.U64(fileBytes);
    hash.U32(header.signature); hash.U16(header.version); hash.U16(header.licensee); hash.U32(header.flags);
    hash.U32(header.names); hash.U32(header.nameOffset); hash.U32(header.exports); hash.U32(header.exportOffset);
    hash.U32(header.imports); hash.U32(header.importOffset);
    hash.U8(header.version < 68u ? 0u : 1u);
    if (header.version < 68u) {
        hash.U32(header.heritageCount); hash.U32(header.heritageOffset);
    } else {
        hash.Bytes(header.guid.data(), header.guid.size()); hash.U64(header.generations.size());
        for (const auto& generation : header.generations) { hash.U32(generation.first); hash.U32(generation.second); }
    }
    hash.U16(table.version); hash.U16(table.licenseeMode); hash.U32(table.flags);
    hash.U64(table.names.size());
    for (const auto& name : table.names) { hash.String(name.Name.ToString()); hash.U32(name.Flags); }
    hash.U64(table.exports.size());
    for (const auto& entry : table.exports) {
        hash.I32(entry.ObjClass); hash.I32(entry.ObjBase); hash.I32(entry.ObjOuter); hash.I32(entry.ObjName);
        hash.U32(static_cast<std::uint32_t>(entry.ObjFlags)); hash.I32(entry.ObjSize); hash.I32(entry.ObjOffset);
    }
    hash.U64(table.imports.size());
    for (const auto& entry : table.imports) {
        hash.I32(entry.ClassPackage); hash.I32(entry.ClassName); hash.I32(entry.ObjOuter); hash.I32(entry.ObjName);
    }
    return hash.Finish();
}
void SelfTest() {
    Sha256 empty;
    Require(empty.Finish() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "Empty SHA-256 known answer failed");
    Sha256 abc; abc.Bytes("a", 1u); abc.Bytes("b", 1u); abc.Bytes("c", 1u);
    Require(abc.Finish() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "Chunked SHA-256 known answer failed");
    Sha256 million; const std::string block(1000u, 'a');
    for (unsigned index = 0u; index < 1000u; ++index) million.Bytes(block.data(), block.size());
    Require(million.Finish() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "Staged SHA-256 known answer failed");
    Header header; header.signature = 0x9e2a83c1u; header.version = 68u;
    PortablePackageTables original; original.version = 68u;
    NameTableEntry name{}; name.Name = "AuditSentinel"; name.Flags = 7u; original.names.push_back(name);
    original.exports.push_back({}); original.imports.push_back({});
    const auto baseline = CanonicalDigest("System/Synthetic.u", 100u, header, original);
    Require(baseline == CanonicalDigest("System/Synthetic.u", 100u, header, original), "Canonical digest is not deterministic");
    auto changed = original; changed.names.front().Flags ^= 1u;
    Require(baseline != CanonicalDigest("System/Synthetic.u", 100u, header, changed), "Canonical name flags omitted");
    changed = original; changed.names.front().Name = "AUDITSENTINEL";
    Require(baseline != CanonicalDigest("System/Synthetic.u", 100u, header, changed), "Canonical literal name spelling omitted");
    changed = original; changed.exports.front().ObjOffset = -1;
    Require(baseline != CanonicalDigest("System/Synthetic.u", 100u, header, changed), "Canonical signed export offset omitted");
    changed = original; changed.imports.front().ClassPackage = 1;
    Require(baseline != CanonicalDigest("System/Synthetic.u", 100u, header, changed), "Canonical import field omitted");
    header.guid.front() = 1u;
    Require(baseline != CanonicalDigest("System/Synthetic.u", 100u, header, original), "Canonical raw header GUID omitted");
}
std::string LowerAscii(std::string value) {
    for (auto& letter : value) if (letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter + ('a' - 'A'));
    return value;
}
void Add(std::uint64_t& total, const std::uint64_t amount) {
    Require(amount <= std::numeric_limits<std::uint64_t>::max() - total, "Package audit aggregate overflow");
    total += amount;
}
void Audit(const fs::path& root, const std::string& label) {
    const auto system = fs::canonical(root / "System"), maps = fs::canonical(root / "Maps");
    std::vector<fs::path> catalog;
    for (const auto& entry : fs::directory_iterator(system)) {
        if (!entry.is_regular_file() || LowerAscii(entry.path().extension().string()) != ".u") continue;
        Require(fs::canonical(entry.path()).parent_path() == system, "Package audit source resolves outside System");
        Require(catalog.size() < 4096u, "Package audit catalog exceeds bound");
        catalog.push_back(entry.path());
    }
    Require(!catalog.empty(), "Package audit System catalog is empty");
    std::sort(catalog.begin(), catalog.end(), [](const auto& left, const auto& right) {
        return left.filename().generic_string() < right.filename().generic_string();
    });
    const auto systemCount = catalog.size();
    for (const auto* name : {"00_Training.dx", "01_NYC_UNATCOIsland.dx", "00_Intro.dx"}) {
        const auto path = maps / name;
        Require(fs::is_regular_file(path) && fs::canonical(path).parent_path() == maps, "Required package audit map is missing/outside Maps");
        catalog.push_back(path);
    }
    std::cout << "AUDIT format=sha256-canonical-v1 label=" << label << " system=" << systemCount << " maps=3 packages=" << catalog.size() << '\n';
    std::cout << "TIMING reader_us excludes header audit/digest/output; OS cache/concurrent load uncontrolled; no Quest proof\n";
    Sha256 aggregate; aggregate.String("QuestVR.PortablePackageCatalog.canonical.v1"); aggregate.U64(catalog.size());
    std::uint64_t bytes{}, names{}, exports{}, imports{}, parseMicros{};
    const auto start = Clock::now();
    for (std::size_t index = 0u; index < catalog.size(); ++index) {
        const auto& path = catalog[index];
        const auto relative = std::string(index < systemCount ? "System/" : "Maps/") + path.filename().generic_string();
        const auto sizeBefore = fs::file_size(path);
        const auto timeBefore = fs::last_write_time(path);
        Require(sizeBefore <= 512u * 1024u * 1024u, "Package audit source exceeds size bound");
        const auto loadStart = Clock::now();
        const auto table = LoadPortablePackageTables(path.string());
        const auto loadEnd = Clock::now();
        const auto header = ReadHeader(path, sizeBefore);
        Require(table.sourcePath == path.string() && table.version == header.version && table.licenseeMode == header.licensee &&
            table.flags == header.flags && table.names.size() == header.names && table.exports.size() == header.exports &&
            table.imports.size() == header.imports, "Loaded package table header/provenance disagrees with original header");
        const auto digest = CanonicalDigest(relative, sizeBefore, header, table);
        Require(fs::file_size(path) == sizeBefore && fs::last_write_time(path) == timeBefore, "Package audit source changed during read");
        const auto loadMicros = std::chrono::duration_cast<std::chrono::microseconds>(loadEnd - loadStart).count();
        Require(loadMicros >= 0, "Package audit clock went backwards");
        Add(bytes, sizeBefore); Add(names, table.names.size()); Add(exports, table.exports.size()); Add(imports, table.imports.size());
        Add(parseMicros, static_cast<std::uint64_t>(loadMicros));
        aggregate.String(relative); aggregate.String(digest);
        std::cout << "TABLE path=" << relative << " bytes=" << sizeBefore << " names=" << table.names.size() <<
            " exports=" << table.exports.size() << " imports=" << table.imports.size() << " digest=" << digest << " reader_us=" << loadMicros << '\n';
    }
    const auto digest = aggregate.Finish();
    const auto wallMicros = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
    std::cout << "CATALOG packages=" << catalog.size() << " system=" << systemCount << " maps=3 bytes=" << bytes <<
        " names=" << names << " exports=" << exports << " imports=" << imports << " digest=" << digest << '\n';
    std::cout << "PASS table-only audit label=" << label << " reader_us=" << parseMicros << " wall_us=" << wallMicros <<
        "; no runtime/actor/script execution or commercial-content output\n";
}
} // namespace

int main(int argc, char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        SelfTest();
        if (argc == 2 && std::string(argv[1]) == "--self-test") {
            std::cout << "PASS table audit SHA-256 known answers/canonical sensitivity; synthetic inputs only\n";
            return 0;
        }
        Require(argc == 2 || (argc == 4 && std::string(argv[2]) == "--label"),
            "Usage: portable_table_reader_audit <game-root> [--label baseline-or-buffered] | --self-test");
        const auto label = argc == 4 ? std::string(argv[3]) : std::string("unlabelled");
        Require(!label.empty() && label.size() <= 32u && std::all_of(label.begin(), label.end(), [](const auto letter) {
            return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') ||
                (letter >= '0' && letter <= '9') || letter == '-' || letter == '_';
        }), "Package audit label must be short ASCII identifier");
        Audit(fs::canonical(fs::path(argv[1])), label);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL table-only audit: " << error.what() << '\n';
        return 1;
    }
}
