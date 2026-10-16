#pragma once

#include "quest_model_fast_line_check.h"
#include "quest_script_random.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace QuestVr {

// Detached scalar prerequisites of retail FCollisionHash, not a registered
// actor hash, MoveActor, original world startup or a spatial query service.
struct OriginalCollisionHashBox {PortableModelVec3 minimum{},maximum{};};
struct OriginalCollisionHashRange {
    std::array<std::int32_t,3u> minimum{},maximum{}; // Both endpoints inclusive.
};
inline constexpr std::size_t OriginalCollisionHashTableSize=16'384u;
inline constexpr std::size_t OriginalCollisionHashInitializationDraws=3u*OriginalCollisionHashTableSize;
struct OriginalCollisionHashTables {
    std::array<std::uint32_t,OriginalCollisionHashTableSize> x{},y{},z{};
};

namespace CollisionHashMathDetail {
using ActorTimerDetail::Bits;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::RoundShift;
using ActorTimerDetail::Store32;
using ActorTimerDetail::Wide;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Negate;
[[noreturn]] inline void Fail(const char* reason) {
    throw std::invalid_argument(std::string("Original collision hash math: ")+reason);
}
inline Dyadic Load(float value) {
    if(!std::isfinite(value)) Fail("nonfinite active Float data");
    return Decode(value);
}
inline Dyadic Dimension(float value) {
    auto result=Load(value);
    if(value<0.0f) Fail("negative active collision dimension");
    return result;
}
inline Dyadic Sub(const Dyadic& a,const Dyadic& b) {return Add64(a,Negate(b));}
inline void Cell(std::int32_t value) {
    if(value<0||value>=static_cast<std::int32_t>(OriginalCollisionHashTableSize)) Fail("cell outside native14-bit clamped range");
}
inline std::int32_t Integer(float value) {
    // Actual FISTP occurs AFTER a Float store/reload and BEFORE integer clamp.
    // Refuse masked-invalid conversion rather than inventing a saturated cell.
    const auto number=Load(value);
    Wide magnitude{};
    if(number.significand) {
        if(number.exponent>=0) {
            if(static_cast<std::int64_t>(Bits(number.significand))+number.exponent>32)
                Fail("FISTP signed32 overflow");
            magnitude=number.significand<<static_cast<unsigned>(number.exponent);
        } else magnitude=RoundShift(number.significand,static_cast<unsigned>(-number.exponent));
    }
    const Wide maximum=number.negative?Wide{0x80000000u}:Wide{0x7fffffffu};
    if(magnitude>maximum) Fail("FISTP signed32 overflow");
    const auto signedValue=number.negative?-static_cast<std::int64_t>(magnitude):static_cast<std::int64_t>(magnitude);
    return static_cast<std::int32_t>(signedValue);
}
inline void Retained(const Dyadic& value) {
    // Public retained input must represent a finite PC64/x87 datum. Scalar
    // consumers produce this from Float differences; reject hostile packets
    // before exponent arithmetic or shifts. Zero's exponent is immaterial.
    const auto bits=Bits(value.significand);
    if(bits>64u) Fail("retained grid input is not PC64");
    if(!bits) return;
    const auto top=static_cast<std::int64_t>(value.exponent)+bits-1u;
    if(value.exponent< -16445||top>16383) Fail("retained grid input is outside finite x87 range");
}
inline OriginalCollisionHashBox EndpointBox(const PortableModelVec3& end,const PortableModelVec3& start) {
    // Core's invalid box adds Start then End. Equal minimum and maximum
    // comparisons both keep the OLD value, including either signed zero.
    const auto fold=[](float from,float to) {
        Load(from);Load(to);return std::pair{to<from?to:from,to>from?to:from};
    };
    const auto x=fold(start.x,end.x),y=fold(start.y,end.y),z=fold(start.z,end.z);
    return {{x.first,y.first,z.first},{x.second,y.second,z.second}};
}
} // namespace CollisionHashMathDetail

