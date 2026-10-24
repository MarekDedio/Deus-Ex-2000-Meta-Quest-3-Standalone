#include "quest_actor_animation_phase.h"

#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {
using F=QuestVr::OriginalActorAnimationFloat;
using B=QuestVr::OriginalActorAnimationBool;
using Q=QuestVr::OriginalActorAnimationPhaseQueries;
using Stats=QuestVr::OriginalActorAnimationPhaseStats;
constexpr std::uintptr_t Actor=7u,Hit=1u,Other=2u,AnimEnd=99u;
unsigned checks{},refusals{},oracleCases{};
void Require(bool condition,const char* reason) {
    ++checks;if(!condition)throw std::runtime_error(reason);
}
std::uint32_t Raw(float value) {return std::bit_cast<std::uint32_t>(value);}
[[maybe_unused]] float Float(std::uint32_t value) {return std::bit_cast<float>(value);}
std::size_t Index(F field) {return static_cast<std::size_t>(field);}
const char* Label(F field) {
    constexpr std::array names{"Frame","Rate","TweenRate","Last","MinRate",
        "VelocityX","VelocityY","VelocityZ","SimX","SimY"};
    return names.at(Index(field));
}
struct Notice {float time;std::string function;};
struct Event {
    std::uintptr_t function;
    float frame,rate;
    bool finished;
};
struct State {
    std::array<float,10u> fields{0.0f,1.0f,0.0f,0.875f,0.0f,0.0f,0.0f,0.0f,17.0f,19.0f};
    bool notify{},loop{},finished{},weapon{},deleted{};
    std::uint8_t role{2u};
    std::uint32_t latent{};
    std::string sequence{"Attack"};
    std::uintptr_t mesh{100u};
    float& At(F field) {return fields.at(Index(field));}
    float At(F field) const {return fields.at(Index(field));}
};
bool Same(const State& a,const State& b) {
    for(std::size_t i=0u;i<a.fields.size();++i)if(Raw(a.fields[i])!=Raw(b.fields[i]))return false;
    return a.notify==b.notify && a.loop==b.loop && a.finished==b.finished &&
        a.weapon==b.weapon && a.deleted==b.deleted && a.role==b.role &&
        a.latent==b.latent && a.sequence==b.sequence && a.mesh==b.mesh;
}
[[maybe_unused]] bool Same(const std::vector<Event>& a,const std::vector<Event>& b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0u;i<a.size();++i)
        if(a[i].function!=b[i].function || Raw(a[i].frame)!=Raw(b[i].frame) ||
            Raw(a[i].rate)!=Raw(b[i].rate) || a[i].finished!=b[i].finished)return false;
    return true;
}
template<class Action> void Refuses(Action action,const char* label) {
    bool failed{};
    try {action();}catch(const std::runtime_error&) {failed=true;}
    ++refusals;Require(failed,label);
}
struct Fixture {
    State state;
    Q query;
    std::map<std::uintptr_t,std::vector<Notice>> notices{{100u,{}}};
    std::vector<std::string> calls;
    std::vector<Event> events;
    std::function<void(std::uintptr_t)> onEvent;
    std::size_t writes{},work{},writeCut{std::numeric_limits<std::size_t>::max()},
        workCut{std::numeric_limits<std::size_t>::max()};
    Fixture() {
        query.work=[this] {
            if(work==workCut)throw std::runtime_error("injected native work cutoff");
            ++work;
        };
        query.readFloat=[this](std::uintptr_t actor,F field) {
            Identity(actor);calls.emplace_back(std::string("read:")+Label(field));return state.At(field);
        };
        query.writeFloat=[this](std::uintptr_t actor,F field,float value) {
            Identity(actor);Store(field,value);
        };
        query.readBool=[this](std::uintptr_t actor,B field) {
            Identity(actor);calls.emplace_back(field==B::Notify?"read:Notify":"read:Loop");
            return field==B::Notify?state.notify:state.loop;
        };
        query.writeFinished=[this](std::uintptr_t actor,bool value) {
            Identity(actor);BeforeWrite();calls.emplace_back("write:Finished");state.finished=value;
        };
        query.sequence=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("sequence");return state.sequence;
        };
        query.mesh=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("mesh");return state.mesh;
        };
        query.meshSequence=[this](std::uintptr_t mesh,const std::string& name) {
            calls.emplace_back("meshSequence:"+name);
            return name=="Unknown"?std::uintptr_t{}:mesh;
        };
        query.notifyCount=[this](std::uintptr_t sequence) {
            calls.emplace_back("notifyCount");return notices.at(sequence).size();
        };
        query.notifyTime=[this](std::uintptr_t sequence,std::size_t slot) {
            calls.emplace_back("notifyTime:"+std::to_string(slot));return notices.at(sequence).at(slot).time;
        };
        query.notifyFunction=[this](std::uintptr_t sequence,std::size_t slot) {
            calls.emplace_back("notifyFunction:"+std::to_string(slot));
            Require(Raw(state.At(F::Frame))==Raw(notices.at(sequence).at(slot).time),
                "Notify function name was fetched before the boundary store");
            return notices.at(sequence).at(slot).function;
        };
        query.findFunction=[this](std::uintptr_t actor,const std::string& name) {
            Identity(actor);calls.emplace_back("find:"+name);
            return name=="Hit"?Hit:name=="Other"?Other:std::uintptr_t{};
        };
        query.findAnimEnd=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("find:AnimEnd");return AnimEnd;
        };
        query.processEvent=[this](std::uintptr_t actor,std::uintptr_t function) {
            Identity(actor);calls.emplace_back("event:"+std::to_string(function));
            events.push_back({function,state.At(F::Frame),state.At(F::Rate),state.finished});
            if(onEvent)onEvent(function);
        };
        query.remoteRole=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("remoteRole");return state.role;
        };
        query.isWeapon=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("isWeapon");return state.weapon;
        };
        query.nativeLatentAction=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("latent");return state.latent;
        };
    }
    void Identity(std::uintptr_t actor) {
        Require(actor==Actor,"A callback changed the retained source identity");
        // A deferred-deleted UObject remains retained: deliberately no new
        // deleted/world-slot/renderer-active eligibility check here.
    }
    void BeforeWrite() {
        if(writes==writeCut)throw std::runtime_error("injected native write cutoff");
        ++writes;
    }
    void Store(F field,float value) {
        BeforeWrite();calls.emplace_back(std::string("write:")+Label(field));state.At(field)=value;
    }
    Stats Advance(float elapsed) {
        return QuestVr::AdvanceOriginalActorMainAnimation(Actor,elapsed,query);
    }
    // A small test journal exercises the provider contract, not the real
    // runtime journal. Production integration must use its existing one root.
    Stats Root(float elapsed) {
        const auto before=state;
        const auto oldEvents=events;
        try {return Advance(elapsed);}catch(...) {state=before;events=oldEvents;throw;}
    }
    std::size_t First(const std::string& name) const {
        const auto found=std::find(calls.begin(),calls.end(),name);
        return static_cast<std::size_t>(found-calls.begin());
    }
};

