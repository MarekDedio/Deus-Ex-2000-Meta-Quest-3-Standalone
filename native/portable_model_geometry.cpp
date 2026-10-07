#include "Precomp.h"
#include "portable_model_geometry.h"
#include "surreal_portable_package_tables.h"
#include "Utils/File.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
constexpr std::size_t MaxPayloadBytes = 256u * 1024u * 1024u;
constexpr std::size_t MaxDecodedBytes = 256u * 1024u * 1024u;
constexpr std::size_t MaxRecords = 1'000'000u;
constexpr std::size_t MaxVertices = 2'000'000u;
constexpr std::size_t MaxLightBits = 128u * 1024u * 1024u;
constexpr std::size_t MaxLightMapAxis = 16'384u;
constexpr std::size_t MaxLightMapSamples = 16u * 1024u * 1024u;

[[noreturn]] void Fail(const std::string& detail) {
    throw std::runtime_error("UE1 model v68: " + detail);
}
void RequireLayout(std::uint16_t version, std::uint16_t licenseeMode) {
    if (version != 68u || licenseeMode != 0u)
        Fail("unsupported package version/licensee layout");
}
std::size_t Multiply(std::size_t left, std::size_t right, const char* label) {
    if (right != 0u && left > std::numeric_limits<std::size_t>::max() / right)
        Fail(std::string(label) + " size overflow");
    return left * right;
}
void ValidateReference(std::int32_t reference, std::size_t imports,
                       std::size_t exports) {
    if (reference > 0 && static_cast<std::uint64_t>(reference) > exports)
        Fail("object reference exceeds export table");
    if (reference < 0 && static_cast<std::uint64_t>(
            -static_cast<std::int64_t>(reference)) > imports)
        Fail("object reference exceeds import table");
}

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {
        if (bytes.size() > MaxPayloadBytes) Fail("payload exceeds 256 MiB bound");
    }
    std::size_t Position() const { return position_; }
    std::size_t Remaining() const { return bytes_.size() - position_; }
    std::uint8_t U8() { Require(1u); return bytes_[position_++]; }
    std::uint16_t U16() {
        const std::uint16_t lo = U8();
        return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(U8()) << 8u));
    }
    std::uint32_t U32() {
        const std::uint32_t lo = U16();
        return lo | (static_cast<std::uint32_t>(U16()) << 16u);
    }
    std::uint64_t U64() {
        const std::uint64_t lo = U32();
        return lo | (static_cast<std::uint64_t>(U32()) << 32u);
    }
    std::int32_t I32() { return static_cast<std::int32_t>(U32()); }
    std::int16_t I16() { return static_cast<std::int16_t>(U16()); }
    float Float() {
        const std::uint32_t bits = U32();
        float value{};
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) Fail("nonfinite floating-point field");
        return value;
    }
    PortableModelVec3 Vector() { return {Float(), Float(), Float()}; }
    std::int32_t Index() {
        std::uint8_t value = U8();
        const bool negative = (value & 0x80u) != 0u;
        bool more = (value & 0x40u) != 0u;
        std::uint64_t magnitude = value & 0x3fu;
        unsigned shift = 6u;
        while (more && shift < 32u) {
            value = U8();
            magnitude |= static_cast<std::uint64_t>(value & 0x7fu) << shift;
            more = (value & 0x80u) != 0u;
            shift += 7u;
        }
        if (more || magnitude > static_cast<std::uint64_t>(
                std::numeric_limits<std::int32_t>::max()))
            Fail("overflowing compact index");
        return negative ? -static_cast<std::int32_t>(magnitude)
                        : static_cast<std::int32_t>(magnitude);
    }
    std::size_t Count(const char* label, std::size_t maximum,
                      std::size_t minimumBytes, std::size_t decodedSize) {
        const std::int32_t raw = Index();
        if (raw < 0 || static_cast<std::size_t>(raw) > maximum)
            Fail(std::string("invalid ") + label + " count");
        return CheckedCount(static_cast<std::size_t>(raw), label, minimumBytes, decodedSize);
    }
    std::size_t CheckedCount(std::size_t count, const char* label,
                             std::size_t minimumBytes, std::size_t decodedSize) {
        Require(Multiply(count, minimumBytes, label));
        const std::size_t allocation = Multiply(count, decodedSize, label);
        if (allocation > MaxDecodedBytes - decodedBytes_)
            Fail("decoded arrays exceed 256 MiB bound");
        decodedBytes_ += allocation;
        return count;
    }
    std::vector<std::uint8_t> Bytes(std::size_t count) {
        Require(count);
        std::vector<std::uint8_t> result(bytes_.begin() + position_,
                                        bytes_.begin() + position_ + count);
        position_ += count;
        return result;
    }
    void Skip(std::size_t count) { Require(count); position_ += count; }
    void String() {
        const std::int32_t units = Index();
        const std::uint64_t magnitude = units < 0
            ? static_cast<std::uint64_t>(-static_cast<std::int64_t>(units))
            : static_cast<std::uint64_t>(units);
        if (magnitude > 65'536u) Fail("Level URL string is too long");
        if (magnitude == 0u) return;
        const std::size_t byteCount = Multiply(static_cast<std::size_t>(magnitude),
                                              units < 0 ? 2u : 1u, "URL string");
        Require(byteCount);
        const std::size_t last = position_ + byteCount - (units < 0 ? 2u : 1u);
        if (bytes_[last] != 0u || (units < 0 && bytes_[last + 1u] != 0u))
            Fail("Level URL string is not terminated");
        Skip(byteCount);
    }
private:
    void Require(std::size_t count) const {
        if (count > Remaining()) Fail("serialized section is truncated");
    }
    const std::vector<std::uint8_t>& bytes_;
    std::size_t position_{}, decodedBytes_{};
};

