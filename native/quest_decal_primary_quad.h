#pragma once

#include "portable_model_geometry.h"
#include "quest_script_random.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace QuestVr {

// Authored UE coordinates. This is the original primary quad, not a rectangular
// USize/VSize substitute, an AttachDecal native, clipping, or a rendered decal.
struct DecalDirectionLimits { std::size_t attempts{4096u}; };
struct DecalPreparedDirection {
    PortableModelVec3 direction;
    bool randomized{};
    std::size_t attempts{}, draws{};
};
enum class DecalPrimaryAxisPath { SuppliedCombined, RandomDirect, SurfaceBaseFallback };
struct DecalPrimaryQuad {
    std::array<PortableModelVec3,4> worldCorners{}, relativeCorners{};
    float radius{};
    DecalPrimaryAxisPath axisPath{DecalPrimaryAxisPath::SuppliedCombined};
};

namespace DecalPrimaryDetail {
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original decal primary quad: ") + reason);
}
inline bool Finite(const PortableModelVec3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
inline PortableModelVec3 Checked(const PortableModelVec3 p) {
    if (!Finite(p)) Fail("coordinate overflow/nonfinite result");
    return p;
}
inline PortableModelVec3 Add(const PortableModelVec3 a,const PortableModelVec3 b) {
    return Checked({a.x+b.x,a.y+b.y,a.z+b.z});
}
inline PortableModelVec3 Subtract(const PortableModelVec3 a,const PortableModelVec3 b) {
    return Checked({a.x-b.x,a.y-b.y,a.z-b.z});
}
inline PortableModelVec3 Scale(const PortableModelVec3 p,const double scale) {
    return Checked({static_cast<float>(p.x*scale),static_cast<float>(p.y*scale),static_cast<float>(p.z*scale)});
}
inline double Square(const PortableModelVec3 p,const bool xzy = false) {
    const double x=static_cast<double>(p.x)*p.x, y=static_cast<double>(p.y)*p.y,
        z=static_cast<double>(p.z)*p.z;
    return xzy ? (x+z)+y : (x+y)+z;
}
inline PortableModelVec3 Normalize(const PortableModelVec3 p,const bool guarded,
    const bool xzy = false) {
    const double square=Square(p,xzy);
    // Native compares the unrounded register sum with DOUBLE 1e-8. A small
    // vector is retained verbatim; no substitute tangent or fallback is made.
    if (guarded && square < 1e-8) return p;
    if (!std::isfinite(square) || square <= 0.0) Fail("invalid unsafe normalization");
    return Scale(p,1.0/std::sqrt(square));
}
inline PortableModelVec3 Cross(const PortableModelVec3 p,const PortableModelVec3 n) {
    return Checked({static_cast<float>(static_cast<double>(p.y)*n.z-static_cast<double>(p.z)*n.y),
        static_cast<float>(static_cast<double>(p.z)*n.x-static_cast<double>(p.x)*n.z),
        static_cast<float>(static_cast<double>(p.x)*n.y-static_cast<double>(p.y)*n.x)});
}
inline PortableModelVec3 Project(const PortableModelVec3 d,const PortableModelVec3 n,
    const bool suppliedProbe) {
    const double dot=(static_cast<double>(d.z)*n.z+static_cast<double>(d.y)*n.y)+static_cast<double>(d.x)*n.x;
    // 103E7A68/70 store Z/Y products, but X stays in a register through its
    // subtraction. The common direct path stores all three products first.
    const float zProduct=static_cast<float>(dot*n.z), yProduct=static_cast<float>(dot*n.y);
    const double xProduct=suppliedProbe ? dot*n.x : static_cast<float>(dot*n.x);
    return Checked({static_cast<float>(static_cast<double>(d.x)-xProduct),d.y-yProduct,d.z-zProduct});
}
} // namespace DecalPrimaryDetail

