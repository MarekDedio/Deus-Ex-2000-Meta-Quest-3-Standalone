#pragma once

#include "quest_model_owner_coords.h"

#include <span>

namespace QuestVr {

// Detached native field VALUES. Offset names are intentional: no reflected
// MainScale/PostScale offset mapping or native UClass identity is invented.
struct OriginalBrushScale {PortableModelVec3 scale{1,1,1};float sheerRate{};std::uint8_t sheerAxis{};};
struct OriginalBrushRotation {std::int32_t pitch{},yaw{},roll{};};
using OriginalBrushSinTable=std::span<const float,16384u>;

inline OriginalModelCoords IdentityOriginalModelCoords() {return {{},{1,0,0},{0,1,0},{0,0,1}};}

inline OriginalModelCoords ComposeOriginalModelCoords(OriginalModelCoords lhs,const OriginalModelCoords& rhs) {
    // Native FCoords*=FCoords updates Origin, then X/Y/Z vectors, with final
    // XYZ Float copies before each next vector. No host matrix or inverse.
    lhs.origin=TransformOriginalPoint(lhs.origin,rhs);
    lhs.xAxis=TransformOriginalVector(lhs.xAxis,rhs);
    lhs.yAxis=TransformOriginalVector(lhs.yAxis,rhs);
    lhs.zAxis=TransformOriginalVector(lhs.zAxis,rhs);
    return lhs;
}

namespace BrushCoordsDetail {
using namespace ModelOwnerCoordsDetail;
using NativeWindowClipDetail::Compare64;
using ScriptRandom::VectorDetail::DecodeDouble;
inline Dyadic Snap(float value) {
    const auto rate=Load(value);
    const auto constant=[](std::uint64_t bits) {return DecodeDouble(std::bit_cast<double>(bits));};
    const auto low65=constant(0xbfe4cccccccccccdull),high65=constant(0x3fe4cccccccccccdull),
        low55=constant(0xbfe199999999999aull),high55=constant(0x3fe199999999999aull),
        low05=constant(0xbfa999999999999aull),high05=constant(0x3fa999999999999aull),
        adjust15=constant(0x3fc3333333333333ull);
    // Engine1030F240 and Core1011C0B5 use DOUBLE thresholds and retain the
    // selected addition until the shear matrix's one Float store.
    if(Compare64(rate,low65)<0) return Add64(rate,adjust15);
    if(Compare64(rate,high65)>0) return Sub(rate,adjust15);
    if(Compare64(rate,low55)<0) return Decode(-0.5f);
    if(Compare64(rate,high55)>0) return Decode(0.5f);
    if(Compare64(rate,low05)<0) return Add64(rate,high05);
    if(Compare64(rate,high05)>0) return Sub(rate,high05);
    return Decode(0.0f);
}
inline PortableModelVec3 Times(PortableModelVec3 value,PortableModelVec3 scale) {
    return {Store32(Product(value.x,scale.x)),Store32(Product(value.y,scale.y)),Store32(Product(value.z,scale.z))};
}
inline PortableModelVec3 Divide(PortableModelVec3 value,PortableModelVec3 scale) {
    const auto component=[](float numerator,float denominator) {
        const auto a=Load(numerator),b=Load(denominator);
        if(!b.significand) throw std::invalid_argument("Original brush scale reaches active division by zero");
        return Store32(ModelFastLineDetail::Divide64(a,b));
    };
    return {component(value.x,scale.x),component(value.y,scale.y),component(value.z,scale.z)};
}
inline OriginalModelCoords Translate(PortableModelVec3 location) {
    auto coords=IdentityOriginalModelCoords();
    // UnitCoords Origin-Location stores, not unary minus (signed-zero differs).
    coords.origin={Store32(Sub(Decode(0.0f),Load(location.x))),Store32(Sub(Decode(0.0f),Load(location.y))),
        Store32(Sub(Decode(0.0f),Load(location.z)))};
    return coords;
}
inline float Table(OriginalBrushSinTable table,std::int32_t angle,std::uint32_t offset=0u) {
    // SAR2 followed by AND3FFF is exactly low16(unsigned(angle+offset))>>2.
    // Unsigned addition avoids C++ signed overflow at native wrapping angles.
    const auto bits=std::bit_cast<std::uint32_t>(angle)+offset;
    const auto value=table[(bits>>2u)&0x3fffu];static_cast<void>(Load(value));return value;
}
inline OriginalModelCoords Rotate(OriginalModelCoords coords,OriginalBrushRotation rotation,OriginalBrushSinTable table) {
    // Caller supplies the genuine original 16384-Float lookup VALUE. This
    // header does not replace x87 FSIN table initialization with std::sin.
    const float sy=Table(table,rotation.yaw),cy=Table(table,rotation.yaw,0x4000u);
    coords=ComposeOriginalModelCoords(coords,{{},{cy,sy,0},{-sy,cy,0},{0,0,1}});
    const float sp=Table(table,rotation.pitch),cp=Table(table,rotation.pitch,0x4000u);
    coords=ComposeOriginalModelCoords(coords,{{},{cp,0,sp},{0,1,0},{-sp,0,cp}});
    const float sr=Table(table,rotation.roll),cr=Table(table,rotation.roll,0x4000u);
    return ComposeOriginalModelCoords(coords,{{},{1,0,0},{0,cr,-sr},{0,sr,cr}});
}
} // namespace BrushCoordsDetail

// The complete native FCoords*=FRotator value operation, including Origin and
// all three yaw/pitch/roll stages. Unlike Actor.ToWorld this does NOT prepend
// a Location translation or manufacture identity UnitCoords for its caller.
inline OriginalModelCoords RotateOriginalModelCoords(OriginalModelCoords coords,OriginalBrushRotation rotation,
    OriginalBrushSinTable table) {
    return BrushCoordsDetail::Rotate(coords,rotation,table);
}

inline float SnapOriginalBrushSheer(float value) {return ActorTimerDetail::Store32(BrushCoordsDetail::Snap(value));}

inline OriginalModelCoords ScaleOriginalModelCoords(OriginalModelCoords coords,const OriginalBrushScale& value) {
    auto shear=IdentityOriginalModelCoords();
    if(value.sheerAxis>=1u&&value.sheerAxis<=6u) {
        const auto snapped=SnapOriginalBrushSheer(value.sheerRate);
        switch(value.sheerAxis) {
        case 1:shear.xAxis.y=snapped;break;case 2:shear.xAxis.z=snapped;break;
        case 3:shear.yAxis.x=snapped;break;case 4:shear.yAxis.z=snapped;break;
        case 5:shear.zAxis.x=snapped;break;case 6:shear.zAxis.y=snapped;break;
        }
    }
    // Invalid/none axis really uses UnitCoords, NOT an enum error. Native
    // computes/discards Snap there; omit its unused FP status, including NaNs.
    // Still execute full identity composition/products/divisions: no shortcut.
    coords=ComposeOriginalModelCoords(coords,shear);
    coords.xAxis=BrushCoordsDetail::Times(coords.xAxis,value.scale);
    coords.yAxis=BrushCoordsDetail::Times(coords.yAxis,value.scale);
    coords.zAxis=BrushCoordsDetail::Times(coords.zAxis,value.scale);
    coords.origin=BrushCoordsDetail::Divide(coords.origin,value.scale);
    return coords;
}

inline OriginalModelCoords BuildOriginalActorCoords(PortableModelVec3 location,OriginalBrushRotation rotation,
    OriginalBrushSinTable table) {
    // Engine Actor.ToWorld1030DAB0..DE70 reads only Location and Rotation.
    return BrushCoordsDetail::Rotate(BrushCoordsDetail::Translate(location),rotation,table);
}

inline OriginalModelCoords BuildOriginalBrushCoords(PortableModelVec3 location,OriginalBrushRotation rotation,
    PortableModelVec3 prePivot,const OriginalBrushScale& scale340,const OriginalBrushScale& scale32c,
    OriginalBrushSinTable table) {
    using namespace ModelOwnerCoordsDetail;
    // Engine ABrush.ToWorld1030F310..F4F6 first FCHS/Float-stores PrePivot.
    const PortableModelVec3 negativePivot{Store32(Negate(Load(prePivot.x))),Store32(Negate(Load(prePivot.y))),
        Store32(Negate(Load(prePivot.z)))};
    auto coords=ScaleOriginalModelCoords(BrushCoordsDetail::Translate(location),scale340);
    coords=BrushCoordsDetail::Rotate(coords,rotation,table);
    coords=ScaleOriginalModelCoords(coords,scale32c);
    // Subtract the stored negative pivot, not a regrouped Location expression.
    coords.origin={Store32(Sub(Load(coords.origin.x),Decode(negativePivot.x))),
        Store32(Sub(Load(coords.origin.y),Decode(negativePivot.y))),Store32(Sub(Load(coords.origin.z),Decode(negativePivot.z)))};
    return coords;
}
} // namespace QuestVr
