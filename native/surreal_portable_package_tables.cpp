#include "surreal_portable_package_tables.h"

#include "Package/PackageStream.h"
#include "Utils/File.h"

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

constexpr std::uint32_t kPackageSignature = 0x9E2A83C1u;
constexpr std::uint32_t kMaxTableEntries = 2'000'000u;
constexpr std::uint32_t kMaxSerializedNameUnits = 65'536u;

void ValidateTable(std::uint32_t count, std::uint32_t offset, std::int64_t fileSize) {
    if (count > kMaxTableEntries || offset > static_cast<std::uint64_t>(fileSize)) {
        throw std::runtime_error("UE1 package table is outside the file");
    }
}

void AppendUtf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7fu) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ffu) {
        output.push_back(static_cast<char>(0xc0u | (codepoint >> 6u)));
        output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    } else if (codepoint <= 0xffffu) {
        output.push_back(static_cast<char>(0xe0u | (codepoint >> 12u)));
        output.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
        output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    } else {
        output.push_back(static_cast<char>(0xf0u | (codepoint >> 18u)));
        output.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3fu)));
        output.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
        output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    }
}

std::string ReadVersionedString(PackageStream& stream, std::uint16_t version) {
    if (version < 64) {
        std::string result;
        for (std::uint32_t i = 0; i < kMaxSerializedNameUnits; ++i) {
            const char value = static_cast<char>(stream.ReadInt8());
            if (value == '\0') return result;
            result.push_back(value);
        }
        throw std::runtime_error("UE1 name is not terminated");
    }

    const std::int32_t serializedLength = stream.ReadIndex();
    if (serializedLength == 0) return {};
    const std::int64_t units64 = serializedLength < 0
        ? -static_cast<std::int64_t>(serializedLength)
        : static_cast<std::int64_t>(serializedLength);
    if (units64 <= 0 || units64 > kMaxSerializedNameUnits) {
        throw std::runtime_error("UE1 name length is invalid");
    }
    const auto units = static_cast<std::uint32_t>(units64);

    if (serializedLength > 0) {
        std::string bytes(units, '\0');
        stream.ReadBytes(bytes.data(), units);
        if (bytes.back() != '\0') throw std::runtime_error("UE1 ANSI name is not terminated");
        bytes.pop_back();
        return bytes;
    }

    std::string result;
    for (std::uint32_t i = 0; i < units; ++i) {
        const std::uint16_t first = stream.ReadUInt16();
        if (i + 1 == units) {
            if (first != 0) throw std::runtime_error("UE1 Unicode name is not terminated");
            return result;
        }
        std::uint32_t codepoint = first;
        if (first >= 0xd800u && first <= 0xdbffu) {
            if (++i >= units - 1) throw std::runtime_error("UE1 Unicode surrogate is truncated");
            const std::uint16_t second = stream.ReadUInt16();
            if (second < 0xdc00u || second > 0xdfffu) {
                throw std::runtime_error("UE1 Unicode surrogate is invalid");
            }
            codepoint = 0x10000u +
                ((static_cast<std::uint32_t>(first) - 0xd800u) << 10u) +
                (static_cast<std::uint32_t>(second) - 0xdc00u);
        } else if (first >= 0xdc00u && first <= 0xdfffu) {
            throw std::runtime_error("UE1 Unicode surrogate is invalid");
        }
        AppendUtf8(result, codepoint);
    }
    throw std::runtime_error("UE1 Unicode name is not terminated");
}

void ValidateNameIndex(std::int32_t index, std::size_t count) {
    if (index < 0 || static_cast<std::size_t>(index) >= count) {
        throw std::runtime_error("UE1 name index is outside the name table");
    }
}

void ValidateObjectReference(std::int32_t reference, std::size_t imports, std::size_t exports) {
    const bool valid = reference == 0 ||
        (reference > 0 && static_cast<std::size_t>(reference) <= exports) ||
        (reference < 0 && static_cast<std::uint64_t>(-static_cast<std::int64_t>(reference)) <= imports);
    if (!valid) throw std::runtime_error("UE1 object reference is outside the package tables");
}

class PayloadReader {
public:
    explicit PayloadReader(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}