void LazyGates() {
    const float nan=std::numeric_limits<float>::quiet_NaN();
    Q onlyNone;
    onlyNone.sequence=[](std::uintptr_t) {return std::string("nOnE");};
    const auto none=QuestVr::AdvanceOriginalActorMainAnimation(Actor,nan,onlyNone);
    Require(none.inactive && none.iterations==0u && none.steps==1u,"None gate eagerly touched clock/delta providers");
    Fixture frozen;
    frozen.state.At(F::Rate)=-0.0f;
    frozen.state.At(F::Last)=nan;
    auto result=frozen.Advance(nan);
    Require(result.inactive && frozen.writes==0u && result.floatReads==2u,
        "Frozen rate did not lazily preserve invalid inactive data");
    Fixture tween;
    tween.state.At(F::Frame)=-0.25f;
    result=tween.Advance(nan);
    Require(result.inactive && tween.writes==0u && result.floatReads==2u,
        "Frozen tween was not a lazy exit");
    Fixture negative;
    result=negative.Advance(-1.0f);
    Require(result.inactive && result.iterations==0u && negative.writes==0u,
        "Finite negative elapsed was not the original inactive gate");
    Fixture missing;
    missing.query.readFloat={};
    Refuses([&] {missing.Advance(0.25f);},"Missing active field provider was accepted");
    Fixture bad;
    Refuses([&] {bad.Root(nan);},"Active nonfinite elapsed was accepted");
    bad.state.At(F::Frame)=nan;
    Refuses([&] {bad.Root(0.25f);},"Active nonfinite frame was accepted");
    Fixture negativeZero;
    negativeZero.state.At(F::Frame)=-0.0f;
    negativeZero.state.At(F::Rate)=0.0f;
    negativeZero.state.At(F::TweenRate)=1.0f;
    result=negativeZero.Advance(1.0f);
    Require(result.inactive && Raw(negativeZero.state.At(F::Frame))==0x80000000u,
        "Negative-zero Frame incorrectly entered tween playback");
}