inline std::int32_t OriginalCollisionHashRetainedCell(const ActorTimerDetail::Dyadic& coordinate) {
    using namespace CollisionHashMathDetail;
    Retained(coordinate);
    const auto scaled=Multiply64(Add64(coordinate,Decode(32768.0f)),Decode(1.0f/256.0f));
    const auto integer=Integer(Store32(scaled));
    return std::clamp(integer,std::int32_t{0},static_cast<std::int32_t>(OriginalCollisionHashTableSize-1u));
}
inline std::int32_t OriginalCollisionHashCell(float coordinate) {
    return OriginalCollisionHashRetainedCell(CollisionHashMathDetail::Load(coordinate));
}

// UPrimitive.GetCollisionBoundingBox with a genuine nonnull owner. Its +1 pad
// and retained-Y radius are unlike the pin's raw cylinder/brush+0.1 box.
inline OriginalCollisionHashBox OriginalPrimitiveCollisionBounds(const PortableModelVec3& location,
    float radius,float height) {
    using namespace CollisionHashMathDetail;
    const auto retainedRadius=Add64(Dimension(radius),Decode(1.0f));
    const float storedRadius=Store32(retainedRadius);
    const float storedHeight=Store32(Add64(Dimension(height),Decode(1.0f)));
    const float maxX=Store32(Add64(Decode(storedRadius),Load(location.x))),
        maxY=Store32(Add64(retainedRadius,Load(location.y))),
        maxZ=Store32(Add64(Decode(storedHeight),Load(location.z)));
    const float minX=Store32(Sub(Load(location.x),Decode(storedRadius))),
        minY=Store32(Sub(Load(location.y),retainedRadius)),
        minZ=Store32(Sub(Load(location.z),Decode(storedHeight)));
    return {{minX,minY,minZ},{maxX,maxY,maxZ}};
}

inline OriginalCollisionHashRange OriginalCollisionHashBoxRange(const OriginalCollisionHashBox& box) {
    OriginalCollisionHashRange result;
    // Actual GetActorExtent converts minimum XYZ then maximum XYZ. An
    // inverted finite box remains an inverted range, not normalized/repaired.
    result.minimum={OriginalCollisionHashCell(box.minimum.x),OriginalCollisionHashCell(box.minimum.y),
        OriginalCollisionHashCell(box.minimum.z)};
    result.maximum={OriginalCollisionHashCell(box.maximum.x),OriginalCollisionHashCell(box.maximum.y),
        OriginalCollisionHashCell(box.maximum.z)};
    return result;
}

inline OriginalCollisionHashRange OriginalCollisionHashSweptRange(const PortableModelVec3& end,
    const PortableModelVec3& start,const PortableModelVec3& extent) {
    using namespace CollisionHashMathDetail;
    const auto box=EndpointBox(end,start);
    // 103591F9: min Z Float-store FIRST, then min Y; X retains its
    // subtraction through shifted/scaled conversion. Max likewise stores Z,
    // then Y, while X addition stays retained until the final grid spill.
    const float minZ=Store32(Sub(Load(box.minimum.z),Dimension(extent.z))),
        minY=Store32(Sub(Load(box.minimum.y),Dimension(extent.y)));
    OriginalCollisionHashRange result;
    result.minimum={OriginalCollisionHashRetainedCell(Sub(Load(box.minimum.x),Dimension(extent.x))),
        OriginalCollisionHashCell(minY),OriginalCollisionHashCell(minZ)};
    const float maxZ=Store32(Add64(Load(box.maximum.z),Dimension(extent.z))),
        maxY=Store32(Add64(Load(box.maximum.y),Dimension(extent.y)));
    result.maximum={OriginalCollisionHashRetainedCell(Add64(Dimension(extent.x),Load(box.maximum.x))),
        OriginalCollisionHashCell(maxY),OriginalCollisionHashCell(maxZ)};
    return result;
}

