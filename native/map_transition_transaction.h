#pragma once

#include <exception>
#include <string>
#include <utility>

namespace QuestVr {

struct MapReplacementStatus {
    bool prepared{};
    bool runtimeAvailable{};
    bool rollbackAttempted{};
    std::string error;
};

// Run on the map worker while main-thread runtime access is suspended. The
// private checkpoint is mandatory before replacing a usable runtime. Preparation
// must set mutationStarted BEFORE its first source-runtime mutation, including a
// replacement that may itself throw. Cache-only failures need no rollback.
template <typename Checkpoint, typename Prepare, typename Restore>
MapReplacementStatus RunMapReplacementTransaction(
    const bool hadUsableRuntime, Checkpoint&& checkpoint,
    Prepare&& prepare, Restore&& restore) {
    MapReplacementStatus status;
    status.runtimeAvailable = hadUsableRuntime;
    bool mutationStarted = false;
    try {
        if (hadUsableRuntime && !checkpoint()) {
            status.error = "transition checkpoint failed; runtime replacement cancelled";
            return status;
        }
        status.prepared = prepare(mutationStarted);
        if (status.prepared) {
            status.runtimeAvailable = true;
            return status;
        }
        status.error = "map preparation returned failure";
    } catch (const std::exception& error) {
        status.error = error.what();
    }
    if (!mutationStarted) return status;
    status.runtimeAvailable = false;
    if (!hadUsableRuntime) return status;
    status.rollbackAttempted = true;
    try {
        status.runtimeAvailable = restore();
        if (!status.runtimeAvailable) status.error += "; prior-map rollback returned failure";
    } catch (const std::exception& error) {
        status.error += "; prior-map rollback failed: ";
        status.error += error.what();
    }
    return status;
}

} // namespace QuestVr
