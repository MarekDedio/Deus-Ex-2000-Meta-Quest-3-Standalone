#pragma once

#include "portable_model_geometry.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace QuestVr {
struct OriginalBrushPoolQueries {
    std::function<void()> work;
    // The caller measures current geometry/bookkeeping AND its beforeimages.
    // Admit the additional overlapping allocation before reserve/replacement.
    std::function<void(std::size_t)> validateExtra;
};
struct OriginalBrushModelCounts {
    std::size_t surfaces{},vectors{},points{},nodes{},vertices{};
};
template<class Owner> struct OriginalBrushNodeOwner {Owner* owner{};std::int32_t parent{};};
template<class Owner> struct OriginalBrushPointOwner {
    // Native word is a real Mover pointer while active, absolute free-next
    // index otherwise. Typed fields must never truncate a 64-bit host pointer.
    Owner* owner{};
    std::int32_t freeNext{};
};
template<class Owner> struct OriginalBrushTrackerPools {
    OriginalBrushModelCounts original;
    std::size_t postSetupPoints{};
    std::int32_t nodeCursor{},vertexCursor{},freePoint{-1};
    std::vector<OriginalBrushNodeOwner<Owner>> nodes;
    std::vector<Owner*> vertices;
    std::vector<OriginalBrushPointOwner<Owner>> points;
};
namespace BrushTrackerPoolDetail {
[[noreturn]] inline void Fail(const char* why) {
    throw std::runtime_error(std::string("Original brush tracker pools: ")+why);
}
inline std::size_t Bytes(std::size_t count,std::size_t stride) {
    if(stride&&count>std::numeric_limits<std::size_t>::max()/stride)Fail("byte arithmetic overflow");
    return count*stride;
}
inline void Callbacks(const OriginalBrushPoolQueries& q) {
    if(!q.work||!q.validateExtra)Fail("requires shared work/byte callbacks");
}
inline std::int32_t Signed(std::size_t value) {
    if(value>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))Fail("array exceeds signed index space");
    return static_cast<std::int32_t>(value);
}
template<class T> inline void Exact(std::vector<T>& values,const OriginalBrushPoolQueries& q) {
    if(values.capacity()==values.size())return;
    q.validateExtra(Bytes(values.size(),sizeof(T)));
    std::vector<T> replacement(values.begin(),values.end());
    if(replacement.capacity()!=values.size())Fail("pinned exact array allocation contract differs");
    values.swap(replacement);
}
template<class T> inline void AppendZero(std::vector<T>& values,const OriginalBrushPoolQueries& q) {
    const auto count=values.size()+1u;Signed(count);
    if(values.capacity()<count) {
        const auto capacity=count+(3u*count)/8u+32u;
        q.validateExtra(Bytes(capacity,sizeof(T)));values.reserve(capacity);
        if(values.capacity()!=capacity)Fail("pinned growth array capacity differs");
    }
    values.emplace_back();
}
template<class T> inline void Grow(std::vector<T>& values,std::size_t ceiling,const OriginalBrushPoolQueries& q) {
    const auto old=values.size();Signed(old);
    const auto target=std::min(2u*old+512u,ceiling);
    if(target>old) {
        if(target>values.capacity()) {
            const auto capacity=target+(3u*target)/8u+32u;
            q.validateExtra(Bytes(capacity,sizeof(T)));values.reserve(capacity);
            if(values.capacity()!=capacity)Fail("pinned pool capacity differs");
        }
        for(std::size_t i=old;i<target;++i){q.work();values.emplace_back();}
    }
    Exact(values,q);
}
template<class Owner> inline void NodeShape(const PortableModelGeometry& model,
    const OriginalBrushTrackerPools<Owner>& pool) {
    if(pool.original.nodes>model.nodes.size()||model.nodes.size()-pool.original.nodes!=pool.nodes.size())
        Fail("node owner array lost actual Model correspondence");
    if(pool.nodeCursor<0||static_cast<std::size_t>(pool.nodeCursor)<pool.original.nodes||
        static_cast<std::size_t>(pool.nodeCursor)>model.nodes.size())Fail("node cursor outside pool");
}
template<class Owner> inline void VertexShape(const PortableModelGeometry& model,
    const OriginalBrushTrackerPools<Owner>& pool) {
    if(pool.original.vertices>model.vertices.size()||model.vertices.size()-pool.original.vertices!=pool.vertices.size())
        Fail("vertex owner array lost actual Model correspondence");
    if(pool.vertexCursor<0||static_cast<std::size_t>(pool.vertexCursor)<pool.original.vertices||
        static_cast<std::size_t>(pool.vertexCursor)>model.vertices.size())Fail("vertex cursor outside pool");
}
} // namespace BrushTrackerPoolDetail

