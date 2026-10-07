#include "Precomp.h"
#include "quest_actor_materials.h"
#include "portable_unreal_runtime.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
using QuestVr::ActorMaterialSource;
using QuestVr::ActorTextureOverrides;
using QuestVr::ResolveActorMeshMaterial;

void Require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

bool Same(const QuestVr::ActorTextureOverride& a, const QuestVr::ActorTextureOverride& b) {
    return a.specified == b.specified && a.path == b.path;
}
bool Same(const ActorTextureOverrides& a, const ActorTextureOverrides& b) {
    if (!Same(a.skin, b.skin) || !Same(a.texture, b.texture)) return false;
    for (std::size_t i = 0; i < a.multiSkins.size(); ++i)
        if (!Same(a.multiSkins[i], b.multiSkins[i])) return false;
    return true;
}

void TestInheritance() {
    ActorTextureOverrides instance, child, parent;
    Require(QuestVr::SetActorTextureOverride(instance, "MultiSkins", 2u, ""),
        "Explicit None array override was not retained");
    Require(QuestVr::SetActorTextureOverride(instance, "MultiSkins", 7u, "Test.Instance7"),
        "Last valid MultiSkins index was rejected");
    Require(QuestVr::SetActorTextureOverride(instance, "Skin", 0u, ""),
        "Explicit None scalar override was not retained");
    QuestVr::SetActorTextureOverride(child, "MultiSkins", 0u, "Test.Child0");
    QuestVr::SetActorTextureOverride(child, "MultiSkins", 2u, "Test.Child2");
    QuestVr::SetActorTextureOverride(child, "MultiSkins", 7u, "Test.Child7");
    QuestVr::SetActorTextureOverride(child, "Skin", 0u, "Test.ChildSkin");
    QuestVr::SetActorTextureOverride(parent, "MultiSkins", 0u, "Test.Parent0");
    QuestVr::SetActorTextureOverride(parent, "MultiSkins", 1u, "Test.Parent1");
    QuestVr::SetActorTextureOverride(parent, "Texture", 0u, "Test.ParentTexture");
    const auto beforeInvalid = instance;
    for (const auto index : {8u, 255u, std::numeric_limits<std::uint32_t>::max()})
        Require(!QuestVr::SetActorTextureOverride(instance, "MultiSkins", index, "Test.Bad"),
            "Out-of-range fixed array index was accepted");
    Require(!QuestVr::SetActorTextureOverride(instance, "Skin", 1u, "Test.Bad") &&
            !QuestVr::SetActorTextureOverride(instance, "Texture", 1u, "Test.Bad") &&
            !QuestVr::SetActorTextureOverride(instance, "Mesh", 0u, "Test.Bad") &&
            Same(instance, beforeInvalid), "Invalid property changed material overrides");
    QuestVr::InheritActorTextureOverrides(instance, child);
    QuestVr::InheritActorTextureOverrides(instance, parent);
    Require(instance.multiSkins[0].path == "Test.Child0" &&
        instance.multiSkins[1].path == "Test.Parent1" &&
        instance.multiSkins[2].specified && instance.multiSkins[2].path.empty() &&
        instance.multiSkins[7].path == "Test.Instance7" &&
        instance.skin.specified && instance.skin.path.empty() &&
        instance.texture.path == "Test.ParentTexture",
        "Nearest per-index inheritance or explicit None suppression is wrong");
}

