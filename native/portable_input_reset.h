#pragma once

#include "portable_core_static_exec.h"

#include <cstddef>
#include <optional>
#include <string>

struct PortableInputResetSourceSnapshot {
    bool prepared{};
    std::string sourcePath;
    std::size_t sourceBytes{},sections{},entries{},retainedBytes{};
};
// Separately pinned immutable DefUser.ini cache. Never a writable User.ini
// backend alias, never inferred from User.ini bytes or absent class fields.
PortableInputResetSourceSnapshot PreparePortableRuntimeInputResetConfigSource(const std::string& defUserIniPath);
PortableInputResetSourceSnapshot ReadPortableRuntimeInputResetConfigSource();

struct PortableInputResetCommands {
    // Explicit diagnostic original SET calls in the SAME root, not startup
    // bypasses. They verify cache-only reset versus pending Flush snapshots.
    std::optional<std::string> before,after;
};
struct PortableInputResetResult {
    QuestVr::Vm::Result execution;
    bool completed{},sectionPresent{},cacheChanged{},cacheDirty{};
    std::string classPath;
    std::size_t overlayCalls{},defaultReloads{},instanceReloads{},configPasses{},configImports{},importFailures{},importWarnings{};
    std::size_t forwardNativeWork{}; // Before deliberate failure/VM undo.
    PortableCoreExecResult beforeCommand,afterCommand;
};
// Actual configured Input/InputExt ResetConfig, with original selected-class
// CDO-first/instance-second reload. Does not imply Input.Init/ResetInput,
// ReadInput, successful startup or a live controller/weapon bridge.
PortableInputResetResult ExecutePortableRuntimeInputResetConfig(const std::string& classPath,
    PortableCoreExecMode mode=PortableCoreExecMode::RollbackOnly,
    const QuestVr::Vm::Limits& limits={},const PortableInputResetCommands& commands={});
