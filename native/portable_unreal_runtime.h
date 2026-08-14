#pragma once

#include "surreal_portable_package_tables.h"

#include <cstddef>
#include <cstdint>
#include <string>

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

struct PortableMapRuntimeSummary {
    bool passed{};
    std::size_t exports{};
    std::size_t actors{};
    std::size_t actorProperties{};
    std::size_t resolvedClasses{};
    std::size_t unresolvedClasses{};
};

PortableRuntimeSummary BuildAndVerifyPortableRuntime(
    const PortablePackageTables& package);
PortableRuntimeSummary InitializePortableRuntime(
    const PortablePackageTables& package);
PortableRuntimeSummary InitializePortableRuntime(
    const std::vector<PortablePackageTables>& packages);
void ShutdownPortableRuntime();

enum class PortableVmValueType {
    Nothing,
    Integer,
    Float,
    Boolean,
    String,
    ObjectReference,
    NameReference,
};

struct PortableVmValue {
    PortableVmValueType type{PortableVmValueType::Nothing};
    std::int32_t integer{};
    float floating{};
    bool boolean{};
    std::string string;
};

PortableVmValue ExecutePortableFunction(const std::string& objectPath);
PortableMapRuntimeSummary LoadPortableRuntimeMap(
    const PortablePackageTables& package);
