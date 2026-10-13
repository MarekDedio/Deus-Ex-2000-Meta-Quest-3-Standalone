#include "Precomp.h"
#include "portable_unreal_runtime.h"
#include "quest_map_lighting.h"
#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using QuestVr::Vm::Kind;
using QuestVr::Vm::Value;
void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
std::vector<char> Bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    Require(bool(input), "Could not read test file: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
struct RuntimeLifetime {
    ~RuntimeLifetime() { ShutdownPortableRuntime(); }
};
struct TemporaryCheckpoints {
    std::filesystem::path directory, before, after;
    TemporaryCheckpoints() {
        auto parent = std::filesystem::absolute(std::filesystem::temp_directory_path()).lexically_normal();
        if (parent.filename().empty()) parent = parent.parent_path();
        Require(parent != parent.root_path(), "Lamp checkpoint parent cannot be a drive root");
        directory = parent / ("quest-original-lamp-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(directory.parent_path() == parent && directory.filename().string().starts_with("quest-original-lamp-"),
            "Lamp checkpoint directory escaped its dedicated temporary parent");
        Require(std::filesystem::create_directory(directory), "Lamp checkpoint directory already exists");
        before = directory / "before.save";
        after = directory / "after.save";
    }
    ~TemporaryCheckpoints() {
        // Exact files in the newly created owned directory, never a recursive
        // delete or a computed original-game path.
        std::error_code ignored;
        std::filesystem::remove(before, ignored);
        std::filesystem::remove(after, ignored);
        std::filesystem::remove(directory, ignored);
    }
};
PortableActorSnapshot Snapshot(const std::string& path) {
    const auto actors = GetPortableRuntimeMapActors();
    const auto found = std::find_if(actors.begin(), actors.end(), [&](const auto& actor) {
        return actor.objectPath == path;
    });
    Require(found != actors.end(), "Original lamp is absent from actor publication");
    return *found;
}
void SameProperties(const std::string& actor,
    const std::vector<std::pair<std::string, Value>>& expected) {
    for (const auto& [name, value] : expected)
        Require(QuestVr::Vm::Equal(value, ReadPortableActorScriptProperty(actor, name)),
            "A refused original Lamp.Frob leaked a property write: " + name);
}
void AssertEmissionProjection(const PortableActorSnapshot& actor) {
    // Detached renderer-input control, not a mutation of the game's lamp.
    // LightType is the original script's off switch, not class-name matching.
    auto on = actor;
    on.light = true;
    on.lightType = 1u;
    const OVR::Vector3f origin{actor.x, actor.y, actor.z};
    const auto lights = QuestVr::BuildMapLights(std::vector{on}, nullptr, &origin);
    Require(lights.size() == (on.lightBrightness == 0u ? 0u : 1u),
        "Authored lamp LightBrightness/LightType projection is inconsistent");
    auto off = on;
    off.lightType = 0u;
    Require(QuestVr::BuildMapLights(std::vector{off}, nullptr, &origin).empty(),
        "LT_None still emits actor direct lighting");
    Require(off.lightBrightness == on.lightBrightness && off.lightRadius == on.lightRadius,
        "Off control changed authored brightness/radius instead of LightType");
}
void Original(const std::filesystem::path& root) {
    const auto source = root / "System" / "DeusEx.u";
    const auto sourceBefore = Bytes(source);
    std::vector<PortablePackageTables> tables;
    for (const auto* name : {"Core", "Engine", "DeusEx", "Extension"})
        tables.push_back(LoadPortablePackageTables((root / "System" /
            (std::string(name) + ".u")).string()));
    for (const auto* name : {"ConSys", "IpDrv", "IpServer", "UBrowser", "UWindow", "Editor", "Fire", "MPCharacters"})
        tables.push_back(LoadPortablePackageTables((root / "System" /
            (std::string(name) + ".u")).string()));
    // Frob's object constants must resolve to the actual authored USounds.
    // This only reads the installed content and does not run Windows code.
    tables.push_back(LoadPortablePackageTables((root / "System" / "DeusExSounds.u").string()));
    const auto& deusex = tables.at(2u);
    const auto frob = LoadPortableFunctionScript(deusex, FindPortableExport(deusex, "Lamp.Frob"));
    Require(frob.nativeIndex == 0u && frob.bytecode.size() == 113u,
        "Original GOTY Lamp.Frob is not the audited scripted implementation");
    const auto& engine = tables.at(1u);
    const auto sound = LoadPortableFunctionScript(engine, FindPortableExport(engine, "Actor.PlaySound"));
    Require(sound.nativeIndex == 264u && sound.functionFlags == 0x401u,
        "Original Engine.Actor.PlaySound declaration differs from the audited native");
    Require(InitializePortableRuntime(tables).passed, "Could not initialize original script runtime");
    RuntimeLifetime runtime;
    const auto map = LoadPortablePackageTables((root / "Maps" / "00_Training.dx").string());
    const auto mapBefore = Bytes(map.sourcePath);
    LoadPortableRuntimeMap(map);
    const auto actors = GetPortableRuntimeMapActors();
    std::vector<PortableActorSnapshot> lamps;
    for (const auto& actor : actors) {
        const auto isLamp = ExecutePortableActorFunction(actor.objectPath, "IsA",
            {{Value::Text(Kind::Name, "Lamp"), {}}});
        Require(isLamp.passed(), "Original actor IsA query failed: " + isLamp.error);
        if (QuestVr::Vm::ToBool(isLamp.value)) lamps.push_back(actor);
    }
    Require(!lamps.empty(), "Original Training map contains no Lamp-derived actor");
    TemporaryCheckpoints checkpoints;
    const auto& before = checkpoints.before;
    const auto& after = checkpoints.after;
    std::size_t completed{}, refused{};
    for (const auto& lamp : lamps) {
        std::vector<std::pair<std::string, Value>> properties;
        for (const auto* name : {"bOn", "LightType", "LightBrightness", "LightRadius", "bUnlit", "ScaleGlow"})
            properties.emplace_back(name, ReadPortableActorScriptProperty(lamp.objectPath, name));
        const bool initiallyOn = QuestVr::Vm::ToBool(properties.at(0u).second);
        const auto revision = GetPortableRuntimeWorldRevision();
        Require(SavePortableRuntimeState(before.string()), "Could not save lamp rollback checkpoint");
        // None is a legitimate explicit Engine.Actor parameter here. This is
        // an isolated original script probe, NOT a replacement for a real VR
        // player or permission to skip DeusExDecoration.Frob's player branch.
        const auto result = ExecutePortableActorFunction(lamp.objectPath, "Frob",
            {{Value::Text(Kind::Object, {}), {}}, {Value::Text(Kind::Object, {}), {}}});
        if (result.passed()) {
            const auto current = Snapshot(lamp.objectPath);
            Require(QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(lamp.objectPath, "bOn")) != initiallyOn,
                "Original Lamp.Frob returned without toggling bOn");
            Require(current.lightType == (initiallyOn ? 0u : 1u) && current.unlit == !initiallyOn,
                "Committed Lamp.Frob is not projected to LightType/bUnlit");
            ++completed;
            Require(LoadPortableRuntimeState(before.string()), "Could not restore committed lamp probe");
        } else {
            Require(result.error.find("Unsupported runtime native 264") != std::string::npos &&
                result.function == "DeusEx.Lamp.Frob" && (result.offset == 43u || result.offset == 89u),
                "Original Lamp.Frob stopped before the audited sound prerequisite: " + result.error +
                " at " + result.function + ':' + std::to_string(result.offset));
            SameProperties(lamp.objectPath, properties);
            Require(GetPortableRuntimeWorldRevision() == revision,
                "Failed original Lamp.Frob published a world revision");
            Require(SavePortableRuntimeState(after.string()) && Bytes(before) == Bytes(after),
                "Original Lamp.Frob failure did not roll back complete serialized runtime state");
            ++refused;
            std::cout << "PREREQUISITE " << lamp.objectPath << ": " << result.error <<
                " at " << result.function << ':' << result.offset << "; exact rollback verified\n";
        }
        AssertEmissionProjection(lamp);
    }
    Require(Bytes(source) == sourceBefore && Bytes(map.sourcePath) == mapBefore,
        "Lamp probes changed original installed content");
    std::cout << "PASS original Training lamp metadata, " << completed << " completed / " << refused <<
        " explicitly refused Frob probes, complete rollback and LightType emission projection; "
        "not a claim of VR player interaction or dynamic BSP lighting\n";
}
} // namespace
int main(int argc, char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        if (argc == 1) {
            std::cout << "SKIP: supply readonly original Deus Ex installation for Training lamp integration\n";
            return 77;
        }
        Require(argc == 2, "Usage: portable_lamp_interaction_test GAME_ROOT");
        Original(argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
