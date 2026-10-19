#include "quest_actor_touch.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Detached original-protocol fixtures. Providers below own only a synthetic
// four-slot world; these tests do NOT implement or accept runtime/map physics.
namespace {
using Id=std::uintptr_t;
using Event=QuestVr::OriginalActorTouchEvent;
using Queries=QuestVr::OriginalActorTouchQueries;
constexpr Id source=1u,other=2u;
std::size_t checks{},cases{},refusals{},layouts{},policies{},mutations{},prefixes{},roots{};
void Require(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
struct Call {
    char kind{};Id actor{},peer{};unsigned slot{};
    bool operator==(const Call&)const=default;
};
struct Actor {
    std::array<Id,4u> slots{};
    std::uint8_t role{3u};bool pawn{},player{},valid{true};
    bool operator==(const Actor&)const=default;
};
struct World {
    std::array<Actor,10u> actors{};
    std::array<unsigned,10u> pawnReads{};
    std::vector<Call> trace,events;
    std::size_t work{},limit=std::numeric_limits<std::size_t>::max(),
        failAt=std::numeric_limits<std::size_t>::max();
    unsigned aux{};bool withWork{true};
    std::function<void(World&,const Call&)> before,after;
    World(){actors[0].valid=false;}
    Actor& At(Id id) {
        if(!id||id>=actors.size())throw std::runtime_error("Mock provider cannot dereference this actor");
        return actors[static_cast<std::size_t>(id)];
    }
    void Work(){if(++work>limit)throw std::runtime_error("Shared synthetic root work exhausted");}
    void Emit(const Call& call) {
        const auto index=trace.size();trace.push_back(call);
        if(index==failAt)throw std::runtime_error("Injected active provider refusal");
        if(before)before(*this,call);
    }
    void After(const Call& call){if(after)after(*this,call);}
    Id Read(Id id,unsigned slot) {
        const Call call{'r',id,0u,slot};Emit(call);
        if(slot>=4u)throw std::runtime_error("Touching exceeded the original four slots");
        const auto value=At(id).slots[slot];After(call);return value;
    }
    void Write(Id id,unsigned slot,Id peer) {
        const Call call{'w',id,peer,slot};Emit(call);
        if(slot>=4u)throw std::runtime_error("Touching wrote outside the original four slots");
        At(id).slots[slot]=peer;After(call);
    }
    void Valid(Id id) {
        const Call call{'v',id};Emit(call);
        if(!At(id).valid)throw std::runtime_error("Mock IsValid refused the active checked pointer");
        After(call);
    }
    std::uint8_t Role(Id id) {
        const Call call{'l',id};Emit(call);const auto value=At(id).role;After(call);return value;
    }
    bool Pawn(Id id) {
        const Call call{'p',id};Emit(call);const auto value=At(id).pawn;
        ++pawnReads[static_cast<std::size_t>(id)];After(call);return value;
    }
    bool Player(Id id) {
        const Call call{'i',id};Emit(call);const auto value=At(id).player;After(call);return value;
    }
    void Notify(Id id,Event event,Id peer) {
        const Call call{event==Event::Touch?'t':'u',id,peer};Emit(call);At(id);
        events.push_back(call);After(call);
    }
    Queries Providers() {
        Queries q;
        q.readSlot=[this](Id id,unsigned slot){return Read(id,slot);};
        q.writeSlot=[this](Id id,unsigned slot,Id peer){Write(id,slot,peer);};
        q.requireValid=[this](Id id){Valid(id);};q.role=[this](Id id){return Role(id);};
        q.pawn=[this](Id id){return Pawn(id);};q.isPlayer=[this](Id id){return Player(id);};
        q.event=[this](Id id,Event event,Id peer){Notify(id,event,peer);};
        if(withWork)q.work=[this]{Work();};
        return q;
    }
};

// Independent interpreter of 10369630..103698B4 and 10369970..10369A48.
// The explicit labels follow original branch targets rather than using any
// production helper, selected-slot shortcut, or precomputed slot snapshot.
struct RetailMachine {
    World& w;
    void Charge(){if(w.withWork)w.Work();}
    Id R(Id id,unsigned slot){Charge();return w.Read(id,slot);}
    void W(Id id,unsigned slot,Id peer){Charge();w.Write(id,slot,peer);}
    void V(Id id){Charge();w.Valid(id);}
    std::uint8_t L(Id id){Charge();return w.Role(id);}
    bool P(Id id){Charge();return w.Pawn(id);}
    bool I(Id id){Charge();return w.Player(id);}
    void E(Id id,Event event,Id peer){Charge();w.Notify(id,event,peer);}
    static void Admit(Id a,Id b){if(!a||!b||a==b)throw std::invalid_argument("Portable admission, not raw invalid memory");}
    void End(Id a,Id b,std::int32_t suppress) {
        Admit(a,b);unsigned index=0u;
    self_scan:
        if(index==4u)goto reciprocal_start;
        if(R(a,index)!=b){++index;goto self_scan;}
        W(a,index,0u);if(!suppress)E(a,Event::UnTouch,b);
    reciprocal_start:
        index=0u;
    reciprocal_scan:
        if(index==4u)return;
        if(R(b,index)!=a){++index;goto reciprocal_scan;}
        W(b,index,0u);E(b,Event::UnTouch,a);
    }
    bool Side(Id a,Id b) {
        Admit(a,b);int selected=-1;unsigned index=0u;Id peer{},classified{};
    initial_scan:
        if(index==4u)goto full_or_insert;
        peer=R(a,index);
        if(!peer)selected=static_cast<int>(index);
        else if(peer==b)return true;
        ++index;goto initial_scan;
    full_or_insert:
        if(selected!=-1)goto insert;
        index=0u;
    role_scan:
        if(index==4u)goto after_roles;
        peer=R(a,index);V(peer);peer=R(a,index);
        if(L(peer)==0u){End(a,peer,0);selected=static_cast<int>(index);}
        ++index;goto role_scan;
    after_roles:
        if(selected!=-1)goto insert;
        if(!P(b))return true;
        index=0u;
    pawn_scan:
        if(index==4u)goto after_pawns;
        classified=R(a,index);
        if(P(classified)){++index;goto pawn_scan;}
        peer=R(a,index);End(a,peer,0);selected=static_cast<int>(index);goto insert;
    after_pawns:
        if(!I(b))return true;
        index=0u;
    player_scan:
        if(index==4u)return true;
        classified=R(a,index);
        if(P(classified)) {
            peer=R(a,index);
            if(I(peer)){++index;goto player_scan;}
        }
        peer=R(a,index);End(a,peer,0);selected=static_cast<int>(index);
    insert:
        W(a,static_cast<unsigned>(selected),b);E(a,Event::Touch,b);
        return R(a,static_cast<unsigned>(selected))==b;
    }
    void Begin(Id a,Id b){if(Side(a,b))static_cast<void>(Side(b,a));}
};
enum class Op {Side,Begin,End};
struct Outcome {bool refused{},result{true};bool operator==(const Outcome&)const=default;};
Outcome Run(World& w,bool retail,Op op,Id a=source,Id b=other,std::int32_t suppress=0) {
    Outcome out;
    try {
        if(retail) {
            RetailMachine machine{w};
            if(op==Op::Side)out.result=machine.Side(a,b);
            else if(op==Op::Begin)machine.Begin(a,b);else machine.End(a,b,suppress);
        } else {
            const auto q=w.Providers();
            if(op==Op::Side)out.result=QuestVr::AddOriginalActorTouchSide(a,b,q);
            else if(op==Op::Begin)QuestVr::BeginOriginalActorTouch(a,b,q);
            else QuestVr::EndOriginalActorTouch(a,b,suppress,q);
        }
    } catch(const std::exception&) {out.refused=true;}
    return out;
}
struct Result {World world;Outcome outcome;};
Result Compare(const World& seed,Op op,Id a=source,Id b=other,std::int32_t suppress=0) {
    World actual=seed,expected=seed;const auto truth=Run(expected,true,op,a,b,suppress),got=Run(actual,false,op,a,b,suppress);
    Require(got==truth,"Touch protocol result/refusal differs from independent original interpreter");
    Require(actual.trace==expected.trace,"Touch protocol provider order/arguments differ from original instruction schedule");
    Require(actual.actors==expected.actors&&actual.events==expected.events&&actual.pawnReads==expected.pawnReads&&actual.aux==expected.aux,
        "Touch callbacks, writes or class/role samples differ from independent original world");
    Require(actual.work==expected.work,"Touching did not share work across every active nested provider");
    ++cases;if(got.refused)++refusals;return {std::move(actual),got};
}
bool Has(const World& w,char kind,Id id) {
    return std::any_of(w.trace.begin(),w.trace.end(),[=](const Call& call){return call.kind==kind&&call.actor==id;});
}
std::size_t Count(const World& w,char kind) {
    return static_cast<std::size_t>(std::count_if(w.trace.begin(),w.trace.end(),[=](const Call& call){return call.kind==kind;}));
}
bool Contains(const World& w,const std::vector<Call>& sequence) {
    return std::search(w.trace.begin(),w.trace.end(),sequence.begin(),sequence.end())!=w.trace.end();
}
World Full() {
    World w;w.actors[source].slots={3u,4u,5u,6u};
    for(Id id=3u;id<=6u;++id)w.actors[id].slots[2u]=source;
    return w;
}
void SlotLayouts() {
    const auto empty=Compare(World{},Op::Side);
    Require(empty.world.trace==std::vector<Call>{{'r',source,0u,0u},{'r',source,0u,1u},
        {'r',source,0u,2u},{'r',source,0u,3u},{'w',source,other,3u},{'t',source,other},{'r',source,0u,3u}},
        "Empty insertion golden transcript differs from complete retail body");
    constexpr std::array<Id,5u> values{0u,other,3u,4u,5u};
    for(unsigned word=0u;word<625u;++word) {
        World seed;auto value=word;int last=-1;bool duplicate{};
        for(unsigned slot=0u;slot<4u;++slot){const auto id=values[value%5u];value/=5u;
            seed.actors[source].slots[slot]=id;if(!id)last=static_cast<int>(slot);if(id==other)duplicate=true;}
        const auto result=Compare(seed,Op::Side);Require(!result.outcome.refused&&result.outcome.result,"Ordinary side layout unexpectedly refused or returned false");
        if(duplicate)Require(result.world.actors==seed.actors&&Count(result.world,'t')==0u&&Count(result.world,'w')==0u,
            "Duplicate insertion wrote an earlier empty slot or delivered Touch");
        else if(last>=0)Require(result.world.actors[source].slots[static_cast<unsigned>(last)]==other&&Count(result.world,'t')==1u,
            "Side insertion did not select exactly the LAST empty slot");
        else Require(result.world.actors==seed.actors&&Count(result.world,'t')==0u,
            "Full non-Pawn/no-eviction path did not retain true without insertion");
        ++layouts;
    }
    for(unsigned a=0u;a<81u;++a)for(unsigned b=0u;b<81u;++b)for(std::int32_t suppress:{0,1,-7}) {
        World seed;auto av=a,bv=b;constexpr std::array<Id,3u> left{0u,other,3u},right{0u,source,4u};
        for(unsigned slot=0u;slot<4u;++slot){seed.actors[source].slots[slot]=left[av%3u];av/=3u;
            seed.actors[other].slots[slot]=right[bv%3u];bv/=3u;}
        const auto result=Compare(seed,Op::End,source,other,suppress);
        auto slots=seed.actors;const auto own=std::find(slots[source].slots.begin(),slots[source].slots.end(),other),
            peer=std::find(slots[other].slots.begin(),slots[other].slots.end(),source);
        const bool selfFound=own!=slots[source].slots.end(),peerFound=peer!=slots[other].slots.end();
        if(selfFound)*own=0u;
        if(peerFound)*peer=0u;
        Require(result.world.actors==slots&&Count(result.world,'u')==static_cast<std::size_t>((selfFound&&!suppress)+peerFound),
            "EndTouch removed duplicates, skipped reciprocal-only removal, or suppressed reciprocal event");++layouts;
    }
}
void ReplacementPolicies() {
    // All 16 Role0 masks, every four-slot non-Pawn/Pawn/player combination,
    // and all three incoming classes. Real native Player binding is NOT used.
    for(unsigned roles=0u;roles<16u;++roles)for(unsigned classes=0u;classes<81u;++classes)for(unsigned incoming=0u;incoming<3u;++incoming) {
        auto seed=Full();auto code=classes;seed.actors[other].pawn=incoming!=0u;seed.actors[other].player=incoming==2u;
        for(unsigned slot=0u;slot<4u;++slot){auto& peer=seed.actors[slot+3u];peer.role=(roles&(1u<<slot))?0u:3u;
            const auto type=code%3u;code/=3u;peer.pawn=type!=0u;peer.player=type==2u;}
        const auto result=Compare(seed,Op::Side);Require(!result.outcome.refused&&result.outcome.result,"Finite static replacement policy refused/failed");
        int selected=-1;unsigned removed{};
        for(unsigned slot=0u;slot<4u;++slot)if(roles&(1u<<slot)){selected=static_cast<int>(slot);++removed;}
        if(selected<0&&incoming)for(unsigned slot=0u;slot<4u;++slot)if(!seed.actors[slot+3u].pawn){selected=static_cast<int>(slot);removed=1u;break;}
        if(selected<0&&incoming==2u)for(unsigned slot=0u;slot<4u;++slot)if(!seed.actors[slot+3u].player){selected=static_cast<int>(slot);removed=1u;break;}
        auto expected=seed.actors;
        for(unsigned slot=0u;slot<4u;++slot)if(roles&(1u<<slot)){expected[source].slots[slot]=0u;expected[slot+3u].slots[2u]=0u;}
        if(selected>=0){const auto slot=static_cast<unsigned>(selected);expected[slot+3u].slots[2u]=0u;expected[source].slots[slot]=other;}
        Require(result.world.actors==expected&&Count(result.world,'u')==2u*removed&&Count(result.world,'t')==static_cast<std::size_t>(selected>=0),
            "Role0 last-selected/all-evictions or Pawn/player first-eligible policy differs from independent truth table");++policies;
    }
}
void CallbackMutations() {
    for(unsigned replacement:{0u,7u}) {
        World seed;seed.after=[=](World& w,const Call& call){if(call.kind=='t'&&call.actor==source){
            w.actors[source].slots[3u]=replacement;w.actors[source].slots[0u]=other;}};
        const auto side=Compare(seed,Op::Side);
        Require(!side.outcome.refused&&!side.outcome.result,"One-sided helper failed to return exact changed-inserted-slot result");
        const auto result=Compare(seed,Op::Begin);
        Require(!result.outcome.refused&&Count(result.world,'t')==1u&&result.world.actors[other].slots==std::array<Id,4u>{},
            "BeginTouch ignored changed exact inserted slot or rescanned a duplicate elsewhere");++mutations;
    }
    World empty;const auto symmetric=Compare(empty,Op::Begin);
    Require(symmetric.world.actors[source].slots[3u]==other&&symmetric.world.actors[other].slots[3u]==source,
        "Ordinary BeginTouch did not use both original one-sided insertions");
    auto saturated=Full();const auto asymmetric=Compare(saturated,Op::Begin);
    Require(asymmetric.world.actors[source].slots==saturated.actors[source].slots&&asymmetric.world.actors[other].slots[3u]==source&&Count(asymmetric.world,'t')==1u,
        "True-with-no-insertion incorrectly prevented reciprocal BeginTouch");++mutations;
    World duplicate;duplicate.actors[source].slots[0u]=other;const auto repeated=Compare(duplicate,Op::Begin);
    Require(Count(repeated.world,'t')==1u&&repeated.world.actors[other].slots[3u]==source,"Duplicate first side blocked reciprocal insertion");++mutations;
    World secondFalse;secondFalse.after=[](World& w,const Call& c){if(c.kind=='t'&&c.actor==other)w.actors[other].slots[3u]=0u;};
    const auto ignored=Compare(secondFalse,Op::Begin);
    Require(!ignored.outcome.refused&&Count(ignored.world,'t')==2u&&ignored.world.actors[source].slots[3u]==other,
        "BeginTouch undid first insertion or treated second false as a transaction failure");++mutations;
    World invalidated;invalidated.after=[](World& w,const Call& c){if(c.kind=='t'&&c.actor==source)w.actors[source].valid=false;};
    const auto noRecheck=Compare(invalidated,Op::Begin);
    Require(!noRecheck.outcome.refused&&Count(noRecheck.world,'v')==0u&&Count(noRecheck.world,'t')==2u,
        "Callback-triggered insertion revalidated actors outside original active validity loop");++mutations;
    World end;end.actors[source].slots={other,other,0u,0u};end.actors[other].slots={source,source,0u,0u};
    end.after=[](World& w,const Call& c){if(c.kind=='u'&&c.actor==source){
        w.actors[source].slots[0u]=other;w.actors[other].slots={0u,0u,0u,source};}};
    const auto liveEnd=Compare(end,Op::End);
    Require(liveEnd.world.actors[source].slots[0u]==other&&liveEnd.world.actors[source].slots[1u]==other&&
        liveEnd.world.actors[other].slots==std::array<Id,4u>{}&&Count(liveEnd.world,'u')==2u,
        "EndTouch rescanned self or sampled reciprocal slots before synchronous self UnTouch");++mutations;
    for(std::int32_t suppress:{std::numeric_limits<std::int32_t>::min(),-1,1,std::numeric_limits<std::int32_t>::max()}) {
        const auto suppressed=Compare(end,Op::End,source,other,suppress);
        Require(!Has(suppressed.world,'u',source)&&Has(suppressed.world,'u',other),"Nonzero suppression affected peer UnTouch");++mutations;
    }
    auto validate=Full();validate.actors[7u].valid=false;validate.actors[7u].role=0u;
    validate.after=[](World& w,const Call& c){if(c.kind=='v'&&c.actor==3u)w.actors[source].slots[0u]=7u;};
    const auto reread=Compare(validate,Op::Side);
    Require(!reread.outcome.refused&&Has(reread.world,'v',3u)&&!Has(reread.world,'v',7u)&&Has(reread.world,'l',7u)&&
        reread.world.actors[source].slots[0u]==other,"Role loop cached checked pointer or revalidated replacement pointer");++mutations;
    Require(Contains(reread.world,{{'r',source,0u,0u},{'v',3u},{'r',source,0u,0u},{'l',7u}}),
        "Golden IsValid/reread/live-Role ordering differs from original loads");
    auto roleCaptured=validate;roleCaptured.after=[](World& w,const Call& c){
        if(c.kind=='v'&&c.actor==3u)w.actors[source].slots[0u]=7u;
        if(c.kind=='l'&&c.actor==7u)w.actors[source].slots[0u]=8u;};
    roleCaptured.actors[7u].slots[0u]=source;
    const auto captured=Compare(roleCaptured,Op::Side);
    Require(captured.world.actors[7u].slots[0u]==0u&&captured.world.actors[source].slots[0u]==other&&Has(captured.world,'u',7u)&&!Has(captured.world,'u',8u),
        "Role0 EndTouch reread peer after role callback or rescanned insertion ownership");++mutations;
    auto later=Full();later.actors[3u].role=0u;later.actors[7u].valid=false;
    later.after=[](World& w,const Call& c){if(c.kind=='u'&&c.actor==source)w.actors[source].slots[1u]=7u;};
    const auto invalidLater=Compare(later,Op::Side);
    Require(invalidLater.outcome.refused&&Has(invalidLater.world,'v',7u)&&!Has(invalidLater.world,'t',source),
        "Role0 loop skipped a callback-modified later index or inserted before finishing all four");++mutations;
    auto liveRole=Full();liveRole.actors[3u].role=0u;
    liveRole.after=[](World& w,const Call& c){if(c.kind=='u'&&c.actor==source)w.actors[6u].role=0u;};
    const auto updated=Compare(liveRole,Op::Side);
    Require(!updated.outcome.refused&&updated.world.actors[source].slots==std::array<Id,4u>{0u,4u,5u,other},
        "Role priority used stale role or first-eviction insertion index");++mutations;
    auto pawn=Full();pawn.actors[other].pawn=true;pawn.actors[7u].pawn=true;pawn.actors[7u].slots[1u]=source;
    pawn.after=[](World& w,const Call& c){if(c.kind=='p'&&c.actor==3u)w.actors[source].slots[0u]=7u;};
    const auto pawnReread=Compare(pawn,Op::Side);
    Require(!pawnReread.outcome.refused&&Has(pawnReread.world,'u',7u)&&!Has(pawnReread.world,'u',3u)&&!Has(pawnReread.world,'p',7u)&&
        pawnReread.world.actors[source].slots[0u]==other,"Pawn eviction did not reread exact eligible slot without reclassification");++mutations;
    Require(Contains(pawnReread.world,{{'p',3u},{'r',source,0u,0u},{'r',source,0u,0u},{'w',source,0u,0u}}),
        "Golden Pawn classification/reread/EndTouch first read ordering differs");
    auto player=Full();player.actors[other].pawn=player.actors[other].player=true;
    for(Id id=3u;id<=6u;++id)player.actors[id].pawn=player.actors[id].player=true;
    player.actors[7u].pawn=true;player.actors[8u].pawn=player.actors[8u].player=true;player.actors[8u].slots[0u]=source;
    player.after=[](World& w,const Call& c){
        if(c.kind=='p'&&c.actor==3u&&w.pawnReads[3u]==2u)w.actors[source].slots[0u]=7u;
        if(c.kind=='i'&&c.actor==7u)w.actors[source].slots[0u]=8u;};
    const auto triple=Compare(player,Op::Side);
    Require(!triple.outcome.refused&&Has(triple.world,'i',7u)&&!Has(triple.world,'i',8u)&&Has(triple.world,'u',8u)&&
        triple.world.actors[source].slots[0u]==other,"Player priority did not observe three distinct slot pointer loads");++mutations;
    Require(Contains(triple.world,{{'p',3u},{'r',source,0u,0u},{'i',7u},{'r',source,0u,0u},{'r',source,0u,0u},{'w',source,0u,0u}}),
        "Golden player classification/bit-test/eviction slot rereads differ");
    auto newlyNonPawn=Full();newlyNonPawn.actors[other].pawn=newlyNonPawn.actors[other].player=true;
    for(Id id=3u;id<=6u;++id)newlyNonPawn.actors[id].pawn=true;
    newlyNonPawn.after=[](World& w,const Call& c){if(c.kind=='p'&&c.actor==6u&&w.pawnReads[6u]==1u)w.actors[3u].pawn=false;};
    const auto reclass=Compare(newlyNonPawn,Op::Side);
    Require(!reclass.outcome.refused&&!Has(reclass.world,'i',3u)&&Has(reclass.world,'u',3u),
        "Final priority failed to reclassify live Pawn state or read inactive non-Pawn player bit");++mutations;
}

void FailurePrefixes() {
    std::vector<std::pair<World,Op>> cohorts;
    World end;end.actors[source].slots[1u]=other;end.actors[other].slots[2u]=source;
    cohorts.push_back({end,Op::End});cohorts.push_back({World{},Op::Side});cohorts.push_back({World{},Op::Begin});
    auto roles=Full();for(Id id=3u;id<=6u;++id)roles.actors[id].role=0u;cohorts.push_back({roles,Op::Side});
    auto pawn=Full();pawn.actors[other].pawn=true;pawn.actors[3u].pawn=true;cohorts.push_back({pawn,Op::Side});
    auto player=Full();player.actors[other].pawn=player.actors[other].player=true;
    for(Id id=3u;id<=6u;++id)player.actors[id].pawn=player.actors[id].player=true;
    player.actors[4u].player=false;cohorts.push_back({player,Op::Side});
    for(const auto& cohort:cohorts) {
        const auto golden=Compare(cohort.first,cohort.second);Require(!golden.outcome.refused,"Failure cohort did not reach original full path");
        for(std::size_t index=0u;index<golden.world.trace.size();++index) {
            auto seed=cohort.first;seed.failAt=index;const auto result=Compare(seed,cohort.second);
            Require(result.outcome.refused&&result.world.trace.size()==index+1u&&
                std::equal(result.world.trace.begin(),result.world.trace.end(),golden.world.trace.begin()),
                "Touch callback/provider refusal continued beyond exact active original prefix");++prefixes;
            seed=cohort.first;seed.limit=index;const auto exhausted=Compare(seed,cohort.second);
            Require(exhausted.outcome.refused&&exhausted.world.work==index+1u&&exhausted.world.trace.size()==index&&
                std::equal(exhausted.world.trace.begin(),exhausted.world.trace.end(),golden.world.trace.begin()),
                "Shared work refusal occurred after provider or reset allowance in nested EndTouch");++prefixes;
        }
        auto noWork=cohort.first;noWork.withWork=false;const auto optional=Compare(noWork,cohort.second);
        Require(!optional.outcome.refused&&optional.world.work==0u&&optional.world.trace==golden.world.trace&&optional.world.actors==golden.world.actors,
            "Optional absent work callback changed original protocol");
    }
    for(const Op op:{Op::Side,Op::Begin,Op::End})for(const auto pair:std::array<std::array<Id,2u>,4u>{{{0u,other},{source,0u},{source,source},{0u,0u}}}) {
        const auto result=Compare(World{},op,pair[0u],pair[1u]);
        Require(result.outcome.refused&&result.world.trace.empty()&&result.world.work==0u,
            "Deliberate portable null/self admission invoked raw providers");
    }
    // Missing active provider is refused BEFORE charging it. Inactive fields
    // can all be absent on paths which never touch their original operation.
    for(char kind:{'r','w','v','l','p','i','t'}) {
        auto seed=player;auto q=seed.Providers();
        if(kind=='r')q.readSlot={};else if(kind=='w')q.writeSlot={};else if(kind=='v')q.requireValid={};
        else if(kind=='l')q.role={};else if(kind=='p')q.pawn={};else if(kind=='i')q.isPlayer={};else q.event={};
        const auto golden=Compare(player,Op::Side);const auto position=std::find_if(golden.world.trace.begin(),golden.world.trace.end(),
            [=](const Call& call){return kind=='t'?(call.kind=='t'||call.kind=='u'):call.kind==kind;});
        Require(position!=golden.world.trace.end(),"Missing-provider fixture did not exercise requested provider");
        bool failed{};try{static_cast<void>(QuestVr::AddOriginalActorTouchSide(source,other,q));}catch(const std::exception&){failed=true;}
        const auto size=static_cast<std::size_t>(position-golden.world.trace.begin());
        Require(failed&&seed.work==size&&seed.trace.size()==size&&std::equal(seed.trace.begin(),seed.trace.end(),golden.world.trace.begin()),
            "Missing provider was preflighted eagerly or charged before availability test");++refusals;
    }
    World lazy;auto q=lazy.Providers();q.requireValid={};q.role={};q.pawn={};q.isPlayer={};
    Require(QuestVr::AddOriginalActorTouchSide(source,other,q)&&lazy.actors[source].slots[3u]==other,
        "Empty slot required inactive validity/class/role providers");
    World duplicate;duplicate.actors[source].slots[0u]=other;auto d=duplicate.Providers();d.writeSlot={};d.event={};d.requireValid={};d.role={};d.pawn={};d.isPlayer={};
    Require(QuestVr::AddOriginalActorTouchSide(source,other,d)&&duplicate.trace.size()==1u,"Duplicate required inactive providers");
    World unmatched;auto e=unmatched.Providers();e.writeSlot={};e.event={};e.requireValid={};e.role={};e.pawn={};e.isPlayer={};
    QuestVr::EndOriginalActorTouch(source,other,0,e);Require(unmatched.trace.size()==8u,"No-match EndTouch read inactive event/metadata providers");
}
void CallerOwnedRoot() {
    // Synthetic root deliberately belongs to the caller, not the wrapper.
    // Provider-owned slot writes and queued event effects survive a refusal
    // until that caller rolls back its complete state. Work never rewinds.
    struct Root {
        World world,before;bool poison{};
        explicit Root(World seed):world(seed),before(std::move(seed)){}
        void Call(const std::function<void()>& run){try{run();}catch(...){poison=true;throw;}}
        void Commit(){if(poison)throw std::runtime_error("Synthetic touching root is poisoned");}
        void Rollback(){world.actors=before.actors;world.events=before.events;world.pawnReads=before.pawnReads;world.aux=before.aux;}
    };
    World seed;seed.before=[](World&,const Call& c){if(c.kind=='t'&&c.actor==other)throw std::runtime_error("Reciprocal event refused");};
    Root failed(seed);auto q=failed.world.Providers();bool refused{};
    try{failed.Call([&]{QuestVr::BeginOriginalActorTouch(source,other,q);});}catch(const std::exception&){refused=true;}
    Require(refused&&failed.poison&&failed.world.actors[source].slots[3u]==other&&failed.world.actors[other].slots[3u]==source&&failed.world.events.size()==1u,
        "Touch protocol hid provider failure or pretended to undo coupled slot/event writes");
    bool rejected{};try{failed.Commit();}catch(const std::exception&){rejected=true;}Require(rejected,"Poisoned mock root published partial touching");
    const auto used=failed.world.work;failed.Rollback();Require(failed.world.actors==failed.before.actors&&failed.world.events.empty()&&failed.world.work==used,
        "Caller-owned rollback did not restore full mock state or incorrectly rewound shared work");++roots;
    Root shared(World{});auto s=shared.world.Providers();shared.Call([&]{QuestVr::BeginOriginalActorTouch(source,other,s);});
    const auto firstWork=shared.world.work;shared.world.limit=firstWork;refused=false;
    try{shared.Call([&]{QuestVr::EndOriginalActorTouch(source,other,0,s);});}catch(const std::exception&){refused=true;}
    Require(refused&&shared.poison&&shared.world.work==firstWork+1u&&shared.world.actors[source].slots[3u]==other,
        "Second touching operation reset shared work or mutated before denied provider");shared.Rollback();
    Require(shared.world.actors==shared.before.actors,"Shared caller failed to rewind successful earlier touching after later refusal");++roots;
    auto full=Full();Root ordinary(full);auto o=ordinary.world.Providers();
    ordinary.Call([&]{Require(QuestVr::AddOriginalActorTouchSide(source,other,o),"Original no-insertion result changed");});ordinary.Commit();
    Require(!ordinary.poison&&ordinary.world.actors==full.actors,"True/no-insertion was treated as rollback/failure");++roots;
}
} // namespace

int main(){try{SlotLayouts();ReplacementPolicies();CallbackMutations();FailurePrefixes();CallerOwnedRoot();
    std::cout<<"PASS original touching "<<checks<<" checks, "<<cases<<" independent interpreter cases, "<<layouts
        <<" exhaustive slot layouts, "<<policies<<" role/class policies, "<<mutations<<" callback mutations, "<<prefixes
        <<" provider/work prefixes, "<<refusals<<" explicit refusals, "<<roots
        <<" caller-owned mock roots. No runtime touching or map-physics acceptance.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"Original touching failed: "<<error.what()<<'\n';return 1;}}
