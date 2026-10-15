#pragma once
#include "quest_actor_timer.h"
#include <array>

namespace QuestVr {
struct NativeWindowClip {
    float originX{}, originY{}, clipX{}, clipY{}, clipWidth{}, clipHeight{};
};

namespace NativeWindowClipDetail {
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Wide;

inline Dyadic Negate(Dyadic value) { value.negative=!value.negative;return value; }

inline Dyadic Normalize64(Dyadic value) {
    const auto bits=ActorTimerDetail::Bits(value.significand);
    if (bits > 64u) throw std::runtime_error("Native window arithmetic requires PC64 operands");
    if (bits) {
        const auto shift=64u-bits;
        if (value.exponent < std::numeric_limits<int>::min()+static_cast<int>(shift))
            throw std::runtime_error("Native window arithmetic exponent underflow refused");
        value.significand<<=shift;value.exponent-=static_cast<int>(shift);
    }
    return value;
}

// Normalization makes exponent comparison a magnitude comparison: each nonzero
// significand occupies exactly 64 bits. Signed zero compares numerically equal.
inline int Compare64(Dyadic a,Dyadic b) {
    a=Normalize64(a);b=Normalize64(b);
    if (!a.significand && !b.significand) return 0;
    if (!a.significand) return b.negative ? 1 : -1;
    if (!b.significand) return a.negative ? -1 : 1;
    if (a.negative != b.negative) return a.negative ? -1 : 1;
    const int magnitude=a.exponent!=b.exponent ? (a.exponent<b.exponent ? -1 : 1) :
        (a.significand<b.significand ? -1 : a.significand>b.significand ? 1 : 0);
    return a.negative ? -magnitude : magnitude;
}

using Words=std::array<std::uint64_t,3u>;
inline Words Shifted(std::uint64_t value,unsigned shift) {
    Words out{};
    const auto word=shift/64u,bit=shift%64u;
    out.at(word)=value<<bit;
    if (bit) out.at(word+1u)=value>>(64u-bit);
    return out;
}
inline int CompareWords(const Words& a,const Words& b) {
    for (std::size_t index=a.size();index--;) {
        if (a[index]!=b[index]) return a[index]<b[index] ? -1 : 1;
    }
    return 0;
}
inline Words AddWords(const Words& a,const Words& b) {
    Words out{};Wide carry{};
    for (std::size_t index=0u;index<out.size();++index) {
        const Wide sum=Wide{a[index]}+b[index]+carry;
        out[index]=static_cast<std::uint64_t>(sum);carry=sum>>64u;
    }
    if (carry) throw std::runtime_error("Native window exact addition overflow refused");
    return out;
}
inline Words SubtractWords(const Words& a,const Words& b) {
    Words out{};Wide borrow{};
    for (std::size_t index=0u;index<out.size();++index) {
        const Wide subtrahend=Wide{b[index]}+borrow;
        out[index]=static_cast<std::uint64_t>(Wide{a[index]}-subtrahend);
        borrow=Wide{a[index]}<subtrahend ? 1u : 0u;
    }
    if (borrow) throw std::runtime_error("Native window exact subtraction underflow refused");
    return out;
}
inline bool Bit(const Words& value,unsigned index) {
    return ((value.at(index/64u)>>(index%64u))&1u)!=0u;
}
inline bool AnyBelow(const Words& value,unsigned count) {
    for (unsigned index=0u;index<count;++index) if (Bit(value,index)) return true;
    return false;
}
inline Dyadic RoundWords(const Words& value,int exponent,bool negative) {
    unsigned bits{};
    for (std::size_t index=value.size();index--;) {
        if (value[index]) {bits=static_cast<unsigned>(index*64u)+ActorTimerDetail::Bits(value[index]);break;}
    }
    if (bits<=64u) return {value[0u],exponent,negative};
    const auto shift=bits-64u;
    const auto word=shift/64u,bit=shift%64u;
    std::uint64_t integer=value.at(word)>>bit;
    if (bit && word+1u<value.size()) integer|=value[word+1u]<<(64u-bit);
    const bool increment=Bit(value,shift-1u) && (AnyBelow(value,shift-1u) || (integer&1u));
    if (exponent>std::numeric_limits<int>::max()-static_cast<int>(shift)-1)
        throw std::runtime_error("Native window arithmetic exponent overflow refused");
    exponent+=static_cast<int>(shift);
    if (increment && integer==std::numeric_limits<std::uint64_t>::max())
        return {Wide{1u}<<63u,exponent+1,negative};
    return {Wide{integer}+(increment ? 1u : 0u),exponent,negative};
}

// Exact alignment followed by nearest-even rounding, independent of host
// long-double width. At most 65 places of normalized exponent separation need
// alignment. Above that, the tiny operand is STRICTLY below half of even the
// smaller ULP below a power-of-two boundary, so it cannot change the result.
// Three limbs retain the 129-bit aligned value, cancellation and all tie bits.
inline Dyadic Add64(Dyadic a,Dyadic b) {
    a=Normalize64(a);b=Normalize64(b);
    if (!a.significand) {
        if (!b.significand) return {0u,0,a.negative && b.negative};
        return b;
    }
    if (!b.significand) return a;
    if (a.exponent<b.exponent) std::swap(a,b);
    const auto difference=static_cast<std::int64_t>(a.exponent)-b.exponent;
    if (difference>65) return a;
    const auto x=Shifted(static_cast<std::uint64_t>(a.significand),static_cast<unsigned>(difference));
    const auto y=Shifted(static_cast<std::uint64_t>(b.significand),0u);
    if (a.negative==b.negative) return RoundWords(AddWords(x,y),b.exponent,a.negative);
    const auto order=CompareWords(x,y);
    if (!order) return {}; // Exact cancellation is +0 under nearest-even.
    return order>0 ? RoundWords(SubtractWords(x,y),b.exponent,a.negative) :
        RoundWords(SubtractWords(y,x),b.exponent,b.negative);
}
inline float AddAndStore(float a,float b) {
    return ActorTimerDetail::Store32(Add64(ActorTimerDetail::Decode(a),ActorTimerDetail::Decode(b)));
}
} // namespace NativeWindowClipDetail

// XWindow.ClipTree10051160 arithmetic only; ownership, GC propagation and
// unconditional descendant traversal belong in the runtime transaction.
// Parentless clipping does not read X/Y. Finite active inputs are required, and
// binary32 storage overflow is refused without changing any caller input.
inline NativeWindowClip ComputeNativeWindowClip(const NativeWindowClip* parent,
    float x,float y,float width,float height) {
    if (!std::isfinite(width) || !std::isfinite(height))
        throw std::runtime_error("Native window clip requires finite dimensions");
    if (!parent) return {0.0f,0.0f,0.0f,0.0f,width,height};
    for (float value:{x,y,parent->originX,parent->originY,parent->clipX,parent->clipY,
        parent->clipWidth,parent->clipHeight})
        if (!std::isfinite(value)) throw std::runtime_error("Native window clip requires finite active inputs");
    using namespace NativeWindowClipDetail;
    using ActorTimerDetail::Decode;
    using ActorTimerDetail::Store32;
    NativeWindowClip result;
    result.originY=AddAndStore(parent->originY,y);
    result.originX=AddAndStore(x,parent->originX);
    float left=AddAndStore(result.originX,0.0f),top=AddAndStore(result.originY,0.0f);
    const float parentLeft=AddAndStore(parent->clipX,parent->originX);
    const float parentTop=AddAndStore(parent->clipY,parent->originY);
    float right=AddAndStore(left,width);
    auto bottom=Add64(Decode(top),Decode(height)); // Remains live PC64.
    const auto parentRight=Add64(Decode(parentLeft),Decode(parent->clipWidth));
    const float parentBottom=AddAndStore(parentTop,parent->clipHeight);
    if (left<parentLeft) left=parentLeft;
    if (top<parentTop) top=parentTop;
    if (Compare64(Decode(right),parentRight)>0) right=Store32(parentRight);
    if (Compare64(bottom,Decode(parentBottom))>0) bottom=Decode(parentBottom);
    result.clipX=Store32(Add64(Decode(left),Negate(Decode(result.originX))));
    result.clipY=Store32(Add64(Decode(top),Negate(Decode(result.originY))));
    result.clipWidth=Store32(Add64(Decode(right),Negate(Decode(left))));
    result.clipHeight=Store32(Add64(bottom,Negate(Decode(top))));
    return result; // Retail preserves negative intersection extents.
}
} // namespace QuestVr
