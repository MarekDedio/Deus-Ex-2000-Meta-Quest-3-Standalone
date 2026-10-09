#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"
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

// Generated UE1 packages only. No original game, actor startup, Spawn, headset,
// or user checkpoint is read/written by this executable.
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
void U16(Bytes& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
}
void U32(Bytes& bytes, std::uint32_t value) {
    U16(bytes, static_cast<std::uint16_t>(value)); U16(bytes, static_cast<std::uint16_t>(value >> 16u));
}
void U64(Bytes& bytes, std::uint64_t value) { U32(bytes, static_cast<std::uint32_t>(value)); U32(bytes, static_cast<std::uint32_t>(value >> 32u)); }
void Index(Bytes& bytes, std::int32_t value) {
    auto rest = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((rest & 63u) | (value < 0 ? 128u : 0u)); rest >>= 6u;
    if (rest) first |= 64u;
    bytes.push_back(first);
    while (rest) { auto next = static_cast<std::uint8_t>(rest & 127u); rest >>= 7u; if (rest) next |= 128u; bytes.push_back(next); }
}
void Replace32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8u * i));
}
void Append(Bytes& bytes, const Bytes& other) { bytes.insert(bytes.end(), other.begin(), other.end()); }
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    Require(size <= 4u * 1024u * 1024u, "Generated fixture/checkpoint exceeds independent read bound");
    Bytes bytes(static_cast<std::size_t>(size)); std::ifstream file(path, std::ios::binary);
    Require(static_cast<bool>(file), "Cannot inspect generated fixture/checkpoint");
    if (!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "Generated fixture/checkpoint was truncated"); return bytes;
}
void WriteBytes(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    Require(static_cast<bool>(file), "Cannot create generated fixture/checkpoint");
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "Cannot write generated fixture/checkpoint");
}
struct Code {
    Bytes raw;
    std::size_t logical{};
};
Code Token(std::uint8_t token) { return {{token}, 1u}; }
Code Ref(std::uint8_t token, std::int32_t reference) { Code code{{token}, 5u}; Index(code.raw, reference); return code; }
Code Int(std::int32_t value) { Code code{{0x1du}, 5u}; U32(code.raw, static_cast<std::uint32_t>(value)); return code; }
Code Float(float value) { Code code{{0x1eu}, 5u}; std::uint32_t bits{}; std::memcpy(&bits, &value, 4u); U32(code.raw, bits); return code; }
Code Join(std::initializer_list<Code> parts) {
    Code code; for (const auto& part : parts) { Append(code.raw, part.raw); code.logical += part.logical; } return code;
}
Code Assign(Code left, Code right) { return Join({Token(0x0fu), std::move(left), std::move(right)}); }
Code Return(Code value = Token(0x0bu)) { return Join({Token(0x04u), std::move(value)}); }
Code Native(std::uint16_t index, std::initializer_list<Code> arguments={}) {
    Code code;
    if(index>=256u) { code.raw={static_cast<std::uint8_t>(0x60u+(index>>8u)),static_cast<std::uint8_t>(index)}; code.logical=2u; }
    else { code.raw={static_cast<std::uint8_t>(index)}; code.logical=1u; }
    for(const auto& argument:arguments) { Append(code.raw,argument.raw); code.logical+=argument.logical; }
    code.raw.push_back(0x16u); ++code.logical; return code;
}
Code Slot(std::int32_t property, std::int32_t index, bool defaults = true) {
    return Join({Token(0x1au), Int(index), Ref(defaults ? 0x02u : 0x01u, property)});
}
Code Member(std::int32_t field, std::int32_t property) { return Join({Ref(0x36u, field), Ref(0x02u, property)}); }

