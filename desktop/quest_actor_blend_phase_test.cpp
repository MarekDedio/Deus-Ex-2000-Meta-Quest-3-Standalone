#include "quest_actor_blend_phase.h"

#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using F=QuestVr::OriginalActorBlendFloat;
using M=QuestVr::OriginalActorAnimationFloat;
using Q=QuestVr::OriginalActorBlendPhaseQueries;
using Main=QuestVr::OriginalActorAnimationPhaseStats;
constexpr std::uintptr_t Actor=17u;
unsigned checks{},refusals{};
void Require(bool condition,const char* message) {
    ++checks;if(!condition)throw std::runtime_error(message);
}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
float& At(std::array<float,7u>& values,F field) {return values.at(static_cast<std::size_t>(field));}
const char* Label(F field) {
    constexpr std::array names{"Frame","Rate","TweenRate","Last","MinRate","SimX","SimY"};
    return names.at(static_cast<std::size_t>(field));
}
template<class Action> void Refuses(Action action,const char* message) {
    bool failed{};try {action();}catch(const std::runtime_error&) {failed=true;}
    ++refusals;Require(failed,message);
}
struct Slot {
    std::string sequence{"None"};
    std::array<float,7u> values{0.0f,1.0f,0.0f,10.0f,0.0f,101.0f,103.0f};
    float& Get(F field) {return At(values,field);}
    bool operator==(const Slot&) const=default;
};
struct State {
    std::string mainSequence{"Idle"};
    float mainFrame{0.0f},mainRate{1.0f},mainTween{0.0f};
    std::array<Slot,4u> slots;
    std::array<float,3u> velocity{};
    std::uint8_t role{2u};bool weapon{};
    bool operator==(const State&) const=default;
};
struct Fixture {
    State state;Main main;Q query;
    std::vector<std::string> calls;
    std::size_t work{},cutoff{std::numeric_limits<std::size_t>::max()};
    std::function<void(unsigned,F)> afterWrite;
    Fixture() {
        main.remainingSeconds=0.125f;
        query.work=[this] {
            if(work==cutoff)throw std::runtime_error("injected blend provider cutoff");
            ++work;
        };
        query.mainSequence=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("mainSequence");return state.mainSequence;
        };
        query.readMainFloat=[this](std::uintptr_t actor,M field) {
            Identity(actor);
            if(field==M::Frame) {calls.emplace_back("main:Frame");return state.mainFrame;}
            if(field==M::Rate) {calls.emplace_back("main:Rate");return state.mainRate;}
            if(field==M::TweenRate) {calls.emplace_back("main:TweenRate");return state.mainTween;}
            throw std::runtime_error("blend queried an unrelated main field");
        };
        query.sequence=[this](std::uintptr_t actor,unsigned slot) {
            Identity(actor);calls.emplace_back("sequence:"+std::to_string(slot));return state.slots.at(slot).sequence;
        };
        query.readFloat=[this](std::uintptr_t actor,unsigned slot,F field) {
            Identity(actor);calls.emplace_back("read:"+std::to_string(slot)+":"+Label(field));
            return state.slots.at(slot).Get(field);
        };
        query.writeFloat=[this](std::uintptr_t actor,unsigned slot,F field,float value) {
            Identity(actor);calls.emplace_back("write:"+std::to_string(slot)+":"+Label(field));
            state.slots.at(slot).Get(field)=value;if(afterWrite)afterWrite(slot,field);
        };
        query.velocity=[this](std::uintptr_t actor,unsigned axis) {
            Identity(actor);calls.emplace_back("velocity:"+std::to_string(axis));return state.velocity.at(axis);
        };
        query.remoteRole=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("remoteRole");return state.role;
        };
        query.isWeapon=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("isWeapon");return state.weapon;
        };
    }
    void Identity(std::uintptr_t actor) {Require(actor==Actor,"blend provider changed retained Actor identity");}
    void Active(unsigned slot,float frame,float rate,float last=10.0f) {
        auto& selected=state.slots.at(slot);selected.sequence="Attack";
        selected.Get(F::Frame)=frame;selected.Get(F::Rate)=rate;selected.Get(F::Last)=last;
    }
    auto Advance() {return QuestVr::AdvanceOriginalActorBlendAnimation(Actor,main,query);}
    auto Root() {
        const auto before=state;
        try {return Advance();}catch(...) {state=before;throw;}
    }
    std::size_t First(const std::string& name) const {
        return static_cast<std::size_t>(std::find(calls.begin(),calls.end(),name)-calls.begin());
    }
};

