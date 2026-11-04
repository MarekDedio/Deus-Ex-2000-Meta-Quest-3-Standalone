#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

namespace QuestVr {
inline constexpr std::size_t AudioVoiceCapacity=32u;
inline constexpr std::uint64_t AudioVoiceIdentityMaximum=std::numeric_limits<std::uint64_t>::max()>>2u;
enum class AudioVoicePlayback : std::uint8_t {Empty,Pending,Playing,Finished};
// One acquire/release word publishes voice identity and state together. A
// mismatched identity must never retire a pending replacement voice. Empty or
// Pending is not evidence of audible PCM; Playing requires a positive mix.
struct AudioVoiceFeedback {
    std::uint64_t voice{};
    AudioVoicePlayback playback{AudioVoicePlayback::Empty};
};
} // namespace QuestVr
