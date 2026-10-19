#pragma once

#include "quest_two_wall_adjust.h"

#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

struct OriginalActorSmoothMove {
    // Retail selects the virtual slot from dispatchLevel, then rereads this
    // Level for calls1/2. Call3 uses one Level read for both. The real adapter
    // must authenticate the immutable native dispatch class and generation.
    std::uintptr_t dispatchLevel{},level{},actor{};
    PortableModelVec3 delta{};
    std::array<std::int32_t,3u> rotation{};
    // bTest, bIgnorePawns, bIgnoreBases, bNoFail: always literal0 here.
    std::array<std::int32_t,4u> options{};
};
struct OriginalActorSmoothQueries {
    std::function<std::int32_t(std::uintptr_t,unsigned)> rotationChannel;
    std::function<std::uintptr_t(std::uintptr_t)> xLevel;
    // Delta/Rotation/options are copies, as in the native by-value arguments.
    // The SAME live caller packet is reused, not copied/reset between calls.
    std::function<std::int32_t(OriginalActorSmoothMove,OriginalActorLineHit&)> moveActor;
    std::function<void(std::uintptr_t,PortableModelVec3,std::uintptr_t)> hitWall;
    std::function<void()> work;
};
struct OriginalActorSlide {
    PortableModelVec3 delta{};
    bool forward{};
};

namespace ActorMoveSmoothDetail {
using ActorLineTraceDetail::Load;
using ActorLineTraceDetail::Product;
using ActorLineTraceDetail::Sub;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;

template<class Function,class... Arguments>
inline auto Read(const Function& function,Arguments... arguments) {
    if(!function)throw std::invalid_argument("Original moveSmooth lacks an active provider");
    return function(arguments...);
}
inline void Work(const OriginalActorSmoothQueries& queries) {if(queries.work)queries.work();}
inline void RequireLevel(std::uintptr_t level) {
    if(!level)throw std::invalid_argument("Original moveSmooth requires its nonnull actual XLevel");
}
// Actual component/raw-pointer load ordering. There is no retail callback
// between these loads. The split provider nevertheless makes their ordering
// explicit instead of inventing an eagerly captured pose/Level snapshot.
inline OriginalActorSmoothMove Sample(std::uintptr_t actor,const PortableModelVec3& delta,
    unsigned attempt,const OriginalActorSmoothQueries& queries) {
    Work(queries);OriginalActorSmoothMove call;call.actor=actor;call.delta=delta;
    if(attempt==2u) {
        call.dispatchLevel=Read(queries.xLevel,actor);
        call.rotation[0u]=Read(queries.rotationChannel,actor,0u);
        RequireLevel(call.dispatchLevel);
    }else {
        call.rotation[0u]=Read(queries.rotationChannel,actor,0u);
        call.dispatchLevel=Read(queries.xLevel,actor);
        if(attempt==3u)RequireLevel(call.dispatchLevel);
    }
    call.rotation[1u]=Read(queries.rotationChannel,actor,1u);
    call.rotation[2u]=Read(queries.rotationChannel,actor,2u);
    if(attempt==1u)RequireLevel(call.dispatchLevel);
    call.level=attempt==3u?call.dispatchLevel:Read(queries.xLevel,actor);
    RequireLevel(call.level);
    return call;
}
inline std::int32_t Move(OriginalActorSmoothMove call,OriginalActorLineHit& hit,
    const OriginalActorSmoothQueries& queries) {
    if(!queries.moveActor)throw std::invalid_argument("Original moveSmooth lacks its complete MoveActor provider");
    return queries.moveActor(std::move(call),hit);
}
// Original FCOMP/TEST C0: unordered Time continues too. It is only refused
// later if actually used in unsafe arithmetic, not before a repairable
// second-hit HitWall callback. Positive infinity is an ordered clear return.
inline bool Continues(float time) {
    const auto bits=std::bit_cast<std::uint32_t>(time);
    const bool nan=(bits&0x7f800000u)==0x7f800000u&&(bits&0x7fffffu)!=0u;
    return nan||time<1.0f;
}
} // namespace ActorMoveSmoothDetail

// Complete first-slide numeric schedule103C90E4..9172. Finite active operands
// and finite Float stores are the portable admission domain; no time clamp or
// normal-unit assumption is introduced. The gate uses RETAINED slideX, even
// though the next MoveActor receives its separately rounded Float word.
inline OriginalActorSlide ProjectOriginalActorSlide(const PortableModelVec3& delta,
    const PortableModelVec3& normal,float hitTime) {
    using namespace ActorMoveSmoothDetail;
    const auto dot=Add64(Add64(Product(delta.z,normal.z),Product(delta.y,normal.y)),Product(normal.x,delta.x));
    const float projectedZ=Store32(Multiply64(Load(normal.z),dot)),
        projectedY=Store32(Multiply64(Load(normal.y),dot));
    const auto projectedX=Multiply64(Load(normal.x),dot);
    const float rejectedZ=Store32(Sub(Load(delta.z),Decode(projectedZ))),
        rejectedY=Store32(Sub(Load(delta.y),Decode(projectedY)));
    const auto rejectedX=Sub(Load(delta.x),projectedX);
    const auto remaining=Sub(Decode(1.0f),Load(hitTime));
    const float z=Store32(Multiply64(Decode(rejectedZ),remaining)),
        y=Store32(Multiply64(Decode(rejectedY),remaining));
    const auto retainedX=Multiply64(rejectedX,remaining);const float x=Store32(retainedX);
    const auto gate=Add64(Add64(Product(delta.y,y),Product(delta.z,z)),Multiply64(retainedX,Load(delta.x)));
    return {{x,y,z},Compare64(gate,Decode(0.0f))>=0};
}

// Complete AActor.moveSmooth103C9020..92AA protocol. This does NOT substitute
// for MoveActor266/299: a real caller must supply its COMPLETE collision-aware
// movement/dispatch provider and the SAME outer work/hash/VM transaction.
// Provider refusals propagate. Roll back that root on errors, not on an
// ordinary native integer result, which may preserve earlier movement/events.
inline std::int32_t MoveOriginalActorSmooth(std::uintptr_t actor,PortableModelVec3 delta,
    const OriginalActorSmoothQueries& queries) {
    if(!actor)throw std::invalid_argument("Original moveSmooth requires a nonnull source");
    using namespace ActorMoveSmoothDetail;
    OriginalActorLineHit hit;
    const auto first=Move(Sample(actor,delta,1u,queries),hit,queries);
    if(!Continues(hit.time))return first;
    auto slide=ProjectOriginalActorSlide(delta,hit.normal,hit.time);
    if(!slide.forward)return first;
    const auto oldNormal=hit.normal;
    const auto desired=ActorLineTraceDetail::Normal(delta,true);
    static_cast<void>(Move(Sample(actor,slide.delta,2u,queries),hit,queries));
    if(!Continues(hit.time))return first;
    Work(queries);
    if(!queries.hitWall)throw std::invalid_argument("Original moveSmooth lacks its active HitWall dispatcher");
    queries.hitWall(actor,hit.normal,hit.actor); // RAW event-parameter copies.
    AdjustOriginalActorTwoWalls(desired,slide.delta,hit.normal,oldNormal,hit.time);
    static_cast<void>(Move(Sample(actor,slide.delta,3u,queries),hit,queries));
    return first; // Never normalize or substitute the last movement's result.
}

} // namespace QuestVr
