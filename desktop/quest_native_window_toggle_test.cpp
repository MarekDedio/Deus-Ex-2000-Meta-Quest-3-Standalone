#include "quest_native_window_toggle.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks{},refusals{};
void Require(bool condition,const char* message) {
    ++checks;if(!condition) throw std::runtime_error(message);
}
using Window=std::size_t;
struct Node {
    std::uint32_t flags{0xacu}; // Unrelated packed flags must survive.
    Window parent{},selection{};
    bool toggle{},radio{},oneCheck{};
    std::vector<Window> registered;
    std::array<unsigned,3u> appearance{}; // text, texture, tile.
    bool operator==(const Node&) const=default;
};
struct State {
    std::array<Node,9u> nodes;
    unsigned revision{},rootValue{};
    std::uint32_t random{0x12345678u};
    bool operator==(const State&) const=default;
};
struct Operations {
    using Window=::Window;
    State state;
    std::vector<std::string> events;
    std::function<bool(Operations&,Window,Window,bool)> callback;
    std::size_t work{},maximumWork{1'000'000u},writes{},maximumWrites{1'000'000u};
    unsigned depth{},peakDepth{},maximumDepth{128u};
    void Step() {
        if(work>=maximumWork) throw std::runtime_error("Shared native work limit");
        ++work;
    }
    void EnterToggle() {
        if(depth>=maximumDepth) throw std::runtime_error("Shared native toggle depth limit");
        ++depth;peakDepth=std::max(peakDepth,depth);
    }
    void LeaveToggle() noexcept {--depth;}
    Node& At(Window window) {
        if(!window || window>=state.nodes.size()) throw std::runtime_error("Invalid live Window");
        return state.nodes[window];
    }
    void Write() {
        if(writes>=maximumWrites) throw std::runtime_error("Shared root write limit");
        ++writes;++state.revision;
    }
    bool ReadPressed(Window window) {return (At(window).flags&1u)!=0u;}
    void WritePressed(Window window,bool value) {
        Write();auto& flags=At(window).flags;flags=(flags&~1u)|(value?1u:0u);
        events.push_back("pressed"+std::to_string(window)+"="+(value?"1":"0"));
    }
    void ChangeButtonAppearance(Window window) {
        static constexpr std::array<const char*,3u> fields{"text","texture","tile"};
        for(unsigned index=0u;index<3u;++index) {
            Step();Write();At(window).appearance[index]=(ReadPressed(window)?100u:200u)+index;
            events.push_back("appearance"+std::to_string(window)+":"+fields[index]);
        }
    }
    Window ReadParent(Window window) {return At(window).parent;}
    bool IsRadioBox(Window window) {return At(window).radio;}
    bool IsToggle(Window window) {return window!=0u && At(window).toggle;}
    std::size_t ToggleButtonCount(Window window) {return At(window).registered.size();}
    Window ToggleButtonAt(Window window,std::size_t index) {return At(window).registered.at(index);}
    Window ReadSelection(Window window) {return At(window).selection;}
    void WriteSelection(Window window,Window selection) {
        Write();At(window).selection=selection;
        events.push_back("selection"+std::to_string(window)+"="+std::to_string(selection));
    }
    bool ReadOneCheck(Window window) {return At(window).oneCheck;}
    bool ScriptToggleChanged(Window receiver,Window source,bool value) {
        events.push_back("event"+std::to_string(receiver)+":"+std::to_string(source)+"="+(value?"1":"0"));
        return callback ? callback(*this,receiver,source,value) : false;
    }
};
Operations Toggle(Window source=1u) {
    Operations operations;operations.At(source).toggle=true;return operations;
}
Operations Radio() {
    auto operations=Toggle();operations.At(2u).toggle=true;
    operations.At(1u).parent=3u;operations.At(2u).parent=3u;
    auto& radio=operations.At(3u);radio.radio=true;radio.registered={1u,2u};
    return operations;
}
std::vector<std::string> Events(const Operations& operations) {
    std::vector<std::string> result;
    for(const auto& event:operations.events) if(event.starts_with("event")) result.push_back(event);
    return result;
}
std::size_t Position(const Operations& operations,const std::string& event) {
    const auto found=std::find(operations.events.begin(),operations.events.end(),event);
    Require(found!=operations.events.end(),"Expected event was not observed");
    return static_cast<std::size_t>(found-operations.events.begin());
}
// This fixture is an enclosing-root transaction, NOT rollback supplied by the
// coordinator. Actual RuntimeObject/VM journal coverage belongs to integration
// tests. Diagnostic events/work remain observable after failure; native/root
// graph, appearance, selection, registered list and random state are restored.
template<class Function>
void RefusesAndRollsBack(Operations& operations,Function&& function,const char* message) {
    const State before=operations.state;bool refused{};
    try {function();} catch(const std::runtime_error&) {
        refused=true;operations.state=before;++refusals;
    }
    Require(refused,message);
    Require(operations.state==before,"Mock root failure did not restore all state");
    Require(operations.depth==0u,"Native toggle depth leaked through failure");
}
} // namespace

