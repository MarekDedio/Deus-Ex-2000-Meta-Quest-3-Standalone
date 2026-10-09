#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace QuestVr {

// Actor-only CollisionSystem/OverlapTester prerequisite, pinned SurrealEngine
// 677ee14c5b83486e6634687953779aafb7973ad6. This is not physics, floor finding,
// world collision, Spawn278 or Touch dispatch. Identity is opaque and nonzero.
struct ActorOverlapVec3 { float x{}, y{}, z{}; };
struct ActorOverlapRecord {
    std::uintptr_t identity{};
    ActorOverlapVec3 location;
    float radius{}, height{};
    bool brush{}, collideActors{};
};
struct ActorCollisionBox { ActorOverlapVec3 location, extents; };
struct ActorCollisionEntry {
    ActorOverlapRecord live;
    ActorCollisionBox registered;
    bool inserted{};
};
struct ActorOverlapLimits {
    std::size_t actors{65'536u}, buckets{262'144u}, links{1'048'576u};
    std::size_t registrationCells{262'144u}, mutationSteps{4'194'304u};
    std::size_t querySteps{4'194'304u};
};
struct ActorOverlapStats {
    std::size_t cells{}, bucketEntries{}, actorsTested{}, steps{};
    bool oversizedQuery{};
};
struct ActorOverlapResult {
    // First bucket-encounter order, deduplicated like the pin's CheckCounter.
    // OverlapTest then stable-sorts by UActor* address. The runtime caller must
    // perform that final sort on REAL actor pointers; these are opaque tokens.
    std::vector<std::uintptr_t> actors;
    ActorOverlapStats stats;
};

namespace ActorOverlapDetail {
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Actor collision registry: ") + reason);
}
inline void ValidateLimits(const ActorOverlapLimits& limits) {
    const ActorOverlapLimits maximum;
    if (!limits.actors || limits.actors > maximum.actors ||
        !limits.buckets || limits.buckets > maximum.buckets ||
        !limits.links || limits.links > maximum.links ||
        !limits.registrationCells || limits.registrationCells > maximum.registrationCells ||
        !limits.mutationSteps || limits.mutationSteps > maximum.mutationSteps ||
        !limits.querySteps || limits.querySteps > maximum.querySteps)
        Fail("invalid limits; only tightened fixed limits are supported");
}
inline void Step(std::size_t& used, const std::size_t maximum) {
    if (used >= maximum) Fail("work budget exhausted");
    ++used;
}
inline void Finite(const ActorOverlapVec3& value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
        Fail("nonfinite coordinates");
}
inline void Extents(const ActorOverlapVec3& value) {
    Finite(value);
    if (value.x < 0.0f || value.y < 0.0f || value.z < 0.0f)
        Fail("negative collision extents");
}
inline void Record(const ActorOverlapRecord& record) {
    if (!record.identity) Fail("null actor identity");
    // Live reflected bounds may be negative. In particular native283 must
    // retain them on an actor with collision disabled: AddToCollision returns
    // before computing any registration cells in the original engine.
    Finite(record.location); Finite({record.radius, record.radius, record.height});
}
struct Cells { std::int32_t start[3]{}, end[3]{}; };
inline std::int32_t Cell(float location, float extent, bool end) {
    // Preserve float subtraction/addition and float power-of-two scaling, then
    // floor. Never use truncation (wrong for negative coordinates) or double
    // endpoint arithmetic (different float boundary rounding).
    const float endpoint = end ? location + extent : location - extent;
    const float scaled = endpoint * (1.0f / 256.0f);
    if (!std::isfinite(endpoint) || !std::isfinite(scaled)) Fail("cell arithmetic overflow");
    const double floored = std::floor(static_cast<double>(scaled));
    const double minimum = std::numeric_limits<std::int32_t>::min();
    const double maximum = std::numeric_limits<std::int32_t>::max();
    if (floored < minimum || floored > maximum - (end ? 1.0 : 0.0))
        Fail("cell coordinate is outside signed int32 range");
    return static_cast<std::int32_t>(floored) + (end ? 1 : 0);
}
inline Cells CellRange(const ActorCollisionBox& box) {
    Finite(box.location); Extents(box.extents);
    Cells result;
    const float location[]{box.location.x, box.location.y, box.location.z};
    const float extent[]{box.extents.x, box.extents.y, box.extents.z};
    for (std::size_t axis = 0u; axis < 3u; ++axis) {
        result.start[axis] = Cell(location[axis], extent[axis], false);
        result.end[axis] = Cell(location[axis], extent[axis], true);
    }
    return result;
}
inline std::size_t CellCount(const Cells& range, std::size_t bound) {
    std::size_t result{1u};
    for (std::size_t axis = 0u; axis < 3u; ++axis) {
        const auto width = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(range.end[axis]) - range.start[axis]);
        if (!width || width > bound || result > bound / width)
            Fail("registration cell budget exceeded");
        result *= static_cast<std::size_t>(width);
    }
    return result;
}
inline std::uint32_t Bucket(std::int32_t x, std::int32_t y, std::int32_t z) {
    // Deliberately retain the pin's 1024-cell wrap/aliasing on every axis.
    return ((static_cast<std::uint32_t>(x) & 0x3ffu) << 20u) |
        ((static_cast<std::uint32_t>(y) & 0x3ffu) << 10u) |
        (static_cast<std::uint32_t>(z) & 0x3ffu);
}
inline bool Cylinder(const ActorOverlapVec3& location, float height, float radius,
                     const ActorOverlapRecord& actor) {
    if (actor.brush) return false; // CylinderActorOverlap ignores EVERY Brush.
    const double dx = static_cast<double>(location.x) - actor.location.x;
    const double dy = static_cast<double>(location.y) - actor.location.y;
    const double dz = static_cast<double>(location.z) - actor.location.z;
    const double summedHeight = static_cast<double>(height) + actor.height;
    const double summedRadius = static_cast<double>(radius) + actor.radius;
    return std::abs(dz) < summedHeight && dx * dx + dy * dy < summedRadius * summedRadius;
}
}

