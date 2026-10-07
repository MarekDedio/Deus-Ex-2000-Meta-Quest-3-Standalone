#include "visual_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace questvisual;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
Camera Origin() { Camera camera; camera.position = {}; return camera; }
std::array<std::uint8_t,3> Center(const RenderResult& result) {
    const auto offset = ((result.image.height/2u)*result.image.width+result.image.width/2u)*3u;
    return {result.image.rgb[offset],result.image.rgb[offset+1u],result.image.rgb[offset+2u]};
}
float CenterDepth(const RenderResult& result) {
    return result.depth[(result.image.height/2u)*result.image.width+result.image.width/2u];
}
Chunk Triangle(TextureBank bank, float depth = 2.0f, std::int32_t layer = 0) {
    return {0,{
        {{-depth,-depth,-depth},{0,0,1},.5f,.5f,layer},
        {{depth,-depth,-depth},{0,0,1},.5f,.5f,layer},
        {{0,depth,-depth},{0,0,1},.5f,.5f,layer}},bank,
        bank == TextureBank::Actor ? kPolyMasked : 0u};
}
void ConstantActorTexture(Scene& scene, std::array<std::uint8_t,4> rgba) {
    // Same array extent as the shared actor preparation path, not a renumbered
    // world material. Uniform texels make sample identity exact at every UV.
    scene.actorTextureWidth = scene.actorTextureHeight = 96u;
    scene.actorTextureLayers = 1u;
    scene.actorTextures.resize(96u*96u*4u);
    for (std::size_t offset = 0; offset < scene.actorTextures.size(); offset += 4u)
        std::copy(rgba.begin(),rgba.end(),scene.actorTextures.begin()+offset);
}
Scene TwoBanks() {
    Scene scene;
    scene.textureWidth = scene.textureHeight = scene.textureLayers = 1u;
    scene.textures = {17,33,201,255};
    ConstantActorTexture(scene,{211,47,23,255});
    scene.chunks = {Triangle(TextureBank::World,4),Triangle(TextureBank::Actor,2)};
    return scene;
}
bool Rejects(const Scene& scene) {
    try { (void)Render(scene,Origin(),64,64); }
    catch (const std::runtime_error&) { return true; }
    return false;
}

void OptionalBankAndMaterialIsolation() {
    auto scene = MakeSyntheticScene();
    const auto baseline = Render(scene,Camera{},256,256);
    for (const auto& chunk : scene.chunks)
        Require(chunk.textureBank == TextureBank::World,"Chunk does not default to immutable world bank");
    ConstantActorTexture(scene,{250,1,2,255});
    const auto unusedActor = Render(scene,Camera{},256,256);
    Require(unusedActor.image.rgb == baseline.image.rgb && unusedActor.depth == baseline.depth &&
        unusedActor.frameHash == baseline.frameHash,"An unused optional actor bank changed default world output");

    scene = TwoBanks();
    const auto textures = scene.textures;
    const auto actors = scene.actorTextures;
    const auto foreground = Render(scene,Origin(),256,256);
    Require(Center(foreground) == std::array<std::uint8_t,3>{211,47,23} &&
        std::abs(CenterDepth(foreground)-2.0f) < .001f,"Actor layer zero sampled world bank zero or wrong depth");
    scene.chunks[1].textureBank = TextureBank::World;
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{17,33,201},
        "Chunk bank selection did not isolate identical material indices");
    Require(scene.textures == textures && scene.actorTextures == actors,
        "Material bank selection mutated/renumbered either texture array");

    // Each bank retains its own 255-layer limit, allowing 510 independently
    // indexed layers without consuming or changing any world layer index.
    scene.textureWidth = scene.textureHeight = scene.actorTextureWidth = scene.actorTextureHeight = 1;
    scene.textureLayers = scene.actorTextureLayers = 255;
    scene.textures.assign(255u*4u,0u); scene.actorTextures.assign(255u*4u,0u);
    std::copy(textures.begin(),textures.end(),scene.textures.begin()+254u*4u);
    const std::array<std::uint8_t,4> actorLast{103,207,31,255};
    std::copy(actorLast.begin(),actorLast.end(),scene.actorTextures.begin()+254u*4u);
    scene.chunks = {Triangle(TextureBank::World,4,254),Triangle(TextureBank::Actor,2,254)};
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{103,207,31},
        "Separate actor layer 254 was restricted by a shared world/actor 255-layer cap");
    scene.chunks.pop_back();
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{17,33,201},
        "Actor layer allocation altered original world layer 254");
}

