#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

enum class OriginalActorTouchEvent { Touch,UnTouch };
struct OriginalActorTouchQueries {
    std::function<std::uintptr_t(std::uintptr_t,unsigned)> readSlot;
    std::function<void(std::uintptr_t,unsigned,std::uintptr_t)> writeSlot;
    std::function<void(std::uintptr_t)> requireValid;
    std::function<std::uint8_t(std::uintptr_t)> role;
    std::function<bool(std::uintptr_t)> pawn,isPlayer;
    std::function<void(std::uintptr_t,OriginalActorTouchEvent,std::uintptr_t)> event;
    // Optional shared caller work ledger. Every active provider invocation
    // charges it; no nested touching operation receives a fresh allowance.
    std::function<void()> work;
};

namespace ActorTouchDetail {
inline void Actors(std::uintptr_t source,std::uintptr_t other) {
    // Retail asserts distinct/non-null inputs before the one-sided insertion
    // body, and EndTouch asserts distinctness before either slot scan. Refuse
    // null portable pointers instead of emulating invalid raw dereferences.
    if(!source||!other||source==other)
        throw std::invalid_argument("Original actor touching requires two distinct nonnull actors");
}
template<class Function,class... Arguments>
inline auto Call(const OriginalActorTouchQueries& queries,const Function& function,Arguments... arguments) {
    if(!function)throw std::invalid_argument("Original actor touching lacks an active native provider");
    if(queries.work)queries.work();
    return function(std::forward<Arguments>(arguments)...);
}
inline std::uintptr_t Read(const OriginalActorTouchQueries& queries,std::uintptr_t actor,unsigned slot) {
    return Call(queries,queries.readSlot,actor,slot);
}
inline void Write(const OriginalActorTouchQueries& queries,std::uintptr_t actor,unsigned slot,std::uintptr_t peer) {
    Call(queries,queries.writeSlot,actor,slot,peer);
}
inline void Event(const OriginalActorTouchQueries& queries,std::uintptr_t actor,
    OriginalActorTouchEvent event,std::uintptr_t peer) {
    Call(queries,queries.event,actor,event,peer);
}
} // namespace ActorTouchDetail

// AActor.EndTouch10369970..10369A48. Clear FIRST matches only. The reciprocal
// scan begins AFTER a possible synchronous source UnTouch, observes live peer
// slots, and notifies that peer even when source had no match or suppressSelf
// was any nonzero signed native int. No deletion/history/compaction shortcut.
inline void EndOriginalActorTouch(std::uintptr_t source,std::uintptr_t other,
    std::int32_t suppressSelf,const OriginalActorTouchQueries& queries) {
    using namespace ActorTouchDetail;
    Actors(source,other);
    for(unsigned slot=0u;slot<4u;++slot) {
        if(Read(queries,source,slot)!=other)continue;
        Write(queries,source,slot,0u);
        if(!suppressSelf)Event(queries,source,OriginalActorTouchEvent::UnTouch,other);
        break;
    }
    for(unsigned slot=0u;slot<4u;++slot) {
        if(Read(queries,other,slot)!=source)continue;
        Write(queries,other,slot,0u);
        Event(queries,other,OriginalActorTouchEvent::UnTouch,source);
        break;
    }
}

// Original one-sided helper10369630..103698B4. A true result does NOT promise
// insertion: duplicate and full/no-replacement paths also return true. Only
// changing the exact newly inserted slot during Touch returns false.
inline bool AddOriginalActorTouchSide(std::uintptr_t source,std::uintptr_t other,
    const OriginalActorTouchQueries& queries) {
    using namespace ActorTouchDetail;
    Actors(source,other);
    int selected=-1;
    for(unsigned slot=0u;slot<4u;++slot) {
        const auto peer=Read(queries,source,slot);
        if(!peer)selected=static_cast<int>(slot); // LAST empty, not first.
        else if(peer==other)return true;
    }
    if(selected<0) {
        // All four indices are visited, even after the first eviction. EndTouch
        // can synchronously alter later entries. Read again AFTER IsValid;
        // never cache a full-array snapshot or skip a now-invalid later peer.
        for(unsigned slot=0u;slot<4u;++slot) {
            const auto checked=Read(queries,source,slot);
            Call(queries,queries.requireValid,checked);
            const auto peer=Read(queries,source,slot);
            if(!Call(queries,queries.role,peer)) {
                EndOriginalActorTouch(source,peer,0,queries);
                selected=static_cast<int>(slot); // LAST Role0 index wins.
            }
        }
        if(selected<0&&Call(queries,queries.pawn,other)) {
            // Incoming Pawn displaces FIRST non-Pawn, not every such peer.
            for(unsigned slot=0u;slot<4u;++slot) {
                const auto classified=Read(queries,source,slot);
                if(Call(queries,queries.pawn,classified))continue;
                const auto peer=Read(queries,source,slot);
                EndOriginalActorTouch(source,peer,0,queries);
                selected=static_cast<int>(slot);
                break;
            }
            if(selected<0&&Call(queries,queries.isPlayer,other)) {
                // Here isPlayer is Pawn.bIsPlayer(+318 mask02), NOT local
                // GetPlayerPawn. Reclassify each live peer, then read its
                // current pointer again before the player-bit test/eviction.
                for(unsigned slot=0u;slot<4u;++slot) {
                    const auto classified=Read(queries,source,slot);
                    if(Call(queries,queries.pawn,classified)) {
                        const auto player=Read(queries,source,slot);
                        if(Call(queries,queries.isPlayer,player))continue;
                    }
                    const auto peer=Read(queries,source,slot);
                    EndOriginalActorTouch(source,peer,0,queries);
                    selected=static_cast<int>(slot);
                    break;
                }
            }
        }
    }
    if(selected<0)return true;
    // No rescan for emptiness or duplicate insertion after EndTouch callbacks.
    const auto slot=static_cast<unsigned>(selected);
    Write(queries,source,slot,other);
    Event(queries,source,OriginalActorTouchEvent::Touch,other);
    return Read(queries,source,slot)==other;
}

// AActor.BeginTouch10369570..103695C5. The second one-sided result is ignored;
// a full first side may therefore still attempt reciprocal insertion. All
// writes/events must join the SAME caller VM/hash root. This detached protocol
// propagates refusal; it cannot undo provider-owned state or events itself.
inline void BeginOriginalActorTouch(std::uintptr_t source,std::uintptr_t other,
    const OriginalActorTouchQueries& queries) {
    if(AddOriginalActorTouchSide(source,other,queries))
        static_cast<void>(AddOriginalActorTouchSide(other,source,queries));
}

} // namespace QuestVr
