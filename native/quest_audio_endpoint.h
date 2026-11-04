#pragma once
#include "quest_native_client.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace QuestVr {
struct AudioOutputFormat {
    std::uint32_t sampleRate{};
    std::uint32_t channels{};
    enum class Sample : std::uint8_t {I16};
    Sample sample{Sample::I16};
};
// Real AAudio owner capability only: no stream, callback userdata, UObject,
// package, decoder or render handles cross into the runtime worker.
class AudioEndpointControl final {
public:
    enum class State : std::uint8_t {Pending,Ready,Revoked};
    AudioEndpointControl(std::uint64_t generation,std::shared_ptr<NativeClientLifetime> client,
        std::uint64_t clientIdentity,std::uint64_t viewportIdentity,AudioOutputFormat format)
        :generation_(generation),client_(std::move(client)),clientIdentity_(clientIdentity),
         viewportIdentity_(viewportIdentity),format_(format) {
        if(!generation_||!client_||!client_->Alive()||!clientIdentity_||!viewportIdentity_||
            format_.sampleRate<8'000u||format_.sampleRate>192'000u||format_.channels!=2u||
            format_.sample!=AudioOutputFormat::Sample::I16)
            throw std::runtime_error("Audio endpoint has invalid output/client identity");
    }
    AudioEndpointControl(const AudioEndpointControl&)=delete;
    AudioEndpointControl& operator=(const AudioEndpointControl&)=delete;
    std::uint64_t Generation() const noexcept {return generation_;}
    const AudioOutputFormat& Format() const noexcept {return format_;}
    State Observe() const noexcept {return state_.load(std::memory_order_acquire);}
    bool Ready() const noexcept {return Observe()==State::Ready&&client_->Alive();}
    bool CallbackObserved() const noexcept {return callbackObserved_.load(std::memory_order_acquire);}
    std::int32_t Error() const noexcept {return error_.load(std::memory_order_acquire);}
    // Identity is not readiness: a transport can be constructed while the real
    // stream is Pending, but no command may publish until actual start proof.
    bool MatchesOwner(const std::shared_ptr<NativeClientLifetime>& client,
        std::uint64_t clientIdentity,std::uint64_t viewportIdentity) const noexcept {
        return client==client_&&clientIdentity==clientIdentity_&&viewportIdentity==viewportIdentity_;
    }
    // Called ONLY by the registered per-stream real data callback.
    void ObserveDataCallback(std::int32_t frames) noexcept {
        if(frames>0)callbackObserved_.store(true,std::memory_order_release);
    }
    // The production owner supplies actual refreshed AAudio STARTED evidence;
    // a real positive data callback is an independent successful-start proof.
    // Constructor metadata, requestStart==OK, focus, and nonzero rate are NOT.
    bool ConfirmStartedOnOwner(bool actualStarted) noexcept {
        if(!client_->Alive()){Revoke();return false;}
        if(!actualStarted&&!CallbackObserved())return false;
        auto expected=State::Pending;
        state_.compare_exchange_strong(expected,State::Ready,std::memory_order_acq_rel);
        return Ready(); // A prior error/revoke can never transition back.
    }
    void Revoke(std::int32_t error=0) noexcept {
        if(error) {std::int32_t empty{};error_.compare_exchange_strong(empty,error,std::memory_order_acq_rel);}
        state_.store(State::Revoked,std::memory_order_release);
    }
    AudioOutputFormat RequireReady(const std::shared_ptr<NativeClientLifetime>& client,
        std::uint64_t clientIdentity,std::uint64_t viewportIdentity) const {
        if(!Ready()||!MatchesOwner(client,clientIdentity,viewportIdentity))
            throw std::runtime_error("Audio endpoint is pending, revoked or belongs to another client/viewport");
        return format_;
    }
    static constexpr std::size_t RetainedBytes() noexcept {return sizeof(AudioEndpointControl)+8u*sizeof(void*);}
private:
    const std::uint64_t generation_;
    const std::shared_ptr<NativeClientLifetime> client_;
    const std::uint64_t clientIdentity_,viewportIdentity_;
    const AudioOutputFormat format_;
    std::atomic<State> state_{State::Pending};
    std::atomic<bool> callbackObserved_{};
    std::atomic<std::int32_t> error_{};
};
inline bool SameReadyAudioEndpoint(const std::shared_ptr<AudioEndpointControl>& current,
    const std::shared_ptr<AudioEndpointControl>& expected) noexcept {
    return current&&current==expected&&current->Ready();
}
} // namespace QuestVr
