#pragma once

#include "quest_actor_line_trace.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

struct OriginalLevelEncroachmentQueries {
    // Live native Actor+1DC bits: CollideActors=01, BlockActors=04,
    // BlockPlayers=08. All unrelated bits, including CollideWorld=02, retain
    // their original meaning but do not independently admit this operation.
    std::function<std::uint32_t(std::uintptr_t)> admissionFlags;
    // Raw nonnull Brush + actual Engine.Brush ancestry + !bStatic. This is
    // neither a primitive/asset validator nor the broader IsBrush predicate.
    std::function<bool(std::uintptr_t)> movingBrush;
    std::function<std::uintptr_t(std::uintptr_t)> levelHash;

    // One mark covers the ORIGINAL list and ALL push requery lists. Tokens
    // are opaque and may be zero. popMark MUST be noexcept, allocation-free,
    // and unmetered; mandatory cleanup cannot receive a new work allowance
    // or be prevented by exhausted work. A throwing cleanup violates this
    // provider contract, and must not be treated as a successful operation.
    std::function<std::uintptr_t()> mark;
    std::function<void(std::uintptr_t)> popMark;
    // Actual source-pose/hash query. The final word is ALWAYS literal0, not
    // the higher-level touch flag. Result handles and their Actor/Next fields
    // remain live until the mark is popped; zero is the null-list handle.
    std::function<std::uintptr_t(std::uintptr_t,std::uintptr_t,PortableModelVec3,
        std::array<std::int32_t,3u>,std::uint32_t)> hashEncroachment;
    std::function<std::uintptr_t(std::uintptr_t)> hitActor,hitNext;

    // Separate raw slot0 loads from its authentic LevelInfo class check.
    // The first and second hit passes have distinct final raw-read ordering.
    std::function<std::uintptr_t(std::uintptr_t)> levelActor0;
    std::function<void(std::uintptr_t)> requireLevelInfo;
    std::function<bool(std::uintptr_t,std::uintptr_t)> blockedBy;

    // Channel numbers are X=0,Y=1,Z=2. Loads return RAW Float words, including
    // inactive NaNs and signed zero. Only arithmetic use requires finiteness.
    // Temporary writes affect Location ONLY, never Rotation/ColLocation/hash.
    std::function<float(std::uintptr_t,unsigned)> locationChannel;
    std::function<void(std::uintptr_t,unsigned,std::uint32_t)> writeLocationChannel;
    // Fixed raw restoration, noexcept/allocation-free/unmetered like popMark.
    // This stronger exceptional guarantee is NOT attributed to retail EH.
    std::function<void(std::uintptr_t,std::array<std::uint32_t,3u>)> restoreLocation;

    // A COMPLETE collision-aware native dependency, not a location setter.
    // Integer results are ignored by this higher operation. A real adapter
    // must join the same MoveActor/hash/VM transaction and work ledger.
    std::function<std::int32_t(std::uintptr_t,PortableModelVec3)> moveSmooth;
    // EncroachingOn must perform authentic selected-event dispatch with its
    // Bool DWORD initially0. Return raw signed native int; any nonzero vetoes.
    std::function<std::int32_t(std::uintptr_t,std::uintptr_t)> encroachingOn;
    std::function<void(std::uintptr_t,std::uintptr_t)> encroachedBy;
    std::function<std::uintptr_t(std::uintptr_t,unsigned)> touchSlot;
    std::function<bool(std::uintptr_t,std::uintptr_t)> overlaps;
    std::function<void(std::uintptr_t,std::uintptr_t,std::int32_t)> endTouch;
    std::function<void(std::uintptr_t,std::uintptr_t)> beginTouch;

    // Optional at the detached layer; the real adapter MUST supply its shared
    // caller work ledger. Every active ordinary provider invocation charges
    // it BEFORE the provider. Nested moves/queries/events cannot reset it.
    std::function<void()> work;
};

