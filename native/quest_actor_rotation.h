#pragma once

#include "quest_actor_line_trace.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>

namespace QuestVr {

struct OriginalActorRotationQueries {
    // One actual packed native read. Only bits0x20(bFixedRotationDir) and
    //0x40(bRotateToDesired) are consumed; other bits have no effect here.
    std::function<std::uint8_t(std::uintptr_t)> rotationFlags;
    // Raw signed INT channels: Pitch=0, Yaw=1, Roll=2. These are not degrees.
    std::function<std::int32_t(std::uintptr_t,unsigned)> rotation,desiredRotation,rotationRate;
    std::function<std::uintptr_t(std::uintptr_t)> level;
    std::function<std::int32_t(std::uintptr_t,std::uintptr_t,PortableModelVec3,
        std::array<std::int32_t,3u>,OriginalActorLineHit&,std::array<std::int32_t,4u>)> moveActor;
    // Original FName325/probe25 and the actual selected no-argument event.
    // The provider keeps real ProcessEvent eligibility and the caller's root.
    std::function<bool(std::uintptr_t)> isProbingEndedRotation;
    std::function<void(std::uintptr_t)> endedRotation;
    std::function<void()> work;
};

struct OriginalActorRotationStats {
    std::size_t steps{},flagReads{},rotationReads{},desiredReads{},rateReads{},
        conversions{},fixedTurns{},moveCalls{},probeCalls{},eventCalls{};
    bool inactive{},alreadyDesired{};
    // Captured only after both entry gates. A zero-Delta axis keeps its raw
    // upper16 bits; only an actual fixedTurn result is normalized to low16.
    std::array<std::int32_t,3u> proposed{},delta{};
};

namespace ActorRotationDetail {
inline std::int32_t Add(std::int32_t a,std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b)));
}
inline std::int32_t Sub(std::int32_t a,std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b)));
}
inline std::int32_t Negate(std::int32_t value) {return Sub(0,value);}
inline std::int32_t Low16(std::int32_t value) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(value)&0xffffu);
}
template<class Provider,class... Arguments>
inline auto Call(const OriginalActorRotationQueries& queries,OriginalActorRotationStats& stats,
    const Provider& provider,Arguments&&... arguments) {
    if(!provider)throw std::invalid_argument("Original Actor rotation lacks an active native provider");
    if(queries.work)queries.work();
    ++stats.steps;
    return provider(std::forward<Arguments>(arguments)...);
}
} // namespace ActorRotationDetail

// Original Engine.dll fixedTurn103C9DC0..103C9EDE. Every native integer
// add/subtract/negate wraps to32 bits, including abs(INT_MIN)==INT_MIN.
// The pure flags argument represents ONE already captured original flag read;
// callers must not fetch flags at all when an original Delta0 path is skipped.
inline std::int32_t OriginalActorFixedTurn(std::int32_t current,std::int32_t desired,
    std::int32_t delta,std::uint8_t flags) {
    using namespace ActorRotationDetail;
    const auto c=Low16(current);
    if(!delta)return c;
    auto d=Low16(desired);
    if(flags&0x20u) {
        if(!(flags&0x40u))return Low16(Add(c,delta));
        std::int32_t distance{},step{};
        if(delta>0) {
            if(c>d)d+=65536;
            distance=d-c;
            step=delta<distance?delta:distance;
        }else {
            const auto extended=c<d?c+65536:c;
            distance=d-extended;
            step=delta>distance?delta:distance;
        }
        return Low16(Add(c,step));
    }
    if(!(flags&0x40u))return c;
    const auto magnitude=delta<0?Negate(delta):delta;
    std::int32_t distance{};bool increase{};
    if(c>d) {
        distance=c-d;
        if(distance>=32768){distance=d-c+65536;increase=true;}
    }else {
        distance=d-c;
        if(distance<32768)increase=true;
        else distance=c-d+65536;
    }
    const auto step=distance<magnitude?distance:magnitude;
    return Low16(increase?Add(c,step):Sub(c,step));
}

// Original signed INT FILD * Float FMUL -> MSVCRT._ftol -> low32 EAX.
// INT32*Float32 has at most55 significant bits, hence its PC64 product is
// exact. Decode that product as unsigned integer mantissa/exponent instead of
// rounding through host Float/Double or assuming host long-double precision.
// Reached nonfinite/CRT-indefinite or out-of-signed64 results explicitly refuse.
inline std::int32_t OriginalActorRotationDelta(std::int32_t rate,float deltaSeconds) {
    static_assert(sizeof(float)==4u && std::numeric_limits<float>::is_iec559 &&
        std::numeric_limits<float>::digits==24);
    const auto bits=std::bit_cast<std::uint32_t>(deltaSeconds);
    const auto exponentBits=(bits>>23u)&0xffu;
    if(exponentBits==0xffu)
        throw std::invalid_argument("Original Actor rotation reached nonfinite rate/time conversion");
    const auto fraction=bits&0x7fffffu;
    const auto significand=exponentBits?(fraction|0x800000u):fraction;
    const auto rateBits=static_cast<std::uint32_t>(rate);
    const auto rateMagnitude=rate<0?static_cast<std::uint32_t>(0u-rateBits):rateBits;
    std::uint64_t magnitude=std::uint64_t(rateMagnitude)*significand;
    if(!magnitude)return 0;
    const bool negative=(rate<0)!=((bits&0x80000000u)!=0u);
    const auto limit=negative?(std::uint64_t{1u}<<63u):
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const int exponent=static_cast<int>(exponentBits?exponentBits:1u)-150;
    if(exponent>=0) {
        const auto shift=static_cast<unsigned>(exponent);
        if(shift>=64u || magnitude>(limit>>shift))
            throw std::invalid_argument("Original Actor rotation conversion is outside signed64");
        magnitude<<=shift;
    }else {
        const auto shift=static_cast<unsigned>(-exponent);
        magnitude=shift>=64u?std::uint64_t{}:magnitude>>shift;
    }
    // Truncation is toward zero; only low32 of the signed64 CRT result is
    // consumed, so an ordinary signed32 cast/clamp is not the native contract.
    auto low=static_cast<std::uint32_t>(magnitude);
    if(negative)low=static_cast<std::uint32_t>(0u-low);
    return std::bit_cast<std::int32_t>(low);
}

