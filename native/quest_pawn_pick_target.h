#pragma once

#include "quest_script_random_vector.h"

#include <optional>
#include <stdexcept>

namespace QuestVr {

// Detached scalar candidate only, not PawnList traversal, class/team filters,
// LineOfSightTo, target selection or OUT publication. The runtime owns those
// operations and their enclosing VM journal/native-work allowance.
struct OriginalPawnPickTargetCandidate {
    float aim{},distance{};
};

namespace OriginalPawnPickTargetDetail {
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;

inline Dyadic Load(float value) {
    if(!std::isfinite(value))
        throw std::invalid_argument("Original Pawn.PickTarget requires finite active Float data");
    return Decode(value);
}
inline Dyadic Delta(float target,float start) {return Add64(Load(target),Negate(Load(start)));}
} // namespace OriginalPawnPickTargetDetail

// APawn.PickTarget531, Engine103baf14..103bafd4. Each original subtraction
// remains PC64; there is NO intermediate FVector/Float3 delta. Dot and square
// use (Z+Y)+X. Angle is Float-spilled before its strict-positive test. Square
// stays PC64 through the strict 4,000,000 range test, then spills to Double for
// appSqrt, whose PC64 result spills to Float distance. Aim's separate FST Float
// does NOT narrow the still-live PC64 quotient used against the live bestAim.
// Only that final reached comparison reads bestAim. Do not eagerly load or
// validate an OUT value for a behind/out-of-range/nonfinite refused candidate.
template<class BestAimReader>
inline std::optional<OriginalPawnPickTargetCandidate> EvaluateOriginalPawnPickTargetCandidate(
    const PortableModelVec3& candidateLocation,const PortableModelVec3& projStart,
    const PortableModelVec3& projDir,BestAimReader readBestAim) {
    using namespace OriginalPawnPickTargetDetail;
    const auto x=Delta(candidateLocation.x,projStart.x),y=Delta(candidateLocation.y,projStart.y),
        z=Delta(candidateLocation.z,projStart.z);
    const auto dot=Add64(Add64(Multiply64(z,Load(projDir.z)),Multiply64(y,Load(projDir.y))),
        Multiply64(x,Load(projDir.x)));
    const float angle=Store32(dot);
    if(!(angle>0.0f))return {};
    // Original code reloads and recomputes all three deltas after the angle
    // gate. Values are immutable here, but retain that independent arithmetic.
    const auto sx=Delta(candidateLocation.x,projStart.x),sy=Delta(candidateLocation.y,projStart.y),
        sz=Delta(candidateLocation.z,projStart.z);
    const auto square=Add64(Add64(Multiply64(sz,sz),Multiply64(sy,sy)),Multiply64(sx,sx));
    if(Compare64(square,Decode(4000000.0f))>=0)return {};
    // SqrtStoredDouble64 refuses zero. A finite coincident candidate already
    // returned at angle<=0; never replace undefined division with an axis/one.
    const auto root=ScriptRandom::VectorDetail::SqrtStoredDouble64(
        ScriptRandom::VectorDetail::StoreDouble(square));
    const float distance=Store32(root);
    if(!(distance>0.0f))
        throw std::runtime_error("Original Pawn.PickTarget undefined zero stored distance refused");
    const auto quotient=ModelFastLineDetail::Divide64(Decode(angle),Decode(distance));
    const float aim=Store32(quotient); // FST: comparison still uses quotient.
    const float bestAim=readBestAim();
    if(Compare64(quotient,Load(bestAim))<=0)return {};
    return OriginalPawnPickTargetCandidate{aim,distance};
}
} // namespace QuestVr
