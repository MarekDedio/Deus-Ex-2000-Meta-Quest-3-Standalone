#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_actor_line_trace.h"
#include "quest_model_line_hit.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Owned-original, read-only dormant-world integration. No original binary is
// loaded or executed. The cylinder helper below is an ADAPTER oracle only;
// independent x87 arithmetic and wrapper tests cover its numerical contract.
// This test does not prove original Move, startup, falling, crates or rendering.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Kind=QuestVr::Vm::Kind;
using Value=QuestVr::Vm::Value;
using Array=std::array<float,3u>;
using Vec=PortableModelVec3;
std::size_t checks{},refusals{},actorsSeen{},modelsSeen{},meshesSeen{},fallbacksSeen{},lineQueries{},modelQueries{};

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
            "Owned source bytes/size/timestamp changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;mutable std::vector<fs::path> files;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),
            "Primitive diagnostic temporary directory overlaps original installation/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-actor-primitive-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create primitive diagnostic directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& file:files) if(file.parent_path()==directory) fs::remove(file,ignored);
        if(!directory.empty()&&directory.parent_path()==parent) fs::remove(directory,ignored);
    }
    Bytes Snapshot(const std::string& leaf) const {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid primitive checkpoint leaf");
        const auto path=directory/(leaf+".sav");
        if(std::find(files.begin(),files.end(),path)==files.end()) files.push_back(path);
        Require(SavePortableRuntimeState(path.string()),"Cannot capture complete primitive checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),
            "Cannot read bounded primitive checkpoint");return bytes;
    }
};
template<class Operation> void Refused(Operation&& operation,const std::string& reason) {
    bool rejected{};try {operation();}catch(const std::exception&) {rejected=true;}
    Require(rejected,reason);++refusals;
}
Value Declared(const std::string& actor,const char* field,Kind kind) {
    const auto result=ReadPortableActorDeclaredScriptProperty(actor,field);
    Require(result.kind==kind,"Original declared field has wrong kind: "+actor+' '+field);return result;
}
std::string Qualified(const PortablePackageTables& table,std::int32_t reference) {
    auto path=GetPortableObjectPath(table,reference);
    if(reference>0&&!path.empty()) path=fs::path(table.sourcePath).stem().string()+'.'+path;
    return path;
}
std::string AssetClass(const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map,
    const std::string& path) {
    const PortablePackageTables* source{};
    const auto dot=path.find('.');Require(dot!=std::string::npos,"Selected asset lacks package qualification");
    const auto stem=Fold(path.substr(0u,dot));
    if(stem==Fold(fs::path(map.sourcePath).stem().string())) source=&map;
    for(const auto& table:packages)
        if(stem==Fold(fs::path(table.sourcePath).stem().string())) {
            Require(!source,"Selected asset package identity is ambiguous");source=&table;
        }
    Require(source!=nullptr,"Selected primitive package is not explicitly loaded: "+path);
    const auto index=FindPortableExport(*source,path.substr(dot+1u));
    Require(index<source->exports.size(),"Selected primitive export is absent");
    const auto& entry=source->exports.at(index);
    Require(entry.ObjClass!=0&&Qualified(*source,static_cast<std::int32_t>(index+1u))==path,
        "Selected primitive is a class or has noncanonical source/export identity");
    // Asset RF flags are not native-class/vtable authority. The class reference
    // must itself be an actual Core.Class import, under its Engine package.
    Require(entry.ObjClass<0,"Owned selected primitive unexpectedly uses a serialized asset UClass");
    const auto& imported=source->imports.at(static_cast<std::size_t>(-entry.ObjClass-1));
    Require(source->names.at(static_cast<std::size_t>(imported.ClassPackage)).Name=="Core"&&
        source->names.at(static_cast<std::size_t>(imported.ClassName)).Name=="Class"&&
        GetPortableObjectPath(*source,imported.ObjOuter)=="Engine",
        "Actual selected primitive Class import lacks its Core.Class/Engine provenance");
    return Qualified(*source,entry.ObjClass);
}
bool SamePrimitive(const PortableActorPrimitiveSnapshot& a,const PortableActorPrimitiveSnapshot& b) {
    return a.actorPath==b.actorPath&&a.primitivePath==b.primitivePath&&a.nativeClassPath==b.nativeClassPath&&
        a.lineImplementation==b.lineImplementation&&a.boundsImplementation==b.boundsImplementation;
}
void PrimitiveIs(const PortableActorPrimitiveSnapshot& actual,const std::string& actor,const std::string& asset,
    const std::string& cls) {
    const bool model=cls=="Engine.Model";
    Require(actual.actorPath==actor&&actual.primitivePath==asset&&actual.nativeClassPath==cls&&
        actual.lineImplementation==(model?"UModel::LineCheck":"UMesh::LineCheck")&&
        actual.boundsImplementation==(model?"UModel::GetCollisionBoundingBox":"UPrimitive::GetCollisionBoundingBox"),
        "Selected primitive has wrong actor/asset/native virtual implementation: "+actor);
}
void SameFloat(float actual,float expected,const std::string& reason) {
    Require(std::bit_cast<std::uint32_t>(actual)==std::bit_cast<std::uint32_t>(expected),reason);
}
void SameVector(const Array& actual,const Array& expected,const std::string& reason) {
    for(std::size_t i=0u;i<3u;++i) SameFloat(actual[i],expected[i],reason+" component "+std::to_string(i));
}
Vec Vector(const Array& value) {return {value[0u],value[1u],value[2u]};}
Array ArrayOf(const Vec& value) {return {value.x,value.y,value.z};}
struct Cylinder {Array location{};float radius{},height{};};
Cylinder Dimensions(const std::string& actor) {
    Cylinder result;
    result.location=Declared(actor,"Engine.Actor.Location",Kind::Vector).vector;
    result.radius=Declared(actor,"Engine.Actor.CollisionRadius",Kind::Float).floating;
    result.height=Declared(actor,"Engine.Actor.CollisionHeight",Kind::Float).floating;
    Require(std::isfinite(result.radius)&&std::isfinite(result.height)&&result.radius>=0.0f&&result.height>=0.0f,
        "Owned primitive cylinder dimensions are outside supported finite/nonnegative domain");
    for(const auto component:result.location)
        Require(std::isfinite(component)&&std::abs(component)<10'000'000.0f,
            "Owned bounds formula fixture exceeds its independently exact Float-sum domain");
    Require(result.radius<10'000'000.0f&&result.height<10'000'000.0f,
        "Owned bounds formula fixture exceeds its independently exact dimension domain");
    return result;
}
void Bounds(const PortableActorPrimitiveSnapshot& selected,const Cylinder& owner) {
    const auto actual=ReadPortableRuntimeActorPrimitiveBounds(selected.actorPath);
    Require(SamePrimitive(actual.primitive,selected),"Bounds query changed selected primitive provenance");
    // Independent literal original Float-store schedule, not the production
    // bounds helper. This checks the owned fixture values, not a universal
    // host-Double substitute for separately verified original PC64 arithmetic.
    const double retainedRadius=static_cast<double>(owner.radius)+1.0;
    const float storedRadius=static_cast<float>(retainedRadius);
    const float storedHeight=static_cast<float>(static_cast<double>(owner.height)+1.0);
    const Array minimum{
        static_cast<float>(static_cast<double>(owner.location[0u])-storedRadius),
        static_cast<float>(static_cast<double>(owner.location[1u])-retainedRadius),
        static_cast<float>(static_cast<double>(owner.location[2u])-storedHeight)};
    const Array maximum{
        static_cast<float>(static_cast<double>(storedRadius)+owner.location[0u]),
        static_cast<float>(retainedRadius+owner.location[1u]),
        static_cast<float>(static_cast<double>(storedHeight)+owner.location[2u])};
    SameVector(actual.minimum,minimum,"Actual primitive bounds minimum differs");
    SameVector(actual.maximum,maximum,"Actual primitive bounds maximum differs");
}
bool Line(const PortableActorPrimitiveSnapshot& selected,const Cylinder& owner,const Array& end,
    const Array& start,const Array& extent,std::uint32_t flags=0u) {
    const auto actual=ProbePortableRuntimeActorPrimitiveLine(selected.actorPath,end,start,extent,flags);
    Require(SamePrimitive(actual.primitive,selected),"Line query changed selected primitive provenance");
    QuestVr::OriginalActorLineHit expected{};expected.time=1.0f;expected.item=-1;
    auto canonicalExtent=extent;
    if(canonicalExtent[0u]==0.0f&&canonicalExtent[1u]==0.0f&&canonicalExtent[2u]==0.0f) canonicalExtent={0.0f,0.0f,0.0f};
    const QuestVr::OriginalActorCylinder cylinder{1u,Vector(owner.location),owner.radius,owner.height};
    const bool clear=QuestVr::CheckOriginalActorCylinder(expected,cylinder,Vector(end),Vector(start),Vector(canonicalExtent));
    Require(actual.clear==clear&&actual.hitOwner==(expected.actor==1u)&&
        actual.hitPrimitiveIsNone==(expected.primitive==0u)&&actual.item==expected.item,
        "Actual Mesh line query changed clear/owner/primitive/item packet semantics");
    SameFloat(actual.time,expected.time,"Actual Mesh line query changed seeded/result Time");
    SameVector(actual.location,ArrayOf(expected.location),"Actual Mesh line query changed seeded/result Location");
    SameVector(actual.normal,ArrayOf(expected.normal),"Actual Mesh line query changed seeded/partial/result Normal");
    ++lineQueries;return !clear;
}
// These are provider/packet adapter oracles. Independent x87/corner/extent
// tests, not this reuse of the kernels, own the numerical algorithm evidence.
void ModelQueries(const PortableActorPrimitiveSnapshot& selected,const PortableModelGeometry& model) {
    const auto packet=ReadPortableRuntimeActorToWorld(selected.actorPath);
    const QuestVr::OriginalModelCoords coords{Vector(packet.origin),Vector(packet.xAxis),Vector(packet.yAxis),Vector(packet.zAxis)};
    const auto expectedBounds=QuestVr::TransformOriginalModelBounds(model.bounds.minimum,model.bounds.maximum,coords);
    const auto bounds=ReadPortableRuntimeActorPrimitiveBounds(selected.actorPath);
    Require(SamePrimitive(bounds.primitive,selected),"Model bounds changed actual selected asset provenance");
    SameVector(bounds.minimum,ArrayOf(expectedBounds.minimum),"Selected Model bounds minimum differs from captured native ToWorld adapter");
    SameVector(bounds.maximum,ArrayOf(expectedBounds.maximum),"Selected Model bounds maximum differs from captured native ToWorld adapter");
    const auto location=Declared(selected.actorPath,"Engine.Actor.Location",Kind::Vector).vector;
    const Array start{location[0u]+256.0f,location[1u]+16.0f,location[2u]+8.0f};
    const Array end{location[0u]-256.0f,location[1u]-16.0f,location[2u]-8.0f},extent{1.0f,1.0f,1.0f};
    QuestVr::OriginalActorLineHit expected{};
    const bool clear=QuestVr::CheckOriginalModelExtent(expected,model,1u,2u,&coords,Vector(end),Vector(start),Vector(extent));
    const auto actual=ProbePortableRuntimeActorPrimitiveLine(selected.actorPath,end,start,extent);
    Require(SamePrimitive(actual.primitive,selected)&&actual.clear==clear&&actual.hitOwner==(expected.actor==2u)&&
        actual.hitPrimitiveIsNone==(expected.primitive==0u)&&actual.item==expected.item,"Selected Model line result/token provenance differs");
    SameFloat(actual.time,expected.time,"Selected Model line Time differs");
    SameVector(actual.location,ArrayOf(expected.location),"Selected Model line Location differs");
    SameVector(actual.normal,ArrayOf(expected.normal),"Selected Model line Normal differs");
    ++modelQueries;
}
std::vector<std::string> WorldPaths() {
    std::vector<std::string> paths;
    for(const auto& actor:GetPortableRuntimeMapActors(true)) paths.push_back(actor.objectPath+'|'+actor.classPath);
    return paths;
}
void SameModel(const PortableNativeModelSnapshot& before) {
    const auto after=ReadPortableRuntimeNativeModel();
    Require(after.available==before.available&&after.geometryCached==before.geometryCached&&
        after.mapGeneration==before.mapGeneration&&after.levelPath==before.levelPath&&after.modelPath==before.modelPath&&
        after.modelExportIndex==before.modelExportIndex&&after.cachedNodes==before.cachedNodes,
        "Mesh primitive queries decoded Model geometry or changed loader binding");
}
void Original(const fs::path& root) {
    Temporary temporary(root);std::vector<Source> sources;std::vector<PortablePackageTables> packages;
    for(const char* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow",
        "Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}) {
        sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
    }
    sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
    Require(InitializePortableRuntime(packages).passed&&LoadPortableRuntimeMap(map).passed,
        "Cannot initialize owned dormant Training primitive fixture");
    Require(!ReadPortableRuntimeNativeEngineClasses().prepared,"Fixture unexpectedly prepared an Engine/Primitive class domain");
    Require(!Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
        "Primitive fixture unexpectedly started the original world");
    const auto actors=GetPortableRuntimeMapActors(true);Require(!actors.empty(),"Owned Training actor registry is empty");
    const auto before=temporary.Snapshot("Before");const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto model=ReadPortableRuntimeNativeModel();const auto world=WorldPaths();
    std::string firstModel,firstMesh,firstNonemptyModel;std::size_t blocked{},explicitCrates{},explicitKnife{},explicitCrowbar{};
    std::map<std::string,PortableModelGeometry> selectedModels;
    Require(ReadPortableRuntimePrimitiveModelCache().entries.empty(),"Fresh map already has selected Model geometry cached");
    for(const auto& actor:actors) {
        ++actorsSeen;
        const auto brush=Declared(actor.objectPath,"Engine.Actor.Brush",Kind::Object).text;
        if(!brush.empty()) {
            const auto cls=AssetClass(packages,map,brush);Require(cls=="Engine.Model","Owned Brush is not an exact native Model");
            const auto selected=ReadPortableRuntimeActorPrimitive(actor.objectPath);
            PrimitiveIs(selected,actor.objectPath,brush,cls);
            auto found=selectedModels.find(brush);
            if(found==selectedModels.end()) {
                const auto dot=brush.find('.');const auto stem=Fold(brush.substr(0u,dot));
                const PortablePackageTables* source=stem==Fold(fs::path(map.sourcePath).stem().string())?&map:nullptr;
                for(const auto& table:packages) if(Fold(fs::path(table.sourcePath).stem().string())==stem) source=&table;
                Require(source!=nullptr,"Selected Model source disappeared");
                const auto index=FindPortableExport(*source,brush.substr(dot+1u));
                found=selectedModels.emplace(brush,LoadPortableCollisionModel68(*source,index)).first;
            }
            ModelQueries(selected,found->second);
            if(firstNonemptyModel.empty()&&!found->second.nodes.empty()) firstNonemptyModel=actor.objectPath;
            ++modelsSeen;if(firstModel.empty()) firstModel=actor.objectPath;
            continue; // Selected Brush does not require the Actor.Mesh branch.
        }
        const auto mesh=Declared(actor.objectPath,"Engine.Actor.Mesh",Kind::Object).text;
        if(mesh.empty()) {
            Refused([&] {(void)ReadPortableRuntimeActorPrimitive(actor.objectPath);},
                "Null Brush/Mesh fabricated an absent Engine.Cylinder fallback");
            ++fallbacksSeen;continue;
        }
        const auto cls=AssetClass(packages,map,mesh);
        Require(cls=="Engine.Mesh"||cls=="Engine.LodMesh","Owned Mesh has an unaudited native asset implementation");
        const auto selected=ReadPortableRuntimeActorPrimitive(actor.objectPath);
        PrimitiveIs(selected,actor.objectPath,mesh,cls);++meshesSeen;if(firstMesh.empty()) firstMesh=actor.objectPath;
        const auto owner=Dimensions(actor.objectPath);Bounds(selected,owner);
        const float reach=owner.radius+128.0f;
        const Array start{owner.location[0u]+reach,owner.location[1u],owner.location[2u]};
        const Array end{owner.location[0u]-reach,owner.location[1u],owner.location[2u]};
        blocked+=Line(selected,owner,end,start,{0.0f,0.0f,0.0f})?1u:0u;
        (void)Line(selected,owner,end,start,{-0.0f,+0.0f,-0.0f},std::numeric_limits<std::uint32_t>::max());
        (void)Line(selected,owner,end,start,{2.0f,3.0f,4.0f},0x20u);
        const Array away{start[0u]+128.0f,start[1u],start[2u]};
        Require(!Line(selected,owner,away,start,{}),"Owned away-from-cylinder sweep unexpectedly blocked");
        if(actor.classPath=="DeusEx.CrateBreakableMedGeneral") {
            Require(mesh=="DeusExDeco.CrateBreakableMed"&&cls=="Engine.LodMesh"&&owner.radius==34.0f&&owner.height==24.0f,
                "Actual Training crate primitive/default dimensions changed");
            if(actor.objectPath=="00_Training.CrateBreakableMedGeneral1")
                SameVector(owner.location,{1022.1097412109375f,-275.102783203125f,-103.89986419677734375f},
                    "Actual lower-crate authored Location changed");
            else {
                Require(actor.objectPath=="00_Training.CrateBreakableMedGeneral2","Unexpected third Training crate identity");
                SameVector(owner.location,{1023.128173828125f,-274.078857421875f,-16.89986419677734375f},
                    "Actual upper-crate authored Location changed");
            }
            ++explicitCrates;
        }
        if(actor.classPath=="DeusEx.WeaponCombatKnife") ++explicitKnife;
        if(actor.classPath=="DeusEx.WeaponCrowbar") ++explicitCrowbar;
    }
    Require(modelsSeen>0u&&meshesSeen>0u&&fallbacksSeen>0u&&blocked>0u&&
        explicitCrates==2u&&explicitKnife>0u&&explicitCrowbar>0u,
        "Owned Training did not exercise Model/Mesh/absent-engine branches and actual crate/knife/crowbar assets");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    Require(!firstNonemptyModel.empty(),"Owned fixture has no nonempty selected Model");
    Refused([&] {(void)ProbePortableRuntimeActorPrimitiveLine(firstNonemptyModel,{nan,nan,nan},{nan,nan,nan},{1,1,1});},
        "Model line query admitted active nonfinite endpoints");
    Refused([&] {(void)ProbePortableRuntimeActorPrimitiveLine(firstNonemptyModel,{},{},{});},
        "Nonempty Model zero-extent query silently substituted extent/Mesh collision");
    const auto selectedCache=ReadPortableRuntimePrimitiveModelCache();
    Require(selectedCache.entries.size()==selectedModels.size()&&selectedCache.retainedBytes>0u&&
        selectedCache.entries.size()<=selectedCache.maximumEntries&&selectedCache.retainedBytes<=selectedCache.maximumRetainedBytes,
        "Selected Model cache lost exact distinct geometry count or retained budget");
    for(const auto& entry:selectedCache.entries) {
        const auto found=selectedModels.find(entry.modelPath);
        Require(entry.mapGeneration==model.mapGeneration&&entry.levelPath==model.levelPath&&found!=selectedModels.end()&&
            entry.modelExportIndex==found->second.exportIndex&&entry.cachedNodes==found->second.nodes.size(),
            "Selected Model cache key/node identity disagrees with actual current map/export");
    }
    for(const auto& path:{firstModel,firstMesh}) {
        const auto canonical=ReadPortableRuntimeActorPrimitive(path),folded=ReadPortableRuntimeActorPrimitive(Fold(path));
        Require(SamePrimitive(canonical,folded),"Case-insensitive primitive lookup lost canonical actual identity");
    }
    for(const auto& path:std::vector<std::string>{"","None","Engine.Actor","Engine.Brush","Engine.Mover","Core.Object",
        "Engine.Actor.Mesh","@ClassDefault:Engine.Actor","@ClassDefault:Engine.Brush","@ClassDefault:Engine.Mover",
        "DeusExDeco.CrateBreakableMed","00_Training","00_Training.MissingActor","Engine.Primitive0","Transient.Primitive0",
        std::string(4097u,'x'),std::string("00_Training.LevelInfo0\0suffix",29u)})
        Refused([&] {(void)ReadPortableRuntimeActorPrimitive(path);},"Invalid/class/CDO/asset/non-slot receiver was accepted");
    Require(temporary.Snapshot("After")==before&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths()==world&&
        ReadPortableRuntimeLevelTime()==0.0&&GC::GetStats().numObjects==gc.numObjects&&GC::GetStats().memoryUsage==gc.memoryUsage&&
        !GetPortableRuntimeUnsavedScriptState()&&!ReadPortableRuntimeNativeEngineClasses().prepared,
        "Primitive diagnostics changed complete checkpoint/revision/world/clock/GC/dirty state or initialized Engine classes");
    Require(!Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
        "Primitive diagnostics started the original world");
    SameModel(model);for(const auto& source:sources) source.Unchanged();
    std::cout<<"PASS owned dormant Training primitive provider: "<<actorsSeen<<" actors, "<<modelsSeen<<" Model identities, "
        <<meshesSeen<<" Mesh/LodMesh queries, "<<modelQueries<<" Model bounds/extent adapter probes, "
        <<selectedCache.entries.size()<<" retained Models/"<<selectedCache.retainedBytes<<" accounted bytes, "
        <<fallbacksSeen<<" explicit absent-engine refusals, "<<lineQueries
        <<" caller-seeded line probes; actual crates/knife/crowbar, qualified fields and source/import class identity, "
        "all packet/bounds Float bits, complete read-only checkpoint/revision/GC/clock/source checks. "
        "Not original startup, Move/FindBase/falling, crate gameplay, render or headset evidence.\n";
}
} // namespace
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;
        if(argc==3&&std::string(argv[1])=="--game-root") root=argv[2];
        else if(argc==1) {const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured) root=configured;}
        else throw std::runtime_error("Usage: portable_actor_primitive_test [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        if(root.empty()) {std::cout<<"SKIP owned actor-primitive integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto gc=GC::GetStats();Original(fs::canonical(root));
        ShutdownPortableRuntime();GC::Collect();Require(GC::GetStats().numObjects==gc.numObjects&&GC::GetStats().memoryUsage==gc.memoryUsage,
            "Owned primitive test leaked rooted UObjects after shutdown");
        std::cout<<"PASS actor primitive integration "<<checks<<" checks, "<<refusals<<" explicit refusals\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
