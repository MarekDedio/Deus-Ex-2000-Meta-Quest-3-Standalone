#pragma once

#include <algorithm>
#include <cmath>

// Shared by the Quest update loop and the desktop regression executable.
// The world pose maps authored map coordinates into OpenXR tracking space.
namespace QuestVr {

constexpr float Pi = 3.14159265358979323846f;

template <typename Vector>
Vector StageDirectionToLocal(const Vector& stage, float worldYaw) {
    const float cosine = std::cos(worldYaw);
    const float sine = std::sin(worldYaw);
    return {cosine * stage.x - sine * stage.z, stage.y,
            sine * stage.x + cosine * stage.z};
}

template <typename Vector>
Vector StageToLocal(const Vector& stage, const Vector& worldPosition, float worldYaw) {
    const Vector displacement{stage.x - worldPosition.x,
                              stage.y - worldPosition.y,
                              stage.z - worldPosition.z};
    return StageDirectionToLocal(displacement, worldYaw);
}

template <typename Vector>
void AnchorSpawnToHead(const Vector& headStage, Vector& worldPosition) {
    // Tracking-space zero can be several metres from the wearer. The map's
    // PlayerStart must appear at the tracked head's current horizontal location.
    worldPosition.x = headStage.x;
    worldPosition.z = headStage.z;
}

template <typename Vector>
void SnapTurnAroundHead(float deltaYaw, const Vector& headStage,
                       Vector& worldPosition, float& worldYaw) {
    const Vector pivotLocal = StageToLocal(headStage, worldPosition, worldYaw);
    worldYaw = std::remainder(worldYaw + deltaYaw, 2.0f * Pi);
    const float cosine = std::cos(worldYaw);
    const float sine = std::sin(worldYaw);
    worldPosition.x = headStage.x -
        (cosine * pivotLocal.x + sine * pivotLocal.z);
    worldPosition.z = headStage.z -
        (-sine * pivotLocal.x + cosine * pivotLocal.z);
}

template <typename Vector>
Vector HorizontalHeadRight(const Vector& headForward, const Vector& headRight) {
    const float forwardLength = std::hypot(headForward.x, headForward.z);
    if (forwardLength > 0.0001f) {
        return {-headForward.z / forwardLength, 0.0f,
                headForward.x / forwardLength};
    }
    const float rightLength = std::hypot(headRight.x, headRight.z);
    if (rightLength > 0.0001f) {
        return {headRight.x / rightLength, 0.0f, headRight.z / rightLength};
    }
    return {1.0f, 0.0f, 0.0f};
}

template <typename Vector>
Vector LocomotionCandidate(const Vector& worldPosition, const Vector& stageRight,
                           float stickX, float stickY, float speed,
                           float deltaSeconds) {
    const float stickLength = std::max(1.0f, std::hypot(stickX, stickY));
    // Bound a hitch to less than the wall capsule's diameter so virtual movement
    // cannot leap across an entire wall between endpoint collision checks.
    const float distance = speed * std::clamp(deltaSeconds, 0.0f, 0.1f) / stickLength;
    const float moveX = stickX * stageRight.x + stickY * stageRight.z;
    const float moveZ = stickX * stageRight.z - stickY * stageRight.x;
    return {worldPosition.x - moveX * distance, worldPosition.y,
            worldPosition.z - moveZ * distance};
}

template <typename Vector>
Vector RestoreHorizontalHeadPosition(const Vector& worldPosition, const Vector& headStage,
                                     const Vector& safeLocalHead, float worldYaw) {
    const float cosine = std::cos(worldYaw);
    const float sine = std::sin(worldYaw);
    // Reconstruct against the NEW yaw. Adding a stage-space head delta after a
    // simultaneous snap turn would rotate the rollback and miss the safe point.
    return {headStage.x - cosine * safeLocalHead.x - sine * safeLocalHead.z,
            worldPosition.y,
            headStage.z + sine * safeLocalHead.x - cosine * safeLocalHead.z};
}

template <typename Vector>
void RebaseReferenceSpace(const Vector& newOriginInPreviousSpace, float newOriginYaw,
                          Vector& worldPosition, float& worldYaw, Vector& previousHead) {
    worldPosition = StageToLocal(worldPosition, newOriginInPreviousSpace, newOriginYaw);
    previousHead = StageToLocal(previousHead, newOriginInPreviousSpace, newOriginYaw);
    worldYaw = std::remainder(worldYaw - newOriginYaw, 2.0f * Pi);
}

template <typename Vector, typename TouchesWall>
bool HorizontalMotionBlocked(const Vector& safeHead, const Vector& candidateHead,
                             TouchesWall touchesWall) {
    const float distance = std::hypot(candidateHead.x - safeHead.x,
                                      candidateHead.z - safeHead.z);
    // Capsule radius is 0.28m; sample more often than its diameter so a thin
    // wall cannot lie entirely between two checks. Large tracking jumps fail
    // closed and are rebased to the safe head position by the caller.
    if (!std::isfinite(distance) || distance > 7.68f) return true;
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / 0.12f)));
    for (int step = 1; step <= steps; ++step) {
        const float fraction = static_cast<float>(step) / static_cast<float>(steps);
        const Vector sample{safeHead.x + (candidateHead.x - safeHead.x) * fraction,
                            safeHead.y + (candidateHead.y - safeHead.y) * fraction,
                            safeHead.z + (candidateHead.z - safeHead.z) * fraction};
        if (touchesWall(sample)) return true;
    }
    return false;
}

} // namespace QuestVr
