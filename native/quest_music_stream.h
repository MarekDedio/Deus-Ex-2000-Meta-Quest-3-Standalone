#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace QuestVr {
// Owns a tracker decoder and fixed SPSC PCM ring. Only its worker decodes;
// the single audio consumer performs bounded copies/adds without waiting,
// allocation, package I/O or libopenmpt calls. Construction/destruction are
// control/worker-thread operations, never audio-callback operations.
class MusicStream {
public:
    MusicStream(std::vector<std::uint8_t> module,std::uint32_t rate,std::uint8_t section);
    ~MusicStream();
    MusicStream(const MusicStream&)=delete;
    MusicStream& operator=(const MusicStream&)=delete;
    std::size_t Mix(std::int16_t* stereo,std::size_t frames,float gain) noexcept;
    std::uint64_t UnderrunFrames() const noexcept;
    std::uint32_t SampleRate() const noexcept;
    double DurationSeconds() const noexcept;
    std::string Title() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
