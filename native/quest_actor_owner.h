#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

enum class OriginalActorOwnerEvent { LostChild,GainedChild };
struct OriginalActorOwnerQueries {
    // The original reads Actor.Owner ONCE before either callback. No reverse
    // children array exists in this operation's native storage protocol.
    std::function<std::uintptr_t(std::uintptr_t)> owner;
    std::function<void(std::uintptr_t,std::uintptr_t)> writeOwner;
    // Authenticate selected LostChild/GainedChild and invoke ProcessEvent on
    // the CAPTURED receiver, with the original Actor as the one Other parm.
    // Begun/probe/event eligibility belongs to that dispatch, not this helper.
    std::function<void(std::uintptr_t,OriginalActorOwnerEvent,std::uintptr_t)> event;
    // The real adapter borrows its enclosing native-work/Execution root. Pure
    // detached controls may omit this; no new budget/transaction is created.
    std::function<void()> work;
};

namespace ActorOwnerDetail {
template<class Function,class... Arguments>
inline auto Call(const OriginalActorOwnerQueries& queries,const Function& function,Arguments... arguments) {
    if(!function)throw std::invalid_argument("Original SetOwner lacks an active provider");
    if(queries.work)queries.work();
    return function(std::forward<Arguments>(arguments)...);
}
}

// COMPLETE original AActor.SetOwner normal body10369B70..10369BFE, reached
// by Engine.dll export thunk10301EAB. Arguments/Owner are native identities,
// not copied Actor snapshots. No same-owner/cycle/pending-delete/Role gate or
// postcallback reread appears. LostChild may mutate Owner, but the retained
// new argument is then stored. GainedChild mutations remain after return.
// Ordinary returns preserve callback effects; provider failures propagate to
// the caller's real root for rollback. This helper owns no UObject lifetime.
inline void SetOriginalActorOwner(std::uintptr_t actor,std::uintptr_t newOwner,
    const OriginalActorOwnerQueries& queries) {
    if(!actor)throw std::invalid_argument("Original SetOwner requires a nonnull Actor");
    using ActorOwnerDetail::Call;
    const auto oldOwner=Call(queries,queries.owner,actor);
    if(oldOwner)Call(queries,queries.event,oldOwner,OriginalActorOwnerEvent::LostChild,actor);
    Call(queries,queries.writeOwner,actor,newOwner);
    if(newOwner)Call(queries,queries.event,newOwner,OriginalActorOwnerEvent::GainedChild,actor);
}

} // namespace QuestVr
