#pragma once

#include "quest_native_scalar.h"

#include <cstdint>

namespace QuestVr::NativeTextPreferredSize {

// Original TextWindow virtual100465A0, shared by TextLog. The provider owns
// real GC acquire/release, font metrics, native text extent and optional script
// fallback. Exceptions propagate into the one original root journal.
template<class Operations>
void Query(Operations& operations,bool widthSpecified,float& width,bool heightSpecified,float& height) {
    using namespace NativeScalar;
    const auto integer=[](std::int32_t value) {return Load(static_cast<double>(value));};
    operations.Step();auto gc=operations.GetGC();
    if(!gc)return; // No clamp, script fallback or release in the null branch.
    operations.Step();const bool nonempty=operations.HasText();
    if(nonempty) {
        bool constrained{};
        if(!heightSpecified) {
            operations.Step();const auto minimum=operations.MinimumLines();
            if(minimum>=0)constrained=true;
            else {operations.Step();constrained=operations.MaximumLines()>=0;}
        }
        bool wrapped{};
        if(widthSpecified) {operations.Step();wrapped=operations.WordWrap();}
        float destinationWidth{};
        if(wrapped) {
            operations.Step();const auto margin=Load(operations.HorizontalMargin());
            destinationWidth=StoreFloat(Subtract(Load(width),Add(margin,margin)));
        }
        operations.Step();
        if(constrained&&wrapped) {
            float discardedWidth{};operations.Measure(gc,destinationWidth,discardedWidth,height);
        } else operations.Measure(gc,destinationWidth,width,height);
        if(constrained) {
            operations.Step();const float lineHeight=operations.FontHeight(gc,true);
            auto candidate=Load(height);
            operations.Step();const auto minimum=operations.MinimumLines();
            if(minimum>=0) {
                const auto product=Multiply(integer(minimum),Load(lineHeight));
                const auto stored=StoreFloat(product); // FST before extended comparison, not FSTP.
                if(Compare(177u,product,Load(height)))candidate=Load(stored);
            }
            operations.Step();const auto maximum=operations.MaximumLines();
            if(maximum>=0) {
                const auto stored=StoreFloat(Multiply(integer(maximum),Load(lineHeight)));
                // Retail compares ORIGINAL measured output, not min candidate.
                if(Compare(177u,Load(height),Load(stored)))candidate=Load(stored);
            }
            operations.Step();height=StoreFloat(candidate);
        }
        // These stores are ordered even when width/height alias. Specified
        // dimensions do not suppress these original nonempty-text margins.
        operations.Step();const auto h=Load(operations.HorizontalMargin());
        operations.Step();width=StoreFloat(Add(Add(h,h),Load(width)));
        operations.Step();const auto v=Load(operations.VerticalMargin());
        operations.Step();height=StoreFloat(Add(Add(v,v),Load(height)));
    } else {
        operations.Step();const auto background=operations.Background();
        if(background) {
            operations.Step();height=StoreFloat(integer(operations.BackgroundHeight(background)));
            if(!heightSpecified) {
                operations.Step();const auto minimum=operations.MinimumLines();
                if(minimum>=0) {
                    operations.Step();const float lineHeight=operations.FontHeight(gc,true);
                    operations.Step();const auto current=operations.MinimumLines();
                    operations.Step();height=StoreFloat(Multiply(integer(current),Load(lineHeight)));
                } else {
                    operations.Step();const auto maximum=operations.MaximumLines();
                    if(maximum>=0) {
                        operations.Step();const float lineHeight=operations.FontHeight(gc,true);
                        operations.Step();const auto current=operations.MaximumLines();
                        operations.Step();height=StoreFloat(Multiply(integer(current),Load(lineHeight)));
                    }
                }
            }
            // Original reloads Window.Background after the font-height call.
            operations.Step();const auto currentBackground=operations.Background();
            operations.Step();width=StoreFloat(integer(operations.BackgroundWidth(currentBackground)));
        } else {
            operations.Step();operations.BasePreferredSize(widthSpecified,width,heightSpecified,height);
        }
    }
    if(!widthSpecified) {
        operations.Step();const auto minimum=operations.MinimumWidth();
        if(Compare(176u,Load(width),Load(minimum))) {operations.Step();width=minimum;}
    }
    operations.Step();operations.ReleaseGC(gc);
}

} // namespace QuestVr::NativeTextPreferredSize
