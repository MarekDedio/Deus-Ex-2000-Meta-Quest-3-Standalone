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
};

struct PortablePropertyStream {
    std::vector<PortableTaggedProperty> properties;
    std::uint32_t bytesConsumed{};
};

PortablePackageTables LoadPortablePackageTables(const std::string& path);
PortablePropertyStream LoadPortableExportProperties(
    const PortablePackageTables& package,
    std::size_t exportIndex);
