#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Optional read-only GOTY integration. Explicit native calls and an actual
// empty-owned-resource Destroyed branch are not an automatic startup claim.
namespace {
using Bytes = std::vector<std::uint8_t>;
using Value = QuestVr::Vm::Value;
std::size_t checks{};
void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
struct Temporary {
    std::filesystem::path parent, directory;
    explicit Temporary(const std::filesystem::path& original) {
        parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        Require(parent.string().rfind(std::filesystem::canonical(original).string(), 0u) != 0u,
            "Original pawn checkpoint parent overlaps the owned installation");
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0u; attempt < 20u; ++attempt) {
            const auto candidate = parent / ("deusex-original-pawn-test-" + std::to_string(stamp) + '-' + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) { directory = std::filesystem::canonical(candidate); break; }
        }
        Require(!directory.empty() && directory.parent_path() == parent, "Original pawn fixture escaped its temporary parent");
    }
    ~Temporary() {
        ShutdownPortableRuntime();
        std::error_code error;
        const auto actual = std::filesystem::weakly_canonical(directory, error);
        if (!error && !directory.empty() && actual == directory && actual.parent_path() == parent &&
            actual.filename().string().rfind("deusex-original-pawn-test-", 0u) == 0u)
            std::filesystem::remove_all(actual, error);
    }
    std::filesystem::path Path(const std::string& name) const { return directory / (name + ".sav"); }
    Bytes Snapshot(const std::string& name) const {
        const auto path = Path(name);
        Require(SavePortableRuntimeState(path.string()), "Cannot save original PawnList checkpoint");
        Bytes bytes;
        Require(QuestVr::ReadBoundedSaveFile(path.string(), QuestVr::kMaximumSaveRuntimeBytes, bytes),
            "Cannot inspect original PawnList checkpoint");
        return bytes;
    }
};
Value Read(const std::string& actor, const std::string& property) {
    return ReadPortableActorScriptProperty(actor, property);
}
void Link(const Value& value, const std::string& expected, const std::string& message) {
    Require(value.kind == QuestVr::Vm::Kind::Object && value.text == expected, message);
}
void Call(const std::string& actor, const std::string& function) {
    const auto result = ExecutePortableActorFunction(actor, function);
    Require(result.passed(), function + " failed at " + result.function + ':' + std::to_string(result.offset) + ": " + result.error);
}
void Original(const std::filesystem::path& root) {
    Temporary temporary(root);
    std::vector<PortablePackageTables> packages;
    for (const auto* name : {
        "ConSys", "Core", "DeusEx", "DeusExCharacters", "DeusExConAudioAIBarks", "DeusExConAudioEndGame",
        "DeusExConAudioHK_Shared", "DeusExConAudioIntro", "DeusExConAudioMission00", "DeusExConAudioMission01",
        "DeusExConAudioMission02", "DeusExConAudioMission03", "DeusExConAudioMission04", "DeusExConAudioMission05",
        "DeusExConAudioMission08", "DeusExConAudioMission09", "DeusExConAudioMission10", "DeusExConAudioMission11",
        "DeusExConAudioMission12", "DeusExConAudioMission14", "DeusExConAudioMission15", "DeusExConAudioNYShared",
        "DeusExConText", "DeusExConversations", "DeusExDeco", "DeusExItems", "DeusExSounds", "DeusExText",
        "DeusExUI", "Editor", "Engine", "Extension", "Fire", "IpDrv", "IpServer", "MPCharacters", "UBrowser", "UWindow"})
        packages.push_back(LoadPortablePackageTables((root / "System" / (std::string(name) + ".u")).string()));
    Require(InitializePortableRuntime(packages).passed, "Cannot initialize owned original packages");
    const auto map = LoadPortablePackageTables((root / "Maps" / "00_Training.dx").string());
    Require(LoadPortableRuntimeMap(map).passed, "Cannot load unmodified original Training");
    const std::string info = "00_Training.LevelInfo0";
    const std::vector<std::string> pawns{"00_Training.Doctor1", "00_Training.RepairBot0", "00_Training.JaimeReyes0"};
    const auto actors = GetPortableRuntimeMapActors(true);
    for (const auto& pawn : pawns)
        Require(std::any_of(actors.begin(), actors.end(), [&](const auto& actor) { return actor.objectPath == pawn && actor.pawn; }),
            "Original Training lacks expected actual Pawn fixture: " + pawn);
    Link(Read(info, "PawnList"), "", "Untouched original Training unexpectedly has an initialized PawnList");
    Require(!QuestVr::Vm::ToBool(Read(info, "bBegunPlay")), "Readonly original loading falsely began world startup");
    const auto baseline = temporary.Snapshot("Baseline");
    std::vector<PortableActorDispatchContext> contexts;
    for (const auto& pawn : pawns) {
        contexts.push_back(ReadPortableActorDispatchContext(pawn));
        Require(!ReadPortableActorStateObject(pawn), "Original Pawn already has fabricated portable execution storage");
        Call(pawn, "Engine.Pawn.AddPawn");
    }
    Link(Read(info, "PawnList"), pawns[2], "Original AddPawn did not use actual call-order prepend");
    Link(Read(pawns[2], "nextPawn"), pawns[1], "Original nextPawn lost actual robot identity");
    Link(Read(pawns[1], "nextPawn"), pawns[0], "Original robot nextPawn lost actual human identity");
    Link(Read(pawns[0], "nextPawn"), "", "Original first Pawn nextPawn was not the actual empty head");
    const auto saved = temporary.Snapshot("OriginalList");
    Call(pawns[1], "Engine.Pawn.RemovePawn");
    Link(Read(pawns[2], "nextPawn"), pawns[0], "Original middle removal did not splice its predecessor");
    Link(Read(pawns[1], "nextPawn"), pawns[0], "Original middle removal erased retained nextPawn");
    Require(LoadPortableRuntimeState(temporary.Path("OriginalList").string()) && temporary.Snapshot("Warm") == saved,
        "Original PawnList warm restore is not byte-exact");
    Require(LoadPortableRuntimeState(temporary.Path("Baseline").string()) && temporary.Snapshot("Reset") == baseline,
        "Original PawnList reset did not restore the untouched map");
    UnloadPortableRuntimeMap();
    Require(LoadPortableRuntimeMap(map).passed, "Cannot cold-reload the unmodified original map");
    Require(LoadPortableRuntimeState(temporary.Path("OriginalList").string()) && temporary.Snapshot("Cold") == saved,
        "Original PawnList cold replacement after map teardown is not byte-exact");
    GC::Collect(); Require(temporary.Snapshot("AfterGc") == saved, "GC changed the restored original Pawn list");
    for (std::size_t index = 0u; index < pawns.size(); ++index) {
        const auto actual = ReadPortableActorDispatchContext(pawns[index]); const auto& expected = contexts[index];
        Require(!ReadPortableActorStateObject(pawns[index]) && actual.selectedPath == expected.selectedPath &&
            actual.runningPath == expected.runningPath && actual.liveProbeMask == expected.liveProbeMask &&
            actual.codePresent == expected.codePresent, "Native pawn links promoted or altered an original dormant frame");
    }
    Call(pawns[2], "Engine.Pawn.RemovePawn"); Link(Read(info, "PawnList"), pawns[1], "Original head removal failed");
    Link(Read(pawns[2], "nextPawn"), pawns[1], "Original head removal cleared retained nextPawn");
    Call(pawns[1], "Engine.Pawn.RemovePawn"); Call(pawns[0], "Engine.Pawn.RemovePawn");
    Link(Read(info, "PawnList"), "", "Original last-Pawn removal failed");
    Require(LoadPortableRuntimeState(temporary.Path("Baseline").string()), "Cannot restore original callback baseline");
    // Observed unmodified empty-resource branch, not seeded inventory/shadow or
    // selectively skipped Killed callbacks. Single insertion leaves no peers.
    for (const auto* name : {"Shadow", "Inventory", "PlayerReplicationInfo"})
        Link(Read(pawns[0], name), "", "Original empty-resource Destroyed branch prerequisite changed");
    Require(!QuestVr::Vm::ToBool(Read(pawns[0], "bIsPlayer")), "Original callback fixture is not a non-player Pawn");
    Call(pawns[0], "Engine.Pawn.AddPawn"); Call(pawns[0], "Engine.Pawn.Destroyed");
    Link(Read(info, "PawnList"), "", "Actual original Pawn.Destroyed did not run RemovePawn");
    Require(!ReadPortableActorStateObject(pawns[0]), "Original Destroyed fabricated a continuation frame");
    Require(LoadPortableRuntimeState(temporary.Path("Baseline").string()), "Cannot reset original Training before Intro");
    UnloadPortableRuntimeMap();
    Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root / "Maps" / "00_Intro.dx").string())).passed,
        "Cannot load the original Intro bird control");
    const std::string bird = "00_Intro.Pigeon0";
    const auto introActors = GetPortableRuntimeMapActors(true);
    Require(std::any_of(introActors.begin(), introActors.end(), [&](const auto& actor) { return actor.objectPath == bird && actor.pawn; }),
        "Original Intro lacks its actual Pigeon Pawn");
    const auto birdInfo = Read(bird, "Level").text;
    Link(Read(birdInfo, "PawnList"), "", "Original Intro has an unexpected initialized Pawn list");
    const auto birdContext = ReadPortableActorDispatchContext(bird);
    Call(bird, "Engine.Pawn.AddPawn"); Link(Read(birdInfo, "PawnList"), bird, "Actual bird AddPawn failed");
    Call(bird, "Engine.Pawn.RemovePawn"); Link(Read(birdInfo, "PawnList"), "", "Actual bird RemovePawn failed");
    Require(!ReadPortableActorStateObject(bird) && ReadPortableActorDispatchContext(bird).liveProbeMask == birdContext.liveProbeMask,
        "Original bird pawn links promoted or modified its raw frame");
    std::cout << "ORIGINAL Pawn.AddPawn529/RemovePawn530 actual Training humans/robot and Intro bird, retained next pointers,"
        " before-begun natives, raw dispatch identity retention, exact replacement/GC and real empty-resource Pawn.Destroyed;"
        " no automatic startup, owned inventory teardown or campaign claim\n";
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 2) { std::cout << "SKIP original PawnList integration: supply the read-only GOTY installation root.\n"; return 77; }
    try {
        const auto before = GC::GetStats().numObjects; Original(std::filesystem::path(argv[1])); GC::Collect();
        Require(GC::GetStats().numObjects == before, "Original PawnList integration retained UObjects after shutdown");
        std::cout << "Original PawnList integration checks=" << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << "Original PawnList integration failed: " << error.what() << '\n'; return 1; }
}
