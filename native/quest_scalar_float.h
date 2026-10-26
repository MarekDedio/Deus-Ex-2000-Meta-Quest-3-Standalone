#pragma once

#include "quest_actor_timer.h"

#include <bit>
#include <cstdint>

namespace QuestVr::ScalarFloat {
// Owned Core.dll scalar natives under the ordinary masked x87 PC64,
// nearest-even profile, with binary32 result slots. No host NaN selection,
// flushing modes, or a finite-only geometry policy for these scalar words.
inline std::uint32_t Bits(float value) { return std::bit_cast<std::uint32_t>(value); }
inline float Word(std::uint32_t bits) { return std::bit_cast<float>(bits); }
inline bool NaN(std::uint32_t bits) { return (bits&0x7fffffffu)>0x7f800000u; }
inline bool Infinity(std::uint32_t bits) { return (bits&0x7fffffffu)==0x7f800000u; }
inline bool Zero(std::uint32_t bits) { return (bits&0x7fffffffu)==0u; }
inline bool Signaling(std::uint32_t bits) { return NaN(bits)&&!(bits&0x00400000u); }
inline std::uint32_t Loaded(std::uint32_t bits) { return NaN(bits)?bits|0x00400000u:bits; }
inline float Load(float value) { return Word(Loaded(Bits(value))); }
inline float Negate(float value) { return Word(Loaded(Bits(value))^0x80000000u); }
inline std::uint32_t SelectNaN(std::uint32_t reg,std::uint32_t memory) {
    // FLD has already quieted the register operand. A signaling memory NaN
    // loses to that quiet register NaN; otherwise the larger significand wins.
    if(!NaN(reg))return Loaded(memory);
    if(!NaN(memory)||Signaling(memory))return reg;
    const auto a=reg&0x7fffffffu,b=memory&0x7fffffffu;
    return a>b?reg:b>a?memory:reg&memory; // Equal magnitude: prefer positive.
}
inline float Store(ActorTimerDetail::Dyadic value) {
    using namespace ActorTimerDetail;
    std::uint32_t bits=value.negative?0x80000000u:0u;
    if(value.significand) {
        const int top=value.exponent+static_cast<int>(ActorTimerDetail::Bits(value.significand))-1;
        int quantum=std::max(top-23,-149);
        const int shift=quantum-value.exponent;
        Wide rounded=shift>=0?RoundShift(value.significand,static_cast<unsigned>(shift)):
            value.significand<<static_cast<unsigned>(-shift);
        if(ActorTimerDetail::Bits(rounded)>24u) {rounded>>=1u;++quantum;}
        if(quantum>104)bits|=0x7f800000u;
        else if(rounded<0x800000u&&quantum==-149)bits|=static_cast<std::uint32_t>(rounded);
        else if(rounded) {
            bits|=static_cast<std::uint32_t>(quantum+150)<<23u;
            bits|=static_cast<std::uint32_t>(rounded)&0x7fffffu;
        }
    }
    return Word(bits);
}
enum class Operation { Multiply,Divide,Add,Subtract };
inline float Binary(Operation op,float lhs,float rhs) {
    const auto a=Bits(lhs),b=Bits(rhs);
    // Multiply/Add load RHS first; Divide/Subtract load LHS first. This is
    // observable when FLD quiets a signaling NaN before the memory operation.
    const bool reverse=op==Operation::Multiply||op==Operation::Add;
    const auto reg=Loaded(reverse?b:a),memory=reverse?a:b;
    if(NaN(reg)||NaN(memory))return Word(SelectNaN(reg,memory));
    const auto sign=(a^b)&0x80000000u;
    if(op==Operation::Multiply) {
        if((Infinity(a)&&Zero(b))||(Zero(a)&&Infinity(b)))return Word(0xffc00000u);
        if(Infinity(a)||Infinity(b))return Word(sign|0x7f800000u);
        auto x=ActorTimerDetail::Decode(lhs),y=ActorTimerDetail::Decode(rhs);
        return Store({x.significand*y.significand,x.exponent+y.exponent,sign!=0u});
    }
    if(op==Operation::Divide) {
        if((Zero(a)&&Zero(b))||(Infinity(a)&&Infinity(b)))return Word(0xffc00000u);
        if(Infinity(a)||Zero(b))return Word(sign|0x7f800000u);
        if(Zero(a)||Infinity(b))return Word(sign);
        auto result=ActorTimerDetail::Divide64(ActorTimerDetail::Decode(lhs),ActorTimerDetail::Decode(rhs));
        result.negative=sign!=0u;return Store(result);
    }
    const auto adjusted=op==Operation::Subtract?b^0x80000000u:b;
    if(Infinity(a)&&Infinity(adjusted)&&((a^adjusted)&0x80000000u))return Word(0xffc00000u);
    if(Infinity(a))return Word(a);
    if(Infinity(adjusted))return Word(adjusted);
    return Store(ActorTimerDetail::AddStoredFloats(lhs,Word(adjusted)));
}
inline bool Compare(std::uint16_t native,float lhs,float rhs) {
    const auto a=Bits(lhs),b=Bits(rhs);
    const bool unordered=NaN(a)||NaN(b);
    const bool equal=!unordered&&(a==b||(Zero(a)&&Zero(b)));
    const bool less=!unordered&&!equal&&(((a^b)&0x80000000u)?(a>>31u)!=0u:
        (a&0x80000000u)?a>b:a<b);
    // Literal FCOMP/FNSTSW C0/C3 predicates, including unordered results.
    switch(native) {
    case 176:return unordered||less;
    case 177:return !unordered&&!less&&!equal;
    case 178:return unordered||less||equal;
    case 179:return !unordered&&!less;
    case 180:return unordered||equal;
    case 181:return !unordered&&!equal;
    default:throw std::invalid_argument("Unknown original scalar comparison");
    }
}
} // namespace QuestVr::ScalarFloat
