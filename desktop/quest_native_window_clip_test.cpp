#include "quest_native_window_clip.h"
#include <iostream>

namespace {
unsigned checks{},oracleTuples{},arithmeticTuples{};
void Require(bool value,const char* message) {++checks;if (!value) throw std::runtime_error(message);}
std::uint32_t Raw(float value) {std::uint32_t bits;std::memcpy(&bits,&value,4u);return bits;}
float Float(std::uint32_t bits) {float value;std::memcpy(&value,&bits,4u);return value;}
using QuestVr::NativeWindowClip;
using QuestVr::ActorTimerDetail::Dyadic;
std::array<float,6u> Channels(const NativeWindowClip& value) {
    return {value.originX,value.originY,value.clipX,value.clipY,value.clipWidth,value.clipHeight};
}
bool Same(const NativeWindowClip& a,const NativeWindowClip& b) {
    const auto x=Channels(a),y=Channels(b);
    for (std::size_t index=0u;index<x.size();++index) if (Raw(x[index])!=Raw(y[index])) return false;
    return true;
}
NativeWindowClip Compute(const std::array<float,10u>& values) {
    const NativeWindowClip parent{values[0u],values[1u],values[2u],values[3u],values[4u],values[5u]};
    return QuestVr::ComputeNativeWindowClip(&parent,values[6u],values[7u],values[8u],values[9u]);
}
void Refuses(const std::array<float,10u>& values) {
    std::array<std::uint32_t,10u> before{};
    for (std::size_t index=0u;index<values.size();++index) before[index]=Raw(values[index]);
    bool refused{};try {static_cast<void>(Compute(values));} catch (const std::runtime_error&) {refused=true;}
    Require(refused,"Unsafe native window clip input was accepted");
    for (std::size_t index=0u;index<values.size();++index)
        Require(Raw(values[index])==before[index],"Clip refusal mutated an input");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct RetailPrecisionOracle {
    unsigned short original{};
    RetailPrecisionOracle() {
        asm volatile("fnstcw %0" : "=m"(original));
        const unsigned short pc64Nearest=static_cast<unsigned short>((original&~0x0f00u)|0x0300u);
        asm volatile("fldcw %0" : : "m"(pc64Nearest));
    }
    ~RetailPrecisionOracle() {asm volatile("fldcw %0" : : "m"(original));}
};
long double Add(long double a,long double b) {
    volatile long double x=a,y=b,result=x+y;return result;
}
long double Subtract(long double a,long double b) {
    volatile long double x=a,y=b,result=x-y;return result;
}
float Store(long double value) {
    const float result=static_cast<float>(value);
    if (!std::isfinite(result)) throw std::runtime_error("Oracle binary32 overflow");
    return result;
}
long double Extended(const Dyadic& value) {
    const auto magnitude=std::ldexp(static_cast<long double>(static_cast<std::uint64_t>(value.significand)),value.exponent);
    return value.negative ? -magnitude : magnitude;
}
// Independent host x87 arithmetic with the retail's exact Float spill points.
// No original executable or DLL is loaded or called.
NativeWindowClip Oracle(const std::array<float,10u>& v) {
    for (float value:v) if (!std::isfinite(value)) throw std::runtime_error("Oracle requires finite inputs");
    NativeWindowClip out;
    out.originY=Store(Add(v[1u],v[7u]));out.originX=Store(Add(v[6u],v[0u]));
    float left=Store(Add(out.originX,0.0f)),top=Store(Add(out.originY,0.0f));
    const float parentLeft=Store(Add(v[2u],v[0u])),parentTop=Store(Add(v[3u],v[1u]));
    float right=Store(Add(left,v[8u]));
    long double bottom=Add(top,v[9u]);
    const long double parentRight=Add(parentLeft,v[4u]);
    const float parentBottom=Store(Add(parentTop,v[5u]));
    if (left<parentLeft) left=parentLeft;
    if (top<parentTop) top=parentTop;
    if (static_cast<long double>(right)>parentRight) right=Store(parentRight);
    if (bottom>static_cast<long double>(parentBottom)) bottom=parentBottom;
    out.clipX=Store(Subtract(left,out.originX));out.clipY=Store(Subtract(top,out.originY));
    out.clipWidth=Store(Subtract(right,left));out.clipHeight=Store(Subtract(bottom,top));
    return out;
}
void CheckOracle(const std::array<float,10u>& values) {
    bool actualRefused{},oracleRefused{};NativeWindowClip actual,expected;
    try {actual=Compute(values);} catch (const std::runtime_error&) {actualRefused=true;}
    try {expected=Oracle(values);} catch (const std::runtime_error&) {oracleRefused=true;}
    if (actualRefused!=oracleRefused || (!actualRefused && !Same(actual,expected))) {
        std::cerr << std::hex << "Clip oracle mismatch inputs=";
        for (float value:values) std::cerr << Raw(value) << ',';
        std::cerr << " actual=";for (float value:Channels(actual)) std::cerr << Raw(value) << ',';
        std::cerr << " expected=";for (float value:Channels(expected)) std::cerr << Raw(value) << ',';
        std::cerr << std::dec << '\n';throw std::runtime_error("Native window PC64 clip oracle mismatch");
    }
    ++checks;++oracleTuples;
}
void CheckArithmetic(Dyadic a,Dyadic b) {
    using namespace QuestVr::NativeWindowClipDetail;
    const long double x=Extended(a),y=Extended(b);
    const auto actual=Add64(a,b);const long double expected=Add(x,y),stored=Extended(actual);
    if (stored!=expected || (!stored && std::signbit(stored)!=std::signbit(expected))) {
        std::cerr << "PC64 add oracle mismatch a=" << static_cast<std::uint64_t>(a.significand) << ':' << a.exponent
            << ':' << a.negative << " b=" << static_cast<std::uint64_t>(b.significand) << ':' << b.exponent << ':' << b.negative << '\n';
        throw std::runtime_error("Native window PC64 addition oracle mismatch");
    }
    const int compare=x<y ? -1 : x>y ? 1 : 0;
    Require(Compare64(a,b)==compare,"Native window signed PC64 comparison oracle mismatch");
    ++checks;++arithmeticTuples;
}
#endif
} // namespace

int main() {
    try {
        using QuestVr::ComputeNativeWindowClip;
        using namespace QuestVr::NativeWindowClipDetail;
        const float negativeZero=Float(0x80000000u),tiny=std::numeric_limits<float>::denorm_min();
        const float maximum=std::numeric_limits<float>::max();
        const auto root=ComputeNativeWindowClip(nullptr,123.0f,-456.0f,-8.0f,negativeZero);
        Require(Same(root,{0.0f,0.0f,0.0f,0.0f,-8.0f,negativeZero}),"Root clip used X/Y or changed dimensions");
        Require(Same(ComputeNativeWindowClip(nullptr,std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(),-8.0f,negativeZero),root),"Root read unused X/Y");
        NativeWindowClip parent{10.0f,20.0f,2.0f,3.0f,100.0f,80.0f};
        const auto originalParent=parent;
        Require(Same(ComputeNativeWindowClip(&parent,5.0f,6.0f,10.0f,20.0f),
            {15.0f,26.0f,0.0f,0.0f,10.0f,20.0f}),"Contained child clip differs");
        Require(Same(ComputeNativeWindowClip(&parent,-5.0f,-6.0f,10.0f,20.0f),
            {5.0f,14.0f,7.0f,9.0f,3.0f,11.0f}),"Child top/left intersection differs");
        Require(Same(ComputeNativeWindowClip(&parent,-100.0f,0.0f,5.0f,10.0f),
            {-90.0f,20.0f,102.0f,3.0f,-97.0f,7.0f}),"Outside left child extent was clamped");
        Require(Same(ComputeNativeWindowClip(&parent,200.0f,0.0f,10.0f,10.0f),
            {210.0f,20.0f,0.0f,3.0f,-98.0f,7.0f}),"Outside right child extent was clamped");
        Require(Same(parent,originalParent),"Clip computation mutated its parent");
        parent={0.0f,0.0f,0.0f,1.0f,10.0f,1.0f};
        for (int exponent:{-54,-63,-64}) {
            const float residual=std::ldexp(1.0f,exponent);
            const auto clipped=ComputeNativeWindowClip(&parent,0.0f,1.0f,1.0f,residual);
            Require(clipped.clipHeight==(exponent==-64 ? 0.0f : residual),
                "Live child bottom lost PC64 residual or tie rounding");
        }
        parent={negativeZero,negativeZero,negativeZero,negativeZero,10.0f,10.0f};
        const auto signedClip=ComputeNativeWindowClip(&parent,negativeZero,negativeZero,0.0f,0.0f);
        Require(Raw(signedClip.originX)==0x80000000u && Raw(signedClip.originY)==0x80000000u,
            "Negative-zero origins were not preserved");
        Require(Raw(signedClip.clipX)==0u && Raw(signedClip.clipY)==0u &&
            Raw(signedClip.clipWidth)==0u && Raw(signedClip.clipHeight)==0u,"Zero clip spill signs differ");
        const Dyadic one{QuestVr::ActorTimerDetail::Wide{1u}<<63u,-63,false};
        const Dyadic boundaryTiny{std::numeric_limits<std::uint64_t>::max(),-128,true};
        const auto below=Add64(one,boundaryTiny);
        Require(Compare64(below,{std::numeric_limits<std::uint64_t>::max(),-64,false})==0,
            "PC64 power-of-two lower boundary rounding differs");
        Require(Compare64(Add64(one,{std::numeric_limits<std::uint64_t>::max(),-129,true}),one)==0,
            "Beyond-65 exponent separation changed dominant value");
        Require(Compare64(Add64(one,Negate(one)),{})==0 && !Add64(one,Negate(one)).negative,
            "Exact cancellation did not produce positive zero");
        Require(Add64({0u,0,true},{0u,0,true}).negative && !Add64({0u,0,true},{0u,0,false}).negative,
            "PC64 zero-addition signs differ");
        Require(Negate({0u,0,false}).negative && !Negate({0u,0,true}).negative,"Negation failed to flip zero sign");
        Require(Compare64({0u,0,true},{0u,0,false})==0 && Compare64(QuestVr::ActorTimerDetail::Decode(-tiny),{})<0 &&
            Compare64({},QuestVr::ActorTimerDetail::Decode(tiny))<0,"Signed zero/subnormal comparison differs");
        Refuses({maximum,0.0f,0.0f,0.0f,1.0f,1.0f,maximum,0.0f,1.0f,1.0f});
        Refuses({0.0f,maximum,0.0f,0.0f,1.0f,1.0f,0.0f,maximum,1.0f,1.0f});
        Refuses({0.0f,0.0f,maximum,0.0f,1.0f,1.0f,maximum,0.0f,maximum,1.0f});
        Refuses({0.0f,0.0f,0.0f,maximum,1.0f,maximum,0.0f,0.0f,1.0f,1.0f});
        Refuses({-maximum,0.0f,maximum,0.0f,maximum,1.0f,-maximum,0.0f,1.0f,1.0f});
        for (float bad:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()})
            for (std::size_t index=0u;index<10u;++index) {
                std::array<float,10u> values{};values[index]=bad;Refuses(values);
            }
        for (float bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
            bool refused{};try {static_cast<void>(ComputeNativeWindowClip(nullptr,0.0f,0.0f,bad,1.0f));}
            catch (const std::runtime_error&) {refused=true;}
            Require(refused,"Root accepted nonfinite dimension");
        }
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        static_assert(std::numeric_limits<long double>::digits==64,"x87 oracle requires extended significand64");
        RetailPrecisionOracle precision;
        const std::array<float,16u> adversarial{negativeZero,0.0f,tiny,-tiny,Float(0x007fffffu),Float(0x00800000u),
            1.0f,-1.0f,16777216.0f,-16777216.0f,std::ldexp(1.0f,-64),-std::ldexp(1.0f,-64),
            std::ldexp(1.0f,-63),std::ldexp(1.0f,-54),maximum,-maximum};
        for (float origin:adversarial) for (float extent:adversarial) {
            CheckOracle({origin,origin,0.0f,0.0f,extent,extent,0.0f,0.0f,extent,extent});
            CheckOracle({0.0f,0.0f,origin,origin,extent,extent,origin,origin,extent,extent});
            CheckOracle({origin,origin,extent,extent,origin,origin,extent,extent,origin,origin});
            CheckArithmetic(QuestVr::ActorTimerDetail::Decode(origin),QuestVr::ActorTimerDetail::Decode(extent));
        }
        for (int difference:{0,1,23,24,53,54,63,64,65,66,100,300})
            for (std::uint64_t a:{0x8000000000000000ull,0x8000000000000001ull,0xfffffffffffffffeull,0xffffffffffffffffull})
                for (std::uint64_t b:{0x8000000000000000ull,0x8000000000000001ull,0xfffffffffffffffeull,0xffffffffffffffffull})
                    for (bool signA:{false,true}) for (bool signB:{false,true})
                        CheckArithmetic({a,-63,signA},{b,-63-difference,signB});
        std::uint32_t random=0xc11f5116u;
        const auto next=[&]() {random^=random<<13u;random^=random>>17u;random^=random<<5u;return random;};
        const auto finite=[&]() {const auto raw=next();return Float((raw&0x80000000u)|((raw&0x7fffffffu)%0x7f800000u));};
        for (unsigned index=0u;index<30000u;++index) {
            std::array<float,10u> values{};for (auto& value:values) value=finite();
            if ((index&3u)==0u) {values[6u]=0.0f;values[7u]=0.0f;values[8u]=values[4u];values[9u]=values[5u];}
            CheckOracle(values);
            const auto a=(std::uint64_t{next()}<<32u)|next();
            const auto b=(std::uint64_t{next()}<<32u)|next();
            CheckArithmetic({a,static_cast<int>(next()%340u)-213,(next()&1u)!=0u},
                {b,static_cast<int>(next()%340u)-213,(next()&1u)!=0u});
        }
        std::cout << "Independent native clip PC64 x87 oracle tuples: " << oracleTuples << '\n';
        std::cout << "Independent native arithmetic PC64 x87 oracle tuples: " << arithmeticTuples << '\n';
#else
        std::cout << "Independent x87 oracle unavailable on this architecture; deterministic fixtures only\n";
#endif
        std::cout << "Native window clip tests passed: " << checks << " checks\n";return 0;
    } catch (const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
}
