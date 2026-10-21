#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

enum class OriginalDestroyActorEvent { EndState,Destroyed,LostChild };
struct OriginalDestroyActorUndoItem {
    std::int32_t index{};
    std::int32_t count{1},operation{};
    std::uint32_t originalElementBytes{4u};
};

// Native services for the COMPLETE ordinary ULevel.DestroyActor coordinator,
// Engine10395BA0..10396120. A provider is required only on its reached path.
// These are native identities/services, not names guessed from reflected data.
// Real adapters join ONE Level/Execution/hash/UObject ownership transaction and
// work ledger; no provider may pretend an unsupported service is a no-op.
struct OriginalDestroyActorQueries {
    std::function<std::int32_t(std::uintptr_t)> isValid;
    std::function<std::int32_t()> editor;
    std::function<std::uint32_t(std::uintptr_t)> actorFlags;
    std::function<std::uint8_t(std::uintptr_t)> role;
    std::function<std::int32_t(std::uintptr_t)> playerPawn;
    std::function<std::uintptr_t(std::uintptr_t)> player;
    std::function<std::int32_t(std::uintptr_t)> netConnection;
    // These three are the as-yet unnamed original connection+E64, +60 and
    // pointed object's virtual+5C contracts. No guessed close/channel behavior.
    std::function<std::uintptr_t(std::uintptr_t)> connectionTargetE64;
    std::function<std::int32_t(std::uintptr_t)> connectionState60;
    std::function<void(std::uintptr_t)> connectionTargetVirtual5C;
    // Original GetActorIndex: actual ascending live slots, first pointer match.
    // Not a cached worldActorIndex or a search filtered by deletion/rendering.
    std::function<std::int32_t(std::uintptr_t,std::uintptr_t)> getActorIndex;
    std::function<std::uintptr_t()> undo;
    // Must authenticate the actual native array/data and Actor serializer /
    // destructor, preserving the original one-item0/4-byte source contract.
    std::function<void(std::uintptr_t,std::uintptr_t,OriginalDestroyActorUndoItem)> saveActorArrayItem;
    std::function<void(std::uintptr_t)> modify;
    std::function<std::uintptr_t(std::uintptr_t)> stateFrame;
    std::function<std::uintptr_t(std::uintptr_t)> stateCode;
    std::function<std::int32_t(std::uintptr_t,std::uint32_t)> isProbing;
    std::function<std::uintptr_t(std::uintptr_t,OriginalDestroyActorEvent)> findFunctionChecked;
    // Third argument0 denotes null parameters for EndState/Destroyed; nonnull
    // source denotes the ONE Actor parameter packet for captured Owner.LostChild.
    // All three have null result pointers; selection and ProcessEvent are real.
    std::function<void(std::uintptr_t,std::uintptr_t,std::uintptr_t)> processEvent;
    std::function<std::int32_t(std::uintptr_t)> pendingDelete;
    std::function<std::uintptr_t(std::uintptr_t)> base;
    std::function<void(std::uintptr_t,std::uintptr_t,std::int32_t)> setBase;
    std::function<std::uint8_t(std::uintptr_t)> standingCount;
    std::function<std::int32_t(std::uintptr_t)> actorCount;
    std::function<std::uintptr_t(std::uintptr_t,std::int32_t)> actorSlot;
    std::function<std::uintptr_t(std::uintptr_t)> levelHash;
    std::function<std::int32_t(std::uintptr_t)> collideActors;
    std::function<void(std::uintptr_t,std::uintptr_t)> removeFromHash;
    std::function<void(std::uintptr_t,std::uintptr_t)> checkActorNotReferenced;
    std::function<std::uintptr_t(std::uintptr_t)> owner;
    std::function<void(std::uintptr_t,std::uintptr_t)> setOwner;
    std::function<std::uintptr_t(std::uintptr_t,unsigned)> touching;
    std::function<void(std::uintptr_t,std::uintptr_t,std::int32_t)> endTouch;
    std::function<std::uintptr_t(std::uintptr_t)> netDriver;
    std::function<std::uintptr_t(std::uintptr_t)> demoRecDriver;
    std::function<std::uintptr_t(std::uintptr_t)> serverConnection;
    std::function<void(std::uintptr_t,std::uintptr_t)> notifyActorDestroyed;
    // Clear the retained original slot, preserving a genuine array hole. The
    // preceding actorSlot assertion is NOT permission to rescan/compact slots.
    std::function<void(std::uintptr_t,std::int32_t)> clearActorSlot;
    std::function<void(std::uintptr_t)> setPendingDelete;
    std::function<std::uintptr_t(std::uintptr_t)> engine;
    std::function<std::uintptr_t(std::uintptr_t)> audio;
    std::function<void(std::uintptr_t,std::uintptr_t)> audioActorDestroyed;
    // UObject.ConditionalDestroy, INCLUDING selected native Destroy / dynamic
    // property/linker teardown. It is NOT scalar deletion, unhash or index free.
    std::function<std::int32_t(std::uintptr_t)> conditionalDestroy;
    std::function<std::uintptr_t(std::uintptr_t)> firstDeleted;
    std::function<void(std::uintptr_t,std::uintptr_t)> writeDeleted;
    std::function<void(std::uintptr_t,std::uintptr_t)> writeFirstDeleted;
    std::function<void(std::uintptr_t,std::int32_t)> cleanupDestroyed;
    std::function<void()> work; // Optional detached controls; SAME root in use.
};

