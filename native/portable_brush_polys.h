#pragma once
#include "portable_model_geometry.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct PortablePackageTables;
// Lossless SERIALIZED v68 FPoly fields. References are source package indices,
// NOT host UObject pointers or global FName indices. Unserialized vertex slots
// are portable zero storage, not proof of original constructor initialization.
// This is not a rendered triangle, native ABI object or a completed BSP tracker.
struct PortableBrushPoly {
    PortableModelVec3 base,normal,textureU,textureV;
    std::array<PortableModelVec3,16u> vertices{};
    std::uint32_t polyFlags{};
    std::int32_t actorReference{},textureReference{},itemNameReference{},vertexCount{},link{},brushPoly{};
    std::int16_t panU{},panV{};
};
static_assert(sizeof(PortableBrushPoly)==0x110u);
struct PortableBrushPolys {
    std::vector<PortableBrushPoly> polygons;
    std::int32_t serializedCapacity{};
    std::size_t bytesConsumed{};
    std::string sourcePath,objectPath;
    std::size_t exportIndex{};
};
// Body starts immediately after the UObject tagged-property sentinel. Exact
// consumption, count/byte/ref/name bounds, native16-vertex capacity and finite
// vectors are checked BEFORE publishing a complete detached result.
PortableBrushPolys DecodePortableBrushPolys68Body(std::span<const std::uint8_t> body,
    std::size_t importCount,std::size_t exportCount,std::size_t nameCount,
    std::size_t byteLimit=32u<<20u);
PortableBrushPolys LoadPortableBrushPolys68(const PortablePackageTables& package,
    std::size_t polysExportIndex,std::size_t byteLimit=32u<<20u);
