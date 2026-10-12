#include "quest_decal_projection.h"
#include "quest_decal_state.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using Vec=PortableModelVec3;
std::size_t checks{},refusals{};
void Require(bool condition,const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Refuse(const std::function<void()>& fn,const std::string& message) {
    bool refused{};
    try { fn(); } catch (const std::runtime_error&) { refused=true; }
    Require(refused,message); ++refusals;
}
PortableModelGeometry SplitFloor() {
    PortableModelGeometry model; model.rootOutside=1;
    model.points={{0,0,0},{-10,-10,0},{0,-10,0},{0,10,0},{-10,10,0},
        {0,-10,0},{10,-10,0},{10,10,0},{0,10,0}};
    model.vectors={{0,0,1}};
    model.surfaces.resize(2);
    for (auto& surface:model.surfaces) { surface.basePoint=0; surface.normalVector=0; }
    for (int i=1;i<=8;++i) model.vertices.push_back({i,-1});
    model.nodes.resize(2);
    for (int i=0;i<2;++i) {
        auto& node=model.nodes[static_cast<std::size_t>(i)];
        node.planeZ=1; node.front=node.back=node.plane=-1;
        node.surface=i; node.vertexPool=i*4; node.vertexCount=4;
    }
    model.nodes[0].plane=1;
    return model;
}
QuestVr::DecalProjectionPlan Project(const PortableModelGeometry& model,std::uint32_t& seed,
    const std::vector<std::int32_t>& existing={},Vec direction={1,0,0},int multi=4,
    const QuestVr::DecalProjectionLimits& limits={}) {
    return QuestVr::ProjectOriginalDecalGeometry(model,{-.5f,0,1},{0,0,1},100,direction,4,1,multi,existing,seed,limits);
}
void PrimaryGates() {
    using QuestVr::AcceptOriginalDecalPrimarySurface;
    Require(AcceptOriginalDecalPrimarySurface({0,0,1},{},{},{0,0,1},0),"Valid primary plane refused");
    Require(AcceptOriginalDecalPrimarySurface({0,0,-1},{},{},{0,0,1},0),"Primary applied face-direction restriction");
    Require(!AcceptOriginalDecalPrimarySurface({1,0,0},{},{},{0,0,1},0),"Primary exact zero dot accepted");
    const float threshold=.001f;
    Require(AcceptOriginalDecalPrimarySurface({0,0,1},{0,0,threshold},{},{0,0,1},0),"FLOAT.001 exact boundary refused");
    Require(!AcceptOriginalDecalPrimarySurface({0,0,1},{0,0,std::nextafter(threshold,1.0f)},{},{0,0,1},0),"Above FLOAT residual boundary accepted");
    Require(AcceptOriginalDecalPrimarySurface({0,0,1},{0,0,-threshold},{},{0,0,1},0),"Negative residual boundary refused");
    for (unsigned bit=0;bit<32;++bit)
        Require(AcceptOriginalDecalPrimarySurface({0,0,1},{},{},{0,0,1},1u<<bit)==(bit!=9 && bit!=10),"Wrong numeric PolyFlags mask");
}
void ComposedSplitFloor() {
    auto model=SplitFloor(); std::uint32_t seed=1;
    const auto plan=Project(model,seed);
    Require(seed==1 && plan.randomDraws==0,"Supplied projection changed RNG");
    Require(plan.hasPrimaryGeometry && plan.primaryTraceNode==0 && plan.primarySelectedNode==0,
        "Composed trace/surface selection lost primary identities");
    Require(plan.primaryCenter.x==-.5f && plan.primaryCenter.y==0 && plan.primaryCenter.z==0,"Projection retained backed-off hit center");
    Require(plan.surfaces.size()==2 && plan.surfaces[0].surface==0 && plan.surfaces[0].primary &&
        plan.surfaces[1].surface==1 && !plan.surfaces[1].primary,"Split floor missed ordered secondary surface");
    Require(plan.secondarySamples==16 && plan.secondaryDuplicates==15 && !plan.secondaryMisses &&
        !plan.secondaryFlagRejects && !plan.secondaryAngleRejects,"Secondary grid did not preserve first-attachment uniqueness");
    Require(plan.membershipEntries==2 && plan.surfaces[0].nodeMembership==std::vector<std::int32_t>{0} &&
        plan.surfaces[1].nodeMembership==std::vector<std::int32_t>{1},"Quad clipping lost surface-specific membership");
    for (std::size_t i=0;i<4;++i) {
        Require(plan.surfaces[0].worldCorners[i].z==0 && plan.surfaces[1].worldCorners[i].z==0,"Parallel secondary correction left plane");
        Require(plan.surfaces[0].worldCorners[i].x==plan.surfaces[1].worldCorners[i].x &&
            plan.surfaces[0].worldCorners[i].y==plan.surfaces[1].worldCorners[i].y,"Whole secondary quad was pre-clipped/replaced");
    }
    const auto existing=Project(model,seed,{1,1,0});
    Require(existing.surfaces.size()==1 && existing.surfaces[0].primary && existing.secondaryDuplicates==16,
        "Primary duplicates suppressed or existing secondary duplicates ignored");
    model.surfaces[1].polyFlags=0x200;
    const auto flagged=Project(model,seed);
    Require(flagged.surfaces.size()==1 && flagged.secondaryFlagRejects==4 && flagged.secondaryDuplicates==12,
        "Rejected secondary flag surface incorrectly entered SurfList");
    model.surfaces[0].polyFlags=0x400;
    const auto primaryFlag=Project(model,seed,{}, {},4);
    Require(!primaryFlag.hasPrimaryGeometry && primaryFlag.surfaces.empty() && primaryFlag.randomDraws==6 &&
        seed==1030492215u,"Ordinary primary flag rejection incorrectly rolled back sampled RNG");
    model=SplitFloor(); seed=1;
    const auto primaryOnly=Project(model,seed,{}, {1,0,0},-7);
    Require(primaryOnly.hasPrimaryGeometry && primaryOnly.clampedMultiLevel==-7 && primaryOnly.secondarySamples==0 &&
        primaryOnly.surfaces.size()==1,"Negative multilevel not preserved as primary-only");
}
void PlanToOrderedState() {
    const auto model=SplitFloor(); std::uint32_t seed=1;
    const auto plan=Project(model,seed);
    QuestVr::DecalState state(model.surfaces.size(),model.nodes.size());
    const auto resolver=[](const std::string&) -> std::optional<std::string> { return "Fixture.Texture"; };
    const auto publish=[&](const QuestVr::DecalProjectionPlan& candidate) {
        for (const auto& projected:candidate.surfaces) {
            QuestVr::DecalSurfaceRecord record; record.ownerIdentity="Fixture.Shadow";
            record.nodeMembership=projected.nodeMembership;
            for (std::size_t i=0;i<4;++i) {
                const auto p=projected.relativeCorners[i]; record.relativeCorners[i]={p.x,p.y,p.z};
            }
            state.AttachPrimary(projected.surface,record,"Fixture.Texture",resolver);
        }
    };
    publish(plan);
    const auto secondPlan=Project(model,seed,state.ReadOnlySnapshot().owners[0].surfaces);
    publish(secondPlan);
    const auto& saved=state.ReadOnlySnapshot();
    Require(saved.owners.size()==1 && saved.owners[0].surfaces==std::vector<std::int32_t>({0,1,0}),
        "Repeated projection lost duplicate primary/unique secondary SurfList entries");
    Require(saved.surfaces[0].size()==2 && saved.surfaces[1].size()==1,"Repeated projection replaced primary or duplicated secondary");
    const auto detached=state.Detach("Fixture.Shadow");
    Require(detached.recordsRemoved==3 && detached.surfListEntriesPopped==3 &&
        state.ReadOnlySnapshot().owners[0].surfaces.empty() && state.Usage().records==0,
        "Projected records did not detach exact ordered repeated graph");
}
void AcceptedEmptyMembership() {
    PortableModelGeometry model; model.rootOutside=1;
    model.points={{0,0,0},{1,0,0},{0,1,0},{0,0,0},{1,0,0},{0,-1,0},{0,0,0}};
    model.vectors={{0,0,1}}; model.surfaces.resize(1); model.nodes.resize(1);
    for (int i=1;i<=6;++i) model.vertices.push_back({i,-1});
    auto& node=model.nodes[0]; node.planeZ=1; node.front=node.back=node.plane=-1; node.vertexCount=6;
    std::uint32_t seed=1;
    const auto plan=QuestVr::ProjectOriginalDecalGeometry(model,{0,0,1},{0,0,1},100,{1,0,0},4,0,0,{},seed);
    Require(plan.hasPrimaryGeometry && plan.surfaces.size()==1 && plan.surfaces[0].nodeMembership.empty() &&
        plan.membershipEntries==0,"Accepted zero-sign-bridged polygon lost unrestricted empty membership");
    QuestVr::DecalState state(1,1); QuestVr::DecalSurfaceRecord record; record.ownerIdentity="Fixture.EmptyShadow";
    for (std::size_t i=0;i<4;++i) {
        const auto p=plan.surfaces[0].relativeCorners[i]; record.relativeCorners[i]={p.x,p.y,p.z};
    }
    state.AttachPrimary(0,record,"Fixture.Texture",[](const std::string&) -> std::optional<std::string> { return "Fixture.Texture"; });
    Require(state.Usage().records==1 && state.ReadOnlySnapshot().surfaces[0][0].nodeMembership.empty(),
        "Unrestricted empty membership discarded on publication");
    Require(state.Detach(record.ownerIdentity).recordsRemoved==1 && !state.Usage().records,"Empty membership attachment did not detach");
}
void AtomicRefusalAndMiss() {
    auto model=SplitFloor(); std::uint32_t seed=1;
    QuestVr::DecalProjectionLimits limits; limits.workSteps=1;
    Refuse([&] { Project(model,seed,{}, {},4,limits); },"Projection work bound did not refuse");
    Require(seed==1,"Work-bound failure published random seed");
    limits={}; limits.surfListEntries=1;
    Refuse([&] { Project(model,seed,{}, {},4,limits); },"Whole-plan SurfList bound did not refuse");
    Require(seed==1,"Later secondary SurfList refusal advanced seed");
    limits={}; limits.membershipEntries=1;
    Refuse([&] { Project(model,seed,{}, {},4,limits); },"Whole-plan membership bound did not refuse");
    Require(seed==1,"Later secondary membership refusal advanced seed");
    Refuse([&] { Project(model,seed,{-1},{}); },"Invalid reflected SurfList index accepted");
    Refuse([&] { Project(model,seed,{2},{}); },"Out of range reflected SurfList index accepted");
    Require(seed==1,"Invalid SurfList advanced seed");
    model.nodes[0].front=0;
    Refuse([&] { Project(model,seed,{},{}); },"Cyclic BSP plan accepted");
    Require(seed==1,"Cyclic BSP plan advanced seed");
    model=SplitFloor();
    limits={}; limits.surface.querySteps=0;
    Refuse([&] { QuestVr::ProjectOriginalDecalGeometry(model,{-.5f,0,1},{0,0,-1},100,{},4,1,4,{},seed,limits); },
        "Invalid nested surface limits accepted on clear ray");
    Require(seed==1,"Invalid nested limits published pretrace draws on clear ray");
    limits={}; limits.workSteps=10;
    Refuse([&] { Project(model,seed,{}, {1,0,0},4,limits); },"Aggregate remaining helper budget did not bound validation/query");
    Require(seed==1,"Tight subordinate budget changed seed");
    const auto miss=QuestVr::ProjectOriginalDecalGeometry(model,{-.5f,0,1},{0,0,-1},100,{},4,1,4,{},seed);
    Require(!miss.hasPrimaryGeometry && miss.surfaces.empty() && miss.randomDraws==6 && seed==1030492215u,
        "Ordinary clear ray incorrectly removed original pretrace draws");
    seed=1;
    const auto noPolygon=QuestVr::ProjectOriginalDecalGeometry(model,{50,0,1},{0,0,1},100,{},4,1,4,{},seed);
    Require(!noPolygon.hasPrimaryGeometry && noPolygon.primaryTraceNode==0 && seed==1030492215u,
        "Point-trace hit outside polygon bypassed actual coplanar selection");
}
void ReentrantPhases() {
    using namespace QuestVr;
    auto model=SplitFloor(); std::uint32_t seed=1;
    const auto prepared=PrepareOriginalDecalPrimary(model,{-.5f,0,1},{0,0,1},100,{},seed);
    Require(prepared.hasPrimaryGeometry && prepared.primaryTraceNode==0 && prepared.selectedNode==0 && prepared.surface==0,
        "Pre-callback selection lost authored identities");
    Require(prepared.center.x==-.5f && prepared.center.z==0 && prepared.surfaceBase.x==0 && prepared.surfaceNormal.z==1,
        "Pre-callback phase did not retain selected center/base/normal");
    Require(prepared.randomDraws==6 && prepared.preparedDirection.randomized && seed==1030492215u,
        "Successful pretrace RNG was not visible before callback");
    const auto beforeCallbackSeed=seed;
    ScriptRandom::Draw15(seed);
    const auto callbackSeed=seed;
    const auto small=BuildOriginalDecalPrimaryQuad(prepared.center,prepared.surfaceBase,prepared.surfaceNormal,
        4,1,prepared.preparedDirection);
    const auto large=BuildOriginalDecalPrimaryQuad(prepared.center,prepared.surfaceBase,prepared.surfaceNormal,
        12,2,prepared.preparedDirection);
    Require(large.radius==small.radius*6 && seed==callbackSeed && seed!=beforeCallbackSeed,
        "Post-callback quad froze USize/DrawScale or overwrote callback RNG");
    DecalProjectionWork work; work.workSteps=prepared.workSteps;
    const auto primary=BuildOriginalDecalPrimaryMembership(model,prepared.surface,small,work);
    Require(primary.primary && primary.surface==0 && primary.nodeMembership==std::vector<std::int32_t>{0},
        "Separate primary membership altered the whole quad");
    std::vector<std::int32_t> liveList{prepared.surface};
    for (int i=0;i<5;++i) for (int j=0;j<5;++j) {
        const auto secondary=ProcessOriginalDecalSecondarySample(model,small.worldCorners,prepared.surfaceNormal,
            5,i,j,liveList,work);
        if (secondary.status==DecalSecondarySampleStatus::Accepted) liveList.push_back(secondary.projected.surface);
    }
    Require(work.secondarySamples==25 && liveList==std::vector<std::int32_t>({0,1}) && work.secondaryDuplicates==24,
        "Post-callback Multi5 was reclamped or live SurfList was frozen");
    const auto rawFive=BuildOriginalDecalSecondarySample(small.worldCorners,prepared.surfaceNormal,5,0,0);
    const auto oldFour=BuildOriginalDecalSecondaryGrid(small.worldCorners,prepared.surfaceNormal,5);
    Require(rawFive.point.x!=oldFour[0].point.x || rawFive.point.y!=oldFour[0].point.y,
        "Raw Multi5 fractions still used clamped denominator four");
    const auto lastFive=BuildOriginalDecalSecondarySample(small.worldCorners,prepared.surfaceNormal,5,4,4);
    Require(lastFive.outer==4 && lastFive.inner==4 && lastFive.point.x==small.worldCorners[2].x &&
        lastFive.point.y==small.worldCorners[2].y,"Raw Multi grid did not include the far corner");
    const auto originalPlan=Project(model,seed,{}, {1,0,0},4);
    std::uint32_t phaseSeed=seed;
    const auto fixedPrepared=PrepareOriginalDecalPrimary(model,{-.5f,0,1},{0,0,1},100,{1,0,0},phaseSeed);
    const auto fixedQuad=BuildOriginalDecalPrimaryQuad(fixedPrepared.center,fixedPrepared.surfaceBase,
        fixedPrepared.surfaceNormal,4,1,fixedPrepared.preparedDirection);
    DecalProjectionWork fixedWork; fixedWork.workSteps=fixedPrepared.workSteps;
    std::vector<DecalProjectedSurface> fixedSurfaces{
        BuildOriginalDecalPrimaryMembership(model,fixedPrepared.surface,fixedQuad,fixedWork)};
    std::vector<std::int32_t> fixedList{fixedPrepared.surface};
    for (int i=0;i<4;++i) for (int j=0;j<4;++j) {
        const auto secondary=ProcessOriginalDecalSecondarySample(model,fixedQuad.worldCorners,fixedPrepared.surfaceNormal,
            4,i,j,fixedList,fixedWork);
        if (secondary.status==DecalSecondarySampleStatus::Accepted) {
            fixedList.push_back(secondary.projected.surface); fixedSurfaces.push_back(secondary.projected);
        }
    }
    Require(fixedWork.workSteps==originalPlan.workSteps && fixedWork.membershipEntries==originalPlan.membershipEntries &&
        fixedWork.secondarySamples==originalPlan.secondarySamples && fixedWork.secondaryDuplicates==originalPlan.secondaryDuplicates &&
        fixedSurfaces.size()==originalPlan.surfaces.size(),"Phased and composed budgets/record order diverged");
    for (std::size_t s=0;s<fixedSurfaces.size();++s) for (std::size_t c=0;c<4;++c) {
        const auto a=fixedSurfaces[s].relativeCorners[c],b=originalPlan.surfaces[s].relativeCorners[c];
        Require(a.x==b.x && a.y==b.y && a.z==b.z && fixedSurfaces[s].nodeMembership==originalPlan.surfaces[s].nodeMembership,
            "Phased and composed exact corners/membership diverged");
    }
    DecalProjectionLimits limits; limits.secondarySamples=1;
    DecalProjectionWork capped;
    const auto first=ProcessOriginalDecalSecondarySample(model,fixedQuad.worldCorners,fixedPrepared.surfaceNormal,
        5,0,0,{0,1},capped,limits);
    Require(first.status==DecalSecondarySampleStatus::Duplicate && capped.secondarySamples==1,
        "Live SurfList duplicate check ignored current entries");
    const auto before=capped;
    Refuse([&] { ProcessOriginalDecalSecondarySample(model,fixedQuad.worldCorners,fixedPrepared.surfaceNormal,
        5,0,1,{0,1},capped,limits); },"Raw Multi sample loop ignored the fixed sample cap");
    Require(capped.workSteps==before.workSteps && capped.secondarySamples==before.secondarySamples,
        "Sample budget exception partially published work");
    for (const auto multi:{-7,0})
        Refuse([&] { BuildOriginalDecalSecondarySample(fixedQuad.worldCorners,fixedPrepared.surfaceNormal,multi,0,0); },
            "Nonpositive live Multi manufactured a secondary sample");
    Refuse([&] { BuildOriginalDecalSecondarySample(fixedQuad.worldCorners,fixedPrepared.surfaceNormal,5,5,0); },
        "Out of range raw grid index accepted");
    DecalProjectionWork invalidWork; invalidWork.workSteps=std::numeric_limits<std::size_t>::max();
    Refuse([&] { BuildOriginalDecalPrimaryMembership(model,0,fixedQuad,invalidWork); },
        "Overflowed phase work counter accepted");
    std::uint32_t failedSeed=1; limits={}; limits.workSteps=1;
    Refuse([&] { PrepareOriginalDecalPrimary(model,{-.5f,0,1},{0,0,1},100,{},failedSeed,limits); },
        "Pretrace phase ignored bounded random preparation");
    Require(failedSeed==1,"Rejected pretrace phase published RNG");
    limits={}; const auto miss=PrepareOriginalDecalPrimary(model,{-.5f,0,1},{0,0,-1},100,{},failedSeed,limits);
    Require(!miss.hasPrimaryGeometry && failedSeed==1030492215u && miss.randomDraws==6,
        "Phased ordinary miss did not publish successful pretrace draws");
}
} // namespace
int main() {
    try {
        PrimaryGates(); ComposedSplitFloor(); PlanToOrderedState(); AcceptedEmptyMembership(); AtomicRefusalAndMiss(); ReentrantPhases();
        std::cout<<"PASS original composed decal geometry checks="<<checks<<" expected-refusals="<<refusals
            <<"; trace/selection/gates/quad/grid/clip/order/atomic RNG; not native/renderer/GC/save completion\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n'; return 1; }
}
