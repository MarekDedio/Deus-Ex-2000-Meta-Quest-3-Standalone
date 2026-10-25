#include "quest_pawn_line_of_sight.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

// Test-owned arithmetic/instructions only. No commercial DLL is loaded or
// executed, and the independent oracle does not call production dyadic math.
namespace {
namespace Math=QuestVr::PawnLineOfSightDetail;
using Vec=PortableModelVec3;
std::size_t checks{},refusals{},portableChecks{},hardwareCases{};
void Require(bool value,const char* why) {
    ++checks;if(!value)throw std::runtime_error(why);
}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(Vec a,Vec b) {return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
template<class F>void Refuse(F operation,const char* why) {
    bool refused{};try{operation();}catch(const std::exception&){refused=true;}
    Require(refused,why);++refusals;
}
void Deterministic() {
    Require(Math::DistanceSquared({3,4,12},{})==169.0f,"LOS distance changed XYZ squared length");
    Require(Math::DistanceSquared({1000,0,0},{})==1000000.0f,"LOS distant equality was lost");
    Require(Math::DistanceSquared({500,0,0},{})==250000.0f,"LOS corner equality was lost");
    for(float threshold:{500.0f,1000.0f}) {
        const float square=threshold*threshold;
        Require(Math::DistanceSquared({std::nextafter(threshold,0.0f),0,0},{})<square,
            "LOS lower distance neighborhood changed");
        Require(Math::DistanceSquared({std::nextafter(threshold,2000.0f),0,0},{})>square,
            "LOS upper distance neighborhood changed");
    }
    // Float-subtracting first would lose the small residual before squaring.
    Require(Bits(Math::DistanceSquared({1,0,0},{std::ldexp(1.0f,-25),0,0}))==0x3f7fffffu,
        "LOS distance prematurely Float-stored a component difference");
    for(bool player:{false,true}) {
        const auto cap=Math::Decode(player?25000000.0f:16000000.0f);
        Require(Math::Compare64(Math::RangeSquared(player,50u),cap)<0,"Visibility50 was prematurely capped");
        Require(Math::Compare64(Math::RangeSquared(player,51u),cap)==0,"Visibility51 did not saturate");
        Require(Math::Compare64(Math::RangeSquared(player,255u),cap)==0,"Visibility255 escaped cap");
    }
    const auto retained=Math::RangeSquared(true,0u);
    Require(Math::Compare64(retained,Math::Decode(1440000.0f))<0&&
        Bits(Math::Store32(retained))==0x49afc7ffu&&
        Math::Compare64(retained,Math::Decode(Math::Store32(retained)))>0,
        "LOS range lost its sub-Float retained threshold");
    const auto eye=Math::Eye({2,3,1},std::ldexp(1.0f,-24));
    Require(Same(eye,{2,3,1}),"Eye Float tie did not round to even");
    Require(Bits(Math::Eye({0,0,-0.0f},-0.0f).z)==0x80000000u,"Eye lost negative zero");
    const auto head=Math::Head({2,3,-0.8f},1.0f);
    Require(head.x==2&&head.y==3&&head.z<0&&head.z!=0,
        "Head used Float0.8 or prematurely Float-stored its product");
    Require(Math::Head({0,0,10},-5.0f).z==6.0f,"Negative finite collision height was rejected/normalized");
    Require(Bits(Math::Head({0,0,-0.0f},-0.0f).z)==0x80000000u,"Head lost negative zero");
    const auto corners=Math::SelectCorners({10,20,30},2.0f);
    const std::array<Vec,4u> expected{{{8,22,30},{12,22,30},{8,18,30},{12,18,30}}};
    for(std::size_t i=0u;i<expected.size();++i)
        Require(Same(corners.points[i],expected[i]),"LOS corner index/sign order changed");
    Require(corners.minimum==2u&&corners.maximum==1u,"LOS absolute world-size pruning changed");
    const auto negative=Math::SelectCorners({10,20,30},-2.0f);
    Require(negative.minimum==1u&&negative.maximum==2u,"Negative radius was normalized/rejected");
    const auto tied=Math::SelectCorners({},1.0f);
    Require(tied.minimum==0u&&tied.maximum==0u,"Strict corner ties changed first extrema indices");
    std::size_t rays{};
    for(std::size_t i=0u;i<4u;++i)if(i!=tied.minimum&&i!=tied.maximum)++rays;
    Require(rays==3u,"Coincident skipped indices must leave three side rays");
    const auto roundedDown=Math::SelectCorners({1,std::ldexp(1.0f,-12),0},0.0f);
    Require(roundedDown.minimum==0u&&roundedDown.maximum==3u,
        "Retained square vs rounded-down max must update each strict max tie");
    const auto roundedUp=Math::SelectCorners({1,std::ldexp(1.0f,-12),std::ldexp(1.0f,-13)},0.0f);
    Require(roundedUp.minimum==3u&&roundedUp.maximum==0u,
        "Retained square vs rounded-up min must update each strict min tie");
    for(std::uint32_t draw=0u;draw<32768u;++draw)
        Require(Math::RandomVisible(draw)==(draw>=16384u),"Raw CRT half-range classification changed");
}
void ActiveFieldsAndSafety() {
    const float nan=std::bit_cast<float>(std::uint32_t{0x7fc12345u});
    const float infinity=std::numeric_limits<float>::infinity();
    const Vec copied{nan,infinity,2};
    Require(Same(Math::Eye(copied,3),{nan,infinity,5}),"Eye eagerly validated copied inactive XY words");
    Require(Same(Math::Head(copied,5),{nan,infinity,6}),"Head eagerly validated copied inactive XY words");
    for(const float invalid:{nan,infinity,-infinity}) {
        Refuse([&]{static_cast<void>(Math::DistanceSquared({invalid,0,0},{}));},"Nonfinite distance input passed");
        Refuse([&]{static_cast<void>(Math::Eye({0,0,invalid},1));},"Nonfinite active eye Z passed");
        Refuse([&]{static_cast<void>(Math::Eye({},invalid));},"Nonfinite active eye height passed");
        Refuse([&]{static_cast<void>(Math::Head({},invalid));},"Nonfinite active collision height passed");
        Refuse([&]{static_cast<void>(Math::SelectCorners({},invalid));},"Nonfinite active radius passed");
        Refuse([&]{static_cast<void>(Math::SelectCorners({0,0,invalid},1));},"Nonfinite active corner Z passed");
    }
    const auto maximum=std::numeric_limits<float>::max();
    Refuse([&]{static_cast<void>(Math::DistanceSquared({maximum,0,0},{}));},"Distance Float overflow passed");
    Refuse([&]{static_cast<void>(Math::Eye({0,0,maximum},maximum));},"Eye Float overflow passed");
    Refuse([&]{static_cast<void>(Math::Head({0,0,maximum},maximum));},"Head Float overflow passed");
    Refuse([&]{static_cast<void>(Math::SelectCorners({maximum,0,0},maximum));},"Corner coordinate overflow passed");
    Refuse([&]{static_cast<void>(Math::SelectCorners({maximum,0,0},0));},"Corner squared-size overflow passed");
    Refuse([&]{static_cast<void>(Math::RandomVisible(32768u));},"Non-15-bit random sample passed");
    Refuse([&]{static_cast<void>(Math::RandomVisible(std::numeric_limits<std::uint32_t>::max()));},
        "Oversized random sample passed");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Hardware {
using Real=long double;
struct Precision {
    unsigned short original{};
    Precision() {
        static_assert(std::numeric_limits<Real>::digits==64,"Independent oracle requires x87 PC64");
        asm volatile("fnstcw %0":"=m"(original));
        const auto mode=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision(){asm volatile("fldcw %0"::"m"(original));}
};
Real Read(float value) {
    if(!std::isfinite(value))throw std::runtime_error("Independent nonfinite active Float");
    return value;
}
Real Add(Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,z=x*y;return z;}
float Store(Real value) {
    float result;
    asm volatile("fldt %1; fstps %0":"=m"(result):"m"(value):"st");
    if(!std::isfinite(result))throw std::runtime_error("Independent Float overflow");
    return result;
}
Real Square(Vec point) {
    const auto x=Read(point.x),y=Read(point.y),z=Read(point.z);
    return Add(Add(Mul(x,x),Mul(y,y)),Mul(z,z));
}
float Distance(Vec target,Vec source) {
    const auto dz=Sub(Read(target.z),Read(source.z));
    const auto dy=Sub(Read(target.y),Read(source.y));
    const auto dx=Sub(Read(target.x),Read(source.x));
    return Store(Add(Add(Mul(dx,dx),Mul(dy,dy)),Mul(dz,dz)));
}
Real Range(bool player,std::uint8_t visibility) {
    auto value=Mul(static_cast<int>(visibility)+16,
        static_cast<Real>(std::bit_cast<float>(std::uint32_t{0x3c75c28fu})));
    if(value>=1)value=1;
    value=Mul(value,player?5000:4000);
    value=Mul(value,value);
    const Real maximum=player?25000000:16000000;
    return value>=maximum?maximum:value;
}
Vec Eye(Vec point,float height) {
    point.z=Store(Add(Read(point.z),Read(height)));return point;
}
Vec Head(Vec point,float height) {
    const auto product=Mul(Read(height),static_cast<Real>(
        std::bit_cast<double>(std::uint64_t{0x3fe999999999999aull})));
    point.z=Store(Add(product,Read(point.z)));return point;
}
struct Corners {
    std::array<Vec,4u> points{};
    std::size_t minimum{},maximum{};
};
Corners Select(Vec point,float radius) {
    Corners result;
    const auto r=Read(radius),negative=Mul(r,-1);
    // Independently spell each retail corner; do not use the production
    // corner builder or a generalized min/max algorithm as the oracle.
    result.points[0].x=Store(Sub(Read(point.x),r));
    result.points[0].y=Store(Sub(Read(point.y),negative));
    result.points[0].z=point.z;
    result.points[1].y=Store(Add(r,Read(point.y)));
    result.points[1].x=Store(Add(r,Read(point.x)));
    result.points[1].z=point.z;
    result.points[2].y=Store(Sub(Read(point.y),r));
    result.points[2].x=Store(Sub(Read(point.x),r));
    result.points[2].z=point.z;
    result.points[3].y=Store(Add(negative,Read(point.y)));
    result.points[3].x=Store(Add(r,Read(point.x)));
    result.points[3].z=point.z;
    float low=Store(Square(result.points[0])),high=low;
    for(std::size_t i=1u;i<4u;++i) {
        const auto square=Square(result.points[i]);
        if(square>Read(high)) {high=Store(square);result.maximum=i;}
        else if(square<Read(low)) {low=Store(square);result.minimum=i;}
    }
    return result;
}
bool Random(std::uint32_t draw) {
    if(draw>32767u)throw std::runtime_error("Independent non-15-bit draw");
    const auto retained=Mul(static_cast<Real>(draw),static_cast<Real>(
        std::bit_cast<float>(std::uint32_t{0x38000100u})));
    const float unused=Store(retained);static_cast<void>(unused);
    return retained>=static_cast<Real>(std::bit_cast<double>(std::uint64_t{0x3fe0000000000000ull}));
}
Real Representation(const Math::Dyadic& value) {
    Require((value.significand>>64u)==0u,"Production result is not a bounded PC64 significand");
    auto result=std::ldexp(static_cast<Real>(static_cast<std::uint64_t>(value.significand)),value.exponent);
    return value.negative?-result:result;
}
} // namespace Hardware

template<class A,class B,class Equal>void Compare(A actualOperation,B oracleOperation,Equal equal,const char* why) {
    using Result=decltype(actualOperation());
    using Expected=decltype(oracleOperation());
    bool actualRefused{},expectedRefused{};Result actual{};Expected expected{};
    try{actual=actualOperation();}catch(const std::exception&){actualRefused=true;}
    try{expected=oracleOperation();}catch(const std::exception&){expectedRefused=true;}
    Require(actualRefused==expectedRefused,"LOS numeric refusal differs from independent x87");
    if(!actualRefused)Require(equal(actual,expected),why);
    else ++refusals;
    ++hardwareCases;
}
void Check(Vec point,Vec source,float height,float radius) {
    Compare([&]{return Math::DistanceSquared(point,source);},[&]{return Hardware::Distance(point,source);},
        [](float a,float b){return Bits(a)==Bits(b);},"LOS distance differs from independent x87");
    Compare([&]{return Math::Eye(point,height);},[&]{return Hardware::Eye(point,height);},
        [](Vec a,Vec b){return Same(a,b);},"LOS eye differs from independent x87");
    Compare([&]{return Math::Head(point,height);},[&]{return Hardware::Head(point,height);},
        [](Vec a,Vec b){return Same(a,b);},"LOS head differs from independent x87");
    Compare([&]{return Math::SelectCorners(point,radius);},[&]{return Hardware::Select(point,radius);},
        [](const auto& a,const auto& b) {
            if(a.minimum!=b.minimum||a.maximum!=b.maximum)return false;
            for(std::size_t i=0u;i<4u;++i)if(!Same(a.points[i],b.points[i]))return false;
            return true;
        },"LOS corner bits/strict-extrema ordering differs from independent x87");
}
std::uint64_t Next(std::uint64_t& state) {
    state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;
}
float Broad(std::uint64_t& state) {
    const auto bits=static_cast<std::uint32_t>(Next(state));
    return std::bit_cast<float>((bits&0x807fffffu)|(((bits>>23u)%255u)<<23u));
}
float Bounded(std::uint64_t& state,float divisor) {
    return static_cast<float>(static_cast<int>(Next(state)%65537u)-32768)/divisor;
}
void Independent() {
    Hardware::Precision precision;
    for(bool player:{false,true})for(unsigned visibility=0u;visibility<256u;++visibility) {
        const auto actual=Hardware::Representation(Math::RangeSquared(player,static_cast<std::uint8_t>(visibility)));
        const auto expected=Hardware::Range(player,static_cast<std::uint8_t>(visibility));
        Require(actual==expected,"Retained LOS visibility range differs from independent x87");
        ++hardwareCases;
    }
    for(std::uint32_t draw=0u;draw<32768u;++draw) {
        Require(Math::RandomVisible(draw)==Hardware::Random(draw),"LOS retained random comparison differs from x87");
        ++hardwareCases;
    }
    std::uint64_t state=0x5d962f1a3870c4ebull;
    for(unsigned i=0u;i<30000u;++i) {
        const bool broad=i<10000u;
        const auto coordinate=[&]{return broad?Broad(state):Bounded(state,16.0f);};
        const Vec point{coordinate(),coordinate(),coordinate()},source{coordinate(),coordinate(),coordinate()};
        const float height=broad?Broad(state):Bounded(state,128.0f);
        const float radius=broad?Broad(state):Bounded(state,256.0f);
        Check(point,source,height,radius);
    }
    for(float center:{std::numeric_limits<float>::denorm_min(),std::ldexp(1.0f,-100),1.0f,500.0f,1000.0f}) {
        float x=center;
        for(unsigned i=0u;i<16u;++i)x=std::nextafter(x,0.0f);
        for(unsigned i=0u;i<33u;++i) {
            for(float side:{-1.0f,-0.0f,0.0f,1.0f,std::ldexp(1.0f,-12)})
                for(float radius:{-1.0f,-0.0f,0.0f,1.0f,x})
                    Check({x,side,side},{side,0,0},side,radius);
            x=std::nextafter(x,std::numeric_limits<float>::infinity());
        }
    }
    for(unsigned mask=0u;mask<256u;++mask) {
        std::array<float,8u> zero{};
        for(unsigned i=0u;i<zero.size();++i)zero[i]=(mask&(1u<<i))?-0.0f:0.0f;
        Check({zero[0],zero[1],zero[2]},{zero[3],zero[4],zero[5]},zero[6],zero[7]);
    }
    Check({1,std::ldexp(1.0f,-12),0},{},0,0);
    Check({1,std::ldexp(1.0f,-12),std::ldexp(1.0f,-13)},{},0,0);
    Check({1,0,-0.8f},{std::ldexp(1.0f,-25),0,0},1,0);
    const float maximum=std::numeric_limits<float>::max();
    Check({maximum,0,maximum},{-maximum,0,-maximum},maximum,maximum);
}
#else
void Independent() {
    std::cout<<"SKIP independent hardware x87 PC64 oracle on this platform; portable LOS math controls still run\n";
}
#endif
} // namespace

int main() {
    try {
        Deterministic();ActiveFieldsAndSafety();portableChecks=checks;Independent();
        std::cout<<"PASS original Pawn LOS detached math: "<<checks<<" checks / "<<portableChecks
            <<" portable checks / "<<hardwareCases<<" independent hardware x87 cases / "<<refusals
            <<" explicit refusals; not branch protocol, BSP visibility, RNG mutation, enemy writes, Training damage or headset evidence\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL original Pawn LOS math: "<<error.what()<<'\n';return 1;
    }
}