PortableModelBounds ReadBounds(Reader& reader) {
    PortableModelBounds bounds;
    bounds.minimum = reader.Vector();
    bounds.maximum = reader.Vector();
    const std::uint8_t valid = reader.U8();
    if (valid > 1u) Fail("invalid bounding-box validity byte");
    bounds.valid = valid != 0u;
    if (bounds.valid && (bounds.minimum.x > bounds.maximum.x ||
                        bounds.minimum.y > bounds.maximum.y ||
                        bounds.minimum.z > bounds.maximum.z))
        Fail("bounding-box minimum exceeds maximum");
    return bounds;
}
void ValidateIndex(std::int32_t index, std::size_t count, const char* label,
                   bool allowNone = false) {
    if (allowNone && index == -1) return;
    if (index < 0 || static_cast<std::uint64_t>(index) >= count)
        Fail(std::string("invalid ") + label + " index");
}
void ValidateGeometry(const PortableModelGeometry& model) {
    // UE1 serializes the entire vertex pool, including unused/free slots whose
    // point indexes may equal or exceed Points.size(). Preserve those exact
    // records, but require every node-addressed slot to reference a real point.
    // Training and Intro both contain such unused point==pointCount slots.
    std::vector<std::uint8_t> referencedVertices(model.vertices.size(), 0u);
    for (const auto& node : model.nodes) {
        if (node.vertexCount == 0u) continue;
        if (node.vertexPool < 0 || static_cast<std::uint64_t>(node.vertexPool) +
                node.vertexCount > model.vertices.size())
            Fail("node vertex pool exceeds array");
        const std::size_t start = static_cast<std::size_t>(node.vertexPool);
        std::fill(referencedVertices.begin() + start,
                  referencedVertices.begin() + start + node.vertexCount, 1u);
    }
    for (std::size_t index = 0u; index < model.vertices.size(); ++index) {
        const auto& vertex = model.vertices[index];
        if (referencedVertices[index] != 0u &&
            (vertex.point < 0 || static_cast<std::uint64_t>(vertex.point) >= model.points.size()))
            Fail("invalid vertex point index: vertex=" + std::to_string(index) +
                 " point=" + std::to_string(vertex.point) +
                 " pointCount=" + std::to_string(model.points.size()));
        if (referencedVertices[index] != 0u && vertex.side < -1)
            Fail("invalid vertex shared-side index");
    }
    for (const auto& surface : model.surfaces) {
        ValidateIndex(surface.basePoint, model.points.size(), "surface base point");
        ValidateIndex(surface.normalVector, model.vectors.size(), "surface normal");
        ValidateIndex(surface.textureU, model.vectors.size(), "surface texture U");
        ValidateIndex(surface.textureV, model.vectors.size(), "surface texture V");
        ValidateIndex(surface.lightMap, model.lightMaps.size(), "surface lightmap", true);
        if (surface.brushPoly < -1) Fail("invalid surface brush-polygon index");
    }
    for (const auto& node : model.nodes) {
        ValidateIndex(node.surface, model.surfaces.size(), "node surface", true);
        ValidateIndex(node.back, model.nodes.size(), "node back", true);
        ValidateIndex(node.front, model.nodes.size(), "node front", true);
        ValidateIndex(node.plane, model.nodes.size(), "node plane", true);
        if (node.zone0 < 0 || node.zone0 >= 64 || node.zone1 < 0 || node.zone1 >= 64)
            Fail("node zone exceeds 64 engine slots");
        if (node.vertexCount != 0u) {
            if (node.vertexPool < 0 || static_cast<std::uint64_t>(node.vertexPool) +
                    node.vertexCount > model.vertices.size())
                Fail("node vertex pool exceeds array");
            if (node.surface < 0) Fail("node with vertices has no surface");
        }
        ValidateIndex(node.collisionBound, model.leafHulls.size(), "node collision bound", true);
        ValidateIndex(node.renderBound, model.renderBounds.size(), "node render bound", true);
        ValidateIndex(node.leaf0, model.leaves.size(), "node leaf 0", true);
        ValidateIndex(node.leaf1, model.leaves.size(), "node leaf 1", true);
    }
    for (const auto& leaf : model.leaves) {
        if (leaf.zone < 0 || leaf.zone >= 64) Fail("leaf zone exceeds 64 engine slots");
        if (leaf.permeating < -1 || leaf.volumetric < -1)
            Fail("invalid leaf light-list index");
    }
    // Compute each possible list's terminator in one reverse pass. A malformed
    // or adversarial model may reuse a long list for many lightmaps; scanning
    // each list independently here would turn a bounded payload into quadratic
    // parsing time. Scratch memory is at most MaxVertices*sizeof(size_t).
    const std::size_t noTerminator = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> terminators(model.lights.size(), noTerminator);
    std::size_t nextTerminator = noTerminator;
    for (std::size_t index = model.lights.size(); index > 0u; --index) {
        const std::size_t slot = index - 1u;
        if (model.lights[slot] == 0) nextTerminator = slot;
        terminators[slot] = nextTerminator;
    }
    for (std::size_t index = 0; index < model.lightMaps.size(); ++index) {
        const auto& lm = model.lightMaps[index];
        if (lm.uClamp <= 0 || lm.vClamp <= 0 ||
            static_cast<std::size_t>(lm.uClamp) > MaxLightMapAxis ||
            static_cast<std::size_t>(lm.vClamp) > MaxLightMapAxis ||
            Multiply(static_cast<std::size_t>(lm.uClamp),
                     static_cast<std::size_t>(lm.vClamp), "lightmap") > MaxLightMapSamples)
            Fail("invalid/bounded lightmap dimensions");
        if (lm.uScale <= 0.0f || lm.vScale <= 0.0f)
            Fail("lightmap scale must be positive");
        if (lm.dataOffset < -1 || lm.lightActors < -1)
            Fail("invalid lightmap data/list offset");
        std::size_t count{};
        if (lm.lightActors != -1) {
            if (lm.lightActors < 0 || static_cast<std::uint64_t>(lm.lightActors) >= model.lights.size())
                Fail("static-light list starts outside array");
            const std::size_t start = static_cast<std::size_t>(lm.lightActors);
            if (terminators[start] == noTerminator) Fail("static-light list has no zero terminator");
            count = terminators[start] - start;
        }
        if (count != 0u) {
            if (lm.dataOffset < 0) Fail("static-light list has no shadow-data offset");
            const std::size_t pitch = (static_cast<std::size_t>(lm.uClamp) + 7u) / 8u;
            const std::size_t bytesPerLight = Multiply(pitch, static_cast<std::size_t>(lm.vClamp), "shadow mask");
            const std::size_t totalBytes = Multiply(bytesPerLight, count, "ordered shadow masks");
            const std::size_t offset = static_cast<std::size_t>(lm.dataOffset);
            if (offset > model.lightBits.size() || totalBytes > model.lightBits.size() - offset)
                Fail("ordered shadow masks end beyond LightBits");
        }
    }
}
bool LeafClassEquals(const PortablePackageTables& package,
                     std::int32_t reference, const char* expected) {
    std::string name = GetPortableObjectPath(package, reference);
    const std::size_t separator = name.find_last_of('.');
    if (separator != std::string::npos) name.erase(0u, separator + 1u);
    const std::string expectedName(expected);
    if (name.size() != expectedName.size()) return false;
    for (std::size_t index = 0; index < name.size(); ++index) {
        const auto fold = [](unsigned char value) {
            return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
        };
        if (fold(static_cast<unsigned char>(name[index])) !=
            fold(static_cast<unsigned char>(expectedName[index]))) return false;
    }
    return true;
}
std::vector<std::uint8_t> ReadExportBody(const PortablePackageTables& package,
                                       std::size_t exportIndex, const char* className) {
    RequireLayout(package.version, package.licenseeMode);
    if (exportIndex >= package.exports.size()) Fail("export index is outside table");
    const auto& entry = package.exports[exportIndex];
    if (!LeafClassEquals(package, entry.ObjClass, className))
        Fail(std::string("export is not a ") + className);
    if (entry.ObjSize <= 0 || entry.ObjOffset < 0 ||
        static_cast<std::uint64_t>(entry.ObjSize) > MaxPayloadBytes)
        Fail("invalid/bounded export payload");
    const auto file = File::open_existing(package.sourcePath);
    const std::int64_t fileSize = file->size();
    if (fileSize < 0 || static_cast<std::uint64_t>(entry.ObjOffset) +
            static_cast<std::uint64_t>(entry.ObjSize) > static_cast<std::uint64_t>(fileSize))
        Fail("export payload exceeds file");
    const auto properties = LoadPortableExportProperties(package, exportIndex);
    if (properties.bytesConsumed > static_cast<std::uint32_t>(entry.ObjSize))
        Fail("property body boundary exceeds export");
    const std::size_t remaining = static_cast<std::size_t>(entry.ObjSize) - properties.bytesConsumed;
    std::vector<std::uint8_t> bytes(remaining);
    file->seek(static_cast<std::int64_t>(entry.ObjOffset) + properties.bytesConsumed);
    if (!bytes.empty()) file->read(bytes.data(), bytes.size());
    return bytes;
}
}

