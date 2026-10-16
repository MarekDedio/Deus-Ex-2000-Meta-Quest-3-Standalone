#pragma once

#include "quest_actor_line_trace.h"
#include "quest_model_extent_trace.h"

namespace QuestVr {

// Caller-seeded detached FCheckResult adapter for the NONZERO extent branch.
// Tokens must denote the actual Model/Owner in a future native service; this
// is not permission to substitute an actor index or rendered brush bounds.
// Owner nonnull requires its already captured ToWorld value. No point/mesh
// branch, coordinate provider, collision registration or VM bridge is added.
inline bool CheckOriginalModelExtent(OriginalActorLineHit& caller,const PortableModelGeometry& model,
    std::uintptr_t modelIdentity,std::uintptr_t ownerIdentity,const OriginalModelCoords* capturedCoords,
    const PortableModelVec3& end,const PortableModelVec3& start,const PortableModelVec3& extent,
    const ModelExtentTraceLimits& limits={}) {
    // Empty Model never reads Owner/Extent or writes the caller. Still admit
    // bounded serialized structure/RootOutside before returning its value.
    if(model.nodes.empty())
        return ModelExtentTraceDetail::Trace(model,start,end,extent,limits,nullptr).clear;
    if(!modelIdentity||static_cast<bool>(ownerIdentity)!=(capturedCoords!=nullptr))
        throw std::invalid_argument("Original Model extent adapter requires actual Model/Owner coordinate identity");
    const auto result=ModelExtentTraceDetail::Trace(model,start,end,extent,limits,capturedCoords);
    // Native seeds Time2 on every nonempty call. A miss writes ONLY Time.
    // Refusals export none of the original partial writes: publish atomically.
    auto prepared=caller;prepared.time=result.time;
    if(result.hit) {
        prepared.location=result.location;prepared.normal=result.normal;
        prepared.actor=ownerIdentity;prepared.primitive=modelIdentity;
    }
    // Both Next and Item are untouched, including accepted owner/world hits.
    caller=prepared;return result.clear;
}

} // namespace QuestVr
