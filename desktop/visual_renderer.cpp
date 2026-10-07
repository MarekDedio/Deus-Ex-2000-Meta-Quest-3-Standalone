#include "visual_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace questvisual {
namespace {
constexpr float pi = 3.14159265358979323846f;
constexpr std::array<std::uint8_t, 3> background{8u, 12u, 20u};

template<class T> T Read(std::ifstream& stream) {
    T value{};
    if (!stream.read(reinterpret_cast<char*>(&value), sizeof(value)))
        throw std::runtime_error("Truncated cache/image header");
    return value;
}
template<class T> void Write(std::ofstream& stream, const T& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
void EndOfFile(std::ifstream& stream) {
    if (stream.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Unexpected trailing cache bytes");
}
float Edge(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}
Vec3 Subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float Dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

struct ViewVertex { float x{}, y{}, z{}, u{}, v{}; Vec3 lighting{1.0f,1.0f,1.0f}; float lightU{}, lightV{}; };
ViewVertex Mix(const ViewVertex& a, const ViewVertex& b, float t) {
    return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t,
            a.u+(b.u-a.u)*t, a.v+(b.v-a.v)*t,
            {a.lighting.x+(b.lighting.x-a.lighting.x)*t,
             a.lighting.y+(b.lighting.y-a.lighting.y)*t,
             a.lighting.z+(b.lighting.z-a.lighting.z)*t},
            a.lightU+(b.lightU-a.lightU)*t, a.lightV+(b.lightV-a.lightV)*t};
}
std::vector<ViewVertex> ClipNear(const std::array<ViewVertex, 3>& triangle, float nearPlane) {
    std::vector<ViewVertex> polygon;
    ViewVertex previous = triangle.back();
    bool previousInside = previous.z >= nearPlane;
    for (const ViewVertex& vertex : triangle) {
        const bool inside = vertex.z >= nearPlane;
        if (inside != previousInside) {
            const float t = (nearPlane - previous.z) / (vertex.z - previous.z);
            polygon.push_back(Mix(previous, vertex, t));
        }
        if (inside) polygon.push_back(vertex);
        previous = vertex;
        previousInside = inside;
    }
    return polygon;
}
std::array<float, 4> Sample(const Scene& scene, std::int32_t layer, float u, float v) {
    if (!std::isfinite(u) || !std::isfinite(v))
        throw std::runtime_error("Texture interpolation exceeds finite numeric range");
    if (layer < 0 || static_cast<std::uint32_t>(layer) >= scene.textureLayers)
        return {255.0f, 0.0f, 255.0f, 255.0f};
    // GL_REPEAT with GL_LINEAR. Texel centers match normalized OpenGL coordinates.
    u -= std::floor(u);
    v -= std::floor(v);
    const float tx = u*scene.textureWidth - 0.5f;
    const float ty = v*scene.textureHeight - 0.5f;
    const int x0 = static_cast<int>(std::floor(tx));
    const int y0 = static_cast<int>(std::floor(ty));
    const float fx = tx - x0, fy = ty - y0;
    const auto wrap = [](int index, std::uint32_t extent) {
        const int dimension = static_cast<int>(extent);
        return (index % dimension + dimension) % dimension;
    };
    std::array<float, 4> rgba{};
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            const auto x = wrap(x0 + dx, scene.textureWidth);
            const auto y = wrap(y0 + dy, scene.textureHeight);
            const std::size_t offset = ((static_cast<std::size_t>(layer)*scene.textureHeight+y)*
                scene.textureWidth+x)*4u;
            const float weight = (dx == 0 ? 1.0f-fx : fx)*(dy == 0 ? 1.0f-fy : fy);
            for (std::size_t c = 0; c < 4; ++c) rgba[c] += scene.textures[offset+c]*weight;
        }
    }
    return rgba;
}
std::array<float, 3> SampleLightmap(const Scene& scene, std::int32_t page, float u, float v,
    float minU, float minV, float maxU, float maxV) {
    if (!std::isfinite(u) || !std::isfinite(v))
        throw std::runtime_error("Lightmap interpolation exceeds finite numeric range");
    // GL_CLAMP_TO_EDGE with GL_LINEAR. Clamp normalized coordinates before
    // conversion so even large finite UVs never overflow integer indices.
    // Each packed tile owns its first/last texel centers. Clamp only AFTER
    // perspective interpolation: clamping endpoints distorts interior UVs.
    const float tx = std::clamp(u, minU, maxU)*scene.lightmapWidth - 0.5f;
    const float ty = std::clamp(v, minV, maxV)*scene.lightmapHeight - 0.5f;
    const int x0 = static_cast<int>(std::floor(tx)), y0 = static_cast<int>(std::floor(ty));
    const float fx = tx-x0, fy = ty-y0;
    std::array<float, 3> gain{};
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            const auto x = std::clamp(x0+dx, 0, static_cast<int>(scene.lightmapWidth)-1);
            const auto y = std::clamp(y0+dy, 0, static_cast<int>(scene.lightmapHeight)-1);
            const auto offset = ((static_cast<std::size_t>(page)*scene.lightmapHeight+y)*scene.lightmapWidth+x)*4u;
            const float weight = (dx == 0 ? 1.0f-fx : fx)*(dy == 0 ? 1.0f-fy : fy)*scene.lightmapGainScale/255.0f;
            for (std::size_t channel = 0; channel < 3; ++channel)
                gain[channel] += scene.lightmapRgba[offset+channel]*weight;
        }
    }
    return gain;
}
void Rasterize(RenderResult& result, const Scene& scene,
               const std::array<ViewVertex, 3>& triangle, std::int32_t layer,
               bool textured, float focalLength, std::int32_t lightmapPage = -1,
               bool unlit = false, float minU = 0.0f, float minV = 0.0f,
               float maxU = 1.0f, float maxV = 1.0f) {
    const auto width = result.image.width, height = result.image.height;
    struct Projected { float x, y, inverseDepth, u, v; Vec3 lighting; float lightU, lightV; };
    std::array<Projected, 3> points{};
    for (std::size_t i = 0; i < 3; ++i) {
        const auto& vertex = triangle[i];
        const float inverse = 1.0f/vertex.z;
        points[i] = {width*0.5f + vertex.x*inverse*focalLength,
                     height*0.5f - vertex.y*inverse*focalLength, inverse,
                     vertex.u*inverse, vertex.v*inverse,
                     {vertex.lighting.x*inverse,vertex.lighting.y*inverse,vertex.lighting.z*inverse},
                     vertex.lightU*inverse, vertex.lightV*inverse};
        if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y) ||
            !std::isfinite(points[i].inverseDepth) || !std::isfinite(points[i].u) ||
            !std::isfinite(points[i].v) || !std::isfinite(points[i].lighting.x) ||
            !std::isfinite(points[i].lighting.y) || !std::isfinite(points[i].lighting.z) ||
            !std::isfinite(points[i].lightU) || !std::isfinite(points[i].lightV))
            throw std::runtime_error("Projected geometry/UV exceeds finite numeric range");
    }
    const float area = Edge(points[0].x, points[0].y, points[1].x, points[1].y,
                            points[2].x, points[2].y);
    // Quest enables GL backface culling. BMP's downward Y reverses screen winding.
    if (!std::isfinite(area)) throw std::runtime_error("Projected triangle exceeds finite numeric range");
    if (area >= -1.0e-6f) return;
    const float lowX = std::min({points[0].x, points[1].x, points[2].x});
    const float highX = std::max({points[0].x, points[1].x, points[2].x});
    const float lowY = std::min({points[0].y, points[1].y, points[2].y});
    const float highY = std::max({points[0].y, points[1].y, points[2].y});
    if (highX < 0.0f || highY < 0.0f || lowX >= width || lowY >= height) return;
    const int minX = static_cast<int>(std::max(0.0f, std::floor(lowX)));
    const int maxX = static_cast<int>(std::min(static_cast<float>(width-1u), std::ceil(highX)));
    const int minY = static_cast<int>(std::max(0.0f, std::floor(lowY)));
    const int maxY = static_cast<int>(std::min(static_cast<float>(height-1u), std::ceil(highY)));
    ++result.rasterizedTriangles;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = x+0.5f, py = y+0.5f;
            const float a = Edge(points[1].x, points[1].y, points[2].x, points[2].y, px, py)/area;
            const float b = Edge(points[2].x, points[2].y, points[0].x, points[0].y, px, py)/area;
            const float c = 1.0f-a-b;
            if (a < -1.0e-6f || b < -1.0e-6f || c < -1.0e-6f) continue;
            const float inverse = a*points[0].inverseDepth+b*points[1].inverseDepth+
                c*points[2].inverseDepth;
            const float depth = 1.0f/inverse;
            if (!std::isfinite(depth) || depth <= 0.0f)
                throw std::runtime_error("Interpolated depth exceeds finite numeric range");
            const std::size_t pixel = static_cast<std::size_t>(y)*width+x;
            if (depth >= result.depth[pixel]) continue;
            const float u = (a*points[0].u+b*points[1].u+c*points[2].u)/inverse;
            const float v = (a*points[0].v+b*points[1].v+c*points[2].v)/inverse;
            const auto texel = textured ? Sample(scene, layer, u, v) :
                std::array<float, 4>{38.0f, 204.0f, 140.0f, 255.0f};
            // Match the Quest world material's alpha cutoff (no opaque chroma-key holes).
            if (texel[3] < 127.5f) { ++result.transparentSamples; continue; }
            result.depth[pixel] = depth;
            std::array<float,3> gain{1.0f,1.0f,1.0f};
            if (!unlit && lightmapPage >= 0) {
                const float lightU = (a*points[0].lightU+b*points[1].lightU+c*points[2].lightU)/inverse;
                const float lightV = (a*points[0].lightV+b*points[1].lightV+c*points[2].lightV)/inverse;
                gain = SampleLightmap(scene, lightmapPage, lightU, lightV, minU, minV, maxU, maxV);
            } else if (!unlit && !scene.vertexLighting.empty()) {
                gain = {(a*points[0].lighting.x+b*points[1].lighting.x+c*points[2].lighting.x)/inverse,
                        (a*points[0].lighting.y+b*points[1].lighting.y+c*points[2].lighting.y)/inverse,
                        (a*points[0].lighting.z+b*points[1].lighting.z+c*points[2].lighting.z)/inverse};
            }
            for (std::size_t channel = 0; channel < 3; ++channel)
                result.image.rgb[pixel*3u+channel] = static_cast<std::uint8_t>(
                    std::clamp(std::round(texel[channel]*gain[channel]), 0.0f, 255.0f));
        }
    }
}
void AddQuad(Scene& scene, Vec3 a, Vec3 b, Vec3 c, Vec3 d,
             std::int32_t layer, float repeat = 1.0f) {
    if (scene.chunks.empty()) scene.chunks.push_back({0, {}});
    const Vertex va{a,{0,0,1},0,repeat,layer}, vb{b,{0,0,1},repeat,repeat,layer};
    const Vertex vc{c,{0,0,1},repeat,0,layer}, vd{d,{0,0,1},0,0,layer};
    auto& vertices = scene.chunks.front().vertices;
    vertices.insert(vertices.end(), {va,vb,vc,va,vc,vd});
}
} // namespace

