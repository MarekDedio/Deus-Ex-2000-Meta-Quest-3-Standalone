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

struct PortableReflectionObject {
    std::string objectPath;
    std::string metaClass;
    std::string outerPath;
    std::string basePath;
    std::uint32_t flags{};
    std::int32_t serializedSize{};
};

struct PortableReflectionGraph {
    std::vector<PortableReflectionObject> objects;
    std::size_t classCount{};
    std::size_t stateCount{};
    std::size_t functionCount{};
    std::size_t propertyCount{};
    std::size_t enumCount{};
    std::size_t structCount{};
};

struct PortableScriptBody {
    std::string objectPath;
    std::uint32_t logicalSize{};
    std::vector<std::uint8_t> rawBytes;
    std::vector<std::uint8_t> bytecode;
    std::uint16_t nativeIndex{};
    std::uint8_t operatorPrecedence{};
    std::uint32_t functionFlags{};
    std::uint16_t replicationOffset{};
};

struct PortablePropertyDescriptor {
    std::string objectPath;
    std::string type;
    std::string outerPath;
    std::int32_t baseField{};
    std::int32_t nextField{};
    std::int32_t arrayDimension{};
    std::uint32_t flags{};
    NameString category;
    std::uint16_t replicationOffset{};
    std::int32_t referencedType{};
    std::int32_t secondaryType{};
    std::int32_t fixedCount{};
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
PortableReflectionGraph BuildPortableReflectionGraph(
    const PortablePackageTables& package);
PortableScriptBody LoadPortableFunctionScript(
    const PortablePackageTables& package,
    std::size_t exportIndex);
PortablePropertyDescriptor LoadPortablePropertyDescriptor(
    const PortablePackageTables& package,
    std::size_t exportIndex);
