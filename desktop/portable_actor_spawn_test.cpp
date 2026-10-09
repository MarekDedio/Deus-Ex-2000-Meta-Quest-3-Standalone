#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_model_geometry.h"
#include "quest_script_state.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
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
            if (type == "ClassProperty") { Index(body, target); Index(body, secondary); }
            else if (type == "ObjectProperty" || type == "ByteProperty" || type == "StructProperty") Index(body, target);
            return body;
        }; return reference;
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
    std::size_t cursor{}; Require(Read32(bytes, cursor) == 0x53515844u, "Independent spawn checkpoint magic mismatch"); const auto version = Read32(bytes, cursor); Require(version >= 1u && version <= 9u, "Independent spawn checkpoint version mismatch");
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
Bytes Envelope(const Bytes& prefixSource, const QuestVr::ScriptSavedState& script, std::uint32_t version = 8u) {
    const auto prefix = PrefixSize(prefixSource); Bytes result(prefixSource.begin(), prefixSource.begin() + static_cast<std::ptrdiff_t>(prefix)); Replace32(result, 4u, version);
    const auto blob = QuestVr::EncodeScriptSavedState(script); U32(result, static_cast<std::uint32_t>(blob.size())); Append(result, blob); return result;
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
void RefusedSave(const Fixture& fixture, const Bytes& prefix, const QuestVr::ScriptSavedState& script, const std::string& label, std::uint32_t version = 8u) {
    const auto current = fixture.Snapshot("BeforeBadSave"); const auto paths = ActorPaths(true); const auto revision = GetPortableRuntimeWorldRevision();
    const auto path = fixture.directory / (label + ".sav"); WriteBytes(path, Envelope(prefix, script, version));
    Require(!ValidatePortableRuntimeState(path.string(), "SpawnFixture") && !LoadPortableRuntimeState(path.string()), "Invalid birth manifest validated/loaded: " + label);
    Require(fixture.Snapshot("AfterBadSave") == current && ActorPaths(true) == paths && GetPortableRuntimeWorldRevision() == revision, "Invalid birth manifest partially published: " + label); ++refusals;
}

struct Tables { PortablePackageTables core, engine, classes, map; std::size_t initialActors{}; };
Tables Build(Fixture& fixture) {
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
    core.Property("ObjectFlags", "IntProperty", object, corePackage);
    core.ClassBody(object); core.ClassBody(metaclass); core.StructBody(vector); core.StructBody(rotator); core.StructBody(record); const auto coreTable = fixture.Write(core);

    Package engine; engine.stem = "Engine"; const auto engineCore = engine.ImportPackage("Core"), importedObject = engine.Import("Object", engineCore), importedClass = engine.Import("Class", engineCore);
    const auto importedVector = engine.Import("Vector", importedObject, "Struct"), importedRotator = engine.Import("Rotator", importedObject, "Struct");
    const auto actor = engine.Export("Actor", 0, importedObject), zone = engine.Export("ZoneInfo", 0, actor), levelInfo = engine.Export("LevelInfo", 0, zone);
    const auto level = engine.Export("Level", 0, importedObject), model = engine.Export("Model", 0, importedObject), brush = engine.Export("Brush", 0, actor), pawn = engine.Export("Pawn", 0, actor);
    const auto player = engine.Export("Player", 0, importedObject), viewport = engine.Export("Viewport", 0, player);
    const auto playerPawn = engine.Export("PlayerPawn", 0, pawn);
    const auto decoration = engine.Export("Decoration", 0, actor);
    const auto notify = engine.Export("SpawnNotify", 0, actor), pointRegion = engine.Export("PointRegion", engine.Import("Struct", engineCore), 0, actor);
    engine.Property("Zone", "ObjectProperty", pointRegion, engineCore, 1u, zone); engine.Property("iLeaf", "IntProperty", pointRegion, engineCore); engine.Property("ZoneNumber", "ByteProperty", pointRegion, engineCore);
    engine.StructBody(pointRegion);
    for (const auto& name : {"Owner", "Base"}) engine.Property(name, "ObjectProperty", actor, engineCore, 1u, actor);
    engine.Property("Level", "ObjectProperty", actor, engineCore, 1u, levelInfo); engine.Property("XLevel", "ObjectProperty", actor, engineCore, 1u, level);
    engine.Property("Instigator", "ObjectProperty", actor, engineCore, 1u, pawn); engine.Property("Brush", "ObjectProperty", actor, engineCore, 1u, brush);
    for (const auto& name : {"Location", "OldLocation"}) engine.Property(name, "StructProperty", actor, engineCore, 1u, importedVector);
    engine.Property("Rotation", "StructProperty", actor, engineCore, 1u, importedRotator); engine.Property("Region", "StructProperty", actor, engineCore, 1u, pointRegion);
    for (const auto& name : {"Tag", "AttachTag"}) engine.Property(name, "NameProperty", actor, engineCore);
    std::int32_t deleteFlag{};
    for (const auto& name : {"bTicked", "bCollideActors", "bBlockActors", "bBlockPlayers", "bCollideWorld", "bCollideWhenPlacing", "bStatic", "bNoDelete", "bDeleteMe", "bAnimByOwner"}) {
        const auto field = engine.Property(name, "BoolProperty", actor, engineCore);
        if (std::string(name) == "bDeleteMe") deleteFlag = field;
    }
    for (const auto& name : {"CollisionRadius", "CollisionHeight"}) engine.Property(name, "FloatProperty", actor, engineCore);
    engine.Property("Physics", "ByteProperty", actor, engineCore); engine.Property("StandingCount", "ByteProperty", actor, engineCore); engine.Property("Touching", "ObjectProperty", actor, engineCore, 4u, actor);
    engine.Property("bWaterZone", "BoolProperty", zone, engineCore); engine.Property("bBegunPlay", "BoolProperty", levelInfo, engineCore);
    engine.Property("SpawnNotify", "ObjectProperty", levelInfo, engineCore, 1u, notify);
    for (const auto& name : {"FootRegion", "HeadRegion"}) engine.Property(name, "StructProperty", pawn, engineCore, 1u, pointRegion);
    engine.Property("EyeHeight", "FloatProperty", pawn, engineCore);
    const auto isPlayer = engine.Property("bIsPlayer", "BoolProperty", pawn, engineCore);
    engine.Property("PlayerReplicationInfo", "ObjectProperty", pawn, engineCore, 1u, actor);
    const auto hiddenFlag = engine.Property("bHidden", "BoolProperty", actor, engineCore);
    const auto boundPlayer = engine.Property("Player", "ObjectProperty", playerPawn, engineCore, 1u, player);
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
    for (auto cls : {zone, levelInfo, level, model, brush, pawn, player, viewport, playerPawn, notify, decoration}) engine.ClassBody(cls);
    const auto engineTable = fixture.Write(engine);

    Package classes; classes.stem = "SpawnClasses"; const auto classesCore = classes.ImportPackage("Core"), classesEngine = classes.ImportPackage("Engine");
    const auto importedActor = classes.Import("Actor", classesEngine), classesObject = classes.Import("Object", classesCore), classesClass = classes.Import("Class", classesCore);
    const auto classesVector = classes.Import("Vector", classesObject, "Struct"), classesRotator = classes.Import("Rotator", classesObject, "Struct"), classesRecord = classes.Import("Record", classesObject, "Struct");
    const auto importedCount = classes.Import("Count", classesRecord, "IntProperty"), importedTarget = classes.Import("Target", classesRecord, "ObjectProperty");
    const auto probe = classes.Export("Probe", 0, importedActor), leaf = classes.Export("Leaf", 0, probe), abstract = classes.Export("Abstract", 0, probe);
    const auto fail = classes.Export("FailPost", 0, probe), nested = classes.Export("Nested", 0, probe), recursive = classes.Export("Recursive", 0, probe), selfDelete = classes.Export("SelfDelete", 0, probe);
    const auto pointBase = classes.Export("PointBase", 0, classes.Import("Decoration", classesEngine)), support = classes.Export("Support", 0, probe);
    const auto importedPawn = classes.Import("Pawn", classesEngine), importedPlayer = classes.Import("Player", classesEngine);
    const auto importedPlayerPawn = classes.Import("PlayerPawn", classesEngine);
    const auto importedAllActors = classes.Import("AllActors", importedActor, "Function");
    const auto importedTag = classes.Import("Tag", importedActor, "NameProperty");
    const auto importedXLevel = classes.Import("XLevel", importedActor, "ObjectProperty");
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
    const auto importedLocation = classes.Import("Location", importedActor, "StructProperty"), importedRotation = classes.Import("Rotation", importedActor, "StructProperty");
    const auto begun = classes.Import("bBegunPlay", importedLevelInfo, "BoolProperty");
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
    for (const auto& [name, digit] : std::vector<std::pair<std::string, int>>{{"Spawned", 1}, {"PreBeginPlay", 2}, {"BeginPlay", 3}, {"PostBeginPlay", 4}, {"SetInitialState", 5}, {"PostPostBeginPlay", 6}})
        classes.Function(name, probe, classesCore, Join({callbackTrace(digit), Return()}));
    const auto gained = classes.Function("GainedChild", probe, classesCore); classes.Property("Other", "ObjectProperty", gained, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(gained, Join({Assign(prop("ChildEvents"), Native(146u, {prop("ChildEvents"), Int(1)})), Return()}));
    const auto make = classes.Function("Make", probe, classesCore), inputClass = classes.Property("InputClass", "ClassProperty", make, classesCore, 1u, classesClass, 0x80u, importedActor);
    classes.FunctionCode(make, Return(Native(278u, {Ref(0x00u, inputClass)})));
    const auto makeOwned = classes.Function("MakeOwned", probe, classesCore), ownClass = classes.Property("InputClass", "ClassProperty", makeOwned, classesCore, 1u, classesClass, 0x80u, importedActor), own = classes.Property("InputOwner", "ObjectProperty", makeOwned, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(makeOwned, Return(Native(278u, {Ref(0x00u, ownClass), Ref(0x00u, own)})));
    const auto makeAt = classes.Function("MakeAt", probe, classesCore), atClass = classes.Property("InputClass", "ClassProperty", makeAt, classesCore, 1u, classesClass, 0x80u, importedActor);
    const auto atOwner = classes.Property("InputOwner", "ObjectProperty", makeAt, classesCore, 1u, importedActor, 0x80u), atTag = classes.Property("InputTag", "NameProperty", makeAt, classesCore, 1u, 0, 0x80u);
    const auto atLocation = classes.Property("InputLocation", "StructProperty", makeAt, classesCore, 1u, classesVector, 0x80u), atRotation = classes.Property("InputRotation", "StructProperty", makeAt, classesCore, 1u, classesRotator, 0x80u);
    classes.FunctionCode(makeAt, Return(Native(278u, {Ref(0x00u, atClass), Ref(0x00u, atOwner), Ref(0x00u, atTag), Ref(0x00u, atLocation), Ref(0x00u, atRotation)})));
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
    classes.Function("Destroyed", support, classesCore, Join({Assign(prop("NestedActor"), Native(278u, {Ref(0x20u, pointBase), Token(0x17u), Token(0x0bu), Ref(0x01u, importedLocation), Ref(0x01u, importedRotation)})), Return()}));
    Bytes defaults; classes.IntTag(defaults, "Counter", 10); for (std::uint8_t i = 0u; i < 3u; ++i) classes.IntTag(defaults, "Slots", 3 + 2 * i, i);
    Bytes recordDefault; U32(recordDefault, 7u); Index(recordDefault, 0); classes.Tag(defaults, "Payload", 10u, recordDefault, 0u, "Record"); classes.ClassBody(probe, defaults);
    Bytes overrides; classes.IntTag(overrides, "Counter", 20); classes.ClassBody(leaf, overrides); classes.ClassBody(abstract, {}, 1u);
    for (auto cls : {fail, nested, recursive, selfDelete}) classes.ClassBody(cls);
    Bytes pointDefaults; classes.FloatTag(pointDefaults, "CollisionRadius", 0.0f); classes.FloatTag(pointDefaults, "CollisionHeight", 0.0f);
    classes.BoolTag(pointDefaults, "bCollideWorld", true); classes.BoolTag(pointDefaults, "bCollideActors", false); classes.ClassBody(pointBase, pointDefaults);
    classes.ClassBody(support);
    for (auto cls : {fauxPlayer, pawnAlias, loopActor, loopBirth, loopDelete}) classes.ClassBody(cls);
    const auto classesTable = fixture.Write(classes);
    Require(GetPortableObjectPath(coreTable, recordCount) == "Object.Record.Count" && GetPortableObjectPath(coreTable, recordTarget) == "Object.Record.Target", "Generated frozen-struct metadata identity is wrong");

    Package map; map.stem = "SpawnFixture"; const auto mapEngine = map.ImportPackage("Engine"), mapClasses = map.ImportPackage("SpawnClasses");
    const auto world = map.Export("MyLevel", map.Import("Level", mapEngine)), mapModel = map.Export("Model0", map.Import("Model", mapEngine));
    const auto levelActor = map.Export("Level0", map.Import("LevelInfo", mapEngine)), driver = map.Export("Driver", map.Import("Probe", mapClasses));
    const auto owner = map.Export("Owner", map.Import("Support", mapClasses)), instigator = map.Export("Pawn0", map.Import("Pawn", mapEngine));
    const auto collision = map.Export("Leaf0", map.Import("Probe", mapClasses)); // Deliberate authored birth-name collision.
    const auto playerObject = map.Export("BindingPlayer", map.Import("Player", mapEngine));
    const auto viewportObject = map.Export("BindingViewport", map.Import("Viewport", mapEngine));
    map.ActorBody(playerObject, {}); map.ActorBody(viewportObject, {}); // Real UObjects, deliberately not Level actors.
    const auto detachedActor = map.Export("DetachedActor", map.Import("Probe", mapClasses));
    map.ActorBody(detachedActor, {}); // Real Actor class, deliberately absent from serialized Level slots.
    for (const auto reference : {levelActor, driver, owner, instigator, collision}) {
        Bytes properties; map.ObjectTag(properties, "Level", levelActor); map.ObjectTag(properties, "XLevel", world);
        if (reference == levelActor) map.BoolTag(properties, "bBegunPlay", true);
        if (reference == driver) { map.VectorTag(properties, "Location", {12.0f, -25.0f, 40.0f}); map.RotatorTag(properties, "Rotation", {128, 16384, -512}); map.BoolTag(properties, "bTicked", true); map.ObjectTag(properties, "Instigator", instigator); }
        map.ActorBody(reference, std::move(properties));
    }
    Bytes levelBody; Index(levelBody, 0); U32(levelBody, 7u); U32(levelBody, 7u);
    for (const auto reference : {levelActor, driver, 0, owner, instigator, collision, 0}) Index(levelBody, reference);
    for (unsigned i = 0u; i < 4u; ++i) Index(levelBody, 0);
    Index(levelBody, 0); U32(levelBody, 7777u); U32(levelBody, 0u); Index(levelBody, mapModel); Index(levelBody, 0); map.Body(world, std::move(levelBody));
    Bytes modelBody; Index(modelBody, 0); for (unsigned i = 0u; i < 6u; ++i) FloatBytes(modelBody, 0.0f); modelBody.push_back(0u);
    for (unsigned i = 0u; i < 4u; ++i) FloatBytes(modelBody, 0.0f);
    for (unsigned i = 0u; i < 5u; ++i) Index(modelBody, 0); // Vectors, points, nodes, surfaces, vertices.
    U32(modelBody, 0u); U32(modelBody, 1u); Index(modelBody, levelActor); U64(modelBody, 0u); U64(modelBody, 0u);
    for (unsigned i = 0u; i < 7u; ++i) Index(modelBody, 0); // Polys, lightmaps, bits, bounds, hulls, leaves, lights.
    U32(modelBody, 1u); U32(modelBody, 0u); map.Body(mapModel, std::move(modelBody));
    const auto mapTable = fixture.Write(map, true); const auto decodedModel = LoadPortableRootModel68(mapTable);
    Require(decodedModel.nodes.empty() && decodedModel.zones.size() == 1u && decodedModel.zones[0].actorReference == levelActor, "Genuine minimal map model is not decoded correctly");
    Require(ReadPortableLevel68ActorOrder(mapTable) == std::vector<std::int32_t>{levelActor, driver, 0, owner, instigator, collision, 0}, "Genuine Level actor ordering/holes were lost");
    return {coreTable, engineTable, classesTable, mapTable, 5u};
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
    Same(Read(first, "Owner"), Object("").value, "Omitted SpawnOwner did not mean None"); Same(Read(first, "Tag"), Name("Leaf").value, "Omitted SpawnTag did not use prospective class leaf");
    Same(Read(first, "Level"), Object("Level0").value, "Spawn did not bind actual LevelInfo"); Same(Read(first, "XLevel"), Object("MyLevel").value, "Spawn did not bind actual ULevel");
    Same(Read(first, "Instigator"), Object("Pawn0").value, "Spawn did not copy spawner Instigator"); Same(Read(first, "bTicked"), Value::Bool(true), "Spawn did not copy bTicked");
    Same(Read(first, "Brush"), Object("").value, "Spawn did not clear Brush"); Same(Read(first, "Class"), Class("Leaf").value, "Spawn UObject.Class not initialized");
    Same(Read(first, "Name"), Name(first.substr(first.find_last_of('.') + 1u)).value, "Spawn UObject.Name not initialized from unique identity");
    Same(Read(first, "ObjectFlags"), Value::Integer(0x4000), "Spawn UObject.ObjectFlags did not use Transient");
    Same(Read(first, "Outer"), Object("").value, "Spawn UObject.Outer did not copy ULevel.Outer");
    const auto region = Read(first, "Region"); Same(region.fields.at("zone"), Object("Level0").value, "Spawn Region fallback does not use real decoded LevelInfo zone");
    const auto omitted = Spawn("MakeOmitted"); Same(Read(omitted, "Location"), Read("Driver", "Location"), "Explicit omitted Location token became zero vector"); Same(Read(omitted, "Rotation"), Read("Driver", "Rotation"), "Explicit omitted Rotation token became zero rotator");
    const auto owned = Spawn("MakeOwned", {Class("Leaf"), Object("Owner")}); Same(Read(owned, "Owner"), Object("Owner").value, "SpawnOwner was ignored"); Require(Integer("Owner", "ChildEvents") == 1, "Owner did not receive synchronous GainedChild from birth");
    const auto at = Spawn("MakeAt", {Class("Leaf"), Object("Owner"), Name("CustomTag"), Vector({2.0f, 4.0f, 8.0f}), Rotation({-17, 32000, 255})});
    Same(Read(at, "Tag"), Name("CustomTag").value, "Explicit SpawnTag was not retained"); Same(Read(at, "Location"), Vector({2.0f, 4.0f, 8.0f}).value, "Explicit SpawnLocation was not retained"); Same(Read(at, "Rotation"), Rotation({-17, 32000, 255}).value, "Explicit SpawnRotation was not retained");
    const auto noneTag = Spawn("MakeAt", {Class("Leaf"), Object(""), Name("None"), Vector({0.0f, 0.0f, 0.0f}), Rotation({0, 0, 0})}); Same(Read(noneTag, "Tag"), Name("Leaf").value, "Explicit None SpawnTag did not fall back to class leaf");
    Same(Read(noneTag, "Location"), Vector({0.0f, 0.0f, 0.0f}).value, "Explicit zero location was incorrectly treated as omitted");
    RefusedCall(fixture, "MakeAt", {Class("Leaf"), Object("Owner"), Name("BadLocation"), Vector({std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}), Rotation({0, 0, 0})});
    fixture.Reset(); const auto resetBirth = Spawn(); Require(resetBirth == first, "Legacy reset did not restore deterministic name allocation/collision skipping"); fixture.Reset();
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
    Require(saved.births.size() == 2u && bytes[4u] == 8u && QuestVr::EncodeScriptSavedState(saved)[6u] == 5u, "Birth save did not select complete manifest/codec5/envelope8");
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
    Same(Call(self->path, "ReadCounter").value, Value::Integer(10), "Direct function on removed spawned UObject was refused");
    fixture.Reset(); Call("Driver", "SetBegun", {{Value::Bool(false), {}}}); const auto dormant = Spawn(); Require(Integer(dormant, "Trace") == 0, "Before-begun Spawn ran BeginPlay callbacks"); fixture.Reset();
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
    const auto pointAt = [&](std::array<float, 3> location, const std::string& expectedBase) {
        // This is a real Decoration spawn with bCollideWorld=true and PHYS_None.
        // Exact zero extents use the pin's CheckLocation world bypass, while
        // SpawnZone, begun callbacks and actor-hash InitBase actually execute.
        const auto point = Spawn("MakeAt", {Class("PointBase"), Object(""), Name("None"), Vector(location), Rotation({0, 0, 0})});
        Same(Read(point, "Base"), Object(expectedBase).value, "Point spawn chose wrong actual cached-registry InitBase target");
        Require(Integer(point, "BegunEvents") == 1 && Integer(point, "BaseEvents") == (expectedBase.empty() ? 0 : 1),
            "Point spawn skipped BeginPlay/native BaseChange callbacks or invented a base");
        return point;
    };
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
    pointAt({0.0f, 0.0f, 0.0f}, "");
    Same(Call("Owner", "SetCollisionSize", {{Value::Float(12.0f), {}}, {Value::Float(20.0f), {}}}).value,
        Value::Bool(true), "Cannot restore positive dimensions before collision enable");
    Call("Owner", "SetCollision", {boolean(true), boolean(true), boolean(false)});
    Require(QuestVr::Vm::ToBool(Read("Owner", "bCollideActors")) && QuestVr::Vm::ToBool(Read("Owner", "bBlockActors")) &&
        !QuestVr::Vm::ToBool(Read("Owner", "bBlockPlayers")), "Actual native262 did not publish all requested collision flags");
    pointAt({0.0f, 0.0f, 0.0f}, "Owner");
    Call("Owner", "SetCollision", {{Value{}, {}}, {Value{}, {}}, boolean(true)});
    Call("Owner", "SetCollision");
    Require(QuestVr::Vm::ToBool(Read("Owner", "bCollideActors")) && QuestVr::Vm::ToBool(Read("Owner", "bBlockActors")) &&
        QuestVr::Vm::ToBool(Read("Owner", "bBlockPlayers")), "Native262 omitted arguments did not preserve current flags");

    RefusedCall(fixture, "MoveDirectThenFail", {Vector({512.0f, 0.0f, 0.0f})}, {}, "Owner");
    pointAt({0.0f, 0.0f, 0.0f}, "Owner"); pointAt({512.0f, 0.0f, 0.0f}, "");
    RefusedCall(fixture, "ResizeThenFail", {}, {}, "Owner");
    pointAt({8.0f, 0.0f, 0.0f}, "Owner");
    RefusedCall(fixture, "DisableThenFail", {}, {}, "Owner");
    pointAt({0.0f, 0.0f, 0.0f}, "Owner");
    RefusedCall(fixture, "DestroyThenFail", {}, {}, "Owner");
    pointAt({0.0f, 0.0f, 0.0f}, "Owner");
    Same(Read("Owner", "NestedActor"), Object("").value, "Rolled-back Destroy retained callback-spawned child reference");
    Require(!QuestVr::Vm::ToBool(Read("Owner", "bDeleteMe")), "Rolled-back Destroy retained deletion flag");

    Call("Owner", "MoveDirect", {Vector({512.0f, 0.0f, 0.0f})});
    pointAt({512.0f, 0.0f, 0.0f}, ""); pointAt({0.0f, 0.0f, 0.0f}, "");
    Same(Call("Owner", "SetCollisionSize", {{Value::Float(12.0f), {}}, {Value::Float(20.0f), {}}}).value,
        Value::Bool(true), "Native283 failed Remove/assign/Add success");
    pointAt({512.0f, 0.0f, 0.0f}, "Owner");
    Call("Owner", "SetCollision", {boolean(false)});
    Require(!QuestVr::Vm::ToBool(Read("Owner", "bCollideActors")) && QuestVr::Vm::ToBool(Read("Owner", "bBlockActors")) &&
        QuestVr::Vm::ToBool(Read("Owner", "bBlockPlayers")), "Native262 false/omitted flags were conflated");
    pointAt({512.0f, 0.0f, 0.0f}, ""); Call("Owner", "SetCollision", {boolean(true)});
    pointAt({512.0f, 0.0f, 0.0f}, "Owner");
    Require(QuestVr::Vm::ToBool(Call("Owner", "Die").value), "Registered support native Destroy refused");
    const auto duringDestroy = Read("Owner", "NestedActor").text;
    Require(!duringDestroy.empty() && Published(duringDestroy) && Integer(duringDestroy, "BegunEvents") == 1,
        "Support Destroyed callback did not really spawn a begun PointBase actor");
    Same(Read(duringDestroy, "Base"), Object("").value, "Destroy removed registration after, not before, Destroyed callback's Spawn");
    Require(Integer(duringDestroy, "BaseEvents") == 0, "Destroy callback's point unexpectedly based on removed support");
    pointAt({512.0f, 0.0f, 0.0f}, "");
    const auto dead = fixture.Snapshot("CollisionDeadSupport"); fixture.Reset();
    Require(LoadPortableRuntimeState((fixture.directory / "CollisionDeadSupport.sav").string()) && fixture.Snapshot("CollisionDeadSupportRestore") == dead,
        "Destroyed support native topology failed cold fresh-registry restore");
    pointAt({512.0f, 0.0f, 0.0f}, "");

    fixture.Reset(); Call("Owner", "SetCollision", {boolean(true)});
    Call("Owner", "MoveDirect", {Vector({512.0f, 0.0f, 0.0f})}); pointAt({512.0f, 0.0f, 0.0f}, "");
    const auto stale = fixture.Snapshot("StaleCollisionRegistration");
    // The pin does not save native cached hash state; LinkActorsToLevel rebuilds
    // it from saved reflected fields. Cold/repeated loads must do the same,
    // replacing both old memberships and old runtime-born pointers atomically.
    fixture.Reset();
    for (unsigned iteration = 0u; iteration < 3u; ++iteration) {
        Require(LoadPortableRuntimeState((fixture.directory / "StaleCollisionRegistration.sav").string()) &&
            fixture.Snapshot("FreshCollisionRegistrationRestore") == stale,
            "Fresh save registry normalization changed serialized script/birth state");
        pointAt({512.0f, 0.0f, 0.0f}, "Owner");
        Call("Owner", "SetCollision", {boolean(false)});
    }
    fixture.Reset(); pointAt({0.0f, 0.0f, 0.0f}, ""); fixture.Reset();
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
        Require(LoadPortableRuntimeState((fixture.directory / "BoundaryBirthGraph.sav").string()), "Repeated successful v8 replacement failed");
        const auto current = GC::GetStats();
        Require(current.numObjects == stable.numObjects && current.memoryUsage == stable.memoryUsage,
            "Successful v8 replacement retained unrooted prior birth allocations at public boundary");
    }
    auto malformed = Decode(canonical);
    Record(malformed, first).properties.push_back({"SpawnClasses.Probe.Counter", "Counter", 0u, Value::Float(9.0f)});
    const auto malformedPath = fixture.directory / "LateStagedPropertyFailure.sav"; WriteBytes(malformedPath, Envelope(canonical, malformed));
    for (unsigned iteration = 0u; iteration < 12u; ++iteration) {
        const auto beforeRevision = GetPortableRuntimeWorldRevision();
        Require(!LoadPortableRuntimeState(malformedPath.string()), "Late invalid actor overlay after staged birth allocation was accepted");
        const auto current = GC::GetStats();
        Require(current.numObjects == stable.numObjects && current.memoryUsage == stable.memoryUsage && GetPortableRuntimeWorldRevision() == beforeRevision,
            "Failed staged v8 load retained unrooted births or advanced world revision at public boundary");
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
        RefusedSave(fixture, canonical, invalid, label, 9u);
        Require(GC::GetStats().numObjects == beforeGc.numObjects && GC::GetStats().memoryUsage == beforeGc.memoryUsage,
            "Rejected symbolic AI graph retained staged native allocations");
    };
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

void Synthetic() {
    Fixture fixture; const auto tables = Build(fixture);
    Require(InitializePortableRuntime({tables.core, tables.engine, tables.classes}).passed && LoadPortableRuntimeMap(tables.map).passed, "Generated spawn runtime/map metadata initialization failed");
    Require(ActorPaths(true).size() == tables.initialActors, "Generated true Level published nonactors/holes or missed original actors"); fixture.legacy = fixture.Snapshot("Legacy"); Require(fixture.legacy[4u] == 3u, "Untouched generated spawn baseline invented persistent state");
    ArgumentTests(fixture); DefaultsTests(fixture); CallbackTests(fixture); OrderingTests(fixture); PersistenceTests(fixture);
    CollisionIntegrationTests(fixture); AliasAndBoundaryGcTests(fixture); GetPlayerPawnTests(fixture); AllActorsTests(fixture); AiIntegrationTests(fixture); fixture.UnchangedSources();
    Require(InitializePortableRuntime({tables.core, tables.engine, tables.classes}).passed && LoadPortableRuntimeMap(tables.map).passed && fixture.Snapshot("Reinitialized") == fixture.legacy, "Runtime reinitialization retained born objects/frozen defaults"); fixture.UnchangedSources();
}
} // namespace
int main() {
    try {
        const auto before = GC::GetStats().numObjects; Synthetic(); GC::Collect(); Require(GC::GetStats().numObjects == before, "Generated Spawn test leaked rooted UObjects after shutdown");
        std::cout << "Generated actor Spawn integration: " << checks << " checks, " << refusals << " rejection controls; actual natives278/262/283/304/650/710/711/713/714/715/716/720, serialized Level/model, native AI graph/history/rollback, frozen defaults, nested callbacks/live foreach rollback, InitBase, PlayerPawn fallback bindings, birth graph/save replacement/GC. No AI processing/campaign startup claim.\n"; return 0;
    } catch (const std::exception& error) { std::cerr << "Actor Spawn integration failed: " << error.what() << '\n'; return 1; }
}
