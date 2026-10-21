#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_engine_collision.h"
#include "portable_object_lookup.h"
#include "quest_runtime_script_packages.h"
#include "quest_save_bundle.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
namespace fs=std::filesystem;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
std::uint64_t Hash(const fs::path& path){std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint source");
    std::uint64_t hash=14695981039346656037ull;std::array<char,16384u> bytes{};
    while(file){file.read(bytes.data(),bytes.size());for(std::streamsize i=0;i<file.gcount();++i){hash^=static_cast<unsigned char>(bytes[i]);hash*=1099511628211ull;}}
    Require(file.eof(),"Source fingerprint failed");return hash;}
struct Source {fs::path path;std::uintmax_t size;fs::file_time_type time;std::uint64_t hash;
    explicit Source(fs::path file):path(fs::canonical(file)),size(fs::file_size(path)),time(fs::last_write_time(path)),hash(Hash(path)){}
    void Unchanged()const{Require(fs::file_size(path)==size&&fs::last_write_time(path)==time&&Hash(path)==hash,"Original source changed");}};
struct Temporary {
    fs::path directory;
    Temporary(){const auto parent=fs::canonical(fs::temp_directory_path());
        for(unsigned i=0;i<100u;++i){const auto path=parent/("quest-deferred-ownership-"+std::to_string(i));
            if(fs::create_directory(path)){directory=fs::canonical(path);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated checkpoint directory");}
    ~Temporary(){ShutdownPortableRuntime();std::error_code error;fs::remove(directory/"Before.sav",error);fs::remove(directory/"After.sav",error);fs::remove(directory,error);}
    std::vector<std::uint8_t> Save(const char* leaf){std::vector<std::uint8_t> result;const auto path=directory/leaf;
        Require(SavePortableRuntimeState(path.string()),"Cannot save full dormant map state");
        Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,result),"Cannot read full checkpoint");return result;}
};
std::string Link(const PortableDeferredOwnershipSnapshot& state,const std::string& path){
    const auto found=std::find_if(state.actorLinks.begin(),state.actorLinks.end(),[&](const auto& value){return value.first==path;});
    Require(found!=state.actorLinks.end(),"Missing bound native Actor.Deleted field");return found->second;
}
void Run(const fs::path& root){
    Temporary temporary;std::vector<PortablePackageTables> packages;std::vector<Source> sources;
    for(const auto* name:QuestVr::RuntimeScriptPackages){const auto file=root/"System"/(std::string(name)+".u");
        sources.emplace_back(file);packages.push_back(LoadPortablePackageTables(file.string()));}
    sources.emplace_back(root/"Maps"/"00_Training.dx");
    sources.emplace_back(root/"System"/"DeusEx.ini");
    const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize owned scripts");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared&&PreparePortableRuntimeObjectLookup().prepared&&
        PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared,"Cannot prepare actual native Engine/lookup");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load ONLY first Training");
    const auto waiting=ExecutePortableActorFunction("00_Training.Doctor1","PlayWaiting");
    Require(waiting.passed()&&waiting.committed,"Cannot establish actual Doctor clock");
    const auto baseline=temporary.Save("Before.sav");const auto before=ReadPortableRuntimeDeferredOwnership();
    Require(before.available&&before.firstDeletedPath.empty(),"Initial queue is not decoded first-Training None");
    const auto revision=GetPortableRuntimeWorldRevision();const auto lookup=ReadPortableRuntimeObjectLookup();
    const std::vector<std::string> paths{"00_Training.CrateBreakableMedGeneral1","00_Training.CrateBreakableMedGeneral2"};
    std::vector<PortableObjectLinkerSnapshot> linkers;for(const auto& path:paths)linkers.push_back(ReadPortableRuntimeObjectLinker(path));
    GC::Collect();const auto gc=GC::GetStats();
    const auto rollback=[&]{Require(ReadPortableRuntimeDeferredOwnership()==before,"Queue/Deleted native rollback failed");
        Require(temporary.Save("After.sav")==baseline&&GetPortableRuntimeWorldRevision()==revision,"Full save/clock/revision rollback failed");
        const auto actual=ReadPortableRuntimeObjectLookup();Require(actual.objects==lookup.objects&&actual.generation==lookup.generation&&actual.retainedBytes==lookup.retainedBytes,"UObject lookup was retired early");
        for(std::size_t i=0;i<paths.size();++i)Require(ReadPortableRuntimeObjectLinker(paths[i])==linkers[i],"Native linker/header rollback failed");
        GC::Collect();const auto after=GC::GetStats();Require(after.numObjects==gc.numObjects&&after.memoryUsage==gc.memoryUsage,"Deferred edges leaked/collected native objects");};
    const auto completed=[&](const auto& result){Require(result.execution.passed()&&!result.execution.committed&&result.completed,
            "Actual native DestroyActor refused: "+result.execution.error);
        Require(result.returned==std::vector<std::int32_t>{1,1}&&result.before==before,"Wrong original return/before queue");
        Require(result.after.firstDeletedPath==paths[1]&&Link(result.after,paths[1])==paths[0]&&Link(result.after,paths[0]).empty(),"Real two-removal queue did not preserve original LIFO edges");
        Require(result.captureRefused&&result.restoreRefused&&result.travelRefused,"Changed native deferred state passed an unsupported archive/travel path");};
    const auto full=ProbePortableActorNativeDestroy(paths);completed(full);rollback();
    Require(full.execution.writes<128u,"Diagnostic write-prefix sweep exceeds bounded case count");
    for(std::size_t prefix=0;prefix<full.execution.writes;++prefix){QuestVr::Vm::Limits limits;limits.writes=prefix;
        const auto result=ProbePortableActorNativeDestroy(paths,limits);Require(!result.execution.passed()&&!result.execution.committed,"Write-prefix failure published destruction");rollback();++refusals;}
    const auto warm=ProbePortableActorNativeDestroy(paths);completed(warm);rollback();
    Require(warm.execution.nativeWork&&warm.execution.nativeWork->used>3u,"Missing caller native-work ledger");
    const auto work=warm.execution.nativeWork->used;
    for(const auto prefix:{std::size_t{0},work/2u,work-1u}){QuestVr::Vm::Limits limits;limits.nativeWork=prefix;
        const auto result=ProbePortableActorNativeDestroy(paths,limits);Require(!result.execution.passed()&&result.execution.nativeWork&&result.execution.nativeWork->exhausted,"Native-work prefix ignored");rollback();++refusals;}
    std::cout<<"PASS actual first Training two native DestroyActor removals: "<<full.execution.writes<<" writes / "<<work<<" work; exact LIFO, archive refusals and full native rollback. NOT startup/cleanup/usable loot/gravity.\n";
    ShutdownPortableRuntime();for(const auto& source:sources)source.Unchanged();
    std::cout<<"UNCHANGED "<<sources.size()<<" original sources; ONLY first Training loaded\n";
}
}
int main(int argc,char** argv){std::cout.setf(std::ios::unitbuf);try{
    Require(argc<=2,"Usage: portable_deferred_ownership_test [OWNED_GAME_ROOT]");const auto* env=std::getenv("DEUSEX_GAME_ROOT");
    const fs::path root=argc==2?fs::path(argv[1]):env?fs::path(env):fs::path{};
    if(root.empty()){std::cout<<"SKIP owned first-Training deferred ownership\n";return 77;}
    ShutdownPortableRuntime();GC::Collect();const auto before=GC::GetStats();Run(fs::canonical(root));ShutdownPortableRuntime();GC::Collect();const auto after=GC::GetStats();
    Require(before.numObjects==after.numObjects&&before.memoryUsage==after.memoryUsage,"Shutdown leaked deferred actors");
    std::cout<<"PASS deferred ownership: "<<checks<<" checks / "<<refusals<<" refusal prefixes\n";return 0;
}catch(const std::exception& error){ShutdownPortableRuntime();std::cerr<<"FAIL deferred ownership: "<<error.what()<<'\n';return 1;}}
