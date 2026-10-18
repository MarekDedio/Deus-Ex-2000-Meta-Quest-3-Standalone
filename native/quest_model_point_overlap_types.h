#pragma once

#include "quest_model_line_hit.h"

namespace QuestVr {

struct ModelPointOverlapLimits {
    std::size_t nodes{1'000'000u},hullWords{2'000'000u};
    std::size_t validationSteps{16'000'000u},querySteps{16'000'000u},pending{1'000'000u};
};
struct ModelPointOverlapStats {
    std::size_t validationSteps{},querySteps{},nodeVisits{},leafVisits{},hullVisits{},planeTests{},bevelPairs{},bevelTests{};
};

namespace ModelPointOverlapDetail {
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;
using ScriptRandom::VectorDetail::DecodeDouble;
using ScriptRandom::VectorDetail::StoreDouble;

[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original Model PointCheck: ")+reason);
}
inline void Step(std::size_t& used,std::size_t maximum) {
    if(used>=maximum) Fail("shared work budget exhausted");
    ++used;
}
inline Dyadic Load(float value) {
    if(!std::isfinite(value)) Fail("nonfinite active Float data");
    return Decode(value);
}
inline Dyadic Sub(const Dyadic& a,const Dyadic& b) {return Add64(a,Negate(b));}
struct Context {
    const PortableModelGeometry& model;
    std::uintptr_t modelIdentity{},ownerIdentity{};
    PortableModelVec3 point{},extent{};
    const OriginalModelCoords* coords{};
    OriginalActorLineHit& hit;
    float bestDistance{100000.0f};
    const ModelPointOverlapLimits& limits;
    ModelPointOverlapStats& stats;
};
} // namespace ModelPointOverlapDetail
} // namespace QuestVr
