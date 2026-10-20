#include "quest_single_line_trace.h"
#include "quest_native_work.h"

#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Vec=PortableModelVec3;
using Hit=QuestVr::OriginalActorLineHit;
using Arena=QuestVr::OriginalLineHitArena;
using Queries=QuestVr::OriginalSingleLineQueries;
using Result=QuestVr::OriginalSingleLineResult;
using Limits=QuestVr::OriginalSingleLineLimits;
using Class=QuestVr::OriginalSingleLineClass;
using Flag=QuestVr::OriginalSingleLineFlag;
constexpr Id source=10u,owner=11u,grandowner=12u,world=20u,candidate=30u;
std::size_t checks{},refusals{},truthCases{},mixedCases{},orderCases{},rawCases{},transcriptCases{},failurePrefixes{},ledgerCases{};
void Require(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
bool Same(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec a,Vec b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Hit& a,const Hit& b) {return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&
    a.item==b.item&&Same(a.time,b.time)&&Same(a.location,b.location)&&Same(a.normal,b.normal);}
Hit Packet(Id actor,std::uint32_t salt=0u) {
    const auto word=[&](std::uint32_t offset) {return std::bit_cast<float>(0x3f000000u+(salt+offset)*31u);};
    return {0u,actor,Id{0xfedcba98u}+salt,{word(1u),word(2u),word(3u)},
        {word(4u),word(5u),word(6u)},word(7u),static_cast<std::int32_t>(0x80000000u+salt)};
}
Arena Chain(std::vector<Hit> hits) {
    for(std::size_t i=0u;i<hits.size();++i)hits[i].next=i+1u<hits.size()?i+2u:0u;
    const auto first=hits.empty()?0u:1u;return {std::move(hits),first};
}
template<class F>void Refuse(Hit& hit,F&& run) {
    const auto before=hit;bool refused{};try {run();}catch(const std::exception&){refused=true;}
    Require(refused,"Unsupported SingleLine request silently returned clear/hit");
    Require(Same(hit,before),"SingleLine refusal changed caller packet");++refusals;
}
enum class Op { Level,Collect,Owner,Class,Flag };
struct Event {Op op;Id actor{};unsigned selector{};bool operator==(const Event&)const=default;};
Event L(){return {Op::Level};}
Event M(){return {Op::Collect};}
Event O(Id id){return {Op::Owner,id};}
Event C(Id id,Class cls){return {Op::Class,id,static_cast<unsigned>(cls)};}
Event F(Id id,Flag flag){return {Op::Flag,id,static_cast<unsigned>(flag)};}
struct NativeClass {unsigned parent{};std::string_view display;};
struct Actor {unsigned cls{1u};Id owner{};unsigned bits{};};
struct Fixture {
    // Identity-based native ancestry: LevelInfo derives ZoneInfo. The two
    // unrelated misleading display names must not acquire those identities.
    std::array<NativeClass,11u> classes{{{0u,"None"},{0u,"Engine.Actor"},
        {1u,"Engine.ZoneInfo"},{2u,"Engine.LevelInfo"},{1u,"Engine.Pawn"},
        {1u,"Engine.Mover"},{3u,"Fixture.LevelChild"},{4u,"Fixture.PawnChild"},
        {5u,"Fixture.MoverChild"},{1u,"Unrelated.LevelInfo"},{1u,"Unrelated.Pawn"}}};
    std::array<Actor,80u> actors{};
    Arena hits;
    std::vector<Event> trace;
    bool tracing{};
    std::size_t failAt=std::numeric_limits<std::size_t>::max(),reads{},work{};
    std::int32_t seenActors{};Id seenWorld{};std::uint8_t seenExtra{};
    Vec seenEnd{},seenStart{},seenExtent{};
    Fixture(){actors[world].cls=3u;}
    Actor& Get(Id id) {
        if(!id||id>=actors.size())throw std::runtime_error("Independent actor identity is unavailable");
        return actors[static_cast<std::size_t>(id)];
    }
    void Read(Event event) {
        if(tracing)trace.push_back(event);
        if(reads++==failAt)throw std::runtime_error("Explicit active native provider refusal");
    }
    bool Derives(Id id,Class requested) {
        const std::array<unsigned,4u> targets{3u,4u,5u,2u};
        auto cls=Get(id).cls;std::array<bool,11u> visited{};
        while(cls) {
            if(cls>=visited.size()||visited[cls])throw std::runtime_error("Independent class chain is malformed");
            visited[cls]=true;if(cls==targets[static_cast<unsigned>(requested)])return true;
            cls=classes[cls].parent;
        }
        return false;
    }
    Queries Providers() {
        Queries q;
        q.levelActor0=[&]{Read(L());return world;};
        q.multiLine=[&](Vec end,Vec start,Vec extent,std::int32_t actorsOn,Id worldOn,std::uint8_t extra) {
            Read(M());seenEnd=end;seenStart=start;seenExtent=extent;
            seenActors=actorsOn;seenWorld=worldOn;seenExtra=extra;return hits;
        };
        q.owner=[&](Id actor){Read(O(actor));return Get(actor).owner;};
        q.isA=[&](Id actor,Class cls){Read(C(actor,cls));return Derives(actor,cls);};
        q.flag=[&](Id actor,Flag selected){Read(F(actor,selected));return (Get(actor).bits&(1u<<static_cast<unsigned>(selected)))!=0u;};
        q.work=[&]{++work;};return q;
    }
    Result Run(Hit& hit,std::uint8_t flags=0x17u,Id self=source,const Limits& limits={}) {
        return QuestVr::CheckOriginalSingleLine(hit,self,{10.0f,20.0f,30.0f},{1.0f,2.0f,3.0f},
            {4.0f,5.0f,6.0f},flags,0xa5u,Providers(),limits);
    }
};
struct Archetype {unsigned cls;unsigned category;};
// Category is an independent classification table, not provider ancestry or
// production branch evaluation:0 LevelInfo,1 Pawn,2 Mover,3 ZoneInfo,4 Others.
constexpr std::array archetypes{Archetype{1u,4u},Archetype{2u,3u},Archetype{3u,0u},
    Archetype{4u,1u},Archetype{5u,2u},Archetype{6u,0u},Archetype{7u,1u},
    Archetype{8u,2u},Archetype{9u,4u},Archetype{10u,4u}};
bool Expected(unsigned category,unsigned bits,unsigned flags,bool excluded) {
    constexpr std::array<unsigned,5u> masks{4u,1u,2u,8u,16u};
    const auto special=(bits&1u)!=0u||(bits&6u)==6u;
    return !excluded&&(flags&masks[category])!=0u&&(category!=4u||!(flags&32u)||special);
}
void ExhaustiveTruth() {
    Fixture f;f.hits=Chain({Packet(candidate)});auto q=f.Providers();
    for(const auto& kind:archetypes)for(unsigned bits=0u;bits<256u;++bits)for(unsigned flags=0u;flags<256u;++flags) {
        f.Get(candidate).cls=kind.cls;f.Get(candidate).bits=bits;
        auto hit=Packet(70u,13u);auto expected=hit;
        const auto accepted=Expected(kind.category,bits,flags,false);
        if(accepted)expected=f.hits.records.front();else {expected.actor=0u;expected.time=1.0f;}
        const auto result=QuestVr::CheckOriginalSingleLine(hit,0u,{},{},{},static_cast<std::uint8_t>(flags),0xffu,q);
        Require(result.clear!=accepted&&Same(hit,expected),"Full byte matrix differs from independent category/bitfield truth formula");
        Require(f.seenActors==((flags&0x17u)?1:0)&&f.seenWorld==((flags&4u)?world:0u)&&f.seenExtra==0xffu,
            "SingleLine changed exact actor/world enable masks or final byte forwarding");
        Require(result.selected==(accepted?1u:0u)&&result.hits.records.size()==1u,
            "Selector lost retained collector storage or selected local identity");
        ++truthCases;
    }
    Require(truthCases==655'360u,"SingleLine byte matrix missed its intended combinations");
}
void OwnerDirection() {
    Fixture f;f.Get(source).owner=owner;f.Get(owner).owner=grandowner;
    f.hits=Chain({Packet(source),Packet(owner),Packet(grandowner),Packet(candidate)});
    f.Get(candidate).owner=source; // Must NOT exclude a candidate owned by source.
    f.tracing=true;auto hit=Packet(70u);auto result=f.Run(hit);
    Require(!result.clear&&hit.actor==candidate&&result.selected==4u&&result.stats.excluded==3u,
        "Source/self/Owner ancestors or candidate-owned-by-source exclusion direction is wrong");
    const auto selfRead=std::find(f.trace.begin(),f.trace.end(),O(source));
    Require(selfRead!=f.trace.end()&&std::count(f.trace.begin(),f.trace.end(),O(candidate))==0,
        "Owner exclusion inspected candidate.Owner instead of source chain");
    Require(std::count(f.trace.begin(),f.trace.end(),O(source))==3,
        "Self/equality exclusion read inactive Owner or reused a cached source ancestry");
    // A self hit must succeed as an exclusion even if every other provider is
    // absent: no class, Owner, flag or world callback is active.
    Queries q;q.multiLine=[&](Vec,Vec,Vec,std::int32_t,Id,std::uint8_t){return Chain({Packet(source)});};
    hit=Packet(70u);result=QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0u,0u,q);
    Require(result.clear&&hit.actor==0u,"Self exclusion required inactive native services");
    // Each candidate rereads live Owner instead of a precomputed/cached chain.
    f=Fixture{};f.hits=Chain({Packet(owner),Packet(candidate)});f.Get(source).owner=owner;
    q=f.Providers();unsigned ownerReads{};q.owner=[&](Id actor){
        if(actor==source)return ++ownerReads==1u?owner:0u;
        throw std::runtime_error("Equality should not read this ancestor's Owner");
    };
    hit=Packet(70u);result=QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0x10u,0u,q);
    Require(!result.clear&&hit.actor==candidate&&ownerReads==2u,"Source Owner was snapshotted across candidate visits");
}
void MixedLinkedOracle() {
    Fixture f;f.Get(source).owner=owner;f.Get(owner).owner=grandowner;
    f.Get(source).cls=4u;f.Get(owner).cls=5u;f.Get(candidate+1u).cls=2u;
    f.Get(candidate).bits=6u;
    // Independent authored categories/exclusions, in storage order. The
    // oracle scans only the separately specified logical permutation: it
    // never follows production Next, calls ancestry providers or sorts Time.
    constexpr std::array<Id,5u> actors{world,source,owner,candidate,candidate+1u};
    constexpr std::array<unsigned,5u> categories{0u,1u,2u,4u,3u};
    constexpr std::array<unsigned,5u> bits{0u,0u,0u,6u,0u};
    for(unsigned i=0u;i<actors.size();++i) {
        auto packet=Packet(actors[i],i);
        packet.time=std::bit_cast<float>(std::array<std::uint32_t,5u>{0x7fc00123u,0x3f000000u,
            0x3f000000u,0xff800000u,0x80000000u}[i]);
        f.hits.records.push_back(packet);
    }
    std::array<unsigned,5u> permutation{0u,1u,2u,3u,4u};
    do {
        f.hits.first=permutation.front()+1u;
        for(std::size_t i=0u;i<permutation.size();++i)
            f.hits.records[permutation[i]].next=i+1u<permutation.size()?permutation[i+1u]+1u:0u;
        const auto original=f.hits;
        for(const bool hasSource:{false,true})for(unsigned flags=0u;flags<256u;++flags) {
            std::size_t selected=actors.size();
            for(const auto index:permutation) {
                const bool excluded=hasSource&&(actors[index]==source||actors[index]==owner);
                if(Expected(categories[index],bits[index],flags,excluded)){selected=index;break;}
            }
            auto hit=Packet(70u,77u);auto expected=hit;
            if(selected<actors.size())expected=f.hits.records[selected];else {expected.actor=0u;expected.time=1.0f;}
            const auto result=f.Run(hit,static_cast<std::uint8_t>(flags),hasSource?source:0u);
            Require(result.clear==(selected==actors.size())&&Same(hit,expected)&&
                result.selected==(selected<actors.size()?selected+1u:0u),
                "Mixed linked traversal/Owner/precedence result differs from independent logical-order oracle");
            Require(result.hits.first==original.first&&result.hits.records.size()==original.records.size(),
                "Mixed selector lost the owning arena's original layout");
            for(std::size_t i=0u;i<original.records.size();++i)
                Require(Same(result.hits.records[i],original.records[i])&&Same(f.hits.records[i],original.records[i]),
                    "Selector rewrote/reordered provider or retained packets");
            ++mixedCases;
        }
    }while(std::next_permutation(permutation.begin(),permutation.end()));
    Require(mixedCases==61'440u,"Mixed linked-order cohort omitted permutations, source states or byte flags");
}
void OrderAndRawPackets() {
    Fixture f;
    std::array<Id,5u> permutation{30u,31u,32u,33u,34u};
    do {
        std::vector<Hit> values;
        for(std::size_t i=0u;i<permutation.size();++i) {auto hit=Packet(permutation[i],static_cast<std::uint32_t>(i));hit.time=0.5f;values.push_back(hit);}
        f.hits=Chain(std::move(values));
        // Storage order deliberately differs from linked traversal order.
        f.hits.first=4u;f.hits.records[3u].next=2u;f.hits.records[1u].next=5u;
        f.hits.records[4u].next=1u;f.hits.records[0u].next=3u;f.hits.records[2u].next=0u;
        auto hit=Packet(70u);const auto result=f.Run(hit,0x10u,0u);
        Require(!result.clear&&result.selected==4u&&Same(hit,f.hits.records[3u]),
            "Selector resorted tied hits, used storage order or invented a pointer tie breaker");
        Require(result.hits.records[3u].next==2u&&Same(result.hits.records[3u],hit),
            "Selected complete packet/local Next lost its still-owning arena");++orderCases;
    }while(std::next_permutation(permutation.begin(),permutation.end()));
    // Selection never compares/rejects Time, vectors, Item or primitive words.
    constexpr std::array<std::uint32_t,13u> words{0u,0x80000000u,1u,0x807fffffu,0x7f7fffffu,
        0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00001u,0xffc54321u,0x7f800001u,0xffffffffu,0x3f800000u};
    for(const auto raw:words) {
        const auto value=std::bit_cast<float>(raw);auto packet=Packet(candidate,raw);
        packet.time=value;packet.location={value,value,value};packet.normal={value,value,value};
        packet.next=std::numeric_limits<Id>::max();packet.primitive=std::numeric_limits<Id>::max();
        f.hits={{packet},1u};auto hit=Packet(70u);const auto result=f.Run(hit,0x10u,0u);
        Require(!result.clear&&Same(hit,packet),"SingleLine read/normalized raw packet floats or changed an unused tail");
        ++rawCases;
    }
}
void MissAndInputCopies() {
    Fixture f;auto hit=Packet(70u);const auto before=hit;auto expected=before;expected.actor=0u;expected.time=1.0f;
    auto q=f.Providers();q.owner={};q.isA={};q.flag={};q.levelActor0={};
    const auto empty=QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0u,0u,q);
    Require(empty.clear&&Same(hit,expected)&&empty.stats.hitVisits==0u,"Empty miss reset more than Actor/Time or touched inactive actor providers");
    // Miss preserves arbitrary payload words without any floating arithmetic.
    for(const auto raw:{0x80000000u,0x7fc54321u,0x7f800001u,0xff800000u}) {
        hit=Packet(70u);hit.location.x=std::bit_cast<float>(raw);hit.normal.z=std::bit_cast<float>(raw);
        hit.next=std::numeric_limits<Id>::max();auto wanted=hit;wanted.actor=0u;wanted.time=1.0f;
        const auto result=f.Run(hit,0u);Require(result.clear&&Same(hit,wanted),"Miss destroyed inactive raw packet storage");++rawCases;
    }
    Vec end{10,20,30},start{1,2,3},extent{4,5,6};q=f.Providers();
    q.levelActor0=[&] {end={90,91,92};start={80,81,82};extent={70,71,72};return world;};
    hit=before;const auto result=QuestVr::CheckOriginalSingleLine(hit,0u,end,start,extent,4u,0xffu,q);
    Require(result.clear&&Same(f.seenEnd,Vec{10,20,30})&&Same(f.seenStart,Vec{1,2,3})&&Same(f.seenExtent,Vec{4,5,6}),
        "Native by-value endpoint/extent inputs aliased callback-mutated caller storage");
}
struct Answer {Event event;Id value;};
void Golden(bool accepted,std::uint8_t flags,Id self,const std::vector<Answer>& expected) {
    for(const bool missingInactive:{false,true}) {
        Fixture f;f.hits=Chain({Packet(candidate)});std::size_t cursor{};
        const auto read=[&](Event event) {
            Require(cursor<expected.size()&&expected[cursor].event==event,"Selector read an inactive provider or changed exact lazy native-field order");
            return expected[cursor++].value;
        };
        const auto active=[&](Op op) {return !missingInactive||std::any_of(expected.begin(),expected.end(),[&](const auto& a){return a.event.op==op;});};
        Queries q;
        if(active(Op::Level))q.levelActor0=[&]{return read(L());};
        q.multiLine=[&](Vec,Vec,Vec,std::int32_t,Id,std::uint8_t){static_cast<void>(read(M()));return f.hits;};
        if(active(Op::Owner))q.owner=[&](Id id){return read(O(id));};
        if(active(Op::Class))q.isA=[&](Id id,Class cls){return read(C(id,cls))!=0u;};
        if(active(Op::Flag))q.flag=[&](Id id,Flag flag){return read(F(id,flag))!=0u;};
        auto hit=Packet(70u);const auto result=QuestVr::CheckOriginalSingleLine(hit,self,{},{},{},flags,0u,q);
        Require(result.clear!=accepted&&cursor==expected.size(),"Selector did not consume complete source-backed lazy transcript");++transcriptCases;
    }
    for(std::size_t failure=0u;failure<expected.size();++failure) {
        Fixture f;f.hits=Chain({Packet(candidate)});std::size_t cursor{};
        const auto read=[&](Event event) {
            Require(cursor<expected.size()&&expected[cursor].event==event,"Throwing selector transcript changed active read order");
            const auto at=cursor++;if(at==failure)throw std::runtime_error("Forced provider failure");return expected[at].value;
        };
        Queries q;
        q.levelActor0=[&]{return read(L());};
        q.multiLine=[&](Vec,Vec,Vec,std::int32_t,Id,std::uint8_t){static_cast<void>(read(M()));return f.hits;};
        q.owner=[&](Id id){return read(O(id));};q.isA=[&](Id id,Class cls){return read(C(id,cls))!=0u;};
        q.flag=[&](Id id,Flag selected){return read(F(id,selected))!=0u;};
        auto hit=Packet(70u);Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,self,{},{},{},flags,0u,q);});
        Require(cursor==failure+1u,"Selector continued reading after an active provider failed");++failurePrefixes;
    }
}
void LazyTranscripts() {
    using Class::LevelInfo;using Class::Pawn;using Class::Mover;using Class::ZoneInfo;
    using Flag::ProjTarget;using Flag::BlockActors;using Flag::BlockPlayers;
    Golden(true,4u,0u,{{L(),world},{C(world,LevelInfo),1u},{M(),0u},{C(candidate,LevelInfo),1u}});
    Golden(false,8u,0u,{{M(),0u},{C(candidate,LevelInfo),1u}}); // No ZoneInfo fallback.
    Golden(true,1u,source,{{M(),0u},{O(source),0u},{C(candidate,LevelInfo),0u},{C(candidate,Pawn),1u}});
    Golden(true,2u,0u,{{M(),0u},{C(candidate,LevelInfo),0u},{C(candidate,Pawn),0u},{C(candidate,Mover),1u}});
    Golden(true,8u,0u,{{M(),0u},{C(candidate,LevelInfo),0u},{C(candidate,Pawn),0u},{C(candidate,Mover),0u},{C(candidate,ZoneInfo),1u}});
    const std::vector<Answer> others{{M(),0u},{C(candidate,LevelInfo),0u},{C(candidate,Pawn),0u},
        {C(candidate,Mover),0u},{C(candidate,ZoneInfo),0u}};
    Golden(true,0x10u,0u,others);Golden(false,0x20u,0u,others);
    auto seq=others;seq.push_back({F(candidate,ProjTarget),1u});Golden(true,0x30u,0u,seq);
    seq=others;seq.insert(seq.end(),{{F(candidate,ProjTarget),0u},{F(candidate,BlockActors),0u}});Golden(false,0x30u,0u,seq);
    for(const Id blocks:{Id{0u},Id{1u}}) {
        seq=others;seq.insert(seq.end(),{{F(candidate,ProjTarget),0u},{F(candidate,BlockActors),1u},{F(candidate,BlockPlayers),blocks}});
        Golden(blocks!=0u,0x30u,0u,seq);
    }
    Golden(false,0u,candidate,{{M(),0u}}); // Self match never reads Owner or class.
}
void RefusalsAndBounds() {
    Fixture f;f.hits=Chain({Packet(candidate)});auto hit=Packet(70u);
    const auto run=[&](const Queries& q,const Limits& limits=Limits{}) {return QuestVr::CheckOriginalSingleLine(hit,0u,{},{},{},0x10u,0u,q,limits);};
    Refuse(hit,[&]{run({});});
    auto q=f.Providers();q.isA={};Refuse(hit,[&]{run(q);});
    q=f.Providers();q.flag={};Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,0u,{},{},{},0x30u,0u,q);});
    q=f.Providers();q.owner={};Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0x10u,0u,q);});
    q=f.Providers();q.levelActor0={};Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,0u,{},{},{},4u,0u,q);});
    q=f.Providers();q.levelActor0=[] {return Id{};};Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,0u,{},{},{},4u,0u,q);});
    q=f.Providers();q.levelActor0=[] {return candidate;};Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,0u,{},{},{},4u,0u,q);});
    for(unsigned which=0u;which<6u;++which) {
        Limits limits;const auto maximum=Limits{};
        if(which==0u)limits.records=0u;
        if(which==1u)limits.records=maximum.records+1u;
        if(which==2u)limits.ownerVisits=0u;
        if(which==3u)limits.ownerVisits=maximum.ownerVisits+1u;
        if(which==4u)limits.steps=0u;
        if(which==5u)limits.steps=maximum.steps+1u;
        f.reads=0u;Refuse(hit,[&]{run(f.Providers(),limits);});Require(f.reads==0u,"Invalid selector limits reached native providers");
    }
    f.hits.first=2u;Refuse(hit,[&]{run(f.Providers());});
    f.hits={{Packet(0u)},1u};Refuse(hit,[&]{run(f.Providers());});
    f.hits=Chain({Packet(candidate),Packet(candidate+1u)});Limits limits;limits.records=1u;
    Refuse(hit,[&]{run(f.Providers(),limits);});
    f.hits=Chain({Packet(candidate)});f.hits.records.front().next=2u;
    Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,0u,{},{},{},0u,0u,f.Providers());});
    // A selected hit does not inspect its tail, and first=None does not inspect
    // unreachable malformed records or require class/Owner/flag callbacks.
    auto selected=run(f.Providers());Require(!selected.clear&&hit.next==2u,"Accepted hit validated an inactive tail");
    f.hits.first=0u;f.hits.records.front().actor=0u;q=f.Providers();q.isA={};q.owner={};q.flag={};
    const auto unreachable=run(q);Require(unreachable.clear,"Null list head read an unreachable malformed packet");
    f.hits=Chain({Packet(candidate)});f.hits.records.front().next=1u;
    limits=Limits{};limits.steps=19u;Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,0u,{},{},{},0u,0u,f.Providers(),limits);});
    f.hits=Chain({Packet(candidate)});f.Get(source).owner=owner;f.Get(owner).owner=source;
    limits=Limits{};limits.ownerVisits=7u;
    Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0x10u,0u,f.Providers(),limits);});
    // The cumulative Owner cap is shared across different candidates.
    f=Fixture{};f.Get(source).owner=owner;f.hits=Chain({Packet(candidate),Packet(candidate+1u)});
    limits=Limits{};limits.ownerVisits=3u;
    Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0u,0u,f.Providers(),limits);});
    limits.ownerVisits=4u;const auto enough=QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0u,0u,f.Providers(),limits);
    Require(enough.clear&&enough.stats.ownerVisits==4u,"Exact cumulative Owner cap refused complete traversal");
    // The original64-slot collector boundary is retained, not an invented
    // shorter prefix. A64th eligible hit is reachable after63 filtered hits.
    f=Fixture{};f.Get(candidate+1u).cls=4u;
    std::vector<Hit> full;
    for(unsigned i=0u;i<64u;++i)full.push_back(Packet(i==63u?candidate+1u:candidate,i));
    f.hits=Chain(std::move(full));
    const auto last=QuestVr::CheckOriginalSingleLine(hit,0u,{},{},{},1u,0u,f.Providers());
    Require(!last.clear&&last.selected==64u&&Same(hit,f.hits.records.back())&&last.stats.hitVisits==64u,
        "Selector truncated an admitted collector at its64-record boundary");
    f.hits.records.push_back(Packet(candidate));Refuse(hit,[&]{run(f.Providers());});
    // Independent malformed provider ancestry is not converted into clear.
    f=Fixture{};f.hits=Chain({Packet(candidate)});f.Get(candidate).cls=9u;f.classes[9u].parent=9u;
    Refuse(hit,[&]{run(f.Providers());});
}
void SharedLedger() {
    Fixture baseline;baseline.hits=Chain({Packet(candidate)});auto hit=Packet(70u);
    const auto result=baseline.Run(hit,0x30u);const auto cost=result.stats.steps;
    Require(cost==baseline.work&&cost>5u,"Selector work callback missed an active helper action");
    for(std::size_t cut=0u;cut<=cost;++cut) {
        Fixture f;f.hits=baseline.hits;auto q=f.Providers();QuestVr::NativeWork::Budget budget(cut);
        q.work=[&]{budget.Step();};hit=Packet(70u);
        if(cut<cost) {
            const auto before=hit;bool typed{};
            try {QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0x30u,0u,q);}
            catch(const QuestVr::NativeWork::Exhausted& e){typed=true;Require(e.Used()==cut&&e.Limit()==cut,"Selector replaced shared typed exhaustion fields");}
            Require(typed&&Same(hit,before)&&budget.Read().used==cut&&budget.Read().exhausted,
                "Selector swallowed/reset/overspent a shared work refusal");++refusals;
        }else {
            const auto completed=QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0x30u,0u,q);
            Require(completed.clear&&budget.Read().used==cost&&!budget.Read().exhausted,
                "Exact shared work allowance failed successful miss");
        }
        ++ledgerCases;
    }
    // The collector and selector borrow ONE ledger. A collector refusal cannot
    // get a fresh filtering allowance by returning/catching inside a provider.
    for(const std::size_t allowance:{cost+2u,cost+3u}) {
        Fixture f;f.hits=baseline.hits;auto q=f.Providers();QuestVr::NativeWork::Budget budget(allowance);
        const auto collect=q.multiLine;
        q.work=[&]{budget.Step();};q.multiLine=[&](Vec e,Vec s,Vec x,std::int32_t a,Id w,std::uint8_t extra){
            budget.Step(3u);return collect(e,s,x,a,w,extra);
        };
        hit=Packet(70u);
        if(allowance==cost+2u)Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0x30u,0u,q);});
        else {const auto done=QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0x30u,0u,q);
            Require(done.clear&&budget.Read().used==cost+3u,"Collector did not retain the same root work authority");}
        ++ledgerCases;
    }
    Fixture f;f.hits=baseline.hits;auto q=f.Providers();QuestVr::NativeWork::Budget budget(cost*2u-1u);
    q.work=[&]{budget.Step();};hit=Packet(70u);
    static_cast<void>(QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0x30u,0u,q));
    hit=Packet(70u);Refuse(hit,[&]{QuestVr::CheckOriginalSingleLine(hit,source,{},{},{},0x30u,0u,q);});
    Require(budget.Read().used==cost*2u-1u&&budget.Read().exhausted,"Repeated selectors reset the enclosing-root work allowance");++ledgerCases;
}
} // namespace

int main() {
    try {
        ExhaustiveTruth();OwnerDirection();MixedLinkedOracle();OrderAndRawPackets();MissAndInputCopies();LazyTranscripts();RefusalsAndBounds();SharedLedger();
        std::cout<<"PASS original SingleLine selector "<<checks<<" checks, "<<refusals<<" explicit refusals, "
            <<truthCases<<" independent byte/class/flag cases, "<<mixedCases<<" mixed linked-oracle cases, "<<orderCases<<" tied linked orders, "<<rawCases
            <<" raw packet controls, "<<transcriptCases<<" exact lazy transcripts, "<<failurePrefixes<<" provider failure prefixes, "
            <<ledgerCases<<" shared-ledger cases. Not FindBase, support/falling, world startup, crates or headset acceptance.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"Original SingleLine selector failed: "<<error.what()<<'\n';return 1;}
}
