#pragma once

#include <cstdint>

#include "Utils/Array.h"
#include "Package/PackageTables.h"

#include <string>
#include <vector>
#include <optional>
#include "quest_mesh_animation.h"

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

// Exact serialized UObject HasStack header. Class-backed dormant records and
// genuine state continuations must remain distinguishable by their actual
// referenced metadata; retaining these bytes does not start/interpret a state.
struct PortableObjectStack {
    std::int32_t functionReference{}, stateReference{};
    std::uint64_t probeMask{};
    std::uint32_t latentAction{};
    std::optional<std::int32_t> logicalOffset;
};

struct PortablePropertyStream {
    std::vector<PortableTaggedProperty> properties;
    std::uint32_t bytesConsumed{};
    std::optional<PortableObjectStack> stack;
};

struct PortableMipmap {
    std::vector<std::uint8_t> pixels;
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint8_t uBits{};
    std::uint8_t vBits{};
};

struct PortableTextureImage {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> rgba;
};

// Original UE1 bitmap font atlas rectangles. Width is also the exact advance;
// UGC does not add spacing between glyphs. Indices follow UFont::FindGlyph's
// concatenation of each page's character array, not Unicode or an SDF font.
struct PortableBitmapFontGlyph {
    std::uint32_t pageIndex{};
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t width{};
    std::uint32_t height{};
};

struct PortableBitmapFont {
    std::string objectPath;
    std::vector<PortableTextureImage> pages;
    std::vector<std::string> texturePaths;
    std::vector<std::int32_t> textureReferences;
    std::vector<PortableBitmapFontGlyph> glyphs;
    std::uint32_t charactersPerPage{};
    std::uint32_t lineHeight{};
};

struct PortableBitmapTextMetrics {
    std::uint64_t width{};
    std::uint32_t height{};
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
    std::int32_t baseField{}, nextField{}, children{};
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

// Exact serialized UField/UStruct/UState metadata. Retention is not state
// execution or evidence that a HasStack actor is a live state continuation.
struct PortableStateDescriptor {
    std::string objectPath;
    std::int32_t baseField{}, nextField{}, scriptText{}, children{};
    NameString friendlyName;
    std::uint32_t line{}, textPos{}, logicalSize{};
    std::vector<std::uint8_t> rawBytes;
    std::vector<std::uint8_t> bytecode;
    std::uint64_t probeMask{}, ignoreMask{};
    // Preserve the authored uint16 verbatim, including the 0xffff sentinel.
    // This decoder does not invent label/statement execution semantics.
    std::uint16_t labelTableOffset{};
    std::uint32_t stateFlags{};
};

// Common UField prefix of Struct/Enum/Const siblings needed to traverse authored
// Children/Next dispatch chains. This does not decode their remaining schema.
struct PortableFieldLinks {
    std::int32_t baseField{}, nextField{};
};
PortableFieldLinks LoadPortableFieldLinks(const PortablePackageTables& package,
    std::size_t exportIndex);

struct PortableClassDescriptor {
    std::string objectPath;
    PortableStateDescriptor state;
    std::uint32_t classFlags{};
    std::vector<std::uint8_t> stateBytecode;
    std::vector<PortableTaggedProperty> defaults;
    std::size_t dependencyCount{};
    std::size_t packageImportCount{};
};

struct PortableMeshVertex {
    float x{};
    float y{};
    float z{};
    float u{};
    float v{};
    std::uint16_t material{};
    float nx{}, ny{}, nz{};
    std::uint32_t polyFlags{};
};

struct PortableLodMesh {
    std::vector<PortableMeshVertex> triangles;
    std::vector<std::int32_t> textures;
    std::vector<std::string> texturePaths;
    std::vector<std::int32_t> materialTextureIndices;
    std::vector<std::uint32_t> materialPolyFlags;
    std::uint32_t frameVertices{};
    std::uint32_t animationFrames{};
    float scaleX{};
    float scaleY{};
    float scaleZ{};
    float originX{};
    float originY{};
    float originZ{};
    std::int32_t rotationOriginPitch{}, rotationOriginYaw{}, rotationOriginRoll{};
    std::shared_ptr<const PortableMeshAnimationData> animation;
};

PortablePackageTables LoadPortablePackageTables(const std::string& path);
PortablePropertyStream LoadPortableExportProperties(
    const PortablePackageTables& package,
    std::size_t exportIndex);
std::vector<std::int32_t> LoadPortableObjectReferenceArrayTail(
    const PortablePackageTables& package,
    std::size_t exportIndex);
std::size_t FindPortableExport(
    const PortablePackageTables& package,
    const std::string& objectPath);
// Texture and Palette exports may share an identical UE1 object path.
std::size_t FindPortableTextureExport(
    const PortablePackageTables& package,
    const std::string& objectPath);
std::vector<PortableMipmap> LoadPortableTextureMipmaps(
    const PortablePackageTables& package,
    std::size_t exportIndex);
std::int32_t DecodePortableObjectReference(const PortableTaggedProperty& property);
std::string DecodePortableNameProperty(
    const PortablePackageTables& package,
    const PortableTaggedProperty& property);
std::string DecodePortableStringProperty(const PortableTaggedProperty& property);
std::vector<std::uint32_t> LoadPortablePalette(
    const PortablePackageTables& package,
    std::size_t exportIndex);
PortableTextureImage DecodePortableIndexedTexture(
    const PortablePackageTables& package,
    const std::string& objectPath,
    bool transparentIndexZero = false);
// v64+ fonts reference atlas textures. Legacy v60-63 texture-derived fonts and
// imported/external atlas references are deliberately rejected, not guessed.
PortableBitmapFont LoadPortableBitmapFont(
    const PortablePackageTables& package,
    std::size_t exportIndex);
PortableBitmapFont DecodePortableBitmapFont(
    const PortablePackageTables& package,
    const std::string& objectPath);
const PortableBitmapFontGlyph* GetPortableBitmapGlyph(
    const PortableBitmapFont& font,
    std::uint32_t character);
// Measures a single raw UE1 byte string exactly as UGC::GetTextSize; wrapping,
// markup and localization are the caller's responsibility.
PortableBitmapTextMetrics MeasurePortableBitmapText(
    const PortableBitmapFont& font,
    const std::string& text);
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
PortableClassDescriptor LoadPortableClassDescriptor(
    const PortablePackageTables& package,
    std::size_t exportIndex);
// Only actual Core.State metaclass exports are accepted; Class metadata is
// retained separately by LoadPortableClassDescriptor via the same header.
PortableStateDescriptor LoadPortableStateDescriptor(
    const PortablePackageTables& package,
    std::size_t exportIndex);
PortableLodMesh LoadPortableLodMesh(
    const PortablePackageTables& package,
    std::size_t exportIndex);
PortableLodMesh LoadPortableBrushMesh(
    const PortablePackageTables& package,
    std::size_t exportIndex);
