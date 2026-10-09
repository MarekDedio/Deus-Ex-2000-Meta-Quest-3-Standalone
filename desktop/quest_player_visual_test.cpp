#include "Precomp.h"
#include "quest_player_visual.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::size_t checks{}, rejections{};
void Require(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class F> void Reject(F operation) {
    bool rejected{};
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, "Expected bounded player visual rejection");
    ++rejections;
}
PortableLodMesh Mesh(const std::vector<std::array<PortablePackedMeshVertex, 3>>& triangles,
    const std::vector<std::uint16_t>& materials) {
    PortableLodMesh mesh;
    mesh.scaleX = mesh.scaleY = mesh.scaleZ = 1.0f;
    auto data = std::make_shared<PortableMeshAnimationData>();
    data->scale = {1.0f, 1.0f, 1.0f};
    data->lodMesh = true;
    data->animationFrames = 1u;
    data->frameVertices = static_cast<std::uint32_t>(triangles.size() * 3u);
    data->sequences.push_back({"Still", "", 0, 1, 30.0f, {}, false});
    for (std::size_t triangle = 0u; triangle < triangles.size(); ++triangle) {
        const auto first = static_cast<std::uint32_t>(data->frameVerticesPacked.size());
        data->normalTopology.push_back({first, first + 1u, first + 2u});
        for (std::size_t corner = 0u; corner < 3u; ++corner) {
            const auto& v = triangles[triangle][corner];
            data->frameVerticesPacked.push_back(v);
            data->triangleSourceVertexIndices.push_back(first + static_cast<std::uint32_t>(corner));
            mesh.triangles.push_back({static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z),
                static_cast<float>(corner == 1u), static_cast<float>(corner == 2u), materials.at(triangle), 0, 0, 1, 0u});
        }
    }
    mesh.frameVertices = data->frameVertices;
    mesh.animationFrames = 1u;
    mesh.animation = std::move(data);
    return mesh;
}
void TextureUploadContracts() {
    std::vector<QuestVr::PlayerVisualTexture> textures;
    for (const auto dimensions : {std::array<std::uint32_t,2>{1u,2u}, {2u,4u}, {4u,4u}}) {
        QuestVr::PlayerVisualTexture texture;
        texture.texturePath = "synthetic.layer." + std::to_string(textures.size());
        texture.image.width = dimensions[0]; texture.image.height = dimensions[1];
        for (std::uint32_t y = 0u; y < dimensions[1]; ++y)
            for (std::uint32_t x = 0u; x < dimensions[0]; ++x) {
                texture.image.rgba.push_back(static_cast<std::uint8_t>(x));
                texture.image.rgba.push_back(static_cast<std::uint8_t>(y));
                texture.image.rgba.push_back(static_cast<std::uint8_t>(textures.size()));
                texture.image.rgba.push_back(x == 0u && y == 0u ? 0u : 255u);
            }
        textures.push_back(std::move(texture));
    }
    const auto unchanged = textures;
    const auto plan = QuestVr::BuildPlayerVisualTextureUploadPlan(textures);
    Require(plan.size() == 3u, "Mixed native texture dimensions did not produce three upload entries");
    for (std::size_t layer = 0u; layer < textures.size(); ++layer) {
        const auto& image = textures[layer].image;
        Require(image.rgba == unchanged[layer].image.rgba && image.width == unchanged[layer].image.width &&
            image.height == unchanged[layer].image.height, "GPU preparation edited native source textures");
        Require(plan[layer].textureIndex == layer && plan[layer].width == image.width &&
            plan[layer].height == image.height && plan[layer].rgbaByteCount == image.rgba.size(),
            "Native upload dimensions, unchanged index order or RGBA byte count changed");
    }
    auto limits = QuestVr::PlayerVisualLimits{};
    limits.maximumTextureBytes = 104u; // Exact aggregate native source bytes.
    Require(QuestVr::BuildPlayerVisualTextureUploadPlan(textures, limits).size() == 3u,
        "Exact aggregate retained-byte boundary should be accepted");
    limits.maximumTextureBytes = 103u;
    Reject([&] { QuestVr::BuildPlayerVisualTextureUploadPlan(textures, limits); });
    limits = {}; limits.maximumTextures = 2u;
    Reject([&] { QuestVr::BuildPlayerVisualTextureUploadPlan(textures, limits); });
    Reject([&] { QuestVr::BuildPlayerVisualTextureUploadPlan({}); });
    auto malformed = textures; malformed[0].image.rgba.pop_back();
    Reject([&] { QuestVr::BuildPlayerVisualTextureUploadPlan(malformed); });
    malformed = textures; malformed[0].image.width = 0u;
    Reject([&] { QuestVr::BuildPlayerVisualTextureUploadPlan(malformed); });
    malformed = textures; malformed[0].image.height = 0u;
    Reject([&] { QuestVr::BuildPlayerVisualTextureUploadPlan(malformed); });
    malformed = textures; malformed[0].image.width = 2049u;
    Reject([&] { QuestVr::BuildPlayerVisualTextureUploadPlan(malformed); });
    malformed = textures; malformed[0].image.width = malformed[0].image.height = std::numeric_limits<std::uint32_t>::max();
    Reject([&] { QuestVr::BuildPlayerVisualTextureUploadPlan(malformed); });
    malformed = textures; malformed[0].image.width = 3u; malformed[0].image.rgba.resize(24u);
    Require(QuestVr::BuildPlayerVisualTextureUploadPlan(malformed).front().width == 3u,
        "Native-size grouping must permit noninteger dimension ratios without resizing");
    limits = {}; limits.maximumTextureDimension = 3u;
    Reject([&] { QuestVr::BuildPlayerVisualTextureUploadPlan(textures, limits); });
    limits = {}; limits.maximumTextures = 0u;
    Reject([&] { QuestVr::BuildPlayerVisualTextureUploadPlan(textures, limits); });
    const std::vector<QuestVr::PlayerVisualTexture> tooMany(256u);
    Reject([&] { QuestVr::BuildPlayerVisualTextureUploadPlan(tooMany); });
}
void Synthetic() {
    TextureUploadContracts();
    Require(!QuestVr::CullPlayerVisualPart(0u,0u), "Self-view lower body must not cull open-waist interiors");
    Require(!QuestVr::CullPlayerVisualPart(0u,258u), "Own lower-coat two-sided/masked state must remain uncullable");
    for (const auto hand : {1u,2u}) {
        Require(QuestVr::CullPlayerVisualPart(hand,0u), "Opaque original hand surfaces must retain culling");
        const std::uint32_t flags = 258u;
        Require(!QuestVr::CullPlayerVisualPart(hand,flags) && flags == 258u,
            "Hand two-sided/masked source flags must remain unchanged");
    }
    Reject([&] { QuestVr::CullPlayerVisualPart(3u,0u); });
    auto body = Mesh({{{{0,0,-47},{10,0,-47},{0,10,10}}}, {{{0,0,-32},{10,0,-32},{0,10,11}}},
        {{{0,0,26},{10,0,26},{0,10,38}}}, {{{0,0,35},{10,0,35},{0,10,47}}}}, {2,4,4,0});
    body.texturePaths.assign(8u, "");
    body.materialTextureIndices = {0,1,2,4,5,6,7};
    QuestVr::ActorTextureOverrides skins;
    skins.multiSkins[2] = {true, "DeusExCharacters.Skins.JCDentonTex3"};
    skins.multiSkins[5] = {true, "DeusExCharacters.Skins.JCDentonTex2"};
    auto hand = Mesh({{{{0,0,0},{10,0,0},{0,10,0}}}, {{{10,0,0},{20,0,0},{10,10,0}}},
        {{{100,100,100},{110,100,100},{100,110,100}}}}, {0,1,2});
    hand.texturePaths = {QuestVr::PlayerVisualDetail::OriginalHandTexture,
        QuestVr::PlayerVisualDetail::OriginalHandTexture, "DeusExItems.Skins.GlockTex1"};
    hand.materialTextureIndices = {0,1,2};
    const auto assets = QuestVr::BuildPlayerVisualGeometry(body, skins, hand);
    Require(!assets.passed && assets.error.empty(), "Geometry-only preparation must not claim decoded texture success");
    Require(assets.lowerBody.triangles.size() == 2u, "Head and collar must be excluded, legs/lower coat retained");
    Require(assets.rightHand.triangles.size() == 2u && assets.leftHand.triangles.size() == 2u,
        "Weapon surface must be omitted from original hand extraction");
    Require(assets.textures.size() == 3u, "Textures must be shared between both hand variants");
    Require(!assets.rightHand.derivedMirrored && assets.leftHand.derivedMirrored, "Left variant provenance is required");
    Require(std::fabs(assets.rightHand.originalPivotObjectUnits.x - 40.0f/3.0f) < 0.0001f,
        "Controller origin must use original unique material1 grasp vertices");
    float lowest = std::numeric_limits<float>::infinity();
    for (const auto& triangle : assets.lowerBody.triangles) for (const auto& vertex : triangle.vertices)
        lowest = std::min(lowest, vertex.position.y);
    Require(std::fabs(lowest) < 0.00001f, "Lower body must be feet-centered without actor spawn offset");
    for (std::size_t i = 0u; i < assets.rightHand.triangles.size(); ++i) {
        const auto& right = assets.rightHand.triangles[i];
        const auto& left = assets.leftHand.triangles[i];
        const std::size_t order[3]{0u,2u,1u};
        for (std::size_t c = 0u; c < 3u; ++c) {
            const auto& a = right.vertices[order[c]]; const auto& b = left.vertices[c];
            Require(a.position.x == -b.position.x && a.position.y == b.position.y && a.position.z == b.position.z,
                "Mirrored hand positions/winding differ from the original source");
            Require(a.normal.x == -b.normal.x && a.normal.y == b.normal.y && a.normal.z == b.normal.z,
                "Mirrored hand normal reflection is incorrect");
            Require(a.u == b.u && a.v == b.v, "Original mirrored hand UVs must be preserved");
        }
    }
    auto limits = QuestVr::PlayerVisualLimits{};
    limits.maximumTrianglesPerPart = 1u;
    Reject([&] { QuestVr::BuildPlayerVisualGeometry(body, skins, hand, limits); });
    limits = {}; limits.maximumTotalTriangles = 5u;
    Reject([&] { QuestVr::BuildPlayerVisualGeometry(body, skins, hand, limits); });
    limits = {}; limits.maximumTextures = 2u;
    Reject([&] { QuestVr::BuildPlayerVisualGeometry(body, skins, hand, limits); });
    limits = {}; limits.maximumTextureDimension = 0u;
    Reject([&] { QuestVr::BuildPlayerVisualGeometry(body, skins, hand, limits); });
    auto bad = body; bad.materialTextureIndices[2] = 3;
    Reject([&] { QuestVr::BuildPlayerVisualGeometry(bad, skins, hand); });
    bad = body; bad.animation.reset();
    Reject([&] { QuestVr::BuildPlayerVisualGeometry(bad, skins, hand); });
    bad = body; bad.triangles[0].u = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { QuestVr::BuildPlayerVisualGeometry(bad, skins, hand); });
    auto badHand = hand; badHand.texturePaths[0] = badHand.texturePaths[1] = "not.original.hands";
    Reject([&] { QuestVr::BuildPlayerVisualGeometry(body, skins, badHand); });
    auto nonfinite = assets.rightHand;
    nonfinite.triangles[0].vertices[0].normal.x = std::numeric_limits<float>::infinity();
    Reject([&] { QuestVr::PlayerVisualDetail::MirroredHand(nonfinite); });
}
void Original(const std::string& root) {
    const auto assets = QuestVr::LoadOriginalPlayerVisualAssets(root);
    if (!assets.passed) throw std::runtime_error(assets.error);
    Require(assets.lowerBody.triangles.size() == 147u, "Original lower-body count changed");
    Require(assets.rightHand.triangles.size() == 142u && assets.leftHand.triangles.size() == 142u,
        "Original WeaponHandsTex surface count changed");
    Require(assets.textures.size() == 3u, "Original lower-body/hand shared texture count changed");
    for (const auto& texture : assets.textures) {
        Require(texture.image.width > 0u && texture.image.height > 0u &&
            texture.image.rgba.size() == static_cast<std::size_t>(texture.image.width)*texture.image.height*4u,
            "Original texture RGBA dimensions differ");
    }
    const auto originalPixels = assets.textures;
    const auto plan = QuestVr::BuildPlayerVisualTextureUploadPlan(assets.textures);
    Require(plan.size() == 3u && plan[0].textureIndex == 0u && plan[0].width == 128u && plan[0].height == 128u &&
        plan[0].rgbaByteCount == 65536u && plan[1].textureIndex == 1u && plan[1].width == 128u &&
        plan[1].height == 256u && plan[1].rgbaByteCount == 131072u && plan[2].textureIndex == 2u &&
        plan[2].width == 256u && plan[2].height == 256u && plan[2].rgbaByteCount == 262144u,
        "Original native-size upload plan dimensions, counts or order changed");
    for (std::size_t layer = 0u; layer < assets.textures.size(); ++layer)
        Require(assets.textures[layer].image.rgba == originalPixels[layer].image.rgba &&
            assets.textures[layer].image.width == originalPixels[layer].image.width &&
            assets.textures[layer].image.height == originalPixels[layer].image.height,
            "Original native pixels/dimensions changed while preparing upload metadata");
    for (const auto* part : {&assets.lowerBody, &assets.rightHand, &assets.leftHand})
        for (const auto& triangle : part->triangles) {
            QuestVr::PlayerVisualDetail::ValidateTriangle(triangle);
            Require(triangle.textureIndex < assets.textures.size(), "Original source texture index is invalid");
        }
    Require(std::fabs(assets.lowerBody.originalPivotObjectUnits.z + 47.50390625f) < 0.001f,
        "Original feet pivot differs from GM_Trench Still");
    const auto& pivot = assets.rightHand.originalPivotObjectUnits;
    Require(std::fabs(pivot.x - 8.313528f) < 0.001f && std::fabs(pivot.y + 2.531867f) < 0.001f &&
        std::fabs(pivot.z + 0.938219f) < 0.001f, "Original grasp pivot differs from material1 unique source centroid");
    auto limits = QuestVr::PlayerVisualLimits{}; limits.maximumTextureBytes = 1u;
    const auto failed = QuestVr::LoadOriginalPlayerVisualAssets(root, limits);
    Require(!failed.passed && !failed.error.empty() && failed.textures.empty() &&
        failed.lowerBody.triangles.empty() && failed.rightHand.triangles.empty() && failed.leftHand.triangles.empty(),
        "Bounded original decode failure must retain no partial visible assets");
    std::cout << "Read-only original player assets: lowerBody=147 rightHand=142 mirroredLeftHand=142 sharedTextures=3; restricted lower-body/rigid source-hand scope.\n";
    std::cout << "Native-size GPU upload plan: 128x128/128x256/256x256, unchanged RGBA/UVs, 458752 source bytes; no resampling.\n";
}
} // namespace
int main(int argc, char** argv) {
    try {
        Synthetic();
        if (argc == 3 && std::string(argv[1]) == "--game-root") Original(argv[2]);
        else if (argc != 1) throw std::runtime_error("Usage: quest_player_visual_test [--game-root <owned Deus Ex directory>]");
        std::cout << "Player visual geometry: " << checks << " checks, " << rejections << " synthetic rejection controls passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
