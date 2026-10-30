#include "Precomp.h"
#include "GC/GC.h"
#include "portable_music_sources.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "quest_runtime_script_packages.h"
#include "quest_save_bundle.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <tuple>

namespace {
namespace fs=std::filesystem;using Value=QuestVr::Vm::Value;using Kind=QuestVr::Vm::Kind;
std::size_t checks{},refusals{};
constexpr QuestVr::OriginalLinkerLoadProfile profile{false,true,true};
void Require(bool good,const std::string& why){++checks;if(!good)throw std::runtime_error(why);}
template<class F>void Refuse(F&& operation,const char* why){bool failed{};try{operation();}catch(const std::exception&){failed=true;}
    Require(failed,why);++refusals;}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(fs::path p):path(std::move(p)),size(fs::file_size(path)),modified(fs::last_write_time(path)),hash(Hash()){}
    std::uint64_t Hash()const{std::ifstream input(path,std::ios::binary);Require(bool(input),"Cannot fingerprint Music source");
        std::array<char,65536u> block{};std::uint64_t result=14695981039346656037ull;
        while(input){input.read(block.data(),block.size());for(std::streamsize i=0;i<input.gcount();++i){result^=static_cast<unsigned char>(block[static_cast<std::size_t>(i)]);result*=1099511628211ull;}}
        Require(input.eof()&&!input.bad(),"Cannot finish Music source fingerprint");return result;}
    void Unchanged()const{Require(size==fs::file_size(path)&&modified==fs::last_write_time(path)&&hash==Hash(),"Owned Music/map/script/INI source changed");}
};
struct Files {
    fs::path parent=fs::canonical(fs::temp_directory_path()),directory;
    Files(){directory=parent/("quest-music-sources-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(fs::create_directory(directory),"Cannot create Music source fixture directory");directory=fs::canonical(directory);}
    ~Files(){std::error_code error;const auto actual=fs::weakly_canonical(directory,error);
        if(!error&&actual==directory&&actual.parent_path()==parent&&actual.filename().string().rfind("quest-music-sources-",0u)==0u)fs::remove_all(actual,error);}
    fs::path Write(const char* name,const std::string& bytes)const{const auto path=directory/name;std::ofstream output(path,std::ios::binary);
        Require(bool(output),"Cannot create isolated Music source fixture");output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));output.close();
        Require(bool(output),"Cannot finish isolated Music source fixture");return path;}
    std::vector<std::uint8_t> Save(const char* name)const{const auto path=directory/name;Require(SavePortableRuntimeState(path.string()),"Cannot save Music fixture");
        std::vector<std::uint8_t> bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read Music fixture");return bytes;}
};
void Discovery(const std::vector<PortablePackageTables>& scripts,const PortablePackageTables& map,const fs::path& root,
    const Files& files,PortableMusicSourceBundle& bundle){
    const auto gc=GC::GetStats();const auto ini=(root/"System"/"DeusEx.ini").string();
    bundle=LoadPortableRuntimeMusicSources(scripts,map,ini,profile);
    Require(bundle.packages.size()==1u&&bundle.bindings.size()==1u,"Training did not discover its one actual Music dependency");
    const auto& b=bundle.bindings[0];Require(b.importReference==-180&&b.importerSourcePath==map.sourcePath&&b.exportIndex==0u&&
        b.objectPath=="Training_Music.Training_Music"&&b.objectFlags==0x000f0004u&&
        fs::equivalent(b.sourcePath,root/"Music"/"Training_Music.umx"),"Music import/export/source provenance differs");
    Require(bundle.nativeWork.used&&bundle.retainedBytes<=PortableMusicSourceLimits{}.retainedBytes&&bundle.serializedBytes==1450137u,
        "Music source discovery lost its bounded work/retained/source accounting");
    Require(gc.numObjects==GC::GetStats().numObjects&&gc.memoryUsage==GC::GetStats().memoryUsage&&!ReadPortableRuntimeObjectLookup().prepared,
        "Pre-runtime source discovery published UObjects/lookup or a hidden native root");
    const auto ordered=files.Write("Ordered.ini","[Core.System]\nPaths="+(files.directory/"Absent"/"*.umx").generic_string()+
        "\nPaths="+(root/"Music"/"*.umx").generic_string()+"\nPaths="+(files.directory/"LastAbsent"/"*.umx").generic_string()+"\n");
    const auto reordered=LoadPortableRuntimeMusicSources(scripts,map,ordered.string(),profile);
    Require(reordered.bindings.size()==1u&&fs::equivalent(reordered.packages[0].sourcePath,b.sourcePath),"Configured ordered Paths did not select first actual Music source");
    auto noMusic=map;noMusic.imports[179u].ClassName=noMusic.imports[175u].ClassName;
    Require(LoadPortableRuntimeMusicSources(scripts,noMusic,ini,profile).packages.empty(),"Loader enumerated unreferenced Music sources");
    auto ignoredPackage=map;ignoredPackage.imports[179u].ClassPackage=ignoredPackage.imports[175u].ClassPackage;
    Require(LoadPortableRuntimeMusicSources(scripts,ignoredPackage,ini,profile).bindings.size()==1u,
        "Music import class matching invented a ClassPackage gate absent from original UObject value resolution");
    auto bad=map;bad.imports[179u].ObjOuter=-180;
    Refuse([&]{LoadPortableRuntimeMusicSources(scripts,bad,ini,profile);},"Cyclic Music import outer accepted");
    bad=map;bad.imports[179u].ClassName=static_cast<std::int32_t>(bad.names.size());
    Refuse([&]{LoadPortableRuntimeMusicSources(scripts,bad,ini,profile);},"Music source admitted invalid authored name index");
    bad=map;bad.imports[179u].ObjOuter=1;
    Refuse([&]{LoadPortableRuntimeMusicSources(scripts,bad,ini,profile);},"Music import outer incorrectly accepted an export");
    auto duplicate=scripts;duplicate.push_back(scripts.front());
    Refuse([&]{LoadPortableRuntimeMusicSources(duplicate,map,ini,profile);},"Duplicate baseline Package identity accepted");
    Refuse([&]{LoadPortableRuntimeMusicSources(scripts,map,ini,{});},"Fresh Music export ignored source load mask0");
    const auto missing=files.Write("Missing.ini","[Core.System]\nPaths="+(files.directory/"Absent"/"*.umx").generic_string()+"\n");
    Refuse([&]{LoadPortableRuntimeMusicSources(scripts,map,missing.string(),profile);},"Missing Music source fabricated None");
    Refuse([&]{LoadPortableRuntimeMusicSources(scripts,map,(files.directory/"NoIni.ini").string(),profile);},"Missing INI became default search policy");
    for(unsigned control=0u;control<7u;++control){PortableMusicSourceLimits limits;
        if(control==0u)limits.nativeWork=0u;
        if(control==1u)limits.retainedBytes=1u;
        if(control==2u)limits.serializedBytes=16u;
        if(control==3u)limits.packages=0u;
        if(control==4u)limits.bindings=0u;
        if(control==5u)limits.directoryEntries=0u;
        if(control==6u)limits.nativeWork=QuestVr::NativeWork::MaximumLimit+1u;
        Refuse([&]{LoadPortableRuntimeMusicSources(scripts,map,ini,profile,limits);},"Music source ignored explicit finite limit");
    }
    // A real named but truncated package cannot force huge table reserves before
    // validation. This file exists only under this test's canonical temp root.
    auto fake=std::string(36u,'\0');const auto put=[&](std::size_t offset,std::uint32_t value){for(unsigned i=0;i<4u;++i)fake[offset+i]=static_cast<char>(value>>(8u*i));};
    put(0u,0x9e2a83c1u);put(4u,68u);put(12u,2000000u);put(16u,36u);put(24u,36u);put(32u,36u);
    const auto fakePath=files.Write("Training_Music.umx",fake);
    const auto fakeIni=files.Write("Fake.ini","[Core.System]\nPaths="+(files.directory/"*.umx").generic_string()+"\n");
    Refuse([&]{LoadPortableRuntimeMusicSources(scripts,map,fakeIni.string(),profile);},"Malformed Music package table count allocated/accepted");
    fs::copy_file(b.sourcePath,fakePath,fs::copy_options::overwrite_existing);
    auto copied=LoadPortablePackageTables(fakePath.string());copied.exports[0u].ObjClass=0;
    auto withWrong=scripts;withWrong.push_back(std::move(copied));
    Refuse([&]{LoadPortableRuntimeMusicSources(withWrong,map,ini,profile);},"Wrong actual Music export Class admitted by name alone");
}
void Runtime(std::vector<PortablePackageTables>& scripts,const PortablePackageTables& map,PortableMusicSourceBundle& bundle,
    const fs::path& root,const Files& files){
    for(auto& source:bundle.packages)scripts.push_back(std::move(source));
    Require(InitializePortableRuntime(scripts,1u,profile).passed,"Music baseline source admission failed");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&
        LoadPortableRuntimeMap(map).passed,"Music source/map prerequisites failed");
    const auto path=bundle.bindings[0u].objectPath;const auto before=ReadPortableRuntimeObjectLinker(path);
    Require(before.bound&&before.attached&&before.sourceExport&&before.sourceExportIndex==0u&&before.flags==0x000f0004u&&
        before.objectIndex&&before.outerIndex&&before.sourcePath==bundle.bindings[0u].sourcePath,"Music asset is not the actual baseline indexed export/Package-owned object");
    const auto cleanMap=files.Save("CleanMap.sav");
    const auto level="00_Training.LevelInfo0";const auto actors=GetPortableRuntimeMapActors(true);
    const auto start=std::find_if(actors.begin(),actors.end(),[](const auto& actor){return actor.classPath=="Engine.PlayerStart"&&actor.hasLocation;});
    Require(start!=actors.end(),"Authored PlayerStart missing for isolated Camera placement");
    const auto engine=std::find_if(scripts.begin(),scripts.end(),[](const auto& source){return fs::path(source.sourcePath).stem()=="Engine";});
    Require(engine!=scripts.end(),"Original Engine class source missing");
    const auto classIndex=FindPortableExport(*engine,"Camera");const auto cameraClass=LoadPortableClassDescriptor(*engine,classIndex);
    const auto camera=ExecutePortableActorFunction(level,"Engine.Actor.Spawn",{
        {Value::Text(Kind::Object,"Engine.Camera"),{}},{Value::Text(Kind::Object,{}),{}},{Value::Text(Kind::Name,"None"),{}},
        {Value::Vector({start->x,start->y,start->z}),{}},{Value::Rotator({start->pitch,start->yaw,start->roll}),{}}});
    Require(camera.passed()&&camera.committed&&camera.value.kind==Kind::Object&&!camera.value.text.empty(),
        "Cannot construct isolated original Camera for typed music stores: "+camera.error+" classFlags="+
            std::to_string(cameraClass.classFlags)+" sourceFlags="+std::to_string(static_cast<std::uint32_t>(engine->exports[classIndex].ObjFlags)));
    const auto pawn=camera.value.text;const auto baseline=files.Save("Baseline.sav");
    const std::vector<QuestVr::Vm::Evaluation> arguments{{Value::Text(Kind::Object,path),{}},{Value::Byte(3u),{}},{Value::Byte(7u),{}},{Value::Byte(1u),{}}};
    const auto set=ExecutePortableActorFunction(pawn,"Engine.PlayerPawn.ClientSetMusic",arguments);
    Require(set.passed()&&set.committed&&set.writes==4u,"Original ClientSetMusic did not perform its four ordinary typed stores: "+set.error);
    Require(ReadPortableActorDeclaredScriptProperty(pawn,"Engine.PlayerPawn.Song").text==path&&
        ReadPortableActorDeclaredScriptProperty(pawn,"Engine.PlayerPawn.SongSection").integer==3&&
        ReadPortableActorDeclaredScriptProperty(pawn,"Engine.PlayerPawn.CdTrack").integer==7&&
        ReadPortableActorDeclaredScriptProperty(pawn,"Engine.PlayerPawn.Transition").integer==1,"Typed Music reference or remaining original stores differ");
    Require(LoadPortableRuntimeState((files.directory/"Baseline.sav").string())&&files.Save("Restored.sav")==baseline,
        "Music source typed reference checkpoint restore differs");
    for(const auto cut:{std::size_t{0u},std::size_t{1u},std::size_t{3u}}){QuestVr::Vm::Limits limits;limits.writes=cut;
        const auto refused=ExecutePortableActorFunction(pawn,"Engine.PlayerPawn.ClientSetMusic",arguments,limits);
        Require(!refused.passed()&&!refused.committed&&files.Save("Refused.sav")==baseline,"Partial Music assignment escaped rollback");++refusals;
    }
    const auto stable=ReadPortableRuntimeObjectLinker(path);Require(stable==before,"Music source identity changed during typed stores/restore");
    // The spawned Camera is real script state. Respect the normal per-map
    // archive guard: restore our actual pre-birth checkpoint before retiring
    // the map, rather than bypassing preservation for this ownership test.
    Require(LoadPortableRuntimeState((files.directory/"CleanMap.sav").string())&&files.Save("CleanRestored.sav")==cleanMap,
        "Cannot restore genuine pre-birth map checkpoint before source-lifetime retirement");
    Require(UnloadPortableRuntimeMap()!=0u&&ReadPortableRuntimeObjectLinker(path)==before,"Map retirement incorrectly retired baseline Music export");
    Require(LoadPortableRuntimeMap(map).passed&&ReadPortableRuntimeObjectLinker(path)==before,"Same-map reload replaced/leaked baseline Music export");
    ShutdownPortableRuntime();GC::Collect();
}
}
int main(int argc,char** argv){std::cout.setf(std::ios::unitbuf);try{
    fs::path root;if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];else if(argc==1){if(const auto* value=std::getenv("DEUSEX_GAME_ROOT"))root=value;}
    else throw std::runtime_error("Usage: portable_music_sources_test [--game-root OWNED_GAME_ROOT]");
    if(root.empty()){std::cout<<"SKIP owned Music source admission: supply --game-root or DEUSEX_GAME_ROOT\n";return 77;}
    ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();std::vector<Source> sources;std::vector<PortablePackageTables> scripts;
    for(const auto* name:QuestVr::RuntimeScriptPackages){sources.emplace_back(root/"System"/(std::string(name)+".u"));scripts.push_back(LoadPortablePackageTables(sources.back().path.string()));}
    sources.emplace_back(root/"System"/"DeusEx.ini");sources.emplace_back(root/"Maps"/"00_Training.dx");
    const auto map=LoadPortablePackageTables(sources.back().path.string());sources.emplace_back(root/"Music"/"Training_Music.umx");
    Files files;PortableMusicSourceBundle bundle;Discovery(scripts,map,root,files,bundle);Runtime(scripts,map,bundle,root,files);
    const auto final=GC::GetStats();Require(initial.numObjects==final.numObjects&&initial.memoryUsage==final.memoryUsage,"Music source shutdown leaked UObject/source/Package ownership");
    for(const auto& source:sources)source.Unchanged();
    std::cout<<"PASS owned Music source admission checks="<<checks<<" refusals="<<refusals
        <<"; real authored import/export/Paths, baseline Package/linker/index lifetime, original ClientSetMusic typed stores and undo. NOT full native Music audio lifecycle/startup/Quest acceptance\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL Music source admission: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}}
