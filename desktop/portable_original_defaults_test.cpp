#include "Precomp.h"
#include "GC/GC.h"
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
    Require(version >= 3u && version <= 7u, "Independent runtime checkpoint version mismatch");
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
    const bool hasState = std::any_of(state.objects.begin(), state.objects.end(), [](const auto& object) { return object.state.has_value(); });
    const bool hasLifecycle = std::any_of(state.objects.begin(), state.objects.end(), [](const auto& object) { return object.lifecycle.has_value(); });
    bytes[4u] = static_cast<std::uint8_t>(hasLifecycle ? 7u : !state.classDefaults.empty() ? 6u : hasState ? 5u : 4u);
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
    const auto level = ReadPortableActorScriptProperty(actor, "Level").text;
    const auto levelSnapshot = std::find_if(snapshots.begin(), snapshots.end(), [&](const auto& item) { return item.objectPath == level; });
    Require(levelSnapshot != snapshots.end(), "Original LevelInfo fixture is absent");
    Require(!QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor, "bDeleteMe")) &&
        !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(level, "bBegunPlay")),
        "Natural original fixture is not a live actor in a dormant level");
    const auto inWorld = [&](const bool includeInactive) {
        const auto current = GetPortableRuntimeMapActors(includeInactive);
        return std::any_of(current.begin(), current.end(), [&](const auto& item) { return item.objectPath == actor; });
    };
    const auto deletedSelf = [&]() {
        Require(QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor, "bDeleteMe")),
            "Original Destroy did not retain its reflected deletion flag");
        Require(!inWorld(false) && !inWorld(true),
            "Original deleted actor remained in the Level actor registry/snapshots");
        Require(QuestVr::Vm::Equal(ReadPortableClassDefault(found->classPath, "mpPickupAmmoCount"), pickup),
            "Original enclosing callback did not continue its concrete CDO assignment after Destroy");
        Require(QuestVr::Vm::Equal(ReadPortableActorScriptProperty(actor, "mpPickupAmmoCount"), instanceMp),
            "Deleted object's CDO mutation retroactively changed its instance copy");
        Require(QuestVr::Vm::Equal(ReadPortableActorScriptProperty(actor, "CollisionRadius"), radius) &&
            std::abs(QuestVr::Vm::ToFloat(ReadPortableActorScriptProperty(actor, "CollisionHeight")) -
                (height.floating - 0.75f)) < 0.00001f,
            "Original natural callback lost its collision trim on the deleted UObject");
        Require(QuestVr::Vm::Equal(ReadPortableClassDefault("DeusEx.DeusExWeapon", "mpPickupAmmoCount"), baseMp) &&
            QuestVr::Vm::Equal(ReadPortableClassDefault("DeusEx.WeaponPistol", "mpPickupAmmoCount"), siblingMp),
            "Natural deleted-self continuation changed base/sibling CDOs");
    };
    const auto result = ExecutePortableActorFunction(actor, "PreBeginPlay");
    Require(result.passed() && result.committed,
        "Natural original PreBeginPlay failed: " + result.error + " at " + result.function + ':' + std::to_string(result.offset));
    deletedSelf();
    const auto natural = temporary.Snapshot("NaturalDeleted");
    Require(Word(natural, 4u) == 7u, "Original Destroy lifecycle did not select the runtime v7 envelope");
    const auto naturalScript = SavedScript(natural);
    const auto naturalActor = std::find_if(naturalScript.objects.begin(), naturalScript.objects.end(),
        [&](const auto& object) { return object.path == actor; });
    Require(naturalActor != naturalScript.objects.end() && naturalActor->classPath == found->classPath &&
        naturalActor->lifecycle && naturalActor->lifecycle->worldRemoved,
        "Original deleted actor checkpoint lost its qualified tombstone/lifecycle identity");
    const auto deleteProperty = std::find_if(naturalActor->properties.begin(), naturalActor->properties.end(),
        [](const auto& property) { return property.key == "Engine.Actor.bDeleteMe"; });
    Require(deleteProperty != naturalActor->properties.end() && deleteProperty->index == 0u &&
        deleteProperty->name == "bDeleteMe" && QuestVr::Vm::ToBool(deleteProperty->value),
        "Original deletion flag was not captured with its actual inherited property identity");
    Require(natural.at(PrefixSize(natural) + 4u + 6u) == QuestVr::ScriptStateDetail::ActorLifecycleVersion,
        "Original deletion checkpoint did not use lifecycle codec v4");
    Require(ValidatePortableRuntimeState(temporary.Path("NaturalDeleted").string(), "02_NYC_Underground") &&
        temporary.Snapshot("AfterNaturalValidation") == natural,
        "Original deletion checkpoint validation changed native/reflected/CDO state");
    const auto gcObjects = GC::GetStats().numObjects;
    GC::Collect();
    Require(GC::GetStats().numObjects == gcObjects && temporary.Snapshot("AfterDeletedGc") == natural,
        "Deleted original UObject/defaults were prematurely collected or changed");
    const auto direct = ExecutePortableActorFunction(actor, "GetStateName");
    Require(direct.passed() && direct.committed && direct.value.kind == Kind::Name &&
        direct.value.text == ReadPortableActorDispatchContext(actor).stateName,
        "Deleted original UObject became unavailable to a direct native call after collection");
    const auto repeatedDestroy = ExecutePortableActorFunction(actor, "Destroy");
    Require(repeatedDestroy.passed() && repeatedDestroy.committed && repeatedDestroy.value.kind == Kind::Bool &&
        repeatedDestroy.value.boolean && temporary.Snapshot("AfterRepeatedDestroy") == natural,
        "Repeated original Destroy did not return true without changing the completed deletion");
    const auto collision = ExecutePortableActorFunction(actor, "SetCollisionSize",
        {{radius, {}}, {Value::Float(height.floating - 0.75f), {}}});
    Require(collision.passed() && collision.committed && collision.value.kind == Kind::Bool &&
        collision.value.boolean && temporary.Snapshot("AfterDeletedNativeWrite") == natural,
        "Deleted original UObject could not receive a direct reflected native write");
    Require(LoadPortableRuntimeState(temporary.Path("Legacy").string()) && inWorld(false) && inWorld(true) &&
        !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor, "bDeleteMe")) && !GetPortableRuntimeScriptStatePresent(),
        "Legacy restoration did not republish the original actor or clear its deletion timeline");
    Require(LoadPortableRuntimeState(temporary.Path("NaturalDeleted").string()) &&
        temporary.Snapshot("NaturalRoundTrip") == natural,
        "Original deletion/reflected/CDO timeline did not round-trip canonically");
    deletedSelf();
    GC::Collect();
    const auto restoredDirect = ExecutePortableActorFunction(actor, "GetStateName");
    Require(restoredDirect.passed() && restoredDirect.value.kind == Kind::Name &&
        temporary.Snapshot("AfterRestoredDeletedCall") == natural,
        "Restored deleted original UObject lost direct-call identity or canonical state after collection");
    bool provedNaturalRollback{};
    for (std::size_t instructionLimit = 1u; instructionLimit < result.instructions; ++instructionLimit) {
        Require(LoadPortableRuntimeState(temporary.Path("Legacy").string()), "Cannot reset natural deletion budget fixture");
        QuestVr::Vm::Limits limits; limits.instructions = instructionLimit;
        const auto failed = ExecutePortableActorFunction(actor, "PreBeginPlay", {}, limits);
        Require(failed.status == QuestVr::Vm::Status::Budget && !failed.committed,
            "Bounded natural original callback unexpectedly committed or failed outside its instruction budget");
        Require(temporary.Snapshot("AfterNaturalBudgetRefusal") == baseline && inWorld(false) &&
            !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor, "bDeleteMe")),
            "Natural nested failure leaked actor-registry deletion, reflected native state or CDO changes");
        if (failed.function == "DeusEx.DeusExWeapon.PreBeginPlay" && failed.offset >= 28u && failed.writes >= 1u) {
            Require(failed.callStack.size() >= 2u, "Natural deletion rollback fixture did not fail inside the original nested callback");
            provedNaturalRollback = true; break;
        }
    }
    Require(provedNaturalRollback, "No natural callback budget failure proved rollback after deleted-self CDO continuation");
    Require(LoadPortableRuntimeState(temporary.Path("Legacy").string()), "Cannot restore baseline after natural deletion proof");
    std::cout << "ORIGINAL NATURAL PREBEGINPLAY relevance Destroy279; deleted-self CDO continuation; v7/codec4 tombstone, GC/direct-call lifetime, legacy revival and nested rollback verified\n";

    // This is a separately labelled begun-play callback fixture, not automatic
    // world startup or an artificial relevance bypass. The original item has
    // no owner/marker, so the actual Weapon.Destroyed -> Inventory.Destroyed
    // code takes its authored no-marker/no-Pawn-owner branches.
    Require(ReadPortableActorScriptProperty(actor, "Owner").text.empty() &&
        ReadPortableActorScriptProperty(actor, "myMarker").text.empty(),
        "Original begun-play destruction fixture unexpectedly has an owner or inventory marker");
    QuestVr::ScriptSavedState begunPreconditions;
    begunPreconditions.mapName = "02_NYC_Underground";
    begunPreconditions.objects.push_back({level, levelSnapshot->classPath,
        {{"Engine.LevelInfo.bBegunPlay", "bBegunPlay", 0u, Value::Bool(false)},
         {"Engine.LevelInfo.NetMode", "NetMode", 0u, Value::Byte(0u)}}, {}});
    LoadGenerated(temporary, "DormantDestroyedBaseline", Envelope(baseline, begunPreconditions));
    const auto authoredDestroyed = ReadPortableActorDispatchContext(actor);
    const auto authoredDestroyedStack = ReadPortableActorSerializedStack(actor);
    Require(authoredDestroyedStack && authoredDestroyed.liveProbeMask == authoredDestroyedStack->probeMask &&
        QuestVr::ScriptDispatch::IsEnabled("Destroyed", authoredDestroyed.classProbeMask,
            authoredDestroyed.codeMasks, authoredDestroyed.disabledNames, authoredDestroyed.liveProbeMask),
        "Original positive authored Destroyed bit was inverted or recomputed from class masks");
    const auto disable = ExecutePortableActorFunction(actor, "Disable", {{Value::Text(Kind::Name, "Destroyed"), {}}});
    Require(disable.passed() && disable.committed,
        "Actual owned-frame Disable118 failed to establish the explicit disabled Destroyed control");
    const auto beforeEnabled = ReadPortableActorDispatchContext(actor);
    Require(beforeEnabled.disabledNames.count("Destroyed") == 1u || beforeEnabled.disabledNames.count("destroyed") == 1u,
        "Explicit owned-frame Disable did not retain its negative Destroyed gate");
    Require(!QuestVr::ScriptDispatch::IsEnabled("Destroyed", beforeEnabled.classProbeMask,
        beforeEnabled.codeMasks, beforeEnabled.disabledNames, beforeEnabled.liveProbeMask),
        "Actual Disable left the dormant Destroyed control enabled");
    const auto enable = ExecutePortableActorFunction(actor, "Enable", {{Value::Text(Kind::Name, "Destroyed"), {}}});
    const auto afterEnabled = ReadPortableActorDispatchContext(actor);
    Require(enable.passed() && enable.committed &&
        QuestVr::ScriptDispatch::IsEnabled("Destroyed", afterEnabled.classProbeMask, afterEnabled.codeMasks, afterEnabled.disabledNames, afterEnabled.liveProbeMask) &&
        afterEnabled.disabledNames.size() + 1u == beforeEnabled.disabledNames.size(),
        "Actual inherited Enable117 did not enable only the original Destroyed event probe");
    Require(ResolvePortableActorFunction(actor, afterEnabled.stateName, "Destroyed") ==
        std::optional<std::string>("Engine.Weapon.Destroyed"),
        "Original generated lifecycle fixture resolved a substituted Destroyed callback");
    const auto weaponDestroyed = LoadPortableFunctionScript(*engine, FindPortableExport(*engine, "Weapon.Destroyed"));
    Require(weaponDestroyed.bytecode.size() == 72u && weaponDestroyed.bytecode[0u] == 0x1cu &&
        GetPortableObjectPath(*engine, static_cast<std::int32_t>(Word(weaponDestroyed.bytecode, 1u))) == "Inventory.Destroyed",
        "Original Weapon.Destroyed no longer calls its actual Inventory.Destroyed superclass bytecode");
    const auto inventoryDestroyed = LoadPortableFunctionScript(*engine, FindPortableExport(*engine, "Inventory.Destroyed"));
    Require(inventoryDestroyed.bytecode.size() == 66u && inventoryDestroyed.bytecode[57u] == 0x1bu,
        "Original Inventory.Destroyed no longer contains its Pawn.DeleteInventory callback branch");
    const auto enabledDormant = temporary.Snapshot("EnabledDormantDestroyedBaseline");
    const auto matchedDormantResult = ExecutePortableActorFunction(actor, "PreBeginPlay");
    Require(matchedDormantResult.passed() && matchedDormantResult.committed,
        "Matched NetMode0 dormant original callback failed: " + matchedDormantResult.error);
    deletedSelf();
    auto enabledBegun = SavedScript(enabledDormant);
    auto enabledLevel = std::find_if(enabledBegun.objects.begin(), enabledBegun.objects.end(),
        [&](const auto& object) { return object.path == level; });
    Require(enabledLevel != enabledBegun.objects.end(), "Enabled dormant fixture lost its actual Level record");
    auto begunProperty = std::find_if(enabledLevel->properties.begin(), enabledLevel->properties.end(),
        [](const auto& property) { return property.key == "Engine.LevelInfo.bBegunPlay"; });
    Require(begunProperty != enabledLevel->properties.end(), "Enabled dormant fixture lost its actual begun-play property");
    begunProperty->value = Value::Bool(true);
    LoadGenerated(temporary, "BegunDestroyedBaseline", Envelope(baseline, enabledBegun));
    const auto begunResult = ExecutePortableActorFunction(actor, "PreBeginPlay");
    Require(begunResult.passed() && begunResult.committed && begunResult.instructions > matchedDormantResult.instructions,
        "Original begun-play Destroyed callback failed or was suppressed: " + begunResult.error + " at " +
            begunResult.function + ':' + std::to_string(begunResult.offset));
    deletedSelf();
    const auto begunDeleted = temporary.Snapshot("BegunDeleted");
    const auto enumDestroyed = ExecutePortableActorEvent(actor, "Destroyed", true);
    Require(enumDestroyed.passed() && enumDestroyed.committed && enumDestroyed.instructions != 0u &&
        begunResult.instructions - matchedDormantResult.instructions == enumDestroyed.instructions &&
        temporary.Snapshot("AfterEnumDestroyed") == begunDeleted,
        "Original deleted actor lost its enum-only Destroyed event exception or changed completed cleanup");
    const auto nameDestroyed = ExecutePortableActorEvent(actor, "Destroyed", false);
    const auto deletedName = ExecutePortableActorEvent(actor, "GetStateName", false);
    Require(nameDestroyed.passed() && nameDestroyed.instructions == 0u && nameDestroyed.value.kind == Kind::Nothing &&
        deletedName.passed() && deletedName.instructions == 0u && deletedName.value.kind == Kind::Nothing &&
        temporary.Snapshot("AfterDeletedEventGates") == begunDeleted,
        "Deleted actor incorrectly admitted named Destroyed/ordinary events or changed their skipped timeline");
    Require(LoadPortableRuntimeState(temporary.Path("Legacy").string()), "Cannot restore baseline after begun-play destruction proof");
    std::cout << "ORIGINAL DESTROYED CALLBACK generated bBegunPlay=true/NetMode=0 + actual Enable117(Destroyed); matched dormant instructions="
        << matchedDormantResult.instructions << " begun=" << begunResult.instructions << " actual Destroyed=" << enumDestroyed.instructions
        << "; original Weapon.Destroyed/Inventory.Destroyed no-owner/no-marker branch, enum-only deletion exception; not automatic startup or owned inventory cleanup\n";
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
        const auto gcBefore = GC::GetStats().numObjects;
        Original(std::filesystem::path(argv[1]));
        GC::Collect();
        Require(GC::GetStats().numObjects == gcBefore, "Original lifecycle/default integration leaked rooted objects after shutdown");
        std::cout << "Original defaults integration checks=" << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Original defaults integration failed: " << error.what() << '\n';
        return 1;
    }
}
