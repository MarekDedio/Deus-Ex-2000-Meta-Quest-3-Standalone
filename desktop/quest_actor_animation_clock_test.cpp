#include "quest_actor_animation_clock.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace QuestVr;
std::size_t checks{};

void Require(const bool value,const std::string& message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}

void Near(const double actual,const double expected,const std::string& message,const double epsilon=0.00001) {
    Require(std::isfinite(actual) && std::fabs(actual-expected)<=epsilon,message);
}

PortableMeshAnimationData Data() {
    PortableMeshAnimationData data;
    data.frameVertices=3u; data.animationFrames=9u;
    data.frameVerticesPacked.resize(27u);
    data.sequences={
        {"Primary","Movement",0,4,8.0f,{{0.25f,"Step"},{0.25f,"Duplicate"},{0.5f,"Absent"},{0.6f,"Fire"}}},
        {"Next","Movement",4,4,4.0f,{}},
        {"Single","Still",8,1,30.0f,{}},
        {"Dangling","Broken",8,3,30.0f,{},true}};
    return data;
}

ActorAnimationCommand Command(const ActorAnimationCommandKind kind,const std::string& sequence,
                              const float rate=1.0f,const float tween=0.0f,
                              const float minimum=0.0f,const std::int32_t slot=0) {
    return {kind,sequence,rate,tween,minimum,slot};
}

void Apply(const PortableMeshAnimationData& data,ActorAnimationClock& clock,const ActorAnimationCommand& command,
           const float speed=0.0f) {
    const auto result=ApplyActorAnimationCommand(&data,clock,command,speed);
    Require(result.applied && result.error.empty(),"Command rejected: "+result.error);
}

bool HasNotify(const std::string& function) { return function=="Step" || function=="Duplicate" || function=="Fire"; }

