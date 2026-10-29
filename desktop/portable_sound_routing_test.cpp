#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_level_clock.h"
#include "quest_script_state.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
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
#include <utility>
#include <vector>

// Generated real v68 packages exercise production metadata, bytecode, native
// routing and transactional callbacks. No retail executable, fake PlayerPawn
// possession, synthetic renderer/audio output or manual callback dispatch.
namespace {
using Bytes=std::vector<std::uint8_t>;
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
using Evaluation=QuestVr::Vm::Evaluation;
using Point=std::array<float,3u>;
std::size_t checks{},refusals{};

void Require(bool condition,const std::string& reason) {
    ++checks;if(!condition) throw std::runtime_error(reason);
}
void U16(Bytes& bytes,std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));bytes.push_back(static_cast<std::uint8_t>(value>>8u));
}
void U32(Bytes& bytes,std::uint32_t value) {
    U16(bytes,static_cast<std::uint16_t>(value));U16(bytes,static_cast<std::uint16_t>(value>>16u));
}
void U64(Bytes& bytes,std::uint64_t value) {
    U32(bytes,static_cast<std::uint32_t>(value));U32(bytes,static_cast<std::uint32_t>(value>>32u));
}
void Float(Bytes& bytes,float value) {
    std::uint32_t bits{};std::memcpy(&bits,&value,sizeof(bits));U32(bytes,bits);
}
void Index(Bytes& bytes,std::int32_t value) {
    auto remaining=static_cast<std::uint32_t>(value<0?-static_cast<std::int64_t>(value):value);
    auto first=static_cast<std::uint8_t>((remaining&63u)|(value<0?128u:0u));
    remaining>>=6u;
    if(remaining) {first|=64u;}
    bytes.push_back(first);
    while(remaining) {
        auto next=static_cast<std::uint8_t>(remaining&127u);remaining>>=7u;
        if(remaining) {next|=128u;}
        bytes.push_back(next);
    }
}
void Append(Bytes& bytes,const Bytes& other) {bytes.insert(bytes.end(),other.begin(),other.end());}
void Replace32(Bytes& bytes,std::size_t offset,std::uint32_t value) {
    for(unsigned i=0;i<4u;++i) bytes.at(offset+i)=static_cast<std::uint8_t>(value>>(8u*i));
}
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);
    Require(size<=64u*1024u*1024u,"Sound fixture exceeds bounded inspection size");
    Bytes bytes(static_cast<std::size_t>(size));std::ifstream file(path,std::ios::binary);
    Require(static_cast<bool>(file),"Cannot inspect sound fixture");
    if(!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file),"Sound fixture was truncated");return bytes;
}
void WriteBytes(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    Require(static_cast<bool>(file),"Cannot create generated sound package");
    file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file),"Cannot write generated sound package");
}

struct Code {Bytes raw;std::size_t logical{};};
Code Token(std::uint8_t op) {return {{op},1u};}
Code Ref(std::uint8_t op,std::int32_t reference) {
    Code code{{op},5u};Index(code.raw,reference);return code;
}
Code Integer(std::int32_t value) {Code code{{0x1du},5u};U32(code.raw,static_cast<std::uint32_t>(value));return code;}
Code Real(float value) {Code code{{0x1eu},5u};Float(code.raw,value);return code;}
Code Byte(std::uint8_t value) {return {{0x24u,value},2u};}
Code Vector(Point value) {
    Code code{{0x23u},13u};
    for(auto component:value) {Float(code.raw,component);}
    return code;
}
Code Join(std::initializer_list<Code> parts) {
    Code code;for(const auto& part:parts) {Append(code.raw,part.raw);code.logical+=part.logical;}return code;
}
Code Assign(Code left,Code right) {return Join({Token(0x0fu),std::move(left),std::move(right)});}
Code Return(Code value=Token(0x0bu)) {return Join({Token(0x04u),std::move(value)});}
Code Native(std::uint16_t index,std::initializer_list<Code> arguments={}) {
    Code code;
    if(index>=256u) {code.raw={static_cast<std::uint8_t>(0x60u+(index>>8u)),static_cast<std::uint8_t>(index)};code.logical=2u;}
    else {code.raw={static_cast<std::uint8_t>(index)};code.logical=1u;}
    for(const auto& argument:arguments) {Append(code.raw,argument.raw);code.logical+=argument.logical;}
    return Join({code,Token(0x16u)});
}
Code Final(std::int32_t function,std::initializer_list<Code> arguments={}) {
    Code code=Ref(0x1cu,function);
    for(const auto& argument:arguments) {Append(code.raw,argument.raw);code.logical+=argument.logical;}
    return Join({code,Token(0x16u)});
}
Code Context(Code target,Code expression,std::uint8_t zeroSize=0u) {
    Require(expression.logical<=65535u,"Generated sound Context skip exceeds bound");
    Code code=Join({Token(0x19u),std::move(target)});
    U16(code.raw,static_cast<std::uint16_t>(expression.logical));code.raw.push_back(zeroSize);code.logical+=3u;
    Append(code.raw,expression.raw);code.logical+=expression.logical;return code;
}

