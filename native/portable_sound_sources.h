#pragma once
#include "surreal_portable_package_tables.h"
#include "quest_linker_load_context.h"
#include "quest_native_work.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Detached read-only source admission, before InitializePortableRuntime. The
// actual caller-supplied script prefix is unchanged; only missing packages
// named by real Sound imports are returned. No UObject/PCM/driver is created.
// Limits cover detached tables/workspace, not the process-owned NameString
// arena. Normal table loading interns names there with its existing lifetime.
struct PortableSoundSourceLimits {
    std::size_t nativeWork{QuestVr::NativeWork::MaximumLimit},retainedBytes{64u<<20u},serializedBytes{32u<<20u};
    std::size_t packages{128u},bindings{4096u},directoryEntries{65536u};
};
struct PortableSoundSourceBinding {
    std::string importerSourcePath;
    std::int32_t importReference{};
    std::string sourcePath,objectPath;
    std::size_t exportIndex{};
    std::uint32_t objectFlags{};
};
struct PortableSoundSourceBundle {
    std::vector<PortablePackageTables> packages;
    std::vector<PortableSoundSourceBinding> bindings;
    QuestVr::NativeWork::Usage nativeWork;
    std::size_t retainedBytes{},serializedBytes{};
};
PortableSoundSourceBundle LoadPortableRuntimeSoundSources(
    const std::vector<PortablePackageTables>& baseSources,const PortablePackageTables& map,
    const std::string& engineIniPath,QuestVr::OriginalLinkerLoadProfile profile,
    const PortableSoundSourceLimits& limits={});
