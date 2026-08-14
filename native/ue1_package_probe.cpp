#include <jni.h>
#include <android/log.h>

#include "Utils/Array.h"
#include "Package/NameString.h"
#include "Package/PackageStream.h"
#include "Utils/File.h"
#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr const char* kLogTag = "DeusExQuest";
constexpr std::uint32_t kUe1PackageSignature = 0x9E2A83C1u;

struct ExportRecord {
    std::int32_t classReference{};
    std::int32_t outerReference{};
    std::int32_t nameIndex{};
    std::int32_t size{};
    std::int32_t offset{-1};
    std::int32_t flags{};
};

struct ImportRecord {
    std::int32_t classPackage{};
    std::int32_t className{};
    std::int32_t outerReference{};
    std::int32_t nameIndex{};
};

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

struct BspNodeGeometry {
    std::int32_t vertexPool{};
    std::int32_t surface{};
    std::uint8_t vertexCount{};
};

struct BspVertexGeometry {
    std::int32_t point{};
};

struct BspSurfaceGeometry {
    std::int32_t material{};
    std::uint32_t flags{};
    std::int32_t basePoint{};
    std::int32_t textureU{};
    std::int32_t textureV{};
    std::int16_t panU{};
    std::int16_t panV{};
};

struct MeshVertex {
    Vec3 position;
    Vec3 normal;
    float u{};
    float v{};
    std::int32_t materialSlot{-1};
};

struct PackageSummary {
    std::uint16_t version{};
    std::uint16_t licenseeMode{};
    std::uint32_t flags{};
    std::uint32_t nameCount{};
    std::uint32_t nameOffset{};
    std::uint32_t exportCount{};
    std::uint32_t exportOffset{};
    std::uint32_t importCount{};
    std::uint32_t importOffset{};
    std::uint32_t namesParsed{};
    std::uint32_t importsParsed{};
    std::uint32_t exportsParsed{};
    std::string firstName;
    std::string firstExportName;
    std::string firstExportClass;
    std::int32_t firstExportClassReference{};
    std::int32_t firstExportFlags{};
    std::int32_t firstExportSize{};
    std::int32_t firstExportOffset{-1};
    std::uint32_t firstExportHash{};
    std::uint32_t firstExportPropertyCount{};
    std::uint32_t firstExportPropertyBytes{};
    std::string firstExportProperty;
    std::vector<std::string> names;
    std::vector<std::string> importObjectNames;
    std::vector<ImportRecord> imports;
    std::vector<ExportRecord> exports;
    std::uint32_t levelExportCount{};
    std::uint32_t modelExportCount{};
    std::string levelName;
    std::int32_t levelSize{};
    std::int32_t levelOffset{-1};
    std::string firstModelName;
    std::int32_t firstModelSize{};
    std::int32_t firstModelOffset{-1};
    std::uint32_t rootModelCount{};
    std::string rootModelName;
    std::int32_t rootModelSize{};
    std::int32_t rootModelOffset{-1};
    std::uint32_t vectorCount{};
    std::uint32_t pointCount{};
    std::uint32_t bspNodeCount{};
    std::uint32_t surfaceCount{};
    std::uint32_t vertexCount{};
    std::uint32_t zoneCount{};
    Vec3 boundsMin;
    Vec3 boundsMax;
    std::vector<Vec3> points;
    std::vector<Vec3> vectors;
    std::vector<BspNodeGeometry> bspNodes;
    std::vector<std::uint32_t> surfaceFlags;
    std::vector<std::int32_t> surfaceMaterials;
    std::vector<BspSurfaceGeometry> bspSurfaces;
    std::vector<std::string> materialNames;
    std::vector<BspVertexGeometry> bspVertices;
    std::vector<std::int32_t> levelActorReferences;
    std::uint32_t actorCount{};
    std::string playerStartName;
    std::string playerStartClass;
    std::int32_t playerStartReference{};
    Vec3 playerStartLocation;
    bool playerStartLocationValid{};
};

bool ReadExact(std::FILE* file, void* output, std::size_t bytes) {
    return std::fread(output, 1, bytes, file) == bytes;
}

bool ReadU16(std::FILE* file, std::uint16_t& output) {
    std::uint8_t bytes[2]{};
    if (!ReadExact(file, bytes, sizeof(bytes))) {
        return false;
    }
    output = static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << 8u);
    return true;
}

bool ReadU32(std::FILE* file, std::uint32_t& output) {
    std::uint8_t bytes[4]{};
    if (!ReadExact(file, bytes, sizeof(bytes))) {
        return false;
    }
    output = static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8u) |
        (static_cast<std::uint32_t>(bytes[2]) << 16u) |
        (static_cast<std::uint32_t>(bytes[3]) << 24u);
    return true;
}

bool ReadI32(std::FILE* file, std::int32_t& output) {
    std::uint32_t bits{};
    if (!ReadU32(file, bits)) {
        return false;
    }
    output = static_cast<std::int32_t>(bits);
    return true;
}

bool ReadI16(std::FILE* file, std::int16_t& output) {
    std::uint16_t bits{};
    if (!ReadU16(file, bits)) return false;
    output = static_cast<std::int16_t>(bits);
    return true;
}

bool ReadFloat(std::FILE* file, float& output) {
    std::uint32_t bits{};
    if (!ReadU32(file, bits)) return false;
    std::memcpy(&output, &bits, sizeof(output));
    return true;
}

bool ReadCompactIndex(std::FILE* file, std::int32_t& output) {
    std::uint8_t value{};
    if (!ReadExact(file, &value, 1)) {
        return false;
    }

    const bool negative = (value & 0x80u) != 0;
    std::uint32_t magnitude = value & 0x3fu;
    bool more = (value & 0x40u) != 0;
    unsigned shift = 6;
    while (more && shift < 32) {
        if (!ReadExact(file, &value, 1)) {
            return false;
        }
        magnitude |= static_cast<std::uint32_t>(value & 0x7fu) << shift;
        more = (value & 0x80u) != 0;
        shift += 7;
    }
    if (more || magnitude > 0x7fffffffu) {
        return false;
    }
    output = negative ? -static_cast<std::int32_t>(magnitude)
                      : static_cast<std::int32_t>(magnitude);
    return true;
}

