#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

struct PortablePackageTables;

// Exact serialized UModel records for Deus Ex package version 68/licensee 0.
// Coordinates remain in authored Unreal units. Object references are signed
// package references, not runtime pointers. No renderer/GC/OVR dependencies.
struct PortableModelVec3 { float x{}, y{}, z{}; };
struct PortableModelBounds {
    PortableModelVec3 minimum, maximum;
    bool valid{};
    // Exact serialized byte, including values other than 0/1 in collision-only
    // decode. Appended so existing {minimum,maximum,bool} initializers retain
    // their meaning. `valid` remains the convenient nonzero projection.
    std::uint8_t rawValid{};
};
struct PortableModelSphere { float x{}, y{}, z{}, radius{}; };

struct PortableModelNode {
    float planeX{}, planeY{}, planeZ{}, planeW{};
    std::uint64_t zoneMask{};
    std::uint8_t nodeFlags{};
    std::int32_t vertexPool{}, surface{}, back{}, front{}, plane{};
    std::int32_t collisionBound{}, renderBound{}, zone0{}, zone1{};
    std::uint8_t vertexCount{};
    std::int32_t leaf0{}, leaf1{};
};
struct PortableModelSurface {
    std::int32_t materialReference{};
    std::uint32_t polyFlags{};
    std::int32_t basePoint{}, normalVector{}, textureU{}, textureV{};
    std::int32_t lightMap{}, brushPoly{};
    std::int16_t panU{}, panV{};
    std::int32_t brushActorReference{};
};
struct PortableModelVertex { std::int32_t point{}, side{}; };
struct PortableModelZone {
    std::int32_t actorReference{};
    std::uint64_t connectivity{}, visibility{};
};
struct PortableModelLightMapIndex {
    std::int32_t dataOffset{};
    float panX{}, panY{}, panZ{};
    std::int32_t uClamp{}, vClamp{};
    float uScale{}, vScale{};
    // Index into model.lights; each ordered list ends with reference zero.
    // The list's ordinal selects the corresponding per-light shadow mask.
    std::int32_t lightActors{};
};
struct PortableModelLeaf {
    std::int32_t zone{}, permeating{}, volumetric{};
    std::uint64_t visibleZones{};
};
struct PortableModelGeometry {
    std::string objectPath;
    std::size_t exportIndex{std::numeric_limits<std::size_t>::max()};
    std::uint16_t version{68}, licenseeMode{};
    PortableModelBounds bounds;
    PortableModelSphere sphere;
    std::vector<PortableModelVec3> vectors, points;
    std::vector<PortableModelNode> nodes;
    std::vector<PortableModelSurface> surfaces;
    std::vector<PortableModelVertex> vertices;
    std::int32_t sharedSides{};
    // Keep the actual serialized count. Zone indexes can address any of the
    // engine's 64 slots: slots >= zones.size() are null/LevelInfo fallback.
    std::vector<PortableModelZone> zones;
    std::int32_t polysReference{};
    std::vector<PortableModelLightMapIndex> lightMaps;
    std::vector<std::uint8_t> lightBits;
    std::vector<PortableModelBounds> renderBounds;
    std::vector<std::int32_t> leafHulls;
    std::vector<PortableModelLeaf> leaves;
    // Preserve duplicate references and zero terminators. Never deduplicate
    // this array: doing so associates authored shadow masks with wrong lights.
    std::vector<std::int32_t> lights;
    std::int32_t rootOutside{}, linked{};
    std::size_t bodyBytesConsumed{};
};

struct PortableModelShadowSpan {
    std::size_t offset{}, pitch{}, width{}, height{}, byteCount{};
};

// The byte API starts immediately AFTER the tagged-property None sentinel,
// includes the 41-byte UPrimitive prefix, and must consume the exact body.
// Throws std::runtime_error on unsupported layouts or malformed/bounded data.
PortableModelGeometry DecodePortableModel68Body(
    const std::vector<std::uint8_t>& bytes,
    std::size_t importCount,
    std::size_t exportCount,
    std::uint16_t version = 68,
    std::uint16_t licenseeMode = 0);
// Collision-only structural admission of the SAME serialized v68 layout.
// Preserves every Float bit (including inactive NaNs), raw box Valid byte and
// reversed endpoints. Does not validate render/lightmap relationships or
// numeric/query-index data; the consuming collision kernel validates active
// fields. Counts, payload, actual object-reference ranges and exact tail stay
// bounded. decodedWorkingBytes limits retained ARRAY capacities, independently
// of caller-owned source bytes/property scratch; fixed value/container/string
// overhead belongs to the caller's aggregate cache ledger.
PortableModelGeometry DecodePortableCollisionModel68Body(
    const std::vector<std::uint8_t>& bytes,
    std::size_t importCount,
    std::size_t exportCount,
    std::uint16_t version = 68,
    std::uint16_t licenseeMode = 0,
    std::size_t decodedWorkingBytes = 256u * 1024u * 1024u);
// propertyWorkingBytes bounds property decoding and native body-copy scratch;
// the native geometry decoder retains its separate fixed decoded-array bounds.
PortableModelGeometry LoadPortableModel68(
    const PortablePackageTables& package, std::size_t exportIndex,
    std::size_t propertyWorkingBytes = 512u * 1024u * 1024u);
// Same export/property boundary checks as the rendering loader, then the
// collision-only body decoder. This is data admission, not UObject identity,
// native bounds/LineCheck permission or a renderer geometry fallback.
PortableModelGeometry LoadPortableCollisionModel68(
    const PortablePackageTables& package, std::size_t exportIndex,
    std::size_t propertyWorkingBytes = 512u * 1024u * 1024u,
    std::size_t decodedWorkingBytes = 256u * 1024u * 1024u);
// Selects ULevel's serialized Model reference, not the first Model export
// (maps also contain many brush/mover Models).
std::size_t FindPortableRootModel68Export(const PortablePackageTables& package,
    std::size_t propertyWorkingBytes = 512u * 1024u * 1024u);
PortableModelGeometry LoadPortableRootModel68(const PortablePackageTables& package,
    std::size_t propertyWorkingBytes = 512u * 1024u * 1024u);
// Exact serialized ULevel actor order, including zero references. This lets
// cache/lighting adapters select the same first authored PlayerStart.
std::vector<std::int32_t> ReadPortableLevel68ActorOrder(const PortablePackageTables& package,
    std::size_t propertyWorkingBytes = 512u * 1024u * 1024u);

// These validate the requested list/span even if callers modified the public
// records after decoding. Widths not divisible by eight retain padded row pitch.
std::size_t GetPortableModelStaticLightCount(
    const PortableModelGeometry& model, std::size_t lightMapIndex);
PortableModelShadowSpan GetPortableModelShadowSpan(
    const PortableModelGeometry& model, std::size_t lightMapIndex,
    std::size_t lightOrdinal);