Scene ReadQuestCache(const std::filesystem::path& mesh, const std::filesystem::path& materials) {
    Scene scene;
    std::ifstream textureFile(materials, std::ios::binary);
    if (!textureFile) throw std::runtime_error("Cannot open material cache: " + materials.string());
    if (Read<std::uint32_t>(textureFile) != 0x41515844u || Read<std::uint32_t>(textureFile) != 1u)
        throw std::runtime_error("Expected DXQA v1 material array");
    scene.textureWidth = Read<std::uint32_t>(textureFile);
    scene.textureHeight = Read<std::uint32_t>(textureFile);
    scene.textureLayers = Read<std::uint32_t>(textureFile);
    const std::uint64_t bytes = static_cast<std::uint64_t>(scene.textureWidth)*
        scene.textureHeight*scene.textureLayers*4u;
    if (!scene.textureWidth || !scene.textureHeight || !scene.textureLayers ||
        scene.textureWidth > 2048u || scene.textureHeight > 2048u ||
        scene.textureLayers > 255u || bytes > 512u*1024u*1024u)
        throw std::runtime_error("Invalid/oversized DXQA dimensions");
    scene.textures.resize(static_cast<std::size_t>(bytes));
    if (!textureFile.read(reinterpret_cast<char*>(scene.textures.data()),
                          static_cast<std::streamsize>(bytes)))
        throw std::runtime_error("Truncated DXQA texels");
    EndOfFile(textureFile);

    std::ifstream meshFile(mesh, std::ios::binary);
    if (!meshFile) throw std::runtime_error("Cannot open world mesh: " + mesh.string());
    if (Read<std::uint32_t>(meshFile) != 0x4d515844u || Read<std::uint32_t>(meshFile) != 2u)
        throw std::runtime_error("Expected DXQM v2 world mesh");
    const auto chunks = Read<std::uint32_t>(meshFile);
    if (chunks == 0 || chunks >= 128u) throw std::runtime_error("Invalid DXQM chunk count");
    for (std::uint32_t i = 0; i < chunks; ++i) {
        Chunk chunk;
        chunk.materialSlot = Read<std::int32_t>(meshFile);
        const auto vertices = Read<std::uint32_t>(meshFile);
        if (!vertices || vertices > 60000u || vertices%3u != 0)
            throw std::runtime_error("Invalid DXQM vertex count");
        if (chunk.materialSlot != 0 && chunk.materialSlot != -1)
            throw std::runtime_error("Unsupported DXQM chunk material slot");
        chunk.vertices.resize(vertices);
        if (!meshFile.read(reinterpret_cast<char*>(chunk.vertices.data()), vertices*sizeof(Vertex)))
            throw std::runtime_error("Truncated DXQM vertices");
        for (const auto& vertex : chunk.vertices) {
            if (!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y) ||
                !std::isfinite(vertex.position.z) || !std::isfinite(vertex.normal.x) ||
                !std::isfinite(vertex.normal.y) || !std::isfinite(vertex.normal.z) ||
                !std::isfinite(vertex.u) || !std::isfinite(vertex.v))
                throw std::runtime_error("Non-finite DXQM vertex");
            if (chunk.materialSlot == 0 && (vertex.materialLayer < 0 ||
                static_cast<std::uint32_t>(vertex.materialLayer) >= scene.textureLayers))
                throw std::runtime_error("DXQM references a missing DXQA material layer");
        }
        for (std::size_t j = 0; j < chunk.vertices.size(); j += 3u) {
            if (chunk.vertices[j].materialLayer != chunk.vertices[j+1u].materialLayer ||
                chunk.vertices[j].materialLayer != chunk.vertices[j+2u].materialLayer)
                throw std::runtime_error("DXQM triangle has mixed material layers");
        }
        scene.chunks.push_back(std::move(chunk));
    }
    EndOfFile(meshFile);
    return scene;
}

