#pragma once

#include "quest_actor_line_trace.h"
#include "quest_native_scalar.h"

#include <bit>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {
struct OriginalPawnFallingSetupQueries {
    std::function<float(std::uintptr_t)> airControl,accelRate,groundSpeed,radius,height;
    std::function<float(std::uintptr_t,unsigned)> acceleration,velocity,location;
    // Actual imported Core.SafeNormal on a copied FVector. No world effects
    // are fabricated by normalization; an unsupported numeric case refuses.
    std::function<PortableModelVec3(PortableModelVec3)> safeNormal;
    std::function<std::uintptr_t(std::uintptr_t)> level;
    std::function<std::int32_t(std::uintptr_t,OriginalActorLineHit&,std::uintptr_t,
        PortableModelVec3,PortableModelVec3,std::uint32_t,PortableModelVec3,std::int32_t)> singleLineCheck;
    std::function<void(std::uintptr_t,unsigned,std::uint32_t)> writeAccelerationWord,writeVelocityWord;
    std::function<void()> work;
};
struct OriginalPawnFallingSetupStats {
    std::size_t steps{},traceCalls{},normalCalls{},accelerationWords{};
    float control{},limit{},horizontalSpeedCap{};
    bool predictedHit{},lowSpeed{},clampedAcceleration{};
};
struct OriginalPawnFallingVelocityCapStats {
    std::size_t steps{},velocityWords{};
    bool clamped{};
};
namespace PawnFallingSetupDetail {
template<class Stats,class Provider,class... Args>
inline auto Call(const OriginalPawnFallingSetupQueries& q,Stats& out,const Provider& provider,Args&&... args) {
    if(!provider)throw std::invalid_argument("Original Pawn falling setup lacks a reached provider");
    if(!q.work)throw std::invalid_argument("Original Pawn falling setup lacks shared work authority");
    q.work();++out.steps;return provider(std::forward<Args>(args)...);
}
inline float Channel(PortableModelVec3 v,unsigned i){return i==0u?v.x:i==1u?v.y:v.z;}
inline void Channel(PortableModelVec3& v,unsigned i,float value){if(i==0u)v.x=value;else if(i==1u)v.y=value;else v.z=value;}
} // namespace PawnFallingSetupDetail

