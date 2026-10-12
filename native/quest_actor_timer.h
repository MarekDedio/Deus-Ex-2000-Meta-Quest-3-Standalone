#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace QuestVr {
struct ActorTimerStep { float rate{}, counter{}; bool loop{}, expired{}; };

namespace ActorTimerDetail {
// The owned retail ordinary timer phase uses x87 PC64, round-to-nearest-even.
// Integer dyadics avoid host long-double width (MinGW64 vs Android113) and
// double-rounding dependencies. No original DLL is loaded by this helper.
#if !defined(__SIZEOF_INT128__)
#error "Retail timer arithmetic requires compiler unsigned 128-bit integers"
#endif
using Wide = unsigned __int128;
struct Dyadic { Wide significand{}; int exponent{}; bool negative{}; };
inline unsigned Bits(Wide value) {
    unsigned bits{}; while (value) { value >>= 1u; ++bits; } return bits;
}
inline Wide RoundShift(Wide value,unsigned shift) {
    if (!shift) return value;
    if (shift > 128u) return 0u;
    const Wide half=Wide{1u}<<(shift-1u);
    if (shift == 128u) return value > half ? 1u : 0u; // Tie rounds to even zero.
    Wide integer=value>>shift;
    const Wide tail=value&((Wide{1u}<<shift)-1u);
    if (tail > half || (tail == half && (integer&1u))) ++integer;
    return integer;
}
inline Dyadic Round64(Dyadic value) {
    const auto bits=Bits(value.significand);
    if (bits > 64u) {
        const auto shift=bits-64u;
        value.significand=RoundShift(value.significand,shift);
        value.exponent+=static_cast<int>(shift);
        if (Bits(value.significand) > 64u) { value.significand>>=1u; ++value.exponent; }
    }
    return value;
}
inline Dyadic Decode(float value) {
    static_assert(sizeof(float) == 4u && std::numeric_limits<float>::is_iec559 &&
        std::numeric_limits<float>::digits == 24 && std::numeric_limits<float>::max_exponent == 128,
        "Retail timer storage requires IEEE binary32");
    std::uint32_t bits; std::memcpy(&bits,&value,sizeof(bits));
    const auto exponent=(bits>>23u)&255u;
    return {static_cast<Wide>((bits&0x7fffffu)|(exponent ? 0x800000u : 0u)),
        exponent ? static_cast<int>(exponent)-150 : -149,(bits>>31u) != 0u};
}
inline float Store32(Dyadic value) {
    std::uint32_t bits=value.negative ? 0x80000000u : 0u;
    if (value.significand) {
        const int top=value.exponent+static_cast<int>(Bits(value.significand))-1;
        int quantum=std::max(top-23,-149);
        const int shift=quantum-value.exponent;
        Wide rounded=shift >= 0 ? RoundShift(value.significand,static_cast<unsigned>(shift)) :
            value.significand<<static_cast<unsigned>(-shift);
        if (Bits(rounded) > 24u) { rounded>>=1u; ++quantum; }
        if (quantum > 104) throw std::runtime_error("Retail timer binary32 counter overflow refused");
        if (rounded < 0x800000u && quantum == -149) bits|=static_cast<std::uint32_t>(rounded);
        else if (rounded) {
            const int biased=quantum+150;
            if (biased <= 0 || biased >= 255)
                throw std::runtime_error("Retail timer binary32 storage boundary refused");
            bits|=static_cast<std::uint32_t>(biased)<<23u;
            bits|=static_cast<std::uint32_t>(rounded)&0x7fffffu;
        }
    }
    float out; std::memcpy(&out,&bits,sizeof(out)); return out;
}
inline Dyadic AddExactAndRound64(Dyadic a,Dyadic b) {
    if (!a.significand) {
        if (!b.significand) return {0u,0,a.negative && b.negative};
        return b;
    }
    if (!b.significand) return a;
    if (a.exponent < b.exponent) std::swap(a,b);
    const auto shift=static_cast<unsigned>(a.exponent-b.exponent);
    if (shift >= 128u || Bits(a.significand)+shift > 127u)
        throw std::runtime_error("Retail timer exact arithmetic boundary refused");
    const Wide x=a.significand<<shift, y=b.significand;
    if (a.negative == b.negative) return Round64({x+y,b.exponent,a.negative});
    if (x == y) return {};
    return Round64(x > y ? Dyadic{x-y,b.exponent,a.negative} : Dyadic{y-x,b.exponent,b.negative});
}
inline Dyadic AddStoredFloats(float counter,float elapsed) {
    auto a=Decode(counter), b=Decode(elapsed);
    if (a.exponent < b.exponent) std::swap(a,b);
    // Each nonzero input has <=24 bits. If exponent separation exceeds103,
    // its exact tiny addend cannot reach half a PC64 ULP of the leading float.
    if (a.significand && b.significand && a.exponent-b.exponent > 103) return a;
    return AddExactAndRound64(a,b);
}
inline int ComparePositive(const Dyadic& a,const Dyadic& b) {
    if (a.negative && a.significand) return -1;
    if (!a.significand) return b.significand ? -1 : 0;
    const int topA=a.exponent+static_cast<int>(Bits(a.significand));
    const int topB=b.exponent+static_cast<int>(Bits(b.significand));
    if (topA != topB) return topA < topB ? -1 : 1;
    const int exponent=std::min(a.exponent,b.exponent);
    const Wide x=a.significand<<static_cast<unsigned>(a.exponent-exponent);
    const Wide y=b.significand<<static_cast<unsigned>(b.exponent-exponent);
    return x < y ? -1 : x > y ? 1 : 0;
}
inline Dyadic Divide64(const Dyadic& numerator,const Dyadic& denominator) {
    int ratioTop=static_cast<int>(Bits(numerator.significand))-static_cast<int>(Bits(denominator.significand));
    const bool below=ratioTop >= 0 ? numerator.significand < (denominator.significand<<static_cast<unsigned>(ratioTop)) :
        (numerator.significand<<static_cast<unsigned>(-ratioTop)) < denominator.significand;
    if (below) --ratioTop;
    const auto shift=static_cast<unsigned>(63-ratioTop);
    // <=64 numerator bits and <=24 denominator bits make this <=88 bits.
    const Wide dividend=numerator.significand<<shift;
    Wide quotient=dividend/denominator.significand;
    const Wide remainder=dividend%denominator.significand;
    const Wide twice=remainder*2u;
    if (twice > denominator.significand || (twice == denominator.significand && (quotient&1u))) ++quotient;
    return Round64({quotient,numerator.exponent-denominator.exponent-static_cast<int>(shift),false});
}
inline std::uint32_t Signed32WholePeriods(const Dyadic& quotient) {
    Wide count{};
    if (quotient.exponent >= 0) {
        if (static_cast<int>(Bits(quotient.significand))+quotient.exponent > 31)
            throw std::runtime_error("Retail timer signed32 period conversion overflow refused");
        count=quotient.significand<<static_cast<unsigned>(quotient.exponent);
    } else if (quotient.exponent > -128) count=quotient.significand>>static_cast<unsigned>(-quotient.exponent);
    if (!count || count > 0x7fffffffu)
        throw std::runtime_error("Retail timer signed32 period conversion boundary refused");
    return static_cast<std::uint32_t>(count);
}
} // namespace ActorTimerDetail

