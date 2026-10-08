#include "Precomp.h"
#include "quest_mesh_animation.h"
#include "quest_actor_geometry.h"
#include "surreal_portable_package_tables.h"
#include "Math/coords.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <set>

namespace {
using Vec = QuestVr::ActorVec3;
using Data = PortableMeshAnimationData;
using State = QuestVr::MeshAnimationState;
std::size_t checks{}, rejections{}, differentialVertices{}, poseSampleCalls{}, sampledDistinctFrames{}, normalTriangleSamples{};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
Vec Convert(const vec3& v) { return {v.x,v.y,v.z}; }
vec3 Convert(const Vec& v) { return {v.x,v.y,v.z}; }
void Near(const Vec& actual, const Vec& expected, const std::string& message, float tolerance=0.0003f) {
    Require(std::isfinite(actual.x) && std::isfinite(actual.y) && std::isfinite(actual.z) &&
        std::fabs(actual.x-expected.x)<=tolerance && std::fabs(actual.y-expected.y)<=tolerance &&
        std::fabs(actual.z-expected.z)<=tolerance, message);
}
bool Different(const Vec& a, const Vec& b, float tolerance=0.0001f) {
    return std::fabs(a.x-b.x)>tolerance || std::fabs(a.y-b.y)>tolerance || std::fabs(a.z-b.z)>tolerance;
}
std::string Fold(std::string text) {
    if(text.empty()) return "NONE"; // NameString's canonical empty name.
    for (char& c : text) c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}
const PortableMeshAnimationSequence* ExactSequence(const Data& data,const std::string& name) {
    const auto folded=Fold(name);
    for (const auto& sequence : data.sequences) if (Fold(sequence.name)==folded) return &sequence;
    return nullptr;
}
bool UsableSpan(const Data& data,const PortableMeshAnimationSequence& seq) {
    return seq.startFrame>=0 && seq.numFrames>0 &&
        static_cast<std::uint64_t>(seq.startFrame)+static_cast<std::uint32_t>(seq.numFrames)<=data.animationFrames;
}
// Independent reference implementation of the pinned DX branches. It uses the
// pinned math types/Coords matrices and never calls the portable pose helpers.
struct Oracle {
    const Data& data;
    std::map<std::uint32_t,std::vector<vec3>> normals;
    explicit Oracle(const Data& source) : data(source) {}
    vec3 Vertex(std::uint32_t frame,std::uint32_t index) const {
        const auto& v=data.frameVerticesPacked.at(static_cast<std::size_t>(frame)*data.frameVertices+index);
        return {static_cast<float>(v.x),static_cast<float>(v.y),static_cast<float>(v.z)};
    }
    const std::vector<vec3>& Normals(std::uint32_t frame) {
        auto found=normals.find(frame);
        if(found!=normals.end()) return found->second;
        std::vector<vec3> result(data.frameVertices,vec3(0));
        for(const auto& face : data.normalTopology) {
            const vec3 normal=normalize(cross(Vertex(frame,face[1])-Vertex(frame,face[0]),
                Vertex(frame,face[2])-Vertex(frame,face[0])));
            for(const auto index : face) result.at(index)+=normal;
        }
        for(auto& normal : result) normal=normalize(normal);
        return normals.emplace(frame,std::move(result)).first->second;
    }
    struct Sampling {std::uint32_t frame0{},frame1{},frame2{};float t0{},t1{},weight{1};};
    Sampling Channel(const PortableMeshAnimationSequence& seq,
        const QuestVr::MeshAnimationChannel& channel,bool blend) const {
        const float frame=channel.normalizedFrame*static_cast<float>(seq.numFrames);
        Sampling sample;
        if(frame>=0) {
            const int f0=static_cast<int>(frame)%seq.numFrames;
            const int f1=(static_cast<int>(frame)+1)%seq.numFrames;
            sample.frame0=static_cast<std::uint32_t>(seq.startFrame+f0);
            sample.frame1=static_cast<std::uint32_t>(seq.startFrame+f1);
            sample.t0=frame-static_cast<float>(f0);
        } else {
            const float tween=std::clamp(frame+1.0f,0.0f,1.0f);
            if(blend && channel.previous.fraction<0) {
                sample.frame0=sample.frame1=sample.frame2=static_cast<std::uint32_t>(seq.startFrame);
                sample.weight=tween;
            } else {
                sample.frame0=channel.previous.vertexOffset0/data.frameVertices;
                sample.frame1=channel.previous.vertexOffset1/data.frameVertices;
                sample.frame2=static_cast<std::uint32_t>(seq.startFrame);
                sample.t0=channel.previous.fraction;
                sample.t1=tween;
            }
        }
        return sample;
    }
    vec3 Position(const Sampling& sample,std::uint32_t index,float fatness) {
        vec3 p=mix(Vertex(sample.frame0,index)+Normals(sample.frame0).at(index)*fatness,
            Vertex(sample.frame1,index)+Normals(sample.frame1).at(index)*fatness,sample.t0);
        if(sample.t1!=0) p=mix(p,Vertex(sample.frame2,index)+Normals(sample.frame2).at(index)*fatness,sample.t1);
        return p;
    }
    vec3 Normal(const Sampling& sample,std::uint32_t index) {
        vec3 n=mix(Normals(sample.frame0).at(index),Normals(sample.frame1).at(index),sample.t0);
        if(sample.t1!=0) n=mix(n,Normals(sample.frame2).at(index),sample.t1);
        return n;
    }
    std::pair<Vec,Vec> Sample(const State& state,std::uint32_t index) {
        auto seq=ExactSequence(data,state.main.sequence);
        if(!seq) seq=&data.sequences.front();
        const auto main=Channel(*seq,state.main,false);
        vec3 p=Position(main,index,static_cast<float>(state.fatness)/16.0f-8.0f);
        vec3 n=Normal(main,index);
        for(const auto& blend : state.blends) {
            if(blend.sequence.empty() || Fold(blend.sequence)=="NONE") continue;
            const auto blendSeq=ExactSequence(data,blend.sequence);
            if(!blendSeq) continue;
            const auto sampling=Channel(*blendSeq,blend,true);
            if(sampling.weight<=0) continue; // Pinned DX branches do not access zero-weight blends.
            p+=(Position(sampling,index,0)-Vertex(0,index))*sampling.weight;
            if(data.lodMesh) n+=(Normal(sampling,index)-Normals(0).at(index))*sampling.weight;
        }
        const mat4 matrix=Coords::Rotation(Rotator(data.rotationOriginPitch,data.rotationOriginYaw,data.rotationOriginRoll)).ToMatrix()*
            mat4::scale(Convert(data.scale))*mat4::translate(-Convert(data.origin));
        const mat3 normalMatrix=mat3::transpose(mat3::inverse(mat3(matrix)));
        return {Convert((matrix*vec4(p,1)).xyz()),Convert(normalize(normalMatrix*n))};
    }
    Vec Attachment(const State& state,std::uint32_t index) {
        auto seq=ExactSequence(data,state.main.sequence);
        if(!seq) seq=&data.sequences.front();
        const auto main=Channel(*seq,state.main,false);
        const vec3 p=Position(main,index,0);
        const mat4 matrix=Coords::Rotation(Rotator(data.rotationOriginPitch,data.rotationOriginYaw,data.rotationOriginRoll)).ToMatrix()*
            mat4::scale(Convert(data.scale))*mat4::translate(-Convert(data.origin));
        return Convert((matrix*vec4(p,1)).xyz());
    }
};
void Differential(const Data& data,const State& state,const std::string& context) {
    const auto pose=QuestVr::PrepareMeshPose(data,state);
    Require(pose.drawable && pose.error.empty(),context+": valid pose rejected: "+pose.error);
    Require(pose.objectPositions.size()==data.frameVertices && pose.objectNormals.size()==data.frameVertices,
        context+": pose is not indexed by retained source vertex identity");
    ++poseSampleCalls;sampledDistinctFrames+=pose.sampledFrames;normalTriangleSamples+=pose.normalTriangleSamples;
    Oracle oracle(data);
    for(std::uint32_t index=0;index<data.frameVertices;++index) {
        const auto expected=oracle.Sample(state,index);
        Near(pose.objectPositions[index],expected.first,context+": position differs from pinned DX formula",0.001f);
        Near(pose.objectNormals[index],expected.second,context+": normal differs from pinned DX formula",0.001f);
        ++differentialVertices;
    }
    Require(pose.attachmentPositions.size()==data.specialFaceVertexIndices.size(),context+": attachment count changed");
    for(std::size_t index=0;index<data.specialFaceVertexIndices.size();++index)
        Near(pose.attachmentPositions[index],oracle.Attachment(state,data.specialFaceVertexIndices[index]),
            context+": attachment applied fatness/blend or lost remap",0.001f);
}
Data Synthetic(bool lod=true) {
    Data data;
    data.frameVertices=4;data.animationFrames=6;data.lodMesh=lod;
    const std::array<std::array<PortablePackedMeshVertex,4>,6> frames{{
        {{{0,0,0},{8,0,0},{0,8,0},{0,0,8}}},
        {{{3,0,0},{11,0,0},{3,8,0},{3,0,8}}},
        {{{7,0,2},{15,0,2},{7,8,5},{7,2,10}}},
        {{{10,3,0},{18,3,0},{10,11,2},{10,6,8}}},
        {{{16,5,0},{24,5,0},{16,13,4},{16,9,8}}},
        {{{21,8,0},{29,8,0},{21,16,6},{21,13,8}}}
    }};
    for(const auto& frame : frames) data.frameVerticesPacked.insert(data.frameVerticesPacked.end(),frame.begin(),frame.end());
    data.normalTopology={{{0,1,2}},{{0,3,1}}};
    data.triangleSourceVertexIndices={0,1,2,0,3,1};
    data.specialFaceVertexIndices={3,2,1};
    data.sequences={{"Primary","Fixture",1,2,7,{}},{"Additive","Fixture",3,2,11,{}},{"Single","",5,1,0,{}}};
    data.scale={2,3,4};data.origin={1,2,3};
    data.rotationOriginPitch=8192;data.rotationOriginYaw=16384;data.rotationOriginRoll=-4096;
    return data;
}
void SyntheticContracts() {
    auto data=Synthetic();State state;state.main.sequence="pRiMaRy";
    auto pose=QuestVr::PrepareMeshPose(data,state);
    Require(pose.drawable && !pose.fallbackUsed && pose.resolvedSequence=="Primary","Case-insensitive main lookup changed");
    Differential(data,state,"exact sequence frame zero");
    for(float frame : {0.125f,0.25f,0.625f,0.875f}) {
        state.main.normalizedFrame=frame;Differential(data,state,"interpolation and last-to-first wrap");
    }
    state.main.sequence="Absent";state.main.normalizedFrame=0.25f;
    pose=QuestVr::PrepareMeshPose(data,state);
    Require(pose.drawable && pose.fallbackUsed && pose.resolvedSequence=="Primary","Unknown main name did not use first serialized sequence");
    Differential(data,state,"main first-sequence fallback");
    state.main.sequence="Primary";state.fatness=160;
    Differential(data,state,"fatness before nonuniform Scale/Origin/RotOrigin");
    const auto fat=QuestVr::PrepareMeshPose(data,state);state.fatness=128;
    const auto lean=QuestVr::PrepareMeshPose(data,state);
    Oracle oracle(data);const auto rawNormal=oracle.Normal(oracle.Channel(data.sequences[0],state.main,false),0);
    const mat4 matrix=Coords::Rotation(Rotator(8192,16384,-4096)).ToMatrix()*mat4::scale(2,3,4);
    const Vec rawDisplacement=Convert((matrix*vec4(rawNormal*2.0f,0)).xyz());
    Near({fat.objectPositions[0].x-lean.objectPositions[0].x,fat.objectPositions[0].y-lean.objectPositions[0].y,
        fat.objectPositions[0].z-lean.objectPositions[0].z},rawDisplacement,"Fatness was applied after object transform");
    for(auto& blend : state.blends) {blend.sequence="aDdItIvE";blend.normalizedFrame=0.25f;}
    Differential(data,state,"four additive absolute-frame-zero-reference channels");
    const auto four=QuestVr::PrepareMeshPose(data,state);
    for(auto& blend : state.blends) blend.sequence="None";
    const auto zero=QuestVr::PrepareMeshPose(data,state);
    state.blends[0].sequence="Additive";const auto one=QuestVr::PrepareMeshPose(data,state);
    Near({four.objectPositions[0].x-zero.objectPositions[0].x,four.objectPositions[0].y-zero.objectPositions[0].y,
        four.objectPositions[0].z-zero.objectPositions[0].z},
        {(one.objectPositions[0].x-zero.objectPositions[0].x)*4,(one.objectPositions[0].y-zero.objectPositions[0].y)*4,
        (one.objectPositions[0].z-zero.objectPositions[0].z)*4},"Blend channels became weighted crossfades");
    Require(Different(one.objectNormals[0],zero.objectNormals[0]),"LOD additive blend did not affect normals");
    data.lodMesh=false;const auto meshBlend=QuestVr::PrepareMeshPose(data,state);
    Differential(data,state,"MeshDX additive positions only");
    state.blends[0].sequence="None";const auto meshMain=QuestVr::PrepareMeshPose(data,state);
    Near(meshBlend.objectNormals[0],meshMain.objectNormals[0],"MeshDX additive blend must leave normals unchanged");
    state.blends[0].sequence="Missing";
    const auto missing=QuestVr::PrepareMeshPose(data,state);
    Near(missing.objectPositions[0],meshMain.objectPositions[0],"Unknown blend fell back to first sequence");
    data.lodMesh=true;state=State{};state.main.sequence="Primary";state.main.normalizedFrame=-0.25f;
    state.main.previous={3*data.frameVertices,4*data.frameVertices,0.375f};
    Differential(data,state,"negative main tween from native vertex-offset history");
    state.fatness=144;
    state.blends[0].sequence="Additive";state.blends[0].normalizedFrame=-0.125f;
    state.blends[0].previous={1*data.frameVertices,2*data.frameVertices,0.625f};
    Differential(data,state,"negative blend tween with native history");
    state.blends[0].previous={std::numeric_limits<std::uint32_t>::max(),1,-1};
    Differential(data,state,"negative blend no-history fades sequence-first delta");
    state.blends[0].normalizedFrame=-4;
    Differential(data,state,"frozen below-negative-one blend");
    state=State{};state.main.sequence="Single";state.main.normalizedFrame=0.75f;
    Differential(data,state,"single-frame sequence");
    state.main.sequence="Primary";state.main.normalizedFrame=-0.25f;
    Differential(data,state,"pinned initial zero-offset tween-history exception");
    state.main.normalizedFrame=0.875f;
    const auto history=QuestVr::CaptureMeshTweenHistory(data,state.main);
    Require(history.vertexOffset0==2*data.frameVertices && history.vertexOffset1==data.frameVertices && history.fraction==0.75f,
        "Native tween-history capture did not preserve sequence offsets/last-to-first fraction");
    state.main.normalizedFrame=-0.25f;
    const auto clampedHistory=QuestVr::CaptureMeshTweenHistory(data,state.main);
    Require(clampedHistory.vertexOffset0==data.frameVertices && clampedHistory.vertexOffset1==2*data.frameVertices && clampedHistory.fraction==0,
        "Native capture must clamp old negative main frame to sequence zero");
    state.main.sequence="Absent";
    const auto noHistory=QuestVr::CaptureMeshTweenHistory(data,state.main,false);
    Require(noHistory.vertexOffset0==0 && noHistory.vertexOffset1==0 && noHistory.fraction==-1,
        "Missing exact blend sequence history was guessed from fallback");
    auto noneNamed=data;noneNamed.sequences[2].name="None";state=State{};
    const auto nonePose=QuestVr::PrepareMeshPose(noneNamed,state);
    Require(nonePose.drawable && nonePose.resolvedSequence=="None" && !nonePose.fallbackUsed,
        "Empty main NameString did not match an authored None-named sequence");
    Differential(noneNamed,state,"canonical empty/None sequence name");
    data.sequences.clear();const auto omitted=QuestVr::PrepareMeshPose(data,state);
    Require(!omitted.drawable && omitted.error.empty(),"No-sequence mesh was rendered or falsely marked malformed");
}
void SharedGeometryContracts() {
    auto data=Synthetic();
    data.triangleSourceVertexIndices={2,0,1,0,3,1};
    PortableLodMesh mesh;mesh.animation=std::make_shared<const Data>(data);
    mesh.scaleX=data.scale.x;mesh.scaleY=data.scale.y;mesh.scaleZ=data.scale.z;
    // Static-cache points are deliberately unrelated to the animated pose.
    mesh.triangles={{1000,1000,1000,0.1f,0.2f,0,0,0,1,0x100u},
        {2000,2000,2000,0.3f,0.4f,0,0,0,1,0x100u},
        {3000,3000,3000,0.5f,0.6f,0,0,0,1,0x100u},
        {0,0,0,0,0,0},{1,0,0,1,0,0},{0,1,0,0,1,0}};
    PortableActorSnapshot actor;actor.x=52.5f;actor.y=-105;actor.z=21;
    actor.prePivotX=4;actor.prePivotY=5;actor.prePivotZ=6;
    actor.pitch=4096;actor.yaw=12000;actor.roll=-7000;actor.drawScale=1.5f;
    actor.drawScaleX=2;actor.drawScaleY=3;actor.drawScaleZ=4;
    actor.animation.sequence="Primary";actor.animation.frame=0.375f;actor.fatness=144;
    actor.animation.blends[2].sequence="Additive";actor.animation.blends[2].frame=0.25f;
    const auto state=QuestVr::BuildSnapshotMeshAnimationState(actor);
    Require(state.main.sequence==actor.animation.sequence && state.main.normalizedFrame==actor.animation.frame &&
        state.blends[2].sequence=="Additive" && state.blends[2].normalizedFrame==0.25f && state.fatness==144,
        "Snapshot animation channel/index/fatness extraction changed");
    const auto pose=QuestVr::PrepareMeshPose(mesh,state);
    const auto transform=QuestVr::BuildSnapshotActorTransform(actor,{});
    const auto triangle=QuestVr::BuildActorTriangle(mesh,0,transform,&pose);
    const bool reversed=transform.mirrored != (data.scale.x*data.scale.y*data.scale.z<0);
    const std::array<std::size_t,3> order={0,reversed ? 2u : 1u,reversed ? 1u : 2u};
    for(std::size_t c=0;c<3;++c) {
        const auto source=data.triangleSourceVertexIndices[order[c]];
        Near(triangle[c].position,transform.TransformPoint(pose.objectPositions[source]),"Shared geometry ignored animated source identity/winding");
        Near(triangle[c].normal,transform.TransformNormal(pose.objectNormals[source]),"Shared geometry lost animated inverse-transpose normal");
        Require(triangle[c].u==mesh.triangles[order[c]].u && triangle[c].v==mesh.triangles[order[c]].v,
            "Shared animated geometry changed original corner UVs");
    }
    const auto reject=[&](const QuestVr::MeshPose& bad,const std::string& context) {
        bool rejected{};try {QuestVr::BuildActorTriangle(mesh,0,transform,&bad);}catch(const std::runtime_error&) {rejected=true;}
        Require(rejected,context);++rejections;
    };
    auto bad=pose;bad.drawable=false;reject(bad,"Shared geometry accepted a nondrawable pose");
    bad=pose;bad.objectPositions.resize(1);reject(bad,"Shared geometry accepted a truncated position pose");
    bad=pose;bad.objectNormals.clear();reject(bad,"Shared geometry accepted a truncated normal pose");
}
void Reject(const Data& data,const State& state,const std::string& context,
    const QuestVr::MeshAnimationLimits& limits=QuestVr::MeshAnimationLimits{}) {
    const auto pose=QuestVr::PrepareMeshPose(data,state,limits);
    Require(!pose.drawable && !pose.error.empty(),context+": malformed pose accepted or rejection reason missing");
    Require(pose.objectPositions.empty() && pose.objectNormals.empty() && pose.attachmentPositions.empty(),
        context+": rejected pose leaked partial geometry");
    ++rejections;
}
void InvalidContracts() {
    const auto valid=Synthetic();State state;state.main.sequence="Primary";
    const auto badData=[&](const std::function<void(Data&)>& edit,const std::string& why) {
        auto data=valid;edit(data);Reject(data,state,why);
    };
    badData([](Data& d){d.frameVertices=0;},"Zero FrameVerts");
    badData([](Data& d){d.animationFrames=0;},"Zero AnimFrames");
    badData([](Data& d){d.frameVerticesPacked.pop_back();},"Insufficient retained packed frames");
    badData([](Data& d){d.frameVertices=std::numeric_limits<std::uint32_t>::max();d.animationFrames=std::numeric_limits<std::uint32_t>::max();},"Overflow-sized dimensions");
    badData([](Data& d){d.normalTopology[0][0]=d.frameVertices;},"Normal topology index");
    badData([](Data& d){d.triangleSourceVertexIndices[0]=d.frameVertices;},"Triangle source index");
    badData([](Data& d){d.specialFaceVertexIndices[0]=d.frameVertices;},"Attachment source index");
    badData([](Data& d){d.triangleSourceVertexIndices.pop_back();},"Incomplete render triangle topology");
    badData([](Data& d){d.specialFaceVertexIndices.pop_back();},"Incomplete attachment triangle topology");
    badData([](Data& d){d.sequences[0].startFrame=-1;},"Negative sequence start");
    badData([](Data& d){d.sequences[0].numFrames=0;},"Zero sequence frames");
    badData([](Data& d){d.sequences[0].startFrame=5;d.sequences[0].numFrames=2;},"Sequence end beyond retained frames");
    badData([](Data& d){d.sequences[0].rate=std::numeric_limits<float>::infinity();},"Nonfinite sequence rate");
    badData([](Data& d){d.sequences[0].notifies.push_back({std::numeric_limits<float>::quiet_NaN(),"Fixture"});},"Nonfinite notify time");
    badData([](Data& d){d.scale.y=0;},"Singular Scale");
    badData([](Data& d){d.origin.x=std::numeric_limits<float>::infinity();},"Nonfinite Origin");
    const auto badState=[&](const std::function<void(State&)>& edit,const std::string& why) {
        auto changed=state;edit(changed);Reject(valid,changed,why);
    };
    badState([](State& s){s.main.normalizedFrame=std::numeric_limits<float>::quiet_NaN();},"NaN main frame");
    badState([](State& s){s.main.normalizedFrame=1;},"Unnormalized positive frame");
    badState([](State& s){s.main.normalizedFrame=-0.25f;s.main.previous={1,4,0.5f};},"Unaligned tween vertex offsets");
    badState([&](State& s){s.main.normalizedFrame=-0.25f;s.main.previous={valid.frameVertices*valid.animationFrames,0,0.5f};},"Tween offsets beyond packed frame bounds");
    badState([](State& s){s.main.normalizedFrame=-0.25f;s.main.previous={4,0,-1};},"Missing noninitial native tween history");
    badState([](State& s){s.main.normalizedFrame=-0.25f;s.main.previous={0,0,1};},"Unnormalized native tween fraction");
    badState([](State& s){s.blends[0].sequence="Additive";s.blends[0].normalizedFrame=std::numeric_limits<float>::infinity();},"Nonfinite active blend");
    badState([](State& s){s.blends[0].sequence="Additive";s.blends[0].normalizedFrame=-0.25f;s.blends[0].previous={1,4,0.5f};},"Invalid required blend tween offsets");
    badState([](State& s){s.blends[0].sequence="Additive";s.blends[0].normalizedFrame=-0.25f;s.blends[0].previous.fraction=std::numeric_limits<float>::quiet_NaN();},"Nonfinite required blend tween fraction");
    const auto budget=[&](const std::function<void(QuestVr::MeshAnimationLimits&)>& edit,const std::string& why) {
        QuestVr::MeshAnimationLimits limits;edit(limits);Reject(valid,state,why,limits);
    };
    budget([](auto& l){l.maxRetainedBytes=1;},"Retained byte budget");
    budget([](auto& l){l.maxSampledBytes=1;},"Sampled byte budget");
    budget([](auto& l){l.maxNormalTriangleSamples=1;},"Normal work budget");
    budget([](auto& l){l.maxFrameVertices=3;},"Source vertex budget");
    budget([](auto& l){l.maxAnimationFrames=5;},"Animation frame budget");
    budget([](auto& l){l.maxSequences=2;},"Sequence table budget");
    auto withNotify=valid;withNotify.sequences[0].notifies.push_back({0.5f,"Fixture"});
    QuestVr::MeshAnimationLimits notifyBudget;notifyBudget.maxNotifies=0;
    Reject(withNotify,state,"Notify table budget",notifyBudget);
    state.main.normalizedFrame=0.25f;QuestVr::MeshAnimationLimits frameBudget;frameBudget.maxSampledFrames=1;
    Reject(valid,state,"Distinct sampled frame budget",frameBudget);
    // Pinned assets may have nonpositive rate or out-of-[0,1] notify metadata;
    // only finite values are required, not invented authoring restrictions.
    auto finiteMetadata=valid;finiteMetadata.sequences[0].rate=-1;finiteMetadata.sequences[0].notifies={{-2,"Fixture"},{3,"Fixture"}};
    Differential(finiteMetadata,state,"finite legacy authoring metadata");
    auto trailing=valid;trailing.frameVerticesPacked.push_back({1,2,3});
    Differential(trailing,state,"retained trailing packed vertices compatibility");
    auto legacySpan=valid;legacySpan.sequences[1].startFrame=5;legacySpan.sequences[1].numFrames=3;
    Differential(legacySpan,state,"unused authored sequence with unavailable original frames is retained");
    state.main.sequence="Additive";
    Reject(legacySpan,state,"Selected unavailable legacy sequence diagnosed without invented frames");
    state.main.sequence="Primary";state.blends[0].sequence="Additive";
    Reject(legacySpan,state,"Selected unavailable legacy additive channel diagnosed without invented frames");
    auto partial=valid;partial.sequences[0].startFrame=4;partial.sequences[0].numFrames=3;
    state=State{};state.main.sequence="Primary";
    const auto early=QuestVr::PrepareMeshPose(partial,state);
    Require(early.drawable && early.selectedOriginalSpanInvalid,
        "Available early frame pair of a dangling original span was wrongly omitted or not diagnosed");
    Differential(partial,state,"partially unavailable span with real early frame pair");
    state.main.normalizedFrame=0.5f;
    Reject(partial,state,"Partially unavailable span actually accessing missing next frame");
    state=State{};state.main.sequence="Primary";state.blends[0].sequence="Additive";
    state.blends[0].normalizedFrame=-4;
    const auto faded=QuestVr::PrepareMeshPose(legacySpan,state);
    Require(faded.drawable && faded.selectedOriginalSpanInvalid,
        "Zero-weight missing-frame blend must preserve diagnosis without accessing nonexistent vertices");
    Differential(legacySpan,state,"zero-weight dangling additive channel");
}

// Independent serialized reader for the original-package audit. Only tagged
// properties/package tables use the established reader; the native mesh tail,
// including every packed frame/sequence/notify/remap, is read again here.
struct Reader {
    std::vector<std::uint8_t> bytes;
    std::size_t cursor{};
    void Need(std::size_t n) const {if(n>bytes.size()-cursor) throw std::runtime_error("Oracle mesh payload truncated");}
    void Skip(std::size_t n) {Need(n);cursor+=n;}
    std::uint8_t U8() {Need(1);return bytes[cursor++];}
    std::uint16_t U16() {const auto a=U8(),b=U8();return static_cast<std::uint16_t>(a|static_cast<std::uint16_t>(b)<<8u);}
    std::uint32_t U32() {std::uint32_t result{};for(unsigned i=0;i<4;++i) result|=static_cast<std::uint32_t>(U8())<<(i*8u);return result;}
    std::int32_t I32() {return static_cast<std::int32_t>(U32());}
    float Float() {const auto bits=U32();float value{};std::memcpy(&value,&bits,4);return value;}
    std::int32_t Index() {
        const auto first=U8();std::uint32_t value=first&0x3fu;unsigned shift=6;auto more=static_cast<bool>(first&0x40u);
        while(more) {const auto next=U8();if(shift>27) throw std::runtime_error("Oracle compact index overflow");
            value|=static_cast<std::uint32_t>(next&0x7fu)<<shift;shift+=7;more=(next&0x80u)!=0;}
        return (first&0x80u) ? -static_cast<std::int32_t>(value) : static_cast<std::int32_t>(value);
    }
    std::size_t Count(std::size_t stride=1) {
        const auto count=Index();
        if(count<0 || static_cast<std::size_t>(count)>(bytes.size()-cursor)/stride)
            throw std::runtime_error("Oracle mesh count invalid");
        return static_cast<std::size_t>(count);
    }
    Vec Vector() {const float x=Float(),y=Float(),z=Float();return {x,y,z};}
};
Data ReadOriginal(const PortablePackageTables& package,std::size_t exportIndex,bool lod) {
    const auto& entry=package.exports.at(exportIndex);Reader reader;
    reader.bytes.resize(static_cast<std::size_t>(entry.ObjSize));
    std::ifstream file(package.sourcePath,std::ios::binary);file.seekg(entry.ObjOffset);
    file.read(reinterpret_cast<char*>(reader.bytes.data()),static_cast<std::streamsize>(reader.bytes.size()));
    Require(static_cast<bool>(file),"Could not read original mesh tail for independent audit");
    reader.Skip(LoadPortableExportProperties(package,exportIndex).bytesConsumed);reader.Skip(41);
    const auto lazyEnd=[&](std::uint32_t end) {Require(static_cast<std::uint64_t>(entry.ObjOffset)+reader.cursor==end,"Oracle lazy-array end mismatch");};
    const auto name=[&]() {const auto index=reader.Index();Require(index>=0 && static_cast<std::size_t>(index)<package.names.size(),"Oracle name index invalid");
        return package.names[static_cast<std::size_t>(index)].Name.ToString();};
    Data data;data.lodMesh=lod;
    const auto verticesEnd=reader.U32();const auto vertices=reader.Count(8);data.frameVerticesPacked.reserve(vertices);
    for(std::size_t i=0;i<vertices;++i) {const auto x=static_cast<std::int16_t>(reader.U16()),y=static_cast<std::int16_t>(reader.U16()),z=static_cast<std::int16_t>(reader.U16());
        reader.U16();data.frameVerticesPacked.push_back({x,y,z});}lazyEnd(verticesEnd);
    const auto trianglesEnd=reader.U32();const auto legacyFaces=reader.Count(20);
    std::vector<std::array<std::uint32_t,3>> legacyTopology;
    for(std::size_t i=0;i<legacyFaces;++i) {const auto a=reader.U16(),b=reader.U16(),c=reader.U16();legacyTopology.push_back({a,b,c});reader.Skip(14);}lazyEnd(trianglesEnd);
    const auto sequences=reader.Count();
    for(std::size_t i=0;i<sequences;++i) {PortableMeshAnimationSequence seq;seq.name=name();seq.group=name();seq.startFrame=reader.I32();seq.numFrames=reader.I32();
        const auto notifications=reader.Count();for(std::size_t j=0;j<notifications;++j) {const float time=reader.Float();seq.notifies.push_back({time,name()});}
        seq.rate=reader.Float();std::stable_sort(seq.notifies.begin(),seq.notifies.end(),[](const auto& a,const auto& b){return a.time<b.time;});data.sequences.push_back(std::move(seq));}
    const auto connectsEnd=reader.U32();reader.Skip(reader.Count(8)*8);lazyEnd(connectsEnd);reader.Skip(41);
    const auto linksEnd=reader.U32();reader.Skip(reader.Count(4)*4);lazyEnd(linksEnd);
    const auto textures=reader.Count();for(std::size_t i=0;i<textures;++i) reader.Index();
    reader.Skip(reader.Count(25)*25);reader.Skip(reader.Count(16)*16);
    data.frameVertices=reader.U32();data.animationFrames=reader.U32();reader.Skip(8);
    data.scale=reader.Vector();data.origin=reader.Vector();data.rotationOriginPitch=reader.I32();data.rotationOriginYaw=reader.I32();data.rotationOriginRoll=reader.I32();
    reader.Skip(8);reader.Skip(reader.Count(4)*4);
    if(!lod) {
        data.normalTopology=legacyTopology;
        for(const auto& face : legacyTopology) for(auto index : face) data.triangleSourceVertexIndices.push_back(index);
    } else {
        reader.Skip(reader.Count(2)*2);reader.Skip(reader.Count(2)*2);
        const auto faceCount=reader.Count(8);std::vector<std::array<std::uint32_t,3>> faces;
        for(std::size_t i=0;i<faceCount;++i) {const auto a=reader.U16(),b=reader.U16(),c=reader.U16();faces.push_back({a,b,c});reader.U16();}
        reader.Skip(reader.Count(2)*2);const auto wedgeCount=reader.Count(4);std::vector<std::uint16_t> wedges;
        for(std::size_t i=0;i<wedgeCount;++i) {wedges.push_back(reader.U16());reader.Skip(2);}
        reader.Skip(reader.Count(8)*8);const auto specialFaceCount=reader.Count(8);std::vector<std::uint16_t> specialCorners;
        for(std::size_t i=0;i<specialFaceCount;++i) {for(unsigned c=0;c<3;++c) specialCorners.push_back(reader.U16());reader.U16();}
        reader.U32();const auto specialVerts=reader.U32();reader.Skip(24);
        const auto remapCount=reader.Count(2);std::vector<std::uint16_t> remap;
        for(std::size_t i=0;i<remapCount;++i) remap.push_back(reader.U16());
        reader.U32();
        for(const auto& face : faces) {std::array<std::uint32_t,3> topology{};
            for(unsigned c=0;c<3;++c) {topology[c]=wedges.at(face[c])+specialVerts;
                data.triangleSourceVertexIndices.push_back(remap.empty() ? topology[c] : remap.at(topology[c]));}
            data.normalTopology.push_back(topology);}
        for(auto index : specialCorners) data.specialFaceVertexIndices.push_back(remap.empty() ? index : remap.at(index));
    }
    Require(reader.cursor==reader.bytes.size(),"Oracle mesh tail did not end at original export boundary");
    for(auto& seq : data.sequences) seq.invalidOriginalSpan=!UsableSpan(data,seq);
    return data;
}
void CompareRetained(const Data& data,const Data& source,const std::string& context) {
    Require(data.frameVertices==source.frameVertices && data.animationFrames==source.animationFrames && data.lodMesh==source.lodMesh,context+": retained dimensions/class changed");
    Require(data.frameVerticesPacked.size()==source.frameVerticesPacked.size(),context+": packed vertex count changed");
    for(std::size_t i=0;i<source.frameVerticesPacked.size();++i) {
        const auto& a=data.frameVerticesPacked[i];const auto& b=source.frameVerticesPacked[i];
        Require(a.x==b.x && a.y==b.y && a.z==b.z,context+": original packed signed coordinate changed");
    }
    Require(data.normalTopology==source.normalTopology && data.triangleSourceVertexIndices==source.triangleSourceVertexIndices &&
        data.specialFaceVertexIndices==source.specialFaceVertexIndices,context+": normal/source/attachment remap changed");
    Require(data.sequences.size()==source.sequences.size(),context+": original sequence count changed");
    for(std::size_t i=0;i<source.sequences.size();++i) {const auto& a=data.sequences[i];const auto& b=source.sequences[i];
        Require(a.name==b.name && a.group==b.group && a.startFrame==b.startFrame && a.numFrames==b.numFrames && a.rate==b.rate && a.notifies.size()==b.notifies.size(),
            context+": original sequence metadata changed");
        Require(UsableSpan(data,a)==UsableSpan(source,b),context+": legacy sequence availability classification changed");
        Require(a.invalidOriginalSpan==b.invalidOriginalSpan && a.invalidOriginalSpan==!UsableSpan(source,b),
            context+": original unavailable-span metadata flag changed");
        for(std::size_t n=0;n<a.notifies.size();++n) Require(a.notifies[n].time==b.notifies[n].time && a.notifies[n].function==b.notifies[n].function,
            context+": notify time/function/stable order changed");
    }
    Near(data.scale,source.scale,context+": retained Scale changed",0);Near(data.origin,source.origin,context+": retained Origin changed",0);
    Require(data.rotationOriginPitch==source.rotationOriginPitch && data.rotationOriginYaw==source.rotationOriginYaw && data.rotationOriginRoll==source.rotationOriginRoll,
        context+": retained RotOrigin changed");
}
void OriginalMeshes(const std::filesystem::path& root) {
    std::size_t exports{}, packed{}, sequences{}, animated{}, notifyCount{}, attachmentCorners{}, noSequences{},
        retainedBytes{},maximumAssetBytes{},animationFrames{},sourceVertices{},sequenceFrameRanges{},
        unavailableSpans{},legacySpanExports{},noUsableSpanExports{},unavailablePoseOmissions{},partialSpanEarlyPoses{};
    std::string maximumAsset;
    for(const auto& file : std::filesystem::directory_iterator(root/"System")) {
        if(Fold(file.path().extension().string())!=".U") continue;
        const auto package=LoadPortablePackageTables(file.path().string());
        for(std::size_t index=0;index<package.exports.size();++index) {
            const auto path=GetPortableObjectPath(package,package.exports[index].ObjClass);
            const auto cls=path.substr(path.find_last_of('.')+1);
            if(cls!="Mesh" && cls!="LodMesh") continue;
            const auto context=file.path().filename().string()+":"+GetPortableObjectPath(package,static_cast<std::int32_t>(index+1));
            const auto original=ReadOriginal(package,index,cls=="LodMesh");
            PortableLodMesh mesh;
            try {mesh=LoadPortableLodMesh(package,index);}catch(const std::exception& error) {
                std::cerr<<"Original decoder rejection "<<context<<" source="<<file.path().string()<<" export="<<index
                    <<" FrameVerts="<<original.frameVertices<<" AnimFrames="<<original.animationFrames
                    <<" packedVertices="<<original.frameVerticesPacked.size()<<" reason="<<error.what()<<"\n";
                for(const auto& seq : original.sequences) {
                    if(seq.startFrame<0 || seq.numFrames<=0 ||
                        static_cast<std::uint64_t>(seq.startFrame)+static_cast<std::uint32_t>(seq.numFrames)>original.animationFrames || !std::isfinite(seq.rate))
                        std::cerr<<"Invalid original sequence name="<<seq.name<<" group="<<seq.group<<" start="<<seq.startFrame
                            <<" frames="<<seq.numFrames<<" rate="<<seq.rate<<"\n";
                }
                throw std::runtime_error(context+": "+error.what());
            }
            Require(static_cast<bool>(mesh.animation),context+": animation frames not retained");
            const auto copy=mesh;Require(copy.animation.get()==mesh.animation.get(),context+": mesh copies duplicated immutable packed frames");
            CompareRetained(*mesh.animation,original,context);
            ++exports;packed+=original.frameVerticesPacked.size();sequences+=original.sequences.size();
            animationFrames+=original.animationFrames;sourceVertices+=original.frameVertices;
            const auto assetBytes=QuestVr::ValidateMeshAnimationData(*mesh.animation);retainedBytes+=assetBytes;
            if(assetBytes>maximumAssetBytes) {maximumAssetBytes=assetBytes;maximumAsset=context;}
            animated+=original.animationFrames>1;attachmentCorners+=original.specialFaceVertexIndices.size();
            std::vector<std::size_t> validSequenceIndices;
            bool unavailable{};
            for(std::size_t seqIndex=0;seqIndex<original.sequences.size();++seqIndex) {
                const auto& seq=original.sequences[seqIndex];notifyCount+=seq.notifies.size();
                if(seq.numFrames>0) sequenceFrameRanges+=static_cast<std::size_t>(seq.numFrames);
                if(UsableSpan(original,seq)) {validSequenceIndices.push_back(seqIndex);continue;}
                ++unavailableSpans;unavailable=true;
                std::cout<<"Original authored unavailable span "<<context<<" name="<<seq.name<<" start="<<seq.startFrame
                    <<" frames="<<seq.numFrames<<" retainedFrames="<<original.animationFrames<<"\n";
                // Exact names can be repeated in UE1 tables; only the first
                // match is selectable, just as pinned UMesh::GetSequence.
                if(ExactSequence(original,seq.name)!=&seq) continue;
                State selected;selected.main.sequence=seq.name;
                const auto early=QuestVr::PrepareMeshPose(mesh,selected);
                Require(early.selectedOriginalSpanInvalid,context+": selected original unavailable span diagnosis lost");
                if(early.drawable) {++partialSpanEarlyPoses;Differential(original,selected,context+" available original early pair in dangling span");}
                else Require(!early.error.empty(),context+": missing frame-pair omission reason lost");
                selected.main.normalizedFrame=seq.numFrames>0 ?
                    (static_cast<float>(seq.numFrames)-0.5f)/static_cast<float>(seq.numFrames) : 0;
                const auto omitted=QuestVr::PrepareMeshPose(mesh,selected);
                Require(!omitted.drawable && !omitted.error.empty(),context+": actually missing original late frame was guessed renderable");
                ++unavailablePoseOmissions;
            }
            legacySpanExports+=unavailable;
            Require(mesh.triangles.size()==original.triangleSourceVertexIndices.size(),context+": retained render topology count differs from draw cache");
            if(original.sequences.empty()) {++noSequences;State state;const auto pose=QuestVr::PrepareMeshPose(mesh,state);
                Require(!pose.drawable && pose.error.empty(),context+": no-sequence original mesh was guessed playable");continue;}
            if(validSequenceIndices.empty()) {++noUsableSpanExports;continue;}
            std::set<std::size_t> selected={validSequenceIndices.front(),validSequenceIndices[validSequenceIndices.size()/2],validSequenceIndices.back()};
            for(auto seqIndex : selected) for(float frame : {0.0f,0.25f,0.875f}) {State state;state.main.sequence=original.sequences[seqIndex].name;
                state.main.normalizedFrame=frame;state.fatness=frame==0.25f ? 144 : 128;
                Differential(original,state,context+" original sequence "+state.main.sequence);
                const auto retainedPose=QuestVr::PrepareMeshPose(mesh,state);
                Require(retainedPose.drawable,context+": retained original sampler rejected valid authored sequence");
                Oracle oracle(original);
                for(auto sourceIndex : original.triangleSourceVertexIndices) {const auto expected=oracle.Sample(state,sourceIndex);
                    Near(retainedPose.objectPositions.at(sourceIndex),expected.first,context+": mapped render vertex sample differs",0.001f);
                    Near(retainedPose.objectNormals.at(sourceIndex),expected.second,context+": mapped render normal sample differs",0.001f);}
            }
            State tween;tween.main.sequence=original.sequences[validSequenceIndices.front()].name;tween.main.normalizedFrame=-0.125f;
            tween.main.previous={0,(original.animationFrames-1)*original.frameVertices,0.375f};
            tween.fatness=136;tween.blends[0].sequence=original.sequences[validSequenceIndices.back()].name;tween.blends[0].normalizedFrame=0.25f;
            Differential(original,tween,context+" native-history tween/additive blend");
            const auto tweenPose=QuestVr::PrepareMeshPose(mesh,tween);
            Require(tweenPose.drawable,context+": retained original native-history tween rejected");
        }
    }
    Require(exports>0,"No owned original mesh exports found");
    std::cout<<"Original animation export audit: meshes="<<exports<<" animated="<<animated<<" packedVertices="<<packed<<" sequences="<<sequences
        <<" notifies="<<notifyCount<<" attachmentCorners="<<attachmentCorners<<" noSequenceOmissions="<<noSequences
        <<" sourceVertices="<<sourceVertices<<" animationFrames="<<animationFrames<<" sequenceFrameRanges="<<sequenceFrameRanges
        <<" retainedBytes="<<retainedBytes<<" packedPayloadBytes="<<packed*sizeof(PortablePackedMeshVertex)
        <<" maximumAssetBytes="<<maximumAssetBytes<<" maximumAsset="<<maximumAsset
        <<" authoredUnavailableSpans="<<unavailableSpans<<" legacySpanExports="<<legacySpanExports
        <<" noUsableFullSpanExports="<<noUsableSpanExports<<" explicitMissingFramePoseOmissions="<<unavailablePoseOmissions
        <<" availablePartialSpanEarlyPoses="<<partialSpanEarlyPoses<<".\n";
}
}
int main(int argc,char** argv) {
    try {
        SyntheticContracts();SharedGeometryContracts();InvalidContracts();
        if(argc==3 && std::string(argv[1])=="--game-root") OriginalMeshes(argv[2]);
        else if(argc!=1) throw std::runtime_error("Usage: quest_mesh_animation_test [--game-root <owned Deus Ex directory>]");
        std::cout<<"Pinned DX mesh sequence/interpolation/fatness/tween/additive blend/normal/remap differential: "<<checks
            <<" checks, "<<rejections<<" rejection controls, "<<differentialVertices<<" source-vertex pose samples, "
            <<poseSampleCalls<<" differential pose calls, "<<sampledDistinctFrames<<" distinct sampled frames, "
            <<normalTriangleSamples<<" normal-triangle samples passed.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 1;}
}
