#pragma once

#include "quest_model_extent_clip.h"

#include <optional>

namespace QuestVr {

// Detached native UPrimitive.LineCheck data, not a reflected UObject layout.
// Identity is an opaque non-null owner token, never a persistent actor index.
struct OriginalActorCylinder {
    std::uintptr_t identity{};
    PortableModelVec3 location{};
    float radius{},height{};
};
struct OriginalActorLineHit {
    std::uintptr_t next{},actor{},primitive{};
    PortableModelVec3 location{},normal{};
    float time{1.0f};
    std::int32_t item{-1};
};

namespace ActorLineTraceDetail {
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

inline constexpr double Threshold=std::bit_cast<double>(std::uint64_t{0x3e45798ee2308c3aull});
inline constexpr double Inward=std::bit_cast<double>(std::uint64_t{0xbfb999999999999aull});
inline constexpr double Backoff=std::bit_cast<double>(std::uint64_t{0x3f50624dd2f1a9fcull});
inline float Read(float value) {
    if(!std::isfinite(value)) throw std::invalid_argument("Original actor line trace requires finite active Float data");
    return value;
}
inline Dyadic Load(float value) {return Decode(Read(value));}
inline Dyadic Dimension(float value) {
    Read(value);
    if(value<0.0f) throw std::invalid_argument("Original actor line trace refuses negative active collision dimensions");
    return Decode(value);
}
inline Dyadic Sub(const Dyadic& a,const Dyadic& b) {return Add64(a,Negate(b));}
inline Dyadic Product(float a,float b) {return Multiply64(Load(a),Load(b));}
inline Dyadic Squared(const PortableModelVec3& value) {
    return Add64(Add64(Product(value.x,value.x),Product(value.y,value.y)),Product(value.z,value.z));
}
inline Dyadic Root(double value) {
    const auto number=DecodeDouble(value);
    // FSQRT accepts either signed zero; the shared reciprocal-root helper
    // intentionally rejects zero, so preserve this separate original branch.
    return number.significand?SqrtStoredDouble64(value):number;
}
inline PortableModelVec3 Normal(const PortableModelVec3& value,bool safe) {
    const auto sum=Squared(value);
    const double stored=StoreDouble(sum);
    if(Compare64(sum,DecodeDouble(Threshold))<0) return safe?PortableModelVec3{}:value;
    auto root=Root(stored);
    // The inlined radial normal stores/reloads the ROOT as Double. The
    // imported Core.SafeNormal keeps it in PC64 instead (10144BE0).
    if(!safe) root=DecodeDouble(StoreDouble(root));
    const auto inverse=ModelFastLineDetail::Divide64(Decode(1.0f),root);
    return {Store32(Multiply64(Load(value.x),inverse)),Store32(Multiply64(Load(value.y),inverse)),
        Store32(Multiply64(Load(value.z),inverse))};
}

inline bool Check(OriginalActorLineHit& hit,const OriginalActorCylinder& owner,
    const PortableModelVec3& end,const PortableModelVec3& start,const PortableModelVec3& extent) {
    if(!owner.identity) throw std::invalid_argument("Original actor line trace owner identity is null");
    // Match the original active-read order; in particular do not validate all
    // endpoints, owner coordinates or seeded hit fields before early returns.
    const float height=Store32(Add64(Dimension(extent.z),Dimension(owner.height)));
    const float radiusY=Store32(Add64(Dimension(extent.y),Dimension(owner.radius)));
    const auto retainedRadiusX=Add64(Dimension(extent.x),Dimension(owner.radius));
    const float radiusX=Store32(retainedRadiusX);
    const auto aboveX=Add64(retainedRadiusX,Load(owner.location.x));
    if(Compare64(Load(start.x),aboveX)>0&&Compare64(Load(end.x),aboveX)>0) return true;
    const auto belowX=Sub(Load(owner.location.x),Decode(radiusX));
    if(Compare64(Load(start.x),belowX)<0&&Compare64(Load(end.x),belowX)<0) return true;
    const auto aboveY=Add64(Decode(radiusY),Load(owner.location.y));
    if(Compare64(Load(start.y),aboveY)>0&&Compare64(Load(end.y),aboveY)>0) return true;
    const auto belowY=Sub(Load(owner.location.y),Decode(radiusY));
    if(Compare64(Load(start.y),belowY)<0&&Compare64(Load(end.y),belowY)<0) return true;
    const float upper=Store32(Add64(Decode(height),Load(owner.location.z)));
    if(Read(start.z)>upper&&Read(end.z)>upper) return true;
    const float lower=Store32(Sub(Load(owner.location.z),Decode(height)));
    if(Read(start.z)<lower&&Read(end.z)<lower) return true;

    float enter=0.0f,exit=1.0f;
    const auto capTime=[&](float z) {return ModelFastLineDetail::Divide64(Sub(Decode(z),Load(start.z)),Sub(Load(end.z),Load(start.z)));};
    if(Read(start.z)>upper&&Read(end.z)<upper) {
        const auto time=capTime(upper);
        if(Compare64(time,Decode(0.0f))>0) {enter=Store32(time);hit.normal={0,0,1};}
    } else if(Read(start.z)<upper&&Read(end.z)>upper) {
        const auto time=capTime(upper);
        if(Compare64(Decode(1.0f),time)>0) exit=Store32(time);
    }
    if(Read(start.z)<lower&&Read(end.z)>lower) {
        const auto time=capTime(lower);
        if(Compare64(time,Decode(enter))>0) {enter=Store32(time);hit.normal={0,0,-1};}
    } else if(Read(start.z)>lower&&Read(end.z)<lower) {
        const auto time=capTime(lower);
        if(Compare64(Decode(exit),time)>0) exit=Store32(time);
    }
    if(enter>=exit) return true;

    const auto px=Sub(Load(start.x),Load(owner.location.x)),py=Sub(Load(start.y),Load(owner.location.y));
    const auto dx=Sub(Load(end.x),Load(start.x)),dy=Sub(Load(end.y),Load(start.y));
    const float storedDx=Store32(dx),storedDy=Store32(dy);
    const float a=Store32(Add64(Multiply64(dy,dy),Multiply64(dx,dx)));
    const float b=Store32(Multiply64(Decode(2.0f),Add64(Multiply64(dy,py),Multiply64(dx,px))));
    const float c=Store32(Sub(Add64(Multiply64(py,py),Multiply64(px,px)),Product(radiusX,radiusX)));
    const float discriminant=Store32(Sub(Product(b,b),Multiply64(Product(c,a),Decode(4.0f))));
    if(Compare64(Decode(c),Decode(1.0f))<0&&Read(start.z)>lower&&Read(start.z)<upper) {
        const auto pz=Sub(Load(start.z),Load(owner.location.z));
        // Retail computes an overwritten Float zDelta here. Its finite store
        // can still overflow before the dot, even though the dot uses +0.
        static_cast<void>(Store32(Sub(Load(end.z),Load(start.z))));
        const auto dot=Add64(Add64(Multiply64(Decode(storedDx),px),Multiply64(Decode(storedDy),py)),
            Multiply64(Decode(0.0f),pz));
        if(Compare64(dot,DecodeDouble(Inward))>=0) return true;
        hit.time=0.0f;hit.location=start;
        static_cast<void>(Store32(pz)); // Original overwritten Float scratch at 103D8F7C.
        hit.normal=Normal({Store32(px),Store32(py),0.0f},true);
        hit.actor=owner.identity;hit.primitive=0;
        return false;
    }
    if(discriminant<0.0f) return true;
    if(Compare64(Decode(a),DecodeDouble(Threshold))<0) {
        if(c>0.0f) return true;
    } else {
        const auto root=Root(static_cast<double>(discriminant));
        // FST Double root at 103D9045 does not pop or reload the PC64 root.
        static_cast<void>(StoreDouble(root));
        const auto inverse=ModelFastLineDetail::Divide64(DecodeDouble(0.5),Decode(a));
        const auto exitRoot=Multiply64(Sub(root,Decode(b)),inverse);
        if(Compare64(Decode(exit),exitRoot)>0) exit=Store32(exitRoot);
        const auto entryRoot=Multiply64(Negate(Add64(root,Decode(b))),inverse);
        if(Compare64(entryRoot,Decode(enter))>0) {
            enter=Store32(entryRoot); // Keep retained root for point/normal math.
            const float xOffset=Store32(Multiply64(Decode(storedDx),entryRoot)),
                yOffset=Store32(Multiply64(Decode(storedDy),entryRoot));
            const float x=Store32(Add64(Decode(xOffset),Load(start.x))),
                y=Store32(Add64(Decode(yOffset),Load(start.y)));
            const auto z=Add64(Multiply64(Sub(Load(end.z),Load(start.z)),entryRoot),Load(start.z));
            hit.normal={Store32(Sub(Decode(x),Load(owner.location.x))),
                Store32(Sub(Decode(y),Load(owner.location.y))),Store32(Sub(z,Load(owner.location.z)))};
            hit.normal.z=0.0f;
            hit.normal=Normal(hit.normal,false);
        }
    }
    if(enter>=exit) return true;
    auto time=Sub(Decode(enter),DecodeDouble(Backoff));
    if(Compare64(time,Decode(0.0f))<0) time=Decode(0.0f);
    else if(Compare64(time,Decode(1.0f))>=0) time=Decode(1.0f);
    hit.time=Store32(time); // Retained time, not hit.time, feeds location.
    const float finalDy=Store32(Sub(Load(end.y),Load(start.y))),
        finalDz=Store32(Sub(Load(end.z),Load(start.z)));
    const float xOffset=Store32(Multiply64(Decode(storedDx),time)),
        yOffset=Store32(Multiply64(Decode(finalDy),time));
    const auto zOffset=Multiply64(Decode(finalDz),time);
    hit.location={Store32(Add64(Decode(xOffset),Load(start.x))),Store32(Add64(Decode(yOffset),Load(start.y))),
        Store32(Add64(zOffset,Load(start.z)))};
    hit.actor=owner.identity;hit.primitive=0;
    return false;
}
} // namespace ActorLineTraceDetail

// True means native clear; false means blocked. Caller Time never bounds the
// query. Genuine clear returns retain partial Normal writes; every refusal
// leaves the entire caller hit unchanged, and a future native adapter must
// propagate refusal into its SAME root VM transaction. No callbacks/hash here.
inline bool CheckOriginalActorCylinder(OriginalActorLineHit& hit,
    const std::optional<OriginalActorCylinder>& owner,const PortableModelVec3& end,
    const PortableModelVec3& start,const PortableModelVec3& extent={}) {
    if(!owner) return true;
    auto working=hit;
    const bool clear=ActorLineTraceDetail::Check(working,*owner,end,start,extent);
    hit=working;
    return clear;
}

} // namespace QuestVr
