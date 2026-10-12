#pragma once

#include "quest_decal_state.h"

#include <cstdint>
#include <string>

namespace QuestVr {

// Deliberate port-checkpoint extension, NOT original GOTY disk serialization.
// The original model archive does not save FDecal geometry. Restore must stage
// this exact symbolic graph without reattaching, regrouping by live Texture,
// projecting corners again, or replaying the engine's random stream. Runtime
// schema preflight must prove every identity/count against the actual map and
// native Level/model, and resolve owner strings to independently rooted actors.
struct ScriptSavedNativeDecals {
    std::string mapName, modelPath, levelPath;
    std::uint32_t surfaceCount{}, nodeCount{};
    // Surface indices are positional. Record order, node order/duplicates,
    // owner-mirror order and SurfList duplicates are all retained verbatim.
    // Missing/foreign mirror references do not reconstruct or delete records.
    DecalStateSnapshot state;
};

// Non-copy preflight for a host's aggregate capture/transaction budget. Charge
// actual reserved capacities (including conservatively charged string SSO),
// not just serialized element sizes. Allocator bookkeeping/RSS is excluded.
// This does not replace structural/schema validation or independently rooted
// object edges. It refuses before walking an unbounded nested record graph.
inline std::size_t DecalSnapshotRetainedBytes(const DecalStateSnapshot& state,
                                            std::size_t surfaceCount, std::size_t nodeCount,
                                            std::size_t maxBytes = 64u * 1024u * 1024u) {
    const DecalStateLimits caps;
    if (surfaceCount > caps.surfaces || nodeCount > caps.nodes ||
        state.surfaces.size() != surfaceCount || state.owners.size() > caps.owners)
        DecalStateDetail::Fail("checkpoint counts exceed fixed model bounds");
    std::size_t bytes = sizeof(DecalStateSnapshot);
    const auto retain = [&](std::size_t count, std::size_t size) {
        const auto added = DecalStateDetail::Multiply(count, size);
        if (added > maxBytes || bytes > maxBytes - added)
            DecalStateDetail::Fail("checkpoint retained capacity budget exceeded");
        bytes += added;
    };
    const auto text = [&](const std::string& value) {
        if (value.empty() || value.size() > caps.identityBytes || value.find('\0') != std::string::npos)
            DecalStateDetail::Fail("checkpoint identity is null, oversized or contains NUL");
        retain(DecalStateDetail::Add(value.capacity(), 1u), sizeof(char));
    };
    retain(state.surfaces.capacity(), sizeof(std::vector<DecalSurfaceRecord>));
    retain(state.owners.capacity(), sizeof(DecalOwnerSurfList));
    std::size_t records{}, membership{}, entries{};
    for (const auto& surface : state.surfaces) {
        if (surface.size() > caps.records - records)
            DecalStateDetail::Fail("checkpoint retained record count budget exceeded");
        records += surface.size(); retain(surface.capacity(), sizeof(DecalSurfaceRecord));
        for (const auto& record : surface) {
            text(record.ownerIdentity);
            if (record.nodeMembership.size() > caps.nodeMembership - membership)
                DecalStateDetail::Fail("checkpoint retained membership count budget exceeded");
            membership += record.nodeMembership.size();
            retain(record.nodeMembership.capacity(), sizeof(std::int32_t));
        }
    }
    for (const auto& owner : state.owners) {
        text(owner.ownerIdentity);
        if (owner.surfaces.size() > caps.surfListEntries - entries)
            DecalStateDetail::Fail("checkpoint retained SurfList count budget exceeded");
        entries += owner.surfaces.size(); retain(owner.surfaces.capacity(), sizeof(std::int32_t));
    }
    if (bytes > maxBytes) DecalStateDetail::Fail("checkpoint retained byte budget exceeded");
    return bytes;
}

inline std::size_t DecalCheckpointRetainedBytes(const ScriptSavedNativeDecals& saved,
                                              std::size_t maxBytes = 64u * 1024u * 1024u) {
    auto bytes = DecalSnapshotRetainedBytes(saved.state,saved.surfaceCount,saved.nodeCount,maxBytes);
    const auto retain = [&](std::size_t added) {
        if (added > maxBytes || bytes > maxBytes - added)
            DecalStateDetail::Fail("checkpoint retained capacity budget exceeded");
        bytes += added;
    };
    retain(sizeof(ScriptSavedNativeDecals)-sizeof(DecalStateSnapshot));
    for (const auto* value : {&saved.mapName,&saved.modelPath,&saved.levelPath}) {
        if (value->empty() || value->size() > DecalStateLimits{}.identityBytes ||
            value->find('\0') != std::string::npos)
            DecalStateDetail::Fail("checkpoint identity is null, oversized or contains NUL");
        retain(DecalStateDetail::Add(value->capacity(),1u));
    }
    return bytes;
}

} // namespace QuestVr
