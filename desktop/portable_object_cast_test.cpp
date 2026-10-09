#include "Precomp.h"
#include "portable_unreal_runtime.h"

#include <algorithm>
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
using Value = QuestVr::Vm::Value;
using Kind = QuestVr::Vm::Kind;
std::size_t checks{}, rejections{};
void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
template<class Action> void Reject(Action action, const std::string& message) {
    bool rejected{};
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected, message); ++rejections;
}
void U16(Bytes& bytes, const std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
}
void U32(Bytes& bytes, const std::uint32_t value) {
    U16(bytes, static_cast<std::uint16_t>(value));
    U16(bytes, static_cast<std::uint16_t>(value >> 16u));
}
void Replace32(Bytes& bytes, const std::size_t offset, const std::uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i)
        bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8u * i));
}
void Index(Bytes& bytes, const std::int32_t value) {
    auto magnitude = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((value < 0 ? 0x80u : 0u) | (magnitude & 63u));
    magnitude >>= 6u;
    if (magnitude != 0u) first |= 64u;
    bytes.push_back(first);
    while (magnitude != 0u) {
        auto next = static_cast<std::uint8_t>(magnitude & 127u);
        magnitude >>= 7u;
        if (magnitude != 0u) next |= 128u;
        bytes.push_back(next);
    }
}
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    Require(size <= 1u << 20u, "Generated metadata/snapshot exceeded fixture read bound");
    Bytes result(static_cast<std::size_t>(size));
    std::ifstream file(path, std::ios::binary);
    Require(static_cast<bool>(file), "Cannot inspect generated metadata/snapshot");
    file.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
    Require(static_cast<bool>(file), "Cannot read generated metadata/snapshot");
    return result;
}
struct Package {
    std::string stem;
    std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
    std::int32_t Name(const std::string& text) {
        const auto found = std::find(names.begin(), names.end(), text);
        if (found != names.end()) return static_cast<std::int32_t>(found - names.begin());
        names.push_back(text); return static_cast<std::int32_t>(names.size() - 1u);
    }
    std::int32_t Import(const std::string& name, const std::int32_t outer,
        const std::string& type = "Class", const std::string& sourceClassPackage = "Core") {
        imports.push_back({Name(sourceClassPackage), Name(type), outer, Name(name)});
        return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) { return Import(name, 0, "Package"); }
    std::int32_t Export(const std::string& name, const std::int32_t cls = 0,
        const std::int32_t base = 0, const std::int32_t outer = 0) {
        exports.push_back({cls, base, outer, Name(name), ObjectFlags{}, 0, -1});
        return static_cast<std::int32_t>(exports.size());
    }
    Bytes Serialize() const {
        // Complete v68 header, including GUID and a zero generation count.
        // No export has a body: the adapter under test needs actual table
        // identities/ObjClass/ObjBase, not invented script or startup state.
        Bytes bytes; U32(bytes, 0x9e2a83c1u); U16(bytes, 68u); U16(bytes, 0u);
        for (unsigned i = 0u; i < 7u; ++i) U32(bytes, 0u);
        bytes.resize(56u, 0u);
        Replace32(bytes, 12u, static_cast<std::uint32_t>(names.size()));
        Replace32(bytes, 16u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& name : names) {
            Index(bytes, static_cast<std::int32_t>(name.size() + 1u));
            bytes.insert(bytes.end(), name.begin(), name.end()); bytes.push_back(0u); U32(bytes, 0u);
        }
        Replace32(bytes, 20u, static_cast<std::uint32_t>(exports.size()));
        Replace32(bytes, 24u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& entry : exports) {
            Index(bytes, entry.ObjClass); Index(bytes, entry.ObjBase);
            U32(bytes, static_cast<std::uint32_t>(entry.ObjOuter)); Index(bytes, entry.ObjName);
            U32(bytes, static_cast<std::uint32_t>(entry.ObjFlags)); Index(bytes, 0);
        }
        Replace32(bytes, 28u, static_cast<std::uint32_t>(imports.size()));
        Replace32(bytes, 32u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& entry : imports) {
            Index(bytes, entry.ClassPackage); Index(bytes, entry.ClassName);
            U32(bytes, static_cast<std::uint32_t>(entry.ObjOuter)); Index(bytes, entry.ObjName);
        }
        return bytes;
    }
};
struct Fixture {
    std::filesystem::path directory;
    std::map<std::filesystem::path, Bytes> originalBytes;
    Fixture() {
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0u; attempt < 20u; ++attempt) {
            const auto candidate = parent / ("deusex-object-cast-test-" +
                std::to_string(stamp) + '-' + std::to_string(attempt));
            if (!std::filesystem::create_directory(candidate)) continue;
            directory = std::filesystem::canonical(candidate);
            Require(directory.parent_path() == parent &&
                directory.filename().string().rfind("deusex-object-cast-test-", 0u) == 0u,
                "Generated cast fixture escaped temporary parent");
            break;
        }
        Require(!directory.empty(), "Cannot create isolated object cast fixture");
    }
    ~Fixture() {
        ShutdownPortableRuntime();
        std::error_code ignored;
        if (!directory.empty()) std::filesystem::remove_all(directory, ignored);
    }
    PortablePackageTables Write(const Package& package) {
        const auto path = directory / (package.stem + ".u");
        const auto bytes = package.Serialize();
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            Require(static_cast<bool>(file), "Cannot create generated UE1 cast package");
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            Require(static_cast<bool>(file), "Cannot write generated UE1 cast package");
        }
        originalBytes.emplace(path, bytes);
        auto table = LoadPortablePackageTables(path.string());
        Require(table.version == 68u && table.names.size() == package.names.size() &&
            table.exports.size() == package.exports.size() && table.imports.size() == package.imports.size(),
            "Generated package did not load through real UE1 table reader");
        for (std::size_t i = 0u; i < package.exports.size(); ++i) {
            const auto& expected = package.exports[i]; const auto& actual = table.exports[i];
            Require(actual.ObjClass == expected.ObjClass && actual.ObjBase == expected.ObjBase &&
                actual.ObjOuter == expected.ObjOuter && actual.ObjSize == 0 &&
                table.names.at(static_cast<std::size_t>(actual.ObjName)).Name ==
                    NameString(package.names.at(static_cast<std::size_t>(expected.ObjName))),
                "Actual loaded export table differs from generated metadata");
        }
        return table;
    }
    void Initialize(const std::vector<PortablePackageTables>& tables, const std::size_t expectedObjects,
        const std::size_t expectedClasses) {
        const auto summary = InitializePortableRuntime(tables);
        Require(!summary.passed && summary.objects == expectedObjects && summary.classes == expectedClasses &&
            summary.functions == 0u && summary.states == 0u && summary.properties == 0u &&
            summary.normalizedBytecodeBytes == 0u && summary.serializedClassDefaults == 0u,
            "Metadata-only runtime summary gained fake script/startup success");
        UnchangedState();
    }
    void UnchangedState() const {
        Require(!GetPortableRuntimeScriptStatePresent() && !GetPortableRuntimeUnsavedScriptState() &&
            GetPortableRuntimeInventoryCount() == 0u && GetPortableRuntimeMapActors().empty(),
            "Read-only cast inspection created actor/script/inventory state");
    }
    Bytes Snapshot(const std::string& name) const {
        const auto path = directory / (name + ".sav");
        Require(SavePortableRuntimeState(path.string()), "Cannot snapshot generated empty runtime");
        return ReadBytes(path);
    }
    void CheckFiles() const {
        for (const auto& [path, bytes] : originalBytes)
            Require(ReadBytes(path) == bytes, "Read-only cast modified generated source metadata");
    }
};
Value Object(const std::string& path) { return Value::Text(Kind::Object, path); }
void Cast(const std::string& source, const std::int32_t reference, const Value& operand,
    const bool meta, const std::string& expected) {
    const auto before = operand;
    auto result = CastPortableRuntimeObject(source, reference, operand, meta);
    Require(result.kind == Kind::Object && result.text == expected && result.fields.empty(),
        std::string(meta ? "MetaCast" : "DynamicCast") + " result mismatch from " + source);
    result.text = "changed detached result";
    Require(QuestVr::Vm::Equal(operand, before), "Cast result aliases or mutates operand value");
}
void Synthetic() {
    Fixture fixture;
    Package one; one.stem = "CastOne";
    const auto missingPackage = one.ImportPackage("AbsentPackage");
    const auto missingParent = one.Import("Parent", missingPackage);
    const auto foreignPackage = one.ImportPackage("CastTwo");
    const auto foreignParent = one.Import("Shared", foreignPackage);
    const auto foreignObject = one.Import("Shared", foreignPackage, "Object");
    const auto foreignActor = one.Import("Shared", foreignPackage, "Actor");
    const auto foreignStruct = one.Import("Shared", foreignPackage, "Struct");
    const auto foreignDotted = one.Import("Literal.Dotted", foreignPackage);
    const auto base = one.Export("Base");
    const auto derived = one.Export("Derived", 0, base);
    const auto item = one.Export("Item", derived);
    const auto unrelated = one.Export("Unrelated");
    const auto dotted = one.Export("Literal.Dotted");
    one.Export("DottedItem", dotted);
    const auto leaf = one.Export("Leaf"); one.Export("LeafItem", leaf);
    const auto zero = one.Export("ZeroBase");
    const auto objectRoot = one.Export("Object");
    const auto cycleA = one.Export("CycleA");
    const auto cycleB = one.Export("CycleB", 0, cycleA);
    one.exports.at(static_cast<std::size_t>(cycleA - 1)).ObjBase = cycleB;
    const auto missing = one.Export("MissingChild", 0, missingParent);
    const auto wrongParent = one.Export("WrongParent", 0, item);
    const auto deepFirst = static_cast<std::int32_t>(one.exports.size() + 1u);
    for (std::int32_t i = 0; i < 129; ++i)
        one.Export("Deep" + std::to_string(i), 0, i == 128 ? 0 : deepFirst + i + 1);
    const auto foreign = one.Export("ForeignChild", 0, foreignParent);
    const auto highName = std::string("High") + static_cast<char>(0xc4u);
    const auto high = one.Export(highName); one.Export("HighItem", high);
    one.Export("BadClassObject", foreignObject);
    one.Export("BadClassActor", foreignActor);
    one.Export("BadClassDotted", foreignDotted);
    one.Export("BadClassKind", item);
    one.Export("BadBaseObject", 0, foreignObject);
    one.Export("BadBaseStruct", 0, foreignStruct);
    one.Export("BadBaseDotted", 0, foreignDotted);

    Package two; two.stem = "CastTwo";
    const auto shared = two.Export("Shared");
    const auto secondDerived = two.Export("Derived", 0, shared);
    two.Export("Item", secondDerived);
    const auto secondLeaf = two.Export("Leaf");
    const auto secondDotted = two.Export("Literal.Dotted");
    const auto dottedLeafOnly = two.Export("Dotted");
    const auto secondHigh = two.Export(std::string("hIGH") + static_cast<char>(0xc4u));
    const auto distinctHigh = two.Export(std::string("High") + static_cast<char>(0xe4u));

    Package imports; imports.stem = "CastImports";
    imports.Export("DeclaringObject");
    const auto corePackage = imports.ImportPackage("Core");
    std::map<std::string, std::int32_t> native;
    for (const char* name : {"Object", "Class", "State", "Struct", "Field", "Function", "Property"})
        native.emplace(name, imports.Import(name, corePackage));
    const auto unknownNative = imports.Import("UnregisteredNative", corePackage);
    const auto firstPackage = imports.ImportPackage("CastOne");
    const auto importedBase = imports.Import("Base", firstPackage);
    const auto importedDerived = imports.Import("Derived", firstPackage);
    const auto importedInstance = imports.Import("Item", firstPackage);
    std::vector<std::int32_t> wrongImportClasses;
    for (const char* name : {"Object", "Struct", "State", "Actor"})
        wrongImportClasses.push_back(imports.Import("Base", firstPackage, name));
    const auto ignoredClassPackage = imports.Import("Base", firstPackage, "Class", "UnrelatedClassPackage");
    const auto absentPackage = imports.ImportPackage("AbsentPackage");
    const auto absentTarget = imports.Import("AbsentClass", absentPackage);
    const auto enginePackage = imports.ImportPackage("Engine");
    const auto mesh = imports.Import("Mesh", enginePackage);
    const auto lodMesh = imports.Import("LodMesh", enginePackage);
    const auto primitive = imports.Import("Primitive", enginePackage);
    const auto outerExport = imports.Import("Base", 1);
    const auto dottedImport = imports.Import("Literal.Dotted", firstPackage);

    const auto oneTable = fixture.Write(one), twoTable = fixture.Write(two), importTable = fixture.Write(imports);
    const auto objectCount = one.exports.size() + two.exports.size() + imports.exports.size();
    // Eight ordinary instances in CastOne and one in CastTwo, all other exports UClass.
    fixture.Initialize({oneTable, twoTable, importTable}, objectCount, objectCount - 9u);
    const auto snapshot = fixture.Snapshot("BeforeCasts");
    const std::string receiver = "CastImports.DeclaringObject";
    Cast("CastOne.Derived", base, Object("CastOne.Derived"), true, "CastOne.Derived");
    Cast("CastOne.Item", derived, Object("CastOne.Derived"), true, "CastOne.Derived");
    Cast("CastOne.Derived", derived, Object("CastOne.Base"), true, {});
    Cast("CastOne.Derived", unrelated, Object("CastOne.Derived"), true, {});
    Cast("CastOne.Item", base, Object("CastOne.Item"), true, {});
    Cast("CastOne.Item", base, Object("castone.ITEM"), false, "CastOne.Item");
    Cast("CastOne.Item", derived, Object("CastOne.Item"), false, "CastOne.Item");
    Cast("CastOne.Item", unrelated, Object("CastOne.Item"), false, {});
    Cast("CastOne.Item", base, Object("CastOne.Derived"), false, {});
    // A normalized integer belongs to its declaring SOURCE package, not the operand.
    Require(base == shared, "Cross-package source-table probe must use the same integer");
    Cast("CastOne.Item", base, Object("CastOne.Base"), true, "CastOne.Base");
    Cast("CastTwo.Item", shared, Object("CastOne.Base"), true, {});
    Cast("CastTwo.Item", shared, Object("CastTwo.Derived"), true, "CastTwo.Derived");
    Cast("CastTwo.Item", secondLeaf, Object("CastOne.Leaf"), true, {});
    Cast("CastTwo.Item", secondLeaf, Object("CastOne.LeafItem"), false, "CastOne.LeafItem");
    Cast("CastTwo.Item", secondDotted, Object("CastOne.DottedItem"), false, "CastOne.DottedItem");
    Cast("CastTwo.Item", dottedLeafOnly, Object("CastOne.DottedItem"), false, {});
    Cast("CastTwo.Item", secondDotted, Object("CastOne.Literal.Dotted"), true, {});
    Cast("CastTwo.Item", secondHigh, Object("CastOne.HighItem"), false, "CastOne.HighItem");
    Cast("CastTwo.Item", distinctHigh, Object("CastOne.HighItem"), false, {});
    Cast("CastTwo.Item", shared, Object("CastOne.ForeignChild"), true, "CastOne.ForeignChild");
    Cast("CastOne.Item", foreign, Object("CastOne.ForeignChild"), true, "CastOne.ForeignChild");
    Cast(receiver, importedBase, Object("CastOne.Derived"), true, "CastOne.Derived");
    Cast(receiver, importedDerived, Object("CastOne.Item"), false, "CastOne.Item");
    Cast(receiver, ignoredClassPackage, Object("CastOne.Derived"), true, "CastOne.Derived");
    for (const auto reference : wrongImportClasses)
        Reject([&] { (void)CastPortableRuntimeObject(receiver, reference, Object("CastOne.Base"), true); },
            "Non-Class import resolved a serialized ObjClass=0 UClass through metaclass ancestry");
    for (const char* path : {"CastOne.BadClassObject", "CastOne.BadClassActor", "CastOne.BadClassDotted", "CastOne.BadClassKind"}) {
        for (const bool meta : {false, true})
            Reject([&] { (void)CastPortableRuntimeObject(receiver, importedBase, Object(path), meta); },
                "Operand ObjClass accepted a wrong-kind/import-provenance/dotted-alias class reference");
    }
    for (const char* path : {"CastOne.BadBaseObject", "CastOne.BadBaseStruct", "CastOne.BadBaseDotted"})
        Reject([&] { (void)CastPortableRuntimeObject(receiver, importedBase, Object(path), true); },
            "Operand ObjBase accepted wrong import provenance or dotted import alias");
    for (const bool meta : {false, true}) {
        Cast(receiver, importedBase, Object({}), meta, {});
        Cast(receiver, importedBase, Value{}, meta, {});
        for (const auto reference : {0, std::numeric_limits<std::int32_t>::min(),
                std::numeric_limits<std::int32_t>::max(), unknownNative, absentTarget, importedInstance,
                outerExport, dottedImport, firstPackage})
            Reject([&] { (void)CastPortableRuntimeObject(receiver, reference, Value{}, meta); },
                "Malformed/missing/wrong-kind/null cast target accepted for a null operand");
        Reject([&] { (void)CastPortableRuntimeObject("CastOne.Item", item, Object("CastOne.Item"), meta); },
            "Ordinary object export accepted as cast target");
        Reject([&] { (void)CastPortableRuntimeObject(receiver, importedBase, Object("Missing.Object"), meta); },
            "Missing nonnull operand treated as cast None");
        Reject([&] { (void)CastPortableRuntimeObject("Missing.Declaration", importedBase, Value{}, meta); },
            "Missing source table/declaration accepted");
        for (const auto& value : {Value::Integer(0), Value::Bool(false), Value::Float(0.0f),
                Value::Text(Kind::String, {}), Value::Text(Kind::Name, "None"), Value::Vector({})})
            Reject([&] { (void)CastPortableRuntimeObject(receiver, importedBase, value, meta); },
                "Non-object scalar converted to None by runtime cast");
    }
    // Native Core metadata target ancestry is the class object's ACTUAL Class,
    // not the actor/asset class that the UClass operand represents.
    for (const char* name : {"Class", "State", "Struct", "Field", "Object"})
        Cast(receiver, native.at(name), Object("CastOne.Derived"), false, "CastOne.Derived");
    Cast(receiver, native.at("Function"), Object("CastOne.Derived"), false, {});
    Cast(receiver, native.at("Class"), Object("CastOne.Derived"), true, {});
    Cast(receiver, native.at("Object"), Object("CastOne.ZeroBase"), true, "CastOne.ZeroBase");
    Cast(receiver, native.at("Object"), Object("CastOne.Object"), true, {});
    Cast("CastOne.Item", objectRoot, Object("CastOne.Item"), false, "CastOne.Item");
    Cast("CastOne.Item", zero, Object("CastOne.ZeroBase"), true, "CastOne.ZeroBase");
    Cast(receiver, native.at("Object"), Object("Core.IntProperty"), true, "Core.IntProperty");
    Cast(receiver, native.at("Property"), Object("Core.IntProperty"), true, "Core.IntProperty");
    Cast(receiver, native.at("Property"), Object("Core.IntProperty"), false, {});
    Cast(receiver, native.at("Class"), Object("Core.IntProperty"), false, "Core.IntProperty");
    Cast(receiver, mesh, Object("Engine.LodMesh"), true, "Engine.LodMesh");
    Cast(receiver, primitive, Object("Engine.LodMesh"), true, "Engine.LodMesh");
    Cast(receiver, lodMesh, Object("Engine.Mesh"), true, {});
    Cast(receiver, native.at("Class"), Object("Engine.LodMesh"), false, "Engine.LodMesh");
    Cast(receiver, mesh, Object("Engine.LodMesh"), false, {});
    // Preserve early match; reject only when an unmatched traversal reaches an
    // actually missing/wrong-kind/cyclic base or exceeds the 128-class bound.
    Cast("CastOne.Item", cycleA, Object("CastOne.CycleA"), true, "CastOne.CycleA");
    Cast("CastOne.Item", cycleB, Object("CastOne.CycleA"), true, "CastOne.CycleA");
    Reject([&] { (void)CastPortableRuntimeObject("CastOne.Item", missing, Object("CastOne.MissingChild"), true); },
        "Matching class concealed an immediately missing nonzero parent reference");
    Reject([&] { (void)CastPortableRuntimeObject("CastOne.Item", wrongParent, Object("CastOne.WrongParent"), true); },
        "Matching class concealed an immediately wrong-kind parent reference");
    Cast("CastOne.Item", deepFirst + 127, Object("CastOne.Deep0"), true, "CastOne.Deep0");
    for (const auto& path : {"CastOne.CycleA", "CastOne.CycleB", "CastOne.MissingChild", "CastOne.WrongParent", "CastOne.Deep0"})
        Reject([&] { (void)CastPortableRuntimeObject("CastOne.Item", unrelated, Object(path), true); },
            "Malformed/unbounded nonmatching class graph quietly returned None");
    Reject([&] { (void)CastPortableRuntimeObject("CastOne.Item", deepFirst + 128, Object("CastOne.Deep0"), true); },
        "Depth-129 matching class bypassed hierarchy budget");
    fixture.UnchangedState();
    Require(fixture.Snapshot("AfterCasts") == snapshot,
        "Read-only cast or rejection changed complete runtime checkpoint");

    // A serialized canonical Core export takes precedence over the fallback
    // registrations. This deliberately different declared Class base is an
    // adapter precedence fixture, not a replacement for original Core.u.
    Package core; core.stem = "Core";
    const auto coreObject = core.Export("Object");
    core.Export("Class", 0, coreObject);
    const auto coreTable = fixture.Write(core);
    fixture.Initialize({oneTable, twoTable, importTable, coreTable}, objectCount + 2u, objectCount - 7u);
    Cast(receiver, native.at("Object"), Object("CastOne.Derived"), false, "CastOne.Derived");
    Cast(receiver, native.at("Class"), Object("CastOne.Derived"), false, "CastOne.Derived");
    Cast(receiver, native.at("State"), Object("CastOne.Derived"), false, {});
    Cast(receiver, native.at("Class"), Object("Core.Class"), true, "Core.Class");
    Cast(receiver, native.at("State"), Object("Core.Class"), true, {});
    fixture.UnchangedState(); fixture.CheckFiles();
    std::cout << "PASS source-backed runtime object casts " << checks << " checks, " << rejections
        << " rejections; packages=4 metadata-only exports=" << objectCount + 2u << '\n';
}
} // namespace

int main() {
    try { Synthetic(); return 0; }
    catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; ShutdownPortableRuntime(); return 1;
    }
}
