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

// Original left-column arrangement: health (0,0,84,106), compass
// (0,110,73,40). A literal 640x480 screen-edge translation put the compass
// about 35 degrees left, which the user found almost outside the Quest FOV.
// Inset the whole column into the comfortable central view without shrinking
// its artwork or changing the original relative alignment/proportions.
inline constexpr float kHudVrPixelsToMeters=0.004f;
inline constexpr float kHudVrDistance=1.65f;
inline constexpr float kHudCompassCenterX=-0.50f;
inline constexpr float kHudCompassCenterY=0.25f;
inline constexpr float kHudCompassHalfWidth=73.0f/2.0f*kHudVrPixelsToMeters;
inline constexpr float kHudCompassHalfHeight=40.0f/2.0f*kHudVrPixelsToMeters;
inline constexpr float kHudHealthCenterX=kHudCompassCenterX+(84.0f-73.0f)/2.0f*kHudVrPixelsToMeters;
inline constexpr float kHudHealthCenterY=kHudCompassCenterY+(110.0f+40.0f/2.0f-106.0f/2.0f)*kHudVrPixelsToMeters;
inline constexpr float kHudHealthHalfWidth=84.0f/2.0f*kHudVrPixelsToMeters;
inline constexpr float kHudHealthHalfHeight=106.0f/2.0f*kHudVrPixelsToMeters;

} // namespace QuestVr
