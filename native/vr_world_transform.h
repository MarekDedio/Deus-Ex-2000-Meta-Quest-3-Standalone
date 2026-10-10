#pragma once

#include <algorithm>
#include <cmath>

// Shared by the Quest update loop and the desktop regression executable.
// The world pose maps authored map coordinates into OpenXR tracking space.
namespace QuestVr {

constexpr float Pi = 3.14159265358979323846f;
constexpr float DebugHudDistance = 1.05f;
// Render-only clearance for the original static torso. This moves the body,
// never the tracked eyes, floor, collision capsule or saved player position.
constexpr float SelfBodyRearClearanceMeters = 0.16f;

inline constexpr float WorldYAfterVirtualFloorChange(
    float previousWorldY, float previousFloorY, float nextFloorY) {
    // Preserve map feet (virtualFloorY - worldY). A standing floor remains
    // zero after a vertical reference change; a calibrated seated floor moves
    // with that change. Callers validate the bounded floor values first.
    return previousWorldY + (nextFloorY - previousFloorY);
}

template <typename Pose>
Pose HeadLockedHudPose(Pose head) {
    using Vector = decltype(head.Translation);
    head.Translation += head.Rotation.Rotate(Vector{0.0f, 0.0f, -DebugHudDistance});
    return head;
}

template <typename Pose>
Pose FeetLocalSelfBodyPose(Pose head, float horizontalYaw, float virtualFloorY) {
    using Vector = decltype(head.Translation);
    using Rotation = decltype(head.Rotation);
    head.Rotation = Rotation(Vector{0.0f, 1.0f, 0.0f}, horizontalYaw);
    head.Translation += head.Rotation.Rotate(Vector{0.0f, 0.0f, SelfBodyRearClearanceMeters});
    head.Translation.y = virtualFloorY;
    return head;
}

template <typename Vector>
float StableSelfBodyYaw(const Vector& forward, const Vector& right,
    float previousYaw, bool initialized) {
    // A forward projection becomes singular at straight up/down and reverses
    // across it. Freeze only the render body's last heading inside this cone;
    // do not change locomotion, save heading or the tracked camera.
    const float horizontal=std::hypot(forward.x,forward.z);
    if (std::isfinite(horizontal) && horizontal>=0.25f)
        return std::atan2(-forward.x,-forward.z);
    if (initialized && std::isfinite(previousYaw)) return previousYaw;
    const float rightLength=std::hypot(right.x,right.z);
    if (std::isfinite(rightLength) && rightLength>=0.25f)
        return std::atan2(-right.z,right.x);
    return 0.0f;
}

template <typename Pose>
Pose LegacyGlockHandGripPose(Pose grip) {
    using Vector = decltype(grip.Translation);
    using Rotation = decltype(grip.Rotation);
    // Historical Glock diagnostics only; production hands are now prepared
    // directly in grip space by quest_vr_hand_geometry.h. The old grasp runs
    // little-finger to thumb along
    // source-hand +Y. OpenXR grip defines that ray as -Z, not the aim ray.
    // Rotate in hand-local space; the grasp centroid remains at grip origin.
    // This X rotation commutes with the left hand's X reflection.
    grip.Rotation = grip.Rotation * Rotation(Vector{1.0f,0.0f,0.0f},-Pi*0.5f);
    return grip;
}

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

template <typename Vector>
void RecenterReferenceSpace(const Vector& newOriginInPreviousSpace, float newOriginYaw,
                            Vector& worldPosition, float worldYaw, Vector& previousHead) {
    // Keep the player's map position, but honor the new physical forward.
    // Compensating worldYaw too would cancel the system recenter. Room-locked
    // STAGE changes continue to use RebaseReferenceSpace instead.
    const Vector mapHead = StageToLocal(previousHead, worldPosition, worldYaw);
    previousHead = StageToLocal(previousHead, newOriginInPreviousSpace, newOriginYaw);
    const Vector rebasedWorld = StageToLocal(worldPosition, newOriginInPreviousSpace, newOriginYaw);
    worldPosition = RestoreHorizontalHeadPosition(rebasedWorld, previousHead, mapHead, worldYaw);
}

template <typename Vector>
void RestoreSavedMapPose(const Vector& localFeet, float savedLocalHeadYaw,
                         const Vector& currentHeadStage, float currentHeadYaw,
                         Vector& worldPosition, float& worldYaw, float virtualFloorY = 0.0f) {
    // The saved player position/heading belongs to the map, not the room.
    // Reconstruct from the wearer's current origin, yaw and standing height;
    // restoring the old tracking translation would move the player after a
    // room-scale walk, recenter, or new XR session.
    worldYaw = std::remainder(currentHeadYaw - savedLocalHeadYaw, 2.0f * Pi);
    const Vector floorOffset{0.0f, virtualFloorY-localFeet.y, 0.0f};
    worldPosition = RestoreHorizontalHeadPosition(
        floorOffset, currentHeadStage, localFeet, worldYaw);
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