std::size_t GetPortableModelStaticLightCount(const PortableModelGeometry& model,
                                            std::size_t lightMapIndex) {
    if (lightMapIndex >= model.lightMaps.size()) Fail("lightmap index exceeds array");
    const std::int32_t start = model.lightMaps[lightMapIndex].lightActors;
    if (start == -1) return 0u;
    if (start < 0 || static_cast<std::uint64_t>(start) >= model.lights.size())
        Fail("static-light list starts outside array");
    const std::size_t first = static_cast<std::size_t>(start);
    for (std::size_t index = first; index < model.lights.size(); ++index) {
        if (model.lights[index] == 0) return index - first;
    }
    Fail("static-light list has no zero terminator");
}

PortableModelShadowSpan GetPortableModelShadowSpan(const PortableModelGeometry& model,
                                                  std::size_t lightMapIndex,
                                                  std::size_t lightOrdinal) {
    const std::size_t count = GetPortableModelStaticLightCount(model, lightMapIndex);
    if (lightOrdinal >= count) Fail("static-light ordinal exceeds ordered list");
    const auto& lm = model.lightMaps[lightMapIndex];
    if (lm.dataOffset < 0 || lm.uClamp <= 0 || lm.vClamp <= 0)
        Fail("invalid shadow mask dimensions/offset");
    PortableModelShadowSpan span;
    span.width = static_cast<std::size_t>(lm.uClamp);
    span.height = static_cast<std::size_t>(lm.vClamp);
    if (span.width > MaxLightMapAxis || span.height > MaxLightMapAxis)
        Fail("shadow dimensions exceed bound");
    span.pitch = (span.width + 7u) / 8u;
    span.byteCount = Multiply(span.pitch, span.height, "shadow mask");
    const std::size_t ordinalOffset = Multiply(lightOrdinal, span.byteCount, "shadow mask ordinal");
    const std::size_t baseOffset = static_cast<std::size_t>(lm.dataOffset);
    if (baseOffset > model.lightBits.size() || ordinalOffset > model.lightBits.size() - baseOffset)
        Fail("shadow mask starts beyond LightBits");
    span.offset = baseOffset + ordinalOffset;
    if (span.byteCount > model.lightBits.size() - span.offset)
        Fail("shadow mask ends beyond LightBits");
    return span;
}

