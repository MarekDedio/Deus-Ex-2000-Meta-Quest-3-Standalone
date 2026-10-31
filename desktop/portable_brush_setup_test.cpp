#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_brush_setup.h"
#include "portable_brush_geometry.h"
#include "portable_brush_polys.h"
#include "portable_brush_texture_sources.h"
#include "portable_music_sources.h"
#include "quest_runtime_script_packages.h"
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

namespace {
namespace fs=std::filesystem;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Refuse(F run){bool failed{};try{run();}catch(const std::exception&){failed=true;}
    Require(failed,"Invalid texture source packet was admitted");++refusals;}
std::string Fold(std::string value){for(char& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c+32);return value;}
std::uint64_t Hash(const fs::path& path){std::ifstream input(path,std::ios::binary);Require(bool(input),"Cannot fingerprint original source");
    std::uint64_t value=14695981039346656037ull;std::array<char,65536u> bytes{};
    while(input){input.read(bytes.data(),bytes.size());for(std::streamsize i=0;i<input.gcount();++i){value^=static_cast<unsigned char>(bytes[static_cast<std::size_t>(i)]);value*=1099511628211ull;}}
    Require(input.eof()&&!input.bad(),"Cannot finish source fingerprint");return value;}
struct Source {fs::path path;std::uintmax_t size{};fs::file_time_type stamp;std::uint64_t hash{};
    explicit Source(fs::path p):path(fs::canonical(p)),size(fs::file_size(path)),stamp(fs::last_write_time(path)),hash(Hash(path)){}
    void Check()const{Require(fs::file_size(path)==size&&fs::last_write_time(path)==stamp&&Hash(path)==hash,"Original source changed: "+path.string());}};
struct Temporary {
    fs::path parent,path;std::vector<fs::path> files;
    explicit Temporary(const fs::path& original){parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&Fold(parent.string()).find(Fold(fs::canonical(original).string()))!=0u,"Temporary directory overlaps original/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<20u;++i){const auto candidate=parent/("quest-brush-setup-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)){path=fs::canonical(candidate);break;}}
        Require(!path.empty()&&path.parent_path()==parent,"Cannot create bounded test directory");}
    ~Temporary(){ShutdownPortableRuntime();std::error_code error;
        for(const auto& file:files)if(file.parent_path()==path)fs::remove(file,error);
        if(!path.empty()&&path.parent_path()==parent)fs::remove(path,error);}
    std::vector<std::uint8_t> Snapshot(const char* leaf){const auto file=path/(std::string(leaf)+".sav");
        if(std::find(files.begin(),files.end(),file)==files.end())files.push_back(file);
        Require(SavePortableRuntimeState(file.string()),"Cannot capture complete native setup checkpoint");std::vector<std::uint8_t> out;
        Require(QuestVr::ReadBoundedSaveFile(file.string(),QuestVr::kMaximumSaveRuntimeBytes,out),"Cannot read complete checkpoint");return out;}
};
std::size_t Export(const PortablePackageTables& table,const std::string& path){
    const auto stem=fs::path(table.sourcePath).stem().string();std::size_t result=table.exports.size();
    for(std::size_t i=0;i<table.exports.size();++i)if(Fold(stem+'.'+GetPortableObjectPath(table,static_cast<std::int32_t>(i+1u)))==Fold(path)){
        Require(result==table.exports.size(),"Ambiguous original export");result=i;}
    Require(result<table.exports.size(),"Missing original export: "+path);return result;}
