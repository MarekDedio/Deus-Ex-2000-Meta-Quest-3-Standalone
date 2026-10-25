#pragma once

#include "quest_script_random_vector.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

// Original Engine.dll AActor.InStasis1036BFE0..1036C06C. Native XLevel,
// retained Model and the named Actor.Level are distinct caller-owned edges.
// No renderer-camera distance or inferred serialized timestamp is admitted.
struct OriginalActorStasisQueries {
    std::function<float(std::uintptr_t)> lastRenderTime;
    std::function<double(std::uintptr_t)> nativeTime;
    // The original reads the packed flags once, retaining both bits.
    std::function<std::uint32_t(std::uintptr_t)> stasisFlags;
    std::function<std::uint8_t(std::uintptr_t)> physics,zoneNumber,netMode;
    std::function<float(std::uintptr_t,std::uint8_t)> zoneLastRenderTime;
    std::function<float(std::uintptr_t)> distanceFromPlayer;
    std::function<void()> work;
};
struct OriginalActorStasisStats {
    bool inStasis{},physicsRead{},zoneRead{},distanceRead{},netModeRead{};
    std::size_t steps{},nativeTimeReads{};
};
namespace ActorStasisDetail {
template<class Provider,class... Args>
inline auto Call(const OriginalActorStasisQueries& q,OriginalActorStasisStats& s,
    const Provider& provider,Args&&... args) {
    if(!provider)throw std::runtime_error("Original Actor stasis lacks a reached provider");
    if(q.work)q.work();
    ++s.steps;
    return provider(std::forward<Args>(args)...);
}
inline bool OlderThanFive(double now,float last) {
    // Native test AH,41 is ordered strict >, including unordered false.
    // Deal with infinities before decoding finite PC64 dyadics. There is no
    // binary64/Float subtraction spill between FSUBR and FCOMP Double5.
    if(std::isnan(now)||std::isnan(last))return false;
    if(std::isinf(now)||std::isinf(last)) {
        if(std::isinf(now)&&std::isinf(last)&&std::signbit(now)==std::signbit(last))return false;
        return (std::isinf(now)&&now>0.0)||(std::isinf(last)&&last<0.0f);
    }
    using namespace NativeWindowClipDetail;
    return Compare64(Add64(ScriptRandom::VectorDetail::DecodeDouble(now),
        Negate(ActorTimerDetail::Decode(last))),ActorTimerDetail::Dyadic{5u,0,false})>0;
}
} // namespace ActorStasisDetail

inline OriginalActorStasisStats QueryOriginalActorStasis(std::uintptr_t actor,
    const OriginalActorStasisQueries& q) {
    if(!actor)throw std::invalid_argument("Original Actor stasis requires a nonnull Actor");
    OriginalActorStasisStats s;
    const auto read=[&](const auto& provider,auto... args){return ActorStasisDetail::Call(q,s,provider,args...);};
    const float last=read(q.lastRenderTime,actor);
    const double now=read(q.nativeTime,actor);++s.nativeTimeReads;
    if(!ActorStasisDetail::OlderThanFive(now,last))return s;
    const auto flags=read(q.stasisFlags,actor);
    if(!(flags&0x00200000u))return s;
    if(!(flags&0x00400000u)) {
        const auto physics=read(q.physics,actor);s.physicsRead=true;
        if(physics!=0u&&physics!=5u)return s;
    }
    const auto zone=read(q.zoneNumber,actor);
    if(zone>=64u)throw std::runtime_error("Original Actor stasis zone exceeds native Model's 64 entries");
    const float zoneLast=read(q.zoneLastRenderTime,actor,zone);s.zoneRead=true;
    const double zoneNow=read(q.nativeTime,actor);++s.nativeTimeReads;
    if(!ActorStasisDetail::OlderThanFive(zoneNow,zoneLast)) {
        const float distance=read(q.distanceFromPlayer,actor);s.distanceRead=true;
        if(!(distance>1200.0))return s;
    }
    const auto mode=read(q.netMode,actor);s.netModeRead=true;
    s.inStasis=mode==0u;
    return s;
}
} // namespace QuestVr
