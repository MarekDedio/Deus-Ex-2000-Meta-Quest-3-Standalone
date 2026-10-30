#pragma once
#include "surreal_portable_package_tables.h"
#include "quest_linker_load_context.h"
#include "quest_native_work.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Detached, read-only pre-runtime source admission. These tables must join the
// normal baseline before InitializePortableRuntime; never append after a map.
struct PortableMusicSourceLimits {
    std::size_t nativeWork{QuestVr::NativeWork::MaximumLimit},retainedBytes{64u<<20u},serializedBytes{32u<<20u};
    std::size_t packages{128u},bindings{4096u},directoryEntries{65536u};
};
struct PortableMusicSourceBinding {
    std::string importerSourcePath;
    std::int32_t importReference{};
    std::string sourcePath,objectPath;
    std::size_t exportIndex{};
    std::uint32_t objectFlags{};
};
struct PortableMusicSourceBundle {
    std::vector<PortablePackageTables> packages; // Additional sources ONLY; script prefix is unchanged.
    std::vector<PortableMusicSourceBinding> bindings;
    QuestVr::NativeWork::Usage nativeWork;
    std::size_t retainedBytes{},serializedBytes{};
};
PortableMusicSourceBundle LoadPortableRuntimeMusicSources(
    const std::vector<PortablePackageTables>& baseSources,const PortablePackageTables& map,
    const std::string& engineIniPath,QuestVr::OriginalLinkerLoadProfile profile,
    const PortableMusicSourceLimits& limits = {});
