#pragma once

#include "quest_brush_poly_split.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

namespace QuestVr {
// Each read is authenticated by the caller against the CURRENT owning Model
// and tracker. Free pool records are not serialized source nodes/proof. The
// callbacks share one outer work/byte ledger and journal all insertion writes.
struct OriginalBrushPolyFilterQueries {
    std::function<PortableModelNode(std::int32_t)> readNode;
    std::function<void(std::int32_t,std::int32_t,std::int32_t,std::int32_t,
        const OriginalBrushClipPolygon&)> addPoly;
    std::function<void()> work;
    // Original stack allocations survive recursive returns until the mover's
    // marker is popped. The caller retains these charges for that same lifetime.
    // This includes conservative portable recursive-frame/value scratch too.
    std::function<void(std::size_t)> scratch;
};
namespace BrushPolyFilterDetail {
[[noreturn]] inline void Fail(const char* why) {
    throw std::runtime_error(std::string("Original brush polygon filter: ")+why);
}
inline void Index(std::int32_t index,std::size_t count) {
    if(index<0||static_cast<std::size_t>(index)>=count)Fail("selected node outside actual Model");
}
inline bool Csg(const PortableModelNode& node) {
    return node.vertexCount>0u&&(node.nodeFlags&0x21u)==0u;
}
inline bool SameOrientation(PortableModelVec3 normal,const PortableModelNode& node) {
    using namespace BrushPolySplitDetail;
    const auto zy=Add64(Multiply64(Load(normal.z),Load(node.planeZ)),
        Multiply64(Load(normal.y),Load(node.planeY)));
    return Compare64(Add64(zy,Multiply64(Load(node.planeX),Load(normal.x))),Decode(0.0f))>=0;
}
inline void Walk(std::int32_t surface,std::int32_t index,std::int32_t coplanar,
    OriginalBrushClipPolygon poly,bool outside,PortableModelVec3 normal,std::size_t nodeCount,
    const OriginalBrushPolyFilterQueries& q,std::size_t depth,std::size_t maximumDepth) {
    // Two original FPoly stack slots plus portable value/recursive-frame peak.
    // No independent allocation/work budget is created inside this helper.
    q.scratch(4096u+2u*0x110u);
    const auto recurse=[&](std::int32_t child,const OriginalBrushClipPolygon& fragment,bool nextOutside) {
        if(depth==maximumDepth)Fail("recursive depth budget exhausted");
        Walk(surface,child,coplanar,fragment,nextOutside,normal,nodeCount,q,depth+1u,maximumDepth);
    };
    const auto insert=[&](std::int32_t place,const OriginalBrushClipPolygon& fragment) {
        q.work();q.addPoly(surface,index,coplanar,place,fragment);
    };
    for(;;) {
        q.work();Index(index,nodeCount);auto node=q.readNode(index);
        BrushPolySplitDetail::Count(poly.vertexCount);
        if(poly.vertexCount>=14) {
            q.scratch(0x110u);
            auto halves=HalveOriginalBrushClipPolygon(poly);
            poly=std::move(halves.first);
            // Output FIRST, then mutated original at the same live node.
            recurse(index,halves.second,outside);
            node=q.readNode(index);
        }
        std::int32_t side{};
        OriginalBrushPlaneSplit split;
        if(node.nodeFlags&0x40u)side=1;
        else if(node.nodeFlags&0x80u)side=2;
        else {
            split=SplitOriginalBrushClipPolygon(poly,{node.planeX,node.planeY,node.planeZ,node.planeW});
            side=split.side;
        }
        if(side==0) {
            if(SameOrientation(normal,node))coplanar=index;
            side=1; // Both coplanar orientations traverse FRONT.
        }
        if(side==3) {
            if(!split.front||!split.back)Fail("selected split omitted its source-defined fragments");
            const bool frontOutside=outside||Csg(node);
            q.work();const auto front=node.front;
            if(front!=-1) {Index(front,nodeCount);recurse(front,*split.front,frontOutside);}
            else if(frontOutside)insert(1,*split.front);
            // Front recursion/insertion may mutate the actual parent's flags
            // and Back edge. Reacquire BEFORE applying the BACK CSG predicate.
            poly=std::move(*split.back);node=q.readNode(index);
            q.scratch(0x110u); // Replacement back output cannot alias input.
            side=2;
        }
        if(side==1) {
            outside=outside||Csg(node);q.work();const auto child=node.front;
            if(child==-1) {if(outside)insert(1,poly);return;}
            Index(child,nodeCount);index=child;
        }else if(side==2) {
            outside=outside&&!Csg(node);q.work();const auto child=node.back;
            if(child==-1) {if(outside)insert(0,poly);return;}
            Index(child,nodeCount);index=child;
        }else return; // Original unknown splitter status has no insertion.
    }
}
} // namespace BrushPolyFilterDetail

inline void FilterOriginalBrushPolygon(std::int32_t surface,std::int32_t node,std::int32_t coplanar,
    const OriginalBrushClipPolygon& polygon,bool outside,PortableModelVec3 trackerNormal,std::size_t nodeCount,
    const OriginalBrushPolyFilterQueries& queries,std::size_t maximumDepth=256u) {
    if(!queries.readNode||!queries.addPoly||!queries.work||!queries.scratch)
        BrushPolyFilterDetail::Fail("requires live read/insertion/shared work/scratch callbacks");
    if(!maximumDepth||maximumDepth>256u)BrushPolyFilterDetail::Fail("recursive depth must be within1..256");
    // Unlike the original Update caller, FilterPoly itself has no empty-Model
    // shortcut. An invalid selected node must never turn into a successful no-op.
    BrushPolyFilterDetail::Index(node,nodeCount);
    if(coplanar!=-1)BrushPolyFilterDetail::Index(coplanar,nodeCount);
    BrushPolyFilterDetail::Walk(surface,node,coplanar,polygon,outside,trackerNormal,nodeCount,queries,1u,maximumDepth);
}
} // namespace QuestVr