RenderResult Render(const Scene& scene, const Camera& camera,
                    std::uint32_t width, std::uint32_t height) {
    if (width < 32u || height < 32u || width > 4096u || height > 4096u ||
        !std::isfinite(camera.yawDegrees) || !std::isfinite(camera.pitchDegrees) ||
        !std::isfinite(camera.position.x) || !std::isfinite(camera.position.y) ||
        !std::isfinite(camera.position.z) || !std::isfinite(camera.verticalFovDegrees) ||
        camera.verticalFovDegrees < 10.0f || camera.verticalFovDegrees > 150.0f ||
        !std::isfinite(camera.nearPlane) || camera.nearPlane <= 0.0f)
        throw std::runtime_error("Invalid camera/image dimensions");
    if (!scene.vertexLighting.empty()) {
        if (scene.vertexLighting.size() != scene.chunks.size())
            throw std::runtime_error("Lighting stream chunk count does not match world geometry");
        for (std::size_t chunkIndex = 0u; chunkIndex < scene.chunks.size(); ++chunkIndex) {
            const auto& gains = scene.vertexLighting[chunkIndex];
            if (gains.size() != scene.chunks[chunkIndex].vertices.size())
                throw std::runtime_error("Lighting stream vertex count does not match world geometry");
            for (const auto& gain : gains) {
                if (!std::isfinite(gain.x) || !std::isfinite(gain.y) || !std::isfinite(gain.z) ||
                    gain.x < 0.0f || gain.y < 0.0f || gain.z < 0.0f ||
                    gain.x > 16.0f || gain.y > 16.0f || gain.z > 16.0f)
                    throw std::runtime_error("Lighting gain is invalid or oversized");
            }
        }
    }
    const bool hasLightmapAtlas = scene.lightmapWidth != 0u || scene.lightmapHeight != 0u ||
        scene.lightmapLayers != 0u || !scene.lightmapRgba.empty();
    if (!std::isfinite(scene.lightmapGainScale) || scene.lightmapGainScale <= 0.0f || scene.lightmapGainScale > 16.0f)
        throw std::runtime_error("Lightmap gain scale is nonfinite, nonpositive or oversized");
    if (hasLightmapAtlas) {
        const auto bytes = static_cast<std::uint64_t>(scene.lightmapWidth)*scene.lightmapHeight*scene.lightmapLayers*4u;
        if (scene.lightmapWidth == 0u || scene.lightmapHeight == 0u || scene.lightmapLayers == 0u ||
            scene.lightmapWidth > 4096u || scene.lightmapHeight > 4096u || scene.lightmapLayers > 255u ||
            bytes > 256u*1024u*1024u || bytes != scene.lightmapRgba.size() || scene.lightmapVertices.empty())
            throw std::runtime_error("Lightmap atlas is invalid, oversized or missing its UV stream");
    }
    if (!scene.lightmapVertices.empty()) {
        if (scene.lightmapVertices.size() != scene.chunks.size())
            throw std::runtime_error("Lightmap stream chunk count does not match world geometry");
        for (std::size_t chunkIndex = 0; chunkIndex < scene.chunks.size(); ++chunkIndex) {
            const auto& maps = scene.lightmapVertices[chunkIndex];
            if (maps.size() != scene.chunks[chunkIndex].vertices.size())
                throw std::runtime_error("Lightmap stream vertex count does not match world geometry");
            for (const auto& map : maps) {
                if (!std::isfinite(map.u) || !std::isfinite(map.v) || (map.flags & ~kLightmapUnlit) != 0u ||
                    !std::isfinite(map.minU) || !std::isfinite(map.minV) ||
                    !std::isfinite(map.maxU) || !std::isfinite(map.maxV) ||
                    map.minU < 0.0f || map.minV < 0.0f || map.maxU > 1.0f || map.maxV > 1.0f ||
                    map.minU > map.maxU || map.minV > map.maxV ||
                    map.page < -1 || (map.page >= 0 && (!hasLightmapAtlas ||
                    static_cast<std::uint32_t>(map.page) >= scene.lightmapLayers)))
                    throw std::runtime_error("Lightmap vertex UV, flags or page is invalid");
            }
            for (std::size_t i = 0; i+2u < maps.size(); i += 3u)
                if (maps[i].page != maps[i+1u].page || maps[i].page != maps[i+2u].page ||
                    maps[i].flags != maps[i+1u].flags || maps[i].flags != maps[i+2u].flags ||
                    maps[i].minU != maps[i+1u].minU || maps[i].minU != maps[i+2u].minU ||
                    maps[i].minV != maps[i+1u].minV || maps[i].minV != maps[i+2u].minV ||
                    maps[i].maxU != maps[i+1u].maxU || maps[i].maxU != maps[i+2u].maxU ||
                    maps[i].maxV != maps[i+1u].maxV || maps[i].maxV != maps[i+2u].maxV)
                    throw std::runtime_error("Lightmap triangle mixes pages, lighting flags or tile bounds");
        }
    }
    RenderResult result;
    result.image = {width,height,std::vector<std::uint8_t>(static_cast<std::size_t>(width)*height*3u)};
    result.depth.assign(static_cast<std::size_t>(width)*height, std::numeric_limits<float>::infinity());
    for (std::size_t i = 0; i < result.depth.size(); ++i)
        std::copy(background.begin(), background.end(), result.image.rgb.begin()+i*3u);
    const float yaw = std::remainder(camera.yawDegrees,360.0f)*pi/180.0f;
    const float pitch = std::remainder(camera.pitchDegrees,360.0f)*pi/180.0f;
    const Vec3 right{std::cos(yaw),0,std::sin(yaw)};
    const Vec3 forward{std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
    const Vec3 up{-std::sin(yaw)*std::sin(pitch),std::cos(pitch),std::cos(yaw)*std::sin(pitch)};
    const float focal = height*0.5f/std::tan(camera.verticalFovDegrees*pi/360.0f);
    for (std::size_t chunkIndex = 0u; chunkIndex < scene.chunks.size(); ++chunkIndex) {
        const auto& chunk = scene.chunks[chunkIndex];
        for (std::size_t offset = 0; offset+2u < chunk.vertices.size(); offset += 3u) {
            ++result.inputTriangles;
            if (chunk.materialSlot != 0) ++result.flatTriangles;
            std::array<ViewVertex,3> triangle{};
            const auto lightmapPage = scene.lightmapVertices.empty() ? -1 : scene.lightmapVertices[chunkIndex][offset].page;
            const bool unlit = !scene.lightmapVertices.empty() &&
                (scene.lightmapVertices[chunkIndex][offset].flags & kLightmapUnlit) != 0u;
            const auto bounds = scene.lightmapVertices.empty() ? LightmapVertex{} : scene.lightmapVertices[chunkIndex][offset];
            bool crossesNear = false;
            for (std::size_t i = 0; i < 3; ++i) {
                const auto& vertex = chunk.vertices[offset+i];
                const Vec3 relative = Subtract(vertex.position, camera.position);
                triangle[i] = {Dot(relative,right),Dot(relative,up),Dot(relative,forward),vertex.u,vertex.v};
                if (!scene.vertexLighting.empty()) triangle[i].lighting = scene.vertexLighting[chunkIndex][offset+i];
                if (!scene.lightmapVertices.empty()) {
                    triangle[i].lightU = scene.lightmapVertices[chunkIndex][offset+i].u;
                    triangle[i].lightV = scene.lightmapVertices[chunkIndex][offset+i].v;
                }
                if (!std::isfinite(triangle[i].x) || !std::isfinite(triangle[i].y) ||
                    !std::isfinite(triangle[i].z))
                    throw std::runtime_error("Camera/geometry exceeds finite numeric range");
                crossesNear = crossesNear || triangle[i].z < camera.nearPlane;
            }
            const auto clipped = ClipNear(triangle,camera.nearPlane);
            if (crossesNear && !clipped.empty()) ++result.clippedTriangles;
            for (std::size_t i = 1; i+1u < clipped.size(); ++i)
                Rasterize(result, scene, {clipped[0],clipped[i],clipped[i+1u]},
                    chunk.vertices[offset].materialLayer, chunk.materialSlot == 0, focal, lightmapPage, unlit,
                    bounds.minU, bounds.minV, bounds.maxU, bounds.maxV);
        }
    }
    double luminanceSum{}, squaredSum{};
    result.frameHash = 14695981039346656037ull;
    for (std::size_t i = 0; i < result.depth.size(); ++i) {
        if (std::isfinite(result.depth[i])) ++result.coveredPixels;
        const auto* pixel = result.image.rgb.data()+i*3u;
        const double luminance = (0.2126*pixel[0]+0.7152*pixel[1]+0.0722*pixel[2])/255.0;
        luminanceSum += luminance;
        squaredSum += luminance*luminance;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            result.frameHash ^= pixel[channel];
            result.frameHash *= 1099511628211ull;
        }
    }
    result.meanLuminance = luminanceSum/result.depth.size();
    result.luminanceDeviation = std::sqrt(std::max(0.0,
        squaredSum/result.depth.size()-result.meanLuminance*result.meanLuminance));
    return result;
}

