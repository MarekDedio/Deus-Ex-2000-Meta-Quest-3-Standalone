#include "quest_collision_hash.h"
#include "quest_collision_hash_oracle.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <new>

// Faults are confined to this executable. The original DLL is never loaded.
namespace EncroachmentAllocationFault {
thread_local std::size_t remaining=std::numeric_limits<std::size_t>::max(),attempts{};
void* Allocate(std::size_t bytes) {
    ++attempts;
    if(!remaining) throw std::bad_alloc();
    if(remaining!=std::numeric_limits<std::size_t>::max()) --remaining;
    if(auto* memory=std::malloc(bytes?bytes:1u)) return memory;
    throw std::bad_alloc();
}
void Enable(std::size_t allowed) noexcept {remaining=allowed;attempts=0u;}
void Disable() noexcept {remaining=std::numeric_limits<std::size_t>::max();}
}
void* operator new(std::size_t bytes) {return EncroachmentAllocationFault::Allocate(bytes);}
void* operator new[](std::size_t bytes) {return EncroachmentAllocationFault::Allocate(bytes);}
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
using World=QuestVr::OriginalCollisionHashWorld;
using Actor=QuestVr::OriginalHashActor;
using Hit=QuestVr::OriginalActorLineHit;
using Arena=QuestVr::OriginalLineHitArena;
using Queries=QuestVr::OriginalHashEncroachmentQueries;
using Cylinder=QuestVr::OriginalActorCylinder;
using VectorOracle=QuestVr::CollisionHashOracle::Oracle;
std::size_t checks{},refusals{},comparisons{},randomQueries{},allocationRefusals{};
std::size_t argumentWordPackets{},hardwareWordStores{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
template<class F> void Refuse(F&& run,const char* reason="Unsupported encroachment silently succeeded") {
    bool failed{};try {run();}catch(const std::exception&) {failed=true;}
    Require(failed,reason);++refusals;
}
bool Same(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec a,Vec b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Actor& a,const Actor& b) {
    return a.identity==b.identity&&Same(a.location,b.location)&&Same(a.colLocation,b.colLocation)&&
        a.collideActors==b.collideActors&&a.pendingDelete==b.pendingDelete&&
        a.collisionTag==b.collisionTag&&a.storageGeneration==b.storageGeneration;
}
bool Same(const Hit& a,const Hit& b) {
    return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&a.item==b.item&&
        Same(a.location,b.location)&&Same(a.normal,b.normal)&&Same(a.time,b.time);
}
bool Same(const Arena& a,const Arena& b) {
    if(a.first!=b.first||a.records.size()!=b.records.size()) return false;
    for(std::size_t i=0u;i<a.records.size();++i)if(!Same(a.records[i],b.records[i])) return false;
    return true;
}
QuestVr::OriginalCollisionHashTables Tables(bool identity=false) {
    // Independent CRT recurrence and fixed-range X/Y/Z swaps. This is an
    // explicitly synthetic process seed, not a guessed real game RNG state.
    QuestVr::OriginalCollisionHashTables result;
    for(std::size_t i=0u;i<result.x.size();++i)result.x[i]=result.y[i]=result.z[i]=static_cast<std::uint32_t>(i);
    if(identity) return result;
    std::uint32_t seed=0xc5a3b917u;
    const auto draw=[&] {seed=seed*214013u+2531011u;return ((seed>>16u)&0x7fffu)%16384u;};
    for(std::size_t i=0u;i<result.x.size();++i) {
        std::swap(result.x[i],result.x[draw()]);
        std::swap(result.y[i],result.y[draw()]);
        std::swap(result.z[i],result.z[draw()]);
    }
    return result;
}
struct CellBox {std::array<int,3u> low{},high{};};
CellBox Cell(int x,int y=0,int z=0) {return {{x,y,z},{x,y,z}};}
Vec Point(const std::array<int,3u>& cell) {
    return {static_cast<float>(cell[0])*256.0f-32768.0f,static_cast<float>(cell[1])*256.0f-32768.0f,
        static_cast<float>(cell[2])*256.0f-32768.0f};
}
Box Bounds(const CellBox& box) {return {Point(box.low),Point(box.high)};}
std::vector<VectorOracle::Cell> Cells(const CellBox& range,const QuestVr::OriginalCollisionHashTables& tables) {
    std::vector<VectorOracle::Cell> result;
    for(auto x=range.low[0];x<=range.high[0];++x)
        for(auto y=range.low[1];y<=range.high[1];++y)
            for(auto z=range.low[2];z<=range.high[2];++z) {
                // No production grid, range, hashing, or encoded-cell helper.
                result.emplace_back(tables.x[static_cast<std::size_t>(x)]^tables.y[static_cast<std::size_t>(y)]^
                    tables.z[static_cast<std::size_t>(z)],static_cast<std::uint32_t>(z)*65536u+
                    static_cast<std::uint32_t>(y)*256u+static_cast<std::uint32_t>(x));
            }
    return result;
}
void Seed(const Hit& hit) {
    Require(hit.next==0u&&hit.actor==0u&&hit.primitive==0u&&hit.item==-1&&Same(hit.time,1.0f)&&
        Same(hit.location,Vec{})&&Same(hit.normal,Vec{}),"Encroachment PointCheck seed is not the complete original Time1/Item-1 packet");
}
void Fill(Hit& hit,std::uintptr_t id) {
    if(id%3u==0u) return; // Untouched provider fields must remain initialized.
    hit.next=0xabababu;hit.actor=9000u+id;hit.primitive=8000u+id;
    hit.location={static_cast<float>(id),-0.0f,static_cast<float>(id)+.25f};
    hit.normal={-1.0f,.125f,0};hit.time=static_cast<float>(id)/128.0f;
    hit.item=static_cast<std::int32_t>(id)+41;
}
void Append(Arena& result,Hit hit,std::uintptr_t actor) {
    hit.actor=actor;hit.primitive=0u;hit.next=0u;
    if(result.records.empty())result.first=1u;else result.records.back().next=result.records.size()+1u;
    result.records.push_back(hit);
}
struct Fixture {
    std::unique_ptr<QuestVr::OriginalCollisionHashTables> tables=std::make_unique<QuestVr::OriginalCollisionHashTables>();
    std::unique_ptr<World> world;
    std::unique_ptr<VectorOracle> oracle=std::make_unique<VectorOracle>();
    std::uint64_t hash{};
    std::map<std::uintptr_t,CellBox> boxes;
    std::map<std::uintptr_t,Cylinder> cylinders;
    std::map<std::uintptr_t,bool> moving,clear;
    std::map<std::uintptr_t,std::uint32_t> tags;
    std::vector<std::uintptr_t> gates,samples,points;
    std::uintptr_t source{1000u};
    std::uint32_t tag{};
    explicit Fixture(std::uint32_t initial=0u,const QuestVr::OriginalHashLimits& limits={},bool identity=false):tag(initial) {
        *tables=Tables(identity);world=std::make_unique<World>(*tables,initial,limits);hash=world->CreateHash();
        Define(source,Cell(0));
    }
    void Define(std::uintptr_t id,CellBox box,std::uint32_t initialTag=0u) {
        Actor actor;actor.identity=id;actor.location={static_cast<float>(id)+.125f,-static_cast<float>(id),.5f};
        actor.colLocation={71,72,73};actor.collideActors=true;actor.collisionTag=initialTag;
        world->DefineActor(actor);boxes[id]=box;tags[id]=initialTag;
        // Returned shape identity is deliberately unrelated: the link owns ID.
        cylinders[id]={id+7000u,actor.location,static_cast<float>(id)+.25f,static_cast<float>(id)+.75f};
        moving[id]=false;clear[id]=false;
    }
    QuestVr::OriginalHashQueries Registration() {
        QuestVr::OriginalHashQueries query;query.bounds=[&](std::uintptr_t id){return Bounds(boxes.at(id));};return query;
    }
    void Add(std::uintptr_t id) {
        world->Add(hash,id,Registration());oracle->Add(id,Cells(boxes.at(id),*tables));
    }
    void Remove(std::uintptr_t id) {
        world->Remove(hash,id,Registration());std::vector<std::uint32_t> buckets;
        for(auto [bucket,cell]:Cells(boxes.at(id),*tables)) {static_cast<void>(cell);buckets.push_back(bucket);}
        oracle->Remove(id,buckets);
    }
    std::uintptr_t Identify(const Vec& location,const Vec& extent) const {
        for(const auto& [id,shape]:cylinders)if(Same(location,shape.location)&&
            Same(extent,Vec{shape.radius,shape.radius,shape.height}))return id;
        throw std::runtime_error("Source PointCheck received source dimensions, a wrong candidate, or reordered cylinder components");
    }
    Queries Provider() {
        Queries query;
        query.sourceBounds=[&](std::uintptr_t id) {Require(id==source,"Encroachment requested candidate rather than source bounds");return Bounds(boxes.at(id));};
        query.movingBrush=[&](std::uintptr_t id) {gates.push_back(id);
            Require(world->ReadActor(id).collisionTag==world->ReadTag(),"Moving-brush gate ran before current shared tag store");return moving.at(id);};
        query.candidateCylinder=[&](std::uintptr_t id) {
            Require(!gates.empty()&&gates.back()==id,"Candidate sampling ran before its moving-brush gate");
            samples.push_back(id);return cylinders.at(id);
        };
        query.sourcePoint=[&](Hit& hit,std::uintptr_t owner,const Vec& location,const Vec& extent,std::uint32_t flags) {
            Seed(hit);Require(owner==source&&flags==0u,"PointCheck did not target the source primitive with literal flags0");
            const auto id=Identify(location,extent);
            Require(!samples.empty()&&samples.back()==id,"Source PointCheck ran before candidate sampling");
            points.push_back(id);Fill(hit,id);return clear.at(id);
        };
        return query;
    }
    Arena Query(const Queries& query) {return world->ActorEncroachmentCheck(hash,source,query);}
    Arena Query() {return Query(Provider());}
    void CompareQuery() {
        gates.clear();samples.clear();points.clear();const auto result=Query();Arena expected;
        std::vector<std::uintptr_t> expectedGates,expectedPoints;
        std::size_t links{},cells{};++tag;
        for(auto [bucket,cell]:Cells(boxes.at(source),*tables)) {
            ++cells;auto current=oracle->Head(bucket);
            while(current) {
                ++links;const auto slot=oracle->Slot(current);
                Require(slot.initialized,"Independent encroachment encountered undefined link payload");
                if(tags.at(slot.actor)!=tag&&slot.cell==cell) {
                    tags[slot.actor]=tag;expectedGates.push_back(slot.actor);
                    if(!moving.at(slot.actor)&&slot.actor!=source) {
                        expectedPoints.push_back(slot.actor);Hit hit;Fill(hit,slot.actor);
                        if(!clear.at(slot.actor))Append(expected,hit,slot.actor);
                    }
                }
                current=oracle->Next(current);
            }
        }
        const auto stats=world->ReadStats();
        Require(world->ReadTag()==tag&&Same(result,expected)&&gates==expectedGates&&samples==expectedPoints&&points==expectedPoints,
            "Encroachment packet/tail order or gate/sample encounter differs from independent CRT/vector oracle");
        Require(stats.cells==cells&&stats.links==links&&stats.primitiveCalls==expectedPoints.size()&&stats.hits==expected.records.size(),
            "Encroachment cell/link/primitive/hit census differs from independent vector traversal");
        for(const auto& [id,value]:tags)Require(world->ReadActor(id).collisionTag==value,"Encroachment tag state differs from independent shared32-bit oracle");
        ++comparisons;
    }
    void CompareStorage() const {
        oracle->Validate();const auto actual=world->ReadHash(hash);
        Require(actual.links.size()==oracle->SlotsView().size()&&actual.activeLinks==oracle->LinkCount(),"Encroachment callback changed hash chunk/membership ownership");
        for(std::size_t bucket=0u;bucket<actual.heads.size();++bucket)
            Require(actual.heads[bucket]==oracle->Head(static_cast<std::uint32_t>(bucket)),"Encroachment callback retained an incorrect bucket head");
        Require(actual.free==(oracle->FreeView().empty()?0u:oracle->FreeView().front()),"Encroachment callback retained an incorrect free head");
        for(std::size_t id=1u;id<actual.links.size();++id) {
            const auto& slot=oracle->Slot(id);const auto& link=actual.links[id];
            Require(link.actor==slot.actor&&link.cell==slot.cell&&link.initialized==slot.initialized&&link.next==oracle->Next(id),
                "Encroachment live-link state differs from independent vector membership");
        }
    }
};
struct Snapshot {
    QuestVr::OriginalHashStorage hash;
    std::vector<Actor> actors;
    std::uint32_t tag{};
    std::uint64_t generation{};
    explicit Snapshot(const Fixture& fixture):hash(fixture.world->ReadHash(fixture.hash)),tag(fixture.world->ReadTag()),
        generation(fixture.world->ReadUsage().nextGeneration) {
        for(const auto& [id,box]:fixture.boxes) {static_cast<void>(box);actors.push_back(fixture.world->ReadActor(id));}
    }
    void Check(const Fixture& fixture) const {
        Require(fixture.world->ReadHash(fixture.hash)==hash&&fixture.world->ReadTag()==tag&&
            fixture.world->ReadUsage().nextGeneration==generation&&fixture.world->ReadUsage().actors==actors.size()&&
            fixture.world->ReadUsage().hashes==1u,"Encroachment refusal retained hash/tag/generation/storage effects");
        for(const auto& actor:actors)Require(Same(fixture.world->ReadActor(actor.identity),actor),"Encroachment refusal retained actor/tag/ColLocation effects");
    }
};
std::uint32_t IndependentFloatLoadStoreWord(std::uint32_t raw,std::uint32_t golden) {
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    static_cast<void>(golden);
    // Original10358F26..44 executes FLD32/FST32 for extent Y/Z, but X is a
    // plain DWORD move. Use real x87 instructions, not the production bit
    // converter or C++ NaN classification. FNSTENV masks exceptions; restore
    // the complete prior environment so the masked signaling-NaN status is
    // neither exported nor left pending in the rest of the test executable.
    alignas(16) std::array<std::uint32_t,7u> environment{};
    std::uint32_t stored{};
    asm volatile("fnstenv %0":"=m"(environment)::"memory");
    asm volatile("flds %1; fstps %0":"=m"(stored):"m"(raw):"st","memory");
    asm volatile("fldenv %0"::"m"(environment):"memory");
    ++hardwareWordStores;return stored;
#else
    // ARM syntax checks and deterministic controls still exercise the same
    // observed argument words, but do not claim an independent x87 execution.
    static_cast<void>(raw);return golden;
#endif
}
void CandidateArgumentWordStores() {
    struct Word {std::uint32_t raw,stored;};
    // Literal positive/negative goldens include quiet/signaling NaN payloads,
    // infinities, both subnormal boundaries, normals and both signed zeros.
    // Signaling NaNs quiet on the Float store; all other words are unchanged.
    constexpr std::array words{
        Word{0x00000000u,0x00000000u},Word{0x80000000u,0x80000000u},
        Word{0x00000001u,0x00000001u},Word{0x80000001u,0x80000001u},
        Word{0x007fffffu,0x007fffffu},Word{0x807fffffu,0x807fffffu},
        Word{0x00800000u,0x00800000u},Word{0x80800000u,0x80800000u},
        Word{0x3f800000u,0x3f800000u},Word{0xbf800000u,0xbf800000u},
        Word{0x7f7fffffu,0x7f7fffffu},Word{0xff7fffffu,0xff7fffffu},
        Word{0x7f800000u,0x7f800000u},Word{0xff800000u,0xff800000u},
        Word{0x7fc00000u,0x7fc00000u},Word{0xffc00000u,0xffc00000u},
        Word{0x7fe12345u,0x7fe12345u},Word{0xffe12345u,0xffe12345u},
        Word{0x7f800001u,0x7fc00001u},Word{0xff800001u,0xffc00001u},
        Word{0x7fa12345u,0x7fe12345u},Word{0xffa12345u,0xffe12345u},
        Word{0x7fbfffffu,0x7fffffffu},Word{0xffbfffffu,0xffffffffu}};
    std::array<std::uint32_t,words.size()> independent{};
    for(std::size_t i=0u;i<words.size();++i) {
        independent[i]=IndependentFloatLoadStoreWord(words[i].raw,words[i].stored);
        Require(independent[i]==words[i].stored,"Independent x87 Float load/store disagrees with literal exceptional-word golden");
    }
    Fixture fixture;fixture.Define(1u,Cell(0));fixture.Add(1u);auto query=fixture.Provider();
    std::uint32_t radius{},height{};std::array<std::uint32_t,3u> captured{};unsigned calls{};
    const Vec location{-0.0f,17.0f,-23.0f};
    query.candidateCylinder=[&](std::uintptr_t id) {
        Require(id==1u,"Argument-word matrix sampled a different candidate");
        return Cylinder{999u,location,std::bit_cast<float>(radius),std::bit_cast<float>(height)};
    };
    query.sourcePoint=[&](Hit& hit,std::uintptr_t source,const Vec& point,const Vec& extent,std::uint32_t flags) {
        Seed(hit);Require(source==fixture.source&&flags==0u&&Same(point,location),
            "Argument-word matrix changed source/location/flags before PointCheck");
        captured={std::bit_cast<std::uint32_t>(extent.x),std::bit_cast<std::uint32_t>(extent.y),
            std::bit_cast<std::uint32_t>(extent.z)};++calls;return false;
    };
    for(std::size_t r=0u;r<words.size();++r)for(std::size_t h=0u;h<words.size();++h) {
        radius=words[r].raw;height=words[h].raw;calls=0u;captured={};const auto result=fixture.Query(query);
        Require(calls==1u&&captured==std::array<std::uint32_t,3u>{radius,independent[r],independent[h]},
            "Encroachment did not copy Radius X raw while Float-loading/storing Radius Y and Height Z");
        Require(result.first==1u&&result.records.size()==1u&&result.records[0].actor==1u&&
            result.records[0].primitive==0u&&Same(result.records[0].time,1.0f)&&result.records[0].item==-1,
            "Argument-word matrix imposed a generic numeric gate or changed untouched provider packet fields");
        ++argumentWordPackets;
    }
    Require(argumentWordPackets==words.size()*words.size(),"Argument-word matrix did not cover every independent radius/height pair");
}
void DirectionSeedsAndOrder() {
    Fixture fixture;fixture.boxes[fixture.source]={{0,0,0},{3,2,1}};
    for(std::uintptr_t id=1u;id<=75u;++id) {
        fixture.Define(id,Cell(static_cast<int>(id%4u),static_cast<int>(id/4u%3u),static_cast<int>(id/12u%2u)));
        fixture.clear[id]=id%5u==0u;fixture.moving[id]=id%11u==0u;fixture.Add(id);
        if(id%7u==0u)fixture.Add(id); // Duplicate membership, not duplicate output.
    }
    fixture.Add(fixture.source);
    fixture.world->SetActorLive(1u,{9,8,7},false,true); // Membership, not live actor flags, admits a candidate.
    fixture.world->SetActorLive(fixture.source,{6,5,4},false,true);
    const auto source=fixture.world->ReadActor(fixture.source);
    fixture.CompareQuery();Require(fixture.points.size()>50u,"Ordinary encroachment controls did not exercise enough candidate packets");
    Require(Same(fixture.world->ReadActor(fixture.source).location,source.location)&&
        Same(fixture.world->ReadActor(fixture.source).colLocation,source.colLocation),"Hash-only encroachment fabricated a source pose or registration refresh");
    Require(std::find(fixture.gates.begin(),fixture.gates.end(),fixture.source)!=fixture.gates.end()&&
        std::find(fixture.samples.begin(),fixture.samples.end(),fixture.source)==fixture.samples.end(),"Self gate ran before moving-brush callback or self reached PointCheck");
    fixture.CompareStorage();
    // Dispatcher policy must not preempt the actual primitive's active-field
    // policy. Quiet NaN radii can select Model's zero/unordered branch; signed
    // dimensions likewise belong to PointCheck, not a generic hash filter.
    Fixture raw;raw.Define(1u,Cell(0));raw.Add(1u);auto rawQuery=raw.Provider();
    const float nan=std::bit_cast<float>(std::uint32_t{0xffc12345u});unsigned rawCalls{};
    rawQuery.candidateCylinder=[&](std::uintptr_t) {return Cylinder{999u,{nan,-0.0f,7.0f},nan,-2.0f};};
    rawQuery.sourcePoint=[&](Hit& hit,std::uintptr_t owner,const Vec& p,const Vec& e,std::uint32_t flags) {
        Seed(hit);++rawCalls;Require(owner==raw.source&&flags==0u&&Same(p,Vec{nan,-0.0f,7.0f})&&
            Same(e,Vec{nan,nan,-2.0f}),"Hash dispatcher changed raw candidate words or imposed a primitive numeric policy");return false;
    };
    const auto rawResult=raw.Query(rawQuery);
    Require(rawCalls==1u&&rawResult.records.size()==1u&&rawResult.records[0].actor==1u,
        "Hash dispatcher rejected primitive-owned exceptional candidate fields");
    // No collector64 cap and no time sort: provider times are deliberately
    // unrelated to hash encounter order; initialized fields are also retained.
    Fixture uncapped;
    for(std::uintptr_t id=1u;id<=70u;++id) {uncapped.Define(id,Cell(0));uncapped.Add(id);}
    const auto result=uncapped.Query();Require(result.records.size()==70u&&result.first==1u,"Actor encroachment inherited a collector64 cap or prepend order");
    for(std::size_t i=0u;i<result.records.size();++i) {
        Hit expected;const auto id=70u-i;Fill(expected,id);expected.actor=id;expected.primitive=0u;expected.next=i+1u<70u?i+2u:0u;
        Require(Same(result.records[i],expected),"Encroachment did not retain provider packet fields in forward tail encounter order");
    }
}
void BoundsTagsAndInactiveProviders() {
    Fixture fixture;fixture.Define(1u,Cell(0));fixture.Add(1u);auto query=fixture.Provider();
    unsigned bounds{};query.sourceBounds=[&](std::uintptr_t id) {
        ++bounds;Require(id==fixture.source&&fixture.world->ReadTag()==0u,"Encroachment incremented tag before source bounds");
        return Bounds(Cell(0));
    };
    fixture.Query(query);Require(bounds==1u&&fixture.world->ReadTag()==1u,"Encroachment bounds/tag entry order differs");
    Fixture empty(17u);empty.Define(1u,Cell(0));empty.Add(1u);Queries onlyBounds;
    onlyBounds.sourceBounds=[&](std::uintptr_t) {return Bounds({{1,0,0},{0,0,0}});};
    const auto none=empty.Query(onlyBounds);Require(none.records.empty()&&none.first==0u&&empty.world->ReadTag()==18u&&
        empty.world->ReadStats().cells==0u&&empty.world->ReadActor(1u).collisionTag==0u,
        "Empty inverted range did not increment tag or inspected inactive providers");
    Fixture self;self.Add(self.source);Queries selfQuery;unsigned gates{};
    selfQuery.sourceBounds=[&](std::uintptr_t) {return Bounds(Cell(0));};
    selfQuery.movingBrush=[&](std::uintptr_t id) {++gates;Require(id==self.source&&self.world->ReadActor(id).collisionTag==self.world->ReadTag(),
        "Self was not tagged before the moving-brush predicate");return false;};
    Require(self.Query(selfQuery).records.empty()&&gates==1u,"Self-only query required inactive cylinder/PointCheck provider");
    Fixture moving;moving.Define(1u,Cell(0));moving.Add(1u);selfQuery.sourceBounds=[&](std::uintptr_t){return Bounds(Cell(0));};
    selfQuery.movingBrush=[&](std::uintptr_t id){Require(id==1u&&moving.world->ReadActor(id).collisionTag==moving.world->ReadTag(),
        "Moving brush was not tagged before exclusion");return true;};
    Require(moving.Query(selfQuery).records.empty(),"Excluded moving brush required inactive cylinder/PointCheck provider");
    Fixture wrong(0u,{},true);wrong.Define(1u,Cell(1,0));wrong.Define(2u,Cell(0,1));wrong.Add(1u);wrong.Add(2u);
    wrong.boxes[wrong.source]={{0,0,0},{1,1,0}};wrong.CompareQuery();
    Require(wrong.points==std::vector<std::uintptr_t>{2u,1u},"Wrong encoded-cell encounter tagged an actor before its matching cell");
    Fixture wrap(std::numeric_limits<std::uint32_t>::max());wrap.Define(1u,Cell(0));wrap.Add(1u);wrap.CompareQuery();
    Require(wrap.points.empty()&&wrap.world->ReadTag()==0u,"32-bit tag wrap reset stored actor tags");
    wrap.CompareQuery();Require(wrap.points==std::vector<std::uintptr_t>{1u},"Post-wrap encroachment did not resume actor admission");
}
void ReentranceAndLiveLinks() {
    Fixture nested;nested.Define(1u,Cell(0));nested.Define(2u,Cell(0));nested.Add(2u);nested.Add(1u);
    auto query=nested.Provider();bool entered{};std::vector<std::uintptr_t> trace;
    query.sourcePoint=[&](Hit& hit,std::uintptr_t source,const Vec& p,const Vec& e,std::uint32_t flags) {
        Seed(hit);Require(source==nested.source&&flags==0u,"Nested point dispatch changed source or flags");
        const auto id=nested.Identify(p,e);trace.push_back(id);Fill(hit,id);
        if(!entered) {entered=true;const auto inner=nested.Query(query);Require(inner.records.size()==2u,"Nested encroachment did not use current shared tag");}
        return false;
    };
    const auto outer=nested.Query(query);
    Require(trace==std::vector<std::uintptr_t>{1u,1u,2u}&&outer.records.size()==1u&&outer.records[0].actor==1u&&
        nested.world->ReadTag()==2u&&nested.world->ReadActor(2u).collisionTag==2u,"Outer encroachment cached its initial tag across nested query");
    // A nested EMPTY query changes the process tag without marking any actor.
    // The duplicate source link must then be admitted again, not deduplicated
    // by an invented query-local visited set.
    Fixture duplicate;duplicate.Define(1u,Cell(0));duplicate.Add(1u);duplicate.Add(1u);entered=false;trace.clear();
    query=duplicate.Provider();query.sourcePoint=[&](Hit& hit,std::uintptr_t,const Vec& p,const Vec& e,std::uint32_t) {
        const auto id=duplicate.Identify(p,e);trace.push_back(id);Fill(hit,id);
        if(!entered) {entered=true;Queries empty;empty.sourceBounds=[](std::uintptr_t){return Bounds({{1,0,0},{0,0,0}});};duplicate.Query(empty);}
        return false;
    };
    Require(duplicate.Query(query).records.size()==2u&&trace==std::vector<std::uintptr_t>{1u,1u}&&duplicate.world->ReadTag()==2u,
        "Nested empty query did not expose live global-tag duplicate admission");
    Fixture removed;removed.Define(1u,Cell(0));removed.Define(2u,Cell(0));removed.Add(2u);removed.Add(1u);trace.clear();
    query=removed.Provider();query.sourcePoint=[&](Hit& hit,std::uintptr_t,const Vec& p,const Vec& e,std::uint32_t) {
        const auto id=removed.Identify(p,e);trace.push_back(id);Fill(hit,id);if(id==1u)removed.Remove(2u);return false;
    };
    Require(removed.Query(query).records.size()==1u&&trace==std::vector<std::uintptr_t>{1u},"Encroachment cached Next before callback removed its follower");removed.CompareStorage();
    // Exhaust a full chunk so removing/reusing the CURRENT slot leaves no
    // undefined free-slot metadata in the live traversal. Native10358FA7 also
    // rereads this slot's Actor AFTER PointCheck, before its live Next read.
    Fixture reused;for(const auto id:{1u,2u,3u})reused.Define(id,Cell(0));
    for(unsigned i=0u;i<1023u;++i)reused.Add(2u);
    reused.Add(1u);trace.clear();bool changed{};query=reused.Provider();
    query.sourcePoint=[&](Hit& hit,std::uintptr_t,const Vec& p,const Vec& e,std::uint32_t) {
        const auto id=reused.Identify(p,e);trace.push_back(id);Fill(hit,id);
        if(!changed) {changed=true;reused.Remove(1u);reused.Add(3u);}return false;
    };
    const auto replaced=reused.Query(query);
    Require(trace==std::vector<std::uintptr_t>{1u,2u}&&replaced.records.size()==2u&&replaced.records[0].actor==3u&&
        replaced.records[0].item==42&&replaced.records[1].actor==2u,"Encroachment cached current Actor or Next across exact-slot reuse");reused.CompareStorage();
    Fixture later(0u,{},true);later.boxes[later.source]={{0,0,0},{1,1,0}};
    later.Define(1u,Cell(0,1));later.Define(2u,Cell(1,0));later.Add(1u);trace.clear();query=later.Provider();
    query.sourcePoint=[&](Hit& hit,std::uintptr_t,const Vec& p,const Vec& e,std::uint32_t) {
        const auto id=later.Identify(p,e);trace.push_back(id);Fill(hit,id);if(id==1u)later.Add(2u);return false;
    };
    Require(later.Query(query).records.size()==2u&&trace==std::vector<std::uintptr_t>{1u,2u},"Later aliased cell did not fetch the live bucket head");later.CompareStorage();
    Fixture boundsNested;boundsNested.Define(1u,Cell(0));boundsNested.Add(1u);entered=false;query=boundsNested.Provider();
    query.sourceBounds=[&](std::uintptr_t) {
        Require(boundsNested.world->ReadTag()==0u,"Bounds callback began after outer tag increment");
        QuestVr::OriginalHashQueries line;line.line=[](Hit&,std::uintptr_t,const Vec&,const Vec&,const Vec&,std::uint32_t){return true;};
        const auto p=Point({0,0,0});boundsNested.world->ActorLineCheck(boundsNested.hash,p,p,{},0u,line);return Bounds(Cell(0));
    };
    Require(boundsNested.Query(query).records.size()==1u&&boundsNested.world->ReadTag()==2u&&
        boundsNested.world->ReadActor(1u).collisionTag==2u,"Encroachment advanced a cached pre-bounds tag instead of the shared post-bounds tag");
    Fixture cross;cross.Define(1u,Cell(0));cross.Add(1u);const auto second=cross.world->CreateHash();
    cross.world->Add(second,1u,cross.Registration());query=cross.Provider();entered=false;trace.clear();
    query.sourcePoint=[&](Hit& hit,std::uintptr_t,const Vec& p,const Vec& e,std::uint32_t) {
        const auto id=cross.Identify(p,e);trace.push_back(id);Fill(hit,id);
        if(!entered) {entered=true;const auto inner=cross.world->ActorEncroachmentCheck(second,cross.source,query);
            Require(inner.records.size()==1u,"Second native hash did not join shared encroachment domain");}
        return false;
    };
    Require(cross.Query(query).records.size()==1u&&trace==std::vector<std::uintptr_t>{1u,1u}&&
        cross.world->ReadTag()==2u&&cross.world->ReadActor(1u).collisionTag==2u,
        "Separate native hashes acquired private encroachment tags or actor state");
}
void RollbackAndGenerations() {
    Fixture fixture;fixture.Define(1u,Cell(0));fixture.Define(2u,Cell(0));fixture.Add(2u);fixture.Add(1u);const Snapshot before(fixture);
    for(unsigned phase=0u;phase<4u;++phase) {
        auto query=fixture.Provider();const auto fail=[&] {
            fixture.world->SetActorLive(fixture.source,{99,88,77},false,true);fixture.world->Remove(fixture.hash,2u,fixture.Registration());
            throw std::runtime_error("Explicit provider refusal");
        };
        if(phase==0u)query.sourceBounds=[&](std::uintptr_t)->Box{fail();return {};};
        if(phase==1u)query.movingBrush=[&](std::uintptr_t)->bool{fail();return false;};
        if(phase==2u)query.candidateCylinder=[&](std::uintptr_t)->Cylinder{fail();return {};};
        if(phase==3u)query.sourcePoint=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,std::uint32_t)->bool{fail();return false;};
        Refuse([&]{fixture.Query(query);});before.Check(fixture);
    }
    for(unsigned mode=0u;mode<3u;++mode) {
        auto query=fixture.Provider();query.sourcePoint=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,std::uint32_t) {
            try {if(mode==0u)fixture.world->Add(fixture.hash,999u,{});
                else if(mode==1u)static_cast<void>(fixture.world->ReadActor(999u));
                else fixture.world->WithRoot([](World&){throw std::runtime_error("Caught nested provider failure");});}
            catch(const std::exception&) {}return false;
        };
        Refuse([&]{fixture.Query(query);},"Caught nested failure did not poison encroachment root");before.Check(fixture);
    }
    for(unsigned phase=0u;phase<4u;++phase)for(unsigned target=0u;target<(phase?3u:2u);++target) {
        auto query=fixture.Provider();const auto replace=[&](std::uintptr_t candidate) {
            if(target==0u)fixture.world->DestroyHash(fixture.hash);
            else {const auto id=target==1u?fixture.source:candidate;const auto old=fixture.world->ReadActor(id);
                fixture.world->DestroyActorStorage(id);fixture.world->DefineActor(old);}
        };
        if(phase==0u)query.sourceBounds=[&](std::uintptr_t)->Box{replace(1u);return Bounds(Cell(0));};
        if(phase==1u)query.movingBrush=[&](std::uintptr_t id){replace(id);return false;};
        if(phase==2u)query.candidateCylinder=[&](std::uintptr_t id){const auto value=fixture.cylinders.at(id);replace(id);return value;};
        if(phase==3u)query.sourcePoint=[&](Hit&,std::uintptr_t,const Vec& p,const Vec& e,std::uint32_t){replace(fixture.Identify(p,e));return false;};
        Refuse([&]{fixture.Query(query);},"Provider storage destruction/replacement escaped generation guard");before.Check(fixture);
    }
    Refuse([&]{fixture.world->WithRoot([&](World& world) {
        fixture.Query();world.SetActorLive(fixture.source,{5,4,3},true,false);throw std::runtime_error("Late caller publication failed");
    });});before.Check(fixture);
    {
        auto root=fixture.world->BeginRoot();fixture.Query();fixture.world->SetActorLive(fixture.source,{9,8,7},true,false);
        auto bad=fixture.Provider();bad.sourcePoint=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,std::uint32_t) {
            try {static_cast<void>(fixture.world->ReadHash(999u));}catch(const std::exception&) {}return false;
        };
        Refuse([&]{fixture.Query(bad);});Refuse([&]{root->PrepareCommit();},"Caught encroachment failure allowed enclosing root publication");
        Require(!root->CommitPrepared()&&root->Rollback(),"Poisoned encroachment lost shared caller undo ownership");before.Check(fixture);
    }
}
void LimitsAndAllocation() {
    Fixture missing;missing.Define(1u,Cell(0));missing.Add(1u);const Snapshot before(missing);
    for(unsigned phase=0u;phase<4u;++phase) {
        auto query=missing.Provider();if(phase==0u)query.sourceBounds={};else if(phase==1u)query.movingBrush={};
        else if(phase==2u)query.candidateCylinder={};else query.sourcePoint={};
        Refuse([&]{missing.Query(query);});before.Check(missing);
    }
    Refuse([&]{missing.world->ActorEncroachmentCheck(missing.hash,0u,missing.Provider());});before.Check(missing);
    Refuse([&]{missing.world->ActorEncroachmentCheck(999u,missing.source,missing.Provider());});before.Check(missing);
    Refuse([&]{missing.world->ActorEncroachmentCheck(missing.hash,999u,missing.Provider());});before.Check(missing);
    auto bad=missing.Provider();bad.sourceBounds=[](std::uintptr_t){const float nan=std::numeric_limits<float>::quiet_NaN();return Box{{nan,0,0},{0,0,0}};};
    Refuse([&]{missing.Query(bad);});before.Check(missing);
    QuestVr::OriginalHashLimits limits;limits.hits=1u;Fixture hits(0u,limits);hits.Define(1u,Cell(0));hits.Define(2u,Cell(0));hits.Add(2u);hits.Add(1u);
    const Snapshot hitBefore(hits);Refuse([&]{hits.Query();});hitBefore.Check(hits);
    limits={};limits.cells=1u;Fixture cells(0u,limits);cells.Define(1u,Cell(0));cells.Add(1u);const Snapshot cellBefore(cells);
    bad=cells.Provider();bool entered{};bad.sourcePoint=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,std::uint32_t) {
        if(!entered) {entered=true;Queries inner;inner.sourceBounds=[](std::uintptr_t){return Bounds(Cell(0));};cells.Query(inner);}return false;
    };
    Refuse([&]{cells.Query(bad);});cellBefore.Check(cells);
    limits={};limits.steps=1030u;Fixture work(0u,limits);work.Define(1u,Cell(0));work.Add(1u);const Snapshot workBefore(work);
    auto repeat=work.Provider();
    Refuse([&]{work.world->WithRoot([&](World&) {for(unsigned i=0u;i<300u;++i)work.Query(repeat);});},
        "Repeated encroachment queries acquired fresh per-call work ledgers");
    Require(work.world->ReadStats().steps==1030u,"Cumulative encroachment control failed before exhausting its actual shared work budget");
    workBefore.Check(work);
    limits={};limits.depth=2u;Fixture depth(0u,limits);depth.Define(1u,Cell(0));depth.Add(1u);const Snapshot depthBefore(depth);
    auto recurse=depth.Provider();recurse.sourcePoint=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,std::uint32_t){depth.Query(recurse);return false;};
    Refuse([&]{depth.Query(recurse);});depthBefore.Check(depth);
    // Remove current slot without reuse reaches undefined fresh free payload.
    // Refusal must restore the whole transaction, not guess an actor identity.
    bad=missing.Provider();bad.sourcePoint=[&](Hit&,std::uintptr_t,const Vec&,const Vec&,std::uint32_t) {
        missing.world->Remove(missing.hash,1u,missing.Registration());return false;
    };
    Refuse([&]{missing.Query(bad);});before.Check(missing);
    bool complete{};
    for(std::size_t allowed=0u;allowed<96u&&!complete;++allowed) {
        Fixture allocation;for(const auto id:{1u,2u,3u}){allocation.Define(id,Cell(0));allocation.Add(id);}
        const Snapshot saved(allocation);auto provider=allocation.Provider();
        // Reserve diagnostic vectors before arming faults: this sweep targets
        // real hash snapshot/hit allocations, not test trace growth.
        allocation.gates.reserve(8u);allocation.samples.reserve(8u);allocation.points.reserve(8u);
        bool failed{};EncroachmentAllocationFault::Enable(allowed);
        try {static_cast<void>(allocation.Query(provider));}catch(const std::bad_alloc&) {failed=true;}
        catch(...) {EncroachmentAllocationFault::Disable();throw;}
        EncroachmentAllocationFault::Disable();
        if(failed) {++allocationRefusals;saved.Check(allocation);
            auto root=allocation.world->BeginRoot();root->PrepareCommit();Require(root->CommitPrepared(),"Allocation refusal left hidden encroachment root ownership");}
        else complete=true;
    }
    Require(complete&&allocationRefusals>=8u,"Encroachment allocation sweep did not reach full snapshot and hit storage");
}
struct HitStorageLedger {
    std::size_t live{},peak{},limit{std::numeric_limits<std::size_t>::max()};
};
struct HitStorageTrace {
    using Phase=QuestVr::OriginalHashHitStoragePhase;
    HitStorageLedger& ledger;
    std::size_t retained{},requested{},calls{},growths{};
    bool pending{};
    explicit HitStorageTrace(HitStorageLedger& owner):ledger(owner){}
    ~HitStorageTrace() noexcept {if(ledger.live<retained)std::terminate();ledger.live-=retained;}
    void Observe(Phase phase,std::size_t old,std::size_t next) {
        Require(old<=std::numeric_limits<std::size_t>::max()/sizeof(Hit)&&
            next<=std::numeric_limits<std::size_t>::max()/sizeof(Hit),"Hit storage callback overflowed its owning capacity");
        Require(old*sizeof(Hit)==retained&&next>old,"Hit storage callback changed its retained collector capacity");
        if(phase==Phase::BeforeReserve) {
            Require(!pending,"Hit storage began another reserve before completing the current allocation");
            pending=true;requested=next;++growths;
        }else Require(pending&&next>=requested,"Hit storage did not report actual post-reserve owning capacity");
        Require(next*sizeof(Hit)<=std::numeric_limits<std::size_t>::max()-ledger.live,
            "Hit storage reallocation peak overflowed its shared ledger");
        const auto peak=ledger.live+next*sizeof(Hit);
        if(peak>ledger.limit)throw std::runtime_error("Explicit shared hit-storage byte allowance exhausted");
        ledger.peak=std::max(ledger.peak,peak);++calls;
        if(phase==Phase::AfterReserve) {
            ledger.live=ledger.live-retained+next*sizeof(Hit);retained=next*sizeof(Hit);pending=false;
        }
    }
    void Bind(Queries& query) {
        query.hitStorage=[this](Phase phase,std::size_t old,std::size_t next){Observe(phase,old,next);};
    }
};
void HitStorageAccounting() {
    using Phase=QuestVr::OriginalHashHitStoragePhase;
    {
        Fixture empty;auto query=empty.Provider();
        query.hitStorage=[](Phase,std::size_t,std::size_t){throw std::runtime_error("Inactive hit storage was requested");};
        const auto zero=empty.Query(query);
        Require(zero.records.empty()&&zero.records.capacity()==0u,"Empty encroachment preallocated hit storage");
        for(std::uintptr_t id=1u;id<=9u;++id){empty.Define(id,Cell(0));empty.Add(id);empty.clear[id]=true;}
        const auto clear=empty.Query(query);
        Require(clear.records.empty()&&clear.records.capacity()==0u,"Clear PointChecks allocated a no-hit collector");
    }
    {
        QuestVr::OriginalHashLimits limits;limits.hits=17u;Fixture fixture(0u,limits);
        for(std::uintptr_t id=1u;id<=17u;++id){fixture.Define(id,Cell(0));fixture.Add(id);}
        HitStorageLedger ledger;
        {
            HitStorageTrace trace(ledger);auto query=fixture.Provider();trace.Bind(query);
            const auto point=query.sourcePoint;std::size_t points{};
            query.sourcePoint=[&](Hit& hit,std::uintptr_t id,const Vec& location,const Vec& extent,std::uint32_t flags){
                Require(ledger.live==trace.retained,"PointCheck cannot observe its live hit-vector reservation");
                Require(points==0u?trace.retained==0u:trace.retained>=points*sizeof(Hit),
                    "Hit storage charged all-world capacity or forgot preceding accepted records");
                ++points;return point(hit,id,location,extent,flags);
            };
            const auto result=fixture.Query(query);
            Require(result.records.size()==17u&&result.records.capacity()==17u&&trace.retained==17u*sizeof(Hit),
                "Hit storage does not correspond to actual returned owning capacity or existing hit-limit clamp");
            Require(points==17u&&trace.growths==6u&&trace.calls==12u&&!trace.pending,
                "Hit storage growth callbacks do not cover exactly1/2/4/8/16/17 owning allocations");
            Require(ledger.live==17u*sizeof(Hit)&&ledger.peak==33u*sizeof(Hit),
                "Hit storage omitted the simultaneous old16/new17 reallocation peak");
        }
        Require(ledger.live==0u,"Hit storage scope did not release its retained capacity");
    }
    {
        Fixture fixture;
        for(std::uintptr_t id=1u;id<=9u;++id){fixture.Define(id,Cell(0));fixture.Add(id);}
        const auto second=fixture.world->CreateHash();
        for(std::uintptr_t id=101u;id<=102u;++id){fixture.Define(id,Cell(0));fixture.world->Add(second,id,fixture.Registration());}
        HitStorageLedger ledger;bool nested{};
        {
            HitStorageTrace outer(ledger);auto query=fixture.Provider();outer.Bind(query);const auto point=query.sourcePoint;
            query.sourcePoint=[&](Hit& hit,std::uintptr_t id,const Vec& location,const Vec& extent,std::uint32_t flags){
                const auto clear=point(hit,id,location,extent,flags);
                if(!nested&&outer.retained) {
                    nested=true;const auto outerBytes=outer.retained;
                    {
                        HitStorageTrace inner(ledger);auto nestedQuery=fixture.Provider();inner.Bind(nestedQuery);
                        const auto innerPoint=nestedQuery.sourcePoint;bool retainedInner{};
                        nestedQuery.sourcePoint=[&](Hit& packet,std::uintptr_t actor,const Vec& at,const Vec& size,std::uint32_t mask){
                            Require(ledger.live==outerBytes+inner.retained,"Nested PointCheck lost simultaneous outer/inner hit storage");
                            retainedInner|=inner.retained!=0u;return innerPoint(packet,actor,at,size,mask);
                        };
                        const auto result=fixture.world->ActorEncroachmentCheck(second,fixture.source,nestedQuery);
                        Require(retainedInner&&result.records.size()==2u&&inner.retained==result.records.capacity()*sizeof(Hit),
                            "Nested encroachment did not retain its actual nonempty collector");
                    }
                    Require(ledger.live==outerBytes,"Nested hit storage cleanup discarded its caller's live collector");
                }
                return clear;
            };
            const auto result=fixture.Query(query);
            Require(nested&&result.records.size()==9u&&outer.retained==result.records.capacity()*sizeof(Hit),
                "Nested storage accounting changed complete original query traversal");
        }
        Require(ledger.live==0u,"Nested hit storage leaked a lexical reservation");
    }
    for(std::size_t fail=0u;fail<10u;++fail) {
        Fixture fixture;for(std::uintptr_t id=1u;id<=9u;++id){fixture.Define(id,Cell(0));fixture.Add(id);}
        const Snapshot before(fixture);HitStorageLedger ledger;
        {
            HitStorageTrace trace(ledger);auto query=fixture.Provider();std::size_t calls{};
            query.hitStorage=[&](Phase phase,std::size_t old,std::size_t next){
                trace.Observe(phase,old,next);if(calls++==fail)throw std::runtime_error("Injected hit-storage admission refusal");
            };
            Refuse([&]{fixture.Query(query);},"Hit-storage callback refusal silently published an encroachment query");
        }
        Require(ledger.live==0u,"Refused hit storage leaked its currently retained vector capacity");before.Check(fixture);
    }
    for(const bool enough:{false,true}) {
        Fixture fixture;for(std::uintptr_t id=1u;id<=9u;++id){fixture.Define(id,Cell(0));fixture.Add(id);}
        const Snapshot before(fixture);HitStorageLedger ledger;ledger.limit=24u*sizeof(Hit)-(enough?0u:1u);
        {
            HitStorageTrace trace(ledger);auto query=fixture.Provider();trace.Bind(query);
            if(enough) {
                const auto result=fixture.Query(query);
                Require(result.records.size()==9u&&ledger.peak==24u*sizeof(Hit),"Exact hit-vector reallocation allowance did not succeed");
            }else Refuse([&]{fixture.Query(query);},"N-1 shared byte allowance accepted the last hit-vector reallocation");
        }
        Require(ledger.live==0u,"Byte-budget refusal/success leaked a hit-vector reservation");if(!enough)before.Check(fixture);
    }
    {
        Fixture fixture;fixture.Define(1u,Cell(0));fixture.Add(1u);const Snapshot before(fixture);
        auto root=fixture.world->BeginRoot();HitStorageLedger ledger;
        {
            HitStorageTrace trace(ledger);auto query=fixture.Provider();
            query.hitStorage=[&](Phase phase,std::size_t old,std::size_t next){
                trace.Observe(phase,old,next);
                if(phase==Phase::AfterReserve)throw std::runtime_error("Caught post-allocation admission refusal");
            };
            Refuse([&]{fixture.Query(query);});
        }
        Require(ledger.live==0u,"Caught post-allocation failure retained native scratch memory");
        Refuse([&]{root->PrepareCommit();},"Caught hit-storage failure allowed enclosing root publication");
        Require(!root->CommitPrepared()&&root->Rollback(),"Poisoned storage admission lost the original root rollback");before.Check(fixture);
    }
}
std::uint32_t Next(std::uint32_t& value) {value^=value<<13u;value^=value>>17u;value^=value<<5u;return value;}
void Randomized() {
    Fixture fixture;std::uint32_t rng=0x39b81aa7u;
    for(std::uintptr_t id=1u;id<=24u;++id) {fixture.Define(id,Cell(static_cast<int>(id%5u),static_cast<int>(id/5u%4u)));fixture.Add(id);}
    for(unsigned iteration=0u;iteration<300u;++iteration) {
        const auto id=static_cast<std::uintptr_t>(Next(rng)%24u+1u);
        switch(Next(rng)%5u) {
        case 0u:fixture.Add(id);break;
        case 1u:fixture.Remove(id);break;
        case 2u:fixture.moving[id]=!fixture.moving.at(id);break;
        case 3u:fixture.clear[id]=!fixture.clear.at(id);break;
        default:fixture.world->SetActorLive(id,{7,8,9},false,true);break;
        }
        // Add/Remove still require their actual original registration gates.
        fixture.world->SetActorLive(id,fixture.cylinders.at(id).location,true,false);
        const int x=static_cast<int>(Next(rng)%5u),y=static_cast<int>(Next(rng)%4u);
        fixture.boxes[fixture.source]={{0,0,0},{x,y,0}};fixture.CompareQuery();++randomQueries;
    }
    fixture.CompareStorage();
}
} // namespace
int main() {
    try {CandidateArgumentWordStores();DirectionSeedsAndOrder();BoundsTagsAndInactiveProviders();ReentranceAndLiveLinks();RollbackAndGenerations();LimitsAndAllocation();HitStorageAccounting();Randomized();
        std::cout<<"PASS original ActorEncroachmentCheck "<<checks<<" checks, "<<refusals<<" explicit refusals, "
            <<comparisons<<" independent CRT/vector queries ("<<randomQueries<<" randomized), "<<allocationRefusals
            <<" allocation refusals, "<<argumentWordPackets<<" argument-word packets ("<<hardwareWordStores
            <<" hardware x87 word stores); source-direction, Time1 seed, live Actor/Next, tail order and shared-root rollback verified.\n";return 0;}
    catch(const std::exception& error) {EncroachmentAllocationFault::Disable();std::cerr<<"Original actor encroachment failed: "<<error.what()<<'\n';return 1;}
}
