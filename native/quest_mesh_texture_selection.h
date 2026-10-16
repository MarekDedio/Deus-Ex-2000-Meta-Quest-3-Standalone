#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace QuestVr {

enum class MeshTextureSource : std::uint8_t {
    None, MultiSkin, Skin, MeshTexture
};

struct MeshTextureSelection {
    static constexpr std::size_t NoSlot = std::numeric_limits<std::size_t>::max();
    // Borrowed identity only. The caller owns the referenced UObject/path and
    // its lifetime; this helper neither loads assets nor changes ownership.
    std::string_view texture;
    MeshTextureSource source{MeshTextureSource::None};
    std::size_t sourceIndex{NoSlot};
};

// Retail GOTY Actor.GetMeshTexture1013, not the renderer's material resolver.
// Engine.dll thunk10302f9a -> body103e17c0: a missing Mesh returns None even
// when Skin/MultiSkins is present. With a Mesh, actual GetMultiskin(texnum)
// precedes the positive mesh-slot branch, then Skin, then any valid mesh slot.
// texnum is the RAW Mesh.Textures / MultiSkins index, never a material ordinal.
// Negative and huge ordinary Int inputs are legal; they can still select Skin.
// Empty views represent actual Object None, not a missing/unresolved field.
// The runtime caller must validate schemas and referenced object identities.
inline constexpr MeshTextureSelection ChooseOriginalMeshTextureSlot(
    const bool meshPresent,
    const std::int32_t texnum,
    const std::array<std::string_view, 8u>& multiSkins,
    const std::string_view skin,
    const std::size_t meshTextureCount,
    const std::string_view requestedMeshTexture) noexcept {
    if (!meshPresent) return {};
    // Signed negatives must not wrap into an unchecked array dereference.
    // No arbitrary index clamp changes the original valid Skin/None branches.
    const bool nonnegative = texnum >= 0;
    const auto index = nonnegative ? static_cast<std::size_t>(texnum) : MeshTextureSelection::NoSlot;
    if (index < multiSkins.size() && !multiSkins[index].empty())
        return {multiSkins[index], MeshTextureSource::MultiSkin, index};
    const bool meshSlot = nonnegative && index < meshTextureCount;
    if (texnum > 0 && meshSlot && !requestedMeshTexture.empty())
        return {requestedMeshTexture, MeshTextureSource::MeshTexture, index};
    if (!skin.empty()) return {skin, MeshTextureSource::Skin, MeshTextureSelection::NoSlot};
    // A valid null mesh slot preserves its branch provenance while returning
    // Object None. There is no Actor.Texture, last-MultiSkin or DefaultTexture
    // fallback in this retail native.
    if (meshSlot) return {requestedMeshTexture, MeshTextureSource::MeshTexture, index};
    return {};
}

// The native already owns an immutable slot table: it can pass just its count
// and the one bounds-checked requested slot to the O(1) core above. No temporary
// whole-table view allocation or unrelated slot traversal is required.
inline constexpr MeshTextureSelection ChooseOriginalMeshTexture(
    const bool meshPresent,
    const std::int32_t texnum,
    const std::array<std::string_view, 8u>& multiSkins,
    const std::string_view skin,
    const std::span<const std::string_view> meshTextures) noexcept {
    const auto requested=texnum>=0 && static_cast<std::size_t>(texnum)<meshTextures.size() ?
        meshTextures[static_cast<std::size_t>(texnum)] : std::string_view{};
    return ChooseOriginalMeshTextureSlot(meshPresent,texnum,multiSkins,skin,meshTextures.size(),requested);
}

} // namespace QuestVr