// Attach samples its zero optional direction BEFORE tracing the world. Keep
// preparation separate so a subsequent ordinary miss does not undo its draws.
// Seed writes are atomic on invalid input/rejection-budget failures; successful
// writes belong in the surrounding VM transaction's RNG journal.
inline DecalPreparedDirection PrepareOriginalDecalDirection(const PortableModelVec3 optionalDirection,
    std::uint32_t& seed,const DecalDirectionLimits& limits = {}) {
    using namespace DecalPrimaryDetail;
    if (!limits.attempts || limits.attempts > DecalDirectionLimits{}.attempts) Fail("invalid rejection limit");
    if (!Finite(optionalDirection)) Fail("nonfinite optional direction");
    DecalPreparedDirection result; result.direction=optionalDirection;
    if (optionalDirection.x != 0.0f || optionalDirection.y != 0.0f || optionalDirection.z != 0.0f) return result;
    auto candidateSeed=seed;
    result.randomized=true;
    for (std::size_t attempt=0;attempt<limits.attempts;++attempt) {
        // Core appFrand's exact binary32 constant, returned in a register.
        // 103E7503 uses fsts (not fstps): the rounding does not replace that
        // register before doubling/subtracting1 and storing the component.
        const auto component=[&candidateSeed]() {
            return static_cast<float>(static_cast<double>(ScriptRandom::Draw15(candidateSeed))*0x1.0002p-15*2.0-1.0);
        };
        const PortableModelVec3 sample{component(),component(),component()};
        ++result.attempts; result.draws+=3u;
        // FVector.SizeSquared returns an unrounded register (Core10117B70).
        if (Square(sample)>1.0) continue;
        result.direction=Normalize(sample,false); seed=candidateSeed; return result;
    }
    Fail("random direction rejection budget exhausted");
}

inline DecalPrimaryQuad BuildOriginalDecalPrimaryQuad(const PortableModelVec3 center,
    const PortableModelVec3 surfaceBase,const PortableModelVec3 surfaceNormal,
    const std::int32_t textureUSize,const float drawScale,const DecalPreparedDirection& prepared) {
    using namespace DecalPrimaryDetail;
    if (!Finite(center) || !Finite(surfaceBase) || !Finite(surfaceNormal) ||
        !Finite(prepared.direction) || !std::isfinite(drawScale)) Fail("nonfinite geometry input");
    DecalPrimaryQuad result;
    // Signed USize is read by fild; VSize is never read here. Negative scale
    // keeps positive geometric radius (UV scale is a separate render input).
    const double scale=drawScale, size=textureUSize;
    result.radius=static_cast<float>(std::sqrt((scale*scale*size*size)*0.5));
    if (!std::isfinite(result.radius)) Fail("radius overflow");
    PortableModelVec3 a,b;
    if (prepared.randomized) {
        a=Project(prepared.direction,surfaceNormal,false); b=Cross(a,surfaceNormal);
        result.axisPath=DecalPrimaryAxisPath::RandomDirect;
    } else {
        const auto projected=Project(prepared.direction,surfaceNormal,true);
        if (std::fabs(static_cast<double>(projected.x)) >= 0.0001 ||
            std::fabs(static_cast<double>(projected.y)) >= 0.0001 ||
            std::fabs(static_cast<double>(projected.z)) >= 0.0001) {
            const auto p=Normalize(projected,true);
            const auto q=Normalize(Cross(projected,surfaceNormal),true,true);
            a=Add(p,q); b=Subtract(p,q);
            result.axisPath=DecalPrimaryAxisPath::SuppliedCombined;
        } else {
            a=Project(Subtract(surfaceBase,center),surfaceNormal,false); b=Cross(a,surfaceNormal);
            result.axisPath=DecalPrimaryAxisPath::SurfaceBaseFallback;
        }
    }
    a=Scale(Normalize(a,true),result.radius); b=Scale(Normalize(b,true),result.radius);
    result.worldCorners={Add(center,a),Add(center,b),Subtract(center,a),Subtract(center,b)};
    for (std::size_t i=0;i<result.worldCorners.size();++i)
        result.relativeCorners[i]=Subtract(result.worldCorners[i],surfaceBase);
    return result;
}

} // namespace QuestVr
