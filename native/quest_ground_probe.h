#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

namespace QuestVr {

inline constexpr float GroundMinimumAbsNormalY = 0.55f;
inline constexpr float GroundBarycentricTolerance = 0.001f;
inline constexpr float GroundMinimumDenominator = 0.000001f;
inline constexpr float GroundMaximumStepUp = 0.45f;
inline constexpr float GroundMaximumDrop = 2.0f;
inline constexpr float PlayerCollisionRadius = 0.28f;

inline bool GroundCollisionReady(bool runtimeAvailable, bool pendingMap, bool transitionMap,
                                 bool actorCollisionPublicationPending = false) {
    return runtimeAvailable && !pendingMap && !transitionMap && !actorCollisionPublicationPending;
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

// A capsule has a footprint, not a point. A point-only probe cannot reach a
// stair's tread before its lower sphere hits the riser; on descent it lowers
// the capsule while its trailing edge still overlaps the previous riser.
// Probe the nearest point in the projected triangle, never an infinite plane.
template <typename Vector, typename Triangle>
std::optional<float> GroundHeightWithinRadius(const Vector& point, const Triangle& triangle,
                                             float radius = PlayerCollisionRadius) {
    if (!std::isfinite(radius) || radius < 0.0f ||
        !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z) ||
        !std::isfinite(triangle.normal.y) ||
        std::fabs(triangle.normal.y) < GroundMinimumAbsNormalY)
        return std::nullopt;
    const float radiusSquared = radius*radius;
    if (!std::isfinite(radiusSquared)) return std::nullopt;
    if (point.x+radius < std::min({triangle.a.x,triangle.b.x,triangle.c.x}) ||
        point.x-radius > std::max({triangle.a.x,triangle.b.x,triangle.c.x}) ||
        point.z+radius < std::min({triangle.a.z,triangle.b.z,triangle.c.z}) ||
        point.z-radius > std::max({triangle.a.z,triangle.b.z,triangle.c.z})) return std::nullopt;
    if (const auto height = GroundHeightAtXZ(point, triangle)) return height;
    float nearestSquared = radiusSquared;
    std::optional<float> height;
    const Vector vertices[] = {triangle.a, triangle.b, triangle.c};
    for (unsigned edge = 0u; edge < 3u; ++edge) {
        const auto& a = vertices[edge];
        const auto& b = vertices[(edge+1u)%3u];
        const float dx = b.x-a.x, dz = b.z-a.z;
        const float lengthSquared = dx*dx+dz*dz;
        if (!std::isfinite(lengthSquared)) return std::nullopt;
        const float fraction = lengthSquared > 0.0f
            ? std::clamp(((point.x-a.x)*dx+(point.z-a.z)*dz)/lengthSquared,0.0f,1.0f) : 0.0f;
        if (!std::isfinite(fraction)) return std::nullopt;
        const Vector nearest{a.x+dx*fraction,a.y+(b.y-a.y)*fraction,a.z+dz*fraction};
        const float distanceSquared = (nearest.x-point.x)*(nearest.x-point.x)+
                                      (nearest.z-point.z)*(nearest.z-point.z);
        if (!std::isfinite(distanceSquared)) return std::nullopt;
        if (distanceSquared <= nearestSquared) {
            if (const auto candidate = GroundHeightAtXZ(nearest,triangle)) {
                nearestSquared = distanceSquared;
                height = candidate;
            }
        }
    }
    return height;
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

template <typename Vector>
struct GroundedHorizontalMotionResult {
    Vector feet;
    bool blocked{};
};

template <typename Vector, typename Triangle, typename ClosestPoint>
bool CapsuleTouchesTriangle(const Vector& feet, const Triangle& triangle, ClosestPoint closestPoint) {
    if (std::fabs(triangle.normal.y) > 0.65f) {
        // Floor contact is intentional. A horizontal ceiling is not a floor
        // and must still be checked when stepping onto a tread underneath it.
        const auto floor = GroundHeightWithinRadius(feet,triangle);
        if (!floor || *floor <= feet.y+0.015f) return false;
    }
    constexpr float sampleHeights[] = {0.3f,0.85f,1.4f};
    constexpr float radiusSquared = PlayerCollisionRadius*PlayerCollisionRadius;
    for (const float height : sampleHeights) {
        const Vector point{feet.x,feet.y+height,feet.z};
        const auto nearest = closestPoint(point,triangle);
        const float dx = point.x-nearest.x,dy = point.y-nearest.y,dz = point.z-nearest.z;
        const float squared = dx*dx+dy*dy+dz*dz;
        if (!std::isfinite(squared) || squared < radiusSquared) return true;
    }
    return false;
}

// Resolve each bounded horizontal sample against the previous sample's feet,
// not the destination's floor. Applying a lower destination height to the
// entire sweep makes a descending capsule collide with the previous tread.
// A refused sweep publishes no partially moved position; the caller retains
// its existing safe room-scale rollback policy.
template <typename Vector, typename FloorAt, typename TouchesWall>
GroundedHorizontalMotionResult<Vector> ResolveGroundedHorizontalMotion(
    const Vector& safeFeet, const Vector& candidateFeet, FloorAt floorAt, TouchesWall touchesWall) {
    GroundedHorizontalMotionResult<Vector> result{safeFeet,false};
    const auto finite = [](const auto& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    const float distance = std::hypot(candidateFeet.x-safeFeet.x,candidateFeet.z-safeFeet.z);
    if (!finite(safeFeet) || !finite(candidateFeet) || !std::isfinite(distance) || distance > 7.68f) {
        result.blocked = true;
        return result;
    }
    const int steps = std::max(1,static_cast<int>(std::ceil(distance/0.12f)));
    float feetY = safeFeet.y;
    for (int step = 1; step <= steps; ++step) {
        const float fraction = static_cast<float>(step)/static_cast<float>(steps);
        Vector sample{safeFeet.x+(candidateFeet.x-safeFeet.x)*fraction,feetY,
                      safeFeet.z+(candidateFeet.z-safeFeet.z)*fraction};
        if (const auto floor = floorAt(sample)) {
            GroundProbe bounded(feetY);
            if (!bounded.Add(*floor)) {
                result.blocked = true;
                return result;
            }
            sample.y = *floor;
        }
        if (touchesWall(sample)) {
            result.blocked = true;
            return result;
        }
        feetY = sample.y;
    }
    result.feet = {candidateFeet.x,feetY,candidateFeet.z};
    return result;
}

} // namespace QuestVr
