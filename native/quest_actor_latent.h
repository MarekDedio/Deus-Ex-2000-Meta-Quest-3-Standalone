#pragma once

#include <cmath>
#include <stdexcept>

namespace QuestVr {

// Original GOTY Engine.dll execPollSleep, not the reference engine's clamp.
// The float store does not reload the x87 comparison operand: compare the
// difference of the two binary32 inputs before rounding the stored remainder.
inline bool PollActorSleep(float& timeLeft, const float elapsed) {
    if (!std::isfinite(timeLeft) || !std::isfinite(elapsed) || elapsed < 0.0f)
        throw std::runtime_error("Actor Sleep timer/elapsed is invalid");
    const double remainder = static_cast<double>(timeLeft) - static_cast<double>(elapsed);
    const float stored = static_cast<float>(remainder);
    if (!std::isfinite(stored)) throw std::runtime_error("Actor Sleep remainder exceeds finite float storage");
    timeLeft = stored;
    return remainder < 0.5 * static_cast<double>(elapsed); // Strict, signed, no clamp.
}

// Original Pawn execPollWaitForLanding: only call while Physics is Falling.
// A timeout requests LongFall; it does NOT release/reset the native wait.
inline bool PollActorLanding(float& timeLeft, const float elapsed) {
    if (!std::isfinite(timeLeft) || !std::isfinite(elapsed) || elapsed < 0.0f)
        throw std::runtime_error("Actor landing timer/elapsed is invalid");
    const double remainder = static_cast<double>(timeLeft) - static_cast<double>(elapsed);
    const float stored = static_cast<float>(remainder);
    if (!std::isfinite(stored)) throw std::runtime_error("Actor landing remainder exceeds finite float storage");
    timeLeft = stored;
    return remainder < 0.0; // Strict, signed, no clamp; equality does not call LongFall.
}

} // namespace QuestVr
