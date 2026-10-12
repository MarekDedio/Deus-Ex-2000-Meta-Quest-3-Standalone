#pragma once

#include "quest_decal_secondary_quad.h"
#include "quest_decal_surface_select.h"
#include "quest_model_point_trace.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace QuestVr {

// Geometry-only plan for original Attach after its truthful viewport/Texture
// guards. RotationXAxis comes from the original rotator transform consumer.
// No actor, reflected property, owner graph, renderer, mover base, or save state
// is changed here. A reentrant host must publish its provisional primary before
// SetBase, then read the live Texture/DrawScale/Multi/SurfList afterwards. The
// composed fixed-input query below is not a replacement for that ordering.
struct DecalProjectionLimits {
    ModelPointTraceLimits trace;
    DecalSurfaceSelectLimits surface;
    DecalDirectionLimits direction;
    std::size_t surfListEntries{262'144u}, membershipEntries{1'048'576u};
    std::size_t workSteps{512u * 1024u * 1024u};
    // Independent bound: an empty model can trace a sample with zero helper
    // work. Do not permit callback-modified Multi to make that loop unbounded.
    std::size_t secondarySamples{65'536u};
};
struct DecalProjectedSurface {
    std::int32_t surface{-1};
    bool primary{};
    std::array<PortableModelVec3,4> worldCorners{},relativeCorners{};
    std::vector<std::int32_t> nodeMembership;
};
struct DecalProjectionWork {
    std::size_t secondarySamples{},secondaryMisses{},secondaryDuplicates{},secondaryFlagRejects{},secondaryAngleRejects{};
    std::size_t workSteps{},membershipEntries{};
};
struct DecalProjectionPlan : DecalProjectionWork {
    bool hasPrimaryGeometry{};
    std::int32_t clampedMultiLevel{};
    std::int32_t primaryTraceNode{-1},primarySelectedNode{-1};
    PortableModelVec3 primaryCenter;
    std::vector<DecalProjectedSurface> surfaces;
    std::size_t randomDraws{};
};
struct DecalPreparedPrimary {
    bool hasPrimaryGeometry{};
    std::int32_t primaryTraceNode{-1},selectedNode{-1},surface{-1};
    PortableModelVec3 center,surfaceBase,surfaceNormal;
    DecalPreparedDirection preparedDirection;
    ModelPointTraceStats traceStats;
    DecalSurfaceSelectStats surfaceStats;
    std::size_t randomDraws{},workSteps{};
};
enum class DecalSecondarySampleStatus { Accepted, Miss, Duplicate, FlagRejected, AngleRejected };
struct DecalProcessedSecondarySample {
    DecalSecondarySampleStatus status{DecalSecondarySampleStatus::Miss};
    DecalSecondarySample sample;
    DecalProjectedSurface projected;
};

namespace DecalProjectionDetail {
inline void ValidateLimits(const DecalProjectionLimits& limits) {
    using DecalPrimaryDetail::Fail;
    const DecalProjectionLimits maximum;
    const auto bounded=[](std::size_t value,std::size_t bound) { return value && value<=bound; };
    if (!bounded(limits.surfListEntries,maximum.surfListEntries) ||
        !bounded(limits.membershipEntries,maximum.membershipEntries) ||
        !bounded(limits.workSteps,maximum.workSteps) ||
        !bounded(limits.secondarySamples,maximum.secondarySamples)) Fail("invalid projection plan limits");
    if (!bounded(limits.trace.nodes,maximum.trace.nodes) ||
        !bounded(limits.trace.validationSteps,maximum.trace.validationSteps) ||
        !bounded(limits.trace.querySteps,maximum.trace.querySteps) ||
        !bounded(limits.surface.nodes,maximum.surface.nodes) ||
        !bounded(limits.surface.surfaces,maximum.surface.surfaces) ||
        !bounded(limits.surface.points,maximum.surface.points) ||
        !bounded(limits.surface.vectors,maximum.surface.vectors) ||
        !bounded(limits.surface.vertices,maximum.surface.vertices) ||
        !bounded(limits.surface.validationSteps,maximum.surface.validationSteps) ||
        !bounded(limits.surface.querySteps,maximum.surface.querySteps) ||
        !bounded(limits.direction.attempts,maximum.direction.attempts)) Fail("invalid nested projection limits");
}
inline void ValidateWork(const DecalProjectionWork& work,const DecalProjectionLimits& limits) {
    ValidateLimits(limits);
    if (work.workSteps>limits.workSteps || work.membershipEntries>limits.membershipEntries ||
        work.secondarySamples>limits.secondarySamples || work.secondaryMisses>work.secondarySamples ||
        work.secondaryDuplicates>work.secondarySamples || work.secondaryFlagRejects>work.secondarySamples ||
        work.secondaryAngleRejects>work.secondarySamples) DecalPrimaryDetail::Fail("invalid projection work counters");
}
inline void Charge(DecalProjectionWork& work,const DecalProjectionLimits& limits,const std::size_t steps) {
    if (work.workSteps>limits.workSteps || steps>limits.workSteps-work.workSteps)
        DecalPrimaryDetail::Fail("projection work budget exhausted");
    work.workSteps+=steps;
}
inline ModelPointTraceLimits TraceLimits(const DecalProjectionWork& work,const DecalProjectionLimits& limits) {
    const auto remaining=limits.workSteps-work.workSteps;
    if (remaining<2u) DecalPrimaryDetail::Fail("projection work budget exhausted");
    auto result=limits.trace;
    result.validationSteps=std::min(result.validationSteps,remaining/2u);
    result.querySteps=std::min(result.querySteps,remaining-remaining/2u);
    return result;
}
inline DecalSurfaceSelectLimits SurfaceLimits(const DecalProjectionWork& work,const DecalProjectionLimits& limits) {
    const auto remaining=limits.workSteps-work.workSteps;
    if (remaining<2u) DecalPrimaryDetail::Fail("projection work budget exhausted");
    auto result=limits.surface;
    result.validationSteps=std::min(result.validationSteps,remaining/2u);
    result.querySteps=std::min(result.querySteps,remaining-remaining/2u);
    return result;
}
inline PortableModelVec3 NormalizeRay(const PortableModelVec3 direction) {
    using namespace DecalPrimaryDetail;
    // Ray normalization103E742A differs from quad/grid normalization: Z^2,
    // then Y^2, then X^2. Retain that order independently of other consumers.
    const double square=(static_cast<double>(direction.z)*direction.z+
        static_cast<double>(direction.y)*direction.y)+static_cast<double>(direction.x)*direction.x;
    return square<1e-8 ? direction : Scale(direction,1.0/std::sqrt(square));
}
} // namespace DecalProjectionDetail

inline bool AcceptOriginalDecalPrimarySurface(const PortableModelVec3 rotationXAxis,
    const PortableModelVec3 center,const PortableModelVec3 base,const PortableModelVec3 normal,
    const std::uint32_t polyFlags) {
    using namespace DecalPrimaryDetail;
    if (!Finite(rotationXAxis) || !Finite(center) || !Finite(base) || !Finite(normal)) Fail("nonfinite primary acceptance input");
    const double facing=(static_cast<double>(rotationXAxis.y)*normal.y+
        static_cast<double>(rotationXAxis.z)*normal.z)+static_cast<double>(rotationXAxis.x)*normal.x;
    if (!std::isfinite(facing)) Fail("primary facing overflow");
    if (facing==0.0) return false;
    const auto offset=Subtract(base,center);
    const double residual=(static_cast<double>(offset.x)*normal.x+static_cast<double>(offset.z)*normal.z)+
        static_cast<double>(offset.y)*normal.y;
    if (!std::isfinite(residual)) Fail("primary residual overflow");
    // Original compares with FLOAT0.001 (bits3A83126F), not the DOUBLE0.001
    // used by the earlier UModel point-recursion distance thresholds.
    return std::fabs(residual)<=static_cast<double>(0.001f) && !(polyFlags&0x600u);
}

// Pre-callback phase: samples the optional direction and selects the accepted
// primary plane, but deliberately never reads texture dimensions or DrawScale.
// Ordinary misses and successful selection publish RNG immediately. Exceptions
// publish neither RNG nor a result. priorWorkSteps supports the fixed-input
// composition's already-validated existing SurfList; normal host calls use zero.
inline DecalPreparedPrimary PrepareOriginalDecalPrimary(const PortableModelGeometry& model,
    const PortableModelVec3 location,const PortableModelVec3 rotationXAxis,const float traceDistance,
    const PortableModelVec3 optionalDirection,std::uint32_t& seed,
    const DecalProjectionLimits& limits = {},const std::size_t priorWorkSteps = 0u) {
    using namespace DecalPrimaryDetail;
    using namespace DecalProjectionDetail;
    ValidateLimits(limits);
    if (!Finite(location) || !Finite(rotationXAxis) || !std::isfinite(traceDistance))
        Fail("nonfinite projection request");
    DecalProjectionWork work; work.workSteps=priorWorkSteps; ValidateWork(work,limits);
    DecalPreparedPrimary result;
    auto candidateSeed=seed;
    auto directionLimits=limits.direction;
    if (optionalDirection.x==0 && optionalDirection.y==0 && optionalDirection.z==0) {
        directionLimits.attempts=std::min(directionLimits.attempts,(limits.workSteps-work.workSteps)/3u);
        if (!directionLimits.attempts) Fail("projection work budget exhausted");
    }
    result.preparedDirection=PrepareOriginalDecalDirection(optionalDirection,candidateSeed,directionLimits);
    result.randomDraws=result.preparedDirection.draws; Charge(work,limits,result.randomDraws);
    const auto end=Add(location,Scale(NormalizeRay(
        {-rotationXAxis.x,-rotationXAxis.y,-rotationXAxis.z}),traceDistance));
    const auto hit=TraceOriginalModelPoint(model,location,end,TraceLimits(work,limits));
    result.traceStats=hit.stats;
    Charge(work,limits,hit.stats.validationSteps); Charge(work,limits,hit.stats.querySteps);
    const auto finish=[&]() { result.workSteps=work.workSteps; seed=candidateSeed; return result; };
    if (hit.clear || hit.node==-1) return finish();
    result.primaryTraceNode=hit.node;
    const auto selected=SelectOriginalDecalPrimarySurface(model,hit.node,location,end,SurfaceLimits(work,limits));
    result.surfaceStats=selected.stats;
    Charge(work,limits,selected.stats.validationSteps); Charge(work,limits,selected.stats.querySteps);
    if (selected.surface==-1) return finish();
    result.selectedNode=selected.node; result.surface=selected.surface;
    result.center=selected.center; result.surfaceBase=selected.surfaceBase; result.surfaceNormal=selected.surfaceNormal;
    result.hasPrimaryGeometry=AcceptOriginalDecalPrimarySurface(rotationXAxis,result.center,result.surfaceBase,
        result.surfaceNormal,model.surfaces[static_cast<std::size_t>(result.surface)].polyFlags);
    return finish();
}

// Post-callback primary membership phase. The caller builds its quad from the
// retained prepared plane/direction and THEN-live USize/DrawScale. No insertion,
// owner grouping or SurfList mutation is hidden inside this geometry operation.
inline DecalProjectedSurface BuildOriginalDecalPrimaryMembership(const PortableModelGeometry& model,
    const std::int32_t surface,const DecalPrimaryQuad& quad,DecalProjectionWork& work,
    const DecalProjectionLimits& limits = {}) {
    using namespace DecalProjectionDetail;
    ValidateWork(work,limits); auto candidate=work;
    const auto membership=BuildOriginalDecalNodeMembership(model,surface,quad.worldCorners,SurfaceLimits(candidate,limits));
    Charge(candidate,limits,membership.stats.validationSteps); Charge(candidate,limits,membership.stats.querySteps);
    if (membership.nodes.size()>limits.membershipEntries-candidate.membershipEntries)
        DecalPrimaryDetail::Fail("projected membership budget exceeded");
    candidate.membershipEntries+=membership.nodes.size();
    DecalProjectedSurface result{surface,true,quad.worldCorners,quad.relativeCorners,membership.nodes};
    work=candidate; return result;
}

// Build ONE sample using the live post-callback raw Multi. This does not call
// ClampOriginalDecalMultiLevel: callback changes above four are meaningful.
// No M*M allocation and no stored grid freeze those later property reads.
inline DecalSecondarySample BuildOriginalDecalSecondarySample(
    const std::array<PortableModelVec3,4>& primaryCorners,const PortableModelVec3 primaryNormal,
    const std::int32_t rawMultiLevel,const std::int32_t outer,const std::int32_t inner) {
    using namespace DecalPrimaryDetail;
    if (rawMultiLevel<=0 || outer<0 || inner<0 || outer>=rawMultiLevel || inner>=rawMultiLevel)
        Fail("invalid live secondary sample index");
    for (const auto corner:primaryCorners) if (!Finite(corner)) Fail("nonfinite primary grid corner");
    if (!Finite(primaryNormal)) Fail("nonfinite primary grid normal");
    const auto offset=Scale(Normalize(primaryNormal,false),-16.0);
    const auto e0=Subtract(primaryCorners[1],primaryCorners[0]),e1=Subtract(primaryCorners[3],primaryCorners[0]);
    const auto point=Add(Add(primaryCorners[0],Scale(e0,static_cast<double>(outer+1)/rawMultiLevel)),
        Scale(e1,static_cast<double>(inner+1)/rawMultiLevel));
    return {outer,inner,point,Subtract(point,offset),Add(point,offset)};
}

// Process one bounded secondary sample against the THEN-live SurfList. The
// host owns loop tests/fraction reads, publication and Texture grouping. A miss
// never consumes SurfList values; a selected candidate validates every index
// while charging the original whole-list scan. Work updates are atomic on
// exceptions, and no RNG or script callback occurs in this phase.
inline DecalProcessedSecondarySample ProcessOriginalDecalSecondarySample(const PortableModelGeometry& model,
    const std::array<PortableModelVec3,4>& primaryCorners,const PortableModelVec3 primaryNormal,
    const std::int32_t rawMultiLevel,const std::int32_t outer,const std::int32_t inner,
    const std::vector<std::int32_t>& liveSurfList,DecalProjectionWork& work,
    const DecalProjectionLimits& limits = {}) {
    using namespace DecalPrimaryDetail;
    using namespace DecalProjectionDetail;
    ValidateWork(work,limits);
    if (liveSurfList.size()>limits.surfListEntries) Fail("existing SurfList budget exceeded");
    auto candidate=work;
    if (candidate.secondarySamples>=limits.secondarySamples) Fail("secondary sample budget exhausted");
    DecalProcessedSecondarySample result;
    result.sample=BuildOriginalDecalSecondarySample(primaryCorners,primaryNormal,rawMultiLevel,outer,inner);
    ++candidate.secondarySamples;
    const auto finish=[&](DecalSecondarySampleStatus status) { result.status=status; work=candidate; return result; };
    const auto hit=TraceOriginalModelPoint(model,result.sample.start,result.sample.end,TraceLimits(candidate,limits));
    Charge(candidate,limits,hit.stats.validationSteps); Charge(candidate,limits,hit.stats.querySteps);
    if (hit.clear || hit.node==-1) { ++candidate.secondaryMisses; return finish(DecalSecondarySampleStatus::Miss); }
    const auto selected=SelectOriginalDecalSecondarySurface(model,hit.node,result.sample.start,result.sample.end,
        SurfaceLimits(candidate,limits));
    Charge(candidate,limits,selected.stats.validationSteps); Charge(candidate,limits,selected.stats.querySteps);
    if (selected.surface==-1) { ++candidate.secondaryMisses; return finish(DecalSecondarySampleStatus::Miss); }
    Charge(candidate,limits,liveSurfList.size());
    bool duplicate{};
    for (const auto surface:liveSurfList) {
        if (surface<0 || static_cast<std::size_t>(surface)>=model.surfaces.size()) Fail("invalid existing SurfList index");
        if (surface==selected.surface) duplicate=true;
    }
    if (duplicate) { ++candidate.secondaryDuplicates; return finish(DecalSecondarySampleStatus::Duplicate); }
    if (model.surfaces[static_cast<std::size_t>(selected.surface)].polyFlags&0x600u) {
        ++candidate.secondaryFlagRejects; return finish(DecalSecondarySampleStatus::FlagRejected);
    }
    const auto angle=CheckOriginalDecalSecondaryAngle(primaryNormal,selected.surfaceNormal);
    if (!angle.accepted) { ++candidate.secondaryAngleRejects; return finish(DecalSecondarySampleStatus::AngleRejected); }
    const auto quad=BuildOriginalDecalSecondaryQuad(primaryCorners,primaryNormal,
        selected.surfaceBase,selected.surfaceNormal,angle.signedCosine);
    if (liveSurfList.size()>=limits.surfListEntries) Fail("projected SurfList budget exceeded");
    const auto membership=BuildOriginalDecalNodeMembership(model,selected.surface,quad.worldCorners,SurfaceLimits(candidate,limits));
    Charge(candidate,limits,membership.stats.validationSteps); Charge(candidate,limits,membership.stats.querySteps);
    if (membership.nodes.size()>limits.membershipEntries-candidate.membershipEntries)
        Fail("projected membership budget exceeded");
    candidate.membershipEntries+=membership.nodes.size();
    result.projected={selected.surface,false,quad.worldCorners,quad.relativeCorners,membership.nodes};
    return finish(DecalSecondarySampleStatus::Accepted);
}

inline DecalProjectionPlan ProjectOriginalDecalGeometry(const PortableModelGeometry& model,
    const PortableModelVec3 location,const PortableModelVec3 rotationXAxis,const float traceDistance,
    const PortableModelVec3 optionalDirection,const std::int32_t textureUSize,const float drawScale,
    const std::int32_t multiLevel,const std::vector<std::int32_t>& existingSurfList,std::uint32_t& seed,
    const DecalProjectionLimits& limits = {}) {
    using namespace DecalPrimaryDetail;
    using namespace DecalProjectionDetail;
    ValidateLimits(limits);
    if (!Finite(location) || !Finite(rotationXAxis) || !std::isfinite(traceDistance) || !std::isfinite(drawScale))
        Fail("nonfinite projection request");
    if (existingSurfList.size()>limits.surfListEntries) Fail("existing SurfList budget exceeded");
    DecalProjectionPlan result; result.clampedMultiLevel=ClampOriginalDecalMultiLevel(multiLevel);
    for (const auto surface:existingSurfList) {
        Charge(result,limits,1u);
        if (surface<0 || static_cast<std::size_t>(surface)>=model.surfaces.size()) Fail("invalid existing SurfList index");
    }
    // Local candidate prevents partial RNG publication on malformed/budget
    // exceptions. Ordinary no-hit returns DO publish successful random draws,
    // as the native samples the optional direction before its world trace.
    auto candidateSeed=seed;
    const auto prepared=PrepareOriginalDecalPrimary(model,location,rotationXAxis,traceDistance,
        optionalDirection,candidateSeed,limits,result.workSteps);
    result.randomDraws=prepared.randomDraws; result.workSteps=prepared.workSteps;
    result.primaryTraceNode=prepared.primaryTraceNode;
    result.primarySelectedNode=prepared.selectedNode; result.primaryCenter=prepared.center;
    const auto ordinaryFailure=[&]() { seed=candidateSeed; return result; };
    if (!prepared.hasPrimaryGeometry) return ordinaryFailure();
    const auto primary=BuildOriginalDecalPrimaryQuad(prepared.center,prepared.surfaceBase,prepared.surfaceNormal,
        textureUSize,drawScale,prepared.preparedDirection);
    std::vector<std::int32_t> surfList=existingSurfList;
    if (surfList.size()>=limits.surfListEntries) Fail("projected SurfList budget exceeded");
    result.surfaces.push_back(BuildOriginalDecalPrimaryMembership(model,prepared.surface,primary,result,limits));
    surfList.push_back(prepared.surface); result.hasPrimaryGeometry=true;
    // Preserve the fixed-input helper's existing validation even for M<=0;
    // the reentrant native host instead reads loop tests after its callback.
    const auto grid=BuildOriginalDecalSecondaryGrid(primary.worldCorners,prepared.surfaceNormal,result.clampedMultiLevel);
    for (const auto& sample:grid) {
        auto secondary=ProcessOriginalDecalSecondarySample(model,primary.worldCorners,prepared.surfaceNormal,
            result.clampedMultiLevel,sample.outer,sample.inner,surfList,result,limits);
        if (secondary.status!=DecalSecondarySampleStatus::Accepted) continue;
        surfList.push_back(secondary.projected.surface);
        result.surfaces.push_back(std::move(secondary.projected));
    }
    seed=candidateSeed;
    return result;
}

} // namespace QuestVr