void QueriesAndCommands() {
    const auto data=Data(); ActorAnimationClock clock;
    Require(ActorClockHasAnim(&data,"Unknown"),"Pinned HasAnim first-sequence fallback changed");
    Require(!ActorClockHasAnim(nullptr,"Primary"),"No mesh HasAnim returned true");
    Near(clock.main.rate,0,"Initial animation unexpectedly runs");
    Require(!ActorClockIsAnimating(clock),"Initial zero-rate animation is active");
    Require(ActorClockGetAnimGroup(&data,"missing")=="Movement","GetAnimGroup fallback changed");
    Require(ActorClockGetAnimGroup(nullptr,"Primary").empty(),"No mesh group is nonempty");
    auto result=ApplyActorAnimationCommand(nullptr,clock,Command(ActorAnimationCommandKind::PlayAnim,"Primary"));
    Require(!result.applied && result.error.empty(),"No-mesh PlayAnim is not a native no-op");
    auto empty=data; empty.sequences.clear();
    result=ApplyActorAnimationCommand(&empty,clock,Command(ActorAnimationCommandKind::PlayAnim,"Primary"));
    Require(!result.applied && result.error.empty(),"No-sequence PlayAnim is not a native no-op");

    clock.pose.main.sequence="Primary"; clock.pose.main.normalizedFrame=0.125f;
    clock.main.minRate=0.7f; clock.main.finished=true;
    Apply(data,clock,Command(ActorAnimationCommandKind::PlayAnim,"Next",2.0f,0.2f));
    Require(clock.pose.main.sequence=="Next","PlayAnim lost requested spelling");
    Require(clock.pose.main.previous.vertexOffset0==0 && clock.pose.main.previous.vertexOffset1==3,"Previous frame offsets changed");
    Near(clock.pose.main.previous.fraction,0.5,"Previous fraction changed");
    Near(clock.pose.main.normalizedFrame,-0.25,"PlayAnim negative tween start changed");
    Near(clock.main.last,0.75,"PlayAnim terminal frame changed");
    Near(clock.main.rate,2,"PlayAnim native normalized rate changed");
    Near(clock.main.tweenRate,1.25,"PlayAnim tween rate changed");
    Near(clock.main.oldRate,2,"PlayAnim OldAnimRate changed");
    Near(clock.main.minRate,0.7,"Multi-frame PlayAnim must preserve existing AnimMinRate");
    Require(!clock.main.loop && !clock.main.finished && !clock.main.notify,"PlayAnim flags changed");

    Apply(data,clock,Command(ActorAnimationCommandKind::LoopAnim,"Primary",1.0f,0.0f,0.5f));
    clock.pose.main.normalizedFrame=0.3f; const auto history=clock.pose.main.previous;
    clock.main.finished=true;
    Apply(data,clock,Command(ActorAnimationCommandKind::LoopAnim,"pRiMaRy",0.5f,0.3f,0.2f));
    Near(clock.pose.main.normalizedFrame,0.3,"Repeated LoopAnim reset frame");
    Require(clock.pose.main.sequence=="Primary","Repeated LoopAnim rewrote sequence spelling");
    Require(clock.pose.main.previous.vertexOffset0==history.vertexOffset0 &&
        clock.pose.main.previous.vertexOffset1==history.vertexOffset1 && clock.pose.main.previous.fraction==history.fraction,
        "Repeated LoopAnim recaptured history");
    Near(clock.main.rate,1,"Repeated LoopAnim did not update rate");
    Near(clock.main.minRate,0.4,"Repeated LoopAnim did not update MinRate");
    Near(clock.main.tweenRate,1.0f/1.2f,"Repeated LoopAnim did not update TweenRate");
    Require(clock.main.finished,"Repeated LoopAnim rewrote bAnimFinished");

    Apply(data,clock,Command(ActorAnimationCommandKind::TweenAnim,"Next",1.0f,0.4f));
    Near(clock.pose.main.previous.fraction,0.2,"TweenAnim did not capture old pose");
    Near(clock.pose.main.normalizedFrame,-0.25,"TweenAnim negative frame changed");
    Near(clock.main.tweenRate,0.625,"TweenAnim rate changed");
    Require(clock.main.rate==0 && clock.main.last==0 && clock.main.minRate==0 &&
        !clock.main.loop && !clock.main.notify && !clock.main.finished,"TweenAnim flags/scalars changed");

    Apply(data,clock,Command(ActorAnimationCommandKind::PlayAnim,"Single"));
    Near(clock.pose.main.normalizedFrame,-1,"Single-frame command must tween for minimum duration");
    Near(clock.main.tweenRate,10,"Single-frame native default duration changed");
    Require(clock.main.rate==0 && clock.main.last==0 && clock.main.minRate==0 && !clock.main.notify,
        "Single-frame native properties changed");
    Apply(data,clock,Command(ActorAnimationCommandKind::LoopAnim,"Single",1.0f,0.2f));
    Near(clock.main.tweenRate,5,"Single-frame LoopAnim explicit tween changed");
    Require(clock.main.loop,"Single-frame LoopAnim flag missing");
    Apply(data,clock,Command(ActorAnimationCommandKind::FinishAnim,""));
    Require(!clock.main.loop && clock.main.finishAnimWaiting && !clock.main.finished,"FinishAnim latent flags changed");

    result=ApplyActorAnimationCommand(&data,clock,Command(ActorAnimationCommandKind::PlayAnim,"missing"));
    Require(result.applied && result.fallbackUsed && result.resolvedSequence=="Primary" && clock.pose.main.sequence=="missing",
        "PlayAnim unknown name fallback/requested name changed");
    result=ApplyActorAnimationCommand(&data,clock,Command(ActorAnimationCommandKind::PlayAnim,"Dangling"));
    Require(result.applied && result.selectedOriginalSpanInvalid,"Dangling authored declaration blocked a command or was hidden");
    result=ApplyActorAnimationCommand(&data,clock,Command(ActorAnimationCommandKind::PlayAnim,"Next",1,0.2f));
    Require(result.applied && result.capturedOriginalSpanInvalid,"Dangling old history blocked available target metadata");
    Require(clock.pose.main.previous.vertexOffset0==24 && clock.pose.main.previous.vertexOffset1==27,
        "Dangling previous offsets were fabricated/clamped");
}

