#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_script_state.h"
#include "quest_script_dispatch.h"

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

// Actual generated UE1 metadata and bytecode through the production runtime.
// No game installation, headset, actor Spawn, or user checkpoint is required.
namespace {
using Bytes = std::vector<std::uint8_t>;
using Value = QuestVr::Vm::Value;
using Kind = QuestVr::Vm::Kind;
using Evaluation = QuestVr::Vm::Evaluation;
std::size_t checks{}, refusals{};
void Require(bool condition, const std::string& description) {
    if (!condition) throw std::runtime_error(description);
    ++checks;
}
void U16(Bytes& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
}
void U32(Bytes& bytes, std::uint32_t value) {
    U16(bytes, static_cast<std::uint16_t>(value)); U16(bytes, static_cast<std::uint16_t>(value >> 16u));
}
void U64(Bytes& bytes, std::uint64_t value) {
    U32(bytes, static_cast<std::uint32_t>(value)); U32(bytes, static_cast<std::uint32_t>(value >> 32u));
}
void Index(Bytes& bytes, std::int32_t value) {
    auto rest = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((rest & 63u) | (value < 0 ? 128u : 0u)); rest >>= 6u;
    if (rest) first |= 64u;
    bytes.push_back(first);
    while (rest) {
        auto next = static_cast<std::uint8_t>(rest & 127u); rest >>= 7u;
        if (rest) next |= 128u;
        bytes.push_back(next);
    }
}
void Replace32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8u * i));
}
void Append(Bytes& bytes, const Bytes& other) { bytes.insert(bytes.end(), other.begin(), other.end()); }
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    Require(size <= 4u * 1024u * 1024u, "Generated lifecycle file exceeds independent read bound");
    Bytes bytes(static_cast<std::size_t>(size)); std::ifstream stream(path, std::ios::binary);
    Require(static_cast<bool>(stream), "Cannot inspect generated lifecycle file");
    if (!bytes.empty()) stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(stream), "Generated lifecycle file was truncated"); return bytes;
}
void WriteBytes(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    Require(static_cast<bool>(stream), "Cannot create generated lifecycle file");
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(stream), "Cannot write generated lifecycle file");
}
struct Code { Bytes raw; std::size_t logical{}; };
Code Token(std::uint8_t token) { return {{token}, 1u}; }
Code Ref(std::uint8_t token, std::int32_t reference) { Code code{{token}, 5u}; Index(code.raw, reference); return code; }
Code Int(std::int32_t value) {
    Code code{{0x1du}, 5u}; U32(code.raw, static_cast<std::uint32_t>(value)); return code;
}
Code Join(std::initializer_list<Code> parts) {
    Code code; for (const auto& part : parts) { Append(code.raw, part.raw); code.logical += part.logical; } return code;
}
Code Assign(Code left, Code right) { return Join({Token(0x0fu), std::move(left), std::move(right)}); }
Code Return(Code value = Token(0x0bu)) { return Join({Token(0x04u), std::move(value)}); }
Code Native(std::uint16_t index, std::initializer_list<Code> arguments = {}) {
    Code code;
    if (index >= 256u) {
        code.raw = {static_cast<std::uint8_t>(0x60u + (index >> 8u)), static_cast<std::uint8_t>(index)};
        code.logical = 2u;
    } else { code.raw = {static_cast<std::uint8_t>(index)}; code.logical = 1u; }
    for (const auto& argument : arguments) { Append(code.raw, argument.raw); code.logical += argument.logical; }
    code.raw.push_back(0x16u); ++code.logical; return code;
}
Code Context(Code receiver, Code expression) {
    Require(expression.logical <= std::numeric_limits<std::uint16_t>::max(), "Generated Context exceeds logical skip range");
    Code code = Join({Token(0x19u), std::move(receiver)});
    U16(code.raw, static_cast<std::uint16_t>(expression.logical)); code.raw.push_back(0u); code.logical += 3u;
    Append(code.raw, expression.raw); code.logical += expression.logical; return code;
}
Code If(Code condition, Code body, std::size_t start = 0u) {
    Code code{{0x07u}, 3u};
    const auto target = start + code.logical + condition.logical + body.logical;
    Require(target <= std::numeric_limits<std::uint16_t>::max(), "Generated conditional exceeds logical offset range");
    U16(code.raw, static_cast<std::uint16_t>(target));
    Append(code.raw, condition.raw); code.logical += condition.logical;
    Append(code.raw, body.raw); code.logical += body.logical; return code;
}

