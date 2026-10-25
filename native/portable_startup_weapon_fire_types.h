#pragma once

#include "portable_startup_weapon_types.h"
#include <array>

// Explicit diagnostic pose, applied through original SetLocation and selected
// ClientSetRotation, never raw property patches or a forced trace target.
struct PortableStartupWeaponFirePose {
    std::array<float,3u> location{};
    std::array<std::int32_t,3u> viewRotation{};
    bool operator==(const PortableStartupWeaponFirePose&) const = default;
};

// Bounded original player Fire cycles after the proven Training equip.
// No hit target, damage or desired attack sequence is supplied.
struct PortableStartupWeaponFireCommand {
    float deltaSeconds{0.025f};
    std::uint32_t frames{64u};
    std::uint32_t attacks{1u};
    std::optional<PortableStartupWeaponFirePose> pose;
    bool operator==(const PortableStartupWeaponFireCommand&) const = default;
};

struct PortableStartupWeaponFireNotify {
    std::string function;
    float time{};
    std::uint32_t serializedIndex{};
    bool operator==(const PortableStartupWeaponFireNotify&) const = default;
};

struct PortableStartupWeaponFireCrateSnapshot {
    std::string actorPath,classPath,statePath,basePath,contentsPath;
    std::array<float,3u> location{};
    std::int32_t hitPoints{},physics{};
    bool removed{},deleteMe{};
    bool operator==(const PortableStartupWeaponFireCrateSnapshot&) const = default;
};
struct PortableStartupWeaponFireBirth {
    std::string actorPath,classPath,statePath,ownerPath,basePath;
    std::array<float,3u> location{};
    std::int32_t physics{};
    bool removed{},deleteMe{},inventory{};
    bool operator==(const PortableStartupWeaponFireBirth&) const = default;
};
// Only an actual returned native277 query creates a row. outputsComplete is
// set after its ordered script OUT stores, not merely after the query returns.
struct PortableStartupWeaponFireTrace {
    std::string sourcePath,actorPath;
    std::array<float,3u> start{},end{},extent{},hitLocation{},hitNormal{};
    bool traceActors{},outputsComplete{};
    std::size_t writesBeforeOutputs{},writesAfterOutputs{};
    bool operator==(const PortableStartupWeaponFireTrace&) const = default;
};
enum class PortableStartupWeaponFireStage { SetLocation,ClientSetRotation,Fire,AnimationThenState };
struct PortableStartupWeaponFireObservation {
    PortableStartupWeaponFireStage stage{PortableStartupWeaponFireStage::Fire};
    std::size_t commandIndex{},attackIndex{},frameIndex{},writesBefore{},writesAfter{};
    std::string actorPath,playerPath,gameInfoPath,functionPath,functionSource;
    bool entered{},returned{},captured{},failed{},partialAfterCaptured{},ownedAfter{},
        attackSelected{},attackCycleComplete{},suppressAnimEnd{},animationReturned{},animationIterationCap{};
    bool placementAccepted{},poseCaptured{};
    std::array<float,3u> locationBefore{},locationAfter{};
    std::array<std::int32_t,3u> rotationBefore{},rotationAfter{},viewRotationBefore{},viewRotationAfter{};
    float baseEyeHeightBefore{},baseEyeHeightAfter{};
    QuestVr::Vm::Kind resultKind{QuestVr::Vm::Kind::Nothing};
    std::optional<QuestVr::Vm::StatePhaseStatus> stateDisposition;
    float deltaSeconds{};
    // Valid only when animationReturned. These are the original phase's
    // dispatch-attempt counts, not a fabricated trace/hit/TakeDamage witness.
    std::size_t animationIterations{},animationFloatWrites{},animationFinishedWrites{},
        notifyEvents{},animEndEvents{},missingHandlers{},totalReturnedNotifyEvents{};
    // Actual mesh/sequence metadata observed after a successful Fire or phase;
    // having a notify in this array does not prove that it has been dispatched.
    std::vector<PortableStartupWeaponFireNotify> authoredNotifies;
    std::array<PortableStartupWeaponFireCrateSnapshot,2u> cratesBefore{},cratesAfter{};
    std::vector<PortableStartupWeaponFireTrace> traces;
    std::vector<PortableStartupWeaponFireBirth> births;
    PortableStartupWeaponPlayerSnapshot playerBefore,playerAfter;
    PortableStartupWeaponItemSnapshot weaponBefore,weaponAfter;
    bool operator==(const PortableStartupWeaponFireObservation&) const = default;
};
