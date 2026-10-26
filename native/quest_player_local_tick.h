#pragma once

#include "quest_actor_pawn_tick.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

// Original AActor.Tick local-player slice103A2577..103A2641. The enclosing
// Pawn phase has already selected a real nonnull PlayerPawn/Player. Providers
// own canonical native identities, checked script events and the caller root;
// this coordinator supplies neither replacement input nor rollback itself.
struct OriginalPlayerLocalTickQueries {
    std::function<bool(std::uintptr_t)> cameraCast;
    std::function<std::uint32_t(std::uintptr_t)> cameraShowFlags;
    // The original reloads source.Player before BOTH virtual+54 calls. A
    // script callback may change the field; do not retain the earlier pointer.
    std::function<std::uintptr_t(std::uintptr_t)> player;
    std::function<void(std::uintptr_t,float)> readInput,playerInput,playerTick;
    std::function<bool(std::uintptr_t)> hasNetDriver;
    // Owns the retained actual driver's ServerConnection conditional/copy.
    // Reached nonnull network work cannot be replaced with absent callbacks.
    std::function<void(std::uintptr_t)> networkTail;
    std::function<void()> work;
};

struct OriginalPlayerLocalTickStats {
    OriginalPawnLocalTickResult result{OriginalPawnLocalTickResult::ContinueState};
    std::size_t steps{},cameraCasts{},cameraFlagReads{},playerReads{},readInputCalls{},
        playerInputCalls{},playerTickCalls{},driverReads{},networkTailCalls{};
    std::uintptr_t prePlayer{},postPlayer{};
    bool cameraEarlyReturn{},networkBranch{};
};

namespace PlayerLocalTickDetail {
template<class Provider,class... Args>
inline auto Call(const OriginalPlayerLocalTickQueries& q,OriginalPlayerLocalTickStats& out,
    const Provider& provider,Args&&... args) {
    if(!provider)throw std::invalid_argument("Original local Player Tick lacks a reached provider");
    if(!q.work)throw std::invalid_argument("Original local Player Tick lacks shared work authority");
    q.work();++out.steps;
    return provider(std::forward<Args>(args)...);
}
} // namespace PlayerLocalTickDetail

inline OriginalPlayerLocalTickStats AdvanceOriginalPlayerLocalTick(std::uintptr_t actor,
    std::uintptr_t playerPawn,std::uintptr_t initialPlayer,float wholeDelta,
    const OriginalPlayerLocalTickQueries& q) {
    if(!actor||actor!=playerPawn||!initialPlayer)
        throw std::invalid_argument("Original local Player Tick requires its actual nonnull source/Pawn/Player");
    using PlayerLocalTickDetail::Call;
    OriginalPlayerLocalTickStats out;
    ++out.cameraCasts;
    if(Call(q,out,q.cameraCast,actor)) {
        ++out.cameraFlagReads;
        // Original +5FC is PlayerPawn.ShowFlags Int, NOT bUpdatePosition.
        // The actual ACamera-only bit0800 clear arm returns1 before input.
        if(!(Call(q,out,q.cameraShowFlags,actor)&0x800u)) {
            out.cameraEarlyReturn=true;out.result=OriginalPawnLocalTickResult::ReturnOne;
            return out;
        }
    }
    ++out.playerReads;out.prePlayer=Call(q,out,q.player,actor);
    if(!out.prePlayer)throw std::runtime_error("Original local Player Tick reached null pre-input Player");
    ++out.readInputCalls;Call(q,out,q.readInput,out.prePlayer,wholeDelta);
    ++out.playerInputCalls;Call(q,out,q.playerInput,actor,wholeDelta);
    ++out.playerTickCalls;Call(q,out,q.playerTick,actor,wholeDelta);
    ++out.playerReads;out.postPlayer=Call(q,out,q.player,actor);
    if(!out.postPlayer)throw std::runtime_error("Original local Player Tick reached null post-input Player");
    ++out.readInputCalls;Call(q,out,q.readInput,out.postPlayer,-1.0f);
    ++out.driverReads;
    if(Call(q,out,q.hasNetDriver,actor)) {
        out.networkBranch=true;++out.networkTailCalls;Call(q,out,q.networkTail,actor);
    }
    // Joins real ProcessState/timer103A2690. There is no ordinary Tick probe
    // or tickType1 skip in this slice, and the native post-input call is not
    // an unconditional scope cleanup after a thrown script/native callback.
    return out;
}

} // namespace QuestVr
