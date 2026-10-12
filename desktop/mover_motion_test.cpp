#include "quest_mover_motion.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace QuestVr;
std::size_t checks{},refusals{};
void Require(const bool yes,const char* message) {
    if (!yes) throw std::runtime_error(message);
    ++checks;
}
void Near(const float actual,const float expected,const char* message) {
    Require(std::isfinite(actual) && std::fabs(actual-expected)<=0.0001f,message);
}
void Pose(const MoverPose& actual,const MoverPose& expected,const char* message) {
    Near(actual.location.x,expected.location.x,message);
    Near(actual.location.y,expected.location.y,message);
    Near(actual.location.z,expected.location.z,message);
    Require(actual.rotation.pitch==expected.rotation.pitch && actual.rotation.yaw==expected.rotation.yaw &&
        actual.rotation.roll==expected.rotation.roll,message);
}
bool Same(const MoverMotionState& a,const MoverMotionState& b) {
    const auto pose=[](const MoverPose& x,const MoverPose& y) {
        return x.location.x==y.location.x && x.location.y==y.location.y && x.location.z==y.location.z &&
            x.rotation.pitch==y.rotation.pitch && x.rotation.yaw==y.rotation.yaw && x.rotation.roll==y.rotation.roll;
    };
    return pose(a.pose,b.pose) && pose(a.sourcePose,b.sourcePose) && a.targetKey==b.targetKey &&
        a.returnKey==b.returnKey && a.phase==b.phase && a.elapsedSeconds==b.elapsedSeconds &&
        a.remainingStaySeconds==b.remainingStaySeconds;
}
void Refused(const std::function<void()>& action,const char* message) {
    bool refused{};
    try { action(); } catch (const std::runtime_error&) { refused=true; }
    Require(refused,message); ++refusals;
}
MoverMotionSpec Fixture() {
    MoverMotionSpec spec;
    spec.base={{100,-50,25},{100,200,-300}};
    spec.keys[0]={{1,2,3},{7,8,9}};
    spec.keys[1]={{33,66,99},{16391,32776,-16375}};
    spec.moveTime=2.0f; spec.stayOpenTime=3.0f;
    return spec;
}
void AuthoredTargetsAndGlide() {
    auto spec=Fixture();
    const auto key0=MoverPoseAtKey(spec,0u),key1=MoverPoseAtKey(spec,1u);
    Pose(key0,{{101,-48,28},{107,208,-291}},"Base pose plus key0 offsets differs");
    Pose(key1,{{133,16,124},{16491,32976,-16675}},"Base pose plus key1 offsets differs");
    auto state=MakeMoverMotionState(spec);
    Require(state.phase==MoverMotionPhase::Idle && state.targetKey==0u,"Initial state is not closed key0");
    Require(RequestMoverMotionKey(spec,state,1u),"Open key command did not start");
    Pose(state.pose,key0,"Opening jumped on command");
    const auto started=state;
    Require(!AdvanceMoverMotion(spec,state,0.0f) && Same(state,started),"Zero delta changed moving state");
    Require(AdvanceMoverMotion(spec,state,0.5f),"Quarter-time glide did not advance");
    // Pinned TickMovingBrush applies smoothstep(0,1,alpha), not linear motion.
    Pose(state.pose,{{106,-38,43},{2667,5328,-2851}},"Smooth glide quarter-time pose differs");
    const auto quarter=state;
    Require(!RequestMoverMotionKey(spec,state,1u) && Same(state,quarter),"Repeated open reset in-flight progress");
    AdvanceMoverMotion(spec,state,0.5f);
    Pose(state.pose,{{117,-16,76},{8299,16592,-8483}},"Smooth glide midpoint differs");
    AdvanceMoverMotion(spec,state,1.0f);
    Pose(state.pose,key1,"Opening endpoint differs");
    Require(state.phase==MoverMotionPhase::Idle && state.elapsedSeconds==0.0f,"Open endpoint did not idle");
    const auto idle=state;
    Require(!RequestMoverMotionKey(spec,state,1u) && Same(state,idle),"Repeated idle key command altered checkpoint state");
    Require(!AdvanceMoverMotion(spec,state,1000.0f),"Untimed open key moved spontaneously");
    Require(RequestMoverMotionKey(spec,state,0u),"Close key command did not start");
    AdvanceMoverMotion(spec,state,2.0f);
    Pose(state.pose,key0,"Closing endpoint differs");
    spec.smooth=false;
    state=MakeMoverMotionState(spec);
    RequestMoverMotionKey(spec,state,1u);
    AdvanceMoverMotion(spec,state,0.5f);
    Pose(state.pose,{{109,-32,52},{4203,8400,-4387}},"Linear glide was silently smoothed");
}
void TimedReturnAndReversal() {
    auto spec=Fixture(); auto state=MakeMoverMotionState(spec);
    const auto closed=state.pose;
    RequestMoverMotionKey(spec,state,1u,0);
    AdvanceMoverMotion(spec,state,2.0f);
    Require(state.phase==MoverMotionPhase::Holding && state.remainingStaySeconds==3.0f,"Opening did not enter authored wait");
    const auto opened=state.pose;
    AdvanceMoverMotion(spec,state,1.0f);
    Require(state.remainingStaySeconds==2.0f,"StayOpenTime did not decrease");
    Require(RequestMoverMotionKey(spec,state,1u,0) && state.remainingStaySeconds==3.0f,"Repeated use did not renew held door");
    AdvanceMoverMotion(spec,state,3.5f);
    Require(state.phase==MoverMotionPhase::Moving && state.targetKey==0u && state.returnKey==-1 &&
        state.elapsedSeconds==0.5f,"Wait boundary lost remaining time on return trip");
    const auto closing=state.pose;
    Require(RequestMoverMotionKey(spec,state,1u),"Mid-close reversal refused");
    Pose(state.pose,closing,"Mid-close reversal jumped pose");
    Pose(state.sourcePose,closing,"Reversal did not retain its transition source");
    AdvanceMoverMotion(spec,state,2.0f);
    Pose(state.pose,opened,"Reopened endpoint differs");
    RequestMoverMotionKey(spec,state,0u);
    AdvanceMoverMotion(spec,state,0.5f);
    const auto before=state;
    auto restored=before; // All required transition data is plain saved state.
    AdvanceMoverMotion(spec,state,0.75f); AdvanceMoverMotion(spec,restored,0.75f);
    Require(Same(state,restored),"Copied mid-motion checkpoint does not resume identically");
    RequestMoverMotionKey(spec,state,1u,0);
    AdvanceMoverMotion(spec,state,std::numeric_limits<float>::max());
    Require(state.phase==MoverMotionPhase::Idle && state.targetKey==0u,"Large finite delta did not settle bounded timed return");
    Pose(state.pose,closed,"Large finite delta missed return endpoint");
    state=MakeMoverMotionState(spec);
    RequestMoverMotionKey(spec,state,1u,0); AdvanceMoverMotion(spec,state,7.0f);
    Require(state.phase==MoverMotionPhase::Idle && state.targetKey==0u,"Exact full cycle duration missed closed endpoint");
    Pose(state.pose,closed,"Exact timed cycle endpoint differs");
    spec.stayOpenTime=0.0f;
    state=MakeMoverMotionState(spec); RequestMoverMotionKey(spec,state,1u,0);
    AdvanceMoverMotion(spec,state,2.5f);
    Require(state.phase==MoverMotionPhase::Moving && state.targetKey==0u && state.elapsedSeconds==0.5f,
        "Zero wait was not handled without an extra frame");
    spec.moveTime=0.0f;
    state=MakeMoverMotionState(spec); RequestMoverMotionKey(spec,state,1u,0);
    Pose(state.pose,MoverPoseAtKey(spec,1u),"Instantaneous movement missed requested key");
    AdvanceMoverMotion(spec,state,0.0f);
    Require(state.phase==MoverMotionPhase::Idle && state.targetKey==0u,"Zero-duration cycle did not terminate");
    Pose(state.pose,MoverPoseAtKey(spec,0u),"Zero-duration cycle return endpoint differs");
}
void RotationAndMultipleKeys() {
    MoverMotionSpec spec; spec.smooth=false; spec.moveTime=2.0f; spec.keyCount=8u;
    spec.base.rotation={std::numeric_limits<std::int32_t>::max(),0,0};
    spec.keys[0].rotation={1,0,0};
    Require(MoverPoseAtKey(spec,0).rotation.pitch==std::numeric_limits<std::int32_t>::min(),
        "Rotator endpoint addition did not wrap 32-bit storage safely");
    spec.base.rotation={0,0,0};
    spec.keys[0].rotation={0,65000,0};
    spec.keys[1].rotation={65536,0,-65536};
    auto state=MakeMoverMotionState(spec);
    RequestMoverMotionKey(spec,state,1u); AdvanceMoverMotion(spec,state,1.0f);
    Require(state.pose.rotation.pitch==32768 && state.pose.rotation.yaw==32500 && state.pose.rotation.roll==-32768,
        "Raw authored rotation was incorrectly reduced to shortest angle");
    for (std::uint8_t i=2u;i<spec.keyCount;++i) {
        spec.keys[i].position={static_cast<float>(i*10u),static_cast<float>(-static_cast<int>(i)),1.0f};
        spec.keys[i].rotation={i,static_cast<std::int32_t>(i*8192u),-static_cast<std::int32_t>(i)};
    }
    for (std::uint8_t i=0u;i<spec.keyCount;++i) {
        RequestMoverMotionKey(spec,state,i); AdvanceMoverMotion(spec,state,2.0f);
        Pose(state.pose,MoverPoseAtKey(spec,i),"Generic authored key endpoint differs");
        ValidateMoverMotionState(spec,state);
    }
    // Repeated small ticks and copied checkpoints remain valid and monotonic.
    state=MakeMoverMotionState(spec); RequestMoverMotionKey(spec,state,7u,0);
    for (unsigned i=0u;i<1200u;++i) {
        ValidateMoverMotionState(spec,state);
        auto copy=state;
        AdvanceMoverMotion(spec,state,0.01f); AdvanceMoverMotion(spec,copy,0.01f);
        Require(Same(state,copy),"Small-tick checkpoint was nondeterministic");
    }
    Require(state.phase==MoverMotionPhase::Idle && state.targetKey==0u,"Small tick sequence did not finish one timed cycle");
}
void ValidationAndAtomicRefusal() {
    const auto good=Fixture();
    const float nan=std::numeric_limits<float>::quiet_NaN(), inf=std::numeric_limits<float>::infinity();
    for (const float bad : {nan,inf,-1.0f,86401.0f}) {
        auto spec=good; spec.moveTime=bad;
        Refused([&] { ValidateMoverMotionSpec(spec); },"Invalid MoveTime accepted");
        spec=good; spec.stayOpenTime=bad;
        Refused([&] { ValidateMoverMotionSpec(spec); },"Invalid StayOpenTime accepted");
    }
    for (const std::uint8_t count : {0u,1u,9u,255u}) {
        auto spec=good; spec.keyCount=count;
        Refused([&] { MakeMoverMotionState(spec); },"Invalid NumKeys accepted");
    }
    auto spec=good; spec.base.location.x=inf;
    Refused([&] { MoverPoseAtKey(spec,0); },"Non-finite base accepted");
    spec=good; spec.keys[1].position.z=nan;
    Refused([&] { MoverPoseAtKey(spec,0); },"Non-finite used key accepted");
    spec=good; spec.base.location.x=std::numeric_limits<float>::max(); spec.keys[1].position.x=spec.base.location.x;
    Refused([&] { ValidateMoverMotionSpec(spec); },"Overflowing key target accepted");
    Refused([&] { MoverPoseAtKey(good,2); },"Out-of-range key accepted");
    auto state=MakeMoverMotionState(good); const auto before=state;
    for (const float bad : {nan,inf,-1.0f}) {
        Refused([&] { AdvanceMoverMotion(good,state,bad); },"Invalid elapsed accepted");
        Require(Same(state,before),"Invalid elapsed mutated state");
    }
    Refused([&] { RequestMoverMotionKey(good,state,2u); },"Invalid requested key accepted");
    Require(Same(state,before),"Invalid requested key mutated state");
    for (const std::int8_t key : {-2,1,2,127}) {
        Refused([&] { RequestMoverMotionKey(good,state,1u,key); },"Invalid requested return key accepted");
        Require(Same(state,before),"Invalid return key mutated state");
    }
    const auto badState=[&](const std::function<void(MoverMotionState&)>& corrupt) {
        auto invalid=MakeMoverMotionState(good); corrupt(invalid);
        Refused([&] { ValidateMoverMotionState(good,invalid); },"Corrupt persisted state accepted");
    };
    badState([&](auto& value) { value.pose.location.x=nan; });
    badState([&](auto& value) { value.sourcePose.location.z=inf; });
    badState([&](auto& value) { value.elapsedSeconds=nan; });
    badState([&](auto& value) { value.remainingStaySeconds=inf; });
    badState([](auto& value) { value.targetKey=2; });
    badState([](auto& value) { value.returnKey=2; });
    badState([](auto& value) { value.returnKey=0; });
    badState([](auto& value) { value.phase=static_cast<MoverMotionPhase>(255); });
    badState([](auto& value) { value.elapsedSeconds=0.1f; });
    badState([](auto& value) { value.pose.location.x+=1.0f; });
    badState([](auto& value) { value.pose.rotation.yaw+=1; });
    badState([](auto& value) { value.phase=MoverMotionPhase::Holding; });
    badState([](auto& value) { value.phase=MoverMotionPhase::Moving; value.elapsedSeconds=2.0f; });
    RequestMoverMotionKey(good,state,1u); AdvanceMoverMotion(good,state,0.5f);
    auto invalid=state; invalid.pose.location.x+=1.0f;
    Refused([&] { ValidateMoverMotionState(good,invalid); },"Inconsistent moving sample accepted");
    invalid=state; invalid.remainingStaySeconds=0.5f;
    Refused([&] { ValidateMoverMotionState(good,invalid); },"Moving state with hold timer accepted");
}
} // namespace

int main() {
    try {
        AuthoredTargetsAndGlide(); TimedReturnAndReversal(); RotationAndMultipleKeys(); ValidationAndAtomicRefusal();
        std::cout<<"Mover motion: "<<checks<<" checks, "<<refusals<<" expected refusals\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"Mover motion test failed: "<<error.what()<<"\n";
        return 1;
    }
}
