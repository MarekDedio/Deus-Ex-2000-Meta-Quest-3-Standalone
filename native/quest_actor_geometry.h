#pragma once

#include "portable_unreal_runtime.h"
#include "quest_actor_transform.h"
#include "quest_mesh_animation.h"

#include <array>
#include <stdexcept>

namespace QuestVr {

// Shared CPU triangle preparation for the Quest streaming renderer and offline
// actor captures. Package vertices remain in decoded UE object units until here.
struct ActorTriangleVertex {
    ActorVec3 position, normal;
    float u{}, v{};
};

// Snapshot values are authored defaults/instance properties, not a substitute
// for state-machine events. Authored maps have no retained tween history;
// explicitly executed native transitions supply it through the snapshot.
inline MeshAnimationState BuildSnapshotMeshAnimationState(const PortableActorSnapshot& actor) {
    MeshAnimationState state;
    state.main.sequence = actor.animation.sequence;
    state.main.normalizedFrame = actor.animation.frame;
    state.main.previous = actor.animation.previous;
    state.fatness = actor.fatness;
    for (std::size_t i = 0u; i < state.blends.size(); ++i) {
        state.blends[i].sequence = actor.animation.blends[i].sequence;
        state.blends[i].normalizedFrame = actor.animation.blends[i].frame;
        state.blends[i].previous = actor.animation.blends[i].previous;
    }
    return state;
}

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
    std::size_t first, const ActorTransform& transform, const MeshPose* pose = nullptr) {
    if (first % 3u != 0u || first+3u > mesh.triangles.size())
        throw std::runtime_error("Actor triangle cursor is outside the decoded mesh");
    const auto& a = mesh.triangles[first];
    const auto& b = mesh.triangles[first+1u];
    const auto& c = mesh.triangles[first+2u];
    if (a.material != b.material || a.material != c.material ||
        a.polyFlags != b.polyFlags || a.polyFlags != c.polyFlags)
        throw std::runtime_error("Actor triangle has mixed materials or polygon flags");
    const auto positionAt = [&](std::size_t corner) -> ActorVec3 {
        if (!pose) {
            const auto& vertex = mesh.triangles[first+corner];
            return {vertex.x,vertex.y,vertex.z};
        }
        if (!pose->drawable || !mesh.animation ||
            mesh.animation->triangleSourceVertexIndices.size() != mesh.triangles.size())
            throw std::runtime_error("Actor animation pose/topology is not drawable");
        const auto index = mesh.animation->triangleSourceVertexIndices[first+corner];
        if (index >= pose->objectPositions.size() || index >= pose->objectNormals.size())
            throw std::runtime_error("Actor animation source index is outside the sampled pose");
        return pose->objectPositions[index];
    };
    const auto pa = positionAt(0), pb = positionAt(1), pc = positionAt(2);
    const ActorVec3 ab{pb.x-pa.x,pb.y-pa.y,pb.z-pa.z}, ac{pc.x-pa.x,pc.y-pa.y,pc.z-pa.z};
    const ActorVec3 face = NormalizeActorVector({ab.y*ac.z-ab.z*ac.y,
        ab.z*ac.x-ab.x*ac.z,ab.x*ac.y-ab.y*ac.x});
    std::array<ActorTriangleVertex,3> result;
    const bool reversed = transform.mirrored != (mesh.scaleX*mesh.scaleY*mesh.scaleZ < 0.0f);
    const std::size_t order[3]{0u,reversed ? 2u : 1u,reversed ? 1u : 2u};
    for (std::size_t corner = 0u; corner < 3u; ++corner) {
        const auto& source = mesh.triangles[first+order[corner]];
        const auto position = positionAt(order[corner]);
        const auto sourceNormal = pose ? pose->objectNormals[
            mesh.animation->triangleSourceVertexIndices[first+order[corner]]] :
            ActorVec3{source.nx,source.ny,source.nz};
        if (!std::isfinite(source.u) || !std::isfinite(source.v) ||
            !IsFiniteActorVector(position))
            throw std::runtime_error("Actor vertex is non-finite");
        const ActorVec3 normal = NormalizeActorVector(sourceNormal);
        result[corner] = {transform.TransformPoint(position),
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