// Independent compact copy of the genuine-package writer used by FastTrace
// integration. All generated files stay in this fixture's unique temp folder.
struct Package {
    std::string stem;
    std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
    std::vector<std::function<Bytes(std::int32_t,std::int32_t)>> builders;
    std::map<std::int32_t,std::vector<std::int32_t>> children;
    std::int32_t Name(const std::string& name) {
        const auto found=std::find(names.begin(),names.end(),name);
        if(found!=names.end()) return static_cast<std::int32_t>(found-names.begin());
        names.push_back(name);return static_cast<std::int32_t>(names.size()-1u);
    }
    std::int32_t Import(const std::string& name,std::int32_t outer,const std::string& type="Class") {
        imports.push_back({Name("Core"),Name(type),outer,Name(name)});return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) {return Import(name,0,"Package");}
    std::int32_t Export(const std::string& name,std::int32_t cls=0,std::int32_t base=0,
        std::int32_t outer=0,bool field=false) {
        exports.push_back({cls,base,outer,Name(name),ObjectFlags{},0,-1});builders.emplace_back();
        const auto reference=static_cast<std::int32_t>(exports.size());
        if(field) {children[outer].push_back(reference);}
        return reference;
    }
    std::int32_t Property(const std::string& name,const std::string& type,std::int32_t owner,
        std::int32_t core,std::int32_t target=0,std::uint32_t flags=0u,std::uint32_t dimension=1u) {
        const auto reference=Export(name,Import(type,core),0,owner,true);
        builders.at(static_cast<std::size_t>(reference-1))=[type,target,flags,dimension](std::int32_t next,std::int32_t) {
            Bytes body{0u};Index(body,0);Index(body,next);U32(body,dimension);U32(body,flags);Index(body,0);
            if(flags&0x20u) U16(body,0u); // CPF_Net replication offset precedes subtype data.
            if(type=="ObjectProperty"||type=="StructProperty"||type=="ByteProperty") Index(body,target);
            else if(type=="ClassProperty") {Index(body,target);Index(body,0);}
            return body;
        };return reference;
    }
    void ClassBody(std::int32_t reference,Bytes defaults={}) {
        builders.at(static_cast<std::size_t>(reference-1))=[this,reference,defaults=std::move(defaults)](std::int32_t next,std::int32_t first) {
            Bytes body;Index(body,exports.at(static_cast<std::size_t>(reference-1)).ObjBase);Index(body,next);
            Index(body,0);Index(body,first);Index(body,exports.at(static_cast<std::size_t>(reference-1)).ObjName);
            U32(body,0);U32(body,0);U32(body,0);U64(body,~std::uint64_t{});U64(body,~std::uint64_t{});
            U16(body,0xffffu);U32(body,0);U32(body,0);body.resize(body.size()+16u,0u);
            for(unsigned i=0;i<4u;++i) {Index(body,0);}
            Append(body,defaults);Index(body,0);
            return body;
        };
    }
    void StructBody(std::int32_t reference) {
        builders.at(static_cast<std::size_t>(reference-1))=[this,reference](std::int32_t next,std::int32_t first) {
            Bytes body{0u};Index(body,0);Index(body,next);Index(body,0);Index(body,first);
            Index(body,exports.at(static_cast<std::size_t>(reference-1)).ObjName);U32(body,0);U32(body,0);U32(body,0);return body;
        };
    }
    std::int32_t Enum(const std::string& name,std::int32_t owner,std::int32_t core,
        std::vector<std::string> items) {
        const auto reference=Export(name,Import("Enum",core),0,owner,true);
        builders.at(static_cast<std::size_t>(reference-1))=[this,items=std::move(items)](std::int32_t next,std::int32_t) {
            Bytes body{0u};Index(body,0);Index(body,next);Index(body,static_cast<std::int32_t>(items.size()));
            for(const auto& item:items) {Index(body,Name(item));}
            return body;
        };return reference;
    }
    std::int32_t Function(const std::string& name,std::int32_t owner,std::int32_t core,
        Code code={},std::uint16_t native=0u,std::uint32_t flags=0u) {
        const auto reference=Export(name,Import("Function",core),0,owner,true);
        FunctionCode(reference,std::move(code),native,flags);return reference;
    }
    void FunctionCode(std::int32_t reference,Code code,std::uint16_t native=0u,std::uint32_t flags=0u) {
        builders.at(static_cast<std::size_t>(reference-1))=[this,reference,native,flags,code=std::move(code)]
            (std::int32_t next,std::int32_t first) {
            Bytes body{0u};Index(body,0);Index(body,next);Index(body,0);Index(body,first);
            Index(body,exports.at(static_cast<std::size_t>(reference-1)).ObjName);
            U32(body,0);U32(body,0);U32(body,static_cast<std::uint32_t>(code.logical));Append(body,code.raw);
            const auto functionFlags=flags?flags:native?0x401u:2u;
            U16(body,native);body.push_back(0u);U32(body,functionFlags);
            // FUNC_Net stores its U16 replication offset after FunctionFlags,
            // including named native ClientHearSound (0xd40). Omitting this
            // genuine v68 trailer truncates the export before runtime begins.
            if(functionFlags&0x40u) {U16(body,0u);}
            return body;
        };
    }
    void State(const std::string& name,std::int32_t owner,std::int32_t core,Code prefix) {
        const auto reference=Export(name,Import("State",core),0,owner,true);
        builders.at(static_cast<std::size_t>(reference-1))=[this,reference,prefix=std::move(prefix)]
            (std::int32_t next,std::int32_t first) {
            Code code=Join({prefix,Token(0x08u),Token(0x0cu)});
            Index(code.raw,Name("Begin"));U32(code.raw,0u);Index(code.raw,0);U32(code.raw,0xffffffffu);code.logical+=16u;
            Bytes body{0u};Index(body,0);Index(body,next);Index(body,0);Index(body,first);
            Index(body,exports.at(static_cast<std::size_t>(reference-1)).ObjName);
            U32(body,0);U32(body,0);U32(body,static_cast<std::uint32_t>(code.logical));Append(body,code.raw);
            U64(body,~std::uint64_t{});U64(body,~std::uint64_t{});
            Require(prefix.logical+2u<=65535u,"State label table exceeds bounded logical address");
            U16(body,static_cast<std::uint16_t>(prefix.logical+2u));U32(body,0u);return body;
        };
    }
    void Body(std::int32_t reference,Bytes body) {
        builders.at(static_cast<std::size_t>(reference-1))=[body=std::move(body)](std::int32_t,std::int32_t) {return body;};
    }
    void ActorBody(std::int32_t reference,Bytes body) {Index(body,0);Body(reference,std::move(body));}
    void Tag(Bytes& bytes,const std::string& name,std::uint8_t type,const Bytes& data) {
        Require(data.size()<=255u,"Sound fixture tag too large");Index(bytes,Name(name));
        bytes.push_back(static_cast<std::uint8_t>(0x50u|type));bytes.push_back(static_cast<std::uint8_t>(data.size()));Append(bytes,data);
    }
    void ObjectTag(Bytes& bytes,const std::string& name,std::int32_t reference) {Bytes data;Index(data,reference);Tag(bytes,name,5u,data);}
    void ByteTag(Bytes& bytes,const std::string& name,std::uint8_t value) {Tag(bytes,name,1u,{value});}
    void FloatTag(Bytes& bytes,const std::string& name,float value) {Bytes data;Float(data,value);Tag(bytes,name,4u,data);}
    void BoolTag(Bytes& bytes,const std::string& name,bool value) {
        Index(bytes,Name(name));bytes.push_back(static_cast<std::uint8_t>(3u|(value?0x80u:0u)));
    }
    void VectorTag(Bytes& bytes,const std::string& name,Point value) {
        Index(bytes,Name(name));bytes.push_back(0x5au);Index(bytes,Name("Vector"));bytes.push_back(12u);
        for(auto component:value) Float(bytes,component);
    }
    Bytes Serialize() {
        std::map<std::int32_t,std::int32_t> next;
        for(const auto& [owner,list]:children) {
            static_cast<void>(owner);
            for(std::size_t i=1u;i<list.size();++i) {next[list[i-1u]]=list[i];}
        }
        Bytes bytes;U32(bytes,0x9e2a83c1u);U16(bytes,68u);U16(bytes,0u);bytes.resize(56u,0u);
        std::vector<Bytes> bodies;std::vector<std::int32_t> offsets;
        for(std::size_t i=0u;i<exports.size();++i) {
            const auto reference=static_cast<std::int32_t>(i+1u);const auto owner=children.find(reference);
            auto body=builders[i]?builders[i](next[reference],owner==children.end()||owner->second.empty()?0:owner->second.front()):Bytes{};
            offsets.push_back(static_cast<std::int32_t>(bytes.size()));Append(bytes,body);bodies.push_back(std::move(body));
        }
        Replace32(bytes,12u,static_cast<std::uint32_t>(names.size()));Replace32(bytes,16u,static_cast<std::uint32_t>(bytes.size()));
        for(const auto& name:names) {
            Index(bytes,static_cast<std::int32_t>(name.size()+1u));bytes.insert(bytes.end(),name.begin(),name.end());bytes.push_back(0u);U32(bytes,0u);
        }
        Replace32(bytes,20u,static_cast<std::uint32_t>(exports.size()));Replace32(bytes,24u,static_cast<std::uint32_t>(bytes.size()));
        for(std::size_t i=0u;i<exports.size();++i) {
            const auto& entry=exports[i];Index(bytes,entry.ObjClass);Index(bytes,entry.ObjBase);U32(bytes,static_cast<std::uint32_t>(entry.ObjOuter));
            Index(bytes,entry.ObjName);U32(bytes,static_cast<std::uint32_t>(entry.ObjFlags));Index(bytes,static_cast<std::int32_t>(bodies[i].size()));
            if(!bodies[i].empty()) Index(bytes,offsets[i]);
        }
        Replace32(bytes,28u,static_cast<std::uint32_t>(imports.size()));Replace32(bytes,32u,static_cast<std::uint32_t>(bytes.size()));
        for(const auto& entry:imports) {
            Index(bytes,entry.ClassPackage);Index(bytes,entry.ClassName);U32(bytes,static_cast<std::uint32_t>(entry.ObjOuter));Index(bytes,entry.ObjName);
        }
        return bytes;
    }
};

