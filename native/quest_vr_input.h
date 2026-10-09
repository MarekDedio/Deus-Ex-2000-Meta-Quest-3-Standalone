#pragma once

#include <algorithm>
#include <cmath>

// SDK-independent input filtering shared by Quest and synthetic host tests.
namespace QuestVr {

struct Stick {
    float x{};
    float y{};
};

inline constexpr float StickDeadzone = 0.18f;
inline constexpr float AxisEngageThreshold = 0.7f;
inline constexpr float AxisReleaseThreshold = 0.35f;

inline Stick ApplyRadialDeadzone(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return {};
    // Double precision keeps even finite FLT_MAX inputs from overflowing the
    // radius. Clamp magnitude, not components, to preserve diagonal direction.
    const double radius = std::hypot(static_cast<double>(x), static_cast<double>(y));
    const double deadzone = static_cast<double>(StickDeadzone);
    if (radius <= deadzone) return {};
    const double magnitude = (std::min(radius, 1.0) - deadzone) / (1.0 - deadzone);
    return {static_cast<float>(static_cast<double>(x) / radius * magnitude),
            static_cast<float>(static_cast<double>(y) / radius * magnitude)};
}

// One activation per centered excursion. A held/opposite input cannot rearm
// until a finite sample reaches the release band. Invalid data is not neutral.
inline bool UpdateAxis(float axis, bool& latched) {
    if (!std::isfinite(axis)) return false;
    const float magnitude = std::fabs(axis);
    if (latched) {
        if (magnitude <= AxisReleaseThreshold) latched = false;
        return false;
    }
    if (magnitude >= AxisEngageThreshold) {
        latched = true;
        return true;
    }
    return false;
}

} // namespace QuestVr
