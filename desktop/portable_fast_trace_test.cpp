#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_model_geometry.h"
#include "quest_model_fast_line_check.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Generated genuine v68 packages exercise the production interpreter/native
// adapter. Their single-plane expectations are independent of the production
// BSP helper. Owned-map comparisons prove adapter/model binding only; separate
// helper tests own the retail x87 algorithm evidence. No original EXE/DLL runs.
namespace {
using Bytes = std::vector<std::uint8_t>;
using Value = QuestVr::Vm::Value;
using Kind = QuestVr::Vm::Kind;
using Evaluation = QuestVr::Vm::Evaluation;
using Point = std::array<float, 3u>;
std::size_t checks{}, refusals{};

void Require(const bool condition, const std::string& reason) {
    ++checks;
    if (!condition) throw std::runtime_error(reason);
}
void U16(Bytes& bytes, const std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
}
void U32(Bytes& bytes, const std::uint32_t value) {
    U16(bytes, static_cast<std::uint16_t>(value)); U16(bytes, static_cast<std::uint16_t>(value >> 16u));
}
void U64(Bytes& bytes, const std::uint64_t value) {
    U32(bytes, static_cast<std::uint32_t>(value)); U32(bytes, static_cast<std::uint32_t>(value >> 32u));
}
void Float(Bytes& bytes, const float value) {
    std::uint32_t bits{}; std::memcpy(&bits, &value, sizeof(bits)); U32(bytes, bits);
}
void Index(Bytes& bytes, const std::int32_t value) {
    auto remaining = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((remaining & 63u) | (value < 0 ? 128u : 0u));
    remaining >>= 6u; if (remaining) first |= 64u; bytes.push_back(first);
    while (remaining) {
        auto next = static_cast<std::uint8_t>(remaining & 127u); remaining >>= 7u;
        if (remaining) next |= 128u;
        bytes.push_back(next);
    }
}
void Append(Bytes& bytes, const Bytes& other) { bytes.insert(bytes.end(), other.begin(), other.end()); }
void Replace32(Bytes& bytes, const std::size_t offset, const std::uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8u * i));
}
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    Require(size <= 512u * 1024u * 1024u, "FastTrace source/save exceeds independent inspection bound");
    Bytes bytes(static_cast<std::size_t>(size)); std::ifstream file(path, std::ios::binary);
    Require(static_cast<bool>(file), "Cannot inspect FastTrace source/save");
    if (!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "FastTrace source/save was truncated"); return bytes;
}
void WriteBytes(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    Require(static_cast<bool>(file), "Cannot create generated FastTrace package");
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "Cannot write generated FastTrace package");
}

struct Code { Bytes raw; std::size_t logical{}; };
Code Token(const std::uint8_t op) { return {{op}, 1u}; }
Code Ref(const std::uint8_t op, const std::int32_t reference) {
    Code code{{op}, 5u}; Index(code.raw, reference); return code;
}
Code Integer(const std::int32_t value) {
    Code code{{0x1du}, 5u}; U32(code.raw, static_cast<std::uint32_t>(value)); return code;
}
Code Vector(const Point value) {
    Code code{{0x23u}, 13u}; for (const auto component : value) Float(code.raw, component); return code;
}
Code Join(const std::initializer_list<Code> parts) {
    Code code; for (const auto& part : parts) { Append(code.raw, part.raw); code.logical += part.logical; } return code;
}
Code Assign(Code left, Code right) { return Join({Token(0x0fu), std::move(left), std::move(right)}); }
Code Return(Code value = Token(0x0bu)) { return Join({Token(0x04u), std::move(value)}); }
Code Native(const std::uint16_t index, const std::initializer_list<Code> arguments = {}) {
    Code code;
    if (index >= 256u) {
        code.raw = {static_cast<std::uint8_t>(0x60u + (index >> 8u)), static_cast<std::uint8_t>(index)};
        code.logical = 2u;
    } else { code.raw = {static_cast<std::uint8_t>(index)}; code.logical = 1u; }
    for (const auto& argument : arguments) { Append(code.raw, argument.raw); code.logical += argument.logical; }
    return Join({code, Token(0x16u)});
}
Code Final(const std::int32_t function, const std::initializer_list<Code> arguments = {}) {
    Code code = Ref(0x1cu, function);
    for (const auto& argument : arguments) { Append(code.raw, argument.raw); code.logical += argument.logical; }
    return Join({code, Token(0x16u)});
}
Code Context(Code target, Code expression, const std::uint8_t zeroSize = 0u) {
    Require(expression.logical <= 65535u, "Generated FastTrace Context skip exceeds bound");
    Code code = Join({Token(0x19u), std::move(target)});
    U16(code.raw, static_cast<std::uint16_t>(expression.logical)); code.raw.push_back(zeroSize); code.logical += 3u;
    Append(code.raw, expression.raw); code.logical += expression.logical; return code;
}

