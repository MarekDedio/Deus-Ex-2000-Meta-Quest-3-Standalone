#pragma once

#include "quest_script_random_vector.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace QuestVr::VectorToRotatorDetail {
using ActorTimerDetail::Bits;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Round64;
using ActorTimerDetail::Wide;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Normalize64;
using ScriptRandom::VectorDetail::DecodeDouble;
using ScriptRandom::VectorDetail::SqrtStoredDouble64;
using ScriptRandom::VectorDetail::StoreDouble;
using Signed=__int128;

inline constexpr unsigned FractionBits=112u;
inline constexpr Wide One=Wide{1u}<<FractionBits;
inline constexpr Wide SoftwareError=65536u;
inline constexpr double Scale=std::bit_cast<double>(std::uint64_t{0x40c45f1c0e995ab9ull});

// The fixed-point operands used below are <=1. Four explicit limbs keep the
// complete224-bit product before its directed Q112 truncation; no host libm,
// long-double format or overflowing128-bit intermediate is involved.
inline Wide MultiplyFixed(Wide a,Wide b) {
    if(a>One||b>One)throw std::runtime_error("VectorToRotator fixed product operand exceeds one");
    const std::array<std::uint64_t,2u> x{static_cast<std::uint64_t>(a),static_cast<std::uint64_t>(a>>64u)};
    const std::array<std::uint64_t,2u> y{static_cast<std::uint64_t>(b),static_cast<std::uint64_t>(b>>64u)};
    std::array<std::uint64_t,4u> product{};
    for(std::size_t i=0u;i<2u;++i) {
        Wide carry{};
        for(std::size_t j=0u;j<2u;++j) {
            const Wide sum=Wide{x[i]}*y[j]+product[i+j]+carry;
            product[i+j]=static_cast<std::uint64_t>(sum);carry=sum>>64u;
        }
        product[i+2u]=static_cast<std::uint64_t>(carry);
    }
    const auto low=(product[1u]>>48u)|(product[2u]<<16u);
    const auto high=(product[2u]>>48u)|(product[3u]<<16u);
    return (Wide{high}<<64u)|low;
}

// Exact binary long division for bounded fixed-point fractions. Every
// remainder is <denominator<=2*One, so its next shift fits unsigned128.
inline Wide Fraction(Wide numerator,Wide denominator) {
    if(!denominator||denominator>2u*One||numerator>denominator)
        throw std::runtime_error("VectorToRotator fixed fraction is outside zero to one");
    if(numerator==denominator)return One;
    Wide quotient{},remainder=numerator;
    for(unsigned bit=0u;bit<FractionBits;++bit) {
        remainder<<=1u;quotient<<=1u;
        if(remainder>=denominator){remainder-=denominator;quotient|=1u;}
    }
    return quotient;
}

inline Dyadic Magnitude(Dyadic value) {value.negative=false;return value;}
inline Wide Ratio(Dyadic numerator,Dyadic denominator) {
    numerator=Normalize64(Magnitude(numerator));denominator=Normalize64(Magnitude(denominator));
    if(!denominator.significand||Compare64(numerator,denominator)>0)
        throw std::runtime_error("VectorToRotator atan ratio is outside zero to one");
    if(!numerator.significand)return 0u;
    const int shift=static_cast<int>(FractionBits)+numerator.exponent-denominator.exponent;
    if(shift<0) {
        const auto right=static_cast<unsigned>(-shift);
        return right>=128u?0u:(numerator.significand>>right)/denominator.significand;
    }
    // A normalized at-most64-bit numerator and shift<=112 produce at most176
    // input bits. The quotient is <=One, despite the wider logical dividend.
    if(shift>static_cast<int>(FractionBits))
        throw std::runtime_error("VectorToRotator normalized ratio exponent is invalid");
    const unsigned length=Bits(numerator.significand)+static_cast<unsigned>(shift);
    Wide quotient{},remainder{};
    for(unsigned bit=length;bit--;) {
        const bool incoming=bit>=static_cast<unsigned>(shift)&&
            ((numerator.significand>>(bit-static_cast<unsigned>(shift)))&1u)!=0u;
        remainder=(remainder<<1u)|(incoming?1u:0u);quotient<<=1u;
        if(remainder>=denominator.significand){remainder-=denominator.significand;quotient|=1u;}
    }
    return quotient;
}

