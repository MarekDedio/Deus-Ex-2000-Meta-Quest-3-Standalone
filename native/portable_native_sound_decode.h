#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

struct PortableNativeSoundPcm {
    std::vector<std::int16_t> stereo;
    std::uint32_t rate{},loopBegin{},loopEnd{};
    bool loop{};
};
struct PortableNativeSoundDecodeWork {
    // Bounded input scans, parsed records and output samples. A caller may
    // throw to refuse work; callbacks must not mutate the borrowed input.
    std::function<void(std::size_t)> step;
    // Absolute simultaneous decoder workspace, EXCLUDING borrowed encoded
    // bytes and caller-owned state. Invoked before index/vector allocations;
    // includes fixed decoder handles and old+new MP3 index realloc storage.
    // Caller storage/copy admission for these callbacks remains its own duty.
    std::function<void(std::size_t)> admit;
};
// CPU-only source resource preparation. Preserves native frame rate and
// PCM8/16 mono/stereo and forward integer-position infinite WAV smpl loops.
// loopEnd is an EXCLUSIVE source frame converted from the file's inclusive end.
// Non-forward/fractional/finite/multiple loops explicitly refuse; they are not
// silently replaced by whole-file looping. No device/runtime/callback work.
PortableNativeSoundPcm DecodePortableNativeSoundPcm(std::string_view format,
    const std::vector<std::uint8_t>& bytes,const PortableNativeSoundDecodeWork& work={});