void GatesAndCarry() {
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    {
        Fixture f;f.state.mainSequence="nOnE";f.state.mainFrame=nan;f.main.remainingSeconds=nan;
        f.query.readMainFloat={};f.query.sequence={};
        const auto out=f.Advance();
        Require(out.inactive&&out.iterations==0u&&f.calls.size()==1u,
            "None MAIN sequence must gate even invalid/unprovided later storage");
    }
    {
        Fixture f;f.state.mainRate=0.0f;f.main.remainingSeconds=nan;f.query.sequence={};
        const auto out=f.Advance();
        Require(out.inactive&&out.iterations==0u,"zero MAIN rate did not gate before elapsed validation");
        Require(f.First("main:TweenRate")==f.calls.size(),"nonnegative main frame touched main tween");
    }
    {
        Fixture f;f.state.mainFrame=-0.25f;f.state.mainTween=0.0f;f.state.mainRate=nan;
        const auto out=f.Advance();
        Require(out.inactive&&f.First("main:Rate")==f.calls.size(),"negative MAIN frame did not lazily select tween gate");
    }
    for(float elapsed:{0.0f,-0.125f}) {
        Fixture f;f.main.remainingSeconds=elapsed;f.query.sequence={};const auto out=f.Advance();
        Require(out.iterations==0u&&!out.iterationCap&&out.nativeCounter==0u,
            "nonpositive shared elapsed must gate before count increment or slot access");
    }
    for(std::size_t used=0u;used<=4u;++used) {
        Fixture f;f.main.iterations=used;f.Active(0u,0.0f,1.0f);
        const auto out=f.Advance();const auto passes=4u-used;
        Require(out.initialCounter==used&&out.nativeCounter==5u&&out.iterations==passes&&out.iterationCap,
            "blend did not inherit native MAIN counter and increment before testing literal four cap");
        Require(Bits(f.state.slots[0].Get(F::Frame))==Bits(0.125f*static_cast<float>(passes)),
            "ordinary blend scan must reuse unchanged residual on each admitted outer pass");
        Require(Bits(out.remainingSeconds)==Bits(0.125f),"ordinary blend scan incorrectly consumed shared elapsed");
    }
    {
        Fixture f;f.main.iterations=4u;f.main.iterationCap=true;
        const auto out=f.Advance();
        Require(out.initialCounter==5u&&out.nativeCounter==6u&&out.iterations==0u&&out.iterationCap,
            "MAIN cap's already incremented fifth count was lost before blend outer gate");
    }
    {
        Fixture f;f.main.iterations=3u;f.state.mainFrame=-0.25f;f.state.mainTween=1.0f;
        f.Active(0u,0.0f,1.0f);const auto out=f.Advance();
        Require(out.iterations==1u&&f.state.slots[0].Get(F::Frame)==0.125f,
            "active MAIN tween gate failed to admit blend pass");
    }
    {
        Fixture f;f.main.remainingSeconds=nan;Refuses([&]{f.Advance();},"active nonfinite residual was accepted");
    }
    {
        Fixture f;f.state.mainFrame=nan;Refuses([&]{f.Advance();},"active nonfinite MAIN frame was accepted");
    }
}

