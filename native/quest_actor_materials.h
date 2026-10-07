#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace QuestVr {

// A present null reference is not an absent property: it suppresses a more
// distant class default at this index. It does not suppress the mesh renderer's
// later Skin / mesh / Texture / last-MultiSkin fallback rules.
struct ActorTextureOverride {
    bool specified{};
    std::string path;
};

struct ActorTextureOverrides {
    ActorTextureOverride skin;
    ActorTextureOverride texture;
    std::array<ActorTextureOverride, 8> multiSkins;
};

inline bool SetActorTextureOverride(
    ActorTextureOverrides& overrides,
    const std::string& propertyName,
    std::uint32_t arrayIndex,
    std::string path) {
    ActorTextureOverride* destination{};
    if (propertyName == "MultiSkins" && arrayIndex < overrides.multiSkins.size()) {
        destination = &overrides.multiSkins[arrayIndex];
    } else if (arrayIndex == 0u && propertyName == "Skin") {
        destination = &overrides.skin;
    } else if (arrayIndex == 0u && propertyName == "Texture") {
        destination = &overrides.texture;
    }
    if (destination == nullptr) return false;
    destination->specified = true;
    destination->path = std::move(path);
    return true;
}

// Call nearest-to-farthest: instance, immediate class, then successive bases.
// Each fixed-array element inherits independently, including explicit None.
inline void InheritActorTextureOverrides(
    ActorTextureOverrides& nearest,
    const ActorTextureOverrides& moreDistant) {
    const auto inherit = [](ActorTextureOverride& target, const ActorTextureOverride& source) {
        if (!target.specified && source.specified) target = source;
    };
    inherit(nearest.skin, moreDistant.skin);
    inherit(nearest.texture, moreDistant.texture);
    for (std::size_t index = 0; index < nearest.multiSkins.size(); ++index)
        inherit(nearest.multiSkins[index], moreDistant.multiSkins[index]);
}

enum class ActorMaterialSource : std::uint8_t {
    None, MultiSkin, Skin, Mesh, Texture, LastMultiSkin
};

struct ActorMaterialSelection {
    bool validMaterial{};
    std::int32_t textureIndex{-1};
    ActorMaterialSource source{ActorMaterialSource::None};
    std::string texturePath;
};

// Mirrors pinned VisibleMesh::SetupMeshTextures / SetupLodMeshTextures.
// A LOD face's material ordinal is NOT the MultiSkins index: first map it via
// MeshMaterial.TextureIndex. GetMultiskin itself is limited to eight entries.
inline ActorMaterialSelection ResolveActorMeshMaterial(
    const ActorTextureOverrides& overrides,
    const std::vector<std::string>& meshTexturePaths,
    const std::vector<std::int32_t>& materialTextureIndices,
    std::size_t material) {
    ActorMaterialSelection result;
    const bool lodMesh = !materialTextureIndices.empty();
    if (lodMesh) {
        if (material >= materialTextureIndices.size()) return result;
        result.textureIndex = materialTextureIndices[material];
    } else {
        if (material >= meshTexturePaths.size()) return result;
        result.textureIndex = static_cast<std::int32_t>(material);
    }
    if (result.textureIndex < 0 ||
        static_cast<std::size_t>(result.textureIndex) >= meshTexturePaths.size())
        return result;
    result.validMaterial = true;
    const auto index = static_cast<std::size_t>(result.textureIndex);
    const auto nonNull = [](const ActorTextureOverride& value) -> const std::string* {
        return value.specified && !value.path.empty() ? &value.path : nullptr;
    };
    const auto choose = [&](const std::string& path, ActorMaterialSource source) {
        result.texturePath = path;
        result.source = source;
    };
    if (index < overrides.multiSkins.size()) {
        if (const auto* path = nonNull(overrides.multiSkins[index])) {
            choose(*path, ActorMaterialSource::MultiSkin);
            return result;
        }
    }
    const auto& meshDefault = meshTexturePaths[index];
    if (index == 0u || meshDefault.empty()) {
        if (const auto* path = nonNull(overrides.skin)) {
            choose(*path, ActorMaterialSource::Skin);
            return result;
        }
    }
    if (!meshDefault.empty()) {
        choose(meshDefault, ActorMaterialSource::Mesh);
        return result;
    }
    if (const auto* path = nonNull(overrides.texture)) {
        choose(*path, ActorMaterialSource::Texture);
        return result;
    }
    const auto fallbackCount = std::min(overrides.multiSkins.size(),
        lodMesh ? materialTextureIndices.size() : meshTexturePaths.size());
    for (std::size_t fallback = 0; fallback < fallbackCount; ++fallback) {
        if (const auto* path = nonNull(overrides.multiSkins[fallback]))
            choose(*path, ActorMaterialSource::LastMultiSkin);
    }
    return result;
}

// Actor-level bits used by both the desktop and Quest actor draw paths. Mesh
// polygon flags and the selected texture's PF_Masked are combined separately.
inline std::uint32_t ActorMaterialPolyFlags(
    std::uint8_t style, bool unlit, bool noSmooth, bool meshEnvironmentMap) {
    std::uint32_t flags{};
    if (style == 2u) flags |= 0x00000002u; // PF_Masked
    else if (style == 3u) flags |= 0x00000004u; // PF_Translucent
    else if (style == 4u) flags |= 0x00000040u; // PF_Modulated
    if (unlit) flags |= 0x00400000u;
    if (noSmooth) flags |= 0x00000800u;
    if (meshEnvironmentMap) flags |= 0x00000010u;
    return flags;
}

// Keep the palette color for opaque use. The alpha channel encodes the exact
// UE1 P8 masked key (index zero); masking is a material decision, not a guessed
// magenta border color. Opaque material shaders must ignore this alpha.
inline std::array<std::uint8_t, 4> ActorPaletteTexel(
    std::uint32_t color, std::uint8_t paletteIndex) {
    return {static_cast<std::uint8_t>(color),
        static_cast<std::uint8_t>(color >> 8u),
        static_cast<std::uint8_t>(color >> 16u),
        paletteIndex == 0u ? std::uint8_t{0u} : std::uint8_t{255u}};
}

} // namespace QuestVr
