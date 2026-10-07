#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace questvisual {

struct Vec3 { float x{}, y{}, z{}; };
// DXQM v2's exact serialized layout, shared with ue1_package_probe.cpp.
struct Vertex {
    Vec3 position;
    Vec3 normal;
    float u{}, v{};
    std::int32_t materialLayer{-1};
};
static_assert(sizeof(Vertex) == 36, "DXQM v2 vertex layout changed");
struct Chunk { std::int32_t materialSlot{}; std::vector<Vertex> vertices; };
// Optional baked lightmap stream stays outside DXQM v2's immutable layout.
// page=-1 falls back to vertexLighting. Unlit bypasses both lighting sources.
inline constexpr std::uint32_t kLightmapUnlit = 1u;
struct LightmapVertex {
    float u{}, v{};
    std::int32_t page{-1};
    std::uint32_t flags{};
    float minU{}, minV{}, maxU{1.0f}, maxV{1.0f};
};
static_assert(sizeof(LightmapVertex) == 32, "Lightmap vertex layout changed");
struct Scene {
    std::vector<Chunk> chunks;
    std::uint32_t textureWidth{}, textureHeight{}, textureLayers{};
    std::vector<std::uint8_t> textures;
    // Optional direct-light RGB gains parallel to each chunk's vertex array.
    // Kept outside Vertex so DXQM v2's on-disk layout remains unchanged.
    std::vector<std::vector<Vec3>> vertexLighting;
    std::uint32_t lightmapWidth{}, lightmapHeight{}, lightmapLayers{};
    float lightmapGainScale{1.0f}; // Recover HDR gain from normalized atlas RGB.
    std::vector<std::uint8_t> lightmapRgba;
    std::vector<std::vector<LightmapVertex>> lightmapVertices;
};
struct Camera {
    Vec3 position{0.0f, 1.65f, 0.0f};
    float yawDegrees{};   // Positive turns right, looking toward +X at 90 degrees.
    float pitchDegrees{}; // Positive looks up.
    float verticalFovDegrees{90.0f};
    float nearPlane{0.05f};
};
struct Image {
    std::uint32_t width{}, height{};
    std::vector<std::uint8_t> rgb;
};
struct RenderResult {
    Image image;
    std::vector<float> depth;
    std::size_t inputTriangles{}, clippedTriangles{}, rasterizedTriangles{};
    std::size_t coveredPixels{}, transparentSamples{}, flatTriangles{};
    std::uint64_t frameHash{};
    double meanLuminance{}, luminanceDeviation{};
};

Scene ReadQuestCache(const std::filesystem::path& mesh, const std::filesystem::path& materials);
RenderResult Render(const Scene& scene, const Camera& camera,
                    std::uint32_t width, std::uint32_t height);
void WriteBmp(const std::filesystem::path& path, const Image& image);
Image ReadBmp(const std::filesystem::path& path);
double MeanAbsoluteImageError(const Image& actual, const Image& baseline);
Scene MakeSyntheticScene();
void WriteSyntheticCache(const std::filesystem::path& directory, const Scene& scene);

} // namespace questvisual
