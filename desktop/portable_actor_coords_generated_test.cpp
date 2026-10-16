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

// Commercial-data-free v68 packages exercise the LIVE native ToWorld reader.
// These tests deliberately do not call the detached coordinate builder for
// expected results: exact simple transforms and independently retained query
// packets distinguish native declaring fields from same-name script shadows.
namespace {
using Bytes=std::vector<std::uint8_t>;
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
using Evaluation=QuestVr::Vm::Evaluation;
using Vector=std::array<float,3u>;
using Rotation=std::array<std::int32_t,3u>;
std::size_t checks{},refusals{};
void Require(bool condition,const std::string& message) {++checks;if(!condition) throw std::runtime_error(message);}
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
    const auto count=std::filesystem::file_size(path);Require(count<=16u*1024u*1024u,"Coordinate fixture/save exceeds bound");
    Bytes bytes(static_cast<std::size_t>(count));std::ifstream file(path,std::ios::binary);Require(static_cast<bool>(file),"Cannot read coordinate fixture/save");
    if(!bytes.empty())file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file),"Coordinate fixture/save is truncated");return bytes;
}
void WriteBytes(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);Require(static_cast<bool>(file),"Cannot create generated coordinate fixture");
    file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));Require(static_cast<bool>(file),"Cannot write coordinate fixture");
}
struct Code {Bytes raw;std::size_t logical{};};
Code Token(std::uint8_t op) {return {{op},1u};}
Code Ref(std::uint8_t op,std::int32_t reference) {Code c{{op},5u};Index(c.raw,reference);return c;}
Code Join(std::initializer_list<Code> parts) {Code c;for(const auto& p:parts){Append(c.raw,p.raw);c.logical+=p.logical;}return c;}
Code Return() {return Join({Token(0x04u),Token(0x0bu)});}
Code Assign(Code left,Code right) {return Join({Token(0x0fu),std::move(left),std::move(right)});}
struct Package {
    std::string stem;
    std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
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
    void Native(std::int32_t ref) {exports.at(static_cast<std::size_t>(ref-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);}
    void Body(std::int32_t ref,Bytes body) {builders.at(static_cast<std::size_t>(ref-1))=[body=std::move(body)](std::int32_t,std::int32_t){return body;};}
    void Tag(Bytes& b,const std::string& name,std::uint8_t type,const Bytes& data,const std::string& structure={}) {
        Require(data.size()<=255u,"Coordinate property tag exceeds bound");Index(b,Name(name));b.push_back(static_cast<std::uint8_t>(type|0x50u));
        if(type==10u)Index(b,Name(structure));
        b.push_back(static_cast<std::uint8_t>(data.size()));Append(b,data);
    }
    void VectorTag(Bytes& b,const std::string& name,Vector v) {Bytes data;for(const auto n:v)Float(data,n);Tag(b,name,10u,data,"Vector");}
    void RotationTag(Bytes& b,const std::string& name,Rotation v) {Bytes data;for(const auto n:v)U32(data,static_cast<std::uint32_t>(n));Tag(b,name,10u,data,"Rotator");}
    void ScaleTag(Bytes& b,const std::string& name,Vector scale,float rate=0.0f,std::uint8_t axis=0u) {
        Bytes data;for(const auto n:scale)Float(data,n);Float(data,rate);data.push_back(axis);Tag(b,name,10u,data,"Scale");
    }
    void ClassBody(std::int32_t ref,Bytes defaults={}) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,defaults=std::move(defaults)](std::int32_t next,std::int32_t first) {
            const auto& e=exports.at(static_cast<std::size_t>(ref-1));Bytes b;Index(b,e.ObjBase);Index(b,next);Index(b,0);Index(b,first);Index(b,e.ObjName);
            U32(b,0);U32(b,0);U32(b,0);U64(b,~std::uint64_t{});U64(b,~std::uint64_t{});U16(b,0xffffu);U32(b,0);U32(b,0);
            b.resize(b.size()+16u,0u);for(unsigned i=0;i<4u;++i)Index(b,0);Append(b,defaults);Index(b,0);return b;
        };
    }
    void StructBody(std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);U32(b,0);U32(b,0);U32(b,0);return b;
        };
    }
    void EnumBody(std::int32_t ref) {
        std::vector<std::int32_t> values;for(const auto* name:{"SHEER_None","SHEER_XY","SHEER_XZ","SHEER_YX","SHEER_YZ","SHEER_ZX","SHEER_ZY"})values.push_back(Name(name));
        builders.at(static_cast<std::size_t>(ref-1))=[values=std::move(values)](std::int32_t next,std::int32_t) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,static_cast<std::int32_t>(values.size()));for(const auto n:values)Index(b,n);return b;
        };
    }
    std::int32_t Property(const std::string& name,const std::string& type,std::int32_t owner,std::int32_t core,std::int32_t target=0,std::uint32_t flags=0u,std::uint32_t dim=1u) {
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
        for(const auto& [owner,list]:children){static_cast<void>(owner);for(std::size_t i=1;i<list.size();++i)next[list[i-1u]]=list[i];}
        Bytes b;U32(b,0x9e2a83c1u);U16(b,68u);U16(b,0);b.resize(56u,0u);std::vector<Bytes> bodies;std::vector<std::int32_t> offsets;
        for(std::size_t i=0;i<exports.size();++i){const auto ref=static_cast<std::int32_t>(i+1u);const auto it=children.find(ref);
            auto body=builders[i]?builders[i](next[ref],it==children.end()||it->second.empty()?0:it->second.front()):Bytes{};
            offsets.push_back(static_cast<std::int32_t>(b.size()));Append(b,body);bodies.push_back(std::move(body));}
        Replace32(b,12u,static_cast<std::uint32_t>(names.size()));Replace32(b,16u,static_cast<std::uint32_t>(b.size()));
        for(const auto& name:names){Index(b,static_cast<std::int32_t>(name.size()+1u));b.insert(b.end(),name.begin(),name.end());b.push_back(0);U32(b,0);}
        Replace32(b,20u,static_cast<std::uint32_t>(exports.size()));Replace32(b,24u,static_cast<std::uint32_t>(b.size()));
        for(std::size_t i=0;i<exports.size();++i){const auto& e=exports[i];Index(b,e.ObjClass);Index(b,e.ObjBase);U32(b,static_cast<std::uint32_t>(e.ObjOuter));Index(b,e.ObjName);U32(b,static_cast<std::uint32_t>(e.ObjFlags));Index(b,static_cast<std::int32_t>(bodies[i].size()));if(!bodies[i].empty())Index(b,offsets[i]);}
        Replace32(b,28u,static_cast<std::uint32_t>(imports.size()));Replace32(b,32u,static_cast<std::uint32_t>(b.size()));
        for(const auto& e:imports){Index(b,e.ClassPackage);Index(b,e.ClassName);U32(b,static_cast<std::uint32_t>(e.ObjOuter));Index(b,e.ObjName);}return b;
    }
};
struct Fixture {
    std::filesystem::path parent,directory;std::map<std::filesystem::path,Bytes> sources;
    Fixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());const auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<20u;++i){const auto candidate=parent/("deusex-actor-coords-test-"+std::to_string(stamp)+'-'+std::to_string(i));if(std::filesystem::create_directory(candidate)){directory=std::filesystem::canonical(candidate);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Coordinate fixture escaped temporary parent");std::filesystem::create_directory(directory/"System");std::filesystem::create_directory(directory/"Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime();std::error_code error;const auto actual=std::filesystem::weakly_canonical(directory,error);
        if(!error&&!directory.empty()&&actual==directory&&actual.parent_path()==parent&&actual.filename().string().rfind("deusex-actor-coords-test-",0u)==0u)std::filesystem::remove_all(actual,error);
    }
    PortablePackageTables Write(Package& p,bool map=false) {const auto path=directory/(map?"Maps":"System")/(p.stem+(map?".dx":".u"));const auto bytes=p.Serialize();WriteBytes(path,bytes);sources.emplace(path,bytes);return LoadPortablePackageTables(path.string());}
    Bytes Snapshot(const std::string& label) const {const auto path=directory/(label+".sav");Require(SavePortableRuntimeState(path.string()),"Cannot snapshot coordinate runtime");return ReadBytes(path);}
    void UnchangedSources() const {for(const auto& [path,bytes]:sources)Require(ReadBytes(path)==bytes,"Coordinate reader modified source package");}
};
enum class Variant {Good,LocationFlags,LocationDimension,RotationFlags,RotationOrder,VectorChannel,ScaleFlags,ScaleEnumOwner,NoNativeActor};
struct Tables {std::vector<PortablePackageTables> scripts;PortablePackageTables map;};
Tables Build(Fixture& fixture,Variant variant=Variant::Good) {
    Package core;core.stem="Core";const auto cp=core.ImportPackage("Core"),object=core.Export("Object"),meta=core.Export("Class",0,object);
    const auto vec=core.Export("Vector",core.Import("Struct",cp),0,object),rot=core.Export("Rotator",core.Import("Struct",cp),0,object),scale=core.Export("Scale",core.Import("Struct",cp),0,object);
    for(const auto* name:{"X","Y","Z"})core.Property(name,variant==Variant::VectorChannel&&std::string(name)=="Z"?"IntProperty":"FloatProperty",vec,cp,0,0x1u);
    for(const auto* name:variant==Variant::RotationOrder?std::array<const char*,3u>{"Yaw","Pitch","Roll"}:std::array<const char*,3u>{"Pitch","Yaw","Roll"})core.Property(name,"IntProperty",rot,cp,0,0x1u);
    const auto enumeration=core.Export("ESheerAxis",core.Import("Enum",cp),0,variant==Variant::ScaleEnumOwner?object:scale,true);core.EnumBody(enumeration);
    core.Property("Scale","StructProperty",scale,cp,vec,variant==Variant::ScaleFlags?0x1u:0x4001u);
    core.Property("SheerRate","FloatProperty",scale,cp,0,0x4001u);core.Property("SheerAxis","ByteProperty",scale,cp,enumeration,0x4001u);
    core.ClassBody(object);core.ClassBody(meta);core.StructBody(vec);core.StructBody(rot);core.StructBody(scale);const auto ct=fixture.Write(core);
    Package engine;engine.stem="Engine";const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec),ev=engine.Import("Vector",eo,"Struct"),er=engine.Import("Rotator",eo,"Struct"),es=engine.Import("Scale",eo,"Struct");
    const auto actor=engine.Export("Actor",0,eo),brush=engine.Export("Brush",0,actor),mover=engine.Export("Mover",0,brush),decoration=engine.Export("Decoration",0,actor);
    for(const auto cls:{actor,brush,mover,decoration})engine.Native(cls);
    if(variant==Variant::NoNativeActor)engine.exports.at(static_cast<std::size_t>(actor-1)).ObjFlags=ObjectFlags{};
    engine.Property("Location","StructProperty",actor,ec,ev,variant==Variant::LocationFlags?0x3u:0x23u,variant==Variant::LocationDimension?2u:1u);
    engine.Property("Rotation","StructProperty",actor,ec,er,variant==Variant::RotationFlags?0x3u:0x23u);
    engine.Property("PrePivot","StructProperty",actor,ec,ev,0x20u);
    engine.Property("MainScale","StructProperty",brush,ec,es,0x1u);engine.Property("PostScale","StructProperty",brush,ec,es,0x1u);
    engine.Property("TempScale","StructProperty",brush,ec,es,0x1u);engine.Property("DrawScale","FloatProperty",actor,ec);
    engine.ClassBody(actor);Bytes bd;engine.ScaleTag(bd,"MainScale",{1,1,1});engine.ScaleTag(bd,"PostScale",{1,1,1});engine.ScaleTag(bd,"TempScale",{0,0,0});engine.ClassBody(brush,bd);engine.ClassBody(mover);engine.ClassBody(decoration);const auto et=fixture.Write(engine);
    Package fake;fake.stem="Foreign";const auto fc=fake.ImportPackage("Core"),fo=fake.Import("Object",fc),fa=fake.Export("Actor",0,fo);fake.Native(fa);fake.ClassBody(fa);const auto ft=fixture.Write(fake);
    Package classes;classes.stem="CoordsClasses";const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine"),co=classes.Import("Object",cc);
    const auto ca=classes.Import("Actor",ce),cb=classes.Import("Brush",ce),cm=classes.Import("Mover",ce),cd=classes.Import("Decoration",ce);
    const auto cv=classes.Import("Vector",co,"Struct"),cr=classes.Import("Rotator",co,"Struct"),cs=classes.Import("Scale",co,"Struct");
    const auto probe=classes.Export("Probe",0,ca),derived=classes.Export("Derived",0,probe),shadow=classes.Export("Shadow",0,probe);
    const auto bp=classes.Export("BrushProbe",0,cb),mp=classes.Export("MoverProbe",0,cm),dp=classes.Export("DecorationProbe",0,cd),unknown=classes.Export("UnknownNative",0,probe);classes.Native(unknown);
    const auto ql=classes.Import("Location",ca,"StructProperty"),qr=classes.Import("Rotation",ca,"StructProperty"),qp=classes.Import("PrePivot",ca,"StructProperty");
    const auto qm=classes.Import("MainScale",cb,"StructProperty"),qs=classes.Import("PostScale",cb,"StructProperty");
    const auto setter=[&](const char* name,std::int32_t owner,std::int32_t field,std::int32_t type,bool defaults=false) {
        const auto fn=classes.Function(name,owner,cc),input=classes.Property("Value","StructProperty",fn,cc,type,0x80u);
        classes.FunctionBody(fn,Join({Assign(Ref(defaults?0x02u:0x01u,field),Ref(0x00u,input)),Return()}));
    };
    setter("SetLocation",probe,ql,cv);setter("SetRotation",probe,qr,cr);setter("SetDefaultLocation",probe,ql,cv,true);
    setter("SetLocation",bp,ql,cv);setter("SetRotation",bp,qr,cr);setter("SetPrePivot",bp,qp,cv);setter("SetMainScale",bp,qm,cs);setter("SetPostScale",bp,qs,cs);
    const auto sl=classes.Property("Location","StructProperty",shadow,cc,cv),sr=classes.Property("Rotation","StructProperty",shadow,cc,cr);
    setter("SetShadowLocation",shadow,sl,cv);setter("SetShadowRotation",shadow,sr,cr);
    Bytes defaults;classes.VectorTag(defaults,"Location",{12,-24,36});classes.RotationTag(defaults,"Rotation",{0,0,0});classes.ClassBody(probe,defaults);classes.ClassBody(derived);
    Bytes sd;classes.VectorTag(sd,"Location",{999,777,333});classes.RotationTag(sd,"Rotation",{8192,16384,32768});classes.ClassBody(shadow,sd);
    Bytes bpd;classes.VectorTag(bpd,"Location",{8,12,18});classes.VectorTag(bpd,"PrePivot",{2,3,4});classes.ScaleTag(bpd,"MainScale",{2,4,8});classes.ClassBody(bp,bpd);
    classes.ClassBody(mp);classes.ClassBody(dp);classes.ClassBody(unknown);const auto st=fixture.Write(classes);
    Package map;map.stem="CoordsFixture";const auto mc=map.ImportPackage("CoordsClasses"),mf=map.ImportPackage("Foreign");
    for(const auto& [name,cls]:std::vector<std::pair<std::string,std::string>>{{"Driver","Probe"},{"Plain","Probe"},{"Derived","Derived"},{"Shadow","Shadow"},{"Brush0","BrushProbe"},{"Mover0","MoverProbe"},{"Decoration0","DecorationProbe"},{"Unknown","UnknownNative"}}) {
        const auto ref=map.Export(name,map.Import(cls,mc));Bytes tags;if(name=="Driver")map.VectorTag(tags,"Location",{3,5,7});map.ActorBody(ref,tags);
    }
    const auto foreign=map.Export("ForeignActor",map.Import("Actor",mf));map.ActorBody(foreign);
    return {{ct,et,ft,st},fixture.Write(map,true)};
}
std::string Path(const std::string& name) {return "CoordsFixture."+name;}
void Call(const std::string& actor,const char* function,Value value) {
    const auto result=ExecutePortableActorFunction(Path(actor),function,{{std::move(value),{}}});
    Require(result.passed()&&result.committed,actor+'.'+function+" failed: "+result.error);
}
Value Scale(Vector scale,float rate=0.0f,std::uint8_t axis=0u) {Value value;value.kind=Kind::Struct;value.fields={{"scale",Value::Vector(scale)},{"sheerrate",Value::Float(rate)},{"sheeraxis",Value::Byte(axis)}};return value;}
void Near(Vector actual,Vector expected,const std::string& reason) {
    for(std::size_t i=0;i<3u;++i)Require(std::isfinite(actual[i])&&std::abs(actual[i]-expected[i])<=0.00002f,reason+" component "+std::to_string(i));
}
void Shape(const PortableActorToWorldSnapshot& s,const char* actor,const char* native,const char* implementation,Vector origin,Vector x={1,0,0},Vector y={0,1,0},Vector z={0,0,1}) {
    Require(s.actorPath==Path(actor)&&s.nativeClassPath==native&&s.nativeImplementation==implementation,"Native ToWorld provenance differs from actual receiver/nearest native class");
    Near(s.origin,origin,"Origin");Near(s.xAxis,x,"XAxis");Near(s.yAxis,y,"YAxis");Near(s.zAxis,z,"ZAxis");
}
bool BitsEqual(Vector a,Vector b) {for(std::size_t i=0;i<3u;++i)if(std::bit_cast<std::uint32_t>(a[i])!=std::bit_cast<std::uint32_t>(b[i]))return false;return true;}
void SameCoords(const PortableActorToWorldSnapshot& a,const PortableActorToWorldSnapshot& b,const std::string& reason) {Require(BitsEqual(a.origin,b.origin)&&BitsEqual(a.xAxis,b.xAxis)&&BitsEqual(a.yAxis,b.yAxis)&&BitsEqual(a.zAxis,b.zAxis),reason);}
template<class Action> void Unchanged(Fixture& fixture,Action action,const char* reason) {
    const auto before=fixture.Snapshot("Before");const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();action();const auto after=GC::GetStats();
    Require(fixture.Snapshot("After")==before&&GetPortableRuntimeWorldRevision()==revision&&gc.numObjects==after.numObjects&&gc.memoryUsage==after.memoryUsage,reason);
}
void Refused(Fixture& fixture,const std::string& path) {
    Unchanged(fixture,[&]{bool rejected{};try{static_cast<void>(ReadPortableRuntimeActorToWorld(path));}catch(const std::exception&){rejected=true;}Require(rejected,"Unsupported native ToWorld receiver/schema accepted: "+path);++refusals;},"Refused ToWorld changed complete save/revision/GC objects");
}
void Positive() {
    Fixture fixture;const auto t=Build(fixture);Require(InitializePortableRuntime(t.scripts).passed&&LoadPortableRuntimeMap(t.map).passed,"Generated ToWorld runtime/map failed initialization");
    Unchanged(fixture,[&]{
        Shape(ReadPortableRuntimeActorToWorld(Path("Driver")),"Driver","Engine.Actor","AActor::ToWorld",{-3,-5,-7});
        Shape(ReadPortableRuntimeActorToWorld(Path("Plain")),"Plain","Engine.Actor","AActor::ToWorld",{-12,24,-36});
        Shape(ReadPortableRuntimeActorToWorld(Path("Derived")),"Derived","Engine.Actor","AActor::ToWorld",{-12,24,-36});
        Shape(ReadPortableRuntimeActorToWorld(Path("Decoration0")),"Decoration0","Engine.Decoration","AActor::ToWorld",{0,0,0});
        Shape(ReadPortableRuntimeActorToWorld(Path("Brush0")),"Brush0","Engine.Brush","ABrush::ToWorld",{-2,0,1.75f},{2,0,0},{0,4,0},{0,0,8});
        Shape(ReadPortableRuntimeActorToWorld(Path("Mover0")),"Mover0","Engine.Mover","ABrush::ToWorld",{0,0,0});
        SameCoords(ReadPortableRuntimeActorToWorld(Path("Plain")),ReadPortableRuntimeActorToWorld(Path("Shadow")),"Native ToWorld read shadow Location/Rotation rather than original declaring fields");
        for(unsigned i=0;i<32u;++i)static_cast<void>(ReadPortableRuntimeActorToWorld("coordsfixture.driver"));
    },"Successful ToWorld reads changed complete save/revision/GC objects");
    const auto shadow=ReadPortableRuntimeActorToWorld(Path("Shadow"));
    Require(QuestVr::Vm::Equal(ReadPortableActorScriptProperty(Path("Shadow"),"Location"),Value::Vector({999,777,333})),"Shadow Location fixture did not create a distinct authored field");
    Require(QuestVr::Vm::Equal(ReadPortableActorDeclaredScriptProperty(Path("Shadow"),"Engine.Actor.Location"),Value::Vector({12,-24,36})),"Qualified original Location not independently retained");
    Call("Shadow","SetShadowLocation",Value::Vector({100,200,300}));Call("Shadow","SetShadowRotation",Value::Rotator({222,333,444}));
    SameCoords(shadow,ReadPortableRuntimeActorToWorld(Path("Shadow")),"Shadow live overlays altered original native coordinates");
    Call("Shadow","SetLocation",Value::Vector({2,4,8}));Call("Shadow","SetRotation",Value::Rotator({0,16384,0}));
    Shape(ReadPortableRuntimeActorToWorld(Path("Shadow")),"Shadow","Engine.Actor","AActor::ToWorld",{-4,2,-8},{0,-1,0},{1,0,0});
    Require(QuestVr::Vm::Equal(ReadPortableActorScriptProperty(Path("Shadow"),"Location"),Value::Vector({100,200,300})),"Qualified setter overwrote closest shadow instead of native declaring field");
    const auto frozenPlain=ReadPortableRuntimeActorToWorld(Path("Plain")),frozenDerived=ReadPortableRuntimeActorToWorld(Path("Derived"));
    Call("Driver","SetDefaultLocation",Value::Vector({88,99,111}));
    Require(QuestVr::Vm::Equal(ReadPortableClassDefault("CoordsClasses.Probe","Location"),Value::Vector({88,99,111})),"Default setter did not update actual concrete CDO");
    SameCoords(frozenPlain,ReadPortableRuntimeActorToWorld(Path("Plain")),"CDO mutation rewrote frozen native instance Location");
    SameCoords(frozenDerived,ReadPortableRuntimeActorToWorld(Path("Derived")),"Parent CDO mutation rewrote frozen derived default/instance Location");
    Call("Driver","SetLocation",Value::Vector({7,-9,11}));Call("Driver","SetRotation",Value::Rotator({0,0,0}));
    Shape(ReadPortableRuntimeActorToWorld(Path("Driver")),"Driver","Engine.Actor","AActor::ToWorld",{-7,9,-11});
    Call("Brush0","SetPostScale",Scale({2,3,4}));
    Shape(ReadPortableRuntimeActorToWorld(Path("Brush0")),"Brush0","Engine.Brush","ABrush::ToWorld",{0,2,3.4375f},{4,0,0},{0,12,0},{0,0,32});
    Call("Brush0","SetPrePivot",Value::Vector({4,6,8}));Call("Brush0","SetMainScale",Scale({1,2,4}));
    Shape(ReadPortableRuntimeActorToWorld(Path("Brush0")),"Brush0","Engine.Brush","ABrush::ToWorld",{0,4,6.875f},{2,0,0},{0,6,0},{0,0,16});
    Call("Brush0","SetMainScale",Scale({1,0,4}));Refused(fixture,Path("Brush0"));Call("Brush0","SetMainScale",Scale({1,2,4}));
    for(const auto& path:std::vector<std::string>{Path("Unknown"),Path("ForeignActor"),"CoordsClasses.Probe","CoordsClasses.Probe@Default",Path("Missing"),"",std::string("CoordsFixture.Driver\0suffix",27u),std::string(4097u,'a')})Refused(fixture,path);
    fixture.UnchangedSources();
}
void NegativeSchemas() {
    for(const auto variant:{Variant::LocationFlags,Variant::LocationDimension,Variant::RotationFlags,Variant::RotationOrder,Variant::VectorChannel,Variant::ScaleFlags,Variant::ScaleEnumOwner,Variant::NoNativeActor}) {
        Fixture fixture;const auto t=Build(fixture,variant);const auto initialized=InitializePortableRuntime(t.scripts);
        if(!initialized.passed){++refusals;fixture.UnchangedSources();continue;}const auto loaded=LoadPortableRuntimeMap(t.map);
        if(!loaded.passed){++refusals;fixture.UnchangedSources();continue;}
        const bool scale=variant==Variant::ScaleFlags||variant==Variant::ScaleEnumOwner;
        if(scale)Shape(ReadPortableRuntimeActorToWorld(Path("Plain")),"Plain","Engine.Actor","AActor::ToWorld",{-12,24,-36});
        Refused(fixture,Path(scale?"Brush0":"Plain"));fixture.UnchangedSources();
    }
}
} // namespace
int main() {
    try{Positive();NegativeSchemas();std::cout<<"PASS generated live native Actor/Brush ToWorld: "<<checks<<" checks, "<<refusals<<" refusals; qualified original fields, frozen defaults, live overlays, native dispatch and nonmutating complete snapshots.\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL generated live native ToWorld: "<<error.what()<<'\n';return 1;}
}
