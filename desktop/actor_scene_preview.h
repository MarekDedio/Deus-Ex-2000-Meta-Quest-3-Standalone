#pragma once

#include "visual_renderer.h"
#include "quest_actor_geometry.h"
#include "quest_map_lighting.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace questvisual {

struct ActorPosePreviewOptions {
    std::string actorPath;
    std::optional<std::string> sequence;
    std::optional<float> frame;
    std::optional<std::uint8_t> fatness;
};

struct ActorPreviewRecord {
    std::string path, classPath, assetPath, error;
    Vec3 position, minimum, maximum;
    std::size_t triangles{}, overriddenMaterials{}, missingMaterials{};
    std::size_t firstChunk{}, chunkCount{};
    bool brush{}, hidden{}, placeholder{};
    bool poseSampled{}, sequenceFallback{}, poseFixture{};
    bool selectedOriginalSpanInvalid{};
    std::string requestedSequence, resolvedSequence, animationSource;
    float animationFrame{};
    std::uint8_t fatness{128u};
    std::size_t sampledFrames{}, normalTriangleSamples{}, animationBytes{};
    std::vector<std::string> availableSequences;
    std::vector<std::string> invalidOriginalSpanSequences;
};
struct ActorScenePreview {
    bool enabled{};
    std::size_t meshInstances{}, brushInstances{}, hiddenActors{}, unsupportedActors{};
    std::size_t overriddenMaterials{}, missingMaterials{}, decodedTextures{}, fallbackTextures{};
    std::size_t vertices{}, environmentMappedTriangles{}, spriteActorsOmitted{};
    std::size_t maskedTextureVariants{};
    std::size_t sampledPoses{}, poseOmissions{};
    std::vector<std::string> texturePaths;
    std::vector<ActorPreviewRecord> records;
};

