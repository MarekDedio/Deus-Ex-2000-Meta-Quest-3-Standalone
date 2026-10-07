#include "visual_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace questvisual;
void Require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
Camera Origin() { Camera camera; camera.position = {}; return camera; }
Scene Triangle() {
    Scene scene;
    scene.textureWidth = scene.textureHeight = scene.textureLayers = 1u;
    scene.textures = {255,255,255,255};
    scene.chunks = {{0, {
        {{-1,-1,-1},{0,0,1},0,0,0},
        {{3,-3,-3},{0,0,1},0,0,0},
        {{0,3,-3},{0,0,1},0,0,0}}}};
    return scene;
}
std::array<std::uint8_t,3> Center(const RenderResult& image) {
    const auto index = ((image.image.height/2u)*image.image.width + image.image.width/2u)*3u;
    return {image.image.rgb[index], image.image.rgb[index+1u], image.image.rgb[index+2u]};
}
void ConstantLightmap(Scene& scene, std::array<std::uint8_t,3> gain) {
    scene.lightmapWidth = scene.lightmapHeight = scene.lightmapLayers = 1u;
    scene.lightmapRgba = {gain[0],gain[1],gain[2],255u};
    scene.lightmapVertices.resize(scene.chunks.size());
    for (std::size_t i = 0; i < scene.chunks.size(); ++i)
        scene.lightmapVertices[i].assign(scene.chunks[i].vertices.size(), {0.5f,0.5f,0,0});
}

void AlbedoFallbackAndUnlit() {
    auto scene = MakeSyntheticScene();
    const Camera camera;
    const auto albedo = Render(scene,camera,256,256);
    scene.lightmapVertices.resize(scene.chunks.size());
    for (std::size_t i = 0; i < scene.chunks.size(); ++i)
        scene.lightmapVertices[i].assign(scene.chunks[i].vertices.size(), {0,0,-1,0});
    const auto absent = Render(scene,camera,256,256);
    Require(absent.image.rgb == albedo.image.rgb && absent.depth == albedo.depth && absent.frameHash == albedo.frameHash,
            "Absent baked pages changed exact default albedo output");
    scene.vertexLighting.resize(scene.chunks.size());
    for (std::size_t i = 0; i < scene.chunks.size(); ++i)
        scene.vertexLighting[i].assign(scene.chunks[i].vertices.size(), {.5f,.25f,.75f});
    const auto fallback = Render(scene,camera,256,256);
    auto directOnly = scene;
    directOnly.lightmapVertices.clear();
    Require(fallback.image.rgb == Render(directOnly,camera,256,256).image.rgb,
            "No-baked-page fallback changed existing vertex-light gains");
    for (auto& chunk : scene.lightmapVertices)
        for (auto& map : chunk) map.flags = kLightmapUnlit;
    Require(Render(scene,camera,256,256).image.rgb == albedo.image.rgb,
            "Unlit surface was affected by fallback vertex lighting");
    ConstantLightmap(scene,{0,0,0});
    for (auto& chunk : scene.lightmapVertices)
        for (auto& map : chunk) map.flags = kLightmapUnlit;
    scene.lightmapGainScale = 4.0f;
    Require(Render(scene,camera,256,256).image.rgb == albedo.image.rgb,
            "Unlit surface was affected by a black baked lightmap or gain scale");
    scene.lightmapGainScale = 1.0f;
    ConstantLightmap(scene,{255,255,255});
    const auto white = Render(scene,camera,256,256);
    Require(white.image.rgb == albedo.image.rgb && white.depth == albedo.depth &&
            white.clippedTriangles == albedo.clippedTriangles,
            "Unit baked lighting altered albedo/depth/near-plane clipping or multiplied fallback gains");
}

