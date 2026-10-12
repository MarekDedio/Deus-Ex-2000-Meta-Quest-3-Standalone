#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace QuestVr {

// The surface-owned FDecal/Self.SurfList portion of the original GOTY native
// contract, docs/DECAL-NATIVE-CONTRACT.md. Geometry supplied to AttachPrimary
// must already be projected and relative to the selected authored surface base;
// Begin/Complete also represent the original callback-visible provisional path.
// This is NOT AttachDecal itself, a reflected SurfList, a renderer, disk
// serialization or GC ownership.
struct DecalStateVec3 { float x{}, y{}, z{}; };
struct DecalStateRotation { std::int32_t pitch{}, yaw{}, roll{}; };
struct DecalSurfaceRecord {
    std::array<DecalStateVec3, 4> relativeCorners{};
    std::string ownerIdentity;
    // Empty means no node restriction, NOT an invisible record. Preserve the
    // caller's authored order and duplicates; this helper does not clip quads.
    std::vector<std::int32_t> nodeMembership;
    DecalStateRotation moverRotation;
};
struct DecalOwnerSurfList {
    std::string ownerIdentity;
    std::vector<std::int32_t> surfaces;
};
struct DecalStateSnapshot {
    std::vector<std::vector<DecalSurfaceRecord>> surfaces;
    std::vector<DecalOwnerSurfList> owners;
};
struct DecalStateLimits {
    std::size_t surfaces{1'000'000u}, nodes{1'000'000u};
    std::size_t owners{65'536u}, records{262'144u};
    std::size_t nodeMembership{1'048'576u}, surfListEntries{262'144u};
    std::size_t identityBytes{1'024u};
    std::size_t storageBytes{64u * 1024u * 1024u};
    std::size_t stagingBytes{128u * 1024u * 1024u};
    std::size_t operationSteps{16u * 1024u * 1024u};
};
struct DecalStateUsage {
    std::size_t surfaces{}, owners{}, records{}, nodeMembership{}, surfListEntries{};
    // Stable identity payloads include one terminator per stored identity.
    // Deterministic structural/payload charge, not allocator capacity or RSS.
    // The fixed element limits also bound retained vector capacity independently.
    std::size_t identityBytes{}, storageBytes{};
};
struct DecalStateStats {
    std::size_t steps{}, resolverCalls{}, recordsInserted{}, recordsRemoved{};
    std::size_t surfListEntriesPopped{}, stagedBytes{};
};

namespace DecalStateDetail {
// Shared only by a live state, its journal copies and ephemeral tokens. A new
// checkpoint import gets a distinct domain; keeping the old domain alive in a
// token prevents an allocator address reuse from accepting a stale token.
struct PrimaryTokenDomain {};
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Native decal state: ") + reason);
}
inline std::size_t Add(std::size_t a, std::size_t b) {
    if (b > std::numeric_limits<std::size_t>::max() - a) Fail("size arithmetic overflow");
    return a + b;
}
inline std::size_t Multiply(std::size_t a, std::size_t b) {
    if (a && b > std::numeric_limits<std::size_t>::max() / a) Fail("size arithmetic overflow");
    return a * b;
}
inline std::uint64_t IncrementSerial(std::uint64_t serial) {
    if (serial == std::numeric_limits<std::uint64_t>::max())
        Fail("primary record token serial exhausted");
    return serial + 1u;
}
inline void Step(DecalStateStats& stats, std::size_t count, const DecalStateLimits& limits) {
    if (stats.steps > limits.operationSteps || count > limits.operationSteps - stats.steps)
        Fail("work budget exhausted");
    stats.steps += count;
}
inline void ValidateLimits(const DecalStateLimits& limits) {
    const DecalStateLimits maximum;
    if (!limits.surfaces || limits.surfaces > maximum.surfaces ||
        !limits.nodes || limits.nodes > maximum.nodes ||
        !limits.owners || limits.owners > maximum.owners ||
        !limits.records || limits.records > maximum.records ||
        !limits.nodeMembership || limits.nodeMembership > maximum.nodeMembership ||
        !limits.surfListEntries || limits.surfListEntries > maximum.surfListEntries ||
        !limits.identityBytes || limits.identityBytes > maximum.identityBytes ||
        !limits.storageBytes || limits.storageBytes > maximum.storageBytes ||
        !limits.stagingBytes || limits.stagingBytes > maximum.stagingBytes ||
        !limits.operationSteps || limits.operationSteps > maximum.operationSteps)
        Fail("invalid limits; callers may only tighten fixed bounds");
}
inline void Identity(const std::string& identity, DecalStateStats& stats,
                     const DecalStateLimits& limits) {
    if (identity.empty() || identity.size() > limits.identityBytes)
        Fail("null or oversized identity");
    Step(stats, identity.size(), limits);
    if (identity.find('\0') != std::string::npos) Fail("identity contains NUL");
    // Opaque exact byte identity: no case folding, pointer reuse or locale
    // conversion. The host is responsible for stable lifetime/GC identity.
}
inline void ValidateUsage(const DecalStateUsage& usage, const DecalStateLimits& limits) {
    if (usage.surfaces > limits.surfaces || usage.owners > limits.owners ||
        usage.records > limits.records || usage.nodeMembership > limits.nodeMembership ||
        usage.surfListEntries > limits.surfListEntries || usage.storageBytes > limits.storageBytes)
        Fail("storage budget exceeded");
}
inline void AddIdentity(DecalStateUsage& usage, const std::string& identity) {
    const auto bytes = Add(identity.size(), 1u);
    usage.identityBytes = Add(usage.identityBytes, bytes);
    usage.storageBytes = Add(usage.storageBytes, bytes);
}
} // namespace DecalStateDetail

class DecalPrimaryToken {
public:
    DecalPrimaryToken() = default;
    std::int32_t SurfaceIndex() const noexcept { return surfaceIndex_; }
    std::size_t RecordIndex() const noexcept { return recordIndex_; }
private:
    friend class DecalState;
    std::shared_ptr<const DecalStateDetail::PrimaryTokenDomain> domain_;
    std::string ownerIdentity_;
    std::int32_t surfaceIndex_{};
    std::size_t recordIndex_{};
    std::uint64_t surfaceSerial_{};
};
struct DecalPrimaryBegin {
    DecalPrimaryToken token;
    DecalStateStats stats;
};

class DecalState {
public:
    // None is a valid live Texture on an EXISTING owner, and never matches the
    // required non-null receiverTexture. Resolve every visited record afresh;
    // no live texture is cached in this state. Lookup work/allocations performed
    // inside the callback belong to the host's separately bounded operation.
    using LiveTextureResolver = std::function<std::optional<std::string>(const std::string&)>;

