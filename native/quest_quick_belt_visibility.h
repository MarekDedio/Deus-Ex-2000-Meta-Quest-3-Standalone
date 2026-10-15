#pragma once

#include <cmath>
#include <stdexcept>

namespace QuestVr {

// Presentation timing only: no inventory selection, saved state, clock reads,
// allocation, texture changes or runtime access. The caller supplies elapsed
// frame time, including frames where tracking/rendering is unavailable.
class QuickBeltVisibility {
public:
    static constexpr float DisplaySeconds = 3.0f;
    static constexpr float FadeSeconds = 0.25f;

    // A new session starts unobstructed. Reset also releases an active hold.
    void Reset() noexcept {
        remainingSeconds_ = 0.0;
        held_ = false;
    }

    // Deliberate choose/use actions replace, rather than accumulate, the window.
    void Reveal() noexcept { remainingSeconds_ = DisplaySeconds; }

    // Optional continuous action input. A genuine release starts a complete
    // grace window; repeated idle false inputs must never reveal the bar.
    // Callers must release this input on tracking/focus loss or cancellation.
    void SetHeld(bool held) noexcept {
        if (held) {
            held_ = true;
            Reveal();
        } else if (held_) {
            held_ = false;
            Reveal();
        }
    }

    // Validate before touching state, even while hidden/held. Any finite large
    // delta expires a released window without overflowing or wrapping a timer.
    void Tick(float deltaSeconds) {
        if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f)
            throw std::invalid_argument("Quickbar elapsed time must be finite and nonnegative");
        if (held_) {
            remainingSeconds_ = DisplaySeconds;
        } else if (static_cast<double>(deltaSeconds) >= remainingSeconds_) {
            remainingSeconds_ = 0.0;
        } else {
            remainingSeconds_ -= static_cast<double>(deltaSeconds);
        }
    }

    [[nodiscard]] float Alpha() const noexcept {
        if (held_ || remainingSeconds_ >= FadeSeconds) return 1.0f;
        return static_cast<float>(remainingSeconds_ / FadeSeconds);
    }

    [[nodiscard]] bool Visible() const noexcept {
        return held_ || remainingSeconds_ > 0.0;
    }

private:
    // Double retains small ordinary frame deltas through the three-second
    // window; both time and storage remain bounded independently of uptime.
    double remainingSeconds_{};
    bool held_{};
};

} // namespace QuestVr
