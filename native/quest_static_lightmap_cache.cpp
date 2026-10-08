#include "quest_static_lightmap_cache.h"
#include "portable_model_geometry.h"
#include "portable_unreal_runtime.h"
#include "surreal_portable_package_tables.h"
#include <algorithm>
#include <cstdio>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <unordered_map>

namespace QuestVr {
namespace {
constexpr std::size_t kMaximumPixels = 8u * 1024u * 1024u;
constexpr std::size_t kMaximumOperations = 512u * 1024u * 1024u;
constexpr std::size_t kMaximumPages = 16u; // 64 MiB RGBA8 on CPU/GPU
constexpr std::uint32_t kUnlit = 0x00400000u;
LightmapVec3 FindOrigin(const PortablePackageTables& package,
    const std::vector<PortableActorSnapshot>& actors, std::string& path) {
    const std::string map = std::filesystem::path(package.sourcePath).stem().string();
    for (const auto ref : ReadPortableLevel68ActorOrder(package)) {
        if (ref <= 0) continue;
        const auto& entry = package.exports.at(static_cast<std::size_t>(ref - 1));
        const auto object = GetPortableObjectPath(package,ref);
        const auto cls = GetPortableObjectPath(package,entry.ObjClass);
        if (object.find("PlayerStart") == std::string::npos &&
            cls.find("PlayerStart") == std::string::npos) continue;
        const auto properties = LoadPortableExportProperties(package,static_cast<std::size_t>(ref - 1));
        for (const auto& property : properties.properties) {
            if (property.name != "Location" || property.type != 10u || property.value.size() != 12u) continue;
            LightmapVec3 origin;
            std::memcpy(&origin,property.value.data(),12u);
            if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z))
                throw std::runtime_error("Non-finite cache PlayerStart origin");
            path = map + "." + object;
            const auto actor = std::find_if(actors.begin(),actors.end(),[&](const auto& value) {
                return value.objectPath == path;
            });
            if (actor == actors.end() || !actor->hasLocation || actor->x != origin.x ||
                actor->y != origin.y || actor->z != origin.z)
                throw std::runtime_error("Static lightmap and cache PlayerStart origins disagree");
            return origin;
        }
        // Match cache FindPlayerStart: a candidate without Location is skipped.
    }
    throw std::runtime_error("Static lightmaps require a verified Level-ordered PlayerStart origin");
}
void AddStats(PortableLightmapBakeStats& target, const PortableLightmapBakeStats& source) {
    target.listedLights += source.listedLights; target.addedLights += source.addedLights;
    target.disabledLights += source.disabledLights; target.missingLights += source.missingLights;
    target.unsupportedTypes += source.unsupportedTypes; target.unsupportedEffects += source.unsupportedEffects;
    target.coincidentSamples += source.coincidentSamples;
    for (std::size_t i = 0; i < 256u; ++i) {
        target.unsupportedTypeCounts[i] += source.unsupportedTypeCounts[i];
        target.unsupportedEffectCounts[i] += source.unsupportedEffectCounts[i];
    }
}
struct Tile {
    std::size_t surface{};
    PortableSurfaceLightmap bake;
    std::uint32_t x{}, y{}, page{};
};
using TriangleKey = std::array<std::uint32_t,11>;
TriangleKey MakeTriangleKey(std::int32_t surface, std::int32_t zone,
    const LightmapVec3& a, const LightmapVec3& b, const LightmapVec3& c) {
    TriangleKey key{}; key[0] = static_cast<std::uint32_t>(surface); key[1] = static_cast<std::uint32_t>(zone);
    const float xyz[9]{a.x,a.y,a.z,b.x,b.y,b.z,c.x,c.y,c.z};
    for (std::size_t i = 0; i < 9u; ++i) {
        if (!std::isfinite(xyz[i])) throw std::runtime_error("Non-finite world/lightmap triangle position");
        std::memcpy(&key[i+2u],&xyz[i],4u);
    }
    return key;
}
std::vector<TriangleKey> OriginalCacheTriangles(const PortableModelGeometry& model, const LightmapVec3& origin) {
    std::vector<TriangleKey> keys;
    const auto local = [&](std::int32_t index) {
        const auto& p = model.points.at(static_cast<std::size_t>(index));
        constexpr float scale = 1.0f/52.5f;
        return LightmapVec3{(p.y-origin.y)*scale,(p.z-origin.z)*scale+1.0f,-(p.x-origin.x)*scale};
    };
    for (const auto& node : model.nodes) {
        if (node.vertexCount < 3u || node.surface < 0 || node.vertexPool < 0) continue;
        const auto& surface = model.surfaces.at(static_cast<std::size_t>(node.surface));
        if (surface.polyFlags & (0x1u|0x04000000u)) continue;
        const auto a = local(model.vertices.at(static_cast<std::size_t>(node.vertexPool)).point);
        for (std::size_t i = 1; i+1u < node.vertexCount; ++i) {
            const auto b = local(model.vertices.at(static_cast<std::size_t>(node.vertexPool)+i).point);
            const auto c = local(model.vertices.at(static_cast<std::size_t>(node.vertexPool)+i+1u).point);
            const float bx=b.x-a.x, by=b.y-a.y, bz=b.z-a.z, cx=c.x-a.x, cy=c.y-a.y, cz=c.z-a.z;
            const float nx=by*cz-bz*cy, ny=bz*cx-bx*cz, nz=bx*cy-by*cx;
            if (nx*nx+ny*ny+nz*nz < 1.0e-12f) continue;
            if (keys.size() >= 8u*1024u*1024u/3u) throw std::runtime_error("Lightmap source triangle budget exceeded");
            keys.push_back(MakeTriangleKey(node.surface,node.zone1,a,b,c));
            keys.push_back(MakeTriangleKey(node.surface,node.zone0,a,c,b));
        }
    }
    std::sort(keys.begin(),keys.end());
    keys.erase(std::unique(keys.begin(),keys.end()),keys.end());
    return keys;
}
}
LightmapVec3 VerifyQuestMapOrigin(const PortablePackageTables& package,
    const std::vector<PortableActorSnapshot>& actors, std::string& playerStartPath) {
    return FindOrigin(package,actors,playerStartPath);
}
StaticLightmapCache BuildQuestStaticLightmapCache(const PortablePackageTables& package,
    const std::vector<PortableActorSnapshot>& actors,
    const std::vector<StaticLightmapMeshChunk>& chunks) {
    if (chunks.empty() || chunks.size() > 2048u) throw std::runtime_error("Invalid lightmap mesh chunks");
    const auto model = LoadPortableRootModel68(package);
    StaticLightmapCache output;
    output.unrealOrigin = FindOrigin(package,actors,output.playerStartPath);
    output.modelLightMaps = model.lightMaps.size(); output.shadowBytes = model.lightBits.size();
    const auto sourceTriangles = OriginalCacheTriangles(model,output.unrealOrigin);
    std::unordered_map<std::string,const PortableActorSnapshot*> actorByPath;
    const PortableActorSnapshot* levelInfo = nullptr;
    for (const auto& actor : actors) {
        if (!actorByPath.emplace(actor.objectPath,&actor).second)
            throw std::runtime_error("Duplicate runtime actor path in static lightmap input");
        const auto leaf = actor.classPath.substr(actor.classPath.find_last_of('.') + 1u);
        if (leaf == "LevelInfo" || leaf == "DeusExLevelInfo") levelInfo = &actor;
    }
    if (!levelInfo) throw std::runtime_error("Static lightmap ZoneInfo fallback has no LevelInfo");
    const std::string map = std::filesystem::path(package.sourcePath).stem().string();
    const auto actorFor = [&](std::int32_t reference) -> const PortableActorSnapshot* {
        if (reference == 0) return nullptr;
        const std::string object = GetPortableObjectPath(package,reference);
        const auto found = actorByPath.find(reference > 0 ? map + "." + object : object);
        return found == actorByPath.end() ? nullptr : found->second;
    };
    std::map<std::pair<std::size_t,std::uint32_t>,std::size_t> tileByKey;
    std::vector<Tile> tiles;
    std::vector<std::vector<std::size_t>> tileIndexes(chunks.size());
    output.vertices.resize(chunks.size());
    std::size_t totalVertices{}, operations{};
    for (std::size_t c = 0; c < chunks.size(); ++c) {
        const auto& chunk = chunks[c];
        if (chunk.localPositions.size() != chunk.surfaces.size() || chunk.surfaces.empty() ||
            chunk.surfaces.size() % 3u != 0u || chunk.surfaces.size() > 60000u ||
            (chunk.materialSlot != 0 && chunk.materialSlot != -1))
            throw std::runtime_error("World/lightmap surface stream mismatch");
        totalVertices += chunk.surfaces.size();
        if (totalVertices > 8u * 1024u * 1024u) throw std::runtime_error("Lightmap vertex budget exceeded");
        output.vertices[c].resize(chunk.surfaces.size());
        tileIndexes[c].assign(chunk.surfaces.size(),static_cast<std::size_t>(-1));
        for (std::size_t v = 0; v < chunk.surfaces.size(); ++v) {
            const auto& record = chunk.surfaces[v];
            if (record.surface < 0 || static_cast<std::size_t>(record.surface) >= model.surfaces.size() ||
                record.zone < 0 || record.zone > 63 ||
                (v % 3u != 0u && (record.surface != chunk.surfaces[v-v%3u].surface ||
                    record.zone != chunk.surfaces[v-v%3u].zone)))
                throw std::runtime_error("Invalid lightmap surface/zone association");
            auto& vertex = output.vertices[c][v];
            if (v % 3u == 0u && !std::binary_search(sourceTriangles.begin(),sourceTriangles.end(),
                MakeTriangleKey(record.surface,record.zone,chunk.localPositions[v],chunk.localPositions[v+1u],chunk.localPositions[v+2u])))
                throw std::runtime_error("World/lightmap triangle is not an exact authored BSP fan/zone association");
            const auto& surface = model.surfaces[static_cast<std::size_t>(record.surface)];
            if (surface.polyFlags & kUnlit) { vertex.flags = 1u; ++output.unlitVertices; continue; }
            if (chunk.materialSlot != 0 || surface.lightMap < 0) { ++output.noLightmapVertices; continue; }
            const auto* zone = static_cast<std::size_t>(record.zone) < model.zones.size()
                ? actorFor(model.zones[static_cast<std::size_t>(record.zone)].actorReference) : nullptr;
            if (!zone) zone = levelInfo;
            const PortableLightmapZoneAmbient ambient{zone->ambientHue,zone->ambientSaturation,zone->ambientBrightness};
            const std::uint32_t ambientKey = ambient.hue | (static_cast<std::uint32_t>(ambient.saturation) << 8u) |
                (static_cast<std::uint32_t>(ambient.brightness) << 16u);
            const auto key = std::make_pair(static_cast<std::size_t>(record.surface),ambientKey);
            auto found = tileByKey.find(key);
            if (found == tileByKey.end()) {
                const auto& lm = model.lightMaps.at(static_cast<std::size_t>(surface.lightMap));
                const auto count = GetPortableModelStaticLightCount(model,static_cast<std::size_t>(surface.lightMap));
                const auto pixels = static_cast<std::size_t>(lm.uClamp) * static_cast<std::size_t>(lm.vClamp);
                if (output.pixelSamples + pixels > kMaximumPixels || count > kMaximumOperations ||
                    pixels > (kMaximumOperations-operations)/std::max<std::size_t>(count,1u))
                    throw std::runtime_error("Map static lightmap work/memory budget exceeded");
                operations += pixels * std::max<std::size_t>(count,1u);
                std::vector<PortableStaticLight> orderedLights;
                orderedLights.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    const auto ref = model.lights.at(static_cast<std::size_t>(lm.lightActors) + i);
                    const auto* actor = actorFor(ref);
                    if (!actor || !actor->hasLocation || !actor->light)
                        throw std::runtime_error("Static shadow list actor missing location: " + GetPortableObjectPath(package,ref));
                    orderedLights.push_back({ref,true,{actor->x,actor->y,actor->z},actor->yaw,actor->pitch,
                        actor->lightType,actor->lightEffect,actor->lightHue,actor->lightSaturation,
                        actor->lightBrightness,actor->lightRadius,actor->lightCone});
                    const auto span = GetPortableModelShadowSpan(model,static_cast<std::size_t>(surface.lightMap),i);
                    for (std::size_t y = 0; y < span.height; ++y) for (std::size_t x = 0; x < span.width; ++x) {
                        const bool visible = (model.lightBits[span.offset+y*span.pitch+x/8u] & (1u << (x%8u))) != 0u;
                        if (visible) ++output.visibleMaskSamples; else ++output.shadowedMaskSamples;
                    }
                }
                Tile tile; tile.surface = key.first;
                std::string error;
                if (!BakeStaticSurfaceLightmap(model,key.first,ambient,orderedLights,tile.bake,error))
                    throw std::runtime_error("Static surface lightmap failed: " + error);
                if (tile.bake.width + 2u > output.width || tile.bake.height + 2u > output.height)
                    throw std::runtime_error("Authored lightmap exceeds bounded atlas page");
                for (const auto& p : tile.bake.pixels) {
                    const float peak = std::max({p.x,p.y,p.z});
                    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
                        std::min({p.x,p.y,p.z}) < 0.0f || peak > 16.0f) {
                        char diagnostic[256];
                        std::snprintf(diagnostic, sizeof(diagnostic),
                            "Static lightmap gain exceeds supported finite HDR range: surface=%zu rgb=(%.9g,%.9g,%.9g)",
                            key.first, static_cast<double>(p.x), static_cast<double>(p.y), static_cast<double>(p.z));
                        throw std::runtime_error(diagnostic);
                    }
                    output.maximumGain = std::max(output.maximumGain,peak);
                }
                output.pixelSamples += pixels; AddStats(output.bakeStats,tile.bake.stats);
                found = tileByKey.emplace(key,tiles.size()).first;
                tiles.emplace_back(std::move(tile));
            }
            tileIndexes[c][v] = found->second;
            const auto& local = chunk.localPositions[v];
            const LightmapVec3 ue{output.unrealOrigin.x-local.z*52.5f,
                output.unrealOrigin.y+local.x*52.5f,output.unrealOrigin.z+(local.y-1.0f)*52.5f};
            if (!std::isfinite(ue.x) || !std::isfinite(ue.y) || !std::isfinite(ue.z))
                throw std::runtime_error("Non-finite lightmap mesh position");
            const auto& normal = model.vectors.at(static_cast<std::size_t>(surface.normalVector));
            const auto& base = model.points.at(static_cast<std::size_t>(surface.basePoint));
            const double norm = std::sqrt(static_cast<double>(normal.x)*normal.x+static_cast<double>(normal.y)*normal.y+static_cast<double>(normal.z)*normal.z);
            const double distance = (static_cast<double>(ue.x)-base.x)*normal.x+(static_cast<double>(ue.y)-base.y)*normal.y+(static_cast<double>(ue.z)-base.z)*normal.z;
            if (!std::isfinite(norm) || !std::isfinite(distance) || norm <= 0.0 || std::fabs(distance) > norm*0.5)
                throw std::runtime_error("World cache vertex does not belong to authored lightmap plane");
            std::string error;
            LightmapUv uv;
            if (!SurfaceLightmapUv(model,key.first,ue,uv,error))
                throw std::runtime_error("Lightmap UV mapping failed: " + error);
            vertex.u = uv.u; vertex.v = uv.v;
        }
    }
    output.bakedSurfaces = tiles.size();
    while (output.gainScale < output.maximumGain) output.gainScale *= 2.0f;
    std::vector<std::size_t> order(tiles.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(),order.end(),[&](auto a, auto b) { return tiles[a].bake.height > tiles[b].bake.height; });
    std::uint32_t x{}, y{}, rowHeight{}, page{};
    for (const auto index : order) {
        auto& tile = tiles[index]; const auto w = tile.bake.width+2u, h = tile.bake.height+2u;
        if (x+w > output.width) { x = 0u; y += rowHeight; rowHeight = 0u; }
        if (y+h > output.height) { ++page; x = y = rowHeight = 0u; }
        if (page >= kMaximumPages) throw std::runtime_error("Static lightmap atlas memory budget exceeded");
        tile.x = x+1u; tile.y = y+1u; tile.page = page; x += w; rowHeight = std::max(rowHeight,h);
    }
    output.layers = page+1u;
    output.rgba.assign(static_cast<std::size_t>(output.width)*output.height*output.layers*4u,0u);
    for (const auto& tile : tiles) {
        for (std::uint32_t py = 0; py < tile.bake.height+2u; ++py) for (std::uint32_t px = 0; px < tile.bake.width+2u; ++px) {
            const auto sx = std::clamp(static_cast<int>(px)-1,0,static_cast<int>(tile.bake.width)-1);
            const auto sy = std::clamp(static_cast<int>(py)-1,0,static_cast<int>(tile.bake.height)-1);
            const auto& rgb = tile.bake.pixels[static_cast<std::size_t>(sy)*tile.bake.width+static_cast<std::size_t>(sx)];
            const float channels[3]{rgb.x,rgb.y,rgb.z};
            const auto target = ((static_cast<std::size_t>(tile.page)*output.height+tile.y+py-1u)*output.width+tile.x+px-1u)*4u;
            for (std::size_t channel = 0; channel < 3u; ++channel) {
                const auto value = static_cast<std::uint8_t>(std::lround(channels[channel]/output.gainScale*255.0f));
                output.rgba[target+channel] = value;
                output.maximumQuantizationError = std::max(output.maximumQuantizationError,
                    std::fabs(channels[channel]-static_cast<float>(value)*output.gainScale/255.0f));
            }
            output.rgba[target+3u] = 255u;
        }
    }
    for (std::size_t c = 0; c < chunks.size(); ++c) for (std::size_t v = 0; v < chunks[c].surfaces.size(); ++v) {
        const auto index = tileIndexes[c][v]; if (index == static_cast<std::size_t>(-1)) continue;
        const auto& tile = tiles[index]; auto& vertex = output.vertices[c][v];
        // Preserve the raw coordinates through interpolation; tile clamping
        // must occur per fragment, not at polygon vertices.
        vertex.u = (tile.x+vertex.u*tile.bake.width)/output.width;
        vertex.v = (tile.y+vertex.v*tile.bake.height)/output.height;
        vertex.minU = (tile.x+0.5f)/output.width; vertex.minV = (tile.y+0.5f)/output.height;
        vertex.maxU = (tile.x+tile.bake.width-0.5f)/output.width;
        vertex.maxV = (tile.y+tile.bake.height-0.5f)/output.height;
        vertex.page = static_cast<std::int32_t>(tile.page);
    }
    return output;
}
}