    explicit DecalState(std::size_t surfaceCount, std::size_t nodeCount,
                        const DecalStateLimits& limits = {})
        : limits_(limits), nodeCount_(nodeCount) {
        DecalStateDetail::ValidateLimits(limits_);
        if (surfaceCount > limits_.surfaces || nodeCount > limits_.nodes)
            DecalStateDetail::Fail("model count budget exceeded");
        usage_.surfaces = surfaceCount;
        usage_.storageBytes = InitialStorageBytes(surfaceCount);
        DecalStateDetail::ValidateUsage(usage_, limits_);
        state_.surfaces.resize(surfaceCount);
        surfaceSerials_.resize(surfaceCount);
        tokenDomain_ = std::make_shared<const DecalStateDetail::PrimaryTokenDomain>();
    }
    DecalState(const DecalState& other)
        : limits_(other.limits_), nodeCount_(other.nodeCount_), state_(other.state_), usage_(other.usage_),
          surfaceSerials_(other.surfaceSerials_), tokenDomain_(other.tokenDomain_) {}
    DecalState& operator=(const DecalState& other) {
        if (this == &other) return *this;
        CheckIdle();
        DecalState prepared(other);
        static_cast<void>(Swap(prepared));
        return *this;
    }

    DecalStateStats AttachPrimary(std::int32_t surfaceIndex, const DecalSurfaceRecord& record,
                                 const std::string& receiverTexture,
                                 const LiveTextureResolver& resolveLiveTexture) {
        MutationGuard guard(*this);
        return InsertPrimary(surfaceIndex, record.ownerIdentity, &record, record.moverRotation,
            receiverTexture, resolveLiveTexture, false).stats;
    }

