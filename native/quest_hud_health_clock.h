#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace QuestVr {

// Original DeusEx.u HUDHitDisplay.InitWindow assigns these values, rather
// than serialized class defaults. SetHitColor's refresh interval is literal.
inline constexpr float kHudHealthDamageFlashSeconds=0.4f;
inline constexpr float kHudHealthHealFlashSeconds=1.0f;
inline constexpr float kHudHealthRefreshSeconds=0.5f;
inline constexpr std::size_t kHudHealthClockIdentityBytes=1024u;
using HudHealthClockInput=std::array<std::optional<std::int32_t>,6>;

struct HudHealthClockPartSnapshot {
    std::optional<std::int32_t> displayedHealth;
    float damageFlashFraction{};
    // Original SetHitColor reached SetTileColor on this presentation tick.
    // This may be true even if the resulting RGB is unchanged at .5s refresh.
    bool colorUpdated{};
};
using HudHealthClockSnapshot=std::array<HudHealthClockPartSnapshot,6>;

struct HudHealthClockPartState {
    // Exactly the CreateBodyPart initial values: the first known positive
    // health value starts the authored one-second reveal from zero, not a
    // fabricated initial fully healthy limb.
    std::int32_t displayedHealth{},lastHealth{},healHealth{};
    float damageCounter{},healCounter{},refreshCounter{};
    // The actual part window keeps its last SetTileColor across the original
    // early return. Counter expiry alone must not clear the applied tint.
    float appliedDamageFlashFraction{};
    bool known{};
    bool operator==(const HudHealthClockPartState&) const = default;
};

// Pure presentation state for six ACTUAL independent limb-health values in
// original Tick order: head, torso, arm-left, arm-right, leg-left, leg-right.
// This never damages, heals or otherwise writes gameplay health. Root/menu
// visibility and whether this is an eligible local player are caller-owned.
class HudHealthClock {
public:
    void Reset() {
        parts_={};playerPath_.clear();mapName_.clear();
    }

    // Call when publishing a bound player/map, including replacement after
    // travel/load. Case-sensitive identity equality deliberately avoids
    // guessing whether two supplied host handles identify the same UObject.
    // Empty/empty means unbound; a partial identity is a caller error.
    bool ResetForContext(std::string_view actualPlayerPath,std::string_view actualMapName) {
        Identity(actualPlayerPath);Identity(actualMapName);
        if (actualPlayerPath.empty()!=actualMapName.empty())
            throw std::runtime_error("HUD health clock has a partial player/map identity");
        if (actualPlayerPath==playerPath_ && actualMapName==mapName_) return false;
        std::string player(actualPlayerPath),map(actualMapName);
        parts_={};playerPath_=std::move(player);mapName_=std::move(map);return true;
    }

    HudHealthClockSnapshot Snapshot() const {return Present(parts_,{});}
    const std::array<HudHealthClockPartState,6>& ReadParts() const {return parts_;}

    HudHealthClockSnapshot Advance(const HudHealthClockInput& actualHealth,float deltaSeconds,
        bool tickEnabled=true) {
        // Portable ingress policy, not a claim that retail validates its
        // engine-supplied delta: fail atomically instead of accepting NaN,
        // infinity or backwards time. Any finite nonnegative interval is O(6).
        if (!std::isfinite(deltaSeconds) || deltaSeconds<0.0f)
            throw std::runtime_error("HUD health clock delta is invalid");
        if (!tickEnabled) return Snapshot(); // Original Tick gates SetHitColor.
        auto next=parts_;
        std::array<bool,6> updated{};
        for (std::size_t i=0u;i<next.size();++i) {
            if (!actualHealth[i]) {
                // Missing data has no retail Int equivalent. Portable policy:
                // hide and reset only this presentation clock; never replace
                // it with aggregate health, a healthy value or stale color.
                next[i]={};continue;
            }
            updated[i]=AdvancePart(next[i],*actualHealth[i],deltaSeconds);
        }
        auto snapshot=Present(next,updated);
        parts_=next;
        return snapshot;
    }

private:
    static void Identity(std::string_view value) {
        if (value.size()>kHudHealthClockIdentityBytes || value.find('\0')!=std::string_view::npos)
            throw std::runtime_error("HUD health clock player/map identity exceeds bounds");
    }

    static bool AdvancePart(HudHealthClockPartState& part,std::int32_t actual,float delta) {
        part.known=true;
        // Preserve original statement order and binary32 storage. A newly
        // observed change resets its timer AFTER this tick's subtraction.
        part.damageCounter=part.damageCounter-delta;
        if (part.damageCounter<0.0f) part.damageCounter=0.0f;
        part.healCounter=part.healCounter-delta;
        if (part.healCounter<0.0f) part.healCounter=0.0f;
        part.refreshCounter=part.refreshCounter-delta;
        if (part.healCounter==0.0f && part.damageCounter==0.0f &&
            part.lastHealth==actual && part.refreshCounter>0.0f) return false;
        if (part.refreshCounter<=0.0f) part.refreshCounter=kHudHealthRefreshSeconds;

        if (actual<part.lastHealth) {
            part.damageCounter=kHudHealthDamageFlashSeconds;
            part.displayedHealth=actual;
        } else if (actual>part.lastHealth) {
            part.healCounter=kHudHealthHealFlashSeconds;
            part.healHealth=part.displayedHealth;
        }
        part.lastHealth=actual;
        if (part.healCounter>0.0f) {
            const float mult=part.healCounter/kHudHealthHealFlashSeconds;
            // Core.Object.Subtract_IntInt is wrapping int32, followed by
            // IntToFloat, multiply, add and FloatToInt truncation. Keep those
            // operations separate; don't clamp actual health to [0,100].
            const auto difference=std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(part.healHealth)-static_cast<std::uint32_t>(actual));
            // A volatile binary32 intermediate prevents cross-statement FMA
            // contraction from merging original separate VM math operations.
            const volatile float scaled=static_cast<float>(difference)*mult;
            const float displayed=static_cast<float>(actual)+scaled;
            // Portable refusal for retail's undefined out-of-range conversion.
            if (!std::isfinite(displayed) || static_cast<double>(displayed)<-2147483648.0 ||
                static_cast<double>(displayed)>=2147483648.0)
                throw std::runtime_error("HUD health interpolation exceeds int32 range");
            part.displayedHealth=static_cast<std::int32_t>(displayed);
        } else part.displayedHealth=actual;
        part.appliedDamageFlashFraction=part.damageCounter/kHudHealthDamageFlashSeconds;
        return true;
    }

    static HudHealthClockSnapshot Present(const std::array<HudHealthClockPartState,6>& parts,
        const std::array<bool,6>& updated) {
        HudHealthClockSnapshot snapshot;
        for (std::size_t i=0u;i<parts.size();++i) if (parts[i].known) {
            snapshot[i].displayedHealth=parts[i].displayedHealth;
            snapshot[i].damageFlashFraction=parts[i].appliedDamageFlashFraction;
            snapshot[i].colorUpdated=updated[i];
        }
        return snapshot;
    }

    std::array<HudHealthClockPartState,6> parts_{};
    std::string playerPath_,mapName_;
};

} // namespace QuestVr
