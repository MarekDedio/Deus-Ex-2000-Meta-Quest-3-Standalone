#include "Precomp.h"
#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
std::size_t checks{}, rejections{};
void Require(bool condition, const std::string& description) {
    if (!condition) throw std::runtime_error(description);
    ++checks;
}
void U16(Bytes& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
}
void U32(Bytes& bytes, std::uint32_t value) {
    U16(bytes, static_cast<std::uint16_t>(value));
    U16(bytes, static_cast<std::uint16_t>(value >> 16u));
}
void U64(Bytes& bytes, std::uint64_t value) {
    U32(bytes, static_cast<std::uint32_t>(value));
    U32(bytes, static_cast<std::uint32_t>(value >> 32u));
}
void Index(Bytes& bytes, std::int32_t value) {
    auto magnitude = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((value < 0 ? 0x80u : 0u) | (magnitude & 63u));
    magnitude >>= 6u;
    if (magnitude) first |= 64u;
    bytes.push_back(first);
    while (magnitude) {
        auto next = static_cast<std::uint8_t>(magnitude & 127u);
        magnitude >>= 7u;
        if (magnitude) next |= 128u;
        bytes.push_back(next);
    }
}
void ReplaceU32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8u * i));
}
Bytes Properties() {
    Bytes bytes; Index(bytes, 6); bytes.push_back(0x22u); U32(bytes, 0x89abcdefu);
    Index(bytes, 7); bytes.push_back(0x83u); Index(bytes, 0);
    return bytes;
}
Bytes Script() {
    Bytes bytes{0x21u}; Index(bytes, 5); bytes.push_back(0x20u); Index(bytes, 2);
    bytes.push_back(0x08u); bytes.push_back(0x0cu);
    Index(bytes, 5); U32(bytes, 0u); Index(bytes, 0); U32(bytes, 0xffffffffu);
    return bytes;
}
struct Payload {
    Bytes bytes;
    std::array<std::size_t, 4> references{};
    std::size_t friendly{}, logical{}, raw{};
};
Payload MakePayload(bool state, std::uint16_t version = 68u, const Bytes& script = Script(),
    std::uint32_t logicalSize = 28u, std::uint16_t labels = 12u, bool stack = false) {
    Payload result;
    auto& bytes = result.bytes;
    if (stack) {
        Index(bytes, 2); Index(bytes, 2); U64(bytes, 0xa5a55a5a12345678ull);
        U32(bytes, 0xfedcba98u); Index(bytes, -1);
    }
    if (state) {
        const auto properties = Properties(); bytes.insert(bytes.end(), properties.begin(), properties.end());
    }
    // The generated fields exercise local/import/null reference retention.
    const std::array<std::int32_t, 4> references{{-2, 2, 0, 0}};
    for (std::size_t i = 0u; i < references.size(); ++i) {
        result.references[i] = bytes.size(); Index(bytes, references[i]);
    }
    result.friendly = bytes.size(); Index(bytes, 5);
    U32(bytes, 0x12345678u); U32(bytes, 0x9abcdef0u);
    result.logical = bytes.size(); U32(bytes, logicalSize);
    result.raw = bytes.size(); bytes.insert(bytes.end(), script.begin(), script.end());
    U64(bytes, 0x0123456789abcdefull); U64(bytes, 0xfedcba9876543210ull);
    U16(bytes, labels); U32(bytes, 0x80000007u);
    if (!state) {
        if (version <= 61u) U32(bytes, 0x12345678u);
        U32(bytes, 0xcafebabeu); bytes.resize(bytes.size() + 16u, 0x3cu);
        Index(bytes, 1); Index(bytes, 2); U32(bytes, 1); U32(bytes, 0xdeadbeefu);
        Index(bytes, 1); Index(bytes, 1);
        if (version >= 62u) { Index(bytes, 0); Index(bytes, 8); }
        const auto defaults = Properties(); bytes.insert(bytes.end(), defaults.begin(), defaults.end());
    }
    return result;
}
struct Fixture {
    PortablePackageTables package;
    std::filesystem::path directory, path;
    Fixture() {
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned i = 0u; i < 20u; ++i) {
            const auto candidate = parent / ("deusex-state-descriptor-test-" + std::to_string(stamp) + '-' + std::to_string(i));
            if (!std::filesystem::create_directory(candidate)) continue;
            directory = std::filesystem::canonical(candidate);
            Require(directory.parent_path() == parent &&
                directory.filename().string().rfind("deusex-state-descriptor-test-", 0u) == 0u,
                "Generated descriptor fixture escaped its temporary parent");
            break;
        }
        Require(!directory.empty(), "Could not create isolated descriptor fixture");
        path = directory / "GeneratedStatePayload.bin";
        Reset();
    }
    ~Fixture() {
        std::error_code ignored;
        if (!directory.empty()) std::filesystem::remove_all(directory, ignored);
    }
    void Reset() {
        package = {}; package.sourcePath = path.string(); package.version = 68u;
        for (const char* name : {"None", "Core", "State", "Owner", "TestState", "Friendly",
            "Counter", "bEnabled", "Config", "Foreign", "Function"})
            package.names.push_back({NameString(name), 0});
        package.imports = {{0, 0, 0, 1}, {1, 2, -1, 2}, {0, 0, 0, 9}, {9, 2, -3, 2}};
        package.exports = {{-2, 0, 2, 4, ObjectFlags{}, 0, 37},
            {0, 0, 0, 3, ObjectFlags{}, 0, 0}};
    }
    void Write(const Bytes& bytes) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        Require(static_cast<bool>(file), "Could not create generated descriptor payload");
        const std::array<char, 37> prefix{};
        file.write(prefix.data(), static_cast<std::streamsize>(prefix.size()));
        if (!bytes.empty()) file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        Require(static_cast<bool>(file), "Could not write generated descriptor payload");
        package.exports[0].ObjSize = static_cast<std::int32_t>(bytes.size());
    }
    PortableStateDescriptor State(const Payload& payload) { Write(payload.bytes); return LoadPortableStateDescriptor(package, 0u); }
    PortableClassDescriptor Class(const Payload& payload) {
        package.exports[0].ObjClass = 0; package.exports[0].ObjOuter = 0;
        Write(payload.bytes); return LoadPortableClassDescriptor(package, 0u);
    }
    template<class Action> void Reject(Action action, const char* message) {
        bool rejected{};
        try { action(); } catch (const std::exception&) { rejected = true; }
        Require(rejected, message); ++rejections;
    }
};
void CheckHeader(const PortableStateDescriptor& state, const Payload& payload) {
    Require(state.baseField == -2 && state.nextField == 2 && state.scriptText == 0 && state.children == 0,
        "UField/UStruct object references changed");
    Require(state.friendlyName == "Friendly" && state.line == 0x12345678u && state.textPos == 0x9abcdef0u,
        "UStruct name/source metadata changed");
    Require(state.logicalSize == 28u && state.bytecode.size() == 28u && state.rawBytes == Script(),
        "Raw versus normalized state bytecode was not retained");
    Require(state.bytecode[0] == 0x21u && state.bytecode[1] == 5u && state.bytecode[5] == 0x20u &&
        state.bytecode[6] == 2u && state.bytecode[10] == 0x08u && state.bytecode[11] == 0x0cu,
        "Compact name/object operands were not normalized to uint32");
    Require(state.probeMask == 0x0123456789abcdefull && state.ignoreMask == 0xfedcba9876543210ull &&
        state.labelTableOffset == 12u && state.stateFlags == 0x80000007u,
        "UState mask/label/unknown flag bits changed");
    Require(payload.bytes.size() > state.rawBytes.size(), "Fixture did not contain a full state header");
}
void Synthetic() {
    Fixture fixture;
    const auto valid = MakePayload(true);
    CheckHeader(fixture.State(valid), valid);
    Require(fixture.State(valid).objectPath == "Owner.TestState", "Actual State object identity was replaced by FriendlyName");
    for (const auto labels : {std::uint16_t{0xffffu}, std::uint16_t{0u}, std::uint16_t{0x1234u}}) {
        const auto empty = fixture.State(MakePayload(true, 68u, {}, 0u, labels));
        Require(empty.rawBytes.empty() && empty.bytecode.empty() && empty.logicalSize == 0u &&
            empty.labelTableOffset == labels, "Empty authored script or uint16 label sentinel was invented/clamped");
    }
    for (const auto version : {std::uint16_t{60u}, std::uint16_t{61u}, std::uint16_t{62u}, std::uint16_t{68u}}) {
        fixture.Reset(); fixture.package.version = version;
        const auto payload = MakePayload(false, version);
        const auto cls = fixture.Class(payload); CheckHeader(cls.state, payload);
        Require(cls.objectPath == "TestState" && cls.state.objectPath == cls.objectPath &&
            cls.stateBytecode == cls.state.bytecode && cls.classFlags == 0xcafebabeu &&
            cls.dependencyCount == 1u && cls.packageImportCount == 1u,
            "Class state metadata or legacy/class suffix compatibility changed");
        Require(cls.defaults.size() == 2u && cls.defaults[0].name == "Counter" &&
            cls.defaults[0].value == Bytes({0xefu, 0xcdu, 0xabu, 0x89u}) && cls.defaults[1].boolValue,
            "Class defaults changed after shared State header decoding");
    }
    fixture.Reset(); fixture.package.exports[0].ObjFlags = ObjectFlags::HasStack;
    CheckHeader(fixture.State(MakePayload(true, 68u, Script(), 28u, 12u, true)), valid);
    fixture.Reset(); fixture.package.exports[0].ObjFlags = ObjectFlags::HasStack;
    CheckHeader(fixture.Class(MakePayload(false, 68u, Script(), 28u, 12u, true)).state, valid);
    fixture.Reset();
    for (std::size_t length = 0u; length < valid.bytes.size(); ++length) {
        auto truncated = valid; truncated.bytes.resize(length);
        fixture.Reject([&] { fixture.State(truncated); }, "Truncated State header/script/masks accepted");
    }
    const auto malformed = [&](const auto& modify, const char* message) {
        fixture.Reset(); auto payload = valid; modify(payload);
        fixture.Reject([&] { fixture.State(payload); }, message);
    };
    for (std::size_t field = 0u; field < valid.references.size(); ++field) {
        malformed([&](Payload& p) { p.bytes[p.references[field]] = 63u; }, "Out-of-range header reference accepted");
        malformed([&](Payload& p) { p.bytes[p.references[field]] = 0xbfu; }, "Out-of-range imported header reference accepted");
    }
    malformed([](Payload& p) { p.bytes[p.friendly] = 0; }, "None FriendlyName accepted");
    malformed([](Payload& p) { p.bytes[p.friendly] = 63; }, "Out-of-range FriendlyName accepted");
    malformed([](Payload& p) { ReplaceU32(p.bytes, p.logical, 64u * 1024u * 1024u + 1u); }, "Unbounded logical script accepted");
    malformed([](Payload& p) { ReplaceU32(p.bytes, p.logical, 27u); }, "Token expanded past declared logical size");
    malformed([](Payload& p) { ReplaceU32(p.bytes, p.logical, 29u); }, "Wrong normalized logical size accepted");
    malformed([](Payload& p) { p.bytes[p.raw + 1u] = 63u; }, "Invalid bytecode name operand accepted");
    malformed([](Payload& p) { p.bytes[p.raw + 3u] = 63u; }, "Invalid bytecode object operand accepted");
    malformed([](Payload& p) { p.bytes[p.raw + 6u] = 63u; }, "Invalid bytecode label name accepted");
    malformed([](Payload& p) { p.bytes.push_back(0u); }, "State payload trailing bytes accepted");
    fixture.Reset();
    fixture.Reject([&] { fixture.State(MakePayload(true, 68u, Bytes{0x03u}, 1u)); }, "Unknown script token accepted");
    fixture.Reject([&] { Bytes deep(64u, 0x39u); deep.push_back(0x26u);
        fixture.State(MakePayload(true, 68u, deep, 65u)); }, "Excessive bytecode nesting accepted");
    for (const auto& change : std::vector<std::pair<int, const char*>>{{0,"Class accepted as named State"},
        {-4,"Foreign.State suffix accepted as Core.State"}, {63,"Out-of-range metaclass accepted"}}) {
        fixture.Reset(); fixture.package.exports[0].ObjClass = change.first;
        fixture.Reject([&] { fixture.State(valid); }, change.second);
    }
    fixture.Reset(); fixture.Write(valid.bytes);
    fixture.Reject([&] { LoadPortableStateDescriptor(fixture.package, fixture.package.exports.size()); }, "Out-of-range export index accepted");
    fixture.package.exports[0].ObjOffset = -1;
    fixture.Reject([&] { LoadPortableStateDescriptor(fixture.package, 0u); }, "Negative payload offset accepted");
    fixture.Reset(); fixture.Write(valid.bytes); fixture.package.exports[0].ObjSize = 64 * 1024 * 1024 + 1;
    fixture.Reject([&] { LoadPortableStateDescriptor(fixture.package, 0u); }, "Unbounded export preallocation accepted");
    fixture.package.exports[0].ObjSize = static_cast<std::int32_t>(valid.bytes.size() + 1u);
    fixture.Reject([&] { LoadPortableStateDescriptor(fixture.package, 0u); }, "Payload outside source file accepted");
    fixture.Reset(); fixture.package.exports[0].ObjOuter = 1;
    fixture.Reject([&] { fixture.State(valid); }, "Cyclic export identity accepted");
    fixture.Reset(); fixture.package.imports[0].ObjOuter = -2;
    fixture.Reject([&] { fixture.State(valid); }, "Cyclic metaclass identity accepted");
    fixture.Reset(); fixture.package.exports[0].ObjName = 63;
    fixture.Reject([&] { fixture.State(valid); }, "Invalid actual object name accepted");
    fixture.Reset(); fixture.package.exports[0].ObjBase = 63;
    fixture.Reject([&] { fixture.State(valid); }, "Invalid export base identity accepted");
    fixture.Reset(); fixture.package.names[4].Name = NameString(std::string(64u * 1024u + 1u, 'N'));
    fixture.Reject([&] { fixture.State(valid); }, "Unbounded descriptor terminal name accepted");
    fixture.Reset();
    fixture.package.names[4].Name = NameString(std::string(40u * 1024u, 'A'));
    fixture.package.names[3].Name = NameString(std::string(40u * 1024u, 'B'));
    fixture.Reject([&] { fixture.State(valid); }, "Unbounded aggregate descriptor path accepted");
    fixture.Reset(); auto extraClass = MakePayload(false); extraClass.bytes.push_back(0u);
    fixture.Reject([&] { fixture.Class(extraClass); }, "Class trailing bytes accepted");
    std::cout << "PASS exact state/class descriptor retention " << checks << " checks, " << rejections << " rejection controls\n";
}

