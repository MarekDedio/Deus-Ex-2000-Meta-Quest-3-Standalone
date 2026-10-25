#include "quest_vector_to_rotator.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

// The oracle uses test-owned x87 instructions only, never original commercial
// code or production fixed/dyadic arithmetic for its expected rotations.
namespace {
using Vec=std::array<float,3u>;
using Rot=std::array<std::int32_t,3u>;
namespace Detail=QuestVr::VectorToRotatorDetail;
std::size_t checks{},portableCases{},oracleCases{},accepted{},ambiguous{},refusals{};
void Require(bool condition,const char* reason) {
    ++checks;if(!condition)throw std::runtime_error(reason);
}
float Float(std::uint32_t bits) {return std::bit_cast<float>(bits);}
Rot Convert(Vec point) {return QuestVr::OriginalVectorToRotator(point);}
void Expected(Vec point,Rot expected) {
    Require(Convert(point)==expected,"Portable original VectorToRotator result differs from explicit control");
    ++portableCases;
}
bool Ambiguity(const std::runtime_error& error) {
    return std::string(error.what())=="Original VectorToRotator quantization interval crosses an integer boundary";
}
void Portable() {
    Expected({1,0,0},{0,0,0});Expected({0,1,0},{0,16383,0});
    Expected({-1,0,0},{0,32767,0});Expected({0,-1,0},{0,-16383,0});
    Expected({0,0,1},{16383,0,0});Expected({0,0,-1},{-16383,0,0});
    Expected({1,1,0},{0,8191,0});Expected({-1,1,0},{0,24575,0});
    Expected({-1,-1,0},{0,-24575,0});Expected({1,-1,0},{0,-8191,0});
    for(unsigned mask=0u;mask<8u;++mask) {
        Vec zero{};
        for(unsigned i=0u;i<3u;++i)zero[i]=(mask&(1u<<i))?-0.0f:0.0f;
        Expected(zero,{0,(mask&1u)?((mask&2u)?-32767:32767):0,0});
    }
    for(float magnitude:{std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::min(),
        1.0f,std::numeric_limits<float>::max()}) {
        Expected({magnitude,0,0},{0,0,0});Expected({0,magnitude,0},{0,16383,0});
        Expected({0,0,magnitude},{16383,0,0});Expected({-magnitude,-0.0f,0},{0,-32767,0});
        Expected({magnitude,magnitude,0},{0,8191,0});
    }
    // Review-provided ordinary Float pairs lie within~2..3e-15 rotation units
    // below integer bins. Global pi-scale error would over-refuse them; the
    // actual angle's extended ulp proves these three observable integers.
    const std::array<Vec,3u> ordinary{{
        {Float(0x4b515fd5u),Float(0x499cdf18u),0},
        {Float(0x4b0489bfu),Float(0x498649d8u),0},
        {Float(0x4b788f01u),Float(0x4b0c4e5bu),0}}};
    const std::array<std::int32_t,3u> yaws{973,1313,5359};
    for(std::size_t i=0u;i<ordinary.size();++i)Expected(ordinary[i],{0,yaws[i],0});
    const std::array<Vec,3u> uncertain{{
        {Float(0xcb6e07b1u),Float(0x47b3a688u),0},
        {Float(0xca36e69cu),Float(0x4b3ce347u),0},
        {Float(0xc6dd5541u),Float(0x4b1f9b3au),0}}};
    for(const auto& point:uncertain) {
        bool refused{};
        try{static_cast<void>(Convert(point));}catch(const std::runtime_error& error){refused=Ambiguity(error);}
        Require(refused,"Genuinely ambiguous near-bin input was silently assigned a rotation");++refusals;++portableCases;
    }
    const std::array<float,4u> invalid{Float(0x7fc12345u),Float(0xffc54321u),
        std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()};
    for(float value:invalid)for(std::size_t axis=0u;axis<3u;++axis) {
        Vec point{1,2,3};point[axis]=value;bool refused{};
        try{static_cast<void>(Convert(point));}catch(const std::runtime_error& error){refused=!Ambiguity(error);}
        Require(refused,"Nonfinite active Vector component was admitted");++refusals;++portableCases;
    }
    // Independent integer identities exercise carry propagation and the
    // directed rounding of the custom224-bit fixed product/division.
    const auto one=Detail::One;
    Require(Detail::MultiplyFixed(one,one)==one,"Q112 one product changed");
    Require(Detail::MultiplyFixed(one-1u,one-1u)==one-2u,"Q112 low-limb carry product changed");
    Require(Detail::MultiplyFixed(one/2u,one/2u)==one/4u,"Q112 half product changed");
    Require(Detail::MultiplyFixed(one-1u,one/2u)==one/2u-1u,"Q112 directed half-tie truncation changed");
    Require(Detail::MultiplyFixed(Detail::Wide{1u}<<80u,Detail::Wide{1u}<<90u)==
        (Detail::Wide{1u}<<58u),"Q112 cross-limb product bit placement changed");
    Require(Detail::Fraction(1u,3u)==one/3u,"Q112 exact third fraction changed");
    Require(Detail::Fraction(one-1u,one)==one-1u,"Q112 division lost near-one numerator bits");
    Require(Detail::Ratio(Detail::Decode(1.0f),Detail::Decode(2.0f))==one/2u,"Q112 dyadic half ratio changed");
    Require(Detail::Ratio(Detail::Decode(1.0f),Detail::Decode(3.0f))==one/3u,"Q112 shifted dyadic third changed");
    Require(Detail::Ratio(Detail::Decode(Float(0x3f7fffffu)),Detail::Decode(1.0f))==
        one-(Detail::Wide{1u}<<88u),"Q112 shifted dyadic near-one ratio changed");
    Require(Detail::Ratio(Detail::Decode(std::numeric_limits<float>::denorm_min()),
        Detail::Decode(std::numeric_limits<float>::max()))==0u,"Tiny finite ratio did not retain conservative zero-floor support");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Hardware {
using Real=long double;
struct Precision {
    unsigned short before{};
    Precision() {
        static_assert(std::numeric_limits<Real>::digits==64,"Independent oracle requires x87 extended precision");
        asm volatile("fnstcw %0":"=m"(before));
        const auto mode=static_cast<unsigned short>((before&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision(){asm volatile("fldcw %0"::"m"(before));}
};
Real Add(Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;}
Real Multiply(Real a,Real b){volatile Real x=a,y=b,z=x*y;return z;}
double Double(Real value) {
    double result;asm volatile("fldt %1; fstpl %0":"=m"(result):"m"(value):"st");return result;
}
Real Root(double value) {
    Real result;asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(result):"m"(value):"st");return result;
}
Real Atan(double y,double x) {
    Real result;
    asm volatile("fldl %2; fldl %1; fpatan; fstpt %0":"=m"(result):"m"(x),"m"(y):"st","st(1)");
    return result;
}
std::int32_t ScaleTruncate(Real angle) {
    const double scale=std::bit_cast<double>(std::uint64_t{0x40c45f1c0e995ab9ull});
    Real product;
    asm volatile("fldt %1; fmull %2; fstpt %0":"=m"(product):"m"(angle),"m"(scale):"st");
    unsigned short before{};asm volatile("fnstcw %0":"=m"(before));
    const auto chop=static_cast<unsigned short>((before&~0x0c00u)|0x0c00u);
    std::int64_t integer{};
    asm volatile("fldcw %2; fldt %1; fistpq %0; fldcw %3":"=m"(integer):"m"(product),"m"(chop),"m"(before):"st");
    Require(integer>=std::numeric_limits<std::int32_t>::min()&&integer<=std::numeric_limits<std::int32_t>::max(),
        "Independent x87 finite vector unexpectedly overflowed rotation INT");
    return static_cast<std::int32_t>(integer);
}
Rot Oracle(Vec point) {
    const double x=point[0],y=point[1],z=point[2];
    const auto yaw=ScaleTruncate(Atan(y,x));
    const double square=Double(Add(Multiply(static_cast<Real>(y),static_cast<Real>(y)),
        Multiply(static_cast<Real>(x),static_cast<Real>(x))));
    const double horizontal=Double(Root(square));
    const auto pitch=ScaleTruncate(Atan(z,horizontal));return {pitch,yaw,0};
}
} // namespace Hardware
void Check(Vec point) {
    const auto expected=Hardware::Oracle(point);bool refused{};Rot actual{};
    const auto x=Detail::Decode(point[0]),y=Detail::Decode(point[1]);
    const double actualSquare=Detail::StoreDouble(Detail::Add64(Detail::Multiply64(y,y),Detail::Multiply64(x,x)));
    const double oracleSquare=Hardware::Double(Hardware::Add(
        Hardware::Multiply(static_cast<Hardware::Real>(point[1]),static_cast<Hardware::Real>(point[1])),
        Hardware::Multiply(static_cast<Hardware::Real>(point[0]),static_cast<Hardware::Real>(point[0]))));
    Require(std::bit_cast<std::uint64_t>(actualSquare)==std::bit_cast<std::uint64_t>(oracleSquare),
        "Original PC64 XY square/Double spill differs from independent x87");
    const double actualRoot=actualSquare==0.0?0.0:Detail::StoreDouble(Detail::SqrtStoredDouble64(actualSquare));
    const double oracleRoot=Hardware::Double(Hardware::Root(oracleSquare));
    Require(std::bit_cast<std::uint64_t>(actualRoot)==std::bit_cast<std::uint64_t>(oracleRoot),
        "Original PC64 sqrt/Double root spill differs from independent x87");
    try{actual=Convert(point);}catch(const std::runtime_error& error) {
        Require(Ambiguity(error),"Finite vector refused for a reason other than certified quantization ambiguity");
        refused=true;++ambiguous;
    }
    if(!refused){Require(actual==expected,"Certified general rotation differs from independent hardware x87");++accepted;}
    ++oracleCases;
}
std::uint64_t Next(std::uint64_t& state) {
    state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;
}
float Broad(std::uint64_t& state) {
    const auto bits=static_cast<std::uint32_t>(Next(state));
    return Float((bits&0x807fffffu)|(((bits>>23u)%255u)<<23u));
}
void Independent() {
    Hardware::Precision precision;
    std::uint64_t state=0x0da9b3c5e1627f48ull;
    for(unsigned i=0u;i<20000u;++i)Check({Broad(state),Broad(state),Broad(state)});
    const std::array<float,13u> edges{0.0f,-0.0f,std::numeric_limits<float>::denorm_min(),
        -std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::min(),
        -std::numeric_limits<float>::min(),1.0f,-1.0f,Float(0x3f7fffffu),Float(0x3f800001u),
        std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),Float(0x00800001u)};
    for(float x:edges)for(float y:edges)for(float z:edges)Check({x,y,z});
    // Complete finite exponent-ratio span, including subnormal and signed-zero
    // inputs. No Float-product/root or host-libm approximation supplies expecteds.
    for(int exponent=-149;exponent<=127;++exponent) {
        const float small=std::ldexp(1.0f,exponent);
        for(float large:{std::numeric_limits<float>::denorm_min(),1.0f,std::numeric_limits<float>::max()}) {
            Check({small,large,small});Check({large,small,large});
            Check({-small,large,-small});Check({large,-small,-large});
        }
    }
    const std::array<Vec,6u> fixtures{{
        {Float(0x4b515fd5u),Float(0x499cdf18u),0},
        {Float(0x4b0489bfu),Float(0x498649d8u),0},
        {Float(0x4b788f01u),Float(0x4b0c4e5bu),0},
        {Float(0xcb6e07b1u),Float(0x47b3a688u),0},
        {Float(0xca36e69cu),Float(0x4b3ce347u),0},
        {Float(0xc6dd5541u),Float(0x4b1f9b3au),0}}};
    for(const auto& fixture:fixtures) {
        float x=fixture[0];for(unsigned i=0u;i<8u;++i)x=std::nextafter(x,-std::numeric_limits<float>::infinity());
        for(unsigned i=0u;i<17u;++i) {
            float y=fixture[1];for(unsigned j=0u;j<8u;++j)y=std::nextafter(y,0.0f);
            for(unsigned j=0u;j<17u;++j) {
                Check({x,y,0});Check({x,-y,0});Check({y,0,x});
                y=std::nextafter(y,std::numeric_limits<float>::infinity());
            }
            x=std::nextafter(x,std::numeric_limits<float>::infinity());
        }
    }
    Require(accepted>20000u,"General certified conversion did not admit the broad ordinary vector cohort");
    Require(ambiguous>=3u,"Independent near-bin cohort failed to exercise honest certificate refusal");
}
#else
void Independent() {
    std::cout<<"SKIP independent x87 oracle on this platform; portable explicit VectorToRotator controls still run\n";
}
#endif
} // namespace
int main() {
    try {
        Portable();Independent();
        std::cout<<"PASS original VectorToRotator certified math checks="<<checks<<" portable="<<portableCases
            <<" x87="<<oracleCases<<" accepted="<<accepted<<" ambiguous="<<ambiguous<<" explicitRefusals="<<refusals
            <<"; observable INT certificate only, not unique FPATAN bits, all-finite fidelity, VM conversion or gameplay proof\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL original VectorToRotator certified math: "<<error.what()<<'\n';return 1;
    }
}