bool Zero(PortableModelVec3 v){return std::bit_cast<std::uint32_t>(v.x)==0u&&std::bit_cast<std::uint32_t>(v.y)==0u&&std::bit_cast<std::uint32_t>(v.z)==0u;}
void Run(const fs::path& root){
    Temporary temporary(root);std::vector<Source> sources;std::vector<PortablePackageTables> scripts;
    for(const auto* name:QuestVr::RuntimeScriptPackages){sources.emplace_back(root/"System"/(std::string(name)+".u"));scripts.push_back(LoadPortablePackageTables(sources.back().path.string()));}
    sources.emplace_back(root/"System"/"DeusEx.ini");const auto ini=sources.back().path.string();
    sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
    const auto profile=QuestVr::OriginalLinkerLoadProfile{false,true,true};
    auto music=LoadPortableRuntimeMusicSources(scripts,map,ini,profile);
    for(auto& table:music.packages){sources.emplace_back(table.sourcePath);scripts.push_back(std::move(table));}
    auto textures=LoadPortableRuntimeBrushTextureSources(scripts,map,ini,profile);
    Require(textures.packages.size()==2u&&textures.bindings.size()==2u,"First Training did not discover its two actual texture sources");
    Require(textures.retainedBytes<=64u*1024u*1024u&&textures.serializedBytes<32u*1024u*1024u,
        "Texture tables borrowed whole mip payload as unbounded native identity scratch");
    for(const auto& binding:textures.bindings){
        const auto& table=textures.packages.at(static_cast<std::size_t>(std::find_if(textures.packages.begin(),textures.packages.end(),[&](const auto& p){return p.sourcePath==binding.sourcePath;})-textures.packages.begin()));
        Require(binding.exportIndex==Export(table,binding.objectPath)&&Fold(GetPortableObjectPath(table,table.exports[binding.exportIndex].ObjClass))=="engine.texture",
            "Actual material binding has wrong source export/Class");
    }
    for(const auto limit:{0u,1u}){PortableBrushTextureSourceLimits policy;policy.packages=limit;
        Refuse([&]{LoadPortableRuntimeBrushTextureSources(scripts,map,ini,profile,policy);});}
    {PortableBrushTextureSourceLimits policy;policy.nativeWork=0;Refuse([&]{LoadPortableRuntimeBrushTextureSources(scripts,map,ini,profile,policy);});}
    {PortableBrushTextureSourceLimits policy;policy.retainedBytes=1024;Refuse([&]{LoadPortableRuntimeBrushTextureSources(scripts,map,ini,profile,policy);});}
    {PortableBrushTextureSourceLimits policy;policy.serializedBytes=1;Refuse([&]{LoadPortableRuntimeBrushTextureSources(scripts,map,ini,profile,policy);});}
    {PortableBrushTextureSourceLimits policy;policy.bindings=1;Refuse([&]{LoadPortableRuntimeBrushTextureSources(scripts,map,ini,profile,policy);});}
    {PortableBrushTextureSourceLimits policy;policy.directoryEntries=0;Refuse([&]{LoadPortableRuntimeBrushTextureSources(scripts,map,ini,profile,policy);});}
    Refuse([&]{LoadPortableRuntimeBrushTextureSources(scripts,map,ini,{});});
    Refuse([&]{LoadPortableRuntimeBrushTextureSources(scripts,map,(temporary.path/"Absent.ini").string(),profile);});
    for(auto& table:textures.packages){sources.emplace_back(table.sourcePath);scripts.push_back(std::move(table));}
    Require(InitializePortableRuntime(scripts,0u,profile).passed&&LoadPortableRuntimeMap(map).passed,"Cannot initialize actual standalone source/Training context");
    const auto before=temporary.Snapshot("Before");const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    const auto sourceWorld=LoadPortableRootModel68(map);
    QuestVr::Vm::Limits limits;limits.nativeWork=1000000u;
    const auto setup=ProbePortableRuntimeBrushSetup(0u,limits);
    Require(setup.execution.status==QuestVr::Vm::Status::Returned&&!setup.execution.committed,"Original SetupActor root failed: "+setup.execution.error);
    Require(setup.movers.size()==25u&&setup.surfaces.size()==150u&&setup.completedPolygons==150u&&setup.gcOwnershipChecked&&
        setup.modelPolysEdges&&setup.textureEdges&&setup.actorEdges,"SetupActor cohort/typed GC graph is incomplete");
    Require(setup.originalSurfaces==sourceWorld.surfaces.size()&&setup.originalVectors==sourceWorld.vectors.size()&&setup.originalPoints==sourceWorld.points.size(),
        "SetupActor appended to another world Model");
    std::size_t ordinal{};
    for(const auto& mover:setup.movers){
        const auto model=LoadPortableCollisionModel68(map,Export(map,mover.modelPath));
        Require(model.polysReference>0&&static_cast<std::size_t>(model.polysReference)<=map.exports.size(),"Mover source lost its native Polys reference");
        const auto polys=LoadPortableBrushPolys68(map,static_cast<std::size_t>(model.polysReference-1));
        Require(mover.polygons==polys.polygons.size()&&mover.firstSurface==setup.originalSurfaces+ordinal&&mover.brushRoot==-1&&
            mover.oldPos==std::array<float,3u>{-1,-1,-1}&&mover.oldRot==std::array<std::int32_t,3u>{123,456,789}&&!mover.assimilated,
            "SetupActor native cache/root stores differ");
        for(std::size_t i=0;i<polys.polygons.size();++i,++ordinal){const auto& p=polys.polygons[i];const auto& row=setup.surfaces[ordinal];const auto& value=row.value;
            Require(row.moverPath==mover.actorPath&&row.polysPath==mover.polysPath&&row.polygon==i&&row.surface==setup.originalSurfaces+ordinal&&row.polygonLink==static_cast<std::int32_t>(row.surface),
                "SetupActor polygon LinkIndex/surface/actor identity correspondence differs");
            Require(row.texturePath==GetPortableObjectPath(map,p.textureReference)&&value.materialReference==p.textureReference&&
                value.brushActorReference==static_cast<std::int32_t>(Export(map,mover.actorPath)+1u),"SetupActor material/Actor edges lost original identity/provenance");
            Require(value.basePoint==static_cast<std::int32_t>(setup.originalPoints+ordinal)&&value.normalVector==static_cast<std::int32_t>(setup.originalVectors+3u*ordinal)&&
                value.textureU==value.normalVector+1&&value.textureV==value.normalVector+2&&value.lightMap==p.brushPoly&&value.brushPoly==static_cast<std::int32_t>(i)&&
                value.panU==p.panU&&value.panV==p.panV&&value.polyFlags==((p.polyFlags&0x3cffffffu)|(mover.specialLit?0x100000u:0u)),
                "SetupActor surface flags/indices/links/pans stores differ");
            for(const auto& v:row.placeholders)Require(Zero(v),"SetupActor prematurely transformed nonzero placeholders");
        }
    }
    Require(!HasPortableRuntimeMutableBrushGeometry()&&temporary.Snapshot("After")==before&&GetPortableRuntimeWorldRevision()==revision,
        "Completed SetupActor observation published native/reflected changes");
    for(const auto count:{1u,7u,149u,150u}){const auto refused=ProbePortableRuntimeBrushSetup(count,limits);
        Require(refused.execution.status==QuestVr::Vm::Status::Unsupported&&refused.completedPolygons==count&&
            refused.execution.error.find("Injected SetupActor refusal")!=std::string::npos,"Partial SetupActor write refused at a different boundary: "+refused.execution.error);
        Require(!HasPortableRuntimeMutableBrushGeometry()&&temporary.Snapshot("AfterRefusal")==before&&GetPortableRuntimeWorldRevision()==revision,
            "Partial native surface/polygon/pose/GC journal did not restore complete checkpoint");}
    const auto after=GC::GetStats();Require(after.numObjects==gc.numObjects&&after.memoryUsage==gc.memoryUsage,"SetupActor leaked/retired GC objects across rollback");
    for(const auto& source:sources)source.Check();
    std::cout<<"PASS actual first Training SetupActor:25 Movers/150 polygons; typed Model->Polys, Texture/Actor GC edges, exact surface/cache/link stores, "
        "1 completed and4 partial-write root rollbacks; "<<sources.size()<<" original sources unchanged. Texture admission packages=2 bindings=2 retained="<<textures.retainedBytes
        <<" serialized="<<textures.serializedBytes<<" work="<<textures.nativeWork.used<<"\n";
}
}
int main(int argc,char** argv){try{
    const char* root=argc==3&&std::string(argv[1])=="--game-root"?argv[2]:argc==1?std::getenv("DEUSEX_GAME_ROOT"):nullptr;
    if(!root){if(argc!=1)throw std::runtime_error("Usage: portable_brush_setup_test [--game-root OWNED_GAME_ROOT]");std::cout<<"SKIP owned brush SetupActor source integration\n";return 77;}
    ShutdownPortableRuntime();GC::Collect();const auto before=GC::GetStats();Run(fs::canonical(root));ShutdownPortableRuntime();GC::Collect();
    Require(GC::GetStats().numObjects==before.numObjects&&GC::GetStats().memoryUsage==before.memoryUsage,"SetupActor shutdown left GC roots");
    std::cout<<"PASS brush setup checks="<<checks<<" source refusals="<<refusals<<"; no pool growth/BSP insertion/tracker/startup/headset proof\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL native brush setup: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}}
