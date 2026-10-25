#pragma once

#include "quest_native_window_clip.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace QuestVr::NativeTileMath {
using Real=ActorTimerDetail::Dyadic;

namespace Detail {
// Real is a detached PC64-precision dyadic, not an unrestricted integer or a
// serialized native x87 frame. Leave room for normalization/rounding in the
// existing arithmetic. All finite Float/INT Tile inputs and their bounded
// layout sums are comfortably inside this representation boundary.
inline void Validate(Real value) {
    if (ActorTimerDetail::Bits(value.significand)>64u ||
        value.exponent<std::numeric_limits<int>::min()+128 ||
        value.exponent>std::numeric_limits<int>::max()-128)
        throw std::runtime_error("Native Tile malformed PC64 arithmetic operand refused");
}
} // namespace Detail

inline Real Load(float value) {
    if (!std::isfinite(value))
        throw std::runtime_error("Native Tile nonfinite active Float refused");
    return ActorTimerDetail::Decode(value); // Includes the original zero sign.
}

inline Real Integer(std::int32_t value) {
    // Widen before negating: INT_MIN is a genuine, exactly representable input.
    const auto wide=static_cast<std::int64_t>(value);
    const auto magnitude=wide<0 ? static_cast<std::uint64_t>(-wide) :
        static_cast<std::uint64_t>(wide);
    return {magnitude,0,wide<0};
}

inline Real Add(Real a,Real b) {
    Detail::Validate(a);Detail::Validate(b);
    const auto result=NativeWindowClipDetail::Add64(a,b);
    Detail::Validate(result);return result;
}

inline Real Subtract(Real a,Real b) {
    Detail::Validate(a);Detail::Validate(b);
    const auto result=NativeWindowClipDetail::Add64(a,NativeWindowClipDetail::Negate(b));
    Detail::Validate(result);return result;
}

inline int Compare(Real a,Real b) {
    Detail::Validate(a);Detail::Validate(b);
    return NativeWindowClipDetail::Compare64(a,b);
}

inline float Store(Real value) {
    Detail::Validate(value);
    if (!value.significand) return ActorTimerDetail::Store32(value);
    const auto top=static_cast<std::int64_t>(value.exponent)+
        static_cast<std::int64_t>(ActorTimerDetail::Bits(value.significand))-1;
    if (top>127)
        throw std::runtime_error("Native Tile finite Float storage overflow refused");
    // Strictly below half the least subnormal rounds to signed zero. Besides
    // avoiding huge shifts, this keeps malformed exponents out of Store32's
    // int-sized Float-quantum arithmetic. The half/subnormal boundary itself
    // still goes through the original nearest-even store implementation.
    if (top<-150) return ActorTimerDetail::Store32({0u,0,value.negative});
    return ActorTimerDetail::Store32(value);
}

inline std::int32_t Truncate(Real value) {
    Detail::Validate(value);
    if (!value.significand) return 0;
    ActorTimerDetail::Wide whole{};
    if (value.exponent>=0) {
        const auto wholeBits=static_cast<std::int64_t>(ActorTimerDetail::Bits(value.significand))+
            value.exponent;
        if (wholeBits>32)
            throw std::runtime_error("Native Tile signed32 truncation overflow refused");
        whole=value.significand<<static_cast<unsigned>(value.exponent);
    } else {
        const auto shift=-static_cast<std::int64_t>(value.exponent);
        if (shift<static_cast<std::int64_t>(ActorTimerDetail::Bits(value.significand)))
            whole=value.significand>>static_cast<unsigned>(shift);
    }
    const ActorTimerDetail::Wide limit=value.negative ? 0x80000000u : 0x7fffffffu;
    if (whole>limit)
        throw std::runtime_error("Native Tile signed32 truncation boundary refused");
    if (value.negative && whole==0x80000000u) return std::numeric_limits<std::int32_t>::min();
    const auto integer=static_cast<std::int32_t>(whole);
    return value.negative ? -integer : integer;
}
} // namespace QuestVr::NativeTileMath