// Compact independent copy of the existing genuine-package fixture writer.
// Only files under the fixture's verified unique temporary directory are made.
struct Package {
    std::string stem;
    std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
    std::vector<std::function<Bytes(std::int32_t, std::int32_t)>> builders;
    std::map<std::int32_t, std::vector<std::int32_t>> children;
    std::int32_t Name(const std::string& name) {
        const auto found = std::find(names.begin(), names.end(), name);
        if (found != names.end()) return static_cast<std::int32_t>(found - names.begin());
        names.push_back(name); return static_cast<std::int32_t>(names.size() - 1u);
    }
    std::int32_t Import(const std::string& name, const std::int32_t outer, const std::string& type = "Class") {
        imports.push_back({Name("Core"), Name(type), outer, Name(name)});
        return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) { return Import(name, 0, "Package"); }
    std::int32_t Export(const std::string& name, const std::int32_t cls = 0, const std::int32_t base = 0,
        const std::int32_t outer = 0, const bool field = false) {
        exports.push_back({cls, base, outer, Name(name), ObjectFlags{}, 0, -1}); builders.emplace_back();
        const auto reference = static_cast<std::int32_t>(exports.size());
        if (field) children[outer].push_back(reference);
        return reference;
    }
    std::int32_t Property(const std::string& name, const std::string& type, const std::int32_t owner,
        const std::int32_t core, const std::int32_t target = 0, const std::uint32_t flags = 0u,
        const std::uint32_t dimension = 1u) {
        const auto reference = Export(name, Import(type, core), 0, owner, true);
        builders.at(static_cast<std::size_t>(reference - 1)) = [type, target, flags, dimension](const std::int32_t next, std::int32_t) {
            Bytes body{0u}; Index(body, 0); Index(body, next); U32(body, dimension); U32(body, flags); Index(body, 0);
            if (type == "ObjectProperty" || type == "StructProperty" || type == "ByteProperty") Index(body, target);
            return body;
        }; return reference;
    }
    void ClassBody(const std::int32_t reference) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference](const std::int32_t next, const std::int32_t first) {
            Bytes body; Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjBase);
            Index(body, next); Index(body, 0); Index(body, first); Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName);
            U32(body, 0); U32(body, 0); U32(body, 0); U64(body, ~std::uint64_t{}); U64(body, ~std::uint64_t{});
            U16(body, 0xffffu); U32(body, 0); U32(body, 0); body.resize(body.size() + 16u, 0u);
            for (unsigned i = 0u; i < 5u; ++i) Index(body, 0);
            return body;
        };
    }
    void StructBody(const std::int32_t reference) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference](const std::int32_t next, const std::int32_t first) {
            Bytes body{0u}; Index(body, 0); Index(body, next); Index(body, 0); Index(body, first);
            Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName);
            U32(body, 0); U32(body, 0); U32(body, 0); return body;
        };
    }
    std::int32_t Function(const std::string& name, const std::int32_t owner, const std::int32_t core,
        Code code = {}, const std::uint16_t native = 0u, const std::uint32_t flags = 0u) {
        const auto reference = Export(name, Import("Function", core), 0, owner, true);
        FunctionCode(reference, std::move(code), native, flags); return reference;
    }
    void FunctionCode(const std::int32_t reference, Code code, const std::uint16_t native = 0u,
        const std::uint32_t flags = 0u) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference, native, flags, code = std::move(code)]
            (const std::int32_t next, const std::int32_t first) {
            Bytes body{0u}; Index(body, 0); Index(body, next); Index(body, 0); Index(body, first);
            Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName);
            U32(body, 0); U32(body, 0); U32(body, static_cast<std::uint32_t>(code.logical)); Append(body, code.raw);
            U16(body, native); body.push_back(0u); U32(body, flags ? flags : native ? 0x401u : 2u); return body;
        };
    }
    void Body(const std::int32_t reference, Bytes body) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [body = std::move(body)](std::int32_t, std::int32_t) { return body; };
    }
    void ActorBody(const std::int32_t reference, Bytes body) { Index(body, 0); Body(reference, std::move(body)); }
    void ObjectTag(Bytes& bytes, const std::string& name, const std::int32_t reference) {
        Bytes data; Index(data, reference); Index(bytes, Name(name)); bytes.push_back(0x55u);
        bytes.push_back(static_cast<std::uint8_t>(data.size())); Append(bytes, data);
    }
    void VectorTag(Bytes& bytes, const std::string& name, const Point value) {
        Index(bytes, Name(name)); bytes.push_back(0x5au); Index(bytes, Name("Vector")); bytes.push_back(12u);
        for (const auto component : value) Float(bytes, component);
    }
    void BoolTag(Bytes& bytes, const std::string& name, const bool value) {
        Index(bytes, Name(name)); bytes.push_back(static_cast<std::uint8_t>(3u | (value ? 0x80u : 0u)));
    }
    Bytes Serialize() {
        std::map<std::int32_t, std::int32_t> next;
        for (const auto& [owner, list] : children) {
            static_cast<void>(owner); for (std::size_t i = 1u; i < list.size(); ++i) next[list[i - 1u]] = list[i];
        }
        Bytes bytes; U32(bytes, 0x9e2a83c1u); U16(bytes, 68u); U16(bytes, 0u); bytes.resize(56u, 0u);
        std::vector<Bytes> bodies; std::vector<std::int32_t> offsets;
        for (std::size_t i = 0u; i < exports.size(); ++i) {
            const auto reference = static_cast<std::int32_t>(i + 1u); const auto owner = children.find(reference);
            auto body = builders[i] ? builders[i](next[reference], owner == children.end() || owner->second.empty() ? 0 : owner->second.front()) : Bytes{};
            offsets.push_back(static_cast<std::int32_t>(bytes.size())); Append(bytes, body); bodies.push_back(std::move(body));
        }
        Replace32(bytes, 12u, static_cast<std::uint32_t>(names.size())); Replace32(bytes, 16u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& name : names) {
            Index(bytes, static_cast<std::int32_t>(name.size() + 1u)); bytes.insert(bytes.end(), name.begin(), name.end());
            bytes.push_back(0u); U32(bytes, 0u);
        }
        Replace32(bytes, 20u, static_cast<std::uint32_t>(exports.size())); Replace32(bytes, 24u, static_cast<std::uint32_t>(bytes.size()));
        for (std::size_t i = 0u; i < exports.size(); ++i) {
            const auto& entry = exports[i]; Index(bytes, entry.ObjClass); Index(bytes, entry.ObjBase);
            U32(bytes, static_cast<std::uint32_t>(entry.ObjOuter)); Index(bytes, entry.ObjName);
            U32(bytes, static_cast<std::uint32_t>(entry.ObjFlags)); Index(bytes, static_cast<std::int32_t>(bodies[i].size()));
            if (!bodies[i].empty()) Index(bytes, offsets[i]);
        }
        Replace32(bytes, 28u, static_cast<std::uint32_t>(imports.size())); Replace32(bytes, 32u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& entry : imports) {
            Index(bytes, entry.ClassPackage); Index(bytes, entry.ClassName);
            U32(bytes, static_cast<std::uint32_t>(entry.ObjOuter)); Index(bytes, entry.ObjName);
        }
        return bytes;
    }
};

struct Fixture {
    std::filesystem::path parent, directory;
    std::map<std::filesystem::path, Bytes> sources;
    Fixture() {
        parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0u; attempt < 20u; ++attempt) {
            const auto candidate = parent / ("deusex-fast-trace-test-" + std::to_string(stamp) + '-' + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) { directory = std::filesystem::canonical(candidate); break; }
        }
        Require(!directory.empty() && directory.parent_path() == parent, "Generated FastTrace directory escaped its temporary parent");
        std::filesystem::create_directory(directory / "System"); std::filesystem::create_directory(directory / "Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime(); std::error_code error; const auto actual = std::filesystem::weakly_canonical(directory, error);
        if (!error && !directory.empty() && actual == directory && actual.parent_path() == parent &&
            actual.filename().string().rfind("deusex-fast-trace-test-", 0u) == 0u)
            std::filesystem::remove_all(actual, error);
    }
    PortablePackageTables Write(Package& package, const bool map = false) {
        const auto path = directory / (map ? "Maps" : "System") / (package.stem + (map ? ".dx" : ".u"));
        auto bytes = package.Serialize(); WriteBytes(path, bytes); sources.insert_or_assign(path, std::move(bytes));
        return LoadPortablePackageTables(path.string());
    }
    Bytes Snapshot(const std::string& label) const {
        const auto path = directory / (label + ".sav");
        Require(SavePortableRuntimeState(path.string()), "Could not save FastTrace test state"); return ReadBytes(path);
    }
    void Unchanged() const {
        for (const auto& [path, bytes] : sources) Require(ReadBytes(path) == bytes, "FastTrace changed a source package");
    }
};

