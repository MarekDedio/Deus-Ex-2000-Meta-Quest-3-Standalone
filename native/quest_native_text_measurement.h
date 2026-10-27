#pragma once

#include "quest_native_scalar.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace QuestVr::NativeTextMeasurement {

struct Glyph { std::int32_t width{},height{}; };

// Retail copies four byte flags and four raw Plane words. Defined-mask is
// portable diagnostic metadata: native GetTextExtent does not initialize its
// Plane temporary. Unknown words are never used to choose glyphs or extents.
struct State {
    std::uint8_t bold{},explicitColor{},linefeed{},accelerator{};
    std::array<std::uint32_t,4u> planeWords{};
    std::uint8_t planeDefinedMask{};
};
struct Line {
    bool present{};
    std::size_t start{},next{},length{};
    float width{};
};

namespace Detail {
inline NativeScalar::Real Integer(const std::int32_t value) {
    // Every signed32 integer is exactly representable in binary64.
    return NativeScalar::Load(static_cast<double>(value));
}
inline float Store(const NativeScalar::Real& value) {return NativeScalar::StoreFloat(value);}
inline bool LessC0(const NativeScalar::Real& left,const NativeScalar::Real& right) {
    return NativeScalar::Compare(176u,left,right); // Less OR unordered, x87 C0.
}
inline bool GreaterOrdered(const NativeScalar::Real& left,const NativeScalar::Real& right) {
    return NativeScalar::Compare(177u,left,right);
}
inline char16_t Fold(const char16_t value) {
    return value>=u'A'&&value<=u'Z'?static_cast<char16_t>(value+u'a'-u'A'):value;
}
inline int Hex(const char16_t value) {
    if(value>=u'0'&&value<=u'9')return value-u'0';
    if(value>=u'a'&&value<=u'f')return value-u'a'+10;
    if(value>=u'A'&&value<=u'F')return value-u'A'+10;
    return -1;
}
inline bool Whitespace(const char16_t value) {
    return (value>=u'\t'&&value<=u'\r')||value==u' ';
}
template<class Operations>
char16_t Read(Operations& operations,const std::u16string_view text,const std::size_t cursor) {
    operations.Step();return text[cursor];
}
inline void Bounds(const std::u16string_view text,const std::size_t cursor,const std::size_t end) {
    if(cursor>end||end>text.size())
        throw std::runtime_error("Native text scanner cursor/end is outside its owned UTF16 input");
}
template<class Operations>
bool Channel(Operations& operations,const std::u16string_view text,std::size_t& cursor,
    const std::size_t end,std::uint8_t& output) {
    operations.Step();output=0u;
    if(cursor>=end)return false;
    const auto high=Hex(Read(operations,text,cursor));
    if(high<0)return false;
    operations.Step();output=static_cast<std::uint8_t>(high<<4u);++cursor;
    if(cursor>=end)return false;
    const auto low=Hex(Read(operations,text,cursor));
    if(low<0)return false;
    operations.Step();output=static_cast<std::uint8_t>(output|low);++cursor;return true;
}
template<class Operations>
void Rgb(Operations& operations,State& state,const std::array<std::uint8_t,3u>& rgb) {
    // Retail scanner/hex parser uses1005E3E8=3F70101010101027, NOT the
    // SetTextColor reciprocal3F70101010101010 at1005E3C0. Preserve that actual
    // binary64 datum and PC64 multiply/store, not a decimal approximation.
    constexpr auto reciprocalWord=std::uint64_t{0x3f70101010101027uLL};
    const auto reciprocal=NativeScalar::Load(std::bit_cast<double>(reciprocalWord));
    for(std::size_t channel=0u;channel<rgb.size();++channel) {
        operations.Step();
        state.planeWords[channel]=std::bit_cast<std::uint32_t>(
            Store(NativeScalar::Multiply(Integer(rgb[channel]),reciprocal)));
        state.planeDefinedMask|=static_cast<std::uint8_t>(1u<<channel);
    }
}
template<class Operations>
void Restore(Operations& operations,State& state) {
    operations.Step();const auto plane=operations.TextPlaneWords();
    for(std::size_t channel=0u;channel<plane.size();++channel) {
        operations.Step();state.planeWords[channel]=plane[channel];
    }
    state.planeDefinedMask=15u;
}
} // namespace Detail

