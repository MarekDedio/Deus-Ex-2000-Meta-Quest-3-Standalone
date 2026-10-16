#include "quest_model_owner_coords.h"

#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

// Captured-coordinate primitives only. No original DLL execution, Actor
// ToWorld construction, owner-model sweep, hash/runtime binding or gameplay.
namespace {
using Vec=PortableModelVec3;
using Coords=QuestVr::OriginalModelCoords;
using Plane=QuestVr::OriginalExtentPlane;
using Bounds=QuestVr::OriginalModelBounds;
std::size_t checks{},refusals{},differential{},hardwareRefusals{};
void Require(bool value,const std::string& message) {++checks;if(!value) throw std::runtime_error(message);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(Vec a,Vec b) {return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
bool Same(Plane a,Plane b) {return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z)&&Bits(a.w)==Bits(b.w);}
bool Same(const Bounds& a,const Bounds& b) {return a.isValid==b.isValid&&Same(a.minimum,b.minimum)&&Same(a.maximum,b.maximum);}
Coords Identity() {return {{0,0,0},{1,0,0},{0,1,0},{0,0,1}};}
template<class F> void Refuse(F operation) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,"Unsafe captured-coordinate arithmetic silently succeeded");++refusals;
}
std::array<float*,12u> Fields(Coords& coords) {
    return {&coords.origin.x,&coords.origin.y,&coords.origin.z,&coords.xAxis.x,&coords.xAxis.y,&coords.xAxis.z,
        &coords.yAxis.x,&coords.yAxis.y,&coords.yAxis.z,&coords.zAxis.x,&coords.zAxis.y,&coords.zAxis.z};
}
void Deterministic() {
    const auto identity=Identity();const Vec vector{-3,5,7};const Plane plane{-3,5,7,11};
    QuestVr::ValidateOriginalModelCoords(identity);
    Require(Same(QuestVr::TransformOriginalPoint(vector,identity),vector),"Identity PointBy differs");
    Require(Same(QuestVr::TransformOriginalVector(vector,identity),vector),"Identity VectorBy differs");
    Require(Same(QuestVr::TransformOriginalTraversalPlane(plane,identity),plane),"Identity traversal plane differs");
    Require(Same(QuestVr::TransformOriginalHullPlane(plane,identity),plane),"Identity hull plane differs");
    auto translated=identity;translated.origin={10,-20,30};
    Require(Same(QuestVr::TransformOriginalPoint({11,-18,33},translated),{1,2,3}),"PointBy added Origin instead of subtracting it");
    Require(Same(QuestVr::TransformOriginalVector(vector,translated),vector),"VectorBy incorrectly consumed Origin");
    Require(Same(QuestVr::TransformOriginalTraversalPlane(plane,translated),{-3,5,7,-69}),"Translated traversal W differs");
    Require(Same(QuestVr::TransformOriginalHullPlane(plane,translated),{-3,5,7,-69}),"Translated hull W differs");
    const Coords rotation{{0,0,0},{0,-1,0},{1,0,0},{0,0,1}};
    Require(Same(QuestVr::TransformOriginalPoint({1,2,3},rotation),{-2,1,3}),"Captured90-degree axes were transposed");
    Require(Same(QuestVr::TransformOriginalTraversalPlane({1,2,3,4},rotation),{-2,1,3,4}),"Rotated plane was normalized/inverted");
    const Coords shear{{0,0,0},{2,1,0},{0,-3,0.5f},{4,0,1}};
    QuestVr::ValidateOriginalModelCoords(shear);
    Require(Same(QuestVr::TransformOriginalVector({1,2,3},shear),{4,-4.5f,7}),"Nonuniform/shear captured axes were replaced by unit axes");
    const Coords zero{};QuestVr::ValidateOriginalModelCoords(zero);
    Require(Same(QuestVr::TransformOriginalPoint(vector,zero),{}),"Finite singular axes were refused or repaired");
    Require(Same(QuestVr::TransformOriginalHullPlane(plane,zero),{0,0,0,11}),"Zero captured normal was normalized or discarded");

    // Point-Origin must remain PC64 before products; Float(16777217)=16777216.
    auto retained=identity;retained.origin={-1,0,0};retained.xAxis={1,-16777216.0f,0};
    Require(QuestVr::TransformOriginalPoint({16777216.0f,1,0},retained).x==1.0f,
        "PointBy prematurely Float-spilled the retained origin subtraction");
    const auto huge=std::ldexp(1.0f,70);auto cancellation=identity;cancellation.xAxis={1,1,1};
    const Plane cancelPlane{huge,-huge,1,5};
    Require(QuestVr::TransformOriginalTraversalPlane(cancelPlane,cancellation).x==0.0f,
        "Traversal normal used wrong XYZ association instead of ZYX");
    Require(QuestVr::TransformOriginalHullPlane(cancelPlane,cancellation).x==0.0f,
        "Hull normal used wrong XYZ association instead of YZX");
    Require(QuestVr::TransformOriginalVector({huge,-huge,1},cancellation).x==1.0f,
        "VectorBy reused the plane-dot schedule instead of XYZ");
    auto retainedW=identity;retainedW.origin={16777216.0f,1,0};
    Require(QuestVr::TransformOriginalTraversalPlane({1,1,0,16777216.0f},retainedW).w==-1.0f,
        "Traversal origin dot Float-spilled before W subtraction");
    Require(QuestVr::TransformOriginalHullPlane({1,1,0,16777216.0f},retainedW).w==-1.0f,
        "Hull origin dot Float-spilled before W subtraction");

    const Vec low{-1,-2,-3},high{4,5,6};
    Require(Same(QuestVr::TransformOriginalModelBounds(low,high,identity),{low,high,true}),"Identity owner bounds differ");
    Require(Same(QuestVr::TransformOriginalModelBounds(low,high,shear),{{-4,-16.5f,-7},{13,9,22},true}),
        "Sheared/negative-scale box did not transform all eight corners");
    Require(Same(QuestVr::TransformOriginalModelBounds(low,high,rotation),{{-5,-1,-3},{2,4,6},true}),
        "Rotated bounding-box corner fold differs");
    Require(Same(QuestVr::TransformOriginalModelBounds(high,low,identity),{low,high,true}),
        "Reversed finite source endpoints were repaired/refused before the original corner fold");
    Require(Same(QuestVr::TransformOriginalModelBounds(vector,vector,identity),{vector,vector,true}),
        "Zero-volume source box failed to become a valid eight-corner result");
    // First X corner is -0, second is +0. Strict native equality keeps FIRST.
    const auto minusZero=std::bit_cast<float>(0x80000000u);
    const Coords reflected{{0,0,0},{-1,0,0},{0,-1,0},{0,0,-1}};
    const auto zeroBounds=QuestVr::TransformOriginalModelBounds({0,minusZero,minusZero},
        {minusZero,minusZero,minusZero},reflected);
    Require(Bits(zeroBounds.minimum.x)==0x80000000u&&Bits(zeroBounds.maximum.x)==0x80000000u,
        "Bounding-box equality/corner order lost first negative-zero bits");

    const auto maximum=std::numeric_limits<float>::max();
    auto productCancel=identity;productCancel.xAxis={maximum,maximum,0};
    Require(QuestVr::TransformOriginalVector({maximum,-maximum,0},productCancel).x==0.0f,
        "Transform Float-spilled cancelling oversized products before summation");
    auto deltaLarge=identity;deltaLarge.origin.x=-maximum;deltaLarge.xAxis.x=0.25f;
    Require(Bits(QuestVr::TransformOriginalPoint({maximum,0,0},deltaLarge).x)==Bits(maximum*0.5f),
        "Retained PointBy subtraction was rejected as an overflowing Float delta");
    auto overflow=identity;overflow.xAxis.x=maximum;
    QuestVr::ValidateOriginalModelCoords(overflow); // Finite packet, overflowing active STORE below.
    Refuse([&]{QuestVr::TransformOriginalVector({maximum,0,0},overflow);});
    Refuse([&]{QuestVr::TransformOriginalPoint({maximum,0,0},overflow);});
    Refuse([&]{QuestVr::TransformOriginalTraversalPlane({maximum,0,0,0},overflow);});
    Refuse([&]{QuestVr::TransformOriginalHullPlane({maximum,0,0,0},overflow);});

    for(float bad:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()}) {
        for(std::size_t field=0;field<12u;++field) {
            auto invalid=identity;*Fields(invalid)[field]=bad;
            Refuse([&]{QuestVr::ValidateOriginalModelCoords(invalid);});
            Refuse([&]{QuestVr::TransformOriginalPoint(vector,invalid);});
            Refuse([&]{QuestVr::TransformOriginalTraversalPlane(plane,invalid);});
            Refuse([&]{QuestVr::TransformOriginalHullPlane(plane,invalid);});
            Refuse([&]{QuestVr::TransformOriginalModelBounds(low,high,invalid);});
            if(field<3u) Require(Same(QuestVr::TransformOriginalVector(vector,invalid),vector),
                "VectorBy validated its unconsumed Origin");
            else Refuse([&]{QuestVr::TransformOriginalVector(vector,invalid);});
        }
        for(std::size_t field=0;field<4u;++field) {
            auto invalid=plane;std::array<float*,4u> fields{&invalid.x,&invalid.y,&invalid.z,&invalid.w};*fields[field]=bad;
            Refuse([&]{QuestVr::TransformOriginalTraversalPlane(invalid,identity);});
            Refuse([&]{QuestVr::TransformOriginalHullPlane(invalid,identity);});
        }
        for(std::size_t field=0;field<3u;++field) {
            auto invalid=vector;std::array<float*,3u> fields{&invalid.x,&invalid.y,&invalid.z};*fields[field]=bad;
            Refuse([&]{QuestVr::TransformOriginalVector(invalid,identity);});
            Refuse([&]{QuestVr::TransformOriginalPoint(invalid,identity);});
            Refuse([&]{QuestVr::TransformOriginalModelBounds(invalid,high,identity);});
            Refuse([&]{QuestVr::TransformOriginalModelBounds(low,invalid,identity);});
        }
    }
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Hardware {
using Real=long double;
struct Precision {
    unsigned short original{};
    Precision() {
        static_assert(std::numeric_limits<Real>::digits==64,"Independent oracle requires actual x87 PC64");
        asm volatile("fnstcw %0":"=m"(original));
        const auto mode=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fldcw %0"::"m"(original));}
    Precision(const Precision&)=delete;Precision& operator=(const Precision&)=delete;
};
Real Read(float value) {
    if(!std::isfinite(value)) throw std::runtime_error("Independent x87 active Float is nonfinite");
    return value;
}
// Separate volatile80-bit stores keep every original arithmetic instruction at
// hardware PC64 without a host Double spill, FMA or production dyadic helper.
Real Add(Real a,Real b) {volatile Real x=a,y=b,result=x+y;return result;}
Real Sub(Real a,Real b) {volatile Real x=a,y=b,result=x-y;return result;}
Real Mul(Real a,Real b) {volatile Real x=a,y=b,result=x*y;return result;}
float Store(Real value) {
    float result;asm volatile("fldt %1; fstps %0":"=m"(result):"m"(value):"st");
    if(!std::isfinite(result)) throw std::runtime_error("Independent x87 Float store overflow");
    return result;
}
Real TraversalDot(Vec a,Vec b) {
    const auto z=Mul(Read(b.z),Read(a.z)),y=Mul(Read(b.y),Read(a.y));
    return Add(Add(z,y),Mul(Read(b.x),Read(a.x)));
}
Real HullDot(Vec axis,Vec normal) {
    const auto y=Mul(Read(axis.y),Read(normal.y)),z=Mul(Read(axis.z),Read(normal.z));
    return Add(Add(y,z),Mul(Read(axis.x),Read(normal.x)));
}
Vec Vector(const std::array<Real,3u>& value,const Coords& coords) {
    const auto component=[&](Vec axis) {
        const auto x=Mul(value[0u],Read(axis.x)),y=Mul(value[1u],Read(axis.y));
        return Store(Add(Add(x,y),Mul(value[2u],Read(axis.z))));
    };
    return {component(coords.xAxis),component(coords.yAxis),component(coords.zAxis)};
}
Vec Vector(Vec value,const Coords& coords) {
    return Vector(std::array<Real,3u>{Read(value.x),Read(value.y),Read(value.z)},coords);
}
Vec Point(Vec value,const Coords& coords) {
    const std::array<Real,3u> delta{Sub(Read(value.x),Read(coords.origin.x)),
        Sub(Read(value.y),Read(coords.origin.y)),Sub(Read(value.z),Read(coords.origin.z))};
    return Vector(delta,coords);
}
Plane Traversal(Plane value,const Coords& coords) {
    const Vec normal{value.x,value.y,value.z};
    const auto z=Store(TraversalDot(normal,coords.zAxis)),y=Store(TraversalDot(normal,coords.yAxis)),
        x=Store(TraversalDot(normal,coords.xAxis));
    const auto origin=Vector(coords.origin,coords);
    return {x,y,z,Store(Sub(Read(value.w),TraversalDot(origin,{x,y,z})))};
}
Plane Hull(Plane value,const Coords& coords) {
    const Vec normal{value.x,value.y,value.z};
    const auto x=Store(HullDot(coords.xAxis,normal)),y=Store(HullDot(coords.yAxis,normal)),
        z=Store(HullDot(coords.zAxis,normal));
    const auto origin=Vector(coords.origin,coords);
    return {x,y,z,Store(Sub(Read(value.w),HullDot(origin,{x,y,z})))};
}
Bounds Box(Vec low,Vec high,const Coords& coords) {
    Bounds result;
    // Explicit native corner sequence, independent of production nested loops.
    const std::array<Vec,8u> corners{{{low.x,low.y,low.z},{low.x,low.y,high.z},
        {low.x,high.y,low.z},{low.x,high.y,high.z},{high.x,low.y,low.z},
        {high.x,low.y,high.z},{high.x,high.y,low.z},{high.x,high.y,high.z}}};
    for(const auto corner:corners) {
        const auto value=Point(corner,coords);
        if(!result.isValid) {result.minimum=value;result.maximum=value;result.isValid=true;continue;}
        if(value.x<result.minimum.x) result.minimum.x=value.x;
        if(value.y<result.minimum.y) result.minimum.y=value.y;
        if(value.z<result.minimum.z) result.minimum.z=value.z;
        if(value.x>result.maximum.x) result.maximum.x=value.x;
        if(value.y>result.maximum.y) result.maximum.y=value.y;
        if(value.z>result.maximum.z) result.maximum.z=value.z;
    }
    return result;
}
} // namespace Hardware
std::uint64_t Next(std::uint64_t& value) {value^=value<<13u;value^=value>>7u;value^=value<<17u;return value;}
float Number(std::uint64_t& seed,bool broad) {
    const auto raw=static_cast<std::uint32_t>(Next(seed));
    return broad?std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u)):
        static_cast<float>(static_cast<int>(raw%16'777'217u)-8'388'608)/16.0f;
}
template<class A,class E> void DifferentialOne(A actual,E expected,const std::string& label) {
    using Value=decltype(actual());Value a{},b{};bool refused{},expectedRefusal{};
    try {a=actual();} catch(const std::exception&) {refused=true;}
    try {b=expected();} catch(const std::exception&) {expectedRefusal=true;}
    Require(refused==expectedRefusal,"Owner coordinates finite-domain mismatch: "+label);
    if(refused) {++hardwareRefusals;return;}
    Require(Same(a,b),"Owner coordinates exact x87 bits differ: "+label);++differential;
}
void Differential() {
    Hardware::Precision precision;std::uint64_t seed=0x8e157639bd023ca4ull;
    for(unsigned iteration=0;iteration<12'000u;++iteration) {
        const bool broad=iteration<4'000u;const auto number=[&]{return Number(seed,broad);};
        const auto vector=[&]{return Vec{number(),number(),number()};};
        const Coords coords{vector(),vector(),vector(),vector()};
        const Vec input=vector(),high=vector();const Plane plane{number(),number(),number(),number()};
        const auto label=std::to_string(iteration);
        DifferentialOne([&]{return QuestVr::TransformOriginalVector(input,coords);},
            [&]{return Hardware::Vector(input,coords);},"VectorBy case"+label);
        DifferentialOne([&]{return QuestVr::TransformOriginalPoint(input,coords);},
            [&]{return Hardware::Point(input,coords);},"PointBy case"+label);
        DifferentialOne([&]{return QuestVr::TransformOriginalTraversalPlane(plane,coords);},
            [&]{return Hardware::Traversal(plane,coords);},"traversal plane case"+label);
        DifferentialOne([&]{return QuestVr::TransformOriginalHullPlane(plane,coords);},
            [&]{return Hardware::Hull(plane,coords);},"hull plane case"+label);
        DifferentialOne([&]{return QuestVr::TransformOriginalModelBounds(input,high,coords);},
            [&]{return Hardware::Box(input,high,coords);},"eight-corner bounds case"+label);
    }
    const std::array<float,11u> edges{0.0f,std::bit_cast<float>(0x80000000u),
        std::numeric_limits<float>::denorm_min(),-std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(),-std::numeric_limits<float>::min(),
        std::nextafter(1.0f,0.0f),1.0f,std::nextafter(1.0f,2.0f),
        std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};
    for(float a:edges) for(float b:edges) {
        const Coords coords{{a,b,a},{b,a,b},{a,b,a},{b,b,a}};const Vec vector{a,b,a};
        const Plane plane{a,b,a,b};
        DifferentialOne([&]{return QuestVr::TransformOriginalPoint(vector,coords);},
            [&]{return Hardware::Point(vector,coords);},"signed-zero/subnormal edge point");
        DifferentialOne([&]{return QuestVr::TransformOriginalTraversalPlane(plane,coords);},
            [&]{return Hardware::Traversal(plane,coords);},"signed-zero/subnormal traversal plane");
        DifferentialOne([&]{return QuestVr::TransformOriginalHullPlane(plane,coords);},
            [&]{return Hardware::Hull(plane,coords);},"signed-zero/subnormal hull plane");
        DifferentialOne([&]{return QuestVr::TransformOriginalModelBounds(vector,{b,a,b},coords);},
            [&]{return Hardware::Box(vector,{b,a,b},coords);},"signed-zero/subnormal corner fold");
    }
    Require(differential>40'000u,"Independent x87 owner-coordinate controls lack successful ordinary cases");
    Require(hardwareRefusals>100u,"Broad independent x87 controls missed active Float overflow refusals");
}
#else
void Differential() {std::cout<<"SKIP independent hardware x87 owner-coordinate comparisons on non-x86/GNU target.\n";}
#endif
} // namespace
int main() {
    try {
        Deterministic();Differential();
        std::cout<<"PASS captured original Model owner coordinates: "<<checks<<" checks, "<<refusals
            <<" explicit nonfinite/overflow refusals, "<<differential<<" exact independent x87 comparisons, "
            <<hardwareRefusals<<" matching hardware finite-store refusals.\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"Original Model owner coordinates failed: "<<error.what()<<'\n';return 1;}
}
