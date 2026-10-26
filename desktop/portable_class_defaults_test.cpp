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
#include <tuple>
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
Code Context(Code selector,Code child,std::uint8_t zero=0u,std::uint8_t opcode=0x12u) {
    Code prefix=Join({Token(opcode),std::move(selector)});
    U16(prefix.raw,static_cast<std::uint16_t>(child.logical));prefix.raw.push_back(zero);prefix.logical+=3u;
    return Join({std::move(prefix),std::move(child)});
}
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
            else if(type=="ClassProperty") { Index(body,0);Index(body,target); }
            return body;
        }; return ref;
    }
    std::int32_t Function(const std::string& name, std::int32_t owner, std::int32_t core, Code code,
        std::uint16_t native=0u,std::uint32_t flags=2u) {
        const auto ref=Export(name,Import("Function",core),0,owner,true);
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,native,flags,code=std::move(code)](std::int32_t next,std::int32_t child) {
            Bytes body{0u}; Index(body,0); Index(body,next); Index(body,0); Index(body,child);
            Index(body,exports.at(static_cast<std::size_t>(ref-1)).ObjName); U32(body,0); U32(body,0);
            U32(body,static_cast<std::uint32_t>(code.logical)); Append(body,code.raw); U16(body,native); body.push_back(0u); U32(body,native ? 0x402u : flags); return body;
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
QuestVr::Vm::Result RefusedCall(const Fixture& fixture,const std::string& actor,const std::string& function,
    const std::vector<Evaluation>& arguments={}) {
    const auto before=fixture.Snapshot("BeforeRefusal");
    const auto result=ExecutePortableActorFunction("DefaultFixture."+actor,function,arguments);
    Require(!result.passed() && !result.committed,"Invalid CDO operation silently committed: "+function);
    Require(fixture.Snapshot("AfterRefusal")==before,"Failed CDO operation changed canonical persistent state: "+function); ++refusals;
    return result;
}
std::uint32_t Read32(const Bytes& bytes,std::size_t& cursor) {
    Require(cursor<=bytes.size() && bytes.size()-cursor>=4u,"Independent checkpoint inspection encountered truncation");
    std::uint32_t value{}; for(unsigned i=0u;i<4u;++i) value|=static_cast<std::uint32_t>(bytes[cursor++])<<(8u*i); return value;
}
std::size_t PrefixSize(const Bytes& bytes) {
    std::size_t cursor{}; Require(Read32(bytes,cursor)==0x53515844u,"Independent runtime magic mismatch");
    const auto version=Read32(bytes,cursor);
    Require((version>=1u && version<=6u)||version==19u,"Independent runtime version mismatch");
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
    const auto blob=QuestVr::EncodeScriptSavedState(script);
    Replace32(bytes,4u,version ? version : blob[6u]==QuestVr::ScriptStateDetail::ScalarFloatBitsVersion ? 19u :
        !script.classDefaults.empty() ? 6u : states ? 5u : 4u);
    U32(bytes,static_cast<std::uint32_t>(blob.size())); Append(bytes,blob); return bytes;
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
    const auto headerClass=core.Property("Class","ClassProperty",object,corePackage,1u,object,2u);
    const auto headerName=core.Property("Name","NameProperty",object,corePackage,1u,0,2u);
    const auto headerOuter=core.Property("Outer","ObjectProperty",object,corePackage,1u,object,2u);
    const auto headerFlags=core.Property("ObjectFlags","IntProperty",object,corePackage,1u,0,2u);
    const auto headerInternal=core.Property("ObjectInternal","IntProperty",object,corePackage,6u,0,2u);
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
    const auto importedCoreObject=classes.Import("Object",classesCore);
    const auto importedHeaderClass=classes.Import("Class",importedCoreObject,"ClassProperty");
    const auto importedHeaderName=classes.Import("Name",importedCoreObject,"NameProperty");
    const auto importedHeaderOuter=classes.Import("Outer",importedCoreObject,"ObjectProperty");
    const auto importedHeaderFlags=classes.Import("ObjectFlags",importedCoreObject,"IntProperty");
    const auto importedHeaderInternal=classes.Import("ObjectInternal",importedCoreObject,"IntProperty");
    Require(headerClass && headerName && headerOuter && headerFlags && headerInternal,"Generated Core.Object header declarations missing");
    const auto base=classes.Export("Base",0,importedActor), derived=classes.Export("Derived",0,base);
    const auto uiDefault=classes.Export("UiDefault",0,importedCoreObject);
    const auto screenType=classes.Property("ScreenType","ByteProperty",uiDefault,classesCore);
    Bytes uiTags;classes.Tag(uiTags,"ScreenType",1u,Bytes{3u});classes.ClassBody(uiDefault,uiTags);
    const auto sibling=classes.Export("Sibling",0,base), child=classes.Export("Child",0,derived);
    const auto foreign=classes.Export("Foreign",0,importedActor);
    const auto foreignCounter=classes.Property("Counter","IntProperty",foreign,classesCore);
    const auto shadow=classes.Export("Shadow",0,base), shadowZero=classes.Export("ShadowZero",0,base);
    const auto shadowCounter=classes.Property("Counter","IntProperty",shadow,classesCore);
    classes.Property("Counter","IntProperty",shadowZero,classesCore);
    const auto counter=classes.Property("Counter","IntProperty",base,classesCore);
    const auto orphan=classes.Property("Orphan","IntProperty",base,classesCore);
    auto& baseChildren=classes.children[base];
    baseChildren.erase(std::remove(baseChildren.begin(),baseChildren.end(),orphan),baseChildren.end());
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
    classes.IntTag(defaults,"Orphan",900);
    for(std::uint8_t i=0u;i<3u;++i) classes.IntTag(defaults,"Slots",3+2*i,i);
    classes.NameTag(defaults,"Label","Baseline"); Index(defaults,classes.Name("Enabled")); defaults.push_back(3u);
    Bytes f; std::uint32_t bits{}; const float baseline=1.25f; std::memcpy(&bits,&baseline,4u); U32(f,bits); classes.Tag(defaults,"Ratio",4u,f);
    classes.ObjectTag(defaults,"Link",0); Bytes members; U32(members,4u); Index(members,0); classes.Tag(defaults,"Payload",10u,members,0u,"Record");
    classes.ClassBody(base,defaults); Bytes overrides; classes.IntTag(overrides,"Counter",20); classes.IntTag(overrides,"Source",12); classes.ClassBody(derived,overrides);
    overrides.clear(); classes.IntTag(overrides,"Counter",30); classes.IntTag(overrides,"Source",13); classes.ClassBody(sibling,overrides); classes.ClassBody(child,{});
    Bytes foreignDefaults;classes.IntTag(foreignDefaults,"Counter",777);classes.ClassBody(foreign,foreignDefaults);
    Bytes shadowDefaults;classes.IntTag(shadowDefaults,"Counter",777);classes.ClassBody(shadow,shadowDefaults);
    classes.ClassBody(shadowZero,{});
    classes.Function("WrongDefaultRead",base,classesCore,Return(Ref(0x02u,foreignCounter)));
    classes.Function("WrongInstanceRead",base,classesCore,Return(Ref(0x01u,foreignCounter)));
    classes.Function("WrongReadAfterWrite",base,classesCore,Join({Assign(Ref(0x02u,counter),Int(321)),Return(Ref(0x02u,foreignCounter))}));
    classes.Function("OrphanDefaultRead",base,classesCore,Return(Ref(0x02u,orphan)));
    classes.Function("OrphanInstanceRead",base,classesCore,Return(Ref(0x01u,orphan)));
    classes.Function("OrphanWriteAfterDefault",base,classesCore,Join({
        Assign(Ref(0x02u,counter),Int(321)),Assign(Ref(0x01u,orphan),Int(123)),Return()}));
    classes.Function("CopyDefault",base,classesCore,Join({Assign(Ref(0x02u,counter),Ref(0x02u,source)),Return()}));
    classes.Function("ReadDefault",base,classesCore,Return(Ref(0x02u,counter)));
    classes.Function("ReadInstanceCounter",base,classesCore,Return(Ref(0x01u,counter)));
    classes.Function("ClassSelectedBase",base,classesCore,Return(Context(Ref(0x20u,base),Ref(0x02u,counter))));
    classes.Function("ClassSelectedDerived",base,classesCore,Return(Context(Ref(0x20u,derived),Ref(0x02u,counter))));
    classes.Function("ClassSelf",base,classesCore,Return(Context(Ref(0x20u,base),Token(0x17u),4u)));
    classes.Function("ClassInstanceField",base,classesCore,Return(Context(Ref(0x20u,base),Ref(0x01u,counter),4u)));
    classes.Function("DefaultHeaderClass",base,classesCore,Return(Context(Ref(0x20u,base),Ref(0x01u,importedHeaderClass),4u)));
    classes.Function("DefaultHeaderName",base,classesCore,Return(Context(Ref(0x20u,base),Ref(0x01u,importedHeaderName),4u)));
    classes.Function("DefaultHeaderOuter",base,classesCore,Return(Context(Ref(0x20u,base),Ref(0x01u,importedHeaderOuter),4u)));
    classes.Function("DefaultHeaderFlags",base,classesCore,Return(Context(Ref(0x20u,base),Ref(0x01u,importedHeaderFlags),4u)));
    classes.Function("DefaultHeaderInternal",base,classesCore,Return(Context(Ref(0x20u,base),Ref(0x01u,importedHeaderInternal),24u)));
    classes.Function("DefaultHeaderWrite",base,classesCore,Join({
        Assign(Context(Ref(0x20u,base),Ref(0x01u,importedHeaderOuter),4u),Token(0x17u)),Return()}));
    classes.Function("NonActorClassRead",base,classesCore,Return(Context(Ref(0x20u,uiDefault),Ref(0x02u,screenType),1u)));
    classes.Function("NonActorClassWrite",base,classesCore,Join({
        Assign(Context(Ref(0x20u,uiDefault),Ref(0x02u,screenType),1u),Int(7)),Return()}));
    classes.Function("NonActorClassWrongObject",base,classesCore,Join({
        Assign(Ref(0x01u,link),Context(Ref(0x20u,uiDefault),Token(0x17u),4u)),Return()}));
    classes.Function("ClassObjectString",base,classesCore,Return(Context(Ref(0x20u,base),
        Join({Token(0x56u),Token(0x17u)}),0u)));
    const auto staticIdentity=classes.Function("StaticSelf",base,classesCore,Return(Token(0x17u)),0u,0x2002u);
    const auto staticDefault=classes.Function("StaticDefault",base,classesCore,Return(Ref(0x02u,counter)),0u,0x2002u);
    const auto staticString=classes.Function("StaticString",base,classesCore,Return(Join({Token(0x56u),Token(0x17u)})),0u,0x2002u);
    const auto ordinaryIdentity=classes.Function("OrdinarySelf",base,classesCore,Return(Token(0x17u)));
    classes.Function("ClassStaticCall",base,classesCore,Return(Context(Ref(0x20u,base),
        Join({Ref(0x1cu,staticIdentity),Token(0x16u)}),4u)));
    classes.Function("ClassStaticDefault",base,classesCore,Return(Context(Ref(0x20u,base),
        Join({Ref(0x1cu,staticDefault),Token(0x16u)}),4u)));
    classes.Function("ClassStaticString",base,classesCore,Return(Context(Ref(0x20u,base),
        Join({Ref(0x1cu,staticString),Token(0x16u)}),0u)));
    classes.Function("ClassOrdinaryCall",base,classesCore,Return(Context(Ref(0x20u,base),
        Join({Ref(0x1cu,ordinaryIdentity),Token(0x16u)}),4u)));
    classes.Function("DefaultObjectDynamicCast",base,classesCore,Return(Join({Ref(0x2eu,base),
        Context(Ref(0x20u,base),Token(0x17u),4u)})));
    classes.Function("DefaultObjectMetaCast",base,classesCore,Return(Join({Ref(0x13u,base),
        Context(Ref(0x20u,base),Token(0x17u),4u)})));
    classes.Function("ClassSlot",base,classesCore,Return(Slot(slots,1)));
    classes.Function("ClassSelectedSlot",base,classesCore,Return(Join({Token(0x1au),Int(1),
        Context(Ref(0x20u,derived),Ref(0x02u,slots),12u)})));
    classes.Function("ClassWrite",base,classesCore,Join({
        Assign(Context(Ref(0x20u,base),Ref(0x02u,counter)),Int(123)),Return()}));
    classes.Function("ClassWriteRollback",base,classesCore,Join({
        Assign(Context(Ref(0x20u,base),Ref(0x02u,counter)),Int(321)),Native(278u),Return()}));
    classes.Function("ClassForeignRead",base,classesCore,Return(Context(Ref(0x20u,base),Ref(0x02u,foreignCounter))));
    classes.Function("ClassNonClass",base,classesCore,Return(Context(Token(0x17u),Ref(0x02u,counter))));
    classes.Function("ClassNative",base,classesCore,Return(Context(Ref(0x20u,base),Native(283u,{Float(1.0f),Float(2.0f)}))));
    classes.Function("ClassStoreObject",base,classesCore,Join({
        Assign(Ref(0x01u,link),Context(Ref(0x20u,base),Token(0x17u),4u)),Return()}));
    classes.Function("SetShadowCounters",shadow,classesCore,Join({
        Assign(Ref(0x02u,shadowCounter),Int(555)),Assign(Ref(0x01u,shadowCounter),Int(888)),Return()}));
    // Both compiled declarations remain legitimate instance slots even when
    // the nearer class reuses the same leaf name. Their defaults stay separate.
    classes.Function("ReadShadowInstanceCounter",shadow,classesCore,Return(Ref(0x01u,shadowCounter)));
    for(const auto& definition:std::vector<std::tuple<std::int32_t,std::int32_t,std::string>>{
        {base,counter,"SetBaseInstanceCounter"},{shadow,shadowCounter,"SetNearestInstanceCounter"}}) {
        const auto& [owner,field,name]=definition;
        const auto function=classes.Function(name,owner,classesCore,{});
        const auto input=classes.Property("Value","IntProperty",function,classesCore,1u,0,0x80u);
        const auto code=Join({Assign(Ref(0x01u,field),Ref(0x00u,input)),Return()});
        classes.builders.at(static_cast<std::size_t>(function-1))=[&classes,function,code](std::int32_t next,std::int32_t first) {
            Bytes body{0u};Index(body,0);Index(body,next);Index(body,0);Index(body,first);
            Index(body,classes.exports.at(static_cast<std::size_t>(function-1)).ObjName);
            U32(body,0);U32(body,0);U32(body,static_cast<std::uint32_t>(code.logical));Append(body,code.raw);
            U16(body,0);body.push_back(0u);U32(body,2u);return body;
        };
    }
    const auto bothInstanceWrites=Join({Assign(Ref(0x01u,counter),Int(211)),Assign(Ref(0x01u,shadowCounter),Int(422))});
    classes.Function("SetBothInstanceCounters",shadow,classesCore,Join({bothInstanceWrites,Return()}));
    const auto failedInstanceWrites=Join({Assign(Ref(0x01u,counter),Int(321)),Assign(Ref(0x01u,shadowCounter),Int(654))});
    classes.Function("BothInstanceThenFail",shadow,classesCore,Join({failedInstanceWrites,Native(999u),Return()}));
    classes.Function("BothInstanceThenForeign",shadow,classesCore,Join({failedInstanceWrites,Return(Ref(0x01u,foreignCounter))}));
    classes.Function("BothInstanceThenOrphan",shadow,classesCore,Join({failedInstanceWrites,Assign(Ref(0x01u,orphan),Int(123)),Return()}));
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
    for(const auto& [name,cls]:std::vector<std::pair<std::string,std::string>>{{"Base0","Base"},{"Base1","Base"},{"Derived0","Derived"},{"Derived1","Derived"},{"Sibling0","Sibling"},{"Child0","Child"},{"Shadow0","Shadow"},{"ShadowZero0","ShadowZero"}}) {
        const auto ref=map.Export(name,map.Import(cls,importedClasses)); Bytes properties;
        if(name=="Base0") map.IntTag(properties,"Counter",42);
        if(name=="Shadow0") map.IntTag(properties,"Counter",999);
        map.ActorBody(ref,properties);
    }
    const auto mapTable=fixture.Write(map,true);
    const auto initialized=InitializePortableRuntime({coreTable,engineTable,classesTable});
    Require(initialized.passed && initialized.functions>=9u,"Generated class-default runtime failed real metadata initialization");
    Require(LoadPortableRuntimeMap(mapTable).passed,"Generated map actors were not loaded");
    Require(!GetPortableRuntimeScriptStatePresent(),"Read-only generated metadata initialization created persistent writes");
    const auto legacy=fixture.Snapshot("Legacy"); Require(legacy.size()>8u && legacy[4u]==3u,"Initial generated snapshot is not legacy v3");
    const auto coldStats=GC::GetStats();
    RefusedCall(fixture,"Base0","ClassWriteRollback");
    GC::Collect();
    Require(GC::GetStats().numObjects==coldStats.numObjects && GC::GetStats().memoryUsage==coldStats.memoryUsage &&
        fixture.Snapshot("ColdClassRollback")==legacy,"First failed ClassContext changed default-handle GC roots or persistent state");
    Same(Call("Derived0","ClassSelectedBase").value,Value::Integer(10),"ClassContext selected derived rather than explicit base defaults");
    Same(Call("Base0","ClassSelectedDerived").value,Value::Integer(20),"ClassContext failed to use actual selected concrete class");
    const auto defaultHandle=Call("Base0","ClassSelf").value;
    Require(defaultHandle.kind==Kind::Object && defaultHandle.text=="@ClassDefault:DefaultClasses.Base",
        "ClassContext returned UClass/map actor instead of distinct class-owned identity");
    Same(Call("Base0","ClassInstanceField").value,Value::Integer(10),"Default-object instance field did not address its copied default block");
    Same(Call("Base0","ClassSelectedSlot").value,Value::Integer(5),"ClassContext inherited fixed-array lvalue lost its concrete class or element binding");
    Same(Call("Base0","DefaultHeaderClass").value,Value::Text(Kind::Object,"DefaultClasses.Base"),"Default-object Class header is not its owning UClass");
    Same(Call("Base0","DefaultHeaderName").value,Value::Text(Kind::Name,"None"),"Initialized default-object Name header is not None");
    Same(Call("Base0","DefaultHeaderOuter").value,Value::Text(Kind::Object,{}),"Initialized default-object Outer header is not null");
    Same(Call("Base0","DefaultHeaderFlags").value,Value::Integer(0),"Initialized default-object flags header is not zero");
    RefusedCall(fixture,"Base0","DefaultHeaderInternal");RefusedCall(fixture,"Base0","DefaultHeaderWrite");
    Same(Call("Base0","NonActorClassRead").value,Value::Byte(3u),"Non-Actor class defaults failed actual Core.Object inheritance");
    Same(Call("Base0","ClassStaticCall").value,defaultHandle,"Static call retargeted class-default Self");
    Same(Call("Base0","ClassOrdinaryCall").value,defaultHandle,"Ordinary script call lost class-default receiver frame");
    Same(Call("Base0","ClassStaticDefault").value,Value::Integer(10),"Static call failed real receiver's default block");
    Same(Call("Base0","ClassStaticString").value,Value::Text(Kind::String,"None"),"Default-block GetPathName did not use actual initialized None/Outer header");
    Same(Call("Base0","ClassObjectString").value,Value::Text(Kind::String,"DefaultFixture.Base0"),
        "ObjectToString direct ClassContext operand failed caller-frame evaluation");
    Same(Call("Base0","DefaultObjectDynamicCast").value,defaultHandle,"Dynamic cast rejected class-default object's actual Class");
    Same(Call("Base0","DefaultObjectMetaCast").value,Value::Text(Kind::Object,{}),"Metacast treated class-default object as actual UClass");
    RefusedCall(fixture,"Base0","NonActorClassWrongObject");
    Call("Base0","NonActorClassWrite");
    Same(ReadPortableClassDefault("DefaultClasses.UiDefault","ScreenType"),Value::Byte(7u),"Non-Actor mutable class default lost typed Byte storage");
    const auto nonActorState=fixture.Snapshot("NonActorClassDefaults");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Non-Actor class default legacy reset failed");
    Require(LoadPortableRuntimeState((fixture.directory/"NonActorClassDefaults.sav").string()) &&
        fixture.Snapshot("NonActorClassRoundTrip")==nonActorState,"Non-Actor class defaults did not restore canonically");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Non-Actor class controls did not reset baseline");
    for(const auto& function:{"ClassWriteRollback","ClassForeignRead","ClassNonClass","ClassNative"})
        RefusedCall(fixture,"Base0",function);
    Call("Derived0","ClassWrite");
    Same(Cdo("Base","Counter"),Value::Integer(123),"ClassContext write missed selected concrete default storage");
    Same(Cdo("Derived","Counter"),Value::Integer(20),"Base default write leaked into derived copied defaults");
    Same(Instance("Base0","Counter"),Value::Integer(42),"ClassContext write altered existing map instance");
    const auto classWritten=fixture.Snapshot("ClassContextWritten");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"ClassContext write legacy reset failed");
    Require(LoadPortableRuntimeState((fixture.directory/"ClassContextWritten.sav").string()) &&
        fixture.Snapshot("ClassContextWriteRestored")==classWritten,"ClassContext default write did not restore canonically");
    Call("Base0","ClassStoreObject");
    Same(Instance("Base0","Link"),defaultHandle,"ObjectProperty rejected or rewrote valid default-object identity");
    const auto withDefaultObject=fixture.Snapshot("ClassDefaultObjectReference");
    const auto references=SavedScript(withDefaultObject);
    Require(references.births.empty() && references.objects.size()==1u && references.objects.front().path=="DefaultFixture.Base0",
        "Default-object reference became a world actor/export/birth checkpoint entry");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Default-object reference reset failed");
    Require(LoadPortableRuntimeState((fixture.directory/"ClassDefaultObjectReference.sav").string()) &&
        fixture.Snapshot("DefaultObjectReferenceRestored")==withDefaultObject,"Default-object reference did not restore canonically");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"ClassContext controls did not restore untouched baseline");
    const auto declaredRevision=GetPortableRuntimeWorldRevision();
    const auto declaredStats=GC::GetStats();
    Same(ReadPortableActorDeclaredScriptProperty("DefaultFixture.Shadow0","DefaultClasses.Base.Counter"),Value::Integer(10),
        "Declared read redirected a compiled base field to the shadowed map tag");
    Same(ReadPortableActorDeclaredScriptProperty("defaultfixture.shadow0","defaultclasses.shadow.counter"),Value::Integer(999),
        "Declared read lost the nearest field's map value or case-insensitive identity");
    Same(ReadPortableActorDeclaredScriptProperty("DefaultFixture.Base0","DefaultClasses.Base.Counter"),Value::Integer(42),
        "Declared read lost an ordinary authored map override");
    const auto refusedDeclared=[&](const std::string& receiver,const std::string& property,
        const std::uint32_t slot,const std::string& expected) {
        std::string error;
        try { static_cast<void>(ReadPortableActorDeclaredScriptProperty(receiver,property,slot)); }
        catch(const std::exception& failure) { error=failure.what(); }
        Require(error==expected,"Declared read did not retain its exact typed refusal: "+error);
        ++refusals;
    };
    for (const auto& property:{"DefaultClasses.Foreign.Counter","DefaultClasses.Base.Orphan","DefaultClasses.Base.PutPayload.Value"})
        refusedDeclared("DefaultFixture.Base0",property,0u,"Declared property is not owned by its concrete receiver class");
    refusedDeclared("DefaultFixture.Base0","DefaultClasses.Base.Counter",1u,"Declared property fixed-array index is out of range");
    refusedDeclared("DefaultFixture.Base0","DefaultClasses.Base.Slots",3u,"Declared property fixed-array index is out of range");
    refusedDeclared("DefaultFixture.Base0","DefaultClasses.Base.ReadDefault",0u,"VM object has no property descriptor");
    refusedDeclared("DefaultClasses.Base","DefaultClasses.Base.Counter",0u,"Declared property receiver is not an active nonretired Engine.Actor");
    for (const auto& invalid:{std::string{},std::string("x\0y",3u),std::string(4097u,'x')}) {
        refusedDeclared(invalid,"DefaultClasses.Base.Counter",0u,"Declared actor property identity is invalid");
        refusedDeclared("DefaultFixture.Base0",invalid,0u,"Declared actor property identity is invalid");
    }
    Require(fixture.Snapshot("DeclaredReadAfter")==legacy && GetPortableRuntimeWorldRevision()==declaredRevision &&
        GC::GetStats().numObjects==declaredStats.numObjects && GC::GetStats().memoryUsage==declaredStats.memoryUsage,
        "Declared reads or typed refusals changed checkpoint, revision, or rooted GC state");
    Same(Cdo("Foreign","Counter"),Value::Integer(777),"Qualified foreign-class read control is not a distinct real default");
    for(const auto& function:{"WrongDefaultRead","WrongInstanceRead","WrongReadAfterWrite",
        "OrphanDefaultRead","OrphanInstanceRead","OrphanWriteAfterDefault"}) {
        const auto refused=RefusedCall(fixture,"Base0",function);
        Require(refused.error=="VM property is not owned by its concrete receiver class",
            "Qualified foreign read failed for an unrelated reason: "+std::string(function)+": "+refused.error);
    }
    Same(Cdo("Base","Counter"),Value::Integer(10),"Rejected qualified read retained its earlier CDO write");
    Same(Instance("Base0","Counter"),Value::Integer(42),"Rejected same-name property read changed actual instance storage");
    Same(Cdo("Shadow","Counter"),Value::Integer(777),"Redeclared default did not select its own tag");
    Same(Cdo("ShadowZero","Counter"),Value::Integer(0),"Redeclared zero default leaked its ancestor's tag");
    Same(Call("Shadow0","ReadDefault").value,Value::Integer(10),"Inherited compiled default reference aliased the redeclared field");
    Same(Call("ShadowZero0","ReadDefault").value,Value::Integer(10),"Inherited compiled read lost the original declaration");
    Same(Instance("Shadow0","Counter"),Value::Integer(999),"Redeclared instance did not select its own map tag");
    Same(Call("Shadow0","ReadInstanceCounter").value,Value::Integer(10),"Inherited compiled instance reference aliased a shadowed map tag");
    Call("Shadow0","SetShadowCounters");
    Same(Cdo("Shadow","Counter"),Value::Integer(555),"Redeclared mutable default lost its own write");
    Same(Instance("Shadow0","Counter"),Value::Integer(888),"Redeclared mutable instance lost its own write");
    Same(Call("Shadow0","ReadDefault").value,Value::Integer(10),"Inherited compiled default read consumed a redeclared mutable overlay");
    Same(Call("Shadow0","ReadInstanceCounter").value,Value::Integer(10),"Inherited compiled instance read consumed a redeclared mutable overlay");
    const auto shadowSaved=fixture.Snapshot("ShadowSaved");
    const auto shadowRevision=GetPortableRuntimeWorldRevision();
    Same(ReadPortableActorDeclaredScriptProperty("DefaultFixture.Shadow0","DefaultClasses.Base.Counter"),Value::Integer(10),
        "Declared base read consumed a shadowed mutable instance/CDO overlay");
    Same(ReadPortableActorDeclaredScriptProperty("DefaultFixture.Shadow0","DefaultClasses.Shadow.Counter"),Value::Integer(888),
        "Declared shadow read lost its actual mutable instance overlay");
    Require(fixture.Snapshot("DeclaredShadowAfter")==shadowSaved && GetPortableRuntimeWorldRevision()==shadowRevision,
        "Declared shadow reads changed checkpoint or revision");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Shadow overlay legacy reset failed");
    Same(Cdo("Shadow","Counter"),Value::Integer(777),"Shadow legacy reset retained mutable defaults");
    Same(Instance("Shadow0","Counter"),Value::Integer(999),"Shadow legacy reset lost authored map tag");
    Require(LoadPortableRuntimeState((fixture.directory/"ShadowSaved.sav").string()) &&
        fixture.Snapshot("ShadowRoundTrip")==shadowSaved,"Shadowed declarations did not round-trip canonically");
    Same(Call("Shadow0","ReadDefault").value,Value::Integer(10),"Restored shadow CDO overlay redirected its inherited field");
    Same(Call("Shadow0","ReadInstanceCounter").value,Value::Integer(10),"Restored shadow instance overlay redirected its inherited field");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Shadow fixture did not restore untouched baseline");
    const auto instanceCounters=[&](const std::int32_t inherited,const std::int32_t nearest) {
        Same(Call("Shadow0","ReadInstanceCounter").value,Value::Integer(inherited),"Compiled base instance Counter lost its declaring slot");
        Same(Call("Shadow0","ReadShadowInstanceCounter").value,Value::Integer(nearest),"Compiled shadow instance Counter lost its declaring slot");
        Same(ReadPortableActorDeclaredScriptProperty("DefaultFixture.Shadow0","DefaultClasses.Base.Counter"),Value::Integer(inherited),
            "Qualified base instance Counter selected the nearer same-name overlay");
        Same(ReadPortableActorDeclaredScriptProperty("defaultfixture.shadow0","defaultclasses.shadow.counter"),Value::Integer(nearest),
            "Qualified shadow instance Counter selected the inherited same-name overlay");
        Same(Instance("Shadow0","Counter"),Value::Integer(nearest),"Ordinary leaf Counter did not select its nearest declaration");
        Same(Cdo("Base","Counter"),Value::Integer(10),"Instance base write leaked into Base CDO");
        Same(Cdo("Shadow","Counter"),Value::Integer(777),"Instance shadow write leaked into Shadow CDO");
        Same(Instance("Base0","Counter"),Value::Integer(42),"Shadow instance write altered an unrelated Base instance");
    };
    instanceCounters(10,999);
    Call("Shadow0","SetBaseInstanceCounter",{{Value::Integer(123),{}}});instanceCounters(123,999);
    Call("Shadow0","SetNearestInstanceCounter",{{Value::Integer(456),{}}});instanceCounters(123,456);
    const auto beforeInstanceFailure=fixture.Snapshot("BeforeInstanceShadowFailure");
    const auto instanceRevision=GetPortableRuntimeWorldRevision();const auto instanceGc=GC::GetStats();
    for(const auto& function:{"BothInstanceThenFail","BothInstanceThenForeign","BothInstanceThenOrphan"}) {
        const auto failure=RefusedCall(fixture,"Shadow0",function);
        Require(failure.writes==2u,"Later shadow failure occurred before both valid declaring-slot writes: "+std::string(function));
        instanceCounters(123,456);
        Require(GetPortableRuntimeWorldRevision()==instanceRevision && GC::GetStats().numObjects==instanceGc.numObjects &&
            GC::GetStats().memoryUsage==instanceGc.memoryUsage,"Shadow later failure retained revision/GC changes");
    }
    QuestVr::Vm::Limits oneWrite;oneWrite.writes=1u;
    const auto budgeted=ExecutePortableActorFunction("DefaultFixture.Shadow0","SetBothInstanceCounters",{},oneWrite);
    Require(budgeted.status==QuestVr::Vm::Status::Budget && !budgeted.committed && budgeted.writes==1u &&
        fixture.Snapshot("AfterInstanceShadowBudget")==beforeInstanceFailure && GetPortableRuntimeWorldRevision()==instanceRevision &&
        GC::GetStats().numObjects==instanceGc.numObjects && GC::GetStats().memoryUsage==instanceGc.memoryUsage,
        "Shadow write budget failed to restore its already-written inherited slot");
    ++refusals;instanceCounters(123,456);
    const auto both=Call("Shadow0","SetBothInstanceCounters");
    Require(both.committed && both.writes==2u,"Independent shadow instance writes were not both charged/committed");
    instanceCounters(211,422);
    const auto instanceShadowSaved=fixture.Snapshot("InstanceShadowSaved");
    const auto instanceShadowGraph=SavedScript(instanceShadowSaved);
    Require(instanceShadowGraph.classDefaults.empty() && instanceShadowGraph.objects.size()==1u &&
        instanceShadowGraph.objects.front().path=="DefaultFixture.Shadow0" &&
        instanceShadowGraph.objects.front().properties.size()==2u,
        "Independent same-name instance slots became defaults or duplicate actor identities");
    const auto& sameNameProperties=instanceShadowGraph.objects.front().properties;
    Require(sameNameProperties[0u].key=="DefaultClasses.Base.Counter" && sameNameProperties[0u].name=="Counter" &&
        sameNameProperties[0u].index==0u && QuestVr::Vm::Equal(sameNameProperties[0u].value,Value::Integer(211)) &&
        sameNameProperties[1u].key=="DefaultClasses.Shadow.Counter" && sameNameProperties[1u].name=="Counter" &&
        sameNameProperties[1u].index==0u && QuestVr::Vm::Equal(sameNameProperties[1u].value,Value::Integer(422)),
        "Same-leaf actor checkpoint did not retain two canonical qualified declaring keys");
    Require(instanceShadowSaved[4u]==4u && QuestVr::EncodeScriptSavedState(instanceShadowGraph)[6u]==1u &&
        ValidatePortableRuntimeState((fixture.directory/"InstanceShadowSaved.sav").string(),"DefaultFixture"),
        "Same-leaf instance slots changed codec/envelope version or failed typed read-only validation");
    Call("Shadow0","SetBaseInstanceCounter",{{Value::Integer(312),{}}});
    Call("Shadow0","SetNearestInstanceCounter",{{Value::Integer(624),{}}});instanceCounters(312,624);
    Require(LoadPortableRuntimeState((fixture.directory/"InstanceShadowSaved.sav").string()) &&
        fixture.Snapshot("InstanceShadowWarmRoundTrip")==instanceShadowSaved,
        "Same-name declaring-slot instance overlays did not round-trip canonically in the warm runtime");
    instanceCounters(211,422);
    for(const bool omitInherited:{false,true}) {
        auto omitted=instanceShadowGraph;auto& properties=omitted.objects.front().properties;
        properties.erase(properties.begin()+static_cast<std::ptrdiff_t>(omitInherited?0u:1u));
        const auto partial=Envelope(instanceShadowSaved,omitted);
        const auto path=fixture.directory/(omitInherited?"OmitInheritedInstance.sav":"OmitNearestInstance.sav");
        WriteBytes(path,partial);
        Require(LoadPortableRuntimeState(path.string()),"Partial same-name actor overlay replacement failed");
        instanceCounters(omitInherited?10:211,omitInherited?422:999);
        Require(LoadPortableRuntimeState((fixture.directory/"InstanceShadowSaved.sav").string()) &&
            fixture.Snapshot("InstanceShadowAfterPartial")==instanceShadowSaved,
            "Restoring both same-name instance slots after an omission was not canonical");
    }
    for(const auto& wrongKey:{"DefaultClasses.Foreign.Counter","DefaultClasses.Base.Orphan"}) {
        RefusedScript(fixture,instanceShadowSaved,[&](auto& graph) {
            graph.objects.front().properties.front().key=wrongKey;
            graph.objects.front().properties.front().name=std::string(wrongKey).substr(std::string(wrongKey).find_last_of('.')+1u);
        },"SameNameInstanceForeignOrOrphan"+std::string(wrongKey));
    }
    Require(InitializePortableRuntime({coreTable,engineTable,classesTable}).passed && LoadPortableRuntimeMap(mapTable).passed &&
        fixture.Snapshot("InstanceShadowColdBaseline")==legacy,
        "Cold shadow instance restart retained previous hidden/nearest patches");
    instanceCounters(10,999);
    Require(LoadPortableRuntimeState((fixture.directory/"InstanceShadowSaved.sav").string()) &&
        fixture.Snapshot("InstanceShadowColdRoundTrip")==instanceShadowSaved,
        "Cold runtime could not restore both same-leaf qualified instance values");
    instanceCounters(211,422);
    const auto instanceRestoredGc=GC::GetStats();GC::Collect();
    Require(GC::GetStats().numObjects==instanceRestoredGc.numObjects && GC::GetStats().memoryUsage==instanceRestoredGc.memoryUsage,
        "Same-name qualified instance restoration retained unrooted GC garbage");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()) && fixture.Snapshot("AfterInstanceShadowControls")==legacy,
        "Same-name instance controls failed to restore untouched CDO/instance baseline");
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
    Call("Base1","ClearState");
    Require(fixture.Snapshot("FramelessClearState")==nativeOnly,
        "Actual GotoState(None) fabricated a frame on a frameless actor");
    // Retain a generated legacy v5 payload solely to exercise old-checkpoint
    // replacement below. Native GotoState must not author this ambiguous frame.
    auto legacyState=nativeScript;
    legacyState.objects[0].state.emplace();
    legacyState.objects[0].state->frameOverride=true;
    legacyState.objects[0].state->frame.emplace();
    const auto stateOnly=Envelope(nativeOnly,legacyState,5u);
    Require(SavedScript(stateOnly).objects[0].state.has_value(),
        "Generated legacy v5 compatibility fixture lost its frame");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Native/state fixture cleanup failed");
    Same(Instance("Base1","CollisionRadius"),Value::Float(12.5f),"Legacy baseline restore retained native radius");
    Same(Cdo("Base","Counter"),Value::Integer(10),"Base default mismatch"); Same(Cdo("Derived","Counter"),Value::Integer(20),"Derived default mismatch");
    Call("Base0","CopyDefault"); Same(Cdo("Base","Counter"),Value::Integer(7),"Original-style default assignment did not alter shared concrete CDO");
    Same(Call("Base1","ReadDefault").value,Value::Integer(7),"Second existing actor did not read same class default");
    Same(Instance("Base0","Counter"),Value::Integer(42),"Explicit instance override was changed by CDO write"); Same(Instance("Base1","Counter"),Value::Integer(10),"Existing inherited instance changed retroactively");
    Same(ReadPortableActorDeclaredScriptProperty("DefaultFixture.Base1","DefaultClasses.Base.Counter"),Value::Integer(10),
        "Declared existing instance read retroactively inherited a mutable CDO write");
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
    // Active scalar Float values now opt into codec14/envelope19. Preserve
    // infinity and NaN payload words; this is not permission for nonfinite
    // Vector/native collision data or for relabeling an old finite format.
    for(const auto word:{0x7f800000u,0xff800000u,0x7fc12345u,0xff812345u}) {
        float raw{};std::memcpy(&raw,&word,sizeof(word));auto ieee=canonical;
        DefaultProperty(ieee,"Derived","Ratio").value=Value::Float(raw);
        const auto encoded=QuestVr::EncodeScriptSavedState(ieee);
        auto decoded=QuestVr::DecodeScriptSavedState(encoded);
        const auto& value=DefaultProperty(decoded,"Derived","Ratio").value;
        std::uint32_t restored{};std::memcpy(&restored,&value.floating,sizeof(restored));
        Require(encoded[6u]==14u&&value.kind==Kind::Float&&restored==word&&
            QuestVr::EncodeScriptSavedState(decoded)==encoded,
            "Scalar IEEE CDO codec14 lost infinity/NaN sign/payload or canonical roundtrip");
        DefaultProperty(ieee,"Derived","Ratio").value=Value::Float(2.5f);
        Require(QuestVr::EncodeScriptSavedState(ieee)==blob,
            "Removing scalar IEEE CDO data failed to restore exact finite codec3 bytes");
    }
    auto ieee=canonical;DefaultProperty(ieee,"Derived","Ratio").value=Value::Float(std::numeric_limits<float>::infinity());
    const auto ieeeBytes=Envelope(current,ieee);const auto ieeePath=fixture.directory/"IeeeDefaults.sav";WriteBytes(ieeePath,ieeeBytes);
    Require(ieeeBytes[4u]==19u&&ValidatePortableRuntimeState(ieeePath.string(),"DefaultFixture"),
        "Scalar infinity CDO codec14/envelope19 did not pass typed nonmutating validation");
    Require(fixture.Snapshot("AfterIeeeValidation")==current,"Scalar IEEE checkpoint validation published CDO changes");
    RefusedCheckpoint(fixture,Envelope(current,ieee,6u),"ScalarIeeeCodec14UnderV6");
    Require(LoadPortableRuntimeState(ieeePath.string()),"Scalar infinity CDO envelope19 failed actual runtime restore");
    const auto restoredIeee=Cdo("Derived","Ratio");std::uint32_t restoredIeeeWord{};
    std::memcpy(&restoredIeeeWord,&restoredIeee.floating,sizeof(restoredIeeeWord));
    Require(restoredIeee.kind==Kind::Float&&restoredIeeeWord==0x7f800000u&&
        fixture.Snapshot("IeeeDefaultsRoundTrip")==ieeeBytes,
        "Actual CDO infinity restore/save lost exact word, codec14 or envelope19");
    Same(Instance("Derived1","Ratio"),Value::Float(1.25f),"Restoring IEEE CDO changed an existing instance's frozen inherited Float");
    Require(LoadPortableRuntimeState((fixture.directory/"Current.sav").string())&&
        fixture.Snapshot("FiniteDefaultsAfterIeee")==current,
        "Finite CDO replacement retained IEEE values or changed legacy codec3/v6 bytes");
    RefusedCodec([&] {auto bad=canonical;auto vector=Value::Vector({0.0f,0.0f,0.0f});
        vector.vector[0]=std::numeric_limits<float>::infinity();DefaultProperty(bad,"Derived","Ratio").value=vector;
        static_cast<void>(QuestVr::EncodeScriptSavedState(bad));},"NonFiniteDefaultVector");
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
    Require(LoadPortableRuntimeState((fixture.directory/"ClassDefaultObjectReference.sav").string()) &&
        fixture.Snapshot("ColdDefaultObjectReference")==withDefaultObject,
        "Cold runtime restore could not resolve class-owned default-object reference");
    Same(Instance("Base0","Link"),defaultHandle,"Cold restore changed default-object symbolic identity");
    RefusedScript(fixture,withDefaultObject,[](auto& state) {
        auto& actor=state.objects.front();actor.path="@ClassDefault:DefaultClasses.Base";
    },"DefaultHandleAsWorldActor");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Cold default-object controls failed baseline reset");
    Require(LoadPortableRuntimeMap(mapTable).passed,"Clean map replacement failed after class-default handle use");
    Same(Call("Base0","ClassSelf").value,defaultHandle,"Map replacement invalidated class-owned default handle identity");
    fixture.UnchangedSources();
}
void MalformedAncestry() {
    // Use genuine serialized ObjBase/SuperField links. A field on the first
    // class must not permit an early successful read that hides a later cycle
    // or ancestry exceeding the fixed 128-class runtime bound.
    for (unsigned mode=0u;mode<3u;++mode) {
        Fixture fixture;
        Package core; core.stem="Core";
        const auto object=core.Export("Object"); core.ClassBody(object,{});
        const auto coreTable=fixture.Write(core);
        Package engine; engine.stem="Engine";
        const auto engineCore=engine.ImportPackage("Core");
        const auto actor=engine.Export("Actor",0,engine.Import("Object",engineCore));
        engine.Property("Counter","IntProperty",actor,engineCore);
        Bytes baseline; engine.IntTag(baseline,"Counter",10); engine.ClassBody(actor,baseline);
        std::string rejected="Engine.Actor";
        if (mode==0u) {
            engine.exports.at(static_cast<std::size_t>(actor-1)).ObjBase=actor;
        } else if (mode==1u) {
            const auto other=engine.Export("CycleMate",0,actor);
            engine.exports.at(static_cast<std::size_t>(actor-1)).ObjBase=other;
            engine.ClassBody(other,{});
        } else {
            auto parent=actor;
            for (unsigned index=0u;index<127u;++index) {
                const auto cls=engine.Export("Deep"+std::to_string(index),0,parent);
                Bytes defaults;
                if (index>=125u) {
                    engine.Property("Counter","IntProperty",cls,engineCore);
                    engine.IntTag(defaults,"Counter",static_cast<std::int32_t>(index));
                }
                engine.ClassBody(cls,std::move(defaults)); parent=cls;
            }
            rejected="Engine.Deep126"; // 127 deep classes + Actor + Object.
        }
        const auto engineTable=fixture.Write(engine);
        InitializePortableRuntime(std::vector<PortablePackageTables>{coreTable,engineTable});
        const auto before=fixture.Snapshot("AncestryBefore");
        const auto revision=GetPortableRuntimeWorldRevision();
        const auto stats=GC::GetStats();
        if (mode==2u) {
            Same(ReadPortableClassDefault("Engine.Deep125","Counter"),Value::Integer(125),
                "Exact 128-class ancestry was incorrectly refused");
            Same(ReadPortableClassDefault("Engine.Deep124","Counter"),Value::Integer(10),
                "Bounded ancestry changed ordinary inherited field selection");
        }
        for (const auto& name:{"Counter","MissingProperty"}) {
            std::string error;
            try { static_cast<void>(ReadPortableClassDefault(rejected,name)); }
            catch(const std::exception& failure) { error=failure.what(); }
            Require(error=="Runtime class hierarchy cycles or exceeds its native work bound",
                "Malformed ancestry was not refused explicitly before field lookup: "+rejected+'.'+name+": "+error);
            ++refusals;
        }
        Require(fixture.Snapshot("AncestryAfter")==before && GetPortableRuntimeWorldRevision()==revision &&
            GC::GetStats().numObjects==stats.numObjects && GC::GetStats().memoryUsage==stats.memoryUsage,
            "Malformed ancestry refusal changed checkpoint, world revision, or rooted GC state");
        fixture.UnchangedSources();
    }
}
} // namespace
int main(int argc,char** argv) {
    try {
        Require(argc==1 || (argc==2 && std::string(argv[1])=="--case-order"),
            "Usage: portable_class_defaults_test [--case-order]");
        if (argc==2) {
            // Fresh-process FName insertion order must not change fully
            // qualified reflected type identity. All normal/shadow/foreign
            // controls below run unchanged against this mixed display casing.
            static_cast<void>(NameString("eNgInE"));
            static_cast<void>(NameString("aCtOr"));
            static_cast<void>(NameString("mOvEr"));
            static_cast<void>(NameString("iNvEnToRy"));
        }
        const auto gcBefore=GC::GetStats().numObjects;
        Synthetic();
        if(argc==1) MalformedAncestry();
        GC::Collect();
        Require(GC::GetStats().numObjects==gcBefore,"Generated class-default test leaked rooted GC objects after shutdown");
        std::cout << "Generated class-default integration: " << checks << " checks, " << refusals
                  << " rejection controls; shared CDO/instance isolation, typed writes, native 283 rollback, codec 3/v6 and legacy 1-5 replacement saves. No Spawn or campaign startup claim.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << "Class-default integration failed: " << error.what() << '\n'; return 1; }
}