bool ReadNameTable(std::FILE* file, PackageSummary& summary) {
    if (std::fseek(file, static_cast<long>(summary.nameOffset), SEEK_SET) != 0) {
        return false;
    }

    for (std::uint32_t index = 0; index < summary.nameCount; ++index) {
        std::int32_t serializedLength{};
        if (!ReadCompactIndex(file, serializedLength) ||
            serializedLength <= 0 || serializedLength > 4096) {
            return false;
        }

        std::vector<char> bytes(static_cast<std::size_t>(serializedLength));
        std::uint32_t flags{};
        if (!ReadExact(file, bytes.data(), bytes.size()) ||
            bytes.back() != '\0' || !ReadU32(file, flags)) {
            return false;
        }
        summary.names.emplace_back(bytes.data(), bytes.size() - 1);
        if (index == 0) summary.firstName = summary.names.back();
        ++summary.namesParsed;
    }
    return summary.namesParsed == summary.nameCount && !summary.firstName.empty();
}

bool IsNameIndexValid(std::int32_t value, const PackageSummary& summary) {
    return value >= 0 && static_cast<std::uint32_t>(value) < summary.nameCount;
}

bool IsObjectReferenceValid(std::int32_t value, const PackageSummary& summary) {
    return value == 0 ||
        (value > 0 && static_cast<std::uint32_t>(value) <= summary.exportCount) ||
        (value < 0 && static_cast<std::uint64_t>(-static_cast<std::int64_t>(value)) <=
            summary.importCount);
}

bool ReadImportTable(std::FILE* file, PackageSummary& summary) {
    if (std::fseek(file, static_cast<long>(summary.importOffset), SEEK_SET) != 0) {
        return false;
    }
    for (std::uint32_t index = 0; index < summary.importCount; ++index) {
        std::int32_t classPackage{}, className{}, objectOuter{}, objectName{};
        if (!ReadCompactIndex(file, classPackage) ||
            !ReadCompactIndex(file, className) ||
            !ReadI32(file, objectOuter) ||
            !ReadCompactIndex(file, objectName) ||
            !IsNameIndexValid(classPackage, summary) ||
            !IsNameIndexValid(className, summary) ||
            !IsNameIndexValid(objectName, summary) ||
            !IsObjectReferenceValid(objectOuter, summary)) {
            return false;
        }
        summary.importObjectNames.push_back(summary.names[objectName]);
        summary.imports.push_back({classPackage, className, objectOuter, objectName});
        ++summary.importsParsed;
    }
    return summary.importsParsed == summary.importCount;
}

std::string ResolveObjectPath(
    std::int32_t reference,
    const PackageSummary& summary,
    int depth = 0) {
    if (reference == 0 || depth > 32) return {};
    std::int32_t outer{};
    std::int32_t nameIndex{-1};
    if (reference > 0) {
        const std::size_t index = static_cast<std::size_t>(reference - 1);
        if (index >= summary.exports.size()) return {};
        outer = summary.exports[index].outerReference;
        nameIndex = summary.exports[index].nameIndex;
    } else {
        const std::size_t index = static_cast<std::size_t>(-static_cast<std::int64_t>(reference) - 1);
        if (index >= summary.imports.size()) return {};
        outer = summary.imports[index].outerReference;
        nameIndex = summary.imports[index].nameIndex;
    }
    if (!IsNameIndexValid(nameIndex, summary)) return {};
    const std::string prefix = ResolveObjectPath(outer, summary, depth + 1);
    return prefix.empty() ? summary.names[nameIndex] : prefix + "." + summary.names[nameIndex];
}

bool ReadExportTable(std::FILE* file, long fileLength, PackageSummary& summary) {
    if (std::fseek(file, static_cast<long>(summary.exportOffset), SEEK_SET) != 0) {
        return false;
    }
    for (std::uint32_t index = 0; index < summary.exportCount; ++index) {
        std::int32_t objectClass{}, objectBase{}, objectOuter{}, objectName{};
        std::int32_t objectFlags{}, objectSize{}, objectOffset{-1};
        if (!ReadCompactIndex(file, objectClass) ||
            !ReadCompactIndex(file, objectBase) ||
            !ReadI32(file, objectOuter) ||
            !ReadCompactIndex(file, objectName) ||
            !ReadI32(file, objectFlags) ||
            !ReadCompactIndex(file, objectSize) ||
            (objectSize > 0 && !ReadCompactIndex(file, objectOffset)) ||
            !IsObjectReferenceValid(objectClass, summary) ||
            !IsObjectReferenceValid(objectBase, summary) ||
            !IsObjectReferenceValid(objectOuter, summary) ||
            !IsNameIndexValid(objectName, summary) || objectSize < 0 ||
            (objectSize > 0 && (objectOffset < 0 ||
                static_cast<std::uint64_t>(objectOffset) + objectSize >
                    static_cast<std::uint64_t>(fileLength)))) {
            return false;
        }
        if (index == 0) {
            summary.firstExportName = summary.names[objectName];
            summary.firstExportSize = objectSize;
            summary.firstExportOffset = objectOffset;
            summary.firstExportClassReference = objectClass;
            summary.firstExportFlags = objectFlags;
            if (objectClass < 0) {
                const std::uint64_t importIndex =
                    static_cast<std::uint64_t>(-static_cast<std::int64_t>(objectClass) - 1);
                if (importIndex < summary.importObjectNames.size()) {
                    summary.firstExportClass = summary.importObjectNames[importIndex];
                }
            }
        }
        summary.exports.push_back(
            {objectClass, objectOuter, objectName, objectSize, objectOffset, objectFlags});
        ++summary.exportsParsed;
    }
    return summary.exportsParsed == summary.exportCount &&
        !summary.firstExportName.empty();
}

std::string ResolveExportClass(const ExportRecord& record, const PackageSummary& summary) {
    if (record.classReference < 0) {
        const std::uint64_t index = static_cast<std::uint64_t>(
            -static_cast<std::int64_t>(record.classReference) - 1);
        if (index < summary.importObjectNames.size()) return summary.importObjectNames[index];
    } else if (record.classReference > 0) {
        const std::uint64_t index = static_cast<std::uint64_t>(record.classReference - 1);
        if (index < summary.exports.size()) return summary.names[summary.exports[index].nameIndex];
    }
    return {};
}