// Full v68 package writer, adapted from the existing generated inventory/table
// fixtures. Field Children/Next ownership is real, not a private host injection.
struct Package {
    std::string stem;
    std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
    std::vector<std::function<Bytes(std::int32_t,std::int32_t)>> builders;
    std::map<std::int32_t,std::vector<std::int32_t>> children;
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
        const auto ref = static_cast<std::int32_t>(exports.size());
        if (field) children[outer].push_back(ref);
        return ref;
    }
    void Tag(Bytes& bytes, const std::string& name, std::uint8_t type, const Bytes& value,
        std::uint8_t slot = 0u, const std::string& structName = {}) {
        Require(value.size() <= 255u, "Generated tag exceeds one-byte fixture bound");
        Index(bytes, Name(name)); bytes.push_back(static_cast<std::uint8_t>(type | 0x50u | (slot ? 0x80u : 0u)));
        if (type == 10u) Index(bytes, Name(structName));
        bytes.push_back(static_cast<std::uint8_t>(value.size()));
        if (slot) bytes.push_back(slot);
        Append(bytes, value);
    }
    void IntTag(Bytes& bytes, const std::string& name, std::int32_t value, std::uint8_t slot = 0u) {
        Bytes data; U32(data, static_cast<std::uint32_t>(value)); Tag(bytes, name, 2u, data, slot);
    }
    void FloatTag(Bytes& bytes, const std::string& name, float value) {
        Bytes data; std::uint32_t bits{}; std::memcpy(&bits,&value,4u); U32(data,bits); Tag(bytes,name,4u,data);
    }
    void ObjectTag(Bytes& bytes, const std::string& name, std::int32_t reference) {
        Bytes data; Index(data, reference); Tag(bytes, name, 5u, data);
    }
    void NameTag(Bytes& bytes, const std::string& name, const std::string& value) {
        Bytes data; Index(data, Name(value)); Tag(bytes, name, 6u, data);
    }
    void ClassBody(std::int32_t ref, Bytes defaults) {
        builders.at(static_cast<std::size_t>(ref - 1)) = [this,ref,defaults=std::move(defaults)](std::int32_t next,std::int32_t child) {
            const auto& entry = exports.at(static_cast<std::size_t>(ref - 1)); Bytes body;
            Index(body, entry.ObjBase); Index(body, next); Index(body, 0); Index(body, child); Index(body, entry.ObjName);
            U32(body, 0); U32(body, 0); U32(body, 0); U64(body, 0); U64(body, 0); U16(body, 0xffffu); U32(body, 0);
            U32(body, 0); body.resize(body.size()+16u,0u); for (unsigned i=0u;i<4u;++i) Index(body,0);
            Append(body,defaults); Index(body,0); return body;
        };
    }
    void StructBody(std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref - 1)) = [this,ref](std::int32_t next,std::int32_t child) {
            Bytes body{0u}; Index(body,0); Index(body,next); Index(body,0); Index(body,child);
            Index(body,exports.at(static_cast<std::size_t>(ref-1)).ObjName); U32(body,0); U32(body,0); U32(body,0); return body;
        };
    }
    std::int32_t Property(const std::string& name, const std::string& type, std::int32_t owner,
        std::int32_t core, std::uint32_t dimension=1u, std::int32_t target=0, std::uint32_t flags=0u) {
        const auto ref=Export(name,Import(type,core),0,owner,true);
        builders.at(static_cast<std::size_t>(ref-1))=[type,dimension,target,flags](std::int32_t next,std::int32_t) {
            Bytes body{0u}; Index(body,0); Index(body,next); U32(body,dimension); U32(body,flags); Index(body,0);
            if (type=="ObjectProperty" || type=="ByteProperty" || type=="StructProperty") Index(body,target);
            return body;
        }; return ref;
    }
    std::int32_t Function(const std::string& name, std::int32_t owner, std::int32_t core, Code code,
        std::uint16_t native=0u) {
        const auto ref=Export(name,Import("Function",core),0,owner,true);
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,native,code=std::move(code)](std::int32_t next,std::int32_t child) {
            Bytes body{0u}; Index(body,0); Index(body,next); Index(body,0); Index(body,child);
            Index(body,exports.at(static_cast<std::size_t>(ref-1)).ObjName); U32(body,0); U32(body,0);
            U32(body,static_cast<std::uint32_t>(code.logical)); Append(body,code.raw); U16(body,native); body.push_back(0u); U32(body,native ? 0x402u : 2u); return body;
        }; return ref;
    }
    void ActorBody(std::int32_t ref, Bytes properties={}) {
        properties.push_back(0u); builders.at(static_cast<std::size_t>(ref-1))=[properties=std::move(properties)](std::int32_t,std::int32_t) { return properties; };
    }
    Bytes Serialize() {
        std::map<std::int32_t,std::int32_t> next;
        for (const auto& [owner,list]:children) { (void)owner; for(std::size_t i=1u;i<list.size();++i) next[list[i-1]]=list[i]; }
        Bytes bytes; U32(bytes,0x9e2a83c1u); U16(bytes,68u); U16(bytes,0u); bytes.resize(56u,0u);
        std::vector<Bytes> bodies; std::vector<std::int32_t> offsets;
        for (std::size_t i=0u;i<exports.size();++i) {
            const auto ref=static_cast<std::int32_t>(i+1u); const auto found=children.find(ref);
            auto body=builders[i] ? builders[i](next[ref],found==children.end() || found->second.empty() ? 0 : found->second.front()) : Bytes{};
            offsets.push_back(static_cast<std::int32_t>(bytes.size())); Append(bytes,body); bodies.push_back(std::move(body));
        }
        Replace32(bytes,12u,static_cast<std::uint32_t>(names.size())); Replace32(bytes,16u,static_cast<std::uint32_t>(bytes.size()));
        for(const auto& name:names) { Index(bytes,static_cast<std::int32_t>(name.size()+1u)); bytes.insert(bytes.end(),name.begin(),name.end()); bytes.push_back(0u); U32(bytes,0); }
        Replace32(bytes,20u,static_cast<std::uint32_t>(exports.size())); Replace32(bytes,24u,static_cast<std::uint32_t>(bytes.size()));
        for(std::size_t i=0u;i<exports.size();++i) {
            const auto& e=exports[i]; Index(bytes,e.ObjClass); Index(bytes,e.ObjBase); U32(bytes,static_cast<std::uint32_t>(e.ObjOuter)); Index(bytes,e.ObjName);
            U32(bytes,static_cast<std::uint32_t>(e.ObjFlags)); Index(bytes,static_cast<std::int32_t>(bodies[i].size())); if(!bodies[i].empty()) Index(bytes,offsets[i]);
        }
        Replace32(bytes,28u,static_cast<std::uint32_t>(imports.size())); Replace32(bytes,32u,static_cast<std::uint32_t>(bytes.size()));
        for(const auto& e:imports) { Index(bytes,e.ClassPackage); Index(bytes,e.ClassName); U32(bytes,static_cast<std::uint32_t>(e.ObjOuter)); Index(bytes,e.ObjName); }
        return bytes;
    }
};
struct Fixture {
    std::filesystem::path parent,directory;
    std::map<std::filesystem::path,Bytes> sourceBytes;
    Fixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for(unsigned attempt=0u;attempt<20u;++attempt) {
            const auto candidate=parent/("deusex-class-default-test-"+std::to_string(stamp)+'-'+std::to_string(attempt));
            if(std::filesystem::create_directory(candidate)) { directory=std::filesystem::canonical(candidate); break; }
        }
        Require(!directory.empty() && directory.parent_path()==parent,"Generated CDO fixture escaped owned temporary parent");
        std::filesystem::create_directory(directory/"System"); std::filesystem::create_directory(directory/"Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime(); std::error_code error;
        const auto actual=std::filesystem::weakly_canonical(directory,error);
        if(!error && !directory.empty() && actual==directory && actual.parent_path()==parent &&
            actual.filename().string().rfind("deusex-class-default-test-",0u)==0u) std::filesystem::remove_all(actual,error);
    }
    PortablePackageTables Write(Package& package,bool map=false) {
        const auto path=directory/(map ? "Maps" : "System")/(package.stem+(map ? ".dx" : ".u"));
        auto bytes=package.Serialize(); WriteBytes(path,bytes); sourceBytes.emplace(path,std::move(bytes));
        auto table=LoadPortablePackageTables(path.string());
        Require(table.version==68u && table.exports.size()==package.exports.size(),"Generated fixture did not pass production table loader"); return table;
    }
    Bytes Snapshot(const std::string& name) const {
        const auto path=directory/(name+".sav"); Require(SavePortableRuntimeState(path.string()),"Cannot snapshot generated CDO runtime"); return ReadBytes(path);
    }
    void UnchangedSources() const { for(const auto& [path,bytes]:sourceBytes) Require(ReadBytes(path)==bytes,"CDO mutation modified generated package source"); }
};
QuestVr::Vm::Result Call(const std::string& actor,const std::string& function,const std::vector<Evaluation>& args={}) {
    auto result=ExecutePortableActorFunction("DefaultFixture."+actor,function,args);
    Require(result.passed(),function+" failed: "+result.error+" at "+result.function+':'+std::to_string(result.offset)); return result;
}
Value Cdo(const std::string& cls,const std::string& property,std::uint32_t slot=0u) {
    return ReadPortableClassDefault("DefaultClasses."+cls,property,slot);
}
Value Instance(const std::string& actor,const std::string& property,std::uint32_t slot=0u) {
    return ReadPortableActorScriptProperty("DefaultFixture."+actor,property,slot);
}
void Same(const Value& actual,const Value& expected,const std::string& description) {
    Require(QuestVr::Vm::Equal(actual,expected),description);
}
void RefusedCall(const Fixture& fixture,const std::string& actor,const std::string& function,
    const std::vector<Evaluation>& arguments={}) {
    const auto before=fixture.Snapshot("BeforeRefusal");
    const auto result=ExecutePortableActorFunction("DefaultFixture."+actor,function,arguments);
    Require(!result.passed() && !result.committed,"Invalid CDO operation silently committed: "+function);
    Require(fixture.Snapshot("AfterRefusal")==before,"Failed CDO operation changed canonical persistent state: "+function); ++refusals;
}
std::uint32_t Read32(const Bytes& bytes,std::size_t& cursor) {
    Require(cursor<=bytes.size() && bytes.size()-cursor>=4u,"Independent checkpoint inspection encountered truncation");
    std::uint32_t value{}; for(unsigned i=0u;i<4u;++i) value|=static_cast<std::uint32_t>(bytes[cursor++])<<(8u*i); return value;
}
std::size_t PrefixSize(const Bytes& bytes) {
    std::size_t cursor{}; Require(Read32(bytes,cursor)==0x53515844u,"Independent runtime magic mismatch");
    const auto version=Read32(bytes,cursor); Require(version>=1u && version<=6u,"Independent runtime version mismatch");
    const auto skipString=[&]() {
        const auto length=Read32(bytes,cursor); Require(cursor<=bytes.size() && length<=bytes.size()-cursor,"Independent checkpoint string exceeds input"); cursor+=length;
    };
    const auto skipStrings=[&]() { const auto count=Read32(bytes,cursor); for(std::uint32_t i=0u;i<count;++i) skipString(); };
    skipStrings(); skipStrings(); skipStrings();
    if(version>=2u) { static_cast<void>(Read32(bytes,cursor)); const auto count=Read32(bytes,cursor); for(std::uint32_t i=0u;i<count;++i) { skipString(); static_cast<void>(Read32(bytes,cursor)); } }
    if(version>=3u) { static_cast<void>(Read32(bytes,cursor)); static_cast<void>(Read32(bytes,cursor)); for(unsigned i=0u;i<4u;++i) skipStrings(); }
    return cursor;
}
QuestVr::ScriptSavedState SavedScript(const Bytes& bytes) {
    auto cursor=PrefixSize(bytes); const auto size=Read32(bytes,cursor);
    Require(size==bytes.size()-cursor,"Independent script trailer size mismatch");
    return QuestVr::DecodeScriptSavedState(Bytes(bytes.begin()+static_cast<std::ptrdiff_t>(cursor),bytes.end()));
}
Bytes Envelope(const Bytes& prefixSource,const QuestVr::ScriptSavedState& script,std::uint32_t version=0u) {
    const auto prefix=PrefixSize(prefixSource); Bytes bytes(prefixSource.begin(),prefixSource.begin()+static_cast<std::ptrdiff_t>(prefix));
    const bool states=std::any_of(script.objects.begin(),script.objects.end(),[](const auto& object) { return object.state.has_value(); });
    Replace32(bytes,4u,version ? version : !script.classDefaults.empty() ? 6u : states ? 5u : 4u);
    const auto blob=QuestVr::EncodeScriptSavedState(script); U32(bytes,static_cast<std::uint32_t>(blob.size())); Append(bytes,blob); return bytes;
}
Bytes LegacyEnvelope(std::uint32_t version) {
    Bytes bytes; U32(bytes,0x53515844u); U32(bytes,version); for(unsigned i=0u;i<3u;++i) U32(bytes,0u);
    if(version>=2u) { std::uint32_t bits{}; const float health=100.0f; std::memcpy(&bits,&health,4u); U32(bytes,bits); U32(bytes,0u); }
    if(version>=3u) for(unsigned i=0u;i<6u;++i) U32(bytes,0u);
    return bytes;
}
QuestVr::ScriptSavedProperty& DefaultProperty(QuestVr::ScriptSavedState& state,const std::string& cls,const std::string& name) {
    for(auto& record:state.classDefaults) if(record.classPath=="DefaultClasses."+cls)
        for(auto& property:record.properties) if(property.name==name) return property;
    throw std::runtime_error("Generated canonical CDO checkpoint lacks "+cls+'.'+name);
}
void RefusedCheckpoint(const Fixture& fixture,const Bytes& bytes,const std::string& label) {
    const auto before=fixture.Snapshot("BeforeCheckpointRefusal"); const auto path=fixture.directory/("Reject-"+label+".sav"); WriteBytes(path,bytes);
    Require(!ValidatePortableRuntimeState(path.string(),"DefaultFixture"),"Invalid CDO checkpoint validated: "+label);
    Require(fixture.Snapshot("AfterRejectedValidation")==before,"Rejected validation mutated runtime: "+label);
    Require(!LoadPortableRuntimeState(path.string()),"Invalid CDO checkpoint loaded: "+label);
    Require(fixture.Snapshot("AfterRejectedLoad")==before,"Rejected CDO checkpoint changed persistent state: "+label); ++refusals;
}
template<class Mutation> void RefusedScript(const Fixture& fixture,const Bytes& current,Mutation mutation,const std::string& label) {
    auto state=SavedScript(current); mutation(state); RefusedCheckpoint(fixture,Envelope(current,state),label);
}
template<class Action> void RefusedCodec(Action action,const std::string& label) {
    bool threw{}; try { action(); } catch(const std::exception&) { threw=true; }
    Require(threw,"Invalid class-default codec input was accepted: "+label); ++refusals;
}
void Synthetic() {
    Fixture fixture;
    Package core; core.stem="Core"; const auto corePackage=core.ImportPackage("Core");
    const auto object=core.Export("Object"); core.ClassBody(object,{});
    const auto record=core.Export("Record",core.Import("Struct",corePackage));
    const auto count=core.Property("Count","IntProperty",record,corePackage);
    const auto enginePackage=core.ImportPackage("Engine"), actorConstraint=core.Import("Actor",enginePackage);
    const auto target=core.Property("Target","ObjectProperty",record,corePackage,1u,actorConstraint); core.StructBody(record);
    const auto coreTable=fixture.Write(core);

    Package engine; engine.stem="Engine";
    const auto engineCore=engine.ImportPackage("Core"), importedObject=engine.Import("Object",engineCore);
    const auto actor=engine.Export("Actor",0,importedObject);
    engine.Property("CollisionRadius","FloatProperty",actor,engineCore);
    engine.Property("CollisionHeight","FloatProperty",actor,engineCore);
    const auto setCollision=engine.Function("SetCollisionSize",actor,engineCore,{},283u);
    engine.Property("NewRadius","FloatProperty",setCollision,engineCore,1u,0,0x80u);
    engine.Property("NewHeight","FloatProperty",setCollision,engineCore,1u,0,0x80u);
    engine.Property("ReturnValue","BoolProperty",setCollision,engineCore,1u,0,0x480u);
    Bytes bounds; engine.FloatTag(bounds,"CollisionRadius",12.5f); engine.FloatTag(bounds,"CollisionHeight",22.0f);
    engine.ClassBody(actor,bounds);
    const auto engineTable=fixture.Write(engine);

    Package classes; classes.stem="DefaultClasses";
    const auto classesCore=classes.ImportPackage("Core"), classesEngine=classes.ImportPackage("Engine");
    const auto importedActor=classes.Import("Actor",classesEngine);
    const auto base=classes.Export("Base",0,importedActor), derived=classes.Export("Derived",0,base);
    const auto sibling=classes.Export("Sibling",0,base), child=classes.Export("Child",0,derived);
    const auto counter=classes.Property("Counter","IntProperty",base,classesCore);
    const auto source=classes.Property("Source","IntProperty",base,classesCore);
    const auto slots=classes.Property("Slots","IntProperty",base,classesCore,3u);
    const auto label=classes.Property("Label","NameProperty",base,classesCore);
    const auto enabled=classes.Property("Enabled","BoolProperty",base,classesCore);
    const auto ratio=classes.Property("Ratio","FloatProperty",base,classesCore);
    const auto link=classes.Property("Link","ObjectProperty",base,classesCore,1u,importedActor);
    const auto importedRecord=classes.Import("Record",classesCore);
    const auto payload=classes.Property("Payload","StructProperty",base,classesCore,1u,importedRecord);
    const auto importedCount=classes.Import("Count",importedRecord,"IntProperty");
    const auto importedTarget=classes.Import("Target",importedRecord,"ObjectProperty");
    Bytes defaults; classes.IntTag(defaults,"Counter",10); classes.IntTag(defaults,"Source",7);
    for(std::uint8_t i=0u;i<3u;++i) classes.IntTag(defaults,"Slots",3+2*i,i);
    classes.NameTag(defaults,"Label","Baseline"); Index(defaults,classes.Name("Enabled")); defaults.push_back(3u);
    Bytes f; std::uint32_t bits{}; const float baseline=1.25f; std::memcpy(&bits,&baseline,4u); U32(f,bits); classes.Tag(defaults,"Ratio",4u,f);
    classes.ObjectTag(defaults,"Link",0); Bytes members; U32(members,4u); Index(members,0); classes.Tag(defaults,"Payload",10u,members,0u,"Record");
    classes.ClassBody(base,defaults); Bytes overrides; classes.IntTag(overrides,"Counter",20); classes.IntTag(overrides,"Source",12); classes.ClassBody(derived,overrides);
    overrides.clear(); classes.IntTag(overrides,"Counter",30); classes.IntTag(overrides,"Source",13); classes.ClassBody(sibling,overrides); classes.ClassBody(child,{});
    classes.Function("CopyDefault",base,classesCore,Join({Assign(Ref(0x02u,counter),Ref(0x02u,source)),Return()}));
    classes.Function("ReadDefault",base,classesCore,Return(Ref(0x02u,counter)));
    classes.Function("ReadSlot",base,classesCore,Return(Slot(slots,1)));
    const auto set=classes.Function("SetDefaults",base,classesCore,Join({
        Assign(Ref(0x02u,counter),Int(99)),Assign(Slot(slots,1),Int(77)),Assign(Ref(0x02u,label),Ref(0x21u,classes.Name("Changed"))),
        Assign(Ref(0x02u,enabled),Token(0x27u)),Assign(Ref(0x02u,ratio),Float(2.5f)),Assign(Ref(0x02u,link),Token(0x17u)),
        Assign(Member(importedCount,payload),Int(11)),Assign(Member(importedTarget,payload),Token(0x17u)),Return()}));
    classes.Function("SetInstance",base,classesCore,Join({Assign(Ref(0x01u,counter),Int(88)),Return()}));
    classes.Function("Rollback",base,classesCore,Join({Ref(0x1cu,set),Token(0x16u),Code{{0x61u,0x16u,0x16u},3u},Return()}));
    classes.Function("WrongObject",base,classesCore,Join({Assign(Ref(0x02u,counter),Int(123)),Assign(Ref(0x02u,link),Ref(0x20u,base)),Return()}));
    classes.Function("BadSlot",base,classesCore,Join({Assign(Slot(slots,3),Int(5)),Return()}));
    classes.Function("NegativeSlot",base,classesCore,Join({Assign(Slot(slots,-100),Int(6)),Return()}));
    classes.Function("SetBounds",base,classesCore,Return(Native(283u,{Float(18.5f),Int(31)})));
    classes.Function("NegativeBounds",base,classesCore,Return(Native(283u,{Float(-3.5f),Float(-8.25f)})));
    classes.Function("BoundsRollback",base,classesCore,Join({
        Assign(Ref(0x02u,counter),Int(321)), Native(283u,{Float(44.0f),Float(55.0f)}),
        Assign(Ref(0x01u,source),Int(222)), Native(278u), Return()}));
    classes.Function("BadBoundsName",base,classesCore,Join({Assign(Ref(0x02u,counter),Int(321)),
        Native(283u,{Ref(0x21u,classes.Name("BadRadius")),Float(3.0f)}),Return()}));
    classes.Function("BadBoundsBool",base,classesCore,Join({Assign(Ref(0x02u,counter),Int(321)),
        Native(283u,{Token(0x27u),Float(3.0f)}),Return()}));
    classes.Function("BadBoundsCount",base,classesCore,Join({Assign(Ref(0x02u,counter),Int(321)),Native(283u,{Float(1.0f)}),Return()}));
    classes.Function("BadBoundsNan",base,classesCore,Join({Assign(Ref(0x02u,counter),Int(321)),
        Native(283u,{Float(std::numeric_limits<float>::quiet_NaN()),Float(3.0f)}),Return()}));
    classes.Function("BadBoundsInfiniteHeight",base,classesCore,Join({Assign(Ref(0x02u,counter),Int(321)),
        Native(283u,{Float(1.0f),Float(std::numeric_limits<float>::infinity())}),Return()}));
    classes.Function("ClearState",base,classesCore,Join({Native(113u,{Ref(0x21u,classes.Name("None"))}),Return()}));
    const auto putPayload=classes.Export("PutPayload",classes.Import("Function",classesCore),0,base,true);
    const auto parameter=classes.Property("Value","StructProperty",putPayload,classesCore,1u,importedRecord,0x80u);
    const auto parameterCode=Join({Assign(Ref(0x02u,payload),Ref(0x00u,parameter)),Return()});
    classes.builders.at(static_cast<std::size_t>(putPayload-1))=[&classes,putPayload,parameterCode](std::int32_t next,std::int32_t first) {
        Bytes body{0u}; Index(body,0); Index(body,next); Index(body,0); Index(body,first); Index(body,classes.exports.at(static_cast<std::size_t>(putPayload-1)).ObjName);
        U32(body,0); U32(body,0); U32(body,static_cast<std::uint32_t>(parameterCode.logical)); Append(body,parameterCode.raw); U16(body,0); body.push_back(0u); U32(body,2u); return body;
    };
    const auto classesTable=fixture.Write(classes);
    // Struct import identities are verified independently by the actual loader.
    Require(GetPortableObjectPath(coreTable,count)=="Record.Count" && GetPortableObjectPath(coreTable,target)=="Record.Target","Generated struct declaration identity was not retained");
    Package map; map.stem="DefaultFixture"; const auto importedClasses=map.ImportPackage("DefaultClasses");
    for(const auto& [name,cls]:std::vector<std::pair<std::string,std::string>>{{"Base0","Base"},{"Base1","Base"},{"Derived0","Derived"},{"Derived1","Derived"},{"Sibling0","Sibling"},{"Child0","Child"}}) {
        const auto ref=map.Export(name,map.Import(cls,importedClasses)); Bytes properties;
        if(name=="Base0") map.IntTag(properties,"Counter",42);
        map.ActorBody(ref,properties);
    }
    const auto mapTable=fixture.Write(map,true);
    const auto initialized=InitializePortableRuntime({coreTable,engineTable,classesTable});
    Require(initialized.passed && initialized.functions>=9u,"Generated class-default runtime failed real metadata initialization");
    Require(LoadPortableRuntimeMap(mapTable).passed,"Generated map actors were not loaded");
    Require(!GetPortableRuntimeScriptStatePresent(),"Read-only generated metadata initialization created persistent writes");
    const auto legacy=fixture.Snapshot("Legacy"); Require(legacy.size()>8u && legacy[4u]==3u,"Initial generated snapshot is not legacy v3");
    Same(Call("Base1","SetCollisionSize",{{Value::Float(18.5f),{}},{Value::Integer(31),{}}}).value,Value::Bool(true),
        "Actual Engine.Actor native 283 declaration did not return Bool success");
    Same(Call("Base1","SetBounds").value,Value::Bool(true),"Native 283 did not return its declared Bool success");
    Same(Instance("Base1","CollisionRadius"),Value::Float(18.5f),"Native 283 failed to update actor radius");
    Same(Instance("Base1","CollisionHeight"),Value::Float(31.0f),"Native 283 failed numeric Int-to-Float height conversion");
    Same(Instance("Base0","CollisionRadius"),Value::Float(12.5f),"Native 283 altered a different actor");
    Same(ReadPortableClassDefault("Engine.Actor","CollisionHeight"),Value::Float(22.0f),"Native 283 altered the authored CDO");
    const auto nativeOnly=fixture.Snapshot("NativeOnly"); Require(nativeOnly[4u]==4u,"Native-only reflected writes did not preserve v4 envelope");
    const auto nativeScript=SavedScript(nativeOnly);
    Require(nativeScript.classDefaults.empty() && nativeScript.objects.size()==1u && nativeScript.objects[0].properties.size()==2u,
        "Native-only capture invented CDOs or omitted one collision bound");
    RefusedCall(fixture,"Base1","BoundsRollback");
    for(const auto& function:{"BadBoundsName","BadBoundsBool","BadBoundsCount","BadBoundsNan","BadBoundsInfiniteHeight"}) RefusedCall(fixture,"Base1",function);
    Same(Cdo("Base","Counter"),Value::Integer(10),"Failed native call did not roll back earlier CDO mutation");
    Same(Instance("Base1","CollisionRadius"),Value::Float(18.5f),"Failed native call did not roll back prior radius write");
    Same(Instance("Base1","CollisionHeight"),Value::Float(31.0f),"Failed native call did not roll back prior height write");
    Same(Instance("Base1","Source"),Value::Integer(7),"Failed native call did not roll back later instance mutation");
    Same(Call("Base1","NegativeBounds").value,Value::Bool(true),"Native 283 invented a negative-bound rejection");
    Same(Instance("Base1","CollisionRadius"),Value::Float(-3.5f),"Native 283 incorrectly clamped radius");
    Same(Instance("Base1","CollisionHeight"),Value::Float(-8.25f),"Native 283 incorrectly clamped height");
    Require(LoadPortableRuntimeState((fixture.directory/"NativeOnly.sav").string()),"Native-only v4 restore failed");
    Require(fixture.Snapshot("NativeRoundTrip")==nativeOnly,"Native-only v4 restore was not canonical");
    Call("Base1","ClearState"); const auto stateOnly=fixture.Snapshot("StateOnly");
    Require(stateOnly[4u]==5u && SavedScript(stateOnly).objects[0].state.has_value(),"Actual GotoState(None) did not produce legacy v5 state capture");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Native/state fixture cleanup failed");
    Same(Instance("Base1","CollisionRadius"),Value::Float(12.5f),"Legacy baseline restore retained native radius");
    Same(Cdo("Base","Counter"),Value::Integer(10),"Base default mismatch"); Same(Cdo("Derived","Counter"),Value::Integer(20),"Derived default mismatch");
    Call("Base0","CopyDefault"); Same(Cdo("Base","Counter"),Value::Integer(7),"Original-style default assignment did not alter shared concrete CDO");
    Same(Call("Base1","ReadDefault").value,Value::Integer(7),"Second existing actor did not read same class default");
    Same(Instance("Base0","Counter"),Value::Integer(42),"Explicit instance override was changed by CDO write"); Same(Instance("Base1","Counter"),Value::Integer(10),"Existing inherited instance changed retroactively");
    Same(Cdo("Derived","Counter"),Value::Integer(20),"Base CDO write changed derived CDO"); Same(Cdo("Sibling","Counter"),Value::Integer(30),"Base CDO write changed sibling CDO");
    RefusedCall(fixture,"Derived0","Rollback"); Same(Cdo("Derived","Counter"),Value::Integer(20),"Rollback failed to remove previously missing concrete CDO patches");
    Call("Derived0","SetDefaults");
    Same(Cdo("Derived","Counter"),Value::Integer(99),"Concrete derived default assignment failed"); Same(Cdo("Child","Counter"),Value::Integer(20),"Derived CDO write changed already-loaded child CDO");
    Same(Instance("Derived0","Counter"),Value::Integer(20),"Writing actor itself changed retroactively"); Same(Instance("Derived1","Counter"),Value::Integer(20),"Other old derived instance changed retroactively");
    Same(Cdo("Derived","Slots",0u),Value::Integer(3),"Fixed array other slot changed"); Same(Cdo("Derived","Slots",1u),Value::Integer(77),"Fixed array default slot write failed");
    Same(Cdo("Derived","Slots",2u),Value::Integer(7),"Fixed array trailing slot changed"); Same(Instance("Derived1","Slots",1u),Value::Integer(5),"Old fixed array default changed retroactively");
    Same(Call("Derived1","ReadSlot").value,Value::Integer(77),"Shared default array getter failed");
    Same(Cdo("Derived","Label"),Value::Text(Kind::Name,"Changed"),"Name default write failed"); Same(Cdo("Derived","Enabled"),Value::Bool(true),"Bool default write failed");
    Same(Cdo("Derived","Ratio"),Value::Float(2.5f),"Float default write failed"); Same(Cdo("Derived","Link"),Value::Text(Kind::Object,"DefaultFixture.Derived0"),"Typed object default write failed");
    const auto savedPayload=Cdo("Derived","Payload"); Same(savedPayload.fields.at("count"),Value::Integer(11),"Nested default member write failed");
    Same(savedPayload.fields.at("target"),Value::Text(Kind::Object,"DefaultFixture.Derived0"),"Nested typed object default write failed");
    Same(Instance("Derived1","Payload").fields.at("count"),Value::Integer(4),"Existing struct storage changed retroactively");
    auto detached=savedPayload; detached.fields["count"]=Value::Integer(-999); Same(Cdo("Derived","Payload"),savedPayload,"Default getter aliases mutable CDO storage");
    Call("Derived1","SetInstance"); Same(Instance("Derived1","Counter"),Value::Integer(88),"Instance write failed"); Same(Cdo("Derived","Counter"),Value::Integer(99),"Instance write altered CDO");
    RefusedCall(fixture,"Base1","Rollback"); RefusedCall(fixture,"Derived1","WrongObject");
    // The pinned ArrayElement expression clamps, whereas saved/raw indices are
    // strict. Verify both tails without replacing original bytecode semantics.
    Call("Derived1","BadSlot"); Same(Cdo("Derived","Slots",2u),Value::Integer(5),"High bytecode array index did not clamp to final slot");
    Call("Derived1","NegativeSlot"); Same(Cdo("Derived","Slots",0u),Value::Integer(6),"Negative bytecode array index did not clamp to zero");
    auto malformed=savedPayload; malformed.fields.erase("target"); RefusedCall(fixture,"Derived1","PutPayload",{{malformed,{}}});
    malformed=savedPayload; malformed.fields["target"]=Value::Text(Kind::Object,"DefaultClasses.Base"); RefusedCall(fixture,"Derived1","PutPayload",{{malformed,{}}});
    Same(Call("Derived0","SetBounds").value,Value::Bool(true),"Mixed CDO/native collision call failed");
    Same(Instance("Derived0","CollisionHeight"),Value::Float(31.0f),"Mixed CDO/native collision field did not commit");
    fixture.UnchangedSources();
    // Persistence/replacement controls are below; all paths belong to fixture.
    const auto current=fixture.Snapshot("Current");
    Require(current.size()>8u && current[4u]==6u,"Shared class defaults did not choose the v6 envelope");
    const auto canonical=SavedScript(current);
    Require(canonical.classDefaults.size()==2u && canonical.objects.size()==2u,"Mixed v6 checkpoint lacks concrete CDOs/instance/native records");
    const auto blob=QuestVr::EncodeScriptSavedState(canonical);
    Require(blob[6u]==3u,"Class-default trailer did not select structural codec 3");
    Require(QuestVr::EncodeScriptSavedState(QuestVr::DecodeScriptSavedState(blob))==blob,"Class-default codec 3 roundtrip is not canonical");
    Require(ValidatePortableRuntimeState((fixture.directory/"Current.sav").string(),"DefaultFixture"),"Generated CDO checkpoint failed nonmutating schema validation");
    Require(fixture.Snapshot("AfterValidation")==current,"Read-only checkpoint validation mutated shared CDOs");
    Require(!ValidatePortableRuntimeState((fixture.directory/"Current.sav").string(),"DifferentMap"),"v6 validation ignored metadata map mismatch");
    Require(fixture.Snapshot("AfterWrongMapValidation")==current,"Failed map validation mutated shared CDOs");
    RefusedScript(fixture,current,[](auto& state) { state.classDefaults.back().classPath="DefaultClasses.Missing"; },"MissingClass");
    RefusedScript(fixture,current,[](auto& state) { state.classDefaults.back().classPath="DefaultFixture.Derived0"; },"ActorNotClass");
    RefusedScript(fixture,current,[](auto& state) { state.classDefaults.back().classPath="Core.Object"; },"NonActorClass");
    RefusedScript(fixture,current,[](auto& state) { DefaultProperty(state,"Derived","Counter").key="DefaultClasses.Sibling.Counter"; },"WrongDeclaringKey");
    RefusedScript(fixture,current,[](auto& state) { DefaultProperty(state,"Derived","Counter").name="MissingProperty"; },"MissingProperty");
    RefusedScript(fixture,current,[](auto& state) { DefaultProperty(state,"Derived","Counter").value=Value::Float(5.0f); },"WrongValueKind");
    RefusedScript(fixture,current,[](auto& state) { DefaultProperty(state,"Derived","Link").value=Value::Text(Kind::Object,"DefaultClasses.Base"); },"WrongObjectClass");
    RefusedScript(fixture,current,[](auto& state) { DefaultProperty(state,"Derived","Link").value=Value::Text(Kind::Object,"DefaultFixture.Missing"); },"MissingObject");
    RefusedScript(fixture,current,[](auto& state) { DefaultProperty(state,"Derived","Slots").index=3u; },"ArrayIndexPastEnd");
    RefusedScript(fixture,current,[](auto& state) { DefaultProperty(state,"Derived","Slots").index=std::numeric_limits<std::uint32_t>::max(); },"ArrayIndexOverflow");
    RefusedScript(fixture,current,[](auto& state) { DefaultProperty(state,"Derived","Payload").value.fields.erase("target"); },"MissingStructField");
    RefusedScript(fixture,current,[](auto& state) { DefaultProperty(state,"Derived","Payload").value.fields["target"]=Value::Text(Kind::Object,"DefaultClasses.Base"); },"WrongNestedObjectClass");
    RefusedScript(fixture,current,[](auto& state) {
        DefaultProperty(state,"Base","Counter").value=Value::Integer(555);
        DefaultProperty(state,"Derived","Counter").key="DefaultClasses.Base.WrongField";
        state.objects.front().properties.front().value=Value::Float(666.0f);
    },"LateFailureNoPartialPublication");
    RefusedCheckpoint(fixture,Envelope(current,canonical,4u),"Codec3UnderV4");
    RefusedCheckpoint(fixture,Envelope(current,canonical,5u),"Codec3UnderV5");
    auto truncated=current; truncated.pop_back(); RefusedCheckpoint(fixture,truncated,"TruncatedV6");
    auto trailing=current; trailing.push_back(0u); RefusedCheckpoint(fixture,trailing,"TrailingV6");
    RefusedCodec([&] { auto bad=canonical; bad.classDefaults.push_back(bad.classDefaults.front()); bad.classDefaults.back().classPath="defaultclasses.base"; static_cast<void>(QuestVr::EncodeScriptSavedState(bad)); },"CaseCollidingClass");
    RefusedCodec([&] { auto bad=canonical; bad.classDefaults.front().properties.clear(); static_cast<void>(QuestVr::EncodeScriptSavedState(bad)); },"EmptyClassRecord");
    RefusedCodec([&] { auto bad=canonical; auto alias=bad.classDefaults.front().properties.front(); alias.key="DefaultClasses.Forged.Counter";
        bad.classDefaults.front().properties.push_back(alias); static_cast<void>(QuestVr::EncodeScriptSavedState(bad)); },"DuplicatePropertyAlias");
    RefusedCodec([&] { auto bad=canonical; DefaultProperty(bad,"Derived","Ratio").value=Value::Float(std::numeric_limits<float>::infinity()); static_cast<void>(QuestVr::EncodeScriptSavedState(bad)); },"NonFiniteDefault");
    RefusedCodec([&] { QuestVr::ScriptStateLimits limits; limits.maxObjects=canonical.objects.size()+canonical.classDefaults.size()-1u; static_cast<void>(QuestVr::EncodeScriptSavedState(canonical,limits)); },"AggregateActorClassBudget");
    RefusedCodec([&] { auto bad=blob; bad[6u]=4u; static_cast<void>(QuestVr::DecodeScriptSavedState(bad)); },"UnsupportedCodecVersion");
    RefusedCodec([&] { auto bad=blob; bad.pop_back(); static_cast<void>(QuestVr::DecodeScriptSavedState(bad)); },"TruncatedCodec3");
    RefusedCodec([&] { auto bad=blob; bad.push_back(0u); static_cast<void>(QuestVr::DecodeScriptSavedState(bad)); },"TrailingCodec3");
    RefusedCodec([&] { static_cast<void>(ReadPortableClassDefault("DefaultClasses.Derived","Slots",3u)); },"RawGetterArrayBounds");
    Require(fixture.Snapshot("AfterCodecRejections")==current,"Structural/default getter rejections mutated runtime");

    // Class-only captures are valid v6, even with no actor property records.
    auto classOnly=canonical; classOnly.objects.clear(); const auto classOnlyBytes=Envelope(current,classOnly);
    const auto classOnlyPath=fixture.directory/"ClassOnly.sav"; WriteBytes(classOnlyPath,classOnlyBytes);
    Require(ValidatePortableRuntimeState(classOnlyPath.string(),"DefaultFixture"),"Class-only v6 checkpoint did not validate");
    Require(fixture.Snapshot("AfterClassOnlyValidation")==current,"Class-only validation published state");
    Require(LoadPortableRuntimeState(classOnlyPath.string()),"Class-only v6 restore failed");
    Require(fixture.Snapshot("ClassOnlyRoundTrip")==classOnlyBytes,"Class-only v6 restore was not canonical");
    Require(GetPortableRuntimeScriptStatePresent() && GetPortableRuntimeUnsavedScriptState(),
        "Class-only defaults did not participate in the persistent-state guard after saving");
    Require(!LoadPortableRuntimeMap(mapTable).passed,"Map replacement discarded saved class-only defaults");
    bool unloadRefused{};
    try { static_cast<void>(UnloadPortableRuntimeMap()); } catch(const std::exception&) { unloadRefused=true; }
    Require(unloadRefused,"Map unload discarded saved class-only defaults");
    GC::Collect();
    Require(fixture.Snapshot("AfterClassOnlyGuards")==classOnlyBytes,
        "Map guards or garbage collection altered rooted class-only defaults/references");
    Same(Cdo("Derived","Payload"),savedPayload,"Class-only v6 dropped nested shared default");
    Same(Instance("Derived1","Counter"),Value::Integer(20),"Class-only replacement retained omitted instance overlay");
    Same(Instance("Derived0","CollisionRadius"),Value::Float(12.5f),"Class-only replacement retained omitted native overlay");

    // Loading a partial class set must replace, not merge, the previous set.
    auto partial=classOnly; partial.classDefaults.erase(partial.classDefaults.begin()+1);
    const auto partialBytes=Envelope(current,partial); const auto partialPath=fixture.directory/"PartialDefaults.sav"; WriteBytes(partialPath,partialBytes);
    Require(LoadPortableRuntimeState(partialPath.string()),"Partial v6 defaults replacement failed");
    Same(Cdo("Base","Counter"),Value::Integer(7),"Partial replacement dropped retained base CDO");
    Same(Cdo("Derived","Counter"),Value::Integer(20),"Partial replacement retained omitted derived CDO");
    Same(Cdo("Derived","Slots",1u),Value::Integer(5),"Partial replacement retained omitted derived array patch");
    Require(fixture.Snapshot("PartialRoundTrip")==partialBytes,"Partial default replacement was not canonical");
    Require(LoadPortableRuntimeState((fixture.directory/"Current.sav").string()),"Cannot restore full v6 after partial replacement");

    // All five older envelopes clear CDO patches; v4/v5 retain only their own
    // actual reflected/native/state payload. v1/v2 are structurally exact old
    // empty prefixes, written only under this generated fixture's directory.
    for(std::uint32_t version=1u;version<=5u;++version) {
        const auto old=version<=3u ? LegacyEnvelope(version) : version==4u ? nativeOnly : stateOnly;
        const auto path=fixture.directory/("LegacyVersion"+std::to_string(version)+".sav"); WriteBytes(path,old);
        Require(ValidatePortableRuntimeState(path.string(),"DefaultFixture"),"Legacy envelope did not validate: "+std::to_string(version));
        Require(fixture.Snapshot("AfterLegacyValidation")==current,"Legacy validation cleared CDOs: "+std::to_string(version));
        Require(LoadPortableRuntimeState(path.string()),"Legacy envelope did not load: "+std::to_string(version));
        Same(Cdo("Base","Counter"),Value::Integer(10),"Legacy load retained shared base CDO: "+std::to_string(version));
        Same(Cdo("Derived","Counter"),Value::Integer(20),"Legacy load retained shared derived CDO: "+std::to_string(version));
        Same(Instance("Derived1","Counter"),Value::Integer(20),"Legacy load retained omitted instance counter: "+std::to_string(version));
        Same(Instance("Base1","CollisionHeight"),Value::Float(version>=4u ? 31.0f : 22.0f),"Legacy collision payload replacement failed: "+std::to_string(version));
        Require(LoadPortableRuntimeState((fixture.directory/"Current.sav").string()),"Cannot restore full CDOs after legacy envelope: "+std::to_string(version));
        Require(fixture.Snapshot("AfterLegacyCycle")==current,"Legacy/v6 cycle was not canonical: "+std::to_string(version));
    }
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Legacy checkpoint did not clear shared class defaults");
    Same(Cdo("Base","Counter"),Value::Integer(10),"Legacy load retained omitted base CDO patch"); Same(Cdo("Derived","Counter"),Value::Integer(20),"Legacy load retained omitted derived CDO patch");
    Same(Instance("Derived1","Counter"),Value::Integer(20),"Legacy load did not restore immutable instance baseline");
    Require(LoadPortableRuntimeState((fixture.directory/"Current.sav").string()),"v6 generated CDO checkpoint restore failed");
    Require(fixture.Snapshot("RoundTrip")==current,"v6 class/default/instance roundtrip was not canonical");
    Same(Cdo("Derived","Payload"),savedPayload,"v6 nested default values were not restored");
    fixture.UnchangedSources();
    Require(InitializePortableRuntime({coreTable,engineTable,classesTable}).passed && LoadPortableRuntimeMap(mapTable).passed,
        "Explicit runtime reinitialization did not recreate generated immutable defaults");
    Same(Cdo("Base","Counter"),Value::Integer(10),"Runtime reinitialization retained old base defaults");
    Same(Cdo("Derived","Counter"),Value::Integer(20),"Runtime reinitialization retained old derived defaults");
    Require(!GetPortableRuntimeScriptStatePresent() && fixture.Snapshot("Reinitialized")==legacy,
        "Runtime reinitialization retained abandoned default/instance state");
    fixture.UnchangedSources();
}
} // namespace
int main() {
    try {
        const auto gcBefore=GC::GetStats().numObjects;
        Synthetic();
        GC::Collect();
        Require(GC::GetStats().numObjects==gcBefore,"Generated class-default test leaked rooted GC objects after shutdown");
        std::cout << "Generated class-default integration: " << checks << " checks, " << refusals
                  << " rejection controls; shared CDO/instance isolation, typed writes, native 283 rollback, codec 3/v6 and legacy 1-5 replacement saves. No Spawn or campaign startup claim.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << "Class-default integration failed: " << error.what() << '\n'; return 1; }
}
