#pragma once
#include "portable_lightmap_math.h"
#include "quest_world_surface_stream.h"
#include <cstdint>
#include <string>
#include <vector>

struct PortablePackageTables;
struct PortableActorSnapshot;
namespace QuestVr {
struct StaticLightmapMeshChunk {
    std::int32_t materialSlot{};
    std::vector<LightmapVec3> localPositions;
    std::vector<WorldSurfaceRecord> surfaces;
};
struct StaticLightmapVertex {
    float u{}, v{};
    std::int32_t page{-1};
    std::uint32_t flags{}; // bit 0 = original PF_Unlit
    float minU{}, minV{}, maxU{1.0f}, maxV{1.0f};
};
struct StaticLightmapCache {
    std::uint32_t width{1024u}, height{1024u}, layers{};
    float gainScale{1.0f};
    std::vector<std::uint8_t> rgba;
    std::vector<std::vector<StaticLightmapVertex>> vertices;
    std::string playerStartPath;
    LightmapVec3 unrealOrigin;
    std::size_t modelLightMaps{}, shadowBytes{}, bakedSurfaces{}, unlitVertices{}, noLightmapVertices{};
    std::size_t pixelSamples{}, shadowedMaskSamples{}, visibleMaskSamples{};
    PortableLightmapBakeStats bakeStats;
    float maximumGain{}, maximumQuantizationError{};
};
LightmapVec3 VerifyQuestMapOrigin(const PortablePackageTables& package,
    const std::vector<PortableActorSnapshot>& actors, std::string& playerStartPath);
// Original model + inherited runtime actors + a matched DXQM/DXQS stream.
// Surface-index and plane checks fail closed. Uses true static shadow masks,
// no ambient floor, bounded atlas with duplicated one-pixel gutters.
StaticLightmapCache BuildQuestStaticLightmapCache(const PortablePackageTables& package,
    const std::vector<PortableActorSnapshot>& actors,
    const std::vector<StaticLightmapMeshChunk>& chunks);
}
