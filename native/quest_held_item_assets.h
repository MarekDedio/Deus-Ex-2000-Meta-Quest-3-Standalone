#pragma once

#include "quest_player_visual.h"

#include <map>
#include <set>

namespace QuestVr {

// Detached, bounded original POV assemblies. The complete authored right hand
// and item replace the generic hand together; this is not a pickup mesh pasted
// onto an unrelated fist, an arm rig, or the legacy PC camera-offset transform.
struct HeldItemVisualAsset {
    std::string classPath, meshPath;
    float playerViewScale{};
    ActorTextureOverrides materialOverrides;
    ActorVec3 originalMeshScale;
    VrHandGripBasis originalGripBasis;
    PlayerVisualPart rightHandAndItem;
    std::vector<PlayerVisualTexture> textures;
    std::size_t originalHandTriangleCount{}, originalItemTriangleCount{}, originalAdditionalSleeveTriangleCount{};
    bool supportsNativeStarterKeyRing{};
};

struct HeldItemVisualAssets {
    bool passed{};
    std::string error;
    std::array<HeldItemVisualAsset,3> variants; // Key ring, combat knife, crowbar.
};

namespace HeldItemVisualDetail {

struct Audit {
    const char* classPath;
    const char* meshPath;
    const char* itemTexture;
    std::uint16_t handMaterial;
    std::size_t originalFaces, itemFaces, additionalSleeveFaces;
    std::array<std::uint32_t,4> palm, back;
    std::array<std::uint32_t,3> little, index;
    std::array<std::uint32_t,6> rim;
    std::uint32_t cuffSwatch;
};

// Each identity set was audited independently against that mesh's original
// WeaponHandsTex UV atlas and Still topology. Equal UV landmarks do not imply
// equal source vertex indices across the three independently authored meshes.
inline constexpr std::array<Audit,3> Audits{{
    {"DeusEx.NanoKeyRing","DeusExItems.NanoKeyRingPOV","DeusExItems.Skins.NanoKeyRingPOVTex1",0u,212u,60u,0u,
        {77u,78u,9u,11u},{109u,68u,13u,17u},{48u,96u,15u},{20u,64u,30u},{1u,3u,38u,2u,63u,25u},38u},
    {"DeusEx.WeaponCombatKnife","DeusExItems.CombatKnife","DeusExItems.Skins.CombatKnifeTex1",1u,266u,114u,0u,
        {129u,99u,109u,4u},{103u,80u,24u,102u},{7u,86u,100u},{138u,14u,89u},{1u,61u,21u,2u,3u,30u},3u},
    {"DeusEx.WeaponCrowbar","DeusExItems.Crowbar","DeusExItems.Skins.CrowbarTex1",1u,248u,80u,16u,
        {61u,66u,14u,23u},{59u,24u,9u,102u},{33u,107u,46u},{95u,26u,45u},{8u,19u,67u,18u,94u,63u},67u}
}};

inline const Audit& FindAudit(const std::string& classPath,const std::string& meshPath) {
    for(const auto& audit:Audits) if(classPath==audit.classPath&&meshPath==audit.meshPath) return audit;
    throw std::runtime_error("Held-item identity is not an audited original POV assembly");
}

inline ActorVec3 GripPoint(ActorVec3 p,const VrHandGripBasis& basis,bool direction=false) {
    using namespace VrHandGeometryDetail;
    if(!direction) p=Subtract(p,basis.origin);
    return {Dot(p,basis.xAxis),Dot(p,basis.yAxis),Dot(p,basis.zAxis)};
}

inline VrHandGripBasis AnatomicalBasis(const VrHandSourceGeometry& source,const Audit& audit) {
    using namespace VrHandGeometryDetail;
    const auto topology=Inspect(source.triangles,{},false);
    const auto mean=[&](const auto& ids) {
        ActorVec3 value{};
        for(const auto id:ids) {
            const auto p=topology.positions.at(id);const auto count=static_cast<float>(ids.size());
            value.x+=p.x/count;value.y+=p.y/count;value.z+=p.z/count;
        }
        return value;
    };
    const auto inRegion=[&](std::uint32_t id,float minU,float maxU,float minV,float maxV) {
        for(const auto& face:source.triangles) for(std::size_t c=0u;c<3u;++c) {
            const auto& vertex=face.vertices[c];
            if(face.sourceVertexIds[c]==id&&vertex.u>=minU&&vertex.u<=maxU&&vertex.v>=minV&&vertex.v<=maxV) return true;
        }
        return false;
    };
    for(const auto id:audit.palm) if(!inRegion(id,0.15f,0.35f,0.25f,0.46f))
        throw std::runtime_error("Held original palm UV landmarks differ from their independent audit");
    for(const auto id:audit.back) if(!inRegion(id,0.45f,0.75f,0.30f,0.48f))
        throw std::runtime_error("Held original back-of-hand UV landmarks differ from their independent audit");
    // Three exact atlas landmarks on each end of the curled-finger tube.
    const std::array<std::pair<float,float>,3> littleUvs{{{192.0f/255.0f,68.0f/255.0f},
        {174.0f/255.0f,61.0f/255.0f},{175.0f/255.0f,66.0f/255.0f}}};
    const std::array<std::pair<float,float>,3> indexUvs{{{236.0f/255.0f,186.0f/255.0f},
        {236.0f/255.0f,201.0f/255.0f},{198.0f/255.0f,204.0f/255.0f}}};
    for(std::size_t i=0u;i<3u;++i) {
        constexpr float epsilon=0.00001f;
        for(const bool little:{true,false}) {
            const auto uv=little?littleUvs[i]:indexUvs[i];
            if(!inRegion(little?audit.little[i]:audit.index[i],uv.first-epsilon,uv.first+epsilon,uv.second-epsilon,uv.second+epsilon))
                throw std::runtime_error("Held original curled-finger atlas identity differs from its audit");
        }
    }
    const auto palm=mean(audit.palm),back=mean(audit.back),finger=Subtract(mean(audit.index),mean(audit.little));
    const auto intoPalm=Subtract(back,palm);
    if(Dot(finger,finger)<0.0001f||Dot(intoPalm,intoPalm)<0.0001f)
        throw std::runtime_error("Held original hand anatomical landmarks are collapsed");
    VrHandGripBasis basis;
    basis.origin={(palm.x+back.x)*0.5f,(palm.y+back.y)*0.5f,(palm.z+back.z)*0.5f};
    const auto curl=NormalizeActorVector(finger);basis.zAxis={-curl.x,-curl.y,-curl.z};
    const auto along=Dot(intoPalm,basis.zAxis);
    basis.xAxis=NormalizeActorVector({intoPalm.x-along*basis.zAxis.x,intoPalm.y-along*basis.zAxis.y,intoPalm.z-along*basis.zAxis.z});
    basis.yAxis=NormalizeActorVector(Cross(basis.zAxis,basis.xAxis));
    if(source.derivedMirrored||Dot(Subtract(mean(audit.rim),basis.origin),basis.yAxis)<0.15f)
        throw std::runtime_error("Held original hand chirality/wrist direction differs from audited right grip");
    basis.provenance=std::string(audit.meshPath)+" independently audited Still anatomical atlas grip: palm/back midpoint origin, +X into palm, -Z little-to-index, proper +Y toward wrist; original chirality and scale, no PC PlayerViewOffset";
    ValidateGripBasis(basis);return basis;
}

inline void ReadClassDefaults(HeldItemVisualAsset& asset,const std::map<std::string,PortablePackageTables>& packages) {
    std::string path=asset.classPath;
    bool meshSeen{},scaleSeen{};std::set<std::string> seen;
    for(std::size_t depth=0u;!path.empty();++depth) {
        if(depth>=128u||!seen.insert(path).second) throw std::runtime_error("Held original class ancestry is cyclic or exceeds its budget");
        const auto dot=path.find('.');
        if(dot==std::string::npos) throw std::runtime_error("Held original class ancestry lacks package identity");
        const auto found=packages.find(path.substr(0u,dot));
        if(found==packages.end()) throw std::runtime_error("Held original class ancestry leaves its audited System packages");
        const auto& package=found->second;
        const auto descriptor=LoadPortableClassDescriptor(package,FindPortableExport(package,path.substr(dot+1u)));
        ActorTextureOverrides local;
        for(const auto& property:descriptor.defaults) {
            const auto name=property.name.ToString();
            if(name=="PlayerViewMesh"&&!meshSeen) {
                if(property.type!=5u||property.arrayIndex) throw std::runtime_error("Held PlayerViewMesh original property is malformed");
                asset.meshPath=PlayerVisualDetail::QualifiedReference(package,property);meshSeen=true;
            } else if(name=="PlayerViewScale"&&!scaleSeen) {
                if(property.type!=4u||property.value.size()!=4u||property.arrayIndex)
                    throw std::runtime_error("Held PlayerViewScale original property is malformed");
                std::memcpy(&asset.playerViewScale,property.value.data(),4u);scaleSeen=true;
            }
            if(name=="Skin"||name=="Texture"||name=="MultiSkins") {
                if(property.type!=5u||!SetActorTextureOverride(local,name,property.arrayIndex,PlayerVisualDetail::QualifiedReference(package,property)))
                    throw std::runtime_error("Held original material override is malformed");
            }
        }
        InheritActorTextureOverrides(asset.materialOverrides,local);
        path=descriptor.state.baseField?GetPortableObjectPath(package,descriptor.state.baseField):std::string{};
        if(descriptor.state.baseField>0&&!path.empty()) path=std::filesystem::path(package.sourcePath).stem().string()+'.'+path;
    }
    if(!meshSeen||!scaleSeen||!std::isfinite(asset.playerViewScale)||asset.playerViewScale<=0.0f||asset.playerViewScale>4.0f)
        throw std::runtime_error("Held original view mesh/scale metadata is unavailable or outside its budget");
}

} // namespace HeldItemVisualDetail

// Pure CPU preparation from an already decoded original mesh. It samples one
// original Still pose once, retains every authored face (including Crowbar's
// separate 16-face two-sided sleeve continuation), and closes only the audited
// six-vertex core hand cuff. It does not generate or alter any item surface.
inline HeldItemVisualAsset BuildOriginalHeldItemGeometry(const PortableLodMesh& mesh,
    const std::string& classPath,const std::string& meshPath,float playerViewScale,
    const ActorTextureOverrides& materialOverrides={},const PlayerVisualLimits& limits={}) {
    using namespace HeldItemVisualDetail;
    using namespace VrHandGeometryDetail;
    PlayerVisualDetail::ValidateLimits(limits);
    const auto& audit=FindAudit(classPath,meshPath);
    if(!std::isfinite(playerViewScale)||playerViewScale<=0.0f||playerViewScale>4.0f||
        !IsFiniteActorVector({mesh.scaleX,mesh.scaleY,mesh.scaleZ})||
        std::fabs(mesh.scaleX)<1.0e-12f||std::fabs(mesh.scaleY)<1.0e-12f||std::fabs(mesh.scaleZ)<1.0e-12f||
        !mesh.animation||mesh.triangles.size()%3u||mesh.triangles.size()/3u!=audit.originalFaces||
        mesh.animation->triangleSourceVertexIndices.size()!=mesh.triangles.size()||
        audit.originalFaces+4u>limits.maximumTrianglesPerPart)
        throw std::runtime_error("Held original mesh topology/scale exceeds its audited budget");
    HeldItemVisualAsset asset;
    asset.classPath=classPath;asset.meshPath=meshPath;asset.playerViewScale=playerViewScale;
    asset.materialOverrides=materialOverrides;asset.originalMeshScale={mesh.scaleX,mesh.scaleY,mesh.scaleZ};
    const auto pose=PlayerVisualDetail::OriginalStillPose(mesh);
    auto transform=BuildActorToQuest({},{},0,0,0,playerViewScale,{1.0f,1.0f,1.0f},{});transform.translation.y-=1.0f;
    const bool reversed=transform.mirrored!=(mesh.scaleX*mesh.scaleY*mesh.scaleZ<0.0f);
    const std::size_t order[]{0u,reversed?2u:1u,reversed?1u:2u};
    PlayerVisualAssets textureOwner;
    VrHandSourceGeometry source;source.provenance=meshPath+" complete authored Still right-hand surfaces";
    std::vector<PlayerVisualTriangle> originals;
    for(std::size_t first=0u;first<mesh.triangles.size();first+=3u) {
        const auto& face=mesh.triangles[first];
        const auto material=ResolveActorMeshMaterial(materialOverrides,mesh.texturePaths,mesh.materialTextureIndices,face.material);
        const bool hand=face.material==audit.handMaterial;
        const bool extra=classPath=="DeusEx.WeaponCrowbar"&&face.material==2u;
        if(!material.validMaterial||material.texturePath!=(hand||extra?PlayerVisualDetail::OriginalHandTexture:audit.itemTexture))
            throw std::runtime_error("Held original hand/item material fingerprint differs from its audit");
        const auto texture=PlayerVisualDetail::TextureIndex(textureOwner,material.texturePath,limits);
        PlayerVisualTriangle triangle{BuildActorTriangle(mesh,first,transform,&pose),face.polyFlags,texture,face.material};
        PlayerVisualDetail::ValidateTriangle(triangle);originals.push_back(triangle);
        if(hand) {
            VrHandGeometryTriangle original;original.vertices=triangle.vertices;original.textureIndex=texture;
            original.polyFlags=face.polyFlags;original.sourceMaterial=face.material;original.sourceFaceIndex=static_cast<std::uint32_t>(first/3u);
            for(std::size_t c=0u;c<3u;++c) original.sourceVertexIds[c]=mesh.animation->triangleSourceVertexIndices.at(first+order[c]);
            source.triangles.push_back(original);++asset.originalHandTriangleCount;
        } else if(extra) {
            if(face.polyFlags!=0x100u) throw std::runtime_error("Crowbar's original sleeve extension flags differ from its audit");
            ++asset.originalAdditionalSleeveTriangleCount;
        } else ++asset.originalItemTriangleCount;
    }
    const auto topology=Inspect(source.triangles,{},false);
    const std::set<std::uint32_t> rim(topology.boundary.begin(),topology.boundary.end());
    if(asset.originalHandTriangleCount!=152u||topology.positions.size()!=80u||
        rim!=std::set<std::uint32_t>(audit.rim.begin(),audit.rim.end())||
        asset.originalItemTriangleCount!=audit.itemFaces||asset.originalAdditionalSleeveTriangleCount!=audit.additionalSleeveFaces)
        throw std::runtime_error("Held original surface counts or core cuff topology differ from their audit");
    ActorVec3 pivot{};
    for(const auto* ids:{&audit.palm,&audit.back}) for(const auto id:*ids) {
        const auto p=pose.objectPositions.at(id);pivot.x+=p.x/8.0f;pivot.y+=p.y/8.0f;pivot.z+=p.z/8.0f;
    }
    source.originalPivotObjectUnits=pivot;
    asset.originalGripBasis=AnatomicalBasis(source,audit);
    const auto gripSource=PrepareVrHandSourceInGripSpace(source,asset.originalGripBasis);
    VrHandSleeveClosure closure;closure.boundaryVertexIds.assign(audit.rim.begin(),audit.rim.end());bool swatchFound{};
    for(const auto& face:gripSource.triangles) for(std::size_t c=0u;c<3u;++c)
        if(!swatchFound&&face.sourceVertexIds[c]==audit.cuffSwatch) {
            closure.u=face.vertices[c].u;closure.v=face.vertices[c].v;closure.textureIndex=face.textureIndex;
            closure.polyFlags=face.polyFlags;closure.sourceMaterial=face.sourceMaterial;swatchFound=true;
        }
    if(!swatchFound) throw std::runtime_error("Held original cuff texture swatch is unavailable");
    const auto closed=BuildClosedVrHandGeometry(gripSource,closure);
    asset.rightHandAndItem.originalPivotObjectUnits=pivot;
    asset.rightHandAndItem.originalTriangleCount=originals.size();
    asset.rightHandAndItem.derivedClosureTriangleCount=closed.closureTriangleCount;
    for(auto& face:originals) {
        for(auto& v:face.vertices) {
            v.position=GripPoint(v.position,asset.originalGripBasis);
            v.normal=NormalizeActorVector(GripPoint(v.normal,asset.originalGripBasis,true));
            if(std::fabs(v.position.x)>2.0f||std::fabs(v.position.y)>2.0f||std::fabs(v.position.z)>2.0f)
                throw std::runtime_error("Held original item exceeds its controller-local coordinate budget");
        }
        PlayerVisualDetail::Append(asset.rightHandAndItem,std::move(face),limits);
    }
    for(const auto& face:closed.triangles) if(face.origin==VrHandFaceOrigin::DerivedSleeveClosure)
        PlayerVisualDetail::Append(asset.rightHandAndItem,{face.vertices,face.polyFlags,face.textureIndex,face.sourceMaterial},limits);
    asset.rightHandAndItem.provenance=meshPath+" complete original Still assembly: all hand/item/sleeve faces, native UV/material flags and decoded mesh scale retained; "+
        asset.originalGripBasis.provenance+"; only four derived core rear-cuff closure triangles; no original hand overlay or PC camera offset";
    asset.textures=std::move(textureOwner.textures);
    asset.supportsNativeStarterKeyRing=classPath=="DeusEx.NanoKeyRing";
    return asset;
}

// All package, animation and texture I/O happens here during CPU preparation,
// never during equipment selection, controller tracking, or frame rendering.
inline HeldItemVisualAssets LoadOriginalHeldItemAssets(const std::string& gameRoot,const PlayerVisualLimits& limits={}) {
    try {
        PlayerVisualDetail::ValidateLimits(limits);
        const auto system=std::filesystem::path(gameRoot)/"System";
        std::map<std::string,PortablePackageTables> packages;
        for(const auto* name:{"DeusEx","Engine","Core"})
            packages.emplace(name,LoadPortablePackageTables((system/(std::string(name)+".u")).string()));
        const auto items=LoadPortablePackageTables((system/"DeusExItems.u").string());
        HeldItemVisualAssets result;std::size_t totalTriangles{},totalTextureBytes{};
        for(std::size_t index=0u;index<HeldItemVisualDetail::Audits.size();++index) {
            HeldItemVisualAsset metadata;metadata.classPath=HeldItemVisualDetail::Audits[index].classPath;
            HeldItemVisualDetail::ReadClassDefaults(metadata,packages);
            const auto& audit=HeldItemVisualDetail::FindAudit(metadata.classPath,metadata.meshPath);
            const auto exportIndex=FindPortableExport(items,metadata.meshPath.substr(std::string("DeusExItems.").size()));
            PlayerVisualDetail::CheckMeshExport(items,exportIndex,limits);
            auto mesh=LoadPortableLodMesh(items,exportIndex);
            for(const auto reference:mesh.textures) {
                auto path=GetPortableObjectPath(items,reference);
                if(reference>0&&!path.empty()) path="DeusExItems."+path;
                mesh.texturePaths.push_back(std::move(path));
            }
            auto asset=BuildOriginalHeldItemGeometry(mesh,audit.classPath,audit.meshPath,metadata.playerViewScale,metadata.materialOverrides,limits);
            PlayerVisualAssets textureOwner;textureOwner.rightHand=std::move(asset.rightHandAndItem);textureOwner.textures=std::move(asset.textures);
            PlayerVisualDetail::DecodeTextures(textureOwner,items,items,limits);
            asset.rightHandAndItem=std::move(textureOwner.rightHand);asset.textures=std::move(textureOwner.textures);
            BuildPlayerVisualTextureUploadPlan(asset.textures,limits);
            if(asset.rightHandAndItem.triangles.size()>limits.maximumTotalTriangles-totalTriangles)
                throw std::runtime_error("Held original assemblies exceed their aggregate triangle budget");
            totalTriangles+=asset.rightHandAndItem.triangles.size();
            for(const auto& texture:asset.textures) {
                if(texture.image.rgba.size()>limits.maximumTextureBytes-totalTextureBytes)
                    throw std::runtime_error("Held original assemblies exceed their aggregate texture budget");
                totalTextureBytes+=texture.image.rgba.size();
            }
            result.variants[index]=std::move(asset);
        }
        result.passed=true;return result;
    } catch(const std::exception& error) {
        HeldItemVisualAssets result;result.error=error.what();return result;
    }
}

} // namespace QuestVr
