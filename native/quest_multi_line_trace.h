#pragma once

#include "quest_actor_line_trace.h"
#include "quest_collision_hit_sort.h"

#include <functional>
#include <vector>

namespace QuestVr {

// Detached equivalents of temporary native hit lists. Next/first are portable
// one-based record handles (zero=None), NOT byte addresses. Result Next links
// are regenerated after sorting. A native adapter must establish the actual
// Model/hash/class identities and own its temporary storage for the whole call.
struct OriginalLineHitArena {
    std::vector<OriginalActorLineHit> records;
    std::uintptr_t first{};
};
struct OriginalWorldLineReply {
    bool clear{true};
    OriginalActorLineHit hit;
};
struct OriginalMultiLineQueries {
    // world receives supplied LevelInfo, owner=None is inherent in its contract.
    std::function<OriginalWorldLineReply(std::uintptr_t,const PortableModelVec3&,
        const PortableModelVec3&,const PortableModelVec3&,std::uint32_t)> world;
    // An empty actor function denotes absent native CollisionHash. Input arena
    // is owned by value so correction/repeated links mutate temporary hits only.
    std::function<OriginalLineHitArena(const PortableModelVec3&,const PortableModelVec3&,
        const PortableModelVec3&,std::uint8_t)> actors;
    std::function<bool(std::uintptr_t)> isMover; // Actual UClass ancestry, not a name.
    // Optional enclosing-root ledger. Every collector step charges it, also
    // when a later provider or collector step fails. Not a fresh per-call cap.
    std::function<void()> work{};
};
struct OriginalMultiLineLimits {
    std::size_t actorRecords{1'048'576u},querySteps{4'194'304u};
};
struct OriginalMultiLineStats {
    std::size_t steps{},actorVisits{},moverTests{},skipped{},corrected{};
};
struct OriginalMultiLineResult {
    OriginalLineHitArena hits;
    PortableModelVec3 actorQueryEnd{};
    float scale{1.0f};
    bool hasWorldHit{},nearWorldHit{};
    OriginalMultiLineStats stats;
};

namespace MultiLineTraceDetail {
using ActorLineTraceDetail::Load;
using ActorLineTraceDetail::Product;
using ActorLineTraceDetail::Read;
using ActorLineTraceDetail::Root;
using ActorLineTraceDetail::Sub;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using ScriptRandom::VectorDetail::DecodeDouble;
using ScriptRandom::VectorDetail::StoreDouble;

[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original MultiLineCheck: ")+reason);
}
inline void Limits(const OriginalMultiLineLimits& value) {
    const OriginalMultiLineLimits maximum;
    if(!value.actorRecords||value.actorRecords>maximum.actorRecords||!value.querySteps||value.querySteps>maximum.querySteps)
        Fail("callers may only tighten fixed positive budgets");
}
inline void Step(OriginalMultiLineStats& stats,const OriginalMultiLineLimits& limits,
    const std::function<void()>& work) {
    if(stats.steps>=limits.querySteps) Fail("shared collector work budget exhausted");
    if(work) work();
    ++stats.steps;
}
inline std::array<Dyadic,3u> Difference(const PortableModelVec3& a,const PortableModelVec3& b) {
    return {Sub(Load(a.x),Load(b.x)),Sub(Load(a.y),Load(b.y)),Sub(Load(a.z),Load(b.z))};
}
inline Dyadic Square(const std::array<Dyadic,3u>& value,bool reverse=false) {
    const auto x=Multiply64(value[0u],value[0u]),y=Multiply64(value[1u],value[1u]),z=Multiply64(value[2u],value[2u]);
    return reverse?Add64(Add64(z,y),x):Add64(Add64(x,y),z);
}
inline PortableModelVec3 Shorten(const PortableModelVec3& end,const PortableModelVec3& start,float scale) {
    const auto delta=Difference(end,start);const auto factor=Decode(scale);
    const float x=Store32(Multiply64(Decode(Store32(delta[0u])),factor)),
        y=Store32(Multiply64(Decode(Store32(delta[1u])),factor));
    return {Store32(Add64(Decode(x),Load(start.x))),Store32(Add64(Decode(y),Load(start.y))),
        Store32(Add64(Multiply64(delta[2u],factor),Load(start.z)))};
}
inline bool SameNormal(const PortableModelVec3& a,const PortableModelVec3& b) {
    // Match short-circuit XYZ equality, without reading inactive components.
    return Read(a.x)==Read(b.x)&&Read(a.y)==Read(b.y)&&Read(a.z)==Read(b.z);
}
inline void CorrectMover(OriginalActorLineHit& candidate,const OriginalActorLineHit& world,
    const PortableModelVec3& end,const PortableModelVec3& start) {
    const auto delta=Difference(end,start);
    const PortableModelVec3 stored{Store32(delta[0u]),Store32(delta[1u]),Store32(delta[2u])};
    const auto square=Add64(Add64(Multiply64(delta[0u],Decode(stored.x)),Product(stored.y,stored.y)),Product(stored.z,stored.z));
    const auto length=Root(StoreDouble(square));
    static_cast<void>(StoreDouble(length)); // FST Double, no reload.
    const float storedLength=Store32(length);
    if(!length.significand) Fail("zero-length mover correction has no finite native result");
    const auto inverse=ModelFastLineDetail::Divide64(Decode(1.0f),length);
    // X normalized component remains retained; Y/Z each Float-store first.
    const auto nx=Multiply64(Decode(stored.x),inverse);
    const auto ny=Decode(Store32(Multiply64(Decode(stored.y),inverse))),
        nz=Decode(Store32(Multiply64(Decode(stored.z),inverse)));
    const PortableModelVec3 doubled{Store32(Add64(nx,nx)),Store32(Add64(ny,ny)),Store32(Add64(nz,nz))};
    candidate.location={Store32(Sub(Load(world.location.x),Decode(doubled.x))),
        Store32(Sub(Load(world.location.y),Decode(doubled.y))),Store32(Sub(Load(world.location.z),Decode(doubled.z)))};
    auto corrected=Difference(candidate.location,start);
    for(auto& component:corrected) component=Decode(Store32(component));
    const auto distance=Root(StoreDouble(Square(corrected,true)));
    static_cast<void>(StoreDouble(distance)); // Original otherwise-unused FST.
    candidate.time=Store32(ModelFastLineDetail::Divide64(distance,Decode(storedLength)));
}
} // namespace MultiLineTraceDetail

// Complete detached retail collector/shortening/mover-correction operation.
// Providers are the still-distinct original primitive/hash services, NOT a
// source-filtered render ray. Each provider must bound its own work and share
// the caller's root transaction/ledger. No script-event dispatch or UObject
// writes are performed by this collector; exceptions publish no partial result.
// This does NOT implement native Move,
// primitive selection, owner transforms, hash registration or world startup.
inline OriginalMultiLineResult CollectOriginalMultiLineHits(PortableModelVec3 end,
    PortableModelVec3 start,PortableModelVec3 extent,std::int32_t checkActors,
    std::uintptr_t suppliedLevelInfo,std::uint8_t extraNodeFlags,const OriginalMultiLineQueries& query,
    const OriginalMultiLineLimits& limits={}) {
    using namespace MultiLineTraceDetail;
    Limits(limits);OriginalMultiLineResult result;result.actorQueryEnd=end;
    result.hits.records.reserve(64u);
    if(suppliedLevelInfo) {
        if(!query.world) Fail("supplied native LevelInfo has no Model query");
        Step(result.stats,limits,query.work);
        const auto reply=query.world(suppliedLevelInfo,end,start,extent,static_cast<std::uint32_t>(extraNodeFlags));
        if(!reply.clear) {
            auto hit=reply.hit;hit.actor=suppliedLevelInfo;
            const auto distance=Root(StoreDouble(Square(Difference(hit.location,start))));
            static_cast<void>(StoreDouble(distance));
            const auto scale=ModelFastLineDetail::Divide64(Multiply64(Add64(distance,Decode(5.0f)),Load(hit.time)),
                Add64(distance,Decode(std::bit_cast<float>(std::uint32_t{0x38d1b717u}))));
            result.scale=Compare64(Decode(1.0f),scale)>0?Store32(scale):1.0f;
            result.actorQueryEnd=Shorten(end,start,result.scale);
            result.hasWorldHit=true;
            result.nearWorldHit=Compare64(Load(hit.time),DecodeDouble(std::bit_cast<double>(std::uint64_t{0x3f847ae147ae147bull})))<0&&
                Compare64(distance,Decode(30.0f))<0;
            result.hits.records.push_back(hit);
        }
    }
    if(checkActors&&query.actors) {
        Step(result.stats,limits,query.work);
        auto arena=query.actors(result.actorQueryEnd,start,extent,extraNodeFlags);
        if(arena.records.size()>limits.actorRecords) Fail("actor temporary record budget exceeded");
        auto current=arena.first;
        while(current&&result.hits.records.size()<64u) {
            Step(result.stats,limits,query.work);++result.stats.actorVisits;
            if(current>arena.records.size()) Fail("active actor hit link is outside its temporary arena");
            auto& hit=arena.records[static_cast<std::size_t>(current-1u)];
            const auto mover=[&] {
                Step(result.stats,limits,query.work);++result.stats.moverTests;
                if(!hit.actor||!query.isMover) Fail("active actor hit has no native class ancestry provider");
                return query.isMover(hit.actor);
            };
            if(result.nearWorldHit&&!mover()) {++result.stats.skipped;current=hit.next;continue;}
            if(result.hasWorldHit&&mover()&&SameNormal(hit.normal,result.hits.records.front().normal)&&
                Compare64(Square(Difference(hit.location,result.hits.records.front().location)),Decode(4.0f))<0) {
                CorrectMover(hit,result.hits.records.front(),result.actorQueryEnd,start);++result.stats.corrected;
            }
            hit.time=Store32(Multiply64(Decode(result.scale),Load(hit.time)));
            result.hits.records.push_back(hit);
            current=hit.next;
        }
    }
    SortOriginalCollisionHits(result.hits.records);
    if(!result.hits.records.empty()) result.hits.first=1u;
    for(std::size_t index=0u;index<result.hits.records.size();++index)
        result.hits.records[index].next=index+1u<result.hits.records.size()?index+2u:0u;
    return result;
}

} // namespace QuestVr
