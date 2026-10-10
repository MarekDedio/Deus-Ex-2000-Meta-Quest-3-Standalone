#pragma once

#include "quest_actor_geometry.h"
#include "quest_vr_hand_geometry.h"

#include <cstring>
#include <filesystem>
#include <limits>

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
    std::size_t originalTriangleCount{}, derivedClosureTriangleCount{};
};

struct PlayerVisualTexture {
    std::string texturePath;
    PortableTextureImage image;
    std::uint32_t polyFlags{};
};

struct PlayerVisualAssets {
    bool passed{};
    std::string error;
    // The retained lowerBody field now includes authored torso/chest/collar
    // coverage; its feet-local renderer attachment and callers stay unchanged.
    PlayerVisualPart lowerBody, rightHand, leftHand;
    std::vector<PlayerVisualTexture> textures;
};

struct PlayerVisualTextureUploadEntry {
    std::uint32_t textureIndex{}, width{}, height{};
    std::size_t rgbaByteCount{};
};
using PlayerVisualTextureUploadPlan = std::vector<PlayerVisualTextureUploadEntry>;

// Torso coverage replaces the old cut-off waist, so the body again honors
// authored PF_TwoSided instead of exposing every garment interior. Position
// the feet-local body behind the eyes for near-camera clearance; this function
// changes no geometry, flags, alpha/depth or existing hand culling behavior.
inline bool CullPlayerVisualPart(std::size_t part, std::uint32_t polyFlags) {
    if (part >= 3u) throw std::runtime_error("Player visual part index is outside the three prepared parts");
    return (polyFlags & 0x100u) == 0u;
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

inline PlayerVisualPart PrepareClosedHandPart(const VrHandGeometry& geometry,
    const PlayerVisualLimits& limits) {
    ValidateLimits(limits);
    if (geometry.originalProvenance.empty() || geometry.derivativeProvenance.empty() ||
        geometry.triangles.size()>limits.maximumTrianglesPerPart ||
        geometry.originalTriangleCount==0u || geometry.closureTriangleCount==0u ||
        geometry.originalTriangleCount>geometry.triangles.size() ||
        geometry.closureTriangleCount!=geometry.triangles.size()-geometry.originalTriangleCount ||
        !IsFiniteActorVector(geometry.originalPivotObjectUnits))
        throw std::runtime_error("Player hand original/derivative metadata is incomplete");
    VrHandGeometryDetail::Inspect(geometry.triangles,{},true);
    PlayerVisualPart part;
    part.provenance=geometry.originalProvenance+"; "+geometry.derivativeProvenance;
    part.originalPivotObjectUnits=geometry.originalPivotObjectUnits;
    part.derivedMirrored=geometry.derivedMirrored;
    for (const auto& face:geometry.triangles) {
        Append(part,{face.vertices,face.polyFlags,face.textureIndex,face.sourceMaterial},limits);
        if (face.origin==VrHandFaceOrigin::OriginalSurface) ++part.originalTriangleCount;
        else if (face.origin==VrHandFaceOrigin::DerivedSleeveClosure) ++part.derivedClosureTriangleCount;
        else throw std::runtime_error("Player hand has an unknown face provenance");
    }
    if (part.originalTriangleCount!=geometry.originalTriangleCount ||
        part.derivedClosureTriangleCount!=geometry.closureTriangleCount)
        throw std::runtime_error("Player hand faces disagree with original/derivative provenance");
    return part;
}

// Historical lower surfaces are emitted first to retain their original source
// ordering and texture indices. This is not the final body selection: the
// complete upper coat/collar/chest is appended below without geometric cuts.
inline bool LowerBodyTriangle(std::uint16_t material,
    const std::array<ActorTriangleVertex, 3>& triangle) {
    if (material == 2u) return true;
    if (material != 4u) return false;
    for (const auto& vertex : triangle)
        if (vertex.position.y > 12.0f / 52.5f) return false;
    return true;
}

// GM_Trench Still's authored material1 UV atlas separates 64 torso/back/shoulder
// faces (all V<=112/255) from 60 sleeve/arm faces (all V>=182/255), with no mixed
// triangles. The unused 0.5 gap selects whole original faces, not clipped arms
// or a fabricated waist cap. Material3 is the 27-face chest/neck. Material2 is
// trousers/shoes and material4 includes all33 lower-coat/collar faces. Material0
// contains head plus static hands, and materials5/6 are glasses: all omitted.
inline bool SelfBodyTriangle(std::uint16_t material,
    const std::array<ActorTriangleVertex,3>& triangle) {
    if (material==2u || material==3u || material==4u) return true;
    if (material!=1u) return false;
    for (const auto& vertex:triangle) if (vertex.v>0.5f) return false;
    return true;
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
inline PlayerVisualAssets BuildPlayerBodyGeometry(const PortableLodMesh& body,
    const ActorTextureOverrides& bodySkins,
    const PlayerVisualLimits& limits = {}) {
    using namespace PlayerVisualDetail;
    PlayerVisualAssets assets;
    ValidateLimits(limits);
    if (body.triangles.empty() || body.triangles.size() % 3u)
        throw std::runtime_error("Original player body has invalid triangle topology");
    if (body.materialTextureIndices.size() <= 4u || body.materialTextureIndices[1] != 1 || body.materialTextureIndices[2] != 2 ||
        body.materialTextureIndices[3] != 4 || body.materialTextureIndices[4] != 5)
        throw std::runtime_error("GM_Trench material/skin mapping does not match the audited original");
    const auto bodyPose = OriginalStillPose(body);
    const auto transform = ObjectToQuestMeters();
    float lowestFoot = std::numeric_limits<float>::infinity();
    for (std::size_t pass=0u;pass<2u;++pass) for (std::size_t first = 0u; first < body.triangles.size(); first += 3u) {
        const auto& source = body.triangles[first];
        const auto vertices = BuildActorTriangle(body, first, transform, &bodyPose);
        if (!SelfBodyTriangle(source.material, vertices) || LowerBodyTriangle(source.material,vertices)!=(pass==0u)) continue;
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
    assets.lowerBody.provenance = "DeusEx.JCDentonMale / DeusExCharacters.GM_Trench original Still: trousers, shoes, complete authored coat torso/back, chest and collar; static sleeves, hands, head and glasses omitted; no generated waist cap or IK";

    assets.lowerBody.originalTriangleCount=assets.lowerBody.triangles.size();
    if (assets.lowerBody.triangles.size() > limits.maximumTotalTriangles)
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
            skins.multiSkins[1].path != "DeusExCharacters.Skins.JCDentonTex2" ||
            skins.multiSkins[2].path != "DeusExCharacters.Skins.JCDentonTex3" ||
            skins.multiSkins[4].path != "DeusExCharacters.Skins.JCDentonTex1" ||
            skins.multiSkins[5].path != "DeusExCharacters.Skins.JCDentonTex2")
            throw std::runtime_error("JCDentonMale original mesh/skin defaults differ from the audited assets");
        const auto bodyIndex = FindPortableExport(characters, "GM_Trench");
        const auto handIndex = FindPortableExport(items, "NanoKeyRingPOV");
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
        auto assets = BuildPlayerBodyGeometry(body, skins, limits);
        const auto handTexture=PlayerVisualDetail::TextureIndex(assets,
            PlayerVisualDetail::OriginalHandTexture,limits);
        const auto right=BuildOriginalNanoKeyRingVrHand(hand,handTexture);
        const auto left=MirrorClosedVrHandGeometry(right);
        assets.rightHand=PlayerVisualDetail::PrepareClosedHandPart(right,limits);
        assets.leftHand=PlayerVisualDetail::PrepareClosedHandPart(left,limits);
        std::array<std::size_t,5> bodyMaterials{};
        for (const auto& triangle:assets.lowerBody.triangles) {
            if (triangle.sourceMaterial>=bodyMaterials.size()) throw std::runtime_error("Original self-body retained head/glasses material");
            ++bodyMaterials[triangle.sourceMaterial];
        }
        if (assets.lowerBody.triangles.size() != 251u || bodyMaterials!=std::array<std::size_t,5>{0u,64u,127u,27u,33u} ||
            assets.rightHand.originalTriangleCount!=152u || assets.rightHand.derivedClosureTriangleCount!=4u ||
            assets.leftHand.originalTriangleCount!=152u || assets.leftHand.derivedClosureTriangleCount!=4u ||
            assets.lowerBody.triangles.size()+assets.rightHand.triangles.size()+assets.leftHand.triangles.size()>
                limits.maximumTotalTriangles)
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