void InventoryWorldExports(PackageSummary& summary) {
    std::int32_t levelReference{};
    for (std::size_t index = 0; index < summary.exports.size(); ++index) {
        const ExportRecord& record = summary.exports[index];
        const std::string className = ResolveExportClass(record, summary);
        if (className == "Level") {
            ++summary.levelExportCount;
            if (summary.levelName.empty()) {
                levelReference = static_cast<std::int32_t>(index + 1);
                summary.levelName = summary.names[record.nameIndex];
                summary.levelSize = record.size;
                summary.levelOffset = record.offset;
            }
        }
        if (className == "Model") {
            ++summary.modelExportCount;
            if (summary.firstModelName.empty()) {
                summary.firstModelName = summary.names[record.nameIndex];
                summary.firstModelSize = record.size;
                summary.firstModelOffset = record.offset;
            }
        }
    }

    if (levelReference != 0) {
        for (const ExportRecord& record : summary.exports) {
            if (record.outerReference == levelReference &&
                ResolveExportClass(record, summary) == "Model") {
                ++summary.rootModelCount;
                if (summary.rootModelName.empty()) {
                    summary.rootModelName = summary.names[record.nameIndex];
                    summary.rootModelSize = record.size;
                    summary.rootModelOffset = record.offset;
                }
            }
        }
    }
}

bool HashFirstExport(std::FILE* file, PackageSummary& summary) {
    if (summary.firstExportSize <= 0 || summary.firstExportOffset < 0 ||
        std::fseek(file, summary.firstExportOffset, SEEK_SET) != 0) {
        return false;
    }

    std::uint32_t hash = 2166136261u;
    std::int32_t remaining = summary.firstExportSize;
    std::uint8_t buffer[4096];
    while (remaining > 0) {
        const std::size_t chunk = static_cast<std::size_t>(
            remaining < static_cast<std::int32_t>(sizeof(buffer)) ? remaining : sizeof(buffer));
        if (!ReadExact(file, buffer, chunk)) {
            return false;
        }
        for (std::size_t index = 0; index < chunk; ++index) {
            hash = (hash ^ buffer[index]) * 16777619u;
        }
        remaining -= static_cast<std::int32_t>(chunk);
    }
    summary.firstExportHash = hash;
    return true;
}

bool ReadFirstExportProperties(std::FILE* file, PackageSummary& summary) {
    if (summary.firstExportSize <= 0 || summary.firstExportOffset < 0 ||
        std::fseek(file, summary.firstExportOffset, SEEK_SET) != 0) return false;
    const long objectEnd = static_cast<long>(summary.firstExportOffset) + summary.firstExportSize;

    // A zero class reference denotes a serialized UClass/UStruct definition,
    // whose body is not an instance property stream.
    if (summary.firstExportClassReference == 0) return true;

    if ((static_cast<std::uint32_t>(summary.firstExportFlags) & 0x02000000u) != 0) {
        std::int32_t functionReference{}, stateReference{};
        std::uint8_t frameData[12]{};
        if (!ReadCompactIndex(file, functionReference) ||
            !ReadCompactIndex(file, stateReference) ||
            !ReadExact(file, frameData, sizeof(frameData))) return false;
        if (functionReference != 0) {
            std::int32_t codeOffset{};
            if (!ReadCompactIndex(file, codeOffset)) return false;
        }
    }

    while (std::ftell(file) >= 0 && std::ftell(file) < objectEnd) {
        std::int32_t nameIndex{};
        if (!ReadCompactIndex(file, nameIndex) || !IsNameIndexValid(nameIndex, summary)) return false;
        const std::string& propertyName = summary.names[nameIndex];
        if (propertyName == "None") {
            summary.firstExportPropertyBytes = static_cast<std::uint32_t>(
                std::ftell(file) - summary.firstExportOffset);
            return true;
        }

        std::uint8_t info{};
        if (!ReadExact(file, &info, 1)) return false;
        const std::uint8_t type = info & 0x0fu;
        if (type == 10) {
            std::int32_t structName{};
            if (!ReadCompactIndex(file, structName) || !IsNameIndexValid(structName, summary)) return false;
        }

        std::uint32_t size{};
        switch ((info & 0x70u) >> 4u) {
            case 0: size = 1; break;
            case 1: size = 2; break;
            case 2: size = 4; break;
            case 3: size = 12; break;
            case 4: size = 16; break;
            case 5: { std::uint8_t v{}; if (!ReadExact(file, &v, 1)) return false; size = v; break; }
            case 6: { std::uint16_t v{}; if (!ReadU16(file, v)) return false; size = v; break; }
            case 7: if (!ReadU32(file, size)) return false; break;
        }

        if ((info & 0x80u) != 0 && type != 3) {
            std::uint8_t byte1{};
            if (!ReadExact(file, &byte1, 1)) return false;
            if ((byte1 & 0xc0u) == 0xc0u) {
                std::uint8_t extra[3]{};
                if (!ReadExact(file, extra, sizeof(extra))) return false;
            } else if ((byte1 & 0x80u) != 0) {
                std::uint8_t extra{};
                if (!ReadExact(file, &extra, 1)) return false;
            }
        }

        if (summary.firstExportPropertyCount == 0) summary.firstExportProperty = propertyName;
        ++summary.firstExportPropertyCount;
        const long position = std::ftell(file);
        if (position < 0 || (type != 3 &&
            (size > static_cast<std::uint64_t>(objectEnd - position) ||
             std::fseek(file, static_cast<long>(size), SEEK_CUR) != 0))) return false;
    }
    return false;
}

bool FindPropertyEnd(
    std::FILE* file,
    PackageSummary& summary,
    const ExportRecord& record,
    long& propertyEnd) {
    const auto savedClassReference = summary.firstExportClassReference;
    const auto savedFlags = summary.firstExportFlags;
    const auto savedSize = summary.firstExportSize;
    const auto savedOffset = summary.firstExportOffset;
    const auto savedCount = summary.firstExportPropertyCount;
    const auto savedBytes = summary.firstExportPropertyBytes;
    const auto savedProperty = summary.firstExportProperty;

    summary.firstExportClassReference = record.classReference;
    summary.firstExportFlags = record.flags;
    summary.firstExportSize = record.size;
    summary.firstExportOffset = record.offset;
    summary.firstExportPropertyCount = 0;
    summary.firstExportPropertyBytes = 0;
    summary.firstExportProperty.clear();
    const bool valid = ReadFirstExportProperties(file, summary);
    propertyEnd = valid ? std::ftell(file) : -1;

    summary.firstExportClassReference = savedClassReference;
    summary.firstExportFlags = savedFlags;
    summary.firstExportSize = savedSize;
    summary.firstExportOffset = savedOffset;
    summary.firstExportPropertyCount = savedCount;
    summary.firstExportPropertyBytes = savedBytes;
    summary.firstExportProperty = savedProperty;
    return valid && propertyEnd >= 0;
}