void SharedDepthAlphaAndOrdering() {
    auto scene = TwoBanks();
    const auto opaque = Render(scene,Origin(),256,256);
    std::reverse(scene.chunks.begin(),scene.chunks.end());
    const auto reverse = Render(scene,Origin(),256,256);
    Require(reverse.image.rgb == opaque.image.rgb && reverse.depth == opaque.depth,
        "Actor and world geometry do not share an order-independent opaque depth buffer");
    ConstantActorTexture(scene,{211,47,23,0});
    const auto masked = Render(scene,Origin(),256,256);
    Require(Center(masked) == std::array<std::uint8_t,3>{17,33,201} &&
        std::abs(CenterDepth(masked)-4.0f) < .001f && masked.transparentSamples > 0,
        "Transparent actor texels wrote depth or occluded world geometry");
    ConstantActorTexture(scene,{211,47,23,127});
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{17,33,201},
        "Actor alpha below the Quest shader cutoff was accepted");
    ConstantActorTexture(scene,{211,47,23,128});
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{211,47,23},
        "Actor alpha at the Quest shader cutoff was rejected");
    // Alpha from the independent actor bank must be used even if world layer 0
    // is transparent, and vice versa.
    scene.textures[3] = 0;
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{211,47,23},
        "World alpha contaminated independent actor material alpha");
    ConstantActorTexture(scene,{211,47,23,0});
    Require(Render(scene,Origin(),256,256).coveredPixels == 0,
        "All-transparent world/actor materials still wrote depth");
}

void PerspectiveBilinearAndNearClip() {
    Scene actor;
    ConstantActorTexture(actor,{0,0,0,255});
    for (std::size_t y = 0; y < 96u; ++y) for (std::size_t x = 0; x < 96u; ++x)
        actor.actorTextures[(y*96u+x)*4u] = static_cast<std::uint8_t>(std::round(x*255.0/95.0));
    actor.chunks = {{0,{
        {{-1,-1,-1},{0,0,1},0,.5f,0},
        {{3,-3,-3},{0,0,1},1,.5f,0},
        {{0,3,-3},{0,0,1},0,.5f,0}},TextureBank::Actor}};
    const auto perspective = Render(actor,Origin(),256,256);
    const auto pixel = Center(perspective);
    Require(pixel[0] >= 40u && pixel[0] <= 44u && pixel[1] == 0 && pixel[2] == 0,
        "Actor texture UVs are affine rather than perspective-correct");
    auto world = actor;
    world.textureWidth = world.actorTextureWidth; world.textureHeight = world.actorTextureHeight;
    world.textureLayers = world.actorTextureLayers; world.textures = world.actorTextures;
    world.actorTextureWidth = world.actorTextureHeight = world.actorTextureLayers = 0;
    world.actorTextures.clear(); world.chunks[0].textureBank = TextureBank::World;
    const auto worldCapture = Render(world,Origin(),256,256);
    Require(worldCapture.image.rgb == perspective.image.rgb && worldCapture.depth == perspective.depth,
        "Actor bank changed perspective interpolation compared with identical world bank texels");

    actor.actorTextureWidth = actor.actorTextureHeight = 2;
    actor.actorTextures = {0,0,0,255,200,0,0,255,0,100,0,255,200,100,80,255};
    for (auto& vertex : actor.chunks[0].vertices) vertex.u = vertex.v = .5f;
    Require(Center(Render(actor,Origin(),256,256)) == std::array<std::uint8_t,3>{100,50,20},
        "Actor bank GL_LINEAR texel-center bilinear filtering differs");
    for (auto& vertex : actor.chunks[0].vertices) { vertex.u = -.5f; vertex.v = 1.5f; }
    Require(Center(Render(actor,Origin(),256,256)) == std::array<std::uint8_t,3>{100,50,20},
        "Actor bank GL_REPEAT wrapping differs for negative/outside UVs");

    actor = MakeSyntheticScene();
    world = actor;
    actor.actorTextureWidth = actor.textureWidth; actor.actorTextureHeight = actor.textureHeight;
    actor.actorTextureLayers = actor.textureLayers; actor.actorTextures = actor.textures;
    actor.textureWidth = actor.textureHeight = actor.textureLayers = 0; actor.textures.clear();
    for (auto& chunk : actor.chunks) chunk.textureBank = TextureBank::Actor;
    const auto clippedActor = Render(actor,Camera{},256,256), clippedWorld = Render(world,Camera{},256,256);
    Require(clippedActor.clippedTriangles > 0 && clippedActor.clippedTriangles == clippedWorld.clippedTriangles &&
        clippedActor.image.rgb == clippedWorld.image.rgb && clippedActor.depth == clippedWorld.depth,
        "Actor bank selection lost UVs/material identity during near-plane clipping");
}