PortableModelGeometry DecodePortableModel68Body(const std::vector<std::uint8_t>& bytes,
                                               std::size_t importCount,
                                               std::size_t exportCount,
                                               std::uint16_t version,
                                               std::uint16_t licenseeMode) {
    RequireLayout(version, licenseeMode);
    Reader reader(bytes);
    PortableModelGeometry model;
    model.version = version;
    model.licenseeMode = licenseeMode;
    model.bounds = ReadBounds(reader);
    model.sphere = {reader.Float(), reader.Float(), reader.Float(), reader.Float()};
    const auto vectors = [&](std::vector<PortableModelVec3>& target, const char* label) {
        const std::size_t count = reader.Count(label, MaxRecords, 12u, sizeof(PortableModelVec3));
        target.reserve(count);
        for (std::size_t index = 0; index < count; ++index) target.push_back(reader.Vector());
    };
    vectors(model.vectors, "vector");
    vectors(model.points, "point");
    const std::size_t nodeCount = reader.Count("node", MaxRecords, 43u, sizeof(PortableModelNode));
    model.nodes.reserve(nodeCount);
    for (std::size_t index = 0; index < nodeCount; ++index) {
        PortableModelNode node;
        node.planeX = reader.Float(); node.planeY = reader.Float();
        node.planeZ = reader.Float(); node.planeW = reader.Float();
        node.zoneMask = reader.U64(); node.nodeFlags = reader.U8();
        node.vertexPool = reader.Index(); node.surface = reader.Index();
        node.back = reader.Index(); node.front = reader.Index(); node.plane = reader.Index();
        node.collisionBound = reader.Index(); node.renderBound = reader.Index();
        node.zone0 = reader.Index(); node.zone1 = reader.Index();
        node.vertexCount = reader.U8(); node.leaf0 = reader.I32(); node.leaf1 = reader.I32();
        model.nodes.push_back(node);
    }
    const std::size_t surfaceCount = reader.Count("surface", MaxRecords, 16u, sizeof(PortableModelSurface));
    model.surfaces.reserve(surfaceCount);
    for (std::size_t index = 0; index < surfaceCount; ++index) {
        PortableModelSurface surface;
        surface.materialReference = reader.Index();
        ValidateReference(surface.materialReference, importCount, exportCount);
        surface.polyFlags = reader.U32(); surface.basePoint = reader.Index();
        surface.normalVector = reader.Index(); surface.textureU = reader.Index();
        surface.textureV = reader.Index(); surface.lightMap = reader.Index();
        surface.brushPoly = reader.Index(); surface.panU = reader.I16(); surface.panV = reader.I16();
        surface.brushActorReference = reader.Index();
        ValidateReference(surface.brushActorReference, importCount, exportCount);
        model.surfaces.push_back(surface);
    }
    const std::size_t vertexCount = reader.Count("vertex", MaxVertices, 2u, sizeof(PortableModelVertex));
    model.vertices.reserve(vertexCount);
    for (std::size_t index = 0; index < vertexCount; ++index)
        model.vertices.push_back({reader.Index(), reader.Index()});
    model.sharedSides = reader.I32();
    if (model.sharedSides < 0) Fail("negative shared-side count");
    const std::int32_t zoneCount = reader.I32(); // int32, NOT a compact count in Load.
    if (zoneCount < 0 || zoneCount > 64) Fail("invalid zone count");
    reader.CheckedCount(static_cast<std::size_t>(zoneCount), "zone", 17u, sizeof(PortableModelZone));
    model.zones.reserve(static_cast<std::size_t>(zoneCount));
    for (std::int32_t index = 0; index < zoneCount; ++index) {
        PortableModelZone zone;
        zone.actorReference = reader.Index();
        ValidateReference(zone.actorReference, importCount, exportCount);
        zone.connectivity = reader.U64(); zone.visibility = reader.U64();
        model.zones.push_back(zone);
    }
    model.polysReference = reader.Index();
    ValidateReference(model.polysReference, importCount, exportCount);
    const std::size_t lightMapCount = reader.Count("lightmap", MaxRecords, 30u, sizeof(PortableModelLightMapIndex));
    model.lightMaps.reserve(lightMapCount);
    for (std::size_t index = 0; index < lightMapCount; ++index) {
        PortableModelLightMapIndex lm;
        lm.dataOffset = reader.I32(); lm.panX = reader.Float();
        lm.panY = reader.Float(); lm.panZ = reader.Float();
        lm.uClamp = reader.Index(); lm.vClamp = reader.Index();
        lm.uScale = reader.Float(); lm.vScale = reader.Float(); lm.lightActors = reader.I32();
        model.lightMaps.push_back(lm);
    }
    model.lightBits = reader.Bytes(reader.Count("LightBits", MaxLightBits, 1u, 1u));
    const std::size_t boundCount = reader.Count("render bound", MaxRecords, 25u, sizeof(PortableModelBounds));
    model.renderBounds.reserve(boundCount);
    for (std::size_t index = 0; index < boundCount; ++index) model.renderBounds.push_back(ReadBounds(reader));
    const std::size_t hullCount = reader.Count("leaf hull", MaxVertices, 4u, sizeof(std::int32_t));
    model.leafHulls.reserve(hullCount);
    for (std::size_t index = 0; index < hullCount; ++index) model.leafHulls.push_back(reader.I32());
    const std::size_t leafCount = reader.Count("leaf", MaxRecords, 11u, sizeof(PortableModelLeaf));
    model.leaves.reserve(leafCount);
    for (std::size_t index = 0; index < leafCount; ++index) {
        PortableModelLeaf leaf;
        leaf.zone = reader.Index(); leaf.permeating = reader.Index(); leaf.volumetric = reader.Index();
        leaf.visibleZones = reader.U64(); model.leaves.push_back(leaf);
    }
    const std::size_t lightCount = reader.Count("light reference", MaxVertices, 1u, sizeof(std::int32_t));
    model.lights.reserve(lightCount);
    for (std::size_t index = 0; index < lightCount; ++index) {
        const std::int32_t reference = reader.Index();
        ValidateReference(reference, importCount, exportCount); model.lights.push_back(reference);
    }
    model.rootOutside = reader.I32(); model.linked = reader.I32();
    if (reader.Remaining() != 0u) Fail("trailing bytes after model tail");
    model.bodyBytesConsumed = reader.Position();
    ValidateGeometry(model);
    return model;
}