namespace LevelEncroachmentDetail {
using ActorLineTraceDetail::Load;
using ActorLineTraceDetail::Sub;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;

template<class Function,class... Arguments>
inline auto Call(const OriginalLevelEncroachmentQueries& queries,const Function& function,
    Arguments... arguments) {
    if(!function)throw std::invalid_argument("Original Level encroachment lacks an active native provider");
    if(queries.work)queries.work();
    return function(std::forward<Arguments>(arguments)...);
}

class MarkScope {
public:
    explicit MarkScope(const OriginalLevelEncroachmentQueries& queries):queries_(queries) {
        // Never acquire a resource whose cleanup provider is unavailable.
        if(!queries_.popMark)
            throw std::invalid_argument("Original Level encroachment lacks its mark cleanup provider");
        token_=Call(queries_,queries_.mark);active_=true;
    }
    ~MarkScope() noexcept {Close();}
    MarkScope(const MarkScope&)=delete;
    MarkScope& operator=(const MarkScope&)=delete;
    void Close() noexcept {
        if(!active_)return;
        active_=false; // A cleanup must never be retried if its contract fails.
        queries_.popMark(token_);
    }
private:
    const OriginalLevelEncroachmentQueries& queries_;
    std::uintptr_t token_{};
    bool active_{};
};

class LocationScope {
public:
    LocationScope(std::uintptr_t actor,std::array<std::uint32_t,3u> saved,
        const OriginalLevelEncroachmentQueries& queries):queries_(queries),actor_(actor),saved_(saved) {
        if(!queries_.restoreLocation)
            throw std::invalid_argument("Original Level encroachment lacks raw Location restoration");
        active_=true; // BEFORE the first write or its work hook can throw.
    }
    ~LocationScope() noexcept {Restore();}
    LocationScope(const LocationScope&)=delete;
    LocationScope& operator=(const LocationScope&)=delete;
    void Restore() noexcept {
        if(!active_)return;
        active_=false;
        queries_.restoreLocation(actor_,saved_);
    }
private:
    const OriginalLevelEncroachmentQueries& queries_;
    std::uintptr_t actor_{};
    std::array<std::uint32_t,3u> saved_{};
    bool active_{};
};

class LinkVisits {
public:
    explicit LinkVisits(std::size_t maximum):maximum_(maximum) {}
    void Visit() {
        if(used_>=maximum_)
            throw std::runtime_error("Original Level encroachment cumulative linked-hit work refused");
        ++used_;
    }
private:
    std::size_t maximum_{},used_{};
};

inline std::optional<std::uintptr_t> Candidate(std::uintptr_t level,std::uintptr_t source,
    std::uintptr_t hit,bool secondPass,const OriginalLevelEncroachmentQueries& queries) {
    if(Call(queries,queries.hitActor,hit)==source)return std::nullopt;
    // Retail first asserts nonnull, then authentic LevelInfo ancestry on a
    // separately reloaded slot0. It does not cache that pointer for comparison.
    if(!Call(queries,queries.levelActor0,level))
        throw std::invalid_argument("Original Level encroachment requires actual Level.Actors[0]");
    const auto checked=Call(queries,queries.levelActor0,level);
    Call(queries,queries.requireLevelInfo,checked);
    std::uintptr_t actor{},info{};
    if(secondPass) {
        info=Call(queries,queries.levelActor0,level);
        actor=Call(queries,queries.hitActor,hit);
    }else {
        actor=Call(queries,queries.hitActor,hit);
        info=Call(queries,queries.levelActor0,level);
    }
    if(actor==info)return std::nullopt;
    return actor;
}

inline PortableModelVec3 Delta(std::uintptr_t source,const PortableModelVec3& proposed,
    const OriginalLevelEncroachmentQueries& queries) {
    // 1039A53A..571: load/subtract Z,Y,X in PC64, then Float-spill X,Y,Z.
    // Separate statements retain raw provider order; operand evaluation order
    // in a single C++ expression must not select a different live pose.
    const auto pz=Load(proposed.z);const auto sz=Call(queries,queries.locationChannel,source,2u);
    const auto z=Sub(pz,Load(sz));
    const auto py=Load(proposed.y);const auto sy=Call(queries,queries.locationChannel,source,1u);
    const auto y=Sub(py,Load(sy));
    const auto px=Load(proposed.x);const auto sx=Call(queries,queries.locationChannel,source,0u);
    const auto x=Sub(px,Load(sx));
    const float storedX=Store32(x),storedY=Store32(y),storedZ=Store32(z);
    return {storedX,storedY,storedZ};
}

inline void Reverse(std::uintptr_t source,std::uintptr_t originalHit,PortableModelVec3 delta,
    const PortableModelVec3& proposed,const OriginalLevelEncroachmentQueries& queries) {
    // 1039A6F6..74B: literal Float -1 products stay retained in Z,Y,X order.
    // Raw saved Location loads are X,Y,Z, and can contain inactive NaN words.
    const auto z=Multiply64(Load(delta.z),Decode(-1.0f));
    const auto y=Multiply64(Load(delta.y),Decode(-1.0f));
    const auto x=Multiply64(Load(delta.x),Decode(-1.0f));
    const auto rawX=std::bit_cast<std::uint32_t>(Call(queries,queries.locationChannel,source,0u));
    const auto rawY=std::bit_cast<std::uint32_t>(Call(queries,queries.locationChannel,source,1u));
    const auto rawZ=std::bit_cast<std::uint32_t>(Call(queries,queries.locationChannel,source,2u));
    LocationScope temporary(source,{rawX,rawY,rawZ},queries);
    Call(queries,queries.writeLocationChannel,source,0u,std::bit_cast<std::uint32_t>(proposed.x));
    const float storedX=Store32(x);
    Call(queries,queries.writeLocationChannel,source,1u,std::bit_cast<std::uint32_t>(proposed.y));
    const float storedY=Store32(y);
    Call(queries,queries.writeLocationChannel,source,2u,std::bit_cast<std::uint32_t>(proposed.z));
    const float storedZ=Store32(z);
    const auto actor=Call(queries,queries.hitActor,originalHit); // AFTER override.
    static_cast<void>(Call(queries,queries.moveSmooth,actor,PortableModelVec3{storedX,storedY,storedZ}));
    // Normal restore is equally mandatory, before the caller reads hit.Actor
    // again for EncroachingOn. This does NOT undo the candidate's reverse move.
    temporary.Restore();
}
} // namespace LevelEncroachmentDetail