// Adapted from the established generated class-default/table fixtures. Every
// property/function has genuine field ownership and Children/Next links.
struct Package {
    std::string stem;
    std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
    std::vector<std::function<Bytes(std::int32_t, std::int32_t)>> builders;
    std::map<std::int32_t, std::vector<std::int32_t>> children;
    std::int32_t Name(const std::string& text) {
        const auto found = std::find(names.begin(), names.end(), text);
        if (found != names.end()) return static_cast<std::int32_t>(found - names.begin());
        names.push_back(text); return static_cast<std::int32_t>(names.size() - 1u);
    }
    std::int32_t Import(const std::string& name, std::int32_t outer, const std::string& type = "Class") {
        imports.push_back({Name("Core"), Name(type), outer, Name(name)}); return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) { return Import(name, 0, "Package"); }
    std::int32_t Export(const std::string& name, std::int32_t cls = 0, std::int32_t base = 0,
        std::int32_t outer = 0, bool field = false) {
        exports.push_back({cls, base, outer, Name(name), ObjectFlags{}, 0, -1}); builders.emplace_back();
        const auto reference = static_cast<std::int32_t>(exports.size());
        if (field) children[outer].push_back(reference);
        return reference;
    }
    void Tag(Bytes& bytes, const std::string& name, std::uint8_t type, const Bytes& value, std::uint8_t slot = 0u) {
        Require(value.size() <= 255u, "Generated tag exceeds one-byte fixture bound");
        Index(bytes, Name(name)); bytes.push_back(static_cast<std::uint8_t>(type | 0x50u | (slot ? 0x80u : 0u)));
        bytes.push_back(static_cast<std::uint8_t>(value.size())); if (slot) bytes.push_back(slot); Append(bytes, value);
    }
    void IntTag(Bytes& bytes, const std::string& name, std::int32_t value) {
        Bytes data; U32(data, static_cast<std::uint32_t>(value)); Tag(bytes, name, 2u, data);
    }
    void ByteTag(Bytes& bytes, const std::string& name, std::uint8_t value) {
        Tag(bytes, name, 1u, Bytes{value});
    }
    void BoolTag(Bytes& bytes, const std::string& name, bool value) {
        Index(bytes, Name(name)); bytes.push_back(static_cast<std::uint8_t>(3u | (value ? 0x80u : 0u)));
    }
    void ObjectTag(Bytes& bytes, const std::string& name, std::int32_t reference, std::uint8_t slot = 0u) {
        Bytes data; Index(data, reference); Tag(bytes, name, 5u, data, slot);
    }
    void ClassBody(std::int32_t reference, Bytes defaults = {}, std::uint64_t probes = ~std::uint64_t{}) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference, probes, defaults = std::move(defaults)](std::int32_t next, std::int32_t child) {
            const auto& entry = exports.at(static_cast<std::size_t>(reference - 1)); Bytes body;
            Index(body, entry.ObjBase); Index(body, next); Index(body, 0); Index(body, child); Index(body, entry.ObjName);
            U32(body, 0); U32(body, 0); U32(body, 0); U64(body, probes); U64(body, ~std::uint64_t{}); U16(body, 0xffffu); U32(body, 0);
            U32(body, 0); body.resize(body.size() + 16u, 0u); for (unsigned i = 0u; i < 4u; ++i) Index(body, 0);
            Append(body, defaults); Index(body, 0); return body;
        };
    }
    std::int32_t Property(const std::string& name, const std::string& type, std::int32_t owner,
        std::int32_t core, std::uint32_t dimension = 1u, std::int32_t target = 0, std::uint32_t flags = 0u) {
        const auto reference = Export(name, Import(type, core), 0, owner, true);
        builders.at(static_cast<std::size_t>(reference - 1)) = [type, dimension, target, flags](std::int32_t next, std::int32_t) {
            Bytes body{0u}; Index(body, 0); Index(body, next); U32(body, dimension); U32(body, flags); Index(body, 0);
            if(flags&0x20u)U16(body,0u); // Original Net field replication offset.
            if (type == "ObjectProperty" || type == "ByteProperty" || type == "StructProperty") Index(body, target);
            return body;
        }; return reference;
    }
    std::int32_t Function(const std::string& name, std::int32_t owner, std::int32_t core,
        Code code = {}, std::uint16_t native = 0u, std::uint32_t flags = 0u) {
        const auto reference = Export(name, Import("Function", core), 0, owner, true);
        FunctionCode(reference, std::move(code), native, flags); return reference;
    }
    void FunctionCode(std::int32_t reference, Code code, std::uint16_t native = 0u, std::uint32_t flags = 0u) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference, native, flags, code = std::move(code)](std::int32_t next, std::int32_t child) {
            Bytes body{0u}; Index(body, 0); Index(body, next); Index(body, 0); Index(body, child);
            Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName); U32(body, 0); U32(body, 0);
            U32(body, static_cast<std::uint32_t>(code.logical)); Append(body, code.raw); U16(body, native); body.push_back(0u);
            U32(body, flags ? flags : native ? 0x402u : 2u); return body;
        };
    }
    void ActorBody(std::int32_t reference, Bytes properties = {}) {
        properties.push_back(0u);
        builders.at(static_cast<std::size_t>(reference - 1)) = [properties = std::move(properties)](std::int32_t, std::int32_t) { return properties; };
    }
    Bytes Serialize() {
        std::map<std::int32_t, std::int32_t> next;
        for (const auto& [owner, list] : children) {
            (void)owner; for (std::size_t i = 1u; i < list.size(); ++i) next[list[i - 1]] = list[i];
        }
        Bytes bytes; U32(bytes, 0x9e2a83c1u); U16(bytes, 68u); U16(bytes, 0u); bytes.resize(56u, 0u);
        std::vector<Bytes> bodies; std::vector<std::int32_t> offsets;
        for (std::size_t i = 0u; i < exports.size(); ++i) {
            const auto reference = static_cast<std::int32_t>(i + 1u);
            const auto first = children.find(reference);
            auto body = builders[i] ? builders[i](next[reference], first == children.end() || first->second.empty() ? 0 : first->second.front()) : Bytes{};
            offsets.push_back(static_cast<std::int32_t>(bytes.size())); Append(bytes, body); bodies.push_back(std::move(body));
        }
        Replace32(bytes, 12u, static_cast<std::uint32_t>(names.size())); Replace32(bytes, 16u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& name : names) {
            Index(bytes, static_cast<std::int32_t>(name.size() + 1u)); bytes.insert(bytes.end(), name.begin(), name.end()); bytes.push_back(0u); U32(bytes, 0);
        }
        Replace32(bytes, 20u, static_cast<std::uint32_t>(exports.size())); Replace32(bytes, 24u, static_cast<std::uint32_t>(bytes.size()));
        for (std::size_t i = 0u; i < exports.size(); ++i) {
            const auto& entry = exports[i]; Index(bytes, entry.ObjClass); Index(bytes, entry.ObjBase); U32(bytes, static_cast<std::uint32_t>(entry.ObjOuter));
            Index(bytes, entry.ObjName); U32(bytes, static_cast<std::uint32_t>(entry.ObjFlags)); Index(bytes, static_cast<std::int32_t>(bodies[i].size()));
            if (!bodies[i].empty()) Index(bytes, offsets[i]);
        }
        Replace32(bytes, 28u, static_cast<std::uint32_t>(imports.size())); Replace32(bytes, 32u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& entry : imports) {
            Index(bytes, entry.ClassPackage); Index(bytes, entry.ClassName); U32(bytes, static_cast<std::uint32_t>(entry.ObjOuter)); Index(bytes, entry.ObjName);
        }
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
            const auto candidate = parent / ("deusex-actor-lifecycle-test-" + std::to_string(stamp) + '-' + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) { directory = std::filesystem::canonical(candidate); break; }
        }
        Require(!directory.empty() && directory.parent_path() == parent, "Generated lifecycle fixture escaped owned temporary parent");
        std::filesystem::create_directory(directory / "System"); std::filesystem::create_directory(directory / "Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime(); std::error_code error;
        const auto actual = std::filesystem::weakly_canonical(directory, error);
        if (!error && !directory.empty() && actual == directory && actual.parent_path() == parent &&
            actual.filename().string().rfind("deusex-actor-lifecycle-test-", 0u) == 0u) std::filesystem::remove_all(actual, error);
    }
    PortablePackageTables Write(Package& package, bool map = false) {
        const auto path = directory / (map ? "Maps" : "System") / (package.stem + (map ? ".dx" : ".u"));
        auto bytes = package.Serialize(); WriteBytes(path, bytes); sourceBytes.emplace(path, std::move(bytes));
        const auto table = LoadPortablePackageTables(path.string());
        Require(table.version == 68u && table.exports.size() == package.exports.size(), "Generated lifecycle package failed production table loader"); return table;
    }
    Bytes Snapshot(const std::string& name) const {
        const auto path = directory / (name + ".sav");
        Require(SavePortableRuntimeState(path.string()), "Cannot snapshot generated lifecycle runtime"); return ReadBytes(path);
    }
    void Reset() const {
        Require(LoadPortableRuntimeState((directory / "Legacy.sav").string()), "Cannot replace lifecycle state with generated legacy baseline");
        Require(Snapshot("ResetRoundTrip") == legacy, "Legacy replacement retained lifecycle/property state");
    }
    void UnchangedSources() const {
        for (const auto& [path, bytes] : sourceBytes) Require(ReadBytes(path) == bytes, "Lifecycle native modified generated package source");
    }
};
std::string Path(const std::string& actor) { return "LifecycleFixture." + actor; }
Evaluation Object(const std::string& actor) { return {Value::Text(Kind::Object, actor.empty() ? std::string{} : Path(actor)), {}}; }
Evaluation Integer(std::int32_t value) { return {Value::Integer(value), {}}; }
Evaluation Byte(std::uint8_t value) { return {Value::Byte(value), {}}; }
Evaluation Boolean(bool value) { return {Value::Bool(value), {}}; }
Evaluation Name(const std::string& value) { return {Value::Text(Kind::Name, value), {}}; }
QuestVr::Vm::Result Call(const std::string& actor, const std::string& function,
    const std::vector<Evaluation>& arguments = {}) {
    auto result = ExecutePortableActorFunction(Path(actor), function, arguments);
    Require(result.passed(), actor + '.' + function + " failed: " + result.error + " at " + result.function + ':' + std::to_string(result.offset)); return result;
}
Value Read(const std::string& actor, const std::string& property, std::uint32_t slot = 0u) {
    return ReadPortableActorScriptProperty(Path(actor), property, slot);
}
void Same(const Value& actual, const Value& expected, const std::string& description) {
    Require(QuestVr::Vm::Equal(actual, expected), description);
}
void HasObject(const std::string& actor, const std::string& property, const std::string& target,
    const std::string& description, std::uint32_t slot = 0u) {
    Same(Read(actor, property, slot), Object(target).value, description);
}
std::int32_t Count(const std::string& actor, const std::string& property) {
    return QuestVr::Vm::ToInt(Read(actor, property));
}
bool Published(const std::string& actor, bool includeInactive = false) {
    const auto actors = GetPortableRuntimeMapActors(includeInactive);
    return std::any_of(actors.begin(), actors.end(), [&](const auto& item) { return item.objectPath == Path(actor); });
}
void RefusedCall(const Fixture& fixture, const std::string& actor, const std::string& function,
    const std::vector<Evaluation>& arguments = {}, const QuestVr::Vm::Limits& limits = {}) {
    const auto before = fixture.Snapshot("BeforeRefusal");
    const auto revision = GetPortableRuntimeWorldRevision();
    const auto result = ExecutePortableActorFunction(Path(actor), function, arguments, limits);
    Require(!result.passed() && !result.committed, "Invalid lifecycle call silently committed: " + actor + '.' + function);
    Require(fixture.Snapshot("AfterRefusal") == before, "Failed lifecycle call changed canonical state: " + actor + '.' + function);
    Require(GetPortableRuntimeWorldRevision() == revision, "Failed lifecycle call advanced world publication revision: " + actor + '.' + function);
    GC::Collect();
    Require(fixture.Snapshot("AfterRefusalGC") == before && GetPortableRuntimeWorldRevision() == revision,
        "Failed lifecycle call changed state/revision across GC: " + actor + '.' + function);
    ++refusals;
}
std::uint32_t Read32(const Bytes& bytes, std::size_t& cursor) {
    Require(cursor <= bytes.size() && bytes.size() - cursor >= 4u, "Independent lifecycle checkpoint parser encountered truncation");
    std::uint32_t value{}; for (unsigned i = 0u; i < 4u; ++i) value |= static_cast<std::uint32_t>(bytes[cursor++]) << (8u * i); return value;
}
std::size_t PrefixSize(const Bytes& bytes) {
    std::size_t cursor{}; Require(Read32(bytes, cursor) == 0x53515844u, "Independent lifecycle runtime magic mismatch");
    const auto version = Read32(bytes, cursor); Require(version >= 1u && version <= 12u, "Independent lifecycle runtime envelope version mismatch");
    const auto skipString = [&]() {
        const auto size = Read32(bytes, cursor); Require(cursor <= bytes.size() && size <= bytes.size() - cursor, "Independent lifecycle string exceeds checkpoint"); cursor += size;
    };
    const auto skipStrings = [&]() { const auto count = Read32(bytes, cursor); for (std::uint32_t i = 0u; i < count; ++i) skipString(); };
    skipStrings(); skipStrings(); skipStrings();
    if (version >= 2u) {
        static_cast<void>(Read32(bytes, cursor)); const auto count = Read32(bytes, cursor);
        for (std::uint32_t i = 0u; i < count; ++i) { skipString(); static_cast<void>(Read32(bytes, cursor)); }
    }
    if (version >= 3u) {
        static_cast<void>(Read32(bytes, cursor)); static_cast<void>(Read32(bytes, cursor));
        for (unsigned i = 0u; i < 4u; ++i) skipStrings();
    }
    return cursor;
}
QuestVr::ScriptSavedState Decode(const Bytes& bytes) {
    auto cursor = PrefixSize(bytes); const auto size = Read32(bytes, cursor);
    Require(size == bytes.size() - cursor, "Independent lifecycle trailer size mismatch");
    return QuestVr::DecodeScriptSavedState(Bytes(bytes.begin() + static_cast<std::ptrdiff_t>(cursor), bytes.end()));
}
Bytes Envelope(const Bytes& prefixSource, const QuestVr::ScriptSavedState& script, std::uint32_t version = 0u) {
    const auto prefix = PrefixSize(prefixSource);
    Bytes bytes(prefixSource.begin(), prefixSource.begin() + static_cast<std::ptrdiff_t>(prefix));
    const auto blob = QuestVr::EncodeScriptSavedState(script); Replace32(bytes, 4u, version ? version : 3u + blob.at(6u));
    U32(bytes, static_cast<std::uint32_t>(blob.size())); Append(bytes, blob); return bytes;
}
Bytes OldEmptyEnvelope(std::uint32_t version) {
    Require(version >= 1u && version <= 3u, "Generated old lifecycle envelope version is invalid");
    Bytes bytes; U32(bytes, 0x53515844u); U32(bytes, version);
    for (unsigned i = 0u; i < 3u; ++i) U32(bytes, 0u);
    if (version >= 2u) {
        const float health = 100.0f; std::uint32_t bits{}; std::memcpy(&bits, &health, 4u);
        U32(bytes, bits); U32(bytes, 0u);
    }
    if (version >= 3u) for (unsigned i = 0u; i < 6u; ++i) U32(bytes, 0u);
    return bytes;
}
QuestVr::ScriptSavedObject& Record(QuestVr::ScriptSavedState& state, const std::string& actor) {
    const auto found = std::find_if(state.objects.begin(), state.objects.end(), [&](const auto& object) { return object.path == Path(actor); });
    if (found == state.objects.end()) throw std::runtime_error("Generated lifecycle checkpoint lacks actor " + actor);
    return *found;
}
const QuestVr::ScriptSavedActorLifecycle& Life(const QuestVr::ScriptSavedState& state, const std::string& actor) {
    const auto found = std::find_if(state.objects.begin(), state.objects.end(), [&](const auto& object) { return object.path == Path(actor); });
    Require(found != state.objects.end() && found->lifecycle.has_value(), "Generated checkpoint lacks native lifecycle for " + actor); return *found->lifecycle;
}
bool EmptyChildren(const QuestVr::ScriptSavedState& state, const std::string& actor) {
    const auto found = std::find_if(state.objects.begin(), state.objects.end(), [&](const auto& object) { return object.path == Path(actor); });
    // These generated parents have no authored ChildActors baseline. A native
    // no-op need not invent a lifecycle record merely to describe an empty list.
    return found == state.objects.end() || !found->lifecycle || found->lifecycle->children.empty();
}
void EmptyLegacyBases(const QuestVr::ScriptSavedState& state) {
    for (const auto& object : state.objects)
        Require(!object.lifecycle || object.lifecycle->basedActors.empty(),
            "Canonical capture revived retired reverse Base authority: " + object.path);
}
void SetSavedProperty(QuestVr::ScriptSavedObject& object, const std::string& name, const std::string& key,
    Value value, std::uint32_t slot = 0u) {
    const auto found = std::find_if(object.properties.begin(), object.properties.end(), [&](const auto& property) { return property.name == name && property.index == slot; });
    if (found == object.properties.end()) object.properties.push_back({key, name, slot, std::move(value)});
    else found->value = std::move(value);
}
void LoadScript(const Fixture& fixture, const Bytes& prefix, const QuestVr::ScriptSavedState& script, const std::string& label,
    std::uint32_t version = 0u) {
    const auto path = fixture.directory / (label + ".sav"); WriteBytes(path, Envelope(prefix, script, version));
    Require(ValidatePortableRuntimeState(path.string(), "LifecycleFixture"), "Generated lifecycle payload did not validate: " + label);
    Require(LoadPortableRuntimeState(path.string()), "Generated lifecycle payload did not load: " + label);
}
void OwnNullCodeFrame(const Fixture& fixture, const std::string& actor) {
    Require(!ReadPortableActorSerializedStack(Path(actor)), "Generated frameless actor unexpectedly has an authored stack");
    QuestVr::ScriptSavedState script; script.mapName = "LifecycleFixture";
    QuestVr::ScriptSavedObject object; object.path = Path(actor); object.classPath = "LifecycleClasses.Probe";
    object.state.emplace(); object.state->hasStack = true; object.state->frameOverride = true;
    object.state->frame.emplace(); // Generated modern InitExecution-shaped frame, not invented bytecode.
    object.state->frame->codePath = object.classPath;
    object.state->frame->control = QuestVr::StateControl{object.classPath, false, ~std::uint64_t{0u}};
    script.objects.push_back(std::move(object));
    LoadScript(fixture, fixture.legacy, script, "OwnedNullCodeFrame" + actor);
    const auto state = ReadPortableActorStateObject(Path(actor));
    Require(state && state->hasStack && state->frameOverride && state->frame && state->frame->codePath == "LifecycleClasses.Probe" &&
        state->frame->control && state->frame->control->selectedPath == "LifecycleClasses.Probe" && !state->frame->control->codePresent &&
        state->frame->control->liveProbeMask == ~std::uint64_t{0u} && state->disabled.empty() &&
        state->frame->localsCodePath.empty() && state->frame->locals.empty(), "Typed checkpoint did not establish the requested modern owned null-code frame");
    Require(!ReadPortableActorSerializedStack(Path(actor)), "Owned portable frame altered immutable authored stack metadata");
}
bool ProbeEnabled(const std::string& actor, const std::string& event) {
    const auto context = ReadPortableActorDispatchContext(Path(actor));
    Require(context.liveProbeMask.has_value() && context.disabledNames.empty(),
        "Modern owned frame lost its positive probe mask or invented a legacy negative gate");
    return QuestVr::ScriptDispatch::IsEnabled(event, context.classProbeMask, context.codeMasks,
        context.disabledNames, context.liveProbeMask);
}
void RefusedScript(const Fixture& fixture, const Bytes& prefix, QuestVr::ScriptSavedState script, const std::string& label) {
    const auto before = fixture.Snapshot("BeforeBadCheckpoint");
    const auto revision = GetPortableRuntimeWorldRevision();
    const auto path = fixture.directory / (label + ".sav");
    WriteBytes(path, Envelope(prefix, script));
    Require(!ValidatePortableRuntimeState(path.string(), "LifecycleFixture"), "Invalid lifecycle checkpoint validated: " + label);
    Require(!LoadPortableRuntimeState(path.string()), "Invalid lifecycle checkpoint loaded: " + label);
    Require(fixture.Snapshot("AfterBadCheckpoint") == before, "Invalid lifecycle checkpoint partially published: " + label); ++refusals;
    Require(GetPortableRuntimeWorldRevision() == revision, "Invalid lifecycle checkpoint advanced world revision: " + label);
}

