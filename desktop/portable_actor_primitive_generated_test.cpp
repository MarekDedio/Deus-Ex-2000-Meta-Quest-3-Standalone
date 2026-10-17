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
#include <utility>
#include <vector>

// Commercial-data-free live primitive VALUE/provider integration. Read-only
// fixtures deliberately have unusable geometry; mutable fixtures have actual
// minimal geometry required by normal committed-world asset preparation.
// Expected bounds/packets are literals, never production math helper calls.
namespace {
using Bytes=std::vector<std::uint8_t>;
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
using Vec=std::array<float,3u>;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why) {++checks;if(!value) throw std::runtime_error(why);}
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
void Replace32(Bytes& b,std::size_t at,std::uint32_t n) {for(unsigned i=0u;i<4u;++i)b.at(at+i)=static_cast<std::uint8_t>(n>>(8u*i));}
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto count=std::filesystem::file_size(path);Require(count<=16u*1024u*1024u,"Primitive fixture/save exceeds bound");
    Bytes b(static_cast<std::size_t>(count));std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot read primitive fixture/save");
    if(!b.empty())file.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));
    Require(bool(file),"Primitive fixture/save is truncated");return b;
}
void WriteBytes(const std::filesystem::path& path,const Bytes& b) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);Require(bool(file),"Cannot create generated primitive fixture");
    file.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));Require(bool(file),"Cannot write primitive fixture");
}
struct Code {Bytes raw;std::size_t logical{};};
Code Token(std::uint8_t op) {return {{op},1u};}
Code Ref(std::uint8_t op,std::int32_t ref) {Code c{{op},5u};Index(c.raw,ref);return c;}
Code Join(std::initializer_list<Code> parts) {Code c;for(const auto& p:parts){Append(c.raw,p.raw);c.logical+=p.logical;}return c;}
Code Return() {return Join({Token(0x04u),Token(0x0bu)});}
Code Assign(Code left,Code right) {return Join({Token(0x0fu),std::move(left),std::move(right)});}
Code Unsupported() {return {{0x63u,0xe7u,0x16u},3u};} // native999, EndFunctionParms
struct Package {
    std::string stem;std::vector<std::string> names{"None"};std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;std::vector<std::function<Bytes(std::int32_t,std::int32_t)>> builders;
    std::map<std::int32_t,std::vector<std::int32_t>> children;
    std::map<std::int32_t,std::vector<std::pair<std::size_t,std::size_t>>> absoluteEnds;
    std::int32_t Name(const std::string& name) {
        const auto it=std::find(names.begin(),names.end(),name);if(it!=names.end())return static_cast<std::int32_t>(it-names.begin());
        names.push_back(name);return static_cast<std::int32_t>(names.size()-1u);
    }
    std::int32_t Import(const std::string& name,std::int32_t outer,const std::string& type="Class",const std::string& source="Core") {
        imports.push_back({Name(source),Name(type),outer,Name(name)});return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) {return Import(name,0,"Package");}
    std::int32_t Export(const std::string& name,std::int32_t cls=0,std::int32_t base=0,std::int32_t outer=0,bool field=false) {
        exports.push_back({cls,base,outer,Name(name),ObjectFlags{},0,-1});builders.emplace_back();
        const auto ref=static_cast<std::int32_t>(exports.size());if(field)children[outer].push_back(ref);return ref;
    }
    void Native(std::int32_t ref) {exports.at(static_cast<std::size_t>(ref-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);}
    void Body(std::int32_t ref,Bytes b) {builders.at(static_cast<std::size_t>(ref-1))=[b=std::move(b)](std::int32_t,std::int32_t){return b;};}
    void Tag(Bytes& b,const std::string& name,std::uint8_t type,const Bytes& data,const std::string& structure={}) {
        Require(data.size()<=255u,"Primitive property tag exceeds bound");Index(b,Name(name));b.push_back(static_cast<std::uint8_t>(type|0x50u));
        if(type==10u)Index(b,Name(structure));
        b.push_back(static_cast<std::uint8_t>(data.size()));Append(b,data);
    }
    void ObjectTag(Bytes& b,const std::string& name,std::int32_t ref) {Bytes data;Index(data,ref);Tag(b,name,5u,data);}
    void FloatTag(Bytes& b,const std::string& name,float value) {Bytes data;Float(data,value);Tag(b,name,4u,data);}
    void VectorTag(Bytes& b,const std::string& name,Vec value) {Bytes data;for(const auto n:value)Float(data,n);Tag(b,name,10u,data,"Vector");}
    void ClassBody(std::int32_t ref,Bytes defaults={}) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,defaults=std::move(defaults)](std::int32_t next,std::int32_t first) {
            const auto& e=exports.at(static_cast<std::size_t>(ref-1));Bytes b;Index(b,e.ObjBase);Index(b,next);Index(b,0);Index(b,first);Index(b,e.ObjName);
            U32(b,0);U32(b,0);U32(b,0);U64(b,~std::uint64_t{});U64(b,~std::uint64_t{});U16(b,0xffffu);U32(b,0);U32(b,0);
            b.resize(b.size()+16u,0u);for(unsigned i=0u;i<4u;++i)Index(b,0);Append(b,defaults);Index(b,0);return b;
        };
    }
    void StructBody(std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);
            U32(b,0);U32(b,0);U32(b,0);return b;
        };
    }
    std::int32_t Property(const std::string& name,const std::string& type,std::int32_t owner,std::int32_t core,
        std::int32_t target=0,std::uint32_t flags=0u,std::uint32_t dim=1u) {
        const auto ref=Export(name,Import(type,core),0,owner,true);
        builders.at(static_cast<std::size_t>(ref-1))=[type,target,flags,dim](std::int32_t next,std::int32_t) {
            Bytes b{0u};Index(b,0);Index(b,next);U32(b,dim);U32(b,flags);Index(b,0);if(flags&0x20u)U16(b,0);
            if(type=="StructProperty"||type=="ByteProperty"||type=="ObjectProperty")Index(b,target);
            return b;
        };return ref;
    }
    std::int32_t Function(const std::string& name,std::int32_t owner,std::int32_t core) {return Export(name,Import("Function",core),0,owner,true);}
    void FunctionBody(std::int32_t ref,Code code) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,code=std::move(code)](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);
            U32(b,0);U32(b,0);U32(b,static_cast<std::uint32_t>(code.logical));Append(b,code.raw);U16(b,0);b.push_back(0u);U32(b,2u);return b;
        };
    }
    void ActorBody(std::int32_t ref,Bytes tags={}) {Index(tags,0);Body(ref,std::move(tags));}
    Bytes Serialize() {
        std::map<std::int32_t,std::int32_t> next;
        for(const auto& [owner,list]:children) {static_cast<void>(owner);for(std::size_t i=1u;i<list.size();++i)next[list[i-1u]]=list[i];}
        Bytes b;U32(b,0x9e2a83c1u);U16(b,68u);U16(b,0);b.resize(56u,0u);std::vector<Bytes> bodies;std::vector<std::int32_t> offsets;
        for(std::size_t i=0u;i<exports.size();++i) {const auto ref=static_cast<std::int32_t>(i+1u);const auto it=children.find(ref);
            auto body=builders[i]?builders[i](next[ref],it==children.end()||it->second.empty()?0:it->second.front()):Bytes{};
            for(const auto& [pointer,end]:absoluteEnds[ref])Replace32(body,pointer,static_cast<std::uint32_t>(b.size()+end));
            offsets.push_back(static_cast<std::int32_t>(b.size()));Append(b,body);bodies.push_back(std::move(body));}
        Replace32(b,12u,static_cast<std::uint32_t>(names.size()));Replace32(b,16u,static_cast<std::uint32_t>(b.size()));
        for(const auto& name:names) {Index(b,static_cast<std::int32_t>(name.size()+1u));b.insert(b.end(),name.begin(),name.end());b.push_back(0);U32(b,0);}
        Replace32(b,20u,static_cast<std::uint32_t>(exports.size()));Replace32(b,24u,static_cast<std::uint32_t>(b.size()));
        for(std::size_t i=0u;i<exports.size();++i) {const auto& e=exports[i];Index(b,e.ObjClass);Index(b,e.ObjBase);U32(b,static_cast<std::uint32_t>(e.ObjOuter));
            Index(b,e.ObjName);U32(b,static_cast<std::uint32_t>(e.ObjFlags));Index(b,static_cast<std::int32_t>(bodies[i].size()));if(!bodies[i].empty())Index(b,offsets[i]);}
        Replace32(b,28u,static_cast<std::uint32_t>(imports.size()));Replace32(b,32u,static_cast<std::uint32_t>(b.size()));
        for(const auto& e:imports) {Index(b,e.ClassPackage);Index(b,e.ClassName);U32(b,static_cast<std::uint32_t>(e.ObjOuter));Index(b,e.ObjName);}return b;
    }
};
// Real v68 native Mesh/LodMesh serialization for the mutable publication path.
// Adapted locally from the existing independent generated mesh fixture; lazy
// array pointers are relocated by Package::Serialize, never fake decoder data.
void MeshBody(Package& package,std::int32_t reference,bool lod) {
    Bytes b{0u};b.insert(b.end(),41u,0u);
    const auto lazy=[&](const std::function<void()>& contents) {
        const auto pointer=b.size();U32(b,0);contents();package.absoluteEnds[reference].emplace_back(pointer,b.size());
    };
    lazy([&] {Index(b,4);for(const auto& point:std::array<std::array<std::uint16_t,3u>,4u>{{{0,0,0},{10,0,0},{0,10,0},{0,0,10}}}) {
        for(const auto component:point)U16(b,component);
        U16(b,0);
    }});
    lazy([&] {Index(b,lod?0:2);if(!lod)for(unsigned face=0u;face<2u;++face) {
        for(const auto vertex:face==0u?std::array<unsigned,3u>{0,1,2}:std::array<unsigned,3u>{0,3,1})U16(b,static_cast<std::uint16_t>(vertex));
        for(unsigned corner=0u;corner<3u;++corner) {b.push_back(static_cast<std::uint8_t>(corner*100u));b.push_back(static_cast<std::uint8_t>(face*100u));}
        U32(b,0);U32(b,face);
    }});
    Index(b,0);lazy([&] {Index(b,0);});b.insert(b.end(),41u,0u);lazy([&] {Index(b,0);});
    Index(b,3);for(unsigned i=0u;i<3u;++i)Index(b,0);Index(b,0);Index(b,0);
    U32(b,4);U32(b,1);U32(b,0);U32(b,0);for(float n:{1,1,1,0,0,0})Float(b,n);
    U32(b,0);U32(b,0);U32(b,0);U32(b,0);U32(b,0);Index(b,0);
    if(lod) {
        Index(b,0);Index(b,0);Index(b,2);
        for(unsigned face=0u;face<2u;++face) {
            for(const auto wedge:face==0u?std::array<unsigned,3u>{0,1,2}:std::array<unsigned,3u>{0,3,1})U16(b,static_cast<std::uint16_t>(wedge));
            U16(b,static_cast<std::uint16_t>(face));
        }
        Index(b,0);Index(b,4);for(unsigned vertex=0u;vertex<4u;++vertex) {
            U16(b,static_cast<std::uint16_t>(vertex));b.push_back(static_cast<std::uint8_t>(vertex*70u));b.push_back(static_cast<std::uint8_t>(vertex*20u));
        }
        Index(b,2);U32(b,0);U32(b,2);U32(b,0);U32(b,1);
        Index(b,0);U32(b,4);U32(b,0);b.insert(b.end(),24u,0u);Index(b,0);U32(b,4);
    }
    package.Body(reference,std::move(b));
}
struct Fixture {
    std::filesystem::path parent,directory;std::map<std::filesystem::path,std::pair<Bytes,std::filesystem::file_time_type>> sources;
    Fixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {const auto candidate=parent/("deusex-primitive-provider-test-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) {directory=std::filesystem::canonical(candidate);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Primitive fixture escaped temporary parent");
        std::filesystem::create_directory(directory/"System");std::filesystem::create_directory(directory/"Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime();std::error_code error;const auto actual=std::filesystem::weakly_canonical(directory,error);
        if(!error&&!directory.empty()&&actual==directory&&actual.parent_path()==parent&&
            actual.filename().string().rfind("deusex-primitive-provider-test-",0u)==0u)std::filesystem::remove_all(actual,error);
    }
    PortablePackageTables Write(Package& p,bool map=false) {
        const auto path=directory/(map?"Maps":"System")/(p.stem+(map?".dx":".u"));const auto b=p.Serialize();WriteBytes(path,b);
        sources.emplace(path,std::pair{b,std::filesystem::last_write_time(path)});return LoadPortablePackageTables(path.string());
    }
    Bytes Snapshot(const std::string& name) const {const auto path=directory/(name+".sav");Require(SavePortableRuntimeState(path.string()),"Cannot snapshot primitive runtime");return ReadBytes(path);}
    void UnchangedSources() const {
        for(const auto& [path,original]:sources)Require(ReadBytes(path)==original.first&&std::filesystem::last_write_time(path)==original.second,
            "Primitive provider changed generated source bytes/mtime");
    }
};
enum class Variant {Good,BrushFlags,BrushDimension,BrushConstraint,MeshFlags,MeshDimension,MeshConstraint,
    LocationFlags,LocationDimension,RadiusFlags,RadiusDimension,HeightFlags,VectorChannel,
    LoadedNativeClasses,NonNativeMesh,WrongMeshBase};
struct Tables {std::vector<PortablePackageTables> scripts;PortablePackageTables map;};
Tables Build(Fixture& fixture,Variant variant=Variant::Good,bool mutableFixture=false) {
    Package core;core.stem="Core";const auto cp=core.ImportPackage("Core"),object=core.Export("Object"),meta=core.Export("Class",0,object);
    core.Native(object);
    const auto vector=core.Export("Vector",core.Import("Struct",cp),0,object);
    for(const auto* channel:{"X","Y","Z"})core.Property(channel,variant==Variant::VectorChannel&&std::string(channel)=="Z"?"IntProperty":"FloatProperty",vector,cp,0,1u);
    core.ClassBody(object);core.ClassBody(meta);core.StructBody(vector);const auto ct=fixture.Write(core);
    Package engine;engine.stem="Engine";const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec);
    const auto ev=engine.Import("Vector",eo,"Struct"),ep=engine.ImportPackage("Engine"),mesh=engine.Import("Mesh",ep),model=engine.Import("Model",ep);
    const auto actor=engine.Export("Actor",0,eo);engine.Native(actor);
    engine.Property("Brush","ObjectProperty",actor,ec,variant==Variant::BrushConstraint?mesh:model,
        variant==Variant::BrushFlags?0x21u:0x2au,variant==Variant::BrushDimension?2u:1u);
    engine.Property("Mesh","ObjectProperty",actor,ec,variant==Variant::MeshConstraint?model:mesh,
        variant==Variant::MeshFlags?0x20u:0x21u,variant==Variant::MeshDimension?2u:1u);
    engine.Property("Location","StructProperty",actor,ec,ev,variant==Variant::LocationFlags?0x3u:0x23u,variant==Variant::LocationDimension?2u:1u);
    engine.Property("CollisionRadius","FloatProperty",actor,ec,0,variant==Variant::RadiusFlags?0x3u:0x23u,variant==Variant::RadiusDimension?2u:1u);
    engine.Property("CollisionHeight","FloatProperty",actor,ec,0,variant==Variant::HeightFlags?0x3u:0x23u);
    if(variant==Variant::LoadedNativeClasses||variant==Variant::NonNativeMesh||variant==Variant::WrongMeshBase) {
        const auto nativePrimitive=engine.Export("Primitive",0,eo);
        const auto nativeMesh=engine.Export("Mesh",0,variant==Variant::WrongMeshBase?actor:nativePrimitive);
        const auto nativeLod=engine.Export("LodMesh",0,nativeMesh),nativeModel=engine.Export("Model",0,nativePrimitive);
        for(const auto cls:{nativePrimitive,nativeMesh,nativeLod,nativeModel}) {
            engine.ClassBody(cls);if(cls!=nativeMesh||variant!=Variant::NonNativeMesh)engine.Native(cls);
        }
    }
    engine.Property("DrawType","ByteProperty",actor,ec);engine.Property("bHidden","BoolProperty",actor,ec);engine.ClassBody(actor);const auto et=fixture.Write(engine);
    Package foreign;foreign.stem="Foreign";const auto fc=foreign.ImportPackage("Core"),fo=foreign.Import("Object",fc),fm=foreign.Export("Mesh",0,fo);
    foreign.Native(fm);foreign.ClassBody(fm);const auto ft=fixture.Write(foreign);
    Package assets;assets.stem="NativeAssets";const auto ae=assets.ImportPackage("Engine"),af=assets.ImportPackage("Foreign");
    const auto am=assets.Import("Mesh",ae),al=assets.Import("LodMesh",ae),ab=assets.Import("Model",ae);
    const auto emptyMesh=assets.Export("EmptyMesh",am),garbageMesh=assets.Export("GarbageMesh",am);
    const auto emptyLod=assets.Export("EmptyLod",al),garbageLod=assets.Export("GarbageLod",al);
    if(mutableFixture) {
        MeshBody(assets,emptyMesh,false);MeshBody(assets,garbageMesh,false);MeshBody(assets,emptyLod,true);MeshBody(assets,garbageLod,true);
    } else {assets.Body(garbageMesh,{0xffu,0xacu,0x53u});assets.Body(garbageLod,{0xffu,0xbbu,0x75u});}
    assets.Export("EmptyModel",ab);assets.Body(assets.Export("GarbageModel",ab),{0xffu,0xddu});
    assets.Export("WrongKindMesh",assets.Import("Mesh",ae,"Object"));
    assets.Export("UnknownMesh",assets.Import("UnregisteredMesh",ae));assets.Export("SkeletalMesh",assets.Import("SkeletalMesh",ae));
    assets.Export("ForeignMesh",assets.Import("Mesh",af));const auto at=fixture.Write(assets);
    Package classes;classes.stem="PrimitiveClasses";const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine"),ca=classes.ImportPackage("NativeAssets");
    const auto co=classes.Import("Object",cc),cv=classes.Import("Vector",co,"Struct"),cm=classes.Import("Mesh",ce),cb=classes.Import("Model",ce),actorClass=classes.Import("Actor",ce);
    const auto probe=classes.Export("Probe",0,actorClass),derived=classes.Export("Derived",0,probe),shadow=classes.Export("Shadow",0,probe);
    const auto brushProbe=classes.Export("BrushProbe",0,probe),empty=classes.Export("NoPrimitive",0,actorClass),unknown=classes.Export("UnknownNativeActor",0,probe);classes.Native(unknown);
    const auto qBrush=classes.Import("Brush",actorClass,"ObjectProperty"),qMesh=classes.Import("Mesh",actorClass,"ObjectProperty"),qLocation=classes.Import("Location",actorClass,"StructProperty");
    const auto qRadius=classes.Import("CollisionRadius",actorClass,"FloatProperty"),qHeight=classes.Import("CollisionHeight",actorClass,"FloatProperty");
    const auto setter=[&](const char* name,std::int32_t owner,std::int32_t field,const std::string& type,std::int32_t target,bool defaults=false,bool fail=false) {
        const auto fn=classes.Function(name,owner,cc),input=classes.Property("Value",type,fn,cc,target,0x80u);
        classes.FunctionBody(fn,fail?Join({Assign(Ref(0x01u,field),Ref(0x00u,input)),Unsupported(),Return()}):
            Join({Assign(Ref(defaults?0x02u:0x01u,field),Ref(0x00u,input)),Return()}));
    };
    setter("SetMesh",probe,qMesh,"ObjectProperty",cm);setter("SetBrush",probe,qBrush,"ObjectProperty",cb);
    setter("SetLocation",probe,qLocation,"StructProperty",cv);setter("SetRadius",probe,qRadius,"FloatProperty",0);setter("SetHeight",probe,qHeight,"FloatProperty",0);
    setter("SetDefaultMesh",probe,qMesh,"ObjectProperty",cm,true);setter("SetMeshThenFail",probe,qMesh,"ObjectProperty",cm,false,true);
    const auto sMesh=classes.Property("Mesh","ObjectProperty",shadow,cc,cm),sBrush=classes.Property("Brush","ObjectProperty",shadow,cc,cb);
    const auto sLocation=classes.Property("Location","StructProperty",shadow,cc,cv),sRadius=classes.Property("CollisionRadius","FloatProperty",shadow,cc),sHeight=classes.Property("CollisionHeight","FloatProperty",shadow,cc);
    setter("SetShadowMesh",shadow,sMesh,"ObjectProperty",cm);setter("SetShadowBrush",shadow,sBrush,"ObjectProperty",cb);
    setter("SetShadowLocation",shadow,sLocation,"StructProperty",cv);setter("SetShadowRadius",shadow,sRadius,"FloatProperty",0);setter("SetShadowHeight",shadow,sHeight,"FloatProperty",0);
    Bytes defaults;classes.ObjectTag(defaults,"Mesh",classes.Import("GarbageLod",ca,"LodMesh"));classes.VectorTag(defaults,"Location",{10,20,30});
    classes.FloatTag(defaults,"CollisionRadius",2);classes.FloatTag(defaults,"CollisionHeight",3);classes.ClassBody(probe,defaults);classes.ClassBody(derived);
    Bytes sd;classes.ObjectTag(sd,"Mesh",classes.Import("EmptyMesh",ca,"Mesh"));classes.ObjectTag(sd,"Brush",classes.Import("EmptyModel",ca,"Model"));
    classes.VectorTag(sd,"Location",{100,200,300});classes.FloatTag(sd,"CollisionRadius",100);classes.FloatTag(sd,"CollisionHeight",200);classes.ClassBody(shadow,sd);
    Bytes bd;classes.ObjectTag(bd,"Brush",classes.Import("EmptyModel",ca,"Model"));classes.ClassBody(brushProbe,bd);classes.ClassBody(empty);classes.ClassBody(unknown);const auto st=fixture.Write(classes);
    Package map;map.stem="PrimitiveFixture";const auto mc=map.ImportPackage("PrimitiveClasses"),ma=map.ImportPackage("NativeAssets");
    for(const auto& [name,cls]:std::vector<std::pair<std::string,std::string>>{{"Driver","Probe"},{"Plain","Probe"},{"Derived","Derived"},
        {"Shadow","Shadow"},{"Brush","BrushProbe"},{"None","NoPrimitive"},{"UnknownNative","UnknownNativeActor"},
        {"EmptyMesh","Probe"},{"EmptyLod","Probe"},{"GarbageMesh","Probe"},{"WrongKind","Probe"},{"UnknownMesh","Probe"},
        {"SkeletalMesh","Probe"},{"ForeignMesh","Probe"},{"WrongImportType","Probe"},{"MeshBaseImport","Probe"},{"IgnoredClassPackage","Probe"}}) {
        if(mutableFixture&&(name=="WrongKind"||name=="UnknownMesh"||name=="SkeletalMesh"||name=="ForeignMesh"||name=="WrongImportType"))continue;
        const auto ref=map.Export(name,map.Import(cls,mc));Bytes tags;
        if(name=="EmptyMesh"||name=="GarbageMesh")map.ObjectTag(tags,"Mesh",map.Import(name,ma,"Mesh"));
        else if(name=="EmptyLod")map.ObjectTag(tags,"Mesh",map.Import("EmptyLod",ma,"LodMesh"));
        else if(name=="WrongKind")map.ObjectTag(tags,"Mesh",map.Import("WrongKindMesh",ma,"Mesh"));
        else if(name=="UnknownMesh"||name=="SkeletalMesh"||name=="ForeignMesh")map.ObjectTag(tags,"Mesh",map.Import(name,ma,"Mesh"));
        else if(name=="WrongImportType")map.ObjectTag(tags,"Mesh",map.Import("GarbageLod",ma,"Texture"));
        else if(name=="MeshBaseImport")map.ObjectTag(tags,"Mesh",map.Import("GarbageLod",ma,"Mesh"));
        else if(name=="IgnoredClassPackage")map.ObjectTag(tags,"Mesh",map.Import("GarbageLod",ma,"Mesh","IgnoredForeignClassPackage"));
        if(name=="EmptyMesh") { // Presentation inactivity never selects a different primitive.
            map.Tag(tags,"DrawType",1u,{0u});Index(tags,map.Name("bHidden"));tags.push_back(0x83u);
        }
        map.ActorBody(ref,tags);
    }
    return {{ct,et,ft,at,st},fixture.Write(map,true)};
}
std::string Path(const std::string& leaf) {return "PrimitiveFixture."+leaf;}
Value Object(const std::string& path) {return Value::Text(Kind::Object,path);}
void Call(const std::string& actor,const char* function,Value value) {
    const auto result=ExecutePortableActorFunction(Path(actor),function,{{std::move(value),{}}});
    Require(result.passed()&&result.committed,actor+'.'+function+" failed: "+result.error);
}
template<class F> void Refused(F&& operation,const std::string& why) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,why);++refusals;
}
bool BitsEqual(Vec a,Vec b) {for(std::size_t i=0u;i<3u;++i)if(std::bit_cast<std::uint32_t>(a[i])!=std::bit_cast<std::uint32_t>(b[i]))return false;return true;}
void Near(Vec a,Vec b,const std::string& why) {
    for(std::size_t i=0u;i<3u;++i)Require(std::isfinite(a[i])&&std::abs(a[i]-b[i])<=0.00003f,why+" component "+std::to_string(i));
}
void Primitive(const PortableActorPrimitiveSnapshot& p,const std::string& actor,const std::string& asset,const std::string& cls) {
    Require(p.actorPath==Path(actor)&&p.primitivePath=="NativeAssets."+asset&&p.nativeClassPath=="Engine."+cls,
        "Selected primitive identity/source class differs from actual typed native field");
    Require(p.lineImplementation==(cls=="Model"?"UModel::LineCheck":"UMesh::LineCheck")&&
        p.boundsImplementation==(cls=="Model"?"UModel::GetCollisionBoundingBox":"UPrimitive::GetCollisionBoundingBox"),
        "Selected primitive native virtual implementation differs from audited class");
}
template<class F> void Unchanged(Fixture& fixture,F&& operation,const std::string& why) {
    const auto before=fixture.Snapshot("BeforeRead");const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    const auto actors=GetPortableRuntimeMapActors(true);operation();const auto after=GC::GetStats();
    Require(fixture.Snapshot("AfterRead")==before&&GetPortableRuntimeWorldRevision()==revision&&
        gc.numObjects==after.numObjects&&gc.memoryUsage==after.memoryUsage&&GetPortableRuntimeMapActors(true).size()==actors.size(),why);
}
void MeshReads(const std::string& actor,const std::string& asset,const std::string& cls,Vec location={10,20,30},float radius=2,float height=3) {
    Primitive(ReadPortableRuntimeActorPrimitive(Path(actor)),actor,asset,cls);
    const auto bounds=ReadPortableRuntimeActorPrimitiveBounds(Path(actor));Primitive(bounds.primitive,actor,asset,cls);
    Near(bounds.minimum,{location[0]-radius-1,location[1]-radius-1,location[2]-height-1},"Literal owner cylinder padded bounds minimum");
    Near(bounds.maximum,{location[0]+radius+1,location[1]+radius+1,location[2]+height+1},"Literal owner cylinder padded bounds maximum");
    const auto hit=ProbePortableRuntimeActorPrimitiveLine(Path(actor),{location[0]+5,location[1],location[2]},
        {location[0]-5,location[1],location[2]});Primitive(hit.primitive,actor,asset,cls);
    Require(!hit.clear&&hit.hitOwner&&hit.hitPrimitiveIsNone&&hit.item==-1&&std::abs(hit.time-0.299f)<0.000003f,
        "Fixed-seed central Mesh cylinder packet/time differs");
    Near(hit.location,{location[0]-2.01f,location[1],location[2]},"Literal central Mesh cylinder hit location");
    Require(BitsEqual(hit.normal,{-1,0,0}),"Literal central Mesh cylinder hit normal differs");
    const auto clear=ProbePortableRuntimeActorPrimitiveLine(Path(actor),{location[0]+5,location[1],location[2]},
        {location[0]+4,location[1],location[2]}, {},0xffffffffu);
    Require(clear.clear&&!clear.hitOwner&&clear.hitPrimitiveIsNone&&clear.time==1.0f&&clear.item==-1&&
        BitsEqual(clear.location,{0,0,0})&&BitsEqual(clear.normal,{0,0,0}),"Fixed-seed early-clear packet was changed");
}
void ReadOnlyOpaque() {
    Fixture fixture;const auto t=Build(fixture);Require(InitializePortableRuntime(t.scripts).passed&&LoadPortableRuntimeMap(t.map).passed,
        "Cannot initialize generated live primitive fixture");
    Unchanged(fixture,[&] {
        for(const char* actor:{"Driver","Plain","Derived","Shadow","UnknownNative","MeshBaseImport","IgnoredClassPackage"})
            MeshReads(actor,"GarbageLod","LodMesh");
        MeshReads("EmptyMesh","EmptyMesh","Mesh");MeshReads("EmptyLod","EmptyLod","LodMesh");MeshReads("GarbageMesh","GarbageMesh","Mesh");
        Primitive(ReadPortableRuntimeActorPrimitive("primitivefixture.driver"),"Driver","GarbageLod","LodMesh");
        Primitive(ReadPortableRuntimeActorPrimitive(Path("Brush")),"Brush","EmptyModel","Model");
        Refused([&] {(void)ReadPortableRuntimeActorPrimitiveBounds(Path("Brush"));},"Model query silently substituted actor cylinder bounds");
        Refused([&] {(void)ProbePortableRuntimeActorPrimitiveLine(Path("Brush"),{},{});},"Model query silently substituted Mesh/cylinder LineCheck");
        Refused([&] {(void)ReadPortableRuntimeActorPrimitive(Path("None"));},"Null Brush/Mesh fabricated Engine.Cylinder authority");
        for(const char* actor:{"WrongKind","UnknownMesh","SkeletalMesh","ForeignMesh","WrongImportType"})
            Refused([&] {(void)ReadPortableRuntimeActorPrimitive(Path(actor));},"Wrong-kind/unknown/native-foreign/source import primitive was accepted");
        for(const auto& path:std::vector<std::string>{"", "None", "PrimitiveClasses.Probe", "@ClassDefault:PrimitiveClasses.Probe",
            "PrimitiveFixture.Missing", "NativeAssets.GarbageLod",std::string(4097u,'p'),std::string("PrimitiveFixture.Driver\0extra",29u)})
            Refused([&] {(void)ReadPortableRuntimeActorPrimitive(path);},"Non-Actor/index/invalid identity was accepted");
        const auto partial=ProbePortableRuntimeActorPrimitiveLine(Path("Driver"),{6,20,32.9f},{14,20,35});
        Require(partial.clear&&!partial.hitOwner&&partial.hitPrimitiveIsNone&&partial.time==1&&partial.item==-1&&
            BitsEqual(partial.location,{0,0,0})&&BitsEqual(partial.normal,{0,0,1}),"Live Mesh clear lost partial Normal/seed packet behavior");
        const float nan=std::bit_cast<float>(0x7f800001u);
        const auto discarded=ProbePortableRuntimeActorPrimitiveLine(Path("Driver"),{15,20,30},{5,20,30},{nan,-0.0f,nan});
        Require(!discarded.clear&&discarded.hitOwner&&discarded.hitPrimitiveIsNone,"Live Mesh wrapper rejected discarded zero-or-NaN extent");
    },"Read-only primitive selection/bounds/line/refusals changed save/revision/GC/world graph");
    fixture.UnchangedSources();
}
void Mutable() {
    Fixture fixture;const auto t=Build(fixture,Variant::Good,true);
    Require(InitializePortableRuntime(t.scripts).passed&&LoadPortableRuntimeMap(t.map).passed,"Cannot initialize mutable primitive fixture with valid world Mesh assets");
    Require(QuestVr::Vm::Equal(ReadPortableActorScriptProperty(Path("Shadow"),"Mesh"),Object("NativeAssets.EmptyMesh")),"Mesh shadow fixture not distinct");
    Require(QuestVr::Vm::Equal(ReadPortableActorDeclaredScriptProperty(Path("Shadow"),"Engine.Actor.Mesh"),Object("NativeAssets.GarbageLod")),"Native declared Mesh did not retain inherited value");
    Call("Shadow","SetShadowMesh",Object("NativeAssets.GarbageMesh"));Call("Shadow","SetShadowBrush",Object("NativeAssets.GarbageModel"));
    Call("Shadow","SetShadowLocation",Value::Vector({777,888,999}));Call("Shadow","SetShadowRadius",Value::Float(700));Call("Shadow","SetShadowHeight",Value::Float(900));
    Unchanged(fixture,[&] {MeshReads("Shadow","GarbageLod","LodMesh");},"Shadow-field reads changed native fields or state");
    Call("Driver","SetDefaultMesh",Object("NativeAssets.EmptyMesh"));
    Require(QuestVr::Vm::Equal(ReadPortableClassDefault("PrimitiveClasses.Probe","Mesh"),Object("NativeAssets.EmptyMesh")),"Native Mesh default setter failed");
    Unchanged(fixture,[&] {MeshReads("Plain","GarbageLod","LodMesh");MeshReads("Derived","GarbageLod","LodMesh");},"Default mutation changed frozen existing native Mesh values");
    Call("Driver","SetMesh",Object("NativeAssets.EmptyMesh"));
    Unchanged(fixture,[&] {MeshReads("Driver","EmptyMesh","Mesh");},"Live qualified Mesh overlay not used read-only");
    Call("Driver","SetBrush",Object("NativeAssets.GarbageModel"));
    Unchanged(fixture,[&] {
        Primitive(ReadPortableRuntimeActorPrimitive(Path("Driver")),"Driver","GarbageModel","Model");
        Refused([&] {(void)ReadPortableRuntimeActorPrimitiveBounds(Path("Driver"));},"Brush precedence fell back to Mesh on unsupported Model query");
    },"Brush precedence/refusal changed state");
    Call("Driver","SetBrush",Object(""));Call("Driver","SetMesh",Object(""));
    Unchanged(fixture,[&] {Refused([&] {(void)ReadPortableRuntimeActorPrimitive(Path("Driver"));},"Cleared native fields invented engine fallback");},"Cleared-field refusal changed state");
    Call("Driver","SetMesh",Object("NativeAssets.EmptyLod"));
    const auto rollbackBefore=fixture.Snapshot("BeforeRollback");const auto rollbackRevision=GetPortableRuntimeWorldRevision();const auto rollbackGc=GC::GetStats();
    const auto failed=ExecutePortableActorFunction(Path("Driver"),"SetMeshThenFail",{{Object("NativeAssets.EmptyMesh"),{}}});
    Require(!failed.passed()&&!failed.committed,"Writer followed by unsupported native unexpectedly committed");++refusals;
    Require(fixture.Snapshot("AfterRollback")==rollbackBefore&&GetPortableRuntimeWorldRevision()==rollbackRevision&&
        GC::GetStats().numObjects==rollbackGc.numObjects&&GC::GetStats().memoryUsage==rollbackGc.memoryUsage,"Nested writer refusal failed complete rollback");
    Primitive(ReadPortableRuntimeActorPrimitive(Path("Driver")),"Driver","EmptyLod","LodMesh");
    Call("Driver","SetLocation",Value::Vector({2,4,8}));Call("Driver","SetRadius",Value::Float(2));Call("Driver","SetHeight",Value::Float(3));
    Unchanged(fixture,[&] {MeshReads("Driver","EmptyLod","LodMesh",{2,4,8});},"Live native Location/dimension overlays were not consumed read-only");
    fixture.UnchangedSources();
}
void NativeClassWrappers() {
    for(const auto variant:{Variant::LoadedNativeClasses,Variant::NonNativeMesh,Variant::WrongMeshBase}) {
        Fixture fixture;const auto t=Build(fixture,variant);
        Require(InitializePortableRuntime(t.scripts).passed&&LoadPortableRuntimeMap(t.map).passed,
            "Cannot initialize generated loaded-native-class primitive fixture");
        Unchanged(fixture,[&] {
            if(variant==Variant::LoadedNativeClasses) {
                MeshReads("Driver","GarbageLod","LodMesh");MeshReads("EmptyMesh","EmptyMesh","Mesh");
            } else {
                Refused([&] {(void)ReadPortableRuntimeActorPrimitive(Path("Driver"));},"Selected LodMesh admitted non-native/wrong-direct-base Mesh UClass");
                Refused([&] {(void)ReadPortableRuntimeActorPrimitiveBounds(Path("Driver"));},"Invalid loaded Mesh superclass admitted bounds query");
                Refused([&] {(void)ProbePortableRuntimeActorPrimitiveLine(Path("Driver"),{15,20,30},{5,20,30});},"Invalid loaded Mesh superclass admitted line query");
                Refused([&] {(void)ReadPortableRuntimeActorPrimitive(Path("EmptyMesh"));},"Direct Mesh asset bypassed invalid loaded native UClass authority");
            }
            Primitive(ReadPortableRuntimeActorPrimitive(Path("Brush")),"Brush","EmptyModel","Model");
        },"Loaded native class admission/refusal changed complete runtime state");fixture.UnchangedSources();
    }
}
void NegativeSchemas() {
    for(const auto variant:{Variant::BrushFlags,Variant::BrushDimension,Variant::BrushConstraint,Variant::MeshFlags,Variant::MeshDimension,
        Variant::MeshConstraint,Variant::LocationFlags,Variant::LocationDimension,Variant::RadiusFlags,Variant::RadiusDimension,Variant::HeightFlags,Variant::VectorChannel}) {
        Fixture fixture;const auto t=Build(fixture,variant);Require(InitializePortableRuntime(t.scripts).passed&&LoadPortableRuntimeMap(t.map).passed,
            "Malformed-field test failed before live provider could inspect the actual schema");
        const bool brush=variant==Variant::BrushFlags||variant==Variant::BrushDimension||variant==Variant::BrushConstraint;
        const bool mesh=variant==Variant::MeshFlags||variant==Variant::MeshDimension||variant==Variant::MeshConstraint;
        Unchanged(fixture,[&] {
            if(brush||mesh)Refused([&] {(void)ReadPortableRuntimeActorPrimitive(Path("Plain"));},"Malformed original Brush/Mesh schema was guessed or skipped");
            if(mesh)Primitive(ReadPortableRuntimeActorPrimitive(Path("Brush")),"Brush","EmptyModel","Model"); // Later Mesh schema inactive.
            if(!brush&&!mesh) {
                Primitive(ReadPortableRuntimeActorPrimitive(Path("Plain")),"Plain","GarbageLod","LodMesh"); // Owner shape inactive for identity.
                Refused([&] {(void)ReadPortableRuntimeActorPrimitiveBounds(Path("Plain"));},"Malformed native owner schema accepted bounds query");
                Refused([&] {(void)ProbePortableRuntimeActorPrimitiveLine(Path("Plain"),{15,20,30},{5,20,30});},"Malformed native owner schema accepted line query");
            }
        },"Malformed primitive/native owner schema refusal changed complete runtime state");fixture.UnchangedSources();
    }
}
} // namespace
int main() {
    try {
        ShutdownPortableRuntime();GC::Collect();const auto baseline=GC::GetStats();ReadOnlyOpaque();Mutable();NativeClassWrappers();NegativeSchemas();ShutdownPortableRuntime();GC::Collect();
        Require(GC::GetStats().numObjects==baseline.numObjects&&GC::GetStats().memoryUsage==baseline.memoryUsage,"Generated primitive test leaked rooted objects");
        std::cout<<"PASS generated live Actor primitive provider: "<<checks<<" checks, "<<refusals
            <<" refusals; exact qualified fields, typed source/native asset authority, Mesh ancestor imports, opaque geometry, "
              "literal owner bounds/line packets, live/shadow/default overlays, rollback and complete read-only state invariants. "
              "Not Model query, Engine.Cylinder lifetime, Move/gravity or headset evidence.\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL generated live primitive provider: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
