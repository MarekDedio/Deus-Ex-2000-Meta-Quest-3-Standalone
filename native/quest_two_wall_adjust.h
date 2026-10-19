#pragma once

#include "quest_actor_line_trace.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace QuestVr {
namespace ActorTwoWallDetail {
using ActorLineTraceDetail::Add64;
using ActorLineTraceDetail::Compare64;
using ActorLineTraceDetail::Decode;
using ActorLineTraceDetail::DecodeDouble;
using ActorLineTraceDetail::Dyadic;
using ActorLineTraceDetail::Load;
using ActorLineTraceDetail::Multiply64;
using ActorLineTraceDetail::Store32;
using ActorLineTraceDetail::Sub;

inline Dyadic DesiredDot(const PortableModelVec3& desired,const PortableModelVec3& delta) {
    // This read occurs AFTER all three Delta stores. Desired may be the SAME
    // FVector object as Delta (or share that alias group with either normal).
    const auto z=Multiply64(Load(desired.z),Load(delta.z));
    const auto y=Multiply64(Load(desired.y),Load(delta.y));
    const auto zy=Add64(z,y);
    const auto x=Multiply64(Load(desired.x),Load(delta.x));
    return Add64(zy,x);
}

inline void Adjust(const PortableModelVec3& desired,PortableModelVec3& delta,
    const PortableModelVec3& hitNormal,const PortableModelVec3& oldNormal,float hitTime) {
    // Original10323793..7B2: (Ny*Oy + Oz*Nz) + Nx*Ox, retained PC64.
    const auto normalY=Multiply64(Load(hitNormal.y),Load(oldNormal.y));
    const auto normalZ=Multiply64(Load(oldNormal.z),Load(hitNormal.z));
    const auto normalYZ=Add64(normalY,normalZ);
    const auto normalX=Multiply64(Load(hitNormal.x),Load(oldNormal.x));
    const auto normalDot=Add64(normalYZ,normalX);
    if(Compare64(normalDot,Decode(0.0f))<=0) {
        // Cross products are computed Z,Y,X, then Float-stored X,Y,Z before
        // calling imported Core.SafeNormal. No original-reference writes yet.
        const auto crossZ=Sub(Multiply64(Load(hitNormal.x),Load(oldNormal.y)),
            Multiply64(Load(hitNormal.y),Load(oldNormal.x)));
        const auto crossY=Sub(Multiply64(Load(oldNormal.x),Load(hitNormal.z)),
            Multiply64(Load(hitNormal.x),Load(oldNormal.z)));
        const auto crossX=Sub(Multiply64(Load(hitNormal.y),Load(oldNormal.z)),
            Multiply64(Load(hitNormal.z),Load(oldNormal.y)));
        const PortableModelVec3 cross{Store32(crossX),Store32(crossY),Store32(crossZ)};
        const auto axis=ActorLineTraceDetail::Normal(cross,true);
        const auto projectionZ=Multiply64(Load(axis.z),Load(delta.z));
        const auto projectionY=Multiply64(Load(axis.y),Load(delta.y));
        const auto projectionZY=Add64(projectionZ,projectionY);
        const auto projectionX=Multiply64(Load(axis.x),Load(delta.x));
        const auto projection=Add64(projectionZY,projectionX);
        // Corner branch retains BOTH the one-minus-time and final scale.
        const auto remaining=Sub(DecodeDouble(1.0),Load(hitTime));
        const auto scale=Multiply64(projection,remaining);
        const float x=Store32(Multiply64(Load(axis.x),scale));
        const auto retainedY=Multiply64(Load(axis.y),scale);
        delta.x=x;
        const float y=Store32(retainedY);
        const auto retainedZ=Multiply64(Load(axis.z),scale);
        delta.y=y;delta.z=Store32(retainedZ);
        if(Compare64(DesiredDot(desired,delta),Decode(0.0f))<0) {
            // All old Delta components are read before any negated write.
            const float negativeX=Store32(Multiply64(Load(delta.x),Decode(-1.0f)));
            const float negativeY=Store32(Multiply64(Load(delta.y),Decode(-1.0f)));
            const auto negativeZ=Multiply64(Load(delta.z),Decode(-1.0f));
            delta.x=negativeX;delta.y=negativeY;delta.z=Store32(negativeZ);
        }
        return;
    }
    // Original103238CE..8E2: (Ny*Dy + Dx*Nx) + Dz*Nz, not the
    // corner projection's Z/Y/X association or a stored Float dot.
    const auto projectionY=Multiply64(Load(hitNormal.y),Load(delta.y));
    const auto projectionX=Multiply64(Load(delta.x),Load(hitNormal.x));
    const auto projectionYX=Add64(projectionY,projectionX);
    const auto projectionZ=Multiply64(Load(delta.z),Load(hitNormal.z));
    const auto projection=Add64(projectionYX,projectionZ);
    const float projectedNormalZ=Store32(Multiply64(projection,Load(hitNormal.z)));
    const float projectedNormalY=Store32(Multiply64(projection,Load(hitNormal.y)));
    const auto projectedNormalX=Multiply64(projection,Load(hitNormal.x));
    const float residualZ=Store32(Sub(Load(delta.z),Decode(projectedNormalZ)));
    const float residualY=Store32(Sub(Load(delta.y),Decode(projectedNormalY)));
    const float residualX=Store32(Sub(Load(delta.x),projectedNormalX));
    // This branch, unlike the corner branch, STORES the factor to Float.
    const float remaining=Store32(Sub(DecodeDouble(1.0),Load(hitTime)));
    const auto retainedZ=Multiply64(Decode(residualZ),Decode(remaining));
    const auto retainedY=Multiply64(Decode(residualY),Decode(remaining));
    const auto retainedX=Multiply64(Decode(residualX),Decode(remaining));
    const float x=Store32(retainedX),y=Store32(retainedY);
    delta.x=x;
    const float z=Store32(retainedZ);
    delta.y=y;delta.z=z;
    // Face branch requires STRICTLY forward. Ordered zero writes exact +0.
    if(Compare64(DesiredDot(desired,delta),Decode(0.0f))<=0)delta={};
}
} // namespace ActorTwoWallDetail

