#include "Precomp.h"
#include "portable_model_geometry.h"
#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
std::size_t rejectionControls{};
void Require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
void U16(Bytes& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
}
void U32(Bytes& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32u; shift += 8u)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
void U64(Bytes& bytes, std::uint64_t value) {
    U32(bytes, static_cast<std::uint32_t>(value));
    U32(bytes, static_cast<std::uint32_t>(value >> 32u));
}
void Float(Bytes& bytes, float value) {
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(value)); U32(bytes, bits);
}
void Vector(Bytes& bytes, float x, float y, float z) {
    Float(bytes, x); Float(bytes, y); Float(bytes, z);
}
void Index(Bytes& bytes, std::int32_t value) {
    const bool negative = value < 0;
    std::uint32_t magnitude = static_cast<std::uint32_t>(negative
        ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((magnitude & 0x3fu) | (negative ? 0x80u : 0u));
    magnitude >>= 6u;
    if (magnitude != 0u) first |= 0x40u;
    bytes.push_back(first);
    while (magnitude != 0u) {
        auto next = static_cast<std::uint8_t>(magnitude & 0x7fu);
        magnitude >>= 7u;
        if (magnitude != 0u) next |= 0x80u;
        bytes.push_back(next);
    }
}
void Replace32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned index = 0; index < 4u; ++index)
        bytes.at(offset + index) = static_cast<std::uint8_t>(value >> (8u * index));
}
void ExpectFailure(const std::function<void()>& action, const char* description) {
    bool rejected{};
    try { action(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, description); ++rejectionControls;
}

struct Fixture {
    Bytes bytes;
    std::map<std::string, std::size_t> offsets;
    void Mark(const std::string& name) { offsets[name] = bytes.size(); }
    void Bounds() {
        Vector(bytes, 0.0f, 0.0f, 0.0f); Vector(bytes, 64.0f, 64.0f, 64.0f);
        bytes.push_back(1u);
    }
    Fixture() {
        Bounds(); Vector(bytes, 0.0f, 0.0f, 0.0f); Float(bytes, 64.0f);
        Mark("vectors"); Index(bytes, 3);
        Vector(bytes, 0.0f, 0.0f, 1.0f); Vector(bytes, 2.0f, 0.0f, 0.0f); Vector(bytes, 0.0f, 3.0f, 0.0f);
        Mark("points"); Index(bytes, 3);
        Vector(bytes, 0.0f, 0.0f, 0.0f); Vector(bytes, 64.0f, 0.0f, 0.0f); Vector(bytes, 0.0f, 64.0f, 0.0f);
        Mark("nodes"); Index(bytes, 1);
        Vector(bytes, 0.0f, 0.0f, 1.0f); Float(bytes, 0.0f);
        U64(bytes, 0x123456789abcdef0ull); bytes.push_back(5u);
        Mark("node.pool"); Index(bytes, 0);
        Mark("node.surface"); Index(bytes, 0);
        Mark("node.back"); Index(bytes, -1);
        Index(bytes, -1); Index(bytes, -1); Index(bytes, -1); Index(bytes, 0);
        Index(bytes, 0); Mark("node.zone1"); Index(bytes, 63);
        bytes.push_back(3u); Mark("node.leaf0"); U32(bytes, 0); U32(bytes, 0xffffffffu);
        Mark("surfaces"); Index(bytes, 2);
        for (unsigned surface = 0u; surface < 2u; ++surface) {
            Mark("surface.material" + std::to_string(surface)); Index(bytes, -1);
            U32(bytes, surface == 0u ? 0x00000002u : 0x00400000u);
            Mark("surface.base" + std::to_string(surface)); Index(bytes, 0);
            Mark("surface.normal" + std::to_string(surface)); Index(bytes, 0);
            Index(bytes, 1); Index(bytes, 2);
            Mark("surface.lightmap" + std::to_string(surface)); Index(bytes, surface == 0u ? 0 : -1);
            Index(bytes, -1); U16(bytes, 0xfff9u); U16(bytes, 11u); Index(bytes, 5);
        }
        Mark("vertices"); Index(bytes, 3);
        for (unsigned vertex = 0u; vertex < 3u; ++vertex) {
            Mark("vertex.point" + std::to_string(vertex)); Index(bytes, static_cast<std::int32_t>(vertex)); Index(bytes, -1);
        }
        U32(bytes, 0u); Mark("zoneCount"); U32(bytes, 2u);
        Mark("zone.actor"); Index(bytes, 4); U64(bytes, 0x8000000000000001ull); U64(bytes, 0x4000000000000002ull);
        Index(bytes, 0); U64(bytes, 0); U64(bytes, 0);
        Mark("polys"); Index(bytes, 0);
        Mark("lightmaps"); Index(bytes, 2);
        Mark("lm.offset"); U32(bytes, 3u);
        Float(bytes, 1.25f); Float(bytes, -2.5f); Mark("lm.panZ"); Float(bytes, 3.75f);
        Mark("lm.uClamp"); Index(bytes, 9); Index(bytes, 3);
        Mark("lm.uScale"); Float(bytes, 2.0f); Float(bytes, 4.0f);
        Mark("lm.lightActors"); U32(bytes, 0u);
        U32(bytes, 0xffffffffu); Float(bytes, 0.0f); Float(bytes, 0.0f); Float(bytes, 0.0f);
        Index(bytes, 2); Index(bytes, 2); Float(bytes, 8.0f); Float(bytes, 8.0f); U32(bytes, 0xffffffffu);
        Mark("bits"); Index(bytes, 15);
        bytes.insert(bytes.end(), {0xddu, 0xeeu, 0xffu, 0x55u, 0x01u, 0xaau, 0x00u, 0xffu, 0x01u,
                                   0x00u, 0x00u, 0x01u, 0x01u, 0x80u, 0x00u});
        Mark("bounds"); Index(bytes, 1); Bounds();
        Mark("hulls"); Index(bytes, 1); U32(bytes, 0xffffffffu);
        Mark("leaves"); Index(bytes, 1); Index(bytes, 63); Index(bytes, -1); Index(bytes, -1); U64(bytes, 0x8000000000000000ull);
        Mark("lights"); Index(bytes, 3);
        Mark("light0"); Index(bytes, 5); Index(bytes, 5);
        Mark("lightTerminator"); Index(bytes, 0);
        Mark("tail"); U32(bytes, 1u); U32(bytes, 0u);
    }
};

PortableModelGeometry Decode(const Bytes& bytes) { return DecodePortableModel68Body(bytes, 3u, 5u); }
void TestBody() {
    const Fixture fixture;
    const auto model = Decode(fixture.bytes);
    Require(model.bodyBytesConsumed == fixture.bytes.size(), "Body not consumed exactly");
    Require(model.vectors.size() == 3u && model.points.size() == 3u && model.nodes.size() == 1u, "Geometry counts lost");
    Require(model.nodes[0].zoneMask == 0x123456789abcdef0ull && model.nodes[0].nodeFlags == 5u &&
            model.nodes[0].zone0 == 0 && model.nodes[0].zone1 == 63 && model.nodes[0].leaf1 == -1,
            "Node fields/padded zone slots lost");
    Require(model.surfaces[0].normalVector == 0 && model.vectors[0].z == 1.0f &&
            model.surfaces[0].textureU == 1 && model.surfaces[0].textureV == 2 &&
            model.surfaces[0].panU == -7 && model.surfaces[0].panV == 11 &&
            model.surfaces[0].materialReference == -1 && model.surfaces[0].brushActorReference == 5,
            "Authored surface fields lost");
    Require(model.surfaces[1].polyFlags == 0x00400000u && model.surfaces[1].lightMap == -1,
            "PF_Unlit/sentinel lost");
    Require(model.zones.size() == 2u && model.zones[0].actorReference == 4 &&
            model.zones[0].connectivity == 0x8000000000000001ull &&
            model.zones[0].visibility == 0x4000000000000002ull,
            "Zone object/connectivity/visibility lost");
    Require(model.lightMaps[0].panZ == 3.75f && model.lightMaps[0].uScale == 2.0f &&
            model.lightMaps[0].vScale == 4.0f && model.rootOutside == 1 && model.linked == 0,
            "Lightmap metadata/tail flags lost");
    Require(model.lights == std::vector<std::int32_t>({5, 5, 0}), "Ordered duplicates/zero terminator lost");
    Require(GetPortableModelStaticLightCount(model, 0u) == 2u &&
            GetPortableModelStaticLightCount(model, 1u) == 0u, "Static-light count incorrect");
    const auto first = GetPortableModelShadowSpan(model, 0u, 0u);
    const auto second = GetPortableModelShadowSpan(model, 0u, 1u);
    Require(first.offset == 3u && first.pitch == 2u && first.width == 9u &&
            first.height == 3u && first.byteCount == 6u && second.offset == 9u,
            "Padded shadow-row pitch/ordered ordinal incorrect");
    Require(model.lightBits[first.offset] == 0x55u && model.lightBits[second.offset] == 0u,
            "Original mask bytes lost");
    Require(model.leaves[0].visibleZones == 0x8000000000000000ull &&
            model.renderBounds.size() == 1u && model.leafHulls[0] == -1, "Post-LightBits sections lost");
    auto unusedPool = fixture.bytes;
    unusedPool[fixture.offsets.at("vertices")] = 4u;
    const std::size_t unusedOffset = fixture.offsets.at("vertex.point2") + 2u;
    Bytes freeSlot; Index(freeSlot, 3); Index(freeSlot, -1);
    unusedPool.insert(unusedPool.begin() + unusedOffset, freeSlot.begin(), freeSlot.end());
    const auto unusedModel = Decode(unusedPool);
    Require(unusedModel.vertices.size() == 4u && unusedModel.vertices[3].point == 3,
            "Unused serialized pool point==pointCount was not preserved");
    unusedPool[fixture.offsets.at("node.pool")] = 1u;
    ExpectFailure([&] { (void)Decode(unusedPool); }, "Referenced invalid free pool point accepted");

    // Every byte boundary exercises prefix, every array, each scalar, and tail.
    for (std::size_t length = 0u; length < fixture.bytes.size(); ++length) {
        const Bytes truncated(fixture.bytes.begin(), fixture.bytes.begin() + length);
        ExpectFailure([&] { (void)Decode(truncated); }, "Truncated body accepted");
    }
    auto bad = fixture.bytes; bad.push_back(0u);
    ExpectFailure([&] { (void)Decode(bad); }, "Trailing body byte accepted");
    ExpectFailure([&] { (void)DecodePortableModel68Body(fixture.bytes, 3u, 5u, 67u, 0u); }, "Unsupported version accepted");
    ExpectFailure([&] { (void)DecodePortableModel68Body(fixture.bytes, 3u, 5u, 68u, 1u); }, "Unsupported licensee layout accepted");
    const auto byteMutation = [&](const char* field, std::uint8_t value) {
        auto bytes = fixture.bytes; bytes.at(fixture.offsets.at(field)) = value;
        ExpectFailure([&] { (void)Decode(bytes); }, field);
    };
    for (const auto* section : {"vectors", "points", "nodes", "surfaces", "vertices", "lightmaps",
                                 "bits", "bounds", "hulls", "leaves", "lights"}) {
        byteMutation(section, 0x81u); // Negative compact array count.
        auto bytes = fixture.bytes;
        const std::size_t offset = fixture.offsets.at(section);
        Bytes excess; Index(excess, 2'000'001);
        bytes.erase(bytes.begin() + offset); bytes.insert(bytes.begin() + offset, excess.begin(), excess.end());
        ExpectFailure([&] { (void)Decode(bytes); }, "Excessive/truncated array count accepted");
    }
    byteMutation("node.pool", 3u); byteMutation("node.surface", 2u);
    byteMutation("node.back", 1u); byteMutation("node.zone1", 0x40u);
    byteMutation("surface.material0", 0x84u); byteMutation("surface.base0", 3u);
    byteMutation("surface.normal0", 3u); byteMutation("surface.lightmap0", 2u);
    byteMutation("vertex.point0", 3u); byteMutation("zone.actor", 6u);
    byteMutation("polys", 6u); byteMutation("lm.uClamp", 0u);
    byteMutation("light0", 6u); byteMutation("lightTerminator", 5u);
    for (const auto& mutation : std::vector<std::pair<const char*, std::uint32_t>>{
             {"zoneCount", 65u}, {"node.leaf0", 1u}, {"lm.offset", 4u},
             {"lm.lightActors", 3u}, {"lm.uScale", 0u}, {"lm.panZ", 0x7fc00000u}}) {
        auto bytes = fixture.bytes; Replace32(bytes, fixture.offsets.at(mutation.first), mutation.second);
        ExpectFailure([&] { (void)Decode(bytes); }, mutation.first);
    }
    bad = fixture.bytes; bad[24u] = 2u;
    ExpectFailure([&] { (void)Decode(bad); }, "Invalid bounding-box validity accepted");
    bad = fixture.bytes; Replace32(bad, 0u, 0x7f800000u);
    ExpectFailure([&] { (void)Decode(bad); }, "Infinite bound accepted");
    bad = fixture.bytes;
    const std::size_t countOffset = fixture.offsets.at("vectors");
    bad.erase(bad.begin() + countOffset);
    bad.insert(bad.begin() + countOffset, {0x40u, 0x80u, 0x80u, 0x80u, 0x10u});
    ExpectFailure([&] { (void)Decode(bad); }, "Overflowing fifth compact byte accepted");
    auto changed = model; changed.lights.back() = 5;
    ExpectFailure([&] { (void)GetPortableModelStaticLightCount(changed, 0u); }, "Mutated unterminated light list accepted");
    changed = model; changed.lightMaps[0].dataOffset = 4;
    ExpectFailure([&] { (void)GetPortableModelShadowSpan(changed, 0u, 1u); }, "Mutated shadow span exceeds LightBits");
    ExpectFailure([&] { (void)GetPortableModelShadowSpan(model, 0u, 2u); }, "Out-of-range light ordinal accepted");
    ExpectFailure([&] { (void)GetPortableModelStaticLightCount(model, 2u); }, "Out-of-range lightmap index accepted");
}

struct TempFixture {
    std::filesystem::path directory;
    PortablePackageTables package;
    Bytes payload;
    TempFixture() {
        directory = std::filesystem::temp_directory_path() /
            ("deusex-model-fixture-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(!std::filesystem::exists(directory), "Temporary model fixture directory collision");
        std::filesystem::create_directory(directory);
        package.sourcePath = (directory / "synthetic-model.bin").string();
        package.version = 68; package.licenseeMode = 0;
        for (const auto* name : {"None", "Engine", "Model", "Level", "FirstBrush", "ActualWorld", "MyLevel", "Zone", "Light"})
            package.names.push_back({NameString(name), 0});
        package.imports = {{0, 0, 0, 1}, {0, 0, -1, 2}, {0, 0, -1, 3}};
        package.exports = {{-2, 0, 0, 4, ObjectFlags{}, 0, 0},
                           {-2, 0, 0, 5, ObjectFlags{}, 0, 0},
                           {-3, 0, 0, 6, ObjectFlags{}, 0, 0},
                           {-2, 0, 0, 7, ObjectFlags{}, 0, 0},
                           {-2, 0, 0, 8, ObjectFlags{}, 0, 0}};
        const Fixture body;
        Bytes model; Index(model, 0); model.insert(model.end(), body.bytes.begin(), body.bytes.end());
        Bytes level; Index(level, 0); U32(level, 4); U32(level, 4);
        Index(level, 5); Index(level, 0); Index(level, 4); Index(level, 5);
        for (unsigned string = 0u; string < 4u; ++string) Index(level, 0);
        Index(level, 0); U32(level, 7777); U32(level, 0); Index(level, 2);
        Index(level, 0); // ULevel reach-spec count; outside the model decoder.
        const std::vector<Bytes> exports{model, model, level, model, model};
        for (std::size_t index = 0u; index < exports.size(); ++index) {
            package.exports[index].ObjOffset = static_cast<std::int32_t>(payload.size());
            package.exports[index].ObjSize = static_cast<std::int32_t>(exports[index].size());
            payload.insert(payload.end(), exports[index].begin(), exports[index].end());
        }
        std::ofstream stream(package.sourcePath, std::ios::binary | std::ios::trunc);
        Require(static_cast<bool>(stream), "Cannot create synthetic model payload");
        stream.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        Require(static_cast<bool>(stream), "Cannot write synthetic model payload");
    }
    ~TempFixture() {
        // Remove only this program's exact synthetic file and empty directory.
        std::error_code ignored;
        std::filesystem::remove(package.sourcePath, ignored);
        std::filesystem::remove(directory, ignored);
    }
};
void TestFileLoader() {
    TempFixture fixture;
    Require(FindPortableRootModel68Export(fixture.package) == 1u,
            "Root selected first brush instead of Level Model reference");
    const auto model = LoadPortableRootModel68(fixture.package);
    Require(model.exportIndex == 1u && model.objectPath == "ActualWorld" && model.lightBits.size() == 15u,
            "File loader lost root identity/body");
    Require(ReadPortableLevel68ActorOrder(fixture.package) == std::vector<std::int32_t>({5, 0, 4, 5}),
            "Level actor order dropped duplicate/null entries");
    auto package = fixture.package; package.version = 69;
    ExpectFailure([&] { (void)LoadPortableRootModel68(package); }, "File loader unsupported version accepted");
    package = fixture.package; package.exports[1].ObjSize = 0;
    ExpectFailure([&] { (void)LoadPortableModel68(package, 1u); }, "Zero model payload accepted");
    package = fixture.package; package.exports[1].ObjSize = std::numeric_limits<std::int32_t>::max();
    ExpectFailure([&] { (void)LoadPortableModel68(package, 1u); }, "Oversized model export allocated");
    package = fixture.package; package.exports[1].ObjOffset = static_cast<std::int32_t>(fixture.payload.size());
    ExpectFailure([&] { (void)LoadPortableModel68(package, 1u); }, "Model payload outside file accepted");
    package = fixture.package; package.exports[1].ObjClass = -3;
    ExpectFailure([&] { (void)LoadPortableModel68(package, 1u); }, "Non-Model export accepted");
    package = fixture.package; package.exports[4].ObjClass = -3;
    ExpectFailure([&] { (void)FindPortableRootModel68Export(package); }, "Ambiguous Level exports accepted");
    ExpectFailure([&] { (void)LoadPortableModel68(fixture.package, 5u); }, "Out-of-table export accepted");
}

std::uint64_t HashBytes(const Bytes& bytes) {
    std::uint64_t value = 14695981039346656037ull;
    for (const auto byte : bytes) value = (value ^ byte) * 1099511628211ull;
    return value;
}
std::string JsonString(const std::string& text) {
    std::string result = "\"";
    for (const char value : text) {
        if (value == '"' || value == '\\') result.push_back('\\');
        result.push_back(value);
    }
    return result + "\"";
}
void AuditMap(const std::filesystem::path& path) {
    const auto package = LoadPortablePackageTables(path.string());
    const auto model = LoadPortableRootModel68(package);
    std::size_t maxLights{}, maxSamples{}, maskBytes{}, unlit{};
    for (std::size_t index = 0u; index < model.lightMaps.size(); ++index) {
        const std::size_t count = GetPortableModelStaticLightCount(model, index);
        maxLights = std::max(maxLights, count);
        const auto& lm = model.lightMaps[index];
        maxSamples = std::max(maxSamples, static_cast<std::size_t>(lm.uClamp) * static_cast<std::size_t>(lm.vClamp));
        if (count != 0u) maskBytes += GetPortableModelShadowSpan(model, index, count - 1u).byteCount * count;
    }
    for (const auto& surface : model.surfaces) if ((surface.polyFlags & 0x00400000u) != 0u) ++unlit;
    std::cout << "{\"map\":" << JsonString(path.stem().string())
              << ",\"rootModel\":" << JsonString(model.objectPath)
              << ",\"modelReference\":" << model.exportIndex + 1u
              << ",\"nodes\":" << model.nodes.size() << ",\"surfaces\":" << model.surfaces.size()
              << ",\"zones\":" << model.zones.size() << ",\"lightMaps\":" << model.lightMaps.size()
              << ",\"lightBitsBytes\":" << model.lightBits.size()
              << ",\"lightListEntries\":" << model.lights.size()
              << ",\"maximumLightsPerList\":" << maxLights
              << ",\"maximumLightmapSamples\":" << maxSamples
              << ",\"shadowBytesAcrossLists\":" << maskBytes << ",\"unlitSurfaces\":" << unlit
              << ",\"lightBitsFnv1a64\":\"" << std::hex << HashBytes(model.lightBits) << std::dec
              << "\",\"bodyBytesConsumed\":" << model.bodyBytesConsumed << ",\"passed\":true}\n";
}
}

int main(int argc, char** argv) {
    try {
        std::string gameRoot, mapName = "00_Training";
        bool allMaps{};
        for (int argument = 1; argument < argc; ++argument) {
            const std::string option = argv[argument];
            if (option == "--game-root" && argument + 1 < argc) gameRoot = argv[++argument];
            else if (option == "--map" && argument + 1 < argc) mapName = argv[++argument];
            else if (option == "--all-maps") allMaps = true;
            else throw std::runtime_error("Usage: portable_model_geometry_test [--game-root PATH [--map NAME | --all-maps]]");
        }
        TestBody(); TestFileLoader();
        std::cout << "Portable UModel v68 synthetic body/file tests passed; rejection controls=" << rejectionControls << '\n';
        if (!gameRoot.empty()) {
            const auto maps = std::filesystem::path(gameRoot) / "Maps";
            std::vector<std::filesystem::path> paths;
            if (allMaps) {
                for (const auto& entry : std::filesystem::directory_iterator(maps)) {
                    if (!entry.is_regular_file()) continue;
                    std::string extension = entry.path().extension().string();
                    for (char& character : extension) if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
                    if (extension == ".dx") paths.push_back(entry.path());
                }
                std::sort(paths.begin(), paths.end());
            } else {
                Require(!mapName.empty() && mapName.find_first_not_of(
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos,
                    "Map name must be a simple map basename");
                paths.push_back(maps / (mapName + ".dx"));
            }
            Require(!paths.empty(), "No original maps found");
            for (const auto& path : paths) {
                try { AuditMap(path); }
                catch (const std::exception& error) {
                    throw std::runtime_error("Map " + path.filename().string() + ": " + error.what());
                }
            }
            std::cout << "Original UModel map audits passed=" << paths.size()
                      << "; authored source data only, no XR/render/playability claim\n";
        } else if (allMaps) throw std::runtime_error("--all-maps requires --game-root");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Portable model test failed: " << error.what() << '\n';
        return 1;
    }
}