PortableModelGeometry LoadPortableModel68(const PortablePackageTables& package,
                                         std::size_t exportIndex) {
    const auto bytes = ReadExportBody(package, exportIndex, "Model");
    auto model = DecodePortableModel68Body(bytes, package.imports.size(), package.exports.size(),
                                          package.version, package.licenseeMode);
    model.objectPath = GetPortableObjectPath(package, static_cast<std::int32_t>(exportIndex + 1u));
    model.exportIndex = exportIndex;
    return model;
}

namespace {
struct LevelSelection {
    std::size_t modelIndex{};
    std::vector<std::int32_t> actorOrder;
};
LevelSelection ReadLevelSelection(const PortablePackageTables& package) {
    RequireLayout(package.version, package.licenseeMode);
    std::size_t levelIndex = std::numeric_limits<std::size_t>::max();
    for (std::size_t index = 0; index < package.exports.size(); ++index) {
        if (!LeafClassEquals(package, package.exports[index].ObjClass, "Level")) continue;
        if (levelIndex != std::numeric_limits<std::size_t>::max()) Fail("ambiguous Level exports");
        levelIndex = index;
    }
    if (levelIndex == std::numeric_limits<std::size_t>::max()) Fail("map has no Level export");
    const auto bytes = ReadExportBody(package, levelIndex, "Level");
    Reader reader(bytes);
    const std::int32_t actorCount = reader.I32();
    const std::int32_t actorCapacity = reader.I32();
    if (actorCount < 0 || actorCount > 100'000 || actorCapacity < actorCount)
        Fail("invalid Level actor count/capacity");
    reader.CheckedCount(static_cast<std::size_t>(actorCount), "Level actor", 1u, sizeof(std::int32_t));
    LevelSelection selection;
    selection.actorOrder.reserve(static_cast<std::size_t>(actorCount));
    for (std::int32_t index = 0; index < actorCount; ++index) {
        const std::int32_t reference = reader.Index();
        ValidateReference(reference, package.imports.size(), package.exports.size());
        selection.actorOrder.push_back(reference);
    }
    for (unsigned index = 0; index < 4u; ++index) reader.String();
    const std::size_t options = reader.Count("Level URL option", 1024u, 1u, 0u);
    for (std::size_t index = 0; index < options; ++index) reader.String();
    reader.Skip(8u); // URL Port and Unknown.
    const std::int32_t modelReference = reader.Index();
    if (modelReference <= 0 || static_cast<std::uint64_t>(modelReference) > package.exports.size())
        Fail("Level Model is not a local export reference");
    const std::size_t modelIndex = static_cast<std::size_t>(modelReference - 1);
    if (!LeafClassEquals(package, package.exports[modelIndex].ObjClass, "Model"))
        Fail("Level Model reference is not a Model");
    // ULevel has reach-spec data after this reference; it belongs to ULevel,
    // not UModel, and is deliberately left to the level decoder.
    selection.modelIndex = modelIndex;
    return selection;
}
}
std::size_t FindPortableRootModel68Export(const PortablePackageTables& package) {
    return ReadLevelSelection(package).modelIndex;
}
PortableModelGeometry LoadPortableRootModel68(const PortablePackageTables& package) {
    return LoadPortableModel68(package, FindPortableRootModel68Export(package));
}
std::vector<std::int32_t> ReadPortableLevel68ActorOrder(const PortablePackageTables& package) {
    return ReadLevelSelection(package).actorOrder;
}
