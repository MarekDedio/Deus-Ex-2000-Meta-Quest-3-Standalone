#pragma once

#include "quest_script_state.h"

namespace QuestVr {
// Live native/script state is not its serialized save representation. This
// ledger also admits the weak lookup index, collision state and overlapping
// native query scratch, none of which belongs to the runtime save payload.
// Keep a fixed allowance, matching the existing structural/VM retained default,
// independently of the unchanged save-file cap. This is NOT total process RAM:
// assets and execution-owned allocations have separate owners and bounds.
inline constexpr std::size_t kMaximumLiveRuntimeStateBytes = 32u << 20u;

inline ScriptStateLimits LiveRuntimeStateLimits() {
    ScriptStateLimits limits;
    limits.maxBytes = kMaximumLiveRuntimeStateBytes;
    return limits;
}

inline void ValidateRuntimeStateCombinedBudget(const ScriptStateLimits& limits,
    const ScriptStateDetail::Budget& native, const ScriptStateDetail::Budget& state) {
    // Neither independently bounded component may borrow the other's full
    // allowance. Subtract only after checking the subtrahend, even for malformed
    // accounting inputs, so exact-cap success never becomes overflow success.
    if (native.retained > limits.maxBytes || state.retained > limits.maxBytes - native.retained)
        throw std::runtime_error("Persistent combined default/state byte budget exceeded");
    if (native.nodes > limits.totalValueNodes || state.nodes > limits.totalValueNodes - native.nodes)
        throw std::runtime_error("Persistent combined default/state value-node budget exceeded");
}
} // namespace QuestVr