void ParallelLightingAndUnlit() {
    auto scene = TwoBanks();
    scene.vertexLighting.resize(2);
    scene.vertexLighting[0].assign(3,{1,1,1}); scene.vertexLighting[1].assign(3,{.5f,.25f,1});
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{106,12,23},
        "Actor texture bank ignored parallel RGB vertex lighting");
    scene.lightmapWidth = scene.lightmapHeight = scene.lightmapLayers = 1;
    scene.lightmapRgba = {0,128,255,255};
    scene.lightmapVertices.resize(2);
    for (auto& maps : scene.lightmapVertices) maps.assign(3,{.5f,.5f,0,0});
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{0,24,23},
        "Optional actor texture bank broke original baked-lightmap stream or double-multiplied fallback lighting");
    for (auto& map : scene.lightmapVertices[1]) map.flags = kLightmapUnlit;
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{211,47,23},
        "Actor texture bank broke unlit bypass of both light sources");
    scene.lightmapRgba.clear(); scene.lightmapWidth = scene.lightmapHeight = scene.lightmapLayers = 0;
    for (auto& maps : scene.lightmapVertices) for (auto& map : maps) map.page = -1;
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{211,47,23},
        "Actor unlit flag depends on a baked-lightmap atlas being present");
    for (auto& map : scene.lightmapVertices[1]) map.flags = 0;
    scene.chunks[1].polyFlags |= kPolyUnlit;
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{211,47,23},
        "Authored actor PF_Unlit flag did not bypass fallback lighting");
}

void OpaqueMaskedTwoSidedAndInvisibleFlags() {
    auto scene = TwoBanks();
    ConstantActorTexture(scene,{211,47,23,0});
    scene.chunks[1].polyFlags = 0;
    const auto opaque = Render(scene,Origin(),256,256);
    Require(Center(opaque) == std::array<std::uint8_t,3>{211,47,23} &&
        std::abs(CenterDepth(opaque)-2) < .001f && opaque.transparentSamples == 0,
        "Opaque actor discarded palette index-zero RGB because its decoded alpha was zero");
    scene.chunks[1].polyFlags = kPolyMasked;
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{17,33,201},
        "Authored PF_Masked actor did not discard palette index-zero alpha");
    scene.chunks[1].polyFlags = 0;
    std::swap(scene.chunks[1].vertices[1],scene.chunks[1].vertices[2]);
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{17,33,201},
        "Ordinary actor back face was not culled");
    scene.chunks[1].polyFlags = kPolyTwoSided;
    const auto twoSided = Render(scene,Origin(),256,256);
    Require(twoSided.image.rgb == opaque.image.rgb && twoSided.depth == opaque.depth,
        "PF_TwoSided actor back face did not retain front-face coverage/material/depth");
    scene.chunks[1].polyFlags |= kPolyInvisible;
    const auto invisible = Render(scene,Origin(),256,256);
    Require(Center(invisible) == std::array<std::uint8_t,3>{17,33,201} &&
        std::abs(CenterDepth(invisible)-2) < .001f,
        "PF_Invisible opaque actor did not suppress RGB while retaining authored occluding depth");
}