// Operations owns authentic live GC/font authority and the caller work budget:
// Font (nullable), Step(), NormalFont(), BoldFont(), ParseMetachars(),
// TextPlaneWords(), TextVSpacing(), Glyph(Font,char16_t)->{width,height}.
// Algorithms allocate no retained storage and do not catch callback/budget
// failure. Enclosing VM transaction owns undo of any externally visible writes.
template<class Operations>
char16_t NextChar(Operations& operations,const std::u16string_view text,
    std::size_t& cursor,const std::size_t end,State& state) {
    Detail::Bounds(text,cursor,end);
    operations.Step();state.accelerator=0u;
    for(;;) {
        operations.Step();if(cursor>=end)return u'\0';
        operations.Step();const bool parse=operations.ParseMetachars();
        const auto current=Detail::Read(operations,text,cursor);
        if(!parse||current!=u'|') {operations.Step();++cursor;return current;}
        operations.Step();++cursor;if(cursor>=end)return u'\0';
        auto command=Detail::Fold(Detail::Read(operations,text,cursor));
        bool clearing{};
        if(command==u'!') {
            operations.Step();clearing=true;++cursor;if(cursor>=end)return u'\0';
            command=Detail::Fold(Detail::Read(operations,text,cursor));
        }
        switch(command) {
        case u'b':
            operations.Step();++cursor;state.bold=static_cast<std::uint8_t>(!clearing);break;
        case u'c': {
            operations.Step();++cursor;
            if(clearing)Detail::Restore(operations,state);
            else {
                std::array<std::uint8_t,3u> rgb{};
                if(Detail::Channel(operations,text,cursor,end,rgb[0u]))
                    if(Detail::Channel(operations,text,cursor,end,rgb[1u]))
                        static_cast<void>(Detail::Channel(operations,text,cursor,end,rgb[2u]));
                Detail::Rgb(operations,state,rgb);
            }
            operations.Step();state.explicitColor=static_cast<std::uint8_t>(!clearing);break;
        }
        case u'p': {
            operations.Step();++cursor;
            // Retail tests this bound BEFORE !p's restore branch. A terminal
            // |!p does not restore, whereas terminal |!c and |!b do apply.
            if(cursor>=end)break;
            if(clearing) {
                Detail::Restore(operations,state);operations.Step();state.explicitColor=0u;break;
            }
            const auto digit=Detail::Read(operations,text,cursor);
            if(digit<u'0'||digit>=u'8')break;
            operations.Step();++cursor;
            constexpr std::array<std::array<std::uint8_t,3u>,8u> palette{{
                {{0u,0u,0u}},{{255u,255u,255u}},{{255u,0u,0u}},{{0u,255u,0u}},
                {{255u,255u,0u}},{{0u,0u,255u}},{{255u,0u,255u}},{{0u,255u,255u}}}};
            Detail::Rgb(operations,state,palette[static_cast<std::size_t>(digit-u'0')]);
            operations.Step();state.explicitColor=1u;break;
        }
        case u'&':operations.Step();++cursor;state.accelerator=1u;break;
        default: {
            const auto visible=Detail::Read(operations,text,cursor);operations.Step();++cursor;return visible;
        }
        }
    }
}

template<class Operations>
Line GetLine(Operations& operations,const std::u16string_view text,const std::size_t start,
    const std::size_t end,float destinationWidth,State& outputState) {
    Detail::Bounds(text,start,end);
    operations.Step();
    if(!Detail::GreaterOrdered(NativeScalar::Load(destinationWidth),NativeScalar::Load(0.0f)))
        destinationWidth=std::bit_cast<float>(std::uint32_t{0x48f42400u});
    auto state=outputState;
    const bool previousLinefeed=state.linefeed!=0u;state.linefeed=0u;
    operations.Step();auto selectedFont=state.bold?operations.BoldFont():operations.NormalFont();
    std::size_t cursor=start,lengthEnd=start,next=start;
    std::optional<std::size_t> whitespaceStart,wordStart;
    State wordState{};
    float width{},whitespaceWidth{},resultWidth{};
    bool atWhitespace=true,overflowWhitespace=false,firstCharacter=true;
    for(;;) {
        operations.Step();const auto before=cursor;const auto beforeState=state;
        const auto character=NextChar(operations,text,cursor,end,state);
        if(character==u'\0'||character==u'\n') {
            resultWidth=overflowWhitespace?whitespaceWidth:width;
            lengthEnd=overflowWhitespace?*whitespaceStart:before;
            if(character==u'\n')state.linefeed=1u;
            next=cursor;break;
        }
        if(Detail::Whitespace(character)) {
            if(!atWhitespace) {atWhitespace=true;whitespaceStart=before;whitespaceWidth=width;}
        } else if(atWhitespace) {
            if(overflowWhitespace) {
                resultWidth=whitespaceWidth;lengthEnd=*whitespaceStart;next=before;state=beforeState;break;
            }
            atWhitespace=false;wordStart=before;wordState=beforeState;
        }
        if(beforeState.bold!=state.bold) {
            operations.Step();selectedFont=state.bold?operations.BoldFont():operations.NormalFont();
        }
        if(selectedFont) {
            operations.Step();const auto glyph=operations.Glyph(selectedFont,character);
            const auto candidate=NativeScalar::Add(Detail::Integer(glyph.width),NativeScalar::Load(width));
            if(Detail::GreaterOrdered(candidate,NativeScalar::Load(destinationWidth))&&!firstCharacter) {
                if(!whitespaceStart) {resultWidth=width;lengthEnd=before;next=before;state=beforeState;break;}
                if(wordStart&&*wordStart>*whitespaceStart) {
                    resultWidth=whitespaceWidth;lengthEnd=*whitespaceStart;next=*wordStart;state=wordState;break;
                }
                overflowWhitespace=true;
            }
            width=Detail::Store(candidate);
        }
        firstCharacter=false;
    }
    operations.Step();outputState=state;
    return {next>start||previousLinefeed,start,next,lengthEnd-start,resultWidth};
}

