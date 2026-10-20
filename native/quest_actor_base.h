#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

enum class OriginalActorBaseEvent { Detach,Attach,BaseChange };
struct OriginalActorBaseQueries {
    // Direct live native fields, not basedActors/reverse-list bookkeeping.
    std::function<std::uintptr_t(std::uintptr_t)> base,level;
    std::function<void(std::uintptr_t,std::uintptr_t)> writeBase;
    std::function<std::uint8_t(std::uintptr_t)> standingCount;
    std::function<void(std::uintptr_t,std::uint8_t)> writeStandingCount;
    // BaseChange has no original argument and supplies zero here. Authentic
    // selected-event lookup/enabled-state policy belongs to the host adapter.
    std::function<void(std::uintptr_t,OriginalActorBaseEvent,std::uintptr_t)> event;
    // Borrowed enclosing-root authority. No reset or child root is created.
    std::function<void()> work;
};
struct OriginalActorBaseLimits {
    // Portable safety policy, not retail StandingCount or engine counters.
    // In particular a prospective chain cycle not containing self must refuse
    // rather than turn into an unchanged-Base no-op or an infinite traversal.
    std::size_t chainVisits{65'536u},steps{4'194'304u};
};
struct OriginalActorBaseStats {
    std::size_t steps{},chainVisits{},baseReads{},levelReads{},baseWrites{},
        countReads{},countWrites{},events{};
    bool cycleBlocked{},unchanged{};
};

namespace ActorBaseDetail {
inline void Limits(const OriginalActorBaseLimits& limits) {
    const OriginalActorBaseLimits maximum;
    if(!limits.chainVisits||limits.chainVisits>maximum.chainVisits||
        !limits.steps||limits.steps>maximum.steps)
        throw std::invalid_argument("Original SetBase callers may only tighten fixed positive bounds");
}
inline void Step(OriginalActorBaseStats& stats,const OriginalActorBaseLimits& limits,
    const OriginalActorBaseQueries& queries) {
    if(stats.steps>=limits.steps)
        throw std::runtime_error("Original SetBase shared protocol work budget exhausted");
    if(queries.work)queries.work();
    ++stats.steps;
}
template<class Function,class... Arguments>
inline auto Call(OriginalActorBaseStats& stats,const OriginalActorBaseLimits& limits,
    const OriginalActorBaseQueries& queries,const Function& function,Arguments... arguments) {
    if(!function)throw std::invalid_argument("Original SetBase lacks an active native provider");
    Step(stats,limits,queries);
    return function(std::forward<Arguments>(arguments)...);
}
} // namespace ActorBaseDetail

// AActor.SetBase10369C80..10369D8E. This detached orchestration does NOT own a
// transaction: all stores and synchronous selected events must join the SAME
// caller VM journal. Exceptions propagate; partial provider effects are not
// locally undone. It is not FindBase, SupportActor, gravity or a physics Tick.
inline OriginalActorBaseStats SetOriginalActorBase(std::uintptr_t actor,std::uintptr_t newBase,
    std::int32_t sendEvent,const OriginalActorBaseQueries& queries,const OriginalActorBaseLimits& limits={}) {
    using namespace ActorBaseDetail;
    Limits(limits);
    if(!actor)throw std::invalid_argument("Original SetBase requires a nonnull source Actor");
    OriginalActorBaseStats stats;
    const auto base=[&](std::uintptr_t identity) {
        ++stats.baseReads;return Call(stats,limits,queries,queries.base,identity);
    };
    const auto level=[&] {
        ++stats.levelReads;return Call(stats,limits,queries,queries.level,actor);
    };
    const auto count=[&](std::uintptr_t identity) {
        ++stats.countReads;return Call(stats,limits,queries,queries.standingCount,identity);
    };
    const auto writeCount=[&](std::uintptr_t identity,std::uint8_t value) {
        ++stats.countWrites;Call(stats,limits,queries,queries.writeStandingCount,identity,value);
    };
    const auto event=[&](std::uintptr_t receiver,OriginalActorBaseEvent selected,std::uintptr_t argument) {
        if(!receiver)throw std::invalid_argument("Original SetBase event receiver is an unsafe null Actor");
        ++stats.events;Call(stats,limits,queries,queries.event,receiver,selected,argument);
    };
    for(auto prospective=newBase;prospective;) {
        if(stats.chainVisits>=limits.chainVisits)
            throw std::runtime_error("Original SetBase prospective Base traversal bound exceeded");
        Step(stats,limits,queries);++stats.chainVisits;
        if(prospective==actor){stats.cycleBlocked=true;return stats;}
        prospective=base(prospective);
    }
    const auto oldBase=base(actor);
    if(oldBase==newBase){stats.unchanged=true;return stats;}
    if(oldBase&&oldBase!=level()) {
        // BYTE DEC wraps, even for an inconsistent zero count. The actual
        // event receiver is a fresh Base load AFTER the counter store.
        writeCount(oldBase,static_cast<std::uint8_t>(static_cast<unsigned>(count(oldBase))+255u));
        event(base(actor),OriginalActorBaseEvent::Detach,actor);
    }
    ++stats.baseWrites;Call(stats,limits,queries,queries.writeBase,actor,newBase);
    // EBX retains the by-value newBase across Detach: the condition AND count
    // use that captured pointer, but Level is freshly read after Detach. A
    // subsequent Base reload supplies Attach's receiver, not its count target.
    if(newBase&&newBase!=level()) {
        writeCount(newBase,static_cast<std::uint8_t>(static_cast<unsigned>(count(newBase))+1u));
        event(base(actor),OriginalActorBaseEvent::Attach,actor);
    }
    if(sendEvent)event(actor,OriginalActorBaseEvent::BaseChange,0u);
    return stats;
}

} // namespace QuestVr