inline Signed AtanSeries(Wide argument) {
    if(argument>One/2u)throw std::runtime_error("VectorToRotator atan series lacks its bounded reduction");
    const Wide square=MultiplyFixed(argument,argument);
    Wide power=argument;Signed sum{};
    for(unsigned term=0u;term<128u;++term) {
        const Signed part=static_cast<Signed>(power/(2u*term+1u));
        sum+=(term&1u)?-part:part;
        power=MultiplyFixed(power,square);
    }
    return sum;
}
inline Signed Pi() {
    // Machin's identity: pi=16atan(1/5)-4atan(1/239). Exact rational inputs
    // are rounded downward to Q112 before the bounded alternating series.
    return 16*AtanSeries(Fraction(1u,5u))-4*AtanSeries(Fraction(1u,239u));
}
inline Signed AtanUnit(Wide ratio,Signed pi) {
    if(ratio<=One/2u)return AtanSeries(ratio);
    return pi/4-AtanSeries(Fraction(One-ratio,One+ratio));
}
inline Signed Atan2(Dyadic y,Dyadic x,Signed pi) {
    const bool negativeY=y.negative,negativeX=x.negative;
    y=Magnitude(y);x=Magnitude(x);
    Signed angle{};
    if(!x.significand&&!y.significand)angle=0;
    else if(Compare64(y,x)<=0)angle=AtanUnit(Ratio(y,x),pi);
    else angle=pi/2-AtanUnit(Ratio(x,y),pi);
    if(negativeX)angle=pi-angle;
    return negativeY?-angle:angle;
}
inline Dyadic FixedDyadic(Signed value) {
    const bool negative=value<0;
    return Round64({static_cast<Wide>(negative?-value:value),-static_cast<int>(FractionBits),negative});
}
inline std::int32_t Truncate(Dyadic value) {
    Wide integer{};
    if(value.exponent>=0) {
        if(static_cast<int>(Bits(value.significand))+value.exponent>31)
            throw std::runtime_error("VectorToRotator signed32 conversion overflow refused");
        integer=value.significand<<static_cast<unsigned>(value.exponent);
    } else if(value.exponent>-128)integer=value.significand>>static_cast<unsigned>(-value.exponent);
    if(integer>static_cast<Wide>(std::numeric_limits<std::int32_t>::max()))
        throw std::runtime_error("VectorToRotator signed32 conversion overflow refused");
    const auto result=static_cast<std::int32_t>(integer);return value.negative?-result:result;
}

inline std::int32_t CertifiedAngle(Signed angle) {
    // SOFTWARE certificate, in units of2^-112 radians:
    // Each128-term |t|<=1/2 series has <512 units error (fixed products/divides
    // plus alternating tail); Machin pi has <10240. Ratio/reduction/quadrant
    // error, including both rounded pi fractions, is strictly <65536 units.
    // No result of this approximation is asserted to be an exact FPATAN word.
    const Wide magnitude=static_cast<Wide>(angle<0?-angle:angle);
    const unsigned upperBits=Bits(magnitude+SoftwareError);
    const int top=static_cast<int>(upperBits)-1-static_cast<int>(FractionBits);
    // Intel SDM Vol1§8.3.10, Pentium+ nearest-even FPATAN: error<1 extended
    // ulp, NOT correctly-rounded0.5ulp. This angle-specific ulp is essential
    // near integer bins; a global pi-scale allowance would reject good data.
    // Official primary source, read before implementation:
    // https://cdrdv2-public.intel.com/843827/253665-sdm-vol-1-dec-24.pdf pp8-21/22.
    const int shift=top-63+static_cast<int>(FractionBits);
    const Wide hardwareError=shift<0?1u:Wide{1u}<<static_cast<unsigned>(shift);
    const Signed error=static_cast<Signed>(SoftwareError+hardwareError);
    // Nearest-even PC64 rounding is monotonic, and any actual FPATAN result
    // is already a PC64 value. Rounding these enclosing endpoints therefore
    // cannot exclude it. The final multiply uses the ORIGINAL Double scale.
    const auto scale=DecodeDouble(Scale);
    const auto low=Truncate(Multiply64(FixedDyadic(angle-error),scale));
    const auto high=Truncate(Multiply64(FixedDyadic(angle+error),scale));
    if(low!=high)
        throw std::runtime_error("Original VectorToRotator quantization interval crosses an integer boundary");
    return low;
}
} // namespace QuestVr::VectorToRotatorDetail

namespace QuestVr {
// Core.FVector::Rotation10144390..10144408. A bounded general finite-input
// implementation certifies the final observable INTs, rather than guessing a
// unique CPU's transcendental result. Near-bin uncertainty explicitly refuses.
// Input admission is not a complete all-finite-fidelity or gameplay claim.
inline std::array<std::int32_t,3u> OriginalVectorToRotator(std::array<float,3u> vector) {
    using namespace VectorToRotatorDetail;
    for(float value:vector)if(!std::isfinite(value))
        throw std::runtime_error("Original VectorToRotator requires finite active Vector components");
    const auto x=Decode(vector[0u]),y=Decode(vector[1u]),z=Decode(vector[2u]);
    const Signed pi=Pi();
    const auto yaw=CertifiedAngle(Atan2(y,x,pi));
    const double square=StoreDouble(Add64(Multiply64(y,y),Multiply64(x,x)));
    // Original appSqrt takes the stored Double, returns retained PC64, then
    // FVector.Rotation explicitly stores its root back to DOUBLE for FPATAN.
    const auto root=square==0.0?Dyadic{}:DecodeDouble(StoreDouble(SqrtStoredDouble64(square)));
    const auto pitch=CertifiedAngle(Atan2(z,root,pi));
    return {pitch,yaw,0};
}
} // namespace QuestVr
