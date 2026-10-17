#pragma once

#include "quest_model_line_hit.h"

#include <optional>
#include <vector>

namespace QuestVr {

// Full caller-seeded UModel.LineCheck POINT branch, not FastLineCheck or an
// extent approximation. Captured owner coordinates remain native-provider
// values; no actor/Model identity, collision registration or engine is made.
namespace ModelPointLineDetail {
using namespace ModelOwnerCoordsDetail;
using NativeWindowClipDetail::Compare64;
using ScriptRandom::VectorDetail::SqrtStoredDouble64;

struct Result {
    bool clear{},hit{};
    PortableModelVec3 location{},normal{};
    float time{1.0f};
    std::int32_t node{-1};
    ModelPointTraceStats stats;
};
inline bool Csg(const PortableModelNode& node,std::uint32_t flags,bool sameSide) {
    if(sameSide) flags&=~0x10u;
    return node.vertexCount!=0u && (node.nodeFlags&static_cast<std::uint8_t>(flags|0x21u))==0u;
}
inline float Distance(const PortableModelVec3& point,const PortableModelNode& node,
    const OriginalModelCoords* coords) {
    const PortableModelVec3 normal{node.planeX,node.planeY,node.planeZ};
    if(!coords) {
        // Point recursion's null-owner inline sum is (Z+X)+Y, unlike FastLine.
        return Store32(Sub(Add64(Add64(Product(point.z,normal.z),Product(point.x,normal.x)),
            Product(point.y,normal.y)),Load(node.planeW)));
    }
    // Point owner path stores transformed normal Z/Y/X, but retains W-dot
    // through the endpoint subtraction: NO intermediate Float plane.W spill.
    const float z=Store32(TraversalDot(normal,coords->zAxis)),
        y=Store32(TraversalDot(normal,coords->yAxis)),x=Store32(TraversalDot(normal,coords->xAxis));
    const PortableModelVec3 transformed{x,y,z};
    const auto origin=TransformOriginalVector(coords->origin,*coords);
    const auto w=Sub(Load(node.planeW),TraversalDot(origin,transformed));
    return Store32(Sub(TraversalDot(point,transformed),w));
}
inline PortableModelVec3 Middle(const PortableModelVec3& start,const PortableModelVec3& end,float from,float to) {
    const auto fraction=ModelFastLineDetail::Divide64(Load(from),Sub(Load(to),Load(from)));
    const auto component=[&](float a,float b) {
        const float direction=Store32(Sub(Load(a),Load(b)));
        const float offset=Store32(Multiply64(Load(direction),fraction));
        return Store32(Add64(Load(a),Load(offset)));
    };
    return {component(start.x,end.x),component(start.y,end.y),component(start.z,end.z)};
}
inline void Finish(Result& result,const PortableModelVec3& start,const PortableModelVec3& end,bool owned) {
    const PortableModelVec3 delta{Store32(Sub(Load(end.x),Load(start.x))),
        Store32(Sub(Load(end.y),Load(start.y))),Store32(Sub(Load(end.z),Load(start.z)))};
    const float square=Store32(Add64(Add64(Product(delta.x,delta.x),Product(delta.y,delta.y)),Product(delta.z,delta.z)));
    if(square<=0.0f) ModelPointTraceDetail::Fail("blocked zero-length segment has no finite hit time");
    const auto dot=Add64(Add64(Multiply64(Sub(Load(result.location.x),Load(start.x)),Load(delta.x)),
        Multiply64(Sub(Load(result.location.y),Load(start.y)),Load(delta.y))),
        Multiply64(Sub(Load(result.location.z),Load(start.z)),Load(delta.z)));
    const float projected=Store32(ModelFastLineDetail::Divide64(dot,Load(square)));
    auto time=Sub(Load(projected),ModelFastLineDetail::Divide64(Load(0.5f),SqrtStoredDouble64(static_cast<double>(square))));
    if(Compare64(time,Load(0.0f))<0) time=Load(0.0f);
    else if(Compare64(time,Load(1.0f))>=0) time=Load(1.0f);
    result.time=Store32(time);
    const auto component=[&](float a,float d) {
        const float offset=Store32(Multiply64(Load(d),time));
        return Store32(Add64(Load(a),Load(offset)));
    };
    result.location={component(start.x,delta.x),component(start.y,delta.y),component(start.z,delta.z)};
    // Nonnull owner does NOT face against travel. The wrapper instead makes
    // a second ToWorld call and transforms the raw local normal after Finish.
    if(!owned && Compare64(TraversalDot({-delta.x,-delta.y,-delta.z},result.normal),Load(0.0f))<0)
        result.normal={-result.normal.x,-result.normal.y,-result.normal.z};
}
inline Result Trace(const PortableModelGeometry& model,const PortableModelVec3& start,
    const PortableModelVec3& end,std::uint32_t flags,const OriginalModelCoords* coords,
    const ModelPointTraceLimits& limits) {
    Result result;ModelPointTraceDetail::Validate(model,limits,result.stats);
    result.clear=model.rootOutside!=0;
    if(model.nodes.empty()) return result;
    Finite(start);Finite(end);
    struct Segment {std::int32_t node{},last{};PortableModelVec3 start,end;bool outside{};};
    Segment current{0,0,start,end,result.clear};std::vector<Segment> pending;
    pending.reserve(std::min<std::size_t>(model.nodes.size(),64u));
    bool beenOutside{}; // Original query-local global1058F34C, reset by wrapper.
    for(;;) {
        ModelPointTraceDetail::Step(result.stats.querySteps,limits.querySteps);
        if(current.node==-1) {
            ++result.stats.leafVisits;
            if(!current.outside && (beenOutside || (flags&0x10u)==0u)) {
                result.clear=false;result.hit=true;result.node=current.last;result.location=current.start;
                const auto& plane=model.nodes.at(static_cast<std::size_t>(result.node));
                result.normal={plane.planeX,plane.planeY,plane.planeZ};Finish(result,start,end,coords!=nullptr);return result;
            }
            if(current.outside) beenOutside=true;
            if(pending.empty()) {result.clear=true;return result;}
            current=pending.back();pending.pop_back();continue;
        }
        ++result.stats.nodeVisits;const auto& node=model.nodes.at(static_cast<std::size_t>(current.node));
        const float from=Distance(current.start,node,coords),to=Distance(current.end,node,coords);
        if(static_cast<double>(from)>-0.001 && static_cast<double>(to)>-0.001) {
            current.node=node.front;current.outside=current.outside||Csg(node,flags,true);
        }else if(static_cast<double>(from)<0.001 && static_cast<double>(to)<0.001) {
            current.node=node.back;current.outside=current.outside&&!Csg(node,flags,true);
        }else {
            const auto middle=Middle(current.start,current.end,from,to);const bool front=from>0.0f;
            const bool csg=Csg(node,flags,false);
            if(pending.size()>=limits.nodes) ModelPointTraceDetail::Fail("pending branch budget exceeded");
            pending.push_back({front?node.back:node.front,current.node,middle,current.end,
                front?current.outside&&!csg:current.outside||csg});
            current.node=front?node.front:node.back;current.end=middle;
            current.outside=front?current.outside||csg:current.outside&&!csg;
        }
    }
}
} // namespace ModelPointLineDetail

// Capture supplies actual Owner.ToWorld on demand: once for a nonempty owner
// query, again ONLY on a hit for its final normal. Empty Models never call it
// or inspect endpoints. Clear queries leave the ENTIRE caller packet intact;
// hits preserve Next, write Actor/Primitive/Item/Time/Location/Normal. Publish
// stats and packet only after all supported-domain work/captures succeed.
template<class Capture>
bool CheckOriginalModelPoint(OriginalActorLineHit& caller,const PortableModelGeometry& model,
    std::uintptr_t modelIdentity,std::uintptr_t ownerIdentity,const PortableModelVec3& end,
    const PortableModelVec3& start,std::uint32_t flags,Capture&& capture,
    const ModelPointTraceLimits& limits={},ModelPointTraceStats* stats=nullptr) {
    if(model.nodes.empty()) {
        const auto result=ModelPointLineDetail::Trace(model,start,end,flags,nullptr,limits);
        if(stats)*stats=result.stats;
        return result.clear;
    }
    if(!modelIdentity) throw std::invalid_argument("Original Model point adapter requires actual Model identity");
    std::optional<OriginalModelCoords> coords;
    if(ownerIdentity) coords=capture();
    auto result=ModelPointLineDetail::Trace(model,start,end,flags,coords?&*coords:nullptr,limits);
    auto prepared=caller;
    if(result.hit) {
        if(ownerIdentity) result.normal=TransformOriginalVector(result.normal,capture());
        prepared.actor=ownerIdentity;prepared.primitive=modelIdentity;prepared.item=result.node;
        prepared.location=result.location;prepared.normal=result.normal;prepared.time=result.time;
    }
    caller=prepared;if(stats)*stats=result.stats;return result.clear;
}

} // namespace QuestVr
