#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_model_geometry.h"
#include "quest_script_state.h"
#include "quest_decal_primary_quad.h"
#include "quest_script_random.h"
#include "quest_load_context_save.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// Genuine generated UE1 packages, serialized ULevel and UModel, compiled
// callbacks, and the production Native278 path. No original game/user files.
namespace {
using Bytes = std::vector<std::uint8_t>;
using Value = QuestVr::Vm::Value;
using Kind = QuestVr::Vm::Kind;
using Evaluation = QuestVr::Vm::Evaluation;
std::size_t checks{}, refusals{};
void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void U16(Bytes& bytes, std::uint16_t value) { bytes.push_back(static_cast<std::uint8_t>(value)); bytes.push_back(static_cast<std::uint8_t>(value >> 8u)); }
void U32(Bytes& bytes, std::uint32_t value) { U16(bytes, static_cast<std::uint16_t>(value)); U16(bytes, static_cast<std::uint16_t>(value >> 16u)); }
void U64(Bytes& bytes, std::uint64_t value) { U32(bytes, static_cast<std::uint32_t>(value)); U32(bytes, static_cast<std::uint32_t>(value >> 32u)); }
void FloatBytes(Bytes& bytes, float value) { std::uint32_t bits{}; std::memcpy(&bits, &value, 4u); U32(bytes, bits); }
void Index(Bytes& bytes, std::int32_t value) {
    auto remaining = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((remaining & 63u) | (value < 0 ? 128u : 0u)); remaining >>= 6u;
    if (remaining) first |= 64u;
    bytes.push_back(first);
    while (remaining) { auto next = static_cast<std::uint8_t>(remaining & 127u); remaining >>= 7u; if (remaining) next |= 128u; bytes.push_back(next); }
}
void Replace32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8u * i));
}
void Append(Bytes& bytes, const Bytes& other) { bytes.insert(bytes.end(), other.begin(), other.end()); }
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path); Require(size <= 8u * 1024u * 1024u, "Generated spawn file exceeds independent read bound");
    Bytes bytes(static_cast<std::size_t>(size)); std::ifstream file(path, std::ios::binary); Require(static_cast<bool>(file), "Cannot inspect generated spawn file");
    if (!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "Generated spawn file was truncated"); return bytes;
}
void WriteBytes(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc); Require(static_cast<bool>(file), "Cannot create generated spawn file");
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); Require(static_cast<bool>(file), "Cannot write generated spawn file");
}
struct Code { Bytes raw; std::size_t logical{}; };
Code Token(std::uint8_t op) { return {{op}, 1u}; }
Code Ref(std::uint8_t op, std::int32_t reference) { Code code{{op}, 5u}; Index(code.raw, reference); return code; }
Code Int(std::int32_t value) { Code code{{0x1du}, 5u}; U32(code.raw, static_cast<std::uint32_t>(value)); return code; }
Code Number(float value) { Code code{{0x1eu}, 5u}; FloatBytes(code.raw, value); return code; }
Code VectorLiteral(std::array<float, 3> value) {
    Code code{{0x23u}, 13u}; for (const auto component : value) FloatBytes(code.raw, component); return code;
}
Code RotatorLiteral(std::array<std::int32_t, 3> value) {
    Code code{{0x22u}, 13u}; for (const auto component : value) U32(code.raw, static_cast<std::uint32_t>(component)); return code;
}
Code Text(const std::string& value) {
    Code code{{0x1fu}, value.size() + 2u};
    code.raw.insert(code.raw.end(), value.begin(), value.end()); code.raw.push_back(0u); return code;
}
Code Join(std::initializer_list<Code> parts) {
    Code code; for (const auto& part : parts) { Append(code.raw, part.raw); code.logical += part.logical; } return code;
}
Code Assign(Code left, Code right) { return Join({Token(0x0fu), std::move(left), std::move(right)}); }
Code Return(Code result = Token(0x0bu)) { return Join({Token(0x04u), std::move(result)}); }
Code Native(std::uint16_t index, std::initializer_list<Code> arguments = {}) {
    Code code;
    if (index >= 256u) { code.raw = {static_cast<std::uint8_t>(0x60u + (index >> 8u)), static_cast<std::uint8_t>(index)}; code.logical = 2u; }
    else { code.raw = {static_cast<std::uint8_t>(index)}; code.logical = 1u; }
    for (const auto& argument : arguments) { Append(code.raw, argument.raw); code.logical += argument.logical; }
    code.raw.push_back(0x16u); ++code.logical; return code;
}
Code Context(Code target, Code expression) {
    Require(expression.logical <= 65535u, "Generated spawn Context logical skip overflow");
    Code code = Join({Token(0x19u), std::move(target)}); U16(code.raw, static_cast<std::uint16_t>(expression.logical)); code.raw.push_back(0u); code.logical += 3u;
    Append(code.raw, expression.raw); code.logical += expression.logical; return code;
}
Code Array(std::int32_t property, std::int32_t slot, bool defaults = false) { return Join({Token(0x1au), Int(slot), Ref(defaults ? 0x02u : 0x01u, property)}); }
Code StructMember(std::int32_t field, std::int32_t property, bool defaults = false) { return Join({Ref(0x36u, field), Ref(defaults ? 0x02u : 0x01u, property)}); }
Code Final(std::int32_t function, std::initializer_list<Code> arguments = {}) {
    Code code = Ref(0x1cu, function);
    for (const auto& argument : arguments) { Append(code.raw, argument.raw); code.logical += argument.logical; }
    return Join({code, Token(0x16u)});
}
struct Flow {
    Code code;
    struct Patch { std::size_t raw; std::string label; };
    std::vector<Patch> patches;
    std::map<std::string, std::size_t> labels;
    void Add(Code part) { Append(code.raw, part.raw); code.logical += part.logical; }
    void Label(const std::string& name) { Require(labels.emplace(name, code.logical).second, "Duplicate generated flow label"); }
    void Target(const std::string& label) { patches.push_back({code.raw.size(), label}); U16(code.raw, 0u); code.logical += 2u; }
    void IfNot(Code condition, const std::string& label) { Add(Token(0x07u)); Target(label); Add(std::move(condition)); }
    void Iterator(Code factory, const std::string& end) { Add(Token(0x2fu)); Add(std::move(factory)); Target(end); }
    Code Finish() {
        for (const auto& patch : patches) {
            const auto found = labels.find(patch.label);
            Require(found != labels.end() && found->second <= 65535u, "Generated flow target outside logical bytecode");
            code.raw.at(patch.raw) = static_cast<std::uint8_t>(found->second);
            code.raw.at(patch.raw + 1u) = static_cast<std::uint8_t>(found->second >> 8u);
        }
        return std::move(code);
    }
};

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
    std::int32_t Import(const std::string& name, std::int32_t outer, const std::string& type = "Class") {
        imports.push_back({Name("Core"), Name(type), outer, Name(name)}); return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) { return Import(name, 0, "Package"); }
    std::int32_t Export(const std::string& name, std::int32_t cls = 0, std::int32_t base = 0, std::int32_t outer = 0, bool field = false) {
        exports.push_back({cls, base, outer, Name(name), ObjectFlags{}, 0, -1}); builders.emplace_back();
        const auto reference = static_cast<std::int32_t>(exports.size()); if (field) children[outer].push_back(reference); return reference;
    }
    void Tag(Bytes& bytes, const std::string& name, std::uint8_t type, const Bytes& value, std::uint8_t slot = 0u, const std::string& structName = {}) {
        Require(value.size() <= 255u, "Generated spawn tag exceeds short fixture bound");
        Index(bytes, Name(name)); bytes.push_back(static_cast<std::uint8_t>(type | 0x50u | (slot ? 0x80u : 0u)));
        if (type == 10u) Index(bytes, Name(structName));
        bytes.push_back(static_cast<std::uint8_t>(value.size())); if (slot) bytes.push_back(slot); Append(bytes, value);
    }
    void IntTag(Bytes& bytes, const std::string& name, std::int32_t value, std::uint8_t slot = 0u) { Bytes data; U32(data, static_cast<std::uint32_t>(value)); Tag(bytes, name, 2u, data, slot); }
    void FloatTag(Bytes& bytes, const std::string& name, float value) { Bytes data; FloatBytes(data, value); Tag(bytes, name, 4u, data); }
    void BoolTag(Bytes& bytes, const std::string& name, bool value) { Index(bytes, Name(name)); bytes.push_back(static_cast<std::uint8_t>(3u | (value ? 0x80u : 0u))); }
    void ObjectTag(Bytes& bytes, const std::string& name, std::int32_t reference) { Bytes data; Index(data, reference); Tag(bytes, name, 5u, data); }
    void NameTag(Bytes& bytes, const std::string& name, const std::string& value) { Bytes data; Index(data, Name(value)); Tag(bytes, name, 6u, data); }
    void VectorTag(Bytes& bytes, const std::string& name, std::array<float, 3> value) { Bytes data; for (auto number : value) FloatBytes(data, number); Tag(bytes, name, 10u, data, 0u, "Vector"); }
    void RotatorTag(Bytes& bytes, const std::string& name, std::array<std::int32_t, 3> value) { Bytes data; for (auto number : value) U32(data, static_cast<std::uint32_t>(number)); Tag(bytes, name, 10u, data, 0u, "Rotator"); }
    void ClassBody(std::int32_t reference, Bytes defaults = {}, std::uint32_t flags = 0u) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference, flags, defaults = std::move(defaults)](std::int32_t next, std::int32_t first) {
            Bytes body; Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjBase); Index(body, next); Index(body, 0); Index(body, first); Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName);
            U32(body, 0); U32(body, 0); U32(body, 0); U64(body, ~std::uint64_t{}); U64(body, ~std::uint64_t{}); U16(body, 0xffffu); U32(body, 0);
            U32(body, flags); body.resize(body.size() + 16u, 0u); for (unsigned i = 0u; i < 4u; ++i) Index(body, 0);
            Append(body, defaults); Index(body, 0); return body;
        };
    }
    void StructBody(std::int32_t reference) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference](std::int32_t next, std::int32_t first) {
            Bytes body{0u}; Index(body, 0); Index(body, next); Index(body, 0); Index(body, first); Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName);
            U32(body, 0); U32(body, 0); U32(body, 0); return body;
        };
    }
    std::int32_t Property(const std::string& name, const std::string& type, std::int32_t owner, std::int32_t core,
        std::uint32_t dimension = 1u, std::int32_t target = 0, std::uint32_t flags = 0u, std::int32_t secondary = 0) {
        const auto reference = Export(name, Import(type, core), 0, owner, true);
        builders.at(static_cast<std::size_t>(reference - 1)) = [type, dimension, target, flags, secondary](std::int32_t next, std::int32_t) {
            Bytes body{0u}; Index(body, 0); Index(body, next); U32(body, dimension); U32(body, flags); Index(body, 0);
            if(flags&0x20u)U16(body,0u);
            if (type == "ClassProperty") { Index(body, target); Index(body, secondary); }
            else if (type == "ObjectProperty" || type == "ByteProperty" || type == "StructProperty" || type == "ArrayProperty") Index(body, target);
            return body;
        }; return reference;
    }
    std::int32_t IntArrayProperty(const std::string& name, std::int32_t owner, std::int32_t core, std::uint32_t flags) {
        // Actual owned UProperty reference, not a name/type approximation.
        const auto array = Property(name, "ArrayProperty", owner, core, 1u, 0, flags);
        const auto inner = Property(name, "IntProperty", array, core);
        builders.at(static_cast<std::size_t>(array - 1)) = [inner, flags](std::int32_t next, std::int32_t) {
            Bytes body{0u}; Index(body, 0); Index(body, next); U32(body, 1u); U32(body, flags); Index(body, 0); Index(body, inner); return body;
        };
        return array;
    }
    std::int32_t Function(const std::string& name, std::int32_t owner, std::int32_t core, Code code = {}, std::uint16_t native = 0u, std::uint32_t flags = 0u) {
        const auto reference = Export(name, Import("Function", core), 0, owner, true); FunctionCode(reference, std::move(code), native, flags); return reference;
    }
    void FunctionCode(std::int32_t reference, Code code, std::uint16_t native = 0u, std::uint32_t flags = 0u) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference, native, flags, code = std::move(code)](std::int32_t next, std::int32_t first) {
            Bytes body{0u}; Index(body, 0); Index(body, next); Index(body, 0); Index(body, first); Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName);
            U32(body, 0); U32(body, 0); U32(body, static_cast<std::uint32_t>(code.logical)); Append(body, code.raw); U16(body, native); body.push_back(0u); U32(body, flags ? flags : native ? 0x402u : 2u); return body;
        };
    }
    void Body(std::int32_t reference, Bytes body) { builders.at(static_cast<std::size_t>(reference - 1)) = [body = std::move(body)](std::int32_t, std::int32_t) { return body; }; }
    void ActorBody(std::int32_t reference, Bytes properties) { Index(properties, 0); Body(reference, std::move(properties)); }
    Bytes Serialize() {
        std::map<std::int32_t, std::int32_t> next;
        for (const auto& [owner, list] : children) { (void)owner; for (std::size_t i = 1u; i < list.size(); ++i) next[list[i - 1]] = list[i]; }
        Bytes bytes; U32(bytes, 0x9e2a83c1u); U16(bytes, 68u); U16(bytes, 0u); bytes.resize(56u, 0u);
        std::vector<Bytes> bodies; std::vector<std::int32_t> offsets;
        for (std::size_t i = 0u; i < exports.size(); ++i) {
            const auto reference = static_cast<std::int32_t>(i + 1u);
            const auto owner = children.find(reference);
            auto body = builders[i] ? builders[i](next[reference], owner == children.end() || owner->second.empty() ? 0 : owner->second.front()) : Bytes{};
            offsets.push_back(static_cast<std::int32_t>(bytes.size())); Append(bytes, body); bodies.push_back(std::move(body));
        }
        Replace32(bytes, 12u, static_cast<std::uint32_t>(names.size())); Replace32(bytes, 16u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& name : names) { Index(bytes, static_cast<std::int32_t>(name.size() + 1u)); bytes.insert(bytes.end(), name.begin(), name.end()); bytes.push_back(0u); U32(bytes, 0); }
        Replace32(bytes, 20u, static_cast<std::uint32_t>(exports.size())); Replace32(bytes, 24u, static_cast<std::uint32_t>(bytes.size()));
        for (std::size_t i = 0u; i < exports.size(); ++i) {
            const auto& e = exports[i]; Index(bytes, e.ObjClass); Index(bytes, e.ObjBase); U32(bytes, static_cast<std::uint32_t>(e.ObjOuter)); Index(bytes, e.ObjName); U32(bytes, static_cast<std::uint32_t>(e.ObjFlags));
            Index(bytes, static_cast<std::int32_t>(bodies[i].size())); if (!bodies[i].empty()) Index(bytes, offsets[i]);
        }
        Replace32(bytes, 28u, static_cast<std::uint32_t>(imports.size())); Replace32(bytes, 32u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& e : imports) { Index(bytes, e.ClassPackage); Index(bytes, e.ClassName); U32(bytes, static_cast<std::uint32_t>(e.ObjOuter)); Index(bytes, e.ObjName); }
        return bytes;
    }
};
struct Fixture {
    std::filesystem::path parent, directory;
    std::map<std::filesystem::path, Bytes> sourceBytes;
    Bytes legacy;
    Fixture() {
        parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0u; attempt < 20u; ++attempt) {
            const auto candidate = parent / ("deusex-actor-spawn-test-" + std::to_string(stamp) + '-' + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) { directory = std::filesystem::canonical(candidate); break; }
        }
        Require(!directory.empty() && directory.parent_path() == parent, "Generated spawn fixture escaped owned temporary parent");
        std::filesystem::create_directory(directory / "System"); std::filesystem::create_directory(directory / "Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime(); std::error_code error; const auto actual = std::filesystem::weakly_canonical(directory, error);
        if (!error && !directory.empty() && actual == directory && actual.parent_path() == parent && actual.filename().string().rfind("deusex-actor-spawn-test-", 0u) == 0u)
            std::filesystem::remove_all(actual, error);
    }
    PortablePackageTables Write(Package& package, bool map = false) {
        const auto path = directory / (map ? "Maps" : "System") / (package.stem + (map ? ".dx" : ".u"));
        auto bytes = package.Serialize(); WriteBytes(path, bytes); sourceBytes.emplace(path, std::move(bytes)); const auto table = LoadPortablePackageTables(path.string());
        Require(table.version == 68u && table.exports.size() == package.exports.size(), "Generated spawn package failed production table reader"); return table;
    }
    Bytes Snapshot(const std::string& name) const {
        const auto path = directory / (name + ".sav"); Require(SavePortableRuntimeState(path.string()), "Cannot capture generated spawn checkpoint"); return ReadBytes(path);
    }
    void Reset() const { Require(LoadPortableRuntimeState((directory / "Legacy.sav").string()), "Cannot reset generated spawn runtime"); Require(Snapshot("ResetCapture") == legacy, "Legacy restore retained births/default patches/native links"); }
    void UnchangedSources() const { for (const auto& [path, bytes] : sourceBytes) Require(ReadBytes(path) == bytes, "Spawn modified generated package source"); }
};
std::string Path(const std::string& actor) { return actor.find('.') == std::string::npos ? "SpawnFixture." + actor : actor; }
Evaluation Object(const std::string& actor) { return {Value::Text(Kind::Object, actor.empty() ? std::string{} : Path(actor)), {}}; }
Evaluation Class(const std::string& cls) { return {Value::Text(Kind::Object, cls.empty() ? std::string{} : cls.find('.') == std::string::npos ? "SpawnClasses." + cls : cls), {}}; }
Evaluation Name(const std::string& name) { return {Value::Text(Kind::Name, name), {}}; }
Evaluation Vector(std::array<float, 3> value) { return {Value::Vector(value), {}}; }
Evaluation Rotation(std::array<std::int32_t, 3> value) { return {Value::Rotator(value), {}}; }
QuestVr::Vm::Result Call(const std::string& actor, const std::string& function, const std::vector<Evaluation>& args = {}) {
    auto result = ExecutePortableActorFunction(Path(actor), function, args); Require(result.passed(), actor + '.' + function + " failed: " + result.error + " at " + result.function + ':' + std::to_string(result.offset)); return result;
}
Value Read(const std::string& actor, const std::string& property, std::uint32_t slot = 0u) { return ReadPortableActorScriptProperty(Path(actor), property, slot); }
void Same(const Value& a, const Value& b, const std::string& message) { Require(QuestVr::Vm::Equal(a, b), message); }
std::int32_t Integer(const std::string& actor, const std::string& property) { return QuestVr::Vm::ToInt(Read(actor, property)); }
std::string Spawn(const std::string& function = "Make", const std::vector<Evaluation>& args = {Class("Leaf")}) {
    const auto value = Call("Driver", function, args).value; Require(value.kind == Kind::Object && !value.text.empty(), "Native278 did not return an actual actor identity"); return value.text;
}
std::vector<std::string> ActorPaths(bool inactive = false) {
    std::vector<std::string> paths; for (const auto& actor : GetPortableRuntimeMapActors(inactive)) paths.push_back(actor.objectPath); return paths;
}
bool Published(const std::string& actor) { const auto paths = ActorPaths(true); return std::find(paths.begin(), paths.end(), Path(actor)) != paths.end(); }
void Unavailable(const std::string& path, const std::string& message) {
    bool refused{}; try { static_cast<void>(Read(path, "Counter")); } catch (const std::exception&) { refused = true; } Require(refused, message);
}
void RefusedCall(const Fixture& fixture, const std::string& function, const std::vector<Evaluation>& args = {}, const QuestVr::Vm::Limits& limits = {}, const std::string& actor = "Driver") {
    const auto before = fixture.Snapshot("BeforeRefusal"); const auto paths = ActorPaths(true); const auto revision = GetPortableRuntimeWorldRevision();
    const auto beforeGc = GC::GetStats();
    const auto result = ExecutePortableActorFunction(Path(actor), function, args, limits); Require(!result.passed() && !result.committed, "Invalid actor call silently committed: " + actor + '.' + function);
    // Inspect immediately on return, before a save or another runtime entry can
    // hide leaked rollback allocations by performing its own collection.
    const auto afterGc = GC::GetStats();
    Require(afterGc.numObjects == beforeGc.numObjects && afterGc.memoryUsage == beforeGc.memoryUsage,
        "Rejected Spawn retained unrooted allocations at its public execution boundary: " + function);
    Require(fixture.Snapshot("AfterRefusal") == before && ActorPaths(true) == paths && GetPortableRuntimeWorldRevision() == revision, "Failed Spawn callback/root changed actor publication/checkpoint/revision: " + function); ++refusals;
}
std::uint32_t Read32(const Bytes& bytes, std::size_t& cursor) {
    Require(cursor <= bytes.size() && bytes.size() - cursor >= 4u, "Independent spawn checkpoint parser truncated"); std::uint32_t result{};
    for (unsigned i = 0u; i < 4u; ++i) result |= static_cast<std::uint32_t>(bytes[cursor++]) << (8u * i);
    return result;
}
std::size_t PrefixSize(const Bytes& bytes) {
    std::size_t cursor{}; Require(Read32(bytes, cursor) == 0x53515844u, "Independent spawn checkpoint magic mismatch"); const auto version = Read32(bytes, cursor); Require(version >= 1u && version <= 13u, "Independent spawn checkpoint version mismatch");
    const auto string = [&]() { const auto size = Read32(bytes, cursor); Require(cursor <= bytes.size() && size <= bytes.size() - cursor, "Independent spawn checkpoint string overflow"); cursor += size; };
    const auto strings = [&]() { const auto count = Read32(bytes, cursor); for (std::uint32_t i = 0u; i < count; ++i) string(); };
    strings(); strings(); strings();
    if (version >= 2u) { static_cast<void>(Read32(bytes, cursor)); const auto count = Read32(bytes, cursor); for (std::uint32_t i = 0u; i < count; ++i) { string(); static_cast<void>(Read32(bytes, cursor)); } }
    if (version >= 3u) { static_cast<void>(Read32(bytes, cursor)); static_cast<void>(Read32(bytes, cursor)); for (unsigned i = 0u; i < 4u; ++i) strings(); }
    return cursor;
}
QuestVr::ScriptSavedState Decode(const Bytes& bytes) {
    auto cursor = PrefixSize(bytes); const auto size = Read32(bytes, cursor); Require(size == bytes.size() - cursor, "Independent spawn trailer size mismatch");
    return QuestVr::DecodeScriptSavedState(Bytes(bytes.begin() + static_cast<std::ptrdiff_t>(cursor), bytes.end()));
}
Bytes Envelope(const Bytes& prefixSource, const QuestVr::ScriptSavedState& script, std::uint32_t version = 0u) {
    const auto prefix = PrefixSize(prefixSource); Bytes result(prefixSource.begin(), prefixSource.begin() + static_cast<std::ptrdiff_t>(prefix));
    const auto blob = QuestVr::EncodeScriptSavedState(script); Replace32(result, 4u, version ? version : 3u + blob.at(6u));
    U32(result, static_cast<std::uint32_t>(blob.size())); Append(result, blob); return result;
}
Bytes InactivePrefix(Bytes bytes, const std::string& actor) {
    // Change only the gameplay inactive list, leaving the actual envelope,
    // remaining legacy fields, and complete versioned script trailer intact.
    std::size_t cursor = 8u; const auto inventory = Read32(bytes, cursor);
    for (std::uint32_t i = 0u; i < inventory; ++i) {
        const auto length = Read32(bytes, cursor); Require(length <= bytes.size() - cursor, "Inactive-prefix fixture inventory string overflow"); cursor += length;
    }
    const auto listStart = cursor; Require(Read32(bytes, cursor) == 0u, "Inactive-prefix fixture requires an initially empty inactive list");
    Bytes changed(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(listStart));
    U32(changed, 1u); U32(changed, static_cast<std::uint32_t>(actor.size())); changed.insert(changed.end(), actor.begin(), actor.end());
    changed.insert(changed.end(), bytes.begin() + static_cast<std::ptrdiff_t>(cursor), bytes.end()); return changed;
}
std::string Fold(std::string value) {
    for (auto& letter : value) if (letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter - 'A' + 'a');
    return value;
}
bool HasPath(const std::vector<std::string>& paths, const std::string& target) {
    return std::any_of(paths.begin(), paths.end(), [&](const auto& path) { return Fold(path) == Fold(target); });
}
QuestVr::ScriptSavedObject& Record(QuestVr::ScriptSavedState& state, const std::string& path) {
    const auto found = std::find_if(state.objects.begin(), state.objects.end(), [&](const auto& object) { return Fold(object.path) == Fold(path); });
    if (found == state.objects.end()) throw std::runtime_error("Generated spawn checkpoint lacks actor record " + path);
    return *found;
}
void InitializedFrame(QuestVr::ScriptSavedState& state, const std::string& path, const std::string& cls) {
    const auto& object = Record(state, path);
    Require(object.state && object.state->hasStack && object.state->frameOverride && object.state->frame,
        "Owned Spawn did not initialize an explicit HasStack frame");
    const auto& frame = *object.state->frame;
    Require(frame.control && frame.control->selectedPath == cls && frame.codePath == cls && !frame.control->codePresent &&
        frame.control->liveProbeMask == ~std::uint64_t{}, "Owned InitExecution lost class-backed Node/StateNode, null Code or initial positive probe mask");
    Require(frame.localsCodePath.empty() && frame.locals.empty() && object.state->disabled.empty() &&
        frame.statementIndex == 0u && frame.latent == QuestVr::StateLatent::Continue,
        "Owned InitExecution fabricated Locals/legacy disabled sets or lost the portable null-Code latent policy");
}
QuestVr::ScriptSavedBirth& Birth(QuestVr::ScriptSavedState& state, const std::string& path) {
    const auto found = std::find_if(state.births.begin(), state.births.end(), [&](const auto& birth) { return birth.path == path; });
    if (found == state.births.end()) throw std::runtime_error("Generated save lacks birth " + path);
    return *found;
}
QuestVr::ScriptSavedProperty& Frozen(QuestVr::ScriptSavedState& state, const std::string& path, const std::string& name, std::uint32_t slot = 0u) {
    auto& properties = Birth(state, path).frozenDefaults; const auto found = std::find_if(properties.begin(), properties.end(), [&](const auto& property) { return property.name == name && property.index == slot; });
    if (found == properties.end()) throw std::runtime_error("Birth lacks frozen property " + name);
    return *found;
}
void ChangeBirthClass(QuestVr::ScriptSavedState& state, const std::string& path, const std::string& cls) {
    Birth(state, path).classPath = cls;
    for (auto& object : state.objects) if (object.path == path) object.classPath = cls;
}
void RefusedSave(const Fixture& fixture, const Bytes& prefix, const QuestVr::ScriptSavedState& script, const std::string& label, std::uint32_t version = 0u) {
    const auto current = fixture.Snapshot("BeforeBadSave"); const auto paths = ActorPaths(true); const auto revision = GetPortableRuntimeWorldRevision();
    const auto path = fixture.directory / (label + ".sav"); WriteBytes(path, Envelope(prefix, script, version));
    Require(!ValidatePortableRuntimeState(path.string(), "SpawnFixture") && !LoadPortableRuntimeState(path.string()), "Invalid birth manifest validated/loaded: " + label);
    Require(fixture.Snapshot("AfterBadSave") == current && ActorPaths(true) == paths && GetPortableRuntimeWorldRevision() == revision, "Invalid birth manifest partially published: " + label); ++refusals;
}