struct Tables { std::vector<PortablePackageTables> scripts; PortablePackageTables map; };
enum class Variant : unsigned {
    Plane, EmptyOutside, EmptyInside, NonCsgInside, NonCsgOutside, OtherFlags,
    WrongFlags, WrongOptional, WrongReturn, WrongVectorConstraint, WrongModelClass,
    WrongLevelClass, MalformedVector, ExtraParameter, WrongVectorMetaclass, DuplicateActorSlot
};
std::string Path(const char* name) { return std::string("TraceFixture.") + name; }
Evaluation Vec(const Point point) { return {Value::Vector(point), {}}; }

Bytes ModelBody(const bool plane, const bool outside, const std::uint8_t flags) {
    Bytes body{0u}; // Tagged-property None; then exact UPrimitive native prefix.
    for (unsigned i = 0u; i < 6u; ++i) Float(body, 0.0f);
    body.push_back(0u);
    for (unsigned i = 0u; i < 4u; ++i) Float(body, 0.0f);
    Index(body, plane ? 3 : 0); // Vectors.
    if (plane) for (const auto value : {Point{1, 0, 0}, Point{0, 1, 0}, Point{0, 0, 1}})
        for (const auto component : value) Float(body, component);
    Index(body, plane ? 3 : 0); // Points, finite triangle on x=0.
    if (plane) for (const auto value : {Point{0, -100, -100}, Point{0, 100, -100}, Point{0, 0, 100}})
        for (const auto component : value) Float(body, component);
    Index(body, plane ? 1 : 0);
    if (plane) {
        Float(body, 1); Float(body, 0); Float(body, 0); Float(body, 0); U64(body, 0); body.push_back(flags);
        Index(body, 0); Index(body, 0); // VertexPool and Surface.
        for (unsigned i = 0u; i < 5u; ++i) Index(body, -1); // Back/front/plane/collision/render.
        Index(body, 0); Index(body, 0); body.push_back(3u); U32(body, 0xffffffffu); U32(body, 0xffffffffu);
    }
    Index(body, plane ? 1 : 0);
    if (plane) {
        Index(body, 0); U32(body, 0); Index(body, 0); Index(body, 0); Index(body, 1); Index(body, 2);
        Index(body, -1); Index(body, -1); U16(body, 0); U16(body, 0); Index(body, 0);
    }
    Index(body, plane ? 3 : 0);
    if (plane) for (std::int32_t i = 0; i < 3; ++i) { Index(body, i); Index(body, -1); }
    U32(body, 0); U32(body, 0); // SharedSides; actual serialized zero zone count.
    for (unsigned i = 0u; i < 7u; ++i) Index(body, 0); // Polys/lightmaps/bits/bounds/hulls/leaves/lights.
    U32(body, outside ? 1u : 0u); U32(body, 0); return body;
}

