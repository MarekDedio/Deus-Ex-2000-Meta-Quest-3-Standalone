#pragma once

#include "vr_world_transform.h"
#include <cstdint>
#include <optional>

namespace QuestVr {

// The rendered map maps UE (X,Y,Z) to local (-Z,X,Y), independent of
// tracking-room origin. Derive the compass from that same transform instead
// of raw controller/head yaw or a fabricated PlayerPawn.Rotation.
template<class Vector>
std::optional<std::uint16_t> HudRenderedViewYaw(const Vector& stageForward,
    const Vector& stageRight,float sceneYaw) {
    const auto finite=[](const Vector& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    if (!finite(stageForward) || !finite(stageRight) || !std::isfinite(sceneYaw))
        return std::nullopt;
    const auto forward=StageDirectionToLocal(stageForward,sceneYaw);
    const auto right=StageDirectionToLocal(stageRight,sceneYaw);
    double angle{};
    if (std::hypot(forward.x,forward.z)>0.0001f)
        angle=std::atan2(static_cast<double>(forward.x),-static_cast<double>(forward.z));
    else if (std::hypot(right.x,right.z)>0.0001f)
        angle=std::atan2(static_cast<double>(right.z),static_cast<double>(right.x));
    else return std::nullopt; // Keep the previous display; do not invent north.
    constexpr double unitsPerRadian=65536.0/6.283185307179586476925286766559;
    const auto yaw=static_cast<std::int32_t>(angle*unitsPerRadian);
    return static_cast<std::uint16_t>(static_cast<std::uint32_t>(yaw)&65535u);
}

// Shared eye-relative HUD plane. Keep original artwork scale/aspect and the
// four-pixel health/compass gap, but align the column to the quickbar's left
// edge. User feedback moved the column down/left and the belt substantially
// below the aim point. These constants also drive offline placement checks;
// no separate PC-screen or per-eye offsets may move the three panels apart.
inline constexpr float kHudVrPixelsToMeters=0.004f;
inline constexpr float kHudVrDistance=1.65f;
inline constexpr float kPlayableBeltPixelsToMeters=0.003f;
inline constexpr float kPlayableBeltCenterX=0.0f;
// The first lowered position (-0.84m / 27deg) still obstructed the user's
// view. Move only the belt to about38deg down; keep its bottom within40deg.
inline constexpr float kPlayableBeltCenterY=-1.28f;
inline constexpr float kPlayableBeltCenterZ=-kHudVrDistance;
inline constexpr float kPlayableBeltHalfWidth=541.0f/2.0f*kPlayableBeltPixelsToMeters;
inline constexpr float kPlayableBeltHalfHeight=69.0f/2.0f*kPlayableBeltPixelsToMeters;
inline constexpr float kHudColumnLeftX=kPlayableBeltCenterX-kPlayableBeltHalfWidth;
inline constexpr float kHudCompassHalfWidth=73.0f/2.0f*kHudVrPixelsToMeters;
inline constexpr float kHudCompassHalfHeight=40.0f/2.0f*kHudVrPixelsToMeters;
inline constexpr float kHudHealthHalfWidth=84.0f/2.0f*kHudVrPixelsToMeters;
inline constexpr float kHudHealthHalfHeight=106.0f/2.0f*kHudVrPixelsToMeters;
inline constexpr float kHudCompassCenterX=kHudColumnLeftX+kHudCompassHalfWidth;
inline constexpr float kHudCompassCenterY=0.07f;
inline constexpr float kHudHealthCenterX=kHudColumnLeftX+kHudHealthHalfWidth;
inline constexpr float kHudHealthCenterY=kHudCompassCenterY+
    kHudCompassHalfHeight+4.0f*kHudVrPixelsToMeters+kHudHealthHalfHeight;

} // namespace QuestVr