void TestSelection() {
    ActorTextureOverrides actor;
    QuestVr::SetActorTextureOverride(actor, "Skin", 0u, "Test.Skin");
    QuestVr::SetActorTextureOverride(actor, "Texture", 0u, "Test.Texture");
    QuestVr::SetActorTextureOverride(actor, "MultiSkins", 1u, "Test.Multi1");
    const std::vector<std::string> mesh{"Test.Mesh0", "Test.Mesh1", "Test.Mesh2", ""};
    const std::vector<std::int32_t> materials{2, 0, 1, 3};
    auto selected = ResolveActorMeshMaterial(actor, mesh, materials, 0u);
    Require(selected.validMaterial && selected.textureIndex == 2 &&
        selected.source == ActorMaterialSource::Mesh && selected.texturePath == "Test.Mesh2",
        "LOD material index was mistaken for texture/MultiSkins index");
    selected = ResolveActorMeshMaterial(actor, mesh, materials, 1u);
    Require(selected.source == ActorMaterialSource::Skin && selected.texturePath == "Test.Skin",
        "Skin did not override default texture zero");
    selected = ResolveActorMeshMaterial(actor, mesh, materials, 2u);
    Require(selected.source == ActorMaterialSource::MultiSkin && selected.texturePath == "Test.Multi1",
        "Indexed MultiSkins did not override Skin and mesh defaults");
    selected = ResolveActorMeshMaterial(actor, mesh, materials, 3u);
    Require(selected.source == ActorMaterialSource::Skin, "Skin did not fill a null mesh slot");
    QuestVr::SetActorTextureOverride(actor, "Skin", 0u, "");
    selected = ResolveActorMeshMaterial(actor, mesh, materials, 1u);
    Require(selected.source == ActorMaterialSource::Mesh, "Null Skin suppressed mesh fallback");
    selected = ResolveActorMeshMaterial(actor, mesh, materials, 3u);
    Require(selected.source == ActorMaterialSource::Texture && selected.texturePath == "Test.Texture",
        "Actor.Texture did not fill a remaining null slot");
    QuestVr::SetActorTextureOverride(actor, "Texture", 0u, "");
    QuestVr::SetActorTextureOverride(actor, "MultiSkins", 2u, "Test.Multi2");
    QuestVr::SetActorTextureOverride(actor, "MultiSkins", 7u, "Test.Multi7");
    selected = ResolveActorMeshMaterial(actor, mesh, materials, 3u);
    Require(selected.source == ActorMaterialSource::LastMultiSkin && selected.texturePath == "Test.Multi2",
        "LOD last-MultiSkin fallback did not respect material count");
    const std::vector<std::string> largeMesh(10u);
    selected = ResolveActorMeshMaterial(actor, largeMesh, {}, 9u);
    Require(selected.validMaterial && selected.texturePath == "Test.Multi7",
        "Mesh fallback accessed beyond eight MultiSkins or selected the wrong last entry");
    selected = ResolveActorMeshMaterial({}, std::vector<std::string>{""}, {}, 0u);
    Require(selected.validMaterial && selected.source == ActorMaterialSource::None &&
        selected.texturePath.empty(), "Untextured group did not remain untextured");
    Require(!ResolveActorMeshMaterial(actor, mesh, materials, 4u).validMaterial &&
        !ResolveActorMeshMaterial(actor, mesh, std::vector<std::int32_t>{-1}, 0u).validMaterial &&
        !ResolveActorMeshMaterial(actor, mesh, std::vector<std::int32_t>{4}, 0u).validMaterial &&
        !ResolveActorMeshMaterial(actor, mesh, {}, std::numeric_limits<std::size_t>::max()).validMaterial,
        "Malformed material/texture index was accepted");
    // Unspecified slots must never act as a non-null reference, even if an
    // incorrectly constructed caller leaves stale path data behind.
    ActorTextureOverrides stale;
    stale.skin.path = "Test.Stale";
    stale.multiSkins[0].path = "Test.Stale";
    Require(ResolveActorMeshMaterial(stale, mesh, {}, 0u).texturePath == "Test.Mesh0",
        "Unspecified stale override path took precedence");
}

void TestFlagsAndPalette() {
    Require(QuestVr::ActorMaterialPolyFlags(1u, false, false, false) == 0u &&
        QuestVr::ActorMaterialPolyFlags(2u, false, false, false) == 2u &&
        QuestVr::ActorMaterialPolyFlags(3u, false, false, false) == 4u &&
        QuestVr::ActorMaterialPolyFlags(4u, false, false, false) == 0x40u &&
        QuestVr::ActorMaterialPolyFlags(255u, true, true, true) == 0x00400810u,
        "Actor style/unlit/filter/environment flags differ from original UE1 bits");
    const auto key = QuestVr::ActorPaletteTexel(0x00332211u, 0u);
    Require(key == std::array<std::uint8_t, 4>{0x11u,0x22u,0x33u,0u},
        "Palette-zero RGB was lost for opaque material use");
    for (unsigned index = 1u; index < 256u; ++index)
        Require(QuestVr::ActorPaletteTexel(0x00ff00ffu, static_cast<std::uint8_t>(index)) ==
            std::array<std::uint8_t,4>{255u,0u,255u,255u},
            "Magenta or nonzero palette index was incorrectly made transparent");
}

