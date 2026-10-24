#pragma once

#include "surreal_portable_package_tables.h"
#include "quest_actor_materials.h"
#include "quest_portable_vm.h"
#include "quest_script_dispatch.h"
#include "quest_state_frame.h"
#include "quest_native_client.h"
#include "quest_held_item_visual.h"
#include "quest_linker_load_context.h"

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
    std::size_t contextExcludedExports{};
};

// Rollback-only native payload diagnostic. Values are captured inside the
// genuine caller root (including partial refusal prefixes), never published.
// This is not full ConditionalDestroy/linker/deferred deletion or gameplay.
struct PortablePropertyTeardownEntry {
    std::string propertyPath;std::uint32_t arrayIndex{};
    QuestVr::Vm::Value before,after;
};
struct PortablePropertyTeardownProbeResult {
    QuestVr::Vm::Result execution;
    bool completed{};
    std::vector<PortablePropertyTeardownEntry> values;
};
PortablePropertyTeardownProbeResult ProbePortableActorPropertyTeardown(
    const std::string& actorPath,const QuestVr::Vm::Limits& limits={});

// Loader-owned live attachment, separate from immutable file provenance and
// UObject identity. Opaque linker identities are session-only, not save data.
struct PortableObjectLinkerSnapshot {
    bool bound{},attached{},sourceExport{};
    std::uintptr_t linkerIdentity{},sourceLinkerIdentity{};
    std::int32_t nativeIndex{-1};
    std::string sourcePath;
    std::size_t sourceExportIndex{};
    std::string exportObjectPath;
    std::uint32_t objectIndex{},outerIndex{},flags{};
    bool operator==(const PortableObjectLinkerSnapshot&)const=default;
};
PortableObjectLinkerSnapshot ReadPortableRuntimeObjectLinker(const std::string& objectPath);
struct PortableObjectDestroyProbeResult {
    QuestVr::Vm::Result execution;
    bool completed{};
    std::int32_t returned{},repeated{};
    PortableObjectLinkerSnapshot before,after;
};
// Real native ConditionalDestroy, repeated inside one rollback-only root.
// Does not publish actor deletion or substitute for deferred Level cleanup.
PortableObjectDestroyProbeResult ProbePortableActorConditionalDestroy(
    const std::string& actorPath,const QuestVr::Vm::Limits& limits={});

struct PortableDeferredOwnershipSnapshot {
    bool available{};
    std::string levelPath,firstDeletedPath;
    std::vector<std::pair<std::string,std::string>> actorLinks;
    bool operator==(const PortableDeferredOwnershipSnapshot&)const=default;
};
PortableDeferredOwnershipSnapshot ReadPortableRuntimeDeferredOwnership();
struct PortableActorDestroyProbeResult {
    QuestVr::Vm::Result execution;
    bool completed{},captureRefused{},restoreRefused{},travelRefused{};
    std::vector<std::int32_t> returned;
    PortableDeferredOwnershipSnapshot before,after;
};
// Actual full native DestroyActor in a single rollback-only caller root.
// Original dormant events stay dormant; no begun/startup flags are fabricated.
PortableActorDestroyProbeResult ProbePortableActorNativeDestroy(
    const std::vector<std::string>& actorPaths,const QuestVr::Vm::Limits& limits={});

// Original qualified AActor::performPhysics BASE method, not APawn virtual
// dispatch, in a caller-owned rollback-only root.
// No invented script native index, world scheduler or partial startup commit.
struct PortableActorPhysicsObservation {
    std::string actorPath;
    bool entered{},returned{};
    std::size_t fallingCalls{},rotationCalls{},rotationMoveCalls{},endedRotationCalls{},postTouchCalls{},pendingWrites{};
    std::vector<std::pair<std::string,std::string>> pendingBefore,pendingAfter;
};
struct PortableActorPhysicsProbeResult {
    QuestVr::Vm::Result execution;
    std::vector<PortableActorPhysicsObservation> actors;
};
PortableActorPhysicsProbeResult ProbePortableActorNativePhysics(
    const std::vector<std::string>& actorPaths,float deltaSeconds,const QuestVr::Vm::Limits& limits={});

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

// Exact named, single-speech -> End authored datalinks. Complex conversation
// graphs remain unsupported rather than being flattened into a different line.
PortableDialogueResult GetPortableRuntimeDataLink(const std::string& name, std::int32_t missionNumber = 0);
bool GetPortableRuntimeTouchDataLink(float x, float y, float z, float radius,
    float height, PortableDialogueResult& dialogue);

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
    std::size_t contextExcludedExports{};
};

