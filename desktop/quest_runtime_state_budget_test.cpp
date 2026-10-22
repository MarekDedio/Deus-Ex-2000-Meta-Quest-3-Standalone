#include "quest_runtime_state_budget.h"
#include "quest_native_scratch_scope.h"
#include "quest_save_bundle.h"

#include <iostream>

namespace {
std::size_t checks{}, refusals{};
void Need(bool value, const char* why) {
    ++checks;
    if (!value) throw std::runtime_error(why);
}
template<class F> void Refuse(F run, const char* why) {
    bool refused{};
    try { run(); } catch (const std::runtime_error&) { refused = true; }
    Need(refused, why); ++refusals;
}
void Run() {
    const auto limits = QuestVr::LiveRuntimeStateLimits();
    const QuestVr::ScriptStateLimits original;
    Need(limits.maxBytes == 32u * 1024u * 1024u, "Live retained allowance is not explicitly bounded");
    Need(limits.maxBytes == original.maxBytes && limits.maxBytes == QuestVr::Vm::Limits{}.retainedBytes,
        "Live allowance does not match the existing structural/VM retained default");
    Need(QuestVr::kMaximumSaveRuntimeBytes == 16u * 1024u * 1024u,
        "Live allowance changed the encoded save-file cap");
    Need(limits.maxProperties == original.maxProperties && limits.totalValueNodes == original.totalValueNodes &&
        limits.maxObjects == original.maxObjects && limits.maxDepth == original.maxDepth &&
        limits.maxActorLinks == original.maxActorLinks && limits.maxIntArrayElements == original.maxIntArrayElements,
        "Separating retained bytes widened structural limits");
    QuestVr::ScriptStateDetail::Budget native{limits}, state{limits};
    const auto combined = [&] { QuestVr::ValidateRuntimeStateCombinedBudget(limits, native, state); };
    // Actual former Training failure: the independently owned lookup alone is
    // 11.4 MB. Neither that index nor native query scratch is saved payload.
    native.Retain(16'665'955u); state.Retain(200'000u); combined();
    Need(native.retained + state.retained > QuestVr::kMaximumSaveRuntimeBytes,
        "Regression no longer crosses the unrelated save-file boundary");
    state.Retain(limits.maxBytes - native.retained - state.retained); combined();
    Need(native.retained + state.retained == limits.maxBytes, "Exact combined live cap refused");
    state.Retain(1u);
    Refuse(combined, "Individually legal retained blocks each borrowed the full combined allowance");
    --state.retained;
    native.retained = limits.maxBytes + 1u;
    Refuse(combined, "Oversized native block wrapped the subtraction");
    native.retained = std::numeric_limits<std::size_t>::max();
    Refuse(combined, "Overflow-sized native accounting passed");
    native.retained = 0u; state.retained = std::numeric_limits<std::size_t>::max();
    Refuse(combined, "Overflow-sized measured state accounting passed");
    state.retained = 0u; native.nodes = limits.totalValueNodes; combined();
    state.nodes = 1u;
    Refuse(combined, "Separate byte allowance relaxed combined value-node limits");
    native.nodes = std::numeric_limits<std::size_t>::max();
    Refuse(combined, "Overflow-sized native nodes passed");
    native.nodes = 0u; state.nodes = std::numeric_limits<std::size_t>::max();
    Refuse(combined, "Overflow-sized measured nodes passed");
    native.nodes = state.nodes = 0u;
    Refuse([&] { native.Retain(limits.maxBytes + 1u); }, "Single native allocation exceeded the fixed cap");
    Need(native.retained == 0u, "Rejected retained admission changed its ledger");
    // Use the production combined validator with the same lexical scratch
    // reservation used by collision callbacks; failed nesting must unwind.
    native.Retain(24u << 20u); state.Retain(4u << 20u);
    std::size_t scratch{};
    const auto validate = [&](std::size_t extra) {
        auto staged = native; staged.Retain(scratch); staged.Retain(extra);
        QuestVr::ValidateRuntimeStateCombinedBudget(limits, staged, state);
    };
    {
        QuestVr::NativeScratchScope outer(scratch, 3u << 20u, validate);
        Refuse([&] { QuestVr::NativeScratchScope child(scratch, (1u << 20u) + 1u, validate); },
            "Nested native scratch ignored other live owners");
        Need(scratch == (3u << 20u), "Rejected child changed the caller scratch ledger");
        QuestVr::NativeScratchScope child(scratch, 1u << 20u, validate);
        Need(scratch == (4u << 20u), "Exact composed native scratch cap refused");
    }
    Need(scratch == 0u, "Composed native scratch survived its lexical lifetime");
    // Exercise the real save encoder, not just an assertion about constants.
    QuestVr::SaveBundleData saved; saved.generation = 1u; saved.metadata = {1u};
    saved.runtime.resize(QuestVr::kMaximumSaveRuntimeBytes + 1u);
    std::vector<std::uint8_t> encoded;
    Need(!QuestVr::EncodeSaveBundle(saved, encoded) && encoded.empty(),
        "Higher live allowance admitted an oversized encoded save");
    saved.runtime.resize(QuestVr::kMaximumSaveRuntimeBytes);
    Need(QuestVr::EncodeSaveBundle(saved, encoded), "Exact encoded save cap stopped working");
    QuestVr::SaveBundleData decoded;
    Need(QuestVr::DecodeSaveBundle(encoded, decoded) && decoded.runtime == saved.runtime,
        "Exact-cap encoded save did not round-trip independently of live memory");
}
}
int main() {
    try { Run(); std::cout << "PASS live/runtime versus save budgets: " << checks << " checks / " << refusals << " refusals.\n"; return 0; }
    catch (const std::exception& error) { std::cerr << "Runtime state budget test failed: " << error.what() << '\n'; return 1; }
}
