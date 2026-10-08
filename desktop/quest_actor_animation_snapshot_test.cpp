#include "Precomp.h"
#include "portable_unreal_runtime.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
void Require(const bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
bool SameFloat(const float a, const float b) {
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}
bool Same(const PortableActorBlendAnimationSnapshot& a,
    const PortableActorBlendAnimationSnapshot& b) {
    return a.sequence == b.sequence && SameFloat(a.frame, b.frame) &&
        SameFloat(a.rate, b.rate) && SameFloat(a.last, b.last) &&
        SameFloat(a.minRate, b.minRate) && SameFloat(a.tweenRate, b.tweenRate) &&
        SameFloat(a.oldRate, b.oldRate) && a.loop == b.loop;
}
bool Same(const PortableActorAnimationSnapshot& a,
    const PortableActorAnimationSnapshot& b) {
    if (a.sequence != b.sequence || !SameFloat(a.frame, b.frame) ||
        !SameFloat(a.rate, b.rate) || !SameFloat(a.last, b.last) ||
        !SameFloat(a.minRate, b.minRate) || !SameFloat(a.tweenRate, b.tweenRate) ||
        !SameFloat(a.oldRate, b.oldRate) || a.loop != b.loop ||
        a.notify != b.notify || a.finished != b.finished) return false;
    for (std::size_t i = 0u; i < a.blends.size(); ++i)
        if (!Same(a.blends[i], b.blends[i])) return false;
    return true;
}

struct TemporaryFixtureDirectory {
    std::filesystem::path path;
    TemporaryFixtureDirectory() {
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0u; attempt < 20u; ++attempt) {
            const auto candidate = parent / ("deusex-animation-snapshot-" +
                std::to_string(stamp) + '-' + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) {
                path = std::filesystem::canonical(candidate);
                Require(path.parent_path() == parent &&
                    path.filename().string().rfind("deusex-animation-snapshot-", 0u) == 0u,
                    "Generated animation fixture escaped temporary parent");
                return;
            }
        }
        throw std::runtime_error("Could not create isolated animation fixture directory");
    }
    ~TemporaryFixtureDirectory() {
        std::error_code ignored;
        if (!path.empty()) std::filesystem::remove_all(path, ignored);
    }
};

