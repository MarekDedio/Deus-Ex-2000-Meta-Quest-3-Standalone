#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
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

// Genuine generated v68 metadata and production VM/native execution only.
// These isolated Pawn arguments are fixtures, not original VR-player possession,
// startup, a world Tick, a campaign playthrough or audible on-device evidence.
namespace {
using Bytes=std::vector<std::uint8_t>;
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
using Evaluation=QuestVr::Vm::Evaluation;
using Point=std::array<float,3u>;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& reason) {++checks;if(!value) throw std::runtime_error(reason);}
void U16(Bytes& b,std::uint16_t v) {b.push_back(static_cast<std::uint8_t>(v));b.push_back(static_cast<std::uint8_t>(v>>8u));}
void U32(Bytes& b,std::uint32_t v) {U16(b,static_cast<std::uint16_t>(v));U16(b,static_cast<std::uint16_t>(v>>16u));}
void U64(Bytes& b,std::uint64_t v) {U32(b,static_cast<std::uint32_t>(v));U32(b,static_cast<std::uint32_t>(v>>32u));}
void Float(Bytes& b,float v) {U32(b,std::bit_cast<std::uint32_t>(v));}
void Index(Bytes& b,std::int32_t value) {
    auto magnitude=static_cast<std::uint32_t>(value<0?-static_cast<std::int64_t>(value):value);
    auto first=static_cast<std::uint8_t>((magnitude&63u)|(value<0?128u:0u));magnitude>>=6u;if(magnitude) first|=64u;b.push_back(first);
    while(magnitude) {auto byte=static_cast<std::uint8_t>(magnitude&127u);magnitude>>=7u;if(magnitude) byte|=128u;b.push_back(byte);}
}
void Append(Bytes& b,const Bytes& tail) {b.insert(b.end(),tail.begin(),tail.end());}
void Replace32(Bytes& b,std::size_t offset,std::uint32_t v) {for(unsigned i=0u;i<4u;++i) b.at(offset+i)=static_cast<std::uint8_t>(v>>(8u*i));}
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);Require(size<=64u*1024u*1024u,"Noise fixture read exceeds independent bound");
    Bytes bytes(static_cast<std::size_t>(size));std::ifstream file(path,std::ios::binary);Require(static_cast<bool>(file),"Cannot inspect noise fixture");
    if(!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file),"Noise fixture read was truncated");return bytes;
}
void WriteBytes(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);Require(static_cast<bool>(file),"Cannot create generated noise file");
    file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));Require(static_cast<bool>(file),"Cannot write generated noise file");
}
struct Code {Bytes raw;std::size_t logical{};};
Code Token(std::uint8_t op) {return {{op},1u};}
Code Ref(std::uint8_t op,std::int32_t reference) {Code c{{op},5u};Index(c.raw,reference);return c;}
Code Integer(std::int32_t value) {Code c{{0x1du},5u};U32(c.raw,static_cast<std::uint32_t>(value));return c;}
Code Real(float value) {Code c{{0x1eu},5u};Float(c.raw,value);return c;}
Code Vector(Point point) {Code c{{0x23u},13u};for(auto v:point) Float(c.raw,v);return c;}
Code Join(std::initializer_list<Code> parts) {Code c;for(const auto& p:parts) {Append(c.raw,p.raw);c.logical+=p.logical;}return c;}
Code Assign(Code left,Code right) {return Join({Token(0x0fu),std::move(left),std::move(right)});}
Code Return(Code value=Token(0x0bu)) {return Join({Token(0x04u),std::move(value)});}
Code Native(std::uint16_t index,std::initializer_list<Code> args={}) {
    Code c;if(index>=256u) {c.raw={static_cast<std::uint8_t>(0x60u+(index>>8u)),static_cast<std::uint8_t>(index)};c.logical=2u;}
    else {c.raw={static_cast<std::uint8_t>(index)};c.logical=1u;}
    for(const auto& a:args) {Append(c.raw,a.raw);c.logical+=a.logical;}return Join({c,Token(0x16u)});
}
Code Final(std::int32_t fn,std::initializer_list<Code> args={}) {auto c=Ref(0x1cu,fn);for(const auto& a:args) {Append(c.raw,a.raw);c.logical+=a.logical;}return Join({c,Token(0x16u)});}
Code Context(Code target,Code body,std::uint8_t zero=0u) {
    Require(body.logical<=65535u,"Noise Context exceeds independent skip bound");auto c=Join({Token(0x19u),std::move(target)});
    U16(c.raw,static_cast<std::uint16_t>(body.logical));c.raw.push_back(zero);c.logical+=3u;Append(c.raw,body.raw);c.logical+=body.logical;return c;
}
struct Package {
    std::string stem;std::vector<std::string> names{"None"};std::vector<ImportTableEntry> imports;std::vector<ExportTableEntry> exports;
    std::vector<std::function<Bytes(std::int32_t,std::int32_t)>> builders;std::map<std::int32_t,std::vector<std::int32_t>> children;
    std::int32_t Name(const std::string& value) {const auto found=std::find(names.begin(),names.end(),value);if(found!=names.end()) return static_cast<std::int32_t>(found-names.begin());names.push_back(value);return static_cast<std::int32_t>(names.size()-1u);}
    std::int32_t Import(const std::string& name,std::int32_t outer,const std::string& type="Class") {imports.push_back({Name("Core"),Name(type),outer,Name(name)});return -static_cast<std::int32_t>(imports.size());}
    std::int32_t ImportPackage(const std::string& name) {return Import(name,0,"Package");}
    std::int32_t Export(const std::string& name,std::int32_t cls=0,std::int32_t base=0,std::int32_t outer=0,bool field=false) {
        exports.push_back({cls,base,outer,Name(name),ObjectFlags{},0,-1});builders.emplace_back();const auto ref=static_cast<std::int32_t>(exports.size());if(field) children[outer].push_back(ref);return ref;
    }
    void Body(std::int32_t ref,Bytes bytes) {builders.at(static_cast<std::size_t>(ref-1))=[bytes=std::move(bytes)](std::int32_t,std::int32_t) {return bytes;};}
    std::int32_t Property(const std::string& name,const std::string& type,std::int32_t owner,std::int32_t core,std::int32_t target=0,std::uint32_t flags=0u,std::uint32_t dim=1u) {
        const auto ref=Export(name,Import(type,core),0,owner,true);
        builders.at(static_cast<std::size_t>(ref-1))=[type,target,flags,dim](std::int32_t next,std::int32_t) {
            Bytes b{0u};Index(b,0);Index(b,next);U32(b,dim);U32(b,flags);Index(b,0);if(flags&0x20u) U16(b,0);
            if(type=="ObjectProperty"||type=="StructProperty"||type=="ByteProperty") Index(b,target);
            return b;
        };return ref;
    }
    void ClassBody(std::int32_t ref,Bytes defaults={}) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,defaults=std::move(defaults)](std::int32_t next,std::int32_t first) {
            Bytes b;const auto& e=exports.at(static_cast<std::size_t>(ref-1));Index(b,e.ObjBase);Index(b,next);Index(b,0);Index(b,first);Index(b,e.ObjName);
            U32(b,0);U32(b,0);U32(b,0);U64(b,~std::uint64_t{});U64(b,~std::uint64_t{});U16(b,0xffffu);U32(b,0);U32(b,0);b.resize(b.size()+16u,0u);
            for(unsigned i=0u;i<4u;++i) Index(b,0);
            Append(b,defaults);Index(b,0);return b;
        };
    }
    void StructBody(std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);U32(b,0);U32(b,0);U32(b,0);return b;
        };
    }
    std::int32_t Enum(const std::string& name,std::int32_t owner,std::int32_t core,std::vector<std::string> items) {
        const auto ref=Export(name,Import("Enum",core),0,owner,true);builders.at(static_cast<std::size_t>(ref-1))=[this,items=std::move(items)](std::int32_t next,std::int32_t) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,static_cast<std::int32_t>(items.size()));for(const auto& item:items) Index(b,Name(item));return b;
        };return ref;
    }
    std::int32_t Function(const std::string& name,std::int32_t owner,std::int32_t core,Code code={},std::uint16_t native=0u,std::uint32_t flags=2u) {
        const auto ref=Export(name,Import("Function",core),0,owner,true);FunctionCode(ref,std::move(code),native,flags);return ref;
    }
    void FunctionCode(std::int32_t ref,Code code,std::uint16_t native=0u,std::uint32_t flags=2u) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,code=std::move(code),native,flags](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);
            U32(b,0);U32(b,0);U32(b,static_cast<std::uint32_t>(code.logical));Append(b,code.raw);U16(b,native);b.push_back(0u);U32(b,flags);if(flags&0x40u) U16(b,0);return b;
        };
    }
    void ActorBody(std::int32_t ref,Bytes tags={}) {Index(tags,0);Body(ref,std::move(tags));}
    void Tag(Bytes& b,const std::string& name,std::uint8_t type,const Bytes& data) {Require(data.size()<=255u,"Noise fixture tag too large");Index(b,Name(name));b.push_back(static_cast<std::uint8_t>(0x50u|type));b.push_back(static_cast<std::uint8_t>(data.size()));Append(b,data);}
    void ObjectTag(Bytes& b,const std::string& name,std::int32_t ref) {Bytes d;Index(d,ref);Tag(b,name,5u,d);}
    void FloatTag(Bytes& b,const std::string& name,float value) {Bytes d;Float(d,value);Tag(b,name,4u,d);}
    void ByteTag(Bytes& b,const std::string& name,std::uint8_t value) {Tag(b,name,1u,{value});}
    void BoolTag(Bytes& b,const std::string& name,bool value) {Index(b,Name(name));b.push_back(static_cast<std::uint8_t>(3u|(value?0x80u:0u)));}
    void VectorTag(Bytes& b,const std::string& name,Point point) {Index(b,Name(name));b.push_back(0x5au);Index(b,Name("Vector"));b.push_back(12u);for(auto value:point) Float(b,value);}
    Bytes Serialize() {
        std::map<std::int32_t,std::int32_t> next;for(const auto& [owner,list]:children) {static_cast<void>(owner);for(std::size_t i=1u;i<list.size();++i) next[list[i-1u]]=list[i];}
        Bytes b;U32(b,0x9e2a83c1u);U16(b,68u);U16(b,0u);b.resize(56u,0u);std::vector<Bytes> bodies;std::vector<std::int32_t> offsets;
        for(std::size_t i=0u;i<exports.size();++i) {const auto ref=static_cast<std::int32_t>(i+1u);const auto found=children.find(ref);
            auto body=builders[i]?builders[i](next[ref],found==children.end()||found->second.empty()?0:found->second.front()):Bytes{};
            offsets.push_back(static_cast<std::int32_t>(b.size()));Append(b,body);bodies.push_back(std::move(body));}
        Replace32(b,12u,static_cast<std::uint32_t>(names.size()));Replace32(b,16u,static_cast<std::uint32_t>(b.size()));
        for(const auto& name:names) {Index(b,static_cast<std::int32_t>(name.size()+1u));b.insert(b.end(),name.begin(),name.end());b.push_back(0u);U32(b,0);}
        Replace32(b,20u,static_cast<std::uint32_t>(exports.size()));Replace32(b,24u,static_cast<std::uint32_t>(b.size()));
        for(std::size_t i=0u;i<exports.size();++i) {const auto& e=exports[i];Index(b,e.ObjClass);Index(b,e.ObjBase);U32(b,static_cast<std::uint32_t>(e.ObjOuter));Index(b,e.ObjName);U32(b,static_cast<std::uint32_t>(e.ObjFlags));Index(b,static_cast<std::int32_t>(bodies[i].size()));if(!bodies[i].empty()) Index(b,offsets[i]);}
        Replace32(b,28u,static_cast<std::uint32_t>(imports.size()));Replace32(b,32u,static_cast<std::uint32_t>(b.size()));
        for(const auto& e:imports) {Index(b,e.ClassPackage);Index(b,e.ClassName);U32(b,static_cast<std::uint32_t>(e.ObjOuter));Index(b,e.ObjName);}return b;
    }
};
struct Fixture {
    std::filesystem::path parent,directory;std::map<std::filesystem::path,Bytes> sources;
    Fixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());const auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {const auto candidate=parent/("deusex-make-noise-test-"+std::to_string(stamp)+'-'+std::to_string(i));if(std::filesystem::create_directory(candidate)) {directory=std::filesystem::canonical(candidate);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Noise fixture escaped its temporary parent");std::filesystem::create_directory(directory/"System");std::filesystem::create_directory(directory/"Maps");
    }
    ~Fixture() {ShutdownPortableRuntime();std::error_code error;const auto actual=std::filesystem::weakly_canonical(directory,error);if(!error&&!directory.empty()&&actual==directory&&actual.parent_path()==parent&&actual.filename().string().rfind("deusex-make-noise-test-",0u)==0u) std::filesystem::remove_all(actual,error);}
    PortablePackageTables Write(Package& package,bool map=false) {const auto path=directory/(map?"Maps":"System")/(package.stem+(map?".dx":".u"));const auto bytes=package.Serialize();WriteBytes(path,bytes);sources.emplace(path,bytes);return LoadPortablePackageTables(path.string());}
    Bytes Snapshot(const std::string& name) const {const auto path=directory/(name+".sav");Require(SavePortableRuntimeState(path.string()),"Cannot snapshot noise runtime");return ReadBytes(path);}
    void Unchanged() const {for(const auto& [path,bytes]:sources) Require(ReadBytes(path)==bytes,"MakeNoise changed a source package");}
};
struct Cache {Point spot{10000,0,0};float time{},loudness{};};
struct Options {bool outside{true};Cache first{},second{};std::uint32_t flags{0x401u},argumentFlags{0x80u};};
struct Tables {std::vector<PortablePackageTables> scripts;PortablePackageTables map;};
Bytes EmptyModel(bool outside) {
    Bytes b{0u};for(unsigned i=0u;i<6u;++i) Float(b,0);b.push_back(0u);for(unsigned i=0u;i<4u;++i) Float(b,0);
    for(unsigned i=0u;i<5u;++i) Index(b,0);
    U32(b,0);U32(b,0);for(unsigned i=0u;i<7u;++i) Index(b,0);
    U32(b,outside?1u:0u);U32(b,0);return b;
}
Tables Build(Fixture& fixture,const Options& options={}) {
    Package core;core.stem="Core";const auto cp=core.ImportPackage("Core"),object=core.Export("Object"),meta=core.Export("Class",0,object);
    const auto vector=core.Export("Vector",core.Import("Struct",cp),0,object),rotator=core.Export("Rotator",core.Import("Struct",cp),0,object);
    for(const auto* name:{"X","Y","Z"}) core.Property(name,"FloatProperty",vector,cp);
    for(const auto* name:{"Pitch","Yaw","Roll"}) core.Property(name,"IntProperty",rotator,cp);
    const auto disable=core.Function("Disable",object,cp,{},118u,0x401u),enable=core.Function("Enable",object,cp,{},117u,0x401u);
    for(const auto fn:{disable,enable}) {const auto input=core.Property("ProbeFunc","NameProperty",fn,cp,0,0x80u);core.FunctionCode(fn,Ref(0x29u,input),fn==disable?118u:117u,0x401u);}
    core.ClassBody(object);core.ClassBody(meta);core.StructBody(vector);core.StructBody(rotator);const auto coreTable=fixture.Write(core);
    Package engine;engine.stem="Engine";const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec),ev=engine.Import("Vector",eo,"Struct"),er=engine.Import("Rotator",eo,"Struct");
    const auto actor=engine.Export("Actor",0,eo),zone=engine.Export("ZoneInfo",0,actor),info=engine.Export("LevelInfo",0,zone);
    const auto level=engine.Export("Level",0,eo),model=engine.Export("Model",0,eo),pawn=engine.Export("Pawn",0,actor),player=engine.Export("PlayerPawn",0,pawn);
    const auto game=engine.Export("GameInfo",0,actor),pri=engine.Export("PlayerReplicationInfo",0,actor);
    std::map<std::string,std::int32_t> fields;
    const auto field=[&](const char* name,const char* type,std::int32_t owner,std::int32_t target=0,std::uint32_t flags=0u) {const auto ref=engine.Property(name,type,owner,ec,target,flags);fields.emplace(name,ref);return ref;};
    field("Location","StructProperty",actor,ev,0x23u);field("OldLocation","StructProperty",actor,ev);field("Rotation","StructProperty",actor,er);
    field("Level","ObjectProperty",actor,info);field("XLevel","ObjectProperty",actor,level);field("Instigator","ObjectProperty",actor,pawn,0x20u);
    for(const auto* name:{"Owner","Base","Brush","Inventory"}) field(name,"ObjectProperty",actor,actor);
    for(const auto* name:{"bCollideActors","bBlockActors","bBlockPlayers","bCollideWorld","bStatic","bNoDelete","bDeleteMe","bAnimByOwner"}) field(name,"BoolProperty",actor);
    for(const auto* name:{"CollisionRadius","CollisionHeight"}) field(name,"FloatProperty",actor);
    for(const auto* name:{"Tag","AttachTag"}) field(name,"NameProperty",actor);
    const auto region=engine.Export("PointRegion",engine.Import("Struct",ec),0,actor);
    engine.Property("Zone","ObjectProperty",region,ec,zone);engine.Property("iLeaf","IntProperty",region,ec);engine.Property("ZoneNumber","ByteProperty",region,ec);engine.StructBody(region);field("Region","StructProperty",actor,region);
    field("PawnList","ObjectProperty",info,pawn,2u);field("Game","ObjectProperty",info,game);field("bBegunPlay","BoolProperty",info);field("bStartup","BoolProperty",info);
    field("TimeDilation","FloatProperty",info,0,0x21u);field("TimeSeconds","FloatProperty",info);
    const auto modes=engine.Enum("ENetMode",info,ec,{"NM_Standalone","NM_DedicatedServer","NM_ListenServer","NM_Client"});field("NetMode","ByteProperty",info,modes);
    field("nextPawn","ObjectProperty",pawn,pawn,2u);field("bIsPlayer","BoolProperty",pawn,0,0x20u);field("Enemy","ObjectProperty",pawn,pawn);
    field("PlayerReplicationInfo","ObjectProperty",pawn,pri,0x20u);field("Alertness","FloatProperty",pawn);field("Stimulus","FloatProperty",pawn);field("HearingThreshold","FloatProperty",pawn,0,1u);
    for(const auto* name:{"noise1time","noise1loudness","noise2time","noise2loudness"}) field(name,"FloatProperty",pawn,0,2u);
    for(const auto* name:{"noise1spot","noise2spot"}) field(name,"StructProperty",pawn,ev,2u);
    for(const auto* name:{"noise1other","noise2other"}) field(name,"ObjectProperty",pawn,pawn,2u);
    field("bTeamGame","BoolProperty",game,0,1u);field("Team","ByteProperty",pri,0,0x20u);
    const auto noise=engine.Function("MakeNoise",actor,ec,{},512u,options.flags),loudness=engine.Property("Loudness","FloatProperty",noise,ec,0,options.argumentFlags);
    engine.FunctionCode(noise,Ref(0x29u,loudness),512u,options.flags);
    const auto originalHear=engine.Function("HearNoise",pawn,ec,Return(),0u,0x800u);
    engine.Property("Loudness","FloatProperty",originalHear,ec,0,0x80u);engine.Property("NoiseMaker","ObjectProperty",originalHear,ec,actor,0x80u);
    const auto setter=[&](const char* name,std::int32_t owner,const char* key,const char* type,std::int32_t target=0) {
        const auto fn=engine.Function(name,owner,ec),value=engine.Property("Value",type,fn,ec,target,0x80u);
        engine.FunctionCode(fn,Join({Assign(Ref(0x01u,fields.at(key)),Ref(0x00u,value)),Return()}));
    };
    setter("SetInstigator",actor,"Instigator","ObjectProperty",pawn);setter("SetLocation",actor,"Location","StructProperty",ev);
    setter("SetLevel",actor,"Level","ObjectProperty",info);setter("SetXLevel",actor,"XLevel","ObjectProperty",level);
    setter("SetHead",info,"PawnList","ObjectProperty",pawn);setter("SetNetMode",info,"NetMode","ByteProperty",modes);setter("SetMirror",info,"TimeSeconds","FloatProperty");
    setter("SetNext",pawn,"nextPawn","ObjectProperty",pawn);setter("SetIsPlayer",pawn,"bIsPlayer","BoolProperty");setter("SetEnemy",pawn,"Enemy","ObjectProperty",pawn);
    setter("SetPri",pawn,"PlayerReplicationInfo","ObjectProperty",pri);setter("SetAlertness",pawn,"Alertness","FloatProperty");setter("SetThreshold",pawn,"HearingThreshold","FloatProperty");
    setter("SetStimulus",pawn,"Stimulus","FloatProperty");setter("SetTeamGame",game,"bTeamGame","BoolProperty");setter("SetTeam",pri,"Team","ByteProperty");
    setter("IllegalCacheWrite",pawn,"noise1time","FloatProperty");
    for(const auto cls:{actor,zone,info,level,model,pawn,player,game,pri}) engine.ClassBody(cls);
    Bytes defaults;engine.FloatTag(defaults,"TimeDilation",1.0f);engine.ClassBody(info,std::move(defaults));const auto engineTable=fixture.Write(engine);
    Package classes;classes.stem="NoiseClasses";const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine");
    const auto ca=classes.Import("Actor",ce),cpa=classes.Import("Pawn",ce),cpp=classes.Import("PlayerPawn",ce),co=classes.Import("Object",cc),cv=classes.Import("Vector",co,"Struct");
    const auto probe=classes.Export("Probe",0,cpa),derived=classes.Export("Derived",0,probe),sibling=classes.Export("Sibling",0,cpa),pp=classes.Export("PlayerListener",0,cpp);
    const auto splicer=classes.Export("Splicer",0,probe),mover=classes.Export("Moving",0,probe),instigatorChanger=classes.Export("InstigatorChanger",0,probe),failing=classes.Export("Failing",0,probe);
    const auto inputField=classes.Property("Input","FloatProperty",probe,cc),target=classes.Property("Target","ObjectProperty",probe,cc,ca),counter=classes.Property("Counter","IntProperty",probe,cc);
    const auto linkNext=classes.Property("Replacement","ObjectProperty",probe,cc,cpa),noiseRef=classes.Import("MakeNoise",ca,"Function");
    const auto loc=classes.Import("Location",ca,"StructProperty"),inst=classes.Import("Instigator",ca,"ObjectProperty"),next=classes.Import("nextPawn",cpa,"ObjectProperty");
    classes.Function("Noise",probe,cc,Join({Final(noiseRef,{Ref(0x01u,inputField)}),Return()}));
    classes.Function("NoiseRaw",probe,cc,Join({Native(512u,{Ref(0x01u,inputField)}),Return()}));
    classes.Function("NoiseOther",probe,cc,Join({Context(Ref(0x01u,target),Native(512u,{Ref(0x01u,inputField)})),Return()}));
    for(const auto& [name,argument]:std::vector<std::pair<std::string,Code>>{{"NoiseNothing",Token(0x0bu)},
        {"NoiseNoWrite",Context(Token(0x2au),Ref(0x01u,inputField))},{"NoisePartial",Context(Token(0x2au),Ref(0x01u,inputField),1u)}})
        classes.Function(name,probe,cc,Join({Native(512u,{argument}),Return()}));
    classes.Function("NoiseThenFail",probe,cc,Join({Assign(Ref(0x01u,counter),Integer(42)),Native(512u,{Real(1)}),Native(999u),Return()}));
    const auto sideWrapper=classes.Function("NoiseArgumentWrites",probe,cc);
    const auto inputSideEffect=classes.Function("InputSideEffect",probe,cc,Join({Assign(Ref(0x01u,counter),Integer(17)),Return(Real(.25f))}));
    classes.Property("ReturnValue","FloatProperty",inputSideEffect,cc,0,0x580u);
    classes.FunctionCode(sideWrapper,Join({Native(512u,{Final(inputSideEffect)}),Return()}));
    const auto scalarSetter=[&](const char* name,std::int32_t field,const char* type,std::int32_t constraint=0) {
        const auto fn=classes.Function(name,probe,cc),value=classes.Property("Value",type,fn,cc,constraint,0x80u);
        classes.FunctionCode(fn,Join({Assign(Ref(0x01u,field),Ref(0x00u,value)),Return()}));
    };
    scalarSetter("SetInput",inputField,"FloatProperty");scalarSetter("SetTarget",target,"ObjectProperty",ca);scalarSetter("SetReplacement",linkNext,"ObjectProperty",cpa);
    std::map<std::int32_t,std::array<std::int32_t,3u>> record;
    for(const auto cls:{probe,sibling,pp}) record.emplace(cls,std::array<std::int32_t,3u>{
        classes.Property("Heard","IntProperty",cls,cc),classes.Property("SeenLoudness","FloatProperty",cls,cc),classes.Property("SeenMaker","ObjectProperty",cls,cc,ca)});
    for(const auto cls:{probe,sibling,pp,splicer,mover,instigatorChanger,failing}) {
        const auto fn=classes.Function("HearNoise",cls,cc,{},0u,0x800u),l=classes.Property("Loudness","FloatProperty",fn,cc,0,0x80u),maker=classes.Property("NoiseMaker","ObjectProperty",fn,cc,ca,0x80u);
        const auto fieldsFor=record.at(cls==sibling?sibling:cls==pp?pp:probe);
        auto body=Join({Assign(Ref(0x01u,fieldsFor[0u]),Native(146u,{Ref(0x01u,fieldsFor[0u]),Integer(1)})),
            Assign(Ref(0x01u,fieldsFor[1u]),Ref(0x00u,l)),Assign(Ref(0x01u,fieldsFor[2u]),Ref(0x00u,maker))});
        if(cls==splicer) body=Join({body,Assign(Ref(0x01u,next),Ref(0x01u,linkNext))});
        if(cls==mover) body=Join({body,Assign(Context(Ref(0x00u,maker),Ref(0x01u,loc)),Vector({10000,0,0}))});
        if(cls==instigatorChanger) body=Join({body,Assign(Context(Ref(0x00u,maker),Ref(0x01u,inst)),Ref(0x01u,linkNext))});
        if(cls==failing) body=Join({body,Native(999u)});
        classes.FunctionCode(fn,Join({body,Return()}),0u,0x800u);
    }
    for(const auto cls:{probe,derived,sibling,pp,splicer,mover,instigatorChanger,failing}) classes.ClassBody(cls);
    const auto classesTable=fixture.Write(classes);
    Package map;map.stem="NoiseFixture";const auto me=map.ImportPackage("Engine"),mc=map.ImportPackage("NoiseClasses");
    const auto world=map.Export("MyLevel",map.Import("Level",me)),worldModel=map.Export("WorldModel",map.Import("Model",me));
    const auto levelActor=map.Export("Level0",map.Import("LevelInfo",me)),otherInfo=map.Export("AlternateInfo",map.Import("LevelInfo",me));
    std::map<std::string,std::int32_t> refs;
    for(const auto& [name,cls]:std::vector<std::pair<std::string,std::string>>{{"Driver","Probe"},{"Other","Probe"},{"Initial","Probe"},{"First","Probe"},{"Second","Derived"},
        {"Sibling","Sibling"},{"Player","PlayerListener"},{"Splicer","Splicer"},{"Mover","Moving"},{"Changer","InstigatorChanger"},{"Failure","Failing"},{"Orphan","Probe"}})
        refs.emplace(name,map.Export(name,map.Import(cls,mc)));
    refs.emplace("Game0",map.Export("Game0",map.Import("GameInfo",me)));refs.emplace("Pri0",map.Export("Pri0",map.Import("PlayerReplicationInfo",me)));refs.emplace("Pri1",map.Export("Pri1",map.Import("PlayerReplicationInfo",me)));
    refs.emplace("BasePawn",map.Export("BasePawn",map.Import("Pawn",me)));
    std::vector<std::int32_t> actors{levelActor};for(const auto& [name,ref]:refs) if(name!="Orphan") actors.push_back(ref);
    const auto author=[&](std::int32_t ref,const std::string& name) {
        Bytes tags;auto& entry=map.exports.at(static_cast<std::size_t>(ref-1));
        if(name=="First"||name=="Second") {entry.ObjFlags=ObjectFlags::HasStack;Index(tags,entry.ObjClass);Index(tags,entry.ObjClass);U64(tags,~std::uint64_t{});U32(tags,0u);Index(tags,-1);}
        map.ObjectTag(tags,"Level",levelActor);map.ObjectTag(tags,"XLevel",world);map.VectorTag(tags,"Location",name=="Other"?Point{20,0,0}:name=="Driver"?Point{0,0,0}:Point{10,0,0});
        if(ref==levelActor||ref==otherInfo) {map.BoolTag(tags,"bBegunPlay",true);map.FloatTag(tags,"TimeDilation",1);map.ByteTag(tags,"NetMode",0);map.ObjectTag(tags,"Game",refs.at("Game0"));}
        if(name=="Driver"||name=="Other"||name=="Orphan") {map.FloatTag(tags,"Input",name=="Driver"?1.0f:.25f);map.ObjectTag(tags,"Target",refs.at("Other"));map.ObjectTag(tags,"Instigator",refs.at("Initial"));}
        if(name=="Initial") {
            map.VectorTag(tags,"noise1spot",options.first.spot);map.FloatTag(tags,"noise1time",options.first.time);map.FloatTag(tags,"noise1loudness",options.first.loudness);
            map.VectorTag(tags,"noise2spot",options.second.spot);map.FloatTag(tags,"noise2time",options.second.time);map.FloatTag(tags,"noise2loudness",options.second.loudness);
            map.ObjectTag(tags,"noise1other",refs.at("First"));map.ObjectTag(tags,"noise2other",refs.at("Second"));
        }
        map.ActorBody(ref,std::move(tags));
    };
    author(levelActor,"Level0");author(otherInfo,"AlternateInfo");for(const auto& [name,ref]:refs) author(ref,name);
    Bytes body{0u};U32(body,static_cast<std::uint32_t>(actors.size()));U32(body,static_cast<std::uint32_t>(actors.size()));for(auto ref:actors) Index(body,ref);
    for(unsigned i=0u;i<5u;++i) Index(body,0);
    U32(body,7777u);U32(body,0);Index(body,worldModel);Index(body,0);
    U32(body,0u);Index(body,0); // Original serialized Float time and preliminary +100.
    for(unsigned i=0u;i<16u;++i)Index(body,0); // +9C pointer sequence, including final +100 alias.
    map.Body(world,std::move(body));map.Body(worldModel,EmptyModel(options.outside));
    static_cast<void>(cv);return {{coreTable,engineTable,classesTable},fixture.Write(map,true)};
}

