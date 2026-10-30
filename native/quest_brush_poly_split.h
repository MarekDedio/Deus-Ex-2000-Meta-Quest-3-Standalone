#pragma once

#include "portable_brush_polys.h"
#include "quest_model_owner_coords.h"

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

// Only fields actually consumed/produced by selected moving-brush clipping.
// This is NOT the native FPoly ABI or authority for omitted texture/ref/link
// fields. Unused vertex slots are portable value storage, not native ctor-init.
struct OriginalBrushClipPolygon {
    PortableModelVec3 base,normal;
    std::array<PortableModelVec3,16u> vertices{};
    std::int32_t vertexCount{};
    std::uint32_t polyFlags{};
};
struct OriginalBrushPlaneSplit {
    // Native SplitWithPlaneFast: 0coplanar,1front,2back,3split.
    std::int32_t side{};
    // Present only for side3 with requested construction. Only Base, Normal,
    // Flags, count and active vertices have a source-defined fragment value.
    std::optional<OriginalBrushClipPolygon> front,back;
};

namespace BrushPolySplitDetail {
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using ModelOwnerCoordsDetail::Load;
using ModelOwnerCoordsDetail::Sub;

inline void Count(std::int32_t count) {
    if(count<0||count>16)
        throw std::invalid_argument("Original brush clipping exceeds fixed native vertex storage");
}
inline void Append(OriginalBrushClipPolygon& poly,PortableModelVec3 vertex) {
    if(poly.vertexCount<0||poly.vertexCount>=16)
        throw std::runtime_error("Original brush plane split output exceeds native vertex capacity");
    poly.vertices[static_cast<std::size_t>(poly.vertexCount++)]=vertex;
}
inline Dyadic Distance(PortableModelVec3 vertex,const OriginalExtentPlane& plane) {
    // Engine10386AFB..B23: (Z+X)+Y-W stays PC64, not Float-spilled.
    const auto zx=Add64(Multiply64(Load(plane.z),Load(vertex.z)),
        Multiply64(Load(plane.x),Load(vertex.x)));
    return Sub(Add64(zx,Multiply64(Load(plane.y),Load(vertex.y))),Load(plane.w));
}
inline PortableModelVec3 Intersection(PortableModelVec3 previous,PortableModelVec3 current,
    const OriginalExtentPlane& plane) {
    // Engine10386C20..CF7 has asymmetric spills. Retain original X delta,
    // but also perform its observed Float store before the denominator uses
    // that rounded value. Y/Z deltas and offsets each have Float boundaries.
    const auto dxRetained=Sub(Load(current.x),Load(previous.x));
    const float dx=Store32(dxRetained),
        dy=Store32(Sub(Load(current.y),Load(previous.y))),
        dz=Store32(Sub(Load(current.z),Load(previous.z)));
    const auto xz=Add64(Multiply64(Load(plane.x),Load(previous.x)),
        Multiply64(Load(plane.z),Load(previous.z)));
    const auto numerator=Sub(Load(plane.w),Add64(xz,
        Multiply64(Load(plane.y),Load(previous.y))));
    const auto zy=Add64(Multiply64(Load(plane.z),Load(dz)),
        Multiply64(Load(plane.y),Load(dy)));
    const auto denominator=Add64(zy,Multiply64(Load(dx),Load(plane.x)));
    if(!denominator.significand)
        throw std::runtime_error("Original brush split reaches active undefined plane division");
    const auto t=Store32(ModelFastLineDetail::Divide64(numerator,denominator));
    const auto xOffset=Multiply64(dxRetained,Load(t));
    const float yOffset=Store32(Multiply64(Load(dy),Load(t))),
        zOffset=Store32(Multiply64(Load(dz),Load(t)));
    return {Store32(Add64(xOffset,Load(previous.x))),
        Store32(Add64(Load(yOffset),Load(previous.y))),
        Store32(Add64(Load(zOffset),Load(previous.z)))};
}
} // namespace BrushPolySplitDetail