enum class PortableWorldStartupPhase : std::uint8_t {
    ValidateWorld, SelectGameInfo, ResetTime, RestoreTime, ClearTouching,
    InitializeExecution, PublishStartup, InitGame, PreBeginPlay, BeginPlay,
    SetActorZone, LastRenderTime, PostBeginPlay, SetInitialState, WorldBases,
    ClearStartup, PostPostBeginPlay, CompactActorSlots, ClientSetup, Count
};
struct PortablePlayerLoginSource {
    // Decoded original FURL portal/options, preserving native option order.
    // Not a new URL parser or permission to synthesize a PlayerPawn.
    std::u16string portal;
    std::vector<std::u16string> urlOptions;
    // Explicit owned-file alias resolution; absence is a failed alias load,
    // never a hardcoded replacement class. Loading still checks the real UClass.
    std::optional<std::u16string> userDefaultPlayerClass,iniUrlClass;
};
// Optional diagnostic commands inside genuine startup, AFTER original Login
// returns a real Pawn and BEFORE pairing/Possess. Not a live weapon bridge.
struct PortableStartupDamageCommand {
    std::string actorPath;
    std::int32_t damage{};
    std::optional<std::array<float,3u>> hitLocation;
    std::array<float,3u> momentum{};
    std::string damageType{"shot"};
};
// Explicit collision-to-landing diagnostic, NOT a gravity integration step.
// Only a real swept MoveActor floor hit can reach the landing adapter.
struct PortableStartupLandingCommand {
    std::string actorPath;
    std::array<float,3u> delta{};
};
struct PortableStartupLandingObservation {
    std::size_t commandIndex{},writesBefore{},writesAfter{};
    std::string actorPath,gameInfoPath,zoneBefore,zoneAfter,floorPath,selectedLanded,baseBefore,baseAfter;
    std::array<float,3u> locationBefore{},locationAfter{},velocityBefore{},velocityAfter{},normal{};
    std::uint8_t physicsBefore{},physicsAfter{};
    std::int32_t counterBefore{},counterAfter{};
    float hitTime{1.0f};
    bool entered{},moved{},landingEntered{},returned{},captured{},removed{},deleteMe{};
    std::size_t traceCalls{},counterWrites{},velocityWords{},landedCalls{},setPhysicsCalls{};
    bool recovery{},zoneBounce{};
    std::array<std::int32_t,4u> clear{};
    std::vector<std::uint8_t> clockBefore,clockAfter;
    bool operator==(const PortableStartupLandingObservation&) const = default;
};
// Explicit original physFalling diagnostics, not a world Tick/performPhysics
// scheduler or a way to commit a partially initialized player.
struct PortableStartupFallingCommand {
    std::string actorPath;
    float deltaSeconds{0.1f};
    std::uint8_t steps{1u};
    bool afterDamage{};
    bool performPhysics{};
};
struct PortableStartupFallingObservation : PortableStartupLandingObservation {
    std::string classPath,selectedHitWall,stateBefore,stateAfter;
    std::size_t stepIndex{},substeps{},moveCalls{},apexSplits{},oldLocationWords{},accelerationWords{},teleportWrites{};
    std::size_t hitWallCalls{};
    float deltaSeconds{};
    bool afterDamage{},teleportedBefore{},teleportedAfter{},hasLandingCounter{};
    bool bounceBefore{},bounceAfter{},stoppedByBounce{};
    bool performPhysics{};
    std::size_t physicsRotationCalls{},rotationMoveCalls{},endedRotationCalls{},postTouchCalls{},pendingWrites{};
    std::array<std::int32_t,3u> rotationBefore{},rotationAfter{},rotationRateBefore{},rotationRateAfter{};
    std::array<float,3u> accelerationBefore{},accelerationAfter{},oldLocationBefore{},oldLocationAfter{};
    bool operator==(const PortableStartupFallingObservation&) const = default;
};
struct PortableStartupDamageBirth {
    std::string actorPath,classPath,statePath,ownerPath,basePath;
    std::array<float,3u> location{},velocity{};
    std::int32_t physics{},remoteRole{};
    bool removed{},deleteMe{},pickup{};
    bool operator==(const PortableStartupDamageBirth&) const = default;
};
struct PortableStartupDamageObservation {
    std::size_t commandIndex{};
    std::size_t writesBefore{},writesAfter{};
    std::string actorPath,playerPath,gameInfoPath,functionPath,stateBefore,stateAfter,contentsBefore,baseBefore,baseAfter;
    std::int32_t hitPointsBefore{},hitPointsAfter{};
    std::uint8_t physicsBefore{},physicsAfter{};
    bool entered{},returned{},captured{},worldBegun{},worldStartup{},playerPaired{},removed{},deleteMe{};
    std::vector<PortableStartupDamageBirth> births;
    bool operator==(const PortableStartupDamageObservation&) const = default;
};
// Detached observations of genuine original Frob on actual surviving damage
// births. No fabricated grant, selected-player input, grid placement or audio
// acceptance is implied by this pre-pair rollback-only diagnostic.
struct PortableStartupLootItem {
    std::string actorPath,classPath,statePath,ownerPath,basePath,inventoryPath,instigatorPath;
    std::int32_t physics{},remoteRole{};
    std::optional<std::int32_t> numCopies,maxCopies;
    std::int32_t invPosX{},invPosY{},invSlotsX{},invSlotsY{};
    bool removed{},deleteMe{},heldItem{},hidden{},carriedItem{},onlyOwnerSee{},sleepTouch{};
    bool collideActors{},collideWorld{},blockActors{},blockPlayers{};
    bool operator==(const PortableStartupLootItem&) const = default;
};
struct PortableStartupLootGrid {
    std::int32_t rows{},columns{};
    std::vector<std::uint8_t> cells;
    bool operator==(const PortableStartupLootGrid&) const = default;
};
struct PortableStartupLootObservation {
    std::size_t commandIndex{},damageCommandIndex{},birthIndex{},writesBefore{},writesAfter{};
    std::string actorPath,playerPath,gameInfoPath,functionPath,playerStatePath;
    bool entered{},returned{},captured{},worldBegun{},worldStartup{},playerPaired{},isPawn{},isPlayer{};
    std::int32_t playerHealth{};
    PortableStartupLootItem before,after;
    std::vector<PortableStartupLootItem> inventoryBefore,inventoryAfter;
    bool playerRightClick{};
    std::string targetBefore,targetAfter;
    PortableStartupLootGrid gridBefore,gridAfter;
    std::vector<std::uint8_t> playerClockBefore,playerClockAfter;
    bool operator==(const PortableStartupLootObservation&) const = default;
};
struct PortableWorldStartupProbeOptions {
    // Already decoded URL GAME= class and the configured local client's
    // DefaultGame class. Empty URL selects the map's actual DefaultGameType
    // before the actual client/server configuration branch, never a guess.
    std::string urlGameClass;
    std::string defaultGameClass;
    std::string initGameOptions;
    // Used only without an actual native client (viewport count is irrelevant).
    // Omission refuses that missing configuration rather than using DefaultGame.
    std::string defaultServerGameClass{};
    std::optional<PortablePlayerLoginSource> playerLogin{};
    // At most eight commands, single actual viewport only. Empty is normal
    // startup. The entire startup still rolls back at its genuine later gap.
    std::vector<PortableStartupDamageCommand> damageCommands{};
    // At most eight explicit sweeps after real Login and before damage. Empty
    // is normal startup. Does not synthesize a velocity, floor or Landed event.
    std::vector<PortableStartupLandingCommand> landingCommands{};
    // At most eight bounded commands/64 total steps in this same root.
    // Inventory targets must be captured surviving original damage births.
    std::vector<PortableStartupFallingCommand> fallingCommands{};
    // Follow actual surviving Inventory births with state-aware Frob(JC,None).
    // Requires damageCommands and one real Login viewport; at most eight live
    // births/128 inventory links per observation. Normal startup leaves false.
    bool collectDamageLoot{};
    // Stronger rollback-only input proof: bind actual birth to the real
    // FrobTarget member, then invoke original ParseRightClick instead of Frob.
    // This supplies a diagnostic target, not controller reach/occlusion proof.
    bool collectDamageLootViaPlayer{};
};
struct PortablePlayerLoginObservation {
    std::uint64_t viewportIdentity{};
    std::string selectedClass,gamePath,functionPath,playerPath,error;
    bool attempted{},loginEntered{},loginReturned{},paired{};
    bool operator==(const PortablePlayerLoginObservation&) const = default;
};
struct PortableSpawnEncroachmentSnapshot {
    std::string actorPath,classPath;
    std::array<float,3u> location{};
    std::array<std::int32_t,3u> rotation{};
    std::int32_t touch{}; // Original literal0, not a configurable diagnostic input.
    bool attempted{},completed{},blocked{},initialStateReached{},baseReached{},postPostBeginReached{};
    // A blocked query really calls native DestroyActor(Actor,0). Its return
    // is ignored by original Spawn, but distinguished from a refused call.
    std::optional<std::int32_t> destroyReturn;
    std::size_t marksBefore{},arenasBefore{},marksAfter{},arenasAfter{};
};
struct PortableWorldActorOrderSnapshot {
    bool available{};
    std::string levelPath;
    std::optional<std::uint32_t> firstDynamic;
    // Actual process-local UObject indices; zero is a native null slot.
    std::vector<std::uint32_t> actorIndices;
    std::vector<std::uint8_t> staticFlags;
    bool operator==(const PortableWorldActorOrderSnapshot&) const = default;
};
struct PortableWorldActorPartitionProbeResult {
    QuestVr::Vm::Result execution;
    PortableWorldActorOrderSnapshot before,after;
};
struct PortableWorldActorPartitionDestroyProbeResult {
    QuestVr::Vm::Result execution;
    PortableWorldActorOrderSnapshot before,partitioned,afterDestroy;
    PortableActorDestroyProbeResult destruction;
    std::vector<PortableObjectLinkerSnapshot> linkersBefore,linkersAfter;
};
// Real partition followed by real native DestroyActor in ONE rollback root.
// Used to verify permutation/deletion undo composition, not begun gameplay.
PortableWorldActorPartitionDestroyProbeResult ProbePortableWorldActorPartitionThenDestroy(
    const std::vector<std::string>& actorPaths,const QuestVr::Vm::Limits& limits = {});