using Bytes = std::vector<std::uint8_t>;
void Index(Bytes& bytes, const std::int32_t value) {
    std::uint32_t rest = value < 0 ? static_cast<std::uint32_t>(-value) :
        static_cast<std::uint32_t>(value);
    bytes.push_back(static_cast<std::uint8_t>((value < 0 ? 0x80u : 0u) |
        (rest > 63u ? 0x40u : 0u) | (rest & 63u)));
    rest >>= 6u;
    while (rest != 0u) {
        bytes.push_back(static_cast<std::uint8_t>((rest > 127u ? 0x80u : 0u) | (rest & 127u)));
        rest >>= 7u;
    }
}
void Word32(Bytes& bytes, const std::uint32_t value) {
    for (unsigned shift = 0u; shift < 32u; shift += 8u)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
struct FixturePackage {
    PortablePackageTables table;
    Bytes bytes;
    explicit FixturePackage(const std::filesystem::path& path) {
        table.sourcePath = path.string(); table.version = 68u;
        Name("None");
    }
    std::int32_t Name(const std::string& name) {
        for (std::size_t i = 0u; i < table.names.size(); ++i)
            if (table.names[i].Name == name) return static_cast<std::int32_t>(i);
        NameTableEntry entry{}; entry.Name = name;
        table.names.push_back(entry);
        return static_cast<std::int32_t>(table.names.size() - 1u);
    }
    std::int32_t ImportClass(const std::string& package, const std::string& name) {
        ImportTableEntry outer{};
        outer.ClassPackage = Name("Core"); outer.ClassName = Name("Package");
        outer.ObjName = Name(package);
        table.imports.push_back(outer);
        const auto outerReference = -static_cast<std::int32_t>(table.imports.size());
        ImportTableEntry cls{};
        cls.ClassPackage = Name("Core"); cls.ClassName = Name("Class");
        cls.ObjName = Name(name); cls.ObjOuter = outerReference;
        table.imports.push_back(cls);
        return -static_cast<std::int32_t>(table.imports.size());
    }
    void Float(Bytes& properties, const char* name, const float value,
        const std::uint8_t slot = 0u) {
        Index(properties, Name(name)); properties.push_back(slot ? 0xa4u : 0x24u);
        if (slot) properties.push_back(slot);
        std::uint32_t bits{}; std::memcpy(&bits, &value, sizeof(bits));
        Word32(properties, bits);
    }
    void NameValue(Bytes& properties, const char* name, const std::string& value,
        const std::uint8_t slot = 0u) {
        Bytes encoded; Index(encoded, Name(value));
        Require(encoded.size() <= 255u, "Synthetic name exceeds compact fixture bound");
        Index(properties, Name(name)); properties.push_back(slot ? 0xd6u : 0x56u);
        properties.push_back(static_cast<std::uint8_t>(encoded.size()));
        if (slot) properties.push_back(slot);
        properties.insert(properties.end(), encoded.begin(), encoded.end());
    }
    void Bool(Bytes& properties, const char* name, const bool value) {
        Index(properties, Name(name)); properties.push_back(value ? 0x83u : 3u);
    }
    void Byte(Bytes& properties, const char* name, const std::uint8_t value) {
        Index(properties, Name(name)); properties.push_back(1u); properties.push_back(value);
    }
    void Object(Bytes& properties, const char* name, const std::int32_t value) {
        Bytes encoded; Index(encoded, value);
        Index(properties, Name(name)); properties.push_back(0x55u);
        properties.push_back(static_cast<std::uint8_t>(encoded.size()));
        properties.insert(properties.end(), encoded.begin(), encoded.end());
    }
    std::int32_t Export(const std::string& name, const std::int32_t cls,
        const std::int32_t base, const Bytes& payload, const std::int32_t outer = 0) {
        ExportTableEntry entry{};
        entry.ObjName = Name(name); entry.ObjClass = cls; entry.ObjBase = base;
        entry.ObjOuter = outer; entry.ObjOffset = static_cast<std::int32_t>(bytes.size());
        entry.ObjSize = static_cast<std::int32_t>(payload.size());
        table.exports.push_back(entry); bytes.insert(bytes.end(), payload.begin(), payload.end());
        return static_cast<std::int32_t>(table.exports.size());
    }
    std::int32_t Class(const std::string& name, const std::int32_t base, Bytes properties) {
        Bytes payload;
        for (unsigned i = 0u; i < 4u; ++i) Index(payload, 0);
        Index(payload, Name(name)); payload.resize(payload.size() + 8u, 0u);
        Word32(payload, 0u); // No state bytecode.
        payload.resize(payload.size() + 8u + 8u + 2u + 4u + 4u + 16u, 0u);
        Index(payload, 0); Index(payload, 0); Index(payload, 0); Index(payload, Name("None"));
        properties.push_back(0u); payload.insert(payload.end(), properties.begin(), properties.end());
        return Export(name, 0, base, payload);
    }
    void ConstantFunction(const std::int32_t outer) {
        const auto cls = ImportClass("Core", "Function");
        Bytes payload{0u};
        for (unsigned i = 0u; i < 4u; ++i) Index(payload, 0);
        Index(payload, Name("Stub")); payload.resize(payload.size() + 8u, 0u);
        Word32(payload, 2u); payload.push_back(4u); payload.push_back(0x0bu);
        payload.resize(payload.size() + 2u + 1u + 4u, 0u);
        Export("Stub", cls, 0, payload, outer);
    }
    void Actor(const std::string& name, const std::int32_t cls, Bytes properties) {
        properties.push_back(0u); Export(name, cls, 0, properties);
    }
    void Save() const {
        std::ofstream file(table.sourcePath, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        Require(static_cast<bool>(file), "Could not write bounded synthetic package fixture");
    }
};

void TestSerializedFixture() {
    TemporaryFixtureDirectory directory;
    FixturePackage engine(directory.path / "Engine.u");
    Bytes defaults; engine.Byte(defaults, "Fatness", 128u);
    const auto actorClass = engine.Class("Actor", 0, defaults);
    engine.ConstantFunction(actorClass); engine.Save();

    FixturePackage parent(directory.path / "AnimationParent.u");
    const auto engineActor = parent.ImportClass("Engine", "Actor");
    // A qualified late-loaded map reference exercises Owner=None suppressing
    // inherited non-null references, without mutating any actual game data.
    const auto defaultOwner = parent.ImportClass("AnimationFixture", "GrandOwnerActor");
    defaults.clear();
    parent.NameValue(defaults, "AnimSequence", "ParentSequence");
    parent.Float(defaults, "AnimFrame", 0.25f); parent.Float(defaults, "AnimRate", -0.5f);
    parent.Float(defaults, "AnimLast", 0.95f); parent.Float(defaults, "AnimMinRate", 0.2f);
    parent.Float(defaults, "TweenRate", 3.0f); parent.Float(defaults, "OldAnimRate", -0.25f);
    parent.Object(defaults, "Owner", defaultOwner);
    parent.Bool(defaults, "bAnimLoop", true); parent.Bool(defaults, "bAnimNotify", true);
    for (std::uint8_t slot = 0u; slot < 3u; ++slot) {
        parent.NameValue(defaults, "BlendAnimSequence", "ParentBlend" + std::to_string(slot), slot);
        parent.Float(defaults, "BlendAnimFrame", 0.1f * static_cast<float>(slot + 1u), slot);
    }
    parent.Byte(defaults, "Fatness", 120u);
    parent.Class("ParentActor", engineActor, defaults); parent.Save();

    FixturePackage child(directory.path / "AnimationChild.u");
    // Same sequence names intentionally have different indices in each package.
    child.Name("UnrelatedChildName"); child.Name("AnotherChildName");
    const auto parentClass = child.ImportClass("AnimationParent", "ParentActor");
    defaults.clear(); child.NameValue(defaults, "AnimSequence", "ChildSequence");
    child.Float(defaults, "AnimFrame", 0.5f); child.Byte(defaults, "Fatness", 144u);
    child.NameValue(defaults, "BlendAnimSequence", "ChildBlend1", 1u);
    child.Float(defaults, "BlendAnimFrame", 0.8f, 1u);
    child.NameValue(defaults, "BlendAnimSequence", "None", 2u);
    child.Class("ChildActor", parentClass, defaults); child.Save();

    FixturePackage map(directory.path / "AnimationFixture.dx");
    map.Name("YetAnotherMapName");
    const auto childClass = map.ImportClass("AnimationChild", "ChildActor");
    Bytes properties;
    map.NameValue(properties, "AnimSequence", "OwnerSequence");
    map.Float(properties, "AnimFrame", 0.9f); map.Byte(properties, "Fatness", 200u);
    map.Bool(properties, "bAnimByOwner", true); map.Object(properties, "Owner", 3);
    map.Actor("OwnerActor", childClass, properties);
    properties.clear(); map.Bool(properties, "bAnimByOwner", true);
    map.Object(properties, "Owner", 1); map.Byte(properties, "Fatness", 168u);
    map.Actor("RenderedActor", childClass, properties);
    properties.clear(); map.NameValue(properties, "AnimSequence", "GrandOwnerSequence");
    map.Float(properties, "AnimFrame", 0.6f);
    map.Actor("GrandOwnerActor", childClass, properties);
    properties.clear(); map.Float(properties, "AnimFrame", 0.75f);
    map.NameValue(properties, "BlendAnimSequence", "InstanceBlend0");
    map.Float(properties, "BlendAnimFrame", 0.15f);
    map.Bool(properties, "bAnimLoop", false); map.Bool(properties, "bAnimFinished", true);
    map.Actor("InheritedActor", childClass, properties);
    properties.clear(); map.NameValue(properties, "AnimSequence", "None");
    map.Float(properties, "AnimFrame", -0.25f); map.Bool(properties, "bAnimByOwner", true);
    map.Object(properties, "Owner", 0); map.Actor("ExplicitNoneActor", childClass, properties);
    properties.clear(); map.Actor("DefaultActor", childClass, properties); map.Save();

    struct Shutdown { ~Shutdown() { ShutdownPortableRuntime(); } } shutdown;
    Require(InitializePortableRuntime(std::vector<PortablePackageTables>{
        engine.table, parent.table, child.table}).passed, "Synthetic multi-package runtime failed");
    Require(LoadPortableRuntimeMap(map.table).passed, "Synthetic map runtime failed");
    const auto actors = GetPortableRuntimeMapActors();
    Require(actors.size() == 6u, "Synthetic snapshot fixture actor count changed");
    const auto& inherited = actors[3u];
    Require(inherited.animation.sequence == "ChildSequence" && inherited.animation.frame == 0.75f &&
        inherited.animation.rate == -0.5f && inherited.animation.last == 0.95f &&
        inherited.animation.minRate == 0.2f && inherited.animation.tweenRate == 3.0f &&
        inherited.animation.oldRate == -0.25f && !inherited.animation.loop &&
        inherited.animation.notify && inherited.animation.finished && inherited.fatness == 144u,
        "Instance/nearest-class/parent animation inheritance differs from serialized values");
    Require(inherited.animation.blends[0u].sequence == "InstanceBlend0" &&
        inherited.animation.blends[0u].frame == 0.15f &&
        inherited.animation.blends[1u].sequence == "ChildBlend1" &&
        inherited.animation.blends[1u].frame == 0.8f &&
        inherited.animation.blends[2u].sequence == "None" &&
        inherited.animation.blends[2u].frame == 0.3f &&
        inherited.animation.blends[3u].sequence.empty(),
        "Per-index blend inheritance, explicit None or package-local name decoding is wrong");
    const auto& rendered = actors[1u];
    Require(rendered.animByOwner && rendered.ownerPath == "AnimationFixture.OwnerActor" &&
        rendered.animationSourcePath == rendered.ownerPath &&
        rendered.animation.sequence == "OwnerSequence" && rendered.animation.frame == 0.9f &&
        rendered.fatness == 168u && actors[0u].animation.sequence == "GrandOwnerSequence",
        "Owner override recursed or replaced rendered actor Fatness");
    Require(actors[4u].ownerPath.empty() && actors[4u].animationSourcePath == actors[4u].objectPath &&
        actors[4u].animation.sequence == "None" && actors[4u].animation.frame == -0.25f,
        "Explicit None or negative authored tween frame was replaced with a fake animation");
    Require(actors[5u].animation.sequence == "ChildSequence" && actors[5u].animation.frame == 0.5f &&
        actors[5u].animation.blends[0u].sequence == "ParentBlend0" && actors[5u].fatness == 144u,
        "Default snapshot decoded inherited names against the map's name table");
    for (const auto& actor : actors)
        for (const auto& blend : actor.animation.blends)
            Require(!blend.loop, "A nonexistent serialized blend-loop flag was manufactured");
    std::cout << "Serialized snapshot fixture passed: three name tables, instance/class/parent array inheritance, "
        "explicit None, immediate-only owner animation, rendered Fatness, negative tween diagnostic state.\n";
}

std::string QualifiedReference(const PortablePackageTables& package, const std::int32_t reference) {
    if (reference == 0) return {};
    auto path = GetPortableObjectPath(package, reference);
    return reference > 0 && !path.empty() ?
        std::filesystem::path(package.sourcePath).stem().string() + '.' + path : path;
}
struct OriginalPropertySource {
    const PortablePackageTables* package{};
    std::vector<PortableTaggedProperty> properties;
};
using OriginalSources = std::vector<OriginalPropertySource>;
struct OriginalProperty {
    const PortablePackageTables* package{};
    const PortableTaggedProperty* property{};
};
OriginalProperty Lookup(const OriginalSources& sources, const char* name,
    const std::uint32_t slot = 0u) {
    for (const auto& source : sources) {
        for (auto property = source.properties.rbegin(); property != source.properties.rend(); ++property)
            if (property->name == name && property->arrayIndex == slot)
                return {source.package, &*property};
    }
    return {};
}
float Float(const OriginalSources& sources, const char* name, const std::uint32_t slot = 0u) {
    const auto found = Lookup(sources, name, slot);
    float result{};
    if (found.property && found.property->type == 4u && found.property->value.size() == 4u)
        std::memcpy(&result, found.property->value.data(), sizeof(result));
    return result;
}
bool Bool(const OriginalSources& sources, const char* name) {
    const auto found = Lookup(sources, name);
    return found.property && found.property->type == 3u && found.property->boolValue;
}
std::string Name(const OriginalSources& sources, const char* name, const std::uint32_t slot = 0u) {
    const auto found = Lookup(sources, name, slot);
    return found.property && found.property->type == 6u ?
        DecodePortableNameProperty(*found.package, *found.property) : std::string();
}
PortableActorAnimationSnapshot OriginalAnimation(const OriginalSources& sources) {
    PortableActorAnimationSnapshot expected;
    expected.sequence = Name(sources, "AnimSequence"); expected.frame = Float(sources, "AnimFrame");
    expected.rate = Float(sources, "AnimRate"); expected.last = Float(sources, "AnimLast");
    expected.minRate = Float(sources, "AnimMinRate"); expected.tweenRate = Float(sources, "TweenRate");
    expected.oldRate = Float(sources, "OldAnimRate"); expected.loop = Bool(sources, "bAnimLoop");
    expected.notify = Bool(sources, "bAnimNotify"); expected.finished = Bool(sources, "bAnimFinished");
    for (std::uint32_t i = 0u; i < expected.blends.size(); ++i) {
        auto& blend = expected.blends[i];
        blend.sequence = Name(sources, "BlendAnimSequence", i);
        blend.frame = Float(sources, "BlendAnimFrame", i); blend.rate = Float(sources, "BlendAnimRate", i);
        blend.last = Float(sources, "BlendAnimLast", i); blend.minRate = Float(sources, "BlendAnimMinRate", i);
        blend.tweenRate = Float(sources, "BlendTweenRate", i); blend.oldRate = Float(sources, "OldBlendAnimRate", i);
    }
    return expected;
}
OriginalSources ReadOriginalSources(const PortablePackageTables& map,
    const std::string& actorPath, const std::map<std::string, PortablePackageTables>& packages,
    std::unordered_map<std::string, PortableClassDescriptor>& classes) {
    const auto dot = actorPath.find('.');
    Require(dot != std::string::npos, "Original actor path is not qualified");
    const auto index = FindPortableExport(map, actorPath.substr(dot + 1u));
    OriginalSources sources{{&map, LoadPortableExportProperties(map, index).properties}};
    std::string classPath = QualifiedReference(map, map.exports[index].ObjClass);
    std::set<std::string> visited;
    while (!classPath.empty()) {
        Require(visited.size() < 128u && visited.insert(classPath).second,
            "Original snapshot class ancestry exceeds bound or cycles");
        const auto separator = classPath.find('.');
        Require(separator != std::string::npos, "Original class path is unqualified");
        const auto found = packages.find(classPath.substr(0u, separator));
        if (found == packages.end()) break;
        const auto cls = FindPortableExport(found->second, classPath.substr(separator + 1u));
        auto descriptor = classes.find(classPath);
        if (descriptor == classes.end()) descriptor = classes.emplace(classPath,
            LoadPortableClassDescriptor(found->second, cls)).first;
        sources.push_back({&found->second, descriptor->second.defaults});
        classPath = QualifiedReference(found->second, found->second.exports[cls].ObjBase);
    }
    return sources;
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
    std::vector<PortablePackageTables> runtimePackages;
    std::map<std::string, PortablePackageTables> packages;
    for (const auto* name : names) {
        auto package = LoadPortablePackageTables((gameRoot / "System" / (std::string(name) + ".u")).string());
        packages.emplace(name, package); runtimePackages.push_back(std::move(package));
    }
    struct Shutdown { ~Shutdown() { ShutdownPortableRuntime(); } } shutdown;
    Require(InitializePortableRuntime(runtimePackages).passed, "Original runtime initialization failed");
    std::unordered_map<std::string, PortableClassDescriptor> classes;
    const std::vector<std::string> defaults{"00_Training", "01_NYC_UNATCOIsland"};
    const auto& maps = suppliedMaps.empty() ? defaults : suppliedMaps;
    std::size_t checked{}, inheritedNames{}, ownerOverrides{}, authoredPoses{}, decorationPoses{}, tweenStates{};
    for (const auto& mapName : maps) {
        const auto map = LoadPortablePackageTables((gameRoot / "Maps" / (mapName + ".dx")).string());
        Require(LoadPortableRuntimeMap(map).passed, "Original map runtime load failed");
        const auto actors = GetPortableRuntimeMapActors();
        std::unordered_map<std::string, OriginalSources> sources;
        for (const auto& actor : actors)
            sources.emplace(actor.objectPath, ReadOriginalSources(map, actor.objectPath, packages, classes));
        for (const auto& actor : actors) {
            const auto& rendered = sources.at(actor.objectPath);
            const auto ownerTag = Lookup(rendered, "Owner");
            const auto ownerPath = ownerTag.property && ownerTag.property->type == 5u ?
                QualifiedReference(*ownerTag.package, DecodePortableObjectReference(*ownerTag.property)) : std::string();
            const auto owner = sources.find(ownerPath);
            const bool byOwner = Bool(rendered, "bAnimByOwner");
            const auto& selected = byOwner && owner != sources.end() ? owner->second : rendered;
            const auto sourcePath = byOwner && owner != sources.end() ? ownerPath : actor.objectPath;
            const auto expected = OriginalAnimation(selected);
            if (!Same(actor.animation, expected) || actor.animByOwner != byOwner ||
                actor.ownerPath != ownerPath || actor.animationSourcePath != sourcePath)
                throw std::runtime_error("Original animation inheritance/source mismatch for " + actor.objectPath);
            const auto fatness = Lookup(rendered, "Fatness");
            const auto expectedFatness = fatness.property && fatness.property->type == 1u &&
                !fatness.property->value.empty() ? fatness.property->value.front() : 128u;
            Require(actor.fatness == expectedFatness, "Rendered original actor Fatness used the owner");
            const auto sequence = Lookup(selected, "AnimSequence");
            if (sequence.property && sequence.package != &map) ++inheritedNames;
            if (sourcePath != actor.objectPath) ++ownerOverrides;
            if (!actor.meshPath.empty() && actor.animation.frame != 0.0f) {
                ++authoredPoses;
                if (actor.decoration) ++decorationPoses;
                if (actor.animation.frame < 0.0f) ++tweenStates;
                if (authoredPoses <= 12u)
                    std::cout << "Authored mesh pose: " << actor.objectPath << " class=" << actor.classPath <<
                        " sequence=" << actor.animation.sequence << " frame=" << actor.animation.frame <<
                        " source=" << actor.animationSourcePath << " fatness=" << unsigned(actor.fatness) << '\n';
            }
            ++checked;
        }
        std::cout << "Original snapshot map passed: " << mapName << " actors=" << actors.size() << '\n';
    }
    Require(checked != 0u, "Original snapshot audit checked no actors");
    std::cout << "Original authored snapshot evidence: actors=" << checked << " inheritedNameSources=" <<
        inheritedNames << " immediateOwnerOverrides=" << ownerOverrides << " nonzeroMeshFrames=" <<
        authoredPoses << " nonzeroDecorationFrames=" << decorationPoses << " negativeTweenStates=" << tweenStates <<
        "; no guessed startup animation, VM ticks, events or animation save claims.\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        TestSerializedFixture();
        if (argc > 1) {
            std::vector<std::string> maps;
            for (int i = 2; i < argc; ++i) maps.emplace_back(argv[i]);
            TestOriginal(std::filesystem::canonical(argv[1]), maps);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Actor animation snapshot test failed: " << error.what() << '\n';
        return 1;
    }
}
