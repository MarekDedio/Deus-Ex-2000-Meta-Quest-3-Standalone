#pragma once

#include "quest_model_extent_clip.h"

#include <array>

namespace QuestVr {

// A captured native Owner.ToWorld FCoords VALUE, not an Actor property,
// matrix inverse, native pointer, or authority to invent the owner's transform.
// Original ABrush/AMover coordinate construction remains a separate provider.
struct OriginalModelCoords {
    PortableModelVec3 origin{},xAxis{},yAxis{},zAxis{};
};
struct OriginalModelBounds {
    PortableModelVec3 minimum{},maximum{};
    bool isValid{};
};

namespace ModelOwnerCoordsDetail {
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Negate;
inline Dyadic Load(float value) {
    if(!std::isfinite(value))
        throw std::invalid_argument("Original Model owner coordinates require finite active Float data");
    return Decode(value);
}
inline void Finite(const PortableModelVec3& value) {
    static_cast<void>(Load(value.x));static_cast<void>(Load(value.y));static_cast<void>(Load(value.z));
}
inline Dyadic Sub(const Dyadic& a,const Dyadic& b) {return Add64(a,Negate(b));}
inline Dyadic Product(float a,float b) {return Multiply64(Load(a),Load(b));}
inline Dyadic TraversalDot(const PortableModelVec3& a,const PortableModelVec3& b) {
    // Imported Core FVector::operator|, 10117710..728: (Z+Y)+X.
    const auto z=Product(b.z,a.z),y=Product(b.y,a.y);
    return Add64(Add64(z,y),Product(b.x,a.x));
}
inline Dyadic HullDot(const PortableModelVec3& axis,const PortableModelVec3& normal) {
    // Inlined TransformPlaneByOrtho, 1011A00B..056: (Y+Z)+X.
    const auto y=Product(axis.y,normal.y),z=Product(axis.z,normal.z);
    return Add64(Add64(y,z),Product(axis.x,normal.x));
}
inline float VectorComponent(const std::array<Dyadic,3u>& value,const PortableModelVec3& axis) {
    const auto x=Multiply64(value[0u],Load(axis.x)),y=Multiply64(value[1u],Load(axis.y));
    return Store32(Add64(Add64(x,y),Multiply64(value[2u],Load(axis.z))));
}
inline PortableModelVec3 Vector(const std::array<Dyadic,3u>& value,const OriginalModelCoords& coords) {
    // Core10119E40..ECC and PointBy10119D70..E08 each retain (X+Y)+Z
    // through the final component Float store. No product/addition Float spill.
    return {VectorComponent(value,coords.xAxis),VectorComponent(value,coords.yAxis),
        VectorComponent(value,coords.zAxis)};
}
} // namespace ModelOwnerCoordsDetail

// Explicit packet admission only. Individual operations below validate their
// consumed fields lazily; VectorBy does NOT consume Origin. Finite singular,
// zero, negative, nonuniform and shear axes are not rejected or normalized:
// these captured-coordinate primitives contain no inverse or determinant test.
inline void ValidateOriginalModelCoords(const OriginalModelCoords& coords) {
    ModelOwnerCoordsDetail::Finite(coords.origin);ModelOwnerCoordsDetail::Finite(coords.xAxis);
    ModelOwnerCoordsDetail::Finite(coords.yAxis);ModelOwnerCoordsDetail::Finite(coords.zAxis);
}

inline PortableModelVec3 TransformOriginalVector(const PortableModelVec3& value,const OriginalModelCoords& coords) {
    using namespace ModelOwnerCoordsDetail;
    const std::array<Dyadic,3u> loaded{Load(value.x),Load(value.y),Load(value.z)};
    return Vector(loaded,coords);
}
inline PortableModelVec3 TransformOriginalPoint(const PortableModelVec3& point,const OriginalModelCoords& coords) {
    using namespace ModelOwnerCoordsDetail;
    // All three Point-Origin differences stay PC64, NOT Float3. This exact
    // schedule is also inlined in UModel.GetCollisionBoundingBox103F0F3F..F9F.
    const std::array<Dyadic,3u> delta{Sub(Load(point.x),Load(coords.origin.x)),
        Sub(Load(point.y),Load(coords.origin.y)),Sub(Load(point.z),Load(coords.origin.z))};
    return Vector(delta,coords);
}

inline OriginalExtentPlane TransformOriginalTraversalPlane(const OriginalExtentPlane& plane,
    const OriginalModelCoords& coords) {
    using namespace ModelOwnerCoordsDetail;
    const PortableModelVec3 normal{plane.x,plane.y,plane.z};
    // Native traversal transforms/stores ZAxis first, then YAxis, then XAxis
    // (103F432D..4371), before computing Origin.TransformVectorBy(Coords).
    const float z=Store32(TraversalDot(normal,coords.zAxis)),
        y=Store32(TraversalDot(normal,coords.yAxis)),x=Store32(TraversalDot(normal,coords.xAxis));
    const PortableModelVec3 transformed{x,y,z};
    const auto origin=TransformOriginalVector(coords.origin,coords);
    const auto dot=TraversalDot(origin,transformed);
    return {x,y,z,Store32(Sub(Load(plane.w),dot))};
}
inline OriginalExtentPlane TransformOriginalHullPlane(const OriginalExtentPlane& plane,
    const OriginalModelCoords& coords) {
    using namespace ModelOwnerCoordsDetail;
    const PortableModelVec3 normal{plane.x,plane.y,plane.z};
    // Core TransformPlaneByOrtho1011A000..0F0 has distinct X/Y/Z stores and
    // Y/Z/X dot schedules. Do not Float-spill the final dot before W-dot.
    const float x=Store32(HullDot(coords.xAxis,normal)),y=Store32(HullDot(coords.yAxis,normal)),
        z=Store32(HullDot(coords.zAxis,normal));
    const PortableModelVec3 transformed{x,y,z};
    const auto origin=TransformOriginalVector(coords.origin,coords);
    const auto dot=HullDot(origin,transformed);
    return {x,y,z,Store32(Sub(Load(plane.w),dot))};
}

inline OriginalModelBounds TransformOriginalModelBounds(const PortableModelVec3& minimum,
    const PortableModelVec3& maximum,const OriginalModelCoords& coords) {
    // Nonnull-owner UModel.GetCollisionBoundingBox103F0E70..10E6 ignores
    // source BoundingBox.IsValid. It visits ALL8 endpoint combinations, X
    // outer/Y middle/Z inner. Even reversed finite endpoint pairs are folded
    // exactly as visited, not refused, repaired before transformation or padded.
    ModelOwnerCoordsDetail::Finite(minimum);ModelOwnerCoordsDetail::Finite(maximum);
    OriginalModelBounds result;
    for(unsigned x=0u;x<2u;++x) for(unsigned y=0u;y<2u;++y) for(unsigned z=0u;z<2u;++z) {
        const auto point=TransformOriginalPoint({x?maximum.x:minimum.x,y?maximum.y:minimum.y,
            z?maximum.z:minimum.z},coords);
        if(!result.isValid) {result.minimum=result.maximum=point;result.isValid=true;continue;}
        // Strict comparisons preserve the FIRST equal Float, including zero
        // sign; std::fmin/fmax or different corner order can lose those bits.
        const auto fold=[](float value,float& low,float& high) {
            if(low>value) low=value;
            if(high<value) high=value;
        };
        fold(point.x,result.minimum.x,result.maximum.x);fold(point.y,result.minimum.y,result.maximum.y);
        fold(point.z,result.minimum.z,result.maximum.z);
    }
    return result;
}
} // namespace QuestVr