struct Tables { PortablePackageTables core, engine, classes, map; std::size_t initialActors{}; };
// Decal fixture variants: 1 exact; 2 malformed Texture target; 3 malformed
// native function flags; 4 malformed return flags; 5 malformed optional flags;
// 6 malformed signed Bitmap.USize declaration. World models5/6 are a real
// finite floor; model6 also binds its surface to an authored Mover.
Tables Build(Fixture& fixture, unsigned worldModel = 0u, unsigned decalSchema = 0u, unsigned loginVariant = 0u,
    bool contextVariant = false) {
    const bool decalFloor = worldModel == 5u || worldModel == 6u;
    const bool decalMover = worldModel == 6u;
    const auto contextFlags=[&](Package& package) {
        if(contextVariant) for(auto& entry:package.exports)
            entry.ObjFlags=static_cast<ObjectFlags>(static_cast<std::uint32_t>(entry.ObjFlags)|0x00070000u);
    };
    Package core; core.stem = "Core"; const auto corePackage = core.ImportPackage("Core"), object = core.Export("Object"), metaclass = core.Export("Class", 0, object);
    const auto vector = core.Export("Vector", core.Import("Struct", corePackage), 0, object), rotator = core.Export("Rotator", core.Import("Struct", corePackage), 0, object);
    for (const auto& name : {"X", "Y", "Z"}) core.Property(name, "FloatProperty", vector, corePackage);
    for (const auto& name : {"Pitch", "Yaw", "Roll"}) core.Property(name, "IntProperty", rotator, corePackage);
    const auto record = core.Export("Record", core.Import("Struct", corePackage), 0, object);
    const auto recordCount = core.Property("Count", "IntProperty", record, corePackage);
    const auto coreEngine = core.ImportPackage("Engine"), coreActor = core.Import("Actor", coreEngine);
    const auto recordTarget = core.Property("Target", "ObjectProperty", record, corePackage, 1u, coreActor);
    core.Property("Class", "ClassProperty", object, corePackage, 1u, metaclass, 0u, object);
    core.Property("Name", "NameProperty", object, corePackage); core.Property("Outer", "ObjectProperty", object, corePackage, 1u, object);
    core.Property("ObjectFlags", "IntProperty", object, corePackage,1u,0,contextVariant?0x1002u:0u);
    core.ClassBody(object); core.ClassBody(metaclass); core.StructBody(vector); core.StructBody(rotator); core.StructBody(record);
    const auto coreExcluded=contextVariant ? core.Export("ContextSourceOnly",object) : 0;
    if(contextVariant)core.ActorBody(coreExcluded,{});
    contextFlags(core);
    if(contextVariant)core.exports.at(static_cast<std::size_t>(coreExcluded-1)).ObjFlags=static_cast<ObjectFlags>(0x00340000u);
    const auto coreTable = fixture.Write(core);

    Package engine; engine.stem = "Engine"; const auto engineCore = engine.ImportPackage("Core"), importedObject = engine.Import("Object", engineCore), importedClass = engine.Import("Class", engineCore);
    const auto importedVector = engine.Import("Vector", importedObject, "Struct"), importedRotator = engine.Import("Rotator", importedObject, "Struct");
    const auto actor = engine.Export("Actor", 0, importedObject), zone = engine.Export("ZoneInfo", 0, actor), levelInfo = engine.Export("LevelInfo", 0, zone);
    const auto level = engine.Export("Level", 0, importedObject), model = engine.Export("Model", 0, importedObject), brush = engine.Export("Brush", 0, actor), pawn = engine.Export("Pawn", 0, actor);
    const auto player = engine.Export("Player", 0, importedObject), viewport = engine.Export("Viewport", 0, player);
    const auto playerPawn = engine.Export("PlayerPawn", 0, pawn);
    const auto contextCamera=contextVariant ? engine.Export("Camera",0,playerPawn) : 0;
    const auto decoration = engine.Export("Decoration", 0, actor);
    const auto gameInfo = engine.Export("GameInfo", 0, actor);
    const auto bitmap = decalSchema ? engine.Export("Bitmap", 0, importedObject) : 0;
    const auto texture = decalSchema ? engine.Export("Texture", 0, bitmap) : 0;
    const auto decal = decalSchema ? engine.Export("Decal", 0, actor) : 0;
    const auto mover = decalSchema ? engine.Export("Mover", 0, brush) : 0;
    const auto notify = engine.Export("SpawnNotify", 0, actor), pointRegion = engine.Export("PointRegion", engine.Import("Struct", engineCore), 0, actor);
    engine.Property("Zone", "ObjectProperty", pointRegion, engineCore, 1u, zone); engine.Property("iLeaf", "IntProperty", pointRegion, engineCore); engine.Property("ZoneNumber", "ByteProperty", pointRegion, engineCore);
    engine.StructBody(pointRegion);
    engine.Property("Owner", "ObjectProperty", actor, engineCore, 1u, actor, 0x22u);
    for (const auto& name : {"LostChild", "GainedChild"}) {
        const auto event = engine.Function(name, actor, engineCore, Return(), 0u, 0x800u);
        engine.Property("Other", "ObjectProperty", event, engineCore, 1u, actor, 0x80u);
    }
    engine.Property("Base", "ObjectProperty", actor, engineCore, 1u, actor, 0x22u);
    const auto actorLevelField = engine.Property("Level", "ObjectProperty", actor, engineCore, 1u, levelInfo, 0x2u);
    const auto actorXLevelField = engine.Property("XLevel", "ObjectProperty", actor, engineCore, 1u, level);
    const auto loadedObjectPath = engine.Function("LoadedObjectPath",actor,engineCore);
    engine.Property("ReturnValue","StrProperty",loadedObjectPath,engineCore,1u,0,0x580u);
    engine.FunctionCode(loadedObjectPath,Join({Token(0x04u),Token(0x56u),Ref(0x20u,actorLevelField)}));
    engine.Property("Instigator", "ObjectProperty", actor, engineCore, 1u, pawn);
    // Exact original declarations are needed even for CheckEncroachment's
    // inactive false path. A Brush is an Actor, not Actor.Brush's Model type.
    engine.Property("Brush", "ObjectProperty", actor, engineCore, 1u, model, 0x2au);
    engine.Property("Location", "StructProperty", actor, engineCore, 1u, importedVector, 0x23u);
    engine.Property("OldLocation", "StructProperty", actor, engineCore, 1u, importedVector);
    engine.Property("Rotation", "StructProperty", actor, engineCore, 1u, importedRotator, 0x23u);
    engine.Property("Region", "StructProperty", actor, engineCore, 1u, pointRegion);
    for (const auto& name : {"Tag", "AttachTag"}) engine.Property(name, "NameProperty", actor, engineCore);
    std::int32_t deleteFlag{},tickedFlag{};
    for (const auto& name : {"bTicked", "bCollideActors", "bBlockActors", "bBlockPlayers", "bCollideWorld", "bCollideWhenPlacing", "bStatic", "bNoDelete", "bDeleteMe", "bAnimByOwner"}) {
        const auto flags=std::string(name)=="bTicked"?0x2002u:std::string(name)=="bStatic"?3u:std::string(name)=="bCollideActors"?0x23u:
            (std::string(name)=="bCollideWorld"||std::string(name)=="bBlockActors"||std::string(name)=="bBlockPlayers")?0x21u:0u;
        const auto field = engine.Property(name, "BoolProperty", actor, engineCore, 1u, 0, flags);
        if (std::string(name) == "bDeleteMe") deleteFlag = field;
        if (std::string(name) == "bTicked") tickedFlag = field;
    }
    const auto setTicked=engine.Function("SetFixtureTicked",actor,engineCore);
    const auto tickedInput=engine.Property("Input","BoolProperty",setTicked,engineCore,1u,0,0x80u);
    engine.FunctionCode(setTicked,Join({Assign(Ref(0x01u,tickedFlag),Ref(0x00u,tickedInput)),Return()}));
    for (const auto& name : {"CollisionRadius", "CollisionHeight"}) engine.Property(name, "FloatProperty", actor, engineCore);
    // Native startup/Spawn now joins this genuine scalar Float to its saved
    // property mirror. The generated Engine schema must declare it too.
    engine.Property("LastRenderTime", "FloatProperty", actor, engineCore);
    if (decalSchema) {
        engine.Property("USize", decalSchema == 6u ? "FloatProperty" : "IntProperty", bitmap, engineCore);
        const auto textureField = engine.Property("Texture", "ObjectProperty", actor, engineCore, 1u, decalSchema == 2u ? actor : texture);
        engine.Property("DrawScale", "FloatProperty", actor, engineCore);
        const auto multi = engine.Property("MultiDecalLevel", "IntProperty", decal, engineCore);
        engine.Property("LastRenderedTime", "FloatProperty", decal, engineCore);
        const auto surfaces = engine.IntArrayProperty("SurfList", decal, engineCore, 0x00400002u);
        const auto attach = engine.Function("AttachDecal", decal, engineCore, {}, 0u, decalSchema == 3u ? 0x501u : 0x500u);
        engine.Function("DetachDecal", decal, engineCore, {}, 0u, 0x500u);
        engine.Property("TraceDistance", "FloatProperty", attach, engineCore, 1u, 0, 0x80u);
        engine.Property("DecalDir", "StructProperty", attach, engineCore, 1u, importedVector, decalSchema == 5u ? 0x80u : 0x90u);
        engine.Property("ReturnValue", "BoolProperty", attach, engineCore, 1u, 0, decalSchema == 4u ? 0x480u : 0x580u);
        const auto attachDefault = engine.Function("AttachDefault", decal, engineCore, Return(Final(attach, {Number(100.0f)})));
        engine.Property("ReturnValue", "BoolProperty", attachDefault, engineCore, 1u, 0, 0x580u);
        engine.Function("AttachThenFail", decal, engineCore, Join({Final(attach, {Number(100.0f)}), Native(999u), Return()}));
        const auto counter = engine.Property("ArgumentCounter", "IntProperty", decal, engineCore);
        // The interpreter write quota meters actual script assignments/guarded
        // OUT references, not the host native's direct property journal. Fail
        // after a real attachment and one script write, rather than assuming
        // native SurfList/clamp mutations are interpreter assignment opcodes.
        engine.Function("AttachThenWrites", decal, engineCore, Join({Final(attach, {Number(100.0f)}),
            Assign(Ref(0x01u,counter),Int(1)),Assign(Ref(0x01u,counter),Int(2)),Return()}));
        const auto distance = engine.Function("DistanceWithSideEffect", decal, engineCore);
        engine.Property("ReturnValue", "FloatProperty", distance, engineCore, 1u, 0, 0x580u);
        engine.FunctionCode(distance, Join({Assign(Ref(0x01u, counter), Native(146u, {Ref(0x01u, counter), Int(1)})), Return(Number(100.0f))}));
        const auto direction = engine.Function("DirectionWithSideEffect", decal, engineCore);
        engine.Property("ReturnValue", "StructProperty", direction, engineCore, 1u, importedVector, 0x580u);
        engine.FunctionCode(direction, Join({Assign(Ref(0x01u, counter), Native(146u, {Ref(0x01u, counter), Int(10)})), Return(VectorLiteral({1.0f, 0.0f, 0.0f}))}));
        const auto sideEffects = engine.Function("AttachEvaluatedArguments", decal, engineCore, Return(Final(attach, {Final(distance), Final(direction)})));
        engine.Property("ReturnValue", "BoolProperty", sideEffects, engineCore, 1u, 0, 0x580u);
        const auto attachThenOut = engine.Function("AttachThenOutWrite", decal, engineCore);
        const auto out = engine.Property("Output", "IntProperty", attachThenOut, engineCore, 1u, 0, 0x180u);
        engine.FunctionCode(attachThenOut, Join({Final(attach, {Final(distance), Final(direction)}),
            Assign(Ref(0x00u, out), Int(73)), Return()}));
        const auto setMulti = engine.Function("SetMulti", decal, engineCore);
        const auto multiInput = engine.Property("Input", "IntProperty", setMulti, engineCore, 1u, 0, 0x80u);
        engine.FunctionCode(setMulti, Join({Assign(Ref(0x01u, multi), Ref(0x00u, multiInput)), Return()}));
        const auto setTexture = engine.Function("SetTexture", decal, engineCore);
        const auto textureInput = engine.Property("Input", "ObjectProperty", setTexture, engineCore, 1u, decalSchema == 2u ? actor : texture, 0x80u);
        engine.FunctionCode(setTexture, Join({Assign(Ref(0x01u, textureField), Ref(0x00u, textureInput)), Return()}));
        for (const auto& [name, fieldName, type, target] : std::vector<std::tuple<std::string, std::string, std::string, std::int32_t>>{
            {"SetDrawScale", "DrawScale", "FloatProperty", 0},
            {"SetDecalLocation", "Location", "StructProperty", importedVector},
            {"SetDecalRotation", "Rotation", "StructProperty", importedRotator}}) {
            const auto function = engine.Function(name, decal, engineCore);
            const auto input = engine.Property("Input", type, function, engineCore, 1u, target, 0x80u);
            const auto found = std::find_if(engine.exports.begin(), engine.exports.end(), [&](const auto& entry) {
                return entry.ObjOuter == actor && engine.names.at(static_cast<std::size_t>(entry.ObjName)) == fieldName;
            });
            Require(found != engine.exports.end(), "Generated Decal mutator lacks actual Actor field");
            const auto field = static_cast<std::int32_t>(found - engine.exports.begin() + 1u);
            engine.FunctionCode(function, Join({Assign(Ref(0x01u, field), Ref(0x00u, input)), Return()}));
        }
        // SurfList's original Const bit must still deny script assignment.
        const auto writeSurfaces = engine.Function("WriteSurfList", decal, engineCore);
        const auto surfacesInput = engine.IntArrayProperty("Input", writeSurfaces, engineCore, 0x00400080u);
        engine.FunctionCode(writeSurfaces, Join({Assign(Ref(0x01u, surfaces), Ref(0x00u, surfacesInput)), Return()}));
    }
    engine.Property("Physics", "ByteProperty", actor, engineCore); engine.Property("StandingCount", "ByteProperty", actor, engineCore, 1u, 0, 0x2u); engine.Property("Touching", "ObjectProperty", actor, engineCore, 4u, actor, 2u);
    engine.Property("bWaterZone", "BoolProperty", zone, engineCore); engine.Property("bBegunPlay", "BoolProperty", levelInfo, engineCore);
    engine.Property("bStartup", "BoolProperty", levelInfo, engineCore);
    engine.Property("TimeSeconds", "FloatProperty", levelInfo, engineCore);
    engine.Property("NetMode", "ByteProperty", levelInfo, engineCore);
    engine.Property("Game", "ObjectProperty", levelInfo, engineCore, 1u, gameInfo);
    engine.Property("DefaultGameType", "ClassProperty", levelInfo, engineCore, 1u, importedClass, 0u, gameInfo);
    engine.Property("SpawnNotify", "ObjectProperty", levelInfo, engineCore, 1u, notify);
    engine.Property("PawnList", "ObjectProperty", levelInfo, engineCore, 1u, pawn);
    const auto nextPawn = engine.Property("nextPawn", "ObjectProperty", pawn, engineCore, 1u, pawn);
    const auto pawnRole = engine.Property("Role", "ByteProperty", actor, engineCore);
    const auto pawnShadow = engine.Property("Shadow", "ObjectProperty", pawn, engineCore, 1u, actor);
    engine.Function("AddPawn", pawn, engineCore, {}, 529u, 0x401u);
    engine.Function("RemovePawn", pawn, engineCore, {}, 530u, 0x401u);
    engine.Function("PawnAddThenFail", pawn, engineCore, Join({Native(529u), Native(999u), Return()}));
    engine.Function("PawnRemoveThenFail", pawn, engineCore, Join({Native(530u), Native(999u), Return()}));
    engine.Function("PawnAddTooMany", pawn, engineCore, Join({Native(529u, {Int(1)}), Return()}));
    engine.Function("PawnRemoveTooMany", pawn, engineCore, Join({Native(530u, {Int(1)}), Return()}));
    for (const auto& [name, property, type] : std::vector<std::tuple<std::string, std::int32_t, std::int32_t>>{
        {"SetNextPawn", nextPawn, pawn}, {"SetPawnShadow", pawnShadow, actor}}) {
        const auto function = engine.Function(name, pawn, engineCore);
        const auto input = engine.Property("Input", "ObjectProperty", function, engineCore, 1u, type, 0x80u);
        engine.FunctionCode(function, Join({Assign(Ref(0x01u, property), Ref(0x00u, input)), Return()}));
    }
    const auto pawnFlags = engine.Function("SetPawnFlags", pawn, engineCore);
    const auto pawnDeleted = engine.Property("Deleted", "BoolProperty", pawnFlags, engineCore, 1u, 0, 0x80u);
    const auto pawnRoleInput = engine.Property("RoleInput", "ByteProperty", pawnFlags, engineCore, 1u, 0, 0x80u);
    engine.FunctionCode(pawnFlags, Join({Assign(Ref(0x01u, deleteFlag), Ref(0x00u, pawnDeleted)),
        Assign(Ref(0x01u, pawnRole), Ref(0x00u, pawnRoleInput)), Return()}));
    const auto pawnBindings = engine.Function("SetPawnBindings", pawn, engineCore);
    const auto pawnLevelInput = engine.Property("LevelInput", "ObjectProperty", pawnBindings, engineCore, 1u, levelInfo, 0x80u);
    const auto pawnXLevelInput = engine.Property("XLevelInput", "ObjectProperty", pawnBindings, engineCore, 1u, level, 0x80u);
    engine.FunctionCode(pawnBindings, Join({Assign(Ref(0x01u, actorLevelField), Ref(0x00u, pawnLevelInput)),
        Assign(Ref(0x01u, actorXLevelField), Ref(0x00u, pawnXLevelInput)), Return()}));
    for (const auto& name : {"FootRegion", "HeadRegion"}) engine.Property(name, "StructProperty", pawn, engineCore, 1u, pointRegion);
    engine.Property("EyeHeight", "FloatProperty", pawn, engineCore);
    const auto isPlayer = engine.Property("bIsPlayer", "BoolProperty", pawn, engineCore);
    engine.Property("PlayerReplicationInfo", "ObjectProperty", pawn, engineCore, 1u, actor);
    const auto hiddenFlag = engine.Property("bHidden", "BoolProperty", actor, engineCore);
    const auto boundPlayer = engine.Property("Player", "ObjectProperty", playerPawn, engineCore, 1u, player);
    // Only the Login fixture prepares an actual native Client/Viewport. Keep
    // existing generated schemas/identities unchanged in all older variants.
    if (loginVariant) engine.Property("Actor", "ObjectProperty", player, engineCore, 1u, actor);
    const auto bindPlayer = engine.Function("BindPlayer", playerPawn, engineCore);
    const auto playerInput = engine.Property("Input", "ObjectProperty", bindPlayer, engineCore, 1u, player, 0x80u);
    engine.FunctionCode(bindPlayer, Join({Assign(Ref(0x01u, boundPlayer), Ref(0x00u, playerInput)), Return()}));
    const auto playerFlags = engine.Function("SetPlayerFlags", playerPawn, engineCore);
    const auto deleteInput = engine.Property("Deleted", "BoolProperty", playerFlags, engineCore, 1u, 0, 0x80u);
    const auto isPlayerInput = engine.Property("IsPlayer", "BoolProperty", playerFlags, engineCore, 1u, 0, 0x80u);
    engine.FunctionCode(playerFlags, Join({Assign(Ref(0x01u, deleteFlag), Ref(0x00u, deleteInput)),
        Assign(Ref(0x01u, isPlayer), Ref(0x00u, isPlayerInput)), Return()}));
    engine.Property("Next", "ObjectProperty", notify, engineCore, 1u, notify); engine.Property("ActorClass", "ClassProperty", notify, engineCore, 1u, importedClass, 0u, actor);
    const auto spawnFunction = engine.Function("Spawn", actor, engineCore, {}, 278u);
    engine.Property("SpawnClass", "ClassProperty", spawnFunction, engineCore, 1u, importedClass, 0x80u, actor);
    engine.Property("SpawnOwner", "ObjectProperty", spawnFunction, engineCore, 1u, actor, 0x90u); engine.Property("SpawnTag", "NameProperty", spawnFunction, engineCore, 1u, 0, 0x90u);
    engine.Property("SpawnLocation", "StructProperty", spawnFunction, engineCore, 1u, importedVector, 0x90u); engine.Property("SpawnRotation", "StructProperty", spawnFunction, engineCore, 1u, importedRotator, 0x90u);
    engine.Property("ReturnValue", "ObjectProperty", spawnFunction, engineCore, 1u, actor, 0x480u);
    const auto setCollision = engine.Function("SetCollision", actor, engineCore, {}, 262u);
    for (const auto& name : {"NewColActors", "NewBlockActors", "NewBlockPlayers"}) engine.Property(name, "BoolProperty", setCollision, engineCore, 1u, 0, 0x90u);
    const auto setSize = engine.Function("SetCollisionSize", actor, engineCore, {}, 283u);
    for (const auto& name : {"NewRadius", "NewHeight"}) engine.Property(name, "FloatProperty", setSize, engineCore, 1u, 0, 0x80u);
    engine.Property("ReturnValue", "BoolProperty", setSize, engineCore, 1u, 0, 0x480u);
    const auto getPlayer = engine.Function("GetPlayerPawn", actor, engineCore, {}, 720u);
    engine.Property("ReturnValue", "ObjectProperty", getPlayer, engineCore, 1u, playerPawn, 0x480u);
    const auto destroy = engine.Function("Destroy", actor, engineCore, {}, 279u);
    engine.Property("ReturnValue", "BoolProperty", destroy, engineCore, 1u, 0, 0x480u);
    const auto allActors = engine.Function("AllActors", actor, engineCore, {}, 304u, 0x404u);
    engine.Property("BaseClass", "ClassProperty", allActors, engineCore, 1u, importedClass, 0x80u, actor);
    engine.Property("Actor", "ObjectProperty", allActors, engineCore, 1u, actor, 0x180u);
    engine.Property("MatchTag", "NameProperty", allActors, engineCore, 1u, 0, 0x90u);
    const auto actorFlags = engine.Function("SetIterationFlags", actor, engineCore);
    const auto hiddenInput = engine.Property("Hidden", "BoolProperty", actorFlags, engineCore, 1u, 0, 0x80u);
    const auto deletedInput = engine.Property("Deleted", "BoolProperty", actorFlags, engineCore, 1u, 0, 0x80u);
    engine.FunctionCode(actorFlags, Join({Assign(Ref(0x01u, hiddenFlag), Ref(0x00u, hiddenInput)),
        Assign(Ref(0x01u, deleteFlag), Ref(0x00u, deletedInput)), Return()}));
    engine.Function("InitEventManager", levelInfo, engineCore, {}, 650u);
    engine.Function("InitAI", levelInfo, engineCore, Join({Native(650u), Return()}));
    engine.Function("InitAIThenFail", levelInfo, engineCore, Join({Native(650u), Native(999u), Return()}));
    for (const auto& [name, index] : std::vector<std::pair<std::string, std::uint16_t>>{
        {"AISetEventCallback", 710u}, {"AIClearEventCallback", 711u}, {"AISendEvent", 713u},
        {"AIStartEvent", 714u}, {"AIEndEvent", 715u}, {"AIClearEvent", 716u}}) {
        const auto function = engine.Function(name, actor, engineCore, {}, index);
        engine.Property("EventName", "NameProperty", function, engineCore, 1u, 0, 0x80u);
        if (index == 710u) {
            engine.Property("Callback", "NameProperty", function, engineCore, 1u, 0, 0x80u);
            engine.Property("ScoreCallback", "NameProperty", function, engineCore, 1u, 0, 0x90u);
            for (const auto& flag : {"Visibility", "Direction", "Cylinder", "LineOfSight"})
                engine.Property(flag, "BoolProperty", function, engineCore, 1u, 0, 0x90u);
        } else if (index == 713u || index == 714u || index == 715u) {
            engine.Property("Channel", "ByteProperty", function, engineCore, 1u, 0, 0x80u);
            if (index != 715u) for (const auto& field : {"Value", "Radius"})
                engine.Property(field, "FloatProperty", function, engineCore, 1u, 0, 0x90u);
        }
    }
    Bytes actorDefaults; engine.FloatTag(actorDefaults, "CollisionRadius", 12.0f); engine.FloatTag(actorDefaults, "CollisionHeight", 20.0f); engine.ClassBody(actor, actorDefaults);
    for (auto cls : {zone, levelInfo, level, model, brush, pawn, player, viewport, playerPawn, notify, decoration, gameInfo}) engine.ClassBody(cls);
    if (decalSchema) {
        engine.ClassBody(bitmap); engine.ClassBody(texture); engine.ClassBody(mover);
        Bytes defaults; engine.IntTag(defaults, "MultiDecalLevel", 4); engine.ClassBody(decal, defaults);
    }
    const auto engineExcluded=contextVariant ? engine.Export("ContextSourceOnly",importedObject) : 0;
    if(contextVariant) {engine.ClassBody(contextCamera);engine.ActorBody(engineExcluded,{});}
    contextFlags(engine);
    if(contextVariant)engine.exports.at(static_cast<std::size_t>(engineExcluded-1)).ObjFlags=static_cast<ObjectFlags>(0x00340000u);
    const auto engineTable = fixture.Write(engine);

    Package classes; classes.stem = "SpawnClasses"; const auto classesCore = classes.ImportPackage("Core"), classesEngine = classes.ImportPackage("Engine");
    const auto importedActor = classes.Import("Actor", classesEngine), classesObject = classes.Import("Object", classesCore), classesClass = classes.Import("Class", classesCore);
    const auto classesVector = classes.Import("Vector", classesObject, "Struct"), classesRotator = classes.Import("Rotator", classesObject, "Struct"), classesRecord = classes.Import("Record", classesObject, "Struct");
    const auto importedCount = classes.Import("Count", classesRecord, "IntProperty"), importedTarget = classes.Import("Target", classesRecord, "ObjectProperty");
    const auto probe = classes.Export("Probe", 0, importedActor), leaf = classes.Export("Leaf", 0, probe), abstract = classes.Export("Abstract", 0, probe);
    const auto fail = classes.Export("FailPost", 0, probe), nested = classes.Export("Nested", 0, probe), recursive = classes.Export("Recursive", 0, probe), selfDelete = classes.Export("SelfDelete", 0, probe);
    const auto captureOwner = classes.Export("CaptureOwner", 0, probe);
    const auto tagMutator = classes.Export("TagMutator", 0, probe);
    const auto tagNotify = classes.Export("TagNotify", 0, classes.Import("SpawnNotify", classesEngine));
    const auto pointBase = classes.Export("PointBase", 0, classes.Import("Decoration", classesEngine)), support = classes.Export("Support", 0, probe);
    const auto attachTaggedBirth = classes.Export("AttachTaggedBirth", 0, probe);
    const auto importedPawn = classes.Import("Pawn", classesEngine), importedPlayer = classes.Import("Player", classesEngine);
    const auto importedPlayerPawn = classes.Import("PlayerPawn", classesEngine);
    const auto importedAllActors = classes.Import("AllActors", importedActor, "Function");
    const auto importedTag = classes.Import("Tag", importedActor, "NameProperty");
    const auto importedAttachTag = classes.Import("AttachTag", importedActor, "NameProperty");
    const auto importedOwner = classes.Import("Owner", importedActor, "ObjectProperty");
    const auto importedXLevel = classes.Import("XLevel", importedActor, "ObjectProperty");
    const auto importedInstigator = classes.Import("Instigator", importedActor, "ObjectProperty");
    const auto importedObjectFlags = classes.Import("ObjectFlags", classesObject, "IntProperty");
    const auto fauxPlayer = classes.Export("FauxPlayer", 0, importedPawn);
    const auto pawnAlias = classes.Export("Pawn", 0, classesObject); // Non-Actor UClass, same NameString leaf as Engine.Pawn.
    const auto loopActor = classes.Export("LoopActor", 0, probe), loopBirth = classes.Export("LoopBirth", 0, loopActor);
    const auto loopDelete = classes.Export("LoopDelete", 0, probe);
    const auto fauxPlayerField = classes.Property("Player", "ObjectProperty", fauxPlayer, classesCore, 1u, importedPlayer);
    const auto bindFauxPlayer = classes.Function("BindPlayer", fauxPlayer, classesCore);
    const auto fauxPlayerInput = classes.Property("Input", "ObjectProperty", bindFauxPlayer, classesCore, 1u, importedPlayer, 0x80u);
    classes.FunctionCode(bindFauxPlayer, Join({Assign(Ref(0x01u, fauxPlayerField), Ref(0x00u, fauxPlayerInput)), Return()}));
    std::map<std::string, std::int32_t> properties;
    for (const auto& name : {"Counter", "Trace", "Seen", "ChildEvents"}) properties.emplace(name, classes.Property(name, "IntProperty", probe, classesCore));
    for (const auto& name : {"Link", "NestedActor"}) properties.emplace(name, classes.Property(name, "ObjectProperty", probe, classesCore, 1u, importedActor));
    const auto slots = classes.Property("Slots", "IntProperty", probe, classesCore, 3u), payload = classes.Property("Payload", "StructProperty", probe, classesCore, 1u, classesRecord);
    const auto matches = classes.Property("Matches", "ObjectProperty", probe, classesCore, 32u, importedActor);
    const auto cursor = classes.Property("Cursor", "ObjectProperty", probe, classesCore, 1u, importedActor);
    const auto pawnCursor = classes.Property("PawnCursor", "ObjectProperty", probe, classesCore, 1u, importedPawn);
    const auto innerCursor = classes.Property("InnerCursor", "ObjectProperty", probe, classesCore, 1u, importedActor);
    const auto matchCount = classes.Property("MatchCount", "IntProperty", probe, classesCore);
    const auto innerCount = classes.Property("InnerCount", "IntProperty", probe, classesCore);
    const auto importedLevel = classes.Import("Level", importedActor, "ObjectProperty"), importedLevelInfo = classes.Import("LevelInfo", classesEngine);
    const auto importedSpawnNotify = classes.Import("SpawnNotify", importedLevelInfo, "ObjectProperty");
    const auto importedLocation = classes.Import("Location", importedActor, "StructProperty"), importedRotation = classes.Import("Rotation", importedActor, "StructProperty");
    const auto begun = classes.Import("bBegunPlay", importedLevelInfo, "BoolProperty");
    const auto importedPawnList = classes.Import("PawnList", importedLevelInfo, "ObjectProperty");
    const auto importedPawnShadow = classes.Import("Shadow", importedPawn, "ObjectProperty");
    const auto importedPawnRole = classes.Import("Role", importedActor, "ByteProperty");
    const auto pawnProbe = classes.Export("PawnListProbe", 0, importedPawn);
    const auto pawnFailure = classes.Export("PawnListFailure", 0, pawnProbe);
    const auto pawnShadowProbe = classes.Export("PawnListShadow", 0, probe);
    const auto pawnEvents = classes.Property("PawnEvents", "IntProperty", pawnProbe, classesCore);
    const auto observedPawn = classes.Property("ObservedPawn", "ObjectProperty", pawnShadowProbe, classesCore, 1u, importedPawn);
    const auto pawnEventValue = Ref(0x01u, pawnEvents);
    classes.Function("PreBeginPlay", pawnProbe, classesCore, Join({Native(529u),
        Assign(pawnEventValue, Native(146u, {pawnEventValue, Int(1)})), Return()}));
    Flow pawnDestroyed;
    pawnDestroyed.IfNot(Native(119u, {Ref(0x01u, importedPawnShadow), Token(0x2au)}), "remove");
    pawnDestroyed.Add(Context(Ref(0x01u, importedPawnShadow), Native(279u))); pawnDestroyed.Label("remove");
    pawnDestroyed.Add(Native(530u));
    pawnDestroyed.IfNot(Native(150u, {Join({Token(0x3au), Ref(0x01u, importedPawnRole)}), Int(4)}), "authority");
    pawnDestroyed.Add(Return()); pawnDestroyed.Label("authority");
    pawnDestroyed.Add(Assign(pawnEventValue, Native(146u, {pawnEventValue, Int(10)}))); pawnDestroyed.Add(Return());
    classes.Function("Destroyed", pawnProbe, classesCore, pawnDestroyed.Finish());
    classes.Function("Destroyed", pawnFailure, classesCore, Join({Native(530u), Native(999u), Return()}));
    classes.Function("DisablePawnDestroyed", pawnProbe, classesCore, Join({Native(118u, {Ref(0x21u, classes.Name("Destroyed"))}), Return()}));
    classes.Function("Destroyed", pawnShadowProbe, classesCore, Join({
        Assign(Ref(0x01u, observedPawn), Context(Ref(0x01u, importedLevel), Ref(0x01u, importedPawnList))), Return()}));
    for (const auto& [name, index] : std::vector<std::pair<std::string, std::uint16_t>>{
        {"PawnAddWrongReceiver", 529u}, {"PawnRemoveWrongReceiver", 530u}})
        classes.Function(name, probe, classesCore, Join({Native(index), Return()}));
    for (const auto& [name, index] : std::vector<std::pair<std::string, std::uint16_t>>{
        {"ContextPawnAdd", 529u}, {"ContextPawnRemove", 530u}}) {
        const auto function = classes.Function(name, probe, classesCore);
        const auto target = classes.Property("Target", "ObjectProperty", function, classesCore, 1u, importedPawn, 0x80u);
        classes.FunctionCode(function, Join({Context(Ref(0x00u, target), Native(index)), Return()}));
    }
    classes.Function("PawnSpawnThenFail", probe, classesCore, Join({
        Assign(Ref(0x01u, pawnCursor), Native(278u, {Ref(0x20u, importedPawn)})),
        Context(Ref(0x01u, pawnCursor), Native(529u)), Native(999u), Return()}));
    const auto prop = [&](const std::string& name) { return Ref(0x01u, properties.at(name)); };
    for (const auto& [name, index, defaults] : std::vector<std::tuple<std::string, std::uint16_t, bool>>{
        {"RegisterAI", 710u, false}, {"RegisterDefaultAI", 710u, true}, {"ClearReceiverAI", 711u, false},
        {"SendAI", 713u, false}, {"SendDefaultAI", 713u, true}, {"StartAI", 714u, false},
        {"StartDefaultAI", 714u, true}, {"EndAI", 715u, false}, {"ClearSenderAI", 716u, false}}) {
        const auto function = classes.Function(name, probe, classesCore);
        std::vector<Code> arguments{Ref(0x00u, classes.Property("EventName", "NameProperty", function, classesCore, 1u, 0, 0x80u))};
        if (index == 710u) {
            arguments.push_back(Ref(0x00u, classes.Property("Callback", "NameProperty", function, classesCore, 1u, 0, 0x80u)));
            if (!defaults) {
                arguments.push_back(Ref(0x00u, classes.Property("ScoreCallback", "NameProperty", function, classesCore, 1u, 0, 0x80u)));
                for (const auto& flag : {"Visibility", "Direction", "Cylinder", "LineOfSight"})
                    arguments.push_back(Ref(0x00u, classes.Property(flag, "BoolProperty", function, classesCore, 1u, 0, 0x80u)));
            }
        } else if (index == 713u || index == 714u || index == 715u) {
            arguments.push_back(Ref(0x00u, classes.Property("Channel", "IntProperty", function, classesCore, 1u, 0, 0x80u)));
            if (index != 715u && !defaults) for (const auto& field : {"Value", "Radius"})
                arguments.push_back(Ref(0x00u, classes.Property(field, "FloatProperty", function, classesCore, 1u, 0, 0x80u)));
        }
        auto call = Native(index); call.raw.pop_back(); --call.logical;
        for (const auto& argument : arguments) { Append(call.raw, argument.raw); call.logical += argument.logical; }
        classes.FunctionCode(function, Join({call, Token(0x16u), Return()}));
    }
    classes.Function("AIMutationThenFail", probe, classesCore, Join({
        Native(710u, {Ref(0x21u, classes.Name("RollbackEvent")), Ref(0x21u, classes.Name("Callback"))}),
        Native(714u, {Ref(0x21u, classes.Name("RollbackEvent")), Int(1)}), Native(999u), Return()}));
    classes.Function("AISpawnThenFail", probe, classesCore, Join({Assign(prop("NestedActor"), Native(278u, {Ref(0x20u, leaf)})),
        Context(prop("NestedActor"), Native(714u, {Ref(0x21u, classes.Name("RollbackBirth")), Int(0)})), Native(999u), Return()}));
    classes.Function("OverrideReflectedXLevel", probe, classesCore, Join({Assign(Ref(0x01u, importedXLevel), Token(0x2au)), Return()}));
    classes.Function("InitAIWrongReceiver", probe, classesCore, Join({Native(650u), Return()}));
    for (const auto& [name, index] : std::vector<std::pair<std::string, std::uint16_t>>{{"ContextStartAI", 714u}, {"ContextEndAI", 715u}}) {
        const auto function = classes.Function(name, probe, classesCore);
        const auto target = classes.Property("Target", "ObjectProperty", function, classesCore, 1u, importedActor, 0x80u);
        const auto event = classes.Property("EventName", "NameProperty", function, classesCore, 1u, 0, 0x80u);
        const auto channel = classes.Property("Channel", "IntProperty", function, classesCore, 1u, 0, 0x80u);
        classes.FunctionCode(function, Join({Context(Ref(0x00u, target), Native(index, {Ref(0x00u, event), Ref(0x00u, channel)})), Return()}));
    }
    const auto count = [&] { return Ref(0x01u, matchCount); };
    const auto output = [&] { return Ref(0x01u, cursor); };
    const auto increment = [](Code value) { return Assign(value, Native(146u, {value, Int(1)})); };
    const auto recordMatch = [&](Code value) {
        return Join({Assign(Join({Token(0x1au), count(), Ref(0x01u, matches)}), std::move(value)), increment(count())});
    };
    for (const auto& name : {"ScanActors", "ScanLocal", "ScanArray", "ScanStruct", "ScanContext", "ScanDeclared", "ScanEarlyReturn", "ScanTypedPawn", "ScanBadOutput", "ScanBadTag", "ScanTooManyArguments", "ScanThenFail"}) {
        const auto function = classes.Function(name, probe, classesCore);
        const auto cls = classes.Property("InputClass", "ClassProperty", function, classesCore, 1u, classesClass, 0x80u, classesObject);
        const auto tag = classes.Property("InputTag", "NameProperty", function, classesCore, 1u, 0, 0x90u);
        Code destination = output();
        if (std::string(name) == "ScanLocal") destination = Ref(0x00u, classes.Property("LocalActor", "ObjectProperty", function, classesCore, 1u, importedActor));
        else if (std::string(name) == "ScanTypedPawn") destination = Ref(0x01u, pawnCursor);
        else if (std::string(name) == "ScanArray") destination = Array(matches, 31);
        else if (std::string(name) == "ScanStruct") destination = StructMember(importedTarget, payload);
        else if (std::string(name) == "ScanContext") destination = Context(prop("Link"), output());
        else if (std::string(name) == "ScanBadOutput") destination = Token(0x17u);
        Code factory = std::string(name) == "ScanDeclared" ?
            Final(importedAllActors, {Ref(0x00u, cls), destination, Ref(0x00u, tag)}) :
            Native(304u, {Ref(0x00u, cls), destination, std::string(name) == "ScanBadTag" ? Int(7) : Ref(0x00u, tag)});
        if (std::string(name) == "ScanTooManyArguments") factory = Native(304u, {Ref(0x00u, cls), destination, Ref(0x00u, tag), Int(0)});
        Flow flow; flow.Add(Assign(count(), Int(0))); flow.Iterator(factory, "done"); flow.Add(recordMatch(destination));
        if (std::string(name) == "ScanEarlyReturn") flow.Add(Return(destination));
        flow.Add(Token(0x31u)); flow.Label("done"); flow.Add(Token(0x30u));
        if (std::string(name) == "ScanThenFail") flow.Add(Native(999u));
        flow.Add(Return(destination)); classes.FunctionCode(function, flow.Finish());
    }
    classes.Function("AllActorsOutsideIterator", probe, classesCore,
        Join({Native(304u, {Ref(0x20u, importedActor), output()}), Return()}));
    for (const auto& name : {"AppendDuringScan", "AppendForeverScan", "RemoveFutureDuringScan", "RemoveSelfDuringScan", "TagDuringScan", "NestedScan"}) {
        const auto function = classes.Function(name, probe, classesCore);
        const bool targetParameter = std::string(name) == "RemoveFutureDuringScan" || std::string(name) == "TagDuringScan";
        const auto target = targetParameter ? classes.Property("Target", "ObjectProperty", function, classesCore, 1u, importedActor, 0x80u) : 0;
        const auto tag = std::string(name) == "TagDuringScan" ? Ref(0x21u, classes.Name("Live")) : Token(0x0bu);
        Flow flow; flow.Add(Assign(count(), Int(0))); flow.Add(Assign(Ref(0x01u, innerCount), Int(0)));
        flow.Iterator(Native(304u, {Ref(0x20u, loopActor), output(), tag}), "done"); flow.Add(recordMatch(output()));
        if (std::string(name) == "AppendDuringScan") {
            flow.IfNot(Native(154u, {count(), Int(1)}), "skipBirth");
            flow.Add(Assign(prop("NestedActor"), Native(278u, {Ref(0x20u, loopBirth)}))); flow.Label("skipBirth");
        } else if (std::string(name) == "AppendForeverScan") {
            flow.Add(Assign(prop("NestedActor"), Native(278u, {Ref(0x20u, loopActor)})));
        } else if (std::string(name) == "RemoveFutureDuringScan") flow.Add(Context(Ref(0x00u, target), Native(279u)));
        else if (std::string(name) == "RemoveSelfDuringScan") flow.Add(Context(output(), Native(279u)));
        else if (std::string(name) == "TagDuringScan") flow.Add(Assign(Context(Ref(0x00u, target), Ref(0x01u, importedTag)), Ref(0x21u, classes.Name("lIvE"))));
        else {
            flow.Iterator(Native(304u, {Ref(0x20u, loopActor), Ref(0x01u, innerCursor)}), "innerDone");
            flow.Add(increment(Ref(0x01u, innerCount))); flow.Add(Token(0x31u)); flow.Label("innerDone"); flow.Add(Token(0x30u));
        }
        flow.Add(Token(0x31u)); flow.Label("done"); flow.Add(Token(0x30u)); flow.Add(Return()); classes.FunctionCode(function, flow.Finish());
    }
    for (const auto& [cls, event, baseClass] : std::vector<std::tuple<std::int32_t, std::string, std::int32_t>>{
        {loopBirth, "PostBeginPlay", loopActor}, {loopDelete, "Destroyed", loopDelete}}) {
        Flow flow; flow.Add(Assign(count(), Int(0))); flow.Iterator(Native(304u, {Ref(0x20u, baseClass), output()}), "done");
        flow.Add(recordMatch(output())); flow.Add(Token(0x31u)); flow.Label("done"); flow.Add(Token(0x30u)); flow.Add(Return());
        classes.Function(event, cls, classesCore, flow.Finish());
    }
    classes.Function("FindPlayer", probe, classesCore, Return(Native(720u)));
    classes.Function("FindPlayerWithArgument", probe, classesCore, Return(Native(720u, {Int(0)})));
    const auto failBoundPlayer = classes.Function("MakeBoundPlayerThenFail", probe, classesCore);
    const auto failPlayerInput = classes.Property("Input", "ObjectProperty", failBoundPlayer, classesCore, 1u, importedPlayer, 0x80u);
    const auto importedPlayerField = classes.Import("Player", importedPlayerPawn, "ObjectProperty");
    classes.FunctionCode(failBoundPlayer, Join({Assign(prop("Link"), Native(278u, {Ref(0x20u, importedPlayerPawn)})),
        Assign(Context(prop("Link"), Ref(0x01u, importedPlayerField)), Ref(0x00u, failPlayerInput)),
        Assign(prop("Link"), Native(720u)), Native(999u), Return()}));
    const auto callbackTrace = [&](int digit) { return Assign(prop("Trace"), Native(146u, {Native(144u, {prop("Trace"), Int(10)}), Int(digit)})); };
    std::map<std::string, std::int32_t> callbackTags;
    for (const auto& [name, digit] : std::vector<std::pair<std::string, int>>{{"Spawned", 1}, {"PreBeginPlay", 2}, {"BeginPlay", 3}, {"PostBeginPlay", 4}, {"SetInitialState", 5}, {"PostPostBeginPlay", 6}}) {
        const auto observed = classes.Property("Observed" + name + "Tag", "NameProperty", probe, classesCore);
        callbackTags.emplace(name, observed);
        classes.Function(name, probe, classesCore, Join({callbackTrace(digit),
            Assign(Ref(0x01u, observed), Ref(0x01u, importedTag)), Return()}));
    }
    classes.Function("PostPostBeginPlay", tagMutator, classesCore, Join({callbackTrace(6),
        Assign(Ref(0x01u, callbackTags.at("PostPostBeginPlay")), Ref(0x01u, importedTag)),
        Assign(Ref(0x01u, importedTag), Ref(0x21u, classes.Name("CallbackTag"))), Return()}));
    const auto gained = classes.Function("GainedChild", probe, classesCore); classes.Property("Other", "ObjectProperty", gained, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(gained, Join({Assign(prop("ChildEvents"), Native(146u, {prop("ChildEvents"), Int(1)})), Return()}));
    const auto observedInstigator = classes.Property("ObservedChildInstigator", "ObjectProperty", captureOwner, classesCore, 1u, importedPawn);
    const auto observedFlags = classes.Property("ObservedChildFlags", "IntProperty", captureOwner, classesCore);
    const auto observedState = classes.Property("ObservedChildState", "NameProperty", captureOwner, classesCore);
    const auto observedChildTag = classes.Property("ObservedChildTag", "NameProperty", captureOwner, classesCore);
    const auto captureGained = classes.Function("GainedChild", captureOwner, classesCore);
    const auto captureChild = classes.Property("Other", "ObjectProperty", captureGained, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(captureGained, Join({
        Assign(prop("ChildEvents"), Native(146u, {prop("ChildEvents"), Int(1)})),
        Assign(Ref(0x01u, observedInstigator), Context(Ref(0x00u, captureChild), Ref(0x01u, importedInstigator))),
        Assign(Ref(0x01u, observedFlags), Context(Ref(0x00u, captureChild), Ref(0x01u, importedObjectFlags))),
        Assign(Ref(0x01u, observedState), Context(Ref(0x00u, captureChild), Native(284u))),
        Assign(Ref(0x01u, observedChildTag), Context(Ref(0x00u, captureChild), Ref(0x01u, importedTag))),
        Assign(Context(prop("Link"), Ref(0x01u, importedTag)), Ref(0x21u, classes.Name("CallbackChangedInput"))),
        Assign(Context(prop("Link"), Ref(0x01u, importedInstigator)), Token(0x2au)), Return()}));
    const auto make = classes.Function("Make", probe, classesCore), inputClass = classes.Property("InputClass", "ClassProperty", make, classesCore, 1u, classesClass, 0x80u, importedActor);
    classes.FunctionCode(make, Return(Native(278u, {Ref(0x00u, inputClass)})));
    const auto makeOwned = classes.Function("MakeOwned", probe, classesCore), ownClass = classes.Property("InputClass", "ClassProperty", makeOwned, classesCore, 1u, classesClass, 0x80u, importedActor), own = classes.Property("InputOwner", "ObjectProperty", makeOwned, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(makeOwned, Return(Native(278u, {Ref(0x00u, ownClass), Ref(0x00u, own)})));
    const auto makeAt = classes.Function("MakeAt", probe, classesCore), atClass = classes.Property("InputClass", "ClassProperty", makeAt, classesCore, 1u, classesClass, 0x80u, importedActor);
    const auto atOwner = classes.Property("InputOwner", "ObjectProperty", makeAt, classesCore, 1u, importedActor, 0x80u), atTag = classes.Property("InputTag", "NameProperty", makeAt, classesCore, 1u, 0, 0x80u);
    const auto atLocation = classes.Property("InputLocation", "StructProperty", makeAt, classesCore, 1u, classesVector, 0x80u), atRotation = classes.Property("InputRotation", "StructProperty", makeAt, classesCore, 1u, classesRotator, 0x80u);
    classes.FunctionCode(makeAt, Return(Native(278u, {Ref(0x00u, atClass), Ref(0x00u, atOwner), Ref(0x00u, atTag), Ref(0x00u, atLocation), Ref(0x00u, atRotation)})));
    const auto makeReferenceTag = classes.Function("MakeReferenceTag", probe, classesCore);
    const auto referenceOwner = classes.Property("InputOwner", "ObjectProperty", makeReferenceTag, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(makeReferenceTag, Join({Assign(Ref(0x01u, importedTag), Ref(0x21u, classes.Name("CapturedInput"))),
        Return(Native(278u, {Ref(0x20u, leaf), Ref(0x00u, referenceOwner), Ref(0x01u, importedTag)}))}));
    const auto mutateTagLocation = classes.Function("MutateTagLocation", probe, classesCore);
    classes.Property("ReturnValue", "StructProperty", mutateTagLocation, classesCore, 1u, classesVector, 0x480u);
    classes.FunctionCode(mutateTagLocation, Join({
        Assign(Ref(0x01u, importedTag), Ref(0x21u, classes.Name("LaterArgumentChangedTag"))),
        Return(Ref(0x01u, importedLocation))}));
    classes.Function("MakeLaterArgumentTag", probe, classesCore, Join({
        Assign(Ref(0x01u, importedTag), Ref(0x21u, classes.Name("EarlierArgumentTag"))),
        Return(Native(278u, {Ref(0x20u, leaf), Token(0x2au), Ref(0x01u, importedTag),
            Final(mutateTagLocation)}))}));
    const auto makeTaggedFail = classes.Function("MakeTaggedThenFail", probe, classesCore);
    const auto failedTag = classes.Property("InputTag", "NameProperty", makeTaggedFail, classesCore, 1u, 0, 0x80u);
    classes.FunctionCode(makeTaggedFail, Join({Assign(prop("Link"), Native(278u, {Ref(0x20u, leaf),
        Token(0x2au), Ref(0x00u, failedTag)})), Native(999u), Return()}));
    const auto setNotify = classes.Function("SetSpawnNotify", probe, classesCore);
    const auto notifyInput = classes.Property("Input", "ObjectProperty", setNotify, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(setNotify, Join({Assign(Context(Ref(0x01u, importedLevel), Ref(0x01u, importedSpawnNotify)),
        Ref(0x00u, notifyInput)), Return()}));
    const auto notifyReplacement = classes.Property("Replacement", "ObjectProperty", tagNotify, classesCore, 1u, importedActor);
    const auto notifyObservedTag = classes.Property("ObservedNativeTag", "NameProperty", tagNotify, classesCore);
    const auto notifyObservedActor = classes.Property("ObservedSpawned", "ObjectProperty", tagNotify, classesCore, 1u, importedActor);
    const auto notifyClass = classes.Import("ActorClass", classes.Import("SpawnNotify", classesEngine), "ClassProperty");
    const auto configureNotify = classes.Function("ConfigureNotify", tagNotify, classesCore);
    const auto replacementInput = classes.Property("Input", "ObjectProperty", configureNotify, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(configureNotify, Join({Assign(Ref(0x01u, notifyReplacement), Ref(0x00u, replacementInput)),
        Assign(Ref(0x01u, notifyClass), Ref(0x20u, leaf)), Return()}));
    const auto notification = classes.Function("SpawnNotification", tagNotify, classesCore);
    const auto notifiedActor = classes.Property("Actor", "ObjectProperty", notification, classesCore, 1u, importedActor, 0x80u);
    classes.Property("ReturnValue", "ObjectProperty", notification, classesCore, 1u, importedActor, 0x480u);
    classes.FunctionCode(notification, Join({
        Assign(Ref(0x01u, notifyObservedTag), Context(Ref(0x00u, notifiedActor), Ref(0x01u, importedTag))),
        Assign(Ref(0x01u, notifyObservedActor), Ref(0x00u, notifiedActor)),
        Return(Ref(0x01u, notifyReplacement))}));
    const auto omitted = classes.Function("MakeOmitted", probe, classesCore), omittedClass = classes.Property("InputClass", "ClassProperty", omitted, classesCore, 1u, classesClass, 0x80u, importedActor);
    classes.FunctionCode(omitted, Return(Native(278u, {Ref(0x00u, omittedClass), Token(0x0bu), Token(0x0bu), Token(0x0bu), Token(0x0bu)})));
    const auto makeFail = classes.Function("MakeThenFail", probe, classesCore), failureClass = classes.Property("InputClass", "ClassProperty", makeFail, classesCore, 1u, classesClass, 0x80u, importedActor);
    classes.FunctionCode(makeFail, Join({Assign(prop("Link"), Native(278u, {Ref(0x00u, failureClass)})), Native(999u), Return()}));
    classes.Function("ChangeDefaults", probe, classesCore, Join({Assign(Ref(0x02u, properties.at("Counter")), Int(99)), Assign(Array(slots, 1, true), Int(77)), Assign(StructMember(importedCount, payload, true), Int(11)), Assign(StructMember(importedTarget, payload, true), Token(0x17u)), Return()}));
    classes.Function("ChangeOwnValues", probe, classesCore, Join({Assign(prop("Counter"), Int(123)), Assign(Array(slots, 1), Int(88)), Assign(StructMember(importedCount, payload), Int(44)), Return()}));
    const auto link = classes.Function("KeepReference", probe, classesCore), linkInput = classes.Property("Input", "ObjectProperty", link, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(link, Join({Assign(prop("Link"), Ref(0x00u, linkInput)), Return(Context(prop("Link"), prop("Counter")))}));
    for (const auto& name : {"StoreReference", "StoreArrayReference", "StoreStructReference"}) {
        const auto function = classes.Function(name, probe, classesCore);
        const auto input = classes.Property("Input", "ObjectProperty", function, classesCore, 1u, importedActor, 0x80u);
        const auto destination = std::string(name) == "StoreReference" ? prop("Link") :
            std::string(name) == "StoreArrayReference" ? Array(matches, 31) : StructMember(importedTarget, payload);
        classes.FunctionCode(function, Join({Assign(destination, Ref(0x00u, input)), Return(destination)}));
    }
    const auto setBegun = classes.Function("SetBegun", probe, classesCore), begunInput = classes.Property("Input", "BoolProperty", setBegun, classesCore, 1u, 0, 0x80u);
    classes.FunctionCode(setBegun, Join({Assign(Context(Ref(0x01u, importedLevel), Ref(0x01u, begun)), Ref(0x00u, begunInput)), Return()}));
    classes.Function("Die", probe, classesCore, Return(Native(279u))); classes.Function("ReadCounter", probe, classesCore, Return(prop("Counter")));
    classes.Function("PostBeginPlay", fail, classesCore, Join({callbackTrace(4), Native(999u), Return()}));
    classes.Function("PostBeginPlay", nested, classesCore, Join({callbackTrace(4), Assign(prop("NestedActor"), Native(278u, {Ref(0x20u, leaf), Token(0x17u)})), Return()}));
    classes.Function("PostBeginPlay", recursive, classesCore, Join({Assign(prop("NestedActor"), Native(278u, {Ref(0x20u, recursive)})), Return()}));
    classes.Function("BeginPlay", selfDelete, classesCore, Join({callbackTrace(3), Native(279u), Return()}));
    for (const auto& name : {"MoveDirect", "MoveDirectThenFail"}) {
        const auto function = classes.Function(name, probe, classesCore);
        const auto locationInput = classes.Property("Input", "StructProperty", function, classesCore, 1u, classesVector, 0x80u);
        const auto assignment = Assign(Ref(0x01u, importedLocation), Ref(0x00u, locationInput));
        classes.FunctionCode(function, std::string(name) == "MoveDirect" ? Join({assignment, Return()}) : Join({assignment, Native(999u), Return()}));
    }
    classes.Function("ResizeThenFail", probe, classesCore, Join({Native(283u, {Int(1), Int(1)}), Native(999u), Return()}));
    classes.Function("DisableThenFail", probe, classesCore, Join({Native(262u, {Token(0x28u)}), Native(999u), Return()}));
    classes.Function("DestroyThenFail", probe, classesCore, Join({Native(279u), Native(999u), Return()}));
    const auto pointBegun = classes.Property("BegunEvents", "IntProperty", pointBase, classesCore), pointChanges = classes.Property("BaseEvents", "IntProperty", pointBase, classesCore);
    classes.Function("BeginPlay", pointBase, classesCore, Join({Assign(Ref(0x01u, pointBegun), Native(146u, {Ref(0x01u, pointBegun), Int(1)})), Return()}));
    classes.Function("BaseChange", pointBase, classesCore, Join({Assign(Ref(0x01u, pointChanges), Native(146u, {Ref(0x01u, pointChanges), Int(1)})), Return()}));
    classes.Function("BeginPlay", attachTaggedBirth, classesCore, Join({
        Assign(Ref(0x01u, importedAttachTag), Ref(0x21u, classes.Name("AttachTarget"))),
        Assign(Context(Ref(0x01u, importedOwner), Ref(0x01u, importedTag)), Ref(0x21u, classes.Name("AttachTarget"))), Return()}));
    classes.Function("Destroyed", support, classesCore, Join({Assign(prop("NestedActor"), Native(278u, {Ref(0x20u, pointBase), Token(0x17u), Token(0x0bu), Ref(0x01u, importedLocation), Ref(0x01u, importedRotation)})), Return()}));
    Bytes defaults; classes.IntTag(defaults, "Counter", 10); for (std::uint8_t i = 0u; i < 3u; ++i) classes.IntTag(defaults, "Slots", 3 + 2 * i, i);
    Bytes recordDefault; U32(recordDefault, 7u); Index(recordDefault, 0); classes.Tag(defaults, "Payload", 10u, recordDefault, 0u, "Record"); classes.ClassBody(probe, defaults);
    Bytes overrides; classes.IntTag(overrides, "Counter", 20); classes.ClassBody(leaf, overrides); classes.ClassBody(abstract, {}, 1u);
    for (auto cls : {fail, nested, recursive, selfDelete, captureOwner, tagNotify}) classes.ClassBody(cls);
    Bytes authoredTag; classes.NameTag(authoredTag, "Tag", "AuthoredTag"); classes.ClassBody(tagMutator, authoredTag);
    Bytes pointDefaults; classes.FloatTag(pointDefaults, "CollisionRadius", 0.0f); classes.FloatTag(pointDefaults, "CollisionHeight", 0.0f);
    classes.BoolTag(pointDefaults, "bCollideWorld", true); classes.BoolTag(pointDefaults, "bCollideActors", false); classes.ClassBody(pointBase, pointDefaults);
    classes.ClassBody(support);
    classes.ClassBody(attachTaggedBirth);
    for (auto cls : {fauxPlayer, pawnAlias, loopActor, loopBirth, loopDelete, pawnProbe, pawnFailure, pawnShadowProbe}) classes.ClassBody(cls);
    // World diagnostics execute real generated callbacks in one production VM
    // transaction. Reaching the terminal native gap proves these assertions
    // observed the required prefix values before the outer rollback.
    const auto worldGame = classes.Export("WorldGame", 0, classes.Import("GameInfo", classesEngine));
    const auto alternateWorldGame = classes.Export("AlternateWorldGame", 0, worldGame);
    const auto failingWorldGame = classes.Export("FailingWorldGame", 0, worldGame);
    const auto blockActorsWorldGame = classes.Export("BlockActorsWorldGame", 0, worldGame);
    const auto blockPlayersWorldGame = classes.Export("BlockPlayersWorldGame", 0, worldGame);
    const auto collideActorsWorldGame = classes.Export("CollideActorsWorldGame", 0, worldGame);
    const auto worldProbe = classes.Export("WorldProbe", 0, probe);
    const auto worldPassive = classes.Export("WorldPassive", 0, importedActor);
    const auto worldBirth = classes.Export("WorldBirth", 0, probe);
    const auto restoredFixture = classes.Property("RestoredFixture", "BoolProperty", worldProbe, classesCore);
    const auto touchingWitness = classes.Property("TouchingWitness", "ObjectProperty", worldProbe, classesCore, 1u, importedActor);
    const auto startup = classes.Import("bStartup", importedLevelInfo, "BoolProperty");
    const auto touching = classes.Import("Touching", importedActor, "ObjectProperty");
    const auto physics = classes.Import("Physics", importedActor, "ByteProperty");
    const auto game = classes.Import("Game", importedLevelInfo, "ObjectProperty");
    const auto defaultGame = classes.Import("DefaultGameType", importedLevelInfo, "ClassProperty");
    const auto importedGameInfo = classes.Import("GameInfo", classesEngine);
    const auto baseProperty = classes.Import("Base", importedActor, "ObjectProperty");
    const auto region = classes.Import("Region", importedActor, "StructProperty");
    const auto importedPointRegion = classes.Import("PointRegion", importedActor, "Struct");
    const auto regionLeaf = classes.Import("iLeaf", importedPointRegion, "IntProperty");
    const auto levelValue = [&](std::int32_t property) { return Context(Ref(0x01u, importedLevel), Ref(0x01u, property)); };
    const auto assertCode = [&](Flow& flow, Code condition, const std::string& label) {
        flow.IfNot(Native(129u, {condition}), label); flow.Add(Native(999u)); flow.Label(label);
    };
    if (loginVariant) {
        const auto loginPawn = classes.Export("LoginPawn", 0, importedPlayerPawn);
        classes.ClassBody(loginPawn);
        const auto seenClass = classes.Property("LoginClass", "ClassProperty", worldGame, classesCore,
            1u, classesClass, 0u, importedPlayerPawn);
        const auto loginDraw = classes.Property("LoginDraw", "IntProperty", worldGame, classesCore);
        const auto login = classes.Function("Login", worldGame, classesCore, {}, 0u, 0x802u);
        const auto portal = classes.Property("Portal", "StrProperty", login, classesCore, 1u, 0, 0x400080u);
        const auto options = classes.Property("Options", "StrProperty", login, classesCore, 1u, 0, 0x400080u);
        const auto error = classes.Property("Error", "StrProperty", login, classesCore, 1u, 0, 0x400180u);
        const auto selected = classes.Property("SpawnClass", "ClassProperty", login, classesCore,
            1u, classesClass, 0x80u, loginVariant == 3u ? importedActor : importedPlayerPawn);
        classes.Property("ReturnValue", "ObjectProperty", login, classesCore, 1u, importedPlayerPawn, 0x580u);
        Flow flow;
        assertCode(flow, levelValue(begun), "login-real-begun-world");
        assertCode(flow, Native(129u, {levelValue(startup)}), "login-startup-cleared");
        flow.Add(Assign(Ref(0x01u, seenClass), Ref(0x00u, selected)));
        flow.Add(Assign(Ref(0x01u, loginDraw), Native(167u, {Int(1000)})));
        flow.Add(Assign(Ref(0x00u, error), Text("small")));
        // Original EX_StringConst appFromAnsi truncates each literal to1023
        // WCHARs. Build the long OUT via genuine concatenation of short
        // literals, rather than assuming a4096-byte literal survives intact.
        flow.Add(Assign(Ref(0x00u, error), Text(std::string(512u, 'L'))));
        for(unsigned piece=1u;piece<8u;++piece)
            flow.Add(Assign(Ref(0x00u, error), Native(112u, {Ref(0x00u, error),Text(std::string(512u, 'L'))})));
        flow.Add(Assign(Ref(0x00u, error), Native(112u, {Ref(0x00u, error), Ref(0x00u, portal)})));
        flow.Add(Assign(Ref(0x00u, error), Native(112u, {Ref(0x00u, error), Ref(0x00u, options)})));
        if (loginVariant == 2u) flow.Add(Native(999u));
        flow.Add(Return(Token(0x2au))); // Actual scripted PlayerPawn None, not a fabricated player.
        classes.FunctionCode(login, flow.Finish(), 0u, 0x802u);
    }
    for (const auto& [name, field, type, target] : std::vector<std::tuple<std::string, std::int32_t, std::string, std::int32_t>>{
        {"SetWorldGameType", defaultGame, "ClassProperty", importedGameInfo},
        {"SetWorldGame", game, "ObjectProperty", importedGameInfo}}) {
        const auto function = classes.Function(name, probe, classesCore);
        const auto input = classes.Property("Input", type, function, classesCore, 1u,
            type == "ClassProperty" ? classesClass : target, 0x80u, type == "ClassProperty" ? target : 0);
        classes.FunctionCode(function, Join({Assign(levelValue(field), Ref(0x00u, input)), Return()}));
    }
    const auto birthPreCount = classes.Property("PreCount", "IntProperty", worldBirth, classesCore);
    const auto birthBeginCount = classes.Property("BeginCount", "IntProperty", worldBirth, classesCore);
    const auto birthPostCount = classes.Property("PostCount", "IntProperty", worldBirth, classesCore);
    const auto birthStateCount = classes.Property("StateCount", "IntProperty", worldBirth, classesCore);
    const auto birthPostLocation = classes.Property("PostLocation", "StructProperty", worldBirth, classesCore, 1u, classesVector);
    const auto birthPostRotation = classes.Property("PostRotation", "StructProperty", worldBirth, classesCore, 1u, classesRotator);
    const auto birthStateLocation = classes.Property("StateLocation", "StructProperty", worldBirth, classesCore, 1u, classesVector);
    const auto birthStateRotation = classes.Property("StateRotation", "StructProperty", worldBirth, classesCore, 1u, classesRotator);
    const auto birthPre = Ref(0x01u, birthPreCount), birthBegin = Ref(0x01u, birthBeginCount);
    classes.Function("PreBeginPlay", worldBirth, classesCore, Join({Assign(birthPre, Native(146u, {birthPre, Int(1)})), Return()}));
    classes.Function("BeginPlay", worldBirth, classesCore, Join({Assign(birthBegin, Native(146u, {birthBegin, Int(1)})), Return()}));
    // This birth occurs inside the genuine PublishStartup/InitGame path, not a
    // SetBegun fixture. Its PostBeginPlay replaces both initial Spawn arguments
    // with distinctive finite values before the native check. State/PPP retain
    // the observable poses on both nested-birth and later world-pass callbacks.
    classes.Function("PostBeginPlay",worldBirth,classesCore,Join({callbackTrace(4),
        Assign(Ref(0x01u,importedRotation),RotatorLiteral({-913,27123,157})),
        Assign(Ref(0x01u,importedLocation),VectorLiteral({17.25f,-31.5f,77.0f})),
        Assign(Ref(0x01u,birthPostRotation),Ref(0x01u,importedRotation)),
        Assign(Ref(0x01u,birthPostLocation),Ref(0x01u,importedLocation)),
        Assign(Ref(0x01u,birthPostCount),Native(146u,{Ref(0x01u,birthPostCount),Int(1)})),Return()}));
    Flow birthState;
    assertCode(birthState,Native(154u,{Ref(0x01u,birthPostCount),Native(146u,{Ref(0x01u,birthStateCount),Int(1)})}),"post-before-state");
    birthState.Add(callbackTrace(5));
    birthState.Add(Assign(Ref(0x01u,birthStateRotation),Ref(0x01u,importedRotation)));
    birthState.Add(Assign(Ref(0x01u,birthStateLocation),Ref(0x01u,importedLocation)));
    birthState.Add(Assign(Ref(0x01u,birthStateCount),Native(146u,{Ref(0x01u,birthStateCount),Int(1)})));
    birthState.Add(Return());classes.Function("SetInitialState",worldBirth,classesCore,birthState.Finish());
    Flow birthFinal;
    birthFinal.IfNot(levelValue(startup), "world-final"); birthFinal.Add(Return()); birthFinal.Label("world-final");
    assertCode(birthFinal, Native(154u, {birthPre, Int(2)}), "pre-twice");
    assertCode(birthFinal, Native(154u, {birthBegin, Int(2)}), "begin-twice");
    assertCode(birthFinal,Native(154u,{Ref(0x01u,birthPostCount),Int(2)}),"post-twice");
    assertCode(birthFinal,Native(154u,{Ref(0x01u,birthStateCount),Int(2)}),"state-twice");
    for(const auto& [name,value]:std::vector<std::pair<const char*,float>>{{"X",17.25f},{"Y",-31.5f},{"Z",77.0f}}) {
        const auto field=classes.Import(name,classesVector,"FloatProperty");
        for(const auto property:{importedLocation,birthPostLocation,birthStateLocation})
            assertCode(birthFinal,Native(180u,{StructMember(field,property),Number(value)}),
                std::string("current-post-state-location-")+name+'-'+std::to_string(property));
    }
    for(const auto& [name,value]:std::vector<std::pair<const char*,std::int32_t>>{{"Pitch",-913},{"Yaw",27123},{"Roll",157}}) {
        const auto field=classes.Import(name,classesRotator,"IntProperty");
        for(const auto property:{importedRotation,birthPostRotation,birthStateRotation})
            assertCode(birthFinal,Native(154u,{StructMember(field,property),Int(value)}),
                std::string("current-post-state-rotation-")+name+'-'+std::to_string(property));
    }
    birthFinal.Add(Return()); classes.Function("PostPostBeginPlay", worldBirth, classesCore, birthFinal.Finish());
    for (const auto cls : {worldGame, failingWorldGame}) {
        const auto init = classes.Function("InitGame", cls, classesCore);
        const auto options = classes.Property("Options", "StrProperty", init, classesCore, 1u, 0, 0x80u);
        const auto error = classes.Property("Error", "StrProperty", init, classesCore, 1u, 0, 0x180u);
        Flow flow;
        assertCode(flow, Native(122u, {Ref(0x00u, options), Text("fixture-options")}), "options");
        assertCode(flow, levelValue(begun), "begun"); assertCode(flow, levelValue(startup), "startup");
        assertCode(flow, Native(114u, {Ref(0x01u, importedInstigator), Token(0x2au)}), "null-instigator");
        assertCode(flow, Native(154u, {Native(156u, {Ref(0x01u, importedObjectFlags), Int(0x02000000)}), Int(0x02000000)}), "frame");
        flow.Add(Native(278u, {Ref(0x20u, worldBirth)}));
        flow.Add(Assign(Ref(0x00u, error), Text("generated nonempty out error")));
        if (cls == failingWorldGame) flow.Add(Native(999u));
        flow.Add(Return()); classes.FunctionCode(init, flow.Finish());
    }
    classes.Function("Spawned", worldGame, classesCore, Join({Native(999u), Return()}));
    Flow driverPre;
    assertCode(driverPre, levelValue(startup), "startup"); assertCode(driverPre, levelValue(begun), "begun");
    assertCode(driverPre, Native(154u, {Native(156u, {Ref(0x01u, importedObjectFlags), Int(0x02000000)}), Int(0x02000000)}), "frame");
    for (std::int32_t slot = 0; slot < 4; ++slot)
        assertCode(driverPre, Native(114u, {Array(touching, slot), Token(0x2au)}), "touch-" + std::to_string(slot));
    // The same declared Engine.Actor slots on an already-empty actor must
    // remain readable as None without requiring reset overlays.
    for (std::int32_t slot = 0; slot < 4; ++slot)
        assertCode(driverPre, Native(114u, {Context(prop("NestedActor"), Array(touching, slot)), Token(0x2au)}),
            "empty-touch-" + std::to_string(slot));
    driverPre.IfNot(Native(114u, {Ref(0x01u, touchingWitness), Token(0x2au)}), "has-touching-witness");
    driverPre.Add(Token(0x06u)); driverPre.Target("touching-witness-done"); driverPre.Label("has-touching-witness");
    for (std::int32_t slot = 0; slot < 4; ++slot)
        assertCode(driverPre, Native(114u, {Context(Ref(0x01u, touchingWitness), Array(touching, slot)), Token(0x2au)}),
            "frozen-touch-" + std::to_string(slot));
    driverPre.Label("touching-witness-done");
    driverPre.Add(Context(prop("Link"), Native(279u))); // Delete future slot before its PreBeginPlay.
    driverPre.Add(Native(278u, {Ref(0x20u, worldBirth)})); // Append during live outer pass.
    driverPre.Add(Assign(prop("Counter"), Int(1))); driverPre.Add(Return());
    classes.Function("PreBeginPlay", worldProbe, classesCore, driverPre.Finish());
    const auto setTouchingWitness = classes.Function("SetTouchingWitness", worldProbe, classesCore);
    const auto touchingWitnessInput = classes.Property("Input", "ObjectProperty", setTouchingWitness, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(setTouchingWitness, Join({Assign(Ref(0x01u, touchingWitness), Ref(0x00u, touchingWitnessInput)), Return()}));
    Flow driverBegin;
    assertCode(driverBegin, Native(154u, {prop("Counter"), Int(1)}), "phase-order");
    driverBegin.Add(Assign(prop("Counter"), Int(2)));
    driverBegin.Add(Assign(Ref(0x01u, importedAttachTag), Ref(0x21u, classes.Name("WorldTarget"))));
    driverBegin.Add(Return()); classes.Function("BeginPlay", worldProbe, classesCore, driverBegin.Finish());
    Flow driverPost;
    assertCode(driverPost, Native(154u, {prop("Counter"), Int(2)}), "phase-order");
    assertCode(driverPost, Native(154u, {Join({Token(0x3au), Ref(0x01u, physics)}), Int(0)}), "world-no-water-physics");
    driverPost.Add(Assign(prop("Counter"), Int(3))); driverPost.Add(Return());
    classes.Function("PostBeginPlay", worldProbe, classesCore, driverPost.Finish());
    Flow driverState;
    assertCode(driverState, Native(154u, {prop("Counter"), Int(3)}), "phase-order");
    driverState.Add(Assign(prop("Counter"), Int(4))); driverState.Add(Return());
    classes.Function("SetInitialState", worldProbe, classesCore, driverState.Finish());
    Flow driverFinal;
    assertCode(driverFinal, Native(129u, {levelValue(startup)}), "startup-cleared");
    driverFinal.IfNot(Ref(0x01u,restoredFixture),"fresh-world"); driverFinal.Add(Return()); driverFinal.Label("fresh-world");
    assertCode(driverFinal, Native(154u, {prop("Counter"), Int(4)}), "phase-order");
    assertCode(driverFinal, Native(114u, {Ref(0x01u, baseProperty), prop("NestedActor")}), "first-match-world-base");
    driverFinal.Add(Return()); classes.Function("PostPostBeginPlay", worldProbe, classesCore, driverFinal.Finish());
    classes.Function("PrepareRestoredWorld", worldProbe, classesCore, Join({Assign(Ref(0x01u,restoredFixture),Token(0x27u)),Return()}));
    classes.Function("BaseChange", worldProbe, classesCore, Join({Native(999u), Return()}));
    classes.Function("PreBeginPlay", worldPassive, classesCore, Join({Native(999u), Return()}));
    // Region lookup is read-only, independently checkable after rolled-back probes.
    classes.Function("ReadRegionLeaf", probe, classesCore, Return(Join({Ref(0x36u, regionLeaf), Ref(0x01u, region)})));
    for (auto cls : {worldGame, alternateWorldGame, failingWorldGame, worldProbe, worldPassive, worldBirth}) classes.ClassBody(cls);
    // Original CheckEncroachment's early-return mask is 0x0d, not just
    // bCollideActors and not bCollideWorld. Isolate each actual tested bit.
    for (const auto& [cls, flag] : std::vector<std::pair<std::int32_t,const char*>>{
        {blockActorsWorldGame,"bBlockActors"}, {blockPlayersWorldGame,"bBlockPlayers"},
        {collideActorsWorldGame,"bCollideActors"}}) {
        Bytes collisionDefaults;
        for (const auto* field : {"bCollideActors","bBlockActors","bBlockPlayers"})
            classes.BoolTag(collisionDefaults,field,std::string(field)==flag);
        classes.ClassBody(cls,std::move(collisionDefaults));
    }
    std::int32_t decalProbeClass{}, decalOwnerClass{};
    if (decalMover) {
        const auto importedDecal = classes.Import("Decal", classesEngine);
        const auto importedTexture = classes.Import("Texture", classesEngine);
        const auto importedMover = classes.Import("Mover", classesEngine);
        const auto textureProperty = classes.Import("Texture", importedActor, "ObjectProperty");
        const auto drawScaleProperty = classes.Import("DrawScale", importedActor, "FloatProperty");
        const auto multiProperty = classes.Import("MultiDecalLevel", importedDecal, "IntProperty");
        const auto surfListProperty = classes.Import("SurfList", importedDecal, "ArrayProperty");
        const auto detachFunction = classes.Import("DetachDecal", importedDecal, "Function");
        decalProbeClass = classes.Export("DecalProbe", 0, importedDecal);
        decalOwnerClass = classes.Export("DecalOwner", 0, importedActor);
        const auto mode = classes.Property("CallbackMode", "IntProperty", decalOwnerClass, classesCore);
        const auto trace = classes.Property("CallbackTrace", "IntProperty", decalOwnerClass, classesCore);
        const auto seenMulti = classes.Property("ObservedMulti", "IntProperty", decalOwnerClass, classesCore);
        const auto seenRandom = classes.Property("ObservedRandom", "IntProperty", decalOwnerClass, classesCore);
        const auto seenSurfaces = classes.IntArrayProperty("ObservedSurfList", decalOwnerClass, classesCore, 0u);
        const auto replacement = classes.Property("ReplacementTexture", "ObjectProperty", decalOwnerClass, classesCore, 1u, importedTexture);
        const auto surfaceMover = classes.Property("SurfaceMover", "ObjectProperty", decalOwnerClass, classesCore, 1u, importedMover);
        const auto setMode = classes.Function("SetCallbackMode", decalOwnerClass, classesCore);
        const auto modeInput = classes.Property("Input", "IntProperty", setMode, classesCore, 1u, 0, 0x80u);
        classes.FunctionCode(setMode, Join({Assign(Ref(0x01u, mode), Ref(0x00u, modeInput)), Return()}));
        const auto onAttach = classes.Function("Attach", decalOwnerClass, classesCore);
        const auto attached = classes.Property("Other", "ObjectProperty", onAttach, classesCore, 1u, importedActor, 0x80u);
        const auto receiver = [&]() { return Ref(0x00u, attached); };
        const auto receiverField = [&](std::int32_t field) { return Context(receiver(), Ref(0x01u, field)); };
        Flow callback;
        callback.Add(Assign(Ref(0x01u, trace), Int(1)));
        callback.Add(Assign(Ref(0x01u, seenSurfaces), receiverField(surfListProperty)));
        callback.Add(Assign(Ref(0x01u, seenMulti), receiverField(multiProperty)));
        callback.Add(Assign(Ref(0x01u, seenRandom), Native(167u, {Int(1000)})));
        callback.Add(Assign(receiverField(drawScaleProperty), Number(2.0f)));
        callback.Add(Assign(receiverField(multiProperty), Int(5)));
        callback.Add(Assign(receiverField(importedLocation), VectorLiteral({1000.0f, 1000.0f, 100.0f})));
        callback.Add(Assign(receiverField(importedRotation), RotatorLiteral({0, 0, 0})));
        callback.Add(Assign(Context(Ref(0x01u, surfaceMover), Ref(0x01u, importedRotation)), RotatorLiteral({9, 10, 11})));
        callback.IfNot(Native(154u, {Ref(0x01u, mode), Int(2)}), "nonnull-texture");
        callback.Add(Assign(receiverField(textureProperty), Token(0x17u)));
        callback.Add(Return()); callback.Label("nonnull-texture");
        callback.Add(Assign(receiverField(textureProperty), Ref(0x01u, replacement)));
        callback.IfNot(Native(154u, {Ref(0x01u, mode), Int(3)}), "keep-primary");
        callback.Add(Context(receiver(), Final(detachFunction))); callback.Label("keep-primary");
        callback.IfNot(Native(154u, {Ref(0x01u, mode), Int(4)}), "callback-success");
        callback.Add(Native(999u)); callback.Label("callback-success"); callback.Add(Return());
        classes.FunctionCode(onAttach, callback.Finish());
        const auto baseTrace = classes.Property("BaseChangeTrace", "IntProperty", decalProbeClass, classesCore);
        const auto baseMulti = classes.Property("BaseChangeMulti", "IntProperty", decalProbeClass, classesCore);
        const auto baseSurfaces = classes.IntArrayProperty("BaseChangeSurfList", decalProbeClass, classesCore, 0u);
        classes.Function("BaseChange", decalProbeClass, classesCore, Join({
            Assign(Ref(0x01u, baseTrace), Context(Ref(0x01u, importedOwner), Ref(0x01u, trace))),
            Assign(Ref(0x01u, baseMulti), Ref(0x01u, multiProperty)),
            Assign(Ref(0x01u, baseSurfaces), Ref(0x01u, surfListProperty)), Return()}));
        Bytes ownerDefaults; classes.IntTag(ownerDefaults, "CallbackMode", 1);
        classes.ClassBody(decalOwnerClass, std::move(ownerDefaults)); classes.ClassBody(decalProbeClass);
    }
    if(contextVariant) {
        for(const auto& [name,type]:std::vector<std::pair<std::string,std::string>>{
            {"ContextValidImportedObject","Object"},{"ContextWrongImportedObject","Actor"}}) {
            const auto target=classes.Import("ContextSourceOnly",classesCore,type);
            const auto function=classes.Function(name,probe,classesCore,Return(Ref(0x20u,target)));
            classes.Property("ReturnValue","ObjectProperty",function,classesCore,1u,classesObject,0x580u);
        }
    }
    const auto classesExcluded=contextVariant ? classes.Export("ContextSourceOnly",classesObject) : 0;
    if(contextVariant)classes.ActorBody(classesExcluded,{});
    contextFlags(classes);
    if(contextVariant)classes.exports.at(static_cast<std::size_t>(classesExcluded-1)).ObjFlags=static_cast<ObjectFlags>(0x00340000u);
    const auto classesTable = fixture.Write(classes);
    Require(GetPortableObjectPath(coreTable, recordCount) == "Object.Record.Count" && GetPortableObjectPath(coreTable, recordTarget) == "Object.Record.Target", "Generated frozen-struct metadata identity is wrong");

    Package map; map.stem = "SpawnFixture"; const auto mapEngine = map.ImportPackage("Engine"), mapClasses = map.ImportPackage("SpawnClasses");
    const auto world = map.Export("MyLevel", map.Import("Level", mapEngine)), mapModel = map.Export("Model0", map.Import("Model", mapEngine));
    const auto misleadingLevel = worldModel ? map.Export("MisleadingLevel", map.Import("LevelInfo", mapEngine)) : 0;
    const auto zoneActor = worldModel ? map.Export("RegionZone", map.Import("ZoneInfo", mapEngine)) : 0;
    const auto levelActor = map.Export("Level0", map.Import("LevelInfo", mapEngine)), driver = map.Export("Driver", map.Import(worldModel ? "WorldProbe" : "Probe", mapClasses));
    const auto owner = map.Export("Owner", map.Import(decalMover ? "DecalOwner" : worldModel ? "WorldPassive" : "Support", mapClasses)), instigator = map.Export("Pawn0", map.Import("Pawn", mapEngine));
    const auto collision = map.Export("Leaf0", map.Import("Probe", mapClasses)); // Deliberate authored birth-name collision.
    const auto playerObject = map.Export("BindingPlayer", map.Import("Player", mapEngine));
    const auto viewportObject = map.Export("BindingViewport", map.Import("Viewport", mapEngine));
    const auto contextActor=contextVariant ? map.Export("OrdinaryLookingOne",map.Import("Actor",mapEngine)) : 0;
    const auto contextBrush=contextVariant ? map.Export("OrdinaryLookingTwo",map.Import("Brush",mapEngine)) : 0;
    const auto contextView=contextVariant ? map.Export("OrdinaryLookingThree",map.Import("Camera",mapEngine)) : 0;
    const auto contextPawn=contextVariant ? map.Export("AdmittedPlayer",map.Import("PlayerPawn",mapEngine)) : 0;
    const auto decalActor = decalSchema ? map.Export("Decal0", map.Import(decalMover ? "DecalProbe" : "Decal", decalMover ? mapClasses : mapEngine)) : 0;
    const auto surfaceMoverActor = decalMover ? map.Export("SurfaceMover", map.Import("Mover", mapEngine)) : 0;
    std::int32_t replacementTexture{};
    if (decalSchema) {
        const auto textureObject = map.Export("Texture0", map.Import("Texture", mapEngine));
        Bytes textureProperties;
        if (decalSchema == 6u) map.FloatTag(textureProperties, "USize", 32.0f);
        else map.IntTag(textureProperties, "USize", 32);
        map.ActorBody(textureObject, std::move(textureProperties));
        replacementTexture = map.Export("Texture1", map.Import("Texture", mapEngine));
        Bytes replacementProperties; map.IntTag(replacementProperties, "USize", 64);
        map.ActorBody(replacementTexture, std::move(replacementProperties));
        Bytes properties; map.ObjectTag(properties, "Level", levelActor); map.ObjectTag(properties, "XLevel", world);
        map.FloatTag(properties, "LastRenderedTime", 27.5f); map.FloatTag(properties, "DrawScale", 1.0f);
        if (decalFloor) {
            map.VectorTag(properties, "Location", {0.0f, 0.0f, 0.1f});
            map.RotatorTag(properties, "Rotation", {16384, 0, 0});
            if (decalMover) map.ObjectTag(properties, "Owner", owner);
        }
        map.IntTag(properties, "MultiDecalLevel", 9); map.ActorBody(decalActor, std::move(properties));
    }
    if (decalMover) {
        Bytes properties; map.ObjectTag(properties, "Level", levelActor); map.ObjectTag(properties, "XLevel", world);
        map.RotatorTag(properties, "Rotation", {128, 512, -256});
        map.ActorBody(surfaceMoverActor, std::move(properties));
    }
    map.ActorBody(playerObject, {}); map.ActorBody(viewportObject, {}); // Real UObjects, deliberately not Level actors.
    const auto detachedActor = map.Export("DetachedActor", map.Import("Probe", mapClasses));
    map.ActorBody(detachedActor, {}); // Real Actor class, deliberately absent from serialized Level slots.
    const auto detachedPawn = map.Export("DetachedPawn", map.Import("Pawn", mapEngine));
    map.ActorBody(detachedPawn, {}); // Reflected Pawn identity has no native XLevel/slot membership.
    if (worldModel) { map.ActorBody(misleadingLevel, {}); map.ActorBody(zoneActor, {}); }
    for (const auto reference : {levelActor, driver, owner, instigator, collision}) {
        Bytes properties; map.ObjectTag(properties, "Level", levelActor); map.ObjectTag(properties, "XLevel", world);
        if (reference == levelActor) {
            // Startup-probe variants deliberately precede BeginPlay. The mover
            // attachment variant instead exercises live Attach/BaseChange
            // callbacks, which original enum-event dispatch gates on begun play.
            map.BoolTag(properties, "bBegunPlay", !worldModel || decalMover);
            if (worldModel) { map.BoolTag(properties, "bWaterZone", true); map.FloatTag(properties, "TimeSeconds", 123.0f); }
        }
        if (reference == driver) {
            map.VectorTag(properties, "Location", worldModel ? std::array<float,3>{0.0f,0.0f,40.0f} : std::array<float,3>{12.0f,-25.0f,40.0f});
            map.RotatorTag(properties, "Rotation", {128, 16384, -512}); map.BoolTag(properties, "bTicked", true); map.ObjectTag(properties, "Instigator", instigator);
            if (worldModel) {
                map.ObjectTag(properties, "Link", contextVariant ? contextActor : owner);
                map.ObjectTag(properties, "NestedActor", contextVariant ? contextPawn : instigator);
                for (std::uint8_t slot=0u;slot<4u;++slot) { Bytes value; Index(value,owner); map.Tag(properties,"Touching",5u,value,slot); }
            }
            if(contextVariant) {
                Bytes recordValue;U32(recordValue,29u);Index(recordValue,contextBrush);
                map.Tag(properties,"Payload",10u,recordValue,0u,"Record");
            }
        }
        if (worldModel && (reference==instigator || reference==collision)) map.NameTag(properties,"Tag","WorldTarget");
        if (decalMover && reference == owner) {
            map.ObjectTag(properties, "ReplacementTexture", replacementTexture);
            map.ObjectTag(properties, "SurfaceMover", surfaceMoverActor);
        }
        if (worldModel==4u && reference==instigator) map.BoolTag(properties,"bCollideWorld",true);
        map.ActorBody(reference, std::move(properties));
    }
    if(contextVariant) for(const auto reference:{contextActor,contextBrush,contextView,contextPawn}) {
        Bytes properties;map.ObjectTag(properties,"Level",levelActor);map.ObjectTag(properties,"XLevel",world);
        map.ActorBody(reference,std::move(properties));
    }
    const auto levelSlotCount = (decalSchema ? 8u : 7u) + (decalMover ? 1u : 0u) + (contextVariant ? 5u : 0u);
    Bytes levelBody; Index(levelBody, 0); U32(levelBody, levelSlotCount); U32(levelBody, levelSlotCount);
    for (const auto reference : {levelActor, driver, 0, owner, instigator, collision, 0}) Index(levelBody, reference);
    if (decalSchema) Index(levelBody, decalActor);
    if (decalMover) Index(levelBody, surfaceMoverActor);
    if(contextVariant)for(const auto reference:{contextActor,contextPawn,0,contextBrush,contextView})Index(levelBody,reference);
    for (unsigned i = 0u; i < 4u; ++i) Index(levelBody, 0);
    Index(levelBody, 0); U32(levelBody, 7777u); U32(levelBody, 0u); Index(levelBody, mapModel); Index(levelBody, 0);
    U32(levelBody,0u); Index(levelBody,0); // Original serialized Float time and actual +0x100 FirstDeleted.
    for(unsigned i=0u;i<16u;++i)Index(levelBody,0); // Separate +0x9C..+0xD8 pointer sequence; no +0x100 alias.
    Index(levelBody,0); // Original v68 serialized native travel-map count.
    map.Body(world, std::move(levelBody));
    Bytes modelBody; Index(modelBody, 0); for (unsigned i = 0u; i < 6u; ++i) FloatBytes(modelBody, 0.0f); modelBody.push_back(0u);
    for (unsigned i = 0u; i < 4u; ++i) FloatBytes(modelBody, 0.0f);
    const bool nodes = worldModel==2u || worldModel==3u;
    const auto zoneCount = worldModel==3u ? 0u : nodes ? 2u : 1u;
    if (decalFloor) {
        Index(modelBody, 3);
        for (const auto& value : std::array<std::array<float,3>,3>{{{0,0,1}, {1,0,0}, {0,1,0}}})
            for (const auto component : value) FloatBytes(modelBody, component);
        Index(modelBody, 5);
        for (const auto& value : std::array<std::array<float,3>,5>{{{0,0,0}, {-256,-256,0},
            {256,-256,0}, {256,256,0}, {-256,256,0}}})
            for (const auto component : value) FloatBytes(modelBody, component);
        Index(modelBody, 1); // One solid plane and its real finite authored polygon.
        FloatBytes(modelBody, 0); FloatBytes(modelBody, 0); FloatBytes(modelBody, 1); FloatBytes(modelBody, 0);
        U64(modelBody, 0); modelBody.push_back(0);
        Index(modelBody, 0); Index(modelBody, 0);
        for (unsigned i=0u;i<5u;++i) Index(modelBody, -1); // Back/front/plane, collision/render bounds.
        Index(modelBody, 0); Index(modelBody, 0); modelBody.push_back(4);
        U32(modelBody, 0xffffffffu); U32(modelBody, 0xffffffffu);
        Index(modelBody, 1); // Real FBspSurf; only mover mode has its AMover Actor.
        Index(modelBody, 0); U32(modelBody, 0); Index(modelBody, 0); Index(modelBody, 0);
        Index(modelBody, 1); Index(modelBody, 2); Index(modelBody, -1); Index(modelBody, -1);
        U16(modelBody, 0); U16(modelBody, 0); Index(modelBody, surfaceMoverActor);
        Index(modelBody, 4);
        for (std::int32_t i=1;i<=4;++i) { Index(modelBody, i); Index(modelBody, -1); }
        U32(modelBody, 0); U32(modelBody, 1); Index(modelBody, levelActor); U64(modelBody, 0); U64(modelBody, 0);
        for (unsigned i=0u;i<7u;++i) Index(modelBody, 0); // Polys, lightmaps, bits, bounds, hulls, leaves, lights.
    } else {
    Index(modelBody,0); Index(modelBody,0); // Vectors, points.
    Index(modelBody,nodes ? 2 : 0);
    if (nodes) for (unsigned index=0u;index<2u;++index) {
        FloatBytes(modelBody,index ? 0.0f : 1.0f); FloatBytes(modelBody,index ? 1.0f : 0.0f);
        FloatBytes(modelBody,0.0f); FloatBytes(modelBody,index ? 100.0f : 0.0f);
        U64(modelBody,0u); modelBody.push_back(0u);
        Index(modelBody,-1); Index(modelBody,-1); // Empty vertex pool, no surface.
        Index(modelBody,index ? -1 : 1); Index(modelBody,-1); Index(modelBody,-1); // Back child exists only for root; front missing.
        Index(modelBody,-1); Index(modelBody,-1); Index(modelBody,0); Index(modelBody,1);
        modelBody.push_back(0u); U32(modelBody,index ? 2u : 0u); U32(modelBody,index ? 3u : 1u);
    }
    Index(modelBody,0); Index(modelBody,0); // Surfaces, vertices.
    U32(modelBody,0u);
    U32(modelBody,zoneCount);
    for (unsigned index=0u;index<zoneCount;++index) { Index(modelBody,nodes ? index ? zoneActor : 0 : levelActor); U64(modelBody,0u); U64(modelBody,0u); }
    for (unsigned i=0u;i<5u;++i) Index(modelBody,0); // Polys, lightmaps, bits, bounds, hulls.
    Index(modelBody,nodes ? 4 : 0);
    if (nodes) for (unsigned index=0u;index<4u;++index) { Index(modelBody,static_cast<std::int32_t>(index&1u)); Index(modelBody,-1); Index(modelBody,-1); U64(modelBody,0u); }
    Index(modelBody,0); // Lights.
    }
    U32(modelBody, 1u); U32(modelBody, 0u); map.Body(mapModel, std::move(modelBody));
    contextFlags(map);
    if(contextVariant)for(const auto reference:{contextActor,contextBrush,contextView})
        map.exports.at(static_cast<std::size_t>(reference-1)).ObjFlags=static_cast<ObjectFlags>(0x00340000u);
    const auto mapTable = fixture.Write(map, true); const auto decodedModel = LoadPortableModel68(mapTable, static_cast<std::size_t>(mapModel - 1));
    Require(decodedModel.nodes.size()==(decalFloor ? 1u : nodes ? 2u : 0u) && decodedModel.zones.size()==zoneCount &&
        (nodes || decodedModel.zones[0].actorReference==levelActor), "Genuine generated map model is not decoded correctly");
    if (decalFloor) Require(decodedModel.surfaces.size()==1u && decodedModel.points.size()==5u &&
        decodedModel.vertices.size()==4u && decodedModel.nodes[0].surface==0 &&
        decodedModel.surfaces[0].brushActorReference==surfaceMoverActor,
        "Generated decal floor lost real polygon/surface/mover native records");
    auto expectedOrder = std::vector<std::int32_t>{levelActor, driver, 0, owner, instigator, collision, 0};
    if (decalSchema) expectedOrder.push_back(decalActor);
    if (decalMover) expectedOrder.push_back(surfaceMoverActor);
    if(contextVariant)for(const auto reference:{contextActor,contextPawn,0,contextBrush,contextView})expectedOrder.push_back(reference);
    Require(ReadPortableLevel68ActorOrder(mapTable) == expectedOrder, "Genuine Level actor ordering/holes were lost");
    Require(FindPortableRootModel68Export(mapTable) == static_cast<std::size_t>(mapModel - 1), "Genuine Level root model selection was lost");
    return {coreTable, engineTable, classesTable, mapTable, (decalSchema ? 6u : 5u) + (decalMover ? 1u : 0u) +
        (contextVariant ? 4u : 0u)};
}

void ArgumentTests(Fixture& fixture) {
    const auto baselinePaths = ActorPaths(); const auto revision = GetPortableRuntimeWorldRevision();
    Same(Call("Driver", "Make", {Class("")}).value, Value::Text(Kind::Object, {}), "Spawn(None) did not return a null Object");
    Same(Call("Driver", "Make", {Class("Abstract")}).value, Value::Text(Kind::Object, {}), "Spawn(abstract) did not return a null Object");
    Require(ActorPaths() == baselinePaths && GetPortableRuntimeWorldRevision() == revision && fixture.Snapshot("NullAbstract") == fixture.legacy, "Null/abstract Spawn changed world/counters/checkpoint");
    RefusedCall(fixture, "Make", {Class("Core.Object")}); RefusedCall(fixture, "Make", {Object("Driver")});
    RefusedCall(fixture, "Make", {{Value::Integer(3), {}}}); RefusedCall(fixture, "Make", {Class("Missing")});
    const auto first = Spawn(); Require(first != Path("Leaf0") && first.rfind("SpawnFixture.Leaf", 0u) == 0u, "Native birth identity collided with authored actor or lost class leaf");
    Require(GetPortableRuntimeWorldRevision() > revision && Published(first), "Successful Spawn was not committed/world-published");
    Same(Read(first, "Location"), Read("Driver", "Location"), "Omitted Spawn Location did not use spawner"); Same(Read(first, "OldLocation"), Read("Driver", "Location"), "Spawn OldLocation not initialized to spawn location");
    Same(Read(first, "Rotation"), Read("Driver", "Rotation"), "Omitted Spawn Rotation did not use spawner");
    Same(Read(first, "Owner"), Object("").value, "Omitted SpawnOwner did not mean None"); Same(Read(first, "Tag"), Name("None").value, "Omitted SpawnTag did not overwrite native Class.Name after return");
    Same(Read(first, "Level"), Object("Level0").value, "Spawn did not bind actual LevelInfo"); Same(Read(first, "XLevel"), Object("MyLevel").value, "Spawn did not bind actual ULevel");
    Same(Read(first, "Instigator"), Object("Pawn0").value, "Spawn did not copy spawner Instigator"); Same(Read(first, "bTicked"), Value::Bool(true), "Spawn did not oppose fresh native Level parity0");
    Same(Read(first, "Brush"), Object("").value, "Spawn did not clear Brush"); Same(Read(first, "Class"), Class("Leaf").value, "Spawn UObject.Class not initialized");
    Same(Read(first, "Name"), Name(first.substr(first.find_last_of('.') + 1u)).value, "Spawn UObject.Name not initialized from unique identity");
    Same(Read(first, "ObjectFlags"), Value::Integer(0x02004000), "Spawn UObject.ObjectFlags did not retain Transient and InitExecution HasStack");
    auto initialized = Decode(fixture.Snapshot("InitializedBirth")); InitializedFrame(initialized, first, "SpawnClasses.Leaf");
    Same(Read(first, "Outer"), Object("").value, "Spawn UObject.Outer did not copy ULevel.Outer");
    const auto region = Read(first, "Region"); Same(region.fields.at("zone"), Object("Level0").value, "Spawn Region fallback does not use real decoded LevelInfo zone");
    const auto omitted = Spawn("MakeOmitted"); Same(Read(omitted, "Location"), Read("Driver", "Location"), "Explicit omitted Location token became zero vector"); Same(Read(omitted, "Rotation"), Read("Driver", "Rotation"), "Explicit omitted Rotation token became zero rotator");
    const auto owned = Spawn("MakeOwned", {Class("Leaf"), Object("Owner")}); Same(Read(owned, "Owner"), Object("Owner").value, "SpawnOwner was ignored"); Require(Integer("Owner", "ChildEvents") == 1, "Owner did not receive synchronous GainedChild from birth");
    const auto at = Spawn("MakeAt", {Class("Leaf"), Object("Owner"), Name("CustomTag"), Vector({2.0f, 4.0f, 8.0f}), Rotation({-17, 32000, 255})});
    Same(Read(at, "Tag"), Name("CustomTag").value, "Explicit SpawnTag was not retained"); Same(Read(at, "Location"), Vector({2.0f, 4.0f, 8.0f}).value, "Explicit SpawnLocation was not retained"); Same(Read(at, "Rotation"), Rotation({-17, 32000, 255}).value, "Explicit SpawnRotation was not retained");
    const auto noneTag = Spawn("MakeAt", {Class("Leaf"), Object(""), Name("None"), Vector({0.0f, 0.0f, 0.0f}), Rotation({0, 0, 0})}); Same(Read(noneTag, "Tag"), Name("None").value, "Explicit None SpawnTag was replaced by native Class.Name");
    Same(Read(noneTag, "Location"), Vector({0.0f, 0.0f, 0.0f}).value, "Explicit zero location was incorrectly treated as omitted");
    RefusedCall(fixture, "MakeAt", {Class("Leaf"), Object("Owner"), Name("BadLocation"), Vector({std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}), Rotation({0, 0, 0})});
    fixture.Reset(); const auto resetBirth = Spawn(); Require(resetBirth == first, "Legacy reset did not restore deterministic name allocation/collision skipping"); fixture.Reset();
    Call("Driver","SetFixtureTicked",{{Value::Bool(false),{}}});
    Same(Read("Driver","bTicked"),Value::Bool(false),"Explicit generated spawner marker control was not written");
    const auto opposite=Spawn();
    Same(Read(opposite,"bTicked"),Value::Bool(true),"Newborn copied the deliberately false spawner marker instead of opposing Level0");
    const auto savedOpposite=fixture.Snapshot("NativeLevelParityBirth");
    fixture.Reset();WriteBytes(fixture.directory/"ParityReload.sav",savedOpposite);
    Require(LoadPortableRuntimeState((fixture.directory/"ParityReload.sav").string()),"Cannot restore native-parity birth checkpoint");
    Same(Read(opposite,"bTicked"),Value::Bool(true),"Checkpoint lost genuine newborn bTicked field");
    const auto following=Spawn();Same(Read(following,"bTicked"),Value::Bool(true),"Legacy/checkpoint load changed immutable native Level parity baseline");
    fixture.Reset();
}
void SpawnTagTests(Fixture& fixture) {
    const auto callbackTags = [&](const std::string& actor, const std::string& cls) {
        for (const auto* event : {"Spawned", "PreBeginPlay", "BeginPlay", "PostBeginPlay", "SetInitialState", "PostPostBeginPlay"})
            Same(Read(actor, std::string("Observed") + event + "Tag"), Name(cls).value,
                std::string(event) + " saw script SpawnTag instead of native Class.Name");
    };
    for (const auto& tag : {"CustomTag", "None", ""}) {
        const auto actor = Spawn("MakeAt", {Class("TagMutator"), Object(""), Name(tag),
            Vector({2.0f, 4.0f, 8.0f}), Rotation({0, 0, 0})});
        callbackTags(actor, "TagMutator");
        Same(Read(actor, "Tag"), Name(tag).value, "execSpawn failed to overwrite a callback-written Tag after native return");
    }
    const auto omitted = Spawn("MakeOmitted", {Class("TagMutator")});
    callbackTags(omitted, "TagMutator");
    Same(Read(omitted, "Tag"), Name("None").value, "Omitted SpawnTag token retained a callback-written Tag");
    const auto canonical = fixture.Snapshot("ScriptSpawnTagCanonical"); fixture.Reset();
    Require(LoadPortableRuntimeState((fixture.directory / "ScriptSpawnTagCanonical.sav").string()) &&
        fixture.Snapshot("ScriptSpawnTagRestored") == canonical, "Post-wrapper SpawnTag did not survive cold canonical restore");
    RefusedCall(fixture, "MakeTaggedThenFail", {Name("RollbackTag")}); fixture.Reset();

    // The third argument aliases Driver.Tag. SetOwner runs GainedChild, which
    // changes that property; the wrapper must use its detached entry capture.
    const auto capture = Spawn("Make", {Class("CaptureOwner")});
    Call(capture, "StoreReference", {Object("Driver")});
    const auto captured = Spawn("MakeReferenceTag", {Object(capture)});
    Same(Read("Driver", "Tag"), Name("CallbackChangedInput").value, "Tag alias regression did not execute owner mutation");
    Same(Read(capture, "ObservedChildTag"), Name("Leaf").value, "GainedChild did not see pre-wrapper native Class.Name");
    callbackTags(captured, "Leaf");
    Same(Read(captured, "Tag"), Name("CapturedInput").value, "execSpawn reloaded SpawnTag alias after callbacks");
    fixture.Reset();

    // Original execSpawn copies SpawnTag when its expression is evaluated,
    // before the later optional Location expression calls another function.
    const auto earlier = Spawn("MakeLaterArgumentTag", {});
    Same(Read("Driver", "Tag"), Name("LaterArgumentChangedTag").value, "Later native argument did not mutate the aliased Tag");
    Same(Read(earlier, "Tag"), Name("EarlierArgumentTag").value, "Spawn captured Tag only after a later argument mutated its source");
    callbackTags(earlier, "Leaf"); fixture.Reset();

    // SpawnNotify replaces the native return. Only the replacement receives
    // execSpawn's script Tag; the actual birth retains its native Class.Name.
    const auto replacement = Spawn("Make", {Class("Engine.GameInfo")});
    const auto notify = Spawn("Make", {Class("TagNotify")});
    Call(notify, "ConfigureNotify", {Object(replacement)});
    Call("Driver", "SetSpawnNotify", {Object(notify)});
    for (const auto& tag : {"ReplacementTag", "None"}) {
        const auto returned = Spawn("MakeAt", {Class("Leaf"), Object(""), Name(tag),
            Vector({2.0f, 4.0f, 8.0f}), Rotation({0, 0, 0})});
        Same(Object(returned).value, Object(replacement).value, "SpawnNotify did not replace native Spawn return");
        Same(Read(notify, "ObservedNativeTag"), Name("Leaf").value, "SpawnNotification observed script Tag before native return");
        const auto born = Read(notify, "ObservedSpawned").text;
        Require(!born.empty() && born != replacement && Published(born), "SpawnNotify lost the actual retained birth");
        callbackTags(born, "Leaf");
        Same(Read(born, "Tag"), Name("Leaf").value, "execSpawn wrote Tag on original birth instead of returned replacement");
        Same(Read(replacement, "Tag"), Name(tag).value, "SpawnNotify replacement missed post-return SpawnTag write");
    }
    RefusedCall(fixture, "MakeTaggedThenFail", {Name("RolledBackReplacementTag")});
    Same(Read(replacement, "Tag"), Name("None").value, "Failed outer transaction retained replacement Tag write");
    Call(notify, "ConfigureNotify", {Object("")});
    Same(Call("Driver", "Make", {Class("Leaf")}).value, Object("").value, "Null SpawnNotify replacement did not return None");
    const auto nullBorn = Read(notify, "ObservedSpawned").text;
    Require(!nullBorn.empty() && Published(nullBorn), "Null replacement destroyed its actual native birth");
    Same(Read(nullBorn, "Tag"), Name("Leaf").value, "Null replacement incorrectly applied script Tag to original birth");
    const auto replaced = fixture.Snapshot("SpawnTagReplacementCanonical"); fixture.Reset();
    Require(LoadPortableRuntimeState((fixture.directory / "SpawnTagReplacementCanonical.sav").string()) &&
        fixture.Snapshot("SpawnTagReplacementRestored") == replaced, "SpawnNotify replacement/actual birth tags failed canonical restore");
    fixture.Reset();
}
void DefaultsTests(Fixture& fixture) {
    const auto old = Spawn(); Require(Integer(old, "Counter") == 20, "Spawn did not freeze concrete inherited leaf CDO");
    Call(old, "ChangeDefaults"); Same(ReadPortableClassDefault("SpawnClasses.Leaf", "Counter"), Value::Integer(99), "Birth did not mutate its actual shared concrete CDO");
    Require(Integer(old, "Counter") == 20 && Integer("Driver", "Counter") == 10, "CDO write retroactively changed old birth/other class instance");
    const auto newer = Spawn(); Require(Integer(newer, "Counter") == 99 && Integer(old, "Counter") == 20, "Later Spawn did not use new CDO independent of old frozen baseline");
    Same(Read(old, "Slots", 1u), Value::Integer(5), "CDO array mutation retroactively changed old birth"); Same(Read(newer, "Slots", 1u), Value::Integer(77), "Later Spawn did not freeze mutable CDO array");
    Require(QuestVr::Vm::ToInt(Read(old, "Payload").fields.at("count")) == 7 && QuestVr::Vm::ToInt(Read(newer, "Payload").fields.at("count")) == 11, "Frozen nested struct defaults were shared/omitted");
    Same(Read(newer, "Payload").fields.at("target"), Object(old).value, "Frozen typed nested Object did not retain earlier birth identity");
    Call(newer, "ChangeOwnValues"); Require(Integer(old, "Counter") == 20 && Integer(newer, "Counter") == 123, "Birth property write changed another birth's frozen storage");
    Same(Read(old, "Slots", 1u), Value::Integer(5), "Birth fixed-array slot aliases another birth");
    auto detached = Read(old, "Payload"); detached.fields["count"] = Value::Integer(-900); Require(QuestVr::Vm::ToInt(Read(old, "Payload").fields.at("count")) == 7, "Birth getter aliases mutable frozen storage");
    const auto bytes = fixture.Snapshot("FrozenDefaults"); auto saved = Decode(bytes);
    Require(saved.births.size() == 2u && bytes[4u] == 12u && QuestVr::EncodeScriptSavedState(saved)[6u] == 9u, "Birth save did not select complete manifest and modern control/codec9/envelope12");
    InitializedFrame(saved, old, "SpawnClasses.Leaf"); InitializedFrame(saved, newer, "SpawnClasses.Leaf");
    Require(Birth(saved, old).frozenDefaults.empty(), "Unchanged birth copied redundant authored defaults instead of immutable baseline plus sparse patch");
    Same(Frozen(saved, newer, "Counter").value, Value::Integer(99), "Manifest omitted later mutable CDO baseline");
    Same(Frozen(saved, newer, "Payload").value.fields.at("target"), Object(old).value, "Manifest lost typed birth-to-birth nested reference");
    fixture.Reset(); Unavailable(old, "Legacy restore retained old birth VM identity"); Unavailable(newer, "Legacy restore retained newer birth VM identity");
    Require(LoadPortableRuntimeState((fixture.directory / "FrozenDefaults.sav").string()) && fixture.Snapshot("FrozenRestore") == bytes, "Frozen CDO/birth overlay manifest failed canonical restore");
    Require(Integer(old, "Counter") == 20 && Integer(newer, "Counter") == 123, "Manifest restore normalized frozen CDOs or omitted overlays"); fixture.Reset();
}
void CallbackTests(Fixture& fixture) {
    const auto leaf = Spawn(); Require(Integer(leaf, "Trace") == 123456, "Spawn callbacks were omitted/reordered or skipped PostPostBeginPlay");
    const auto nested = Spawn("Make", {Class("Nested")}); const auto nestedChild = Read(nested, "NestedActor").text;
    Require(!nestedChild.empty() && Published(nestedChild) && Integer(nestedChild, "Trace") == 123456 && Integer(nested, "Trace") == 123456, "Synchronous nested Spawn did not complete both actors' begun lifecycle");
    Same(Read(nestedChild, "Owner"), Object(nested).value, "Nested spawned actor lost outer callback Self owner");
    auto saved = Decode(fixture.Snapshot("NestedCallbacks")); Require(saved.births.size() == 3u, "Nested Spawn root transaction omitted a birth manifest");
    RefusedCall(fixture, "Make", {Class("FailPost")}); RefusedCall(fixture, "MakeThenFail", {Class("Nested")});
    QuestVr::Vm::Limits limits; limits.callDepth = 8u; RefusedCall(fixture, "Make", {Class("Recursive")}, limits);
    const auto afterFailures = Spawn();
    Require(afterFailures == Path("Leaf3"), "Rolled-back nested Spawn consumed committed class-name allocation counter");
    const auto beforeDeleteBirth = ActorPaths(true).size(); Same(Call("Driver", "Make", {Class("SelfDelete")}).value, Object("").value, "BeginPlay self-destruction did not make Spawn return None");
    saved = Decode(fixture.Snapshot("SelfDeletedBirth")); const auto self = std::find_if(saved.births.begin(), saved.births.end(), [](const auto& birth) { return birth.classPath == "SpawnClasses.SelfDelete"; });
    Require(self != saved.births.end() && !Published(self->path) && ActorPaths(true).size() == beforeDeleteBirth, "Self-deleted Spawn was forgotten or remained world published");
    Require(Integer(self->path, "Trace") == 123 && QuestVr::Vm::ToBool(Read(self->path, "bDeleteMe")), "Self-deleted birth lost exact completed callback phase or UObject lifetime");
    Same(Read(self->path, "Tag"), Name("SelfDelete").value, "Null self-deleted Spawn return received the script wrapper's omitted Tag");
    Same(Call(self->path, "ReadCounter").value, Value::Integer(10), "Direct function on removed spawned UObject was refused");
    fixture.Reset(); Call("Driver", "SetBegun", {{Value::Bool(false), {}}}); const auto dormant = Spawn(); Require(Integer(dormant, "Trace") == 0, "Before-begun Spawn ran BeginPlay callbacks");
    auto dormantState = Decode(fixture.Snapshot("BeforeBegunInitialized")); InitializedFrame(dormantState, dormant, "SpawnClasses.Leaf");
    Same(Read(dormant, "ObjectFlags"), Value::Integer(0x02004000), "Before-begun Spawn skipped native InitExecution HasStack");
    const auto dormantBytes = fixture.Snapshot("BeforeBegunCanonical");
    fixture.Reset(); Require(LoadPortableRuntimeState((fixture.directory / "BeforeBegunCanonical.sav").string()) &&
        fixture.Snapshot("BeforeBegunRestored") == dormantBytes, "Before-begun initialized class/null-Code/null-Locals frame did not restore canonically");
    dormantState = Decode(fixture.Snapshot("BeforeBegunFrameRestored")); InitializedFrame(dormantState, dormant, "SpawnClasses.Leaf"); fixture.Reset();

    // execSpawn captures the spawner's Instigator before SetOwner callbacks;
    // Level::SpawnActor publishes it only after the synchronous GainedChild.
    const auto capturingOwner = Spawn("Make", {Class("CaptureOwner")});
    Call(capturingOwner, "StoreReference", {Object("Driver")});
    Same(Read("Driver", "Instigator"), Object("Pawn0").value, "Spawn capture regression did not start with a distinct Instigator");
    const auto capturedBirth = Spawn("MakeOwned", {Class("Leaf"), Object(capturingOwner)});
    Require(Integer(capturingOwner, "ChildEvents") == 1, "CaptureOwner did not execute its isolated GainedChild callback");
    Same(Read("Driver", "Instigator"), Object("").value, "GainedChild did not mutate the spawner Instigator for the capture regression");
    Same(Read(capturingOwner, "ObservedChildInstigator"), Object("").value, "Spawn published captured Instigator before GainedChild returned");
    Same(Read(capturingOwner, "ObservedChildFlags"), Value::Integer(0x4000), "Spawn initialized HasStack before SetOwner/GainedChild completed");
    Same(Read(capturingOwner, "ObservedChildState"), Name("None").value, "GainedChild observed a frame before native InitExecution");
    Same(Read(capturedBirth, "Instigator"), Object("Pawn0").value, "GainedChild spawner mutation replaced the entry-captured Spawn Instigator");
    auto capturedState = Decode(fixture.Snapshot("CapturedSpawnInstigator")); InitializedFrame(capturedState, capturedBirth, "SpawnClasses.Leaf");
    fixture.Reset();
}
void OrderingTests(Fixture& fixture) {
    std::vector<std::string> births;
    for (unsigned i = 0u; i < 12u; ++i) {
        births.push_back(Spawn());
        if (i == 3u || i == 8u) births.push_back(Spawn("Make", {Class("Probe")}));
    }
    const auto published = ActorPaths(true);
    Require(published.size() == 5u + births.size() &&
        std::equal(births.begin(), births.end(), published.end() - static_cast<std::ptrdiff_t>(births.size())),
        "Live actor publication lost actual birth order across interleaved classes/double-digit names");
    const auto bytes = fixture.Snapshot("OrderedBirths"); auto saved = Decode(bytes);
    Require(saved.births.size() == births.size(), "Birth manifest omitted ordered world actors");
    for (std::size_t i = 0u; i < births.size(); ++i)
        Require(Birth(saved, births[i]).worldActorIndex == 7u + i, "Manifest tail index ignored original serialized ULevel holes or birth order");
    Require(saved.births.front().path != births.front() || saved.births.back().path != births.back(),
        "Ordering fixture did not distinguish canonical manifest sorting from actual world birth order");
    fixture.Reset();
    Require(LoadPortableRuntimeState((fixture.directory / "OrderedBirths.sav").string()) && ActorPaths(true) == published && fixture.Snapshot("OrderedRestore") == bytes,
        "Sorted birth manifest changed actual world tail publication order on cold restore");
    const auto continued = Spawn(); Require(continued == Path("Leaf13"), "Manifest restore lost per-class allocator continuation or used lexicographic last name");
    auto bad = saved; Birth(bad, births.front()).worldActorIndex = 6u;
    RefusedSave(fixture, bytes, bad, "BirthIndexOverlapsOriginalLevelSlot");
    fixture.Reset();
}
void PersistenceTests(Fixture& fixture) {
    const auto first = Spawn("MakeOwned", {Class("Leaf"), Object("Owner")}), second = Spawn();
    Call("Driver", "KeepReference", {Object(first)}); Call(first, "KeepReference", {Object(second)}); Call(second, "KeepReference", {Object("Driver")});
    const auto canonical = fixture.Snapshot("BirthGraph"); auto saved = Decode(canonical); const auto revision = GetPortableRuntimeWorldRevision(); const auto beforePaths = ActorPaths(true);
    Require(ValidatePortableRuntimeState((fixture.directory / "BirthGraph.sav").string(), "SpawnFixture"), "Read-only manifest validation rejected typed staged graph");
    Require(GetPortableRuntimeWorldRevision() == revision && ActorPaths(true) == beforePaths && fixture.Snapshot("AfterGraphValidation") == canonical, "Read-only manifest validation mutated/published staged actors");
    GC::Collect(); Require(fixture.Snapshot("AfterBirthGC") == canonical, "Births/frozen/native/typed references were not rooted across GC");
    const auto extra = Spawn(); Require(Published(extra), "Replacement setup extra Spawn failed");
    Require(LoadPortableRuntimeState((fixture.directory / "BirthGraph.sav").string()) && fixture.Snapshot("BirthGraphReplacement") == canonical, "Manifest restore merged births or changed canonical graph");
    Unavailable(extra, "Manifest replacement retained absent extra birth identity"); Same(Read("Driver", "Link"), Object(first).value, "Manifest replacement lost original-to-birth reference"); Same(Read(first, "Link"), Object(second).value, "Manifest replacement lost birth-to-birth reference");
    fixture.Reset(); Require(ActorPaths(true).size() == 5u, "Legacy restore retained runtime birth actors"); Unavailable(first, "Legacy restore retained first birth index");
    Require(ValidatePortableRuntimeState((fixture.directory / "BirthGraph.sav").string(), "SpawnFixture"), "Cold manifest validation cannot stage unknown birth graph"); Require(ActorPaths(true).size() == 5u, "Cold manifest validation prematurely published births");
    Require(LoadPortableRuntimeState((fixture.directory / "BirthGraph.sav").string()) && fixture.Snapshot("ColdManifestRestore") == canonical, "Cold manifest restore failed canonical complete frozen graph");
    auto bad = saved; ChangeBirthClass(bad, first, "Core.Object"); RefusedSave(fixture, canonical, bad, "NonActorBirthClass");
    bad = saved; ChangeBirthClass(bad, first, "SpawnClasses.Abstract"); RefusedSave(fixture, canonical, bad, "AbstractBirthClass");
    bad = saved; Birth(bad, first).frozenDefaults.push_back({"SpawnClasses.Probe.Counter", "Counter", 0u, Value::Float(1.0f)}); RefusedSave(fixture, canonical, bad, "WrongFrozenPropertyKind");
    bad = saved; Birth(bad, first).frozenDefaults.push_back({"SpawnClasses.Probe.Slots", "Slots", 7u, Value::Integer(1)}); RefusedSave(fixture, canonical, bad, "WrongFrozenArrayBounds");
    bad = saved; auto malformed = Read(first, "Payload"); malformed.fields.erase("target");
    Birth(bad, first).frozenDefaults.push_back({"SpawnClasses.Probe.Payload", "Payload", 0u, malformed}); RefusedSave(fixture, canonical, bad, "IncompleteFrozenStruct");
    bad = saved; malformed = Read(first, "Payload"); malformed.fields["target"] = Class("Leaf").value;
    Birth(bad, first).frozenDefaults.push_back({"SpawnClasses.Probe.Payload", "Payload", 0u, malformed}); RefusedSave(fixture, canonical, bad, "WrongFrozenNestedObjectType");
    bad = saved; Birth(bad, first).path = "SpawnFixture.Leaf0"; for (auto& object : bad.objects) if (object.path == first) object.path = "SpawnFixture.Leaf0";
    RefusedSave(fixture, canonical, bad, "BirthAliasesAuthoredActor");
    bad = saved; Birth(bad, first).worldActorIndex += 10u; RefusedSave(fixture, canonical, bad, "NonContiguousBirthTailIndex");
    const auto beforeDie = fixture.Snapshot("BeforeBirthDie"); Require(QuestVr::Vm::ToBool(Call(first, "Die").value), "Spawned UObject Destroy failed"); Require(!Published(first), "Destroyed birth remains world-published");
    Same(Call(first, "ReadCounter").value, Value::Integer(20), "Destroyed birth no longer directly callable");
    const auto dead = fixture.Snapshot("DeadBirth"); Require(ValidatePortableRuntimeState((fixture.directory / "DeadBirth.sav").string(), "SpawnFixture"), "Dead birth manifest failed validation");
    fixture.Reset(); Require(LoadPortableRuntimeState((fixture.directory / "DeadBirth.sav").string()) && fixture.Snapshot("DeadBirthRestore") == dead, "Dead birth native removal/retained identity failed cold restore");
    Require(!Published(first) && Integer(first, "Counter") == 20 && beforeDie != dead, "Dead birth restore lost independent world/UObject lifetime"); fixture.Reset();
}
void CollisionIntegrationTests(Fixture& fixture) {
    const auto boolean = [](bool value) { return Evaluation{Value::Bool(value), {}}; };
    const auto missingFindBase = [&](const std::string& function,const std::vector<Evaluation>& arguments,
        const std::string& receiver="Driver") {
        const auto before=fixture.Snapshot("BeforeMissingFindBase");
        const auto paths=ActorPaths(true);const auto revision=GetPortableRuntimeWorldRevision();
        const auto beforeGc=GC::GetStats();
        const auto result=ExecutePortableActorFunction(Path(receiver),function,arguments);
        // This generated fixture has no prepared actual UObject lookup or
        // Level/Engine/Model collision service. A physical Spawn must refuse
        // instead of reviving the removed cached-overlap/pointer-sort base.
        Require(!result.passed()&&!result.committed&&result.status==QuestVr::Vm::Status::Unsupported&&
            result.error=="Retail collision actor requires actual object lookup authority"&&result.writes>0u,
            "Unprepared physical Spawn did not refuse its genuine FindBase provider boundary: "+result.error);
        const auto afterGc=GC::GetStats(); // Inspect before any save can collect.
        Require(afterGc.numObjects==beforeGc.numObjects&&afterGc.memoryUsage==beforeGc.memoryUsage,
            "Missing FindBase retained physical birth/callback allocation roots");
        Require(fixture.Snapshot("AfterMissingFindBase")==before&&ActorPaths(true)==paths&&
            GetPortableRuntimeWorldRevision()==revision,
            "Missing FindBase leaked callback writes, birth/name state, collision fields or revision");
        ++refusals;
    };
    const auto pointAt = [&](std::array<float, 3> location) {
        // The real generated Decoration has bCollideWorld=true, PHYS_None
        // and zero dimensions. Placement bypass is not FindBase authority.
        // Full original support selection is covered by actual first Training,
        // not by inferring a floor from this fixture's cached overlap boxes.
        missingFindBase("MakeAt",{Class("PointBase"),Object(""),Name("None"),Vector(location),Rotation({0,0,0})});
    };
    fixture.Reset();
    // Original SpawnActor has no AttachTag scan. An authored BeginPlay may
    // establish matching tags, but neither its owner nor this fresh actor is
    // attached by that separate LoadMap-only startup rule during a birth.
    const auto taggedBirth = Spawn("MakeAt", {Class("AttachTaggedBirth"), Object("Owner"),
        Name("None"), Vector({256.0f, 0.0f, 0.0f}), Rotation({0, 0, 0})});
    Same(Read(taggedBirth, "AttachTag"), Name("AttachTarget").value, "AttachTag birth callback did not really execute");
    Same(Read("Owner", "Tag"), Name("AttachTarget").value, "AttachTag birth callback did not publish its matching target tag");
    Same(Read(taggedBirth, "Base"), Object("").value, "Spawn invented LoadMap-only AttachTag binding on fresh actor");
    Same(Read("Owner", "Base"), Object("").value, "Spawn attached matching target to the fresh actor in the wrong direction");
    const auto taggedSave = fixture.Snapshot("AttachTagBirthIgnored");
    fixture.Reset();
    Require(LoadPortableRuntimeState((fixture.directory / "AttachTagBirthIgnored.sav").string()) &&
        fixture.Snapshot("AttachTagBirthIgnoredRestore") == taggedSave,
        "Ignored Spawn AttachTag state failed cold canonical restore");
    Same(Read("Owner", "Base"), Object("").value, "Restore synthesized omitted Spawn AttachTag binding");
    fixture.Reset();
    // Pin native283 does not clamp/reject negative reflected dimensions. A
    // disabled actor is not registered, so those fields also round-trip through
    // save/load without making the collision hash or world-placement test run.
    Same(Call("Owner", "SetCollisionSize", {{Value::Float(-3.5f), {}}, {Value::Float(-8.25f), {}}}).value,
        Value::Bool(true), "Disabled native283 invented a negative-size refusal");
    Same(Read("Owner", "CollisionRadius"), Value::Float(-3.5f), "Disabled native283 clamped negative radius");
    Same(Read("Owner", "CollisionHeight"), Value::Float(-8.25f), "Disabled native283 clamped negative height");
    const auto negative = fixture.Snapshot("DisabledNegativeCollision");
    Require(ValidatePortableRuntimeState((fixture.directory / "DisabledNegativeCollision.sav").string(), "SpawnFixture") &&
        LoadPortableRuntimeState((fixture.directory / "DisabledNegativeCollision.sav").string()) &&
        fixture.Snapshot("DisabledNegativeCollisionRestore") == negative,
        "Disabled negative dimensions failed typed save/fresh-registry restore");
    pointAt({0.0f, 0.0f, 0.0f});
    Same(Call("Owner", "SetCollisionSize", {{Value::Float(12.0f), {}}, {Value::Float(20.0f), {}}}).value,
        Value::Bool(true), "Cannot restore positive dimensions before collision enable");
    Call("Owner", "SetCollision", {boolean(true), boolean(true), boolean(false)});
    Require(QuestVr::Vm::ToBool(Read("Owner", "bCollideActors")) && QuestVr::Vm::ToBool(Read("Owner", "bBlockActors")) &&
        !QuestVr::Vm::ToBool(Read("Owner", "bBlockPlayers")), "Actual native262 did not publish all requested collision flags");
    pointAt({0.0f, 0.0f, 0.0f});
    Call("Owner", "SetCollision", {{Value{}, {}}, {Value{}, {}}, boolean(true)});
    Call("Owner", "SetCollision");
    Require(QuestVr::Vm::ToBool(Read("Owner", "bCollideActors")) && QuestVr::Vm::ToBool(Read("Owner", "bBlockActors")) &&
        QuestVr::Vm::ToBool(Read("Owner", "bBlockPlayers")), "Native262 omitted arguments did not preserve current flags");

    RefusedCall(fixture, "MoveDirectThenFail", {Vector({512.0f, 0.0f, 0.0f})}, {}, "Owner");
    pointAt({0.0f, 0.0f, 0.0f}); pointAt({512.0f, 0.0f, 0.0f});
    RefusedCall(fixture, "ResizeThenFail", {}, {}, "Owner");
    pointAt({8.0f, 0.0f, 0.0f});
    RefusedCall(fixture, "DisableThenFail", {}, {}, "Owner");
    pointAt({0.0f, 0.0f, 0.0f});
    RefusedCall(fixture, "DestroyThenFail", {}, {}, "Owner");
    pointAt({0.0f, 0.0f, 0.0f});
    Same(Read("Owner", "NestedActor"), Object("").value, "Rolled-back Destroy retained callback-spawned child reference");
    Require(!QuestVr::Vm::ToBool(Read("Owner", "bDeleteMe")), "Rolled-back Destroy retained deletion flag");

    Call("Owner", "MoveDirect", {Vector({512.0f, 0.0f, 0.0f})});
    pointAt({512.0f, 0.0f, 0.0f}); pointAt({0.0f, 0.0f, 0.0f});
    Same(Call("Owner", "SetCollisionSize", {{Value::Float(12.0f), {}}, {Value::Float(20.0f), {}}}).value,
        Value::Bool(true), "Native283 failed Remove/assign/Add success");
    pointAt({512.0f, 0.0f, 0.0f});
    Call("Owner", "SetCollision", {boolean(false)});
    Require(!QuestVr::Vm::ToBool(Read("Owner", "bCollideActors")) && QuestVr::Vm::ToBool(Read("Owner", "bBlockActors")) &&
        QuestVr::Vm::ToBool(Read("Owner", "bBlockPlayers")), "Native262 false/omitted flags were conflated");
    pointAt({512.0f, 0.0f, 0.0f}); Call("Owner", "SetCollision", {boolean(true)});
    pointAt({512.0f, 0.0f, 0.0f});
    // Support.Destroyed really spawns PointBase. The child's missing FindBase
    // must roll back the WHOLE enclosing Destroy, not commit a fake supported
    // child or leave its original support deleted. Nonphysical native Destroy
    // success/removal/save controls remain in the other lifecycle tests.
    missingFindBase("Die",{},"Owner");
    Require(!QuestVr::Vm::ToBool(Read("Owner","bDeleteMe"))&&Published("Owner"),
        "Nested missing FindBase retained the enclosing support deletion");
    Same(Read("Owner","NestedActor"),Object("").value,"Nested missing FindBase retained the callback child");
    pointAt({512.0f, 0.0f, 0.0f});
    const auto live = fixture.Snapshot("CollisionLiveSupportAfterRefusal"); fixture.Reset();
    Require(LoadPortableRuntimeState((fixture.directory / "CollisionLiveSupportAfterRefusal.sav").string()) &&
        fixture.Snapshot("CollisionLiveSupportAfterRefusalRestore") == live,
        "Live collision fields after nested refusal failed cold canonical restore");
    Require(!QuestVr::Vm::ToBool(Read("Owner","bDeleteMe"))&&Published("Owner"),
        "Cold restore invented a committed support deletion from its refused Destroy");
    pointAt({512.0f, 0.0f, 0.0f});

    fixture.Reset(); Call("Owner", "SetCollision", {boolean(true)});
    Call("Owner", "MoveDirect", {Vector({512.0f, 0.0f, 0.0f})}); pointAt({512.0f, 0.0f, 0.0f});
    const auto stale = fixture.Snapshot("StaleCollisionRegistration");
    // Cold/repeated loads must replace saved reflected collision fields and
    // runtime-born identities atomically. Their save equality does not prove
    // actual retail hash membership or turn unavailable FindBase into overlap.
    fixture.Reset();
    for (unsigned iteration = 0u; iteration < 3u; ++iteration) {
        Require(LoadPortableRuntimeState((fixture.directory / "StaleCollisionRegistration.sav").string()) &&
            fixture.Snapshot("FreshCollisionRegistrationRestore") == stale,
            "Fresh save registry normalization changed serialized script/birth state");
        pointAt({512.0f, 0.0f, 0.0f});
        Call("Owner", "SetCollision", {boolean(false)});
    }
    fixture.Reset(); pointAt({0.0f, 0.0f, 0.0f}); fixture.Reset();
}
void AliasAndBoundaryGcTests(Fixture& fixture) {
    const auto first = Spawn(), second = Spawn();
    Call("Driver", "KeepReference", {Object(first)}); Call(first, "KeepReference", {Object(second)});
    const auto canonical = fixture.Snapshot("BoundaryBirthGraph"); auto mixed = Decode(canonical);
    auto& manifest = Birth(mixed, first);
    manifest.path = "spawnfixture.lEAF1"; manifest.classPath = "spawnclasses.lEAF";
    auto& overlay = Record(mixed, first);
    overlay.path = "SPAWNfixture.LeAF1"; overlay.classPath = "SPAWNCLASSES.Leaf";
    const auto setProperty = [](QuestVr::ScriptSavedObject& object, const std::string& name, Value value) {
        const auto property = std::find_if(object.properties.begin(), object.properties.end(), [&](const auto& entry) {
            return Fold(entry.key) == Fold("Engine.Actor." + name) && entry.index == 0u;
        });
        if (property == object.properties.end()) object.properties.push_back({"Engine.Actor." + name, name, 0u, std::move(value)});
        else property->value = std::move(value);
    };
    setProperty(overlay, "Owner", Value::Text(Kind::Object, "SPAWNFIXTURE.lEAF2"));
    setProperty(overlay, "bAnimByOwner", Value::Bool(true));
    setProperty(Record(mixed, second), "Owner", Value::Text(Kind::Object, "spawnfixture.dRIVER"));
    setProperty(Record(mixed, second), "bAnimByOwner", Value::Bool(true));
    const auto aliasBytes = InactivePrefix(Envelope(canonical, mixed), "SpawnFixture.LEAF1");
    const auto aliasPath = fixture.directory / "MixedCaseInactiveBirth.sav"; WriteBytes(aliasPath, aliasBytes);
    const auto revision = GetPortableRuntimeWorldRevision(), priorCount = GC::GetStats().numObjects;
    Require(ValidatePortableRuntimeState(aliasPath.string(), "SpawnFixture"), "Valid mixed-case birth/overlay/gameplay identities failed read-only validation");
    Require(GetPortableRuntimeWorldRevision() == revision && GC::GetStats().numObjects == priorCount && fixture.Snapshot("AfterAliasValidation") == canonical,
        "Read-only case-alias validation allocated/published staged UObject identities");
    Require(LoadPortableRuntimeState(aliasPath.string()), "Valid mixed-case birth/overlay/gameplay identities failed load");
    Require(!HasPath(ActorPaths(false), first) && HasPath(ActorPaths(true), first) && HasPath(ActorPaths(false), second),
        "Case-variant gameplay inactive prefix did not resolve the actual staged birth identity");
    const auto snapshots = GetPortableRuntimeMapActors(true);
    const auto firstSnapshot = std::find_if(snapshots.begin(), snapshots.end(), [&](const auto& actor) { return Fold(actor.objectPath) == Fold(first); });
    const auto secondSnapshot = std::find_if(snapshots.begin(), snapshots.end(), [&](const auto& actor) { return Fold(actor.objectPath) == Fold(second); });
    Require(firstSnapshot != snapshots.end() && secondSnapshot != snapshots.end() && firstSnapshot->animByOwner && secondSnapshot->animByOwner,
        "Mixed-case Owner fixture lost actual born snapshots or typed bAnimByOwner properties");
    Require(firstSnapshot->ownerPath == "SPAWNFIXTURE.lEAF2" && firstSnapshot->animationSourcePath == secondSnapshot->objectPath &&
        secondSnapshot->ownerPath == "spawnfixture.dRIVER" && secondSnapshot->animationSourcePath == Path("Driver"),
        "Snapshot Owner lookup ignored UE case-insensitive identity or followed owner's own animation source instead of immediate owner");
    const auto savedAlias = fixture.Snapshot("AliasCanonical");
    auto aliasState = Decode(savedAlias);
    Require(aliasState.births.size() == 2u && Fold(aliasState.births.front().path).find("spawnfixture.") == 0u,
        "Saving mixed-case identities dropped staged birth manifests");
    Require(ValidatePortableRuntimeState((fixture.directory / "AliasCanonical.sav").string(), "SpawnFixture") &&
        LoadPortableRuntimeState((fixture.directory / "AliasCanonical.sav").string()) && fixture.Snapshot("AliasRoundTrip") == savedAlias,
        "Case-variant birth inactive state did not round-trip canonically");
    Require(!HasPath(ActorPaths(false), first) && HasPath(ActorPaths(true), first), "Case-variant inactive gameplay state was lost on resave/reload");

    Require(LoadPortableRuntimeState((fixture.directory / "BoundaryBirthGraph.sav").string()), "Cannot establish active boundary-GC fixture");
    const auto stable = GC::GetStats();
    // No explicit Collect occurs before/inside these assertions. Public runtime
    // boundaries must reclaim replaced or rolled-back staged actors themselves.
    for (unsigned iteration = 0u; iteration < 24u; ++iteration) {
        Require(LoadPortableRuntimeState((fixture.directory / "BoundaryBirthGraph.sav").string()), "Repeated successful modern birth replacement failed");
        const auto current = GC::GetStats();
        Require(current.numObjects == stable.numObjects && current.memoryUsage == stable.memoryUsage,
            "Successful modern birth replacement retained unrooted prior birth allocations at public boundary");
    }
    auto malformed = Decode(canonical);
    Record(malformed, first).properties.push_back({"SpawnClasses.Probe.Counter", "Counter", 0u, Value::Float(9.0f)});
    const auto malformedPath = fixture.directory / "LateStagedPropertyFailure.sav"; WriteBytes(malformedPath, Envelope(canonical, malformed));
    for (unsigned iteration = 0u; iteration < 12u; ++iteration) {
        const auto beforeRevision = GetPortableRuntimeWorldRevision();
        Require(!LoadPortableRuntimeState(malformedPath.string()), "Late invalid actor overlay after staged birth allocation was accepted");
        const auto current = GC::GetStats();
        Require(current.numObjects == stable.numObjects && current.memoryUsage == stable.memoryUsage && GetPortableRuntimeWorldRevision() == beforeRevision,
            "Failed staged modern birth load retained unrooted births or advanced world revision at public boundary");
    }
    for (unsigned iteration = 0u; iteration < 16u; ++iteration) {
        RefusedCall(fixture, "MakeThenFail", {Class("Nested")}); const auto current = GC::GetStats();
        Require(current.numObjects == stable.numObjects && current.memoryUsage == stable.memoryUsage,
            "Rolled-back nested Spawn retained unrooted actor allocations after VM aliases expired");
    }
    Require(fixture.Snapshot("BoundaryGcCanonical") == canonical, "Repeated load/Spawn refusals mutated committed birth graph");
    fixture.Reset();
}
void GetPlayerPawnTests(Fixture& fixture) {
    fixture.Reset();
    const auto none = Object("").value;
    const auto boolean = [](bool value) { return Evaluation{Value::Bool(value), {}}; };
    const auto lookup = [&](const std::string& expected) {
        const auto before = fixture.Snapshot("BeforePlayerLookup");
        const auto revision = GetPortableRuntimeWorldRevision();
        Same(Call("Driver", "FindPlayer").value, Object(expected).value, "Compiled native720 selected the wrong bound PlayerPawn");
        Same(Call("Driver", "GetPlayerPawn").value, Object(expected).value, "Direct native720 disagrees with compiled lookup");
        Require(GetPortableRuntimeWorldRevision() == revision && fixture.Snapshot("AfterPlayerLookup") == before,
            "Read-only native720 mutated the player/world/save timeline");
    };
    lookup("");
    RefusedCall(fixture, "FindPlayerWithArgument");
    RefusedCall(fixture, "GetPlayerPawn", {Object("BindingPlayer")});
    const auto faux = Spawn("Make", {Class("FauxPlayer")});
    Call(faux, "BindPlayer", {Object("BindingPlayer")});
    Require(Read(faux, "Player").text == Path("BindingPlayer"), "Faux Pawn lacks its real typed same-name Player field");
    lookup("");
    const auto ordinary = Spawn("Make", {Class("Engine.Pawn")});
    Require(Published(ordinary), "Ordinary Pawn fixture did not create a real Level slot");
    lookup("");
    const auto first = Spawn("Make", {Class("Engine.PlayerPawn")});
    Same(Read(first, "Player"), none, "New PlayerPawn fabricated a Player binding");
    lookup("");
    const auto second = Spawn("Make", {Class("Engine.PlayerPawn")});
    Call(second, "BindPlayer", {Object("BindingViewport")});
    Same(Read(second, "Player"), Object("BindingViewport").value, "Typed Player field rejected actual Viewport ancestry");
    lookup(second);
    Call(first, "BindPlayer", {Object("BindingPlayer")});
    Require(!QuestVr::Vm::ToBool(Read(first, "bIsPlayer")), "PlayerPawn fixture requires bIsPlayer=false to test exact lookup policy");
    lookup(first); // First actual Level slot, not most recently linked binding.
    RefusedCall(fixture, "BindPlayer", {Object("Driver")}, {}, first);
    lookup(first);
    Call(first, "SetPlayerFlags", {boolean(true), boolean(false)});
    Require(QuestVr::Vm::ToBool(Read(first, "bDeleteMe")) && Published(first), "Direct delete-flag fixture accidentally destroyed the Level slot");
    lookup(first); // The pin checks neither bDeleteMe nor bIsPlayer here.
    Call(first, "SetPlayerFlags", {boolean(false), boolean(true)});
    lookup(first);
    Call(first, "SetPlayerFlags", {boolean(false), boolean(false)});

    const auto active = fixture.Snapshot("BoundPlayerPawns");
    auto state = Decode(active);
    Require(Birth(state, first).worldActorIndex == 9u && Birth(state, second).worldActorIndex == 10u,
        "PlayerPawn test changed the original seven-slot authored baseline/order");
    const auto inactivePath = fixture.directory / "InactiveBoundPlayerPawn.sav";
    WriteBytes(inactivePath, InactivePrefix(active, first));
    Require(ValidatePortableRuntimeState(inactivePath.string(), "SpawnFixture") && LoadPortableRuntimeState(inactivePath.string()),
        "Inactive bound PlayerPawn prefix could not be restored");
    Require(!HasPath(ActorPaths(false), first) && HasPath(ActorPaths(true), first),
        "Inactive PlayerPawn fixture did not distinguish UI inactivity from Level membership");
    lookup(first); // Portable UI inactivity is not the engine's Player predicate.
    Require(LoadPortableRuntimeState((fixture.directory / "BoundPlayerPawns.sav").string()), "Cannot restore active player lookup fixture");
    lookup(first);
    const auto stable = GC::GetStats();
    RefusedCall(fixture, "MakeBoundPlayerThenFail", {Object("BindingPlayer")});
    Require(GC::GetStats().numObjects == stable.numObjects && GC::GetStats().memoryUsage == stable.memoryUsage,
        "Failed nested player binding retained a provisional PlayerPawn/root");
    lookup(first);

    Same(Call(first, "Destroy").value, Value::Bool(true), "Bound PlayerPawn Destroy failed");
    Require(!Published(first) && Read(first, "Player").text == Path("BindingPlayer"),
        "Destroy lost UObject binding or kept its world slot");
    lookup(second);
    const auto removed = fixture.Snapshot("RemovedBoundPlayerPawn");
    Call(second, "BindPlayer", {Object("")}); lookup("");
    fixture.Reset(); lookup("");
    Require(ValidatePortableRuntimeState((fixture.directory / "RemovedBoundPlayerPawn.sav").string(), "SpawnFixture"),
        "Cold PlayerPawn graph failed read-only typed preflight");
    lookup("");
    Require(LoadPortableRuntimeState((fixture.directory / "RemovedBoundPlayerPawn.sav").string()) &&
        fixture.Snapshot("ColdBoundPlayerPawnRestore") == removed,
        "Cold born PlayerPawn graph did not preserve binding/removal fields and Level order");
    Require(!Published(first) && Read(first, "Player").text == Path("BindingPlayer") &&
        Read(second, "Player").text == Path("BindingViewport"), "Cold player bindings lost native object/class identity");
    lookup(second);
    GC::Collect(); lookup(second);
    RefusedCall(fixture, "MakeBoundPlayerThenFail", {Object("BindingViewport")});
    lookup(second);
    Same(Call(second, "Destroy").value, Value::Bool(true), "Second bound PlayerPawn Destroy failed");
    lookup(""); fixture.Reset();
}
void PawnListTests(Fixture& fixture) {
    // Generated native ownership/order controls, not automatic world startup.
    // The wrappers and callback ordering mirror the independently audited GOTY
    // DLL; no normalized hidden Pawn vector substitutes for the actual fields.
    const auto head = [] { return Read("Level0", "PawnList"); };
    const auto next = [](const std::string& actor) { return Read(actor, "nextPawn"); };
    const auto pawn = [] { return Spawn("Make", {Class("Engine.Pawn")}); };
    const auto flags = [](const std::string& actor, const bool deleted, const std::uint8_t role) {
        Call(actor, "SetPawnFlags", {{Value::Bool(deleted), {}}, {Value::Byte(role), {}}});
    };
    fixture.Reset(); Same(head(), Object("").value, "Untouched world invented a native PawnList");
    const auto a = pawn(), b = pawn(), c = pawn(), absent = pawn();
    for (const auto& actor : {a, b, c}) Call(actor, "AddPawn");
    Same(head(), Object(c).value, "AddPawn did not prepend in actual call order");
    Same(next(c), Object(b).value, "Newest Pawn lost its previous native head");
    Same(next(b), Object(a).value, "Middle Pawn next pointer changed order");
    Same(next(a), Object("").value, "First Pawn next pointer is not original empty head");
    const auto revision = GetPortableRuntimeWorldRevision(); const auto beforeMissing = fixture.Snapshot("PawnBeforeMissing");
    Call(absent, "RemovePawn");
    Require(fixture.Snapshot("PawnAfterMissing") == beforeMissing && GetPortableRuntimeWorldRevision() == revision,
        "Removing an absent Pawn mutated the native list or world revision");
    Call(b, "RemovePawn"); Same(next(c), Object(a).value, "RemovePawn did not splice first matching predecessor");
    Same(next(b), Object(a).value, "RemovePawn incorrectly cleared the removed middle Pawn.nextPawn");
    Call(c, "RemovePawn"); Same(head(), Object(a).value, "RemovePawn did not replace the actual head");
    Same(next(c), Object(a).value, "RemovePawn incorrectly cleared the removed head Pawn.nextPawn");
    Call(a, "RemovePawn"); Same(head(), Object("").value, "RemovePawn did not splice the tail/last Pawn");
    RefusedCall(fixture, "PawnAddWrongReceiver"); RefusedCall(fixture, "PawnRemoveWrongReceiver");
    RefusedCall(fixture, "PawnAddTooMany", {}, {}, a); RefusedCall(fixture, "PawnRemoveTooMany", {}, {}, a);
    RefusedCall(fixture, "ContextPawnAdd", {Object("DetachedPawn")});
    RefusedCall(fixture, "ContextPawnRemove", {Object("DetachedPawn")});
    RefusedCall(fixture, "SetNextPawn", {Object("Driver")}, {}, a);
    RefusedCall(fixture, "PawnAddThenFail", {}, {}, a);
    Call(a, "AddPawn"); RefusedCall(fixture, "PawnRemoveThenFail", {}, {}, a);
    RefusedCall(fixture, "PawnSpawnThenFail");

    // Native routing is tied to the actual ULevel actor slot zero even when
    // reflected Level/XLevel have been overwritten, including after a cold save.
    Call(b, "SetPawnBindings", {Object(""), Object("")});
    Same(Read(b, "Level"), Object("").value, "Pawn reflected Level override fixture failed");
    Same(Read(b, "XLevel"), Object("").value, "Pawn reflected XLevel override fixture failed");
    Call(b, "AddPawn"); Same(head(), Object(b).value, "AddPawn routed through reflected None instead of native Level");
    Same(next(b), Object(a).value, "Native Level routing lost the existing head");
    const auto saved = fixture.Snapshot("PawnNativeBindings");
    Call(b, "RemovePawn");
    Require(LoadPortableRuntimeState((fixture.directory / "PawnNativeBindings.sav").string()) &&
        fixture.Snapshot("PawnWarmBindings") == saved, "Warm restore changed exact PawnList/nextPawn/overlays");
    fixture.Reset();
    Require(LoadPortableRuntimeState((fixture.directory / "PawnNativeBindings.sav").string()) &&
        fixture.Snapshot("PawnColdBindings") == saved, "Cold restore lost born Pawns or native-list linkage");
    GC::Collect(); Require(fixture.Snapshot("PawnAfterGc") == saved && Published(a) && Published(b),
        "GC released native PawnList links or changed the cold-restored graph");
    Call(b, "RemovePawn"); Same(head(), Object(a).value, "Cold RemovePawn failed to rebind native Level behind None overlays");
    Same(next(b), Object(a).value, "Cold removal cleared retained nextPawn");
    Call(b, "AddPawn");
    const auto canonical = fixture.Snapshot("PawnCanonical"); const auto canonicalState = Decode(canonical);
    const auto corrupt = [&](const std::string& actor, const std::string& name, const Value& value, const std::string& label) {
        auto invalid = canonicalState; auto& record = Record(invalid, Path(actor));
        const auto property = std::find_if(record.properties.begin(), record.properties.end(),
            [&](const auto& field) { return Fold(field.name) == Fold(name); });
        Require(property != record.properties.end(), "Pawn save lacks required actual link property");
        property->value = value; const auto beforeGc = GC::GetStats();
        RefusedSave(fixture, canonical, invalid, label);
        Require(GC::GetStats().numObjects == beforeGc.numObjects && GC::GetStats().memoryUsage == beforeGc.memoryUsage,
            "Rejected native Pawn link retained staged allocations");
    };
    corrupt("Level0", "PawnList", Object("Driver").value, "PawnHeadWrongClass");
    corrupt(b, "nextPawn", Object("BindingPlayer").value, "PawnNextNonActor");
    corrupt(b, "nextPawn", Object("MissingPawn").value, "PawnNextUnknownIdentity");

    // Neither native suppresses work before begun play, for lower Role, or for
    // a still-live UObject marked bDeleteMe. Event suppression stays separate.
    fixture.Reset(); Call("Driver", "SetBegun", {{Value::Bool(false), {}}});
    const auto dormant = Spawn("Make", {Class("PawnListProbe")});
    const auto preEvent = fixture.Snapshot("PawnDormantBeforeEvent");
    const auto skipped = ExecutePortableActorEvent(dormant, "PreBeginPlay", true);
    Require(skipped.passed() && fixture.Snapshot("PawnDormantAfterEvent") == preEvent,
        "Before-begun enum PreBeginPlay bypassed the actual event gate");
    flags(dormant, true, 0u); Call(dormant, "AddPawn"); Same(head(), Object(dormant).value,
        "AddPawn invented a begun-play/Role/deletion gate");
    Call(dormant, "RemovePawn"); Same(head(), Object("").value,
        "RemovePawn invented a begun-play/Role/deletion gate");
    flags(dormant, false, 0u); Call(dormant, "PreBeginPlay"); Same(head(), Object(dormant).value,
        "Direct PreBeginPlay incorrectly suppressed its AddPawn call");
    Require(Integer(dormant, "PawnEvents") == 1, "Direct pre-begun callback body did not execute");

    fixture.Reset();
    const auto callbackPawn = Spawn("Make", {Class("PawnListProbe")});
    const auto shadow = Spawn("Make", {Class("PawnListShadow")});
    Call(callbackPawn, "SetPawnShadow", {Object(shadow)}); flags(callbackPawn, false, 0u);
    Require(Integer(callbackPawn, "PawnEvents") == 1 && head().text == callbackPawn,
        "Spawn PreBeginPlay did not publish original-order Pawn insertion");
    Call(callbackPawn, "Destroy");
    Same(Read(shadow, "ObservedPawn"), Object(callbackPawn).value,
        "Pawn removal happened before the nested Shadow.Destroy callback");
    Same(head(), Object("").value, "Lower-Role Destroyed did not remove Pawn before its early return");
    Require(!Published(callbackPawn) && !Published(shadow) && Integer(callbackPawn, "PawnEvents") == 1,
        "Destroyed lower-Role ordering/deletion membership changed");
    const auto authority = Spawn("Make", {Class("PawnListProbe")}); flags(authority, false, 4u);
    Call(authority, "Destroy"); Require(Integer(authority, "PawnEvents") == 11 && head().text.empty(),
        "Authority Destroyed continuation skipped removal or callback effects");
    const auto failure = Spawn("Make", {Class("PawnListFailure")});
    RefusedCall(fixture, "Destroy", {}, {}, failure);
    Same(head(), Object(failure).value, "Rejected Destroyed did not restore native PawnList head");
    Require(Published(failure) && !QuestVr::Vm::ToBool(Read(failure, "bDeleteMe")),
        "Rejected Destroyed retained deletion before its pawn-list rollback");

    // Generic Destroy must NOT manufacture the cleanup which a disabled actual
    // Destroyed callback would have performed. References remain usable later.
    fixture.Reset(); const auto disabled = Spawn("Make", {Class("PawnListProbe")});
    Call(disabled, "DisablePawnDestroyed"); Call(disabled, "Destroy");
    Same(head(), Object(disabled).value, "Generic Destroy unlinked Pawn despite suppressed Destroyed callback");
    Require(!Published(disabled), "Disabled Destroyed incorrectly blocked native deletion");
    const auto retained = fixture.Snapshot("PawnRemovedButLinked"); fixture.Reset();
    Require(LoadPortableRuntimeState((fixture.directory / "PawnRemovedButLinked.sav").string()) &&
        fixture.Snapshot("PawnColdRemovedButLinked") == retained,
        "Cold restore discarded original retained removed-Pawn links");
    Call(disabled, "RemovePawn"); Same(head(), Object("").value, "RemovePawn rejected a removed-but-live Pawn UObject");

    // Duplicate Add intentionally creates cyclic pointers. Only an actually
    // encountered repeat during an unsuccessful search is bounded/refused.
    fixture.Reset(); const auto repeated = pawn(), missing = pawn();
    Call(repeated, "AddPawn"); Call(repeated, "AddPawn");
    Same(head(), Object(repeated).value, "Repeated Add was silently deduplicated");
    Same(next(repeated), Object(repeated).value, "Repeated Add did not retain the original self-link consequence");
    const auto cycle = fixture.Snapshot("PawnDuplicateCycle"); fixture.Reset();
    Require(LoadPortableRuntimeState((fixture.directory / "PawnDuplicateCycle.sav").string()) &&
        fixture.Snapshot("PawnColdCycle") == cycle, "Save/restore normalized an original duplicate-Add cycle");
    RefusedCall(fixture, "RemovePawn", {}, {}, missing);
    Call(repeated, "RemovePawn"); Same(head(), Object(repeated).value, "Head removal normalized its retained self-cycle");
    const auto firstMatch = pawn(); Call(firstMatch, "AddPawn");
    Call(repeated, "RemovePawn"); Same(next(firstMatch), Object(repeated).value,
        "Removal pre-rejected a cycle before finding its first matching predecessor");
    fixture.Reset(); const auto slotPawn = pawn(); Call("Level0", "Destroy");
    RefusedCall(fixture, "AddPawn", {}, {}, slotPawn); RefusedCall(fixture, "RemovePawn", {}, {}, slotPawn);
    fixture.Reset();
}
void AllActorsTests(Fixture& fixture) {
    fixture.Reset();
    const std::vector<std::string> baseline{Path("Level0"), Path("Driver"), Path("Owner"), Path("Pawn0"), Path("Leaf0")};
    const auto assertMatches = [&](const std::vector<std::string>& expected, const std::string& actor = "Driver") {
        Require(Integer(actor, "MatchCount") == static_cast<std::int32_t>(expected.size()), "AllActors produced the wrong match count");
        for (std::size_t i = 0u; i < expected.size(); ++i)
            Same(Read(actor, "Matches", static_cast<std::uint32_t>(i)), Object(expected[i]).value, "AllActors changed live Level slot order or OUT identity");
    };
    const auto scan = [&](const std::vector<std::string>& expected, const std::string& cls = "Engine.Actor",
        const std::string& tag = "None", const std::string& function = "ScanActors") {
        Same(Call("Driver", function, {Class(cls), Name(tag)}).value, Object("").value,
            "Exhausted AllActors did not write Object None to its OUT binding");
        assertMatches(expected);
    };
    scan(baseline); // Genuine seven-slot Level includes holes, receiver and five actors.
    scan(baseline, "eNgInE.aCtOr"); scan(baseline, "Core.Object");
    scan({Path("Driver"), Path("Owner"), Path("Leaf0")}, "Probe");
    scan({Path("Pawn0")}, "Engine.Pawn"); scan({Path("Pawn0")}, "Pawn");
    scan({}, "Engine.Player"); // Non-Actor class is valid, but no actor has this ancestry leaf.
    scan(baseline, "Engine.Actor", ""); scan(baseline, "Engine.Actor", "nOnE");
    scan(baseline, "Engine.Actor", "None", "ScanDeclared");
    Require(Call("Driver", "ScanEarlyReturn", {Class("Engine.Actor")}).value.text == baseline.front(),
        "Function Return cleared the current foreach OUT value");
    assertMatches({baseline.front()});
    scan(baseline, "Engine.Actor", "None", "ScanLocal");
    Same(Read("Driver", "Cursor"), Object(baseline.front()).value, "Local iterator alias overwrote unrelated persistent OUT storage");
    scan(baseline, "Engine.Actor", "None", "ScanArray");
    Same(Read("Driver", "Matches", 31u), Object("").value, "Array element OUT alias was not exhausted independently");
    scan(baseline, "Engine.Actor", "None", "ScanStruct");
    Same(Read("Driver", "Payload").fields.at("target"), Object("").value, "Struct member OUT alias retained last actor after exhaustion");
    Call("Driver", "KeepReference", {Object("Owner")});
    scan(baseline, "Engine.Actor", "None", "ScanContext");
    Same(Read("Owner", "Cursor"), Object("").value, "Context OUT alias failed to publish into its actual receiver");
    scan({Path("Pawn0")}, "Engine.Pawn", "None", "ScanTypedPawn");
    RefusedCall(fixture, "ScanTypedPawn", {Class("Engine.Actor")});
    RefusedCall(fixture, "ScanActors", {Class("")});
    RefusedCall(fixture, "ScanActors", {Object("Driver")});
    RefusedCall(fixture, "ScanActors", {Class("Missing")});
    RefusedCall(fixture, "ScanBadOutput", {Class("Engine.Actor")});
    RefusedCall(fixture, "ScanBadTag", {Class("Engine.Actor")});
    RefusedCall(fixture, "ScanTooManyArguments", {Class("Engine.Actor")});
    RefusedCall(fixture, "AllActorsOutsideIterator");
    RefusedCall(fixture, "ScanThenFail", {Class("Engine.Actor")});
    QuestVr::Vm::Limits limits; limits.writes = 4u;
    RefusedCall(fixture, "ScanActors", {Class("Engine.Actor")}, limits);
    limits = {}; limits.instructions = 15u;
    RefusedCall(fixture, "ScanActors", {Class("Engine.Actor")}, limits);
    limits = {}; limits.retainedBytes = 1u;
    RefusedCall(fixture, "ScanActors", {Class("Engine.Actor")}, limits);
    const Evaluation yes{Value::Bool(true), {}}, no{Value::Bool(false), {}};
    Call("Pawn0", "SetIterationFlags", {yes, yes});
    scan(baseline); // Hidden/bDeleteMe are not substitutes for a cleared native Level slot.
    const auto active = fixture.Snapshot("ActiveIteratorFlags");
    const auto inactivePath = fixture.directory / "InactiveIteratorPawn.sav";
    WriteBytes(inactivePath, InactivePrefix(active, Path("Pawn0")));
    Require(LoadPortableRuntimeState(inactivePath.string()) && !HasPath(ActorPaths(false), Path("Pawn0")) && HasPath(ActorPaths(true), Path("Pawn0")),
        "Iterator inactive fixture did not retain actual native Level membership");
    scan(baseline);
    scan({Path("Pawn0")}, "Engine.Pawn", "None", "ScanTypedPawn");
    for (const auto& function : {"StoreReference", "StoreArrayReference", "StoreStructReference"})
        Same(Call("Driver", function, {Object("Pawn0")}).value, Object("Pawn0").value,
            "Presentation inactivity incorrectly invalidated an assigned UObject reference");
    const auto assertInactiveReferences = [&] {
        Same(Read("Driver", "Link"), Object("Pawn0").value, "Persistent field lost its inactive UObject identity");
        Same(Read("Driver", "Matches", 31u), Object("Pawn0").value, "Persistent array lost its inactive UObject identity");
        Same(Read("Driver", "Payload").fields.at("target"), Object("Pawn0").value, "Persistent struct lost its inactive UObject identity");
    };
    assertInactiveReferences();
    RefusedCall(fixture, "StoreReference", {Object("BindingPlayer")});
    RefusedCall(fixture, "StoreReference", {Object("MissingActor")});
    RefusedCall(fixture, "SetIterationFlags", {no, no}, {}, "Pawn0");
    bool inactiveReadRefused{};
    try { static_cast<void>(Read("Pawn0", "Tag")); } catch (const std::exception&) { inactiveReadRefused = true; }
    Require(inactiveReadRefused, "Accepting inactive reference values widened the explicit inactive receiver read guard");
    const auto inactiveReferences = fixture.Snapshot("StoredInactiveObjectReferences"); fixture.Reset();
    Require(ValidatePortableRuntimeState((fixture.directory / "StoredInactiveObjectReferences.sav").string(), "SpawnFixture") &&
        LoadPortableRuntimeState((fixture.directory / "StoredInactiveObjectReferences.sav").string()) &&
        fixture.Snapshot("ColdInactiveReferences") == inactiveReferences,
        "Cold save restoration changed inactive UObject references or presentation flags");
    assertInactiveReferences(); GC::Collect(); assertInactiveReferences();
    scan(baseline); // Native membership and stored identity survive the same cold restore/GC boundary.
    RefusedCall(fixture, "SetIterationFlags", {no, no}, {}, "Pawn0");
    Require(LoadPortableRuntimeState((fixture.directory / "ActiveIteratorFlags.sav").string()), "Cannot restore active iterator fixture");
    Call("Pawn0", "SetIterationFlags", {no, no});
    fixture.Reset();

    const auto makeTagged = [&](const std::string& cls, const std::string& tag) {
        return Spawn("MakeAt", {Class(cls), Object(""), Name(tag), Vector({0.0f, 0.0f, 0.0f}), Rotation({0, 0, 0})});
    };
    const auto tagged = makeTagged("LoopActor", "MiXeD");
    scan({tagged}, "LoopActor", "mixed"); scan({}, "LoopActor", "different");
    scan({tagged}, "LoopActor", "None");
    fixture.Reset();
    const auto first = Spawn("Make", {Class("LoopActor")});
    Call("Driver", "AppendDuringScan"); const auto born = Read("Driver", "NestedActor").text;
    Require(!born.empty() && Published(born), "Iterator body failed to publish its nested birth");
    assertMatches({first, born});
    Require(Integer(born, "MatchCount") == 2, "Synchronous Spawn callback iterator missed its own provisional Level slot");
    Same(Read(born, "Matches", 0u), Object(first).value, "Callback iterator lost first live actor");
    Same(Read(born, "Matches", 1u), Object(born).value, "Callback iterator lost the currently spawning actor");
    Call("Driver", "NestedScan"); assertMatches({first, born});
    Require(Integer("Driver", "InnerCount") == 4, "Nested foreach cursors shared advancement or failed to restart");
    Same(Read("Driver", "InnerCursor"), Object("").value, "Nested iterator OUT alias was not exhausted");
    Same(Read("Driver", "Cursor"), Object("").value, "Outer iterator OUT alias was corrupted by nested traversal");
    const auto persistent = fixture.Snapshot("IteratedBornActors"); fixture.Reset();
    Require(ValidatePortableRuntimeState((fixture.directory / "IteratedBornActors.sav").string(), "SpawnFixture") &&
        LoadPortableRuntimeState((fixture.directory / "IteratedBornActors.sav").string()) && fixture.Snapshot("ColdIteratorRestore") == persistent,
        "Cold restore changed persistent iterator OUT writes, callback fields or dynamic Level order");
    assertMatches({first, born}); scan({first, born}, "LoopActor");
    RefusedCall(fixture, "ScanThenFail", {Class("LoopActor")});
    limits = {}; limits.instructions = 1200u;
    RefusedCall(fixture, "AppendForeverScan", {}, limits);
    scan({first, born}, "LoopActor");
    fixture.Reset();

    const auto futureFirst = Spawn("Make", {Class("LoopActor")}), futureSecond = Spawn("Make", {Class("LoopActor")});
    Call("Driver", "RemoveFutureDuringScan", {Object(futureSecond)}); assertMatches({futureFirst});
    Require(Published(futureFirst) && !Published(futureSecond), "Iterator body Destroy did not clear the future native slot");
    scan({futureFirst}, "LoopActor");
    const auto futureThird = Spawn("Make", {Class("LoopActor")});
    Call("Driver", "RemoveSelfDuringScan"); assertMatches({futureFirst, futureThird});
    Require(!Published(futureFirst) && !Published(futureThird), "Destroying current iterator outputs skipped or retained live slots");
    scan({}, "LoopActor");
    const auto deleting = Spawn("Make", {Class("LoopDelete")});
    Same(Call(deleting, "Destroy").value, Value::Bool(true), "Destroyed-callback iterator fixture failed to destroy");
    Require(Integer(deleting, "MatchCount") == 1 && Read(deleting, "Matches", 0u).text == deleting,
        "AllActors hid the bDeleteMe receiver before Destroyed returned and cleared its native slot");
    scan({}, "LoopDelete");
    fixture.Reset();

    const auto liveFirst = makeTagged("LoopActor", "LIVE"), liveSecond = makeTagged("LoopActor", "Other");
    Call("Driver", "TagDuringScan", {Object(liveSecond)}); assertMatches({liveFirst, liveSecond});
    Same(Read(liveSecond, "Tag"), Name("lIvE").value, "Live Tag update was not committed through its typed context alias");
    scan({liveFirst, liveSecond}, "LoopActor", "LiVe");
    fixture.Reset();
}

void AiIntegrationTests(Fixture& fixture) {
    fixture.Reset();
    const auto integer = [](std::int32_t value) { return Evaluation{Value::Integer(value), {}}; };
    const auto number = [](float value) { return Evaluation{Value::Float(value), {}}; };
    const Evaluation yes{Value::Bool(true), {}}, no{Value::Bool(false), {}};
    const auto state = [&] {
        auto decoded = Decode(fixture.Snapshot("AiInspect"));
        Require(decoded.aiManagers.size() == 1u, "Runtime did not persist exactly its initialized native AI manager");
        Require(decoded.aiManagers.front().ownerPath == Path("Level0") && decoded.aiManagers.front().levelPath == Path("MyLevel"),
            "AI manager confused its LevelInfo owner and actual serialized native ULevel");
        return std::move(decoded.aiManagers.front());
    };
    const auto sender = [](const QuestVr::Ai::State& manager, const std::string& actor, const std::string& event) -> const QuestVr::Ai::Sender& {
        const auto found = std::find_if(manager.senders.begin(), manager.senders.end(), [&](const auto& item) {
            return Fold(item.actor) == Fold(Path(actor)) && Fold(manager.eventTypes.at(item.eventType - 1u).name) == Fold(event);
        });
        Require(found != manager.senders.end(), "AI sender identity/event was not captured"); return *found;
    };
    const auto noneGc = GC::GetStats(); const auto noManagerRevision = GetPortableRuntimeWorldRevision();
    Call("Driver", "RegisterDefaultAI", {Name("Absent"), Name("Callback")});
    Call("Driver", "StartDefaultAI", {Name("Absent"), integer(0)});
    Call("Driver", "SendDefaultAI", {Name("Absent"), integer(1)});
    Call("Driver", "EndAI", {Name("Absent"), integer(0)});
    Call("Driver", "ClearReceiverAI", {Name("Absent")}); Call("Driver", "ClearSenderAI", {Name("Absent")});
    Require(fixture.Snapshot("AbsentAiManager") == fixture.legacy && GetPortableRuntimeWorldRevision() == noManagerRevision &&
        GC::GetStats().numObjects == noneGc.numObjects && GC::GetStats().memoryUsage == noneGc.memoryUsage,
        "Absent-manager wrappers allocated a graph or changed saved/world state");
    RefusedCall(fixture, "InitAIWrongReceiver");
    RefusedCall(fixture, "AISetEventCallback", {Name("Event")});
    RefusedCall(fixture, "AISetEventCallback", {Name("Event"), Name("Callback"), Name("Score"), integer(1)});
    RefusedCall(fixture, "InitAIThenFail", {}, {}, "Level0");
    Call("Level0", "InitAI");
    const auto empty = fixture.Snapshot("EmptyAiManager"); auto emptyDecoded = Decode(empty);
    Require(empty.at(4u) == 9u && empty.at(PrefixSize(empty) + 4u + 6u) == 6u &&
        emptyDecoded.objects.empty() && emptyDecoded.births.empty() && emptyDecoded.aiManagers.size() == 1u,
        "Empty initialized AI manager did not independently select envelope9/codec6");
    Require(state().eventTypes.empty() && GetPortableRuntimeWorldRevision() == noManagerRevision,
        "Native AI initialization invented events or dirtied actor geometry publication");
    const auto managerGc = GC::GetStats(); Call("Level0", "InitEventManager"); Call("Level0", "InitAI");
    Require(fixture.Snapshot("IdempotentAiInit") == empty && GC::GetStats().numObjects == managerGc.numObjects &&
        GC::GetStats().memoryUsage == managerGc.memoryUsage && GetPortableRuntimeWorldRevision() == noManagerRevision,
        "Repeated InitEventManager replaced its native object or changed geometry/save state");
    fixture.Reset();
    Require(GC::GetStats().numObjects == noneGc.numObjects && GC::GetStats().memoryUsage == noneGc.memoryUsage,
        "Legacy reset retained the empty native AI manager allocation");
    Require(LoadPortableRuntimeState((fixture.directory / "EmptyAiManager.sav").string()) && fixture.Snapshot("ColdEmptyAi") == empty,
        "Cold restore lost empty native manager presence");
    const auto otherLevelInfo = Spawn("Make", {Class("Engine.LevelInfo")});
    Call(otherLevelInfo, "InitAI");
    Call("Driver", "StartDefaultAI", {Name("SlotZeroOnly"), integer(0)});
    Call(otherLevelInfo, "AIStartEvent", {Name("FromOtherLevelInfo"), integer(0)});
    auto multiple = Decode(fixture.Snapshot("TwoLevelInfoManagers"));
    const auto attached = [&](const auto& decoded, const std::string& owner) -> const QuestVr::Ai::State& {
        const auto found = std::find_if(decoded.aiManagers.begin(), decoded.aiManagers.end(), [&](const auto& item) { return item.ownerPath == owner; });
        Require(found != decoded.aiManagers.end(), "Initialized LevelInfo lost its independent native manager attachment"); return *found;
    };
    Require(multiple.aiManagers.size() == 2u && attached(multiple, otherLevelInfo).eventTypes.empty() &&
        attached(multiple, Path("Level0")).senders.size() == 2u,
        "Native AI wrappers routed through receiver-owned manager instead of actual Level slot zero");
    Require(sender(attached(multiple, Path("Level0")), otherLevelInfo, "FromOtherLevelInfo").current.visibility == 1.0f,
        "Secondary linked LevelInfo did not use the canonical native Level manager");
    fixture.Reset(); Require(LoadPortableRuntimeState((fixture.directory / "TwoLevelInfoManagers.sav").string()), "Cannot cold restore two LevelInfo manager attachments");
    Call(otherLevelInfo, "AIEndEvent", {Name("FromOtherLevelInfo"), integer(0)});
    multiple = Decode(fixture.Snapshot("ColdTwoLevelInfoManagers"));
    Require(attached(multiple, otherLevelInfo).eventTypes.empty() &&
        sender(attached(multiple, Path("Level0")), otherLevelInfo, "FromOtherLevelInfo").current.visibility == 0.0f,
        "Cold restore confused separate manager attachment with wrapper slot-zero routing");
    fixture.Reset(); Require(LoadPortableRuntimeState((fixture.directory / "EmptyAiManager.sav").string()), "Cannot reset secondary-LevelInfo manager test");
    Call("Driver", "EndAI", {Name("Missing"), integer(1)}); Call("Driver", "ClearSenderAI", {Name("Missing")});
    Call("Driver", "ClearReceiverAI", {Name("Missing")}); Call("Driver", "RegisterDefaultAI", {Name("None"), Name("Callback")});
    Require(fixture.Snapshot("MissingAiNodes") == empty, "Absent End/Clear or None registration created an AI event type");
    Call("Driver", "RegisterDefaultAI", {Name("Distress"), Name("OnDistress")});
    auto registered = state(); Require(registered.receivers.size() == 1u && registered.receivers.front().flags == QuestVr::Ai::PerceptionFlags{} &&
        registered.receivers.front().scoreCallback.empty(), "AI registration optional perception defaults differ from original wrappers");
    auto seeded = Decode(fixture.Snapshot("RegisteredAi")); auto& manager = seeded.aiManagers.front();
    manager.historyCursor = 5u; auto& receiver = manager.receivers.front();
    receiver.callbackPending = true; receiver.eventState = 3u; receiver.detected = true; receiver.previousScore = 2.25f;
    receiver.previousBestActor = Path("Owner"); receiver.historyCursor = 3u;
    receiver.params = {Path("Pawn0"), 7.0f, 0.25f, 0.5f, 0.75f};
    const auto seededPath = fixture.directory / "SeededAiDetection.sav";
    WriteBytes(seededPath, Envelope(empty, seeded, 9u)); Require(LoadPortableRuntimeState(seededPath.string()), "Cannot restore typed AI detection/history fixture");
    auto expectedReceiver = receiver;
    Call("Driver", "RegisterAI", {Name("dIsTrEsS"), Name("Replacement"), Name("Score"), no, yes, yes, no});
    expectedReceiver.callback = "Replacement"; expectedReceiver.scoreCallback = "Score"; expectedReceiver.flags = {false, true, true, false};
    registered = state();
    Require(registered.receivers.size() == 1u && registered.receivers.front() == expectedReceiver && registered.historyCursor == 5u,
        "Re-registration reset detection/history or misread explicit perception bool flags");
    Call("Driver", "StartAI", {Name("Distress"), integer(1), number(0.5f), number(60.0f)});
    auto emissions = state(); Require(sender(emissions, "Driver", "Distress").current.volume == 0.5f &&
        sender(emissions, "Driver", "Distress").current.radius == 60.0f, "Persistent audio emission did not set current volume/radius");
    Call("Driver", "SendAI", {Name("dIsTrEsS"), integer(1), number(0.9f), number(100.0f)});
    emissions = state(); const auto& pulse = sender(emissions, "Driver", "Distress");
    Require(pulse.history[5u].volume == 0.9f && pulse.history[5u].radius == 100.0f && pulse.current.volume == 0.5f && pulse.current.radius == 60.0f,
        "Send pulse enabled persistent emission or lost per-slot maxima");
    Call("Driver", "EndAI", {Name("DISTRESS"), integer(1)}); emissions = state();
    Require(sender(emissions, "Driver", "Distress").current.volume == 0.0f && sender(emissions, "Driver", "Distress").current.radius == 0.0f &&
        sender(emissions, "Driver", "Distress").history[5u].volume == 0.9f, "End erased history maxima or failed to zero current channel");
    Call("Driver", "StartAI", {Name("Distress"), integer(256), number(0.75f), number(9000.0f)});
    Call("Driver", "StartAI", {Name("Distress"), integer(2), number(-0.2f), number(-30.0f)}); emissions = state();
    Require(sender(emissions, "Driver", "Distress").current.visibility == 0.75f && sender(emissions, "Driver", "Distress").current.smell == -0.2f &&
        sender(emissions, "Driver", "Distress").history[5u].smell == 0.0f, "Byte-masked sensory type or finite negative channel semantics changed");
    const auto beforeUnknown = sender(emissions, "Driver", "Distress");
    Call("Driver", "StartAI", {Name("Distress"), integer(255), number(7.0f), number(30.0f)}); emissions = state();
    Require(sender(emissions, "Driver", "Distress") == beforeUnknown, "Unknown masked sensory channel wrote valid sensory state");
    Call("Driver", "ClearSenderAI", {Name("Distress")}); emissions = state();
    Require(sender(emissions, "Driver", "Distress").current == QuestVr::Ai::Channels{} &&
        !sender(emissions, "Driver", "Distress").deleted && sender(emissions, "Driver", "Distress").history[5u].visibility == 0.75f,
        "Clear716 incorrectly deleted sender identity or erased channel history");
    Call("Owner", "StartDefaultAI", {Name("Other"), integer(1)}); emissions = state();
    Require(sender(emissions, "Owner", "Other").current.volume == 1.0f && sender(emissions, "Owner", "Other").current.radius == 800.0f,
        "Original default emission intensity/radius or independent actor/name identity changed");
    Call("Driver", "SetIterationFlags", {no, yes});
    Call("Driver", "StartAI", {Name("Distress"), integer(1), number(1.0f), number(400.0f)}); emissions = state();
    Require(sender(emissions, "Driver", "Distress").current.volume == 0.0f && sender(emissions, "Driver", "Distress").history[5u].volume == 0.9f,
        "Pending-kill emitter retained a live current emission or erased prior history");
    Call("Driver", "SetIterationFlags", {no, no});
    Call("Driver", "ClearReceiverAI", {Name("Distress")}); const auto deleted = state();
    Require(deleted.receivers.front().deleted && deleted.pendingDeleteCount == 1u, "Clear711 failed to defer receiver deletion");
    Call("Driver", "ClearReceiverAI", {Name("Distress")}); Require(state() == deleted, "Repeated Clear711 incremented deletion count twice");
    Call("Driver", "RegisterDefaultAI", {Name("Distress"), Name("NewReceiver")}); registered = state();
    Require(registered.receivers.size() == 2u && registered.receivers.front().deleted && !registered.receivers.back().deleted &&
        registered.pendingDeleteCount == 1u && !registered.receivers.back().detected,
        "Re-registration reused a tombstone or reset pending-deletion identity");
    RefusedCall(fixture, "AIMutationThenFail"); RefusedCall(fixture, "AISpawnThenFail");
    RefusedCall(fixture, "StartAI", {Name("Bad"), integer(1), number(std::numeric_limits<float>::infinity()), number(1.0f)});
    const auto activeAi = fixture.Snapshot("ActiveAiBeforeContext");
    const auto inactiveAiPath = fixture.directory / "InactiveAiContext.sav";
    WriteBytes(inactiveAiPath, InactivePrefix(activeAi, Path("Owner")));
    Require(LoadPortableRuntimeState(inactiveAiPath.string()) && !HasPath(ActorPaths(false), Path("Owner")),
        "Inactive AI Context fixture did not retain its presentation-inactive target");
    Call("Driver", "ContextStartAI", {Object("Owner"), Name("ContextEvent"), integer(0)}); emissions = state();
    Require(sender(emissions, "Owner", "ContextEvent").current.visibility == 1.0f,
        "Native AI wrapper incorrectly rejected a presentation-inactive Context receiver");
    RefusedCall(fixture, "StartDefaultAI", {Name("ContextEvent"), integer(0)}, {}, "Owner");
    bool inactiveReadRejected{};
    try { static_cast<void>(Read("Owner", "Tag")); } catch (const std::exception&) { inactiveReadRejected = true; }
    Require(inactiveReadRejected, "AI Context support widened unrelated explicit inactive property reads");
    Call("Driver", "ContextEndAI", {Object("Owner"), Name("ContextEvent"), integer(0)}); emissions = state();
    Require(sender(emissions, "Owner", "ContextEvent").current.visibility == 0.0f &&
        sender(emissions, "Owner", "ContextEvent").history[5u].visibility == 1.0f,
        "Inactive Context End failed to preserve its pulse/history state");
    Require(LoadPortableRuntimeState((fixture.directory / "ActiveAiBeforeContext.sav").string()), "Cannot reset inactive AI Context fixture");
    const auto born = Spawn(); Call(born, "RegisterDefaultAI", {Name("Born"), Name("Callback")});
    Call(born, "StartAI", {Name("Born"), integer(2), number(0.6f), number(1.0f)}); Call(born, "OverrideReflectedXLevel");
    Same(Read(born, "XLevel"), Object("").value, "Reflected XLevel override fixture failed");
    Call(born, "EndAI", {Name("Born"), integer(2)}); // Native routing ignores the reflected None overlay while live.
    const auto saved = fixture.Snapshot("BornAiGraph"); const auto savedState = Decode(saved); const auto savedGraph = savedState.aiManagers.front();
    fixture.Reset(); Require(LoadPortableRuntimeState((fixture.directory / "BornAiGraph.sav").string()) && fixture.Snapshot("ColdBornAiGraph") == saved,
        "Cold native AI graph restore lost runtime-born identity, fields or Level membership");
    Same(Read(born, "XLevel"), Object("").value, "Cold restore replaced the reflected XLevel override with its native root");
    Require(state() == savedGraph, "Cold restore changed native receiver/sender identities or history/ring state");
    GC::Collect(); Require(state() == savedGraph && Published(born), "GC released a native AI graph or its restored born actor roots");
    Call(born, "StartAI", {Name("Born"), integer(2), number(0.3f), number(2.0f)}); emissions = state();
    Require(sender(emissions, born, "Born").current.smell == 0.3f, "Cold birth native XLevel routing incorrectly used reflected None");
    const auto canonical = fixture.Snapshot("CanonicalAiGraph"); const auto canonicalState = Decode(canonical);
    const auto badGraph = [&](const std::string& label, const std::function<void(QuestVr::Ai::State&)>& mutate) {
        auto invalid = canonicalState; mutate(invalid.aiManagers.front()); const auto beforeGc = GC::GetStats();
        RefusedSave(fixture, canonical, invalid, label);
        Require(GC::GetStats().numObjects == beforeGc.numObjects && GC::GetStats().memoryUsage == beforeGc.memoryUsage,
            "Rejected symbolic AI graph retained staged native allocations");
    };
    // Explicit old envelope remains a pairing rejection; graph mutation checks
    // use the actual modern pair so a version mismatch cannot mask their checks.
    RefusedSave(fixture, canonical, canonicalState, "ModernBirthAiWrongEnvelope", 9u);
    badGraph("AiWrongOwner", [](auto& graph) { graph.ownerPath = Path("Driver"); });
    badGraph("AiWrongLevel", [](auto& graph) { graph.levelPath = Path("Level0"); });
    badGraph("AiWrongSenderClass", [](auto& graph) { graph.senders.front().actor = Path("BindingPlayer"); });
    badGraph("AiActorOutsideLevel", [](auto& graph) { graph.senders.front().actor = Path("DetachedActor"); });
    badGraph("AiMissingReceiver", [](auto& graph) { graph.receivers.back().actor = Path("MissingActor"); });
    badGraph("AiWrongPreviousBest", [](auto& graph) { graph.receivers.front().previousBestActor = Path("BindingViewport"); });
    badGraph("AiWrongParamsBest", [](auto& graph) { graph.receivers.front().params.bestActor = "OtherMap.Actor0"; });
    badGraph("AiInFlightProcess", [](auto& graph) { graph.processDepth = 1u; });
    RefusedCall(fixture, "AISpawnThenFail");
    fixture.Reset(); Require(GC::GetStats().numObjects == noneGc.numObjects && GC::GetStats().memoryUsage == noneGc.memoryUsage,
        "Legacy reset retained native AI graph roots or restored runtime births");
}

void WorldStartupSynthetic() {
    using Phase=PortableWorldStartupPhase;
    const auto at=[](const PortableWorldStartupProbeResult& result,Phase phase) { return result.visited.at(static_cast<std::size_t>(phase)); };
    for (const unsigned model : {1u,2u,3u,4u}) {
        Fixture fixture; const auto tables=Build(fixture,model);
        Require(InitializePortableRuntime({tables.core,tables.engine,tables.classes}).passed && LoadPortableRuntimeMap(tables.map).passed,
            "Generated world-startup map failed initialization");
        fixture.legacy=fixture.Snapshot("Legacy");
        const auto declaredTouching=[](const std::string& actor,std::uint32_t slot) {
            return ReadPortableActorDeclaredScriptProperty(Path(actor),"Engine.Actor.Touching",slot);
        };
        for (std::uint32_t slot=0u;slot<4u;++slot) {
            Same(declaredTouching("Driver",slot),Object("Owner").value,
                "World fixture lost authored nonempty declared Touching slot");
            Same(declaredTouching("Pawn0",slot),Object("").value,
                "World fixture's already-empty declared Touching slot is not None");
        }
        const auto regionBefore=fixture.Snapshot("BeforeRegion");
        const auto fallback=Read("Level0","Region");
        Same(fallback.fields.at("zone"),Object("Level0").value,"Slot0 Region did not use its native identity");
        Require(QuestVr::Vm::ToInt(fallback.fields.at("ileaf"))==-1 && QuestVr::Vm::ToInt(fallback.fields.at("zonenumber"))==0,
            "Slot0 Region entered BSP or lost -1/0 fallback");
        const auto forward=Read("Driver","Region");
        Same(forward.fields.at("zone"),Object(model==2u ? "RegionZone" : "Level0").value,
            "Actor Region fallback scanned a misleading LevelInfo or lost actual node zone");
        Require(QuestVr::Vm::ToInt(forward.fields.at("ileaf"))==((model==2u||model==3u) ? 1 : -1) &&
            QuestVr::Vm::ToInt(forward.fields.at("zonenumber"))==(model==2u ? 1 : 0),
            "Plane equality took opposite child, reversed leaf side, or lost empty/NumZones0 fallback");
        Require(fixture.Snapshot("AfterRegion")==regionBefore,"Read-only PointRegion authored a save overlay");
        if (model==2u || model==3u) {
            Call("Driver","MoveDirect",{Vector({-1.0f,0.0f,40.0f})});
            const auto backward=Read("Driver","Region");
            Same(backward.fields.at("zone"),Object("Level0").value,"Null native zone-table slot did not fallback to actual slot0");
            Require(QuestVr::Vm::ToInt(backward.fields.at("ileaf"))==2 &&
                QuestVr::Vm::ToInt(backward.fields.at("zonenumber"))==0,"Negative-side traversal chose wrong node leaf or zone");
            fixture.Reset();
        }
        const auto probe=[&](const PortableWorldStartupProbeOptions& options,Phase expected,const QuestVr::Vm::Limits& limits=QuestVr::Vm::Limits{}) {
            const auto before=fixture.Snapshot("BeforeWorldProbe"); const auto paths=ActorPaths(true);
            std::vector<std::pair<std::string,std::array<Value,4>>> touchingBefore;
            for (const auto& path:paths) {
                std::array<Value,4> slots;
                for (std::uint32_t slot=0u;slot<4u;++slot) slots[slot]=declaredTouching(path,slot);
                touchingBefore.emplace_back(path,std::move(slots));
            }
            const auto revision=GetPortableRuntimeWorldRevision(); const auto gc=GC::GetStats();
            const auto result=ProbePortableWorldStartup(options,limits);
            Require(!result.execution.passed() && !result.execution.committed && !result.execution.error.empty(),
                "World diagnostic incorrectly accepted or published a partial campaign");
            Require(result.phase==expected,"World diagnostic failed at "+std::string(PortableWorldStartupPhaseName(result.phase))+
                " instead of "+PortableWorldStartupPhaseName(expected)+": "+result.execution.error+
                " at "+result.execution.function+":"+std::to_string(result.execution.offset));
            Require(GC::GetStats().numObjects==gc.numObjects && GC::GetStats().memoryUsage==gc.memoryUsage,
                "World diagnostic retained rolled-back allocation roots");
            Require(fixture.Snapshot("AfterWorldProbe")==before && ActorPaths(true)==paths && GetPortableRuntimeWorldRevision()==revision,
                "World diagnostic leaked phase writes, frames, births, names, attachments or revision");
            for (const auto& [path,slots]:touchingBefore)
                for (std::uint32_t slot=0u;slot<4u;++slot)
                    Same(declaredTouching(path,slot),slots[slot],
                        "World diagnostic failed to restore exact declared Touching slot: "+path+":"+std::to_string(slot));
            ++refusals; return result;
        };
        const PortableWorldStartupProbeOptions fallbackOptions{"","SpawnClasses.WorldGame","fixture-options"};
        const auto terminal=model==4u ? Phase::WorldBases : Phase::ClientSetup;
        if (model==1u) {
            Require(!HasPortableRuntimeClient(), "Fresh world fixture unexpectedly bound a native client");
            probe(fallbackOptions,Phase::SelectGameInfo);
            const auto server=probe({"","Missing.Local","fixture-options","SpawnClasses.WorldGame"},terminal);
            Require(server.gameInfoPath==Path("WorldGame0") && at(server,Phase::InitializeExecution)==6u &&
                at(server,Phase::PreBeginPlay)==7u,
                "Absent-client startup ignored configured DefaultServerGame or ran a shortened world prefix");
            probe({"","SpawnClasses.WorldGame","fixture-options","Missing.Server"},Phase::SelectGameInfo);
        }
        std::uint64_t clientGeneration=1000u+model*100u;
        std::shared_ptr<QuestVr::NativeClientLifetime> localLifetime;
        const auto bindLocalClient=[&]() {
            ClearPortableRuntimeClient(); localLifetime=std::make_shared<QuestVr::NativeClientLifetime>();
            BindPortableRuntimeClient(localLifetime,++clientGeneration);
            Require(HasPortableRuntimeClient() && ReadPortableRuntimeClientView().empty(),
                "Configured local DefaultGame test lost client presence or invented a viewport");
        };
        const auto resetLocal=[&]() { fixture.Reset(); bindLocalClient(); };
        bindLocalClient();
        const auto full=probe(fallbackOptions,terminal);
        std::size_t observedWorldBirths{};
        for(const auto& birth:full.spawnedEncroachment)if(birth.classPath=="SpawnClasses.WorldBirth") {
            Require(birth.actorPath.starts_with("SpawnFixture.WorldBirth")&&birth.attempted&&birth.completed&&!birth.blocked&&
                birth.touch==0&&birth.location==std::array<float,3u>{17.25f,-31.5f,77.0f}&&
                birth.rotation==std::array<std::int32_t,3u>{-913,27123,157},
                "Genuine startup birth check did not sample CURRENT post-PostBeginPlay pose/literal touch0");
            Require(birth.initialStateReached&&birth.baseReached&&birth.postPostBeginReached&&
                birth.marksBefore==birth.marksAfter&&birth.arenasBefore==birth.arenasAfter,
                "Clear original birth check skipped the ordered lifecycle tail or retained native hit storage");
            ++observedWorldBirths;
        }
        Require(observedWorldBirths>0u,"Genuine startup diagnostic omitted native WorldBirth encroachment observations");
        // These generated actors retain mask0D clear and Brush=None. This
        // proves the real shared coordinator/current-pose/early-false adapter,
        // not active hash overlap, moving-brush push or a blocking destruction.
        Require(full.execution.function.empty() && full.execution.callStack.empty() && full.execution.offset==0u && full.execution.opcode==0u,
            "Native world terminal gap retained an unrelated successful script PC");
        Require(full.gameInfoPath==Path("WorldGame0") && !full.restoredWorld &&
            full.initGameError=="generated nonempty out error","Native GameInfo choice or InitGame out-error copy was lost");
        Require(at(full,Phase::InitializeExecution)==6u && at(full,Phase::PreBeginPlay)==7u &&
            at(full,Phase::BeginPlay)==7u && at(full,Phase::SetActorZone)==7u &&
            at(full,Phase::PostBeginPlay)==7u && at(full,Phase::SetInitialState)==7u,
            "World loops captured actor count, failed nested callbacks/live births, or called deleted future slot");
        if (model==4u) {
            Require(full.actorPath==Path("Pawn0") && full.actorSlot==4u && at(full,Phase::PostPostBeginPlay)==0u &&
                full.execution.status==QuestVr::Vm::Status::Unsupported &&
                full.execution.error=="Retail collision actor requires actual object lookup authority",
                "Physical world FindBase did not refuse its exact missing-actual-provider receiver/slot boundary");
        } else Require(at(full,Phase::PostPostBeginPlay)==7u,"World final callbacks did not observe cleared startup and first-match attachment");
        Require(!QuestVr::Vm::ToBool(Read("Level0","bBegunPlay")) &&
            QuestVr::Vm::ToInt(Read("Level0","TimeSeconds"))==123,"Rolled-back world published begun play or reset time");
        if (model==1u) {
            // A preexisting runtime-born actor reads its nonempty slots from
            // frozen typed defaults, not authored map tags or a live overlay.
            // The existing Driver callback observes this witness after the
            // native ClearTouching pass, before its deletion/birth mutations.
            const auto born=Call("Level0","Spawn",{Class("Engine.Actor"),Object(""),Name("None"),
                Vector({0.0f,0.0f,0.0f}),Rotation({0,0,0})}).value;
            Require(born.kind==Kind::Object && !born.text.empty(),"Cannot create frozen Touching startup witness");
            Call("Driver","SetTouchingWitness",{{born,{}}});
            const auto authored=fixture.Snapshot("BeforeFrozenTouching"); auto saved=Decode(authored);
            auto& frozen=Birth(saved,born.text).frozenDefaults;
            for (std::uint32_t slot=0u;slot<4u;++slot)
                frozen.push_back({"Engine.Actor.Touching","Touching",slot,Object(slot==3u ? "Pawn0" : "Owner").value});
            const auto savePath=fixture.directory/"FrozenTouching.sav";
            WriteBytes(savePath,Envelope(authored,saved));
            Require(ValidatePortableRuntimeState(savePath.string(),"SpawnFixture") && LoadPortableRuntimeState(savePath.string()),
                "Valid typed frozen Touching defaults failed checkpoint preflight/restore");
            const auto frozenBefore=fixture.Snapshot("FrozenTouchingCanonical");
            auto frozenDecoded=Decode(frozenBefore);
            const auto& frozenBirth=Birth(frozenDecoded,born.text);
            for (std::uint32_t slot=0u;slot<4u;++slot) {
                Same(declaredTouching(born.text,slot),Object(slot==3u ? "Pawn0" : "Owner").value,
                    "Frozen Touching witness did not expose its actual typed source slot");
                Require(std::any_of(frozenBirth.frozenDefaults.begin(),frozenBirth.frozenDefaults.end(),[&](const auto& property) {
                    return property.key=="Engine.Actor.Touching" && property.index==slot;
                }),"Frozen Touching witness normalized away its typed frozen baseline");
            }
            const auto frozenProbe=probe(fallbackOptions,terminal);
            Require(at(frozenProbe,Phase::InitializeExecution)==7u && at(frozenProbe,Phase::PreBeginPlay)==8u &&
                fixture.Snapshot("AfterFrozenTouchingProbe")==frozenBefore,
                "Frozen Touching witness skipped real native reset/callback phases or leaked rollback state");
            resetLocal();
            Call("Pawn0","SetPawnBindings",{Object("MisleadingLevel"),Object("MyLevel")});
            const auto badBinding=probe(fallbackOptions,Phase::InitializeExecution);
            Require(badBinding.actorSlot==4u && badBinding.actorPath==Path("Pawn0") && at(badBinding,Phase::InitGame)==0u,
                "InitExecution accepted reflected Level different from its native slot0 binding");
            resetLocal();
            probe({"","","fixture-options"},Phase::SelectGameInfo);
            probe({"","Missing.GameInfo","fixture-options"},Phase::SelectGameInfo);
            probe({"SpawnFixture.Driver","","fixture-options"},Phase::SelectGameInfo);
            probe({"SpawnClasses.Probe","","fixture-options"},Phase::SelectGameInfo);
            for (const auto* cls : {"BlockActorsWorldGame","BlockPlayersWorldGame","CollideActorsWorldGame"}) {
                const auto blocked=probe({std::string("SpawnClasses.")+cls,"SpawnClasses.WorldGame","fixture-options"},Phase::SelectGameInfo);
                Require(blocked.execution.error.find("CheckEncroachment")!=std::string::npos &&
                    at(blocked,Phase::InitializeExecution)==0u && at(blocked,Phase::InitGame)==0u,
                    "Colliding GameInfo default bypassed exact native encroachment guard or advanced world phases");
            }
            ClearPortableRuntimeClient();
            Require(!HasPortableRuntimeClient(), "URL/map precedence fixture retained a local client");
            const auto alternate=probe({"SpawnClasses.AlternateWorldGame","Missing.Fallback","fixture-options"},terminal);
            Require(alternate.gameInfoPath==Path("AlternateWorldGame0"),"URL GameInfo did not take precedence over configured fallback");
            Call("Driver","SetWorldGameType",{Class("WorldGame")});
            const auto mapDefault=probe({"","Missing.Fallback","fixture-options"},terminal);
            Require(mapDefault.gameInfoPath==Path("WorldGame0"),"Map DefaultGameType did not precede configured fallback");
            const auto urlWins=probe({"SpawnClasses.AlternateWorldGame","Missing.Fallback","fixture-options"},terminal);
            Require(urlWins.gameInfoPath==Path("AlternateWorldGame0"),"URL GameInfo did not override actual map DefaultGameType");
            resetLocal();
            const auto failed=probe({"SpawnClasses.FailingWorldGame","","fixture-options"},Phase::InitGame);
            Require(failed.execution.function=="SpawnClasses.FailingWorldGame.InitGame" &&
                at(failed,Phase::PreBeginPlay)==0u,"InitGame failure lost actual function evidence or advanced phases");
            QuestVr::Vm::Limits tight; tight.instructions=4u;
            const auto bounded=ProbePortableWorldStartup(fallbackOptions,tight);
            Require(!bounded.execution.passed() && !bounded.execution.committed &&
                fixture.Snapshot("BoundedWorldRollback")==fixture.legacy,"World VM budget failure leaked its prefix");
            ++refusals;
            const auto existing=Call("Level0","Spawn",{Class("Engine.GameInfo"),Object(""),Name("None"),Vector({0.0f,0.0f,0.0f}),Rotation({0,0,0})}).value;
            Require(existing.kind==Kind::Object && !existing.text.empty(),"Cannot create before-begun existing GameInfo fixture");
            Call("Driver","SetWorldGame",{{existing,{}}});
            ClearPortableRuntimeClient();
            const auto preserved=probe({"Missing.Url","Missing.Fallback","fixture-options"},terminal);
            Require(preserved.gameInfoPath==existing.text,"Existing Level.Game was replaced by URL/config selection");
            resetLocal();
            const auto restoredGame=Call("Level0","Spawn",{Class("Engine.GameInfo"),Object(""),Name("None"),Vector({0.0f,0.0f,0.0f}),Rotation({0,0,0})}).value;
            Call("Driver","SetWorldGame",{{restoredGame,{}}});
            Call("Driver","PrepareRestoredWorld"); Call("Driver","SetBegun",{{Value::Bool(true),{}}});
            ClearPortableRuntimeClient();
            const auto restored=probe(fallbackOptions,Phase::ClientSetup);
            Require(restored.restoredWorld && at(restored,Phase::InitializeExecution)==0u && at(restored,Phase::InitGame)==0u &&
                at(restored,Phase::PreBeginPlay)==0u && at(restored,Phase::SetActorZone)==0u &&
                at(restored,Phase::WorldBases)==0u && at(restored,Phase::PostPostBeginPlay)==6u,
                "Already-begun world reran fresh phases or omitted PostPostBeginPlay");
        }
        fixture.UnchangedSources();
    }
}
void PlayerLoginSynthetic() {
    using Phase=PortableWorldStartupPhase;
    const std::string terminal="Unsupported original post-partition client/player/detail setup; non-publishing startup probe rolled back";
    for (const unsigned variant : {1u,2u,3u}) {
        Fixture fixture; const auto tables=Build(fixture,1u,0u,variant);
        const auto loginBody=LoadPortableFunctionScript(tables.classes,
            FindPortableExport(tables.classes,"WorldGame.Login"));
        Require(InitializePortableRuntime({tables.core,tables.engine,tables.classes}).passed &&
            LoadPortableRuntimeMap(tables.map).passed,"Generated Login fixture failed initialization");
        auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();
        BindPortableRuntimeClient(lifetime,5101u);RegisterPortableRuntimeViewport(5101u,{5102u,1024u,1152u,3u,2u});
        const auto session=PreparePortableRuntimePlayerObjects(5102u);
        Require(!session.bound && session.playerPath.empty() && session.viewportClassPath=="Engine.Viewport" &&
            session.viewportOuterPath==session.clientPath,"Generated Login did not prepare an actual unpaired native Viewport");
        fixture.legacy=fixture.Snapshot("Legacy");
        const auto order=ReadPortableWorldActorOrder();
        const auto paths=ActorPaths(true);const auto revision=GetPortableRuntimeWorldRevision();
        GC::Collect();const auto gc=GC::GetStats();
        const auto unchanged=[&] {
            const auto immediate=GC::GetStats();
            Require(immediate.numObjects==gc.numObjects && immediate.memoryUsage==gc.memoryUsage,
                "Login root retained allocations before another public boundary could collect them");
            Require(fixture.Snapshot("AfterLogin")==fixture.legacy && ActorPaths(true)==paths &&
                ReadPortableWorldActorOrder()==order && GetPortableRuntimeWorldRevision()==revision &&
                !GetPortableRuntimeUnsavedScriptState(),
                "Login root retained OUT/script/RNG/birth/native order/revision state");
            const auto actual=ReadPortableRuntimePlayerSession(5102u);
            Require(lifetime->Alive() && HasPortableRuntimeClient() && actual.clientIdentity==session.clientIdentity &&
                actual.viewportIdentity==session.viewportIdentity && actual.clientPath==session.clientPath &&
                actual.viewportPath==session.viewportPath && actual.viewportClassPath==session.viewportClassPath &&
                actual.viewportOuterPath==session.viewportOuterPath && actual.clientObjectFlags==session.clientObjectFlags &&
                actual.viewportObjectFlags==session.viewportObjectFlags && !actual.bound && actual.playerPath.empty(),
                "Refused Login replaced/revoked or paired its actual native session");
            GC::Collect();const auto after=GC::GetStats();
            Require(after.numObjects==gc.numObjects && after.memoryUsage==gc.memoryUsage,
                "Login rollback lost native session roots or retained provisional objects");
            fixture.UnchangedSources();
        };
        const auto probe=[&](const PortableWorldStartupProbeOptions& options,const QuestVr::Vm::Limits& limits=QuestVr::Vm::Limits{}) {
            const auto result=ProbePortableWorldStartup(options,limits);
            Require(!result.execution.passed() && !result.execution.committed && !result.execution.error.empty(),
                "Generated Login accepted or published incomplete startup");
            unchanged();++refusals;return result;
        };
        PortableWorldStartupProbeOptions options{"","SpawnClasses.WorldGame","fixture-options"};
        const auto missing=probe(options);
        Require(missing.phase==Phase::ClientSetup && missing.playerLogins.empty() &&
            missing.execution.error=="Original SpawnPlayActor requires explicit decoded URL/config sources",
            "Actual viewport skipped the missing explicit Login URL/config source refusal");
        options.playerLogin.emplace();auto& source=*options.playerLogin;
        source.portal=u"generated-portal";
        source.urlOptions={u"class=SpawnClasses.LoginPawn",u"X=one",u"X=two"};
        source.userDefaultPlayerClass=u"Engine.PlayerPawn";source.iniUrlClass=u"Engine.PlayerPawn";
        const auto entered=probe(options);
        Require(entered.phase==Phase::ClientSetup && entered.playerLogins.size()==1u,
            "Generated Login did not reach the actual startup client boundary once");
        const auto& login=entered.playerLogins.front();
        Require(login.attempted && login.viewportIdentity==5102u && login.selectedClass=="SpawnClasses.LoginPawn" &&
            !login.paired && login.playerPath.empty(),"Login lost original class precedence, actual viewport or scripted None");
        if (variant==3u) {
            Require(!login.loginEntered && !login.loginReturned && login.error.empty() &&
                entered.execution.error=="SpawnPlayActor Login class/result constraints are not PlayerPawn",
                "Generated malformed Login ClassProperty secondary constraint reached script dispatch");
            continue;
        }
        Require(login.loginEntered && login.gamePath==Path("WorldGame0") &&
            login.functionPath=="SpawnClasses.WorldGame.Login","Login did not dispatch the real most-derived generated Game event");
        if (variant==2u) {
            // Native999 plus EndFunctionParms is three logical bytes, followed
            // by Return NoObject. Distinguish this deliberate late failure
            // from any of the earlier generated assertion natives.
            Require(loginBody.logicalSize>=5u,"Generated late Login fixture lost its final native/return");
            Require(!login.loginReturned && login.error.empty() &&
                entered.execution.function=="SpawnClasses.WorldGame.Login" && entered.execution.opcode==0x63u &&
                entered.execution.offset==loginBody.logicalSize-5u &&
                entered.execution.error.find("999")!=std::string::npos,
                "Late Login failure copied unfinished local OUT storage or lost its genuine script failure");
            continue;
        }
        const auto expectedError=[&](const PortablePlayerLoginSource& input) {
            std::string text(4096u,'L');text+="generated-portal";
            for(const auto& option:input.urlOptions) {
                text+='?';for(const auto unit:option) {
                    Require(unit<128u,"Generated Login oracle requires ASCII options");text+=static_cast<char>(unit);
                }
            }
            return text;
        };
        Require(login.loginReturned && login.error==expectedError(source) && entered.execution.error==terminal,
            "Login None/growing OUT copyback did not preserve exact Portal/ordered native Options before outer refusal: returned="+
            std::to_string(login.loginReturned)+" actualOutBytes="+std::to_string(login.error.size())+
            " expectedOutBytes="+std::to_string(expectedError(source).size())+" outTail="+
            login.error.substr(login.error.size()>100u?login.error.size()-100u:0u)+" failure="+entered.execution.error);
        const auto repeated=probe(options);
        Require(repeated.playerLogins==entered.playerLogins && repeated.execution.writes==entered.execution.writes,
            "Warm Login rollback changed class/event/None/OUT observations or deterministic write count");
        // The last VM write is the scripted OUT copyback after Return None.
        // Cutting exactly there fails AFTER all local string growth/RNG/stores,
        // while the native caller's owned Error is still its initial empty value.
        Require(entered.execution.writes>1u,"Generated Login has no final OUT write witness");
        QuestVr::Vm::Limits tight;tight.writes=entered.execution.writes-1u;
        const auto copyback=probe(options,tight);
        Require(copyback.execution.status==QuestVr::Vm::Status::Budget &&
            copyback.execution.function=="SpawnClasses.WorldGame.Login" && copyback.execution.opcode==0x2au &&
            copyback.execution.offset==loginBody.logicalSize-1u &&
            copyback.playerLogins.size()==1u && copyback.playerLogins.front().loginEntered &&
            !copyback.playerLogins.front().loginReturned && copyback.playerLogins.front().error.empty(),
            "Final growing OUT copyback budget did not fail at the actual Login return under the same outer root");
        source.urlOptions={u"CLASS=Engine.Actor",u"X=fallback"};
        const auto nonPawn=probe(options);
        Require(nonPawn.playerLogins.size()==1u && nonPawn.playerLogins.front().selectedClass=="Engine.PlayerPawn" &&
            nonPawn.playerLogins.front().loginReturned && nonPawn.playerLogins.front().error==expectedError(source),
            "Present non-Pawn CLASS did not fall through to the actual valid user class");
        source.urlOptions={u"CLASS=",u"X=ini-fallback"};source.userDefaultPlayerClass=u"Engine.Actor";
        const auto empty=probe(options);
        Require(empty.playerLogins.size()==1u && empty.playerLogins.front().selectedClass=="Engine.PlayerPawn" &&
            empty.playerLogins.front().loginReturned && empty.playerLogins.front().error==expectedError(source),
            "Present-empty CLASS/wrong-base user class skipped the ordered valid INI fallback");
        source.urlOptions={u"CLASS=Missing.Player"};
        const auto unknown=probe(options);
        Require(unknown.playerLogins.size()==1u && !unknown.playerLogins.front().loginEntered &&
            unknown.playerLogins.front().selectedClass.empty() &&
            unknown.execution.error.find("StaticLoadClass of an unmaterialized class/package is not implemented")!=std::string::npos,
            "Unknown class load was silently treated as a proven failure/fallback");
        source.urlOptions.clear();source.userDefaultPlayerClass.reset();source.iniUrlClass.reset();
        const auto noClass=probe(options);
        Require(noClass.playerLogins.size()==1u && !noClass.playerLogins.front().loginEntered &&
            noClass.execution.error=="Original SpawnPlayActor fatal LoadPlayerClass: no valid PlayerPawn class",
            "Absent class sources manufactured a player class or entered Login");
    }
    std::cout<<"Generated actual startup Login: prepared native Viewport, exact PlayerPawn declaration/secondary constraint, ordered CLASS/user/INI selection, real Game event/None, growing owned OUT copyback and late same-root rollback; NOT player completion or playable startup\n";
}
void LoadContextSynthetic() {
    // Actual generated runtime/cache/save integration, not another real map.
    // These fixtures do not prepare the independent native weak-hash service;
    // ownership/VM lookup and live native Actor slots must still exclude None.
    using Profile=QuestVr::OriginalLinkerLoadProfile;
    const Profile game{false,true,true},clientOnly{false,true,false},editor{true,false,false};
    Fixture fixture;const auto tables=Build(fixture,2u,0u,0u,true);
    const std::vector<PortablePackageTables> packages{tables.core,tables.engine,tables.classes};
    const auto sourceOrder=ReadPortableLevel68ActorOrder(tables.map);
    Require(sourceOrder.size()==12u,"Explicit-context fixture lost its source slots/interleaved holes");
    const std::vector<std::string> excluded{"Core.ContextSourceOnly","Engine.ContextSourceOnly",
        "SpawnClasses.ContextSourceOnly",Path("OrdinaryLookingOne"),Path("OrdinaryLookingTwo"),Path("OrdinaryLookingThree")};
    const auto boot=[&](const std::optional<Profile>& profile) {
        const auto initialized=InitializePortableRuntime(packages,1u,profile);
        Require(initialized.passed && ReadPortableRuntimeLoadProfile()==profile,
            "Runtime did not retain its explicit profile/raw inspection distinction");
        const auto loaded=LoadPortableRuntimeMap(tables.map);
        Require(loaded.passed,"Context-profile generated map did not load");
        const auto absent=profile&&!profile->editor;
        Require(initialized.contextExcludedExports==(absent?3u:0u) && loaded.contextExcludedExports==(absent?3u:0u),
            "Initial-script/map context exclusions lost source-aligned cache accounting");
        Require(ReadPortableLevel68ActorOrder(tables.map)==sourceOrder,
            "Runtime context admission rewrote raw source Level references");
        GC::Collect();return GC::GetStats();
    };
    const auto verify=[&](bool filtered) {
        const auto actual=ReadPortableWorldActorOrder();
        Require(actual.available && actual.actorIndices.size()==sourceOrder.size() && !actual.firstDynamic,
            "Context loading compacted original slots or invented a native static/dynamic split");
        std::vector<std::string> expected;
        for(std::size_t slot=0u;slot<sourceOrder.size();++slot) {
            const auto ref=sourceOrder[slot];
            if(!ref) {Require(actual.actorIndices[slot]==0u,"Serialized None became a runtime Actor");continue;}
            const auto path=Path(GetPortableObjectPath(tables.map,ref));
            const auto rejected=filtered && (tables.map.exports.at(static_cast<std::size_t>(ref-1)).ObjFlags&
                static_cast<ObjectFlags>(0x00030000u))==static_cast<ObjectFlags>(0u);
            if(rejected) {
                Require(actual.actorIndices[slot]==0u && actual.staticFlags[slot]==0u,
                    "Excluded source export received a native Actor slot/static flag");continue;
            }
            expected.push_back(path);const auto ownership=ReadPortableRuntimeObjectOwnership(path);
            const auto linker=ReadPortableRuntimeObjectLinker(path);
            Require(actual.actorIndices[slot]==ownership.objectIndex && linker.attached &&
                linker.nativeIndex==ref-1 && linker.sourceExportIndex==static_cast<std::size_t>(ref-1),
                "Source-aligned cache hole shifted a later admitted Actor/export index");
        }
        // Public render snapshots use retained export order; native Level
        // slots above deliberately interleave the admitted Pawn and exclusions
        // in a different order. Compare membership here, not those two orders.
        auto actualPaths=ActorPaths(true);std::sort(actualPaths.begin(),actualPaths.end());
        std::sort(expected.begin(),expected.end());
        Require(actualPaths==expected && expected.size()==tables.initialActors-(filtered?3u:0u),
            "Context loading leaked editor-only Actors into public runtime enumeration");
        for(const auto& path:excluded) {
            bool ownershipMissing{},linkerMissing{};
            try {static_cast<void>(ReadPortableRuntimeObjectOwnership(path));}
            catch(const std::exception&) {ownershipMissing=true;}
            try {static_cast<void>(ReadPortableRuntimeObjectLinker(path));}
            catch(const std::exception&) {linkerMissing=true;}
            Require(ownershipMissing==filtered && linkerMissing==filtered,
                "Excluded reflection metadata masqueraded as an actual indexed UObject/linker cache entry");
        }
        const auto direct=Read("Driver","Link");
        const auto nested=Read("Driver","Payload");
        Same(direct,Object(filtered?"":"OrdinaryLookingOne").value,"Direct excluded source Object reference did not resolve to actual None");
        Require(nested.kind==Kind::Struct && nested.fields.at("count").integer==29,
            "Nested reference filtering erased nonreference Struct payload");
        Same(nested.fields.at("target"),Object(filtered?"":"OrdinaryLookingTwo").value,
            "Nested Struct Object reference bypassed loader context None resolution");
        Same(Read("Driver","NestedActor"),Object("AdmittedPlayer").value,
            "Context loader removed an admitted PlayerPawn by type/name");
        Same(Call("Driver","ContextValidImportedObject").value,Object(filtered?"":"Core.ContextSourceOnly").value,
            "Valid typed import of an excluded same-package-class Object did not resolve to None");
        if(filtered)RefusedCall(fixture,"ContextWrongImportedObject");
        Require(ReadPortableRuntimeObjectOwnership(Path("AdmittedPlayer")).classPath=="Engine.PlayerPawn" &&
            ReadPortableRuntimeNativeModel().available,"Admitted PlayerPawn/native Model authority was lost");
        const auto model=LoadPortableModel68(tables.map,FindPortableRootModel68Export(tables.map));
        Require(model.nodes.size()==2u && model.zones.size()==2u,
            "Context exclusion erased required independently decoded BSP source geometry");
        fixture.UnchangedSources();
    };
    const auto rawGc=boot(std::nullopt);verify(false);
    const auto raw=fixture.Snapshot("ContextRawPristine");
    Require(raw.size()>4u && !(raw[0]=='D'&&raw[1]=='X'&&raw[2]=='L'&&raw[3]=='C'),
        "Historical raw inspection save was silently wrapped");
    const auto gameGc=boot(game);verify(true);
    Require(rawGc.numObjects==gameGc.numObjects+6u,
        "Context-excluded script/map exports were allocated/rooted behind hidden wrappers");
    const auto pristine=fixture.Snapshot("ContextGamePristine");
    Require(pristine.size()>20u && pristine[0]=='D'&&pristine[1]=='X'&&pristine[2]=='L'&&pristine[3]=='C',
        "Explicit runtime profile checkpoint lacks its distinct DXLC envelope");
    const auto unchanged=[&](const Bytes& before,const std::vector<std::string>& paths,
        const PortableWorldActorOrderSnapshot& order,std::uint64_t revision,const GCStats& gc) {
        Require(fixture.Snapshot("ContextUnchanged")==before && ActorPaths(true)==paths &&
            ReadPortableWorldActorOrder()==order && GetPortableRuntimeWorldRevision()==revision,
            "Rejected context checkpoint mutated save/slots/identities/revision");
        GC::Collect();const auto after=GC::GetStats();
        Require(after.numObjects==gc.numObjects && after.memoryUsage==gc.memoryUsage,
            "Rejected context checkpoint changed native UObject/GC ownership");
        fixture.UnchangedSources();
    };
    const auto reject=[&](const std::filesystem::path& path) {
        const auto before=fixture.Snapshot("ContextBeforeRejected");const auto file=ReadBytes(path);
        const auto paths=ActorPaths(true);const auto order=ReadPortableWorldActorOrder();
        const auto revision=GetPortableRuntimeWorldRevision();GC::Collect();const auto gc=GC::GetStats();
        Require(!ValidatePortableRuntimeState(path.string(),"SpawnFixture") && !LoadPortableRuntimeState(path.string()),
            "Checkpoint admitted incompatible loader profile or context-excluded gameplay identity");
        Require(ReadBytes(path)==file,"Rejected context checkpoint overwrote its original input file");
        unchanged(before,paths,order,revision,gc);++refusals;
    };
    reject(fixture.directory/"ContextRawPristine.sav");
    const auto prefixCheckpoint=[&](const unsigned section,const std::string& target) {
        // Independent minimal valid legacy v3 inner payload, under a freshly
        // checksummed SAME-profile envelope. Exclusion must reject its actual
        // identity, not merely a bad CRC/profile or malformed script trailer.
        Bytes inner;U32(inner,0x53515844u);U32(inner,3u);
        const auto pathString=[&] {
            U32(inner,static_cast<std::uint32_t>(target.size()));
            inner.insert(inner.end(),target.begin(),target.end());
        };
        for(unsigned list=0u;list<3u;++list) {
            U32(inner,list==section?1u:0u);if(list==section)pathString();
        }
        FloatBytes(inner,100.0f);U32(inner,section==3u?1u:0u);
        if(section==3u){pathString();FloatBytes(inner,25.0f);}
        U32(inner,0u);U32(inner,0u); // Credits/skill points.
        for(unsigned list=0u;list<4u;++list)U32(inner,0u); // Flags/goals/notes/applied.
        return QuestVr::EncodeLoadContextSave(game,inner);
    };
    for(unsigned section=0u;section<4u;++section) {
        unsigned ordinal{};
        for(const auto& target:excluded) {
            const auto path=fixture.directory/("ContextExcludedPrefix"+std::to_string(section)+'-'+std::to_string(ordinal++)+".sav");
            WriteBytes(path,prefixCheckpoint(section,target));reject(path);
        }
        const auto path=fixture.directory/("ContextExcludedFoldedPrefix"+std::to_string(section)+".sav");
        WriteBytes(path,prefixCheckpoint(section,Fold(Path("OrdinaryLookingTwo"))));reject(path);
    }
    // Remove all map lookup/exclusion entries. These validate-only calls must
    // now use the intended map's immutable source schema, without loading its
    // UObjects or publishing a replacement world merely to inspect the save.
    Require(UnloadPortableRuntimeMap()!=0u && ActorPaths(true).empty() &&
        !ReadPortableWorldActorOrder().available && !ReadPortableRuntimeNativeModel().available,
        "Cold context-prefix fixture retained a loaded map/actor registry");
    const auto unloaded=fixture.Snapshot("ContextUnloaded");
    const auto unloadedOrder=ReadPortableWorldActorOrder();const auto unloadedRevision=GetPortableRuntimeWorldRevision();
    GC::Collect();const auto unloadedGc=GC::GetStats();
    for(unsigned section=0u;section<4u;++section) {
        const auto path=fixture.directory/("ContextExcludedFoldedPrefix"+std::to_string(section)+".sav");
        const auto originalFile=ReadBytes(path);
        Require(!ValidatePortableRuntimeState(path.string(),"SpawnFixture"),
            "Cold checkpoint validation admitted an excluded intended-map gameplay prefix");
        Require(ReadBytes(path)==originalFile,"Cold excluded-prefix validation rewrote its input file");
        unchanged(unloaded,{},unloadedOrder,unloadedRevision,unloadedGc);++refusals;
    }
    Require(ValidatePortableRuntimeState((fixture.directory/"ContextGamePristine.sav").string(),"SpawnFixture"),
        "Cold context-prefix validation rejected an otherwise valid same-profile checkpoint");
    unchanged(unloaded,{},unloadedOrder,unloadedRevision,unloadedGc);
    boot(game);verify(true);
    Require(fixture.Snapshot("ContextRebootAfterColdPrefix")==pristine,
        "Cold read-only validation changed subsequent pristine source reconstruction");
    // Existing legacy campaign inventory may name an object from a previous
    // map which is not loaded now. Do not replace source-proved exclusion with
    // a blanket refusal of every currently absent lookup identity.
    const auto previousMap=std::string("PreviousFixture.OldInventory");
    const auto previousPath=fixture.directory/"ContextPreviousMapPrefix.sav";
    WriteBytes(previousPath,prefixCheckpoint(0u,previousMap));
    Require(ValidatePortableRuntimeState(previousPath.string(),"SpawnFixture") &&
        LoadPortableRuntimeState(previousPath.string()) &&
        GetPortableRuntimeInventoryItems()==std::vector<std::string>{previousMap},
        "Context exclusion rejected tolerant legacy inventory from an unrelated prior map");
    Require(LoadPortableRuntimeState((fixture.directory/"ContextGamePristine.sav").string()) &&
        fixture.Snapshot("ContextAfterPreviousMapPrefix")==pristine,
        "Legacy prior-map prefix control did not restore the original sparse checkpoint");
    verify(true);
    const auto changedPawn=ReadPortableRuntimeObjectOwnership(Path("AdmittedPlayer")).objectIndex;
    Call("Driver","StoreReference",{Object("AdmittedPlayer")});
    const auto birth=Spawn();const auto changedPaths=ActorPaths(true);
    const auto changed=fixture.Snapshot("ContextGameChanged");
    Require(changed!=pristine && Published(birth) && Read("Driver","Link").text==Path("AdmittedPlayer"),
        "Explicit-profile changed checkpoint lacks genuine script/birth state");
    Require(ValidatePortableRuntimeState((fixture.directory/"ContextGameChanged.sav").string(),"SpawnFixture"),
        "Same-profile changed save refused read-only preflight");
    Require(LoadPortableRuntimeState((fixture.directory/"ContextGamePristine.sav").string()),
        "Same-profile pristine checkpoint refused warm restore");
    verify(true);
    Require(fixture.Snapshot("ContextGameWarmPristine")==pristine && !Published(birth),
        "Warm pristine context restore retained births or shifted sparse export storage");
    Require(LoadPortableRuntimeState((fixture.directory/"ContextGameChanged.sav").string()) &&
        fixture.Snapshot("ContextGameWarmChanged")==changed && ActorPaths(true)==changedPaths &&
        ReadPortableRuntimeObjectOwnership(Path("AdmittedPlayer")).objectIndex==changedPawn,
        "Warm context replacement shifted original export identity or lost appended birth/save bytes");
    boot(game);
    Require(ValidatePortableRuntimeState((fixture.directory/"ContextGameChanged.sav").string(),"SpawnFixture") &&
        LoadPortableRuntimeState((fixture.directory/"ContextGameChanged.sav").string()) &&
        fixture.Snapshot("ContextGameColdChanged")==changed && ActorPaths(true)==changedPaths,
        "Cold same-profile restore lost sparse source indices, genuine birth tail or exact checkpoint");
    boot(clientOnly);verify(true);reject(fixture.directory/"ContextGameChanged.sav");
    const auto client=fixture.Snapshot("ContextClientPristine");
    Require(client!=pristine,"Distinct explicit profiles produced an identical profile-bound checkpoint");
    boot(editor);verify(false);reject(fixture.directory/"ContextGameChanged.sav");
    const auto editorSave=fixture.Snapshot("ContextEditorPristine");
    Require(editorSave!=raw,"Explicit editor profile was conflated with unfiltered raw inspection");
    boot(std::nullopt);verify(false);reject(fixture.directory/"ContextGameChanged.sav");
    reject(fixture.directory/"ContextEditorPristine.sav");
    Require(ValidatePortableRuntimeState((fixture.directory/"ContextRawPristine.sav").string(),"SpawnFixture") &&
        LoadPortableRuntimeState((fixture.directory/"ContextRawPristine.sav").string()) &&
        fixture.Snapshot("ContextRawRestored")==raw,
        "Legacy raw save stopped working after explicit-profile initialization/teardown");
    fixture.UnchangedSources();
    std::cout<<"Generated actual load-context runtime: sparse script/map export caches, arbitrary-name editor-only Actor/Brush/Camera exclusions, direct/nested None, preserved source slots/indices/geometry, GC and same/warm/cold profile-bound checkpoint replacement; NOT actual Training/player completion\n";
}
void DecalGuardSynthetic() {
    // Real generated reflection, native Level slots and valid empty root BSP.
    // After the exact entry guards, this BSP genuinely has no geometry. The
    // ordinary false result still publishes the native's RNG and upper clamp.
    for (unsigned schema = 1u; schema <= 5u; ++schema) {
        Fixture fixture; const auto tables = Build(fixture, 0u, schema);
        Require(InitializePortableRuntime({tables.core, tables.engine, tables.classes}).passed &&
            LoadPortableRuntimeMap(tables.map).passed, "Generated Decal guard map failed initialization");
        fixture.legacy = fixture.Snapshot("Legacy");
        Require(!HasPortableRuntimeClient() && ReadPortableRuntimeClientView().empty(),
            "Runtime initialization invented a renderer client or a viewport");
        Require(Integer("Decal0", "MultiDecalLevel") == 9 &&
            Read("Decal0", "LastRenderedTime").floating == 27.5f,
            "Generated Decal native guard input fields were not authored correctly");
        const auto surfaces = Read("Decal0", "SurfList");
        Require(surfaces.kind == Kind::IntArray && surfaces.intArray.empty(),
            "Generated original Const SurfList lacks its owned IntProperty schema");
        const auto falseGuard = [&](const std::string& label, const std::vector<Evaluation>& args = {Evaluation{Value::Float(100.0f), {}}}) {
            const auto before = fixture.Snapshot("Before" + label); const auto revision = GetPortableRuntimeWorldRevision();
            const auto gc = GC::GetStats(); const auto result = ExecutePortableActorFunction(Path("Decal0"), "AttachDecal", args);
            Require(result.passed() && result.committed && result.value.kind == Kind::Bool && !QuestVr::Vm::ToBool(result.value),
                "Original AttachDecal negative guard did not return bool false: " + label + ": " + result.error);
            Require(GC::GetStats().numObjects == gc.numObjects && GC::GetStats().memoryUsage == gc.memoryUsage &&
                GetPortableRuntimeWorldRevision() == revision && fixture.Snapshot("After" + label) == before,
                "AttachDecal negative guard changed actor fields, RNG, native graph, save, revision or GC: " + label);
            Require(Integer("Decal0", "MultiDecalLevel") == 9 &&
                Read("Decal0", "LastRenderedTime").floating == 27.5f && Read("Decal0", "SurfList").intArray.empty(),
                "Early AttachDecal false clamped MultiDecalLevel or changed unrelated native fields: " + label);
        };
        if (schema >= 3u) {
            RefusedCall(fixture, "AttachDefault", {}, {}, "Decal0");
            // Supply both parameters so the optional-flag variant also reaches
            // the native descriptor check, rather than only failing arity.
            RefusedCall(fixture, "AttachDecal", {Evaluation{Value::Float(100.0f), {}}, Vector({1.0f, 0.0f, 0.0f})}, {}, "Decal0");
            fixture.UnchangedSources(); continue;
        }
        falseGuard("AbsentClient");
        // Compiled final-call arguments are evaluated before the native guard,
        // including optional DecalDir. The guard cannot suppress their effects.
        const auto argumentRevision = GetPortableRuntimeWorldRevision();
        Same(Call("Decal0", "AttachEvaluatedArguments").value, Value::Bool(false),
            "Compiled AttachDecal arguments changed the original no-client result");
        Require(Integer("Decal0", "ArgumentCounter") == 11 && GetPortableRuntimeWorldRevision() == argumentRevision + 1u,
            "AttachDecal guard bypassed either argument expression or split their shared transaction");
        const auto sideEffects = Decode(fixture.Snapshot("EvaluatedArguments"));
        Require(!sideEffects.nativeDecals && !sideEffects.randomSeed,
            "AttachDecal no-client guard allocated a native graph or drew RNG after evaluating arguments");
        fixture.Reset();
        RefusedCall(fixture, "AttachDecal", {}, {}, "Decal0");
        RefusedCall(fixture, "AttachDecal", {Evaluation{Value::Float(100.0f), {}}, Vector({0.0f, 0.0f, 0.0f}), Name("extra")}, {}, "Decal0");
        RefusedCall(fixture, "WriteSurfList", {Evaluation{Value::IntArray({3, 3, 1}), {}}}, {}, "Decal0");

        auto lifetime = std::make_shared<QuestVr::NativeClientLifetime>();
        BindPortableRuntimeClient(lifetime, 101u);
        Require(HasPortableRuntimeClient() && ReadPortableRuntimeClientView().empty(),
            "Live native client without viewports was confused with absence");
        falseGuard("EmptyRegistry");
        const QuestVr::NativeViewport viewport{201u, 1024u, 1152u, 3u, 2u};
        auto currentViewport=viewport;
        RegisterPortableRuntimeViewport(101u, viewport);
        const auto bound = [&]() {
            const auto views = ReadPortableRuntimeClientView();
            Require(HasPortableRuntimeClient() && views.size() == 1u && views[0].identity == currentViewport.identity &&
                views[0].width == currentViewport.width && views[0].height == currentViewport.height &&
                views[0].swapchainImages == currentViewport.swapchainImages && views[0].eyeCount == currentViewport.eyeCount,
                "Runtime lost or fabricated engine-owned live viewport metadata");
        };
        bound();
        Require(fixture.Snapshot("ClientBinding") == fixture.legacy && !GetPortableRuntimeUnsavedScriptState(),
            "Native renderer client binding leaked into actor/save state or map discard guard");
        fixture.Reset(); bound();
        Require(UnloadPortableRuntimeMap() != 0u, "Generated map refused clean unload with engine-owned viewport metadata");
        bound(); Require(LoadPortableRuntimeMap(tables.map).passed, "Cannot reload generated map with an existing native client"); bound();
        if (schema == 2u) {
            // Wrong declaration is deliberately untouched by absent/empty
            // client guards, but cannot pass the live-client Texture guard.
            RefusedCall(fixture, "AttachDecal", {Evaluation{Value::Float(100.0f), {}}}, {}, "Decal0");
        } else {
            falseGuard("TextureNone", {Evaluation{Value::Float(100.0f), {}}, Vector({0.0f, 0.0f, 0.0f})});
            Same(Call("Driver", "FindPlayer").value, Object("").value,
                "Native renderer viewport fabricated PlayerPawn possession");
            const auto beforeRevoke=fixture.Snapshot("BeforePostNativeRevoke");
            const auto revokeRevision=GetPortableRuntimeWorldRevision(); const auto revokeGc=GC::GetStats();
            Value externalOut=Value::Integer(17); std::vector<std::int32_t> outWrites;
            auto output=std::make_shared<QuestVr::Vm::Reference>(); output->zero=Value::Integer(0);
            output->read=[&]() { return externalOut; };
            output->write=[&](const Value& value) {
                externalOut=value; outWrites.push_back(QuestVr::Vm::ToInt(value));
                if (QuestVr::Vm::ToInt(value)==73) lifetime->Revoke();
            };
            const auto revoked=ExecutePortableActorFunction(Path("Decal0"),"AttachThenOutWrite",{{Value{},output}});
            Require(!revoked.passed() && !revoked.committed && revoked.status==QuestVr::Vm::Status::Unsupported &&
                revoked.error.find("Native client lifetime revoked during script transaction")!=std::string::npos,
                "Post-native render teardown committed a client-dependent script transaction");
            Require(externalOut.kind==Kind::Int && externalOut.integer==17 && outWrites==std::vector<std::int32_t>{73,17},
                "Post-native revocation failed to restore the externally owned OUT alias exactly");
            Require(Integer("Decal0","ArgumentCounter")==0 && GetPortableRuntimeWorldRevision()==revokeRevision &&
                GC::GetStats().numObjects==revokeGc.numObjects && GC::GetStats().memoryUsage==revokeGc.memoryUsage &&
                fixture.Snapshot("AfterPostNativeRevoke")==beforeRevoke,
                "Post-native revocation leaked evaluated argument writes, RNG, graph, save, revision or GC allocations");
            Require(!HasPortableRuntimeClient() && ReadPortableRuntimeClientView().empty(),
                "Script rollback resurrected the externally revoked renderer lifetime"); ++refusals;
            ClearPortableRuntimeClient(); lifetime=std::make_shared<QuestVr::NativeClientLifetime>();
            BindPortableRuntimeClient(lifetime,102u);
            currentViewport={202u,viewport.width,viewport.height,viewport.swapchainImages,viewport.eyeCount};
            RegisterPortableRuntimeViewport(102u,currentViewport); bound();
            Call("Decal0", "SetTexture", {Object("Texture0")});
            const auto revision = GetPortableRuntimeWorldRevision();
            const auto gc = GC::GetStats();
            const auto miss = Call("Decal0", "AttachDecal", {{Value::Float(100.0f), {}}});
            const auto missedState = Decode(fixture.Snapshot("EmptyBspMiss"));
            Require(miss.committed && miss.value.kind==Kind::Bool && !QuestVr::Vm::ToBool(miss.value) &&
                missedState.randomSeed && *missedState.randomSeed==1030492215u && !missedState.nativeDecals &&
                Integer("Decal0","MultiDecalLevel")==4 && Read("Decal0","SurfList").intArray.empty(),
                "Real empty BSP no-hit lost six pretrace draws/clamp, invented graph or returned true");
            Require(GC::GetStats().numObjects == gc.numObjects && GC::GetStats().memoryUsage == gc.memoryUsage &&
                GetPortableRuntimeWorldRevision() == revision + 1u && Read("Decal0","LastRenderedTime").floating==27.5f,
                "Ordinary no-hit retained geometry allocations, lost its shared revision or changed render time");
            Same(Call("Decal0", "AttachEvaluatedArguments").value, Value::Bool(false),
                "Evaluated supplied-direction call on real empty BSP fabricated a hit");
            const auto evaluatedMiss = Decode(fixture.Snapshot("EvaluatedEmptyBspMiss"));
            Require(Integer("Decal0","ArgumentCounter")==11 && evaluatedMiss.randomSeed==missedState.randomSeed &&
                !evaluatedMiss.nativeDecals, "Supplied-direction no-hit lost argument effects or drew optional RNG");
            fixture.Reset();
            lifetime->Revoke();
            Require(!HasPortableRuntimeClient() && ReadPortableRuntimeClientView().empty(),
                "Revoked renderer lifetime retained a live client view");
            falseGuard("RevokedClient");
        }
        ClearPortableRuntimeClient();
        Require(!HasPortableRuntimeClient() && ReadPortableRuntimeClientView().empty(),
            "Explicit client teardown retained viewport metadata");
        auto replacement = std::make_shared<QuestVr::NativeClientLifetime>();
        BindPortableRuntimeClient(replacement, 103u);
        RegisterPortableRuntimeViewport(103u, {203u, viewport.width, viewport.height, viewport.swapchainImages, viewport.eyeCount});
        ShutdownPortableRuntime();
        Require(!HasPortableRuntimeClient() && ReadPortableRuntimeClientView().empty(),
            "Runtime shutdown retained the prior renderer session binding");
        Require(InitializePortableRuntime({tables.core, tables.engine, tables.classes}).passed &&
            LoadPortableRuntimeMap(tables.map).passed && !HasPortableRuntimeClient() && ReadPortableRuntimeClientView().empty(),
            "Fresh runtime adopted stale renderer metadata from its previous lifetime");
        falseGuard("FreshLifetimeAbsent"); fixture.UnchangedSources();
    }
}
void DecalNativeSynthetic() {
    const auto refusedNative=[](const Fixture& fixture,const std::string& label,
        const std::string& function,const std::vector<Evaluation>& args=std::vector<Evaluation>{},
        const QuestVr::Vm::Limits& limits=QuestVr::Vm::Limits{}) {
        try { RefusedCall(fixture,function,args,limits,"Decal0"); }
        catch (const std::exception& error) { throw std::runtime_error(label+": "+error.what()); }
    };
    const auto bind = []() {
        auto lifetime = std::make_shared<QuestVr::NativeClientLifetime>();
        BindPortableRuntimeClient(lifetime, 401u);
        RegisterPortableRuntimeViewport(401u, {402u, 1024u, 1152u, 3u, 2u});
        return lifetime;
    };
    const auto expectGraph = [](const QuestVr::ScriptSavedState& saved, std::size_t records, std::size_t listSize) {
        Require(saved.nativeDecals.has_value(), "Successful native Attach did not publish an actual surface-owned graph");
        const auto& graph = *saved.nativeDecals;
        Require(graph.mapName=="SpawnFixture" && graph.modelPath=="SpawnFixture.Model0" &&
            graph.levelPath=="SpawnFixture.MyLevel" && graph.surfaceCount==1u && graph.nodeCount==1u &&
            graph.state.surfaces.size()==1u && graph.state.surfaces[0].size()==records && graph.state.owners.size()==1u &&
            graph.state.owners[0].ownerIdentity==Path("Decal0") &&
            graph.state.owners[0].surfaces==std::vector<std::int32_t>(listSize,0),
            "Native Attach graph identity/count/order or mirrored duplicate SurfList was lost");
        for (const auto& record : graph.state.surfaces[0]) Require(record.ownerIdentity==Path("Decal0") &&
            record.nodeMembership==std::vector<std::int32_t>{0},
            "Native Attach graph lost its canonical owner or actual finite-polygon membership");
    };
    const auto expectQuad = [](const QuestVr::DecalSurfaceRecord& record, const QuestVr::DecalPrimaryQuad& quad) {
        for (std::size_t i=0u;i<4u;++i) {
            const auto actual=record.relativeCorners[i]; const auto expected=quad.relativeCorners[i];
            if (!(std::fabs(actual.x-expected.x)<=0.0001f && std::fabs(actual.y-expected.y)<=0.0001f &&
                std::fabs(actual.z-expected.z)<=0.0001f))
                throw std::runtime_error("Native record corner " + std::to_string(i) + " actual=" +
                    std::to_string(actual.x) + "," + std::to_string(actual.y) + "," + std::to_string(actual.z) +
                    " expected=" + std::to_string(expected.x) + "," + std::to_string(expected.y) + "," +
                    std::to_string(expected.z));
        }
    };
    {
        Fixture fixture; const auto tables=Build(fixture,5u,1u);
        Require(InitializePortableRuntime({tables.core,tables.engine,tables.classes}).passed &&
            LoadPortableRuntimeMap(tables.map).passed,"Generated native decal floor failed initialization");
        fixture.legacy=fixture.Snapshot("Legacy"); auto lifetime=bind();
        Call("Decal0","SetTexture",{Object("Texture0")});
        const auto revision=GetPortableRuntimeWorldRevision(), gc=GC::GetStats().numObjects;
        const auto attached=Call("Decal0","AttachDecal",{{Value::Float(100.0f),{}}});
        Require(attached.committed && attached.value.kind==Kind::Bool && QuestVr::Vm::ToBool(attached.value),
            "Native Attach did not return true for a real authored finite floor");
        const auto firstBytes=fixture.Snapshot("FirstNativeAttachment"); const auto first=Decode(firstBytes);
        expectGraph(first,1u,1u);
        Require(first.randomSeed && *first.randomSeed==1030492215u && Integer("Decal0","MultiDecalLevel")==4 &&
            Read("Decal0","SurfList").intArray==std::vector<std::int32_t>{0} &&
            Read("Decal0","LastRenderedTime").floating==27.5f &&
            GetPortableRuntimeWorldRevision()==revision+1u && GC::GetStats().numObjects==gc+1u,
            "Native Attach did not atomically publish six RNG draws/clamp/list/one graph or changed render timestamp");
        std::uint32_t expectedSeed=1u;
        const auto prepared=QuestVr::PrepareOriginalDecalDirection({},expectedSeed);
        Require(prepared.draws==6u && expectedSeed==1030492215u,"Independent prepared-direction evidence changed");
        const auto quad=QuestVr::BuildOriginalDecalPrimaryQuad({0,0,0},{0,0,0},{0,0,1},32,1.0f,prepared);
        expectQuad(first.nativeDecals->state.surfaces[0][0],quad);
        GC::Collect(); Require(fixture.Snapshot("FirstNativeAfterGc")==firstBytes,
            "Garbage collection retired the map-owned native decal or its real owner");
        Require(ValidatePortableRuntimeState((fixture.directory/"FirstNativeAttachment.sav").string(),"SpawnFixture"),
            "Production native attachment failed typed cold checkpoint preflight");
        fixture.Reset(); Require(LoadPortableRuntimeState((fixture.directory/"FirstNativeAttachment.sav").string()) &&
            fixture.Snapshot("FirstNativeRestore")==firstBytes,"Native Attach graph failed canonical replacement restore");
        fixture.Reset(); Call("Decal0","SetTexture",{Object("Texture0")});
        const auto supplied=Call("Decal0","AttachDecal",{{Value::Float(100.0f),{}},Vector({0.1f,0.1f,0.0f})});
        Require(supplied.value.kind==Kind::Bool && QuestVr::Vm::ToBool(supplied.value),
            "Supplied-direction native attachment failed a real authored polygon");
        const auto suppliedState=Decode(fixture.Snapshot("SuppliedNativeAttachment")); expectGraph(suppliedState,1u,1u);
        Require(!suppliedState.randomSeed,"Nonzero supplied DecalDir drew or materialized the optional RNG stream");
        expectedSeed=1u;
        const auto suppliedDirection=QuestVr::PrepareOriginalDecalDirection({0.1f,0.1f,0.0f},expectedSeed);
        expectQuad(suppliedState.nativeDecals->state.surfaces[0][0],
            QuestVr::BuildOriginalDecalPrimaryQuad({0,0,0},{0,0,0},{0,0,1},32,1.0f,suppliedDirection));
        Call("Decal0","SetDecalLocation",{Vector({0.25f,0.0f,0.1f})});
        Same(Call("Decal0","AttachDecal",{{Value::Float(100.0f),{}},Vector({0.1f,0.1f,0.0f})}).value,
            Value::Bool(true),"Repeated native Attach silently suppressed a duplicate primary");
        const auto repeatedBytes=fixture.Snapshot("RepeatedNativeAttachment"); const auto repeated=Decode(repeatedBytes);
        expectGraph(repeated,2u,2u); Require(!repeated.randomSeed && Read("Decal0","SurfList").intArray==std::vector<std::int32_t>({0,0}),
            "Repeated supplied native attachment lost duplicate mirrors or advanced RNG");
        expectQuad(repeated.nativeDecals->state.surfaces[0][0],
            QuestVr::BuildOriginalDecalPrimaryQuad({0.25f,0,0},{0,0,0},{0,0,1},32,1.0f,suppliedDirection));
        expectQuad(repeated.nativeDecals->state.surfaces[0][1],
            QuestVr::BuildOriginalDecalPrimaryQuad({0,0,0},{0,0,0},{0,0,1},32,1.0f,suppliedDirection));
        const auto detachRevision=GetPortableRuntimeWorldRevision(); Call("Decal0","DetachDecal");
        const auto detachedBytes=fixture.Snapshot("RepeatedNativeDetached"); const auto detached=Decode(detachedBytes);
        expectGraph(detached,0u,0u);
        Require(Read("Decal0","SurfList").intArray.empty() && GetPortableRuntimeWorldRevision()==detachRevision+1u &&
            detached.randomSeed==repeated.randomSeed,"Reverse-pop native Detach retained repeated records/list or consumed RNG");
        Call("Decal0","DetachDecal"); Require(fixture.Snapshot("SecondNativeDetach")==detachedBytes &&
            GetPortableRuntimeWorldRevision()==detachRevision+1u,"Empty native Detach invented graph/property/revision changes");
        fixture.Reset(); Call("Decal0","SetTexture",{Object("Texture0")});
        refusedNative(fixture,"Positive floor attachment followed by unsupported instruction","AttachThenFail");
        QuestVr::Vm::Limits tight; tight.writes=1u;
        refusedNative(fixture,"Positive floor attachment followed by script write budget","AttachThenWrites",{},tight);
        // Remain below one VM Value even when scalar payload storage shrinks.
        tight={}; tight.retainedBytes=sizeof(Value)-1u;
        refusedNative(fixture,"Positive floor attachment retained byte budget","AttachDecal",{{Value::Float(100.0f),{}}},tight);
        const auto beforeRevoke=fixture.Snapshot("BeforePositiveNativeRevoke");
        const auto revokeRevision=GetPortableRuntimeWorldRevision(); const auto revokeGc=GC::GetStats();
        Value externalOut=Value::Integer(17); std::vector<std::int32_t> outWrites;
        auto output=std::make_shared<QuestVr::Vm::Reference>(); output->zero=Value::Integer(0);
        output->read=[&]() { return externalOut; };
        output->write=[&](const Value& value) {
            externalOut=value; outWrites.push_back(QuestVr::Vm::ToInt(value));
            if (QuestVr::Vm::ToInt(value)==73) lifetime->Revoke();
        };
        const auto revoked=ExecutePortableActorFunction(Path("Decal0"),"AttachThenOutWrite",{{Value{},output}});
        Require(!revoked.passed() && !revoked.committed && revoked.status==QuestVr::Vm::Status::Unsupported &&
            revoked.error.find("Native client lifetime revoked during script transaction")!=std::string::npos &&
            outWrites==std::vector<std::int32_t>{73,17} && externalOut.integer==17,
            "Client teardown after real positive Attach did not roll back the external OUT alias");
        Require(fixture.Snapshot("AfterPositiveNativeRevoke")==beforeRevoke &&
            GetPortableRuntimeWorldRevision()==revokeRevision && GC::GetStats().numObjects==revokeGc.numObjects &&
            GC::GetStats().memoryUsage==revokeGc.memoryUsage && !HasPortableRuntimeClient(),
            "Postgeometry renderer teardown leaked native graph, argument effects, RNG, revision or GC allocations"); ++refusals;
        ClearPortableRuntimeClient(); lifetime=bind();
        // This is a real clear ray above the same solid floor, not an early
        // client/Texture guard. It must retain six successful pretrace draws.
        Call("Decal0","SetDecalRotation",{Rotation({-16384,0,0})});
        const auto missRevision=GetPortableRuntimeWorldRevision();
        Same(Call("Decal0","AttachDecal",{{Value::Float(100.0f),{}}}).value,Value::Bool(false),
            "Native up-ray invented a solid floor hit");
        const auto miss=Decode(fixture.Snapshot("NativeFloorMiss"));
        Require(miss.randomSeed && *miss.randomSeed==1030492215u && !miss.nativeDecals &&
            Integer("Decal0","MultiDecalLevel")==4 && GetPortableRuntimeWorldRevision()==missRevision+1u,
            "Ordinary floor miss rolled back sampled RNG/clamp or fabricated a native graph");
        fixture.Reset(); Require(LoadPortableRuntimeState((fixture.directory/"RepeatedNativeAttachment.sav").string()) &&
            fixture.Snapshot("RepeatedNativeRestore")==repeatedBytes,"Ordered duplicate graph failed restore");
        ShutdownPortableRuntime();
        Require(InitializePortableRuntime({tables.core,tables.engine,tables.classes}).passed &&
            LoadPortableRuntimeMap(tables.map).passed &&
            LoadPortableRuntimeState((fixture.directory/"RepeatedNativeAttachment.sav").string()) &&
            fixture.Snapshot("RepeatedNativeColdRestore")==repeatedBytes && !HasPortableRuntimeClient(),
            "Cold native graph restore replayed geometry/RNG/callbacks or fabricated a renderer session");
        fixture.Reset(); fixture.UnchangedSources();
    }
    {
        Fixture fixture; const auto tables=Build(fixture,5u,6u);
        Require(InitializePortableRuntime({tables.core,tables.engine,tables.classes}).passed &&
            LoadPortableRuntimeMap(tables.map).passed,"Malformed late Bitmap fixture failed basic initialization");
        fixture.legacy=fixture.Snapshot("Legacy"); auto lifetime=bind();
        Call("Decal0","SetTexture",{Object("Texture0")});
        refusedNative(fixture,"Malformed late Bitmap.USize","AttachDecal",{{Value::Float(100.0f),{}}});
        fixture.Reset(); fixture.UnchangedSources();
    }
    {
        Fixture fixture; const auto tables=Build(fixture,6u,1u);
        Require(InitializePortableRuntime({tables.core,tables.engine,tables.classes}).passed &&
            LoadPortableRuntimeMap(tables.map).passed,"Generated native decal mover callback fixture failed initialization");
        fixture.legacy=fixture.Snapshot("Legacy"); auto lifetime=bind();
        const auto prepare = [&]() { fixture.Reset(); Call("Decal0","SetTexture",{Object("Texture0")}); };
        prepare();
        const auto revision=GetPortableRuntimeWorldRevision();
        Same(Call("Decal0","AttachDecal",{{Value::Float(100.0f),{}}}).value,Value::Bool(true),
            "Full native Attach failed its original mover/SetBase callback path");
        const auto callbackBytes=fixture.Snapshot("NativeMoverAttachment"); const auto callback=Decode(callbackBytes);
        expectGraph(callback,1u,1u);
        Same(Read("Decal0","Base"),Object("Owner").value,"Mover Attach based self on surface Actor instead of self.Owner");
        Same(Read("Decal0","Texture"),Object("Texture1").value,"Original SetBase callback Texture write was lost");
        Same(Read("Decal0","Location"),Value::Vector({1000,1000,100}),"SetBase callback Location write was discarded");
        Same(Read("Decal0","Rotation"),Value::Rotator({0,0,0}),"SetBase callback Rotation write was discarded");
        Same(Read("SurfaceMover","Rotation"),Value::Rotator({9,10,11}),"Mover callback rotation change was lost");
        Require(Read("Owner","ObservedSurfList").intArray==std::vector<std::int32_t>{0} &&
            Integer("Owner","ObservedMulti")==4 && Integer("Owner","ObservedRandom")==478 &&
            Integer("Decal0","BaseChangeTrace")==1 && Integer("Decal0","BaseChangeMulti")==5 &&
            Read("Decal0","BaseChangeSurfList").intArray==std::vector<std::int32_t>{0},
            "Native primary/clamp/RNG was not visible before Attach/BaseChange or callback order changed");
        Require(callback.randomSeed && *callback.randomSeed==752224798u &&
            Read("Decal0","DrawScale").floating==2.0f && Integer("Decal0","MultiDecalLevel")==5 &&
            Read("Decal0","LastRenderedTime").floating==27.5f && GetPortableRuntimeWorldRevision()==revision+1u,
            "Postcallback quad used stale radius/multi/seed, reapplied upper clamp or split the outer transaction");
        const auto& record=callback.nativeDecals->state.surfaces[0][0];
        Require(record.moverRotation.pitch==128 && record.moverRotation.yaw==512 && record.moverRotation.roll==-256,
            "Native primary record captured mover Rotation after, instead of before, SetBase callbacks");
        std::uint32_t seed=1u; const auto prepared=QuestVr::PrepareOriginalDecalDirection({},seed);
        expectQuad(record,QuestVr::BuildOriginalDecalPrimaryQuad({0,0,0},{0,0,0},{0,0,1},64,2.0f,prepared));
        prepare();
        Same(Call("Decal0","AttachDecal",{{Value::Float(100.0f),{}},Vector({0.1f,0.1f,0})}).value,Value::Bool(true),
            "Supplied-direction Attach failed live mover callbacks");
        const auto supplied=Decode(fixture.Snapshot("NativeMoverSupplied"));
        Require(supplied.randomSeed && *supplied.randomSeed==2745024u && Integer("Owner","ObservedRandom")==41,
            "Supplied-direction native Attach consumed hidden draws or overwrote its callback's first RNG draw");
        for (const auto mode : {2,3,4}) {
            prepare(); Call("Owner","SetCallbackMode",{{Value::Integer(mode),{}}});
            refusedNative(fixture,"Mover callback mode "+std::to_string(mode),"AttachDecal",{{Value::Float(100.0f),{}}});
        }
        prepare(); QuestVr::Vm::Limits tight; tight.writes=7u;
        refusedNative(fixture,"Positive mover attachment write budget","AttachDecal",{{Value::Float(100.0f),{}}},tight);
        prepare(); refusedNative(fixture,"Positive mover attachment followed by unsupported instruction","AttachThenFail");
        fixture.Reset(); Require(LoadPortableRuntimeState((fixture.directory/"NativeMoverAttachment.sav").string()) &&
            fixture.Snapshot("NativeMoverRestore")==callbackBytes,
            "Saved mover native record failed exact restore without replaying SetBase/RNG callbacks");
        GC::Collect(); Require(fixture.Snapshot("NativeMoverRestoreGc")==callbackBytes,
            "Restored surface-owned mover attachment lost its actual GC roots");
        fixture.Reset(); fixture.UnchangedSources();
    }
}
#include "portable_actor_order_birth_cases.inl"
void IncomingTravelCheckpointGuards(const Fixture& fixture,const Tables& tables) {
    const auto baselinePath=fixture.directory/"Legacy.sav";
    const auto baseline=ReadBytes(baselinePath);
    Require(baseline==fixture.legacy,"Incoming travel guard test lost its authored checkpoint baseline");
    const auto reload=[&](const PortableMapTravelSource* source) {
        Require(InitializePortableRuntime({tables.core,tables.engine,tables.classes}).passed,
            "Cannot reinitialize generated incoming travel checkpoint fixture");
        Require(LoadPortableRuntimeMap(tables.map,source).passed,
            "Cannot load actual generated Level with explicit incoming travel source");
        Require(ActorPaths(true).size()==tables.initialActors,
            "Incoming travel source changed generated authored actor publication");
    };
    // Ordinary authored checkpoints remain accepted both with absent source
    // capability and explicitly established no-incoming-map authority.
    Require(ValidatePortableRuntimeState(baselinePath.string(),"SpawnFixture")&&
        LoadPortableRuntimeState(baselinePath.string())&&fixture.Snapshot("IncomingAuthoredBaseline")==baseline,
        "Authored checkpoint round trip failed before incoming replacement controls");
    for(const auto nonempty:{false,true}) {
        PortableMapTravelSource source;source.replacement.emplace();
        if(nonempty)source.replacement->emplace_back(u"Player",u"class=SpawnClasses.Leaf name=ImportedLeaf");
        reload(&source);
        const auto suffix=nonempty?"Nonempty":"Empty";
        const auto existing=fixture.directory/(std::string("IncomingPreserve")+suffix+".sav");
        const auto missing=fixture.directory/(std::string("IncomingMissing")+suffix+".sav");
        const Bytes sentinel{'u','n','t','o','u','c','h','e','d','-',static_cast<std::uint8_t>(nonempty?'1':'0')};
        WriteBytes(existing,sentinel);
        std::filesystem::last_write_time(existing,std::filesystem::file_time_type::clock::now()-std::chrono::hours(2));
        const auto modified=std::filesystem::last_write_time(existing);
        const auto baselineModified=std::filesystem::last_write_time(baselinePath);
        Require(!std::filesystem::exists(missing),"Missing-output incoming checkpoint control already exists");
        const auto paths=ActorPaths(true);const auto order=ReadPortableWorldActorOrder();
        Require(order.available,"Incoming checkpoint controls do not own a real native Level actor order");
        const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
        const auto rejected=[&](const std::function<bool()>& operation,const char* label) {
            Require(!operation(),std::string("Incoming travel checkpoint unexpectedly accepted ")+label);
            // Inspect immediately, without a fresh collection/save/runtime
            // mutation that could conceal a rejected entry point's effects.
            const auto after=GC::GetStats();
            Require(after.numObjects==gc.numObjects&&after.memoryUsage==gc.memoryUsage,
                std::string("Incoming checkpoint refusal changed immediate GC state: ")+label);
            Require(GetPortableRuntimeWorldRevision()==revision&&ActorPaths(true)==paths&&ReadPortableWorldActorOrder()==order,
                std::string("Incoming checkpoint refusal changed actor/order/revision state: ")+label);
            Require(ReadBytes(existing)==sentinel&&std::filesystem::last_write_time(existing)==modified,
                std::string("Incoming checkpoint refusal changed existing output bytes/mtime: ")+label);
            Require(!std::filesystem::exists(missing),
                std::string("Incoming checkpoint refusal created a missing output: ")+label);
            Require(ReadBytes(baselinePath)==baseline&&std::filesystem::last_write_time(baselinePath)==baselineModified,
                std::string("Incoming checkpoint refusal changed its input save: ")+label);
            ++refusals;
        };
        rejected([&]{return SavePortableRuntimeState(existing.string());},"capture over existing output");
        rejected([&]{return SavePortableRuntimeState(missing.string());},"capture to missing output");
        rejected([&]{return ValidatePortableRuntimeState(baselinePath.string(),"SpawnFixture");},"read-only validation");
        rejected([&]{return LoadPortableRuntimeState(baselinePath.string());},"restore");
        fixture.UnchangedSources();
    }
    const PortableMapTravelSource noIncoming;
    for(const auto* source:{static_cast<const PortableMapTravelSource*>(nullptr),&noIncoming}) {
        reload(source);
        Require(ValidatePortableRuntimeState(baselinePath.string(),"SpawnFixture")&&
            LoadPortableRuntimeState(baselinePath.string())&&fixture.Snapshot(source?"IncomingExplicitNone":"IncomingAbsentSource")==baseline,
            "Returning to authored/no-incoming authority did not restore canonical checkpoint support");
    }
    fixture.Reset();fixture.UnchangedSources();
}
void Synthetic() {
    Fixture fixture; const auto tables = Build(fixture);
    Require(InitializePortableRuntime({tables.core, tables.engine, tables.classes}).passed && LoadPortableRuntimeMap(tables.map).passed, "Generated spawn runtime/map metadata initialization failed");
    Require(ActorPaths(true).size() == tables.initialActors, "Generated true Level published nonactors/holes or missed original actors"); fixture.legacy = fixture.Snapshot("Legacy"); Require(fixture.legacy[4u] == 3u, "Untouched generated spawn baseline invented persistent state");
    IncomingTravelCheckpointGuards(fixture,tables);
    const auto nestedPath=ExecutePortableActorFunction(Path("Driver"),"LoadedObjectPath");
    Require(nestedPath.passed() && nestedPath.value.kind==Kind::String && nestedPath.value.text=="Engine.Actor.Level" &&
        fixture.Snapshot("ReadonlyObjectString")==fixture.legacy,"ObjectToString lost a loaded property's complete nested path or changed state");
    ArgumentTests(fixture); SpawnTagTests(fixture); DefaultsTests(fixture); CallbackTests(fixture); OrderingTests(fixture); PersistenceTests(fixture);
    ActorOrderBirthTests(fixture,tables);
    CollisionIntegrationTests(fixture); AliasAndBoundaryGcTests(fixture); GetPlayerPawnTests(fixture); PawnListTests(fixture); AllActorsTests(fixture); AiIntegrationTests(fixture); fixture.UnchangedSources();
    Require(InitializePortableRuntime({tables.core, tables.engine, tables.classes}).passed && LoadPortableRuntimeMap(tables.map).passed && fixture.Snapshot("Reinitialized") == fixture.legacy, "Runtime reinitialization retained born objects/frozen defaults"); fixture.UnchangedSources();
}
} // namespace
int main() {
    try {
        const auto before = GC::GetStats().numObjects; Synthetic(); WorldStartupSynthetic(); PlayerLoginSynthetic(); LoadContextSynthetic(); DecalGuardSynthetic(); DecalNativeSynthetic(); GC::Collect(); Require(GC::GetStats().numObjects == before, "Generated Spawn test leaked rooted UObjects after shutdown");
        std::cout << "Generated actor Spawn integration: " << checks << " checks, " << refusals << " rejection controls; actual natives278/262/283/304/529/530/650/710/711/713/714/715/716/720, serialized Level/model, native PawnList and AI graph rollback, frozen defaults, callbacks/live foreach, birth/save/GC, read-only BSP region side/fallback controls, non-publishing world startup prefix/live-growth/deletion/order/attachment rollback, native AttachDecal exact guards/real finite polygon/provisional SurfList/ordered records/postcallback radius and RNG/mover Rotation/Detach/checkpoint/GC/late rollback. No rendered decal, AI processing or accepted campaign startup claim.\n"; return 0;
    } catch (const std::exception& error) { std::cerr << "Actor Spawn integration failed: " << error.what() << '\n'; return 1; }
}