inline OriginalBrushClipPolygon MakeOriginalBrushClipPolygon(const PortableBrushPoly& source) {
    BrushPolySplitDetail::Count(source.vertexCount);
    // Bit copies only: neither native clipping nor this adapter numerically
    // consumes Base/Normal or inactive vertex slots merely to copy them.
    return {source.base,source.normal,source.vertices,source.vertexCount,source.polyFlags};
}

inline std::pair<OriginalBrushClipPolygon,OriginalBrushClipPolygon>
HalveOriginalBrushClipPolygon(const OriginalBrushClipPolygon& source) {
    // Engine10386F70..1038707B sends invalid counts to the real error provider.
    // There is no fabricated replacement half/triangle or silent success.
    if(source.vertexCount<=3||source.vertexCount>16)
        throw std::runtime_error("Unsupported original FPoly.SplitInHalf error provider: count must be4..16");
    auto current=source,output=source;
    const auto middle=source.vertexCount/2;
    current.vertexCount=middle+1;
    output.vertexCount=source.vertexCount-middle+1;
    for(std::int32_t i=0;i<output.vertexCount-1;++i)
        output.vertices[static_cast<std::size_t>(i)]=source.vertices[static_cast<std::size_t>(middle+i)];
    output.vertices[static_cast<std::size_t>(output.vertexCount-1)]=source.vertices[0];
    current.polyFlags|=0x80000000u;output.polyFlags|=0x80000000u;
    // Pair order is mutated input first, copied/rearranged output second.
    // FilterPoly must recurse second FIRST, then continue first.
    return {current,output};
}

inline OriginalBrushPlaneSplit SplitOriginalBrushClipPolygon(const OriginalBrushClipPolygon& source,
    const OriginalExtentPlane& plane,bool buildFragments=true) {
    using namespace BrushPolySplitDetail;
    Count(source.vertexCount);
    OriginalBrushPlaneSplit result;
    if(!source.vertexCount)return result; // Native empty input reads no Plane.
    std::array<bool,16u> backSide{};
    bool hasFront{},hasBack{};
    const auto zero=Decode(0.0f),frontThreshold=Decode(0.25f),backThreshold=Decode(-0.25f);
    for(std::int32_t i=0;i<source.vertexCount;++i) {
        const auto distance=Distance(source.vertices[static_cast<std::size_t>(i)],plane);
        const bool back=Compare64(distance,zero)<0;
        backSide[static_cast<std::size_t>(i)]=back;
        // Sign bits and strict tolerance classification are separate. Values
        // at exactly +/-0.25 do not set the corresponding has-side predicate.
        if(back)hasBack=hasBack||Compare64(distance,backThreshold)<0;
        else hasFront=hasFront||Compare64(distance,frontThreshold)>0;
    }
    if(!hasFront) {result.side=hasBack?2:0;return result;}
    if(!hasBack) {result.side=1;return result;}
    result.side=3;
    if(!buildFragments)return result; // Selected native null-front query path.

    OriginalBrushClipPolygon front,back;
    auto previous=source.vertices[static_cast<std::size_t>(source.vertexCount-1)];
    bool previousBack=backSide[static_cast<std::size_t>(source.vertexCount-1)];
    for(std::int32_t i=0;i<source.vertexCount;++i) {
        const auto current=source.vertices[static_cast<std::size_t>(i)];
        const auto currentBack=backSide[static_cast<std::size_t>(i)];
        if(currentBack!=previousBack) {
            const auto intersection=Intersection(previous,current,plane);
            // The same computed Float bits go to both outputs, not two
            // independent evaluations or welded/epsilon-adjusted points.
            Append(back,intersection);Append(front,intersection);
        }
        if(currentBack)Append(back,current);else Append(front,current);
        previous=current;previousBack=currentBack;
    }
    // Only these fields are copied by10386DD7..E5F. Omitted native FPoly fields
    // are not invented; this typed subset deliberately has no such members.
    front.base=source.base;front.normal=source.normal;front.polyFlags=source.polyFlags;
    back.base=source.base;back.normal=source.normal;back.polyFlags=source.polyFlags;
    result.front=std::move(front);result.back=std::move(back);
    return result;
}

} // namespace QuestVr
