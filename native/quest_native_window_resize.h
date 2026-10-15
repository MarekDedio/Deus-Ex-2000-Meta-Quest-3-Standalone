#pragma once
#include "quest_native_window_clip.h"
#include "quest_native_window_query.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace QuestVr {
namespace NativeWindowResizeDetail {
inline float Finite(float value) {
    if(!std::isfinite(value)) throw std::runtime_error("Native child resize requires finite Float storage");
    return value;
}
inline ActorTimerDetail::Dyadic Float(float value) {return ActorTimerDetail::Decode(Finite(value));}
inline ActorTimerDetail::Dyadic Subtract(ActorTimerDetail::Dyadic left,ActorTimerDetail::Dyadic right) {
    return NativeWindowClipDetail::Add64(left,NativeWindowClipDetail::Negate(right));
}
// Retail _ftol returns a signed64 result, then ResizeChild retains its low32
// bits. Portable geometry admits only the unambiguous signed32 conversion
// domain; it does not claim to reproduce retail wraparound or invalid results.
inline std::int32_t TruncateSigned32(ActorTimerDetail::Dyadic value) {
    using namespace ActorTimerDetail;
    Wide magnitude{};
    if(value.significand) {
        if(value.exponent>=0) {
            if(static_cast<int>(Bits(value.significand))+value.exponent>32)
                throw std::runtime_error("Native child center signed32 conversion overflow refused");
            magnitude=value.significand<<static_cast<unsigned>(value.exponent);
        } else if(value.exponent>-128) magnitude=value.significand>>static_cast<unsigned>(-value.exponent);
    }
    const Wide limit=value.negative ? Wide{0x80000000u} : Wide{0x7fffffffu};
    if(magnitude>limit) throw std::runtime_error("Native child center signed32 conversion overflow refused");
    if(!value.negative) return static_cast<std::int32_t>(magnitude);
    if(magnitude==Wide{0x80000000u}) return std::numeric_limits<std::int32_t>::min();
    return -static_cast<std::int32_t>(magnitude);
}
inline ActorTimerDetail::Dyadic Integer(std::int32_t value) {
    const auto wide=static_cast<std::int64_t>(value);
    return {static_cast<ActorTimerDetail::Wide>(wide<0 ? -wide : wide),0,wide<0};
}
inline float Stretch(float parent,float margin1,float margin0) {
    const auto sum=NativeWindowClipDetail::Add64(Float(margin1),Float(margin0));
    return ActorTimerDetail::Store32(Subtract(Float(parent),sum));
}
inline float Position(std::uint8_t alignment,float parent,float preferred,float margin0) {
    if(alignment!=1u && alignment!=2u) return Finite(margin0); // Raw native copy, including zero sign.
    auto difference=Subtract(Float(parent),Float(preferred));
    if(alignment==1u) {
        --difference.exponent; // Exact multiply by the native double constant 0.5.
        return ActorTimerDetail::Store32(NativeWindowClipDetail::Add64(Integer(TruncateSigned32(difference)),Float(margin0)));
    }
    return ActorTimerDetail::Store32(Subtract(difference,Float(margin0)));
}
} // namespace NativeWindowResizeDetail

// Extension.dll1004eab0. The parent dimensions are captured before the query,
// but alignment and margin0 are read again afterwards: original script query
// callbacks may mutate them. Operations own callbacks and the transaction.
template<class Operations>
void ResizeNativeWindowChild(Operations& operations) {
    const auto parent=operations.CaptureParentSize();
    bool widthSpecified{},heightSpecified{};
    float width=0.0f,height=0.0f;
    if(operations.ReadHorizontalAlignment()==3u) {
        const float margin1=operations.ReadHorizontalMargin1();
        const float margin0=operations.ReadHorizontalMargin0();
        width=NativeWindowResizeDetail::Stretch(parent.width,margin1,margin0);
        widthSpecified=true;
    }
    if(operations.ReadVerticalAlignment()==3u) {
        const float margin1=operations.ReadVerticalMargin1();
        const float margin0=operations.ReadVerticalMargin0();
        height=NativeWindowResizeDetail::Stretch(parent.height,margin1,margin0);
        heightSpecified=true;
    }
    const auto preferred=operations.QueryPreferredSize(widthSpecified,width,heightSpecified,height);
    NativeWindowResizeDetail::Finite(preferred.width);
    NativeWindowResizeDetail::Finite(preferred.height);
    const auto horizontal=operations.ReadHorizontalAlignment();
    const float x=NativeWindowResizeDetail::Position(horizontal,parent.width,preferred.width,operations.ReadHorizontalMargin0());
    const auto vertical=operations.ReadVerticalAlignment();
    const float y=NativeWindowResizeDetail::Position(vertical,parent.height,preferred.height,operations.ReadVerticalMargin0());
    operations.ConfigureChild(x,y,preferred.width,preferred.height);
}
} // namespace QuestVr
