#pragma once

#include "portable_model_geometry.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

// Independent TEST-ONLY captured-owner UModel extent oracle. No production
// transform, dyadic, clip, bevel, validation or trace helper is included/called;
// no original executable is loaded. Arithmetic/control stages are independently
// transcribed from Engine/Core static bytes and the earlier null-owner scalar
// oracle. Coordinates are an already captured native ToWorld VALUE, not an
// invented Actor/ABrush ToWorld builder or proof of runtime ownership.
namespace QuestVr::ModelOwnerTraceOracle {
using Vec=PortableModelVec3;
using Real=long double;
struct Coords {Vec origin{},xAxis{},yAxis{},zAxis{};};
struct Plane {Vec normal{};float w{};};
struct Stats {
    std::size_t validationSteps{},querySteps{},nodeVisits{},leafVisits{},hullVisits{},planeTests{},bevelPairs{},bevelTests{};
};
struct Limits {
    std::size_t validationSteps{16'000'000u},querySteps{16'000'000u},recursionDepth{512u};
};
struct Result {
    bool clear{true},hit{};
    float time{1.0f};
    Vec location{},normal{};
    std::int32_t node{-1}; // Accepted hull parent, NOT FCheckResult.Item.
    Stats stats;
};

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
inline constexpr bool Available=true;
struct Precision {
    unsigned short previous{};
    Precision() {
        static_assert(std::numeric_limits<Real>::digits==64,"Owner oracle requires hardware x87 PC64");
        asm volatile("fnstcw %0":"=m"(previous));
        const auto mode=static_cast<unsigned short>((previous&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fldcw %0"::"m"(previous));}
    Precision(const Precision&)=delete;
    Precision& operator=(const Precision&)=delete;
};
#else
inline constexpr bool Available=false;
struct Precision {
    Precision() {throw std::runtime_error("Owner extent oracle requires x86 GCC hardware x87");}
};
#endif

namespace Detail {
[[noreturn]] inline void Fail(const char* reason) {throw std::runtime_error(reason);}
inline void Finite(float value) {if(!std::isfinite(value)) Fail("Owner scalar oracle active Float is nonfinite");}
inline void Finite(Vec value) {Finite(value.x);Finite(value.y);Finite(value.z);}
inline Real Add(Real a,Real b) {volatile Real x=a,y=b,z=x+y;return z;}
inline Real Sub(Real a,Real b) {volatile Real x=a,y=b,z=x-y;return z;}
inline Real Mul(Real a,Real b) {volatile Real x=a,y=b,z=x*y;return z;}
inline Real Div(Real a,Real b) {volatile Real x=a,y=b,z=x/y;return z;}
inline float Store(Real value) {
    volatile float result=static_cast<float>(value);
    if(!std::isfinite(result)) Fail("Owner scalar oracle Float store overflow");
    return result;
}
inline double Spill(Real value) {
    volatile double result=static_cast<double>(value);
    if(!std::isfinite(result)) Fail("Owner scalar oracle Double store overflow");
    return result;
}
inline Real Root(double value) {
    if(!std::isfinite(value)||value<0.0) Fail("Owner scalar oracle invalid square root");
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    Real result;asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(result):"m"(value):"st");return result;
#else
    static_cast<void>(value);Fail("Owner scalar oracle has no hardware x87");
#endif
}
inline Real Abs(Real value) {return value<0?-value:value;} // FCOM/FCHS preserves -0.
inline void Step(std::size_t& used,std::size_t maximum) {
    if(used>=maximum) Fail("Owner scalar oracle work budget exhausted");
    ++used;
}
inline void Depth(std::size_t depth,const Limits& limits) {
    if(depth>limits.recursionDepth) Fail("Independent recursive owner oracle depth guard exhausted");
}
inline std::array<float,3u> Components(Vec value) {return {value.x,value.y,value.z};}
inline Vec Cross(Vec a,Vec b) {
    return {Store(Sub(Mul(a.y,b.z),Mul(a.z,b.y))),Store(Sub(Mul(a.z,b.x),Mul(a.x,b.z))),
        Store(Sub(Mul(a.x,b.y),Mul(a.y,b.x)))};
}
inline Real DotZYX(Vec a,Vec b) {return Add(Add(Mul(a.z,b.z),Mul(a.y,b.y)),Mul(a.x,b.x));}
inline Real DotYZX(Vec a,Vec b) {return Add(Add(Mul(a.y,b.y),Mul(a.z,b.z)),Mul(a.x,b.x));}
inline Vec VectorByCoords(Vec value,const Coords& coords) {
    // Core 10119e40: each row-axis dot sums XYZ, with one output Float store.
    // X uses offsets0C/10/14, Y18/1C/20, Z24/28/2C. No origin subtraction.
    return {Store(Add(Add(Mul(value.x,coords.xAxis.x),Mul(value.y,coords.xAxis.y)),Mul(value.z,coords.xAxis.z))),
        Store(Add(Add(Mul(value.x,coords.yAxis.x),Mul(value.y,coords.yAxis.y)),Mul(value.z,coords.yAxis.z))),
        Store(Add(Add(Mul(value.x,coords.zAxis.x),Mul(value.y,coords.zAxis.y)),Mul(value.z,coords.zAxis.z)))};
}
inline Plane TraversalPlane(Plane authored,const Coords& coords) {
    // Engine 103f4328: calls imported dot for ZAxis, YAxis, XAxis, each Float
    // stored. Origin.TransformVectorBy is computed anew for each plane.
    const float z=Store(DotZYX(authored.normal,coords.zAxis)),
        y=Store(DotZYX(authored.normal,coords.yAxis)),x=Store(DotZYX(authored.normal,coords.xAxis));
    const Vec normal{x,y,z};const auto origin=VectorByCoords(coords.origin,coords);
    return {normal,Store(Sub(authored.w,DotZYX(origin,normal)))};
}
inline Plane HullPlane(Plane authored,const Coords& coords) {
    // Core 1011a000: row normal products AND final W dot use Y/Z/X, unlike
    // traversal's imported dot. Normal is not normalized or inverse-transposed.
    const Vec normal{Store(DotYZX(coords.xAxis,authored.normal)),Store(DotYZX(coords.yAxis,authored.normal)),
        Store(DotYZX(coords.zAxis,authored.normal))};
    const auto origin=VectorByCoords(coords.origin,coords);
    return {normal,Store(Sub(authored.w,DotYZX(origin,normal)))};
}
inline Plane Flip(Plane plane) {
    return {{-plane.normal.x,-plane.normal.y,-plane.normal.z},-plane.w};
}
inline Vec Normalize(Vec value,bool guarded) {
    const auto square=Add(Add(Mul(value.x,value.x),Mul(value.y,value.y)),Mul(value.z,value.z));
    const auto stored=Spill(square);
    if(guarded&&square<std::bit_cast<double>(std::uint64_t{0x3e45798ee2308c3aull})) return value;
    if(stored<=0.0) Fail("Owner scalar oracle unsafe zero normalization");
    const auto inverse=Div(1.0,Root(stored));
    return {Store(Mul(value.x,inverse)),Store(Mul(value.y,inverse)),Store(Mul(value.z,inverse))};
}
inline Plane Bevel(Plane first,Plane second,Vec axis) {
    auto direction=Cross(first.normal,second.normal);Vec point{};
    const auto determinant=Add(Add(Mul(direction.x,direction.x),Mul(direction.y,direction.y)),Mul(direction.z,direction.z));
    if(determinant<std::bit_cast<double>(std::uint64_t{0x3eb0c6f7a0b5ed8dull})) direction={};
    else {
        // Retail intersection stores X/Y cross terms before W products;
        // Z cross/products stay retained. Failed intersection is NOT a gate
        // skip: zero direction must continue into UnsafeNormal refusal below.
        const float crossAX=Store(Sub(Mul(direction.y,first.normal.z),Mul(direction.z,first.normal.y))),
            crossAY=Store(Sub(Mul(direction.z,first.normal.x),Mul(direction.x,first.normal.z))),
            crossBX=Store(Sub(Mul(second.normal.y,direction.z),Mul(second.normal.z,direction.y))),
            crossBY=Store(Sub(Mul(second.normal.z,direction.x),Mul(second.normal.x,direction.z)));
        const auto az=Sub(Mul(direction.x,first.normal.y),Mul(direction.y,first.normal.x)),
            bz=Sub(Mul(second.normal.x,direction.y),Mul(second.normal.y,direction.x));
        const float ax=Store(Mul(crossAX,second.w)),ay=Store(Mul(crossAY,second.w)),
            bx=Store(Mul(crossBX,first.w)),by=Store(Mul(crossBY,first.w));
        const float sx=Store(Add(ax,bx)),sy=Store(Add(ay,by));
        const auto sz=Add(Mul(az,second.w),Mul(bz,first.w));
        const float inverse=Store(Div(1.0,determinant));
        point={Store(Mul(sx,inverse)),Store(Mul(sy,inverse)),Store(Mul(sz,inverse))};
        direction=Normalize(direction,true);
    }
    auto normal=Normalize(Cross(axis,direction),false);
    if(DotZYX(normal,first.normal)<0.0)
        normal={Store(Mul(normal.x,-1.0f)),Store(Mul(normal.y,-1.0f)),Store(Mul(normal.z,-1.0f))};
    return {normal,Store(DotYZX(point,normal))};
}
struct Interval {float enter{-1.0f},exit{2.0f};Vec normal{};};
inline bool Clip(Interval& interval,Plane plane,Vec start,Vec end,Vec extents) {
    const auto n=plane.normal;
    const float pushout=Store(Add(Add(Abs(Mul(extents.z,n.z)),Abs(Mul(extents.y,n.y))),Abs(Mul(extents.x,n.x))));
    const auto distance=[&](Vec point) {return Store(Sub(DotZYX(point,n),plane.w));};
    const float from=distance(start),to=distance(end);auto numerator=Sub(from,pushout);
    if(from>to&&numerator>=-static_cast<Real>(pushout)&&numerator<0.0) numerator=0.0L;
    const float denominator=Store(Sub(from,to));
    // Independent hardware oracle executes the original unused divide even
    // for parallel zero denominators. Its masked FP status is not gameplay.
    const auto time=Div(numerator,denominator);
    if(denominator<std::bit_cast<double>(std::uint64_t{0xbee4f8b588e368f1ull})) {
        if(time<interval.exit) interval.exit=Store(time);
    } else if(denominator>std::bit_cast<double>(std::uint64_t{0x3ee4f8b588e368f1ull})) {
        if(time>interval.enter) {interval.enter=Store(time);interval.normal=n;}
    } else if(from>pushout&&to>pushout) return false;
    return interval.enter<interval.exit;
}
struct Hull {std::vector<std::int32_t> references;Vec minimum{},maximum{};};
inline Hull ReadHull(const PortableModelGeometry& model,std::int32_t offset,std::size_t& used,std::size_t maximum) {
    if(offset<0||static_cast<std::size_t>(offset)>=model.leafHulls.size()) Fail("Oracle invalid leaf-hull offset");
    Hull hull;auto cursor=static_cast<std::size_t>(offset);
    for(;;) {
        Step(used,maximum);
        if(cursor>=model.leafHulls.size()) Fail("Oracle missing literal leaf-hull terminator");
        const auto raw=model.leafHulls[cursor++];if(raw==-1) break;
        if(raw<0||hull.references.size()==64u) Fail("Oracle invalid or over64 hull references");
        const auto index=static_cast<std::uint32_t>(raw)&~0x40000000u;
        if(index>=model.nodes.size()) Fail("Oracle hull reference outside nodes");
        hull.references.push_back(raw);
    }
    if(model.leafHulls.size()-cursor<6u) Fail("Oracle truncated packed hull box");
    // Owner still copies the six packed bbox words, but never uses their
    // numeric values. Structural presence/count are required; NaN/Inf/reversed
    // values remain inactive and MUST NOT invent an owner query refusal.
    const auto next=[&] {Step(used,maximum);return std::bit_cast<float>(model.leafHulls[cursor++]);};
    hull.minimum={next(),next(),next()};hull.maximum={next(),next(),next()};
    return hull;
}
inline Plane Authored(const PortableModelNode& node) {return {{node.planeX,node.planeY,node.planeZ},node.planeW};}
inline void Validate(const PortableModelGeometry& model,Stats& stats,const Limits& limits) {
    if(model.version!=68u||model.licenseeMode!=0u||(model.rootOutside!=0&&model.rootOutside!=1))
        Fail("Oracle unsupported Model metadata");
    if(model.nodes.size()>1'000'000u||model.leafHulls.size()>2'000'000u) Fail("Oracle Model arrays exceed fixed native domain");
    for(const auto& node:model.nodes) {
        Step(stats.validationSteps,limits.validationSteps);Finite(Vec{node.planeX,node.planeY,node.planeZ});Finite(node.planeW);
        for(const auto child:{node.front,node.back})
            if(child!=-1&&(child<0||static_cast<std::size_t>(child)>=model.nodes.size())) Fail("Oracle invalid BSP child");
        if(node.collisionBound!=-1) static_cast<void>(ReadHull(model,node.collisionBound,stats.validationSteps,limits.validationSteps));
    }
    // Independent recursive three-colour proof over every component. Count
    // the same three node-frame stages as production's iterative validator.
    std::vector<unsigned char> color(model.nodes.size());
    std::function<void(std::size_t,std::size_t)> visit=[&](std::size_t index,std::size_t depth) {
        Depth(depth,limits);color[index]=1;
        const auto& node=model.nodes[index];
        for(const auto child:{node.front,node.back}) {
            Step(stats.validationSteps,limits.validationSteps);
            if(child==-1) continue;
            const auto next=static_cast<std::size_t>(child);
            if(color[next]==1) Fail("Oracle cyclic BSP graph");
            if(!color[next]) visit(next,depth+1u);
        }
        Step(stats.validationSteps,limits.validationSteps);color[index]=2;
    };
    for(std::size_t index=0;index<model.nodes.size();++index) if(!color[index]) visit(index,1u);
}
inline void Finalize(Result& result,Vec start,Vec delta,float size) {
    Real time=0.0L;
    if(size!=0.0f) {
        const auto quotient=Div(0.1f,size);
        const auto backoff=quotient>static_cast<Real>(0.1f)?quotient:static_cast<Real>(0.1f);
        time=Sub(result.time,backoff);
        if(time<0.0L) time=0.0L;else if(time>=1.0L) time=1.0L;
    }
    result.time=Store(time);
    result.location={Store(Add(start.x,Store(Mul(delta.x,result.time)))),Store(Add(start.y,Store(Mul(delta.y,result.time)))),
        Store(Add(start.z,Store(Mul(delta.z,result.time))))};
    result.clear=result.time==1.0f;
}
} // namespace Detail

// Captured coordinates are consumed in WORLD space: the moving Start/End and
// extents are never localized/rotated. Recursive depth is deliberately guarded
// independently from the production iterative pending-visit limit; fixtures
// beyond this test-only guard must not call this oracle recursively.
inline Result Trace(const PortableModelGeometry& model,const Coords& coords,Vec start,Vec end,Vec extents,
    const Limits& limits={}) {
    using namespace Detail;Precision precision;
    if(!limits.validationSteps||limits.validationSteps>16'000'000u||!limits.querySteps||limits.querySteps>16'000'000u||
        !limits.recursionDepth||limits.recursionDepth>512u) Fail("Oracle limits may only tighten fixed positive budgets");
    Result result;Validate(model,result.stats,limits);
    if(model.nodes.empty()) {result.clear=model.rootOutside!=0;return result;}
    // Empty native Model returns before endpoint/extent use or Owner.ToWorld;
    // do not validate ANY of those inactive values in that branch.
    Finite(start);Finite(end);Finite(extents);
    if(extents.x<0||extents.y<0||extents.z<0||(extents.x==0&&extents.y==0&&extents.z==0))
        Fail("Owner scalar oracle requires nonnegative, nonzero extents");
    Finite(coords.origin);Finite(coords.xAxis);Finite(coords.yAxis);Finite(coords.zAxis);
    result.time=2.0f;
    const Vec delta{Store(Sub(end.x,start.x)),Store(Sub(end.y,start.y)),Store(Sub(end.z,start.z))};
    const float size=Store(Root(Spill(Add(Add(Mul(delta.x,delta.x),Mul(delta.y,delta.y)),Mul(delta.z,delta.z)))));
    const float paddedY=Store(Mul(extents.y,1.1f)),paddedZ=Store(Mul(extents.z,1.1f));
    const auto paddedX=Mul(extents.x,1.1f);
    std::function<void(std::int32_t,std::int32_t,bool,std::size_t)> visit;
    visit=[&](std::int32_t index,std::int32_t parent,bool outside,std::size_t depth) {
        Depth(depth,limits);Step(result.stats.querySteps,limits.querySteps);
        if(index!=-1) {
            ++result.stats.nodeVisits;const auto& node=model.nodes.at(static_cast<std::size_t>(index));
            const auto plane=TraversalPlane(Authored(node),coords);const auto normal=plane.normal;
            const auto from=Sub(DotZYX(start,normal),plane.w),
                to=Sub(Add(Add(Mul(normal.x,end.x),Mul(normal.y,end.y)),Mul(normal.z,end.z)),plane.w);
            const auto radius=Add(Add(Abs(Mul(paddedZ,normal.z)),Abs(Mul(paddedY,normal.y))),Abs(Mul(paddedX,normal.x)));
            const bool csg=node.vertexCount!=0u&&(node.nodeFlags&0x21u)==0u;
            const auto front=[&] {if(!(from< -radius&&to< -radius)) visit(node.front,index,outside||csg,depth+1u);};
            const auto back=[&] {if(!(from>radius&&to>radius)) visit(node.back,index,outside&&!csg,depth+1u);};
            if(from<to) {back();front();} else {front();back();}
            return;
        }
        ++result.stats.leafVisits;if(outside) return;
        const auto offset=model.nodes.at(static_cast<std::size_t>(parent)).collisionBound;if(offset==-1) return;
        ++result.stats.hullVisits;
        const auto hull=ReadHull(model,offset,result.stats.querySteps,limits.querySteps);
        std::vector<Plane> planes;planes.reserve(hull.references.size());
        // Retail gathers/transforms the ENTIRE original plane list before
        // clipping any plane. Flip is after transformation, not before.
        for(const auto raw:hull.references) {
            const auto bits=static_cast<std::uint32_t>(raw);
            auto plane=HullPlane(Authored(model.nodes.at(bits&~0x40000000u)),coords);
            if(bits&0x40000000u) plane=Flip(plane);
            planes.push_back(plane);
        }
        Interval interval{-1.0f,result.time,{}};
        const auto clip=[&](Plane plane) {
            Step(result.stats.querySteps,limits.querySteps);++result.stats.planeTests;
            return Clip(interval,plane,start,end,extents);
        };
        for(const auto plane:planes) if(!clip(plane)) return;
        // Owner branch 103f4962 bypasses ALL six packed-bbox clipping planes.
        for(std::size_t i=0;i<planes.size();++i) for(std::size_t j=0;j<i;++j) {
            Step(result.stats.querySteps,limits.querySteps);++result.stats.bevelPairs;
            const auto a=Components(planes[i].normal),b=Components(planes[j].normal);
            for(unsigned axis=0;axis<3u;++axis) {
                Step(result.stats.querySteps,limits.querySteps);
                if(!((a[axis]<0&&b[axis]>0)||(a[axis]>0&&b[axis]<0))) continue;
                const Vec direction{axis==0u?1.0f:0.0f,axis==1u?1.0f:0.0f,axis==2u?1.0f:0.0f};
                if(!(DotZYX(Cross(direction,planes[i].normal),Cross(direction,planes[j].normal))>
                    std::bit_cast<double>(std::uint64_t{0x3f50624dd2f1a9fcull}))) continue;
                const auto bevel=Bevel(planes[i],planes[j],direction);++result.stats.bevelTests;
                if(!clip(bevel)) return;
            }
        }
        if(interval.enter>-1.0f&&interval.enter<interval.exit&&interval.exit>0.0f) {
            result.hit=true;result.time=interval.enter;result.normal=interval.normal;result.node=parent;
        }
    };
    visit(0,0,model.rootOutside!=0,1u);
    if(result.hit) Finalize(result,start,delta,size);
    return result;
}

} // namespace QuestVr::ModelOwnerTraceOracle
