#include "Precomp.h"
#include "portable_unreal_runtime.h"
#include "quest_mesh_animation.h"

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
using QuestVr::Vm::Evaluation;
using QuestVr::Vm::Kind;
using QuestVr::Vm::Status;
using QuestVr::Vm::Value;
void Require(const bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
Evaluation Name(const std::string& value) { return {Value::Text(Kind::Name, value), {}}; }
Evaluation Number(const float value) { return {Value::Float(value), {}}; }
Evaluation Vector(const std::array<float, 3>& value) { return {Value::Vector(value), {}}; }
QuestVr::Vm::Result Call(const std::string& actor, const std::string& function,
    const std::vector<Evaluation>& arguments = {}) {
    auto result = ExecutePortableActorFunction(actor, function, arguments);
    Require(result.passed(), function + " failed: " + result.error +
        " at " + result.function + ':' + std::to_string(result.offset));
    return result;
}
bool IsA(const PortableActorSnapshot& actor, const std::string& type) {
    return QuestVr::Vm::ToBool(Call(actor.objectPath, "IsA", {Name(type)}).value);
}
PortableActorSnapshot Snapshot(const std::string& actor) {
    const auto snapshots = GetPortableRuntimeMapActors();
    const auto found = std::find_if(snapshots.begin(), snapshots.end(), [&](const auto& value) {
        return value.objectPath == actor;
    });
    Require(found != snapshots.end(), "Original actor disappeared during scoped execution");
    return *found;
}
bool Same(const PortableActorSnapshot& a, const PortableActorSnapshot& b) {
    const auto& x = a.animation; const auto& y = b.animation;
    return x.sequence == y.sequence && x.frame == y.frame && x.rate == y.rate &&
        x.last == y.last && x.minRate == y.minRate && x.tweenRate == y.tweenRate &&
        x.oldRate == y.oldRate && x.loop == y.loop && x.notify == y.notify && x.finished == y.finished &&
        x.previous.vertexOffset0 == y.previous.vertexOffset0 && x.previous.vertexOffset1 == y.previous.vertexOffset1 &&
        x.previous.fraction == y.previous.fraction && a.prePivotX == b.prePivotX &&
        a.prePivotY == b.prePivotY && a.prePivotZ == b.prePivotZ;
}
struct TemporaryCheckpoint {
    std::filesystem::path directory;
    explicit TemporaryCheckpoint(const std::filesystem::path& original) {
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        Require(parent.string().rfind(std::filesystem::canonical(original).string(), 0u) != 0u,
            "Generated checkpoint must be outside original installation");
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned i = 0u; i < 20u; ++i) {
            const auto candidate = parent / ("deusex-actor-script-test-" + std::to_string(stamp) + '-' + std::to_string(i));
            if (std::filesystem::create_directory(candidate)) {
                directory = std::filesystem::canonical(candidate);
                Require(directory.parent_path() == parent &&
                    directory.filename().string().rfind("deusex-actor-script-test-", 0u) == 0u,
                    "Generated checkpoint escaped temporary parent");
                return;
            }
        }
        throw std::runtime_error("Could not create isolated script test checkpoint");
    }
    ~TemporaryCheckpoint() {
        std::error_code ignored;
        if (!directory.empty()) std::filesystem::remove_all(directory, ignored);
    }
};