    // The original primary path publishes an owner-bearing, zero-corner FDecal
    // and appends SurfList BEFORE SetBase can run script callbacks. The host must
    // leave this mutation before invoking those callbacks, then complete using
    // the token. Tokens are transient, not snapshot or wire data.
    DecalPrimaryBegin BeginPrimary(std::int32_t surfaceIndex, const std::string& ownerIdentity,
                                  const std::string& receiverTexture,
                                  const LiveTextureResolver& resolveLiveTexture,
                                  DecalStateRotation moverRotation = {}) {
        MutationGuard guard(*this);
        return InsertPrimary(surfaceIndex, ownerIdentity, nullptr, moverRotation,
            receiverTexture, resolveLiveTexture, true);
    }

    // A same-surface insertion/removal can invalidate the original raw FDecal
    // pointer: refuse explicitly instead of accidentally updating a replacement
    // record at the old index. Foreign-surface edits and owner-list-only changes
    // do not invalidate it. Update only corners/membership, preserving the
    // BeginPrimary owner, mover rotation, insertion order and current SurfList.
    DecalStateStats CompletePrimary(const DecalPrimaryToken& token,
                                    const DecalSurfaceRecord& record) {
        MutationGuard guard(*this);
        DecalStateStats stats;
        SurfaceIndex(token.surfaceIndex_, stats);
        const auto surface = static_cast<std::size_t>(token.surfaceIndex_);
        if (!token.domain_ || token.domain_ != tokenDomain_ ||
            token.surfaceSerial_ != surfaceSerials_[surface] ||
            token.recordIndex_ >= state_.surfaces[surface].size())
            DecalStateDetail::Fail("invalidated primary record token");
        const auto& previous = state_.surfaces[surface][token.recordIndex_];
        DecalStateDetail::Step(stats, DecalStateDetail::Add(1u,
            DecalStateDetail::Add(previous.ownerIdentity.size(), token.ownerIdentity_.size())), limits_);
        if (previous.ownerIdentity != token.ownerIdentity_ || record.ownerIdentity != previous.ownerIdentity)
            DecalStateDetail::Fail("primary record token owner mismatch");
        Record(record, stats);
        auto projected = usage_;
        projected.nodeMembership -= previous.nodeMembership.size();
        projected.storageBytes -= previous.nodeMembership.size() * sizeof(std::int32_t);
        projected.nodeMembership = DecalStateDetail::Add(projected.nodeMembership, record.nodeMembership.size());
        projected.storageBytes = DecalStateDetail::Add(projected.storageBytes,
            DecalStateDetail::Multiply(record.nodeMembership.size(), sizeof(std::int32_t)));
        Prepare(projected, stats, RecordPayloadBytes(record));
        auto prepared = state_;
        auto& completed = prepared.surfaces[surface][token.recordIndex_];
        completed.relativeCorners = record.relativeCorners;
        completed.nodeMembership = record.nodeMembership;
        Publish(prepared, projected);
        return stats;
    }

