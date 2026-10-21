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
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// Commercial-data-free, genuine v68 UObject/native-Level fixtures. Expected
// bounds and hit packets below are literals, not production collision helpers.
namespace {
using Bytes=std::vector<std::uint8_t>;
using Vec=std::array<float,3u>;
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& reason) {++checks;if(!value)throw std::runtime_error(reason);}
void U16(Bytes& b,std::uint16_t n) {b.push_back(static_cast<std::uint8_t>(n));b.push_back(static_cast<std::uint8_t>(n>>8u));}
void U32(Bytes& b,std::uint32_t n) {U16(b,static_cast<std::uint16_t>(n));U16(b,static_cast<std::uint16_t>(n>>16u));}
void U64(Bytes& b,std::uint64_t n) {U32(b,static_cast<std::uint32_t>(n));U32(b,static_cast<std::uint32_t>(n>>32u));}
void Float(Bytes& b,float n) {U32(b,std::bit_cast<std::uint32_t>(n));}
void Index(Bytes& b,std::int32_t n) {
    auto m=static_cast<std::uint32_t>(n<0?-static_cast<std::int64_t>(n):n);
    auto c=static_cast<std::uint8_t>((m&63u)|(n<0?128u:0u));m>>=6u;if(m)c|=64u;b.push_back(c);
    while(m) {c=static_cast<std::uint8_t>(m&127u);m>>=7u;if(m)c|=128u;b.push_back(c);}
}
void Append(Bytes& b,const Bytes& tail) {b.insert(b.end(),tail.begin(),tail.end());}
void Replace32(Bytes& b,std::size_t at,std::uint32_t n) {for(unsigned i=0;i<4u;++i)b.at(at+i)=static_cast<std::uint8_t>(n>>(8u*i));}
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);Require(size<=32u*1024u*1024u,"Model fixture/save exceeds inspection bound");
    Bytes b(static_cast<std::size_t>(size));std::ifstream f(path,std::ios::binary);Require(bool(f),"Cannot inspect Model fixture/save");
    if(!b.empty())f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));
    Require(bool(f),"Model fixture/save is truncated");return b;
}
void WriteBytes(const std::filesystem::path& path,const Bytes& b) {
    std::ofstream f(path,std::ios::binary|std::ios::trunc);Require(bool(f),"Cannot create Model fixture");
    f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));Require(bool(f),"Cannot write Model fixture");
}
struct Code {Bytes raw;std::size_t logical{};};
Code Token(std::uint8_t op) {return {{op},1u};}
Code Ref(std::uint8_t op,std::int32_t ref) {Code c{{op},5u};Index(c.raw,ref);return c;}
Code Join(std::initializer_list<Code> parts) {Code c;for(const auto& p:parts){Append(c.raw,p.raw);c.logical+=p.logical;}return c;}
Code Return() {return Join({Token(0x04u),Token(0x0bu)});}
Code Assign(Code l,Code r) {return Join({Token(0x0fu),std::move(l),std::move(r)});}
struct Package {
    std::string stem;std::vector<std::string> names{"None"};std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;std::vector<std::function<Bytes(std::int32_t,std::int32_t)>> builders;
    std::map<std::int32_t,std::vector<std::int32_t>> children;
    std::int32_t Name(const std::string& n) {const auto it=std::find(names.begin(),names.end(),n);if(it!=names.end())return static_cast<std::int32_t>(it-names.begin());names.push_back(n);return static_cast<std::int32_t>(names.size()-1u);}
    std::int32_t Import(const std::string& n,std::int32_t outer,const std::string& type="Class") {imports.push_back({Name("Core"),Name(type),outer,Name(n)});return -static_cast<std::int32_t>(imports.size());}
    std::int32_t ImportPackage(const std::string& n) {return Import(n,0,"Package");}
    std::int32_t Export(const std::string& n,std::int32_t cls=0,std::int32_t base=0,std::int32_t outer=0,bool field=false) {
        exports.push_back({cls,base,outer,Name(n),ObjectFlags{},0,-1});builders.emplace_back();const auto ref=static_cast<std::int32_t>(exports.size());if(field)children[outer].push_back(ref);return ref;
    }
    void Native(std::int32_t ref) {exports.at(static_cast<std::size_t>(ref-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);}
    void Body(std::int32_t ref,Bytes b) {builders.at(static_cast<std::size_t>(ref-1))=[b=std::move(b)](std::int32_t,std::int32_t){return b;};}
    void ClassBody(std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref](std::int32_t next,std::int32_t first) {
            Bytes b;Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjBase);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);
            U32(b,0);U32(b,0);U32(b,0);U64(b,~std::uint64_t{});U64(b,~std::uint64_t{});U16(b,0xffffu);U32(b,0);U32(b,0);b.resize(b.size()+16u,0u);for(unsigned i=0;i<5u;++i)Index(b,0);return b;
        };
    }
    void StructBody(std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);U32(b,0);U32(b,0);U32(b,0);return b;
        };
    }
    std::int32_t Property(const std::string& n,const std::string& type,std::int32_t owner,std::int32_t core,std::int32_t target=0,std::uint32_t flags=0u) {
        const auto ref=Export(n,Import(type,core),0,owner,true);
        builders.at(static_cast<std::size_t>(ref-1))=[type,target,flags](std::int32_t next,std::int32_t) {
            Bytes b{0u};Index(b,0);Index(b,next);U32(b,1u);U32(b,flags);Index(b,0);if(flags&0x20u)U16(b,0);
            if(type=="StructProperty"||type=="ObjectProperty"||type=="ByteProperty") {Index(b,target);}
            return b;
        };return ref;
    }
    std::int32_t Function(const std::string& n,std::int32_t owner,std::int32_t core) {return Export(n,Import("Function",core),0,owner,true);}
    void FunctionBody(std::int32_t ref,Code code) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,code=std::move(code)](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);U32(b,0);U32(b,0);U32(b,static_cast<std::uint32_t>(code.logical));Append(b,code.raw);U16(b,0);b.push_back(0u);U32(b,2u);return b;
        };
    }
    void Tag(Bytes& b,const std::string& n,std::uint8_t type,const Bytes& data,const std::string& structure={}) {
        Require(data.size()<=255u,"Model fixture tag exceeds bound");Index(b,Name(n));b.push_back(static_cast<std::uint8_t>(type|0x50u));if(type==10u)Index(b,Name(structure));b.push_back(static_cast<std::uint8_t>(data.size()));Append(b,data);
    }
    void ObjectTag(Bytes& b,const std::string& n,std::int32_t ref) {Bytes d;Index(d,ref);Tag(b,n,5u,d);}
    void VectorTag(Bytes& b,const std::string& n,Vec v) {Bytes d;for(auto n2:v)Float(d,n2);Tag(b,n,10u,d,"Vector");}
    void ActorBody(std::int32_t ref,Bytes b={}) {Index(b,0);Body(ref,std::move(b));}
    Bytes Serialize() {
        std::map<std::int32_t,std::int32_t> next;for(const auto& [owner,list]:children){static_cast<void>(owner);for(std::size_t i=1;i<list.size();++i)next[list[i-1u]]=list[i];}
        Bytes b;U32(b,0x9e2a83c1u);U16(b,68);U16(b,0);b.resize(56u,0u);std::vector<Bytes> bodies;std::vector<std::int32_t> offsets;
        for(std::size_t i=0;i<exports.size();++i) {const auto ref=static_cast<std::int32_t>(i+1u);const auto it=children.find(ref);auto body=builders[i]?builders[i](next[ref],it==children.end()||it->second.empty()?0:it->second.front()):Bytes{};offsets.push_back(static_cast<std::int32_t>(b.size()));Append(b,body);bodies.push_back(std::move(body));}
        Replace32(b,12u,static_cast<std::uint32_t>(names.size()));Replace32(b,16u,static_cast<std::uint32_t>(b.size()));for(const auto& n:names){Index(b,static_cast<std::int32_t>(n.size()+1u));b.insert(b.end(),n.begin(),n.end());b.push_back(0);U32(b,0);}
        Replace32(b,20u,static_cast<std::uint32_t>(exports.size()));Replace32(b,24u,static_cast<std::uint32_t>(b.size()));
        for(std::size_t i=0;i<exports.size();++i){const auto& e=exports[i];Index(b,e.ObjClass);Index(b,e.ObjBase);U32(b,static_cast<std::uint32_t>(e.ObjOuter));Index(b,e.ObjName);U32(b,static_cast<std::uint32_t>(e.ObjFlags));Index(b,static_cast<std::int32_t>(bodies[i].size()));if(!bodies[i].empty())Index(b,offsets[i]);}
        Replace32(b,28u,static_cast<std::uint32_t>(imports.size()));Replace32(b,32u,static_cast<std::uint32_t>(b.size()));for(const auto& e:imports){Index(b,e.ClassPackage);Index(b,e.ClassName);U32(b,static_cast<std::uint32_t>(e.ObjOuter));Index(b,e.ObjName);}return b;
    }
};
struct Fixture {
    std::filesystem::path parent,directory;std::map<std::filesystem::path,std::pair<Bytes,std::filesystem::file_time_type>> sources;
    Fixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<20u;++i){const auto p=parent/("deusex-primitive-model-test-"+std::to_string(stamp)+'-'+std::to_string(i));if(std::filesystem::create_directory(p)){directory=std::filesystem::canonical(p);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Model fixture escaped temporary parent");std::filesystem::create_directory(directory/"System");std::filesystem::create_directory(directory/"Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime();std::error_code e;const auto actual=std::filesystem::weakly_canonical(directory,e);
        if(!e&&!directory.empty()&&actual==directory&&actual.parent_path()==parent&&actual.filename().string().rfind("deusex-primitive-model-test-",0u)==0u)std::filesystem::remove_all(actual,e);
    }
    PortablePackageTables Write(Package& p,bool map=false) {const auto path=directory/(map?"Maps":"System")/(p.stem+(map?".dx":".u"));const auto b=p.Serialize();WriteBytes(path,b);sources.insert_or_assign(path,std::pair{b,std::filesystem::last_write_time(path)});return LoadPortablePackageTables(path.string());}
    Bytes Snapshot(const std::string& n) const {const auto path=directory/(n+".sav");Require(SavePortableRuntimeState(path.string()),"Cannot snapshot Model fixture");return ReadBytes(path);}
    void Unchanged() const {for(const auto& [p,s]:sources)Require(ReadBytes(p)==s.first&&std::filesystem::last_write_time(p)==s.second,"Native Model query changed source bytes/mtime");}
};
// Includes the tagged-property None. The box spans X[-10,0], Y/Z[-10,10].
// All six hull planes are real BSP nodes; only the root has a collision hull.
Bytes ModelBody(bool box,bool outside=true,bool inactiveGarbage=false,bool badPlane=false) {
    Bytes b{0u};const float nan=std::bit_cast<float>(0x7fc12345u);
    for(float f:inactiveGarbage?std::array<float,6u>{0,10,10,-10,-10,-10}:std::array<float,6u>{-10,-10,-10,0,10,10})Float(b,f);
    b.push_back(inactiveGarbage?0xffu:1u);for(unsigned i=0;i<4u;++i)Float(b,inactiveGarbage?nan:0.0f);
    Index(b,box?3:0);if(box)for(float f:{1,0,0,0,1,0,0,0,1})Float(b,f);
    Index(b,box?3:0);if(box)for(float f:{0,0,0,0,1,0,0,0,1})Float(b,f);
    Index(b,box?6:0);
    if(box)for(unsigned i=0;i<6u;++i) {
        const std::array<std::array<float,4u>,6u> planes{{{1,0,0,0},{-1,0,0,10},{0,1,0,10},{0,-1,0,10},{0,0,1,10},{0,0,-1,10}}};
        for(unsigned j=0;j<4u;++j) {Float(b,badPlane&&i==5u&&j==0u?nan:planes[i][j]);}
        U64(b,0);b.push_back(0);
        Index(b,0);Index(b,0);Index(b,-1);Index(b,-1);Index(b,-1);Index(b,i==0u?0:-1);Index(b,-1);Index(b,0);Index(b,0);b.push_back(3);U32(b,0xffffffffu);U32(b,0xffffffffu);
    }
    Index(b,box?1:0);if(box){Index(b,0);U32(b,0);for(auto n:{0,0,1,2,-1,-1})Index(b,n);U16(b,0);U16(b,0);Index(b,0);}
    Index(b,box?3:0);if(box)for(auto n:{0,1,2}){Index(b,n);Index(b,-1);}U32(b,0);U32(b,0);
    for(unsigned i=0;i<4u;++i)Index(b,0); // Polys, lightmaps, lightbits, render bounds.
    Index(b,box?13:0);if(box){for(unsigned i=0;i<6u;++i)U32(b,i);U32(b,0xffffffffu);for(float f:{-10,-10,-10,0,10,10})Float(b,f);}
    Index(b,0);Index(b,0);U32(b,outside?1u:0u);U32(b,0);return b;
}
struct Tables {std::vector<PortablePackageTables> scripts;PortablePackageTables map;};
Tables Build(Fixture& fixture,bool replacement=false,bool inactiveGarbage=false,bool negativeModels=true) {
    Package core;core.stem="Core";const auto cp=core.ImportPackage("Core"),object=core.Export("Object"),meta=core.Export("Class",0,object);
    const auto vector=core.Export("Vector",core.Import("Struct",cp),0,object),rotator=core.Export("Rotator",core.Import("Struct",cp),0,object);
    for(const auto* n:{"X","Y","Z"})core.Property(n,"FloatProperty",vector,cp,0,1u);
    for(const auto* n:{"Pitch","Yaw","Roll"})core.Property(n,"IntProperty",rotator,cp,0,1u);
    core.ClassBody(object);core.ClassBody(meta);core.StructBody(vector);core.StructBody(rotator);const auto ct=fixture.Write(core);
    Package engine;engine.stem="Engine";const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec),ep=engine.ImportPackage("Engine");
    const auto ev=engine.Import("Vector",eo,"Struct"),er=engine.Import("Rotator",eo,"Struct"),model=engine.Import("Model",ep),mesh=engine.Import("Mesh",ep);
    const auto actor=engine.Export("Actor",0,eo),info=engine.Export("LevelInfo",0,actor),level=engine.Export("Level",0,eo);engine.Native(actor);
    engine.Property("Brush","ObjectProperty",actor,ec,model,0x2au);engine.Property("Mesh","ObjectProperty",actor,ec,mesh,0x21u);
    engine.Property("Location","StructProperty",actor,ec,ev,0x23u);engine.Property("Rotation","StructProperty",actor,ec,er,0x23u);
    engine.Property("Level","ObjectProperty",actor,ec,info);engine.Property("XLevel","ObjectProperty",actor,ec,level);
    engine.Property("CollisionRadius","FloatProperty",actor,ec,0,0x23u);engine.Property("CollisionHeight","FloatProperty",actor,ec,0,0x23u);
    engine.Property("bBegunPlay","BoolProperty",info,ec);engine.ClassBody(actor);engine.ClassBody(info);engine.ClassBody(level);const auto et=fixture.Write(engine);
    Package assets;assets.stem="OtherModels";const auto ae=assets.ImportPackage("Engine");assets.Body(assets.Export("ExternalEmpty",assets.Import("Model",ae)),ModelBody(false));const auto at=fixture.Write(assets);
    Package classes;classes.stem="ModelClasses";const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine"),ca=classes.Import("Actor",ce);
    const auto probe=classes.Export("Probe",0,ca),unknown=classes.Export("UnknownNative",0,probe);classes.Native(unknown);
    const auto fn=classes.Function("SetLocation",probe,cc),value=classes.Property("Value","StructProperty",fn,cc,classes.Import("Vector",classes.Import("Object",cc),"Struct"),0x80u);
    classes.FunctionBody(fn,Join({Assign(Ref(0x01u,classes.Import("Location",ca,"StructProperty")),Ref(0x00u,value)),Return()}));
    classes.ClassBody(probe);classes.ClassBody(unknown);const auto st=fixture.Write(classes);
    Package map;map.stem="PrimitiveModelFixture";const auto me=map.ImportPackage("Engine"),mc=map.ImportPackage("ModelClasses"),mo=map.ImportPackage("OtherModels"),mm=map.Import("Model",me);
    const auto root=map.Export("MisleadingRoot",mm),world=map.Export("MyLevel",map.Import("Level",me)),box=map.Export("SelectedBox",mm);
    const auto emptyOutside=map.Export("EmptyOutside",mm),emptyInside=map.Export("EmptyInside",mm),malformed=map.Export("MalformedModel",mm),badPlane=map.Export("BadPlane",mm);
    map.Body(root,ModelBody(false,true));map.Body(box,ModelBody(!replacement,true,inactiveGarbage));map.Body(emptyOutside,ModelBody(false,true,inactiveGarbage));map.Body(emptyInside,ModelBody(false,false,inactiveGarbage));map.Body(malformed,negativeModels?Bytes{0u,0xffu}:ModelBody(false));map.Body(badPlane,ModelBody(true,true,false,negativeModels));
    const auto levelInfo=map.Export("Level0",map.Import("LevelInfo",me));std::vector<std::int32_t> order{levelInfo};map.ActorBody(levelInfo);
    for(const auto& [name,selected,unknownClass,external]:std::vector<std::tuple<std::string,std::int32_t,bool,bool>>{
        {"Driver",box,false,false},{"EmptyOutsideActor",emptyOutside,false,false},{"EmptyInsideActor",emptyInside,false,false},
        {"UnknownEmpty",emptyOutside,true,false},{"UnknownBox",box,true,false},{"Malformed",malformed,false,false},
        {"BadPlaneActor",badPlane,false,false},{"External",0,false,true}}) {
        const auto actorRef=map.Export(name,map.Import(unknownClass?"UnknownNative":"Probe",mc));order.push_back(actorRef);Bytes tags;
        map.ObjectTag(tags,"Brush",external?map.Import("ExternalEmpty",mo,"Model"):selected);map.ObjectTag(tags,"Level",levelInfo);map.ObjectTag(tags,"XLevel",world);map.ActorBody(actorRef,tags);
    }
    Bytes wb{0u};U32(wb,static_cast<std::uint32_t>(order.size()));U32(wb,static_cast<std::uint32_t>(order.size()));for(auto ref:order)Index(wb,ref);
    for(unsigned i=0;i<5u;++i) {Index(wb,0);}
    U32(wb,7777);U32(wb,0);Index(wb,root);Index(wb,0);
    U32(wb,0u);Index(wb,0); // Original serialized Float time and preliminary +100.
    for(unsigned i=0u;i<16u;++i)Index(wb,0); // +9C pointer sequence, including final +100 alias.
    map.Body(world,std::move(wb));
    return {{ct,et,at,st},fixture.Write(map,true)};
}
std::string Path(const std::string& n) {return "PrimitiveModelFixture."+n;}
void Initialize(const Tables& t) {Require(InitializePortableRuntime(t.scripts).passed&&LoadPortableRuntimeMap(t.map).passed,"Cannot initialize genuine selected Model fixture");}
template<class F> void Refused(F&& op,const std::string& why) {bool rejected{};try{op();}catch(const std::exception&){rejected=true;}Require(rejected,why);++refusals;}
void Near(Vec a,Vec b,const std::string& why) {for(std::size_t i=0;i<3u;++i)Require(std::isfinite(a[i])&&std::abs(a[i]-b[i])<=0.00003f,why+" component "+std::to_string(i));}
bool SameCache(const PortablePrimitiveModelCacheSnapshot& a,const PortablePrimitiveModelCacheSnapshot& b) {
    if(a.retainedBytes!=b.retainedBytes||a.maximumEntries!=b.maximumEntries||a.maximumRetainedBytes!=b.maximumRetainedBytes||a.entries.size()!=b.entries.size())return false;
    for(std::size_t i=0;i<a.entries.size();++i){const auto& x=a.entries[i];const auto& y=b.entries[i];if(x.mapGeneration!=y.mapGeneration||x.levelPath!=y.levelPath||x.modelPath!=y.modelPath||x.sourcePath!=y.sourcePath||x.modelExportIndex!=y.modelExportIndex||x.cachedNodes!=y.cachedNodes)return false;}return true;
}
void CacheEmpty() {const auto c=ReadPortableRuntimePrimitiveModelCache();Require(c.entries.empty()&&c.retainedBytes==0u&&c.maximumEntries>0u&&c.maximumRetainedBytes>0u,"Selected Model cache lacks a clean bounded lifetime");}
void PacketSeed(const PortableActorPrimitiveLineSnapshot& p,bool clear,const std::string& name) {
    Require(p.primitive.actorPath==Path(name)&&p.primitive.nativeClassPath=="Engine.Model"&&p.primitive.lineImplementation=="UModel::LineCheck"&&
        p.clear==clear&&!p.hitOwner&&p.hitPrimitiveIsNone&&p.item==-1&&p.time==1.0f,"Empty Model changed fixed caller seed or RootOutside");
    for(std::size_t i=0u;i<3u;++i) {
        Require(std::bit_cast<std::uint32_t>(p.location[i])==0u,"Empty packet location changed literal positive-zero seed bits");
        Require(std::bit_cast<std::uint32_t>(p.normal[i])==0u,"Empty packet normal changed literal positive-zero seed bits");
    }
}
auto Sweep(const char* actor="Driver") {return ProbePortableRuntimeActorPrimitiveLine(Path(actor),{-20,0,0},{10,0,0},{1,1,1});}
void LiteralBox(const char* actor="Driver",Vec offset={}) {
    const auto b=ReadPortableRuntimeActorPrimitiveBounds(Path(actor));Require(b.primitive.primitivePath==Path("SelectedBox")&&b.primitive.boundsImplementation=="UModel::GetCollisionBoundingBox","Bounds did not select actual Brush Model");
    Near(b.minimum,{offset[0]-10,offset[1]-10,offset[2]-10},"Unpadded Model minimum");Near(b.maximum,{offset[0],offset[1]+10,offset[2]+10},"Unpadded Model maximum");
}
std::vector<std::string> Actors() {std::vector<std::string> result;for(const auto& a:GetPortableRuntimeMapActors(true))result.push_back(a.objectPath+'|'+a.classPath);return result;}
template<class F> void GameplayUnchanged(Fixture& f,F&& op) {
    const auto before=f.Snapshot("BeforeRead");const auto actors=Actors();const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();const auto time=ReadPortableRuntimeLevelTime();const bool dirty=GetPortableRuntimeUnsavedScriptState();op();const auto after=GC::GetStats();
    Require(f.Snapshot("AfterRead")==before&&GetPortableRuntimeWorldRevision()==revision&&Actors()==actors&&ReadPortableRuntimeLevelTime()==time&&GetPortableRuntimeUnsavedScriptState()==dirty&&gc.numObjects==after.numObjects&&gc.memoryUsage==after.memoryUsage,"Model query changed checkpoint/revision/actors/clock/dirty/GC state");f.Unchanged();
}
void Queries() {
    Fixture f;const auto t=Build(f);Initialize(t);CacheEmpty();const auto root=ReadPortableRuntimeNativeModel();Require(root.available&&!root.geometryCached&&root.modelPath==Path("MisleadingRoot"),"Root Model fixture binding is not independent of selected Brush");
    GameplayUnchanged(f,[&] {
        const auto emptyCache=ReadPortableRuntimePrimitiveModelCache();Refused([&]{(void)ReadPortableRuntimeActorPrimitiveBounds(Path("Malformed"));},"Malformed selected Model body was accepted");Require(SameCache(emptyCache,ReadPortableRuntimePrimitiveModelCache()),"Failed decode published entry/retained-byte ledger");
        LiteralBox();const auto first=ReadPortableRuntimePrimitiveModelCache();Require(first.entries.size()==1u&&first.retainedBytes>0u&&first.retainedBytes<=first.maximumRetainedBytes&&first.entries.size()<=first.maximumEntries,"Cold selected Model was not retained under aggregate limits");
        const auto& entry=first.entries.front();Require(entry.mapGeneration==root.mapGeneration&&entry.levelPath==root.levelPath&&entry.modelPath==Path("SelectedBox")&&entry.modelExportIndex==2u&&entry.cachedNodes==6u&&entry.sourcePath==t.map.sourcePath,"Selected Model cache key lacks actual generation/Level/export/source authority");
        for(unsigned i=0;i<24u;++i){LiteralBox();const auto h=Sweep();Require(!h.clear&&h.hitOwner&&!h.hitPrimitiveIsNone&&h.item==-1&&std::abs(h.time-0.2f)<=0.000003f&&h.primitive.primitivePath==Path("SelectedBox"),"Literal owned Model hull hit differs or selected root/cylinder");Near(h.location,{4,0,0},"Literal backed-off hull hit");Near(h.normal,{1,0,0},"Literal hull hit normal");}
        Require(SameCache(first,ReadPortableRuntimePrimitiveModelCache()),"Repeated warm query accumulated retained entries/bytes");
        const auto miss=ProbePortableRuntimeActorPrimitiveLine(Path("Driver"),{20,30,0},{10,30,0},{1,1,1},0xffffffffu);Require(miss.clear&&miss.time==2.0f&&!miss.hitOwner&&miss.hitPrimitiveIsNone&&miss.item==-1,"Nonempty miss did not write only caller Time2");Near(miss.location,{0,0,0},"Miss location seed");Near(miss.normal,{0,0,0},"Miss normal seed");
        const auto point=ProbePortableRuntimeActorPrimitiveLine(Path("Driver"),{-20,0,0},{10,0,0});
        Require(!point.clear&&point.hitOwner&&!point.hitPrimitiveIsNone&&point.item==0&&
            std::abs(point.time-0.31666666f)<=0.0000001f,"Literal owned point query lost node/owner/Model/time");
        Near(point.location,{0.5f,0,0},"Point half-unit backoff");Near(point.normal,{1,0,0},"Owned point normal");
        const auto pointMiss=ProbePortableRuntimeActorPrimitiveLine(Path("Driver"),{20,30,0},{10,30,0});
        Require(pointMiss.clear&&pointMiss.time==1.0f&&!pointMiss.hitOwner&&pointMiss.hitPrimitiveIsNone&&pointMiss.item==-1,
            "Point miss changed full caller seed (extent Time2 semantics leaked)");
        Near(pointMiss.location,{0,0,0},"Point miss location");Near(pointMiss.normal,{0,0,0},"Point miss normal");
        Refused([&]{(void)ReadPortableRuntimeActorPrimitiveBounds(Path("UnknownBox"));},"Unknown native ToWorld was guessed for Model bounds");Refused([&]{(void)Sweep("UnknownBox");},"Unknown native ToWorld was guessed for nonempty Model query");Require(SameCache(first,ReadPortableRuntimePrimitiveModelCache()),"Failed warm query discarded/published independent immutable cache state");
        const float nan=std::bit_cast<float>(0x7f800001u),inf=std::bit_cast<float>(0x7f800000u);
        const auto discarded=ProbePortableRuntimeActorPrimitiveLine(Path("Driver"),{-20,0,0},{10,0,0},{nan,-0.0f,nan});
        Require(discarded.item==point.item&&discarded.clear==point.clear&&discarded.time==point.time&&
            discarded.location==point.location&&discarded.normal==point.normal,"Point extent branch failed to discard inactive NaNs");
        Refused([&]{(void)ProbePortableRuntimeActorPrimitiveLine(Path("Driver"),{-20,0,0},{10,0,0},{nan,1,0});},
            "Active nonzero extent silently discarded earlier NaN");
        for(const auto& [name,clear]:std::vector<std::pair<std::string,bool>>{{"EmptyOutsideActor",true},{"EmptyInsideActor",false},{"UnknownEmpty",true}})
            PacketSeed(ProbePortableRuntimeActorPrimitiveLine(Path(name),{nan,inf,nan},{inf,nan,inf},{nan,-1,inf},0xffffffffu),clear,name);
        PacketSeed(ProbePortableRuntimeActorPrimitiveLine(Path("External"),{nan,inf,nan},{inf,nan,inf},{nan,-1,inf}),true,"External");
        const auto several=ReadPortableRuntimePrimitiveModelCache();Require(several.entries.size()==4u&&several.retainedBytes>first.retainedBytes&&several.retainedBytes<=several.maximumRetainedBytes,"Distinct Models did not share a bounded aggregate ledger");
        const auto external=std::find_if(several.entries.begin(),several.entries.end(),[](const auto& e){return e.modelPath=="OtherModels.ExternalEmpty";});Require(external!=several.entries.end()&&external->sourcePath!=t.map.sourcePath&&external->levelPath==root.levelPath&&external->mapGeneration==root.mapGeneration,"Cross-package actual Model was refused or keyed to foreign Level");
        Refused([&]{(void)Sweep("BadPlaneActor");},"Nonfinite active BSP plane was accepted");const auto afterBad=ReadPortableRuntimePrimitiveModelCache();Require(afterBad.entries.size()==several.entries.size()+1u,"Valid decoded immutable geometry was not retained after active-kernel refusal");Refused([&]{(void)Sweep("BadPlaneActor");},"Cached nonfinite active BSP plane skipped full validation");Require(SameCache(afterBad,ReadPortableRuntimePrimitiveModelCache()),"Repeated invalid query changed cache ledger");
        const auto untouchedRoot=ReadPortableRuntimeNativeModel();Require(!untouchedRoot.geometryCached&&untouchedRoot.mapGeneration==root.mapGeneration&&untouchedRoot.modelPath==root.modelPath,"Brush queries redirected/populated native root FastTrace cache");
    });
}
void InactiveNumbers() {
    Fixture f;const auto t=Build(f,false,true);Initialize(t);
    GameplayUnchanged(f,[&] {LiteralBox();const float nan=std::bit_cast<float>(0x7f800001u);PacketSeed(ProbePortableRuntimeActorPrimitiveLine(Path("UnknownEmpty"),{nan,nan,nan},{nan,nan,nan},{nan,-1,nan}),true,"UnknownEmpty");
        const auto b=ReadPortableRuntimeActorPrimitiveBounds(Path("EmptyOutsideActor"));Near(b.minimum,{-10,-10,-10},"Empty/reversed source Valid255 bounds minimum");Near(b.maximum,{0,10,10},"Empty/reversed source Valid255 bounds maximum");});
}
void Lifetime() {
    Fixture f;auto t=Build(f,false,false,false);Initialize(t);const auto baseline=f.Snapshot("Baseline");const auto original=ReadPortableRuntimeNativeModel();LiteralBox();const auto cached=ReadPortableRuntimePrimitiveModelCache();
    const auto set=ExecutePortableActorFunction(Path("Driver"),"SetLocation",{{Value::Vector({2,4,8}),{}}});Require(set.passed()&&set.committed,"Normal VM writer failed with render-valid Model fixture: "+set.error);LiteralBox("Driver",{2,4,8});Require(SameCache(cached,ReadPortableRuntimePrimitiveModelCache()),"Live owner transform mutation rebuilt immutable Model cache");
    Require(LoadPortableRuntimeState((f.directory/"Baseline.sav").string()),"Warm Model checkpoint replacement failed");Require(f.Snapshot("WarmRestored")==baseline&&SameCache(cached,ReadPortableRuntimePrimitiveModelCache()),"Warm restore serialized/replaced native geometry cache");LiteralBox();
    ShutdownPortableRuntime();CacheEmpty();Initialize(t);const auto cold=ReadPortableRuntimeNativeModel();Require(cold.mapGeneration>original.mapGeneration,"Cold load reused native Model generation");CacheEmpty();Require(LoadPortableRuntimeState((f.directory/"Baseline.sav").string()),"Cold Model checkpoint restoration failed");CacheEmpty();LiteralBox();
    const auto prior=ReadPortableRuntimeNativeModel();t=Build(f,true,false,false);Require(LoadPortableRuntimeMap(t.map).passed,"Same-filename Model replacement failed");const auto replaced=ReadPortableRuntimeNativeModel();Require(replaced.mapGeneration>prior.mapGeneration&&replaced.modelPath==prior.modelPath,"Same-filename map replacement reused native generation");CacheEmpty();PacketSeed(Sweep(),true,"Driver");Require(ReadPortableRuntimePrimitiveModelCache().entries.front().cachedNodes==0u,"Replacement Model revived stale six-node geometry");
    Require(UnloadPortableRuntimeMap()!=0u,"Model map unload removed no exports");CacheEmpty();Require(LoadPortableRuntimeMap(t.map).passed,"Post-unload Model reload failed");CacheEmpty();Require(ReadPortableRuntimeNativeModel().mapGeneration>replaced.mapGeneration,"Post-unload reload reused native generation");ShutdownPortableRuntime();CacheEmpty();f.Unchanged();
}
} // namespace
int main() {
    try {ShutdownPortableRuntime();GC::Collect();const auto baseline=GC::GetStats();Queries();InactiveNumbers();Lifetime();ShutdownPortableRuntime();GC::Collect();Require(GC::GetStats().numObjects==baseline.numObjects&&GC::GetStats().memoryUsage==baseline.memoryUsage,"Selected Model tests leaked GC roots");std::cout<<"PASS live selected Model collision provider: "<<checks<<" checks, "<<refusals<<" refusals; literal bounds/extent/point packets, inactive empty fields, actual Level/export/cache identity, aggregate ledger, source/state invariants and warm/cold lifetime. Not Move/falling, crate gameplay or headset evidence.\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL live selected Model provider: "<<e.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
