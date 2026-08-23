#include "surreal_portable_package_tables.h"

#include "Package/PackageStream.h"
#include "Utils/File.h"

#include <algorithm>
#include <limits>
#include <cstring>
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
    std::int32_t ReadInt32() { return static_cast<std::int32_t>(ReadUInt32()); }
    float ReadFloat() {
        const std::uint32_t bits = ReadUInt32();
        float value{};
        std::memcpy(&value, &bits, sizeof(value));
        return value;
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
    std::size_t Size() const { return bytes_.size(); }
    std::string ReadAsciiZ() {
        std::string result;
        while (true) {
            const char value = static_cast<char>(ReadUInt8());
            if (value == '\0') return result;
            result.push_back(value);
        }
    }
    std::size_t ReadUnicodeZUnits() {
        std::size_t units{};
        while (ReadUInt16() != 0) ++units;
        return units;
    }

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

std::uint8_t DecodeScriptToken(
    PayloadReader& reader,
    const PortablePackageTables& package,
    std::size_t& logicalSize,
    std::vector<std::uint8_t>& bytecode,
    unsigned depth) {
    if (depth >= 64) throw std::runtime_error("UE1 bytecode nesting is too deep");
    const std::uint8_t token = reader.ReadUInt8();
    bytecode.push_back(token);
    ++logicalSize;
    const auto append16 = [&](std::uint16_t value) {
        bytecode.push_back(static_cast<std::uint8_t>(value));
        bytecode.push_back(static_cast<std::uint8_t>(value >> 8u));
    };
    const auto append32 = [&](std::uint32_t value) {
        append16(static_cast<std::uint16_t>(value));
        append16(static_cast<std::uint16_t>(value >> 16u));
    };
    const auto byte = [&]() {
        const std::uint8_t value = reader.ReadUInt8();
        bytecode.push_back(value);
        ++logicalSize;
        return value;
    };
    const auto word = [&]() {
        const std::uint16_t value = reader.ReadUInt16();
        append16(value);
        logicalSize += 2;
        return value;
    };
    const auto dword = [&]() {
        const std::uint32_t value = reader.ReadUInt32();
        append32(value);
        logicalSize += 4;
        return value;
    };
    const auto compactIndex = [&]() {
        const std::int32_t value = reader.ReadIndex();
        append32(static_cast<std::uint32_t>(value));
        logicalSize += 4;
        return value;
    };
    const auto child = [&]() {
        return DecodeScriptToken(reader, package, logicalSize, bytecode, depth + 1);
    };
    if (token >= 0x39u && token <= 0x60u) {
        child();
    } else if (token >= 0x70u) {
        while (child() != 0x16u) {}
    } else if (token >= 0x60u) {
        byte();
        while (child() != 0x16u) {}
    } else if (token == 0x1bu || token == 0x38u) {
        compactIndex();
        while (child() != 0x16u) {}
    } else if (token == 0x1cu) {
        compactIndex();
        while (child() != 0x16u) {}
    } else {
        switch (token) {
            case 0x00: case 0x01: case 0x02: compactIndex(); break;
            case 0x04: if (package.version > 61) child(); break;
            case 0x05: byte(); child(); break;
            case 0x06: word(); break;
            case 0x07: word(); child(); break;
            case 0x08: break;
            case 0x09: word(); child(); break;
            case 0x0a: {
                const std::uint16_t next = word();
                if (next != 0xffffu) child();
                break;
            }
            case 0x0b: break;
            case 0x0c:
                while (true) {
                    const std::int32_t nameIndex = compactIndex();
                    ValidateNameIndex(nameIndex, package.names.size());
                    dword();
                    if (package.names[static_cast<std::size_t>(nameIndex)].Name == "None") break;
                }
                break;
            case 0x0d: case 0x0e: child(); break;
            case 0x0f: case 0x10: child(); child(); break;
            case 0x11: child(); child(); child(); child(); break;
            case 0x12: case 0x19:
                child(); word(); byte(); child(); break;
            case 0x13: compactIndex(); child(); break;
            case 0x14: child(); child(); break;
            case 0x15: case 0x16: case 0x17: break;
            case 0x18: word(); child(); break;
            case 0x1a: child(); child(); break;
            case 0x1d: case 0x1e: dword(); break;
            case 0x1f: {
                const std::string value = reader.ReadAsciiZ();
                bytecode.insert(bytecode.end(), value.begin(), value.end());
                bytecode.push_back(0);
                logicalSize += value.size() + 1;
                break;
            }
            case 0x20: case 0x21: compactIndex(); break;
            case 0x22: dword(); dword(); dword(); break;
            case 0x23: dword(); dword(); dword(); break;
            case 0x24: byte(); break;
            case 0x25: case 0x26: case 0x27: case 0x28: break;
            case 0x29: compactIndex(); break;
            case 0x2a: break;
            case 0x2b: byte(); child(); break;
            case 0x2c: byte(); break;
            case 0x2d: child(); break;
            case 0x2e: compactIndex(); child(); break;
            case 0x2f: child(); word(); break;
            case 0x30: case 0x31: break;
            case 0x32: case 0x33: compactIndex(); child(); child(); break;
            case 0x34: {
                while (word() != 0) {}
                break;
            }
            case 0x36: compactIndex(); child(); break;
            default: throw std::runtime_error("Unknown UE1 script bytecode token " +
                std::to_string(token));
        }
    }
    return token;
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

std::string DecodePortableStringProperty(const PortableTaggedProperty& property) {
    if (property.type != 13u || property.value.empty()) return {};
    PayloadReader reader(property.value);
    const std::int32_t length = reader.ReadIndex();
    if (length == 0 || length == std::numeric_limits<std::int32_t>::min()) return {};
    if (length > 0) {
        const std::vector<std::uint8_t> bytes =
            reader.ReadBytes(static_cast<std::size_t>(length));
        const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{});
        return std::string(bytes.begin(), end);
    }
    const std::size_t characters = static_cast<std::size_t>(-length);
    const std::vector<std::uint8_t> bytes = reader.ReadBytes(characters * 2u);
    std::string result;
    result.reserve(characters);
    for (std::size_t index = 0; index + 1u < bytes.size(); index += 2u) {
        const std::uint16_t character = static_cast<std::uint16_t>(bytes[index]) |
            (static_cast<std::uint16_t>(bytes[index + 1u]) << 8u);
        if (character == 0u) break;
        result.push_back(character <= 0x7fu ? static_cast<char>(character) : '?');
    }
    return result;
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

std::string GetPortableObjectPath(
    const PortablePackageTables& package,
    std::int32_t reference) {
    return ResolvePortableObjectPath(reference, package);
}

PortableSound LoadPortableSound(
    const PortablePackageTables& package,
    std::size_t exportIndex) {
    if (exportIndex >= package.exports.size()) {
        throw std::runtime_error("UE1 sound export index is outside the table");
    }
    const ExportTableEntry& entry = package.exports[exportIndex];
    const PortablePropertyStream properties = LoadPortableExportProperties(package, exportIndex);
    const std::shared_ptr<File> file = File::open_existing(package.sourcePath);
    PackageStream stream(nullptr, file);
    stream.Seek(static_cast<std::uint32_t>(entry.ObjOffset) + properties.bytesConsumed);
    const std::int32_t formatIndex = stream.ReadIndex();
    ValidateNameIndex(formatIndex, package.names.size());
    PortableSound sound;
    sound.format = package.names[static_cast<std::size_t>(formatIndex)].Name;
    if (package.version >= 63) stream.ReadUInt32();
    const std::int32_t byteCount = stream.ReadIndex();
    if (byteCount <= 0 || byteCount > 64 * 1024 * 1024 ||
        static_cast<std::uint64_t>(stream.Tell()) + static_cast<std::uint64_t>(byteCount) >
            static_cast<std::uint64_t>(entry.ObjOffset) + static_cast<std::uint64_t>(entry.ObjSize)) {
        throw std::runtime_error("UE1 sound payload is outside the export");
    }
    sound.data.resize(static_cast<std::size_t>(byteCount));
    stream.ReadBytes(sound.data.data(), static_cast<std::uint32_t>(sound.data.size()));
    return sound;
}

PortableReflectionGraph BuildPortableReflectionGraph(
    const PortablePackageTables& package) {
    PortableReflectionGraph graph;
    graph.objects.reserve(package.exports.size());
    for (std::size_t index = 0; index < package.exports.size(); ++index) {
        const ExportTableEntry& entry = package.exports[index];
        PortableReflectionObject object;
        object.objectPath = ResolvePortableObjectPath(
            static_cast<std::int32_t>(index + 1), package);
        object.outerPath = ResolvePortableObjectPath(entry.ObjOuter, package);
        object.basePath = ResolvePortableObjectPath(entry.ObjBase, package);
        object.flags = static_cast<std::uint32_t>(entry.ObjFlags);
        object.serializedSize = entry.ObjSize;
        if (entry.ObjClass == 0) {
            // UE1 serializes class objects with a null metaclass reference. Their
            // superclass is carried by ObjBase in the export table.
            object.metaClass = "Class";
        } else {
            object.metaClass = ResolvePortableObjectPath(entry.ObjClass, package);
            const std::size_t separator = object.metaClass.find_last_of('.');
            if (separator != std::string::npos) {
                object.metaClass.erase(0, separator + 1);
            }
        }

        if (object.metaClass == "Class") {
            ++graph.classCount;
        } else if (object.metaClass == "State") {
            ++graph.stateCount;
        } else if (object.metaClass == "Function") {
            ++graph.functionCount;
        } else if (object.metaClass == "Enum") {
            ++graph.enumCount;
        } else if (object.metaClass == "Struct") {
            ++graph.structCount;
        } else if (object.metaClass.size() >= 8 &&
            object.metaClass.compare(object.metaClass.size() - 8, 8, "Property") == 0) {
            ++graph.propertyCount;
        }
        graph.objects.push_back(std::move(object));
    }
    return graph;
}

PortableScriptBody LoadPortableFunctionScript(
    const PortablePackageTables& package,
    std::size_t exportIndex) {
    if (exportIndex >= package.exports.size()) {
        throw std::runtime_error("UE1 function export index is outside the table");
    }
    const ExportTableEntry& entry = package.exports[exportIndex];
    std::string metaClass = ResolvePortableObjectPath(entry.ObjClass, package);
    const std::size_t separator = metaClass.find_last_of('.');
    if (separator != std::string::npos) metaClass.erase(0, separator + 1);
    if (metaClass != "Function") {
        throw std::runtime_error("UE1 export is not a Function");
    }
    if (entry.ObjSize <= 0 || entry.ObjOffset < 0) {
        throw std::runtime_error("UE1 function has no payload");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    const std::shared_ptr<File> file = File::open_existing(package.sourcePath);
    file->seek(entry.ObjOffset);
    file->read(bytes.data(), bytes.size());
    PayloadReader reader(bytes);
    const PortablePropertyStream properties = LoadPortableExportProperties(package, exportIndex);
    reader.Skip(properties.bytesConsumed);

    const std::int32_t baseField = reader.ReadIndex();
    const std::int32_t nextField = reader.ReadIndex();
    const std::int32_t scriptText = reader.ReadIndex();
    const std::int32_t children = reader.ReadIndex();
    ValidateObjectReference(baseField, package.imports.size(), package.exports.size());
    ValidateObjectReference(nextField, package.imports.size(), package.exports.size());
    ValidateObjectReference(scriptText, package.imports.size(), package.exports.size());
    ValidateObjectReference(children, package.imports.size(), package.exports.size());
    const std::int32_t friendlyName = reader.ReadIndex();
    ValidateNameIndex(friendlyName, package.names.size());
    reader.Skip(8);

    PortableScriptBody result;
    result.objectPath = ResolvePortableObjectPath(
        static_cast<std::int32_t>(exportIndex + 1), package);
    result.logicalSize = reader.ReadUInt32();
    if (result.logicalSize > 64u * 1024u * 1024u) {
        throw std::runtime_error("UE1 function logical script size is unreasonable");
    }
    const std::size_t rawStart = reader.Tell();
    std::size_t decodedLogical{};
    while (decodedLogical < result.logicalSize) {
        DecodeScriptToken(reader, package, decodedLogical, result.bytecode, 0);
        if (decodedLogical > result.logicalSize) {
            throw std::runtime_error("UE1 bytecode exceeded declared logical size");
        }
    }
    const std::size_t rawEnd = reader.Tell();
    result.rawBytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(rawStart),
        bytes.begin() + static_cast<std::ptrdiff_t>(rawEnd));
    if (result.bytecode.size() != result.logicalSize) {
        throw std::runtime_error("UE1 normalized bytecode size mismatch");
    }
    result.nativeIndex = reader.ReadUInt16();
    result.operatorPrecedence = reader.ReadUInt8();
    result.functionFlags = reader.ReadUInt32();
    if ((result.functionFlags & 0x40u) != 0) {
        result.replicationOffset = reader.ReadUInt16();
    }
    if (reader.Tell() != reader.Size()) {
        throw std::runtime_error("UE1 function payload has trailing bytes");
    }
    return result;
}

PortablePropertyDescriptor LoadPortablePropertyDescriptor(
    const PortablePackageTables& package,
    std::size_t exportIndex) {
    if (exportIndex >= package.exports.size()) {
        throw std::runtime_error("UE1 property export index is outside the table");
    }
    const ExportTableEntry& entry = package.exports[exportIndex];
    std::string metaClass = ResolvePortableObjectPath(entry.ObjClass, package);
    const std::size_t separator = metaClass.find_last_of('.');
    if (separator != std::string::npos) metaClass.erase(0, separator + 1);
    if (metaClass.size() < 8 ||
        metaClass.compare(metaClass.size() - 8, 8, "Property") != 0) {
        throw std::runtime_error("UE1 export is not a Property");
    }
    if (entry.ObjSize <= 0 || entry.ObjOffset < 0) {
        throw std::runtime_error("UE1 property has no payload");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    const std::shared_ptr<File> file = File::open_existing(package.sourcePath);
    file->seek(entry.ObjOffset);
    file->read(bytes.data(), bytes.size());
    PayloadReader reader(std::move(bytes));
    const PortablePropertyStream defaults = LoadPortableExportProperties(package, exportIndex);
    reader.Skip(defaults.bytesConsumed);

    PortablePropertyDescriptor result;
    result.objectPath = ResolvePortableObjectPath(
        static_cast<std::int32_t>(exportIndex + 1), package);
    result.type = metaClass;
    result.outerPath = ResolvePortableObjectPath(entry.ObjOuter, package);
    result.baseField = reader.ReadIndex();
    result.nextField = reader.ReadIndex();
    ValidateObjectReference(result.baseField, package.imports.size(), package.exports.size());
    ValidateObjectReference(result.nextField, package.imports.size(), package.exports.size());
    result.arrayDimension = static_cast<std::int32_t>(reader.ReadUInt32());
    result.flags = reader.ReadUInt32();
    const std::int32_t categoryIndex = reader.ReadIndex();
    ValidateNameIndex(categoryIndex, package.names.size());
    result.category = package.names[static_cast<std::size_t>(categoryIndex)].Name;
    if ((result.flags & 0x20u) != 0) result.replicationOffset = reader.ReadUInt16();

    const auto reference = [&]() {
        const std::int32_t value = reader.ReadIndex();
        ValidateObjectReference(value, package.imports.size(), package.exports.size());
        return value;
    };
    if (metaClass == "ByteProperty" || metaClass == "ObjectProperty" ||
        metaClass == "StructProperty" || metaClass == "ArrayProperty") {
        result.referencedType = reference();
    } else if (metaClass == "ClassProperty") {
        result.referencedType = reference();
        result.secondaryType = reference();
    } else if (metaClass == "MapProperty") {
        result.referencedType = reference();
        result.secondaryType = reference();
    } else if (metaClass == "FixedArrayProperty") {
        result.referencedType = reference();
        result.fixedCount = static_cast<std::int32_t>(reader.ReadUInt32());
    }
    if (result.arrayDimension <= 0 || result.arrayDimension > 1'000'000) {
        throw std::runtime_error("UE1 property array dimension is invalid");
    }
    if (reader.Tell() != reader.Size()) {
        throw std::runtime_error("UE1 property payload has trailing bytes");
    }
    return result;
}

PortableClassDescriptor LoadPortableClassDescriptor(
    const PortablePackageTables& package,
    std::size_t exportIndex) {
    if (exportIndex >= package.exports.size()) {
        throw std::runtime_error("UE1 class export index is outside the table");
    }
    const ExportTableEntry& entry = package.exports[exportIndex];
    if (entry.ObjClass != 0 || entry.ObjSize <= 0 || entry.ObjOffset < 0) {
        throw std::runtime_error("UE1 export is not a serialized Class");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    const std::shared_ptr<File> file = File::open_existing(package.sourcePath);
    file->seek(entry.ObjOffset);
    file->read(bytes.data(), bytes.size());
    PayloadReader reader(std::move(bytes));
    const auto objectReference = [&]() {
        const std::int32_t value = reader.ReadIndex();
        ValidateObjectReference(value, package.imports.size(), package.exports.size());
        return value;
    };
    objectReference(); // UField::BaseField
    objectReference(); // UField::Next
    objectReference(); // UStruct::ScriptText
    objectReference(); // UStruct::Children
    const std::int32_t friendlyName = reader.ReadIndex();
    ValidateNameIndex(friendlyName, package.names.size());
    reader.Skip(8); // Line, TextPos

    PortableClassDescriptor result;
    result.objectPath = ResolvePortableObjectPath(
        static_cast<std::int32_t>(exportIndex + 1), package);
    const std::uint32_t logicalScriptSize = reader.ReadUInt32();
    std::size_t decodedLogical{};
    while (decodedLogical < logicalScriptSize) {
        DecodeScriptToken(
            reader, package, decodedLogical, result.stateBytecode, 0);
        if (decodedLogical > logicalScriptSize) {
            throw std::runtime_error("UE1 class state bytecode exceeded declared size");
        }
    }
    reader.Skip(8 + 8 + 2); // UState probe/ignore masks and label offset
    reader.ReadUInt32(); // UState flags
    if (package.version <= 61) reader.ReadUInt32();
    result.classFlags = reader.ReadUInt32();
    reader.Skip(16); // class GUID
    const std::int32_t dependencyCount = reader.ReadIndex();
    if (dependencyCount < 0 || dependencyCount > 1'000'000) {
        throw std::runtime_error("UE1 class dependency count is invalid");
    }
    result.dependencyCount = static_cast<std::size_t>(dependencyCount);
    for (std::int32_t index = 0; index < dependencyCount; ++index) {
        objectReference();
        reader.Skip(8);
    }
    const std::int32_t packageImportCount = reader.ReadIndex();
    if (packageImportCount < 0 || packageImportCount > 1'000'000) {
        throw std::runtime_error("UE1 class package import count is invalid");
    }
    result.packageImportCount = static_cast<std::size_t>(packageImportCount);
    for (std::int32_t index = 0; index < packageImportCount; ++index) {
        reader.ReadIndex();
    }
    if (package.version >= 62) {
        reader.ReadIndex(); // ClassWithin (package table index, not object reference)
        const std::int32_t configName = reader.ReadIndex();
        ValidateNameIndex(configName, package.names.size());
    }

    while (true) {
        const NameString& name = ReadPayloadName(reader, package);
        if (name == "None") break;
        const std::uint8_t info = reader.ReadUInt8();
        PortableTaggedProperty property;
        property.name = name;
        property.type = info & 0x0fu;
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
        result.defaults.push_back(std::move(property));
    }
    if (reader.Tell() != reader.Size()) {
        throw std::runtime_error("UE1 class payload has trailing bytes");
    }
    return result;
}

PortableLodMesh LoadPortableLodMesh(
    const PortablePackageTables& package,
    std::size_t exportIndex) {
    if (exportIndex >= package.exports.size()) {
        throw std::runtime_error("UE1 LodMesh export index is outside the table");
    }
    const ExportTableEntry& entry = package.exports[exportIndex];
    std::string metaClass = ResolvePortableObjectPath(entry.ObjClass, package);
    const std::size_t separator = metaClass.find_last_of('.');
    if (separator != std::string::npos) metaClass.erase(0, separator + 1);
    if (metaClass != "LodMesh" && metaClass != "SkeletalMesh") {
        throw std::runtime_error("UE1 export is not a LodMesh");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    const std::shared_ptr<File> file = File::open_existing(package.sourcePath);
    file->seek(entry.ObjOffset);
    file->read(bytes.data(), bytes.size());
    PayloadReader reader(std::move(bytes));
    const PortablePropertyStream properties = LoadPortableExportProperties(package, exportIndex);
    reader.Skip(properties.bytesConsumed);
    reader.Skip(41); // UPrimitive bounds and sphere for package version 68.
    const auto count = [&](const char* label) {
        const std::int32_t value = reader.ReadIndex();
        if (value < 0 || value > 20'000'000) {
            throw std::runtime_error(std::string("UE1 LodMesh invalid ") + label + " count");
        }
        return static_cast<std::size_t>(value);
    };
    const auto objectReference = [&]() {
        const std::int32_t value = reader.ReadIndex();
        ValidateObjectReference(value, package.imports.size(), package.exports.size());
        return value;
    };

    reader.ReadUInt32(); // vertex lazy-array end offset
    const std::size_t vertexCount = count("vertex");
    struct Vertex { float x, y, z; };
    std::vector<Vertex> vertices;
    vertices.reserve(vertexCount);
    for (std::size_t index = 0; index < vertexCount; ++index) {
        const std::int16_t x = static_cast<std::int16_t>(reader.ReadUInt16());
        const std::int16_t y = static_cast<std::int16_t>(reader.ReadUInt16());
        const std::int16_t z = static_cast<std::int16_t>(reader.ReadUInt16());
        reader.ReadUInt16();
        vertices.push_back({static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
    }

    reader.ReadUInt32(); // legacy triangle lazy-array end offset
    const std::size_t legacyTriangles = count("legacy triangle");
    reader.Skip(legacyTriangles * 20u);
    const std::size_t animationSequences = count("animation sequence");
    for (std::size_t index = 0; index < animationSequences; ++index) {
        const std::int32_t name = reader.ReadIndex();
        const std::int32_t group = reader.ReadIndex();
        ValidateNameIndex(name, package.names.size());
        ValidateNameIndex(group, package.names.size());
        reader.Skip(8);
        const std::size_t notifications = count("animation notification");
        for (std::size_t notify = 0; notify < notifications; ++notify) {
            reader.ReadFloat();
            const std::int32_t function = reader.ReadIndex();
            ValidateNameIndex(function, package.names.size());
        }
        reader.ReadFloat();
    }
    reader.ReadUInt32();
    reader.Skip(count("vertex connect") * 8u);
    reader.Skip(25 + 16);
    reader.ReadUInt32();
    reader.Skip(count("vertex link") * 4u);

    PortableLodMesh result;
    const std::size_t textureCount = count("texture");
    result.textures.reserve(textureCount);
    for (std::size_t index = 0; index < textureCount; ++index) {
        result.textures.push_back(objectReference());
    }
    reader.Skip(count("bounding box") * 25u);
    reader.Skip(count("bounding sphere") * 16u);
    result.frameVertices = reader.ReadUInt32();
    result.animationFrames = reader.ReadUInt32();
    reader.Skip(8);
    result.scaleX = reader.ReadFloat();
    result.scaleY = reader.ReadFloat();
    result.scaleZ = reader.ReadFloat();
    result.originX = reader.ReadFloat();
    result.originY = reader.ReadFloat();
    result.originZ = reader.ReadFloat();
    reader.Skip(12 + 8);
    const std::size_t textureLods = count("texture LOD");
    reader.Skip(textureLods * 4u);

    reader.Skip(count("collapse point") * 2u);
    reader.Skip(count("face level") * 2u);
    struct Face { std::uint16_t wedge[3]; std::uint16_t material; };
    const std::size_t faceCount = count("face");
    std::vector<Face> faces(faceCount);
    for (Face& face : faces) {
        face.wedge[0] = reader.ReadUInt16();
        face.wedge[1] = reader.ReadUInt16();
        face.wedge[2] = reader.ReadUInt16();
        face.material = reader.ReadUInt16();
    }
    reader.Skip(count("collapse wedge") * 2u);
    struct Wedge { std::uint16_t vertex; std::uint8_t u, v; };
    const std::size_t wedgeCount = count("wedge");
    std::vector<Wedge> wedges(wedgeCount);
    for (Wedge& wedge : wedges) {
        wedge.vertex = reader.ReadUInt16();
        wedge.u = reader.ReadUInt8();
        wedge.v = reader.ReadUInt8();
    }
    const std::size_t materialCount = count("material");
    reader.Skip(materialCount * 8u);
    reader.Skip(count("special face") * 8u);
    reader.ReadUInt32(); // ModelVerts
    const std::uint32_t specialVertices = reader.ReadUInt32();
    reader.Skip(24);
    reader.Skip(count("remapped animation vertex") * 2u);
    reader.ReadUInt32(); // OldFrameVerts

    result.triangles.reserve(faceCount * 3u);
    for (const Face& face : faces) {
        for (const std::uint16_t wedgeIndex : face.wedge) {
            if (wedgeIndex >= wedges.size()) {
                throw std::runtime_error("UE1 LodMesh face wedge is out of bounds");
            }
            const Wedge& wedge = wedges[wedgeIndex];
            const std::uint64_t vertexIndex =
                static_cast<std::uint64_t>(wedge.vertex) + specialVertices;
            if (vertexIndex >= vertices.size()) {
                throw std::runtime_error("UE1 LodMesh wedge vertex is out of bounds");
            }
            const Vertex& vertex = vertices[static_cast<std::size_t>(vertexIndex)];
            result.triangles.push_back({
                (vertex.x - result.originX) * result.scaleX,
                (vertex.y - result.originY) * result.scaleY,
                (vertex.z - result.originZ) * result.scaleZ,
                wedge.u / 255.0f,
                wedge.v / 255.0f,
                face.material});
        }
    }
    if (result.frameVertices == 0 || result.animationFrames == 0 ||
        result.triangles.empty()) {
        throw std::runtime_error("UE1 LodMesh has no renderable first frame");
    }
    return result;
}
