#include "Precomp.h"
#include "quest_held_item_assets.h"
#include "player_body_preview.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {
std::size_t checks{},rejections{};
void Require(bool value,const char* message) {
    ++checks;if(!value) throw std::runtime_error(message);
}
template<class Operation> void Reject(Operation operation) {
    bool rejected{};try {operation();} catch(const std::exception&) {rejected=true;}
    Require(rejected,"Expected bounded held-item asset rejection");++rejections;
}
bool Close(float a,float b,float tolerance=0.000001f) {return std::fabs(a-b)<=tolerance;}
float Dot(QuestVr::ActorVec3 a,QuestVr::ActorVec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
void Synthetic() {
    using namespace QuestVr;
    PortableLodMesh empty;
    Reject([&]{BuildOriginalHeldItemGeometry(empty,"DeusEx.NanoKeyRing","DeusExItems.NanoKeyRingPOV",1.0f);});
    Reject([&]{BuildOriginalHeldItemGeometry(empty,"DeusEx.WeaponCombatKnife","DeusExItems.Crowbar",1.0f);});
    for(const float scale:{0.0f,-1.0f,5.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        Reject([&]{BuildOriginalHeldItemGeometry(empty,"DeusEx.NanoKeyRing","DeusExItems.NanoKeyRingPOV",scale);});
    PlayerVisualLimits limits;limits.maximumTextures=0u;
    Reject([&]{BuildOriginalHeldItemGeometry(empty,"DeusEx.NanoKeyRing","DeusExItems.NanoKeyRingPOV",1.0f,{},limits);});
    for(std::size_t index=0u;index<HeldItemVisualDetail::Audits.size();++index) {
        const auto& audit=HeldItemVisualDetail::Audits[index];
        Require(&HeldItemVisualDetail::FindAudit(audit.classPath,audit.meshPath)==&audit,"Audited asset identity lookup differs");
        Require(audit.originalFaces==152u+audit.itemFaces+audit.additionalSleeveFaces,"Audited original surface partition is incomplete");
        Require(std::set<std::uint32_t>(audit.rim.begin(),audit.rim.end()).size()==6u,"Audited core cuff identities are repeated");
        for(const auto* landmarks:{&audit.palm,&audit.back})
            Require(std::set<std::uint32_t>(landmarks->begin(),landmarks->end()).size()==4u,"Audited palm/back identities are repeated");
        const auto& next=HeldItemVisualDetail::Audits[(index+1u)%HeldItemVisualDetail::Audits.size()];
        Require(audit.palm!=next.palm&&audit.back!=next.back&&audit.rim!=next.rim,"Different original meshes reuse unaudited landmark indices");
    }
    VrHandGripBasis basis;basis.origin={1,2,3};basis.xAxis={1,0,0};basis.yAxis={0,1,0};basis.zAxis={0,0,1};basis.provenance="synthetic proper frame";
    VrHandGeometryDetail::ValidateGripBasis(basis);
    const auto point=HeldItemVisualDetail::GripPoint({2,4,6},basis);
    const auto direction=HeldItemVisualDetail::GripPoint({2,4,6},basis,true);
    Require(point.x==1&&point.y==2&&point.z==3,"Grip point translation is not applied once");
    Require(direction.x==2&&direction.y==4&&direction.z==6,"Grip direction incorrectly uses point translation");
    const auto missing=LoadOriginalHeldItemAssets("Z:/this-held-original-data-does-not-exist");
    Require(!missing.passed&&!missing.error.empty(),"Missing original packages do not fail honestly");
    for(const auto& asset:missing.variants)
        Require(asset.classPath.empty()&&asset.textures.empty()&&asset.rightHandAndItem.triangles.empty(),"Failed preparation retains partial visible assets");
}

PortableLodMesh SourceMesh(const PortablePackageTables& package,const std::string& meshPath) {
    auto mesh=LoadPortableLodMesh(package,FindPortableExport(package,meshPath.substr(std::string("DeusExItems.").size())));
    for(const auto reference:mesh.textures) {
        auto path=GetPortableObjectPath(package,reference);if(reference>0&&!path.empty())path="DeusExItems."+path;
        mesh.texturePaths.push_back(std::move(path));
    }
    return mesh;
}

void Original(const std::string& root,const std::filesystem::path& preview={}) {
    using namespace QuestVr;
    const auto assets=LoadOriginalHeldItemAssets(root);
    Require(assets.passed,assets.error.c_str());Require(assets.error.empty(),"Successful held preparation retains error");
    const auto items=LoadPortablePackageTables((std::filesystem::path(root)/"System"/"DeusExItems.u").string());
    std::size_t aggregateFaces{},aggregateBytes{};
    for(std::size_t index=0u;index<assets.variants.size();++index) {
        const auto& asset=assets.variants[index];const auto& audit=HeldItemVisualDetail::Audits[index];
        Require(asset.classPath==audit.classPath&&asset.meshPath==audit.meshPath,"Prepared original class/view-mesh identity changed");
        Require(asset.playerViewScale==1.0f,"Original inherited Engine.Inventory.PlayerViewScale differs");
        Require(asset.originalMeshScale.x==1.0f/256.0f&&asset.originalMeshScale.y==1.0f/256.0f&&asset.originalMeshScale.z==1.0f/256.0f,
            "Original authored mesh scale was replaced with player-view scale");
        Require(asset.materialOverrides.skin.path.empty(),"Original effective Skin None changed");
        for(const auto& skin:asset.materialOverrides.multiSkins)Require(skin.path.empty(),"Original effective MultiSkins None changed");
        Require(asset.materialOverrides.texture.path==(index==0u?"Engine.S_Inventory":"Engine.S_Weapon"),"Original inherited Texture fallback fingerprint changed");
        Require(asset.supportsNativeStarterKeyRing==(index==0u),"Native starter ring asset mapping is not explicit");
        Require(asset.originalHandTriangleCount==152u&&asset.originalItemTriangleCount==audit.itemFaces&&
            asset.originalAdditionalSleeveTriangleCount==audit.additionalSleeveFaces,"Original hand/item/sleeve counts differ");
        Require(asset.rightHandAndItem.originalTriangleCount==audit.originalFaces&&asset.rightHandAndItem.derivedClosureTriangleCount==4u&&
            asset.rightHandAndItem.triangles.size()==audit.originalFaces+4u&&!asset.rightHandAndItem.derivedMirrored,"Original assembly face count or parity differs");
        Require(!asset.rightHandAndItem.provenance.empty()&&!asset.originalGripBasis.provenance.empty(),"Original and derivative provenance is absent");
        VrHandGeometryDetail::ValidateGripBasis(asset.originalGripBasis);
        Require(asset.textures.size()==2u,"Original held assembly should use two native textures");
        const auto plan=BuildPlayerVisualTextureUploadPlan(asset.textures);Require(plan.size()==2u,"Original held upload plan differs");
        for(const auto& texture:asset.textures) {
            Require(texture.texturePath==PlayerVisualDetail::OriginalHandTexture||texture.texturePath==audit.itemTexture,"Unexpected held original texture identity");
            const auto original=DecodePortableIndexedTexture(items,texture.texturePath.substr(12u),(texture.polyFlags&2u)!=0u);
            Require(texture.image.width==original.width&&texture.image.height==original.height&&texture.image.rgba==original.rgba,
                "Original held texture dimensions/pixels were resampled or replaced");aggregateBytes+=texture.image.rgba.size();
        }
        auto mesh=SourceMesh(items,asset.meshPath);
        const auto pose=PlayerVisualDetail::OriginalStillPose(mesh);auto transform=PlayerVisualDetail::ObjectToQuestMeters();
        for(std::size_t first=0u;first<mesh.triangles.size();first+=3u) {
            const auto source=BuildActorTriangle(mesh,first,transform,&pose);const auto& prepared=asset.rightHandAndItem.triangles.at(first/3u);
            const auto material=ResolveActorMeshMaterial(asset.materialOverrides,mesh.texturePaths,mesh.materialTextureIndices,mesh.triangles[first].material);
            Require(prepared.sourceMaterial==mesh.triangles[first].material&&prepared.polyFlags==mesh.triangles[first].polyFlags&&
                asset.textures.at(prepared.textureIndex).texturePath==material.texturePath,"Original held material, polygon flags, texture mapping or face ordering changed");
            for(std::size_t corner=0u;corner<3u;++corner) {
                const auto position=HeldItemVisualDetail::GripPoint(source[corner].position,asset.originalGripBasis);
                const auto normal=NormalizeActorVector(HeldItemVisualDetail::GripPoint(source[corner].normal,asset.originalGripBasis,true));
                const auto& actual=prepared.vertices[corner];
                Require(Close(position.x,actual.position.x)&&Close(position.y,actual.position.y)&&Close(position.z,actual.position.z)&&
                    Close(normal.x,actual.normal.x)&&Close(normal.y,actual.normal.y)&&Close(normal.z,actual.normal.z)&&
                    actual.u==source[corner].u&&actual.v==source[corner].v,"Original held surface lost rigid relative placement, normals, or native UVs");
                Require(Close(Dot(actual.normal,actual.normal),1.0f,0.0001f),"Prepared held original normal is not unit length");
            }
            const auto a=VrHandGeometryDetail::Subtract(prepared.vertices[1].position,prepared.vertices[0].position);
            const auto b=VrHandGeometryDetail::Subtract(prepared.vertices[2].position,prepared.vertices[0].position);
            const auto cross=VrHandGeometryDetail::Cross(a,b);
            Require(Dot(cross,cross)>1.0e-16f,"Prepared held assembly contains a degenerate original face");
        }
        aggregateFaces+=asset.rightHandAndItem.triangles.size();
        std::cout<<"OWNED "<<asset.classPath<<" mesh="<<asset.meshPath<<" original="<<asset.rightHandAndItem.originalTriangleCount<<
            " hand="<<asset.originalHandTriangleCount<<" item="<<asset.originalItemTriangleCount<<" additionalSleeve="<<asset.originalAdditionalSleeveTriangleCount<<
            " derivedCuff="<<asset.rightHandAndItem.derivedClosureTriangleCount<<" nativeTextures="<<asset.textures.size()<<'\n';
        auto malformed=mesh;malformed.triangles.pop_back();
        Reject([&]{BuildOriginalHeldItemGeometry(malformed,asset.classPath,asset.meshPath,1.0f);});
        malformed=mesh;malformed.texturePaths.clear();
        Reject([&]{BuildOriginalHeldItemGeometry(malformed,asset.classPath,asset.meshPath,1.0f);});
        malformed=mesh;malformed.animation.reset();
        Reject([&]{BuildOriginalHeldItemGeometry(malformed,asset.classPath,asset.meshPath,1.0f);});
        malformed=mesh;malformed.scaleX=std::numeric_limits<float>::quiet_NaN();
        Reject([&]{BuildOriginalHeldItemGeometry(malformed,asset.classPath,asset.meshPath,1.0f);});
        auto override=asset.materialOverrides;override.skin={true,"DeusExItems.Skins.CandybarTex1"};
        Reject([&]{BuildOriginalHeldItemGeometry(mesh,asset.classPath,asset.meshPath,1.0f,override);});
        auto limit=PlayerVisualLimits{};limit.maximumTrianglesPerPart=audit.originalFaces+3u;
        Reject([&]{BuildOriginalHeldItemGeometry(mesh,asset.classPath,asset.meshPath,1.0f,{},limit);});
        if(!preview.empty()) {
            PlayerVisualAssets visual;visual.lowerBody=asset.rightHandAndItem;visual.textures=asset.textures;
            for(const int view:{0,1,2}) {
                questvisual::Camera camera;
                if(view==0) {camera.position={0.0f,0.25f,0.85f};camera.pitchDegrees=-10.0f;}
                if(view==1) {camera.position={0.85f,0.25f,0.0f};camera.yawDegrees=-90.0f;camera.pitchDegrees=-10.0f;}
                if(view==2) {camera.position={0.0f,0.85f,0.3f};camera.pitchDegrees=-65.0f;}
                const auto capture=questvisual::CapturePlayerBody(visual,camera);
                Require(capture.coveredPixels>100u,"Prepared held original assembly is not visible in software preview");
                questvisual::WriteBmp(preview/("held-"+std::to_string(index)+"-view"+std::to_string(view)+".bmp"),capture.image);
            }
        }
    }
    Require(aggregateFaces==738u,"Combined three original POV plus derived cuff count differs");
    auto limits=PlayerVisualLimits{};limits.maximumTotalTriangles=aggregateFaces-1u;
    auto failed=LoadOriginalHeldItemAssets(root,limits);
    Require(!failed.passed&&!failed.error.empty(),"Combined original held triangle budget must be enforced atomically");
    limits={};limits.maximumTextureBytes=aggregateBytes-1u;failed=LoadOriginalHeldItemAssets(root,limits);
    Require(!failed.passed&&!failed.error.empty(),"Combined original held texture budget must be enforced atomically");
    for(const auto& asset:failed.variants)Require(asset.textures.empty()&&asset.rightHandAndItem.triangles.empty(),"Failed bounded held preparation retained partial renderable data");
}
}

int main(int argc,char** argv) {
    try {
        Synthetic();
        if(argc==3&&std::string(argv[1])=="--game-root") Original(argv[2]);
        else if(argc==4&&std::string(argv[1])=="--preview") {
            const auto root=std::filesystem::canonical(argv[2]);const auto output=std::filesystem::absolute(argv[3]);
            for(auto ancestor=output;!ancestor.empty();) {
                if(std::filesystem::exists(ancestor)&&std::filesystem::equivalent(root,ancestor))
                    throw std::runtime_error("Held original previews must remain outside original game installation");
                const auto next=ancestor.parent_path();if(next==ancestor)break;ancestor=next;
            }
            std::filesystem::create_directories(output);Original(root.string(),output);
        } else if(argc!=1) throw std::runtime_error("Usage: quest_held_item_assets_test [--game-root ROOT | --preview ROOT OUTPUT_DIR]");
        std::cout<<"Held original item assets: "<<checks<<" checks, "<<rejections<<" bounded rejection controls passed.\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