template<class Operations>
std::size_t CStringEnd(Operations& operations,const std::u16string_view text) {
    for(std::size_t end=0u;end<text.size();++end)
        if(Detail::Read(operations,text,end)==u'\0')return end;
    return text.size();
}

template<class Operations>
void GetTextExtent(Operations& operations,const std::u16string_view text,const float destinationWidth,
    float& xExtent,float& yExtent) {
    const auto end=CStringEnd(operations,text);
    State state{};
    operations.Step();auto selectedFont=operations.NormalFont();
    std::uint8_t selectedBold{};
    std::size_t next{};
    float maximumWidth{},totalHeight{},lineHeight{};
    for(;;) {
        // Retail GetLine writes END state into this same local. The second
        // pass intentionally does not restore a saved line-start state.
        const auto line=GetLine(operations,text,next,end,destinationWidth,state);
        next=line.next;if(!line.present)break;
        auto cursor=line.start;const auto segmentEnd=line.start+line.length;
        for(;;) {
            const auto character=NextChar(operations,text,cursor,segmentEnd,state);
            if(character==u'\0')break;
            if(selectedBold!=state.bold) {
                operations.Step();selectedFont=state.bold?operations.BoldFont():operations.NormalFont();selectedBold=state.bold;
            }
            if(character==u'\n'||!selectedFont)continue;
            operations.Step();const auto glyph=operations.Glyph(selectedFont,character);
            const auto candidate=Detail::Integer(glyph.height);
            if(Detail::LessC0(NativeScalar::Load(lineHeight),candidate))lineHeight=Detail::Store(candidate);
        }
        if(Detail::LessC0(NativeScalar::Load(lineHeight),NativeScalar::Load(1.0f))&&selectedFont) {
            operations.Step();lineHeight=Detail::Store(Detail::Integer(operations.Glyph(selectedFont,u' ').height));
        }
        if(Detail::LessC0(NativeScalar::Load(maximumWidth),NativeScalar::Load(line.width)))maximumWidth=line.width;
        operations.Step();const auto spacing=operations.TextVSpacing();
        const auto lineAndSpacing=NativeScalar::Add(NativeScalar::Load(lineHeight),NativeScalar::Load(spacing));
        lineHeight=0.0f;
        totalHeight=Detail::Store(NativeScalar::Add(lineAndSpacing,NativeScalar::Load(totalHeight)));
    }
    operations.Step();xExtent=maximumWidth;
    operations.Step();yExtent=Detail::Store(NativeScalar::Add(NativeScalar::Load(lineHeight),NativeScalar::Load(totalHeight)));
}

template<class Operations>
float GetFontHeight(Operations& operations,const bool includeSpacing) {
    float height{};
    operations.Step();const auto normal=operations.NormalFont();
    if(normal) {
        operations.Step();const auto candidate=Detail::Integer(operations.Glyph(normal,u' ').height);
        if(Detail::LessC0(NativeScalar::Load(0.0f),candidate))height=Detail::Store(candidate);
    }
    operations.Step();const auto bold=operations.BoldFont();
    if(bold) {
        operations.Step();const auto candidate=Detail::Integer(operations.Glyph(bold,u' ').height);
        if(Detail::LessC0(NativeScalar::Load(height),candidate))height=Detail::Store(candidate);
    }
    if(includeSpacing) {
        operations.Step();height=Detail::Store(NativeScalar::Add(NativeScalar::Load(height),NativeScalar::Load(operations.TextVSpacing())));
    }
    return height;
}

} // namespace QuestVr::NativeTextMeasurement