void BlendPrecedenceDepthAndSharedEdges() {
    auto scene = TwoBanks();
    ConstantActorTexture(scene,{100,50,200,0});
    scene.chunks[1].polyFlags = kPolyTranslucent|kPolyMasked;
    auto result = Render(scene,Origin(),256,256);
    Require(Center(result) == std::array<std::uint8_t,3>{110,77,243} &&
        std::abs(CenterDepth(result)-4) < .001f && result.transparentSamples == 0,
        "UE1 translucent blend/precedence or non-depth-writing semantics differ");
    // Modulated is 2*source*destination and does not multiply actor light gains.
    scene.chunks[1].polyFlags = kPolyModulated;
    scene.vertexLighting = {{{1,1,1},{1,1,1},{1,1,1}},{{0,0,0},{0,0,0},{0,0,0}}};
    result = Render(scene,Origin(),256,256);
    Require(Center(result) == std::array<std::uint8_t,3>{13,13,255} &&
        std::abs(CenterDepth(result)-4) < .001f,
        "UE1 modulated blend was alpha-based, depth-writing or affected by actor light gain");
    scene.chunks[1].polyFlags |= kPolyMasked;
    Require(Center(Render(scene,Origin(),256,256)) == std::array<std::uint8_t,3>{17,33,201},
        "UE1 modulated material incorrectly removed its masked flag");
    scene.vertexLighting.clear();
    scene.chunks[1].polyFlags = kPolyTranslucent|kPolyOcclude;
    Require(std::abs(CenterDepth(Render(scene,Origin(),256,256))-2) < .001f,
        "Explicit authored PF_Occlude on translucent actor was discarded");
    // Far modulated triangle then near translucent must be independent of input
    // actor chunk order. This fixture does not claim intersection/BSP parity.
    scene.chunks[1] = Triangle(TextureBank::Actor,3);
    scene.chunks[1].polyFlags = kPolyModulated;
    scene.chunks.push_back(Triangle(TextureBank::Actor,2));
    scene.chunks[2].polyFlags = kPolyTranslucent;
    const auto sorted = Render(scene,Origin(),256,256);
    Require(Center(sorted) == std::array<std::uint8_t,3>{108,60,255},
        "Blend pass did not render far modulated then near translucent after world depth");
    std::reverse(scene.chunks.begin(),scene.chunks.end());
    const auto reordered = Render(scene,Origin(),256,256);
    Require(reordered.image.rgb == sorted.image.rgb && reordered.depth == sorted.depth,
        "Nonintersecting actor transparency changes when submission order is reversed");

    scene = TwoBanks();
    ConstantActorTexture(scene,{100,50,200,255});
    auto& quad = scene.chunks[1]; quad.polyFlags = kPolyTranslucent;
    const Vertex a{{-2,-2,-2},{0,0,1},.5f,.5f,0}, b{{2,-2,-2},{0,0,1},.5f,.5f,0};
    const Vertex c{{2,2,-2},{0,0,1},.5f,.5f,0}, d{{-2,2,-2},{0,0,1},.5f,.5f,0};
    quad.vertices = {a,b,c,a,c,d};
    result = Render(scene,Origin(),256,256);
    const auto edgeOffset = (128u*256u+127u)*3u;
    Require(std::array<std::uint8_t,3>{result.image.rgb[edgeOffset],result.image.rgb[edgeOffset+1u],
        result.image.rgb[edgeOffset+2u]} == std::array<std::uint8_t,3>{110,77,243},
        "Two blended actor triangles double-blended their shared raster edge");
}