void WriteBmp(const std::filesystem::path& path, const Image& image) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot write screenshot: " + path.string());
    const std::uint32_t rowBytes = (image.width*3u+3u)&~3u;
    const std::uint32_t pixelBytes = rowBytes*image.height;
    Write(file,std::uint16_t{0x4d42u}); Write(file,std::uint32_t{54u+pixelBytes});
    Write(file,std::uint32_t{}); Write(file,std::uint32_t{54u});
    Write(file,std::uint32_t{40u}); Write(file,image.width); Write(file,image.height);
    Write(file,std::uint16_t{1u}); Write(file,std::uint16_t{24u});
    Write(file,std::uint32_t{}); Write(file,pixelBytes);
    for (int i = 0; i < 4; ++i) Write(file,std::uint32_t{});
    std::vector<std::uint8_t> row(rowBytes);
    for (std::uint32_t y = image.height; y > 0; --y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const auto source = ((y-1u)*image.width+x)*3u;
            row[x*3u] = image.rgb[source+2u]; row[x*3u+1u] = image.rgb[source+1u];
            row[x*3u+2u] = image.rgb[source];
        }
        file.write(reinterpret_cast<const char*>(row.data()),row.size());
    }
    if (!file) throw std::runtime_error("Failed writing BMP pixels");
}

