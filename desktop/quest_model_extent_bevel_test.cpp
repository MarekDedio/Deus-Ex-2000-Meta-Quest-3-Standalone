#include "quest_model_extent_bevel.h"

#include <iostream>
#include <limits>

namespace {
using Vec=PortableModelVec3;
using Plane=QuestVr::OriginalExtentPlane;
namespace Detail=QuestVr::ModelExtentBevelDetail;
std::size_t checks{},refusals{},bevels{},skips{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
void Same(const Vec& actual,const Vec& expected) {
    ++checks;
    if(Bits(actual.x)!=Bits(expected.x)||Bits(actual.y)!=Bits(expected.y)||Bits(actual.z)!=Bits(expected.z))
        throw std::runtime_error("Original extent bevel Vector Float bits differ: actual="+
            std::to_string(Bits(actual.x))+","+std::to_string(Bits(actual.y))+","+std::to_string(Bits(actual.z))+
            " expected="+std::to_string(Bits(expected.x))+","+std::to_string(Bits(expected.y))+","+std::to_string(Bits(expected.z)));
}
void Same(const Plane& actual,const Plane& expected) {
    Same(Vec{actual.x,actual.y,actual.z},Vec{expected.x,expected.y,expected.z});
    Require(Bits(actual.w)==Bits(expected.w),"Original extent bevel plane W Float bits differ");
}
template<class F> void Refuse(F run) {
    bool refused{};try {run();} catch(const std::exception&) {refused=true;}
    Require(refused,"Invalid/nonfinite original extent bevel silently succeeded");++refusals;
}
void Boundaries() {
    const Plane first{1,1,0,4},second{-1,1,0,2};
    const auto bevel=QuestVr::BuildOriginalExtentBevel(first,second,0);
    Require(bevel.has_value(),"Simple two-wall bevel was skipped");
    Same(*bevel,{-0.0f,1,0,3}); // Retail Float -1 flip preserves this signed zero.
    Require(!QuestVr::BuildOriginalExtentBevel(first,second,1)&&!QuestVr::BuildOriginalExtentBevel(first,second,2),
        "Bevel sign gate accepted same sign/zero components");
    Require(!QuestVr::BuildOriginalExtentBevel({1,1,0,0},{-1,-1,0,0},0),
        "Opposing projected normals passed positive dot gate");
    Require(QuestVr::BuildOriginalExtentBevel({1,1,0,0},{-1,.001f,0,0},0).has_value(),
        "Float .001 above retail Double .001 gate was skipped");
    Require(!QuestVr::BuildOriginalExtentBevel({1,1,0,0},{-1,std::nextafter(.001f,0.0f),0,0},0),
        "Projected dot below Double .001 gate was accepted");
    Require(!QuestVr::BuildOriginalExtentBevel({-0.0f,1,0,0},{1,1,0,0},0),
        "Negative zero was treated as a negative component sign");
    Same(Detail::CoreNormalize({},true),{});
    Same(Detail::CoreNormalize({.0001f,0,0},true),{.0001f,0,0});
    Same(Detail::CoreNormalize({std::nextafter(.0001f,1.0f),0,0},true),{1,0,0});
    Refuse([]{Detail::CoreNormalize({},false);});
    const auto rejected=Detail::IntersectPlanes({1,0,0,1},{1,std::nextafter(.001f,0.0f),0,1});
    Require(!rejected.accepted,"Two-plane determinant below Double1e-6 was accepted");
    Same(rejected.point,{});Same(rejected.direction,{});
    Require(Detail::IntersectPlanes({1,0,0,1},{1,.001f,0,1}).accepted,
        "Two-plane determinant above Double1e-6 was rejected");
    Refuse([]{QuestVr::BuildOriginalExtentBevel({.00001f,1,0,0},{-.00001f,1,0,0},0);});
    // The native caller ignores failed intersection. Returning None here would
    // falsely certify a clear query instead of refusing zero UnsafeNormal.
    Refuse([&]{QuestVr::BuildOriginalExtentBevel(first,second,3);});
    const float nan=std::numeric_limits<float>::quiet_NaN(),infinity=std::numeric_limits<float>::infinity();
    Refuse([&]{QuestVr::BuildOriginalExtentBevel({nan,1,0,0},second,0);});
    Refuse([&]{QuestVr::BuildOriginalExtentBevel(first,{-1,infinity,0,0},0);});
    Refuse([&]{QuestVr::BuildOriginalExtentBevel({1,1,0,nan},second,0);});
    Require(!QuestVr::BuildOriginalExtentBevel({1,1,0,nan},{1,1,0,nan},0),
        "Skipped bevel unnecessarily read inactive plane W");
    Require(!Detail::IntersectPlanes({1,0,0,nan},{1,0,0,nan}).accepted,
        "Rejected two-plane determinant unnecessarily read inactive plane W");
    Refuse([&]{Detail::Cross({infinity,0,0},{0,1,0});});
    Refuse([]{const auto maximum=std::numeric_limits<float>::max();Detail::Cross({maximum,maximum,0},{-maximum,maximum,0});});
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct Precision {
    unsigned short saved{};
    Precision() {static_assert(std::numeric_limits<long double>::digits==64);asm volatile("fnstcw %0":"=m"(saved));
        const auto mode=static_cast<unsigned short>((saved&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
    ~Precision() {asm volatile("fldcw %0"::"m"(saved));}
};
long double Add(long double a,long double b) {volatile long double x=a,y=b,z=x+y;return z;}
long double Sub(long double a,long double b) {volatile long double x=a,y=b,z=x-y;return z;}
long double Mul(long double a,long double b) {volatile long double x=a,y=b,z=x*y;return z;}
long double Div(long double a,long double b) {volatile long double x=a,y=b,z=x/y;return z;}
float Store(long double value) {volatile float result=static_cast<float>(value);
    if(!std::isfinite(result)) throw std::runtime_error("Independent x87 bevel Float overflow");
    return result;}
double StoreDouble(long double value) {volatile double result=static_cast<double>(value);
    if(!std::isfinite(result)) throw std::runtime_error("Independent x87 bevel Double overflow");
    return result;}
long double Root(double value) {long double result;
    asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(result):"m"(value):"st");return result;}
std::array<long double,3u> CrossRetained(const Vec& a,const Vec& b) {
    return {Sub(Mul(a.y,b.z),Mul(b.y,a.z)),Sub(Mul(a.z,b.x),Mul(a.x,b.z)),Sub(Mul(b.y,a.x),Mul(a.y,b.x))};
}
Vec Cross(const Vec& a,const Vec& b) {const auto value=CrossRetained(a,b);return {Store(value[0]),Store(value[1]),Store(value[2])};}
long double Dot(const Vec& a,const Vec& b) {return Add(Add(Mul(a.z,b.z),Mul(a.y,b.y)),Mul(a.x,b.x));}
long double Squared(const Vec& v) {return Add(Add(Mul(v.x,v.x),Mul(v.y,v.y)),Mul(v.z,v.z));}
Vec Normalize(const Vec& v,bool guarded) {
    const auto squared=Squared(v);const auto spilled=StoreDouble(squared);
    if(guarded&&squared<std::bit_cast<double>(std::uint64_t{0x3e45798ee2308c3aull})) return v;
    if(squared==0) throw std::runtime_error("Independent x87 bevel zero UnsafeNormal refusal");
    const auto reciprocal=Div(1.0,Root(spilled));return {Store(Mul(v.x,reciprocal)),Store(Mul(v.y,reciprocal)),Store(Mul(v.z,reciprocal))};
}
struct Intersection {bool accepted{};Vec point{},direction{};};
Intersection Intersect(const Plane& first,const Plane& second) {
    const Vec a{first.x,first.y,first.z},b{second.x,second.y,second.z};
    const auto direction=Cross(a,b);const auto determinant=Squared(direction);
    if(determinant<std::bit_cast<double>(std::uint64_t{0x3eb0c6f7a0b5ed8dull})) return {};
    auto termA=CrossRetained(direction,a),termB=CrossRetained(b,direction);
    for(std::size_t i=0;i<2;++i) {termA[i]=Store(Mul(Store(termA[i]),second.w));termB[i]=Store(Mul(Store(termB[i]),first.w));}
    termA[2]=Mul(termA[2],second.w);termB[2]=Mul(termB[2],first.w);
    const std::array<long double,3u> sum{Store(Add(termA[0],termB[0])),Store(Add(termA[1],termB[1])),Add(termA[2],termB[2])};
    const auto inverse=Store(Div(1.0,determinant));
    return {true,{Store(Mul(sum[0],inverse)),Store(Mul(sum[1],inverse)),Store(Mul(sum[2],inverse))},Normalize(direction,true)};
}
Plane At(const Vec& point,const Vec& normal) {
    return {normal.x,normal.y,normal.z,Store(Add(Add(Mul(point.y,normal.y),Mul(point.z,normal.z)),Mul(point.x,normal.x)))};
}
std::optional<Plane> Oracle(const Plane& first,const Plane& second,unsigned axis) {
    const std::array<float,3u> a{first.x,first.y,first.z},b{second.x,second.y,second.z};
    if(!((a[axis]<0&&b[axis]>0)||(a[axis]>0&&b[axis]<0))) return {};
    const std::array<Vec,3u> axes{{{1,0,0},{0,1,0},{0,0,1}}};
    const Vec an{a[0],a[1],a[2]},bn{b[0],b[1],b[2]};
    if(Dot(Cross(axes[axis],an),Cross(axes[axis],bn))<=std::bit_cast<double>(std::uint64_t{0x3f50624dd2f1a9fcull})) return {};
    const auto intersection=Intersect(first,second);auto normal=Normalize(Cross(axes[axis],intersection.direction),false);
    if(Dot(an,normal)<0) normal={Store(Mul(normal.x,-1.0f)),Store(Mul(normal.y,-1.0f)),Store(Mul(normal.z,-1.0f))};
    return At(intersection.point,normal);
}
std::uint64_t Next(std::uint64_t& value) {value^=value<<13u;value^=value>>7u;value^=value<<17u;return value;}
float Number(std::uint64_t& seed,bool broad) {
    const auto raw=static_cast<std::uint32_t>(Next(seed));
    return broad?std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u)):
        static_cast<float>(static_cast<int>(raw%65537u)-32768)/1024.0f;
}
template<class A,class B,class Verify> void DifferentialCall(A portable,B independent,Verify verify) {
    bool failed{},oracleFailed{};decltype(portable()) actual{},expected{};
    try {actual=portable();} catch(const std::exception&) {failed=true;}
    try {expected=independent();} catch(const std::exception&) {oracleFailed=true;}
    Require(failed==oracleFailed,"Original extent bevel refusal differs from independent x87");
    if(failed) ++refusals;else verify(actual,expected);
}
void Differential() {
    Precision mode;std::uint64_t seed=0x7858b501ac49d63full;
    for(unsigned i=0;i<30'000u;++i) {
        const auto n=[&]{return Number(seed,i<10'000u);};
        const Plane first{n(),n(),n(),n()},second{n(),n(),n(),n()};const unsigned axis=i%3u;
        DifferentialCall([&]{return QuestVr::BuildOriginalExtentBevel(first,second,axis);},[&]{return Oracle(first,second,axis);},
            [](const auto& actual,const auto& expected){Require(actual.has_value()==expected.has_value(),"Original extent bevel gate differs from independent x87");
                if(actual) {Same(*actual,*expected);++bevels;} else ++skips;});
        // Compare the stages separately, including intersections whose normals
        // would not pass the outer bevel gate and all three Core sum orders.
        const Vec a{first.x,first.y,first.z},b{second.x,second.y,second.z};
        DifferentialCall([&]{return Detail::Cross(a,b);},[&]{return Cross(a,b);},[](const Vec& actual,const Vec& expected){Same(actual,expected);});
        DifferentialCall([&]{return Detail::CoreNormalize(a,false);},[&]{return Normalize(a,false);},[](const Vec& actual,const Vec& expected){Same(actual,expected);});
        DifferentialCall([&]{const auto value=Detail::IntersectPlanes(first,second);return Intersection{value.accepted,value.point,value.direction};},
            [&]{return Intersect(first,second);},[](const Intersection& actual,const Intersection& expected){
                Require(actual.accepted==expected.accepted,"Original two-plane determinant gate differs from independent x87");
                Same(actual.point,expected.point);Same(actual.direction,expected.direction);});
        DifferentialCall([&]{return Detail::PlaneAt(a,b);},[&]{return At(a,b);},[](const Plane& actual,const Plane& expected){Same(actual,expected);});
        Require(std::bit_cast<std::uint64_t>(Detail::StoreDouble(Detail::Dot(a,b)))==std::bit_cast<std::uint64_t>(StoreDouble(Dot(a,b))),
            "Original Core retained dot differs from independent x87 after Double store");
    }
    for(const float value:{0.0f,-0.0f,.001f,std::nextafter(.001f,0.0f),std::nextafter(.001f,1.0f),
            .0001f,std::nextafter(.0001f,0.0f),std::nextafter(.0001f,1.0f)}) {
        const Vec vector{value,-0.0f,0.0f};
        Same(Detail::CoreNormalize(vector,true),Normalize(vector,true));
        const Plane first{1,0,0,1},second{1,value,0,-1};
        const auto actual=Detail::IntersectPlanes(first,second);const auto expected=Intersect(first,second);
        Require(actual.accepted==expected.accepted,"Original determinant threshold neighbors differ from independent x87");
        Same(actual.point,expected.point);Same(actual.direction,expected.direction);
    }
    Require(bevels>1'000u&&skips>10'000u,"Independent x87 bevel samples did not cover both generation and gates");
}
#else
void Differential() {std::cout<<"SKIP independent x87 oracle on non-x86 compiler; deterministic bevel controls still run.\n";}
#endif
} // namespace
int main() {
    try {Boundaries();Differential();std::cout<<"PASS original extent bevel "<<checks<<" checks, "<<refusals
        <<" safety refusals, "<<bevels<<" bevels, "<<skips<<" gate skips; exact staged intersection/Core math.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Original extent bevel failed: "<<error.what()<<'\n';return 1;}
}