bool SkipSerializedString(std::FILE* file, long objectEnd) {
    std::int32_t length{};
    if (!ReadCompactIndex(file, length) || length < 0 || length > 65536) return false;
    const long position = std::ftell(file);
    return position >= 0 && length <= objectEnd - position &&
        std::fseek(file, length, SEEK_CUR) == 0;
}

bool ReadRootLevelModel(std::FILE* file, PackageSummary& summary) {
    const ExportRecord* level = nullptr;
    for (const ExportRecord& record : summary.exports) {
        if (ResolveExportClass(record, summary) == "Level") {
            level = &record;
            break;
        }
    }
    if (level == nullptr || level->size <= 0 || level->offset < 0) return false;

    long propertyEnd{};
    if (!FindPropertyEnd(file, summary, *level, propertyEnd)) return false;
    const long objectEnd = static_cast<long>(level->offset) + level->size;
    std::int32_t actorCount{}, actorCapacity{};
    if (!ReadI32(file, actorCount) || !ReadI32(file, actorCapacity) ||
        actorCount < 0 || actorCount > 100000 || actorCapacity < actorCount) return false;
    for (std::int32_t index = 0; index < actorCount; ++index) {
        std::int32_t actorReference{};
        if (!ReadCompactIndex(file, actorReference) ||
            !IsObjectReferenceValid(actorReference, summary)) return false;
        summary.levelActorReferences.push_back(actorReference);
    }
    summary.actorCount = static_cast<std::uint32_t>(actorCount);
    for (int index = 0; index < 4; ++index) {
        if (!SkipSerializedString(file, objectEnd)) return false;
    }
    std::int32_t optionCount{};
    if (!ReadCompactIndex(file, optionCount) || optionCount < 0 || optionCount > 1024) return false;
    for (std::int32_t index = 0; index < optionCount; ++index) {
        if (!SkipSerializedString(file, objectEnd)) return false;
    }
    std::int32_t port{}, unknown{};
    std::int32_t modelReference{};
    if (!ReadI32(file, port) || !ReadI32(file, unknown) ||
        !ReadCompactIndex(file, modelReference) || modelReference <= 0) return false;
    const std::uint64_t modelIndex = static_cast<std::uint64_t>(modelReference - 1);
    if (modelIndex >= summary.exports.size()) return false;
    const ExportRecord& model = summary.exports[modelIndex];
    if (ResolveExportClass(model, summary) != "Model") return false;

    summary.rootModelCount = 1;
    summary.rootModelName = summary.names[model.nameIndex];
    summary.rootModelSize = model.size;
    summary.rootModelOffset = model.offset;
    return std::ftell(file) <= objectEnd;
}

bool ReadPlayerStartTransform(std::FILE* file, PackageSummary& summary, const ExportRecord& actor) {
    if (actor.offset < 0 || actor.size <= 0 ||
        std::fseek(file, actor.offset, SEEK_SET) != 0) return false;
    const long objectEnd = static_cast<long>(actor.offset) + actor.size;
    if ((static_cast<std::uint32_t>(actor.flags) & 0x02000000u) != 0) {
        std::int32_t functionReference{}, stateReference{};
        std::uint8_t frameData[12]{};
        if (!ReadCompactIndex(file, functionReference) ||
            !ReadCompactIndex(file, stateReference) ||
            !ReadExact(file, frameData, sizeof(frameData))) return false;
        if (functionReference != 0) {
            std::int32_t codeOffset{};
            if (!ReadCompactIndex(file, codeOffset)) return false;
        }
    }

    while (std::ftell(file) >= 0 && std::ftell(file) < objectEnd) {
        std::int32_t nameIndex{};
        if (!ReadCompactIndex(file, nameIndex) || !IsNameIndexValid(nameIndex, summary)) return false;
        const std::string& propertyName = summary.names[nameIndex];
        if (propertyName == "None") return summary.playerStartLocationValid;
        std::uint8_t info{};
        if (!ReadExact(file, &info, 1)) return false;
        const std::uint8_t type = info & 0x0fu;
        if (type == 10) {
            std::int32_t structName{};
            if (!ReadCompactIndex(file, structName) || !IsNameIndexValid(structName, summary)) return false;
        }
        std::uint32_t size{};
        switch ((info & 0x70u) >> 4u) {
            case 0: size = 1; break;
            case 1: size = 2; break;
            case 2: size = 4; break;
            case 3: size = 12; break;
            case 4: size = 16; break;
            case 5: { std::uint8_t v{}; if (!ReadExact(file, &v, 1)) return false; size = v; break; }
            case 6: { std::uint16_t v{}; if (!ReadU16(file, v)) return false; size = v; break; }
            case 7: if (!ReadU32(file, size)) return false; break;
        }
        if ((info & 0x80u) != 0 && type != 3) {
            std::uint8_t byte1{};
            if (!ReadExact(file, &byte1, 1)) return false;
            if ((byte1 & 0xc0u) == 0xc0u) {
                std::uint8_t extra[3]{};
                if (!ReadExact(file, extra, sizeof(extra))) return false;
            } else if ((byte1 & 0x80u) != 0) {
                std::uint8_t extra{};
                if (!ReadExact(file, &extra, 1)) return false;
            }
        }
        const long valuePosition = std::ftell(file);
        if (valuePosition < 0 || size > static_cast<std::uint64_t>(objectEnd - valuePosition)) return false;
        if (propertyName == "Location" && type == 10 && size == 12) {
            if (!ReadFloat(file, summary.playerStartLocation.x) ||
                !ReadFloat(file, summary.playerStartLocation.y) ||
                !ReadFloat(file, summary.playerStartLocation.z)) return false;
            summary.playerStartLocationValid = true;
        } else if (type != 3 && std::fseek(file, static_cast<long>(size), SEEK_CUR) != 0) {
            return false;
        }
    }
    return false;
}

bool FindPlayerStart(std::FILE* file, PackageSummary& summary) {
    for (const std::int32_t reference : summary.levelActorReferences) {
        if (reference <= 0 || static_cast<std::uint32_t>(reference) > summary.exports.size()) continue;
        const ExportRecord& actor = summary.exports[reference - 1];
        const std::string className = ResolveExportClass(actor, summary);
        const std::string& objectName = summary.names[actor.nameIndex];
        if (className.find("PlayerStart") != std::string::npos ||
            objectName.find("PlayerStart") != std::string::npos) {
            summary.playerStartReference = reference;
            summary.playerStartName = objectName;
            summary.playerStartClass = className;
            return ReadPlayerStartTransform(file, summary, actor);
        }
    }
    return false;
}

