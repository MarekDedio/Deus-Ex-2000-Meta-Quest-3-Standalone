#pragma once

#include "persona_ui_canvas.h"

#include <cmath>
#include <limits>
#include <optional>

namespace QuestVr {

struct HudRgb { std::uint8_t red{}, green{}, blue{}; };
struct HudTheme {
    HudRgb background, border, text, tickMarks;
};
// Original Amber theme values used by the user's PC references. This is an
// explicit presentation choice, not a read of a live ThemeManager instance.
inline constexpr HudTheme kOriginalAmberHudTheme{
    {139u,105u,35u},{185u,177u,140u},{202u,158u,73u},{200u,200u,200u}};
inline constexpr std::uint32_t kHudCompassWidth=73u, kHudCompassHeight=40u;
inline constexpr PersonaUiRect kHudCompassClip{13u,7u,55u,22u};
inline constexpr std::uint32_t kHudCompassTickWidth=240u, kHudCompassTickHeight=15u;
inline constexpr const char* kHudCompassBackground="UserInterface.HUDCompassBackground_1";
inline constexpr const char* kHudCompassBorder="UserInterface.HUDCompassBorder_1";
inline constexpr const char* kHudCompassTicks="UserInterface.HUDCompassTicks";
inline constexpr const char* kHudCompassTickBox="UserInterface.HUDCompassTickBox";
inline constexpr const char* kHudTinyFont="FontTiny";

struct HudCompassArtwork {
    PersonaUiImage background, border, ticks, tickBox;
};
struct HudCompassPositions { std::int32_t draw{}, wrap{}; };
enum class HudPieceBlend : std::uint8_t { Masked, Translucent };

inline PersonaUiImage EmptyHudCanvas(std::uint32_t width,std::uint32_t height) {
    if (width==0u || height==0u || width>2048u || height>2048u)
        throw std::runtime_error("Original HUD canvas exceeds bounded dimensions");
    return {width,height,std::vector<std::uint8_t>(static_cast<std::size_t>(width)*height*4u,0u)};
}
inline void RequireHudSource(const PersonaUiImage& image,std::uint32_t width,std::uint32_t height) {
    ValidatePersonaUiImage(image);
    if (image.width<width || image.height<height)
        throw std::runtime_error("Original HUD logical source window exceeds its texture");
}

// One source texel per logical pixel. The source rectangle is intentional:
// original DrawTexture crops texture padding instead of stretching it. Alpha
// is decoded mask coverage, never the original palette Color.A (often zero).
inline void CopyHudPiece(PersonaUiImage& destination,const PersonaUiImage& source,
    std::int32_t x,std::int32_t y,std::uint32_t width,std::uint32_t height,HudRgb color,
    const PersonaUiRect& clip,std::uint32_t sourceX=0u,std::uint32_t sourceY=0u,
    HudPieceBlend blend=HudPieceBlend::Masked) {
    ValidatePersonaUiImage(destination); ValidatePersonaUiImage(source);
    if (width==0u || height==0u || width>2048u || height>2048u ||
        static_cast<std::uint64_t>(sourceX)+width>source.width ||
        static_cast<std::uint64_t>(sourceY)+height>source.height ||
        static_cast<std::uint64_t>(clip.x)+clip.width>destination.width ||
        static_cast<std::uint64_t>(clip.y)+clip.height>destination.height)
        throw std::runtime_error("Original HUD piece/source/clip exceeds bounds");
    const auto firstX=std::max<std::int64_t>(0,static_cast<std::int64_t>(clip.x)-x);
    const auto firstY=std::max<std::int64_t>(0,static_cast<std::int64_t>(clip.y)-y);
    const auto lastX=std::min<std::int64_t>(width,static_cast<std::int64_t>(clip.x)+clip.width-x);
    const auto lastY=std::min<std::int64_t>(height,static_cast<std::int64_t>(clip.y)+clip.height-y);
    for (auto row=firstY;row<lastY;++row) for (auto column=firstX;column<lastX;++column) {
        const auto pixel=(static_cast<std::size_t>(sourceY+row)*source.width+sourceX+column)*4u;
        if (source.rgba[pixel+3u]==0u) continue;
        auto red=static_cast<std::uint8_t>((source.rgba[pixel]*color.red+127u)/255u);
        auto green=static_cast<std::uint8_t>((source.rgba[pixel+1u]*color.green+127u)/255u);
        auto blue=static_cast<std::uint8_t>((source.rgba[pixel+2u]*color.blue+127u)/255u);
        auto alpha=source.rgba[pixel+3u];
        if (blend==HudPieceBlend::Translucent) {
            // UE PF_Translucent uses ONE, ONE_MINUS_SRC_COLOR (screen), not
            // conventional alpha. Black contributes nothing in THIS style;
            // opaque black remains valid content in Masked draw calls.
            if (red==0u && green==0u && blue==0u) continue;
            const auto target=(static_cast<std::size_t>(y+row)*destination.width+x+column)*4u;
            const auto screen=[&](std::uint8_t sourceValue,std::uint8_t destinationValue) {
                return static_cast<std::uint8_t>(sourceValue+(destinationValue*(255u-sourceValue)+127u)/255u);
            };
            red=screen(red,destination.rgba[target]);green=screen(green,destination.rgba[target+1u]);blue=screen(blue,destination.rgba[target+2u]);
            alpha=std::max(alpha,destination.rgba[target+3u]);
        }
        SetPersonaUiPixel(destination,static_cast<std::uint32_t>(x+column),static_cast<std::uint32_t>(y+row),red,green,blue,alpha);
    }
}

inline HudCompassPositions OriginalHudCompassPositions(std::int32_t playerYaw,std::int32_t mapNorth) {
    // Subtraction is modulo 2^32 before the authored &65535, avoiding host
    // signed overflow. UE FloatToInt truncates toward zero, not floor.
    const auto yaw=(static_cast<std::uint32_t>(playerYaw)-static_cast<std::uint32_t>(mapNorth))&65535u;
    constexpr float unitsPerPixel=65536.0f/static_cast<float>(kHudCompassTickWidth);
    const auto draw=static_cast<std::int32_t>(27.0f-static_cast<float>(yaw)/unitsPerPixel);
    std::int32_t wrap{};
    if (draw>0 && draw<55) wrap=draw-240;
    else if (draw-240<27) wrap=draw+240;
    else wrap=100;
    return {draw,wrap};
}

inline PersonaUiImage BuildHudCompassCanvas(const HudCompassArtwork& art,
    std::int32_t playerYaw,std::int32_t mapNorth,const HudTheme& theme=kOriginalAmberHudTheme) {
    RequireHudSource(art.background,60u,19u); RequireHudSource(art.border,73u,40u);
    RequireHudSource(art.ticks,240u,15u); RequireHudSource(art.tickBox,60u,19u);
    auto result=EmptyHudCanvas(kHudCompassWidth,kHudCompassHeight);
    constexpr PersonaUiRect whole{0u,0u,kHudCompassWidth,kHudCompassHeight};
    CopyHudPiece(result,art.background,11,6,60u,19u,theme.background,whole);
    CopyHudPiece(result,art.tickBox,11,6,60u,19u,theme.background,whole);
    CopyHudPiece(result,art.border,0,0,73u,40u,theme.border,whole);
    const auto positions=OriginalHudCompassPositions(playerYaw,mapNorth);
    CopyHudPiece(result,art.ticks,13+positions.draw,7,240u,15u,theme.tickMarks,kHudCompassClip);
    CopyHudPiece(result,art.ticks,13+positions.wrap,7,240u,15u,theme.tickMarks,kHudCompassClip);
    return result;
}

inline constexpr std::uint32_t kHudBeltWidth=541u, kHudBeltHeight=69u;
inline constexpr std::array<const char*,3> kHudBeltBorders{{
    "UserInterface.HUDObjectBeltBorder_1","UserInterface.HUDObjectBeltBorder_2",
    "UserInterface.HUDObjectBeltBorder_3"}};
inline constexpr const char* kHudBeltBackgroundLeft="UserInterface.HUDObjectBeltBackground_Left";
inline constexpr const char* kHudBeltBackgroundRight="UserInterface.HUDObjectBeltBackground_Right";
inline constexpr const char* kHudBeltBackgroundCell="UserInterface.HUDObjectBeltBackground_Cell";
struct HudBeltArtwork {
    PersonaUiImage backgroundLeft, backgroundRight, cell;
    std::array<PersonaUiImage,3> borders;
    std::array<PersonaUiImage,9> selection;
};
struct HudBeltItem {
    // The caller supplies actual owned actor identity and authoritative belt
    // assignment. This renderer never generates an item, allocates a slot,
    // interprets an inventory-list selection as equipped, or edits a save.
    std::string actorPath;
    const PersonaUiImage* icon{};
    std::string description, itemText;
    bool keyRing{}, selected{}, dragging{};
};
struct HudBeltSnapshot { std::array<std::optional<HudBeltItem>,10> slots; };
inline void ValidateHudBeltSnapshot(const HudBeltSnapshot& snapshot);
template<class PlayerSnapshot,class IconLookup>
inline std::optional<HudBeltSnapshot> BuildBoundPlayerHudBelt(const PlayerSnapshot& player,IconLookup&& lookup) {
    if (!player.available || !player.bound) return std::nullopt;
    const auto validIdentity=[](const std::string& path) {
        return !path.empty() && path.size()<=4096u && path.find('\0')==std::string::npos;
    };
    if (!validIdentity(player.playerPath) || player.inventory.size()>1024u ||
        (!player.keyRingPath.empty() && !validIdentity(player.keyRingPath)))
        throw std::runtime_error("Bound original HUD player/inventory snapshot is invalid");
    HudBeltSnapshot belt;
    std::vector<std::string> identities;
    identities.reserve(player.inventory.size());
    bool foundRing=player.keyRingPath.empty();
    for (const auto& item:player.inventory) {
        if (!validIdentity(item.actorPath) ||
            std::find(identities.begin(),identities.end(),item.actorPath)!=identities.end())
            throw std::runtime_error("Bound original HUD owned Inventory chain has invalid or duplicate identity");
        identities.push_back(item.actorPath);
        const bool ring=item.actorPath==player.keyRingPath && !player.keyRingPath.empty();
        if (!ring && !item.inObjectBelt) continue;
        const auto position=ring?0:item.beltPosition;
        if (position<0 || position>9 || (!ring && position==0) || belt.slots[static_cast<std::size_t>(position)])
            throw std::runtime_error("Actual object-belt assignments overlap or misuse special slot0");
        belt.slots[static_cast<std::size_t>(position)]=HudBeltItem{
            item.actorPath,lookup(item.iconPath),item.beltDescription,"",ring,false,false};
        if (ring) foundRing=true;
    }
    if (!foundRing) throw std::runtime_error("Bound player keyring is absent from its actual owned Inventory chain");
    ValidateHudBeltSnapshot(belt);
    return belt;
}
inline PersonaUiRect OriginalHudBeltSlot(std::size_t position) {
    if (position>=10u) throw std::runtime_error("Original object-belt position exceeds 0..9");
    // UE TileWindow's child order places special slot0 after slots1..9.
    return {position==0u?469u:10u+static_cast<std::uint32_t>(position-1u)*51u,6u,
        position==0u?44u:51u,54u};
}
inline void ValidateHudBeltSnapshot(const HudBeltSnapshot& snapshot) {
    std::vector<std::string> identities;
    identities.reserve(10u);
    for (std::size_t position=0u;position<snapshot.slots.size();++position) {
        const auto& item=snapshot.slots[position];
        if (!item) continue;
        if (item->actorPath.empty() || item->actorPath.size()>4096u || item->actorPath.find('\0')!=std::string::npos ||
            item->description.size()>4096u || item->itemText.size()>4096u || item->keyRing!=(position==0u) ||
            std::find(identities.begin(),identities.end(),item->actorPath)!=identities.end())
            throw std::runtime_error("Object-belt snapshot has invalid identity, special slot or duplicate actor");
        identities.push_back(item->actorPath);
        if (item->icon) RequireHudSource(*item->icon,42u,37u);
    }
}
inline PersonaUiImage BuildHudBeltCanvas(const HudBeltArtwork& art,const HudBeltSnapshot& snapshot,
    const HudTheme& theme=kOriginalAmberHudTheme) {
    ValidateHudBeltSnapshot(snapshot);
    RequireHudSource(art.backgroundLeft,9u,54u);RequireHudSource(art.backgroundRight,8u,54u);
    RequireHudSource(art.cell,51u,54u);
    RequireHudSource(art.borders[0],256u,69u);RequireHudSource(art.borders[1],256u,69u);RequireHudSource(art.borders[2],29u,69u);
    auto result=EmptyHudCanvas(kHudBeltWidth,kHudBeltHeight);
    constexpr PersonaUiRect whole{0u,0u,kHudBeltWidth,kHudBeltHeight};
    for (const auto& item:snapshot.slots) if (item && item->selected)
        ValidatePersonaInventorySelection(result,art.selection,{0u,0u,44u,50u});
    CopyHudPiece(result,art.backgroundLeft,2,6,9u,54u,theme.background,whole);
    CopyHudPiece(result,art.backgroundRight,514,6,8u,54u,theme.background,whole);
    for (std::size_t position=0u;position<10u;++position) {
        const auto slot=OriginalHudBeltSlot(position);
        CopyHudPiece(result,art.cell,static_cast<std::int32_t>(slot.x),6,slot.width,slot.height,theme.background,slot);
    }
    for (std::size_t piece=0u;piece<3u;++piece)
        CopyHudPiece(result,art.borders[piece],static_cast<std::int32_t>(piece*256u),0,piece==2u?29u:256u,69u,theme.border,whole);
    for (std::size_t position=0u;position<10u;++position) {
        const auto& item=snapshot.slots[position];if (!item) continue;
        const auto slot=OriginalHudBeltSlot(position);
        if (item->icon && !item->dragging)
            CopyHudPiece(result,*item->icon,static_cast<std::int32_t>(slot.x+1u),9,42u,37u,{255u,255u,255u},slot);
        if (item->selected && item->icon && !item->dragging)
            DrawPersonaInventorySelection(result,art.selection,{slot.x,slot.y+2u,44u,50u});
    }
    return result;
}

// Original single-line DrawText geometry for FontTiny. This operation clips
// rather than wraps; alpha-zero atlas pixels leave the previous artwork alone.
template<class Font,class Lookup>
inline void DrawHudText(PersonaUiImage& canvas,const Font& font,const std::string& text,
    const PersonaUiRect& rect,HudRgb color,std::uint8_t alignment,Lookup&& lookup) {
    ValidatePersonaUiImage(canvas);
    if (font.lineHeight==0u || font.lineHeight>256u || text.size()>4096u || alignment>2u ||
        static_cast<std::uint64_t>(rect.x)+rect.width>canvas.width ||
        static_cast<std::uint64_t>(rect.y)+rect.height>canvas.height)
        throw std::runtime_error("Original HUD text/font window exceeds bounds");
    std::uint64_t textWidth{};
    for (const unsigned char letter:text) {
        const auto* glyph=lookup(letter);if (!glyph) continue;
        if (glyph->pageIndex>=font.pages.size()) throw std::runtime_error("Original HUD font page exceeds bounds");
        const auto& atlas=font.pages[glyph->pageIndex];ValidatePersonaUiImage(atlas);
        if (static_cast<std::uint64_t>(glyph->x)+glyph->width>atlas.width ||
            static_cast<std::uint64_t>(glyph->y)+glyph->height>atlas.height)
            throw std::runtime_error("Original HUD glyph exceeds atlas bounds");
        textWidth+=glyph->width;
    }
    if (textWidth>1048576u) throw std::runtime_error("Original HUD text advance exceeds bound");
    auto pen=static_cast<std::int64_t>(rect.x);
    if (alignment==1u) pen+=(static_cast<std::int64_t>(rect.width)-static_cast<std::int64_t>(textWidth))/2;
    else if (alignment==2u) pen+=static_cast<std::int64_t>(rect.width)-static_cast<std::int64_t>(textWidth);
    for (const unsigned char letter:text) {
        const auto* glyph=lookup(letter);if (!glyph) continue;
        const auto& atlas=font.pages[glyph->pageIndex];
        const auto firstColumn=std::max<std::int64_t>(0,static_cast<std::int64_t>(rect.x)-pen);
        const auto lastColumn=std::min<std::int64_t>(glyph->width,static_cast<std::int64_t>(rect.x)+rect.width-pen);
        for (std::uint32_t row=0u;row<std::min(glyph->height,rect.height);++row)
            for (auto column=firstColumn;column<lastColumn;++column) {
                const auto x=pen+column;if (x<rect.x || x>=static_cast<std::int64_t>(rect.x)+rect.width) continue;
                const auto pixel=(static_cast<std::size_t>(glyph->y+row)*atlas.width+glyph->x+column)*4u;
                if (atlas.rgba[pixel+3u]==0u) continue;
                SetPersonaUiPixel(canvas,static_cast<std::uint32_t>(x),rect.y+row,
                    static_cast<std::uint8_t>((atlas.rgba[pixel]*color.red+127u)/255u),
                    static_cast<std::uint8_t>((atlas.rgba[pixel+1u]*color.green+127u)/255u),
                    static_cast<std::uint8_t>((atlas.rgba[pixel+2u]*color.blue+127u)/255u),atlas.rgba[pixel+3u]);
            }
        pen+=glyph->width;
    }
}
template<class Font,class Lookup>
inline void DrawHudBeltText(PersonaUiImage& canvas,const HudBeltSnapshot& snapshot,
    const Font& tinyFont,Lookup&& lookup,const HudTheme& theme=kOriginalAmberHudTheme) {
    ValidateHudBeltSnapshot(snapshot);
    if (canvas.width!=kHudBeltWidth || canvas.height!=kHudBeltHeight)
        throw std::runtime_error("Original object-belt text destination has wrong dimensions");
    for (std::size_t position=0u;position<10u;++position) {
        const auto slot=OriginalHudBeltSlot(position);const auto& item=snapshot.slots[position];
        if (item && item->icon && !item->dragging) {
            DrawHudText(canvas,tinyFont,item->description,{slot.x+1u,48u,42u,7u},theme.text,1u,lookup);
            DrawHudText(canvas,tinyFont,item->itemText,{slot.x+1u,38u,42u,8u},theme.text,1u,lookup);
        }
        DrawHudText(canvas,tinyFont,std::to_string(position),{slot.x+37u,9u,6u,7u},theme.text,2u,lookup);
    }
}

inline constexpr std::uint32_t kHudHealthWidth=84u,kHudHealthHeight=106u;
inline constexpr const char* kHudHealthBackground="UserInterface.HUDHitDisplayBackground_1";
inline constexpr const char* kHudHealthBorder="UserInterface.HUDHitDisplayBorder_1";
inline constexpr const char* kHudHealthBody="UserInterface.HUDHitDisplay_Body";
inline constexpr std::array<const char*,6> kHudHealthPartNames{{
    "UserInterface.HUDHitDisplay_Head","UserInterface.HUDHitDisplay_Torso",
    "UserInterface.HUDHitDisplay_ArmLeft","UserInterface.HUDHitDisplay_ArmRight",
    "UserInterface.HUDHitDisplay_LegLeft","UserInterface.HUDHitDisplay_LegRight"}};
inline constexpr std::array<PersonaUiRect,6> kHudHealthPartRects{{
    {39u,17u,4u,7u},{36u,25u,10u,23u},{46u,27u,10u,23u},
    {26u,27u,10u,23u},{41u,44u,8u,36u},{33u,44u,8u,36u}}};
struct HudHealthArtwork { PersonaUiImage background,border,body;std::array<PersonaUiImage,6> parts; };
struct HudBodyPartSnapshot {
    std::optional<std::int32_t> displayedHealth;
    float damageFlashFraction{};
};
struct HudHealthSnapshot {
    // Missing values stay missing: no aggregate-health substitution and no
    // implicit healthy body. Timers/interpolation come from the original HUD
    // state owner; this detached compositor never applies gameplay damage.
    std::array<HudBodyPartSnapshot,6> parts;
    std::optional<float> energy,energyMax,breathPercent;
    bool underwater{};
    float swimTimer{};
};
inline HudRgb OriginalHudScaledColor(float percent) {
    if (!std::isfinite(percent)) throw std::runtime_error("Original HUD scaled color is not finite");
    if (percent>0.8f) return {0u,255u,0u};
    if (percent>0.4f) return {static_cast<std::uint8_t>(255.0f-255.0f*((percent-0.4f)/(0.8f-0.4f))),255u,0u};
    if (percent>0.1f) return {255u,static_cast<std::uint8_t>(255.0f*((percent-0.1f)/(0.4f-0.1f))),0u};
    if (percent>0.0f) return {255u,0u,0u};
    return {};
}
inline HudRgb OriginalHudPartColor(const HudBodyPartSnapshot& part) {
    if (!std::isfinite(part.damageFlashFraction) || part.damageFlashFraction<0.0f || part.damageFlashFraction>1.0f)
        throw std::runtime_error("Original HUD damage flash fraction exceeds bounds");
    if (!part.displayedHealth) return {};
    auto color=OriginalHudScaledColor(static_cast<float>(*part.displayedHealth)/100.0f);
    const auto flash=[&](std::uint8_t value) {return static_cast<std::uint8_t>(value+static_cast<std::uint8_t>((255u-value)*part.damageFlashFraction));};
    return {flash(color.red),flash(color.green),flash(color.blue)};
}
inline void ValidateHudHealthSnapshot(const HudHealthSnapshot& snapshot) {
    for (const auto& part:snapshot.parts) (void)OriginalHudPartColor(part);
    if (snapshot.energy.has_value()!=snapshot.energyMax.has_value() ||
        (snapshot.energy && (!std::isfinite(*snapshot.energy) || *snapshot.energy<0.0f || !std::isfinite(*snapshot.energyMax) || *snapshot.energyMax<=0.0f)) ||
        (snapshot.energy && !std::isfinite(100.0f*(*snapshot.energy / *snapshot.energyMax))) ||
        (snapshot.breathPercent && (!std::isfinite(*snapshot.breathPercent) || *snapshot.breathPercent<0.0f || *snapshot.breathPercent>100.0f)) ||
        !std::isfinite(snapshot.swimTimer))
        throw std::runtime_error("Original HUD energy/breath snapshot is invalid");
}
struct HudProgressBarPresentation { std::uint32_t height{};HudRgb color; };
inline HudProgressBarPresentation OriginalHudProgressBarPresentation(float percent) {
    // SetCurrentValue uses integer Max/Min AFTER FloatToInt, then UpdateBars
    // divides the stored integer-valued Float by100 before multiplying55.
    // Ingress beyond the original Int conversion domain refuses explicitly.
    if(!std::isfinite(percent) || static_cast<double>(percent)<-2147483648.0 ||
        static_cast<double>(percent)>=2147483648.0)
        throw std::runtime_error("Original HUD progress value exceeds its Int conversion domain");
    const auto current=std::clamp(static_cast<std::int32_t>(percent),0,100);
    const float fraction=static_cast<float>(current)/100.0f;
    const auto height=static_cast<std::uint32_t>(fraction*55.0f);
    return {height,OriginalHudScaledColor(fraction)}; // Authored scaleColorModifier1.
}
inline void DrawHudProgressBar(PersonaUiImage& canvas,std::uint32_t x,float percent) {
    ValidatePersonaUiImage(canvas);
    if (canvas.height<75u || x>canvas.width || 5u>canvas.width-x || !std::isfinite(percent))
        throw std::runtime_error("Original HUD progress window/value exceeds bounds");
    const auto [height,color]=OriginalHudProgressBarPresentation(percent);
    for (std::uint32_t row=0u;row<height;++row) for (std::uint32_t column=0u;column<5u;++column)
        SetPersonaUiPixel(canvas,x+column,20u+55u-height+row,color.red,color.green,color.blue);
}
inline PersonaUiImage BuildHudHealthCanvas(const HudHealthArtwork& art,const HudHealthSnapshot& snapshot,
    const HudTheme& theme=kOriginalAmberHudTheme) {
    ValidateHudHealthSnapshot(snapshot);
    RequireHudSource(art.background,60u,76u);RequireHudSource(art.border,84u,106u);RequireHudSource(art.body,34u,68u);
    for (std::size_t part=0u;part<6u;++part) RequireHudSource(art.parts[part],kHudHealthPartRects[part].width,kHudHealthPartRects[part].height);
    auto result=EmptyHudCanvas(kHudHealthWidth,kHudHealthHeight);
    constexpr PersonaUiRect whole{0u,0u,kHudHealthWidth,kHudHealthHeight};
    CopyHudPiece(result,art.background,11,11,60u,76u,theme.background,whole);
    CopyHudPiece(result,art.border,0,0,84u,106u,theme.border,whole);
    // InitWindow explicitly Lowers the neutral body child behind the six
    // independently tinted part children.
    CopyHudPiece(result,art.body,24,15,34u,68u,{255u,255u,255u},whole,0u,0u,HudPieceBlend::Translucent);
    for (std::size_t part=0u;part<6u;++part) if (snapshot.parts[part].displayedHealth) {
        const auto rect=kHudHealthPartRects[part];
        CopyHudPiece(result,art.parts[part],static_cast<std::int32_t>(rect.x),static_cast<std::int32_t>(rect.y),rect.width,rect.height,OriginalHudPartColor(snapshot.parts[part]),whole,0u,0u,HudPieceBlend::Translucent);
    }
    if (snapshot.energy) DrawHudProgressBar(result,15u,100.0f*(*snapshot.energy / *snapshot.energyMax));
    if (snapshot.underwater && snapshot.breathPercent) DrawHudProgressBar(result,61u,*snapshot.breathPercent);
    return result;
}
template<class Font,class Lookup>
inline void DrawHudHealthText(PersonaUiImage& canvas,const HudHealthSnapshot& snapshot,
    const Font& tinyFont,Lookup&& lookup) {
    ValidateHudHealthSnapshot(snapshot);
    if (canvas.width!=kHudHealthWidth || canvas.height!=kHudHealthHeight)
        throw std::runtime_error("Original body-health text destination has wrong dimensions");
    if (snapshot.energy) DrawHudText(canvas,tinyFont,"BE",{13u,74u,8u,8u},
        OriginalHudProgressBarPresentation(100.0f*(*snapshot.energy / *snapshot.energyMax)).color,0u,lookup);
    if (snapshot.underwater && snapshot.breathPercent) {
        auto color=OriginalHudProgressBarPresentation(*snapshot.breathPercent).color;
        if (*snapshot.breathPercent<10.0f) color.red=std::fmod(snapshot.swimTimer,0.5f)>0.25f?255u:0u;
        DrawHudText(canvas,tinyFont,"O2",{61u,74u,8u,8u},color,0u,lookup);
    }
}

} // namespace QuestVr
