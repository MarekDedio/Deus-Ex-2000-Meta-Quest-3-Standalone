#pragma once

#include "surreal_portable_package_tables.h"
#include "quest_actor_materials.h"
#include "quest_portable_vm.h"
#include "quest_script_dispatch.h"
#include "quest_state_frame.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <string>
#include <set>
#include <optional>

struct PortableRuntimeSummary {
    bool passed{};
    std::size_t objects{};
    std::size_t classes{};
    std::size_t functions{};
    std::size_t states{};
    std::size_t normalizedStateBytecodeBytes{};
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

struct PortablePlayerProgress {
    std::int32_t credits{};
    std::int32_t skillPoints{};
    std::vector<std::string> goals;
    std::vector<std::string> notes;
};

struct PortableMapRuntimeSummary {
    bool passed{};
    std::size_t exports{};
    std::size_t actors{};
    std::size_t actorProperties{};
    std::size_t serializedActorStacks{};
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
    std::vector<std::uint32_t> texturePolyFlags; // Same layer order as texturePaths.
    std::vector<std::int32_t> maskedTextureLayers; // -1 or separately premultiplied P8 masked variant.
    std::size_t maskedTextureVariants{};
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
// A stable seed1 baseline is the portable embedding/test default. Quest passes
// time(nullptr) once at engine initialization, matching the original Core policy.
// Nonbaseline seeds are saved immediately, including before the first draw.
PortableRuntimeSummary InitializePortableRuntime(
    const PortablePackageTables& package, std::uint32_t initialRandomSeed = 1u);
PortableRuntimeSummary InitializePortableRuntime(
    const std::vector<PortablePackageTables>& packages, std::uint32_t initialRandomSeed = 1u);
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

// These are authored/current property snapshots, not a guessed idle state or
// a replacement for UnrealScript startup, animation notifies, or state ticks.
// A negative frame requires runtime tween history that original map properties
// do not provide; callers must diagnose that state instead of inventing it.
struct PortableActorBlendAnimationSnapshot {
    std::string sequence;
    float frame{};
    float rate{};
    float last{};
    float minRate{};
    float tweenRate{};
    float oldRate{};
    // Engine.Actor has no serialized blend-loop flag. Keep this unavailable
    // state false; a future actual native animation command can supply it.
    bool loop{};
    QuestVr::MeshTweenHistory previous;
};

struct PortableActorAnimationSnapshot {
    std::string sequence;
    float frame{};
    float rate{};
    float last{};
    float minRate{};
    float tweenRate{};
    float oldRate{};
    bool loop{};
    bool notify{};
    bool finished{};
    QuestVr::MeshTweenHistory previous;
    std::array<PortableActorBlendAnimationSnapshot, 4u> blends;
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
    bool light{}; // Engine.Light-derived or any actor with non-LT_None LightType.
    bool hidden{};
    bool activated{};
    std::uint8_t drawType{};
    std::uint8_t style{1u};
    bool unlit{};
    bool noSmooth{};
    bool meshEnvironmentMap{};
    std::string destinationMap;
    float drawScale{1.0f};
    float drawScaleX{1.0f};
    float drawScaleY{1.0f};
    float drawScaleZ{1.0f};
    float prePivotX{}, prePivotY{}, prePivotZ{};
    float mainScaleX{1.0f}, mainScaleY{1.0f}, mainScaleZ{1.0f};
    std::int32_t pitch{};
    std::int32_t yaw{};
    std::int32_t roll{};
    std::string meshPath;
    std::string meshClassPath;
    PortableActorAnimationSnapshot animation;
    bool animByOwner{};
    std::string ownerPath;
    std::string animationSourcePath;
    // Actor.Fatness applies to the rendered actor even when its animation
    // properties come from its owner. The UE1 neutral default is 128.
    std::uint8_t fatness{128u};
    std::string brushPath;
    std::string texturePath;
    QuestVr::ActorTextureOverrides materialOverrides;
    std::string ambientSoundPath;
    std::uint8_t soundRadius{64u};
    std::uint8_t soundVolume{255u};
    std::uint8_t soundPitch{64u};
    std::uint8_t lightType{}; // UE1 LT_None=0, LT_Steady=1.
    std::uint8_t lightEffect{}; // UE1 LE_StaticSpot=8, LE_Spotlight=12.
    std::uint8_t lightBrightness{64u};
    std::uint8_t lightHue{};
    std::uint8_t lightSaturation{255u};
    std::uint8_t lightRadius{64u};
    std::uint8_t lightCone{128u};
    // ZoneInfo defaults are inherited too; an absent zone uses LevelInfo.
    std::uint8_t ambientHue{}, ambientSaturation{255u}, ambientBrightness{};
};

PortableVmValue ExecutePortableFunction(const std::string& objectPath);
// Executes actual compiled member bytecode on an explicit indexed actor. A
// Destroyed actor retains its UObject identity until map teardown. No
// automatic BeginPlay/AI/idle invocation occurs. Unsupported operations roll
// back every property and native-animation change in the nested call tree.
QuestVr::Vm::Result ExecutePortableActorFunction(
    const std::string& actorPath, const std::string& functionName,
    const std::vector<QuestVr::Vm::Evaluation>& arguments = {},
    const QuestVr::Vm::Limits& limits = {});
// Changes only after committed actor writes/map replacement/checkpoint load,
// never for a rolled-back callback. Consumers must not read during map workers.
std::uint64_t GetPortableRuntimeWorldRevision();
QuestVr::Vm::Value ReadPortableActorScriptProperty(
    const std::string& actorPath, const std::string& propertyName,
    std::uint32_t arrayIndex = 0u);
// Read-only concrete loaded Actor UClass default block. Later CDO writes do
// not change already-loaded instances or derived/sibling default blocks.
QuestVr::Vm::Value ReadPortableClassDefault(
    const std::string& classPath, const std::string& propertyName,
    std::uint32_t arrayIndex = 0u);
// Read-only contiguous fixed-array slots using one property resolution. Count
// must be 1..1024, and the complete range must fit the actual declaration.
std::vector<QuestVr::Vm::Value> ReadPortableActorScriptPropertySlots(
    const std::string& actorPath, const std::string& propertyName,
    std::uint32_t firstIndex, std::uint32_t count);
// Read-only original object/class cast inspection using an indexed declaring
// object's source table. The normalized reference belongs to that table, not
// the operand's package. It does not execute bytecode or enter actor lifecycle.
QuestVr::Vm::Value CastPortableRuntimeObject(const std::string& declaringObjectPath,
    std::int32_t targetReference, const QuestVr::Vm::Value& value, bool meta);
// Read-only authored metadata, not an active AI frame or a resumable state.
// Requires the indexed vector-package initialization path used by the game,
// not the single-package lifecycle-verification helper. Descriptors are loaded
// from script packages; map loading retains Actor stack records only.
// Stack references retain their actor source package's original table indices;
// descriptor references likewise belong to the descriptor's source package.
std::optional<PortableObjectStack> ReadPortableActorSerializedStack(
    const std::string& actorPath);
PortableStateDescriptor ReadPortableRuntimeAuthoredStateDescriptor(
    const std::string& objectPath);
// Effective authored-dormant or explicit portable state context. Runnable raw
// HasStack continuations remain unsupported; no automatic callback caller.
struct PortableActorDispatchContext {
    std::string codePath, stateName{"None"};
    std::uint64_t classProbeMask{};
    std::optional<QuestVr::ScriptDispatch::CodeMasks> codeMasks;
    std::set<std::string> disabledNames;
};
struct PortableScriptDispatchSummary {
    std::size_t classes{}, states{}, classFunctions{}, stateFunctions{}, commonFields{};
};
PortableScriptDispatchSummary ReadPortableRuntimeDispatchSummary();
PortableActorDispatchContext ReadPortableActorDispatchContext(const std::string& actorPath);
std::optional<QuestVr::StateObject> ReadPortableActorStateObject(const std::string& actorPath);
// Explicit bounded state slice, not a world tick or automatic startup phase.
QuestVr::Vm::Result ResumePortableActorState(const std::string& actorPath,
    const QuestVr::Vm::Limits& limits = {});
// Explicit original Actor.ProcessState poll + continuation, not a full actor
// Tick: no animation, Tick event, Timer, physics or automatic world startup.
QuestVr::Vm::Result AdvancePortableActorState(const std::string& actorPath, float elapsed,
    const QuestVr::Vm::Limits& limits = {});
std::optional<float> ReadPortableActorLatentTimeLeft(const std::string& actorPath);
std::optional<std::string> ResolvePortableActorState(const std::string& actorPath,
    const std::string& stateName);
std::optional<std::string> ResolvePortableActorFunction(const std::string& actorPath,
    const std::string& stateName, const std::string& functionName,
    QuestVr::ScriptDispatch::LookupKind kind = QuestVr::ScriptDispatch::LookupKind::Virtual);
QuestVr::Vm::ProgramLayout ReadPortableRuntimeStateProgram(const std::string& objectPath,
    const QuestVr::Vm::Limits& limits = {});
// Only explicit enum dispatch grants Destroyed's bDeleteMe exception.
QuestVr::Vm::Result ExecutePortableActorEvent(const std::string& actorPath,
    const std::string& eventName, bool enumDispatch = false,
    const std::vector<QuestVr::Vm::Evaluation>& arguments = {},
    const QuestVr::Vm::Limits& limits = {});
// v4 preserves supported actor overlays/clocks; v5 adds portable state frames,
// local storage and state-keyed disabled sets; v6 adds concrete-class defaults.
// This
// predicate means state is present, not that a successful save has cleared it.
// Envelope10/codec7 additionally retains the engine-global random stream.
// Map-scoped actor/default/AI state still requires an archive before replacement.
// Engine-global random state survives ordinary map changes and does not by
// itself block them. Legacy v1-v3 load explicitly restores authored properties,
// and every v1-v5 load clears mutable class defaults;
// every v1-v9 load resets absent random state to the deterministic seed1 baseline.
// ShutdownPortableRuntime explicitly discards the runtime's object lifetime.
bool GetPortableRuntimeScriptStatePresent();
// Map-discard guard (historical name), not an indicator that saving cleared
// dirty state. Excludes the engine-global RNG that survives map replacement.
bool GetPortableRuntimeUnsavedScriptState();
PortableMapRuntimeSummary LoadPortableRuntimeMap(
    const PortablePackageTables& package);
std::size_t UnloadPortableRuntimeMap();
// Renderer lists normally exclude picked-up/destroyed actors. Asset preparation
// includes them so a later quickload can reactivate their original materials.
std::vector<PortableActorSnapshot> GetPortableRuntimeMapActors(bool includeInactive = false);
PortableActorMeshSummary DecodePortableRuntimeActorMeshes();
PortableLodMesh GetPortableRuntimeMesh(const std::string& meshPath);
PortableLodMesh GetPortableRuntimeBrush(const std::string& brushPath);
PortableTextureArray BuildPortableRuntimeActorTextureArray(
    std::uint32_t width,
    std::uint32_t height);
PortableInteractionResult InteractPortableRuntimeActor(const std::string& objectPath);
bool VerifyPortableRuntimeInteraction();
bool SavePortableRuntimeState(const std::string& path);
// Optional binding protects paired metadata from referring to another v4/v5 map.
// Legacy v1-v3 have no embedded map identity and retain their existing behavior.
bool ValidatePortableRuntimeState(const std::string& path, const std::string& expectedMapName = {});
bool LoadPortableRuntimeState(const std::string& path);
PortableDamageResult DamagePortableRuntimeActor(
    const std::string& objectPath,
    float damage);
bool VerifyPortableRuntimeDamage();
std::size_t GetPortableRuntimeInventoryCount();
std::vector<std::string> GetPortableRuntimeInventoryItems();
enum class PortableInventoryDescriptorStatus : std::uint8_t {
    Available, InvalidIdentity, RuntimeUnavailable, UnindexedIdentity,
    NotInventoryActor, MissingPropertyMetadata, MalformedProperty, InvalidLayout,
    InvalidIconDimensions, MissingIconMetadata
};
enum class PortableInventoryValueOrigin : std::uint8_t { Zero, ClassDefault, Instance, Overlay };
struct PortableInventoryIconProvenance {
    PortableInventoryValueOrigin origin{PortableInventoryValueOrigin::Zero};
    std::string declarationPath, declarationSourcePath, ownerPath, ownerSourcePath;
};
struct PortableInventoryDescriptor {
    PortableInventoryDescriptorStatus status{PortableInventoryDescriptorStatus::RuntimeUnavailable};
    std::string requestedPath, actorPath, classPath, actorSourcePath, classSourcePath;
    bool active{};
    std::int32_t invSlotsX{}, invSlotsY{}, invPosX{}, invPosY{};
    bool bDisplayableInv{}, positionAssigned{};
    std::string largeIconPath, fallbackIconPath, iconPath;
    std::int32_t largeIconWidth{}, largeIconHeight{}, displayWidth{}, displayHeight{};
    bool usesLargeIcon{};
    PortableInventoryIconProvenance largeIconProvenance, fallbackIconProvenance;
};
struct PortableInventoryDescriptorLimits {
    std::size_t count{1024u}, retainedBytes{4u * 1024u * 1024u}, stringBytes{4096u}, hierarchy{128u};
};
// One detached, ordered descriptor per explicit input identity, including
// inactive/picked-up indexed Engine.Inventory instances. No class@event string
// interpretation, actor allocation, bytecode, asset/file reads, state/clock/index
// writes or GC collection. The generic live-actor property API stays unchanged.
// Effective values use typed indexed declarations, actor overlay, instance tag,
// then actual class defaults/typed zero; unavailable rows do not hide valid rows.
// For displayable items, slot/assigned-position validation is against the actual
// original 5x6 grid. Values are retained even when layout validation fails.
// A non-null largeIcon uses its exact authored logical window dimensions; only
// absent largeIcon permits Icon fallback, with original UI constants 40x35.
// Icon ownerSourcePath describes its indexed owner, not a dynamic write trace.
// Non-null icons require an indexed actual object and validated Texture class
// metadata; missing asset metadata is unavailable, never a guessed Texture.
// Narrow immutable class-reference metadata is cached at package/map load.
// Count/aggregate limits reject the whole request before returning any results;
// oversized/invalid individual identities are marked without retaining them.
std::vector<PortableInventoryDescriptor> ReadPortableRuntimeInventoryDescriptors(
    const std::vector<std::string>& actorPaths,
    const PortableInventoryDescriptorLimits& limits = {});
bool ConsumePortableRuntimeInventoryItem(const std::string& objectPath);
PortablePlayerProgress GetPortableRuntimePlayerProgress();
float GetPortableRuntimePlayerHealth();
float DamagePortableRuntimePlayer(float damage);
float HealPortableRuntimePlayer(float amount);
