#pragma once

#include "quest_portable_vm.h"
#include "quest_state_frame.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Optional, non-publishing first-Training diagnostic input. This selects an
// existing authored item; it neither grants inventory nor starts a new root.
struct PortableStartupWeaponCommand {
    std::string actorPath;
    float deltaSeconds{0.05f};
    std::uint32_t frames{32u};
    bool operator==(const PortableStartupWeaponCommand&) const = default;
};

struct PortableStartupWeaponStateSnapshot {
    std::string selectedPath,codePath,localsCodePath;
    bool hasStateObject{},hasStack{},frameOverride{},hasFrame{},hasControl{},codePresent{};
    std::uint32_t statementIndex{};
    std::uint64_t liveProbeMask{},localsRevision{};
    // These are DIFFERENT domains. A modern Continue ordinal is not evidence
    // of native LatentAction0. The immutable loader stack is copied verbatim.
    std::optional<QuestVr::StateLatent> portableLatent;
    bool hasSerializedStack{};
    std::int32_t rawFunctionReference{},rawStateReference{};
    std::uint64_t rawProbeMask{};
    std::uint32_t rawLatentAction{};
    std::optional<std::int32_t> rawLogicalOffset;
    std::optional<float> latentTimeLeft;
    bool operator==(const PortableStartupWeaponStateSnapshot&) const = default;
};

struct PortableStartupWeaponInventoryLink {
    std::string actorPath,classPath,ownerPath,inventoryPath,statePath;
    bool runtimeBirth{},removed{},deleteMe{};
    bool operator==(const PortableStartupWeaponInventoryLink&) const = default;
};

struct PortableStartupWeaponPlayerSnapshot {
    std::string actorPath,classPath,playerPath,frobTargetPath,inHandPath,inHandPendingPath,
        clientInHandPendingPath,selectedItemPath,weaponPath,pendingWeaponPath;
    std::vector<PortableStartupWeaponInventoryLink> inventory;
    std::vector<std::uint8_t> grid,clock;
    std::int32_t gridRows{},gridColumns{},role{},health{};
    bool isPawn{},isPlayer{},inHandTransition{};
    PortableStartupWeaponStateSnapshot state;
    bool operator==(const PortableStartupWeaponPlayerSnapshot&) const = default;
};

struct PortableStartupWeaponItemSnapshot {
    std::string actorPath,classPath,ownerPath,inventoryPath,instigatorPath,meshPath,
        playerViewMeshPath,ammoTypePath,sequence;
    std::vector<std::uint8_t> clock;
    PortableStartupWeaponStateSnapshot state;
    float frame{},rate{},tweenRate{},last{},drawScale{};
    std::int32_t physics{},inventoryGroup{},invPosX{},invPosY{},invSlotsX{},invSlotsY{};
    bool removed{},deleteMe{},heldItem{},hidden{},carriedItem{},onlyOwnerSee{},
        animLoop{},animFinished{},weaponUp{},readyToFire{};
    bool operator==(const PortableStartupWeaponItemSnapshot&) const = default;
};

enum class PortableStartupWeaponStage { Pickup,PutInHand,UpdateInHand,AnimationThenState };
struct PortableStartupWeaponObservation {
    PortableStartupWeaponStage stage{PortableStartupWeaponStage::Pickup};
    std::size_t commandIndex{},frameIndex{},writesBefore{},writesAfter{};
    std::string actorPath,playerPath,gameInfoPath,functionPath,functionSource;
    bool entered{},returned{},captured{},ownedAfter{},equipComplete{};
    // Empty for ordinary selected-method calls. A poll-ineligible phase may
    // return Returned without resuming; its actual frame/latent is authoritative.
    std::optional<QuestVr::Vm::StatePhaseStatus> stateDisposition;
    QuestVr::Vm::Kind resultKind{QuestVr::Vm::Kind::Nothing};
    float deltaSeconds{};
    bool suppressAnimEnd{},animationIterationCap{};
    std::size_t animationIterations{},animationFloatWrites{},animationFinishedWrites{},
        notifyEvents{},animEndEvents{};
    PortableStartupWeaponPlayerSnapshot playerBefore,playerAfter;
    PortableStartupWeaponItemSnapshot weaponBefore,weaponAfter;
    bool operator==(const PortableStartupWeaponObservation&) const = default;
};
