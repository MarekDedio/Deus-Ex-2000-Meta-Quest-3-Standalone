#pragma once

#include "quest_model_extent_bevel.h"

#include <vector>

namespace QuestVr {

// Retail UModel.LineCheck's nonzero-extent, null-owner path. Authored UE
// coordinates only. This is not ULevel.SingleLineCheck, actor collision,
// mover transforms, Move266, FindBase or a physics/world Tick coordinator.
struct ModelExtentTraceLimits {
    std::size_t nodes{1'000'000u}, hullWords{2'000'000u};
    std::size_t validationSteps{16'000'000u}, querySteps{16'000'000u}, pending{1'000'000u};
};
struct ModelExtentTraceStats {
    std::size_t validationSteps{},querySteps{},nodeVisits{},leafVisits{},hullVisits{},planeTests{},bevelPairs{},bevelTests{};
};
struct ModelExtentTraceResult {
    bool clear{true},hit{};
    float time{1.0f};
    PortableModelVec3 location{},normal{};
    // Diagnostic accepted hull parent only. Retail extent checks do NOT write
    // FCheckResult.Item; a native adapter must preserve its caller's Item.
    std::int32_t node{-1};
    ModelExtentTraceStats stats;
};

namespace ModelExtentTraceDetail {
using namespace ModelExtentClipDetail;
using ScriptRandom::VectorDetail::DecodeDouble;
using ScriptRandom::VectorDetail::StoreDouble;
using ScriptRandom::VectorDetail::SqrtStoredDouble64;
[[noreturn]] inline void Fail(const char* reason) {throw std::runtime_error(std::string("Original model extent trace: ")+reason);}
inline void Step(std::size_t& used,std::size_t maximum) {if(used>=maximum) Fail("work budget exhausted");++used;}
inline void Limits(const ModelExtentTraceLimits& value) {
    const ModelExtentTraceLimits maximum;
    if(!value.nodes||value.nodes>maximum.nodes||!value.hullWords||value.hullWords>maximum.hullWords||
        !value.validationSteps||value.validationSteps>maximum.validationSteps||!value.querySteps||
        value.querySteps>maximum.querySteps||!value.pending||value.pending>maximum.pending)
        Fail("callers may only tighten fixed positive limits");
}
inline void Index(std::int32_t value,std::size_t count) {
    if(value!=-1&&(value<0||static_cast<std::size_t>(value)>=count)) Fail("invalid BSP child index");
}
struct Hull {
    std::array<OriginalExtentPlane,64u> planes{};
    std::size_t count{};
    PortableModelVec3 minimum{},maximum{};
};
inline Hull ReadHull(const PortableModelGeometry& model,std::int32_t offset,std::size_t& used,std::size_t maximum) {
    if(offset<0||static_cast<std::size_t>(offset)>=model.leafHulls.size()) Fail("invalid collision-bound offset");
    Hull hull;auto cursor=static_cast<std::size_t>(offset);
    for(;;) {
        Step(used,maximum);
        if(cursor>=model.leafHulls.size()) Fail("missing literal leaf-hull terminator");
        const auto raw=model.leafHulls[cursor++];
        if(raw==-1) break;
        if(raw<0||hull.count==hull.planes.size()) Fail("invalid or over-64 retail hull plane list");
        const auto bits=static_cast<std::uint32_t>(raw),index=bits&~0x40000000u;
        if(index>=model.nodes.size()) Fail("invalid hull plane-node index");
        const auto& node=model.nodes[index];
        OriginalExtentPlane plane{node.planeX,node.planeY,node.planeZ,node.planeW};
        if(bits&0x40000000u) plane={-plane.x,-plane.y,-plane.z,-plane.w};
        hull.planes[hull.count++]=plane;
    }
    if(model.leafHulls.size()-cursor<6u) Fail("truncated leaf-hull bounds");
    const auto next=[&] {Step(used,maximum);const auto value=std::bit_cast<float>(model.leafHulls[cursor++]);Finite(value);return value;};
    hull.minimum={next(),next(),next()};hull.maximum={next(),next(),next()};
    if(hull.minimum.x>hull.maximum.x||hull.minimum.y>hull.maximum.y||hull.minimum.z>hull.maximum.z)
        Fail("reversed leaf-hull bounds");
    return hull;
}
inline void Validate(const PortableModelGeometry& model,const ModelExtentTraceLimits& limits,ModelExtentTraceStats& stats) {
    if(model.version!=68u||model.licenseeMode!=0u) Fail("unsupported serialized Model layout");
    if(model.rootOutside!=0&&model.rootOutside!=1) Fail("invalid root-outside value");
    if(model.nodes.size()>limits.nodes||model.leafHulls.size()>limits.hullWords) Fail("Model array budget exceeded");
    for(const auto& node:model.nodes) {
        Step(stats.validationSteps,limits.validationSteps);
        Finite(node.planeX);Finite(node.planeY);Finite(node.planeZ);Finite(node.planeW);
        Index(node.front,model.nodes.size());Index(node.back,model.nodes.size());
        if(node.collisionBound!=-1) static_cast<void>(ReadHull(model,node.collisionBound,stats.validationSteps,limits.validationSteps));
    }
    // Validate ALL components, including unreachable/pruned ones. Shared DAG
    // children are valid and still incur repeated bounded query visits.
    struct Frame {std::size_t node;unsigned next;};
    std::vector<unsigned char> colors(model.nodes.size());std::vector<Frame> stack;
    for(std::size_t root=0;root<model.nodes.size();++root) {
        if(colors[root]) continue;
        colors[root]=1;stack.push_back({root,0});
        while(!stack.empty()) {
            Step(stats.validationSteps,limits.validationSteps);auto& frame=stack.back();
            if(frame.next==2u) {colors[frame.node]=2;stack.pop_back();continue;}
            const auto& node=model.nodes[frame.node];const auto child=frame.next++?node.back:node.front;
            if(child==-1) continue;
            const auto index=static_cast<std::size_t>(child);
            if(colors[index]==1) Fail("cyclic BSP front/back graph");
            if(!colors[index]) {colors[index]=1;stack.push_back({index,0});}
        }
    }
}
inline PortableModelVec3 Delta(const PortableModelVec3& start,const PortableModelVec3& end) {
    return {Store32(Add64(Decode(end.x),Negate(Decode(start.x)))),
        Store32(Add64(Decode(end.y),Negate(Decode(start.y)))),Store32(Add64(Decode(end.z),Negate(Decode(start.z))))};
}
inline float Size(const PortableModelVec3& delta) {
    const auto x=Decode(delta.x),y=Decode(delta.y),z=Decode(delta.z);
    const auto square=Add64(Add64(Multiply64(x,x),Multiply64(y,y)),Multiply64(z,z));
    if(!square.significand) return 0.0f;
    return Store32(SqrtStoredDouble64(StoreDouble(square)));
}
inline Dyadic Distance(const PortableModelVec3& point,const OriginalExtentPlane& plane,bool start) {
    const auto x=Multiply64(Decode(point.x),Decode(plane.x)),y=Multiply64(Decode(point.y),Decode(plane.y)),
        z=Multiply64(Decode(point.z),Decode(plane.z));
    return Add64(start?Add64(Add64(z,y),x):Add64(Add64(x,y),z),Negate(Decode(plane.w)));
}
inline Dyadic Radius(const PortableModelVec3& extent,const OriginalExtentPlane& plane) {
    const auto padding=Decode(std::bit_cast<float>(std::uint32_t{0x3f8ccccdu}));
    const auto x=Multiply64(Decode(extent.x),padding);
    const auto y=Decode(Store32(Multiply64(Decode(extent.y),padding))),z=Decode(Store32(Multiply64(Decode(extent.z),padding)));
    return Add64(Add64(Magnitude(Multiply64(z,Decode(plane.z))),Magnitude(Multiply64(y,Decode(plane.y)))),
        Magnitude(Multiply64(x,Decode(plane.x))));
}
inline void Finalize(ModelExtentTraceResult& result,const PortableModelVec3& start,
    const PortableModelVec3& delta,float size) {
    Dyadic time{};
    if(size==0.0f) {
        // Retail .1/positive-zero => +Inf, Max backoff => +Inf, clamp=>+0.
        // Preserve the observable result without exporting masked x87 status.
        time=Decode(0.0f);
    } else {
        const auto margin=Decode(std::bit_cast<float>(std::uint32_t{0x3dcccccdu}));
        const auto quotient=ModelFastLineDetail::Divide64(margin,Decode(size));
        time=Add64(Decode(result.time),Negate(Compare64(quotient,margin)>0?quotient:margin));
        if(Compare64(time,Decode(0.0f))<0) time=Decode(0.0f);
        else if(Compare64(time,Decode(1.0f))>=0) time=Decode(1.0f);
    }
    result.time=Store32(time);const auto stored=Decode(result.time);
    const auto component=[&](float origin,float direction) {
        const auto offset=Decode(Store32(Multiply64(Decode(direction),stored)));
        return Store32(Add64(Decode(origin),offset));
    };
    result.location={component(start.x,delta.x),component(start.y,delta.y),component(start.z,delta.z)};
    result.clear=result.time==1.0f;
}
} // namespace ModelExtentTraceDetail

inline ModelExtentTraceResult TraceOriginalModelExtent(const PortableModelGeometry& model,
    const PortableModelVec3& start,const PortableModelVec3& end,const PortableModelVec3& extent,
    const ModelExtentTraceLimits& limits={}) {
    using namespace ModelExtentTraceDetail;
    Limits(limits);Finite(start);Finite(end);Finite(extent);
    if(extent.x<0||extent.y<0||extent.z<0) Fail("negative collision extents");
    if(extent.x==0&&extent.y==0&&extent.z==0) Fail("zero extents require the separate point-trace branch");
    ModelExtentTraceResult result;Validate(model,limits,result.stats);
    if(model.nodes.empty()) {result.clear=model.rootOutside!=0;return result;}
    result.time=2.0f;const auto delta=Delta(start,end);const auto size=Size(delta);
    struct Visit {std::int32_t node,parent;bool outside;};
    std::vector<Visit> pending{{0,0,model.rootOutside!=0}};
    const auto push=[&](Visit visit) {if(pending.size()>=limits.pending) Fail("pending BSP budget exceeded");pending.push_back(visit);};
    while(!pending.empty()) {
        Step(result.stats.querySteps,limits.querySteps);const auto visit=pending.back();pending.pop_back();
        if(visit.node!=-1) {
            ++result.stats.nodeVisits;const auto& node=model.nodes[static_cast<std::size_t>(visit.node)];
            const OriginalExtentPlane plane{node.planeX,node.planeY,node.planeZ,node.planeW};
            const auto from=Distance(start,plane,true),to=Distance(end,plane,false),radius=Radius(extent,plane);
            const bool back=!(Compare64(from,radius)>0&&Compare64(to,radius)>0),
                front=!(Compare64(from,Negate(radius))<0&&Compare64(to,Negate(radius))<0);
            const bool csg=node.vertexCount!=0&&(node.nodeFlags&0x21u)==0;
            const Visit backVisit{node.back,visit.node,visit.outside&&!csg},
                frontVisit{node.front,visit.node,visit.outside||csg};
            if(Compare64(from,to)<0) {if(front) push(frontVisit);if(back) push(backVisit);}
            else {if(back) push(backVisit);if(front) push(frontVisit);}
            continue;
        }
        ++result.stats.leafVisits;if(visit.outside) continue;
        const auto offset=model.nodes[static_cast<std::size_t>(visit.parent)].collisionBound;
        if(offset==-1) continue;
        ++result.stats.hullVisits;const auto hull=ReadHull(model,offset,result.stats.querySteps,limits.querySteps);
        OriginalExtentClipState interval{-1.0f,result.time,{}};
        const auto clip=[&](const OriginalExtentPlane& plane) {
            Step(result.stats.querySteps,limits.querySteps);++result.stats.planeTests;
            return ClipOriginalExtentPlane(interval,plane,start,end,extent);
        };
        bool accepted=true;
        for(std::size_t i=0;i<hull.count;++i) if(!clip(hull.planes[i])) {accepted=false;break;}
        if(!accepted) continue;
        for(const auto& plane:BuildOriginalExtentBoundsPlanes(hull.minimum,hull.maximum))
            if(!clip(plane)) {accepted=false;break;}
        if(!accepted) continue;
        for(std::size_t i=0;i<hull.count&&accepted;++i) for(std::size_t j=0;j<i&&accepted;++j) {
            Step(result.stats.querySteps,limits.querySteps);++result.stats.bevelPairs;
            for(unsigned axis=0;axis<3u;++axis) {
                Step(result.stats.querySteps,limits.querySteps);
                if(const auto plane=BuildOriginalExtentBevel(hull.planes[i],hull.planes[j],axis)) {
                    ++result.stats.bevelTests;
                    if(!clip(*plane)) {accepted=false;break;}
                }
            }
        }
        if(accepted&&interval.enter>-1.0f&&interval.enter<interval.exit&&interval.exit>0.0f) {
            result.hit=true;result.time=interval.enter;result.normal=interval.normal;result.node=visit.parent;
        }
    }
    if(result.hit) Finalize(result,start,delta,size);
    return result;
}

} // namespace QuestVr
