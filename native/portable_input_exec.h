#pragma once
#include "portable_viewport_input.h"
#include "quest_portable_vm.h"
#include <functional>

struct PortableInputExecProbeResult {
    QuestVr::Vm::Result execution;
    PortableInputQueryResult command;
    bool completed{};
};
// Actual native Input/alias/FExec/output route in a rollback-only VM root.
// Requires an explicitly bound real process-loop capability. Does not construct
// or initialize Input, invent Action/Viewport state or publish gameplay.
PortableInputExecProbeResult ProbePortableRuntimeInputCommands(std::uint64_t viewportIdentity,
    const std::string& command,bool lateFailure=false,const QuestVr::Vm::Limits& limits={});
// Narrow READ-ONLY actual KEYNAME1 query, followed by an explicit test-source
// callback and normal Commit. Proves client lease checks even when Input alone
// handles the command; it does not authorize committing arbitrary command lists.
PortableInputExecProbeResult ProbePortableRuntimeInputQueryLease(std::uint64_t viewportIdentity,
    const std::function<void()>& beforeCommit={},const QuestVr::Vm::Limits& limits={});
