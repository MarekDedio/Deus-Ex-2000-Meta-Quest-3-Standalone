#pragma once

#include "Utils/Array.h"
#include "Package/PackageTables.h"

#include <cstdint>
#include <string>
#include <vector>

struct PortablePackageTables {
    std::string sourcePath;
    std::uint16_t version{};
    std::uint16_t licenseeMode{};
    std::uint32_t flags{};
    std::vector<NameTableEntry> names;
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
};

struct PortableTaggedProperty {
    NameString name;
    std::uint8_t type{};
    NameString structName;
    std::uint32_t arrayIndex{};
    std::uint32_t size{};
    std::uint32_t valueOffset{};
    bool boolValue{};
    std::vector<std::uint8_t> value;
};

struct PortablePropertyStream {
    std::vector<PortableTaggedProperty> properties;
    std::uint32_t bytesConsumed{};
};

struct PortableMipmap {
    std::vector<std::uint8_t> pixels;
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint8_t uBits{};
    std::uint8_t vBits{};
};

struct PortableSound {
    NameString format;
    std::vector<std::uint8_t> data;
};

PortablePackageTables LoadPortablePackageTables(const std::string& path);
PortablePropertyStream LoadPortableExportProperties(
    const PortablePackageTables& package,
    std::size_t exportIndex);
std::size_t FindPortableExport(
    const PortablePackageTables& package,
    const std::string& objectPath);
std::vector<PortableMipmap> LoadPortableTextureMipmaps(
    const PortablePackageTables& package,
    std::size_t exportIndex);
std::int32_t DecodePortableObjectReference(const PortableTaggedProperty& property);
std::vector<std::uint32_t> LoadPortablePalette(
    const PortablePackageTables& package,
    std::size_t exportIndex);
std::string GetPortableObjectPath(
    const PortablePackageTables& package,
    std::int32_t reference);
PortableSound LoadPortableSound(
    const PortablePackageTables& package,
    std::size_t exportIndex);
