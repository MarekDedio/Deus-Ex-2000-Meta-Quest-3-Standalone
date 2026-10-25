#include "Precomp.h"
#include "GC/GC.h"
#include "portable_engine_collision.h"
#include "portable_model_geometry.h"
#include "portable_object_lookup.h"
#include "portable_retail_collision.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Owned first-Training data, actual native base Root/Window constructors and
// original selected empty Window callback. Explicit journaled fixture settings;
// NOT a configured game HUD, generated script override or published UI graph.
namespace {
using Value=QuestVr::Vm::Value;using Kind=QuestVr::Vm::Kind;
using Mode=PortableWindowAxisProbeMode;using Probe=PortableWindowAxisProbeResult;
using Stage=PortableWindowAxisProbeStage;
std::size_t checks{},refusals{};
void Require(bool good,const std::string& why) {++checks;if(!good)throw std::runtime_error(why);}
bool Same(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
struct Fingerprint {
    std::filesystem::path path;std::uint64_t bytes{},hash{14695981039346656037ull};
};
Fingerprint Source(const std::filesystem::path& path) {
    Fingerprint result;result.path=path;std::ifstream input(path,std::ios::binary);
    Require(bool(input),"Cannot open owned source for read-only fingerprint: "+path.string());
    std::array<char,65536u> block{};
    while(input) {
        input.read(block.data(),static_cast<std::streamsize>(block.size()));const auto count=input.gcount();
        for(std::streamsize i=0;i<count;++i) {
            result.hash^=static_cast<unsigned char>(block[static_cast<std::size_t>(i)]);
            result.hash*=1099511628211ull;
        }
        result.bytes+=static_cast<std::uint64_t>(count);
    }
    Require(input.eof()&&!input.bad(),"Cannot finish owned source fingerprint");return result;
}
struct Temporary {
    std::filesystem::path parent,path;
    Temporary() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-window-axis-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) {path=std::filesystem::canonical(candidate);break;}
        }
        Require(!path.empty()&&path.parent_path()==parent,"Cannot create private preferred-axis checkpoint directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code error;
        const auto actual=std::filesystem::weakly_canonical(path,error);
        if(!error&&!path.empty()&&actual==path&&actual.parent_path()==parent&&
            actual.filename().string().rfind("quest-window-axis-",0u)==0u)
            std::filesystem::remove_all(actual,error);
    }
    std::vector<std::uint8_t> Save(const char* leaf) const {
        const auto target=path/leaf;Require(SavePortableRuntimeState(target.string()),"Cannot save preferred-axis checkpoint");
        std::vector<std::uint8_t> bytes;
        Require(QuestVr::ReadBoundedSaveFile(target.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),
            "Cannot read preferred-axis checkpoint");return bytes;
    }
};
void Query(const Stage& stage,std::uint16_t index,float input,float result,
    float width,float height,bool widthSpecified,bool heightSpecified) {
    Require(stage.nativeIndex==index&&Same(stage.input,input)&&Same(stage.result,result)&&
        Same(stage.lastSpecifiedWidth,width)&&Same(stage.lastSpecifiedHeight,height)&&
        Same(stage.lastQueryWidth,width)&&Same(stage.lastQueryHeight,height)&&
        stage.lastWidthSpecified==widthSpecified&&stage.lastHeightSpecified==heightSpecified&&!stage.needsQuery,
        "Preferred-axis actual result/cache/flags differ at "+stage.name);
}
void Stages(const Probe& probe) {
    const std::array<const char*,14u> names{"width","width-cache","height","height-cache",
        "width-negative","height-negative","width-minuszero","width-pluszero-cache",
        "height-minuszero","height-pluszero-cache","width-nowrite","height-zerofill",
        "width-promoted","height-promoted"};
    Require(!probe.rootPath.empty()&&!probe.windowPath.empty()&&probe.rootPath!=probe.windowPath&&
        probe.stages.size()==names.size(),"Preferred-axis fixture lost its distinct native identities or stages");
    for(std::size_t i=0u;i<names.size();++i)Require(probe.stages[i].name==names[i],"Preferred-axis stage order changed");
    const auto& s=probe.stages;
    Query(s[0],1463u,21,37,-1,21,false,true);Query(s[1],1463u,21,37,-1,21,false,true);
    Query(s[2],1464u,31,53,31,-1,true,false);Query(s[3],1464u,31,53,31,-1,true,false);
    Query(s[4],1463u,-7,37,-1,-7,false,true);Query(s[5],1464u,-9,53,-9,-1,true,false);
    Query(s[6],1463u,-0.0f,37,-1,-0.0f,false,true);
    Query(s[7],1463u,0.0f,37,-1,-0.0f,false,true);
    Query(s[8],1464u,-0.0f,53,-0.0f,-1,true,false);
    Query(s[9],1464u,0.0f,53,-0.0f,-1,true,false);
    Query(s[10],1463u,0.0f,37,-1,0.0f,false,true);
    Query(s[11],1464u,0.0f,53,0.0f,-1,true,false);
    // The explicit hardcoded mirror promotes the missing dimension. Both
    // specified values return immediately, including the negative supplied
    // constraint, WITHOUT updating the preceding query's cache or callback.
    Query(s[12],1463u,-17,44,0.0f,-1,true,false);
    Query(s[13],1464u,-19,55,0.0f,-1,true,false);
    for(const auto i:{1u,3u,7u,9u})
        Require(s[i].writes==s[i-1u].writes,"Preferred-axis cache hit performed script/native stores");
    for(const auto i:{2u,4u,5u,6u,8u,10u,11u})
        Require(s[i].writes==s[i-1u].writes+9u,
            "Preferred-axis miss lost five pre-cache/two real script OUT-copy/two post-cache stores");
    Require(s[12].writes==s[11].writes+2u&&s[13].writes==s[12].writes,
        "Both-specified query touched cache/callback after two explicit fixture hardcoded stores");
}
} // namespace
int main(int argc,char** argv) {
    try {
        if(argc==1) {std::cout<<"SKIP owned preferred-axis controls: supply Deus Ex GOTY root\n";return 77;}
        if(argc!=2)throw std::runtime_error("Usage: portable_window_axis_query_test GAME_ROOT");
        const auto root=std::filesystem::canonical(argv[1]);Temporary temporary;
        std::vector<Fingerprint> sources;std::vector<PortablePackageTables> scripts;
        for(const auto* name:{"ConSys","Core","DeusEx","DeusExCharacters","DeusExConAudioAIBarks",
            "DeusExConAudioEndGame","DeusExConAudioHK_Shared","DeusExConAudioIntro",
            "DeusExConAudioMission00","DeusExConAudioMission01","DeusExConAudioMission02",
            "DeusExConAudioMission03","DeusExConAudioMission04","DeusExConAudioMission05",
            "DeusExConAudioMission08","DeusExConAudioMission09",
            "DeusExConAudioMission10","DeusExConAudioMission11","DeusExConAudioMission12",
            "DeusExConAudioMission14","DeusExConAudioMission15","DeusExConAudioNYShared",
            "DeusExConText","DeusExConversations","DeusExDeco","DeusExItems","DeusExSounds",
            "DeusExText","DeusExUI","Editor","Engine","Extension","Fire","IpDrv","IpServer",
            "MPCharacters","UBrowser","UWindow"}) {
            const auto source=root/"System"/(std::string(name)+".u");sources.push_back(Source(source));
            scripts.push_back(LoadPortablePackageTables(source.string()));
        }
        sources.push_back(Source(root/"System"/"DeusEx.ini"));sources.push_back(Source(root/"Maps"/"00_Training.dx"));
        Require(InitializePortableRuntime(scripts).passed,"Cannot initialize owned preferred-axis runtime");
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeObjectLookup().prepared&&
            PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared,
            "Preferred-axis fixture lacks genuine native metadata/config/lookup/collision services");
        const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
        Require(LoadPortableRuntimeMap(map).passed,"Cannot load first Training for preferred-axis controls");
        const auto order=ReadPortableLevel68ActorOrder(map);
        Require(!order.empty()&&order.front()>0,"Training lacks its actual slot-zero LevelInfo");
        const auto level=std::filesystem::path(map.sourcePath).stem().string()+'.'+GetPortableObjectPath(map,order.front());
        const auto actors=GetPortableRuntimeMapActors(true);
        const auto start=std::find_if(actors.begin(),actors.end(),[](const auto& actor) {return actor.classPath=="Engine.PlayerStart";});
        Require(start!=actors.end()&&start->hasLocation,"Training lacks an authored PlayerStart");
        const auto born=ExecutePortableActorFunction(level,"Engine.Actor.Spawn",{
            {Value::Text(Kind::Object,"DeusEx.JCDentonMale"),{}},{Value::Text(Kind::Object,{}),{}},
            {Value::Text(Kind::Name,{}),{}},{Value::Vector({start->x,start->y,start->z}),{}},
            {Value::Rotator({start->pitch,start->yaw,start->roll}),{}}});
        Require(born.passed()&&born.committed&&born.value.kind==Kind::Object&&!born.value.text.empty(),
            "Dormant Training native player birth failed: "+born.error);
        const auto player=born.value.text;
        Require(!ReadPortableActorDeclaredScriptProperty(level,"Engine.LevelInfo.bBegunPlay").boolean,
            "Isolated preferred-axis fixture unexpectedly began the world");
        const auto beforeWindow=ReadPortableRuntimeNativeWindowSummary();
        Require(!beforeWindow.objects&&!beforeWindow.roots,"Preferred-axis fixture began with a native window graph");
        const auto beforeHash=ReadPortableRuntimeRetailCollision();const auto beforeOrder=ReadPortableWorldActorOrder();
        const auto beforeGc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
        const auto model=ReadPortableRuntimeNativeModel();const auto count=GetPortableRuntimeMapActors(true).size();
        const auto baseline=temporary.Save("baseline.sav");
        const auto unchanged=[&] {
            const auto after=ReadPortableRuntimeNativeWindowSummary();const auto gc=GC::GetStats();
            const auto currentModel=ReadPortableRuntimeNativeModel();
            const auto hash=ReadPortableRuntimeRetailCollision();
            Require(!after.objects&&!after.roots&&after.configuredRootClass==beforeWindow.configuredRootClass&&
                after.configuredRootNameCounter==beforeWindow.configuredRootNameCounter&&
                after.graphicsContextNameCounter==beforeWindow.graphicsContextNameCounter&&
                ReadPortableActorDeclaredScriptProperty(player,"Extension.PlayerPawnExt.RootWindow").text.empty()&&
                gc.numObjects==beforeGc.numObjects&&gc.memoryUsage==beforeGc.memoryUsage&&
                GetPortableRuntimeWorldRevision()==revision&&GetPortableRuntimeMapActors(true).size()==count&&
                ReadPortableWorldActorOrder()==beforeOrder&&hash.initialized==beforeHash.initialized&&
                hash.enabled==beforeHash.enabled&&hash.hashIdentity==beforeHash.hashIdentity&&
                hash.mapGeneration==beforeHash.mapGeneration&&hash.levelPath==beforeHash.levelPath&&
                hash.tag==beforeHash.tag&&hash.seed==beforeHash.seed&&hash.retainedBytes==beforeHash.retainedBytes&&
                hash.hashes==beforeHash.hashes&&hash.allocatedLinks==beforeHash.allocatedLinks&&
                hash.activeLinks==beforeHash.activeLinks&&hash.actors.size()==beforeHash.actors.size()&&
                currentModel.mapGeneration==model.mapGeneration&&currentModel.modelPath==model.modelPath&&
                temporary.Save("after.sav")==baseline,
                "Preferred-axis failure retained Window roots/names/GC/player/model/actor/save state");
        };
        const auto run=[&](Mode mode,const QuestVr::Vm::Limits& limits=QuestVr::Vm::Limits{}) {
            auto result=ProbePortableNativeWindowAxisQuery(player,mode,limits);
            Require(!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
                "Preferred-axis isolated diagnostic unexpectedly committed");
            unchanged();++refusals;return result;
        };
        const auto complete=run(Mode::RoundTrip);
        Require(complete.execution.error=="Native preferred-axis diagnostic complete; rollback required"&&
            complete.execution.nativeWork&&complete.execution.nativeWork->used&&!complete.execution.nativeWork->exhausted,
            "Preferred-axis fixture failed before completion: "+complete.execution.error);
        Stages(complete);
        const auto repeated=run(Mode::RoundTrip);Stages(repeated);
        Require(repeated.rootPath==complete.rootPath&&repeated.windowPath==complete.windowPath&&
            repeated.execution.writes==complete.execution.writes&&repeated.execution.instructions==complete.execution.instructions,
            "Preferred-axis rollback changed identity allocation or deterministic work");
        const auto late=run(Mode::LateFailure);Stages(late);
        Require(late.execution.error=="Native preferred-axis diagnostic late failure after valid queries"&&
            late.execution.writes==complete.execution.writes,"Late axis failure did not follow all real queries");
        for(const auto mode:{Mode::WrongReceiver,Mode::WrongArgument,Mode::MissingArgument}) {
            const auto invalid=run(mode);
            Require(invalid.stages.empty()&&invalid.execution.error!="Native preferred-axis diagnostic complete; rollback required",
                "Preferred-axis malformed receiver/required Float ingress was accepted");
        }
        for(const auto cut:{std::size_t{0u},std::size_t{1u},complete.stages[0u].writes-1u,
            complete.stages[4u].writes+1u,complete.stages[11u].writes-1u,complete.execution.writes-1u}) {
            QuestVr::Vm::Limits limits;limits.writes=cut;const auto partial=run(Mode::RoundTrip,limits);
            Require(partial.execution.status==QuestVr::Vm::Status::Budget&&partial.execution.writes==cut,
                "Preferred-axis construction/cache/late stores reset caller write budget: "+partial.execution.error);
        }
        for(const auto cut:{std::size_t{0u},complete.execution.nativeWork->used/2u,complete.execution.nativeWork->used-1u}) {
            QuestVr::Vm::Limits limits;limits.nativeWork=cut;const auto partial=run(Mode::RoundTrip,limits);
            Require(partial.execution.nativeWork&&partial.execution.nativeWork->exhausted,
                "Preferred-axis constructor/query/callback ignored shared native-work budget");
        }
        for(const auto& before:sources) {
            const auto after=Source(before.path);Require(before.bytes==after.bytes&&before.hash==after.hash,
                "Preferred-axis controls changed an owned package/map/INI source");
        }
        std::cout<<"PASS owned first-Training preferred-axis native1463/1464: "<<checks<<" checks / "<<refusals
            <<" rollback controls; actual native Root/Window and original empty callback, scalar copied Float0 defaults, "
            "cache hits/misses, negative/signed-zero constraints, promoted early return, shared write/work and late undo. "
            "Isolated fixture only; configured HUD recursion/startup/Quest remain separate proofs\n";
        return 0;
    }catch(const std::exception& error) {
        ShutdownPortableRuntime();std::cerr<<"FAIL owned preferred-axis fixture: "<<error.what()<<'\n';return 1;
    }
}
