#pragma once

#include "quest_portable_vm.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct PortableConsoleFieldSnapshot {
    std::string declaration;
    std::uint32_t index{};
    QuestVr::Vm::Value value;
};
struct PortableViewportConsoleSnapshot {
    bool prepared{},initialized{},outputDeviceBound{},hasStack{},frameOverride{},codePresent{};
    std::string objectPath,classPath,outerPath,ownerViewportPath,viewportPath,nodePath,stateNodePath;
    std::uint32_t objectIndex{},classIndex{},outerIndex{},ownerViewportIndex{},viewportIndex{},nodeIndex{},stateNodeIndex{},objectFlags{};
    std::int32_t uniqueCounter{};
    std::uint64_t probeMask{},stateLocalsRevision{};
    std::size_t inventoryCapacity{},inventoryBytes{},propertyRootBytes{},frozenDefaultOverrides{};
    std::vector<PortableConsoleFieldSnapshot> fields;
};
struct PortableViewportConsoleResult {
    QuestVr::Vm::Result execution;
    PortableViewportConsoleSnapshot console;
};

// Actual configured native constructor and Console.Init/Core.InitExecution.
// Requires this viewport's prior Canvas initialization and Input construction.
// No command execution, Typing state, platform OpenWindow or Input.Init is run.
// ownerViewport is constructor provenance; Viewport is the real mutable field.
// vtblOut is a native output-provider identity, never a script-visible address.
PortableViewportConsoleResult PreparePortableRuntimeViewportConsole(std::uint64_t viewportIdentity,
    const std::string& engineIni,bool commit=true,const QuestVr::Vm::Limits& limits={});
PortableViewportConsoleSnapshot ReadPortableRuntimeViewportConsole(std::uint64_t viewportIdentity);

struct PortableConsoleExecPrefixResult {
    QuestVr::Vm::Result execution;
    bool handled{};
};
// Actual Console ScriptConsoleExec selection prefix. Missing/non-Exec methods
// decline; matching Exec methods refuse until their parameter/call service is
// implemented. This is not UViewport/UPlayer fallback or successful startup.
PortableConsoleExecPrefixResult ExecutePortableRuntimeConsoleExecPrefix(std::uint64_t viewportIdentity,
    const std::string& command,const QuestVr::Vm::Limits& limits={});
