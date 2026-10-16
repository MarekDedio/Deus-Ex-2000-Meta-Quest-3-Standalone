#pragma once

#include "quest_model_fast_line_check.h"
#include "quest_script_random.h"

#include <array>
#include <cstddef>

namespace QuestVr::ScriptRandom {

inline constexpr std::size_t DefaultVRandMaxAttempts=4096u;
struct VRandResult {
    std::array<float,3u> vector{};
    std::size_t draws{};
};

namespace VectorDetail {
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Wide;
using ActorTimerDetail::Bits;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Round64;
using ActorTimerDetail::RoundShift;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using ModelFastLineDetail::Divide64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;

inline Dyadic DecodeDouble(const double value) {
    static_assert(sizeof(double)==8u && std::numeric_limits<double>::is_iec559 &&
        std::numeric_limits<double>::digits==53 && std::numeric_limits<double>::max_exponent==1024);
    const auto bits=std::bit_cast<std::uint64_t>(value);
    const auto exponent=static_cast<unsigned>((bits>>52u)&0x7ffu);
    if(exponent==0x7ffu) throw std::invalid_argument("VRand Double input must be finite");
    return {static_cast<Wide>((bits&0x000fffffffffffffull)|(exponent?0x0010000000000000ull:0ull)),
        exponent?static_cast<int>(exponent)-1075:-1074,(bits>>63u)!=0u};
}

inline void ValidateDyadic(const Dyadic& value) {
    if(Bits(value.significand)>64u || value.exponent < -32768 || value.exponent > 32768)
        throw std::invalid_argument("VRand arithmetic requires a bounded PC64 operand");
}

inline double StoreDouble(const Dyadic value) {
    ValidateDyadic(value);
    std::uint64_t bits=value.negative?0x8000000000000000ull:0ull;
    if(value.significand) {
        const int top=value.exponent+static_cast<int>(Bits(value.significand))-1;
        int quantum=std::max(top-52,-1074);
        const int shift=quantum-value.exponent;
        Wide rounded=shift>=0?RoundShift(value.significand,static_cast<unsigned>(shift)):
            value.significand<<static_cast<unsigned>(-shift);
        if(Bits(rounded)>53u) {rounded>>=1u;++quantum;}
        if(quantum>971) throw std::runtime_error("VRand binary64 storage overflow refused");
        if(rounded<0x0010000000000000ull && quantum==-1074) bits|=static_cast<std::uint64_t>(rounded);
        else if(rounded) {
            const int biased=quantum+1075;
            if(biased<=0 || biased>=2047) throw std::runtime_error("VRand binary64 storage boundary refused");
            bits|=static_cast<std::uint64_t>(biased)<<52u;
            bits|=static_cast<std::uint64_t>(rounded)&0x000fffffffffffffull;
        }
    }
    return std::bit_cast<double>(bits);
}

inline Dyadic SqrtStoredDouble64(const double value) {
    const auto input=DecodeDouble(value);
    if(input.negative || !input.significand)
        throw std::invalid_argument("VRand reciprocal square root requires a positive Double");
    const int top=input.exponent+static_cast<int>(Bits(input.significand))-1;
    // Integer division truncates toward zero; the root exponent needs floor.
    const int rootTop=top>=0?top/2:-((-top+1)/2);
    const int quantum=rootTop-63;
    const int shift=input.exponent-2*quantum;
    // A <=53-bit Double significand scales to an exact 127/128-bit integer.
    if(shift<0 || shift>=128 || Bits(input.significand)+static_cast<unsigned>(shift)>128u)
        throw std::runtime_error("VRand square-root integer scaling boundary refused");
    const Wide radicand=input.significand<<static_cast<unsigned>(shift);
    Wide root{};
    // Exactly 64 bounded integer steps; no host sqrt or long-double width.
    for(unsigned bit=64u;bit--;) {
        const Wide candidate=root|(Wide{1u}<<bit);
        if(candidate*candidate<=radicand) root=candidate;
    }
    const Wide remainder=radicand-root*root;
    // Midpoint^2=root^2+root+1/4. The integer radicand cannot tie it;
    // remainder>root is exactly the nearest-even FSQRT PC64 round-up test.
    if(remainder>root) ++root;
    return Round64({root,quantum,false});
}

inline Dyadic RetainedCoordinate(const std::uint32_t draw) {
    if(draw>32767u) throw std::invalid_argument("VRand candidate draw is not a 15-bit CRT sample");
    // appFrand returns its unspilled x87 result. C=32769*2^-30;
    // 2*(draw*C)-1 is exact within PC64, including draw32767=1-2^-29.
    const auto numerator=static_cast<std::int64_t>(draw)*32769ll-(std::int64_t{1}<<29u);
    return {static_cast<Wide>(numerator<0?-numerator:numerator),-29,numerator<0};
}

struct Candidate {
    std::array<float,3u> stored{};
    Dyadic retainedZ{};
    bool accepted{};
};

inline Candidate CandidateFromDraws(const std::array<std::uint32_t,3u>& draws) {
    Candidate result;
    const auto x=RetainedCoordinate(draws[0u]),y=RetainedCoordinate(draws[1u]);
    result.retainedZ=RetainedCoordinate(draws[2u]);
    result.stored={Store32(x),Store32(y),Store32(result.retainedZ)};
    // Retail101390b9 is FST Float, not FSTP. The still-retained Z is
    // multiplied by its Float spill; X/Y have already been popped/stored.
    const auto sum=Add64(Add64(Multiply64(result.retainedZ,Decode(result.stored[2u])),
        Multiply64(Decode(result.stored[1u]),Decode(result.stored[1u]))),
        Multiply64(Decode(result.stored[0u]),Decode(result.stored[0u])));
    result.accepted=Compare64(sum,Decode(1.0f))<=0;
    return result;
}

inline std::array<float,3u> NormalizeStored(const std::array<float,3u>& vector) {
    for(const float component:vector)
        if(!std::isfinite(component)) throw std::invalid_argument("VRand normalization component must be finite");
    const auto x=Decode(vector[0u]),y=Decode(vector[1u]),z=Decode(vector[2u]);
    const auto sum=Add64(Add64(Multiply64(z,z),Multiply64(y,y)),Multiply64(x,x));
    // Retail10139101 spills the recomputed sum to Double. appSqrt then
    // returns FSQRT in PC64 with no Double result spill; FDIVR Double1
    // retains a PC64 reciprocal for all three component Float stores.
    const auto length=SqrtStoredDouble64(StoreDouble(sum));
    const auto reciprocal=ModelFastLineDetail::Divide64(Decode(1.0f),length);
    return {Store32(Multiply64(x,reciprocal)),Store32(Multiply64(y,reciprocal)),
        Store32(Multiply64(z,reciprocal))};
}
} // namespace VectorDetail

inline VRandResult VRand(std::uint32_t& seed,const std::size_t maxAttempts=DefaultVRandMaxAttempts) {
    if(maxAttempts>DefaultVRandMaxAttempts)
        throw std::invalid_argument("VRand attempt budget exceeds its portable per-call work limit");
    VRandResult result;
    for(std::size_t attempt=0u;attempt<maxAttempts;++attempt) {
        const std::array<std::uint32_t,3u> draws{Draw15(seed),Draw15(seed),Draw15(seed)};
        result.draws+=3u;
        const auto candidate=VectorDetail::CandidateFromDraws(draws);
        if(candidate.accepted) {result.vector=VectorDetail::NormalizeStored(candidate.stored);return result;}
    }
    // The original loop is unbounded. This explicit embedding refusal must
    // poison/roll back its ROOT VM transaction, including every consumed RNG
    // draw. Do not silently reseed, return an arbitrary axis or locally undo
    // draws while allowing surrounding original script effects to commit.
    throw std::runtime_error("VRand rejection-sampling attempt budget exhausted");
}
} // namespace QuestVr::ScriptRandom