Tables Build(Fixture& fixture, const Variant variant = Variant::Plane) {
    Package core; core.stem = "Core"; const auto cp = core.ImportPackage("Core");
    const auto object = core.Export("Object"), metaclass = core.Export("Class", 0, object);
    const auto vector = core.Export("Vector", core.Import("Struct",
        variant == Variant::WrongVectorMetaclass ? core.ImportPackage("Fake") : cp), 0, object);
    for (const auto* name : {"X", "Y", "Z"})
        core.Property(name, variant == Variant::MalformedVector && std::string(name) == "Y" ? "IntProperty" : "FloatProperty", vector, cp);
    const auto rotator = core.Export("Rotator", core.Import("Struct", cp), 0, object);
    for (const auto* name : {"Pitch", "Yaw", "Roll"}) core.Property(name, "IntProperty", rotator, cp);
    core.ClassBody(object); core.ClassBody(metaclass); core.StructBody(vector); core.StructBody(rotator);
    const auto coreTable = fixture.Write(core);

    Package fake; fake.stem = "Fake"; const auto fcp = fake.ImportPackage("Core");
    const auto fo = fake.Import("Object", fcp), fakeObject = fake.Export("Object", 0, fo);
    const auto fm = fake.Export("Model", 0, fo), fl = fake.Export("Level", 0, fo);
    const auto fakeStruct = fake.Export("Struct", 0, fo);
    const auto fv = fake.Export("Vector", fake.Import("Struct", fcp), 0, fakeObject);
    for (const auto* name : {"X", "Y", "Z"}) fake.Property(name, "FloatProperty", fv, fcp);
    fake.ClassBody(fakeObject); fake.ClassBody(fm); fake.ClassBody(fl); fake.ClassBody(fakeStruct); fake.StructBody(fv);
    const auto fakeTable = fixture.Write(fake);

    Package engine; engine.stem = "Engine"; const auto ecp = engine.ImportPackage("Core");
    const auto eo = engine.Import("Object", ecp), ev = engine.Import("Vector", eo, "Struct"), er = engine.Import("Rotator", eo, "Struct");
    const auto actor = engine.Export("Actor", 0, eo), zone = engine.Export("ZoneInfo", 0, actor);
    const auto info = engine.Export("LevelInfo", 0, zone), level = engine.Export("Level", 0, eo), model = engine.Export("Model", 0, eo);
    const auto pawn = engine.Export("Pawn", 0, actor);
    const auto location = engine.Property("Location", "StructProperty", actor, ecp, ev);
    engine.Property("OldLocation", "StructProperty", actor, ecp, ev); engine.Property("Rotation", "StructProperty", actor, ecp, er);
    const auto actorLevel = engine.Property("Level", "ObjectProperty", actor, ecp, info);
    const auto actorXLevel = engine.Property("XLevel", "ObjectProperty", actor, ecp, level);
    for (const auto* name : {"Owner", "Base", "Brush", "Inventory"}) engine.Property(name, "ObjectProperty", actor, ecp, actor);
    engine.Property("Instigator", "ObjectProperty", actor, ecp, pawn);
    for (const auto* name : {"bCollideActors", "bBlockActors", "bBlockPlayers", "bCollideWorld", "bStatic", "bNoDelete", "bDeleteMe", "bAnimByOwner"})
        engine.Property(name, "BoolProperty", actor, ecp);
    for (const auto* name : {"CollisionRadius", "CollisionHeight"}) engine.Property(name, "FloatProperty", actor, ecp);
    for (const auto* name : {"Tag", "AttachTag"}) engine.Property(name, "NameProperty", actor, ecp);
    engine.Property("bBegunPlay", "BoolProperty", info, ecp); engine.Property("bStartup", "BoolProperty", info, ecp);
    const auto region = engine.Export("PointRegion", engine.Import("Struct", ecp), 0, actor);
    engine.Property("Zone", "ObjectProperty", region, ecp, zone); engine.Property("iLeaf", "IntProperty", region, ecp);
    engine.Property("ZoneNumber", "ByteProperty", region, ecp); engine.StructBody(region);
    const auto regionField = engine.Property("Region", "StructProperty", actor, ecp, region);
    const auto trace = engine.Function("FastTrace", actor, ecp, {}, 548u, variant == Variant::WrongFlags ? 0x501u : 0x401u);
    const auto end = engine.Property("TraceEnd", "StructProperty", trace, ecp, ev, 0x80u);
    const auto wrongVector = engine.Import("Vector", engine.Import("Object", engine.ImportPackage("Fake")), "Struct");
    const auto start = engine.Property("TraceStart", "StructProperty", trace, ecp,
        variant == Variant::WrongVectorConstraint ? wrongVector : ev, variant == Variant::WrongOptional ? 0x80u : 0x90u);
    engine.Property("ReturnValue", "BoolProperty", trace, ecp, 0, variant == Variant::WrongReturn ? 0x480u : 0x580u);
    if (variant == Variant::ExtraParameter) engine.Property("Unexpected", "FloatProperty", trace, ecp, 0, 0x90u);
    engine.FunctionCode(trace, Join({Ref(0x29u, end), Ref(0x29u, start)}), 548u, variant == Variant::WrongFlags ? 0x501u : 0x401u);
    for (const auto cls : {actor, zone, info, level, model, pawn}) engine.ClassBody(cls);
    const auto engineTable = fixture.Write(engine);
    static_cast<void>(location); static_cast<void>(actorLevel); static_cast<void>(actorXLevel); static_cast<void>(regionField);

    Package classes; classes.stem = "TraceClasses"; const auto ccp = classes.ImportPackage("Core"), cep = classes.ImportPackage("Engine");
    const auto ca = classes.Import("Actor", cep), co = classes.Import("Object", ccp), cv = classes.Import("Vector", co, "Struct");
    const auto probe = classes.Export("Probe", 0, ca);
    const auto loc = classes.Import("Location", ca, "StructProperty"), levelField = classes.Import("Level", ca, "ObjectProperty");
    const auto xlevelField = classes.Import("XLevel", ca, "ObjectProperty"), reg = classes.Import("Region", ca, "StructProperty");
    const auto fast = classes.Import("FastTrace", ca, "Function");
    const auto point = classes.Property("End", "StructProperty", probe, ccp, cv);
    const auto counter = classes.Property("Counter", "IntProperty", probe, ccp);
    const auto link = classes.Property("Link", "ObjectProperty", probe, ccp, ca);
    const auto seen = classes.Property("SeenRegion", "StructProperty", probe, ccp,
        classes.Import("PointRegion", ca, "Struct"));
    const auto boolFunction = [&](const std::string& name, Code body) {
        const auto fn = classes.Function(name, probe, ccp, std::move(body));
        classes.Property("ReturnValue", "BoolProperty", fn, ccp, 0, 0x580u); return fn;
    };
    const auto vectorFunction = [&](const std::string& name, Code body) {
        const auto fn = classes.Function(name, probe, ccp, std::move(body));
        classes.Property("ReturnValue", "StructProperty", fn, ccp, cv, 0x580u); return fn;
    };
    const auto mutateEnd = vectorFunction("MutateEnd", Join({Assign(Ref(0x01u, point), Vector({-6, 0, 0})), Return(Vector({5, 0, 0}))}));
    const auto mutateLocationEnd = vectorFunction("MutateLocationEnd",
        Join({Assign(Ref(0x01u, loc), Vector({5, 0, 0})), Return(Vector({6, 0, 0}))}));
    const auto mutateNone = classes.Function("MutateLocationNone", probe, ccp,
        Join({Assign(Ref(0x01u, loc), Vector({-5, 0, 0})), Return(Token(0x2au))}));
    classes.Property("ReturnValue", "ObjectProperty", mutateNone, ccp, ca, 0x580u);
    boolFunction("TraceFinal", Return(Final(fast, {Vector({6, 0, 0}), Vector({5, 0, 0})})));
    boolFunction("TraceIndex", Return(Native(548u, {Vector({6, 0, 0}), Vector({5, 0, 0})})));
    boolFunction("TraceVirtual", Return(Join({Ref(0x1bu, classes.Name("FastTrace")),
        Vector({6, 0, 0}), Vector({5, 0, 0}), Token(0x16u)})));
    boolFunction("TraceOmitted", Return(Final(fast, {Vector({6, 0, 0})})));
    boolFunction("TraceNothing", Return(Final(fast, {Vector({6, 0, 0}), Token(0x0bu)})));
    boolFunction("TraceOrderedDefault", Return(Final(fast, {Final(mutateLocationEnd), Token(0x0bu)})));
    boolFunction("TraceNoWriteStart", Return(Final(fast,
        {Vector({6, 0, 0}), Context(Final(mutateNone), Ref(0x01u, loc))})));
    boolFunction("TraceCopiedEnd", Return(Final(fast, {Ref(0x01u, point), Final(mutateEnd)})));
    boolFunction("TraceOtherReceiver", Return(Context(Ref(0x01u, link),
        Native(548u, {Final(mutateLocationEnd), Token(0x0bu)}))));
    boolFunction("TraceZeroEnd", Return(Final(fast,
        {Context(Token(0x2au), Ref(0x01u, loc), 12u), Vector({5, 0, 0})})));
    boolFunction("TraceZeroStart", Return(Final(fast,
        {Vector({6, 0, 0}), Context(Token(0x2au), Ref(0x01u, loc), 12u)})));
    boolFunction("TraceUnwrittenEnd", Return(Final(fast,
        {Context(Token(0x2au), Ref(0x01u, loc)), Vector({5, 0, 0})})));
    boolFunction("TracePartialEnd", Return(Final(fast,
        {Context(Token(0x2au), Ref(0x01u, loc), 4u), Vector({5, 0, 0})})));
    boolFunction("TracePartialStart", Return(Final(fast,
        {Vector({6, 0, 0}), Context(Token(0x2au), Ref(0x01u, loc), 4u)})));
    boolFunction("TraceWarmRegion", Join({Assign(Ref(0x01u, seen), Ref(0x01u, reg)),
        Return(Final(fast, {Vector({6, 0, 0}), Vector({5, 0, 0})}))}));
    boolFunction("TraceWarmThenClearReflectedLevel", Join({Final(fast, {Vector({6, 0, 0}), Vector({5, 0, 0})}),
        Assign(Ref(0x01u, levelField), Token(0x2au)), Assign(Ref(0x01u, xlevelField), Token(0x2au)),
        Return(Final(fast, {Vector({6, 0, 0}), Vector({5, 0, 0})}))}));
    classes.Function("WriteTraceThenFail", probe, ccp, Join({Assign(Ref(0x01u, counter), Integer(42)),
        Final(fast, {Vector({6, 0, 0}), Vector({5, 0, 0})}), Native(999u), Return()}));
    const auto setter = classes.Function("SetLocationValue", probe, ccp);
    const auto input = classes.Property("Input", "StructProperty", setter, ccp, cv, 0x80u);
    classes.FunctionCode(setter, Join({Assign(Ref(0x01u, loc), Ref(0x00u, input)), Return()}));
    classes.ClassBody(probe); const auto classesTable = fixture.Write(classes);

    Package map; map.stem = "TraceFixture"; const auto mep = map.ImportPackage("Engine"), mcp = map.ImportPackage("TraceClasses");
    const auto mfp = map.ImportPackage("Fake");
    // Misleading brush precedes actual world Model and has opposite solidity.
    const auto misleading = map.Export("FirstBrush", map.Import("Model", mep));
    const auto world = map.Export("MyLevel", map.Import("Level", variant == Variant::WrongLevelClass ? mfp : mep));
    const auto actualModel = map.Export("ActualWorld", map.Import("Model", variant == Variant::WrongModelClass ? mfp : mep));
    const auto misleadingInfo = map.Export("WrongLevelInfo", map.Import("LevelInfo", mep));
    const auto levelActor = map.Export("Level0", map.Import("LevelInfo", mep));
    const auto driver = map.Export("Driver", map.Import("Probe", mcp)), other = map.Export("Other", map.Import("Probe", mcp));
    const auto orphan = map.Export("Orphan", map.Import("Probe", mcp));
    for (const auto reference : {misleadingInfo, levelActor, driver, other, orphan}) {
        Bytes tags; map.ObjectTag(tags, "Level", levelActor); map.ObjectTag(tags, "XLevel", world);
        if (reference == levelActor) map.BoolTag(tags, "bBegunPlay", true);
        if (reference == driver || reference == other || reference == orphan) {
            map.VectorTag(tags, "Location", reference == other ? Point{-5, 0, 0} : Point{5, 0, 0});
            map.VectorTag(tags, "End", {6, 0, 0}); map.ObjectTag(tags, "Link", other);
        }
        map.ActorBody(reference, std::move(tags));
    }
    Bytes worldBody{0u}; U32(worldBody, 4u); U32(worldBody, 4u);
    for (const auto reference : {levelActor, driver, 0, variant==Variant::DuplicateActorSlot?driver:other}) Index(worldBody, reference);
    for (unsigned i = 0u; i < 5u; ++i) Index(worldBody, 0);
    U32(worldBody, 7777u); U32(worldBody, 0u); Index(worldBody, actualModel); Index(worldBody, 0); map.Body(world, std::move(worldBody));
    const bool plane = variant != Variant::EmptyOutside && variant != Variant::EmptyInside;
    const bool outside = variant != Variant::EmptyInside && variant != Variant::NonCsgInside;
    const std::uint8_t flags = variant == Variant::NonCsgInside || variant == Variant::NonCsgOutside ? 1u :
        variant == Variant::OtherFlags ? 0xfeu : 0u;
    map.Body(misleading, ModelBody(false, !outside, 0u)); map.Body(actualModel, ModelBody(plane, outside, flags));
    const auto mapTable = fixture.Write(map, true);
    Require(FindPortableRootModel68Export(mapTable) == static_cast<std::size_t>(actualModel - 1),
        "Generated Level.Model decoder selected misleading first brush");
    return {{coreTable, engineTable, fakeTable, classesTable}, mapTable};
}