inline ActorScenePreview AppendActorScenePreview(Scene& scene,
    const std::vector<PortableActorSnapshot>& actors, const QuestVr::ActorVec3& origin,
    const std::vector<QuestVr::MapLight>& lights, bool actorLighting,
    const ActorPosePreviewOptions& poseOptions = {}) {
    ActorScenePreview metadata;
    metadata.enabled = true;
    const auto summary = DecodePortableRuntimeActorMeshes();
    if (!summary.passed) throw std::runtime_error("Original actor asset decoding failed");
    auto textures = BuildPortableRuntimeActorTextureArray(96u,96u);
    metadata.decodedTextures = textures.decodedTextures;
    metadata.fallbackTextures = textures.failedTextures;
    metadata.maskedTextureVariants = textures.maskedTextureVariants;
    metadata.texturePaths = textures.texturePaths;
    if (!textures.texturePaths.empty() && (!textures.passed || textures.texturePaths.size()>255u))
        throw std::runtime_error("Original actor material array is invalid");
    scene.actorTextureWidth = textures.texturePaths.empty() ? 0u : textures.width;
    scene.actorTextureHeight = textures.texturePaths.empty() ? 0u : textures.height;
    scene.actorTextureLayers = static_cast<std::uint32_t>(textures.texturePaths.size());
    scene.actorTextures = std::move(textures.rgba);
    std::map<std::string,std::size_t> layers;
    for (std::size_t i = 0u; i < textures.texturePaths.size(); ++i) layers.emplace(textures.texturePaths[i],i);
    std::map<std::string,PortableLodMesh> meshes, brushes;
    const auto ensureStreams = [&] {
        if (scene.vertexLighting.empty()) {
            scene.vertexLighting.resize(scene.chunks.size());
            for (std::size_t c = 0; c < scene.chunks.size(); ++c)
                scene.vertexLighting[c].assign(scene.chunks[c].vertices.size(),{1,1,1});
        }
    };
    ensureStreams();
    for (const auto& actor : actors) {
        if (!actor.hasLocation || !(actor.pawn || actor.inventory || actor.decoration || actor.mover)) continue;
        ActorPreviewRecord record;
        record.path = actor.objectPath; record.classPath = actor.classPath;
        record.position = {(actor.y-origin.y)/52.5f,(actor.z-origin.z)/52.5f+1.0f,-(actor.x-origin.x)/52.5f};
        record.hidden = actor.hidden || actor.drawType == 0u;
        if (record.hidden) { ++metadata.hiddenActors; metadata.records.push_back(record); continue; }
        record.brush = actor.mover && !actor.activated && !actor.brushPath.empty();
        record.assetPath = record.brush ? actor.brushPath : actor.meshPath;
        if (record.assetPath.empty()) {
            if (actor.drawType == 1u || actor.drawType == 4u || actor.drawType == 5u || actor.drawType == 7u)
                ++metadata.spriteActorsOmitted;
            else ++metadata.unsupportedActors;
            record.error = "No decoded mesh/brush; sprites and cube placeholders are intentionally not rendered in this preview";
            metadata.records.push_back(record); continue;
        }
        const auto priorVertices = metadata.vertices;
        const auto priorOverrides = metadata.overriddenMaterials;
        const auto priorMissing = metadata.missingMaterials;
        const auto priorEnvironmentTriangles = metadata.environmentMappedTriangles;
        const auto priorChunks = scene.chunks.size();
        const auto priorLightingChunks = scene.vertexLighting.size();
        const auto priorLightmapChunks = scene.lightmapVertices.size();
        try {
            auto& cache = record.brush ? brushes : meshes;
            auto found = cache.find(record.assetPath);
            if (found == cache.end()) found = cache.emplace(record.assetPath,record.brush ?
                GetPortableRuntimeBrush(record.assetPath) : GetPortableRuntimeMesh(record.assetPath)).first;
            const auto& mesh = found->second;
            std::optional<QuestVr::MeshPose> pose;
            if (!record.brush && mesh.animation) {
                auto state = QuestVr::BuildSnapshotMeshAnimationState(actor);
                record.animationSource = actor.animationSourcePath;
                if (actor.objectPath == poseOptions.actorPath) {
                    record.poseFixture = poseOptions.sequence || poseOptions.frame || poseOptions.fatness;
                    if (poseOptions.sequence) state.main.sequence = *poseOptions.sequence;
                    if (poseOptions.frame) state.main.normalizedFrame = *poseOptions.frame;
                    if (poseOptions.fatness) state.fatness = *poseOptions.fatness;
                    for (const auto& sequence : mesh.animation->sequences) {
                        record.availableSequences.push_back(sequence.name);
                        if (sequence.invalidOriginalSpan) record.invalidOriginalSpanSequences.push_back(sequence.name);
                    }
                }
                record.requestedSequence = state.main.sequence;
                record.animationFrame = state.main.normalizedFrame;
                record.fatness = state.fatness;
                pose = QuestVr::PrepareMeshPose(mesh,state);
                record.resolvedSequence = pose->resolvedSequence;
                record.sequenceFallback = pose->fallbackUsed;
                record.selectedOriginalSpanInvalid = pose->selectedOriginalSpanInvalid;
                if (!pose->drawable) {
                    ++metadata.poseOmissions;
                    record.error = pose->error.empty() ? "Mesh has no authored animation sequences; omitted as in pinned renderer" : pose->error;
                    metadata.records.push_back(std::move(record));
                    continue;
                }
                record.poseSampled = true;
                record.sampledFrames = pose->sampledFrames;
                record.normalTriangleSamples = pose->normalTriangleSamples;
                record.animationBytes = pose->retainedBytes;
                ++metadata.sampledPoses;
            }
            const auto transform = QuestVr::BuildSnapshotActorTransform(actor,origin,record.brush);
            const auto lighting = actorLighting && !actor.unlit ? QuestVr::CalculateMapLighting(lights,
                {record.position.x,record.position.y,record.position.z},{0,1,0}) : OVR::Vector3f{1,1,1};
            std::map<std::pair<std::size_t,std::uint32_t>,Chunk> chunks;
            std::set<std::uint16_t> accountedMaterials;
            bool bounds{};
            for (std::size_t first = 0u; first+2u < mesh.triangles.size(); first += 3u) {
                const auto& source = mesh.triangles[first];
                const auto material = QuestVr::ResolveActorMeshMaterial(actor.materialOverrides,
                    mesh.texturePaths,mesh.materialTextureIndices,source.material);
                const auto layer = layers.find(material.texturePath);
                const bool firstMaterial = accountedMaterials.insert(source.material).second;
                if (layer == layers.end()) {
                    if (firstMaterial) { ++record.missingMaterials; ++metadata.missingMaterials; }
                    continue;
                }
                if (firstMaterial && material.source != QuestVr::ActorMaterialSource::Mesh) {
                    ++record.overriddenMaterials; ++metadata.overriddenMaterials;
                }
                const std::uint32_t textureFlags = layer->second < textures.texturePolyFlags.size() ?
                    textures.texturePolyFlags[layer->second] : 0u;
                const auto flags = QuestVr::ActorTrianglePolyFlags(actor,source,textureFlags);
                const auto maskedLayer = layer->second < textures.maskedTextureLayers.size() ?
                    textures.maskedTextureLayers[layer->second] : -1;
                const auto drawLayer = (flags & 2u) != 0u && maskedLayer >= 0 ?
                    static_cast<std::size_t>(maskedLayer) : layer->second;
                if ((flags & 0x00000001u) != 0u) continue; // PF_Invisible
                if ((flags & 0x00000010u) != 0u) ++metadata.environmentMappedTriangles;
                auto& chunk = chunks[{drawLayer,flags}];
                chunk.materialSlot = 0; chunk.textureBank = TextureBank::Actor; chunk.polyFlags = flags;
                for (const auto& vertex : QuestVr::BuildActorTriangle(mesh,first,transform,pose ? &*pose : nullptr)) {
                    const Vec3 p{vertex.position.x,vertex.position.y,vertex.position.z};
                    chunk.vertices.push_back({p,{vertex.normal.x,vertex.normal.y,vertex.normal.z},
                        vertex.u,vertex.v,static_cast<std::int32_t>(drawLayer)});
                    if (!bounds) { record.minimum = record.maximum = p; bounds = true; }
                    else {
                        record.minimum = {std::min(record.minimum.x,p.x),std::min(record.minimum.y,p.y),std::min(record.minimum.z,p.z)};
                        record.maximum = {std::max(record.maximum.x,p.x),std::max(record.maximum.y,p.y),std::max(record.maximum.z,p.z)};
                    }
                }
                ++record.triangles;
                metadata.vertices += 3u;
                if (metadata.vertices > 2'000'000u) throw std::runtime_error("Actor preview vertex budget exceeded");
            }
            record.firstChunk = scene.chunks.size();
            record.chunkCount = chunks.size();
            for (auto& [key,chunk] : chunks) {
                static_cast<void>(key);
                scene.vertexLighting.emplace_back(chunk.vertices.size(),Vec3{lighting.x,lighting.y,lighting.z});
                if (!scene.lightmapVertices.empty()) scene.lightmapVertices.emplace_back(chunk.vertices.size());
                scene.chunks.push_back(std::move(chunk));
            }
            if (record.triangles != 0u) {
                if (record.brush) ++metadata.brushInstances; else ++metadata.meshInstances;
            } else { ++metadata.unsupportedActors; record.error = "No available materials for this mesh"; }
        } catch (const std::exception& error) {
            // An actor is a transaction: local/partially committed chunks must
            // not inflate submitted triangle counts or consume future budget.
            scene.chunks.resize(priorChunks);
            scene.vertexLighting.resize(priorLightingChunks);
            scene.lightmapVertices.resize(priorLightmapChunks);
            metadata.vertices = priorVertices;
            metadata.overriddenMaterials = priorOverrides;
            metadata.missingMaterials = priorMissing;
            metadata.environmentMappedTriangles = priorEnvironmentTriangles;
            record.triangles = record.overriddenMaterials = record.missingMaterials = 0u;
            record.firstChunk = record.chunkCount = 0u;
            record.minimum = record.maximum = {};
            ++metadata.unsupportedActors; record.error = error.what();
        }
        metadata.records.push_back(std::move(record));
    }
    return metadata;
}

} // namespace questvisual
