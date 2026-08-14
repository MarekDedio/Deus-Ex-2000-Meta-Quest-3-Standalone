#pragma once

#include "surreal_portable_package_tables.h"

#include <cstddef>

struct PortableRuntimeSummary {
    bool passed{};
    std::size_t objects{};
    std::size_t classes{};
    std::size_t functions{};
    std::size_t properties{};
    std::size_t resolvedLinks{};
    std::size_t unresolvedExternalLinks{};
    std::size_t normalizedBytecodeBytes{};
    std::size_t peakGcObjects{};
    std::size_t destroyedObjects{};
};

PortableRuntimeSummary BuildAndVerifyPortableRuntime(
    const PortablePackageTables& package);
PortableRuntimeSummary InitializePortableRuntime(
    const PortablePackageTables& package);
void ShutdownPortableRuntime();
