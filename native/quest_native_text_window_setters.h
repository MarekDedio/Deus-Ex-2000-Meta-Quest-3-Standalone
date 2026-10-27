#pragma once

#include <bit>
#include <cstdint>
#include <limits>

namespace QuestVr::NativeTextWindowSetters {

static_assert(sizeof(float)==sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559,
    "Original TextWindow Float storage requires binary32");

// Extension.dll1004608B..100460B6 tests only x87 C3 after FCOMP. Both
// equality and unordered set C3. Classifying stored bits avoids quieting a
// signaling NaN or inventing a finite-only input restriction in the port.
inline bool CompareEqualC3(const float left,const float right) noexcept {
    const auto a=std::bit_cast<std::uint32_t>(left);
    const auto b=std::bit_cast<std::uint32_t>(right);
    const auto nan=[](const std::uint32_t bits) noexcept {
        return (bits&0x7f800000u)==0x7f800000u && (bits&0x007fffffu)!=0u;
    };
    if(nan(a) || nan(b)) return true;
    if((a&0x7fffffffu)==0u && (b&0x7fffffffu)==0u) return true;
    return a==b;
}

// The adapter owns the actual Window and caller Execution. Each Step shares
// the root native work budget; each Write uses the journaled native setter.
// AskForReconfigure must perform the genuine current visibility/parent path,
// or propagate its explicit unsupported-service exception. No local catch or
// rollback is legitimate: the enclosing root owns complete undo.
template<class Operations>
void SetAlignments(Operations& operations,const std::uint8_t horizontal,const std::uint8_t vertical) {
    //10046050: no reads, equality shortcut, enum clamp or layout callback.
    operations.Step();operations.WriteHAlignment(horizontal);
    operations.Step();operations.WriteVAlignment(vertical);
}

template<class Operations>
void SetMargins(Operations& operations,const float horizontal,const float vertical) {
    operations.Step();const auto currentHorizontal=operations.ReadHMargin();
    bool unchanged=CompareEqualC3(currentHorizontal,horizontal);
    if(unchanged) {
        operations.Step();const auto currentVertical=operations.ReadVMargin();
        unchanged=CompareEqualC3(currentVertical,vertical);
    }
    if(unchanged) return;
    //100460B8: both incoming raw Float words precede the one layout request.
    operations.Step();operations.WriteHMargin(horizontal);
    operations.Step();operations.WriteVMargin(vertical);
    operations.Step();operations.AskForReconfigure();
}

template<class Operations>
void SetLines(Operations& operations,std::int32_t minimum,std::int32_t maximum) {
    //10046120: signed INT clamps, not arithmetic that can overflow INT_MIN.
    if(minimum<0) minimum=0;
    if(maximum<0) maximum=0;
    if(minimum>maximum) minimum=maximum;
    operations.Step();const auto currentMinimum=operations.ReadMinimumLines();
    bool unchanged=currentMinimum==minimum;
    if(unchanged) {
        operations.Step();const auto currentMaximum=operations.ReadMaximumLines();
        unchanged=currentMaximum==maximum;
    }
    if(unchanged) return;
    operations.Step();operations.WriteMinimumLines(minimum);
    operations.Step();operations.WriteMaximumLines(maximum);
    operations.Step();operations.AskForReconfigure();
}

} // namespace QuestVr::NativeTextWindowSetters
