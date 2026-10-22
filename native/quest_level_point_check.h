#pragma once

#include "quest_multi_line_trace.h"
#include "quest_native_scratch_scope.h"
#include "quest_decal_state.h"
#include <cstring>
#include <functional>
#include <optional>
#include <stdexcept>

namespace QuestVr {
// Retail Engine1039ACC0/1039AAC0. Hit links are opaque arena-owned handles,
// not source indices. Runtime providers must retain their actual Level/hash
// authority and share the enclosing work/scratch/transaction ownership.
struct OriginalMultiPointQueries {
    std::function<std::uintptr_t()> hash;
    std::function<OriginalLineHitArena(std::uintptr_t,PortableModelVec3,PortableModelVec3,std::uint32_t)> actors;
    std::function<OriginalWorldLineReply(std::uintptr_t,std::uintptr_t,PortableModelVec3,PortableModelVec3,std::uint32_t)> world;
    std::function<void()> work;
    std::size_t* scratchBytes{};
    std::function<void(std::size_t)> validate;
};
struct OriginalSinglePointQueries {
    std::function<std::uintptr_t()> mark;
    // Mandatory noexcept, allocation-free cleanup, including on provider error.
    std::function<void(std::uintptr_t)> pop;
    std::function<std::uintptr_t(PortableModelVec3,PortableModelVec3,std::uint32_t,std::uintptr_t,std::int32_t)> multiPoint;
    std::function<OriginalActorLineHit(std::uintptr_t)> packet;
    std::function<void()> work;
};
namespace LevelPointCheckDetail {
template<class F,class... A>inline auto Call(const std::function<void()>& work,const F& f,A&&... args) {
    if(!f)throw std::runtime_error("Original Level point check lacks an active provider");
    if(work)work();
    return f(std::forward<A>(args)...);
}
inline ActorTimerDetail::Dyadic Distance(PortableModelVec3 point,PortableModelVec3 at,bool selected) {
    using namespace ActorLineTraceDetail;
    const auto x=Sub(Load(at.x),Load(point.x)),y=Sub(Load(at.y),Load(point.y));
    auto z=Sub(Load(at.z),Load(point.z));
    // SinglePoint's selected Z delta alone spills Float at1039ABA8.
    if(selected)z=Decode(Store32(z));
    return Add64(Add64(Multiply64(z,z),Multiply64(y,y)),Multiply64(x,x));
}
}
inline OriginalLineHitArena CollectOriginalMultiPointHits(PortableModelVec3 location,PortableModelVec3 extent,
    std::uint32_t extraFlags,std::uintptr_t world,std::int32_t checkActors,const OriginalMultiPointQueries& q,
    std::size_t maximumRecords=65'536u) {
    using namespace LevelPointCheckDetail;
    if(!maximumRecords || maximumRecords>65'536u || !q.scratchBytes || !q.validate)
        throw std::runtime_error("Original MultiPointCheck lacks bounded scratch ownership");
    // The temporary reservation is declared before the arena it accounts for.
    std::optional<NativeScratchScope> retained;
    OriginalLineHitArena hits;
    if(checkActors) {
        const auto hash=Call(q.work,q.hash);
        if(hash)hits=Call(q.work,q.actors,hash,location,extent,extraFlags);
    }
    if(hits.records.size()>maximumRecords)
        throw std::runtime_error("Original MultiPointCheck actor record bound exceeded");
    retained.emplace(*q.scratchBytes,DecalStateDetail::Multiply(hits.records.capacity(),sizeof(OriginalActorLineHit)),q.validate);
    if(world) {
        auto hit=Call(q.work,q.world,world,std::uintptr_t{},location,extent,std::uint32_t{});
        if(!hit.clear) {
            if(hits.records.size()==maximumRecords)
                throw std::runtime_error("Original MultiPointCheck world record bound exceeded");
            // Admission includes old+new vector overlap. No allocation can
            // escape a failed query as a fabricated successful empty list.
            if(hits.records.size()==hits.records.capacity()) {
                const auto capacity=std::min(maximumRecords,std::max<std::size_t>(1u,hits.records.size()*2u));
                q.validate(DecalStateDetail::Multiply(capacity,sizeof(OriginalActorLineHit)));
                hits.records.reserve(capacity);
                retained.reset();
                retained.emplace(*q.scratchBytes,DecalStateDetail::Multiply(hits.records.capacity(),sizeof(OriginalActorLineHit)),q.validate);
            }
            hit.hit.next=hits.first;hit.hit.actor=world;
            hits.records.push_back(hit.hit);hits.first=hits.records.size();
        }
    }
    return hits; // Receiver must admit/own the returned arena before queries.
}
inline std::int32_t CheckOriginalSinglePoint(OriginalActorLineHit& caller,PortableModelVec3 location,
    PortableModelVec3 extent,std::uint32_t extraFlags,std::uintptr_t world,std::int32_t checkActors,
    const OriginalSinglePointQueries& q,std::size_t maximumVisits=65'536u) {
    using namespace LevelPointCheckDetail;
    if(!maximumVisits || maximumVisits>65'536u || !q.pop)
        throw std::runtime_error("Original SinglePointCheck lacks bounded mark cleanup");
    const auto mark=Call(q.work,q.mark);
    struct Scope {const OriginalSinglePointQueries& q;std::uintptr_t token;
        ~Scope()noexcept{q.pop(token);}} scope{q,mark};
    auto current=Call(q.work,q.multiPoint,location,extent,extraFlags,world,checkActors);
    if(!current)return 1; // ALL caller fields remain unchanged on a miss.
    auto selected=Call(q.work,q.packet,current);current=selected.next;
    std::size_t visits=1u;
    while(current) {
        if(visits++>=maximumVisits)throw std::runtime_error("Original SinglePointCheck hit traversal bound exceeded");
        const auto candidate=Call(q.work,q.packet,current);
        if(NativeWindowClipDetail::Compare64(Distance(location,selected.location,true),
            Distance(location,candidate.location,false))>0)selected=candidate;
        current=candidate.next;
    }
    static_assert(std::is_trivially_copyable_v<OriginalActorLineHit>);
    std::memcpy(&caller,&selected,sizeof(caller));return 0;
}
} // namespace QuestVr
