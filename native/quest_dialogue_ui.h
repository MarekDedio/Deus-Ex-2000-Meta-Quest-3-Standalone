#pragma once

#include "persona_ui_canvas.h"

#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

namespace QuestVr {

// Pinned original HUDInfoLinkDisplay.InitWindow/CreateControls/DrawBackground/
// DrawBorder defaults. The 256x128 source textures include padded texels;
// DrawTexture clips them to 256x114 and 175x114, never scales the padding.
inline constexpr std::uint32_t kDataLinkWidth=431u, kDataLinkHeight=114u;
inline constexpr PersonaUiRect kDataLinkPortraitRect{20u,25u,64u,64u};
inline constexpr PersonaUiRect kDataLinkNameRect{94u,15u,293u,15u};
inline constexpr PersonaUiRect kDataLinkTextRect{94u,36u,294u,60u};
inline constexpr std::uint32_t kDataLinkColumns=42u, kDataLinkRows=6u;
inline constexpr std::uint32_t kDataLinkCellWidth=7u, kDataLinkCellHeight=10u;
inline constexpr float kDataLinkSecondsPerCharacter=0.03f;
inline constexpr std::array<const char*,2> kDataLinkBackgroundNames{{
    "UserInterface.HUDInfolinkBackground_1","UserInterface.HUDInfolinkBackground_2"}};
inline constexpr std::array<const char*,2> kDataLinkBorderNames{{
    "UserInterface.HUDInfolinkBorder_1","UserInterface.HUDInfolinkBorder_2"}};
inline constexpr const char* kDataLinkNameFont="FontMenuHeaders_DS";
inline constexpr const char* kDataLinkTextFont="FontFixedWidthSmall_DS";

// Advance these together before any tracking/render early return. Audio keeps
// playing through a temporarily lost pose, so the UI's typewriter must use the
// same elapsed time as its expiry rather than a separately clamped frame clock.
// Invalid input is refused without publishing either half of the clock.
inline bool AdvanceDialoguePresentationClock(float seconds,float& remaining,float& elapsed) {
    if (!std::isfinite(seconds) || !std::isfinite(remaining) || !std::isfinite(elapsed) ||
        seconds<0.0f || remaining<0.0f || elapsed<0.0f ||
        seconds>std::numeric_limits<float>::max()-elapsed) return false;
    if (remaining==0.0f) return true;
    const float activeSeconds=std::min(seconds,remaining);
    elapsed+=activeSeconds;
    remaining=std::max(0.0f,remaining-seconds);
    return true;
}

inline bool DataLinkNameEquals(std::string_view a,std::string_view b) {
    if (a.size()!=b.size()) return false;
    for (std::size_t i=0u;i<a.size();++i)
        if (std::tolower(static_cast<unsigned char>(a[i]))!=
            std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}
// Authored DataLinkPlay.infoLinkNames[0..16]. GetDisplayName uses exact native
// string equality and starts with an empty result; unknown speakers stay blank.
inline std::string DataLinkDisplayName(std::string_view bindName) {
    constexpr std::array<std::array<const char*,2>,17> names{{
        {{"AlexJacobson","Alex Jacobson"}},{{"AnnaNavarre","Anna Navarre"}},
        {{"BobPage","Bob Page"}},{{"BobPageAug","Bob Page"}},
        {{"Daedalus","Daedalus"}},{{"GarySavage","Gary Savage"}},
        {{"GuntherHermann","Gunther Hermann"}},{{"Helios","Helios"}},
        {{"Icarus","Icarus"}},{{"JaimeReyes","Jaime Reyes"}},{{"Jock","Jock"}},
        {{"MorganEverett","Morgan Everett"}},{{"PaulDenton","Paul Denton"}},
        {{"SamCarter","Sam Carter"}},{{"StantonDowd","Stanton Dowd"}},
        {{"TracerTong","Tracer Tong"}},{{"WaltonSimons","Walton Simons"}}
    }};
    for (const auto& item:names) if (bindName==item[0]) return item[1];
    return {};
}
// HUDInfoLinkDisplay.SetSpeaker loads Left(BindName,16), with two explicit
// original special cases. The caller resolves this exact texture export and
// leaves a missing portrait empty; it must not substitute another NPC.
inline std::string DataLinkPortraitName(std::string_view bindName,
    std::int32_t missionNumber,std::uint32_t playerSkin=0u) {
    if (bindName.empty() || bindName.size()>4096u)
        throw std::runtime_error("Datalink speaker identity exceeds bound");
    std::string result(bindName.substr(0u,16u));
    if (bindName=="PaulDenton") {
        if (playerSkin>4u) throw std::runtime_error("Datalink Paul portrait skin exceeds original range");
        result="PaulDenton_"+std::to_string(playerSkin+1u);
    } else if (bindName=="BobPage" && missionNumber==15) result="BobPageAug";
    return result;
}

template<class Image>
inline PersonaUiImage BuildDataLinkUiCanvas(const std::array<Image,2>& backgrounds,
    const std::array<Image,2>& borders) {
    PersonaUiImage result{kDataLinkWidth,kDataLinkHeight,
        std::vector<std::uint8_t>(static_cast<std::size_t>(kDataLinkWidth)*kDataLinkHeight*4u,0u)};
    const PersonaUiRect window{0u,0u,kDataLinkWidth,kDataLinkHeight};
    for (std::size_t i=0u;i<2u;++i) {
        ValidatePersonaUiImage(backgrounds[i]); ValidatePersonaUiImage(borders[i]);
        if (backgrounds[i].width!=256u || backgrounds[i].height!=128u ||
            borders[i].width!=256u || borders[i].height!=128u)
            throw std::runtime_error("Datalink original artwork dimensions differ");
        CopyPersonaUiPiece(result,backgrounds[i],static_cast<std::uint32_t>(i)*256u,0u,window,127u);
    }
    for (std::size_t i=0u;i<2u;++i)
        CopyPersonaUiPiece(result,borders[i],static_cast<std::uint32_t>(i)*256u,0u,window,255u);
    return result;
}

inline void AddDataLinkPortrait(PersonaUiImage& canvas,const PersonaUiImage& portrait) {
    ValidatePersonaUiImage(canvas); ValidatePersonaUiImage(portrait);
    if (canvas.width!=kDataLinkWidth || canvas.height!=kDataLinkHeight ||
        portrait.width!=64u || portrait.height!=64u)
        throw std::runtime_error("Datalink portrait/canvas dimensions differ");
    CopyPersonaUiPiece(canvas,portrait,0u,0u,kDataLinkPortraitRect,255u);
}

struct DataLinkTextWindow {
    std::array<std::string,kDataLinkRows> lines;
    std::uint32_t cursorColumn{},cursorRow{};
    bool allTextVisible{};
};

// Bounded fixed-cell progressive text with six visible rows, scrolling older
// rows when the original transmission is longer. This is a CPU compositor,
// not execution of Extension.ComputerWindow's native command queue.
inline DataLinkTextWindow BuildDataLinkTextWindow(std::string_view text,
    std::size_t visibleCharacters=std::numeric_limits<std::size_t>::max()) {
    if (text.size()>16384u) throw std::runtime_error("Datalink speech exceeds display bound");
    DataLinkTextWindow result;
    const auto count=std::min(text.size(),visibleCharacters);
    result.allTextVisible=count==text.size();
    const auto newline=[&]() {
        result.cursorColumn=0u;
        if (result.cursorRow+1u<kDataLinkRows) ++result.cursorRow;
        else {
            for (std::size_t row=0u;row+1u<kDataLinkRows;++row)
                result.lines[row]=std::move(result.lines[row+1u]);
            result.lines.back().clear();
        }
    };
    for (std::size_t i=0u;i<count;++i) {
        char character=text[i];
        if (character=='\r') continue;
        if (character=='\n') { newline(); continue; }
        if (character=='\t') character=' ';
        if (character!=' ' && (i==0u || text[i-1u]==' ' || text[i-1u]=='\n')) {
            std::size_t wordLength{};
            for (std::size_t j=i;j<text.size() && text[j]!=' ' && text[j]!='\n' && text[j]!='\r';++j)
                ++wordLength;
            if (result.cursorColumn>0u && wordLength<=kDataLinkColumns &&
                result.cursorColumn+wordLength>kDataLinkColumns) newline();
        }
        if (result.cursorColumn==kDataLinkColumns) {
            newline();
            if (character==' ') continue;
        }
        if (result.cursorColumn==0u && character==' ') continue;
        result.lines[result.cursorRow]+=character;
        ++result.cursorColumn;
    }
    return result;
}

template<class NameFont,class TextFont,class NameLookup,class TextLookup>
inline void DrawDataLinkUiText(PersonaUiImage& canvas,const NameFont& nameFont,
    const TextFont& textFont,const std::string& displayName,const DataLinkTextWindow& text,
    bool cursorVisible,NameLookup&& nameLookup,TextLookup&& textLookup) {
    ValidatePersonaUiImage(canvas);
    if (canvas.width!=kDataLinkWidth || canvas.height!=kDataLinkHeight || displayName.size()>4096u)
        throw std::runtime_error("Datalink text canvas/name exceeds original bounds");
    DrawPersonaUiText(canvas,nameFont,displayName,kDataLinkNameRect,255u,nameLookup);
    for (std::size_t row=0u;row<text.lines.size();++row) {
        if (text.lines[row].size()>kDataLinkColumns)
            throw std::runtime_error("Datalink line exceeds original text columns");
        for (std::size_t column=0u;column<text.lines[row].size();++column) {
            const auto* glyph=textLookup(static_cast<unsigned char>(text.lines[row][column]));
            if (glyph==nullptr) continue;
            if (glyph->pageIndex>=textFont.pages.size()) throw std::runtime_error("Datalink glyph atlas exceeds font");
            const auto& atlas=textFont.pages[glyph->pageIndex];
            ValidatePersonaUiImage(atlas);
            if (static_cast<std::uint64_t>(glyph->x)+glyph->width>atlas.width ||
                static_cast<std::uint64_t>(glyph->y)+glyph->height>atlas.height)
                throw std::runtime_error("Datalink glyph exceeds atlas");
            for (std::uint32_t y=0u;y<glyph->height && row*kDataLinkCellHeight+y<kDataLinkTextRect.height;++y)
                for (std::uint32_t x=0u;x<glyph->width && column*kDataLinkCellWidth+x<kDataLinkTextRect.width;++x) {
                    const auto offset=(static_cast<std::size_t>(glyph->y+y)*atlas.width+glyph->x+x)*4u;
                    if (atlas.rgba[offset+3u]==0u) continue;
                    SetPersonaUiPixel(canvas,kDataLinkTextRect.x+static_cast<std::uint32_t>(column)*kDataLinkCellWidth+x,
                        kDataLinkTextRect.y+static_cast<std::uint32_t>(row)*kDataLinkCellHeight+y,
                        static_cast<std::uint8_t>((atlas.rgba[offset]*200u+127u)/255u),
                        static_cast<std::uint8_t>((atlas.rgba[offset+1u]*200u+127u)/255u),
                        static_cast<std::uint8_t>((atlas.rgba[offset+2u]*200u+127u)/255u),atlas.rgba[offset+3u]);
                }
        }
    }
    for (std::uint32_t x=0u;x<293u;++x) SetPersonaUiPixel(canvas,94u+x,32u,200u,200u,200u);
    if (cursorVisible && text.cursorColumn<kDataLinkColumns && text.cursorRow<kDataLinkRows)
        for (std::uint32_t y=0u;y<kDataLinkCellHeight;++y)
            for (std::uint32_t x=0u;x<kDataLinkCellWidth;++x)
                SetPersonaUiPixel(canvas,kDataLinkTextRect.x+text.cursorColumn*kDataLinkCellWidth+x,
                    kDataLinkTextRect.y+text.cursorRow*kDataLinkCellHeight+y,255u,255u,255u);
}

} // namespace QuestVr
