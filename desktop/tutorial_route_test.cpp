#include "Precomp.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
std::size_t checks{};
void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}
struct OwnedTemporary {
    std::filesystem::path parent, directory;
    explicit OwnedTemporary(const std::filesystem::path& gameRoot) {
        parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto original = std::filesystem::canonical(gameRoot);
        Require(parent.string().rfind(original.string(), 0u) != 0u,
            "Test checkpoint parent overlaps the commercial installation");
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned i = 0u; i < 20u; ++i) {
            const auto candidate = parent / ("deusex-tutorial-route-" + std::to_string(stamp) + '-' + std::to_string(i));
            if (std::filesystem::create_directory(candidate)) {
                directory = std::filesystem::canonical(candidate); break;
            }
        }
        Require(!directory.empty() && directory.parent_path() == parent,
            "Test could not create an isolated checkpoint directory");
    }
    ~OwnedTemporary() {
        ShutdownPortableRuntime();
        std::error_code error;
        const auto actual = std::filesystem::weakly_canonical(directory, error);
        if (!error && !directory.empty() && actual == directory && actual.parent_path() == parent &&
            actual.filename().string().rfind("deusex-tutorial-route-", 0u) == 0u)
            std::filesystem::remove_all(actual, error);
    }
    std::string Path(const std::string& name) const { return (directory / (name + ".sav")).string(); }
};
PortableActorSnapshot Mover(const std::string& path) {
    const auto actors = GetPortableRuntimeMoverActors();
    const auto found = std::find_if(actors.begin(), actors.end(), [&](const auto& actor) { return actor.objectPath == path; });
    Require(found != actors.end(), "Original mover missing from the runtime: " + path);
    Require(found->mover && found->hasLocation && !found->brushPath.empty(),
        "Original mover lost its authored brush/location: " + path);
    return *found;
}
auto Fields(const PortableActorSnapshot& actor) {
    return std::tie(actor.objectPath, actor.classPath, actor.x, actor.y, actor.z,
        actor.pitch, actor.yaw, actor.roll, actor.hidden, actor.activated,
        actor.collideActors, actor.blockPlayers, actor.moverMoving, actor.moverClosing, actor.brushPath);
}
void SameMover(const PortableActorSnapshot& expected) {
    Require(Fields(Mover(expected.objectPath)) == Fields(expected),
        "Mover checkpoint/reversal changed pose, visibility, collision or movement direction: " + expected.objectPath);
}
void Near(float actual, float expected, const std::string& message) {
    Require(std::isfinite(actual) && std::fabs(actual - expected) <= 0.002f,
        message + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}
void Advance(unsigned count) {
    for (unsigned i = 0u; i < count; ++i) AdvancePortableRuntimeMovers(0.05f);
}
void PairPose(const PortableActorSnapshot& first, const PortableActorSnapshot& second,
    float offset, bool moving) {
    const auto a = Mover(first.objectPath), b = Mover(second.objectPath);
    Near(a.x, first.x, "First panel X drifted"); Near(a.z, first.z, "First panel Z drifted");
    Near(b.x, second.x, "Second panel X drifted"); Near(b.z, second.z, "Second panel Z drifted");
    Near(a.y, first.y - offset, "First panel did not follow its authored negative-Y KeyPos");
    Near(b.y, second.y + offset, "Second panel did not follow its authored positive-Y KeyPos");
    Require(!a.hidden && !b.hidden && a.collideActors && b.collideActors && a.blockPlayers && b.blockPlayers,
        "Opening a door hid its brush or disabled actual brush collision");
    Require(a.moverMoving == moving && b.moverMoving == moving,
        "Paired door panels disagreed on moving/stopped status");
}
std::vector<std::uint8_t> Snapshot(const OwnedTemporary& temporary, const std::string& name) {
    const auto path = temporary.Path(name);
    Require(SavePortableRuntimeState(path), "Could not save isolated tutorial route checkpoint");
    std::vector<std::uint8_t> bytes;
    Require(QuestVr::ReadBoundedSaveFile(path, QuestVr::kMaximumSaveRuntimeBytes, bytes),
        "Could not inspect isolated tutorial route checkpoint");
    return bytes;
}
struct MoverWireRecord {
    std::size_t nameOffset{}, nameSize{}, stateOffset{};
};
std::vector<MoverWireRecord> InspectGeneratedMoverRecords(const std::vector<std::uint8_t>& bytes) {
    std::size_t cursor{};
    const auto skip = [&](const std::size_t count) {
        Require(cursor <= bytes.size() && count <= bytes.size() - cursor,
            "Independent generated v14 parser exceeded its payload");
        cursor += count;
    };
    const auto word = [&]() {
        const auto start = cursor; skip(4u);
        std::uint32_t result{};
        for (unsigned i = 0u; i < 4u; ++i) result |= std::uint32_t(bytes[start + i]) << (8u * i);
        return result;
    };
    const auto string = [&]() {
        const auto size = word();
        Require(size <= 1'048'576u, "Independent generated v14 string exceeded its bound");
        const auto start = cursor; skip(size);
        return std::make_pair(start, static_cast<std::size_t>(size));
    };
    const auto strings = [&]() {
        const auto count = word();
        Require(count <= 100'000u && count <= (bytes.size() - cursor) / 4u,
            "Independent generated v14 string-list count exceeded its payload");
        for (std::uint32_t i = 0u; i < count; ++i) string();
    };
    Require(word() == 0x53515844u && word() == 14u && word() == 3u,
        "Malformed-motion controls require the exact generated v14 wrapper around the unchanged v3 prefix");
    strings(); strings(); strings(); skip(4u);
    const auto damaged = word();
    Require(damaged <= 100'000u && damaged <= (bytes.size() - cursor) / 8u,
        "Independent generated v14 damaged-list count exceeded its payload");
    for (std::uint32_t i = 0u; i < damaged; ++i) { string(); skip(4u); }
    skip(8u); strings(); strings(); strings(); strings();
    const auto count = word();
    Require(count == 2u, "Malformed-motion controls need both actual first-door records");
    std::vector<MoverWireRecord> records;
    for (std::uint32_t i = 0u; i < count; ++i) {
        const auto [offset, size] = string();
        records.push_back({offset, size, cursor});
        skip(68u); // Two exact poses, keys, phase, elapsed and remaining stay.
    }
    Require(cursor == bytes.size(), "Independent generated v14 parser did not consume its exact trailer");
    return records;
}
void WriteWord(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint32_t value) {
    Require(offset <= bytes.size() && bytes.size() - offset >= 4u,
        "Malformed-motion control word exceeded its generated payload");
    for (unsigned i = 0u; i < 4u; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
}
void RejectGeneratedCheckpoint(const OwnedTemporary& temporary, const std::string& name,
    const std::vector<std::uint8_t>& bytes, const std::vector<std::uint8_t>& expected) {
    const auto before = GetPortableRuntimeMoverActors();
    const auto revision = GetPortableRuntimeWorldRevision();
    const auto path = temporary.Path(name);
    Require(QuestVr::WriteDurableSaveFile(path, bytes), "Could not stage generated malformed-motion control");
    Require(!ValidatePortableRuntimeState(path, "00_Training") && !LoadPortableRuntimeState(path),
        "Malformed-motion checkpoint was accepted: " + name);
    const auto after = GetPortableRuntimeMoverActors();
    Require(after.size() == before.size() && GetPortableRuntimeWorldRevision() == revision &&
        Snapshot(temporary, name + "-after") == expected,
        "Malformed-motion refusal changed world revision or complete saved state: " + name);
    for (std::size_t i = 0u; i < before.size(); ++i)
        Require(Fields(after[i]) == Fields(before[i]),
            "Malformed-motion refusal changed a live mover snapshot: " + name);
}
void Original(const std::filesystem::path& gameRoot) {
    OwnedTemporary temporary(gameRoot);
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
    for (const auto* name : packages)
        tables.push_back(LoadPortablePackageTables((gameRoot / "System" / (std::string(name) + ".u")).string()));
    Require(InitializePortableRuntime(tables).passed, "Owned package runtime initialization failed");
    Require(LoadPortableRuntimeMap(LoadPortablePackageTables((gameRoot / "Maps" / "00_Training.dx").string())).passed,
        "Owned Training map did not load");
    const auto freshLegacy = Snapshot(temporary, "fresh-legacy");
    Require(freshLegacy.size() >= 8u && freshLegacy[4u] == 3u && freshLegacy[5u] == 0u &&
        freshLegacy[6u] == 0u && freshLegacy[7u] == 0u,
        "Untouched authored Training no longer produces the legacy v3 checkpoint fixture");
    const auto checkDataLink = [&](const char* name, const char* event, const std::int32_t sound,
                                  const char* fragment) {
        const auto line = GetPortableRuntimeDataLink(name, 0);
        Require(line.found && line.eventPath == event && line.soundId == sound &&
            line.audioPackageName == "Mission00" && line.matchingLines == 1u &&
            line.speech.find(fragment) != std::string::npos && line.effects.empty() && line.choices.empty(),
            "Named Training datalink resolved the wrong authored event: " + std::string(name));
        const auto audio = LoadPortableRuntimeDialogueSound(line);
        Require(!audio.data.empty(), "Named Training datalink did not resolve real owned voice audio");
        return line;
    };
    const auto startLine = checkDataLink("DL_Start", "DeusExConText.ConEventSpeech79", 79, "Academy");
    checkDataLink("DL_door", "DeusExConText.ConEventSpeech41", 41, "Open the door");
    checkDataLink("DL_key", "DeusExConText.ConEventSpeech52", 52, "key on the desk");
    const auto lowerStart = GetPortableRuntimeDataLink("dl_sTaRt", 0);
    Require(lowerStart.found && lowerStart.eventPath == startLine.eventPath &&
        lowerStart.speech == startLine.speech && lowerStart.soundId == startLine.soundId,
        "DataLink conName matching did not preserve original case-insensitive comparison");
    Require(!GetPortableRuntimeDataLink("DL_Start", 99).found &&
        !GetPortableRuntimeDataLink("NotAnAuthoredTrainingDataLink", 0).found,
        "DataLink lookup guessed another mission or an unknown conversation");
    // Actual Conversation9 has speech31 -> Trigger(combatentry) -> End.
    // It must not be flattened into a speech-only substitute that drops the gate.
    Require(!GetPortableRuntimeDataLink("DL_CombatEntry", 0).found,
        "Complex original datalink was silently flattened and lost its door trigger");
    Require(!GetPortableRuntimeDataLink("DL_StealthAdvice2", 0).found &&
        !GetPortableRuntimeDataLink("DL_lockstr", 0).found,
        "Single-speech datalink bypassed its original conversation flagRefList");
    Require(Snapshot(temporary, "after-readonly-datalinks") == freshLegacy,
        "Read-only datalink/audio inspection consumed a trigger or changed the map");
    PortableDialogueResult touched;
    Require(GetPortableRuntimeTouchDataLink(-811.617f, 832.431f, -75.0674f, 17.0f, 39.0f, touched) &&
        touched.eventPath == startLine.eventPath && touched.soundId == 79 &&
        touched.actorPath == "00_Training.DataLinkTrigger23",
        "Actual Training start contact did not select its exact instructor line");
    Require(!GetPortableRuntimeTouchDataLink(-811.617f, 832.431f, -75.0674f, 17.0f, 39.0f, touched),
        "One-shot Training start contact replayed its voice line");
    Snapshot(temporary, "played-start");
    Require(LoadPortableRuntimeState(temporary.Path("fresh-legacy")),
        "Fresh legacy checkpoint could not reset tutorial contact state");
    Require(GetPortableRuntimeTouchDataLink(-811.617f, 832.431f, -75.0674f, 17.0f, 39.0f, touched) &&
        touched.eventPath == startLine.eventPath,
        "Restoring untouched legacy data failed to re-enable the start instructor");
    Require(LoadPortableRuntimeState(temporary.Path("played-start")) &&
        !GetPortableRuntimeTouchDataLink(-811.617f, 832.431f, -75.0674f, 17.0f, 39.0f, touched),
        "Played datalink checkpoint lost its one-shot consumed state");
    Require(!GetPortableRuntimeTouchDataLink(1289.05f, 143.366f, -87.8999f, 17.0f, 39.0f, touched),
        "Flag-gated original lock-strength contact bypassed its unimplemented flag timer");
    Require(LoadPortableRuntimeState(temporary.Path("fresh-legacy")),
        "Fresh tutorial route could not restore before door tests");
    // This is explicit VR interaction-layer coverage over the actual authored
    // data. It does not seed begun-play flags or claim original world startup.
    const auto first = Mover("00_Training.DeusExMover16");
    const auto second = Mover("00_Training.DeusExMover17");
    Near(first.x, -220.0f, "Wrong original first-door fixture");
    Near(first.y, 800.0f, "Wrong original first-panel location");
    Near(second.y, 863.999f, "Wrong original second-panel location");
    Require(!first.moverMoving && !second.moverMoving && !first.activated && !second.activated,
        "Training first door did not start closed and stationary");
    const auto opened = InteractPortableRuntimeActor(first.objectPath);
    Require(opened.handled && opened.worldChanged && opened.action == "mover_open",
        "Original unlocked first door was not usable");
    PairPose(first, second, 0.0f, true);
    Advance(20u);
    PairPose(first, second, 28.0f, true);
    const auto midA = Mover(first.objectPath), midB = Mover(second.objectPath);
    const auto midBytes = Snapshot(temporary, "mid-motion");
    Require(ValidatePortableRuntimeState(temporary.Path("mid-motion"), "00_Training"),
        "Schema rejected a valid same-map mid-motion checkpoint");
    const auto beforeWrongMap = GetPortableRuntimeWorldRevision();
    Require(!ValidatePortableRuntimeState(temporary.Path("mid-motion"), "00_TrainingCombat") &&
        GetPortableRuntimeWorldRevision() == beforeWrongMap &&
        Snapshot(temporary, "wrong-map-after") == midBytes,
        "v14 mover-only preflight accepted the wrong metadata map or changed live state");
    SameMover(midA); SameMover(midB);
    const auto wire = InspectGeneratedMoverRecords(midBytes);
    auto malformed = midBytes;
    WriteWord(malformed, wire[0u].stateOffset + 56u, 3u);
    RejectGeneratedCheckpoint(temporary, "invalid-motion-phase", malformed, midBytes);
    malformed = midBytes;
    WriteWord(malformed, wire[0u].stateOffset + 60u, 0x7fc00000u);
    RejectGeneratedCheckpoint(temporary, "nan-motion-elapsed", malformed, midBytes);
    malformed = midBytes;
    Require(wire[0u].nameSize == wire[1u].nameSize, "Duplicate-owner control needs same-sized original identities");
    std::copy_n(midBytes.begin() + static_cast<std::ptrdiff_t>(wire[0u].nameOffset), wire[0u].nameSize,
        malformed.begin() + static_cast<std::ptrdiff_t>(wire[1u].nameOffset));
    RejectGeneratedCheckpoint(temporary, "duplicate-motion-owner", malformed, midBytes);
    malformed = midBytes;
    malformed.resize(wire[0u].stateOffset + 65u);
    RejectGeneratedCheckpoint(temporary, "truncated-motion-record", malformed, midBytes);
    Advance(21u);
    PairPose(first, second, 56.0f, false);
    const auto endA = Mover(first.objectPath), endB = Mover(second.objectPath);
    Require(LoadPortableRuntimeState(temporary.Path("mid-motion")), "Mid-motion checkpoint could not restore");
    SameMover(midA); SameMover(midB);
    Require(Snapshot(temporary, "mid-roundtrip") == midBytes,
        "Mid-motion checkpoint roundtrip lost exact movement progress or duration");
    Advance(21u);
    SameMover(endA); SameMover(endB);

    Require(InteractPortableRuntimeActor(first.objectPath).action == "mover_close",
        "Opened first door could not start closing");
    Advance(10u);
    const auto reversalA = Mover(first.objectPath), reversalB = Mover(second.objectPath);
    Require(reversalA.moverClosing && reversalB.moverClosing && reversalA.moverMoving && reversalB.moverMoving,
        "First door did not enter continuous closing motion");
    Require(InteractPortableRuntimeActor(second.objectPath).action == "mover_open",
        "Frobbing the other panel did not reverse both panels");
    const auto afterA = Mover(first.objectPath), afterB = Mover(second.objectPath);
    Require(afterA.x == reversalA.x && afterA.y == reversalA.y && afterA.z == reversalA.z &&
        afterB.x == reversalB.x && afterB.y == reversalB.y && afterB.z == reversalB.z &&
        !afterA.moverClosing && !afterB.moverClosing,
        "Direction reversal jumped the paired door's current pose");
    Advance(41u);
    PairPose(first, second, 56.0f, false);

    Require(InteractPortableRuntimeActor(first.objectPath).action == "mover_close",
        "Block test could not start a closing step");
    const auto beforeBlockA = Mover(first.objectPath), beforeBlockB = Mover(second.objectPath);
    Advance(1u);
    Require(BlockPortableRuntimeMover(first.objectPath), "Closing door did not accept an explicit collision block");
    const auto blockedA = Mover(first.objectPath), blockedB = Mover(second.objectPath);
    Require(blockedA.x == beforeBlockA.x && blockedA.y == beforeBlockA.y && blockedA.z == beforeBlockA.z &&
        blockedB.x == beforeBlockB.x && blockedB.y == beforeBlockB.y && blockedB.z == beforeBlockB.z &&
        !blockedA.moverClosing && !blockedB.moverClosing,
        "Blocked closing pair did not undo the most recent step and reopen");
    Advance(41u);

    const auto invalidBefore = Snapshot(temporary, "invalid-before");
    const auto revisionBefore = GetPortableRuntimeWorldRevision();
    for (const auto elapsed : {-0.01f, std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity()}) {
        bool refused = false;
        try { AdvancePortableRuntimeMovers(elapsed); } catch (const std::exception&) { refused = true; }
        Require(refused, "Mover update accepted an invalid elapsed time");
        Require(GetPortableRuntimeWorldRevision() == revisionBefore &&
            Snapshot(temporary, "invalid-after") == invalidBefore,
            "Invalid mover delta mutated movement state or checkpoint data");
    }
    const auto lockedA = Mover("00_Training.DeusExMover47"), lockedB = Mover("00_Training.DeusExMover48");
    const auto refused = InteractPortableRuntimeActor(lockedA.objectPath);
    Require(refused.handled && !refused.worldChanged && refused.action == "mover_locked",
        "Training NanoKey door did not return its authored Train-key refusal: handled=" +
        std::to_string(refused.handled) + ", worldChanged=" + std::to_string(refused.worldChanged) +
        ", action=" + refused.action + ", inventory=" + std::to_string(GetPortableRuntimeInventoryCount()));
    SameMover(lockedA); SameMover(lockedB);
    const auto inventoryBefore = GetPortableRuntimeInventoryCount();
    const auto pickup = InteractPortableRuntimeActor("00_Training.NanoKey0");
    Require(pickup.handled && pickup.worldChanged && pickup.action == "pickup" &&
        GetPortableRuntimeInventoryCount() == inventoryBefore + 1u,
        "Training's actual NanoKey pickup was not retained in inventory");
    const auto keyOpened = InteractPortableRuntimeActor(lockedA.objectPath);
    Require(keyOpened.handled && keyOpened.worldChanged && keyOpened.action == "mover_open",
        "Matching NanoKey did not unlock/open Training's paired key door");
    Require(Mover(lockedA.objectPath).moverMoving && Mover(lockedB.objectPath).moverMoving,
        "NanoKey door unlocked only one panel");
    Advance(41u);
    const auto keyedA = Mover(lockedA.objectPath), keyedB = Mover(lockedB.objectPath);
    Near(keyedA.x, lockedA.x - 56.0f, "Key door first panel has wrong authored endpoint");
    Near(keyedB.x, lockedB.x + 56.0f, "Key door second panel has wrong authored endpoint");
    Require(!keyedA.hidden && !keyedB.hidden && !keyedA.moverMoving && !keyedB.moverMoving,
        "Opened keyed panels vanished or never finished moving");
    Snapshot(temporary, "keyed-door");
    Require(InteractPortableRuntimeActor(lockedA.objectPath).action == "mover_close", "Key door could not close");
    Advance(41u);
    Require(LoadPortableRuntimeState(temporary.Path("keyed-door")), "Key door checkpoint did not restore");
    SameMover(keyedA); SameMover(keyedB);
    Require(GetPortableRuntimeInventoryCount() == inventoryBefore + 1u, "Restoring key door lost its collected key");
    std::cout << "Owned Training recording-route interaction tests passed: " << checks << " checks.\n";
}
} // namespace

int main(int argc, char** argv) {
    std::cout.setf(std::ios::unitbuf);
    if (argc == 1) {
        std::cout << "SKIPPED owned Training route: opt in with --game-root <owned Deus Ex folder>.\n";
        return 77;
    }
    try {
        Require(argc == 3 && std::string(argv[1]) == "--game-root",
            "Usage: tutorial_route_test --game-root <owned Deus Ex folder>");
        Original(argv[2]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Training route test failed: " << error.what() << '\n';
        return 1;
    }
}
