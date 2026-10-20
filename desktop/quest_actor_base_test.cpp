#include "quest_actor_base.h"
#include "quest_native_work.h"

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Detached original-protocol fixtures only. They neither read owned packages
// nor accept the actual VM adapter, FindBase, crate support or falling physics.
namespace {
using Id=std::uintptr_t;
using Event=QuestVr::OriginalActorBaseEvent;
using Queries=QuestVr::OriginalActorBaseQueries;
using Limits=QuestVr::OriginalActorBaseLimits;
constexpr Id self=1u,old=2u,next=3u,info=4u,alternate=5u,auxiliary=6u;
constexpr auto never=std::numeric_limits<std::size_t>::max();
std::size_t checks{},cases{},refusals{},transcripts{},callbackCases{},failurePrefixes{},rootCases{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
struct Call {
    char operation{};Id receiver{},value{};
    bool operator==(const Call&)const=default;
};
struct Actor {
    Id base{},level{info};std::uint8_t count{};
    // Deliberately outside this protocol: no class, deleted/hidden, reverse
    // list, active state or collision predicate may replace direct fields.
    std::vector<Id> inventedReverseList;
    bool hidden{},deleted{},active{};
    bool operator==(const Actor&)const=default;
};
struct World {
    std::array<Actor,10u> actors;
    std::vector<Call> trace,executed;
    std::function<void(World&,const Call&)> after;
    std::function<void()> sharedWork;
    std::size_t work{},limit{never},failAt{never};bool failAfter{};
    unsigned enabledEvents{7u};bool latentState{};
    Actor& At(Id actor){
        if(!actor||actor>=actors.size())throw std::runtime_error("Synthetic direct field received an unsafe pointer");
        return actors[static_cast<std::size_t>(actor)];
    }
    void Work(){
        if(sharedWork)sharedWork();
        if(work>=limit)throw std::runtime_error("Synthetic enclosing-root work exhausted");
        ++work;
    }
    std::size_t Begin(Call call){
        const auto index=trace.size();trace.push_back(call);
        if(!failAfter&&index==failAt)throw std::runtime_error("Injected provider-entry refusal");
        return index;
    }
    void End(std::size_t index,Call call){
        if(after)after(*this,call);
        if(failAfter&&index==failAt)throw std::runtime_error("Injected provider-exit refusal");
    }
    Id Base(Id actor){const Call call{'b',actor};const auto at=Begin(call);const auto value=At(actor).base;End(at,call);return value;}
    Id Level(Id actor){const Call call{'l',actor};const auto at=Begin(call);const auto value=At(actor).level;End(at,call);return value;}
    void Base(Id actor,Id value){const Call call{'B',actor,value};const auto at=Begin(call);At(actor).base=value;End(at,call);}
    std::uint8_t Count(Id actor){const Call call{'c',actor};const auto at=Begin(call);const auto value=At(actor).count;End(at,call);return value;}
    void Count(Id actor,std::uint8_t value){const Call call{'C',actor,value};const auto at=Begin(call);At(actor).count=value;End(at,call);}
    void Notify(Id actor,Event event,Id argument){
        const char operation=event==Event::Detach?'d':event==Event::Attach?'a':'e';
        const Call call{operation,actor,argument};const auto at=Begin(call);At(actor);
        const auto bit=event==Event::Detach?1u:event==Event::Attach?2u:4u;
        // Synthetic host dispatch policy, NOT a production enabled/latent
        // implementation. Protocol must request the same selected events;
        // only its provider may select/suppress the original function body.
        if(enabledEvents&bit)executed.push_back(call);
        End(at,call);
    }
    Queries Providers(){
        Queries q;
        q.base=[this](Id actor){return Base(actor);};q.level=[this](Id actor){return Level(actor);};
        q.writeBase=[this](Id actor,Id value){Base(actor,value);};
        q.standingCount=[this](Id actor){return Count(actor);};
        q.writeStandingCount=[this](Id actor,std::uint8_t value){Count(actor,value);};
        q.event=[this](Id actor,Event event,Id argument){Notify(actor,event,argument);};
        q.work=[this]{Work();};return q;
    }
};

// Independent register/branch interpreter of10369CAD..10369D7E. It does not
// call production helpers or derive outcomes from protocol statistics.
struct RetailMachine {
    World& w;std::size_t chain{},steps{};Limits limits;
    void Charge(){if(steps>=limits.steps)throw std::runtime_error("Oracle step bound");w.Work();++steps;}
    Id B(Id actor){Charge();return w.Base(actor);}
    Id L(){Charge();return w.Level(self);}
    unsigned C(Id actor){Charge();return w.Count(actor);}
    void C(Id actor,unsigned value){Charge();w.Count(actor,static_cast<std::uint8_t>(value&255u));}
    void E(Id actor,Event event,Id argument){
        if(!actor)throw std::invalid_argument("Oracle unsafe null event receiver");
        Charge();w.Notify(actor,event,argument);
    }
    void Run(Id ebx,std::int32_t sendEvent){
        Id eax=ebx,edi{};
    scan:
        if(!eax)goto equality;
        if(chain>=limits.chainVisits)throw std::runtime_error("Oracle prospective chain bound");
        Charge();++chain;
        if(eax==self)goto done;
        eax=B(eax);goto scan;
    equality:
        eax=B(self);
        if(ebx==eax)goto done;
        if(!eax)goto store;
        if(eax==L())goto store;
        C(eax,C(eax)+255u);
        edi=B(self);E(edi,Event::Detach,self);
    store:
        Charge();w.Base(self,ebx);
        if(!ebx)goto finalEvent;
        if(ebx==L())goto finalEvent;
        C(ebx,C(ebx)+1u);
        edi=B(self);E(edi,Event::Attach,self);
    finalEvent:
        if(sendEvent)E(self,Event::BaseChange,0u);
    done:
        return;
    }
};
World Initial(){World w;w.At(self).base=old;w.At(old).count=1u;w.At(next).count=0u;return w;}
void Equal(const World& actual,const World& expected){
    Require(actual.actors==expected.actors,"Protocol state differs from independent original register machine");
    Require(actual.trace==expected.trace,"Protocol native field/event transcript differs from independent original branch order");
    Require(actual.executed==expected.executed,"Protocol selected event effects differ from independent dispatch fixture");
    Require(actual.work==expected.work,"Protocol reset, omitted or invented a shared root work charge");
}
void Pair(World seed,Id replacement,std::int32_t sendEvent,const Limits& limits={}){
    auto actual=seed,expected=seed;bool actualThrew{},expectedThrew{};
    try{const auto stats=QuestVr::SetOriginalActorBase(self,replacement,sendEvent,actual.Providers(),limits);
        Require(stats.steps==actual.work,"Returned protocol statistics lost enclosing-root work count");}
    catch(const std::exception&){actualThrew=true;}
    try{RetailMachine{expected,0u,0u,limits}.Run(replacement,sendEvent);}catch(const std::exception&){expectedThrew=true;}
    Require(actualThrew==expectedThrew,"Protocol/oracle disagree about an active refusal");Equal(actual,expected);
    if(actualThrew)++refusals;
    ++cases;
}
void Golden(World seed,Id replacement,std::int32_t sendEvent,std::vector<Call> expected){
    auto w=seed;QuestVr::SetOriginalActorBase(self,replacement,sendEvent,w.Providers());
    Require(w.trace==expected,"Source-backed golden provider transcript changed");++transcripts;
    Pair(std::move(seed),replacement,sendEvent);
}
void BasicTranscripts(){
    Golden(Initial(),next,1,{{'b',next},{'b',self},{'l',self},{'c',old},{'C',old,0u},{'b',self},{'d',old,self},
        {'B',self,next},{'l',self},{'c',next},{'C',next,1u},{'b',self},{'a',next,self},{'e',self}});
    auto w=Initial();w.At(self).base=0u;
    Golden(w,0u,1,{{'b',self}});
    Golden(Initial(),self,1,{}); // Prospective equality before ANY Base field.
    Golden(Initial(),old,1,{{'b',old},{'b',self}});
    Golden(Initial(),0u,0,{{'b',self},{'l',self},{'c',old},{'C',old,0u},{'b',self},{'d',old,self},{'B',self}});
    w=Initial();w.At(self).base=info;
    Golden(w,0u,-1,{{'b',self},{'l',self},{'B',self},{'e',self}});
    w=Initial();w.At(self).base=0u;
    Golden(w,info,0,{{'b',info},{'b',self},{'B',self,info},{'l',self}});
    // Equality is pointer equality to live Level, never a class predicate:
    // even an otherwise ordinary object stored in Level gets the exemption.
    w=Initial();w.At(self).level=old;
    Golden(w,next,0,{{'b',next},{'b',self},{'l',self},{'B',self,next},{'l',self},
        {'c',next},{'C',next,1u},{'b',self},{'a',next,self}});
}
void GeneratedFields(){
    constexpr std::array<Id,4u> bases{0u,old,next,info};
    constexpr std::array<Id,5u> replacements{0u,self,old,next,info};
    constexpr std::array<unsigned,8u> counts{0u,1u,2u,127u,128u,253u,254u,255u};
    for(const auto base:bases)for(const auto replacement:replacements)for(const auto level:bases)
        for(const auto count:counts)for(const auto send:{0,1,-1,std::numeric_limits<std::int32_t>::min()}){
            auto w=Initial();w.At(self).base=base;w.At(self).level=level;
            w.At(old).count=static_cast<std::uint8_t>(count);w.At(next).count=static_cast<std::uint8_t>(255u-count);
            w.At(info).count=static_cast<std::uint8_t>(count);
            w.At(old).hidden=true;w.At(next).deleted=true;w.At(info).active=false;
            w.At(old).inventedReverseList={auxiliary,self,self};
            w.At(next).inventedReverseList={old,info};
            Pair(std::move(w),replacement,send);
        }
    for(unsigned value=0u;value<256u;++value){
        auto w=Initial();w.At(old).count=static_cast<std::uint8_t>(value);w.At(next).count=static_cast<std::uint8_t>(value);
        auto actual=w;QuestVr::SetOriginalActorBase(self,next,0,actual.Providers());
        Require(actual.At(old).count==static_cast<std::uint8_t>((value+255u)&255u)&&
            actual.At(next).count==static_cast<std::uint8_t>((value+1u)&255u),"StandingCount did not wrap as independent BYTE arithmetic");
        Pair(std::move(w),next,0);
    }
}
void MutableEvents(){
    for(const auto newLevel:{Id{0u},old,next,info,alternate})for(const auto changedBase:{Id{0u},self,old,next,alternate})
        for(const auto count:{0u,1u,254u,255u}){
            auto w=Initial();w.after=[=](World& live,const Call& call){
                if(call.operation=='d'){
                    live.At(self).base=changedBase;live.At(self).level=newLevel;
                    live.At(next).count=static_cast<std::uint8_t>(count);
                    live.At(old).count=203u;
                }
                if(call.operation=='a'){
                    live.At(self).base=auxiliary;live.At(self).level=alternate;
                    live.At(next).count=91u;
                }
                if(call.operation=='e')live.At(auxiliary).count=59u;
            };
            Pair(std::move(w),next,-1);++callbackCases;
        }
    // Counter writes themselves are not script callbacks in retail. Synthetic
    // provider side effects nevertheless expose its distinct later receiver
    // rereads without conflating receiver with captured counter target.
    for(const bool oldStore:{false,true}){
        auto w=Initial();w.after=[=](World& live,const Call& call){
            if(call.operation=='C'&&call.receiver==(oldStore?old:next))live.At(self).base=alternate;
        };
        auto actual=w;QuestVr::SetOriginalActorBase(self,next,1,actual.Providers());
        bool selected{};for(const auto& call:actual.trace)
            if(call.operation==(oldStore?'d':'a')&&call.receiver==alternate)selected=true;
        Require(selected,"Event receiver reused a captured Base instead of the original post-counter reload");
        Pair(std::move(w),next,1);++callbackCases;
    }
    // Detach can make the captured next pointer into Level; then increment and
    // Attach disappear, but the captured Base store and BaseChange still run.
    auto w=Initial();w.after=[](World& live,const Call& call){if(call.operation=='d')live.At(self).level=next;};
    auto actual=w;QuestVr::SetOriginalActorBase(self,next,1,actual.Providers());
    Require(actual.At(self).base==next&&actual.At(next).count==0u&&actual.executed.size()==2u&&
        actual.executed.back().operation=='e',"Post-Detach Level read or captured replacement store was bypassed");
    Pair(std::move(w),next,1);
    w=Initial();w.after=[](World& live,const Call& call){if(call.operation=='d')live.At(next).base=self;};
    actual=w;QuestVr::SetOriginalActorBase(self,next,0,actual.Providers());
    Require(actual.At(self).base==next&&actual.At(next).base==self&&actual.At(next).count==1u,
        "Protocol invented a second prospective cycle scan after Detach");Pair(std::move(w),next,0);
}
void CyclesAndLazyServices(){
    auto w=Initial();w.At(next).base=alternate;w.At(alternate).base=self;
    auto actual=w;const auto blocked=QuestVr::SetOriginalActorBase(self,next,1,actual.Providers());
    Require(blocked.cycleBlocked&&!blocked.unchanged&&actual.trace==std::vector<Call>{{'b',next},{'b',alternate}}&&
        actual.actors==w.actors,"Prospective source cycle was checked after equality or mutated native fields");Pair(w,next,1);
    w.At(self).base=next;Pair(w,next,1); // Cycle still blocks before same-Base fast return.
    Limits limits;limits.chainVisits=5u;
    w=Initial();w.At(self).base=next;w.At(next).base=alternate;w.At(alternate).base=next;
    Pair(w,next,1,limits); // Non-source cycle must refuse BEFORE equality.
    Queries q;const auto selfCycle=QuestVr::SetOriginalActorBase(self,self,1,q);
    Require(selfCycle.cycleBlocked&&selfCycle.baseReads==0u,"Self cycle required an inactive provider");
    q.base=[](Id actor){if(actor!=self)throw std::runtime_error("Inactive prospective Base was read");return Id{};};
    const auto unchanged=QuestVr::SetOriginalActorBase(self,0u,1,q);
    Require(unchanged.unchanged&&unchanged.levelReads==0u&&unchanged.events==0u,"Unchanged None Base required inactive providers/events");
    q={};q.base=[](Id actor){return actor==next?Id{}:info;};q.level=[](Id){return info;};
    q.writeBase=[](Id,Id){};
    // Old Level exemption and null replacement need neither counters nor events.
    QuestVr::SetOriginalActorBase(self,0u,0,q);
    for(const auto invalid:{Id{},self}){
        bool rejected{};try{QuestVr::SetOriginalActorBase(invalid,0u,0,Queries{});}catch(const std::exception&){rejected=true;}
        Require(rejected,"Null source or missing active Base provider silently succeeded");++refusals;
    }
    for(unsigned missing=0u;missing<6u;++missing){
        auto live=Initial();auto unavailable=live.Providers();
        switch(missing){
        case 0u:unavailable.base={};break;
        case 1u:unavailable.level={};break;
        case 2u:unavailable.writeBase={};break;
        case 3u:unavailable.standingCount={};break;
        case 4u:unavailable.writeStandingCount={};break;
        default:unavailable.event={};break;
        }
        bool rejected{};try{QuestVr::SetOriginalActorBase(self,next,1,unavailable);}catch(const std::invalid_argument&){rejected=true;}
        Require(rejected,"Missing active native provider silently became a SetBase no-op");++refusals;
    }
    for(unsigned field=0u;field<4u;++field){
        Limits bad;if(field==0u)bad.chainVisits=0u;if(field==1u)bad.chainVisits=65'537u;
        if(field==2u)bad.steps=0u;
        if(field==3u)bad.steps=4'194'305u;
        bool rejected{};try{QuestVr::SetOriginalActorBase(self,self,0,Queries{},bad);}catch(const std::invalid_argument&){rejected=true;}
        Require(rejected,"Invalid protocol bounds reached even the zero-provider self cycle");++refusals;
    }
}
void FailureAndRootControls(){
    auto seed=Initial();seed.after=[](World& live,const Call& call){
        if(call.operation=='d'){live.At(self).base=alternate;live.At(next).count=255u;}
        if(call.operation=='a')live.At(auxiliary).count=77u;
    };
    auto complete=seed;QuestVr::SetOriginalActorBase(self,next,1,complete.Providers());
    for(const bool after:{false,true})for(std::size_t index=0u;index<complete.trace.size();++index){
        auto w=seed;w.failAt=index;w.failAfter=after;Pair(w,next,1);++failurePrefixes;
        // Explicit synthetic OUTER journal: not an implicit per-event undo in
        // SetBase. Preserve spent work/trace evidence while restoring effects.
        const auto initialActors=w.actors;const auto initialExecuted=w.executed;
        bool refused{};try{QuestVr::SetOriginalActorBase(self,next,1,w.Providers());}
        catch(const std::exception&){refused=true;w.actors=initialActors;w.executed=initialExecuted;}
        Require(refused&&w.actors==initialActors&&w.executed==initialExecuted&&!w.trace.empty(),
            "Synthetic outer journal did not restore a partially completed SetBase failure");++rootCases;
    }
    // Native effects really are partial until an OUTER journal undoes them.
    auto partial=Initial();partial.failAt=7u;
    bool refused{};try{QuestVr::SetOriginalActorBase(self,next,1,partial.Providers());}catch(const std::exception&){refused=true;}
    Require(refused&&partial.At(old).count==0u&&partial.At(self).base==old,
        "Detached protocol invented local rollback or published its captured store before the Detach path finished");++refusals;
    for(std::size_t cap=1u;cap<complete.work;++cap){
        auto w=seed;Limits limits;limits.steps=cap;Pair(w,next,1,limits);
        w=seed;w.limit=cap;Pair(w,next,1);
    }
    // Borrowed sticky root authority spans separate calls and reentrant calls.
    namespace Work=QuestVr::NativeWork;
    Work::Budget budget(complete.work);
    auto first=seed;first.sharedWork=[&]{budget.Step();};
    QuestVr::SetOriginalActorBase(self,next,1,first.Providers());
    Require(budget.Read().used==complete.work&&!budget.Read().exhausted,"Protocol reset or lost borrowed native-root charges");
    auto second=Initial();second.sharedWork=[&]{budget.Step();};
    bool exhausted{};try{QuestVr::SetOriginalActorBase(self,next,0,second.Providers());}
    catch(const Work::Exhausted&){exhausted=true;}
    Require(exhausted&&budget.Read().exhausted&&second.trace.empty(),"Second SetBase received fresh work or entered a provider after sticky exhaustion");++refusals;
    exhausted=false;try{QuestVr::SetOriginalActorBase(self,self,0,second.Providers());}
    catch(const Work::Exhausted&){exhausted=true;}
    Require(exhausted&&second.trace.empty(),"A no-op cycle reset or bypassed exhausted shared work");++refusals;
    budget.Reset(100u);auto nested=Initial();nested.sharedWork=[&]{budget.Step();};bool entered{};
    nested.after=[&](World& live,const Call& call){
        if(call.operation=='d'&&!entered){entered=true;
            QuestVr::SetOriginalActorBase(auxiliary,alternate,0,live.Providers());}
    };
    const auto outer=QuestVr::SetOriginalActorBase(self,next,1,nested.Providers());
    Require(entered&&nested.At(auxiliary).base==alternate&&budget.Read().used==nested.work&&
        nested.work>outer.steps,"Reentrant SetBase received a private ledger or lost its outer captured replacement");++rootCases;
}
void SyntheticEventPolicy(){
    for(unsigned enabled=0u;enabled<8u;++enabled)for(const bool latent:{false,true}){
        auto w=Initial();w.enabledEvents=enabled;w.latentState=latent;
        auto actual=w;const auto stats=QuestVr::SetOriginalActorBase(self,next,1,actual.Providers());
        Require(stats.events==3u&&actual.At(self).base==next&&actual.At(old).count==0u&&actual.At(next).count==1u,
            "Protocol gated native fields or selected-event requests on synthetic dispatch policy");
        const auto expected=static_cast<std::size_t>((enabled&1u)!=0u)+static_cast<std::size_t>((enabled&2u)!=0u)+
            static_cast<std::size_t>((enabled&4u)!=0u);
        Require(actual.executed.size()==expected,"Synthetic host policy changed the wrong selected-event effects");Pair(w,next,1);
    }
}
}
int main(){
    try{
        BasicTranscripts();GeneratedFields();MutableEvents();CyclesAndLazyServices();FailureAndRootControls();SyntheticEventPolicy();
        std::cout<<"PASS detached original SetBase "<<checks<<" checks / "<<cases<<" oracle cases / "<<refusals
            <<" refusals / "<<transcripts<<" golden transcripts / "<<callbackCases<<" callback cases / "
            <<failurePrefixes<<" failure prefixes / "<<rootCases<<" synthetic outer-root controls; not runtime/map physics.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL detached SetBase: "<<error.what()<<'\n';return 1;}
}