// COMPLETE detached ULevel.CheckEncroachment1039A350..1039A90D protocol.
// Proposed pose and Rotation are native by-value arguments. It is NOT a real
// MoveActor implementation, and must not be exposed as one with a fake Smooth
// provider. A real adapter must authenticate all live native objects/handles,
// join one current Level/hash/VM root, dispatch real events and touching, and
// supply complete collision-aware movement. No renderer or other-map state.
//
// Ordinary true veto preserves previous pushes/events/touches. Errors/refusals
// propagate for caller-root rollback; only temporary source Location and the
// memory mark have stronger local exceptional restoration. The local cumulative
// link cap is a portable safety refusal, not truncation, and never replaces the
// shared work ledger. Cycles are not skipped and no Actor/Next snapshot is made.
inline bool CheckOriginalLevelEncroachment(std::uintptr_t level,std::uintptr_t source,
    PortableModelVec3 proposed,std::array<std::int32_t,3u> rotation,std::int32_t touch,
    const OriginalLevelEncroachmentQueries& queries,std::size_t maximumLinkVisits=65536u) {
    if(!level||!source)
        throw std::invalid_argument("Original Level encroachment requires nonnull Level and source");
    using namespace LevelEncroachmentDetail;
    if(!(Call(queries,queries.admissionFlags,source)&0x0du)&&!Call(queries,queries.movingBrush,source))
        return false;
    MarkScope memory(queries);
    LinkVisits visits(maximumLinkVisits);
    const auto initialHash=Call(queries,queries.levelHash,level);
    const auto original=initialHash?Call(queries,queries.hashEncroachment,initialHash,source,proposed,rotation,0u):0u;
    for(auto hit=original;hit;hit=Call(queries,queries.hitNext,hit)) {
        visits.Visit();
        const auto actor=Candidate(level,source,hit,false,queries);
        if(!actor||!Call(queries,queries.blockedBy,source,*actor))continue;
        if(Call(queries,queries.movingBrush,source)) {
            const auto current=Call(queries,queries.hitActor,hit);
            if(!Call(queries,queries.movingBrush,current)) {
                const auto delta=Delta(source,proposed,queries);
                const auto pushed=Call(queries,queries.hitActor,hit);
                static_cast<void>(Call(queries,queries.moveSmooth,pushed,delta));
                const auto hash=Call(queries,queries.levelHash,level);
                if(!hash)throw std::invalid_argument("Original Level encroachment lost its required push-requery hash");
                auto requery=Call(queries,queries.hashEncroachment,hash,source,proposed,rotation,0u);
                bool remains=false;
                while(requery) {
                    visits.Visit();
                    const auto found=Call(queries,queries.hitActor,requery);
                    const auto wanted=Call(queries,queries.hitActor,hit);
                    if(found==wanted) {remains=true;break;}
                    requery=Call(queries,queries.hitNext,requery);
                }
                if(!remains)continue;
                Reverse(source,hit,delta,proposed,queries);
            }
        }
        const auto other=Call(queries,queries.hitActor,hit);
        if(Call(queries,queries.encroachingOn,source,other))return true;
    }
    if(touch) {
        for(unsigned slot=0u;slot<4u;++slot) {
            const auto other=Call(queries,queries.touchSlot,source,slot);
            if(!other||Call(queries,queries.overlaps,source,other))continue;
            const auto current=Call(queries,queries.touchSlot,source,slot);
            Call(queries,queries.endTouch,source,current,0);
        }
    }
    // ORIGINAL list, not the most recent requery, and no new overlap test.
    for(auto hit=original;hit;hit=Call(queries,queries.hitNext,hit)) {
        visits.Visit();
        const auto actor=Candidate(level,source,hit,true,queries);
        if(!actor)continue;
        if(Call(queries,queries.blockedBy,source,*actor)) {
            const auto current=Call(queries,queries.hitActor,hit);
            Call(queries,queries.encroachedBy,current,source);
        }else if(touch) {
            const auto current=Call(queries,queries.hitActor,hit);
            Call(queries,queries.beginTouch,source,current);
        }
    }
    return false; // MarkScope pops on BOTH normal native results.
}

} // namespace QuestVr