std::string Path(const std::string& name) {return name.empty()?std::string{}:"NoiseFixture."+name;}
Evaluation Obj(const std::string& name) {return {Value::Text(Kind::Object,Path(name)),{}};}
Evaluation RealArg(float value) {return {Value::Float(value),{}};}
Evaluation BoolArg(bool value) {return {Value::Bool(value),{}};}
Evaluation VecArg(Point value) {return {Value::Vector(value),{}};}
Value Read(const std::string& actor,const std::string& field) {return ReadPortableActorScriptProperty(Path(actor),field);}
void Same(const Value& actual,const Value& expected,const std::string& reason) {Require(QuestVr::Vm::Equal(actual,expected),reason);}
QuestVr::Vm::Result Call(const std::string& actor,const std::string& fn,const std::vector<Evaluation>& args={}) {
    const auto result=ExecutePortableActorFunction(Path(actor),fn,args);Require(result.passed()&&result.committed,actor+'.'+fn+" failed: "+result.error+" at "+result.function+':'+std::to_string(result.offset));return result;
}
void Initialize(const Tables& tables,bool advance=true) {
    ShutdownPortableRuntime();Require(InitializePortableRuntime(tables.scripts).passed&&LoadPortableRuntimeMap(tables.map).passed,"Noise generated runtime/map failed initialization");
    if(advance) {const auto clock=AdvancePortableRuntimeLevelClock(1.0f);Require(clock.passed()&&clock.committed&&ReadPortableRuntimeLevelTime()==1.0,"Noise fixture did not advance actual native ULevel clock");}
}
void Listen(std::initializer_list<std::string> names) {
    std::vector<std::string> list(names);Call("Level0","SetHead",{Obj(list.empty()?"":list.front())});
    for(std::size_t i=0u;i<list.size();++i) Call(list[i],"SetNext",{Obj(i+1u<list.size()?list[i+1u]:"")});
}
void Heard(const std::string& listener,int count,float loudness=1.0f,const std::string& maker="Driver") {
    Same(Read(listener,"Heard"),Value::Integer(count),"HearNoise callback count differs: "+listener);
    if(count) {Same(Read(listener,"SeenLoudness"),Value::Float(loudness),"HearNoise Loudness argument differs");Same(Read(listener,"SeenMaker"),Obj(maker).value,"HearNoise source Actor was replaced by Instigator");}
}
void CacheIs(unsigned slot,const Cache& expected) {
    const auto prefix=std::string("noise")+std::to_string(slot);Same(Read("Initial",prefix+"spot"),Value::Vector(expected.spot),"Cache source location differs");
    Same(Read("Initial",prefix+"time"),Value::Float(expected.time),"Cache timestamp did not use native Double time Float spill");Same(Read("Initial",prefix+"loudness"),Value::Float(expected.loudness),"Cache loudness differs");
}
void OthersUnchanged() {Same(Read("Initial","noise1other"),Obj("First").value,"MakeNoise changed noise1other");Same(Read("Initial","noise2other"),Obj("Second").value,"MakeNoise changed noise2other");}
QuestVr::Vm::Result Refused(Fixture& fixture,const std::string& actor,const std::string& fn,const std::vector<Evaluation>& args={},const QuestVr::Vm::Limits& limits={}) {
    const auto before=fixture.Snapshot("BeforeRefusal");const auto clock=ReadPortableRuntimeLevelTime();const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    const auto result=ExecutePortableActorFunction(Path(actor),fn,args,limits);const auto after=GC::GetStats();
    Require(!result.passed()&&!result.committed,"Invalid MakeNoise call committed: "+fn);
    Require(fixture.Snapshot("AfterRefusal")==before&&ReadPortableRuntimeLevelTime()==clock&&GetPortableRuntimeWorldRevision()==revision&&
        gc.numObjects==after.numObjects&&gc.memoryUsage==after.memoryUsage,"MakeNoise refusal retained cache/Stimulus/callback/nativeclock/revision/GC changes");++refusals;return result;
}

