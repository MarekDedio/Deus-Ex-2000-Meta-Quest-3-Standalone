#pragma once

#include "quest_actor_transform.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace QuestVr {

// A bounded motion primitive for the tutorial interaction layer. This is NOT
// the original Mover script state machine, encroachment, events or networking.
// Authored targets/glide are based on the pinned SurrealEngine implementation:
// Packages/Engine/Actors/UActor_PhysMovingBrush.cpp (BasePos+KeyPos,
// BaseRot+KeyRot, smoothstep for MV_GlideByTime) and Brush/UMover.h (8 keys).
// PrePivot stays with BuildBrushToQuest: it is subtracted in brush-local space,
// and must not be folded into either BasePos or the world-axis KeyPos offsets.
struct MoverRotator { std::int32_t pitch{}, yaw{}, roll{}; };
struct MoverPose { ActorVec3 location; MoverRotator rotation; };
struct MoverKey { ActorVec3 position; MoverRotator rotation; };
struct MoverMotionSpec {
    MoverPose base;
    std::array<MoverKey,8> keys{};
    std::uint8_t keyCount{2};
    float moveTime{1.0f}, stayOpenTime{4.0f};
    bool smooth{true}; // MV_GlideByTime=1; otherwise constant-speed glide.
};
enum class MoverMotionPhase : std::uint8_t { Idle, Moving, Holding };
struct MoverMotionState {
    // All state required to resume a reversible movement after a checkpoint.
    MoverPose pose, sourcePose;
    std::uint8_t targetKey{};
    std::int8_t returnKey{-1}; // -1 means remain at the requested key.
    MoverMotionPhase phase{MoverMotionPhase::Idle};
    float elapsedSeconds{}, remainingStaySeconds{};
};

namespace MoverMotionDetail {
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(reason);
}
inline std::int32_t Wrap(const std::int64_t value) {
    const auto bits=static_cast<std::uint32_t>(value);
    return bits<0x80000000u ? static_cast<std::int32_t>(bits)
        : static_cast<std::int32_t>(static_cast<std::int64_t>(bits)-0x100000000ll);
}
inline std::int32_t Add(const std::int32_t a,const std::int32_t b) {
    return Wrap(static_cast<std::int64_t>(a)+b);
}
inline bool SameRotation(const MoverRotator& a,const MoverRotator& b) {
    return a.pitch==b.pitch && a.yaw==b.yaw && a.roll==b.roll;
}
inline bool SamePose(const MoverPose& a,const MoverPose& b) {
    return a.location.x==b.location.x && a.location.y==b.location.y &&
        a.location.z==b.location.z && SameRotation(a.rotation,b.rotation);
}
inline void ValidatePose(const MoverPose& pose) {
    if (!IsFiniteActorVector(pose.location)) Fail("Mover pose is non-finite");
}
inline std::int32_t MixAngle(const std::int32_t from,const std::int32_t to,const float t) {
    // Deliberately not shortest-angle interpolation: authored full turns must
    // remain full turns. Rotator arithmetic has 32-bit component storage.
    const auto difference=Wrap(static_cast<std::int64_t>(to)-from);
    const auto delta=static_cast<std::int64_t>(static_cast<double>(difference)*t);
    return Wrap(static_cast<std::int64_t>(from)+delta);
}
inline MoverPose Mix(const MoverPose& from,const MoverPose& to,const float alpha,const bool smooth) {
    if (alpha<=0.0f) return from;
    if (alpha>=1.0f) return to;
    const float t=smooth ? alpha*alpha*(3.0f-2.0f*alpha) : alpha;
    const auto scalar=[t](const float a,const float b) {
        return static_cast<float>(static_cast<double>(a)+
            (static_cast<double>(b)-a)*t);
    };
    MoverPose result{{scalar(from.location.x,to.location.x),
        scalar(from.location.y,to.location.y),scalar(from.location.z,to.location.z)},
        {MixAngle(from.rotation.pitch,to.rotation.pitch,t),
         MixAngle(from.rotation.yaw,to.rotation.yaw,t),
         MixAngle(from.rotation.roll,to.rotation.roll,t)}};
    ValidatePose(result);
    return result;
}
inline bool NearScalar(const float a,const float b) {
    // Source/pose may have crossed host/Android save boundaries. Require the
    // sampled position, allowing only a tiny float rounding difference.
    const double scale=1.0+std::max(std::fabs(static_cast<double>(a)),std::fabs(static_cast<double>(b)));
    return std::fabs(static_cast<double>(a)-b)<=scale*0.0000005;
}
inline bool EquivalentPose(const MoverPose& a,const MoverPose& b) {
    return NearScalar(a.location.x,b.location.x) && NearScalar(a.location.y,b.location.y) &&
        NearScalar(a.location.z,b.location.z) && SameRotation(a.rotation,b.rotation);
}
} // namespace MoverMotionDetail

