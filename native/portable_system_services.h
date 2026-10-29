#pragma once
#include "quest_portable_vm.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct PortableSystemField {
    std::string path,classPath,outerPath,nextPath,innerPath,category;
    std::uint32_t index{},classIndex{},outerIndex{},objectFlags{},flags{},dimension{},elementSize{},offset{};
};
struct PortableSystemServicesSnapshot {
    bool prepared{},rooted{},execInstalled{},constructorLinksVerified{};
    std::string classPath,objectPath,outerPath,configPath;
    std::uint32_t classIndex{},objectIndex{},outerIndex{},objectFlags{},classFlags{},nativeSize{},superIndex{},withinIndex{};
    std::int32_t purgeCacheDays{};
    std::string savePath,cachePath,cacheExt;
    std::vector<std::string> paths,suppress;
    std::vector<std::string> defaultPaths,defaultSuppress;
    std::vector<PortableSystemField> fields;
};
struct PortableSystemCommandResult {
    QuestVr::Vm::Result execution;
    bool handled{};
    std::string output;
};
// Explicit CPU-owner preparation of actual native USystem class/defaults,
// six linked Config fields/two array inners and rooted GSys instance. Requires
// prior EngineCollision/InputSchema (shared native Property/Name defaults).
// Original sources are read-only; no original DLL/EXE or system command runs.
PortableSystemServicesSnapshot PreparePortableRuntimeSystemServices(const std::string& engineIni);
PortableSystemServicesSnapshot ReadPortableRuntimeSystemServices();
PortableSystemCommandResult ExecutePortableRuntimeSystemCommand(const std::string& command,
    const QuestVr::Vm::Limits& limits={});