PortableWorldActorOrderSnapshot ReadPortableWorldActorOrder();
// The real native partition under one rollback-only root. No startup bits,
// flags, client/player possession or native cleanup are fabricated.
PortableWorldActorPartitionProbeResult ProbePortableWorldActorPartition(
    const QuestVr::Vm::Limits& limits = {});
struct PortableDumpLocationObservation {
    std::string path,outerPath,classPath,currentUser;
    std::vector<std::string> directory;
    std::uint32_t objectIndex{},flags{};
    std::int32_t fileIndex{},locationIndex{},locationCount{};
    bool saved{},nonActor{},noExport{},noActorSlot{};
    bool operator==(const PortableDumpLocationObservation&) const = default;
};
// Isolated native-service diagnostic, not original Login/Possess or startup.
// The constructed UObject lives only inside the real caller-owned VM root;
// observations are detached before weak-object GC at the public boundary.
enum class PortableDumpLocationAction { Inspect, Save, Load };
struct PortableDumpLocationProbeOptions {
    std::string playerPath;
    PortableDumpLocationAction action{PortableDumpLocationAction::Inspect};
    std::optional<QuestVr::Vm::Value> currentLocation;
    bool rollbackOnly{true};
};
struct PortableDumpLocationProbeResult {
    QuestVr::Vm::Result execution;
    PortableDumpLocationObservation object;
    QuestVr::Vm::Value before,after;
    bool savedBefore{},savedAfter{},available{};
};
PortableDumpLocationProbeResult ProbePortableDumpLocationSession(
    const PortableDumpLocationProbeOptions& options,const QuestVr::Vm::Limits& limits = {});
