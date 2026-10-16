#pragma once

#include "quest_script_random_vector.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace QuestVr {

struct OriginalLevelClockStep {
    double timeSeconds{};
    float timeMirror{}, scaledDelta{}, actorDelta{};
};

inline constexpr float OriginalLevelMinimumActorDelta = std::bit_cast<float>(std::uint32_t{0x3ba3d70au});
inline constexpr float OriginalLevelMaximumActorDelta = std::bit_cast<float>(std::uint32_t{0x3ecccccdu});

// Arithmetic phase of retail ULevel.Tick103a5468..103a54df only, not a world
// scheduler. The caller owns the actual native Level Double clock and writes
// LevelInfo.TimeSeconds/actor delta in the same transaction as subsequent work.
// FST Float(scaledDelta) and FST Double(timeSeconds) do NOT pop/reload ST0:
// the clock consumes the retained PC64 product, and the Float time mirror
// consumes the retained PC64 sum BEFORE its separate binary64 storage rounding.
// Actor delta is clamped only afterwards; neither clamp limits clock advance.
inline OriginalLevelClockStep AdvanceOriginalLevelClock(
    const double prior, const float realDelta, const float dilation) {
    if (!std::isfinite(prior) || prior < 0.0 || !std::isfinite(realDelta) || realDelta < 0.0f ||
        !std::isfinite(dilation))
        throw std::invalid_argument("Original Level clock requires finite nonnegative time/delta and finite dilation");
    using ScriptRandom::VectorDetail::DecodeDouble;
    using ScriptRandom::VectorDetail::StoreDouble;
    using ActorTimerDetail::Decode;
    using ActorTimerDetail::Store32;
    const auto retainedDelta = ModelFastLineDetail::Multiply64(Decode(realDelta), Decode(dilation));
    OriginalLevelClockStep result;
    // Even though retail retains the product, its stored Float delta is also
    // observable. Refuse overflow rather than invent a saturated/clamped value.
    result.scaledDelta = Store32(retainedDelta);
    const auto retainedTime = NativeWindowClipDetail::Add64(retainedDelta, DecodeDouble(prior));
    // Negative finite dilation is not clamped or rejected by itself. The
    // embedding refuses an actually negative resulting time (signed zero is
    // still valid), rather than silently making backwards time nonnegative.
    if (retainedTime.negative && retainedTime.significand != 0u)
        throw std::invalid_argument("Original Level clock would publish a negative native time");
    result.timeSeconds = StoreDouble(retainedTime);
    result.timeMirror = Store32(retainedTime);
    result.actorDelta = result.scaledDelta < OriginalLevelMinimumActorDelta ? OriginalLevelMinimumActorDelta :
        result.scaledDelta >= OriginalLevelMaximumActorDelta ? OriginalLevelMaximumActorDelta : result.scaledDelta;
    return result;
}

} // namespace QuestVr