void EntryAndCache() {
    Fixture fixture;const auto tables=Build(fixture);Initialize(tables);Call("Driver","SetInstigator",{Obj("")});
    auto baseline=fixture.Snapshot("NoneBaseline");const auto revision=GetPortableRuntimeWorldRevision();
    for(const auto* fn:{"Noise","NoiseRaw","NoiseNothing","NoiseNoWrite","NoisePartial"}) Call("Driver",fn);
    Call("Driver","MakeNoise",{RealArg(1)});Require(fixture.Snapshot("NoneAfter")==baseline&&GetPortableRuntimeWorldRevision()==revision,"None Instigator native manufactured effects");
    Call("Driver","NoiseArgumentWrites");Same(Read("Driver","Counter"),Value::Integer(17),"Native no-op skipped required input expression");
    Refused(fixture,"Driver","MakeNoise",{});Refused(fixture,"Driver","MakeNoise",{RealArg(1),RealArg(2)});
    Refused(fixture,"Driver","MakeNoise",{Obj("First")});Refused(fixture,"Orphan","Noise");Refused(fixture,"Initial","IllegalCacheWrite",{RealArg(7)});
    for(const auto invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        Call("Driver","SetInstigator",{Obj("Initial")});Refused(fixture,"Driver","MakeNoise",{RealArg(invalid)});
    }
    Initialize(tables);Listen({"First"});Call("Level0","SetNetMode",{{Value::Byte(3),{}}});baseline=fixture.Snapshot("ClientBaseline");
    Call("Driver","Noise");Require(fixture.Snapshot("ClientAfter")==baseline,"NetMode3 failed early return before cache/Instigator routing");Heard("First",0);
    for(const auto mode:{1u,2u,255u}) {
        Initialize(tables);Listen({"First"});Call("Level0","SetNetMode",{{Value::Byte(static_cast<std::uint8_t>(mode)),{}}});
        Call("Driver","Noise");Heard("First",1);CacheIs(1,{{0,0,0},1,1});
    }
    Initialize(tables);Listen({"First","Second"});Call("Level0","SetMirror",{RealArg(1234)});Call("Driver","Noise");
    CacheIs(1,{{0,0,0},1,1});CacheIs(2,{});OthersUnchanged();Heard("First",1);Heard("Second",1);
    const auto continuation=fixture.Snapshot("NoiseContinuation");
    Require(AdvancePortableRuntimeLevelClock(.5f).passed(),"Cannot perturb successful noise clock");
    Call("Driver","SetLocation",{VecArg({100,0,0})});Call("Driver","Noise");
    Require(LoadPortableRuntimeState((fixture.directory/"NoiseContinuation.sav").string())&&
        fixture.Snapshot("NoiseWarmRestored")==continuation&&ReadPortableRuntimeLevelTime()==1.0,
        "Noise cache/Stimulus/callback/nativeclock continuation failed warm replacement");
    Initialize(tables,false);
    Require(LoadPortableRuntimeState((fixture.directory/"NoiseContinuation.sav").string())&&
        fixture.Snapshot("NoiseColdRestored")==continuation&&ReadPortableRuntimeLevelTime()==1.0,
        "Noise cache/Stimulus/callback/nativeclock continuation failed cold replacement");
    baseline=fixture.Snapshot("SuppressedFirst");Call("Driver","NoiseRaw");Require(fixture.Snapshot("SuppressedFirstAfter")==baseline,"Recent nearby noise1 failed suppression");
    Initialize(tables);Call("Driver","SetLevel",{Obj("AlternateInfo")});Call("Driver","SetXLevel",{Obj("")});Listen({"First"});Call("Driver","Noise");Heard("First",1);CacheIs(1,{{0,0,0},1,1});
    Initialize(tables);Call("Driver","SetInput",{RealArg(4)});Call("Driver","NoiseOther");CacheIs(1,{{20,0,0},1,4});
    for(const auto* fn:{"NoiseNothing","NoiseNoWrite","NoisePartial"}) {Initialize(tables);Call("Driver",fn);CacheIs(1,{{0,0,0},1,0});OthersUnchanged();}
    fixture.Unchanged();
    for(const auto& options:std::vector<Options>{
        {true,{{10000,0,0},0,0},{{0,0,0},1,1}},
        {true,{{10000,0,0},1,10},{{10000,0,0},0,0}},
        {true,{{0,0,0},1,.5f},{{10000,0,0},1,10}},
        {true,{{10000,0,0},1,10},{{10000,0,0},1,.5f}},
        {true,{{10000,0,0},1,10},{{10000,0,0},1,10}}}) {
        Fixture branch;const auto branchTables=Build(branch,options);Initialize(branchTables);Listen({"First"});const auto before=branch.Snapshot("BeforeCacheBranch");Call("Driver","Noise");OthersUnchanged();
        if(options.second.spot[0u]==0) {Require(branch.Snapshot("AfterCacheBranch")==before,"Cache2 did not suppress before any write/delivery");Heard("First",0);}
        else if(options.second.time==0) {CacheIs(1,options.first);CacheIs(2,{{0,0,0},1,1});Heard("First",1);}
        else if(options.first.loudness<=1 || options.second.loudness<=1) {CacheIs(1,{{0,0,0},1,1});CacheIs(2,options.second);Heard("First",1);}
        else {CacheIs(1,options.first);CacheIs(2,options.second);Heard("First",1);}
        branch.Unchanged();
    }
}

