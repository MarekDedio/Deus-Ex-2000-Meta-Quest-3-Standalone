#include "quest_native_scalar.h"

#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace QuestVr::NativeScalar;
std::size_t checks{};
void Require(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
enum class Op { Add,Subtract,Multiply,Divide };
[[maybe_unused]] constexpr std::array Operations{Op::Add,Op::Subtract,Op::Multiply,Op::Divide};
[[maybe_unused]] Real Apply(Op op,Real a,Real b) {
    switch(op) {
    case Op::Add:return Add(a,b);
    case Op::Subtract:return Subtract(a,b);
    case Op::Multiply:return Multiply(a,b);
    case Op::Divide:return Divide(a,b);
    }
    throw std::runtime_error("Unknown test operation");
}
float Float(std::uint32_t word) {return std::bit_cast<float>(word);}
double Double(std::uint64_t word) {return std::bit_cast<double>(word);}
void SameFloat(float actual,float expected,const char* message) {
    Require(std::bit_cast<std::uint32_t>(actual)==std::bit_cast<std::uint32_t>(expected),message);
}
void SameDouble(double actual,double expected,const char* message) {
    Require(std::bit_cast<std::uint64_t>(actual)==std::bit_cast<std::uint64_t>(expected),message);
}
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__) && __LDBL_MANT_DIG__ == 64
struct Precision {
    unsigned short prior{};
    Precision() {
        asm volatile("fnstcw %0":"=m"(prior));
        const auto mode=static_cast<unsigned short>((prior&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fnclex; fldcw %0"::"m"(prior));}
};
struct Extended {std::uint64_t significand{};std::uint16_t signExponent{};};
Extended Hardware(long double value) {
    Extended out;
    std::memcpy(&out.significand,&value,sizeof(out.significand));
    std::memcpy(&out.signExponent,reinterpret_cast<const char*>(&value)+8u,sizeof(out.signExponent));
    return out;
}
Extended Emulated(const Real& value) {
    Extended out;out.signExponent=value.finite.negative?0x8000u:0u;
    if(value.kind==Kind::NaN) {
        out.signExponent|=0x7fffu;out.significand=0x8000000000000000ull|value.payload;return out;
    }
    if(value.kind==Kind::Infinity) {
        out.signExponent|=0x7fffu;out.significand=0x8000000000000000ull;return out;
    }
    if(!value.finite.significand)return out;
    const auto bits=QuestVr::ActorTimerDetail::Bits(value.finite.significand);
    const int top=value.finite.exponent+static_cast<int>(bits)-1;
    if(top < -16382) {
        Require(value.finite.exponent>=-16445,"Emulated extended subnormal is not representable");
        out.significand=static_cast<std::uint64_t>(value.finite.significand <<
            static_cast<unsigned>(value.finite.exponent+16445));return out;
    }
    Require(top<=16383&&bits<=64u,"Emulated extended normal is not representable");
    out.signExponent|=static_cast<std::uint16_t>(top+16383);
    out.significand=static_cast<std::uint64_t>(value.finite.significand)<<static_cast<unsigned>(64u-bits);
    return out;
}
float HardwareFloat(long double value) {
    float out{};asm volatile("fldt %1; fstps %0":"=m"(out):"m"(value):"st");return out;
}
double HardwareDouble(long double value) {
    double out{};asm volatile("fldt %1; fstpl %0":"=m"(out):"m"(value):"st");return out;
}
void Same(const Real& actual,long double expected,const char* message) {
    const auto a=Emulated(actual),b=Hardware(expected);
    if(a.significand!=b.significand||a.signExponent!=b.signExponent) {
        std::cerr<<message<<" actual="<<std::hex<<a.signExponent<<':'<<a.significand
            <<" expected="<<b.signExponent<<':'<<b.significand<<std::dec<<'\n';
        throw std::runtime_error(message);
    }
    ++checks;
    SameFloat(StoreFloat(actual),HardwareFloat(expected),"Retained native Float spill mismatch");
    SameDouble(StoreDouble(actual),HardwareDouble(expected),"Retained native Double spill mismatch");
}
long double Oracle(Op op,float a,float b,bool reverse=false) {
    long double out{};
    if(reverse) {
        switch(op) {
        case Op::Add:asm volatile("flds %2; fadds %1; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Subtract:asm volatile("flds %2; fsubrs %1; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Multiply:asm volatile("flds %2; fmuls %1; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Divide:asm volatile("flds %2; fdivrs %1; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        }
    } else {
        switch(op) {
        case Op::Add:asm volatile("flds %1; fadds %2; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Subtract:asm volatile("flds %1; fsubs %2; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Multiply:asm volatile("flds %1; fmuls %2; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Divide:asm volatile("flds %1; fdivs %2; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        }
    }
    return out;
}
long double Oracle(Op op,double a,double b,bool reverse=false) {
    long double out{};
    if(reverse) {
        switch(op) {
        case Op::Add:asm volatile("fldl %2; faddl %1; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Subtract:asm volatile("fldl %2; fsubrl %1; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Multiply:asm volatile("fldl %2; fmull %1; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Divide:asm volatile("fldl %2; fdivrl %1; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        }
    } else {
        switch(op) {
        case Op::Add:asm volatile("fldl %1; faddl %2; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Subtract:asm volatile("fldl %1; fsubl %2; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Multiply:asm volatile("fldl %1; fmull %2; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        case Op::Divide:asm volatile("fldl %1; fdivl %2; fstpt %0":"=m"(out):"m"(a),"m"(b):"st");break;
        }
    }
    return out;
}
long double RegisterOracle(Op op,long double a,long double b) {
    long double out{};
    switch(op) {
    case Op::Add:asm volatile("fldt %1; fldt %2; fadd %%st(1),%%st; fstp %%st(1); fstpt %0":"=m"(out):"m"(a),"m"(b):"st","st(1)");break;
    case Op::Subtract:asm volatile("fldt %1; fldt %2; fsubr %%st(1),%%st; fstp %%st(1); fstpt %0":"=m"(out):"m"(a),"m"(b):"st","st(1)");break;
    case Op::Multiply:asm volatile("fldt %1; fldt %2; fmul %%st(1),%%st; fstp %%st(1); fstpt %0":"=m"(out):"m"(a),"m"(b):"st","st(1)");break;
    case Op::Divide:asm volatile("fldt %1; fldt %2; fdivr %%st(1),%%st; fstp %%st(1); fstpt %0":"=m"(out):"m"(a),"m"(b):"st","st(1)");break;
    }
    return out;
}
unsigned short ComparisonOracle(long double a,long double b) {
    unsigned short out{};
    asm volatile("fldt %2; fldt %1; fcompp; fnstsw %0":"=a"(out):"m"(a),"m"(b):"st","st(1)");return out;
}
long double SqrtOracle(double value) {
    long double out{};asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(out):"m"(value):"st");return out;
}
template<class T> void Pair(T a,T b) {
    for(const auto op:Operations) {
        Same(Apply(op,Load(a),Memory(b)),Oracle(op,a,b),"Native register/memory arithmetic mismatch");
        Same(Apply(op,Memory(a),Load(b)),Oracle(op,a,b,true),"Native memory/register arithmetic mismatch");
    }
    // FLD alone is independently observable: it quiets sNaNs before any op.
    const auto loadedA=Oracle(Op::Multiply,a,T{1}),loadedB=Oracle(Op::Multiply,b,T{1});
    const auto flags=ComparisonOracle(loadedA,loadedB);
    const std::array<bool,6u> expected{(flags&0x0100u)!=0u,(flags&0x4100u)==0u,
        (flags&0x4100u)!=0u,(flags&0x0100u)==0u,(flags&0x4000u)!=0u,(flags&0x4000u)==0u};
    for(std::uint16_t i=0u;i<6u;++i)Require(Compare(176u+i,Load(a),Load(b))==expected[i],"Native comparison C0/C3 mismatch");
    auto retained=Divide(Load(a),Memory(b));auto hardware=Oracle(Op::Divide,a,b);
    for(const auto op:Operations) {
        retained=Apply(op,retained,Load(a));hardware=RegisterOracle(op,hardware,loadedA);
        Same(retained,hardware,"Native retained-register arithmetic chain mismatch");
    }
}
void HardwareChecks() {
    Precision precision;
    const std::vector<std::uint32_t> floats{0u,0x80000000u,1u,0x80000001u,2u,3u,0x007fffffu,
        0x00800000u,0x00800001u,0x3f800000u,0xbf800000u,0x3f000000u,0x40000000u,0x7f7fffffu,
        0xff7fffffu,0x7f800000u,0xff800000u,0x7f800001u,0xff800001u,0x7fbfffffu,0xffbfffffu,
        0x7fc00000u,0xffc00000u,0x7fc00001u,0xffc00001u,0x7fffffffu,0xffffffffu};
    const std::vector<std::uint64_t> doubles{0ull,0x8000000000000000ull,1ull,0x8000000000000001ull,
        0x000fffffffffffffull,0x0010000000000000ull,0x0010000000000001ull,0x3ff0000000000000ull,
        0xbff0000000000000ull,0x3fe0000000000000ull,0x4000000000000000ull,0x7fefffffffffffffull,
        0xffefffffffffffffull,0x7ff0000000000000ull,0xfff0000000000000ull,0x7ff0000000000001ull,
        0xfff0000000000001ull,0x7ff7ffffffffffffull,0xfff7ffffffffffffull,0x7ff8000000000000ull,
        0xfff8000000000000ull,0x7ff8000000000001ull,0xfff8000000000001ull,0x7fffffffffffffffull,
        0xffffffffffffffffull};
    for(auto a:floats)for(auto b:floats)Pair(Float(a),Float(b));
    for(auto a:doubles)for(auto b:doubles)Pair(Double(a),Double(b));
    std::uint64_t stream=0x734b2d0f9266ac81ull;
    const auto draw=[&] {stream^=stream<<13u;stream^=stream>>7u;stream^=stream<<17u;return stream;};
    for(unsigned i=0u;i<12000u;++i) {
        const auto a=draw(),b=draw();Pair(Float(static_cast<std::uint32_t>(a)),Float(static_cast<std::uint32_t>(b)));
        Pair(Double(a),Double(b));Same(Sqrt(Double(a)),SqrtOracle(Double(a)),"Native Double appSqrt mismatch");
    }
    for(auto a:doubles)Same(Sqrt(Double(a)),SqrtOracle(Double(a)),"Native exceptional appSqrt mismatch");
    // Reach extended-range overflow and gradual underflow in registers, not
    // just Float/Double input ranges; compare every intermediate and spill.
    auto high=Load(1.0),low=high;
    long double highOracle=1.0L,lowOracle=1.0L;
    const double factor=Double(0x7fe0000000000000ull);
    const auto factorOracle=Oracle(Op::Multiply,factor,1.0);
    for(unsigned i=0u;i<20u;++i) {
        high=Multiply(high,Memory(factor));low=Divide(low,Memory(factor));
        highOracle=RegisterOracle(Op::Multiply,highOracle,factorOracle);
        lowOracle=RegisterOracle(Op::Divide,lowOracle,factorOracle);
        Same(high,highOracle,"Native extended overflow mismatch");Same(low,lowOracle,"Native extended gradual underflow mismatch");
    }
    std::cout<<"Independent own x87 PC64 retained arithmetic, reverse memory ops, spills and appSqrt verified.\n";
}
#endif
}
int main() {
    try {
        SameFloat(StoreFloat(Divide(Load(-1.0f),Memory(0.0f))),Float(0xff800000u),"Native division infinity mismatch");
        SameDouble(StoreDouble(Divide(Load(0.0),Memory(0.0))),Double(0xfff8000000000000ull),"Native indefinite NaN mismatch");
        SameFloat(StoreFloat(Load(Float(0x7f800001u))),Float(0x7fc00001u),"Native FLD did not quiet sNaN");
        SameDouble(StoreDouble(Negate(Load(Double(0x7ff0000000000001ull)))),Double(0xfff8000000000001ull),"Native FCHS ordering mismatch");
        SameDouble(StoreDouble(Subtract(Add(Load(1.0),Load(Double(0x3c30000000000000ull))),Load(1.0))),
            Double(0x3c30000000000000ull),"Native retained precision rounded too early to binary64");
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__) && __LDBL_MANT_DIG__ == 64
        HardwareChecks();
#endif
        std::cout<<"PASS native retained scalar: "<<checks<<" checks. No original binary execution or gameplay completion claim.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