std::string Stem(const std::string& sourcePath) {
    return std::filesystem::path(sourcePath).stem().string();
}
std::string QualifiedReference(const PortablePackageTables& package, std::int32_t reference) {
    if (reference == 0) return {};
    auto path = GetPortableObjectPath(package, reference);
    return reference > 0 && !path.empty() ? Stem(package.sourcePath) + '.' + path : path;
}

// Independently reads the original instance/class streams; it does not use the
// runtime's flattened object-property dictionary, which loses fixed-array keys.
ActorTextureOverrides OriginalOverrides(
    const PortablePackageTables& map,
    const PortableActorSnapshot& actor,
    const std::map<std::string, PortablePackageTables>& systemPackages,
    std::unordered_map<std::string, PortableClassDescriptor>& classes) {
    const auto separator = actor.objectPath.find('.');
    const auto exportIndex = FindPortableExport(map, actor.objectPath.substr(separator + 1u));
    auto properties = LoadPortableExportProperties(map, exportIndex).properties;
    std::string classPath = QualifiedReference(map, map.exports.at(exportIndex).ObjClass);
    ActorTextureOverrides expected;
    const auto read = [&](const PortablePackageTables& package,
        const std::vector<PortableTaggedProperty>& source) {
        // The last serialized tag for a key is the assignment that remains.
        for (auto it = source.rbegin(); it != source.rend(); ++it) {
            if (it->type != 5u || it->value.empty()) continue;
            QuestVr::ActorTextureOverride* value{};
            if (it->name == "Skin" && it->arrayIndex == 0u) value = &expected.skin;
            else if (it->name == "Texture" && it->arrayIndex == 0u) value = &expected.texture;
            else if (it->name == "MultiSkins" && it->arrayIndex < 8u)
                value = &expected.multiSkins[it->arrayIndex];
            if (value != nullptr && !value->specified) {
                value->specified = true;
                value->path = QualifiedReference(package, DecodePortableObjectReference(*it));
            }
        }
    };
    read(map, properties);
    std::set<std::string> visited;
    while (!classPath.empty()) {
        Require(visited.size() < 128u && visited.insert(classPath).second,
            "Original class ancestry has a cycle or exceeds test bound");
        const auto dot = classPath.find('.');
        Require(dot != std::string::npos, "Original class path is not qualified");
        const auto package = systemPackages.find(classPath.substr(0u, dot));
        if (package == systemPackages.end()) break;
        const auto index = FindPortableExport(package->second, classPath.substr(dot + 1u));
        auto found = classes.find(classPath);
        if (found == classes.end()) found = classes.emplace(classPath,
            LoadPortableClassDescriptor(package->second, index)).first;
        read(package->second, found->second.defaults);
        classPath = QualifiedReference(package->second, package->second.exports.at(index).ObjBase);
    }
    return expected;
}

