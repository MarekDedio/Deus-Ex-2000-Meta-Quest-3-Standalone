#pragma once

#include "surreal_portable_package_tables.h"
#include "quest_linker_load_context.h"
#include "quest_native_work.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Detached original source admission only. Additional tables MUST join the
// baseline BEFORE InitializePortableRuntime; never append in a post-map epoch.
// This grants no UObject/Class/CDO/constructor, image or live tracker authority.
// retainedBytes bounds this admission's detached tables/workspace, not the
// already-live process NameString arena. The shared table loader interns names
// there and may resize its existing containers; its process-lifetime allocation
// policy and lack of rollback are NOT granted a new bound by this API.
// Existing baseSources belong to the caller's immutable source epoch. Paths
// accepts the shipped '*.extension' filename-mask form; broader masks refuse.
struct PortableBrushTextureSourceLimits {
    std::size_t nativeWork{QuestVr::NativeWork::MaximumLimit},retainedBytes{64u<<20u},serializedBytes{32u<<20u};
    std::size_t packages{128u},bindings{4096u},directoryEntries{65536u};
};
struct PortableBrushTextureSourceBinding {
    std::string importerSourcePath;
    std::size_t modelExportIndex{},polysExportIndex{};
    std::int32_t textureReference{};
    std::string sourcePath,objectPath;
    std::size_t exportIndex{};
    std::uint32_t objectFlags{};
};
struct PortableBrushTextureSourceBundle {
    std::vector<PortablePackageTables> packages;
    std::vector<PortableBrushTextureSourceBinding> bindings;
    QuestVr::NativeWork::Usage nativeWork;
    std::size_t retainedBytes{},serializedBytes{};
};
PortableBrushTextureSourceBundle LoadPortableRuntimeBrushTextureSources(
    const std::vector<PortablePackageTables>& baseSources,const PortablePackageTables& map,
    const std::string& engineIniPath,QuestVr::OriginalLinkerLoadProfile profile,
    const PortableBrushTextureSourceLimits& limits = {});