Image ReadBmp(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if (!file || Read<std::uint16_t>(file) != 0x4d42u) throw std::runtime_error("Invalid baseline BMP");
    Read<std::uint32_t>(file); Read<std::uint32_t>(file);
    const auto offset = Read<std::uint32_t>(file);
    if (Read<std::uint32_t>(file) != 40u) throw std::runtime_error("Expected BITMAPINFOHEADER baseline");
    const auto width = Read<std::int32_t>(file), height = Read<std::int32_t>(file);
    if (width < 32 || height < 32 || width > 4096 || height > 4096 ||
        Read<std::uint16_t>(file) != 1u || Read<std::uint16_t>(file) != 24u ||
        Read<std::uint32_t>(file) != 0u) throw std::runtime_error("Expected uncompressed 24-bit BMP baseline");
    file.seekg(offset);
    Image result{static_cast<std::uint32_t>(width),static_cast<std::uint32_t>(height),
        std::vector<std::uint8_t>(static_cast<std::size_t>(width)*height*3u)};
    std::vector<std::uint8_t> row((width*3u+3u)&~3u);
    for (int y = height-1; y >= 0; --y) {
        if (!file.read(reinterpret_cast<char*>(row.data()),row.size()))
            throw std::runtime_error("Truncated baseline BMP");
        for (int x = 0; x < width; ++x) {
            const auto pixel = (static_cast<std::size_t>(y)*width+x)*3u;
            result.rgb[pixel] = row[x*3u+2u]; result.rgb[pixel+1u] = row[x*3u+1u];
            result.rgb[pixel+2u] = row[x*3u];
        }
    }
    return result;
}
double MeanAbsoluteImageError(const Image& actual, const Image& baseline) {
    if (actual.width != baseline.width || actual.height != baseline.height)
        throw std::runtime_error("Baseline dimensions differ from capture");
    double difference{};
    for (std::size_t i = 0; i < actual.rgb.size(); ++i)
        difference += std::abs(static_cast<int>(actual.rgb[i])-baseline.rgb[i]);
    return difference/(actual.rgb.size()*255.0);
}

