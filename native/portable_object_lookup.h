#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

struct PortableObjectLookupState {
    std::size_t objects{}, retainedBytes{};
    std::uint64_t generation{};
    bool prepared{};
};
struct PortableObjectLookupRequest {
    std::string name;
    std::uint32_t requestedClassIndex{}, outerIndex{};
    bool anyPackage{true}, exact{};
};
struct PortableObjectLookupResult {
    std::string objectPath;
    std::uint32_t objectIndex{}, classIndex{}, outerIndex{};
    int nameIndex{};
    std::uint64_t generation{};
};
// Exclusive CPU-owner service; requires the explicit actual Package phase.
// Handles are current object-index leases, not export slots or PE pointers.
// No package loading, constructors, native module binding or name interning.
PortableObjectLookupState PreparePortableRuntimeObjectLookup();
PortableObjectLookupState ReadPortableRuntimeObjectLookup();
std::optional<PortableObjectLookupResult> FindPortableRuntimeObject(const PortableObjectLookupRequest& request);
