#include "surreal_portable_package_tables.h"
#include "quest_actor_transform.h"
#include "portable_model_geometry.h"

#include "Package/PackageStream.h"
#include "Utils/File.h"

#include <algorithm>
#include <filesystem>
#include <map>
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
    std::uint64_t ReadUInt64() {
        const std::uint64_t low = ReadUInt32();
        return low | (static_cast<std::uint64_t>(ReadUInt32()) << 32u);
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
        std::uint64_t magnitude = value & 0x3fu;
        unsigned shift = 6;
        while (more && shift < 32) {
            value = ReadUInt8();
            magnitude |= static_cast<std::uint64_t>(value & 0x7fu) << shift;
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

std::string ResolveDescriptorObjectPath(std::int32_t reference,
    const PortablePackageTables& package);

std::uint8_t DecodeScriptToken(
    PayloadReader& reader,
    const PortablePackageTables& package,
    std::size_t& logicalSize,
    std::vector<std::uint8_t>& bytecode,
    unsigned depth,
    bool validateOperands = false,
    std::size_t logicalLimit = std::numeric_limits<std::size_t>::max(),
    bool validateOperandPaths = false) {
    if (depth >= 64) throw std::runtime_error("UE1 bytecode nesting is too deep");
    const auto logical = [&](const std::size_t amount) {
        if (logicalSize > logicalLimit || amount > logicalLimit - logicalSize)
            throw std::runtime_error("UE1 bytecode exceeded declared logical size");
        logicalSize += amount;
    };
    const std::uint8_t token = reader.ReadUInt8();
    logical(1u);
    bytecode.push_back(token);
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
        logical(1u);
        bytecode.push_back(value);
        return value;
    };
    const auto word = [&]() {
        const std::uint16_t value = reader.ReadUInt16();
        logical(2u);
        append16(value);
        return value;
    };
    const auto dword = [&]() {
        const std::uint32_t value = reader.ReadUInt32();
        logical(4u);
        append32(value);
        return value;
    };
    const auto compactIndex = [&](const bool name = false) {
        const std::int32_t value = reader.ReadIndex();
        if (validateOperands) {
            if (name) ValidateNameIndex(value, package.names.size());
            else if (validateOperandPaths) ResolveDescriptorObjectPath(value, package);
            else ValidateObjectReference(value, package.imports.size(), package.exports.size());
        }
        logical(4u);
        append32(static_cast<std::uint32_t>(value));
        return value;
    };
    const auto child = [&]() {
        return DecodeScriptToken(reader, package, logicalSize, bytecode, depth + 1,
            validateOperands, logicalLimit, validateOperandPaths);
    };
    if (token >= 0x39u && token < 0x60u) {
        child();
    } else if (token >= 0x70u) {
        while (child() != 0x16u) {}
    } else if (token >= 0x60u) {
        byte();
        while (child() != 0x16u) {}
    } else if (token == 0x1bu || token == 0x38u) {
        compactIndex(true);
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
                    const std::int32_t nameIndex = compactIndex(true);
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
                logical(value.size() + 1u);
                bytecode.insert(bytecode.end(), value.begin(), value.end());
                bytecode.push_back(0);
                break;
            }
            case 0x20: compactIndex(); break;
            case 0x21: compactIndex(true); break;
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

// Descriptor identity must not use the permissive rendering lookup's empty
// result or depth truncation for a malformed outer/reference chain.
std::string ResolveDescriptorObjectPath(std::int32_t reference,
    const PortablePackageTables& package) {
    std::vector<std::int32_t> visited;
    std::string result;
    while (reference != 0) {
        ValidateObjectReference(reference, package.imports.size(), package.exports.size());
        if (visited.size() >= 32u ||
            std::find(visited.begin(), visited.end(), reference) != visited.end())
            throw std::runtime_error("UE1 descriptor object identity has a cyclic/deep outer chain");
        visited.push_back(reference);
        std::int32_t nameIndex{}, outer{};
        if (reference > 0) {
            const auto& entry = package.exports[static_cast<std::size_t>(reference - 1)];
            nameIndex = entry.ObjName;
            outer = entry.ObjOuter;
        } else {
            const auto& entry = package.imports[static_cast<std::size_t>(
                -static_cast<std::int64_t>(reference) - 1)];
            nameIndex = entry.ObjName;
            outer = entry.ObjOuter;
        }
        ValidateNameIndex(nameIndex, package.names.size());
        const auto& name = package.names[static_cast<std::size_t>(nameIndex)].Name;
        if (name.IsNone()) throw std::runtime_error("UE1 descriptor object identity has a None name");
        const auto& spelling = name.ToString();
        constexpr std::size_t maxIdentityBytes = 64u * 1024u;
        const std::size_t separatorBytes = result.empty() ? 0u : 1u;
        if (spelling.size() > maxIdentityBytes ||
            separatorBytes > maxIdentityBytes - spelling.size() ||
            result.size() > maxIdentityBytes - spelling.size() - separatorBytes)
            throw std::runtime_error("UE1 descriptor object identity exceeds 64 KiB");
        result = result.empty() ? spelling : spelling + '.' + result;
        reference = outer;
    }
    return result;
}

std::vector<std::uint8_t> ReadDescriptorPayload(const PortablePackageTables& package,
    const std::size_t exportIndex,
    const std::size_t inputByteBudget = 64u * 1024u * 1024u) {
    if (exportIndex >= package.exports.size())
        throw std::runtime_error("UE1 state/class export index is outside the table");
    const auto& entry = package.exports[exportIndex];
    constexpr std::int32_t maxPayload = 64 * 1024 * 1024;
    if (entry.ObjSize <= 0 || entry.ObjSize > maxPayload || entry.ObjOffset < 0)
        throw std::runtime_error("UE1 state/class payload size or offset is invalid");
    if (static_cast<std::size_t>(entry.ObjSize) > inputByteBudget)
        throw std::runtime_error("UE1 descriptor payload exceeds caller byte budget");
    ResolveDescriptorObjectPath(static_cast<std::int32_t>(exportIndex + 1u), package);
    ResolveDescriptorObjectPath(entry.ObjBase, package);
    const auto file = File::open_existing(package.sourcePath);
    const auto fileSize = file->size();
    if (fileSize < 0 || static_cast<std::int64_t>(entry.ObjOffset) > fileSize ||
        static_cast<std::int64_t>(entry.ObjSize) > fileSize - entry.ObjOffset)
        throw std::runtime_error("UE1 state/class payload is outside its source file");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    file->seek(entry.ObjOffset);
    file->read(bytes.data(), bytes.size());
    return bytes;
}

template<class Descriptor>
Descriptor ReadStructHeader(PayloadReader& reader,
    const std::vector<std::uint8_t>& bytes, const PortablePackageTables& package,
    const std::size_t exportIndex, const bool validateOperandPaths = false,
    const std::size_t retainedByteBudget = std::numeric_limits<std::size_t>::max()) {
    Descriptor result;
    result.objectPath = ResolveDescriptorObjectPath(static_cast<std::int32_t>(exportIndex + 1u), package);
    const auto reference = [&] {
        const auto value = reader.ReadIndex();
        ResolveDescriptorObjectPath(value, package);
        return value;
    };
    result.baseField = reference();
    result.nextField = reference();
    result.scriptText = reference();
    result.children = reference();
    result.friendlyName = ReadPayloadName(reader, package);
    if (result.friendlyName.IsNone())
        throw std::runtime_error("UE1 state/class FriendlyName must not be None");
    result.line = reader.ReadUInt32();
    result.textPos = reader.ReadUInt32();
    result.logicalSize = reader.ReadUInt32();
    if (result.logicalSize > 64u * 1024u * 1024u)
        throw std::runtime_error("UE1 state/class logical script size is unreasonable");
    const auto rawStart = reader.Tell();
    if (retainedByteBudget != std::numeric_limits<std::size_t>::max()) {
        // A Core.Struct ends immediately after its script. Charge both copies
        // and identities before decoding/allocating normalized script storage.
        const auto fixed = sizeof(Descriptor) + result.objectPath.capacity() +
            result.friendlyName.ToString().size();
        const auto rawSize = bytes.size() - rawStart;
        if (fixed > retainedByteBudget || rawSize > retainedByteBudget - fixed ||
            result.logicalSize > retainedByteBudget - fixed - rawSize)
            throw std::runtime_error("UE1 struct raw/normalized descriptor exceeds caller byte budget");
        result.bytecode.reserve(result.logicalSize);
    }
    std::size_t logicalSize{};
    while (logicalSize < result.logicalSize)
        DecodeScriptToken(reader, package, logicalSize, result.bytecode, 0u, true,
            result.logicalSize, validateOperandPaths);
    if (result.bytecode.size() != result.logicalSize)
        throw std::runtime_error("UE1 state/class normalized bytecode size mismatch");
    result.rawBytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(rawStart),
        bytes.begin() + static_cast<std::ptrdiff_t>(reader.Tell()));
    return result;
}

PortableStateDescriptor ReadStateHeader(PayloadReader& reader,
    const std::vector<std::uint8_t>& bytes, const PortablePackageTables& package,
    const std::size_t exportIndex) {
    auto result = ReadStructHeader<PortableStateDescriptor>(reader, bytes, package, exportIndex);
    result.probeMask = reader.ReadUInt64();
    result.ignoreMask = reader.ReadUInt64();
    result.labelTableOffset = reader.ReadUInt16();
    result.stateFlags = reader.ReadUInt32();
    // Pinned UState::Load stores these exact bits. In particular 0xffff is not
    // a bytecode address to clamp/reject, and unknown flags are not invented AI.
    return result;
}

// Metadata descriptors need only the end of UObject's prefix, not a retained
// vector for every tagged default. Walk the already bounded payload directly so
// even a malicious sequence of tiny Boolean tags cannot amplify allocations.
void SkipStructObjectPrefix(PayloadReader& reader, const PortablePackageTables& package,
    const ExportTableEntry& entry) {
    if (AnyFlags(entry.ObjFlags, ObjectFlags::HasStack)) {
        const auto function = reader.ReadIndex();
        const auto state = reader.ReadIndex();
        ResolveDescriptorObjectPath(function, package);
        ResolveDescriptorObjectPath(state, package);
        reader.ReadUInt64();
        reader.ReadUInt32();
        if (function != 0) reader.ReadIndex();
    }
    while (!ReadPayloadName(reader, package).IsNone()) {
        const auto info = reader.ReadUInt8();
        const auto type = info & 0x0fu;
        if (type == 10u) ReadPayloadName(reader, package);
        std::uint32_t size{};
        switch ((info & 0x70u) >> 4u) {
            case 0: size = 1u; break;
            case 1: size = 2u; break;
            case 2: size = 4u; break;
            case 3: size = 12u; break;
            case 4: size = 16u; break;
            case 5: size = reader.ReadUInt8(); break;
            case 6: size = reader.ReadUInt16(); break;
            case 7: size = reader.ReadUInt32(); break;
        }
        if (type != 3u && (info & 0x80u) != 0u) {
            const auto first = reader.ReadUInt8();
            if ((first & 0xc0u) == 0xc0u) reader.Skip(3u);
            else if ((first & 0x80u) != 0u) reader.Skip(1u);
        }
        if (type != 3u) reader.Skip(size);
    }
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

    PortablePropertyStream result;
    if (AnyFlags(entry.ObjFlags, ObjectFlags::HasStack)) {
        PortableObjectStack stack;
        stack.functionReference = reader.ReadIndex();
        stack.stateReference = reader.ReadIndex();
        ValidateObjectReference(stack.functionReference, package.imports.size(), package.exports.size());
        ValidateObjectReference(stack.stateReference, package.imports.size(), package.exports.size());
        const std::uint32_t maskLow = reader.ReadUInt32();
        stack.probeMask = static_cast<std::uint64_t>(maskLow) |
            (static_cast<std::uint64_t>(reader.ReadUInt32()) << 32u);
        stack.latentAction = reader.ReadUInt32();
        if (stack.functionReference != 0) stack.logicalOffset = reader.ReadIndex();
        result.stack = stack;
    }
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

std::vector<std::int32_t> LoadPortableObjectReferenceArrayTail(
    const PortablePackageTables& package,
    std::size_t exportIndex) {
    if (exportIndex >= package.exports.size()) {
        throw std::runtime_error("UE1 tail export index is out of range");
    }
    const ExportTableEntry& entry = package.exports[exportIndex];
    const PortablePropertyStream properties = LoadPortableExportProperties(package, exportIndex);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    const std::shared_ptr<File> file = File::open_existing(package.sourcePath);
    file->seek(entry.ObjOffset);
    file->read(bytes.data(), bytes.size());
    PayloadReader reader(std::move(bytes));
    reader.Skip(properties.bytesConsumed);
    const std::int32_t count = reader.ReadIndex();
    if (count < 0 || count > 1'000'000) {
        throw std::runtime_error("UE1 object-reference tail count is invalid");
    }
    std::vector<std::int32_t> references;
    references.reserve(static_cast<std::size_t>(count));
    for (std::int32_t index = 0; index < count; ++index) {
        const std::int32_t reference = reader.ReadIndex();
        ValidateObjectReference(reference, package.imports.size(), package.exports.size());
        references.push_back(reference);
    }
    return references;
}

std::size_t FindPortableExport(
    const PortablePackageTables& package,
    const std::string& objectPath) {
    const NameString requested(objectPath);
    for (std::size_t index = 0; index < package.exports.size(); ++index) {
        if (NameString(ResolvePortableObjectPath(
                static_cast<std::int32_t>(index + 1), package)) == requested) {
            return index;
        }
    }
    throw std::runtime_error("UE1 export was not found: " + objectPath);
}

std::size_t FindPortableTextureExport(
    const PortablePackageTables& package,
    const std::string& objectPath) {
    const NameString requested(objectPath);
    std::size_t match = std::numeric_limits<std::size_t>::max();
    for (std::size_t index = 0; index < package.exports.size(); ++index) {
        if (NameString(ResolvePortableObjectPath(
                static_cast<std::int32_t>(index + 1), package)) != requested) continue;
        const NameString cls(ResolvePortableObjectPath(package.exports[index].ObjClass,package));
        const bool texture = cls == "Engine.Texture" || cls == "Engine.ScriptedTexture" ||
            cls == "Fire.FractalTexture" || cls == "Fire.FireTexture" ||
            cls == "Fire.WaterTexture" || cls == "Fire.WaveTexture" ||
            cls == "Fire.WetTexture" || cls == "Fire.IceTexture";
        if (!texture) continue;
        if (match != std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("UE1 texture path is ambiguous: " + objectPath);
        match = index;
    }
    if (match == std::numeric_limits<std::size_t>::max())
        throw std::runtime_error("UE1 texture export was not found: " + objectPath);
    return match;
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
    // UTexture permits an empty stored mip list. Its procedural subclasses
    // create their working surface from UClamp/VClamp after loading this list.
    if (mipCount > 32) throw std::runtime_error("UE1 mip count is invalid");

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
    if ((property.type != 5u && property.type != 8u) || property.value.empty()) {
        throw std::runtime_error("UE1 property is not an object or class reference");
    }
    PayloadReader reader(property.value);
    const std::int32_t reference = reader.ReadIndex();
    if (reader.Tell() != property.value.size()) {
        throw std::runtime_error("UE1 object-reference property has trailing bytes");
    }
    return reference;
}

std::string DecodePortableNameProperty(
    const PortablePackageTables& package,
    const PortableTaggedProperty& property) {
    if (property.type != 6u || property.value.empty()) return {};
    PayloadReader reader(property.value);
    const std::int32_t index = reader.ReadIndex();
    if (reader.Tell() != property.value.size() || index < 0 ||
        static_cast<std::size_t>(index) >= package.names.size()) {
        throw std::runtime_error("UE1 name property has an invalid name-table index");
    }
    return package.names[static_cast<std::size_t>(index)].Name.ToString();
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

PortableTextureImage DecodePortableIndexedTexture(
    const PortablePackageTables& package,
    const std::string& objectPath,
    const bool transparentIndexZero) {
    const std::size_t textureExport = FindPortableTextureExport(package, objectPath);
    const std::vector<PortableMipmap> mipmaps =
        LoadPortableTextureMipmaps(package, textureExport);
    if (mipmaps.empty()) throw std::runtime_error("UE1 texture has no mipmaps");
    const PortableMipmap& mip = mipmaps.front();
    if (mip.width == 0u || mip.height == 0u ||
        mip.pixels.size() != static_cast<std::size_t>(mip.width) * mip.height) {
        throw std::runtime_error("UE1 texture top mip is malformed");
    }
    const PortablePropertyStream properties =
        LoadPortableExportProperties(package, textureExport);
    std::vector<std::uint32_t> palette;
    for (const PortableTaggedProperty& property : properties.properties) {
        if (property.name != "Palette") continue;
        const std::int32_t paletteReference = DecodePortableObjectReference(property);
        if (paletteReference <= 0) {
            throw std::runtime_error("UE1 texture palette is not a local export");
        }
        palette = LoadPortablePalette(package, static_cast<std::size_t>(paletteReference - 1));
        break;
    }
    if (palette.empty()) throw std::runtime_error("UE1 texture has no palette");

    PortableTextureImage result;
    result.width = mip.width;
    result.height = mip.height;
    result.rgba.reserve(mip.pixels.size() * 4u);
    for (const std::uint8_t index : mip.pixels) {
        if (index >= palette.size()) throw std::runtime_error("UE1 texture palette index overflow");
        const std::uint32_t color = palette[index];
        const bool transparent = transparentIndexZero && index == 0u;
        result.rgba.push_back(transparent ? 0u : static_cast<std::uint8_t>(color));
        result.rgba.push_back(transparent ? 0u : static_cast<std::uint8_t>(color >> 8u));
        result.rgba.push_back(transparent ? 0u : static_cast<std::uint8_t>(color >> 16u));
        result.rgba.push_back(transparent ? 0u : 255u);
    }
    return result;
}

const PortableBitmapFontGlyph* GetPortableBitmapGlyph(
    const PortableBitmapFont& font,
    const std::uint32_t character) {
    const auto find = [&](const std::uint32_t code) -> const PortableBitmapFontGlyph* {
        return code < font.glyphs.size() ? &font.glyphs[code] : nullptr;
    };
    const PortableBitmapFontGlyph* glyph = find(character);
    if (!glyph || glyph->width == 0u) {
        if (character >= 'a' && character <= 'z') glyph = find(character + 'A' - 'a');
        if (!glyph || glyph->width == 0u) glyph = find(32u);
    }
    return glyph;
}

PortableBitmapTextMetrics MeasurePortableBitmapText(
    const PortableBitmapFont& font,
    const std::string& text) {
    PortableBitmapTextMetrics result;
    for (const unsigned char character : text) {
        const PortableBitmapFontGlyph* glyph = GetPortableBitmapGlyph(font, character);
        if (!glyph) continue;
        result.width += glyph->width;
        result.height = std::max(result.height, glyph->height);
    }
    return result;
}

PortableBitmapFont LoadPortableBitmapFont(
    const PortablePackageTables& package,
    const std::size_t exportIndex) {
    // Authority: upstream Engine/Resources/UFont.cpp v64+ branch loads UObject
    // properties, compact page count, object reference and compact glyph count,
    // four signed int32 atlas fields per glyph, then uint32 charactersPerPage.
    if (package.version <= 63u)
        throw std::runtime_error("UE1 legacy texture-derived bitmap fonts are unsupported");
    if (exportIndex >= package.exports.size())
        throw std::runtime_error("UE1 font export index is outside the table");
    const auto& entry = package.exports[exportIndex];
    if (NameString(ResolvePortableObjectPath(entry.ObjClass, package)) != "Engine.Font")
        throw std::runtime_error("UE1 bitmap font export is not Engine.Font");
    constexpr std::uint32_t maxFontPayload = 16u * 1024u * 1024u;
    const std::shared_ptr<File> file = File::open_existing(package.sourcePath);
    const auto validatePayload = [&](const ExportTableEntry& object) {
        if (object.ObjSize <= 0 || object.ObjOffset < 0 ||
            static_cast<std::uint64_t>(object.ObjOffset) +
                static_cast<std::uint64_t>(object.ObjSize) > static_cast<std::uint64_t>(file->size()))
            throw std::runtime_error("UE1 font or atlas export payload is outside the file");
        // Property readers copy the entire export before parsing it. Bound
        // font, atlas and palette payloads before any of those allocations,
        // rather than relying only on the later decoded-pixel budget.
        if (static_cast<std::uint32_t>(object.ObjSize) > maxFontPayload)
            throw std::runtime_error("UE1 bitmap font or atlas payload is too large");
    };
    validatePayload(entry);
    const auto properties = LoadPortableExportProperties(package, exportIndex);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    file->seek(entry.ObjOffset);
    file->read(bytes.data(), bytes.size());
    PayloadReader reader(std::move(bytes));
    reader.Skip(properties.bytesConsumed);
    const auto pageCount = reader.ReadIndex();
    if (pageCount <= 0 || pageCount > 256)
        throw std::runtime_error("UE1 bitmap font page count is invalid");
    PortableBitmapFont result;
    result.objectPath = ResolvePortableObjectPath(static_cast<std::int32_t>(exportIndex + 1u), package);
    result.pages.reserve(static_cast<std::size_t>(pageCount));
    result.texturePaths.reserve(static_cast<std::size_t>(pageCount));
    result.textureReferences.reserve(static_cast<std::size_t>(pageCount));
    std::vector<std::uint32_t> pageGlyphCounts;
    std::uint64_t atlasPixels{};
    for (std::int32_t pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
        const auto textureReference = reader.ReadIndex();
        ValidateObjectReference(textureReference, package.imports.size(), package.exports.size());
        if (textureReference <= 0)
            throw std::runtime_error("UE1 bitmap font atlas must be a local texture export");
        const auto textureExport = static_cast<std::size_t>(textureReference - 1);
        const auto texturePath = ResolvePortableObjectPath(textureReference, package);
        const auto glyphCount = reader.ReadIndex();
        if (glyphCount < 0 || glyphCount > 65'536 ||
            static_cast<std::size_t>(glyphCount) > (reader.Size() - reader.Tell()) / 16u ||
            result.glyphs.size() + static_cast<std::size_t>(glyphCount) > 65'536u)
            throw std::runtime_error("UE1 bitmap font glyph count is invalid or truncated");
        if (FindPortableTextureExport(package, texturePath) != textureExport)
            throw std::runtime_error("UE1 bitmap font atlas reference is not a texture");
        validatePayload(package.exports[textureExport]);
        // Keep font atlas palette validation class-aware too; a same-path
        // Texture/Palette pair must not cause the palette to be read as pixels.
        for (const auto& property : LoadPortableExportProperties(package, textureExport).properties) {
            if (property.name != "Palette") continue;
            const auto paletteReference = DecodePortableObjectReference(property);
            ValidateObjectReference(paletteReference, package.imports.size(), package.exports.size());
            if (paletteReference <= 0 ||
                NameString(ResolvePortableObjectPath(
                    package.exports[static_cast<std::size_t>(paletteReference - 1)].ObjClass, package)) !=
                    "Engine.Palette")
                throw std::runtime_error("UE1 bitmap font atlas palette is not a local Palette export");
            validatePayload(package.exports[static_cast<std::size_t>(paletteReference - 1)]);
        }
        {
            const auto mipmaps = LoadPortableTextureMipmaps(package, textureExport);
            if (mipmaps.empty()) throw std::runtime_error("UE1 bitmap font atlas has no mipmaps");
            atlasPixels += static_cast<std::uint64_t>(mipmaps.front().width) * mipmaps.front().height;
            if (atlasPixels > 16u * 1024u * 1024u)
                throw std::runtime_error("UE1 bitmap font atlas memory limit exceeded");
        }
        auto image = DecodePortableIndexedTexture(package, texturePath, true);
        pageGlyphCounts.push_back(static_cast<std::uint32_t>(glyphCount));
        for (std::int32_t glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex) {
            const auto x = reader.ReadInt32(), y = reader.ReadInt32();
            const auto width = reader.ReadInt32(), height = reader.ReadInt32();
            if (x < 0 || y < 0 || width < 0 || height < 0 ||
                static_cast<std::uint32_t>(x) > image.width ||
                static_cast<std::uint32_t>(y) > image.height ||
                static_cast<std::uint32_t>(width) > image.width - static_cast<std::uint32_t>(x) ||
                static_cast<std::uint32_t>(height) > image.height - static_cast<std::uint32_t>(y))
                throw std::runtime_error("UE1 bitmap font glyph rectangle is outside its atlas");
            result.glyphs.push_back({static_cast<std::uint32_t>(pageIndex),
                static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y),
                static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)});
        }
        result.texturePaths.push_back(texturePath);
        result.textureReferences.push_back(textureReference);
        result.pages.push_back(std::move(image));
    }
    result.charactersPerPage = reader.ReadUInt32();
    if (result.charactersPerPage == 0u || result.charactersPerPage > 65'536u ||
        std::any_of(pageGlyphCounts.begin(), pageGlyphCounts.end(), [&](const auto count) {
            return count > result.charactersPerPage;
        }))
        throw std::runtime_error("UE1 bitmap font characters-per-page is invalid");
    if (reader.Tell() != reader.Size())
        throw std::runtime_error("UE1 bitmap font payload has trailing bytes");
    if (const auto* xGlyph = GetPortableBitmapGlyph(result, 'X')) result.lineHeight = xGlyph->height;
    return result;
}

PortableBitmapFont DecodePortableBitmapFont(
    const PortablePackageTables& package,
    const std::string& objectPath) {
    const NameString requested(objectPath);
    std::size_t match = std::numeric_limits<std::size_t>::max();
    for (std::size_t index = 0; index < package.exports.size(); ++index) {
        if (NameString(ResolvePortableObjectPath(static_cast<std::int32_t>(index + 1u), package)) !=
                requested ||
            NameString(ResolvePortableObjectPath(package.exports[index].ObjClass, package)) !=
                "Engine.Font") continue;
        if (match != std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("UE1 bitmap font path is ambiguous: " + objectPath);
        match = index;
    }
    if (match == std::numeric_limits<std::size_t>::max())
        throw std::runtime_error("UE1 bitmap font export was not found: " + objectPath);
    return LoadPortableBitmapFont(package, match);
}

std::string GetPortableObjectPath(
    const PortablePackageTables& package,
    std::int32_t reference) {
    return ResolvePortableObjectPath(reference, package);
}

std::string ResolvePortableValueObjectReference(
    const PortablePackageTables& package,
    const std::int32_t reference,
    const std::function<bool(const std::string&, const std::string&)>& importedClassMatches) {
    if (reference == 0) return {};
    ValidateObjectReference(reference, package.imports.size(), package.exports.size());
    std::string importedClass;
    if (reference < 0) {
        const auto& entry = package.imports[static_cast<std::size_t>(
            -static_cast<std::int64_t>(reference) - 1)];
        // Package::GetUObject resolves import outers as imports, never exports;
        // a root package entry is not an UObject value in the pinned runtime.
        if (entry.ObjOuter == 0)
            throw std::runtime_error("UE1 value reference names a root package, not an object");
        std::vector<std::int32_t> visited;
        for (auto current = reference; current != 0;) {
            if (current > 0 || visited.size() >= 32u ||
                std::find(visited.begin(), visited.end(), current) != visited.end())
                throw std::runtime_error("UE1 value import has an invalid/cyclic/deep outer chain");
            ValidateObjectReference(current, package.imports.size(), package.exports.size());
            visited.push_back(current);
            const auto& outerEntry = package.imports[static_cast<std::size_t>(
                -static_cast<std::int64_t>(current) - 1)];
            ValidateNameIndex(outerEntry.ObjName, package.names.size());
            const auto& segment = package.names[static_cast<std::size_t>(outerEntry.ObjName)].Name.ToString();
            if (segment.find('.') != std::string::npos || segment.find('\0') != std::string::npos)
                throw std::runtime_error("UE1 value import has an unsupported dotted/NUL path segment");
            current = outerEntry.ObjOuter;
        }
        ValidateNameIndex(entry.ClassName, package.names.size());
        const auto& name = package.names[static_cast<std::size_t>(entry.ClassName)].Name;
        const auto& spelling = name.ToString();
        if (name.IsNone() || spelling.empty() || spelling.size() > 64u * 1024u ||
            spelling.find('\0') != std::string::npos)
            throw std::runtime_error("UE1 value import class name is invalid or exceeds budget");
        importedClass = spelling;
    }
    auto path = ResolveDescriptorObjectPath(reference, package);
    if (reference > 0) {
        const auto stem = std::filesystem::path(package.sourcePath).stem().string();
        constexpr std::size_t maxIdentityBytes = 64u * 1024u;
        if (stem.empty() || stem.size() >= maxIdentityBytes || path.size() > maxIdentityBytes - stem.size() - 1u)
            throw std::runtime_error("UE1 value export identity is invalid or exceeds budget");
        path = stem + '.' + path;
    }
    if (path.find('\0') != std::string::npos)
        throw std::runtime_error("UE1 value object identity contains an embedded NUL");
    if (reference < 0 && (!importedClassMatches || !importedClassMatches(path, importedClass)))
        throw std::runtime_error("UE1 value import violates its declared class provenance");
    return path;
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
    result.baseField = baseField;
    result.nextField = nextField;
    result.children = children;
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

PortableFieldLinks LoadPortableFieldLinks(const PortablePackageTables& package,
    const std::size_t exportIndex) {
    const auto bytes = ReadDescriptorPayload(package, exportIndex);
    const auto& entry = package.exports[exportIndex];
    auto metaClass = ResolveDescriptorObjectPath(entry.ObjClass, package);
    if (entry.ObjClass > 0)
        metaClass = std::filesystem::path(package.sourcePath).stem().string() + '.' + metaClass;
    if (NameString(metaClass) != "Core.Struct" && NameString(metaClass) != "Core.Enum" && NameString(metaClass) != "Core.Const")
        throw std::runtime_error("UE1 field-link export is not Core.Struct/Enum/Const: " +
            ResolveDescriptorObjectPath(static_cast<std::int32_t>(exportIndex + 1u), package) + " (" + metaClass + ")");
    PayloadReader reader(bytes);
    reader.Skip(LoadPortableExportProperties(package, exportIndex).bytesConsumed);
    PortableFieldLinks result{reader.ReadIndex(), reader.ReadIndex()};
    ResolveDescriptorObjectPath(result.baseField, package);
    ResolveDescriptorObjectPath(result.nextField, package);
    return result;
}

PortableStateDescriptor LoadPortableStateDescriptor(
    const PortablePackageTables& package,
    std::size_t exportIndex) {
    if (exportIndex >= package.exports.size())
        throw std::runtime_error("UE1 state export index is outside the table");
    const auto& entry = package.exports[exportIndex];
    std::string metaClass = ResolveDescriptorObjectPath(entry.ObjClass, package);
    if (entry.ObjClass > 0)
        metaClass = std::filesystem::path(package.sourcePath).stem().string() + '.' + metaClass;
    if (NameString(metaClass) != "Core.State")
        throw std::runtime_error("UE1 export is not an actual Core.State");
    const auto bytes = ReadDescriptorPayload(package, exportIndex);
    PayloadReader reader(bytes);
    // Unlike UClass, a UState serializes UObject's tagged properties (and its
    // optional exact HasStack prefix). This does not execute that dormant data.
    reader.Skip(LoadPortableExportProperties(package, exportIndex).bytesConsumed);
    auto result = ReadStateHeader(reader, bytes, package, exportIndex);
    if (reader.Tell() != reader.Size())
        throw std::runtime_error("UE1 state payload has trailing bytes");
    return result;
}

PortableStructDescriptor LoadPortableStructDescriptor(
    const PortablePackageTables& package,
    const std::size_t exportIndex,
    const std::size_t retainedByteBudget) {
    if (exportIndex >= package.exports.size())
        throw std::runtime_error("UE1 struct export index is outside the table");
    const auto& entry = package.exports[exportIndex];
    auto metaClass = ResolveDescriptorObjectPath(entry.ObjClass, package);
    if (entry.ObjClass > 0)
        metaClass = std::filesystem::path(package.sourcePath).stem().string() + '.' + metaClass;
    if (NameString(metaClass) != "Core.Struct")
        throw std::runtime_error("UE1 export is not an actual Core.Struct");
    const auto bytes = ReadDescriptorPayload(package, exportIndex, retainedByteBudget);
    PayloadReader reader(bytes);
    SkipStructObjectPrefix(reader, package, entry);
    auto result = ReadStructHeader<PortableStructDescriptor>(reader, bytes, package, exportIndex, true,
        retainedByteBudget);
    if (reader.Tell() != reader.Size())
        throw std::runtime_error("UE1 struct payload has trailing bytes");
    if (PortableStructRetainedBytes(result) > retainedByteBudget)
        throw std::runtime_error("UE1 struct descriptor capacity exceeds caller byte budget");
    return result;
}

std::size_t PortableStructRetainedBytes(const PortableStructDescriptor& descriptor) {
    std::size_t bytes = sizeof(PortableStructDescriptor);
    for (const auto part : {descriptor.objectPath.capacity(), descriptor.friendlyName.ToString().size(),
                           descriptor.rawBytes.capacity(), descriptor.bytecode.capacity()}) {
        if (part > std::numeric_limits<std::size_t>::max() - bytes)
            throw std::runtime_error("UE1 retained struct descriptor byte charge overflow");
        bytes += part;
    }
    return bytes;
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
    const auto bytes = ReadDescriptorPayload(package, exportIndex);
    PayloadReader reader(bytes);
    const auto objectReference = [&]() {
        const std::int32_t value = reader.ReadIndex();
        ValidateObjectReference(value, package.imports.size(), package.exports.size());
        return value;
    };
    PortableClassDescriptor result;
    // UObject omits tagged class properties, but its HasStack prefix (if
    // present) still precedes UField according to the pinned UObject loader.
    if (AnyFlags(entry.ObjFlags, ObjectFlags::HasStack)) {
        const auto function = objectReference();
        objectReference();
        reader.ReadUInt64();
        reader.ReadUInt32();
        if (function != 0) reader.ReadIndex();
    }
    result.state = ReadStateHeader(reader, bytes, package, exportIndex);
    result.objectPath = result.state.objectPath;
    result.stateBytecode = result.state.bytecode;
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
    if (package.version != 68u || package.licenseeMode != 0u) {
        throw std::runtime_error("UE1 vertex mesh decoder requires Deus Ex version 68/licensee 0");
    }
    const ExportTableEntry& entry = package.exports[exportIndex];
    if (entry.ObjSize <= 0 || entry.ObjSize > 256*1024*1024 || entry.ObjOffset < 0) {
        throw std::runtime_error("UE1 vertex mesh payload size or offset is invalid");
    }
    std::string metaClass = ResolvePortableObjectPath(entry.ObjClass, package);
    const std::size_t separator = metaClass.find_last_of('.');
    if (separator != std::string::npos) metaClass.erase(0, separator + 1);
    if (metaClass != "Mesh" && metaClass != "LodMesh" && metaClass != "SkeletalMesh") {
        throw std::runtime_error("UE1 export is not a supported vertex mesh");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    const std::shared_ptr<File> file = File::open_existing(package.sourcePath);
    file->seek(entry.ObjOffset);
    file->read(bytes.data(), bytes.size());
    PayloadReader reader(std::move(bytes));
    const PortablePropertyStream properties = LoadPortableExportProperties(package, exportIndex);
    reader.Skip(properties.bytesConsumed);
    reader.Skip(41); // UPrimitive bounds and sphere for package version 68.
    const auto count = [&](const char* label, const std::size_t stride = 1u) {
        const std::int32_t value = reader.ReadIndex();
        if (value < 0 || value > 20'000'000 ||
            static_cast<std::size_t>(value) > (reader.Size()-reader.Tell())/stride) {
            throw std::runtime_error(std::string("UE1 LodMesh invalid ") + label + " count");
        }
        return static_cast<std::size_t>(value);
    };
    const auto lazyArrayEnd = [&](const std::uint32_t expected, const char* label) {
        if (static_cast<std::uint64_t>(entry.ObjOffset)+reader.Tell() != expected) {
            throw std::runtime_error(std::string("UE1 vertex mesh inconsistent ") + label + " lazy-array offset");
        }
    };
    const auto objectReference = [&]() {
        const std::int32_t value = reader.ReadIndex();
        ValidateObjectReference(value, package.imports.size(), package.exports.size());
        return value;
    };

    const QuestVr::MeshAnimationLimits animationLimits;
    auto animation = std::make_shared<PortableMeshAnimationData>();
    std::uint64_t retainedBytes = sizeof(PortableMeshAnimationData);
    const auto retain = [&](const std::uint64_t amount) {
        if (amount > animationLimits.maxRetainedBytes ||
            retainedBytes > animationLimits.maxRetainedBytes-amount)
            throw std::runtime_error("UE1 mesh animation retained-data budget exceeded");
        retainedBytes += amount;
    };
    const std::uint32_t verticesEnd = reader.ReadUInt32();
    const std::size_t vertexCount = count("vertex", 8u);
    struct Vertex { float x, y, z; };
    retain(static_cast<std::uint64_t>(vertexCount)*sizeof(PortablePackedMeshVertex));
    animation->frameVerticesPacked.reserve(vertexCount);
    const auto& vertices = animation->frameVerticesPacked;
    for (std::size_t index = 0; index < vertexCount; ++index) {
        const std::int16_t x = static_cast<std::int16_t>(reader.ReadUInt16());
        const std::int16_t y = static_cast<std::int16_t>(reader.ReadUInt16());
        const std::int16_t z = static_cast<std::int16_t>(reader.ReadUInt16());
        reader.ReadUInt16();
        animation->frameVerticesPacked.push_back({x, y, z});
    }
    lazyArrayEnd(verticesEnd, "vertex");

    const std::uint32_t trianglesEnd = reader.ReadUInt32();
    const std::size_t legacyTriangles = count("legacy triangle", 20u);
    struct LegacyTriangle {
        std::uint16_t vertex[3];
        std::uint8_t u[3];
        std::uint8_t v[3];
        std::uint32_t polyFlags;
        std::int32_t textureIndex;
    };
    std::vector<LegacyTriangle> legacyFaces(legacyTriangles);
    for (LegacyTriangle& triangle : legacyFaces) {
        triangle.vertex[0] = reader.ReadUInt16();
        triangle.vertex[1] = reader.ReadUInt16();
        triangle.vertex[2] = reader.ReadUInt16();
        for (std::size_t corner = 0; corner < 3u; ++corner) {
            triangle.u[corner] = reader.ReadUInt8();
            triangle.v[corner] = reader.ReadUInt8();
        }
        triangle.polyFlags = reader.ReadUInt32();
        triangle.textureIndex = static_cast<std::int32_t>(reader.ReadUInt32());
    }
    lazyArrayEnd(trianglesEnd, "triangle");
    const std::size_t animationSequences = count("animation sequence");
    if (animationSequences > animationLimits.maxSequences)
        throw std::runtime_error("UE1 mesh animation sequence budget exceeded");
    retain(static_cast<std::uint64_t>(animationSequences)*sizeof(PortableMeshAnimationSequence));
    animation->sequences.reserve(animationSequences);
    std::size_t totalNotifies = 0u;
    for (std::size_t index = 0; index < animationSequences; ++index) {
        const std::int32_t name = reader.ReadIndex();
        const std::int32_t group = reader.ReadIndex();
        ValidateNameIndex(name, package.names.size());
        ValidateNameIndex(group, package.names.size());
        PortableMeshAnimationSequence sequence;
        sequence.name = package.names[static_cast<std::size_t>(name)].Name.ToString();
        sequence.group = package.names[static_cast<std::size_t>(group)].Name.ToString();
        retain(sequence.name.capacity()+sequence.group.capacity()+2u);
        sequence.startFrame = reader.ReadInt32();
        sequence.numFrames = reader.ReadInt32();
        const std::size_t notifications = count("animation notification");
        if (notifications > animationLimits.maxNotifies-totalNotifies)
            throw std::runtime_error("UE1 mesh animation notification budget exceeded");
        totalNotifies += notifications;
        retain(static_cast<std::uint64_t>(notifications)*sizeof(PortableMeshAnimationNotify));
        sequence.notifies.reserve(notifications);
        for (std::size_t notify = 0; notify < notifications; ++notify) {
            PortableMeshAnimationNotify notification;
            notification.time = reader.ReadFloat();
            if (!std::isfinite(notification.time))
                throw std::runtime_error("UE1 mesh animation notification time is non-finite");
            const std::int32_t function = reader.ReadIndex();
            ValidateNameIndex(function, package.names.size());
            notification.function = package.names[static_cast<std::size_t>(function)].Name.ToString();
            retain(notification.function.capacity()+1u);
            sequence.notifies.push_back(std::move(notification));
        }
        sequence.rate = reader.ReadFloat();
        if (!std::isfinite(sequence.rate))
            throw std::runtime_error("UE1 mesh animation sequence rate is non-finite");
        std::stable_sort(sequence.notifies.begin(), sequence.notifies.end(),
            [](const auto& a, const auto& b) { return a.time < b.time; });
        animation->sequences.push_back(std::move(sequence));
    }
    const std::uint32_t connectsEnd = reader.ReadUInt32();
    reader.Skip(count("vertex connect", 8u) * 8u);
    lazyArrayEnd(connectsEnd, "vertex connect");
    reader.Skip(25 + 16);
    const std::uint32_t linksEnd = reader.ReadUInt32();
    reader.Skip(count("vertex link", 4u) * 4u);
    lazyArrayEnd(linksEnd, "vertex link");

    PortableLodMesh result;
    const std::size_t textureCount = count("texture");
    result.textures.reserve(textureCount);
    for (std::size_t index = 0; index < textureCount; ++index) {
        result.textures.push_back(objectReference());
    }
    reader.Skip(count("bounding box", 25u) * 25u);
    reader.Skip(count("bounding sphere", 16u) * 16u);
    result.frameVertices = reader.ReadUInt32();
    result.animationFrames = reader.ReadUInt32();
    reader.Skip(8);
    result.scaleX = reader.ReadFloat();
    result.scaleY = reader.ReadFloat();
    result.scaleZ = reader.ReadFloat();
    result.originX = reader.ReadFloat();
    result.originY = reader.ReadFloat();
    result.originZ = reader.ReadFloat();
    result.rotationOriginPitch = reader.ReadInt32();
    result.rotationOriginYaw = reader.ReadInt32();
    result.rotationOriginRoll = reader.ReadInt32();
    reader.Skip(8);
    const std::size_t textureLods = count("texture LOD");
    reader.Skip(textureLods * 4u);
    if (result.frameVertices == 0u || result.animationFrames == 0u ||
        static_cast<std::uint64_t>(result.frameVertices)*result.animationFrames > vertices.size() ||
        !QuestVr::IsFiniteActorVector({result.originX, result.originY, result.originZ})) {
        throw std::runtime_error("UE1 mesh animation dimensions or origin are invalid");
    }
    animation->frameVertices = result.frameVertices;
    animation->animationFrames = result.animationFrames;
    animation->scale = {result.scaleX, result.scaleY, result.scaleZ};
    animation->origin = {result.originX, result.originY, result.originZ};
    animation->rotationOriginPitch = result.rotationOriginPitch;
    animation->rotationOriginYaw = result.rotationOriginYaw;
    animation->rotationOriginRoll = result.rotationOriginRoll;
    animation->lodMesh = metaClass != "Mesh";
    for (auto& sequence : animation->sequences)
        sequence.invalidOriginalSpan = !QuestVr::HasUsableMeshAnimationSpan(*animation,sequence);
    QuestVr::ValidateMeshAnimationData(*animation, animationLimits);
    const QuestVr::ActorMatrix3 meshToObject = QuestVr::UnrealActorRotation(
        result.rotationOriginPitch, result.rotationOriginYaw, result.rotationOriginRoll) *
        QuestVr::ActorScaleMatrix({result.scaleX, result.scaleY, result.scaleZ});
    const QuestVr::ActorMatrix3 meshNormalToObject = meshToObject.NormalMatrix();
    // UMesh/ULodMesh smooth unit face normals by serialized vertex identity,
    // not by coincident positions (UV seams need not share a vertex).
    std::vector<QuestVr::ActorVec3> normals(result.frameVertices);
    const auto addFaceNormal = [&](const std::size_t a, const std::size_t b, const std::size_t c) {
        if (a >= result.frameVertices || b >= result.frameVertices || c >= result.frameVertices)
            throw std::runtime_error("UE1 mesh first-frame normal vertex is out of bounds");
        const Vertex v0{static_cast<float>(vertices[a].x), static_cast<float>(vertices[a].y), static_cast<float>(vertices[a].z)};
        const Vertex v1{static_cast<float>(vertices[b].x), static_cast<float>(vertices[b].y), static_cast<float>(vertices[b].z)};
        const Vertex v2{static_cast<float>(vertices[c].x), static_cast<float>(vertices[c].y), static_cast<float>(vertices[c].z)};
        const QuestVr::ActorVec3 u{v1.x-v0.x, v1.y-v0.y, v1.z-v0.z};
        const QuestVr::ActorVec3 v{v2.x-v0.x, v2.y-v0.y, v2.z-v0.z};
        const auto normal = QuestVr::NormalizeActorVector(
            {u.y*v.z-u.z*v.y, u.z*v.x-u.x*v.z, u.x*v.y-u.y*v.x});
        for (const auto index : {a, b, c}) {
            normals[index].x += normal.x; normals[index].y += normal.y; normals[index].z += normal.z;
        }
    };
    const auto makeVertex = [&](const std::size_t index, const float u, const float v,
                                const std::uint16_t material, const std::uint32_t flags) {
        if (index >= result.frameVertices || index >= vertices.size())
            throw std::runtime_error("UE1 mesh first-frame vertex is out of bounds");
        const auto& vertex = vertices[index];
        const auto position = meshToObject.Transform(
            {vertex.x-result.originX, vertex.y-result.originY, vertex.z-result.originZ});
        if (!QuestVr::IsFiniteActorVector(position))
            throw std::runtime_error("UE1 mesh transformed vertex is non-finite");
        const auto normal = QuestVr::NormalizeActorVector(meshNormalToObject.Transform(normals[index]));
        return PortableMeshVertex{position.x, position.y, position.z, u, v, material,
                                  normal.x, normal.y, normal.z, flags};
    };

    if (metaClass == "Mesh") {
        if (reader.Tell() != reader.Size()) throw std::runtime_error("UE1 Mesh payload has trailing bytes");
        result.materialPolyFlags.assign(textureCount, 0u);
        retain(static_cast<std::uint64_t>(legacyFaces.size())*3u*sizeof(std::uint32_t)*2u);
        animation->normalTopology.reserve(legacyFaces.size());
        animation->triangleSourceVertexIndices.reserve(legacyFaces.size()*3u);
        for (const LegacyTriangle& face : legacyFaces) {
            addFaceNormal(face.vertex[0], face.vertex[1], face.vertex[2]);
            animation->normalTopology.push_back({face.vertex[0], face.vertex[1], face.vertex[2]});
        }
        result.triangles.reserve(legacyFaces.size() * 3u);
        for (const LegacyTriangle& face : legacyFaces) {
            if (face.textureIndex < 0 || face.textureIndex > 65535) {
                throw std::runtime_error("UE1 Mesh triangle texture index is invalid");
            }
            for (std::size_t corner = 0; corner < 3u; ++corner) {
                const std::uint16_t vertexIndex = face.vertex[corner];
                if (vertexIndex >= result.frameVertices || vertexIndex >= vertices.size()) {
                    throw std::runtime_error("UE1 Mesh triangle vertex is out of bounds");
                }
                result.triangles.push_back(makeVertex(vertexIndex, face.u[corner]/255.0f,
                    face.v[corner]/255.0f, static_cast<std::uint16_t>(face.textureIndex), face.polyFlags));
                animation->triangleSourceVertexIndices.push_back(vertexIndex);
            }
        }
        if (result.frameVertices == 0u || result.animationFrames == 0u ||
            result.triangles.empty()) {
            throw std::runtime_error("UE1 Mesh has no renderable first frame");
        }
        QuestVr::ValidateMeshAnimationData(*animation, animationLimits);
        result.animation = std::move(animation);
        return result;
    }

    reader.Skip(count("collapse point", 2u) * 2u);
    reader.Skip(count("face level", 2u) * 2u);
    struct Face { std::uint16_t wedge[3]; std::uint16_t material; };
    const std::size_t faceCount = count("face", 8u);
    std::vector<Face> faces(faceCount);
    for (Face& face : faces) {
        face.wedge[0] = reader.ReadUInt16();
        face.wedge[1] = reader.ReadUInt16();
        face.wedge[2] = reader.ReadUInt16();
        face.material = reader.ReadUInt16();
    }
    reader.Skip(count("collapse wedge", 2u) * 2u);
    struct Wedge { std::uint16_t vertex; std::uint8_t u, v; };
    const std::size_t wedgeCount = count("wedge", 4u);
    std::vector<Wedge> wedges(wedgeCount);
    for (Wedge& wedge : wedges) {
        wedge.vertex = reader.ReadUInt16();
        wedge.u = reader.ReadUInt8();
        wedge.v = reader.ReadUInt8();
    }
    const std::size_t materialCount = count("material", 8u);
    result.materialTextureIndices.reserve(materialCount);
    result.materialPolyFlags.reserve(materialCount);
    for (std::size_t index = 0; index < materialCount; ++index) {
        result.materialPolyFlags.push_back(reader.ReadUInt32());
        result.materialTextureIndices.push_back(
            static_cast<std::int32_t>(reader.ReadUInt32()));
    }
    const std::size_t specialFaceCount = count("special face", 8u);
    retain(static_cast<std::uint64_t>(specialFaceCount)*3u*sizeof(std::uint32_t));
    std::vector<std::array<std::uint16_t, 3>> specialFaces;
    specialFaces.reserve(specialFaceCount);
    for (std::size_t index = 0u; index < specialFaceCount; ++index) {
        specialFaces.push_back({reader.ReadUInt16(), reader.ReadUInt16(), reader.ReadUInt16()});
        reader.ReadUInt16(); // Attachment MaterialIndex is not used by FindAttachmentPoints.
    }
    reader.ReadUInt32(); // ModelVerts
    const std::uint32_t specialVertices = reader.ReadUInt32();
    reader.Skip(24);
    const std::size_t remapCount = count("remapped animation vertex", 2u);
    std::vector<std::uint16_t> remappedVertices;
    remappedVertices.reserve(remapCount);
    for (std::size_t index = 0u; index < remapCount; ++index)
        remappedVertices.push_back(reader.ReadUInt16());
    reader.ReadUInt32(); // OldFrameVerts
    if (metaClass == "LodMesh" && reader.Tell() != reader.Size())
        throw std::runtime_error("UE1 LodMesh payload has trailing bytes");

    retain(static_cast<std::uint64_t>(faces.size())*3u*sizeof(std::uint32_t)*2u);
    animation->normalTopology.reserve(faces.size());
    animation->triangleSourceVertexIndices.reserve(faces.size()*3u);
    for (const Face& face : faces) {
        std::size_t source[3]{};
        if (face.material >= materialCount) throw std::runtime_error("UE1 LodMesh material index is out of bounds");
        for (std::size_t corner = 0u; corner < 3u; ++corner) {
            if (face.wedge[corner] >= wedges.size())
                throw std::runtime_error("UE1 LodMesh normal wedge is out of bounds");
            source[corner] = static_cast<std::size_t>(wedges[face.wedge[corner]].vertex)+specialVertices;
        }
        // Match ULodMesh::Load smoothing: normals are built from the direct
        // wedge+SpecialVerts indices; DrawLodMesh then applies ReMapAnimVerts.
        addFaceNormal(source[0], source[1], source[2]);
        animation->normalTopology.push_back({static_cast<std::uint32_t>(source[0]),
            static_cast<std::uint32_t>(source[1]), static_cast<std::uint32_t>(source[2])});
    }

    result.triangles.reserve(faceCount * 3u);
    for (const Face& face : faces) {
        for (const std::uint16_t wedgeIndex : face.wedge) {
            if (wedgeIndex >= wedges.size()) {
                throw std::runtime_error("UE1 LodMesh face wedge is out of bounds");
            }
            const Wedge& wedge = wedges[wedgeIndex];
            std::uint64_t vertexIndex =
                static_cast<std::uint64_t>(wedge.vertex) + specialVertices;
            if (!remappedVertices.empty()) {
                if (vertexIndex >= remappedVertices.size())
                    throw std::runtime_error("UE1 LodMesh animation remap index is out of bounds");
                vertexIndex = remappedVertices[static_cast<std::size_t>(vertexIndex)];
            }
            if (vertexIndex >= vertices.size()) {
                throw std::runtime_error("UE1 LodMesh wedge vertex is out of bounds");
            }
            result.triangles.push_back(makeVertex(static_cast<std::size_t>(vertexIndex),
                wedge.u/255.0f, wedge.v/255.0f, face.material, result.materialPolyFlags[face.material]));
            animation->triangleSourceVertexIndices.push_back(static_cast<std::uint32_t>(vertexIndex));
        }
    }
    if (result.frameVertices == 0 || result.animationFrames == 0 ||
        result.triangles.empty()) {
        throw std::runtime_error("UE1 LodMesh has no renderable first frame");
    }
    animation->specialFaceVertexIndices.reserve(specialFaceCount*3u);
    for (const auto& face : specialFaces) {
        for (const auto directIndex : face) {
            std::uint32_t vertexIndex = directIndex;
            if (!remappedVertices.empty()) {
                if (vertexIndex >= remappedVertices.size())
                    throw std::runtime_error("UE1 LodMesh attachment animation remap index is out of bounds");
                vertexIndex = remappedVertices[vertexIndex];
            }
            animation->specialFaceVertexIndices.push_back(vertexIndex);
        }
    }
    QuestVr::ValidateMeshAnimationData(*animation, animationLimits);
    result.animation = std::move(animation);
    return result;
}

PortableLodMesh LoadPortableBrushMesh(
    const PortablePackageTables& package, std::size_t exportIndex) {
    const auto model = LoadPortableModel68(package,exportIndex);
    if (model.polysReference <= 0) throw std::runtime_error("Mover Model has no local authored Polys");
    const auto polyIndex = static_cast<std::size_t>(model.polysReference-1);
    const auto& entry = package.exports.at(polyIndex);
    if (GetPortableObjectPath(package,entry.ObjClass) != "Engine.Polys" ||
        entry.ObjOffset < 0 || entry.ObjSize <= 0 || entry.ObjSize > 32*1024*1024)
        throw std::runtime_error("Mover Polys export is invalid");
    const auto properties = LoadPortableExportProperties(package,polyIndex);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    const auto file = File::open_existing(package.sourcePath);
    file->seek(entry.ObjOffset); file->read(bytes.data(),bytes.size());
    PayloadReader reader(std::move(bytes));
    reader.Skip(properties.bytesConsumed);
    const auto count = reader.ReadInt32(), capacity = reader.ReadInt32();
    if (count < 0 || count > 100000 || capacity < count)
        throw std::runtime_error("Mover Polys count is invalid");
    const auto vector = [&]() {
        QuestVr::ActorVec3 value{reader.ReadFloat(),reader.ReadFloat(),reader.ReadFloat()};
        if (!QuestVr::IsFiniteActorVector(value)) throw std::runtime_error("Mover polygon vector is non-finite");
        return value;
    };
    const auto dot = [](const QuestVr::ActorVec3& a,const QuestVr::ActorVec3& b) {
        return a.x*b.x+a.y*b.y+a.z*b.z;
    };
    const auto root = std::filesystem::path(package.sourcePath).parent_path().parent_path();
    std::map<std::string,PortablePackageTables> texturePackages;
    std::map<std::int32_t,std::pair<std::uint16_t,std::array<float,2>>> materials;
    PortableLodMesh result;
    for (std::int32_t poly = 0; poly < count; ++poly) {
        const auto vertices = reader.ReadIndex();
        if (vertices < 3 || vertices > 65536) throw std::runtime_error("Mover polygon vertex count is invalid");
        const auto base = vector(), normal = vector(), u = vector(), v = vector();
        std::vector<QuestVr::ActorVec3> points;
        points.reserve(static_cast<std::size_t>(vertices));
        for (std::int32_t corner = 0; corner < vertices; ++corner) points.push_back(vector());
        const auto flags = reader.ReadUInt32();
        const auto actor = reader.ReadIndex(), texture = reader.ReadIndex();
        ValidateObjectReference(actor,package.imports.size(),package.exports.size());
        ValidateObjectReference(texture,package.imports.size(),package.exports.size());
        ReadPayloadName(reader,package); reader.ReadIndex(); reader.ReadIndex();
        const auto panU = static_cast<std::int16_t>(reader.ReadUInt16());
        const auto panV = static_cast<std::int16_t>(reader.ReadUInt16());
        if ((flags & 1u) != 0u) continue;
        auto material = materials.find(texture);
        if (material == materials.end()) {
            if (materials.size() >= 65536u) throw std::runtime_error("Mover material count exceeds uint16");
            const auto slot = static_cast<std::uint16_t>(materials.size());
            std::string path = GetPortableObjectPath(package,texture);
            if (texture > 0) path = std::filesystem::path(package.sourcePath).stem().string()+"."+path;
            std::array<float,2> dimensions{1.0f,1.0f};
            if (texture != 0) {
                const auto separator = path.find('.');
                if (separator == std::string::npos) throw std::runtime_error("Mover texture is not qualified");
                const auto name = path.substr(0,separator);
                const PortablePackageTables* texturePackage = &package;
                if (name != std::filesystem::path(package.sourcePath).stem().string()) {
                    auto found = texturePackages.find(name);
                    if (found == texturePackages.end()) {
                        std::filesystem::path source;
                        for (const auto& candidate : {root/"System"/(name+".u"),root/"Textures"/(name+".utx"),root/"Maps"/(name+".dx")})
                            if (std::filesystem::is_regular_file(candidate)) { source = candidate; break; }
                        if (source.empty()) throw std::runtime_error("Mover texture package is missing: "+name);
                        found = texturePackages.emplace(name,LoadPortablePackageTables(source.string())).first;
                    }
                    texturePackage = &found->second;
                }
                const auto texExport = texture > 0 ? static_cast<std::size_t>(texture-1) :
                    FindPortableTextureExport(*texturePackage,path.substr(separator+1u));
                const auto texProperties = LoadPortableExportProperties(*texturePackage,texExport);
                float scale = 1.0f;
                for (const auto& property : texProperties.properties) {
                    if ((property.name == "USize" || property.name == "VSize") && property.type == 2u && property.value.size() == 4u) {
                        std::int32_t extent{}; std::memcpy(&extent,property.value.data(),4u);
                        if (extent <= 0 || extent > 16384) throw std::runtime_error("Mover texture extent is invalid");
                        dimensions[property.name == "USize" ? 0u : 1u] = static_cast<float>(extent);
                    } else if (property.name == "DrawScale" && property.type == 4u && property.value.size() == 4u)
                        std::memcpy(&scale,property.value.data(),4u);
                }
                if (!std::isfinite(scale) || scale <= 0.0f) throw std::runtime_error("Mover texture DrawScale is invalid");
                dimensions[0] *= scale; dimensions[1] *= scale;
            }
            result.textures.push_back(texture); result.texturePaths.push_back(path);
            result.materialTextureIndices.push_back(slot); result.materialPolyFlags.push_back(flags);
            material = materials.emplace(texture,std::make_pair(slot,dimensions)).first;
        }
        const auto outward = QuestVr::NormalizeActorVector(normal);
        for (std::size_t corner = 1u; corner+1u < points.size(); ++corner) {
            const std::size_t order[3]{0u,corner,corner+1u};
            for (auto index : order) {
                const auto& p = points[index];
                const QuestVr::ActorVec3 delta{p.x-base.x,p.y-base.y,p.z-base.z};
                result.triangles.push_back({p.x,p.y,p.z,
                    (dot(delta,u)+panU)/material->second.second[0],
                    (dot(delta,v)+panV)/material->second.second[1],material->second.first,
                    outward.x,outward.y,outward.z,flags});
            }
            if (result.triangles.size() > 2'000'000u) throw std::runtime_error("Mover triangle budget exceeded");
        }
    }
    if (reader.Tell() != reader.Size()) throw std::runtime_error("Mover Polys has trailing bytes");
    if (result.triangles.empty()) throw std::runtime_error("Mover Polys has no visible geometry");
    result.frameVertices = static_cast<std::uint32_t>(result.triangles.size());
    result.animationFrames = 1u;
    return result;
}
