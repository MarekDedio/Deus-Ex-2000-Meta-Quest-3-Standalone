#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "quest_authored_struct_value.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
std::size_t checks{}, rejections{};
void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
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
    Bytes bytes;
    Index(bytes, 6); bytes.push_back(0x22u); U32(bytes, 0x89abcdefu);
    Index(bytes, 7); bytes.push_back(0x83u);
    Index(bytes, 6); bytes.push_back(0xbau); Index(bytes, 5); bytes.push_back(0xc0u);
    bytes.push_back(0x01u); bytes.push_back(0x02u); bytes.push_back(0x03u);
    bytes.resize(bytes.size() + 12u, 0x5au);
    Index(bytes, 6); bytes.push_back(0xd2u); bytes.push_back(3u);
    bytes.push_back(0x80u); bytes.push_back(5u); bytes.resize(bytes.size() + 3u, 0x7au);
    Index(bytes, 6); bytes.push_back(0x62u); U16(bytes, 3u); bytes.resize(bytes.size() + 3u, 0x7bu);
    Index(bytes, 6); bytes.push_back(0x72u); U32(bytes, 3u); bytes.resize(bytes.size() + 3u, 0x7cu);
    Index(bytes, 0);
    return bytes;
}
Bytes Script() {
    Bytes bytes{0x21u}; Index(bytes, 5);
    bytes.push_back(0x20u); Index(bytes, 4);
    bytes.push_back(0x08u); bytes.push_back(0x0cu);
    Index(bytes, 5); U32(bytes, 0u); Index(bytes, 0); U32(bytes, 0xffffffffu);
    return bytes;
}
struct Payload {
    Bytes bytes;
    std::array<std::size_t, 4> references{};
    std::size_t friendly{}, logical{}, raw{};
};
Payload MakePayload(const Bytes& script = Script(), std::uint32_t logical = 28u,
    bool stack = false, bool nullStack = false, const Bytes& properties = Properties()) {
    Payload result; auto& bytes = result.bytes;
    if (stack) {
        Index(bytes, nullStack ? 0 : 4); Index(bytes, 2);
        U64(bytes, 0xa5a55a5a12345678ull); U32(bytes, 0xfedcba98u);
        if (!nullStack) Index(bytes, -1);
    }
    bytes.insert(bytes.end(), properties.begin(), properties.end());
    const std::array<std::int32_t, 4> refs{{-2, 2, 3, 4}};
    for (std::size_t i = 0u; i < refs.size(); ++i) {
        result.references[i] = bytes.size(); Index(bytes, refs[i]);
    }
    result.friendly = bytes.size(); Index(bytes, 5);
    U32(bytes, 0x12345678u); U32(bytes, 0x9abcdef0u);
    result.logical = bytes.size(); U32(bytes, logical);
    result.raw = bytes.size(); bytes.insert(bytes.end(), script.begin(), script.end());
    return result;
}
struct Fixture {
    PortablePackageTables package;
    std::filesystem::path directory, path;
    Fixture() {
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned i = 0u; i < 20u; ++i) {
            const auto candidate = parent / ("deusex-struct-descriptor-test-" + std::to_string(stamp) + '-' + std::to_string(i));
            if (!std::filesystem::create_directory(candidate)) continue;
            directory = std::filesystem::canonical(candidate);
            Require(directory.parent_path() == parent &&
                directory.filename().string().rfind("deusex-struct-descriptor-test-", 0u) == 0u,
                "Generated Struct fixture escaped its temporary parent");
            break;
        }
        Require(!directory.empty(), "Could not create isolated Struct fixture");
        path = directory / "GeneratedStructPayload.bin";
        Reset();
    }
    ~Fixture() {
        std::error_code ignored;
        if (!directory.empty()) std::filesystem::remove_all(directory, ignored);
    }
    void Reset() {
        path = directory / "GeneratedStructPayload.bin";
        package = {}; package.sourcePath = path.string(); package.version = 68u;
        for (const char* name : {"None", "Core", "Struct", "Owner", "TestStruct", "Friendly",
            "Counter", "bEnabled", "Config", "Foreign", "State", "Class", "TextBuffer", "ScriptText", "Member"})
            package.names.push_back({NameString(name), 0});
        package.imports = {{0, 0, 0, 1}, {1, 11, -1, 2}, {0, 0, 0, 9},
            {9, 11, -3, 2}, {1, 11, -1, 10}, {1, 11, -1, 11}, {1, 11, -1, 12}};
        package.exports = {{-2, 0, 2, 4, ObjectFlags{}, 0, 37},
            {0, 0, 0, 3, ObjectFlags{}, 0, 0}, {-7, 0, 2, 13, ObjectFlags{}, 0, 0},
            {0, 0, 2, 14, ObjectFlags{}, 0, 0}};
    }
    void Write(const Bytes& bytes) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        Require(static_cast<bool>(file), "Could not create generated Struct payload");
        const std::array<char, 37> prefix{};
        file.write(prefix.data(), static_cast<std::streamsize>(prefix.size()));
        if (!bytes.empty()) file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        Require(static_cast<bool>(file), "Could not write generated Struct payload");
        package.exports[0].ObjSize = static_cast<std::int32_t>(bytes.size());
    }
    PortableStructDescriptor Load(const Payload& payload) {
        Write(payload.bytes); return LoadPortableStructDescriptor(package, 0u);
    }
    template<class Action> void Reject(Action action, const std::string& message) {
        bool rejected{};
        try { action(); } catch (const std::exception&) { rejected = true; }
        Require(rejected, message); ++rejections;
    }
};
void CheckHeader(const PortableStructDescriptor& descriptor) {
    Require(descriptor.objectPath == "Owner.TestStruct", "Actual Struct identity was replaced by FriendlyName");
    Require(descriptor.baseField == -2 && descriptor.nextField == 2 && descriptor.scriptText == 3 && descriptor.children == 4,
        "Exact UField/UStruct references changed");
    Require(descriptor.friendlyName == "Friendly" && descriptor.line == 0x12345678u && descriptor.textPos == 0x9abcdef0u,
        "UStruct FriendlyName/source metadata changed");
    Require(descriptor.logicalSize == 28u && descriptor.bytecode.size() == 28u && descriptor.rawBytes == Script(),
        "Raw compact script bytes or normalized logical size changed");
    Require(descriptor.bytecode[0] == 0x21u && descriptor.bytecode[1] == 5u && descriptor.bytecode[5] == 0x20u &&
        descriptor.bytecode[6] == 4u && descriptor.bytecode[10] == 0x08u && descriptor.bytecode[11] == 0x0cu,
        "Compact name/object operands were not normalized to uint32");
}
void Synthetic() {
    Fixture fixture;
    const auto valid = MakePayload();
    CheckHeader(fixture.Load(valid));
    const auto retained = fixture.Load(valid);
    const auto descriptorBytes = PortableStructRetainedBytes(retained);
    Require(descriptorBytes >= sizeof(PortableStructDescriptor) + retained.rawBytes.size() + retained.bytecode.size() +
        retained.objectPath.size() + retained.friendlyName.ToString().size(), "Descriptor charge omitted retained storage");
    fixture.Reject([&] { LoadPortableStructDescriptor(fixture.package, 0u, descriptorBytes - 1u); },
        "Combined raw/normalized descriptor byte budget ignored");
    CheckHeader(LoadPortableStructDescriptor(fixture.package, 0u, descriptorBytes));
    fixture.Reject([&] { LoadPortableStructDescriptor(fixture.package, 0u, valid.bytes.size() - 1u); },
        "Descriptor input preallocation byte budget ignored");
    // Large valid Nothing-token script, small declaration: both retained copies
    // must count, not just fields. Keep fixture small rather than allocate64MiB.
    const Bytes bigScript(4096u, 0x0bu);
    const auto big = MakePayload(bigScript, static_cast<std::uint32_t>(bigScript.size()));
    fixture.Write(big.bytes);
    fixture.Reject([&] { LoadPortableStructDescriptor(fixture.package, 0u, 7000u); },
        "Large script bypassed aggregate raw/normalized descriptor charge");
    Require(LoadPortableStructDescriptor(fixture.package, 0u, 10000u).bytecode == bigScript,
        "Bounded large valid script rejected");
    fixture.Reset(); fixture.Write(valid.bytes);
    const auto propertyPrefix = LoadPortableExportProperties(fixture.package, 0u);
    Require(propertyPrefix.properties.size() == 6u && propertyPrefix.bytesConsumed == valid.references[0] &&
        propertyPrefix.properties[1].boolValue && propertyPrefix.properties[2].arrayIndex == 0x010203u &&
        propertyPrefix.properties[3].arrayIndex == 5u, "Generated tag prefix did not exercise actual size/array conventions");
    for (const auto version : {std::uint16_t{60u}, std::uint16_t{61u}, std::uint16_t{62u}, std::uint16_t{68u}}) {
        fixture.Reset(); fixture.package.version = version;
        CheckHeader(fixture.Load(valid));
        const auto empty = fixture.Load(MakePayload({}, 0u));
        Require(empty.logicalSize == 0u && empty.rawBytes.empty() && empty.bytecode.empty(),
            "Empty authored Struct script was synthesized");
        const Bytes script = version <= 61u ? Bytes{0x04u} : Bytes{0x04u, 0x0bu};
        Require(fixture.Load(MakePayload(script, static_cast<std::uint32_t>(script.size()))).bytecode == script,
            "Version-specific Return token normalization changed");
    }
    for (const bool nullStack : {false, true}) {
        fixture.Reset(); fixture.package.exports[0].ObjFlags = ObjectFlags::HasStack;
        CheckHeader(fixture.Load(MakePayload(Script(), 28u, true, nullStack)));
    }
    // Local Core.Struct class references must be accepted only when the source
    // package identity actually makes the class Core.Struct, not by suffix.
    fixture.Reset(); fixture.path = fixture.directory / "Core.u";
    fixture.package.sourcePath = fixture.path.string(); fixture.package.exports[0].ObjClass = 2;
    fixture.package.exports[0].ObjOuter = 4; fixture.package.exports[3].ObjOuter = 0;
    fixture.package.exports[1].ObjName = 2;
    Require(fixture.Load(valid).objectPath == "Member.TestStruct", "Local actual Core.Struct identity rejected");
    fixture.Reset();
    for (std::size_t length = 0u; length < valid.bytes.size(); ++length) {
        auto truncated = valid; truncated.bytes.resize(length);
        fixture.Reject([&] { fixture.Load(truncated); }, "Truncated Struct UObject/UStruct/script accepted");
    }
    const auto malformed = [&](const auto& modify, const char* message) {
        fixture.Reset(); auto payload = valid; modify(payload);
        fixture.Reject([&] { fixture.Load(payload); }, message);
    };
    for (std::size_t field = 0u; field < valid.references.size(); ++field) {
        malformed([&](Payload& p) { p.bytes[p.references[field]] = 63u; }, "Out-of-range Struct header reference accepted");
        malformed([&](Payload& p) { p.bytes[p.references[field]] = 0xbfu; }, "Out-of-range imported Struct header reference accepted");
    }
    malformed([](Payload& p) { p.bytes[p.friendly] = 0u; }, "None FriendlyName accepted");
    malformed([](Payload& p) { p.bytes[p.friendly] = 63u; }, "Out-of-range FriendlyName accepted");
    malformed([](Payload& p) { p.bytes[p.friendly] = 0x81u; }, "Negative FriendlyName index accepted");
    malformed([](Payload& p) { ReplaceU32(p.bytes, p.logical, 64u * 1024u * 1024u + 1u); }, "Unbounded logical script accepted");
    malformed([](Payload& p) { ReplaceU32(p.bytes, p.logical, 27u); }, "Token expansion past declared logical size accepted");
    malformed([](Payload& p) { ReplaceU32(p.bytes, p.logical, 29u); }, "Missing extra logical token accepted");
    malformed([](Payload& p) { p.bytes[p.raw + 1u] = 63u; }, "Invalid bytecode name operand accepted");
    malformed([](Payload& p) { p.bytes[p.raw + 3u] = 63u; }, "Invalid bytecode object operand accepted");
    malformed([](Payload& p) { p.bytes[p.raw + 6u] = 63u; }, "Invalid label table name operand accepted");
    malformed([](Payload& p) { p.bytes.push_back(0u); }, "Struct payload trailing byte accepted");
    malformed([](Payload& p) { p.bytes[0] = 63u; }, "Invalid tagged property name accepted");
    malformed([](Payload& p) { p.bytes[10u] = 63u; }, "Invalid tagged struct name accepted");
    fixture.Reset();
    fixture.Reject([&] { fixture.Load(MakePayload(Bytes{0x03u}, 1u)); }, "Unknown bytecode token accepted");
    fixture.Reject([&] { Bytes deep(64u, 0x39u); deep.push_back(0x26u);
        fixture.Load(MakePayload(deep, 65u)); }, "Excessive bytecode recursion accepted");
    fixture.Reject([&] { fixture.Load(MakePayload(Bytes{0x70u, 0x26u}, 2u)); }, "Unterminated native arguments accepted");
    fixture.Reject([&] { fixture.Load(MakePayload(Bytes{0x1fu, 'a'}, 3u)); }, "Unterminated ASCII literal accepted");
    fixture.Reject([&] { fixture.Load(MakePayload(Bytes{0x34u, 'a', 0u}, 5u)); }, "Unterminated Unicode literal accepted");
    fixture.Reject([&] { fixture.Load(MakePayload(Bytes{0x20u, 0xffu, 0xffu, 0xffu, 0xffu, 0x7fu}, 5u)); },
        "Overflowing compact operand accepted");
    for (const std::int32_t meta : {0, -4, -5, -6, 63, -63}) {
        fixture.Reset(); fixture.package.exports[0].ObjClass = meta;
        fixture.Reject([&] { fixture.Load(valid); }, "Class/State/Foreign.Struct or invalid metaclass accepted");
    }
    fixture.Reset(); fixture.Write(valid.bytes);
    fixture.Reject([&] { LoadPortableStructDescriptor(fixture.package, fixture.package.exports.size()); },
        "Out-of-range export index accepted");
    fixture.package.exports[0].ObjOffset = -1;
    fixture.Reject([&] { LoadPortableStructDescriptor(fixture.package, 0u); }, "Negative payload offset accepted");
    fixture.Reset(); fixture.Write(valid.bytes); fixture.package.exports[0].ObjSize = 64 * 1024 * 1024 + 1;
    fixture.Reject([&] { LoadPortableStructDescriptor(fixture.package, 0u); }, "Unbounded payload preallocation accepted");
    fixture.package.exports[0].ObjSize = static_cast<std::int32_t>(valid.bytes.size() + 1u);
    fixture.Reject([&] { LoadPortableStructDescriptor(fixture.package, 0u); }, "Payload beyond source end accepted");
    fixture.Reset(); fixture.Write(valid.bytes); fixture.package.exports[0].ObjOffset = std::numeric_limits<std::int32_t>::max();
    fixture.Reject([&] { LoadPortableStructDescriptor(fixture.package, 0u); }, "Payload offset outside source accepted");
    fixture.Reset(); fixture.package.exports[0].ObjOuter = 1;
    fixture.Reject([&] { fixture.Load(valid); }, "Cyclic export outer identity accepted");
    fixture.Reset(); fixture.package.imports[0].ObjOuter = -2;
    fixture.Reject([&] { fixture.Load(valid); }, "Cyclic metaclass outer identity accepted");
    fixture.Reset(); fixture.package.exports[2].ObjOuter = 3;
    fixture.Reject([&] { fixture.Load(valid); }, "Cyclic scriptText reference outer identity accepted");
    fixture.Reset(); fixture.package.exports[3].ObjOuter = 4;
    auto scriptOnlyCycle = valid; scriptOnlyCycle.bytes[scriptOnlyCycle.references[3]] = 0u;
    fixture.Reject([&] { fixture.Load(scriptOnlyCycle); }, "Cyclic bytecode operand outer identity accepted");
    fixture.Reset(); fixture.package.exports[0].ObjName = 63;
    fixture.Reject([&] { fixture.Load(valid); }, "Invalid export object name accepted");
    fixture.Reset(); fixture.package.exports[0].ObjName = 0;
    fixture.Reject([&] { fixture.Load(valid); }, "None export object name accepted");
    fixture.Reset(); fixture.package.exports[0].ObjBase = 63;
    fixture.Reject([&] { fixture.Load(valid); }, "Invalid export base identity accepted");
    fixture.Reset(); fixture.package.names[4].Name = NameString(std::string(64u * 1024u + 1u, 'N'));
    fixture.Reject([&] { fixture.Load(valid); }, "Unbounded descriptor terminal name accepted");
    fixture.Reset(); fixture.package.names[4].Name = NameString(std::string(40u * 1024u, 'A'));
    fixture.package.names[3].Name = NameString(std::string(40u * 1024u, 'B'));
    fixture.Reject([&] { fixture.Load(valid); }, "Unbounded aggregate descriptor identity accepted");
    fixture.Reset();
    for (unsigned i = 0u; i < 33u; ++i) {
        fixture.package.exports.push_back({0, 0, i == 32u ? 0 : static_cast<std::int32_t>(6u + i),
            14, ObjectFlags{}, 0, 0});
    }
    fixture.package.exports[0].ObjOuter = 5;
    fixture.Reject([&] { fixture.Load(valid); }, "Excessively deep descriptor outer identity accepted");
    fixture.Reset(); fixture.package.exports[0].ObjFlags = ObjectFlags::HasStack;
    auto badStack = MakePayload(Script(), 28u, true); badStack.bytes[0] = 63u;
    fixture.Reject([&] { fixture.Load(badStack); }, "Invalid HasStack function reference accepted");
    fixture.Reset(); fixture.package.exports[0].ObjFlags = ObjectFlags::HasStack;
    badStack = MakePayload(Script(), 28u, true); badStack.bytes[1] = 63u;
    fixture.Reject([&] { fixture.Load(badStack); }, "Invalid HasStack state reference accepted");
    fixture.Reset();
    Bytes tinyTags;
    for (unsigned i = 0u; i < 8192u; ++i) { Index(tinyTags, 7); tinyTags.push_back(0x83u); }
    Index(tinyTags, 0);
    CheckHeader(fixture.Load(MakePayload(Script(), 28u, false, false, tinyTags)));
    std::cout << "PASS full authored Struct descriptor retention " << checks << " checks, " << rejections << " rejection controls\n";
}

