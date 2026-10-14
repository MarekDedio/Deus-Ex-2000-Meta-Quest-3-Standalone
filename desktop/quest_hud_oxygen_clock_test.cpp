#include "quest_hud_oxygen_clock.h"

#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {
std::size_t checks{};
void Require(bool condition,const char* message) {++checks;if(!condition) throw std::runtime_error(message);}
template<class Action> void Reject(Action&& action) {
    bool refused{};try {action();}catch(const std::exception&) {refused=true;}
    Require(refused,"Invalid oxygen data was accepted");
}
void Contracts() {
    using namespace QuestVr;
    HudOxygenClock clock;
    HudOxygenClockInput input{true,true,true,false,5.0f,10.0f};
    Require(clock.ReadState()==HudOxygenClockState{},"Original initial oxygen state was invented");
    auto view=clock.Advance(input);
    Require(view && !view->underwater && view->breathPercent==0.0f && !view->swimTimer,"Initial dry tick assigned oxygen");
    input.headInWater=true;input.swimDuration.reset();
    view=clock.Advance(input);
    Require(view && view->underwater && view->breathPercent==100.0f && view->swimTimer==5.0f,
        "First water-entry tick did not show100 before reading duration");
    const auto held=clock.ReadState();
    Require(!clock.Advance(input) && clock.ReadState()==held,"Missing duration changed oxygen state");
    input.swimDuration=10.0f;view=clock.Advance(input);
    Require(view && view->breathPercent==50.0f,"Second submerged tick did not use actual timer/duration");
    for(const auto [timer,duration,expected]:{std::array{20.0f,10.0f,100.0f},std::array{-5.0f,10.0f,0.0f},
        std::array{-5.0f,-10.0f,50.0f},std::array{5.0f,-10.0f,0.0f},std::array{0.0f,10.0f,0.0f}}) {
        input.swimTimer=timer;input.swimDuration=duration;view=clock.Advance(input);
        Require(view && view->breathPercent==expected,"Original signed arithmetic/clamp differs");
    }
    input.swimTimer=-0.0f;input.swimDuration=10.0f;view=clock.Advance(input);
    Require(view && std::signbit(view->breathPercent),"Clamp equality lost original negative-zero percent");
    // Compare exactly against two float32 stores over a broad finite sample.
    for(std::uint32_t n=1u;n<=1024u;++n) {
        input.swimTimer=static_cast<float>(n)*0.013f;input.swimDuration=13.71f;
        volatile float numerator=100.0f*(*input.swimTimer);
        const float quotient=numerator/(*input.swimDuration);
        const float expected=quotient<0.0f?0.0f:quotient>100.0f?100.0f:quotient;
        view=clock.Advance(input);
        Require(view && std::bit_cast<std::uint32_t>(view->breathPercent)==std::bit_cast<std::uint32_t>(expected),
            "Oxygen multiplication/division store order changed");
    }
    const auto before=clock.ReadState();
    for(unsigned gate=0u;gate<3u;++gate) {
        auto disabled=input;
        if(gate==0u) disabled.playerPresent=false;
        else if(gate==1u) disabled.hudVisible=false;
        else disabled.localPlayerEligible=false;
        disabled.swimTimer=std::numeric_limits<float>::quiet_NaN();
        Require(!clock.Advance(disabled) && clock.ReadState()==before,"Tick gate mutated held oxygen");
    }
    auto missing=input;missing.headInWater.reset();
    Require(!clock.Advance(missing) && clock.ReadState()==before,"Missing water state became dry");
    missing=input;missing.swimTimer.reset();
    Require(!clock.Advance(missing) && clock.ReadState()==before,"Missing timer changed presentation");
    for(float divisor:{0.0f,-0.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        auto invalid=input;invalid.swimDuration=divisor;
        Reject([&] {clock.Advance(invalid);});Require(clock.ReadState()==before,"Invalid divisor partially published oxygen");
    }
    for(float timer:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::max()}) {
        auto invalid=input;invalid.swimTimer=timer;
        Reject([&] {clock.Advance(invalid);});Require(clock.ReadState()==before,"Invalid numerator partially published oxygen");
    }
    auto invalid=input;invalid.swimTimer=1.0f;invalid.swimDuration=std::numeric_limits<float>::denorm_min();
    Reject([&] {clock.Advance(invalid);});Require(clock.ReadState()==before,"Overflow quotient mutated oxygen");
    input.headInWater=false;input.swimTimer.reset();input.swimDuration.reset();view=clock.Advance(input);
    Require(view && !view->underwater && view->breathPercent==100.0f && !view->swimTimer,"Leaving water failed to hide/resetoxygen");
    view=clock.Advance(input);Require(view && view->breathPercent==100.0f,"Later dry tick reset held percent");
    input.headInWater=true;input.swimTimer=0.0f;input.swimDuration=0.0f;view=clock.Advance(input);
    Require(view && view->breathPercent==100.0f,"Reentry used denominator before original math branch");
    Require(clock.ResetForContext("Training.JCDenton","Training"),"New bound context did not reset oxygen");
    Require(clock.ReadState()==HudOxygenClockState{} && !clock.ResetForContext("Training.JCDenton","Training"),
        "Context reset was not zero-initialized/idempotent");
    Reject([&] {clock.ResetForContext("Training.JCDenton","");});
    Reject([&] {clock.ResetForContext(std::string(1025u,'x'),"Training");});
    Reject([&] {clock.ResetForContext(std::string("bad\0name",8u),"Training");});
    Require(clock.ReadState()==HudOxygenClockState{},"Invalid identity changed oxygen");
    Require(clock.ResetForContext("","") && clock.ReadState()==HudOxygenClockState{},"Unbind retained oxygen context");
    clock.Advance(input);clock.Reset();Require(clock.ReadState()==HudOxygenClockState{},"Explicit reset retained oxygen");
}
}
int main() {
    try {Contracts();std::cout<<"PASS HUD oxygen presentation: "<<checks<<" checks; actual typed inputs, entry/exit, arithmetic, gates, context and atomic refusals\n";return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