    DecalStateStats Detach(const std::string& ownerIdentity) {
        MutationGuard guard(*this);
        DecalStateStats stats;
        DecalStateDetail::Identity(ownerIdentity, stats, limits_);
        const auto ownerIndex = FindOwner(ownerIdentity, stats);
        if (ownerIndex == state_.owners.size()) return stats;
        Prepare(usage_, stats);
        auto prepared = state_;
        auto preparedSerials = surfaceSerials_;
        auto projected = usage_;
        auto& surfList = prepared.owners[ownerIndex].surfaces;
        while (!surfList.empty()) {
            DecalStateDetail::Step(stats, 1u, limits_);
            const auto surface = static_cast<std::size_t>(surfList.back());
            auto& records = prepared.surfaces[surface];
            for (std::size_t index = 0u; index < records.size(); ++index) {
                DecalStateDetail::Step(stats,
                    DecalStateDetail::Add(1u, DecalStateDetail::Add(records[index].ownerIdentity.size(),
                        ownerIdentity.size())), limits_);
                if (records[index].ownerIdentity != ownerIdentity) continue;
                preparedSerials[surface] = NextSerial(preparedSerials[surface], stats);
                DecalStateDetail::Step(stats, records.size() - index, limits_);
                RemoveRecordUsage(projected, records[index]);
                records.erase(records.begin() + static_cast<std::ptrdiff_t>(index));
                ++stats.recordsRemoved;
                break; // FIRST matching owner, not the last inserted record.
            }
            surfList.pop_back(); // Pop even if surface has no matching record.
            --projected.surfListEntries;
            projected.storageBytes -= sizeof(std::int32_t);
            ++stats.surfListEntriesPopped;
        }
        Publish(prepared, projected);
        surfaceSerials_.swap(preparedSerials);
        return stats;
    }

    // Explicit mirror synchronization only. Native records are intentionally
    // untouched: reflected lists can refer to missing/foreign records. This
    // operation does not reconstruct geometry from disk or own reflected fields.
    DecalStateStats ReplaceOwnerSurfList(const std::string& ownerIdentity,
                                        const std::vector<std::int32_t>& surfaces) {
        MutationGuard guard(*this);
        DecalStateStats stats;
        DecalStateDetail::Identity(ownerIdentity, stats, limits_);
        if (surfaces.size() > limits_.surfListEntries)
            DecalStateDetail::Fail("owner surface-list budget exceeded");
        for (const auto surface : surfaces) SurfaceIndex(surface, stats);
        const auto ownerIndex = FindOwner(ownerIdentity, stats);
        auto projected = usage_;
        if (ownerIndex == state_.owners.size()) AddOwnerUsage(projected, ownerIdentity);
        else {
            const auto old = state_.owners[ownerIndex].surfaces.size();
            projected.surfListEntries -= old;
            projected.storageBytes -= old * sizeof(std::int32_t);
        }
        projected.surfListEntries = DecalStateDetail::Add(projected.surfListEntries, surfaces.size());
        projected.storageBytes = DecalStateDetail::Add(projected.storageBytes,
            DecalStateDetail::Multiply(surfaces.size(), sizeof(std::int32_t)));
        Prepare(projected, stats);
        auto prepared = state_;
        if (ownerIndex == prepared.owners.size()) {
            prepared.owners.reserve(DecalStateDetail::Add(prepared.owners.size(), 1u));
            prepared.owners.push_back({ownerIdentity, {}});
        }
        prepared.owners[ownerIndex].surfaces = surfaces;
        Publish(prepared, projected);
        return stats;
    }

