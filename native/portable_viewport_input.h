#pragma once

#include "portable_input_schema.h"

// Explicit embedding output-device profile: ordinary enabled FString output,
// CString appends without a newline. No claim about retail fatal/silent globals.
enum class PortableInputLoggingProfile {Ordinary};
struct PortableViewportInputSnapshot {
    bool prepared{},constructed{},initialized{},execInstalled{},extensionNamesPrepared{};
    std::string objectPath,classPath,outerPath,ownerViewportPath,enumPath;
    std::uint32_t objectIndex{},classIndex{},outerIndex{},ownerViewportIndex{},enumIndex{},nativeViewportIndex{},objectFlags{};
    std::uint64_t clientIdentity{},viewportIdentity{},lookupGeneration{};
    std::int32_t uniqueCounter{};
    std::size_t retainedBytes{},configReloads{},configPasses{},configImports{},configFailures{},configWarnings{},configWork{};
    std::array<PortableInputAliasSnapshot,40u> aliases;
    std::array<std::string,255u> bindings;
    std::array<std::string,52u> extensionNames;
    std::array<int,52u> extensionNameIndices{};
};
struct PortableInputQueryResult {bool handled{};std::string output;std::size_t work{};};

// Explicit standalone/non-editor CPU-owner constructor-only service. Requires configured defaults and
// a genuine prepared viewport. This publishes viewport.Input, but NOT Init's
// native Input.Viewport edge, ResetInput, event dispatch or ReadInput support.
PortableViewportInputSnapshot PreparePortableRuntimeViewportInput(std::uint64_t viewportIdentity,PortableInputLoggingProfile logging);
PortableViewportInputSnapshot ReadPortableRuntimeViewportInput(std::uint64_t viewportIdentity);
// Direct genuine Input provider diagnostic; unlike UViewport.Exec, does not
// require a possessed Actor. An unknown command returns handled=false, not a
// fabricated successful viewport/Console fallback. Never constructs on reads.
PortableInputQueryResult ExecutePortableRuntimeInputQuery(std::uint64_t viewportIdentity,const std::string& command);