void BlendCommands() {
    const auto data=Data(); ActorAnimationClock clock;
    Apply(data,clock,Command(ActorAnimationCommandKind::PlayBlendAnim,"Primary",1,0.2f,0,2));
    const auto& channel=clock.pose.blends[2]; const auto& blend=clock.blends[2];
    Require(channel.sequence=="Primary" && channel.previous.fraction==-1,"Initial blend history changed");
    Near(channel.normalizedFrame,-0.25,"Blend negative tween start changed");
    Near(blend.rate,2,"Blend normalized rate changed");
    Near(blend.last,0.75,"Blend last frame changed");
    Near(blend.tweenRate,1.25,"Blend tween rate changed");
    Near(blend.simulated[0],1250,"SimBlendAnim x changed");
    Near(blend.simulated[1],7500,"SimBlendAnim y changed");
    Near(blend.simulated[2],-2500,"SimBlendAnim z changed");
    Near(blend.simulated[3],20000,"SimBlendAnim w changed");
    clock.pose.blends[2].normalizedFrame=0.1f;
    Apply(data,clock,Command(ActorAnimationCommandKind::PlayBlendAnim,"Next",1,0,0,2));
    Near(clock.pose.blends[2].previous.fraction,0.4,"PlayBlendAnim old fraction changed");
    Near(clock.pose.blends[2].normalizedFrame,0.001,"PlayBlendAnim no-tween nonzero frame quirk changed");
    Near(clock.blends[2].tweenRate,0,"No-tween blend rate changed");
    Apply(data,clock,Command(ActorAnimationCommandKind::PlayBlendAnim,"Next",1,-1,0,2));
    Near(clock.pose.blends[2].normalizedFrame,0,"Blend special -1 tween-time start changed");
    Near(clock.blends[2].tweenRate,10,"Blend special -1 tween-time rate changed");
    clock.blends[2].minRate=-0.1f;
    Apply(data,clock,Command(ActorAnimationCommandKind::PlayBlendAnim,"Next",1,-1,0,2),10);
    Near(clock.blends[2].tweenRate,1,"Blend velocity special tween-time rate changed");
    clock.blends[2].minRate=0.3f;
    Apply(data,clock,Command(ActorAnimationCommandKind::PlayBlendAnim,"Next",1,-1,0,2));
    Near(clock.blends[2].tweenRate,0.3,"Blend positive MinRate tween selection changed");

    const auto prior=clock.pose.blends[2].previous;
    const auto sim=clock.blends[2].simulated;
    const auto result=ApplyActorAnimationCommand(&data,clock,Command(ActorAnimationCommandKind::TweenBlendAnim,"Primary",1,0.5f,0,2));
    Require(result.applied && result.pinnedTweenBlendPositiveFrame,"Pinned positive TweenBlendAnim diagnostic missing");
    Near(clock.pose.blends[2].normalizedFrame,0.25,"Pinned positive TweenBlendAnim frame changed");
    Require(clock.pose.blends[2].previous.vertexOffset0==prior.vertexOffset0 &&
        clock.pose.blends[2].previous.vertexOffset1==prior.vertexOffset1 && clock.pose.blends[2].previous.fraction==prior.fraction,
        "Pinned TweenBlendAnim unexpectedly captures old history");
    Require(clock.blends[2].simulated==sim,"Pinned TweenBlendAnim rewrote SimBlendAnim");
    Require(clock.blends[2].last==0 && clock.blends[2].rate==0 && clock.blends[2].minRate==0,"TweenBlend scalar resets changed");
    const auto ignored=ApplyActorAnimationCommand(&data,clock,Command(ActorAnimationCommandKind::TweenBlendAnim,"Unknown",1,0,0,2));
    Require(!ignored.applied && ignored.error.empty(),"TweenBlendAnim unknown name must not use fallback");
    Apply(data,clock,Command(ActorAnimationCommandKind::PlayBlendAnim,"Single",1,0,0,1));
    Require(clock.pose.blends[1].normalizedFrame==0 && clock.blends[1].rate==0 && clock.blends[1].tweenRate==10,
        "Single-frame blend native properties changed");
}

