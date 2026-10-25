#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_script_state.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// Serialized UE1 reflection and bytecode exercise the production runtime and
// transaction, not a substitute timer host. Generated files are private temp
// files; the optional owned-game path is always read-only.
namespace {
using Bytes = std::vector<std::uint8_t>;
using Value = QuestVr::Vm::Value;
using Kind = QuestVr::Vm::Kind;
using Evaluation = QuestVr::Vm::Evaluation;
using Status = QuestVr::Vm::Status;
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
    auto rest = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((rest & 63u) | (value < 0 ? 128u : 0u)); rest >>= 6u;
    if (rest) first |= 64u;
    bytes.push_back(first);
    while (rest) { auto next = static_cast<std::uint8_t>(rest & 127u); rest >>= 7u; if (rest) next |= 128u; bytes.push_back(next); }
}
void Replace32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    Require(offset <= bytes.size() && bytes.size() - offset >= 4u, "Fixture word write escaped payload");
    for (unsigned i = 0u; i < 4u; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
}
void Append(Bytes& bytes, const Bytes& other) { bytes.insert(bytes.end(), other.begin(), other.end()); }
struct Code { Bytes raw; std::size_t logical{}; };
Code Token(std::uint8_t op) { return {{op}, 1u}; }
Code Ref(std::uint8_t op, std::int32_t reference) { Code code{{op}, 5u}; Index(code.raw, reference); return code; }
Code Int(std::int32_t value) { Code code{{0x1du}, 5u}; U32(code.raw, static_cast<std::uint32_t>(value)); return code; }
Code Real(float value) { Code code{{0x1eu}, 5u}; FloatBytes(code.raw, value); return code; }
Code Vector(std::array<float, 3> value) { Code code{{0x23u}, 13u}; for (const auto component : value) FloatBytes(code.raw, component); return code; }
Code Join(std::initializer_list<Code> parts) { Code code; for (const auto& part : parts) { Append(code.raw, part.raw); code.logical += part.logical; } return code; }
Code Assign(Code left, Code right) { return Join({Token(0x0fu), std::move(left), std::move(right)}); }
Code Return(Code value = Token(0x0bu)) { return Join({Token(0x04u), std::move(value)}); }
Code Native(std::uint16_t index, std::initializer_list<Code> arguments = {}) {
    Code code;
    if (index >= 256u) { code.raw = {static_cast<std::uint8_t>(0x60u + (index >> 8u)), static_cast<std::uint8_t>(index)}; code.logical = 2u; }
    else { code.raw = {static_cast<std::uint8_t>(index)}; code.logical = 1u; }
    for (const auto& argument : arguments) { Append(code.raw, argument.raw); code.logical += argument.logical; }
    return Join({code, Token(0x16u)});
}
Code Final(std::int32_t function, std::initializer_list<Code> arguments = {}) {
    Code code = Ref(0x1cu, function);
    for (const auto& argument : arguments) { Append(code.raw, argument.raw); code.logical += argument.logical; }
    return Join({code, Token(0x16u)});
}
Code Virtual(std::int32_t name) { return Join({Ref(0x1bu, name), Token(0x16u)}); }
Code Context(Code target, Code expression) {
    Require(expression.logical <= 65535u, "Fixture Context logical skip exceeds uint16");
    Code code = Join({Token(0x19u), std::move(target)}); U16(code.raw, static_cast<std::uint16_t>(expression.logical)); code.raw.push_back(0u); code.logical += 3u;
    Append(code.raw, expression.raw); code.logical += expression.logical; return code;
}
struct Flow {
    Code code;
    std::vector<std::pair<std::size_t, std::string>> patches;
    std::map<std::string, std::size_t> labels;
    void Add(Code part) { Append(code.raw, part.raw); code.logical += part.logical; }
    void IfNot(Code condition, const std::string& label) {
        Add(Token(0x07u)); patches.emplace_back(code.raw.size(), label); U16(code.raw, 0u); code.logical += 2u; Add(std::move(condition));
    }
    void Label(const std::string& name) { Require(labels.emplace(name, code.logical).second, "Duplicate fixture label"); }
    Code Finish() {
        for (const auto& [offset, label] : patches) { const auto target = labels.at(label); Require(target <= 65535u, "Fixture branch exceeds uint16"); code.raw.at(offset) = static_cast<std::uint8_t>(target); code.raw.at(offset + 1u) = static_cast<std::uint8_t>(target >> 8u); }
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
    void ClassBody(std::int32_t reference) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference](std::int32_t next, std::int32_t first) {
            Bytes body; Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjBase); Index(body, next); Index(body, 0); Index(body, first); Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName);
            U32(body, 0); U32(body, 0); U32(body, 0); U64(body, ~std::uint64_t{}); U64(body, ~std::uint64_t{}); U16(body, 0xffffu); U32(body, 0);
            U32(body, 0); body.resize(body.size() + 16u, 0u); for (unsigned i = 0u; i < 5u; ++i) Index(body, 0); return body;
        };
    }
    void StructBody(std::int32_t reference) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference](std::int32_t next, std::int32_t first) {
            Bytes body{0u}; Index(body, 0); Index(body, next); Index(body, 0); Index(body, first); Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName); U32(body, 0); U32(body, 0); U32(body, 0); return body;
        };
    }
    std::int32_t State(const std::string& name, std::int32_t owner, std::int32_t core) {
        const auto reference = Export(name, Import("State", core), 0, owner, true);
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference](std::int32_t next, std::int32_t first) {
            Bytes body{0u}; Index(body, 0); Index(body, next); Index(body, 0); Index(body, first); Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName); U32(body, 0); U32(body, 0); U32(body, 18u); body.push_back(0x08u); body.push_back(0x0cu);
            Index(body,Name("Begin"));U32(body,0u);Index(body,0);U32(body,0xffffffffu);
            U64(body, ~std::uint64_t{}); U64(body, ~std::uint64_t{}); U16(body, 2u); U32(body, 0); return body;
        }; return reference;
    }
    std::int32_t Property(const std::string& name, const std::string& type, std::int32_t owner, std::int32_t core, std::int32_t target = 0, std::uint32_t flags = 0u, std::uint32_t dimension = 1u) {
        const auto reference = Export(name, Import(type, core), 0, owner, true);
        builders.at(static_cast<std::size_t>(reference - 1)) = [type, target, flags, dimension](std::int32_t next, std::int32_t) {
            Bytes body{0u}; Index(body, 0); Index(body, next); U32(body, dimension); U32(body, flags); Index(body, 0);
            if(flags&0x20u)U16(body,0u);
            if (type == "ObjectProperty" || type == "StructProperty" || type == "ByteProperty") Index(body, target);
            else if (type == "ClassProperty") { Index(body, target); Index(body, 0); }
            return body;
        }; return reference;
    }
    std::int32_t Function(const std::string& name, std::int32_t owner, std::int32_t core, Code code = {}, std::uint16_t native = 0u, std::uint32_t flags = 0u) {
        const auto reference = Export(name, Import("Function", core), 0, owner, true); FunctionCode(reference, std::move(code), native, flags); return reference;
    }
    void FunctionCode(std::int32_t reference, Code code, std::uint16_t native = 0u, std::uint32_t flags = 0u) {
        builders.at(static_cast<std::size_t>(reference - 1)) = [this, reference, native, flags, code = std::move(code)](std::int32_t next, std::int32_t first) {
            Bytes body{0u}; Index(body, 0); Index(body, next); Index(body, 0); Index(body, first); Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName); U32(body, 0); U32(body, 0); U32(body, static_cast<std::uint32_t>(code.logical)); Append(body, code.raw); U16(body, native); body.push_back(0u); U32(body, flags ? flags : native ? 0x401u : 2u); return body;
        };
    }
    void Body(std::int32_t reference, Bytes body) { builders.at(static_cast<std::size_t>(reference - 1)) = [body = std::move(body)](std::int32_t, std::int32_t) { return body; }; }
    void Tag(Bytes& bytes, const std::string& name, std::uint8_t type, const Bytes& value) { Require(value.size() <= 255u, "Fixture tag overflow"); Index(bytes, Name(name)); bytes.push_back(static_cast<std::uint8_t>(type | 0x50u)); bytes.push_back(static_cast<std::uint8_t>(value.size())); Append(bytes, value); }
    void ObjectTag(Bytes& bytes, const std::string& name, std::int32_t value) { Bytes data; Index(data, value); Tag(bytes, name, 5u, data); }
    void BoolTag(Bytes& bytes, const std::string& name, bool value) { Index(bytes, Name(name)); bytes.push_back(static_cast<std::uint8_t>(3u | (value ? 0x80u : 0u))); }
    void ActorBody(std::int32_t reference, Bytes properties) { Index(properties, 0); Body(reference, std::move(properties)); }
    Bytes Serialize() {
        std::map<std::int32_t, std::int32_t> next;
        for (const auto& [owner, list] : children) { (void)owner; for (std::size_t i = 1u; i < list.size(); ++i) next[list[i - 1u]] = list[i]; }
        Bytes bytes; U32(bytes, 0x9e2a83c1u); U16(bytes, 68u); U16(bytes, 0u); bytes.resize(56u, 0u);
        std::vector<Bytes> bodies; std::vector<std::int32_t> offsets;
        for (std::size_t i = 0u; i < exports.size(); ++i) {
            const auto reference = static_cast<std::int32_t>(i + 1u); const auto owner = children.find(reference);
            auto body = builders[i] ? builders[i](next[reference], owner == children.end() || owner->second.empty() ? 0 : owner->second.front()) : Bytes{};
            offsets.push_back(static_cast<std::int32_t>(bytes.size())); Append(bytes, body); bodies.push_back(std::move(body));
        }
        Replace32(bytes, 12u, static_cast<std::uint32_t>(names.size())); Replace32(bytes, 16u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& name : names) { Index(bytes, static_cast<std::int32_t>(name.size() + 1u)); bytes.insert(bytes.end(), name.begin(), name.end()); bytes.push_back(0u); U32(bytes, 0); }
        Replace32(bytes, 20u, static_cast<std::uint32_t>(exports.size())); Replace32(bytes, 24u, static_cast<std::uint32_t>(bytes.size()));
        for (std::size_t i = 0u; i < exports.size(); ++i) { const auto& e = exports[i]; Index(bytes, e.ObjClass); Index(bytes, e.ObjBase); U32(bytes, static_cast<std::uint32_t>(e.ObjOuter)); Index(bytes, e.ObjName); U32(bytes, static_cast<std::uint32_t>(e.ObjFlags)); Index(bytes, static_cast<std::int32_t>(bodies[i].size())); if (!bodies[i].empty()) Index(bytes, offsets[i]); }
        Replace32(bytes, 28u, static_cast<std::uint32_t>(imports.size())); Replace32(bytes, 32u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& e : imports) { Index(bytes, e.ClassPackage); Index(bytes, e.ClassName); U32(bytes, static_cast<std::uint32_t>(e.ObjOuter)); Index(bytes, e.ObjName); }
        return bytes;
    }
};
Bytes ReadBytes(const std::filesystem::path& path) { Bytes bytes; Require(QuestVr::ReadBoundedSaveFile(path.string(), QuestVr::kMaximumSaveRuntimeBytes, bytes), "Cannot inspect timer fixture file"); return bytes; }
void WriteBytes(const std::filesystem::path& path, const Bytes& bytes) { Require(QuestVr::WriteDurableSaveFile(path.string(), bytes), "Cannot write timer fixture file"); }
struct Fixture {
    std::filesystem::path parent, directory;
    std::map<std::filesystem::path, Bytes> sources;
    Bytes legacy;
    Fixture() {
        parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned i = 0u; i < 20u; ++i) { const auto candidate = parent / ("deusex-actor-timer-test-" + std::to_string(stamp) + '-' + std::to_string(i)); if (std::filesystem::create_directory(candidate)) { directory = std::filesystem::canonical(candidate); break; } }
        Require(!directory.empty() && directory.parent_path() == parent, "Timer fixture escaped temporary parent");
        std::filesystem::create_directory(directory / "System"); std::filesystem::create_directory(directory / "Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime(); std::error_code error; const auto actual = std::filesystem::weakly_canonical(directory, error);
        if (!error && !directory.empty() && actual == directory && actual.parent_path() == parent && actual.filename().string().rfind("deusex-actor-timer-test-", 0u) == 0u) std::filesystem::remove_all(actual, error);
    }
    PortablePackageTables Write(Package& package, bool map = false) { const auto path = directory / (map ? "Maps" : "System") / (package.stem + (map ? ".dx" : ".u")); auto bytes = package.Serialize(); WriteBytes(path, bytes); sources.emplace(path, bytes); return LoadPortablePackageTables(path.string()); }
    Bytes Snapshot(const std::string& name) const { const auto path = directory / (name + ".sav"); Require(SavePortableRuntimeState(path.string()), "Cannot save timer fixture"); return ReadBytes(path); }
    void Reset() const { Require(LoadPortableRuntimeState((directory / "Legacy.sav").string()), "Cannot restore authored timer fixture"); Require(Snapshot("Reset") == legacy, "Legacy reset retained timer mutations"); }
    void SourcesUnchanged() const { for (const auto& [path, bytes] : sources) Require(ReadBytes(path) == bytes, "Timer execution modified source package"); }
};
std::uint32_t Read32(const Bytes& bytes,std::size_t& cursor) {
    Require(cursor<=bytes.size() && bytes.size()-cursor>=4u,"Timer checkpoint word truncated"); std::uint32_t result{};
    for(unsigned i=0u;i<4u;++i) result|=std::uint32_t(bytes[cursor++])<<(i*8u);
    return result;
}
std::size_t PrefixSize(const Bytes& bytes) {
    std::size_t cursor{}; Require(Read32(bytes,cursor)==0x53515844u,"Timer checkpoint magic mismatch"); const auto version=Read32(bytes,cursor); Require(version>=3u && version<=13u,"Timer checkpoint version mismatch");
    const auto text=[&] {const auto count=Read32(bytes,cursor); Require(cursor<=bytes.size() && count<=bytes.size()-cursor,"Timer checkpoint string escaped payload");cursor+=count;};
    const auto list=[&] {const auto count=Read32(bytes,cursor);Require(count<=100'000u,"Timer checkpoint list exceeds bound");for(std::uint32_t i=0u;i<count;++i)text();};
    list();list();list();static_cast<void>(Read32(bytes,cursor));const auto damaged=Read32(bytes,cursor);Require(damaged<=100'000u,"Timer checkpoint damage list exceeds bound");for(std::uint32_t i=0u;i<damaged;++i){text();static_cast<void>(Read32(bytes,cursor));}
    static_cast<void>(Read32(bytes,cursor));static_cast<void>(Read32(bytes,cursor));for(unsigned i=0u;i<4u;++i)list();return cursor;
}
QuestVr::ScriptSavedState Decode(const Bytes& bytes) {
    auto cursor=PrefixSize(bytes);const auto size=Read32(bytes,cursor);Require(size==bytes.size()-cursor,"Timer script trailer length mismatch");return QuestVr::DecodeScriptSavedState(Bytes(bytes.begin()+static_cast<std::ptrdiff_t>(cursor),bytes.end()));
}
Bytes Envelope(const Bytes& prefixSource,const QuestVr::ScriptSavedState& state) {
    const auto prefix=PrefixSize(prefixSource);Bytes bytes(prefixSource.begin(),prefixSource.begin()+static_cast<std::ptrdiff_t>(prefix));const auto blob=QuestVr::EncodeScriptSavedState(state);Replace32(bytes,4u,3u+blob.at(6u));U32(bytes,static_cast<std::uint32_t>(blob.size()));Append(bytes,blob);return bytes;
}
QuestVr::ScriptSavedObject& Record(QuestVr::ScriptSavedState& state,const std::string& path) {
    const auto found=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& object){return object.path==path;});Require(found!=state.objects.end(),"Timer checkpoint lacks record "+path);return *found;
}
void Property(QuestVr::ScriptSavedObject& object,const std::string& key,const std::string& name,Value value) {
    const auto found=std::find_if(object.properties.begin(),object.properties.end(),[&](const auto& property){return property.key==key && property.index==0u;});
    if(found==object.properties.end())object.properties.push_back({key,name,0u,std::move(value)});else found->value=std::move(value);
}
std::string Path(const std::string& actor) { return "TimerFixture." + actor; }
Evaluation Number(float value) { return {Value::Float(value), {}}; }
Evaluation Boolean(bool value) { return {Value::Bool(value), {}}; }
Evaluation Name(const std::string& value) { return {Value::Text(Kind::Name, value), {}}; }
QuestVr::Vm::Result Call(const std::string& actor, const std::string& function, const std::vector<Evaluation>& args = {}) { const auto result = ExecutePortableActorFunction(Path(actor), function, args); Require(result.passed() && result.committed, "Timer fixture call " + function + " failed: " + result.error); return result; }
Value Read(const std::string& actor, const std::string& property) { return ReadPortableActorScriptProperty(Path(actor), property); }
void Equal(const Value& actual, const Value& expected, const std::string& context) { Require(QuestVr::Vm::Equal(actual, expected), context); }
void Timers(const std::string& actor, float rate, float counter, bool loop, const std::string& context) { Equal(Read(actor, "TimerRate"), Value::Float(rate), context + " Rate"); Equal(Read(actor, "TimerCounter"), Value::Float(counter), context + " Counter"); Equal(Read(actor, "bTimerLoop"), Value::Bool(loop), context + " Loop"); }
struct Tables { std::vector<PortablePackageTables> packages; PortablePackageTables map; };
Tables Build(Fixture& fixture, unsigned schema = 0u) {
    Package core; core.stem = "Core"; const auto cp = core.ImportPackage("Core"), object = core.Export("Object"), meta = core.Export("Class", 0, object);
    core.Property("Class", "ClassProperty", object, cp, meta); core.Property("Name", "NameProperty", object, cp); core.Property("Outer", "ObjectProperty", object, cp, object); core.Property("ObjectFlags", "IntProperty", object, cp);
    const auto vec = core.Export("Vector", core.Import("Struct", cp), 0, object), rot = core.Export("Rotator", core.Import("Struct", cp), 0, object);
    for (const auto* name : {"X", "Y", "Z"}) core.Property(name, "FloatProperty", vec, cp);
    for (const auto* name : {"Pitch", "Yaw", "Roll"}) core.Property(name, "IntProperty", rot, cp);
    core.ClassBody(object); core.ClassBody(meta); core.StructBody(vec); core.StructBody(rot); const auto coreTable = fixture.Write(core);
    Package engine; engine.stem = "Engine"; const auto ec = engine.ImportPackage("Core"), eo = engine.Import("Object", ec), ev = engine.Import("Vector", eo, "Struct"), er = engine.Import("Rotator", eo, "Struct");
    const auto actor = engine.Export("Actor", 0, eo), zone = engine.Export("ZoneInfo", 0, actor), levelInfo = engine.Export("LevelInfo", 0, zone), level = engine.Export("Level", 0, eo), model = engine.Export("Model", 0, eo);
    const auto pointRegion=engine.Export("PointRegion",engine.Import("Struct",ec),0,actor);
    engine.Property("Zone","ObjectProperty",pointRegion,ec,zone);engine.Property("iLeaf","IntProperty",pointRegion,ec);engine.Property("ZoneNumber","ByteProperty",pointRegion,ec);engine.StructBody(pointRegion);
    engine.Property("Owner", "ObjectProperty", actor, ec, actor, 0x22u);
    engine.Property("Instigator", "ObjectProperty", actor, ec, actor);
    for (const auto* name : {"LostChild", "GainedChild"}) {
        const auto event = engine.Function(name, actor, ec, Return(), 0u, 0x800u);
        engine.Property("Other", "ObjectProperty", event, ec, actor, 0x80u);
    }
    engine.Property("Base", "ObjectProperty", actor, ec, actor, 0x22u);
    engine.Property("StandingCount", "ByteProperty", actor, ec, 0, 0x2u);
    engine.Property("Touching","ObjectProperty",actor,ec,actor,0u,4u);
    engine.Property("Brush","ObjectProperty",actor,ec,actor);engine.Property("Region","StructProperty",actor,ec,pointRegion);engine.Property("OldLocation","StructProperty",actor,ec,ev);
    for(const auto* name:{"bTicked","bCollideWorld","bCollideWhenPlacing","bCollideActors","bBlockActors","bBlockPlayers"})engine.Property(name,"BoolProperty",actor,ec,0,std::string(name)=="bTicked"?0x2002u:0u);
    for(const auto* name:{"CollisionRadius","CollisionHeight"})engine.Property(name,"FloatProperty",actor,ec);
    engine.Property("Level", "ObjectProperty", actor, ec, levelInfo, 0x2u); engine.Property("XLevel", "ObjectProperty", actor, ec, level);
    const auto location = engine.Property("Location", "StructProperty", actor, ec, ev);
    engine.Property("Rotation", "StructProperty", actor, ec, er); engine.Property("Tag", "NameProperty", actor, ec);
    const auto deleted = engine.Property("bDeleteMe", "BoolProperty", actor, ec); engine.Property("bNoDelete", "BoolProperty", actor, ec); engine.Property("bStatic", "BoolProperty", actor, ec);
    const auto rate = engine.Property("TimerRate", "FloatProperty", actor, ec), counter = engine.Property("TimerCounter", "FloatProperty", actor, ec, 0, schema==4u ? 0u : 2u), loop = engine.Property("bTimerLoop", "BoolProperty", actor, ec);
    const auto begun = engine.Property("bBegunPlay", "BoolProperty", levelInfo, ec);
    const auto setTimer = engine.Function("SetTimer", actor, ec, {}, schema == 9u ? 281u : 280u, schema == 1u ? 0x402u : 0x401u);
    engine.Property(schema == 7u ? "WrongRateName" : "NewTimerRate", schema == 2u ? "IntProperty" : "FloatProperty", setTimer, ec, 0, schema == 3u ? 0x90u : 0x80u, schema == 6u ? 2u : 1u);
    if (schema != 8u) engine.Property("bLoop", schema == 5u ? "FloatProperty" : "BoolProperty", setTimer, ec, 0, 0x80u);
    if(schema==10u)engine.Property("ReturnValue","BoolProperty",setTimer,ec,0,0x580u);
    engine.Function("Timer", actor, ec, Return(), 0u, 0x800u);
    const auto gotoState = engine.Function("GotoState", actor, ec, {}, 113u); engine.Property("NewState", "NameProperty", gotoState, ec, 0, 0x90u); engine.Property("Label", "NameProperty", gotoState, ec, 0, 0x90u);
    for (const auto& [name, index] : std::vector<std::pair<std::string, std::uint16_t>>{{"Enable",117u},{"Disable",118u}}) { const auto fn = engine.Function(name, actor, ec, {}, index); engine.Property("ProbeFunc", "NameProperty", fn, ec, 0, 0x80u); }
    const auto destroy = engine.Function("Destroy", actor, ec, {}, 279u); engine.Property("ReturnValue", "BoolProperty", destroy, ec, 0, 0x480u);
    engine.Function("Destroyed", actor, ec, Return(), 0u, 0x800u);
    for (const auto& [name, field, type] : std::vector<std::tuple<std::string,std::int32_t,std::string>>{{"SetBegun",begun,"BoolProperty"},{"SetDeleted",deleted,"BoolProperty"},{"SetRawRate",rate,"FloatProperty"},{"SetRawLoop",loop,"BoolProperty"}}) { const auto owner = name == "SetBegun" ? levelInfo : actor; const auto fn = engine.Function(name, owner, ec); const auto input = engine.Property("Input", type, fn, ec, 0, 0x80u); engine.FunctionCode(fn, Join({Assign(Ref(0x01u,field),Ref(0x00u,input)),Return()})); }
    engine.Function("WriteConstCounter", actor, ec, Join({Assign(Ref(0x01u,counter),Real(9.0f)),Return()}));
    engine.Function("ArmThenFail", actor, ec, Join({Native(280u,{Real(5.0f),Token(0x27u)}),Native(999u),Return()}));
    engine.Function("ArmOpcode", actor, ec, Join({Native(280u,{Real(1.0f),Token(0x27u)}),Return()}));
    const auto argumentCounter=engine.Property("ArgumentCounter","IntProperty",actor,ec);
    const auto loopArgument=engine.Function("LoopArgument",actor,ec);
    engine.Property("ReturnValue","BoolProperty",loopArgument,ec,0,0x580u);
    engine.FunctionCode(loopArgument,Join({Assign(Ref(0x01u,argumentCounter),Native(146u,{Ref(0x01u,argumentCounter),Int(1)})),Assign(Ref(0x01u,rate),Real(9.0f)),Return(Token(0x27u))}));
    engine.Function("ArmAliasedArguments",actor,ec,Join({Native(280u,{Ref(0x01u,rate),Final(loopArgument)}),Return()}));
    engine.Function("ArmArgumentsThenFail",actor,ec,Join({Native(280u,{Ref(0x01u,rate),Final(loopArgument)}),Native(999u),Return()}));
    for (const auto reference : {actor,zone,levelInfo,level,model}) engine.ClassBody(reference);
    const auto engineTable = fixture.Write(engine);
    Package classes; classes.stem = "TimerClasses"; const auto cc = classes.ImportPackage("Core"), ce = classes.ImportPackage("Engine"), ca = classes.Import("Actor",ce), cl = classes.Import("LevelInfo",ce), co = classes.Import("Object",cc);
    const auto probe = classes.Export("Probe",0,ca), other = classes.Export("NotActor",0,co);
    const auto count = classes.Property("Count","IntProperty",probe,cc), mode = classes.Property("Mode","IntProperty",probe,cc), observedRate = classes.Property("ObservedRate","FloatProperty",probe,cc), observedCounter = classes.Property("ObservedCounter","FloatProperty",probe,cc), observedLoop = classes.Property("ObservedLoop","BoolProperty",probe,cc), link = classes.Property("Link","ObjectProperty",probe,cc,ca);
    const auto importedRate = classes.Import("TimerRate",ca,"FloatProperty"), importedCounter = classes.Import("TimerCounter",ca,"FloatProperty"), importedLoop = classes.Import("bTimerLoop",ca,"BoolProperty"), importedLocation = classes.Import("Location",ca,"StructProperty"), importedLevel=classes.Import("Level",ca,"ObjectProperty"), importedBegun=classes.Import("bBegunPlay",cl,"BoolProperty");
    const auto setMode = classes.Function("SetMode",probe,cc); const auto input = classes.Property("Input","IntProperty",setMode,cc,0,0x80u); classes.FunctionCode(setMode,Join({Assign(Ref(0x01u,mode),Ref(0x00u,input)),Return()}));
    Flow callback;
    callback.Add(Assign(Ref(0x01u,count),Native(146u,{Ref(0x01u,count),Int(1)})));
    callback.Add(Assign(Ref(0x01u,observedRate),Ref(0x01u,importedRate))); callback.Add(Assign(Ref(0x01u,observedCounter),Ref(0x01u,importedCounter))); callback.Add(Assign(Ref(0x01u,observedLoop),Ref(0x01u,importedLoop)));
    const auto branch = [&](int value, Code code) { const auto label = "mode" + std::to_string(value); callback.IfNot(Native(154u,{Ref(0x01u,mode),Int(value)}),label); callback.Add(std::move(code)); callback.Label(label); };
    branch(1,Native(280u,{Real(2.0f),Token(0x27u)}));
    branch(2,Join({Assign(Ref(0x01u,importedRate),Real(4.0f)),Assign(Ref(0x01u,importedLoop),Token(0x28u))}));
    branch(3,Native(118u,{Ref(0x21u,classes.Name("Timer"))}));
    branch(4,Native(113u,{Ref(0x21u,classes.Name("Second"))}));
    branch(5,Native(279u));
    // Retail Let evaluates its operands on Frame.Object: the selected object
    // belongs in the LHS Context, not around the whole assignment.
    const auto birth=Join({Assign(Context(Ref(0x01u,importedLevel),Ref(0x01u,importedBegun)),Token(0x28u)),Native(278u,{Ref(0x20u,probe)}),Assign(Context(Ref(0x01u,importedLevel),Ref(0x01u,importedBegun)),Token(0x27u))});
    branch(6,Join({Assign(Context(Ref(0x01u,link),Ref(0x01u,importedLocation)),Vector({90.0f,80.0f,70.0f})),Native(195u),birth,Native(999u)}));
    branch(7,Assign(Ref(0x01u,importedCounter),Real(88.0f)));
    branch(8,Virtual(classes.Name("Timer")));
    branch(9,birth);
    callback.Add(Return()); const auto classTimer = classes.Function("Timer",probe,cc,callback.Finish(),0u,0x800u);
    const auto first = classes.State("First",probe,cc), second = classes.State("Second",probe,cc);
    classes.Function("Timer",first,cc,Join({Final(classTimer),Return()}),0u,0x800u);
    classes.Function("Timer",second,cc,Join({Assign(Ref(0x01u,count),Native(146u,{Ref(0x01u,count),Int(100)})),Return()}),0u,0x800u);
    classes.Function("ArmOpcode",other,cc,Join({Native(280u,{Real(1.0f),Token(0x27u)}),Return()}));
    classes.ClassBody(probe); classes.ClassBody(other); const auto classTable = fixture.Write(classes);
    Package map; map.stem="TimerFixture"; const auto me=map.ImportPackage("Engine"), mc=map.ImportPackage("TimerClasses");
    const auto world=map.Export("MyLevel",map.Import("Level",me)), worldModel=map.Export("Model0",map.Import("Model",me)), levelActor=map.Export("Level0",map.Import("LevelInfo",me)), driver=map.Export("Driver",map.Import("Probe",mc)), receiver=map.Export("Other",map.Import("Probe",mc)), nonactor=map.Export("Object0",map.Import("NotActor",mc));
    for (const auto reference : {levelActor,driver,receiver}) {
        Bytes properties;
        if(reference==driver) {
            // GotoState cannot create a native FStateFrame on a frameless
            // actor. This actor has an actual serialized dormant class-backed
            // stack; Other intentionally has none for the frameless control.
            auto& entry=map.exports.at(static_cast<std::size_t>(reference-1));entry.ObjFlags=ObjectFlags::HasStack;
            Index(properties,entry.ObjClass);Index(properties,entry.ObjClass);U64(properties,~std::uint64_t{});U32(properties,0u);Index(properties,-1);
        }
        map.ObjectTag(properties,"Level",levelActor); map.ObjectTag(properties,"XLevel",world); if(reference==levelActor) map.BoolTag(properties,"bBegunPlay",true); if(reference==driver) map.ObjectTag(properties,"Link",receiver); map.ActorBody(reference,std::move(properties));
    }
    map.ActorBody(nonactor,{});
    Bytes worldBytes; Index(worldBytes,0); U32(worldBytes,3u); U32(worldBytes,3u); for(const auto reference:{levelActor,driver,receiver}) Index(worldBytes,reference); for(unsigned i=0u;i<5u;++i) Index(worldBytes,0); U32(worldBytes,7777u); U32(worldBytes,0u); Index(worldBytes,worldModel); Index(worldBytes,0);
    U32(worldBytes,0u); Index(worldBytes,0); // Original serialized Float time and preliminary +100.
    for(unsigned i=0u;i<16u;++i)Index(worldBytes,0); // +9C pointer sequence, including final +100 alias.
    map.Body(world,std::move(worldBytes));
    Bytes modelBytes; Index(modelBytes,0); for(unsigned i=0u;i<6u;++i) FloatBytes(modelBytes,0); modelBytes.push_back(0); for(unsigned i=0u;i<4u;++i) FloatBytes(modelBytes,0); for(unsigned i=0u;i<5u;++i) Index(modelBytes,0); U32(modelBytes,0); U32(modelBytes,1); Index(modelBytes,levelActor); U64(modelBytes,0); U64(modelBytes,0); for(unsigned i=0u;i<7u;++i) Index(modelBytes,0); U32(modelBytes,1); U32(modelBytes,0); map.Body(worldModel,std::move(modelBytes));
    (void)location;
    return {{coreTable,engineTable,classTable},fixture.Write(map,true)};
}
void Initialize(Fixture& fixture,const Tables& tables) { Require(InitializePortableRuntime(tables.packages).passed && LoadPortableRuntimeMap(tables.map).passed,"Generated timer runtime/map failed initialization"); fixture.legacy=fixture.Snapshot("Legacy"); }
QuestVr::Vm::Result Refusal(Fixture& fixture,const std::string& label,const std::function<QuestVr::Vm::Result()>& action,Status expected=Status::Unsupported) {
    const auto bytes=fixture.Snapshot("BeforeRefusal"); const auto revision=GetPortableRuntimeWorldRevision(); const auto gc=GC::GetStats(); const auto paths=GetPortableRuntimeMapActors(true);
    const auto result=action(); const auto afterGc=GC::GetStats();
    Require(!result.passed() && !result.committed && result.status==expected,"Timer refusal status: "+label+": "+result.error);
    Require(afterGc.numObjects==gc.numObjects && afterGc.memoryUsage==gc.memoryUsage,"Timer refusal leaked UObject allocation: "+label);
    Require(fixture.Snapshot("AfterRefusal")==bytes && GetPortableRuntimeWorldRevision()==revision && GetPortableRuntimeMapActors(true).size()==paths.size(),"Timer refusal changed save/revision/actors: "+label); ++refusals;
    return result;
}
void Synthetic() {
    Fixture fixture; const auto tables=Build(fixture); Initialize(fixture,tables);
    const auto arm=[&](float rate,bool loop) { return Call("Driver","SetTimer",{Number(rate),Boolean(loop)}); };
    Call("Other","SetTimer",{Number(1),Boolean(false)});Require(AdvancePortableActorTimer(Path("Other"),1.25f).passed() && !ReadPortableActorStateObject(Path("Other")),"Frameless Timer fabricated state storage or failed callback");Timers("Other",0,.25f,false,"Frameless native timer");Equal(Read("Other","Count"),Value::Integer(1),"Frameless eligible Timer body did not run");fixture.Reset();
    auto revision=GetPortableRuntimeWorldRevision(); auto armed=arm(1.0f,true); Timers("Driver",1,0,true,"Native SetTimer");
    Require(armed.writes==3u,"Native280 did not charge all three original field stores");
    Require(GetPortableRuntimeUnsavedScriptState() && GetPortableRuntimeWorldRevision()==revision && !ReadPortableActorStateObject(Path("Driver")),"Native SetTimer failed persistence or fabricated frame/render change");
    auto result=AdvancePortableActorTimer(Path("Driver"),0.25f); Require(result.passed() && result.committed && result.value.kind==Kind::Bool && !result.value.boolean,"Below-threshold timer reported expiration"); Timers("Driver",1,.25f,true,"Below threshold"); Equal(Read("Driver","Count"),Value::Integer(0),"SetTimer fired immediately"); Require(GetPortableRuntimeWorldRevision()==revision,"Counter-only advance published geometry");
    Require(result.writes==1u && result.instructions==0u,"Counter-only timer store bypassed write accounting or ran a callback");
    arm(1,true); Timers("Driver",1,0,true,"Same-rate rearm");
    Call("Driver","ArmAliasedArguments"); Timers("Driver",1,0,true,"Native arguments capture before later side effect"); Equal(Read("Driver","ArgumentCounter"),Value::Integer(1),"SetTimer skipped a later argument expression");
    revision=GetPortableRuntimeWorldRevision();
    result=AdvancePortableActorTimer(Path("Driver"),1); Require(result.passed() && result.value.boolean,"Exact timer equality failed"); Timers("Driver",1,0,true,"Dormant class-backed equality"); Equal(Read("Driver","Count"),Value::Integer(1),"Dormant class-backed Timer dispatch failed");
    Require(GetPortableRuntimeWorldRevision()>revision,"Successful non-timer callback effects skipped world publication");
    result=AdvancePortableActorTimer(Path("Driver"),3.25f); Require(result.passed() && result.value.boolean,"Long frame did not expire"); Timers("Driver",1,.25f,true,"Coalesced periods"); Equal(Read("Driver","Count"),Value::Integer(2),"Long frame replayed multiple callbacks");
    arm(1,false); Require(AdvancePortableActorTimer(Path("Driver"),2.25f).passed(),"One-shot timer failed"); Timers("Driver",0,.25f,false,"One-shot remainder"); Equal(Read("Driver","ObservedRate"),Value::Float(0),"One-shot callback observed uncleared rate"); Equal(Read("Driver","ObservedCounter"),Value::Float(.25f),"Callback observed stale remainder");
    const auto inactive=fixture.Snapshot("Inactive"); revision=GetPortableRuntimeWorldRevision(); Require(AdvancePortableActorTimer(Path("Driver"),4).passed() && fixture.Snapshot("InactiveAfter")==inactive && GetPortableRuntimeWorldRevision()==revision,"Inactive one-shot consumed retained remainder");
    for(const auto rate:{0.0f,-2.0f}) { arm(rate,true); Require(AdvancePortableActorTimer(Path("Driver"),4).passed(),"Signed inactive timer failed"); Timers("Driver",rate,0,true,"Signed inactive rate"); }
    fixture.Reset(); Call("Driver","GotoState",{Name("First")}); arm(1,false); Call("Driver","SetMode",{{Value::Integer(1),{}}}); Require(AdvancePortableActorTimer(Path("Driver"),5.25f).passed(),"Callback rearm failed"); Timers("Driver",2,0,true,"Rearmed callback"); Equal(Read("Driver","Count"),Value::Integer(1),"Rearm caused a second same-tick callback");
    Call("Driver","SetMode",{{Value::Integer(2),{}}}); Require(AdvancePortableActorTimer(Path("Driver"),2.25f).passed(),"Callback direct timer writes failed"); Timers("Driver",4,.25f,false,"Callback direct rate/loop writes");
    fixture.Reset(); Call("Driver","GotoState",{Name("First")}); arm(1,true); Call("Driver","SetMode",{{Value::Integer(4),{}}}); Require(AdvancePortableActorTimer(Path("Driver"),1).passed(),"Callback state transition failed"); Require(ReadPortableActorDispatchContext(Path("Driver")).stateName=="Second","Callback did not select new StateNode"); Require(AdvancePortableActorTimer(Path("Driver"),1).passed(),"Second state Timer failed"); Equal(Read("Driver","Count"),Value::Integer(101),"Timer cached original class/state callback");
    fixture.Reset(); Call("Driver","GotoState",{Name("First")}); arm(1,true); Call("Driver","SetMode",{{Value::Integer(3),{}}}); Require(AdvancePortableActorTimer(Path("Driver"),1).passed(),"Timer self-disable failed"); const auto suppressedCount=Read("Driver","Count"); Require(AdvancePortableActorTimer(Path("Driver"),2.25f).passed(),"Suppressed Timer failed"); Timers("Driver",1,.25f,true,"Suppressed event consumes periods"); Equal(Read("Driver","Count"),suppressedCount,"Disabled Timer executed body");
    fixture.Reset(); Call("Level0","SetBegun",{Boolean(false)}); arm(1,false); Require(AdvancePortableActorTimer(Path("Driver"),1.25f).passed(),"Before-begun timer failed"); Timers("Driver",0,.25f,false,"Begun-play suppression"); Equal(Read("Driver","Count"),Value::Integer(0),"Before-begun Timer executed body");
    fixture.Reset(); arm(1,false); Call("Driver","SetDeleted",{Boolean(true)}); Require(AdvancePortableActorTimer(Path("Driver"),1.25f).passed(),"Pending-delete timer failed"); Timers("Driver",0,.25f,false,"Pending-delete suppression"); Equal(Read("Driver","Count"),Value::Integer(0),"Pending-delete Timer executed body");
    fixture.Reset(); arm(1,true); auto counterBytes=fixture.Snapshot("CounterSeed"); auto counterState=Decode(counterBytes);
    const auto generated=fixture.directory/"GeneratedCounter.sav";
    Property(Record(counterState,Path("Driver")),"Engine.Actor.TimerCounter","TimerCounter",Value::Float(2.25f)); WriteBytes(generated,Envelope(counterBytes,counterState));
    Require(ValidatePortableRuntimeState(generated.string()) && LoadPortableRuntimeState(generated.string()),"Already-due reflected counter checkpoint rejected");
    result=AdvancePortableActorTimer(Path("Driver"),0.0f);Require(result.passed() && result.value.boolean,"Zero elapsed did not expire already-due counter");Timers("Driver",1,.25f,true,"Already-due zero elapsed");Equal(Read("Driver","Count"),Value::Integer(1),"Already-due timer replayed periods");
    Property(Record(counterState,Path("Driver")),"Engine.Actor.TimerCounter","TimerCounter",Value::Float(-2.0f)); WriteBytes(generated,Envelope(counterBytes,counterState));
    Require(ValidatePortableRuntimeState(generated.string()) && LoadPortableRuntimeState(generated.string()),"Signed reflected timer counter checkpoint rejected");revision=GetPortableRuntimeWorldRevision();
    result=AdvancePortableActorTimer(Path("Driver"),.5f);Require(result.passed() && !result.value.boolean,"Negative counter fabricated an expiration");Timers("Driver",1,-1.5f,true,"Signed counter addition");Require(GetPortableRuntimeWorldRevision()==revision,"Signed counter-only advance rebuilt geometry");
    // These phases have no callback instructions; a zero/partial write or
    // retained-journal budget must still reject them atomically. The old raw
    // host stores silently committed with writes0, so callback-only budget
    // controls could not detect this regression.
    fixture.Reset();arm(1,true);
    for(unsigned mode=0u;mode<3u;++mode) {
        QuestVr::Vm::Limits limits;
        if(mode==0u) limits.writes=0u;
        if(mode==1u) limits.retainedBytes=0u;
        if(mode==2u) limits.retainedBytes=sizeof(Value)-1u;
        const auto rejected=Refusal(fixture,"counter-only native storage budget "+std::to_string(mode),
            [&]{return AdvancePortableActorTimer(Path("Driver"),.25f,limits);},Status::Budget);
        Require(rejected.instructions==0u && rejected.writes==(mode==0u ? 0u : 1u),
            "Counter-only rejection charged an incorrect write count or entered bytecode");
    }
    for(unsigned suppression=0u;suppression<2u;++suppression) {
        fixture.Reset();arm(1,false);
        if(suppression==0u) Call("Level0","SetBegun",{Boolean(false)});
        else Call("Driver","SetDeleted",{Boolean(true)});
        for(unsigned mode=0u;mode<3u;++mode) {
            QuestVr::Vm::Limits limits;
            if(mode<2u) limits.writes=mode;
            else limits.retainedBytes=sizeof(Value);
            const auto rejected=Refusal(fixture,"suppressed one-shot partial storage "+
                std::to_string(suppression)+"/"+std::to_string(mode),
                [&]{return AdvancePortableActorTimer(Path("Driver"),1.25f,limits);},Status::Budget);
            Require(rejected.instructions==0u && rejected.writes==mode,
                "Suppressed one-shot did not account/rollback its partial counter/rate prefix");
        }
        revision=GetPortableRuntimeWorldRevision();
        const auto suppressed=AdvancePortableActorTimer(Path("Driver"),1.25f);
        Require(suppressed.passed() && suppressed.writes==2u && suppressed.instructions==0u &&
            GetPortableRuntimeWorldRevision()==revision,"Suppressed one-shot storage escaped accounting or published geometry");
        Timers("Driver",0,.25f,false,"Budgeted suppressed one-shot");
        Equal(Read("Driver","Count"),Value::Integer(0),"Budgeted suppressed timer entered its callback");
    }
    fixture.Reset();arm(2,true);Require(AdvancePortableActorTimer(Path("Driver"),.25f).passed(),"Native rearm budget seed failed");
    for(std::size_t writes=0u;writes<3u;++writes) {
        QuestVr::Vm::Limits limits;limits.writes=writes;
        const auto rejected=Refusal(fixture,"native280 partial store budget "+std::to_string(writes),
            [&]{return ExecutePortableActorFunction(Path("Driver"),"SetTimer",{Number(1),Boolean(false)},limits);},Status::Budget);
        Require(rejected.writes==writes,"Native280 write budget did not account its rejected original store prefix");
    }
    fixture.Reset(); arm(1,true); Refusal(fixture,"native root rollback",[&]{return ExecutePortableActorFunction(Path("Driver"),"ArmThenFail");});
    Refusal(fixture,"native evaluated argument rollback",[&]{return ExecutePortableActorFunction(Path("Driver"),"ArmArgumentsThenFail");});
    Refusal(fixture,"scalar Const script write",[&]{return ExecutePortableActorFunction(Path("Driver"),"WriteConstCounter");},Status::Invalid);
    for(const auto& args:std::vector<std::vector<Evaluation>>{{},{Number(1)},{Number(1),Boolean(true),Boolean(false)}}) Refusal(fixture,"declared native argument count",[&]{return ExecutePortableActorFunction(Path("Driver"),"SetTimer",args);},Status::Invalid);
    for(const auto& args:std::vector<std::vector<Evaluation>>{{Number(1),Name("True")},{Name("bad"),Boolean(true)},{{Value::Integer(1),{}},Boolean(true)}}) Refusal(fixture,"native argument concrete type",[&]{return ExecutePortableActorFunction(Path("Driver"),"SetTimer",args);});
    Refusal(fixture,"nonactor native receiver",[&]{return ExecutePortableActorFunction(Path("Object0"),"ArmOpcode");});
    Refusal(fixture,"nonactor advance receiver",[&]{return AdvancePortableActorTimer(Path("Object0"),1.0f);});
    Refusal(fixture,"unavailable advance receiver",[&]{return AdvancePortableActorTimer(Path("Missing"),1.0f);});
    for(const auto elapsed:{-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) Refusal(fixture,"invalid elapsed",[&]{return AdvancePortableActorTimer(Path("Driver"),elapsed);},Status::Invalid);
    for(const auto rate:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) Refusal(fixture,"nonfinite public native input",[&]{return ExecutePortableActorFunction(Path("Driver"),"SetTimer",{Number(rate),Boolean(true)});},Status::Invalid);
    for(const auto mode:{6,7,8}) { fixture.Reset(); Call("Driver","GotoState",{Name("First")}); arm(1,false); Call("Driver","SetMode",{{Value::Integer(mode),{}}}); QuestVr::Vm::Limits limits; if(mode==8) limits.callDepth=4u; Refusal(fixture,"callback rollback mode "+std::to_string(mode),[&]{return AdvancePortableActorTimer(Path("Driver"),1.25f,limits);},mode==8 ? Status::Budget : mode==7 ? Status::Invalid : Status::Unsupported); }
    fixture.Reset(); Call("Driver","GotoState",{Name("First")}); arm(1,false);
    for(unsigned kind=0u;kind<4u;++kind) { QuestVr::Vm::Limits limit; if(kind==0u) limit.instructions=0u; if(kind==1u) limit.writes=2u; if(kind==2u) limit.callDepth=1u; if(kind==3u) limit.retainedBytes=0u; Refusal(fixture,"shared timer callback budget "+std::to_string(kind),[&]{return AdvancePortableActorTimer(Path("Driver"),1.25f,limit);},Status::Budget); }
    fixture.Reset(); arm(std::numeric_limits<float>::min(),true); Refusal(fixture,"unsafe whole-period count",[&]{return AdvancePortableActorTimer(Path("Driver"),std::numeric_limits<float>::max());});
    fixture.Reset(); arm(1,false); Call("Driver","SetMode",{{Value::Integer(5),{}}});
    const auto destruction=AdvancePortableActorTimer(Path("Driver"),1.25f);
    Require(destruction.passed(),"Timer destruction callback failed: "+destruction.error);
    Require(QuestVr::Vm::ToBool(Read("Driver","bDeleteMe")),"Timer Destroy lost pending-kill mutation");
    fixture.Reset();arm(1,false);Call("Driver","SetMode",{{Value::Integer(9),{}}});const auto actorsBeforeBirth=GetPortableRuntimeMapActors(true).size();Require(AdvancePortableActorTimer(Path("Driver"),1.25f).passed(),"Timer birth callback failed");Require(GetPortableRuntimeMapActors(true).size()==actorsBeforeBirth+1u && Decode(fixture.Snapshot("TimerBirth")).births.size()==1u && QuestVr::Vm::ToBool(Read("Level0","bBegunPlay")),"Timer callback lost born actor, Level mutation or birth persistence");
    fixture.Reset(); arm(1,true); Require(AdvancePortableActorTimer(Path("Driver"),.25f).passed(),"Cold live timer preparation failed"); const auto cold=fixture.Snapshot("Cold");
    Require(!LoadPortableRuntimeMap(tables.map).passed && fixture.Snapshot("RefusedMapReplacement")==cold,"Map replacement discarded unsaved native timer");
    ShutdownPortableRuntime(); Require(InitializePortableRuntime(tables.packages).passed && LoadPortableRuntimeMap(tables.map).passed,"Cold timer runtime failed"); Require(ValidatePortableRuntimeState((fixture.directory/"Cold.sav").string()) && LoadPortableRuntimeState((fixture.directory/"Cold.sav").string()) && fixture.Snapshot("ColdRestored")==cold,"Cold timer checkpoint changed fields/frame identity"); Timers("Driver",1,.25f,true,"Cold live timer");
    Require(AdvancePortableActorTimer(Path("Driver"),.75f).passed(),"Cold timer future expiration failed");Equal(Read("Driver","Count"),Value::Integer(1),"Cold timer lost its future callback");
    fixture.Reset(); arm(-2,true); const auto signedCold=fixture.Snapshot("SignedCold");ShutdownPortableRuntime();Require(InitializePortableRuntime(tables.packages).passed && LoadPortableRuntimeMap(tables.map).passed && LoadPortableRuntimeState((fixture.directory/"SignedCold.sav").string()) && fixture.Snapshot("SignedColdRestored")==signedCold,"Cold signed inactive rate failed exact restore");Timers("Driver",-2,0,true,"Cold signed inactive timer");fixture.Reset();
    Require(LoadPortableRuntimeMap(tables.map).passed,"Clean map replacement after legacy reset failed");Timers("Driver",0,0,false,"Map retirement resets timer overlays");
    fixture.SourcesUnchanged();
    for(const auto schema:{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u}) { Fixture bad; const auto malformed=Build(bad,schema); Initialize(bad,malformed); Refusal(bad,"malformed authored declaration "+std::to_string(schema),[&]{return ExecutePortableActorFunction(Path("Driver"),"ArmOpcode");}); bad.SourcesUnchanged(); }
}
void Original(const std::filesystem::path& root) {
    Require(std::filesystem::exists(root/"System"/"Engine.u") && std::filesystem::exists(root/"Maps"/"00_Training.dx"),"Owned Deus Ex installation is missing required data");
    Fixture fixture;
    static constexpr const char* names[]={"ConSys","Core","DeusEx","DeusExCharacters","DeusExConAudioAIBarks","DeusExConAudioEndGame","DeusExConAudioHK_Shared","DeusExConAudioIntro","DeusExConAudioMission00","DeusExConAudioMission01","DeusExConAudioMission02","DeusExConAudioMission03","DeusExConAudioMission04","DeusExConAudioMission05","DeusExConAudioMission08","DeusExConAudioMission09","DeusExConAudioMission10","DeusExConAudioMission11","DeusExConAudioMission12","DeusExConAudioMission14","DeusExConAudioMission15","DeusExConAudioNYShared","DeusExConText","DeusExConversations","DeusExDeco","DeusExItems","DeusExSounds","DeusExText","DeusExUI","Editor","Engine","Extension","Fire","IpDrv","IpServer","MPCharacters","UBrowser","UWindow"};
    std::vector<PortablePackageTables> tables;for(const auto* name:names)tables.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
    const auto& engine=*std::find_if(tables.begin(),tables.end(),[](const auto& package){return std::filesystem::path(package.sourcePath).stem()=="Engine";});
    const auto declaration=LoadPortableFunctionScript(engine,FindPortableExport(engine,"Actor.SetTimer"));
    Require(declaration.nativeIndex==280u && declaration.functionFlags==0x401u && declaration.logicalSize==10u && declaration.bytecode.size()==10u && declaration.bytecode[0]==0x29u && declaration.bytecode[5]==0x29u,"Owned SetTimer NativeParm declaration changed");
    const auto rateParameter=LoadPortablePropertyDescriptor(engine,FindPortableExport(engine,"Actor.SetTimer.NewTimerRate"));
    const auto loopParameter=LoadPortablePropertyDescriptor(engine,FindPortableExport(engine,"Actor.SetTimer.bLoop"));
    const auto counterProperty=LoadPortablePropertyDescriptor(engine,FindPortableExport(engine,"Actor.TimerCounter"));
    Require(rateParameter.type=="FloatProperty" && rateParameter.flags==0x80u && rateParameter.arrayDimension==1 && loopParameter.type=="BoolProperty" && loopParameter.flags==0x80u && loopParameter.arrayDimension==1 && counterProperty.type=="FloatProperty" && counterProperty.flags==2u && counterProperty.arrayDimension==1,"Owned timer parameter/Const schema changed");
    Require(InitializePortableRuntime(tables).passed,"Owned timer runtime initialization failed");
    const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());Require(LoadPortableRuntimeMap(map).passed,"Owned Training timer map failed");fixture.legacy=fixture.Snapshot("Legacy");
    const std::string doctor="00_Training.Doctor1";
    const auto levelValue=ReadPortableActorScriptProperty(doctor,"Level");Require(levelValue.kind==Kind::Object && !levelValue.text.empty() && !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(levelValue.text,"bBegunPlay")),"Owned timer fixture silently began world play");
    const auto call=[&](const std::string& actor,const std::string& function,const std::vector<Evaluation>& args=std::vector<Evaluation>{}){const auto result=ExecutePortableActorFunction(actor,function,args);Require(result.passed() && result.committed,"Owned timer call "+function+" failed: "+result.error);return result;};
    const auto timerFields=[&](const std::string& actor,float rate,float counter,bool loop){Equal(ReadPortableActorScriptProperty(actor,"TimerRate"),Value::Float(rate),"Owned Rate");Equal(ReadPortableActorScriptProperty(actor,"TimerCounter"),Value::Float(counter),"Owned Counter");Equal(ReadPortableActorScriptProperty(actor,"bTimerLoop"),Value::Bool(loop),"Owned Loop");};
    const auto spawn=[&](const std::string& cls){return call(doctor,"Spawn",{{Value::Text(Kind::Object,cls),{}},{Value::Text(Kind::Object,doctor),{}},Name("None"),{Value::Vector({0.0f,0.0f,100.0f}),{}},{Value::Rotator({0,0,0}),{}}}).value.text;};
    call(doctor,"PlayWaiting"); auto animationBefore=Decode(fixture.Snapshot("AnimationBeforeTimer"));Require(Record(animationBefore,doctor).clock.has_value(),"Owned timer preservation control lacks real native animation clock");
    const auto animationRevision=GetPortableRuntimeWorldRevision();call(doctor,"SetTimer",{Number(1),Boolean(true)});Require(AdvancePortableActorTimer(doctor,.25f).passed(),"Owned animation/timer counter control failed");call(doctor,"SetTimer",{Number(1),Boolean(true)});
    auto animationAfter=Decode(fixture.Snapshot("AnimationAfterTimer"));const auto withoutTimers=[](QuestVr::ScriptSavedState& state){for(auto& object:state.objects)object.properties.erase(std::remove_if(object.properties.begin(),object.properties.end(),[](const auto& property){return property.key=="Engine.Actor.TimerRate" || property.key=="Engine.Actor.TimerCounter" || property.key=="Engine.Actor.bTimerLoop";}),object.properties.end());};
    withoutTimers(animationBefore);withoutTimers(animationAfter);Require(QuestVr::EncodeScriptSavedState(animationBefore)==QuestVr::EncodeScriptSavedState(animationAfter) && GetPortableRuntimeWorldRevision()==animationRevision,"Timer-only update/rearm rebuilt real animation clock, pose/history, native graph or world geometry");Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Owned animation preservation reset failed");
    const auto pri=spawn("Engine.PlayerReplicationInfo");Require(!pri.empty(),"Owned PlayerReplicationInfo birth failed");
    const auto beforePri=Decode(fixture.Snapshot("BeforePri"));Require(!beforePri.randomSeed,"Owned dormant spawn drew RNG before PostBeginPlay");
    const auto authored=call(pri,"PostBeginPlay");Require(authored.instructions>0u,"Owned PRI PostBeginPlay did not execute authored bytecode");timerFields(pri,2.0f,0.0f,true);
    Require(Decode(fixture.Snapshot("AfterPri")).randomSeed.has_value(),"Owned explicit Timer call before native280 did not execute its actual RNG branch");
    // Full untouched caller above must succeed. The isolated advance remains
    // before-begun-play and therefore consumes periods without replaying Timer.
    auto revision=GetPortableRuntimeWorldRevision();Require(AdvancePortableActorTimer(pri,4.5f).passed(),"Owned PRI timer advance failed");timerFields(pri,2,.5f,true);Require(GetPortableRuntimeWorldRevision()==revision,"Owned suppressed timer-only advance rebuilt geometry");
    const auto datalink=spawn("DeusEx.DataLinkPlay");Require(!datalink.empty(),"Owned DataLinkPlay birth failed");
    call(datalink,"GotoState",{Name("WaitForSpeech"),Name("Idle")});
    auto baseline=fixture.Snapshot("DataLinkSelected");auto state=Decode(baseline);auto& datalinkRecord=Record(state,datalink);
    Require(datalinkRecord.state && datalinkRecord.state->frame && datalinkRecord.state->frame->control,"Owned DataLink state lacks exact selected/running identity");
    const auto layout=ReadPortableRuntimeStateProgram("DeusEx.DataLinkPlay.WaitForSpeech");
    const auto nativeStatement=std::find(layout.statementOffsets.begin(),layout.statementOffsets.end(),154u);Require(nativeStatement!=layout.statementOffsets.end(),"Owned WaitForSpeech fallback native280 is no longer a statement at PC154");
    datalinkRecord.state->frame->statementIndex=static_cast<std::uint32_t>(nativeStatement-layout.statementOffsets.begin());
    Property(datalinkRecord,"DeusEx.ConPlayBase.lastSpeechTextLength","lastSpeechTextLength",Value::Integer(20));
    Property(datalinkRecord,"DeusEx.DataLinkPlay.perCharDelay","perCharDelay",Value::Float(.125f));
    const auto generated=fixture.directory/"OriginalDataLink.sav";WriteBytes(generated,Envelope(baseline,state));Require(ValidatePortableRuntimeState(generated.string()) && LoadPortableRuntimeState(generated.string()),"Owned DataLink native statement fixture rejected typed state");
    const auto armed=ResumePortableActorState(datalink);Require(armed.passed() && armed.status==Status::Waiting,"Owned WaitForSpeech native280/text-delay/Idle Sleep path failed: "+armed.error);timerFields(datalink,2.5f,0,false);Require(ReadPortableActorLatentTimeLeft(datalink)==.5f,"Owned DataLink confused Sleep and SetTimer counters");
    baseline=fixture.Snapshot("OriginalDataLinkArmed");state=Decode(baseline);
    const auto actors=GetPortableRuntimeMapActors(true);const auto levelActor=std::find_if(actors.begin(),actors.end(),[&](const auto& actor){return actor.objectPath==levelValue.text;});Require(levelActor!=actors.end(),"Owned Level binding is not a real map actor");
    auto levelRecord=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& object){return object.path==levelValue.text;});
    if(levelRecord==state.objects.end()){state.objects.push_back({levelValue.text,levelActor->classPath,{},{},{},{},{}});levelRecord=std::prev(state.objects.end());}
    Property(*levelRecord,"Engine.LevelInfo.bBegunPlay","bBegunPlay",Value::Bool(true));WriteBytes(generated,Envelope(baseline,state));Require(ValidatePortableRuntimeState(generated.string()) && LoadPortableRuntimeState(generated.string()),"Owned isolated enum-event begun-play fixture failed");
    const auto eventTimer=ReadPortableActorScriptProperty(datalink,"eventTimer");const auto callback=AdvancePortableActorTimer(datalink,2.75f);Require(callback.passed() && callback.value.kind==Kind::Bool && callback.value.boolean,"Owned state-specific Timer event failed: "+callback.error);timerFields(datalink,0,.25f,false);
    Equal(ReadPortableActorScriptProperty(datalink,"eventTimer"),eventTimer,"Owned selected StateNode Timer dispatched the class blinking callback");
    const auto frame=ReadPortableActorStateObject(datalink);Require(frame && frame->frame && frame->frame->codePath=="DeusEx.DataLinkPlay.WaitForSpeech" && frame->frame->statementIndex==0u && frame->frame->latent==QuestVr::StateLatent::Continue && frame->frame->control && frame->frame->control->codePresent,"Owned WaitForSpeech.Timer did not select actual SpeechFinished label");
    const auto saved=fixture.Snapshot("OriginalCold");Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Owned legacy reset failed");ShutdownPortableRuntime();Require(InitializePortableRuntime(tables).passed && LoadPortableRuntimeMap(map).passed,"Owned cold timer runtime failed");
    Require(ValidatePortableRuntimeState((fixture.directory/"OriginalCold.sav").string()) && LoadPortableRuntimeState((fixture.directory/"OriginalCold.sav").string()) && fixture.Snapshot("OriginalColdRestored")==saved,"Owned cold timer/state/RNG/birth checkpoint changed");timerFields(datalink,0,.25f,false);
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()) && !GetPortableRuntimeScriptStatePresent(),"Owned legacy reset retained native timer/birth/state overlays");
    std::cout<<"ORIGINAL timers: exact declaration, unmodified PRI.PostBeginPlay/explicit Timer/RNG/native280, focused WaitForSpeech PC154 compiled fallback and selected state Timer, Sleep isolation, complete cold checkpoint and legacy reset passed; no automatic campaign ticking claim.\n";
}
} // namespace
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        if(argc==2 && std::string(argv[1])=="--original") { std::cout<<"SKIP: supply --original GAME_ROOT for readonly original timer callers\n"; return 77; }
        Require(argc==1 || argc==2 || (argc==3 && std::string(argv[1])=="--original"),"Usage: portable_actor_timer_test [GAME_ROOT | --original [GAME_ROOT]]");
        const auto gc=GC::GetStats().numObjects; Synthetic(); if(argc>1) Original(std::filesystem::path(argv[argc-1])); GC::Collect(); Require(GC::GetStats().numObjects==gc,"Timer suite leaked runtime objects");
        std::cout<<"Actor timer integration: "<<checks<<" checks, "<<refusals<<" atomic rejection controls; serialized UE1 native280, once-per-tick callback dispatch, reflected persistence and shared rollback. No world scheduler/campaign-completion claim.\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<"Timer integration failed: "<<error.what()<<'\n'; return 1; }
}