struct Fixture {
    std::filesystem::path parent,directory;
    std::map<std::filesystem::path,Bytes> sources;
    Fixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for(unsigned attempt=0u;attempt<20u;++attempt) {
            const auto candidate=parent/("deusex-sound-routing-test-"+std::to_string(stamp)+'-'+std::to_string(attempt));
            if(std::filesystem::create_directory(candidate)) {directory=std::filesystem::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Generated sound fixture escaped its temporary parent");
        std::filesystem::create_directory(directory/"System");std::filesystem::create_directory(directory/"Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime();std::error_code error;const auto actual=std::filesystem::weakly_canonical(directory,error);
        if(!error&&!directory.empty()&&actual==directory&&actual.parent_path()==parent&&
            actual.filename().string().rfind("deusex-sound-routing-test-",0u)==0u) std::filesystem::remove_all(actual,error);
    }
    PortablePackageTables Write(Package& package,bool map=false) {
        const auto path=directory/(map?"Maps":"System")/(package.stem+(map?".dx":".u"));
        auto bytes=package.Serialize();WriteBytes(path,bytes);sources.emplace(path,std::move(bytes));return LoadPortablePackageTables(path.string());
    }
    Bytes Snapshot(const std::string& label) const {
        const auto path=directory/(label+".sav");Require(SavePortableRuntimeState(path.string()),"Cannot save sound test state");return ReadBytes(path);
    }
    void Unchanged() const {for(const auto& [path,bytes]:sources) Require(ReadBytes(path)==bytes,"Sound routing changed source package");}
};
struct Tables {std::vector<PortablePackageTables> scripts;PortablePackageTables map;};
std::string Path(const std::string& name) {return name.empty()?std::string{}:"SoundFixture."+name;}
Evaluation Object(const std::string& name) {return {Value::Text(Kind::Object,Path(name)),{}};}
Evaluation Bool(bool value) {return {Value::Bool(value),{}};}

Bytes EmptyModel(bool outside) {
    Bytes body{0u};for(unsigned i=0u;i<6u;++i) {Float(body,0.0f);}
    body.push_back(0u);
    for(unsigned i=0u;i<4u;++i) {Float(body,0.0f);}
    for(unsigned i=0u;i<5u;++i) {Index(body,0);}
    U32(body,0);U32(body,0);
    for(unsigned i=0u;i<7u;++i) {Index(body,0);}
    U32(body,outside?1u:0u);U32(body,0);return body;
}

Tables Build(Fixture& fixture,bool outside=true) {
    Package core;core.stem="Core";const auto cp=core.ImportPackage("Core");
    const auto object=core.Export("Object"),meta=core.Export("Class",0,object);
    core.Property("Class","ClassProperty",object,cp,meta);core.Property("Name","NameProperty",object,cp);
    core.Property("Outer","ObjectProperty",object,cp,object);core.Property("ObjectFlags","IntProperty",object,cp);
    const auto vector=core.Export("Vector",core.Import("Struct",cp),0,object),rotator=core.Export("Rotator",core.Import("Struct",cp),0,object);
    for(const auto* name:{"X","Y","Z"}) core.Property(name,"FloatProperty",vector,cp);
    for(const auto* name:{"Pitch","Yaw","Roll"}) core.Property(name,"IntProperty",rotator,cp);
    core.ClassBody(object);core.ClassBody(meta);core.StructBody(vector);core.StructBody(rotator);const auto coreTable=fixture.Write(core);

    Package engine;engine.stem="Engine";const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec);
    const auto ev=engine.Import("Vector",eo,"Struct"),er=engine.Import("Rotator",eo,"Struct");
    const auto actor=engine.Export("Actor",0,eo),zone=engine.Export("ZoneInfo",0,actor),info=engine.Export("LevelInfo",0,zone);
    const auto level=engine.Export("Level",0,eo),model=engine.Export("Model",0,eo),sound=engine.Export("Sound",0,eo);
    const auto pawn=engine.Export("Pawn",0,actor),playerPawn=engine.Export("PlayerPawn",0,pawn),player=engine.Export("Player",0,eo);
    engine.Property("Actor","ObjectProperty",player,ec,playerPawn);
    const auto location=engine.Property("Location","StructProperty",actor,ec,ev);
    engine.Property("OldLocation","StructProperty",actor,ec,ev);engine.Property("Rotation","StructProperty",actor,ec,er);
    const auto actorLevel=engine.Property("Level","ObjectProperty",actor,ec,info),actorXLevel=engine.Property("XLevel","ObjectProperty",actor,ec,level);
    const auto owner=engine.Property("Owner","ObjectProperty",actor,ec,actor);
    for(const auto* name:{"Base","Brush","Inventory"}) engine.Property(name,"ObjectProperty",actor,ec,actor);
    const auto instigator=engine.Property("Instigator","ObjectProperty",actor,ec,pawn);
    for(const auto* name:{"bCollideActors","bBlockActors","bBlockPlayers","bCollideWorld","bStatic","bNoDelete","bDeleteMe","bAnimByOwner"})
        engine.Property(name,"BoolProperty",actor,ec);
    for(const auto* name:{"CollisionRadius","CollisionHeight"}) engine.Property(name,"FloatProperty",actor,ec);
    for(const auto* name:{"Tag","AttachTag"}) engine.Property(name,"NameProperty",actor,ec);
    const auto volume=engine.Property("TransientSoundVolume","FloatProperty",actor,ec),radius=engine.Property("TransientSoundRadius","FloatProperty",actor,ec);
    const auto modes=engine.Enum("ENetMode",info,ec,{"NM_Standalone","NM_DedicatedServer","NM_ListenServer","NM_Client"});
    const auto netMode=engine.Property("NetMode","ByteProperty",info,ec,modes),head=engine.Property("PawnList","ObjectProperty",info,ec,pawn);
    const auto timeDilation=engine.Property("TimeDilation","FloatProperty",info,ec,0,0x21u),timeSeconds=engine.Property("TimeSeconds","FloatProperty",info,ec);
    engine.Property("bBegunPlay","BoolProperty",info,ec);engine.Property("bStartup","BoolProperty",info,ec);
    const auto next=engine.Property("nextPawn","ObjectProperty",pawn,ec,pawn),isPlayer=engine.Property("bIsPlayer","BoolProperty",pawn,ec);
    engine.Property("Player","ObjectProperty",playerPawn,ec,player);engine.Property("ViewTarget","ObjectProperty",playerPawn,ec,actor);
    const auto region=engine.Export("PointRegion",engine.Import("Struct",ec),0,actor);
    engine.Property("Zone","ObjectProperty",region,ec,zone);engine.Property("iLeaf","IntProperty",region,ec);
    engine.Property("ZoneNumber","ByteProperty",region,ec);engine.StructBody(region);engine.Property("Region","StructProperty",actor,ec,region);
    const auto slots=engine.Enum("ESoundSlot",actor,ec,{"SLOT_None","SLOT_Misc","SLOT_Pain","SLOT_Interact","SLOT_Ambient","SLOT_Talk","SLOT_Interface"});
    const auto playSound=engine.Function("PlaySound",actor,ec,{},264u,0x401u);
    std::vector<std::int32_t> parameters;
    parameters.push_back(engine.Property("Sound","ObjectProperty",playSound,ec,sound,0x80u));
    parameters.push_back(engine.Property("Slot","ByteProperty",playSound,ec,slots,0x90u));
    parameters.push_back(engine.Property("Volume","FloatProperty",playSound,ec,0,0x90u));
    parameters.push_back(engine.Property("bNoOverride","BoolProperty",playSound,ec,0,0x90u));
    parameters.push_back(engine.Property("Radius","FloatProperty",playSound,ec,0,0x90u));
    parameters.push_back(engine.Property("Pitch","FloatProperty",playSound,ec,0,0x90u));
    engine.Property("ReturnValue","IntProperty",playSound,ec,0,0x580u);
    Code signature;
    for(auto param:parameters) {signature=Join({signature,Ref(0x29u,param)});}
    engine.FunctionCode(playSound,std::move(signature),264u,0x401u);
    const auto hear=engine.Function("ClientHearSound",pawn,ec,{},0u,0xd40u);
    engine.Property("Actor","ObjectProperty",hear,ec,actor,0x80u);engine.Property("Id","IntProperty",hear,ec,0,0x80u);
    engine.Property("S","ObjectProperty",hear,ec,sound,0x80u);
    for(const auto* name:{"SoundLocation","Parameters"}) engine.Property(name,"StructProperty",hear,ec,ev,0x80u);
    const auto gotoState=engine.Function("GotoState",actor,ec,{},113u);
    engine.Property("NewState","NameProperty",gotoState,ec,0,0x90u);engine.Property("Label","NameProperty",gotoState,ec,0,0x90u);
    const auto setter=[&](const char* name,std::int32_t owner,std::int32_t field,const char* type,std::int32_t target=0) {
        const auto fn=engine.Function(name,owner,ec);const auto input=engine.Property("Input",type,fn,ec,target,0x80u);
        engine.FunctionCode(fn,Join({Assign(Ref(0x01u,field),Ref(0x00u,input)),Return()}));
    };
    setter("SetLevel",actor,actorLevel,"ObjectProperty",info);setter("SetXLevel",actor,actorXLevel,"ObjectProperty",level);
    setter("SetOwner",actor,owner,"ObjectProperty",actor);setter("SetInstigator",actor,instigator,"ObjectProperty",pawn);
    setter("SetLocation",actor,location,"StructProperty",ev);setter("SetVolume",actor,volume,"FloatProperty");
    setter("SetRadius",actor,radius,"FloatProperty");setter("SetHead",info,head,"ObjectProperty",pawn);
    setter("SetNetMode",info,netMode,"ByteProperty");setter("SetNext",pawn,next,"ObjectProperty",pawn);
    setter("SetIsPlayer",pawn,isPlayer,"BoolProperty");
    setter("SetTimeDilation",info,timeDilation,"FloatProperty");setter("SetTimeSeconds",info,timeSeconds,"FloatProperty");
    for(auto cls:{actor,zone,info,level,model,sound,pawn,playerPawn,player}) engine.ClassBody(cls);
    Bytes levelDefaults;engine.FloatTag(levelDefaults,"TimeDilation",1.0f);engine.FloatTag(levelDefaults,"TimeSeconds",0.0f);
    engine.ClassBody(info,std::move(levelDefaults));
    const auto engineTable=fixture.Write(engine);

    Package classes;classes.stem="SoundClasses";const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine");
    const auto ca=classes.Import("Actor",ce),cpa=classes.Import("Pawn",ce),cs=classes.Import("Sound",ce),co=classes.Import("Object",cc);
    const auto cv=classes.Import("Vector",co,"Struct"),probe=classes.Export("Probe",0,ca),listener=classes.Export("Listener",0,cpa);
    const auto splicer=classes.Export("SpliceListener",0,listener),moving=classes.Export("MovingListener",0,listener),failing=classes.Export("FailingListener",0,listener);
    const auto soundField=classes.Property("TestSound","ObjectProperty",probe,cc,cs),link=classes.Property("Link","ObjectProperty",probe,cc,ca);
    const auto counter=classes.Property("Counter","IntProperty",probe,cc),stateChannel=classes.Property("StateChannel","IntProperty",probe,cc);
    const auto play=classes.Import("PlaySound",ca,"Function");
    const auto intFunction=[&](const char* name,Code code,std::uint32_t flags=2u) {
        const auto fn=classes.Function(name,probe,cc,std::move(code),0u,flags);classes.Property("ReturnValue","IntProperty",fn,cc,0,0x580u);return fn;
    };
    intFunction("Play",Return(Final(play,{Ref(0x01u,soundField)})));
    intFunction("PlaySimulated",Return(Final(play,{Ref(0x01u,soundField)})),0x102u);
    intFunction("PlayOpcode",Return(Native(264u,{Ref(0x01u,soundField)})));
    intFunction("PlayVirtual",Return(Join({Ref(0x1bu,classes.Name("PlaySound")),Ref(0x01u,soundField),Token(0x16u)})));
    intFunction("PlayOther",Return(Context(Ref(0x01u,link),Native(264u,{Ref(0x01u,soundField)}))));
    intFunction("PlayFull",Return(Final(play,{Ref(0x01u,soundField),Byte(3u),Real(.75f),Token(0x27u),Real(512.0f),Real(1.25f)})));
    classes.Function("PlayThenFail",probe,cc,Join({Assign(Ref(0x01u,counter),Integer(42)),Final(play,{Ref(0x01u,soundField)}),Native(999u),Return()}));
    classes.State("Speaking",probe,cc,Assign(Ref(0x01u,stateChannel),Final(play,{Ref(0x01u,soundField)})));
    const auto heard=classes.Property("Heard","IntProperty",listener,cc),seenActor=classes.Property("SeenActor","ObjectProperty",listener,cc,ca);
    const auto seenId=classes.Property("SeenId","IntProperty",listener,cc),seenSound=classes.Property("SeenSound","ObjectProperty",listener,cc,cs);
    const auto seenLocation=classes.Property("SeenLocation","StructProperty",listener,cc,cv),seenParameters=classes.Property("SeenParameters","StructProperty",listener,cc,cv);
    const auto replacement=classes.Property("ReplacementNext","ObjectProperty",listener,cc,cpa);
    const auto importedNext=classes.Import("nextPawn",cpa,"ObjectProperty"),importedLocation=classes.Import("Location",ca,"StructProperty");
    const auto replace=classes.Function("SetReplacement",listener,cc),replaceInput=classes.Property("Input","ObjectProperty",replace,cc,cpa,0x80u);
    classes.FunctionCode(replace,Join({Assign(Ref(0x01u,replacement),Ref(0x00u,replaceInput)),Return()}));
    for(auto cls:{listener,splicer,moving,failing}) {
        const auto fn=classes.Function("ClientHearSound",cls,cc,{},0u,0x902u);
        const auto inputActor=classes.Property("Actor","ObjectProperty",fn,cc,ca,0x80u),inputId=classes.Property("Id","IntProperty",fn,cc,0,0x80u);
        const auto inputSound=classes.Property("S","ObjectProperty",fn,cc,cs,0x80u),inputLocation=classes.Property("SoundLocation","StructProperty",fn,cc,cv,0x80u);
        const auto inputParameters=classes.Property("Parameters","StructProperty",fn,cc,cv,0x80u);
        Code body=Join({Assign(Ref(0x01u,heard),Native(146u,{Ref(0x01u,heard),Integer(1)})),
            Assign(Ref(0x01u,seenActor),Ref(0x00u,inputActor)),Assign(Ref(0x01u,seenId),Ref(0x00u,inputId)),
            Assign(Ref(0x01u,seenSound),Ref(0x00u,inputSound)),Assign(Ref(0x01u,seenLocation),Ref(0x00u,inputLocation)),
            Assign(Ref(0x01u,seenParameters),Ref(0x00u,inputParameters))});
        if(cls==splicer) body=Join({body,Assign(Ref(0x01u,importedNext),Ref(0x01u,replacement))});
        if(cls==moving) body=Join({body,Assign(Context(Ref(0x00u,inputActor),Ref(0x01u,importedLocation)),Vector({4000,0,0}))});
        if(cls==failing) body=Join({body,Native(999u)});
        classes.FunctionCode(fn,Join({body,Return()}),0u,0x902u);
    }
    for(auto cls:{probe,listener,splicer,moving,failing}) {classes.ClassBody(cls);}
    const auto classesTable=fixture.Write(classes);

    Package map;map.stem="SoundFixture";const auto me=map.ImportPackage("Engine"),mc=map.ImportPackage("SoundClasses");
    const auto world=map.Export("MyLevel",map.Import("Level",me)),worldModel=map.Export("WorldModel",map.Import("Model",me));
    const auto soundA=map.Export("SoundA",map.Import("Sound",me)),soundB=map.Export("SoundB",map.Import("Sound",me));
    map.ActorBody(soundA,{});map.ActorBody(soundB,{});
    const auto levelActor=map.Export("Level0",map.Import("LevelInfo",me)),alternateInfo=map.Export("AlternateInfo",map.Import("LevelInfo",me));
    const auto driver=map.Export("Driver",map.Import("Probe",mc)),other=map.Export("Other",map.Import("Probe",mc));
    const auto first=map.Export("First",map.Import("Listener",mc)),second=map.Export("Second",map.Import("Listener",mc));
    const auto third=map.Export("Third",map.Import("Listener",mc)),splice=map.Export("Splicer",map.Import("SpliceListener",mc));
    const auto move=map.Export("Mover",map.Import("MovingListener",mc)),failure=map.Export("Failure",map.Import("FailingListener",mc));
    const auto basePawn=map.Export("BasePawn",map.Import("Pawn",me)),plainPlayer=map.Export("PlainPlayer",map.Import("PlayerPawn",me));
    const auto orphan=map.Export("Orphan",map.Import("Probe",mc));
    const std::vector<std::int32_t> actors{levelActor,driver,other,first,second,third,splice,move,failure,basePawn,plainPlayer};
    auto authored=actors;authored.push_back(alternateInfo);authored.push_back(orphan);
    for(auto reference:authored) {
        Bytes tags;
        if(reference==driver) {
            auto& entry=map.exports.at(static_cast<std::size_t>(reference-1));entry.ObjFlags=ObjectFlags::HasStack;
            Index(tags,entry.ObjClass);Index(tags,entry.ObjClass);U64(tags,~std::uint64_t{});U32(tags,0u);Index(tags,-1);
        }
        map.ObjectTag(tags,"Level",levelActor);map.ObjectTag(tags,"XLevel",world);
        map.VectorTag(tags,"Location",reference==other?Point{10,0,0}:reference==driver?Point{0,0,0}:Point{1,0,0});
        map.FloatTag(tags,"TransientSoundVolume",1.0f);map.FloatTag(tags,"TransientSoundRadius",800.0f);
        if(reference==levelActor||reference==alternateInfo) {
            map.BoolTag(tags,"bBegunPlay",true);map.ByteTag(tags,"NetMode",reference==alternateInfo?3u:0u);
            map.FloatTag(tags,"TimeDilation",1.0f);map.FloatTag(tags,"TimeSeconds",0.0f);
        }
        if(reference==driver||reference==other||reference==orphan) {map.ObjectTag(tags,"TestSound",reference==other?soundB:soundA);map.ObjectTag(tags,"Link",other);}
        map.ActorBody(reference,std::move(tags));
    }
    Bytes worldBody{0u};U32(worldBody,static_cast<std::uint32_t>(actors.size()));U32(worldBody,static_cast<std::uint32_t>(actors.size()));
    for(auto actorRef:actors) {Index(worldBody,actorRef);}
    for(unsigned i=0u;i<5u;++i) {Index(worldBody,0);}
    U32(worldBody,7777u);U32(worldBody,0);Index(worldBody,worldModel);Index(worldBody,0);
    U32(worldBody,0u);Index(worldBody,0); // Original serialized Float time and preliminary +100.
    for(unsigned i=0u;i<16u;++i)Index(worldBody,0); // Separate +0x9C..+0xD8 pointers, not FirstDeleted+0x100.
    Index(worldBody,0); // Original v68 serialized native travel-map count.
    map.Body(world,std::move(worldBody));
    map.Body(worldModel,EmptyModel(outside));return {{coreTable,engineTable,classesTable},fixture.Write(map,true)};
}

void Initialize(const Tables& tables) {
    ShutdownPortableRuntime();const auto initialized=InitializePortableRuntime(tables.scripts);
    Require(initialized.passed,"Sound fixture initialization failed");const auto loaded=LoadPortableRuntimeMap(tables.map);
    Require(loaded.passed,"Sound fixture native Level map failed");
}
QuestVr::Vm::Result Call(const std::string& actor,const std::string& function,const std::vector<Evaluation>& arguments={}) {
    const auto result=ExecutePortableActorFunction(Path(actor),function,arguments);
    Require(result.passed()&&result.committed,actor+'.'+function+" failed: "+result.error+" at "+result.function+':'+std::to_string(result.offset));return result;
}
Value Read(const std::string& actor,const std::string& field) {return ReadPortableActorScriptProperty(Path(actor),field);}
void Same(const Value& actual,const Value& expected,const std::string& label) {Require(QuestVr::Vm::Equal(actual,expected),label);}
std::uint32_t Channel(const QuestVr::Vm::Result& result) {
    Require(result.value.kind==Kind::Int,"PlaySound did not write its native Int channel");return static_cast<std::uint32_t>(result.value.integer);
}
QuestVr::Vm::Result Refused(Fixture& fixture,const std::string& actor,const std::string& function,const std::vector<Evaluation>& args={}) {
    const auto bytes=fixture.Snapshot("BeforeRefusal");const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    const auto count=GetPortableRuntimeMapActors(true).size();const auto result=ExecutePortableActorFunction(Path(actor),function,args);const auto afterGc=GC::GetStats();
    Require(!result.passed()&&!result.committed,"Invalid sound call committed: "+actor+'.'+function);
    Require(gc.numObjects==afterGc.numObjects&&gc.memoryUsage==afterGc.memoryUsage,"Refused sound call leaked provisional UObjects");
    Require(fixture.Snapshot("AfterRefusal")==bytes&&GetPortableRuntimeWorldRevision()==revision&&GetPortableRuntimeMapActors(true).size()==count,
        "Refused sound call retained callback/argument writes, save or revision changes");++refusals;fixture.Unchanged();return result;
}
void Listen(const std::string& first,const std::string& second="") {
    Call("Level0","SetHead",{Object(first)});if(!first.empty()) {Call(first,"SetIsPlayer",{Bool(true)});Call(first,"SetNext",{Object(second)});}
    if(!second.empty()) {Call(second,"SetIsPlayer",{Bool(true)});Call(second,"SetNext",{Object("")});}
}
void Packet(const std::string& listener,std::uint32_t id,const std::string& source="Driver",const std::string& sound="SoundA",
    Point location={0,0,0},Point parameters={100,800,100}) {
    Same(Read(listener,"Heard"),Value::Integer(1),"Native sound event did not execute exactly once: "+listener);
    Same(Read(listener,"SeenActor"),Object(source).value,"Sound event source Actor is wrong");
    Same(Read(listener,"SeenSound"),Object(sound).value,"Sound event asset is wrong");
    Same(Read(listener,"SeenId"),Value::Integer(static_cast<std::int32_t>(id)),"Sound event channel differs from native return");
    Same(Read(listener,"SeenLocation"),Value::Vector(location),"Sound event source location is wrong");
    Same(Read(listener,"SeenParameters"),Value::Vector(parameters),"Sound event packed parameter vector is wrong");
}

void ZeroRecipientsAndIdentity(Fixture& fixture,const Tables& tables) {
    Initialize(tables);const auto baseline=fixture.Snapshot("NoRecipients");const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    const auto id=Channel(Call("Driver","Play"));Require(id!=0u,"Real source UObject has a reserved None sound channel");
    Require(Channel(Call("Driver","Play"))==id&&Channel(Call("Driver","PlayOpcode"))==id&&Channel(Call("Driver","PlayVirtual"))==id,
        "Repeated/final/native/virtual sound calls changed the source UObject channel");
    Require(Channel(Call("Driver","PlaySimulated"))==id,"No-client simulated local sound did not return the same stable channel");
    const auto otherId=Channel(Call("Other","Play"));Require(otherId!=id,"Distinct live source UObjects share a sound channel");
    Require(Channel(Call("Driver","PlayOther"))==otherId,"Context used FFrame.Object rather than opcode receiver for channel identity");
    const auto slotZero=Channel(Call("Driver","PlaySound",{Object("SoundA"),{Value::Byte(0),{}}}));
    Require(slotZero!=0u&&id-slotZero==2u,"Default Slot is not SLOT_Misc1 or source index is not shifted by four");
    for(unsigned slot=0u;slot<256u;++slot) for(bool noOverride:{false,true}) {
        const auto channel=Channel(Call("Driver","PlaySound",{Object("SoundA"),{Value::Byte(static_cast<std::uint8_t>(slot)),{}},
            {Value::Float(1),{}},Bool(noOverride)}));
        Require(channel-slotZero==2u*slot+static_cast<unsigned>(noOverride),"Channel arithmetic normalized Slot or lost bNoOverride");
    }
    const auto afterGc=GC::GetStats();Require(fixture.Snapshot("AfterNoRecipients")==baseline&&GetPortableRuntimeWorldRevision()==revision&&
        afterGc.numObjects==gc.numObjects&&afterGc.memoryUsage==gc.memoryUsage,"Zero-recipient sound changed save/revision/UObject graph");
    Listen("First","Second");Call("First","SetIsPlayer",{Bool(false)});Call("Second","SetIsPlayer",{Bool(false)});
    const auto falseChain=fixture.Snapshot("FalsePawnChain");Require(Channel(Call("Driver","Play"))==id,"Nonplayer Pawn chain changed sound channel");
    Require(fixture.Snapshot("AfterFalsePawnChain")==falseChain,"bIsPlayer=false Pawn chain fabricated ClientHearSound delivery");
    Refused(fixture,"Orphan","Play");Refused(fixture,"Driver","PlayThenFail");
    for(std::uint8_t mode:{1u,2u,3u,255u}) {Call("Level0","SetNetMode",{{Value::Byte(mode),{}}});Refused(fixture,"Driver","Play");}
    fixture.Unchanged();
}

void GenuineRecipientsAndRollback(Fixture& fixture,const Tables& tables) {
    Initialize(tables);Listen("First","Second");const auto id=Channel(Call("Driver","Play"));Packet("First",id);Packet("Second",id);
    Initialize(tables);Listen("First");const auto full=Channel(Call("Driver","PlayFull"));Packet("First",full,"Driver","SoundA",{0,0,0},{75,512,125});
    Initialize(tables);Listen("First");const auto other=Channel(Call("Driver","PlayOther"));Packet("First",other,"Other","SoundA",{10,0,0});
    // The sound argument still belongs to Driver's FFrame.Object (SoundA),
    // while the native source/default location and index belong to Other.
    Initialize(tables);Listen("Splicer","First");Call("Splicer","SetReplacement",{Object("Second")});Call("Second","SetIsPlayer",{Bool(true)});
    const auto spliced=Channel(Call("Driver","Play"));Packet("Splicer",spliced);Packet("Second",spliced);
    Same(Read("First","Heard"),Value::Integer(0),"Sound recipient loop snapshotted nextPawn before callback mutation");
    Initialize(tables);Listen("Mover","Second");const auto moved=Channel(Call("Driver","Play"));Packet("Mover",moved);
    Same(Read("Driver","Location"),Value::Vector({4000,0,0}),"Sound callback did not synchronously mutate the real source Actor");
    Same(Read("Second","Heard"),Value::Integer(0),"Subsequent listener did not reread the source Location/range after callback");
    Initialize(tables);Listen("First","Failure");const auto failed=Refused(fixture,"Driver","Play");
    Require(failed.error.find("999")!=std::string::npos,"Callback rollback fixture stopped before its intentional late unsupported native");
    Same(Read("First","Heard"),Value::Integer(0),"Later callback refusal retained an earlier listener's writes");
    Initialize(tables);Listen("First");Refused(fixture,"Driver","PlayThenFail");Same(Read("First","Heard"),Value::Integer(0),"Caller late failure retained sound callback effects");
    Initialize(tables);Listen("BasePawn");Call("Driver","Play");
    Initialize(tables);Listen("PlainPlayer");Call("Driver","Play"); // Actual native ClientHearSound has no bound Player/Viewport: real no-op.
    fixture.Unchanged();
}

void CallerAndOwnership(Fixture& fixture,const Tables& tables) {
    Initialize(tables);Listen("First");const auto baseline=fixture.Snapshot("LocalBaseline");
    const auto id=Channel(Call("Driver","PlaySimulated"));Require(fixture.Snapshot("LocalAfter")==baseline,"Simulated caller incorrectly used nonlocal PawnList");
    const auto normal=Channel(Call("Driver","Play"));Require(normal==id,"Caller selection changed channel identity");Packet("First",normal);
    Initialize(tables);Listen("First");Call("Driver","GotoState",{{Value::Text(Kind::Name,"Speaking"),{}}});
    const auto resumed=ResumePortableActorState(Path("Driver"));Require(resumed.passed()&&resumed.committed,"Original State caller sound failed: "+resumed.error);
    const auto stateId=Read("Driver","StateChannel");Require(stateId.kind==Kind::Int&&stateId.integer!=0,"State program did not retain returned sound channel");
    Packet("First",static_cast<std::uint32_t>(stateId.integer));
    // Native NetMode comes from immutable XLevel.Actors[0], but the nonlocal
    // PawnList head really does come through the current reflected Actor.Level.
    Initialize(tables);Listen("First");Call("Driver","SetLevel",{Object("AlternateInfo")});Call("Driver","SetXLevel",{Object("")});
    const auto alternateBaseline=fixture.Snapshot("AlternateLevel");Channel(Call("Driver","Play"));
    Require(fixture.Snapshot("AlternateAfter")==alternateBaseline,"Sound used native PawnList instead of real reflected Level head, or foreign reflected NetMode3 redirected native ownership");
    Call("AlternateInfo","SetHead",{Object("Second")});Call("Second","SetIsPlayer",{Bool(true)});
    const auto alternate=Channel(Call("Driver","Play"));Packet("Second",alternate);Same(Read("First","Heard"),Value::Integer(0),"Sound selected native LevelInfo PawnList over reflected Level");
    Call("Driver","SetLevel",{Object("")});Refused(fixture,"Driver","Play");
    // None sound leaves return bytes untouched before ownership/routing work.
    const auto silent=Call("Driver","PlaySound",{Object("")});Require(silent.value.kind==Kind::Nothing,"PlaySound(None) wrote a synthetic zero return");
    Initialize(tables);Listen("First");auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,17u);
    const auto emptyClient=fixture.Snapshot("EmptyClient");Channel(Call("Driver","PlaySimulated"));
    Require(fixture.Snapshot("AfterEmptyClient")==emptyClient,"Client with zero viewports was not legitimate empty local selection");
    RegisterPortableRuntimeViewport(17u,{31u,1024u,1024u,3u,2u});Refused(fixture,"Driver","PlaySimulated");
    const auto prepared=PreparePortableRuntimePlayerObjects(31u);Require(!prepared.bound&&prepared.playerPath.empty(),"Viewport preparation fabricated a PlayerPawn");
    const auto unbound=fixture.Snapshot("UnboundViewport");Channel(Call("Driver","PlaySimulated"));
    Require(fixture.Snapshot("AfterUnboundViewport")==unbound,"Actual unbound native viewport fabricated local recipient effects");
    lifetime->Revoke();Refused(fixture,"Driver","PlaySimulated");fixture.Unchanged();
}

