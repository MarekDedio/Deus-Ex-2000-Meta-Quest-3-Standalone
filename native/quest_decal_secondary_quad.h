#pragma once

#include "quest_decal_primary_quad.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace QuestVr {

struct DecalSecondarySample {
    std::int32_t outer{},inner{};
    PortableModelVec3 point,start,end;
};
struct DecalSecondaryAngle { bool accepted{}; float signedCosine{}; };
struct DecalSecondaryQuad {
    std::array<PortableModelVec3,4> worldCorners{},relativeCorners{};
};

namespace DecalSecondaryDetail {
using namespace DecalPrimaryDetail;
inline double Dot(const PortableModelVec3 a,const PortableModelVec3 b) {
    return (static_cast<double>(a.z)*b.z+static_cast<double>(a.y)*b.y)+static_cast<double>(a.x)*b.x;
}
inline double LengthZyx(const PortableModelVec3 v) {
    // Inline secondary corner Size orders Z^2 + Y^2, then X^2.
    return std::sqrt((static_cast<double>(v.z)*v.z+static_cast<double>(v.y)*v.y)+static_cast<double>(v.x)*v.x);
}
inline PortableModelVec3 ProjectCorner(const PortableModelVec3 q,const PortableModelVec3 e,
    const PortableModelVec3 base,const PortableModelVec3 normal) {
    const auto direction=Subtract(q,e);
    // Both FVector::operator| results are explicitly FLOAT-stored before
    // their division (103E8618/8640 and 103E86D4/86F9).
    const float numerator=static_cast<float>(Dot(Subtract(base,e),normal));
    const float denominator=static_cast<float>(Dot(direction,normal));
    if (!std::isfinite(numerator) || !std::isfinite(denominator) || denominator==0.0f)
        Fail("singular/nonfinite secondary corner projection");
    const float ratio=numerator/denominator;
    if (!std::isfinite(ratio)) Fail("secondary corner ratio overflow");
    return Add(e,Scale(direction,ratio));
}
} // namespace DecalSecondaryDetail

// Fresh Attach upper-clamps the reflected MultiDecalLevel to4 after viewport
// and texture guards. This geometry helper only exposes that resulting value;
// caller writes belong in the surrounding host transaction, not this query.
inline std::int32_t ClampOriginalDecalMultiLevel(const std::int32_t level) { return std::min(level,4); }

inline std::vector<DecalSecondarySample> BuildOriginalDecalSecondaryGrid(
    const std::array<PortableModelVec3,4>& primaryCorners,const PortableModelVec3 primaryNormal,
    const std::int32_t multiLevel) {
    using namespace DecalSecondaryDetail;
    for (const auto p:primaryCorners) if (!Finite(p)) Fail("nonfinite primary grid corner");
    if (!Finite(primaryNormal)) Fail("nonfinite primary grid normal");
    // Original performs UnsafeNormal before entering even an empty grid. The
    // primary X dot N acceptance guard ordinarily excludes zero normals.
    const auto offset=Scale(Normalize(primaryNormal,false),-16.0);
    const auto e0=Subtract(primaryCorners[1],primaryCorners[0]),e1=Subtract(primaryCorners[3],primaryCorners[0]);
    const auto level=ClampOriginalDecalMultiLevel(multiLevel);
    std::vector<DecalSecondarySample> result;
    if (level<=0) return result;
    result.reserve(static_cast<std::size_t>(level*level));
    for (std::int32_t i=0;i<level;++i) for (std::int32_t j=0;j<level;++j) {
        // Fractions remain in the native register, not rounded to float. Each
        // scaled vector is stored, then two separately stored additions follow.
        const auto point=Add(Add(primaryCorners[0],Scale(e0,static_cast<double>(i+1)/level)),
            Scale(e1,static_cast<double>(j+1)/level));
        result.push_back({i,j,point,Subtract(point,offset),Add(point,offset)});
    }
    return result;
}

inline DecalSecondaryAngle CheckOriginalDecalSecondaryAngle(const PortableModelVec3 primaryNormal,
    const PortableModelVec3 secondaryNormal) {
    using namespace DecalSecondaryDetail;
    if (!Finite(primaryNormal) || !Finite(secondaryNormal)) Fail("nonfinite secondary angle normal");
    const float primaryLength=static_cast<float>(std::sqrt(Square(primaryNormal)));
    const double secondaryLength=std::sqrt(Square(secondaryNormal));
    const double denominator=static_cast<double>(primaryLength)*secondaryLength;
    if (!std::isfinite(denominator) || denominator<=0.0) Fail("singular secondary angle");
    const double cosine=Dot(primaryNormal,secondaryNormal)/denominator;
    const float stored=static_cast<float>(cosine);
    if (!std::isfinite(cosine) || !std::isfinite(stored)) Fail("secondary angle overflow");
    // fsts saves signed cosine without replacing the register: acceptance
    // uses unrounded absCos > DOUBLE0.7; reprojection uses stored signedCos.
    return {std::fabs(cosine)>0.7,stored};
}

inline DecalSecondaryQuad BuildOriginalDecalSecondaryQuad(
    const std::array<PortableModelVec3,4>& primaryCorners,const PortableModelVec3 primaryNormal,
    const PortableModelVec3 secondaryBase,const PortableModelVec3 secondaryNormal,const float signedCosine) {
    using namespace DecalSecondaryDetail;
    if (!Finite(primaryNormal) || !Finite(secondaryBase) || !Finite(secondaryNormal) ||
        !std::isfinite(signedCosine) || signedCosine==0.0f) Fail("invalid secondary quad input");
    DecalSecondaryQuad result;
    for (std::size_t i=0;i<primaryCorners.size();++i) {
        const auto q=primaryCorners[i]; if (!Finite(q)) Fail("nonfinite primary reprojection corner");
        const auto r=ProjectCorner(q,Subtract(q,primaryNormal),secondaryBase,secondaryNormal);
        const auto s=ProjectCorner(q,Subtract(q,secondaryNormal),secondaryBase,secondaryNormal);
        const float a=static_cast<float>(LengthZyx(Subtract(q,s))/signedCosine);
        const float b=static_cast<float>(LengthZyx(Subtract(q,r))/signedCosine);
        if (!std::isfinite(a) || !std::isfinite(b)) Fail("secondary correction overflow");
        const auto direction=Normalize(Subtract(s,r),true);
        // Retain the original two-line correction, NOT simple projection.
        result.worldCorners[i]=Subtract(s,Scale(direction,b-a));
        result.relativeCorners[i]=Subtract(result.worldCorners[i],secondaryBase);
    }
    return result;
}

} // namespace QuestVr