namespace DestroyActorDetail {
template<class Function,class... Arguments>
inline auto Call(const OriginalDestroyActorQueries& query,const Function& function,Arguments... arguments) {
    if(!function)throw std::invalid_argument("Original DestroyActor lacks a reached native provider");
    if(query.work)query.work();
    return function(std::forward<Arguments>(arguments)...);
}
inline void Event(const OriginalDestroyActorQueries& query,std::uintptr_t receiver,
    OriginalDestroyActorEvent event,std::uintptr_t argument=0u) {
    const auto function=Call(query,query.findFunctionChecked,receiver,event);
    if(!function)throw std::runtime_error("Original DestroyActor selected event has no native Function");
    Call(query,query.processEvent,receiver,function,argument);
}
inline std::int32_t Count(const OriginalDestroyActorQueries& query,std::uintptr_t level) {
    const auto count=Call(query,query.actorCount,level);
    if(count<0)throw std::runtime_error("Original DestroyActor native Actor array count is invalid");
    return count;
}
} // namespace DestroyActorDetail

// Full original normal ordering, with all native lower services explicit.
// Provider failures propagate to the enclosing root. Ordinary true/false returns
// preserve actual callback effects; there is NO helper-local snapshot/rollback,
// private work allowance, pending-delete prefilter or synthetic destructor.
inline std::int32_t DestroyOriginalLevelActor(std::uintptr_t level,std::uintptr_t source,
    std::int32_t force,const OriginalDestroyActorQueries& query) {
    using DestroyActorDetail::Call;
    using DestroyActorDetail::Event;
    if(!level||!source)throw std::invalid_argument("Original DestroyActor requires a nonnull native Level and Actor");
    if(!Call(query,query.isValid,source))throw std::runtime_error("Original DestroyActor source fails UObject.IsValid");
    if(!Call(query,query.editor)) {
        const auto flags=Call(query,query.actorFlags,source);
        if(flags&0x05u)return 0; // bStatic/bNoDelete BEFORE pending-delete.
        if(flags&0x80u)return 1;
        if(Call(query,query.role,source)!=4u&&!force&&!(flags&0x01000000u))return 0;
        if(Call(query,query.playerPawn,source)) {
            const auto player=Call(query,query.player,source);
            if(player&&Call(query,query.netConnection,player)) {
                const auto target=Call(query,query.connectionTargetE64,player);
                if(target&&Call(query,query.connectionState60,player)!=1) {
                    Call(query,query.connectionTargetVirtual5C,target);return 0;
                }
            }
        }
    }
    const auto originalSlot=Call(query,query.getActorIndex,level,source);
    if(originalSlot<0)throw std::runtime_error("Original DestroyActor source has no native Level slot");
    const auto undo=Call(query,query.undo);
    if(undo)Call(query,query.saveActorArrayItem,undo,level,OriginalDestroyActorUndoItem{originalSlot,1,0,4u});
    Call(query,query.modify,source);
    const auto frame=Call(query,query.stateFrame,source);
    if(frame&&Call(query,query.stateCode,frame)&&Call(query,query.isProbing,source,0x13du)) {
        Event(query,source,OriginalDestroyActorEvent::EndState);
        if(Call(query,query.pendingDelete,source))return 1;
    }
    if(Call(query,query.base,source)) {
        Call(query,query.setBase,source,std::uintptr_t{},1);
        if(Call(query,query.pendingDelete,source))return 1;
    }
    if(Call(query,query.standingCount,source)) {
        // Count/storage are LIVE on every ascending iteration. No reverse
        // support list or pending-delete gate replaces this original scan.
        for(std::uint64_t slot=0u;slot<static_cast<std::uint64_t>(DestroyActorDetail::Count(query,level));++slot) {
            const auto candidate=Call(query,query.actorSlot,level,static_cast<std::int32_t>(slot));
            if(candidate&&Call(query,query.base,candidate)==source)
                Call(query,query.setBase,candidate,std::uintptr_t{},1);
        }
    }
    const auto hash=Call(query,query.levelHash,level);
    if(hash) {
        if(Call(query,query.collideActors,source))Call(query,query.removeFromHash,hash,source);
        const auto currentHash=Call(query,query.levelHash,level);
        if(!currentHash)throw std::runtime_error("Original DestroyActor live Hash disappeared before CheckActorNotReferenced");
        Call(query,query.checkActorNotReferenced,currentHash,source);
    }
    Event(query,source,OriginalDestroyActorEvent::Destroyed);
    if(Call(query,query.pendingDelete,source))return 1;
    for(std::uint64_t slot=0u;slot<static_cast<std::uint64_t>(DestroyActorDetail::Count(query,level));++slot) {
        const auto candidate=Call(query,query.actorSlot,level,static_cast<std::int32_t>(slot));
        if(!candidate)continue;
        if(Call(query,query.owner,candidate)==source) {
            Call(query,query.setOwner,candidate,std::uintptr_t{});
            if(Call(query,query.pendingDelete,source))return 1;
            continue; // Original owned-candidate branch SKIPS its Touching.
        }
        for(unsigned touch=0u;touch<4u;++touch) {
            if(Call(query,query.touching,candidate,touch)!=source)continue;
            Call(query,query.endTouch,source,candidate,1);
            if(Call(query,query.pendingDelete,source))return 1;
        }
    }
    const auto owner=Call(query,query.owner,source);
    if(owner) {
        Event(query,owner,OriginalDestroyActorEvent::LostChild,source);
        if(Call(query,query.pendingDelete,source))return 1;
    }
    const auto net=Call(query,query.netDriver,level);
    if(net)Call(query,query.notifyActorDestroyed,net,source);
    const auto demo=Call(query,query.demoRecDriver,level);
    if(demo&&!Call(query,query.serverConnection,demo))Call(query,query.notifyActorDestroyed,demo,source);
    if(Call(query,query.actorSlot,level,originalSlot)!=source)
        throw std::runtime_error("Original DestroyActor retained native Level slot no longer contains source");
    Call(query,query.clearActorSlot,level,originalSlot);
    Call(query,query.setPendingDelete,source);
    const auto engine=Call(query,query.engine,level);
    if(!engine)throw std::runtime_error("Original DestroyActor native Level has no Engine for Audio");
    const auto audio=Call(query,query.audio,engine);
    if(audio)Call(query,query.audioActorDestroyed,audio,source);
    static_cast<void>(Call(query,query.conditionalDestroy,source));
    if(Call(query,query.editor))Call(query,query.cleanupDestroyed,level,1);
    else {
        const auto first=Call(query,query.firstDeleted,level);
        Call(query,query.writeDeleted,source,first);
        Call(query,query.writeFirstDeleted,level,source);
    }
    return 1;
}

} // namespace QuestVr