struct PortableWorldStartupProbeResult {
    QuestVr::Vm::Result execution;
    PortableWorldStartupPhase phase{PortableWorldStartupPhase::ValidateWorld};
    std::size_t actorSlot{static_cast<std::size_t>(-1)};
    std::string actorPath, gameInfoPath, initGameError;
    std::array<std::size_t, static_cast<std::size_t>(PortableWorldStartupPhase::Count)> visited{};
    bool restoredWorld{};
    float worldTime{}; // Staged native clock; never published by this probe.
    // Detached bounded observation of actual native Spawn calls in this SAME
    // original startup root. No pose setters, begun-play override or publication.
    std::vector<PortableSpawnEncroachmentSnapshot> spawnedEncroachment;
    PortableWorldActorOrderSnapshot beforeActorOrder,afterActorOrder;
    std::vector<PortablePlayerLoginObservation> playerLogins;
    std::vector<PortableDumpLocationObservation> dumpLocations;
    std::vector<PortableStartupDamageObservation> damageObservations;
    std::vector<PortableStartupLandingObservation> landingObservations;
    std::vector<PortableStartupFallingObservation> fallingObservations;
    std::vector<PortableStartupLootObservation> lootObservations;
};
const char* PortableWorldStartupPhaseName(PortableWorldStartupPhase phase);
// Non-publishing diagnostic over actual Level slots and original callbacks.
// One shared VM/host transaction is ALWAYS rolled back, including a prefix
// that reaches native actor ordering and the later client/player/detail gap.
// This never publishes a playable world or player; reached genuine Login/
// possession is staged in that same rollback root. Exact first
// VM failure and phase/slot/receiver are retained without accepting partial work.
PortableWorldStartupProbeResult ProbePortableWorldStartup(
    const PortableWorldStartupProbeOptions& options,
    const QuestVr::Vm::Limits& limits = {});

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
    const PortablePackageTables& package, std::uint32_t initialRandomSeed = 1u,
    std::optional<QuestVr::OriginalLinkerLoadProfile> loadProfile = {});
PortableRuntimeSummary InitializePortableRuntime(
    const std::vector<PortablePackageTables>& packages, std::uint32_t initialRandomSeed = 1u,
    std::optional<QuestVr::OriginalLinkerLoadProfile> loadProfile = {});
// Omission retains the historical UNFILTERED inspection/test mode. Standalone
// gameplay must select its explicit noneditor profile BEFORE object creation.
std::optional<QuestVr::OriginalLinkerLoadProfile> ReadPortableRuntimeLoadProfile();
void ShutdownPortableRuntime();
// Explicit read-only config/file-manager bindings for native DumpLocation.
// They are engine-session-owned; no process CWD change or original DLL call.
void PreparePortableRuntimeDumpLocationServices(const std::string& userIniPath,const std::string& searchDirectory);
struct PortableNativeEngineClassSnapshot {
    std::string classPath, baseClassPath, withinClassPath;
    // Original native constructor baseline, not serialized post-load flags or
    // proof that full UClass.Register/default/config construction has executed.
    std::uint32_t nativeSize{}, registrationClassFlags{}, registrationObjectFlags{};
};
struct PortableNativeEngineClassesSnapshot {
    std::optional<std::string> configuredGameEngineClass;
    std::vector<PortableNativeEngineClassSnapshot> classes;
    bool prepared{};
};
// CPU-owner native registration phase, independent of Client/headset and map
// lifetime. Binds only audited original native-only UClasses and their actual
// loaded base metadata; never fabricates serialized class descriptors/CDOs.
// Requires the configured concrete GameEngine identity. Atomic and idempotent;
// does not allocate an Engine/Primitive instance or claim constructor/Init,
// Level.Engine, startup, collision, movement or possession success.
PortableNativeEngineClassesSnapshot PreparePortableRuntimeNativeEngineClasses();
PortableNativeEngineClassesSnapshot ReadPortableRuntimeNativeEngineClasses();
struct PortableNativeMetadataClassSnapshot {
    std::string classPath, baseClassPath, withinClassPath, metaClassPath;
    std::uint32_t nativeSize{}, registrationClassFlags{}, registrationObjectFlags{};
    // Actual process-local UObject allocation indices, never PE addresses,
    // package/export slots or serialized save identities. Zero means absent.
    std::uint32_t objectIndex{}, baseClassIndex{}, withinClassIndex{}, metaClassIndex{};
};
struct PortableNativeMetadataClassesSnapshot {
    std::vector<PortableNativeMetadataClassSnapshot> classes;
    std::size_t boundSerializedClasses{};
    bool prepared{};
};
// Explicit class-only metadata phase, requiring the five native Engine class
// registrations already prepared. Atomically publishes actual Core.Class
// metaclass/super/Within relationships and binds existing serialized UClasses.
// This does not replay native Register/Cpp-field/default constructors, synthesize
// package Outers, allocate Engine/Cylinder instances or start the world.
PortableNativeMetadataClassesSnapshot PreparePortableRuntimeNativeMetadataClasses();
PortableNativeMetadataClassesSnapshot ReadPortableRuntimeNativeMetadataClasses();
struct PortableNativeConfigClassesSnapshot {
    std::vector<PortableNativeMetadataClassSnapshot> classes;
    std::string consoleClassPath;
    std::uint32_t consoleObjectIndex{}, consoleMetaClassIndex{}, consoleWithinClassIndex{};
    std::uint32_t consoleClassFlags{}, consoleObjectFlags{};
    bool prepared{};
};
// The four missing native config constraint UClasses plus the actual serialized
// Engine.Console authority. Requires the explicit Core metadata phase. Does not
// load configured native subclasses, create package Outers, perform StaticFind,
// construct native defaults/instances or initialize an Engine/Cylinder.
PortableNativeConfigClassesSnapshot PreparePortableRuntimeNativeConfigClasses();
PortableNativeConfigClassesSnapshot ReadPortableRuntimeNativeConfigClasses();
struct PortableNativePackageSnapshot {
    std::string objectPath;
    std::uint32_t objectIndex{}, classIndex{}, outerIndex{}, objectFlags{}, packageFlags{};
    bool legacyHandleAbsent{}, bindingAttempted{};
};
struct PortableNativePackagesSnapshot {
    std::vector<PortableNativePackageSnapshot> packages;
    std::uint32_t defaultObjectIndex{}, defaultClassIndex{};
    bool prepared{};
};
// Explicit bounded Package-default/instance and source LinkerRoot phase.
// Requires actual Core metadata. No legacy module is loaded; missing-module
// binding grants identity/Outer only, never native symbols or Engine startup.
PortableNativePackagesSnapshot PreparePortableRuntimeNativePackages();
PortableNativePackagesSnapshot ReadPortableRuntimeNativePackages();
struct PortableRuntimeObjectOwnershipSnapshot {
    std::string objectPath, classPath, outerPath;
    std::uint32_t objectIndex{}, classIndex{}, outerIndex{};
    bool serializedOuterZero{};
};
PortableRuntimeObjectOwnershipSnapshot ReadPortableRuntimeObjectOwnership(const std::string& objectPath);
// Actual loader-owned ULevel Double time. The explicit clock phase performs
// original retained-product accumulation and LevelInfo Float mirroring only;
// it is not a world Tick, startup, Timer/AI/physics pass or XR-thread entry.
// realDelta is before TimeDilation and actor-delta clamping. Call once from the
// exclusive CPU world's future Tick coordinator, never once per actor.
double ReadPortableRuntimeLevelTime();
QuestVr::Vm::Result AdvancePortableRuntimeLevelClock(float realDelta,
    const QuestVr::Vm::Limits& limits = {});
