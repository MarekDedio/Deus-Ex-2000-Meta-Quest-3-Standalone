#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_engine_collision.h"
#include "portable_object_lookup.h"
#include "portable_model_geometry.h"
#include "quest_save_bundle.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using QuestVr::Vm::Value;using QuestVr::Vm::Kind;
std::size_t checks{},refusals{};
void Require(bool condition,const std::string& why) {++checks;if(!condition)throw std::runtime_error(why);}
std::vector<std::uint8_t> Bytes(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);Require(size<=64u<<20u,"GC checkpoint exceeds independent bound");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));std::ifstream input(path,std::ios::binary);
    Require(static_cast<bool>(input),"Cannot open GC checkpoint");input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    Require(static_cast<bool>(input),"GC checkpoint truncated");return bytes;
}
}
int main(int argc,char** argv) {
    std::filesystem::path directory;bool owned{};
    try {
        std::filesystem::path root;
        if(argc==3 && std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc!=1)throw std::runtime_error("Usage: portable_gc_context_test [--game-root OWNED_GAME_ROOT]");
        if(root.empty())if(const auto* configured=std::getenv("DEUSEX_GAME_ROOT"))root=configured;
        if(root.empty()){std::cout<<"SKIP native GC owned integration: supply --game-root or DEUSEX_GAME_ROOT\n";return 77;}
        std::vector<PortablePackageTables> sources;
        for(const auto* name:{"ConSys","Core","DeusEx","DeusExCharacters","DeusExConAudioAIBarks",
            "DeusExConAudioEndGame","DeusExConAudioHK_Shared","DeusExConAudioIntro","DeusExConAudioMission00",
            "DeusExConAudioMission01","DeusExConAudioMission02","DeusExConAudioMission03","DeusExConAudioMission04",
            "DeusExConAudioMission05","DeusExConAudioMission08","DeusExConAudioMission09",
            "DeusExConAudioMission10","DeusExConAudioMission11","DeusExConAudioMission12","DeusExConAudioMission14",
            "DeusExConAudioMission15","DeusExConAudioNYShared","DeusExConText","DeusExConversations","DeusExDeco",
            "DeusExItems","DeusExSounds","DeusExText","DeusExUI","Editor","Engine","Extension","Fire","IpDrv",
            "IpServer","MPCharacters","UBrowser","UWindow"}) {
            sources.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
        }
        Require(InitializePortableRuntime(sources).passed,"Cannot initialize actual GC source graph");
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeObjectLookup().prepared&&
            PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared,"GC diagnostic services unavailable");
        const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
        Require(LoadPortableRuntimeMap(map).passed,"Cannot load original first Training");
        const auto actors=GetPortableRuntimeMapActors(true);
        const auto start=std::find_if(actors.begin(),actors.end(),[](const auto& actor){return actor.classPath=="Engine.PlayerStart";});
        Require(start!=actors.end()&&start->hasLocation,"GC diagnostic requires original PlayerStart");
        const auto order=ReadPortableLevel68ActorOrder(map);Require(!order.empty()&&order.front()>0,"No original Training LevelInfo");
        const auto level="00_Training."+GetPortableObjectPath(map,order.front());
        const auto birth=ExecutePortableActorFunction(level,"Engine.Actor.Spawn",{
            {Value::Text(Kind::Object,"DeusEx.JCDentonMale"),{}},{Value::Text(Kind::Object,{}),{}},{Value::Text(Kind::Name,{}),{}},
            {Value::Vector({start->x,start->y,start->z}),{}},{Value::Rotator({start->pitch,start->yaw,start->roll}),{}}});
        Require(birth.passed()&&birth.committed&&birth.value.kind==Kind::Object&&!birth.value.text.empty(),"Dormant diagnostic JC native birth failed");
        const auto player=birth.value.text;
        QuestVr::Vm::Limits probeLimits;probeLimits.nativeWork=QuestVr::NativeWork::MaximumLimit;
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        directory=std::filesystem::temp_directory_path()/("deusex-native-gc-"+std::to_string(stamp));
        owned=std::filesystem::create_directory(directory);Require(owned,"Cannot create GC diagnostic save directory");
        const auto baselinePath=directory/"baseline.sav",afterPath=directory/"after.sav";
        Require(SavePortableRuntimeState(baselinePath.string()),"Cannot save GC baseline");const auto baseline=Bytes(baselinePath);
        const auto baselineWindows=ReadPortableRuntimeNativeWindowSummary();const auto baselineGC=GC::GetStats();
        auto revision=GetPortableRuntimeWorldRevision();const auto actorCount=GetPortableRuntimeMapActors(true).size();
        Require(!baselineWindows.objects&&!baselineWindows.roots,"GC fixture started with existing native windows");
        const auto unchanged=[&] {
            const auto windows=ReadPortableRuntimeNativeWindowSummary();
            Require(windows.objects==baselineWindows.objects&&windows.roots==baselineWindows.roots&&
                windows.configuredRootNameCounter==baselineWindows.configuredRootNameCounter&&
                windows.graphicsContextNameCounter==baselineWindows.graphicsContextNameCounter,
                "GC refusal retained identities, native roots or unique-name counters");
            Require(GC::GetStats().numObjects==baselineGC.numObjects&&GetPortableRuntimeWorldRevision()==revision&&
                GetPortableRuntimeMapActors(true).size()==actorCount,"GC rollback changed runtime/Actor ownership or revision: gc="+
                std::to_string(GC::GetStats().numObjects)+"/"+std::to_string(baselineGC.numObjects)+" revision="+
                std::to_string(GetPortableRuntimeWorldRevision())+"/"+std::to_string(revision)+" actors="+
                std::to_string(GetPortableRuntimeMapActors(true).size())+"/"+std::to_string(actorCount));
            Require(SavePortableRuntimeState(afterPath.string())&&Bytes(afterPath)==baseline,"GC rollback changed actual save bytes");
        };
        const auto verify=[&](const PortableGcContextProbeResult& result) {
            Require(!result.execution.passed()&&!result.execution.committed&&
                result.execution.error=="Native GC context diagnostic complete; rollback required",
                "Native GC fixture failed before diagnostic completion: "+result.execution.error);
            Require(result.contextCases==262u&&result.rawCases==15u&&result.stackCases==9u&&result.windowCases==60u&&result.referenceCases==21u&&result.textCases==18u,
                "Native GC fixture omitted actual context/stack/reference controls");
            Require(result.reusedPushWrites==35u&&result.freshPushWrites>35u&&result.nestedObjects==5u&&
                !result.firstSavedPath.empty()&&result.firstSavedPath!=result.secondSavedPath,"Native GC saved-node identity/store counts differ");
        };
        const auto first=ProbePortableNativeGcContext(player,PortableGcContextProbeMode::RoundTrip,probeLimits);verify(first);unchanged();
        const auto second=ProbePortableNativeGcContext(player,PortableGcContextProbeMode::RoundTrip,probeLimits);verify(second);unchanged();
        Require(first.execution.nativeWork&&second.execution.nativeWork&&!first.execution.nativeWork->exhausted&&
            !second.execution.nativeWork->exhausted,"Native GC diagnostic lost its shared work ledger");
        std::cout<<"OWNED GC writes="<<first.execution.writes<<" nativeWork="<<first.execution.nativeWork->used
            <<" freshPush="<<first.freshPushWrites<<" reusedPush="<<first.reusedPushWrites<<" textCases="<<first.textCases<<'\n';
        Require(first.rootPath==second.rootPath&&first.firstSavedPath==second.firstSavedPath&&
            first.execution.writes==second.execution.writes,"Cold/warm GC rollback lost name or write determinism");
        for(const auto mode:{PortableGcContextProbeMode::NoneCopy,PortableGcContextProbeMode::WrongReceiver,
            PortableGcContextProbeMode::WrongArgument,PortableGcContextProbeMode::MissingArgument,
            PortableGcContextProbeMode::CyclicGraph,PortableGcContextProbeMode::NoWriteCopy,PortableGcContextProbeMode::LateFailure}) {
            const auto failed=ProbePortableNativeGcContext(player,mode,probeLimits);
            Require(!failed.execution.passed()&&!failed.execution.committed&&!failed.execution.error.empty()&&
                failed.execution.error!="Native GC context diagnostic guard unexpectedly accepted invalid input",
                "Invalid GC packet/graph was accepted");++refusals;unchanged();
        }
        for(const auto limit:{0u,1u,32u,512u,2048u}) {
            auto limits=probeLimits;limits.writes=limit;
            const auto result=ProbePortableNativeGcContext(player,PortableGcContextProbeMode::RoundTrip,limits);
            Require(!result.execution.passed()&&!result.execution.committed&&
                result.execution.error!="Native GC context diagnostic complete; rollback required","GC write cut did not stop original ingress");
            ++refusals;unchanged();
        }
        auto lateWrite=probeLimits;lateWrite.writes=first.execution.writes-1u;
        const auto late=ProbePortableNativeGcContext(player,PortableGcContextProbeMode::RoundTrip,lateWrite);
        Require(late.execution.status==QuestVr::Vm::Status::Budget&&!late.execution.committed&&
            late.execution.writes==lateWrite.writes,"Native GC late write cut escaped root budget/undo");++refusals;unchanged();
        for(const auto work:{std::size_t{0u},std::size_t{1u},first.execution.nativeWork->used-1u}) {
            auto limited=probeLimits;limited.nativeWork=work;
            const auto result=ProbePortableNativeGcContext(player,PortableGcContextProbeMode::RoundTrip,limited);
            Require(!result.execution.passed()&&!result.execution.committed&&result.execution.nativeWork&&
                result.execution.nativeWork->exhausted,"Native GC shared work cut was swallowed or reset");++refusals;unchanged();
        }
        Require(LoadPortableRuntimeState(baselinePath.string()),"Cannot reload dormant GC baseline");
        Require(GetPortableRuntimeWorldRevision()==revision+1u,"Explicit checkpoint load did not publish exactly one new world revision");
        revision=GetPortableRuntimeWorldRevision(); // The load, not the following diagnostic, changes revision.
        const auto restored=ProbePortableNativeGcContext(player,PortableGcContextProbeMode::RoundTrip,probeLimits);verify(restored);unchanged();
        ShutdownPortableRuntime();GC::Collect();std::filesystem::remove_all(directory);owned=false;
        std::cout<<"PASS original GC context integration: "<<checks<<" checks / "<<refusals
            <<" refusals; actual constructor-owned context copies/nested/reused nodes/reference edges/native wrappers, nonempty real-font sizing and whole-root save/GC/counter undo. "
            <<"Only first Training; not successful player startup, Canvas rendering or live VR crates.\n";return 0;
    }catch(const std::exception& error){ShutdownPortableRuntime();if(owned)std::filesystem::remove_all(directory);
        std::cerr<<"FAIL original GC context integration: "<<error.what()<<'\n';return 1;}
}
