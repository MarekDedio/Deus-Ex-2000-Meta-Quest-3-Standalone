#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_engine_collision.h"
#include "portable_object_lookup.h"
#include "quest_runtime_script_packages.h"
#include "quest_save_bundle.h"
#include "quest_script_state.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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
    void Restore(const std::vector<std::uint8_t>& bytes) {
        const auto file=(path/"state.sav").string();
        Require(QuestVr::WriteDurableSaveFile(file,bytes)&&LoadPortableRuntimeState(file),
            "Cannot restore full original animation checkpoint");
    }
};
std::uint32_t Word(const std::vector<std::uint8_t>& bytes,std::size_t offset) {
    Require(offset<=bytes.size()&&bytes.size()-offset>=4u,"Animation save word is truncated");
    std::uint32_t out{};
    for(unsigned i=0u;i<4u;++i)out|=std::uint32_t(bytes[offset+i])<<(i*8u);
    return out;
}
QuestVr::ScriptSavedState SavedScript(const std::vector<std::uint8_t>& bytes) {
    // Walk the existing v3 gameplay prefix, not a search for coincidental magic
    // bytes. Modern envelopes retain that prefix and one exact script trailer.
    const auto version=Word(bytes,4u);
    Require(Word(bytes,0u)==0x53515844u&&((version>=4u&&version<=13u)||version==16u||version==17u),
        "Animation checkpoint is not a recognized script-bearing envelope");
    std::size_t cursor=8u;
    const auto skip=[&](std::size_t count) {
        Require(cursor<=bytes.size()&&count<=bytes.size()-cursor,"Animation save prefix is truncated");cursor+=count;
    };
    const auto word=[&] {const auto out=Word(bytes,cursor);cursor+=4u;return out;};
    const auto text=[&] {const auto length=word();Require(length<=1'048'576u,"Animation save text exceeds bound");skip(length);};
    const auto list=[&] {const auto count=word();Require(count<=100'000u,"Animation save list exceeds bound");for(std::uint32_t i=0u;i<count;++i)text();};
    list();list();list();skip(4u);
    const auto damaged=word();Require(damaged<=100'000u,"Animation damage list exceeds bound");
    for(std::uint32_t i=0u;i<damaged;++i){text();skip(4u);}
    skip(8u);list();list();list();list();
    const auto length=word();
    Require(length>0u&&cursor<=bytes.size()&&length==bytes.size()-cursor,"Animation save trailer has a mismatched bound");
    return QuestVr::DecodeScriptSavedState({bytes.begin()+static_cast<std::ptrdiff_t>(cursor),bytes.end()});
}
void Near(float actual,float expected,const std::string& why) {
    Require(std::isfinite(actual)&&std::isfinite(expected)&&
        std::abs(actual-expected)<=0.000001f*std::max(1.0f,std::abs(expected)),why);
}
float LiveFloat(const std::string& actor,const std::string& name,std::uint32_t index=0u) {
    const auto value=ReadPortableActorDeclaredScriptProperty(actor,"Engine.Actor."+name,index);
    Require(value.kind==Kind::Float,"Original animation field is not a live Float: "+name);
    return value.floating;
}
void Call(const std::string& actor,const std::string& function,std::vector<QuestVr::Vm::Evaluation> packet) {
    const auto out=ExecutePortableActorFunction(actor,"Engine.Actor."+function,packet);
    Require(out.passed()&&out.committed,"Original "+function+" command failed: "+out.error);
}
void PlayMain(const std::string& actor,const std::string& sequence,float rate=1.0f) {
    Call(actor,"PlayAnim",{{Value::Text(Kind::Name,sequence),{}},{Value::Float(rate),{}},{Value::Float(0.0f),{}}});
}
void PlayBlend(const std::string& actor,const std::string& sequence,std::uint32_t slot,float rate=1.0f) {
    // Engine.u native1010 has optional Float Rate/TweenTime then signed Int
    // BlendSlot; do not accidentally use the pinned wrapper's Byte packet.
    Call(actor,"PlayBlendAnim",{{Value::Text(Kind::Name,sequence),{}},{Value::Float(rate),{}},
        {Value::Float(0.0f),{}},{Value::Integer(static_cast<std::int32_t>(slot)),{}}});
}
void TweenBlend(const std::string& actor,const std::string& sequence,std::uint32_t slot,float seconds) {
    Call(actor,"TweenBlendAnim",{{Value::Text(Kind::Name,sequence),{}},{Value::Float(seconds),{}},
        {Value::Integer(static_cast<std::int32_t>(slot)),{}}});
}
void ClockCoherent(Temporary& temporary,const std::string& actor) {
    const auto state=SavedScript(temporary.Save());
    const auto object=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& item){return item.path==actor;});
    Require(object!=state.objects.end()&&object->clock,"Committed original animation has no native saved clock");
    const auto& clock=*object->clock;
    Require(ReadPortableActorDeclaredScriptProperty(actor,"Engine.Actor.AnimSequence").text==clock.pose.main.sequence,
        "Committed main sequence disagrees with its native saved clock");
    Require(LiveFloat(actor,"AnimFrame")==clock.pose.main.normalizedFrame&&LiveFloat(actor,"AnimRate")==clock.main.rate&&
        LiveFloat(actor,"AnimLast")==clock.main.last&&LiveFloat(actor,"TweenRate")==clock.main.tweenRate,
        "Committed main fields disagree with their native saved clock");
    for(std::uint32_t slot=0u;slot<4u;++slot) {
        const auto& blend=clock.blends[slot];
        Require(ReadPortableActorDeclaredScriptProperty(actor,"Engine.Actor.BlendAnimSequence",slot).text==clock.pose.blends[slot].sequence,
            "Committed blend sequence disagrees with its native saved clock");
        Require(LiveFloat(actor,"BlendAnimFrame",slot)==clock.pose.blends[slot].normalizedFrame&&
            LiveFloat(actor,"BlendAnimRate",slot)==blend.rate&&LiveFloat(actor,"BlendAnimLast",slot)==blend.last&&
            LiveFloat(actor,"BlendAnimMinRate",slot)==blend.minRate&&LiveFloat(actor,"BlendTweenRate",slot)==blend.tweenRate&&
            LiveFloat(actor,"OldBlendAnimRate",slot)==blend.oldRate,
            "Committed live blend arrays disagree with their native saved clock");
        const auto sim=ReadPortableActorDeclaredScriptProperty(actor,"Engine.Actor.SimBlendAnim",slot);
        Require(sim.kind==Kind::Struct&&sim.fields.size()==4u&&
            sim.fields.at("x").floating==blend.simulated[0]&&sim.fields.at("y").floating==blend.simulated[1]&&
            sim.fields.at("z").floating==blend.simulated[2]&&sim.fields.at("w").floating==blend.simulated[3],
            "Committed native blend Plane disagrees with live reflected SimBlendAnim");
    }
    const auto actors=GetPortableRuntimeMapActors(true);
    const auto found=std::find_if(actors.begin(),actors.end(),[&](const auto& item){return item.objectPath==actor;});
    Require(found!=actors.end()&&found->animation.frame==clock.pose.main.normalizedFrame,
        "Animation render snapshot disagrees with the committed native main clock");
    for(std::size_t slot=0u;slot<4u;++slot)
        Require(found->animation.blends[slot].frame==clock.pose.blends[slot].normalizedFrame,
            "Animation render snapshot disagrees with a committed native blend clock");
}
void CommittedChannels(Temporary& temporary,const std::string& actor,const std::string& sequence) {
    // Real original command packets create all clocks. The original Level stays
    // before begun play throughout; no direct property seed or fake startup.
    PlayMain(actor,sequence);
    PlayBlend(actor,sequence,0u);PlayBlend(actor,sequence,1u,2.0f);
    TweenBlend(actor,sequence,2u,1.0f);TweenBlend(actor,sequence,3u,2.0f);
    const float mainBefore=LiveFloat(actor,"AnimFrame"),mainRate=LiveFloat(actor,"AnimRate");
    Require(mainRate>0.0f,"Original running main command did not produce a positive playback rate");
    const float elapsed=0.001f/mainRate;
    std::array<float,4u> before{},rates{};
    for(std::uint32_t slot=0u;slot<4u;++slot) {
        before[slot]=LiveFloat(actor,"BlendAnimFrame",slot);
        rates[slot]=LiveFloat(actor,slot<2u?"BlendAnimRate":"BlendTweenRate",slot);
        Require(slot<2u?before[slot]>=0.0f:before[slot]<0.0f,"Original Play/Tween blend packet selected the wrong phase");
    }
    const auto baseline=temporary.Save();
    const auto full=AdvancePortableActorAnimationChannels(actor,elapsed);
    Require(full.execution.passed()&&full.execution.committed&&full.completed,
        "Committing original MAIN+BLEND phase failed: "+full.execution.error);
    Require(full.mainIterations==1u&&full.blendIterations==3u&&full.blendWrites==12u,
        "Original ordinary MAIN must leave exactly three shared-counter blend passes, not one blend tick per slot");
    Near(full.mainRemainingSeconds,elapsed,"Sub-end MAIN changed its native remaining local");
    Near(full.remainingSeconds,elapsed,"Ordinary/subzero blend passes incorrectly consumed shared elapsed");
    Near(LiveFloat(actor,"AnimFrame"),mainBefore+elapsed*mainRate,"Original MAIN ordinary playback advanced more than once");
    for(std::uint32_t slot=0u;slot<4u;++slot) {
        auto expected=before[slot];
        for(unsigned pass=0u;pass<3u;++pass)expected=static_cast<float>(double(expected)+double(elapsed)*rates[slot]);
        Near(LiveFloat(actor,"BlendAnimFrame",slot),expected,"Original blend did not reuse the same residual on all three passes");
    }
    Require(full.execution.writes>0u&&full.execution.writes<128u&&full.execution.nativeWork&&
        full.execution.nativeWork->used>0u,"Committing channel control lacks bounded shared write/work accounting");
    ClockCoherent(temporary,actor);const auto committed=temporary.Save();
    temporary.Restore(committed);ClockCoherent(temporary,actor);
    Require(temporary.Save()==committed,"Original committing animation clock did not roundtrip the full save exactly");
    temporary.Restore(baseline);
    const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    const auto undo=[&] {
        Require(temporary.Save()==baseline&&GetPortableRuntimeWorldRevision()==revision,
            "Failed committing channel phase retained actor/clock/save/revision changes");
        const auto stats=GC::GetStats();Require(stats.numObjects==gc.numObjects&&stats.memoryUsage==gc.memoryUsage,
            "Failed committing channel phase retained native roots");
    };
    for(std::size_t prefix=0u;prefix<full.execution.writes;++prefix) {
        QuestVr::Vm::Limits limits;limits.writes=prefix;
        const auto partial=AdvancePortableActorAnimationChannels(actor,elapsed,limits);
        Require(!partial.execution.passed()&&!partial.execution.committed&&!partial.completed&&
            partial.execution.status==QuestVr::Vm::Status::Budget&&partial.execution.writes==prefix,
            "Committing MAIN+BLEND ignored shared write prefix "+std::to_string(prefix)+": "+partial.execution.error);
        ++refusals;undo();
    }
    for(const auto prefix:{std::size_t{0u},full.execution.nativeWork->used/2u,full.execution.nativeWork->used-1u}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=prefix;
        const auto partial=AdvancePortableActorAnimationChannels(actor,elapsed,limits);
        Require(!partial.execution.passed()&&!partial.execution.committed&&!partial.completed&&
            partial.execution.nativeWork&&partial.execution.nativeWork->exhausted,
            "Committing MAIN+BLEND ignored shared native-work prefix");++refusals;undo();
    }
    const auto repeat=AdvancePortableActorAnimationChannels(actor,elapsed);
    Require(repeat.execution.passed()&&repeat.completed&&repeat.execution.committed&&
        repeat.mainIterations==full.mainIterations&&repeat.blendIterations==full.blendIterations&&
        repeat.blendWrites==full.blendWrites&&repeat.execution.writes==full.execution.writes&&temporary.Save()==committed,
        "Repeated original committing MAIN+BLEND phase changed store order or checkpoint");

    // Slot zero ends during the FIRST scan. Its residual is shared by later
    // slots in that scan and reused in both following outer scans.
    PlayMain(actor,sequence,0.00001f);PlayBlend(actor,sequence,0u);
    for(std::uint32_t slot=1u;slot<4u;++slot)PlayBlend(actor,sequence,slot,0.01f);
    const float first=LiveFloat(actor,"BlendAnimFrame",0u),rate=LiveFloat(actor,"BlendAnimRate",0u);
    const float last=LiveFloat(actor,"BlendAnimLast",0u),boundaryElapsed=2.0f*(last-first)/rate;
    const float tentative=static_cast<float>(double(first)+double(boundaryElapsed)*rate);
    const float residual=static_cast<float>((double(tentative)-last)*boundaryElapsed/(double(tentative)-first));
    for(std::uint32_t slot=1u;slot<4u;++slot){before[slot]=LiveFloat(actor,"BlendAnimFrame",slot);rates[slot]=LiveFloat(actor,"BlendAnimRate",slot);}
    const auto boundary=AdvancePortableActorAnimationChannels(actor,boundaryElapsed);
    Require(boundary.execution.passed()&&boundary.execution.committed&&boundary.completed&&
        boundary.mainIterations==1u&&boundary.blendIterations==3u,
        "Original first-slot completion failed or lost the MAIN-shared pass counter: "+boundary.execution.error);
    Near(boundary.mainRemainingSeconds,boundaryElapsed,"Main changed residual before blend boundary");
    Near(boundary.remainingSeconds,residual,"Blend boundary did not publish its native shared residual");
    Require(LiveFloat(actor,"BlendAnimFrame",0u)==last&&LiveFloat(actor,"BlendAnimRate",0u)==0.0f,
        "Original completed blend did not clamp frame and stop its rate");
    for(std::uint32_t slot=1u;slot<4u;++slot) {
        auto expected=before[slot];
        for(unsigned pass=0u;pass<3u;++pass)expected=static_cast<float>(double(expected)+double(residual)*rates[slot]);
        Near(LiveFloat(actor,"BlendAnimFrame",slot),expected,"Later blend slot did not receive the first-slot residual three times");
    }
    ClockCoherent(temporary,actor);

    PlayMain(actor,sequence,0.00001f);TweenBlend(actor,sequence,0u,0.25f);
    for(std::uint32_t slot=1u;slot<4u;++slot)PlayBlend(actor,sequence,slot,0.01f);
    const float tweenBefore=LiveFloat(actor,"BlendAnimFrame",0u),tweenRate=LiveFloat(actor,"BlendTweenRate",0u);
    const float tweenElapsed=-2.0f*tweenBefore/tweenRate;
    const float tweenResidual=static_cast<float>((double(tweenBefore)+double(tweenElapsed)*tweenRate)*tweenElapsed/
        (double(tweenElapsed)*tweenRate));
    for(std::uint32_t slot=1u;slot<4u;++slot){before[slot]=LiveFloat(actor,"BlendAnimFrame",slot);rates[slot]=LiveFloat(actor,"BlendAnimRate",slot);}
    const auto tween=AdvancePortableActorAnimationChannels(actor,tweenElapsed);
    Require(tween.execution.passed()&&tween.execution.committed&&tween.completed&&tween.mainIterations==1u&&tween.blendIterations==3u,
        "Original TweenBlend crossing failed: "+tween.execution.error);
    Require(LiveFloat(actor,"BlendAnimFrame",0u)==0.0f&&LiveFloat(actor,"BlendAnimRate",0u)==0.0f,
        "Original TweenBlend endpoint did not remain its stopped target frame");
    Near(tween.remainingSeconds,tweenResidual,"TweenBlend did not carry its crossing residual across later slots");
    for(std::uint32_t slot=1u;slot<4u;++slot) {
        auto expected=before[slot];
        for(unsigned pass=0u;pass<3u;++pass)expected=static_cast<float>(double(expected)+double(tweenResidual)*rates[slot]);
        Near(LiveFloat(actor,"BlendAnimFrame",slot),expected,"Later blend slot lost the TweenBlend crossing residual");
    }
    ClockCoherent(temporary,actor);

    // Actual TweenAnim(time0) makes MAIN inactive while the blend commands
    // remain active. Retail's MAIN gate prevents every blend store.
    Call(actor,"TweenAnim",{{Value::Text(Kind::Name,sequence),{}},{Value::Float(0.0f),{}}});
    for(std::uint32_t slot=0u;slot<4u;++slot)PlayBlend(actor,sequence,slot);
    const auto inactive=temporary.Save();const auto inactiveRevision=GetPortableRuntimeWorldRevision();
    const auto gated=AdvancePortableActorAnimationChannels(actor,0.5f);
    Require(gated.execution.passed()&&gated.completed&&gated.mainIterations==0u&&gated.blendIterations==0u&&
        gated.blendWrites==0u&&gated.execution.writes==0u&&temporary.Save()==inactive&&
        GetPortableRuntimeWorldRevision()==inactiveRevision,
        "Inactive original MAIN incorrectly allowed active blend channels to advance");
    const auto level=ReadPortableActorDeclaredScriptProperty(actor,"Engine.Actor.Level");
    Require(!ReadPortableActorDeclaredScriptProperty(level.text,"Engine.LevelInfo.bBegunPlay").boolean,
        "Explicit animation channel controls changed the original dormant-world gate");
    std::cout<<"PASS owned first-Training committed MAIN+BLEND: original command packets, MAIN1/BLEND3 shared counter, "
        "ordinary/end/tween residual carry, all four live channels, native clock/render/save coherence, full write/work undo and inactive MAIN gate; "
        "NOT complete Tick, live crate attack, begun startup or headset acceptance.\n";
}
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
    CommittedChannels(temporary,actor,sequence->name);
    std::cout<<"PASS owned animation phases total: "<<checks<<" checks / "<<refusals<<" refusal prefixes.\n";
}
}
int main(int argc,char** argv) {
    std::cout<<std::unitbuf;
    try {
        if(argc==1){std::cout<<"SKIP owned Training main-animation phase: supply original game root\n";return 77;}
        Require(argc==2,"Usage: portable_actor_animation_phase_test OWNED_GAME_ROOT");Original(argv[1]);return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
