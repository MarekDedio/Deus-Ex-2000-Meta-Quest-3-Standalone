#pragma once

#include "portable_core_static_exec.h"

#include <cstddef>
#include <string>

struct PortableStringToNameResult {
    QuestVr::Vm::Result execution;
    bool completed{};
    std::string canonicalName;
    int compareIndex{};
    std::size_t arenaGrowthBytes{},processNameCreditBytes{},forwardNativeWork{};
};
// Explicit original native1024 invocation, not a startup substitute. Receiver
// must be an actual ExtensionObject (an original CDO is valid for this pure
// receiver-independent method). World writes roll back normally; successful
// FName additions have the embedding's established PROCESS lifetime even if
// the enclosing world transaction later rolls back. Credits never retire with
// a map. Indexed mode tests the real null-declaration native-bytecode route.
// The input must be a canonical String Value with no hidden Struct/INT-array
// containers, and must satisfy limits.stringBytes before any argument copy.
PortableStringToNameResult ExecutePortableRuntimeStringToName(
    const std::string& receiverPath,const QuestVr::Vm::Value& argument,
    PortableCoreExecMode mode=PortableCoreExecMode::RollbackOnly,
    const QuestVr::Vm::Limits& limits={},bool indexedNative=false);
