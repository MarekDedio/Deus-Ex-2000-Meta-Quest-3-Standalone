#pragma once

#include "quest_native_scalar.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

// APawn's actual ten-entry dispatch table103C9CDC, NOT AActor's table.
enum class OriginalPawnPhysicsDispatch : std::uint8_t {
    Walking=1u,Falling=2u,Swimming=3u,Flying=4u,Pathing=8u,Spider=10u
};
struct OriginalPawnPerformPhysicsQueries {
    std::function<float(std::uintptr_t,unsigned)> velocity;
    std::function<std::uint8_t(std::uintptr_t)> physics;
    std::function<void(std::uintptr_t,float,std::int32_t)> falling;
    // Entire selected block. Pathing includes original OldLocation copy and
    // displacement-derived Velocity, not merely the inner physPathing call.
    std::function<void(std::uintptr_t,OriginalPawnPhysicsDispatch,float)> otherPhysicsBlock;
    std::function<bool(std::uintptr_t)> isPlayerPawnClass;
    std::function<std::int32_t(std::uintptr_t,unsigned)> rotation,desiredRotation;
    std::function<std::int32_t(std::uintptr_t)> rotationRateRoll;
    std::function<void(std::uintptr_t,float,std::array<float,3u>)> physicsRotation;
    std::function<float(std::uintptr_t)> moveTimer,averagePhysicsTime;
    std::function<void(std::uintptr_t,float)> writeMoveTimer,writeAveragePhysicsTime;
    std::function<std::uintptr_t(std::uintptr_t)> pendingTouch;
    std::function<void(std::uintptr_t,std::uintptr_t)> postTouch,writePendingTouch;
    std::function<void()> work;
};
struct OriginalPawnPerformPhysicsStats {
    std::size_t steps{},velocityReads{},physicsReads{},dispatchCalls{},playerCasts{},
        rotationReads{},desiredReads{},rateReads{},rotationCalls{},timerReads{},timerWrites{},
        averageReads{},averageWrites{},pendingReads{},postTouchCalls{},pendingWrites{};
    std::uint8_t physicsMode{},freshPhysicsMode{};
    std::array<float,3u> capturedVelocity{};
    std::uintptr_t retained{},node{},next{};
    bool dispatched{},rotated{},rotationSkipped{},processedPendingTouch{},callbackClearedPending{};
};
namespace PawnPerformPhysicsDetail {
template<class Provider,class... Arguments>
inline auto Call(const OriginalPawnPerformPhysicsQueries& q,OriginalPawnPerformPhysicsStats& out,
    const Provider& provider,Arguments&&... arguments) {
    if(!provider)throw std::invalid_argument("Original Pawn performPhysics lacks a reached provider");
    if(!q.work)throw std::invalid_argument("Original Pawn performPhysics lacks shared work authority");
    q.work();++out.steps;
    return provider(std::forward<Arguments>(arguments)...);
}
} // namespace PawnPerformPhysicsDetail

// Owned GOTY APawn.performPhysics103C9A70..103C9CA8. All service effects and
// unconditional timer/edge stores belong to the caller's root. In particular,
// no pending-delete check, inner rollback or generic Actor rotation is added.
inline OriginalPawnPerformPhysicsStats PerformOriginalPawnPhysics(std::uintptr_t actor,float wholeDelta,
    const OriginalPawnPerformPhysicsQueries& q) {
    if(!actor)throw std::invalid_argument("Original Pawn performPhysics requires a nonnull Pawn");
    using PawnPerformPhysicsDetail::Call;
    using namespace NativeScalar;
    OriginalPawnPerformPhysicsStats out;
    for(unsigned axis=0u;axis<3u;++axis) {
        ++out.velocityReads;out.capturedVelocity[axis]=Call(q,out,q.velocity,actor,axis);
    }
    ++out.physicsReads;out.physicsMode=Call(q,out,q.physics,actor);
    if(out.physicsMode==2u) {
        out.dispatched=true;++out.dispatchCalls;Call(q,out,q.falling,actor,wholeDelta,std::int32_t{});
    } else if(out.physicsMode==1u||out.physicsMode==3u||out.physicsMode==4u||
        out.physicsMode==8u||out.physicsMode==10u) {
        out.dispatched=true;++out.dispatchCalls;
        Call(q,out,q.otherPhysicsBlock,actor,static_cast<OriginalPawnPhysicsDispatch>(out.physicsMode),wholeDelta);
    }
    ++out.physicsReads;out.freshPhysicsMode=Call(q,out,q.physics,actor);
    if(out.freshPhysicsMode==10u)out.rotationSkipped=true;
    else {
        ++out.playerCasts;bool rotate=Call(q,out,q.isPlayerPawnClass,actor);
        if(!rotate) {
            for(unsigned axis=0u;axis<3u;++axis) {
                ++out.rotationReads;const auto current=Call(q,out,q.rotation,actor,axis);
                ++out.desiredReads;const auto desired=Call(q,out,q.desiredRotation,actor,axis);
                if(current!=desired){rotate=true;break;}
            }
            if(!rotate){++out.rateReads;rotate=Call(q,out,q.rotationRateRoll,actor)>0;}
        }
        if(rotate) {
            ++out.rotationCalls;Call(q,out,q.physicsRotation,actor,wholeDelta,out.capturedVelocity);out.rotated=true;
        }
    }
    ++out.timerReads;const auto timer=Subtract(Load(Call(q,out,q.moveTimer,actor)),Memory(wholeDelta));
    // MOV/TEST PendingTouch occurs between the timer arithmetic and its FSTP.
    ++out.pendingReads;out.retained=Call(q,out,q.pendingTouch,actor);
    ++out.timerWrites;Call(q,out,q.writeMoveTimer,actor,StoreFloat(timer));
    ++out.averageReads;const auto average=Multiply(Load(Call(q,out,q.averagePhysicsTime,actor)),Memory(0.8));
    const auto contribution=Multiply(Load(wholeDelta),Memory(0.2));
    ++out.averageWrites;Call(q,out,q.writeAveragePhysicsTime,actor,StoreFloat(Add(average,contribution)));
    if(out.retained) {
        ++out.postTouchCalls;Call(q,out,q.postTouch,out.retained,actor);
        ++out.pendingReads;out.node=Call(q,out,q.pendingTouch,actor);
        // Unlike AActor's outer method, APawn explicitly accepts callback-null.
        if(!out.node)out.callbackClearedPending=true;
        else {
            ++out.pendingReads;out.next=Call(q,out,q.pendingTouch,out.node);
            ++out.pendingWrites;Call(q,out,q.writePendingTouch,actor,out.next);
            ++out.pendingWrites;Call(q,out,q.writePendingTouch,out.node,std::uintptr_t{});
            out.processedPendingTouch=true;
        }
    }
    return out;
}

} // namespace QuestVr