void SharedBoundaryAndPasses() {
    {
        Fixture f;f.main.remainingSeconds=1.0f;
        f.Active(0u,0.5f,1.0f,1.0f);f.Active(1u,0.0f,1.0f);f.Active(2u,0.0f,2.0f);
        const auto out=f.Advance();
        Require(out.iterations==4u&&out.slotVisits==16u&&out.iterationCap,
            "blend outer passes did not scan all four native slots");
        Require(f.state.slots[0].Get(F::Frame)==1.0f&&f.state.slots[0].Get(F::Rate)==0.0f,
            "finished blend must clamp Frame and zero Rate");
        Require(f.state.slots[1].Get(F::Frame)==2.0f&&f.state.slots[2].Get(F::Frame)==4.0f&&out.remainingSeconds==0.5f,
            "finishing first slot did not carry residual into later slots and following passes");
        Require(f.First("write:0:Frame")<f.First("write:0:Rate")&&f.First("write:0:Rate")<f.First("remoteRole"),
            "native boundary stores must precede Role read and residual publication");
    }
    {
        Fixture f;f.main.iterations=3u;f.main.remainingSeconds=1.0f;
        f.Active(0u,0.0f,1.0f,1.0f);f.Active(1u,0.25f,1.0f);
        const auto out=f.Advance();
        Require(out.iterations==1u&&out.remainingSeconds==0.0f&&!out.iterationCap,
            "exact endpoint must zero shared residual before outer elapsed gate");
        Require(f.First("read:1:Frame")<f.calls.size()&&f.First("write:1:Frame")<f.calls.size(),
            "zero residual from an earlier slot must NOT add an inner per-slot elapsed gate");
        Require(f.state.slots[1].Get(F::Frame)==0.25f,"later zero-delta slot changed frame");
    }
    {
        Fixture f;f.main.iterations=3u;f.main.remainingSeconds=1.0f;
        f.Active(0u,-0.5f,2.0f);f.state.slots[0].Get(F::TweenRate)=1.0f;
        f.Active(1u,0.0f,1.0f);
        const auto out=f.Advance();
        Require(f.state.slots[0].Get(F::Frame)==0.0f&&f.state.slots[0].Get(F::Rate)==2.0f,
            "tween crossing must only clamp frame, not play normal segment in same slot");
        Require(out.remainingSeconds==0.5f&&f.state.slots[1].Get(F::Frame)==0.5f,
            "tween crossing must retain actual PC64 residual for later slots");
        Require(f.First("remoteRole")==f.calls.size(),"tween crossing incorrectly ran completion-role branch");
    }
    {
        Fixture f;f.main.iterations=3u;f.Active(0u,-0.5f,2.0f);f.state.slots[0].Get(F::TweenRate)=1.0f;
        const auto out=f.Advance();
        Require(f.state.slots[0].Get(F::Frame)==-0.375f&&out.remainingSeconds==0.125f,
            "subzero tween must leave shared residual unchanged");
    }
    {
        Fixture f;f.main.remainingSeconds=1.0f;f.Active(0u,-0.5f,2.0f);f.state.slots[0].Get(F::TweenRate)=1.0f;
        const auto out=f.Advance();
        Require(out.iterations==4u&&f.state.slots[0].Get(F::Frame)==3.0f&&out.remainingSeconds==0.5f,
            "crossed tween must resume normal playback only in a later outer pass");
    }
    {
        Fixture f;f.main.iterations=3u;f.Active(0u,0.5f,1.0f);f.Active(1u,0.25f,1.0f);
        f.afterWrite=[&](unsigned slot,F field) {if(slot==0u&&field==F::Frame)f.state.mainRate=0.0f;};
        const auto out=f.Advance();
        Require(out.iterations==1u&&f.state.slots[1].Get(F::Frame)==0.375f&&!out.iterationCap,
            "MAIN gate was incorrectly refreshed inside the current four-slot pass");
    }
}

void VelocityAndSimulation() {
    for(float minimum:{2.0f,7.0f}) {
        Fixture f;f.main.iterations=3u;f.Active(0u,0.0f,-1.0f);
        f.state.velocity={3.0f,4.0f,0.0f};f.state.slots[0].Get(F::MinRate)=minimum;
        f.Advance();
        Require(f.state.slots[0].Get(F::Frame)==0.125f*(minimum==2.0f?5.0f:7.0f),
            "negative blend rate must use max(adjusted speed rate, BlendAnimMinRate), never Last cap");
        Require(f.First("velocity:2")<f.First("velocity:1")&&f.First("velocity:1")<f.First("velocity:0"),
            "native velocity components were not loaded z,y,x");
    }
    {
        Fixture f;f.main.iterations=3u;f.Active(0u,0.0f,1.0f);
        f.query.velocity={};f.state.slots[0].Get(F::MinRate)=std::numeric_limits<float>::quiet_NaN();
        f.Advance();Require(f.state.slots[0].Get(F::Frame)==0.125f,
            "positive-rate blend touched inactive velocity or minimum storage");
    }
    for(unsigned role=0u;role<=3u;++role)for(bool weapon:{false,true}) {
        Fixture f;f.main.iterations=3u;f.main.remainingSeconds=1.0f;f.Active(0u,0.5f,1.0f,1.0f);
        f.state.role=static_cast<std::uint8_t>(role);f.state.weapon=weapon;const auto out=f.Advance();
        const bool publish=role<2u&&!weapon;
        Require(f.state.slots[0].Get(F::SimX)==(publish?10000.0f:101.0f)&&
            f.state.slots[0].Get(F::SimY)==(publish?0.0f:103.0f),
            "original blend terminal Sim X/Y stores or weapon/role gates changed");
        Require(out.roleReads==1u&&out.weaponChecks==(role<2u?1u:0u),
            "weapon query must be lazy after actual RemoteRole<2");
    }
    {
        Fixture f;f.main.iterations=3u;f.main.remainingSeconds=1.0f;f.Active(0u,0.5f,1.0f,1.0f);
        f.state.role=0u;
        f.afterWrite=[&](unsigned slot,F field) {
            if(slot==0u&&field==F::Rate) {
                Require(f.state.slots[0].Get(F::Frame)==1.0f,"Rate completion store preceded Frame clamp");
                f.state.slots[0].Get(F::Rate)=8.0f;
            }
        };
        f.Advance();Require(f.state.slots[0].Get(F::SimY)==32767.0f,
            "terminal Sim rate must read live Rate and cap Float store at32767");
    }
}

