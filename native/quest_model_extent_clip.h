#pragma once

#include "quest_script_random_vector.h"

namespace QuestVr {

// One actual GOTY FBoxCheck clipping operation (103f5b80..103f5d34).
// This is a component of the pending complete extent/BSP query, NOT a swept
// collision service, Move266 implementation, FindBase or physics scheduler.
struct OriginalExtentPlane { float x{},y{},z{},w{}; };
struct OriginalExtentClipState {
    float enter{-1.0f}, exit{2.0f};
    PortableModelVec3 normal{};
};

namespace ModelExtentClipDetail {
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;
inline void Finite(const float value) {
    if (!std::isfinite(value)) throw std::invalid_argument("Original extent clip requires finite active Float data");
}
inline void Finite(const PortableModelVec3& value) {Finite(value.x);Finite(value.y);Finite(value.z);}
inline Dyadic Magnitude(Dyadic value) {
    // Retail FCOM/FCHS implements v<0?-v:v, not FABS. Preserve negative zero.
    if (value.significand && value.negative) value.negative=false;
    return value;
}
inline float Pushout(const PortableModelVec3& extent,const OriginalExtentPlane& plane) {
    return Store32(Add64(Add64(Magnitude(Multiply64(Decode(extent.z),Decode(plane.z))),
        Magnitude(Multiply64(Decode(extent.y),Decode(plane.y)))),
        Magnitude(Multiply64(Decode(extent.x),Decode(plane.x)))));
}
inline float Distance(const PortableModelVec3& point,const OriginalExtentPlane& plane) {
    return Store32(Add64(Add64(Add64(Multiply64(Decode(point.z),Decode(plane.z)),
        Multiply64(Decode(point.y),Decode(plane.y))),Multiply64(Decode(point.x),Decode(plane.x))),
        Negate(Decode(plane.w))));
}
inline constexpr double NegativeParallel=std::bit_cast<double>(std::uint64_t{0xbee4f8b588e368f1ull});
inline constexpr double PositiveParallel=std::bit_cast<double>(std::uint64_t{0x3ee4f8b588e368f1ull});
} // namespace ModelExtentClipDetail

// The retail null-owner hull's six extra planes. Its positive-Z bound EXPANDS,
// unlike the positive-X/Y bounds; minimum faces also use Double0.1, not Float.
// Do not replace this by the bundled engine's uniformly shaved bounding box.
inline std::array<OriginalExtentPlane,6u> BuildOriginalExtentBoundsPlanes(
    const PortableModelVec3& minimum,const PortableModelVec3& maximum) {
    using namespace ModelExtentClipDetail;
    Finite(minimum);Finite(maximum);
    if (minimum.x>maximum.x || minimum.y>maximum.y || minimum.z>maximum.z)
        throw std::invalid_argument("Original extent hull bounds are reversed");
    const auto margin=ScriptRandom::VectorDetail::DecodeDouble(
        std::bit_cast<double>(std::uint64_t{0x3fb999999999999aull}));
    const auto low=[&](float value) {return Store32(Add64(margin,Negate(Decode(value))));};
    const auto high=[&](float value,bool expand) {return Store32(Add64(Decode(value),expand?margin:Negate(margin)));};
    return {{{0,0,-1,low(minimum.z)},{0,0,1,high(maximum.z,true)},
        {-1,0,0,low(minimum.x)},{1,0,0,high(maximum.x,false)},
        {0,-1,0,low(minimum.y)},{0,1,0,high(maximum.y,false)}}};
}

// Stateful hull clip: retain mutations even when the resulting interval is
// rejected, as the retail helper does. Nonfinite/overflow refusal is explicit;
// callers of the eventual native query must abort the enclosing VM root.
inline bool ClipOriginalExtentPlane(OriginalExtentClipState& state,const OriginalExtentPlane& plane,
    const PortableModelVec3& start,const PortableModelVec3& end,const PortableModelVec3& extent) {
    using namespace ModelExtentClipDetail;
    Finite(start);Finite(end);Finite(extent);Finite(state.enter);Finite(state.exit);
    Finite(plane.x);Finite(plane.y);Finite(plane.z);Finite(plane.w);
    if (extent.x<0.0f || extent.y<0.0f || extent.z<0.0f)
        throw std::invalid_argument("Original extent clip requires nonnegative collision extents");
    const float pushout=Pushout(extent,plane);
    const float from=Distance(start,plane),to=Distance(end,plane);
    auto numerator=Add64(Decode(from),Negate(Decode(pushout)));
    if (from>to && Compare64(numerator,Negate(Decode(pushout)))>=0 &&
        Compare64(numerator,Decode(0.0f))<0) numerator=Decode(0.0f);
    const float denominator=Store32(Add64(Decode(from),Negate(Decode(to))));
    const auto divisor=Decode(denominator);
    if (Compare64(divisor,ScriptRandom::VectorDetail::DecodeDouble(NegativeParallel))<0) {
        const auto quotient=ModelFastLineDetail::Divide64(numerator,divisor);
        if (Compare64(quotient,Decode(state.exit))<0) state.exit=Store32(quotient);
    } else if (Compare64(divisor,ScriptRandom::VectorDetail::DecodeDouble(PositiveParallel))>0) {
        const auto quotient=ModelFastLineDetail::Divide64(numerator,divisor);
        if (Compare64(quotient,Decode(state.enter))>0) {
            state.enter=Store32(quotient);state.normal={plane.x,plane.y,plane.z};
        }
    } else if (from>pushout && to>pushout) return false;
    // Retail computes q even for a parallel/zero denominator, then discards it
    // without observing the result. Omit that unused NaN/Inf and x87 status;
    // never interpret it as a hit, normalize a normal, or clamp the interval.
    return state.enter<state.exit;
}

} // namespace QuestVr
