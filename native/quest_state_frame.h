#pragma once

#include "quest_portable_vm.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace QuestVr {

// Stable portable ordinals matching the pinned VM/Frame.h LatentRunState.
// Representing a latent action here does not implement its runtime handler.
enum class StateLatent : std::uint8_t {
    Continue=0, Stop=1, Sleep=2, FinishAnim=3, FinishInterpolation=4,
    MoveTo=5, MoveToward=6, StrafeTo=7, StrafeFacing=8, TurnTo=9,
    TurnToward=10, WaitForLanding=11
};

struct StateLocal {
    std::string key;
    std::vector<Vm::Value> values;
};
struct StateFrame {
    // GotoLabel may replace executable code without recreating Variables.
    // Locals therefore retain their own declaration identity.
    std::string codePath, localsCodePath;
    // Next top-level statement ordinal, not a byte offset. A cleared or stopped
    // frame retains this position; neither empty code nor Stop implies zero.
    std::uint32_t statementIndex{};
    StateLatent latent{StateLatent::Continue};
    std::vector<StateLocal> locals;
};
struct StateObject {
    bool hasStack{};
    // False preserves the immutable authored dormant-frame context without
    // fabricating local storage. Dynamic disabled sets can still be retained.
    // True makes frame (including absent or null-code) the portable override.
    bool frameOverride{};
    std::optional<StateFrame> frame;
    // State names (including None and class names) own distinct disabled sets.
    // Identity spelling is retained; lookup/codec uniqueness is ASCII-folded.
    std::map<std::string, std::set<std::string>> disabled;
};

} // namespace QuestVr
