#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace QuestVr {
// Port app-loop authority, NOT PC-launcher assignment timing, XR focus,
// target/session lifetime or proof of completed original world startup.
class ProcessLoopControl final {
public:
    enum class State : std::uint8_t {Ready,Executing,Stopped,Revoked};
    ProcessLoopControl() {
        auto value=nextIdentity_.load(std::memory_order_relaxed);
        for(;;) {
            if(value==std::numeric_limits<std::uint64_t>::max())Fail("identity exhausted");
            if(nextIdentity_.compare_exchange_weak(value,value+1u,std::memory_order_relaxed)) {
                identity_=value+1u;break;
            }
        }
    }
    ProcessLoopControl(const ProcessLoopControl&)=delete;
    ProcessLoopControl& operator=(const ProcessLoopControl&)=delete;
    std::uint64_t Identity() const noexcept {return identity_;}
    void EnterExecution() {
        auto expected=State::Ready;
        if(!state_.compare_exchange_strong(expected,State::Executing,std::memory_order_acq_rel))
            Fail("entry is not its unique Ready-to-Executing transition");
    }
    void LeaveExecution() {
        auto expected=State::Executing;
        if(!state_.compare_exchange_strong(expected,State::Stopped,std::memory_order_acq_rel))
            Fail("stop is not its unique Executing-to-Stopped transition");
    }
    // Revocation is separate from a genuine false observation and irreversible.
    // App shutdown may also revoke a Ready control after initialization fails.
    void Revoke() noexcept {state_.store(State::Revoked,std::memory_order_release);}
    bool Alive() const noexcept {return state_.load(std::memory_order_acquire)!=State::Revoked;}
    State Observe() const {
        const auto state=state_.load(std::memory_order_acquire);
        if(state==State::Revoked)Fail("capability revoked");
        return state;
    }
    bool Running() const {return Observe()==State::Executing;}
    static constexpr std::size_t RetainedBytes() noexcept {
        return sizeof(ProcessLoopControl)+8u*sizeof(void*); // owning shared-control block allowance.
    }
private:
    [[noreturn]] static void Fail(const char* why) {
        throw std::runtime_error(std::string("Process loop: ")+why);
    }
    inline static std::atomic<std::uint64_t> nextIdentity_{};
    std::uint64_t identity_{};
    std::atomic<State> state_{State::Ready};
};
} // namespace QuestVr
