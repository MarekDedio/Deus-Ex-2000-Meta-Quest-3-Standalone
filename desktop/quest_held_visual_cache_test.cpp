#include "quest_held_visual_cache.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
using QuestVr::HeldVisualCache;
std::size_t checks{}, rejections{};
constexpr const char* Identity = "00_Training.WeaponCrowbar1";
constexpr const char* Map = "00_Training";

void Require(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class Action>
void Reject(Action action) {
    bool refused{};
    try { action(); } catch (const std::invalid_argument&) { refused = true; }
    Require(refused, "Invalid cache input was accepted or threw an unexpected exception");
    ++rejections;
}
void SameContinuations(HeldVisualCache first, HeldVisualCache second) {
    for (float elapsed : std::array<float, 5u>{{0.0f, 0.03125f, 0.03125f, 0.0625f, 1.0f}}) {
        first.Tick(elapsed); second.Tick(elapsed);
        Require(first.LastRevision() == second.LastRevision(), "Rejected mutation changed retained revision");
        for (const auto* identity : std::array<const char*, 3u>{{Identity, "Other.Item", ""}})
            for (const auto* map : std::array<const char*, 3u>{{Map, "00_TrainingCombat", ""}})
                Require(first.NeedsRefresh(identity, map, 999u) == second.NeedsRefresh(identity, map, 999u),
                    "Rejected mutation changed cache keys, invalidation or later timing");
    }
}
void Lifecycle() {
    HeldVisualCache cache;
    Require(!cache.LastRevision(), "Fresh cache claims an observed world revision");
    Require(cache.NeedsRefresh(Identity, Map, 0u), "Fresh cache does not request initial metadata");
    cache.Refreshed(Identity, Map, 41u);
    Require(cache.LastRevision() == 41u, "Refreshed cache lost the observed revision");
    Require(!cache.NeedsRefresh(Identity, Map, 41u), "Fresh metadata is immediately stale");
    Require(!cache.NeedsRefresh("00_training.weaponcrowbar1", "00_TRAINING", 42u),
        "Unreal identity case or a door revision bypassed refresh cadence");
    Require(cache.NeedsRefresh("00_Training.WeaponCombatKnife0", Map, 41u),
        "Equipment identity change did not refresh immediately");
    Require(cache.NeedsRefresh(Identity, "00_TrainingCombat", 41u),
        "Map identity change did not refresh immediately");
    cache.Tick(HeldVisualCache::RefreshSeconds * 0.5f);
    Require(!cache.NeedsRefresh(Identity, Map, std::numeric_limits<std::uint64_t>::max()),
        "Revision churn caused per-frame same-item metadata inspection");
    cache.Tick(HeldVisualCache::RefreshSeconds * 0.5f);
    Require(cache.NeedsRefresh(Identity, Map, 41u),
        "Unchanged world revision incorrectly suppressed periodic overlay inspection");
    Require(cache.LastRevision() == 41u, "NeedsRefresh fabricated an observed metadata revision");
    cache.Refreshed(Identity, Map, 42u);
    cache.Invalidate();
    Require(cache.NeedsRefresh(Identity, Map, 42u), "Explicit restore/worker invalidation was ignored");
    Require(cache.LastRevision() == 42u, "Invalidation discarded diagnostic provenance");
    cache.Refreshed(Identity, Map, 0u);
    Require(cache.LastRevision() == 0u, "A zero/restored world revision is not representable");
    cache.Tick(std::numeric_limits<float>::max());
    Require(cache.NeedsRefresh(Identity, Map, 0u), "Large finite elapsed time wrapped the refresh timer");
    cache.Reset();
    Require(!cache.LastRevision() && cache.NeedsRefresh(Identity, Map, 0u),
        "Session reset retained valid cached metadata");
    cache.Refreshed("", "", 7u);
    Require(!cache.NeedsRefresh("", "", 7u), "Empty equipment/map keys cannot cache a hidden outcome");
    cache.Tick(0.0f); cache.Tick(-0.0f);
    Require(!cache.NeedsRefresh("", "", 99u), "Zero elapsed time advanced the cache clock");
    cache.Tick(HeldVisualCache::RefreshSeconds);
    Require(cache.NeedsRefresh("", "", 7u), "Hidden/unsupported outcome has an unbounded stale lifetime");
}
void InvalidInputs() {
    const std::string longIdentity(HeldVisualCache::MaximumIdentityBytes + 1u, 'a');
    const std::string longMap(HeldVisualCache::MaximumMapBytes + 1u, 'm');
    const std::string nul{"One\0Two", 7u};
    for (unsigned phase = 0u; phase < 3u; ++phase) {
        HeldVisualCache initial;
        if (phase != 0u) initial.Refreshed(Identity, Map, 123u);
        if (phase == 2u) initial.Tick(0.0625f);
        for (float invalid : std::array<float, 6u>{{-1.0f, -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}}) {
            auto value = initial;
            Reject([&] { value.Tick(invalid); });
            SameContinuations(value, initial);
        }
        for (unsigned operation = 0u; operation < 4u; ++operation) {
            auto value = initial;
            Reject([&] {
                if (operation == 0u) value.Refreshed(longIdentity, Map, 456u);
                if (operation == 1u) value.Refreshed("different.valid.identity", longMap, 456u);
                if (operation == 2u) value.Refreshed(nul, Map, 456u);
                if (operation == 3u) value.Refreshed("different.valid.identity", nul, 456u);
            });
            SameContinuations(value, initial);
        }
        Reject([&] { (void)initial.NeedsRefresh(longIdentity, Map, 1u); });
        Reject([&] { (void)initial.NeedsRefresh(Identity, longMap, 1u); });
        Reject([&] { (void)initial.NeedsRefresh(nul, Map, 1u); });
        Reject([&] { (void)initial.NeedsRefresh(Identity, nul, 1u); });
    }
    HeldVisualCache boundary;
    const std::string identity(HeldVisualCache::MaximumIdentityBytes, 'X');
    const std::string map(HeldVisualCache::MaximumMapBytes, 'M');
    boundary.Refreshed(identity, map, std::numeric_limits<std::uint64_t>::max());
    Require(!boundary.NeedsRefresh(std::string(identity.size(), 'x'), std::string(map.size(), 'm'), 0u),
        "Maximum bounded keys or case-insensitive equality failed");
    Require(boundary.LastRevision() == std::numeric_limits<std::uint64_t>::max(),
        "Maximum revision was narrowed or confused with a sentinel");
    boundary.Refreshed("A", "B", 1u);
    Require(!boundary.NeedsRefresh("a", "b", 1u) && boundary.NeedsRefresh(identity, map, 1u),
        "Shorter keys retained stale suffixes from the previous bounded fingerprint");
}
void Cadence() {
    for (unsigned frequency : std::array<unsigned, 5u>{{60u, 72u, 80u, 90u, 120u}}) {
        HeldVisualCache cache;
        std::size_t refreshed{};
        double sinceRefresh{};
        for (unsigned frame = 0u; frame < frequency * 60u; ++frame) {
            const float delta = 1.0f / static_cast<float>(frequency);
            cache.Tick(delta); sinceRefresh += delta;
            // A revision-changing moving door must not force metadata work
            // faster than the ordinary periodic same-item interval.
            if (cache.NeedsRefresh(Identity, Map, frame)) {
                Require(refreshed == 0u || sinceRefresh >= HeldVisualCache::RefreshSeconds,
                    "Same-item metadata refresh exceeded the maximum 10Hz cadence");
                cache.Refreshed(Identity, Map, frame);
                sinceRefresh = 0.0;
                ++refreshed;
                Require(!cache.NeedsRefresh(Identity, Map, frame + 1u),
                    "Unsupported/successful resolution was not throttled after recording");
            }
        }
        Require(refreshed >= 500u && refreshed <= 601u,
            "Refresh clock stalled or performed near-frame-rate metadata work");
    }
}
void LongReplay() {
    HeldVisualCache cache;
    std::uint32_t random = 0x92cd651au;
    double remaining{};
    bool populated{}, invalidated = true;
    unsigned identity{}, map{};
    std::uint64_t revision{};
    const std::array<const char*, 3u> identities{{Identity, "00_Training.WeaponCombatKnife0", ""}};
    const std::array<const char*, 3u> maps{{Map, "00_TrainingCombat", ""}};
    for (std::size_t step = 0u; step < 100'000u; ++step) {
        random = random * 1664525u + 1013904223u;
        const auto action = (random >> 24u) % 8u;
        if (action == 0u) {
            cache.Reset(); populated = false; invalidated = true; remaining = 0.0; revision = 0u;
        } else if (action == 1u) {
            cache.Invalidate(); invalidated = true;
        } else if (action == 2u) {
            identity = (random >> 6u) % 3u; map = (random >> 12u) % 3u; revision = step;
            cache.Refreshed(identities[identity], maps[map], revision);
            populated = true; invalidated = false; remaining = HeldVisualCache::RefreshSeconds;
        } else {
            const float elapsed = static_cast<float>((random >> 8u) % 17u) / 64.0f;
            cache.Tick(elapsed);
            remaining = elapsed >= remaining ? 0.0 : remaining - elapsed;
        }
        Require(cache.LastRevision() == (populated ? std::optional<std::uint64_t>{revision} : std::nullopt),
            "Replay revision provenance differs from the reference model");
        for (unsigned queryIdentity = 0u; queryIdentity < identities.size(); ++queryIdentity)
            for (unsigned queryMap = 0u; queryMap < maps.size(); ++queryMap)
                Require(cache.NeedsRefresh(identities[queryIdentity], maps[queryMap], step + 1u) ==
                    (!populated || invalidated || queryIdentity != identity || queryMap != map || remaining <= 0.0),
                    "Bounded replay disagrees with reference identity/map/invalidation/timing semantics");
    }
}
} // namespace

int main() {
    try {
        static_assert(std::is_trivially_copyable_v<HeldVisualCache>);
        static_assert(sizeof(HeldVisualCache) < 9u * 1024u);
        Lifecycle(); InvalidInputs(); Cadence(); LongReplay();
        std::cout << "PASS held visual cache " << checks << " checks / " << rejections
            << " expected refusals; bounded keys, immediate invalidation and throttled overlay refresh\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL held visual cache after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