// Independent field reader verifies retained masks/source/identity bits against
// original serialized payloads, without constructing a runtime or executing AI.
struct AuditReader {
    const Bytes& bytes; std::size_t pos{};
    std::uint8_t Byte() { if (pos >= bytes.size()) throw std::runtime_error("Truncated audit payload"); return bytes[pos++]; }
    std::uint16_t Word() { const auto a = Byte(); return static_cast<std::uint16_t>(a | (Byte() << 8u)); }
    std::uint32_t Dword() { const auto a = Word(); return a | (static_cast<std::uint32_t>(Word()) << 16u); }
    std::uint64_t Qword() { const auto a = Dword(); return a | (static_cast<std::uint64_t>(Dword()) << 32u); }
    std::int32_t Compact() {
        auto first = Byte(); const bool negative = (first & 0x80u) != 0u;
        std::uint64_t value = first & 63u; unsigned shift = 6u; bool more = (first & 0x40u) != 0u;
        while (more) { if (shift >= 32u) throw std::runtime_error("Invalid audit compact index");
            const auto b = Byte(); value |= static_cast<std::uint64_t>(b & 127u) << shift;
            more = (b & 0x80u) != 0u; shift += 7u; }
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) throw std::runtime_error("Audit compact overflow");
        return negative ? -static_cast<std::int32_t>(value) : static_cast<std::int32_t>(value);
    }
};
void CompareOriginal(const PortablePackageTables& package, std::size_t index,
    const PortableStateDescriptor& descriptor, bool isClass) {
    const auto& entry = package.exports[index];
    Bytes bytes(static_cast<std::size_t>(entry.ObjSize));
    std::ifstream file(package.sourcePath, std::ios::binary); file.seekg(entry.ObjOffset);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "Could not read original descriptor bytes");
    AuditReader reader{bytes, 0u};
    if (isClass && AnyFlags(entry.ObjFlags, ObjectFlags::HasStack)) {
        const auto function = reader.Compact(); reader.Compact(); reader.Qword(); reader.Dword();
        if (function) reader.Compact();
    } else if (!isClass) reader.pos = LoadPortableExportProperties(package, index).bytesConsumed;
    Require(reader.Compact() == descriptor.baseField && reader.Compact() == descriptor.nextField &&
        reader.Compact() == descriptor.scriptText && reader.Compact() == descriptor.children,
        "Original UField/UStruct references were not retained exactly");
    const auto friendly = reader.Compact();
    Require(friendly >= 0 && static_cast<std::size_t>(friendly) < package.names.size() &&
        package.names[static_cast<std::size_t>(friendly)].Name == descriptor.friendlyName,
        "Original FriendlyName identity changed");
    Require(reader.Dword() == descriptor.line && reader.Dword() == descriptor.textPos &&
        reader.Dword() == descriptor.logicalSize, "Original UStruct source/logical metadata changed");
    Require(reader.pos <= bytes.size() && descriptor.rawBytes.size() <= bytes.size() - reader.pos &&
        std::equal(descriptor.rawBytes.begin(), descriptor.rawBytes.end(), bytes.begin() + static_cast<std::ptrdiff_t>(reader.pos)),
        "Original raw state script bytes changed");
    reader.pos += descriptor.rawBytes.size();
    Require(reader.Qword() == descriptor.probeMask && reader.Qword() == descriptor.ignoreMask &&
        reader.Word() == descriptor.labelTableOffset && reader.Dword() == descriptor.stateFlags,
        "Original UState masks/labels/flags changed");
    Require(descriptor.bytecode.size() == descriptor.logicalSize && descriptor.objectPath ==
        GetPortableObjectPath(package, static_cast<std::int32_t>(index + 1u)), "Original state normalized size/object identity changed");
    if (!isClass) Require(reader.pos == bytes.size(), "Original State payload did not end exactly");
}
bool ActualState(const PortablePackageTables& package, const ExportTableEntry& entry) {
    auto meta = GetPortableObjectPath(package, entry.ObjClass);
    if (entry.ObjClass > 0) meta = std::filesystem::path(package.sourcePath).stem().string() + '.' + meta;
    return NameString(meta) == "Core.State";
}
void AuditOriginal(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> packages;
    for (const auto& entry : std::filesystem::directory_iterator(root / "System"))
        if (entry.is_regular_file() && entry.path().extension() == ".u") packages.push_back(entry.path());
    std::sort(packages.begin(), packages.end());
    Require(!packages.empty() && packages.size() <= 256u, "Original System package audit bound exceeded");
    std::size_t states{}, classes{}, emptyClasses{}, scriptedStates{}, scriptedClasses{}, stateSentinels{}, classSentinels{};
    std::size_t rawBytes{}, logicalBytes{}, maximumLogical{}, labelOutOfScript{};
    std::map<std::uint32_t, std::size_t> stateFlags;
    for (const auto& path : packages) {
        Require(std::filesystem::file_size(path) <= 256u * 1024u * 1024u, "Original System package exceeds audit bound");
        const auto package = LoadPortablePackageTables(path.string());
        for (std::size_t i = 0u; i < package.exports.size(); ++i) {
            const auto& entry = package.exports[i]; const bool isClass = entry.ObjClass == 0;
            if (!isClass && !ActualState(package, entry)) continue;
            if (isClass && entry.ObjSize == 0) { ++emptyClasses; continue; }
            PortableStateDescriptor descriptor;
            try {
                if (isClass) {
                    const auto cls = LoadPortableClassDescriptor(package, i); descriptor = cls.state;
                    Require(cls.stateBytecode == cls.state.bytecode, "Compatibility Class stateBytecode changed");
                    ++classes; scriptedClasses += descriptor.logicalSize != 0u; classSentinels += descriptor.labelTableOffset == 0xffffu;
                } else {
                    descriptor = LoadPortableStateDescriptor(package, i); ++states;
                    scriptedStates += descriptor.logicalSize != 0u; stateSentinels += descriptor.labelTableOffset == 0xffffu;
                    ++stateFlags[descriptor.stateFlags];
                }
                CompareOriginal(package, i, descriptor, isClass);
            } catch (const std::exception& error) {
                throw std::runtime_error(path.stem().string() + '.' + GetPortableObjectPath(package,
                    static_cast<std::int32_t>(i + 1u)) + ": " + error.what());
            }
            rawBytes += descriptor.rawBytes.size(); logicalBytes += descriptor.logicalSize;
            maximumLogical = std::max(maximumLogical, static_cast<std::size_t>(descriptor.logicalSize));
            labelOutOfScript += descriptor.labelTableOffset != 0xffffu && descriptor.labelTableOffset >= descriptor.logicalSize;
        }
    }
    std::size_t maps{}, mapClasses{}, mapStates{};
    for (const auto& entry : std::filesystem::directory_iterator(root / "Maps")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".dx") continue;
        const auto package = LoadPortablePackageTables(entry.path().string()); ++maps;
        for (const auto& exported : package.exports) {
            mapClasses += exported.ObjClass == 0; mapStates += ActualState(package, exported);
        }
    }
    Require(states != 0u && classes != 0u, "Original audit contained no actual State/Class metadata");
    std::cout << "AUDIT SystemPackages=" << packages.size() << " actualStates=" << states << " serializedClasses=" << classes <<
        " emptyClasses=" << emptyClasses << " scriptedStates=" << scriptedStates << " scriptedClasses=" << scriptedClasses <<
        " stateLabelSentinels=" << stateSentinels << " classLabelSentinels=" << classSentinels <<
        " nonSentinelOffsetsOutsideScript=" << labelOutOfScript << " rawBytes=" << rawBytes << " normalizedBytes=" << logicalBytes <<
        " maximumLogicalSize=" << maximumLogical << '\n';
    for (const auto& [flags, count] : stateFlags) std::cout << "STATEFLAGS " << flags << " count=" << count << '\n';
    std::cout << "MAP AUDIT maps=" << maps << " classExports=" << mapClasses << " actualStateExports=" << mapStates << '\n';
}
} // namespace

int main(int argc, char** argv) {
    try {
        Synthetic();
        if (argc == 3 && std::string(argv[1]) == "--audit-original") AuditOriginal(argv[2]);
        else Require(argc == 1, "Usage: portable_state_descriptor_test [--audit-original GAME_ROOT]");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
