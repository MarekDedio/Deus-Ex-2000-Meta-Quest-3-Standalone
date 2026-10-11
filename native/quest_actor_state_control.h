#pragma once

#include <cstdint>
#include <string>

namespace QuestVr {

// Original Core.dll GotoState return values and UObject transition flags.
// This sequence is shared by the runtime and asset-free contract controls;
// ownership, journaling, probing and actual script dispatch belong to Ops.
enum class StateEntryResult : std::uint8_t { NoEntry = 0, Entered = 1, Preempted = 2 };
constexpr std::uint32_t StateChangedFlag = 0x1000u;
constexpr std::uint32_t InEndStateFlag = 0x2000u;

struct StateSelection {
    std::string path;
    std::string name;
    std::uint64_t liveProbeMask{};
};

inline bool StateNameEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    const auto fold = [](const unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    for (std::size_t i = 0; i < a.size(); ++i)
        if (fold(static_cast<unsigned char>(a[i])) != fold(static_cast<unsigned char>(b[i]))) return false;
    return true;
}

template<class Ops>
StateEntryResult OriginalGotoState(Ops& ops, const std::string& requested) {
    if (!ops.HasFrame()) return StateEntryResult::NoEntry;
    ops.CancelLatent(); // Also journals before the following flag changes.
    const auto oldName = ops.StateName();
    const auto selection = ops.Resolve(requested);
    const bool changedName = !StateNameEqual(oldName, selection.name);
    if (!StateNameEqual(oldName, "None") && changedName && ops.IsProbing("EndState") &&
        !(ops.Flags() & InEndStateFlag)) {
        ops.ClearFlags(StateChangedFlag);
        ops.SetFlags(InEndStateFlag);
        try { ops.CallEvent("EndState"); }
        catch (...) { ops.ClearFlags(InEndStateFlag); throw; }
        ops.ClearFlags(InEndStateFlag);
        if (ops.Flags() & StateChangedFlag) return StateEntryResult::Preempted;
    }
    ops.Publish(selection); // Node=StateNode, Code=null; retain locals/timer.
    if (StateNameEqual(selection.name, "None")) return StateEntryResult::NoEntry;
    if (changedName && ops.IsProbing("BeginState")) {
        ops.ClearFlags(StateChangedFlag);
        ops.CallEvent("BeginState");
        if (ops.Flags() & StateChangedFlag) return StateEntryResult::Preempted;
    }
    ops.SetFlags(StateChangedFlag);
    return StateEntryResult::Entered;
}

} // namespace QuestVr
