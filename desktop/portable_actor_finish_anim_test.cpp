#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "quest_script_state.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Generated real v68 packages exercise the production loader, VM, native261,
// frame/state poll and save schema. No substitute Host or commercial data.
namespace {
using Bytes=std::vector<std::uint8_t>;
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
using Evaluation=QuestVr::Vm::Evaluation;
using Status=QuestVr::Vm::Status;
std::size_t checks{},refusals{};
void Require(bool good,const std::string& why) {++checks;if(!good)throw std::runtime_error(why);}
void U16(Bytes& b,std::uint16_t n) {b.push_back(static_cast<std::uint8_t>(n));b.push_back(static_cast<std::uint8_t>(n>>8u));}
void U32(Bytes& b,std::uint32_t n) {U16(b,static_cast<std::uint16_t>(n));U16(b,static_cast<std::uint16_t>(n>>16u));}
void U64(Bytes& b,std::uint64_t n) {U32(b,static_cast<std::uint32_t>(n));U32(b,static_cast<std::uint32_t>(n>>32u));}
void Float(Bytes& b,float n) {std::uint32_t bits{};std::memcpy(&bits,&n,4u);U32(b,bits);}
void Index(Bytes& b,std::int32_t n) {
    auto rest=static_cast<std::uint32_t>(n<0?-static_cast<std::int64_t>(n):n);
    auto first=static_cast<std::uint8_t>((rest&63u)|(n<0?128u:0u));rest>>=6u;if(rest)first|=64u;b.push_back(first);
    while(rest){auto next=static_cast<std::uint8_t>(rest&127u);rest>>=7u;if(rest)next|=128u;b.push_back(next);}
}
void Patch32(Bytes& b,std::size_t at,std::uint32_t n) {
    Require(at<=b.size()&&b.size()-at>=4u,"Generated fixture UInt32 escaped payload");
    for(unsigned i=0u;i<4u;++i)b[at+i]=static_cast<std::uint8_t>(n>>(i*8u));
}
void Append(Bytes& b,const Bytes& tail) {b.insert(b.end(),tail.begin(),tail.end());}
struct Code {Bytes raw;std::size_t logical{};};
Code Token(std::uint8_t op) {return {{op},1u};}
Code Ref(std::uint8_t op,std::int32_t ref) {Code c{{op},5u};Index(c.raw,ref);return c;}
Code Int(std::int32_t n) {Code c{{0x1du},5u};U32(c.raw,static_cast<std::uint32_t>(n));return c;}
Code Real(float n) {Code c{{0x1eu},5u};Float(c.raw,n);return c;}
Code Join(std::initializer_list<Code> parts) {Code c;for(const auto& p:parts){Append(c.raw,p.raw);c.logical+=p.logical;}return c;}
Code Assign(Code left,Code right) {return Join({Token(0x0fu),std::move(left),std::move(right)});}
Code Return(Code value=Token(0x0bu)) {return Join({Token(0x04u),std::move(value)});}
Code Native(std::uint16_t index,std::initializer_list<Code> args={}) {
    Code c;
    if(index>=256u){c.raw={static_cast<std::uint8_t>(0x60u+(index>>8u)),static_cast<std::uint8_t>(index)};c.logical=2u;}
    else c=Token(static_cast<std::uint8_t>(index));
    for(const auto& a:args){Append(c.raw,a.raw);c.logical+=a.logical;}return Join({c,Token(0x16u)});
}
Code Final(std::int32_t fn,std::initializer_list<Code> args={}) {
    Code c=Ref(0x1cu,fn);for(const auto& a:args){Append(c.raw,a.raw);c.logical+=a.logical;}return Join({c,Token(0x16u)});
}
struct Package {
    std::string stem;std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports;std::vector<ExportTableEntry> exports;
    std::vector<std::function<Bytes(std::int32_t,std::int32_t)>> builders;
    std::map<std::int32_t,std::vector<std::int32_t>> children;
    std::int32_t Name(const std::string& name) {
        const auto it=std::find(names.begin(),names.end(),name);if(it!=names.end())return static_cast<std::int32_t>(it-names.begin());
        names.push_back(name);return static_cast<std::int32_t>(names.size()-1u);
    }
    std::int32_t Import(const std::string& name,std::int32_t outer,const std::string& type="Class") {
        imports.push_back({Name("Core"),Name(type),outer,Name(name)});return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) {return Import(name,0,"Package");}
    std::int32_t Export(const std::string& name,std::int32_t cls=0,std::int32_t base=0,std::int32_t outer=0,bool field=false) {
        exports.push_back({cls,base,outer,Name(name),ObjectFlags{},0,-1});builders.emplace_back();
        const auto ref=static_cast<std::int32_t>(exports.size());if(field)children[outer].push_back(ref);return ref;
    }
    void Body(std::int32_t ref,Bytes body) {builders.at(static_cast<std::size_t>(ref-1))=[body=std::move(body)](auto,auto){return body;};}
    void ClassBody(std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref](std::int32_t next,std::int32_t first) {
            const auto& e=exports.at(static_cast<std::size_t>(ref-1));Bytes b;
            Index(b,e.ObjBase);Index(b,next);Index(b,0);Index(b,first);Index(b,e.ObjName);
            U32(b,0);U32(b,0);U32(b,0);U64(b,~std::uint64_t{});U64(b,~std::uint64_t{});U16(b,0xffffu);U32(b,0);
            U32(b,0);b.resize(b.size()+16u,0u);for(unsigned i=0u;i<5u;++i)Index(b,0);return b;
        };
    }
    void StructBody(std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);
            U32(b,0);U32(b,0);U32(b,0);return b;
        };
    }
    std::int32_t Property(const std::string& name,const std::string& type,std::int32_t owner,
        std::int32_t core,std::int32_t target=0,std::uint32_t flags=0u,std::uint32_t dim=1u) {
        const auto ref=Export(name,Import(type,core),0,owner,true);
        builders.at(static_cast<std::size_t>(ref-1))=[type,target,flags,dim](std::int32_t next,std::int32_t) {
            Bytes b{0u};Index(b,0);Index(b,next);U32(b,dim);U32(b,flags);Index(b,0);if(flags&0x20u)U16(b,0);
            if(type=="ObjectProperty"||type=="ByteProperty"||type=="StructProperty")Index(b,target);
            if(type=="ClassProperty"){Index(b,target);Index(b,0);}return b;
        };return ref;
    }
    std::int32_t Function(const std::string& name,std::int32_t owner,std::int32_t core,Code code={},
        std::uint16_t native=0u,std::uint32_t flags=2u) {
        const auto ref=Export(name,Import("Function",core),0,owner,true);FunctionBody(ref,std::move(code),native,flags);return ref;
    }
    void FunctionBody(std::int32_t ref,Code code,std::uint16_t native=0u,std::uint32_t flags=2u) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,code=std::move(code),native,flags](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);
            U32(b,0);U32(b,0);U32(b,static_cast<std::uint32_t>(code.logical));Append(b,code.raw);
            U16(b,native);b.push_back(0u);U32(b,flags);return b;
        };
    }
    std::int32_t State(const std::string& name,std::int32_t owner,std::int32_t core,Code prefix) {
        const auto ref=Export(name,Import("State",core),0,owner,true);
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,prefix=std::move(prefix)](std::int32_t next,std::int32_t first) {
            auto code=Join({prefix,Token(0x08u)});Require(code.logical<65535u,"Generated state exceeds label offset bound");
            const auto labels=static_cast<std::uint16_t>(code.logical);code.raw.push_back(0x0cu);
            Index(code.raw,Name("Begin"));U32(code.raw,0u);Index(code.raw,0);U32(code.raw,0xffffffffu);code.logical+=17u;
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);
            U32(b,0);U32(b,0);U32(b,static_cast<std::uint32_t>(code.logical));Append(b,code.raw);
            U64(b,~std::uint64_t{});U64(b,~std::uint64_t{});U16(b,labels);U32(b,0);return b;
        };return ref;
    }
    void Tag(Bytes& b,const std::string& name,std::uint8_t type,Bytes payload) {
        Require(payload.size()<=255u,"Generated tagged property exceeds small fixture bound");
        Index(b,Name(name));b.push_back(static_cast<std::uint8_t>(type|0x50u));b.push_back(static_cast<std::uint8_t>(payload.size()));Append(b,payload);
    }
    void ObjectTag(Bytes& b,const std::string& name,std::int32_t ref) {Bytes p;Index(p,ref);Tag(b,name,5u,std::move(p));}
    void ByteTag(Bytes& b,const std::string& name,std::uint8_t n) {Tag(b,name,1u,{n});}
    void ActorBody(std::int32_t ref,Bytes b) {Index(b,0);Body(ref,std::move(b));}
    Bytes Serialize() {
        std::map<std::int32_t,std::int32_t> next;
        for(const auto& [owner,list]:children){static_cast<void>(owner);for(std::size_t i=1u;i<list.size();++i)next[list[i-1u]]=list[i];}
        Bytes b;U32(b,0x9e2a83c1u);U16(b,68u);U16(b,0);b.resize(56u,0u);
        std::vector<Bytes> bodies;std::vector<std::int32_t> offsets;
        for(std::size_t i=0u;i<exports.size();++i){const auto ref=static_cast<std::int32_t>(i+1u);const auto it=children.find(ref);
            auto body=builders[i]?builders[i](next[ref],it==children.end()||it->second.empty()?0:it->second.front()):Bytes{};
            offsets.push_back(static_cast<std::int32_t>(b.size()));Append(b,body);bodies.push_back(std::move(body));}
        Patch32(b,12u,static_cast<std::uint32_t>(names.size()));Patch32(b,16u,static_cast<std::uint32_t>(b.size()));
        for(const auto& name:names){Index(b,static_cast<std::int32_t>(name.size()+1u));b.insert(b.end(),name.begin(),name.end());b.push_back(0u);U32(b,0);}
        Patch32(b,20u,static_cast<std::uint32_t>(exports.size()));Patch32(b,24u,static_cast<std::uint32_t>(b.size()));
        for(std::size_t i=0u;i<exports.size();++i){const auto& e=exports[i];Index(b,e.ObjClass);Index(b,e.ObjBase);U32(b,static_cast<std::uint32_t>(e.ObjOuter));Index(b,e.ObjName);U32(b,static_cast<std::uint32_t>(e.ObjFlags));Index(b,static_cast<std::int32_t>(bodies[i].size()));if(!bodies[i].empty())Index(b,offsets[i]);}
        Patch32(b,28u,static_cast<std::uint32_t>(imports.size()));Patch32(b,32u,static_cast<std::uint32_t>(b.size()));
        for(const auto& i:imports){Index(b,i.ClassPackage);Index(b,i.ClassName);U32(b,static_cast<std::uint32_t>(i.ObjOuter));Index(b,i.ObjName);}return b;
    }
};
Bytes ReadBytes(const std::filesystem::path& path) {
    Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read generated FinishAnim package/save");return bytes;
}
void WriteBytes(const std::filesystem::path& path,const Bytes& bytes) {Require(QuestVr::WriteDurableSaveFile(path.string(),bytes),"Cannot write generated FinishAnim package/save");}
struct Fixture {
    std::filesystem::path parent,directory;std::map<std::filesystem::path,Bytes> sources;Bytes baseline;
    Fixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i){const auto path=parent/("quest-finish-anim-"+std::to_string(stamp)+'-'+std::to_string(i));if(std::filesystem::create_directory(path)){directory=std::filesystem::canonical(path);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"FinishAnim fixture escaped private temporary parent");
        std::filesystem::create_directory(directory/"System");std::filesystem::create_directory(directory/"Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime();std::error_code error;const auto actual=std::filesystem::weakly_canonical(directory,error);
        if(!error&&!directory.empty()&&actual==directory&&actual.parent_path()==parent&&actual.filename().string().rfind("quest-finish-anim-",0u)==0u)std::filesystem::remove_all(actual,error);
    }
    PortablePackageTables Write(Package& package,bool map=false) {
        const auto path=directory/(map?"Maps":"System")/(package.stem+(map?".dx":".u"));auto bytes=package.Serialize();WriteBytes(path,bytes);sources.emplace(path,bytes);return LoadPortablePackageTables(path.string());
    }
    Bytes Save(const std::string& label) const {const auto path=directory/(label+".sav");Require(SavePortableRuntimeState(path.string()),"Cannot capture complete FinishAnim checkpoint");return ReadBytes(path);}
    void Reset() const {Require(LoadPortableRuntimeState((directory/"Baseline.sav").string())&&Save("Reset")==baseline,"FinishAnim baseline reset retained changes");}
    void UnchangedSources() const {for(const auto& [path,bytes]:sources)Require(ReadBytes(path)==bytes,"FinishAnim execution modified generated source package");}
};
std::uint32_t Read32(const Bytes& bytes,std::size_t& cursor) {
    Require(cursor<=bytes.size()&&bytes.size()-cursor>=4u,"FinishAnim checkpoint UInt32 truncated");std::uint32_t n{};
    for(unsigned i=0u;i<4u;++i)n|=std::uint32_t{bytes[cursor++]}<<(i*8u);
    return n;
}
QuestVr::ScriptSavedState Decode(const Bytes& bytes,std::size_t* prefixSize=nullptr) {
    std::size_t cursor{};Require(Read32(bytes,cursor)==0x53515844u,"FinishAnim checkpoint magic mismatch");
    const auto version=Read32(bytes,cursor);Require(version>=4u&&version<=15u,"FinishAnim scripted checkpoint version mismatch");
    const auto text=[&] {const auto size=Read32(bytes,cursor);Require(size<=bytes.size()-cursor,"FinishAnim prefix text exceeds payload");cursor+=size;};
    const auto list=[&] {const auto count=Read32(bytes,cursor);Require(count<=100'000u,"FinishAnim prefix list exceeds bound");for(std::uint32_t i=0u;i<count;++i)text();};
    list();list();list();static_cast<void>(Read32(bytes,cursor));const auto damaged=Read32(bytes,cursor);
    Require(damaged<=100'000u,"FinishAnim damaged prefix exceeds bound");for(std::uint32_t i=0u;i<damaged;++i){text();static_cast<void>(Read32(bytes,cursor));}
    static_cast<void>(Read32(bytes,cursor));static_cast<void>(Read32(bytes,cursor));for(unsigned i=0u;i<4u;++i)list();
    if(prefixSize)*prefixSize=cursor;
    const auto size=Read32(bytes,cursor);Require(size==bytes.size()-cursor,"FinishAnim script trailer length mismatch");
    return QuestVr::DecodeScriptSavedState(Bytes(bytes.begin()+static_cast<std::ptrdiff_t>(cursor),bytes.end()));
}
Bytes Envelope(const Bytes& source,const QuestVr::ScriptSavedState& state) {
    std::size_t prefix{};static_cast<void>(Decode(source,&prefix));Bytes bytes(source.begin(),source.begin()+static_cast<std::ptrdiff_t>(prefix));
    const auto blob=QuestVr::EncodeScriptSavedState(state);Patch32(bytes,4u,3u+blob.at(6u));U32(bytes,static_cast<std::uint32_t>(blob.size()));Append(bytes,blob);return bytes;
}
const QuestVr::ScriptSavedObject& Record(const QuestVr::ScriptSavedState& state,const std::string& path) {
    const auto it=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& object){return object.path==path;});
    Require(it!=state.objects.end(),"FinishAnim checkpoint missing real actor record "+path);return *it;
}
std::string Path(const std::string& actor="Driver") {return "FinishFixture."+actor;}
Evaluation Name(const std::string& value) {return {Value::Text(Kind::Name,value),{}};}
Evaluation Number(float value) {return {Value::Float(value),{}};}
Evaluation Boolean(bool value) {return {Value::Bool(value),{}};}
QuestVr::Vm::Result Call(const std::string& function,const std::vector<Evaluation>& args={},const std::string& actor="Driver") {
    const auto result=ExecutePortableActorFunction(Path(actor),function,args);
    Require(result.passed()&&result.committed,"FinishAnim fixture "+function+" failed: "+result.error);return result;
}
Value Read(const char* name,const std::string& actor="Driver") {return ReadPortableActorScriptProperty(Path(actor),name);}
void Equal(const char* name,const Value& expected,const std::string& label) {Require(QuestVr::Vm::Equal(Read(name),expected),label+" "+name);}
QuestVr::StateFrame Frame(const std::string& actor="Driver") {
    const auto state=ReadPortableActorStateObject(Path(actor));
    Require(state&&state->hasStack&&state->frameOverride&&state->frame&&state->frame->control,"FinishAnim lacks real modern state frame");return *state->frame;
}
struct Tables {std::vector<PortablePackageTables> scripts;PortablePackageTables map;};
Tables Build(Fixture& fixture,unsigned malformed=0u) {
    Package core;core.stem="Core";const auto cp=core.ImportPackage("Core"),object=core.Export("Object"),subsystem=core.Export("Subsystem",0,object);
    for(const auto ref:{object,subsystem})core.exports.at(static_cast<std::size_t>(ref-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);
    // Core.Class comes ONLY from the production metadata registration, never
    // a serialized same-name Class shim that could fake lookup prerequisites.
    core.Property("Class","ClassProperty",object,cp,core.Import("Class",cp));core.Property("Name","NameProperty",object,cp);
    core.Property("Outer","ObjectProperty",object,cp,object);core.Property("ObjectFlags","IntProperty",object,cp);
    const auto vector=core.Export("Vector",core.Import("Struct",cp),0,object),rotator=core.Export("Rotator",core.Import("Struct",cp),0,object);
    for(const auto* name:{"X","Y","Z"})core.Property(name,"FloatProperty",vector,cp);
    for(const auto* name:{"Pitch","Yaw","Roll"})core.Property(name,"IntProperty",rotator,cp);
    core.ClassBody(object);core.ClassBody(subsystem);core.StructBody(vector);core.StructBody(rotator);const auto coreTable=fixture.Write(core);
    Package engine;engine.stem="Engine";const auto ec=engine.ImportPackage("Core"),ep=engine.ImportPackage("Engine"),eo=engine.Import("Object",ec),ev=engine.Import("Vector",eo,"Struct"),er=engine.Import("Rotator",eo,"Struct");
    const auto actor=engine.Export("Actor",0,eo),zone=engine.Export("ZoneInfo",0,actor),info=engine.Export("LevelInfo",0,zone),level=engine.Export("Level",0,eo),model=engine.Export("Model",0,eo);
    const auto baseEngine=engine.Export("Engine",0,engine.Import("Subsystem",ec));
    for(const auto ref:{actor,baseEngine})engine.exports.at(static_cast<std::size_t>(ref-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);
    engine.Property("Cylinder","ObjectProperty",baseEngine,ec,engine.Import("Primitive",ep));
    const auto region=engine.Export("PointRegion",engine.Import("Struct",ec),0,actor);
    engine.Property("Zone","ObjectProperty",region,ec,zone);engine.Property("iLeaf","IntProperty",region,ec);engine.Property("ZoneNumber","ByteProperty",region,ec);engine.StructBody(region);
    engine.Property("Level","ObjectProperty",actor,ec,info,2u);engine.Property("XLevel","ObjectProperty",actor,ec,level);
    engine.Property("Location","StructProperty",actor,ec,ev);engine.Property("Rotation","StructProperty",actor,ec,er);
    engine.Property("Region","StructProperty",actor,ec,region);engine.Property("OldLocation","StructProperty",actor,ec,ev);
    engine.Property("Owner","ObjectProperty",actor,ec,actor,0x22u);engine.Property("Base","ObjectProperty",actor,ec,actor,0x22u);engine.Property("StandingCount","ByteProperty",actor,ec,0,2u);
    engine.Property("Brush","ObjectProperty",actor,ec,actor);engine.Property("Touching","ObjectProperty",actor,ec,actor,0u,4u);
    for(const auto* name:{"bTicked","bNoDelete","bStatic","bCollideActors","bCollideWorld","bCollideWhenPlacing","bBlockActors","bBlockPlayers"})engine.Property(name,"BoolProperty",actor,ec);
    for(const auto* name:{"CollisionRadius","CollisionHeight"})engine.Property(name,"FloatProperty",actor,ec);
    engine.Property("bDeleteMe","BoolProperty",actor,ec,0,2u);engine.Property("Role","ByteProperty",actor,ec,0,0x20u);engine.Property("bBegunPlay","BoolProperty",info,ec);
    const auto sequence=engine.Property("AnimSequence","NameProperty",actor,ec,0,0x21u);
    const auto frame=engine.Property("AnimFrame","FloatProperty",actor,ec,0,malformed==5u?0u:1u);
    const auto rate=engine.Property("AnimRate","FloatProperty",actor,ec,0,1u),tween=engine.Property("TweenRate","FloatProperty",actor,ec);
    const auto last=engine.Property("AnimLast","FloatProperty",actor,ec);
    const auto loop=engine.Property("bAnimLoop","BoolProperty",actor,ec),finished=engine.Property("bAnimFinished","BoolProperty",actor,ec);
    const auto notify=engine.Property("bAnimNotify","BoolProperty",actor,ec,0,0x20u);
    const auto finish=engine.Function("FinishAnim",actor,ec,malformed==3u?Return():Code{},malformed==2u?262u:261u,malformed==1u?0x401u:0x409u);
    if(malformed==4u)engine.Property("Bogus","IntProperty",finish,ec,0,0x80u);
    const auto go=engine.Function("GotoState",actor,ec,{},113u,0x401u);
    engine.Property("NewState","NameProperty",go,ec,0,0x90u);engine.Property("Label","NameProperty",go,ec,0,0x90u);
    const auto sleep=engine.Function("Sleep",actor,ec,{},256u,0x409u);engine.Property("Seconds","FloatProperty",sleep,ec,0,0x80u);
    const auto makeSetter=[&](const char* name,const char* type,std::int32_t field) {
        const auto fn=engine.Function(name,actor,ec),input=engine.Property("Input",type,fn,ec,0,0x80u);
        engine.FunctionBody(fn,Join({Assign(Ref(0x01u,field),Ref(0x00u,input)),Return()}));
    };
    makeSetter("SetSequence","NameProperty",sequence);makeSetter("SetFrame","FloatProperty",frame);
    makeSetter("SetRate","FloatProperty",rate);makeSetter("SetTween","FloatProperty",tween);makeSetter("SetLast","FloatProperty",last);
    makeSetter("SetLoop","BoolProperty",loop);makeSetter("SetFinished","BoolProperty",finished);makeSetter("SetNotify","BoolProperty",notify);
    engine.Function("FinishOpcode",actor,ec,Join({Native(261u),Return()}));
    engine.Function("FinishFinal",actor,ec,Join({Final(finish),Return()}));
    engine.Function("FinishThenFail",actor,ec,Join({Native(261u),Native(999u),Return()}));
    engine.Function("FinishExtraOpcode",actor,ec,Join({Native(261u,{Int(9)}),Return()}));
    for(const auto ref:{actor,zone,info,level,model,baseEngine})engine.ClassBody(ref);
    const auto engineTable=fixture.Write(engine);
    Package classes;classes.stem="FinishClasses";const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine"),ca=classes.Import("Actor",ce);
    const auto probe=classes.Export("Probe",0,ca),after=classes.Property("After","IntProperty",probe,cc);
    const auto increment=Assign(Ref(0x01u,after),Native(146u,{Ref(0x01u,after),Int(1)}));
    classes.State("Wait",probe,cc,Join({Native(261u),increment}));
    classes.State("SleepWait",probe,cc,Join({Native(256u,{Real(7.5f)}),increment}));
    classes.State("FailAfterWait",probe,cc,Join({Native(261u),increment,Native(999u)}));
    classes.State("StopOnly",probe,cc,{});classes.ClassBody(probe);const auto classTable=fixture.Write(classes);
    Package map;map.stem="FinishFixture";const auto me=map.ImportPackage("Engine"),mc=map.ImportPackage("FinishClasses");
    const auto world=map.Export("MyLevel",map.Import("Level",me)),worldModel=map.Export("Model0",map.Import("Model",me));
    const auto levelActor=map.Export("Level0",map.Import("LevelInfo",me)),driver=map.Export("Driver",map.Import("Probe",mc)),raw=map.Export("Raw",map.Import("Probe",mc)),empty=map.Export("NoFrame",map.Import("Probe",mc));
    for(const auto ref:{levelActor,driver,raw,empty}) {
        Bytes b;
        if(ref==driver||ref==raw) {
            auto& e=map.exports.at(static_cast<std::size_t>(ref-1));e.ObjFlags=ObjectFlags::HasStack;
            Index(b,e.ObjClass);Index(b,e.ObjClass);U64(b,~std::uint64_t{});U32(b,0u);Index(b,-1);
        }
        map.ObjectTag(b,"Level",levelActor);map.ObjectTag(b,"XLevel",world);map.ByteTag(b,"Role",4u);map.ActorBody(ref,std::move(b));
    }
    Bytes b;Index(b,0);U32(b,4u);U32(b,4u);for(const auto ref:{levelActor,driver,raw,empty})Index(b,ref);
    for(unsigned i=0u;i<5u;++i)Index(b,0);
    U32(b,7777u);U32(b,0u);Index(b,worldModel);Index(b,0);
    U32(b,0u);Index(b,0);for(unsigned i=0u;i<16u;++i)Index(b,0);map.Body(world,std::move(b));
    Bytes m;Index(m,0);for(unsigned i=0u;i<6u;++i)Float(m,0.0f);m.push_back(0u);
    for(unsigned i=0u;i<4u;++i)Float(m,0.0f);
    for(unsigned i=0u;i<5u;++i)Index(m,0);
    U32(m,0);U32(m,1);Index(m,levelActor);U64(m,0);U64(m,0);for(unsigned i=0u;i<7u;++i)Index(m,0);U32(m,1);U32(m,0);map.Body(worldModel,std::move(m));
    const std::string config="[Engine.Engine]\nGameEngine=Engine.GameEngine\n";const Bytes configBytes(config.begin(),config.end());
    const auto ini=fixture.directory/"System"/"DeusEx.ini";WriteBytes(ini,configBytes);fixture.sources.emplace(ini,configBytes);
    return {{coreTable,engineTable,classTable},fixture.Write(map,true)};
}
void PrepareActualNativeIdentity() {
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared&&PreparePortableRuntimeObjectLookup().prepared,
        "Generated FinishAnim lacks real native Engine/metadata/Package/lookup preparation");
}
void Initialize(Fixture& fixture,const Tables& tables) {
    const auto initialized=InitializePortableRuntime(tables.scripts);Require(initialized.passed,"Generated FinishAnim runtime initialization failed");
    PrepareActualNativeIdentity();
    const auto loaded=LoadPortableRuntimeMap(tables.map);Require(loaded.passed,"Generated FinishAnim native map failed loading");
    fixture.baseline=fixture.Save("Baseline");
}
QuestVr::Vm::Result Refusal(Fixture& fixture,const std::string& label,const std::function<QuestVr::Vm::Result()>& action,
    Status expected=Status::Unsupported) {
    const auto before=fixture.Save("BeforeRefusal");const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    const auto actors=GetPortableRuntimeMapActors(true);const auto result=action();const auto after=GC::GetStats();
    Require(!result.passed()&&!result.committed&&result.status==expected,"FinishAnim refusal "+label+": "+result.error);
    Require(fixture.Save("AfterRefusal")==before&&GetPortableRuntimeWorldRevision()==revision&&GetPortableRuntimeMapActors(true).size()==actors.size(),"FinishAnim refusal retained checkpoint/revision/actor changes: "+label);
    Require(gc.numObjects==after.numObjects&&gc.memoryUsage==after.memoryUsage,"FinishAnim refusal leaked UObject ownership: "+label);++refusals;return result;
}
void Configure(float frame=0.25f,float rate=1.0f,float tween=2.0f,float last=0.9f,bool loop=false,bool finished=false,const std::string& actor="Driver") {
    Call("SetSequence",{Name("Select")},actor);Call("SetFrame",{Number(frame)},actor);Call("SetRate",{Number(rate)},actor);
    Call("SetTween",{Number(tween)},actor);Call("SetLast",{Number(last)},actor);Call("SetLoop",{Boolean(loop)},actor);
    Call("SetFinished",{Boolean(finished)},actor);Call("SetNotify",{Boolean(true)},actor);
}
void Enter(const std::string& state="Wait") {Call("GotoState",{Name(state)});Require(Frame().control->codePresent,"GotoState did not position actual Begin code");}
void Run() {
    Fixture fixture;const auto tables=Build(fixture);Initialize(fixture,tables);
    const auto reset=[&]{fixture.Reset();};
    // None/zero-rate/past-end paths never fabricate or inspect a native frame.
    const auto none=Call("Engine.Actor.FinishAnim",{},"NoFrame");Require(none.writes==0u&&!ReadPortableActorStateObject(Path("NoFrame")),"Inactive None FinishAnim fabricated frame/writes");
    Configure(0.0f,0.0f,1.0f,1.0f,false,true,"NoFrame");const auto stopped=Call("FinishOpcode",{},"NoFrame");
    Require(stopped.writes==0u&&!ReadPortableActorStateObject(Path("NoFrame")),"Zero-rate FinishAnim required a native frame");
    Configure(1.0f,1.0f,1.0f,1.0f,true,true,"NoFrame");const auto terminal=Call("FinishOpcode",{},"NoFrame");
    Require(terminal.writes==2u&&!Read("bAnimLoop","NoFrame").boolean&&!Read("bAnimFinished","NoFrame").boolean&&Read("bAnimNotify","NoFrame").boolean,"Loop packed clear did not precede inactive past-last return");reset();
    for(const auto* actor:{"NoFrame","Raw"}) {
        Configure(0.25f,1.0f,2.0f,0.9f,false,false,actor);
        Refusal(fixture,std::string("reached unsupported native frame ")+actor,[&]{return ExecutePortableActorFunction(Path(actor),"FinishOpcode");});reset();
    }
    Enter();Configure();
    const auto declaration=LoadPortableFunctionScript(tables.scripts[1u],FindPortableExport(tables.scripts[1u],"Actor.FinishAnim"));
    Require(declaration.nativeIndex==261u&&declaration.functionFlags==0x409u&&declaration.logicalSize==0u&&declaration.children==0&&declaration.bytecode.empty(),"Generated FinishAnim is not actual native261/409 zero-child declaration");
    const auto installed=Call("Engine.Actor.FinishAnim");Require(installed.writes==1u&&Frame().latent==QuestVr::StateLatent::FinishAnim,"Direct native261 did not install exactly one modeled385 slot write");
    const auto waiting=fixture.Save("Waiting");const auto saved=Decode(waiting);const auto stored=Record(saved,Path());
    Require(stored.state&&stored.state->frame&&stored.state->frame->latent==QuestVr::StateLatent::FinishAnim&&!stored.latentTimeLeft,"FinishAnim wait save incorrectly requires or creates a native timer");
    Require(ValidatePortableRuntimeState((fixture.directory/"Waiting.sav").string())&&LoadPortableRuntimeState((fixture.directory/"Waiting.sav").string())&&fixture.Save("WaitingWarm")==waiting,"Warm FinishAnim save replacement changed exact frame/fields");
    const auto prior=Frame();const auto noPoll=ResumePortableActorState(Path());
    Require(noPoll.passed()&&noPoll.status==Status::Waiting&&noPoll.writes==0u&&Frame().statementIndex==prior.statementIndex,"No-time state resume advanced/polled native385");
    const auto blocked=AdvancePortableActorState(Path(),100.0f);
    Require(blocked.passed()&&blocked.status==Status::Waiting&&blocked.writes==0u&&Frame().latent==QuestVr::StateLatent::FinishAnim,"Poll385 used elapsed/frame instead of fresh bAnimFinished");
    Equal("After",Value::Integer(0),"Blocked poll executed later state statements");
    // A direct command does not resume the state; its first native261 remains
    // ahead of the PC. A finished poll releases, then that real statement can
    // immediately install another wait without clearing nonloop finished.
    Call("SetFinished",{Boolean(true)});const auto releaseThenWait=AdvancePortableActorState(Path(),0.0f);
    Require(releaseThenWait.passed()&&releaseThenWait.status==Status::Waiting&&Frame().latent==QuestVr::StateLatent::FinishAnim&&releaseThenWait.writes==2u,"Poll/first real FinishAnim statement failed same-phase wait installation");
    const auto complete=AdvancePortableActorState(Path(),0.0f);
    Require(complete.passed()&&complete.status==Status::Stopped&&Frame().latent==QuestVr::StateLatent::Continue&&!Frame().control->codePresent,"Finished poll did not continue exact state PC to Stop");
    Equal("After",Value::Integer(1),"FinishAnim state continuation did not execute exactly once");Equal("bAnimFinished",Value::Bool(true),"Poll incorrectly cleared finished flag");reset();
    // Real state bytecode installs a wait and yields before its next statement.
    Enter();Configure();const auto stateWait=ResumePortableActorState(Path());
    Require(stateWait.passed()&&stateWait.status==Status::Waiting&&Frame().latent==QuestVr::StateLatent::FinishAnim&&stateWait.writes==1u,"Actual raw native261 state statement failed to yield");
    Equal("After",Value::Integer(0),"New FinishAnim wait ran subsequent statement");
    const auto statePC=Frame().statementIndex;
    Require(AdvancePortableActorState(Path(),0.1f).status==Status::Waiting&&Frame().statementIndex==statePC,"Blocked385 changed state PC");
    Call("SetFinished",{Boolean(true)});Require(AdvancePortableActorState(Path(),0.1f).passed(),"Real state FinishAnim release failed");Equal("After",Value::Integer(1),"Released real state lost its next statement");reset();
    // The authored final-function route is distinct from the raw native opcode.
    Enter();Configure();Call("FinishFinal");Require(Frame().latent==QuestVr::StateLatent::FinishAnim,"Final-function261 route did not install wait");reset();
    for(const auto frame:{0.0f,-0.0f,0.25f,-0.25f}) {
        Enter();Configure(frame,frame<0.0f?0.0f:-2.0f,frame<0.0f?-3.0f:0.0f,0.9f);
        Call("FinishOpcode");Require(Frame().latent==QuestVr::StateLatent::FinishAnim,"FinishAnim incorrectly rejected signed nonzero selected rate");reset();
    }
    for(const auto frame:{0.0f,-0.0f,-0.25f}) {
        Enter();Configure(frame,frame<0.0f?1.0f:-0.0f,frame<0.0f?-0.0f:1.0f,0.9f);
        const auto inactive=fixture.Save("InactiveBefore");const auto result=Call("FinishOpcode");
        Require(result.writes==0u&&fixture.Save("InactiveAfter")==inactive,"Signed zero selected rate changed frame/latent fields");reset();
    }
    // Existing Sleep installs the actual actor timer.261 overwrites only its
    // latent action;385 must retain that stale timer through save and release.
    Enter("SleepWait");Configure();const auto sleeping=ResumePortableActorState(Path());
    Require(sleeping.passed()&&sleeping.status==Status::Waiting&&Frame().latent==QuestVr::StateLatent::Sleep,"Original256 fixture failed to sleep");
    Call("FinishOpcode");const auto stale=Decode(fixture.Save("StaleTimer"));
    Require(Record(stale,Path()).latentTimeLeft==7.5f&&Frame().latent==QuestVr::StateLatent::FinishAnim,"FinishAnim changed actual stale Sleep counter");
    Require(AdvancePortableActorState(Path(),999.0f).status==Status::Waiting,"FinishAnim poll improperly subtracted stale Sleep counter");
    Call("SetFinished",{Boolean(true)});Require(AdvancePortableActorState(Path(),999.0f).passed(),"FinishAnim with stale timer did not release");
    Require(Record(Decode(fixture.Save("StaleReleased")),Path()).latentTimeLeft==7.5f,"385 release mutated unrelated latent timer");Equal("After",Value::Integer(1),"385 release did not resume sleeping state's exact next PC");reset();
    // A genuine transition cancels the wait; no pending old animation wakeup.
    Enter();Configure();Require(ResumePortableActorState(Path()).passed(),"Cannot create cancelable wait");Call("GotoState",{Name("StopOnly")});
    Require(Frame().latent==QuestVr::StateLatent::Continue&&Frame().control->selectedPath=="FinishClasses.Probe.StopOnly","GotoState did not cancel actual FinishAnim latent action");
    Require(AdvancePortableActorState(Path(),0.0f).status==Status::Stopped,"Canceled FinishAnim prevented new state Stop");reset();
    // Three leaves: loop, finished, actual modern latent installation. Every
    // partial write and later unsupported call must undo the complete root.
    Enter();Configure(0.25f,1.0f,2.0f,0.9f,true,true);const auto before=fixture.Save("ThreeWriteBefore");
    const auto full=Call("FinishOpcode");Require(full.writes==3u&&!Read("bAnimLoop").boolean&&!Read("bAnimFinished").boolean&&Read("bAnimNotify").boolean,"FinishAnim loop/native385 stores do not match exact three-leaf packet");
    Require(LoadPortableRuntimeState((fixture.directory/"ThreeWriteBefore.sav").string())&&fixture.Save("ThreeWriteReset")==before,"Cannot reset three-write checkpoint");
    for(std::size_t prefix=0u;prefix<full.writes;++prefix) {
        QuestVr::Vm::Limits limits;limits.writes=prefix;
        const auto rejected=Refusal(fixture,"all native261 write prefixes",[&]{return ExecutePortableActorFunction(Path(),"FinishOpcode",{},limits);},Status::Budget);
        Require(rejected.writes==prefix,"Native261 write cutoff did not stop at exact shared prefix");
    }
    Require(full.nativeWork&&full.nativeWork->used>0u,"Native261 omitted bounded native work accounting");
    for(const auto prefix:{std::size_t{0u},full.nativeWork->used/2u,full.nativeWork->used-1u}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=prefix;
        const auto rejected=Refusal(fixture,"native261 native-work prefixes",[&]{return ExecutePortableActorFunction(Path(),"FinishOpcode",{},limits);},Status::Budget);
        Require(rejected.nativeWork&&rejected.nativeWork->exhausted,"Native261 native-work refusal did not preserve exhausted evidence");
    }
    Refusal(fixture,"completed loop/frame stores then native999",[&]{return ExecutePortableActorFunction(Path(),"FinishThenFail");});
    Refusal(fixture,"raw nonempty261 packet",[&]{return ExecutePortableActorFunction(Path(),"FinishExtraOpcode");});
    Refusal(fixture,"direct nonempty261 packet",[&]{return ExecutePortableActorFunction(Path(),"Engine.Actor.FinishAnim",{{Value::Integer(9),{}}});});reset();
    Enter("FailAfterWait");Configure();Require(ResumePortableActorState(Path()).passed(),"Cannot arm later failed poll continuation");Call("SetFinished",{Boolean(true)});
    Refusal(fixture,"poll clear plus subsequent state write then999",[&]{return AdvancePortableActorState(Path(),0.0f);});
    QuestVr::Vm::Limits pollCut;pollCut.writes=0u;Refusal(fixture,"poll385 clear write cutoff",[&]{return AdvancePortableActorState(Path(),0.0f,pollCut);},Status::Budget);reset();
    // Cold save restore retains FinishAnim with NO timer, then original poll
    // advances that exact persistent PC instead of restarting the state body.
    Enter();Configure();Require(ResumePortableActorState(Path()).passed(),"Cannot prepare cold FinishAnim wait");const auto cold=fixture.Save("Cold");
    ShutdownPortableRuntime();Require(InitializePortableRuntime(tables.scripts).passed,"Cold FinishAnim runtime initialization failed");PrepareActualNativeIdentity();
    Require(LoadPortableRuntimeMap(tables.map).passed,"Cold FinishAnim map reload failed");
    Require(ValidatePortableRuntimeState((fixture.directory/"Cold.sav").string())&&LoadPortableRuntimeState((fixture.directory/"Cold.sav").string())&&fixture.Save("ColdRestored")==cold,"Cold FinishAnim restore changed exact frame/clock/fields");
    Require(!Record(Decode(cold),Path()).latentTimeLeft&&Frame().latent==QuestVr::StateLatent::FinishAnim,"Cold FinishAnim fabricated a timer");
    auto legacy=Decode(cold);const auto legacyActor=std::find_if(legacy.objects.begin(),legacy.objects.end(),[](const auto& row){return row.path==Path();});
    Require(legacyActor!=legacy.objects.end()&&legacyActor->state&&legacyActor->state->frame,"Legacy negative control lost actual saved frame");
    legacyActor->state->frame->control.reset();const auto legacyPath=fixture.directory/"LegacyFinishAnim.sav";WriteBytes(legacyPath,Envelope(cold,legacy));
    const auto beforeLegacy=fixture.Save("BeforeLegacyRefusal");const auto legacyRevision=GetPortableRuntimeWorldRevision();
    Require(!ValidatePortableRuntimeState(legacyPath.string())&&!LoadPortableRuntimeState(legacyPath.string())&&
        fixture.Save("AfterLegacyRefusal")==beforeLegacy&&GetPortableRuntimeWorldRevision()==legacyRevision,
        "Runtime admitted ambiguous legacy portable FinishAnim ordinal or retained failed replacement");++refusals;
    Call("SetFinished",{Boolean(true)});Require(AdvancePortableActorState(Path(),0.0f).passed(),"Cold385 future release failed");Equal("After",Value::Integer(1),"Cold state PC restarted or skipped continuation");fixture.UnchangedSources();
    for(const auto malformed:{1u,2u,3u,4u,5u}) {
        Fixture bad;const auto wrong=Build(bad,malformed);Initialize(bad,wrong);
        if(malformed==5u){Enter();Configure();}
        Refusal(bad,"malformed actual261 declaration/field "+std::to_string(malformed),[&]{return ExecutePortableActorFunction(Path(),"FinishOpcode");});bad.UnchangedSources();
    }
    std::cout<<"PASS generated production FinishAnim261/poll385: "<<checks<<" checks / "<<refusals
        <<" refusals; original native packet, real modern GotoState/state waits, lazy raw-frame refusal, no-timer/stale-timer saves, all command-write prefixes, sampled work cutoffs and late root undo. Not actual Training equip/Fire, full Tick or Quest acceptance.\n";
}
}
int main() {
    std::cout<<std::unitbuf;
    try {Run();return 0;}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
