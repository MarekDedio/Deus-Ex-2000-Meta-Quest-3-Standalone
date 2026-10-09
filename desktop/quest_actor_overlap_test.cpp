#include "quest_actor_overlap.h"

#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <set>

namespace {
using namespace QuestVr;
std::size_t checks{}, refusals{};
void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Reject(const std::function<void()>& call, const std::string& message) {
    bool rejected{};
    try { call(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, message); ++refusals;
}
ActorOverlapRecord Actor(std::uintptr_t identity, float x = 0.0f, float y = 0.0f,
                         float z = 0.0f, float radius = 10.0f, float height = 20.0f) {
    return {identity, {x, y, z}, radius, height, false, true};
}
std::vector<std::uintptr_t> Sorted(std::vector<std::uintptr_t> values) {
    std::sort(values.begin(), values.end()); return values;
}
bool Hit(const ActorCollisionRegistry& registry, std::uintptr_t identity,
         ActorOverlapVec3 location, float height = 1.0f, float radius = 1.0f) {
    const auto result = registry.Query(location, height, radius);
    return std::find(result.actors.begin(), result.actors.end(), identity) != result.actors.end();
}
static_assert(noexcept(std::declval<ActorCollisionRegistry&>().Swap(
    std::declval<ActorCollisionRegistry&>())));
static_assert(noexcept(std::declval<ActorCollisionRegistry&>().Clear()));

// Independent literal grid/cylinder oracle, keeping baked bucket membership
// separate from current reflected fields. It deliberately does NOT call the
// implementation's cell, hash or narrow-phase helpers. Inputs here are bounded
// fixture coordinates, so the unsafe pin's int conversion never overflows.
struct Oracle {
    std::map<std::uintptr_t, ActorOverlapRecord> live;
    std::map<std::uintptr_t, ActorCollisionBox> registered;
    std::set<std::uintptr_t> inserted;
    std::map<std::uint32_t, std::vector<std::uintptr_t>> buckets;
    static std::uint32_t Key(int x, int y, int z) {
        return static_cast<std::uint32_t>(((x & 1023) << 20) | ((y & 1023) << 10) | (z & 1023));
    }
    template<class F> static void Cells(const ActorCollisionBox& box, F call) {
        const int x0 = static_cast<int>(std::floor((box.location.x - box.extents.x) * (1.0f / 256.0f)));
        const int y0 = static_cast<int>(std::floor((box.location.y - box.extents.y) * (1.0f / 256.0f)));
        const int z0 = static_cast<int>(std::floor((box.location.z - box.extents.z) * (1.0f / 256.0f)));
        const int x1 = static_cast<int>(std::floor((box.location.x + box.extents.x) * (1.0f / 256.0f))) + 1;
        const int y1 = static_cast<int>(std::floor((box.location.y + box.extents.y) * (1.0f / 256.0f))) + 1;
        const int z1 = static_cast<int>(std::floor((box.location.z + box.extents.z) * (1.0f / 256.0f))) + 1;
        for (int z = z0; z < z1; ++z) for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) call(Key(x, y, z));
    }
    void Add(const ActorOverlapRecord& actor, std::optional<ActorCollisionBox> overrideBox = {}) {
        live[actor.identity] = actor;
        if (!actor.collideActors) return;
        const auto box = overrideBox.value_or(ActorCollisionBox{
            actor.location, {actor.radius, actor.radius, actor.height}});
        registered[actor.identity] = box; inserted.insert(actor.identity);
        Cells(box, [&](std::uint32_t key) { buckets[key].push_back(actor.identity); });
    }
    void Remove(std::uintptr_t identity) {
        if (!inserted.count(identity)) return;
        Cells(registered.at(identity), [&](std::uint32_t key) {
            const auto found = buckets.find(key);
            if (found == buckets.end()) return;
            auto& values = found->second;
            values.erase(std::remove(values.begin(), values.end(), identity), values.end());
            if (values.empty()) buckets.erase(found);
        });
        inserted.erase(identity);
    }
    std::vector<std::uintptr_t> Query(ActorOverlapVec3 origin, float height, float radius) const {
        std::set<std::uintptr_t> seen;
        std::vector<std::uintptr_t> hits;
        Cells({origin, {radius, radius, height}}, [&](std::uint32_t key) {
            const auto bucket = buckets.find(key);
            if (bucket == buckets.end()) return;
            for (const auto identity : bucket->second) {
                if (!seen.insert(identity).second) continue;
                const auto& actor = live.at(identity);
                if (actor.brush) continue;
                const double dx = static_cast<double>(origin.x) - static_cast<double>(actor.location.x);
                const double dy = static_cast<double>(origin.y) - static_cast<double>(actor.location.y);
                const double dz = static_cast<double>(origin.z) - static_cast<double>(actor.location.z);
                const double h = static_cast<double>(height) + static_cast<double>(actor.height);
                const double r = static_cast<double>(radius) + static_cast<double>(actor.radius);
                if (std::abs(dz) < h && dx * dx + dy * dy < r * r) hits.push_back(identity);
            }
        });
        return hits;
    }
    std::size_t Links() const {
        std::size_t count{}; for (const auto& bucket : buckets) count += bucket.second.size(); return count;
    }
};

void GeometryAndOrdering() {
    ActorCollisionRegistry registry;
    Require(registry.Query({}, 1.0f, 1.0f).actors.empty(), "Empty registry query fabricated an actor");
    registry.Add(Actor(9u)); registry.Add(Actor(2u)); registry.Add(Actor(7u));
    const auto result = registry.Query({}, 1.0f, 1.0f);
    Require(result.actors == std::vector<std::uintptr_t>({9u, 2u, 7u}), "Encounter order was sorted or fabricated");
    Require(result.stats.actorsTested == 3u && result.stats.bucketEntries == 24u,
            "Cross-cell duplicates were not scanned and narrow-phase deduplicated");
    Require(Sorted(result.actors) == std::vector<std::uintptr_t>({2u, 7u, 9u}), "Caller address-token sort fixture failed");
    Require(Hit(registry, 9u, {}), "Newly registered self was excluded");
    Require(!Hit(registry, 9u, {11.0f, 0.0f, 0.0f}), "Exact horizontal contact was inclusive");
    Require(Hit(registry, 9u, {std::nextafter(11.0f, 0.0f), 0.0f, 0.0f}), "Inside horizontal contact was missed");
    Require(!Hit(registry, 9u, {0.0f, 0.0f, 21.0f}), "Exact vertical contact was inclusive");
    Require(Hit(registry, 9u, {0.0f, 0.0f, std::nextafter(21.0f, 0.0f)}), "Inside vertical contact was missed");
    Require(!Hit(registry, 9u, {8.0f, 8.0f, 0.0f}), "Cylinder query used a square horizontal extent");
    auto brush = Actor(9u); brush.brush = true; registry.UpdateLive(brush);
    Require(!Hit(registry, 9u, {}), "Brush actor entered cylinder narrow phase");
    brush.brush = false; brush.collideActors = false; registry.UpdateLive(brush);
    Require(Hit(registry, 9u, {}), "Live bCollideActors incorrectly repaired registered membership");
    registry.Remove(9u);
    Require(!Hit(registry, 9u, {}) && !registry.Find(9u)->inserted, "Remove failed to use cached registration");
    registry.Add(brush);
    Require(!registry.Find(9u)->inserted && !Hit(registry, 9u, {}), "False collideActors Add registered an actor");
    Require(registry.Query({}, 1.0f, 1.0f, true).actors.empty(), "Visibility-only actor query ran narrow phase");
    registry.Clear();
    Require(!registry.ActorCount() && !registry.BucketCount() && !registry.LinkCount(), "Clear retained registry state");

    ActorCollisionRegistry zero;
    zero.Add(Actor(1u, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    Require(zero.LinkCount() == 1u && !Hit(zero, 1u, {}, 0.0f, 0.0f), "Zero dimensions skipped registration or created overlap");
    Require(Hit(zero, 1u, {}), "Point actor did not overlap a nonzero query cylinder");
    for (float coordinate : {-256.0f, std::nextafter(-256.0f, -1000.0f), std::nextafter(-256.0f, 0.0f),
                             -0.01f, 0.0f, 255.99f, 256.0f}) {
        ActorCollisionRegistry edge;
        edge.Add(Actor(1u, coordinate, coordinate, coordinate, 0.0f, 0.0f));
        Require(edge.LinkCount() == 1u && Hit(edge, 1u, {coordinate, coordinate, coordinate}),
                "Negative/boundary cell floor math failed");
    }
    ActorCollisionRegistry rounded;
    rounded.Add(Actor(1u, 16777216.0f, 16777216.0f, 16777216.0f, 0.0f, 0.0f));
    const auto largeFloat = rounded.Query({16777216.0f, 16777216.0f, 16777216.0f}, 0.5f, 0.5f);
    Require(largeFloat.stats.cells == 1u && largeFloat.actors == std::vector<std::uintptr_t>({1u}),
            "Cell endpoints used double instead of pinned float boundary rounding");
    ActorCollisionRegistry intBoundary;
    const float minimumCell = -549755813888.0f;
    intBoundary.Add(Actor(1u, minimumCell, minimumCell, minimumCell, 0.0f, 0.0f));
    Require(Hit(intBoundary, 1u, {minimumCell, minimumCell, minimumCell}), "Exact int32 minimum cell was refused");
}

void CachedMembership() {
    ActorCollisionRegistry registry;
    auto actor = Actor(1u, 10.0f, 10.0f, 10.0f, 2.0f, 2.0f);
    registry.Add(actor);
    actor.location.x = 266.0f; registry.UpdateLive(actor);
    Require(!Hit(registry, 1u, actor.location), "Plain reflected move refreshed stale collision hash");
    Require(!Hit(registry, 1u, {10.0f, 10.0f, 10.0f}), "Narrow phase used old registration location");
    const auto* cached = registry.Find(1u);
    Require(cached && cached->registered.location.x == 10.0f && cached->live.location.x == 266.0f,
            "Live and cached registration state were conflated");
    registry.Remove(1u); registry.Add(actor);
    Require(Hit(registry, 1u, actor.location), "Pinned Remove/assign/Add sequence failed to relocate membership");
    const auto snapshot = registry;
    registry.Remove(1u); registry.UpdateLive(Actor(1u));
    Require(Hit(snapshot, 1u, actor.location) && !Hit(registry, 1u, actor.location), "Journal copy aliased mutable registry");

    ActorCollisionRegistry alias;
    actor = Actor(2u, 10.0f, 10.0f, 10.0f, 2.0f, 2.0f); alias.Add(actor);
    actor.location.x += 262144.0f; alias.UpdateLive(actor);
    Require(Hit(alias, 2u, actor.location), "10-bit cell hash aliasing was replaced by unbounded coordinates");
    actor.location.x += 256.0f; alias.UpdateLive(actor);
    Require(!Hit(alias, 2u, actor.location), "Nearby non-alias cell incorrectly matched stale registration");

    ActorCollisionRegistry repeated;
    auto first = Actor(3u, 10.0f, 10.0f, 10.0f, 2.0f, 2.0f);
    repeated.Add(first); repeated.Add(first);
    Require(repeated.LinkCount() == 2u && repeated.Query(first.location, 1.0f, 1.0f).actors.size() == 1u,
            "Repeated Add was silently deduplicated or duplicated query result");
    auto second = first; second.location.x += 512.0f; repeated.Add(second);
    repeated.Remove(3u);
    Require(repeated.LinkCount() == 2u && !repeated.Find(3u)->inserted, "Remove scrubbed older cached buckets");
    repeated.UpdateLive(first);
    Require(Hit(repeated, 3u, first.location), "Older duplicate Add ghost membership was silently repaired");
    repeated.Remove(3u);
    Require(repeated.LinkCount() == 2u, "Uninserted Remove altered historical ghost memberships");

    ActorCollisionRegistry transformed;
    auto mover = Actor(4u, 10.0f, 10.0f, 10.0f, 2.0f, 2.0f); mover.brush = true;
    transformed.Add(mover, ActorCollisionBox{{522.0f, 10.0f, 10.0f}, {2.1f, 2.1f, 2.1f}});
    Require(transformed.Find(4u)->registered.location.x == 522.0f && !Hit(transformed, 4u, mover.location),
            "Explicit Mover registration box was replaced with its cylinder");
    mover.brush = false; mover.location.x = 522.0f; transformed.UpdateLive(mover);
    Require(Hit(transformed, 4u, mover.location), "Transformed cached box was not retained when live Brush changed");

    ActorCollisionRegistry wrappedRegistration;
    auto wrapped = Actor(5u, 131072.0f, 10.0f, 10.0f, 1.0f, 1.0f);
    wrappedRegistration.Add(wrapped, ActorCollisionBox{{0.0f, 10.0f, 10.0f}, {131072.0f, 0.0f, 0.0f}});
    Require(wrappedRegistration.LinkCount() == 1025u && wrappedRegistration.BucketCount() == 1024u,
            "Wide registration lost repeated 10-bit alias bucket insertion");
    const auto wrapQuery = wrappedRegistration.Query(wrapped.location, 0.5f, 0.5f);
    Require(wrapQuery.actors == std::vector<std::uintptr_t>({5u}) && wrapQuery.stats.bucketEntries == 3u,
            "Aliased registration bucket duplicates escaped narrow-phase deduplication");
    wrappedRegistration.Remove(5u);
    Require(!wrappedRegistration.LinkCount() && !wrappedRegistration.BucketCount(),
            "Remove failed to erase all repeated alias memberships");
}

void LargeQueriesAndRefusals() {
    ActorCollisionRegistry registry;
    registry.Add(Actor(1u));
    const auto guarded = registry.Query({}, 1.0f, 12672.0f);
    Require(guarded.actors.empty() && guarded.stats.oversizedQuery && guarded.stats.steps == 0u,
            "100-cell axis guard threw, scanned, or returned a hit");
    const auto allowed = registry.Query({128.0f, 128.0f, 0.0f}, 1.0f, 12543.0f);
    Require(!allowed.stats.oversizedQuery && !allowed.actors.empty() && allowed.stats.cells == 99u * 99u * 2u,
            "99-cell axis was refused or rounded incorrectly by pin guard");
    Require(registry.Query({}, 12672.0f, 1.0f).stats.oversizedQuery, "100-cell vertical axis guard was skipped");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    Reject([&] { registry.Add(Actor(0u)); }, "Null identity accepted");
    Reject([&] { registry.Add(Actor(2u, nan)); }, "NaN actor position accepted");
    Reject([&] { registry.Query({infinity, 0.0f, 0.0f}, 1.0f, 1.0f); }, "Infinite query accepted");
    Reject([&] { registry.Add(Actor(2u, 0.0f, 0.0f, 0.0f, -1.0f)); }, "Negative collision radius accepted");
    ActorCollisionRegistry disabled;
    auto negative=Actor(9u,0.0f,0.0f,0.0f,-3.5f,-8.25f); negative.collideActors=false;
    disabled.UpdateLive(negative); disabled.Add(negative);
    Require(disabled.Find(9u) && disabled.Find(9u)->live.radius==-3.5f &&
        disabled.Find(9u)->live.height==-8.25f && !disabled.Find(9u)->inserted && disabled.LinkCount()==0u,
        "Disabled actor lost native reflected negative bounds or fabricated collision membership");
    Reject([&] { registry.Query({}, -1.0f, 1.0f); }, "Negative query height accepted");
    Reject([&] { registry.Query({std::numeric_limits<float>::max(), 0.0f, 0.0f}, 1.0f, 1.0f); },
           "Out-of-int32 cell conversion accepted");
    Reject([&] { registry.Add(Actor(2u), ActorCollisionBox{{}, {std::numeric_limits<float>::max(), 1.0f, 1.0f}}); },
           "Registration coordinate overflow accepted");
    ActorOverlapLimits bad; bad.links = 0u;
    Reject([&] { ActorCollisionRegistry invalid(bad); }, "Zero limits accepted");
    bad = {}; ++bad.querySteps;
    Reject([&] { ActorCollisionRegistry invalid(bad); }, "Enlarged fixed limits accepted");

    ActorOverlapLimits small; small.actors = 1u;
    ActorCollisionRegistry actors(small); actors.Add(Actor(1u));
    Reject([&] { actors.Add(Actor(2u)); }, "Actor count limit ignored");
    Require(actors.ActorCount() == 1u && actors.LinkCount() == 8u && Hit(actors, 1u, {}), "Actor refusal partially mutated registry");
    small = {}; small.links = 8u;
    ActorCollisionRegistry links(small); links.Add(Actor(1u));
    Reject([&] { links.Add(Actor(2u)); }, "Aggregate membership limit ignored");
    Require(links.ActorCount() == 1u && links.LinkCount() == 8u, "Link refusal retained actor or membership");
    small = {}; small.registrationCells = 1u;
    ActorCollisionRegistry cells(small);
    Reject([&] { cells.Add(Actor(1u)); }, "Per-registration cell limit ignored");
    Require(cells.ActorCount() == 0u && cells.BucketCount() == 0u, "Cell refusal partially mutated registry");
    small = {}; small.buckets = 1u;
    ActorCollisionRegistry buckets(small);
    Reject([&] { buckets.Add(Actor(1u)); }, "Bucket count limit ignored");
    Require(buckets.ActorCount() == 0u && buckets.BucketCount() == 0u, "Bucket refusal partially mutated registry");
    small = {}; small.mutationSteps = 8u;
    ActorCollisionRegistry mutation(small); mutation.Add(Actor(1u));
    Reject([&] { mutation.Add(Actor(2u)); }, "Mutation work limit ignored");
    Reject([&] { mutation.Remove(1u); }, "Removal work limit ignored");
    Require(mutation.LinkCount() == 8u && mutation.Find(1u)->inserted && Hit(mutation, 1u, {}),
            "Mutation/removal refusal partially changed cached registry");
    small = {}; small.querySteps = 1u;
    ActorCollisionRegistry query(small); query.Add(Actor(1u));
    Reject([&] { query.Query({}, 1.0f, 1.0f); }, "Query work limit ignored");
    Require(query.Find(1u)->inserted && query.LinkCount() == 8u, "Query refusal mutated registry");
}

void SnapshotPublicationAndRollback() {
    ActorCollisionRegistry live;
    auto original = Actor(1u, 10.0f, 10.0f, 10.0f, 2.0f, 2.0f);
    live.Add(original); live.Add(original);
    auto relocated = original; relocated.location.x += 512.0f;
    live.Add(relocated); live.Remove(original.identity);
    live.UpdateLive(original);
    Require(live.LinkCount() == 2u && Hit(live, 1u, original.location) &&
            !live.Find(1u)->inserted, "Rollback fixture lost historical ghost memberships");

    auto before = live;
    live.Add(Actor(2u, 10.0f, 10.0f, 10.0f, 2.0f, 2.0f));
    original.location.x += 256.0f; live.UpdateLive(original);
    live.Swap(before);
    Require(live.ActorCount() == 1u && live.LinkCount() == 2u && live.BucketCount() == 1u &&
            live.Find(1u)->live.location.x == 10.0f && !live.Find(1u)->inserted &&
            Hit(live, 1u, {10.0f, 10.0f, 10.0f}) && !live.Find(2u),
            "Noexcept rollback swap failed to restore complete cached/live registry state");
    Require(before.ActorCount() == 2u && before.LinkCount() == 3u &&
            before.Find(1u)->live.location.x == 266.0f && Hit(before, 2u, {10.0f, 10.0f, 10.0f}),
            "Rollback swap discarded mutated registry instead of transferring it");

    ActorCollisionRegistry restored;
    restored.Add(Actor(7u, 778.0f, 10.0f, 10.0f, 2.0f, 2.0f));
    live.Swap(restored);
    Require(live.ActorCount() == 1u && live.LinkCount() == 1u && live.Find(7u) && !live.Find(1u) &&
            Hit(live, 7u, {778.0f, 10.0f, 10.0f}) && !Hit(live, 1u, {10.0f, 10.0f, 10.0f}),
            "Prepared save publication merged old hash memberships");
    live.Swap(live);
    Require(live.ActorCount() == 1u && live.LinkCount() == 1u && Hit(live, 7u, {778.0f, 10.0f, 10.0f}),
            "Self-swap damaged collision registry");

    ActorOverlapLimits tightLimits; tightLimits.actors = 1u;
    ActorCollisionRegistry tight(tightLimits), ordinary;
    tight.Add(Actor(10u)); ordinary.Add(Actor(20u));
    tight.Swap(ordinary);
    tight.Add(Actor(21u));
    Require(tight.ActorCount() == 2u && tight.Find(20u) && tight.Find(21u) && !tight.Find(10u),
            "Swap kept old restrictive limits instead of transferring complete snapshot");
    Reject([&] { ordinary.Add(Actor(11u)); }, "Swapped actor limit was not preserved");
    ordinary.Clear();
    ordinary.Add(Actor(11u));
    Reject([&] { ordinary.Add(Actor(12u)); }, "Clear reset tightened constructor limits");
    Require(ordinary.ActorCount() == 1u && ordinary.Find(11u) && !ordinary.Find(10u),
            "Clear did not reset actors while preserving configured limits");
}

void OracleComparison() {
    std::mt19937 random(0x278u);
    std::uniform_int_distribution<int> coordinate(-1024, 1024), size(0, 150);
    ActorCollisionRegistry registry;
    Oracle oracle;
    std::vector<ActorOverlapRecord> records;
    for (std::uintptr_t identity = 1u; identity <= 160u; ++identity) {
        auto actor = Actor(identity, static_cast<float>(coordinate(random)), static_cast<float>(coordinate(random)),
                           static_cast<float>(coordinate(random)), static_cast<float>(size(random)), static_cast<float>(size(random)));
        actor.brush = identity % 13u == 0u; actor.collideActors = identity % 11u != 0u;
        registry.Add(actor); oracle.Add(actor); records.push_back(actor);
    }
    for (std::size_t iteration = 0u; iteration < 3000u; ++iteration) {
        const ActorOverlapVec3 origin{static_cast<float>(coordinate(random)), static_cast<float>(coordinate(random)),
                                      static_cast<float>(coordinate(random))};
        const float height = static_cast<float>(size(random)), radius = static_cast<float>(size(random));
        Require(registry.Query(origin, height, radius).actors == oracle.Query(origin, height, radius),
                "Registry diverged from independent cached-grid cylinder oracle");
        if (iteration % 7u == 0u) {
            auto& actor = records[random() % records.size()];
            actor.location.x += 256.0f; actor.height = static_cast<float>(size(random));
            actor.brush = !actor.brush; actor.collideActors = !actor.collideActors;
            registry.UpdateLive(actor); oracle.live[actor.identity] = actor;
        }
        if (iteration % 29u == 0u) {
            auto& actor = records[random() % records.size()];
            registry.Remove(actor.identity); oracle.Remove(actor.identity);
            registry.Add(actor); oracle.Add(actor);
        }
        if (iteration % 113u == 0u) {
            const auto& actor = records[random() % records.size()];
            registry.Add(actor); oracle.Add(actor);
        }
        Require(registry.LinkCount() == oracle.Links() && registry.BucketCount() == oracle.buckets.size(),
                "Registry accounting diverged after stale writes/native relinks/repeated Add");
    }
}
}

int main() {
    try {
        GeometryAndOrdering(); CachedMembership(); LargeQueriesAndRefusals();
        SnapshotPublicationAndRollback(); OracleComparison();
        std::cout << "PASS: " << checks << " checks, " << refusals
                  << " bounded refusals; actor-only cached hash registry/cylinder prerequisite. "
                     "No native Spawn/physics/floor/world-collision/device equivalence claim.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