// Retail AActor.TwoWallAdjust10323770..103239A0, detached numeric adapter.
// All15 exact-reference alias partitions of its four Vector arguments are
// supported. The staged aliases retain writes visible to later native reads;
// snapshotting Desired independently would be wrong when it aliases Delta.
// Explicit portable safety: finite ACTIVE Float inputs, finite Float stores,
// and distinct valid objects or EXACT shared-object aliases. Unsafe partial
// byte overlaps refuse. Refusal leaves every original object bit-unchanged;
// success commits only Delta (and naturally any references aliasing Delta).
// No clamping of HitTime, normalization of wall normals, callbacks or host
// double-vector math is introduced. Enclosing movement owns its root budget.
inline void AdjustOriginalActorTwoWalls(const PortableModelVec3& desired,PortableModelVec3& delta,
    const PortableModelVec3& hitNormal,const PortableModelVec3& oldNormal,float hitTime) {
    const std::array<const PortableModelVec3*,4u> identities{&desired,&delta,&hitNormal,&oldNormal};
    std::array<PortableModelVec3,4u> slots;
    std::array<PortableModelVec3*,4u> views{};
    for(std::size_t i=0u;i<identities.size();++i) {
        for(std::size_t j=0u;j<i;++j) {
            if(identities[i]==identities[j]){views[i]=views[j];break;}
            const auto a=reinterpret_cast<std::uintptr_t>(identities[i]);
            const auto b=reinterpret_cast<std::uintptr_t>(identities[j]);
            if((a<b?b-a:a-b)<sizeof(PortableModelVec3))
                throw std::invalid_argument("Original TwoWallAdjust refuses partial Vector byte overlap");
        }
        if(!views[i]){slots[i]=*identities[i];views[i]=&slots[i];}
    }
    ActorTwoWallDetail::Adjust(*views[0u],*views[1u],*views[2u],*views[3u],hitTime);
    delta=*views[1u];
}
} // namespace QuestVr
