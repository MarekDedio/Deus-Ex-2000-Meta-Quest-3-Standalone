#include "quest_collision_hash.h"
#include "quest_collision_hash_oracle.h"

#include <iostream>
#include <limits>
#include <cstdlib>
#include <new>
#include <unordered_set>

namespace HashAllocationFault {
thread_local std::size_t remaining=std::numeric_limits<std::size_t>::max(),attempts{};
void* Allocate(std::size_t bytes) {
    ++attempts;
    if(!remaining) throw std::bad_alloc();
    if(remaining!=std::numeric_limits<std::size_t>::max()) --remaining;
    if(auto* memory=std::malloc(bytes?bytes:1u)) return memory;
    throw std::bad_alloc();
}
void Enable(std::size_t successes) noexcept {attempts=0u;remaining=successes;}
void Disable() noexcept {remaining=std::numeric_limits<std::size_t>::max();}
}
void* operator new(std::size_t bytes) {return HashAllocationFault::Allocate(bytes);}
void* operator new[](std::size_t bytes) {return HashAllocationFault::Allocate(bytes);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* memory) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* memory,std::size_t) noexcept {std::free(memory);}

namespace {
using Vec=PortableModelVec3;
using Box=QuestVr::OriginalCollisionHashBox;
using Range=QuestVr::OriginalCollisionHashRange;
using World=QuestVr::OriginalCollisionHashWorld;
using Actor=QuestVr::OriginalHashActor;
using Storage=QuestVr::OriginalHashStorage;
using Queries=QuestVr::OriginalHashQueries;
using Arena=QuestVr::OriginalLineHitArena;
using Hit=QuestVr::OriginalActorLineHit;
using Oracle=QuestVr::CollisionHashOracle::Oracle;
static_assert(!std::is_copy_constructible_v<World::RootLease>&&!std::is_move_constructible_v<World::RootLease>);
static_assert(std::is_nothrow_destructible_v<World::RootLease>);
static_assert(noexcept(std::declval<World::RootLease&>().Rollback())&&noexcept(std::declval<World::RootLease&>().CommitPrepared()));
static_assert(noexcept(std::declval<const World&>().RetainedBytes())&&noexcept(std::declval<const World&>().AdditionalRootBytes()));
std::size_t checks{},refusals{},randomOperations{},queryComparisons{};
std::size_t explicitRoots{},rootAllocationRefusals{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
bool Same(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec a,Vec b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Actor& a,const Actor& b) {return a.identity==b.identity&&Same(a.location,b.location)&&Same(a.colLocation,b.colLocation)&&
    a.collideActors==b.collideActors&&a.pendingDelete==b.pendingDelete&&a.collisionTag==b.collisionTag&&a.storageGeneration==b.storageGeneration;}
bool Same(const Hit& a,const Hit& b) {return a.actor==b.actor&&a.next==b.next&&a.primitive==b.primitive&&a.item==b.item&&
    Same(a.location,b.location)&&Same(a.normal,b.normal)&&Same(a.time,b.time);}
bool Same(const Arena& a,const Arena& b) {
    if(a.first!=b.first||a.records.size()!=b.records.size()) return false;
    for(std::size_t i=0;i<a.records.size();++i) if(!Same(a.records[i],b.records[i])) return false;
    return true;
}
template<class F> void Refuse(F run,const char* reason="Unsupported collision-hash operation silently succeeded") {
    bool failed{};try {run();} catch(const std::exception&) {failed=true;}
    Require(failed,reason);++refusals;
}
QuestVr::OriginalCollisionHashTables Tables() {
    QuestVr::OriginalCollisionHashTables result;
    for(std::size_t i=0;i<result.x.size();++i) result.x[i]=result.y[i]=result.z[i]=static_cast<std::uint32_t>(i);
    return result; // EXPLICIT synthetic permutations, not genuine process RNG.
}
float Coordinate(int cell) {return static_cast<float>(cell)*256.0f-32768.0f;}
Vec Point(int x,int y=0,int z=0) {return {Coordinate(x),Coordinate(y),Coordinate(z)};}
Box Bounds(int x,int y=0,int z=0) {const auto p=Point(x,y,z);return {p,p};}
std::vector<Oracle::Cell> Cells(const Range& range,const QuestVr::OriginalCollisionHashTables& tables) {
    std::vector<Oracle::Cell> result;
    for(auto x=range.minimum[0];x<=range.maximum[0];++x)
        for(auto y=range.minimum[1];y<=range.maximum[1];++y)
            for(auto z=range.minimum[2];z<=range.maximum[2];++z) {
                const auto bucket=tables.x[x]^tables.y[y]^tables.z[z];
                result.emplace_back(bucket,static_cast<std::uint32_t>(z)*65536u+static_cast<std::uint32_t>(y)*256u+static_cast<std::uint32_t>(x));
            }
    return result;
}
void Compare(const Storage& actual,const Oracle& expected) {
    expected.Validate();Require(actual.links.size()==expected.SlotsView().size()&&actual.activeLinks==expected.LinkCount(),
        "Hash chunk/active-link count differs from independent vector oracle");
    std::vector<std::size_t> next(actual.links.size());
    for(std::size_t bucket=0;bucket<actual.heads.size();++bucket) {
        const auto& members=expected.BucketsView()[bucket];
        Require(actual.heads[bucket]==(members.empty()?0u:members.front()),"Hash bucket prepend order differs from vector oracle");
        for(std::size_t i=0;i<members.size();++i) next[members[i]]=i+1u<members.size()?members[i+1u]:0u;
    }
    const auto& free=expected.FreeView();Require(actual.free==(free.empty()?0u:free.front()),"Hash free head differs from independent oracle");
    for(std::size_t i=0;i<free.size();++i) next[free[i]]=i+1u<free.size()?free[i+1u]:0u;
    for(std::size_t i=1;i<actual.links.size();++i) {
        const auto& link=actual.links[i];const auto& slot=expected.SlotsView()[i];
        Require(link.actor==slot.actor&&link.cell==slot.cell&&link.initialized==slot.initialized&&link.next==next[i],
            "Stable link/free/stale payload differs from independent vector oracle");
    }
}
struct Fixture {
    std::unique_ptr<QuestVr::OriginalCollisionHashTables> tableStorage=std::make_unique<QuestVr::OriginalCollisionHashTables>(Tables());
    QuestVr::OriginalCollisionHashTables& tables=*tableStorage;
    std::unique_ptr<World> world;
    std::uint64_t hash{};
    std::unique_ptr<Oracle> oracle=std::make_unique<Oracle>();
    std::unordered_map<std::uintptr_t,Box> boxes;
    std::vector<std::uintptr_t> calls;
    std::uint32_t expectedTag{};
    std::unordered_map<std::uintptr_t,std::uint32_t> tags;
    unsigned boundsCalls{};
    explicit Fixture(std::uint32_t tag=0,const QuestVr::OriginalHashLimits& limits={}):
        world(std::make_unique<World>(tables,tag,limits)),expectedTag(tag) {hash=world->CreateHash();}
    void Define(std::uintptr_t id,Box box,bool collide=true,bool pending=false,std::uint32_t tag=0) {
        Actor actor;actor.identity=id;actor.location=box.minimum;actor.colLocation={71,72,73};
        actor.collideActors=collide;actor.pendingDelete=pending;actor.collisionTag=tag;
        world->DefineActor(actor);boxes[id]=box;tags[id]=tag;
    }
    Queries Provider() {return {
        [this](std::uintptr_t id) {++boundsCalls;return boxes.at(id);},
        [this](Hit& hit,std::uintptr_t id,const Vec&,const Vec&,const Vec&,std::uint32_t flags) {
            Require(hit.next==0&&hit.actor==0&&hit.primitive==0&&Same(hit.time,0)&&hit.item==-1&&
                Same(hit.location,{})&&Same(hit.normal,{}),"ActorLine primitive seed is not original zero-Time packet");
            Require(flags==255u,"Hash did not zero-extend exact byte flags");calls.push_back(id);Fill(hit,id);return false;}};}
    static void Fill(Hit& hit,std::uintptr_t id) {
        hit.actor=id+10'000u;hit.primitive=id+20'000u;hit.location={static_cast<float>(id),2,3};
        hit.normal={-1,0,0};hit.time=static_cast<float>(id)/1000;hit.item=static_cast<std::int32_t>(id);
    }
    void Add(std::uintptr_t id) {
        world->Add(hash,id,Provider());
        if(!world->ReadActor(id).pendingDelete) oracle->Add(id,Cells(QuestVr::OriginalCollisionHashBoxRange(boxes.at(id)),tables));
    }
    void Remove(std::uintptr_t id) {
        world->Remove(hash,id,Provider());
        if(!world->ReadActor(id).pendingDelete) {
            std::vector<std::uint32_t> buckets;for(auto [bucket,cell]:Cells(QuestVr::OriginalCollisionHashBoxRange(boxes.at(id)),tables)) {
                static_cast<void>(cell);buckets.push_back(bucket);
            }
            oracle->Remove(id,buckets);
        }
    }
    void Check() {Compare(world->ReadHash(hash),*oracle);}
    Arena Query(Vec end,Vec start={},Vec extent={}) {return world->ActorLineCheck(hash,end,start,extent,255,Provider());}
    void CompareQuery(Vec end,Vec start,Vec extent={}) {
        calls.clear();const auto actual=Query(end,start,extent);Arena expected;++expectedTag;std::vector<std::uintptr_t> expectedCalls;
        for(auto [bucket,cell]:Cells(QuestVr::OriginalCollisionHashSweptRange(end,start,extent),tables)) {
            auto current=oracle->Head(bucket);
            while(current) {
                const auto slot=oracle->Slot(current);
                Require(slot.initialized,"Independent ordinary query reached undefined metadata");
                if(tags[slot.actor]!=expectedTag&&slot.cell==cell) {
                    tags[slot.actor]=expectedTag;expectedCalls.push_back(slot.actor);Hit hit;hit.time=0;Fill(hit,slot.actor);
                    hit.next=expected.first;expected.records.push_back(hit);expected.first=expected.records.size();
                }
                current=oracle->Next(current);
            }
        }
        Require(world->ReadTag()==expectedTag&&Same(actual,expected)&&calls==expectedCalls,
            "ActorLine hit/tag/order differs from independent vector traversal");
        for(auto [actor,tag]:tags) Require(world->ReadActor(actor).collisionTag==tag,"Actor native tag differs from independent oracle");
        ++queryComparisons;
    }
};
void Registration() {
    Fixture fixture;fixture.Define(1,Bounds(1));fixture.Define(2,Bounds(1));fixture.Add(1);fixture.Add(2);fixture.Add(1);fixture.Check();
    fixture.CompareQuery(Point(1),Point(1));Require(fixture.calls==std::vector<std::uintptr_t>{1,2},"Duplicate Add changed first encounter order");
    Require(Same(fixture.world->ReadActor(1).colLocation,Point(1)),"Add did not copy live Location to ColLocation");
    const auto registered=fixture.world->ReadActor(1).colLocation;
    fixture.boxes[1]=Bounds(2);fixture.world->SetActorLive(1,Point(2),true,false);fixture.Remove(1);fixture.Check();
    Require(Same(fixture.world->ReadActor(1).colLocation,registered)&&fixture.world->ReadHash(fixture.hash).activeLinks==3u,
        "Current-bound Remove repaired stale buckets or changed ColLocation");
    fixture.CompareQuery(Point(1),Point(1));fixture.boxes[1]=Bounds(0,1);fixture.Remove(1);fixture.Check();
    Require(fixture.world->ReadHash(fixture.hash).activeLinks==1u,"Remove incorrectly tested encoded cell inside current bucket");
    fixture.Define(3,Bounds(0));fixture.world->SetActorLive(3,{-0.0f,7,8},true,false);
    fixture.boxes[3]={Point(3),Point(2)};fixture.Add(3);fixture.Check();
    Require(Same(fixture.world->ReadActor(3).colLocation,Vec{-0.0f,7,8}),"Inverted Add did not retain bitwise live ColLocation write");
    fixture.Define(4,Bounds(0),true,true);auto before=fixture.world->ReadHash(fixture.hash);auto actor=fixture.world->ReadActor(4);
    const auto count=fixture.boundsCalls;fixture.world->Add(fixture.hash,4,{});fixture.world->Remove(fixture.hash,4,{});
    Require(fixture.world->ReadHash(fixture.hash)==before&&Same(fixture.world->ReadActor(4),actor)&&fixture.boundsCalls==count,
        "Pending-delete Add/Remove read inactive bounds or mutated state");
    fixture.world->SetActorLive(4,Point(0),false,true);Refuse([&]{fixture.world->Add(fixture.hash,4,{});});
    Refuse([&]{fixture.world->Remove(fixture.hash,4,{});});
    Fixture chunks;chunks.Define(1,Bounds(0));
    for(unsigned i=0;i<1025u;++i) chunks.Add(1);
    chunks.Check();Require(chunks.world->ReadHash(chunks.hash).links.size()==2049u,"Hash did not allocate whole1024-slot chunks");
    chunks.Remove(1);chunks.Check();chunks.Add(1);chunks.Check();
    Require(chunks.world->ReadHash(chunks.hash).heads[0]==1u,"Remove/Add did not reuse free links in actual LIFO order");
}
void CandidateOrder() {
    Fixture fixture;fixture.Define(1,Bounds(1,0));fixture.Define(2,Bounds(0,1));fixture.Add(1);fixture.Add(2);
    fixture.CompareQuery(Point(1,1),Point(0,0));
    Require(fixture.calls==std::vector<std::uintptr_t>{2,1},"Wrong-cell encounter incorrectly tagged actor before its real cell");
    const auto stats=fixture.world->ReadStats();Require(stats.links>stats.primitiveCalls,"Aliased bucket nodes were not charged/visited");
    fixture.world->SetActorLive(1,Point(1),false,true);fixture.CompareQuery(Point(1,1),Point(0,0));
    Require(fixture.calls.size()==2u,"Query filtered registered membership by live collision/delete flags");
    Fixture alias;std::swap(alias.tables.y[1],alias.tables.y[256]);
    alias.world=std::make_unique<World>(alias.tables);alias.hash=alias.world->CreateHash();alias.Define(1,Bounds(256,0));alias.Add(1);
    alias.CompareQuery(Point(0,1),Point(0,1));Require(alias.calls==std::vector<std::uintptr_t>{1},"Shifted-ADD encoded-cell alias was incorrectly eliminated");
    Fixture wrap(std::numeric_limits<std::uint32_t>::max());wrap.Define(1,Bounds(0),true,false,0);wrap.Add(1);
    wrap.CompareQuery(Point(0),Point(0));Require(wrap.calls.empty()&&wrap.world->ReadTag()==0u,"Tag wrap reset actor tags or invented64-bit sequence");
    wrap.CompareQuery(Point(0),Point(0));Require(wrap.calls.size()==1u,"Post-wrap query did not resume global tagging");
    auto clear=fixture.Provider();clear.line=[&](Hit&,std::uintptr_t id,const Vec&,const Vec&,const Vec&,std::uint32_t) {
        fixture.calls.push_back(id);return true;};fixture.calls.clear();
    const auto empty=fixture.world->ActorLineCheck(fixture.hash,Point(1,1),Point(0,0),{},255,clear);
    Require(empty.records.empty()&&empty.first==0u&&fixture.calls.size()==2u,"Clear primitive lost tag or produced fake hit");
}
void Reentrance() {
    Fixture fixture;fixture.Define(1,Bounds(0));fixture.Define(2,Bounds(0));fixture.Add(2);fixture.Add(1);
    bool nested{};std::vector<std::uintptr_t> trace;auto query=fixture.Provider();
    query.line=[&](Hit& hit,std::uintptr_t id,const Vec& end,const Vec& start,const Vec& extent,std::uint32_t) {
        trace.push_back(id);Fixture::Fill(hit,id);
        if(!nested) {nested=true;const auto inner=fixture.world->ActorLineCheck(fixture.hash,end,start,extent,255,query);
            Require(inner.records.size()==2u,"Nested query lost already-outer-tagged actor");}
        return false;
    };
    const auto outer=fixture.world->ActorLineCheck(fixture.hash,Point(0),Point(0),{},255,query);
    Require(trace==std::vector<std::uintptr_t>{1,1,2}&&outer.records.size()==1u&&fixture.world->ReadTag()==2u,
        "Outer query cached initial tag instead of reading nested query global tag");
    fixture.Check();
    Fixture removed;removed.Define(1,Bounds(0));removed.Define(2,Bounds(0));removed.Add(2);removed.Add(1);
    auto mutation=removed.Provider();trace.clear();mutation.line=[&](Hit& hit,std::uintptr_t id,const Vec&,const Vec&,const Vec&,std::uint32_t) {
        trace.push_back(id);if(id==1u) removed.Remove(2);Fixture::Fill(hit,id);return false;};
    const auto single=removed.world->ActorLineCheck(removed.hash,Point(0),Point(0),{},255,mutation);
    Require(single.records.size()==1u&&trace==std::vector<std::uintptr_t>{1},"Post-callback traversal used cached Next after removing follower");removed.Check();
    // Consume a complete chunk so current-node remove/reuse cannot wander
    // into never-initialized free Actor metadata. C reuses A's exact slot.
    Fixture reused;reused.Define(1,Bounds(0));reused.Define(2,Bounds(0));reused.Define(3,Bounds(0));
    for(unsigned i=0;i<1023u;++i) reused.Add(2);
    reused.Add(1);bool changed{};trace.clear();
    auto replace=reused.Provider();replace.line=[&](Hit& hit,std::uintptr_t id,const Vec&,const Vec&,const Vec&,std::uint32_t) {
        trace.push_back(id);Fixture::Fill(hit,id);
        if(!changed) {changed=true;reused.Remove(1);reused.Add(3);}return false;};
    const auto result=reused.world->ActorLineCheck(reused.hash,Point(0),Point(0),{},255,replace);
    Require(trace==std::vector<std::uintptr_t>{1,2}&&result.records.size()==2u&&result.records[0].actor==10'001u&&
        reused.world->ReadHash(reused.hash).links[1024].actor==3u,"Current-node reuse changed returned original packet or used stale Next");reused.Check();
    // New head is not retroactively visited in the current cell, but a later
    // same-bucket cell must fetch its head afresh (different encoded cell).
    Fixture prepended;prepended.Define(1,Bounds(0,1));prepended.Define(2,Bounds(1,0));prepended.Add(1);trace.clear();
    auto prepend=prepended.Provider();prepend.line=[&](Hit& hit,std::uintptr_t id,const Vec&,const Vec&,const Vec&,std::uint32_t) {
        trace.push_back(id);if(id==1u) prepended.Add(2);Fixture::Fill(hit,id);return false;};
    const auto nextCell=prepended.world->ActorLineCheck(prepended.hash,Point(1,1),Point(0,0),{},255,prepend);
    Require(trace==std::vector<std::uintptr_t>{1,2}&&nextCell.records.size()==2u,"Later aliased cell did not fetch live bucket head");prepended.Check();
    Fixture shared;shared.Define(1,Bounds(0));shared.Add(1);const auto second=shared.world->CreateHash();shared.world->Add(second,1,shared.Provider());
    bool inside{};auto cross=shared.Provider();trace.clear();cross.line=[&](Hit& hit,std::uintptr_t id,const Vec& e,const Vec& s,const Vec& q,std::uint32_t) {
        trace.push_back(id);if(!inside) {inside=true;shared.world->ActorLineCheck(second,e,s,q,255,cross);}Fixture::Fill(hit,id);return false;};
    shared.world->ActorLineCheck(shared.hash,Point(0),Point(0),{},255,cross);
    Require(trace.size()==2u&&shared.world->ReadTag()==2u&&shared.world->ReadActor(1).collisionTag==2u,
        "Two native hash instances did not share process-global tag/actor state");
}
void Rollback() {
    Fixture fixture;fixture.Define(1,Bounds(0));fixture.Define(2,Bounds(0));fixture.Add(2);fixture.Add(1);
    const auto hash=fixture.world->ReadHash(fixture.hash);const auto actor=fixture.world->ReadActor(1),other=fixture.world->ReadActor(2);
    const auto tag=fixture.world->ReadTag();
    auto verify=[&] {Require(fixture.world->ReadHash(fixture.hash)==hash&&Same(fixture.world->ReadActor(1),actor)&&
        Same(fixture.world->ReadActor(2),other)&&fixture.world->ReadTag()==tag,"Refusal retained hash/free/actor/tag root mutations");};
    auto failing=fixture.Provider();failing.line=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,const Vec&,std::uint32_t)->bool {
        fixture.world->SetActorLive(1,{99,88,77},false,true);fixture.Remove(2);throw std::runtime_error("Late primitive refusal");};
    Refuse([&]{fixture.world->ActorLineCheck(fixture.hash,Point(0),Point(0),{},255,failing);});verify();
    failing=fixture.Provider();failing.line=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,const Vec&,std::uint32_t) {
        try {fixture.world->Add(fixture.hash,999,{});} catch(const std::exception&) {}
        return false;
    };
    Refuse([&]{fixture.world->ActorLineCheck(fixture.hash,Point(0),Point(0),{},255,failing);},"Caught nested refusal allowed root query to commit");verify();
    failing=fixture.Provider();failing.line=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,const Vec&,std::uint32_t) {
        try {fixture.world->ReadHash(999);} catch(const std::exception&) {}
        return false;
    };
    Refuse([&]{fixture.world->ActorLineCheck(fixture.hash,Point(0),Point(0),{},255,failing);},"Caught invalid diagnostic read allowed root commit");verify();
    for(bool destroyOwner:{false,true}) {
        failing=fixture.Provider();failing.line=[&](Hit&,std::uintptr_t id,const Vec&,const Vec&,const Vec&,std::uint32_t) {
            if(destroyOwner) fixture.world->DestroyActorStorage(id);else fixture.world->DestroyHash(fixture.hash);return false;};
        Refuse([&]{fixture.world->ActorLineCheck(fixture.hash,Point(0),Point(0),{},255,failing);});verify();
    }
    failing=fixture.Provider();failing.line=[&](Hit&,std::uintptr_t id,const Vec&,const Vec&,const Vec&,std::uint32_t) {
        const auto old=fixture.world->ReadActor(id);fixture.world->DestroyActorStorage(id);fixture.world->DefineActor(old);return false;};
    Refuse([&]{fixture.world->ActorLineCheck(fixture.hash,Point(0),Point(0),{},255,failing);},
        "Same-handle actor replacement defeated active storage-generation check");verify();
    std::uint64_t provisionalHash{};
    Refuse([&]{fixture.world->WithRoot([&](World& owned) {
        owned.Remove(fixture.hash,2,fixture.Provider());
        owned.SetActorLive(1,{99,88,77},true,false);owned.Add(fixture.hash,1,fixture.Provider());
        provisionalHash=owned.CreateHash();owned.Add(provisionalHash,1,fixture.Provider());
        throw std::runtime_error("Last enclosing caller effect refused after successful natives");
    });});verify();Refuse([&]{fixture.world->ReadHash(provisionalHash);},"Late caller refusal retained a provisional hash instance");
    Refuse([&]{fixture.world->WithRoot([&](World& owned) {
        owned.SetActorLive(1,{9,8,7},true,false);
        try {owned.Remove(fixture.hash,999,{});} catch(const std::exception&) {}
    });},"Caught nested native failure allowed enclosing WithRoot commit");verify();
    Fixture grouping;grouping.Define(1,Bounds(0));grouping.Add(1);
    const auto grouped=grouping.world->WithRoot([&](World& owned) {
        owned.ActorLineCheck(grouping.hash,Point(0),Point(0),{},255,grouping.Provider());return owned.ReadTag();
    });
    Require(grouped==1u&&grouping.world->ReadTag()==1u,"Successful enclosing caller root lost returned result/native state");
    grouping.world->WithRoot([&](World& owned) {owned.SetActorLive(1,{7,8,9},true,false);});
    Require(Same(grouping.world->ReadActor(1).location,Vec{7,8,9}),"Successful void caller root lost native mutation");
    failing=fixture.Provider();failing.line=[&](Hit&,std::uintptr_t id,const Vec&,const Vec&,const Vec&,std::uint32_t) {
        fixture.world->Remove(fixture.hash,id,fixture.Provider());return false;};
    Refuse([&]{fixture.world->ActorLineCheck(fixture.hash,Point(0),Point(0),{},255,failing);},"Query guessed undefined never-used free link Actor data");verify();
    Fixture copies;copies.Define(1,Bounds(0));copies.Define(2,Bounds(0));copies.Add(2);copies.Add(1);
    auto end=Point(0),start=Point(0);Vec extent{};auto copied=copies.Provider();unsigned call{};
    copied.line=[&](Hit&,std::uintptr_t,const Vec& e,const Vec& s,const Vec& q,std::uint32_t) {
        ++call;end={10,20,30};start={40,50,60};extent={70,80,90};
        Require(Same(e,Point(0))&&Same(s,Point(0))&&Same(q,{}),"Callback changed copied native query vectors");return true;};
    copies.world->ActorLineCheck(copies.hash,end,start,extent,255,copied);Require(call==2u,"Copied-input fixture missed later primitive");
    QuestVr::OriginalHashLimits limits;limits.steps=1030u;Fixture budget(0,limits);budget.Define(1,Bounds(0));budget.Add(1);
    const auto baseline=budget.world->ReadHash(budget.hash);auto recur=budget.Provider();
    recur.line=[&](Hit&,std::uintptr_t,const Vec& e,const Vec& s,const Vec& q,std::uint32_t) {
        budget.world->ActorLineCheck(budget.hash,e,s,q,255,recur);return false;};
    Refuse([&]{budget.world->ActorLineCheck(budget.hash,Point(0),Point(0),{},255,recur);});
    Require(budget.world->ReadHash(budget.hash)==baseline&&budget.world->ReadTag()==0u,"Nested budget/depth refusal retained tags or links");
    limits={};limits.hits=1u;Fixture hits(0,limits);hits.Define(1,Bounds(0));hits.Define(2,Bounds(0));hits.Add(1);hits.Add(2);
    Refuse([&]{hits.Query(Point(0),Point(0));});Require(hits.world->ReadTag()==0u&&hits.world->ReadActor(1).collisionTag==0u,
        "Hit allocation budget failure retained first primitive's native tag");
    limits={};limits.links=1024u;Fixture allocation(0,limits);allocation.Define(1,Bounds(0));
    for(unsigned i=0;i<1024u;++i) allocation.Add(1);
    const auto full=allocation.world->ReadHash(allocation.hash);
    Refuse([&]{allocation.Add(1);});Require(allocation.world->ReadHash(allocation.hash)==full,"Chunk refusal retained partial links/free state");
    limits={};limits.cells=1u;Fixture cells(0,limits);cells.Define(1,{Point(0),Point(1)});const auto pristine=cells.world->ReadHash(cells.hash);
    Refuse([&]{cells.Add(1);});Require(cells.world->ReadHash(cells.hash)==pristine,"Cell-product refusal retained registration prefix");
    Refuse([&]{cells.Query(Point(1),Point(0));});Require(cells.world->ReadTag()==0u,"Swept cell refusal retained global tag increment");
    limits={};limits.cells=2u;Fixture cumulative(0,limits);cumulative.Define(1,Bounds(0));cumulative.Add(1);
    auto nestedCells=cumulative.Provider();bool entered{};
    nestedCells.line=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,const Vec&,std::uint32_t) {
        if(!entered) {entered=true;cumulative.world->ActorLineCheck(cumulative.hash,Point(1),Point(0),{},255,nestedCells);}return true;
    };
    const auto beforeCells=cumulative.world->ReadHash(cumulative.hash);
    Refuse([&]{cumulative.world->ActorLineCheck(cumulative.hash,Point(0),Point(0),{},255,nestedCells);});
    Require(cumulative.world->ReadHash(cumulative.hash)==beforeCells&&cumulative.world->ReadTag()==0u,
        "Nested cell visits acquired a fresh budget or retained tag writes");
    limits={};limits.steps=1025u;Fixture partialAllocation(0,limits);partialAllocation.Define(1,Bounds(0));
    const auto beforeChunk=partialAllocation.world->ReadHash(partialAllocation.hash);const auto beforeActor=partialAllocation.world->ReadActor(1);
    Refuse([&]{partialAllocation.Add(1);});
    Require(partialAllocation.world->ReadHash(partialAllocation.hash)==beforeChunk&&Same(partialAllocation.world->ReadActor(1),beforeActor),
        "Mid-chunk initialization work refusal retained allocated/free prefix or ColLocation");
    Fixture invalid;invalid.Define(1,Bounds(0));Refuse([&]{invalid.world->Add(invalid.hash,1,{});});
    invalid.Add(1);Refuse([&]{invalid.world->ActorLineCheck(invalid.hash,Point(0),Point(0),{},0,{});});
    const float nan=std::numeric_limits<float>::quiet_NaN();Refuse([&]{invalid.Query({nan,0,0},Point(0));});
}
void ExplicitRoots() {
    Fixture fixture;fixture.Define(1,Bounds(0));fixture.Define(2,Bounds(0));fixture.Add(2);fixture.Add(1);
    const auto hash=fixture.world->ReadHash(fixture.hash);const auto tag=fixture.world->ReadTag();
    const auto actor=fixture.world->ReadActor(1),other=fixture.world->ReadActor(2);
    const auto verify=[&] {
        Require(fixture.world->ReadHash(fixture.hash)==hash&&fixture.world->ReadTag()==tag&&
            Same(fixture.world->ReadActor(1),actor)&&Same(fixture.world->ReadActor(2),other),
            "Explicit outer root retained a native prefix after refusal/destruction");
    };
    {
        const auto bytes=fixture.world->RetainedBytes(),extra=fixture.world->AdditionalRootBytes();
        auto root=fixture.world->BeginRoot();++explicitRoots;
        Require(root->Active()&&fixture.world->RetainedBytes()>bytes&&fixture.world->RetainedBytes()<=bytes+extra,
            "Root snapshot omitted owned bytes or exceeded its conservative preflight envelope");
        HashAllocationFault::Enable(0u);
        const auto usage=fixture.world->ReadUsage();
        const bool membership=fixture.world->ContainsActor(1u)&&!fixture.world->ContainsActor(999u)&&
            fixture.world->ContainsHash(fixture.hash)&&!fixture.world->ContainsHash(999u);
        const auto observations=HashAllocationFault::attempts;HashAllocationFault::Disable();
        Require(membership&&observations==0u&&usage.actors==2u&&usage.hashes==1u&&usage.allocatedLinks==1024u&&
            usage.activeLinks==2u&&usage.snapshotActors==2u&&usage.snapshotHashes==1u&&
            usage.snapshotAllocatedLinks==1024u&&usage.depth==1u&&!usage.poisoned&&!usage.prepared,
            "Nonallocating membership/usage preflight fabricated records, allocated or poisoned an absent identity");
        fixture.world->Remove(fixture.hash,2,fixture.Provider());
        fixture.world->WithRoot([&](World& world) {world.SetActorLive(1,{99,88,77},true,false);});
        fixture.world->ActorLineCheck(fixture.hash,Point(0),Point(0),{},255,fixture.Provider());
        Require(root->Active(),"A successful native/WithRoot committed the explicit caller root");
        // Unrelated coordinator validation fails AFTER these native returns.
        Refuse([] {throw std::runtime_error("Late external publication refusal");});
    }
    verify();
    for(unsigned mode=0u;mode<5u;++mode) {
        auto root=fixture.world->BeginRoot();++explicitRoots;
        fixture.world->SetActorLive(1,{9,8,7},true,false);
        if(mode==0u) Refuse([&] {fixture.world->Add(fixture.hash,999,{});});
        if(mode==1u) Refuse([&] {static_cast<void>(fixture.world->ReadHash(999));});
        if(mode==2u) Refuse([&] {static_cast<void>(fixture.world->ReadActor(999));});
        if(mode==3u) Refuse([&] {static_cast<void>(fixture.world->BeginRoot());});
        if(mode==4u) Refuse([&] {fixture.world->WithRoot([&](World&) {throw std::runtime_error("External callback failure");});});
        Refuse([&] {root->PrepareCommit();},"Caught nested failure allowed explicit root preparation");
        Require(!root->CommitPrepared(),"Poisoned explicit root published without readiness");
        Require(root->Rollback()&&!root->Active(),"Poisoned explicit root could not roll back");
        verify();
    }
    {
        auto root=fixture.world->BeginRoot();++explicitRoots;
        auto provider=fixture.Provider();provider.line=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,const Vec&,std::uint32_t) {
            Refuse([&] {root->PrepareCommit();},"Live hash child allowed caller root publication");
            Require(!root->Rollback()&&!root->CommitPrepared(),"Live hash child invalidated its own backing root storage");
            return true;
        };
        Refuse([&] {fixture.world->ActorLineCheck(fixture.hash,Point(0),Point(0),{},255,provider);});
        Require(root->Rollback(),"Root with unwound child could not restore its snapshot");verify();
    }
    {
        auto root=fixture.world->BeginRoot();++explicitRoots;
        auto provider=fixture.Provider();provider.line=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,const Vec&,std::uint32_t) {
            root.reset();return true; // Must defer restoration, not dangle active actor/link references.
        };
        Refuse([&] {fixture.world->ActorLineCheck(fixture.hash,Point(0),Point(0),{},255,provider);});verify();
        auto recovered=fixture.world->BeginRoot();++explicitRoots;
        recovered->PrepareCommit();Require(recovered->CommitPrepared(),"Abandoned child root retained hidden depth/ownership");
    }
    {
        auto root=fixture.world->BeginRoot();++explicitRoots;
        fixture.world->SetActorLive(1,{9,8,7},true,false);root->PrepareCommit();
        Refuse([&] {fixture.world->SetActorLive(1,{6,5,4},true,false);},"Prepared root admitted a later mutating operation");
        Require(!root->CommitPrepared(),"Caught operation after preparation did not poison the publication ticket");
        Require(root->Rollback(),"Prepared poisoned root lost rollback ownership");verify();
    }
    {
        auto root=fixture.world->BeginRoot();++explicitRoots;
        root->PrepareCommit();Refuse([&] {root->PrepareCommit();},"Duplicate root preparation was silently accepted");
        Require(!root->CommitPrepared()&&root->Rollback(),"Repeated preparation did not preserve a poisoned undo snapshot");verify();
    }
    for(bool snapshot:{false,true}) {
        auto root=fixture.world->BeginRoot();++explicitRoots;root->PrepareCommit();
        Refuse([&] {if(snapshot) static_cast<void>(fixture.world->ReadHash(fixture.hash));
            else static_cast<void>(fixture.world->ReadActor(1));},"Prepared root admitted a throwing storage read");
        Require(!root->CommitPrepared()&&root->Rollback(),"Read after preparation did not preserve poisoned undo ownership");verify();
    }
    {
        auto root=fixture.world->BeginRoot();++explicitRoots;
        Require(!root->CommitPrepared(),"Unprepared root could publish native state");
        Refuse([&] {root->PrepareCommit();},"Unprepared commit misuse did not poison the caller root");
        Require(root->Rollback(),"Unprepared commit misuse lost undo ownership");verify();
    }
    {
        auto root=fixture.world->BeginRoot();++explicitRoots;
        fixture.world->SetActorLive(1,{7,8,9},true,false);
        fixture.world->ActorLineCheck(fixture.hash,Point(0),Point(0),{},255,fixture.Provider());
        const auto committedTag=fixture.world->ReadTag();root->PrepareCommit();
        bool committed{};HashAllocationFault::Enable(0u);
        committed=root->CommitPrepared();const auto allocations=HashAllocationFault::attempts;HashAllocationFault::Disable();
        Require(committed&&!root->Active()&&allocations==0u&&fixture.world->ReadTag()==committedTag&&
            Same(fixture.world->ReadActor(1).location,Vec{7,8,9}),"Prepared root commit allocated, threw or lost native state");
        Require(!root->CommitPrepared()&&!root->Rollback(),"Closed root reused its spent publication/undo capability");
        Refuse([&] {root->PrepareCommit();},"Closed root was prepared again");
        auto next=fixture.world->BeginRoot();++explicitRoots;
        Refuse([&] {root->PrepareCommit();},"Stale root lease authorized a different active root");
        Refuse([&] {next->PrepareCommit();},"Caught stale lease misuse did not poison the actual root");
        Require(next->Rollback(),"Stale caller token prevented the actual root rollback");
    }
    {
        auto world=std::make_unique<World>(Tables());auto root=world->BeginRoot();++explicitRoots;
        world.reset();Require(!root->Active()&&!root->CommitPrepared()&&!root->Rollback(),"Lease outliving its world used freed native storage");
        Refuse([&] {root->PrepareCommit();},"Expired world lifetime authorized root preparation");root.reset();
    }
    {
        QuestVr::OriginalHashLimits limits;limits.steps=2u;
        auto world=std::make_unique<World>(Tables(),0u,limits);Actor value;value.identity=1;world->DefineActor(value);
        auto root=world->BeginRoot();++explicitRoots;
        world->SetActorLive(1,{1,2,3},false,false);world->WithRoot([&](World& joined) {joined.SetActorLive(1,{4,5,6},false,false);});
        Refuse([&] {world->SetActorLive(1,{7,8,9},false,false);},"Each explicit-root native acquired a fresh work ledger");
        Require(world->ReadStats().steps==2u&&root->Rollback()&&Same(world->ReadActor(1).location,Vec{}),
            "Explicit-root cumulative work failure retained native effects or miscounted work");
    }
    // Enumerate every allocation position in the first explicit root's full
    // snapshot/lease creation. Each bad_alloc must leave no hidden root; a new
    // root can open and commit immediately after the allocation policy clears.
    bool reachedSuccess{};
    for(std::size_t allowed=0u;allowed<128u&&!reachedSuccess;++allowed) {
        Fixture allocation;allocation.Define(1,Bounds(0));allocation.Add(1);
        const auto before=allocation.world->ReadHash(allocation.hash);const auto beforeActor=allocation.world->ReadActor(1);
        const auto beforeTag=allocation.world->ReadTag();const auto beforeStats=allocation.world->ReadStats().steps;
        std::unique_ptr<World::RootLease> root;bool failed{};
        HashAllocationFault::Enable(allowed);
        try {root=allocation.world->BeginRoot();} catch(const std::bad_alloc&) {failed=true;}
        catch(...) {HashAllocationFault::Disable();throw;}
        HashAllocationFault::Disable();
        if(failed) {
            ++rootAllocationRefusals;
            Require(!root&&allocation.world->ReadHash(allocation.hash)==before&&
                Same(allocation.world->ReadActor(1),beforeActor)&&allocation.world->ReadTag()==beforeTag&&
                allocation.world->ReadStats().steps==beforeStats,"Root allocation failure published a partial snapshot/owner/ledger");
            root=allocation.world->BeginRoot();
        } else reachedSuccess=true;
        ++explicitRoots;root->PrepareCommit();Require(root->CommitPrepared(),"Allocation refusal left a hidden active root");
    }
    Require(reachedSuccess&&rootAllocationRefusals>=8u,"Allocation fault sweep did not cover complete root snapshot and owning lease storage");
    {
        Fixture allocation;allocation.Define(1,Bounds(0));allocation.Add(1);
        const auto before=allocation.world->ReadHash(allocation.hash);const auto beforeActor=allocation.world->ReadActor(1);
        auto root=allocation.world->BeginRoot();++explicitRoots;
        allocation.world->SetActorLive(1,{1,2,3},true,false);
        HashAllocationFault::Enable(0u);bool failed{};
        try {static_cast<void>(allocation.world->ReadHash(allocation.hash));} catch(const std::bad_alloc&) {failed=true;}
        catch(...) {HashAllocationFault::Disable();throw;}
        HashAllocationFault::Disable();
        Require(failed,"Diagnostic copy did not observe armed allocation refusal");
        Refuse([&] {root->PrepareCommit();},"Caught diagnostic allocation failure did not poison shared root");
        HashAllocationFault::Enable(0u);const bool restored=root->Rollback();
        const auto attempts=HashAllocationFault::attempts;HashAllocationFault::Disable();
        Require(restored&&attempts==0u&&allocation.world->ReadHash(allocation.hash)==before&&
            Same(allocation.world->ReadActor(1),beforeActor),"Noexcept root rollback allocated or retained native state");
    }
    bool completedMutation{};
    for(std::size_t allowed=0u;allowed<32u&&!completedMutation;++allowed) {
        Fixture allocation;allocation.Define(1,Bounds(0));allocation.Add(1);
        const auto before=allocation.world->ReadHash(allocation.hash);const auto beforeActor=allocation.world->ReadActor(1);
        auto root=allocation.world->BeginRoot();++explicitRoots;allocation.world->SetActorLive(1,{1,2,3},true,false);
        bool failed{};HashAllocationFault::Enable(allowed);
        try {static_cast<void>(allocation.world->CreateHash());} catch(const std::bad_alloc&) {failed=true;}
        catch(...) {HashAllocationFault::Disable();throw;}
        HashAllocationFault::Disable();
        if(failed) {++rootAllocationRefusals;Refuse([&] {root->PrepareCommit();},"Caught hash-allocation refusal did not poison owning root");}
        else completedMutation=true;
        Require(root->Rollback()&&allocation.world->ReadHash(allocation.hash)==before&&
            Same(allocation.world->ReadActor(1),beforeActor)&&allocation.world->ReadUsage().hashes==1u,
            "Allocation failure/success rollback retained a hash birth or earlier actor mutation");
    }
    Require(completedMutation,"Mutation allocation-fault sweep never reached the whole successful operation");
}
std::uint32_t Next(std::uint32_t& seed) {seed^=seed<<13u;seed^=seed>>17u;seed^=seed<<5u;return seed;}
void Randomized() {
    Fixture fixture;std::uint32_t seed=0x9878abcd;
    for(unsigned id=1;id<=16u;++id) fixture.Define(id,Bounds(static_cast<int>(id%4u),static_cast<int>(id/4u%4u)));
    for(unsigned iteration=0;iteration<600u;++iteration) {
        const auto id=Next(seed)%16u+1u;
        switch(Next(seed)%4u) {
        case 0:fixture.Add(id);break;
        case 1:fixture.Remove(id);break;
        case 2:{const auto x=static_cast<int>(Next(seed)%4u),y=static_cast<int>(Next(seed)%4u),z=static_cast<int>(Next(seed)%4u);
            fixture.boxes[id]={Point(x,y,z),Point(x+static_cast<int>(Next(seed)%2u),y,z)};
            fixture.world->SetActorLive(id,fixture.boxes[id].minimum,true,false);break;}
        default:fixture.CompareQuery(Point(4,4,4),Point(0,0,0));break;
        }
        fixture.Check();++randomOperations;
    }
    Require(queryComparisons>100u,"Randomized hash state did not exercise enough independent queries");
}
void CylinderAndCollector() {
    // Actual independently tested scalar geometry, not Fill/mock hits. This
    // deliberately combines pure services, not original actor/Model loading.
    auto tables=Tables();auto world=std::make_unique<World>(std::move(tables));const auto hash=world->CreateHash();
    std::unordered_map<std::uintptr_t,QuestVr::OriginalActorCylinder> shapes;
    for(std::uintptr_t id=1;id<=70u;++id) {
        shapes.emplace(id,QuestVr::OriginalActorCylinder{id,{static_cast<float>(id%7u)/16.0f,0,0},2,3});
        Actor actor;actor.identity=id;actor.location=shapes.at(id).location;actor.collideActors=true;world->DefineActor(actor);
    }
    unsigned calls{};Queries providers;
    providers.bounds=[&](std::uintptr_t id) {const auto& shape=shapes.at(id);
        return QuestVr::OriginalPrimitiveCollisionBounds(shape.location,shape.radius,shape.height);};
    providers.line=[&](Hit& hit,std::uintptr_t id,const Vec& end,const Vec& start,const Vec& extent,std::uint32_t flags) {
        Require(flags==17u&&hit.item==-1&&Same(hit.time,0),"Integrated primitive lost byte flags/original hash seed");++calls;
        return QuestVr::CheckOriginalActorCylinder(hit,shapes.at(id),end,start,extent);
    };
    for(std::uintptr_t id=1;id<=70u;++id) world->Add(hash,id,providers);
    const auto hits=world->ActorLineCheck(hash,{5,0,0},{-5,0,0},{.1f,.1f,.1f},17,providers);
    Require(calls==70u&&hits.records.size()==70u&&hits.first==70u,"Actor hash silently inherited collector's64-hit cap");
    std::uintptr_t current=hits.first;std::uintptr_t expected=1u;
    while(current) {
        const auto& hit=hits.records.at(current-1u);Require(hit.actor==expected&&hit.primitive==0u&&hit.item==-1&&
            hit.time>0&&hit.time<1&&hit.normal.x<0,"Actual cylinder hit lost complete packet/reverse encounter order");
        Hit standalone;standalone.time=0;QuestVr::CheckOriginalActorCylinder(standalone,shapes.at(expected),{5,0,0},{-5,0,0},{.1f,.1f,.1f});
        standalone.next=hit.next;Require(Same(hit,standalone),"Hash geometry packet differs from standalone cylinder query");
        ++expected;current=hit.next;
    }
    Require(expected==71u,"Reversed real primitive hit list lost an actor");
    calls=0;QuestVr::OriginalMultiLineQueries collector;
    collector.actors=[&](const Vec& end,const Vec& start,const Vec& extent,std::uint8_t flags) {
        return world->ActorLineCheck(hash,end,start,extent,flags,providers);};
    const auto limited=QuestVr::CollectOriginalMultiLineHits({5,0,0},{-5,0,0},{.1f,.1f,.1f},1,0,17,collector);
    Require(calls==70u&&limited.hits.records.size()==64u&&limited.stats.actorVisits==64u,
        "Collector64 cap incorrectly truncated primitive hash query work");
    std::unordered_set<std::uintptr_t> accepted;
    float previous=-1;
    for(const auto& hit:limited.hits.records) {
        accepted.insert(hit.actor);Require(hit.time>=previous,"Integrated collector did not sort actual cylinder times");previous=hit.time;
    }
    for(std::uintptr_t id=1;id<=64u;++id) Require(accepted.contains(id),"Collector cap sorted before admission and changed first64 input actors");
    Require(!accepted.contains(65u)&&!accepted.contains(70u),"Collector admitted ignored tail of native70-hit list");
}
} // namespace
int main() {
    try {Registration();CandidateOrder();Reentrance();Rollback();ExplicitRoots();Randomized();CylinderAndCollector();
        std::cout<<"PASS original collision hash state "<<checks<<" checks, "<<refusals<<" root refusals, "<<randomOperations
            <<" independent mutation operations, "<<queryComparisons<<" vector-oracle queries, "<<explicitRoots<<" explicit roots, "
            <<rootAllocationRefusals<<" root allocation refusals; nested tag/live Next/rollback verified.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Original collision hash state failed: "<<error.what()<<'\n';return 1;}
}