void TestOriginal(const std::filesystem::path& gameRoot, const std::vector<std::string>& suppliedMaps) {
    static constexpr const char* names[] = {
        "ConSys", "Core", "DeusEx", "DeusExCharacters", "DeusExConAudioAIBarks",
        "DeusExConAudioEndGame", "DeusExConAudioHK_Shared", "DeusExConAudioIntro",
        "DeusExConAudioMission00", "DeusExConAudioMission01", "DeusExConAudioMission02",
        "DeusExConAudioMission03", "DeusExConAudioMission04", "DeusExConAudioMission05",
        "DeusExConAudioMission08", "DeusExConAudioMission09", "DeusExConAudioMission10",
        "DeusExConAudioMission11", "DeusExConAudioMission12", "DeusExConAudioMission14",
        "DeusExConAudioMission15", "DeusExConAudioNYShared", "DeusExConText",
        "DeusExConversations", "DeusExDeco", "DeusExItems", "DeusExSounds", "DeusExText",
        "DeusExUI", "Editor", "Engine", "Extension", "Fire", "IpDrv", "IpServer",
        "MPCharacters", "UBrowser", "UWindow"};
    std::map<std::string, PortablePackageTables> systemPackages;
    std::vector<PortablePackageTables> packages;
    for (const auto* name : names) {
        auto package = LoadPortablePackageTables((gameRoot / "System" / (std::string(name) + ".u")).string());
        systemPackages.emplace(name, package);
        packages.push_back(std::move(package));
    }
    struct Shutdown { ~Shutdown() { ShutdownPortableRuntime(); } } shutdown;
    Require(InitializePortableRuntime(packages).passed, "Original-package runtime initialization failed");
    std::unordered_map<std::string, PortableClassDescriptor> classes;
    const std::vector<std::string> defaultMaps{
        "00_Training", "01_NYC_UNATCOIsland", "06_HongKong_MJ12lab", "15_Area51_Final"};
    const auto& maps = suppliedMaps.empty() ? defaultMaps : suppliedMaps;
    std::size_t overridesSelected{}, totalActors{}, inheritedSlots{}, pickupsChecked{};
    for (const auto& mapName : maps) {
        const auto map = LoadPortablePackageTables((gameRoot / "Maps" / (mapName + ".dx")).string());
        Require(LoadPortableRuntimeMap(map).passed, "Original map runtime load failed");
        const auto actors = GetPortableRuntimeMapActors();
        totalActors += actors.size();
        for (const auto& actor : actors) {
            const auto expected = OriginalOverrides(map, actor, systemPackages, classes);
            if (!Same(actor.materialOverrides, expected))
                throw std::runtime_error("Original indexed material inheritance mismatch for " + actor.objectPath);
            for (const auto& slot : expected.multiSkins)
                if (slot.specified && !slot.path.empty()) ++inheritedSlots;
            Require(actor.texturePath == expected.texture.path,
                "Sprite Texture compatibility path ignored explicit None inheritance");
        }
        const auto meshSummary = DecodePortableRuntimeActorMeshes();
        Require(meshSummary.passed, "Original actor mesh decode failed");
        const auto bank = BuildPortableRuntimeActorTextureArray(32u, 32u);
        Require(bank.passed && bank.texturePolyFlags.size() == bank.texturePaths.size() &&
            bank.rgba.size() == bank.texturePaths.size() * 32u * 32u * 4u,
            "Original complete actor texture bank is malformed");
        std::size_t selected{}, missing{}, masked{};
        if (mapName == "00_Training") {
            const auto plant = std::find(bank.texturePaths.begin(),bank.texturePaths.end(),"DeusExDeco.Skins.Plant2Tex2");
            Require(plant != bank.texturePaths.end() &&
                (bank.texturePolyFlags[static_cast<std::size_t>(plant-bank.texturePaths.begin())] & 2u) != 0u,
                "Original bMasked foliage texture was not marked as masked");
            const auto base = static_cast<std::size_t>(plant-bank.texturePaths.begin());
            Require(bank.maskedTextureLayers.size() == bank.texturePaths.size() && bank.maskedTextureLayers[base] >= 0,
                "Original masked foliage has no separate P8 upload");
            const auto maskedLayer = static_cast<std::size_t>(bank.maskedTextureLayers[base]);
            std::size_t transparent{};
            for (std::size_t pixel = 0u; pixel < 32u*32u; ++pixel) {
                const auto source = (base*32u*32u+pixel)*4u, masked = (maskedLayer*32u*32u+pixel)*4u;
                if (bank.rgba[source+3u] == 0u) {
                    ++transparent;
                    Require(bank.rgba[masked] == 0u && bank.rgba[masked+1u] == 0u && bank.rgba[masked+2u] == 0u,
                        "Masked P8 upload retains fringe color");
                }
            }
            Require(transparent != 0u,"Original foliage fixture did not exercise transparent P8 texels");
        }
        for (const auto& actor : actors) {
            if (actor.meshPath.empty()) continue;
            const auto mesh = GetPortableRuntimeMesh(actor.meshPath);
            std::set<std::uint16_t> materials;
            for (const auto& vertex : mesh.triangles) materials.insert(vertex.material);
            for (const auto material : materials) {
                const auto choice = ResolveActorMeshMaterial(actor.materialOverrides,
                    mesh.texturePaths, mesh.materialTextureIndices, material);
                Require(choice.validMaterial, "Original mesh material index is invalid");
                if (choice.texturePath.empty()) { ++missing; continue; }
                const auto layer = std::find(bank.texturePaths.begin(), bank.texturePaths.end(), choice.texturePath);
                Require(layer != bank.texturePaths.end(), "Actor's selected override texture was not retained in bank");
                if (choice.source == ActorMaterialSource::MultiSkin || choice.source == ActorMaterialSource::Skin ||
                    choice.source == ActorMaterialSource::Texture || choice.source == ActorMaterialSource::LastMultiSkin)
                    ++overridesSelected;
                if (bank.texturePolyFlags[static_cast<std::size_t>(layer - bank.texturePaths.begin())] & 2u) ++masked;
                ++selected;
            }
        }
        const auto inventory = std::find_if(actors.begin(), actors.end(), [](const auto& actor) {
            return actor.inventory && !actor.meshPath.empty();
        });
        if (inventory != actors.end()) {
            const auto result = InteractPortableRuntimeActor(inventory->objectPath);
            Require(result.handled && result.worldChanged, "Original inventory asset retention fixture could not be picked up");
            const auto active = GetPortableRuntimeMapActors();
            const auto full = GetPortableRuntimeMapActors(true);
            Require(std::none_of(active.begin(), active.end(), [&](const auto& actor) {
                    return actor.objectPath == inventory->objectPath;
                }) && std::any_of(full.begin(), full.end(), [&](const auto& actor) {
                    return actor.objectPath == inventory->objectPath && Same(actor.materialOverrides, inventory->materialOverrides);
                }), "Inactive actor lost authored material sources needed by quickload");
            Require(DecodePortableRuntimeActorMeshes().passed, "Inactive-inclusive mesh decode failed");
            const auto retainedBank = BuildPortableRuntimeActorTextureArray(32u, 32u);
            Require(retainedBank.passed && retainedBank.texturePaths == bank.texturePaths &&
                retainedBank.texturePolyFlags == bank.texturePolyFlags,
                "Picking up an actor removed texture layers required for reactivation");
            ++pickupsChecked;
        }
        std::cout << mapName << ": actors=" << actors.size() << ", selectedMaterials=" << selected
            << ", nullMaterials=" << missing << ", textureMaskedMaterials=" << masked
            << ", bankLayers=" << bank.texturePaths.size() << ", decoded=" << bank.decodedTextures
            << ", proceduralOrMissing=" << bank.failedTextures << '\n';
    }
    Require(totalActors != 0u && inheritedSlots != 0u && overridesSelected != 0u && pickupsChecked != 0u,
        "Original fixtures did not exercise indexed overrides and inactive material retention");
    std::cout << "Original material inheritance verified: actors=" << totalActors
        << ", nonnullMultiSkinSlots=" << inheritedSlots << ", selectedActorOverrides=" << overridesSelected
        << ", inactiveRetentionMaps=" << pickupsChecked << " (owned files read-only).\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        TestInheritance(); TestSelection(); TestFlagsAndPalette();
        std::filesystem::path gameRoot;
        std::vector<std::string> maps;
        bool allCampaignMaps{};
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--game-root" && index + 1 < argc) gameRoot = argv[++index];
            else if (argument == "--map" && index + 1 < argc) maps.emplace_back(argv[++index]);
            else if (argument == "--all-campaign-maps") allCampaignMaps = true;
            else throw std::runtime_error("Usage: quest_actor_materials_test [--game-root PATH] [--map NAME]... [--all-campaign-maps]");
        }
        if (allCampaignMaps) {
            Require(!gameRoot.empty() && maps.empty(),"All campaign audit requires a game root without explicit map choices");
            for (const auto& file : std::filesystem::directory_iterator(gameRoot/"Maps")) {
                const auto name = file.path().stem().string();
                if (file.path().extension() == ".dx" && name.size() > 3u &&
                    name[0] >= '0' && name[0] <= '9' && name[1] >= '0' && name[1] <= '9' && name[2] == '_')
                    maps.push_back(name);
            }
            std::sort(maps.begin(),maps.end());
            Require(!maps.empty(),"No numbered campaign maps found");
            std::cout << "Auditing " << maps.size() << " numbered original campaign maps.\n";
        }
        if (!gameRoot.empty()) TestOriginal(std::filesystem::canonical(gameRoot), maps);
        std::cout << "Actor material selection, indexed inheritance and palette/flag controls passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Actor material test failed: " << error.what() << '\n';
        return 1;
    }
}
