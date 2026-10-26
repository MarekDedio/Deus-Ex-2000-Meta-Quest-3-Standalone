#pragma once

#include "quest_actor_pawn_context.h"
#include "quest_native_scalar.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

enum class OriginalPawnLocalTickResult { ContinueState,ReturnOne };

struct OriginalActorPawnTickQueries {
    std::function<std::uint8_t(std::uintptr_t)> remoteRole,role,physics;
    // A fresh, genuine source-Actor cast to the actual native APlayerPawn
    // Class. It returns either that same source or null, never another Actor.
    std::function<std::uintptr_t(std::uintptr_t)> playerPawnCast,player;
    // Explicit native source services for the other branches. None is an
    // optional success hook. An unfinished reached service must refuse inside
    // the caller root, not replace this branch with ordinary Pawn/non-Pawn Tick.
    // Movement ends after moveSmooth103A2172; this helper owns the Tick probe.
    std::function<void(std::uintptr_t,std::uintptr_t,float)> simulatedPawnMovement;
    std::function<void(std::uintptr_t,std::uintptr_t)> remotePlayerViewCopy;
    // Includes the native subclass/flag early-return gate, Player virtual+54
    // pre/post calls, selected PlayerInput/PlayerTick and local network copy.
    // ContinueState joins103A2690, NOT the ordinary Tick/type1 gate103A2643.
    std::function<OriginalPawnLocalTickResult(std::uintptr_t,std::uintptr_t,std::uintptr_t,float,std::int32_t)> localPlayerTick;
    std::function<bool(std::uintptr_t)> isProbingTick;
    std::function<void(std::uintptr_t,float)> tick,processState,timer,performPhysics;
    std::function<float(std::uintptr_t)> lifeSpan;
    std::function<void(std::uintptr_t,float)> writeLifeSpan;
    std::function<void(std::uintptr_t)> expired;
    std::function<void(std::uintptr_t,std::int32_t)> destroy;
    // Exact103A278D..103A288D network/view-target block, including the real
    // native NetDriver-null skip. It precedes even a null captured-Pawn return.
    std::function<void(std::uintptr_t,std::uintptr_t)> networkTail;
    std::function<bool(std::uintptr_t)> pawnIsPlayer,pawnViewTarget;
    std::function<void(std::uintptr_t,float)> updateEyeHeight;
    // Raw by-value source Rotation -> captured Pawn.ViewRotation.
    std::function<void(std::uintptr_t,std::uintptr_t)> copyRotationToView;
    std::function<std::uintptr_t(std::uintptr_t)> weapon;
    // Actual native XLevel.FarMoveActor(weapon, raw fresh source Location,0,0),
    // not source performPhysics and not a controller/visual position update.
    std::function<void(std::uintptr_t,std::uintptr_t)> farMoveWeapon;
    // Exact103A2946..103A2AB2 sight/perception/pain/speech/tactics block. It
    // is reached only by the fresh Role4 AND literal tickType2 gate below.
    std::function<void(std::uintptr_t,std::uintptr_t,float)> longPawnTail;
    std::function<void()> work;
};
struct OriginalActorPawnTickStats {
    int returned{};
    std::size_t steps{},playerCasts{},tickCalls{},stateCalls{},timerCalls{},physicsCalls{},
        lifeSpanReads{},lifeSpanWrites{},expiredCalls{},destroyCalls{},networkTailCalls{},
        eyeHeightCalls{},viewCopies{},weaponMoves{},longTailCalls{};
    bool simulatedBranch{},remoteRoleBranch{},lowRoleBranch{},localPlayerBranch{},
        localPlayerReturned{},tickTypeSkipped{},expired{},shortPawnTail{},nullPawnTail{};
};
namespace ActorPawnTickDetail {
template<class Provider,class... Args>
inline auto Call(const OriginalActorPawnTickQueries& q,OriginalActorPawnTickStats& out,
    const Provider& provider,Args&&... args) {
    if(!provider)throw std::invalid_argument("Original Pawn Tick continuation lacks a reached provider");
    if(!q.work)throw std::invalid_argument("Original Pawn Tick continuation lacks shared work authority");
    q.work();++out.steps;
    return provider(std::forward<Args>(args)...);
}
} // namespace ActorPawnTickDetail

