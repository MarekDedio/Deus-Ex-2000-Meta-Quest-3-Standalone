#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "portable_process_loop.h"
#include "quest_level_clock.h"
#include "quest_runtime_script_packages.h"
#include "quest_script_state.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

// Generated real v68 packages exercise production metadata, bytecode, native
// routing and transactional callbacks. The owned hybrid variant uses genuine
// native Engine/Client/Viewport ownership with explicit generated PlayerPawn
// subclasses and their test Possess/ClientHearSound overrides. It does not
// establish original Login/Possess/startup or audible output. No retail
// executable, synthetic renderer/audio output or manual callback dispatch.
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
Code Text(const std::string& value) {
    Code code{{0x1fu},value.size()+2u};
    code.raw.insert(code.raw.end(),value.begin(),value.end());code.raw.push_back(0u);return code;
}
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
        std::int32_t core,std::int32_t target=0,std::uint32_t flags=0u,std::uint32_t dimension=1u,
        std::int32_t secondary=0) {
        const auto reference=Export(name,Import(type,core),0,owner,true);
        builders.at(static_cast<std::size_t>(reference-1))=[type,target,flags,dimension,secondary](std::int32_t next,std::int32_t) {
            Bytes body{0u};Index(body,0);Index(body,next);U32(body,dimension);U32(body,flags);Index(body,0);
            if(flags&0x20u) U16(body,0u); // CPF_Net replication offset precedes subtype data.
            if(type=="ObjectProperty"||type=="StructProperty"||type=="ByteProperty") Index(body,target);
            else if(type=="ClassProperty") {Index(body,target);Index(body,secondary);}
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

Tables Build(Fixture& fixture,bool outside=true,bool ownedEngine=false,bool startup=false) {
    Require(!startup||ownedEngine,"Startup Client fixture requires original Engine data");
    PortablePackageTables coreTable,engineTable;
    if(!ownedEngine) {
    Package core;core.stem="Core";const auto cp=core.ImportPackage("Core");
    const auto object=core.Export("Object"),meta=core.Export("Class",0,object);
    core.Property("Class","ClassProperty",object,cp,meta);core.Property("Name","NameProperty",object,cp);
    core.Property("Outer","ObjectProperty",object,cp,object);core.Property("ObjectFlags","IntProperty",object,cp);
    const auto vector=core.Export("Vector",core.Import("Struct",cp),0,object),rotator=core.Export("Rotator",core.Import("Struct",cp),0,object);
    for(const auto* name:{"X","Y","Z"}) core.Property(name,"FloatProperty",vector,cp);
    for(const auto* name:{"Pitch","Yaw","Roll"}) core.Property(name,"IntProperty",rotator,cp);
    core.ClassBody(object);core.ClassBody(meta);core.StructBody(vector);core.StructBody(rotator);coreTable=fixture.Write(core);

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
    engineTable=fixture.Write(engine);
    }

    Package classes;classes.stem="SoundClasses";const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine");
    const auto ca=classes.Import("Actor",ce),cpawn=classes.Import("Pawn",ce);
    const auto cpa=ownedEngine?classes.Import("PlayerPawn",ce):cpawn;
    const auto cs=classes.Import("Sound",ce),co=classes.Import("Object",cc);
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
    const auto importedNext=classes.Import("nextPawn",cpawn,"ObjectProperty"),importedLocation=classes.Import("Location",ca,"StructProperty");
    const auto replace=classes.Function("SetReplacement",listener,cc),replaceInput=classes.Property("Input","ObjectProperty",replace,cc,cpa,0x80u);
    classes.FunctionCode(replace,Join({Assign(Ref(0x01u,replacement),Ref(0x00u,replaceInput)),Return()}));
    if(ownedEngine) {
        // This is an explicit test UClass override, not a skipped retail
        // callback or a flag claiming original world/player startup completed.
        // SetPortableRuntimePlayer must genuinely select and execute it.
        const auto possessions=classes.Property("Possessions","IntProperty",listener,cc);
        classes.Function("Possess",listener,cc,
            Join({Assign(Ref(0x01u,possessions),Native(146u,{Ref(0x01u,possessions),Integer(1)})),Return()}),0u,0x802u);
    }
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
    if(startup) {
        // Explicit generated lifecycle/Login overrides isolate native startup
        // Client semantics. They do not replace retail callbacks in production.
        const auto gameInfo=classes.Import("GameInfo",ce),levelInfo=classes.Import("LevelInfo",ce);
        const auto classType=classes.Import("Class",cc),playerPawn=classes.Import("PlayerPawn",ce);
        const auto worldLevel=classes.Export("WorldLevel",0,levelInfo),worldProbe=classes.Export("WorldProbe",0,probe);
        const auto loginPawn=classes.Export("LoginPawn",0,playerPawn);classes.ClassBody(loginPawn);
        const auto actorLevel=classes.Import("Level",ca,"ObjectProperty");
        const auto defaultGame=classes.Import("DefaultGameType",levelInfo,"ClassProperty");
        const auto gameField=classes.Import("Game",levelInfo,"ObjectProperty");
        for(const auto& [name,field,type,target]:std::vector<std::tuple<std::string,std::int32_t,std::string,std::int32_t>>{
            {"SetWorldGameType",defaultGame,"ClassProperty",classType},{"SetWorldGame",gameField,"ObjectProperty",gameInfo}}) {
            const auto fn=classes.Function(name,worldProbe,cc);
            const auto input=classes.Property("Input",type,fn,cc,target,0x80u,1u,type=="ClassProperty"?gameInfo:0);
            classes.FunctionCode(fn,Join({Assign(Context(Ref(0x01u,actorLevel),Ref(0x01u,field)),Ref(0x00u,input)),Return()}));
        }
        for(auto cls:{worldLevel,worldProbe}) {
            for(const auto* event:{"PreBeginPlay","BeginPlay","PostBeginPlay","SetInitialState","PostPostBeginPlay"})
                classes.Function(event,cls,cc,Return(),0u,0x802u);
            classes.ClassBody(cls);
        }
        for(const auto* name:{"LocalGame","ServerGame","OverrideGame","FailureGame","MalformedGame"}) {
            const auto game=classes.Export(name,0,gameInfo);
            for(const auto* event:{"PreBeginPlay","BeginPlay","PostBeginPlay","SetInitialState","PostPostBeginPlay"})
                classes.Function(event,game,cc,Return(),0u,0x802u);
            const auto init=classes.Function("InitGame",game,cc,Return(),0u,0x802u);
            classes.Property("Options","StrProperty",init,cc,0,0x400080u);
            classes.Property("Error","StrProperty",init,cc,0,0x400180u);
            const auto seenClass=classes.Property("LoginClass","ClassProperty",game,cc,classType,0u,1u,playerPawn);
            const auto draw=classes.Property("LoginDraw","IntProperty",game,cc);
            const auto login=classes.Function("Login",game,cc,{},0u,0x802u);
            const auto portal=classes.Property("Portal","StrProperty",login,cc,0,0x400080u);
            const auto options=classes.Property("Options","StrProperty",login,cc,0,0x400080u);
            const auto error=classes.Property("Error","StrProperty",login,cc,0,0x400180u);
            const auto selected=classes.Property("SpawnClass","ClassProperty",login,cc,classType,0x80u,1u,
                std::string_view(name)=="MalformedGame"?ca:playerPawn);
            classes.Property("ReturnValue","ObjectProperty",login,cc,playerPawn,0x580u);
            Code body=Join({Assign(Ref(0x01u,seenClass),Ref(0x00u,selected)),
                Assign(Ref(0x01u,draw),Native(167u,{Integer(1000)})),
                Assign(Ref(0x00u,error),Text("small")),Assign(Ref(0x00u,error),Text(std::string(512u,'L')))});
            for(unsigned piece=1u;piece<8u;++piece)
                body=Join({body,Assign(Ref(0x00u,error),Native(112u,{Ref(0x00u,error),Text(std::string(512u,'L'))}))});
            body=Join({body,Assign(Ref(0x00u,error),Native(112u,{Ref(0x00u,error),Ref(0x00u,portal)})),
                Assign(Ref(0x00u,error),Native(112u,{Ref(0x00u,error),Ref(0x00u,options)}))});
            if(std::string_view(name)=="FailureGame")body=Join({body,Native(999u)});
            classes.FunctionCode(login,Join({body,Return(Token(0x2au))}),0u,0x802u);
            classes.ClassBody(game);
        }
    }
    const auto classesTable=fixture.Write(classes);

    Package map;map.stem="SoundFixture";const auto me=map.ImportPackage("Engine"),mc=map.ImportPackage("SoundClasses");
    const auto world=map.Export("MyLevel",map.Import("Level",me)),worldModel=map.Export("WorldModel",map.Import("Model",me));
    const auto soundA=map.Export("SoundA",map.Import("Sound",me)),soundB=map.Export("SoundB",map.Import("Sound",me));
    map.ActorBody(soundA,{});map.ActorBody(soundB,{});
    const auto levelActor=map.Export("Level0",map.Import(startup?"WorldLevel":"LevelInfo",startup?mc:me)),alternateInfo=map.Export("AlternateInfo",map.Import("LevelInfo",me));
    const auto driver=map.Export("Driver",map.Import(startup?"WorldProbe":"Probe",mc)),other=map.Export("Other",map.Import("Probe",mc));
    const auto first=map.Export("First",map.Import("Listener",mc)),second=map.Export("Second",map.Import("Listener",mc));
    const auto third=map.Export("Third",map.Import("Listener",mc)),splice=map.Export("Splicer",map.Import("SpliceListener",mc));
    const auto move=map.Export("Mover",map.Import("MovingListener",mc)),failure=map.Export("Failure",map.Import("FailingListener",mc));
    const auto basePawn=map.Export("BasePawn",map.Import("Pawn",me)),plainPlayer=map.Export("PlainPlayer",map.Import("PlayerPawn",me));
    const auto orphan=map.Export("Orphan",map.Import("Probe",mc));
    const std::vector<std::int32_t> actors=startup?std::vector<std::int32_t>{levelActor,driver}:
        std::vector<std::int32_t>{levelActor,driver,other,first,second,third,splice,move,failure,basePawn,plainPlayer};
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
            map.BoolTag(tags,"bBegunPlay",!startup);map.ByteTag(tags,"NetMode",reference==alternateInfo?3u:0u);
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
    map.Body(worldModel,EmptyModel(outside));
    std::vector<PortablePackageTables> scripts;
    if(!ownedEngine) {scripts.push_back(std::move(coreTable));scripts.push_back(std::move(engineTable));}
    scripts.push_back(classesTable);return {std::move(scripts),fixture.Write(map,true)};
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
    const auto missingEngine=Refused(fixture,"Driver","PlaySimulated");
    Require(missingEngine.error.find("Actual Engine.Client")!=std::string::npos,
        "Generated local call did not require genuine native Level.Engine authority");
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
    const auto local=Refused(fixture,"Driver","PlaySimulated");
    Require(local.error.find("Actual Engine.Client")!=std::string::npos&&fixture.Snapshot("LocalAfter")==baseline,
        "Simulated caller incorrectly used nonlocal PawnList or accepted missing native Engine");
    const auto normal=Channel(Call("Driver","Play"));Packet("First",normal);
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
    const auto noEngine=Refused(fixture,"Driver","PlaySimulated");
    Require(noEngine.error.find("Actual Engine.Client")!=std::string::npos,"A render-owner token fabricated native Engine.Client authority");
    RegisterPortableRuntimeViewport(17u,{31u,1024u,1024u,3u,2u});Refused(fixture,"Driver","PlaySimulated");
    const auto prepared=PreparePortableRuntimePlayerObjects(31u);Require(!prepared.bound&&prepared.playerPath.empty(),"Viewport preparation fabricated a PlayerPawn");
    const auto detached=Refused(fixture,"Driver","PlaySimulated");
    Require(detached.error.find("Actual Engine.Client")!=std::string::npos,
        "Constructor-only viewport bypassed missing actual Engine/Client initialization");
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

std::uint64_t SourceHash(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint owned sound source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t total{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        total+=static_cast<std::uintmax_t>(count);Require(total<=512u*1024u*1024u,"Owned sound source exceeds fingerprint bound");
        for(std::streamsize index=0;index<count;++index) {
            hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]);hash*=1099511628211ull;
        }
    }
    Require(file.eof()&&!file.bad(),"Cannot finish owned sound fingerprint");return hash;
}
struct OwnedSource {
    std::filesystem::path path;std::uintmax_t size{};std::filesystem::file_time_type modified;std::uint64_t hash{};
    explicit OwnedSource(const std::filesystem::path& source):path(std::filesystem::canonical(source)),
        size(std::filesystem::file_size(path)),modified(std::filesystem::last_write_time(path)),hash(SourceHash(path)) {}
    void Unchanged() const {
        Require(std::filesystem::file_size(path)==size&&std::filesystem::last_write_time(path)==modified&&SourceHash(path)==hash,
            "Owned sound source bytes, size or mtime changed: "+path.string());
    }
};
bool SameLookup(const PortableObjectLookupState& first,const PortableObjectLookupState& second) {
    return first.prepared==second.prepared&&first.objects==second.objects&&
        first.generation==second.generation&&first.retainedBytes==second.retainedBytes;
}
bool Within(const std::filesystem::path& child,const std::filesystem::path& parent) {
    const auto fold=[](std::string value) {
        for(auto& character:value)if(character>='A'&&character<='Z')character=static_cast<char>(character-'A'+'a');
        return value;
    };
    auto component=child.begin();
    for(auto expected=parent.begin();expected!=parent.end();++component,++expected)
        if(component==child.end()||fold(component->string())!=fold(expected->string()))return false;
    return true;
}
void OwnedStartupClientRouting(Fixture& fixture,const Tables& hybrid,const std::function<void()>& initialize) {
    constexpr std::uint64_t clientId=8401u,firstId=8402u,secondId=8403u;
    constexpr auto terminal="Unsupported original post-partition client/player/detail setup; non-publishing startup probe rolled back";
    initialize();
    PortableWorldStartupProbeOptions options;
    options.defaultGameClass="SoundClasses.LocalGame";options.defaultServerGameClass="SoundClasses.ServerGame";
    const auto probe=[&](const PortableWorldStartupProbeOptions& input,const QuestVr::Vm::Limits& limits=QuestVr::Vm::Limits{}) {
        const auto before=fixture.Snapshot("StartupClientBefore");const auto revision=GetPortableRuntimeWorldRevision();
        const auto order=ReadPortableWorldActorOrder();const auto lookup=ReadPortableRuntimeObjectLookup();
        GC::Collect();const auto gc=GC::GetStats();
        const auto result=ProbePortableWorldStartup(input,limits);
        Require(!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "Startup Client diagnostic published incomplete startup");
        Require(fixture.Snapshot("StartupClientAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&
            ReadPortableWorldActorOrder()==order&&SameLookup(lookup,ReadPortableRuntimeObjectLookup()),
            "Startup Client diagnostic retained native order/lookup/clock/RNG/script/revision state");
        GC::Collect();const auto after=GC::GetStats();
        Require(after.numObjects==gc.numObjects&&after.memoryUsage==gc.memoryUsage,
            "Startup Client diagnostic leaked roots/births/native tracker allocations");
        Require(!Read("Level0","bBegunPlay").boolean&&Read("Level0","Game").text.empty(),
            "Startup Client diagnostic left a begun world or GameInfo");
        ++refusals;return result;
    };
    const auto selected=[&](const PortableWorldStartupProbeResult& result,const char* game,bool tracker) {
        Require(result.phase==PortableWorldStartupPhase::ClientSetup&&result.gameInfoPath==Path(std::string(game)+"0")&&
            result.execution.error==terminal&&result.playerLogins.empty()&&
            result.brushTracker.attempted==tracker&&result.brushTracker.completed==tracker,
            std::string("Actual Engine.Client presence/array selected the wrong GameInfo/Login/tracker path: ")+game+": "+result.execution.error);
    };
    selected(probe(options),"ServerGame",false);
    auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,clientId);
    selected(probe(options),"ServerGame",false); // Raw owner token is not Engine.Client.
    RegisterPortableRuntimeViewport(clientId,{firstId,1440u,1440u,3u,2u});
    selected(probe(options),"ServerGame",false); // Nor is target metadata.
    ClearPortableRuntimeClient();GC::Collect();
    lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,clientId);
    const auto client=PreparePortableRuntimeClientObjects();
    Require(client.initialized&&client.viewports.empty()&&client.registeredTargets.empty(),
        "Startup fixture did not establish actual initialized empty Client");
    selected(probe(options),"LocalGame",true);
    RegisterPortableRuntimeViewport(clientId,{firstId,1440u,1440u,3u,2u});
    selected(probe(options),"LocalGame",true);
    RegisterPortableRuntimeViewport(clientId,{secondId,1600u,1600u,3u,2u});
    selected(probe(options),"LocalGame",true); // Two descriptors still mean zero SpawnPlayActor calls.
    auto url=options;url.urlGameClass="SoundClasses.OverrideGame";selected(probe(url),"OverrideGame",true);
    Call("Driver","SetWorldGameType",{{Value::Text(Kind::Object,"SoundClasses.ServerGame"),{}}});
    selected(probe(options),"ServerGame",true);selected(probe(url),"OverrideGame",true);
    Call("Driver","SetWorldGameType",{{Value::Text(Kind::Object,{}),{}}});

    const auto second=PreparePortableRuntimePlayerObjects(secondId);
    Require(!second.bound,"Startup fixture fabricated a possessed viewport");
    auto missing=probe(options);
    Require(missing.phase==PortableWorldStartupPhase::ClientSetup&&missing.playerLogins.empty()&&
        missing.execution.error=="Original SpawnPlayActor requires explicit decoded URL/config sources",
        "Actual nonempty Client array bypassed the required Login source");
    const auto first=PreparePortableRuntimePlayerObjects(firstId);const auto reversed=ReadPortableRuntimeClientSession();
    Require(!first.bound&&reversed.viewports.size()==2u&&reversed.viewports[0u].identity==secondId&&
        reversed.viewports[1u].identity==firstId&&reversed.registeredTargets[0u].identity==firstId&&
        reversed.registeredTargets[1u].identity==secondId,"Startup actual B,A array became metadata A,B");
    options.playerLogin.emplace();auto& source=*options.playerLogin;
    source.portal=u"generated-portal";source.urlOptions={u"class=SoundClasses.LoginPawn",u"X=one",u"X=two"};
    source.userDefaultPlayerClass=u"Engine.PlayerPawn";source.iniUrlClass=u"Engine.PlayerPawn";
    const auto expectedError=[](const PortablePlayerLoginSource& input) {
        std::string text(4096u,'L');
        for(const auto c:input.portal){Require(c<128u,"Startup Login oracle expects ASCII Portal");text+=static_cast<char>(c);}
        for(const auto& option:input.urlOptions) {
            text+='?';for(const auto c:option){Require(c<128u,"Startup Login oracle expects ASCII options");text+=static_cast<char>(c);}
        }
        return text;
    };
    const auto logins=[&](const PortableWorldStartupProbeResult& result,const std::string& cls) {
        Require(result.execution.error==terminal&&result.phase==PortableWorldStartupPhase::ClientSetup&&
            result.playerLogins.size()==2u&&result.playerLogins[0u].viewportIdentity==secondId&&
            result.playerLogins[1u].viewportIdentity==firstId&&result.brushTracker.completed,
            "Startup loop did not visit the actual B,A viewport array: "+result.execution.error);
        for(const auto& login:result.playerLogins)
            Require(login.attempted&&login.loginEntered&&login.loginReturned&&!login.paired&&login.playerPath.empty()&&
                login.selectedClass==cls&&login.error==expectedError(source),
                "Actual startup Login lost class/Portal/ordered Options/growing OUT/real None result");
        Require(ReadPortableRuntimePlayerSession(secondId).playerPath.empty()&&
            ReadPortableRuntimePlayerSession(firstId).playerPath.empty(),"Rolled-back Login retained a player pair");
    };
    const auto entered=probe(options);logins(entered,"SoundClasses.LoginPawn");
    const auto repeated=probe(options);logins(repeated,"SoundClasses.LoginPawn");
    Require(repeated.playerLogins==entered.playerLogins&&repeated.execution.writes==entered.execution.writes,
        "Warm rollback changed actual Login observations/write count");
    const auto loginBody=LoadPortableFunctionScript(hybrid.scripts.front(),FindPortableExport(hybrid.scripts.front(),"LocalGame.Login"));
    Require(entered.execution.writes>1u,"Actual Login lacks a final growing OUT write witness");
    QuestVr::Vm::Limits tight;tight.writes=entered.execution.writes-1u;
    const auto copyback=probe(options,tight);
    Require(copyback.execution.status==QuestVr::Vm::Status::Budget&&copyback.playerLogins.size()==2u&&
        copyback.playerLogins[0u].loginReturned&&copyback.playerLogins[1u].loginEntered&&
        !copyback.playerLogins[1u].loginReturned&&copyback.playerLogins[1u].error.empty()&&
        copyback.execution.function=="SoundClasses.LocalGame.Login"&&copyback.execution.opcode==0x2au&&
        copyback.execution.offset==loginBody.logicalSize-1u,
        "Final Login OUT copyback cut did not undo both real viewport callbacks at the original return boundary");
    source.urlOptions={u"CLASS=Engine.Actor",u"X=fallback"};logins(probe(options),"Engine.PlayerPawn");
    source.urlOptions={u"CLASS=",u"X=ini-fallback"};source.userDefaultPlayerClass=u"Engine.Actor";
    logins(probe(options),"Engine.PlayerPawn");
    source.urlOptions={u"CLASS=Missing.Player"};const auto unknown=probe(options);
    Require(unknown.playerLogins.size()==1u&&!unknown.playerLogins[0u].loginEntered&&
        unknown.playerLogins[0u].selectedClass.empty()&&unknown.execution.error.find("StaticLoadClass of an unmaterialized class/package is not implemented")!=std::string::npos,
        "Unknown actual class load silently fell through to a default player");
    source.urlOptions.clear();source.userDefaultPlayerClass.reset();source.iniUrlClass.reset();const auto noClass=probe(options);
    Require(noClass.playerLogins.size()==1u&&!noClass.playerLogins[0u].loginEntered&&
        noClass.execution.error=="Original SpawnPlayActor fatal LoadPlayerClass: no valid PlayerPawn class",
        "Missing class sources fabricated a native player class");
    source.userDefaultPlayerClass=u"Engine.PlayerPawn";source.iniUrlClass=u"Engine.PlayerPawn";
    auto failure=options;failure.urlGameClass="SoundClasses.FailureGame";const auto failed=probe(failure);
    Require(failed.playerLogins.size()==1u&&failed.playerLogins[0u].loginEntered&&!failed.playerLogins[0u].loginReturned&&
        failed.playerLogins[0u].error.empty()&&failed.execution.function=="SoundClasses.FailureGame.Login"&&
        failed.execution.error.find("999")!=std::string::npos,"Late actual Login refusal leaked unfinished OUT storage");
    auto malformed=options;malformed.urlGameClass="SoundClasses.MalformedGame";const auto bad=probe(malformed);
    Require(bad.playerLogins.size()==1u&&!bad.playerLogins[0u].loginEntered&&
        bad.execution.error=="SpawnPlayActor Login class/result constraints are not PlayerPawn",
        "Malformed actual Login secondary class constraint reached script dispatch");
    lifetime->Revoke();const auto revoked=probe(options);
    Require(revoked.gameInfoPath.empty()&&revoked.playerLogins.empty()&&
        revoked.execution.error=="Portable native session UObject is revoked or stale",
        "Revoked actual Client bypassed its exact native root lease refusal: "+revoked.execution.error);
    ClearPortableRuntimeClient();GC::Collect();
    lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,clientId);
    RegisterPortableRuntimeViewport(clientId,{firstId,1440u,1440u,3u,2u});
    options.playerLogin.reset();selected(probe(options),"ServerGame",false);
    std::cout<<"OWNED STARTUP CLIENT: actual Engine None/empty Client/metadata-only/native B,A; original same-root Login protocol, generated test Game callbacks/None/long OUT, final-copyback rollback and exact native ownership; NOT playable startup.\n";
}
void OwnedAudioOutputBinding(Fixture& fixture,const std::function<void()>& initialize) {
    initialize();
    constexpr std::uint64_t clientId=8401u,viewportId=8402u;
    auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();
    const auto output=[&](const std::shared_ptr<QuestVr::NativeClientLifetime>& owner,bool started=true) {
        auto endpoint=std::make_shared<QuestVr::AudioEndpointControl>(19u,owner,clientId,viewportId,
            QuestVr::AudioOutputFormat{48'000u,2u});
        // Explicit HOST protocol confirmation, not AAudio or headset evidence.
        if(started)Require(endpoint->ConfirmStartedOnOwner(true),"Owned audio protocol start refused");
        return std::make_shared<QuestVr::AudioOutputTransport>(endpoint,owner,clientId,viewportId);
    };
    const auto refuse=[&](const std::shared_ptr<QuestVr::AudioOutputTransport>& transport) {
        const auto before=fixture.Snapshot("AudioBindingBefore");const auto revision=GetPortableRuntimeWorldRevision();
        const auto gc=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
        bool rejected{};
        try {BindPortableRuntimeAudioOutput(transport);}catch(const std::exception&){rejected=true;}
        Require(rejected,"Unauthenticated audio output binding accepted");++refusals;
        const auto after=GC::GetStats();
        Require(before==fixture.Snapshot("AudioBindingAfter")&&revision==GetPortableRuntimeWorldRevision()&&
            gc.numObjects==after.numObjects&&gc.memoryUsage==after.memoryUsage&&
            SameLookup(lookup,ReadPortableRuntimeObjectLookup()),"Refused audio binding changed native/script/save/lookup state");
    };
    auto transport=output(lifetime);
    Require(!ReadPortableRuntimeAudioOutput().bound,"Fresh runtime fabricated an audio output binding");
    refuse(transport); // No registered owner or actual Client.
    BindPortableRuntimeClient(lifetime,clientId);
    refuse(transport); // Raw owner descriptor alone.
    Require(PreparePortableRuntimeClientObjects().initialized,"Cannot initialize actual Client for audio binding");
    refuse(transport); // Actual Client, but no target or native Viewport.
    RegisterPortableRuntimeViewport(clientId,{viewportId,1440u,1440u,3u,2u});
    refuse(transport); // Registered target still is not Client.Viewports.
    const auto viewport=PreparePortableRuntimePlayerObjects(viewportId);
    const auto client=ReadPortableRuntimeClientSession();
    refuse(output(lifetime,false)); // Real native tuple cannot authorize a Pending stream.
    refuse(output(std::make_shared<QuestVr::NativeClientLifetime>())); // Same numeric IDs, different owner.
    const auto before=fixture.Snapshot("AudioBoundBefore");const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
    BindPortableRuntimeAudioOutput(transport);BindPortableRuntimeAudioOutput(transport);
    auto bound=ReadPortableRuntimeAudioOutput();
    Require(bound.bound&&bound.ready&&bound.notAudioSubsystem&&bound.generation==19u&&
        bound.clientIdentity==clientId&&bound.viewportIdentity==viewportId&&bound.lookupGeneration==client.lookupGeneration&&
        bound.clientIndex==client.clientIndex&&bound.engineIndex==client.engineIndex&&bound.viewportIndex!=0u&&
        bound.clientPath==client.clientPath&&bound.enginePath==client.enginePath&&bound.viewportPath==viewport.viewportPath&&
        bound.engineAudioPath.empty()&&bound.referenceRetainedBytes>=sizeof(QuestVr::AudioOutputTransport)&&
        bound.resourcePcmBytes==0u&&bound.pendingBatches==0u&&bound.format.sampleRate==48'000u&&bound.format.channels==2u,
        "Bound output lost actual native tuple, separated PCM accounting or truthful Engine.Audio=None");
    const auto after=GC::GetStats();
    Require(before==fixture.Snapshot("AudioBoundAfter")&&revision==GetPortableRuntimeWorldRevision()&&
        gc.numObjects==after.numObjects&&gc.memoryUsage==after.memoryUsage&&SameLookup(lookup,ReadPortableRuntimeObjectLookup()),
        "Output capability binding changed UObjects, world/save or actual lookup");
    refuse(output(lifetime)); // Even the same endpoint numbers cannot replace an existing capability.
    const auto process=std::make_shared<QuestVr::ProcessLoopControl>();BindPortableRuntimeProcessLoop(process);
    const auto rootBinding=ProbePortableRuntimeProcessLoop([&]{BindPortableRuntimeAudioOutput(transport);});
    Require(!rootBinding.execution.passed()&&!rootBinding.execution.committed&&!rootBinding.completed&&
        rootBinding.execution.error.find("exclusive CPU boundary")!=std::string::npos,
        "Audio binding mutation was allowed inside a live VM root");++refusals;
    Require(ReadPortableRuntimeAudioOutput().ready&&fixture.Snapshot("AudioBindingRootAfter")==before&&
        revision==GetPortableRuntimeWorldRevision(),"Refused in-root output binding changed persistent state");
    auto pcm=transport->PreparePcm({100,-100,200,-200},48'000u);
    const auto withPcm=ReadPortableRuntimeAudioOutput();
    Require(withPcm.resourcePcmBytes==transport->RetainedPcmBytes()&&withPcm.resourcePcmBytes>=8u&&
        withPcm.referenceRetainedBytes==bound.referenceRetainedBytes,"PCM charge was hidden in VM reference-state budget");
    QuestVr::AudioVoiceBatch batch;batch.generation=transport->Generation();batch.count=1u;
    batch.commands[0u].operation=QuestVr::AudioVoiceOperation::Play;batch.commands[0u].voice=1u;batch.commands[0u].pcm=pcm;
    {
        auto canceled=transport->Reserve(batch);canceled.RequirePublishable();
        Require(ReadPortableRuntimeAudioOutput().pendingBatches==0u,"Reserved output became visible before commit");
    }
    auto ticket=transport->Reserve(batch);ticket.RequirePublishable();ticket.PublishPrepared();
    Require(ReadPortableRuntimeAudioOutput().pendingBatches==1u,"Committed queue was not reflected in bound output snapshot");
    transport->Endpoint()->Revoke(-731);
    const auto revoked=ReadPortableRuntimeAudioOutput();
    Require(revoked.bound&&!revoked.ready&&revoked.engineAudioPath.empty(),"Revoked stream hid its binding or fabricated native audio");
    refuse(transport);
    ClearPortableRuntimeClient();
    Require(!ReadPortableRuntimeAudioOutput().bound&&!transport->Ready(),"Native Client clear retained a usable output binding");
    lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,clientId);
    PreparePortableRuntimeClientObjects();RegisterPortableRuntimeViewport(clientId,{viewportId,1440u,1440u,3u,2u});
    PreparePortableRuntimePlayerObjects(viewportId);refuse(transport);
    auto replacement=output(lifetime);BindPortableRuntimeAudioOutput(replacement);
    Require(ReadPortableRuntimeAudioOutput().ready,"Fresh same-ID Client could not bind its own exact capability");
    ClearPortableRuntimeClient();
    Require(!replacement->Ready()&&replacement->Endpoint()->Ready(),"Client clear failed transport retirement or revoked another owner's stream");
    lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,clientId);
    PreparePortableRuntimeClientObjects();RegisterPortableRuntimeViewport(clientId,{viewportId,1440u,1440u,3u,2u});
    PreparePortableRuntimePlayerObjects(viewportId);auto finalOutput=output(lifetime);BindPortableRuntimeAudioOutput(finalOutput);
    ShutdownPortableRuntime();
    Require(!finalOutput->Ready()&&!ReadPortableRuntimeAudioOutput().bound,"Runtime shutdown retained usable audio capability");
    std::cout<<"OWNED AUDIO OUTPUT: exact actual Client/Engine/Viewport binding, pending/metadata/stale owner refusals, PCM accounting and clear/shutdown; host protocol only, NOT original AudioSubsystem or hardware readiness.\n";
}

void OwnedActualClientRouting(const std::filesystem::path& root,bool startup=false) {
    // ScriptSaveSchema derives its game root from the actual Actor UClass's
    // source. Put verified byte-identical copies beside the generated test map
    // in this isolated game root; never write a fixture into the owned install
    // or weaken production save-source authority. Owned mode does not generate
    // small Core/Engine substitutes at all.
    Fixture fixture;const auto hybrid=Build(fixture,true,true,startup);
    Require(!Within(fixture.directory,root)&&!Within(root,fixture.directory),
        "Owned sound fixture overlaps the commercial installation");
    std::vector<OwnedSource> sources,copies;std::vector<PortablePackageTables> packages;
    const auto copy=[&](const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid isolated owned source leaf");
        sources.emplace_back(root/"System"/leaf);
        const auto target=fixture.directory/"System"/leaf;
        Require(!std::filesystem::exists(target)&&std::filesystem::copy_file(sources.back().path,target),
            "Cannot create isolated byte-identical owned sound source");
        copies.emplace_back(target);
        Require(copies.back().size==sources.back().size&&copies.back().hash==sources.back().hash,
            "Copied owned sound package differs from the protected source");
        return target;
    };
    const auto ini=copy("DeusEx.ini");
    for(const auto* name:QuestVr::RuntimeScriptPackages) {
        packages.push_back(LoadPortablePackageTables(copy(std::string(name)+".u").string()));
    }
    Require(hybrid.scripts.size()==1u&&fixture.sources.size()==2u,"Owned hybrid generated a Core/Engine substitute or changed helper inventory");
    packages.push_back(hybrid.scripts.front());
    const auto initialize=[&] {
        ShutdownPortableRuntime();GC::Collect();
        Require(InitializePortableRuntime(packages).passed&&
            PreparePortableRuntimeNativeEngineClasses().prepared&&
            PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&
            PreparePortableRuntimeNativePackages().prepared&&PreparePortableRuntimeObjectLookup().prepared&&
            PreparePortableRuntimeEngineCollision(ini.string()).prepared&&
            LoadPortableRuntimeMap(hybrid.map).passed,"Cannot prepare genuine owned Engine and hybrid local-sound map");
    };
    if(startup) {
        OwnedStartupClientRouting(fixture,hybrid,initialize);
        fixture.Unchanged();for(const auto& source:sources)source.Unchanged();for(const auto& source:copies)source.Unchanged();
        ShutdownPortableRuntime();GC::Collect();return;
    }
    OwnedAudioOutputBinding(fixture,initialize);
    constexpr std::uint64_t clientId=8301u,firstId=8302u,secondId=8303u;
    const auto empty=[&](const char* label) {
        const auto before=fixture.Snapshot("OwnedEmptyBefore");const auto gc=GC::GetStats();
        const auto lookup=ReadPortableRuntimeObjectLookup();const auto revision=GetPortableRuntimeWorldRevision();
        const auto result=Call("Driver","PlaySimulated");const auto id=Channel(result);const auto after=GC::GetStats();
        Require(!result.writes&&after.numObjects==gc.numObjects&&after.memoryUsage==gc.memoryUsage&&
            SameLookup(lookup,ReadPortableRuntimeObjectLookup())&&GetPortableRuntimeWorldRevision()==revision&&
            fixture.Snapshot("OwnedEmptyAfter")==before,
            std::string("Genuine empty local listener selection mutated state: ")+label);
        return id;
    };
    const auto targets=[&] {
        RegisterPortableRuntimeViewport(clientId,{firstId,1440u,1440u,3u,2u});
        RegisterPortableRuntimeViewport(clientId,{secondId,1600u,1600u,3u,2u});
    };
    const auto bind=[&](bool registerTargets=true) {
        auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,clientId);
        const auto client=PreparePortableRuntimeClientObjects();
        Require(client.initialized&&client.viewports.empty()&&client.registeredTargets.empty(),
            "Local sound did not start with the actual initialized Client's empty array");
        if(registerTargets)targets();
        return lifetime;
    };
    const auto append=[&](std::uint64_t identity) {
        const auto viewport=PreparePortableRuntimePlayerObjects(identity);
        Require(!viewport.bound&&viewport.playerPath.empty(),"Explicit viewport construction fabricated a local sound listener");
        return viewport;
    };
    const auto pair=[&](std::uint64_t identity,const std::string& actor) {
        const auto result=SetPortableRuntimePlayer(identity,Path(actor));
        Require(result.passed()&&result.committed,"Genuine test SetPlayer/Possess failed: "+result.error);
        Same(Read(actor,"Possessions"),Value::Integer(1),"SetPlayer did not execute the actual generated Possess override");
        const auto viewport=ReadPortableRuntimePlayerSession(identity);
        Require(viewport.bound&&viewport.playerPath==Path(actor),"Local sound fixture did not establish actual paired native edges");
    };

    initialize();
    const auto noClient=empty("actual Engine.Client=None");
    Require(Channel(Call("Driver","Play"))==noClient,"Local zero-listener route changed source channel identity");
    auto lifetime=bind(false);
    Require(empty("initialized actual Client, zero targets and zero viewports")==noClient,
        "Initialized empty Client changed source channel identity");
    targets();
    const auto metadataOnly=ReadPortableRuntimeClientSession();
    Require(metadataOnly.viewports.empty()&&metadataOnly.registeredTargets.size()==2u,
        "Target descriptors fabricated actual native viewports");
    Require(empty("descriptor A,B but zero actual viewports")==noClient,"Descriptor-only listener selection changed channel identity");
    const auto second=append(secondId);
    const auto onlySecond=ReadPortableRuntimeClientSession();
    Require(onlySecond.viewports.size()==1u&&onlySecond.viewports[0u].identity==secondId&&
        onlySecond.viewports[0u].path==second.viewportPath&&onlySecond.registeredTargets[0u].identity==firstId,
        "Actual viewport B was replaced by registered target A");
    empty("descriptor A,B and actual B with Actor=None");
    append(firstId);
    const auto reversed=ReadPortableRuntimeClientSession();
    Require(reversed.viewports.size()==2u&&reversed.viewports[0u].identity==secondId&&
        reversed.viewports[1u].identity==firstId&&reversed.registeredTargets[0u].identity==firstId&&
        reversed.registeredTargets[1u].identity==secondId,"Actual B,A order was replaced by descriptor order A,B");
    empty("actual B,A both with Actor=None");
    pair(secondId,"Second");pair(firstId,"First");
    const auto both=Channel(Call("Driver","PlaySimulated"));Packet("Second",both);Packet("First",both);
    lifetime->Revoke();Refused(fixture,"Driver","PlaySimulated");
    ClearPortableRuntimeClient();GC::Collect();
    lifetime=bind();append(secondId);append(firstId);
    empty("Clear and same numeric Client/Viewport IDs do not revive old actors");
    Same(Read("First","Heard"),Value::Integer(1),"Fresh same-ID lifetime reused the retired first listener");
    Same(Read("Second","Heard"),Value::Integer(1),"Fresh same-ID lifetime reused the retired second listener");

    // The first actual viewport is B even though target metadata starts A.
    // Its synchronous callback moves the source out of A's hearing radius.
    initialize();lifetime=bind();append(secondId);append(firstId);pair(secondId,"Mover");pair(firstId,"First");
    const auto moved=Channel(Call("Driver","PlaySimulated"));Packet("Mover",moved);
    Same(Read("Driver","Location"),Value::Vector({4000,0,0}),"Local callback did not synchronously move the actual source");
    Same(Read("First","Heard"),Value::Integer(0),"Local loop used metadata order or cached source location across callback");

    initialize();lifetime=bind();append(secondId);append(firstId);pair(secondId,"First");pair(firstId,"Failure");
    const auto lookup=ReadPortableRuntimeObjectLookup();const auto failed=Refused(fixture,"Driver","PlaySimulated");
    Require(failed.error.find("999")!=std::string::npos,"Local callback rollback stopped before intentional late refusal");
    Same(Read("First","Heard"),Value::Integer(0),"Later local callback failure retained an earlier recipient's effects");
    Require(SameLookup(lookup,ReadPortableRuntimeObjectLookup())&&ReadPortableRuntimePlayerSession(secondId).playerPath==Path("First")&&
        ReadPortableRuntimePlayerSession(firstId).playerPath==Path("Failure"),
        "Local callback rollback changed lookup or established native player pairs");
    fixture.Unchanged();for(const auto& source:sources)source.Unchanged();for(const auto& source:copies)source.Unchanged();
    ShutdownPortableRuntime();GC::Collect();
}
} // namespace

