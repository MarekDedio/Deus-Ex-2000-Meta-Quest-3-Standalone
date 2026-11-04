#pragma once
#include "portable_core_static_exec.h"
#include <functional>

// Detached scalar observations of objects genuinely present at probe entry.
// No lookup/schema operation is attempted on a retired object after rollback.
struct PortableNativeClientGraphLeaseObservation {
    bool playerPresent{},playerBinding{},playerPointer{},playerToViewport{};
    bool viewportActor{},viewportCanvas{},canvasViewport{},viewportConsole{},consoleOwner{},viewportInput{};
    bool inputRetired{},inputOwner{},inputOwnerIsViewport{},inputViewport{},inputInitialized{},inputReceipt{},inputPhysicalDefault{};
    bool inputMutation{};
    std::uint64_t cursorSource{},cursorSequence{},cursorRejections{};
    std::uint32_t viewportIndex{},canvasIndex{},consoleIndex{},inputIndex{},playerIndex{};
    bool operator==(const PortableNativeClientGraphLeaseObservation&) const = default;
};
struct PortableNativeClientGraphLeaseProbeResult {
    QuestVr::Vm::Result execution;
    // Must outlive ExecuteTransaction: Core.StaticExec retains its output pointer
    // until Commit/Rollback has retired the config journal.
    PortableCoreExecResult mutation;
    PortableNativeClientGraphLeaseObservation before,during,after;
    bool completed{},callbackCompleted{},afterCaptured{};
    std::size_t peerWrites{};
};
// Bounded diagnostic sibling of the read-only Input query lease probe. Requires
// actual prior Client/Viewport/Canvas/Input/Console construction. Writes genuine
// scalar properties through production VM references, performs actual Core SET
// fanout/COW, optionally invokes the embedding teardown callback, and ALWAYS
// deliberately rolls back. SET normally requests config Flush: this deliberate
// failure is mandatory and must never be replaced by successful Commit.
// Does not create/possess a player, begin a world, or synthesize Input.Init.
PortableNativeClientGraphLeaseProbeResult ProbePortableRuntimeNativeClientGraphLease(
    std::uint64_t viewportIdentity,const std::function<void()>& afterMutation={},
    const QuestVr::Vm::Limits& limits={});
