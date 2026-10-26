#include "quest_player_local_tick.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
std::size_t checks{},refusals{};
constexpr std::uintptr_t Actor=0x10u,Player=0x20u,Replacement=0x21u;
void Require(bool condition,const std::string& text) {
    ++checks;if(!condition)throw std::runtime_error(text);
}
template<class Function>
void Refuses(Function&& function,const std::string& expected) {
    try {function();}catch(const std::exception& error) {
        Require(std::string(error.what()).find(expected)!=std::string::npos,
            "Unexpected local-player refusal: "+std::string(error.what()));++refusals;return;
    }
    throw std::runtime_error("Local-player schedule admitted an expected refusal");
}
struct Fixture {
    bool camera{},driver{},replacePlayer{},nullAfterInput{},serverConnection{};
    std::uint32_t flags{0x800u};
    std::uintptr_t currentPlayer{Player};
    std::size_t used{100u},limit{std::numeric_limits<std::size_t>::max()},inputCalls{};
    int scriptState{},networkCopies{};
    std::string fault;
    std::vector<std::string> events;
    std::vector<std::pair<std::uintptr_t,std::uint32_t>> nativeInputs;
    std::vector<std::uint32_t> scriptDeltas;
    void Mark(const char* name) {events.emplace_back(name);if(fault==name)throw std::runtime_error(std::string("fault:")+name);}
    void Source(std::uintptr_t actor) {Require(actor==Actor,"Local-player provider changed the actual source");}
    QuestVr::OriginalPlayerLocalTickQueries Query() {
        QuestVr::OriginalPlayerLocalTickQueries q;
        q.work=[&] {if(used==limit)throw std::runtime_error("work cutoff");++used;};
        q.cameraCast=[&](auto actor){Source(actor);Mark("camera");return camera;};
        q.cameraShowFlags=[&](auto actor){Source(actor);Mark("flags");return flags;};
        q.player=[&](auto actor){Source(actor);Mark("player");return currentPlayer;};
        q.readInput=[&](auto player,float delta) {
            Require(player==currentPlayer,"Native ReadInput did not receive freshly read Player");
            Mark(inputCalls++?"post":"pre");nativeInputs.emplace_back(player,std::bit_cast<std::uint32_t>(delta));
        };
        q.playerInput=[&](auto actor,float delta){Source(actor);Mark("input");scriptDeltas.push_back(std::bit_cast<std::uint32_t>(delta));
            scriptState+=3;if(replacePlayer)currentPlayer=Replacement;if(nullAfterInput)currentPlayer=0u;};
        q.playerTick=[&](auto actor,float delta){Source(actor);Mark("tick");scriptDeltas.push_back(std::bit_cast<std::uint32_t>(delta));scriptState+=7;};
        q.hasNetDriver=[&](auto actor){Source(actor);Mark("driver");return driver;};
        q.networkTail=[&](auto actor){Source(actor);Mark("network");if(!serverConnection)++networkCopies;};
        return q;
    }
    QuestVr::OriginalPlayerLocalTickStats Run(float delta=0.25f) {
        return QuestVr::AdvanceOriginalPlayerLocalTick(Actor,Actor,Player,delta,Query());
    }
};
void OrdinaryAndLazy() {
    Fixture fixture;auto q=fixture.Query();q.cameraShowFlags={};q.networkTail={};
    const auto out=QuestVr::AdvanceOriginalPlayerLocalTick(Actor,Actor,Player,0.25f,q);
    Require(out.result==QuestVr::OriginalPawnLocalTickResult::ContinueState&&!out.cameraEarlyReturn&&!out.networkBranch,
        "Ordinary local player did not join state continuation");
    Require(fixture.events==std::vector<std::string>{"camera","player","pre","input","tick","player","post","driver"},
        "Original local player provider order changed");
    Require(out.steps==8u&&fixture.used==108u&&out.cameraCasts==1u&&out.cameraFlagReads==0u&&out.playerReads==2u&&
        out.readInputCalls==2u&&out.playerInputCalls==1u&&out.playerTickCalls==1u&&out.driverReads==1u&&out.networkTailCalls==0u,
        "Ordinary local-player accounting changed");
    Require(out.prePlayer==Player&&out.postPlayer==Player&&fixture.scriptState==10&&
        fixture.nativeInputs==std::vector<std::pair<std::uintptr_t,std::uint32_t>>{{Player,0x3e800000u},{Player,0xbf800000u}}&&
        fixture.scriptDeltas==std::vector<std::uint32_t>{0x3e800000u,0x3e800000u},"Local player changed native/script Float packets");
    for(const auto flags:{0u,0x7ffu,0xfffff7ffu}) {
        Fixture camera;camera.camera=true;camera.flags=flags;auto lazy=camera.Query();
        lazy.player={};lazy.readInput={};lazy.playerInput={};lazy.playerTick={};lazy.hasNetDriver={};lazy.networkTail={};
        const auto returned=QuestVr::AdvanceOriginalPlayerLocalTick(Actor,Actor,Player,0.25f,lazy);
        Require(returned.result==QuestVr::OriginalPawnLocalTickResult::ReturnOne&&returned.cameraEarlyReturn&&
            returned.steps==2u&&camera.used==102u&&camera.events==std::vector<std::string>{"camera","flags"}&&
            returned.playerReads==0u&&returned.driverReads==0u&&camera.scriptState==0,"Camera0800-clear early return is not lazy");
    }
    for(const auto flags:{0x800u,0xffffffffu}) {
        Fixture camera;camera.camera=true;camera.flags=flags;const auto passed=camera.Run();
        Require(passed.result==QuestVr::OriginalPawnLocalTickResult::ContinueState&&passed.steps==9u&&
            passed.cameraFlagReads==1u&&camera.scriptState==10,"Camera0800-set did not run full input slice");
    }
    Fixture changed;changed.replacePlayer=true;const auto refreshed=changed.Run();
    Require(refreshed.prePlayer==Player&&refreshed.postPlayer==Replacement&&changed.nativeInputs.at(1u).first==Replacement,
        "Local player retained stale Player across callbacks");
    Fixture preNull;preNull.currentPlayer=0u;Refuses([&]{preNull.Run();},"null pre-input Player");
    Require(preNull.nativeInputs.empty()&&preNull.scriptState==0,"Null pre-input Player ran later work");
    Fixture postNull;postNull.nullAfterInput=true;Refuses([&]{postNull.Run();},"null post-input Player");
    Require(postNull.nativeInputs.size()==1u&&postNull.scriptState==10&&postNull.events.back()=="player",
        "Null post-input Player became successful absent input or ran network tail");
}
void RawPacketsAndNetwork() {
    for(const auto bits:{0u,0x80000000u,0x00000001u,0x80000001u,0x3f800000u,0xbf800000u,
        0x7f800000u,0xff800000u,0x7fc01234u,0x7f800001u,0xffc05678u}) {
        Fixture fixture;const auto out=fixture.Run(std::bit_cast<float>(bits));
        Require(out.result==QuestVr::OriginalPawnLocalTickResult::ContinueState&&fixture.nativeInputs.at(0u).second==bits&&
            fixture.nativeInputs.at(1u).second==0xbf800000u&&fixture.scriptDeltas==std::vector<std::uint32_t>{bits,bits},
            "Local-player schedule performed arithmetic/normalization on raw Float delta");
    }
    for(const bool connected:{false,true}) {
        Fixture fixture;fixture.driver=true;fixture.serverConnection=connected;const auto out=fixture.Run();
        Require(out.networkBranch&&out.networkTailCalls==1u&&out.steps==9u&&fixture.events.back()=="network"&&
            fixture.events.at(fixture.events.size()-2u)=="driver"&&fixture.networkCopies==(connected?0:1),
            "Nonnull network provider ran before post-input or lost its actual conditional copy");
    }
}
void RefusalAndSharedWork() {
    const std::vector<std::string> stages{"camera","flags","player","pre","input","tick","post","driver","network"};
    for(const auto& stage:stages) {
        Fixture fixture;fixture.camera=true;fixture.driver=true;fixture.fault=stage;
        Refuses([&]{fixture.Run();},"fault:"+stage);
        Require(fixture.events.back()==stage&&fixture.used==100u+fixture.events.size(),"Provider fault ran later work or lost shared accounting");
        if(stage=="input"||stage=="tick")Require(fixture.nativeInputs.size()==1u,"Thrown script callback triggered post-input cleanup");
        if(stage=="tick")Require(fixture.scriptState==3,"Helper fabricated rollback instead of leaving undo to caller root");
    }
    const std::array<std::function<void(QuestVr::OriginalPlayerLocalTickQueries&)>,8u> missing{{
        [](auto& q){q.cameraCast={};},[](auto& q){q.cameraShowFlags={};},[](auto& q){q.player={};},
        [](auto& q){q.readInput={};},[](auto& q){q.playerInput={};},[](auto& q){q.playerTick={};},
        [](auto& q){q.hasNetDriver={};},[](auto& q){q.networkTail={};}}};
    for(const auto& remove:missing) {
        Fixture fixture;fixture.camera=true;fixture.driver=true;auto q=fixture.Query();remove(q);
        Refuses([&]{QuestVr::AdvanceOriginalPlayerLocalTick(Actor,Actor,Player,0.25f,q);},"reached provider");
    }
    Fixture baseline;baseline.camera=true;baseline.driver=true;const auto successful=baseline.Run();
    for(std::size_t cut=0u;cut<successful.steps;++cut) {
        Fixture fixture;fixture.camera=true;fixture.driver=true;fixture.limit=100u+cut;
        Refuses([&]{fixture.Run();},"work cutoff");
        Require(fixture.used==100u+cut&&fixture.events.size()==cut,"Local player reset or exceeded caller shared-work allowance");
    }
    Fixture fixture;auto noWork=fixture.Query();noWork.work={};
    Refuses([&]{QuestVr::AdvanceOriginalPlayerLocalTick(Actor,Actor,Player,0.25f,noWork);},"shared work authority");
    for(const auto& identities:std::array<std::array<std::uintptr_t,3u>,4u>{{{0u,Actor,Player},{Actor,0u,Player},
        {Actor,Actor,0u},{Actor,Actor+1u,Player}}}) {
        Refuses([&]{QuestVr::AdvanceOriginalPlayerLocalTick(identities[0u],identities[1u],identities[2u],0.25f,fixture.Query());},"nonnull source/Pawn/Player");
        Require(fixture.events.empty()&&fixture.used==100u,"Invalid identity consumed work or reached providers");
    }
}
} // namespace
int main() {
    try {
        OrdinaryAndLazy();RawPacketsAndNetwork();RefusalAndSharedWork();
        std::cout<<"PASS original local-player schedule: "<<checks<<" checks / "<<refusals
            <<" refusals; camera ShowFlags0800, fresh Player pre/post, checked-event provider order/raw packets, "
              "network provider, missing/fault providers and shared work. Coordinator only, NOT actual Input/InputExt, "
              "script VM rollback, complete startup or headset gameplay.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