void RoutingAndRollback() {
    Fixture fixture;const auto tables=Build(fixture);Initialize(tables);Listen({"Initial","First","Second","Sibling","Player"});Call("Driver","Noise");
    Heard("Initial",0);Heard("First",1);Heard("Second",1);Heard("Sibling",0);Heard("Player",0);
    Initialize(tables);Listen({"BasePawn"});Call("Driver","Noise");
    Same(Read("BasePawn","Stimulus"),Value::Float(2),"Nonplayer reverse class-ancestry branch omitted a base-class Pawn listener");
    Initialize(tables);Listen({"Sibling","Player"});Call("Initial","SetIsPlayer",{BoolArg(true)});Call("Driver","Noise");Heard("Sibling",1);Heard("Player",0);
    Initialize(tables);Listen({"Sibling","Player"});Call("Initial","SetEnemy",{Obj("Player")});Call("Player","SetIsPlayer",{BoolArg(true)});Call("Driver","Noise");Heard("Sibling",1);Heard("Player",0);
    Initialize(tables);Listen({"Splicer","First"});Call("Splicer","SetReplacement",{Obj("Second")});Call("Driver","Noise");Heard("Splicer",1);Heard("First",0);Heard("Second",1);
    Initialize(tables);Listen({"Mover","Second"});Call("Driver","Noise");Heard("Mover",1);Heard("Second",0);Same(Read("Driver","Location"),Value::Vector({10000,0,0}),"Callback did not synchronously move source");
    Initialize(tables);Listen({"Changer","Second","Initial"});Call("Changer","SetReplacement",{Obj("Second")});Call("Driver","Noise");
    Heard("Changer",1);Heard("Second",0);Heard("Initial",1);Same(Read("Driver","Instigator"),Obj("Second").value,"Callback source.Instigator change was not committed");
    Initialize(tables);Listen({"Changer","First"});Call("Changer","SetReplacement",{Obj("Second")});
    Call("Initial","SetIsPlayer",{BoolArg(true)});Call("First","SetIsPlayer",{BoolArg(true)});Call("Game0","SetTeamGame",{BoolArg(true)});
    Call("Initial","SetPri",{Obj("Pri0")});Call("First","SetPri",{Obj("Pri1")});Call("First","SetLocation",{VecArg({3000,0,0})});
    Call("Driver","Noise");Heard("Changer",1);Heard("First",1);
    Same(Read("Driver","Instigator"),Obj("Second").value,"Captured-instigator regression did not actually replace the source relation");
    Initialize(tables);Listen({"First","Failure"});const auto failure=Refused(fixture,"Driver","Noise");Require(failure.error.find("999")!=std::string::npos,"Late callback did not reach intentional native999");Heard("First",0);
    Initialize(tables);Listen({"First"});Refused(fixture,"Driver","NoiseThenFail");Heard("First",0);
    Initialize(tables);Listen({"First"});QuestVr::Vm::Limits writes;writes.writes=2u;Refused(fixture,"Driver","Noise",{},writes);
    Initialize(tables);Listen({"First"});QuestVr::Vm::Limits instructions;instructions.instructions=1u;Refused(fixture,"Driver","Noise",{},instructions);
    Initialize(tables);Listen({"First"});Call("First","Disable",{{Value::Text(Kind::Name,"HearNoise"),{}}});Call("Driver","Noise");Heard("First",0);
    Same(Read("First","Stimulus"),Value::Float(0),"Disabled HearNoise reached CanHear and changed Stimulus");
    Initialize(tables);Listen({"First"});Call("First","SetNext",{Obj("First")});
    Call("First","Disable",{{Value::Text(Kind::Name,"HearNoise"),{}}});
    const auto cycle=Refused(fixture,"Driver","Noise");
    Require(cycle.error.find("Native actor lifecycle work budget exceeded")!=std::string::npos,
        "Live cyclic PawnList was silently deduplicated/truncated instead of bounded root rollback");
    fixture.Unchanged();
}