QuestVr::Vm::Result Call(const char* actor, const char* function, const std::vector<Evaluation>& args = {},
    const QuestVr::Vm::Limits& limits = {}) {
    const auto result = ExecutePortableActorFunction(Path(actor), function, args, limits);
    Require(result.passed() && result.committed, std::string(actor) + '.' + function + " failed: " + result.error +
        " at " + result.function + ':' + std::to_string(result.offset)); return result;
}
void Boolean(const QuestVr::Vm::Result& result, const bool expected, const std::string& label) {
    Require(result.value.kind == Kind::Bool && result.value.boolean == expected, label + " returned wrong Bool");
}
void Refused(Fixture& fixture, const char* function, const std::vector<Evaluation>& args = {},
    const char* actor = "Driver", const QuestVr::Vm::Limits& limits = {}) {
    const auto before = fixture.Snapshot("BeforeRefusal"); const auto revision = GetPortableRuntimeWorldRevision();
    const auto gc = GC::GetStats(); const auto actors = GetPortableRuntimeMapActors(true).size();
    const auto result = ExecutePortableActorFunction(Path(actor), function, args, limits);
    const auto afterGc = GC::GetStats();
    Require(!result.passed() && !result.committed, std::string("Invalid FastTrace call committed: ") + function);
    Require(afterGc.numObjects == gc.numObjects && afterGc.memoryUsage == gc.memoryUsage,
        "Refused FastTrace call leaked rooted/provisional UObjects");
    Require(fixture.Snapshot("AfterRefusal") == before && GetPortableRuntimeWorldRevision() == revision &&
        GetPortableRuntimeMapActors(true).size() == actors, "Refused FastTrace retained preceding writes/save/revision changes");
    ++refusals; fixture.Unchanged();
}
void Initialize(const Tables& tables) {
    Require(InitializePortableRuntime(tables.scripts).passed && LoadPortableRuntimeMap(tables.map).passed,
        "Generated FastTrace runtime/map failed initialization");
}