// Genuine AActor.Tick after the shared MAIN/BLEND animation exit103A1FD2.
// Entry stasis/owner/parity/capture/animation remain the caller's responsibility.
// The captured Pawn and entry Role2 Boolean NEVER refresh from callback-mutated
// flags/Role/Class; later source Role/Physics reads are independently fresh.
// All normal returns in this source region join the genuine return1 epilogue;
// owner-deferral return0 belongs BEFORE this helper, not to a fabricated tail.
// Provider errors retain the same caller's journal, with no local undo.
inline OriginalActorPawnTickStats AdvanceOriginalActorPawnTick(std::uintptr_t actor,
    const OriginalActorPawnContext& captured,float wholeDelta,std::int32_t tickType,
    const OriginalActorPawnTickQueries& q) {
    if(!actor||(captured.pawn&&captured.pawn!=actor)||(!captured.pawn&&captured.suppressAnimEnd))
        throw std::invalid_argument("Original Pawn Tick continuation has an invalid captured source context");
    using ActorPawnTickDetail::Call;
    using namespace NativeScalar;
    OriginalActorPawnTickStats out;
    const auto finish=[&](){out.returned=1;return out;};
    const auto castPlayer=[&]() {
        ++out.playerCasts;const auto playerPawn=Call(q,out,q.playerPawnCast,actor);
        if(playerPawn&&playerPawn!=actor)
            throw std::runtime_error("Original Pawn Tick PlayerPawn cast changed its retained source");
        return playerPawn;
    };
    const auto selectedTick=[&]() {
        if(Call(q,out,q.isProbingTick,actor)){++out.tickCalls;Call(q,out,q.tick,actor,wholeDelta);}
    };
    const auto stateAndTimer=[&]() {
        ++out.stateCalls;Call(q,out,q.processState,actor,wholeDelta);
        ++out.timerCalls;Call(q,out,q.timer,actor,wholeDelta);
    };
    if(captured.suppressAnimEnd) {
        out.simulatedBranch=true;
        Call(q,out,q.simulatedPawnMovement,actor,captured.pawn,wholeDelta);
        selectedTick(); // No ordinary state/timer/LifeSpan/physics on this arm.
    } else {
        const auto remote=Call(q,out,q.remoteRole,actor);
        const auto role=Call(q,out,q.role,actor); // Always read, even RemoteRole3.
        if(remote==3u) {
            out.remoteRoleBranch=true;
            if(role==4u) {
                if(captured.pawn) {
                    if(const auto playerPawn=castPlayer())Call(q,out,q.remotePlayerViewCopy,actor,playerPawn);
                }
                stateAndTimer();
            }
        } else if(role<2u) {
            out.lowRoleBranch=true;
            if(Call(q,out,q.physics,actor)==2u) {
                ++out.physicsCalls;Call(q,out,q.performPhysics,actor,wholeDelta);
            }
        } else {
            bool localPlayer=false;
            if(captured.pawn) {
                if(const auto playerPawn=castPlayer()) {
                    const auto player=Call(q,out,q.player,playerPawn);
                    if(player) {
                        localPlayer=true;out.localPlayerBranch=true;
                        const auto result=Call(q,out,q.localPlayerTick,actor,playerPawn,player,wholeDelta,tickType);
                        if(result==OriginalPawnLocalTickResult::ReturnOne) {
                            out.localPlayerReturned=true;return finish();
                        }
                        if(result!=OriginalPawnLocalTickResult::ContinueState)
                            throw std::runtime_error("Original Pawn local-player service returned an invalid continuation");
                    }
                }
            }
            if(!localPlayer) {
                if(tickType==1){out.tickTypeSkipped=true;return finish();}
                selectedTick();
            }
            stateAndTimer();
            ++out.lifeSpanReads;const auto initialLife=Load(Call(q,out,q.lifeSpan,actor));
            // Literal native C3 test skips signed zero AND unordered NaN.
            if(Compare(181u,initialLife,Memory(0.0f))) {
                ++out.lifeSpanReads;
                const auto difference=Subtract(Load(Call(q,out,q.lifeSpan,actor)),Memory(wholeDelta));
                ++out.lifeSpanWrites;Call(q,out,q.writeLifeSpan,actor,StoreFloat(difference));
                // The native Float FST does not pop/reload this PC64 result.
                if(Compare(178u,difference,Memory(0.0001))) {
                    ++out.expiredCalls;Call(q,out,q.expired,actor);
                    ++out.destroyCalls;Call(q,out,q.destroy,actor,std::int32_t{});
                    out.expired=true;return finish();
                }
            }
            if(Call(q,out,q.physics,actor)!=0u&&Call(q,out,q.role,actor)!=3u) {
                ++out.physicsCalls;Call(q,out,q.performPhysics,actor,wholeDelta);
            }
        }
    }
    ++out.networkTailCalls;Call(q,out,q.networkTail,actor,captured.pawn);
    if(!captured.pawn){out.nullPawnTail=true;return finish();}
    if(Call(q,out,q.pawnIsPlayer,captured.pawn)&&Call(q,out,q.role,actor)>=3u) {
        if(Call(q,out,q.pawnViewTarget,captured.pawn)) {
            ++out.eyeHeightCalls;Call(q,out,q.updateEyeHeight,captured.pawn,wholeDelta);
        } else {
            ++out.viewCopies;Call(q,out,q.copyRotationToView,actor,captured.pawn);
        }
    }
    const auto weapon=Call(q,out,q.weapon,captured.pawn);
    if(weapon){++out.weaponMoves;Call(q,out,q.farMoveWeapon,actor,weapon);}
    if(Call(q,out,q.role,actor)==4u&&tickType==2) {
        ++out.longTailCalls;Call(q,out,q.longPawnTail,actor,captured.pawn,wholeDelta);
    } else out.shortPawnTail=true;
    return finish();
}

} // namespace QuestVr