void ColorBilinearClampPerspective() {
    auto scene = Triangle();
    ConstantLightmap(scene,{51,102,153});
    auto colored = Render(scene,Origin(),256,256);
    Require(Center(colored) == std::array<std::uint8_t,3>{51,102,153}, "Baked RGB channels were lost or swapped");
    // Four texel centers: bilinear center must be their exact RGB average.
    scene.lightmapWidth = scene.lightmapHeight = 2u;
    scene.lightmapRgba = {0,0,0,255, 200,0,0,255, 0,100,0,255, 200,100,80,255};
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{100,50,20},
            "Baked lightmap bilinear sampling or normalized texel centers differ");
    for (auto& vertex : scene.lightmapVertices[0]) { vertex.u = -1000; vertex.v = -1000; }
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{0,0,0},
            "Negative lightmap coordinates repeat instead of clamp to first texel");
    for (auto& vertex : scene.lightmapVertices[0]) { vertex.u = 1000; vertex.v = 1000; }
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{200,100,80},
            "Large lightmap coordinates repeat instead of clamp to last texel");
    for (auto& vertex : scene.lightmapVertices[0]) { vertex.u = .25f; vertex.v = .75f; }
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{0,100,0},
            "Exact lightmap texel-center coordinates shifted their sample");

    // This UV gradient on oblique geometry is about .17 at the center, while
    // affine interpolation is about .25. Atlas centers at u=.25/.75 encode
    // red 0/255, so correct perspective sampling is about 43 (not about 65).
    scene.lightmapWidth = 2; scene.lightmapHeight = 1;
    scene.lightmapRgba = {0,255,128,255, 255,255,128,255};
    scene.lightmapVertices = {{{.25f,.5f,0,0},{.75f,.5f,0,0},{.25f,.5f,0,0}}};
    const auto perspective = Center(Render(scene,Origin(),256,256));
    Require(perspective[0] >= 41u && perspective[0] <= 45u && perspective[1] == 255u && perspective[2] == 128u,
            "Lightmap UV interpolation is not perspective correct");
    // Raw endpoints extend outside their own tile. At the center the proper
    // interpolated U is negative and clamps to the first texel (red 0).
    // Endpoint-clamped interpolation instead produces interior red about 43.
    scene.lightmapVertices = {{{-.5f,.5f,0,0,.25f,.5f,.75f,.5f},
                              {1.5f,.5f,0,0,.25f,.5f,.75f,.5f},
                              {-.5f,.5f,0,0,.25f,.5f,.75f,.5f}}};
    Require(Center(Render(scene,Origin(),256,256))[0] == 0u,
            "Packed tile clamp happened before perspective interpolation");
    auto endpointClamped = scene;
    for (auto& map : endpointClamped.lightmapVertices[0]) map.u = std::clamp(map.u,map.minU,map.maxU);
    const auto distortedRed = Center(Render(endpointClamped,Origin(),256,256))[0];
    Require(distortedRed >= 41u && distortedRed <= 45u,
            "Tile-clamp regression fixture does not distinguish endpoint clamping");
    scene.textureWidth = scene.textureHeight = scene.textureLayers = 1;
    scene.textures = {100,200,50,255};
    ConstantLightmap(scene,{128,64,255});
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{50,50,50},
            "Baked lightmap was not multiplied by original material RGB");
    scene.lightmapGainScale = 2.0f;
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{100,100,100},
            "Normalized baked lightmap gain scale did not restore HDR material gain");
    scene.textures[3] = 0;
    const auto transparent = Render(scene,Origin(),256,256);
    Require(transparent.coveredPixels == 0 && transparent.transparentSamples > 0,
            "Baked-lit masked material wrote opaque pixels or depth");
}

