#pragma once

#include "quest_script_random_vector.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

// Detached, fixed-work math for the original internal
// APawn::LineOfSightTo(Pawn*,0,false) path. These helpers do not own field
// reads, branch selection, BSP queries, RNG draws, writes or a VM root.
namespace QuestVr::PawnLineOfSightDetail {
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;
using ScriptRandom::VectorDetail::DecodeDouble;

inline constexpr float VisibilityScale=std::bit_cast<float>(std::uint32_t{0x3c75c28fu});
inline constexpr double HeadScale=std::bit_cast<double>(std::uint64_t{0x3fe999999999999aull});
inline constexpr double Half=std::bit_cast<double>(std::uint64_t{0x3fe0000000000000ull});

inline Dyadic Load(float value) {
    if(!std::isfinite(value))
        throw std::invalid_argument("Original Pawn LOS requires finite active Float data");
    return Decode(value);
}
inline Dyadic Subtract(Dyadic a,Dyadic b) {return Add64(a,Negate(b));}
inline Dyadic SquareXYZ(Dyadic x,Dyadic y,Dyadic z) {
    return Add64(Add64(Multiply64(x,x),Multiply64(y,y)),Multiply64(z,z));
}
inline Dyadic PointSquare(const PortableModelVec3& point) {
    return SquareXYZ(Load(point.x),Load(point.y),Load(point.z));
}

// Engine103BEBCC..103BEC09: the subtractions remain PC64; only the completed
// (Xsquare+Ysquare)+Zsquare is stored Float. This is NOT531's Z/Y/X order.
inline float DistanceSquared(PortableModelVec3 target,PortableModelVec3 source) {
    const auto z=Subtract(Load(target.z),Load(source.z));
    const auto y=Subtract(Load(target.y),Load(source.y));
    const auto x=Subtract(Load(target.x),Load(source.x));
    return Store32(SquareXYZ(x,y,z));
}

// Engine103BEF60..103BF05A: the visibility factor, scaled radius and square
// remain PC64. Do not Float-spill the range before later half-range tests.
inline Dyadic RangeSquared(bool player,std::uint8_t visibility) {
    auto factor=Multiply64(Decode(static_cast<float>(static_cast<unsigned>(visibility)+16u)),
        Decode(VisibilityScale));
    if(Compare64(factor,Decode(1.0f))>=0)factor=Decode(1.0f);
    const auto radius=Multiply64(factor,Decode(player?5000.0f:4000.0f));
    auto square=Multiply64(radius,radius);
    const auto maximum=Decode(player?25000000.0f:16000000.0f);
    if(Compare64(square,maximum)>=0)square=maximum;
    return square;
}

// Only Z participates in arithmetic; X/Y are copied native words. The caller
// must not use these helpers to pre-read inactive Location/height fields.
inline PortableModelVec3 Eye(PortableModelVec3 point,float eyeHeight) {
    point.z=Store32(Add64(Load(point.z),Load(eyeHeight)));
    return point;
}
inline PortableModelVec3 Head(PortableModelVec3 point,float collisionHeight) {
    point.z=Store32(Add64(Multiply64(Load(collisionHeight),DecodeDouble(HeadScale)),Load(point.z)));
    return point;
}

struct Corners {
    std::array<PortableModelVec3,4u> points{};
    std::size_t minimum{},maximum{};
};

// Engine103BF282..103BF414: absolute WORLD coordinates, not eye-relative
// distances. Every corner coordinate is Float-stored before size comparisons.
// Extrema thresholds are also Float-stored, but each new square stays PC64;
// strict maximum is tested FIRST, then strict minimum only in its else branch.
inline Corners SelectCorners(PortableModelVec3 point,float radius) {
    Corners result;
    const auto r=Load(radius);
    const auto negativeRadius=Multiply64(r,Decode(-1.0f));
    const auto corner=[&](bool plusX,bool plusY) {
        PortableModelVec3 value;
        value.x=Store32(plusX?Add64(r,Load(point.x)):Subtract(Load(point.x),r));
        value.y=Store32(plusY?Subtract(Load(point.y),negativeRadius):Add64(negativeRadius,Load(point.y)));
        value.z=point.z;
        return value;
    };
    result.points[0u]=corner(false,true);
    result.points[1u]=corner(true,true);
    result.points[2u]=corner(false,false);
    result.points[3u]=corner(true,false);
    float minimum=Store32(PointSquare(result.points[0u]));
    float maximum=minimum;
    for(std::size_t i=1u;i<result.points.size();++i) {
        const auto square=PointSquare(result.points[i]);
        if(Compare64(square,Decode(maximum))>0) {
            maximum=Store32(square);result.maximum=i;
        } else if(Compare64(square,Decode(minimum))<0) {
            minimum=Store32(square);result.minimum=i;
        }
    }
    return result;
}

// Engine103BF10B..103BF11F calls Core.appFrand, NOT the Float-returning script
// native195. Its Float FST does not narrow the still-retained value compared
// against Double0.5. The caller owns and journals the one actual CRT draw.
inline bool RandomVisible(std::uint32_t draw15) {
    if(draw15>32767u)throw std::invalid_argument("Original Pawn LOS requires a fifteen-bit CRT sample");
    const auto retained=Multiply64(Decode(static_cast<float>(draw15)),
        Decode(std::bit_cast<float>(std::uint32_t{0x38000100u})));
    const float unusedTemporary=Store32(retained);
    static_cast<void>(unusedTemporary);
    return Compare64(retained,DecodeDouble(Half))>=0;
}
} // namespace QuestVr::PawnLineOfSightDetail