Scene MakeSyntheticScene() {
    Scene scene;
    scene.textureWidth = scene.textureHeight = 64u;
    scene.textureLayers = 3u;
    scene.textures.resize(64u*64u*3u*4u);
    for (std::uint32_t layer = 0; layer < 3; ++layer) {
        for (std::uint32_t y = 0; y < 64; ++y) {
            for (std::uint32_t x = 0; x < 64; ++x) {
                const bool checker = ((x/8u+y/8u)%2u) == 0;
                const std::array<std::uint8_t,4> color = layer == 0 ?
                    std::array<std::uint8_t,4>{30,70,150,255} : layer == 1 ?
                    std::array<std::uint8_t,4>{220,45,30,255} :
                    (checker ? std::array<std::uint8_t,4>{225,225,190,255} :
                               std::array<std::uint8_t,4>{40,65,55,255});
                std::copy(color.begin(),color.end(),scene.textures.begin()+((layer*64u+y)*64u+x)*4u);
            }
        }
    }
    // Synthetic geometry only: foreground depth occluder and oblique checker floor.
    AddQuad(scene,{-3,0,-6},{3,0,-6},{3,3,-6},{-3,3,-6},0,3);
    AddQuad(scene,{-0.55f,1.1f,-2},{0.55f,1.1f,-2},{0.55f,2.2f,-2},{-0.55f,2.2f,-2},1);
    AddQuad(scene,{-3,0,-0.3f},{3,0,-0.3f},{3,0,-6},{-3,0,-6},2,8);
    scene.chunks.front().vertices.insert(scene.chunks.front().vertices.end(), {
        {{-1.2f,0.8f,-0.02f},{0,0,1},0,1,2},
        {{-0.6f,0.8f,-1.5f},{0,0,1},1,1,2},
        {{-1.2f,1.8f,-1.5f},{0,0,1},0,0,2}});
    return scene;
}

void WriteSyntheticCache(const std::filesystem::path& directory, const Scene& scene) {
    std::filesystem::create_directories(directory);
    std::ofstream mesh(directory/"quest-world.mesh",std::ios::binary);
    Write(mesh,std::uint32_t{0x4d515844u}); Write(mesh,std::uint32_t{2u});
    Write(mesh,static_cast<std::uint32_t>(scene.chunks.size()));
    for (const auto& chunk : scene.chunks) {
        Write(mesh,chunk.materialSlot); Write(mesh,static_cast<std::uint32_t>(chunk.vertices.size()));
        mesh.write(reinterpret_cast<const char*>(chunk.vertices.data()),chunk.vertices.size()*sizeof(Vertex));
    }
    std::ofstream materials(directory/"quest-material-array.rgba",std::ios::binary);
    Write(materials,std::uint32_t{0x41515844u}); Write(materials,std::uint32_t{1u});
    Write(materials,scene.textureWidth); Write(materials,scene.textureHeight); Write(materials,scene.textureLayers);
    materials.write(reinterpret_cast<const char*>(scene.textures.data()),scene.textures.size());
    if (!mesh || !materials) throw std::runtime_error("Failed writing synthetic cache fixture");
}
} // namespace questvisual