// Engine-session embedding state, never a map/checkpoint or player binding.
// These APIs share the runtime's exclusive CPU/script-thread ownership. The
// renderer may ONLY revoke the shared lifetime token before target teardown.
// Initialize/Shutdown reset registration; map replacement and save/load do not.
void BindPortableRuntimeClient(std::shared_ptr<QuestVr::NativeClientLifetime> lifetime,
    std::uint64_t clientIdentity);
void RegisterPortableRuntimeViewport(std::uint64_t clientIdentity,
    const QuestVr::NativeViewport& viewport);
void ClearPortableRuntimeClient();
bool HasPortableRuntimeClient();
std::vector<QuestVr::NativeViewport> ReadPortableRuntimeClientView();
struct PortablePlayerSessionSnapshot {
    std::uint64_t clientIdentity{}, viewportIdentity{};
    std::string clientPath, viewportPath, viewportClassPath, playerPath;
    std::string viewportOuterPath;
    std::int32_t clientObjectFlags{}, viewportObjectFlags{};
    QuestVr::NativeViewport viewport;
    bool bound{};
};
// Runtime CPU-owner entry after actual render-target registration. Materializes
// genuine session-owned Engine.Client/UViewport objects (UViewport inherits the
// original Engine.Player metadata), not a PlayerPawn or inventory item. Session
// objects are GC-rooted outside map exports and never serialized in actor saves.
// Preparation is idempotent for the same live registered viewport generation.
PortablePlayerSessionSnapshot PreparePortableRuntimePlayerObjects(std::uint64_t viewportIdentity);
PortablePlayerSessionSnapshot ReadPortableRuntimePlayerSession(std::uint64_t viewportIdentity);
// Audited retail SetPlayer native pairing followed by dynamic original Possess,
// in one VM/host transaction. Requires a live prepared viewport and actual begun
// native world PlayerPawn; does not perform Login or authorize bypassing it.
// Failure rolls back both old/new Player.Actor / PlayerPawn.Player edges and all
// nested scripts. Renderer lease revocation remains irreversible.
QuestVr::Vm::Result SetPortableRuntimePlayer(std::uint64_t viewportIdentity,
    const std::string& playerActorPath, const QuestVr::Vm::Limits& limits = {});
