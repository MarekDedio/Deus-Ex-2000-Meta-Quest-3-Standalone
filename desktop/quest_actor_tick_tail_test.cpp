#include "quest_actor_tick_tail.h"

#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::size_t checks{},refusals{};
void Require(bool condition,const char* message) {++checks;if(!condition)throw std::runtime_error(message);}
struct Fixture {
    static constexpr std::uintptr_t actor=17u;
    std::uint8_t remote{},role{4u},physics{};
    float life{},elapsed{0.125f};
    bool probe{true};
    std::vector<std::string> calls;
    std::size_t work{},cutoff=std::numeric_limits<std::size_t>::max();
    QuestVr::OriginalNonPawnTickTailQueries q;
    Fixture() {
        q.work=[&]{if(work++==cutoff)throw std::runtime_error("budget");};
        q.remoteRole=[&](auto a){Check(a);calls.emplace_back("remote");return remote;};
        q.role=[&](auto a){Check(a);calls.emplace_back("role");return role;};
        q.physics=[&](auto a){Check(a);calls.emplace_back("physics");return physics;};
        q.isProbingTick=[&](auto a){Check(a);calls.emplace_back("probe");return probe;};
        q.tick=[&](auto a,float dt){Step(a,dt,"tick");};
        q.processState=[&](auto a,float dt){Step(a,dt,"state");};
        q.timer=[&](auto a,float dt){Step(a,dt,"timer");};
        q.performPhysics=[&](auto a,float dt){Step(a,dt,"performPhysics");};
        q.lifeSpan=[&](auto a){Check(a);calls.emplace_back("life");return life;};
        q.writeLifeSpan=[&](auto a,float value){Check(a);calls.emplace_back("store");life=value;};
        q.expired=[&](auto a){Check(a);calls.emplace_back("expired");};
        q.destroy=[&](auto a,std::int32_t force){Check(a);Require(force==0,"Destroy force changed");calls.emplace_back("destroy");};
    }
    void Check(std::uintptr_t a){Require(a==actor,"Retained Actor changed");}
    void Step(std::uintptr_t a,float dt,const char* name) {
        Check(a);Require(std::bit_cast<std::uint32_t>(dt)==std::bit_cast<std::uint32_t>(elapsed),"Whole DeltaTime packet changed");
        calls.emplace_back(name);
    }
    auto Run(std::int32_t type=0){return QuestVr::AdvanceOriginalNonPawnTickTail(actor,elapsed,type,q);}
};
template<class Operation> void Refuses(Operation&& operation) {
    bool caught{};try{operation();}catch(const std::exception&){caught=true;}
    Require(caught,"Active missing provider/arithmetic failure was silently accepted");++refusals;
}
void Branches() {
    for(unsigned remote=0;remote<256u;++remote)for(unsigned role=0;role<256u;++role)
        for(unsigned mode:{0u,2u,6u})for(const auto type:{0,1}) {
            Fixture f;f.remote=static_cast<std::uint8_t>(remote);f.role=static_cast<std::uint8_t>(role);
            f.physics=static_cast<std::uint8_t>(mode);const auto result=f.Run(type);
            const bool normal=remote!=3u&&role>=2u&&type!=1;
            const bool state=normal||(remote==3u&&role==4u);
            const bool physics=remote!=3u&&(role<2u?mode==2u:normal&&mode!=0u&&role!=3u);
            Require(result.tickCalls==static_cast<std::size_t>(normal)&&result.stateCalls==static_cast<std::size_t>(state)&&
                result.timerCalls==static_cast<std::size_t>(state)&&result.physicsCalls==static_cast<std::size_t>(physics),
                "Original unsigned role/physics/tick-type branch changed");
            Require(result.steps==f.work&&result.steps==f.calls.size(),"Per-provider native work accounting changed");
        }
    Fixture normal;normal.physics=2u;normal.life=1.0f;const auto stats=normal.Run();
    Require(normal.calls==std::vector<std::string>{"remote","role","probe","tick","state","timer","life","store","physics","role","performPhysics"},
        "Normal callback/store/fresh-physics order changed");
    Require(normal.life==0.875f&&stats.lifeSpanWrites==1u&&!stats.expired,"LifeSpan subtraction changed");
    Fixture noTick;noTick.probe=false;noTick.Run();
    Require(noTick.calls==std::vector<std::string>{"remote","role","probe","state","timer","life","physics"},
        "Disabled Tick skipped other Actor phases or read Role with Physics0");
    Fixture special;special.remote=3u;special.life=0.01f;special.physics=2u;special.Run(1);
    Require(special.calls==std::vector<std::string>{"remote","role","state","timer"}&&special.life==0.01f,
        "RemoteRole3 Authority was treated as the normal suffix or tickType1");
}
void ReentryAndExpiry() {
    Fixture f;f.q.tick=[&](auto a,float dt){f.Step(a,dt,"tick");f.physics=2u;f.role=3u;};
    f.Run();Require(f.calls.back()=="role","Fresh Role3 after Tick did not skip physics");
    Fixture fresh;fresh.q.timer=[&](auto a,float dt){fresh.Step(a,dt,"timer");fresh.physics=2u;fresh.role=0u;};
    Require(fresh.Run().physicsCalls==1u,"Late callback Physics/Role were not reread");
    Fixture expired;expired.life=0.125f;expired.physics=2u;
    expired.q.expired=[&](auto a){expired.Check(a);expired.calls.emplace_back("expired");expired.life=500.0f;};
    const auto stats=expired.Run();
    Require(stats.expired&&stats.destroyCalls==1u&&!stats.physicsCalls&&expired.life==500.0f&&
        expired.calls==std::vector<std::string>{"remote","role","probe","tick","state","timer","life","store","expired","destroy"},
        "Expired replacement lifespan cancelled native DestroyActor or reached physics");
    Fixture negative;negative.life=-0.125f;Require(negative.Run().expired&&negative.life==-0.25f,"Negative LifeSpan was clamped or disabled");
    Fixture zero;zero.life=-0.0f;Require(!zero.Run().lifeSpanWrites&&std::signbit(zero.life),"Signed zero LifeSpan was overwritten");
    Fixture nan;nan.life=std::bit_cast<float>(0x7fc12345u);Require(!nan.Run().lifeSpanWrites&&std::bit_cast<std::uint32_t>(nan.life)==0x7fc12345u,
        "Unordered initial LifeSpan did not take native inactive branch");
    // Exact PC64 result is just ABOVE Double0.0001, while its stored Float is
    // BELOW it. An implementation that compares the Float expires too soon.
    Fixture boundary;boundary.life=std::nextafter(0.0001f,std::numeric_limits<float>::infinity());
    boundary.elapsed=std::ldexp(5.0f,-40);
    Require(!boundary.Run().expired&&boundary.life==0.0001f,"LifeSpan comparison used its rounded Float instead of live PC64");
    Fixture threshold;threshold.life=0.0001f;threshold.elapsed=0.0f;
    Require(threshold.Run().expired,"Original Double expiry threshold changed");
}
void RefusalAndInactive() {
    Refuses([]{QuestVr::AdvanceOriginalNonPawnTickTail(0u,0.0f,0,{});});
    for(unsigned selected=0u;selected<12u;++selected) {
        Fixture f;f.life=0.01f;f.physics=2u;
        switch(selected) {
        case 0:f.q.remoteRole={};break;
        case 1:f.q.role={};break;
        case 2:f.q.isProbingTick={};break;
        case 3:f.q.tick={};break;
        case 4:f.q.processState={};break;
        case 5:f.q.timer={};break;
        case 6:f.q.lifeSpan={};break;
        case 7:f.q.writeLifeSpan={};break;
        case 8:f.q.expired={};break;
        case 9:f.q.destroy={};break;
        case 10:f.life=0.0f;f.q.physics={};break;
        case 11:f.life=0.0f;f.q.performPhysics={};break;
        }
        Refuses([&]{f.Run();});
    }
    Fixture full;full.physics=2u;full.life=1.0f;const auto stats=full.Run();
    for(std::size_t cut=0u;cut<stats.steps;++cut) {
        Fixture f;f.physics=2u;f.life=1.0f;f.cutoff=cut;
        Refuses([&]{f.Run();});Require(f.calls.size()==cut,"Provider called after exhausted native work");
    }
    Fixture inf;inf.life=std::numeric_limits<float>::infinity();Refuses([&]{inf.Run();});
    Fixture overflow;overflow.life=-std::numeric_limits<float>::max();overflow.elapsed=std::numeric_limits<float>::max();
    Refuses([&]{overflow.Run();});
    Fixture inactive;inactive.elapsed=std::numeric_limits<float>::quiet_NaN();inactive.q={};
    inactive.q.remoteRole=[](auto){return std::uint8_t{0};};inactive.q.role=[](auto){return std::uint8_t{4};};
    Require(inactive.Run(1).tickTypeSkipped,"Inactive arithmetic/provider was eagerly admitted");
    Fixture unprobed;unprobed.probe=false;unprobed.q.tick={};Require(!unprobed.Run().tickCalls,"Inactive Tick provider was required");
}
}
int main() {
    try{Branches();ReentryAndExpiry();RefusalAndInactive();std::cout<<"PASS original non-Pawn Tick tail: "<<checks<<" checks, "<<refusals<<" refusals; not full Actor/Level Tick\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
