#pragma once

#include "quest_script_random_vector.h"

#include <array>
#include <cstddef>
#include <optional>

namespace QuestVr {

struct OriginalNoiseCache {
    float time{}, loudness{};
    PortableModelVec3 spot{};
};
struct OriginalNoiseCacheDecision {
    bool suppressed{};
    std::optional<std::size_t> replacement;
};
struct OriginalCanHearDecision {
    // An eligible noncoincident listener still needs the ORIGINAL directed
    // Model.FastLineCheck(End=noise,Start=listener). This is not final hearing.
    bool eligible{}, needsBsp{}, writeStimulus{};
    float stimulus{};
};

namespace OriginalMakeNoiseDetail {
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Store32;
using ScriptRandom::VectorDetail::DecodeDouble;
using ModelFastLineDetail::Multiply64;
using ModelFastLineDetail::Divide64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;

inline void Finite(const float value) {
    if (!std::isfinite(value)) throw std::invalid_argument("Original MakeNoise requires finite active Float data");
}
inline void Finite(const PortableModelVec3& value) {
    Finite(value.x); Finite(value.y); Finite(value.z);
}
inline Dyadic DistanceSquared(const PortableModelVec3& target,const PortableModelVec3& source) {
    Finite(target); Finite(source);
    const auto x=Add64(Decode(target.x),Negate(Decode(source.x)));
    const auto y=Add64(Decode(target.y),Negate(Decode(source.y)));
    const auto z=Add64(Decode(target.z),Negate(Decode(source.z)));
    // Retail MakeNoise/CanHear retain the subtractions and add x/y/z, NOT
    // CheckHearSound's z/y/x order. Cache distances are never Float-spilled.
    return Add64(Add64(Multiply64(x,x),Multiply64(y,y)),Multiply64(z,z));
}
inline constexpr double RecentWindow=std::bit_cast<double>(std::uint64_t{0x3fc999999999999aull});
inline constexpr double StaleWindow=std::bit_cast<double>(std::uint64_t{0x3fc70a3d70a3d70aull});
inline constexpr double SuppressionFactor=std::bit_cast<double>(std::uint64_t{0x3feccccccccccccdull});
} // namespace OriginalMakeNoiseDetail

// Scalar cache phase of AActor.MakeNoise103bfb34..103bfde8. No UObject access,
// cache mutation, clock advance, recipient enumeration or callback delivery.
// Unvisited cache fields are not evaluated: first-cache suppression really
// returns before cache2. A replacement guarantees that Float(now) is finite;
// the caller must write its time/spot/loudness in the enclosing VM journal.
template<class TimeReader,class SpotReader,class LoudnessReader>
inline OriginalNoiseCacheDecision DecideOriginalNoiseCachesLazy(const double now,const float loudness,
    const PortableModelVec3& source,TimeReader readTime,SpotReader readSpot,LoudnessReader readLoudness) {
    using namespace OriginalMakeNoiseDetail;
    if (!std::isfinite(now) || now<0.0)
        throw std::invalid_argument("Original MakeNoise native time must be finite and nonnegative");
    Finite(loudness); Finite(source);
    const auto time=DecodeDouble(now);
    const auto recent=Add64(time,Negate(DecodeDouble(RecentWindow)));
    const auto factor=Multiply64(DecodeDouble(SuppressionFactor),Decode(loudness));
    std::array<std::optional<Dyadic>,2u> distances;
    std::array<std::optional<float>,2u> times,loudnesses;
    const auto cacheTime=[&](const std::size_t index) {
        if (!times[index]) {const auto value=readTime(index);Finite(value);times[index]=value;}
        return *times[index];
    };
    const auto cacheLoudness=[&](const std::size_t index) {
        if (!loudnesses[index]) {const auto value=readLoudness(index);Finite(value);loudnesses[index]=value;}
        return *loudnesses[index];
    };
    const auto nearby=[&](const std::size_t index) {
        if (!distances[index]) distances[index]=DistanceSquared(readSpot(index),source);
        return Compare64(*distances[index],Decode(2500.0f))<0;
    };
    for (std::size_t i=0u;i<2u;++i) {
        if (Compare64(Decode(cacheTime(i)),recent)>0 && nearby(i)) {
            if (Compare64(Decode(cacheLoudness(i)),factor)>=0) return {true,{}};
        }
    }
    const auto replacement=[&](const std::size_t index) {
        // Retail FSTP Float(now) is observable even if later code does not use
        // this timestamp. Refuse overflow, never saturate or publish infinity.
        static_cast<void>(Store32(time));
        return OriginalNoiseCacheDecision{false,index};
    };
    const auto stale=Add64(time,Negate(DecodeDouble(StaleWindow)));
    if (Compare64(Decode(cacheTime(0u)),stale)<0) return replacement(0u);
    if (Compare64(Decode(cacheTime(1u)),stale)<0) return replacement(1u);
    if (nearby(0u)) {
        if (cacheLoudness(0u)<=loudness) return replacement(0u);
    }
    // Literal retail quirk: a qualifying SECOND loudness replaces FIRST.
    if (cacheLoudness(1u)<=loudness) return replacement(0u);
    return {};
}
inline OriginalNoiseCacheDecision DecideOriginalNoiseCaches(const double now,const float loudness,
    const PortableModelVec3& source,const std::array<OriginalNoiseCache,2u>& caches) {
    return DecideOriginalNoiseCachesLazy(now,loudness,source,
        [&](const std::size_t i) {return caches[i].time;},
        [&](const std::size_t i) {return caches[i].spot;},
        [&](const std::size_t i) {return caches[i].loudness;});
}

// Scalar APawn.CanHear103bf7d9..103bf998 only. Team/class/probe/Instigator
// authority and the directed native BSP query belong to the runtime. Negative
// Loudness/Alertness/Threshold remain genuine finite inputs, not clamped AI.
inline OriginalCanHearDecision EvaluateOriginalCanHear(const PortableModelVec3& noiseLocation,
    const PortableModelVec3& listenerLocation,const float loudness,const float alertness,
    const float threshold,const bool sameTeam) {
    using namespace OriginalMakeNoiseDetail;
    Finite(loudness);
    const float distance=Store32(DistanceSquared(listenerLocation,noiseLocation));
    const auto range=Multiply64(Multiply64(Decode(loudness),Decode(loudness)),
        Decode(sameTeam?16000000.0f:4000000.0f));
    if (Compare64(Decode(distance),range)>0) return {};
    if (sameTeam) return {true,true,false,0.0f};
    if (distance==0.0f) return {true,false,false,0.0f};
    const auto quotient=ModelFastLineDetail::Divide64(Decode(1200000.0f),Decode(distance));
    const float storedQuotient=Store32(quotient); // FST, NOT FSTP/reload before cap test.
    const float perceived=Compare64(quotient,Decode(2.0f))>0?2.0f:storedQuotient;
    const float alertnessFactor=perceived>0.5f?0.5f:perceived;
    Finite(alertness); Finite(threshold);
    // Multiplication/addition order and the retained threshold comparison are
    // independent of the separately stored Float Stimulus (FST, not FSTP).
    const auto alerted=Multiply64(Decode(alertnessFactor),Decode(alertness));
    const auto stimulus=Add64(alerted,Multiply64(Decode(perceived),Decode(loudness)));
    const float storedStimulus=Store32(stimulus);
    const bool accepted=Compare64(stimulus,Decode(threshold))>=0;
    return {accepted,accepted,true,storedStimulus};
}

} // namespace QuestVr