void LayersNearClipAndValidation() {
    auto scene = Triangle();
    ConstantLightmap(scene,{0,0,0});
    scene.lightmapLayers = 2;
    scene.lightmapRgba.insert(scene.lightmapRgba.end(),{20,40,60,255});
    for (auto& map : scene.lightmapVertices[0]) map.page = 1;
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{20,40,60},
            "Baked lightmap page selection sampled the wrong atlas layer");
    auto clipped = MakeSyntheticScene();
    const auto before = Render(clipped,Camera{},256,256);
    ConstantLightmap(clipped,{255,255,255});
    const auto after = Render(clipped,Camera{},256,256);
    Require(before.clippedTriangles > 0 && after.clippedTriangles == before.clippedTriangles &&
            before.image.rgb == after.image.rgb && before.depth == after.depth,
            "Near-plane lightmap clipping changed material coverage/depth/unit gain");
    const auto reject = [](const Scene& invalid) {
        try { (void)Render(invalid,Origin(),64,64); }
        catch (const std::runtime_error&) { return true; }
        return false;
    };
    auto invalid = scene; invalid.lightmapVertices.push_back({});
    Require(reject(invalid), "Extra lightmap chunk accepted");
    invalid = scene; invalid.lightmapVertices[0].pop_back();
    Require(reject(invalid), "Missing lightmap vertex accepted");
    invalid = scene; invalid.lightmapVertices[0][0].u = std::numeric_limits<float>::quiet_NaN();
    Require(reject(invalid), "Nonfinite lightmap UV accepted");
    invalid = scene; invalid.lightmapVertices[0][0].flags = 2;
    Require(reject(invalid), "Unknown lightmap flags accepted");
    invalid = scene; invalid.lightmapVertices[0][0].page = -2;
    Require(reject(invalid), "Invalid negative lightmap page accepted");
    invalid = scene; invalid.lightmapVertices[0][0].page = 2;
    Require(reject(invalid), "Out-of-range lightmap page accepted");
    invalid = scene; invalid.lightmapVertices[0][0].page = 0;
    Require(reject(invalid), "Mixed lightmap triangle pages accepted");
    invalid = scene; invalid.lightmapVertices[0][0].flags = kLightmapUnlit;
    Require(reject(invalid), "Mixed unlit triangle flags accepted");
    invalid = scene; invalid.lightmapRgba.pop_back();
    Require(reject(invalid), "Truncated lightmap atlas accepted");
    invalid = scene; invalid.lightmapWidth = 4097;
    Require(reject(invalid), "Oversized lightmap atlas dimensions accepted");
    invalid = scene; invalid.lightmapVertices.clear();
    Require(reject(invalid), "Lightmap atlas without parallel UVs accepted");
    invalid = scene; invalid.lightmapRgba.clear(); invalid.lightmapWidth = invalid.lightmapHeight = invalid.lightmapLayers = 0;
    Require(reject(invalid), "Lightmap page references without an atlas accepted");
    invalid = scene; invalid.lightmapGainScale = std::numeric_limits<float>::quiet_NaN();
    Require(reject(invalid), "Nonfinite lightmap gain scale accepted");
    invalid = scene; invalid.lightmapGainScale = 0;
    Require(reject(invalid), "Zero lightmap gain scale accepted");
    invalid = scene; invalid.lightmapGainScale = -1;
    Require(reject(invalid), "Negative lightmap gain scale accepted");
    invalid = scene; invalid.lightmapGainScale = 17;
    Require(reject(invalid), "Oversized lightmap gain scale accepted");
    invalid = scene; invalid.lightmapVertices[0][0].minU = std::numeric_limits<float>::quiet_NaN();
    Require(reject(invalid), "Nonfinite tile bound accepted");
    invalid = scene; invalid.lightmapVertices[0][0].minV = -.1f;
    Require(reject(invalid), "Negative tile bound accepted");
    invalid = scene; invalid.lightmapVertices[0][0].maxU = 1.1f;
    Require(reject(invalid), "Out-of-atlas tile bound accepted");
    invalid = scene; invalid.lightmapVertices[0][0].minU = .75f; invalid.lightmapVertices[0][0].maxU = .25f;
    Require(reject(invalid), "Reversed tile bounds accepted");
    invalid = scene; invalid.lightmapVertices[0][0].maxU = .75f;
    Require(reject(invalid), "Mixed pertriangle tile bounds accepted");
}
} // namespace

int main() {
    try {
        AlbedoFallbackAndUnlit();
        ColorBilinearClampPerspective();
        LayersNearClipAndValidation();
        std::cout << "Synthetic baked-lightmap raster tests passed: albedo byte equality, fallback/unlit, RGB/material multiply, "
                  << "clamped bilinear/page sampling, perspective/near clipping, masking and malformed streams.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Baked-lightmap renderer test failed: " << error.what() << '\n';
        return 1;
    }
}
