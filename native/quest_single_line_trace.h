#pragma once

#include "quest_multi_line_trace.h"

#include <cstddef>
#include <cstring>
#include <functional>
#include <string>
#include <type_traits>

namespace QuestVr {

enum class OriginalSingleLineClass { LevelInfo, Pawn, Mover, ZoneInfo };
enum class OriginalSingleLineFlag { ProjTarget, BlockActors, BlockPlayers };

// Lazy services of actual ULevel::SingleLineCheck1039AED0..1039B13B. The
// collector is the SAME native MultiLine service, including its existing
// ordering, geometry, hash and scratch ownership; this selector does not sort.
// levelActor0 is read only for trace bit4, then authenticated through isA.
// Providers own their allocation/field/class-chain limits and must charge the
// enclosing transaction's work ledger too. No nested transaction is permitted.
struct OriginalSingleLineQueries {
    std::function<std::uintptr_t()> levelActor0;
    std::function<OriginalLineHitArena(PortableModelVec3,PortableModelVec3,PortableModelVec3,
        std::int32_t,std::uintptr_t,std::uint8_t)> multiLine;
    std::function<std::uintptr_t(std::uintptr_t)> owner;
    std::function<bool(std::uintptr_t,OriginalSingleLineClass)> isA;
    std::function<bool(std::uintptr_t,OriginalSingleLineFlag)> flag;
    std::function<void()> work;
};
struct OriginalSingleLineLimits {
    // MultiLine's actual collector admits at most64 records. Owner visits are
    // cumulative over ALL candidates, not a new allowance for each hit. These
    // caps are portable safety policy; callers may only tighten positive caps.
    std::size_t records{64u},ownerVisits{65'536u},steps{4'194'304u};
};
struct OriginalSingleLineStats {
    std::size_t steps{},levelReads{},collectorCalls{},hitVisits{},ownerVisits{},
        ownerReads{},classTests{},flagReads{},excluded{},filtered{};
};
struct OriginalSingleLineResult {
    bool clear{true};
    // Keeps the by-value collector storage alive after the selected packet is
    // copied. Packet.Next and selected are LOCAL one-based arena words, never
    // native addresses or generation-qualified global capabilities. Only links
    // actually followed are validated; an unused tail is deliberately unread.
    OriginalLineHitArena hits;
    std::uintptr_t selected{};
    OriginalSingleLineStats stats;
};

namespace SingleLineTraceDetail {
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original SingleLineCheck: ")+reason);
}
inline void Limits(const OriginalSingleLineLimits& value) {
    const OriginalSingleLineLimits maximum;
    if(!value.records||value.records>maximum.records||!value.ownerVisits||
        value.ownerVisits>maximum.ownerVisits||!value.steps||value.steps>maximum.steps)
        Fail("callers may only tighten fixed positive bounds");
}
inline void Step(OriginalSingleLineStats& stats,const OriginalSingleLineLimits& limits,
    const std::function<void()>& work) {
    if(stats.steps>=limits.steps)Fail("shared selector work budget exhausted");
    if(work)work();
    ++stats.steps;
}
template<class Function,class... Arguments>
inline auto Read(const Function& function,Arguments... arguments) {
    if(!function)Fail("active native provider is unavailable");
    return function(arguments...);
}
} // namespace SingleLineTraceDetail

// Native Bool convention: result.clear is true on no accepted hit. On a
// genuine miss ONLY caller.Actor=None and caller.Time=Float1 are written; on a
// hit the complete packet, including raw Float words and Next, is copied.
// Exceptions leave caller unchanged (providers must not independently mutate
// it), propagate through the same enclosing VM root, and never mean clear.
// This is not Actor.Trace, FindBase, support callbacks or a physics scheduler.
inline OriginalSingleLineResult CheckOriginalSingleLine(OriginalActorLineHit& caller,
    std::uintptr_t source,PortableModelVec3 end,PortableModelVec3 start,PortableModelVec3 extent,
    std::uint8_t flags,std::uint8_t extraNodeFlags,const OriginalSingleLineQueries& query,
    const OriginalSingleLineLimits& limits={}) {
    using namespace SingleLineTraceDetail;
    using Class=OriginalSingleLineClass;
    using Flag=OriginalSingleLineFlag;
    Limits(limits);
    OriginalSingleLineResult result;
    const auto step=[&] {Step(result.stats,limits,query.work);};
    const auto isA=[&](std::uintptr_t actor,Class cls) {
        step();++result.stats.classTests;return Read(query.isA,actor,cls);
    };
    const auto flag=[&](std::uintptr_t actor,Flag selected) {
        step();++result.stats.flagReads;return Read(query.flag,actor,selected);
    };
    std::uintptr_t world{};
    if(flags&4u) {
        step();++result.stats.levelReads;world=Read(query.levelActor0);
        if(!world)Fail("actual native slot-zero LevelInfo is null");
        if(!isA(world,Class::LevelInfo))Fail("actual native slot-zero actor is not LevelInfo");
    }
    step();++result.stats.collectorCalls;
    result.hits=Read(query.multiLine,end,start,extent,static_cast<std::int32_t>((flags&0x17u)!=0u),
        world,extraNodeFlags);
    if(result.hits.records.size()>limits.records)Fail("collector record bound exceeded");
    auto current=result.hits.first;
    while(current) {
        step();++result.stats.hitVisits;
        if(current>result.hits.records.size())Fail("active hit link is outside its owning arena");
        const auto& hit=result.hits.records[static_cast<std::size_t>(current-1u)];
        if(!hit.actor)Fail("active hit Actor is null"); // Portable invalid-pointer admission.
        bool excluded{};
        for(auto ancestor=source;ancestor;) {
            if(result.stats.ownerVisits>=limits.ownerVisits)Fail("cumulative source-owner traversal bound exceeded");
            step();++result.stats.ownerVisits;
            if(ancestor==hit.actor) {excluded=true;break;} // No Owner read on self/equality.
            step();++result.stats.ownerReads;ancestor=Read(query.owner,ancestor);
        }
        if(excluded) {++result.stats.excluded;current=hit.next;continue;}
        bool accepted;
        // These are mutually exclusive branches. LevelInfo is also ZoneInfo,
        // but an absent level bit must not fall through to the zone branch.
        if(isA(hit.actor,Class::LevelInfo))accepted=(flags&4u)!=0u;
        else if(isA(hit.actor,Class::Pawn))accepted=(flags&1u)!=0u;
        else if(isA(hit.actor,Class::Mover))accepted=(flags&2u)!=0u;
        else if(isA(hit.actor,Class::ZoneInfo))accepted=(flags&8u)!=0u;
        else accepted=(flags&0x10u)!=0u && (!(flags&0x20u) || flag(hit.actor,Flag::ProjTarget) ||
            (flag(hit.actor,Flag::BlockActors)&&flag(hit.actor,Flag::BlockPlayers)));
        if(accepted) {
            result.clear=false;result.selected=current;
            static_assert(std::is_trivially_copyable_v<OriginalActorLineHit>);
            std::memcpy(&caller,&hit,sizeof(caller));
            return result;
        }
        ++result.stats.filtered;current=hit.next;
    }
    caller.actor=0u;caller.time=1.0f;
    return result;
}

} // namespace QuestVr
