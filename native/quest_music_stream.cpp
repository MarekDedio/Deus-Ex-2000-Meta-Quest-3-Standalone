#include "quest_music_stream.h"
#include "libopenmpt/libopenmpt.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ostream>
#include <stdexcept>
#include <thread>

namespace QuestVr {
struct MusicStream::Impl {
    static constexpr std::size_t Capacity=65'536u,Block=1024u;
    std::array<std::int16_t,Capacity*2u> ring{};
    std::atomic<std::uint64_t> read{},write{},underruns{};
    std::atomic<bool> stopping{};
    std::ostream diagnostics{nullptr}; // Discard optional decoder logs, bounded memory.
    openmpt::module decoder;
    std::uint32_t rate;
    double duration{};
    std::string title;
    std::thread worker;
    Impl(const std::vector<std::uint8_t>& data,std::uint32_t sampleRate,std::uint8_t section)
        : decoder(data,diagnostics),rate(sampleRate) {
        const auto songs=decoder.get_num_subsongs();
        const int selected=section==255u ? 0 : section;
        if (songs<=0 || selected>=songs) throw std::runtime_error("Authored music section is not a decoded tracker subsong");
        decoder.select_subsong(selected);
        decoder.set_repeat_count(-1);
        duration=decoder.get_duration_seconds();title=decoder.get_metadata("title");
        if (!std::isfinite(duration) || duration<=0.0 || duration>3600.0)
            throw std::runtime_error("Music duration is outside the supported stream bound");
        // Prefill on the constructing worker before the stream is published.
        for (unsigned i=0u;i<8u;++i)
            if (!Fill()) throw std::runtime_error("Music decoder could not prefill its PCM ring");
        worker=std::thread([this] {
            while (!stopping.load(std::memory_order_acquire)) {
                if (!Fill()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
    }
    ~Impl() {
        stopping.store(true,std::memory_order_release);
        if (worker.joinable()) worker.join();
    }
    bool Fill() noexcept {
        const auto w=write.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);
        if (w-r>Capacity || Capacity-(w-r)<Block) return false;
        std::array<std::int16_t,Block*2u> pcm{};
        try {
            const auto n=decoder.read_interleaved_stereo(static_cast<std::int32_t>(rate),Block,pcm.data());
            if (n==0u || n>Block) { stopping.store(true,std::memory_order_release);return false; }
            for (std::size_t f=0u;f<n;++f) {
                const auto at=static_cast<std::size_t>((w+f)%Capacity)*2u;
                ring[at]=pcm[f*2u];ring[at+1u]=pcm[f*2u+1u];
            }
            write.store(w+n,std::memory_order_release);return true;
        } catch (...) { stopping.store(true,std::memory_order_release);return false; }
    }
};
MusicStream::MusicStream(std::vector<std::uint8_t> data,std::uint32_t rate,std::uint8_t section) {
    if (data.empty() || data.size()>32u*1024u*1024u || rate<8000u || rate>192000u)
        throw std::runtime_error("Music input/rate exceeds its bounds");
    impl_=std::make_unique<Impl>(data,rate,section);
}
MusicStream::~MusicStream()=default;
std::size_t MusicStream::Mix(std::int16_t* stereo,std::size_t frames,float gain) noexcept {
    if (!stereo || frames>32'768u || !std::isfinite(gain) || gain<0.0f || gain>1.0f) return 0u;
    const auto r=impl_->read.load(std::memory_order_relaxed),w=impl_->write.load(std::memory_order_acquire);
    const auto count=std::min<std::uint64_t>(frames,w-r);
    for (std::size_t f=0u;f<count;++f) {
        const auto at=static_cast<std::size_t>((r+f)%Impl::Capacity)*2u;
        for (std::size_t c=0u;c<2u;++c) {
            const auto mixed=static_cast<std::int32_t>(stereo[f*2u+c])+
                static_cast<std::int32_t>(impl_->ring[at+c]*gain);
            stereo[f*2u+c]=static_cast<std::int16_t>(std::clamp(mixed,-32768,32767));
        }
    }
    impl_->read.store(r+count,std::memory_order_release);
    impl_->underruns.fetch_add(frames-count,std::memory_order_relaxed);
    return static_cast<std::size_t>(count);
}
std::uint64_t MusicStream::UnderrunFrames() const noexcept {return impl_->underruns.load(std::memory_order_relaxed);}
std::uint32_t MusicStream::SampleRate() const noexcept {return impl_->rate;}
double MusicStream::DurationSeconds() const noexcept {return impl_->duration;}
std::string MusicStream::Title() const {return impl_->title;}
}
