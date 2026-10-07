#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

// The consumer translation unit defines MINIMP3_IMPLEMENTATION before this
// include when it supplies the decoder implementation. Do not also include
// minimp3_ex.h in that TU: its implementation lies outside its include guard.
#include "minimp3_ex.h"

namespace QuestVr {
struct DecodedMp3Audio {
    std::vector<std::int16_t> stereo;
    std::uint32_t sourceRate{};
    std::uint32_t channels{};
    std::uint32_t targetRate{};
};

// Dialogue decoding is intentionally bounded for a standalone headset. A
// forged Xing frame count must not cause multi-gigabyte allocations. These
// budgets permit 32 MiB decoded PCM and 32 MiB output stereo PCM, separately.
inline constexpr std::size_t kMaximumMp3Bytes = 64u * 1024u * 1024u;
inline constexpr std::uint64_t kMaximumDecodedMp3Samples = 16u * 1024u * 1024u;
inline constexpr std::size_t kMaximumMp3StereoFrames = 8u * 1024u * 1024u;
inline constexpr std::uint32_t kMaximumMp3SampleRate = 192'000u;

// Pure compressed bytes -> stereo PCM: no package names, global runtime data,
// audio device access, or headset state. Successful resampling matches the
// original Quest dialogue algorithm (linear interpolation, exact truncation).
inline DecodedMp3Audio DecodePortableMp3Audio(
    const std::vector<std::uint8_t>& bytes,
    const std::uint32_t targetRate) {
    DecodedMp3Audio result;
    result.targetRate = targetRate;
    if (bytes.empty() || bytes.size() > kMaximumMp3Bytes || targetRate == 0u ||
        targetRate > kMaximumMp3SampleRate) return result;

    mp3dec_ex_t decoder{};
    struct DecoderCloser {
        mp3dec_ex_t* value;
        void Close() {
            if (value != nullptr) mp3dec_ex_close(value);
            value = nullptr;
        }
        ~DecoderCloser() { Close(); }
    } closer{&decoder};
    if (mp3dec_ex_open_buf(&decoder, bytes.data(), bytes.size(), MP3D_SEEK_TO_SAMPLE) != 0)
        return result;
    result.sourceRate = static_cast<std::uint32_t>(decoder.info.hz);
    result.channels = static_cast<std::uint32_t>(decoder.info.channels);
    if (decoder.info.hz <= 0 || result.sourceRate > kMaximumMp3SampleRate ||
        (result.channels != 1u && result.channels != 2u) || decoder.samples == 0u ||
        decoder.samples > kMaximumDecodedMp3Samples) return result;

    std::vector<mp3d_sample_t> decoded(static_cast<std::size_t>(decoder.samples));
    const std::size_t samples = mp3dec_ex_read(&decoder, decoded.data(), decoded.size());
    closer.Close();
    if (samples == 0u || samples > decoded.size()) return result;
    const std::size_t sourceFrames = samples / result.channels;
    if (sourceFrames == 0u) return result;
    const std::uint64_t frames64 = static_cast<std::uint64_t>(sourceFrames) * targetRate / result.sourceRate;
    if (frames64 > kMaximumMp3StereoFrames) return result;
    const std::size_t outputFrames = static_cast<std::size_t>(frames64);
    result.stereo.resize(outputFrames * 2u);
    for (std::size_t frame = 0; frame < outputFrames; ++frame) {
        const double sourcePosition = static_cast<double>(frame) * result.sourceRate / targetRate;
        const std::size_t first = std::min(static_cast<std::size_t>(sourcePosition), sourceFrames - 1u);
        const std::size_t second = std::min(first + 1u, sourceFrames - 1u);
        const float fraction = static_cast<float>(sourcePosition - first);
        for (std::size_t channel = 0; channel < 2u; ++channel) {
            const std::size_t sourceChannel = result.channels == 1u ? 0u : channel;
            const float a = decoded[first * result.channels + sourceChannel];
            const float b = decoded[second * result.channels + sourceChannel];
            result.stereo[frame * 2u + channel] = static_cast<std::int16_t>(a + (b - a) * fraction);
        }
    }
    return result;
}
} // namespace QuestVr