    // Structural checkpoint import only. Preserve exact per-surface record and
    // mirror order, including duplicates and legitimate missing/foreign links.
    // Do NOT invoke the live-Texture insertion path or reconstruct geometry.
    // The runtime must validate symbolic identities against its actual map and
    // stage the corresponding GC roots before publishing this native state.
    DecalStateStats ImportSnapshot(const DecalStateSnapshot& snapshot) {
        MutationGuard guard(*this);
        DecalStateStats stats;
        if (snapshot.surfaces.size() != state_.surfaces.size())
            DecalStateDetail::Fail("checkpoint model surface count mismatch");
        if (snapshot.owners.size() > limits_.owners)
            DecalStateDetail::Fail("checkpoint owner count budget exceeded");
        DecalStateUsage projected;
        projected.surfaces = snapshot.surfaces.size();
        projected.storageBytes = InitialStorageBytes(projected.surfaces);
        for (const auto& surface : snapshot.surfaces) {
            DecalStateDetail::Step(stats, 1u, limits_);
            if (surface.size() > limits_.records - projected.records)
                DecalStateDetail::Fail("checkpoint record count budget exceeded");
            for (const auto& record : surface) {
                Record(record, stats); AddRecordUsage(projected, record);
                DecalStateDetail::ValidateUsage(projected, limits_);
            }
        }
        for (const auto& owner : snapshot.owners) {
            DecalStateDetail::Identity(owner.ownerIdentity, stats, limits_);
            AddOwnerUsage(projected, owner.ownerIdentity);
            if (owner.surfaces.size() > limits_.surfListEntries - projected.surfListEntries)
                DecalStateDetail::Fail("checkpoint surface-list count budget exceeded");
            for (const auto surface : owner.surfaces) SurfaceIndex(surface, stats);
            projected.surfListEntries = DecalStateDetail::Add(projected.surfListEntries, owner.surfaces.size());
            projected.storageBytes = DecalStateDetail::Add(projected.storageBytes,
                DecalStateDetail::Multiply(owner.surfaces.size(), sizeof(std::int32_t)));
            DecalStateDetail::ValidateUsage(projected, limits_);
        }
        Prepare(projected, stats);
        const auto identityPointers = DecalStateDetail::Multiply(snapshot.owners.size(), sizeof(const std::string*));
        stats.stagedBytes = DecalStateDetail::Add(stats.stagedBytes, identityPointers);
        if (stats.stagedBytes > limits_.stagingBytes)
            DecalStateDetail::Fail("checkpoint staging byte budget exceeded");
        std::vector<const std::string*> identities;
        identities.reserve(snapshot.owners.size());
        for (const auto& owner : snapshot.owners) identities.push_back(&owner.ownerIdentity);
        std::sort(identities.begin(), identities.end(), [&](const auto* a, const auto* b) {
            DecalStateDetail::Step(stats, DecalStateDetail::Add(1u,
                DecalStateDetail::Add(a->size(), b->size())), limits_);
            return *a < *b; // Opaque exact identity, no case folding.
        });
        for (std::size_t i = 1u; i < identities.size(); ++i) {
            DecalStateDetail::Step(stats, DecalStateDetail::Add(1u,
                DecalStateDetail::Add(identities[i-1u]->size(), identities[i]->size())), limits_);
            if (*identities[i-1u] == *identities[i])
                DecalStateDetail::Fail("duplicate checkpoint owner mirror identity");
        }
        auto prepared = snapshot;
        std::vector<std::uint64_t> preparedSerials(snapshot.surfaces.size());
        auto preparedDomain = std::make_shared<const DecalStateDetail::PrimaryTokenDomain>();
        Publish(prepared, projected);
        surfaceSerials_.swap(preparedSerials);
        tokenDomain_.swap(preparedDomain);
        return stats;
    }

    const DecalStateSnapshot& ReadOnlySnapshot() const noexcept { return state_; }
    const DecalStateUsage& Usage() const noexcept { return usage_; }
    const DecalStateLimits& Limits() const noexcept { return limits_; }
    std::size_t NodeCount() const noexcept { return nodeCount_; }

    // The snapshot/wire deliberately excludes the transient token sidecar. The
    // runtime must charge it separately when measuring a live/journal state or
    // staging a checkpoint. This reports actual vector capacity without copying;
    // shared_ptr allocator/control-block metadata is outside the structural
    // payload budget just like other allocator metadata.
    static std::size_t TransientStorageBytes(std::size_t surfaceCount) {
        return DecalStateDetail::Add(sizeof(std::vector<std::uint64_t>) +
            sizeof(std::shared_ptr<const DecalStateDetail::PrimaryTokenDomain>) +
            sizeof(DecalStateDetail::PrimaryTokenDomain),
            DecalStateDetail::Multiply(surfaceCount, sizeof(std::uint64_t)));
    }
    std::size_t TransientRetainedBytes() const {
        return TransientStorageBytes(surfaceSerials_.capacity());
    }

