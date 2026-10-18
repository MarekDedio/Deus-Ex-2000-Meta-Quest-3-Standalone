#pragma once

#include "portable_unreal_runtime.h"

// Read-only actual primitive-provider diagnostic. It does not register a hash,
// pose/move an actor, publish startup, or implement encroachment by itself.
// Primitive/Mesh/LodMesh share retail UPrimitive.PointCheck; Model uses its
// distinct zero/box occupancy service. No caller seed here is claimed as an
// original hash seed, and no process-local object address is exported.
struct PortableActorPrimitivePointSnapshot {
    PortableActorPrimitiveSnapshot primitive;
    std::string pointImplementation;
    bool clear{},hitOwner{},hitPrimitiveIsNone{},untouchedCallerFields{};
    bool nextPreserved{},hitSelectedPrimitive{};
    std::array<float,3u> location{},normal{};
    float time{};
    std::int32_t item{-1};
};
// The detached diagnostic seeds Next=37, Time=17, Item=73, Primitive=None,
// Actor=None, Location=(101,102,103), Normal=(104,105,106). The internal native
// provider instead preserves whatever complete packet its actual caller owns.
PortableActorPrimitivePointSnapshot ProbePortableRuntimeActorPrimitivePoint(
    const std::string& actorPath,const std::array<float,3u>& location,
    const std::array<float,3u>& extent={},std::uint32_t flags=0u);
