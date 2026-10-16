#include "quest_mesh_texture_selection.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

// Pure priority/index controls; no native dispatch, package loading, renderer
// material mapping, original DLL execution or live crate destruction claim.
namespace {
using QuestVr::ChooseOriginalMeshTexture;
using QuestVr::MeshTextureSelection;
using QuestVr::MeshTextureSource;
using Views = std::array<std::string_view, 8u>;
std::size_t checks{};
void Require(const bool condition, const std::string& description) {
    if (!condition) throw std::runtime_error(description);
    ++checks;
}
constexpr Views MultiNames{"Multi0", "Multi1", "Multi2", "Multi3", "Multi4", "Multi5", "Multi6", "Multi7"};
constexpr std::array<std::string_view, 12u> MeshNames{"Mesh0", "Mesh1", "Mesh2", "Mesh3", "Mesh4", "Mesh5",
    "Mesh6", "Mesh7", "Mesh8", "Mesh9", "Mesh10", "Mesh11"};
constexpr std::string_view SkinName = "Skin";
constexpr auto CompileTime = ChooseOriginalMeshTexture(true, 0, Views{}, SkinName, MeshNames);
static_assert(CompileTime.texture == "Skin" && CompileTime.source == MeshTextureSource::Skin);
static_assert(noexcept(ChooseOriginalMeshTexture(true, 0, Views{}, {}, {})));
static_assert(std::is_trivially_copyable_v<MeshTextureSelection>);

// Independent branch tree written in the retail native's return-site order.
// It intentionally does not use the helper's shared index/validity variables.
MeshTextureSelection Reference(const bool mesh, const std::int32_t slot,
    const Views& overrides, const std::string_view skin, const std::span<const std::string_view> textures) {
    if (!mesh) return {};
    if (slot >= 0 && slot <= 7)
        if (overrides[static_cast<unsigned>(slot)].size() != 0u)
            return {overrides[static_cast<unsigned>(slot)], MeshTextureSource::MultiSkin, static_cast<unsigned>(slot)};
    if (slot >= 1 && static_cast<std::uint32_t>(slot) < textures.size())
        if (textures[static_cast<std::uint32_t>(slot)].size() != 0u)
            return {textures[static_cast<std::uint32_t>(slot)], MeshTextureSource::MeshTexture, static_cast<std::uint32_t>(slot)};
    if (skin.size() != 0u) return {skin, MeshTextureSource::Skin, MeshTextureSelection::NoSlot};
    if (slot >= 0 && static_cast<std::uint32_t>(slot) < textures.size())
        return {textures[static_cast<std::uint32_t>(slot)], MeshTextureSource::MeshTexture, static_cast<std::uint32_t>(slot)};
    return {};
}
void Same(const MeshTextureSelection& actual, const MeshTextureSelection& expected, const char* description) {
    Require(actual.texture == expected.texture && actual.source == expected.source &&
        actual.sourceIndex == expected.sourceIndex, description);
    if (!expected.texture.empty())
        Require(actual.texture.data() == expected.texture.data() && actual.texture.size() == expected.texture.size(),
            "Mesh texture selector copied, normalized or substituted the borrowed identity");
}
void ExhaustivePresence() {
    constexpr std::array<std::int32_t, 21u> slots{
        std::numeric_limits<std::int32_t>::min(), -32768, -8, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 32767, 1'000'000, std::numeric_limits<std::int32_t>::max()};
    // Exhaust every MultiSkins presence mask and every combination of six
    // mesh slots. Longer arrays also exercise slots beyond the eight overrides.
    for (unsigned multiMask = 0u; multiMask < 256u; ++multiMask) {
        Views multi;
        for (std::size_t i = 0u; i < multi.size(); ++i) if ((multiMask >> i) & 1u) multi[i] = MultiNames[i];
        for (unsigned meshMask = 0u; meshMask < 64u; ++meshMask) {
            std::array<std::string_view, 12u> textures;
            for (std::size_t i = 0u; i < textures.size(); ++i)
                if ((meshMask >> (i % 6u)) & 1u) textures[i] = MeshNames[i];
            for (const std::size_t length : {0u, 1u, 6u, 7u, 8u, 9u, 12u}) {
                const auto span = std::span<const std::string_view>(textures.data(), length);
                for (const auto slot : slots) for (const bool mesh : {false, true}) for (const bool hasSkin : {false, true}) {
                    const std::string_view skin = hasSkin ? SkinName : std::string_view{};
                    Same(ChooseOriginalMeshTexture(mesh, slot, multi, skin, span),
                        Reference(mesh, slot, multi, skin, span), "Retail mesh texture priority/index presence combination differs");
                }
            }
        }
    }
}
void NamedBoundaries() {
    Same(ChooseOriginalMeshTexture(false, 3, MultiNames, SkinName, MeshNames), {},
        "Mesh None failed its early return before MultiSkins/Skin");
    Same(ChooseOriginalMeshTexture(true, 0, {}, SkinName, MeshNames),
        {SkinName, MeshTextureSource::Skin, MeshTextureSelection::NoSlot}, "Skin does not precede mesh slot zero");
    Same(ChooseOriginalMeshTexture(true, 1, {}, SkinName, MeshNames),
        {MeshNames[1u], MeshTextureSource::MeshTexture, 1u}, "Positive mesh slot does not precede Skin");
    Same(ChooseOriginalMeshTexture(true, 0, {}, {}, MeshNames),
        {MeshNames[0u], MeshTextureSource::MeshTexture, 0u}, "Slot zero mesh fallback was lost without Skin");
    Same(ChooseOriginalMeshTexture(true, 7, MultiNames, SkinName, {}),
        {MultiNames[7u], MeshTextureSource::MultiSkin, 7u}, "MultiSkins selection incorrectly depends on mesh texture count");
    Same(ChooseOriginalMeshTexture(true, 8, MultiNames, SkinName, MeshNames),
        {MeshNames[8u], MeshTextureSource::MeshTexture, 8u}, "Slot eight wrapped/clamped into eight-element MultiSkins");
    for (const auto slot : {std::numeric_limits<std::int32_t>::min(), -1, 12, std::numeric_limits<std::int32_t>::max()}) {
        Same(ChooseOriginalMeshTexture(true, slot, MultiNames, SkinName, MeshNames),
            {SkinName, MeshTextureSource::Skin, MeshTextureSelection::NoSlot}, "Invalid mesh index did not retain legal Skin branch");
        Same(ChooseOriginalMeshTexture(true, slot, MultiNames, {}, MeshNames), {},
            "Invalid mesh index manufactured last-MultiSkin/default/other texture fallback");
    }
    const std::array<std::string_view, 2u> none{};
    Same(ChooseOriginalMeshTexture(true, 1, {}, {}, none),
        {{}, MeshTextureSource::MeshTexture, 1u}, "Valid None mesh slot lost branch provenance or acquired fallback");
    Same(ChooseOriginalMeshTexture(true, 1, {}, SkinName, none),
        {SkinName, MeshTextureSource::Skin, MeshTextureSelection::NoSlot}, "Positive null mesh slot did not fall back to Skin");
}
void RawTextureIndicesAndIdentity() {
    // The O(1) native-facing interface must not allocate/iterate a fake whole
    // table even when its advertised count exceeds a realizable span.
    constexpr auto largest=std::numeric_limits<std::int32_t>::max();
    Same(QuestVr::ChooseOriginalMeshTextureSlot(true,largest,{},SkinName,
        std::numeric_limits<std::size_t>::max(),MeshNames[0u]),
        {MeshNames[0u],MeshTextureSource::MeshTexture,static_cast<std::size_t>(largest)},
        "O(1) raw-slot interface clamped a legal positive index/count");
    Same(QuestVr::ChooseOriginalMeshTextureSlot(true,-1,{},SkinName,
        std::numeric_limits<std::size_t>::max(),MeshNames[0u]),
        {SkinName,MeshTextureSource::Skin,MeshTextureSelection::NoSlot},
        "O(1) raw-slot interface wrapped a negative index into its advertised table");
    Same(QuestVr::ChooseOriginalMeshTextureSlot(false,largest,MultiNames,SkinName,
        std::numeric_limits<std::size_t>::max(),MeshNames[0u]),{},
        "O(1) raw-slot interface consulted assets without a Mesh");
    // A render material ordinal can reference an unrelated raw texture slot.
    // This native takes the raw index directly and never consults face/material
    // ordinals, triangle order or the renderer's broader texture fallbacks.
    constexpr std::array<std::int32_t, 4u> materialTextureIndices{6, 0, 9, 4};
    for (std::size_t material = 0u; material < materialTextureIndices.size(); ++material) {
        const auto raw = materialTextureIndices[material];
        const auto expected = raw < 8 ? MeshTextureSelection{MultiNames[static_cast<std::size_t>(raw)],
            MeshTextureSource::MultiSkin, static_cast<std::size_t>(raw)} :
            MeshTextureSelection{MeshNames[static_cast<std::size_t>(raw)], MeshTextureSource::MeshTexture, static_cast<std::size_t>(raw)};
        Same(ChooseOriginalMeshTexture(true, raw, MultiNames, SkinName, MeshNames), expected,
            "Native texture lookup remapped a raw texture index as a material ordinal");
        Require(raw != static_cast<std::int32_t>(material), "Raw-index regression does not distinguish the material ordinal");
    }
    const std::string embedded("CaseSensitive.Texture\0Tail", 26u);
    Views multi;
    multi[4u] = std::string_view(embedded.data(), embedded.size());
    Same(ChooseOriginalMeshTexture(true, 4, multi, SkinName, MeshNames),
        {multi[4u], MeshTextureSource::MultiSkin, 4u}, "Texture identity was treated as a C-string or normalized");
    std::string first("Same.Path"), second("Same.Path");
    multi[2u] = first;
    std::array<std::string_view, 3u> textures{};
    textures[2u] = second;
    const auto chosen = ChooseOriginalMeshTexture(true, 2, multi, first, textures);
    Same(chosen, {multi[2u], MeshTextureSource::MultiSkin, 2u}, "Equal labels lost selected source identity/provenance");
    Require(chosen.texture.data() != textures[2u].data(), "Identity regression aliases two independent input buffers");
}
} // namespace
int main() {
    try {
        NamedBoundaries(); RawTextureIndicesAndIdentity(); ExhaustivePresence();
        std::cout << "PASS original GetMeshTexture1013 selection: " << checks
                  << " priority, None, signed bounds, eight-slot and borrowed raw-identity controls. "
                     "No renderer/default-texture substitution or native/gameplay claim.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Original mesh texture selection failed: " << error.what() << '\n'; return 1;
    }
}
