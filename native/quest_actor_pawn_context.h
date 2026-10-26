#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

struct OriginalActorPawnContextQueries {
    std::function<bool(std::uintptr_t)> pawnFlag;
    // Actual UObject Class and UClass Super pointers, not a display-name test
    // or an inherited RF_Native/ClassFlags approximation. The caller validates
    // each retained metadata identity within its current root/work authority.
    std::function<std::uintptr_t(std::uintptr_t)> actorClass,superClass;
    // Lazily authenticate the exact native APawn UClass literal1058C660.
    // This metadata service is not demanded by flag-clear/null-Class paths.
    std::function<std::uintptr_t()> nativePawnClass;
    std::function<std::uint8_t(std::uintptr_t)> role;
    std::function<void()> work;
};
struct OriginalActorPawnContext {
    std::uintptr_t pawn{};
    bool suppressAnimEnd{};
    std::size_t steps{},flagReads{},classReads{},nativeClassReads{},superReads{},roleReads{};
};
namespace ActorPawnContextDetail {
template<class Provider,class... Args>
inline auto Call(const OriginalActorPawnContextQueries& q,OriginalActorPawnContext& result,
    const Provider& provider,Args&&... args) {
    if(!provider)throw std::invalid_argument("Original Actor Pawn context lacks a reached provider");
    if(q.work)q.work();
    ++result.steps;
    return provider(std::forward<Args>(args)...);
}
} // namespace ActorPawnContextDetail

// Owned AActor.Tick103A1BA8..103A1BF0, AFTER the caller's real stasis/owner/
// parity prefix and BEFORE shared animation. bIsPawn alone does not cast, and
// actual Pawn ancestry alone does not enable context when the flag is clear.
// Capture once: later animation/event mutations never refresh this pointer or
// Role2 AnimEnd-suppression flag. This does not implement ScriptedPawn.Tick,
// simulated-Pawn movement, the Pawn suffix, or APawn.performPhysics.
// Provider failures propagate in the SAME caller root; no local undo exists.
inline OriginalActorPawnContext CaptureOriginalActorPawnContext(std::uintptr_t actor,
    const OriginalActorPawnContextQueries& q) {
    if(!actor)throw std::invalid_argument("Original Actor Pawn context requires a nonnull Actor");
    OriginalActorPawnContext result;
    using ActorPawnContextDetail::Call;
    ++result.flagReads;
    if(!Call(q,result,q.pawnFlag,actor))return result;
    ++result.classReads;
    auto current=Call(q,result,q.actorClass,actor);
    if(!current)return result;
    ++result.nativeClassReads;
    const auto target=Call(q,result,q.nativePawnClass);
    if(!target)throw std::runtime_error("Original Actor Pawn context lost its actual native Pawn UClass");
    std::array<std::uintptr_t,64u> visited{};std::size_t depth{};
    for(;;) {
        if(current==target) {
            result.pawn=actor;
            ++result.roleReads;
            result.suppressAnimEnd=Call(q,result,q.role,actor)==2u;
            return result;
        }
        if(depth==visited.size()||std::find(visited.begin(),visited.begin()+depth,current)!=visited.begin()+depth)
            throw std::runtime_error("Original Actor Pawn context Class ancestry cycles or exceeds its bound");
        visited[depth++]=current;
        ++result.superReads;
        current=Call(q,result,q.superClass,current);
        if(!current)return result;
    }
}

} // namespace QuestVr