void Generated() {
    for (const auto variant : {Variant::Plane, Variant::EmptyOutside, Variant::EmptyInside,
        Variant::NonCsgInside, Variant::NonCsgOutside, Variant::OtherFlags}) {
        Fixture fixture; const auto tables = Build(fixture, variant); Initialize(tables);
        const auto baseline = fixture.Snapshot("Baseline"); const auto revision = GetPortableRuntimeWorldRevision();
        const auto gc = GC::GetStats();
        for (const auto end : {Point{2, 0, 0}, Point{-2, 0, 0}, Point{7, 2, -9}, Point{-7, -2, 9}})
            for (const auto start : {Point{3, 0, 0}, Point{-3, 0, 0}, Point{9, -4, 2}, Point{-9, 4, -2}}) {
                // The authored single x=0 CSG plane has front outside and back
                // solid. A segment is clear iff both strictly positive. Empty
                // and NF_NotCsg models retain their incoming RootOutside.
                const bool expected = variant == Variant::EmptyOutside || variant == Variant::NonCsgOutside ? true :
                    variant == Variant::EmptyInside || variant == Variant::NonCsgInside ? false : end[0] > 0 && start[0] > 0;
                Boolean(Call("Driver", "FastTrace", {Vec(end), Vec(start)}), expected, "Independent generated half-space oracle");
            }
        for (const auto* function : {"TraceFinal", "TraceIndex", "TraceVirtual", "TraceOmitted", "TraceNothing", "TraceZeroEnd", "TraceZeroStart"}) {
            const bool expected = variant != Variant::EmptyInside && variant != Variant::NonCsgInside;
            Boolean(Call("Driver", function), expected, function);
        }
        const auto afterGc = GC::GetStats();
        Require(fixture.Snapshot("AfterReadonly") == baseline && GetPortableRuntimeWorldRevision() == revision &&
            afterGc.numObjects == gc.numObjects && afterGc.memoryUsage == gc.memoryUsage,
            "Read-only FastTrace changed runtime saves/revision/UObject lifetime");
        Refused(fixture, "FastTrace", {Vec({6, 0, 0}), Vec({5, 0, 0})}, "Orphan");
        Refused(fixture, "WriteTraceThenFail");
        if (variant == Variant::Plane) {
            Call("Driver", "SetLocationValue", {Vec({-5, 0, 0})});
            Boolean(Call("Driver", "TraceOrderedDefault"), true, "TraceStart must observe earlier TraceEnd expression");
            Require(ReadPortableActorScriptProperty(Path("Driver"), "Location").vector == Point{5, 0, 0},
                "TraceEnd side effect was not executed");
            Boolean(Call("Driver", "TraceNoWriteStart"), true, "TraceStart NoWrite must retain its step-time receiver Location");
            Require(ReadPortableActorScriptProperty(Path("Driver"), "Location").vector == Point{-5, 0, 0},
                "NoWrite TraceStart Context target side effect was skipped");
            Boolean(Call("Driver", "TraceCopiedEnd"), true, "TraceEnd ordinary input alias was not captured before next expression");
            Require(ReadPortableActorScriptProperty(Path("Driver"), "End").vector == Point{-6, 0, 0},
                "TraceStart expression did not mutate the earlier aliased field");
            Boolean(Call("Driver", "TraceOtherReceiver"), false,
                "FastTrace mixed opcode receiver default with caller-owned expression evaluation");
            Boolean(Call("Driver", "TraceWarmThenClearReflectedLevel"), true,
                "Mutable reflected Level/XLevel redirected warm native BSP ownership");
            Require(ReadPortableActorScriptProperty(Path("Driver"), "Level").text.empty() &&
                ReadPortableActorScriptProperty(Path("Driver"), "XLevel").text.empty(),
                "Native ownership test did not actually clear reflected links");
            Refused(fixture, "TraceUnwrittenEnd"); Refused(fixture, "TracePartialEnd"); Refused(fixture, "TracePartialStart");
            for (const auto invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                -std::numeric_limits<float>::infinity()}) {
                Refused(fixture, "FastTrace", {Vec({invalid, 0, 0}), Vec({5, 0, 0})});
                Refused(fixture, "FastTrace", {Vec({6, 0, 0}), Vec({invalid, 0, 0})});
            }
            Refused(fixture, "FastTrace", {}); Refused(fixture, "FastTrace", {{Value::Integer(1), {}}});
            Refused(fixture, "FastTrace", {Vec({6, 0, 0}), Vec({5, 0, 0}), Vec({4, 0, 0})});
            QuestVr::Vm::Limits budget; budget.instructions = 1u;
            Refused(fixture, "WriteTraceThenFail", {}, "Driver", budget);
        }
        fixture.Unchanged();
    }
    for (const auto variant : {Variant::WrongFlags, Variant::WrongOptional, Variant::WrongReturn,
        Variant::WrongVectorConstraint, Variant::ExtraParameter}) {
        Fixture fixture; const auto tables = Build(fixture, variant); Initialize(tables);
        Refused(fixture, "FastTrace", {Vec({6, 0, 0}), Vec({5, 0, 0})});
        Refused(fixture, "TraceIndex"); Refused(fixture, "WriteTraceThenFail");
    }
    for (const auto variant : {Variant::WrongLevelClass, Variant::WrongModelClass, Variant::MalformedVector, Variant::WrongVectorMetaclass}) {
        Fixture fixture; const auto tables = Build(fixture, variant);
        try {
            const auto initialized = InitializePortableRuntime(tables.scripts);
            if (!initialized.passed) { ++refusals; fixture.Unchanged(); continue; }
            const auto loaded = LoadPortableRuntimeMap(tables.map);
            if (!loaded.passed) { ++refusals; fixture.Unchanged(); continue; }
            Refused(fixture, "FastTrace", {Vec({6, 0, 0}), Vec({5, 0, 0})});
        } catch (const std::runtime_error& error) {
            const std::string message=error.what();
            const bool expected=variant==Variant::WrongLevelClass ? message=="Map has no linked serialized Level" :
                variant==Variant::WrongModelClass ? message=="Native ULevel.Model loader binding is not an actual Engine.Level/Engine.Model pair" :
                variant==Variant::MalformedVector ? message.find("specialized struct member kind does not match")!=std::string::npos :
                message.find("Core.Struct")!=std::string::npos;
            Require(expected,"Malformed metadata failed outside its audited schema refusal: "+message);
            ++refusals;fixture.Unchanged();
        }
    }
}

