#pragma once

#include "quest_model_point_trace.h"
#include "quest_native_window_clip.h"

#include <bit>

namespace QuestVr {

// Retail UModel::FastLineCheck (Engine.dll103f3280/103f33a0), not
// Model.LineCheck(flags6), Actor.Trace, polygons, actors or moving brushes.
// Reuse only the existing bounded serialized-BSP validation/stats contract.
// Arithmetic is the ordinary simulation PC64/nearest path; retail's temporary
// render-hook FastMath24 mode is not an implicit mode of this CPU adapter.
using ModelFastLineCheckLimits = ModelPointTraceLimits;
struct ModelFastLineCheckResult {
    bool clear{};
    ModelPointTraceStats stats;
};

namespace ModelFastLineDetail {
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Wide;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Round64;
using ActorTimerDetail::Store32;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Negate;
using NativeWindowClipDetail::Normalize64;

[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original model fast line: ")+reason);
}
inline Dyadic Multiply64(Dyadic a,Dyadic b) {
    a=Normalize64(a);b=Normalize64(b);
    // Two at-most64-bit significands fit exactly in the unsigned128 product.
    return Round64({a.significand*b.significand,a.exponent+b.exponent,a.negative!=b.negative});
}
inline Dyadic Divide64(Dyadic numerator,Dyadic denominator) {
    numerator=Normalize64(numerator);denominator=Normalize64(denominator);
    if (!denominator.significand) Fail("undefined split division by zero refused");
    const bool negative=numerator.negative!=denominator.negative;
    if (!numerator.significand) return {0u,0,negative};
    // Both normalized operands have64 significant bits. Shift63 or64 yields
    // a64-bit quotient and at most128-bit dividend, with exact remainder/ties.
    // Unlike the timer-specific division this admits arbitrary signed PC64
    // operands, including a64-bit split denominator.
    const unsigned shift=numerator.significand<denominator.significand?64u:63u;
    const Wide dividend=numerator.significand<<shift;
    Wide quotient=dividend/denominator.significand;
    const Wide remainder=dividend%denominator.significand;
    const Wide twice=remainder*2u;
    if (twice>denominator.significand || (twice==denominator.significand && (quotient&1u))) ++quotient;
    return Round64({quotient,numerator.exponent-denominator.exponent-static_cast<int>(shift),negative});
}
inline float Distance(const PortableModelVec3& point,const PortableModelNode& plane) {
    // Every arithmetic instruction rounds to PC64; only the final distance
    // is spilled to Float. Preserve the retail z/y/x addition order.
    const auto zy=Add64(Multiply64(Decode(point.z),Decode(plane.planeZ)),
        Multiply64(Decode(point.y),Decode(plane.planeY)));
    const auto xyz=Add64(zy,Multiply64(Decode(point.x),Decode(plane.planeX)));
    return Store32(Add64(xyz,Negate(Decode(plane.planeW))));
}
inline bool Front(const float distance) {
    // Retail TEST/SETGE classifies the signed FLOAT BITS. In particular -0
    // is back, +0 is front. A numeric comparison or epsilon is different.
    return (std::bit_cast<std::uint32_t>(distance)&0x80000000u)==0u;
}
inline PortableModelVec3 Middle(const PortableModelVec3& start,const PortableModelVec3& end,
    const float startDistance,const float endDistance) {
    const auto fraction=ModelFastLineDetail::Divide64(Decode(startDistance),Add64(Decode(startDistance),Negate(Decode(endDistance))));
    const auto component=[&](const float from,const float to) {
        const auto delta=Add64(Decode(to),Negate(Decode(from)));
        return Store32(Add64(Multiply64(delta,fraction),Decode(from)));
    };
    return {component(start.x,end.x),component(start.y,end.y),component(start.z,end.z)};
}
inline bool Outside(const bool front,const bool previous,const std::uint8_t flags) {
    return (flags&1u)?previous:front;
}
} // namespace ModelFastLineDetail

inline ModelFastLineCheckResult CheckOriginalModelFastLine(const PortableModelGeometry& model,
    const PortableModelVec3& end,const PortableModelVec3& start,const ModelFastLineCheckLimits& limits={}) {
    using namespace ModelFastLineDetail;
    if (!ModelPointTraceDetail::Finite(start) || !ModelPointTraceDetail::Finite(end)) Fail("nonfinite endpoint refused");
    ModelFastLineCheckResult result;
    ModelPointTraceDetail::Validate(model,limits,result.stats);
    result.clear=model.rootOutside!=0;
    if (model.nodes.empty()) return result;
    struct Segment {
        std::int32_t node;
        PortableModelVec3 end,start;
        bool outside;
    };
    Segment current{0,end,start,result.clear};
    std::vector<Segment> pending;
    pending.reserve(std::min<std::size_t>(model.nodes.size(),64u));
    for (;;) {
        ModelPointTraceDetail::Step(result.stats.querySteps,limits.querySteps);
        if (current.node==-1) {
            ++result.stats.leafVisits;
            if (!current.outside) {result.clear=false;return result;}
            if (pending.empty()) {result.clear=true;return result;}
            current=pending.back();pending.pop_back();continue;
        }
        ++result.stats.nodeVisits;
        const auto& node=model.nodes.at(static_cast<std::size_t>(current.node));
        const float startDistance=Distance(current.start,node);
        const float endDistance=Distance(current.end,node);
        const bool startFront=Front(startDistance),endFront=Front(endDistance);
        const auto child=[&](const bool front) {return front?node.front:node.back;};
        if (startFront==endFront) {
            current.node=child(startFront);
            current.outside=Outside(startFront,current.outside,node.nodeFlags);
        } else {
            const auto middle=Middle(current.start,current.end,startDistance,endDistance);
            // Visit End-side first, with its segment REVERSED: (End=middle,
            // Start=oldEnd). Then tail-visit Start-side with End=middle.
            pending.push_back({child(startFront),middle,current.start,
                Outside(startFront,current.outside,node.nodeFlags)});
            current={child(endFront),middle,current.end,
                Outside(endFront,current.outside,node.nodeFlags)};
        }
    }
}
} // namespace QuestVr
