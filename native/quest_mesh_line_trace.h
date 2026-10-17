#pragma once

#include "quest_actor_line_trace.h"

#include <bit>
#include <cstdint>
#include <optional>

namespace QuestVr {
namespace MeshLineTraceDetail {

inline bool ZeroOrUnordered(float value) {
    const auto bits=std::bit_cast<std::uint32_t>(value);
    return (bits&0x7fffffffu)==0u ||
        ((bits&0x7f800000u)==0x7f800000u && (bits&0x007fffffu)!=0u);
}

inline PortableModelVec3 Extent(const PortableModelVec3& value) {
    // Original x87 equality tests inspect C3, which is also set for unordered
    // NaNs. Classify bits so discarded signaling NaNs need no FP comparison.
    // Any nonzero/non-NaN component preserves ALL original component bits.
    if(ZeroOrUnordered(value.x)&&ZeroOrUnordered(value.y)&&ZeroOrUnordered(value.z))
        return {0.0f,0.0f,0.0f};
    return value;
}

} // namespace MeshLineTraceDetail

// Exact detached UMesh/ULodMesh.LineCheck extent wrapper, not a native Mesh
// identity, animated triangle query, render bound or collision registration.
// The original body forwards the same caller/owner/end/start/flags to its
// UPrimitive implementation; that implementation never reads Flags.
// Null owner therefore still clears without validating inactive data after
// extent canonicalization. The shared core retains partial Normal writes on
// genuine clear and atomically refuses unsupported active cylinder data.
inline bool OriginalMeshLineCheck(OriginalActorLineHit& hit,
    const std::optional<OriginalActorCylinder>& owner,const PortableModelVec3& end,
    const PortableModelVec3& start,const PortableModelVec3& extent={},std::uint32_t flags=0u) {
    static_cast<void>(flags);
    return CheckOriginalActorCylinder(hit,owner,end,start,MeshLineTraceDetail::Extent(extent));
}

} // namespace QuestVr
