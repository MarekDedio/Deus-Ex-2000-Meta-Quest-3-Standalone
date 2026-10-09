#pragma once

#include "quest_actor_geometry.h"

#include <cstring>
#include <filesystem>
#include <limits>
#include <set>

namespace QuestVr {

// A deliberately restricted visual increment, not a spawned player pawn, rig,
// animation/IK system, or independent original left-hand asset.
struct PlayerVisualLimits {
    std::size_t maximumTrianglesPerPart{4096u};
    std::size_t maximumTotalTriangles{12288u};
    std::size_t maximumTextures{8u};
    std::size_t maximumTextureBytes{16u * 1024u * 1024u};
    std::size_t maximumMeshExportBytes{16u * 1024u * 1024u};
    std::uint32_t maximumTextureDimension{2048u};
};

struct PlayerVisualTriangle {
    std::array<ActorTriangleVertex, 3> vertices;
    std::uint32_t polyFlags{};
    std::uint32_t textureIndex{}; // Shared PlayerVisualAssets::textures.
    std::uint16_t sourceMaterial{};
};

struct PlayerVisualPart {
    std::vector<PlayerVisualTriangle> triangles;
    std::string provenance;
    ActorVec3 originalPivotObjectUnits;
    bool derivedMirrored{};
};

struct PlayerVisualTexture {
    std::string texturePath;
    PortableTextureImage image;
    std::uint32_t polyFlags{};
};

struct PlayerVisualAssets {
    bool passed{};
    std::string error;
    PlayerVisualPart lowerBody, rightHand, leftHand;
    std::vector<PlayerVisualTexture> textures;
};

struct PlayerVisualTextureUploadEntry {
    std::uint32_t textureIndex{}, width{}, height{};
    std::size_t rgbaByteCount{};
};
using PlayerVisualTextureUploadPlan = std::vector<PlayerVisualTextureUploadEntry>;

// Only the wearer's restricted lower-body view disables culling: a camera
// above the original open waist otherwise sees culled trouser interiors.
// This returns renderer state only; authored flags, alpha/depth and geometry
// remain unchanged. Hands continue honoring the original PF_TwoSided bit.
inline bool CullPlayerVisualPart(std::size_t part, std::uint32_t polyFlags) {
    if (part >= 3u) throw std::runtime_error("Player visual part index is outside the three prepared parts");
    return part != 0u && (polyFlags & 0x100u) == 0u;
}

namespace PlayerVisualDetail {

inline constexpr const char* OriginalHandTexture = "DeusExItems.Skins.WeaponHandsTex";

inline void ValidateLimits(const PlayerVisualLimits& limits) {
    if (limits.maximumTrianglesPerPart == 0u || limits.maximumTrianglesPerPart > 65536u ||
        limits.maximumTotalTriangles == 0u || limits.maximumTotalTriangles > 196608u ||
        limits.maximumTextures == 0u || limits.maximumTextures > 255u ||
        limits.maximumTextureBytes == 0u || limits.maximumTextureBytes > 256u * 1024u * 1024u ||
        limits.maximumMeshExportBytes == 0u || limits.maximumMeshExportBytes > 256u * 1024u * 1024u ||
        limits.maximumTextureDimension == 0u || limits.maximumTextureDimension > 2048u)
        throw std::runtime_error("Player visual limits are invalid");
}

inline MeshPose OriginalStillPose(const PortableLodMesh& mesh) {
    if (!mesh.animation || !FindMeshAnimationSequence(*mesh.animation, "Still", false))
        throw std::runtime_error("Player visual mesh lacks its original Still sequence");
    MeshAnimationState state;
    state.main.sequence = "Still";
    const auto pose = PrepareMeshPose(mesh, state);
    if (!pose.drawable || pose.fallbackUsed || !MeshAnimationNamesEqual(pose.resolvedSequence, "Still"))
        throw std::runtime_error("Original player Still pose is unavailable: " + pose.error);
    return pose;
}

inline ActorTransform ObjectToQuestMeters() {
    auto transform = BuildActorToQuest({}, {}, 0, 0, 0, 1.0f, {1.0f, 1.0f, 1.0f}, {});
    // The map actor transform's +1m spawn convention does not belong to a
    // feet-/controller-local visual. Mesh Scale/Origin/RotOrigin remain decoded.
    transform.translation.y -= 1.0f;
    return transform;
}

inline std::uint32_t TextureIndex(PlayerVisualAssets& assets, const std::string& path,
    const PlayerVisualLimits& limits) {
    if (path.empty()) throw std::runtime_error("Original player surface has no texture");
    for (std::size_t i = 0u; i < assets.textures.size(); ++i)
        if (assets.textures[i].texturePath == path) return static_cast<std::uint32_t>(i);
    if (assets.textures.size() >= limits.maximumTextures)
        throw std::runtime_error("Player visual texture count exceeds its budget");
    assets.textures.push_back({path, {}, 0u});
    return static_cast<std::uint32_t>(assets.textures.size() - 1u);
}

inline void ValidateTriangle(const PlayerVisualTriangle& triangle) {
    for (const auto& vertex : triangle.vertices)
        if (!IsFiniteActorVector(vertex.position) || !IsFiniteActorVector(vertex.normal) ||
            !std::isfinite(vertex.u) || !std::isfinite(vertex.v))
            throw std::runtime_error("Player visual triangle is non-finite");
}

inline void Append(PlayerVisualPart& part, PlayerVisualTriangle triangle,
    const PlayerVisualLimits& limits) {
    if (part.triangles.size() >= limits.maximumTrianglesPerPart)
        throw std::runtime_error("Player visual part exceeds its triangle budget");
    ValidateTriangle(triangle);
    part.triangles.push_back(std::move(triangle));
}

// Audited GM_Trench Still: material 2 maps texture slot 2 (trousers/shoes).
// Material 4 maps slot 5 and has disconnected lower-coat and collar surfaces;
// all lower-coat corners are <=11.3711 UE Z, all collar corners >=25.5742.
// The 12-unit cut selects complete original lower-coat triangles, never clips
// or manufactures geometry, and deliberately leaves out torso/head/arms.
inline bool LowerBodyTriangle(std::uint16_t material,
    const std::array<ActorTriangleVertex, 3>& triangle) {
    if (material == 2u) return true;
    if (material != 4u) return false;
    for (const auto& vertex : triangle)
        if (vertex.position.y > 12.0f / 52.5f) return false;
    return true;
}

inline PlayerVisualPart MirroredHand(const PlayerVisualPart& original) {
    auto mirrored = original;
    mirrored.derivedMirrored = true;
    mirrored.provenance = "Quest-X mirrored variant of original Glock right-hand/sleeve surfaces; not an authored left-hand asset";
    for (auto& triangle : mirrored.triangles) {
        for (auto& vertex : triangle.vertices) {
            vertex.position.x = -vertex.position.x;
            vertex.normal.x = -vertex.normal.x;
        }
        std::swap(triangle.vertices[1], triangle.vertices[2]);
        ValidateTriangle(triangle);
    }
    return mirrored;
}

inline void CheckMeshExport(const PortablePackageTables& package, std::size_t index,
    const PlayerVisualLimits& limits) {
    const auto bytes = package.exports.at(index).ObjSize;
    if (bytes <= 0 || static_cast<std::size_t>(bytes) > limits.maximumMeshExportBytes)
        throw std::runtime_error("Player visual mesh export exceeds its decode budget");
}

inline std::string QualifiedReference(const PortablePackageTables& package,
    const PortableTaggedProperty& property) {
    const auto reference = DecodePortableObjectReference(property);
    auto path = GetPortableObjectPath(package, reference);
    if (reference > 0 && !path.empty())
        path = std::filesystem::path(package.sourcePath).stem().string() + "." + path;
    return path;
}

inline void DecodeTextures(PlayerVisualAssets& assets,
    const PortablePackageTables& characters, const PortablePackageTables& items,
    const PlayerVisualLimits& limits) {
    std::size_t bytes{};
    for (auto& texture : assets.textures) {
        const auto dot = texture.texturePath.find('.');
        if (dot == std::string::npos) throw std::runtime_error("Player texture identity lacks its package");
        const auto packageName = texture.texturePath.substr(0u, dot);
        const PortablePackageTables* package = packageName == "DeusExCharacters" ? &characters :
            packageName == "DeusExItems" ? &items : nullptr;
        if (!package) throw std::runtime_error("Player texture comes from an unexpected package");
        const auto path = texture.texturePath.substr(dot + 1u);
        const auto index = FindPortableTextureExport(*package, path);
        const auto exportBytes = package->exports.at(index).ObjSize;
        if (exportBytes <= 0 || static_cast<std::size_t>(exportBytes) > limits.maximumTextureBytes)
            throw std::runtime_error("Player texture export exceeds its decode budget");
        std::int32_t width{}, height{};
        for (const auto& property : LoadPortableExportProperties(*package, index).properties) {
            if (property.type != 2u || property.value.size() != 4u) continue;
            std::int32_t value{};
            std::memcpy(&value, property.value.data(), sizeof(value));
            if (property.name == "USize") width = value;
            else if (property.name == "VSize") height = value;
            else if (property.name == "PolyFlags") texture.polyFlags = static_cast<std::uint32_t>(value);
        }
        if (width <= 0 || height <= 0 || static_cast<std::uint32_t>(width) > limits.maximumTextureDimension ||
            static_cast<std::uint32_t>(height) > limits.maximumTextureDimension)
            throw std::runtime_error("Original player texture dimensions are outside the budget");
        const auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
        if (size > limits.maximumTextureBytes || bytes > limits.maximumTextureBytes - size)
            throw std::runtime_error("Player texture array exceeds its retained-byte budget");
        bool masked = (texture.polyFlags & 2u) != 0u;
        for (const auto* part : {&assets.lowerBody, &assets.rightHand, &assets.leftHand})
            for (const auto& triangle : part->triangles)
                if (assets.textures.at(triangle.textureIndex).texturePath == texture.texturePath &&
                    (triangle.polyFlags & 2u) != 0u) masked = true;
        texture.image = DecodePortableIndexedTexture(*package, path, masked);
        if (texture.image.width != static_cast<std::uint32_t>(width) ||
            texture.image.height != static_cast<std::uint32_t>(height) || texture.image.rgba.size() != size)
            throw std::runtime_error("Player texture mip disagrees with its authored dimensions");
        bytes += size;
    }
    for (auto* part : {&assets.lowerBody, &assets.rightHand, &assets.leftHand})
        for (auto& triangle : part->triangles)
            triangle.polyFlags |= assets.textures.at(triangle.textureIndex).polyFlags & 2u;
}

} // namespace PlayerVisualDetail

// The original JC textures have different native dimensions. Validate every
// entry before any GPU call, then let the caller upload each unchanged image
// to a separate one-layer array. Keeping native dimensions and original UVs
// preserves original bilinear sampling; no stretch/replication/pixel copy.
inline PlayerVisualTextureUploadPlan BuildPlayerVisualTextureUploadPlan(
    const std::vector<PlayerVisualTexture>& textures,
    const PlayerVisualLimits& limits = {}) {
    PlayerVisualDetail::ValidateLimits(limits);
    if (textures.empty() || textures.size() > limits.maximumTextures)
        throw std::runtime_error("Player GPU texture count is outside its index budget");
    PlayerVisualTextureUploadPlan result;
    result.reserve(textures.size());
    std::size_t sourceBytes{};
    for (std::size_t index = 0u; index < textures.size(); ++index) {
        const auto& image = textures[index].image;
        if (image.width == 0u || image.height == 0u ||
            image.width > limits.maximumTextureDimension || image.height > limits.maximumTextureDimension)
            throw std::runtime_error("Player GPU source texture dimensions are outside its budget");
        const auto maximum = std::numeric_limits<std::size_t>::max();
        if (static_cast<std::size_t>(image.width) > maximum / image.height / 4u)
            throw std::runtime_error("Player GPU source texture byte count overflows");
        const auto bytes = static_cast<std::size_t>(image.width) * image.height * 4u;
        if (image.rgba.size() != bytes || bytes > limits.maximumTextureBytes ||
            sourceBytes > limits.maximumTextureBytes - bytes)
            throw std::runtime_error("Player GPU source texture pixels are malformed or exceed the byte budget");
        sourceBytes += bytes;
        result.push_back({static_cast<std::uint32_t>(index), image.width, image.height, bytes});
    }
    return result;
}

// Detached CPU preparation. Inputs contain actual decoded mesh transforms and
// original animation topology. This does not allocate a runtime player actor.
inline PlayerVisualAssets BuildPlayerVisualGeometry(const PortableLodMesh& body,
    const ActorTextureOverrides& bodySkins, const PortableLodMesh& glock,
    const PlayerVisualLimits& limits = {}) {
    using namespace PlayerVisualDetail;
    PlayerVisualAssets assets;
    ValidateLimits(limits);
    if (body.triangles.empty() || glock.triangles.empty() || body.triangles.size() % 3u || glock.triangles.size() % 3u)
        throw std::runtime_error("Original player meshes have invalid triangle topology");
    if (body.materialTextureIndices.size() <= 4u || body.materialTextureIndices[2] != 2 || body.materialTextureIndices[4] != 5)
        throw std::runtime_error("GM_Trench material/skin mapping does not match the audited original");
    const auto bodyPose = OriginalStillPose(body);
    const auto handPose = OriginalStillPose(glock);
    const auto transform = ObjectToQuestMeters();
    float lowestFoot = std::numeric_limits<float>::infinity();
    for (std::size_t first = 0u; first < body.triangles.size(); first += 3u) {
        const auto& source = body.triangles[first];
        const auto vertices = BuildActorTriangle(body, first, transform, &bodyPose);
        if (!LowerBodyTriangle(source.material, vertices)) continue;
        const auto selected = ResolveActorMeshMaterial(bodySkins, body.texturePaths, body.materialTextureIndices, source.material);
        const auto index = TextureIndex(assets, selected.texturePath, limits);
        Append(assets.lowerBody, {vertices, source.polyFlags, index, source.material}, limits);
        for (const auto& vertex : vertices) lowestFoot = std::min(lowestFoot, vertex.position.y);
    }
    if (assets.lowerBody.triangles.empty() || !std::isfinite(lowestFoot))
        throw std::runtime_error("Original player lower-body surfaces are unavailable");
    assets.lowerBody.originalPivotObjectUnits = {0.0f, 0.0f, lowestFoot * 52.5f};
    for (auto& triangle : assets.lowerBody.triangles)
        for (auto& vertex : triangle.vertices) vertex.position.y -= lowestFoot;
    for (const auto& triangle : assets.lowerBody.triangles) ValidateTriangle(triangle);
    assets.lowerBody.provenance = "DeusEx.JCDentonMale / DeusExCharacters.GM_Trench original Still: trousers, shoes and lower coat only; head, torso and arms omitted";

    std::set<std::uint32_t> gripVertices;
    for (std::size_t first = 0u; first < glock.triangles.size(); first += 3u) {
        const auto& source = glock.triangles[first];
        const auto selected = ResolveActorMeshMaterial({}, glock.texturePaths, glock.materialTextureIndices, source.material);
        if (selected.texturePath != OriginalHandTexture) continue;
        const auto vertices = BuildActorTriangle(glock, first, transform, &handPose);
        Append(assets.rightHand, {vertices, source.polyFlags, TextureIndex(assets, selected.texturePath, limits), source.material}, limits);
        if (source.material == 1u)
            for (std::size_t corner = 0u; corner < 3u; ++corner)
                gripVertices.insert(glock.animation->triangleSourceVertexIndices.at(first + corner));
    }
    if (assets.rightHand.triangles.empty() || gripVertices.empty())
        throw std::runtime_error("Glock original hand texture/material1 grip surfaces are unavailable");
    // The pivot is the audited material1 grasp's unique source-vertex centroid,
    // not the weapon origin, a guessed wrist bone, or a claimed authored joint.
    double gripX{}, gripY{}, gripZ{};
    for (const auto index : gripVertices) {
        const auto& point = handPose.objectPositions.at(index);
        if (!IsFiniteActorVector(point)) throw std::runtime_error("Original hand grip centroid is non-finite");
        gripX += point.x; gripY += point.y; gripZ += point.z;
    }
    const auto inverseCount = 1.0 / static_cast<double>(gripVertices.size());
    const ActorVec3 grip{static_cast<float>(gripX * inverseCount),
        static_cast<float>(gripY * inverseCount), static_cast<float>(gripZ * inverseCount)};
    if (!IsFiniteActorVector(grip)) throw std::runtime_error("Original hand grip centroid is outside finite float range");
    assets.rightHand.originalPivotObjectUnits = grip;
    const auto gripMeters = transform.TransformPoint(grip);
    for (auto& triangle : assets.rightHand.triangles)
        for (auto& vertex : triangle.vertices) {
            vertex.position.x -= gripMeters.x;
            vertex.position.y -= gripMeters.y;
            vertex.position.z -= gripMeters.z;
        }
    for (const auto& triangle : assets.rightHand.triangles) ValidateTriangle(triangle);
    assets.rightHand.provenance = "DeusExItems.Glock original Still WeaponHandsTex surfaces (sleeve plus grasp); material1 grasp centroid is controller-grip origin; rigid, no finger/arm IK";
    assets.leftHand = MirroredHand(assets.rightHand);
    const auto total = assets.lowerBody.triangles.size() + assets.rightHand.triangles.size() + assets.leftHand.triangles.size();
    if (total > limits.maximumTotalTriangles)
        throw std::runtime_error("Player visual total exceeds its triangle budget");
    return assets; // Texture pixels and passed are populated by the loader.
}

// Reads only the user's original System packages. No runtime overlays, map
// mutation, scripts, generated commercial asset files, or guessed rigs.
inline PlayerVisualAssets LoadOriginalPlayerVisualAssets(const std::string& gameRoot,
    const PlayerVisualLimits& limits = {}) {
    try {
        PlayerVisualDetail::ValidateLimits(limits);
        const auto system = std::filesystem::path(gameRoot) / "System";
        const auto player = LoadPortablePackageTables((system / "DeusEx.u").string());
        const auto characters = LoadPortablePackageTables((system / "DeusExCharacters.u").string());
        const auto items = LoadPortablePackageTables((system / "DeusExItems.u").string());
        const auto descriptor = LoadPortableClassDescriptor(player, FindPortableExport(player, "JCDentonMale"));
        ActorTextureOverrides skins;
        std::string playerMesh;
        for (const auto& property : descriptor.defaults) {
            if (property.type != 5u) continue;
            const auto path = PlayerVisualDetail::QualifiedReference(player, property);
            if (property.name == "Mesh") playerMesh = path;
            SetActorTextureOverride(skins, property.name.ToString(), property.arrayIndex, path);
        }
        if (playerMesh != "DeusExCharacters.GM_Trench" ||
            skins.multiSkins[2].path != "DeusExCharacters.Skins.JCDentonTex3" ||
            skins.multiSkins[5].path != "DeusExCharacters.Skins.JCDentonTex2")
            throw std::runtime_error("JCDentonMale original mesh/skin defaults differ from the audited assets");
        const auto bodyIndex = FindPortableExport(characters, "GM_Trench");
        const auto handIndex = FindPortableExport(items, "Glock");
        PlayerVisualDetail::CheckMeshExport(characters, bodyIndex, limits);
        PlayerVisualDetail::CheckMeshExport(items, handIndex, limits);
        auto body = LoadPortableLodMesh(characters, bodyIndex);
        auto hand = LoadPortableLodMesh(items, handIndex);
        for (auto* packageMesh : {&body, &hand}) {
            auto& mesh = *packageMesh;
            const auto& package = packageMesh == &body ? characters : items;
            for (const auto reference : mesh.textures) {
                auto path = GetPortableObjectPath(package, reference);
                if (reference > 0 && !path.empty()) path = std::filesystem::path(package.sourcePath).stem().string() + "." + path;
                mesh.texturePaths.push_back(std::move(path));
            }
        }
        auto assets = BuildPlayerVisualGeometry(body, skins, hand, limits);
        if (assets.lowerBody.triangles.size() != 147u || assets.rightHand.triangles.size() != 142u)
            throw std::runtime_error("Original player surface counts differ from the bounded audited selection");
        PlayerVisualDetail::DecodeTextures(assets, characters, items, limits);
        assets.passed = true;
        return assets;
    } catch (const std::exception& error) {
        PlayerVisualAssets failed;
        failed.error = error.what();
        return failed; // Failure retains no partial body, hands or texture array.
    }
}

} // namespace QuestVr
