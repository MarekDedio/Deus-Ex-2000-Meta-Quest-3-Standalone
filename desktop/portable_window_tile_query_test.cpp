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

// Actual owned Root/Tile/Window objects and native query dispatch. The root
// remains hidden and all fixture changes are rolled back; this does not stand
// in for original Training HUD recursion or an on-device player session.
namespace {
using Value=QuestVr::Vm::Value;using Kind=QuestVr::Vm::Kind;
using Mode=PortableWindowTileProbeMode;using Probe=PortableWindowTileProbeResult;
using Stage=PortableWindowTileProbeStage;using Child=PortableWindowTileProbeChild;
std::size_t checks{},refusals{};
void Require(bool good,const std::string& why) {++checks;if(!good)throw std::runtime_error(why);}
bool Same(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
struct Fingerprint {std::filesystem::path path;std::uint64_t bytes{},hash{14695981039346656037ull};};
Fingerprint Source(const std::filesystem::path& path) {
    Fingerprint result;result.path=path;std::ifstream input(path,std::ios::binary);
    Require(bool(input),"Cannot open owned Tile source for read-only fingerprint: "+path.string());
    std::array<char,65536u> block{};
    while(input) {
        input.read(block.data(),static_cast<std::streamsize>(block.size()));const auto count=input.gcount();
        for(std::streamsize i=0;i<count;++i) {
            result.hash^=static_cast<unsigned char>(block[static_cast<std::size_t>(i)]);
            result.hash*=1099511628211ull;
        }
        result.bytes+=static_cast<std::uint64_t>(count);
    }
    Require(input.eof()&&!input.bad(),"Cannot finish owned Tile source fingerprint");return result;
}
struct Temporary {
    std::filesystem::path parent,path;
    Temporary() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-window-tile-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) {path=std::filesystem::canonical(candidate);break;}
        }
        Require(!path.empty()&&path.parent_path()==parent,"Cannot create private Tile checkpoint directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code error;
        const auto actual=std::filesystem::weakly_canonical(path,error);
        if(!error&&!path.empty()&&actual==path&&actual.parent_path()==parent&&
            actual.filename().string().rfind("quest-window-tile-",0u)==0u)
            std::filesystem::remove_all(actual,error);
    }
    std::vector<std::uint8_t> Save(const char* leaf) const {
        const auto target=path/leaf;Require(SavePortableRuntimeState(target.string()),"Cannot save Tile checkpoint");
        std::vector<std::uint8_t> bytes;
        Require(QuestVr::ReadBoundedSaveFile(target.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),
            "Cannot read Tile checkpoint");return bytes;
    }
};
void Cache(const Stage& stage,float specifiedWidth,float specifiedHeight,float width,float height,
    bool widthSpecified,bool heightSpecified) {
    Require(!stage.needsQuery&&stage.lastWidthSpecified==widthSpecified&&stage.lastHeightSpecified==heightSpecified&&
        Same(stage.lastSpecifiedWidth,specifiedWidth)&&Same(stage.lastSpecifiedHeight,specifiedHeight)&&
        Same(stage.lastQueryWidth,width)&&Same(stage.lastQueryHeight,height),
        "Actual Tile parent cache differs at "+stage.name);
}
void Rows(const Stage& stage,std::initializer_list<PortableWindowTileProbeRow> expected) {
    Require(stage.rowCount==expected.size(),"Actual Tile native row count differs at "+stage.name);
    std::size_t i{};
    for(const auto& row:expected) {
        const auto& actual=stage.rows[i++];
        Require(actual.crossSize==row.crossSize&&actual.mainExtent==row.mainExtent&&actual.childCount==row.childCount,
            "Actual Tile native row words differ at "+stage.name);
    }
}
void Held(const Child& child,float x,float y,float width,float height,const std::string& where) {
    Require(Same(child.holdX,x)&&Same(child.holdY,y)&&Same(child.holdWidth,width)&&Same(child.holdHeight,height),
        "Actual Tile held geometry differs at "+where);
}
void Stages(const Probe& probe) {
    const std::array<const char*,8u> names{"all-hidden","horizontal","horizontal-cache","wrapped-height",
        "wrapped-placement","vertical-fill","vertical-reversed-placement","replacement-empty"};
    Require(!probe.rootPath.empty()&&!probe.tilePath.empty()&&probe.rootPath!=probe.tilePath&&
        probe.stages.size()==names.size(),"Tile fixture lost real Root/Tile identities or stages");
    for(std::size_t i=0u;i<probe.childPaths.size();++i) {
        Require(!probe.childPaths[i].empty()&&probe.childPaths[i]!=probe.rootPath&&probe.childPaths[i]!=probe.tilePath,
            "Tile fixture lost an actual child identity");
        for(std::size_t j=0u;j<i;++j)Require(probe.childPaths[i]!=probe.childPaths[j],"Tile fixture aliases distinct children");
    }
    for(std::size_t i=0u;i<names.size();++i) {
        const auto& stage=probe.stages[i];Require(stage.name==names[i],"Tile stage order changed");
        Require(stage.nativeIndex==(i==3u||i==5u?1464u:i==4u||i==6u?0u:1465u),
            "Tile fixture bypassed its actual query wrapper at "+stage.name);
        Require(Same(stage.children[2u].holdX,0.0f)&&Same(stage.children[2u].holdY,0.0f)&&
            Same(stage.children[2u].holdWidth,0.0f)&&Same(stage.children[2u].holdHeight,0.0f)&&
            !stage.children[2u].visible&&stage.children[2u].needsQuery,
            "Always-hidden child was queried or lost its four genuine +0 stores at "+stage.name);
    }
    const auto& s=probe.stages;
    Require(Same(s[0u].width,4.0f)&&Same(s[0u].height,6.0f),"Empty Tile preferred output lost margins");
    Cache(s[0u],-1,-1,4,6,false,false);Rows(s[0u],{});
    for(const auto& child:s[0u].children) {
        Held(child,0,0,0,0,s[0u].name);
        Require(!child.visible&&child.needsQuery,"All-hidden Tile dispatched a child query");
    }
    for(const auto i:{1u,2u}) {
        Require(Same(s[i].width,35.0f)&&Same(s[i].height,14.0f),"Horizontal Tile preferred output lost per-child truncation");
        Cache(s[i],-1,-1,35,14,false,false);Rows(s[i],{{8,31,2}});
        Held(s[i].children[0u],0,0,10.75f,4.25f,s[i].name);
        Held(s[i].children[1u],0,0,20.5f,8.75f,s[i].name);
        for(const auto childIndex:{0u,1u})
            Require(s[i].children[childIndex].visible&&!s[i].children[childIndex].needsQuery&&
                Same(s[i].children[childIndex].lastQueryWidth,-1.0f)&&
                Same(s[i].children[childIndex].lastQueryHeight,-1.0f),
                "Actual base Window callback/cache/fallback was replaced by Tile fixture dimensions");
    }
    Require(s[2u].writes==s[1u].writes+2u,"Tile cache hit reran native layout instead of only two1465 OUT stores");
    Require(Same(s[3u].width,20)&&Same(s[3u].height,20),"Constrained Tile preferred height did not wrap");
    Cache(s[3u],20,-1,20,20,true,false);Rows(s[3u],{{4,10,1},{8,20,1}});
    Require(Same(s[4u].width,20)&&Same(s[4u].height,30),"Both-specified Tile layout changed supplied outputs");
    Cache(s[4u],20,-1,20,20,true,false);Rows(s[4u],{{4,10,0},{8,20,0}});
    Held(s[4u].children[0u],2,3,10.75f,4.25f,s[4u].name);
    Held(s[4u].children[1u],2,9,20.5f,8.75f,s[4u].name);
    Require(Same(s[5u].width,15)&&Same(s[5u].height,19),"Vertical fixed-width fill preferred height differs");
    Cache(s[5u],15,-1,15,19,true,false);Rows(s[5u],{{11,13,2}});
    Held(s[5u].children[0u],2,3,11,4.25f,s[5u].name);
    Held(s[5u].children[1u],2,9,11,8.75f,s[5u].name);
    for(const auto childIndex:{0u,1u})
        Require(Same(s[5u].children[childIndex].lastQueryWidth,11.0f)&&
            Same(s[5u].children[childIndex].lastQueryHeight,-1.0f),
            "Fixed-width Tile did not execute genuine child preferred-height constraints");
    Require(Same(s[6u].width,15)&&Same(s[6u].height,30),"Reversed Tile placement changed supplied dimensions");
    Cache(s[6u],15,-1,15,19,true,false);Rows(s[6u],{{11,13,0}});
    Held(s[6u].children[0u],2,22.75f,11,4.25f,s[6u].name);
    Held(s[6u].children[1u],2,13,11,8.75f,s[6u].name);
    Require(Same(s[7u].width,4)&&Same(s[7u].height,6),"Replacing populated Tile rows retained stale content size");
    Cache(s[7u],-1,-1,4,6,false,false);Rows(s[7u],{});
    for(const auto& child:s[7u].children) {
        Held(child,0,0,0,0,s[7u].name);Require(!child.visible,"Replacement-empty Tile retained visible child");
    }
    for(std::size_t i=1u;i<s.size();++i)
        Require(s[i].writes>s[i-1u].writes,"Actual Tile stage lost shared write accounting");
}
} // namespace
int main(int argc,char** argv) {
    try {
        if(argc==1) {std::cout<<"SKIP owned native Tile controls: supply Deus Ex GOTY root\n";return 77;}
        if(argc!=2)throw std::runtime_error("Usage: portable_window_tile_query_test GAME_ROOT");
        const auto root=std::filesystem::canonical(argv[1]);Temporary temporary;
        std::vector<Fingerprint> sources;std::vector<PortablePackageTables> scripts;
        for(const auto* name:{"ConSys","Core","DeusEx","DeusExCharacters","DeusExConAudioAIBarks",
            "DeusExConAudioEndGame","DeusExConAudioHK_Shared","DeusExConAudioIntro",
            "DeusExConAudioMission00","DeusExConAudioMission01","DeusExConAudioMission02",
            "DeusExConAudioMission03","DeusExConAudioMission04","DeusExConAudioMission05",
            "DeusExConAudioMission08","DeusExConAudioMission09","DeusExConAudioMission10",
            "DeusExConAudioMission11","DeusExConAudioMission12","DeusExConAudioMission14",
            "DeusExConAudioMission15","DeusExConAudioNYShared","DeusExConText","DeusExConversations",
            "DeusExDeco","DeusExItems","DeusExSounds","DeusExText","DeusExUI","Editor","Engine",
            "Extension","Fire","IpDrv","IpServer","MPCharacters","UBrowser","UWindow"}) {
            const auto source=root/"System"/(std::string(name)+".u");sources.push_back(Source(source));
            scripts.push_back(LoadPortablePackageTables(source.string()));
        }
        sources.push_back(Source(root/"System"/"DeusEx.ini"));sources.push_back(Source(root/"Maps"/"00_Training.dx"));
        Require(InitializePortableRuntime(scripts).passed,"Cannot initialize owned native Tile runtime");
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeObjectLookup().prepared&&
            PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared,
            "Tile fixture lacks genuine native metadata/config/lookup/collision services");
        const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
        Require(LoadPortableRuntimeMap(map).passed,"Cannot load first Training for native Tile controls");
        const auto order=ReadPortableLevel68ActorOrder(map);
        Require(!order.empty()&&order.front()>0,"Training lacks its actual slot-zero LevelInfo");
        const auto level=std::filesystem::path(map.sourcePath).stem().string()+'.'+GetPortableObjectPath(map,order.front());
        const auto actors=GetPortableRuntimeMapActors(true);
        const auto start=std::find_if(actors.begin(),actors.end(),[](const auto& actor) {return actor.classPath=="Engine.PlayerStart";});
        Require(start!=actors.end()&&start->hasLocation,"Training lacks authored PlayerStart");
        const auto born=ExecutePortableActorFunction(level,"Engine.Actor.Spawn",{
            {Value::Text(Kind::Object,"DeusEx.JCDentonMale"),{}},{Value::Text(Kind::Object,{}),{}},
            {Value::Text(Kind::Name,{}),{}},{Value::Vector({start->x,start->y,start->z}),{}},
            {Value::Rotator({start->pitch,start->yaw,start->roll}),{}}});
        Require(born.passed()&&born.committed&&born.value.kind==Kind::Object&&!born.value.text.empty(),
            "Dormant Training native player birth failed: "+born.error);
        const auto player=born.value.text;
        Require(!ReadPortableActorDeclaredScriptProperty(level,"Engine.LevelInfo.bBegunPlay").boolean,
            "Isolated Tile fixture unexpectedly began world");
        const auto beforeWindow=ReadPortableRuntimeNativeWindowSummary();
        Require(!beforeWindow.objects&&!beforeWindow.roots,"Tile fixture began with native window graph");
        const auto beforeHash=ReadPortableRuntimeRetailCollision();const auto beforeOrder=ReadPortableWorldActorOrder();
        const auto beforeGc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
        const auto model=ReadPortableRuntimeNativeModel();const auto count=GetPortableRuntimeMapActors(true).size();
        const auto baseline=temporary.Save("baseline.sav");
        const auto unchanged=[&] {
            const auto after=ReadPortableRuntimeNativeWindowSummary();const auto gc=GC::GetStats();
            const auto currentModel=ReadPortableRuntimeNativeModel();const auto hash=ReadPortableRuntimeRetailCollision();
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
                "Tile refusal retained rows/held/cache/window names/roots/GC/player/model/actor/save state");
        };
        const auto run=[&](Mode mode,const QuestVr::Vm::Limits& limits=QuestVr::Vm::Limits{}) {
            auto result=ProbePortableNativeWindowTileQuery(player,mode,limits);
            Require(!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
                "Isolated Tile diagnostic unexpectedly committed");
            unchanged();++refusals;return result;
        };
        const auto complete=run(Mode::RoundTrip);
        Require(complete.execution.error=="Native Tile diagnostic complete; rollback required"&&
            complete.execution.nativeWork&&complete.execution.nativeWork->used&&!complete.execution.nativeWork->exhausted,
            "Tile fixture failed before completion: "+complete.execution.error);
        Stages(complete);
        const auto repeated=run(Mode::RoundTrip);Stages(repeated);
        Require(repeated.rootPath==complete.rootPath&&repeated.tilePath==complete.tilePath&&
            repeated.childPaths==complete.childPaths&&repeated.execution.writes==complete.execution.writes&&
            repeated.execution.instructions==complete.execution.instructions&&
            repeated.execution.nativeWork->used==complete.execution.nativeWork->used,
            "Tile rollback changed identity allocation or deterministic shared work");
        const auto late=run(Mode::LateFailure);Stages(late);
        Require(late.execution.error=="Native Tile diagnostic late failure after valid layout"&&
            late.execution.writes==complete.execution.writes,"Late Tile failure did not follow all genuine layout stores");
        for(const auto mode:{Mode::BadGraph,Mode::DeadChild}) {
            const auto invalid=run(mode);
            Require(invalid.stages.size()==2u&&invalid.execution.error.find("incorrectly accepted")==std::string::npos,
                "Native Tile accepted a cyclic/lifetime-invalid child graph after valid row population: "+invalid.execution.error);
        }
        for(const auto cut:{std::size_t{0u},std::size_t{1u},complete.stages[0u].writes-1u,
            complete.stages[1u].writes-1u,complete.stages[3u].writes-1u,complete.stages[4u].writes-1u,
            complete.stages[5u].writes-1u,complete.stages[6u].writes-1u,complete.execution.writes-1u}) {
            QuestVr::Vm::Limits limits;limits.writes=cut;const auto partial=run(Mode::RoundTrip,limits);
            Require(partial.execution.status==QuestVr::Vm::Status::Budget&&partial.execution.writes==cut,
                "Native Tile construction/query/row/placement stores reset shared write budget: "+partial.execution.error);
        }
        for(const auto cut:{std::size_t{0u},complete.execution.nativeWork->used/2u,complete.execution.nativeWork->used-1u}) {
            QuestVr::Vm::Limits limits;limits.nativeWork=cut;const auto partial=run(Mode::RoundTrip,limits);
            Require(partial.execution.nativeWork&&partial.execution.nativeWork->exhausted,
                "Native Tile ignored shared constructor/query/traversal work budget");
        }
        for(const auto cut:{std::size_t{0u},std::size_t{64u<<10u}}) {
            QuestVr::Vm::Limits limits;limits.retainedBytes=cut;const auto partial=run(Mode::RoundTrip,limits);
            Require(partial.execution.error!="Native Tile diagnostic complete; rollback required",
                "Native Tile fixture ignored shared VM retained-byte cut");
        }
        for(const auto& before:sources) {
            const auto after=Source(before.path);Require(before.bytes==after.bytes&&before.hash==after.hash,
                "Native Tile controls changed owned package/map/INI source");
        }
        std::cout<<"PASS owned first-Training native Tile queries: "<<checks<<" checks / "<<refusals
            <<" rollback controls; actual Root/Tile/Window constructors, local visibility, real1465/1464 dispatch, "
            "cache hit, wrap/fill rows, raw/reversed placement, consumed/replaced rows, invalid graph/lifetime, "
            "shared write/work/VM-retained cuts and full late undo. Isolated fixture only; "
            "configured HUD/startup/Quest remain separate proofs\n";
        return 0;
    }catch(const std::exception& error) {
        ShutdownPortableRuntime();std::cerr<<"FAIL owned native Tile fixture: "<<error.what()<<'\n';return 1;
    }
}
