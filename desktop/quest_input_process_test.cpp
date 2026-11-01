#include "quest_input_process.h"

#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <tuple>
#include <vector>

namespace {
std::size_t checks{},refusals{};
void Check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class Run> void Refuse(Run&& run,const char* expected="") {
    bool refused{};try{run();}catch(const std::exception& error){refused=std::string(error.what()).find(expected)!=std::string::npos;}
    Check(refused,"Expected selected original input refusal");++refusals;
}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
struct Harness {
    std::array<std::uint8_t,255u> keys{},down{};
    std::array<bool,255u> bindings{};
    bool root{},rootConsumes{},parent{};std::int32_t mouse{},keyboard{},action{};float delta{};
    std::size_t frames{},peakFrames{},work{},maximumWork{100000u},maximumFrames{100000u},rootCalls{},fire{},alt{};
    std::vector<std::tuple<std::int32_t,std::int32_t,std::uint32_t>> commands;
    std::vector<std::tuple<std::int32_t,std::int32_t,bool>> handlers;
    std::vector<std::string> events;
    std::function<void()> commandHook,rootHook,fireHook,altHook,workHook;
    void Step(){if(workHook)workHook();if(work++>=maximumWork)throw std::runtime_error("shared work exhausted");}
    void Enter(std::size_t bytes){if(bytes>maximumFrames-frames)throw std::runtime_error("frame admission exhausted");frames+=bytes;peakFrames=std::max(peakFrames,frames);}
    void Leave(std::size_t bytes)noexcept{if(frames<bytes)std::terminate();frames-=bytes;}
    QuestVr::OriginalInputProcessQueries Input() {
        return {[this]{Step();},[this](std::size_t b){Enter(b);},[this](std::size_t b){Leave(b);},
            [this]{events.emplace_back("root?");return root;},
            [this](std::int32_t key,std::int32_t a,float d){++rootCalls;events.emplace_back("root");if(rootHook)rootHook();
                Check(key>=0&&key<255,"Root received a different key");static_cast<void>(a);static_cast<void>(d);return rootConsumes;},
            [this](std::int32_t key){events.emplace_back("key"+std::to_string(key));return keys.at(static_cast<std::size_t>(key));},
            [this](std::int32_t key,std::uint8_t v){events.emplace_back("write"+std::to_string(key));keys.at(static_cast<std::size_t>(key))=v;},
            [this](std::int32_t key){events.emplace_back("binding"+std::to_string(key));return bindings.at(static_cast<std::size_t>(key));},
            [this](std::int32_t value){events.emplace_back("action"+std::to_string(value));action=value;},
            [this](float value){events.emplace_back("delta");delta=value;},
            [this](std::int32_t key){events.emplace_back("exec"+std::to_string(key));commands.emplace_back(key,action,Bits(delta));if(commandHook)commandHook();}};
    }
    QuestVr::OriginalRootInputQueries Root() {
        return {[this]{Step();},[this](std::size_t b){Enter(b);},[this](std::size_t b){Leave(b);},
            [this](std::int32_t key){return down.at(static_cast<std::size_t>(key));},
            [this](std::int32_t key,std::uint8_t value){events.emplace_back("down");down.at(static_cast<std::size_t>(key))=value;},
            [this]{events.emplace_back("mouse?");return mouse;},[this]{events.emplace_back("keyboard?");return keyboard;},
            [this]{events.emplace_back("parent?");return parent;},
            [this]{events.emplace_back("fire");++fire;if(fireHook)fireHook();},
            [this]{events.emplace_back("alt");++alt;if(altHook)altHook();},
            [this](std::int32_t key,std::int32_t a){events.emplace_back("buttons");handlers.emplace_back(key,a,false);},
            [this](std::int32_t key,std::int32_t a,bool repeat){events.emplace_back("virtual");handlers.emplace_back(key,a,repeat);}};
    }
    bool Process(std::int32_t key,std::int32_t a,float d=1.0f){return QuestVr::ProcessOriginalInputExt(key,a,d,Input());}
    bool ProcessRoot(std::int32_t key,std::int32_t a,float d=1.0f){return QuestVr::ProcessOriginalRootInput(key,a,d,Root());}
};
void InputStates() {
    for(std::int32_t key=0;key<255;++key) {
        Harness h;h.bindings[static_cast<std::size_t>(key)]=true;
        Check(h.Process(key,1,-0.0f)&&h.keys[static_cast<std::size_t>(key)]==1u,"Press did not set original held byte");
        Check(h.commands==std::vector<std::tuple<std::int32_t,std::int32_t,std::uint32_t>>{{key,1,0x80000000u}}&&
            h.action==0&&Bits(h.delta)==0u&&h.frames==0u,"Press raw Delta or normal return clear differs");
        const auto size=h.commands.size();Check(!h.Process(key,1)&&h.commands.size()==size,"Duplicate press dispatched binding");
        Check(h.Process(key,3,2.0f)&&!h.keys[static_cast<std::size_t>(key)]&&h.commands.size()==size+1u,"Release did not clear held byte/dispatch");
        Check(!h.Process(key,3)&&h.commands.size()==size+1u,"Duplicate release dispatched binding");
        Check(h.Process(key,2,3.0f)&&!h.keys[static_cast<std::size_t>(key)],"Hold event fabricated held-state change");
    }
    Harness empty;Check(!empty.Process(4,1)&&empty.keys[4]==1u&&empty.commands.empty(),"Empty binding failed to retain press prefix");
    Check(!empty.Process(4,3)&&empty.keys[4]==0u,"Empty binding failed to retain release prefix");
    for(const auto bits:{0x7f800000u,0xff800000u,0x7fc12345u,0xffc54321u}) {
        Harness raw;raw.bindings[17]=true;const auto value=std::bit_cast<float>(bits);
        Check(raw.Process(17,4,value)&&std::get<2>(raw.commands.at(0))==bits,"Process changed inactive/nonfinite raw Delta transport");
    }
    Harness absent;auto q=absent.Input();q.rootProcess={};Check(!QuestVr::ProcessOriginalInputExt(7,0,0.0f,q),"Unselected missing Root service failed legitimate empty binding");
}
void RootConsumption() {
    Harness h;h.root=h.rootConsumes=true;
    for(const auto key:{0u,3u,60u,254u}){h.keys[key]=static_cast<std::uint8_t>(key==60u?2u:1u);h.bindings[key]=key!=3u;}
    h.action=9;h.delta=-0.0f;
    Check(h.Process(60,1,42.0f)&&h.rootCalls==1u,"Root-consumed event did not precede input state handling");
    Check(h.commands==std::vector<std::tuple<std::int32_t,std::int32_t,std::uint32_t>>{{0,3,0},{60,3,0},{254,3,0}},
        "Consumed root did not release nonempty held bindings ascending with Action3/Delta+0");
    Check(std::all_of(h.keys.begin(),h.keys.end(),[](auto key){return key==0u;})&&h.action==0&&Bits(h.delta)==0u,
        "Root consumption retained held state/accumulators");
    Harness noHeld;noHeld.root=noHeld.rootConsumes=true;noHeld.action=7;noHeld.delta=2;
    Check(noHeld.Process(1,1)&&noHeld.commands.empty()&&noHeld.action==7&&noHeld.delta==2,"Root empty-held path invented accumulator stores");
    Harness unconsumed;unconsumed.root=true;unconsumed.bindings[22]=true;
    Check(unconsumed.Process(22,1)&&unconsumed.keys[22]==1u&&unconsumed.rootCalls==1u,"Root decline lost ordinary native binding path");
    Check(unconsumed.events[0]=="root?"&&unconsumed.events[1]=="root"&&unconsumed.events[2]=="key22","Root offered after original held-key test");
}
void RootStates() {
    for(std::int32_t key=0;key<255;++key) {
        Harness h;const bool mouse=key==1||key==2||key==4||(key>=228&&key<=231)||key==236||key==237;
        Check(!h.ProcessRoot(key,1)&&h.down[static_cast<std::size_t>(key)]==1u&&h.handlers.empty(),"Zero grab references skipped root-map store or invented handling");
        Check(h.events.back()==(mouse?"mouse?":"keyboard?"),"Root selected wrong actual mouse key set");
        if(mouse){h.mouse=1;h.parent=true;}else h.keyboard=1;
        Check(h.ProcessRoot(key,1)&&h.handlers.size()==1u,"Grabbed duplicate press was wrongly discarded");
        Check(std::get<2>(h.handlers[0])==!mouse,"Keyboard duplicate-press argument differs");
        Check(h.ProcessRoot(key,3)&&!h.down[static_cast<std::size_t>(key)]&&h.handlers.size()==2u,"Grabbed release omitted handler");
        Check(h.ProcessRoot(key,3)&&h.handlers.size()==2u,"Duplicate release did not consume while skipping handler");
        Check(mouse?(h.fire==3u&&h.alt==3u):(h.fire==0u&&h.alt==0u),"Mouse parent Fire/Alt clearing order/count differs");
    }
    Harness mouse;mouse.mouse=-1;mouse.parent=true;Check(mouse.ProcessRoot(1,2)&&mouse.handlers.size()==1u,"Nonzero negative mouse ref was invented as inactive");
    Check(mouse.events==std::vector<std::string>({"mouse?","parent?","fire","alt","buttons"}),"Mouse clear/handler ordering differs");
    Harness parent;parent.mouse=1;parent.parent=true;parent.fireHook=[&]{parent.parent=false;};
    parent.altHook=[&]{Check(!parent.parent,"Alt callback did not reread state after Fire store");};
    Check(parent.ProcessRoot(1,1)&&parent.alt==1u,"Mouse parent callback snapshot substituted for live reread");
    Harness noParent;noParent.mouse=1;auto q=noParent.Root();q.clearParentFire=q.clearParentAltFire={};
    Check(QuestVr::ProcessOriginalRootInput(1,1,0.0f,q)&&noParent.fire==0u&&noParent.handlers.size()==1u,"Null parent branch demanded unselected writes");
    Harness zero;auto zeroQ=zero.Root();zeroQ.hasParentPawn={};zeroQ.clearParentFire={};zeroQ.clearParentAltFire={};zeroQ.handleButtons={};
    Check(!QuestVr::ProcessOriginalRootInput(1,1,0.0f,zeroQ)&&zero.down[1]==1u,"Zero mouse ref demanded unselected native handler");
}
void PartialFailures() {
    Harness command;command.bindings[5]=true;command.commandHook=[](){throw std::runtime_error("exec fault");};
    Refuse([&]{command.Process(5,1,-0.0f);},"exec fault");
    Check(command.keys[5]==1u&&command.action==1&&Bits(command.delta)==0x80000000u&&command.frames==0u,
        "Exec failure discarded original held/Action/Delta prefix or leaked frame");
    Harness consumed;consumed.root=consumed.rootConsumes=true;consumed.keys[2]=consumed.keys[4]=1u;consumed.bindings[2]=consumed.bindings[4]=true;
    consumed.commandHook=[](){throw std::runtime_error("release fault");};Refuse([&]{consumed.Process(8,1);},"release fault");
    Check(!consumed.keys[2]&&consumed.keys[4]&&consumed.action==3&&Bits(consumed.delta)==0u&&consumed.frames==0u,
        "Consumed-root release failure overran or repaired ascending prefix");
    Harness root;root.root=true;auto missing=root.Input();missing.rootProcess={};Refuse([&]{QuestVr::ProcessOriginalInputExt(9,1,0.0f,missing);},"missing");
    Check(!root.keys[9]&&root.frames==0u,"Missing Root service changed ordinary held state");
    Harness exec;exec.bindings[6]=true;auto noExec=exec.Input();noExec.execBinding={};
    Refuse([&]{QuestVr::ProcessOriginalInputExt(6,1,-0.0f,noExec);},"missing");
    Check(exec.keys[6]==1u&&exec.action==1&&Bits(exec.delta)==0x80000000u&&exec.frames==0u,
        "Missing selected Exec service falsely declined or erased original prefix");
    Harness handler;handler.keyboard=1;auto q=handler.Root();q.handleVirtualKeyboard={};
    Refuse([&]{QuestVr::ProcessOriginalRootInput(15,1,0.0f,q);},"missing");
    Check(handler.down[15]==1u&&handler.frames==0u,"Missing selected native handler repaired root-map prefix");
    Harness fire;fire.mouse=1;fire.parent=true;fire.fireHook=[](){throw std::runtime_error("fire fault");};
    Refuse([&]{fire.ProcessRoot(1,1);},"fire fault");Check(fire.down[1]==1u&&fire.fire==1u&&fire.alt==0u&&fire.handlers.empty()&&fire.frames==0u,
        "Parent Fire fault overran original prefix");
    Harness work;work.maximumWork=0u;Refuse([&]{work.Process(1,1);},"work");Check(!work.keys[1]&&work.frames==0u,"Work refusal leaked frame or wrote state");
    Harness frame;frame.maximumFrames=0;Refuse([&]{frame.Process(1,1);},"frame");Check(frame.frames==0u&&!frame.keys[1],"Frame refusal released an unowned frame");
    for(const auto key:{-1,255,std::numeric_limits<std::int32_t>::max()}) {
        Harness invalid;Refuse([&]{invalid.Process(key,1);},"255-key");Refuse([&]{invalid.ProcessRoot(key,1);},"255-key");Check(invalid.frames==0u&&invalid.work==0u,"Bad key entered native services");
    }
    Harness missingFrame;auto noFrame=missingFrame.Input();noFrame.leaveFrame={};
    Refuse([&]{QuestVr::ProcessOriginalInputExt(0,1,0.0f,noFrame);},"frame");Check(missingFrame.frames==0u,"Missing frame callback acquired scratch");
}
void NestedAndSelectedReads() {
    Harness h;h.root=true;h.bindings[10]=true;
    auto input=h.Input();input.rootProcess=[&](std::int32_t key,std::int32_t action,float delta){return QuestVr::ProcessOriginalRootInput(key,action,delta,h.Root());};
    Check(QuestVr::ProcessOriginalInputExt(10,1,0.0f,input)&&h.keys[10]&&h.down[10]&&h.peakFrames==512u&&h.frames==0u,
        "Actual nested coordinator frame lifetime or zero-grab root decline differs");
    Harness budget;budget.root=true;budget.maximumFrames=256u;auto limited=budget.Input();
    limited.rootProcess=[&](std::int32_t key,std::int32_t action,float delta){return QuestVr::ProcessOriginalRootInput(key,action,delta,budget.Root());};
    Refuse([&]{QuestVr::ProcessOriginalInputExt(10,1,0.0f,limited);},"frame");
    Check(budget.frames==0u&&!budget.keys[10]&&!budget.down[10],"Nested frame refusal released caller-owned frame or wrote native state");
    Harness live;live.root=true;live.keys[10]=1;live.rootHook=[&]{live.keys[10]=0;};live.bindings[10]=true;
    Check(live.Process(10,1)&&live.commands.size()==1u,"Held key was captured before Root callback");
    Harness changed;changed.root=changed.rootConsumes=true;changed.keys[1]=changed.keys[2]=1;changed.bindings[1]=true;
    changed.commandHook=[&]{changed.bindings[2]=true;};Check(changed.Process(9,1)&&changed.commands.size()==2u,"Release loop cached later binding state before command callback");
}
} // namespace
int main(){try{
    InputStates();RootConsumption();RootStates();PartialFailures();NestedAndSelectedReads();
    std::cout<<"PASS original InputExt/Root Process checks="<<checks<<" refusals="<<refusals
        <<"; generated selected coordinator order/partial writes only, NOT runtime/native owners/Quest mapping/commands/startup/headset\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL original Input Process: "<<error.what()<<'\n';return 1;}}