void MalformedBankGeometryAndStreams() {
    const auto scene = TwoBanks();
    auto invalid = scene; invalid.actorTextures.pop_back();
    Require(Rejects(invalid),"Truncated actor RGBA bank accepted");
    invalid = scene; invalid.actorTextures.push_back(0);
    Require(Rejects(invalid),"Trailing actor RGBA bytes accepted");
    invalid = scene; invalid.actorTextureWidth = 0;
    Require(Rejects(invalid),"Partial actor bank dimensions accepted");
    invalid = scene; invalid.actorTextureHeight = 2049;
    Require(Rejects(invalid),"Oversized actor texture extent accepted");
    invalid = scene; invalid.actorTextureLayers = 256;
    Require(Rejects(invalid),"Oversized actor layer count accepted");
    invalid = scene; invalid.actorTextureWidth = invalid.actorTextureHeight = invalid.actorTextureLayers =
        std::numeric_limits<std::uint32_t>::max();
    Require(Rejects(invalid),"Overflowing actor bank dimensions accepted");
    invalid = scene; invalid.actorTextureWidth = invalid.actorTextureHeight = invalid.actorTextureLayers = 0;
    invalid.actorTextures.clear();
    Require(Rejects(invalid),"Actor geometry without its texture bank accepted");
    invalid = scene; invalid.chunks[1].textureBank = static_cast<TextureBank>(2);
    Require(Rejects(invalid),"Unknown actor texture bank accepted");
    invalid = scene; invalid.chunks[1].vertices.pop_back();
    Require(Rejects(invalid),"Partial actor triangle accepted");
    invalid = scene; invalid.chunks[1].materialSlot = 1;
    Require(Rejects(invalid),"Unknown actor chunk material mode accepted");
    invalid = scene; for (auto& vertex : invalid.chunks[1].vertices) vertex.materialLayer = 1;
    Require(Rejects(invalid),"Missing actor material layer accepted");
    invalid = scene; invalid.chunks[1].vertices[0].materialLayer = -1;
    Require(Rejects(invalid),"Negative actor material layer accepted");
    invalid = scene; invalid.actorTextureLayers = 2;
    invalid.actorTextures.insert(invalid.actorTextures.end(),96u*96u*4u,255);
    invalid.chunks[1].vertices[0].materialLayer = 1;
    Require(Rejects(invalid),"Actor triangle with mixed material layers accepted");
    invalid = scene; invalid.chunks[1].vertices[0].u = std::numeric_limits<float>::quiet_NaN();
    Require(Rejects(invalid),"Nonfinite actor UV accepted");
    invalid = scene; invalid.chunks[1].vertices[0].position.x = std::numeric_limits<float>::infinity();
    Require(Rejects(invalid),"Nonfinite actor position accepted");
    invalid = scene; invalid.chunks[1].vertices[0].normal.x = std::numeric_limits<float>::quiet_NaN();
    Require(Rejects(invalid),"Nonfinite actor normal accepted");
    invalid = scene; invalid.vertexLighting = {{{1,1,1},{1,1,1},{1,1,1}}};
    Require(Rejects(invalid),"Actor bank bypassed parallel vertex-lighting count validation");
    invalid = scene; invalid.lightmapVertices = {{{0,0,-1,0},{0,0,-1,0},{0,0,-1,0}}};
    Require(Rejects(invalid),"Actor bank bypassed parallel lightmap stream count validation");
    invalid = scene; invalid.chunks.assign(4097u,Chunk{});
    Require(Rejects(invalid),"Excessive actor scene chunk count accepted");
    invalid = scene; invalid.textureWidth = 0;
    Require(Rejects(invalid),"Malformed world bank accepted when actor bank is valid");
    // Reject actor cache writes before making a directory or files. Runtime
    // actor meshes are not a new DXQM version and cannot silently be flattened
    // into its immutable world material bank.
    bool rejectedCache{};
    try { WriteSyntheticCache(std::filesystem::path{},scene); }
    catch (const std::runtime_error&) { rejectedCache = true; }
    Require(rejectedCache,"Runtime actor bank was serialized as immutable DXQM world geometry");
}
} // namespace

int main() {
    try {
        OptionalBankAndMaterialIsolation();
        SharedDepthAlphaAndOrdering();
        PerspectiveBilinearAndNearClip();
        ParallelLightingAndUnlit();
        OpaqueMaskedTwoSidedAndInvisibleFlags();
        BlendPrecedenceDepthAndSharedEdges();
        MalformedBankGeometryAndStreams();
        std::cout << "Actor renderer tests passed: independent 96x96 and 255-layer banks, default world byte equality, "
            << "material/alpha isolation, shared depth, perspective/bilinear/repeat/near clipping, "
            << "parallel lighting/unlit, opaque/masked/two-sided/invisible actor flags, "
            << "UE1 translucent/modulated blend precedence/depth/nonintersecting ordering/shared edges "
            << "and malformed bank/geometry/stream rejection.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Actor renderer test failed: " << error.what() << '\n';
        return 1;
    }
}