void ImportedReferenceProvenance() {
    const auto beforeChecks = checks, beforeRejections = rejections;
    PortablePackageTables package; package.sourcePath = "ValueSource.u"; package.version = 69u;
    for (const char* name : {"None", "Target", "Class", "InventoryClass", "Texture", "Actor", "Npc",
         "OtherPackage", "Group", "Object", "Mesh", "MeshAsset", "Pawn"})
        package.names.push_back({NameString(name), 0u});
    package.imports = {{0, 0, 0, 1}, {7, 2, -1, 3}, {7, 5, -1, 6}, {7, 10, -1, 11}};
    package.exports = {{0, 0, 0, 3, ObjectFlags{}, 0, 0}};
    const auto baseline = package;
    std::size_t resolutions{};
    const auto matches = [&](const std::string& path, const std::string& importedClass) {
        ++resolutions;
        std::vector<std::string> ancestry;
        if (NameString(path) == "Target.InventoryClass") ancestry = {"Core.Class", "Core.State", "Core.Struct", "Core.Field", "Core.Object"};
        else if (NameString(path) == "Target.Npc") ancestry = {"Game.SpecialPawn", "Engine.Pawn", "Engine.Actor", "Core.Object"};
        else if (NameString(path) == "Target.MeshAsset") ancestry = {"Engine.LodMesh", "Engine.Mesh", "Engine.Primitive", "Core.Object"};
        else return false;
        if (NameString(importedClass) == "Class")
            return NameString(ancestry.front().substr(ancestry.front().find_last_of('.') + 1u)) == "Class";
        return std::any_of(ancestry.begin(), ancestry.end(), [&](const auto& cls) {
            return NameString(cls.substr(cls.find_last_of('.') + 1u)) == importedClass;
        });
    };
    const auto reject = [&](const std::function<void()>& action, const std::string& message) {
        bool rejected{};
        try { action(); } catch (const std::runtime_error&) { rejected = true; }
        Require(rejected, message); ++rejections;
    };
    const auto resolve = [&](std::int32_t reference) {
        return ResolvePortableValueObjectReference(package, reference, matches);
    };
    Require(resolve(-2) == "Target.InventoryClass", "Valid imported class reference lost source-package provenance");
    Require(resolve(-3) == "Target.Npc", "Import matching rejected an authored base class in actual ancestry");
    Require(resolve(-4) == "Target.MeshAsset", "Imported native Mesh did not accept actual LodMesh ancestry");
    // FindObjectReference compares ClassName, not ClassPackage, and supports a
    // base class name. Do not replace that with exact qualified-class equality.
    package.imports[2].ClassName = 12;
    Require(resolve(-3) == "Target.Npc", "Valid Pawn-base import was constrained to the concrete SpecialPawn");
    package.imports[2].ClassName = 9;
    Require(resolve(-3) == "Target.Npc", "Valid Object-base import was constrained by ClassPackage");
    const auto beforeScalar = resolutions;
    Require(resolve(0).empty() && resolve(1) == "ValueSource.InventoryClass" && resolutions == beforeScalar,
        "Null/export value reference unexpectedly used imported-class matching");
    package.names[3].Name = NameString("Literal.ExportName");
    Require(resolve(1) == "ValueSource.Literal.ExportName", "Literal dotted export Name was globally prohibited");
    package = baseline;
    QuestVr::AuthoredStructSchema schema; schema.path = "Declaration.InventoryItem"; schema.name = "InventoryItem";
    QuestVr::AuthoredStructField inventory; inventory.key = schema.path + ".Inventory"; inventory.name = "Inventory";
    inventory.zero = QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::Object, {});
    inventory.classReference = true; inventory.referenceClassPath = "Engine.Inventory";
    QuestVr::AuthoredStructField count; count.key = schema.path + ".Count"; count.name = "Count";
    count.zero = QuestVr::Vm::Value::Integer(0); schema.fields = {inventory, count};
    QuestVr::AuthoredStructResolvers resolver;
    resolver.resolveObject = [&](std::int32_t reference, const QuestVr::AuthoredStructField&) { return resolve(reference); };
    Bytes bytes; Index(bytes, -2); U32(bytes, 0xffffffffu);
    const auto value = QuestVr::DecodeAuthoredStructValue(schema, bytes, package.version, resolver);
    Require(value.fields.at("inventory").text == "Target.InventoryClass" && value.fields.at("count").integer == -1,
        "Production struct decoder did not retain imported class and signed count");
    package.imports[1].ClassName = 4;
    reject([&] { QuestVr::DecodeAuthoredStructValue(schema, bytes, package.version, resolver); },
        "Wrong Texture import declaration accepted an otherwise valid Inventory UClass path");
    package = baseline; package.imports[2].ClassName = 2;
    reject([&] { resolve(-3); }, "Class import accepted an actor instance via ancestry");
    package = baseline;
    reject([&] { ResolvePortableValueObjectReference(package, -2); }, "Import accepted without a provenance matcher");
    for (const auto reference : {2, -63, std::numeric_limits<std::int32_t>::min(), -1})
        reject([&] { resolve(reference); }, "Invalid/out-of-range/root-package value reference accepted");
    for (const auto className : {0, 63, -1}) {
        package = baseline; package.imports[1].ClassName = className;
        reject([&] { resolve(-2); }, "Invalid imported ClassName accepted");
    }
    for (const auto outer : {1, -2, -63}) {
        package = baseline; package.imports[1].ObjOuter = outer;
        reject([&] { resolve(-2); }, "Export/cyclic/unavailable import outer accepted");
    }
    for (const auto name : {0, 63}) {
        package = baseline; package.imports[1].ObjName = name;
        reject([&] { resolve(-2); }, "Invalid import object name accepted");
    }
    const auto permit = [](const std::string&, const std::string&) { return true; };
    for (const auto nameIndex : {1, 3}) {
        package = baseline; package.names[static_cast<std::size_t>(nameIndex)].Name = NameString("Group.InventoryClass");
        reject([&] { ResolvePortableValueObjectReference(package, -2, permit); }, "Dotted source import segment aliased a nested target");
        package = baseline; package.names[static_cast<std::size_t>(nameIndex)].Name = NameString(std::string("Target\0Bad", 10u));
        reject([&] { ResolvePortableValueObjectReference(package, -2, permit); }, "Embedded NUL in source import segment accepted");
    }
    package = baseline; package.names[2].Name = NameString(std::string("Class\0Bad", 9u));
    reject([&] { resolve(-2); }, "Embedded NUL in imported class name accepted");
    package = baseline; package.names[2].Name = NameString(std::string(64u * 1024u + 1u, 'C'));
    reject([&] { resolve(-2); }, "Unbounded imported class name accepted");
    package = baseline; package.names[3].Name = NameString(std::string(64u * 1024u + 1u, 'N'));
    reject([&] { resolve(-2); }, "Unbounded value-reference identity accepted");
    package = baseline; package.sourcePath = std::string(64u * 1024u, 'S') + ".u";
    reject([&] { resolve(1); }, "Unbounded qualified export identity accepted");
    package = baseline; package.imports.resize(1u);
    for (unsigned i = 0u; i < 30u; ++i)
        package.imports.push_back({0, 0, -static_cast<std::int32_t>(package.imports.size()), 8});
    package.imports.push_back({0, 2, -static_cast<std::int32_t>(package.imports.size()), 3});
    const auto anyClass = [](const std::string&, const std::string& name) { return NameString(name) == "Class"; };
    Require(!ResolvePortableValueObjectReference(package, -32, anyClass).empty(), "Exact 32-record import outer bound rejected");
    package.imports.push_back({0, 2, -32, 3});
    reject([&] { ResolvePortableValueObjectReference(package, -33, anyClass); }, "Over-depth source import outer chain accepted");
    std::cout << "PASS production-linked imported struct reference provenance " << checks - beforeChecks << " checks, "
        << rejections - beforeRejections << " rejection controls\n";
}