void EventBoundaries() {
    const auto data=Data(); ActorAnimationClock clock;
    Apply(data,clock,Command(ActorAnimationCommandKind::PlayAnim,"Primary"));
    auto step=AdvanceMainAnimationBoundary(&data,clock,0.4f,0,HasNotify);
    Require(step.ok && step.eventReady && step.event.function=="Step","First native notify missing");
    Near(step.consumedSeconds,0.125,"First notify consumed wrong time");
    Near(step.remainingSeconds,0.275,"First notify residual time changed");
    Near(step.event.simulationTime,0.125,"First event simulation time changed");
    step=AdvanceMainAnimationBoundary(&data,clock,step.remainingSeconds,0,HasNotify);
    Require(step.eventReady && step.event.function=="Fire","Duplicate-time or absent notify was wrongly emitted");
    Near(step.event.normalizedFrame,0.6,"Second notify frame changed");
    Near(step.event.simulationTime,0.3,"Second notify simulation time changed");
    step=AdvanceMainAnimationBoundary(&data,clock,step.remainingSeconds,0,HasNotify);
    Require(step.eventReady && step.event.kind==ActorAnimationEventKind::AnimEnd,"Non-loop AnimEnd missing");
    Near(step.event.simulationTime,0.375,"Non-loop AnimEnd time changed");
    Require(clock.main.finished && clock.main.rate==0,"Non-loop completion flags missing");
    step=AdvanceMainAnimationBoundary(&data,clock,step.remainingSeconds,0,HasNotify);
    Require(step.ok && !step.eventReady && step.remainingSeconds==0,"Terminal frame did not account for stationary residual time");
    Near(clock.simulationTime,0.4,"Terminal tick time changed");

    ActorAnimationClock callback;
    Apply(data,callback,Command(ActorAnimationCommandKind::PlayAnim,"Primary"));
    std::vector<std::string> events;
    auto tick=AdvanceActorAnimationClock(&data,callback,0.6f,0,HasNotify,[&](const ActorAnimationEvent& event) {
        events.push_back(event.function);
        if(event.function=="Step") Apply(data,callback,Command(ActorAnimationCommandKind::PlayAnim,"Next"));
    });
    Require(tick.ok && !tick.budgetExhausted && events==std::vector<std::string>{"Step"},
        "Synchronous callback replacement did not prevent stale Primary notifies");
    Require(callback.pose.main.sequence=="Next","Actual callback command was lost");
    Near(callback.pose.main.normalizedFrame,0.475,"Callback replacement residual time was not advanced");

    ActorAnimationClock looping;
    Apply(data,looping,Command(ActorAnimationCommandKind::LoopAnim,"Next"));
    tick=AdvanceActorAnimationClock(&data,looping,1.4f,0,HasNotify,[](const ActorAnimationEvent&){});
    Require(tick.ok && tick.events.size()==1 && tick.events[0].kind==ActorAnimationEventKind::AnimEnd,
        "Loop AnimEnd count changed");
    Near(tick.events[0].simulationTime,0.75,"Loop AnimEnd must precede wrap");
    Near(looping.pose.main.normalizedFrame,0.4,"Loop wrap/remainder changed");
    Near(looping.simulationTime,1.4,"Loop time was dropped");

    auto atLast=data; atLast.sequences[0].notifies={{0.75f,"Fire"}};
    ActorAnimationClock sameBoundary; Apply(atLast,sameBoundary,Command(ActorAnimationCommandKind::LoopAnim,"Primary"));
    tick=AdvanceActorAnimationClock(&atLast,sameBoundary,0.5f,0,HasNotify,[](const ActorAnimationEvent&){});
    Require(tick.events.size()==1 && tick.events[0].kind==ActorAnimationEventKind::Notify,
        "Pinned notify-before-AnimLast strict comparison changed");

    ActorAnimationClock single;
    Apply(data,single,Command(ActorAnimationCommandKind::PlayAnim,"Single"));
    Apply(data,single,Command(ActorAnimationCommandKind::FinishAnim,""));
    tick=AdvanceActorAnimationClock(&data,single,0.3f,0,HasNotify,[](const ActorAnimationEvent&){});
    Require(tick.ok && tick.finishAnimReleased && tick.events.size()==1 && single.main.finished,
        "Single-frame tween finish/latent release changed");
    Near(tick.events[0].simulationTime,0.1,"Single-frame default minimum duration changed");

    // Pinned native history capture reads no packed vertices. A callable notify
    // at1 can issue the next real command immediately; preserve the original
    // wrapped offsets and post-modulo fraction even though the sampler must
    // subsequently diagnose that authored/native extrapolation as invalid.
    auto atOne=data; atOne.sequences[0].notifies={{1.0f,"Fire"}};
    auto metadataOnly=atOne; metadataOnly.frameVerticesPacked.clear();
    ActorAnimationClock finalNotify;
    Apply(metadataOnly,finalNotify,Command(ActorAnimationCommandKind::LoopAnim,"Primary"));
    ActorAnimationCommandResult followup;
    tick=AdvanceActorAnimationClock(&metadataOnly,finalNotify,0.5f,0,HasNotify,[&](const ActorAnimationEvent& event) {
        Require(event.function=="Fire" && event.normalizedFrame==1,"Final notify callback boundary changed");
        followup=ApplyActorAnimationCommand(&metadataOnly,finalNotify,
            Command(ActorAnimationCommandKind::PlayAnim,"Next",1,0.2f));
    });
    Require(tick.ok && tick.events.size()==1 && followup.applied && followup.error.empty(),
        "Final notify command required vertex data or was blocked by sampler range validation");
    Require(followup.capturedOriginalHistoryInvalid && !followup.capturedOriginalSpanInvalid,
        "Final native history extrapolation was not separately diagnosed");
    Require(finalNotify.pose.main.sequence=="Next" && finalNotify.pose.main.previous.vertexOffset0==0 &&
        finalNotify.pose.main.previous.vertexOffset1==3,"Final native history offsets did not wrap exactly");
    Near(finalNotify.pose.main.previous.fraction,4,"Pinned post-modulo final history fraction was fabricated");
    Near(finalNotify.pose.main.normalizedFrame,-0.25,"Final callback target tween command changed");
    const auto invalidPose=PrepareMeshPose(atOne,finalNotify.pose);
    Require(!invalidPose.drawable && invalidPose.error.find("history fraction")!=std::string::npos,
        "Invalid native final-frame history was silently sampled or clamped");

    ActorAnimationClock finalBlend;
    finalBlend.pose.blends[0].sequence="Primary"; finalBlend.pose.blends[0].normalizedFrame=1;
    followup=ApplyActorAnimationCommand(&metadataOnly,finalBlend,
        Command(ActorAnimationCommandKind::PlayBlendAnim,"Next",1,0.2f));
    Require(followup.applied && followup.capturedOriginalHistoryInvalid &&
        finalBlend.pose.blends[0].previous.vertexOffset0==0 && finalBlend.pose.blends[0].previous.vertexOffset1==3,
        "Final blend capture diverged from main metadata-only wrapping");
    Near(finalBlend.pose.blends[0].previous.fraction,4,"Final blend history fraction was clamped");

    ActorAnimationClock velocity;
    Apply(data,velocity,Command(ActorAnimationCommandKind::LoopAnim,"Next",-1,0,0.5f));
    tick=AdvanceActorAnimationClock(&data,velocity,0.2f,0.2f,HasNotify,[](const ActorAnimationEvent&){});
    Near(velocity.pose.main.normalizedFrame,0.1,"Velocity-scaled minimum normalized rate changed");
    velocity.pose.main.normalizedFrame=0.9f;
    Apply(data,velocity,Command(ActorAnimationCommandKind::FinishAnim,""));
    step=AdvanceMainAnimationBoundary(&data,velocity,0.2f,1,HasNotify);
    Require(step.ok && step.correctedPinnedPastEndElapsed && step.consumedSeconds==0 && step.remainingSeconds==0.2f,
        "FinishAnim past AnimLast created or debited negative time");
    Require(step.finishAnimReleased && velocity.main.finished && !step.eventReady,"FinishAnim past-last terminal properties changed");
}

