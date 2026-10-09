#pragma once

#include "visual_renderer.h"
#include "portable_unreal_runtime.h"
#include "quest_map_lighting.h"
#include "quest_static_lightmap_cache.h"
#include "actor_scene_preview.h"
#include "quest_script_dispatch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace questvisual {

struct AuthoredLightingPreview {
    bool enabled{}, synthetic{}, originVerified{};
    bool emitterTalliesFromAuthoredSnapshots{};
    std::string playerStartPath;
    std::vector<std::string> packagePaths;
    std::size_t runtimeActors{}, unresolvedMapClasses{}, texturedVertices{};
    QuestVr::MapLightBuildStats lightStats;
    double minimumLuminance{}, maximumLuminance{}, meanLuminance{};
    bool baked{};
    QuestVr::StaticLightmapCache staticLightmaps;
    ActorScenePreview actors;
    std::string scriptFunction;
    std::string scriptResultActor;
    std::size_t scriptInstructions{}, scriptWrites{};
};

namespace lightingdetail {
// Level's native tail starts with two int32 array fields, unlike a serialized
// TArray compact count. This bounded read verifies the cache decoder's actual
// first PlayerStart order before applying export-ordered runtime lights.
inline std::vector<std::int32_t> ReadLevelActorOrder(const PortablePackageTables& package) {
    std::size_t level = package.exports.size();
    for (std::size_t index = 0u; index < package.exports.size(); ++index) {
        if (GetPortableObjectPath(package,package.exports[index].ObjClass) == "Engine.Level") {
            level = index;
            break;
        }
    }
    if (level == package.exports.size()) throw std::runtime_error("Authored lighting requires a decoded Engine.Level");
    const auto& entry = package.exports[level];
    const auto properties = LoadPortableExportProperties(package,level);
    if (entry.ObjOffset < 0 || entry.ObjSize <= 0 || entry.ObjSize > 32*1024*1024 ||
        properties.bytesConsumed > static_cast<std::uint32_t>(entry.ObjSize) ||
        static_cast<std::uint32_t>(entry.ObjSize)-properties.bytesConsumed < 8u)
        throw std::runtime_error("Authored-lighting Level payload is invalid");
    const auto tailSize = static_cast<std::uint32_t>(entry.ObjSize)-properties.bytesConsumed;
    std::vector<std::uint8_t> tail(tailSize);
    std::ifstream file(package.sourcePath,std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read authored-lighting map source");
    file.seekg(static_cast<std::uint64_t>(entry.ObjOffset)+properties.bytesConsumed);
    if (!file.read(reinterpret_cast<char*>(tail.data()),static_cast<std::streamsize>(tail.size())))
        throw std::runtime_error("Truncated authored-lighting Level payload");
    const auto int32At = [&](std::size_t offset) {
        const std::uint32_t bits = tail[offset] | (static_cast<std::uint32_t>(tail[offset+1u])<<8u) |
            (static_cast<std::uint32_t>(tail[offset+2u])<<16u) |
            (static_cast<std::uint32_t>(tail[offset+3u])<<24u);
        std::int32_t value;
        std::memcpy(&value,&bits,sizeof(value));
        return value;
    };
    const auto count = int32At(0u), capacity = int32At(4u);
    if (count < 0 || count > 100000 || capacity < count)
        throw std::runtime_error("Authored-lighting Level actor count is invalid");
    std::size_t cursor = 8u;
    const auto compact = [&]() {
        if (cursor >= tail.size()) throw std::runtime_error("Truncated Level actor reference");
        std::uint8_t byte = tail[cursor++];
        const bool negative = (byte&0x80u) != 0u;
        bool more = (byte&0x40u) != 0u;
        std::uint64_t magnitude = byte&0x3fu;
        unsigned shift = 6u, bytes = 1u;
        while (more) {
            if (cursor >= tail.size() || bytes >= 5u)
                throw std::runtime_error("Level actor reference is truncated or overlong");
            byte = tail[cursor++];
            magnitude |= static_cast<std::uint64_t>(byte&0x7fu)<<shift;
            more = (byte&0x80u) != 0u;
            shift += 7u;
            ++bytes;
        }
        if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
            throw std::runtime_error("Level actor reference overflows int32");
        const auto value = static_cast<std::int32_t>(magnitude);
        return negative ? -value : value;
    };
    std::vector<std::int32_t> references;
    references.reserve(static_cast<std::size_t>(count));
    for (std::int32_t index = 0; index < count; ++index) {
        const auto reference = compact();
        if ((reference > 0 && static_cast<std::size_t>(reference)>package.exports.size()) ||
            (reference < 0 && static_cast<std::uint64_t>(-static_cast<std::int64_t>(reference))>package.imports.size()))
            throw std::runtime_error("Level actor reference is outside map tables");
        references.push_back(reference);
    }
    return references;
}

inline std::string VerifyMapLightingOrigin(const PortablePackageTables& package,
    const std::vector<PortableActorSnapshot>& actors) {
    const PortableActorSnapshot* lightingStart = nullptr;
    for (const auto& actor : actors) {
        const auto dot = actor.classPath.find_last_of('.');
        const auto leaf = dot == std::string::npos ? actor.classPath : actor.classPath.substr(dot+1u);
        if (leaf == "PlayerStart" && actor.hasLocation) { lightingStart = &actor; break; }
    }
    if (!lightingStart) throw std::runtime_error(
        "Authored lighting cannot verify cache origin without an actual PlayerStart; bounds-fallback maps are unsupported");
    for (const auto reference : ReadLevelActorOrder(package)) {
        if (reference <= 0) continue;
        const auto& entry = package.exports[static_cast<std::size_t>(reference-1)];
        const auto object = GetPortableObjectPath(package,reference);
        const auto cls = GetPortableObjectPath(package,entry.ObjClass);
        if (object.find("PlayerStart") == std::string::npos && cls.find("PlayerStart") == std::string::npos) continue;
        const auto expectedPath = std::filesystem::path(package.sourcePath).stem().string()+"."+object;
        if (lightingStart->objectPath != expectedPath)
            throw std::runtime_error("Map cache and authored lights select different PlayerStart origins");
        const auto properties = LoadPortableExportProperties(package,static_cast<std::size_t>(reference-1));
        for (const auto& property : properties.properties) {
            if (property.name == "Location" && property.type == 10u && property.value.size() == 12u) {
                std::array<float,3> position{};
                std::memcpy(position.data(),property.value.data(),12u);
                if (!std::isfinite(position[0]) || !std::isfinite(position[1]) || !std::isfinite(position[2]) ||
                    position[0] != lightingStart->x || position[1] != lightingStart->y || position[2] != lightingStart->z)
                    throw std::runtime_error("Map cache and authored lights disagree on PlayerStart location");
                return expectedPath;
            }
        }
        throw std::runtime_error("Cache PlayerStart has no valid serialized Location");
    }
    throw std::runtime_error("No Level-ordered PlayerStart found for lighting origin verification");
}

inline void ApplyVertexLighting(Scene& scene, const std::vector<QuestVr::MapLight>& lights,
                                AuthoredLightingPreview& metadata) {
    scene.vertexLighting.clear();
    scene.vertexLighting.resize(scene.chunks.size());
    metadata.texturedVertices = 0u;
    double minimum = std::numeric_limits<double>::infinity(), maximum{}, sum{};
    for (std::size_t chunk = 0u; chunk < scene.chunks.size(); ++chunk) {
        auto& colors = scene.vertexLighting[chunk];
        colors.reserve(scene.chunks[chunk].vertices.size());
        for (const auto& vertex : scene.chunks[chunk].vertices) {
            // The native textured shader consumes these RGB gains. Its flat
            // diagnostic chunks use a different shader and are left unlit here.
            if (scene.chunks[chunk].materialSlot != 0) { colors.push_back({1,1,1}); continue; }
            const auto rgb = QuestVr::CalculateMapLighting(lights,
                {vertex.position.x,vertex.position.y,vertex.position.z},
                {vertex.normal.x,vertex.normal.y,vertex.normal.z});
            colors.push_back({rgb.x,rgb.y,rgb.z});
            const double luminance = 0.2126*rgb.x+0.7152*rgb.y+0.0722*rgb.z;
            minimum = std::min(minimum,luminance);
            maximum = std::max(maximum,luminance);
            sum += luminance;
            ++metadata.texturedVertices;
        }
    }
    metadata.minimumLuminance = metadata.texturedVertices == 0u ? 0.0 : minimum;
    metadata.maximumLuminance = maximum;
    metadata.meanLuminance = metadata.texturedVertices == 0u ? 0.0 : sum/metadata.texturedVertices;
}
} // namespace lightingdetail

inline AuthoredLightingPreview BuildSyntheticLightingPreview(Scene& scene) {
    AuthoredLightingPreview metadata;
    metadata.enabled = metadata.synthetic = true;
    QuestVr::MapLight fixture;
    fixture.localPosition = {0.0f,1.65f,-1.0f};
    fixture.color = {1.0f,1.0f,1.0f};
    fixture.radiusMeters = 8.0f;
    fixture.intensity = 2.0f;
    metadata.lightStats.total = 1u;
    lightingdetail::ApplyVertexLighting(scene,{fixture},metadata);
    return metadata;
}

inline AuthoredLightingPreview BuildAuthoredLightingPreview(Scene& scene,
    const std::filesystem::path& gameRoot, const std::string& map,
    const std::filesystem::path& meshPath = {}, bool baked = false,
    bool includeActors = false, bool applyLighting = true,
    const ActorPosePreviewOptions& poseOptions = {}) {
    AuthoredLightingPreview metadata;
    metadata.enabled = applyLighting;
    // Same package set as Quest startup, so inherited Light/Spotlight defaults
    // and derived actor classes are resolved through the production runtime.
    constexpr std::array<const char*,38> packages{{
        "ConSys","Core","DeusEx","DeusExCharacters","DeusExConAudioAIBarks",
        "DeusExConAudioEndGame","DeusExConAudioHK_Shared","DeusExConAudioIntro",
        "DeusExConAudioMission00","DeusExConAudioMission01","DeusExConAudioMission02",
        "DeusExConAudioMission03","DeusExConAudioMission04","DeusExConAudioMission05",
        "DeusExConAudioMission08","DeusExConAudioMission09","DeusExConAudioMission10",
        "DeusExConAudioMission11","DeusExConAudioMission12","DeusExConAudioMission14",
        "DeusExConAudioMission15","DeusExConAudioNYShared","DeusExConText",
        "DeusExConversations","DeusExDeco","DeusExItems","DeusExSounds","DeusExText",
        "DeusExUI","Editor","Engine","Extension","Fire","IpDrv","IpServer",
        "MPCharacters","UBrowser","UWindow"}};
    std::vector<PortablePackageTables> tables;
    tables.reserve(packages.size());
    for (const auto name : packages) {
        const auto path = gameRoot/"System"/(std::string(name)+".u");
        metadata.packagePaths.push_back(std::filesystem::absolute(path).generic_string());
        tables.push_back(LoadPortablePackageTables(path.string()));
    }
    struct RuntimeCleanup { ~RuntimeCleanup() { ShutdownPortableRuntime(); } } cleanup;
    if (!InitializePortableRuntime(tables).passed)
        throw std::runtime_error("Original package runtime validation failed for authored lighting");
    const auto package = LoadPortablePackageTables((gameRoot/"Maps"/(map+".dx")).string());
    const auto mapRuntime = LoadPortableRuntimeMap(package);
    if (!mapRuntime.passed) throw std::runtime_error("Original map runtime failed for authored lighting");
    metadata.unresolvedMapClasses = mapRuntime.unresolvedClasses;
    auto effectivePoseOptions=poseOptions;
    if (poseOptions.scriptUseResultActor && poseOptions.scriptFunction.empty())
        throw std::runtime_error("Returned-actor isolation requires an original compiled function");
    if (!poseOptions.scriptFunction.empty()) {
        const auto result = ExecutePortableActorFunction(poseOptions.actorPath,
            poseOptions.scriptFunction, poseOptions.scriptArguments);
        if (!result.passed()) throw std::runtime_error("Isolated compiled helper failed: " +
            result.error + " at " + result.function + ':' + std::to_string(result.offset));
        metadata.scriptFunction = result.function;
        metadata.scriptInstructions = result.instructions;
        metadata.scriptWrites = result.writes;
        if (poseOptions.scriptUseResultActor) {
            if (result.value.kind!=QuestVr::Vm::Kind::Object || result.value.text.empty())
                throw std::runtime_error("Original helper did not return a nonnull Actor Object");
            const auto candidates=GetPortableRuntimeMapActors();
            const auto found=std::find_if(candidates.begin(),candidates.end(),[&](const auto& actor) {
                return QuestVr::ScriptDispatch::FoldName(actor.objectPath)==
                    QuestVr::ScriptDispatch::FoldName(result.value.text);
            });
            if (found==candidates.end()) throw std::runtime_error("Original helper result is not a published current-map Actor");
            metadata.scriptResultActor=found->objectPath;
            effectivePoseOptions.actorPath=found->objectPath;
        }
    }
    const auto actors = GetPortableRuntimeMapActors();
    metadata.runtimeActors = actors.size();
    const auto verified = QuestVr::VerifyQuestMapOrigin(package,actors,metadata.playerStartPath);
    metadata.originVerified = true;
    const OVR::Vector3f verifiedOrigin{verified.x,verified.y,verified.z};
    const auto lights = QuestVr::BuildMapLights(actors,&metadata.lightStats,&verifiedOrigin);
    metadata.emitterTalliesFromAuthoredSnapshots = true;
    if (applyLighting && !baked) lightingdetail::ApplyVertexLighting(scene,lights,metadata);
    else if (applyLighting) {
        const auto surfaceChunks = QuestVr::ReadWorldSurfaceStream(meshPath.string()+".surfaces");
        if (surfaceChunks.size() != scene.chunks.size())
            throw std::runtime_error("Desktop world and surface chunk counts disagree");
        std::vector<QuestVr::StaticLightmapMeshChunk> chunks(scene.chunks.size());
        for (std::size_t c = 0; c < chunks.size(); ++c) {
            const auto& source = scene.chunks[c]; auto& target = chunks[c];
            target.materialSlot = source.materialSlot;
            target.surfaces = surfaceChunks[c].records;
            if (surfaceChunks[c].materialSlot != source.materialSlot || target.surfaces.size() != source.vertices.size())
                throw std::runtime_error("Desktop world and surface vertices disagree");
            target.localPositions.reserve(source.vertices.size());
            for (const auto& vertex : source.vertices)
                target.localPositions.push_back({vertex.position.x,vertex.position.y,vertex.position.z});
        }
        metadata.staticLightmaps = QuestVr::BuildQuestStaticLightmapCache(package,actors,chunks);
        auto& cache = metadata.staticLightmaps;
        scene.lightmapWidth = cache.width; scene.lightmapHeight = cache.height;
        scene.lightmapLayers = cache.layers; scene.lightmapGainScale = cache.gainScale;
        scene.lightmapRgba = std::move(cache.rgba);
        scene.lightmapVertices.resize(cache.vertices.size());
        for (std::size_t c = 0; c < cache.vertices.size(); ++c)
            for (const auto& vertex : cache.vertices[c])
                scene.lightmapVertices[c].push_back({vertex.u,vertex.v,vertex.page,vertex.flags,
                    vertex.minU,vertex.minV,vertex.maxU,vertex.maxV});
        cache.vertices.clear();
        metadata.baked = true;
    }
    if (includeActors) metadata.actors = AppendActorScenePreview(scene,actors,
        {verified.x,verified.y,verified.z},lights,applyLighting,effectivePoseOptions);
    return metadata;
}

} // namespace questvisual
