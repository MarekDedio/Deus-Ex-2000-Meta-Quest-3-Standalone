#pragma once

#include "quest_script_random_vector.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {
struct OriginalLevelPlayerDistanceQueries {
    std::function<std::uint8_t(std::uintptr_t)> netMode;
    // Native Level.Engine -> Engine.Client -> Viewports[0].Actor. A missing
    // active dereference refuses; a genuine null viewport Actor skips.
    std::function<std::uintptr_t(std::uintptr_t)> firstViewportActor;
    std::function<bool(std::uintptr_t)> isPlayerPawn,hasPlayer;
    std::function<std::int32_t(std::uintptr_t)> firstDynamic,actorCount;
    std::function<std::uintptr_t(std::uintptr_t,std::int32_t)> actorSlot;
    std::function<float(std::uintptr_t,unsigned)> location;
    std::function<void(std::uintptr_t,float)> writeDistance;
    std::function<void()> work;
};
struct OriginalLevelPlayerDistanceStats {
    std::size_t steps{},visits{},nullSlots{},writes{};
    std::uintptr_t player{};
    std::int32_t firstDynamic{};
    std::array<float,3u> capturedLocation{};
    bool networkSkipped{},noActor{},notPlayerPawn{},noPlayer{};
};
namespace LevelPlayerDistanceDetail {
template<class Provider,class... Args>
inline auto Call(const OriginalLevelPlayerDistanceQueries& q,OriginalLevelPlayerDistanceStats& s,
    const Provider& provider,Args&&... args) {
    if(!provider)throw std::runtime_error("Original Level player-distance pass lacks a reached provider");
    if(q.work)q.work();
    ++s.steps;return provider(std::forward<Args>(args)...);
}
inline float Distance(const std::array<float,3u>& actor,const std::array<float,3u>& player) {
    using namespace NativeWindowClipDetail;
    using ActorTimerDetail::Decode;
    using ActorTimerDetail::Store32;
    using ModelFastLineDetail::Multiply64;
    std::array<ActorTimerDetail::Dyadic,3u> delta{};
    for(unsigned channel=3u;channel--;) {
        if(!std::isfinite(actor[channel])||!std::isfinite(player[channel]))
            throw std::runtime_error("Original Level player-distance active nonfinite arithmetic is not implemented");
        delta[channel]=Add64(Decode(actor[channel]),Negate(Decode(player[channel])));
    }
    // Engine103A5661..74: (Y*Y + Z*Z) + X*X, binary64 ARGUMENT spill.
    const auto sum=Add64(Add64(Multiply64(delta[1u],delta[1u]),Multiply64(delta[2u],delta[2u])),
        Multiply64(delta[0u],delta[0u]));
    const double argument=ScriptRandom::VectorDetail::StoreDouble(sum);
    if(!sum.significand)return 0.0f;
    const auto root=ScriptRandom::VectorDetail::SqrtStoredDouble64(argument);
    // FST Double at5689 does NOT pop/reload the returned PC64 root before
    // FSTP Float at5692. Validate that spill but retain the extended operand.
    static_cast<void>(ScriptRandom::VectorDetail::StoreDouble(root));
    return Store32(root);
}
} // namespace LevelPlayerDistanceDetail

// Arithmetic and live native array protocol at Engine103A55C1..103A5696.
// Caller owns native level/client/viewport/Player lifetimes, journal and delta.
// This is a committing phase service, NOT a substitute for full Level.Tick.
inline OriginalLevelPlayerDistanceStats AdvanceOriginalLevelPlayerDistance(std::uintptr_t level,
    const OriginalLevelPlayerDistanceQueries& q,std::size_t maximumSlots=65'536u) {
    if(!level||maximumSlots>65'536u)
        throw std::invalid_argument("Original Level player-distance requires a nonnull bounded Level");
    OriginalLevelPlayerDistanceStats s;
    const auto read=[&](const auto& p,auto... args){return LevelPlayerDistanceDetail::Call(q,s,p,args...);};
    if(read(q.netMode,level)!=0u){s.networkSkipped=true;return s;}
    s.player=read(q.firstViewportActor,level);
    if(!s.player){s.noActor=true;return s;}
    if(!read(q.isPlayerPawn,s.player)){s.notPlayerPawn=true;return s;}
    if(!read(q.hasPlayer,s.player)){s.noPlayer=true;return s;}
    s.firstDynamic=read(q.firstDynamic,level); // BEFORE the native Location copy.
    if(s.firstDynamic<0||static_cast<std::size_t>(s.firstDynamic)>maximumSlots)
        throw std::runtime_error("Original Level first-dynamic distance index is outside its safe array policy");
    for(unsigned channel=0u;channel<3u;++channel)s.capturedLocation[channel]=read(q.location,s.player,channel);
    for(std::int32_t index=s.firstDynamic;;++index) {
        const auto count=read(q.actorCount,level);
        if(count<0||static_cast<std::size_t>(count)>maximumSlots)
            throw std::runtime_error("Original Level player-distance live Actor count exceeds its safe array policy");
        if(index>=count)break;
        ++s.visits;
        const auto actor=read(q.actorSlot,level,index);
        if(!actor){++s.nullSlots;continue;}
        std::array<float,3u> location{};
        for(unsigned channel=3u;channel--;)location[channel]=read(q.location,actor,channel);
        const float distance=LevelPlayerDistanceDetail::Distance(location,s.capturedLocation);
        // Original reloads Actors data+index AFTER appSqrt. Do not store via
        // the retained source Actor if that native slot has changed.
        const auto destination=read(q.actorSlot,level,index);
        if(!destination)throw std::runtime_error("Original Level player-distance destination became a null Actor");
        read(q.writeDistance,destination,distance);++s.writes;
    }
    return s;
}
} // namespace QuestVr