bool SkipBytes(std::FILE* file, long objectEnd, std::uint64_t bytes) {
    const long position = std::ftell(file);
    return position >= 0 && bytes <= static_cast<std::uint64_t>(objectEnd - position) &&
        std::fseek(file, static_cast<long>(bytes), SEEK_CUR) == 0;
}

bool ReadArrayCount(std::FILE* file, std::uint32_t limit, std::uint32_t& output) {
    std::int32_t count{};
    if (!ReadCompactIndex(file, count) || count < 0 ||
        static_cast<std::uint32_t>(count) > limit) return false;
    output = static_cast<std::uint32_t>(count);
    return true;
}

bool ReadRootModelGeometry(std::FILE* file, PackageSummary& summary) {
    const ExportRecord* model = nullptr;
    for (const ExportRecord& record : summary.exports) {
        if (record.offset == summary.rootModelOffset &&
            ResolveExportClass(record, summary) == "Model") {
            model = &record;
            break;
        }
    }
    if (model == nullptr) return false;
    long propertyEnd{};
    if (!FindPropertyEnd(file, summary, *model, propertyEnd)) return false;
    const long objectEnd = static_cast<long>(model->offset) + model->size;

    std::uint8_t boundsValid{};
    if (!ReadFloat(file, summary.boundsMin.x) ||
        !ReadFloat(file, summary.boundsMin.y) ||
        !ReadFloat(file, summary.boundsMin.z) ||
        !ReadFloat(file, summary.boundsMax.x) ||
        !ReadFloat(file, summary.boundsMax.y) ||
        !ReadFloat(file, summary.boundsMax.z) ||
        !ReadExact(file, &boundsValid, 1) ||
        !SkipBytes(file, objectEnd, 16) ||
        !ReadArrayCount(file, 1000000, summary.vectorCount)) return false;
    (void)boundsValid;
    summary.vectors.resize(summary.vectorCount);
    for (Vec3& vector : summary.vectors) {
        if (!ReadFloat(file, vector.x) || !ReadFloat(file, vector.y) ||
            !ReadFloat(file, vector.z)) return false;
    }
    if (!ReadArrayCount(file, 1000000, summary.pointCount)) return false;
    summary.points.resize(summary.pointCount);
    for (Vec3& point : summary.points) {
        if (!ReadFloat(file, point.x) || !ReadFloat(file, point.y) ||
            !ReadFloat(file, point.z)) return false;
    }
    if (
        !ReadArrayCount(file, 1000000, summary.bspNodeCount)) return false;

    for (std::uint32_t index = 0; index < summary.bspNodeCount; ++index) {
        if (!SkipBytes(file, objectEnd, 25)) return false; // plane, zone mask, flags
        std::int32_t fields[9]{};
        for (std::int32_t& value : fields) if (!ReadCompactIndex(file, value)) return false;
        std::uint8_t vertexCount{};
        if (!ReadExact(file, &vertexCount, 1) || !SkipBytes(file, objectEnd, 8)) return false;
        summary.bspNodes.push_back({fields[0], fields[1], vertexCount});
    }

    if (!ReadArrayCount(file, 1000000, summary.surfaceCount)) return false;
    for (std::uint32_t index = 0; index < summary.surfaceCount; ++index) {
        std::int32_t material{};
        std::uint32_t polyFlags{};
        if (!ReadCompactIndex(file, material) ||
            !IsObjectReferenceValid(material, summary) || !ReadU32(file, polyFlags)) return false;
        std::int32_t fields[6]{};
        for (std::int32_t& value : fields) if (!ReadCompactIndex(file, value)) return false;
        std::int16_t panU{}, panV{};
        std::int32_t brushActor{};
        if (!ReadI16(file, panU) || !ReadI16(file, panV) ||
            !ReadCompactIndex(file, brushActor) ||
            !IsObjectReferenceValid(brushActor, summary)) return false;
        summary.surfaceFlags.push_back(polyFlags);
        summary.surfaceMaterials.push_back(material);
        summary.bspSurfaces.push_back(
            {material, polyFlags, fields[0], fields[2], fields[3], panU, panV});
    }

    {
        std::set<std::string> uniqueMaterials;
        for (std::int32_t material : summary.surfaceMaterials) {
            const std::string path = ResolveObjectPath(material, summary);
            if (!path.empty()) uniqueMaterials.insert(path);
        }
        summary.materialNames.assign(uniqueMaterials.begin(), uniqueMaterials.end());
    }

    if (!ReadArrayCount(file, 2000000, summary.vertexCount)) return false;
    for (std::uint32_t index = 0; index < summary.vertexCount; ++index) {
        std::int32_t vertex{}, side{};
        if (!ReadCompactIndex(file, vertex) || !ReadCompactIndex(file, side)) return false;
        summary.bspVertices.push_back({vertex});
    }
    std::int32_t sharedSides{}, zones{};
    if (!ReadI32(file, sharedSides) || !ReadI32(file, zones) || zones < 0 || zones > 64) return false;
    summary.zoneCount = static_cast<std::uint32_t>(zones);
    for (std::int32_t index = 0; index < zones; ++index) {
        std::int32_t zoneActor{};
        if (!ReadCompactIndex(file, zoneActor) || !IsObjectReferenceValid(zoneActor, summary) ||
            !SkipBytes(file, objectEnd, 16)) return false;
    }
    return std::ftell(file) <= objectEnd;
}

Vec3 Subtract(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

bool Normalize(Vec3& value) {
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (lengthSquared < 1.0e-12f) return false;
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    value.x *= inverseLength;
    value.y *= inverseLength;
    value.z *= inverseLength;
    return true;
}

bool WriteMeshChunk(
    std::FILE* file,
    std::int32_t materialSlot,
    const MeshVertex* vertices,
    std::uint32_t count) {
    return std::fwrite(&materialSlot, sizeof(materialSlot), 1, file) == 1 &&
        std::fwrite(&count, sizeof(count), 1, file) == 1 &&
        std::fwrite(vertices, sizeof(MeshVertex), count, file) == count;
}

bool WriteTextureCache(
    const std::string& path,
    const PortableMipmap& mipmap,
    const std::vector<std::uint32_t>& palette) {
    if (palette.size() < 256 || mipmap.pixels.empty()) return false;
    std::vector<std::uint8_t> rgba(mipmap.pixels.size() * 4u);
    for (std::size_t index = 0; index < mipmap.pixels.size(); ++index) {
        const std::uint32_t color = palette[mipmap.pixels[index]];
        rgba[index * 4u + 0u] = static_cast<std::uint8_t>(color);
        rgba[index * 4u + 1u] = static_cast<std::uint8_t>(color >> 8u);
        rgba[index * 4u + 2u] = static_cast<std::uint8_t>(color >> 16u);
        rgba[index * 4u + 3u] = 255u;
    }
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) return false;
    const std::uint32_t magic = 0x54515844u; // DXQT
    const std::uint32_t version = 1;
    const bool ok =
        std::fwrite(&magic, sizeof(magic), 1, file) == 1 &&
        std::fwrite(&version, sizeof(version), 1, file) == 1 &&
        std::fwrite(&mipmap.width, sizeof(mipmap.width), 1, file) == 1 &&
        std::fwrite(&mipmap.height, sizeof(mipmap.height), 1, file) == 1 &&
        std::fwrite(rgba.data(), 1, rgba.size(), file) == rgba.size();
    std::fclose(file);
    return ok;
}

