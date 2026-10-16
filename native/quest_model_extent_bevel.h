#pragma once

#include "quest_model_extent_clip.h"

#include <optional>

namespace QuestVr {

namespace ModelExtentBevelDetail {
using ModelExtentClipDetail::Finite;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;
using ScriptRandom::VectorDetail::DecodeDouble;
using ScriptRandom::VectorDetail::StoreDouble;
using ScriptRandom::VectorDetail::SqrtStoredDouble64;

inline constexpr double ProjectedDotThreshold=std::bit_cast<double>(std::uint64_t{0x3f50624dd2f1a9fcull});
inline constexpr double IntersectionThreshold=std::bit_cast<double>(std::uint64_t{0x3eb0c6f7a0b5ed8dull});
inline constexpr double NormalizeThreshold=std::bit_cast<double>(std::uint64_t{0x3e45798ee2308c3aull});

inline PortableModelVec3 Normal(const OriginalExtentPlane& plane) {return {plane.x,plane.y,plane.z};}
inline Dyadic Product(float a,float b) {return Multiply64(Decode(a),Decode(b));}
inline Dyadic Difference(const Dyadic& a,const Dyadic& b) {return Add64(a,Negate(b));}

// Core101176C0: standard cross, each component separately Float-stored.
// Keep this retained form for the peculiar Z staging in IntersectPlanes2.
inline std::array<Dyadic,3u> RetainedCross(const PortableModelVec3& a,const PortableModelVec3& b) {
    return {Difference(Product(a.y,b.z),Product(b.y,a.z)),
        Difference(Product(a.z,b.x),Product(a.x,b.z)),
        Difference(Product(b.y,a.x),Product(a.y,b.x))};
}
inline PortableModelVec3 Cross(const PortableModelVec3& a,const PortableModelVec3& b) {
    Finite(a);Finite(b);
    const auto cross=RetainedCross(a,b);
    return {Store32(cross[0u]),Store32(cross[1u]),Store32(cross[2u])};
}

// Core10117710: Z/Y/X retained dot, NOT the FPlane constructor's Y/Z/X.
inline Dyadic Dot(const PortableModelVec3& a,const PortableModelVec3& b) {
    Finite(a);Finite(b);
    return Add64(Add64(Product(a.z,b.z),Product(a.y,b.y)),Product(a.x,b.x));
}
inline Dyadic SquaredLength(const PortableModelVec3& vector) {
    return Add64(Add64(Product(vector.x,vector.x),Product(vector.y,vector.y)),Product(vector.z,vector.z));
}

// Core10117CF0/10117DD0: both sum X/Y/Z, spill the length squared to
// Double, and keep appSqrt's PC64 result and reciprocal unspilled. Guarded
// Normalize tests the RETAINED sum after its FST Double, leaving small inputs
// unchanged. Do not reuse VRand's differently ordered Z/Y/X normalizer.
inline PortableModelVec3 CoreNormalize(const PortableModelVec3& vector,bool guarded) {
    Finite(vector);
    const auto squared=SquaredLength(vector);
    const double stored=StoreDouble(squared);
    if (guarded && Compare64(squared,DecodeDouble(NormalizeThreshold))<0) return vector;
    if (!squared.significand)
        throw std::runtime_error("Original extent bevel refused nonfinite zero UnsafeNormal result");
    const auto reciprocal=ModelFastLineDetail::Divide64(Decode(1.0f),SqrtStoredDouble64(stored));
    return {Store32(Multiply64(Decode(vector.x),reciprocal)),
        Store32(Multiply64(Decode(vector.y),reciprocal)),
        Store32(Multiply64(Decode(vector.z),reciprocal))};
}

struct PlaneIntersection {
    bool accepted{};
    PortableModelVec3 point{},direction{};
};

// Engine103F6600: return flag is deliberately preserved separately from its
// zeroed output on rejection. The actual bevel caller ignores this flag.
inline PlaneIntersection IntersectPlanes(const OriginalExtentPlane& first,const OriginalExtentPlane& second) {
    const auto a=Normal(first),b=Normal(second);
    auto direction=Cross(a,b);
    const auto determinant=SquaredLength(direction);
    if (Compare64(determinant,DecodeDouble(IntersectionThreshold))<0) return {};
    Finite(first.w);Finite(second.w); // The retail rejection branch never reads W.

    auto termA=RetainedCross(direction,a),termB=RetainedCross(b,direction);
    // X/Y cross components spill before W multiplication, and these products
    // spill again. Each Z remains retained through cross, product and sum.
    for (std::size_t axis=0u;axis<2u;++axis) {
        termA[axis]=Decode(Store32(Multiply64(Decode(Store32(termA[axis])),Decode(second.w))));
        termB[axis]=Decode(Store32(Multiply64(Decode(Store32(termB[axis])),Decode(first.w))));
    }
    termA[2u]=Multiply64(termA[2u],Decode(second.w));
    termB[2u]=Multiply64(termB[2u],Decode(first.w));
    const std::array<Dyadic,3u> sum{
        Decode(Store32(Add64(termA[0u],termB[0u]))),
        Decode(Store32(Add64(termA[1u],termB[1u]))),Add64(termA[2u],termB[2u])};
    const auto inverse=Decode(Store32(ModelFastLineDetail::Divide64(Decode(1.0f),determinant)));
    const PortableModelVec3 point{Store32(Multiply64(sum[0u],inverse)),
        Store32(Multiply64(sum[1u],inverse)),Store32(Multiply64(sum[2u],inverse))};
    return {true,point,CoreNormalize(direction,true)};
}

// Core10118360: the Y/Z/X sum and Float W store are separate from Dot.
inline OriginalExtentPlane PlaneAt(const PortableModelVec3& point,const PortableModelVec3& normal) {
    Finite(point);Finite(normal);
    return {normal.x,normal.y,normal.z,Store32(Add64(Add64(Product(point.y,normal.y),
        Product(point.z,normal.z)),Product(point.x,normal.x)))};
}
} // namespace ModelExtentBevelDetail

// One complete original hull-pair/axis bevel construction. Call pairs i then
// j<i, with axes0/1/2 in that order. None denotes only a genuine sign/dot gate
// skip; failed IntersectPlanes is NOT silently skipped. Its zero direction
// proceeds to UnsafeNormal and explicitly refuses a nonfinite result.
inline std::optional<OriginalExtentPlane> BuildOriginalExtentBevel(
    const OriginalExtentPlane& first,const OriginalExtentPlane& second,unsigned axis) {
    using namespace ModelExtentBevelDetail;
    if (axis>2u) throw std::invalid_argument("Original extent bevel axis exceeds XYZ");
    const auto firstNormal=Normal(first),secondNormal=Normal(second);
    Finite(firstNormal);Finite(secondNormal);
    const std::array<float,3u> firstComponents{first.x,first.y,first.z},secondComponents{second.x,second.y,second.z};
    const float left=firstComponents[axis],right=secondComponents[axis];
    if (!((left<0.0f && right>0.0f)||(left>0.0f && right<0.0f))) return {};
    const std::array<PortableModelVec3,3u> axes{{{1,0,0},{0,1,0},{0,0,1}}};
    const auto projectedFirst=Cross(axes[axis],firstNormal),projectedSecond=Cross(axes[axis],secondNormal);
    if (Compare64(Dot(projectedFirst,projectedSecond),DecodeDouble(ProjectedDotThreshold))<=0) return {};
    const auto intersection=IntersectPlanes(first,second); // Original ignores accepted.
    auto normal=CoreNormalize(Cross(axes[axis],intersection.direction),false);
    if (Compare64(Dot(firstNormal,normal),Decode(0.0f))<0)
        normal={Store32(Product(normal.x,-1.0f)),Store32(Product(normal.y,-1.0f)),Store32(Product(normal.z,-1.0f))};
    return PlaneAt(intersection.point,normal);
}

} // namespace QuestVr
