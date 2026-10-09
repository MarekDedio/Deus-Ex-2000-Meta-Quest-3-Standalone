#include "Precomp.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"
#include "quest_script_state.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
using Value = QuestVr::Vm::Value;
using Kind = QuestVr::Vm::Kind;
std::size_t checks{};
void Require(bool condition, const std::string& description) {
    if (!condition) throw std::runtime_error(description);
    ++checks;
}
struct Temporary {
    std::filesystem::path parent, directory;
    explicit Temporary(const std::filesystem::path& original) {
        parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto source = std::filesystem::canonical(original);
        Require(parent.string().rfind(source.string(), 0u) != 0u,
            "Generated checkpoint directory overlaps commercial installation");
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0u; attempt < 20u; ++attempt) {
            const auto candidate = parent / ("deusex-original-default-test-" + std::to_string(stamp) + '-' + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) {
                directory = std::filesystem::canonical(candidate); break;
            }
        }
        Require(!directory.empty() && directory.parent_path() == parent,
            "Generated checkpoint escaped its owned temporary parent");
    }
    ~Temporary() {
        ShutdownPortableRuntime();
        std::error_code error;
        const auto actual = std::filesystem::weakly_canonical(directory, error);
        if (!error && !directory.empty() && actual == directory && actual.parent_path() == parent &&
            actual.filename().string().rfind("deusex-original-default-test-", 0u) == 0u)
            std::filesystem::remove_all(actual, error);
    }
    std::filesystem::path Path(const std::string& name) const { return directory / (name + ".sav"); }
    Bytes Snapshot(const std::string& name) const {
        const auto path = Path(name);
        Require(SavePortableRuntimeState(path.string()), "Could not save isolated original runtime checkpoint");
        Bytes bytes;
        Require(QuestVr::ReadBoundedSaveFile(path.string(), QuestVr::kMaximumSaveRuntimeBytes, bytes),
            "Could not inspect isolated original runtime checkpoint");
        return bytes;
    }
};
std::uint32_t Word(const Bytes& bytes, std::size_t offset) {
    Require(offset <= bytes.size() && bytes.size() - offset >= 4u, "Checkpoint word exceeds independent parser bound");
    std::uint32_t value{};
    for (unsigned i = 0u; i < 4u; ++i) value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8u * i);
    return value;
}
std::size_t PrefixSize(const Bytes& bytes) {
    Require(Word(bytes, 0u) == 0x53515844u, "Independent runtime checkpoint magic mismatch");
    const auto version = Word(bytes, 4u);
    Require(version >= 3u && version <= 6u, "Independent runtime checkpoint version mismatch");
    std::size_t cursor = 8u;
    const auto word = [&]() { const auto value = Word(bytes, cursor); cursor += 4u; return value; };
    const auto skip = [&](std::size_t length) {
        Require(cursor <= bytes.size() && length <= bytes.size() - cursor, "Independent prefix read exceeds checkpoint");
        cursor += length;
    };
    const auto string = [&]() { const auto length = word(); Require(length <= 1'048'576u, "Independent prefix string exceeds bound"); skip(length); };
    const auto list = [&]() {
        const auto count = word(); Require(count <= 100'000u, "Independent prefix list exceeds bound");
        for (std::uint32_t i = 0u; i < count; ++i) string();
    };
    list(); list(); list(); skip(4u);
    const auto damaged = word(); Require(damaged <= 100'000u, "Independent damaged actor list exceeds bound");
    for (std::uint32_t i = 0u; i < damaged; ++i) { string(); skip(4u); }
    skip(8u); list(); list(); list(); list();
    return cursor;
}
QuestVr::ScriptSavedState SavedScript(const Bytes& bytes) {
    Require(Word(bytes, 4u) >= 4u, "Original CDO mutation did not create a script checkpoint");
    const auto offset = PrefixSize(bytes);
    const auto size = Word(bytes, offset);
    Require(offset + 4u <= bytes.size() && size == bytes.size() - offset - 4u,
        "Original script payload length does not cover the exact trailer");
    return QuestVr::DecodeScriptSavedState(Bytes(bytes.begin() + static_cast<std::ptrdiff_t>(offset + 4u), bytes.end()));
}
Bytes Envelope(const Bytes& legacy, const QuestVr::ScriptSavedState& state) {
    Require(Word(legacy, 0u) == 0x53515844u && Word(legacy, 4u) == 3u, "Generated preconditions need untouched authored v3 prefix");
    auto bytes = legacy;
    bytes[4u] = static_cast<std::uint8_t>(state.classDefaults.empty() ? 4u : 6u);
    const auto blob = QuestVr::EncodeScriptSavedState(state);
    for (unsigned i = 0u; i < 4u; ++i) bytes.push_back(static_cast<std::uint8_t>(blob.size() >> (8u * i)));
    bytes.insert(bytes.end(), blob.begin(), blob.end());
    return bytes;
}
void LoadGenerated(const Temporary& temporary, const std::string& name, const Bytes& bytes) {
    const auto path = temporary.Path(name);
    Require(QuestVr::WriteDurableSaveFile(path.string(), bytes) && ValidatePortableRuntimeState(path.string()) &&
        LoadPortableRuntimeState(path.string()), "Generated schema-checked preconditions were rejected");
}
void Original(const std::filesystem::path& root) {
    Temporary temporary(root);
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
    std::vector<PortablePackageTables> packages;
    for (const auto* name : names)
        packages.push_back(LoadPortablePackageTables((root / "System" / (std::string(name) + ".u")).string()));
    const auto initialized = InitializePortableRuntime(packages);
    Require(initialized.passed && initialized.classes != 0u, "Original package runtime initialization failed");
    std::cout << "ORIGINAL RUNTIME indexed objects=" << initialized.objects << '\n';
    const auto map = LoadPortablePackageTables((root / "Maps" / "02_NYC_Underground.dx").string());
    Require(LoadPortableRuntimeMap(map).passed, "Original Underground map did not load");
    const std::string actor = "02_NYC_Underground.WeaponAssaultGun0";
    const auto snapshots = GetPortableRuntimeMapActors();
    const auto found = std::find_if(snapshots.begin(), snapshots.end(), [&](const auto& item) { return item.objectPath == actor; });
    Require(found != snapshots.end() && found->classPath == "DeusEx.WeaponAssaultGun",
        "Authored AssaultGun fixture not present or has different concrete class");
    const auto baselineMp = ReadPortableClassDefault(found->classPath, "mpPickupAmmoCount");
    const auto pickup = ReadPortableClassDefault(found->classPath, "PickupAmmoCount");
    const auto instanceMp = ReadPortableActorScriptProperty(actor, "mpPickupAmmoCount");
    const auto radius = ReadPortableActorScriptProperty(actor, "CollisionRadius");
    const auto height = ReadPortableActorScriptProperty(actor, "CollisionHeight");
    const auto baseMp = ReadPortableClassDefault("DeusEx.DeusExWeapon", "mpPickupAmmoCount");
    const auto siblingMp = ReadPortableClassDefault("DeusEx.WeaponPistol", "mpPickupAmmoCount");
    Require(baselineMp.kind == Kind::Int && baselineMp.integer == 0 && pickup.kind == Kind::Int && pickup.integer == 30,
        "Original AssaultGun CDO precondition differs from the authored GOTY fixture");
    Require(QuestVr::Vm::Equal(instanceMp, baselineMp) && radius.kind == Kind::Float && height.kind == Kind::Float,
        "Original instance/native collision fixture is malformed");
    // Assert the actual original compiled assignment's field identities. This
    // executes the entire original callback below, not a copied script body.
    const auto& source = packages.at(2u);
    const auto compiled = LoadPortableFunctionScript(source, FindPortableExport(source, "DeusExWeapon.PreBeginPlay"));
    Require(compiled.bytecode.size() == 30u && compiled.bytecode[17u] == 0x0fu &&
        compiled.bytecode[18u] == 0x02u && compiled.bytecode[23u] == 0x02u &&
        GetPortableObjectPath(source, static_cast<std::int32_t>(Word(compiled.bytecode, 19u))) == "DeusExWeapon.mpPickupAmmoCount" &&
        GetPortableObjectPath(source, static_cast<std::int32_t>(Word(compiled.bytecode, 24u))) == "Engine.Weapon.PickupAmmoCount",
        "Original DefaultVariable assignment no longer matches its actual declared fields");
    const auto engine = std::find_if(packages.begin(), packages.end(), [](const auto& package) {
        return std::filesystem::path(package.sourcePath).stem().string() == "Engine";
    });
    Require(engine != packages.end(), "Original Engine source table is missing");
    const auto actorCallback = LoadPortableFunctionScript(*engine, FindPortableExport(*engine, "Actor.PreBeginPlay"));
    const auto appendReference = [](Bytes& bytes, std::size_t reference) {
        Require(reference <= 0x7fffffffu, "Original property reference exceeds normalized int32");
        for (unsigned i = 0u; i < 4u; ++i) bytes.push_back(static_cast<std::uint8_t>(reference >> (8u * i)));
    };
    // Actual original native283 arguments: radius, height minus float0.75.
    // Index175 is the pin's Subtract_FloatFloat, not Multiply_FloatFloat171.
    Bytes collisionArguments{0x61u, 0x1bu, 0x01u};
    appendReference(collisionArguments, FindPortableExport(*engine, "Actor.CollisionRadius") + 1u);
    collisionArguments.push_back(0xafu); collisionArguments.push_back(0x01u);
    appendReference(collisionArguments, FindPortableExport(*engine, "Actor.CollisionHeight") + 1u);
    const Bytes subtractionTail{0x1eu, 0x00u, 0x00u, 0x40u, 0x3fu, 0x16u, 0x16u};
    collisionArguments.insert(collisionArguments.end(), subtractionTail.begin(), subtractionTail.end());
    Require(std::search(actorCallback.bytecode.begin(), actorCallback.bytecode.end(),
        collisionArguments.begin(), collisionArguments.end()) != actorCallback.bytecode.end(),
        "Original native283 callback does not contain its typed radius/height-minus0.75 argument sequence");
    const auto baseline = temporary.Snapshot("Legacy");
    const auto result = ExecutePortableActorFunction(actor, "PreBeginPlay");
    Require(result.status == QuestVr::Vm::Status::Unsupported && !result.committed &&
        result.function == "Engine.Actor.PreBeginPlay" && result.offset == 199u && result.opcode == 0x61u &&
        result.error.find("Unsupported runtime native 279") != std::string::npos,
        "Normal authored actor relevance barrier was silently bypassed or changed");
    Require(temporary.Snapshot("AfterRefusal") == baseline, "Original callback failure leaked persistent writes");
    std::cout << "ORIGINAL AUTHORED PREBEGINPLAY refuses native279 at Engine.Actor.PreBeginPlay:199; exact rollback\n";
    const auto level = ReadPortableActorScriptProperty(actor, "Level").text;
    const auto levelSnapshot = std::find_if(snapshots.begin(), snapshots.end(), [&](const auto& item) { return item.objectPath == level; });
    Require(levelSnapshot != snapshots.end(), "Original LevelInfo fixture is absent");
    QuestVr::ScriptSavedState preconditions;
    preconditions.mapName = "02_NYC_Underground";
    preconditions.objects.push_back({actor, found->classPath,
        {{"Engine.Actor.bGameRelevant", "bGameRelevant", 0u, Value::Bool(true)}}, {}});
    preconditions.objects.push_back({level, levelSnapshot->classPath,
        {{"Engine.LevelInfo.NetMode", "NetMode", 0u, Value::Byte(0u)}}, {}});
    for (const auto mode : {std::uint8_t{0u}, std::uint8_t{2u}}) {
        preconditions.objects.back().properties.front().value = Value::Byte(mode);
        const auto modeName = mode == 0u ? "Singleplayer" : "ListenServer";
        LoadGenerated(temporary, modeName, Envelope(baseline, preconditions));
        const auto beforeGeneratedCall = temporary.Snapshot("BeforeGeneratedCall");
        const auto generatedResult = ExecutePortableActorFunction(actor, "PreBeginPlay");
        Require(generatedResult.passed() && generatedResult.committed && generatedResult.writes >= 1u,
            std::string("Complete original PreBeginPlay failed for generated ") + modeName + " preconditions: " +
                generatedResult.error + " at " + generatedResult.function + ':' + std::to_string(generatedResult.offset));
        Require(QuestVr::Vm::Equal(ReadPortableClassDefault(found->classPath, "mpPickupAmmoCount"), pickup),
            "Original default assignment did not update the actor's concrete shared CDO");
        Require(QuestVr::Vm::Equal(ReadPortableActorScriptProperty(actor, "mpPickupAmmoCount"), instanceMp),
            "Original CDO write retroactively changed the already-loaded actor instance");
        Require(QuestVr::Vm::Equal(ReadPortableClassDefault("DeusEx.DeusExWeapon", "mpPickupAmmoCount"), baseMp) &&
            QuestVr::Vm::Equal(ReadPortableClassDefault("DeusEx.WeaponPistol", "mpPickupAmmoCount"), siblingMp),
            "Concrete CDO assignment leaked into its base or sibling class");
        const auto expectedHeight = height.floating - 0.75f;
        Require(QuestVr::Vm::Equal(ReadPortableActorScriptProperty(actor, "CollisionRadius"), radius) &&
            std::abs(QuestVr::Vm::ToFloat(ReadPortableActorScriptProperty(actor, "CollisionHeight")) - expectedHeight) < 0.00001f,
            "Original Actor.PreBeginPlay inventory branch did not retain radius and subtract its authored 0.75 height trim");
        const auto changed = temporary.Snapshot("Changed");
        Require(Word(changed, 4u) == 6u, "Real original CDO mutation did not choose the runtime v6 save envelope");
        const auto script = SavedScript(changed);
        Require(script.classDefaults.size() == 1u && script.classDefaults.front().classPath == found->classPath &&
            script.classDefaults.front().properties.size() == 1u, "Real original CDO save recorded wrong class or extra defaults");
        const auto& property = script.classDefaults.front().properties.front();
        Require(property.key == "DeusEx.DeusExWeapon.mpPickupAmmoCount" && property.name == "mpPickupAmmoCount" &&
            property.index == 0u && QuestVr::Vm::Equal(property.value, pickup),
            "Real original CDO save lost its exact inherited declaration identity/value");
        const auto trailer = PrefixSize(changed);
        Require(changed.at(trailer + 4u + 6u) == QuestVr::ScriptStateDetail::ClassDefaultsVersion,
            "Real original CDO save did not use codec v3");
        Require(ValidatePortableRuntimeState(temporary.Path("Changed").string(), "02_NYC_Underground") &&
            temporary.Snapshot("AfterValidation") == changed, "Original CDO validation mutated shared storage");
        Require(LoadPortableRuntimeState(temporary.Path("Legacy").string()) && !GetPortableRuntimeScriptStatePresent(),
            "Legacy restore failed to remove original CDO and instance patches");
        Require(QuestVr::Vm::Equal(ReadPortableClassDefault(found->classPath, "mpPickupAmmoCount"), baselineMp) &&
            QuestVr::Vm::Equal(ReadPortableActorScriptProperty(actor, "CollisionHeight"), height),
            "Legacy restore did not recover original immutable CDO/instance baselines");
        Require(LoadPortableRuntimeState(temporary.Path("Changed").string()) && temporary.Snapshot("RoundTrip") == changed,
            "Real original v6 class/instance checkpoint did not round-trip canonically");
        // A failure after the actual default write, while still nested in the
        // original superclass callback, must undo both CDO and native changes.
        bool provedNestedRollback{};
        for (std::size_t instructionLimit = 1u; instructionLimit < generatedResult.instructions; ++instructionLimit) {
            LoadGenerated(temporary, "BudgetBaseline", beforeGeneratedCall);
            QuestVr::Vm::Limits limits; limits.instructions = instructionLimit;
            const auto failed = ExecutePortableActorFunction(actor, "PreBeginPlay", {}, limits);
            Require(failed.status == QuestVr::Vm::Status::Budget && !failed.committed,
                "Bounded original callback unexpectedly committed or failed outside its instruction budget");
            Require(temporary.Snapshot("AfterBudgetRefusal") == beforeGeneratedCall,
                "Budgeted nested original callback leaked its CDO/native/instance state");
            if (failed.function == "DeusEx.DeusExWeapon.PreBeginPlay" && failed.offset >= 28u && failed.writes >= 1u) {
                Require(failed.callStack.size() >= 2u, "CDO rollback fixture did not fail within an original nested callback");
                provedNestedRollback = true; break;
            }
        }
        Require(provedNestedRollback, "No bounded original nested failure reached the committed-default write before rollback");
        std::cout << "ORIGINAL COMPLETE PREBEGINPLAY " << modeName << " generated bGameRelevant=true/NetMode=" << static_cast<unsigned>(mode)
            << "; original native283 height=" << expectedHeight << "; concrete default mpPickupAmmoCount=30"
            << "; old instance/base/sibling isolation; v6+codec3 roundtrip and nested rollback verified\n";
    }
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "SKIP original class-default integration: supply the read-only Deus Ex GOTY installation root.\n";
        return 77;
    }
    try {
        Original(std::filesystem::path(argv[1]));
        std::cout << "Original defaults integration checks=" << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Original defaults integration failed: " << error.what() << '\n';
        return 1;
    }
}
