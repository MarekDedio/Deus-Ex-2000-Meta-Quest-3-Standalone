#pragma once

#include "quest_native_tile_math.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace QuestVr {
struct NativeTileRow {
    std::int32_t crossSize{},mainExtent{},childCount{};
    bool operator==(const NativeTileRow&) const = default;
};
// One actual packed-word read at the original snapshot points. Providers
// assemble this without callbacks, not four independently reentrant reads.
struct NativeTileFlags {bool wrap{},fillParent{},equalWidth{},equalHeight{};};

namespace NativeTileLayoutDetail {
template<class Operations>
void AlignCrossAxis(Operations& operations,typename Operations::Child child,std::size_t rowIndex,
    const char* alignment,const char* size,const char* position) {
    using namespace NativeTileMath;
    const auto align=operations.ReadTileByte(alignment);
    if(align==1u || align==2u) {
        const auto cross=Integer(operations.ReadRow(rowIndex).crossSize);
        const auto held=Load(operations.ReadHeldFloat(child,size));
        auto offset=Subtract(cross,held);
        if(align==1u) {
            // Original _ftol first, then CDQ/SUB/SAR signed divide by two.
            // C++ signed /2 also truncates toward zero, including INT_MIN.
            offset=Integer(Truncate(offset)/2);
        }
        const auto current=Load(operations.ReadHeldFloat(child,position));
        operations.WriteHeldFloat(child,position,Store(Add(offset,current)));
    } else if(align==3u) {
        const auto cross=Integer(operations.ReadRow(rowIndex).crossSize);
        operations.WriteHeldFloat(child,size,Store(cross));
    }
    // Every other raw Byte keeps the near position/size, without a read of
    // the inactive held dimension or a fabricated enum-range rejection.
}
inline std::int32_t IncrementCount(std::int32_t count) {
    if(count==std::numeric_limits<std::int32_t>::max())
        throw std::runtime_error("Native Tile row child-count overflow refused");
    return count+1;
}
inline std::size_t NextRow(std::size_t index) {
    if(index==std::numeric_limits<std::size_t>::max())
        throw std::runtime_error("Native Tile row-index overflow refused");
    return index+1u;
}
} // namespace NativeTileLayoutDetail

