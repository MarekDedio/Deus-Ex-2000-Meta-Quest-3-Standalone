#include "quest_actor_pawn_context.h"

#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Queries=QuestVr::OriginalActorPawnContextQueries;
constexpr Id actor=0x100u,pawnClass=0x200u,derivedClass=0x300u,actorClass=0x400u;
std::size_t checks{},refusals{};
void Require(bool value,const char* text) {++checks;if(!value)throw std::runtime_error(text);}
template<class Function> void Refuses(Function&& function,const char* text) {
    bool refused{};try{function();}catch(const std::exception&){refused=true;}
    Require(refused,text);++refusals;
}
struct Fixture {
    bool flag=true;
    Id cls=derivedClass,target=pawnClass;
    std::uint8_t role=2u;
    std::map<Id,Id> supers{{derivedClass,pawnClass},{pawnClass,actorClass},{actorClass,0u}};
    std::vector<std::string> events;
    std::size_t work{},limit=std::numeric_limits<std::size_t>::max(),effects{};
    bool mutate{};
    Queries Query() {
        Queries q;
        q.work=[&]{if(work++>=limit)throw std::runtime_error("caller work exhausted");};
        q.pawnFlag=[&](Id a){Require(a==actor,"Flag source changed");events.push_back("flag");return flag;};
        q.actorClass=[&](Id a){Require(a==actor,"Class source changed");events.push_back("class");
            if(mutate){++effects;role=2u;}return cls;};
        q.nativePawnClass=[&]{events.push_back("target");return target;};
        q.superClass=[&](Id c){events.push_back("super"+std::to_string(c));return supers.at(c);};
        q.role=[&](Id a){Require(a==actor,"Role source changed");events.push_back("role");return role;};
        return q;
    }
};
void ExactAndLazy() {
    Fixture f;const auto result=QuestVr::CaptureOriginalActorPawnContext(actor,f.Query());
    Require(result.pawn==actor&&result.suppressAnimEnd,"Actual flag-set Role2 Pawn was not captured");
    Require(f.events==std::vector<std::string>{"flag","class","target","super768","role"},
        "Flag/Class/native identity/Super/Role provider order changed");
    Require(result.steps==5u&&result.flagReads==1u&&result.classReads==1u&&result.nativeClassReads==1u&&
        result.superReads==1u&&result.roleReads==1u&&f.work==result.steps,"Context counters/shared work changed");
    // Already captured context is immutable evidence, not a lazy actor view.
    f.flag=false;f.cls=actorClass;f.role=4u;f.target=0u;f.supers.clear();
    Require(result.pawn==actor&&result.suppressAnimEnd,"Later callback mutations refreshed captured Pawn context");
    Fixture exact;exact.cls=pawnClass;auto q=exact.Query();q.superClass={};
    const auto direct=QuestVr::CaptureOriginalActorPawnContext(actor,q);
    Require(direct.pawn==actor&&direct.suppressAnimEnd&&!direct.superReads,"Direct Pawn demanded a superclass provider");
    Fixture clear;clear.flag=false;q=clear.Query();q.actorClass={};q.nativePawnClass={};q.superClass={};q.role={};
    const auto absent=QuestVr::CaptureOriginalActorPawnContext(actor,q);
    Require(!absent.pawn&&!absent.suppressAnimEnd&&clear.events==std::vector<std::string>{"flag"}&&absent.steps==1u,
        "Actual Pawn with bIsPawn clear was cast or demanded Class/Role authority");
    Fixture nullClass;nullClass.cls=0u;q=nullClass.Query();q.nativePawnClass={};q.superClass={};q.role={};
    const auto null=QuestVr::CaptureOriginalActorPawnContext(actor,q);
    Require(!null.pawn&&!null.suppressAnimEnd&&nullClass.events==std::vector<std::string>{"flag","class"},
        "Flag-set null Class demanded native identity/Role or fabricated a Pawn");
    Fixture wrong;wrong.cls=actorClass;q=wrong.Query();q.role={};
    const auto mismatch=QuestVr::CaptureOriginalActorPawnContext(actor,q);
    Require(!mismatch.pawn&&!mismatch.suppressAnimEnd&&wrong.events==
        std::vector<std::string>{"flag","class","target","super1024"},
        "bIsPawn set on actual non-Pawn was treated as a cast or demanded Role");
    for(unsigned role=0u;role<256u;++role) {
        Fixture roles;roles.role=static_cast<std::uint8_t>(role);
        const auto captured=QuestVr::CaptureOriginalActorPawnContext(actor,roles.Query());
        Require(captured.pawn==actor&&captured.suppressAnimEnd==(role==2u),"Role byte was clamped or captured suppression was not exactly Role2");
    }
    Fixture fresh;fresh.role=255u;fresh.mutate=true;
    const auto mutated=QuestVr::CaptureOriginalActorPawnContext(actor,fresh.Query());
    Require(mutated.suppressAnimEnd&&fresh.effects==1u,"Role was eagerly loaded before the reached Class provider");
}
void RefusalsAndSharedRoot() {
    Fixture null;Refuses([&]{QuestVr::CaptureOriginalActorPawnContext(0u,null.Query());},"Null Actor accepted");
    Require(null.events.empty()&&!null.work,"Null Actor demanded providers/work");
    for(unsigned provider=0u;provider<5u;++provider) {
        Fixture f;auto q=f.Query();
        if(provider==0u)q.pawnFlag={};
        if(provider==1u)q.actorClass={};
        if(provider==2u)q.nativePawnClass={};
        if(provider==3u)q.superClass={};
        if(provider==4u)q.role={};
        Refuses([&]{QuestVr::CaptureOriginalActorPawnContext(actor,q);},"Missing active context provider accepted");
        Require(f.events.size()==provider&&f.work==provider,"Missing provider was demanded eagerly or skipped a prior read");
    }
    Fixture badTarget;badTarget.target=0u;
    Refuses([&]{QuestVr::CaptureOriginalActorPawnContext(actor,badTarget.Query());},"Missing native Pawn class became a successful non-Pawn");
    Require(badTarget.events==std::vector<std::string>{"flag","class","target"},"Invalid native identity ran later ancestry/Role providers");
    Fixture cycle;cycle.supers[derivedClass]=derivedClass;
    Refuses([&]{QuestVr::CaptureOriginalActorPawnContext(actor,cycle.Query());},"Cyclic Class ancestry accepted");
    Require(cycle.events==std::vector<std::string>{"flag","class","target","super768"},
        "Cycle detection demanded an unbounded repeated superclass");
    Fixture deep;deep.cls=0x1000u;deep.supers.clear();
    for(Id i=0u;i<65u;++i)deep.supers[0x1000u+i]=0x1001u+i;
    Refuses([&]{QuestVr::CaptureOriginalActorPawnContext(actor,deep.Query());},"Over-depth native Class graph accepted");
    Require(deep.events.size()==67u,"Native Class depth refusal lost its exact bounded prefix");
    Fixture chain;chain.cls=0x1000u;chain.supers.clear();
    for(Id i=0u;i<63u;++i)chain.supers[0x1000u+i]=0x1001u+i;
    chain.supers[0x103fu]=pawnClass;
    Require(QuestVr::CaptureOriginalActorPawnContext(actor,chain.Query()).pawn==actor,
        "Maximum bounded successful native ancestry was refused");
    const std::vector<std::string> expected{"flag","class","target","super768","role"};
    for(std::size_t cut=0u;cut<expected.size();++cut) {
        Fixture f;f.limit=cut;
        Refuses([&]{QuestVr::CaptureOriginalActorPawnContext(actor,f.Query());},"Shared work cutoff was ignored");
        Require(f.events==std::vector<std::string>(expected.begin(),expected.begin()+static_cast<std::ptrdiff_t>(cut))&&f.work==cut+1u,
            "Work was not charged before the exact reached provider");
    }
    // Explicit mock CALLER root, not production journal proof: nested provider
    // effects must unwind with the root; the pure helper never undoes locally.
    Fixture prefix;prefix.mutate=true;prefix.role=4u;auto q=prefix.Query();
    q.role=[&](Id)->std::uint8_t {prefix.events.push_back("role");throw std::runtime_error("nested provider failed");};
    Refuses([&]{QuestVr::CaptureOriginalActorPawnContext(actor,q);},"Nested role-provider failure was swallowed");
    Require(prefix.role==2u&&prefix.effects==1u,"Helper invented independent rollback of provider effects");
    Fixture root;root.mutate=true;root.role=4u;q=root.Query();
    q.role=[&](Id)->std::uint8_t {root.events.push_back("role");throw std::runtime_error("nested provider failed");};
    const auto beforeRole=root.role;const auto beforeEffects=root.effects;bool refused{};
    try {QuestVr::CaptureOriginalActorPawnContext(actor,q);}catch(const std::exception&) {
        refused=true;root.role=beforeRole;root.effects=beforeEffects;
    }
    Require(refused&&root.role==4u&&!root.effects,"Caller-owned rollback did not restore nested provider effects");
}
} // namespace
int main() {
    try {
        ExactAndLazy();RefusalsAndSharedRoot();
        std::cout<<"Original Actor Pawn-context controls: "<<checks<<" checks / "<<refusals
            <<" refusals; prefix only, NOT ScriptedPawn Tick, Pawn movement/physics or NPC acceptance\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
