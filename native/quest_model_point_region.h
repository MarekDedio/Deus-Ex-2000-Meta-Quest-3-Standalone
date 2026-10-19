#pragma once

#include "quest_model_fast_line_check.h"

#include <cmath>
#include <functional>
#include <stdexcept>

namespace QuestVr {

// Detached UModel.PointRegion result. A zero actor reference means the
// caller-provided nonnull fallback, NOT an absent region. No UObject pointers,
// reflected Region writes, zone callbacks, or movement are performed here.
struct OriginalModelPointRegion {
    std::int32_t zoneActorReference{};
    std::int32_t leaf{-1};
    std::uint8_t zoneNumber{};
    std::size_t nodeVisits{};
};

namespace ModelPointRegionDetail {
inline ActorTimerDetail::Dyadic Load(float value) {
    if(!std::isfinite(value))
        throw std::invalid_argument("Original PointRegion requires finite active Float data");
    return ActorTimerDetail::Decode(value);
}
inline bool Front(const PortableModelVec3& point,const PortableModelNode& node) {
    // 103f5e23..103f5e4b: PC64 Z/Y/X products and sums, then subtract W.
    // There is NO Float or Double spill before FCOMP Double zero. This is
    // not FastLineCheck's stored-distance sign-bit test: either signed zero
    // is front here. The portable finite domain explicitly refuses NaNs.
    const auto z=ModelFastLineDetail::Multiply64(Load(point.z),Load(node.planeZ));
    const auto y=ModelFastLineDetail::Multiply64(Load(point.y),Load(node.planeY));
    const auto zy=NativeWindowClipDetail::Add64(z,y);
    const auto x=ModelFastLineDetail::Multiply64(Load(point.x),Load(node.planeX));
    const auto xyz=NativeWindowClipDetail::Add64(zy,x);
    const auto distance=NativeWindowClipDetail::Add64(xyz,NativeWindowClipDetail::Negate(Load(node.planeW)));
    return NativeWindowClipDetail::Compare64(distance,ActorTimerDetail::Decode(0.0f))>=0;
}
} // namespace ModelPointRegionDetail

// Original body 103f5db0..103f5f38, with explicit portable active-path bounds.
// The native outside/CSG bookkeeping does not affect any returned field.
// Leaves are returned as raw signed words; the native query never reads a
// Leaves array. Only -1 terminates a child path. The work hook joins the
// caller's shared root ledger; it is never reset by this query.
inline OriginalModelPointRegion QueryOriginalModelPointRegion(const PortableModelGeometry& model,
    const PortableModelVec3& point,std::size_t maximumVisits=65'536u,
    const std::function<void()>& work={}) {
    OriginalModelPointRegion result;
    // Empty BSP does not read the point, rootOutside, planes or zone table.
    if(model.nodes.empty())return result;
    std::int32_t current{};
    for(;;) {
        if(result.nodeVisits>=maximumVisits)
            throw std::runtime_error("Original PointRegion active-path budget exceeded");
        if(work)work();
        ++result.nodeVisits;
        if(current<0||static_cast<std::size_t>(current)>=model.nodes.size())
            throw std::runtime_error("Original PointRegion active node index is invalid");
        const auto& node=model.nodes[static_cast<std::size_t>(current)];
        const bool front=ModelPointRegionDetail::Front(point,node);
        const auto child=front?node.front:node.back;
        if(child!=-1) {
            if(child<0)throw std::runtime_error("Original PointRegion has an invalid negative child");
            current=child;continue;
        }
        result.leaf=front?node.leaf1:node.leaf0;
        if(model.zones.size()>64u)
            throw std::runtime_error("Original PointRegion zone table exceeds64 slots");
        const auto zone=model.zones.empty()?0:(front?node.zone1:node.zone0);
        if(zone<0||zone>=64)
            throw std::runtime_error("Original PointRegion selected zone is outside64 slots");
        result.zoneNumber=static_cast<std::uint8_t>(zone);
        if(static_cast<std::size_t>(zone)<model.zones.size())
            result.zoneActorReference=model.zones[static_cast<std::size_t>(zone)].actorReference;
        return result;
    }
}
} // namespace QuestVr
