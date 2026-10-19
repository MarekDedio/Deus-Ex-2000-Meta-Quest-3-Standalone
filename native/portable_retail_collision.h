#pragma once

#include "portable_unreal_runtime.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// CPU-owner diagnostics of the real current ULevel hash registration prefix.
// These entries ALWAYS roll back their VM root. They are not a publishing
// world-start, movement/physics implementation or a saved native hash format.
struct PortableRetailCollisionActor {
    std::string path;
    std::uint32_t objectIndex{}, tag{};
    std::array<float,3u> location{},colLocation{};
    bool collideActors{},pendingDelete{},declaredMirrorsMatch{};
};
struct PortableRetailCollisionSnapshot {
    bool initialized{},enabled{},initializedHere{};
    std::string levelPath;
    std::uint64_t mapGeneration{},hashIdentity{};
    std::uint32_t tag{},seed{};
    std::size_t retainedBytes{},hashes{},allocatedLinks{},activeLinks{};
    // Work completed in this outer host root, not persistent process totals.
    std::size_t initializationDraws{},lifecycleOperations{};
    std::vector<PortableRetailCollisionActor> actors;
};
enum class PortableRetailCollisionOperation { Enable, Disable, LineCheck, SetCollision, SetCollisionSize, MultiLineCheck, EncroachmentCheck,
    IsBlockedBy, IsBasedOn, LocalPlayerPawn, IsOverlapping, BeginTouch, EndTouch, TouchSide, SetActorZone,
    LevelMoveActor, ActorMove, ActorSetRotation, ActorMoveSmooth, LevelCheckEncroachment };
