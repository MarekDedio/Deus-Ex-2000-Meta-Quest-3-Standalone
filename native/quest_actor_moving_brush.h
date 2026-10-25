#pragma once

#include "quest_model_fast_line_check.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

struct OriginalActorMovingBrushQueries {
    // Raw native Float channels: X=0, Y=1, Z=2. The prefix is a DWORD
    // copy, not arithmetic: providers must preserve signed zero/NaN payloads.
    std::function<float(std::uintptr_t,unsigned)> location,oldLocation;
    std::function<void(std::uintptr_t,unsigned,float)> writeOldLocation,writeVelocity;
    // The complete original physMovingBrush behavior, including active
    // interpolation, movement and callbacks, in the SAME caller Execution.
    // An idle-only or successful empty substitute is not this contract.
    std::function<void(std::uintptr_t,float)> inner;
    std::function<void()> work;
};

struct OriginalActorMovingBrushStats {
    std::size_t steps{},locationReads{},oldLocationReads{},oldLocationWrites{},
        innerCalls{},velocityWrites{};
};

namespace ActorMovingBrushDetail {
template<class Provider,class... Arguments>
inline auto Call(const OriginalActorMovingBrushQueries& queries,
    OriginalActorMovingBrushStats& stats,const Provider& provider,Arguments&&... arguments) {
    if(!provider)
        throw std::invalid_argument("Original Actor moving-brush envelope lacks an active provider");
    if(queries.work)queries.work();
    ++stats.steps;
    return provider(std::forward<Arguments>(arguments)...);
}

inline ActorTimerDetail::Dyadic Difference(float current,float old) {
    // Admission is deliberately here, AFTER both reached source reads, not
    // on the raw-copy prefix or an eagerly captured whole pose packet.
    if(!std::isfinite(current) || !std::isfinite(old))
        throw std::runtime_error("Original Actor moving-brush reached nonfinite displacement");
    return NativeWindowClipDetail::Add64(ActorTimerDetail::Decode(current),
        NativeWindowClipDetail::Negate(ActorTimerDetail::Decode(old)));
}
} // namespace ActorMovingBrushDetail

// Owned GOTY Engine.dll performPhysics Physics9 block103C9856..103C98CE.
// This is ONLY the outer envelope; the supplied inner implements the genuine
// physMovingBrush body. Rotation/PendingTouch still belong to the caller.
// Every provider borrows the current transaction. No local undo, independent
// root, stale pose reuse, fake movement or no-interpolation assumption exists.
// Reached nonfinite/zero-divisor and binary32-overflow arithmetic explicitly
// refuses; the caller's existing root must roll back every preceding effect.
inline OriginalActorMovingBrushStats PerformOriginalActorMovingBrush(
    std::uintptr_t actor,float wholeDelta,const OriginalActorMovingBrushQueries& queries) {
    if(!actor)throw std::invalid_argument("Original Actor moving-brush requires a nonnull Actor");
    using ActorMovingBrushDetail::Call;
    using ActorTimerDetail::Decode;
    using ActorTimerDetail::Store32;
    using ModelFastLineDetail::Multiply64;
    OriginalActorMovingBrushStats stats;
    const auto location=[&](unsigned axis) {
        ++stats.locationReads;return Call(queries,stats,queries.location,actor,axis);
    };
    const auto oldLocation=[&](unsigned axis) {
        ++stats.oldLocationReads;return Call(queries,stats,queries.oldLocation,actor,axis);
    };
    for(unsigned axis=0u;axis<3u;++axis) {
        const float value=location(axis); // One read followed immediately by its raw store.
        ++stats.oldLocationWrites;Call(queries,stats,queries.writeOldLocation,actor,axis,value);
    }
    ++stats.innerCalls;Call(queries,stats,queries.inner,actor,wholeDelta);

    const auto difference=[&](unsigned axis) {
        const float current=location(axis);
        const float old=oldLocation(axis); // A callback can have changed BOTH fields.
        return ActorMovingBrushDetail::Difference(current,old);
    };
    const auto z=difference(2u); // Z remains PC64 on the original x87 stack.
    const float y=Store32(difference(1u));
    const float x=Store32(difference(0u));
    // The original inner has already run, even when this delta is exceptional.
    if(!std::isfinite(wholeDelta) || wholeDelta==0.0f)
        throw std::runtime_error("Original Actor moving-brush reached nonfinite or zero reciprocal divisor");
    const auto reciprocal=ModelFastLineDetail::Divide64(Decode(1.0f),Decode(wholeDelta));
    const float storedReciprocal=Store32(reciprocal); // FST, not FSTP/reload for Z.
    const auto resultZ=Multiply64(z,reciprocal);
    const auto resultY=Multiply64(Decode(storedReciprocal),Decode(y));
    const auto resultX=Multiply64(Decode(storedReciprocal),Decode(x));
    const float storedX=Store32(resultX);
    const float storedY=Store32(resultY);
    ++stats.velocityWrites;Call(queries,stats,queries.writeVelocity,actor,0u,storedX);
    // This Float spill is AFTER the X destination store. A reached Z overflow
    // therefore refuses with that prefix still present in the caller journal.
    const float storedZ=Store32(resultZ);
    ++stats.velocityWrites;Call(queries,stats,queries.writeVelocity,actor,1u,storedY);
    ++stats.velocityWrites;Call(queries,stats,queries.writeVelocity,actor,2u,storedZ);
    return stats;
}

} // namespace QuestVr