// Owned Extension.dll XTileWindow::ComputeChildSizes10048A90..10049181.
// This mutates native rows and actual child HELD geometry; it does not perform
// ConfigurationChanged's later ConfigureChild calls or publish current layout.
//
// Operations::Child is a nullable handle (Child{} is null). Live field reads,
// writes, typed row storage, finite/identity/list validation and ALL callbacks
// belong to the supplied Operations, which owns the caller's journal/budgets.
// QueryPreferredSize(child) performs the actual holdWidth then holdHeight OUT
// stores itself. QueryPreferredWidth/Height return the actual written Float.
// Step charges one shared bounded work unit at entry and each visited child/
// row; providers must not reset that allowance or invoke script from Step.
// No child list is precollected and no inactive arithmetic field is read early.
template<class Operations>
void ComputeNativeTileChildSizes(Operations& operations,bool widthSpecified,float& width,
    bool heightSpecified,float& height) {
    using namespace NativeTileMath;
    using Child=typename Operations::Child;
    operations.Step();
    const auto initial=operations.ReadTileFlags(); // 10048AB5, one packed read.
    bool fixedWidth{},fixedHeight{};float fillConstraint=0.0f;
    if(!initial.wrap && initial.fillParent) {
        const auto orientation=operations.ReadTileByte("orientation");
        if(orientation==1u) {
            if(widthSpecified) {
                fixedWidth=true;
                const auto margin=Load(operations.ReadTileFloat("hMargin"));
                const auto twice=Add(margin,margin);
                const auto retained=Subtract(Load(width),twice);
                fillConstraint=Store(retained); // FST before retained FCOMP.
                if(Compare(retained,Integer(0))<0)fillConstraint=0.0f;
            }
        } else if(heightSpecified) {
            fixedHeight=true;
            const auto margin=Load(operations.ReadTileFloat("vMargin"));
            const auto twice=Add(margin,margin);
            const auto retained=Subtract(Load(height),twice);
            fillConstraint=Store(retained);
            if(Compare(retained,Integer(0))<0)fillConstraint=0.0f;
        }
    }

    float maximumWidth=0.0f,maximumHeight=0.0f;
    auto child=operations.LastChild();
    while(child!=Child{}) {
        operations.Step();
        if(operations.ReadChildVisible(child)) {
            if(fixedWidth) {
                operations.WriteHeldFloat(child,"holdWidth",fillConstraint);
                const float preferred=operations.QueryPreferredHeight(child,fillConstraint);
                operations.WriteHeldFloat(child,"holdHeight",preferred);
            } else if(fixedHeight) {
                operations.WriteHeldFloat(child,"holdHeight",fillConstraint);
                const float preferred=operations.QueryPreferredWidth(child,fillConstraint);
                operations.WriteHeldFloat(child,"holdWidth",preferred);
            } else operations.QueryPreferredSize(child);
            const auto flags=operations.ReadTileFlags(); // 10048BC3.
            if(flags.equalWidth) {
                const auto current=Load(operations.ReadHeldFloat(child,"holdWidth"));
                if(Compare(Load(maximumWidth),current)<0)
                    maximumWidth=operations.ReadHeldFloat(child,"holdWidth");
            }
            if(flags.equalHeight) {
                const auto current=Load(operations.ReadHeldFloat(child,"holdHeight"));
                if(Compare(Load(maximumHeight),current)<0)
                    maximumHeight=operations.ReadHeldFloat(child,"holdHeight");
            }
        } else {
            operations.WriteHeldFloat(child,"holdWidth",0.0f);
            operations.WriteHeldFloat(child,"holdHeight",0.0f);
            operations.WriteHeldFloat(child,"holdX",0.0f);
            operations.WriteHeldFloat(child,"holdY",0.0f);
        }
        child=operations.PreviousSibling(child);
    }

    const auto equalization=operations.ReadTileFlags(); // 10048C2F.
    if(equalization.equalWidth || equalization.equalHeight) {
        child=operations.FirstVisibleChild();
        while(child!=Child{}) {
            operations.Step();
            // These are separate TEST-memory operations, unlike the earlier
            // packed snapshots, with the width store preceding the height test.
            if(operations.ReadTileBool("bEqualWidth"))
                operations.WriteHeldFloat(child,"holdWidth",maximumWidth);
            if(operations.ReadTileBool("bEqualHeight"))
                operations.WriteHeldFloat(child,"holdHeight",maximumHeight);
            child=operations.NextVisibleSibling(child);
        }
    }

    float wrapBound=1000000.0f;
    if(operations.ReadTileBool("bWrap")) {
        if(widthSpecified && operations.ReadTileByte("orientation")!=1u) {
            const auto margin=Load(operations.ReadTileFloat("hMargin"));
            wrapBound=Store(Subtract(Load(width),Add(margin,margin)));
        }
        if(heightSpecified && operations.ReadTileByte("orientation")==1u) {
            const auto margin=Load(operations.ReadTileFloat("vMargin"));
            wrapBound=Store(Subtract(Load(height),Add(margin,margin)));
        }
    }
    operations.ClearRows();
    std::int32_t crossSize=Truncate(Load(fillConstraint)); // Reached even empty.
    std::int32_t runningExtent{},count{};
    child=operations.FirstVisibleChild();
    while(child!=Child{}) {
        operations.Step();
        const bool vertical=operations.ReadTileByte("orientation")==1u;
        const float cross=operations.ReadHeldFloat(child,vertical?"holdWidth":"holdHeight");
        const float main=operations.ReadHeldFloat(child,vertical?"holdHeight":"holdWidth");
        if(count>0) {
            const auto retained=Add(Integer(runningExtent),Load(main));
            if(Compare(retained,Load(wrapBound))>0) {
                const auto minor=Load(operations.ReadTileFloat("minorSpacing"));
                runningExtent=Truncate(Subtract(Integer(runningExtent),minor));
                operations.AppendRow(NativeTileRow{crossSize,runningExtent,count});
                crossSize=Truncate(Load(fillConstraint));runningExtent=0;count=0;
            }
        }
        const auto minor=Load(operations.ReadTileFloat("minorSpacing"));
        const auto spaced=Add(Integer(runningExtent),minor);
        runningExtent=Truncate(Add(spaced,Load(main)));
        if(Compare(Integer(crossSize),Load(cross))<0)crossSize=Truncate(Load(cross));
        count=NativeTileLayoutDetail::IncrementCount(count);
        child=operations.NextVisibleSibling(child);
    }
    if(count>0) {
        const auto minor=Load(operations.ReadTileFloat("minorSpacing"));
        runningExtent=Truncate(Subtract(Integer(runningExtent),minor));
        operations.AppendRow(NativeTileRow{crossSize,runningExtent,count});
    }

    if(widthSpecified && heightSpecified) {
        float cursorX=operations.ReadTileFloat("hMargin");
        float cursorY=operations.ReadTileFloat("vMargin");
        std::size_t rowIndex{};
        child=operations.FirstVisibleChild();
        while(child!=Child{}) {
            operations.Step();
            const bool vertical=operations.ReadTileByte("orientation")==1u;
            const auto vDirection=operations.ReadTileByte("vDirection");
            if(vDirection==1u) {
                const auto dimension=vertical?Load(operations.ReadHeldFloat(child,"holdHeight")):
                    Integer(operations.ReadRow(rowIndex).crossSize);
                const auto offset=vertical?Add(Load(cursorY),dimension):Add(dimension,Load(cursorY));
                operations.WriteHeldFloat(child,"holdY",Store(Subtract(Load(height),offset)));
            } else operations.WriteHeldFloat(child,"holdY",cursorY);
            if(operations.ReadTileByte("hDirection")==1u) {
                const auto dimension=vertical?Integer(operations.ReadRow(rowIndex).crossSize):
                    Load(operations.ReadHeldFloat(child,"holdWidth"));
                const auto offset=vertical?Add(dimension,Load(cursorX)):Add(Load(cursorX),dimension);
                operations.WriteHeldFloat(child,"holdX",Store(Subtract(Load(width),offset)));
            } else operations.WriteHeldFloat(child,"holdX",cursorX);
            NativeTileLayoutDetail::AlignCrossAxis(operations,child,rowIndex,
                vertical?"hChildAlign":"vChildAlign",vertical?"holdWidth":"holdHeight",vertical?"holdX":"holdY");
            const auto minor=Load(operations.ReadTileFloat("minorSpacing"));
            const auto held=Load(operations.ReadHeldFloat(child,vertical?"holdHeight":"holdWidth"));
            const auto spaced=Add(minor,held);
            const auto row=operations.ReadRow(rowIndex);
            if(row.childCount<=0)
                throw std::runtime_error("Native Tile placement has a malformed row child count");
            const auto advanced=Add(spaced,Load(vertical?cursorY:cursorX));
            operations.WriteRowChildCount(rowIndex,row.childCount-1);
            const auto after=operations.ReadRow(rowIndex);
            if(vertical)cursorY=Store(advanced);else cursorX=Store(advanced);
            if(after.childCount<1) {
                const auto rowCross=Integer(operations.ReadRow(rowIndex).crossSize);
                if(vertical)cursorY=operations.ReadTileFloat("vMargin");
                else cursorX=operations.ReadTileFloat("hMargin");
                const auto major=Load(operations.ReadTileFloat("majorSpacing"));
                const auto gap=Add(rowCross,major);
                const auto next=Add(gap,Load(vertical?cursorX:cursorY));
                if(vertical)cursorX=Store(next);else cursorY=Store(next);
                rowIndex=NativeTileLayoutDetail::NextRow(rowIndex);
            }
            child=operations.NextVisibleSibling(child);
        }
        return; // No preferred output stores on the placement path.
    }

    const bool vertical=operations.ReadTileByte("orientation")==1u;
    float contentWidth=0.0f;auto contentHeight=Integer(0);
    for(std::size_t index=0u;index<operations.RowCount();index=NativeTileLayoutDetail::NextRow(index)) {
        operations.Step();
        if(vertical) {
            const float extent=Store(Integer(operations.ReadRow(index).mainExtent));
            if(Compare(contentHeight,Load(extent))<0)contentHeight=Load(extent);
            const auto cross=Integer(operations.ReadRow(index).crossSize);
            contentWidth=Store(Add(cross,Load(contentWidth)));
            if(index>0u) {
                const auto major=Load(operations.ReadTileFloat("majorSpacing"));
                contentWidth=Store(Add(Load(contentWidth),major));
            }
        } else {
            const auto extent=Integer(operations.ReadRow(index).mainExtent);
            if(Compare(Load(contentWidth),extent)<0)contentWidth=Store(extent);
            const auto cross=Integer(operations.ReadRow(index).crossSize);
            contentHeight=Add(contentHeight,cross);
            if(index>0u) {
                const auto major=Load(operations.ReadTileFloat("majorSpacing"));
                contentHeight=Add(contentHeight,major);
            }
        }
    }
    if(!widthSpecified) {
        const auto margin=Load(operations.ReadTileFloat("hMargin"));
        const auto twice=Add(margin,margin);
        width=Store(Add(twice,Load(contentWidth)));
    }
    if(!heightSpecified) {
        const auto margin=Load(operations.ReadTileFloat("vMargin"));
        const auto twice=Add(margin,margin);
        height=Store(Add(twice,contentHeight));
    }
}
} // namespace QuestVr