void TestOriginal(const std::filesystem::path& root) {
    static constexpr const char* packages[] = {
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
    std::vector<PortablePackageTables> tables;
    for (const auto* package : packages)
        tables.push_back(LoadPortablePackageTables((root / "System" / (std::string(package) + ".u")).string()));
    struct Shutdown { ~Shutdown() { ShutdownPortableRuntime(); } } shutdown;
    Require(InitializePortableRuntime(tables).passed, "Original script runtime initialization failed");
    TemporaryCheckpoint temporary(root);
    const auto checkpoint = (temporary.directory / "original-authored.sav").string();
    std::size_t humanTests{}, robotTests{}, birdTests{};
    for (const auto* map : {"00_Training", "01_NYC_UNATCOIsland", "00_Intro"}) {
        Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root / "Maps" /
            (std::string(map) + ".dx")).string())).passed, "Original map load failed");
        Require(!GetPortableRuntimeUnsavedScriptState(), "Map replacement retained retired VM state");
        Require(SavePortableRuntimeState(checkpoint), "Untouched authored runtime save failed");
        const auto checkpointBytes = std::filesystem::file_size(checkpoint);
        const auto actors = GetPortableRuntimeMapActors();
        for (const auto& actor : actors) {
            if (humanTests != 0u && robotTests != 0u && birdTests != 0u) break;
            if (!actor.pawn || actor.meshPath.empty() || !IsA(actor, "ScriptedPawn")) continue;
            if (humanTests == 0u && !IsA(actor, "Robot") && !IsA(actor, "Animal")) {
                const auto region = ReadPortableActorScriptProperty(actor.objectPath, "Region");
                Require(region.kind == Kind::Struct && !region.fields.at("zone").text.empty(),
                    "Original BSP Region did not resolve a real ZoneInfo/LevelInfo");
                const auto handed = Call(actor.objectPath, "HasTwoHandedWeapon");
                Require(handed.value.kind == Kind::Bool, "Original lazy weapon query did not return bool");
                Call(actor.objectPath, "PlayWaiting");
                Require(Snapshot(actor.objectPath).animation.sequence != "None",
                    "Original PlayWaiting failed to select the original script sequence");
                Call(actor.objectPath, "DeusEx.ScriptedPawn.Standing.AnimEnd");
                const auto mesh = GetPortableRuntimeMesh(actor.meshPath);
                Require(mesh.animation && !mesh.animation->sequences.empty(), "Original animation mesh unavailable");
                const auto usable = std::find_if(mesh.animation->sequences.begin(), mesh.animation->sequences.end(), [&](const auto& s) {
                    return s.numFrames > 1 && QuestVr::HasUsableMeshAnimationSpan(*mesh.animation, s);
                });
                Require(usable != mesh.animation->sequences.end(), "Original human mesh lacks a multi-frame sequence");
                Call(actor.objectPath, "PlayAnimPivot", {Name(usable->name), Number(0.0f), Number(0.0f), Vector({3.0f, 4.0f, 5.0f})});
                auto pose = Snapshot(actor.objectPath);
                Require(std::abs(pose.animation.rate - usable->rate / static_cast<float>(usable->numFrames)) < 0.00001f &&
                    std::abs(pose.animation.tweenRate - 10.0f / static_cast<float>(usable->numFrames)) < 0.00001f,
                    "Original script optional-zero Rate/TweenTime defaults were not executed");
                Call(actor.objectPath, "PlayAnimPivot", {Name(usable->name), Number(1.0f), Number(-1.0f), Vector({3.0f, 4.0f, 5.0f})});
                pose = Snapshot(actor.objectPath);
                const auto desired = ReadPortableActorScriptProperty(actor.objectPath, "DesiredPrePivot");
                Require(desired.kind == Kind::Vector && pose.prePivotX == desired.vector[0] &&
                    pose.prePivotY == desired.vector[1] && pose.prePivotZ == desired.vector[2],
                    "Original PrePivotTime<=0 assignment did not reach rendered snapshot");
                // Restore positive-time helper defaults before the next checks.
                Call(actor.objectPath, "PlayAnimPivot", {Name(usable->name), Number(0.0f), Number(0.0f), Vector({3.0f, 4.0f, 5.0f})});
                Require(QuestVr::Vm::ToFloat(ReadPortableActorScriptProperty(actor.objectPath, "PrePivotTime")) == 0.1f,
                    "Original PlayAnimPivot property assignment is missing");
                Call(actor.objectPath, "LoopAnimPivot", {Name(usable->name), Number(2.0f), Number(0.2f), Number(0.5f), Vector({0.0f, 0.0f, 0.0f})});
                pose = Snapshot(actor.objectPath);
                Require(pose.animation.loop && std::abs(pose.animation.minRate - 0.5f * usable->rate /
                    static_cast<float>(usable->numFrames)) < 0.00001f, "Original LoopAnimPivot/native MinRate mismatch");
                Call(actor.objectPath, "TweenAnimPivot", {Name(usable->name), Number(0.25f), Vector({1.0f, 2.0f, 3.0f})});
                pose = Snapshot(actor.objectPath);
                Require(pose.animation.rate == 0.0f && pose.animation.frame < 0.0f,
                    "Original TweenAnimPivot did not stage actual native tween state");
                // Force failure after native command + first actor-property write.
                // The complete transaction, including captured tween history, must undo.
                const auto timeBefore = ReadPortableActorScriptProperty(actor.objectPath, "PrePivotTime");
                QuestVr::Vm::Limits limits; limits.writes = 1u;
                const auto failed = ExecutePortableActorFunction(actor.objectPath, "PlayAnimPivot",
                    {Name(usable->name), Number(3.0f), Number(0.5f), Vector({9.0f, 8.0f, 7.0f})}, limits);
                Require(failed.status == Status::Budget && Same(pose, Snapshot(actor.objectPath)) &&
                    QuestVr::Vm::Equal(timeBefore, ReadPortableActorScriptProperty(actor.objectPath, "PrePivotTime")),
                    "Failed original script leaked property/clock/tween changes");
                Require(GetPortableRuntimeUnsavedScriptState() && !SavePortableRuntimeState(checkpoint) &&
                    std::filesystem::file_size(checkpoint) == checkpointBytes,
                    "Unsaved VM state was silently dropped or existing checkpoint truncated");
                Require(ValidatePortableRuntimeState(checkpoint) && GetPortableRuntimeUnsavedScriptState(),
                    "Readonly checkpoint validation changed committed VM state");
                Require(LoadPortableRuntimeState(checkpoint) && !GetPortableRuntimeUnsavedScriptState(),
                    "Valid legacy checkpoint did not restore authored VM state");
                const auto beforeStartup = Snapshot(actor.objectPath);
                const auto startup = ExecutePortableActorFunction(actor.objectPath, "SetInitialState");
                Require(startup.status == Status::Unsupported && Same(beforeStartup, Snapshot(actor.objectPath)) &&
                    !GetPortableRuntimeUnsavedScriptState(), "Unsupported original state startup was faked or leaked changes");
                ++humanTests;
                std::cout << "ORIGINAL HUMAN " << actor.objectPath << " PlayWaiting/Standing.AnimEnd/Play/Loop/Tween/rollback/save passed\n";
            }
            if (robotTests == 0u && IsA(actor, "Robot")) {
                Call(actor.objectPath, "PlayWaiting");
                const auto robot = Snapshot(actor.objectPath);
                Require(!robot.animation.sequence.empty() && robot.animation.frame < 0.0f,
                    "Original Robot.PlayWaiting optional helper parameters were not executed");
                ++robotTests;
                std::cout << "ORIGINAL ROBOT " << actor.objectPath << " PlayWaiting passed\n";
            }
            if (birdTests == 0u && IsA(actor, "Bird")) {
                const auto wait = ReadPortableActorScriptProperty(actor.objectPath, "WaitAnim");
                Call(actor.objectPath, "PlayWaiting");
                const auto bird = Snapshot(actor.objectPath);
                const auto mesh = GetPortableRuntimeMesh(actor.meshPath);
                const auto* expected = QuestVr::FindMeshAnimationSequence(*mesh.animation, wait.text, true);
                Require(expected && bird.animation.sequence == expected->name && bird.animation.loop,
                    "Original Bird.PlayWaiting native optional defaults were not executed");
                const auto before = bird;
                const auto random = ExecutePortableActorFunction(actor.objectPath, "TweenToWaiting", {Number(0.2f)});
                Require(random.status == Status::Unsupported && Same(before, Snapshot(actor.objectPath)),
                    "Unsupported original FRand branch was replaced or leaked mutations: " + random.error);
                ++birdTests;
                std::cout << "ORIGINAL BIRD " << actor.objectPath << " PlayWaiting/native optional/FRand refusal passed\n";
            }
            if (GetPortableRuntimeUnsavedScriptState()) {
                const auto before = Snapshot(actor.objectPath);
                Require(!LoadPortableRuntimeMap(LoadPortablePackageTables((root / "Maps" /
                    (std::string(map) + ".dx")).string())).passed && Same(before, Snapshot(actor.objectPath)),
                    "Map replacement silently discarded committed script state");
                bool refused{};
                try { UnloadPortableRuntimeMap(); } catch (const std::exception&) { refused = true; }
                Require(refused && Same(before, Snapshot(actor.objectPath)),
                    "Map unload silently discarded committed script state");
                Require(LoadPortableRuntimeState(checkpoint), "Could not explicitly restore authored checkpoint");
            }
        }
    }
    Require(humanTests == 1u && robotTests == 1u && birdTests == 1u,
        "Original maps did not cover required human, robot and bird execution fixtures");
    UnloadPortableRuntimeMap();
    Require(!GetPortableRuntimeUnsavedScriptState(), "Map unload leaked actor VM overlays");
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cout << "SKIP: supply readonly original Deus Ex installation for real actor script integration\n";
            return 77;
        }
        TestOriginal(std::filesystem::path(argv[1]));
        std::cout << "PASS original actor bytecode execution, natives, BSP Region and transactional failure\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
