#pragma once

#include "portable_brush_polys.h"
#include "quest_actor_line_trace.h"
#include "quest_brush_coords.h"

#include <bit>
#include <limits>
#include <span>
#include <utility>

namespace QuestVr {

// Detached selected ABrush.BuildCoords/FPoly.Transform VALUES. These helpers
// neither initialize runtime GMath nor bind Model/UPolys/UObject identities.
// Caller owns span/work/transaction bounds and actual tracker integration.
struct OriginalBrushModelCoords {
    OriginalModelCoords point,vector;
    float orientation{};
};

namespace BrushPolyTransformDetail {
using ModelOwnerCoordsDetail::Load;
using ModelOwnerCoordsDetail::Sub;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;
using ScriptRandom::VectorDetail::DecodeDouble;
using ScriptRandom::VectorDetail::StoreDouble;

inline float Negative(float value) {return Store32(Negate(Load(value)));}

inline float ScaleOrientation(const OriginalBrushScale& value) {
    // Engine1032FD13/Core10118880: retained (Z*Y)*X, not magnitude or
    // Float-spilled products; each sign operation returns FILD -1/0/+1.
    const auto zy=Multiply64(Load(value.scale.z),Load(value.scale.y));
    const auto product=Multiply64(zy,Load(value.scale.x));
    const auto comparison=Compare64(product,Decode(0.0f));
    return comparison>0?1.0f:comparison<0?-1.0f:0.0f;
}

inline PortableModelVec3 Position(PortableModelVec3 value,const OriginalModelCoords& coords,
    PortableModelVec3 prePivot,PortableModelVec3 location) {
    // FPoly.Transform FLOAT-spills PreSubtract differences, then VectorBy
    // results, then PostAdd results. Generic PointBy retains different deltas
    // and consumes coords.Origin, so it is not interchangeable with this path.
    const PortableModelVec3 delta{Store32(Sub(Load(value.x),Load(prePivot.x))),
        Store32(Sub(Load(value.y),Load(prePivot.y))),Store32(Sub(Load(value.z),Load(prePivot.z)))};
    const auto transformed=TransformOriginalVector(delta,coords);
    return {Store32(Add64(Load(transformed.x),Load(location.x))),
        Store32(Add64(Load(transformed.y),Load(location.y))),
        Store32(Add64(Load(transformed.z),Load(location.z)))};
}
} // namespace BrushPolyTransformDetail

inline OriginalModelCoords InverseScaleOriginalModelCoords(OriginalModelCoords coords,
    const OriginalBrushScale& value) {
    // Core1011C860..CBDF first divides all axes, then multiplies Origin,
    // then fully composes negative-shear coordinates. No reciprocal shortcut.
    coords.xAxis=BrushCoordsDetail::Divide(coords.xAxis,value.scale);
    coords.yAxis=BrushCoordsDetail::Divide(coords.yAxis,value.scale);
    coords.zAxis=BrushCoordsDetail::Divide(coords.zAxis,value.scale);
    coords.origin=BrushCoordsDetail::Times(coords.origin,value.scale);
    auto shear=IdentityOriginalModelCoords();
    if(value.sheerAxis>=1u&&value.sheerAxis<=6u) {
        // FCHS precedes the sole coefficient Float store, including -0.
        const auto snapped=ActorTimerDetail::Store32(
            NativeWindowClipDetail::Negate(BrushCoordsDetail::Snap(value.sheerRate)));
        switch(value.sheerAxis) {
        case 1:shear.xAxis.y=snapped;break;case 2:shear.xAxis.z=snapped;break;
        case 3:shear.yAxis.x=snapped;break;case 4:shear.yAxis.z=snapped;break;
        case 5:shear.zAxis.x=snapped;break;case 6:shear.zAxis.y=snapped;break;
        }
    }
    // None/unknown axis discards native Snap's unused value/FP status, like
    // the existing forward helper, but still performs identity composition.
    return ComposeOriginalModelCoords(coords,shear);
}

inline OriginalModelCoords InverseRotateOriginalModelCoords(OriginalModelCoords coords,
    OriginalBrushRotation rotation,OriginalBrushSinTable table) {
    // Core1011B690..BA9E is inverse ROLL, PITCH, YAW. Negating the original
    // angles before quantized lookup, or forward rotation order, is wrong.
    const float sr=BrushCoordsDetail::Table(table,rotation.roll),
        cr=BrushCoordsDetail::Table(table,rotation.roll,0x4000u);
    coords=ComposeOriginalModelCoords(coords,{{},{1,0,0},{0,cr,sr},
        {0,BrushPolyTransformDetail::Negative(sr),cr}});
    const float sp=BrushCoordsDetail::Table(table,rotation.pitch),
        cp=BrushCoordsDetail::Table(table,rotation.pitch,0x4000u);
    coords=ComposeOriginalModelCoords(coords,{{},{cp,0,BrushPolyTransformDetail::Negative(sp)},
        {0,1,0},{sp,0,cp}});
    const float sy=BrushCoordsDetail::Table(table,rotation.yaw),
        cy=BrushCoordsDetail::Table(table,rotation.yaw,0x4000u);
    return ComposeOriginalModelCoords(coords,{{},{cy,BrushPolyTransformDetail::Negative(sy),0},
        {sy,cy,0},{0,0,1}});
}

inline OriginalBrushModelCoords BuildOriginalBrushModelCoords(OriginalBrushRotation rotation,
    const OriginalBrushScale& main,const OriginalBrushScale& post,OriginalBrushSinTable table) {
    // Engine1032FA1B..FB8A selected first output / null second output. No
    // Location, PrePivot, DrawScale or generic matrix inverse is read here.
    OriginalBrushModelCoords result;
    result.point=ScaleOriginalModelCoords(IdentityOriginalModelCoords(),post);
    result.point=RotateOriginalModelCoords(result.point,rotation,table);
    result.point=ScaleOriginalModelCoords(result.point,main);
    auto inverse=InverseScaleOriginalModelCoords(IdentityOriginalModelCoords(),main);
    inverse=InverseRotateOriginalModelCoords(inverse,rotation,table);
    inverse=InverseScaleOriginalModelCoords(inverse,post);
    const auto origin=TransformOriginalVector(inverse.origin,inverse);
    result.vector={{BrushPolyTransformDetail::Negative(origin.x),BrushPolyTransformDetail::Negative(origin.y),
        BrushPolyTransformDetail::Negative(origin.z)},
        {inverse.xAxis.x,inverse.yAxis.x,inverse.zAxis.x},
        {inverse.xAxis.y,inverse.yAxis.y,inverse.zAxis.y},
        {inverse.xAxis.z,inverse.yAxis.z,inverse.zAxis.z}};
    const float mainSign=BrushPolyTransformDetail::ScaleOrientation(main),
        postSign=BrushPolyTransformDetail::ScaleOrientation(post);
    result.orientation=ActorTimerDetail::Store32(ModelFastLineDetail::Multiply64(
        ActorTimerDetail::Decode(mainSign),ActorTimerDetail::Decode(postSign)));
    return result;
}

inline PortableBrushPoly TransformOriginalBrushPoly(PortableBrushPoly poly,
    const OriginalBrushModelCoords& coords,PortableModelVec3 prePivot,PortableModelVec3 location) {
    if(poly.vertexCount<0||poly.vertexCount>16)
        throw std::invalid_argument("Original brush polygon exceeds its fixed native vertex storage");
    // Engine10387360..10387754 mutation order. Only active vertex slots are
    // read; unused slots and every nongeometry serialized field are bit-copied.
    poly.textureU=TransformOriginalVector(poly.textureU,coords.vector);
    poly.textureV=TransformOriginalVector(poly.textureV,coords.vector);
    poly.base=BrushPolyTransformDetail::Position(poly.base,coords.point,prePivot,location);
    for(std::int32_t index=0;index<poly.vertexCount;++index)
        poly.vertices[static_cast<std::size_t>(index)]=BrushPolyTransformDetail::Position(
            poly.vertices[static_cast<std::size_t>(index)],coords.point,prePivot,location);
    if(NativeWindowClipDetail::Compare64(BrushPolyTransformDetail::Load(coords.orientation),
        ActorTimerDetail::Decode(0.0f))<0) {
        for(std::int32_t index=0;index<poly.vertexCount/2;++index)
            std::swap(poly.vertices[static_cast<std::size_t>(index)],
                poly.vertices[static_cast<std::size_t>(poly.vertexCount-1-index)]);
    }
    // The original normal is transformed LAST and then goes through the
    // original Core.SafeNormal schedule, not a host sqrt or unit fast path.
    poly.normal=ActorLineTraceDetail::Normal(TransformOriginalVector(poly.normal,coords.vector),true);
    return poly;
}

inline PortableModelSphere BuildOriginalBrushSphere(std::span<const PortableModelVec3> points) {
    using namespace BrushPolyTransformDetail;
    if(points.size()>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::invalid_argument("Original brush sphere point count exceeds native signed count");
    PortableModelSphere result{};
    if(points.empty())return result;
    // Core10144AB0 FBox: first sample supplies both bounds, strict later
    // comparisons preserve the first equal Float (including signed zero).
    ModelOwnerCoordsDetail::Finite(points.front());
    auto minimum=points.front(),maximum=points.front();
    const auto fold=[](float value,float& low,float& high) {
        const auto loaded=Load(value);
        if(Compare64(Load(low),loaded)>0)low=value;
        if(Compare64(Load(high),loaded)<0)high=value;
    };
    for(std::size_t index=1u;index<points.size();++index) {
        const auto& point=points[index];
        fold(point.x,minimum.x,maximum.x);fold(point.y,minimum.y,maximum.y);fold(point.z,minimum.z,maximum.z);
    }
    // Core101449AB..101449FC retains Z's sum, but FLOAT-spills X/Y sums
    // before the half products. Do not use a uniform midpoint expression.
    const auto zSum=Add64(Load(minimum.z),Load(maximum.z));
    const float ySum=Store32(Add64(Load(minimum.y),Load(maximum.y))),
        xSum=Store32(Add64(Load(minimum.x),Load(maximum.x)));
    result.x=Store32(Multiply64(Load(xSum),Decode(0.5f)));
    result.y=Store32(Multiply64(Load(ySum),Decode(0.5f)));
    result.z=Store32(Multiply64(zSum,Decode(0.5f)));
    float squared{};
    for(const auto& point:points) {
        const auto dx=Sub(Load(result.x),Load(point.x)),dy=Sub(Load(result.y),Load(point.y)),
            dz=Sub(Load(result.z),Load(point.z));
        const auto distance=Add64(Add64(Multiply64(dy,dy),Multiply64(dx,dx)),Multiply64(dz,dz));
        if(Compare64(distance,Load(squared))>0)squared=Store32(distance);
    }
    // appSqrt receives a Double argument; its retained PC64 root is NOT
    // reloaded from the observational FST Double before multiplying1.001.
    const auto root=ActorLineTraceDetail::Root(StoreDouble(Load(squared)));
    static_cast<void>(StoreDouble(root));
    const auto expansion=DecodeDouble(std::bit_cast<double>(std::uint64_t{0x3ff004189374bc6aull}));
    result.radius=Store32(Multiply64(root,expansion));
    return result;
}

} // namespace QuestVr