// Owned GOTY AActor::physicsRotation103CA1E0..103CA45C. This is not APawn's
// different method, the performPhysics low16-rate gate or a world scheduler.
// Actual mutation happens only through MoveActor and selected EndedRotation in
// the SAME caller journal. No local undo, direct pose store, deletion/Physics
// guard, flag/rate clearing or successful empty movement/event is introduced.
inline OriginalActorRotationStats RotateOriginalActor(std::uintptr_t actor,float wholeDelta,
    const OriginalActorRotationQueries& queries) {
    if(!actor)throw std::invalid_argument("Original Actor rotation requires a nonnull Actor");
    using ActorRotationDetail::Call;
    OriginalActorRotationStats stats;
    const auto flags=[&] {
        ++stats.flagReads;return Call(queries,stats,queries.rotationFlags,actor);
    };
    const auto rotation=[&](unsigned axis) {
        ++stats.rotationReads;return Call(queries,stats,queries.rotation,actor,axis);
    };
    const auto desired=[&](unsigned axis) {
        ++stats.desiredReads;return Call(queries,stats,queries.desiredRotation,actor,axis);
    };
    const auto rate=[&](unsigned axis) {
        ++stats.rateReads;return Call(queries,stats,queries.rotationRate,actor,axis);
    };
    const auto sameActualDesired=[&] {
        for(unsigned axis=0u;axis<3u;++axis) {
            // Explicit sequencing: actual Rotation read precedes Desired.
            const auto current=rotation(axis);
            if(current!=desired(axis))return false;
        }
        return true;
    };
    const auto initialFlags=flags();const bool initialRotate=(initialFlags&0x40u)!=0u;
    if(!initialRotate && !(initialFlags&0x20u)){stats.inactive=true;return stats;}
    if(initialRotate && sameActualDesired()){stats.alreadyDesired=true;return stats;}

    // Roll rate is read BEFORE current rotation capture. All three conversions
    // finish before Yaw/Pitch/Roll updates; do not interleave per-axis helpers.
    const auto rollRate=rate(2u);
    for(unsigned axis=0u;axis<3u;++axis)stats.proposed[axis]=rotation(axis);
    ++stats.conversions;stats.delta[2u]=OriginalActorRotationDelta(rollRate,wholeDelta);
    const auto yawRate=rate(1u);
    ++stats.conversions;stats.delta[1u]=OriginalActorRotationDelta(yawRate,wholeDelta);
    const auto pitchRate=rate(0u);
    ++stats.conversions;stats.delta[0u]=OriginalActorRotationDelta(pitchRate,wholeDelta);

    constexpr std::array<unsigned,3u> axisOrder{1u,0u,2u};
    for(const auto axis:axisOrder) {
        if(!stats.delta[axis])continue;
        const bool rotateToDesired=axis==1u?initialRotate:(flags()&0x40u)!=0u;
        if(rotateToDesired && desired(axis)==stats.proposed[axis])continue;
        // Desired is reread for the reached argument, then fixedTurn performs
        // its own fresh packed flag read. Do not reuse this axis's gate word.
        const auto target=desired(axis);
        ++stats.fixedTurns;
        const auto turnFlags=flags();
        stats.proposed[axis]=OriginalActorFixedTurn(stats.proposed[axis],target,stats.delta[axis],turnFlags);
    }

    bool changed{};
    for(unsigned axis=0u;axis<3u;++axis) {
        if(stats.proposed[axis]!=rotation(axis)){changed=true;break;}
    }
    if(changed) {
        OriginalActorLineHit hit;
        const auto currentLevel=Call(queries,stats,queries.level,actor);
        if(!currentLevel)throw std::invalid_argument("Original Actor rotation reached null native Level");
        ++stats.moveCalls;
        static_cast<void>(Call(queries,stats,queries.moveActor,currentLevel,actor,PortableModelVec3{},
            stats.proposed,hit,std::array<std::int32_t,4u>{}));
        // MoveActor's boolean and hit packet are deliberately not consumed.
    }
    if(!(flags()&0x40u) || !sameActualDesired())return stats;
    ++stats.probeCalls;
    if(Call(queries,stats,queries.isProbingEndedRotation,actor)) {
        ++stats.eventCalls;Call(queries,stats,queries.endedRotation,actor);
    }
    return stats;
}

} // namespace QuestVr
