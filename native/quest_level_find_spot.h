#pragma once

#include "quest_actor_line_trace.h"

#include <bit>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

// Detached retail ULevel.FindSpot / AdjustSpot. All native collision providers
// must belong to the caller's SAME world, scratch lifetime and VM root. This
// kernel neither substitutes a candidate grid nor moves/registers an Actor.
struct OriginalLevelFindSpotQueries {
    std::function<std::int32_t(OriginalActorLineHit&,PortableModelVec3,PortableModelVec3,
        std::uint32_t,std::uintptr_t,std::int32_t)> pointCheck;
    std::function<std::int32_t(OriginalActorLineHit&,std::uintptr_t,PortableModelVec3,
        PortableModelVec3,PortableModelVec3,std::uint8_t,std::uint8_t)> lineCheck;
    // Fresh actual LevelInfo lookup/authentication for EVERY point-check site.
    // The final site uses retail GetLevelInfo; no cached first-query identity.
    std::function<std::uintptr_t()> freshLevelInfo;
    std::function<void()> work;
};

namespace LevelFindSpotDetail {
using ActorLineTraceDetail::Load;
using ActorLineTraceDetail::Sub;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using ScriptRandom::VectorDetail::DecodeDouble;
using ScriptRandom::VectorDetail::StoreDouble;

template<class Function,class... Arguments>
inline auto Call(const OriginalLevelFindSpotQueries& q,const Function& function,Arguments&&... arguments) {
    if(!function)throw std::invalid_argument("Original FindSpot lacks an active native provider");
    if(q.work)q.work();
    return function(std::forward<Arguments>(arguments)...);
}
inline bool Nan(float value) {
    const auto bits=std::bit_cast<std::uint32_t>(value);
    return (bits&0x7f800000u)==0x7f800000u&&(bits&0x7fffffu)!=0u;
}
inline bool ZeroOrUnordered(float value){return Nan(value)||value==0.0f;}
inline bool BelowOneOrUnordered(float value){return Nan(value)||value<1.0f;}
inline bool Point(OriginalActorLineHit& hit,PortableModelVec3 location,PortableModelVec3 extent,
    std::int32_t checkActors,const OriginalLevelFindSpotQueries& q) {
    const auto info=Call(q,q.freshLevelInfo);
    return Call(q,q.pointCheck,hit,location,extent,std::uint32_t{},info,checkActors)==1;
}
inline float Diagonal(PortableModelVec3 extent) {
    // Engine1039868F: retained (Z*Z + Y*Y) + X*X, Double store
    // for appSqrt, then retained PC64 root + Double2 -> Float radius.
    const auto z=Load(extent.z),y=Load(extent.y),x=Load(extent.x);
    const auto squared=Add64(Add64(Multiply64(z,z),Multiply64(y,y)),Multiply64(x,x));
    const auto root=ActorLineTraceDetail::Root(StoreDouble(squared));
    static_cast<void>(StoreDouble(root)); // FST scratch does NOT reload root.
    return Store32(Add64(root,DecodeDouble(2.0)));
}
inline float SquaredStored(PortableModelVec3 vector) {
    // Imported Core FVector.SizeSquared10117B70 uses (X*X + Y*Y) + Z*Z.
    const auto x=Load(vector.x),y=Load(vector.y),z=Load(vector.z);
    return Store32(Add64(Add64(Multiply64(x,x),Multiply64(y,y)),Multiply64(z,z)));
}
inline PortableModelVec3 AxisEndpoint(PortableModelVec3 candidate,PortableModelVec3 extent,
    unsigned axis,std::int32_t sign) {
    const auto multiplier=Decode(static_cast<float>(sign));
    if(axis==0u)candidate.x=Store32(Add64(Load(candidate.x),Decode(Store32(Multiply64(multiplier,Load(extent.x))))));
    else if(axis==1u)candidate.y=Store32(Add64(Load(candidate.y),Decode(Store32(Multiply64(multiplier,Load(extent.y))))));
    else candidate.z=Store32(Add64(Load(candidate.z),Multiply64(multiplier,Load(extent.z))));
    return candidate;
}
inline PortableModelVec3 CornerEndpoint(PortableModelVec3 candidate,PortableModelVec3 extent,
    std::int32_t x,std::int32_t y,std::int32_t z) {
    // Signs/products stay retained until each endpoint's Float constructor arg.
    return {Store32(Add64(Multiply64(Decode(static_cast<float>(x)),Load(extent.x)),Load(candidate.x))),
        Store32(Add64(Load(candidate.y),Multiply64(Decode(static_cast<float>(y)),Load(extent.y)))),
        Store32(Add64(Load(candidate.z),Multiply64(Decode(static_cast<float>(z)),Load(extent.z))))};
}
} // namespace LevelFindSpotDetail

