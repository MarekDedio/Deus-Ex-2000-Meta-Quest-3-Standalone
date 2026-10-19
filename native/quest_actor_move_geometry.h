#pragma once

#include "quest_actor_line_trace.h"

#include <bit>
#include <cstdint>

namespace QuestVr {

// Numeric packets only. These functions do not move an Actor, select a hit,
// update a hash, run a callback, or establish world/physics readiness.
struct OriginalActorMoveGeometry {
    bool small{};
    float size{};
    PortableModelVec3 direction{},extended{};
};
struct OriginalActorMoveBackoff {
    PortableModelVec3 delta{};
    float time{};
};

namespace ActorMoveGeometryDetail {
using ActorLineTraceDetail::Load;
using ActorLineTraceDetail::Sub;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;
using ScriptRandom::VectorDetail::DecodeDouble;
using ScriptRandom::VectorDetail::StoreDouble;

inline constexpr double SmallThreshold=std::bit_cast<double>(std::uint64_t{0x3f1a36e2eb1c432dull});
inline bool Small(float value) {
    auto number=Load(value);
    if(number.negative)number=Negate(number);
    return Compare64(number,DecodeDouble(SmallThreshold))<0;
}
inline bool NeedsBackoff(float time) {
    const auto bits=std::bit_cast<std::uint32_t>(time);
    const bool nan=(bits&0x7f800000u)==0x7f800000u&&(bits&0x7fffffu)!=0u;
    return nan||time<1.0f; // Original FCOMP/TEST C0, including unordered.
}
} // namespace ActorMoveGeometryDetail

// Both native small-motion tests inspect X, then Y, then Z and stop at the
// first component outside the strict Double threshold. Do not eagerly reject
// an inactive component. Eligibility and requested/current Rotation branching
// belong to the complete MoveActor caller, not this arithmetic helper.
inline bool IsOriginalActorMoveSmallDelta(const PortableModelVec3& delta) {
    using ActorMoveGeometryDetail::Small;
    return Small(delta.x)&&Small(delta.y)&&Small(delta.z);
}

// Full prequery schedule103992C2..10399406. Each Float spill and the retained
// sqrt result matter. In particular, Float size is NOT the reciprocal's input,
// and the doubled Z direction is not Float-spilled before its addition.
inline OriginalActorMoveGeometry PrepareOriginalActorMoveGeometry(const PortableModelVec3& delta) {
    using namespace ActorMoveGeometryDetail;
    OriginalActorMoveGeometry result;result.small=IsOriginalActorMoveSmallDelta(delta);
    if(result.small) {
        result.size=0.0f;result.direction=delta; // Exact raw words, signed zeros included.
    }else {
        const auto z=Load(delta.z),zSquared=Multiply64(z,z);
        const auto y=Load(delta.y),ySquared=Multiply64(y,y);
        const auto zy=Add64(zSquared,ySquared);
        const auto x=Load(delta.x),squared=Add64(zy,Multiply64(x,x));
        const double storedSquared=StoreDouble(squared);
        const auto root=ActorLineTraceDetail::Root(storedSquared);
        static_cast<void>(StoreDouble(root)); // Original FST Double scratch, NO reload.
        result.size=Store32(root);             // Original FST Float size, NO reload.
        const float inverse=Store32(ModelFastLineDetail::Divide64(DecodeDouble(1.0),root));
        const auto dz=Multiply64(Load(delta.z),Decode(inverse));
        const auto dy=Multiply64(Load(delta.y),Decode(inverse));
        const auto dx=Multiply64(Decode(inverse),Load(delta.x));
        result.direction.x=Store32(dx);result.direction.y=Store32(dy);result.direction.z=Store32(dz);
    }
    const auto doubledZ=Add64(Decode(result.direction.z),Decode(result.direction.z));
    const float doubledY=Store32(Add64(Decode(result.direction.y),Decode(result.direction.y)));
    const float doubledX=Store32(Add64(Decode(result.direction.x),Decode(result.direction.x)));
    result.extended.z=Store32(Add64(Load(delta.z),doubledZ));
    result.extended.y=Store32(Add64(Load(delta.y),Decode(doubledY)));
    result.extended.x=Store32(Add64(Decode(doubledX),Load(delta.x)));
    return result;
}

// Original two-unit hit backoff10399795..1039989D. This stages a new Delta and
// Time only: Location/Normal/Item and the caller packet are not recomputed.
// Ordered Time>=1 (including +infinity) or any nonzero bNoFail copies the raw
// inactive packet values. Finite admission applies only to active arithmetic.
inline OriginalActorMoveBackoff BackoffOriginalActorMove(const PortableModelVec3& requested,
    const OriginalActorMoveGeometry& geometry,float hitTime,std::int32_t noFail) {
    using namespace ActorMoveGeometryDetail;
    if(!NeedsBackoff(hitTime)||noFail)return {requested,hitTime};
    const auto distance=Multiply64(Add64(Decode(2.0f),Load(geometry.size)),Load(hitTime));
    const float storedDistance=Store32(distance); // FST keeps the comparison operand retained.
    if(Compare64(distance,Decode(2.0f))<=0)return {{},0.0f};
    const float backZ=Store32(Multiply64(Load(geometry.direction.z),Decode(2.0f)));
    const float backY=Store32(Multiply64(Load(geometry.direction.y),Decode(2.0f)));
    const float backX=Store32(Multiply64(Load(geometry.direction.x),Decode(2.0f)));
    const float scaledZ=Store32(Multiply64(Load(geometry.extended.z),Load(hitTime)));
    const float scaledY=Store32(Multiply64(Load(geometry.extended.y),Load(hitTime)));
    const float scaledX=Store32(Multiply64(Load(hitTime),Load(geometry.extended.x)));
    OriginalActorMoveBackoff result;
    result.delta.z=Store32(Sub(Decode(scaledZ),Decode(backZ)));
    result.delta.y=Store32(Sub(Decode(scaledY),Decode(backY)));
    result.delta.x=Store32(Sub(Decode(scaledX),Decode(backX)));
    result.time=Store32(ModelFastLineDetail::Divide64(Sub(Decode(storedDistance),Decode(2.0f)),Load(geometry.size)));
    return result;
}
} // namespace QuestVr
