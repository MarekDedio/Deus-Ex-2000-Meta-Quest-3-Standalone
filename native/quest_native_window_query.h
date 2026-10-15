#pragma once
#include <cmath>
#include <stdexcept>

namespace QuestVr {
struct NativeWindowPreferredSize {float width{},height{};};

// Extension.dll1004e790: dimensions not explicitly specified are promoted by
// nonnegative hardcoded sizes. The native cache stores PRE-fallback results,
// and callbacks see the updated cache flags before they run. Operations own
// reflected writes/transactions; this helper does not publish a partial graph.
template<class Operations>
NativeWindowPreferredSize QueryNativeWindowPreferredSize(Operations& operations,
    bool widthSpecified=false,float width=0.0f,bool heightSpecified=false,float height=0.0f) {
    const auto finite=[](float value) {
        if(!std::isfinite(value)) throw std::runtime_error("Native preferred size requires finite Float storage");
        return value;
    };
    if(!widthSpecified) {
        width=finite(operations.ReadFloat("hardcodedWidth"));
        if(width>=0.0f) widthSpecified=true;
        else width=-1.0f;
    } else finite(width);
    if(!heightSpecified) {
        height=finite(operations.ReadFloat("hardcodedHeight"));
        if(height>=0.0f) heightSpecified=true;
        else height=-1.0f;
    } else finite(height);
    if(widthSpecified && heightSpecified) return {width,height};
    if(operations.ReadBool("bNeedsQuery") ||
        operations.ReadBool("bLastWidthSpecified")!=widthSpecified ||
        operations.ReadBool("bLastHeightSpecified")!=heightSpecified ||
        finite(operations.ReadFloat("lastSpecifiedWidth"))!=width ||
        finite(operations.ReadFloat("lastSpecifiedHeight"))!=height) {
        // Native stores height, then updates both specified bits and clears
        // bNeedsQuery, then stores width. No post-callback dirty-bit clearing.
        operations.WriteFloat("lastSpecifiedHeight",height);
        operations.WriteBool("bLastWidthSpecified",widthSpecified);
        operations.WriteBool("bLastHeightSpecified",heightSpecified);
        operations.WriteBool("bNeedsQuery",false);
        operations.WriteFloat("lastSpecifiedWidth",width);
        float callbackWidth=width,callbackHeight=height;
        operations.ParentRequestedPreferredSize(widthSpecified,callbackWidth,heightSpecified,callbackHeight);
        if(!widthSpecified) width=finite(callbackWidth);
        if(!heightSpecified) height=finite(callbackHeight);
        operations.WriteFloat("lastQueryHeight",height);
        operations.WriteFloat("lastQueryWidth",width);
    } else {
        width=finite(operations.ReadFloat("lastQueryWidth"));
        height=finite(operations.ReadFloat("lastQueryHeight"));
    }
    if(width<0.0f) width=finite(operations.FallbackSize(true));
    if(height<0.0f) height=finite(operations.FallbackSize(false));
    // The fallback is not clamped and is deliberately not written to cache.
    return {width,height};
}
} // namespace QuestVr
