#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>

namespace QuestVr {

enum class OriginalBlockingClass { Brush, PlayerPawn, Projectile };
enum class OriginalBlockingFlag { CollideWorld, BlockActors, BlockPlayers };

// Live, lazy native-field providers. Brush and Player are only null tests:
// neither pointer's payload is dereferenced by these original helpers. In
// particular hasPlayer is receiver-local, NOT script native720's Level scan.
struct OriginalActorBlockingQueries {
    std::function<std::uintptr_t(std::uintptr_t)> level;
    std::function<bool(std::uintptr_t)> hasBrush,hasPlayer;
    std::function<bool(std::uintptr_t,OriginalBlockingClass)> isA;
    std::function<bool(std::uintptr_t,OriginalBlockingFlag)> flag;
    std::function<std::uintptr_t(std::uintptr_t)> base;
    std::function<void()> work;
};

namespace ActorBlockingDetail {
template<class Function,class... Arguments>
inline auto Read(const Function& function,Arguments... arguments) {
    if(!function)throw std::invalid_argument("Original actor predicate lacks an active native provider");
    return function(arguments...);
}
inline bool Brush(std::uintptr_t actor,const OriginalActorBlockingQueries& queries) {
    return Read(queries.hasBrush,actor)&&Read(queries.isA,actor,OriginalBlockingClass::Brush);
}
} // namespace ActorBlockingDetail

// AActor::GetPlayerPawn1034C540..577. There is deliberately no fallback to
// another Level actor, bIsPlayer, a viewport scan, or a Player payload read.
inline std::uintptr_t GetOriginalActorLocalPlayerPawn(std::uintptr_t actor,
    const OriginalActorBlockingQueries& queries) {
    if(!actor)throw std::invalid_argument("Original local GetPlayerPawn requires a nonnull receiver");
    using ActorBlockingDetail::Read;
    return Read(queries.isA,actor,OriginalBlockingClass::PlayerPawn)&&Read(queries.hasPlayer,actor)?actor:0u;
}

// AActor::IsBlockedBy1034C5E0..82A. Preserve precedence, direction and lazy
// field/class reads; callers supply their own self/base/deletion/query filters.
// Portable admission refuses null actors instead of emulating invalid native
// dereferences. A missing INACTIVE provider is not examined.
inline bool IsOriginalActorBlockedBy(std::uintptr_t source,std::uintptr_t candidate,
    const OriginalActorBlockingQueries& queries) {
    if(!source||!candidate)throw std::invalid_argument("Original IsBlockedBy requires nonnull actors");
    using ActorBlockingDetail::Read;
    using Class=OriginalBlockingClass;
    using Flag=OriginalBlockingFlag;
    if(candidate==Read(queries.level,source))return Read(queries.flag,source,Flag::CollideWorld);
    if(ActorBlockingDetail::Brush(candidate,queries)) {
        if(!Read(queries.flag,source,Flag::CollideWorld))return false;
        return Read(queries.flag,candidate,GetOriginalActorLocalPlayerPawn(source,queries)?
            Flag::BlockPlayers:Flag::BlockActors);
    }
    if(ActorBlockingDetail::Brush(source,queries)) {
        if(!Read(queries.flag,candidate,Flag::CollideWorld))return false;
        return Read(queries.flag,source,GetOriginalActorLocalPlayerPawn(candidate,queries)?
            Flag::BlockPlayers:Flag::BlockActors);
    }
    const auto selected=[&](std::uintptr_t actor) {
        return GetOriginalActorLocalPlayerPawn(actor,queries)||Read(queries.isA,actor,Class::Projectile)?
            Flag::BlockPlayers:Flag::BlockActors;
    };
    if(!Read(queries.flag,candidate,selected(source)))return false;
    return Read(queries.flag,source,selected(candidate));
}

// AActor::IsBasedOn1034C8D0..8F2: self belongs to its own nonnull Base chain;
// the terminating None never matches. No target fields/class are read. Retail
// has no cycle guard. Bounded traversal is portable safety policy: exhaustion
// THROWS (never a fabricated false) and must poison the same outer VM root.
inline bool IsOriginalActorBasedOn(std::uintptr_t source,std::uintptr_t target,
    const OriginalActorBlockingQueries& queries,std::size_t maximumVisits=65536u) {
    using ActorBlockingDetail::Read;
    std::size_t visits{};
    while(source) {
        if(visits==maximumVisits)throw std::runtime_error("Original IsBasedOn traversal exceeded its portable work budget");
        ++visits;
        if(queries.work)queries.work();
        if(source==target)return true;
        source=Read(queries.base,source);
    }
    return false;
}

} // namespace QuestVr