void BlendClock() {
    const auto data=Data(); ActorAnimationClock clock;
    for(std::int32_t slot=0;slot<4;++slot) Apply(data,clock,Command(ActorAnimationCommandKind::PlayBlendAnim,"Next",1,0.2f,0,slot));
    auto result=AdvanceBlendAnimationClock(clock,1.0f,0);
    Require(result.ok && result.correctedPinnedBlendElapsed,"Pinned shared elapsed correction diagnostic missing");
    for(std::size_t slot=0;slot<4;++slot) {
        Near(clock.pose.blends[slot].normalizedFrame,0.75,"A completing blend starved a later slot or lost tween carry");
        Near(clock.blends[slot].rate,0,"Completed blend rate did not stop");
        Near(result.consumedSeconds[slot],1,"Blend slot elapsed was dropped");
        Near(result.remainingSeconds[slot],0,"Blend completion residual remains");
        Near(clock.blends[slot].simulated[2],7500,"Blend SimBlendAnim terminal frame changed");
        Near(clock.blends[slot].simulated[3],0,"Blend SimBlendAnim terminal rate changed");
    }
    Apply(data,clock,Command(ActorAnimationCommandKind::PlayBlendAnim,"Next",-10,0,0,0));
    clock.blends[0].minRate=999;
    result=AdvanceBlendAnimationClock(clock,0.5f,100);
    Near(clock.pose.blends[0].normalizedFrame,0.376,"Pinned negative blend-rate cap against Last changed");
    Apply(data,clock,Command(ActorAnimationCommandKind::TweenBlendAnim,"Next",1,0.2f,0,1));
    const float frame=clock.pose.blends[1].normalizedFrame;
    result=AdvanceBlendAnimationClock(clock,1,0);
    Near(clock.pose.blends[1].normalizedFrame,frame,"Pinned positive TweenBlendAnim quirk was silently fixed");
}