// Independent raw field reader cross-checks exact serialized metadata. This
// optional test only loads three small schemas, not a map/runtime or actor VM.
struct AuditReader {
    const Bytes& bytes; std::size_t pos{};
    std::uint8_t Byte() { if (pos >= bytes.size()) throw std::runtime_error("Truncated audit payload"); return bytes[pos++]; }
    std::uint16_t Word() { const auto a = Byte(); return static_cast<std::uint16_t>(a | (Byte() << 8u)); }
    std::uint32_t Dword() { const auto a = Word(); return a | (static_cast<std::uint32_t>(Word()) << 16u); }
    std::int32_t Compact() {
        const auto first = Byte(); const bool negative = (first & 0x80u) != 0u;
        std::uint64_t magnitude = first & 63u; unsigned shift = 6u; bool more = (first & 0x40u) != 0u;
        while (more) {
            if (shift >= 32u) throw std::runtime_error("Invalid audit compact index");
            const auto next = Byte(); magnitude |= static_cast<std::uint64_t>(next & 127u) << shift;
            more = (next & 0x80u) != 0u; shift += 7u;
        }
        if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
            throw std::runtime_error("Audit compact index overflow");
        return negative ? -static_cast<std::int32_t>(magnitude) : static_cast<std::int32_t>(magnitude);
    }
};
void CompareOriginal(const PortablePackageTables& package, const std::size_t index,
    const PortableStructDescriptor& descriptor) {
    const auto& entry = package.exports[index];
    Require(entry.ObjSize > 0 && entry.ObjSize <= 1024 * 1024, "Original descriptor exceeds lightweight audit bound");
    Bytes bytes(static_cast<std::size_t>(entry.ObjSize));
    std::ifstream file(package.sourcePath, std::ios::binary); file.seekg(entry.ObjOffset);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "Could not read original Struct bytes");
    AuditReader reader{bytes, LoadPortableExportProperties(package, index).bytesConsumed};
    Require(reader.Compact() == descriptor.baseField && reader.Compact() == descriptor.nextField &&
        reader.Compact() == descriptor.scriptText && reader.Compact() == descriptor.children,
        "Original exact UField/UStruct references changed");
    const auto friendly = reader.Compact();
    Require(friendly >= 0 && static_cast<std::size_t>(friendly) < package.names.size() &&
        package.names[static_cast<std::size_t>(friendly)].Name == descriptor.friendlyName,
        "Original FriendlyName changed");
    Require(reader.Dword() == descriptor.line && reader.Dword() == descriptor.textPos &&
        reader.Dword() == descriptor.logicalSize, "Original source/logical metadata changed");
    Require(reader.pos <= bytes.size() && descriptor.rawBytes.size() == bytes.size() - reader.pos &&
        std::equal(descriptor.rawBytes.begin(), descriptor.rawBytes.end(), bytes.begin() + static_cast<std::ptrdiff_t>(reader.pos)),
        "Original exact raw script/termination changed");
    Require(descriptor.bytecode.size() == descriptor.logicalSize && descriptor.objectPath ==
        GetPortableObjectPath(package, static_cast<std::int32_t>(index + 1u)),
        "Original normalized logical size/object identity changed");
}
std::vector<PortablePropertyDescriptor> Members(const PortablePackageTables& package,
    const PortableStructDescriptor& descriptor) {
    std::vector<PortablePropertyDescriptor> members;
    std::set<std::int32_t> visited;
    auto child = descriptor.children;
    while (child != 0) {
        Require(child > 0 && static_cast<std::size_t>(child) <= package.exports.size() &&
            visited.size() < 64u && visited.insert(child).second, "Invalid original Struct member chain");
        const auto property = LoadPortablePropertyDescriptor(package, static_cast<std::size_t>(child - 1));
        Require(property.outerPath == descriptor.objectPath, "Original member belongs to another Struct");
        std::cout << "  MEMBER ref=" << child << " path=" << property.objectPath << " type=" << property.type <<
            " dim=" << property.arrayDimension << " flags=" << property.flags << " next=" << property.nextField <<
            " referenced=" << GetPortableObjectPath(package, property.referencedType) <<
            " secondary=" << GetPortableObjectPath(package, property.secondaryType) << '\n';
        members.push_back(property); child = property.nextField;
    }
    return members;
}
void AuditOriginal(const std::filesystem::path& root) {
    const auto core = LoadPortablePackageTables((root / "System" / "Core.u").string());
    const auto engine = LoadPortablePackageTables((root / "System" / "Engine.u").string());
    const auto deusEx = LoadPortablePackageTables((root / "System" / "DeusEx.u").string());
    const auto audit = [&](const PortablePackageTables& package, const char* path) {
        const auto index = FindPortableExport(package, path);
        const auto descriptor = LoadPortableStructDescriptor(package, index);
        CompareOriginal(package, index, descriptor);
        std::cout << "ORIGINAL STRUCT " << std::filesystem::path(package.sourcePath).stem().string() << '.' <<
            descriptor.objectPath << " ref=" << index + 1u << " base=" << descriptor.baseField <<
            " next=" << descriptor.nextField << " children=" << descriptor.children <<
            " scriptText=" << descriptor.scriptText << " friendly=" << descriptor.friendlyName.ToString() <<
            " line=" << descriptor.line << " textPos=" << descriptor.textPos <<
            " logical=" << descriptor.logicalSize << " raw=" << descriptor.rawBytes.size() << '\n';
        Require(descriptor.baseField == 0 && descriptor.logicalSize == 0u && descriptor.rawBytes.empty(),
            "Original lightweight Struct unexpectedly has inheritance or script");
        return Members(package, descriptor);
    };
    const auto inventory = audit(deusEx, "ScriptedPawn.InventoryItem");
    Require(inventory.size() == 2u && inventory[0].objectPath == "ScriptedPawn.InventoryItem.Inventory" &&
        inventory[0].type == "ClassProperty" && inventory[0].arrayDimension == 1 &&
        NameString(GetPortableObjectPath(deusEx, inventory[0].referencedType)) == "Core.Class" &&
        NameString(GetPortableObjectPath(deusEx, inventory[0].secondaryType)) == "Engine.Inventory" &&
        inventory[1].objectPath == "ScriptedPawn.InventoryItem.Count" && inventory[1].type == "IntProperty" &&
        inventory[1].arrayDimension == 1, "Original InventoryItem layout changed");
    const auto initialIndex = FindPortableExport(deusEx, "ScriptedPawn.InitialInventory");
    const auto initial = LoadPortablePropertyDescriptor(deusEx, initialIndex);
    Require(initial.type == "StructProperty" && initial.arrayDimension == 8 &&
        NameString(GetPortableObjectPath(deusEx, initial.referencedType)) == "ScriptedPawn.InventoryItem",
        "Original InitialInventory declaration changed");
    std::cout << "ORIGINAL INITIAL INVENTORY ref=" << initialIndex + 1u << " dim=" << initial.arrayDimension <<
        " struct=" << GetPortableObjectPath(deusEx, initial.referencedType) << '\n';
    const auto vector = audit(core, "Object.Vector");
    Require(vector.size() == 3u && vector[0].type == "FloatProperty" && vector[1].type == "FloatProperty" &&
        vector[2].type == "FloatProperty", "Original Vector members changed");
    const auto region = audit(engine, "Actor.PointRegion");
    Require(region.size() == 3u && region[0].type == "ObjectProperty" && region[1].type == "IntProperty" &&
        region[2].type == "ByteProperty", "Original PointRegion members changed");
    std::cout << "PASS original InventoryItem/Vector/PointRegion raw metadata and ordered member audit\n";
}
} // namespace

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    try {
        Synthetic();
        ImportedReferenceProvenance();
        if (argc == 3 && std::string(argv[1]) == "--audit-original") AuditOriginal(argv[2]);
        else Require(argc == 1, "Usage: portable_struct_descriptor_test [--audit-original GAME_ROOT]");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
