#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "portable_window_navigation.h"
#include "quest_save_bundle.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <tuple>

namespace {
namespace fs=std::filesystem;std::size_t checks{},refusals{};
using Mode=PortableWindowNavigationProbeMode;
void Require(bool good,const std::string& why){++checks;if(!good)throw std::runtime_error(why);}
QuestVr::Vm::Limits Limits(){QuestVr::Vm::Limits limits;limits.nativeWork=QuestVr::NativeWork::MaximumLimit;return limits;}
struct Files {
    fs::path parent=fs::canonical(fs::temp_directory_path()),directory;
    Files(){directory=parent/("quest-window-navigation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(fs::create_directory(directory),"Cannot create Window navigation checkpoint directory");directory=fs::canonical(directory);}
    ~Files(){std::error_code error;const auto actual=fs::weakly_canonical(directory,error);
        if(!error&&actual==directory&&actual.parent_path()==parent&&actual.filename().string().rfind("quest-window-navigation-",0u)==0u)
            fs::remove_all(actual,error);}
    std::vector<std::uint8_t> Save(const char* name)const{
        const auto path=directory/name;Require(SavePortableRuntimeState(path.string()),"Cannot capture Window navigation checkpoint");
        std::vector<std::uint8_t> bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),
            "Cannot read Window navigation checkpoint");return bytes;
    }
};
void Cases(const Files& files){
    const auto initial=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto revision=GetPortableRuntimeWorldRevision();const auto before=files.Save("Before.sav");
    const auto order=ReadPortableWorldActorOrder();const auto actors=GetPortableRuntimeMapActors(true).size();
    const auto unchanged=[&]{
        const auto now=GC::GetStats();const auto current=ReadPortableRuntimeObjectLookup();
        const auto windows=ReadPortableRuntimeNativeWindowSummary();
        Require(!windows.objects&&!windows.roots,"Window navigation rollback retained native UI objects/roots");
        Require(now.numObjects==initial.numObjects&&now.memoryUsage==initial.memoryUsage,"Window navigation rollback leaked GC ownership");
        Require(std::tie(current.objects,current.retainedBytes,current.generation)==std::tie(lookup.objects,lookup.retainedBytes,lookup.generation),
            "Window navigation rollback changed actual weak index/lookup");
        Require(GetPortableRuntimeWorldRevision()==revision&&ReadPortableWorldActorOrder()==order&&GetPortableRuntimeMapActors(true).size()==actors&&
            files.Save("After.sav")==before,"Window navigation changed world/header/counter/save state");
    };
    const auto run=[&](Mode mode,const QuestVr::Vm::Limits& limits){
        const auto result=ProbePortableNativeWindowNavigation("00_Training.LevelInfo0",mode,limits);unchanged();return result;
    };
    std::optional<PortableWindowNavigationProbeResult> first;
    for(unsigned repeat=0u;repeat<2u;++repeat){
        const auto probe=run(Mode::RoundTrip,Limits());
        Require(probe.completed&&!probe.execution.passed()&&probe.execution.error=="Window navigation diagnostic complete; rollback required",
            "Original Window navigation diagnostic did not complete: "+probe.execution.error);
        const auto& w=probe.windows;const auto& s=probe.selected;
        Require(probe.queryCount==18u&&probe.walkCount==2u,"Window navigation query/live-walk counts differ");
        Require(s[0]==w[3]&&s[1]==w[1]&&s[2]==w[3]&&s[3]==w[1],"Filtered queries lost actual top/bottom/sibling identity");
        Require(s[4]==w[3]&&s[5]==w[1]&&s[6]==w[2]&&s[7]==w[2],"Unfiltered queries wrongly skipped a local-hidden sibling");
        Require(s[8]==w[1]&&s[9]==w[1]&&s[10]==w[3],"Live hide/default/NoWriteTRUE/full-zeroFALSE input semantics differ");
        Require(s[11]==w[3]&&s[12]==w[1]&&s[13].empty()&&s[14].empty()&&s[15].empty()&&s[16].empty()&&s[17].empty(),
            "Live lower-sibling walk/hidden/empty endpoints did not return original pointers/None");
        Require(probe.liveWalk[0]==w[3]&&probe.liveWalk[1]==w[1],"Hide/clientNone/lower-sibling order changed");
        Require(probe.clients[0]=="00_Training.LevelInfo0"&&probe.clients[1]==w[0]&&probe.clients[2].empty()&&
            probe.clientRooted&&probe.clientCleared&&probe.topologyPreserved,"ClientObject store/GC edge/clear/topology differs");
        if(first)Require(first->windows==probe.windows&&first->selected==probe.selected&&first->clients==probe.clients,
            "Rollback did not restore Window class counters/object index/naming");else first=probe;
    }
    const auto late=run(Mode::LateFailure,Limits());
    Require(late.completed&&!late.execution.passed()&&late.execution.error=="Window navigation late failure after actual live walk",
        "Late failure did not occur after real native queries/client stores");++refusals;
    for(const auto mode:{Mode::WrongReceiver,Mode::WrongArgument,Mode::MissingClientArgument}){
        const auto refused=run(mode,Limits());Require(!refused.execution.passed()&&!refused.completed,"Window navigation invalid input succeeded");++refusals;
    }
    for(const auto cut:{std::size_t{0u},std::size_t{1u},first->execution.nativeWork->used/2u}){
        auto limits=Limits();limits.nativeWork=cut;const auto refused=run(Mode::RoundTrip,limits);
        Require(!refused.execution.passed()&&refused.execution.nativeWork&&refused.execution.nativeWork->exhausted,
            "Window navigation bypassed shared work budget");++refusals;
    }
    for(const auto cut:{std::size_t{0u},std::size_t{1u},first->execution.writes-1u}){
        auto limits=Limits();limits.writes=cut;const auto refused=run(Mode::RoundTrip,limits);
        Require(refused.execution.status==QuestVr::Vm::Status::Budget&&refused.execution.writes==cut,
            "Window navigation constructor/store bypassed caller write budget");++refusals;
    }
    auto cycleLimits=Limits();cycleLimits.nativeWork=first->execution.nativeWork->used+8192u;
    const auto cycle=run(Mode::CyclicHiddenChain,cycleLimits);
    Require(!cycle.completed&&!cycle.execution.passed()&&cycle.execution.nativeWork&&cycle.execution.nativeWork->exhausted,
        "Hidden sibling cycle fabricated None or escaped shared work refusal");++refusals;
    std::cout<<"Window navigation fixture work="<<first->execution.nativeWork->used<<" writes="<<first->execution.writes<<'\n';
}
}
int main(int argc,char** argv){std::cout.setf(std::ios::unitbuf);try{
    fs::path root;if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];else if(argc==1){if(const auto* value=std::getenv("DEUSEX_GAME_ROOT"))root=value;}
    else throw std::runtime_error("Usage: portable_window_navigation_test [--game-root OWNED_GAME_ROOT]");
    if(root.empty()){std::cout<<"SKIP owned Window navigation: supply --game-root or DEUSEX_GAME_ROOT\n";return 77;}
    ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();std::vector<PortablePackageTables> packages;
    for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"})
        packages.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
    const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize original Window navigation runtime");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&
        LoadPortableRuntimeMap(map).passed,"Cannot prepare original Window navigation native schema/map");
    Files files;Cases(files);ShutdownPortableRuntime();GC::Collect();const auto final=GC::GetStats();
    Require(initial.numObjects==final.numObjects&&initial.memoryUsage==final.memoryUsage,"Window navigation shutdown leaked ownership");
    std::cout<<"PASS owned original Window navigation checks="<<checks<<" refusals="<<refusals
        <<"; actual base Window constructors, native queries, local visibility, client GC links, live Hide walk, shared budgets and full rollback. NOT configured HUD/startup/Quest acceptance\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL owned Window navigation: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}}