void BudgetsAndFailure() {
    const auto data=Data(); ActorAnimationClock clock;
    Apply(data,clock,Command(ActorAnimationCommandKind::LoopAnim,"Next"));
    ActorAnimationClockLimits limits; limits.maxSubsteps=1;
    auto tick=AdvanceActorAnimationClock(&data,clock,1.4f,0,HasNotify,[](const ActorAnimationEvent&){},limits);
    Require(tick.ok && tick.budgetExhausted && tick.events.size()==1,"Substep budget did not yield explicitly");
    Near(tick.consumedSeconds,0.75,"Budget processed prefix changed");
    Near(tick.remainingSeconds,0.65,"Budget lost unchecked time");
    Near(tick.pendingBlendSeconds,1.4,"Budget lost deferred blend time");
    Near(clock.simulationTime,0.75,"Budget advanced time past verified prefix");
    // Resume the residual BEFORE accepting another simulation delta; run blends
    // once for the original tick's pending time, not twice for resumed fragments.
    auto remaining=tick.remainingSeconds;
    while(remaining>0) {
        const auto step=AdvanceMainAnimationBoundary(&data,clock,remaining,0,HasNotify);
        Require(step.ok,"Resuming bounded main time failed"); remaining=step.remainingSeconds;
    }
    Require(AdvanceBlendAnimationClock(clock,tick.pendingBlendSeconds,0).ok,"Deferred blends failed");
    Near(clock.simulationTime,1.4,"Resuming bounded time did not reach original simulation time");
    Near(clock.pose.main.normalizedFrame,0.4,"Resuming bounded time changed loop pose");

    ActorAnimationClock notify;
    Apply(data,notify,Command(ActorAnimationCommandKind::PlayAnim,"Primary"));
    limits={}; limits.maxNotifySearches=0;
    auto step=AdvanceMainAnimationBoundary(&data,notify,0.3f,0,HasNotify,limits);
    Require(step.ok && step.budgetExhausted && !step.eventReady && step.consumedSeconds==0 && step.remainingSeconds==0.3f,
        "Notify-search budget consumed unchecked time");
    Near(notify.pose.main.normalizedFrame,0,"Notify budget committed partial state");
    Near(notify.simulationTime,0,"Notify budget advanced simulation time");

    auto waiting=notify; waiting.pose.main.normalizedFrame=0.8f; waiting.main.finishAnimWaiting=true;
    const auto waitingBefore=waiting;
    step=AdvanceMainAnimationBoundary(&data,waiting,0.3f,0,HasNotify,limits);
    Require(step.ok && step.budgetExhausted && !step.finishAnimReleased && !step.eventReady,
        "Notify-search budget reported an uncommitted latent wait release");
    Require(waiting.main.finishAnimWaiting && waiting.pose.main.normalizedFrame==waitingBefore.pose.main.normalizedFrame &&
        waiting.simulationTime==waitingBefore.simulationTime,"Notify budget partially committed latent or clock state");

    for(const float time : {-0.01f,1.01f}) {
        auto invalidNotify=data; invalidNotify.sequences[0].notifies={{time,"Fire"}};
        ActorAnimationClock invalidClock; Apply(invalidNotify,invalidClock,Command(ActorAnimationCommandKind::PlayAnim,"Primary"));
        step=AdvanceMainAnimationBoundary(&invalidNotify,invalidClock,0.3f,0,HasNotify);
        Require(!step.ok && !step.eventReady && !step.finishAnimReleased &&
            step.error.find("notify time is outside")!=std::string::npos,
            "Out-of-range authored notify did not fail explicitly before committing a sampled boundary");
        Require(step.consumedSeconds==0 && step.remainingSeconds==0.3f && invalidClock.pose.main.normalizedFrame==0 &&
            invalidClock.simulationTime==0,"Invalid notify committed unchecked elapsed or frame state");
    }

    const auto before=notify;
    auto bad=Command(ActorAnimationCommandKind::PlayAnim,"Next"); bad.rate=std::numeric_limits<float>::quiet_NaN();
    auto command=ApplyActorAnimationCommand(&data,notify,bad);
    Require(!command.applied && !command.error.empty() && notify.pose.main.sequence==before.pose.main.sequence,
        "Invalid command was applied");
    command=ApplyActorAnimationCommand(&data,notify,Command(ActorAnimationCommandKind::PlayBlendAnim,"Next",1,0,0,4));
    Require(!command.applied && !command.error.empty(),"Out-of-range blend native slot accepted");
    step=AdvanceMainAnimationBoundary(&data,notify,-1,0,HasNotify);
    Require(!step.ok && !step.error.empty() && notify.simulationTime==before.simulationTime,"Negative elapsed time accepted");
    step=AdvanceMainAnimationBoundary(&data,notify,0.1f,std::numeric_limits<float>::infinity(),HasNotify);
    Require(!step.ok && !step.error.empty(),"Non-finite velocity speed accepted");
    auto underflow=notify; underflow.pose.main.normalizedFrame=0.9f;
    underflow.main.rate=std::numeric_limits<float>::denorm_min(); underflow.main.notify=false;
    step=AdvanceMainAnimationBoundary(&data,underflow,0.1f,0,HasNotify);
    Require(!step.ok && !step.correctedPinnedPastEndElapsed && !step.eventReady && !step.finishAnimReleased &&
        step.consumedSeconds==0 && step.remainingSeconds==0.1f && underflow.pose.main.normalizedFrame==0.9f,
        "Failed boundary exposed an uncommitted past-end correction or state");
    auto invalid=notify; invalid.pose.blends[0].sequence="Next"; invalid.blends[0].rate=std::numeric_limits<float>::quiet_NaN();
    const auto blend=AdvanceBlendAnimationClock(invalid,0.1f,0);
    Require(!blend.ok && !blend.error.empty(),"Non-finite blend clock accepted");

    // Full clock copies (save state) include native tween history, not merely
    // the authored snapshot's AnimFrame and sequence.
    ActorAnimationClock saved;
    Apply(data,saved,Command(ActorAnimationCommandKind::PlayAnim,"Primary"));
    AdvanceMainAnimationBoundary(&data,saved,0.06f,0,HasNotify);
    Apply(data,saved,Command(ActorAnimationCommandKind::PlayAnim,"Next",1,0.2f));
    auto restored=saved;
    const auto first=AdvanceActorAnimationClock(&data,saved,0.3f,0,HasNotify,[](const ActorAnimationEvent&){});
    const auto second=AdvanceActorAnimationClock(&data,restored,0.3f,0,HasNotify,[](const ActorAnimationEvent&){});
    Require(first.ok && second.ok && first.events.size()==second.events.size(),"Copied clock continuation differs");
    Near(saved.pose.main.normalizedFrame,restored.pose.main.normalizedFrame,"Copied tween continuation pose differs");
    Near(saved.pose.main.previous.fraction,restored.pose.main.previous.fraction,"Copied tween history differs");
    Near(saved.simulationTime,restored.simulationTime,"Copied clock simulation time differs");
}
}

int main() {
    try {
        QueriesAndCommands(); BlendCommands(); EventBoundaries(); BlendClock(); BudgetsAndFailure();
        std::cout<<"PASS actor animation command/clock controls="<<checks
            <<"; actual VM startup/AI/Quest GPU not verified\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL actor animation command/clock: "<<error.what()<<" after "<<checks<<" controls\n";
        return 1;
    }
}