void NativeModelLifetime() {
    Fixture fixture;auto tables=Build(fixture);Initialize(tables);
    const auto original=ReadPortableRuntimeNativeModel();
    Require(original.available&&!original.geometryCached&&original.mapGeneration!=0u&&
        original.levelPath==Path("MyLevel")&&original.modelPath==Path("ActualWorld")&&original.modelExportIndex==2u,
        "Native Model loader edge/generation was not established before first query");
    const auto baseline=fixture.Snapshot("NativeModelBaseline");
    const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    for(unsigned iteration=0u;iteration<32u;++iteration) {
        Boolean(Call("Driver","FastTrace",{Vec({-6,0,0}),Vec({5,0,0})}),false,"Repeated immutable-Model query");
        const auto cached=ReadPortableRuntimeNativeModel();
        Require(cached.available&&cached.geometryCached&&cached.mapGeneration==original.mapGeneration&&
            cached.levelPath==original.levelPath&&cached.modelPath==original.modelPath&&cached.cachedNodes==1u,
            "Repeated public VM calls did not retain native-owned immutable geometry");
    }
    Require(fixture.Snapshot("NativeModelReadonly")==baseline&&GetPortableRuntimeWorldRevision()==revision&&
        GC::GetStats().numObjects==gc.numObjects&&GC::GetStats().memoryUsage==gc.memoryUsage,
        "Immutable Model cache changed gameplay/checkpoint/revision/GC lifetime");
    Refused(fixture,"WriteTraceThenFail");
    Require(ReadPortableRuntimeNativeModel().geometryCached&&
        ReadPortableRuntimeNativeModel().mapGeneration==original.mapGeneration,
        "Script rollback discarded or redirected its independent immutable source cache");
    Boolean(Call("Driver","TraceWarmThenClearReflectedLevel"),true,"Native Model ownership after reflected links cleared");
    Require(ReadPortableActorScriptProperty(Path("Driver"),"Level").text.empty()&&
        ReadPortableActorScriptProperty(Path("Driver"),"XLevel").text.empty(),"Reflected ownership control did not execute");
    Boolean(Call("Driver","FastTrace",{Vec({-6,0,0}),Vec({5,0,0})}),false,"Reflected links redirected native Model across VM hosts");
    Require(LoadPortableRuntimeState((fixture.directory/"NativeModelBaseline.sav").string()),"Warm native Model baseline replacement failed");
    const auto warm=ReadPortableRuntimeNativeModel();
    Require(warm.mapGeneration==original.mapGeneration&&warm.geometryCached&&warm.cachedNodes==1u&&
        fixture.Snapshot("NativeModelWarmRestored")==baseline,"Warm save replaced/serialized native Model cache ownership");
    Boolean(Call("Driver","FastTrace",{Vec({-6,0,0}),Vec({5,0,0})}),false,"Warm restore changed native Model result");
    ShutdownPortableRuntime();
    Require(!ReadPortableRuntimeNativeModel().available,"Shutdown retained native Model diagnostic/cache authority");
    Initialize(tables);const auto cold=ReadPortableRuntimeNativeModel();
    Require(cold.available&&!cold.geometryCached&&cold.mapGeneration>original.mapGeneration,"Cold load reused an old native generation/cache");
    Require(LoadPortableRuntimeState((fixture.directory/"NativeModelBaseline.sav").string()),"Cold native Model baseline restore failed");
    Require(!ReadPortableRuntimeNativeModel().geometryCached,"Checkpoint recreated a serialized geometry-cache pointer");
    Boolean(Call("Driver","FastTrace",{Vec({-6,0,0}),Vec({5,0,0})}),false,"Cold restore failed actual native Model rebind");
    const auto priorReload=ReadPortableRuntimeNativeModel();
    // Deliberately replace generated contents at the SAME filename, using a
    // new decoded table. No original/commercial asset is modified. The new
    // empty outside Model must not inherit the old cached blocking plane.
    tables=Build(fixture,Variant::EmptyOutside);
    Require(LoadPortableRuntimeMap(tables.map).passed,"Same-filename Model map replacement failed");
    const auto empty=ReadPortableRuntimeNativeModel();
    Require(empty.available&&!empty.geometryCached&&empty.mapGeneration>priorReload.mapGeneration&&
        empty.modelPath==priorReload.modelPath&&empty.modelExportIndex==priorReload.modelExportIndex,
        "Same-filename/export replacement reused previous native Model geometry/generation");
    Boolean(Call("Driver","FastTrace",{Vec({-6,0,0}),Vec({5,0,0})}),true,"Same-filename reload reused stale blocking plane");
    Require(ReadPortableRuntimeNativeModel().geometryCached&&ReadPortableRuntimeNativeModel().cachedNodes==0u,
        "Replacement empty Model was not independently retained");
    Require(UnloadPortableRuntimeMap()!=0u&&!ReadPortableRuntimeNativeModel().available,"Map unload retained native Model cache authority");
    tables=Build(fixture,Variant::Plane);Require(LoadPortableRuntimeMap(tables.map).passed,"Post-unload Model replacement failed");
    Require(ReadPortableRuntimeNativeModel().mapGeneration>empty.mapGeneration&&!ReadPortableRuntimeNativeModel().geometryCached,
        "Post-unload reload revived an old generation/cache");
    Boolean(Call("Driver","FastTrace",{Vec({-6,0,0}),Vec({5,0,0})}),false,"Post-unload original plane was not rebound");
    fixture.Unchanged();
}

void FailedModelPublication() {
    Fixture fixture;auto tables=Build(fixture,Variant::DuplicateActorSlot);
    Require(InitializePortableRuntime(tables.scripts).passed,"Late-failure fixture script initialization failed");
    bool refused{};
    try {static_cast<void>(LoadPortableRuntimeMap(tables.map));}
    catch(const std::runtime_error& error) {refused=std::string(error.what())=="Level contains a duplicate actor slot";}
    Require(refused,"Late malformed Level failed outside exact duplicate-slot boundary");++refusals;
    const auto failed=ReadPortableRuntimeNativeModel();
    Require(!failed.available&&!failed.geometryCached&&!failed.mapGeneration,
        "Late failed map load published native Model query authority");
    const auto revision=GetPortableRuntimeWorldRevision();
    const auto trace=ExecutePortableActorFunction(Path("Driver"),"FastTrace",{Vec({-6,0,0}),Vec({5,0,0})});
    Require(!trace.passed()&&!trace.committed&&trace.error.find("current loader-owned Model")!=std::string::npos,
        "FastTrace acquired/cached Model from a late failed map load");++refusals;
    Require(GetPortableRuntimeWorldRevision()==revision&&!ReadPortableRuntimeNativeModel().available&&
        !ReadPortableRuntimeNativeModel().geometryCached,"Failed-map query published cache or gameplay revision");
    GC::Collect();
    Require(!ReadPortableRuntimeNativeModel().available,"GC revived failed native Model authority");
    Require(UnloadPortableRuntimeMap()!=0u&&!ReadPortableRuntimeNativeModel().available,
        "Explicit failed-map unload retained native Model authority");
    // No intervening Shutdown: the same runtime must recover from the failed
    // map, replacing generated contents at the same source filename/export.
    tables=Build(fixture);Require(LoadPortableRuntimeMap(tables.map).passed,"Valid same-filename reload after failure did not recover");
    const auto restored=ReadPortableRuntimeNativeModel();
    Require(restored.available&&!restored.geometryCached&&restored.mapGeneration!=0u,
        "Valid replacement did not independently publish native Model authority");
    Boolean(Call("Driver","FastTrace",{Vec({-6,0,0}),Vec({5,0,0})}),false,"Recovered map did not query its actual Model");
    Require(ReadPortableRuntimeNativeModel().geometryCached,"Recovered map failed to retain immutable geometry");
    fixture.Unchanged();
}

