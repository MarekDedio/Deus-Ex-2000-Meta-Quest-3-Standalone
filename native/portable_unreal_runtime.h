#pragma once

#include "surreal_portable_package_tables.h"

#include <cstddef>
#include <cstdint>
#include <string>

struct PortableRuntimeSummary {
    bool passed{};
    std::size_t objects{};
    std::size_t classes{};
    std::size_t functions{};
    std::size_t properties{};
    std::size_t resolvedLinks{};
    std::size_t unresolvedExternalLinks{};
    std::size_t normalizedBytecodeBytes{};
    std::size_t serializedClassDefaults{};
    std::size_t classDefaultProperties{};
    std::size_t conversationObjects{};
    std::size_t conversationProperties{};
    std::size_t conversationLoadFailures{};
    std::size_t peakGcObjects{};
    std::size_t destroyedObjects{};
};

struct PortableConversationSummary {
    std::size_t objects{};
    std::size_t conversations{};
    std::size_t events{};
    std::size_t speechObjects{};
    std::size_t speechLines{};
    std::string sampleSpeech;
};

struct PortableDialogueResult {
    bool found{};
    std::string actorPath;
    std::string bindName;
    std::string eventPath;
    std::string speech;
    std::string audioPackageName;
    std::string missionCandidates;
    std::int32_t soundId{-1};
    std::size_t matchingLines{};
    bool invokeFrob{};
    struct Effect {
        enum class Type : std::uint8_t {
            SetFlag, AddGoal, AddNote, AddSkillPoints, AddCredits, Trigger,
            TransferObject
        };
        Type type{Type::SetFlag};
        std::string eventPath;
        std::string key;
        std::string text;
        std::string source;
        std::string target;
        std::int32_t amount{};
        bool value{};
        bool completed{};
        bool primary{};
    };
    struct Choice {
        std::string objectPath;
        std::string text;
        std::string label;
        std::string targetEventPath;
        std::string flagName;
        std::string skillClassPath;
        std::int32_t soundId{-1};
        std::int32_t skillLevelNeeded{};
        std::size_t targetOrdinal{static_cast<std::size_t>(-1)};
        bool displayAsSpeech{};
        bool conditional{};
        bool requiredFlagValue{};
        bool available{true};
    };
    std::vector<Effect> effects;
    std::vector<Choice> choices;
};

struct PortableDialogueEffectResult {
    std::size_t applied{};
    std::int32_t credits{};
    std::int32_t skillPoints{};
    std::size_t goals{};
    std::size_t notes{};
    std::size_t inventoryCount{};
    std::string status;
};

struct PortableMapRuntimeSummary {
    bool passed{};
    std::size_t exports{};
    std::size_t actors{};
    std::size_t actorProperties{};
    std::size_t resolvedClasses{};
    std::size_t unresolvedClasses{};
    std::size_t replacedExports{};
};

struct PortableActorMeshSummary {
    bool passed{};
    std::size_t referencedMeshes{};
    std::size_t decodedMeshes{};
    std::size_t triangleVertices{};
    std::size_t referencedBrushes{};
    std::size_t decodedBrushes{};
    std::size_t brushTriangleVertices{};
};

struct PortableTextureArray {
    bool passed{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::size_t decodedTextures{};
    std::size_t failedTextures{};
    std::vector<std::string> texturePaths;
    std::vector<std::uint8_t> rgba;
};

struct PortableInteractionResult {
    bool handled{};
    bool worldChanged{};
    std::string action;
    std::string objectPath;
    std::string classPath;
    std::string destinationMap;
    std::size_t inventoryCount{};
};

struct PortableDamageResult {
    bool handled{};
    bool worldChanged{};
    bool killed{};
    float remainingHealth{};
    std::string objectPath;
};

PortableRuntimeSummary BuildAndVerifyPortableRuntime(
    const PortablePackageTables& package);
PortableRuntimeSummary InitializePortableRuntime(
    const PortablePackageTables& package);
PortableRuntimeSummary InitializePortableRuntime(
    const std::vector<PortablePackageTables>& packages);
void ShutdownPortableRuntime();
PortableConversationSummary GetPortableConversationSummary();
PortableDialogueResult GetPortableRuntimeDialogue(
    const std::string& actorPath,
    std::size_t ordinal,
    std::int32_t missionNumber);
PortableSound LoadPortableRuntimeDialogueSound(const PortableDialogueResult& dialogue);
PortableSound LoadPortableRuntimeSound(const std::string& objectPath);
PortableDialogueEffectResult ApplyPortableDialogueEffects(
    const PortableDialogueResult& dialogue);

enum class PortableVmValueType {
    Nothing,
    Integer,
    Float,
    Boolean,
    String,
    ObjectReference,
    NameReference,
};

struct PortableVmValue {
    PortableVmValueType type{PortableVmValueType::Nothing};
    std::int32_t integer{};
    float floating{};
    bool boolean{};
    std::string string;
};

struct PortableActorSnapshot {
    std::string objectPath;
    std::string classPath;
    float x{};
    float y{};
    float z{};
    bool hasLocation{};
    bool pawn{};
    bool inventory{};
    bool decoration{};
    bool mover{};
    bool trigger{};
    bool travel{};
    bool light{};
    bool hidden{};
    std::uint8_t drawType{};
    std::string destinationMap;
    float drawScale{1.0f};
    float drawScaleX{1.0f};
    float drawScaleY{1.0f};
    float drawScaleZ{1.0f};
    std::int32_t pitch{};
    std::int32_t yaw{};
    std::int32_t roll{};
    std::string meshPath;
    std::string meshClassPath;
    std::string brushPath;
    std::string texturePath;
    std::string ambientSoundPath;
    std::uint8_t soundRadius{64u};
    std::uint8_t soundVolume{255u};
    std::uint8_t soundPitch{64u};
    std::uint8_t lightBrightness{64u};
    std::uint8_t lightHue{};
    std::uint8_t lightSaturation{255u};
    std::uint8_t lightRadius{64u};
    std::uint8_t lightCone{128u};
};

PortableVmValue ExecutePortableFunction(const std::string& objectPath);
PortableMapRuntimeSummary LoadPortableRuntimeMap(
    const PortablePackageTables& package);
std::size_t UnloadPortableRuntimeMap();
std::vector<PortableActorSnapshot> GetPortableRuntimeMapActors();
PortableActorMeshSummary DecodePortableRuntimeActorMeshes();
PortableLodMesh GetPortableRuntimeMesh(const std::string& meshPath);
PortableLodMesh GetPortableRuntimeBrush(const std::string& brushPath);
PortableTextureArray BuildPortableRuntimeActorTextureArray(
    std::uint32_t width,
    std::uint32_t height);
PortableInteractionResult InteractPortableRuntimeActor(const std::string& objectPath);
bool VerifyPortableRuntimeInteraction();
bool SavePortableRuntimeState(const std::string& path);
bool LoadPortableRuntimeState(const std::string& path);
PortableDamageResult DamagePortableRuntimeActor(
    const std::string& objectPath,
    float damage);
bool VerifyPortableRuntimeDamage();
std::size_t GetPortableRuntimeInventoryCount();
std::vector<std::string> GetPortableRuntimeInventoryItems();
bool ConsumePortableRuntimeInventoryItem(const std::string& objectPath);
float GetPortableRuntimePlayerHealth();
float DamagePortableRuntimePlayer(float damage);
float HealPortableRuntimePlayer(float amount);
