#pragma once

#include "quest_actor_line_trace.h"

namespace QuestVr {

namespace ActorPointCheckDetail {
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;

inline Dyadic Load(float value) {
    if(!std::isfinite(value))
        throw std::invalid_argument("Original actor point check requires finite active Float data");
    return Decode(value);
}
inline Dyadic Dimension(float value) {
    const auto result=Load(value);
    if(value<0.0f)
        throw std::invalid_argument("Original actor point check refuses negative active collision dimensions");
    return result;
}
inline Dyadic Sub(const Dyadic& a,const Dyadic& b) {return Add64(a,Negate(b));}
inline Dyadic Square(const Dyadic& value) {return Multiply64(value,value);}

// UPrimitive.PointCheck103D87E0..103D8A44. UMesh and ULodMesh inherit
// this SAME virtual slot54: unlike LineCheck there is no Mesh wrapper that
// canonicalizes zero-or-NaN extents. The primitive receiver and Flags are
// inactive. Extent.y is never read, even on a blocked query.
inline bool Check(OriginalActorLineHit& hit,const OriginalActorCylinder& owner,
    const PortableModelVec3& location,const PortableModelVec3& extent) {
    if(!owner.identity)
        throw std::invalid_argument("Original actor point check owner identity is null");

    // Neither sum is stored to Float before squaring/comparison. In particular
    // finite dimensions whose sum exceeds FLT_MAX can still return clear.
    const auto dz=Sub(Load(owner.location.z),Load(location.z));
    const auto height=Add64(Dimension(extent.z),Dimension(owner.height));
    if(Compare64(Square(height),Square(dz))<=0) return true;

    const auto dx=Sub(Load(owner.location.x),Load(location.x));
    const auto dy=Sub(Load(owner.location.y),Load(location.y));
    const auto radius=Add64(Dimension(extent.x),Dimension(owner.radius));
    if(Compare64(Square(radius),Add64(Square(dy),Square(dx)))<=0) return true;

    hit.actor=owner.identity;
    // The normal's input differences ARE spilled to Float, after the overlap
    // tests. This full three-dimensional normal remains in the caller packet.
    const auto normalZ=Sub(Load(location.z),Load(owner.location.z));
    const auto normalY=Sub(Load(location.y),Load(owner.location.y));
    const auto normalX=Sub(Load(location.x),Load(owner.location.x));
    hit.normal=ActorLineTraceDetail::Normal(
        {Store32(normalX),Store32(normalY),Store32(normalZ)},true);

    // Both retail cap branches really do subtract Extent.z, including the
    // positive-normal branch. Do not repair this into a symmetric cylinder.
    if(hit.normal.z<-.5f||hit.normal.z>.5f) {
        hit.location={location.x,location.y,Store32(Sub(Load(owner.location.z),Dimension(extent.z)))};
        return false;
    }

    // This second SafeNormal is temporary; do not replace Hit.Normal with it.
    const auto radial=ActorLineTraceDetail::Normal({hit.normal.x,hit.normal.y,0.0f},true);
    const auto xProduct=Multiply64(Dimension(extent.x),Decode(radial.x));
    const float yProduct=Store32(Multiply64(Dimension(extent.x),Decode(radial.y)));
    const float zProduct=Store32(Multiply64(Dimension(extent.x),Decode(radial.z)));
    // X's product remains PC64 until the subtraction, whereas Y/Z products
    // have Float spills. Z then has a second, distinct stored addition of the
    // original query Z. Owner.Z+Location.Z here is intentional source behavior.
    const float y=Store32(Sub(Load(owner.location.y),Decode(yProduct)));
    const float z=Store32(Sub(Load(owner.location.z),Decode(zProduct)));
    const float x=Store32(Sub(Load(owner.location.x),xProduct));
    hit.location={x,y,Store32(Add64(Decode(z),Load(location.z)))};
    return false;
}
} // namespace ActorPointCheckDetail

// Native true=clear/false=blocked. Strict cap/radial contact is clear and
// changes nothing. A hit writes only Actor, Location and Normal: seeded Next,
// Primitive, Time and Item are inactive, not validated and not overwritten.
// Null Owner bypasses every query/extent read. Portable unsafe-input policy:
// refuse active nonfinite values, negative active dimensions and overflowing
// Float stores, atomically. Inactive Extent.y, Flags and early-return fields
// remain unrestricted. The enclosing VM must share its root rollback scope.
inline bool CheckOriginalActorCylinderPoint(OriginalActorLineHit& hit,
    const std::optional<OriginalActorCylinder>& owner,const PortableModelVec3& location,
    const PortableModelVec3& extent={},std::uint32_t flags=0u) {
    static_cast<void>(flags);
    if(!owner) return true;
    auto working=hit;
    const auto clear=ActorPointCheckDetail::Check(working,*owner,location,extent);
    hit=working;
    return clear;
}

} // namespace QuestVr