void HearingMathAndBsp() {
    Fixture fixture;const auto tables=Build(fixture);Initialize(tables);Listen({"First"});Call("First","SetLocation",{VecArg({1000,0,0})});
    Call("First","SetThreshold",{RealArg(2)});Call("Driver","Noise");Heard("First",0);Same(Read("First","Stimulus"),Value::Float(1.2f),"Threshold refusal did not retain stored Stimulus");
    Initialize(tables);Listen({"First"});Call("First","SetLocation",{VecArg({1000,0,0})});Call("First","SetAlertness",{RealArg(.4f)});Call("Driver","Noise");
    const float expectedStimulus=static_cast<float>(static_cast<double>(1.2f)+static_cast<double>(.5f)*static_cast<double>(.4f));
    Heard("First",1);Same(Read("First","Stimulus"),Value::Float(expectedStimulus),"General Stimulus perceived/Alertness formula differs");
    Initialize(tables);Listen({"First"});Call("First","SetLocation",{VecArg({2001,0,0})});Call("First","SetStimulus",{RealArg(9)});Call("Driver","Noise");
    Heard("First",0);Same(Read("First","Stimulus"),Value::Float(9),"General range refusal changed Stimulus");
    Initialize(tables);Listen({"First"});Call("First","SetLocation",{VecArg({2000,0,0})});Call("Driver","Noise");Heard("First",1);
    Same(Read("First","Stimulus"),Value::Float(.3f),"General range equality or perceived quotient differs");
    Initialize(tables);Listen({"First"});Call("Initial","SetIsPlayer",{BoolArg(true)});Call("First","SetIsPlayer",{BoolArg(true)});
    Call("Game0","SetTeamGame",{BoolArg(true)});Call("Initial","SetPri",{Obj("Pri0")});Call("First","SetPri",{Obj("Pri1")});
    Call("First","SetLocation",{VecArg({3000,0,0})});Call("First","SetStimulus",{RealArg(9)});Call("Driver","Noise");
    Heard("First",1);Same(Read("First","Stimulus"),Value::Float(9),"Same-team range path changed general Stimulus");
    Initialize(tables);Listen({"First"});Call("Initial","SetIsPlayer",{BoolArg(true)});Call("First","SetIsPlayer",{BoolArg(true)});
    Call("Game0","SetTeamGame",{BoolArg(true)});Call("Initial","SetPri",{Obj("Pri0")});Call("First","SetPri",{Obj("Pri1")});
    Call("Pri1","SetTeam",{{Value::Byte(1),{}}});Call("First","SetLocation",{VecArg({3000,0,0})});Call("Driver","Noise");Heard("First",0);
    for(const auto loudness:{0.0f,-1.0f}) {Initialize(tables);Listen({"First"});Call("First","SetLocation",{VecArg({0,0,0})});Call("Driver","SetInput",{RealArg(loudness)});Call("Driver","Noise");Heard("First",1,loudness);}
    fixture.Unchanged();
    Fixture blocked;Options options;options.outside=false;const auto solidTables=Build(blocked,options);
    Initialize(solidTables);Listen({"First"});Call("Driver","Noise");Heard("First",0);Same(Read("First","Stimulus"),Value::Float(2),"Blocked BSP did not retain accepted-threshold Stimulus");
    Initialize(solidTables);Listen({"First"});Call("First","SetLocation",{VecArg({0,0,0})});Call("First","SetStimulus",{RealArg(7)});Call("Driver","Noise");
    Heard("First",1);Same(Read("First","Stimulus"),Value::Float(7),"Coincident general path did not bypass Stimulus/BSP");
    Initialize(solidTables);Listen({"First"});Call("First","SetLocation",{VecArg({0,0,0})});Call("Initial","SetIsPlayer",{BoolArg(true)});Call("First","SetIsPlayer",{BoolArg(true)});
    Call("Game0","SetTeamGame",{BoolArg(true)});Call("Initial","SetPri",{Obj("Pri0")});Call("First","SetPri",{Obj("Pri1")});Call("Driver","Noise");Heard("First",0);
    blocked.Unchanged();
}
void DeclarationRefusals() {
    for(const auto& options:std::vector<Options>{{true,{},{},0x501u,0x80u},{true,{},{},0x401u,0x90u}}) {
        Fixture fixture;const auto tables=Build(fixture,options);Initialize(tables);Refused(fixture,"Driver","NoiseRaw");Refused(fixture,"Driver","MakeNoise",{RealArg(1)});fixture.Unchanged();
    }
}
} // namespace
int main() {
    try {const auto baseline=GC::GetStats().numObjects;EntryAndCache();RoutingAndRollback();HearingMathAndBsp();DeclarationRefusals();
        ShutdownPortableRuntime();GC::Collect();Require(GC::GetStats().numObjects==baseline,"Noise fixture leaked rooted UObjects");
        std::cout<<"PASS original MakeNoise512 production integration: "<<checks<<" checks, "<<refusals
            <<" refusals; real native clock/cache fields, live Pawn/probe/event routing, CanHear and complete root rollback.\n";return 0;
    } catch(const std::exception& error) {ShutdownPortableRuntime();std::cerr<<"MakeNoise integration failed: "<<error.what()<<'\n';return 1;}
}
