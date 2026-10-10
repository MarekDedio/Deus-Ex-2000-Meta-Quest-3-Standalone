#include "Precomp.h"
#include "quest_player_visual.h"
#include "player_body_preview.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <set>
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
void ClosedHandImportContracts() {
    QuestVr::VrHandGeometry hand;
    hand.originalProvenance="synthetic tetrahedron source";
    hand.derivativeProvenance="synthetic one-face closure";
    hand.originalTriangleCount=3u;hand.closureTriangleCount=1u;hand.sourceVertexCount=4u;
    const QuestVr::ActorVec3 points[]{{0,0,0},{0.02f,0,0},{0,0.02f,0},{0,0,0.02f}};
    const std::array<std::uint32_t,3> faces[]{{0u,2u,1u},{0u,1u,3u},{0u,3u,2u},{1u,2u,3u}};
    for (const auto& ids:faces) {
        QuestVr::VrHandGeometryTriangle face;face.sourceVertexIds=ids;face.textureIndex=2u;
        const auto normal=QuestVr::NormalizeActorVector(QuestVr::VrHandGeometryDetail::Cross(
            QuestVr::VrHandGeometryDetail::Subtract(points[ids[1]],points[ids[0]]),
            QuestVr::VrHandGeometryDetail::Subtract(points[ids[2]],points[ids[0]])));
        for (std::size_t c=0u;c<3u;++c) face.vertices[c]={points[ids[c]],normal,0.25f,0.75f};
        if (hand.triangles.size()==3u) face.origin=QuestVr::VrHandFaceOrigin::DerivedSleeveClosure;
        hand.triangles.push_back(face);
    }
    const auto part=QuestVr::PlayerVisualDetail::PrepareClosedHandPart(hand,{});
    Require(part.triangles.size()==4u && part.originalTriangleCount==3u &&
        part.derivedClosureTriangleCount==1u && !part.derivedMirrored,
        "Closed hand importer must retain original/closure provenance");
    auto broken=hand;broken.originalTriangleCount=4u;
    Reject([&] {QuestVr::PlayerVisualDetail::PrepareClosedHandPart(broken,{});});
    broken=hand;broken.originalProvenance.clear();
    Reject([&] {QuestVr::PlayerVisualDetail::PrepareClosedHandPart(broken,{});});
    broken=hand;broken.triangles.back().origin=static_cast<QuestVr::VrHandFaceOrigin>(99);
    Reject([&] {QuestVr::PlayerVisualDetail::PrepareClosedHandPart(broken,{});});
    broken=hand;broken.triangles.pop_back();broken.originalTriangleCount=2u;
    Reject([&] {QuestVr::PlayerVisualDetail::PrepareClosedHandPart(broken,{});});
    broken=hand;broken.triangles[0].vertices[0].normal.x=std::numeric_limits<float>::infinity();
    Reject([&] {QuestVr::PlayerVisualDetail::PrepareClosedHandPart(broken,{});});
    auto limits=QuestVr::PlayerVisualLimits{};limits.maximumTrianglesPerPart=3u;
    Reject([&] {QuestVr::PlayerVisualDetail::PrepareClosedHandPart(hand,limits);});
}
void Synthetic() {
    TextureUploadContracts();
    ClosedHandImportContracts();
    Require(QuestVr::CullPlayerVisualPart(0u,0u), "Authored body culling must hide garment interiors once torso is restored");
    Require(!QuestVr::CullPlayerVisualPart(0u,258u), "Authored body two-sided/masked state must remain uncullable");
    for (const auto hand : {1u,2u}) {
        Require(QuestVr::CullPlayerVisualPart(hand,0u), "Opaque original hand surfaces must retain culling");
        const std::uint32_t flags = 258u;
        Require(!QuestVr::CullPlayerVisualPart(hand,flags) && flags == 258u,
            "Hand two-sided/masked source flags must remain unchanged");
    }
    Reject([&] { QuestVr::CullPlayerVisualPart(3u,0u); });
    auto body = Mesh({{{{0,0,-47},{10,0,-47},{0,10,10}}}, {{{0,0,-32},{10,0,-32},{0,10,11}}},
        {{{0,0,26},{10,0,26},{0,10,38}}}, {{{0,0,35},{10,0,35},{0,10,47}}},
        {{{0,0,10},{10,0,10},{0,10,35}}}, {{{20,0,10},{30,0,10},{20,10,30}}},
        {{{10,0,20},{20,0,20},{10,10,35}}}, {{{0,0,11},{7,0,11},{0,7,35}}},
        {{{0,0,35},{4,0,35},{0,4,40}}}, {{{0,0,35},{4,0,35},{0,4,40}}}}, {2,4,4,0,1,1,1,3,5,6});
    for (std::size_t corner=0u;corner<3u;++corner) {
        body.triangles[12u+corner].v=corner==2u ? 112.0f/255.0f : 0.1f;
        body.triangles[15u+corner].v=corner==0u ? 182.0f/255.0f : 0.9f;
        body.triangles[18u+corner].v=corner==2u ? 0.9f : 0.1f;
    }
    body.texturePaths.assign(8u, "");
    body.materialTextureIndices = {0,1,2,4,5,6,7};
    QuestVr::ActorTextureOverrides skins;
    skins.multiSkins[1] = {true, "DeusExCharacters.Skins.JCDentonTex2"};
    skins.multiSkins[2] = {true, "DeusExCharacters.Skins.JCDentonTex3"};
    skins.multiSkins[4] = {true, "DeusExCharacters.Skins.JCDentonTex1"};
    skins.multiSkins[5] = {true, "DeusExCharacters.Skins.JCDentonTex2"};
    const auto assets = QuestVr::BuildPlayerBodyGeometry(body, skins);
    Require(!assets.passed && assets.error.empty(), "Geometry-only preparation must not claim decoded texture success");
    Require(assets.lowerBody.triangles.size() == 5u, "Legs, both coat components, torso and chest must survive while head/arms/glasses are omitted");
    Require(assets.rightHand.triangles.empty() && assets.leftHand.triangles.empty(),
        "Body-only preparation must not fabricate controller hands");
    Require(assets.textures.size() == 3u, "Authored torso texture must be retained without duplicating coat textures");
    Require(assets.lowerBody.originalTriangleCount==5u && assets.lowerBody.derivedClosureTriangleCount==0u,
        "All selected body faces must retain original provenance, not invented caps");
    std::array<std::size_t,7> bodyMaterials{};
    for (const auto& triangle:assets.lowerBody.triangles) ++bodyMaterials.at(triangle.sourceMaterial);
    Require(bodyMaterials==std::array<std::size_t,7>{0u,1u,1u,1u,2u,0u,0u},
        "Self-body UV-island selection retained sleeve/mixed UV faces or omitted complete authored torso surfaces");
    const auto sourcePose=QuestVr::PlayerVisualDetail::OriginalStillPose(body);
    const auto sourceTransform=QuestVr::PlayerVisualDetail::ObjectToQuestMeters();
    const std::size_t selectedSources[]{0u,3u,6u,12u,21u};
    for (const auto first:selectedSources) {
        auto expected=QuestVr::BuildActorTriangle(body,first,sourceTransform,&sourcePose);
        for (auto& vertex:expected) vertex.position.y+=47.0f/52.5f;
        const auto found=std::find_if(assets.lowerBody.triangles.begin(),assets.lowerBody.triangles.end(),[&](const auto& triangle) {
            if (triangle.sourceMaterial!=body.triangles[first].material) return false;
            for (std::size_t corner=0u;corner<3u;++corner) {
                const auto& actual=triangle.vertices[corner];const auto& source=expected[corner];
                if (actual.position.x!=source.position.x || std::fabs(actual.position.y-source.position.y)>0.000001f ||
                    actual.position.z!=source.position.z || actual.u!=source.u || actual.v!=source.v) return false;
            }
            return true;
        });
        Require(found!=assets.lowerBody.triangles.end(),"Selected self-body surface changed source topology/UVs instead of adding authored torso geometry");
    }
    float lowest = std::numeric_limits<float>::infinity();
    for (const auto& triangle : assets.lowerBody.triangles) for (const auto& vertex : triangle.vertices)
        lowest = std::min(lowest, vertex.position.y);
    Require(std::fabs(lowest) < 0.00001f, "Lower body must be feet-centered without actor spawn offset");
    auto limits = QuestVr::PlayerVisualLimits{};
    limits.maximumTrianglesPerPart = 1u;
    Reject([&] { QuestVr::BuildPlayerBodyGeometry(body, skins, limits); });
    limits = {}; limits.maximumTotalTriangles = 4u;
    Reject([&] { QuestVr::BuildPlayerBodyGeometry(body, skins, limits); });
    limits = {}; limits.maximumTextures = 2u;
    Reject([&] { QuestVr::BuildPlayerBodyGeometry(body, skins, limits); });
    limits = {}; limits.maximumTextureDimension = 0u;
    Reject([&] { QuestVr::BuildPlayerBodyGeometry(body, skins, limits); });
    auto bad = body; bad.materialTextureIndices[2] = 3;
    Reject([&] { QuestVr::BuildPlayerBodyGeometry(bad, skins); });
    bad = body; bad.animation.reset();
    Reject([&] { QuestVr::BuildPlayerBodyGeometry(bad, skins); });
    bad = body; bad.triangles[0].u = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { QuestVr::BuildPlayerBodyGeometry(bad, skins); });
}
void AuditOriginalBody(const std::string& root) {
    const auto system=std::filesystem::path(root)/"System";
    const auto player=LoadPortablePackageTables((system/"DeusEx.u").string());
    const auto characters=LoadPortablePackageTables((system/"DeusExCharacters.u").string());
    auto body=LoadPortableLodMesh(characters,FindPortableExport(characters,"GM_Trench"));
    QuestVr::ActorTextureOverrides skins;
    for (const auto& property:LoadPortableClassDescriptor(player,FindPortableExport(player,"JCDentonMale")).defaults)
        if (property.type==5u) QuestVr::SetActorTextureOverride(skins,property.name.ToString(),property.arrayIndex,
            QuestVr::PlayerVisualDetail::QualifiedReference(player,property));
    for (const auto reference:body.textures) {
        auto path=GetPortableObjectPath(characters,reference);
        if (reference>0 && !path.empty()) path="DeusExCharacters."+path;
        body.texturePaths.push_back(std::move(path));
    }
    const auto pose=QuestVr::PlayerVisualDetail::OriginalStillPose(body);
    const auto transform=QuestVr::PlayerVisualDetail::ObjectToQuestMeters();
    std::map<std::uint16_t,std::vector<std::size_t>> materials;
    for (std::size_t first=0u;first<body.triangles.size();first+=3u) materials[body.triangles[first].material].push_back(first);
    std::cout<<"ORIGINAL BODY AUDIT GM_Trench Still triangles="<<body.triangles.size()/3u<<" sourceVertices="<<pose.objectPositions.size()<<'\n';
    for (const auto& [material,faces]:materials) {
        const auto skin=QuestVr::ResolveActorMeshMaterial(skins,body.texturePaths,body.materialTextureIndices,material);
        std::map<std::uint32_t,std::vector<std::size_t>> adjacent;
        for (const auto first:faces) for (std::size_t corner=0u;corner<3u;++corner)
            adjacent[body.animation->triangleSourceVertexIndices.at(first+corner)].push_back(first);
        std::set<std::size_t> remaining(faces.begin(),faces.end());std::size_t component{};
        while (!remaining.empty()) {
            std::vector<std::size_t> pending{*remaining.begin()};remaining.erase(remaining.begin());
            std::size_t cursor{};QuestVr::ActorVec3 low{10000,10000,10000},high{-10000,-10000,-10000};
            float minU=10000,minV=10000,maxU=-10000,maxV=-10000;std::set<std::uint32_t> sourceVertices;
            while (cursor<pending.size()) {
                const auto first=pending[cursor++];const auto vertices=QuestVr::BuildActorTriangle(body,first,transform,&pose);
                for (std::size_t corner=0u;corner<3u;++corner) {
                    const auto index=body.animation->triangleSourceVertexIndices.at(first+corner);sourceVertices.insert(index);
                    for (const auto neighbor:adjacent.at(index)) if (remaining.erase(neighbor)) pending.push_back(neighbor);
                    const auto& vertex=vertices[corner];const auto point=vertex.position;
                    low.x=std::min(low.x,point.x);low.y=std::min(low.y,point.y);low.z=std::min(low.z,point.z);
                    high.x=std::max(high.x,point.x);high.y=std::max(high.y,point.y);high.z=std::max(high.z,point.z);
                    minU=std::min(minU,vertex.u);minV=std::min(minV,vertex.v);maxU=std::max(maxU,vertex.u);maxV=std::max(maxV,vertex.v);
                }
            }
            std::cout<<" material="<<material<<" skin="<<skin.textureIndex<<" texture="<<skin.texturePath<<" component="<<component++<<
                " triangles="<<pending.size()<<" vertices="<<sourceVertices.size()<<" meterBounds=("<<low.x<<','<<low.y<<','<<low.z<<")..("<<
                high.x<<','<<high.y<<','<<high.z<<") uv=("<<minU<<','<<minV<<")..("<<maxU<<','<<maxV<<") sourceIndices=";
            for (const auto index:sourceVertices) std::cout<<index<<',';
            std::cout<<'\n';
        }
        if (material==1u) for (const auto first:faces) {
            const auto vertices=QuestVr::BuildActorTriangle(body,first,transform,&pose);
            std::cout<<" COAT_FACE "<<first/3u<<" flags="<<body.triangles[first].polyFlags;
            for (std::size_t corner=0u;corner<3u;++corner) {
                const auto& vertex=vertices[corner];
                std::cout<<" corner"<<corner<<"=("<<vertex.position.x<<','<<vertex.position.y<<','<<vertex.position.z<<
                    ";uv="<<vertex.u<<','<<vertex.v<<')';
            }
            std::cout<<'\n';
        }
    }
}
void Original(const std::string& root) {
    const auto assets = QuestVr::LoadOriginalPlayerVisualAssets(root);
    if (!assets.passed) throw std::runtime_error(assets.error);
    Require(assets.lowerBody.triangles.size() == 251u, "Original authored torso/lower-body selection count changed");
    Require(assets.rightHand.triangles.size() == 156u && assets.leftHand.triangles.size() == 156u &&
        assets.rightHand.originalTriangleCount==152u && assets.rightHand.derivedClosureTriangleCount==4u &&
        assets.leftHand.originalTriangleCount==152u && assets.leftHand.derivedClosureTriangleCount==4u &&
        !assets.rightHand.derivedMirrored && assets.leftHand.derivedMirrored,
        "Closed hand original/derived/mirrored surface provenance changed");
    Require(assets.textures.size() == 4u, "Original body/chest/hand shared texture count changed");
    for (const auto& texture : assets.textures) {
        Require(texture.image.width > 0u && texture.image.height > 0u &&
            texture.image.rgba.size() == static_cast<std::size_t>(texture.image.width)*texture.image.height*4u,
            "Original texture RGBA dimensions differ");
    }
    const auto originalPixels = assets.textures;
    const auto plan = QuestVr::BuildPlayerVisualTextureUploadPlan(assets.textures);
    Require(plan.size() == 4u && plan[0].textureIndex == 0u && plan[0].width == 128u && plan[0].height == 128u &&
        plan[0].rgbaByteCount == 65536u && plan[1].textureIndex == 1u && plan[1].width == 128u &&
        plan[1].height == 256u && plan[1].rgbaByteCount == 131072u && plan[2].textureIndex == 2u &&
        assets.textures[2].texturePath=="DeusExCharacters.Skins.JCDentonTex1" &&
        plan[2].width==assets.textures[2].image.width && plan[2].height==assets.textures[2].image.height &&
        plan[3].textureIndex==3u && plan[3].width == 256u && plan[3].height == 256u && plan[3].rgbaByteCount == 262144u,
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
    const auto items=LoadPortablePackageTables((std::filesystem::path(root)/"System"/"DeusExItems.u").string());
    auto sourceHand=LoadPortableLodMesh(items,FindPortableExport(items,"NanoKeyRingPOV"));
    for (const auto reference:sourceHand.textures) {
        auto path=GetPortableObjectPath(items,reference);
        if (reference>0 && !path.empty()) path="DeusExItems."+path;
        sourceHand.texturePaths.push_back(std::move(path));
    }
    const auto prepared=QuestVr::BuildOriginalNanoKeyRingVrHand(sourceHand,3u);
    const auto& pivot=assets.rightHand.originalPivotObjectUnits;
    Require(pivot.x==prepared.originalPivotObjectUnits.x && pivot.y==prepared.originalPivotObjectUnits.y &&
        pivot.z==prepared.originalPivotObjectUnits.z,"Imported hand must retain its audited palm pivot");
    for (std::size_t face=0u;face<prepared.triangles.size();++face) {
        const auto& original=prepared.triangles[face];const auto& right=assets.rightHand.triangles[face];
        const auto& left=assets.leftHand.triangles[face];const std::size_t order[]{0u,2u,1u};
        Require(right.polyFlags==original.polyFlags && right.textureIndex==original.textureIndex &&
            right.sourceMaterial==original.sourceMaterial,"Hand import changed source material or flags");
        for (std::size_t corner=0u;corner<3u;++corner) {
            const auto& a=original.vertices[corner];const auto& b=right.vertices[corner];
            Require(a.position.x==b.position.x && a.position.y==b.position.y && a.position.z==b.position.z &&
                a.normal.x==b.normal.x && a.normal.y==b.normal.y && a.normal.z==b.normal.z && a.u==b.u && a.v==b.v,
                "Hand import altered canonical grip coordinates, normals or original UVs");
            const auto& r=right.vertices[order[corner]];const auto& l=left.vertices[corner];
            Require(r.position.x==-l.position.x && r.position.y==l.position.y && r.position.z==l.position.z &&
                r.normal.x==-l.normal.x && r.normal.y==l.normal.y && r.normal.z==l.normal.z && r.u==l.u && r.v==l.v,
                "Left hand import changed reflection, corrected winding or source UVs");
        }
    }
    auto limits = QuestVr::PlayerVisualLimits{}; limits.maximumTextureBytes = 1u;
    const auto failed = QuestVr::LoadOriginalPlayerVisualAssets(root, limits);
    Require(!failed.passed && !failed.error.empty() && failed.textures.empty() &&
        failed.lowerBody.triangles.empty() && failed.rightHand.triangles.empty() && failed.leftHand.triangles.empty(),
        "Bounded original decode failure must retain no partial visible assets");
    limits={};limits.maximumTotalTriangles=562u;
    const auto overBudget=QuestVr::LoadOriginalPlayerVisualAssets(root,limits);
    Require(!overBudget.passed && !overBudget.error.empty() && overBudget.textures.empty() &&
        overBudget.lowerBody.triangles.empty() && overBudget.rightHand.triangles.empty() && overBudget.leftHand.triangles.empty(),
        "Combined self-body/hand budget must reject without retaining partial assets");
    std::cout << "Read-only original player assets: selfBody=251 original; each hand=152 original+4 derived rear-cuff faces; sharedTextures=4; authored torso/chest/coat with static arms/head omitted; rigid grip-local scope.\n";
    std::cout << "Native-size GPU upload plan: trousers128x128, coat128x256, chest"<<plan[2].width<<'x'<<plan[2].height<<
        ", hands256x256; unchanged RGBA/UVs, no resampling.\n";
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc==3 && std::string(argv[1])=="--audit-body") {AuditOriginalBody(argv[2]);return 0;}
        if (argc==4 && std::string(argv[1])=="--preview-body") {
            const auto root=std::filesystem::canonical(argv[2]);const auto output=std::filesystem::absolute(argv[3]);
            for (auto ancestor=output;!ancestor.empty();) {
                if (std::filesystem::exists(ancestor) && std::filesystem::equivalent(ancestor,root))
                    throw std::runtime_error("Player body previews must stay outside the original game installation");
                const auto next=ancestor.parent_path();if (next==ancestor) break;ancestor=next;
            }
            const auto assets=QuestVr::LoadOriginalPlayerVisualAssets(root.string());
            if (!assets.passed) throw std::runtime_error(assets.error);
            questvisual::PreviewOriginalPlayerBody(assets,output);return 0;
        }
        Synthetic();
        if (argc == 3 && std::string(argv[1]) == "--game-root") Original(argv[2]);
        else if (argc != 1) throw std::runtime_error("Usage: quest_player_visual_test [--game-root ROOT | --audit-body ROOT | --preview-body ROOT OUTPUT_DIR]");
        std::cout << "Player visual geometry: " << checks << " checks, " << rejections << " synthetic rejection controls passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
