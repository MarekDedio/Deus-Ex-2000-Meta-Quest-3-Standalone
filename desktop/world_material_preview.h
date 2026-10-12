#pragma once
#include "visual_renderer.h"
#include "portable_model_geometry.h"
#include "quest_world_material.h"

namespace questvisual {
// Recover original surface materials outside the immutable DXQM layout.
// Called before actor append so all parallel streams remain world-only here.
inline void ApplyOriginalWorldMaterials(Scene& scene,const PortablePackageTables& package,
    const std::filesystem::path& meshPath) {
    const auto surfaces = QuestVr::ReadWorldSurfaceStream(meshPath.string()+".surfaces");
    if (surfaces.size() != scene.chunks.size() ||
        (!scene.vertexLighting.empty() && scene.vertexLighting.size() != scene.chunks.size()) ||
        (!scene.lightmapVertices.empty() && scene.lightmapVertices.size() != scene.chunks.size()))
        throw std::runtime_error("Original world material parallel chunk streams disagree");
    const auto model = LoadPortableRootModel68(package);
    std::vector<std::uint32_t> flags;
    for (const auto& surface : model.surfaces) flags.push_back(surface.polyFlags);
    std::vector<Chunk> chunks;
    std::vector<std::vector<Vec3>> lighting;
    std::vector<std::vector<LightmapVertex>> lightmaps;
    for (std::size_t c=0u;c<scene.chunks.size();++c) {
        const auto& original=scene.chunks[c];
        if (original.textureBank != TextureBank::World || surfaces[c].materialSlot != original.materialSlot ||
            surfaces[c].records.size() != original.vertices.size() ||
            (!scene.vertexLighting.empty() && scene.vertexLighting[c].size() != original.vertices.size()) ||
            (!scene.lightmapVertices.empty() && scene.lightmapVertices[c].size() != original.vertices.size()))
            throw std::runtime_error("Original world material parallel vertices disagree");
        for (const auto& range : QuestVr::BuildWorldMaterialRanges(surfaces[c].records,flags)) {
            Chunk chunk;
            chunk.materialSlot=original.materialSlot;
            chunk.polyFlags=range.polyFlags;
            // Both windings already exist in DXQM, so do not add TwoSided:
            // the software renderer must reject the opposite copy like Quest.
            chunk.vertices.assign(original.vertices.begin()+static_cast<std::ptrdiff_t>(range.begin),
                original.vertices.begin()+static_cast<std::ptrdiff_t>(range.end));
            chunks.push_back(std::move(chunk));
            if (!scene.vertexLighting.empty()) lighting.emplace_back(
                scene.vertexLighting[c].begin()+static_cast<std::ptrdiff_t>(range.begin),
                scene.vertexLighting[c].begin()+static_cast<std::ptrdiff_t>(range.end));
            if (!scene.lightmapVertices.empty()) lightmaps.emplace_back(
                scene.lightmapVertices[c].begin()+static_cast<std::ptrdiff_t>(range.begin),
                scene.lightmapVertices[c].begin()+static_cast<std::ptrdiff_t>(range.end));
        }
    }
    scene.chunks=std::move(chunks);
    scene.vertexLighting=std::move(lighting);
    scene.lightmapVertices=std::move(lightmaps);
}
} // namespace questvisual
