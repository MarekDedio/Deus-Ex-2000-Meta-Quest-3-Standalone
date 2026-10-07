#include "Precomp.h"
#include "quest_static_lightmap_cache.h"
#include "portable_model_geometry.h"
#include "portable_unreal_runtime.h"
#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
using QuestVr::LightmapVec3;
using QuestVr::StaticLightmapCache;
using QuestVr::StaticLightmapMeshChunk;
using QuestVr::StaticLightmapVertex;
std::size_t rejectedControls{};
void Require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
void U16(Bytes& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
}
void U32(Bytes& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32u; shift += 8u)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
void U64(Bytes& bytes, std::uint64_t value) {
    U32(bytes, static_cast<std::uint32_t>(value));
    U32(bytes, static_cast<std::uint32_t>(value >> 32u));
}
void Float(Bytes& bytes, float value) {
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(value)); U32(bytes, bits);
}
void Vector(Bytes& bytes, const LightmapVec3& value) {
    Float(bytes, value.x); Float(bytes, value.y); Float(bytes, value.z);
}
void Index(Bytes& bytes, std::int32_t value) {
    const bool negative = value < 0;
    std::uint32_t magnitude = static_cast<std::uint32_t>(negative
        ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((magnitude & 0x3fu) | (negative ? 0x80u : 0u));
    magnitude >>= 6u;
    if (magnitude != 0u) first |= 0x40u;
    bytes.push_back(first);
    while (magnitude != 0u) {
        auto next = static_cast<std::uint8_t>(magnitude & 0x7fu);
        magnitude >>= 7u;
        if (magnitude != 0u) next |= 0x80u;
        bytes.push_back(next);
    }
}
void ExpectFailure(const std::function<void()>& action, const char* description) {
    bool rejected{};
    try { action(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, description); ++rejectedControls;
}
bool Near(float a, float b, float tolerance = 1.0e-6f) { return std::fabs(a-b) <= tolerance; }

struct SyntheticCacheFixture {
    std::filesystem::path directory;
    PortablePackageTables package;
    Bytes payload;
    const LightmapVec3 origin{100.0f,-200.0f,16.0f};
    std::vector<LightmapVec3> points;
    std::vector<PortableActorSnapshot> actors;
    std::vector<StaticLightmapMeshChunk> chunks;

    LightmapVec3 Point(float x, float y, float z = 0.0f) const {
        return {origin.x+x,origin.y+y,origin.z+z};
    }
    LightmapVec3 Local(std::size_t index) const {
        const auto& p = points.at(index);
        constexpr float scale = 1.0f/52.5f;
        return {(p.y-origin.y)*scale,(p.z-origin.z)*scale+1.0f,-(p.x-origin.x)*scale};
    }
    void Bounds(Bytes& bytes) const {
        Vector(bytes,origin); Vector(bytes,Point(320.0f,128.0f,128.0f)); bytes.push_back(1u);
    }
    Bytes Model() const {
        Bytes bytes;
        Index(bytes,0); // End of tagged properties.
        Bounds(bytes); Vector(bytes,origin); Float(bytes,512.0f);
        Index(bytes,3);
        Vector(bytes,{0.0f,0.0f,1.0f}); Vector(bytes,{1.0f,0.0f,0.0f}); Vector(bytes,{0.0f,1.0f,0.0f});
        Index(bytes,static_cast<std::int32_t>(points.size()));
        for (const auto& point : points) Vector(bytes,point);
        Index(bytes,3);
        for (std::int32_t node = 0; node < 3; ++node) {
            Vector(bytes,{0.0f,0.0f,1.0f}); Float(bytes,origin.z);
            U64(bytes,3u); bytes.push_back(0u);
            Index(bytes,node == 0 ? 0 : node == 1 ? 4 : 7);
            Index(bytes,node == 2 ? 3 : node);
            for (unsigned index = 0; index < 5u; ++index) Index(bytes,-1);
            Index(bytes,0); Index(bytes,1);
            bytes.push_back(node == 0 ? 4u : 3u); U32(bytes,0xffffffffu); U32(bytes,0xffffffffu);
        }
        Index(bytes,4);
        for (std::int32_t surface = 0; surface < 4; ++surface) {
            Index(bytes,0); U32(bytes,surface == 1 ? 0x00400000u : 0u);
            Index(bytes,surface == 1 ? 4 : surface == 3 ? 7 : 0);
            Index(bytes,0); Index(bytes,1); Index(bytes,2);
            Index(bytes,surface == 0 || surface == 2 ? 0 : -1);
            Index(bytes,-1); U16(bytes,0); U16(bytes,0); Index(bytes,0);
        }
        Index(bytes,static_cast<std::int32_t>(points.size()));
        for (std::size_t index = 0; index < points.size(); ++index) {
            Index(bytes,static_cast<std::int32_t>(index)); Index(bytes,-1);
        }
        U32(bytes,0); U32(bytes,2);
        Index(bytes,0); U64(bytes,0); U64(bytes,0);
        Index(bytes,5); U64(bytes,0); U64(bytes,0);
        Index(bytes,0); // Polys reference.
        Index(bytes,1); U32(bytes,0);
        Float(bytes,0.0f); Float(bytes,0.0f); Float(bytes,0.0f);
        Index(bytes,3); Index(bytes,3); Float(bytes,32.0f); Float(bytes,32.0f); U32(bytes,0);
        Index(bytes,3); bytes.insert(bytes.end(),{0x07u,0x05u,0x07u});
        Index(bytes,0); Index(bytes,0); Index(bytes,0); // Bounds/hulls/leaves.
        Index(bytes,2); Index(bytes,4); Index(bytes,0);
        U32(bytes,1); U32(bytes,0);
        return bytes;
    }
    Bytes Level() const {
        Bytes bytes;
        Index(bytes,0); U32(bytes,5); U32(bytes,5);
        for (const auto ref : {6,3,4,5,0}) Index(bytes,ref);
        for (unsigned string = 0; string < 4u; ++string) Index(bytes,0);
        Index(bytes,0); U32(bytes,7777); U32(bytes,0); Index(bytes,1);
        Index(bytes,0);
        return bytes;
    }
    Bytes PlayerStart() const {
        Bytes bytes;
        Index(bytes,8); bytes.push_back(0x3au); Index(bytes,9); // Location: struct Vector, 12 bytes.
        Vector(bytes,origin); Index(bytes,0);
        return bytes;
    }
    void AppendTriangle(StaticLightmapMeshChunk& chunk, std::int32_t surface,
                        std::int32_t zone, std::size_t a, std::size_t b, std::size_t c) const {
        for (const auto index : {a,b,c}) {
            chunk.localPositions.push_back(Local(index)); chunk.surfaces.push_back({surface,zone});
        }
    }
    SyntheticCacheFixture() {
        directory = std::filesystem::temp_directory_path() /
            ("deusex-static-atlas-fixture-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(!std::filesystem::exists(directory),"Temporary static atlas fixture collision");
        std::filesystem::create_directory(directory);
        package.sourcePath = (directory / "static-lightmap-fixture.dx").string();
        package.version = 68; package.licenseeMode = 0;
        for (const auto* name : {"None","Engine","Model","Level","PlayerStart","Light","ZoneInfo","LevelInfo",
                                 "Location","Vector","ActualWorld","MyLevel","PlayerStart0","OriginalLight0","ZoneInfo0","LevelInfo0"})
            package.names.push_back({NameString(name),0});
        package.imports.push_back({0,0,0,1});
        for (std::int32_t name = 2; name <= 7; ++name) package.imports.push_back({0,0,-1,name});
        for (std::int32_t object = 0; object < 6; ++object)
            package.exports.push_back({-2-object,0,0,10+object,ObjectFlags{},0,0});
        points = {Point(0,0),Point(128,0),Point(128,128),Point(0,128),
                  Point(160,0),Point(224,0),Point(160,64),Point(256,0),Point(320,0),Point(256,64)};
        const std::vector<Bytes> bodies{Model(),Level(),PlayerStart(),Bytes{0u},Bytes{0u},Bytes{0u}};
        for (std::size_t index = 0; index < bodies.size(); ++index) {
            package.exports[index].ObjOffset = static_cast<std::int32_t>(payload.size());
            package.exports[index].ObjSize = static_cast<std::int32_t>(bodies[index].size());
            payload.insert(payload.end(),bodies[index].begin(),bodies[index].end());
        }
        std::ofstream stream(package.sourcePath,std::ios::binary | std::ios::trunc);
        Require(static_cast<bool>(stream),"Cannot create synthetic static atlas payload");
        stream.write(reinterpret_cast<const char*>(payload.data()),static_cast<std::streamsize>(payload.size()));
        stream.close(); Require(static_cast<bool>(stream),"Cannot finish synthetic static atlas payload");

        PortableActorSnapshot player;
        player.objectPath = "static-lightmap-fixture.PlayerStart0"; player.classPath = "Engine.PlayerStart";
        player.x = origin.x; player.y = origin.y; player.z = origin.z; player.hasLocation = true;
        PortableActorSnapshot light;
        light.objectPath = "static-lightmap-fixture.OriginalLight0"; light.classPath = "Engine.Light";
        const auto location = Point(32.0f,32.0f,128.0f);
        light.x = location.x; light.y = location.y; light.z = location.z;
        light.hasLocation = true; light.light = true; light.lightType = 1; light.lightEffect = 13;
        light.lightBrightness = 64; light.lightRadius = 64;
        PortableActorSnapshot zone;
        zone.objectPath = "static-lightmap-fixture.ZoneInfo0"; zone.classPath = "Engine.ZoneInfo";
        zone.ambientHue = 17; zone.ambientSaturation = 24; zone.ambientBrightness = 32;
        PortableActorSnapshot level;
        level.objectPath = "static-lightmap-fixture.LevelInfo0"; level.classPath = "DeusEx.DeusExLevelInfo";
        level.ambientHue = 155; level.ambientSaturation = 40; level.ambientBrightness = 20;
        actors = {player,light,zone,level};
        StaticLightmapMeshChunk chunk; chunk.materialSlot = 0;
        AppendTriangle(chunk,0,1,0,1,2);
        AppendTriangle(chunk,0,1,0,2,3);
        AppendTriangle(chunk,0,0,0,2,1); // Back face uses LevelInfo ambient.
        AppendTriangle(chunk,1,1,4,5,6);
        AppendTriangle(chunk,3,1,7,8,9);
        chunks.push_back(std::move(chunk));
    }
    ~SyntheticCacheFixture() {
        // Remove only this executable's exact synthetic payload and empty directory.
        std::error_code ignored;
        std::filesystem::remove(package.sourcePath,ignored);
        std::filesystem::remove(directory,ignored);
    }
    StaticLightmapCache Build() const {
        return QuestVr::BuildQuestStaticLightmapCache(package,actors,chunks);
    }
};

std::array<std::uint8_t,4> Pixel(const StaticLightmapCache& cache,
                              std::uint32_t x, std::uint32_t y, std::int32_t page) {
    Require(page >= 0 && static_cast<std::uint32_t>(page) < cache.layers && x < cache.width && y < cache.height,
            "Atlas pixel outside declared dimensions");
    const auto offset = ((static_cast<std::size_t>(page)*cache.height+y)*cache.width+x)*4u;
    return {cache.rgba.at(offset),cache.rgba.at(offset+1u),cache.rgba.at(offset+2u),cache.rgba.at(offset+3u)};
}
struct TileRect { std::uint32_t x{},y{},width{},height{}; };
TileRect Rect(const StaticLightmapCache& cache, const StaticLightmapVertex& vertex) {
    return {static_cast<std::uint32_t>(std::lround(vertex.minU*cache.width-0.5f)),
            static_cast<std::uint32_t>(std::lround(vertex.minV*cache.height-0.5f)),
            static_cast<std::uint32_t>(std::lround((vertex.maxU-vertex.minU)*cache.width))+1u,
            static_cast<std::uint32_t>(std::lround((vertex.maxV-vertex.minV)*cache.height))+1u};
}
void CheckTile(const StaticLightmapCache& cache, const StaticLightmapVertex& vertex,
               const QuestVr::PortableSurfaceLightmap& expected) {
    const auto rect = Rect(cache,vertex);
    Require(rect.width == expected.width && rect.height == expected.height && rect.x > 0 && rect.y > 0,
            "Lightmap interior clamp rectangle is not its exact pixel-center bounds");
    for (std::uint32_t y = 0; y < rect.height; ++y) for (std::uint32_t x = 0; x < rect.width; ++x) {
        const auto actual = Pixel(cache,rect.x+x,rect.y+y,vertex.page);
        const auto& rgb = expected.pixels.at(static_cast<std::size_t>(y)*rect.width+x);
        const float channels[3]{rgb.x,rgb.y,rgb.z};
        for (std::size_t channel = 0; channel < 3u; ++channel)
            Require(actual[channel] == static_cast<std::uint8_t>(std::lround(channels[channel]/cache.gainScale*255.0f)),
                    "Atlas channel differs from source bake plus declared HDR quantization");
        Require(actual[3] == 255u,"Atlas interior alpha not opaque");
    }
    // All four edges and corners duplicate the closest interior sample.
    for (std::uint32_t y = 0; y < rect.height+2u; ++y) for (std::uint32_t x = 0; x < rect.width+2u; ++x) {
        const auto sx = std::clamp(static_cast<int>(x)-1,0,static_cast<int>(rect.width)-1);
        const auto sy = std::clamp(static_cast<int>(y)-1,0,static_cast<int>(rect.height)-1);
        Require(Pixel(cache,rect.x+x-1u,rect.y+y-1u,vertex.page) ==
                Pixel(cache,rect.x+static_cast<std::uint32_t>(sx),rect.y+static_cast<std::uint32_t>(sy),vertex.page),
                "Atlas gutter does not duplicate the edge/corner sample");
    }
}
QuestVr::PortableSurfaceLightmap ReferenceBake(const SyntheticCacheFixture& fixture,
                                              const PortableActorSnapshot& zone) {
    const auto model = LoadPortableRootModel68(fixture.package);
    const auto& actor = fixture.actors[1];
    const QuestVr::PortableStaticLight light{4,true,{actor.x,actor.y,actor.z},actor.yaw,actor.pitch,
        actor.lightType,actor.lightEffect,actor.lightHue,actor.lightSaturation,
        actor.lightBrightness,actor.lightRadius,actor.lightCone};
    QuestVr::PortableSurfaceLightmap expected;
    std::string error;
    Require(QuestVr::BakeStaticSurfaceLightmap(model,0,{zone.ambientHue,zone.ambientSaturation,zone.ambientBrightness},
                {light},expected,error),"Fixture reference bake failed");
    return expected;
}
void TestAtlas(const SyntheticCacheFixture& fixture) {
    const auto cache = fixture.Build();
    Require(cache.playerStartPath == fixture.actors[0].objectPath &&
            cache.unrealOrigin.x == fixture.origin.x && cache.unrealOrigin.y == fixture.origin.y &&
            cache.unrealOrigin.z == fixture.origin.z,"Original PlayerStart identity/origin lost");
    Require(cache.width == 1024u && cache.height == 1024u && cache.layers == 1u &&
            cache.rgba.size() == static_cast<std::size_t>(cache.width)*cache.height*4u,
            "Bounded atlas dimensions/storage incorrect");
    Require(cache.modelLightMaps == 1u && cache.shadowBytes == 3u && cache.bakedSurfaces == 2u &&
            cache.pixelSamples == 18u && cache.unlitVertices == 3u && cache.noLightmapVertices == 3u,
            "Surface/zone bake deduplication or bypass counts incorrect");
    Require(cache.shadowedMaskSamples == 2u && cache.visibleMaskSamples == 16u &&
            cache.bakeStats.listedLights == 2u && cache.bakeStats.addedLights == 2u &&
            cache.bakeStats.unsupportedEffects == 0u && cache.bakeStats.unsupportedTypes == 0u,
            "Ordered original light/mask bake statistics incorrect");
    Require(cache.vertices.size() == 1u && cache.vertices[0].size() == 15u,"Parallel lightmap vertex stream lost");
    const auto& vertices = cache.vertices[0];
    const auto zone = ReferenceBake(fixture,fixture.actors[2]);
    const auto level = ReferenceBake(fixture,fixture.actors[3]);
    CheckTile(cache,vertices[0],zone); CheckTile(cache,vertices[6],level);
    Require(vertices[0].minU == vertices[3].minU && vertices[0].maxU == vertices[3].maxU,
            "Same surface/zone did not reuse its atlas tile");
    Require(vertices[0].minU != vertices[6].minU || vertices[0].minV != vertices[6].minV,
            "Different zone ambient incorrectly reused same tile");
    Require(Pixel(cache,Rect(cache,vertices[0]).x,Rect(cache,vertices[0]).y,vertices[0].page) !=
            Pixel(cache,Rect(cache,vertices[6]).x,Rect(cache,vertices[6]).y,vertices[6].page),
            "Inherited ZoneInfo/LevelInfo ambient did not affect output");
    const auto model = LoadPortableRootModel68(fixture.package);
    const auto rect = Rect(cache,vertices[0]);
    for (std::size_t v = 0; v < 9u; ++v) {
        QuestVr::LightmapUv uv;
        std::string error;
        const auto local = fixture.chunks[0].localPositions[v];
        const LightmapVec3 ue{fixture.origin.x-local.z*52.5f,fixture.origin.y+local.x*52.5f,
                              fixture.origin.z+(local.y-1.0f)*52.5f};
        Require(QuestVr::SurfaceLightmapUv(model,0,ue,uv,error),"Reference source UV failed");
        const auto tile = v < 6u ? rect : Rect(cache,vertices[6]);
        Require(Near(vertices[v].u,(tile.x+uv.u*tile.width)/cache.width) &&
                Near(vertices[v].v,(tile.y+uv.v*tile.height)/cache.height),
                "Atlas UV differs from exact UE-to-Quest source projection");
    }
    Require(vertices[1].u > vertices[1].maxU && vertices[2].v > vertices[2].maxV,
            "Out-of-tile source UV was prematurely clamped at vertices");
    for (std::size_t v = 9; v < 12u; ++v)
        Require(vertices[v].page == -1 && vertices[v].flags == 1u,"Original PF_Unlit was not preserved");
    for (std::size_t v = 12; v < 15u; ++v)
        Require(vertices[v].page == -1 && vertices[v].flags == 0u,"Missing-lightmap fallback was not explicit");
    Require(cache.maximumQuantizationError <= cache.gainScale/510.0f+1.0e-6f,
            "HDR quantization exceeds its half-step error bound");
    Require(Pixel(cache,1000,1000,0)[3] == 0u,"Unused atlas pixels unexpectedly opaque");

    auto actors = fixture.actors;
    auto decoy = actors[0]; decoy.objectPath += "_decoy";
    decoy.x += 777.0f; decoy.y -= 333.0f;
    actors.insert(actors.begin(),decoy);
    const auto ordered = QuestVr::BuildQuestStaticLightmapCache(fixture.package,actors,fixture.chunks);
    Require(ordered.playerStartPath == fixture.actors[0].objectPath &&
            ordered.unrealOrigin.x == fixture.origin.x && ordered.unrealOrigin.y == fixture.origin.y,
            "Runtime-first PlayerStart overrode original serialized Level actor order");
    actors = fixture.actors;
    actors.erase(actors.begin()+2); // A null/unavailable zone falls back to LevelInfo.
    const auto fallback = QuestVr::BuildQuestStaticLightmapCache(fixture.package,actors,fixture.chunks);
    Require(fallback.bakedSurfaces == 1u && fallback.pixelSamples == 9u,
            "Missing ZoneInfo did not use and deduplicate inherited LevelInfo ambient");
    CheckTile(fallback,fallback.vertices[0][0],level);
    auto flatChunks = fixture.chunks; flatChunks[0].materialSlot = -1;
    const auto flat = QuestVr::BuildQuestStaticLightmapCache(fixture.package,fixture.actors,flatChunks);
    Require(flat.bakedSurfaces == 0u && flat.noLightmapVertices == 12u && flat.unlitVertices == 3u,
            "Diagnostic flat material incorrectly baked authored texture illumination");
}
void TestRejectControls(const SyntheticCacheFixture& fixture) {
    const auto rejectChunks = [&](const std::vector<StaticLightmapMeshChunk>& chunks, const char* reason) {
        ExpectFailure([&] { (void)QuestVr::BuildQuestStaticLightmapCache(fixture.package,fixture.actors,chunks); },reason);
    };
    const auto rejectActors = [&](const std::vector<PortableActorSnapshot>& actors, const char* reason) {
        ExpectFailure([&] { (void)QuestVr::BuildQuestStaticLightmapCache(fixture.package,actors,fixture.chunks); },reason);
    };
    rejectChunks({},"Empty mesh stream accepted");
    auto chunks = fixture.chunks;
    chunks[0].localPositions.pop_back(); rejectChunks(chunks,"Mismatched parallel position/surface stream accepted");
    chunks = fixture.chunks; chunks[0].localPositions.pop_back(); chunks[0].surfaces.pop_back();
    rejectChunks(chunks,"Partial triangle accepted");
    chunks = fixture.chunks; chunks[0].materialSlot = 1;
    rejectChunks(chunks,"Unsupported world material slot accepted");
    chunks = fixture.chunks;
    for (std::size_t v = 0; v < 3u; ++v) chunks[0].surfaces[v].surface = 2;
    rejectChunks(chunks,"Wrong coplanar surface association accepted");
    chunks = fixture.chunks;
    for (std::size_t v = 0; v < 3u; ++v) chunks[0].surfaces[v].zone = 0;
    rejectChunks(chunks,"Wrong front/back zone association accepted");
    chunks = fixture.chunks; chunks[0].surfaces[1].zone = 0;
    rejectChunks(chunks,"Mixed triangle zones accepted");
    chunks = fixture.chunks; chunks[0].surfaces[1].surface = 1;
    rejectChunks(chunks,"Mixed triangle surfaces accepted");
    chunks = fixture.chunks; chunks[0].localPositions[0] = fixture.Local(1);
    chunks[0].localPositions[1] = fixture.Local(2); chunks[0].localPositions[2] = fixture.Local(3);
    rejectChunks(chunks,"Non-authored quad fan accepted");
    chunks = fixture.chunks; std::swap(chunks[0].localPositions[1],chunks[0].localPositions[2]);
    rejectChunks(chunks,"Wrong triangle winding with original front zone accepted");
    chunks = fixture.chunks; chunks[0].localPositions[0].x += 0.01f;
    rejectChunks(chunks,"Coplanar non-authored point accepted");
    chunks = fixture.chunks; chunks[0].localPositions[0].y += 1.0f;
    rejectChunks(chunks,"Off-plane world cache position accepted");
    for (const float invalid : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        chunks = fixture.chunks; chunks[0].localPositions[0].x = invalid;
        rejectChunks(chunks,"Non-finite position accepted");
    }
    for (const std::int32_t surface : {-1,4}) {
        chunks = fixture.chunks; chunks[0].surfaces[0].surface = surface;
        rejectChunks(chunks,"Out-of-range surface index accepted");
    }
    for (const std::int32_t zone : {-1,64}) {
        chunks = fixture.chunks; chunks[0].surfaces[0].zone = zone;
        rejectChunks(chunks,"Out-of-range zone index accepted");
    }
    auto actors = fixture.actors; actors[0].x += 1.0f;
    rejectActors(actors,"Runtime and authored PlayerStart origin mismatch accepted");
    actors = fixture.actors; actors[0].hasLocation = false;
    rejectActors(actors,"PlayerStart missing location accepted");
    actors = fixture.actors; actors.erase(actors.begin());
    rejectActors(actors,"Missing PlayerStart snapshot accepted");
    actors = fixture.actors; actors.pop_back();
    rejectActors(actors,"Missing LevelInfo ambient fallback accepted");
    actors = fixture.actors; actors.erase(actors.begin()+1);
    rejectActors(actors,"Missing ordered static light snapshot accepted");
    actors = fixture.actors; actors[1].hasLocation = false;
    rejectActors(actors,"Ordered static light missing location accepted");
    actors = fixture.actors; actors[1].light = false;
    rejectActors(actors,"Ordered shadow actor not marked as light accepted");
    actors = fixture.actors; actors.push_back(actors[2]);
    rejectActors(actors,"Duplicate runtime actor identity accepted");
}
}

int main() {
    try {
        const SyntheticCacheFixture fixture;
        TestAtlas(fixture);
        TestRejectControls(fixture);
        std::cout << "Static lightmap cache: exact source triangle/zone associations, atlas UV/clamp/gutters, "
                     "inherited ambient, unlit/fallback bypass, and " << rejectedControls << " rejection controls passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Static lightmap cache test failed: " << error.what() << '\n';
        return 1;
    }
}
