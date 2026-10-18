#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Explicit CPU-owner collision-service subphase, not full UEngine.Init or
// begun world/player startup. Requires actual Engine/metadata/config classes,
// Packages and UObject lookup. The supplied INI is an explicit portable file
// binding, not a recovered packed-launcher GConfig factory/filename alias.
struct PortableNativeDefaultSnapshot {
    std::string classPath;
    std::uint32_t classIndex{}, defaultIndex{}, superClassIndex{}, nativeSize{}, classFlags{};
    std::array<std::string,5u> configuredClasses{};
    std::int32_t cacheSizeMegs{};
    bool useSound{}, urlValid{};
    float currentTickRate{};
    std::vector<std::string> serverActors, serverPackages;
};
struct PortableNativeEngineFieldSnapshot {
    std::string objectPath, classPath, outerPath, nextPath, innerPath;
    std::string propertyNextPath, configNextPath, constructorNextPath, category;
    std::uint32_t objectIndex{}, classIndex{}, outerIndex{}, objectFlags{};
    std::uint32_t arrayDimension{}, elementSize{}, propertyFlags{}, dataOffset{};
};
struct PortableEngineCollisionSnapshot {
    bool prepared{};
    std::string enginePath, engineClassPath, transientPath, cylinderPath, boundLevelPath;
    std::uint32_t engineIndex{}, engineClassIndex{}, transientIndex{}, cylinderIndex{}, cylinderClassIndex{}, levelIndex{};
    std::uint32_t engineFlags{}, transientFlags{}, cylinderFlags{};
    bool engineExecInstalled{}, instanceUrlValid{}, cylinderBoundsValid{};
    std::int32_t engineUniqueCounter{}, primitiveUniqueCounter{}, instanceCacheSizeMegs{};
    bool instanceUseSound{};
    std::array<std::string,5u> instanceConfiguredClasses{};
    std::vector<std::string> instanceServerActors, instanceServerPackages;
    std::vector<PortableNativeDefaultSnapshot> defaults;
    std::vector<PortableNativeEngineFieldSnapshot> fields;
};

// Atomic, idempotent, outside XR/GL and independent of headset/client lifetime.
// Defaults copy/config precedes real Engine/Primitive construction. Engine and
// Transient are roots; Cylinder is retained only by the actual Engine edge.
// No commercial executable/library is loaded, no begun/Active flag is forced,
// and unsupported configured native drivers are not fabricated.
PortableEngineCollisionSnapshot PreparePortableRuntimeEngineCollision(const std::string& iniPath);
PortableEngineCollisionSnapshot ReadPortableRuntimeEngineCollision();
