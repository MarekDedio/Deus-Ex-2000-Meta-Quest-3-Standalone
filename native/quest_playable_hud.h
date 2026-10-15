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

// Shared view geometry retains the original 541x69 artwork and 1..9/0 order.
// Catch any future artwork-size change before it breaks the aligned HUD edge.
static_assert(kHudBeltWidth==541u && kHudBeltHeight==69u);

} // namespace QuestVr
