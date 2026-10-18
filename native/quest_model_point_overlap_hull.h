#pragma once

#include "quest_model_point_overlap_types.h"
#include "quest_model_extent_bevel.h"

#include <array>
#include <bit>

namespace QuestVr::ModelPointOverlapDetail {

// UModel.PointCheck's nonzero-extent solid-leaf service. True means this hull
// separates from the query box; false means overlap. This is NOT a swept
// LineCheck or its enter/exit clipping interval. The caller owns the packet,
// shared best-distance, traversal order, work ledger and outer transaction.
inline Dyadic HullProduct(float a,float b) {return Multiply64(Load(a),Load(b));}
inline Dyadic HullMagnitude(Dyadic value) {
    // Native FCOM/FCHS, not FABS: an input -0 stays -0.
    if(value.significand && value.negative) value.negative=false;
    return value;
}
inline Dyadic HullRadius(const PortableModelVec3& extent,const OriginalExtentPlane& plane) {
    // Engine103F6590..65DB and the inlined bevel radius103F3053..30CE.
    // Only the primary-plane caller Float-stores the returned PC64 radius.
    const auto x=HullMagnitude(HullProduct(extent.x,plane.x));
    const auto y=HullMagnitude(HullProduct(extent.y,plane.y));
    const auto z=HullMagnitude(HullProduct(extent.z,plane.z));
    return Add64(Add64(z,y),x);
}
inline Dyadic HullDistance(const PortableModelVec3& point,const OriginalExtentPlane& plane) {
    // Core.PlaneDot10118520..53D: (Z+Y)+X-W, retained on return.
    return Sub(Add64(Add64(HullProduct(point.z,plane.z),HullProduct(point.y,plane.y)),
        HullProduct(point.x,plane.x)),Load(plane.w));
}
inline void HullPacket(Context& context,const OriginalExtentPlane& plane,std::int32_t item,
    const PortableModelVec3& location) {
    context.hit.location=location;
    context.hit.normal={plane.x,plane.y,plane.z};
    context.hit.actor=context.ownerIdentity;context.hit.primitive=context.modelIdentity;
    context.hit.item=item;context.hit.time=0.0f;
    // Original Next is never written by these Model helpers.
}
inline bool CheckHullPrimary(Context& context,const OriginalExtentPlane& plane,std::int32_t item) {
    Step(context.stats.querySteps,context.limits.querySteps);++context.stats.planeTests;
    const float radius=Store32(HullRadius(context.extent,plane));
    const auto retainedDistance=HullDistance(context.point,plane);
    const float distance=Store32(retainedDistance);
    // 103F1E28 FST does not pop before the first positive test. Later
    // comparisons reload Float distance, including a positive value rounded0.
    if(Compare64(retainedDistance,Decode(0.0f))>0 && distance<context.bestDistance) {
        if(distance>=radius) return true;
        context.bestDistance=distance;
        const float amount=Store32(Sub(Load(radius),Load(distance)));
        constexpr float push=std::bit_cast<float>(std::uint32_t{0x3f828f5cu});
        // Engine103D7CB0 stores ALL normal*1.02 components, then Core's
        // vector*Float and vector+vector each store another Float3.
        const auto component=[&](float normal,float point) {
            const float scaled=Store32(HullProduct(normal,push));
            const float offset=Store32(HullProduct(scaled,amount));
            return Store32(Add64(Load(point),Load(offset)));
        };
        HullPacket(context,plane,item,{component(plane.x,context.point.x),
            component(plane.y,context.point.y),component(plane.z,context.point.z)});
    }
    // Keep earlier candidate packet writes even when this or a later plane
    // separates: native PointCheck also mutates its packet on a clear return.
    return distance>=radius;
}
inline bool CheckHullBevel(Context& context,const OriginalExtentPlane& plane) {
    Step(context.stats.querySteps,context.limits.querySteps);++context.stats.planeTests;
    // Engine103F3050..3205 retains BOTH values through every comparison.
    const auto radius=HullRadius(context.extent,plane);
    const auto distance=HullDistance(context.point,plane);
    if(Compare64(distance,Decode(0.0f))>0 && Compare64(distance,Load(context.bestDistance))<0) {
        if(Compare64(distance,radius)>=0) return true;
        context.bestDistance=Store32(distance); // FST, no reload of distance.
        const float amount=Store32(Sub(radius,distance));
        constexpr float push=std::bit_cast<float>(std::uint32_t{0x3f828f5cu});
        // X normal*1.02 stays retained; Y/Z spill first. X offset then stays
        // retained until adding query.X; Y/Z offset products spill before sum.
        const auto x=Multiply64(HullProduct(plane.x,push),Load(amount));
        const float y=Store32(HullProduct(Store32(HullProduct(plane.y,push)),amount));
        const float z=Store32(HullProduct(Store32(HullProduct(plane.z,push)),amount));
        HullPacket(context,plane,-1,{Store32(Add64(x,Load(context.point.x))),
            Store32(Add64(Load(y),Load(context.point.y))),Store32(Add64(Load(z),Load(context.point.z)))});
    }
    return Compare64(distance,radius)>=0;
}

inline bool CheckHull(Context& context,std::int32_t parent) {
    Step(context.stats.querySteps,context.limits.querySteps);
    if(parent<0 || static_cast<std::size_t>(parent)>=context.model.nodes.size())
        Fail("solid leaf has no actual parent BSP node");
    const auto offset=context.model.nodes[static_cast<std::size_t>(parent)].collisionBound;
    if(offset==-1) return true;
    if(offset<0 || static_cast<std::size_t>(offset)>=context.model.leafHulls.size() ||
        context.model.leafHulls.size()>context.limits.hullWords)
        Fail("solid leaf collision-bound words exceed their admitted source");
    ++context.stats.hullVisits;
    std::array<OriginalExtentPlane,64u> planes;
    std::array<std::int32_t,64u> items{};
    std::size_t count{},cursor=static_cast<std::size_t>(offset);
    for(;;) {
        Step(context.stats.querySteps,context.limits.querySteps);
        if(cursor>=context.model.leafHulls.size()) Fail("solid hull has no literal terminator");
        const auto raw=context.model.leafHulls[cursor++];
        if(raw==-1) break;
        if(raw<0 || count==planes.size()) Fail("solid hull exceeds its64 actual plane slots");
        const auto bits=static_cast<std::uint32_t>(raw);
        const auto index=bits&~std::uint32_t{0x40000000u};
        if(index>=context.model.nodes.size()) Fail("solid hull plane has no actual source node");
        const auto& node=context.model.nodes[index];
        OriginalExtentPlane plane{node.planeX,node.planeY,node.planeZ,node.planeW};
        if(context.coords) plane=TransformOriginalHullPlane(plane,*context.coords);
        if(bits&0x40000000u) plane={-plane.x,-plane.y,-plane.z,-plane.w};
        // Sign masks are computed for ALL stored hull normals before testing
        // the first plane in retail. The builder below uses equivalent finite
        // component signs, retaining the exact i/j/XYZ order and strict gates.
        static_cast<void>(Load(plane.x));static_cast<void>(Load(plane.y));static_cast<void>(Load(plane.z));
        planes[count]=plane;items[count]=static_cast<std::int32_t>(index);++count;
    }
    if(context.model.leafHulls.size()-cursor<6u) Fail("solid hull bounds words are truncated");
    // Native copies all six WORDS even for an Owner. Their numeric bits are
    // deliberately not inspected on that owner path (including NaNs/reversal).
    std::array<float,6u> bounds;
    for(auto& component:bounds) {
        Step(context.stats.querySteps,context.limits.querySteps);
        component=std::bit_cast<float>(context.model.leafHulls[cursor++]);
    }
    for(std::size_t index=0u;index<count;++index)
        if(CheckHullPrimary(context,planes[index],items[index])) return true;
    if(!context.ownerIdentity) {
        // Same six plane constructor arithmetic as extent LineCheck, not its
        // clipping operation: -Z,+Z,-X,+X,-Y,+Y. In particular highZ expands
        // by Double.1 while highX/Y subtract it. Positive/negative zero and the
        // Double-to-Float store boundaries are retained by the shared builder.
        const auto extra=BuildOriginalExtentBoundsPlanes({bounds[0u],bounds[1u],bounds[2u]},
            {bounds[3u],bounds[4u],bounds[5u]});
        for(const auto& plane:extra) if(CheckHullPrimary(context,plane,-1)) return true;
    }
    for(std::size_t index=0u;index<count;++index) for(std::size_t other=0u;other<index;++other) {
        Step(context.stats.querySteps,context.limits.querySteps);++context.stats.bevelPairs;
        for(unsigned axis=0u;axis<3u;++axis) {
            Step(context.stats.querySteps,context.limits.querySteps);
            // Exact shared Engine103F6600 intersection/Core normalization/
            // FPlane construction; PointCheck's consuming test is DISTINCT.
            // The same pair/XYZ source construction appears103F2772..2B5D.
            // Failed intersections proceed to UnsafeNormal and explicitly
            // refuse nonfinite active results rather than silently skipping.
            if(const auto plane=BuildOriginalExtentBevel(planes[index],planes[other],axis)) {
                ++context.stats.bevelTests;
                if(CheckHullBevel(context,*plane)) return true;
            }
        }
    }
    return false;
}
} // namespace QuestVr::ModelPointOverlapDetail
