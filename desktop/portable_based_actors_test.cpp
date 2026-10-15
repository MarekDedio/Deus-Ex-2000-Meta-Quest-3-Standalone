#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_model_geometry.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Retail/static contract, NOT the pin's BasedActors snapshot/Name filter:
// GOTY Engine.dll SHA256
// 0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4.
// ?execBasedActors@AActor@@QAEXAAUFFrame@@QAX@Z export RVA4048
// jumps to VA103e4800. VA103e4885 defaults None to Actor UClass1057cdf0;
// 48a6 clears OUT; 48ad/48c2 read receiver Level+ac, actor array+2c/count+30;
// 48d4..48ef compare actual Class+24/Super+28 pointers, not class leaf Names;
// 48f1..48fb compare candidate Base+c4 to the receiver; 4904 rereads Level,
// 490e rereads count. Slot cursor survives loop bodies, no array snapshot.
// Reproduce without loading/executing the original DLL:
// objdump -p <root>/System/Engine.dll (named export/RVA)
// objdump -d -Mintel --start-address=0x10304048 --stop-address=0x1030404d <dll>
// objdump -d -Mintel --start-address=0x103e4800 --stop-address=0x103e4980 <dll>
// Generated packages below use production tables, reflected properties,
// bytecode, Level slots, Spawn278/Base298/Destroy279 and transaction journals.
namespace {
using Bytes=std::vector<std::uint8_t>;
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
using Evaluation=QuestVr::Vm::Evaluation;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& text) { if(!value) throw std::runtime_error(text); ++checks; }
void U16(Bytes& b,std::uint16_t v) { b.push_back(static_cast<std::uint8_t>(v)); b.push_back(static_cast<std::uint8_t>(v>>8u)); }
void U32(Bytes& b,std::uint32_t v) { U16(b,static_cast<std::uint16_t>(v)); U16(b,static_cast<std::uint16_t>(v>>16u)); }
void U64(Bytes& b,std::uint64_t v) { U32(b,static_cast<std::uint32_t>(v)); U32(b,static_cast<std::uint32_t>(v>>32u)); }
void Index(Bytes& b,std::int32_t v) {
    auto rest=static_cast<std::uint32_t>(v<0?-static_cast<std::int64_t>(v):v);
    auto first=static_cast<std::uint8_t>((rest&63u)|(v<0?128u:0u)); rest>>=6u;
    if(rest) first|=64u;
    b.push_back(first);
    while(rest) { auto next=static_cast<std::uint8_t>(rest&127u); rest>>=7u; if(rest) next|=128u; b.push_back(next); }
}
void Append(Bytes& b,const Bytes& other) { b.insert(b.end(),other.begin(),other.end()); }
void Replace32(Bytes& b,std::size_t at,std::uint32_t v) { for(unsigned i=0;i<4u;++i) b.at(at+i)=static_cast<std::uint8_t>(v>>(8u*i)); }
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto n=std::filesystem::file_size(path); Require(n<=32u*1024u*1024u,"Test input exceeds independent 32MiB bound");
    Bytes b(static_cast<std::size_t>(n)); std::ifstream file(path,std::ios::binary); Require(static_cast<bool>(file),"Cannot read test input");
    if(!b.empty()) file.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));
    Require(static_cast<bool>(file),"Test input was truncated"); return b;
}
void WriteBytes(const std::filesystem::path& path,const Bytes& b) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc); Require(static_cast<bool>(file),"Cannot create owned fixture");
    file.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size())); Require(static_cast<bool>(file),"Cannot write owned fixture");
}
struct Code { Bytes raw; std::size_t logical{}; };
Code Token(std::uint8_t v) { return {{v},1u}; }
Code Ref(std::uint8_t op,std::int32_t ref) { Code c{{op},5u}; Index(c.raw,ref); return c; }
Code Int(std::int32_t v) { Code c{{0x1du},5u}; U32(c.raw,static_cast<std::uint32_t>(v)); return c; }
Code Join(std::initializer_list<Code> parts) { Code c; for(const auto& p:parts) { Append(c.raw,p.raw); c.logical+=p.logical; } return c; }
Code Assign(Code l,Code r) { return Join({Token(0x0fu),std::move(l),std::move(r)}); }
Code Return(Code v=Token(0x0bu)) { return Join({Token(0x04u),std::move(v)}); }
Code Native(std::uint16_t n,std::initializer_list<Code> args={}) {
    Code c;
    if(n>=256u) { c.raw={static_cast<std::uint8_t>(0x60u+(n>>8u)),static_cast<std::uint8_t>(n)}; c.logical=2u; }
    else c=Token(static_cast<std::uint8_t>(n));
    for(const auto& arg:args) { Append(c.raw,arg.raw); c.logical+=arg.logical; }
    return Join({c,Token(0x16u)});
}
Code Final(std::int32_t n,std::initializer_list<Code> args={}) {
    auto c=Ref(0x1cu,n); for(const auto& arg:args) { Append(c.raw,arg.raw); c.logical+=arg.logical; } return Join({c,Token(0x16u)});
}
Code Context(Code receiver,Code child) {
    Require(child.logical<=65535u,"Generated Context exceeds skip bound"); auto c=Join({Token(0x19u),std::move(receiver)});
    U16(c.raw,static_cast<std::uint16_t>(child.logical)); c.raw.push_back(0u); c.logical+=3u;
    Append(c.raw,child.raw); c.logical+=child.logical; return c;
}
Code Array(std::int32_t ref,Code slot) { return Join({Token(0x1au),std::move(slot),Ref(0x01u,ref)}); }
struct Flow {
    Code code; std::map<std::string,std::size_t> labels;
    std::vector<std::pair<std::size_t,std::string>> patches;
    void Add(Code c) { Append(code.raw,c.raw); code.logical+=c.logical; }
    void Label(const std::string& s) { Require(labels.emplace(s,code.logical).second,"Duplicate generated label"); }
    void Target(const std::string& s) { patches.emplace_back(code.raw.size(),s); U16(code.raw,0u); code.logical+=2u; }
    void IfNot(Code c,const std::string& s) { Add(Token(0x07u)); Target(s); Add(std::move(c)); }
    void Iterator(Code factory,const std::string& end) { Add(Token(0x2fu)); Add(std::move(factory)); Target(end); }
    Code Finish() {
        for(const auto& [at,s]:patches) { const auto target=labels.at(s); Require(target<=65535u,"Generated target exceeds logical bound"); code.raw.at(at)=static_cast<std::uint8_t>(target); code.raw.at(at+1u)=static_cast<std::uint8_t>(target>>8u); }
        return std::move(code);
    }
};
struct Package {
    std::string stem; std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports; std::vector<ExportTableEntry> exports;
    std::vector<std::function<Bytes(std::int32_t,std::int32_t)>> builders;
    std::map<std::int32_t,std::vector<std::int32_t>> children;
    std::int32_t Name(const std::string& s) { const auto it=std::find(names.begin(),names.end(),s); if(it!=names.end()) return static_cast<std::int32_t>(it-names.begin()); names.push_back(s); return static_cast<std::int32_t>(names.size()-1u); }
    std::int32_t Import(const std::string& s,std::int32_t outer,const std::string& type="Class") { imports.push_back({Name("Core"),Name(type),outer,Name(s)}); return -static_cast<std::int32_t>(imports.size()); }
    std::int32_t ImportPackage(const std::string& s) { return Import(s,0,"Package"); }
    std::int32_t Export(const std::string& s,std::int32_t cls=0,std::int32_t base=0,std::int32_t outer=0,bool field=false) {
        exports.push_back({cls,base,outer,Name(s),ObjectFlags{},0,-1}); builders.emplace_back(); const auto ref=static_cast<std::int32_t>(exports.size()); if(field) children[outer].push_back(ref); return ref;
    }
    void Tag(Bytes& b,const std::string& s,std::uint8_t type,Bytes value) { Require(value.size()<=255u,"Generated tag too large"); Index(b,Name(s)); b.push_back(static_cast<std::uint8_t>(type|0x50u)); b.push_back(static_cast<std::uint8_t>(value.size())); Append(b,value); }
    void ObjectTag(Bytes& b,const std::string& s,std::int32_t ref) { Bytes value; Index(value,ref); Tag(b,s,5u,std::move(value)); }
    void BoolTag(Bytes& b,const std::string& s,bool v) { Index(b,Name(s)); b.push_back(static_cast<std::uint8_t>(3u|(v?0x80u:0u))); }
    void ClassBody(std::int32_t ref,Bytes defaults={}) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,defaults=std::move(defaults)](std::int32_t next,std::int32_t first) {
            Bytes b; const auto& e=exports.at(static_cast<std::size_t>(ref-1)); Index(b,e.ObjBase); Index(b,next); Index(b,0); Index(b,first); Index(b,e.ObjName);
            U32(b,0); U32(b,0); U32(b,0); U64(b,~std::uint64_t{}); U64(b,~std::uint64_t{}); U16(b,0xffffu); U32(b,0);
            U32(b,0); b.resize(b.size()+16u,0u); for(unsigned i=0;i<4u;++i) Index(b,0); Append(b,defaults); Index(b,0); return b;
        };
    }
    void StructBody(std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref](std::int32_t next,std::int32_t first) {
            Bytes b{0u}; Index(b,0); Index(b,next); Index(b,0); Index(b,first); Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName); U32(b,0); U32(b,0); U32(b,0); return b;
        };
    }
    std::int32_t Property(const std::string& s,const std::string& type,std::int32_t owner,std::int32_t core,std::uint32_t dim=1u,std::int32_t target=0,std::uint32_t flags=0u,std::int32_t secondary=0) {
        const auto ref=Export(s,Import(type,core),0,owner,true);
        builders.at(static_cast<std::size_t>(ref-1))=[type,dim,target,flags,secondary](std::int32_t next,std::int32_t) {
            Bytes b{0u}; Index(b,0); Index(b,next); U32(b,dim); U32(b,flags); Index(b,0);
            if(type=="ClassProperty") { Index(b,target); Index(b,secondary); }
            else if(type=="ObjectProperty"||type=="ByteProperty"||type=="StructProperty") Index(b,target);
            return b;
        }; return ref;
    }
    std::int32_t Function(const std::string& s,std::int32_t owner,std::int32_t core,Code code={},std::uint16_t native=0u,std::uint32_t flags=0u) {
        const auto ref=Export(s,Import("Function",core),0,owner,true); FunctionCode(ref,std::move(code),native,flags); return ref;
    }
    void FunctionCode(std::int32_t ref,Code code,std::uint16_t native=0u,std::uint32_t flags=0u) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,native,flags,code=std::move(code)](std::int32_t next,std::int32_t first) {
            Bytes b{0u}; Index(b,0); Index(b,next); Index(b,0); Index(b,first); Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);
            U32(b,0); U32(b,0); U32(b,static_cast<std::uint32_t>(code.logical)); Append(b,code.raw); U16(b,native); b.push_back(0u); U32(b,flags?flags:native?0x402u:2u); return b;
        };
    }
    void Body(std::int32_t ref,Bytes b) { builders.at(static_cast<std::size_t>(ref-1))=[b=std::move(b)](std::int32_t,std::int32_t) { return b; }; }
    Bytes Serialize() {
        std::map<std::int32_t,std::int32_t> next; for(const auto& [owner,list]:children) { (void)owner; for(std::size_t i=1u;i<list.size();++i) next[list[i-1u]]=list[i]; }
        Bytes b; U32(b,0x9e2a83c1u); U16(b,68u); U16(b,0u); b.resize(56u,0u);
        std::vector<Bytes> payloads; std::vector<std::int32_t> offsets;
        for(std::size_t i=0;i<exports.size();++i) { const auto ref=static_cast<std::int32_t>(i+1u); const auto child=children.find(ref); auto body=builders[i]?builders[i](next[ref],child==children.end()||child->second.empty()?0:child->second.front()):Bytes{}; offsets.push_back(static_cast<std::int32_t>(b.size())); Append(b,body); payloads.push_back(std::move(body)); }
        Replace32(b,12u,static_cast<std::uint32_t>(names.size())); Replace32(b,16u,static_cast<std::uint32_t>(b.size()));
        for(const auto& name:names) { Index(b,static_cast<std::int32_t>(name.size()+1u)); b.insert(b.end(),name.begin(),name.end()); b.push_back(0u); U32(b,0); }
        Replace32(b,20u,static_cast<std::uint32_t>(exports.size())); Replace32(b,24u,static_cast<std::uint32_t>(b.size()));
        for(std::size_t i=0;i<exports.size();++i) { const auto& e=exports[i]; Index(b,e.ObjClass); Index(b,e.ObjBase); U32(b,static_cast<std::uint32_t>(e.ObjOuter)); Index(b,e.ObjName); U32(b,static_cast<std::uint32_t>(e.ObjFlags)); Index(b,static_cast<std::int32_t>(payloads[i].size())); if(!payloads[i].empty()) Index(b,offsets[i]); }
        Replace32(b,28u,static_cast<std::uint32_t>(imports.size())); Replace32(b,32u,static_cast<std::uint32_t>(b.size()));
        for(const auto& e:imports) { Index(b,e.ClassPackage); Index(b,e.ClassName); U32(b,static_cast<std::uint32_t>(e.ObjOuter)); Index(b,e.ObjName); } return b;
    }
};
struct Fixture {
    std::filesystem::path parent,directory; std::map<std::filesystem::path,Bytes> sources; Bytes baseline;
    Fixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path()); const auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<20u;++i) { const auto p=parent/("deusex-based-actors-test-"+std::to_string(stamp)+'-'+std::to_string(i)); if(std::filesystem::create_directory(p)) { directory=std::filesystem::canonical(p); break; } }
        Require(!directory.empty()&&directory.parent_path()==parent,"Owned fixture escaped temporary parent"); std::filesystem::create_directory(directory/"System"); std::filesystem::create_directory(directory/"Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime(); std::error_code error; const auto p=std::filesystem::weakly_canonical(directory,error);
        if(!error&&!directory.empty()&&p==directory&&p.parent_path()==parent&&p.filename().string().rfind("deusex-based-actors-test-",0u)==0u) std::filesystem::remove_all(p,error);
    }
    PortablePackageTables Write(Package& p,bool map=false) { const auto path=directory/(map?"Maps":"System")/(p.stem+(map?".dx":".u")); auto b=p.Serialize(); WriteBytes(path,b); sources.emplace(path,std::move(b)); return LoadPortablePackageTables(path.string()); }
    Bytes Snapshot(const std::string& s) const { const auto path=directory/(s+".sav"); Require(SavePortableRuntimeState(path.string()),"Cannot save test state"); return ReadBytes(path); }
    void Reset() const { Require(LoadPortableRuntimeState((directory/"Baseline.sav").string()),"Cannot restore test baseline"); Require(Snapshot("Reset")==baseline,"Reset retained iterator or mutation state"); }
    void Unchanged() const { for(const auto& [path,b]:sources) Require(ReadBytes(path)==b,"Iterator changed a generated source package"); }
};
std::string Path(const std::string& s) { return s.empty()?std::string{}:"BasedFixture."+s; }
Evaluation Obj(const std::string& s) { return {Value::Text(Kind::Object,Path(s)),{}}; }
Evaluation Cls(const std::string& s) { return {Value::Text(Kind::Object,s),{}}; }
Value Read(const std::string& s,const std::string& p,std::uint32_t slot=0u) { return ReadPortableActorScriptProperty(Path(s),p,slot); }
QuestVr::Vm::Result Call(const std::string& s,const std::string& fn,const std::vector<Evaluation>& args={}) {
    auto result=ExecutePortableActorFunction(Path(s),fn,args); Require(result.passed(),fn+" failed: "+result.error+" at "+result.function+':'+std::to_string(result.offset)); return result;
}
void Refused(const Fixture& fixture,const std::string& fn,const std::vector<Evaluation>& args={},const QuestVr::Vm::Limits& limits={}) {
    const auto before=fixture.Snapshot("Before"); const auto revision=GetPortableRuntimeWorldRevision(); const auto result=ExecutePortableActorFunction(Path("Root"),fn,args,limits);
    Require(!result.passed()&&!result.committed,"Invalid iterator committed: "+fn); Require(fixture.Snapshot("After")==before,"Rejected iterator partially changed persistent state: "+fn);
    Require(GetPortableRuntimeWorldRevision()==revision,"Rejected iterator advanced publication revision: "+fn); ++refusals;
}
struct Tables { std::vector<PortablePackageTables> scripts; PortablePackageTables map; };
Tables Build(Fixture& fixture,unsigned schema=0u,unsigned extras=0u) {
    Package core; core.stem="Core"; const auto cp=core.ImportPackage("Core"),object=core.Export("Object"),metaclass=core.Export("Class",0,object);
    const auto vector=core.Export("Vector",core.Import("Struct",cp),0,object),rotator=core.Export("Rotator",core.Import("Struct",cp),0,object);
    for(const auto& s:{"X","Y","Z"}) core.Property(s,"FloatProperty",vector,cp);
    for(const auto& s:{"Pitch","Yaw","Roll"}) core.Property(s,"IntProperty",rotator,cp);
    core.Property("Class","ClassProperty",object,cp,1u,metaclass,0u,object); core.Property("Name","NameProperty",object,cp);
    core.Property("Outer","ObjectProperty",object,cp,1u,object); core.Property("ObjectFlags","IntProperty",object,cp);
    core.ClassBody(object); core.ClassBody(metaclass); core.StructBody(vector); core.StructBody(rotator); const auto ct=fixture.Write(core);
    Package engine; engine.stem="Engine"; const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec),eclass=engine.Import("Class",ec);
    const auto ev=engine.Import("Vector",eo,"Struct"),er=engine.Import("Rotator",eo,"Struct");
    const auto actor=engine.Export("Actor",0,eo),zone=engine.Export("ZoneInfo",0,actor),info=engine.Export("LevelInfo",0,zone),level=engine.Export("Level",0,eo),pawn=engine.Export("Pawn",0,actor),model=engine.Export("Model",0,eo);
    const auto region=engine.Export("PointRegion",engine.Import("Struct",ec),0,actor); engine.Property("Zone","ObjectProperty",region,ec,1u,zone); engine.Property("iLeaf","IntProperty",region,ec); engine.Property("ZoneNumber","ByteProperty",region,ec); engine.StructBody(region);
    for(const auto& s:{"Base","Owner","Instigator","Brush"}) engine.Property(s,"ObjectProperty",actor,ec,1u,actor);
    engine.Property("Level","ObjectProperty",actor,ec,1u,info); engine.Property("XLevel","ObjectProperty",actor,ec,1u,level);
    engine.Property("Touching","ObjectProperty",actor,ec,4u,actor); engine.Property("StandingCount","ByteProperty",actor,ec); engine.Property("Physics","ByteProperty",actor,ec);
    for(const auto& s:{"bDeleteMe","bStatic","bNoDelete","bHidden","bTicked","bCollideWorld","bCollideWhenPlacing","bCollideActors","bBlockActors","bBlockPlayers"}) engine.Property(s,"BoolProperty",actor,ec);
    for(const auto& s:{"CollisionHeight","CollisionRadius"}) engine.Property(s,"FloatProperty",actor,ec);
    for(const auto& s:{"Location","OldLocation"}) engine.Property(s,"StructProperty",actor,ec,1u,ev);
    engine.Property("Rotation","StructProperty",actor,ec,1u,er); engine.Property("Region","StructProperty",actor,ec,1u,region);
    engine.Property("Tag","NameProperty",actor,ec); engine.Property("AttachTag","NameProperty",actor,ec);
    engine.Property("bBegunPlay","BoolProperty",info,ec); engine.Property("bWaterZone","BoolProperty",zone,ec);
    const auto based=engine.Function("BasedActors",actor,ec,{},306u,schema==1u?0x404u:0x405u);
    const auto bc=engine.Property("BaseClass",schema==2u?"ObjectProperty":"ClassProperty",based,ec,1u,eclass,0x80u,schema==4u?eo:actor);
    const auto out=engine.Property("Actor","ObjectProperty",based,ec,1u,actor,schema==3u?0x80u:0x180u);
    engine.FunctionCode(based,Join({Ref(0x29u,bc),Ref(0x29u,out)}),306u,schema==1u?0x404u:0x405u);
    const auto impostor=engine.Function("NotBasedActors",actor,ec,{},306u,0x405u);
    engine.Property("BaseClass","ClassProperty",impostor,ec,1u,eclass,0x80u,actor); engine.Property("Actor","ObjectProperty",impostor,ec,1u,actor,0x180u);
    for(auto ref:{actor,zone,info,level,pawn,model}) engine.ClassBody(ref);
    const auto et=fixture.Write(engine);
    Package other; other.stem="OtherClasses"; const auto op=other.ImportPackage("BasedClasses"),oa=other.Import("Probe",op); const auto otherLeaf=other.Export("Leaf",0,oa); other.ClassBody(otherLeaf); const auto ot=fixture.Write(other);
    Package classes; classes.stem="BasedClasses"; const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine"),ca=classes.Import("Actor",ce),ci=classes.Import("LevelInfo",ce);
    const auto probe=classes.Export("Probe",0,ca),leaf=classes.Export("Leaf",0,probe);
    const auto typedPawn=classes.Property("PawnCursor","ObjectProperty",probe,cc,1u,classes.Import("Pawn",ce));
    const auto cursor=classes.Property("Cursor","ObjectProperty",probe,cc,1u,ca),inner=classes.Property("Inner","ObjectProperty",probe,cc,1u,ca),target=classes.Property("Target","ObjectProperty",probe,cc,1u,ca);
    const auto born=classes.Property("Born","ObjectProperty",probe,cc,1u,ca),matches=classes.Property("Matches","ObjectProperty",probe,cc,256u,ca);
    const auto count=classes.Property("Count","IntProperty",probe,cc),innerCount=classes.Property("InnerCount","IntProperty",probe,cc);
    const auto base=classes.Import("Base",ca,"ObjectProperty"),hidden=classes.Import("bHidden",ca,"BoolProperty"),deleted=classes.Import("bDeleteMe",ca,"BoolProperty");
    const auto begun=classes.Import("bBegunPlay",ci,"BoolProperty"),actorLevel=classes.Import("Level",ca,"ObjectProperty");
    const auto declared=classes.Import("BasedActors",ca,"Function"),wrong=classes.Import("NotBasedActors",ca,"Function");
    const auto prop=[](std::int32_t ref) { return Ref(0x01u,ref); };
    const auto increment=[&](std::int32_t ref) { return Assign(prop(ref),Native(146u,{prop(ref),Int(1)})); };
    classes.Function("SetTarget",probe,cc); const auto st=static_cast<std::int32_t>(classes.exports.size()); const auto sti=classes.Property("Input","ObjectProperty",st,cc,1u,ca,0x80u); classes.FunctionCode(st,Join({Assign(prop(target),Ref(0x00u,sti)),Return()}));
    const auto rebase=classes.Function("Rebase",probe,cc),ri=classes.Property("Input","ObjectProperty",rebase,cc,1u,ca,0x80u); classes.FunctionCode(rebase,Join({Native(298u,{Ref(0x00u,ri)}),Return()}));
    classes.Function("Flags",probe,cc,Join({Assign(prop(hidden),Token(0x27u)),Assign(prop(deleted),Token(0x27u)),Return()}));
    classes.Function("Die",probe,cc,Return(Native(279u))); classes.Function("Outside",probe,cc,Join({Native(306u,{Ref(0x20u,ca),prop(cursor)}),Return()}));
    classes.Function("StopBegun",probe,cc,Join({Assign(Context(prop(actorLevel),prop(begun)),Token(0x28u)),Return()}));
    const std::vector<std::string> scans{"Scan","Declared","Early","Local","ArrayOut","ContextOut","PawnOut","AliasBase","RebaseFuture","RemoveFuture","RemoveSelf","Append","Nested","ThenFail","BadOut","BadTypedOut","BadClass","TooMany","WrongDeclared","WrongReceiver"};
    for(const auto& name:scans) {
        const auto fn=classes.Function(name,probe,cc); const auto input=classes.Property("ClassInput","ObjectProperty",fn,cc,1u,classes.Import("Object",cc),0x80u);
        const auto local=name=="Local"?classes.Property("LocalCursor","ObjectProperty",fn,cc,1u,ca):0;
        auto destination=name=="Local"?Ref(0x00u,local):name=="ArrayOut"?Array(matches,Int(255)):name=="AliasBase"?Context(prop(target),prop(base)):
            name=="ContextOut"?Context(prop(target),prop(cursor)):name=="PawnOut"?prop(typedPawn):name=="BadOut"?Token(0x2au):name=="BadTypedOut"?prop(count):prop(cursor);
        auto factory=name=="Declared"||name=="WrongDeclared"?Final(name=="Declared"?declared:wrong,{Ref(0x00u,input),destination}):
            name=="TooMany"?Native(306u,{Ref(0x00u,input),destination,Int(1)}):name=="BadClass"?Native(306u,{Int(7),destination}):Native(306u,{Ref(0x00u,input),destination});
        if(name=="WrongReceiver") factory=Context(Ref(0x20u,ca),std::move(factory));
        Flow f; f.Add(Assign(prop(count),Int(0))); f.Add(Assign(prop(innerCount),Int(0))); f.Iterator(factory,"done");
        f.Add(Assign(Array(matches,prop(count)),destination)); f.Add(increment(count));
        if(name=="Early") f.Add(Return(destination));
        if(name=="RebaseFuture") f.Add(Context(prop(target),Native(298u,{Token(0x17u)})));
        if(name=="RemoveFuture") f.Add(Context(prop(target),Native(279u)));
        if(name=="RemoveSelf") f.Add(Context(destination,Native(279u)));
        if(name=="Append") {
            f.IfNot(Native(154u,{prop(count),Int(1)}),"appended");
            f.Add(Assign(prop(born),Native(278u,{Ref(0x20u,leaf)}))); f.Add(Context(prop(born),Native(298u,{Token(0x17u)}))); f.Label("appended");
        }
        if(name=="Nested") { f.Iterator(Native(306u,{Ref(0x00u,input),prop(inner)}),"innerDone"); f.Add(increment(innerCount)); f.Add(Token(0x31u)); f.Label("innerDone"); f.Add(Token(0x30u)); }
        f.Add(Token(0x31u)); f.Label("done"); f.Add(Token(0x30u)); if(name=="ThenFail") f.Add(Native(999u)); f.Add(Return(destination)); classes.FunctionCode(fn,f.Finish());
    }
    classes.ClassBody(probe); classes.ClassBody(leaf); const auto bt=fixture.Write(classes);
    Package map; map.stem="BasedFixture"; const auto me=map.ImportPackage("Engine"),mb=map.ImportPackage("BasedClasses"),mo=map.ImportPackage("OtherClasses");
    const auto world=map.Export("MyLevel",map.Import("Level",me)),mapModel=map.Export("Model0",map.Import("Model",me));
    std::map<std::string,std::int32_t> actors;
    for(const auto& s:{"Level0","Root","A","Indirect","Future","B","Pawn0"}) {
        const std::string name=s; const auto cls=name=="Level0"?map.Import("LevelInfo",me):name=="Pawn0"?map.Import("Pawn",me):name=="B"?map.Import("Leaf",mo):map.Import(name=="Root"?"Probe":"Leaf",mb);
        actors.emplace(name,map.Export(name,cls));
    }
    for(unsigned i=0u;i<extras;++i) actors.emplace("Extra"+std::to_string(i),map.Export("Extra"+std::to_string(i),map.Import("Leaf",mb)));
    // A reflected Actor export outside the actual ULevel slot array must not
    // silently enumerate the current world's slots as its own native level.
    const auto unregistered=map.Export("Unregistered",map.Import("Probe",mb));
    map.Body(unregistered,Bytes{0u});
    for(const auto& [name,ref]:actors) { Bytes b; map.ObjectTag(b,"Level",actors.at("Level0")); map.ObjectTag(b,"XLevel",world); if(name=="A"||name=="B"||name=="Pawn0"||name.rfind("Extra",0u)==0u) map.ObjectTag(b,"Base",actors.at("Root")); if(name=="Indirect") map.ObjectTag(b,"Base",actors.at("A")); b.push_back(0u); map.Body(ref,std::move(b)); }
    // Actual serialized version68 ULevel. Actor order deliberately differs
    // from the cached BasedActors insertion order after runtime rebasing.
    Bytes levelBody{0u}; U32(levelBody,static_cast<std::uint32_t>(actors.size()+2u)); U32(levelBody,static_cast<std::uint32_t>(actors.size()+2u));
    Index(levelBody,actors.at("Level0")); Index(levelBody,0); Index(levelBody,actors.at("Root"));
    for(const auto& s:{"A","Indirect","Future","B","Pawn0"}) Index(levelBody,actors.at(s));
    for(unsigned i=0u;i<extras;++i) Index(levelBody,actors.at("Extra"+std::to_string(i)));
    Index(levelBody,0);
    for(unsigned i=0u;i<4u;++i) Index(levelBody,0); // Empty URL strings.
    Index(levelBody,0); U32(levelBody,7777u); U32(levelBody,0u); Index(levelBody,mapModel); Index(levelBody,0);
    map.Body(world,std::move(levelBody));
    // Valid empty UModel with the actual LevelInfo zone; no invented BSP floor.
    Bytes modelBody{0u}; for(unsigned i=0u;i<6u;++i) U32(modelBody,0u); modelBody.push_back(0u);
    for(unsigned i=0u;i<4u;++i) U32(modelBody,0u);
    for(unsigned i=0u;i<5u;++i) Index(modelBody,0);
    U32(modelBody,0u); U32(modelBody,1u); Index(modelBody,actors.at("Level0")); U64(modelBody,0u); U64(modelBody,0u);
    for(unsigned i=0u;i<7u;++i) Index(modelBody,0);
    U32(modelBody,1u); U32(modelBody,0u); map.Body(mapModel,std::move(modelBody));
    const auto mt=fixture.Write(map,true);
    Require(ReadPortableLevel68ActorOrder(mt).size()==actors.size()+2u,"Generated Level slots/holes did not decode");
    Require(LoadPortableModel68(mt,static_cast<std::size_t>(mapModel-1)).zones.size()==1u,"Generated empty native Model failed production decoder");
    return {{ct,et,ot,bt},mt};
}
void Matches(const std::vector<std::string>& expected) {
    Require(QuestVr::Vm::ToInt(Read("Root","Count"))==static_cast<std::int32_t>(expected.size()),"BasedActors wrong match count");
    for(std::size_t i=0;i<expected.size();++i) Require(Read("Root","Matches",static_cast<std::uint32_t>(i)).text==Path(expected[i]),"BasedActors wrong world-slot order/OUT identity");
}
void Scan(const std::vector<std::string>& expected,const std::string& cls="Engine.Actor",const std::string& fn="Scan") {
    Require(Call("Root",fn,{Cls(cls)}).value.text.empty(),"Exhaustion did not write Object None"); Matches(expected);
}
void Generated() {
    Fixture fixture; const auto tables=Build(fixture); Require(InitializePortableRuntime(tables.scripts).passed&&LoadPortableRuntimeMap(tables.map).passed,"Generated production runtime initialization failed"); fixture.baseline=fixture.Snapshot("Baseline");
    const auto outsideRevision=GetPortableRuntimeWorldRevision();
    const auto outside=ExecutePortableActorFunction(Path("Unregistered"),"Scan",{Cls("Engine.Actor")});
    Require(!outside.passed()&&!outside.committed&&outside.error=="Native receiver has no actual linked native Level",
        "Unregistered caller native world check: "+outside.error+" at "+outside.function+':'+std::to_string(outside.offset)+
            " passed="+std::to_string(outside.passed())+" committed="+std::to_string(outside.committed));
    Require(fixture.Snapshot("OutsideLevelAfter")==fixture.baseline&&GetPortableRuntimeWorldRevision()==outsideRevision,
        "Missing native world refusal retained pre-scan OUT or counter writes"); ++refusals;
    Scan({"A","B","Pawn0"}); Scan({"A","B","Pawn0"},"","Declared"); Scan({"A"},"BasedClasses.Leaf"); Scan({"B"},"OtherClasses.Leaf");
    Scan({"A","B","Pawn0"},"Core.Object"); Scan({},"Core.Class"); Scan({"Pawn0"},"Engine.Pawn");
    Require(Call("Root","Early",{Cls("Engine.Actor")}).value.text==Path("A"),"Early Return cleared current OUT"); Matches({"A"});
    Require(Read("Root","Cursor").text==Path("A"),"Early return failed to retain actor identity");
    Scan({"A","B","Pawn0"},"Engine.Actor","Local"); Require(Read("Root","Cursor").text==Path("A"),"Local OUT overwrote instance cursor");
    Scan({"A","B","Pawn0"},"Engine.Actor","ArrayOut"); Require(Read("Root","Matches",255u).text.empty(),"Array OUT failed exhaustion");
    Call("Root","SetTarget",{Obj("A")}); Scan({"A","B","Pawn0"},"Engine.Actor","ContextOut"); Require(Read("A","Cursor").text.empty(),"Context OUT failed exhaustion");
    Scan({"Pawn0"},"Engine.Pawn","PawnOut"); Refused(fixture,"PawnOut",{Cls("Engine.Actor")});
    fixture.Reset(); Call("B","Rebase",{Obj("")}); Call("B","Rebase",{Obj("Root")}); Scan({"A","B","Pawn0"}); // Reattached B is last in cached list, not in Level.
    fixture.Reset(); Call("Root","SetTarget",{Obj("Future")}); Scan({"A","Future","B","Pawn0"},"Engine.Actor","RebaseFuture");
    fixture.Reset(); Call("Root","SetTarget",{Obj("B")}); Scan({"A","Pawn0"},"Engine.Actor","RemoveFuture"); Scan({"A","Pawn0"});
    fixture.Reset(); Scan({"A","B","Pawn0"},"Engine.Actor","RemoveSelf"); Scan({});
    fixture.Reset(); Call("Root","SetTarget",{Obj("Future")}); Scan({"A","B","Pawn0"},"Engine.Actor","AliasBase"); Require(Read("Future","Base").text.empty(),"Retail pre-scan None OUT alias was omitted");
    fixture.Reset(); Call("A","Flags"); Scan({"A","B","Pawn0"});
    Require(DamagePortableRuntimeActor(Path("Pawn0"),1'000'000.0f).killed,"Owned generated Pawn failed inactive setup"); Scan({"A","B","Pawn0"});
    fixture.Reset(); Scan({"A","B","Pawn0"},"Engine.Actor","Nested"); Require(QuestVr::Vm::ToInt(Read("Root","InnerCount"))==9,"Nested iterators shared cursors"); Require(Read("Root","Inner").text.empty(),"Inner OUT not exhausted");
    fixture.Reset(); Call("Root","Append",{Cls("Engine.Actor")}); const auto born=Read("Root","Born").text;
    Require(!born.empty()&&Read("Root","Matches",3u).text==born&&QuestVr::Vm::ToInt(Read("Root","Count"))==4,"Live iterator missed actual appended/rebased birth");
    const auto committed=fixture.Snapshot("BornState"); Require(LoadPortableRuntimeState((fixture.directory/"BornState.sav").string())&&fixture.Snapshot("BornRoundTrip")==committed,"Iterator birth/save state failed exact replacement");
    Refused(fixture,"ThenFail",{Cls("Engine.Actor")}); fixture.Reset();
    for(const auto& fn:{"BadOut","BadTypedOut","BadClass","TooMany","WrongDeclared","WrongReceiver","Outside"}) Refused(fixture,fn,fn==std::string("Outside")?std::vector<Evaluation>{}:std::vector<Evaluation>{Cls("Engine.Actor")});
    Refused(fixture,"Scan",{Obj("A")}); Refused(fixture,"Scan",{Cls("Missing.Class")}); Refused(fixture,"Scan",{{Value::Integer(3),{}}});
    QuestVr::Vm::Limits limits; limits.writes=1u; Refused(fixture,"Scan",{Cls("Engine.Actor")},limits); // First None succeeds, first actor write fails.
    limits={}; limits.instructions=15u; Refused(fixture,"Scan",{Cls("Engine.Actor")},limits);
    limits={}; limits.retainedBytes=1u; Refused(fixture,"Scan",{Cls("Engine.Actor")},limits); fixture.Unchanged();
}
void MalformedDeclarations() {
    for(unsigned variant=1u;variant<=4u;++variant) {
        Fixture fixture; const auto tables=Build(fixture,variant); Require(InitializePortableRuntime(tables.scripts).passed&&LoadPortableRuntimeMap(tables.map).passed,"Malformed metadata fixture failed loading before iterator validation"); fixture.baseline=fixture.Snapshot("Baseline");
        Refused(fixture,"Scan",{Cls("Engine.Actor")}); Refused(fixture,"Declared",{Cls("Engine.Actor")}); fixture.Unchanged();
    }
}
void NativeWorkBudget() {
    Fixture fixture; const auto tables=Build(fixture,0u,160u); Require(InitializePortableRuntime(tables.scripts).passed&&LoadPortableRuntimeMap(tables.map).passed,"Native work fixture failed loading"); fixture.baseline=fixture.Snapshot("Baseline");
    QuestVr::Vm::Limits limits; limits.instructions=2'000'000u; limits.writes=200'000u;
    const auto result=ExecutePortableActorFunction(Path("Root"),"Nested",{Cls("Engine.Actor")},limits);
    Require(!result.passed()&&!result.committed&&result.error.find("Native actor lifecycle work budget exceeded")!=std::string::npos,"Nested live scans did not hit shared native work budget");
    Require(fixture.Snapshot("BudgetAfter")==fixture.baseline,"Native scan work refusal retained partial OUT writes"); ++refusals; fixture.Unchanged();
}
bool Original(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> sources;
    // Dispatch validates the complete loaded class graph. Load the original
    // class dependencies too rather than bypassing unresolved browser/UI bases.
    for(const auto& name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire","MPCharacters","DeusExSounds"})
        sources.push_back(root/"System"/(std::string(name)+".u"));
    sources.push_back(root/"Maps"/"00_Training.dx");
    for(const auto& path:sources) if(!std::filesystem::is_regular_file(path)) { std::cout<<"SKIP original BasedActors: missing owned original package "<<path.string()<<'\n'; return false; }
    std::map<std::filesystem::path,Bytes> before; std::vector<PortablePackageTables> packages;
    for(std::size_t i=0u;i<sources.size();++i) { before.emplace(sources[i],ReadBytes(sources[i])); if(i+1u<sources.size()) packages.push_back(LoadPortablePackageTables(sources[i].string())); }
    const auto& engine=packages.at(1u); const auto declaration=LoadPortableFunctionScript(engine,FindPortableExport(engine,"Actor.BasedActors"));
    Require(declaration.nativeIndex==306u&&declaration.functionFlags==0x405u,"Original BasedActors native declaration differs from retail audit");
    Require(InitializePortableRuntime(packages).passed&&LoadPortableRuntimeMap(LoadPortablePackageTables(sources.back().string())).passed,"Original read-only runtime/map metadata initialization failed");
    Fixture checkpoints;
    for(const auto& name:{"CrateBreakableMedGeneral2","CrateBreakableMedGeneral1"}) {
        const std::string actor="00_Training."+std::string(name); Require(ReadPortableActorScriptProperty(actor,"Base").text.empty(),"Original crate Base was not authored None");
        const auto result=ExecutePortableActorFunction(actor,"DropThings"); Require(result.passed(),"Original empty-based DropThings failed: "+result.error);
        Require(ReadPortableActorScriptProperty(actor,"HitPoints").integer==10,"DropThings changed original crate HitPoints");
        const auto baseline=checkpoints.directory/"OriginalBefore.sav";
        Require(SavePortableRuntimeState(baseline.string()),"Could not capture original crate probe baseline");
        // Select the genuine authored auto-state only in this isolated probe.
        // The port has not bootstrapped its world/player: this is not gameplay
        // initialization or permission to bypass startup/physics in the app.
        const auto select=ExecutePortableActorFunction(actor,"GotoState",{{Value::Text(Kind::Name,"Active"),{}}});
        Require(select.passed(),"Original crate Active state selection failed: "+select.error);
        const std::vector<Evaluation> hit{{Value::Integer(6),{}},{Value::Text(Kind::Object,{}),{}},
            {ReadPortableActorScriptProperty(actor,"Location"),{}},{Value::Vector({0.0f,0.0f,0.0f}),{}},
            {Value::Text(Kind::Name,"shot"),{}}};
        const auto surviving=ExecutePortableActorFunction(actor,"TakeDamage",hit);
        Require(surviving.passed(),"Original state-owned surviving crowbar-damage probe failed: "+surviving.error);
        Require(ReadPortableActorScriptProperty(actor,"HitPoints").integer==4 &&
            std::fabs(ReadPortableActorScriptProperty(actor,"ScaleGlow").floating-0.46f)<0.00001f,
            "Original crate damage6 did not preserve authored HitPoints/ResetScaleGlow");
        const auto fatalBefore=checkpoints.Snapshot("FatalBefore"); const auto fatalRevision=GetPortableRuntimeWorldRevision();
        const auto fatal=ExecutePortableActorFunction(actor,"TakeDamage",hit);
        Require(!fatal.passed()&&!fatal.committed&&fatal.error.find("Unsupported runtime native 264")!=std::string::npos,
            "Fatal crate probe stopped outside the documented PlaySound prerequisite: "+fatal.error+
                " at "+fatal.function+':'+std::to_string(fatal.offset));
        Require(checkpoints.Snapshot("FatalAfter")==fatalBefore&&GetPortableRuntimeWorldRevision()==fatalRevision,
            "Fatal original crate damage leaked HP/scale/base/birth/deletion/random writes before unsupported sound");
        // A real nonnull Pawn argument follows a different earlier branch.
        // This NPC is only a legal isolated test input, never a stand-in for
        // a possessed VR player or permission to skip original world startup.
        const std::string instigator="00_Training.JaimeReyes0";
        const auto isPawn=ExecutePortableActorFunction(instigator,"IsA",{{Value::Text(Kind::Name,"Pawn"),{}}});
        Require(isPawn.passed()&&QuestVr::Vm::ToBool(isPawn.value),"Original nonnull damage fixture is not an actual Pawn");
        auto instigatedHit=hit;instigatedHit[1u]={Value::Text(Kind::Object,instigator),{}};
        const auto instigated=ExecutePortableActorFunction(actor,"TakeDamage",instigatedHit);
        Require(!instigated.passed()&&!instigated.committed&&
            instigated.error.find("Unsupported runtime native 512")!=std::string::npos&&
            instigated.function=="DeusEx.DeusExDecoration.Active.TakeDamage"&&instigated.offset==428u,
            "Nonnull instigated fatal damage bypassed its earlier MakeNoise prerequisite: "+instigated.error+
                " at "+instigated.function+':'+std::to_string(instigated.offset));
        Require(checkpoints.Snapshot("InstigatedFatalAfter")==fatalBefore&&GetPortableRuntimeWorldRevision()==fatalRevision,
            "MakeNoise refusal leaked crate HP/Instigator or other complete-checkpoint effects");
        Require(LoadPortableRuntimeState(baseline.string()),"Could not restore isolated original crate probe");
        std::cout<<"PREREQUISITE "<<actor<<": actual Active.TakeDamage6 survives with HP4/ScaleGlow0.46; fatal original hit rolls back at "<<fatal.function<<':'<<fatal.offset<<" native264. Not live VR damage.\n";
        std::cout<<"PREREQUISITE "<<actor<<": legal nonnull Pawn-instigated fatal hit rolls back earlier at "
            <<instigated.function<<':'<<instigated.offset<<" native512, complete checkpoint unchanged. Not a possessed player.\n";
    }
    ShutdownPortableRuntime(); for(const auto& [path,b]:before) Require(ReadBytes(path)==b,"Original inspection changed a source package");
    std::cout<<"PASS original BasedActors306 declaration, both crate DropThings scans and isolated surviving/fatal-refusal damage probes; no DLL/EXE execution, live VR damage, physics, spawned loot, equipment or campaign startup claim\n"; return true;
}
} // namespace
int main(int argc,char** argv) {
    try {
        if(argc>2) throw std::runtime_error("Usage: portable_based_actors_test [original-game-root]");
        const auto gcBefore=GC::GetStats().numObjects;
        if(argc==2) { if(!Original(argv[1])) return 77; }
        else { Generated(); MalformedDeclarations(); NativeWorkBudget(); }
        ShutdownPortableRuntime(); GC::Collect(); Require(GC::GetStats().numObjects==gcBefore,"BasedActors test leaked rooted objects");
        std::cout<<"PASS retail BasedActors306: "<<checks<<" checks, "<<refusals<<" rejection controls; live Level/direct Base/exact UClass/OUT aliases, mutation/birth/deletion, shared budgets and rollback. No startup bypass.\n"; return 0;
    } catch(const std::exception& error) { ShutdownPortableRuntime(); std::cerr<<"BasedActors integration failed: "<<error.what()<<'\n'; return 1; }
}