struct PortableRetailCollisionCommand {
    PortableRetailCollisionOperation operation{PortableRetailCollisionOperation::Enable};
    std::array<float,3u> end{},start{},extent{};
    std::uint8_t flags{};
    std::string actorPath{};
    std::vector<QuestVr::Vm::Value> arguments{};
    bool checkActors{true},checkWorld{true};
    // Encroachment only: actorPath is source, end is proposed Location. Its
    // original unused final DWORD is distinct from LineCheck's byte flags.
    std::array<std::int32_t,3u> rotation{};
    std::uint32_t ignoredEncroachmentFlags{};
    // Original native predicates only: actorPath is receiver. Empty other is
    // accepted as None only for IsBasedOn. No collision hash is required.
    std::string otherActorPath{};
    std::int32_t touchSuppressSelf{};
    // Original SetActorZone only. Any-nonzero native signed integer flags.
    std::int32_t zoneTest{},zoneForce{};
    // Explicit dormant diagnostic fixture ONLY: end is a temporary native
    // Location view, with original Rotation retained. Never a committed Move,
    // hash relocation, startup bypass or claim about live callback mutation.
    bool zoneUsePose{};
    // Original native movement diagnostics: end is Delta (or proposed Location
    // for LevelCheckEncroachment), rotation is by-value requested rotation.
    // Full native flags, not a read-only preview: the enclosing root rolls back.
    std::array<std::int32_t,4u> moveOptions{};
    std::int32_t moveTouch{};
};
struct PortableRetailCollisionMutation {
    std::string actorPath;
    QuestVr::Vm::Value returned;
    float radius{},height{};
    bool collideActors{},blockActors{},blockPlayers{},collideWorld{};
};
struct PortableRetailCollisionHit {
    std::string actorPath,primitivePath;
    std::size_t next{};
    std::array<float,3u> location{},normal{};
    float time{};
    std::int32_t item{-1};
};
struct PortableRetailMultiLineMetadata {
    std::size_t queryIndex{},collectorSteps{},actorVisits{},moverTests{},skipped{},corrected{};
    std::array<float,3u> actorQueryEnd{};
    float scale{1.0f};
    bool hasWorldHit{},nearWorldHit{},worldItemKnown{true};
};
struct PortableRetailActorPredicate {
    std::size_t commandIndex{};
    bool returned{};
    std::string localPlayerPath;
};
struct PortableRetailTouchSnapshot {
    std::size_t commandIndex{};
    // Native Bool only for TouchSide; BeginTouch/EndTouch are void operations.
    bool sideAccepted{};
    std::array<std::string,4u> source,other;
};
struct PortableRetailActorRegion {
    std::string zonePath;
    std::int32_t leaf{-1};
    std::uint8_t number{};
};
struct PortableRetailZoneSnapshot {
    std::size_t commandIndex{};
    std::string actorPath;
    bool pawn{};
    std::array<PortableRetailActorRegion,3u> regions;
    // Optional original clock snapshots encoded with the SAME canonical save
    // codec, taken inside this root before/after the native zone operation.
    // Empty means no actual native clock, not a fabricated pose/idle seed.
    std::vector<std::uint8_t> clockBefore,clockAfter;
};
struct PortableRetailMoveSnapshot {
    std::size_t commandIndex{};
    std::string actorPath;
    std::int32_t returned{};
    std::array<float,3u> locationBefore{},locationAfter{},colLocationBefore{},colLocationAfter{};
    std::array<std::int32_t,3u> rotationBefore{},rotationAfter{};
    // Only LevelMoveActor has a caller-visible hit. Its direct diagnostic seed
    // is Time.375/Item127/Location(11,12,13)/Normal(-1,0,0), with null pointers.
    // No live native opaque handle/address crosses the CPU-owner boundary.
    bool hitAvailable{},hitNextPresent{};
    PortableRetailCollisionHit hit;
    std::size_t marksAfter{},arenasAfter{};
    std::vector<std::uint8_t> clockBefore,clockAfter;
};
struct PortableRetailCollisionProbeResult {
    QuestVr::Vm::Result execution;
    bool available{};
    std::vector<PortableRetailCollisionSnapshot> states;
    std::vector<std::vector<PortableRetailCollisionHit>> queries;
    std::vector<std::size_t> queryHeads;
    std::vector<PortableRetailCollisionMutation> mutations;
    std::vector<PortableRetailMultiLineMetadata> multiLine;
    std::vector<PortableRetailActorPredicate> predicates;
    std::vector<PortableRetailTouchSnapshot> touches;
    std::vector<PortableRetailZoneSnapshot> zones;
    std::vector<PortableRetailMoveSnapshot> moves;
};
// Bounded original SetActorCollision four-case and actual primitive-query
// diagnostic. No native pointer leaves the CPU owner; every state is detached.
// Combined detached output is capped at8MiB and charged during its VM root.
PortableRetailCollisionProbeResult ProbePortableRuntimeRetailCollision(
    const std::vector<PortableRetailCollisionCommand>& commands,
    const QuestVr::Vm::Limits& limits = {});

struct PortableRetailCollisionStartupResult {
    PortableWorldStartupProbeResult world;
    PortableRetailCollisionSnapshot collision;
};
// Extends the existing isolated script-startup coordinator with its authentic
// earlier LoadMap collision-enable boundary, before GameInfo selection/birth.
// Explicit actual Engine/metadata/Package/lookup/Cylinder preparation is needed.
PortableRetailCollisionStartupResult ProbePortableRuntimeRetailCollisionStartup(
    const PortableWorldStartupProbeOptions& options,
    const QuestVr::Vm::Limits& limits = {});
// Outside either diagnostic, this remains uninitialized after rollback.
PortableRetailCollisionSnapshot ReadPortableRuntimeRetailCollision();

// Read-only actual primitive diagnostic using the original hash caller's
// packet seed (Time0), distinct from the older isolated Time1 diagnostic.
PortableActorPrimitiveLineSnapshot ProbePortableRuntimeRetailCollisionPrimitiveLine(
    const std::string& actorPath,const std::array<float,3u>& end,
    const std::array<float,3u>& start,const std::array<float,3u>& extent={},std::uint32_t flags=0u);
