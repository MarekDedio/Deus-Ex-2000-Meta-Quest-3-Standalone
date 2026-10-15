#include "Precomp.h"
#include "GC/GC.h"
#include "quest_npc_animation.h"
#include "actor_scene_preview.h"

#include <chrono>
#include <fstream>
#include <iostream>

namespace {
using namespace QuestVr;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& message) { ++checks; if(!value) throw std::runtime_error(message); }
template<class F> void Reject(F operation) {
    bool rejected{};try {operation();} catch(const std::exception&) {rejected=true;}
    Require(rejected,"Expected NPC animation rejection");++refusals;
}
std::vector<char> Bytes(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot read owned input");
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
struct Checkpoints {
    std::filesystem::path directory,before,after;
    Checkpoints() {
        const auto parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        directory=parent/("quest-npc-waiting-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(directory.parent_path()==parent && std::filesystem::create_directory(directory),"Cannot create owned checkpoints");
        before=directory/"before.sav";after=directory/"after.sav";
    }
    ~Checkpoints() {
        std::error_code error;std::filesystem::remove(before,error);std::filesystem::remove(after,error);
        std::filesystem::remove(directory,error);
    }
};
PortableLodMesh SyntheticMesh() {
    PortableLodMesh mesh;mesh.scaleX=mesh.scaleY=mesh.scaleZ=1.0f;
    auto data=std::make_shared<PortableMeshAnimationData>();
    data->frameVertices=3u;data->animationFrames=2u;data->scale={1,1,1};
    data->frameVerticesPacked={{0,0,0},{10,0,0},{0,10,0},{0,0,4},{10,0,4},{0,10,4}};
    data->normalTopology={{0,1,2}};data->triangleSourceVertexIndices={0,1,2};
    data->sequences.push_back({"AuthoredWaiting","",0,2,2.0f,{},false});
    mesh.animation=data;mesh.triangles.resize(3u);return mesh;
}
void Synthetic() {
    auto mesh=SyntheticMesh();PortableActorWaitingPresentation waiting;
    waiting.available=true;waiting.meshPath="Fixture.Mesh";waiting.animation.sequence="AuthoredWaiting";
    waiting.animation.rate=1.0f;waiting.animation.loop=true;
    const auto clip=BuildNpcAnimationClip(mesh,waiting.meshPath,waiting,128u);
    Require(clip.vertices==3u && clip.frames==2u && clip.positionsRgba.size()==24u,"Clip dimensions differ");
    Require(clip.positionsRgba[2]==0.0f && clip.positionsRgba[14]==4.0f,"Authored source positions differ");
    for(unsigned i=0u;i<10000u;++i) {
        const double time=static_cast<double>(i)/1000.0;
        const auto frame=SampleNpcAnimationFrame(clip,time);
        Require(frame.first<2u && frame.second<2u && frame.fraction>=0.0f && frame.fraction<1.0f,"Loop frame escaped bounds");
        Require(std::fabs(frame.fraction-static_cast<float>(std::fmod(time*2.0,1.0)))<0.000001f,"Frame fraction differs");
    }
    auto stopped=clip;stopped.loop=false;
    const auto end=SampleNpcAnimationFrame(stopped,100.0);
    Require(end.first==1u && end.second==1u && end.fraction==0.0f,"One-shot frame must clamp, not loop");
    for(const auto time:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
        Reject([&]{SampleNpcAnimationFrame(clip,time);});
    Reject([&]{BuildNpcAnimationClip(mesh,"Fixture.OtherMesh",waiting,128u);});
    Reject([&]{BuildNpcAnimationClip(mesh,waiting.meshPath,waiting,129u);});
    waiting.animation.sequence="GuessedMissing";
    Reject([&]{BuildNpcAnimationClip(mesh,waiting.meshPath,waiting,128u);});
    waiting.animation.sequence="AuthoredWaiting";waiting.animation.blends[0].sequence="Other";
    Reject([&]{BuildNpcAnimationClip(mesh,waiting.meshPath,waiting,128u);});
    waiting.animation.blends[0].sequence.clear();
    auto withNotify=std::make_shared<PortableMeshAnimationData>(*mesh.animation);
    withNotify->sequences[0].notifies.push_back({0.5f,"FootStep"});mesh.animation=withNotify;
    Reject([&]{BuildNpcAnimationClip(mesh,waiting.meshPath,waiting,128u);});
    PortableActorSnapshot actor;actor.objectPath="Map.NPC";actor.meshPath=waiting.meshPath;
    actor.pawn=true;actor.drawType=2u;
    NpcWaitingInstance instance{actor.objectPath,actor.meshPath,"Original.Helper",{},nullptr,actor.animation,{}};
    Require(NpcWaitingMatchesSnapshot(instance,actor),"Exact original presentation binding refused");
    auto changed=actor;changed.animation.sequence="ScriptChanged";
    Require(!NpcWaitingMatchesSnapshot(instance,changed),"Original runtime command was overwritten by waiting bootstrap");
    changed=actor;changed.animByOwner=true;
    Require(!NpcWaitingMatchesSnapshot(instance,changed),"Owner animation was replaced by unrelated waiting pose");
    changed=actor;changed.hidden=true;
    Require(!NpcWaitingMatchesSnapshot(instance,changed),"Hidden actor used waiting bootstrap");
    auto exact=SyntheticMesh();
    Require(NeedsNpcWaitingBootstrap(exact,actor),"Unresolved reference pose did not request waiting bootstrap");
    actor.animation.sequence="AuthoredWaiting";
    Require(!NeedsNpcWaitingBootstrap(exact,actor),"Valid original map/script pose was replaced by waiting bootstrap");
    actor.animation.frame=-0.1f;
    Require(!NeedsNpcWaitingBootstrap(exact,actor),"Saved original tween was replaced by a guessed idle");
}
void Original(const std::filesystem::path& root,const std::filesystem::path& preview) {
    Checkpoints checkpoints;
    std::vector<PortablePackageTables> packages;
    std::map<std::filesystem::path,std::vector<char>> beforeSources;
    for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire","MPCharacters","DeusExCharacters","DeusExItems","DeusExDeco","DeusExSounds","DeusExUI"}) {
        const auto path=root/"System"/(std::string(name)+".u");beforeSources.emplace(path,Bytes(path));
        packages.push_back(LoadPortablePackageTables(path.string()));
    }
    const auto map=root/"Maps"/"00_Training.dx";beforeSources.emplace(map,Bytes(map));
    Require(InitializePortableRuntime(packages).passed && LoadPortableRuntimeMap(LoadPortablePackageTables(map.string())).passed,
        "Original Training initialization failed");
    Require(DecodePortableRuntimeActorMeshes().passed,"Original actor mesh decoding failed");
    Require(SavePortableRuntimeState(checkpoints.before.string()),"Cannot retain original baseline");
    const auto baseline=Bytes(checkpoints.before);const auto revision=GetPortableRuntimeWorldRevision();
    const auto actors=GetPortableRuntimeMapActors();std::vector<std::string> errors;
    for(const auto& actor:actors) if(actor.pawn&&!actor.meshPath.empty())
        std::cout<<"NPC input "<<actor.objectPath<<" drawType="<<static_cast<unsigned>(actor.drawType)
            <<" hidden="<<actor.hidden<<" animByOwner="<<actor.animByOwner<<" fatness="<<static_cast<unsigned>(actor.fatness)<<'\n';
    const auto waiting=PrepareNpcWaitingPresentations(actors,&errors);
    for(const auto& error:errors) std::cout<<"NPC refusal "<<error<<'\n';
    std::cout<<"Original Training waiting presentations="<<waiting.size()<<'\n';
    Require(waiting.size()==18u && errors.empty(),"Not all18 original Training NPCs received authored waiting presentations");
    Require(SavePortableRuntimeState(checkpoints.after.string()) && Bytes(checkpoints.after)==baseline &&
        GetPortableRuntimeWorldRevision()==revision && !GetPortableRuntimeUnsavedScriptState(),
        "Waiting preview changed persistent world/RNG/animation/state/birth/revision");
    std::set<const NpcAnimationClip*> unique;
    std::set<std::string> originalMeshes;
    for(const auto& [path,instance]:waiting) {
        const auto found=std::find_if(actors.begin(),actors.end(),[&](const auto& a){return a.objectPath==path;});
        Require(found!=actors.end() && NpcWaitingMatchesSnapshot(instance,*found),"Prepared NPC no longer matches original snapshot");
        const auto& clip=*instance.clip;unique.insert(&clip);
        originalMeshes.insert(instance.meshPath);
        Require(clip.sequence!="All" && clip.sequence!="Fighter","NPC retained missing/default T-pose sequence");
        const auto& mesh=GetPortableRuntimeMeshView(instance.meshPath);
        float maximumMotion{};
        for(unsigned sample=0u;sample<15u;++sample) {
            const double time=sample*0.073;
            const auto gpu=SampleNpcAnimationFrame(clip,time);
            MeshAnimationState state;state.main.sequence=clip.sequence;
            state.main.normalizedFrame=(gpu.first+gpu.fraction)/clip.frames;
            const auto pose=PrepareMeshPose(*mesh.animation,state);
            Require(pose.drawable&&!pose.fallbackUsed,"Original CPU waiting sample unavailable");
            for(std::size_t vertex=0u;vertex<clip.vertices;++vertex) for(std::size_t axis=0u;axis<3u;++axis) {
                const auto a=clip.positionsRgba.at((gpu.first*clip.vertices+vertex)*4u+axis);
                const auto b=clip.positionsRgba.at((gpu.second*clip.vertices+vertex)*4u+axis);
                const auto actual=a+(b-a)*gpu.fraction;
                const auto& p=pose.objectPositions[vertex];const float expected=axis==0u?p.x:axis==1u?p.y:p.z;
                Require(std::fabs(actual-expected)<0.0002f,"GPU source-index/linear morph differs from original CPU sampling");
                maximumMotion=std::max(maximumMotion,std::fabs(actual-clip.positionsRgba.at(vertex*4u+axis)));
            }
        }
        if(clip.loop&&clip.frames>1u) Require(maximumMotion>0.01f,"Original breathing presentation remains static");
        QuestVr::Vm::Limits limited;limited.instructions=1u;
        const auto refused=PreviewPortableActorWaitingPresentation(path,limited);
        Require(!refused.available&&!refused.error.empty(),"Failed bounded preview returned partial pose");++refusals;
        Require(SavePortableRuntimeState(checkpoints.after.string()) && Bytes(checkpoints.after)==baseline &&
            GetPortableRuntimeWorldRevision()==revision,"Rejected preview leaked writes");
        std::cout<<"NPC "<<path<<" helper="<<instance.function<<" sequence="<<clip.sequence<<" frames="<<clip.frames
            <<" rate="<<clip.normalizedRate<<" loop="<<clip.loop<<" motion="<<maximumMotion<<'\n';
        if(!preview.empty() && (path=="00_Training.JaimeReyes0" || path=="00_Training.Doctor1" || path=="00_Training.UNATCOTroop0")) {
            auto actor=*found;actor.animation={};actor.animation.sequence=clip.sequence;
            actor.pitch=actor.yaw=actor.roll=0;
            actor.prePivotX=instance.prePivot.x;actor.prePivotY=instance.prePivot.y;actor.prePivotZ=instance.prePivot.z;
            for(unsigned view=0u;view<2u;++view) {
                actor.animation.frame=view==0u?0.0f:0.35f;
                questvisual::Scene scene;
                const auto metadata=questvisual::AppendActorScenePreview(scene,{actor},{actor.x,actor.y,actor.z},{},false);
                Require(metadata.meshInstances==1u && metadata.poseOmissions==0u && metadata.missingMaterials==0u,
                    "NPC visual preview lost geometry/materials");
                questvisual::Camera camera;camera.position={0,1,-2.3f};camera.yawDegrees=180;camera.verticalFovDegrees=55;
                const auto rendered=questvisual::Render(scene,camera,720,720);
                Require(rendered.coveredPixels>10000u,"NPC visual preview is empty");
                std::filesystem::create_directories(preview);
                questvisual::WriteBmp(preview/(path+'-'+std::to_string(view)+".bmp"),rendered.image);
            }
        }
    }
    Require(unique.size()==originalMeshes.size() && unique.size()<waiting.size(),
        "Training clips were not deduplicated across original mesh/sequence bindings");
    const auto scripted=ExecutePortableActorFunction("00_Training.Doctor1","LoopAnim",{
        {QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::Name,waiting.at("00_Training.Doctor1").clip->sequence),{}}});
    Require(scripted.passed(),"Cannot establish an actual original animation command for preservation test");
    Require(SavePortableRuntimeState(checkpoints.before.string()),"Cannot retain original-command baseline");
    const auto commandBaseline=Bytes(checkpoints.before);const auto commandRevision=GetPortableRuntimeWorldRevision();
    errors.clear();const auto afterCommand=PrepareNpcWaitingPresentations(GetPortableRuntimeMapActors(),&errors);
    Require(afterCommand.size()==17u && errors.empty() && !afterCommand.contains("00_Training.Doctor1"),
        "Waiting bootstrap overrode an actual running original NPC command");
    Require(SavePortableRuntimeState(checkpoints.after.string()) && Bytes(checkpoints.after)==commandBaseline &&
        GetPortableRuntimeWorldRevision()==commandRevision,"Presentation changed the existing original animation command");
    ShutdownPortableRuntime();for(const auto& [path,bytes]:beforeSources) Require(Bytes(path)==bytes,"NPC preview changed original packages");
}
} // namespace
int main(int argc,char** argv) {
    const auto gcBefore=GC::GetStats().numObjects;
    try {
        Synthetic();
        if(argc>1) Original(argv[1],argc>2?std::filesystem::path(argv[2]):std::filesystem::path{});
        ShutdownPortableRuntime();GC::Collect();Require(GC::GetStats().numObjects==gcBefore,"NPC presentation leaked GC roots");
        std::cout<<"PASS NPC original waiting presentation "<<checks<<" checks, "<<refusals<<" refusals; render-only, not AI/startup\n";
        return 0;
    } catch(const std::exception& error) {
        ShutdownPortableRuntime();std::cerr<<"NPC animation failed: "<<error.what()<<'\n';return 1;
    }
}
