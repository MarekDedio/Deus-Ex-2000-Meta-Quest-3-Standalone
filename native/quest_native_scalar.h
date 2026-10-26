#pragma once

#include "quest_native_window_clip.h"

#include <bit>
#include <cstdint>

namespace QuestVr::NativeScalar {
// Explicit ordinary masked x87 PC64/nearest-even profile. Unlike a Float VM
// result slot, this value stays in the extended register until a named spill.
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Wide;
enum class Kind { Finite,Infinity,NaN };
struct Real {
    Dyadic finite{};
    Kind kind{Kind::Finite};
    std::uint64_t payload{}; // x87 fraction bits62..0; integer bit is implicit.
    bool signaling{};
};
inline bool IsZero(const Real& a) {return a.kind==Kind::Finite&&!a.finite.significand;}
inline Real Infinity(bool negative=false) {return {{0u,0,negative},Kind::Infinity,0u,false};}
inline Real NaN(std::uint64_t payload,bool negative,bool signaling=false) {
    return {{0u,0,negative},Kind::NaN,payload,signaling};
}
inline Real Indefinite() {return NaN(std::uint64_t{1u}<<62u,true);}
inline Real Quiet(Real a) {
    if(a.kind==Kind::NaN){a.payload|=std::uint64_t{1u}<<62u;a.signaling=false;}return a;
}
inline Real Finish(Dyadic a) {
    a=ActorTimerDetail::Round64(a);
    if(a.significand) {
        const int top=a.exponent+static_cast<int>(ActorTimerDetail::Bits(a.significand))-1;
        if(top>16383)return Infinity(a.negative);
        // Extended-format gradual underflow, not a host FTZ/DAZ setting.
        if(a.exponent<-16445) {
            a.significand=ActorTimerDetail::RoundShift(a.significand,static_cast<unsigned>(-16445-a.exponent));
            a.exponent=-16445;
        }
    }
    return {a,Kind::Finite,0u,false};
}
inline Real Memory(float value) {
    const auto bits=std::bit_cast<std::uint32_t>(value);
    const auto exponent=(bits>>23u)&255u,fraction=bits&0x7fffffu;
    if(exponent==255u)return fraction?NaN(std::uint64_t{fraction}<<40u,(bits>>31u)!=0u,!(fraction&0x400000u)):
        Infinity((bits>>31u)!=0u);
    return {ActorTimerDetail::Decode(value),Kind::Finite,0u,false};
}
inline Real Memory(double value) {
    const auto bits=std::bit_cast<std::uint64_t>(value);
    const auto exponent=static_cast<unsigned>((bits>>52u)&2047u);
    const auto fraction=bits&0x000fffffffffffffull;
    if(exponent==2047u)return fraction?NaN(fraction<<11u,(bits>>63u)!=0u,!(fraction&0x0008000000000000ull)):
        Infinity((bits>>63u)!=0u);
    return {{static_cast<Wide>(fraction|(exponent?0x0010000000000000ull:0ull)),
        exponent?static_cast<int>(exponent)-1075:-1074,(bits>>63u)!=0u},Kind::Finite,0u,false};
}
inline Real Load(float value) {return Quiet(Memory(value));}
inline Real Load(double value) {return Quiet(Memory(value));}
inline Real Negate(Real a) {a.finite.negative=!a.finite.negative;return a;}
inline Real PropagateNaN(Real a,Real b) {
    if(a.kind!=Kind::NaN)return Quiet(b);
    if(b.kind!=Kind::NaN)return Quiet(a);
    if(a.signaling!=b.signaling)return Quiet(a.signaling?b:a);
    if(a.payload!=b.payload)return Quiet(a.payload>b.payload?a:b);
    a.finite.negative=a.finite.negative&&b.finite.negative;return Quiet(a);
}
inline Real Add(Real a,Real b) {
    if(a.kind==Kind::NaN||b.kind==Kind::NaN)return PropagateNaN(a,b);
    if(a.kind==Kind::Infinity&&b.kind==Kind::Infinity)
        return a.finite.negative==b.finite.negative?a:Indefinite();
    if(a.kind==Kind::Infinity)return a;
    if(b.kind==Kind::Infinity)return b;
    return Finish(NativeWindowClipDetail::Add64(a.finite,b.finite));
}
inline Real Subtract(Real a,Real b) {
    // FSUB does not invert a NaN payload's sign. Handle it before finite
    // subtraction is expressed as addition of a sign-inverted operand.
    if(a.kind==Kind::NaN||b.kind==Kind::NaN)return PropagateNaN(a,b);
    return Add(a,Negate(b));
}
inline Real Multiply(Real a,Real b) {
    if(a.kind==Kind::NaN||b.kind==Kind::NaN)return PropagateNaN(a,b);
    const bool negative=a.finite.negative!=b.finite.negative;
    if(a.kind==Kind::Infinity||b.kind==Kind::Infinity)
        return IsZero(a)||IsZero(b)?Indefinite():Infinity(negative);
    return Finish({a.finite.significand*b.finite.significand,
        a.finite.exponent+b.finite.exponent,negative});
}
inline Real Divide(Real a,Real b) {
    if(a.kind==Kind::NaN||b.kind==Kind::NaN)return PropagateNaN(a,b);
    const bool negative=a.finite.negative!=b.finite.negative;
    if((IsZero(a)&&IsZero(b))||(a.kind==Kind::Infinity&&b.kind==Kind::Infinity))return Indefinite();
    if(a.kind==Kind::Infinity||IsZero(b))return Infinity(negative);
    if(IsZero(a)||b.kind==Kind::Infinity)return {{0u,0,negative},Kind::Finite,0u,false};
    auto numerator=NativeWindowClipDetail::Normalize64(a.finite);
    auto denominator=NativeWindowClipDetail::Normalize64(b.finite);
    const unsigned shift=numerator.significand<denominator.significand?64u:63u;
    const Wide dividend=numerator.significand<<shift;
    Wide quotient=dividend/denominator.significand;
    const Wide remainder=dividend%denominator.significand,twice=remainder*2u;
    if(twice>denominator.significand||(twice==denominator.significand&&(quotient&1u)))++quotient;
    return Finish({quotient,numerator.exponent-denominator.exponent-static_cast<int>(shift),negative});
}
inline bool Compare(std::uint16_t native,const Real& a,const Real& b) {
    const bool unordered=a.kind==Kind::NaN||b.kind==Kind::NaN;
    int order{};
    if(!unordered) {
        if(a.kind==Kind::Infinity)order=b.kind==Kind::Infinity?
            (a.finite.negative==b.finite.negative?0:a.finite.negative?-1:1):a.finite.negative?-1:1;
        else if(b.kind==Kind::Infinity)order=b.finite.negative?1:-1;
        else order=NativeWindowClipDetail::Compare64(a.finite,b.finite);
    }
    switch(native) {
    case 176:return unordered||order<0;
    case 177:return !unordered&&order>0;
    case 178:return unordered||order<=0;
    case 179:return !unordered&&order>=0;
    case 180:return unordered||order==0;
    case 181:return !unordered&&order!=0;
    default:throw std::invalid_argument("Unknown native scalar x87 comparison");
    }
}
inline std::uint64_t StoreWord(const Real& value,unsigned precision,int minSubnormal,int maxQuantum,
    unsigned exponentBits) {
    const unsigned fractionBits=precision-1u;
    const std::uint64_t sign=value.finite.negative?std::uint64_t{1u}<<(fractionBits+exponentBits):0u;
    const std::uint64_t exponentMask=((std::uint64_t{1u}<<exponentBits)-1u)<<fractionBits;
    if(value.kind==Kind::Infinity)return sign|exponentMask;
    if(value.kind==Kind::NaN)return sign|exponentMask|(value.payload>>(63u-fractionBits))|
        (std::uint64_t{1u}<<(fractionBits-1u));
    auto a=value.finite;
    if(!a.significand)return sign;
    const int top=a.exponent+static_cast<int>(ActorTimerDetail::Bits(a.significand))-1;
    int quantum=std::max(top-static_cast<int>(fractionBits),minSubnormal);
    const int shift=quantum-a.exponent;
    Wide rounded=shift>=0?ActorTimerDetail::RoundShift(a.significand,static_cast<unsigned>(shift)):
        a.significand<<static_cast<unsigned>(-shift);
    if(ActorTimerDetail::Bits(rounded)>precision){rounded>>=1u;++quantum;}
    if(quantum>maxQuantum)return sign|exponentMask;
    const auto hidden=std::uint64_t{1u}<<fractionBits;
    if(rounded<hidden&&quantum==minSubnormal)return sign|static_cast<std::uint64_t>(rounded);
    if(!rounded)return sign;
    const int biased=quantum-minSubnormal+1;
    return sign|(static_cast<std::uint64_t>(biased)<<fractionBits)|(static_cast<std::uint64_t>(rounded)&(hidden-1u));
}
inline float StoreFloat(const Real& value) {
    return std::bit_cast<float>(static_cast<std::uint32_t>(StoreWord(value,24u,-149,104,8u)));
}
inline double StoreDouble(const Real& value) {
    return std::bit_cast<double>(StoreWord(value,53u,-1074,971,11u));
}
inline Real Sqrt(double value) {
    auto a=Load(value);
    if(a.kind==Kind::NaN||IsZero(a))return a;
    if(a.finite.negative)return Indefinite();
    if(a.kind==Kind::Infinity)return a;
    const int top=a.finite.exponent+static_cast<int>(ActorTimerDetail::Bits(a.finite.significand))-1;
    const int rootTop=top>=0?top/2:-((-top+1)/2),quantum=rootTop-63;
    const int shift=a.finite.exponent-2*quantum;
    if(shift<0||shift>=128||ActorTimerDetail::Bits(a.finite.significand)+static_cast<unsigned>(shift)>128u)
        throw std::runtime_error("Native scalar square-root scaling exceeds its exact Double domain");
    const Wide radicand=a.finite.significand<<static_cast<unsigned>(shift);
    Wide root{};
    for(unsigned bit=64u;bit--;) {
        const Wide candidate=root|(Wide{1u}<<bit);
        if(candidate*candidate<=radicand)root=candidate;
    }
    if(radicand-root*root>root)++root;
    return Finish({root,quantum,false});
}
} // namespace QuestVr::NativeScalar
