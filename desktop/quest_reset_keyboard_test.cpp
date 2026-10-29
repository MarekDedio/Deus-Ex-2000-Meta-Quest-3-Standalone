#include "quest_reset_keyboard.h"
#include "quest_native_work.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Provider mocks only: proves native544 prefix/gates/shared work, not actual
// Input construction, UObject.ResetConfig effects, player startup or gameplay.
namespace {
using Id=QuestVr::ResetKeyboardNativeId;
using Outcome=QuestVr::OriginalResetKeyboardOutcome;
constexpr Id Pawn=41u,Player=57u,Input=83u,Class=109u;
constexpr std::size_t Unlimited=std::numeric_limits<std::size_t>::max(),OuterWork=7u;
std::size_t checks{},refusals{};
void Require(bool value,const char* why) {++checks;if(!value)throw std::runtime_error(why);}
struct Injected {};
struct Mock {
    QuestVr::NativeWork::Budget work{512u};
    Id player{Player},input{Input},cls{Class};
    bool rootOpen{true},viewport{true},rootChecked{};
    std::size_t failAt{Unlimited},calls{},resetCalls{};
    std::vector<std::string> events;
    Mock() {work.Step(OuterWork);}
    void Enter(const char* event) {
        events.emplace_back(event);
        if(calls++==failAt)throw Injected{};
        if(events.size()>1u)Require(rootChecked&&rootOpen,"ResetKeyboard provider escaped its existing root");
    }
    QuestVr::OriginalResetKeyboardQueries Bind() {
        QuestVr::OriginalResetKeyboardQueries query;
        query.work=[this]{work.Step();};
        query.requireOuterRoot=[this]{Enter("root");if(!rootOpen)throw Injected{};rootChecked=true;};
        query.playerPawnPlayer=[this](Id pawn){Enter("player");Require(pawn==Pawn,"Player field read substituted its receiver");
            work.Step(2u);return player;};
        query.isViewport=[this](Id candidate){Enter("viewport?");Require(candidate==Player,"Viewport predicate used another actual Player");return viewport;};
        query.viewportInput=[this](Id candidate){Enter("input");Require(candidate==Player&&viewport,"Input read preceded actual Viewport proof");return input;};
        query.inputClass=[this](Id candidate){Enter("class");Require(candidate==Input,"Class read substituted the actual Input");return cls;};
        query.resetConfig=[this](Id candidate){Enter("reset");Require(candidate==Class,"ResetConfig did not receive actual selected Input.Class");
            work.Step(3u);++resetCalls;};
        return query;
    }
    Outcome Run() {return QuestVr::RunOriginalResetKeyboard(Pawn,Bind());}
};
template<class Function> void Refuses(Function&& function,const char* why) {
    bool refused{};try {function();}catch(const std::exception&) {refused=true;}catch(const Injected&) {refused=true;}
    Require(refused,why);++refusals;
}
void GatesAndOrder() {
    {
        Mock mock;mock.player=0u;auto query=mock.Bind();
        query.isViewport={};query.viewportInput={};query.inputClass={};query.resetConfig={};
        Require(QuestVr::RunOriginalResetKeyboard(Pawn,query)==Outcome::NoPlayer&&mock.resetCalls==0u,
            "Actual None Player did not preserve its original early return");
        Require(mock.events==std::vector<std::string>{"root","player"},"None Player queried unreachable providers");
    }
    {
        Mock mock;mock.viewport=false;auto query=mock.Bind();query.viewportInput={};query.inputClass={};query.resetConfig={};
        Require(QuestVr::RunOriginalResetKeyboard(Pawn,query)==Outcome::NotViewport&&mock.resetCalls==0u,
            "Actual non-Viewport Player did not preserve its original early return");
        Require(mock.events==std::vector<std::string>{"root","player","viewport?"},"Non-Viewport Player queried Input storage");
    }
    {
        Mock mock;mock.input=0u;auto query=mock.Bind();query.inputClass={};query.resetConfig={};
        Require(QuestVr::RunOriginalResetKeyboard(Pawn,query)==Outcome::NoInput&&mock.resetCalls==0u,
            "Actual None Viewport.Input did not preserve its original early return");
        Require(mock.events==std::vector<std::string>{"root","player","viewport?","input"},"None Input queried Class/reset providers");
    }
    {
        Mock mock;
        Require(mock.Run()==Outcome::ResetConfigInvoked&&mock.resetCalls==1u,"Actual Input.Class did not invoke the reset service exactly once");
        Require(mock.events==std::vector<std::string>{"root","player","viewport?","input","class","reset"},
            "ResetKeyboard did not preserve native pointer/predicate/reset order");
        Require(mock.work.Read().used==OuterWork+11u&&!mock.work.Read().exhausted,
            "ResetKeyboard reset the outer work ledger or omitted provider work");
    }
    {
        Mock mock;mock.cls=0u;
        Refuses([&]{static_cast<void>(mock.Run());},"Invalid Input.Class None was treated as a successful no-op reset");
        Require(mock.resetCalls==0u&&mock.events.back()=="class","None Class reached actual ResetConfig");
    }
}
void MissingAndFailureProviders() {
    for(std::size_t missing=0u;missing<7u;++missing) {
        Mock mock;auto query=mock.Bind();
        switch(missing) {
        case 0u:query.requireOuterRoot={};break;
        case 1u:query.work={};break;
        case 2u:query.playerPawnPlayer={};break;
        case 3u:query.isViewport={};break;
        case 4u:query.viewportInput={};break;
        case 5u:query.inputClass={};break;
        case 6u:query.resetConfig={};break;
        default:throw std::runtime_error("Unknown ResetKeyboard missing provider fixture");
        }
        Refuses([&]{static_cast<void>(QuestVr::RunOriginalResetKeyboard(Pawn,query));},"Missing reached provider was converted into genuine None/success");
        Require(mock.resetCalls==0u,"Missing provider unexpectedly invoked ResetConfig");
    }
    for(std::size_t fail=0u;fail<6u;++fail) {
        Mock mock;mock.failAt=fail;
        Refuses([&]{static_cast<void>(mock.Run());},"Reached provider failure was swallowed by ResetKeyboard");
        Require(mock.events.size()==fail+1u&&mock.resetCalls==0u,"Provider failure continued into a later native operation");
    }
    Mock inactive;inactive.rootOpen=false;
    Refuses([&]{static_cast<void>(inactive.Run());},"Absent existing root was replaced by a new helper-owned root");
    Require(inactive.events==std::vector<std::string>{"root"},"Invalid root queried Player data");
    Mock noPawn;const auto query=noPawn.Bind();
    Refuses([&]{static_cast<void>(QuestVr::RunOriginalResetKeyboard(0u,query));},"None PlayerPawn receiver was accepted");
    Require(noPawn.events.empty()&&noPawn.work.Read().used==OuterWork,"Invalid receiver performed provider work");
}
void WorkCuts() {
    for(unsigned route=0u;route<4u;++route) {
        const auto configure=[route](Mock& mock) {
            if(route==0u)mock.player=0u;
            else if(route==1u)mock.viewport=false;
            else if(route==2u)mock.input=0u;
        };
        Mock complete;configure(complete);const auto expected=complete.Run();
        const auto full=complete.work.Read().used-OuterWork;
        for(std::size_t allowance=0u;allowance<full;++allowance) {
            Mock cut;configure(cut);cut.work.Reset(OuterWork+allowance);cut.work.Step(OuterWork);
            Refuses([&]{static_cast<void>(cut.Run());},"Native work cutoff returned an unverified ResetKeyboard outcome");
            const auto usage=cut.work.Read();
            Require(usage.exhausted&&usage.limit==OuterWork+allowance&&usage.used>=OuterWork&&usage.used<=usage.limit,
                "ResetKeyboard reset/exceeded its borrowed native work limit");
            Require(cut.resetCalls==0u,"Exhausted native prefix performed an unauthorised reset mutation");
        }
        Mock exact;configure(exact);exact.work.Reset(OuterWork+full);exact.work.Step(OuterWork);
        Require(exact.Run()==expected&&exact.work.Read().used==OuterWork+full&&!exact.work.Read().exhausted,
            "Exact shared work allowance changed ResetKeyboard outcome/accounting");
    }
}
}
int main() {
    try {GatesAndOrder();MissingAndFailureProviders();WorkCuts();
        std::cout<<"Original native544 ResetKeyboard prefix: "<<checks<<" checks / "<<refusals<<" explicit refusals\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