// Engine10398360. The Bool from SingleLineCheck is deliberately ignored:
// only the shared caller Hit.Time gates adjustment. A clear provider must
// implement its authentic Hit writes, not rely on us to reset the packet.
inline void AdjustOriginalLevelSpot(OriginalActorLineHit& hit,PortableModelVec3& candidate,
    PortableModelVec3 endpoint,float radius,const OriginalLevelFindSpotQueries& q) {
    using namespace LevelFindSpotDetail;
    static_cast<void>(Call(q,q.lineCheck,hit,std::uintptr_t{},endpoint,candidate,
        PortableModelVec3{},std::uint8_t{6u},std::uint8_t{}));
    if(!BelowOneOrUnordered(hit.time))return;
    // Unordered enters retail arithmetic; active nonfinite arithmetic is an
    // explicit unsupported-domain refusal, never converted to a clear query.
    const auto factor=Sub(DecodeDouble(std::bit_cast<double>(std::uint64_t{0x3ff0cccccccccccdull})),Load(hit.time));
    const float xNormal=Store32(Multiply64(factor,Load(hit.normal.x))),
        yNormal=Store32(Multiply64(factor,Load(hit.normal.y)));
    const auto zNormal=Multiply64(factor,Load(hit.normal.z));
    const float xOffset=Store32(Multiply64(Decode(xNormal),Load(radius))),
        yOffset=Store32(Multiply64(Decode(yNormal),Load(radius)));
    const auto zOffset=Multiply64(zNormal,Load(radius));
    const PortableModelVec3 adjusted{Store32(Add64(Decode(xOffset),Load(candidate.x))),
        Store32(Add64(Decode(yOffset),Load(candidate.y))),Store32(Add64(zOffset,Load(candidate.z)))};
    candidate=adjusted;
}

// Engine10398480. CheckActors and AssumeFit are raw native INTs. No initial
// point check for nonzero extent when AssumeFit==0 (FarMove uses exactly0,0).
// Location commits ONLY on exact1 from an original point-check success site.
// Shared Hit is constructed once, reused across all six axes/eight corners,
// and never reset between providers. Provider exceptions require caller undo.
inline std::int32_t FindOriginalLevelSpot(PortableModelVec3 extent,PortableModelVec3& location,
    std::int32_t checkActors,std::int32_t assumeFit,const OriginalLevelFindSpotQueries& q) {
    using namespace LevelFindSpotDetail;
    OriginalActorLineHit hit;
    if(ZeroOrUnordered(extent.x)&&ZeroOrUnordered(extent.y)&&ZeroOrUnordered(extent.z))
        return Point(hit,location,extent,checkActors,q)?1:0;
    if(assumeFit&&Point(hit,location,extent,checkActors,q))return 1;
    auto candidate=location;
    const float diagonal=Diagonal(extent);
    for(const std::int32_t sign:{-1,1}) {
        AdjustOriginalLevelSpot(hit,candidate,AxisEndpoint(candidate,extent,0u,sign),extent.x,q);
        AdjustOriginalLevelSpot(hit,candidate,AxisEndpoint(candidate,extent,1u,sign),extent.y,q);
        AdjustOriginalLevelSpot(hit,candidate,AxisEndpoint(candidate,extent,2u,sign),extent.z,q);
    }
    if(Point(hit,candidate,extent,checkActors,q)){location=candidate;return 1;}
    for(const std::int32_t x:{-1,1})for(const std::int32_t y:{-1,1})for(const std::int32_t z:{-1,1})
        AdjustOriginalLevelSpot(hit,candidate,CornerEndpoint(candidate,extent,x,y,z),diagonal,q);
    // Both vector-difference constructor arguments and SizeSquared results
    // spill Float before comparison with Double1.5 * stored extent square.
    const PortableModelVec3 displacement{Store32(Sub(Load(candidate.x),Load(location.x))),
        Store32(Sub(Load(candidate.y),Load(location.y))),Store32(Sub(Load(candidate.z),Load(location.z)))};
    const float moved=SquaredStored(displacement),size=SquaredStored(extent);
    if(Compare64(Decode(moved),Multiply64(Decode(size),DecodeDouble(1.5)))>0)return 0;
    if(!Point(hit,candidate,extent,checkActors,q))return 0;
    location=candidate;return 1;
}
} // namespace QuestVr
