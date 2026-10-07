#define MINIMP3_IMPLEMENTATION
#include "portable_mp3_audio.h"
#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
void Require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

// Source-generated MPEG1 Layer III silence: 128 kbps, 44.1 kHz, no padding,
// zero side information and zero spectral data. No original audio is embedded.
Bytes SilentFrames(bool mono, std::size_t frameCount = 5) {
    Bytes bytes(frameCount * 417u, 0);
    for (std::size_t i = 0; i < frameCount; ++i) {
        bytes[i * 417u] = 0xff;
        bytes[i * 417u + 1u] = 0xfb;
        bytes[i * 417u + 2u] = 0x90;
        bytes[i * 417u + 3u] = mono ? 0xc0 : 0x00;
    }
    return bytes;
}

struct RawPcm {
    std::vector<mp3d_sample_t> samples;
    std::uint32_t rate{}, channels{};
};
RawPcm DecodeRawReference(const Bytes& bytes) {
    mp3dec_ex_t decoder{};
    struct Close { mp3dec_ex_t* decoder; ~Close() { mp3dec_ex_close(decoder); } } close{&decoder};
    Require(mp3dec_ex_open_buf(&decoder, bytes.data(), bytes.size(), MP3D_SEEK_TO_SAMPLE) == 0,
            "Reference MP3 decoder open failed");
    Require(decoder.samples > 0 && decoder.samples <= QuestVr::kMaximumDecodedMp3Samples &&
            decoder.info.hz > 0 && (decoder.info.channels == 1 || decoder.info.channels == 2),
            "Reference MP3 metadata is invalid or exceeds test budget");
    RawPcm pcm;
    pcm.rate = static_cast<std::uint32_t>(decoder.info.hz);
    pcm.channels = static_cast<std::uint32_t>(decoder.info.channels);
    pcm.samples.resize(static_cast<std::size_t>(decoder.samples));
    const auto count = mp3dec_ex_read(&decoder, pcm.samples.data(), pcm.samples.size());
    Require(count > 0 && count % pcm.channels == 0, "Reference MP3 sample count is empty or unaligned");
    pcm.samples.resize(count);
    return pcm;
}

// Independent reference for the former native dialogue interpolation math;
// the production shared helper is compared sample-for-sample, not only hashed.
std::vector<std::int16_t> ReferenceStereo(const RawPcm& pcm, std::uint32_t targetRate) {
    const auto sourceFrames = pcm.samples.size() / pcm.channels;
    const auto frames = static_cast<std::size_t>(static_cast<std::uint64_t>(sourceFrames) * targetRate / pcm.rate);
    Require(frames <= QuestVr::kMaximumMp3StereoFrames, "Reference PCM exceeds test output budget");
    std::vector<std::int16_t> stereo(frames * 2u);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const double position = static_cast<double>(frame) * pcm.rate / targetRate;
        const auto first = std::min(static_cast<std::size_t>(position), sourceFrames - 1u);
        const auto second = std::min(first + 1u, sourceFrames - 1u);
        const float fraction = static_cast<float>(position - first);
        for (std::size_t channel = 0; channel < 2u; ++channel) {
            const auto originalChannel = pcm.channels == 1u ? 0u : channel;
            const float a = pcm.samples[first * pcm.channels + originalChannel];
            const float b = pcm.samples[second * pcm.channels + originalChannel];
            stereo[frame * 2u + channel] = static_cast<std::int16_t>(a + (b - a) * fraction);
        }
    }
    return stereo;
}