struct PortablePlayerHudItem {
    std::string actorPath, classPath, iconPath, beltDescription;
    bool inObjectBelt{};
    std::int32_t beltPosition{};
};
struct PortablePlayerHudSnapshot {
    bool available{}, bound{};
    std::string playerPath, keyRingPath;
    std::array<std::int32_t,6u> limbHealth{}; // Head, torso, arms L/R, legs L/R.
    float energy{}, energyMax{};
    std::optional<bool> headInWater; // Actual Pawn.HeadRegion.Zone.bWaterZone; absent zone stays unknown.
    float swimTimer{}, swimDuration{};
    std::vector<PortablePlayerHudItem> inventory; // Actual Inventory chain order.
};
// Explicit detached original-script fixture inspection; does not establish
// native possession. Exact declared typed fields and actual owned Inventory
// links only, with independent limb health and bounded cycle/identity checks.
PortablePlayerHudSnapshot ReadPortablePlayerHudSnapshot(const std::string& playerActorPath);
// Live HUD consumers must use this overload: source is the actual paired
// native viewport.Actor, never an arbitrary actor or selected inventory index.
// A genuinely unbound viewport returns available=false and no invented values.
PortablePlayerHudSnapshot ReadPortableRuntimePlayerHud(std::uint64_t viewportIdentity);
struct PortableNativeWindowSummary {
    std::size_t objects{}, roots{};
    std::optional<std::string> configuredRootClass;
    std::int32_t configuredRootNameCounter{}, graphicsContextNameCounter{};
};
// CPU-owner diagnostics only. Does not initialize a window or claim a running
// root graph. Naming counters and native roots must survive/revert together.
PortableNativeWindowSummary ReadPortableRuntimeNativeWindowSummary();
struct PortableNativeWindowFieldSnapshot {
    std::string declaration;
    QuestVr::Vm::Value value;
    std::vector<std::string> propertyRoots;
    std::uint32_t arrayIndex{};
};
struct PortableNativeWindowObjectSnapshot {
    std::string path,classPath,outerPath;
    std::vector<PortableNativeWindowFieldSnapshot> fields;
    std::vector<std::string> rowMajorWindows,colMajorWindows,tabGroupWindows,toggleButtons;
    bool tabOrderDirty{};
    bool emptyTileRows{};
};
struct PortableRootWindowProbeResult {
    QuestVr::Vm::Result execution;
    std::string pawnRootPath,inspectionError;
    std::vector<std::string> rootedWindows;
    std::vector<PortableNativeWindowObjectSnapshot> objects;
};
// Non-publishing original1052 diagnostic. Captures the pending typed native
// root/GC prefix before rollback, not a live HUD or a successful root. It always
// rolls back, preserves the original callback failure, and never changes INI,
// substitutes an InitWindow script or runs extra callbacks after a failure.
PortableRootWindowProbeResult ProbePortableRootWindowInitialization(
    const std::string& playerActorPath,const QuestVr::Vm::Limits& limits = {});
enum class PortableToggleProbeMode { RoundTrip, OneCheck, EmptySlot, NoResult, WrongReceiver, WrongArgument };
struct PortableToggleProbeStage {
    std::string name;
    std::size_t writes{};
    bool firstPressed{},secondPressed{},oneCheck{};
    std::string selection;
    // Original store order: text, texture, tile. Typed detached values only.
    std::array<QuestVr::Vm::Value,3u> firstAppearance,secondAppearance;
};
struct PortableToggleProbeResult {
    QuestVr::Vm::Result execution;
    std::string rootPath,radioPath,firstPath,secondPath;
    std::vector<std::string> registeredToggles;
    std::vector<PortableToggleProbeStage> stages;
};
// Isolated real native Root/Radio/Toggle constructors and original VM calls,
// ALWAYS rolled back. Explicit base-Root fixture, not configured game HUD or
// player startup, and never additional callbacks after a poisoned HUD failure.
PortableToggleProbeResult ProbePortableNativeWindowToggle(const std::string& playerActorPath,
    PortableToggleProbeMode mode,const QuestVr::Vm::Limits& limits = {});
struct PortableMapNorthSnapshot {
    bool found{};
    std::string actorPath;
    std::int32_t trueNorth{};
};
// Mirrors GetLevelInfo's first actual live Level-slot DeusExLevelInfo selection,
// then reads its exact declared Int TrueNorth. An absent actor legitimately
// leaves the original initial zero; malformed metadata/values throw. Read only,
// with no player creation, script execution or authored-property mutation.
PortableMapNorthSnapshot ReadPortableRuntimeMapNorth();
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

