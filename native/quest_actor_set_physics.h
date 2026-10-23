#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

enum class OriginalPhysicsVector { Velocity,Acceleration };
struct OriginalActorPhysicsQueries {
    std::function<std::uint8_t(std::uintptr_t)> physics;
    std::function<void(std::uintptr_t,std::uint8_t)> writePhysics;
    std::function<std::uintptr_t(std::uintptr_t)> base;
    std::function<void(std::uintptr_t,std::uintptr_t)> setSupportBase;
    std::function<void(std::uintptr_t)> findBase;
    std::function<void(std::uintptr_t,std::uintptr_t,std::int32_t)> setBase;
    // Raw positive-zero scalar stores, not read/modify/write arithmetic on
    // old vectors. Overwritten NaNs/infinities do not become active inputs.
    std::function<void(std::uintptr_t,OriginalPhysicsVector,unsigned,std::uint32_t)> writeVectorWord;
    // Borrow the actual caller root. No fresh work allowance or transaction.
    std::function<void()> work;
};
struct OriginalActorPhysicsStats {
    std::size_t steps{},physicsReads{},baseReads{},physicsWrites{},supportCalls{},findCalls{},detachCalls{},zeroWords{};
    bool unchanged{};
};
namespace ActorPhysicsDetail {
template<class Function,class... Arguments>
inline auto Call(const OriginalActorPhysicsQueries& queries,OriginalActorPhysicsStats& stats,
    const Function& function,Arguments... arguments) {
    if(!function)throw std::invalid_argument("Original SetPhysics lacks an active native provider");
    if(queries.work)queries.work();
    ++stats.steps;
    return function(std::forward<Arguments>(arguments)...);
}
} // namespace ActorPhysicsDetail

// Owned GOTY setPhysics103C95F0..103C96E9, not the pinned assignment-only
// adapter. All argument evaluation precedes this call. Callback reentry is
// synchronous in the SAME root; no partial-effects undo or state normalization
// belongs here. This does not implement performPhysics, falling or world Tick.
inline OriginalActorPhysicsStats SetOriginalActorPhysics(std::uintptr_t actor,std::uint8_t requested,
    std::uintptr_t floor,const OriginalActorPhysicsQueries& queries) {
    if(!actor)throw std::invalid_argument("Original SetPhysics requires a nonnull Actor");
    using ActorPhysicsDetail::Call;
    OriginalActorPhysicsStats stats;
    const auto readPhysics=[&] {++stats.physicsReads;return Call(queries,stats,queries.physics,actor);};
    const auto readBase=[&] {++stats.baseReads;return Call(queries,stats,queries.base,actor);};
    if(readPhysics()==requested){stats.unchanged=true;return stats;}
    ++stats.physicsWrites;Call(queries,stats,queries.writePhysics,actor,requested);
    if(requested==0u || requested==1u || requested==5u || requested==7u || requested==10u) {
        if(floor) {
            if(readBase()!=floor){++stats.supportCalls;Call(queries,stats,queries.setSupportBase,actor,floor);}
        } else {++stats.findCalls;Call(queries,stats,queries.findBase,actor);}
    } else if(readBase()) {
        ++stats.detachCalls;Call(queries,stats,queries.setBase,actor,std::uintptr_t{},std::int32_t{1});
    }
    // BaseChange/SupportActor can replace Physics, even delete the source.
    // Original still rereads it and performs these literal mode tests.
    const auto current=readPhysics();
    if(current==0u || current==5u) {
        const auto zero=[&](OriginalPhysicsVector vector,unsigned channel) {
            ++stats.zeroWords;Call(queries,stats,queries.writeVectorWord,actor,vector,channel,std::uint32_t{});
        };
        zero(OriginalPhysicsVector::Velocity,0u);
        zero(OriginalPhysicsVector::Velocity,1u);
        zero(OriginalPhysicsVector::Acceleration,0u);
        zero(OriginalPhysicsVector::Velocity,2u);
        zero(OriginalPhysicsVector::Acceleration,1u);
        zero(OriginalPhysicsVector::Acceleration,2u);
    }
    return stats;
}

} // namespace QuestVr
