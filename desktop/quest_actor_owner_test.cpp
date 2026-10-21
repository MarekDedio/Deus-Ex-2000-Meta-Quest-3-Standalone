#include "quest_actor_owner.h"

#include <array>
#include <cstddef>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Detached original SetOwner protocol ONLY. Mock events/stores are not an
// actual native279/DestroyActor implementation or first-Training acceptance.
namespace {
using Queries=QuestVr::OriginalActorOwnerQueries;
using Event=QuestVr::OriginalActorOwnerEvent;
using Id=std::uintptr_t;
constexpr Id actor=11u;
constexpr auto unlimited=std::numeric_limits<std::size_t>::max();
std::size_t checks{},refusals{},cases{};
void Require(bool value,const char* why) {++checks;if(!value)throw std::runtime_error(why);}
struct Refusal {};
struct Operation {
    char kind{};Id receiver{},other{};
    bool operator==(const Operation&)const=default;
};
struct Fixture {
    std::map<Id,Id> owners{{actor,0u},{21u,0u},{31u,0u},{41u,0u}};
    std::vector<Operation> trace;
    std::size_t work{},maximum=unlimited,fail=unlimited;
    std::function<void(Fixture&,Operation)> after;
    void Reached(Operation op) {
        trace.push_back(op);
        if(trace.size()-1u==fail)throw Refusal{};
    }
    void After(Operation op) {if(after)after(*this,op);}
    Queries Providers() {
        Queries q;
        q.work=[this]{if(work==maximum)throw Refusal{};++work;};
        q.owner=[this](Id source){
            Require(source==actor,"SetOwner read another Actor's Owner");
            const Operation op{'R',source,0u};Reached(op);
            const auto result=owners.at(source);After(op);return result;
        };
        q.writeOwner=[this](Id source,Id target){
            Require(source==actor,"SetOwner wrote another Actor's Owner");
            const Operation op{'W',source,target};Reached(op);owners.at(source)=target;After(op);
        };
        q.event=[this](Id receiver,Event event,Id other){
            Require(receiver&&other==actor,"SetOwner event changed captured receiver or Other parm");
            const Operation op{event==Event::LostChild?'L':'G',receiver,other};Reached(op);After(op);
        };
        return q;
    }
};
std::vector<Operation> Expected(Id oldOwner,Id newOwner) {
    std::vector<Operation> result{{'R',actor,0u}};
    if(oldOwner)result.push_back({'L',oldOwner,actor});
    result.push_back({'W',actor,newOwner});
    if(newOwner)result.push_back({'G',newOwner,actor});
    return result;
}
void BasicBranchesAndLiveCallbacks() {
    constexpr std::array<Id,5u> ids{0u,actor,21u,31u,41u};
    // Independent plain state-machine oracle: callback changes the live Owner
    // after read/lost/write/gained, not an oracle calling the tested helper.
    for(const auto oldOwner:ids)for(const auto newOwner:ids)
        for(unsigned mutations=0u;mutations<16u;++mutations) {
            Fixture f;f.owners.at(actor)=oldOwner;auto q=f.Providers();
            f.after=[mutations](Fixture& live,Operation op){
                const auto bit=op.kind=='R'?1u:op.kind=='L'?2u:op.kind=='W'?4u:8u;
                if(mutations&bit)live.owners.at(actor)=op.kind=='G'?0u:41u;
            };
            Id expected=newOwner; // The native write supersedes read/lost changes.
            if(mutations&4u)expected=41u;
            if(newOwner&&(mutations&8u))expected=0u;
            QuestVr::SetOriginalActorOwner(actor,newOwner,q);
            Require(f.trace==Expected(oldOwner,newOwner)&&f.owners.at(actor)==expected&&f.work==f.trace.size(),
                "SetOwner changed capture/store/event order, normalized same/self Owner, or reread callback-mutated Owner");
            for(const auto id:{21u,31u,41u})Require(f.owners.at(id)==0u,"SetOwner mutated a reverse owner relationship");
            ++cases;
        }
    // A caller's variable is not the retained native by-value newOwner arg.
    Id alias=31u;Fixture f;f.owners.at(actor)=21u;auto q=f.Providers();
    f.after=[&](Fixture&,Operation op){if(op.kind=='L')alias=0u;};
    QuestVr::SetOriginalActorOwner(actor,alias,q);
    Require(alias==0u&&f.owners.at(actor)==31u&&f.trace==Expected(21u,31u),
        "LostChild redirected the captured native newOwner argument");++cases;
}
void OptionalProvidersAndEveryFailure() {
    for(const auto oldOwner:{Id{},Id{21u}})for(const auto newOwner:{Id{},Id{31u}}) {
        const auto expected=Expected(oldOwner,newOwner);
        Fixture noEvents;noEvents.owners.at(actor)=oldOwner;auto optional=noEvents.Providers();
        if(!oldOwner&&!newOwner)optional.event={};
        QuestVr::SetOriginalActorOwner(actor,newOwner,optional);
        Require(noEvents.trace==expected,"Inactive event provider was required eagerly");
        for(std::size_t stop=0u;stop<expected.size();++stop) {
            Fixture f;f.owners.at(actor)=oldOwner;f.fail=stop;auto q=f.Providers();bool failed{};
            try{QuestVr::SetOriginalActorOwner(actor,newOwner,q);}catch(const Refusal&){failed=true;}
            Require(failed&&f.trace==std::vector<Operation>(expected.begin(),expected.begin()+stop+1u)&&f.work==stop+1u,
                "SetOwner swallowed/retried a provider failure or executed a later store/event");++refusals;
            // No private helper transaction: only operations which reached the
            // mock provider before the refusal can change caller-owned state.
            Id expectedOwner=oldOwner;
            for(std::size_t i=0u;i<stop;++i)if(expected[i].kind=='W')expectedOwner=newOwner;
            Require(f.owners.at(actor)==expectedOwner,"SetOwner invented rollback or premature stores");
        }
        for(std::size_t allowance=0u;allowance<=expected.size();++allowance) {
            Fixture f;f.owners.at(actor)=oldOwner;f.maximum=allowance;auto q=f.Providers();bool failed{};
            try{QuestVr::SetOriginalActorOwner(actor,newOwner,q);}catch(const Refusal&){failed=true;}
            Require(failed==(allowance<expected.size())&&f.work==allowance&&
                f.trace==std::vector<Operation>(expected.begin(),expected.begin()+allowance),
                "SetOwner created a fresh work budget or called a provider after denied work");
            if(failed)++refusals;
            else {
                failed=false;try{QuestVr::SetOriginalActorOwner(actor,newOwner,q);}catch(const Refusal&){failed=true;}
                Require(failed&&f.work==allowance&&f.trace==expected,"A repeated SetOwner refreshed its shared allowance");++refusals;
            }
        }
    }
    for(unsigned missing=0u;missing<3u;++missing) {
        Fixture f;f.owners.at(actor)=21u;auto q=f.Providers();std::size_t prefix{};
        if(!missing){q.owner={};prefix=0u;}
        else if(missing==1u){q.writeOwner={};prefix=2u;}
        else{q.event={};prefix=1u;}
        bool failed{};try{QuestVr::SetOriginalActorOwner(actor,31u,q);}catch(const std::invalid_argument&){failed=true;}
        const auto expected=Expected(21u,31u);
        Require(failed&&f.work==prefix&&f.trace==std::vector<Operation>(expected.begin(),expected.begin()+prefix),
            "Missing SetOwner dependency charged work or ran an inactive provider");
        ++refusals;
    }
    Fixture f;auto q=f.Providers();bool failed{};
    try{QuestVr::SetOriginalActorOwner(0u,0u,q);}catch(const std::invalid_argument&){failed=true;}
    Require(failed&&f.work==0u&&f.trace.empty(),"Null Actor reached a SetOwner provider");++refusals;
}
void ReentrantEventsAndCallerRollback() {
    for(const char trigger:{'L','G'}) {
        Fixture f;f.owners.at(actor)=21u;auto q=f.Providers();bool nested{};
        f.after=[&](Fixture&,Operation op){
            if(op.kind==trigger&&!nested){nested=true;QuestVr::SetOriginalActorOwner(actor,41u,q);}
        };
        QuestVr::SetOriginalActorOwner(actor,31u,q);
        const auto expected=trigger=='L'?std::vector<Operation>{{'R',actor,0u},{'L',21u,actor},
            {'R',actor,0u},{'L',21u,actor},{'W',actor,41u},{'G',41u,actor},{'W',actor,31u},{'G',31u,actor}}:
            std::vector<Operation>{{'R',actor,0u},{'L',21u,actor},{'W',actor,31u},{'G',31u,actor},
            {'R',actor,0u},{'L',31u,actor},{'W',actor,41u},{'G',41u,actor}};
        Require(nested&&f.trace==expected&&f.owners.at(actor)==(trigger=='L'?31u:41u)&&f.work==8u,
            "Reentrant SetOwner used postcallback Owner rereads, a recursion shortcut or private allowance");++cases;
    }
    // Caller-owned rollback remains the enclosing runtime's job. The helper
    // preserves ordinary callback effects and does not restore successful halves.
    Fixture f;f.owners.at(actor)=21u;const auto before=f.owners;auto q=f.Providers();
    f.after=[](Fixture& live,Operation op){if(op.kind=='G'){live.owners.at(31u)=actor;throw Refusal{};}};
    bool failed{};try{QuestVr::SetOriginalActorOwner(actor,31u,q);}catch(const Refusal&){failed=true;}
    Require(failed&&f.owners.at(actor)==31u&&f.owners.at(31u)==actor,"Late SetOwner refusal silently undid caller-owned effects");
    f.owners=before;Require(f.owners==before,"Mock caller rollback did not restore complete owned state");++refusals;
}
}
int main() {
    try {
        BasicBranchesAndLiveCallbacks();OptionalProvidersAndEveryFailure();ReentrantEventsAndCallerRollback();
        std::cout<<"Original SetOwner detached protocol: "<<checks<<" checks, "<<cases<<" cases, "<<refusals<<" refusal controls passed.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
    catch(...){std::cerr<<"FAIL uncaught SetOwner provider refusal\n";return 1;}
}