inline std::uint32_t OriginalCollisionHashEncodedCell(std::int32_t x,std::int32_t y,std::int32_t z) {
    using namespace CollisionHashMathDetail;Cell(x);Cell(y);Cell(z);
    // Literal SHL/ADD contract; fourteen-bit axis fields OVERLAP. OR is wrong.
    return ((static_cast<std::uint32_t>(z)<<8u)+static_cast<std::uint32_t>(y))*256u+static_cast<std::uint32_t>(x);
}
inline std::uint32_t OriginalCollisionHashBucket(const OriginalCollisionHashTables& tables,
    std::int32_t x,std::int32_t y,std::int32_t z) {
    using namespace CollisionHashMathDetail;Cell(x);Cell(y);Cell(z);
    const auto a=tables.x[static_cast<std::size_t>(x)],b=tables.y[static_cast<std::size_t>(y)],
        c=tables.z[static_cast<std::size_t>(z)];
    if(a>=OriginalCollisionHashTableSize||b>=OriginalCollisionHashTableSize||c>=OriginalCollisionHashTableSize)
        Fail("active permutation entry outside native bucket range");
    return a^b^c;
}
inline void ValidateOriginalCollisionHashTables(const OriginalCollisionHashTables& tables) {
    std::array<bool,OriginalCollisionHashTableSize> seen{};
    for(const auto* table:{&tables.x,&tables.y,&tables.z}) {
        seen.fill(false);
        for(const auto value:*table) {
            if(value>=seen.size()||seen[value]) CollisionHashMathDetail::Fail("axis table is not a14-bit permutation");
            seen[value]=true;
        }
    }
}
inline std::size_t CheckedOriginalCollisionHashCellCount(const OriginalCollisionHashRange& range,std::size_t budget) {
    using namespace CollisionHashMathDetail;
    bool empty{};
    for(std::size_t axis=0;axis<3u;++axis) {
        Cell(range.minimum[axis]);Cell(range.maximum[axis]);
        empty=empty||range.minimum[axis]>range.maximum[axis];
    }
    if(empty) return 0u;
    std::size_t count=1u;
    for(std::size_t axis=0;axis<3u;++axis) {
        const auto width=static_cast<std::size_t>(range.maximum[axis]-range.minimum[axis])+1u;
        if(width>budget/count) Fail("inclusive cell product exceeds budget");
        count*=width;
    }
    return count;
}

// Explicit FIRST process-table initialization only. The owning caller must
// track the genuine initialized lifecycle; neither per-map calls nor a magic
// baseline seed are hidden here. Even a late draw-budget refusal leaves the
// supplied shared seed unchanged. These are not decreasing-range shuffles.
inline OriginalCollisionHashTables InitializeOriginalCollisionHashTables(std::uint32_t& sharedSeed,
    std::size_t drawBudget=OriginalCollisionHashInitializationDraws) {
    using namespace CollisionHashMathDetail;
    if(drawBudget>OriginalCollisionHashInitializationDraws) Fail("initialization draw budget may only be tightened");
    OriginalCollisionHashTables result;
    for(std::size_t index=0u;index<OriginalCollisionHashTableSize;++index)
        result.x[index]=result.y[index]=result.z[index]=static_cast<std::uint32_t>(index);
    auto seed=sharedSeed;std::size_t draws{};
    const auto draw=[&] {
        if(draws>=drawBudget) Fail("initialization draw budget exhausted");
        ++draws;return ScriptRandom::Draw15(seed)%static_cast<std::uint32_t>(OriginalCollisionHashTableSize);
    };
    for(std::size_t index=0u;index<OriginalCollisionHashTableSize;++index) {
        std::swap(result.x[index],result.x[draw()]);
        std::swap(result.y[index],result.y[draw()]);
        std::swap(result.z[index],result.z[draw()]);
    }
    sharedSeed=seed;
    return result;
}
} // namespace QuestVr