inline void ValidateMoverMotionSpec(const MoverMotionSpec& spec) {
    using namespace MoverMotionDetail;
    ValidatePose(spec.base);
    if (spec.keyCount<2u || spec.keyCount>spec.keys.size()) Fail("Mover key count must be 2..8");
    // A zero-duration command is explicitly instantaneous in this primitive.
    // The ceiling bounds persisted timers; it is not an original engine limit.
    if (!std::isfinite(spec.moveTime) || spec.moveTime<0.0f || spec.moveTime>86400.0f ||
        !std::isfinite(spec.stayOpenTime) || spec.stayOpenTime<0.0f || spec.stayOpenTime>86400.0f)
        Fail("Mover duration is invalid");
    for (std::size_t i=0u;i<spec.keyCount;++i) {
        if (!IsFiniteActorVector(spec.keys[i].position)) Fail("Mover key position is non-finite");
        const ActorVec3 target{spec.base.location.x+spec.keys[i].position.x,
            spec.base.location.y+spec.keys[i].position.y,spec.base.location.z+spec.keys[i].position.z};
        if (!IsFiniteActorVector(target)) Fail("Mover target position overflow");
    }
}

inline MoverPose MoverPoseAtKey(const MoverMotionSpec& spec,const std::uint8_t key) {
    ValidateMoverMotionSpec(spec);
    if (key>=spec.keyCount) MoverMotionDetail::Fail("Mover key index is invalid");
    const auto& offset=spec.keys[key];
    return {{spec.base.location.x+offset.position.x,spec.base.location.y+offset.position.y,
        spec.base.location.z+offset.position.z},
        {MoverMotionDetail::Add(spec.base.rotation.pitch,offset.rotation.pitch),
         MoverMotionDetail::Add(spec.base.rotation.yaw,offset.rotation.yaw),
         MoverMotionDetail::Add(spec.base.rotation.roll,offset.rotation.roll)}};
}

inline void ValidateMoverMotionState(const MoverMotionSpec& spec,const MoverMotionState& state) {
    using namespace MoverMotionDetail;
    const auto target=MoverPoseAtKey(spec,state.targetKey);
    ValidatePose(state.pose); ValidatePose(state.sourcePose);
    if (state.returnKey < -1 || state.returnKey>=static_cast<std::int8_t>(spec.keyCount) ||
        state.returnKey==static_cast<std::int8_t>(state.targetKey)) Fail("Mover return key is invalid");
    if (!std::isfinite(state.elapsedSeconds) || state.elapsedSeconds<0.0f ||
        state.elapsedSeconds>spec.moveTime || !std::isfinite(state.remainingStaySeconds) ||
        state.remainingStaySeconds<0.0f || state.remainingStaySeconds>spec.stayOpenTime)
        Fail("Mover state timer is invalid");
    switch (state.phase) {
    case MoverMotionPhase::Idle:
        if (state.elapsedSeconds!=0.0f || state.remainingStaySeconds!=0.0f || state.returnKey!=-1 ||
            !EquivalentPose(state.pose,target)) Fail("Idle mover state is inconsistent");
        break;
    case MoverMotionPhase::Moving:
        if (spec.moveTime<=0.0f || state.elapsedSeconds>=spec.moveTime || state.remainingStaySeconds!=0.0f ||
            !EquivalentPose(state.pose,Mix(state.sourcePose,target,state.elapsedSeconds/spec.moveTime,spec.smooth)))
            Fail("Moving mover state is inconsistent");
        break;
    case MoverMotionPhase::Holding:
        if (state.elapsedSeconds!=0.0f || state.returnKey<0 || !EquivalentPose(state.pose,target))
            Fail("Holding mover state is inconsistent");
        break;
    default: Fail("Mover phase is invalid");
    }
}

inline MoverMotionState MakeMoverMotionState(const MoverMotionSpec& spec,const std::uint8_t initialKey=0u) {
    MoverMotionState state;
    state.pose=state.sourcePose=MoverPoseAtKey(spec,initialKey);
    state.targetKey=initialKey;
    return state;
}