struct Tables { PortablePackageTables core, engine, classes, map; };
Tables Build(Fixture& fixture) {
    Package core; core.stem = "Core"; const auto object = core.Export("Object"); core.ClassBody(object);
    const auto coreTable = fixture.Write(core);
    Package engine; engine.stem = "Engine";
    const auto engineCore = engine.ImportPackage("Core"), importedObject = engine.Import("Object", engineCore);
    const auto actor = engine.Export("Actor", 0, importedObject), levelInfo = engine.Export("LevelInfo", 0, actor);
    engine.Property("Owner", "ObjectProperty", actor, engineCore, 1u, actor, 0x22u);
    engine.Property("Instigator", "ObjectProperty", actor, engineCore, 1u, actor);
    engine.Property("Base", "ObjectProperty", actor, engineCore, 1u, actor, 0x22u);
    engine.Property("Level", "ObjectProperty", actor, engineCore, 1u, levelInfo, 0x2u);
    engine.Property("Touching", "ObjectProperty", actor, engineCore, 4u, actor);
    engine.Property("StandingCount", "ByteProperty", actor, engineCore, 1u, 0, 0x2u);
    for (const auto& name : {"bStatic", "bNoDelete", "bDeleteMe"}) engine.Property(name, "BoolProperty", actor, engineCore);
    engine.Property("bBegunPlay", "BoolProperty", levelInfo, engineCore);
    const auto ownerNative = engine.Function("SetOwner", actor, engineCore, {}, 272u);
    engine.Property("NewOwner", "ObjectProperty", ownerNative, engineCore, 1u, actor, 0x80u);
    for (const auto& name : {"LostChild", "GainedChild"}) {
        const auto event = engine.Function(name, actor, engineCore, Return(), 0u, 0x800u);
        engine.Property("Other", "ObjectProperty", event, engineCore, 1u, actor, 0x80u);
    }
    const auto baseNative = engine.Function("SetBase", actor, engineCore, {}, 298u);
    engine.Property("NewBase", "ObjectProperty", baseNative, engineCore, 1u, actor, 0x80u);
    const auto destroyNative = engine.Function("Destroy", actor, engineCore, {}, 279u);
    engine.Property("ReturnValue", "BoolProperty", destroyNative, engineCore, 1u, 0, 0x480u);
    engine.ClassBody(actor); engine.ClassBody(levelInfo); const auto engineTable = fixture.Write(engine);

    Package classes; classes.stem = "LifecycleClasses";
    const auto classesCore = classes.ImportPackage("Core"), classesEngine = classes.ImportPackage("Engine");
    const auto importedActor = classes.Import("Actor", classesEngine), importedLevel = classes.Import("LevelInfo", classesEngine);
    const auto probe = classes.Export("Probe", 0, importedActor);
    const auto recursive = classes.Export("Recursive", 0, probe), failOwner = classes.Export("FailOwner", 0, probe);
    const auto failBase = classes.Export("FailBase", 0, probe), failDestroy = classes.Export("FailDestroy", 0, probe);
    const auto failUnTouch = classes.Export("FailUnTouch", 0, probe);
    const auto countDefault = classes.Export("CountDefault", 0, probe);
    const auto countInherited = classes.Export("CountInherited", 0, countDefault);
    std::map<std::string, std::int32_t> properties;
    for (const auto& name : {"Trace", "Gained", "Lost", "Attached", "Detached", "BaseChanges", "DestroyedCalls", "Untouched", "Ticks", "StateValue"})
        properties.emplace(name, classes.Property(name, "IntProperty", probe, classesCore));
    for (const auto& name : {"Journal", "RedirectOwner", "ObservedOwner", "ObservedBase", "LastArgument", "Link", "RedirectBase"})
        properties.emplace(name, classes.Property(name, "ObjectProperty", probe, classesCore, 1u, importedActor));
    properties.emplace("RedirectLevel", classes.Property("RedirectLevel", "ObjectProperty", probe, classesCore, 1u, importedLevel));
    properties.emplace("ObservedStanding", classes.Property("ObservedStanding", "ByteProperty", probe, classesCore));
    for (const auto& name : {"RedirectOnce", "RedirectLostOnce", "ObservedDeleted", "RedirectBaseOnce", "ReenterBaseOnce", "RedirectLevelOnce"})
        properties.emplace(name, classes.Property(name, "BoolProperty", probe, classesCore));
    const auto ownerProperty = classes.Import("Owner", importedActor, "ObjectProperty");
    const auto baseProperty = classes.Import("Base", importedActor, "ObjectProperty");
    const auto levelProperty = classes.Import("Level", importedActor, "ObjectProperty");
    const auto standingProperty = classes.Import("StandingCount", importedActor, "ByteProperty");
    const auto deletedProperty = classes.Import("bDeleteMe", importedActor, "BoolProperty");
    const auto begunProperty = classes.Import("bBegunPlay", importedLevel, "BoolProperty");
    const auto property = [&](const std::string& name) { return Ref(0x01u, properties.at(name)); };
    const auto increment = [&](const std::string& name) { return Assign(property(name), Native(146u, {property(name), Int(1)})); };
    const auto trace = [&](int digit) {
        const auto target = [&]() { return Context(property("Journal"), property("Trace")); };
        return Assign(target(), Native(146u, {Native(144u, {target(), Int(10)}), Int(digit)}));
    };
    const auto event = [&](const std::string& name, const std::string& counter, int digit, std::int32_t observedProperty) {
        const auto function = classes.Function(name, probe, classesCore);
        const auto parameter = classes.Property("Other", "ObjectProperty", function, classesCore, 1u, importedActor, 0x80u);
        const auto argument = Ref(0x00u, parameter);
        Code code = Join({increment(counter), Assign(property("LastArgument"), argument), trace(digit)});
        if (observedProperty != 0) {
            const auto observed = observedProperty == ownerProperty ? "ObservedOwner" : "ObservedBase";
            code = Join({code, Assign(property(observed), Context(argument, Ref(0x01u, observedProperty)))});
        }
        if (name == "GainedChild") {
            const auto redirect = Join({Assign(property("RedirectOnce"), Token(0x28u)), Context(argument, Native(272u, {property("RedirectOwner")}))});
            code = Join({code, If(property("RedirectOnce"), redirect, code.logical)});
        }
        if (name == "LostChild") {
            const auto redirect = Join({Assign(property("RedirectLostOnce"), Token(0x28u)),
                Assign(Context(argument, Ref(0x01u, ownerProperty)), property("RedirectOwner"))});
            code = Join({code, If(property("RedirectLostOnce"), redirect, code.logical)});
        }
        if (name == "Attach" || name == "Detach") {
            code = Join({code, Assign(property("ObservedStanding"), Ref(0x01u, standingProperty))});
            const auto rawRedirect = Join({Assign(property("RedirectBaseOnce"), Token(0x28u)),
                Assign(Context(argument, Ref(0x01u, baseProperty)), property("RedirectBase"))});
            code = Join({code, If(property("RedirectBaseOnce"), rawRedirect, code.logical)});
            const auto levelRedirect = Join({Assign(property("RedirectLevelOnce"), Token(0x28u)),
                Assign(Context(argument, Ref(0x01u, levelProperty)), property("RedirectLevel"))});
            code = Join({code, If(property("RedirectLevelOnce"), levelRedirect, code.logical)});
            const auto reenter = Join({Assign(property("ReenterBaseOnce"), Token(0x28u)),
                Context(argument, Native(298u, {property("RedirectBase")}))});
            code = Join({code, If(property("ReenterBaseOnce"), reenter, code.logical)});
        }
        classes.FunctionCode(function, Join({code, Return()})); return function;
    };
    event("GainedChild", "Gained", 1, ownerProperty); event("LostChild", "Lost", 2, ownerProperty);
    event("Attach", "Attached", 3, baseProperty); event("Detach", "Detached", 4, baseProperty);
    event("UnTouch", "Untouched", 7, 0);
    classes.Function("BaseChange", probe, classesCore, Join({increment("BaseChanges"), trace(5), Assign(property("ObservedBase"), Ref(0x01u, baseProperty)), Return()}));
    classes.Function("Destroyed", probe, classesCore, Join({increment("DestroyedCalls"), trace(6), Assign(property("ObservedDeleted"), Ref(0x01u, deletedProperty)), Return()}));
    classes.Function("Tick", probe, classesCore, Join({increment("Ticks"), Return()}));
    for (const auto& [name, native] : std::vector<std::pair<std::string, std::uint16_t>>{{"Own", 272u}, {"BaseOn", 298u}}) {
        const auto function = classes.Function(name, probe, classesCore), parameter = classes.Property("Target", "ObjectProperty", function, classesCore, 1u, importedActor, 0x80u);
        classes.FunctionCode(function, Join({Native(native, {Ref(0x00u, parameter)}), Return()}));
        const auto fail = classes.Function(name + "ThenFail", probe, classesCore), input = classes.Property("Target", "ObjectProperty", fail, classesCore, 1u, importedActor, 0x80u);
        classes.FunctionCode(fail, Join({Native(native, {Ref(0x00u, input)}), Native(999u), Return()}));
    }
    classes.Function("Die", probe, classesCore, Return(Native(279u)));
    classes.Function("DieThenFail", probe, classesCore, Join({Native(279u), Native(999u), Return()}));
    classes.Function("DieThenWrite", probe, classesCore, Join({Native(279u), Assign(property("StateValue"), Int(77)), Return(Token(0x17u))}));
    classes.Function("AfterDelete", probe, classesCore, Join({Assign(property("StateValue"), Int(88)), Return(Token(0x17u))}));
    classes.Function("ClearDeleteFlag", probe, classesCore, Join({Assign(Ref(0x01u, deletedProperty), Token(0x28u)), Return()}));
    const auto setValue = classes.Function("SetValue", probe, classesCore), valueParameter = classes.Property("Input", "IntProperty", setValue, classesCore, 1u, 0, 0x80u);
    classes.FunctionCode(setValue, Join({Assign(property("StateValue"), Ref(0x00u, valueParameter)), Return()}));
    const auto keep = classes.Function("KeepReference", probe, classesCore), keepParameter = classes.Property("Target", "ObjectProperty", keep, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(keep, Join({Assign(property("Link"), Ref(0x00u, keepParameter)), Return(Context(property("Link"), property("StateValue")))}));
    const auto begun = classes.Function("SetBegun", probe, classesCore), begunParameter = classes.Property("Input", "BoolProperty", begun, classesCore, 1u, 0, 0x80u);
    classes.FunctionCode(begun, Join({Assign(Context(Ref(0x01u, levelProperty), Ref(0x01u, begunProperty)), Ref(0x00u, begunParameter)), Return()}));
    const auto redirect = classes.Function("RedirectNext", probe, classesCore), redirectParameter = classes.Property("Target", "ObjectProperty", redirect, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(redirect, Join({Assign(property("RedirectOwner"), Ref(0x00u, redirectParameter)), Assign(property("RedirectOnce"), Token(0x27u)), Return()}));
    const auto redirectLost = classes.Function("RawRedirectLostNext", probe, classesCore);
    const auto redirectLostParameter = classes.Property("Target", "ObjectProperty", redirectLost, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(redirectLost, Join({Assign(property("RedirectOwner"), Ref(0x00u, redirectLostParameter)),
        Assign(property("RedirectLostOnce"), Token(0x27u)), Return()}));
    for (const auto& [name, key, type, target] : std::vector<std::tuple<std::string, std::int32_t, std::string, std::int32_t>>{
        {"SetRawBase", baseProperty, "ObjectProperty", importedActor},
        {"SetRawLevel", levelProperty, "ObjectProperty", importedLevel},
        {"SetStanding", standingProperty, "ByteProperty", 0}}) {
        const auto function = classes.Function(name, probe, classesCore);
        const auto input = classes.Property("Input", type, function, classesCore, 1u, target, 0x80u);
        classes.FunctionCode(function, Join({Assign(Ref(0x01u, key), Ref(0x00u, input)), Return()}));
    }
    for (const auto& [name, flag] : std::vector<std::pair<std::string, std::string>>{
        {"RawRedirectBaseNext", "RedirectBaseOnce"}, {"ReenterBaseNext", "ReenterBaseOnce"},
        {"RedirectLevelNext", "RedirectLevelOnce"}}) {
        const bool level = name == "RedirectLevelNext";
        const auto function = classes.Function(name, probe, classesCore);
        const auto input = classes.Property("Input", "ObjectProperty", function, classesCore, 1u,
            level ? importedLevel : importedActor, 0x80u);
        classes.FunctionCode(function, Join({Assign(property(level ? "RedirectLevel" : "RedirectBase"), Ref(0x00u, input)),
            Assign(property(flag), Token(0x27u)), Return()}));
    }
    for (const auto& [name, native] : std::vector<std::pair<std::string, std::uint16_t>>{{"DisableEvent", 118u}, {"EnableEvent", 117u}}) {
        const auto function = classes.Function(name, probe, classesCore), parameter = classes.Property("Event", "NameProperty", function, classesCore, 1u, 0, 0x80u);
        classes.FunctionCode(function, Join({Native(native, {Ref(0x00u, parameter)}), Return()}));
    }
    for (const auto& [name, native] : std::vector<std::pair<std::string, std::uint16_t>>{{"BadOwnerType", 272u}, {"BadBaseType", 298u}})
        classes.Function(name, probe, classesCore, Join({Assign(property("StateValue"), Int(123)), Native(native, {Int(123)}), Return()}));
    classes.Function("BadDestroyCount", probe, classesCore, Join({Assign(property("StateValue"), Int(123)), Native(279u, {Int(123)}), Return()}));
    const auto recursiveGain = classes.Function("GainedChild", recursive, classesCore), recursiveParameter = classes.Property("Other", "ObjectProperty", recursiveGain, classesCore, 1u, importedActor, 0x80u);
    classes.FunctionCode(recursiveGain, Join({increment("Gained"), Context(Ref(0x00u, recursiveParameter), Native(272u, {Token(0x17u)})), Return()}));
    for (const auto& [cls, name] : std::vector<std::pair<std::int32_t, std::string>>{{failOwner, "GainedChild"}, {failBase, "Attach"}, {failUnTouch, "UnTouch"}}) {
        const auto function = classes.Function(name, cls, classesCore);
        classes.Property("Other", "ObjectProperty", function, classesCore, 1u, importedActor, 0x80u);
        classes.FunctionCode(function, Join({Assign(property("StateValue"), Int(321)), Native(999u), Return()}));
    }
    classes.Function("Destroyed", failDestroy, classesCore, Join({Assign(property("StateValue"), Int(321)), Native(999u), Return()}));
    for (const auto cls : {probe, recursive, failOwner, failBase, failDestroy, failUnTouch}) classes.ClassBody(cls);
    Bytes countDefaults; classes.ByteTag(countDefaults, "StandingCount", 19u);
    classes.ClassBody(countDefault, countDefaults); classes.ClassBody(countInherited);
    const auto classesTable = fixture.Write(classes);

    Package map; map.stem = "LifecycleFixture";
    const auto mapClasses = map.ImportPackage("LifecycleClasses"), mapEngine = map.ImportPackage("Engine");
    std::map<std::string, std::int32_t> actors;
    actors.emplace("Level0", map.Export("Level0", map.Import("LevelInfo", mapEngine)));
    actors.emplace("Level1", map.Export("Level1", map.Import("LevelInfo", mapEngine)));
    for (const auto& [name, cls] : std::vector<std::pair<std::string, std::string>>{
        {"Recorder", "Probe"}, {"ParentA", "Probe"}, {"ParentB", "Probe"}, {"Child", "Probe"},
        {"BaseA", "Probe"}, {"BaseB", "Probe"}, {"BasedChild", "Probe"}, {"Victim", "Probe"},
        {"OwnedChild", "Probe"}, {"BasedOnVictim", "Probe"}, {"Peer", "Probe"}, {"Static", "Probe"},
        {"Protected", "Probe"}, {"RecursiveParent", "Recursive"}, {"FailParent", "FailOwner"},
        {"FailBase", "FailBase"}, {"FailVictim", "FailDestroy"}, {"FailPeer", "FailUnTouch"},
        {"DefaultCount", "CountDefault"}, {"InheritedCount", "CountInherited"}})
        actors.emplace(name, map.Export(name, map.Import(cls, mapClasses)));
    for (const auto& [name, reference] : actors) {
        Bytes propertiesBytes; map.ObjectTag(propertiesBytes, "Level", actors.at("Level0"));
        if (name == "Level0" || name == "Level1") map.BoolTag(propertiesBytes, "bBegunPlay", true);
        else map.ObjectTag(propertiesBytes, "Journal", actors.at("Recorder"));
        if (name == "BaseA") map.ByteTag(propertiesBytes, "StandingCount", 7u);
        if (name == "BasedChild") map.ObjectTag(propertiesBytes, "Base", actors.at("BaseA"));
        if (name == "Static") map.BoolTag(propertiesBytes, "bStatic", true);
        if (name == "Protected") map.BoolTag(propertiesBytes, "bNoDelete", true);
        map.ActorBody(reference, propertiesBytes);
    }
    return {coreTable, engineTable, classesTable, fixture.Write(map, true)};
}

void OwnerTests(Fixture& fixture) {
    const auto ownerRevision = GetPortableRuntimeWorldRevision();
    Call("Child", "Own", {Object("ParentA")});
    Require(GetPortableRuntimeWorldRevision() > ownerRevision, "Committed SetOwner/callback writes did not advance world publication revision");
    HasObject("Child", "Owner", "ParentA", "Native SetOwner did not assign the actual Owner property");
    HasObject("ParentA", "ObservedOwner", "ParentA", "GainedChild did not observe newly assigned Owner");
    HasObject("ParentA", "LastArgument", "Child", "GainedChild argument is not the actual child identity");
    Require(Count("ParentA", "Gained") == 1 && Count("Recorder", "Trace") == 1, "SetOwner omitted its first GainedChild callback");
    auto saved = Decode(fixture.Snapshot("OwnerFirst"));
    Require(EmptyChildren(saved, "ParentA"), "SetOwner invented a reverse child-list authority");
    Call("Child", "Own", {Object("ParentA")});
    Require(Count("ParentA", "Lost") == 1 && Count("ParentA", "Gained") == 2 && Count("Recorder", "Trace") == 121,
        "Same-owner SetOwner incorrectly skipped LostChild/GainedChild or reversed them");
    Call("Child", "Own", {Object("ParentB")});
    HasObject("ParentA", "ObservedOwner", "ParentA", "LostChild did not observe the old Owner before assignment");
    HasObject("ParentB", "ObservedOwner", "ParentB", "New GainedChild did not observe assigned Owner");
    saved = Decode(fixture.Snapshot("OwnerTransfer"));
    Require(EmptyChildren(saved, "ParentA") && EmptyChildren(saved, "ParentB"),
        "Owner transfer mutated a reverse child list absent from the original operation");
    Call("Child", "Own", {Object("")}); HasObject("Child", "Owner", "", "SetOwner(None) did not clear reflected Owner");
    Require(EmptyChildren(Decode(fixture.Snapshot("OwnerClear")), "ParentB"), "SetOwner(None) invented native reverse ownership");
    RefusedCall(fixture, "Child", "OwnThenFail", {Object("ParentA")});
    RefusedCall(fixture, "Child", "Own", {Object("FailParent")}); RefusedCall(fixture, "Child", "BadOwnerType");
    fixture.Reset();
    Call("ParentA", "RedirectNext", {Object("ParentB")}); Call("Child", "Own", {Object("ParentA")});
    HasObject("Child", "Owner", "ParentB", "GainedChild reentrant SetOwner was overwritten by outer operation");
    saved = Decode(fixture.Snapshot("OwnerReentrant"));
    Require(EmptyChildren(saved, "ParentA") && EmptyChildren(saved, "ParentB"),
        "GainedChild reentrancy invented reverse ownership or duplicate relations");
    Require(Count("ParentA", "Gained") == 1 && Count("ParentA", "Lost") == 1 && Count("ParentB", "Gained") == 1,
        "Reentrant Owner callbacks did not execute synchronously in one interpreter");
    const auto reentrant = fixture.Snapshot("ReentrantCanonical");
    const auto validatedRevision = GetPortableRuntimeWorldRevision();
    Require(ValidatePortableRuntimeState((fixture.directory / "ReentrantCanonical.sav").string(), "LifecycleFixture"), "Reentrant Owner callback state failed nonmutating save validation");
    Require(fixture.Snapshot("AfterReentrantValidation") == reentrant, "Read-only lifecycle validation mutated Owner callback state");
    Require(GetPortableRuntimeWorldRevision() == validatedRevision, "Read-only lifecycle validation advanced world publication revision");
    Require(LoadPortableRuntimeState((fixture.directory / "ReentrantCanonical.sav").string()) && fixture.Snapshot("ReentrantRestore") == reentrant,
        "Reentrant Owner callback state failed canonical restore");
    // Historical serialized child links remain a round-trip fixture, not
    // authority for native SetOwner or an inferred reverse Base relation.
    auto maximum = Decode(reentrant);
    auto& maximumChildren = Record(maximum, "ParentB");
    if (!maximumChildren.lifecycle) maximumChildren.lifecycle.emplace();
    maximumChildren.lifecycle->children.assign(65'536u, Path("Child"));
    maximumChildren.lifecycle->basedActors.clear();
    const auto structuralMaximum = QuestVr::EncodeScriptSavedState(maximum);
    Require(QuestVr::DecodeScriptSavedState(structuralMaximum).objects.size() == maximum.objects.size(),
        "Exact codec actor-link maximum is structurally invalid");
    LoadScript(fixture, reentrant, maximum, "SerializedExactChildMaximum");
    const auto maximumSaved = Decode(fixture.Snapshot("ExactChildMaximumCapture"));
    Require(Life(maximumSaved, "ParentB").children.size() == 65'536u,
        "Runtime invented inferred Base links or discarded exact maximum child links");
    EmptyLegacyBases(maximumSaved);
    fixture.Reset(); Call("Child", "Own", {Object("ParentA")});
    Call("ParentA", "RawRedirectLostNext", {Object("RecursiveParent")});
    Call("Child", "Own", {Object("ParentB")});
    HasObject("Child", "Owner", "ParentB", "LostChild Owner mutation replaced the retained native NewOwner argument");
    Require(Count("ParentA", "Lost") == 1 && Count("ParentB", "Gained") == 1 && Count("RecursiveParent", "Gained") == 0,
        "SetOwner reread callback-mutated Owner or notified a receiver other than the retained NewOwner");
    HasObject("ParentA", "ObservedOwner", "ParentA", "LostChild did not receive the captured old-owner relationship");
    HasObject("ParentB", "ObservedOwner", "ParentB", "Retained NewOwner was not stored before GainedChild");
    fixture.Reset(); Call("Static", "Own", {Object("Protected")});
    HasObject("Static", "Owner", "Protected", "SetOwner invented bStatic/bNoDelete ownership guards");
    Call("Protected", "Own", {Object("Static")});
    HasObject("Protected", "Owner", "Static", "SetOwner invented an ownership-cycle guard");
    Call("Child", "Own", {Object("Child")});
    HasObject("Child", "Owner", "Child", "SetOwner invented a self-owner guard");
    fixture.Reset(); QuestVr::Vm::Limits limits; limits.callDepth = 8u;
    RefusedCall(fixture, "Child", "Own", {Object("RecursiveParent")}, limits);
    Require(Count("RecursiveParent", "Gained") == 0 && Count("Recorder", "Trace") == 0, "Recursive callback refusal retained intermediate writes");
    fixture.Reset(); Call("Child", "SetBegun", {Boolean(false)}); Call("Child", "Own", {Object("ParentA")});
    Require(Count("ParentA", "Gained") == 0, "SetOwner sent a callback before Level.bBegunPlay");
    HasObject("Child", "Owner", "ParentA", "Before-BeginPlay SetOwner failed to perform actual native mutation");
    fixture.Reset();
    Require(!ReadPortableActorStateObject(Path("ParentA")), "Generated parent unexpectedly owns a state frame before Disable");
    const auto frameless = fixture.Snapshot("FramelessOwnerBeforeDisable"); const auto framelessRevision = GetPortableRuntimeWorldRevision();
    Call("ParentA", "DisableEvent", {Name("GainedChild")}); Call("ParentA", "EnableEvent", {Name("GainedChild")});
    Require(fixture.Snapshot("FramelessOwnerAfterProbeNoops") == frameless && GetPortableRuntimeWorldRevision() == framelessRevision &&
        !ReadPortableActorStateObject(Path("ParentA")), "Recognized probe Enable/Disable fabricated a frame or state on a frameless receiver");
    Call("ParentA", "DisableEvent", {Name("GainedChild")}); Call("Child", "Own", {Object("ParentA")});
    HasObject("Child", "Owner", "ParentA", "Frameless probe Disable suppressed native Owner storage");
    Require(Count("ParentA", "Gained") == 1 && EmptyChildren(Decode(fixture.Snapshot("FramelessOwnerCallback")), "ParentA") &&
        !ReadPortableActorStateObject(Path("ParentA")), "Frameless recognized-probe Disable suppressed the actual owner relation/callback");
    fixture.Reset(); OwnNullCodeFrame(fixture, "ParentA"); Call("ParentA", "DisableEvent", {Name("gAiNeDcHiLd")});
    Require(!ProbeEnabled("ParentA", "GainedChild"), "Owned-frame Disable did not clear its actual positive probe bit");
    Call("Child", "Own", {Object("ParentA")});
    HasObject("Child", "Owner", "ParentA", "Disabled GainedChild suppressed native Owner storage");
    Require(Count("ParentA", "Gained") == 0 && EmptyChildren(Decode(fixture.Snapshot("OwnerDisabled")), "ParentA"),
        "Disabled GainedChild suppressed the owner relation itself or ignored event eligibility");
    Call("ParentA", "EnableEvent", {Name("GAINEDCHILD")}); Call("Child", "Own", {Object("ParentA")});
    Require(Count("ParentA", "Gained") == 1 && ProbeEnabled("ParentA", "GainedChild"),
        "Owned-frame Enable failed to restore the recognized GainedChild callback");
    fixture.Reset();
}

void BaseTests(Fixture& fixture) {
    Require(Count("BaseA", "StandingCount") == 7, "Map load reconstructed StandingCount from one authored Base edge");
    Require(Count("DefaultCount", "StandingCount") == 19 && Count("InheritedCount", "StandingCount") == 19,
        "Map load lost independent class-default/inherited StandingCount bytes");
    HasObject("BasedChild", "Base", "BaseA", "Authored Base edge was lost while preserving independent count");
    const auto baseRevision = GetPortableRuntimeWorldRevision();
    Call("Child", "BaseOn", {Object("BaseB")});
    Require(GetPortableRuntimeWorldRevision() > baseRevision, "Committed SetBase/callback writes did not advance world publication revision");
    HasObject("Child", "Base", "BaseB", "Native SetBase did not assign reflected Base");
    HasObject("BaseB", "ObservedBase", "BaseB", "Attach did not observe newly assigned Base");
    Require(Count("BaseB", "StandingCount") == 1 && Count("BaseB", "ObservedStanding") == 1 && Count("Child", "BaseChanges") == 1 && Count("Recorder", "Trace") == 35,
        "SetBase did not update native count before Attach then send BaseChange");
    const auto first = fixture.Snapshot("BaseFirst"); const auto sameRevision = GetPortableRuntimeWorldRevision();
    Call("Child", "BaseOn", {Object("BaseB")});
    Require(fixture.Snapshot("SameBase") == first, "Same-base SetBase mutated state or emitted callbacks");
    Require(GetPortableRuntimeWorldRevision() == sameRevision, "Same-base native no-op advanced world publication revision");
    Call("Child", "BaseOn", {Object("BaseA")});
    HasObject("BaseB", "ObservedBase", "BaseB", "Detach did not observe old Base before assignment");
    Require(Count("BaseB", "StandingCount") == 0 && Count("BaseB", "ObservedStanding") == 0 && Count("BaseA", "StandingCount") == 8,
        "Base transfer reconstructed count or updated its raw byte at the wrong time");
    auto saved = Decode(fixture.Snapshot("BaseTransfer"));
    EmptyLegacyBases(saved);
    const auto selfBefore = fixture.Snapshot("BeforeSelfBase"); Call("Child", "BaseOn", {Object("Child")});
    Require(fixture.Snapshot("AfterSelfBase") == selfBefore, "SetBase allowed self base or changed state on rejected cycle");
    Call("BaseB", "BaseOn", {Object("Child")});
    const auto cycleBefore = fixture.Snapshot("BeforeBaseCycle"); Call("Child", "BaseOn", {Object("BaseB")});
    Require(fixture.Snapshot("AfterBaseCycle") == cycleBefore, "SetBase allowed an indirect base cycle");
    Call("Child", "BaseOn", {Object("Level0")});
    HasObject("Child", "Base", "Level0", "LevelInfo Base assignment was rejected");
    Require(Count("Level0", "StandingCount") == 0 && Count("BaseA", "StandingCount") == 7,
        "LevelInfo Base received normal based actor counts or old base did not detach");
    saved = Decode(fixture.Snapshot("LevelBase"));
    const auto level = std::find_if(saved.objects.begin(), saved.objects.end(), [&](const auto& value) { return value.path == Path("Level0"); });
    Require(level == saved.objects.end() || !level->lifecycle || level->lifecycle->basedActors.empty(), "SetBase treated LevelInfo as ordinary native based parent");
    Call("Child", "BaseOn", {Object("")}); HasObject("Child", "Base", "", "SetBase(None) did not clear reflected Base");
    RefusedCall(fixture, "Child", "BaseOnThenFail", {Object("BaseB")});
    RefusedCall(fixture, "Child", "BaseOn", {Object("FailBase")}); RefusedCall(fixture, "Child", "BadBaseType");
    // Historical valid wire payloads remain schema-checked but never recreate
    // a reverse Base simulation authority, including duplicate entries.
    fixture.Reset(); Call("Child", "SetValue", {Integer(1)});
    const auto historicalPrefix = fixture.Snapshot("HistoricalBasePrefix");
    auto historical = Decode(historicalPrefix);
    auto& historicalChild = Record(historical, "Child"); historicalChild.lifecycle.emplace();
    historicalChild.lifecycle->basedActors = {Path("Peer"), Path("Peer"), Path("Victim")};
    LoadScript(fixture, historicalPrefix, historical, "HistoricalIgnoredBaseLinks");
    EmptyLegacyBases(Decode(fixture.Snapshot("HistoricalBaseCanonical")));
    Require(Count("BaseA", "StandingCount") == 7 && Count("Child", "StandingCount") == 0,
        "Historical reverse Base wire entries reconstructed StandingCount");
    HasObject("Peer", "Base", "", "Historical reverse Base wire entries changed direct Base");
    GC::Collect(); EmptyLegacyBases(Decode(fixture.Snapshot("HistoricalBaseCanonicalGC")));

    // Native byte operations wrap independently of direct Base topology.
    fixture.Reset(); Call("BaseB", "SetStanding", {Byte(255u)});
    Call("Child", "BaseOn", {Object("BaseB")});
    Require(Count("BaseB", "StandingCount") == 0 && Count("BaseB", "ObservedStanding") == 0,
        "Attach count did not wrap 255 to zero before callback");
    Call("Child", "BaseOn", {Object("")});
    Require(Count("BaseB", "StandingCount") == 255 && Count("BaseB", "ObservedStanding") == 255,
        "Detach count did not wrap zero to 255 before callback");
    fixture.Reset(); Call("Child", "SetRawBase", {Object("BaseB")});
    Call("Child", "BaseOn", {Object("")});
    Require(Count("BaseB", "StandingCount") == 255, "An inconsistent authored/raw Base count was normalized before decrement");

    // The prospective traversal precedes same-base equality. An unrelated
    // cycle must exhaust this root's finite native budget, not silently pass.
    fixture.Reset(); Call("Child", "SetRawBase", {Object("BaseA")});
    Call("BaseA", "SetRawBase", {Object("BaseB")}); Call("BaseB", "SetRawBase", {Object("BaseA")});
    QuestVr::Vm::Limits cycleLimits; cycleLimits.nativeWork = 64u;
    RefusedCall(fixture, "Child", "BaseOn", {Object("BaseA")}, cycleLimits);

    // Detach reentrancy must not replace the captured by-value new Base. Counts
    // target captured pointers, even when the final direct links disagree.
    fixture.Reset(); Call("Child", "BaseOn", {Object("BaseA")});
    Call("BaseA", "ReenterBaseNext", {Object("Peer")});
    Call("Child", "BaseOn", {Object("BaseB")});
    HasObject("Child", "Base", "BaseB", "Outer SetBase lost its captured new Base after reentrant Detach");
    Require(Count("BaseA", "StandingCount") == 6 && Count("BaseA", "Detached") == 2 &&
        Count("Peer", "StandingCount") == 1 && Count("BaseB", "StandingCount") == 1 && Count("Child", "BaseChanges") == 3,
        "Reentrant Detach used list counts, stale fields or deferred callback execution");
    const auto reentrantBase = fixture.Snapshot("ReentrantBaseCanonical");
    Require(LoadPortableRuntimeState((fixture.directory / "ReentrantBaseCanonical.sav").string()) &&
        fixture.Snapshot("ReentrantBaseRestore") == reentrantBase,
        "Independent reentrant Base/count fields did not round-trip canonically");
    GC::Collect(); Require(fixture.Snapshot("ReentrantBaseGC") == reentrantBase, "Reentrant Base/count fields were not rooted across GC");
    EmptyLegacyBases(Decode(reentrantBase));

    fixture.Reset(); Call("BaseB", "ReenterBaseNext", {Object("Peer")});
    Call("Child", "BaseOn", {Object("BaseB")});
    HasObject("Child", "Base", "Peer", "Outer SetBase rewrote Base after reentrant Attach");
    HasObject("Child", "ObservedBase", "Peer", "Final BaseChange did not observe fresh callback-mutated Base");
    Require(Count("BaseB", "StandingCount") == 0 && Count("Peer", "StandingCount") == 1 && Count("Child", "BaseChanges") == 2,
        "Reentrant Attach count/callback sequence is wrong");

    fixture.Reset(); Call("Child", "BaseOn", {Object("BaseA")});
    Call("BaseA", "RawRedirectBaseNext", {Object("Peer")}); Call("Child", "BaseOn", {Object("BaseB")});
    HasObject("Child", "Base", "BaseB", "Raw Detach callback Base overwrite replaced captured new Base");
    Require(Count("BaseA", "StandingCount") == 7 && Count("BaseB", "StandingCount") == 1 && Count("Peer", "StandingCount") == 0,
        "Raw Detach Base overwrite invented relationship/count repair");
    fixture.Reset(); Call("BaseB", "RawRedirectBaseNext", {Object("Peer")}); Call("Child", "BaseOn", {Object("BaseB")});
    HasObject("Child", "Base", "Peer", "Raw Attach callback Base overwrite was lost");
    HasObject("Child", "ObservedBase", "Peer", "BaseChange did not freshly read raw Attach-mutated Base");
    Require(Count("BaseB", "StandingCount") == 1 && Count("Peer", "StandingCount") == 0,
        "Raw Attach Base overwrite derived counts from final topology");

    fixture.Reset(); Call("Child", "BaseOn", {Object("BaseA")});
    Call("BaseA", "RedirectLevelNext", {Object("Level1")}); Call("Child", "BaseOn", {Object("Level0")});
    HasObject("Child", "Level", "Level1", "Detach callback Level overwrite was lost");
    HasObject("Child", "Base", "Level0", "Original LevelInfo pointer could not become an ordinary new Base after Level changed");
    Require(Count("Level0", "StandingCount") == 1 && Count("BaseA", "StandingCount") == 7,
        "SetBase cached Level across Detach instead of rereading the raw field");

    // With callbacks ineligible, each native Base/count write is independently
    // constrained by the SAME VM write limit and rolls back at every prefix.
    for (const std::size_t allowed : {0u, 1u}) {
        fixture.Reset(); Call("Child", "SetBegun", {Boolean(false)});
        QuestVr::Vm::Limits writes; writes.writes = allowed;
        RefusedCall(fixture, "Child", "BaseOn", {Object("BaseB")}, writes);
        Call("Child", "BaseOn", {Object("BaseB")});
        RefusedCall(fixture, "Child", "BaseOn", {Object("")}, writes);
    }
    fixture.Reset(); Call("Child", "SetBegun", {Boolean(false)});
    QuestVr::Vm::Limits twoWrites; twoWrites.writes = 2u;
    const auto exactWrites = ExecutePortableActorFunction(Path("Child"), "BaseOn", {Object("BaseB")}, twoWrites);
    Require(exactWrites.passed() && exactWrites.committed && exactWrites.writes == 2u,
        "Native Base/count stores did not consume exactly two shared VM writes with callbacks ineligible");
    HasObject("Child", "Base", "BaseB", "Exact native write-budget success did not commit Base");
    Require(Count("BaseB", "StandingCount") == 1 && Count("BaseB", "Attached") == 0,
        "Exact native write-budget success lost count or invented an ineligible callback");
    // Count and Base stores plus every callback's own writes share one outer
    // limit. Refuse both early callback prefixes and the late BaseChange edge.
    for (const std::size_t allowed : {2u, 3u, 9u}) {
        fixture.Reset(); QuestVr::Vm::Limits writes; writes.writes = allowed;
        RefusedCall(fixture, "Child", "BaseOn", {Object("BaseB")}, writes);
    }
    fixture.Reset(); Call("Child", "BaseOn", {Object("BaseA")});
    Call("BaseA", "ReenterBaseNext", {Object("Peer")});
    QuestVr::Vm::Limits nestedWrites; nestedWrites.writes = 12u;
    RefusedCall(fixture, "Child", "BaseOn", {Object("BaseB")}, nestedWrites);
    fixture.Reset(); QuestVr::Vm::Limits noWrites; noWrites.writes = 0u;
    RefusedCall(fixture, "Child", "SetRawBase", {Object("BaseB")}, noWrites);
    RefusedCall(fixture, "BaseB", "SetStanding", {Byte(255u)}, noWrites);
    fixture.Reset(); Call("Child", "SetBegun", {Boolean(false)}); Call("Child", "BaseOn", {Object("BaseB")});
    Require(Count("BaseB", "Attached") == 0 && Count("Child", "BaseChanges") == 0 && Count("BaseB", "StandingCount") == 1,
        "Before-BeginPlay base callback gate suppressed native relation or emitted callbacks");
    fixture.Reset();
}

void InjectTouch(Fixture& fixture, const std::string& actor, const std::string& peer,
    const std::string& label, bool sent = true) {
    // Native sent flags cannot be invented by writing Touching[] alone. Load a
    // fully typed generated lifecycle checkpoint to establish the exact pin
    // precondition, without pretending collision/Touch startup is implemented.
    Call(actor, "SetValue", {Integer(1)}); Call(peer, "SetValue", {Integer(2)});
    const auto current = fixture.Snapshot(label + "Prefix"); auto saved = Decode(current);
    auto& left = Record(saved, actor); auto& right = Record(saved, peer);
    if (!left.lifecycle) left.lifecycle.emplace();
    if (!right.lifecycle) right.lifecycle.emplace();
    left.lifecycle->touchEventSent[0] = sent; right.lifecycle->touchEventSent[2] = sent;
    SetSavedProperty(left, "Touching", "Engine.Actor.Touching", Object(peer).value, 0u);
    SetSavedProperty(right, "Touching", "Engine.Actor.Touching", Object(actor).value, 2u);
    LoadScript(fixture, current, saved, label);
}
void DestroyTests(Fixture& fixture) {
    // Support-detach coverage only. The outer legacy Destroy still has its
    // older delete/event ordering and refuses the active retail hash.
    fixture.Reset(); Call("BasedOnVictim", "SetRawBase", {Object("Victim")});
    Require(Count("Victim", "StandingCount") == 0, "Raw Base setup inferred a standing count");
    Require(QuestVr::Vm::ToBool(Call("Victim", "Die").value), "Zero-count support teardown failed");
    HasObject("BasedOnVictim", "Base", "Victim", "Support teardown ignored its initial zero-count gate");
    fixture.Reset(); Call("BasedOnVictim", "SetRawBase", {Object("Victim")});
    Call("Peer", "SetRawBase", {Object("Victim")}); Call("Victim", "SetStanding", {Byte(1u)});
    Require(QuestVr::Vm::ToBool(Call("Victim", "Die").value), "Live-slot support teardown failed");
    HasObject("BasedOnVictim", "Base", "", "Support teardown missed its first direct dependent");
    HasObject("Peer", "Base", "", "Support teardown stopped scanning when the independent count reached zero");
    Require(Count("Victim", "StandingCount") == 255, "Support scan normalized an inconsistent count instead of wrapping twice");
    fixture.Reset(); Call("BasedOnVictim", "BaseOn", {Object("Victim")});
    RefusedCall(fixture, "Victim", "DieThenFail");
    HasObject("BasedOnVictim", "Base", "Victim", "Late failure retained support-detach mutations");
    Require(Count("Victim", "StandingCount") == 1, "Late failure lost the support count");
    fixture.Reset();
    Require(!QuestVr::Vm::ToBool(Call("Static", "Die").value) && !QuestVr::Vm::ToBool(Call("Protected", "Die").value),
        "Destroy ignored bStatic/bNoDelete protection");
    Require(Published("Static") && Published("Protected") && !QuestVr::Vm::ToBool(Read("Static", "bDeleteMe")) && !QuestVr::Vm::ToBool(Read("Protected", "bDeleteMe")),
        "Protected Destroy removed a world actor or set bDeleteMe");
    RefusedCall(fixture, "Victim", "BadDestroyCount"); RefusedCall(fixture, "Victim", "DieThenFail");
    RefusedCall(fixture, "FailVictim", "Die"); Require(Published("FailVictim"), "Failed Destroyed callback removed world publication");
    fixture.Reset();
    Call("Victim", "Own", {Object("ParentA")}); Call("Victim", "BaseOn", {Object("BaseB")});
    Call("OwnedChild", "Own", {Object("Victim")}); Call("BasedOnVictim", "BaseOn", {Object("Victim")});
    InjectTouch(fixture, "Victim", "Peer", "TouchBeforeDestroy");
    const auto before = fixture.Snapshot("DestroySetup"); Require(Published("Victim"), "Destroy setup lost world publication");
    const auto beforeDestroyRevision = GetPortableRuntimeWorldRevision();
    Same(Call("Victim", "DieThenWrite").value, Object("Victim").value, "Destroy made its executing receiver identity unavailable");
    Require(GetPortableRuntimeWorldRevision() > beforeDestroyRevision, "Committed Destroy did not advance world publication revision");
    Require(QuestVr::Vm::ToBool(Read("Victim", "bDeleteMe")) && QuestVr::Vm::ToBool(Read("Victim", "ObservedDeleted")) && Count("Victim", "DestroyedCalls") == 1,
        "Destroy did not set bDeleteMe before its enum Destroyed exception callback");
    Require(!Published("Victim") && !Published("Victim", true), "Destroyed actor remained world-published through snapshot mode");
    Require(Count("Victim", "StateValue") == 77, "Direct post-Destroy write inside the same function was refused");
    HasObject("Victim", "Owner", "", "Destroy retained own Owner"); HasObject("Victim", "Base", "", "Destroy retained own Base");
    HasObject("OwnedChild", "Owner", "", "Destroy did not clear its native child owner");
    HasObject("BasedOnVictim", "Base", "", "Destroy did not clear its native based child's Base");
    Require(Count("Victim", "StandingCount") == 0 && Count("BaseB", "StandingCount") == 0, "Destroy retained standing actor counts");
    HasObject("Peer", "Touching", "", "Destroy did not clear peer Touching slot", 2u);
    Require(Count("Peer", "Untouched") == 1 && Count("Victim", "Untouched") == 0, "Destroy UnTouch event eligibility or sent flags differ from pin");
    const auto current = fixture.Snapshot("Destroyed"); auto saved = Decode(current);
    Require(current[4u] == 7u && QuestVr::EncodeScriptSavedState(saved)[6u] == 4u, "Native lifecycle did not select v7 envelope/codec4");
    Require(Life(saved, "Victim").worldRemoved && Life(saved, "Victim").children.empty() && Life(saved, "Victim").basedActors.empty() &&
        Life(saved, "Victim").touchEventSent[0] && !Life(saved, "Peer").touchEventSent[2], "Destroy did not preserve exact native teardown/touch state");
    HasObject("Victim", "Touching", "Peer", "Destroy incorrectly cleared the deleted self's Touching slot");
    const auto repeated = fixture.Snapshot("BeforeRepeatedDestroy"); Require(QuestVr::Vm::ToBool(Call("Victim", "Die").value), "Repeated Destroy did not return true");
    Require(fixture.Snapshot("AfterRepeatedDestroy") == repeated && Count("Victim", "DestroyedCalls") == 1, "Repeated Destroy repeated lifecycle effects");
    Same(Call("Victim", "AfterDelete").value, Object("Victim").value, "Deleted actor became uncallable after transaction commit");
    Same(Call("Child", "KeepReference", {Object("Victim")}).value, Value::Integer(88), "Deleted actor typed Object reference/property context was invalidated");
    HasObject("Child", "Link", "Victim", "Deleted actor reference identity was silently cleared");
    const auto noTick = ExecutePortableActorEvent(Path("Victim"), "Tick", true);
    Require(noTick.passed() && noTick.value.kind == Kind::Nothing && Count("Victim", "Ticks") == 0, "Deleted world actor received non-Destroyed event");
    const auto namedDestroyed = ExecutePortableActorEvent(Path("Victim"), "Destroyed", false);
    Require(namedDestroyed.passed() && namedDestroyed.value.kind == Kind::Nothing && Count("Victim", "DestroyedCalls") == 1,
        "Name-based Destroyed improperly received enum deletion exception");
    const auto canonical = fixture.Snapshot("DeletedCallable");
    const auto deletedValidationRevision = GetPortableRuntimeWorldRevision();
    Require(ValidatePortableRuntimeState((fixture.directory / "DeletedCallable.sav").string(), "LifecycleFixture"), "Deleted actor typed references failed save validation");
    Require(fixture.Snapshot("AfterDeletedValidation") == canonical, "Read-only deleted actor save validation mutated runtime");
    Require(GetPortableRuntimeWorldRevision() == deletedValidationRevision, "Read-only deleted actor validation advanced world revision");
    GC::Collect(); Require(fixture.Snapshot("AfterDeletedGC") == canonical, "Deleted actor/property/native references were not rooted across GC");
    fixture.Reset(); Require(Published("Victim") && !QuestVr::Vm::ToBool(Read("Victim", "bDeleteMe")) && Count("BaseA", "StandingCount") == 7,
        "Legacy checkpoint failed to restore deleted actor/authored independent count");
    Require(LoadPortableRuntimeState((fixture.directory / "DeletedCallable.sav").string()) && fixture.Snapshot("DeletedRestore") == canonical,
        "Deleted actor/native topology v7 save failed canonical replacement restore");
    Require(!Published("Victim") && Count("Victim", "StateValue") == 88 && Count("Peer", "Untouched") == 1, "v7 restore lost deletion/native callback state");
    // Every prior envelope replaces rather than merges fields and genuine
    // child/touch/removal state. No old reverse Base list becomes authority.
    for (std::uint32_t version = 1u; version <= 6u; ++version) {
        Bytes older;
        if (version <= 3u) older = OldEmptyEnvelope(version);
        else {
            QuestVr::ScriptSavedState old; old.mapName = "LifecycleFixture";
            QuestVr::ScriptSavedObject object; object.path = Path("Victim"); object.classPath = "LifecycleClasses.Probe";
            object.properties.push_back({"LifecycleClasses.Probe.StateValue", "StateValue", 0u, Value::Integer(99)});
            if (version == 5u) {
                object.state.emplace(); object.state->disabled["none"].insert("touch");
            }
            old.objects.push_back(std::move(object));
            if (version == 6u) old.classDefaults.push_back({"LifecycleClasses.Probe",
                {{"LifecycleClasses.Probe.StateValue", "StateValue", 0u, Value::Integer(9)}}});
            older = Envelope(fixture.legacy, old, version);
        }
        const auto path = fixture.directory / ("OldLifecycleVersion" + std::to_string(version) + ".sav");
        WriteBytes(path, older); const auto oldValidationRevision = GetPortableRuntimeWorldRevision();
        Require(ValidatePortableRuntimeState(path.string(), "LifecycleFixture"), "Old envelope failed lifecycle reset schema validation: " + std::to_string(version));
        Require(GetPortableRuntimeWorldRevision() == oldValidationRevision && fixture.Snapshot("AfterOldValidation") == canonical,
            "Read-only old-envelope validation reset native topology/revision: " + std::to_string(version));
        Require(LoadPortableRuntimeState(path.string()), "Old envelope failed native topology replacement: " + std::to_string(version));
        Require(Published("Victim") && !QuestVr::Vm::ToBool(Read("Victim", "bDeleteMe")) && Count("BaseA", "StandingCount") == 7,
            "Old envelope retained deletion or reconstructed authored StandingCount: " + std::to_string(version));
        HasObject("Peer", "Touching", "", "Old envelope retained peer Touching native/script state", 2u);
        HasObject("Victim", "Owner", "", "Old envelope retained deleted actor Owner");
        Require(Count("Victim", "StateValue") == (version >= 4u ? 99 : 0), "Old envelope failed supported instance overlay replacement");
        const auto oldCapture = fixture.Snapshot("OldReplacementCapture");
        if (version <= 3u) Require(oldCapture == fixture.legacy, "Old empty envelope did not restore untouched legacy baseline");
        else {
            Require(oldCapture[4u] == version, "Old envelope replacement invented native lifecycle/version upgrade");
            const auto oldState = Decode(oldCapture);
            Require(std::none_of(oldState.objects.begin(), oldState.objects.end(), [](const auto& actor) { return actor.lifecycle.has_value(); }),
                "Old envelope replacement retained previous v7 native lifecycle payload");
            Require(oldCapture == older, "Old instance/state/CDO replacement was not canonical");
        }
        Require(LoadPortableRuntimeState((fixture.directory / "DeletedCallable.sav").string()) && fixture.Snapshot("AfterOldCycle") == canonical,
            "Old/v7 lifecycle replacement cycle lost canonical topology: " + std::to_string(version));
    }
    // Script field writes do not undo a removed ULevel slot. Preserve this pin
    // state verbatim; do not invent a worldRemoved=>bDeleteMe constraint.
    Call("Victim", "ClearDeleteFlag");
    Require(!QuestVr::Vm::ToBool(Read("Victim", "bDeleteMe")) && !Published("Victim"), "Direct delete-flag clearing resurrected removed world actor");
    const auto cleared = fixture.Snapshot("RemovedWithClearedFlag");
    Require(Life(Decode(cleared), "Victim").worldRemoved, "Saving a cleared delete flag lost native removal state");
    const auto clearedValidationRevision = GetPortableRuntimeWorldRevision();
    Require(ValidatePortableRuntimeState((fixture.directory / "RemovedWithClearedFlag.sav").string(), "LifecycleFixture"), "Valid removed actor with cleared delete flag failed schema validation");
    Require(GetPortableRuntimeWorldRevision() == clearedValidationRevision && fixture.Snapshot("AfterClearedValidation") == cleared,
        "Cleared-flag validation mutated native world state/revision");
    fixture.Reset(); Require(LoadPortableRuntimeState((fixture.directory / "RemovedWithClearedFlag.sav").string()) && fixture.Snapshot("ClearedFlagRestore") == cleared,
        "Removed actor with script-cleared flag did not restore canonically");
    Require(!Published("Victim") && !QuestVr::Vm::ToBool(Read("Victim", "bDeleteMe")), "Restore normalized script flag/native world slot relationship");
    auto bad = Decode(canonical); auto& badChild = Record(bad, "ParentA");
    if (!badChild.lifecycle) badChild.lifecycle.emplace();
    badChild.lifecycle->children.push_back("LifecycleFixture.Missing");
    RefusedScript(fixture, canonical, bad, "MissingNativeChild");
    bad = Decode(canonical); auto& badBase = Record(bad, "BaseB");
    if (!badBase.lifecycle) badBase.lifecycle.emplace();
    badBase.lifecycle->basedActors.push_back("LifecycleClasses.Probe");
    RefusedScript(fixture, canonical, bad, "ClassNotNativeBasedActor");
    fixture.Reset(); InjectTouch(fixture, "Victim", "FailPeer", "TouchRollbackSetup");
    RefusedCall(fixture, "Victim", "Die"); Require(Published("Victim") && !QuestVr::Vm::ToBool(Read("Victim", "bDeleteMe")),
        "Late UnTouch failure retained earlier deletion/publication mutation");
    fixture.Reset(); InjectTouch(fixture, "Victim", "Peer", "UnsentTouchSetup", false);
    Require(QuestVr::Vm::ToBool(Call("Victim", "Die").value), "Destroy with unsent Touching relation failed");
    HasObject("Peer", "Touching", "", "Destroy failed to clear unsent peer Touching slot", 2u);
    Require(Count("Peer", "Untouched") == 0, "Destroy invented UnTouch callback for a tagged/unsent Touching relation");
    fixture.Reset(); const auto frameless = fixture.Snapshot("FramelessVictimBeforeDisable"); const auto framelessRevision = GetPortableRuntimeWorldRevision();
    Call("Victim", "DisableEvent", {Name("Destroyed")});
    Require(fixture.Snapshot("FramelessVictimAfterDisable") == frameless && GetPortableRuntimeWorldRevision() == framelessRevision &&
        !ReadPortableActorStateObject(Path("Victim")), "Frameless Destroyed Disable created an owned frame or disabled probe state");
    Require(QuestVr::Vm::ToBool(Call("Victim", "Die").value) && Count("Victim", "DestroyedCalls") == 1 && !Published("Victim"),
        "Frameless recognized-probe Disable suppressed native destruction or its actual Destroyed callback");
    fixture.Reset(); OwnNullCodeFrame(fixture, "Victim"); Call("Victim", "DisableEvent", {Name("Destroyed")});
    Require(!ProbeEnabled("Victim", "Destroyed"), "Owned-frame Destroyed Disable did not clear its positive probe bit");
    Require(QuestVr::Vm::ToBool(Call("Victim", "Die").value), "Disabled Destroyed prevented native Destroy");
    Require(Count("Victim", "DestroyedCalls") == 0 && !Published("Victim"), "Destroy ignored disabled event mask or suppressed world removal");
    fixture.Reset(); Call("Victim", "SetBegun", {Boolean(false)}); Require(QuestVr::Vm::ToBool(Call("Victim", "Die").value), "Before-BeginPlay Destroy did not perform native destruction");
    Require(Count("Victim", "DestroyedCalls") == 0 && !Published("Victim"), "Before-BeginPlay Destroy emitted callback or retained world publication");
    fixture.Reset(); Require(fixture.Snapshot("FinalBaseline") == fixture.legacy && before != fixture.legacy, "Final reset failed to discard committed native lifecycle");
}
void Synthetic() {
    Fixture fixture; const auto tables = Build(fixture);
    const auto initialized = InitializePortableRuntime({tables.core, tables.engine, tables.classes});
    Require(initialized.passed && initialized.functions >= 20u, "Generated lifecycle runtime metadata initialization failed");
    Require(LoadPortableRuntimeMap(tables.map).passed, "Generated lifecycle map failed production loader");
    fixture.legacy = fixture.Snapshot("Legacy"); Require(fixture.legacy[4u] == 3u, "Untouched lifecycle baseline did not preserve legacy envelope");
    Require(!GetPortableRuntimeScriptStatePresent(), "Generated lifecycle metadata initialization invented persistent state");
    OwnerTests(fixture); BaseTests(fixture); DestroyTests(fixture); fixture.UnchangedSources();
    Require(InitializePortableRuntime({tables.core, tables.engine, tables.classes}).passed && LoadPortableRuntimeMap(tables.map).passed,
        "Explicit lifecycle runtime reinitialization failed");
    Require(fixture.Snapshot("Reinitialized") == fixture.legacy && Published("Victim") && Count("BaseA", "StandingCount") == 7,
        "Explicit runtime reinitialization retained old lifecycle state");
    fixture.UnchangedSources();
}
} // namespace
int main() {
    try {
        const auto gcBefore = GC::GetStats().numObjects;
        Synthetic(); GC::Collect();
        Require(GC::GetStats().numObjects == gcBefore, "Generated lifecycle test leaked rooted objects after shutdown");
        std::cout << "Generated actor lifecycle integration: " << checks << " checks, " << refusals
                  << " rejection controls; actual Owner272/Base298 callbacks, independent wrapping counts, callback reentrancy, retired reverse-Base wire migration, write budgets, deletion lifetime, v7/legacy replacement and GC. Older Destroy279 coverage is not complete retail removal parity; no Spawn or campaign startup claim.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "Actor lifecycle integration failed: " << error.what() << '\n'; return 1; }
}
