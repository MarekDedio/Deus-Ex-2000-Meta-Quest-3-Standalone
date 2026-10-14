#include "quest_hud_health_clock.h"
#include "quest_hud_ui.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace QuestVr;
std::size_t checks{};
void Require(bool value,const char* what) {++checks;if(!value) throw std::runtime_error(what);}
bool Near(float a,float b,float tolerance=0.00001f) {return std::fabs(a-b)<=tolerance;}
template<class Action> void Reject(Action&& action,const char* what) {
    bool refused{};try {action();} catch(const std::runtime_error&) {refused=true;}Require(refused,what);
}
HudHealthClockInput Values(std::int32_t value) {
    HudHealthClockInput result;for(auto& part:result) part=value;return result;
}
HudRgb Color(const HudHealthClockPartSnapshot& part) {
    return OriginalHudPartColor({part.displayedHealth,part.damageFlashFraction});
}
void Healthy(HudHealthClock& clock) {
    clock.Advance(Values(100),0.0f);clock.Advance(Values(100),1.0f);
    for(const auto& part:clock.Snapshot()) Require(part.displayedHealth==100 && part.damageFlashFraction==0.0f,
        "Actual full-health presentation did not finish original initial reveal");
}
void InitAndHeal() {
    HudHealthClock clock;
    for(const auto& part:clock.Snapshot()) Require(!part.displayedHealth && !part.colorUpdated,
        "Unobserved limb invented an initial healthy value");
    auto values=Values(100);auto frame=clock.Advance(values,0.25f);
    for(std::size_t i=0u;i<6u;++i) Require(frame[i].displayedHealth==0 && frame[i].colorUpdated &&
        clock.ReadParts()[i].healCounter==1.0f && clock.ReadParts()[i].refreshCounter==0.5f,
        "First actual positive health did not start reveal from original zero state");
    for(std::int32_t step=1;step<=4;++step) {
        frame=clock.Advance(values,0.25f);
        for(const auto& part:frame) Require(part.displayedHealth==25*step && part.damageFlashFraction==0.0f,
            "Original one-second healing interpolation differs");
    }
    values[0]=50;frame=clock.Advance(values,0.0f);Require(frame[0].displayedHealth==50,"Damage did not update displayed health immediately");
    clock.Advance(values,0.4f);
    values[0]=90;frame=clock.Advance(values,0.0f);Require(frame[0].displayedHealth==50,"Healing jumped to actual target immediately");
    frame=clock.Advance(values,0.25f);Require(frame[0].displayedHealth==60,"Heal quarter interpolation differs");
    values[0]=100;frame=clock.Advance(values,0.0f);
    Require(frame[0].displayedHealth==60 && clock.ReadParts()[0].healHealth==60 && clock.ReadParts()[0].healCounter==1.0f,
        "A second heal did not restart from current displayed health");
    frame=clock.Advance(values,0.5f);Require(frame[0].displayedHealth==80,"Restarted heal midpoint differs");
    frame=clock.Advance(values,0.5f);Require(frame[0].displayedHealth==100,"Restarted heal did not finish");
}
void DamageAndRefresh() {
    HudHealthClock clock;Healthy(clock);auto values=Values(100);values[0]=50;
    auto frame=clock.Advance(values,0.1f);
    Require(frame[0].displayedHealth==50 && frame[0].damageFlashFraction==1.0f,
        "Damage timer was decremented on the same tick it was newly reset");
    auto color=Color(frame[0]);Require(color.red==255u && color.green==255u && color.blue==255u,"New damage flash is not white");
    frame=clock.Advance(values,0.2f);
    Require(Near(frame[0].damageFlashFraction,0.5f),"Damage half-interval flash fraction differs");
    color=Color(frame[0]);
    Require(color.red==223u && color.green==255u && color.blue==127u,
        "Half damage flash did not use original byte-truncated white interpolation");
    frame=clock.Advance(values,0.2f);
    Require(frame[0].damageFlashFraction==0.0f && frame[0].displayedHealth==50,"Damage did not expire after original .4 seconds");
    color=Color(frame[0]);const auto base=OriginalHudScaledColor(0.5f);
    Require(color.red==base.red && color.green==base.green && color.blue==base.blue,"Expired damage did not return to limb health color");
    values[1]=0;frame=clock.Advance(values,0.0f);
    Require(frame[1].displayedHealth==0 && frame[1].damageFlashFraction==1.0f,"Destroyed actual limb failed white damage flash");
    frame=clock.Advance(values,1.0f);color=Color(frame[1]);
    Require(frame[1].displayedHealth==0 && !color.red && !color.green && !color.blue,"Destroyed limb did not settle to original black tint");
    for(const auto& state:clock.ReadParts()) Require(state.damageCounter==0.0f && state.healCounter==0.0f && state.refreshCounter==0.5f,
        "Long delta did not clamp flash timers/reset refresh without carrying overshoot");
    frame=clock.Advance(values,0.25f);Require(!frame[0].colorUpdated,"Unchanged limb refreshed before .5-second interval");
    frame=clock.Advance(values,0.25f);Require(frame[0].colorUpdated && clock.ReadParts()[0].refreshCounter==0.5f,
        "Refresh equality did not update and reset to literal .5");
    frame=clock.Advance(values,1.25f);Require(frame[0].colorUpdated && clock.ReadParts()[0].refreshCounter==0.5f,
        "Large refresh delta incorrectly retained negative overshoot");
    HudHealthClock retained;Healthy(retained);auto damaged=Values(100);damaged[0]=50;
    retained.Advance(damaged,0.0f);const auto beforeExpiry=retained.Advance(damaged,0.39f);
    const auto held=retained.Advance(damaged,0.02f);
    Require(retained.ReadParts()[0].damageCounter==0.0f && !held[0].colorUpdated &&
        held[0].damageFlashFraction==beforeExpiry[0].damageFlashFraction && held[0].damageFlashFraction>0.0f,
        "Damage expiry recomputed the applied tint despite original early return");
    const auto refreshed=retained.Advance(damaged,0.1f);
    Require(refreshed[0].colorUpdated && refreshed[0].damageFlashFraction==0.0f,
        "Next authored refresh failed to clear retained damage tint");
}
void OrderingAndSixLimbs() {
    HudHealthClock clock;auto values=Values(0);clock.Advance(values,0.0f);
    for(std::size_t i=0u;i<6u;++i) values[i]=static_cast<std::int32_t>((i+1u)*10u);
    clock.Advance(values,0.0f);auto frame=clock.Advance(values,0.5f);
    for(std::size_t i=0u;i<6u;++i) Require(frame[i].displayedHealth==static_cast<std::int32_t>((i+1u)*5u),
        "Independent limb interpolation was replaced with an aggregate value");
    // Original damage does NOT clear an active heal counter. The later heal
    // interpolation statement can override the damage assignment in this tick.
    values[0]=4;frame=clock.Advance(values,0.1f);
    Require(frame[0].displayedHealth==2 && frame[0].damageFlashFraction==1.0f && Near(clock.ReadParts()[0].healCounter,0.4f),
        "Damage during healing did not preserve original counter/statement ordering");
    frame=clock.Advance(values,0.4f);Require(frame[0].displayedHealth==4 && frame[0].damageFlashFraction==0.0f,
        "Overlapping heal/damage did not settle at the actual health");
    values[2]=-10;frame=clock.Advance(values,0.0f);Require(frame[2].displayedHealth==-10,"Actual negative limb health was silently clamped");
    values[2]=120;clock.Advance(values,0.0f);frame=clock.Advance(values,1.0f);
    Require(frame[2].displayedHealth==120,"Actual over-100 limb health was silently clamped");
    const auto full=values;
    values[3]=std::nullopt;frame=clock.Advance(values,0.1f);
    Require(!frame[3].displayedHealth && !clock.ReadParts()[3].known,"Missing limb retained a stale/healthy display");
    Require(frame[2].displayedHealth==120 && clock.ReadParts()[2].known,"Missing one limb reset unrelated clocks");
    values=full;frame=clock.Advance(values,0.0f);Require(frame[3].displayedHealth==0 && clock.ReadParts()[3].healCounter==1.0f,
        "Newly available limb did not restart its presentation from original zero state");
}
void TickRatesAndContext() {
    for(const unsigned hz:{30u,72u}) {
        HudHealthClock clock;auto values=Values(100);clock.Advance(values,0.0f);
        for(unsigned tick=0u;tick<hz;++tick) {
            const auto frame=clock.Advance(values,1.0f/static_cast<float>(hz));
            Require(frame[0].displayedHealth>=0 && frame[0].displayedHealth<=100 &&
                frame[0].damageFlashFraction==0.0f,"Realistic tick-rate interpolation exceeded bounds");
        }
        auto frame=clock.Advance(values,1.0f/static_cast<float>(hz));
        const auto lastActiveFloor=100-static_cast<std::int32_t>(std::ceil(100.0f/static_cast<float>(hz)));
        Require(clock.ReadParts()[0].healCounter==0.0f && frame[0].displayedHealth>=lastActiveFloor && frame[0].displayedHealth<=100,
            "Healing expiry differs across binary32 30/72Hz ticks");
        // The original early-return test precedes the final displayed=actual
        // assignment. The last active 30Hz step can retain 96 until refresh.
        frame=clock.Advance(values,0.5f);
        Require(frame[0].displayedHealth==100,"Expired healing failed to settle at the authored next refresh");
        values[4]=20;frame=clock.Advance(values,1.0f/static_cast<float>(hz));
        Require(frame[4].damageFlashFraction==1.0f && frame[4].displayedHealth==20,"Tick-rate damage onset was shortened");
        for(unsigned tick=0u;tick<hz/2u;++tick) frame=clock.Advance(values,1.0f/static_cast<float>(hz));
        Require(frame[4].damageFlashFraction==0.0f,"Damage failed to expire at realistic tick rate");
    }
    HudHealthClock whole,split;Healthy(whole);Healthy(split);auto values=Values(100);values[0]=40;
    whole.Advance(values,0.0f);split.Advance(values,0.0f);
    const auto a=whole.Advance(values,0.25f);split.Advance(values,0.125f);const auto b=split.Advance(values,0.125f);
    Require(a[0].displayedHealth==b[0].displayedHealth && Near(a[0].damageFlashFraction,b[0].damageFlashFraction),
        "Equivalent exactly representable timer splits diverged");
    const auto before=whole.ReadParts();HudHealthClockInput unavailable;
    whole.Advance(unavailable,2.0f,false);Require(whole.ReadParts()==before,"Hidden original Tick advanced clocks or read missing health");
    Require(whole.ResetForContext("Training.JCDenton1","00_Training"),"First actual context did not reset clocks");
    Healthy(whole);const auto kept=whole.ReadParts();
    Require(!whole.ResetForContext("Training.JCDenton1","00_Training") && whole.ReadParts()==kept,"Same player/map reset continuous timers");
    Require(whole.ResetForContext("Training.JCDenton2","00_Training") && !whole.Snapshot()[0].displayedHealth,
        "Replacement player retained former limb clocks");
    Healthy(whole);Require(whole.ResetForContext("Training.JCDenton2","01_NYC_UNATCOIsland") && !whole.Snapshot()[0].displayedHealth,
        "Map replacement retained former limb clocks");
    Require(whole.ResetForContext({},{}),"Unbinding context did not clear clocks");
    whole.Reset();Require(!whole.Snapshot()[0].displayedHealth,"Explicit clock reset invented known health");
}
void RefusalPolicies() {
    HudHealthClock clock;Healthy(clock);const auto original=clock.ReadParts();
    for(float delta:{-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()}) {
        Reject([&] {clock.Advance(Values(50),delta);},"Invalid delta was accepted");
        Require(clock.ReadParts()==original,"Rejected delta partially advanced presentation state");
    }
    Reject([&] {clock.ResetForContext("Player",{});},"Partial player/map identity was accepted");
    Reject([&] {clock.ResetForContext(std::string("A\0B",3u),"Map");},"Embedded-NUL player identity was accepted");
    Reject([&] {clock.ResetForContext("Player",std::string(kHudHealthClockIdentityBytes+1u,'m'));},"Oversized map identity was accepted");
    Require(clock.ReadParts()==original,"Rejected context identity reset valid clocks");
    auto values=Values(100);values[0]=50;
    const auto frame=clock.Advance(values,std::numeric_limits<float>::max());
    Require(frame[0].displayedHealth==50 && frame[0].damageFlashFraction==1.0f,
        "Finite large delta changed authored damage-onset reset order");
    HudHealthClock extreme;auto huge=Values(0);huge[5]=std::numeric_limits<std::int32_t>::max();
    extreme.Advance(huge,0.0f);extreme.Advance(huge,0.5f);const auto before=extreme.ReadParts();
    Reject([&] {extreme.Advance(huge,std::nextafter(0.5f,0.0f));},"Out-of-range original FloatToInt interpolation was accepted");
    Require(extreme.ReadParts()==before,"Interpolation refusal partially committed other limb clocks");
}
void ScaledColorThresholds() {
    for(const auto& [actual,expected]:std::array<std::pair<std::int32_t,HudRgb>,9>{{
        {-1,{0u,0u,0u}},{0,{0u,0u,0u}},{1,{255u,0u,0u}},{10,{255u,0u,0u}},
        {40,{255u,255u,0u}},{80,{0u,255u,0u}},{81,{0u,255u,0u}},
        {100,{0u,255u,0u}},{120,{0u,255u,0u}}}}) {
        HudHealthClock clock;clock.Advance(Values(actual),0.0f);
        const auto frame=clock.Advance(Values(actual),2.0f);
        for(const auto& part:frame) {
            Require(part.displayedHealth==actual && part.damageFlashFraction==0.0f,
                "Stable actual limb health was clamped or retained a flash");
            const auto color=Color(part);
            Require(color.red==expected.red && color.green==expected.green && color.blue==expected.blue,
                "Stable limb did not use original zero/.1/.4/.8 scaled-color thresholds");
        }
    }
}
} // namespace

int main() {
    try {
        InitAndHeal();DamageAndRefresh();OrderingAndSixLimbs();TickRatesAndContext();RefusalPolicies();ScaledColorThresholds();
        std::cout<<"PASS original HUDHitDisplay presentation clock: "<<checks
            <<" checks; six actual limbs, .4s damage/1s healing/.5s refresh, original statement order, 30/72Hz, context reset and portable refusal policies\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
