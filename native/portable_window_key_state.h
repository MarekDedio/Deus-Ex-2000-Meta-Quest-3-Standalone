#pragma once

#include "quest_portable_vm.h"
#include <array>
#include <string>

enum class PortableWindowKeyStateProbeMode {
    RoundTrip, LateFailure, Rootless, WrongReceiver, WrongArgument, MalformedByte, MissingArgument,
    WrongDeclaration, ForeignEnumField, ParentCycle, InvalidTerminalCast
};
struct PortableWindowKeyStateProbeResult {
    QuestVr::Vm::Result execution;
    // Detached observations only. The configured native Root and its real child
    // are created by original InitRootWindow in the same rollback-only root.
    std::string playerPath, rootPath, childPath;
    std::array<bool,255u> cleared{}, written{};
    std::array<bool,7u> changed{};
    std::size_t queries{}, writesBeforeQueries{};
    bool key255False{}, terminalNotNearest{}, completed{};
};
PortableWindowKeyStateProbeResult ProbePortableNativeWindowKeyState(
    const std::string& levelInfoPath, PortableWindowKeyStateProbeMode mode,
    const QuestVr::Vm::Limits& limits = {});
