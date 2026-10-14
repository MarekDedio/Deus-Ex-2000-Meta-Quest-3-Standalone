#pragma once

#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace QuestVr {

struct HudOxygenClockInput {
    // Actual local player's HUDHitDisplay.Tick gates, not tracking/camera
    // height guesses. Missing reflected inputs never imply a dry zone.
    bool playerPresent{}, hudVisible{}, localPlayerEligible{};
    std::optional<bool> headInWater;
    std::optional<float> swimTimer, swimDuration;
};
struct HudOxygenClockState {
    // No serialized overrides or InitWindow assignments: original zeroes.
    bool underwater{};
    float breathPercent{};
    bool operator==(const HudOxygenClockState&) const = default;
};
struct HudOxygenClockSnapshot {
    bool underwater{};
    float breathPercent{};
    std::optional<float> swimTimer; // Actual timer for the original O2 flash.
};

// Presentation only. Never advances swimTimer, changes zones, or supplies
// gameplay oxygen. HUDHitDisplay.Tick PCs330..513: entry shows100 first;
// only subsequent submerged ticks compute (100*swimTimer)/swimDuration.
class HudOxygenClock {
public:
    void Reset() {state_={};playerPath_.clear();mapName_.clear();}
    bool ResetForContext(std::string_view player,std::string_view map) {
        Identity(player);Identity(map);
        if(player.empty()!=map.empty()) throw std::runtime_error("HUD oxygen has a partial player/map identity");
        if(player==playerPath_ && map==mapName_) return false;
        std::string nextPlayer(player),nextMap(map);
        state_={};playerPath_=std::move(nextPlayer);mapName_=std::move(nextMap);return true;
    }
    const HudOxygenClockState& ReadState() const {return state_;}
    std::optional<HudOxygenClockSnapshot> Advance(const HudOxygenClockInput& actual) {
        if(!actual.playerPresent || !actual.hudVisible || !actual.localPlayerEligible || !actual.headInWater)
            return std::nullopt; // Preserve original state while Tick is gated.
        auto next=state_;
        if(*actual.headInWater) {
            // Portable missing-data policy: do not expose oxygen with a
            // guessed timer. Duration is read only on the original math path.
            if(!actual.swimTimer || (state_.underwater && !actual.swimDuration)) return std::nullopt;
            if(!std::isfinite(*actual.swimTimer)) throw std::runtime_error("HUD oxygen timer is not finite");
            if(state_.underwater) {
                if(!std::isfinite(*actual.swimDuration) || *actual.swimDuration==0.0f)
                    throw std::runtime_error("HUD oxygen duration has no finite nonzero divisor");
                // Force the same two binary32 native result stores, not
                // (timer/duration)*100, a fused expression, or double math.
                volatile float numerator=100.0f*(*actual.swimTimer);
                if(!std::isfinite(numerator)) throw std::runtime_error("HUD oxygen numerator overflow");
                const float percent=numerator/(*actual.swimDuration);
                if(!std::isfinite(percent)) throw std::runtime_error("HUD oxygen percentage overflow");
                next.breathPercent=percent<0.0f ? 0.0f : percent>100.0f ? 100.0f : percent;
            } else {next.underwater=true;next.breathPercent=100.0f;}
        } else if(state_.underwater) {next.underwater=false;next.breathPercent=100.0f;}
        // A never-submerged dry tick does NOT overwrite initial percent0.
        const HudOxygenClockSnapshot result{next.underwater,next.breathPercent,
            next.underwater ? actual.swimTimer : std::nullopt};
        state_=next;return result;
    }
private:
    static void Identity(std::string_view value) {
        if(value.size()>1024u || value.find('\0')!=std::string_view::npos)
            throw std::runtime_error("HUD oxygen player/map identity exceeds bounds");
    }
    HudOxygenClockState state_;
    std::string playerPath_,mapName_;
};

} // namespace QuestVr