bool WriteWorldMesh(const std::string& path, const PackageSummary& summary) {
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) return false;
    const std::uint32_t magic = 0x4d515844u; // DXQM
    const std::uint32_t version = 2;
    std::uint32_t chunkCount{};
    bool ok = std::fwrite(&magic, sizeof(magic), 1, file) == 1 &&
        std::fwrite(&version, sizeof(version), 1, file) == 1 &&
        std::fwrite(&chunkCount, sizeof(chunkCount), 1, file) == 1;

    const Vec3 center{
        (summary.boundsMin.x + summary.boundsMax.x) * 0.5f,
        (summary.boundsMin.y + summary.boundsMax.y) * 0.5f,
        (summary.boundsMin.z + summary.boundsMax.z) * 0.5f};
    const Vec3 extent = Subtract(summary.boundsMax, summary.boundsMin);
    const float largestExtent = std::max(extent.x, std::max(extent.y, extent.z));
    const bool playerScale = summary.playerStartLocationValid;
    const Vec3 origin = playerScale ? summary.playerStartLocation : center;
    const float scale = playerScale ? (1.0f / 52.5f) :
        (largestExtent > 0.0f ? 3.0f / largestExtent : 0.001f);
    auto transform = [&](const Vec3& point) {
        return Vec3{
            (point.y - origin.y) * scale,
            (point.z - origin.z) * scale + (playerScale ? 1.0f : 1.4f),
            -(point.x - origin.x) * scale - (playerScale ? 0.0f : 3.0f)};
    };

    std::vector<MeshVertex> flatVertices;
    std::vector<MeshVertex> texturedVertices;
    flatVertices.reserve(150000);
    texturedVertices.reserve(30000);

    for (const BspNodeGeometry& node : summary.bspNodes) {
        if (node.vertexCount < 3 || node.vertexPool < 0 || node.surface < 0 ||
            static_cast<std::uint64_t>(node.vertexPool) + node.vertexCount >
                summary.bspVertices.size() ||
            static_cast<std::uint32_t>(node.surface) >= summary.bspSurfaces.size()) continue;
        const BspSurfaceGeometry& surface = summary.bspSurfaces[node.surface];
        if ((surface.flags & 0x00000001u) != 0) continue;
        const bool textured = !summary.materialNames.empty() &&
            ResolveObjectPath(surface.material, summary) == summary.materialNames.front();
        const bool uvValid = surface.basePoint >= 0 && surface.textureU >= 0 &&
            surface.textureV >= 0 &&
            static_cast<std::size_t>(surface.basePoint) < summary.points.size() &&
            static_cast<std::size_t>(surface.textureU) < summary.vectors.size() &&
            static_cast<std::size_t>(surface.textureV) < summary.vectors.size();
        if (textured && !uvValid) continue;

        std::vector<Vec3> polygon;
        std::vector<Vec3> sourcePolygon;
        polygon.reserve(node.vertexCount);
        sourcePolygon.reserve(node.vertexCount);
        bool polygonValid = true;
        for (std::uint32_t index = 0; index < node.vertexCount; ++index) {
            const std::int32_t point = summary.bspVertices[node.vertexPool + index].point;
            if (point < 0 || static_cast<std::uint32_t>(point) >= summary.points.size()) {
                polygonValid = false;
                break;
            }
            sourcePolygon.push_back(summary.points[point]);
            polygon.push_back(transform(summary.points[point]));
        }
        if (!polygonValid) continue;

        for (std::size_t index = 1; index + 1 < polygon.size(); ++index) {
            Vec3 normal = Cross(Subtract(polygon[index], polygon[0]),
                                Subtract(polygon[index + 1], polygon[0]));
            if (!Normalize(normal)) continue;
            const Vec3 reverse{-normal.x, -normal.y, -normal.z};
            auto makeVertex = [&](std::size_t vertex, const Vec3& vertexNormal) {
                float u{}, v{};
                if (textured) {
                    const Vec3 relative = Subtract(
                        sourcePolygon[vertex], summary.points[surface.basePoint]);
                    const Vec3& textureU = summary.vectors[surface.textureU];
                    const Vec3& textureV = summary.vectors[surface.textureV];
                    u = (relative.x * textureU.x + relative.y * textureU.y +
                        relative.z * textureU.z + surface.panU) / 128.0f;
                    v = (relative.x * textureV.x + relative.y * textureV.y +
                        relative.z * textureV.z + surface.panV) / 128.0f;
                }
                return MeshVertex{polygon[vertex], vertexNormal, u, v, textured ? 0 : -1};
            };
            std::vector<MeshVertex>& target = textured ? texturedVertices : flatVertices;
            target.push_back(makeVertex(0, normal));
            target.push_back(makeVertex(index, normal));
            target.push_back(makeVertex(index + 1, normal));
            target.push_back(makeVertex(0, reverse));
            target.push_back(makeVertex(index + 1, reverse));
            target.push_back(makeVertex(index, reverse));
        }
        if (!ok) break;
    }
    auto writeMaterial = [&](const std::vector<MeshVertex>& vertices, std::int32_t slot) {
        for (std::size_t offset = 0; offset < vertices.size(); offset += 60000) {
            const std::uint32_t count = static_cast<std::uint32_t>(
                std::min<std::size_t>(60000, vertices.size() - offset));
            if (count == 0 || count % 3 != 0 ||
                !WriteMeshChunk(file, slot, vertices.data() + offset, count)) return false;
            ++chunkCount;
        }
        return true;
    };
    if (ok) ok = writeMaterial(flatVertices, -1) && writeMaterial(texturedVertices, 0);
    if (ok && std::fseek(file, 8, SEEK_SET) == 0) {
        ok = std::fwrite(&chunkCount, sizeof(chunkCount), 1, file) == 1;
    } else {
        ok = false;
    }
    std::fclose(file);
    return ok && chunkCount > 0;
}

