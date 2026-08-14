#pragma once

#include "Utils/Array.h"
#include "Package/PackageTables.h"

#include <cstdint>
#include <string>
#include <vector>

struct PortablePackageTables {
    std::uint16_t version{};
    std::uint16_t licenseeMode{};
    std::uint32_t flags{};
    std::vector<NameTableEntry> names;
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
};

PortablePackageTables LoadPortablePackageTables(const std::string& path);
