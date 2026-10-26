#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "quest_script_state.h"
#include "quest_save_bundle.h"
#include "quest_quick_belt.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// Generated serialized UE1 classes, bytecode, Level and Model exercise the
// production normal dynamic-actor phase. No mock Tick provider or source-game
// mutation is used. This is not complete Level.Tick or headset acceptance.
namespace {
using Bytes = std::vector<std::uint8_t>;
using Value = QuestVr::Vm::Value;
using Kind = QuestVr::Vm::Kind;
using Evaluation = QuestVr::Vm::Evaluation;
using Status = QuestVr::Vm::Status;
std::size_t checks{}, refusals{};
[[maybe_unused]] void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
[[maybe_unused]] void U16(Bytes& bytes, std::uint16_t value) { bytes.push_back(static_cast<std::uint8_t>(value)); bytes.push_back(static_cast<std::uint8_t>(value >> 8u)); }
[[maybe_unused]] void U32(Bytes& bytes, std::uint32_t value) { U16(bytes, static_cast<std::uint16_t>(value)); U16(bytes, static_cast<std::uint16_t>(value >> 16u)); }
[[maybe_unused]] void U64(Bytes& bytes, std::uint64_t value) { U32(bytes, static_cast<std::uint32_t>(value)); U32(bytes, static_cast<std::uint32_t>(value >> 32u)); }
[[maybe_unused]] void FloatBytes(Bytes& bytes, float value) { std::uint32_t bits{}; std::memcpy(&bits, &value, 4u); U32(bytes, bits); }
[[maybe_unused]] void Index(Bytes& bytes, std::int32_t value) {
    auto rest = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((rest & 63u) | (value < 0 ? 128u : 0u)); rest >>= 6u;
    if (rest) first |= 64u;
    bytes.push_back(first);
    while (rest) { auto next = static_cast<std::uint8_t>(rest & 127u); rest >>= 7u; if (rest) next |= 128u; bytes.push_back(next); }
}
[[maybe_unused]] void Replace32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    Require(offset <= bytes.size() && bytes.size() - offset >= 4u, "Fixture word write escaped payload");
    for (unsigned i = 0u; i < 4u; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
}
[[maybe_unused]] void Append(Bytes& bytes, const Bytes& other) { bytes.insert(bytes.end(), other.begin(), other.end()); }
struct Code { Bytes raw; std::size_t logical{}; };
[[maybe_unused]] Code Token(std::uint8_t op) { return {{op}, 1u}; }
[[maybe_unused]] Code Ref(std::uint8_t op, std::int32_t reference) { Code code{{op}, 5u}; Index(code.raw, reference); return code; }
[[maybe_unused]] Code Int(std::int32_t value) { Code code{{0x1du}, 5u}; U32(code.raw, static_cast<std::uint32_t>(value)); return code; }
[[maybe_unused]] Code Real(float value) { Code code{{0x1eu}, 5u}; FloatBytes(code.raw, value); return code; }
[[maybe_unused]] Code Vector(std::array<float, 3> value) { Code code{{0x23u}, 13u}; for (const auto component : value) FloatBytes(code.raw, component); return code; }
[[maybe_unused]] Code Join(std::initializer_list<Code> parts) { Code code; for (const auto& part : parts) { Append(code.raw, part.raw); code.logical += part.logical; } return code; }
[[maybe_unused]] Code Assign(Code left, Code right) { return Join({Token(0x0fu), std::move(left), std::move(right)}); }
[[maybe_unused]] Code Return(Code value = Token(0x0bu)) { return Join({Token(0x04u), std::move(value)}); }
[[maybe_unused]] Code Native(std::uint16_t index, std::initializer_list<Code> arguments = {}) {
    Code code;
    if (index >= 256u) { code.raw = {static_cast<std::uint8_t>(0x60u + (index >> 8u)), static_cast<std::uint8_t>(index)}; code.logical = 2u; }
    else { code.raw = {static_cast<std::uint8_t>(index)}; code.logical = 1u; }
    for (const auto& argument : arguments) { Append(code.raw, argument.raw); code.logical += argument.logical; }
    return Join({code, Token(0x16u)});
}
[[maybe_unused]] Code Final(std::int32_t function, std::initializer_list<Code> arguments = {}) {
    Code code = Ref(0x1cu, function);
    for (const auto& argument : arguments) { Append(code.raw, argument.raw); code.logical += argument.logical; }
    return Join({code, Token(0x16u)});
}
[[maybe_unused]] Code Virtual(std::int32_t name) { return Join({Ref(0x1bu, name), Token(0x16u)}); }
[[maybe_unused]] Code Context(Code target, Code expression) {
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
[[maybe_unused]] Bytes ReadBytes(const std::filesystem::path& path) { Bytes bytes; Require(QuestVr::ReadBoundedSaveFile(path.string(), QuestVr::kMaximumSaveRuntimeBytes, bytes), "Cannot inspect timer fixture file"); return bytes; }
[[maybe_unused]] void WriteBytes(const std::filesystem::path& path, const Bytes& bytes) { Require(QuestVr::WriteDurableSaveFile(path.string(), bytes), "Cannot write timer fixture file"); }

struct Fixture {
    std::filesystem::path parent,directory;
    std::map<std::filesystem::path,Bytes> sources;
    Bytes legacy;
    Fixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("deusex-level-tick-test-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(std::filesystem::create_directory(candidate)){directory=std::filesystem::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Level Tick fixture escaped its temporary parent");
        std::filesystem::create_directory(directory/"System");std::filesystem::create_directory(directory/"Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime();std::error_code error;
        const auto actual=std::filesystem::weakly_canonical(directory,error);
        if(!error&&!directory.empty()&&actual==directory&&actual.parent_path()==parent&&
            actual.filename().string().rfind("deusex-level-tick-test-",0u)==0u)std::filesystem::remove_all(actual,error);
    }
    PortablePackageTables Write(Package& package,bool map=false) {
        const auto path=directory/(map?"Maps":"System")/(package.stem+(map?".dx":".u"));
        auto bytes=package.Serialize();WriteBytes(path,bytes);sources.emplace(path,bytes);
        return LoadPortablePackageTables(path.string());
    }
    Bytes Save(const std::string& name)const {
        const auto path=directory/(name+".sav");
        Require(SavePortableRuntimeState(path.string()),"Cannot save generated Level Tick fixture");
        return ReadBytes(path);
    }
    void Restore(const Bytes& bytes)const {
        const auto path=directory/"Restore.sav";WriteBytes(path,bytes);
        Require(ValidatePortableRuntimeState(path.string()),"Generated Level Tick checkpoint failed preflight");
        Require(LoadPortableRuntimeState(path.string()),"Generated Level Tick checkpoint failed restore");
    }
    void Unchanged()const {for(const auto& [path,bytes]:sources)Require(ReadBytes(path)==bytes,"Tick changed its serialized generated source");}
};
std::uint32_t Word(const Bytes& bytes,std::size_t& cursor) {
    Require(cursor<=bytes.size()&&bytes.size()-cursor>=4u,"Level Tick checkpoint word truncated");
    std::uint32_t result{};for(unsigned i=0u;i<4u;++i)result|=std::uint32_t(bytes[cursor++])<<(8u*i);return result;
}
std::size_t Prefix(const Bytes& bytes) {
    std::size_t cursor{};Require(Word(bytes,cursor)==0x53515844u,"Level Tick checkpoint magic mismatch");
    const auto version=Word(bytes,cursor);
    Require(version>=3u&&version<=19u&&version!=14u&&version!=15u,"Unexpected fixture checkpoint wrapper");
    const auto text=[&]{const auto size=Word(bytes,cursor);Require(cursor<=bytes.size()&&size<=bytes.size()-cursor,"Checkpoint text truncated");cursor+=size;};
    const auto list=[&]{const auto count=Word(bytes,cursor);Require(count<100000u,"Checkpoint list bound");for(std::uint32_t i=0u;i<count;++i)text();};
    list();list();list();static_cast<void>(Word(bytes,cursor));const auto damaged=Word(bytes,cursor);
    for(std::uint32_t i=0u;i<damaged;++i){text();static_cast<void>(Word(bytes,cursor));}
    static_cast<void>(Word(bytes,cursor));static_cast<void>(Word(bytes,cursor));for(unsigned i=0u;i<4u;++i)list();
    return cursor;
}
QuestVr::ScriptSavedState Decode(const Bytes& bytes) {
    std::size_t versionOffset=4u;const auto version=Word(bytes,versionOffset);
    if(version==3u){QuestVr::ScriptSavedState result;result.mapName="LevelTickFixture";return result;}
    auto cursor=Prefix(bytes);const auto length=Word(bytes,cursor);
    Require(length==bytes.size()-cursor,"Tick checkpoint trailer length mismatch");
    return QuestVr::DecodeScriptSavedState(Bytes(bytes.begin()+static_cast<std::ptrdiff_t>(cursor),bytes.end()));
}
Bytes Envelope(const Bytes& prefix,const QuestVr::ScriptSavedState& state) {
    const auto count=Prefix(prefix);Bytes result(prefix.begin(),prefix.begin()+static_cast<std::ptrdiff_t>(count));
    const auto blob=QuestVr::EncodeScriptSavedState(state);const auto codec=blob.at(6u);
    const auto version=codec==14u?19u:codec==13u?18u:codec==12u?17u:codec==11u?16u:3u+codec;
    Replace32(result,4u,version);U32(result,static_cast<std::uint32_t>(blob.size()));Append(result,blob);return result;
}
std::string Path(const std::string& name){return "LevelTickFixture."+name;}
QuestVr::ScriptSavedObject& Object(QuestVr::ScriptSavedState& state,const std::string& name) {
    const auto path=Path(name);
    const auto found=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& item){return item.path==path;});
    if(found!=state.objects.end())return *found;
    state.objects.push_back({path,name=="Level0"?"Engine.LevelInfo":name=="Brush0"?"Engine.Brush":
        name=="Bad0"?"TickClasses.BadNative":"TickClasses.TestActor",{}, {},{},{}});
    return state.objects.back();
}
void Property(QuestVr::ScriptSavedState& state,const std::string& actor,const std::string& key,const Value& value,std::uint32_t index=0u) {
    auto& object=Object(state,actor);const auto found=std::find_if(object.properties.begin(),object.properties.end(),
        [&](const auto& item){return item.key==key&&item.index==index;});
    const auto name=key.substr(key.find_last_of('.')+1u);
    if(found==object.properties.end())object.properties.push_back({key,name,index,value});else found->value=value;
}
struct Tables {std::vector<PortablePackageTables> packages;PortablePackageTables map;std::map<std::string,std::uint32_t> actorExports;};
enum class TriggerControlKind {None,Exact,Parent};
enum class DeusExLevelInfoControlKind {None,Exact,Parent};
Tables Build(Fixture& fixture,bool moverControl=false,std::uint32_t oldLocationFlags=2u,
    TriggerControlKind triggerControl=TriggerControlKind::None,
    DeusExLevelInfoControlKind levelInfoControl=DeusExLevelInfoControlKind::None,bool lightControl=false,
    bool scalarLightControl=false) {
    Require(!scalarLightControl||lightControl,"Scalar-Light fixture must retain its genuine native Light ancestry");
    Require(unsigned(triggerControl!=TriggerControlKind::None)+unsigned(levelInfoControl!=DeusExLevelInfoControlKind::None)+
        unsigned(lightControl)<=1u,
        "Fixture native-boundary controls must retain distinct ancestry proofs");
    const std::string ini="[Engine.Engine]\nGameEngine=Engine.GameEngine\n";
    const auto iniPath=fixture.directory/"System"/"DeusEx.ini";
    WriteBytes(iniPath,Bytes(ini.begin(),ini.end()));fixture.sources.emplace(iniPath,Bytes(ini.begin(),ini.end()));
    Package core;core.stem="Core";const auto cp=core.ImportPackage("Core");
    const auto object=core.Export("Object"),meta=core.Import("Class",cp);
    const auto subsystem=core.Export("Subsystem",0,object);
    for(const auto ref:{object,subsystem})core.exports.at(static_cast<std::size_t>(ref-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);
    core.Property("Class","ClassProperty",object,cp,meta);core.Property("Name","NameProperty",object,cp);
    core.Property("Outer","ObjectProperty",object,cp,object);core.Property("ObjectFlags","IntProperty",object,cp);
    const auto vector=core.Export("Vector",core.Import("Struct",cp),0,object);
    const auto rotator=core.Export("Rotator",core.Import("Struct",cp),0,object);
    const auto plane=core.Export("Plane",core.Import("Struct",cp),0,object);
    for(const auto* name:{"X","Y","Z"})core.Property(name,"FloatProperty",vector,cp);
    for(const auto* name:{"Pitch","Yaw","Roll"})core.Property(name,"IntProperty",rotator,cp);
    for(const auto* name:{"X","Y","Z","W"})core.Property(name,"FloatProperty",plane,cp);
    core.StructBody(vector);core.StructBody(rotator);core.StructBody(plane);core.ClassBody(object);core.ClassBody(subsystem);
    const auto coreTable=fixture.Write(core);
    Package engine;engine.stem="Engine";const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec);
    const auto ev=engine.Import("Vector",eo,"Struct"),er=engine.Import("Rotator",eo,"Struct");
    const auto ep=engine.Import("Plane",eo,"Struct");
    const auto actor=engine.Export("Actor",0,eo),zone=engine.Export("ZoneInfo",0,actor);
    const auto engineClass=engine.Export("Engine",0,engine.Import("Subsystem",ec));
    engine.exports.at(static_cast<std::size_t>(engineClass-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);
    const auto enginePackage=engine.ImportPackage("Engine");
    engine.Property("Cylinder","ObjectProperty",engineClass,ec,engine.Import("Primitive",enginePackage));
    const auto info=engine.Export("LevelInfo",0,zone),brush=engine.Export("Brush",0,actor);
    const auto mover=moverControl?engine.Export("Mover",0,brush):0;
    if(moverControl) {
        engine.exports.at(static_cast<std::size_t>(mover-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);
        engine.ClassBody(mover);
    }
    std::int32_t triggers{},trigger{};
    if(triggerControl!=TriggerControlKind::None) {
        // Owned Engine.u ancestry is Trigger -> native Triggers -> Actor,
        // not a fixture-only direct Trigger -> Actor substitute.
        triggers=engine.Export("Triggers",0,actor);trigger=engine.Export("Trigger",0,triggers);
        for(const auto ref:{triggers,trigger}) {
            engine.exports.at(static_cast<std::size_t>(ref-1)).ObjFlags=static_cast<ObjectFlags>(0x040f0004u);
            engine.ClassBody(ref);
        }
    }
    if(levelInfoControl!=DeusExLevelInfoControlKind::None) {
        const auto nativeInfo=engine.Export("Info",0,actor);
        engine.exports.at(static_cast<std::size_t>(nativeInfo-1)).ObjFlags=static_cast<ObjectFlags>(0x040f0004u);
        engine.ClassBody(nativeInfo);
    }
    if(lightControl) {
        const auto light=engine.Export("Light",0,actor);
        engine.exports.at(static_cast<std::size_t>(light-1)).ObjFlags=static_cast<ObjectFlags>(0x040f0004u);
        engine.ClassBody(light);
    }
    const auto level=engine.Export("Level",0,eo),model=engine.Export("Model",0,eo);
    engine.exports.at(static_cast<std::size_t>(actor-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);
    const auto enumeration=[&](const std::string& name,std::vector<std::string> values,std::int32_t owner) {
        const auto ref=engine.Export(name,engine.Import("Enum",ec),0,owner,true);
        engine.builders.at(static_cast<std::size_t>(ref-1))=[&,values](std::int32_t next,std::int32_t){
            Bytes bytes{0u};Index(bytes,0);Index(bytes,next);Index(bytes,static_cast<std::int32_t>(values.size()));
            for(const auto& value:values) {Index(bytes,engine.Name(value));}
            return bytes;
        };return ref;
    };
    const auto roleEnum=enumeration("ENetRole",{"ROLE_None","ROLE_DumbProxy","ROLE_SimulatedProxy","ROLE_AutonomousProxy","ROLE_Authority","ROLE_MAX"},actor);
    const auto physicsEnum=enumeration("EPhysics",{"PHYS_None","PHYS_Walking","PHYS_Falling","PHYS_Swimming","PHYS_Flying","PHYS_Rotating","PHYS_Projectile","PHYS_Rolling","PHYS_Interpolating","PHYS_MovingBrush","PHYS_Spider","PHYS_Trailer","PHYS_MAX"},actor);
    engine.Property("Role","ByteProperty",actor,ec,roleEnum,0x20u);
    engine.Property("RemoteRole","ByteProperty",actor,ec,roleEnum,0x21u);
    engine.Property("Physics","ByteProperty",actor,ec,physicsEnum,0x23u);
    for(const auto& [name,flags]:std::vector<std::pair<std::string,std::uint32_t>>{
        {"bTicked",0x2002u},{"bDeleteMe",2u},{"bStatic",3u},{"bNoDelete",3u},{"bIsPawn",2u},
        {"bTransient",1u},{"bStasis",1u},{"bForceStasis",1u},{"bCollideActors",0x23u},
        {"bCollideWorld",0u},{"bBlockActors",0u},{"bBlockPlayers",0u},{"bHidden",0u},{"bTimerLoop",0u}})
        engine.Property(name,"BoolProperty",actor,ec,0,flags);
    engine.Property("Owner","ObjectProperty",actor,ec,actor,0x22u);
    engine.Property("Deleted","ObjectProperty",actor,ec,actor,2u);
    engine.Property("Brush","ObjectProperty",actor,ec,model,0x2au);
    engine.Property("Level","ObjectProperty",actor,ec,info,2u);engine.Property("XLevel","ObjectProperty",actor,ec,level);
    engine.Property("Location","StructProperty",actor,ec,ev,0x23u);engine.Property("Rotation","StructProperty",actor,ec,er,0x23u);
    // Exact native moving-brush declarations, not a fixture-only host branch.
    // PendingTouch is initialized by the production native actor loader.
    engine.Property("OldLocation","StructProperty",actor,ec,ev,oldLocationFlags);
    engine.Property("Velocity","StructProperty",actor,ec,ev,0x21u);
    engine.Property("RotationRate","StructProperty",actor,ec,er,0x21u);
    engine.Property("PendingTouch","ObjectProperty",actor,ec,actor);
    engine.Property("bInterpolating","BoolProperty",actor,ec);
    engine.Property("PhysAlpha","FloatProperty",actor,ec);engine.Property("PhysRate","FloatProperty",actor,ec);
    if(moverControl) {
        const auto glide=enumeration("EMoverGlideType",{"MV_MoveByTime","MV_GlideByTime"},mover);
        engine.Property("KeyNum","ByteProperty",mover,ec,0,1u);
        engine.Property("MoverGlideType","ByteProperty",mover,ec,glide,1u);
        engine.Property("KeyPos","StructProperty",mover,ec,ev,0u,8u);
        engine.Property("KeyRot","StructProperty",mover,ec,er,0u,8u);
        for(const auto* name:{"BasePos","OldPos"})engine.Property(name,"StructProperty",mover,ec,ev);
        for(const auto* name:{"BaseRot","OldRot"})engine.Property(name,"StructProperty",mover,ec,er);
    }
    engine.Property("Tag","NameProperty",actor,ec);engine.Property("AnimSequence","NameProperty",actor,ec,0,0x21u);
    engine.Property("Fatness","ByteProperty",actor,ec);
    for(const auto* name:{"bAnimLoop","bAnimNotify","bAnimFinished"})engine.Property(name,"BoolProperty",actor,ec);
    for(const auto& [name,flags]:std::vector<std::pair<std::string,std::uint32_t>>{
        {"AnimFrame",1u},{"AnimRate",1u},{"TweenRate",0u},{"AnimLast",0u},{"AnimMinRate",0x20u},{"OldAnimRate",0u}})
        engine.Property(name,"FloatProperty",actor,ec,0,flags);
    engine.Property("BlendAnimSequence","NameProperty",actor,ec,0,0x20u,4u);
    for(const auto* name:{"BlendAnimFrame","BlendAnimRate","BlendTweenRate","BlendAnimLast","BlendAnimMinRate","OldBlendAnimRate"})
        engine.Property(name,"FloatProperty",actor,ec,0,std::string(name)=="BlendAnimMinRate"?0x20u:0u,4u);
    engine.Property("SimBlendAnim","StructProperty",actor,ec,ep,0x20u,4u);
    for(const auto& [name,flags]:std::vector<std::pair<std::string,std::uint32_t>>{
        {"LastRenderTime",0u},{"DistanceFromPlayer",0u},{"TimerRate",0u},{"TimerCounter",2u},
        {"LifeSpan",1u},{"CollisionRadius",0u},{"CollisionHeight",0u}})
        engine.Property(name,"FloatProperty",actor,ec,0,flags);
    engine.Property("bBegunPlay","BoolProperty",info,ec);
    engine.Property("TimeDilation","FloatProperty",info,ec,0,0x21u);engine.Property("TimeSeconds","FloatProperty",info,ec);
    const auto ledger=engine.Property("Ledger","IntProperty",info,ec);
    const auto baseTick=engine.Function("Tick",actor,ec,Return(),0u,0x800u);
    engine.Property("DeltaTime","FloatProperty",baseTick,ec,0,0x80u);
    engine.Function("Timer",actor,ec,Return(),0u,0x800u);
    const auto interpolateEnd=engine.Function("InterpolateEnd",actor,ec,Return(),0u,0x800u);
    engine.Property("Other","ObjectProperty",interpolateEnd,ec,actor,0x80u);
    Bytes actorDefaults;engine.Tag(actorDefaults,"Role",1u,Bytes{4u});
    engine.ClassBody(actor); // Individual instance Role tags avoid any unproven fixture CDO policy.
    for(const auto ref:{zone,info,brush,level,model,engineClass})engine.ClassBody(ref);
    static_cast<void>(ledger);
    const auto engineTable=fixture.Write(engine);
    std::optional<PortablePackageTables> deusExTable;
    if(levelInfoControl!=DeusExLevelInfoControlKind::None) {
        // The owned native class crosses packages and extends Engine.Info,
        // not Engine.LevelInfo. Keep that real nearest-native boundary.
        Package deusex;deusex.stem="DeusEx";const auto de=deusex.ImportPackage("Engine");
        const auto dxInfo=deusex.Export("DeusExLevelInfo",0,deusex.Import("Info",de));
        deusex.exports.at(static_cast<std::size_t>(dxInfo-1)).ObjFlags=static_cast<ObjectFlags>(0x040f0004u);
        deusex.ClassBody(dxInfo);deusExTable=fixture.Write(deusex);
    }
    Package classes;classes.stem="TickClasses";const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine");
    const auto ca=classes.Import("Actor",ce),ci=classes.Import("LevelInfo",ce);
    const auto testBase=lightControl?classes.Import("Light",ce):levelInfoControl!=DeusExLevelInfoControlKind::None?
        classes.Import("DeusExLevelInfo",classes.ImportPackage("DeusEx")):
        triggerControl==TriggerControlKind::None?ca:classes.Import("Trigger",ce);
    const auto test=classes.Export("TestActor",0,testBase),bad=classes.Export("BadNative",0,testBase);
    classes.exports.at(static_cast<std::size_t>(bad-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);
    const auto count=classes.Property("Count","IntProperty",test,cc),stamp=classes.Property("Stamp","IntProperty",test,cc);
    const auto mode=classes.Property("Mode","IntProperty",test,cc);
    const auto own=classes.Import("Owner",ca,"ObjectProperty"),lv=classes.Import("Level",ca,"ObjectProperty");
    const auto counter=classes.Import("Ledger",ci,"IntProperty");
    const auto tick=classes.Function("Tick",test,cc,{},0u,0x800u);
    const auto delta=classes.Property("DeltaTime","FloatProperty",tick,cc,0,0x80u);
    const auto atLedger=[&]{return Context(Ref(0x01u,lv),Ref(0x01u,counter));};
    Flow body;body.Add(Assign(Ref(0x01u,count),Native(146u,{Ref(0x01u,count),Int(1)})));
    body.Add(Assign(atLedger(),Native(146u,{atLedger(),Int(1)})));body.Add(Assign(Ref(0x01u,stamp),atLedger()));
    if(scalarLightControl) {
        const auto alpha=classes.Property("Alpha","FloatProperty",test,cc),
            direction=classes.Property("Direction","FloatProperty",test,cc),
            changeTime=classes.Property("ChangeTime","FloatProperty",test,cc),
            observedAlpha=classes.Property("ObservedAlpha","FloatProperty",test,cc);
        // The reached original TriggerLight arithmetic and recovery branch,
        // with one explicit observation copy of the freshly STORED Alpha.
        // No finite replacement, helper provider or host-side clamp is used.
        body.Add(Native(184u,{Ref(0x01u,alpha),Native(172u,{
            Native(171u,{Ref(0x01u,direction),Ref(0x00u,delta)}),Ref(0x01u,changeTime)})}));
        body.Add(Assign(Ref(0x01u,observedAlpha),Ref(0x01u,alpha)));
        body.IfNot(Native(154u,{Ref(0x01u,mode),Int(1)}),"scalar-clamp");
        body.Add(Return());body.Label("scalar-clamp");
        body.IfNot(Native(177u,{Ref(0x01u,alpha),Real(1.0f)}),"scalar-not-above");
        body.Add(Assign(Ref(0x01u,alpha),Real(1.0f)));body.Label("scalar-not-above");
        body.IfNot(Native(176u,{Ref(0x01u,alpha),Real(0.0f)}),"scalar-not-below");
        body.Add(Assign(Ref(0x01u,alpha),Real(0.0f)));body.Label("scalar-not-below");
    }
    body.IfNot(Native(154u,{Ref(0x01u,mode),Int(2)}),"no-fault");body.Add(Native(999u));body.Label("no-fault");
    body.Add(Return());classes.FunctionCode(tick,body.Finish(),0u,0x800u);
    const auto setOwner=classes.Function("SetFixtureOwner",test,cc);const auto input=classes.Property("Input","ObjectProperty",setOwner,cc,ca,0x80u);
    classes.FunctionCode(setOwner,Join({Assign(Ref(0x01u,own),Ref(0x00u,input)),Return()}));
    classes.ClassBody(test);classes.ClassBody(bad);const auto classTable=fixture.Write(classes);
    Package map;map.stem="LevelTickFixture";const auto me=map.ImportPackage("Engine"),mt=map.ImportPackage("TickClasses");
    const auto world=map.Export("MyLevel",map.Import("Level",me)),worldModel=map.Export("Model0",map.Import("Model",me));
    std::map<std::string,std::int32_t> actors;
    actors["Level0"]=map.Export("Level0",map.Import("LevelInfo",me));
    actors["Brush0"]=map.Export("Brush0",map.Import("Brush",me));
    actors["Bad0"]=map.Export("Bad0",moverControl?map.Import("Mover",me):
        levelInfoControl==DeusExLevelInfoControlKind::Parent?map.Import("Info",me):
        triggerControl==TriggerControlKind::Parent?map.Import("Triggers",me):map.Import("BadNative",mt));
    for(const auto* name:{"Static0","A","B","C"})actors[name]=map.Export(name,map.Import("TestActor",mt));
    for(const auto& [name,ref]:actors) {
        Bytes properties;map.ObjectTag(properties,"Level",actors.at("Level0"));map.ObjectTag(properties,"XLevel",world);
        map.Tag(properties,"Role",1u,Bytes{4u});
        if(name=="Level0")map.BoolTag(properties,"bBegunPlay",true);
        if(name=="Level0"||name=="Brush0"||name=="Bad0"||name=="Static0")map.BoolTag(properties,"bStatic",true);
        map.ActorBody(ref,std::move(properties));
    }
    const std::array<std::int32_t,7u> order{actors.at("Level0"),actors.at("Brush0"),actors.at("Bad0"),actors.at("Static0"),actors.at("A"),actors.at("B"),actors.at("C")};
    Bytes levelBytes;Index(levelBytes,0);U32(levelBytes,7u);U32(levelBytes,7u);for(const auto ref:order)Index(levelBytes,ref);
    for(unsigned i=0u;i<5u;++i) {Index(levelBytes,0);}
    U32(levelBytes,7777u);U32(levelBytes,0u);
    Index(levelBytes,worldModel);Index(levelBytes,0);U32(levelBytes,0u);Index(levelBytes,0);
    for(unsigned i=0u;i<16u;++i) {Index(levelBytes,0);}
    map.Body(world,std::move(levelBytes));
    Bytes modelBytes;Index(modelBytes,0);for(unsigned i=0u;i<6u;++i)FloatBytes(modelBytes,0.0f);
    modelBytes.push_back(0u);for(unsigned i=0u;i<4u;++i)FloatBytes(modelBytes,0.0f);for(unsigned i=0u;i<5u;++i)Index(modelBytes,0);
    U32(modelBytes,0u);U32(modelBytes,1u);Index(modelBytes,actors.at("Level0"));U64(modelBytes,0u);U64(modelBytes,0u);
    for(unsigned i=0u;i<7u;++i) {Index(modelBytes,0);}
    U32(modelBytes,1u);U32(modelBytes,0u);map.Body(worldModel,std::move(modelBytes));
    Tables result{{coreTable,engineTable},fixture.Write(map,true),{}};
    if(deusExTable)result.packages.push_back(*deusExTable);
    result.packages.push_back(classTable);
    for(const auto& [name,ref]:actors)result.actorExports.emplace(name,static_cast<std::uint32_t>(ref-1));
    return result;
}
QuestVr::ScriptSavedState Seed(const Tables& tables,const std::vector<std::string>& dynamic={"A","B","C"}) {
    QuestVr::ScriptSavedState state;state.mapName="LevelTickFixture";
    QuestVr::ScriptSavedNativeActorOrder order;order.levelPath=Path("MyLevel");order.firstDynamic=4u;
    for(const auto* name:{"Level0","Brush0","Bad0","Static0"})
        order.slots.push_back({QuestVr::ScriptSavedNativeActorSlotKind::AuthoredExport,tables.actorExports.at(name),{}});
    for(const auto& name:dynamic) {
        if(name.empty())order.slots.push_back({});
        else order.slots.push_back({QuestVr::ScriptSavedNativeActorSlotKind::AuthoredExport,tables.actorExports.at(name),{}});
    }
    state.nativeActorOrder=std::move(order);
    // Explicit saved fixture preconditions, not evidence of world startup.
    // Native LastRenderTime is not inferred from an absent authored tag.
    for(const auto* name:{"Static0","A","B","C"})
        Property(state,name,"Engine.Actor.LastRenderTime",Value::Float(0.0f));
    return state;
}
std::int32_t Count(const std::string& actor,const std::string& property="Count") {
    const auto value=ReadPortableActorScriptProperty(Path(actor),property);Require(value.kind==Kind::Int,"Tick fixture count is not Int");return value.integer;
}
void RequirePass(const PortableLevelDynamicTickResult& result) {
    Require(result.execution.passed()&&result.execution.committed&&result.completed,"Production dynamic phase failed: "+result.execution.error);
}
void Synthetic() {
    Fixture fixture;const auto tables=Build(fixture);
    Require(InitializePortableRuntime(tables.packages).passed,"Generated Tick package initialization failed");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared,"Generated Level Tick native metadata/Package preparation failed");
    const auto loaded=LoadPortableRuntimeMap(tables.map);Require(loaded.passed,"Generated native Level load failed");
    Require(PreparePortableRuntimeObjectLookup().prepared,"Generated Level Tick has no real UObject lookup");
    fixture.legacy=fixture.Save("Legacy");
    Require(ReadPortableRuntimeLevelTickParity()==0,"Fresh native Level parity is not zero");
    const auto seed=[&](QuestVr::ScriptSavedState state){fixture.Restore(Envelope(fixture.legacy,state));};
    auto state=Seed(tables);
    // Native source marker opposite the current frame is an explicit saved
    // fixture input. The production API itself never receives a marker setter.
    for(const auto* name:{"A","B","C"})Property(state,name,"Engine.Actor.bTicked",Value::Bool(true));
    Property(state,"A","Engine.Actor.Owner",Value::Text(Kind::Object,Path("C")));
    Property(state,"B","Engine.Actor.Owner",Value::Text(Kind::Object,Path("C")));
    seed(state);const auto base=fixture.Save("Base");
    auto tick=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(tick);
    Require(tick.slotsVisited==3u&&tick.actorCalls==5u&&tick.deferredPushes==2u&&tick.pendingPasses==1u,
        "Live ascending/owner-deferred call counts differ from original ordering");
    Require(Count("C","Stamp")==1&&Count("B","Stamp")==2&&Count("A","Stamp")==3,"Owner-first LIFO order is not C,B,A");
    Require(Count("Static0")==0,"Dynamic phase ticked the static prefix");
    Require(tick.parityBefore==0&&tick.parityAfter==1&&ReadPortableRuntimeLevelTickParity()==1,"Frame did not toggle native parity");
    const auto savedParity=fixture.Save("Parity1");const auto decoded=Decode(savedParity);
    Require(decoded.nativeLevelTickParity&&decoded.nativeLevelTickParity->parity==1,"Parity1 checkpoint omitted native continuation");
    fixture.Restore(fixture.legacy);Require(ReadPortableRuntimeLevelTickParity()==0,"Legacy restore retained parity history");
    fixture.Restore(savedParity);Require(ReadPortableRuntimeLevelTickParity()==1,"Checkpoint did not restore parity1");
    tick=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(tick);
    Require(tick.parityBefore==1&&tick.parityAfter==0&&Count("A")==2&&Count("B")==2&&Count("C")==2,
        "Reloaded parity1 frame did not schedule all owners/children exactly once");
    // Captured batch A,B: A defers into the new LIVE head while B can tick.
    state=Seed(tables,{"B","A","C"});
    for(const auto* name:{"A","B","C"})Property(state,name,"Engine.Actor.bTicked",Value::Bool(true));
    Property(state,"A","Engine.Actor.Owner",Value::Text(Kind::Object,Path("B")));
    Property(state,"B","Engine.Actor.Owner",Value::Text(Kind::Object,Path("C")));
    seed(state);tick=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(tick);
    Require(tick.pendingPasses==2u&&tick.deferredPushes==3u&&Count("C","Stamp")==1&&Count("B","Stamp")==2&&Count("A","Stamp")==3,
        "Detached pending batch incorrectly flattened newly deferred nodes");
    // All-deferred owner cycles stop with return-sum zero; no recursive rescue.
    state=Seed(tables);for(const auto* name:{"A","B","C"})Property(state,name,"Engine.Actor.bTicked",Value::Bool(true));
    Property(state,"A","Engine.Actor.Owner",Value::Text(Kind::Object,Path("B")));
    Property(state,"B","Engine.Actor.Owner",Value::Text(Kind::Object,Path("A")));
    Property(state,"C","Engine.Actor.Owner",Value::Text(Kind::Object,Path("A")));
    seed(state);tick=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(tick);
    Require(tick.actorCalls==3u&&tick.deferredPushes==3u&&tick.pendingPasses==0u&&Count("A")==0&&Count("B")==0&&Count("C")==0,
        "All-deferred pass was forced or drained without progress");
    // Dynamic-only compaction keeps reserved/static prefix order intact.
    state=Seed(tables,{"A","","B","C"});seed(state);const auto beforeOrder=ReadPortableWorldActorOrder();
    tick=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(tick);const auto afterOrder=ReadPortableWorldActorOrder();
    Require(beforeOrder.actorIndices.size()==8u&&afterOrder.actorIndices.size()==7u&&afterOrder.firstDynamic==beforeOrder.firstDynamic,
        "Cleanup did not compact only dynamic null holes");
    Require(std::equal(beforeOrder.actorIndices.begin(),beforeOrder.actorIndices.begin()+4,afterOrder.actorIndices.begin()),
        "Cleanup changed reserved/static prefix identities");
    Require(tick.actorCalls==3u&&Count("A")==1&&Count("B")==1&&Count("C")==1,
        "Regular live loop incorrectly skipped source actors already marked with current parity");
    // Caller tick type1 is tested AFTER parity/animations, but suppresses the
    // normal selected Tick/state/timer/lifespan/physics suffix.
    state=Seed(tables);seed(state);tick=AdvancePortableLevelDynamicActors(0.01f,1);RequirePass(tick);
    Require(tick.actorCalls==3u&&ReadPortableRuntimeLevelTickParity()==1&&Count("A")==0&&Count("B")==0&&Count("C")==0,
        "Literal tick type1 skipped parity or ran the normal selected Tick suffix");
    // A native marker assignment is not an animation-property command.
    // Carry a real clock/its reflected mirror through an otherwise inactive
    // phase and compare all clock bytes, including transient tween history.
    state=Seed(tables);QuestVr::ActorAnimationClock clock;
    clock.pose.main.sequence="None";clock.pose.main.previous={12u,24u,0.375f};
    clock.simulationTime=12.5;clock.main.finishAnimWaiting=true;
    Object(state,"A").clock=clock;
    const auto main=[&](const char* name,Value value){Property(state,"A",std::string("Engine.Actor.")+name,value);};
    main("AnimSequence",Value::Text(Kind::Name,"None"));main("AnimFrame",Value::Float(0));
    for(const auto* name:{"AnimRate","AnimLast","AnimMinRate","TweenRate","OldAnimRate"})main(name,Value::Float(0));
    for(const auto* name:{"bAnimLoop","bAnimNotify","bAnimFinished"})main(name,Value::Bool(false));
    main("Fatness",Value::Byte(clock.pose.fatness));main("RemoteRole",Value::Byte(0));
    for(std::uint32_t slot=0;slot<4;++slot) {
        Property(state,"A","Engine.Actor.BlendAnimSequence",Value::Text(Kind::Name,""),slot);
        for(const auto* name:{"BlendAnimFrame","BlendAnimRate","BlendAnimLast","BlendAnimMinRate","BlendTweenRate","OldBlendAnimRate"})
            Property(state,"A",std::string("Engine.Actor.")+name,Value::Float(0),slot);
        Value simulated;simulated.kind=Kind::Struct;
        for(const auto* name:{"x","y","z","w"})simulated.fields.emplace(name,Value::Float(0));
        Property(state,"A","Engine.Actor.SimBlendAnim",simulated,slot);
    }
    seed(state);const auto clockBefore=Decode(fixture.Save("ClockBefore"));
    tick=AdvancePortableLevelDynamicActors(0.01f,1);RequirePass(tick);
    const auto clockAfter=Decode(fixture.Save("ClockAfter"));
    const auto clockOnly=[](const QuestVr::ScriptSavedState& saved) {
        const auto found=std::find_if(saved.objects.begin(),saved.objects.end(),[](const auto& object){return object.path==Path("A");});
        Require(found!=saved.objects.end()&&found->clock.has_value(),"Native marker lost its actual retained animation clock");
        QuestVr::ScriptSavedState result;result.mapName=saved.mapName;
        result.objects.push_back({found->path,found->classPath,{},found->clock,{},{}});
        return QuestVr::EncodeScriptSavedState(result);
    };
    Require(clockOnly(clockBefore)==clockOnly(clockAfter),"Native frame marker changed unrelated animation clock/tween state");
    // An unaudited selected native class is a reached failure, never a skip.
    state=Seed(tables);auto& slots=state.nativeActorOrder->slots;
    const auto badSlot=slots.at(2u);slots.erase(slots.begin()+2);slots.push_back(badSlot);state.nativeActorOrder->firstDynamic=3u;
    seed(state);const auto nativeBase=fixture.Save("NativeBoundaryBase");
    const auto nativeRevision=GetPortableRuntimeWorldRevision();tick=AdvancePortableLevelDynamicActors(0.01f,0);
    Require(!tick.execution.passed()&&!tick.execution.committed&&!tick.completed&&tick.actorCalls==3u&&
        tick.execution.error.find("unimplemented native virtual override")!=std::string::npos,
        "Unknown native class was skipped or executed as the base Actor implementation");
    Require(fixture.Save("NativeBoundaryAfter")==nativeBase&&GetPortableRuntimeWorldRevision()==nativeRevision&&
        ReadPortableRuntimeLevelTickParity()==0,"Unknown-native late boundary retained a partial frame");++refusals;
    // A late selected callback failure restores marker/context/queue/slots and
    // ordinary script writes in the SAME production root.
    state=Seed(tables);Property(state,"C","TickClasses.TestActor.Mode",Value::Integer(2));seed(state);
    const auto failedBase=fixture.Save("FailureBase");const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    tick=AdvancePortableLevelDynamicActors(0.01f,0);
    Require(!tick.execution.passed()&&!tick.execution.committed&&!tick.completed&&tick.actorCalls==3u,"Late Tick callback did not poison the frame");
    Require(ReadPortableRuntimeLevelTickParity()==0&&fixture.Save("AfterFailure")==failedBase&&GetPortableRuntimeWorldRevision()==revision,
        "Late failure retained frame/script/context effects");
    const auto afterGc=GC::GetStats();Require(afterGc.numObjects==gc.numObjects&&afterGc.memoryUsage==gc.memoryUsage,"Frame failure retained UObject allocations");++refusals;
    fixture.Restore(base);const auto successful=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(successful);
    Require(successful.execution.writes>0u&&successful.execution.nativeWork&&successful.execution.nativeWork->used>0u,"Frame lacks shared write/work accounting");
    fixture.Restore(base);
    const auto undo=[&](const QuestVr::Vm::Limits& limits,const std::string& label){
        const auto bytes=fixture.Save("BudgetBefore");const auto oldRevision=GetPortableRuntimeWorldRevision();
        const auto result=AdvancePortableLevelDynamicActors(0.01f,0,limits);
        Require(!result.execution.passed()&&!result.execution.committed&&!result.completed,"Frame ignored "+label);
        Require(ReadPortableRuntimeLevelTickParity()==0&&fixture.Save("BudgetAfter")==bytes&&GetPortableRuntimeWorldRevision()==oldRevision,
            "Frame budget failure retained "+label);++refusals;
    };
    for(std::size_t cut=0u;cut<successful.execution.writes;++cut){QuestVr::Vm::Limits limits;limits.writes=cut;undo(limits,"write prefix "+std::to_string(cut));}
    for(const auto cut:{std::size_t{0u},successful.execution.nativeWork->used/2u,successful.execution.nativeWork->used-1u}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=cut;undo(limits,"native-work prefix");
    }
    for(const auto elapsed:{-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        const auto result=AdvancePortableLevelDynamicActors(elapsed,0);
        Require(!result.execution.passed()&&!result.execution.committed&&!result.completed,"Invalid delta was admitted");++refusals;
    }
    fixture.Unchanged();
    std::cout<<"PASS generated production normal dynamic-actor phase: "<<checks<<" checks / "<<refusals
        <<" refusal controls; owner-first/LIFO/detached batches/cycles, inactive animation, selected Tick, parity save/reset,"
          " dynamic-only compaction and shared-root late/write/work rollback. NOT active Spawn/GC callbacks,"
          " active animation/physics, full paused/network/Pawn Level.Tick, original Training gameplay or headset acceptance.\n";
}
void TriggerControl() {
    for(const auto kind:{TriggerControlKind::Exact,TriggerControlKind::Parent}) {
        Fixture fixture;const auto tables=Build(fixture,false,2u,kind);
        const auto& engine=tables.packages.at(1u);
        const auto trigger=FindPortableExport(engine,"Trigger"),triggers=FindPortableExport(engine,"Triggers"),
            actor=FindPortableExport(engine,"Actor");
        Require(engine.exports.at(trigger).ObjClass==0&&engine.exports.at(triggers).ObjClass==0&&
            static_cast<std::uint32_t>(engine.exports.at(trigger).ObjFlags)==0x040f0004u&&
            static_cast<std::uint32_t>(engine.exports.at(triggers).ObjFlags)==0x040f0004u&&
            engine.exports.at(trigger).ObjBase==static_cast<std::int32_t>(triggers+1u)&&
            engine.exports.at(triggers).ObjBase==static_cast<std::int32_t>(actor+1u)&&
            LoadPortableClassDescriptor(engine,trigger).state.baseField==static_cast<std::int32_t>(triggers+1u)&&
            LoadPortableClassDescriptor(engine,triggers).state.baseField==static_cast<std::int32_t>(actor+1u),
            "Trigger fixture lost actual serialized native Trigger -> Triggers -> Actor ancestry");
        Require(InitializePortableRuntime(tables.packages).passed,"Trigger control package initialization failed");
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativePackages().prepared,"Trigger control native metadata/Package preparation failed");
        Require(LoadPortableRuntimeMap(tables.map).passed&&PreparePortableRuntimeObjectLookup().prepared,
            "Trigger control lacks native map/lookup ownership");
        Require(ReadPortableRuntimeDispatchClass("TickClasses.TestActor").parentPath=="Engine.Trigger"&&
            ReadPortableRuntimeDispatchClass("Engine.Trigger").parentPath=="Engine.Triggers"&&
            ReadPortableRuntimeDispatchClass("Engine.Triggers").parentPath=="Engine.Actor",
            "Production dispatch flattened genuine Trigger native ancestry");
        const auto legacy=fixture.Save("TriggerLegacy");
        const auto makeSeed=[&] {
            auto state=Seed(tables);
            for(const auto* name:{"A","B","C"}) {
                Property(state,name,"Engine.Actor.Physics",Value::Byte(0));
                Property(state,name,"Engine.Actor.bTicked",Value::Bool(true));
            }
            return state;
        };
        const auto seed=[&](const QuestVr::ScriptSavedState& state){fixture.Restore(Envelope(legacy,state));};
        const auto state=makeSeed();seed(state);
        const auto successful=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(successful);
        Require(successful.actorCalls==3u&&successful.slotsVisited==3u&&successful.movingBrushCalls==0u&&
            successful.parityBefore==0&&successful.parityAfter==1&&ReadPortableRuntimeLevelTickParity()==1&&
            Count("A")==1&&Count("B")==1&&Count("C")==1&&Count("Static0")==0&&
            Count("A","Stamp")==1&&Count("B","Stamp")==2&&Count("C","Stamp")==3&&Count("Level0","Ledger")==3,
            "Exact native Trigger did not execute the real selected script Tick in live order with Physics0");
        for(const auto* name:{"A","B","C"}) {
            const auto physics=ReadPortableActorScriptProperty(Path(name),"Physics");
            Require(physics.kind==Kind::Byte&&physics.integer==0,"Trigger admission invented active physics");
        }
        Require(successful.execution.writes>0u&&successful.execution.nativeWork&&successful.execution.nativeWork->used>0u,
            "Trigger phase lacks shared write/work accounting");
        const auto refusal=[&](const QuestVr::ScriptSavedState& input,const QuestVr::Vm::Limits& limits,
            const std::string& label,const std::string& reached={}) {
            seed(input);const auto before=fixture.Save("TriggerBefore");const auto revision=GetPortableRuntimeWorldRevision();
            const auto gc=GC::GetStats();const auto failed=AdvancePortableLevelDynamicActors(0.01f,0,limits);
            Require(!failed.execution.passed()&&!failed.execution.committed&&!failed.completed,"Trigger admitted "+label);
            if(!reached.empty())Require(failed.execution.error.find(reached)!=std::string::npos,
                "Trigger "+label+" failed before its genuine boundary: "+failed.execution.error);
            Require(fixture.Save("TriggerAfter")==before&&ReadPortableRuntimeLevelTickParity()==0&&
                GetPortableRuntimeWorldRevision()==revision,"Trigger failure retained a partial shared root: "+label);
            const auto afterGc=GC::GetStats();Require(afterGc.numObjects==gc.numObjects&&afterGc.memoryUsage==gc.memoryUsage,
                "Trigger failure retained UObject allocations: "+label);++refusals;
        };
        auto late=makeSeed();Property(late,"C","TickClasses.TestActor.Mode",Value::Integer(2));
        refusal(late,{},"late selected script Tick failure");
        // Exact admission must not cross a nearer unknown native class, even
        // when it derives from the newly audited Trigger. Its native parent
        // Triggers is separately left refused by this bounded increment.
        auto boundary=makeSeed();auto& slots=boundary.nativeActorOrder->slots;
        const auto bad=slots.at(2u);slots.erase(slots.begin()+2);slots.push_back(bad);boundary.nativeActorOrder->firstDynamic=3u;
        const auto classPath=kind==TriggerControlKind::Parent?"Engine.Triggers":"TickClasses.BadNative";
        boundary.objects.push_back({Path("Bad0"),classPath,{},{},{},{}});
        Property(boundary,"Bad0","Engine.Actor.LastRenderTime",Value::Float(0));
        Property(boundary,"Bad0","Engine.Actor.bStatic",Value::Bool(false));
        Property(boundary,"Bad0","Engine.Actor.Physics",Value::Byte(0));
        refusal(boundary,{},"unaudmitted nearest native class",std::string("unimplemented native virtual override: ")+classPath);
        if(kind==TriggerControlKind::Exact) {
            for(std::size_t cut=0u;cut<successful.execution.writes;++cut) {
                QuestVr::Vm::Limits limits;limits.writes=cut;refusal(state,limits,"write prefix "+std::to_string(cut));
            }
            for(const auto cut:{std::size_t{0u},successful.execution.nativeWork->used/2u,successful.execution.nativeWork->used-1u}) {
                QuestVr::Vm::Limits limits;limits.nativeWork=cut;refusal(state,limits,"native-work prefix");
            }
        }
        fixture.Unchanged();
    }
    std::cout<<"PASS exact Engine.Trigger native admission: "<<checks<<" cumulative checks / "<<refusals
        <<" refusals; actual native Trigger/Triggers ancestry, selected script Tick with Physics0, "
          "late/write/work shared-root undo, unknown-nearer-native and Engine.Triggers refusal. "
          "NOT active Trigger physics, teardown, original Training completion or headset acceptance.\n";
}
void DeusExLevelInfoControl() {
    for(const auto kind:{DeusExLevelInfoControlKind::Exact,DeusExLevelInfoControlKind::Parent}) {
        Fixture fixture;const auto tables=Build(fixture,false,2u,TriggerControlKind::None,kind);
        Require(tables.packages.size()==4u,"Cross-package LevelInfo fixture lost its separate DeusEx package");
        const auto& engine=tables.packages.at(1u);const auto& deusex=tables.packages.at(2u);
        const auto info=FindPortableExport(engine,"Info"),actor=FindPortableExport(engine,"Actor"),
            dxInfo=FindPortableExport(deusex,"DeusExLevelInfo");
        const auto& entry=deusex.exports.at(dxInfo);
        Require(entry.ObjClass==0&&entry.ObjOuter==0&&static_cast<std::uint32_t>(entry.ObjFlags)==0x040f0004u&&
            entry.ObjBase<0&&LoadPortableClassDescriptor(deusex,dxInfo).state.baseField==entry.ObjBase&&
            GetPortableObjectPath(deusex,entry.ObjBase)=="Engine.Info"&&
            engine.exports.at(info).ObjClass==0&&engine.exports.at(info).ObjOuter==0&&
            static_cast<std::uint32_t>(engine.exports.at(info).ObjFlags)==0x040f0004u&&
            engine.exports.at(info).ObjBase==static_cast<std::int32_t>(actor+1u)&&
            LoadPortableClassDescriptor(engine,info).state.baseField==static_cast<std::int32_t>(actor+1u),
            "LevelInfo fixture lost actual cross-package native DeusExLevelInfo -> Engine.Info -> Actor metadata");
        const auto& imported=deusex.imports.at(static_cast<std::size_t>(-entry.ObjBase-1));
        Require(deusex.names.at(static_cast<std::size_t>(imported.ClassPackage)).Name=="Core"&&
            deusex.names.at(static_cast<std::size_t>(imported.ClassName)).Name=="Class"&&
            imported.ObjOuter<0&&deusex.names.at(static_cast<std::size_t>(
                deusex.imports.at(static_cast<std::size_t>(-imported.ObjOuter-1)).ObjName)).Name=="Engine",
            "DeusExLevelInfo superclass is not an actual serialized Core.Class import owned by Engine");
        Require(InitializePortableRuntime(tables.packages).passed,"DeusExLevelInfo control package initialization failed");
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativePackages().prepared,"DeusExLevelInfo control native preparation failed");
        Require(LoadPortableRuntimeMap(tables.map).passed&&PreparePortableRuntimeObjectLookup().prepared,
            "DeusExLevelInfo control lacks native map/lookup ownership");
        Require(ReadPortableRuntimeDispatchClass("TickClasses.TestActor").parentPath=="DeusEx.DeusExLevelInfo"&&
            ReadPortableRuntimeDispatchClass("DeusEx.DeusExLevelInfo").parentPath=="Engine.Info"&&
            ReadPortableRuntimeDispatchClass("Engine.Info").parentPath=="Engine.Actor",
            "Production dispatch flattened or replaced genuine cross-package DeusExLevelInfo ancestry");
        const auto legacy=fixture.Save("DeusExLevelInfoLegacy");
        const auto makeSeed=[&] {
            auto state=Seed(tables);for(const auto* name:{"A","B","C"}) {
                Property(state,name,"Engine.Actor.Physics",Value::Byte(0));
                Property(state,name,"Engine.Actor.bTicked",Value::Bool(true));
            }return state;
        };
        const auto seed=[&](const QuestVr::ScriptSavedState& state){fixture.Restore(Envelope(legacy,state));};
        const auto state=makeSeed();seed(state);
        const auto successful=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(successful);
        Require(successful.actorCalls==3u&&successful.slotsVisited==3u&&successful.movingBrushCalls==0u&&
            successful.parityBefore==0&&successful.parityAfter==1&&ReadPortableRuntimeLevelTickParity()==1&&
            Count("A")==1&&Count("B")==1&&Count("C")==1&&Count("Static0")==0&&
            Count("A","Stamp")==1&&Count("B","Stamp")==2&&Count("C","Stamp")==3&&Count("Level0","Ledger")==3,
            "Exact native DeusExLevelInfo did not run the real selected script Tick with Physics0 in live order");
        for(const auto* name:{"A","B","C"}) {
            const auto physics=ReadPortableActorScriptProperty(Path(name),"Physics");
            Require(physics.kind==Kind::Byte&&physics.integer==0,"DeusExLevelInfo admission invented active physics");
        }
        Require(successful.execution.writes>0u&&successful.execution.nativeWork&&successful.execution.nativeWork->used>0u,
            "DeusExLevelInfo phase lacks shared write/work accounting");
        const auto refusal=[&](const QuestVr::ScriptSavedState& input,const QuestVr::Vm::Limits& limits,
            const std::string& label,const std::string& reached={}) {
            seed(input);const auto before=fixture.Save("DeusExLevelInfoBefore");const auto revision=GetPortableRuntimeWorldRevision();
            const auto gc=GC::GetStats();const auto failed=AdvancePortableLevelDynamicActors(0.01f,0,limits);
            Require(!failed.execution.passed()&&!failed.execution.committed&&!failed.completed,"DeusExLevelInfo admitted "+label);
            if(!reached.empty())Require(failed.execution.error.find(reached)!=std::string::npos,
                "DeusExLevelInfo "+label+" failed before its genuine boundary: "+failed.execution.error);
            Require(fixture.Save("DeusExLevelInfoAfter")==before&&ReadPortableRuntimeLevelTickParity()==0&&
                GetPortableRuntimeWorldRevision()==revision,"DeusExLevelInfo failure retained a partial shared root: "+label);
            const auto afterGc=GC::GetStats();Require(afterGc.numObjects==gc.numObjects&&afterGc.memoryUsage==gc.memoryUsage,
                "DeusExLevelInfo failure retained UObject allocations: "+label);++refusals;
        };
        auto late=makeSeed();Property(late,"C","TickClasses.TestActor.Mode",Value::Integer(2));
        refusal(late,{},"late selected script Tick failure");
        auto boundary=makeSeed();auto& slots=boundary.nativeActorOrder->slots;const auto bad=slots.at(2u);
        slots.erase(slots.begin()+2);slots.push_back(bad);boundary.nativeActorOrder->firstDynamic=3u;
        const auto classPath=kind==DeusExLevelInfoControlKind::Parent?"Engine.Info":"TickClasses.BadNative";
        boundary.objects.push_back({Path("Bad0"),classPath,{},{},{},{}});
        Property(boundary,"Bad0","Engine.Actor.LastRenderTime",Value::Float(0));
        Property(boundary,"Bad0","Engine.Actor.bStatic",Value::Bool(false));
        Property(boundary,"Bad0","Engine.Actor.Physics",Value::Byte(0));
        refusal(boundary,{},"unadmitted nearest native class",std::string("unimplemented native virtual override: ")+classPath);
        if(kind==DeusExLevelInfoControlKind::Exact) {
            for(std::size_t cut=0u;cut<successful.execution.writes;++cut) {
                QuestVr::Vm::Limits limits;limits.writes=cut;refusal(state,limits,"write prefix "+std::to_string(cut));
            }
            for(const auto cut:{std::size_t{0u},successful.execution.nativeWork->used/2u,successful.execution.nativeWork->used-1u}) {
                QuestVr::Vm::Limits limits;limits.nativeWork=cut;refusal(state,limits,"native-work prefix");
            }
        }
        fixture.Unchanged();
    }
    std::cout<<"PASS exact DeusEx.DeusExLevelInfo native admission: "<<checks<<" cumulative checks / "<<refusals
        <<" refusals; authored cross-package native Info ancestry, selected script Tick/Physics0, "
          "late/write/work whole-root undo, unknown-nearer-native and Engine.Info refusal. "
          "NOT blanket Info/LevelInfo, active physics, teardown, original Training completion or headset acceptance.\n";
}
void LightControl() {
    Fixture fixture;const auto tables=Build(fixture,false,2u,TriggerControlKind::None,DeusExLevelInfoControlKind::None,true);
    const auto& engine=tables.packages.at(1u);
    const auto light=FindPortableExport(engine,"Light"),actor=FindPortableExport(engine,"Actor");
    Require(engine.exports.at(light).ObjClass==0&&engine.exports.at(light).ObjOuter==0&&
        static_cast<std::uint32_t>(engine.exports.at(light).ObjFlags)==0x040f0004u&&
        engine.exports.at(light).ObjBase==static_cast<std::int32_t>(actor+1u)&&
        LoadPortableClassDescriptor(engine,light).state.baseField==static_cast<std::int32_t>(actor+1u),
        "Light fixture lost actual serialized native Light -> Actor Class/RF_Native ancestry");
    Require(InitializePortableRuntime(tables.packages).passed,"Light control package initialization failed");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared,"Light control native metadata preparation failed");
    Require(LoadPortableRuntimeMap(tables.map).passed&&PreparePortableRuntimeObjectLookup().prepared,
        "Light control lacks native map/lookup ownership");
    Require(ReadPortableRuntimeDispatchClass("TickClasses.TestActor").parentPath=="Engine.Light"&&
        ReadPortableRuntimeDispatchClass("Engine.Light").parentPath=="Engine.Actor",
        "Production dispatch flattened or replaced genuine Light ancestry");
    const auto legacy=fixture.Save("LightLegacy");
    const auto makeSeed=[&] {
        auto state=Seed(tables);for(const auto* name:{"A","B","C"}) {
            Property(state,name,"Engine.Actor.Physics",Value::Byte(0));
            Property(state,name,"Engine.Actor.bTicked",Value::Bool(true));
        }return state;
    };
    const auto seed=[&](const QuestVr::ScriptSavedState& state){fixture.Restore(Envelope(legacy,state));};
    const auto state=makeSeed();seed(state);
    const auto successful=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(successful);
    Require(successful.actorCalls==3u&&successful.slotsVisited==3u&&successful.movingBrushCalls==0u&&
        successful.parityBefore==0&&successful.parityAfter==1&&ReadPortableRuntimeLevelTickParity()==1&&
        Count("A")==1&&Count("B")==1&&Count("C")==1&&Count("Static0")==0&&
        Count("A","Stamp")==1&&Count("B","Stamp")==2&&Count("C","Stamp")==3&&Count("Level0","Ledger")==3,
        "Exact native Light did not execute real selected script Tick/Physics0 in live order");
    for(const auto* name:{"A","B","C"}) {
        const auto physics=ReadPortableActorScriptProperty(Path(name),"Physics");
        Require(physics.kind==Kind::Byte&&physics.integer==0,"Light admission invented active physics");
    }
    Require(successful.execution.writes>0u&&successful.execution.nativeWork&&successful.execution.nativeWork->used>0u,
        "Light phase lacks shared write/work accounting");
    const auto refusal=[&](const QuestVr::ScriptSavedState& input,const QuestVr::Vm::Limits& limits,
        const std::string& label,const std::string& reached={}) {
        seed(input);const auto before=fixture.Save("LightBefore");const auto revision=GetPortableRuntimeWorldRevision();
        const auto gc=GC::GetStats();const auto failed=AdvancePortableLevelDynamicActors(0.01f,0,limits);
        Require(!failed.execution.passed()&&!failed.execution.committed&&!failed.completed,"Light admitted "+label);
        if(!reached.empty())Require(failed.execution.error.find(reached)!=std::string::npos,
            "Light "+label+" failed before its genuine boundary: "+failed.execution.error);
        Require(fixture.Save("LightAfter")==before&&ReadPortableRuntimeLevelTickParity()==0&&
            GetPortableRuntimeWorldRevision()==revision,"Light failure retained a partial shared root: "+label);
        const auto afterGc=GC::GetStats();Require(afterGc.numObjects==gc.numObjects&&afterGc.memoryUsage==gc.memoryUsage,
            "Light failure retained UObject allocations: "+label);++refusals;
    };
    auto late=makeSeed();Property(late,"C","TickClasses.TestActor.Mode",Value::Integer(2));
    refusal(late,{},"late selected script Tick failure");
    // An unknown native Light-derived child is still the nearest native
    // boundary. The newly proven Light ancestor must not bypass its refusal.
    auto boundary=makeSeed();auto& slots=boundary.nativeActorOrder->slots;const auto bad=slots.at(2u);
    slots.erase(slots.begin()+2);slots.push_back(bad);boundary.nativeActorOrder->firstDynamic=3u;
    boundary.objects.push_back({Path("Bad0"),"TickClasses.BadNative",{},{},{},{}});
    Property(boundary,"Bad0","Engine.Actor.LastRenderTime",Value::Float(0));
    Property(boundary,"Bad0","Engine.Actor.bStatic",Value::Bool(false));
    Property(boundary,"Bad0","Engine.Actor.Physics",Value::Byte(0));
    refusal(boundary,{},"unadmitted nearer native Light-derived class","unimplemented native virtual override: TickClasses.BadNative");
    for(std::size_t cut=0u;cut<successful.execution.writes;++cut) {
        QuestVr::Vm::Limits limits;limits.writes=cut;refusal(state,limits,"write prefix "+std::to_string(cut));
    }
    for(const auto cut:{std::size_t{0u},successful.execution.nativeWork->used/2u,successful.execution.nativeWork->used-1u}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=cut;refusal(state,limits,"native-work prefix");
    }
    fixture.Unchanged();
    std::cout<<"PASS exact Engine.Light native admission: "<<checks<<" cumulative checks / "<<refusals
        <<" refusals; authored native Light ancestry, selected script Tick/Physics0, late/write/work whole-root undo "
          "and unknown-nearer-native refusal. NOT lighting rendering, brightness, teardown, active physics, "
          "original Training completion or headset acceptance.\n";
}
void ScalarLightControl() {
    Fixture fixture;
    const auto tables=Build(fixture,false,2u,TriggerControlKind::None,DeusExLevelInfoControlKind::None,true,true);
    Require(InitializePortableRuntime(tables.packages).passed,"Scalar-Light package initialization failed");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared,"Scalar-Light native metadata preparation failed");
    Require(LoadPortableRuntimeMap(tables.map).passed&&PreparePortableRuntimeObjectLookup().prepared,
        "Scalar-Light fixture lacks the genuine native Level and lookup");
    Require(ReadPortableRuntimeDispatchClass("TickClasses.TestActor").parentPath=="Engine.Light"&&
        ReadPortableRuntimeDispatchClass("Engine.Light").parentPath=="Engine.Actor",
        "Scalar-Light fixture replaced its genuine selected Light ancestry");
    const auto legacy=fixture.Save("ScalarLightLegacy");
    const auto bits=[](float number){std::uint32_t word{};std::memcpy(&word,&number,4u);return word;};
    const auto fromBits=[](std::uint32_t word){float number{};std::memcpy(&number,&word,4u);return number;};
    const auto scalar=[&](const std::string& actor,const std::string& field) {
        const auto value=ReadPortableActorScriptProperty(Path(actor),field);
        Require(value.kind==Kind::Float,"Scalar-Light field lost its declared Float kind: "+field);
        return value.floating;
    };
    const auto makeSeed=[&](std::int32_t mode=0,float direction=-1.0f) {
        auto state=Seed(tables);
        for(const auto* name:{"A","B","C"}) {
            Property(state,name,"Engine.Actor.Physics",Value::Byte(0));
            Property(state,name,"Engine.Actor.bTicked",Value::Bool(true));
            Property(state,name,"TickClasses.TestActor.Direction",Value::Float(direction));
            Property(state,name,"TickClasses.TestActor.Mode",Value::Integer(mode));
        }
        // Alpha and ChangeTime deliberately retain their real omitted scalar
        // Float0 defaults, as on the reached original Training lights.
        return state;
    };
    const auto seed=[&](const QuestVr::ScriptSavedState& state){fixture.Restore(Envelope(legacy,state));};
    const auto verify=[&](std::uint32_t observed,std::uint32_t alpha) {
        for(const auto* name:{"A","B","C"}) {
            Require(bits(scalar(name,"ObservedAlpha"))==observed,
                "Scalar-Light did not copy its genuine stored intermediate infinity");
            Require(bits(scalar(name,"Alpha"))==alpha,"Scalar-Light original recovery branch changed the result");
            Require(bits(scalar(name,"ChangeTime"))==0u,"Scalar-Light replaced its authored zero transition time");
            Require(Count(name)==1,"Scalar-Light skipped its selected original script Tick");
            const auto physics=ReadPortableActorScriptProperty(Path(name),"Physics");
            Require(physics.kind==Kind::Byte&&physics.integer==0,"Scalar-Light invented active physics");
        }
        Require(Count("A","Stamp")==1&&Count("B","Stamp")==2&&Count("C","Stamp")==3&&
            Count("Level0","Ledger")==3&&Count("Static0")==0,
            "Scalar-Light changed native live order or ticked its static prefix");
    };
    PortableLevelDynamicTickResult successful;
    for(const auto direction:{-1.0f,1.0f}) {
        seed(makeSeed(0,direction));
        Require(bits(scalar("A","Alpha"))==0u&&bits(scalar("A","ChangeTime"))==0u,
            "Scalar-Light seed invented nonzero Alpha/ChangeTime defaults");
        successful=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(successful);
        Require(successful.actorCalls==3u&&successful.slotsVisited==3u&&successful.movingBrushCalls==0u&&
            successful.parityBefore==0&&successful.parityAfter==1&&ReadPortableRuntimeLevelTickParity()==1,
            "Scalar-Light failed the actual shared native actor phase/parity");
        verify(direction<0.0f?0xff800000u:0x7f800000u,direction<0.0f?0u:0x3f800000u);
        const auto saved=fixture.Save("ScalarLightRecovered");std::size_t versionOffset=4u;
        Require(Word(saved,versionOffset)==19u,"Retained intermediate infinity did not select runtime envelope19");
        const auto decoded=Decode(saved);const auto blob=QuestVr::EncodeScriptSavedState(decoded);
        Require(blob.at(6u)==14u&&decoded.nativeLevelTickParity&&decoded.nativeLevelTickParity->parity==1,
            "Scalar-Light checkpoint omitted raw-scalar codec14 or native parity continuation");
        fixture.Restore(legacy);Require(ReadPortableRuntimeLevelTickParity()==0,"Scalar-Light legacy reset retained frame parity");
        fixture.Restore(saved);Require(ReadPortableRuntimeLevelTickParity()==1,"Scalar-Light checkpoint lost frame parity");
        verify(direction<0.0f?0xff800000u:0x7f800000u,direction<0.0f?0u:0x3f800000u);
        Require(fixture.Save("ScalarLightRecoveredAgain")==saved,"Scalar-Light raw-scalar checkpoint is not byte-deterministic");
    }
    // A separate selected mode leaves the actual compound-assignment result
    // in Alpha. The host must retain it, not secretly normalize before save.
    seed(makeSeed(1));const auto retained=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(retained);
    verify(0xff800000u,0xff800000u);const auto retainedSave=fixture.Save("ScalarLightRetainedInfinity");
    fixture.Restore(legacy);fixture.Restore(retainedSave);verify(0xff800000u,0xff800000u);
    Require(fixture.Save("ScalarLightRetainedInfinityAgain")==retainedSave,
        "Scalar-Light retained infinity failed exact production save/reload");
    // These are raw Float-property checkpoint copies, not arithmetic/FloatConst
    // claims. Both quiet and signaling NaN payloads must survive unchanged.
    for(const auto word:{0x7f800000u,0xff800000u,0x7fc12345u,0xffc54321u,0x7f812345u,0xff854321u}) {
        auto state=makeSeed();Property(state,"A","TickClasses.TestActor.ObservedAlpha",Value::Float(fromBits(word)));
        seed(state);Require(bits(scalar("A","ObservedAlpha"))==word,"Raw scalar restore changed IEEE payload bits");
        const auto saved=fixture.Save("ScalarLightRawBits");fixture.Restore(legacy);fixture.Restore(saved);
        Require(bits(scalar("A","ObservedAlpha"))==word&&fixture.Save("ScalarLightRawBitsAgain")==saved,
            "Production raw scalar save/reload changed NaN sign/payload or infinity");
    }
    const auto refusal=[&](const QuestVr::ScriptSavedState& state,const QuestVr::Vm::Limits& limits,
        const std::string& label,bool late=false) {
        seed(state);const auto before=fixture.Save("ScalarLightBefore");const auto revision=GetPortableRuntimeWorldRevision();
        const auto gc=GC::GetStats();const auto result=AdvancePortableLevelDynamicActors(0.01f,0,limits);
        Require(!result.execution.passed()&&!result.execution.committed&&!result.completed,
            "Scalar-Light admitted "+label);
        if(late)Require(result.actorCalls==3u&&result.execution.function=="TickClasses.TestActor.Tick",
            "Scalar-Light late selected failure happened before its genuine callback");
        Require(fixture.Save("ScalarLightAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&
            ReadPortableRuntimeLevelTickParity()==0,"Scalar-Light failure retained a partial native/script root: "+label);
        const auto afterGc=GC::GetStats();Require(afterGc.numObjects==gc.numObjects&&afterGc.memoryUsage==gc.memoryUsage,
            "Scalar-Light failure retained UObject allocations: "+label);++refusals;
    };
    auto late=makeSeed();Property(late,"C","TickClasses.TestActor.Mode",Value::Integer(2));
    refusal(late,{},"late selected failure after transient infinity/recovery",true);
    seed(makeSeed());successful=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(successful);
    Require(successful.execution.writes>0u&&successful.execution.nativeWork&&successful.execution.nativeWork->used>0u,
        "Scalar-Light phase lacks shared write/work accounting");
    for(std::size_t cut=0u;cut<successful.execution.writes;++cut) {
        QuestVr::Vm::Limits limits;limits.writes=cut;
        refusal(makeSeed(),limits,"write prefix "+std::to_string(cut));
    }
    for(const auto cut:{std::size_t{0u},successful.execution.nativeWork->used/2u,successful.execution.nativeWork->used-1u}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=cut;refusal(makeSeed(),limits,"native-work prefix");
    }
    // A genuine codec14 payload cannot be relabeled with old envelope18, and
    // a finite codec13 payload cannot acquire raw-scalar authority from19.
    auto finiteSeed=makeSeed();
    // Capture intentionally omits parity0 (the exact legacy reset baseline).
    // Use real retained frame history1 so this MUST remain finite codec13.
    finiteSeed.nativeLevelTickParity=QuestVr::ScriptSavedLevelTickParity{Path("MyLevel"),1};
    seed(finiteSeed);
    for(const auto* name:{"A","B","C"})
        Require(bits(scalar(name,"Alpha"))==0u&&bits(scalar(name,"ObservedAlpha"))==0u,
            "Finite Scalar-Light restore retained a previous nonfinite property overlay");
    const auto finiteSave=fixture.Save("ScalarLightFiniteEnvelope");
    const auto finiteCodec=QuestVr::EncodeScriptSavedState(Decode(finiteSave)).at(6u);
    Require(finiteCodec==13u,"Finite Scalar-Light boundary selected codec"+std::to_string(finiteCodec)+" instead of13");
    const auto rejectEnvelope=[&](Bytes invalid,const std::string& label) {
        const auto path=fixture.directory/(label+".sav");WriteBytes(path,invalid);
        const auto before=fixture.Save("ScalarLightEnvelopeBefore");const auto revision=GetPortableRuntimeWorldRevision();
        const auto parity=ReadPortableRuntimeLevelTickParity();const auto gc=GC::GetStats();
        const auto unchanged=[&] {
            Require(fixture.Save("ScalarLightEnvelopeAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&
                ReadPortableRuntimeLevelTickParity()==parity,"Scalar-Light envelope rejection changed native/script state: "+label);
            const auto afterGc=GC::GetStats();Require(afterGc.numObjects==gc.numObjects&&afterGc.memoryUsage==gc.memoryUsage,
                "Scalar-Light envelope rejection retained UObject allocations: "+label);
        };
        Require(!ValidatePortableRuntimeState(path.string()),"Scalar-Light preflight admitted "+label);unchanged();
        Require(!LoadPortableRuntimeState(path.string()),"Scalar-Light restore admitted "+label);unchanged();++refusals;
    };
    auto oldEnvelope=retainedSave;Replace32(oldEnvelope,4u,18u);
    rejectEnvelope(std::move(oldEnvelope),"Codec14WithEnvelope18");
    auto falseAuthority=finiteSave;Replace32(falseAuthority,4u,19u);
    rejectEnvelope(std::move(falseAuthority),"Codec13WithEnvelope19");
    // Exercise the real outer equipment wrapper around this genuine captured
    // envelope19. Its functional starter ring is port equipment state, not an
    // invented original Pawn/ring UObject or native Mover service.
    QuestVr::QuickBeltModel belt;belt.Reconcile({{QuestVr::kQuickBeltKeyRingIdentity,true,true}});
    const auto beltBytes=QuestVr::EncodeQuickBeltState(belt.State());
    Bytes wrapped;U32(wrapped,0x53515844u);U32(wrapped,15u);U32(wrapped,static_cast<std::uint32_t>(beltBytes.size()));
    Append(wrapped,beltBytes);U32(wrapped,static_cast<std::uint32_t>(retainedSave.size()));Append(wrapped,retainedSave);
    fixture.Restore(wrapped);verify(0xff800000u,0xff800000u);
    const auto wrappedAgain=fixture.Save("ScalarLightEquipmentWrapper");
    Require(wrappedAgain==wrapped,"QuickBelt wrapper15 failed exact capture of genuine scalar envelope19");
    std::size_t wrapperOffset=4u;Require(Word(wrappedAgain,wrapperOffset)==15u,"Scalar wrapper capture lost equipment envelope15");
    const auto beltLength=Word(wrappedAgain,wrapperOffset);wrapperOffset+=beltLength;
    const auto innerLength=Word(wrappedAgain,wrapperOffset);
    Require(innerLength==wrappedAgain.size()-wrapperOffset&&
        Bytes(wrappedAgain.begin()+static_cast<std::ptrdiff_t>(wrapperOffset),wrappedAgain.end())==retainedSave,
        "Equipment wrapper changed its exact scalar envelope19 payload");
    fixture.Restore(legacy);fixture.Restore(wrappedAgain);verify(0xff800000u,0xff800000u);
    Require(fixture.Save("ScalarLightEquipmentWrapperAgain")==wrappedAgain,
        "Repeated equipment/scalar checkpoint restore is not byte-deterministic");
    fixture.Restore(legacy); // Leave this distinct native-wrapper fixture reset.
    fixture.Unchanged();
    std::cout<<"PASS generated production scalar-Light Tick: "<<checks<<" cumulative checks / "<<refusals
        <<" refusals; actual native184/172/171 stored infinities and script recovery, unchanged ChangeTime0, "
          "retained scalar/NaN payload checkpoint19/codec14, exact envelope-pair rejection, equipment wrapper15, "
          "native parity/order and full late/write/work undo. NOT native Mover wrapper, TriggerLight BeginPlay/Disable, "
          "original-map completion, live lighting or headset acceptance.\n";
}
void MoverControl() {
    Fixture fixture;const auto tables=Build(fixture,true);
    Require(InitializePortableRuntime(tables.packages).passed,"Mover control package initialization failed");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared,"Mover control native metadata/Package preparation failed");
    Require(LoadPortableRuntimeMap(tables.map).passed&&PreparePortableRuntimeObjectLookup().prepared,
        "Mover control lacks native map/lookup ownership");
    const auto legacy=fixture.Save("MoverLegacy");auto state=Seed(tables);
    auto& slots=state.nativeActorOrder->slots;const auto mover=slots.at(2u);
    slots.erase(slots.begin()+2);slots.push_back(mover);state.nativeActorOrder->firstDynamic=3u;
    state.objects.push_back({Path("Bad0"),"Engine.Mover",{
        {"Engine.Actor.LastRenderTime","LastRenderTime",0u,Value::Float(0.0f)}},{},{},{}});
    fixture.Restore(Envelope(legacy,state));
    auto tick=AdvancePortableLevelDynamicActors(0.01f,1);RequirePass(tick);
    Require(tick.actorCalls==4u&&tick.slotsVisited==4u&&tick.parityAfter==1&&Count("Static0")==0,
        "Audited native Mover was skipped or changed the reserved/static prefix");
    tick=AdvancePortableLevelDynamicActors(0.01f,0);RequirePass(tick);
    Require(tick.actorCalls==4u&&tick.parityBefore==1&&tick.parityAfter==0&&Count("A")==1&&Count("B")==1&&Count("C")==1,
        "Audited native Mover did not join the ordinary same-root Actor suffix");
    fixture.Unchanged();
    std::cout<<"PASS exact Engine.Mover native Tick/physics admission: "<<checks<<" cumulative checks; "
        "generated inactive Physics0 only, NOT moving-brush interpolation or original-map acceptance.\n";
}
QuestVr::ScriptSavedState MovingBrushSeed(const Tables& tables) {
    auto state=Seed(tables);
    auto& slots=state.nativeActorOrder->slots;const auto mover=slots.at(2u);
    slots.erase(slots.begin()+2);state.nativeActorOrder->firstDynamic=3u;
    // Run the mover before the script actors so a later callback can test undo
    // of the exact native OldLocation/Velocity words in the same frame root.
    slots.insert(slots.begin()+3,mover);
    state.objects.push_back({Path("Bad0"),"Engine.Mover",{},{},{},{}});
    Property(state,"Bad0","Engine.Actor.LastRenderTime",Value::Float(0));
    Property(state,"Bad0","Engine.Actor.bStatic",Value::Bool(false));
    Property(state,"Bad0","Engine.Actor.Physics",Value::Byte(9));
    Property(state,"Bad0","Engine.Actor.Location",Value::Vector({-0.0f,123.75f,-98.125f}));
    Property(state,"Bad0","Engine.Actor.OldLocation",Value::Vector({19,21,23}));
    Property(state,"Bad0","Engine.Actor.Velocity",Value::Vector({31,37,41}));
    Property(state,"Bad0","Engine.Actor.RotationRate",Value::Rotator({0,0,0}));
    Property(state,"Bad0","Engine.Actor.bInterpolating",Value::Bool(false));
    Property(state,"Bad0","Engine.Actor.PhysAlpha",Value::Float(0));
    Property(state,"Bad0","Engine.Actor.PhysRate",Value::Float(1));
    Property(state,"Bad0","Engine.Mover.KeyNum",Value::Byte(255));
    Property(state,"Bad0","Engine.Mover.MoverGlideType",Value::Byte(0));
    for(const auto* name:{"BasePos","OldPos"})Property(state,"Bad0",std::string("Engine.Mover.")+name,Value::Vector({0,0,0}));
    for(const auto* name:{"BaseRot","OldRot"})Property(state,"Bad0",std::string("Engine.Mover.")+name,Value::Rotator({0,0,0}));
    for(std::uint32_t key=0u;key<8u;++key) {
        Property(state,"Bad0","Engine.Mover.KeyPos",Value::Vector({10,20,30}),key);
        Property(state,"Bad0","Engine.Mover.KeyRot",Value::Rotator({0,0,0}),key);
    }
    return state;
}
void RequireVectorWords(const std::string& actor,const std::string& property,const std::array<float,3>& expected) {
    const auto value=ReadPortableActorScriptProperty(Path(actor),property);
    Require(value.kind==Kind::Vector&&std::memcmp(value.vector.data(),expected.data(),sizeof(expected))==0,
        "Moving-brush "+actor+'.'+property+" did not retain the exact source float words");
}
void MovingBrushPhysics() {
    Fixture fixture;const auto tables=Build(fixture,true);
    Require(InitializePortableRuntime(tables.packages).passed,"Moving-brush fixture package initialization failed");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared,"Moving-brush native metadata/Package preparation failed");
    Require(LoadPortableRuntimeMap(tables.map).passed&&PreparePortableRuntimeObjectLookup().prepared,
        "Moving-brush fixture lacks native map/lookup ownership");
    const auto legacy=fixture.Save("MovingBrushLegacy");
    const auto seed=[&](const QuestVr::ScriptSavedState& state){fixture.Restore(Envelope(legacy,state));};
    auto state=MovingBrushSeed(tables);seed(state);
    const auto successful=AdvancePortableLevelDynamicActors(0.125f,0);RequirePass(successful);
    Require(successful.actorCalls==4u&&Count("A")==1&&Count("B")==1&&Count("C")==1,
        "Physics9 idle mover did not join the genuine ordinary actor phase");
    RequireVectorWords("Bad0","Location",{-0.0f,123.75f,-98.125f});
    RequireVectorWords("Bad0","OldLocation",{-0.0f,123.75f,-98.125f});
    RequireVectorWords("Bad0","Velocity",{0,0,0});
    const auto key=ReadPortableActorScriptProperty(Path("Bad0"),"KeyNum");
    Require(key.kind==Kind::Byte&&key.integer==255,"Native local KeyNum clamp incorrectly rewrote its source field");
    Require(successful.execution.writes>=6u&&successful.execution.nativeWork&&successful.execution.nativeWork->used>0u,
        "Physics9 outer stores lack shared write/work accounting");
    const auto refusal=[&](const QuestVr::ScriptSavedState& input,float elapsed,const QuestVr::Vm::Limits& limits,
        const std::string& label,const std::string& reached={}) {
        seed(input);const auto before=fixture.Save("MovingBrushBefore");const auto revision=GetPortableRuntimeWorldRevision();
        const auto gc=GC::GetStats();const auto tick=AdvancePortableLevelDynamicActors(elapsed,0,limits);
        Require(!tick.execution.passed()&&!tick.execution.committed&&!tick.completed,"Physics9 admitted "+label);
        if(!reached.empty())Require(tick.execution.error.find(reached)!=std::string::npos,
            "Physics9 "+label+" failed before its genuine service boundary: "+tick.execution.error);
        Require(ReadPortableRuntimeLevelTickParity()==0&&fixture.Save("MovingBrushAfter")==before&&
            GetPortableRuntimeWorldRevision()==revision,"Physics9 failure retained frame/source effects: "+label);
        const auto afterGc=GC::GetStats();Require(afterGc.numObjects==gc.numObjects&&afterGc.memoryUsage==gc.memoryUsage,
            "Physics9 failure retained UObject allocations: "+label);++refusals;
    };
    // The inner Mover cast may return, but the Actor outer envelope must still
    // copy OldLocation and derive Velocity for a non-Mover with Physics9.
    state=MovingBrushSeed(tables);Property(state,"Bad0","Engine.Actor.Physics",Value::Byte(0));
    Property(state,"A","Engine.Actor.Physics",Value::Byte(9));
    Property(state,"A","Engine.Actor.Location",Value::Vector({17,-0.0f,-29}));
    Property(state,"A","Engine.Actor.OldLocation",Value::Vector({1,2,3}));
    Property(state,"A","Engine.Actor.Velocity",Value::Vector({5,7,11}));
    Property(state,"A","Engine.Actor.bInterpolating",Value::Bool(true));
    seed(state);RequirePass(AdvancePortableLevelDynamicActors(0.125f,0));
    RequireVectorWords("A","OldLocation",{17,-0.0f,-29});RequireVectorWords("A","Velocity",{0,0,0});
    // Source outer division still executes for an idle inner return. Zero
    // delta cannot quietly become a successful zero-velocity replacement.
    state=MovingBrushSeed(tables);refusal(state,0.0f,{},"zero-delta outer arithmetic");
    state=MovingBrushSeed(tables);Property(state,"C","TickClasses.TestActor.Mode",Value::Integer(2));
    refusal(state,0.125f,{},"late selected Tick exception");
    state=MovingBrushSeed(tables);
    for(std::size_t cut=0u;cut<successful.execution.writes;++cut) {
        QuestVr::Vm::Limits limits;limits.writes=cut;
        refusal(state,0.125f,limits,"write prefix "+std::to_string(cut));
    }
    for(const auto cut:{std::size_t{0u},successful.execution.nativeWork->used/2u,successful.execution.nativeWork->used-1u}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=cut;refusal(state,0.125f,limits,"native-work prefix");
    }
    // This generated Level deliberately does not install an Engine/Cylinder
    // collision authority. Both real interpolation modes must reach actual
    // MoveActor and refuse there, atomically, not claim a stubbed move success.
    for(const auto glide:{std::uint8_t{0u},std::uint8_t{1u}})
    for(const auto key:{std::uint8_t{0u},std::uint8_t{8u},std::uint8_t{255u}}) {
        state=MovingBrushSeed(tables);Property(state,"Bad0","Engine.Actor.bInterpolating",Value::Bool(true));
        Property(state,"Bad0","Engine.Mover.KeyNum",Value::Byte(key));
        Property(state,"Bad0","Engine.Mover.MoverGlideType",Value::Byte(glide));
        refusal(state,0.25f,{},glide?"active glide collision boundary":"active linear collision boundary",
            "Retail collision requires actual current Level/Engine/Cylinder preparation");
    }
    fixture.Unchanged();
    std::cout<<"PASS generated production Physics9 outer/idle/cast controls: "<<checks<<" cumulative checks / "<<refusals
        <<" refusals; raw OldLocation, derived idle Velocity, native slot loading and same-root late/write/work undo. "
          "Active linear/glide reach actual missing Engine/Cylinder collision authority; NOT active movement-success, "
          "full physics, Training crate gameplay or headset acceptance.\n";
}
void MovingBrushSchemaRefusal() {
    Fixture fixture;const auto tables=Build(fixture,true,1u);
    Require(InitializePortableRuntime(tables.packages).passed,"Malformed moving-brush package initialization failed");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared,"Malformed moving-brush metadata preparation failed");
    Require(LoadPortableRuntimeMap(tables.map).passed&&PreparePortableRuntimeObjectLookup().prepared,
        "Malformed moving-brush fixture lacks native Level/lookup");
    const auto legacy=fixture.Save("MalformedLegacy");fixture.Restore(Envelope(legacy,MovingBrushSeed(tables)));
    const auto before=fixture.Save("MalformedBefore");const auto revision=GetPortableRuntimeWorldRevision();
    const auto tick=AdvancePortableLevelDynamicActors(0.125f,0);
    Require(!tick.execution.passed()&&!tick.execution.committed&&!tick.completed&&
        tick.execution.error.find("OldLocation")!=std::string::npos,"Physics9 accepted mismatched actual OldLocation declaration: "+tick.execution.error);
    Require(ReadPortableRuntimeLevelTickParity()==0&&fixture.Save("MalformedAfter")==before&&
        GetPortableRuntimeWorldRevision()==revision,"Physics9 schema refusal retained a partial root");++refusals;
    fixture.Unchanged();
}
} // namespace
int main() {
    try {Synthetic();TriggerControl();DeusExLevelInfoControl();LightControl();ScalarLightControl();MoverControl();MovingBrushPhysics();MovingBrushSchemaRefusal();return 0;}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}