void BlockedBspHearing() {
    // The actual serialized zero-node Model has RootOutside=false, so its
    // genuine FastLineCheck refuses every ray. This isolates retail sound
    // occlusion without inventing a fake visibility result or collision shape.
    Fixture fixture;const auto tables=Build(fixture,false);Initialize(tables);Listen("First","Second");
    const auto channel=Channel(Call("Driver","Play"));
    Packet("First",channel,"Driver","SoundA",{0,0,0},{100,280,100});
    Packet("Second",channel,"Driver","SoundA",{0,0,0},{100,280,100});
    // Packed parameters are copied per recipient: the second radius must not
    // compound another factor .35 onto the first listener's attenuation.
    Initialize(tables);Listen("First");Call("First","SetLocation",{{Value::Vector({630,0,0}),{}}});
    Call("Driver","Play");Same(Read("First","Heard"),Value::Integer(0),"Occluded noninstigator bypassed secondary hearing range");
    Call("Driver","SetOwner",{Object("First")});Call("Driver","Play");
    Same(Read("First","Heard"),Value::Integer(0),"Owner incorrectly supplied the source Instigator range exemption");
    Call("Driver","SetInstigator",{Object("First")});const auto exempt=Channel(Call("Driver","Play"));
    Packet("First",exempt,"Driver","SoundA",{0,0,0},{100,280,100});
    // Instigator bypasses only secondary range reduction, not the strict
    // initial range test or the .35 packed Radius attenuation.
    Call("First","SetLocation",{{Value::Vector({710,0,0}),{}}});Call("Driver","Play");
    Same(Read("First","Heard"),Value::Integer(1),"Instigator incorrectly bypassed the strict initial hearing range");
    fixture.Unchanged();
}

