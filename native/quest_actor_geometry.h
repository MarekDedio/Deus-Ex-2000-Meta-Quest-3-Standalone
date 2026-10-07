#pragma once

#include "portable_unreal_runtime.h"
#include "quest_actor_transform.h"

#include <array>
#include <stdexcept>

namespace QuestVr {

// Shared CPU triangle preparation for the Quest streaming renderer and offline
// actor captures. Package vertices remain in decoded UE object units until here.
struct ActorTriangleVertex {
    ActorVec3 position, normal;
    float u{}, v{};
};

inline ActorTransform BuildSnapshotActorTransform(const PortableActorSnapshot& actor,
    const ActorVec3& origin, bool brush = false) {
    const ActorVec3 location{actor.x,actor.y,actor.z};
    const ActorVec3 pivot{actor.prePivotX,actor.prePivotY,actor.prePivotZ};
    return brush ? BuildBrushToQuest(location,pivot,actor.pitch,actor.yaw,actor.roll,
        {actor.mainScaleX,actor.mainScaleY,actor.mainScaleZ},origin) :
        BuildActorToQuest(location,pivot,actor.pitch,actor.yaw,actor.roll,actor.drawScale,
            {actor.drawScaleX,actor.drawScaleY,actor.drawScaleZ},origin);
}

inline std::array<ActorTriangleVertex,3> BuildActorTriangle(const PortableLodMesh& mesh,
    std::size_t first, const ActorTransform& transform) {
    if (first % 3u != 0u || first+3u > mesh.triangles.size())
        throw std::runtime_error("Actor triangle cursor is outside the decoded mesh");
    const auto& a = mesh.triangles[first];
    const auto& b = mesh.triangles[first+1u];
    const auto& c = mesh.triangles[first+2u];
    if (a.material != b.material || a.material != c.material ||
        a.polyFlags != b.polyFlags || a.polyFlags != c.polyFlags)
        throw std::runtime_error("Actor triangle has mixed materials or polygon flags");
    const ActorVec3 ab{b.x-a.x,b.y-a.y,b.z-a.z}, ac{c.x-a.x,c.y-a.y,c.z-a.z};
    const ActorVec3 face = NormalizeActorVector({ab.y*ac.z-ab.z*ac.y,
        ab.z*ac.x-ab.x*ac.z,ab.x*ac.y-ab.y*ac.x});
    std::array<ActorTriangleVertex,3> result;
    const bool reversed = transform.mirrored != (mesh.scaleX*mesh.scaleY*mesh.scaleZ < 0.0f);
    const std::size_t order[3]{0u,reversed ? 2u : 1u,reversed ? 1u : 2u};
    for (std::size_t corner = 0u; corner < 3u; ++corner) {
        const auto& source = mesh.triangles[first+order[corner]];
        if (!std::isfinite(source.u) || !std::isfinite(source.v) ||
            !IsFiniteActorVector({source.x,source.y,source.z}))
            throw std::runtime_error("Actor vertex is non-finite");
        const ActorVec3 normal = NormalizeActorVector({source.nx,source.ny,source.nz});
        result[corner] = {transform.TransformPoint({source.x,source.y,source.z}),
            transform.TransformNormal(normal.x == 0.0f && normal.y == 0.0f && normal.z == 0.0f ? face : normal),
            source.u,source.v};
        if (!IsFiniteActorVector(result[corner].position))
            throw std::runtime_error("Transformed actor vertex is non-finite");
    }
    return result;
}

inline std::uint32_t ActorTrianglePolyFlags(const PortableActorSnapshot& actor,
    const PortableMeshVertex& vertex, std::uint32_t textureFlags = 0u) {
    return vertex.polyFlags | ActorMaterialPolyFlags(actor.style,actor.unlit,
        actor.noSmooth,actor.meshEnvironmentMap) | (textureFlags & 0x00000002u);
}

} // namespace QuestVr