    std::uint8_t ReadUInt8() {
        Require(1);
        return bytes_[position_++];
    }
    std::uint16_t ReadUInt16() {
        const std::uint16_t low = ReadUInt8();
        return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(ReadUInt8()) << 8u));
    }
    std::uint32_t ReadUInt32() {
        const std::uint32_t low = ReadUInt16();
        return low | (static_cast<std::uint32_t>(ReadUInt16()) << 16u);
    }
    std::int32_t ReadIndex() {
        std::uint8_t value = ReadUInt8();
        const bool negative = (value & 0x80u) != 0;
        bool more = (value & 0x40u) != 0;
        std::uint32_t magnitude = value & 0x3fu;
        unsigned shift = 6;
        while (more && shift < 32) {
            value = ReadUInt8();
            magnitude |= static_cast<std::uint32_t>(value & 0x7fu) << shift;
            more = (value & 0x80u) != 0;
            shift += 7;
        }
        if (more || magnitude > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
            throw std::runtime_error("UE1 compact index is invalid");
        }
        return negative ? -static_cast<std::int32_t>(magnitude) : static_cast<std::int32_t>(magnitude);
    }
    void Skip(std::size_t count) {
        Require(count);
        position_ += count;
    }
    std::vector<std::uint8_t> ReadBytes(std::size_t count) {
        Require(count);
        std::vector<std::uint8_t> result(
            bytes_.begin() + static_cast<std::ptrdiff_t>(position_),
            bytes_.begin() + static_cast<std::ptrdiff_t>(position_ + count));
        position_ += count;
        return result;
    }
    std::uint32_t Tell() const { return static_cast<std::uint32_t>(position_); }

private:
    void Require(std::size_t count) const {
        if (count > bytes_.size() - position_) {
            throw std::runtime_error("UE1 export payload ended unexpectedly");
        }
    }

    std::vector<std::uint8_t> bytes_;
    std::size_t position_{};
};

const NameString& ReadPayloadName(PayloadReader& reader, const PortablePackageTables& package) {
    const std::int32_t index = reader.ReadIndex();
    ValidateNameIndex(index, package.names.size());
    return package.names[static_cast<std::size_t>(index)].Name;
}

std::string ResolvePortableObjectPath(
    std::int32_t reference,
    const PortablePackageTables& package,
    int depth = 0) {
    if (reference == 0 || depth > 32) return {};
    std::int32_t outer{};
    std::int32_t nameIndex{-1};
    if (reference > 0) {
        const std::size_t index = static_cast<std::size_t>(reference - 1);
        if (index >= package.exports.size()) return {};
        const ExportTableEntry& entry = package.exports[index];
        outer = entry.ObjOuter;
        nameIndex = entry.ObjName;
    } else {
        const std::size_t index = static_cast<std::size_t>(-static_cast<std::int64_t>(reference) - 1);
        if (index >= package.imports.size()) return {};
        const ImportTableEntry& entry = package.imports[index];
        outer = entry.ObjOuter;
        nameIndex = entry.ObjName;
    }
    ValidateNameIndex(nameIndex, package.names.size());
    const std::string prefix = ResolvePortableObjectPath(outer, package, depth + 1);
    const std::string& name = package.names[static_cast<std::size_t>(nameIndex)].Name.ToString();
    return prefix.empty() ? name : prefix + "." + name;
}

}  // namespace

PortablePackageTables LoadPortablePackageTables(const std::string& path) {
    const std::shared_ptr<File> file = File::open_existing(path);
    PackageStream stream(nullptr, file);
    PortablePackageTables package;
    package.sourcePath = path;

    if (stream.ReadUInt32() != kPackageSignature) {
        throw std::runtime_error("Not a UE1 package");
    }
    package.version = stream.ReadUInt16();
    package.licenseeMode = stream.ReadUInt16();
    if (package.version < 60 || package.version >= 100) {
        throw std::runtime_error("Unsupported UE1 package version");
    }
    package.flags = stream.ReadUInt32();
    const std::uint32_t nameCount = stream.ReadUInt32();
    const std::uint32_t nameOffset = stream.ReadUInt32();
    const std::uint32_t exportCount = stream.ReadUInt32();
    const std::uint32_t exportOffset = stream.ReadUInt32();
    const std::uint32_t importCount = stream.ReadUInt32();
    const std::uint32_t importOffset = stream.ReadUInt32();
    ValidateTable(nameCount, nameOffset, file->size());
    ValidateTable(exportCount, exportOffset, file->size());
    ValidateTable(importCount, importOffset, file->size());

    package.names.reserve(nameCount);
    stream.Seek(nameOffset);
    for (std::uint32_t i = 0; i < nameCount; ++i) {
        NameTableEntry entry;
        entry.Name = ReadVersionedString(stream, package.version);
        entry.Flags = stream.ReadUInt32();
        package.names.push_back(std::move(entry));
    }

    package.exports.reserve(exportCount);
    stream.Seek(exportOffset);
    for (std::uint32_t i = 0; i < exportCount; ++i) {
        ExportTableEntry entry;
        entry.ObjClass = stream.ReadIndex();
        entry.ObjBase = stream.ReadIndex();
        entry.ObjOuter = stream.ReadInt32();
        entry.ObjName = stream.ReadIndex();
        entry.ObjFlags = static_cast<ObjectFlags>(stream.ReadUInt32());
        entry.ObjSize = stream.ReadIndex();
        entry.ObjOffset = entry.ObjSize > 0 ? stream.ReadIndex() : -1;
        ValidateNameIndex(entry.ObjName, package.names.size());
        package.exports.push_back(entry);
    }

    package.imports.reserve(importCount);
    stream.Seek(importOffset);
    for (std::uint32_t i = 0; i < importCount; ++i) {
        ImportTableEntry entry;
        entry.ClassPackage = stream.ReadIndex();
        entry.ClassName = stream.ReadIndex();
        entry.ObjOuter = stream.ReadInt32();
        entry.ObjName = stream.ReadIndex();
        ValidateNameIndex(entry.ClassPackage, package.names.size());
        ValidateNameIndex(entry.ClassName, package.names.size());
        ValidateNameIndex(entry.ObjName, package.names.size());
        package.imports.push_back(entry);
    }

    for (const ExportTableEntry& entry : package.exports) {
        ValidateObjectReference(entry.ObjClass, package.imports.size(), package.exports.size());
        ValidateObjectReference(entry.ObjBase, package.imports.size(), package.exports.size());
        ValidateObjectReference(entry.ObjOuter, package.imports.size(), package.exports.size());
        if (entry.ObjSize < 0 || (entry.ObjSize > 0 &&
            (entry.ObjOffset < 0 || static_cast<std::uint64_t>(entry.ObjOffset) +
                static_cast<std::uint64_t>(entry.ObjSize) > static_cast<std::uint64_t>(file->size())))) {
            throw std::runtime_error("UE1 export payload is outside the file");
        }
    }
    for (const ImportTableEntry& entry : package.imports) {
        ValidateObjectReference(entry.ObjOuter, package.imports.size(), package.exports.size());
    }
    return package;
}