void RefusalsAndProviderOrdering() {
    {
        Fixture f;f.main.iterations=3u;f.state.slots[0].values.fill(std::numeric_limits<float>::quiet_NaN());
        f.query.readFloat={};f.query.writeFloat={};f.query.remoteRole={};f.query.isWeapon={};f.query.velocity={};
        const auto out=f.Advance();Require(out.iterations==1u,"None blend slots inspected inactive fields/providers");
    }
    {
        Fixture f;f.main.iterations=3u;f.Active(0u,10.0f,1.0f,10.0f);
        f.state.slots[0].Get(F::Rate)=std::numeric_limits<float>::quiet_NaN();f.query.writeFloat={};
        f.Advance();Require(f.First("read:0:Rate")==f.calls.size(),"already terminal blend read inactive Rate");
    }
    {
        Fixture f;f.main.iterations=3u;f.Active(0u,0.0f,std::numeric_limits<float>::quiet_NaN());
        Refuses([&]{f.Advance();},"active nonfinite blend Rate accepted");
    }
    {
        Fixture f;f.main.iterations=3u;f.Active(0u,0.0f,-1.0f);f.query.velocity={};
        Refuses([&]{f.Advance();},"reached absent velocity provider accepted");
    }
    {
        Fixture f;f.main.iterations=3u;f.main.remainingSeconds=1.0f;f.Active(0u,0.5f,1.0f,1.0f);f.query.remoteRole={};
        Refuses([&]{f.Advance();},"reached absent Role provider accepted");
        Require(f.state.slots[0].Get(F::Frame)==1.0f&&f.state.slots[0].Get(F::Rate)==0.0f,
            "native store prefix was deferred beyond the reached Role provider failure");
    }
    {
        Fixture f;f.main.iterations=3u;f.Active(0u,0.0f,1.0f);f.query.writeFloat={};
        const auto before=f.state;
        Refuses([&]{f.Root();},"reached absent native store provider accepted");
        Require(f.state==before,"fixture caller journal did not restore native store failure");
    }
    const auto configure=[](Fixture& f) {
        f.main.iterations=3u;f.main.remainingSeconds=1.0f;f.state.role=0u;
        f.Active(0u,0.5f,1.0f,1.0f);f.Active(1u,-0.25f,1.0f);
        f.state.slots[1].Get(F::TweenRate)=1.0f;
        f.Active(2u,0.0f,-1.0f);f.state.velocity={3.0f,4.0f,0.0f};
    };
    Fixture baseline;configure(baseline);baseline.Advance();
    Require(baseline.work>30u,"provider-cutoff control did not reach the full native phase");
    for(std::size_t cut=0u;cut<baseline.work;++cut) {
        Fixture f;configure(f);const auto before=f.state;f.cutoff=cut;
        Refuses([&]{f.Root();},"native provider budget cutoff was silently bypassed");
        Require(f.state==before,"fixture root journal failed to restore partial blend field stores");
        Require(f.calls.size()<=baseline.calls.size()&&
            std::equal(f.calls.begin(),f.calls.end(),baseline.calls.begin()),
            "provider cutoff changed native read/store prefix ordering");
    }
}
}

int main() {
    try {
        GatesAndCarry();SharedBoundaryAndPasses();VelocityAndSimulation();RefusalsAndProviderOrdering();
        std::cout<<"PASS original Actor blend animation phase: "<<checks<<" checks, "<<refusals<<" refusals\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"FAIL original Actor blend animation phase: "<<error.what()<<'\n';return 1;
    }
}
