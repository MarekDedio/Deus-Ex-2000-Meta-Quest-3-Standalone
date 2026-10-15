#pragma once
#include "quest_actor_timer.h"

namespace QuestVr {
struct NativeWindowLocation { float x{}, y{}; bool changed{}; };

namespace NativeWindowLocationDetail {
inline bool NumericallyEqual(ActorTimerDetail::Dyadic a,ActorTimerDetail::Dyadic b) {
    // Equality is numerical, not a comparison of representation or signed zero.
    // Odd significands give a unique finite nonzero dyadic representation.
    if (!a.significand || !b.significand) return !a.significand && !b.significand;
    if (a.negative != b.negative) return false;
    for (; !(a.significand&1u); a.significand>>=1u) ++a.exponent;
    for (; !(b.significand&1u); b.significand>>=1u) ++b.exponent;
    return a.significand==b.significand && a.exponent==b.exponent;
}
} // namespace NativeWindowLocationDetail

// XTabGroupWindow.ComputeTabGroupLocation10045000 keeps its X addition live
// in an x87 PC64 register through the comparison, but stores Y to binary32
// BEFORE comparing it. Binary64 cannot represent every relevant PC64 residual.
// An unchanged comparison stores neither coordinate, preserving old zero signs.
// When either differs, both coordinates are stored, including new zero signs.
// This pure helper does not dispatch modal resort or mutate any window state.
inline NativeWindowLocation ComputeNativeWindowLocation(float originX,float localX,
    float originY,float localY,float oldX,float oldY) {
    for (float value:{originX,localX,originY,localY,oldX,oldY})
        if (!std::isfinite(value))
            throw std::runtime_error("Native window location requires finite Float inputs");
    const auto x=ActorTimerDetail::AddStoredFloats(originX,localX);
    const auto y=ActorTimerDetail::Store32(ActorTimerDetail::AddStoredFloats(originY,localY));
    if (NativeWindowLocationDetail::NumericallyEqual(x,ActorTimerDetail::Decode(oldX)) && y==oldY)
        return {oldX,oldY,false};
    // Refuse binary32 overflow as a defined portable storage policy. Retail's
    // overflowing FSTP path is not claimed, and no caller state has changed.
    return {ActorTimerDetail::Store32(x),y,true};
}
} // namespace QuestVr