class ActorCollisionRegistry {
public:
    explicit ActorCollisionRegistry(const ActorOverlapLimits& limits = {}) : limits_(limits) {
        ActorOverlapDetail::ValidateLimits(limits_);
    }
    // Update reflected geometry WITHOUT repairing collision hash membership.
    // Property assignment in the pin does not call Remove/AddToCollision.
    void UpdateLive(const ActorOverlapRecord& record) {
        ActorOverlapDetail::Record(record);
        const auto found = actors_.find(record.identity);
        if (found != actors_.end()) found->second.live = record;
        else {
            if (actors_.size() >= limits_.actors) ActorOverlapDetail::Fail("actor count budget exceeded");
            actors_.emplace(record.identity, ActorCollisionEntry{record, {}, false});
        }
    }
    // A Mover with Brush uses its transformed brush bbox (+0.1 extents), NOT
    // its cylinder, for REGISTRATION. The native caller must supply that exact
    // box; this helper does not decode or guess brush transforms. Ordinary
    // actors and Movers without Brush use the cylinder box by default.
    // Repeated Add deliberately retains old/duplicate memberships, as the pin
    // does. Remove only uses the LAST cached registration box, removing every
    // occurrence from those buckets; stale older buckets can therefore remain.
    ActorOverlapStats Add(const ActorOverlapRecord& record,
                          const std::optional<ActorCollisionBox>& registration = {}) {
        ActorOverlapDetail::Record(record);
        ActorOverlapStats stats;
        if (!record.collideActors) { UpdateLive(record); return stats; }
        const auto box = registration.value_or(ActorCollisionBox{
            record.location, {record.radius, record.radius, record.height}});
        const auto range = ActorOverlapDetail::CellRange(box);
        const auto count = ActorOverlapDetail::CellCount(range, limits_.registrationCells);
        if (links_ > limits_.links || count > limits_.links - links_)
            ActorOverlapDetail::Fail("total bucket-link budget exceeded");
        if (!actors_.count(record.identity) && actors_.size() >= limits_.actors)
            ActorOverlapDetail::Fail("actor count budget exceeded");

        // Prepare changed bucket vectors first. Allocation or budget refusal
        // leaves logical registry state unchanged, including cached geometry.
        std::unordered_map<std::uint32_t, std::vector<std::uintptr_t>> prepared;
        for (std::int32_t z = range.start[2]; z < range.end[2]; ++z)
            for (std::int32_t y = range.start[1]; y < range.end[1]; ++y)
                for (std::int32_t x = range.start[0]; x < range.end[0]; ++x) {
                    ActorOverlapDetail::Step(stats.steps, limits_.mutationSteps); ++stats.cells;
                    const auto key = ActorOverlapDetail::Bucket(x, y, z);
                    auto pending = prepared.find(key);
                    if (pending == prepared.end()) {
                        const auto original = buckets_.find(key);
                        std::vector<std::uintptr_t> values;
                        if (original != buckets_.end()) {
                            for (const auto identity : original->second) {
                                ActorOverlapDetail::Step(stats.steps, limits_.mutationSteps);
                                ++stats.bucketEntries; values.push_back(identity);
                            }
                        }
                        pending = prepared.emplace(key, std::move(values)).first;
                    }
                    pending->second.push_back(record.identity);
                }
        std::size_t missing{};
        for (const auto& item : prepared) if (!buckets_.count(item.first)) ++missing;
        if (buckets_.size() > limits_.buckets || missing > limits_.buckets - buckets_.size())
            ActorOverlapDetail::Fail("bucket count budget exceeded");
        std::vector<std::uint32_t> inserted;
        inserted.reserve(missing);
        buckets_.reserve(buckets_.size() + missing);
        try {
            for (const auto& item : prepared)
                if (buckets_.emplace(item.first, std::vector<std::uintptr_t>{}).second)
                    inserted.push_back(item.first);
            auto found = actors_.find(record.identity);
            if (found == actors_.end())
                found = actors_.emplace(record.identity, ActorCollisionEntry{}).first;
            // All remaining operations are non-allocating/non-throwing.
            for (auto& item : prepared) buckets_.at(item.first).swap(item.second);
            found->second = ActorCollisionEntry{record, box, true};
            links_ += count;
        } catch (...) {
            for (const auto key : inserted) buckets_.erase(key);
            throw;
        }
        return stats;
    }
    ActorOverlapStats Remove(std::uintptr_t identity) {
        if (!identity) ActorOverlapDetail::Fail("null actor identity");
        ActorOverlapStats stats;
        auto found = actors_.find(identity);
        if (found == actors_.end() || !found->second.inserted) return stats;
        const auto range = ActorOverlapDetail::CellRange(found->second.registered);
        ActorOverlapDetail::CellCount(range, limits_.registrationCells);
        // Preflight all scans before mutating. Aliased cells repeat work just as
        // in the pin, even though removal from the first occurrence empties it.
        for (std::int32_t z = range.start[2]; z < range.end[2]; ++z)
            for (std::int32_t y = range.start[1]; y < range.end[1]; ++y)
                for (std::int32_t x = range.start[0]; x < range.end[0]; ++x) {
                    ActorOverlapDetail::Step(stats.steps, limits_.mutationSteps); ++stats.cells;
                    const auto bucket = buckets_.find(ActorOverlapDetail::Bucket(x, y, z));
                    if (bucket != buckets_.end()) for (const auto ignored : bucket->second) {
                        static_cast<void>(ignored);
                        ActorOverlapDetail::Step(stats.steps, limits_.mutationSteps); ++stats.bucketEntries;
                    }
                }
        for (std::int32_t z = range.start[2]; z < range.end[2]; ++z)
            for (std::int32_t y = range.start[1]; y < range.end[1]; ++y)
                for (std::int32_t x = range.start[0]; x < range.end[0]; ++x) {
                    auto bucket = buckets_.find(ActorOverlapDetail::Bucket(x, y, z));
                    if (bucket == buckets_.end()) continue;
                    auto& values = bucket->second;
                    const auto before = values.size();
                    values.erase(std::remove(values.begin(), values.end(), identity), values.end());
                    links_ -= before - values.size();
                    if (values.empty()) buckets_.erase(bucket);
                }
        found->second.inserted = false;
        return stats;
    }
    ActorOverlapResult Query(const ActorOverlapVec3& location, float height,
                             float radius, bool visibilityOnly = false) const {
        ActorOverlapDetail::Finite(location);
        ActorOverlapDetail::Extents({radius, radius, height});
        ActorOverlapResult result;
        if (visibilityOnly) return result;
        const auto range = ActorOverlapDetail::CellRange({location, {radius, radius, height}});
        for (std::size_t axis = 0u; axis < 3u; ++axis)
            if (static_cast<std::int64_t>(range.end[axis]) - range.start[axis] >= 100) {
                result.stats.oversizedQuery = true;
                return result; // Pinned actor-overlap guard, NOT budget refusal.
            }
        std::unordered_set<std::uintptr_t> seen;
        for (std::int32_t z = range.start[2]; z < range.end[2]; ++z)
            for (std::int32_t y = range.start[1]; y < range.end[1]; ++y)
                for (std::int32_t x = range.start[0]; x < range.end[0]; ++x) {
                    ActorOverlapDetail::Step(result.stats.steps, limits_.querySteps); ++result.stats.cells;
                    const auto bucket = buckets_.find(ActorOverlapDetail::Bucket(x, y, z));
                    if (bucket == buckets_.end()) continue;
                    for (const auto identity : bucket->second) {
                        ActorOverlapDetail::Step(result.stats.steps, limits_.querySteps);
                        ++result.stats.bucketEntries;
                        if (!seen.insert(identity).second) continue;
                        const auto actor = actors_.find(identity);
                        if (actor == actors_.end()) ActorOverlapDetail::Fail("bucket actor is unavailable");
                        ++result.stats.actorsTested;
                        // bCollideActors/Inserted are NOT reread by the pinned
                        // narrow phase. Plain reflected flag changes stay stale.
                        if (ActorOverlapDetail::Cylinder(location, height, radius, actor->second.live))
                            result.actors.push_back(identity);
                    }
                }
        return result;
    }
    const ActorCollisionEntry* Find(std::uintptr_t identity) const {
        const auto found = actors_.find(identity);
        return found == actors_.end() ? nullptr : &found->second;
    }
    std::size_t ActorCount() const { return actors_.size(); }
    std::size_t BucketCount() const { return buckets_.size(); }
    std::size_t LinkCount() const { return links_; }
    void Clear() noexcept { buckets_.clear(); actors_.clear(); links_ = 0u; }
    // VM rollback and prepared save publication must never allocate or fail.
    // Swap the complete snapshot, including limits and stale/duplicate bucket
    // memberships; rebuilding from live geometry would change the pin's hash.
    void Swap(ActorCollisionRegistry& other) noexcept {
        using std::swap;
        swap(limits_, other.limits_);
        actors_.swap(other.actors_);
        buckets_.swap(other.buckets_);
        swap(links_, other.links_);
    }

private:
    ActorOverlapLimits limits_;
    std::unordered_map<std::uintptr_t, ActorCollisionEntry> actors_;
    std::unordered_map<std::uint32_t, std::vector<std::uintptr_t>> buckets_;
    std::size_t links_{};
};

}