int main() {
    try {
        for(bool value:{false,true}) {
            auto operations=Toggle();operations.At(1u).flags|=value?1u:0u;
            const auto before=operations.state;
            QuestVr::SetNativeWindowToggle(operations,1u,value);
            Require(operations.state==before && operations.writes==0u && operations.events.empty(),
                "Equal Bool setter was not a side-effect-free no-op");
            Require(operations.depth==0u && operations.peakDepth==1u,
                "Equal setter did not balance its native depth scope");
        }
        auto operations=Toggle();operations.At(1u).parent=2u;
        QuestVr::SetNativeWindowToggle(operations,1u,true);
        Require(operations.At(1u).flags==0xadu,"Pressed setter overwrote unrelated packed flags");
        Require(operations.events==std::vector<std::string>{"pressed1=1","appearance1:text","appearance1:texture",
            "appearance1:tile","event1:1=1","event2:1=1"},
            "Pressed/appearance/Self/parent order differs from original");
        Require(operations.At(1u).appearance==std::array<unsigned,3u>{100u,101u,102u},"Pressed appearance not refreshed");
        QuestVr::SetNativeWindowToggle(operations,1u,false);
        Require(operations.At(1u).flags==0xacu && operations.At(1u).appearance==std::array<unsigned,3u>{200u,201u,202u},
            "Unpress did not preserve unrelated flags and refresh appearance");

        operations=Toggle();QuestVr::SetNativeWindowToggle(operations,1u,std::int32_t{2});
        Require(operations.At(1u).flags==0xacu && operations.writes==4u &&
            Events(operations)==std::vector<std::string>{"event1:1=0"},
            "Noncanonical incoming2 was normalized before equality or did not mask bit0 on write");
        operations=Toggle();operations.At(1u).flags|=1u;
        QuestVr::SetNativeWindowToggle(operations,1u,std::int32_t{-1});
        Require(operations.At(1u).flags==0xadu && operations.writes==4u &&
            Events(operations)==std::vector<std::string>{"event1:1=1"},
            "Noncanonical incoming-1 was normalized before equality or overwrote sibling bits");

        // Original HUDObjectSlot rejects selecting an empty slot by recursively
        // unpressing itself; both callbacks stay in one root/work counter.
        operations=Toggle();operations.At(1u).parent=2u;
        operations.callback=[](Operations& target,Window receiver,Window,bool value) {
            if(receiver==1u && value) QuestVr::SetNativeWindowToggle(target,1u,false);
            return receiver==1u;
        };
        QuestVr::SetNativeWindowToggle(operations,1u,true);
        Require(!operations.ReadPressed(1u) && operations.peakDepth==2u && operations.depth==0u,
            "Empty selection did not reentrantly reject the new pressed state");
        Require(Events(operations)==std::vector<std::string>{"event1:1=1","event1:1=0"},
            "Empty-slot handled callback propagated to a parent");
        Require(Position(operations,"appearance1:tile")<Position(operations,"event1:1=1"),
            "Empty-slot callback ran before changed appearance");

        // An unhandled callback can re-toggle the source and reparent it. The
        // outer notification must not retain the original true or old parent.
        operations=Toggle();operations.At(1u).parent=2u;
        operations.callback=[](Operations& target,Window receiver,Window,bool value) {
            if(receiver==1u && value) {
                QuestVr::SetNativeWindowToggle(target,1u,false);
                target.Write();target.At(1u).parent=3u;
            }
            return false;
        };
        QuestVr::SetNativeWindowToggle(operations,1u,true);
        Require(Events(operations)==std::vector<std::string>{"event1:1=1","event1:1=0","event2:1=0","event3:1=0"},
            "Notifications froze pressed state or source parent across callback");
        Require(!operations.ReadPressed(1u) && operations.At(1u).parent==3u,"Reentrant source state was overwritten on return");

        // The current receiver's parent, not the source's parent, is reread.
        // Lawful callback reparenting may revisit an earlier receiver without
        // any frozen ancestor/visited list being an authority to refuse it.
        operations=Toggle();operations.At(1u).parent=2u;operations.At(2u).parent=3u;
        unsigned firstVisits{};
        operations.callback=[&](Operations& target,Window receiver,Window,bool) {
            if(receiver==2u && firstVisits++==0u) {target.Write();target.At(2u).parent=4u;}
            else if(receiver==4u) {target.Write();target.At(4u).parent=3u;}
            else if(receiver==3u) {
                target.Write();target.At(2u).parent=0u;
                target.Write();target.At(3u).parent=2u;
            }
            return false;
        };
        QuestVr::SetNativeWindowToggle(operations,1u,true);
        Require(Events(operations)==std::vector<std::string>{"event1:1=1","event2:1=1","event4:1=1","event3:1=1","event2:1=1"},
            "Live receiver-parent reparent/revisit was frozen or prematurely refused");
        operations=Toggle();operations.At(1u).parent=2u;operations.At(2u).parent=3u;
        operations.callback=[](Operations&,Window receiver,Window,bool) {return receiver==2u;};
        QuestVr::SetNativeWindowToggle(operations,1u,true);
        Require(Events(operations)==std::vector<std::string>{"event1:1=1","event2:1=1"},"Handled parent did not stop propagation");

        operations=Radio();operations.At(3u).selection=1u;operations.At(3u).oneCheck=true;
        operations.At(1u).flags|=1u;operations.At(3u).parent=4u;
        operations.callback=[](Operations& target,Window receiver,Window source,bool value) {
            if(receiver==1u && source==1u && !value)
                Require(target.At(3u).selection==2u,"Old selection callback saw stale Radio selection");
            return false;
        };
        QuestVr::SetNativeWindowToggle(operations,2u,true);
        Require(operations.At(3u).selection==2u && !operations.ReadPressed(1u) && operations.ReadPressed(2u),
            "Radio replacement did not reset previous selection");
        Require(Position(operations,"selection3=2")<Position(operations,"pressed1=0"),"Radio reset old selection before storing replacement");
        Require(Events(operations)==std::vector<std::string>{"event2:2=1","event1:1=0","event3:2=1","event4:2=1"},
            "Radio OneCheck old-unpress handling or script/parent ordering wrong");
        operations.events.clear();
        QuestVr::SetNativeWindowToggle(operations,2u,false);
        Require(operations.ReadPressed(2u) && operations.At(3u).selection==2u,
            "OneCheck failed to reselect the deselected current Toggle");
        Require(Events(operations)==std::vector<std::string>{"event2:2=0","event2:2=1"},
            "OneCheck/current handled branches unexpectedly invoked Radio script or parent");
        operations.events.clear();
        Require(QuestVr::DispatchNativeWindowToggleChanged(operations,3u,1u,false),"OneCheck noncurrent false was not handled");
        Require(operations.events.empty() && operations.At(3u).selection==2u,"OneCheck noncurrent false had side effects");

        operations=Radio();operations.At(3u).selection=1u;operations.At(1u).flags|=1u;
        operations.At(3u).parent=4u;
        QuestVr::SetNativeWindowToggle(operations,1u,false);
        Require(operations.At(3u).selection==0u && !operations.ReadPressed(1u),"Non-OneCheck current false did not clear selection");
        Require(Events(operations)==std::vector<std::string>{"event1:1=0","event3:1=0","event4:1=0"},
            "Cleared Radio selection did not invoke script and propagate its result");
        operations.events.clear();
        Require(QuestVr::DispatchNativeWindowToggleChanged(operations,3u,2u,false),"Non-OneCheck noncurrent false was not handled");
        Require(operations.events.empty(),"Non-OneCheck noncurrent false invoked script");

        operations=Radio();operations.At(3u).selection=1u;
        Require(QuestVr::DispatchNativeWindowToggleChanged(operations,3u,1u,true),"Already-current true Radio callback not handled");
        Require(operations.events.empty() && operations.writes==0u,"Already-current Radio callback changed state");
        operations.At(4u).toggle=true;operations.At(4u).parent=3u;
        QuestVr::SetNativeWindowToggle(operations,4u,true);
        Require(operations.At(3u).selection==1u && Events(operations)==std::vector<std::string>{"event4:4=1","event3:4=1"},
            "Unregistered true Toggle was implicitly registered or omitted script fallback");
        operations.events.clear();
        Require(!QuestVr::DispatchNativeWindowToggleChanged(operations,3u,5u,false),"Non-Toggle source did not use false script default");
        Require(Events(operations)==std::vector<std::string>{"event3:5=0"},"Non-Toggle source incorrectly used native Radio handling");
        operations.events.clear();
        operations.callback=[](Operations&,Window,Window,bool) {return true;};
        Require(QuestVr::DispatchNativeWindowToggleChanged(operations,3u,0u,true),"None source did not use script override result");
        Require(Events(operations)==std::vector<std::string>{"event3:0=1"},"None source was treated as native Toggle");

        // The Radio script receives the incoming true after old-selection
        // callbacks, even if those callbacks unpress the new source themselves.
        operations=Radio();operations.At(3u).selection=1u;operations.At(1u).flags|=1u;
        operations.callback=[](Operations& target,Window receiver,Window source,bool value) {
            if(receiver==1u && source==1u && !value) QuestVr::SetNativeWindowToggle(target,2u,false);
            return false;
        };
        QuestVr::SetNativeWindowToggle(operations,2u,true);
        Require(!operations.ReadPressed(2u) && operations.At(3u).selection==0u,"Radio callback retoggle was overwritten after old reset");
        Require(Events(operations)==std::vector<std::string>{"event2:2=1","event1:1=0","event2:2=0","event3:2=0","event3:2=1"},
            "Radio script lost its captured incoming Bool after old-selection callback");

        // All later failures propagate to one enclosing root, including work,
        // write and native recursion limits. No partial appearance/selection,
        // list mutation, root mutation or random advance survives the abort.
        operations=Radio();operations.At(3u).selection=1u;operations.At(1u).flags|=1u;
        operations.callback=[](Operations& target,Window receiver,Window,bool) {
            if(receiver==3u) {
                target.Write();target.At(3u).registered.push_back(4u);
                target.Write();target.state.rootValue=99u;target.state.random^=0xabcdefu;
                throw std::runtime_error("Later script callback failure");
            }
            return false;
        };
        RefusesAndRollsBack(operations,[&] {QuestVr::SetNativeWindowToggle(operations,2u,true);},
            "Late script failure was swallowed");
        Require(operations.writes>8u,"Failure fixture did not exercise old/new selection and appearance mutations");
        for(std::size_t maximum=0u;maximum<5u;++maximum) {
            operations=Toggle();operations.maximumWrites=maximum;
            operations.callback=[](Operations& target,Window,Window,bool) {
                target.Write();target.state.rootValue=7u;return true;
            };
            RefusesAndRollsBack(operations,[&] {QuestVr::SetNativeWindowToggle(operations,1u,true);},
                "Cumulative root write limit was bypassed");
        }
        operations=Toggle();operations.At(1u).parent=2u;operations.At(2u).parent=1u;
        operations.maximumWork=60u;
        RefusesAndRollsBack(operations,[&] {QuestVr::SetNativeWindowToggle(operations,1u,true);},
            "Cyclic live parent propagation bypassed work limit");
        Require(operations.work==operations.maximumWork && Events(operations).size()>2u,
            "Parent cycle was not bounded by cumulative shared work");
        operations=Toggle();
        operations.callback=[](Operations& target,Window receiver,Window,bool value) {
            QuestVr::SetNativeWindowToggle(target,receiver,!value);return true;
        };
        RefusesAndRollsBack(operations,[&] {QuestVr::SetNativeWindowToggle(operations,1u,true);},
            "Unbounded recursive retoggle bypassed native stack guard");
        Require(operations.peakDepth==128u,"Native stack limit did not apply across reentrant setters");
        Require(operations.work<operations.maximumWork,"Recursion fixture exhausted work before checking native stack bound");

        std::cout << "Native ToggleWindow coordinator tests passed: " << checks << " checks, " << refusals << " refusals\n";
        return 0;
    } catch(const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
}