// Returns true when command/state changed. A reversal starts from the exact
// current pose; each new trip uses MoveTime, not an inferred original script
// duration. Repeating an in-flight command does not restart it. Repeating a
// holding command resets its wait, useful for a player still using the door.
inline bool RequestMoverMotionKey(const MoverMotionSpec& spec,MoverMotionState& state,
    const std::uint8_t key,const std::int8_t returnKey=-1) {
    using namespace MoverMotionDetail;
    ValidateMoverMotionState(spec,state);
    const auto target=MoverPoseAtKey(spec,key);
    if (returnKey < -1 || returnKey>=static_cast<std::int8_t>(spec.keyCount) ||
        returnKey==static_cast<std::int8_t>(key)) Fail("Mover requested return key is invalid");
    auto next=state;
    if (state.targetKey==key && state.returnKey==returnKey && state.phase==MoverMotionPhase::Moving)
        return false;
    if (state.targetKey==key && returnKey==-1 && state.phase==MoverMotionPhase::Idle)
        return false;
    if (SamePose(state.pose,target)) {
        next.sourcePose=target; next.pose=target; next.targetKey=key; next.returnKey=returnKey;
        next.elapsedSeconds=0.0f;
        next.remainingStaySeconds=returnKey<0 ? 0.0f : spec.stayOpenTime;
        next.phase=returnKey<0 ? MoverMotionPhase::Idle : MoverMotionPhase::Holding;
    } else {
        next.sourcePose=state.pose; next.targetKey=key; next.returnKey=returnKey;
        next.elapsedSeconds=next.remainingStaySeconds=0.0f;
        if (spec.moveTime==0.0f) {
            next.pose=target;
            next.remainingStaySeconds=returnKey<0 ? 0.0f : spec.stayOpenTime;
            next.phase=returnKey<0 ? MoverMotionPhase::Idle : MoverMotionPhase::Holding;
        } else next.phase=MoverMotionPhase::Moving;
    }
    ValidateMoverMotionState(spec,next);
    const bool changed=next.targetKey!=state.targetKey || next.returnKey!=state.returnKey ||
        next.phase!=state.phase || next.elapsedSeconds!=state.elapsedSeconds ||
        next.remainingStaySeconds!=state.remainingStaySeconds || !SamePose(next.pose,state.pose) ||
        !SamePose(next.sourcePose,state.sourcePose);
    state=next;
    return changed;
}

// Consumes an arbitrary finite delta with constant work: at most the current
// trip, one wait, and one return trip. No cyclic state-machine loop or callback
// occurs here. Refusals are atomic and leave the caller's state unchanged.
inline bool AdvanceMoverMotion(const MoverMotionSpec& spec,MoverMotionState& state,const float seconds) {
    using namespace MoverMotionDetail;
    ValidateMoverMotionState(spec,state);
    if (!std::isfinite(seconds) || seconds<0.0f) Fail("Mover elapsed time is invalid");
    auto next=state;
    double left=seconds;
    bool changed=false;
    for (unsigned boundary=0u;boundary<3u;++boundary) {
        if (next.phase==MoverMotionPhase::Idle) break;
        if (next.phase==MoverMotionPhase::Holding) {
            if (left<next.remainingStaySeconds) {
                if (left>0.0) { next.remainingStaySeconds-=static_cast<float>(left); changed=true; }
                break;
            }
            left-=next.remainingStaySeconds;
            const auto returnKey=static_cast<std::uint8_t>(next.returnKey);
            next.phase=MoverMotionPhase::Idle; next.returnKey=-1; next.remainingStaySeconds=0.0f;
            RequestMoverMotionKey(spec,next,returnKey);
            changed=true;
            continue;
        }
        const double remaining=static_cast<double>(spec.moveTime)-next.elapsedSeconds;
        if (left<remaining) {
            if (left>0.0) {
                // Keep an unfinished persisted state strictly below its endpoint.
                const float elapsed=static_cast<float>(next.elapsedSeconds+left);
                if (elapsed>=spec.moveTime) {
                    next.elapsedSeconds=std::nextafter(spec.moveTime,0.0f);
                } else next.elapsedSeconds=elapsed;
                next.pose=Mix(next.sourcePose,MoverPoseAtKey(spec,next.targetKey),
                    next.elapsedSeconds/spec.moveTime,spec.smooth);
                changed=true;
            }
            break;
        }
        left-=remaining;
        next.pose=MoverPoseAtKey(spec,next.targetKey);
        next.elapsedSeconds=0.0f;
        next.remainingStaySeconds=next.returnKey<0 ? 0.0f : spec.stayOpenTime;
        next.phase=next.returnKey<0 ? MoverMotionPhase::Idle : MoverMotionPhase::Holding;
        changed=true;
    }
    ValidateMoverMotionState(spec,next);
    state=next;
    return changed;
}

} // namespace QuestVr
