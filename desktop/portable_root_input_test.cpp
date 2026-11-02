#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "portable_retail_collision.h"
#include "portable_flag_base.h"
#include "portable_root_input.h"
#include "quest_save_bundle.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <tuple>

namespace {
namespace fs=std::filesystem;
using Mode=PortableRootInputProbeMode;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
QuestVr::Vm::Limits Limits(){QuestVr::Vm::Limits result;result.nativeWork=QuestVr::NativeWork::MaximumLimit;return result;}
struct Source {
    fs::path path;std::uintmax_t bytes{};fs::file_time_type modified;std::uint64_t hash{};
    static std::uint64_t Hash(const fs::path& path){std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot read original source");
        std::array<char,65536u> data{};std::uint64_t hash=14695981039346656037ull;
        while(file){file.read(data.data(),static_cast<std::streamsize>(data.size()));for(std::streamsize i=0;i<file.gcount();++i){
            hash^=static_cast<unsigned char>(data[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}}
        Require(file.eof()&&!file.bad(),"Cannot finish original fingerprint");return hash;}
    explicit Source(const fs::path& file):path(fs::canonical(file)),bytes(fs::file_size(path)),modified(fs::last_write_time(path)),hash(Hash(path)){}
    void Unchanged()const{Require(fs::file_size(path)==bytes&&fs::last_write_time(path)==modified&&Hash(path)==hash,"Original source changed");}
};
struct Temporary {
    fs::path parent=fs::canonical(fs::temp_directory_path()),path;
    Temporary(){path=parent/("quest-root-input-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(fs::create_directory(path),"Cannot create isolated Root input fixture");path=fs::canonical(path);}
    ~Temporary(){ShutdownPortableRuntime();std::error_code error;const auto actual=fs::weakly_canonical(path,error);
        if(!error&&actual==path&&actual.parent_path()==parent&&actual.filename().string().rfind("quest-root-input-",0u)==0u)
            fs::remove_all(actual,error);}
    std::vector<std::uint8_t> Save(const char* name)const{const auto target=path/name;Require(SavePortableRuntimeState(target.string()),"Cannot save complete Root input checkpoint");
        std::vector<std::uint8_t> bytes;Require(QuestVr::ReadBoundedSaveFile(target.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read Root input checkpoint");return bytes;}
};
void Positive(const PortableRootInputProbeResult& result){
    Require(result.completed&&!result.pawnPath.empty()&&!result.rootPath.empty()&&!result.physicalDelivery,
        "Actual Root input diagnostic did not complete or fabricated physical delivery");
    Require(result.events==7u&&result.declinedWithoutGrab&&result.consumedWithGrab&&result.duplicateReleaseSkipped&&
        result.keyStates==std::array<std::uint8_t,4u>{1u,0u,1u,0u},"Actual Root key/grab/duplicate state differs");
    Require(result.rawCallbacks>=3u&&result.virtualCallbacks>=2u&&
        result.lastRawFunction=="Extension.Window.RawKeyPressed"&&
        result.lastVirtualFunction=="DeusEx.DeusExRootWindow.VirtualKeyPressed",
        "Keyboard route did not invoke actual most-derived original Raw/Virtual scripts: "+result.lastRawFunction+" / "+result.lastVirtualFunction);
    Require(result.execution.writes>result.writesBeforeInput&&result.execution.nativeWork&&result.execution.nativeWork->used,
        "Root input diagnostic omitted actual VM writes/shared native work");
}
}
int main(int argc,char** argv){std::cout.setf(std::ios::unitbuf);try{
    if(argc==1){std::cout<<"SKIP owned Root input: supply original game root\n";return 77;}
    Require(argc==2,"Usage: portable_root_input_test GAME_ROOT");const auto root=fs::canonical(argv[1]);
    ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();Temporary temp;
    std::vector<PortablePackageTables> packages;std::vector<Source> sources;
    for(const auto* name:{"ConSys","Core","DeusEx","DeusExCharacters","DeusExConAudioAIBarks","DeusExConAudioEndGame",
        "DeusExConAudioHK_Shared","DeusExConAudioIntro","DeusExConAudioMission00","DeusExConAudioMission01","DeusExConAudioMission02",
        "DeusExConAudioMission03","DeusExConAudioMission04","DeusExConAudioMission05","DeusExConAudioMission08","DeusExConAudioMission09",
        "DeusExConAudioMission10","DeusExConAudioMission11","DeusExConAudioMission12","DeusExConAudioMission14","DeusExConAudioMission15",
        "DeusExConAudioNYShared","DeusExConText","DeusExConversations","DeusExDeco","DeusExItems","DeusExSounds","DeusExText","DeusExUI",
        "Editor","Engine","Extension","Fire","IpDrv","IpServer","MPCharacters","UBrowser","UWindow"}){
        sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));}
    sources.emplace_back(root/"Maps"/"00_Training.dx");sources.emplace_back(root/"System"/"DeusEx.ini");
    Require(InitializePortableRuntime(packages).passed&&PreparePortableRuntimeNativeEngineClasses().prepared&&
        PreparePortableRuntimeNativeMetadataClasses().prepared&&PreparePortableRuntimeNativeConfigClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared&&PreparePortableRuntimeObjectLookup().prepared&&
        PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&PreparePortableRuntimeFlagBaseClasses().prepared,
        "Cannot prepare actual owned Root input dependencies");
    const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());Require(LoadPortableRuntimeMap(map).passed,"Cannot load dormant first Training");
    const auto order=ReadPortableLevel68ActorOrder(map);Require(!order.empty()&&order.front()>0,"Training has no slot-zero LevelInfo");
    const auto level=std::string("00_Training.")+GetPortableObjectPath(map,order.front());
    const auto save=temp.Save("before.sav");const auto gc=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto windows=ReadPortableRuntimeNativeWindowSummary();const auto revision=GetPortableRuntimeWorldRevision();
    const auto actorOrder=ReadPortableWorldActorOrder();const auto collision=ReadPortableRuntimeRetailCollision();
    const auto unchanged=[&]{const auto now=GC::GetStats();const auto actual=ReadPortableRuntimeObjectLookup();
        const auto win=ReadPortableRuntimeNativeWindowSummary();const auto hash=ReadPortableRuntimeRetailCollision();
        Require(now.numObjects==gc.numObjects&&now.memoryUsage==gc.memoryUsage&&
            std::tie(actual.objects,actual.retainedBytes,actual.generation)==std::tie(lookup.objects,lookup.retainedBytes,lookup.generation)&&
            win.objects==windows.objects&&win.roots==windows.roots&&win.configuredRootNameCounter==windows.configuredRootNameCounter&&
            win.graphicsContextNameCounter==windows.graphicsContextNameCounter&&ReadPortableRuntimeFlagBases().empty()&&
            GetPortableRuntimeWorldRevision()==revision&&ReadPortableWorldActorOrder()==actorOrder&&hash.hashIdentity==collision.hashIdentity&&
            hash.seed==collision.seed&&hash.tag==collision.tag&&hash.activeLinks==collision.activeLinks&&hash.allocatedLinks==collision.allocatedLinks&&
            temp.Save("after.sav")==save,"Root input rollback changed complete save/GC/lookup/Actor/Root/FlagBase/collision state");};
    const auto run=[&](Mode mode,const QuestVr::Vm::Limits& limits=Limits()){
        auto result=ProbePortableRuntimeRootInput(level,mode,limits);unchanged();
        Require(!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),"Root input diagnostic committed");
        if(!result.pawnPath.empty())Require(!FindPortableRuntimeObject({result.pawnPath}),"Provisional player survived Root input rollback");
        return result;};
    const auto first=run(Mode::RoundTrip);
    std::cout<<"OBSERVED Root input writes="<<first.execution.writes<<" beforeInput="<<first.writesBeforeInput
        <<" raw="<<first.rawCallbacks<<" virtual="<<first.virtualCallbacks<<" error="<<first.execution.error<<'\n';
    Require(first.execution.error=="Original Root input diagnostic complete; rollback required","Root input stopped before completion: "+first.execution.error);Positive(first);
    const auto again=run(Mode::RoundTrip);Positive(again);Require(first.pawnPath==again.pawnPath&&first.rootPath==again.rootPath&&
        first.execution.writes==again.execution.writes,"Root input rollback changed constructor identities or write schedule");
    const auto late=run(Mode::LateFailure);Positive(late);Require(late.execution.error=="Original Root input late failure after actual callbacks","Late Root input fault occurred too early");++refusals;
    const auto focus=run(Mode::FocusChild);Positive(focus);Require(focus.rawCallbacks>first.rawCallbacks&&focus.virtualCallbacks>first.virtualCallbacks,
        "Actual FocusWindow child was skipped before propagation to Root");
    const auto destroyed=run(Mode::DestroyedRoot);Require(!destroyed.completed&&destroyed.rawCallbacks==1u&&!destroyed.virtualCallbacks&&
        destroyed.execution.error=="Native keyboard propagation reached a destroyed Window",
        "Root destruction guard did not run AFTER the real unhandled Raw callback");++refusals;
    const auto navigation=run(Mode::Navigation);Require(!navigation.completed&&navigation.execution.error.find("1442")!=std::string::npos,
        "Unimplemented real navigation became fake None or failed before its actual callback: "+navigation.execution.error);++refusals;
    const auto mouse=run(Mode::MouseGrab);Require(!mouse.completed&&mouse.mouseFireCleared&&
        mouse.execution.error=="Original Root mouse-button handler is not prepared","Mouse grab did not clear real Pawn fire bytes before the honest missing handler");++refusals;
    const auto invalid=run(Mode::InvalidKey);Require(!invalid.completed,"Invalid Root key succeeded");++refusals;
    for(const auto cut:{std::size_t{0u},first.writesBeforeInput,first.execution.writes-1u}){auto limits=Limits();limits.writes=cut;
        const auto result=run(Mode::RoundTrip,limits);Require(result.execution.status==QuestVr::Vm::Status::Budget,"Root input escaped shared write cutoff");++refusals;}
    for(const auto cut:{std::size_t{1u},first.execution.nativeWork->used-1u}){auto limits=Limits();limits.nativeWork=cut;
        const auto result=run(Mode::RoundTrip,limits);Require(result.execution.nativeWork&&result.execution.nativeWork->exhausted,"Root input escaped shared native-work cutoff");++refusals;}
    for(const auto& source:sources)source.Unchanged();
    ShutdownPortableRuntime();GC::Collect();const auto final=GC::GetStats();
    Require(final.numObjects==initial.numObjects&&final.memoryUsage==initial.memoryUsage,"Root input shutdown leaked native ownership");
    std::cout<<"PASS owned original Root input checks="<<checks<<" refusals="<<refusals
        <<"; actual configured Root/Pawn/FlagBase, most-derived scripts, key/grab/duplicate ordering and complete rollback. NOT physical Quest delivery, complete UI navigation/Init or playable startup\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL owned Root input: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}}
