#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// Detached inspection of a collected actor, not player ownership/equipment,
// an executed Fire state, an attachment transform, or a renderable mesh claim.
enum class PortableHeldItemDescriptorStatus : std::uint8_t {
    Available, InvalidIdentity, RuntimeUnavailable, UnindexedIdentity,
    NotInventoryActor, NotCollected, MalformedIdentity
};
enum class PortableHeldItemValueStatus : std::uint8_t {
    Unavailable, Available, MissingPropertyMetadata, MalformedProperty,
    InvalidValue, MissingReferencedObject
};
enum class PortableHeldItemValueOrigin : std::uint8_t {
    Zero, ClassDefault, Instance, Overlay, FrozenClassDefault
};
struct PortableHeldItemValueProvenance {
    PortableHeldItemValueOrigin origin{PortableHeldItemValueOrigin::Zero};
    std::string declarationPath, declarationSourcePath, ownerPath, ownerSourcePath;
};
template<class T> struct PortableHeldItemValue {
    PortableHeldItemValueStatus status{PortableHeldItemValueStatus::Unavailable};
    std::optional<T> value;
    PortableHeldItemValueProvenance provenance;
};
struct PortableHeldItemVisualDescriptor {
    PortableHeldItemDescriptorStatus status{PortableHeldItemDescriptorStatus::RuntimeUnavailable};
    std::string requestedPath, actorPath, classPath, actorSourcePath, classSourcePath;
    bool active{}, collected{}, weapon{};
    // Available empty Object references mean an actual None/typed-zero value.
    // Never substitute PickupViewMesh/ThirdPersonMesh for missing PlayerViewMesh.
    PortableHeldItemValue<std::string> playerViewMesh, pickupViewMesh, thirdPersonMesh;
    PortableHeldItemValue<float> playerViewScale, pickupViewScale, thirdPersonScale;
    PortableHeldItemValue<std::string> skin;
    std::array<PortableHeldItemValue<std::string>,8u> multiSkins;
    // Original DeusExWeapon declares all three as Int, in original damage and
    // Unreal world units. No metre conversion, class-name heuristic or coercion.
    PortableHeldItemValue<std::int32_t> hitDamage, accurateRange, maxRange;
    PortableHeldItemValue<bool> handToHand;
};
struct PortableHeldItemDescriptorLimits {
    std::size_t retainedBytes{256u*1024u}, stringBytes{4096u}, hierarchy{128u};
    std::size_t inventoryEntries{100'000u};
};
