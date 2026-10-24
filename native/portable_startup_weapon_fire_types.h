#pragma once

#include "portable_startup_weapon_types.h"

// One explicit original player Fire call after the proven Training equip.
// No aim, position, target, damage or desired attack sequence is supplied.
struct PortableStartupWeaponFireCommand {
    float deltaSeconds{0.025f};
    std::uint32_t frames{64u};
    bool operator==(const PortableStartupWeaponFireCommand&) const = default;
};

struct PortableStartupWeaponFireNotify {
    std::string function;
    float time{};
    std::uint32_t serializedIndex{};
    bool operator==(const PortableStartupWeaponFireNotify&) const = default;
};

enum class PortableStartupWeaponFireStage { Fire,AnimationThenState };
struct PortableStartupWeaponFireObservation {
    PortableStartupWeaponFireStage stage{PortableStartupWeaponFireStage::Fire};
    std::size_t commandIndex{},frameIndex{},writesBefore{},writesAfter{};
    std::string actorPath,playerPath,gameInfoPath,functionPath,functionSource;
    bool entered{},returned{},captured{},failed{},partialAfterCaptured{},ownedAfter{},
        attackSelected{},attackCycleComplete{},suppressAnimEnd{},animationReturned{},animationIterationCap{};
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
    PortableStartupWeaponPlayerSnapshot playerBefore,playerAfter;
    PortableStartupWeaponItemSnapshot weaponBefore,weaponAfter;
    bool operator==(const PortableStartupWeaponFireObservation&) const = default;
};
