#pragma once

#include "quest_decal_secondary_quad.h"
#include "quest_decal_surface_select.h"
#include "quest_model_point_trace.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace QuestVr {

// Geometry-only plan for original Attach after its truthful viewport/Texture
// guards. RotationXAxis comes from the original rotator transform consumer.
// No actor, reflected property, owner graph, renderer, mover base, or save state
// is changed here. The host must atomically publish all plan records, journal
// RNG/properties/roots, and reject unsupported consumers rather than returning
// success for this query alone.
struct DecalProjectionLimits {
    ModelPointTraceLimits trace;
    DecalSurfaceSelectLimits surface;
    DecalDirectionLimits direction;
    std::size_t surfListEntries{262'144u}, membershipEntries{1'048'576u};
    std::size_t workSteps{512u * 1024u * 1024u};
};
struct DecalProjectedSurface {
    std::int32_t surface{-1};
    bool primary{};
    std::array<PortableModelVec3,4> worldCorners{},relativeCorners{};
    std::vector<std::int32_t> nodeMembership;
};
struct DecalProjectionPlan {
    bool hasPrimaryGeometry{};
    std::int32_t clampedMultiLevel{};
    std::int32_t primaryTraceNode{-1},primarySelectedNode{-1};
    PortableModelVec3 primaryCenter;
    std::vector<DecalProjectedSurface> surfaces;
    std::size_t randomDraws{},secondarySamples{},secondaryMisses{},secondaryDuplicates{},secondaryFlagRejects{},secondaryAngleRejects{};
    std::size_t workSteps{},membershipEntries{};
};

namespace DecalProjectionDetail {
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

inline DecalProjectionPlan ProjectOriginalDecalGeometry(const PortableModelGeometry& model,
    const PortableModelVec3 location,const PortableModelVec3 rotationXAxis,const float traceDistance,
    const PortableModelVec3 optionalDirection,const std::int32_t textureUSize,const float drawScale,
    const std::int32_t multiLevel,const std::vector<std::int32_t>& existingSurfList,std::uint32_t& seed,
    const DecalProjectionLimits& limits = {}) {
    using namespace DecalPrimaryDetail;
    const DecalProjectionLimits maximum;
    const auto bounded=[](std::size_t value,std::size_t bound) { return value && value<=bound; };
    if (!limits.surfListEntries || limits.surfListEntries>maximum.surfListEntries ||
        !limits.membershipEntries || limits.membershipEntries>maximum.membershipEntries ||
        !limits.workSteps || limits.workSteps>maximum.workSteps) Fail("invalid projection plan limits");
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
    if (!Finite(location) || !Finite(rotationXAxis) || !std::isfinite(traceDistance) || !std::isfinite(drawScale))
        Fail("nonfinite projection request");
    if (existingSurfList.size()>limits.surfListEntries) Fail("existing SurfList budget exceeded");
    DecalProjectionPlan result; result.clampedMultiLevel=ClampOriginalDecalMultiLevel(multiLevel);
    const auto charge=[&](std::size_t steps) {
        if (steps>limits.workSteps-result.workSteps) Fail("projection work budget exhausted");
        result.workSteps+=steps;
    };
    // Helpers have separate validation/query counters. Partition the remaining
    // aggregate allowance BEFORE entering each helper, so even an exception
    // cannot execute beyond the plan's actual work bound. Tight budgets may
    // refuse conservatively rather than reallocating unused validation quota.
    const auto traceLimits=[&]() {
        const auto remaining=limits.workSteps-result.workSteps;
        if (remaining<2u) Fail("projection work budget exhausted");
        auto boundedLimits=limits.trace;
        boundedLimits.validationSteps=std::min(boundedLimits.validationSteps,remaining/2u);
        boundedLimits.querySteps=std::min(boundedLimits.querySteps,remaining-remaining/2u);
        return boundedLimits;
    };
    const auto surfaceLimits=[&]() {
        const auto remaining=limits.workSteps-result.workSteps;
        if (remaining<2u) Fail("projection work budget exhausted");
        auto boundedLimits=limits.surface;
        boundedLimits.validationSteps=std::min(boundedLimits.validationSteps,remaining/2u);
        boundedLimits.querySteps=std::min(boundedLimits.querySteps,remaining-remaining/2u);
        return boundedLimits;
    };
    for (const auto surface:existingSurfList) {
        charge(1u);
        if (surface<0 || static_cast<std::size_t>(surface)>=model.surfaces.size()) Fail("invalid existing SurfList index");
    }
    // Local candidate prevents partial RNG publication on malformed/budget
    // exceptions. Ordinary no-hit returns DO publish successful random draws,
    // as the native samples the optional direction before its world trace.
    auto candidateSeed=seed;
    auto directionLimits=limits.direction;
    if (optionalDirection.x==0 && optionalDirection.y==0 && optionalDirection.z==0) {
        directionLimits.attempts=std::min(directionLimits.attempts,(limits.workSteps-result.workSteps)/3u);
        if (!directionLimits.attempts) Fail("projection work budget exhausted");
    }
    const auto prepared=PrepareOriginalDecalDirection(optionalDirection,candidateSeed,directionLimits);
    result.randomDraws=prepared.draws; charge(prepared.draws);
    const auto end=Add(location,Scale(DecalProjectionDetail::NormalizeRay(
        {-rotationXAxis.x,-rotationXAxis.y,-rotationXAxis.z}),traceDistance));
    const auto hit=TraceOriginalModelPoint(model,location,end,traceLimits());
    charge(hit.stats.validationSteps); charge(hit.stats.querySteps);
    const auto ordinaryFailure=[&]() { seed=candidateSeed; return result; };
    if (hit.clear || hit.node==-1) return ordinaryFailure();
    result.primaryTraceNode=hit.node;
    const auto selected=SelectOriginalDecalPrimarySurface(model,hit.node,location,end,surfaceLimits());
    charge(selected.stats.validationSteps); charge(selected.stats.querySteps);
    if (selected.surface==-1) return ordinaryFailure();
    result.primarySelectedNode=selected.node; result.primaryCenter=selected.center;
    if (!AcceptOriginalDecalPrimarySurface(rotationXAxis,selected.center,selected.surfaceBase,selected.surfaceNormal,
        model.surfaces[static_cast<std::size_t>(selected.surface)].polyFlags)) return ordinaryFailure();
    const auto primary=BuildOriginalDecalPrimaryQuad(selected.center,selected.surfaceBase,selected.surfaceNormal,
        textureUSize,drawScale,prepared);
    std::vector<std::int32_t> surfList=existingSurfList;
    const auto append=[&](std::int32_t surface,bool isPrimary,const auto& quad) {
        if (surfList.size()>=limits.surfListEntries) Fail("projected SurfList budget exceeded");
        const auto membership=BuildOriginalDecalNodeMembership(model,surface,quad.worldCorners,surfaceLimits());
        charge(membership.stats.validationSteps); charge(membership.stats.querySteps);
        if (membership.nodes.size()>limits.membershipEntries-result.membershipEntries) Fail("projected membership budget exceeded");
        result.membershipEntries+=membership.nodes.size();
        result.surfaces.push_back({surface,isPrimary,quad.worldCorners,quad.relativeCorners,membership.nodes});
        surfList.push_back(surface); // Primary duplicates retained, even across calls.
    };
    append(selected.surface,true,primary); result.hasPrimaryGeometry=true;
    const auto grid=BuildOriginalDecalSecondaryGrid(primary.worldCorners,selected.surfaceNormal,result.clampedMultiLevel);
    for (const auto& sample:grid) {
        ++result.secondarySamples;
        const auto secondaryHit=TraceOriginalModelPoint(model,sample.start,sample.end,traceLimits());
        charge(secondaryHit.stats.validationSteps); charge(secondaryHit.stats.querySteps);
        if (secondaryHit.clear || secondaryHit.node==-1) { ++result.secondaryMisses; continue; }
        const auto secondary=SelectOriginalDecalSecondarySurface(model,secondaryHit.node,sample.start,sample.end,surfaceLimits());
        charge(secondary.stats.validationSteps); charge(secondary.stats.querySteps);
        if (secondary.surface==-1) { ++result.secondaryMisses; continue; }
        charge(surfList.size());
        if (std::find(surfList.begin(),surfList.end(),secondary.surface)!=surfList.end()) { ++result.secondaryDuplicates; continue; }
        if (model.surfaces[static_cast<std::size_t>(secondary.surface)].polyFlags&0x600u) { ++result.secondaryFlagRejects; continue; }
        const auto angle=CheckOriginalDecalSecondaryAngle(selected.surfaceNormal,secondary.surfaceNormal);
        if (!angle.accepted) { ++result.secondaryAngleRejects; continue; }
        const auto quad=BuildOriginalDecalSecondaryQuad(primary.worldCorners,selected.surfaceNormal,
            secondary.surfaceBase,secondary.surfaceNormal,angle.signedCosine);
        append(secondary.surface,false,quad);
    }
    seed=candidateSeed;
    return result;
}

} // namespace QuestVr