// Returns callback-ready fields, never mutates its caller. Expiration dispatch
// belongs in the SAME caller transaction as these writes. Inactive signed rates
// and arbitrary finite reflected counters are retained, not clamped/reset.
inline ActorTimerStep AdvanceActorTimer(float rate,float counter,bool loop,float elapsed) {
    if (!std::isfinite(rate) || !std::isfinite(counter) || !std::isfinite(elapsed) || elapsed < 0.0f)
        throw std::runtime_error("Retail timer nonfinite data or negative elapsed refused");
    ActorTimerStep result{rate,counter,loop,false};
    if (rate <= 0.0f) return result;
    using namespace ActorTimerDetail;
    const auto sum=AddStoredFloats(counter,elapsed);
    result.counter=Store32(sum); // Retail FST does not pop the live PC64 sum.
    const auto originalRate=Decode(rate);
    if (ComparePositive(sum,originalRate) < 0) return result;
    const auto count=Signed32WholePeriods(Divide64(sum,originalRate));
    auto product=originalRate;
    product.significand*=count; // <=24+31=55 bits: exact PC64 multiply.
    product.negative=true;
    result.counter=Store32(AddExactAndRound64(sum,product));
    if (!loop) result.rate=0.0f;
    result.expired=true; // Coalesced ONCE, not a loop over missed periods.
    return result;
}
} // namespace QuestVr
