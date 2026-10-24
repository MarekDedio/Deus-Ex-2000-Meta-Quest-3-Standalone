#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_engine_collision.h"
#include "portable_object_lookup.h"
#include "quest_runtime_script_packages.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
namespace fs=std::filesystem;
using QuestVr::Vm::Kind;
using QuestVr::Vm::Value;
std::size_t checks{},refusals{};
void Require(bool good,const std::string& why) {++checks;if(!good)throw std::runtime_error(why);}
struct Temporary {
    fs::path parent,path;
    Temporary() {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path(),"Temporary checkpoint parent is a drive root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<20u;++i) {
            const auto candidate=parent/("quest-main-animation-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)){path=fs::canonical(candidate);break;}
        }
        Require(!path.empty()&&path.parent_path()==parent,"Cannot create isolated animation checkpoint directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code error;
        if(!path.empty()&&path.parent_path()==parent) {
            fs::remove(path/"state.sav",error);fs::remove(path,error);
        }
    }
    std::vector<std::uint8_t> Save() {
        const auto file=path/"state.sav";
        Require(SavePortableRuntimeState(file.string()),"Cannot capture full animation checkpoint");
        const auto size=fs::file_size(file);Require(size<64u*1024u*1024u,"Animation checkpoint exceeds its bound");
        std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
        std::ifstream input(file,std::ios::binary);input.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(size));
        Require(bool(input),"Animation checkpoint read failed");return data;
    }
};
void Original(const fs::path& root) {
    Temporary temporary;std::vector<PortablePackageTables> packages;
    for(const auto* name:QuestVr::RuntimeScriptPackages)
        packages.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize owned original packages");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&
        PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared,
        "Cannot prepare genuine native lookup/Engine collision services");
    Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string())).passed,
        "Cannot load first Training");
    const auto actors=GetPortableRuntimeMapActors(true);
    const auto found=std::find_if(actors.begin(),actors.end(),[](const auto& actor){return actor.objectPath=="00_Training.Doctor1";});
    Require(found!=actors.end(),"Actual Training Doctor1 is absent");const auto actor=found->objectPath;
    const auto level=ReadPortableActorDeclaredScriptProperty(actor,"Engine.Actor.Level");
    Require(level.kind==Kind::Object&&!level.text.empty()&&
        !ReadPortableActorDeclaredScriptProperty(level.text,"Engine.LevelInfo.bBegunPlay").boolean,
        "Animation diagnostic must remain an actual dormant world, not fabricated begun play");
    const auto separator=found->meshPath.find('.');
    Require(separator!=std::string::npos,"Original Doctor Mesh lacks its source package identity");
    const auto source=std::find_if(packages.begin(),packages.end(),[&](const auto& package){
        return fs::path(package.sourcePath).stem().string()==found->meshPath.substr(0,separator);
    });
    Require(source!=packages.end(),"Original Doctor mesh source package is absent");
    const auto mesh=LoadPortableLodMesh(*source,FindPortableExport(*source,found->meshPath.substr(separator+1u)));
    Require(bool(mesh.animation),"Original Doctor has no retained animation metadata");
    const auto sequence=std::find_if(mesh.animation->sequences.begin(),mesh.animation->sequences.end(),[](const auto& item){
        const float last=item.numFrames>0?1.0f-1.0f/static_cast<float>(item.numFrames):0.0f;
        return item.numFrames>1&&item.rate>0&&std::any_of(item.notifies.begin(),item.notifies.end(),[&](const auto& notify){
            return notify.time>0.0f&&notify.time<last;
        });
    });
    Require(sequence!=mesh.animation->sequences.end(),"Original Doctor has no usable authored interior notify sequence");
    std::vector<bool> ordinals(sequence->notifies.size());
    for(const auto& notify:sequence->notifies) {
        Require(notify.serializedIndex<ordinals.size()&&!ordinals[notify.serializedIndex],
            "Original mesh lost unique serialized notify order while retaining presentation time order");
        ordinals[notify.serializedIndex]=true;
    }
    const float last=1.0f-1.0f/static_cast<float>(sequence->numFrames);
    const auto notice=std::find_if(sequence->notifies.begin(),sequence->notifies.end(),[&](const auto& item){return item.time>0&&item.time<last;});
    const auto armed=ExecutePortableActorFunction(actor,"Engine.Actor.PlayAnim",{
        {Value::Text(Kind::Name,sequence->name),{}},{Value::Float(1.0f),{}},{Value::Float(0.0f),{}}});
    Require(armed.passed()&&armed.committed,"Original PlayAnim failed: "+armed.error);
    const auto runtimeMesh=GetPortableRuntimeMesh(found->meshPath);
    Require(runtimeMesh.animation&&runtimeMesh.animation->sequences.size()==mesh.animation->sequences.size(),
        "Original PlayAnim did not decode the actual same source mesh");
    const float rate=sequence->rate/static_cast<float>(sequence->numFrames);
    const float delta=(notice->time+std::min(0.001f,(last-notice->time)*0.5f))/rate;
    const auto baseline=temporary.Save();const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    const auto undo=[&] {
        Require(temporary.Save()==baseline&&GetPortableRuntimeWorldRevision()==revision,
            "Animation phase retained actor/clock/save/revision changes");
        const auto stats=GC::GetStats();Require(stats.numObjects==gc.numObjects&&stats.memoryUsage==gc.memoryUsage,
            "Animation phase retained native roots");
    };
    const auto full=ProbePortableActorMainAnimation(actor,delta);
    Require(full.execution.passed()&&!full.execution.committed&&full.completed,
        "Original main animation phase failed: "+full.execution.error);
    Require(full.notifyEvents>0u&&full.notifySearches>0u&&full.floatWrites>=2u&&full.frameAfter>=notice->time,
        "Original dormant notify did not commit its boundary before retained handler lookup/ProcessEvent gate");undo();
    Require(full.execution.writes>0u&&full.execution.writes<128u,"Unexpected original main animation write bound");
    for(std::size_t prefix=0;prefix<full.execution.writes;++prefix) {
        QuestVr::Vm::Limits limits;limits.writes=prefix;const auto partial=ProbePortableActorMainAnimation(actor,delta,limits);
        Require(!partial.execution.passed()&&!partial.execution.committed&&!partial.completed&&
            partial.execution.status==QuestVr::Vm::Status::Budget&&partial.execution.writes==prefix,
            "Original animation ignored shared write prefix "+std::to_string(prefix)+": "+partial.execution.error);++refusals;undo();
    }
    Require(full.execution.nativeWork&&full.execution.nativeWork->used>0u,"Original phase omitted native work accounting");
    for(const auto prefix:{std::size_t{0u},full.execution.nativeWork->used/2u,full.execution.nativeWork->used-1u}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=prefix;const auto partial=ProbePortableActorMainAnimation(actor,delta,limits);
        Require(!partial.execution.passed()&&!partial.execution.committed&&partial.execution.nativeWork&&partial.execution.nativeWork->exhausted,
            "Original animation ignored shared native-work prefix");++refusals;undo();
    }
    const auto repeat=ProbePortableActorMainAnimation(actor,delta);
    Require(repeat.execution.passed()&&repeat.completed&&repeat.iterations==full.iterations&&repeat.frameAfter==full.frameAfter&&
        repeat.execution.writes==full.execution.writes&&repeat.notifyEvents==full.notifyEvents&&
        repeat.notifySearches==full.notifySearches,"Warmed original animation phase changed its actual boundary/store sequence");undo();
    std::cout<<"PASS owned first-Training MAIN animation: actor="<<actor<<" sequence="<<sequence->name<<" notify="<<notice->function
        <<" boundary="<<notice->time<<" writes="<<full.execution.writes<<" lookups="<<full.notifyEvents<<" searches="<<full.notifySearches
        <<"; "<<checks<<" checks / "<<refusals<<" refusal prefixes. Dormant original begun gate suppresses script callbacks; "
          "NOT active notify callback, Fire/equip/melee, full Tick, committed startup or headset acceptance.\n";
}
}
int main(int argc,char** argv) {
    std::cout<<std::unitbuf;
    try {
        if(argc==1){std::cout<<"SKIP owned Training main-animation phase: supply original game root\n";return 77;}
        Require(argc==2,"Usage: portable_actor_animation_phase_test OWNED_GAME_ROOT");Original(argv[1]);return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
