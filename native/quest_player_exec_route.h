#pragma once
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace QuestVr::PlayerExecRoute {
inline constexpr std::size_t MaximumInventoryOffers=4096u;

// UPlayer.Exec103D7ED0..103D8098. Handle{} is None. Ops authenticates actual
// fields and each concrete provider. Auxiliary is the native Actor+628 offer;
// this helper deliberately does not misidentify it as Pawn.Weapon(+3D0).
// Each child receives its own command-view value, preserving the caller's
// command pointer. Selectors and callbacks share the owning work budget.
template<class Operations>
bool Exec(Operations& operations,std::u16string_view command) {
    using Handle=typename Operations::Handle;
    const auto actor=[&]{operations.Step();return operations.Actor();};
    auto current=actor();
    if(current!=Handle{}) {
        operations.Step();const auto level=operations.XLevel(current);
        if(level==Handle{})throw std::runtime_error("UPlayer Exec has an Actor without actual XLevel");
        operations.Step();if(operations.ExecLevel(level,command))return true;
    }
    current=actor();
    if(current!=Handle{}) {
        operations.Step();const auto info=operations.LevelInfo(current);
        if(info!=Handle{}) {
            operations.Step();const auto game=operations.Game(info);
            if(game!=Handle{}) {
                operations.Step();if(operations.ScriptConsoleExec(game,command,current))return true;
            }
        }
    }
    current=actor();
    if(current!=Handle{}) {
        operations.Step();const auto auxiliary=operations.Auxiliary(current);
        if(auxiliary!=Handle{}) {
            operations.Step();if(operations.ScriptConsoleExec(auxiliary,command,current))return true;
        }
    }
    current=actor();
    if(current!=Handle{}) {
        operations.Step();if(operations.ScriptConsoleExec(current,command,current))return true;
    }
    current=actor();Handle inventory{};
    if(current!=Handle{}) {operations.Step();inventory=operations.Inventory(current);}
    std::size_t offered{};
    while(inventory!=Handle{}) {
        operations.Step();
        if(offered++==MaximumInventoryOffers)
            throw std::runtime_error("UPlayer Exec Inventory route exceeds its bounded live traversal");
        const auto executor=actor();
        operations.Step();if(operations.ScriptConsoleExec(inventory,command,executor))return true;
        // The callback can splice this exact object's next Inventory pointer.
        operations.Step();inventory=operations.Inventory(inventory);
    }
    current=actor();
    if(current!=Handle{}) {
        operations.Step();const auto level=operations.XLevel(current);
        if(level==Handle{})throw std::runtime_error("UPlayer Exec final Actor has no actual XLevel");
        operations.Step();const auto engine=operations.Engine(level);
        if(engine==Handle{})throw std::runtime_error("UPlayer Exec actual XLevel has no Engine");
        operations.Step();if(operations.ExecEngine(engine,command))return true;
    }
    return false;
}
} // namespace QuestVr::PlayerExecRoute
