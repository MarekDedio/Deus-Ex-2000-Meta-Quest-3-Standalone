#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

// Actual AActor dispatch-table entries, not all EPhysics names. In particular,
// Walking/Flying/Swimming/Rotating/Spider have NO movement call in this Actor
// implementation. APawn::performPhysics is a separate original method.
enum class OriginalActorPhysicsDispatch : std::uint8_t {
    Falling=2u,Projectile=6u,Rolling=7u,Pathing=8u,MovingBrush=9u,Trailer=11u
};

struct OriginalActorPerformPhysicsQueries {
    std::function<std::uint8_t(std::uintptr_t)> physics;
    std::function<void(std::uintptr_t,float,std::int32_t)> falling;
    // This provider implements the ENTIRE selected original dispatch block.
    // Projectile/Rolling call their original native method with iterations=0.
    // Pathing/MovingBrush include the surrounding original OldLocation copy
    // and displacement-derived Velocity stores, not only their inner native
    // method. Trailer calls its original native method. Missing active blocks
    // refuse: they must never be installed as successful empty callbacks.
    std::function<void(std::uintptr_t,OriginalActorPhysicsDispatch,float)> otherPhysicsBlock;
    // Exact signed INT field channels: Pitch=0, Yaw=1, Roll=2. Only low16 is
    // tested; reads short-circuit in this order after the movement callback.
    std::function<std::int32_t(std::uintptr_t,unsigned)> rotationRate;
    std::function<void(std::uintptr_t,float)> physicsRotation;
    // Native Actor-constrained ownership/reference providers, all in the
    // caller's root. postTouch resolves the selected event ON retained, with
    // its one Actor argument Other=source. Callbacks can reenter the graph.
    std::function<std::uintptr_t(std::uintptr_t)> pendingTouch;
    std::function<void(std::uintptr_t,std::uintptr_t)> postTouch,writePendingTouch;
    std::function<void()> work;
};

struct OriginalActorPerformPhysicsStats {
    std::size_t steps{},physicsReads{},dispatchCalls{},rateReads{},rotationCalls{},
        pendingReads{},postTouchCalls{},pendingWrites{};
    std::uint8_t physicsMode{};
    bool dispatched{},rotated{},processedPendingTouch{};
    std::uintptr_t retained{},node{},next{};
};

namespace ActorPerformPhysicsDetail {
template<class Provider,class... Arguments>
inline auto Call(const OriginalActorPerformPhysicsQueries& queries,OriginalActorPerformPhysicsStats& stats,
    const Provider& provider,Arguments&&... arguments) {
    if(!provider)throw std::invalid_argument("Original Actor performPhysics lacks an active native provider");
    if(queries.work)queries.work();
    ++stats.steps;
    return provider(std::forward<Arguments>(arguments)...);
}
} // namespace ActorPerformPhysicsDetail

// Owned GOTY Engine.dll AActor::performPhysics103C9770..103C996F and its actual
// 10-entry table103C99A4. This is a native per-Actor coordinator, NOT a world
// scheduler or APawn physics. Services borrow one caller transaction/budget;
// no local rollback, deletion gate, invented event body or forced Physics is
// introduced. Every normal dispatch return reaches rotation and PendingTouch,
// including a falling body that returned after deletion/landing/Physics0.
// Active absent movement/rotation services explicitly refuse.
inline OriginalActorPerformPhysicsStats PerformOriginalActorPhysics(std::uintptr_t actor,float wholeDelta,
    const OriginalActorPerformPhysicsQueries& queries) {
    if(!actor)throw std::invalid_argument("Original Actor performPhysics requires a nonnull Actor");
    using ActorPerformPhysicsDetail::Call;
    OriginalActorPerformPhysicsStats stats;
    ++stats.physicsReads;stats.physicsMode=Call(queries,stats,queries.physics,actor);
    if(stats.physicsMode==2u) {
        stats.dispatched=true;++stats.dispatchCalls;
        Call(queries,stats,queries.falling,actor,wholeDelta,std::int32_t{});
    } else if(stats.physicsMode==6u||stats.physicsMode==7u||stats.physicsMode==8u||
        stats.physicsMode==9u||stats.physicsMode==11u) {
        stats.dispatched=true;++stats.dispatchCalls;
        Call(queries,stats,queries.otherPhysicsBlock,actor,
            static_cast<OriginalActorPhysicsDispatch>(stats.physicsMode),wholeDelta);
    }
    // Original signed INT reads feed literal bit masks, not numeric rate
    // comparisons. There is no time conversion or finite-time admission here.
    // An inactive nonfinite wholeDelta is not made an active arithmetic input.
    for(unsigned axis=0u;axis<3u;++axis) {
        ++stats.rateReads;
        const auto rate=Call(queries,stats,queries.rotationRate,actor,axis);
        if((static_cast<std::uint32_t>(rate)&0xffffu)!=0u) {
            ++stats.rotationCalls;Call(queries,stats,queries.physicsRotation,actor,wholeDelta);
            stats.rotated=true;break;
        }
    }
    const auto pending=[&](std::uintptr_t owner) {
        ++stats.pendingReads;return Call(queries,stats,queries.pendingTouch,owner);
    };
    stats.retained=pending(actor);
    if(stats.retained) {
        ++stats.postTouchCalls;Call(queries,stats,queries.postTouch,stats.retained,actor);
        stats.node=pending(actor);
        if(!stats.node)throw std::invalid_argument("Original Actor performPhysics reached null fresh PendingTouch after PostTouch");
        stats.next=pending(stats.node);
        ++stats.pendingWrites;Call(queries,stats,queries.writePendingTouch,actor,stats.next);
        ++stats.pendingWrites;Call(queries,stats,queries.writePendingTouch,stats.node,std::uintptr_t{});
        stats.processedPendingTouch=true;
    }
    return stats;
}

} // namespace QuestVr
