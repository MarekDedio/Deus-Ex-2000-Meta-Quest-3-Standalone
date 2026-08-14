// Android reader-only translation unit for SurrealEngine's PackageStream API.
// Serialization behavior mirrors the pinned upstream PackageStream.cpp; the
// writer half is linked later with the UObject/save graph.
#include "Precomp.h"
#include "Package/PackageStream.h"
#include "Package/Package.h"
#include "Utils/File.h"

PackageStream::PackageStream(Package* package, std::shared_ptr<File> file)
    : package(package), file(std::move(file)) {}

void PackageStream::ReadBytes(void* data, uint32_t size) { file->read(data, size); }

int8_t PackageStream::ReadInt8() { int8_t v; ReadBytes(&v, 1); return v; }
int16_t PackageStream::ReadInt16() { int16_t v; ReadBytes(&v, 2); return v; }
int32_t PackageStream::ReadInt32() { int32_t v; ReadBytes(&v, 4); return v; }
int64_t PackageStream::ReadInt64() { int64_t v; ReadBytes(&v, 8); return v; }
float PackageStream::ReadFloat() { float v; ReadBytes(&v, 4); return v; }
uint8_t PackageStream::ReadUInt8() { return static_cast<uint8_t>(ReadInt8()); }
uint16_t PackageStream::ReadUInt16() { return static_cast<uint16_t>(ReadInt16()); }
uint32_t PackageStream::ReadUInt32() { return static_cast<uint32_t>(ReadInt32()); }
uint64_t PackageStream::ReadUInt64() { return static_cast<uint64_t>(ReadInt64()); }

void PackageStream::Seek(uint32_t offset) { file->seek(offset); }
void PackageStream::Skip(uint32_t bytes) { file->seek(file->tell() + bytes); }
uint32_t PackageStream::Tell() { return static_cast<uint32_t>(file->tell()); }

int32_t PackageStream::ReadIndex() {
    uint8_t value = ReadUInt8();
    const bool sign = (value & 0x80u) != 0;
    bool more = (value & 0x40u) != 0;
    int32_t index = value & 0x3f;
    int shift = 6;
    while (more && shift < 32) {
        value = ReadUInt8();
        index |= static_cast<int32_t>(value & 0x7fu) << shift;
        more = (value & 0x80u) != 0;
        shift += 7;
    }
    return sign ? -index : index;
}

std::string PackageStream::ReadString() {
    if (GetVersion() >= 64) {
        const int length = ReadIndex();
        if (length < 0) Exception::Throw("Unicode UE1 strings are not yet supported");
        Array<char> text(static_cast<size_t>(length));
        ReadBytes(text.data(), static_cast<uint32_t>(text.size()));
        text.push_back(0);
        return text.data();
    }
    std::string text;
    for (char c = static_cast<char>(ReadInt8()); c != 0;
         c = static_cast<char>(ReadInt8())) text.push_back(c);
    return text;
}

Package* PackageStream::GetPackage() const { return package; }
int PackageStream::GetVersion() const { return package->GetVersion(); }
