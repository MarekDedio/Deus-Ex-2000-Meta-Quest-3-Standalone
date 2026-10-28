#pragma once
#include "quest_portable_vm.h"
#include <cstddef>
#include <cstdint>
#include <string>

enum class PortableCoreExecMode {RollbackOnly,Commit};
enum class PortableConfigPublication {None,Durable,DurabilityUnknown};
struct PortableCoreExecResult {
    QuestVr::Vm::Result execution;
    bool handled{},flushRequested{},cacheChanged{};
    PortableConfigPublication publication{PortableConfigPublication::None};
    std::size_t instanceImports{},defaultImports{},defaultReloads{},configExports{},importFailures{},importWarnings{};
    std::string output;
};
struct PortableUserConfigStorageSnapshot {
    bool prepared{},owned{};
    std::string seedPath,targetPath;
    std::size_t bytes{},retainedBytes{};
    std::uint64_t generation{};
    PortableConfigPublication publication{PortableConfigPublication::None};
};
// Explicit CPU-owner setup, before PreparePortableRuntimeInputConfig. The
// writable authority is the supplied app directory, never the immutable seed.
PortableUserConfigStorageSnapshot BindPortableRuntimeUserConfigStorage(
    const std::string& applicationDataDirectory,const std::string& userIniSeed);
PortableUserConfigStorageSnapshot ReadPortableRuntimeUserConfigStorage();
// Actual Core.StaticExec SET service, NOT an unknown-Input-query shortcut or a
// claim that the UViewport/UPlayer/Engine provider chain has been initialized.
PortableCoreExecResult ExecutePortableRuntimeCoreStaticExec(const std::string& command,
    PortableCoreExecMode mode=PortableCoreExecMode::RollbackOnly,const QuestVr::Vm::Limits& limits={});
