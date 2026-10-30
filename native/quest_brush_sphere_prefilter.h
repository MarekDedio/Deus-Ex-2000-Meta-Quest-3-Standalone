#pragma once

#include "portable_model_geometry.h"
#include "quest_model_fast_line_check.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

namespace QuestVr {

// The caller authenticates each actual live Model/node/revision and journals
// every flag write. A copied node is neither an immutable validation proof nor
// authority to read a free dynamic pool slot. Work belongs to the SAME outer
// caller budget; this operation never creates or resets an allowance.
struct OriginalBrushSphereFilterQueries {
    std::function<PortableModelNode(std::int32_t)> readNode;
    std::function<void(std::int32_t,std::uint8_t)> writeFlags;
    std::function<void()> work;
};

namespace BrushSpherePrefilterDetail {
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Decode;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;

[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original brush sphere prefilter: ")+reason);
}
inline Dyadic Load(float value) {
    if(!std::isfinite(value))Fail("nonfinite active Float");
    return Decode(value);
}
inline void Index(std::int32_t index,std::size_t nodeCount) {
    if(index<0||static_cast<std::size_t>(index)>=nodeCount)Fail("active node index outside actual Model");
}
inline Dyadic Distance(const PortableModelNode& node,const PortableModelSphere& sphere,bool root) {
    // Root103F64A1 keeps (Z+Y)+X; helper103F63BA keeps (Y+Z)+X. Retain
    // EVERY PC64 product/addition through W subtraction: no Float/Double spill.
    Dyadic pair;
    if(root) {
        const auto z=Multiply64(Load(node.planeZ),Load(sphere.z));
        const auto y=Multiply64(Load(node.planeY),Load(sphere.y));
        pair=Add64(z,y);
        return Add64(Add64(pair,Multiply64(Load(sphere.x),Load(node.planeX))),Negate(Load(node.planeW)));
    }
    const auto y=Multiply64(Load(node.planeY),Load(sphere.y));
    const auto z=Multiply64(Load(node.planeZ),Load(sphere.z));
    pair=Add64(y,z);
    return Add64(Add64(pair,Multiply64(Load(node.planeX),Load(sphere.x))),Negate(Load(node.planeW)));
}
inline std::int32_t Child(std::int32_t parent,bool front,std::size_t nodeCount,
    const OriginalBrushSphereFilterQueries& queries) {
    // Charge even the terminal -1 edge. Reacquire the actual parent's live
    // child AFTER flag callbacks or Back recursion; do not reuse its visit
    // snapshot or select an opposite child when this one is absent.
    queries.work();
    Index(parent,nodeCount);
    const auto current=queries.readNode(parent);
    const auto child=front?current.front:current.back;
    if(child!=-1)Index(child,nodeCount);
    return child;
}
inline void Walk(std::int32_t index,bool root,std::size_t depth,std::size_t maximumDepth,
    std::size_t nodeCount,const PortableModelSphere& sphere,const OriginalBrushSphereFilterQueries& queries) {
    for(;;) {
        queries.work(); // Every visit, including a repeated shared/DAG node.
        Index(index,nodeCount);
        const auto node=queries.readNode(index);
        const auto lowFlags=static_cast<std::uint8_t>(node.nodeFlags&0x3fu);
        // This source write precedes active numeric reads/refusal. It is NOT
        // locally rolled back: the caller's native journal owns atomic undo.
        queries.writeFlags(index,lowFlags);
        const auto distance=Distance(node,sphere,root);
        // FCHS radius remains retained. Strict gates and their PRIORITY are
        // significant for finite negative radii; never clamp/abs the input.
        if(Compare64(distance,Negate(Load(sphere.radius)))<0) {
            queries.writeFlags(index,static_cast<std::uint8_t>(lowFlags|0x80u));
            index=Child(index,false,nodeCount,queries);
        }else if(Compare64(distance,Load(sphere.radius))>0) {
            queries.writeFlags(index,static_cast<std::uint8_t>(lowFlags|0x40u));
            index=Child(index,true,nodeCount,queries);
        }else {
            const auto back=Child(index,false,nodeCount,queries);
            if(back!=-1) {
                if(depth==maximumDepth)Fail("recursive depth budget exhausted");
                Walk(back,false,depth+1u,maximumDepth,nodeCount,sphere,queries);
            }
            index=Child(index,true,nodeCount,queries);
        }
        if(index==-1)return;
        // Tail visits retain both the current arithmetic mode and recursive
        // depth. No dedup/cycle set: caller work must bound repeated visits.
    }
}
} // namespace BrushSpherePrefilterDetail

inline void PrecomputeOriginalBrushSphereFilter(std::size_t nodeCount,const PortableModelSphere& sphere,
    const OriginalBrushSphereFilterQueries& queries,std::size_t maximumDepth=256u) {
    // Original root103F6480..648F ignores ALL inactive inputs when empty.
    if(!nodeCount)return;
    if(!queries.readNode||!queries.writeFlags||!queries.work)
        BrushSpherePrefilterDetail::Fail("nonempty Model requires authenticated read/write/shared work callbacks");
    // This detached portable stack ceiling may only be tightened. Root counts
    // as depth1; iterative Back/Front tails do not consume recursive depth.
    if(!maximumDepth||maximumDepth>256u)
        BrushSpherePrefilterDetail::Fail("recursive depth must be within1..256");
    BrushSpherePrefilterDetail::Walk(0,true,1u,maximumDepth,nodeCount,sphere,queries);
}

} // namespace QuestVr
