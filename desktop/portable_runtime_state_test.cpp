#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"
#include "quest_save_metadata.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
void Require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

struct IsolatedCheckpointDirectory {
    std::filesystem::path path;
    explicit IsolatedCheckpointDirectory(const std::filesystem::path& gameRoot) {
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        for (auto ancestor = parent; !ancestor.empty();) {
            Require(!std::filesystem::equivalent(ancestor, gameRoot),
                    "Temporary checkpoint parent must be outside the original game installation");
            const auto next = ancestor.parent_path();
            if (next == ancestor) break;
            ancestor = next;
        }
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 20; ++attempt) {
            const auto candidate = parent / ("deusex-runtime-state-test-" +
                std::to_string(stamp) + '-' + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) {
                path = std::filesystem::canonical(candidate);
                Require(path.parent_path() == parent &&
                    path.filename().string().rfind("deusex-runtime-state-test-", 0) == 0,
                    "Checkpoint directory escaped its generated temporary parent");
                return;
            }
        }
        throw std::runtime_error("Could not create isolated checkpoint directory");
    }
    ~IsolatedCheckpointDirectory() {
        if (!path.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    }
};

// Inspect the test's generated v3 checkpoint independently of live getters.
// Sorted unordered fields allow exact comparison without relying on hash-table
// iteration order. This is not a new runtime save parser or a user-save editor.
struct SavedState {
    std::vector<std::string> inventory, inactive, activated, flags, goals, notes, applied;
    std::vector<std::pair<std::string, float>> damaged;
    float health{};
    std::int32_t credits{}, skillPoints{};
    bool operator==(const SavedState&) const = default;
};

