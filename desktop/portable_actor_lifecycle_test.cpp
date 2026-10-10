#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
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
            if (type == "ObjectProperty" || type == "ByteProperty" || type == "StructProperty") Index(body, target);
            return body;
        }; return reference;
    }
    std::int32_t Function(const std::string& name, std::int32_t owner, std::int32_t core,
        Code code = {}, std::uint16_t native = 0u) {
        const auto reference = Export(name, Import("Function", core), 0, owner, true);
        FunctionCode(reference, std::move(code), native); return reference;
    }
    void FunctionCode(std::int32_t reference, Code code, std::uint16_t native = 0u) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference, native, code = std::move(code)](std::int32_t next, std::int32_t child) {
            Bytes body{0u}; Index(body, 0); Index(body, next); Index(body, 0); Index(body, child);
            Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName); U32(body, 0); U32(body, 0);
            U32(body, static_cast<std::uint32_t>(code.logical)); Append(body, code.raw); U16(body, native); body.push_back(0u);
            U32(body, native ? 0x402u : 2u); return body;
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
    ++refusals;
}
std::uint32_t Read32(const Bytes& bytes, std::size_t& cursor) {
    Require(cursor <= bytes.size() && bytes.size() - cursor >= 4u, "Independent lifecycle checkpoint parser encountered truncation");
    std::uint32_t value{}; for (unsigned i = 0u; i < 4u; ++i) value |= static_cast<std::uint32_t>(bytes[cursor++]) << (8u * i); return value;
}
std::size_t PrefixSize(const Bytes& bytes) {
    std::size_t cursor{}; Require(Read32(bytes, cursor) == 0x53515844u, "Independent lifecycle runtime magic mismatch");
    const auto version = Read32(bytes, cursor); Require(version >= 1u && version <= 7u, "Independent lifecycle runtime envelope version mismatch");
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
Bytes Envelope(const Bytes& prefixSource, const QuestVr::ScriptSavedState& script, std::uint32_t version = 7u) {
    const auto prefix = PrefixSize(prefixSource);
    Bytes bytes(prefixSource.begin(), prefixSource.begin() + static_cast<std::ptrdiff_t>(prefix)); Replace32(bytes, 4u, version);
    const auto blob = QuestVr::EncodeScriptSavedState(script); U32(bytes, static_cast<std::uint32_t>(blob.size())); Append(bytes, blob); return bytes;
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
void SetSavedProperty(QuestVr::ScriptSavedObject& object, const std::string& name, const std::string& key,
    Value value, std::uint32_t slot = 0u) {
    const auto found = std::find_if(object.properties.begin(), object.properties.end(), [&](const auto& property) { return property.name == name && property.index == slot; });
    if (found == object.properties.end()) object.properties.push_back({key, name, slot, std::move(value)});
    else found->value = std::move(value);
}
void LoadScript(const Fixture& fixture, const Bytes& prefix, const QuestVr::ScriptSavedState& script, const std::string& label,
    std::uint32_t version = 7u) {
    const auto path = fixture.directory / (label + ".sav"); WriteBytes(path, Envelope(prefix, script, version));
    Require(ValidatePortableRuntimeState(path.string(), "LifecycleFixture"), "Generated lifecycle payload did not validate: " + label);
    Require(LoadPortableRuntimeState(path.string()), "Generated lifecycle payload did not load: " + label);
}
void OwnNullCodeFrame(const Fixture& fixture, const std::string& actor) {
    Require(!ReadPortableActorSerializedStack(Path(actor)), "Generated frameless actor unexpectedly has an authored stack");
    QuestVr::ScriptSavedState script; script.mapName = "LifecycleFixture";
    QuestVr::ScriptSavedObject object; object.path = Path(actor); object.classPath = "LifecycleClasses.Probe";
    object.state.emplace(); object.state->hasStack = true; object.state->frameOverride = true;
    object.state->frame.emplace(); // A real owned null-code frame, not invented state bytecode.
    script.objects.push_back(std::move(object));
    LoadScript(fixture, fixture.legacy, script, "OwnedNullCodeFrame" + actor, 5u);
    const auto state = ReadPortableActorStateObject(Path(actor));
    Require(state && state->hasStack && state->frameOverride && state->frame && state->frame->codePath.empty() &&
        state->frame->localsCodePath.empty() && state->frame->locals.empty(), "Typed checkpoint did not establish the requested owned null-code frame");
    Require(!ReadPortableActorSerializedStack(Path(actor)), "Owned portable frame altered immutable authored stack metadata");
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
    for (const auto& name : {"Owner", "Base", "Level", "Instigator"}) engine.Property(name, "ObjectProperty", actor, engineCore, 1u, actor);
    engine.Property("Touching", "ObjectProperty", actor, engineCore, 4u, actor);
    engine.Property("StandingCount", "ByteProperty", actor, engineCore);
    for (const auto& name : {"bStatic", "bNoDelete", "bDeleteMe"}) engine.Property(name, "BoolProperty", actor, engineCore);
    engine.Property("bBegunPlay", "BoolProperty", levelInfo, engineCore);
    const auto ownerNative = engine.Function("SetOwner", actor, engineCore, {}, 272u);
    engine.Property("NewOwner", "ObjectProperty", ownerNative, engineCore, 1u, actor, 0x80u);
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
    std::map<std::string, std::int32_t> properties;
    for (const auto& name : {"Trace", "Gained", "Lost", "Attached", "Detached", "BaseChanges", "DestroyedCalls", "Untouched", "Ticks", "StateValue"})
        properties.emplace(name, classes.Property(name, "IntProperty", probe, classesCore));
    for (const auto& name : {"Journal", "RedirectOwner", "ObservedOwner", "ObservedBase", "LastArgument", "Link"})
        properties.emplace(name, classes.Property(name, "ObjectProperty", probe, classesCore, 1u, importedActor));
    properties.emplace("ObservedStanding", classes.Property("ObservedStanding", "ByteProperty", probe, classesCore));
    for (const auto& name : {"RedirectOnce", "ObservedDeleted"}) properties.emplace(name, classes.Property(name, "BoolProperty", probe, classesCore));
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
        if (name == "Attach" || name == "Detach") code = Join({code, Assign(property("ObservedStanding"), Ref(0x01u, standingProperty))});
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
    const auto classesTable = fixture.Write(classes);

    Package map; map.stem = "LifecycleFixture";
    const auto mapClasses = map.ImportPackage("LifecycleClasses"), mapEngine = map.ImportPackage("Engine");
    std::map<std::string, std::int32_t> actors;
    actors.emplace("Level0", map.Export("Level0", map.Import("LevelInfo", mapEngine)));
    for (const auto& [name, cls] : std::vector<std::pair<std::string, std::string>>{
        {"Recorder", "Probe"}, {"ParentA", "Probe"}, {"ParentB", "Probe"}, {"Child", "Probe"},
        {"BaseA", "Probe"}, {"BaseB", "Probe"}, {"BasedChild", "Probe"}, {"Victim", "Probe"},
        {"OwnedChild", "Probe"}, {"BasedOnVictim", "Probe"}, {"Peer", "Probe"}, {"Static", "Probe"},
        {"Protected", "Probe"}, {"RecursiveParent", "Recursive"}, {"FailParent", "FailOwner"},
        {"FailBase", "FailBase"}, {"FailVictim", "FailDestroy"}, {"FailPeer", "FailUnTouch"}})
        actors.emplace(name, map.Export(name, map.Import(cls, mapClasses)));
    for (const auto& [name, reference] : actors) {
        Bytes propertiesBytes; map.ObjectTag(propertiesBytes, "Level", actors.at("Level0"));
        if (name == "Level0") map.BoolTag(propertiesBytes, "bBegunPlay", true);
        else map.ObjectTag(propertiesBytes, "Journal", actors.at("Recorder"));
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
    Require(Life(saved, "ParentA").children == std::vector<std::string>{Path("Child")}, "SetOwner did not preserve ordered native child list");
    Call("Child", "Own", {Object("ParentA")});
    Require(Count("ParentA", "Lost") == 1 && Count("ParentA", "Gained") == 2 && Count("Recorder", "Trace") == 121,
        "Same-owner SetOwner incorrectly skipped LostChild/GainedChild or reversed them");
    Call("Child", "Own", {Object("ParentB")});
    HasObject("ParentA", "ObservedOwner", "ParentA", "LostChild did not observe the old Owner before assignment");
    HasObject("ParentB", "ObservedOwner", "ParentB", "New GainedChild did not observe assigned Owner");
    saved = Decode(fixture.Snapshot("OwnerTransfer"));
    Require(Life(saved, "ParentA").children.empty() && Life(saved, "ParentB").children == std::vector<std::string>{Path("Child")},
        "Owner transfer did not remove old list member/add new list member");
    Call("Child", "Own", {Object("")}); HasObject("Child", "Owner", "", "SetOwner(None) did not clear reflected Owner");
    Require(Life(Decode(fixture.Snapshot("OwnerClear")), "ParentB").children.empty(), "SetOwner(None) retained native child relation");
    RefusedCall(fixture, "Child", "OwnThenFail", {Object("ParentA")});
    RefusedCall(fixture, "Child", "Own", {Object("FailParent")}); RefusedCall(fixture, "Child", "BadOwnerType");
    fixture.Reset();
    Call("ParentA", "RedirectNext", {Object("ParentB")}); Call("Child", "Own", {Object("ParentA")});
    HasObject("Child", "Owner", "ParentB", "GainedChild reentrant SetOwner was overwritten by outer operation");
    saved = Decode(fixture.Snapshot("OwnerReentrant"));
    Require(EmptyChildren(saved, "ParentA") &&
        Life(saved, "ParentB").children == std::vector<std::string>{Path("Child"), Path("Child")},
        "SetOwner did not re-read callback-mutated Owner or normalized duplicate pin relations");
    Require(Count("ParentA", "Gained") == 1 && Count("ParentA", "Lost") == 1 && Count("ParentB", "Gained") == 1,
        "Reentrant Owner callbacks did not execute synchronously in one interpreter");
    const auto reentrant = fixture.Snapshot("ReentrantCanonical");
    const auto validatedRevision = GetPortableRuntimeWorldRevision();
    Require(ValidatePortableRuntimeState((fixture.directory / "ReentrantCanonical.sav").string(), "LifecycleFixture"), "Duplicate native relations failed nonmutating save validation");
    Require(fixture.Snapshot("AfterReentrantValidation") == reentrant, "Read-only lifecycle validation mutated native relations");
    Require(GetPortableRuntimeWorldRevision() == validatedRevision, "Read-only lifecycle validation advanced world publication revision");
    Require(LoadPortableRuntimeState((fixture.directory / "ReentrantCanonical.sav").string()) && fixture.Snapshot("ReentrantRestore") == reentrant,
        "Reentrant duplicate native relations failed canonical restore");
    // The structural codec sees only serialized links. Runtime replacement
    // must also count omitted actors' immutable map-load BasedActors baseline,
    // including in read-only validation, before allocating/publishing state.
    auto overTotal = Decode(reentrant);
    overTotal.objects.erase(std::remove_if(overTotal.objects.begin(), overTotal.objects.end(),
        [](const auto& object) { return object.path == Path("BaseA"); }), overTotal.objects.end());
    auto& maximumChildren = Record(overTotal, "ParentB");
    maximumChildren.lifecycle->children.assign(65'536u, Path("Child"));
    maximumChildren.lifecycle->basedActors.clear();
    const auto structuralMaximum = QuestVr::EncodeScriptSavedState(overTotal);
    Require(QuestVr::DecodeScriptSavedState(structuralMaximum).objects.size() == overTotal.objects.size(),
        "Exact codec actor-link limit should be structurally valid before runtime baseline accounting");
    RefusedScript(fixture, reentrant, std::move(overTotal), "SerializedMaxPlusInitialBasedLink");
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
    Require(Count("ParentA", "Gained") == 1 && Life(Decode(fixture.Snapshot("FramelessOwnerCallback")), "ParentA").children.size() == 1u &&
        !ReadPortableActorStateObject(Path("ParentA")), "Frameless recognized-probe Disable suppressed the actual owner relation/callback");
    fixture.Reset(); OwnNullCodeFrame(fixture, "ParentA"); Call("ParentA", "DisableEvent", {Name("gAiNeDcHiLd")});
    Require(ReadPortableActorDispatchContext(Path("ParentA")).disabledNames.contains("gainedchild"), "Owned-frame Disable did not retain its actual probe bit gate");
    Call("Child", "Own", {Object("ParentA")});
    Require(Count("ParentA", "Gained") == 0 && Life(Decode(fixture.Snapshot("OwnerDisabled")), "ParentA").children.size() == 1u,
        "Disabled GainedChild suppressed the owner relation itself or ignored event eligibility");
    Call("ParentA", "EnableEvent", {Name("GAINEDCHILD")}); Call("Child", "Own", {Object("ParentA")});
    Require(Count("ParentA", "Gained") == 1 && !ReadPortableActorDispatchContext(Path("ParentA")).disabledNames.contains("gainedchild"),
        "Owned-frame Enable failed to restore the recognized GainedChild callback");
    fixture.Reset();
}

void BaseTests(Fixture& fixture) {
    Require(Count("BaseA", "StandingCount") == 1, "Authored Base initial native relink did not set StandingCount");
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
    Require(Count("BaseB", "StandingCount") == 0 && Count("BaseB", "ObservedStanding") == 0 && Count("BaseA", "StandingCount") == 2,
        "Base transfer updated counts at the wrong time or dropped authored relation");
    auto saved = Decode(fixture.Snapshot("BaseTransfer"));
    Require(Life(saved, "BaseA").basedActors == std::vector<std::string>{Path("BasedChild"), Path("Child")} && Life(saved, "BaseB").basedActors.empty(),
        "Base transfer did not preserve native based actor order");
    const auto selfBefore = fixture.Snapshot("BeforeSelfBase"); Call("Child", "BaseOn", {Object("Child")});
    Require(fixture.Snapshot("AfterSelfBase") == selfBefore, "SetBase allowed self base or changed state on rejected cycle");
    Call("BaseB", "BaseOn", {Object("Child")});
    const auto cycleBefore = fixture.Snapshot("BeforeBaseCycle"); Call("Child", "BaseOn", {Object("BaseB")});
    Require(fixture.Snapshot("AfterBaseCycle") == cycleBefore, "SetBase allowed an indirect base cycle");
    Call("Child", "BaseOn", {Object("Level0")});
    HasObject("Child", "Base", "Level0", "LevelInfo Base assignment was rejected");
    Require(Count("Level0", "StandingCount") == 0 && Count("BaseA", "StandingCount") == 1,
        "LevelInfo Base received normal based actor counts or old base did not detach");
    saved = Decode(fixture.Snapshot("LevelBase"));
    const auto level = std::find_if(saved.objects.begin(), saved.objects.end(), [&](const auto& value) { return value.path == Path("Level0"); });
    Require(level == saved.objects.end() || !level->lifecycle || level->lifecycle->basedActors.empty(), "SetBase treated LevelInfo as ordinary native based parent");
    Call("Child", "BaseOn", {Object("")}); HasObject("Child", "Base", "", "SetBase(None) did not clear reflected Base");
    RefusedCall(fixture, "Child", "BaseOnThenFail", {Object("BaseB")});
    RefusedCall(fixture, "Child", "BaseOn", {Object("FailBase")}); RefusedCall(fixture, "Child", "BadBaseType");
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
    fixture.Reset(); Require(Published("Victim") && !QuestVr::Vm::ToBool(Read("Victim", "bDeleteMe")) && Count("BaseA", "StandingCount") == 1,
        "Legacy checkpoint failed to restore deleted actor/initial based relation");
    Require(LoadPortableRuntimeState((fixture.directory / "DeletedCallable.sav").string()) && fixture.Snapshot("DeletedRestore") == canonical,
        "Deleted actor/native topology v7 save failed canonical replacement restore");
    Require(!Published("Victim") && Count("Victim", "StateValue") == 88 && Count("Peer", "Untouched") == 1, "v7 restore lost deletion/native callback state");
    // Every prior envelope replaces, rather than merges, native topology. The
    // older implementations had no SetBase lifecycle payload, so their absence
    // restores immutable map-load relinks, not inferred script overlay lists.
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
        Require(Published("Victim") && !QuestVr::Vm::ToBool(Read("Victim", "bDeleteMe")) && Count("BaseA", "StandingCount") == 1,
            "Old envelope retained deletion or omitted initial BasedActors relink: " + std::to_string(version));
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
    auto bad = Decode(canonical); Record(bad, "ParentA").lifecycle->children.push_back("LifecycleFixture.Missing");
    RefusedScript(fixture, canonical, bad, "MissingNativeChild");
    bad = Decode(canonical); Record(bad, "BaseB").lifecycle->basedActors.push_back("LifecycleClasses.Probe");
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
    Require(ReadPortableActorDispatchContext(Path("Victim")).disabledNames.contains("destroyed"), "Owned-frame Destroyed Disable did not retain its probe gate");
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
    Require(fixture.Snapshot("Reinitialized") == fixture.legacy && Published("Victim") && Count("BaseA", "StandingCount") == 1,
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
                  << " rejection controls; actual Owner272/Base298/Destroy279 callbacks, reentrancy, native topology, deletion lifetime, v7/legacy replacement and GC. No Spawn or campaign startup claim.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "Actor lifecycle integration failed: " << error.what() << '\n'; return 1; }
}
