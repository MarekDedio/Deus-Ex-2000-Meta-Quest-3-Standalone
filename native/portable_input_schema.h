#pragma once

#include "portable_engine_collision.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct PortableInputAliasSnapshot {
    std::string alias,command;
    int aliasCompareIndex{};
    bool operator==(const PortableInputAliasSnapshot&) const = default;
};
struct PortableInputDefaultSnapshot {
    std::string classPath,superDefaultPath,friendlyName;
    std::uint32_t classIndex{},defaultIndex{},superDefaultIndex{},nativeSize{},classFlags{};
    std::uint32_t propertyDimension{},propertyElementSize{},propertyFlags{},propertyOffset{},structPropertiesSize{};
    bool hasInputValues{},execInstalled{};
    std::uint32_t viewportIndex{},enumIndex{};
    std::array<PortableInputAliasSnapshot,40u> aliases;
    std::array<std::string,255u> bindings;
};
struct PortableInputSchemaFieldSnapshot {
    PortableNativeEngineFieldSnapshot field;
    std::string referencedStructPath;
    std::uint32_t referencedStructIndex{};
};
struct PortableInputSchemaLinksSnapshot {
    std::string ownerPath,childrenPath;
    std::uint32_t ownerIndex{};
    std::array<std::string,3u> heads; // Property, Config, Constructor.
    bool constructorLinksPrepared{};
};
struct PortableInputNativeStructSnapshot {
    std::string objectPath,classPath,outerPath,friendlyName,superPath;
    std::uint32_t objectIndex{},classIndex{},outerIndex{},objectFlags{},propertiesSize{};
};
struct PortableInputSchemaSnapshot {
    bool prepared{},defaultsPrepared{},configApplied{};
    std::string keyEnumPath;
    std::uint32_t keyEnumIndex{};
    std::size_t retainedBytes{};
    std::int32_t strPropertyUniqueCounter{};
    PortableInputNativeStructSnapshot aliasStruct;
    std::vector<PortableInputSchemaFieldSnapshot> fields;
    std::vector<PortableInputDefaultSnapshot> defaults;
    std::vector<PortableInputSchemaLinksSnapshot> links;
};

// Genuine native Struct/Property/default graph, not serialized descriptors or
// an Input instance. Requires the explicit classes/lookup/collision-default
// domains. Atomic and idempotent; no config import, viewport Init/ResetInput,
// FExec/ReadInput, world startup or gameplay success is implied.
PortableInputSchemaSnapshot PreparePortableRuntimeInputSchema();
PortableInputSchemaSnapshot ReadPortableRuntimeInputSchema();