int main(int argc,char** argv) {
    try {
        std::cout.setf(std::ios::unitbuf);
        bool owned{},startup{};std::filesystem::path root;
        for(int argument=1;argument<argc;++argument) {
            const std::string value=argv[argument];
            if(value=="--owned")owned=true;
            else if(value=="--startup"){startup=true;owned=true;}
            else if(value=="--game-root"&&argument+1<argc) {root=argv[++argument];owned=true;}
            else throw std::runtime_error("Usage: portable_sound_routing_test [--owned | --startup] [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        }
        if(owned&&root.empty()) {const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured)root=configured;}
        if(owned&&root.empty()) {std::cout<<"SKIP owned Engine/Client/Viewport integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        const auto baseline=GC::GetStats().numObjects;
        if(owned)OwnedActualClientRouting(std::filesystem::canonical(root),startup);
        else {
            Fixture fixture;const auto tables=Build(fixture);
            ZeroRecipientsAndIdentity(fixture,tables);GenuineRecipientsAndRollback(fixture,tables);CallerAndOwnership(fixture,tables);
            BlockedBspHearing();NativeLevelClock(fixture,tables);
        }
        ShutdownPortableRuntime();GC::Collect();Require(GC::GetStats().numObjects==baseline,"Sound routing fixture leaked rooted UObjects");
        std::cout<<"PASS "<<(startup?"owned startup Client":owned?"owned actual-Client local PlaySound264":"generated nonlocal PlaySound264")<<" production routing: "<<checks<<" checks, "<<refusals
            <<" refusal controls; real UObject channels, caller/native Level ownership, "
            <<(owned?"live actual-Client viewport callbacks":"live PawnList callbacks")<<" and complete rollback."
              <<(startup?" Owned startup mode uses generated GameInfo lifecycle/Login overrides and real script None; not retail JC startup.":
                  " Owned sound mode uses generated test Possess/ClientHearSound overrides, not original Login/Possess/startup.")
              <<" No rendered/audio-queue or complete-crate gameplay claim.\n";return 0;
    } catch(const std::exception& error) {ShutdownPortableRuntime();std::cerr<<"Sound routing integration failed: "<<error.what()<<'\n';return 1;}
}
