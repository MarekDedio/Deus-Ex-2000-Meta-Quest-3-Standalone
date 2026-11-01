#pragma once

#include "quest_brush_tracker_pools.h"

namespace QuestVr {
// These are mutation helpers, NOT an active-query capability or an independent
// transaction. The actual runtime owner supplies authentication, rooted actor
// writes, shared work/scratch admission and the outer rollback journal.
template<class Owner> struct OriginalBrushDependencyQueries {
    std::function<PortableModelNode(std::int32_t)> readOwnedNode;
    std::function<void(Owner*)> enqueueIfUnassimilated;
    std::function<void()> work;
    std::function<void(std::size_t)> enterFrame;
    // Must be nonthrowing/noalloc. Only simultaneous recursive frame scratch
    // is released here; pending entries remain owned by the complete Update.
    std::function<void(std::size_t)> leaveFrame;
};

template<class Owner> struct OriginalBrushFlushQueries {
    OriginalBrushPoolQueries pool;
    std::function<PortableModelNode(std::int32_t)> readOwnedNode;
    std::function<std::int32_t(Owner*)> readBrushRoot;
    std::function<void(Owner*,std::int32_t)> writeBrushRoot;
    // Parent can be original, currently active, or released in THIS mutation
    // pass. The host must not turn this into normal free-record query access.
    std::function<void(std::int32_t,std::int32_t)> admitRecordedParent;
    std::function<void(std::int32_t)> recordReleasedNode;
    std::function<void(std::int32_t)> admitReleasedNode;
};

namespace BrushUpdateDetail {
[[noreturn]] inline void Fail(const char* why) {
    throw std::runtime_error(std::string("Original brush update: ")+why);
}
template<class Owner> inline std::size_t DynamicIndex(const PortableModelGeometry& model,
    const OriginalBrushTrackerPools<Owner>& pool,std::int32_t index) {
    if(index<0||static_cast<std::size_t>(index)<pool.original.nodes||
        static_cast<std::size_t>(index)>=model.nodes.size()||
        static_cast<std::size_t>(index)-pool.original.nodes>=pool.nodes.size())
        Fail("selected node is outside the actual dynamic pool");
    return static_cast<std::size_t>(index)-pool.original.nodes;
}
inline std::size_t NodeIndex(const PortableModelGeometry& model,std::int32_t index) {
    if(index<0||static_cast<std::size_t>(index)>=model.nodes.size())Fail("node index outside actual Model");
    return static_cast<std::size_t>(index);
}
template<class Owner> inline std::size_t VertexRun(const PortableModelGeometry& model,
    const OriginalBrushTrackerPools<Owner>& pool,std::int32_t first,std::size_t count) {
    if(first<0||static_cast<std::size_t>(first)<pool.original.vertices||
        static_cast<std::size_t>(first)>model.vertices.size()||
        count>model.vertices.size()-static_cast<std::size_t>(first)||
        static_cast<std::size_t>(first)-pool.original.vertices>pool.vertices.size()||
        count>pool.vertices.size()-(static_cast<std::size_t>(first)-pool.original.vertices))
        Fail("vertex run outside actual dynamic pool");
    return static_cast<std::size_t>(first);
}
template<class Owner> inline void Dependency(PortableModelGeometry& model,
    OriginalBrushTrackerPools<Owner>& pool,std::int32_t index,
    const OriginalBrushDependencyQueries<Owner>& q,std::size_t depth,std::size_t maximumDepth) {
    if(depth>=maximumDepth)Fail("dependency recursion depth exceeded");
    constexpr std::size_t frameBytes=256u;
    q.enterFrame(frameBytes);
    struct Frame {
        const std::function<void(std::size_t)>& leave;
        std::size_t bytes;
        ~Frame()noexcept {leave(bytes);}
    } frame{q.leaveFrame,frameBytes};
    q.work();const auto offset=DynamicIndex(model,pool,index);
    const auto node=q.readOwnedNode(index);
    if(node.nodeFlags&0x20u)return;
    model.nodes[NodeIndex(model,index)].nodeFlags|=0x20u;
    // Reacquire each field AFTER the previous recursive return. Front, Back,
    // Plane order and postorder owner enqueue are the original operations.
    auto child=q.readOwnedNode(index).front;
    if(child!=-1)Dependency(model,pool,child,q,depth+1u,maximumDepth);
    child=q.readOwnedNode(index).back;
    if(child!=-1)Dependency(model,pool,child,q,depth+1u,maximumDepth);
    child=q.readOwnedNode(index).plane;
    if(child!=-1)Dependency(model,pool,child,q,depth+1u,maximumDepth);
    auto* owner=pool.nodes.at(offset).owner;
    if(owner)q.enqueueIfUnassimilated(owner);
}
} // namespace BrushUpdateDetail

template<class Owner> inline void PropagateOriginalBrushDependency(PortableModelGeometry& model,
    OriginalBrushTrackerPools<Owner>& pool,std::int32_t node,
    const OriginalBrushDependencyQueries<Owner>& q,std::size_t maximumDepth=256u) {
    if(!q.readOwnedNode||!q.enqueueIfUnassimilated||!q.work||!q.enterFrame||!q.leaveFrame)
        BrushUpdateDetail::Fail("dependency requires caller ownership/work/frame callbacks");
    if(!maximumDepth||maximumDepth>256u)BrushUpdateDetail::Fail("invalid dependency depth bound");
    BrushTrackerPoolDetail::NodeShape(model,pool);
    BrushUpdateDetail::Dependency(model,pool,node,q,0u,maximumDepth);
}

template<class Owner> inline void FlushOriginalBrushMover(PortableModelGeometry& model,
    OriginalBrushTrackerPools<Owner>& pool,Owner* mover,const OriginalBrushFlushQueries<Owner>& q) {
    using namespace BrushUpdateDetail;
    BrushTrackerPoolDetail::Callbacks(q.pool);
    if(!mover||!q.readOwnedNode||!q.readBrushRoot||!q.writeBrushRoot||!q.admitRecordedParent||
        !q.recordReleasedNode||!q.admitReleasedNode)
        Fail("flush requires actual owner/root/mutation callbacks");
    auto current=q.readBrushRoot(mover);
    // Exact root==-1 short path does not inspect Model/pool records.
    if(current==-1){q.pool.work();q.writeBrushRoot(mover,-1);return;}
    BrushTrackerPoolDetail::NodeShape(model,pool);BrushTrackerPoolDetail::VertexShape(model,pool);
    while(current!=-1) {
        q.pool.work();const auto offset=DynamicIndex(model,pool,current);
        static_cast<void>(q.readOwnedNode(current));
        if(pool.nodes.at(offset).owner!=mover)Fail("flush selected a foreign/free mover node");
        const auto parent=pool.nodes.at(offset).parent;
        static_cast<void>(NodeIndex(model,parent));q.admitRecordedParent(parent,current);
        q.pool.work();
        auto& selectedParent=model.nodes[NodeIndex(model,parent)];
        if(selectedParent.front==current)selectedParent.front=-1;
        else if(selectedParent.back==current)selectedParent.back=-1;
        else if(selectedParent.plane==current)selectedParent.plane=-1;
        // Strict live-node validation must NOT run after individual point
        // frees: the selected node is deliberately partially released now.
        const auto first=VertexRun(model,pool,model.nodes[NodeIndex(model,current)].vertexPool,
            model.nodes[NodeIndex(model,current)].vertexCount);
        const auto count=model.nodes[NodeIndex(model,current)].vertexCount;
        for(std::size_t vertex=0;vertex<count;++vertex) {
            const auto actual=first+vertex;
            if(pool.vertices.at(actual-pool.original.vertices)!=mover)Fail("flush selected a foreign vertex owner");
            FreeOriginalBrushPoint(model,pool,model.vertices.at(actual).point,mover,q.pool);
        }
        for(std::size_t vertex=0;vertex<count;++vertex) {
            q.pool.work();auto& owner=pool.vertices.at(first+vertex-pool.original.vertices);
            if(owner!=mover)Fail("flush vertex owner changed during release");
            owner=nullptr;
        }
        q.pool.work();pool.nodes.at(offset)={};q.recordReleasedNode(current);
        // Original reads +30 AFTER zeroing owner/parent. This restricted
        // mutation-only permission does not authenticate free query data.
        q.admitReleasedNode(current);q.pool.work();
        current=model.nodes[NodeIndex(model,current)].renderBound;
    }
    q.pool.work();q.writeBrushRoot(mover,-1);
}
} // namespace QuestVr