// Read-only render bootstrap while original world/AI startup is unavailable.
// Executes the receiver's actual PlayWaiting helper in a rollback-only VM
// transaction. Never selects a guessed sequence or starts a state/AI frame.
// Property, RNG, birth, deletion and revision effects are all discarded; only
// immutable asset caches and this detached presentation result may survive.
struct PortableActorWaitingPresentation {
    bool available{};
    std::string function, meshPath, error;
    PortableActorAnimationSnapshot animation;
    QuestVr::ActorVec3 prePivot;
};
PortableActorWaitingPresentation PreviewPortableActorWaitingPresentation(
    const std::string& actorPath, const QuestVr::Vm::Limits& limits = {});

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
    bool collideActors{}, blockPlayers{};
    bool moverMoving{}, moverClosing{};
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
// Read-only loader/native-query diagnostics, not a saved UObject pointer or a
// validation token. Snapshotting never decodes geometry or changes gameplay.
// A new map load receives a non-reused generation even at the same filename;
// warm checkpoint replacement retains its original native Level/Model binding.
struct PortableNativeModelSnapshot {
    bool available{}, geometryCached{};
    std::uint64_t mapGeneration{};
    std::string levelPath,modelPath;
    std::size_t modelExportIndex{},cachedNodes{};
};
PortableNativeModelSnapshot ReadPortableRuntimeNativeModel();
struct PortablePrimitiveModelCacheEntrySnapshot {
    std::uint64_t mapGeneration{};
    std::string levelPath,modelPath,sourcePath;
    std::size_t modelExportIndex{},cachedNodes{};
};
struct PortablePrimitiveModelCacheSnapshot {
    std::size_t retainedBytes{},maximumEntries{},maximumRetainedBytes{};
    std::vector<PortablePrimitiveModelCacheEntrySnapshot> entries;
};
// Copied diagnostics only; never decodes, roots, validates a future query or
// exports a native pointer. Separate from the one-record root FastTrace cache.
PortablePrimitiveModelCacheSnapshot ReadPortableRuntimePrimitiveModelCache();
// Read-only native ToWorld VALUE for an actual indexed current-map Actor.
// Native dispatch is selected from the audited nearest RF_Native UClass;
// current qualified fields include overlays/defaults, never renderer snapshots.
// No native pointer, Model/hash query authority, movement or startup is exposed.
// Call only on the runtime-owning thread. Unsupported identity/schema/arithmetic
// throws without publishing a partial snapshot or changing gameplay state.
struct PortableActorToWorldSnapshot {
    std::string actorPath,nativeClassPath,nativeImplementation;
    std::array<float,3u> origin{},xAxis{},yAxis{},zAxis{};
};
PortableActorToWorldSnapshot ReadPortableRuntimeActorToWorld(const std::string& actorPath);
struct PortableActorPrimitiveSnapshot {
    std::string actorPath, primitivePath, nativeClassPath;
    std::string lineImplementation, boundsImplementation;
};
struct PortableActorPrimitiveBoundsSnapshot {
    PortableActorPrimitiveSnapshot primitive;
    std::array<float,3u> minimum{},maximum{};
};
struct PortableActorPrimitiveLineSnapshot {
    PortableActorPrimitiveSnapshot primitive;
    bool clear{}, hitOwner{}, hitPrimitiveIsNone{};
    std::array<float,3u> location{},normal{};
    float time{1.0f};
    std::int32_t item{-1};
};
// CPU-owner read-only actual selected-primitive diagnostics. Brush precedes
// Mesh; null/null requires the explicit genuine Engine.Cylinder collision
// service and actual current Level.Engine binding (portable_engine_collision.h).
// Unprepared lifetime refuses. No renderer/DrawType/model availability fallback.
// Model bounds and point/extent LineCheck use the actual selected immutable
// Model and current native owner coordinates. Point hits capture ToWorld again
// for the original normal transform; point misses preserve the full hit packet.
// Mesh/LodMesh use their audited wrapper; raw Primitive uses its direct cylinder
// implementation, without Mesh's all-zero-or-NaN extent canonicalization.
// The line diagnostic deliberately seeds Next/Actor/Primitive/Location/Normal
// zero, Time1 and Item-1; it exposes no native pointers or query capability and
// is not the production hash/collector caller's FCheckResult seed contract.
PortableActorPrimitiveSnapshot ReadPortableRuntimeActorPrimitive(const std::string& actorPath);
PortableActorPrimitiveBoundsSnapshot ReadPortableRuntimeActorPrimitiveBounds(const std::string& actorPath);
PortableActorPrimitiveLineSnapshot ProbePortableRuntimeActorPrimitiveLine(const std::string& actorPath,
    const std::array<float,3u>& end,const std::array<float,3u>& start,
    const std::array<float,3u>& extent={},std::uint32_t flags=0u);
QuestVr::Vm::Value ReadPortableActorScriptProperty(
    const std::string& actorPath, const std::string& propertyName,
    std::uint32_t arrayIndex = 0u);
// Read a specific authored declaring field, not the closest same-name shadow.
// Requires an active, nonretired Actor and actual Class.Children/Next ownership.
// Like all runtime snapshots, call only on the runtime-owning thread.
QuestVr::Vm::Value ReadPortableActorDeclaredScriptProperty(
    const std::string& actorPath, const std::string& propertyPath,
    std::uint32_t arrayIndex = 0u);