std::uint64_t PcmHash(const std::vector<std::int16_t>& samples) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto sample : samples) {
        const auto value = static_cast<std::uint16_t>(sample);
        for (const auto byte : {static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8u)}) {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

void VerifyConversions(const Bytes& bytes, const std::string& label) {
    const auto raw = DecodeRawReference(bytes);
    const auto nonzeroSamples = std::count_if(raw.samples.begin(), raw.samples.end(), [](auto sample) { return sample != 0; });
    std::size_t distinctStereoFrames{};
    if (raw.channels == 2u)
        for (std::size_t i = 0; i < raw.samples.size(); i += 2u)
            if (raw.samples[i] != raw.samples[i + 1u]) ++distinctStereoFrames;
    std::cout << label << " input: " << nonzeroSamples << " nonzero samples, "
              << distinctStereoFrames << " distinct L/R frames.\n";
    for (const auto targetRate : {8'000u, 22'050u, 44'100u, 48'000u, 192'000u}) {
        const auto decoded = QuestVr::DecodePortableMp3Audio(bytes, targetRate);
        const auto repeat = QuestVr::DecodePortableMp3Audio(bytes, targetRate);
        const auto expected = ReferenceStereo(raw, targetRate);
        Require(decoded.sourceRate == raw.rate && decoded.channels == raw.channels &&
                decoded.targetRate == targetRate && decoded.stereo == expected,
                "Shared MP3 stereo PCM differs from exact prior interpolation behavior");
        Require(decoded.stereo == repeat.stereo && PcmHash(decoded.stereo) == PcmHash(repeat.stereo),
                "Repeated MP3 decoding was not deterministic");
        const auto expectedFrames = static_cast<std::uint64_t>(raw.samples.size() / raw.channels) * targetRate / raw.rate;
        Require(decoded.stereo.size() == expectedFrames * 2u, "Resampling output frame count differs");
        if (raw.channels == 1u)
            for (std::size_t i = 0; i < decoded.stereo.size(); i += 2u)
                Require(decoded.stereo[i] == decoded.stereo[i + 1u], "Mono was not duplicated equally to stereo");
        if (targetRate == raw.rate)
            for (std::size_t frame = 0; frame < decoded.stereo.size() / 2u; ++frame)
                for (std::size_t channel = 0; channel < 2u; ++channel)
                    Require(decoded.stereo[frame * 2u + channel] ==
                            raw.samples[frame * raw.channels + (raw.channels == 1u ? 0u : channel)],
                            "Native-rate channel mapping altered original PCM");
        std::cout << label << ": " << raw.rate << " Hz/" << raw.channels << " channels -> "
                  << targetRate << " Hz, " << decoded.stereo.size() / 2u << " frames, hash "
                  << std::hex << PcmHash(decoded.stereo) << std::dec << ".\n";
    }
}

void SyntheticTests() {
    const auto invalid = [&](const Bytes& bytes, const std::uint32_t rate) {
        const auto decoded = QuestVr::DecodePortableMp3Audio(bytes, rate);
        Require(decoded.stereo.empty() && decoded.targetRate == rate,
                "Empty/truncated/invalid/unsafe MP3 unexpectedly produced PCM");
    };
    invalid({}, 48'000u);
    for (const auto& bytes : {Bytes{0xff}, Bytes{0xff, 0xfb}, Bytes{0xff, 0xfb, 0x90},
            Bytes{0xff, 0xfb, 0x90, 0xc0}, Bytes(4096u, 0x55), Bytes(4096u, 0xff),
            Bytes{'I', 'D', '3', 4, 0, 0, 0x7f, 0x7f, 0x7f, 0x7f}})
        invalid(bytes, 48'000u);
    const auto mono = SilentFrames(true);
    const auto stereo = SilentFrames(false);
    invalid(mono, 0u);
    invalid(mono, QuestVr::kMaximumMp3SampleRate + 1u);
    invalid(mono, std::numeric_limits<std::uint32_t>::max());
    invalid(Bytes(QuestVr::kMaximumMp3Bytes + 1u, 0), 48'000u);

    auto forgedXing = mono;
    const auto tag = 4u + 17u; // MPEG1 mono header + side info.
    forgedXing[tag] = 'X'; forgedXing[tag + 1u] = 'i';
    forgedXing[tag + 2u] = 'n'; forgedXing[tag + 3u] = 'g';
    forgedXing[tag + 7u] = 1u; // Frame-count field present.
    for (std::size_t i = 8; i < 12; ++i) forgedXing[tag + i] = 0xff;
    mp3dec_ex_t probe{};
    Require(mp3dec_ex_open_buf(&probe, forgedXing.data(), forgedXing.size(), MP3D_SEEK_TO_SAMPLE) == 0,
            "Forged Xing safety fixture is not recognized by upstream decoder");
    const auto advertisedSamples = probe.samples;
    mp3dec_ex_close(&probe);
    Require(advertisedSamples > QuestVr::kMaximumDecodedMp3Samples,
            "Forged Xing fixture did not advertise an excessive allocation");
    invalid(forgedXing, 48'000u);
    // Enough low-rate silence to exceed only the output PCM budget at 192 kHz.
    invalid(SilentFrames(true, 1800), QuestVr::kMaximumMp3SampleRate);

    for (const auto* bytes : {&mono, &stereo}) {
        const auto raw = DecodeRawReference(*bytes);
        Require(raw.rate == 44'100u && raw.samples.size() / raw.channels == 5u * 1152u &&
                std::all_of(raw.samples.begin(), raw.samples.end(), [](auto sample) { return sample == 0; }),
                "Source-generated silence fixture metadata or PCM differs");
    }
    VerifyConversions(mono, "Synthetic mono silence");
    VerifyConversions(stereo, "Synthetic stereo silence");
    std::cout << "Synthetic MP3 invalid/truncated input, size/rate limits, forged Xing allocation guard, "
              << "output budget, mono duplication, stereo mapping and five-rate determinism passed.\n";
}

Bytes ReadBoundedFixture(const std::string& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    Require(static_cast<bool>(stream), "Could not open optional compressed-audio fixture read-only");
    const auto size = stream.tellg();
    Require(size > 0 && static_cast<std::uint64_t>(size) <= QuestVr::kMaximumMp3Bytes,
            "Optional audio fixture is empty or exceeds compressed-input budget");
    Bytes bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), size);
    Require(static_cast<bool>(stream), "Optional audio fixture read was truncated");
    return bytes;
}

void OriginalPackageTest(const std::string& path) {
    const auto package = LoadPortablePackageTables(path);
    for (std::size_t i = 0; i < package.exports.size(); ++i) {
        if (NameString(GetPortableObjectPath(package, package.exports[i].ObjClass)) != "Engine.Sound") continue;
        const auto sound = LoadPortableSound(package, i);
        if (sound.format != "mp3") continue;
        VerifyConversions(sound.data, "Original package " + GetPortableObjectPath(package, static_cast<std::int32_t>(i + 1u)));
        std::cout << "Original sound export decoded read-only. This verifies compressed bytes/PCM only, "
                  << "not audible playback, AAudio, mixing, spatial audio or headset output.\n";
        return;
    }
    throw std::runtime_error("Optional package has no Engine.Sound MP3 export");
}
} // namespace

int main(int argc, char** argv) {
    try {
        Require(argc == 1 || (argc == 3 && (std::string(argv[1]) == "--package" || std::string(argv[1]) == "--mp3")),
                "Usage: portable_mp3_audio_test [--package owned-audio-package.u | --mp3 generated-fixture.mp3]");
        SyntheticTests();
        if (argc == 3) {
            if (std::string(argv[1]) == "--package") OriginalPackageTest(argv[2]);
            else VerifyConversions(ReadBoundedFixture(argv[2]), "Optional compressed fixture");
        } else {
            std::cout << "Original-data audio verification was not requested; use --package to opt in.\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Portable MP3 audio test failed: " << error.what() << '\n';
        return 1;
    }
}
