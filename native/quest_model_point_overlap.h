#pragma once

#include "quest_model_point_overlap_types.h"
#include "quest_model_point_overlap_hull.h"

namespace QuestVr {
namespace ModelPointOverlapDetail {

inline void Limits(const ModelPointOverlapLimits& limits) {
    const ModelPointOverlapLimits maximum;
    if(!limits.nodes||limits.nodes>maximum.nodes||!limits.hullWords||limits.hullWords>maximum.hullWords||
        !limits.validationSteps||limits.validationSteps>maximum.validationSteps||!limits.querySteps||
        limits.querySteps>maximum.querySteps||!limits.pending||limits.pending>maximum.pending)
        Fail("callers may only tighten fixed positive limits");
}
inline void Validate(const PortableModelGeometry& model,const ModelPointOverlapLimits& limits,
    ModelPointOverlapStats& stats,bool box,bool owned) {
    if(model.version!=68u||model.licenseeMode!=0u) Fail("unsupported serialized Model layout");
    if(model.rootOutside!=0&&model.rootOutside!=1) Fail("invalid root-outside value");
    if(model.nodes.size()>limits.nodes||model.leafHulls.size()>limits.hullWords) Fail("Model array budget exceeded");
    if(box) {
        ModelExtentTraceLimits admitted;
        admitted.nodes=limits.nodes;admitted.hullWords=limits.hullWords;
        admitted.validationSteps=limits.validationSteps;
        ModelExtentTraceStats checked;
        // Structural/finite-source admission only, NOT an extent sweep. Owned
        // hull bound words are structurally copied but numerically inactive.
        ModelExtentTraceDetail::Validate(model,admitted,checked,owned);
        stats.validationSteps=checked.validationSteps;return;
    }
    // Zero occupancy never reads collisionBound or leaf hulls. Validate all
    // node graph components, including pruned ones, without inventing an
    // inactive hull/packed-bound numerical dependency.
    for(const auto& node:model.nodes) {
        Step(stats.validationSteps,limits.validationSteps);
        static_cast<void>(Load(node.planeX));static_cast<void>(Load(node.planeY));
        static_cast<void>(Load(node.planeZ));static_cast<void>(Load(node.planeW));
        for(const auto child:{node.front,node.back})
            if(child!=-1&&(child<0||static_cast<std::size_t>(child)>=model.nodes.size())) Fail("invalid BSP child index");
    }
    struct Frame {std::size_t node;unsigned next;};
    std::vector<unsigned char> colors(model.nodes.size());std::vector<Frame> stack;
    for(std::size_t root=0u;root<model.nodes.size();++root) {
        if(colors[root])continue;
        colors[root]=1u;stack.push_back({root,0u});
        while(!stack.empty()) {
            Step(stats.validationSteps,limits.validationSteps);auto& frame=stack.back();
            if(frame.next==2u) {colors[frame.node]=2u;stack.pop_back();continue;}
            const auto& node=model.nodes[frame.node];const auto child=frame.next++?node.back:node.front;
            if(child==-1)continue;
            const auto index=static_cast<std::size_t>(child);
            if(colors[index]==1u)Fail("cyclic BSP front/back graph");
            if(!colors[index]) {colors[index]=1u;stack.push_back({index,0u});}
        }
    }
}
inline bool ZeroOrUnordered(float value) {return value==0.0f||std::isnan(value);}
inline Dyadic Product(float a,float b) {return Multiply64(Load(a),Load(b));}
inline Dyadic ZeroNormalDot(const PortableModelVec3& normal,const PortableModelVec3& axis) {
    // ZERO wrapper's INLINED (Y+X)+Z, not the imported Core dot used by BOX.
    return Add64(Add64(Product(axis.y,normal.y),Product(axis.x,normal.x)),Product(axis.z,normal.z));
}
inline Dyadic ZeroDistance(const PortableModelVec3& point,const PortableModelNode& node,
    const OriginalModelCoords& coords) {
    const PortableModelVec3 normal{node.planeX,node.planeY,node.planeZ};
    const float z=Store32(ZeroNormalDot(normal,coords.zAxis)),
        y=Store32(ZeroNormalDot(normal,coords.yAxis)),x=Store32(ZeroNormalDot(normal,coords.xAxis));
    const PortableModelVec3 transformed{x,y,z};
    const auto origin=TransformOriginalVector(coords.origin,coords);
    const auto dot=ModelOwnerCoordsDetail::TraversalDot(origin,transformed);
    // The original FST Float dot is dead scratch: retained dot feeds W-dot,
    // and retained W feeds the final query comparison. Neither is Float-rounded.
    const auto w=Sub(Load(node.planeW),dot);
    return Sub(ModelOwnerCoordsDetail::TraversalDot(point,transformed),w);
}
inline bool Zero(Context& context,const OriginalModelCoords& coords) {
    std::int32_t current=0;bool outside=context.model.rootOutside!=0;
    for(;;) {
        Step(context.stats.querySteps,context.limits.querySteps);++context.stats.nodeVisits;
        const auto& node=context.model.nodes.at(static_cast<std::size_t>(current));
        const bool front=Compare64(ZeroDistance(context.point,node,coords),Decode(0.0f))>0;
        const bool csg=node.vertexCount!=0u&&(node.nodeFlags&0x21u)==0u;
        outside=front?(outside||csg):(outside&&!csg);
        const auto next=front?node.front:node.back;
        if(next==-1) {
            ++context.stats.leafVisits;context.hit.item=current*2+(front?1:0);return outside;
        }
        current=next;
    }
}
inline float BoxDistance(const PortableModelVec3& point,const OriginalExtentPlane& plane) {
    return Store32(Sub(Add64(Add64(Product(plane.y,point.y),Product(plane.x,point.x)),
        Product(plane.z,point.z)),Load(plane.w)));
}
inline float BoxRadius(const PortableModelVec3& extent,const OriginalExtentPlane& plane) {
    const auto padding=Decode(std::bit_cast<float>(std::uint32_t{0x3f8ccccdu}));
    // All three inflated components are Float-stored in the PointCheck body.
    const PortableModelVec3 inflated{Store32(Multiply64(Load(extent.x),padding)),
        Store32(Multiply64(Load(extent.y),padding)),Store32(Multiply64(Load(extent.z),padding))};
    using ModelExtentClipDetail::Magnitude;
    return Store32(Add64(Add64(Magnitude(Product(inflated.z,plane.z)),Magnitude(Product(inflated.y,plane.y))),
        Magnitude(Product(inflated.x,plane.x))));
}
inline bool Box(Context& context,std::uint32_t flags) {
    struct Visit {std::int32_t node,parent;bool outside;};
    Visit current{0,0,context.model.rootOutside!=0};std::vector<Visit> pending;
    pending.reserve(std::min<std::size_t>({context.model.nodes.size(),context.limits.pending,64u}));
    bool clear=true;
    for(;;) {
        Step(context.stats.querySteps,context.limits.querySteps);
        if(current.node==-1) {
            ++context.stats.leafVisits;
            if(!current.outside&&!CheckHull(context,current.parent))clear=false;
            if(pending.empty())return clear;
            current=pending.back();pending.pop_back();continue;
        }
        ++context.stats.nodeVisits;const auto& node=context.model.nodes.at(static_cast<std::size_t>(current.node));
        OriginalExtentPlane plane{node.planeX,node.planeY,node.planeZ,node.planeW};
        if(context.coords)plane=TransformOriginalTraversalPlane(plane,*context.coords);
        const float radius=BoxRadius(context.extent,plane),distance=BoxDistance(context.point,plane);
        const bool csg=node.vertexCount!=0u&&(node.nodeFlags&static_cast<std::uint8_t>(flags|0x21u))==0u;
        const bool front=distance>-radius,back=distance<=radius;
        if(front) {
            if(back) {
                if(pending.size()>=context.limits.pending)Fail("pending BSP branch budget exceeded");
                pending.push_back({node.back,current.node,current.outside&&!csg});
            }
            current={node.front,current.node,current.outside||csg};
        }else if(back)current={node.back,current.node,current.outside&&!csg};
        else Fail("finite projected BSP range has neither child");
        // No early false return: later leaves still mutate shared best/hit.
    }
}
} // namespace ModelPointOverlapDetail

// Complete caller-seeded Model occupancy service. It is neither LineCheck nor
// an original native-level/hash identity provider. Packets/stats publish only
// after supported-domain success, including native clear returns with writes.
template<class Capture>
bool CheckOriginalModelPointOverlap(OriginalActorLineHit& caller,const PortableModelGeometry& model,
    std::uintptr_t modelIdentity,std::uintptr_t ownerIdentity,const PortableModelVec3& location,
    const PortableModelVec3& extent,std::uint32_t flags,Capture&& capture,
    const ModelPointOverlapLimits& limits={},ModelPointOverlapStats* stats=nullptr) {
    using namespace ModelPointOverlapDetail;
    Limits(limits);if(!modelIdentity)Fail("actual Model identity is null");
    const bool box=!model.nodes.empty()&&!(ZeroOrUnordered(extent.x)&&ZeroOrUnordered(extent.y)&&ZeroOrUnordered(extent.z));
    ModelPointOverlapStats completed;Validate(model,limits,completed,box,ownerIdentity!=0u);
    auto prepared=caller;prepared.actor=ownerIdentity;prepared.primitive=modelIdentity;
    prepared.location=location;prepared.normal={};prepared.time=0.0f;
    bool clear=model.rootOutside!=0;
    if(!model.nodes.empty()) {
        std::optional<OriginalModelCoords> coords;
        if(ownerIdentity)coords=capture();
        Context context{model,modelIdentity,ownerIdentity,location,extent,coords?&*coords:nullptr,prepared,100000.0f,limits,completed};
        if(box) {
            static_cast<void>(Load(extent.x));static_cast<void>(Load(extent.y));static_cast<void>(Load(extent.z));
            clear=Box(context,flags);
        }else {
            const OriginalModelCoords identity{{},{1,0,0},{0,1,0},{0,0,1}};
            clear=Zero(context,coords?*coords:identity);
        }
    }
    caller=prepared;if(stats)*stats=completed;return clear;
}
} // namespace QuestVr