// Read-only exact declared field of a live UObject instance, including native
// non-Actor ConListItems. No construction, lifecycle dispatch or mutation.
QuestVr::Vm::Value ReadPortableRuntimeDeclaredScriptProperty(
    const std::string& objectPath,const std::string& propertyPath,
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
// Detached authored or explicit portable state context. Raw runnable identities
// can be inspected, but their byte offsets are not executable VM continuations.
// Inspection does not initialize a frame or call actor lifecycle callbacks.
struct PortableActorDispatchContext {
    std::string codePath, stateName{"None"};
    // Detached identities: selected StateNode is not the running Node. Raw
    // inspection never converts a serialized byte offset into a VM ordinal.
    std::string selectedPath, runningPath;
    bool framePresent{}, codePresent{}, exactSelected{};
    std::uint64_t classProbeMask{};
    std::optional<QuestVr::ScriptDispatch::CodeMasks> codeMasks;
    // Positive effective FStateFrame bits, independent of authored masks.
    // No frame permits every probe. Legacy portable overrides explicitly
    // retain their old approximation; only modern controls have exact identity.
    std::optional<std::uint64_t> liveProbeMask;
    std::set<std::string> disabledNames;
};
struct PortableScriptDispatchSummary {
    std::size_t classes{}, states{}, classFunctions{}, stateFunctions{}, commonFields{};
};
PortableScriptDispatchSummary ReadPortableRuntimeDispatchSummary();
// Detached read-only class/state dispatch metadata, preserving actual
// Children/Next state order and explicit SuperField paths. No actor effects.
QuestVr::ScriptDispatch::Class ReadPortableRuntimeDispatchClass(const std::string& classPath);
PortableActorDispatchContext ReadPortableActorDispatchContext(const std::string& actorPath);
std::optional<QuestVr::StateObject> ReadPortableActorStateObject(const std::string& actorPath);
// Explicit bounded state slice, not a world tick or automatic startup phase.
QuestVr::Vm::Result ResumePortableActorState(const std::string& actorPath,
    const QuestVr::Vm::Limits& limits = {});
// Explicit original Actor.ProcessState poll + continuation, not a full actor
// Tick: no animation, Tick event, Timer, physics or automatic world startup.
QuestVr::Vm::Result AdvancePortableActorState(const std::string& actorPath, float elapsed,
    const QuestVr::Vm::Limits& limits = {});
// Explicit isolated original Actor timer phase, NOT a world tick/scheduler.
// Advances reflected TimerRate/TimerCounter/bTimerLoop and synchronously calls
// the current-state Timer at most once in one interpreter/root journal. A Bool
// result means a period elapsed, not that an eligible event body ran; suppressed
// events still consume periods. Callback failure rolls back the entire phase.
// Signed finite reflected counters/rates persist through ordinary actor saves.
QuestVr::Vm::Result AdvancePortableActorTimer(const std::string& actorPath, float elapsed,
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
// Read-only retained assets. Valid only on the runtime-owning thread until
// map replacement/unload; workers must take their own detached copy.
const PortableLodMesh& GetPortableRuntimeMeshView(const std::string& meshPath);
const PortableLodMesh& GetPortableRuntimeBrushView(const std::string& brushPath);
PortableTextureArray BuildPortableRuntimeActorTextureArray(
    std::uint32_t width,
    std::uint32_t height);
PortableInteractionResult InteractPortableRuntimeActor(const std::string& objectPath);
// Authored-key motion for the bounded VR interaction layer, not original
// Mover script startup/Tick/encroachment. Never advances while UI is paused.
bool AdvancePortableRuntimeMovers(float elapsedSeconds);
// changedOnly retains in-flight/holding motion plus the last advanced endpoint,
// avoiding full metadata snapshots of every unrelated door during each VR frame.
std::vector<PortableActorSnapshot> GetPortableRuntimeMoverActors(bool changedOnly = false);
// Undo the most recent closing step and reopen instead of crushing the player.
bool BlockPortableRuntimeMover(const std::string& objectPath);
bool VerifyPortableRuntimeInteraction();
bool SavePortableRuntimeState(const std::string& path);
// Optional binding protects paired metadata from referring to another saved map.
// Validation may inspect the target map without replacing the current world.
// Legacy v1-v3 have no embedded map identity and retain their existing behavior;
// v14 additionally binds authored mover owners to their single target map.
// v15 optionally wraps that complete payload with native playable quick-slot
// assignments/equipment. Uninitialized script-only users retain older formats.
bool ValidatePortableRuntimeState(const std::string& path, const std::string& expectedMapName = {});
bool LoadPortableRuntimeState(const std::string& path);
PortableDamageResult DamagePortableRuntimeActor(
    const std::string& objectPath,
    float damage);
bool VerifyPortableRuntimeDamage();
std::size_t GetPortableRuntimeInventoryCount();
std::vector<std::string> GetPortableRuntimeInventoryItems();
// Native playable-port equipment, independent of the unfinished original
// PlayerPawn/Window bootstrap. Slot0 is the functional collected-key ring;
// ordinary slots contain actual identities from the playable inventory.
struct PortableQuickBeltItem {
    std::string actorPath,classPath,iconPath,description;
    bool keyRing{},selected{};
};
struct PortableQuickBeltSnapshot {
    bool available{};
    std::array<std::optional<PortableQuickBeltItem>,10u> slots;
    std::string equippedActorPath;
    std::size_t collectedKeys{};
};
PortableQuickBeltSnapshot ReadPortableRuntimeQuickBelt();
bool EquipPortableRuntimeInventoryItem(const std::string& actorPath);
bool CyclePortableRuntimeQuickBelt(int direction);
std::string GetPortableRuntimeEquippedItem();
bool PortableRuntimeKeyRingEquipped();
enum class PortableInventoryDescriptorStatus : std::uint8_t {
    Available, InvalidIdentity, RuntimeUnavailable, UnindexedIdentity,
    NotInventoryActor, MissingPropertyMetadata, MalformedProperty, InvalidLayout,
    InvalidIconDimensions, MissingIconMetadata
};
enum class PortableInventoryValueOrigin : std::uint8_t { Zero, ClassDefault, Instance, Overlay };
enum class PortableInventoryTextStatus : std::uint8_t {
    Unavailable, Available, MissingPropertyMetadata, MalformedProperty
};
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
    // Original scalar StrProperty values only, detached VM UTF-8/WTF-8 text.
    // Available may contain an explicitly empty/typed-zero string. Optional
    // text errors do not change the independent icon/layout row status.
    std::optional<std::string> itemName, description;
    PortableInventoryTextStatus itemNameStatus{PortableInventoryTextStatus::Unavailable};
    PortableInventoryTextStatus descriptionStatus{PortableInventoryTextStatus::Unavailable};
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
// ItemName/Description require actual inherited scalar StrProperty declarations
// and typed overlay/tag/default data. Missing or malformed text is explicit and
// never replaced with a class-name label, guessed description or item state.
std::vector<PortableInventoryDescriptor> ReadPortableRuntimeInventoryDescriptors(
    const std::vector<std::string>& actorPaths,
    const PortableInventoryDescriptorLimits& limits = {});
// One indexed, actual collected Engine.Inventory instance, including inactive
// pickups. No UClass/class@event interpretation, invented POV name or equipped
// state. Typed optional fields retain independent errors and source provenance.
// Uses only load-time indexed metadata: no file/asset decode, script execution,
// allocation of game actors, runtime/clock/GC writes or cache mutation.
PortableHeldItemVisualDescriptor ReadPortableRuntimeHeldItemVisualDescriptor(
    const std::string& collectedActorPath, const PortableHeldItemDescriptorLimits& limits = {});
bool ConsumePortableRuntimeInventoryItem(const std::string& objectPath);
PortablePlayerProgress GetPortableRuntimePlayerProgress();
float GetPortableRuntimePlayerHealth();
float DamagePortableRuntimePlayer(float damage);
float HealPortableRuntimePlayer(float amount);
