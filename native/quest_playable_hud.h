#pragma once

#include "quest_hud_ui.h"
#include "quest_hud_view.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace QuestVr {

// Temporary presentation for the current playable port, not an original
// PlayerPawn/viewport binding. Until independent limb damage is connected,
// every body segment deliberately reflects the same aggregate health value.
// Energy is an unfinished full-scale display, not a simulated energy owner.
inline HudHealthSnapshot BuildPlayableHealthSnapshot(float aggregateHealth) {
    if (!std::isfinite(aggregateHealth))
        throw std::runtime_error("Playable HUD aggregate health is not finite");
    const auto displayed=static_cast<std::int32_t>(std::clamp(aggregateHealth,0.0f,100.0f));
    HudHealthSnapshot snapshot;
    for (auto& part:snapshot.parts) part.displayedHealth=displayed;
    snapshot.energy=100.0f;
    snapshot.energyMax=100.0f;
    return snapshot;
}

// Keep the original 541x69 object-belt artwork and 1..9/0 ordering, but use a
// separate VR scale so the complete bar fits the comfortable central view.
// Original, human-accepted compass/health placement remains unchanged.
inline constexpr float kPlayableBeltPixelsToMeters=0.003f;
inline constexpr float kPlayableBeltCenterX=0.0f;
inline constexpr float kPlayableBeltCenterY=-0.49f;
inline constexpr float kPlayableBeltCenterZ=-kHudVrDistance;
inline constexpr float kPlayableBeltHalfWidth=
    static_cast<float>(kHudBeltWidth)*0.5f*kPlayableBeltPixelsToMeters;
inline constexpr float kPlayableBeltHalfHeight=
    static_cast<float>(kHudBeltHeight)*0.5f*kPlayableBeltPixelsToMeters;

} // namespace QuestVr