PortablePropertyStream LoadPortableExportProperties(
    const PortablePackageTables& package,
    std::size_t exportIndex) {
    if (exportIndex >= package.exports.size()) {
        throw std::runtime_error("UE1 export index is outside the export table");
    }
    const ExportTableEntry& entry = package.exports[exportIndex];
    if (entry.ObjClass == 0) {
        return {};
    }
    if (entry.ObjSize <= 0 || entry.ObjOffset < 0) {
        throw std::runtime_error("UE1 object instance has no serialized payload");
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    const std::shared_ptr<File> file = File::open_existing(package.sourcePath);
    file->seek(entry.ObjOffset);
    file->read(bytes.data(), bytes.size());
    PayloadReader reader(std::move(bytes));

    if (AnyFlags(entry.ObjFlags, ObjectFlags::HasStack)) {
        const std::int32_t functionReference = reader.ReadIndex();
        const std::int32_t stateReference = reader.ReadIndex();
        ValidateObjectReference(functionReference, package.imports.size(), package.exports.size());
        ValidateObjectReference(stateReference, package.imports.size(), package.exports.size());
        reader.Skip(12);
        if (functionReference != 0) reader.ReadIndex();
    }

    PortablePropertyStream result;
    while (true) {
        const NameString& name = ReadPayloadName(reader, package);
        if (name == "None") {
            result.bytesConsumed = reader.Tell();
            return result;
        }

        const std::uint8_t info = reader.ReadUInt8();
        PortableTaggedProperty property;
        property.name = name;
        property.type = info & 0x0fu;
        if (property.type > 15u) throw std::runtime_error("UE1 property type is invalid");
        if (property.type == 10u) property.structName = ReadPayloadName(reader, package);

        switch ((info & 0x70u) >> 4u) {
            case 0: property.size = 1; break;
            case 1: property.size = 2; break;
            case 2: property.size = 4; break;
            case 3: property.size = 12; break;
            case 4: property.size = 16; break;
            case 5: property.size = reader.ReadUInt8(); break;
            case 6: property.size = reader.ReadUInt16(); break;
            case 7: property.size = reader.ReadUInt32(); break;
        }

        if (property.type == 3u) {
            property.boolValue = (info & 0x80u) != 0;
        } else if ((info & 0x80u) != 0) {
            std::uint32_t first = reader.ReadUInt8();
            if ((first & 0xc0u) == 0xc0u) {
                first &= 0x3fu;
                property.arrayIndex = (first << 24u) |
                    (static_cast<std::uint32_t>(reader.ReadUInt8()) << 16u) |
                    (static_cast<std::uint32_t>(reader.ReadUInt8()) << 8u) |
                    reader.ReadUInt8();
            } else if ((first & 0x80u) != 0) {
                first &= 0x7fu;
                property.arrayIndex = (first << 8u) | reader.ReadUInt8();
            } else {
                property.arrayIndex = first;
            }
        }

        property.valueOffset = reader.Tell();
        if (property.type != 3u) property.value = reader.ReadBytes(property.size);
        result.properties.push_back(std::move(property));
    }
}

std::size_t FindPortableExport(
    const PortablePackageTables& package,
    const std::string& objectPath) {
    for (std::size_t index = 0; index < package.exports.size(); ++index) {
        if (ResolvePortableObjectPath(
                static_cast<std::int32_t>(index + 1), package) == objectPath) {
            return index;
        }
    }
    throw std::runtime_error("UE1 export was not found: " + objectPath);
}

std::vector<PortableMipmap> LoadPortableTextureMipmaps(
    const PortablePackageTables& package,
    std::size_t exportIndex) {
    if (exportIndex >= package.exports.size()) {
        throw std::runtime_error("UE1 texture export index is outside the table");
    }
    const ExportTableEntry& entry = package.exports[exportIndex];
    const PortablePropertyStream properties = LoadPortableExportProperties(package, exportIndex);
    if (properties.bytesConsumed >= static_cast<std::uint32_t>(entry.ObjSize)) {
        throw std::runtime_error("UE1 texture has no mipmap payload");
    }

    const std::shared_ptr<File> file = File::open_existing(package.sourcePath);
    PackageStream stream(nullptr, file);
    stream.Seek(static_cast<std::uint32_t>(entry.ObjOffset) + properties.bytesConsumed);
    const std::uint8_t mipCount = stream.ReadUInt8();
    if (mipCount == 0 || mipCount > 32) throw std::runtime_error("UE1 mip count is invalid");

    std::vector<PortableMipmap> mipmaps;
    mipmaps.reserve(mipCount);
    const std::uint64_t objectEnd =
        static_cast<std::uint64_t>(entry.ObjOffset) + static_cast<std::uint64_t>(entry.ObjSize);
    for (std::uint8_t index = 0; index < mipCount; ++index) {
        if (package.version >= 63) stream.ReadUInt32();
        const std::int32_t byteCount = stream.ReadIndex();
        if (byteCount < 0 || byteCount > 64 * 1024 * 1024 ||
            static_cast<std::uint64_t>(stream.Tell()) + static_cast<std::uint64_t>(byteCount) + 10u >
                objectEnd) {
            throw std::runtime_error("UE1 mip payload is outside the texture export");
        }
        PortableMipmap mipmap;
        mipmap.pixels.resize(static_cast<std::size_t>(byteCount));
        stream.ReadBytes(mipmap.pixels.data(), static_cast<std::uint32_t>(mipmap.pixels.size()));
        mipmap.width = stream.ReadUInt32();
        mipmap.height = stream.ReadUInt32();
        mipmap.uBits = stream.ReadUInt8();
        mipmap.vBits = stream.ReadUInt8();
        if (mipmap.width == 0 || mipmap.height == 0 || mipmap.width > 8192 ||
            mipmap.height > 8192) {
            throw std::runtime_error("UE1 mip dimensions are invalid");
        }
        mipmaps.push_back(std::move(mipmap));
    }
    return mipmaps;
}

std::int32_t DecodePortableObjectReference(const PortableTaggedProperty& property) {
    if (property.type != 5u || property.value.empty()) {
        throw std::runtime_error("UE1 property is not an object reference");
    }
    PayloadReader reader(property.value);
    const std::int32_t reference = reader.ReadIndex();
    if (reader.Tell() != property.value.size()) {
        throw std::runtime_error("UE1 object-reference property has trailing bytes");
    }
    return reference;
}

std::vector<std::uint32_t> LoadPortablePalette(
    const PortablePackageTables& package,
    std::size_t exportIndex) {
    if (exportIndex >= package.exports.size()) {
        throw std::runtime_error("UE1 palette export index is outside the table");
    }
    const ExportTableEntry& entry = package.exports[exportIndex];
    const PortablePropertyStream properties = LoadPortableExportProperties(package, exportIndex);
    const std::shared_ptr<File> file = File::open_existing(package.sourcePath);
    PackageStream stream(nullptr, file);
    stream.Seek(static_cast<std::uint32_t>(entry.ObjOffset) + properties.bytesConsumed);
    const std::int32_t colorCount = stream.ReadIndex();
    if (colorCount <= 0 || colorCount > 65'536 ||
        static_cast<std::uint64_t>(stream.Tell()) +
            static_cast<std::uint64_t>(colorCount) * sizeof(std::uint32_t) >
            static_cast<std::uint64_t>(entry.ObjOffset) + static_cast<std::uint64_t>(entry.ObjSize)) {
        throw std::runtime_error("UE1 palette payload is outside the export");
    }
    std::vector<std::uint32_t> colors(static_cast<std::size_t>(colorCount));
    stream.ReadBytes(colors.data(), static_cast<std::uint32_t>(colors.size() * sizeof(colors[0])));
    if (package.version < 66) {
        for (std::uint32_t& color : colors) color |= 0xff000000u;
    }
    return colors;
}