SavedState ReadGeneratedCheckpoint(const std::filesystem::path& path) {
    Require(std::filesystem::file_size(path) <= 16u * 1024u * 1024u,
            "Generated checkpoint unexpectedly exceeds test read budget");
    std::ifstream stream(path, std::ios::binary);
    Require(static_cast<bool>(stream), "Could not inspect generated checkpoint");
    const auto read = [&](auto& value) {
        stream.read(reinterpret_cast<char*>(&value), sizeof(value));
        Require(static_cast<bool>(stream), "Generated checkpoint was truncated");
    };
    const auto readString = [&]() {
        std::uint32_t size{};
        read(size);
        Require(size <= 1'048'576u, "Generated checkpoint string exceeds test bound");
        std::string value(size, '\0');
        stream.read(value.data(), size);
        Require(static_cast<bool>(stream), "Generated checkpoint string was truncated");
        return value;
    };
    const auto readStrings = [&](auto& values) {
        std::uint32_t count{};
        read(count);
        Require(count <= 100'000u, "Generated checkpoint list exceeds test bound");
        for (std::uint32_t i = 0; i < count; ++i) values.push_back(readString());
    };
    std::uint32_t magic{}, version{};
    read(magic); read(version);
    Require(magic == 0x53515844u && version == 3u, "Generated checkpoint is not runtime format v3");
    SavedState state;
    readStrings(state.inventory); readStrings(state.inactive); readStrings(state.activated);
    read(state.health);
    std::uint32_t count{};
    read(count);
    Require(count <= 100'000u, "Generated damaged-actor list exceeds test bound");
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto actor = readString();
        float health{};
        read(health);
        state.damaged.emplace_back(actor, health);
    }
    read(state.credits); read(state.skillPoints);
    readStrings(state.flags); readStrings(state.goals); readStrings(state.notes); readStrings(state.applied);
    Require(stream.peek() == std::char_traits<char>::eof(), "Generated checkpoint has trailing bytes");
    for (auto* unordered : {&state.inactive, &state.activated, &state.flags, &state.applied})
        std::sort(unordered->begin(), unordered->end());
    std::sort(state.damaged.begin(), state.damaged.end());
    return state;
}

auto ActorFields(const PortableActorSnapshot& actor) {
    return std::tie(actor.objectPath, actor.classPath, actor.x, actor.y, actor.z,
        actor.hasLocation, actor.pawn, actor.inventory, actor.decoration, actor.mover,
        actor.trigger, actor.travel, actor.light, actor.hidden, actor.activated,
        actor.drawType, actor.destinationMap, actor.drawScale, actor.drawScaleX,
        actor.drawScaleY, actor.drawScaleZ, actor.pitch, actor.yaw, actor.roll,
        actor.meshPath, actor.meshClassPath, actor.brushPath, actor.texturePath,
        actor.ambientSoundPath, actor.soundRadius, actor.soundVolume, actor.soundPitch,
        actor.lightType, actor.lightEffect, actor.lightBrightness, actor.lightHue,
        actor.lightSaturation, actor.lightRadius,
        actor.lightCone);
}

struct LiveState {
    std::vector<PortableActorSnapshot> actors;
    std::vector<std::string> inventory;
    PortablePlayerProgress progress;
    float health{};
};

LiveState CaptureLiveState() {
    return {GetPortableRuntimeMapActors(), GetPortableRuntimeInventoryItems(),
            GetPortableRuntimePlayerProgress(), GetPortableRuntimePlayerHealth()};
}

void RequireSameLiveState(const LiveState& expected) {
    const auto current = CaptureLiveState();
    Require(current.health == expected.health, "Rollback did not restore player health");
    Require(current.inventory == expected.inventory &&
            GetPortableRuntimeInventoryCount() == expected.inventory.size(),
            "Rollback did not restore ordered inventory items/count");
    Require(current.progress.credits == expected.progress.credits &&
            current.progress.skillPoints == expected.progress.skillPoints &&
            current.progress.goals == expected.progress.goals &&
            current.progress.notes == expected.progress.notes,
            "Rollback did not restore credits, skill points, goals or notes");
    Require(current.actors.size() == expected.actors.size(), "Rollback did not restore active actor count");
    for (std::size_t i = 0; i < current.actors.size(); ++i)
        Require(ActorFields(current.actors[i]) == ActorFields(expected.actors[i]),
                "Rollback did not restore actor identity, activation or original properties");
}

PortableDialogueResult EffectFixture(const std::string& prefix, const std::int32_t amount, const bool flag) {
    PortableDialogueResult result;
    using Effect = PortableDialogueResult::Effect;
    for (const auto type : {Effect::Type::SetFlag, Effect::Type::AddGoal,
            Effect::Type::AddNote, Effect::Type::AddSkillPoints, Effect::Type::AddCredits}) {
        Effect effect;
        effect.type = type;
        effect.eventPath = "RuntimeRollbackTest." + prefix + '.' + std::to_string(static_cast<int>(type));
        effect.key = "RuntimeRollbackTestFlag";
        effect.value = flag;
        effect.amount = amount;
        effect.text = "Synthetic rollback " + prefix + " progress fixture";
        result.effects.push_back(std::move(effect));
    }
    return result;
}

void TestOriginalRollback(const std::filesystem::path& suppliedGameRoot) {
    const auto gameRoot = std::filesystem::canonical(suppliedGameRoot);
    Require(std::filesystem::is_regular_file(gameRoot / "System" / "DeusEx.u") &&
            std::filesystem::is_regular_file(gameRoot / "Maps" / "00_Training.dx") &&
            std::filesystem::is_regular_file(gameRoot / "Maps" / "00_TrainingCombat.dx"),
            "GameRoot lacks the original packages or Training/TrainingCombat maps");
    // Same explicit package set as Quest startup. Owned data is opened read-only;
    // generated checkpoints live in the separately validated temporary folder.
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
    std::cout << "Loading " << sizeof(names) / sizeof(names[0])
              << " original System packages read-only for rollback verification..." << std::endl;
    for (const auto* name : names)
        packages.push_back(LoadPortablePackageTables((gameRoot / "System" / (std::string(name) + ".u")).string()));
    struct Shutdown { ~Shutdown() { ShutdownPortableRuntime(); } } shutdown;
    const auto runtime = InitializePortableRuntime(packages);
    Require(runtime.passed, "Original-package portable runtime initialization failed");
    std::cout << "Runtime ready: " << runtime.objects
              << " objects. Testing Training map checkpoint and replacement..." << std::endl;
    const auto training = LoadPortablePackageTables((gameRoot / "Maps" / "00_Training.dx").string());
    const auto combat = LoadPortablePackageTables((gameRoot / "Maps" / "00_TrainingCombat.dx").string());
    Require(LoadPortableRuntimeMap(training).passed, "Original Training map runtime failed");
    const auto actors = GetPortableRuntimeMapActors();
    const auto choose = [&](auto predicate, const char* missing) {
        const auto found = std::find_if(actors.begin(), actors.end(), predicate);
        Require(found != actors.end(), missing);
        return found->objectPath;
    };
    const auto item = choose([](const auto& actor) { return actor.inventory; }, "Training has no inventory test actor");
    const auto mover = choose([](const auto& actor) { return actor.mover; }, "Training has no mover test actor");
    const auto pawn = choose([](const auto& actor) { return actor.pawn; }, "Training has no pawn test actor");
    Require(InteractPortableRuntimeActor(item).action == "pickup", "Original inventory pickup fixture failed");
    Require(InteractPortableRuntimeActor(mover).action == "mover_open", "Original mover activation fixture failed");
    const auto damaged = DamagePortableRuntimeActor(pawn, 1.0f);
    Require(damaged.handled && !damaged.killed && damaged.remainingHealth > 1.0f,
            "Original pawn damage fixture failed");
    Require(DamagePortableRuntimePlayer(27.0f) == 73.0f, "Player damage fixture failed");
    const auto seededEffects = EffectFixture("checkpoint", 123, true);
    Require(ApplyPortableDialogueEffects(seededEffects).applied == 5,
            "Synthetic progress effects fixture failed");
    const auto expected = CaptureLiveState();
    Require(expected.inventory.size() == 1 && expected.progress.credits == 123 &&
            expected.progress.skillPoints == 123 && expected.progress.goals.size() == 1 &&
            expected.progress.notes.size() == 1 && expected.actors.size() + 1 == actors.size(),
            "Checkpoint fixture did not mutate all expected live fields");

    IsolatedCheckpointDirectory directory(gameRoot);
    const auto checkpoint = directory.path / "rollback-checkpoint.sav";
    const auto afterRollback = directory.path / "after-rollback.sav";
    Require(SavePortableRuntimeState(checkpoint.string()), "Rollback checkpoint write failed");
    const auto saved = ReadGeneratedCheckpoint(checkpoint);
    Require(saved.inventory == expected.inventory && saved.health == expected.health &&
            saved.flags == std::vector<std::string>{"runtimerollbacktestflag\n1"} &&
            saved.damaged == std::vector<std::pair<std::string, float>>{{pawn, damaged.remainingHealth}} &&
            std::find(saved.inactive.begin(), saved.inactive.end(), item) != saved.inactive.end() &&
            std::find(saved.activated.begin(), saved.activated.end(), mover) != saved.activated.end(),
            "Checkpoint did not serialize inactive/activated actors, pawn health or progress flag");
    Require(!SavePortableRuntimeState(directory.path.string()), "Directory accepted as checkpoint output");
    Require(!LoadPortableRuntimeState((directory.path / "missing.sav").string()), "Missing checkpoint accepted");
    RequireSameLiveState(expected);
    std::vector<std::uint8_t> forgedDamagedPath;
    const auto append32 = [&](const std::uint32_t value) {
        for (unsigned shift = 0; shift < 32u; shift += 8u)
            forgedDamagedPath.push_back(static_cast<std::uint8_t>(value >> shift));
    };
    for (const auto value : {0x53515844u, 2u, 0u, 0u, 0u, 0x42c80000u, 1u, 0xffffffffu}) append32(value);
    const auto forgedCheckpoint = directory.path / "forged-damaged-path.runtime.tmp";
    Require(QuestVr::WriteDurableSaveFile(forgedCheckpoint.string(), forgedDamagedPath) &&
            !ValidatePortableRuntimeState(forgedCheckpoint.string()) &&
            !LoadPortableRuntimeState(forgedCheckpoint.string()),
            "Forged damaged-path length was allocated or accepted");
    RequireSameLiveState(expected);

    Require(ConsumePortableRuntimeInventoryItem(item), "Inventory mutation fixture failed");
    Require(InteractPortableRuntimeActor(mover).action == "mover_close", "Mover mutation fixture failed");
    Require(DamagePortableRuntimeActor(pawn, 10.0f).handled, "Pawn mutation fixture failed");
    Require(HealPortableRuntimePlayer(20.0f) == 93.0f, "Player heal mutation fixture failed");
    Require(ApplyPortableDialogueEffects(EffectFixture("mutation", 50, false)).applied == 5,
            "Progress mutation fixture failed");
    const auto mutated = CaptureLiveState();
    Require(mutated.inventory.empty() && mutated.progress.credits == 173 &&
            mutated.progress.goals.size() == 2, "Mutation did not differ from saved checkpoint");
    Require(ValidatePortableRuntimeState(checkpoint.string()), "Valid runtime preflight failed");
    RequireSameLiveState(mutated);
    const auto truncated = directory.path / "truncated.sav";
    std::filesystem::copy_file(checkpoint, truncated);
    std::filesystem::resize_file(truncated, std::filesystem::file_size(truncated) - 1);
    Require(!LoadPortableRuntimeState(truncated.string()), "Truncated checkpoint accepted");
    RequireSameLiveState(mutated);

    const auto switched = LoadPortableRuntimeMap(combat);
    Require(switched.passed && switched.replacedExports == training.exports.size(),
            "Original Combat map replacement failed");
    Require(GetPortableRuntimeMapActors().front().objectPath.rfind("00_TrainingCombat.", 0) == 0,
            "Map replacement retained the prior map actor namespace");
    Require(LoadPortableRuntimeMap(training).passed, "Rollback prior-map load failed");
    Require(LoadPortableRuntimeState(checkpoint.string()), "Rollback checkpoint load failed");
    RequireSameLiveState(expected);
    Require(SavePortableRuntimeState(afterRollback.string()), "Restored state inspection write failed");
    Require(ReadGeneratedCheckpoint(afterRollback) == saved,
            "Rollback did not exactly restore saved flags/effect deduplication/actor damage state");
    Require(ApplyPortableDialogueEffects(seededEffects).applied == 0,
            "Rollback lost previously-applied progress effect deduplication");
    const auto restoredDamage = DamagePortableRuntimeActor(pawn, 1.0f);
    Require(restoredDamage.handled && restoredDamage.remainingHealth == damaged.remainingHealth - 1.0f,
            "Restored pawn health did not match checkpoint");
    Require(LoadPortableRuntimeState(checkpoint.string()), "Final fixture reset failed");
    RequireSameLiveState(expected);

    // Exercise the actual runtime serializer through paired storage, including
    // cross-map generation identity and recovery from a corrupt latest slot.
    const auto bundlePaths = QuestVr::MakeSaveBundlePaths((directory.path / "quest-save-paired").string());
    const auto captureSecond = directory.path / "paired-capture-second.runtime.tmp";
    const auto loadStage = directory.path / "paired-load.runtime.tmp";
    const QuestVr::QuestSaveMetadata uiFirst{{1.0f, 2.0f, 3.0f, 4.0f}, "00_Training",
        {{item, 1u}}, {"Synthetic paired generation 1 UI log"}, true};
    std::vector<std::uint8_t> firstMetadata, firstRuntime;
    Require(QuestVr::EncodeQuestSaveMetadata(uiFirst, firstMetadata) &&
            QuestVr::ReadBoundedSaveFile(checkpoint.string(), QuestVr::kMaximumSaveRuntimeBytes, firstRuntime),
            "First paired UI/runtime capture failed");
    const auto firstPublish = QuestVr::PublishSaveBundle(bundlePaths, firstMetadata, firstRuntime);
    Require(firstPublish.saved && firstPublish.generation == 1u, "First actual runtime bundle publish failed");
    Require(LoadPortableRuntimeMap(combat).passed, "Second paired generation map load failed");
    Require(DamagePortableRuntimePlayer(5.0f) == 68.0f && ConsumePortableRuntimeInventoryItem(item) &&
            ApplyPortableDialogueEffects(EffectFixture("paired_second", 7, false)).applied == 5u,
            "Second paired generation live mutation failed");
    const auto expectedSecond = CaptureLiveState();
    const QuestVr::QuestSaveMetadata uiSecond{{9.0f, 8.0f, 7.0f, 6.0f}, "00_TrainingCombat",
        {{"SyntheticCombatDialogueCursor", 2u}}, {"Synthetic paired generation 2 UI log"}};
    std::vector<std::uint8_t> secondMetadata, secondRuntime;
    Require(QuestVr::EncodeQuestSaveMetadata(uiSecond, secondMetadata) &&
            SavePortableRuntimeState(captureSecond.string()) &&
            QuestVr::ReadBoundedSaveFile(captureSecond.string(), QuestVr::kMaximumSaveRuntimeBytes, secondRuntime),
            "Second paired UI/runtime capture failed");
    const auto secondPublish = QuestVr::PublishSaveBundle(bundlePaths, secondMetadata, secondRuntime);
    Require(secondPublish.saved && secondPublish.generation == 2u, "Second actual runtime bundle publish failed");
    auto pairs = QuestVr::LoadSaveBundleCandidates(bundlePaths);
    Require(pairs.size() == 2u && pairs[0].generation == 2u && pairs[1].generation == 1u &&
            pairs[0].metadata == secondMetadata && pairs[0].runtime == secondRuntime &&
            pairs[1].metadata == firstMetadata && pairs[1].runtime == firstRuntime,
            "Actual runtime bundle candidates mixed UI/runtime generations");
    const auto verifyMetadata = [](const QuestVr::QuestSaveMetadata& wanted, const QuestVr::SaveBundleData& pair) {
        QuestVr::QuestSaveMetadata ui;
        Require(QuestVr::DecodeQuestSaveMetadata(pair.metadata, "", ui) && ui.pose == wanted.pose &&
                ui.mapName == wanted.mapName && ui.dialogueOffsets == wanted.dialogueOffsets &&
                ui.personaLogs == wanted.personaLogs && ui.mapLocalPose == wanted.mapLocalPose,
                "Paired UI metadata identity was not restored");
    };
    verifyMetadata(uiSecond, pairs.front());
    Require(QuestVr::WriteDurableSaveFile(loadStage.string(), pairs.front().runtime) &&
            LoadPortableRuntimeMap(combat).passed && LoadPortableRuntimeState(loadStage.string()),
            "Actual second-generation paired runtime restoration failed");
    RequireSameLiveState(expectedSecond);
    // Even a CRC-valid newest bundle can hold semantically invalid runtime
    // bytes. Preflight before launching a cross-map worker must reject it and
    // permit the prior candidate without mutating the current Combat runtime.
    const auto preflightPaths = QuestVr::MakeSaveBundlePaths((directory.path / "quest-save-preflight").string());
    Require(QuestVr::PublishSaveBundle(preflightPaths, firstMetadata, firstRuntime).saved &&
            QuestVr::PublishSaveBundle(preflightPaths, secondMetadata, forgedDamagedPath).saved,
            "CRC-valid malformed-runtime bundle fixture failed");
    const auto preflightPairs = QuestVr::LoadSaveBundleCandidates(preflightPaths);
    Require(preflightPairs.size() == 2u, "Malformed-runtime fixture did not pass structural bundle validation");
    std::uint64_t selectedGeneration{};
    for (const auto& pair : preflightPairs) {
        Require(QuestVr::WriteDurableSaveFile(loadStage.string(), pair.runtime), "Runtime preflight staging failed");
        if (ValidatePortableRuntimeState(loadStage.string())) {
            selectedGeneration = pair.generation;
            break;
        }
        RequireSameLiveState(expectedSecond);
    }
    Require(selectedGeneration == 1u, "Runtime preflight did not reject CRC-valid malformed latest save");
    RequireSameLiveState(expectedSecond);
    const auto& newestSlot = pairs.front().slotIndex == 0u ? bundlePaths.slot0 : bundlePaths.slot1;
    std::vector<std::uint8_t> corruptLatest;
    Require(QuestVr::ReadBoundedSaveFile(newestSlot, QuestVr::kMaximumSaveBundleBytes, corruptLatest) &&
            corruptLatest.size() > QuestVr::kSaveBundleHeaderBytes, "Latest bundle corruption fixture capture failed");
    corruptLatest.pop_back();
    Require(QuestVr::WriteDurableSaveFile(newestSlot, corruptLatest), "Latest bundle corruption fixture failed");
    pairs = QuestVr::LoadSaveBundleCandidates(bundlePaths);
    Require(pairs.size() == 1u && pairs.front().generation == 1u, "Paired save did not recover prior complete generation");
    verifyMetadata(uiFirst, pairs.front());
    Require(QuestVr::WriteDurableSaveFile(loadStage.string(), pairs.front().runtime) &&
            LoadPortableRuntimeMap(training).passed && LoadPortableRuntimeState(loadStage.string()),
            "Corrupt-latest fallback did not restore the original runtime map/state");
    RequireSameLiveState(expected);
    Require(SavePortableRuntimeState(afterRollback.string()) && ReadGeneratedCheckpoint(afterRollback) == saved,
            "Paired fallback changed hidden runtime flag/actor health state");
    std::cout << "Actual runtime paired-save generation 1 Training / generation 2 Combat matched UI pose/map/cursors/logs "
              << "and runtime payloads; corrupt latest recovered generation 1 and original live/hidden state.\n";

    // Model the Quest worker's asset-before-inactivity ordering with original
    // actors and materials. Checkpoints stay in this test's isolated directory.
    {
        Require(LoadPortableRuntimeMap(training).passed, "Asset-retention full Training map load failed");
        const auto completeActors = GetPortableRuntimeMapActors();
        const auto eligible = [](const PortableActorSnapshot& actor) {
            return actor.inventory && !actor.hidden && !actor.meshPath.empty();
        };
        const auto meshUsers = [&](const std::string& meshPath) {
            return std::count_if(completeActors.begin(), completeActors.end(),
                [&](const auto& actor) { return actor.meshPath == meshPath; });
        };
        // Prefer a unique mesh so this fixture exercises a resource that no
        // other visible actor could incidentally keep in the texture array.
        auto chosen = std::find_if(completeActors.begin(), completeActors.end(),
            [&](const auto& actor) { return eligible(actor) && meshUsers(actor.meshPath) == 1; });
        if (chosen == completeActors.end())
            chosen = std::find_if(completeActors.begin(), completeActors.end(), eligible);
        Require(chosen != completeActors.end(), "Training has no visible inventory mesh for asset-retention test");
        const PortableActorSnapshot retainedItem = *chosen;
        Require(DecodePortableRuntimeActorMeshes().passed, "Asset-retention initial full mesh decode failed");
        const auto completeTextures = BuildPortableRuntimeActorTextureArray(32u, 32u);
        Require(completeTextures.passed, "Asset-retention initial full texture array failed");
        const auto completeMesh = GetPortableRuntimeMesh(retainedItem.meshPath);
        Require(!completeMesh.triangles.empty(), "Asset-retention item mesh has no geometry");
        std::vector<std::string> materialPaths;
        for (const auto& vertex : completeMesh.triangles) {
            std::string texture = retainedItem.texturePath;
            std::int32_t index = static_cast<std::int32_t>(vertex.material);
            if (vertex.material < completeMesh.materialTextureIndices.size())
                index = completeMesh.materialTextureIndices[vertex.material];
            if (texture.empty() && index >= 0 && static_cast<std::size_t>(index) < completeMesh.texturePaths.size())
                texture = completeMesh.texturePaths[static_cast<std::size_t>(index)];
            if (!texture.empty()) materialPaths.push_back(std::move(texture));
        }
        std::sort(materialPaths.begin(), materialPaths.end());
        materialPaths.erase(std::unique(materialPaths.begin(), materialPaths.end()), materialPaths.end());
        Require(!materialPaths.empty(), "Asset-retention item has no resolved material path");
        const auto coversMaterials = [&](const PortableTextureArray& textures) {
            return std::all_of(materialPaths.begin(), materialPaths.end(), [&](const auto& material) {
                return std::find(textures.texturePaths.begin(), textures.texturePaths.end(), material) !=
                    textures.texturePaths.end();
            });
        };
        Require(coversMaterials(completeTextures), "Initial full texture array lacks item materials");
        const auto visibleCheckpoint = directory.path / "asset-retention-visible.runtime.tmp";
        const auto hiddenCheckpoint = directory.path / "asset-retention-hidden.runtime.tmp";
        Require(SavePortableRuntimeState(visibleCheckpoint.string()), "Asset-retention visible checkpoint failed");
        const auto pickup = InteractPortableRuntimeActor(retainedItem.objectPath);
        Require(pickup.handled && pickup.worldChanged && pickup.action == "pickup" &&
                SavePortableRuntimeState(hiddenCheckpoint.string()), "Asset-retention pickup/hidden checkpoint failed");
        Require(LoadPortableRuntimeMap(combat).passed && LoadPortableRuntimeMap(training).passed,
                "Asset-retention Combat -> Training replacement failed");
        Require(DecodePortableRuntimeActorMeshes().passed, "Asset-retention replacement full mesh decode failed");
        const auto retainedTextures = BuildPortableRuntimeActorTextureArray(32u, 32u);
        Require(retainedTextures.passed && coversMaterials(retainedTextures),
                "Replacement full texture array lacks item materials before inactivity restoration");
        Require(retainedTextures.texturePaths == completeTextures.texturePaths &&
                retainedTextures.rgba.size() == retainedTextures.texturePaths.size() * 32u * 32u * 4u,
                "Asset-retention replacement changed complete authored texture coverage");
        Require(LoadPortableRuntimeState(hiddenCheckpoint.string()), "Asset-retention hidden restoration failed");
        const auto hiddenActors = GetPortableRuntimeMapActors();
        Require(std::none_of(hiddenActors.begin(), hiddenActors.end(), [&](const auto& actor) {
                    return actor.objectPath == retainedItem.objectPath;
                }), "Saved inactive item remained visible after hidden restoration");
        Require(GetPortableRuntimeMesh(retainedItem.meshPath).triangles.size() == completeMesh.triangles.size(),
                "Saved inactivity discarded the retained item geometry");
        // No map-wide mesh decode or texture rebuild occurs during this older
        // same-map restore: the already-prepared resources must be sufficient.
        Require(LoadPortableRuntimeState(visibleCheckpoint.string()), "Asset-retention older visible restoration failed");
        const auto visibleActors = GetPortableRuntimeMapActors();
        const auto restoredItem = std::find_if(visibleActors.begin(), visibleActors.end(), [&](const auto& actor) {
            return actor.objectPath == retainedItem.objectPath;
        });
        Require(restoredItem != visibleActors.end() && ActorFields(*restoredItem) == ActorFields(retainedItem),
                "Older same-map save did not reactivate the original item identity/properties");
        const auto restoredMesh = GetPortableRuntimeMesh(retainedItem.meshPath);
        Require(restoredMesh.triangles.size() == completeMesh.triangles.size() &&
                restoredMesh.texturePaths == completeMesh.texturePaths &&
                restoredMesh.materialTextureIndices == completeMesh.materialTextureIndices && coversMaterials(retainedTextures),
                "Reactivated item lost retained geometry or material-layer coverage");
        std::cout << "Original actor asset retention passed: " << retainedItem.objectPath
                  << ", mesh users=" << meshUsers(retainedItem.meshPath)
                  << ", vertices=" << restoredMesh.triangles.size()
                  << ", used materials=" << materialPaths.size()
                  << ", complete texture layers=" << retainedTextures.texturePaths.size()
                  << " (decoded=" << retainedTextures.decodedTextures
                  << ", fallbacks=" << retainedTextures.failedTextures
                  << "); hidden saved-map -> older same-map visible restore used retained resources.\n";
        Require(LoadPortableRuntimeState(checkpoint.string()), "Asset-retention final original checkpoint reset failed");
        RequireSameLiveState(expected);
        Require(SavePortableRuntimeState(afterRollback.string()) && ReadGeneratedCheckpoint(afterRollback) == saved,
                "Asset-retention fixture failed to restore original hidden state/progress");
    }

    std::cout << "Original-package rollback primitives passed: " << runtime.objects << " runtime objects; "
              << actors.size() << " Training actors; Training -> Combat -> Training; inactive pickup, "
              << "activated mover, pawn/player health, ordered inventory, credits/skill points/goals/notes/flag, "
              << "effect deduplication, missing/truncated checkpoint guards.\n"
              << "Progress effect inputs were synthetic test fixtures. This does not verify full campaign scripts, "
              << "OpenXR, GPU staging, or on-device transition performance.\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cout << "SKIPPED real-data rollback verification: opt in with --game-root <owned Deus Ex folder>.\n";
        return 77;
    }
    try {
        Require(argc == 3 && std::string(argv[1]) == "--game-root",
                "Usage: portable_runtime_state_test --game-root <owned Deus Ex folder>");
        TestOriginalRollback(argv[2]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Portable runtime state test failed: " << error.what() << '\n';
        return 1;
    }
}
