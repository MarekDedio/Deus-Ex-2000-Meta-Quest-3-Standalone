#include "quest_native_window_activation.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks{},refusals{};
void Require(bool condition,const char* message) {
    ++checks;
    if(!condition) throw std::runtime_error(message);
}
std::string Bit(bool value) {return value?"1":"0";}

// Explicit policy mock: the coordinator supplies ordering, not an engine tree,
// layout algorithm, journal, WindowReady delivery or rollback implementation.
struct State {
    bool local{},parentVisible{true},focused{true},grabbed{true},needsReconfigure{};
    int initDelta{};
    unsigned layouts{},modalDirties{};
    std::vector<bool> initArguments,visibilityArguments,layoutVisibility;
    bool operator==(const State&) const=default;
};
struct Operations {
    State state;
    std::vector<std::string> events;
    std::function<void(Operations&,const std::string&)> callback;
    unsigned calls{},failAt{};
    bool Effective() const {return state.local&&state.parentVisible;}
    void Visit(const std::string& event) {
        events.push_back(event);
        if(++calls==failAt) throw std::runtime_error("Injected provider failure: "+event);
    }
    void Callback(const std::string& event) {
        if(callback) callback(*this,event);
    }
    bool ReadLocalVisibility() {
        const bool result=state.local;Visit("local?"+Bit(result));return result;
    }
    bool ReadEffectiveVisibility() {
        const bool result=Effective();Visit("effective?"+Bit(result));return result;
    }
    void WriteLocalVisibility(bool value) {
        Visit("local="+Bit(value));state.local=value;Callback("store");
    }
    void CheckFocus() {
        Visit("focus");if(!Effective()) state.focused=false;Callback("focus");
    }
    void CheckGrab() {
        Visit("grab");if(!Effective()) state.grabbed=false;Callback("grab");
    }
    void MarkNeedsReconfigure() {
        Visit("dirty-layout");state.needsReconfigure=true;Callback("dirty-layout");
    }
    void RecomputeInitTree(bool value) {
        Visit("init="+Bit(value));state.initArguments.push_back(value);
        state.initDelta+=value?1:-1;Callback("init");
    }
    void InvokeVisibilityTree(bool value) {
        Visit("visibility="+Bit(value));state.visibilityArguments.push_back(value);
        Callback("visibility");
    }
    void ReconfigureTree() {
        Visit("layout");++state.layouts;state.layoutVisibility.push_back(Effective());
        Callback("layout");
    }
    void DirtyModalAccelerators() {
        Visit("modal");++state.modalDirties;Callback("modal");
    }
};

std::vector<std::string> FullEvents(bool visible) {
    return {"local?"+Bit(!visible),"effective?"+Bit(!visible),"local="+Bit(visible),
        "effective?"+Bit(visible),"focus","grab","dirty-layout","init="+Bit(visible),
        "visibility="+Bit(visible),"layout","modal"};
}

void EqualityAndHiddenAncestor() {
    for(bool local:{false,true}) for(bool parent:{false,true}) {
        Operations operations;operations.state.local=local;operations.state.parentVisible=parent;
        const State before=operations.state;
        QuestVr::SetNativeWindowChildVisibility(operations,local);
        Require(operations.state==before,"Equal local visibility changed provider state");
        Require(operations.events==std::vector<std::string>{"local?"+Bit(local)},
            "Equal local visibility read ancestors or invoked downstream operations");
    }
    for(bool value:{false,true}) {
        Operations operations;operations.state.local=!value;operations.state.parentVisible=false;
        State expected=operations.state;expected.local=value;
        QuestVr::SetNativeWindowChildVisibility(operations,value);
        Require(operations.state==expected,"Hidden-ancestor change invoked activation side effects");
        Require(operations.events==std::vector<std::string>{"local?"+Bit(!value),
            "effective?0","local="+Bit(value),"effective?0"},
            "Hidden-ancestor change did not compare effective state around its local store");
    }
}

void ShowHideOrder() {
    for(bool value:{false,true}) {
        Operations operations;operations.state.local=!value;
        QuestVr::SetNativeWindowChildVisibility(operations,value);
        Require(operations.events==FullEvents(value),"Activation operation order changed");
        Require(operations.state.local==value && operations.state.needsReconfigure,
            "Activation did not store visibility before requesting configuration");
        Require(operations.state.initArguments==std::vector<bool>{value} &&
            operations.state.visibilityArguments==std::vector<bool>{value},
            "Activation did not pass changed effective visibility to both tree providers");
        Require(operations.state.initDelta==(value?1:-1),"Init provider was omitted or repeated");
        Require(operations.state.layouts==1u && operations.state.modalDirties==1u &&
            operations.events.back()=="modal","Modal dirtiness was not the final operation");
        Require(operations.state.focused==value && operations.state.grabbed==value,
            "Focus/grab providers did not observe the stored local visibility");
    }
}

