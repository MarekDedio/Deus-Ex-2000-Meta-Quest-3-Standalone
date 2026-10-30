#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_brush_coords.h"
#include "portable_brush_transform.h"
#include "quest_brush_poly_split.h"
#include "quest_brush_sphere_prefilter.h"
#include "portable_brush_geometry.h"
#include "quest_original_sine_table.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Owned-original, dormant-world integration only. Executes the port's actual
// read-only provider, never an original DLL, actor startup, Move or physics.
// The scalar helper is an adapter oracle: its arithmetic and pinned sine data
// have separate independent hardware-x87 tests, not this integration fixture.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
using Vec=PortableModelVec3;
std::size_t checks{},refusals{},actorCount{},brushCount{},moverCount{};
std::size_t moverSources{},transformedPolygons{},transformedVertices{};
std::array<std::size_t,4u> planeClassifications{};
std::size_t nativeSphereWrites{};
void Require(bool condition,const std::string& reason) {
    ++checks;if(!condition) throw std::runtime_error(reason);
}
std::string Fold(std::string value) {
    for(auto& c:value) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');
    return value;
}
bool Within(const fs::path& child,const fs::path& parent) {
    auto a=child.begin(),b=parent.begin();
    for(;b!=parent.end();++a,++b) if(a==child.end()||Fold(a->string())!=Fold(b->string())) return false;
    return true;
}
std::uint64_t SourceHash(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot read owned source: "+path.string());
    std::uint64_t hash=14695981039346656037ull;std::array<char,16384u> buffer{};std::uintmax_t total{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        total+=static_cast<std::uintmax_t>(count);Require(total<=512u*1024u*1024u,"Owned source fingerprint exceeds bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(file.eof(),"Owned source fingerprint read failed");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),hash(SourceHash(path)) {}
    void Unchanged() const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&SourceHash(path)==hash,
            "Owned original source bytes/size/timestamp changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;mutable std::vector<fs::path> files;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),
            "Coordinate diagnostic temporary directory overlaps original installation/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-actor-coords-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create owned coordinate diagnostic directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& file:files) if(file.parent_path()==directory) fs::remove(file,ignored);
        if(!directory.empty()&&directory.parent_path()==parent) fs::remove(directory,ignored);
    }
    Bytes Snapshot(const std::string& leaf) const {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid coordinate diagnostic leaf");
        const auto path=directory/(leaf+".sav");
        if(std::find(files.begin(),files.end(),path)==files.end()) files.push_back(path);
        Require(SavePortableRuntimeState(path.string()),"Cannot capture complete coordinate diagnostic checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),
            "Cannot read bounded coordinate diagnostic checkpoint");return bytes;
    }
};
struct ClassRecord {std::string path,base;bool native{};};
struct Classes {
    std::map<std::string,ClassRecord> rows;
    explicit Classes(const std::vector<PortablePackageTables>& packages) {
        for(const auto& package:packages) {
            const auto stem=fs::path(package.sourcePath).stem().string();
            for(std::size_t i=0u;i<package.exports.size();++i) {
                const auto& entry=package.exports[i];if(entry.ObjClass!=0) continue;
                const auto path=stem+'.'+GetPortableObjectPath(package,static_cast<std::int32_t>(i+1u));
                auto base=GetPortableObjectPath(package,entry.ObjBase);
                if(entry.ObjBase>0) base=stem+'.'+base;
                if(entry.ObjBase==0) base.clear();
                Require(rows.emplace(Fold(path),ClassRecord{path,base,
                    (static_cast<std::uint32_t>(entry.ObjFlags)&0x04000000u)!=0u}).second,
                    "Duplicate actual original class identity");
            }
        }
    }
    const ClassRecord& At(const std::string& path) const {
        const auto found=rows.find(Fold(path));Require(found!=rows.end(),"Missing actual original class ancestry: "+path);return found->second;
    }
    std::string Native(const std::string& path) const {
        std::vector<std::string> visited;auto current=path;
        for(unsigned i=0u;i<128u&&!current.empty();++i) {
            Require(std::find(visited.begin(),visited.end(),Fold(current))==visited.end(),"Cyclic original class ancestry");
            visited.push_back(Fold(current));const auto& row=At(current);
            if(row.native) return row.path;
            current=row.base;
        }
        throw std::runtime_error("Original actor class has no bounded RF_Native ancestor");
    }
    bool Derives(const std::string& path,const std::string& target) const {
        auto current=path;
        for(unsigned i=0u;i<128u&&!current.empty();++i) {
            if(Fold(current)==Fold(target)) return true;
            current=At(current).base;
        }
        Require(current.empty(),"Excessive original class ancestry");return false;
    }
};
Value Declared(const std::string& actor,const std::string& field,Kind kind) {
    auto value=ReadPortableActorDeclaredScriptProperty(actor,field);
    Require(value.kind==kind,"Original typed declared field mismatch: "+actor+' '+field);return value;
}
Vec Vector(const Value& value) {
    Require(value.kind==Kind::Vector,"Original field is not a Vector");
    return {value.vector[0u],value.vector[1u],value.vector[2u]};
}
QuestVr::OriginalBrushScale Scale(const Value& value) {
    Require(value.kind==Kind::Struct&&value.fields.size()==3u,"Original Scale field has wrong typed shape");
    const auto& vector=value.fields.at("scale");const auto& rate=value.fields.at("sheerrate");const auto& axis=value.fields.at("sheeraxis");
    Require(rate.kind==Kind::Float&&axis.kind==Kind::Byte,"Original Scale scalar field type mismatch");
    return {Vector(vector),rate.floating,static_cast<std::uint8_t>(axis.integer)};
}
QuestVr::OriginalModelCoords Expected(const std::string& actor,bool brush) {
    const auto location=Vector(Declared(actor,"Engine.Actor.Location",Kind::Vector));
    const auto rotation=Declared(actor,"Engine.Actor.Rotation",Kind::Rotator).rotation;
    const QuestVr::OriginalBrushRotation angles{rotation[0u],rotation[1u],rotation[2u]};
    if(!brush) return QuestVr::BuildOriginalActorCoords(location,angles,QuestVr::OriginalSineProfile::Table());
    const auto pivot=Vector(Declared(actor,"Engine.Actor.PrePivot",Kind::Vector));
    const auto main=Scale(Declared(actor,"Engine.Brush.MainScale",Kind::Struct));
    const auto post=Scale(Declared(actor,"Engine.Brush.PostScale",Kind::Struct));
    // Original native+340 is PostScale, +32C is MainScale; never DrawScale,
    // render snapshot's composite scale, TempScale or unqualified shadows.
    return QuestVr::BuildOriginalBrushCoords(location,angles,pivot,post,main,QuestVr::OriginalSineProfile::Table());
}
void SameVector(const std::array<float,3u>& actual,Vec expected,const std::string& label) {
    const std::array<float,3u> components{expected.x,expected.y,expected.z};
    for(std::size_t i=0u;i<3u;++i) Require(std::bit_cast<std::uint32_t>(actual[i])==std::bit_cast<std::uint32_t>(components[i]),
        label+" Float component bits differ at "+std::to_string(i));
}
void SameCoords(const PortableActorToWorldSnapshot& actual,const QuestVr::OriginalModelCoords& expected) {
    SameVector(actual.origin,expected.origin,actual.actorPath+" Origin");SameVector(actual.xAxis,expected.xAxis,actual.actorPath+" XAxis");
    SameVector(actual.yAxis,expected.yAxis,actual.actorPath+" YAxis");SameVector(actual.zAxis,expected.zAxis,actual.actorPath+" ZAxis");
}
QuestVr::OriginalBrushModelCoords SameModelCoords(const std::string& actor) {
    const auto rotation=Declared(actor,"Engine.Actor.Rotation",Kind::Rotator).rotation;
    const auto main=Scale(Declared(actor,"Engine.Brush.MainScale",Kind::Struct));
    const auto post=Scale(Declared(actor,"Engine.Brush.PostScale",Kind::Struct));
    const auto expected=QuestVr::BuildOriginalBrushModelCoords({rotation[0u],rotation[1u],rotation[2u]},main,post,
        QuestVr::OriginalSineProfile::Table());
    const auto actual=ReadPortableRuntimeBrushModelCoords(actor);
    Require(actual.actorPath==actor,"BuildCoords changed canonical actor identity");
    const auto same=[&](const QuestVr::OriginalModelCoords& a,const QuestVr::OriginalModelCoords& b,const char* label) {
        for(const auto& pair:{std::pair{a.origin,b.origin},std::pair{a.xAxis,b.xAxis},std::pair{a.yAxis,b.yAxis},std::pair{a.zAxis,b.zAxis}})
            SameVector({pair.first.x,pair.first.y,pair.first.z},pair.second,actor+' '+label);
    };
    same(actual.coords.point,expected.point,"BuildCoords PointXform");
    same(actual.coords.vector,expected.vector,"BuildCoords VectorXform");
    Require(std::bit_cast<std::uint32_t>(actual.coords.orientation)==std::bit_cast<std::uint32_t>(expected.orientation),
        "BuildCoords orientation sign bits differ");
    return actual.coords;
}
void TransformMoverSource(const PortablePackageTables& map,const PortableModelGeometry& world,const std::string& actor,
    const QuestVr::OriginalBrushModelCoords& coords) {
    // Composed SOURCE-data integration only: actual qualified Actor.Brush ->
    // serialized Model.Polys -> retained records -> copied transform/sphere.
    // This does not mutate source/live UPolys links or publish a BSP tracker.
    const auto brush=Declared(actor,"Engine.Actor.Brush",Kind::Object);
    Require(!brush.text.empty(),"Selected Training mover has no actual Brush Model");
    const auto stem=fs::path(map.sourcePath).stem().string();std::size_t selected=map.exports.size();
    for(std::size_t i=0;i<map.exports.size();++i)
        if(Fold(stem+'.'+GetPortableObjectPath(map,static_cast<std::int32_t>(i+1u)))==Fold(brush.text)) {
            Require(selected==map.exports.size(),"Ambiguous Training Brush export");selected=i;
        }
    Require(selected<map.exports.size()&&Fold(GetPortableObjectPath(map,map.exports[selected].ObjClass))=="engine.model",
        "Mover Brush does not bind an actual source Model export");
    const auto model=LoadPortableCollisionModel68(map,selected);
    Require(model.polysReference>0&&static_cast<std::size_t>(model.polysReference)<=map.exports.size(),
        "Selected mover Model lacks its actual local Polys reference");
    const auto source=LoadPortableBrushPolys68(map,static_cast<std::size_t>(model.polysReference-1));
    const auto pre=Vector(Declared(actor,"Engine.Actor.PrePivot",Kind::Vector));
    const auto location=Vector(Declared(actor,"Engine.Actor.Location",Kind::Vector));
    std::size_t count{};for(const auto& p:source.polygons)count+=static_cast<std::size_t>(p.vertexCount);
    Require(count<=1600000u,"Source mover vertex sum exceeds admitted polygon storage");
    std::vector<Vec> points;points.reserve(count);
    for(const auto& poly:source.polygons) {
        const auto transformed=QuestVr::TransformOriginalBrushPoly(poly,coords,pre,location);
        Require(transformed.polyFlags==poly.polyFlags&&transformed.actorReference==poly.actorReference&&
            transformed.textureReference==poly.textureReference&&transformed.itemNameReference==poly.itemNameReference&&
            transformed.vertexCount==poly.vertexCount&&transformed.link==poly.link&&transformed.brushPoly==poly.brushPoly&&
            transformed.panU==poly.panU&&transformed.panV==poly.panV,"Actual source polygon lost nongeometry fields during transform");
        const auto clipped=QuestVr::MakeOriginalBrushClipPolygon(transformed);
        const auto stride=std::max<std::size_t>(1u,world.nodes.size()/128u);
        for(std::size_t node=0;node<world.nodes.size();node+=stride) {
            const auto& sourcePlane=world.nodes[node];
            const QuestVr::OriginalExtentPlane plane{sourcePlane.planeX,sourcePlane.planeY,sourcePlane.planeZ,sourcePlane.planeW};
            const auto classified=QuestVr::SplitOriginalBrushClipPolygon(clipped,plane,false);
            const auto split=QuestVr::SplitOriginalBrushClipPolygon(clipped,plane,true);
            Require(classified.side==split.side&&split.side>=0&&split.side<=3&&!classified.front&&!classified.back,
                "Actual Training plane classification/build paths disagree");
            ++planeClassifications[static_cast<std::size_t>(split.side)];
            if(split.side==3) {
                Require(split.front&&split.back,"Actual Training split lacks one fragment");
                for(const auto* fragment:{&*split.front,&*split.back}) {
                    Require(fragment->vertexCount>=3&&fragment->vertexCount<=16&&fragment->polyFlags==clipped.polyFlags,
                        "Actual Training plane split lost active polygon/count/flags");
                    SameVector({fragment->base.x,fragment->base.y,fragment->base.z},clipped.base,"Training split copied Base");
                    SameVector({fragment->normal.x,fragment->normal.y,fragment->normal.z},clipped.normal,"Training split copied Normal");
                }
            }else Require(!split.front&&!split.back,"Actual Training nonsplit constructed fragments");
        }
        for(std::int32_t i=0;i<transformed.vertexCount;++i)points.push_back(transformed.vertices[static_cast<std::size_t>(i)]);
    }
    Require(points.size()==count,"Actual mover transform lost active vertices");
    const auto sphere=QuestVr::BuildOriginalBrushSphere(points);
    Require(std::isfinite(sphere.x)&&std::isfinite(sphere.y)&&std::isfinite(sphere.z)&&std::isfinite(sphere.radius)&&sphere.radius>=0,
        "Actual mover source sphere is nonfinite/negative");
    for(const auto& point:points) {
        const double x=double(point.x)-sphere.x,y=double(point.y)-sphere.y,z=double(point.z)-sphere.z;
        Require(x*x+y*y+z*z<=double(sphere.radius)*sphere.radius,"Actual mover sphere does not enclose transformed vertices");
    }
    auto expected=world;
    QuestVr::OriginalBrushSphereFilterQueries prefilter;
    std::size_t expectedWrites{},steps{};
    prefilter.work=[&]{Require(++steps<=100000u,"Actual source prefilter exceeded fixture work bound");};
    prefilter.readNode=[&](std::int32_t i){return expected.nodes.at(static_cast<std::size_t>(i));};
    prefilter.writeFlags=[&](std::int32_t i,std::uint8_t f){expected.nodes.at(static_cast<std::size_t>(i)).nodeFlags=f;++expectedWrites;};
    QuestVr::PrecomputeOriginalBrushSphereFilter(expected.nodes.size(),sphere,prefilter);
    const auto flags=[](const PortableModelGeometry& model){std::uint64_t hash=14695981039346656037ull;
        for(const auto& node:model.nodes){hash^=node.nodeFlags;hash*=1099511628211ull;}return hash;};
    QuestVr::Vm::Limits limits;limits.nativeWork=200000u;
    const auto probe=ProbePortableRuntimeBrushSphereFilter(sphere,0u,limits);
    Require(probe.execution.status==QuestVr::Vm::Status::Returned&&!probe.execution.committed,
        "Actual Model prefilter root failed: "+probe.execution.error);
    Require(probe.mutableOwnerObserved&&probe.queryUsesMutable&&probe.immutableProofRefused&&probe.revision==1u&&
        probe.generation&&probe.nodes==world.nodes.size()&&probe.surfaces==world.surfaces.size()&&probe.retainedBytes&&
        probe.beforeFlags==flags(world)&&probe.afterFlags==flags(expected)&&probe.writes==expectedWrites,
        "Native Model-owned prefilter disagrees with source geometry/callbacks or query/proof separation");
    Require(!HasPortableRuntimeMutableBrushGeometry(),"Successful prefilter observation published mutable geometry");
    const auto refused=ProbePortableRuntimeBrushSphereFilter(sphere,1u,limits);
    Require(refused.execution.status==QuestVr::Vm::Status::Unsupported&&refused.mutableOwnerObserved&&refused.writes==1u&&
        refused.execution.error.find("Injected native Model journal refusal")!=std::string::npos&&!HasPortableRuntimeMutableBrushGeometry(),
        "Partial native Model flag mutation did not refuse and restore the complete owner");
    nativeSphereWrites+=probe.writes;
    ++moverSources;transformedPolygons+=source.polygons.size();transformedVertices+=count;
}
template<class F> void Refused(F&& operation,const std::string& why) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,why);++refusals;
}
std::vector<std::string> WorldPaths() {
    std::vector<std::string> paths;for(const auto& actor:GetPortableRuntimeMapActors(true)) paths.push_back(actor.objectPath);return paths;
}
void Original(const fs::path& root) {
    Temporary temporary(root);std::vector<Source> sources;std::vector<PortablePackageTables> packages;
    for(const char* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow",
        "Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}) {
        sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
    }
    sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
    const Classes classes(packages);
    Require(InitializePortableRuntime(packages).passed&&LoadPortableRuntimeMap(map).passed,
        "Cannot initialize read-only original Training metadata fixture");
    const auto actors=GetPortableRuntimeMapActors(true);Require(!actors.empty(),"Original Training actor fixture is empty");
    const auto dormant=Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool);
    Require(!dormant.boolean,"Original coordinate test unexpectedly has a begun world");
    const auto before=temporary.Snapshot("Before");const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto model=ReadPortableRuntimeNativeModel();const auto world=WorldPaths();
    const auto sourceWorld=LoadPortableRootModel68(map);
    std::string brushPath,genericPath;
    for(const auto& actor:actors) {
        const bool brush=classes.Derives(actor.classPath,"Engine.Brush");
        const bool mover=classes.Derives(actor.classPath,"Engine.Mover");
        const auto expected=Expected(actor.objectPath,brush);const auto actual=ReadPortableRuntimeActorToWorld(actor.objectPath);
        Require(actual.actorPath==actor.objectPath&&actual.nativeClassPath==classes.Native(actor.classPath)&&
            actual.nativeImplementation==(brush?"ABrush::ToWorld":"AActor::ToWorld"),
            "Original native class/virtual implementation provenance mismatch: "+actor.objectPath);
        SameCoords(actual,expected);++actorCount;
        if(brush) {const auto coords=SameModelCoords(actor.objectPath);++brushCount;if(brushPath.empty()) brushPath=actor.objectPath;
            if(mover&&!Declared(actor.objectPath,"Engine.Actor.bStatic",Kind::Bool).boolean)TransformMoverSource(map,sourceWorld,actor.objectPath,coords);}
        else {Refused([&]{static_cast<void>(ReadPortableRuntimeBrushModelCoords(actor.objectPath));},"Generic Actor accepted by ABrush.BuildCoords");
            if(genericPath.empty()) genericPath=actor.objectPath;}
        if(mover) ++moverCount;
    }
    Require(brushCount>0u&&moverCount>0u&&actorCount>brushCount&&!genericPath.empty(),
        "Original Training did not exercise both actual native ToWorld branches and Movers");
    Require(moverSources&&transformedPolygons&&transformedVertices,"Training did not exercise positive moving-brush source transforms");
    Require(planeClassifications[0]&&planeClassifications[1]&&planeClassifications[2]&&planeClassifications[3],
        "Actual Training planes did not exercise all fast split outcomes");
    // Case-insensitive lookup must still return the original canonical identity.
    for(const auto& path:{brushPath,genericPath}) {
        const auto query=ReadPortableRuntimeActorToWorld(Fold(path));Require(query.actorPath==path,"Case-folded lookup fabricated a new actor identity");
        SameCoords(query,Expected(path,classes.Derives(std::find_if(actors.begin(),actors.end(),[&](const auto& row) {
            return row.objectPath==path;})->classPath,"Engine.Brush")));
    }
    for(const auto& path:std::vector<std::string>{"","None","Engine.Actor","Engine.Brush","Engine.Mover","Core.Object",
        "Engine.Brush.MainScale","@ClassDefault:Engine.Actor","@ClassDefault:Engine.Brush","@ClassDefault:Engine.Mover",
        "00_Training","00_Training.MissingActor","MissingMap.Mover0","Default__Mover",
        "00_Training.Default__Mover",std::string(4097u,'x'),std::string("00_Training.LevelInfo0\0suffix",29u)})
        {Refused([&] {static_cast<void>(ReadPortableRuntimeActorToWorld(path));},"Invalid/class/CDO/non-slot identity was accepted");
        Refused([&] {static_cast<void>(ReadPortableRuntimeBrushModelCoords(path));},"Invalid/class/CDO/non-slot BuildCoords identity was accepted");}
    const auto afterModel=ReadPortableRuntimeNativeModel();
    Require(temporary.Snapshot("After")==before&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths()==world&&
        ReadPortableRuntimeLevelTime()==0.0&&GC::GetStats().numObjects==gc.numObjects&&GC::GetStats().memoryUsage==gc.memoryUsage,
        "Coordinate reads/refusals changed complete checkpoint/revision/native time/world slots/GC graph");
    Require(afterModel.available==model.available&&afterModel.geometryCached==model.geometryCached&&
        afterModel.mapGeneration==model.mapGeneration&&afterModel.levelPath==model.levelPath&&afterModel.modelPath==model.modelPath&&
        afterModel.modelExportIndex==model.modelExportIndex&&afterModel.cachedNodes==model.cachedNodes,
        "Coordinate provider changed native Model binding or decoded geometry");
    for(const auto& source:sources) source.Unchanged();
    std::cout<<"PASS original dormant Training coordinate provider: "<<actorCount<<" actors, "<<brushCount<<" Brushes, "<<moverCount
        <<" Movers; qualified original fields, actual RF_Native ancestry, all12 Float bits, complete read-only checkpoint/revision/GC/source checks. "
        "Not original startup, Move/FindBase/falling, render or headset evidence.\n";
    std::cout<<"Actual non-static mover source transforms: "<<moverSources<<" Models/Polys, "<<transformedPolygons
        <<" polygons, "<<transformedVertices<<" vertices; no live Model/UPolys/BSP writes.\n";
    std::cout<<"Actual source-plane classification outcomes coplanar/front/back/split: "<<planeClassifications[0]<<'/'
        <<planeClassifications[1]<<'/'<<planeClassifications[2]<<'/'<<planeClassifications[3]<<"; copied fragment checks, NOT BSP insertion.\n";
    std::cout<<"Actual Model-owned sphere prefilter writes="<<nativeSphereWrites<<" across25 successful root rollbacks plus25 partial-write refusals; "
        "current geometry queries and immutable-source proof separation verified, no tracker/pool publication.\n";
}
} // namespace
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;
        if(argc==3&&std::string(argv[1])=="--game-root") root=argv[2];
        else if(argc==1) {const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured) root=configured;}
        else throw std::runtime_error("Usage: portable_actor_coords_test [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        if(root.empty()) {std::cout<<"SKIP owned actor-coordinate integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto gc=GC::GetStats();Original(fs::canonical(root));
        ShutdownPortableRuntime();GC::Collect();Require(GC::GetStats().numObjects==gc.numObjects&&GC::GetStats().memoryUsage==gc.memoryUsage,
            "Owned coordinate test leaked rooted UObjects after shutdown");
        std::cout<<"PASS actor coordinate integration "<<checks<<" checks, "<<refusals<<" identity refusals\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