    // Whole-container journal rollback, including duplicate records, owner
    // lists, geometry and limits. No allocation. False only refuses callback
    // reentrancy; normal host rollback outside a mutation always succeeds.
    // The host must journal its actual object roots/reflected fields separately.
    bool Swap(DecalState& other) noexcept {
        if (mutationActive_ || other.mutationActive_) return false;
        using std::swap;
        swap(limits_, other.limits_);
        swap(nodeCount_, other.nodeCount_);
        state_.surfaces.swap(other.state_.surfaces);
        state_.owners.swap(other.state_.owners);
        surfaceSerials_.swap(other.surfaceSerials_);
        tokenDomain_.swap(other.tokenDomain_);
        swap(usage_, other.usage_);
        return true;
    }

private:
    struct MutationGuard {
        DecalState& owner;
        explicit MutationGuard(DecalState& value) : owner(value) {
            owner.CheckIdle(); owner.mutationActive_ = true;
        }
        ~MutationGuard() { owner.mutationActive_ = false; }
        MutationGuard(const MutationGuard&) = delete;
        MutationGuard& operator=(const MutationGuard&) = delete;
    };
    void CheckIdle() const {
        if (mutationActive_) DecalStateDetail::Fail("reentrant mutation from resolver");
    }
    void SurfaceIndex(std::int32_t index, DecalStateStats& stats) const {
        DecalStateDetail::Step(stats, 1u, limits_);
        if (index < 0 || static_cast<std::size_t>(index) >= state_.surfaces.size())
            DecalStateDetail::Fail("surface index out of bounds");
    }
    void Record(const DecalSurfaceRecord& record, DecalStateStats& stats) const {
        DecalStateDetail::Identity(record.ownerIdentity, stats, limits_);
        for (const auto& corner : record.relativeCorners) {
            DecalStateDetail::Step(stats, 1u, limits_);
            if (!std::isfinite(corner.x) || !std::isfinite(corner.y) || !std::isfinite(corner.z))
                DecalStateDetail::Fail("nonfinite relative corner");
        }
        if (record.nodeMembership.size() > limits_.nodeMembership)
            DecalStateDetail::Fail("record node-membership budget exceeded");
        for (const auto node : record.nodeMembership) {
            DecalStateDetail::Step(stats, 1u, limits_);
            if (node < 0 || static_cast<std::size_t>(node) >= nodeCount_)
                DecalStateDetail::Fail("node membership out of bounds");
        }
    }
    std::size_t FindOwner(const std::string& identity, DecalStateStats& stats) const {
        for (std::size_t index = 0u; index < state_.owners.size(); ++index) {
            DecalStateDetail::Step(stats, DecalStateDetail::Add(1u,
                DecalStateDetail::Add(state_.owners[index].ownerIdentity.size(), identity.size())), limits_);
            if (state_.owners[index].ownerIdentity == identity) return index;
        }
        return state_.owners.size();
    }
    static std::size_t InitialStorageBytes(std::size_t surfaceCount) {
        auto bytes = DecalStateDetail::Add(sizeof(DecalStateSnapshot),
            DecalStateDetail::Multiply(surfaceCount, sizeof(std::vector<DecalSurfaceRecord>)));
        return DecalStateDetail::Add(bytes, TransientStorageBytes(surfaceCount));
    }
    std::uint64_t NextSerial(std::uint64_t serial, DecalStateStats& stats) const {
        DecalStateDetail::Step(stats, 1u, limits_);
        return DecalStateDetail::IncrementSerial(serial);
    }
    DecalPrimaryBegin InsertPrimary(std::int32_t surfaceIndex, const std::string& ownerIdentity,
                                   const DecalSurfaceRecord* record,
                                   DecalStateRotation moverRotation,
                                   const std::string& receiverTexture,
                                   const LiveTextureResolver& resolveLiveTexture, bool makeToken) {
        DecalPrimaryBegin result;
        auto& stats = result.stats;
        SurfaceIndex(surfaceIndex, stats);
        if (record) Record(*record, stats);
        else {
            DecalStateDetail::Identity(ownerIdentity, stats, limits_);
            DecalStateDetail::Step(stats, 4u, limits_); // Four finite zero corners.
        }
        DecalStateDetail::Identity(receiverTexture, stats, limits_);
        if (!resolveLiveTexture) DecalStateDetail::Fail("missing live texture resolver");
        const auto surface = static_cast<std::size_t>(surfaceIndex);
        const auto serial = NextSerial(surfaceSerials_[surface], stats);
        const auto ownerIndex = FindOwner(ownerIdentity, stats);
        auto projected = usage_;
        AddRecordUsage(projected, ownerIdentity, record ? record->nodeMembership.size() : 0u);
        if (ownerIndex == state_.owners.size()) AddOwnerUsage(projected, ownerIdentity);
        projected.surfListEntries = DecalStateDetail::Add(projected.surfListEntries, 1u);
        projected.storageBytes = DecalStateDetail::Add(projected.storageBytes, sizeof(std::int32_t));
        auto incomingBytes = DecalStateDetail::Add(sizeof(DecalSurfaceRecord),
            DecalStateDetail::Add(ownerIdentity.size(), 1u));
        if (record) incomingBytes = RecordPayloadBytes(*record);
        if (makeToken) incomingBytes = DecalStateDetail::Add(incomingBytes,
            DecalStateDetail::Add(sizeof(DecalPrimaryToken), DecalStateDetail::Add(ownerIdentity.size(), 1u)));
        Prepare(projected, stats, incomingBytes);

        std::size_t insertion{};
        for (const auto& existing : state_.surfaces[surface]) {
            DecalStateDetail::Step(stats, 1u, limits_);
            ++stats.resolverCalls;
            const auto texture = resolveLiveTexture(existing.ownerIdentity);
            if (texture) {
                DecalStateDetail::Identity(*texture, stats, limits_);
                DecalStateDetail::Step(stats,
                    DecalStateDetail::Add(texture->size(), receiverTexture.size()), limits_);
                if (*texture == receiverTexture) break;
            }
            ++insertion;
        }
        // Precharge all shifts/copies before allocating. Publication of the
        // snapshot and serial is nonthrowing, including token construction.
        DecalStateDetail::Step(stats, state_.surfaces[surface].size() - insertion + 1u, limits_);
        auto prepared = state_;
        auto& records = prepared.surfaces[surface];
        records.reserve(DecalStateDetail::Add(records.size(), 1u));
        if (record) records.insert(records.begin() + static_cast<std::ptrdiff_t>(insertion), *record);
        else {
            DecalSurfaceRecord provisional;
            provisional.ownerIdentity = ownerIdentity;
            provisional.moverRotation = moverRotation;
            records.insert(records.begin() + static_cast<std::ptrdiff_t>(insertion), std::move(provisional));
        }
        if (ownerIndex == prepared.owners.size()) {
            prepared.owners.reserve(DecalStateDetail::Add(prepared.owners.size(), 1u));
            prepared.owners.push_back({ownerIdentity, {}});
        }
        auto& surfList = prepared.owners[ownerIndex].surfaces;
        surfList.reserve(DecalStateDetail::Add(surfList.size(), 1u));
        surfList.push_back(surfaceIndex); // Preserve primary-path duplicates.
        if (makeToken) {
            result.token.domain_ = tokenDomain_;
            result.token.ownerIdentity_ = ownerIdentity;
            result.token.surfaceIndex_ = surfaceIndex;
            result.token.recordIndex_ = insertion;
            result.token.surfaceSerial_ = serial;
        }
        Publish(prepared, projected);
        surfaceSerials_[surface] = serial;
        stats.recordsInserted = 1u;
        return result;
    }
    static void AddOwnerUsage(DecalStateUsage& usage, const std::string& identity) {
        usage.owners = DecalStateDetail::Add(usage.owners, 1u);
        usage.storageBytes = DecalStateDetail::Add(usage.storageBytes, sizeof(DecalOwnerSurfList));
        DecalStateDetail::AddIdentity(usage, identity);
    }
    static void AddRecordUsage(DecalStateUsage& usage, const DecalSurfaceRecord& record) {
        AddRecordUsage(usage, record.ownerIdentity, record.nodeMembership.size());
    }
    static void AddRecordUsage(DecalStateUsage& usage, const std::string& ownerIdentity,
                               std::size_t nodeMembership) {
        usage.records = DecalStateDetail::Add(usage.records, 1u);
        usage.nodeMembership = DecalStateDetail::Add(usage.nodeMembership, nodeMembership);
        usage.storageBytes = DecalStateDetail::Add(usage.storageBytes, sizeof(DecalSurfaceRecord));
        usage.storageBytes = DecalStateDetail::Add(usage.storageBytes,
            DecalStateDetail::Multiply(nodeMembership, sizeof(std::int32_t)));
        DecalStateDetail::AddIdentity(usage, ownerIdentity);
    }
    static void RemoveRecordUsage(DecalStateUsage& usage, const DecalSurfaceRecord& record) noexcept {
        --usage.records;
        usage.nodeMembership -= record.nodeMembership.size();
        const auto identityBytes = record.ownerIdentity.size() + 1u;
        usage.identityBytes -= identityBytes;
        usage.storageBytes -= sizeof(DecalSurfaceRecord) +
            record.nodeMembership.size() * sizeof(std::int32_t) + identityBytes;
    }
    static std::size_t RecordPayloadBytes(const DecalSurfaceRecord& record) {
        auto bytes = DecalStateDetail::Add(sizeof(DecalSurfaceRecord),
            DecalStateDetail::Add(record.ownerIdentity.size(), 1u));
        return DecalStateDetail::Add(bytes,
            DecalStateDetail::Multiply(record.nodeMembership.size(), sizeof(std::int32_t)));
    }
    void Prepare(const DecalStateUsage& projected, DecalStateStats& stats,
                 std::size_t incomingRecordBytes = 0u) const {
        DecalStateDetail::ValidateUsage(projected, limits_);
        // Account structural storage + all payload bytes, NOT allocator metadata
        // or process RSS. Counts also bound every vector/string independently.
        // Both old and candidate states coexist until publication. The candidate
        // first copies the old state, then grows/replaces a vector; during that
        // replacement its old buffer can coexist with the new one. Charge an
        // additional whole old representation (a conservative bound for any such
        // buffer), and charge the copied incoming record including its node list.
        // A shrinking mutation still first copies the FULL old representation.
        // This is a structural/payload budget, not a custom allocation/RSS limit:
        // allocator metadata, reserved capacity, caller-owned inputs, external
        // snapshots and resolver allocations are covered by the host separately.
        const auto candidateBytes = projected.storageBytes > usage_.storageBytes
            ? projected.storageBytes : usage_.storageBytes;
        stats.stagedBytes = DecalStateDetail::Add(usage_.storageBytes, candidateBytes);
        stats.stagedBytes = DecalStateDetail::Add(stats.stagedBytes, usage_.storageBytes);
        stats.stagedBytes = DecalStateDetail::Add(stats.stagedBytes, incomingRecordBytes);
        if (stats.stagedBytes > limits_.stagingBytes)
            DecalStateDetail::Fail("staging byte budget exceeded");
        auto work = DecalStateDetail::Add(usage_.surfaces, usage_.owners);
        work = DecalStateDetail::Add(work, usage_.records);
        work = DecalStateDetail::Add(work, usage_.nodeMembership);
        work = DecalStateDetail::Add(work, usage_.surfListEntries);
        work = DecalStateDetail::Add(work, usage_.identityBytes);
        DecalStateDetail::Step(stats, work, limits_);
    }
    void Publish(DecalStateSnapshot& prepared, const DecalStateUsage& usage) noexcept {
        state_.surfaces.swap(prepared.surfaces);
        state_.owners.swap(prepared.owners);
        usage_ = usage;
    }

    DecalStateLimits limits_;
    std::size_t nodeCount_{};
    DecalStateSnapshot state_;
    DecalStateUsage usage_;
    std::vector<std::uint64_t> surfaceSerials_;
    std::shared_ptr<const DecalStateDetail::PrimaryTokenDomain> tokenDomain_;
    bool mutationActive_{};
};

} // namespace QuestVr