inline OriginalBrushModelCounts OriginalBrushCounts(const PortableModelGeometry& model) {
    return {model.surfaces.size(),model.vectors.size(),model.points.size(),model.nodes.size(),model.vertices.size()};
}
template<class Owner> inline std::size_t OriginalBrushPoolsBytes(const OriginalBrushTrackerPools<Owner>& pool) {
    using BrushTrackerPoolDetail::Bytes;
    return sizeof(pool)+Bytes(pool.nodes.capacity(),sizeof(OriginalBrushNodeOwner<Owner>))+
        Bytes(pool.vertices.capacity(),sizeof(Owner*))+Bytes(pool.points.capacity(),sizeof(OriginalBrushPointOwner<Owner>));
}
template<class Owner> inline void InitializeOriginalBrushPools(PortableModelGeometry& model,
    OriginalBrushTrackerPools<Owner>& pool,const OriginalBrushPoolQueries& q) {
    using namespace BrushTrackerPoolDetail;Callbacks(q);
    if(!pool.nodes.empty()||!pool.vertices.empty()||!pool.points.empty()||pool.freePoint!=-1||
        pool.original.nodes!=model.nodes.size()||pool.original.vertices!=model.vertices.size()||
        pool.original.surfaces>model.surfaces.size()||pool.original.vectors>model.vectors.size()||pool.original.points>model.points.size())
        Fail("constructor requires original counts and fresh owner arrays");
    pool.postSetupPoints=model.points.size();Signed(pool.postSetupPoints);
    Exact(model.vectors,q);Exact(model.surfaces,q);
    Grow(model.nodes,65536u,q);pool.nodeCursor=Signed(pool.original.nodes);
    Grow(model.vertices,static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()),q);
    pool.vertexCursor=Signed(pool.original.vertices);
    const auto nodes=model.nodes.size()-pool.original.nodes;
    q.validateExtra(Bytes(nodes,sizeof(OriginalBrushNodeOwner<Owner>)));pool.nodes.resize(nodes);
    const auto verts=model.vertices.size()-pool.original.vertices;
    q.validateExtra(Bytes(verts,sizeof(Owner*)));pool.vertices.resize(verts);
}
template<class Owner> inline std::int32_t AllocateOriginalBrushNode(const PortableModelGeometry& model,
    OriginalBrushTrackerPools<Owner>& pool,Owner* owner,std::int32_t parent,const OriginalBrushPoolQueries& q) {
    using namespace BrushTrackerPoolDetail;Callbacks(q);NodeShape(model,pool);
    if(!owner||parent<0||static_cast<std::size_t>(parent)>=model.nodes.size())Fail("node allocation lacks actual owner/parent");
    const auto saved=pool.nodeCursor;
    const auto scan=[&](std::int32_t end) {
        while(pool.nodeCursor<end) {
            q.work();auto& slot=pool.nodes[static_cast<std::size_t>(pool.nodeCursor)-pool.original.nodes];
            if(!slot.owner){slot={owner,parent};return pool.nodeCursor;}
            ++pool.nodeCursor;
        }
        return std::int32_t{-1};
    };
    const auto found=scan(Signed(model.nodes.size()));if(found!=-1)return found;
    pool.nodeCursor=Signed(pool.original.nodes);return scan(saved);
}
template<class Owner> inline std::int32_t AllocateOriginalBrushVertices(const PortableModelGeometry& model,
    OriginalBrushTrackerPools<Owner>& pool,Owner* owner,std::int32_t count,const OriginalBrushPoolQueries& q) {
    using namespace BrushTrackerPoolDetail;Callbacks(q);VertexShape(model,pool);
    // Selected native clipping admits0..16, including its unusual count0
    // branch: one inspected free slot is owned and returned instead of silently
    // substituting an empty allocation. Negative/excess counts are unsupported.
    if(!owner||count<0||count>16)Fail("vertex allocation lacks owner/native polygon count");
    const auto saved=pool.vertexCursor;
    const auto scan=[&](std::int32_t end) {
        std::int32_t run{};
        while(static_cast<std::int64_t>(pool.vertexCursor)+count<end) {
            q.work();const auto offset=static_cast<std::size_t>(pool.vertexCursor)-pool.original.vertices;
            if(!pool.vertices[offset]) {
                ++run;
                if(run>=count) {
                    for(std::int32_t i=0;i<run;++i){q.work();pool.vertices[offset-static_cast<std::size_t>(i)]=owner;}
                    return pool.vertexCursor-count+1;
                }
            }else run=0;
            ++pool.vertexCursor;
        }
        return std::int32_t{-1};
    };
    const auto found=scan(Signed(model.vertices.size()));if(found!=-1)return found;
    pool.vertexCursor=Signed(pool.original.vertices);return scan(saved);
}
template<class Owner> inline std::int32_t AllocateOriginalBrushPoint(PortableModelGeometry& model,
    OriginalBrushTrackerPools<Owner>& pool,Owner* owner,const OriginalBrushPoolQueries& q) {
    using namespace BrushTrackerPoolDetail;Callbacks(q);q.work();
    if(!owner||pool.postSetupPoints>model.points.size()||model.points.size()-pool.postSetupPoints!=pool.points.size())
        Fail("point ownership lost actual Model/count correspondence");
    std::int32_t index;
    if(pool.freePoint!=-1) {
        index=pool.freePoint;
        if(index<0||static_cast<std::size_t>(index)<pool.postSetupPoints||static_cast<std::size_t>(index)>=model.points.size())
            Fail("free point outside dynamic point pool");
        auto& slot=pool.points[static_cast<std::size_t>(index)-pool.postSetupPoints];
        if(slot.owner)Fail("free point is still owned");
        pool.freePoint=slot.freeNext;slot.owner=owner; // Reuse does not zero XYZ.
    }else {
        index=Signed(model.points.size());AppendZero(model.points,q);AppendZero(pool.points,q);
        pool.points.back().owner=owner;
    }
    return index;
}
template<class Owner> inline void FreeOriginalBrushPoint(const PortableModelGeometry& model,
    OriginalBrushTrackerPools<Owner>& pool,std::int32_t index,Owner* owner,const OriginalBrushPoolQueries& q) {
    using namespace BrushTrackerPoolDetail;Callbacks(q);q.work();
    if(!owner||pool.postSetupPoints>model.points.size()||model.points.size()-pool.postSetupPoints!=pool.points.size()||
        index<0||static_cast<std::size_t>(index)<pool.postSetupPoints||static_cast<std::size_t>(index)>=model.points.size())
        Fail("free operation lost actual dynamic point ownership");
    auto& slot=pool.points[static_cast<std::size_t>(index)-pool.postSetupPoints];
    if(slot.owner!=owner)Fail("free operation has another point owner");
    slot.owner=nullptr;slot.freeNext=pool.freePoint;pool.freePoint=index;
}
} // namespace QuestVr
