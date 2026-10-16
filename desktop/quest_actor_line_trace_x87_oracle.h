#pragma once

#include "portable_model_geometry.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

// Independent test-only hardware oracle for the audited retail UPrimitive
// cylinder LineCheck. No production dyadic/cylinder helper or original binary
// is called. The independent packets intentionally do not alias runtime types.
namespace QuestVr::ActorLineX87Oracle {
using Vec=PortableModelVec3;
struct Hit {
    std::uintptr_t next{},actor{},primitive{};
    float time{};
    Vec location{},normal{};
    std::int32_t item{};
};
struct Cylinder {std::uintptr_t identity{};Vec location{};float radius{},height{};};

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
inline constexpr bool Available=true;
using Real=long double;
struct Precision {
    unsigned short original{};
    Precision() {
        static_assert(std::numeric_limits<Real>::digits==64,"Oracle requires actual x87 extended arithmetic");
        asm volatile("fnstcw %0":"=m"(original));
        const auto mode=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fldcw %0"::"m"(original));}
    Precision(const Precision&)=delete;
    Precision& operator=(const Precision&)=delete;
};

namespace Detail {
inline Real Read(float value) {
    if(!std::isfinite(value)) throw std::runtime_error("Cylinder x87 oracle active Float is nonfinite");
    return value;
}
inline Real Dimension(float value) {
    const auto result=Read(value);
    if(result<0) throw std::runtime_error("Cylinder x87 oracle refuses negative geometry");
    return result;
}
inline Real Add(Real a,Real b) {volatile Real x=a,y=b,z=x+y;return z;}
inline Real Sub(Real a,Real b) {volatile Real x=a,y=b,z=x-y;return z;}
inline Real Mul(Real a,Real b) {volatile Real x=a,y=b,z=x*y;return z;}
inline Real Div(Real a,Real b) {
    if(b==0) throw std::runtime_error("Cylinder x87 oracle refuses active division by zero");
    volatile Real x=a,y=b,z=x/y;return z;
}
inline float Store(Real value) {
    volatile float result=static_cast<float>(value);
    if(!std::isfinite(result)) throw std::runtime_error("Cylinder x87 oracle Float store overflow");
    return result;
}
inline double Spill(Real value) {
    volatile double result=static_cast<double>(value);
    if(!std::isfinite(result)) throw std::runtime_error("Cylinder x87 oracle Double store overflow");
    return result;
}
inline Real Root(double value) {
    if(!std::isfinite(value)||value<0) throw std::runtime_error("Cylinder x87 oracle invalid square root");
    Real result;
    asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(result):"m"(value):"st");return result;
}
inline constexpr double Tiny=std::bit_cast<double>(std::uint64_t{0x3e45798ee2308c3aull});
inline constexpr double InsideDot=std::bit_cast<double>(std::uint64_t{0xbfb999999999999aull});
inline constexpr double Backoff=std::bit_cast<double>(std::uint64_t{0x3f50624dd2f1a9fcull});
inline Real Square(Vec v) {
    return Add(Add(Mul(Read(v.x),Read(v.x)),Mul(Read(v.y),Read(v.y))),Mul(Read(v.z),Read(v.z)));
}
inline Vec SafeNormal(Vec v) {
    const auto square=Square(v);const auto saved=Spill(square);
    if(square<Tiny) return {};
    const auto reciprocal=Div(1.0,Root(saved));
    return {Store(Mul(v.x,reciprocal)),Store(Mul(v.y,reciprocal)),Store(Mul(v.z,reciprocal))};
}
inline void SideNormalize(Vec& v) {
    const auto square=Square(v);const auto saved=Spill(square);
    if(square<Tiny) return;
    // The inline side path spills SQRT itself to Double and reloads it. This
    // differs from SafeNormal's retained root; do not merge these two paths.
    const auto savedRoot=Spill(Root(saved));const auto reciprocal=Div(1.0,savedRoot);
    v={Store(Mul(v.x,reciprocal)),Store(Mul(v.y,reciprocal)),Store(Mul(v.z,reciprocal))};
}
} // namespace Detail

// Caller selects ordinary PC64 nearest-even with Precision for a whole oracle
// batch. Partial writes on clear are observable. Exceptions likewise leave the
// detached packet partially written; an enclosing production transaction must
// restore aliases, not use this test packet as an atomic transaction oracle.
inline bool Check(Hit& hit,const std::optional<Cylinder>& owner,const Vec& end,
    const Vec& start,const Vec& extent) {
    using namespace Detail;
    if(!owner) return true; // Must precede every field/endpoints validation.
    if(!owner->identity) throw std::runtime_error("Cylinder x87 oracle nonnull owner lacks identity");
    // Explicit finite/nonnegative geometry is the portable domain, not a
    // clamp. Native null-owner bypass above remains before that domain check.
    const auto height=Store(Add(Dimension(extent.z),Dimension(owner->height)));
    const auto radiusY=Store(Add(Dimension(extent.y),Dimension(owner->radius)));
    const auto retainedRadiusX=Add(Dimension(extent.x),Dimension(owner->radius));
    const auto radiusX=Store(retainedRadiusX);
    const auto upperX=Add(retainedRadiusX,Read(owner->location.x));
    if(Read(start.x)>upperX&&Read(end.x)>upperX) return true;
    const auto lowerX=Sub(Read(owner->location.x),radiusX);
    if(Read(start.x)<lowerX&&Read(end.x)<lowerX) return true;
    const auto upperY=Add(radiusY,Read(owner->location.y));
    if(Read(start.y)>upperY&&Read(end.y)>upperY) return true;
    const auto lowerY=Sub(Read(owner->location.y),radiusY);
    if(Read(start.y)<lowerY&&Read(end.y)<lowerY) return true;
    const auto upperZ=Store(Add(height,Read(owner->location.z)));
    if(Read(start.z)>upperZ&&Read(end.z)>upperZ) return true;
    const auto lowerZ=Store(Sub(Read(owner->location.z),height));
    if(Read(start.z)<lowerZ&&Read(end.z)<lowerZ) return true;

    float enter=0.0f,exit=1.0f;
    if(start.z>upperZ&&end.z<upperZ) {
        const auto t=Div(Sub(upperZ,start.z),Sub(end.z,start.z));
        if(t>0) {enter=Store(t);hit.normal={0,0,1};}
    } else if(start.z<upperZ&&end.z>upperZ) {
        const auto t=Div(Sub(upperZ,start.z),Sub(end.z,start.z));
        if(1.0f>t) exit=Store(t);
    }
    if(start.z<lowerZ&&end.z>lowerZ) {
        const auto t=Div(Sub(lowerZ,start.z),Sub(end.z,start.z));
        if(t>enter) {enter=Store(t);hit.normal={0,0,-1};}
    } else if(start.z>lowerZ&&end.z<lowerZ) {
        const auto t=Div(Sub(lowerZ,start.z),Sub(end.z,start.z));
        if(exit>t) exit=Store(t);
    }
    if(enter>=exit) return true;

    const auto px=Sub(start.x,owner->location.x),py=Sub(start.y,owner->location.y);
    const auto dx=Sub(end.x,start.x),dy=Sub(end.y,start.y);
    const float storedX=Store(dx),storedY=Store(dy);
    const float a=Store(Add(Mul(dy,dy),Mul(dx,dx)));
    const float b=Store(Mul(Add(Mul(dy,py),Mul(dx,px)),2.0));
    const float c=Store(Sub(Add(Mul(py,py),Mul(px,px)),Mul(radiusX,radiusX)));
    const float discriminant=Store(Sub(Mul(b,b),Mul(Mul(c,a),4.0)));

    if(c<1.0&&start.z>lowerZ&&start.z<upperZ) {
        const auto pz=Sub(start.z,owner->location.z);
        // The actual body also stores E.z-S.z before overwriting that scratch.
        static_cast<void>(Store(Sub(end.z,start.z)));
        const auto inward=Add(Add(Mul(storedX,px),Mul(storedY,py)),Mul(0.0f,pz));
        if(inward>=InsideDot) return true;
        hit.time=0.0f;hit.location=start;
        // Retail recomputes a Z subtraction, stores it in scratch and then
        // overwrites the vector Z with +0 before the imported SafeNormal.
        static_cast<void>(Store(pz));
        hit.normal=SafeNormal({Store(px),Store(py),0.0f});
        hit.actor=owner->identity;hit.primitive=0u;return false;
    }
    if(discriminant<0) return true;
    if(a<Tiny) {
        if(c>0) return true;
    } else {
        const auto root=Root(static_cast<double>(discriminant));
        static_cast<void>(Spill(root)); // Retail FST Double, retains original ST0.
        const auto inverse=Div(0.5,static_cast<Real>(a));
        const auto exitRoot=Mul(Sub(root,b),inverse);
        if(exit>exitRoot) exit=Store(exitRoot);
        const auto entryRoot=-Mul(Add(root,b),inverse);
        if(entryRoot>enter) {
            enter=Store(entryRoot); // FST only: subsequent offsets retain root.
            const auto z=Add(Mul(Sub(end.z,start.z),entryRoot),start.z);
            const auto x=Store(Add(Store(Mul(storedX,entryRoot)),start.x));
            const auto y=Store(Add(Store(Mul(storedY,entryRoot)),start.y));
            hit.normal={Store(Sub(x,owner->location.x)),Store(Sub(y,owner->location.y)),Store(Sub(z,owner->location.z))};
            hit.normal.z=0.0f;
            SideNormalize(hit.normal);
        }
        if(enter>=exit) return true;
    }
    auto time=Sub(enter,Backoff);
    if(time<0) time=0.0;
    else if(time>=1.0) time=1.0;
    hit.time=Store(time); // FST only: location uses retained time, not stored hit.time.
    const auto deltaY=Store(Sub(end.y,start.y)),deltaZ=Store(Sub(end.z,start.z));
    const auto offsetX=Store(Mul(storedX,time)),offsetY=Store(Mul(deltaY,time));
    const auto offsetZ=Mul(deltaZ,time); // Z offset does NOT Float-spill.
    hit.location={Store(Add(offsetX,start.x)),Store(Add(offsetY,start.y)),Store(Add(offsetZ,start.z))};
    hit.actor=owner->identity;hit.primitive=0u;return false;
}
#else
inline constexpr bool Available=false;
#endif
} // namespace QuestVr::ActorLineX87Oracle
