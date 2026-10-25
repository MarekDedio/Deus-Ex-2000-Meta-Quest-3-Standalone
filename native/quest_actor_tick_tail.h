#pragma once

#include "quest_native_window_clip.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

// Original AActor::Tick AFTER its main/blend animation phases, for non-Pawns
// only. These providers borrow the caller's actor, Execution and journal. The
// caller still owns stasis, owner deferral, parity, ALL preceding animations,
// native virtual selection, Level time/pause/queues and the network tail.
struct OriginalNonPawnTickTailQueries {
    std::function<std::uint8_t(std::uintptr_t)> remoteRole,role,physics;
    std::function<bool(std::uintptr_t)> isProbingTick;
    std::function<void(std::uintptr_t,float)> tick,processState,timer,performPhysics;
    std::function<float(std::uintptr_t)> lifeSpan;
    std::function<void(std::uintptr_t,float)> writeLifeSpan;
    std::function<void(std::uintptr_t)> expired;
    std::function<void(std::uintptr_t,std::int32_t)> destroy;
    std::function<void()> work;
};

struct OriginalNonPawnTickTailStats {
    std::size_t steps{},tickCalls{},stateCalls{},timerCalls{},lifeSpanWrites{},
        expiredCalls{},destroyCalls{},physicsCalls{};
    bool tickTypeSkipped{},remoteRoleBranch{},lowRoleBranch{},expired{};
};

namespace ActorTickTailDetail {
template<class Provider,class... Arguments>
inline auto Call(const OriginalNonPawnTickTailQueries& queries,OriginalNonPawnTickTailStats& stats,
    const Provider& provider,Arguments&&... arguments) {
    if(!provider)throw std::invalid_argument("Original non-Pawn Tick tail lacks an active provider");
    if(queries.work)queries.work();
    ++stats.steps;
    return provider(std::forward<Arguments>(arguments)...);
}

struct LifeSpanStep { float stored{};bool expired{}; };
inline LifeSpanStep SubtractLifeSpan(float lifeSpan,float elapsed) {
    if(!std::isfinite(lifeSpan)||!std::isfinite(elapsed))
        throw std::runtime_error("Original LifeSpan active nonfinite arithmetic is not implemented");
    using namespace NativeWindowClipDetail;
    const auto difference=Add64(ActorTimerDetail::Decode(lifeSpan),Negate(ActorTimerDetail::Decode(elapsed)));
    // FST, NOT FSTP, at103A2721: store Float but compare the still-live PC64
    // subtraction against the original binary64 0.0001 at1042AC40. Comparing
    // the rounded stored Float changes expiration at this exact boundary.
    constexpr ActorTimerDetail::Dyadic threshold{0x1a36e2eb1c432dull,-66,false};
    return {ActorTimerDetail::Store32(difference),Compare64(difference,threshold)<=0};
}
} // namespace ActorTickTailDetail

// Engine.dll103A23CE/103A249F..103A251C/103A2643..103A2783. Never replaces the
// preceding Actor.Tick prefix. No delete/active gate is inserted between
// callbacks. RemoteRole/Role select the branch once, but the normal physics
// tail rereads Physics and (only if nonzero) Role after all callbacks.
inline OriginalNonPawnTickTailStats AdvanceOriginalNonPawnTickTail(std::uintptr_t actor,
    float wholeDelta,std::int32_t tickType,const OriginalNonPawnTickTailQueries& queries) {
    if(!actor)throw std::invalid_argument("Original non-Pawn Tick tail requires a nonnull Actor");
    using ActorTickTailDetail::Call;
    OriginalNonPawnTickTailStats stats;
    const auto remote=Call(queries,stats,queries.remoteRole,actor);
    const auto role=Call(queries,stats,queries.role,actor);
    if(remote==3u) {
        stats.remoteRoleBranch=true;
        if(role==4u) {
            ++stats.stateCalls;Call(queries,stats,queries.processState,actor,wholeDelta);
            ++stats.timerCalls;Call(queries,stats,queries.timer,actor,wholeDelta);
        }
        return stats;
    }
    if(role<2u) {
        stats.lowRoleBranch=true;
        if(Call(queries,stats,queries.physics,actor)==2u) {
            ++stats.physicsCalls;Call(queries,stats,queries.performPhysics,actor,wholeDelta);
        }
        return stats;
    }
    if(tickType==1) {stats.tickTypeSkipped=true;return stats;}
    if(Call(queries,stats,queries.isProbingTick,actor)) {
        ++stats.tickCalls;Call(queries,stats,queries.tick,actor,wholeDelta);
    }
    ++stats.stateCalls;Call(queries,stats,queries.processState,actor,wholeDelta);
    ++stats.timerCalls;Call(queries,stats,queries.timer,actor,wholeDelta);
    const auto life=Call(queries,stats,queries.lifeSpan,actor);
    // Native FCOMP/TestC3 skips both signed zero and unordered NaN. An inactive
    // LifeSpan never makes DeltaTime an arithmetic input to this phase.
    if(life!=0.0f&&!std::isnan(life)) {
        const auto next=ActorTickTailDetail::SubtractLifeSpan(life,wholeDelta);
        ++stats.lifeSpanWrites;Call(queries,stats,queries.writeLifeSpan,actor,next.stored);
        if(next.expired) {
            ++stats.expiredCalls;Call(queries,stats,queries.expired,actor);
            // Expired may reset LifeSpan or delete source. Native still calls
            // DestroyActor(force0), without a new property/deletion read.
            ++stats.destroyCalls;Call(queries,stats,queries.destroy,actor,std::int32_t{});
            stats.expired=true;return stats;
        }
    }
    if(Call(queries,stats,queries.physics,actor)!=0u&&Call(queries,stats,queries.role,actor)!=3u) {
        ++stats.physicsCalls;Call(queries,stats,queries.performPhysics,actor,wholeDelta);
    }
    return stats;
}

} // namespace QuestVr
