#pragma once
#include "persona_ui_canvas.h"
#include <cmath>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>

namespace QuestVr {

// Detached typed bindings of the fields used by the original DrawWindow and
// GetDisplayName. This is presentation, not executing those player/UI scripts.
struct FrobDescriptionMessages {
    std::string locked,unlocked,lockStrength,doorStrength,infinite;
};
struct FrobDescriptionPalette {
    std::array<std::uint8_t,3> header{{255u,255u,255u}},border{{255u,255u,255u}},
        background{{127u,127u,127u}};
};
struct FrobDescriptionFields {
    bool mover{},pawn{},inventory{},decoration{};
    std::optional<std::string> familiarName,unfamiliarName,bindName,itemName;
    std::optional<float> lastConversationEndTime;
    std::optional<bool> locked,pickable,breakable;
    std::optional<float> lockStrength,doorStrength;
};

inline std::optional<std::string> OriginalFrobStrengthPercent(float strength) {
    if (!std::isfinite(strength) || strength < 0.0f || strength > 1.0f) return {};
    // FormatString adds .5, converts the Float to its six-decimal native
    // string, then takes the first 1/2/3 bytes according to the rounded value.
    const float n=strength*100.0f+0.5f;
    std::ostringstream text; text.imbue(std::locale::classic());
    text << std::fixed << std::setprecision(6) << n;
    return text.str().substr(0u,n<10.0f?1u:n<100.0f?2u:3u)+"%";
}

inline std::optional<std::string> BuildOriginalFrobDescription(
    const FrobDescriptionFields& fields,const FrobDescriptionMessages& messages) {
    const auto usable=[](const std::optional<std::string>& text) {
        return text && !text->empty() && text->size()<=4096u && text->find('\0')==std::string::npos;
    };
    if (fields.mover) {
        // Missing/wrong-typed bLocked never becomes an invented Unlocked label.
        if (!fields.locked) return {};
        if (!*fields.locked) return messages.unlocked.empty()?std::nullopt:
            std::optional<std::string>{messages.unlocked};
        if (messages.locked.empty() || messages.lockStrength.empty() || messages.doorStrength.empty() ||
            messages.infinite.empty() || !fields.pickable || !fields.breakable) return {};
        const auto strength=[&](bool finite,const std::optional<float>& value)->std::optional<std::string> {
            return !finite?std::optional<std::string>{messages.infinite}:
                value?OriginalFrobStrengthPercent(*value):std::nullopt;
        };
        const auto lock=strength(*fields.pickable,fields.lockStrength);
        const auto door=strength(*fields.breakable,fields.doorStrength);
        if (!lock || !door) return {};
        return messages.locked+"\n"+messages.lockStrength+*lock+"\n"+messages.doorStrength+*door;
    }
    if (fields.inventory) return usable(fields.itemName)?fields.itemName:std::nullopt;
    if (fields.pawn || fields.decoration) {
        // The focus window does not pass bUseFamiliar; therefore only an
        // actually finished conversation enables the familiar name.
        if (usable(fields.familiarName) && fields.lastConversationEndTime &&
            std::isfinite(*fields.lastConversationEndTime) && *fields.lastConversationEndTime>0.0f)
            return fields.familiarName;
        if (usable(fields.unfamiliarName)) return fields.unfamiliarName;
        const auto& fallback=fields.decoration?fields.itemName:fields.bindName;
        return usable(fallback)?fallback:std::nullopt;
    }
    // Carcass/projectile have their own qualified ItemName binding. Unknown
    // actors retain no technical-name or class-substring UI approximation.
    return usable(fields.itemName)?fields.itemName:std::nullopt;
}

struct FrobLabelCanvas {
    PersonaUiImage image;
    std::uint32_t textWidth{},textHeight{};
};
template<class Font,class GlyphLookup>
inline FrobLabelCanvas BuildOriginalFrobLabelCanvas(const Font& font,
    const std::string& text,const PersonaUiImage& background,GlyphLookup&& lookupGlyph,
    const FrobDescriptionPalette& palette={}) {
    ValidatePersonaUiImage(background);
    if (text.empty() || text.size()>4096u || text.find('\0')!=std::string::npos ||
        font.lineHeight==0u || font.lineHeight>120u)
        throw std::runtime_error("Original focus label text/font exceeds its bounds");
    std::uint64_t lineWidth{},maxWidth{}; std::uint32_t lines=1u;
    for (const unsigned char c:text) {
        if (c=='\r') continue;
        if (c=='\n') { maxWidth=std::max(maxWidth,lineWidth);lineWidth=0u;++lines;continue; }
        const auto* glyph=lookupGlyph(c);
        if (glyph) lineWidth+=glyph->width;
    }
    maxWidth=std::max(maxWidth,lineWidth);
    const auto height=static_cast<std::uint64_t>(lines)*font.lineHeight;
    if (maxWidth==0u || maxWidth>504u || height>120u)
        throw std::runtime_error("Original focus label measured extent exceeds 512x128");
    FrobLabelCanvas result{{512u,128u,std::vector<std::uint8_t>(512u*128u*4u,0u)},
        static_cast<std::uint32_t>(maxWidth),static_cast<std::uint32_t>(height)};
    const auto w=result.textWidth+8u,h=result.textHeight+8u;
    for (std::uint32_t y=0u;y<h;++y) for (std::uint32_t x=0u;x<w;++x) {
        const bool border=x==0u||y==0u||x+1u==w||y+1u==h;
        const auto source=(static_cast<std::size_t>(y%background.height)*background.width+x%background.width)*4u;
        const auto tint=[&](std::size_t channel) {return static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(background.rgba[source+channel])*palette.background[channel]+127u)/255u);};
        SetPersonaUiPixel(result.image,x,y,border?palette.border[0]:tint(0u),
            border?palette.border[1]:tint(1u),border?palette.border[2]:tint(2u),
            border?255u:background.rgba[source+3u]);
    }
    // Exact original FontMenuSmall_DS glyphs, unscaled native UVs, four-pixel
    // text padding and selected serialized original HUD palette. Do not tint
    // transparent glyph texels/background or lose FontMenuSmall_DS shadows.
    std::uint32_t penX=4u,penY=4u;
    for (const unsigned char c:text) {
        if (c=='\r') continue;
        if (c=='\n') {penX=4u;penY+=font.lineHeight;continue;}
        const auto* glyph=lookupGlyph(c);if (!glyph) continue;
        if (glyph->pageIndex>=font.pages.size()) throw std::runtime_error("Focus glyph page is outside original font");
        const auto& atlas=font.pages[glyph->pageIndex];ValidatePersonaUiImage(atlas);
        if (static_cast<std::uint64_t>(glyph->x)+glyph->width>atlas.width ||
            static_cast<std::uint64_t>(glyph->y)+glyph->height>atlas.height)
            throw std::runtime_error("Focus glyph is outside original atlas");
        for (std::uint32_t y=0u;y<glyph->height && penY+y<result.textHeight+4u;++y)
            for (std::uint32_t x=0u;x<glyph->width && penX+x<result.textWidth+4u;++x) {
                const auto at=(static_cast<std::size_t>(glyph->y+y)*atlas.width+glyph->x+x)*4u;
                if (atlas.rgba[at+3u]==0u) continue;
                const auto tint=[&](std::size_t channel) {return static_cast<std::uint8_t>(
                    (static_cast<std::uint32_t>(atlas.rgba[at+channel])*palette.header[channel]+127u)/255u);};
                SetPersonaUiPixel(result.image,penX+x,penY+y,tint(0u),tint(1u),tint(2u),atlas.rgba[at+3u]);
            }
        penX+=glyph->width;
    }
    return result;
}
} // namespace QuestVr