bool ProbePackage(const std::string& path, PackageSummary& summary) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Cannot open %s", path.c_str());
        return false;
    }

    std::uint32_t signature{};
    const bool headerRead =
        ReadU32(file, signature) &&
        ReadU16(file, summary.version) &&
        ReadU16(file, summary.licenseeMode) &&
        ReadU32(file, summary.flags) &&
        ReadU32(file, summary.nameCount) &&
        ReadU32(file, summary.nameOffset) &&
        ReadU32(file, summary.exportCount) &&
        ReadU32(file, summary.exportOffset) &&
        ReadU32(file, summary.importCount) &&
        ReadU32(file, summary.importOffset);

    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);

    const bool offsetsValid = length >= 0 &&
        summary.nameOffset <= static_cast<std::uint64_t>(length) &&
        summary.exportOffset <= static_cast<std::uint64_t>(length) &&
        summary.importOffset <= static_cast<std::uint64_t>(length);
    if (!headerRead || signature != kUe1PackageSignature ||
        summary.version != 68 || !offsetsValid) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "Invalid UE1 package header: %s signature=%08x version=%u bytes=%ld",
            path.c_str(),
            signature,
            summary.version,
            length);
        std::fclose(file);
        return false;
    }

    const bool namesValid = ReadNameTable(file, summary);
    const bool importsValid = namesValid && ReadImportTable(file, summary);
    const bool exportsValid = importsValid && ReadExportTable(file, length, summary);
    if (exportsValid) InventoryWorldExports(summary);
    const bool payloadValid = exportsValid && HashFirstExport(file, summary);
    const bool propertiesValid = payloadValid && ReadFirstExportProperties(file, summary);
    const bool levelValid = propertiesValid &&
        (summary.levelExportCount == 0 || ReadRootLevelModel(file, summary));
    const bool playerStartValid = levelValid &&
        (summary.levelExportCount == 0 || FindPlayerStart(file, summary));
    const bool geometryValid = playerStartValid &&
        (summary.rootModelCount == 0 || ReadRootModelGeometry(file, summary));
    std::fclose(file);
    if (!namesValid || !importsValid || !exportsValid || !payloadValid ||
        !propertiesValid || !levelValid || !playerStartValid || !geometryValid) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "Invalid UE1 tables: %s names=%u/%u imports=%u/%u exports=%u/%u",
            path.c_str(),
            summary.namesParsed,
            summary.nameCount,
            summary.importsParsed,
            summary.importCount,
            summary.exportsParsed,
            summary.exportCount);
        return false;
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "UE1 package OK: %s version=%u names=%u exports=%u imports=%u bytes=%ld firstExport=%s class=%s hash=%08x",
        path.c_str(),
        summary.version,
        summary.nameCount,
        summary.exportCount,
        summary.importCount,
        length,
        summary.firstExportName.c_str(),
        summary.firstExportClass.c_str(),
        summary.firstExportHash);
    if (!summary.materialNames.empty()) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "UE1 world materials: %zu unique, first=%s",
            summary.materialNames.size(),
            summary.materialNames.front().c_str());
    }
    return true;
}

}  // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_deusex_questvr_MainActivity_probeGameData(
    JNIEnv* env,
    jclass,
    jstring gameRoot) {
    if (gameRoot == nullptr) {
        return JNI_FALSE;
    }

    const char* rootChars = env->GetStringUTFChars(gameRoot, nullptr);
    if (rootChars == nullptr) {
        return JNI_FALSE;
    }
    const std::string root(rootChars);
    env->ReleaseStringUTFChars(gameRoot, rootChars);

    try {
        const auto portableFile = File::open_existing(root + "/Maps/00_Training.dx");
        const NameString packageName("00_Training");
        const PortablePackageTables portableTraining =
            LoadPortablePackageTables(root + "/Maps/00_Training.dx");
        const PortablePackageTables portableScripts =
            LoadPortablePackageTables(root + "/System/DeusEx.u");
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "Surreal tables loaded %s (%lld bytes, version %u, names=%zu exports=%zu imports=%zu); DeusEx.u names=%zu exports=%zu imports=%zu",
            packageName.ToString().c_str(),
            static_cast<long long>(portableFile->size()),
            portableTraining.version,
            portableTraining.names.size(),
            portableTraining.exports.size(),
            portableTraining.imports.size(),
            portableScripts.names.size(),
            portableScripts.exports.size(),
            portableScripts.imports.size());
    } catch (const std::exception& error) {
        __android_log_print(
            ANDROID_LOG_ERROR, kLogTag, "Surreal portable core failed: %s", error.what());
        return JNI_FALSE;
    }

    PackageSummary training{};
    PackageSummary gameScripts{};
    const bool trainingValid = ProbePackage(root + "/Maps/00_Training.dx", training);
    const bool scriptsValid = ProbePackage(root + "/System/DeusEx.u", gameScripts);
    bool firstTextureValid = false;
    if (trainingValid && !training.materialNames.empty()) {
        try {
            const std::string& qualified = training.materialNames.front();
            const std::size_t separator = qualified.find('.');
            if (separator == std::string::npos) throw std::runtime_error("Material path has no package");
            const std::string texturePackageName = qualified.substr(0, separator);
            const std::string textureObjectPath = qualified.substr(separator + 1);
            const PortablePackageTables texturePackage = LoadPortablePackageTables(
                root + "/Textures/" + texturePackageName + ".utx");
            const std::size_t textureExport = FindPortableExport(texturePackage, textureObjectPath);
            const PortablePropertyStream textureProperties =
                LoadPortableExportProperties(texturePackage, textureExport);
            const std::vector<PortableMipmap> mipmaps =
                LoadPortableTextureMipmaps(texturePackage, textureExport);
            std::vector<std::uint32_t> palette;
            for (const PortableTaggedProperty& property : textureProperties.properties) {
                if (property.name == "Palette") {
                    const std::int32_t paletteReference = DecodePortableObjectReference(property);
                    if (paletteReference <= 0) {
                        throw std::runtime_error("First texture palette is not a local export");
                    }
                    palette = LoadPortablePalette(
                        texturePackage, static_cast<std::size_t>(paletteReference - 1));
                    break;
                }
            }
            firstTextureValid = !mipmaps.empty() && !palette.empty() &&
                WriteTextureCache(root + "/quest-first-texture.rgba", mipmaps.front(), palette);
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "Surreal texture decoded %s: %zu mips, top=%ux%u (%zu indexed bytes), palette=%zu colors",
                qualified.c_str(),
                mipmaps.size(),
                mipmaps.front().width,
                mipmaps.front().height,
                mipmaps.front().pixels.size(),
                palette.size());
        } catch (const std::exception& error) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                "Surreal texture decode failed: %s", error.what());
        }
    }
    bool portableTablesMatch = false;
    try {
        const PortablePackageTables portableTraining =
            LoadPortablePackageTables(root + "/Maps/00_Training.dx");
        const PortablePackageTables portableScripts =
            LoadPortablePackageTables(root + "/System/DeusEx.u");
        portableTablesMatch =
            portableTraining.names.size() == training.nameCount &&
            portableTraining.exports.size() == training.exportCount &&
            portableTraining.imports.size() == training.importCount &&
            portableScripts.names.size() == gameScripts.nameCount &&
            portableScripts.exports.size() == gameScripts.exportCount &&
            portableScripts.imports.size() == gameScripts.importCount;
        const PortablePropertyStream firstProperties =
            LoadPortableExportProperties(portableTraining, 0);
        portableTablesMatch = portableTablesMatch &&
            firstProperties.properties.size() == training.firstExportPropertyCount &&
            firstProperties.bytesConsumed == training.firstExportPropertyBytes &&
            !firstProperties.properties.empty() &&
            firstProperties.properties.front().name.ToString() == training.firstExportProperty;
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "Surreal export stream decoded %zu properties from %s in %u bytes",
            firstProperties.properties.size(),
            training.firstExportName.c_str(),
            firstProperties.bytesConsumed);
    } catch (const std::exception& error) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
            "Surreal portable table comparison failed: %s", error.what());
    }
    const bool meshValid = trainingValid &&
        WriteWorldMesh(root + "/quest-world.mesh", training);
    const bool valid = trainingValid && scriptsValid && portableTablesMatch &&
        firstTextureValid && meshValid;

    const std::string resultPath = root + "/quest-package-probe.txt";
    if (std::FILE* result = std::fopen(resultPath.c_str(), "wb")) {
        std::fprintf(
            result,
            "result=%s\n"
            "training.version=%u\ntraining.names=%u\ntraining.names_parsed=%u\ntraining.first_name=%s\ntraining.exports=%u\ntraining.exports_parsed=%u\ntraining.level_exports=%u\ntraining.level_name=%s\ntraining.level_size=%d\ntraining.level_offset=%d\ntraining.actors=%u\ntraining.player_start=%s\ntraining.player_start_class=%s\ntraining.player_start_location=%.3f,%.3f,%.3f\ntraining.model_exports=%u\ntraining.first_model=%s\ntraining.first_model_size=%d\ntraining.first_model_offset=%d\ntraining.root_models=%u\ntraining.root_model=%s\ntraining.root_model_size=%d\ntraining.root_model_offset=%d\ntraining.vectors=%u\ntraining.points=%u\ntraining.bsp_nodes=%u\ntraining.surfaces=%u\ntraining.vertices=%u\ntraining.zones=%u\ntraining.first_export=%s\ntraining.first_export_class=%s\ntraining.first_export_size=%d\ntraining.first_export_offset=%d\ntraining.first_export_fnv1a=%08x\ntraining.first_export_properties=%u\ntraining.first_property=%s\ntraining.property_bytes=%u\ntraining.imports=%u\ntraining.imports_parsed=%u\n"
            "training.materials=%zu\ntraining.first_material=%s\n"
            "scripts.version=%u\nscripts.names=%u\nscripts.names_parsed=%u\nscripts.first_name=%s\nscripts.exports=%u\nscripts.exports_parsed=%u\nscripts.first_export=%s\nscripts.first_export_class=%s\nscripts.first_export_size=%d\nscripts.first_export_offset=%d\nscripts.first_export_fnv1a=%08x\nscripts.first_export_properties=%u\nscripts.first_property=%s\nscripts.property_bytes=%u\nscripts.imports=%u\nscripts.imports_parsed=%u\n",
            valid ? "ok" : "failed",
            training.version,
            training.nameCount,
            training.namesParsed,
            training.firstName.c_str(),
            training.exportCount,
            training.exportsParsed,
            training.levelExportCount,
            training.levelName.c_str(),
            training.levelSize,
            training.levelOffset,
            training.actorCount,
            training.playerStartName.c_str(),
            training.playerStartClass.c_str(),
            training.playerStartLocation.x,
            training.playerStartLocation.y,
            training.playerStartLocation.z,
            training.modelExportCount,
            training.firstModelName.c_str(),
            training.firstModelSize,
            training.firstModelOffset,
            training.rootModelCount,
            training.rootModelName.c_str(),
            training.rootModelSize,
            training.rootModelOffset,
            training.vectorCount,
            training.pointCount,
            training.bspNodeCount,
            training.surfaceCount,
            training.vertexCount,
            training.zoneCount,
            training.firstExportName.c_str(),
            training.firstExportClass.c_str(),
            training.firstExportSize,
            training.firstExportOffset,
            training.firstExportHash,
            training.firstExportPropertyCount,
            training.firstExportProperty.c_str(),
            training.firstExportPropertyBytes,
            training.importCount,
            training.importsParsed,
            training.materialNames.size(),
            training.materialNames.empty() ? "" : training.materialNames.front().c_str(),
            gameScripts.version,
            gameScripts.nameCount,
            gameScripts.namesParsed,
            gameScripts.firstName.c_str(),
            gameScripts.exportCount,
            gameScripts.exportsParsed,
            gameScripts.firstExportName.c_str(),
            gameScripts.firstExportClass.c_str(),
            gameScripts.firstExportSize,
            gameScripts.firstExportOffset,
            gameScripts.firstExportHash,
            gameScripts.firstExportPropertyCount,
            gameScripts.firstExportProperty.c_str(),
            gameScripts.firstExportPropertyBytes,
            gameScripts.importCount,
            gameScripts.importsParsed);
        std::fclose(result);
    } else {
        __android_log_print(
            ANDROID_LOG_ERROR, kLogTag, "Cannot write probe result: %s", resultPath.c_str());
    }

    return valid ? JNI_TRUE : JNI_FALSE;
}
