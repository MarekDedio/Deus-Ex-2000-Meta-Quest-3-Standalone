#include "quest_actor_blocking.h"

#include <array>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

// Independent synthetic actors/classes only. No original DLL is loaded and
// no map other than the caller's first-Training integration is implied here.
namespace {
using Id=std::uintptr_t;
using Class=QuestVr::OriginalBlockingClass;
using Flag=QuestVr::OriginalBlockingFlag;
using Queries=QuestVr::OriginalActorBlockingQueries;
constexpr Id source=11u,candidate=22u,world=33u,unrelated=44u;
std::size_t checks{},refusals{},truthCases{},byteCases{},highBitCases{},localCases{},
    transcriptCases{},failurePrefixes{},baseCases{},liveCases{},classLinkVisits{};
std::array<std::size_t,4u> routes{};
void Require(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void Refuse(F&& run,const char* reason="Unsupported original actor predicate silently succeeded") {
    bool failed{};try {run();}catch(const std::exception&) {failed=true;}
    Require(failed,reason);++refusals;
}
enum class Op { Level,Brush,Player,Class,Flag,Base,Work };
struct Event {
    Op op;Id actor{};unsigned selector{};
    bool operator==(const Event&)const=default;
};
Event L(Id id){return {Op::Level,id};}
Event B(Id id){return {Op::Brush,id};}
Event P(Id id){return {Op::Player,id};}
Event C(Id id,Class cls){return {Op::Class,id,static_cast<unsigned>(cls)};}
Event F(Id id,Flag flag){return {Op::Flag,id,static_cast<unsigned>(flag)};}
Event A(Id id){return {Op::Base,id};}
Event W(){return {Op::Work};}
struct NativeClass {unsigned parent{};std::string_view display;};
struct ModelActor {unsigned cls{2u};Id level{world},brush{},player{},base{};std::uint32_t flags{};};
struct Fixture {
    // Exact class identities are the array positions; display leaf spelling
    // deliberately lies for entries10..12. Inheritance, not names, decides.
    std::array<NativeClass,15u> classes{{{0u,"None"},{0u,"Core.Object"},{1u,"Engine.Actor"},
        {2u,"Engine.Brush"},{2u,"Engine.PlayerPawn"},{2u,"Engine.Projectile"},
        {3u,"Engine.Mover"},{4u,"DeusEx.JCDentonMale"},{5u,"Fixture.ProjectileChild"},
        {2u,"Engine.Decoration"},{9u,"Unrelated.PlayerPawn"},{9u,"Unrelated.Brush"},
        {9u,"Unrelated.Projectile"},{7u,"Fixture.PlayerChild"},{13u,"Fixture.PlayerGrandchild"}}};
    std::array<ModelActor,64u> actors{};
    std::vector<Event> trace;
    std::function<void(const Event&)> beforeRead;
    bool tracing{};
    std::size_t work{};
    ModelActor& Actor(Id id) {
        if(!id||id>=actors.size())throw std::runtime_error("Independent fixture actor is unavailable");
        return actors[static_cast<std::size_t>(id)];
    }
    void Read(Event event) {
        if(tracing)trace.push_back(event);
        if(beforeRead)beforeRead(event);
    }
    bool Derives(unsigned cls,Class requested) {
        const unsigned target=requested==Class::Brush?3u:requested==Class::PlayerPawn?4u:5u;
        std::array<bool,15u> seen{};
        for(;cls;cls=classes.at(cls).parent) {
            ++classLinkVisits;
            if(cls>=seen.size()||seen[cls])throw std::runtime_error("Independent native class chain cycles or is unavailable");
            seen[cls]=true;if(cls==target)return true;
        }
        return false;
    }
    Queries Provider() {
        Queries q;
        q.level=[&](Id id){Read(L(id));return Actor(id).level;};
        q.hasBrush=[&](Id id){Read(B(id));return Actor(id).brush!=0u;};
        q.hasPlayer=[&](Id id){Read(P(id));return Actor(id).player!=0u;};
        q.isA=[&](Id id,Class cls){Read(C(id,cls));return Derives(Actor(id).cls,cls);};
        q.flag=[&](Id id,Flag flag) {
            Read(F(id,flag));const unsigned bit=flag==Flag::CollideWorld?1u:flag==Flag::BlockActors?2u:3u;
            return ((Actor(id).flags>>bit)&1u)!=0u;
        };
        q.base=[&](Id id){Read(A(id));return Actor(id).base;};
        q.work=[&]{Read(W());++work;};return q;
    }
};
struct Archetype {unsigned cls,membership;};
// Independently specified memberships, not obtained from the provider's
// ancestry traversal. Bit1=Brush, bit2=PlayerPawn, bit4=Projectile.
constexpr std::array archetypes{Archetype{2u,0u},Archetype{3u,1u},Archetype{6u,1u},
    Archetype{4u,2u},Archetype{7u,2u},Archetype{5u,4u},Archetype{8u,4u},Archetype{9u,0u},
    Archetype{10u,0u},Archetype{11u,0u},Archetype{12u,0u},Archetype{14u,2u}};
struct Expected {bool blocked;unsigned route;};
Expected Truth(const ModelActor& s,const ModelActor& c,unsigned sm,unsigned cm,bool levelMatch) {
    const bool sp=(sm&2u)!=0u&&s.player!=0u,cp=(cm&2u)!=0u&&c.player!=0u;
    const bool sb=(sm&1u)!=0u&&s.brush!=0u,cb=(cm&1u)!=0u&&c.brush!=0u;
    const auto selected=[](std::uint32_t flags,bool player){return (flags&(player?8u:4u))!=0u;};
    // Closed bitfield truth formula: no production helper, provider calls,
    // ancestry scan, or copied control-flow transcript predicts acceptance.
    const std::array answers{(s.flags&2u)!=0u,
        (s.flags&2u)!=0u&&selected(c.flags,sp),
        (c.flags&2u)!=0u&&selected(s.flags,cp),
        selected(c.flags,sp||(sm&4u)!=0u)&&selected(s.flags,cp||(cm&4u)!=0u)};
    const unsigned route=levelMatch?0u:cb?1u:sb?2u:3u;
    return {answers[route],route};
}
void CheckTruth(Fixture& fixture,const Queries& q,const Archetype& s,const Archetype& c,bool levelMatch) {
    fixture.Actor(source).cls=s.cls;fixture.Actor(candidate).cls=c.cls;
    fixture.Actor(source).level=levelMatch?candidate:world;
    const auto expected=Truth(fixture.Actor(source),fixture.Actor(candidate),s.membership,c.membership,levelMatch);
    const auto actual=QuestVr::IsOriginalActorBlockedBy(source,candidate,q);
    Require(actual==expected.blocked,"Blocking matrix differs from independent bitfield/class truth formula");++routes[expected.route];
}
void ExhaustiveTruth() {
    Fixture fixture;auto q=fixture.Provider();
    for(const auto& s:archetypes)for(const auto& c:archetypes)for(unsigned presence=0u;presence<16u;++presence)
        for(std::uint32_t sf=0u;sf<16u;++sf)for(std::uint32_t cf=0u;cf<16u;++cf)for(const bool levelMatch:{false,true}) {
            auto& sa=fixture.Actor(source);auto& ca=fixture.Actor(candidate);
            sa.brush=(presence&1u)?Id{0xfedcba98u}:0u;ca.brush=(presence&2u)?Id{0x87654321u}:0u;
            sa.player=(presence&4u)?std::numeric_limits<Id>::max():0u;ca.player=(presence&8u)?Id{1u}:0u;
            sa.flags=sf;ca.flags=cf;CheckTruth(fixture,q,s,c,levelMatch);++truthCases;
        }
    // Full authored byte range on each branch, including every unrelated bit.
    // The first factorized cohort already exhausts all relevant Bool bits for
    // every class/pointer combination; this independently exhausts raw bytes.
    struct Scenario {unsigned s,c,presence;bool level;};
    constexpr std::array scenarios{Scenario{0u,0u,0u,true},Scenario{0u,1u,2u,false},
        Scenario{3u,1u,6u,false},Scenario{1u,0u,1u,false},Scenario{1u,3u,9u,false},
        Scenario{0u,0u,0u,false},Scenario{3u,4u,12u,false},Scenario{5u,6u,0u,false},
        Scenario{1u,2u,3u,false},Scenario{8u,9u,15u,false}};
    for(const auto& scenario:scenarios)for(std::uint32_t sf=0u;sf<256u;++sf)for(std::uint32_t cf=0u;cf<256u;++cf) {
        auto& sa=fixture.Actor(source);auto& ca=fixture.Actor(candidate);sa.flags=sf;ca.flags=cf;
        sa.brush=(scenario.presence&1u)?1u:0u;ca.brush=(scenario.presence&2u)?1u:0u;
        sa.player=(scenario.presence&4u)?1u:0u;ca.player=(scenario.presence&8u)?1u:0u;
        CheckTruth(fixture,q,archetypes[scenario.s],archetypes[scenario.c],scenario.level);++byteCases;
    }
    constexpr std::array high{0u,0x80000000u,0x7fffff00u,0xffffff00u,0xa5a50000u};
    for(const auto& scenario:scenarios)for(const auto sh:high)for(const auto ch:high)
        for(std::uint32_t sf=0u;sf<16u;++sf)for(std::uint32_t cf=0u;cf<16u;++cf) {
            auto& sa=fixture.Actor(source);auto& ca=fixture.Actor(candidate);sa.flags=sf|sh;ca.flags=cf|ch;
            sa.brush=(scenario.presence&1u)?1u:0u;ca.brush=(scenario.presence&2u)?1u:0u;
            sa.player=(scenario.presence&4u)?1u:0u;ca.player=(scenario.presence&8u)?1u:0u;
            CheckTruth(fixture,q,archetypes[scenario.s],archetypes[scenario.c],scenario.level);++highBitCases;
        }
    Require(truthCases==1'179'648u&&byteCases==655'360u&&highBitCases==64'000u,
        "Factorized blocking truth cohorts did not exhaust their intended dimensions");
    for(const auto count:routes)Require(count>1000u,"Blocking truth cohort failed to exercise one precedence branch");
}
void LocalPlayer() {
    Fixture fixture;fixture.tracing=true;auto q=fixture.Provider();
    for(const auto& kind:archetypes)for(const Id player:{Id{0u},Id{1u},Id{0x80000000u},std::numeric_limits<Id>::max()}) {
        fixture.Actor(source).cls=kind.cls;fixture.Actor(source).player=player;fixture.trace.clear();
        const auto expected=(kind.membership&2u)&&player?source:0u;
        Require(QuestVr::GetOriginalActorLocalPlayerPawn(source,q)==expected,"Local GetPlayerPawn used pointer sign, leaf spelling, or another receiver");
        const std::vector<Event> trace=(kind.membership&2u)?std::vector<Event>{C(source,Class::PlayerPawn),P(source)}:
            std::vector<Event>{C(source,Class::PlayerPawn)};
        Require(fixture.trace==trace,"Local GetPlayerPawn read inactive Player or unrelated Level/viewport fields");++localCases;
    }
    fixture.Actor(source).cls=9u;fixture.Actor(source).player=0u;
    fixture.Actor(candidate).cls=7u;fixture.Actor(candidate).player=1u;
    fixture.trace.clear();Require(!QuestVr::GetOriginalActorLocalPlayerPawn(source,q)&&
        fixture.trace==std::vector<Event>{C(source,Class::PlayerPawn)},"Local player helper fell back to an unrelated bound PlayerPawn");
    Queries missing;missing.isA=[](Id,Class){return false;};
    Require(!QuestVr::GetOriginalActorLocalPlayerPawn(source,missing),"Nonplayer local helper required inactive Player provider");
    missing.isA=[](Id,Class){return true;};Refuse([&]{QuestVr::GetOriginalActorLocalPlayerPawn(source,missing);});
    Refuse([&]{QuestVr::GetOriginalActorLocalPlayerPawn(source,Queries{});});
    fixture.trace.clear();Refuse([&]{QuestVr::GetOriginalActorLocalPlayerPawn(0u,q);});
    Require(fixture.trace.empty(),"Null local receiver admission touched native providers");
}
struct Answer {Event event;Id value;};
Answer Reply(Event event,Id value){return {event,value};}
struct Protocol {
    const std::vector<Answer>& script;
    std::size_t next{},throwAt=std::numeric_limits<std::size_t>::max();
    Id Read(Event event) {
        Require(next<script.size(),"Blocking helper read an inactive provider after its ordered transcript");
        const auto index=next++;
        Require(script[index].event==event,"Blocking helper provider order differs from explicit source-backed golden transcript");
        if(index==throwAt)throw std::runtime_error("Explicit active native-field provider failed");
        return script[index].value;
    }
    Queries Provider(bool missingInactive=false) {
        Queries q;
        const auto active=[&](Op op) {for(const auto& item:script)if(item.event.op==op)return true;return false;};
        if(!missingInactive||active(Op::Level))q.level=[this](Id id){return Read(L(id));};
        if(!missingInactive||active(Op::Brush))q.hasBrush=[this](Id id){return Read(B(id))!=0u;};
        if(!missingInactive||active(Op::Player))q.hasPlayer=[this](Id id){return Read(P(id))!=0u;};
        if(!missingInactive||active(Op::Class))q.isA=[this](Id id,Class cls){return Read(C(id,cls))!=0u;};
        if(!missingInactive||active(Op::Flag))q.flag=[this](Id id,Flag flag){return Read(F(id,flag))!=0u;};
        if(!missingInactive||active(Op::Base))q.base=[this](Id id){return Read(A(id));};
        if(!missingInactive||active(Op::Work))q.work=[this]{static_cast<void>(Read(W()));};
        return q;
    }
};
void Golden(bool expected,const std::vector<Answer>& script,Id s=source,Id c=candidate) {
    for(const bool omitInactive:{false,true}) {
        Protocol protocol{script};const auto q=protocol.Provider(omitInactive);
        Require(QuestVr::IsOriginalActorBlockedBy(s,c,q)==expected&&protocol.next==script.size(),
            "Blocking helper differs from complete lazy source-backed transcript");++transcriptCases;
    }
    for(std::size_t fail=0u;fail<script.size();++fail) {
        Protocol protocol{script};protocol.throwAt=fail;
        Refuse([&]{QuestVr::IsOriginalActorBlockedBy(s,c,protocol.Provider());},"Active blocking provider exception was converted to a Bool");
        Require(protocol.next==fail+1u,"Blocking helper continued native reads after an active provider failure");++failurePrefixes;
    }
}
void LazyGoldenTranscripts() {
    using Class::Brush;using Class::PlayerPawn;using Class::Projectile;
    using Flag::CollideWorld;using Flag::BlockActors;using Flag::BlockPlayers;
    for(const Id answer:{Id{0u},Id{1u}})Golden(answer!=0u,{Reply(L(source),candidate),Reply(F(source,CollideWorld),answer)});
    // Candidate-brush priority: source.Brush and all projectile classifications
    // are absent even if both actors are brushes or source is a projectile.
    Golden(false,{Reply(L(source),world),Reply(B(candidate),1u),Reply(C(candidate,Brush),1u),Reply(F(source,CollideWorld),0u)});
    Golden(true,{Reply(L(source),world),Reply(B(candidate),1u),Reply(C(candidate,Brush),1u),Reply(F(source,CollideWorld),1u),
        Reply(C(source,PlayerPawn),1u),Reply(P(source),1u),Reply(F(candidate,BlockPlayers),1u)});
    Golden(false,{Reply(L(source),world),Reply(B(candidate),1u),Reply(C(candidate,Brush),1u),Reply(F(source,CollideWorld),1u),
        Reply(C(source,PlayerPawn),0u),Reply(F(candidate,BlockActors),0u)});
    Golden(true,{Reply(L(source),world),Reply(B(candidate),1u),Reply(C(candidate,Brush),1u),Reply(F(source,CollideWorld),1u),
        Reply(C(source,PlayerPawn),1u),Reply(P(source),0u),Reply(F(candidate,BlockActors),1u)});
    Golden(false,{Reply(L(source),world),Reply(B(candidate),0u),Reply(B(source),1u),Reply(C(source,Brush),1u),Reply(F(candidate,CollideWorld),0u)});
    Golden(true,{Reply(L(source),world),Reply(B(candidate),1u),Reply(C(candidate,Brush),0u),Reply(B(source),1u),Reply(C(source,Brush),1u),
        Reply(F(candidate,CollideWorld),1u),Reply(C(candidate,PlayerPawn),0u),Reply(F(source,BlockActors),1u)});
    Golden(false,{Reply(L(source),world),Reply(B(candidate),0u),Reply(B(source),1u),Reply(C(source,Brush),1u),Reply(F(candidate,CollideWorld),1u),
        Reply(C(candidate,PlayerPawn),1u),Reply(P(candidate),1u),Reply(F(source,BlockPlayers),0u)});
    Golden(true,{Reply(L(source),world),Reply(B(candidate),0u),Reply(B(source),1u),Reply(C(source,Brush),1u),Reply(F(candidate,CollideWorld),1u),
        Reply(C(candidate,PlayerPawn),1u),Reply(P(candidate),0u),Reply(F(source,BlockActors),1u)});
    // Ordinary first-bit failure must not classify candidate player/projectile
    // or read any source block bit. False class tests can follow nonnull Brush.
    Golden(false,{Reply(L(source),world),Reply(B(candidate),0u),Reply(B(source),0u),Reply(C(source,PlayerPawn),0u),
        Reply(C(source,Projectile),0u),Reply(F(candidate,BlockActors),0u)});
    Golden(false,{Reply(L(source),world),Reply(B(candidate),1u),Reply(C(candidate,Brush),0u),Reply(B(source),1u),Reply(C(source,Brush),0u),
        Reply(C(source,PlayerPawn),0u),Reply(C(source,Projectile),1u),Reply(F(candidate,BlockPlayers),0u)});
    Golden(true,{Reply(L(source),world),Reply(B(candidate),0u),Reply(B(source),0u),Reply(C(source,PlayerPawn),1u),Reply(P(source),1u),
        Reply(F(candidate,BlockPlayers),1u),Reply(C(candidate,PlayerPawn),1u),Reply(P(candidate),1u),Reply(F(source,BlockPlayers),1u)});
    Golden(true,{Reply(L(source),world),Reply(B(candidate),0u),Reply(B(source),0u),Reply(C(source,PlayerPawn),1u),Reply(P(source),0u),
        Reply(C(source,Projectile),0u),Reply(F(candidate,BlockActors),1u),Reply(C(candidate,PlayerPawn),0u),Reply(C(candidate,Projectile),1u),
        Reply(F(source,BlockPlayers),1u)});
    Golden(false,{Reply(L(source),world),Reply(B(candidate),0u),Reply(B(source),0u),Reply(C(source,PlayerPawn),0u),Reply(C(source,Projectile),1u),
        Reply(F(candidate,BlockPlayers),1u),Reply(C(candidate,PlayerPawn),1u),Reply(P(candidate),0u),Reply(C(candidate,Projectile),0u),
        Reply(F(source,BlockActors),0u)});
    // A synchronous live provider can change the same receiver between reads.
    // Even source==candidate does not authorize a cached classification.
    Golden(true,{Reply(L(source),world),Reply(B(source),0u),Reply(B(source),0u),Reply(C(source,PlayerPawn),0u),Reply(C(source,Projectile),0u),
        Reply(F(source,BlockActors),1u),Reply(C(source,PlayerPawn),1u),Reply(P(source),1u),Reply(F(source,BlockPlayers),1u)},source,source);
    Queries missing;
    Refuse([&]{QuestVr::IsOriginalActorBlockedBy(source,candidate,missing);});
    std::size_t reads{};missing.level=[&](Id){++reads;return world;};
    Refuse([&]{QuestVr::IsOriginalActorBlockedBy(0u,candidate,missing);});
    Refuse([&]{QuestVr::IsOriginalActorBlockedBy(source,0u,missing);});
    Refuse([&]{QuestVr::IsOriginalActorBlockedBy(0u,0u,missing);});
    Require(reads==0u,"Null actor portable admission read native providers");
}
void LiveFieldsAndClassPolicy() {
    Fixture fixture;fixture.tracing=true;auto q=fixture.Provider();
    auto& s=fixture.Actor(source);auto& c=fixture.Actor(candidate);s.cls=9u;c.cls=9u;s.flags=4u;c.flags=4u;
    fixture.beforeRead=[&](const Event& event) {if(event==F(candidate,Flag::BlockActors))c.flags=0u;};
    Require(!QuestVr::IsOriginalActorBlockedBy(source,candidate,q)&&fixture.trace.back()==F(candidate,Flag::BlockActors),
        "Blocking helper eagerly sampled flags or ignored a live first-bit rejection");++liveCases;
    fixture.trace.clear();c.flags=4u;
    fixture.beforeRead=[&](const Event& event) {if(event==F(candidate,Flag::BlockActors)) {c.cls=7u;c.player=1u;s.flags=8u;}};
    Require(QuestVr::IsOriginalActorBlockedBy(source,candidate,q)&&fixture.trace.back()==F(source,Flag::BlockPlayers),
        "Blocking helper cached candidate class/player/source flags before first candidate bit");++liveCases;
    fixture.beforeRead={};s.flags=4u;c.flags=4u;s.cls=2u;c.cls=2u;c.player=0u;
    Require(QuestVr::IsOriginalActorBlockedBy(source,candidate,q),"Repeated predicate retained previous live player classification");++liveCases;
    // A malformed inactive candidate class must not be traversed by the
    // world branch. Active class-chain failure propagates, never becomes false.
    fixture.classes[9u].parent=9u;c.cls=9u;s.level=candidate;fixture.trace.clear();s.flags=2u;
    Require(QuestVr::IsOriginalActorBlockedBy(source,candidate,q)&&fixture.trace==std::vector<Event>{L(source),F(source,Flag::CollideWorld)},
        "World early branch inspected an inactive malformed candidate class");
    s.level=world;c.brush=1u;Refuse([&]{QuestVr::IsOriginalActorBlockedBy(source,candidate,q);},"Active class-chain provider failure was swallowed");
}
void BasedChains() {
    Fixture fixture;fixture.tracing=true;auto q=fixture.Provider();
    for(const Id target:{Id{0u},source,candidate}) {
        fixture.trace.clear();Require(!QuestVr::IsOriginalActorBasedOn(0u,target,q,0u)&&fixture.trace.empty(),
            "Null Base-chain receiver read providers, charged work, or matched None");++baseCases;
    }
    Queries noFields;
    Require(QuestVr::IsOriginalActorBasedOn(source,source,noFields,1u),"Self Base-chain equality required a Base provider");++baseCases;
    Refuse([&]{QuestVr::IsOriginalActorBasedOn(source,source,noFields,0u);},"Zero portable traversal budget admitted even a self visit");
    Refuse([&]{QuestVr::IsOriginalActorBasedOn(source,candidate,noFields,1u);},"Active Base read required no provider");
    fixture.Actor(source).base=candidate;fixture.Actor(candidate).base=unrelated;fixture.Actor(unrelated).base=0u;
    struct Case {Id from,to;std::size_t maximum;bool result;std::vector<Event> events;};
    const std::vector<Case> cases{
        {source,source,1u,true,{W()}},
        {source,candidate,2u,true,{W(),A(source),W()}},
        {source,unrelated,3u,true,{W(),A(source),W(),A(candidate),W()}},
        {candidate,source,2u,false,{W(),A(candidate),W(),A(unrelated)}},
        {source,world,3u,false,{W(),A(source),W(),A(candidate),W(),A(unrelated)}},
        {source,0u,3u,false,{W(),A(source),W(),A(candidate),W(),A(unrelated)}}};
    for(const auto& item:cases) {
        fixture.trace.clear();Require(QuestVr::IsOriginalActorBasedOn(item.from,item.to,q,item.maximum)==item.result&&fixture.trace==item.events,
            "Base-chain direction/self/None or exact visit/read order differs from golden chain");++baseCases;
    }
    fixture.trace.clear();Refuse([&]{QuestVr::IsOriginalActorBasedOn(source,world,q,2u);});
    Require(fixture.trace==std::vector<Event>{W(),A(source),W(),A(candidate)},"Base limit charged/read a forbidden extra node");
    // An ordinary acyclic chain can be much deeper than a C++ recursion guard.
    Queries deep;constexpr Id length=8192u;std::size_t reads{},work{};
    deep.base=[&](Id id){++reads;return id<length?id+1u:0u;};deep.work=[&]{++work;};
    Require(QuestVr::IsOriginalActorBasedOn(1u,length,deep,length)&&reads==length-1u&&work==length,
        "Deep iterative Base chain lost exact final self comparison or work count");++baseCases;
    reads=work=0u;Require(!QuestVr::IsOriginalActorBasedOn(1u,0u,deep,length)&&reads==length&&work==length,
        "Deep chain matched terminal None or skipped its final Base read");++baseCases;
    fixture.Actor(candidate).base=source;fixture.trace.clear();
    Require(QuestVr::IsOriginalActorBasedOn(source,candidate,q,2u),"Cycle containing target failed before its original early match");++baseCases;
    fixture.trace.clear();Refuse([&]{QuestVr::IsOriginalActorBasedOn(source,world,q,5u);},"Cycle exhaustion fabricated a false Base result");
    Require(fixture.trace==std::vector<Event>{W(),A(source),W(),A(candidate),W(),A(source),W(),A(candidate),W(),A(source)},
        "Cycle budget changed native live traversal order or admitted an extra visit");
    fixture.Actor(source).base=source;fixture.trace.clear();
    Require(QuestVr::IsOriginalActorBasedOn(source,source,q,1u)&&fixture.trace==std::vector<Event>{W()},"Self target unnecessarily traversed a cyclic Base");++baseCases;
    fixture.trace.clear();Refuse([&]{QuestVr::IsOriginalActorBasedOn(source,world,q,3u);});
    Require(fixture.trace==std::vector<Event>{W(),A(source),W(),A(source),W(),A(source)},"Self-cycle refusal normalized the source into false");
    const std::vector<Answer> successful{Reply(W(),0u),Reply(A(source),candidate),Reply(W(),0u),Reply(A(candidate),0u)};
    for(std::size_t fail=0u;fail<successful.size();++fail) {
        Protocol protocol{successful};protocol.throwAt=fail;
        Refuse([&]{QuestVr::IsOriginalActorBasedOn(source,world,protocol.Provider(),8u);},"Base/work provider failure was converted to a result");
        Require(protocol.next==fail+1u,"Base traversal continued after provider refusal");++failurePrefixes;
    }
    // The provider's enclosing work allowance is shared across calls; passing
    // a large local maximum must not reset it. Helpers propagate its refusal.
    std::size_t shared{};Queries budget;budget.base=[](Id){return Id{0u};};
    budget.work=[&]{if(shared==2u)throw std::runtime_error("Shared caller work exhausted");++shared;};
    Require(!QuestVr::IsOriginalActorBasedOn(source,world,budget)&&!QuestVr::IsOriginalActorBasedOn(source,world,budget),
        "Shared Base-work success controls changed results");
    Refuse([&]{QuestVr::IsOriginalActorBasedOn(source,source,budget);},"Fresh Base call ignored existing caller work exhaustion");
    Require(shared==2u,"Refused Base call reset or overcharged its shared caller ledger");
    // Base fields remain live across reads: the work callback before visit2
    // redirects candidate.Base without a cached aggregate or target read.
    fixture.Actor(source).base=candidate;fixture.Actor(candidate).base=unrelated;fixture.Actor(unrelated).base=0u;
    fixture.trace.clear();fixture.work=0u;fixture.beforeRead=[&](const Event& event) {
        if(event==W()&&fixture.work==1u)fixture.Actor(candidate).base=world;
    };
    Require(QuestVr::IsOriginalActorBasedOn(source,world,q,3u)&&fixture.trace==
        std::vector<Event>{W(),A(source),W(),A(candidate),W()},"Base traversal cached later fields or reversed target lookup");++liveCases;
}
} // namespace
int main() {
    try {ExhaustiveTruth();LocalPlayer();LazyGoldenTranscripts();LiveFieldsAndClassPolicy();BasedChains();
        std::cout<<"PASS original actor blocking "<<checks<<" checks / "<<refusals<<" explicit refusals; "
            <<truthCases<<" factorized class/bitfield cases, "<<byteCases<<" full-byte pairs, "<<highBitCases
            <<" high-bit pairs, routes "<<routes[0]<<'/'<<routes[1]<<'/'<<routes[2]<<'/'<<routes[3]<<"; "
            <<localCases<<" local-player cases, "<<transcriptCases<<" lazy transcripts, "<<failurePrefixes
            <<" active failure prefixes, "<<baseCases<<" Base controls, "<<liveCases<<" live mutations, "
            <<classLinkVisits<<" independent class-link visits. No Level encroachment/Move completion claim.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Original actor blocking failed: "<<error.what()<<'\n';return 1;}
}
