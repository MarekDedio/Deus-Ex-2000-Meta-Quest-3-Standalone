#pragma once

#include "quest_brush_poly_filter.h"
#include "quest_brush_tracker_pools.h"
#include <functional>

namespace QuestVr {
// A typed destination, never a pointer into a reallocatable node vector.
// tail==-1 selects the actual mover Brush.Model root; otherwise node+30.
struct OriginalBrushRootDestination {std::int32_t tail{-1};};
struct OriginalBrushInsertionQueries {
    OriginalBrushPoolQueries pool;
    std::function<PortableModelNode(std::int32_t)> readNode;
    std::function<void(std::int32_t)> writeBrushRoot;
    std::function<void(std::int32_t)> attached;
};
namespace BrushBspInsertDetail {
[[noreturn]] inline void Fail(const char* why) {
    throw std::runtime_error(std::string("Original brush BSP insertion: ")+why);
}
inline bool SameOrientation(const PortableModelNode& parent,PortableModelVec3 normal) {
    // Same retained (Z+Y)+X compare used by1037BCD4..BCF3.
    return BrushPolyFilterDetail::SameOrientation(normal,parent);
}
inline void Zone(std::int32_t zone) {
    if(zone<0||zone>=64)Fail("active zone outside original64-slot domain");
}
} // namespace BrushBspInsertDetail

template<class Owner> inline void InsertOriginalBrushPolygon(PortableModelGeometry& model,
    OriginalBrushTrackerPools<Owner>& pool,Owner* owner,OriginalBrushRootDestination& destination,
    std::int32_t surface,std::int32_t nodeIndex,std::int32_t coplanar,std::int32_t place,
    const OriginalBrushClipPolygon& poly,const OriginalBrushInsertionQueries& q) {
    using namespace BrushBspInsertDetail;
    if(!q.readNode||!q.writeBrushRoot||!q.attached)Fail("requires current node/root/mutation callbacks");
    BrushTrackerPoolDetail::Callbacks(q.pool);BrushPolySplitDetail::Count(poly.vertexCount);
    if(!owner||surface<0||static_cast<std::size_t>(surface)>=model.surfaces.size()||(place!=0&&place!=1))
        Fail("missing original owner/surface/Back-or-Front place");
    const auto originalPlace=place;auto parentIndex=nodeIndex;const bool different=coplanar!=-1&&nodeIndex!=coplanar;
    const auto original=q.readNode(nodeIndex);
    if(coplanar!=-1) {
        place=2;parentIndex=coplanar;
        for(;;) {q.pool.work();const auto node=q.readNode(parentIndex);if(node.plane==-1)break;parentIndex=node.plane;}
    }
    const auto parent=q.readNode(parentIndex);
    const auto allocated=AllocateOriginalBrushNode(model,pool,owner,parentIndex,q.pool);
    if(allocated==-1)return; // No geometry/attachment on this original branch.
    auto& node=model.nodes[static_cast<std::size_t>(allocated)];
    node.surface=surface;node.collisionBound=-1;node.renderBound=-1;node.zoneMask=parent.zoneMask;
    node.vertexCount=static_cast<std::uint8_t>(poly.vertexCount);node.front=node.back=node.plane=-1;
    node.planeX=poly.normal.x;node.planeY=poly.normal.y;node.planeZ=poly.normal.z;
    using namespace BrushPolySplitDetail;
    const auto yz=Add64(Multiply64(Load(poly.base.y),Load(poly.normal.y)),Multiply64(Load(poly.base.z),Load(poly.normal.z)));
    node.planeW=Store32(Add64(yz,Multiply64(Load(poly.base.x),Load(poly.normal.x))));
    node.nodeFlags=(poly.polyFlags&8u)?0x21u:0x20u;
    if(poly.polyFlags&0x04000001u)node.nodeFlags|=4u;
    if(poly.polyFlags&2u)node.nodeFlags|=2u;
    if(coplanar!=-1&&!different) {
        const bool same=SameOrientation(parent,poly.normal);
        node.zone0=same?parent.zone0:parent.zone1;node.zone1=same?parent.zone1:parent.zone0;
        node.leaf0=same?parent.leaf0:parent.leaf1;node.leaf1=same?parent.leaf1:parent.leaf0;
    }else {
        node.zone0=node.zone1=originalPlace?original.zone1:original.zone0;
        node.leaf0=node.leaf1=originalPlace?original.leaf1:original.leaf0;
    }
    Zone(node.zone0);Zone(node.zone1);
    // New mask already copied from chosen T; ORIGINAL N receives this OR.
    model.nodes[static_cast<std::size_t>(nodeIndex)].zoneMask|=(std::uint64_t{1}<<node.zone0)|(std::uint64_t{1}<<node.zone1);
    node.vertexPool=AllocateOriginalBrushVertices(model,pool,owner,poly.vertexCount,q.pool);
    if(node.vertexPool==-1) {
        pool.nodes[static_cast<std::size_t>(allocated)-pool.original.nodes]={};
        return; // Retain earlier unlinked node fields and original mask writes.
    }
    for(std::int32_t i=0;i<poly.vertexCount;++i) {
        q.pool.work();const auto point=AllocateOriginalBrushPoint(model,pool,owner,q.pool);
        // The valid helper never manufactures -1. Its budget/corrupt-input
        // refusals unwind the owning root, rather than select a native failure
        // provider whose negative result has not occurred on this path.
        const auto vertex=static_cast<std::size_t>(node.vertexPool+i);
        if(vertex>=model.vertices.size())Fail("allocated vertex escaped actual world array");
        model.vertices[vertex]={point,-1};
        // AllocatePoint can grow Points, so reacquire storage by index.
        model.points[static_cast<std::size_t>(point)]=poly.vertices[static_cast<std::size_t>(i)];
    }
    auto& actualParent=model.nodes[static_cast<std::size_t>(parentIndex)];
    if(place==0)actualParent.back=allocated;else if(place==1)actualParent.front=allocated;else actualParent.plane=allocated;
    if(destination.tail==-1)q.writeBrushRoot(allocated);
    else {
        const auto prior=q.readNode(destination.tail);
        static_cast<void>(prior);
        const auto offset=static_cast<std::size_t>(destination.tail)-pool.original.nodes;
        if(static_cast<std::size_t>(destination.tail)<pool.original.nodes||offset>=pool.nodes.size()||pool.nodes[offset].owner!=owner)
            Fail("mover root destination lost its actual owner");
        model.nodes[static_cast<std::size_t>(destination.tail)].renderBound=allocated;
    }
    destination.tail=allocated;q.attached(allocated);
}
} // namespace QuestVr
