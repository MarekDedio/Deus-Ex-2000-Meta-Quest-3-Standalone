#pragma once

#include "quest_model_fast_line_check.h"
#include "quest_sound_protocol.h"

namespace QuestVr {

// CheckHearSound retains its first geometric sum in ordinary x87 PC64 and
// also spills the same value to Float for the later occlusion comparison.
// Host double/long-double width is not authority for either operation.
struct OriginalSoundSquaredDistance {
    ActorTimerDetail::Dyadic initial{};
    float stored{};
};

namespace OriginalSoundHearingDetail {
using ModelFastLineDetail::Decode;
using ModelFastLineDetail::Multiply64;
using ModelFastLineDetail::Store32;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;

inline void ValidateDistance(const OriginalSoundSquaredDistance& distance) {
    // Genuine finite binary32 coordinates cannot reach these exponent limits.
    // Bound public detached inputs before helpers perform integer arithmetic.
    if (ActorTimerDetail::Bits(distance.initial.significand)>64u ||
        distance.initial.exponent < -1024 || distance.initial.exponent > 1024 ||
        (distance.initial.negative && distance.initial.significand) ||
        !std::isfinite(distance.stored) || distance.stored<0.0f)
        throw std::invalid_argument("Original sound squared distance is invalid");
    if (Store32(distance.initial)!=distance.stored)
        throw std::invalid_argument("Original sound distance Float spill does not match its retained sum");
}

inline float InitialThreshold(const PlaySoundProtocolPacket& packet) {
    SoundProtocolDetail::ValidateParameters(packet.parameters);
    SoundProtocolDetail::RequireFinite(packet.squaredRadius,"Original sound squared radius must be finite");
    if (packet.squaredRadius<0.0f)
        throw std::invalid_argument("Original sound squared radius must be nonnegative");
    return Store32(Multiply64(Decode(packet.squaredRadius),Decode(kCheckHearSoundInitialFactor)));
}
} // namespace OriginalSoundHearingDetail

inline OriginalSoundSquaredDistance OriginalSoundDistanceSquared(
    const PortableModelVec3& source,const PortableModelVec3& target) {
    using namespace OriginalSoundHearingDetail;
    for (const float coordinate : {source.x,source.y,source.z,target.x,target.y,target.z})
        SoundProtocolDetail::RequireFinite(coordinate,"Original sound coordinate must be finite");
    const auto x=Add64(Decode(target.x),Negate(Decode(source.x)));
    const auto y=Add64(Decode(target.y),Negate(Decode(source.y)));
    const auto z=Add64(Decode(target.z),Negate(Decode(source.z)));
    // Retail103e1b6c..1b9b squares the unspilled deltas and sums z/y/x.
    const auto sum=Add64(Add64(Multiply64(z,z),Multiply64(y,y)),Multiply64(x,x));
    return {sum,Store32(sum)};
}

inline bool OriginalSoundInitialRange(const PlaySoundProtocolPacket& packet,
    const OriginalSoundSquaredDistance& distance) {
    using namespace OriginalSoundHearingDetail;
    ValidateDistance(distance);
    return Compare64(distance.initial,Decode(InitialThreshold(packet)))<0;
}

inline CheckHearSoundProtocolResult EvaluateOriginalSoundHearing(
    const PlaySoundProtocolPacket& packet,const OriginalSoundSquaredDistance& distance,
    const bool bspBlocked,const bool listenerIsSourceInstigator) {
    using namespace OriginalSoundHearingDetail;
    ValidateDistance(distance);
    CheckHearSoundProtocolResult result;
    result.parameters=packet.parameters;
    result.initialThreshold=InitialThreshold(packet);
    result.effectiveThreshold=result.initialThreshold;
    result.comparisonThreshold=static_cast<double>(result.initialThreshold);
    result.initialRangeAccepted=Compare64(distance.initial,Decode(result.initialThreshold))<0;
    if (!result.initialRangeAccepted) return result;
    if (!bspBlocked) {result.audible=true;return result;}
    auto threshold=Decode(result.initialThreshold);
    if (!listenerIsSourceInstigator) {
        // Both inputs are Float: this product is exact within PC64. Keep it
        // unspilled for the inclusive comparison against the stored distance.
        threshold=Multiply64(threshold,Decode(std::bit_cast<float>(std::uint32_t{0x3f19999au})));
        result.comparisonThreshold*=static_cast<double>(std::bit_cast<float>(std::uint32_t{0x3f19999au}));
        result.effectiveThreshold=Store32(threshold);
    }
    result.parameters[1u]=Store32(Multiply64(Decode(result.parameters[1u]),
        Decode(std::bit_cast<float>(std::uint32_t{0x3eb33333u}))));
    result.audible=Compare64(Decode(distance.stored),threshold)<=0;
    return result;
}
} // namespace QuestVr