bool Original(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> sources;
    for (const auto* name : {"Core", "Engine", "DeusEx", "Extension", "ConSys", "IpDrv", "IpServer",
        "UBrowser", "UWindow", "Editor", "Fire", "MPCharacters", "DeusExSounds"})
        sources.push_back(root / "System" / (std::string(name) + ".u"));
    sources.push_back(root / "Maps" / "00_Training.dx");
    for (const auto& source : sources) if (!std::filesystem::is_regular_file(source)) {
        std::cout << "SKIP original FastTrace548: missing owned package " << source.string() << '\n'; return false;
    }
    struct Source { Bytes bytes; std::filesystem::file_time_type time; };
    std::map<std::filesystem::path, Source> before;
    std::vector<PortablePackageTables> scripts;
    for (std::size_t i = 0u; i < sources.size(); ++i) {
        before.emplace(sources[i], Source{ReadBytes(sources[i]), std::filesystem::last_write_time(sources[i])});
        if (i + 1u < sources.size()) scripts.push_back(LoadPortablePackageTables(sources[i].string()));
    }
    const auto& engine = scripts.at(1u);
    const auto declaration = LoadPortableFunctionScript(engine, FindPortableExport(engine, "Actor.FastTrace"));
    Require(declaration.nativeIndex == 548u && declaration.functionFlags == 0x401u,
        "Owned FastTrace native index/flags differ from retail declaration");
    const std::array<const char*, 3u> names{"TraceEnd", "TraceStart", "ReturnValue"};
    const std::array<const char*, 3u> types{"StructProperty", "StructProperty", "BoolProperty"};
    const std::array<std::uint32_t, 3u> flags{0x80u, 0x90u, 0x580u};
    auto child = declaration.children;
    for (std::size_t slot = 0u; slot < names.size(); ++slot) {
        Require(child > 0, "Owned FastTrace declaration child chain ended early");
        const auto property = LoadPortablePropertyDescriptor(engine, static_cast<std::size_t>(child - 1));
        Require(property.objectPath == std::string("Actor.FastTrace.") + names[slot] && property.type == types[slot] &&
            property.flags == flags[slot] && property.arrayDimension == 1u,
            "Owned FastTrace parameter schema differs from original");
        if (slot < 2u) Require(GetPortableObjectPath(engine, property.referencedType) == "Core.Object.Vector",
            "Owned FastTrace argument is not canonical Core.Object.Vector");
        child = property.nextField;
    }
    Require(child == 0, "Owned FastTrace declaration has extra fields");
    const auto map = LoadPortablePackageTables(sources.back().string()); const auto model = LoadPortableRootModel68(map);
    Require(InitializePortableRuntime(scripts).passed && LoadPortableRuntimeMap(map).passed,
        "Owned original FastTrace runtime/map failed initialization");
    const auto nativeModel=ReadPortableRuntimeNativeModel();
    Require(nativeModel.available&&!nativeModel.geometryCached&&nativeModel.modelExportIndex==model.exportIndex&&
        nativeModel.modelPath=="00_Training."+model.objectPath,
        "Owned original loader did not bind its actual serialized native Model");
    Fixture saves; const auto baseline = saves.Snapshot("OwnedBaseline");
    const auto revision = GetPortableRuntimeWorldRevision();
    const auto count = GetPortableRuntimeMapActors(true).size();
    const auto gc = GC::GetStats();
    const std::string actor = "00_Training.CrateBreakableMedGeneral1";
    const auto location = ReadPortableActorScriptProperty(actor, "Location");
    Require(location.kind == Kind::Vector, "Original crate lacks typed Location");
    std::size_t clearCount{}, blockedCount{};
    for (const auto endOffset : {Point{0, 0, 0}, Point{1, 0, 0}, Point{-1, 0, 0}, Point{0, 1, 0},
        Point{0, -1, 0}, Point{0, 0, 1}, Point{0, 0, -1}, Point{127, 71, 43}, Point{-509, 251, 97}, Point{1031, -257, -139},
        Point{4096, 0, 0}, Point{-4096, 0, 0}, Point{0, 4096, 0}, Point{0, -4096, 0}, Point{0, 0, 4096}, Point{0, 0, -4096}})
        for (const auto startOffset : {Point{0, 0, 0}, Point{17, -23, 31}, Point{-241, 367, -101}}) {
            Point end{}, start{};
            for (std::size_t axis = 0u; axis < 3u; ++axis) {
                end[axis] = location.vector[axis] + endOffset[axis]; start[axis] = location.vector[axis] + startOffset[axis];
            }
            const auto expected = QuestVr::CheckOriginalModelFastLine(model,
                {end[0], end[1], end[2]}, {start[0], start[1], start[2]}).clear;
            const auto result = ExecutePortableActorFunction(actor, "FastTrace", {Vec(end), Vec(start)});
            Require(result.passed() && result.committed, "Owned original FastTrace548 failed: " + result.error);
            Boolean(result, expected, "Owned ULevel.Model adapter comparison");
            if (expected) ++clearCount; else ++blockedCount;
            if (startOffset == Point{0, 0, 0}) {
                const auto omitted = ExecutePortableActorFunction(actor, "FastTrace", {Vec(end)});
                Require(omitted.passed() && omitted.committed, "Owned omitted TraceStart failed: " + omitted.error);
                Boolean(omitted, expected, "Owned receiver Location default");
            }
        }
    Require(clearCount != 0u && blockedCount != 0u, "Owned integration ray set did not cover both outcomes");
    const auto cachedModel=ReadPortableRuntimeNativeModel();
    Require(cachedModel.available&&cachedModel.geometryCached&&cachedModel.mapGeneration==nativeModel.mapGeneration&&
        cachedModel.modelPath==nativeModel.modelPath&&cachedModel.cachedNodes==model.nodes.size(),
        "Owned original queries lost their immutable native Model identity/cache");
    const auto afterGc = GC::GetStats();
    Require(saves.Snapshot("OwnedAfter") == baseline && GetPortableRuntimeWorldRevision() == revision &&
        GetPortableRuntimeMapActors(true).size() == count && afterGc.numObjects == gc.numObjects &&
        afterGc.memoryUsage == gc.memoryUsage, "Owned FastTrace changed save/revision/actor/GC state");
    ShutdownPortableRuntime();
    for (const auto& [path, source] : before)
        Require(ReadBytes(path) == source.bytes && std::filesystem::last_write_time(path) == source.time,
            "Owned original inspection changed source bytes/timestamps");
    std::cout << "PASS owned original548 declaration, actual Training ULevel.Model and receiver-default integration: "
        << clearCount << " clear / " << blockedCount << " blocked supplied rays; no DLL/EXE execution, physics or live crate claim\n";
    return true;
}
} // namespace

int main(const int argc, char** argv) {
    try {
        if (argc > 2) throw std::runtime_error("Usage: portable_fast_trace_test [owned-original-game-root]");
        const auto baseline = GC::GetStats().numObjects;
        if (argc == 2) { if (!Original(argv[1])) return 77; }
        else {Generated();NativeModelLifetime();FailedModelPublication();}
        ShutdownPortableRuntime(); GC::Collect();
        Require(GC::GetStats().numObjects == baseline, "FastTrace tests leaked rooted UObjects");
        std::cout << "PASS FastTrace548 integration: " << checks << " checks, " << refusals
            << " refusal controls; genuine native Level/Model, copied inputs, sequential defaults,"
               " canonical Vector destination, namespace guards and transactional rollback. No full crate/gameplay claim.\n";
        return 0;
    } catch (const std::exception& error) {
        ShutdownPortableRuntime(); std::cerr << "FastTrace integration failed: " << error.what() << '\n'; return 1;
    }
}