// Original physFalling103D0BD0..103D0E0C plus its high-speed detour103D0EA0.
// Caller already captured actual Pawn and original Acceleration. The returned
// local speed cap must survive ALL substeps; it is not the acceleration limit.
inline OriginalPawnFallingSetupStats SetupOriginalPawnFalling(std::uintptr_t actor,float wholeDelta,
    const OriginalPawnFallingSetupQueries& q) {
    if(!actor)throw std::invalid_argument("Original Pawn falling setup requires a nonnull Pawn");
    using namespace NativeScalar;
    using PawnFallingSetupDetail::Call;
    using PawnFallingSetupDetail::Channel;
    OriginalPawnFallingSetupStats out;
    const auto read=[&](const auto& provider,auto... args){return Call(q,out,provider,args...);};
    const auto acceleration=[&] {
        PortableModelVec3 v;for(unsigned i=3u;i>0u;--i)Channel(v,i-1u,read(q.acceleration,actor,i-1u));return v;
    };
    const auto normalize=[&](PortableModelVec3 v){++out.normalCalls;return Call(q,out,q.safeNormal,v);};
    const auto write=[&](unsigned axis,float value){++out.accelerationWords;
        Call(q,out,q.writeAccelerationWord,actor,axis,std::bit_cast<std::uint32_t>(value));};
    out.control=read(q.airControl,actor); // MOV preserves raw Float bits.
    if(Compare(177u,Load(out.control),Memory(0.15f))) {
        const float predictiveLimit=StoreFloat(Multiply(Load(out.control),Memory(read(q.accelRate,actor))));
        const auto direction=normalize(acceleration());
        const auto yProduct=Multiply(Load(predictiveLimit),Memory(direction.y));
        const float xProduct=StoreFloat(Multiply(Load(predictiveLimit),Memory(direction.x)));
        const auto ySum=Add(yProduct,Memory(read(q.velocity,actor,1u)));
        const float xSum=StoreFloat(Add(Load(xProduct),Memory(read(q.velocity,actor,0u))));
        const float moveY=StoreFloat(Multiply(ySum,Memory(wholeDelta)));
        const float moveX=StoreFloat(Multiply(Load(xSum),Memory(wholeDelta)));
        const float radiusRaw=read(q.radius,actor),heightRaw=read(q.height,actor);
        const float radiusLoaded=StoreFloat(Load(read(q.radius,actor)));
        const auto endZ=Load(read(q.location,actor,2u));
        const auto firstLevel=read(q.level,actor);
        if(!firstLevel)throw std::invalid_argument("Original Pawn falling prediction has null native Level");
        const auto endY=Add(Load(moveY),Memory(read(q.location,actor,1u)));
        const auto endX=Add(Load(moveX),Memory(read(q.location,actor,0u)));
        const PortableModelVec3 end{StoreFloat(endX),StoreFloat(endY),StoreFloat(endZ)};
        PortableModelVec3 start;
        for(unsigned i=0u;i<3u;++i)Channel(start,i,read(q.location,actor,i));
        const auto level=read(q.level,actor);
        if(!level)throw std::invalid_argument("Original Pawn falling prediction has null call Level");
        OriginalActorLineHit hit;++out.traceCalls;
        static_cast<void>(Call(q,out,q.singleLineCheck,level,hit,actor,end,start,std::uint32_t{6u},
            PortableModelVec3{radiusLoaded,radiusRaw,heightRaw},std::int32_t{}));
        if(hit.actor){out.control=0.05f;out.predictedHit=true;} // Ignore native INT result.
    }
    const auto heldLimit=Multiply(Load(out.control),Memory(read(q.accelRate,actor)));
    write(2u,0.0f);
    const float vx=read(q.velocity,actor,0u);
    out.limit=StoreFloat(heldLimit);
    const float vy=read(q.velocity,actor,1u);
    const auto ySquared=Multiply(Load(vy),Memory(vy));
    const auto xSquared=Multiply(Load(vx),Memory(vx));
    static_cast<void>(read(q.velocity,actor,2u)); // Required raw copy, not used in horizontal magnitude.
    const auto speed=Sqrt(StoreDouble(Add(ySquared,xSquared)));
    if(Compare(176u,speed,Memory(10.0))) {
        out.lowSpeed=true;
        out.limit=StoreFloat(Add(Divide(Subtract(Load(10.0f),speed),Memory(wholeDelta)),Memory(out.limit)));
    } else if(!Compare(176u,speed,Memory(read(q.groundSpeed,actor)))) {
        if(Compare(177u,Load(out.control),Memory(0.05f)))out.horizontalSpeedCap=StoreFloat(speed);
        else out.limit=1.0f;
    }
    const auto current=acceleration();
    const auto magnitude=Add(Add(Multiply(Load(current.x),Load(current.x)),Multiply(Load(current.y),Load(current.y))),
        Multiply(Load(current.z),Load(current.z)));
    if(Compare(176u,Multiply(Load(out.limit),Memory(out.limit)),magnitude)) {
        const auto direction=normalize(acceleration());
        // Native copy of normalized XYZ precedes reads/products of the
        // just-stored fields. This is not one final vector assignment.
        for(unsigned i=0u;i<3u;++i)write(i,Channel(direction,i));
        const auto zProduct=Multiply(Load(out.limit),Memory(read(q.acceleration,actor,2u)));
        const auto yProduct=Multiply(Load(out.limit),Memory(read(q.acceleration,actor,1u)));
        const float x=StoreFloat(Multiply(Load(out.limit),Memory(read(q.acceleration,actor,0u))));
        const float y=StoreFloat(yProduct);write(0u,x);
        const float z=StoreFloat(zProduct);write(1u,y);write(2u,z);
        out.clampedAcceleration=true;
    }
    return out;
}

// Original retained horizontal speed-cap block103D1A7B..103D1B39. It occurs
// after provisional velocity/apex correction and BEFORE drift/swept movement.
inline OriginalPawnFallingVelocityCapStats CapOriginalPawnFallingVelocity(std::uintptr_t actor,float cap,
    const OriginalPawnFallingSetupQueries& q) {
    if(!actor)throw std::invalid_argument("Original Pawn velocity cap requires a nonnull Pawn");
    using namespace NativeScalar;
    using PawnFallingSetupDetail::Call;
    using PawnFallingSetupDetail::Channel;
    OriginalPawnFallingVelocityCapStats out;
    // C3 test: unordered NaN skips just like either signed zero.
    if(!Compare(181u,Load(cap),Memory(0.0f)))return out;
    const float x=Call(q,out,q.velocity,actor,0u),y=Call(q,out,q.velocity,actor,1u);
    static_cast<void>(Call(q,out,q.velocity,actor,2u));
    const auto magnitude=Add(Multiply(Load(y),Memory(y)),Multiply(Load(x),Memory(x)));
    if(!Compare(176u,Multiply(Load(cap),Memory(cap)),magnitude))return out;
    const auto direction=Call(q,out,q.safeNormal,PortableModelVec3{x,y,0.0f});
    const float scaledZ=StoreFloat(Multiply(Load(direction.z),Memory(cap)));
    static_cast<void>(scaledZ); // Original inactive scratch is still evaluated/spilled.
    const float scaledY=StoreFloat(Multiply(Load(direction.y),Memory(cap)));
    const float scaledX=StoreFloat(Multiply(Load(direction.x),Memory(cap)));
    const float freshZ=Call(q,out,q.velocity,actor,2u);
    const PortableModelVec3 value{scaledX,scaledY,freshZ};
    for(unsigned i=0u;i<3u;++i){++out.velocityWords;
        Call(q,out,q.writeVelocityWord,actor,i,std::bit_cast<std::uint32_t>(Channel(value,i)));}
    out.clamped=true;return out;
}
} // namespace QuestVr
