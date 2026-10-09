#pragma once

#include <cmath>
#include <optional>

namespace QuestVr {

inline constexpr float GroundMinimumAbsNormalY = 0.55f;
inline constexpr float GroundBarycentricTolerance = 0.001f;
inline constexpr float GroundMinimumDenominator = 0.000001f;
inline constexpr float GroundMaximumStepUp = 0.45f;
inline constexpr float GroundMaximumDrop = 2.0f;

inline bool GroundCollisionReady(bool runtimeAvailable, bool pendingMap, bool transitionMap) {
    return runtimeAvailable && !pendingMap && !transitionMap;
}

// Geometric height only: deliberately independent of the step/drop interval,
// so diagnostics can inspect a plane even when the bounded probe rejects it.
// Keep the original absolute-normal test: coordinate reflection can give an
// authored floor a negative normal Y. This is not an oriented-floor/physics solver.
template <typename Vector, typename Triangle>
std::optional<float> GroundHeightAtXZ(const Vector& point, const Triangle& triangle) {
    const auto finite = [](const auto& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    if (!finite(point) || !finite(triangle.a) || !finite(triangle.b) ||
        !finite(triangle.c) || !finite(triangle.normal) ||
        std::fabs(triangle.normal.y) < GroundMinimumAbsNormalY) return std::nullopt;
    const float denominator =
        (triangle.b.z - triangle.c.z) * (triangle.a.x - triangle.c.x) +
        (triangle.c.x - triangle.b.x) * (triangle.a.z - triangle.c.z);
    if (!std::isfinite(denominator) || std::fabs(denominator) < GroundMinimumDenominator)
        return std::nullopt;
    const float u = ((triangle.b.z - triangle.c.z) * (point.x - triangle.c.x) +
        (triangle.c.x - triangle.b.x) * (point.z - triangle.c.z)) / denominator;
    const float v = ((triangle.c.z - triangle.a.z) * (point.x - triangle.c.x) +
        (triangle.a.x - triangle.c.x) * (point.z - triangle.c.z)) / denominator;
    const float w = 1.0f - u - v;
    if (!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(w) ||
        u < -GroundBarycentricTolerance || v < -GroundBarycentricTolerance || w < -GroundBarycentricTolerance)
        return std::nullopt;
    const float height = u * triangle.a.y + v * triangle.b.y + w * triangle.c.y;
    return std::isfinite(height) ? std::optional<float>(height) : std::nullopt;
}

class GroundProbe {
public:
    explicit GroundProbe(float feetY) : feetY_(feetY) {}

    // True means the candidate is in the original inclusive step/drop band;
    // a lower accepted plane need not replace the highest accumulated plane.
    bool Add(float floor) {
        if (!std::isfinite(feetY_) || !std::isfinite(floor)) return false;
        const float upper = feetY_ + GroundMaximumStepUp;
        const float lower = feetY_ - GroundMaximumDrop;
        if (!std::isfinite(upper) || !std::isfinite(lower) || floor > upper || floor < lower)
            return false;
        if (!height_ || floor > *height_) height_ = floor;
        return true;
    }

    std::optional<float> Height() const { return height_; }

private:
    float feetY_{};
    std::optional<float> height_;
};

} // namespace QuestVr
