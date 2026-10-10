#pragma once

#include <bit>
#include <cstdint>

namespace QuestVr::ScriptRandom {

// Stable portable representation of the original game's imported MSVCRT
// game-thread stream. Never serialize an implementation-defined std engine or
// use the host libc's thread-local RNG for campaign execution.
inline constexpr std::uint8_t Algorithm = 1u;
inline constexpr std::uint32_t InitialSeed = 1u;

inline std::uint32_t Draw15(std::uint32_t& seed) noexcept {
    seed = seed * 214013u + 2531011u; // Defined uint32 wrap, including all seeds.
    return (seed >> 16u) & 0x7fffu;
}

inline std::int32_t Rand(std::uint32_t& seed, const std::int32_t maximum) noexcept {
    // Original Core.dll returns before appRand for nonpositive bounds. Max=1
    // still draws; bounds above 32768 are not clamped or rejection-sampled.
    return maximum > 0 ? static_cast<std::int32_t>(Draw15(seed)) % maximum : 0;
}

inline float FRand(std::uint32_t& seed) noexcept {
    // Exact binary32 constant loaded by original appFrand (bits 0x38000100).
    // After the result is stored as a float, sample 32767 yields exactly 1.0.
    return static_cast<float>(Draw15(seed)) * std::bit_cast<float>(0x38000100u);
}

} // namespace QuestVr::ScriptRandom
