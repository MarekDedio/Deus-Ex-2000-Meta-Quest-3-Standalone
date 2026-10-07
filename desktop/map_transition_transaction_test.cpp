#include "map_transition_transaction.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct RuntimeFixture {
    std::string map = "prior-map";
    int health = 64;
    std::string item = "Multitool";
    std::string savedMap;
    int savedHealth{};
    std::string savedItem;
    int snapshots{};
    int restores{};

    bool Checkpoint() {
        ++snapshots;
        savedMap = map;
        savedHealth = health;
        savedItem = item;
        return true;
    }
    bool Restore() {
        ++restores;
        map = savedMap;
        health = savedHealth;
        item = savedItem;
        return true;
    }
};

void FailedReplacementRestoresFixture() {
    RuntimeFixture runtime;
    const auto status = QuestVr::RunMapReplacementTransaction(true,
        [&] { return runtime.Checkpoint(); }, [&](bool& modified) -> bool {
            Require(runtime.snapshots == 1, "runtime mutated before checkpoint");
            modified = true;
            runtime.map = "partially-loaded-map";
            runtime.health = 100;
            runtime.item.clear();
            throw std::runtime_error("actor mesh failure");
        }, [&] { return runtime.Restore(); });
    Require(!status.prepared && status.runtimeAvailable && status.rollbackAttempted,
            "failed replacement did not report a usable restored runtime");
    Require(runtime.map == "prior-map" && runtime.health == 64 &&
            runtime.item == "Multitool" && runtime.restores == 1,
            "rollback lost prior map, health, or inventory");
    Require(status.error == "actor mesh failure", "rollback lost original failure reason");
}

void CheckpointFailureDoesNotMutate() {
    bool prepared = false;
    bool restored = false;
    const auto status = QuestVr::RunMapReplacementTransaction(true,
        [] { return false; }, [&](bool&) { prepared = true; return true; },
        [&] { restored = true; return true; });
    Require(!status.prepared && status.runtimeAvailable && !status.rollbackAttempted &&
            !prepared && !restored, "checkpoint failure touched a usable runtime");
    const auto thrown = QuestVr::RunMapReplacementTransaction(true,
        []() -> bool { throw std::runtime_error("checkpoint I/O"); },
        [&](bool&) { prepared = true; return true; }, [&] { restored = true; return true; });
    Require(!thrown.prepared && thrown.runtimeAvailable && !prepared && !restored &&
            thrown.error == "checkpoint I/O", "checkpoint exception did not fail safely");
}

void CacheOnlyFailureDoesNotReload() {
    RuntimeFixture runtime;
    const auto status = QuestVr::RunMapReplacementTransaction(true,
        [&] { return runtime.Checkpoint(); }, [](bool&) -> bool {
            throw std::runtime_error("cache read failure");
        }, [&] { return runtime.Restore(); });
    Require(!status.prepared && status.runtimeAvailable && !status.rollbackAttempted &&
            runtime.restores == 0 && runtime.map == "prior-map",
            "cache-only failure unnecessarily replaced prior runtime");
}

void SuccessfulPreparationDoesNotRestore() {
    RuntimeFixture runtime;
    const auto status = QuestVr::RunMapReplacementTransaction(true,
        [&] { return runtime.Checkpoint(); }, [&](bool& modified) {
            modified = true;
            runtime.map = "next-map";
            return true;
        }, [&] { return runtime.Restore(); });
    Require(status.prepared && status.runtimeAvailable && !status.rollbackAttempted &&
            status.error.empty() && runtime.map == "next-map" && runtime.restores == 0,
            "successful preparation rolled back or lost status");
}

void RollbackFailureIsClosed() {
    for (const bool throws : {false, true}) {
        const auto status = QuestVr::RunMapReplacementTransaction(true,
            [] { return true; }, [](bool& modified) { modified = true; return false; },
            [throws] { if (throws) throw std::runtime_error("restore decode"); return false; });
        Require(!status.prepared && !status.runtimeAvailable && status.rollbackAttempted &&
                status.error.find("rollback") != std::string::npos,
                "failed rollback exposed the partial runtime as available");
    }
}

void RecoveryWithoutUsableRuntime() {
    int checkpointCalls = 0;
    int restoreCalls = 0;
    const auto recovered = QuestVr::RunMapReplacementTransaction(false,
        [&] { ++checkpointCalls; return false; },
        [](bool& modified) { modified = true; return true; },
        [&] { ++restoreCalls; return true; });
    Require(recovered.prepared && recovered.runtimeAvailable &&
            checkpointCalls == 0 && restoreCalls == 0,
            "recovery required a checkpoint of an unavailable runtime");
    const auto failed = QuestVr::RunMapReplacementTransaction(false,
        [&] { ++checkpointCalls; return true; },
        [](bool& modified) { modified = true; return false; },
        [&] { ++restoreCalls; return true; });
    Require(!failed.prepared && !failed.runtimeAvailable && !failed.rollbackAttempted &&
            checkpointCalls == 0 && restoreCalls == 0,
            "failed recovery restored an unavailable prior runtime");
}

} // namespace

int main() {
    try {
        FailedReplacementRestoresFixture();
        CheckpointFailureDoesNotMutate();
        CacheOnlyFailureDoesNotReload();
        SuccessfulPreparationDoesNotRestore();
        RollbackFailureIsClosed();
        RecoveryWithoutUsableRuntime();
        std::cout << "Map transition transaction: 6 regression groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Map transition transaction failed: " << error.what() << '\n';
        return 1;
    }
}