void FailureCutoffs() {
    for(bool value:{false,true}) {
        const auto all=FullEvents(value);
        for(unsigned failure=1u;failure<=static_cast<unsigned>(all.size());++failure) {
            Operations operations;operations.state.local=!value;operations.failAt=failure;
            bool threw{};
            try {QuestVr::SetNativeWindowChildVisibility(operations,value);}
            catch(const std::runtime_error& error) {
                threw=std::string(error.what()).starts_with("Injected provider failure:");
                ++refusals;
            }
            Require(threw,"Activation swallowed or replaced a provider failure");
            const std::vector<std::string> prefix(all.begin(),all.begin()+failure);
            Require(operations.events==prefix && operations.calls==failure,
                "Activation executed operations after a provider failure");
            Require(operations.state.local==(failure>3u?value:!value),
                "Coordinator silently rolled back a local store or stored after failure");
            Require(operations.state.modalDirties==0u,
                "Failed activation reached modal dirtiness or suppressed its failure");
        }
    }
    // A callback may itself write and then fail. This helper must propagate the
    // failure; restoring those writes belongs to the caller's real transaction.
    Operations operations;
    operations.callback=[](Operations& target,const std::string& event) {
        if(event=="visibility") {
            target.state.parentVisible=false;
            throw std::runtime_error("Late callback failure");
        }
    };
    bool threw{};
    try {QuestVr::SetNativeWindowChildVisibility(operations,true);}
    catch(const std::runtime_error& error) {threw=std::string(error.what())=="Late callback failure";++refusals;}
    Require(threw && operations.state.local && !operations.state.parentVisible,
        "Late callback failure was swallowed or mock writes were implicitly restored");
    Require(operations.state.layouts==0u && operations.state.modalDirties==0u,
        "Later layout/modal providers ran after a visibility callback failure");
}

void CapturedEffectiveAndLiveProviders() {
    for(bool value:{false,true}) for(const std::string stage:{"focus","grab","init","visibility"}) {
        Operations operations;operations.state.local=!value;
        operations.callback=[&](Operations& target,const std::string& event) {
            if(event==stage) {
                // An inner script/owner may change the tree after the outer
                // setter captured its effective transition. Do not recapture.
                target.state.local=!value;target.state.parentVisible=true;
            }
        };
        QuestVr::SetNativeWindowChildVisibility(operations,value);
        Require(operations.state.initArguments==std::vector<bool>{value} &&
            operations.state.visibilityArguments==std::vector<bool>{value},
            "Activation recaptured effective visibility after a provider callback");
        Require(operations.state.local==!value &&
            operations.state.layoutVisibility==std::vector<bool>{!value},
            "Activation overwrote callback state or froze the layout provider's live state");
        Require(operations.events==FullEvents(value),"Callback mutation changed coordinator operation order");
    }
    Operations operations;operations.state.local=true;
    operations.callback=[](Operations& target,const std::string& event) {
        if(event=="focus") {target.state.local=true;target.state.parentVisible=true;}
        if(event=="layout") {
            Require(target.state.modalDirties==0u,"Modal provider ran before layout callback finished");
        }
    };
    QuestVr::SetNativeWindowChildVisibility(operations,false);
    Require(!operations.state.focused && operations.state.grabbed,
        "Grab did not run after focus or did not observe focus callback's live state");
    Require(operations.state.visibilityArguments==std::vector<bool>{false},
        "Focus callback replaced the outer captured hide argument");
}

void ReentrantVisibilityChange() {
    Operations operations;
    bool entered{};
    operations.callback=[&](Operations& target,const std::string& event) {
        if(event=="visibility" && !entered) {
            entered=true;
            target.events.push_back("inner-begin");
            QuestVr::SetNativeWindowChildVisibility(target,false);
            target.events.push_back("inner-end");
        }
    };
    QuestVr::SetNativeWindowChildVisibility(operations,true);
    auto expected=FullEvents(true);
    expected.resize(9u);
    expected.push_back("inner-begin");
    const auto inner=FullEvents(false);expected.insert(expected.end(),inner.begin(),inner.end());
    expected.insert(expected.end(),{"inner-end","layout","modal"});
    Require(operations.events==expected,"Reentrant activation reordered outer or inner operation sequence");
    Require(operations.state.initArguments==std::vector<bool>{true,false} &&
        operations.state.visibilityArguments==std::vector<bool>{true,false},
        "Reentrant activation did not preserve each invocation's captured effective value");
    Require(!operations.state.local && operations.state.initDelta==0 && operations.state.layouts==2u &&
        operations.state.modalDirties==2u,"Outer setter overwrote the inner visibility change");
    Require(operations.state.layoutVisibility==std::vector<bool>{false,false},
        "Outer layout did not observe the live reentrant hide");
}
} // namespace

int main() {
    try {
        EqualityAndHiddenAncestor();ShowHideOrder();FailureCutoffs();
        CapturedEffectiveAndLiveProviders();ReentrantVisibilityChange();
        std::cout << "Native Window activation coordinator tests passed: " << checks
            << " checks, " << refusals << " refusals\n";
        return 0;
    } catch(const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
}
