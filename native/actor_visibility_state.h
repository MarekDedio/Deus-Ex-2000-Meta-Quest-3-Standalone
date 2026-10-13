#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace QuestVr {

inline std::string ActorVisibilityKey(std::string path) {
    for (auto& c : path) if (c >= 'A' && c <= 'Z') c += 'a'-'A';
    return path;
}

// Scene-local removals outlive a spatial rebuild. An authoritative restoration
// clears this ledger, but does not mutate an older bank's independent mask.
class ActorRemovalState {
public:
    static constexpr std::size_t MaximumRemovals = 65536u;
    void Remove(const std::string& path) {
        const auto key = ActorVisibilityKey(path);
        if (key.empty()) throw std::runtime_error("Actor removal lacks identity");
        if (removed_.count(key)) return;
        if (removed_.size() >= MaximumRemovals)
            throw std::runtime_error("Actor removal identity budget exceeded");
        removed_.insert(key);
    }
    bool Removed(const std::string& path) const { return removed_.count(ActorVisibilityKey(path)) != 0u; }
    void Reset() { removed_.clear(); }
private:
    std::set<std::string> removed_;
};

// The SDK supports mat4 uniform arrays but not draw-index offsets. Thirty-two
// matrices provide one visibility scalar for each of the bounded 512 actors.
// This is CPU ownership/mapping evidence; it does not establish GPU acceptance.
class ActorVisibilityState {
public:
    static constexpr std::size_t MaximumActors = 512u;
    static constexpr std::size_t MatrixCount = MaximumActors/16u;
    ActorVisibilityState() { visible_.fill(1.0f); }
    std::size_t Add(const std::string& path, const ActorRemovalState& removals) {
        const auto key = ActorVisibilityKey(path);
        if (key.empty() || slots_.count(key) || slots_.size() >= MaximumActors)
            throw std::runtime_error("Invalid/bounded actor visibility identity");
        const auto slot = slots_.size();
        slots_.emplace(key,slot);
        visible_[slot] = removals.Removed(path) ? 0.0f : 1.0f;
        return slot;
    }
    std::optional<std::size_t> Slot(const std::string& path) const {
        const auto found = slots_.find(ActorVisibilityKey(path));
        return found == slots_.end() ? std::nullopt : std::optional<std::size_t>{found->second};
    }
    bool Hide(const std::string& path) {
        const auto slot = Slot(path);
        if (!slot) return false;
        visible_[*slot] = 0.0f;
        return true;
    }
    bool Visible(std::size_t slot) const { return slot < slots_.size() && visible_.at(slot) != 0.0f; }
    float Scalar(std::size_t slot) const { return visible_.at(slot); }
    static constexpr std::size_t Matrix(std::size_t slot) { return slot/16u; }
    static constexpr std::size_t Row(std::size_t slot) { return (slot%16u)/4u; }
    static constexpr std::size_t Column(std::size_t slot) { return slot%4u; }
private:
    std::array<float,MaximumActors> visible_;
    std::unordered_map<std::string,std::size_t> slots_;
};

enum class ActorRefreshKind { Spatial, Authoritative };

// Seed from a complete authoritative world snapshot, then retain every mover
// delta (including its final idle sample). Spatial rebase must not re-read all
// reflected mover data. This cache is confined to the runtime-owning thread.
template<class Snapshot>
class RetainedMoverPoseSnapshots {
public:
    static constexpr std::size_t MaximumMovers = 65536u;
    void Replace(const std::vector<Snapshot>& actors) {
        std::unordered_map<std::string,Snapshot> next;
        for (const auto& actor : actors) {
            if (!actor.mover) continue;
            const auto key = Key(actor);
            if (next.size() >= MaximumMovers || !next.emplace(key,actor).second)
                throw std::runtime_error("Invalid/bounded authoritative mover identities");
        }
        snapshots_.swap(next);
    }
    void Observe(const Snapshot& actor) {
        const auto key = Key(actor);
        const auto found = snapshots_.find(key);
        if (found != snapshots_.end()) { found->second = actor; return; }
        if (snapshots_.size() >= MaximumMovers)
            throw std::runtime_error("Retained mover identity budget exceeded");
        snapshots_.emplace(key,actor);
    }
    const Snapshot* Find(const std::string& path) const {
        const auto found = snapshots_.find(ActorVisibilityKey(path));
        return found == snapshots_.end() ? nullptr : &found->second;
    }
    const auto& Entries() const { return snapshots_; }
    void Clear() { snapshots_.clear(); }
private:
    static std::string Key(const Snapshot& actor) {
        if (!actor.mover || actor.objectPath.empty() || actor.objectPath.size() > 8192u ||
            actor.objectPath.find('\0') != std::string::npos)
            throw std::runtime_error("Retained mover pose lacks a valid mover identity");
        return ActorVisibilityKey(actor.objectPath);
    }
    std::unordered_map<std::string,Snapshot> snapshots_;
};

inline bool ActorRefreshBlocksGameplay(bool hasCompleteBank, bool hasRefresh, ActorRefreshKind kind) {
    return !hasCompleteBank || (hasRefresh && kind != ActorRefreshKind::Spatial);
}
inline bool RetainedActorCoverageContains(bool spatiallyFiltered, float centerX, float centerZ,
    float playerX, float playerZ, float radius = 20.0f) {
    if (!spatiallyFiltered) return true;
    const auto dx = playerX-centerX, dz = playerZ-centerZ;
    return std::isfinite(dx) && std::isfinite(dz) && dx*dx+dz*dz <= radius*radius;
}

} // namespace QuestVr