void PositiveAndNotify() {
    Fixture simple;
    const auto step=simple.Advance(0.25f);
    Require(simple.state.At(F::Frame)==0.25f && simple.events.empty() && step.iterations==1u,
        "Ordinary positive-frame playback changed end/event state");
    Require(step.remainingSeconds==0.25f && simple.state.At(F::Rate)==1.0f,
        "Native remaining local was mislabeled/rewritten as consumed time");
    Require(simple.First("read:Notify")<simple.First("write:Frame"),
        "Notify flag read did not precede tentative Float store");

    Fixture ordered;
    ordered.state.notify=true;
    ordered.notices[100u]={{0.75f,"Hit"},{0.25f,"Other"},{0.5f,"Hit"}};
    auto result=ordered.Advance(1.0f);
    Require(ordered.events.size()==4u && ordered.events[0].function==Other &&
        ordered.events[0].frame==0.25f && ordered.events[1].frame==0.5f &&
        ordered.events[2].frame==0.75f && ordered.events[3].function==AnimEnd,
        "Unsorted authored notifies were not selected nearest-first before AnimEnd");
    Require(result.iterations==4u && !result.iterationCap && ordered.state.At(F::Frame)==0.875f &&
        ordered.state.At(F::Rate)==0.0f && ordered.state.finished,
        "Notify residual/end state or lazy gate before iteration cap changed");

    Fixture duplicate;
    duplicate.state.notify=true;duplicate.state.At(F::Last)=2.0f;
    duplicate.notices[100u]={{0.5f,"Hit"},{0.5f,"Other"}};
    duplicate.Advance(0.75f);
    Require(duplicate.events.size()==1u && duplicate.events[0].function==Hit,
        "Exactly representable equal notify delta failed strict-less first tie");
    Fixture roundedDuplicate;
    roundedDuplicate.state.notify=true;roundedDuplicate.state.At(F::Frame)=0.1f;
    roundedDuplicate.state.At(F::Last)=2.0f;
    roundedDuplicate.notices[100u]={{0.5f,"Hit"},{0.5f,"Other"}};
    roundedDuplicate.Advance(0.5f);
    Require(roundedDuplicate.events.size()==1u && roundedDuplicate.events[0].function==Other,
        "PC64 equal-time notify delta was not compared to rounded Float best");

    Fixture beyondInitializer;
    beyondInitializer.state.notify=true;beyondInitializer.state.At(F::Frame)=1.0f;
    beyondInitializer.state.At(F::Last)=400'000.0f;
    beyondInitializer.notices[100u]={{200'001.0f,"Hit"}};
    beyondInitializer.Advance(300'000.0f);
    Require(beyondInitializer.events.size()==1u && beyondInitializer.events[0].frame==200'001.0f,
        "100000 best-distance initializer wrongly capped first reached notify");

    Fixture absent;
    absent.state.notify=true;
    absent.notices[100u]={{0.1f,"Missing"},{0.2f,"Missing"},{0.3f,"Missing"},
        {0.4f,"Missing"},{0.5f,"Hit"}};
    result=absent.Advance(1.0f);
    Require(result.iterationCap && result.iterations==4u && result.missingHandlers==4u &&
        absent.events.empty() && absent.state.At(F::Frame)==0.4f,
        "Missing handlers were filtered before boundary commit/literal four-iteration cap");
    Require(absent.First("write:Frame")<absent.First("notifyFunction:0") &&
        absent.First("notifyFunction:0")<absent.First("find:Missing"),
        "Notify boundary/name/FindFunction order changed");

    for(const bool missingMesh:{false,true}) {
        Fixture inactive;
        inactive.state.notify=true;
        if(missingMesh)inactive.state.mesh=0u;else inactive.state.sequence="Unknown";
        inactive.query.notifyCount={};
        result=inactive.Advance(0.25f);
        Require(inactive.state.At(F::Frame)==0.25f && result.notifySearches==0u,
            "Missing actual mesh/selected sequence guessed a fallback or read notify table");
    }
    Fixture beforeEnd;
    beforeEnd.state.notify=true;beforeEnd.state.At(F::Last)=0.1f;
    beforeEnd.notices[100u]={{0.5f,"Hit"}};
    beforeEnd.Advance(0.5f);
    Require(beforeEnd.events.size()==1u && beforeEnd.events.front().function==Hit &&
        beforeEnd.events.front().frame==0.5f,
        "AnimLast was clamped before the full tentative notify search");
}

void TweenAndLoop() {
    Fixture negative;
    negative.state.At(F::Frame)=-0.5f;negative.state.At(F::TweenRate)=1.0f;
    auto result=negative.Advance(0.125f);
    Require(negative.state.At(F::Frame)==-0.375f && negative.events.empty() && result.iterations==1u,
        "Unfinished tween incorrectly entered main playback or dispatched AnimEnd");
    Fixture subnormal;
    subnormal.state.At(F::Frame)=-std::numeric_limits<float>::denorm_min();
    subnormal.state.At(F::TweenRate)=std::numeric_limits<float>::denorm_min();
    subnormal.state.At(F::Rate)=0.0f;
    result=subnormal.Advance(0.5f);
    Require(Raw(subnormal.state.At(F::Frame))==0x80000000u && !subnormal.state.finished &&
        subnormal.events.empty() && result.iterations==1u,
        "Tween compared its rounded negative-zero spill instead of retained negative PC64 sum");
    Fixture transition;
    transition.state.At(F::Frame)=-0.25f;transition.state.At(F::TweenRate)=1.0f;
    result=transition.Advance(0.5f);
    Require(transition.state.At(F::Frame)==0.25f && result.iterations==2u &&
        result.remainingSeconds==0.25f && transition.events.empty(),
        "Tween crossing failed to carry exact residual into fresh main animation");
    Fixture pure;
    pure.state.At(F::Frame)=-0.25f;pure.state.At(F::TweenRate)=1.0f;
    pure.state.At(F::Rate)=0.0f;
    result=pure.Advance(0.5f);
    Require(pure.state.At(F::Frame)==0.0f && pure.state.finished && pure.events.size()==1u &&
        pure.events[0].function==AnimEnd && pure.events[0].finished && result.remainingSeconds==0.25f,
        "Completed pure tween did not finish before selected AnimEnd");
    Require(result.roleReads==0u && result.weaponChecks==0u,
        "Pure tween acquired nonloop terminal SimAnim tail");

    for(const auto action:{0u,385u,386u})for(const bool suppress:{false,true}) {
        Fixture loop;
        loop.state.loop=true;loop.state.At(F::Frame)=0.75f;loop.state.latent=action;
        loop.query.suppressAnimEnd=suppress;
        result=loop.Advance(0.125f);
        Require(loop.state.At(F::Frame)==0.875f && loop.state.At(F::Rate)==1.0f &&
            result.remainingSeconds==0.0f && loop.state.finished==(action==385u),
            "Loop Last crossing confused latent native385 with a class/name or stopped rate");
        Require(loop.events.size()==(suppress?0u:1u) && result.latentReads==1u,
            "Captured simulated-Pawn suppression changed loop completion instead of AnimEnd only");
    }
    Fixture alreadyPast;
    alreadyPast.state.loop=true;alreadyPast.state.At(F::Frame)=0.875f;
    alreadyPast.query.nativeLatentAction={};
    result=alreadyPast.Advance(0.25f);
    Require(alreadyPast.state.At(F::Frame)==0.125f && alreadyPast.events.empty() &&
        result.iterations==2u && result.latentReads==0u,
        "Loop wrapping from old>=Last invented another AnimEnd/FinishAnim query");
    Fixture capped;
    capped.state.loop=true;
    result=capped.Advance(10.0f);
    Require(result.iterationCap && result.iterations==4u && capped.events.size()==4u &&
        capped.state.At(F::Frame)==0.0f && result.remainingSeconds==6.0f,
        "Retail literal four iterations was replaced with catch-up or variable budget");
    Fixture stoppedLoop;
    stoppedLoop.state.loop=true;stoppedLoop.state.At(F::Last)=0.0f;
    stoppedLoop.query.nativeLatentAction={};
    result=stoppedLoop.Advance(0.5f);
    Require(stoppedLoop.events.empty() && result.latentReads==0u &&
        stoppedLoop.state.At(F::Frame)==0.5f && result.remainingSeconds==0.0f,
        "Loop old==Last was treated as a crossing");
}

void TerminalAndCallbacks() {
    Fixture terminal;
    terminal.state.At(F::Frame)=1.0f;
    auto result=terminal.Advance(0.25f);
    Require(terminal.events.size()==1u && terminal.events[0].function==AnimEnd &&
        terminal.events[0].rate==0.0f && terminal.events[0].finished &&
        terminal.events[0].frame==0.875f && result.remainingSeconds==0.375f,
        "Original past-end completion was silently corrected/clamped or skipped");
    Require(terminal.First("write:Rate")<terminal.First("write:Finished") &&
        terminal.First("write:Finished")<terminal.First("event:99"),
        "Nonloop completion stores did not precede selected AnimEnd");

    Fixture replacement;
    replacement.state.notify=true;replacement.state.At(F::Last)=2.0f;
    replacement.notices[100u]={{0.25f,"Hit"}};
    replacement.notices[200u]={{0.5f,"Other"}};
    replacement.onEvent=[&](std::uintptr_t function) {
        if(function==Hit) {
            replacement.Store(F::Frame,0.0f);replacement.Store(F::Rate,2.0f);
            replacement.state.mesh=200u;replacement.state.sequence="Replacement";
        }else replacement.state.sequence="None";
    };
    result=replacement.Advance(1.0f);
    Require(replacement.events.size()==2u && replacement.events[0].function==Hit &&
        replacement.events[1].function==Other && replacement.events[1].frame==0.5f &&
        result.iterations==2u && replacement.state.At(F::Rate)==2.0f &&
        replacement.state.sequence=="None" && replacement.state.mesh==200u,
        "Notify continuation reinstalled captured clock/mesh/rate after synchronous callback");
    Require(replacement.First("event:1")<replacement.First("meshSequence:Replacement"),
        "Replacement notify source was not looked up freshly after callback");

    Fixture liveTail;
    liveTail.state.role=3u;
    liveTail.onEvent=[&](std::uintptr_t function) {
        Require(function==AnimEnd,"Unexpected event before terminal callback");
        liveTail.Store(F::Frame,0.25f);liveTail.Store(F::Rate,10.0f);
        liveTail.state.role=1u;liveTail.state.sequence="None";liveTail.state.deleted=true;
    };
    result=liveTail.Advance(1.0f);
    Require(liveTail.state.At(F::SimX)==2500.0f && liveTail.state.At(F::SimY)==32767.0f &&
        liveTail.state.At(F::Rate)==10.0f && liveTail.state.At(F::Frame)==0.25f &&
        result.roleReads==1u && result.weaponChecks==1u,
        "Terminal SimAnim tail used pre-event values or a new deletion/active gate");
    Require(liveTail.First("event:99")<liveTail.First("remoteRole") &&
        liveTail.First("write:SimX")<liveTail.First("write:SimY"),
        "Fresh terminal role/SimAnim tail moved before selected AnimEnd");
    std::size_t simYStores{};
    for(const auto& call:liveTail.calls)simYStores+=call=="write:SimY";
    Require(simYStores==2u,"SimAnim.Y tentative Float and upper-clamp stores were merged");

    for(const auto role:{1u,2u})for(const bool weapon:{false,true}) {
        Fixture exempt;
        exempt.state.role=static_cast<std::uint8_t>(role);exempt.state.weapon=weapon;
        exempt.query.suppressAnimEnd=true;
        if(role>=2u)exempt.query.isWeapon={};
        result=exempt.Advance(1.0f);
        Require(exempt.events.empty() && result.weaponChecks==(role<2u?1u:0u) &&
            exempt.state.At(F::SimX)==(role<2u&&!weapon?8750.0f:17.0f),
            "Terminal role/real Weapon ancestry gates or captured AnimEnd suppression changed");
    }
    Fixture suppressedNotify;
    suppressedNotify.query.suppressAnimEnd=true;suppressedNotify.state.notify=true;
    suppressedNotify.notices[100u]={{0.5f,"Hit"}};
    result=suppressedNotify.Advance(1.0f);
    Require(suppressedNotify.events.size()==1u && suppressedNotify.events[0].function==Hit &&
        result.animEndFinds==0u,"Simulated-Pawn AnimEnd suppression incorrectly suppressed mesh notify");
}

void RefusalAndJournal() {
    Fixture budget;
    budget.state.notify=true;budget.notices[100u]={{0.1f,"Hit"},{0.2f,"Other"}};
    budget.query.maxNotifySearches=1u;
    const auto before=budget.state;
    Refuses([&] {budget.Root(0.5f);},"Notify search budget was ignored");
    Require(Same(before,budget.state) && budget.events.empty(),"Budget refusal escaped caller journal undo");
    Fixture name;
    name.query.maxNameBytes=3u;
    Refuses([&] {name.Root(0.5f);},"Live sequence name budget was ignored");
    Fixture functionName;
    functionName.state.notify=true;functionName.notices[100u]={{0.5f,"MissingTooLong"}};
    functionName.query.maxNameBytes=8u;
    Refuses([&] {functionName.Root(0.5f);},"Selected notify function-name budget was ignored");
    Fixture disabledScript;
    disabledScript.query.findAnimEnd=[](std::uintptr_t) {return std::uintptr_t{};};
    disabledScript.Root(1.0f);
    Require(disabledScript.events.size()==1u && disabledScript.events[0].function==0u &&
        disabledScript.state.finished && disabledScript.state.At(F::Frame)==0.875f,
        "Disabled-scriptability checked-null was not passed to the real ProcessEvent provider");
    Fixture missingAnimEnd;
    missingAnimEnd.query.findAnimEnd=[](std::uintptr_t)->std::uintptr_t {
        throw std::runtime_error("native checked lookup failed with scripting enabled");
    };
    const auto missingBefore=missingAnimEnd.state;
    Refuses([&] {missingAnimEnd.Root(1.0f);},"Authentic checked-handler failure was swallowed");
    Require(Same(missingBefore,missingAnimEnd.state),"Missing checked handler retained terminal property stores");
    Fixture zeroDivide;
    zeroDivide.state.At(F::Frame)=1.0f;zeroDivide.state.At(F::Last)=1.0f;
    zeroDivide.state.At(F::Rate)=std::numeric_limits<float>::denorm_min();
    const auto divideBefore=zeroDivide.state;
    Refuses([&] {zeroDivide.Root(0.5f);},"Undefined active boundary division was approximated");
    Require(Same(divideBefore,zeroDivide.state),"Undefined division escaped caller journal undo");

    const auto prepare=[](Fixture& fixture) {
        fixture.state.notify=true;fixture.state.role=1u;
        fixture.notices[100u]={{0.25f,"Hit"},{0.5f,"Missing"}};
        fixture.onEvent=[&fixture](std::uintptr_t function) {
            if(function==Hit)fixture.Store(F::MinRate,7.0f);
            if(function==AnimEnd)fixture.state.deleted=true;
        };
    };
    Fixture complete;prepare(complete);complete.Root(1.0f);
    const auto expectedWrites=complete.writes,expectedWork=complete.work;
    Require(expectedWrites>=9u && expectedWork>expectedWrites,"Rollback fixture did not reach full callback/completion tail");
    for(std::size_t cut=0u;cut<expectedWrites;++cut) {
        Fixture fixture;prepare(fixture);fixture.writeCut=cut;
        const auto original=fixture.state;
        Refuses([&] {fixture.Root(1.0f);},"An actual write-prefix cutoff did not refuse");
        Require(Same(original,fixture.state) && fixture.events.empty(),
            "Write-prefix rollback retained callback clocks, deletion or events");
    }
    for(std::size_t cut=0u;cut<expectedWork;++cut) {
        Fixture fixture;prepare(fixture);fixture.workCut=cut;
        const auto original=fixture.state;
        Refuses([&] {fixture.Root(1.0f);},"An actual native work-prefix cutoff did not refuse");
        Require(Same(original,fixture.state) && fixture.events.empty(),
            "Work-prefix rollback retained callback clocks, deletion or events");
    }
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct RetailPrecision {
    unsigned short saved{};
    RetailPrecision() {
        asm volatile("fnstcw %0":"=m"(saved));
        const auto nearest64=static_cast<unsigned short>((saved&~0x0f00u)|0x0300u);
        asm volatile("fldcw %0"::"m"(nearest64));
    }
    ~RetailPrecision() {asm volatile("fldcw %0"::"m"(saved));}
};
struct OracleResult {State state;std::vector<Event> events;float remaining;unsigned iterations{};bool cap{};};
// Independent x87 arithmetic, written from the static retail instruction
// sequence. It calls none of the portable dyadic/sqrt/clock implementations.
// Only unchanged live fields/no-op callbacks are supported by this oracle;
// synchronous mutations and native-provider cuts have separate tests above.
OracleResult Oracle(State state,float elapsed,const std::vector<Notice>& notices,bool suppress) {
    OracleResult result{state,{},elapsed,0u,false};
    auto& s=result.state;
    auto emit=[&](std::uintptr_t function) {
        result.events.push_back({function,s.At(F::Frame),s.At(F::Rate),s.finished});
    };
    for(;;) {
        if(s.sequence.empty() || s.sequence=="None")return result;
        if(s.At(F::Frame)>=0.0f?s.At(F::Rate)==0.0f:s.At(F::TweenRate)==0.0f)return result;
        if(result.remaining<=0.0f)return result;
        if(result.iterations==4u) {result.cap=true;return result;}
        ++result.iterations;
        const float old=s.At(F::Frame);
        if(old<0.0f) {
            volatile long double advance=static_cast<long double>(result.remaining)*s.At(F::TweenRate);
            volatile long double next=advance+s.At(F::Frame);
            s.At(F::Frame)=static_cast<float>(next);
            if(next<0.0L)return result;
            volatile long double numerator=next*result.remaining;
            volatile long double denominator=next-old;
            s.At(F::Frame)=0.0f;
            volatile long double residual=numerator/denominator;
            result.remaining=static_cast<float>(residual);
            if(s.At(F::Rate)==0.0f) {s.finished=true;if(!suppress)emit(AnimEnd);}
            continue;
        }
        volatile long double advance{};
        if(s.At(F::Rate)>=0.0f)advance=static_cast<long double>(result.remaining)*s.At(F::Rate);
        else {
            volatile long double x=s.At(F::VelocityX),y=s.At(F::VelocityY),z=s.At(F::VelocityZ);
            volatile long double xx=x*x,yy=y*y,zz=z*z;
            volatile long double xy=xx+yy;
            volatile long double sum=xy+zz;
            volatile double stored=static_cast<double>(sum);
            long double speed{};
            asm volatile("fsqrt":"=t"(speed):"0"(static_cast<long double>(stored)));
            volatile long double product=speed*s.At(F::Rate);
            volatile long double scaled=-product;
            volatile long double selected=s.At(F::MinRate)<scaled?scaled:static_cast<long double>(s.At(F::MinRate));
            advance=selected*result.remaining;
        }
        volatile long double tentative=advance+s.At(F::Frame);
        s.At(F::Frame)=static_cast<float>(tentative);
        if(s.notify && s.mesh && s.sequence!="Unknown") {
            bool found{};std::size_t selected{};float best=100000.0f;
            for(std::size_t i=0u;i<notices.size();++i) {
                if(!(old<notices[i].time) || s.At(F::Frame)<notices[i].time)continue;
                volatile long double difference=static_cast<long double>(notices[i].time)-old;
                if(!found || difference<best) {best=static_cast<float>(difference);selected=i;found=true;}
            }
            if(found) {
                const auto& notice=notices[selected];
                volatile long double distance=static_cast<long double>(s.At(F::Frame))-notice.time;
                volatile long double numerator=distance*result.remaining;
                volatile long double denominator=static_cast<long double>(s.At(F::Frame))-old;
                s.At(F::Frame)=notice.time;
                volatile long double residual=numerator/denominator;
                result.remaining=static_cast<float>(residual);
                if(notice.function=="Hit")emit(Hit);else if(notice.function=="Other")emit(Other);
                continue;
            }
        }
        if(s.At(F::Frame)<s.At(F::Last))return result;
        if(s.loop) {
            if(s.At(F::Frame)<1.0f)result.remaining=0.0f;
            else {
                volatile long double distance=static_cast<long double>(s.At(F::Frame))-1.0L;
                volatile long double numerator=distance*result.remaining;
                volatile long double denominator=static_cast<long double>(s.At(F::Frame))-old;
                s.At(F::Frame)=0.0f;
                volatile long double residual=numerator/denominator;
                result.remaining=static_cast<float>(residual);
            }
            if(old<s.At(F::Last)) {
                if(s.latent==385u)s.finished=true;
                if(!suppress)emit(AnimEnd);
            }
        } else {
            volatile long double distance=static_cast<long double>(s.At(F::Frame))-s.At(F::Last);
            const float last=s.At(F::Last);
            s.At(F::Rate)=0.0f;
            volatile long double numerator=distance*result.remaining;
            volatile long double denominator=static_cast<long double>(s.At(F::Frame))-old;
            s.At(F::Frame)=last;s.finished=true;
            volatile long double residual=numerator/denominator;
            result.remaining=static_cast<float>(residual);
            if(!suppress)emit(AnimEnd);
            if(s.role<2u&&!s.weapon) {
                volatile long double simX=static_cast<long double>(s.At(F::Frame))*10000.0L;
                s.At(F::SimX)=static_cast<float>(simX);
                volatile long double simY=static_cast<long double>(s.At(F::Rate))*5000.0L;
                s.At(F::SimY)=static_cast<float>(simY);
                if(simY>32767.0L)s.At(F::SimY)=32767.0f;
            }
        }
    }
}
void CheckOracle(const State& state,float elapsed,const std::vector<Notice>& notices,bool suppress) {
    Fixture fixture;
    fixture.state=state;fixture.notices[100u]=notices;fixture.query.suppressAnimEnd=suppress;
    const auto expected=Oracle(state,elapsed,notices,suppress);
    const auto actual=fixture.Advance(elapsed);
    if(!Same(expected.state,fixture.state) || !Same(expected.events,fixture.events) ||
        Raw(expected.remaining)!=Raw(actual.remainingSeconds) || expected.iterations!=actual.iterations ||
        expected.cap!=actual.iterationCap) {
        std::cerr<<std::hex<<"x87 case frame="<<Raw(state.At(F::Frame))<<" rate="<<Raw(state.At(F::Rate))
            <<" tween="<<Raw(state.At(F::TweenRate))<<" dt="<<Raw(elapsed)
            <<" actualFrame="<<Raw(fixture.state.At(F::Frame))<<" expectedFrame="<<Raw(expected.state.At(F::Frame))
            <<" actualRem="<<Raw(actual.remainingSeconds)<<" expectedRem="<<Raw(expected.remaining)<<std::dec<<'\n';
        throw std::runtime_error("Independent PC64 main-animation oracle mismatch");
    }
    ++checks;++oracleCases;
}
void IndependentOracle() {
    static_assert(std::numeric_limits<long double>::digits==64,"Independent oracle needs x87 significand64");
    RetailPrecision precision;
    const std::vector<Notice> notices{{0.75f,"Hit"},{0.1f,"Missing"},{0.5f,"Other"},{0.5f,"Hit"}};
    for(const float frame:{-0.25f,-0.0f,0.0f,0.1f,0.75f,0.875f,1.125f})
        for(const float rate:{-3.0f,0.0f,0.1f,1.0f,2.0f})
            for(const float elapsed:{-0.125f,0.0f,0.03125f,0.125f,0.5f,2.0f})
                for(unsigned flags=0u;flags<8u;++flags) {
                    State state;
                    state.At(F::Frame)=frame;state.At(F::Rate)=rate;state.At(F::TweenRate)=0.75f;
                    state.At(F::VelocityX)=0.3f;state.At(F::VelocityY)=-0.4f;state.At(F::VelocityZ)=0.5f;
                    state.At(F::MinRate)=0.25f;state.notify=(flags&1u)!=0u;state.loop=(flags&2u)!=0u;
                    state.latent=(flags&4u)?385u:0u;state.role=static_cast<std::uint8_t>(flags%4u);
                    CheckOracle(state,elapsed,notices,(flags&4u)!=0u);
                }
    std::uint32_t random=0x5eed04a1u;
    const auto next=[&] {random^=random<<13u;random^=random>>17u;random^=random<<5u;return random;};
    const auto signedFloat=[&] {return Float(0x3f000000u|(next()&0x807fffffu));};
    for(unsigned i=0u;i<6000u;++i) {
        State state;
        state.At(F::Frame)=signedFloat();state.At(F::Rate)=signedFloat();
        state.At(F::TweenRate)=std::abs(signedFloat());state.At(F::MinRate)=signedFloat();
        state.At(F::VelocityX)=signedFloat();state.At(F::VelocityY)=signedFloat();state.At(F::VelocityZ)=signedFloat();
        state.At(F::Last)=std::abs(signedFloat());
        state.notify=(i&1u)!=0u;state.loop=(i&2u)!=0u;state.finished=(i&4u)!=0u;
        state.role=static_cast<std::uint8_t>(i%4u);state.weapon=(i&8u)!=0u;state.latent=(i&16u)?385u:0u;
        CheckOracle(state,std::abs(signedFloat()),notices,(i&32u)!=0u);
    }
}
#else
void IndependentOracle() {
    std::cout<<"Independent x87 oracle unavailable on this architecture; deterministic/provider tests only\n";
}
#endif
} // namespace

int main() {
    try {
        LazyGates();PositiveAndNotify();TweenAndLoop();TerminalAndCallbacks();RefusalAndJournal();IndependentOracle();
        std::cout<<"Original Actor main-animation tests passed: "<<checks<<" checks / "<<refusals
            <<" refusals / "<<oracleCases<<" independent x87 cases\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