struct ScriptEnvelope {
    Bytes prefix;
    QuestVr::ScriptSavedState state;
};
ScriptEnvelope ClockEnvelope(const Bytes& bytes) {
    std::size_t cursor{};
    const auto word=[&]() {
        Require(cursor<=bytes.size()&&bytes.size()-cursor>=4u,"Clock envelope word exceeds input");
        std::uint32_t value{};for(unsigned byte=0u;byte<4u;++byte) value|=static_cast<std::uint32_t>(bytes[cursor++])<<(byte*8u);
        return value;
    };
    const auto skip=[&](std::size_t size) {Require(cursor<=bytes.size()&&size<=bytes.size()-cursor,"Clock envelope field exceeds input");cursor+=size;};
    const auto string=[&]() {const auto size=word();Require(size<=1'048'576u,"Clock envelope string exceeds independent bound");skip(size);};
    const auto strings=[&]() {const auto count=word();Require(count<=100'000u,"Clock envelope list exceeds independent bound");for(std::uint32_t index=0u;index<count;++index) string();};
    Require(word()==0x53515844u,"Clock envelope magic differs");
    Require(word()==16u,"Native level clock did not select exact runtime envelope16");
    strings();strings();strings();skip(4u);const auto damaged=word();
    Require(damaged<=100'000u,"Clock damaged-prefix count exceeds independent bound");
    for(std::uint32_t index=0u;index<damaged;++index) {string();skip(4u);}
    skip(8u);strings();strings();strings();strings();
    ScriptEnvelope envelope;envelope.prefix.assign(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(cursor));
    const auto length=word();Require(length>=8u&&length==bytes.size()-cursor,"Clock script blob does not cover exact envelope tail");
    const Bytes blob(bytes.begin()+static_cast<std::ptrdiff_t>(cursor),bytes.end());
    Require(blob[6u]==11u,"Native level clock did not select exact script codec11");
    envelope.state=QuestVr::DecodeScriptSavedState(blob);return envelope;
}
Bytes WrapClockEnvelope(const ScriptEnvelope& envelope,const QuestVr::ScriptSavedState& state,std::uint32_t version) {
    Bytes bytes=envelope.prefix;Replace32(bytes,4u,version);const auto blob=QuestVr::EncodeScriptSavedState(state);
    U32(bytes,static_cast<std::uint32_t>(blob.size()));Append(bytes,blob);return bytes;
}
std::uint64_t DoubleBits(double value) {return std::bit_cast<std::uint64_t>(value);}
std::uint32_t FloatBits(float value) {return std::bit_cast<std::uint32_t>(value);}
void ClockRefused(Fixture& fixture,float elapsed,const QuestVr::Vm::Limits& limits={}) {
    const auto before=fixture.Snapshot("ClockBeforeRefusal");const auto time=ReadPortableRuntimeLevelTime();
    const auto mirror=Read("Level0","TimeSeconds");const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    const auto result=AdvancePortableRuntimeLevelClock(elapsed,limits);const auto after=GC::GetStats();
    Require(!result.passed()&&!result.committed,"Invalid native level clock phase committed");
    Require(DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(time)&&QuestVr::Vm::Equal(Read("Level0","TimeSeconds"),mirror)&&
        GetPortableRuntimeWorldRevision()==revision&&after.numObjects==gc.numObjects&&after.memoryUsage==gc.memoryUsage&&
        fixture.Snapshot("ClockAfterRefusal")==before,"Clock refusal changed native/mirror/complete save/revision/GC state");++refusals;
}
void NativeLevelClock(Fixture& fixture,const Tables& tables) {
    Initialize(tables);Require(DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(0.0),"New actual native ULevel time is not zero");
    Same(Read("Level0","TimeDilation"),Value::Float(1.0f),"Authored native LevelInfo dilation is not one");
    const auto gc=GC::GetStats();auto revision=GetPortableRuntimeWorldRevision();
    auto expected=QuestVr::AdvanceOriginalLevelClock(0.0,.1f,1.0f);
    const auto first=AdvancePortableRuntimeLevelClock(.1f);
    Require(first.passed()&&first.committed&&first.writes==1u,"First actual native level clock phase failed/write was unbudgeted: "+first.error);
    Require(DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(expected.timeSeconds)&&
        FloatBits(Read("Level0","TimeSeconds").floating)==FloatBits(expected.timeMirror),"Native Double/mirror clock differs from exact pure audited phase");
    Require(GetPortableRuntimeWorldRevision()==revision,"Clock-only phase republished world geometry");
    const auto baseline=fixture.Snapshot("ClockFirst");const auto envelope=ClockEnvelope(baseline);
    Require(envelope.state.nativeLevelClock&&envelope.state.nativeLevelClock->levelPath==Path("MyLevel")&&
        DoubleBits(envelope.state.nativeLevelClock->timeSeconds)==DoubleBits(expected.timeSeconds),"Codec11 native clock owner/bits differ from actual ULevel");
    // An ordinary script write changes only the reflected mirror. It is never
    // native double-clock authority, even if the Float value is much larger.
    Call("Level0","SetTimeSeconds",{{Value::Float(12345.0f),{}}});
    Require(DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(expected.timeSeconds),"Reflected TimeSeconds redirected native Double authority");
    revision=GetPortableRuntimeWorldRevision();expected=QuestVr::AdvanceOriginalLevelClock(expected.timeSeconds,.03125f,1.0f);
    const auto second=AdvancePortableRuntimeLevelClock(.03125f);Require(second.passed()&&second.committed,"Second native clock phase failed");
    Require(DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(expected.timeSeconds)&&
        FloatBits(Read("Level0","TimeSeconds").floating)==FloatBits(expected.timeMirror)&&GetPortableRuntimeWorldRevision()==revision,
        "Native advance did not replace the script mirror without republishing geometry");
    Call("Level0","SetTimeDilation",{{Value::Float(1.7f),{}}});
    Call("Driver","SetLevel",{Object("AlternateInfo")});Call("Driver","SetXLevel",{Object("")});
    Call("Level0","SetLevel",{Object("AlternateInfo")});Call("Level0","SetXLevel",{Object("")});
    Call("AlternateInfo","SetTimeDilation",{{Value::Float(33.0f),{}}});
    for(const auto delta:{0.0f,.0001f,.019f,.11f,.8f}) {
        const auto previous=expected.timeSeconds;revision=GetPortableRuntimeWorldRevision();
        expected=QuestVr::AdvanceOriginalLevelClock(previous,delta,1.7f);
        const auto result=AdvancePortableRuntimeLevelClock(delta);Require(result.passed()&&result.committed,"Dilated native clock phase failed: "+result.error);
        Require(DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(expected.timeSeconds)&&
            FloatBits(Read("Level0","TimeSeconds").floating)==FloatBits(expected.timeMirror)&&GetPortableRuntimeWorldRevision()==revision,
            "Native clock used reflected Actor.Level/XLevel or altered audited dilation/clamp ordering");
    }
    const auto saved=fixture.Snapshot("ClockWarm");const auto savedTime=ReadPortableRuntimeLevelTime();const auto savedMirror=Read("Level0","TimeSeconds");
    const auto changed=AdvancePortableRuntimeLevelClock(.25f);Require(changed.passed()&&DoubleBits(ReadPortableRuntimeLevelTime())!=DoubleBits(savedTime),"Clock mutation before warm restore failed");
    Require(ValidatePortableRuntimeState((fixture.directory/"ClockWarm.sav").string(),"SoundFixture"),"Clock checkpoint read-only validation refused");
    const auto beforeValidate=fixture.Snapshot("ClockValidationCurrent");
    Require(ValidatePortableRuntimeState((fixture.directory/"ClockWarm.sav").string(),"SoundFixture")&&
        fixture.Snapshot("ClockValidationAfter")==beforeValidate,"Clock checkpoint validation published native state");
    Require(LoadPortableRuntimeState((fixture.directory/"ClockWarm.sav").string())&&DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(savedTime)&&
        QuestVr::Vm::Equal(Read("Level0","TimeSeconds"),savedMirror)&&fixture.Snapshot("ClockWarmRestored")==saved,
        "Warm checkpoint did not restore exact native Double/mirror state");
    Initialize(tables);Require(ReadPortableRuntimeLevelTime()==0.0,"Cold fixture did not own a fresh native Level clock");
    Require(LoadPortableRuntimeState((fixture.directory/"ClockWarm.sav").string())&&DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(savedTime)&&
        fixture.Snapshot("ClockColdRestored")==saved,"Cold checkpoint did not restore complete native clock exactly");
    const auto withClock=ClockEnvelope(saved);auto legacyState=withClock.state;legacyState.nativeLevelClock.reset();
    // With no other optional state domains this property-only fixture is exact
    // pre-clock envelope4/codec1. The reflected mirror intentionally remains.
    const auto legacy=WrapClockEnvelope(withClock,legacyState,4u);const auto legacyPath=fixture.directory/"ClockLegacy.sav";WriteBytes(legacyPath,legacy);
    Require(LoadPortableRuntimeState(legacyPath.string())&&DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(0.0)&&
        QuestVr::Vm::Equal(Read("Level0","TimeSeconds"),savedMirror),"Legacy clock absence merged live native time or inferred reflected mirror");
    Require(LoadPortableRuntimeState((fixture.directory/"ClockWarm.sav").string()),"Clock checkpoint restore after legacy reset failed");
    for(const auto elapsed:{-.01f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) ClockRefused(fixture,elapsed);
    QuestVr::Vm::Limits noWrites;noWrites.writes=0u;
    ClockRefused(fixture,.03125f,noWrites); // Native Double changes first, then VM mirror write budget refuses: restore BOTH.
    // Malformed native-clock values cannot publish anything, including a
    // valid preceding reflected-property block in the same checkpoint.
    for(const auto bad:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-1.0}) {
        auto malformed=saved;const auto old=std::bit_cast<std::uint64_t>(savedTime),replacement=std::bit_cast<std::uint64_t>(bad);
        bool replaced{};for(std::size_t offset=malformed.size();offset>=8u;--offset) {
            std::uint64_t bits{};for(unsigned byte=0u;byte<8u;++byte) bits|=std::uint64_t(malformed[offset-8u+byte])<<(8u*byte);
            if(bits!=old) continue;
            for(unsigned byte=0u;byte<8u;++byte) malformed[offset-8u+byte]=static_cast<std::uint8_t>(replacement>>(8u*byte));
            replaced=true;break;
        }
        Require(replaced,"Could not independently locate native-clock tail Double bits");
        const auto path=fixture.directory/"ClockInvalid.sav";WriteBytes(path,malformed);const auto current=fixture.Snapshot("ClockBeforeInvalidSave");
        Require(!ValidatePortableRuntimeState(path.string(),"SoundFixture")&&!LoadPortableRuntimeState(path.string())&&
            fixture.Snapshot("ClockAfterInvalidSave")==current&&DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(savedTime),
            "Malformed clock checkpoint retained a partial native/reflected restore");++refusals;
    }
    auto wrongOwner=withClock.state;wrongOwner.nativeLevelClock->levelPath=Path("Other");
    const auto wrongOwnerPath=fixture.directory/"ClockWrongOwner.sav";
    WriteBytes(wrongOwnerPath,WrapClockEnvelope(withClock,wrongOwner,16u));const auto beforeOwnerRefusal=fixture.Snapshot("ClockBeforeWrongOwner");
    Require(!ValidatePortableRuntimeState(wrongOwnerPath.string(),"SoundFixture")&&!LoadPortableRuntimeState(wrongOwnerPath.string())&&
        fixture.Snapshot("ClockAfterWrongOwner")==beforeOwnerRefusal&&DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(savedTime),
        "Clock checkpoint owner not the actual serialized native Level was accepted");++refusals;
    auto doubleRound=withClock.state;
    const double midpoint=1.0+std::ldexp(1.0,-24);doubleRound.nativeLevelClock->timeSeconds=midpoint;
    const auto midpointPath=fixture.directory/"ClockMidpoint.sav";
    WriteBytes(midpointPath,WrapClockEnvelope(withClock,doubleRound,16u));
    Require(LoadPortableRuntimeState(midpointPath.string()),"Exact midpoint native-clock fixture failed restoration");
    Call("Level0","SetTimeDilation",{{Value::Float(1.0f),{}}});revision=GetPortableRuntimeWorldRevision();
    const auto doubleRoundExpected=QuestVr::AdvanceOriginalLevelClock(midpoint,std::ldexp(1.0f,-63),1.0f);
    const auto midpointAdvance=AdvancePortableRuntimeLevelClock(std::ldexp(1.0f,-63));
    Require(midpointAdvance.passed()&&midpointAdvance.committed&&DoubleBits(ReadPortableRuntimeLevelTime())==DoubleBits(doubleRoundExpected.timeSeconds)&&
        FloatBits(Read("Level0","TimeSeconds").floating)==FloatBits(doubleRoundExpected.timeMirror)&&
        FloatBits(doubleRoundExpected.timeMirror)!=FloatBits(static_cast<float>(doubleRoundExpected.timeSeconds))&&
        GetPortableRuntimeWorldRevision()==revision,"Native mirror reloaded the Double spill instead of consuming retained PC64 sum");
    Require(LoadPortableRuntimeState((fixture.directory/"ClockWarm.sav").string()),"Clock restore after retained-mirror boundary failed");
    auto oversized=withClock.state;oversized.nativeLevelClock->timeSeconds=std::numeric_limits<double>::max();
    const auto hugePath=fixture.directory/"ClockHuge.sav";WriteBytes(hugePath,WrapClockEnvelope(withClock,oversized,16u));
    if(LoadPortableRuntimeState(hugePath.string())) {
        ClockRefused(fixture,.01f); // Float mirror overflow is refused atomically.
    } else ++refusals; // Preflight may reject an unrepresentable mirror earlier.
    Require(LoadPortableRuntimeState((fixture.directory/"ClockWarm.sav").string()),"Clock final restore failed");
    const auto after=GC::GetStats();Require(after.numObjects==gc.numObjects&&after.memoryUsage==gc.memoryUsage,"Native level clock fixture leaked UObject roots");
    fixture.Unchanged();
}
} // namespace

int main() {
    try {
        const auto baseline=GC::GetStats().numObjects;Fixture fixture;const auto tables=Build(fixture);
        ZeroRecipientsAndIdentity(fixture,tables);GenuineRecipientsAndRollback(fixture,tables);CallerAndOwnership(fixture,tables);
        BlockedBspHearing();NativeLevelClock(fixture,tables);
        ShutdownPortableRuntime();GC::Collect();Require(GC::GetStats().numObjects==baseline,"Sound routing fixture leaked rooted UObjects");
        std::cout<<"PASS PlaySound264 production routing: "<<checks<<" checks, "<<refusals
            <<" refusal controls; real UObject channels, caller/native Level ownership, live PawnList callbacks and complete rollback."
              " No rendered/audio-queue or complete-crate gameplay claim.\n";return 0;
    } catch(const std::exception& error) {ShutdownPortableRuntime();std::cerr<<"Sound routing integration failed: "<<error.what()<<'\n';return 1;}
}
