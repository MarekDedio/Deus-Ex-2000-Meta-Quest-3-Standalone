#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "portable_retail_collision.h"
#include "portable_flag_base.h"
#include "portable_window_key_state.h"
#include "quest_save_bundle.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <tuple>

namespace {
namespace fs=std::filesystem;
using Value=QuestVr::Vm::Value;using Kind=QuestVr::Vm::Kind;
using Mode=PortableWindowKeyStateProbeMode;
std::size_t checks{},refusals{};
void Require(bool good,const std::string& why) {++checks;if(!good)throw std::runtime_error(why);}
QuestVr::Vm::Limits Limits() {
    QuestVr::Vm::Limits limits;limits.nativeWork=QuestVr::NativeWork::MaximumLimit;return limits;
}
struct Fingerprint {fs::path path;std::uint64_t bytes{},hash{14695981039346656037ull};};
Fingerprint Source(const fs::path& path) {
    Fingerprint result;result.path=path;std::ifstream file(path,std::ios::binary);
    Require(bool(file),"Cannot fingerprint original source: "+path.string());std::array<char,65536u> data{};
    while(file) {
        file.read(data.data(),static_cast<std::streamsize>(data.size()));const auto count=file.gcount();
        for(std::streamsize i=0;i<count;++i) {
            result.hash^=static_cast<unsigned char>(data[static_cast<std::size_t>(i)]);result.hash*=1099511628211ull;
        }
        result.bytes+=static_cast<std::uint64_t>(count);
    }
    Require(file.eof()&&!file.bad(),"Cannot finish original source fingerprint");return result;
}
struct Temporary {
    fs::path parent=fs::canonical(fs::temp_directory_path()),path;
    Temporary() {
        path=parent/("quest-window-key-state-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(fs::create_directory(path),"Cannot create isolated key-state checkpoint directory");path=fs::canonical(path);
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code error;const auto actual=fs::weakly_canonical(path,error);
        if(!error&&actual==path&&actual.parent_path()==parent&&actual.filename().string().rfind("quest-window-key-state-",0u)==0u)
            fs::remove_all(actual,error);
    }
    std::vector<std::uint8_t> Save(const char* name) const {
        const auto target=path/name;Require(SavePortableRuntimeState(target.string()),"Cannot save key-state checkpoint");
        std::vector<std::uint8_t> bytes;
        Require(QuestVr::ReadBoundedSaveFile(target.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read key-state checkpoint");
        return bytes;
    }
};
void Observations(const PortableWindowKeyStateProbeResult& result) {
    Require(result.completed&&!result.playerPath.empty()&&!result.rootPath.empty()&&!result.childPath.empty()&&result.rootPath!=result.childPath,
        "Key-state fixture did not observe distinct actual configured Root/child identities");
    Require(result.queries==1042u&&result.key255False&&result.terminalNotNearest,
        "Key-state query count/range/live-terminal acceptance differs");
    for(std::size_t key=0u;key<255u;++key)
        Require(!result.cleared[key]&&result.written[key]==(key%3u!=0u),"Actual Root clear/nonzero-byte result differs");
    constexpr std::array<std::uint8_t,7u> keys{0u,1u,16u,17u,18u,128u,254u};
    for(std::size_t slot=0u;slot<keys.size();++slot)
        Require(result.changed[slot]!=result.written[keys[slot]],"Key-state read cached a previous live Root byte");
    Require(result.execution.writes==result.writesBeforeQueries+264u,
        "Key-state queries wrote storage or actual fixture writes were skipped");
}
}
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        if(argc==1) {std::cout<<"SKIP owned Window key-state: supply original GOTY root\n";return 77;}
        if(argc!=2)throw std::runtime_error("Usage: portable_window_key_state_test GAME_ROOT");
        const fs::path root=fs::canonical(argv[1]);Temporary temporary;ShutdownPortableRuntime();GC::Collect();
        const auto initial=GC::GetStats();std::vector<Fingerprint> sources;std::vector<PortablePackageTables> packages;
        for(const auto* name:{"ConSys","Core","DeusEx","DeusExCharacters","DeusExConAudioAIBarks",
            "DeusExConAudioEndGame","DeusExConAudioHK_Shared","DeusExConAudioIntro","DeusExConAudioMission00",
            "DeusExConAudioMission01","DeusExConAudioMission02","DeusExConAudioMission03","DeusExConAudioMission04",
            "DeusExConAudioMission05","DeusExConAudioMission08","DeusExConAudioMission09",
            "DeusExConAudioMission10","DeusExConAudioMission11","DeusExConAudioMission12","DeusExConAudioMission14",
            "DeusExConAudioMission15","DeusExConAudioNYShared","DeusExConText","DeusExConversations","DeusExDeco",
            "DeusExItems","DeusExSounds","DeusExText","DeusExUI","Editor","Engine","Extension","Fire","IpDrv",
            "IpServer","MPCharacters","UBrowser","UWindow"}) {
            const auto path=root/"System"/(std::string(name)+".u");sources.push_back(Source(path));
            packages.push_back(LoadPortablePackageTables(path.string()));
        }
        const auto mapPath=root/"Maps"/"00_Training.dx";const auto ini=root/"System"/"DeusEx.ini";
        sources.push_back(Source(mapPath));sources.push_back(Source(ini));
        Require(InitializePortableRuntime(packages).passed,"Cannot initialize owned Window key-state runtime");
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision(ini.string()).prepared&&
            PreparePortableRuntimeFlagBaseClasses().prepared,
            "Window key-state fixture lacks original native metadata/config/lookup/collision authority");
        const auto map=LoadPortablePackageTables(mapPath.string());Require(LoadPortableRuntimeMap(map).passed,"Cannot load first Training");
        const auto order=ReadPortableLevel68ActorOrder(map);Require(!order.empty()&&order.front()>0,"Training lacks slot-zero LevelInfo");
        const auto level=std::string("00_Training.")+GetPortableObjectPath(map,order.front());
        Require(!ReadPortableActorDeclaredScriptProperty(level,"Engine.LevelInfo.bBegunPlay").boolean,
            "Window key-state fixture unexpectedly began gameplay");
        const auto before=temporary.Save("before.sav");const auto gc=GC::GetStats();
        const auto windows=ReadPortableRuntimeNativeWindowSummary();const auto lookup=ReadPortableRuntimeObjectLookup();
        const auto revision=GetPortableRuntimeWorldRevision();const auto actorOrder=ReadPortableWorldActorOrder();
        const auto actorCount=GetPortableRuntimeMapActors(true).size();const auto hash=ReadPortableRuntimeRetailCollision();
        Require(!windows.objects&&!windows.roots,"Window key-state fixture started with a Root graph");
        const auto unchanged=[&] {
            const auto now=GC::GetStats();const auto win=ReadPortableRuntimeNativeWindowSummary();
            const auto index=ReadPortableRuntimeObjectLookup();const auto collision=ReadPortableRuntimeRetailCollision();
            Require(!win.objects&&!win.roots&&win.configuredRootClass==windows.configuredRootClass&&
                win.configuredRootNameCounter==windows.configuredRootNameCounter&&
                win.graphicsContextNameCounter==windows.graphicsContextNameCounter&&ReadPortableRuntimeFlagBases().empty(),
                "Key-state rollback retained actual Window/Root/Pawn identity");
            Require(now.numObjects==gc.numObjects&&now.memoryUsage==gc.memoryUsage&&
                std::tie(index.objects,index.retainedBytes,index.generation)==std::tie(lookup.objects,lookup.retainedBytes,lookup.generation)&&
                collision.hashIdentity==hash.hashIdentity&&collision.tag==hash.tag&&collision.seed==hash.seed&&
                collision.activeLinks==hash.activeLinks&&collision.allocatedLinks==hash.allocatedLinks&&
                GetPortableRuntimeWorldRevision()==revision&&ReadPortableWorldActorOrder()==actorOrder&&
                GetPortableRuntimeMapActors(true).size()==actorCount&&temporary.Save("after.sav")==before,
                "Key-state rollback changed save/GC/lookup/collision/world authority");
        };
        const auto run=[&](Mode mode,const QuestVr::Vm::Limits& limits=Limits()) {
            auto result=ProbePortableNativeWindowKeyState(level,mode,limits);
            Require(!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
                "Rollback-only key-state diagnostic unexpectedly committed");unchanged();
            if(!result.playerPath.empty())Require(!FindPortableRuntimeObject({result.playerPath}),
                "Rollback left the actual newly constructed player indexed");
            return result;
        };
        const auto first=run(Mode::RoundTrip);
        std::cout<<"OBSERVED original key-state first fixture writes="<<first.execution.writes
            <<" beforeQueries="<<first.writesBeforeQueries<<" queries="<<first.queries<<'\n';
        Require(first.execution.error=="Window key-state diagnostic complete; rollback required",
            "Actual key-state integration stopped before completion: "+first.execution.error);Observations(first);
        const auto again=run(Mode::RoundTrip);Observations(again);
        Require(first.playerPath==again.playerPath&&first.rootPath==again.rootPath&&first.childPath==again.childPath&&
            first.execution.writes==again.execution.writes,
            "Window key-state rollback did not restore original allocation/counter identity");
        const auto late=run(Mode::LateFailure);Observations(late);++refusals;
        Require(late.execution.error=="Window key-state late failure after actual Root queries","Late key-state failure occurred too early");
        const auto rootless=run(Mode::Rootless);
        Require(rootless.completed&&rootless.queries==8u&&rootless.key255False&&
            rootless.execution.error=="Window key-state rootless diagnostic complete; rollback required",
            "Genuine rootless base Window/child did not return false");
        for(const auto mode:{Mode::WrongReceiver,Mode::WrongArgument,Mode::MalformedByte,Mode::MissingArgument,Mode::WrongDeclaration,
            Mode::ForeignEnumField,Mode::ParentCycle,Mode::InvalidTerminalCast}) {
            const auto failed=run(mode);Require(!failed.completed,"Invalid native key-state authority succeeded");++refusals;
        }
        for(const auto cut:{std::size_t{0u},std::size_t{1u},first.writesBeforeQueries,first.execution.writes-1u}) {
            auto limits=Limits();limits.writes=cut;const auto partial=run(Mode::RoundTrip,limits);
            Require(partial.execution.status==QuestVr::Vm::Status::Budget&&partial.execution.writes==cut,
                "Key-state construction/live-byte writes escaped the shared write budget");++refusals;
        }
        Require(first.execution.nativeWork&&first.execution.nativeWork->used,"Key-state integration did not observe native work");
        for(const auto cut:{std::size_t{0u},first.execution.nativeWork->used/2u,first.execution.nativeWork->used-1u}) {
            auto limits=Limits();limits.nativeWork=cut;const auto partial=run(Mode::RoundTrip,limits);
            Require(partial.execution.nativeWork&&partial.execution.nativeWork->exhausted,
                "Key-state queries escaped the caller's shared native-work budget");++refusals;
        }
        for(const auto& source:sources) {
            const auto after=Source(source.path);Require(source.bytes==after.bytes&&source.hash==after.hash,"Owned original source changed");
        }
        std::cout<<"Observed original key-state fixture writes="<<first.execution.writes
            <<" beforeQueries="<<first.writesBeforeQueries<<" queries="<<first.queries
            <<" nativeWork="<<first.execution.nativeWork->used<<'\n';
        ShutdownPortableRuntime();GC::Collect();const auto final=GC::GetStats();
        Require(final.numObjects==initial.numObjects&&final.memoryUsage==initial.memoryUsage,"Window key-state shutdown leaked GC state");
        std::cout<<"PASS owned first-Training Window.IsKeyDown native1470 checks="<<checks<<" refusals="<<refusals
            <<"; configured original Root/child, all255 live key bytes, nonzero values, range/None/type, schema ingress, cycles, budgets and full rollback. NOT Quest input or playable startup\n";
        return 0;
    } catch(const std::exception& error) {
        ShutdownPortableRuntime();std::cerr<<"FAIL owned Window key-state: "<<error.what()<<'\n';return 1;
    }
}
