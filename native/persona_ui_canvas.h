#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// The Quest upload and desktop preview consume this exact CPU compositor.
// It does not simulate OpenXR, GL blending or controller input.
namespace QuestVr {

struct PersonaUiImage {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> rgba;
};

struct PersonaUiRect {
    std::uint32_t x, y, width, height;
};

// Defaults from the original game's PersonaScreenBaseWindow and
// PersonaScreenInventory, not the power-of-two texture padding dimensions.
inline constexpr std::uint32_t kPersonaCanvasWidth = 640u;
inline constexpr std::uint32_t kPersonaCanvasHeight = 480u;
inline constexpr PersonaUiRect kPersonaClientRect{33u, 43u, 585u, 361u};
inline constexpr PersonaUiRect kPersonaBorderRect{0u, 33u, 640u, 450u};
inline constexpr std::uint32_t kPersonaBackgroundTint = 127u;
inline constexpr std::uint32_t kPersonaBorderTint = 255u;
inline constexpr std::uint32_t kPersonaGridX = 42u;
inline constexpr std::uint32_t kPersonaGridY = 62u;
inline constexpr std::uint32_t kPersonaGridCell = 54u;
inline constexpr std::uint32_t kPersonaGridStep = 53u;
inline constexpr std::uint32_t kPersonaGridColumns = 5u;
inline constexpr std::uint32_t kPersonaGridRows = 6u;
inline constexpr std::size_t kPersonaVisibleItems = 30u;

enum class PersonaUiPage : std::uint8_t { Inventory, Health, GoalsNotes, Logs, Count };
struct PersonaUiLayout {
    const char* backgroundPrefix;
    const char* borderPrefix;
    PersonaUiRect client;
    PersonaUiRect border;
    std::size_t backgroundCount;
    std::size_t backgroundColumns;
};
// Serialized original page defaults. Logs uses ConversationsBorder, not a
// nonexistent LogsBorder; its four client tiles are a two-column arrangement.
inline constexpr std::array<PersonaUiLayout, 4> kPersonaPageLayouts{{
    {"InventoryBackground_", "InventoryBorder_", kPersonaClientRect, kPersonaBorderRect, 6u, 3u},
    {"HealthBackground_", "HealthBorder_", {25u,37u,596u,427u}, {0u,32u,640u,450u}, 6u, 3u},
    {"GoalsBackground_", "GoalsBorder_", {15u,39u,604u,433u}, {0u,29u,640u,450u}, 6u, 3u},
    {"LogsBackground_", "ConversationsBorder_", {105u,47u,426u,407u}, {0u,30u,640u,450u}, 4u, 2u}
}};
inline const PersonaUiLayout& GetPersonaUiLayout(PersonaUiPage page) {
    const auto index = static_cast<std::size_t>(page);
    if (index >= kPersonaPageLayouts.size()) throw std::runtime_error("Invalid Persona page");
    return kPersonaPageLayouts[index];
}

template<class Image>
inline void ValidatePersonaUiImage(const Image& image) {
    if (image.width == 0u || image.height == 0u || image.width > 8192u ||
        image.height > 8192u || image.rgba.size() !=
            static_cast<std::size_t>(image.width) * image.height * 4u) {
        throw std::runtime_error("Persona texture dimensions or pixel count are invalid");
    }
}

// Decode callers must mask UE1 palette index zero. RGB black is not a mask:
// an opaque black texel can be legitimate content. Alpha-zero pixels preserve
// the preceding layer, matching the original masked child windows.
template<class Image>
inline void CopyPersonaUiPiece(
    PersonaUiImage& canvas, const Image& piece,
    std::uint32_t tileX, std::uint32_t tileY,
    const PersonaUiRect& window, std::uint32_t tint) {
    ValidatePersonaUiImage(canvas);
    ValidatePersonaUiImage(piece);
    if (tint > 255u) throw std::runtime_error("Persona tint exceeds one byte");
    for (std::uint32_t row = 0u; row < piece.height; ++row) {
        const std::uint64_t y = static_cast<std::uint64_t>(tileY) + row;
        if (y >= window.height || window.y + y >= canvas.height) break;
        for (std::uint32_t column = 0u; column < piece.width; ++column) {
            const std::uint64_t x = static_cast<std::uint64_t>(tileX) + column;
            if (x >= window.width || window.x + x >= canvas.width) break;
            const std::size_t source =
                (static_cast<std::size_t>(row) * piece.width + column) * 4u;
            if (piece.rgba[source + 3u] == 0u) continue;
            const std::size_t destination =
                (static_cast<std::size_t>(window.y + y) * canvas.width + window.x + x) * 4u;
            for (std::size_t channel = 0u; channel < 3u; ++channel) {
                canvas.rgba[destination + channel] = static_cast<std::uint8_t>(
                    (piece.rgba[source + channel] * tint + 127u) / 255u);
            }
            canvas.rgba[destination + 3u] = piece.rgba[source + 3u];
        }
    }
}

template<class Image>
inline PersonaUiImage BuildPersonaUiCanvas(
    const std::array<Image, 6>& backgrounds,
    const std::array<Image, 6>& borders,
    const PersonaUiLayout& layout = kPersonaPageLayouts[0]) {
    PersonaUiImage canvas{kPersonaCanvasWidth, kPersonaCanvasHeight,
        std::vector<std::uint8_t>(
            static_cast<std::size_t>(kPersonaCanvasWidth) * kPersonaCanvasHeight * 4u, 0u)};
    const auto drawTiles = [&](const auto& pieces, const PersonaUiRect& window,
                               const std::uint32_t tint, const std::size_t count,
                               const std::size_t columns) {
        if (count == 0u || count > pieces.size() || columns == 0u || count % columns != 0u)
            throw std::runtime_error("Invalid Persona tile arrangement");
        std::uint32_t topHeight{};
        for (std::size_t index = 0u; index < count; ++index) {
            ValidatePersonaUiImage(pieces[index]);
            if (index < columns) topHeight = std::max(topHeight, pieces[index].height);
        }
        for (std::size_t row = 0u; row < count / columns; ++row) {
            std::uint32_t x{};
            const std::uint32_t y = row == 0u ? 0u : topHeight;
            for (std::size_t column = 0u; column < columns; ++column) {
                const auto& piece = pieces[row * columns + column];
                CopyPersonaUiPiece(canvas, piece, x, y, window, tint);
                x += piece.width;
            }
        }
    };
    drawTiles(backgrounds, layout.client, kPersonaBackgroundTint,
              layout.backgroundCount, layout.backgroundColumns);
    drawTiles(borders, layout.border, kPersonaBorderTint, 6u, 3u);
    return canvas;
}

template<class Image>
inline void AddPersonaHealthBody(PersonaUiImage& canvas,
    const std::array<Image, 2>& body, const std::array<Image, 2>& overlays) {
    // Original Health windows: client (25,37) plus (24,36), size 219x357.
    // Overlays are created first, then neutral masked body; no invented limb
    // health colors. The live runtime currently exposes only aggregate health.
    constexpr PersonaUiRect window{49u,73u,219u,357u};
    CopyPersonaUiPiece(canvas, overlays[0], 0u, 0u, window, kPersonaBackgroundTint);
    CopyPersonaUiPiece(canvas, overlays[1], 0u, 256u, window, kPersonaBackgroundTint);
    CopyPersonaUiPiece(canvas, body[0], 0u, 0u, window, 255u);
    CopyPersonaUiPiece(canvas, body[1], 0u, 256u, window, 255u);
}

inline std::size_t PersonaFirstVisibleItem(
    std::size_t itemCount, std::size_t selectedIndex) {
    if (itemCount <= kPersonaVisibleItems) return 0u;
    return std::min(selectedIndex > kPersonaVisibleItems / 2u
        ? selectedIndex - kPersonaVisibleItems / 2u : 0u, itemCount - kPersonaVisibleItems);
}

inline void SetPersonaUiPixel(
    PersonaUiImage& canvas, std::uint32_t x, std::uint32_t y,
    std::uint8_t red, std::uint8_t green, std::uint8_t blue,
    std::uint8_t alpha = 255u) {
    if (x >= canvas.width || y >= canvas.height) return;
    const std::size_t pixel = (static_cast<std::size_t>(y) * canvas.width + x) * 4u;
    canvas.rgba[pixel] = red;
    canvas.rgba[pixel + 1u] = green;
    canvas.rgba[pixel + 2u] = blue;
    canvas.rgba[pixel + 3u] = alpha;
}

template<class Image>
inline void CopyPersonaInventoryIcon(
    PersonaUiImage& canvas, const Image& icon, std::size_t visibleSlot) {
    ValidatePersonaUiImage(canvas);
    ValidatePersonaUiImage(icon);
    if (visibleSlot >= kPersonaVisibleItems)
        throw std::runtime_error("Persona icon slot is outside visible inventory");
    constexpr std::uint32_t iconLimit = 46u;
    std::uint32_t iconWidth = iconLimit;
    std::uint32_t iconHeight = iconLimit;
    if (icon.width > icon.height) {
        iconHeight = std::max(1u, iconLimit * icon.height / icon.width);
    } else {
        iconWidth = std::max(1u, iconLimit * icon.width / icon.height);
    }
    const std::uint32_t x = kPersonaGridX +
        static_cast<std::uint32_t>(visibleSlot % kPersonaGridColumns) * kPersonaGridStep;
    const std::uint32_t y = kPersonaGridY +
        static_cast<std::uint32_t>(visibleSlot / kPersonaGridColumns) * kPersonaGridStep;
    const std::uint32_t iconX = x + (kPersonaGridCell - iconWidth) / 2u;
    const std::uint32_t iconY = y + (kPersonaGridCell - iconHeight) / 2u;
    for (std::uint32_t row = 0u; row < iconHeight; ++row) {
        const std::uint32_t sourceY = row * icon.height / iconHeight;
        for (std::uint32_t column = 0u; column < iconWidth; ++column) {
            const std::uint32_t sourceX = column * icon.width / iconWidth;
            const std::size_t source =
                (static_cast<std::size_t>(sourceY) * icon.width + sourceX) * 4u;
            if (icon.rgba[source + 3u] == 0u) continue;
            SetPersonaUiPixel(canvas, iconX + column, iconY + row,
                icon.rgba[source], icon.rgba[source + 1u],
                icon.rgba[source + 2u], icon.rgba[source + 3u]);
        }
    }
}

template<class IconLookup>
inline void DrawPersonaInventoryGrid(
    PersonaUiImage& canvas, std::size_t itemCount,
    std::size_t selectedIndex, IconLookup&& lookupIcon) {
    ValidatePersonaUiImage(canvas);
    const std::size_t first = PersonaFirstVisibleItem(itemCount, selectedIndex);
    for (std::size_t slot = 0u; slot < kPersonaVisibleItems; ++slot) {
        const std::uint32_t x = kPersonaGridX +
            static_cast<std::uint32_t>(slot % kPersonaGridColumns) * kPersonaGridStep;
        const std::uint32_t y = kPersonaGridY +
            static_cast<std::uint32_t>(slot / kPersonaGridColumns) * kPersonaGridStep;
        const std::size_t inventoryIndex = first + slot;
        const bool selected = inventoryIndex < itemCount && inventoryIndex == selectedIndex;
        for (std::uint32_t row = 0u; row < kPersonaGridCell; ++row) {
            for (std::uint32_t column = 0u; column < kPersonaGridCell; ++column) {
                const bool edge = row < 2u || column < 2u ||
                    row + 2u >= kPersonaGridCell || column + 2u >= kPersonaGridCell;
                const std::uint8_t shade = edge ? (selected ? 255u : 100u) : 18u;
                SetPersonaUiPixel(canvas, x + column, y + row, shade, shade, shade);
            }
        }
        if (inventoryIndex >= itemCount) continue;
        const auto* icon = lookupIcon(inventoryIndex);
        if (icon == nullptr || icon->width == 0u || icon->height == 0u) continue;
        CopyPersonaInventoryIcon(canvas, *icon, slot);
    }
}

struct PersonaUiChrome {
    std::array<PersonaUiImage, 3> navigationBackgrounds;
    std::array<PersonaUiImage, 3> navigationBorders;
    std::array<PersonaUiImage, 3> normalButton;
    PersonaUiImage filler;
};

inline void AddPersonaNavigationArtwork(PersonaUiImage& canvas, const PersonaUiChrome& chrome) {
    for (std::size_t index = 0u; index < 3u; ++index) {
        CopyPersonaUiPiece(canvas, chrome.navigationBackgrounds[index],
            static_cast<std::uint32_t>(index) * 256u, 0u, {17u,6u,609u,21u}, 127u);
        CopyPersonaUiPiece(canvas, chrome.navigationBorders[index],
            static_cast<std::uint32_t>(index) * 256u, 0u, {0u,0u,640u,64u}, 255u);
    }
}

inline void DrawPersonaButtonArtwork(PersonaUiImage& canvas, const PersonaUiChrome& chrome,
                                     const PersonaUiRect& rect) {
    ValidatePersonaUiImage(canvas);
    for (const auto& piece : chrome.normalButton) ValidatePersonaUiImage(piece);
    ValidatePersonaUiImage(chrome.filler);
    if (chrome.normalButton[0].width != 4u || chrome.normalButton[1].width != 2u ||
        chrome.normalButton[2].width != 8u || chrome.normalButton[0].height != 16u ||
        chrome.normalButton[1].height != 16u || chrome.normalButton[2].height != 16u ||
        chrome.filler.width != 2u || chrome.filler.height != 2u ||
        rect.width < 12u || rect.height != 16u || rect.width > 640u)
        throw std::runtime_error("Original Persona button dimensions are invalid");
    if (static_cast<std::uint64_t>(rect.x) + rect.width > canvas.width ||
        static_cast<std::uint64_t>(rect.y) + rect.height > canvas.height)
        throw std::runtime_error("Persona button window exceeds canvas");
    for (std::uint32_t y = 0u; y < rect.height; y += 2u)
        for (std::uint32_t x = 0u; x < rect.width; x += 2u)
            CopyPersonaUiPiece(canvas, chrome.filler, x, y, rect, 127u);
    CopyPersonaUiPiece(canvas, chrome.normalButton[0], 0u, 0u, rect, 127u);
    for (std::uint32_t x = 4u; x < rect.width - 8u; x += 2u)
        CopyPersonaUiPiece(canvas, chrome.normalButton[1], x, 0u, rect, 127u);
    CopyPersonaUiPiece(canvas, chrome.normalButton[2], rect.width - 8u, 0u, rect, 127u);
}

template<class Font, class GlyphLookup>
inline void DrawPersonaUiText(PersonaUiImage& canvas, const Font& font,
    const std::string& text, const PersonaUiRect& rect, std::uint32_t tint,
    GlyphLookup&& lookupGlyph) {
    ValidatePersonaUiImage(canvas);
    if (font.lineHeight == 0u || font.lineHeight > 256u || tint > 255u)
        throw std::runtime_error("Original Persona font height or tint is invalid");
    if (static_cast<std::uint64_t>(rect.x) + rect.width > canvas.width ||
        static_cast<std::uint64_t>(rect.y) + rect.height > canvas.height)
        throw std::runtime_error("Persona text window exceeds canvas");
    std::uint32_t penX{}, penY{};
    for (std::size_t index = 0u; index < text.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(text[index]);
        if (character == '\r') continue;
        if (character == '\n') { penX = 0u; penY += font.lineHeight; continue; }
        if (penY >= rect.height) break;
        // Match original single-byte UE1 font indexing. Text is clipped within
        // the real child window; no proportional-font spacing approximations.
        const auto* glyph = lookupGlyph(character == '\t' ? ' ' : character);
        if (glyph == nullptr) continue;
        if (glyph->pageIndex >= font.pages.size())
            throw std::runtime_error("Persona font page is out of range");
        const auto& atlas = font.pages[glyph->pageIndex];
        ValidatePersonaUiImage(atlas);
        if (static_cast<std::uint64_t>(glyph->x) + glyph->width > atlas.width ||
            static_cast<std::uint64_t>(glyph->y) + glyph->height > atlas.height)
            throw std::runtime_error("Persona glyph exceeds original atlas");
        // Wrap at the next word when possible, then hard-wrap oversized words.
        if (character != ' ' && (index == 0u || text[index - 1u] == ' ')) {
            std::uint64_t wordWidth{};
            for (std::size_t word = index; word < text.size() && text[word] != ' ' &&
                 text[word] != '\n' && text[word] != '\r'; ++word) {
                const auto* next = lookupGlyph(static_cast<unsigned char>(text[word]));
                if (next != nullptr) wordWidth += next->width;
            }
            if (penX > 0u && penX + wordWidth > rect.width) {
                penX = 0u; penY += font.lineHeight;
            }
        }
        if (penX > 0u && static_cast<std::uint64_t>(penX) + glyph->width > rect.width) {
            penX = 0u; penY += font.lineHeight;
            if (character == ' ') continue;
        }
        if (penY >= rect.height) break;
        for (std::uint32_t y = 0u; y < glyph->height && penY + y < rect.height; ++y) {
            for (std::uint32_t x = 0u; x < glyph->width && penX + x < rect.width; ++x) {
                const auto source =
                    (static_cast<std::size_t>(glyph->y + y) * atlas.width + glyph->x + x) * 4u;
                if (atlas.rgba[source + 3u] == 0u) continue;
                SetPersonaUiPixel(canvas, rect.x + penX + x, rect.y + penY + y,
                    static_cast<std::uint8_t>((atlas.rgba[source] * tint + 127u) / 255u),
                    static_cast<std::uint8_t>((atlas.rgba[source + 1u] * tint + 127u) / 255u),
                    static_cast<std::uint8_t>((atlas.rgba[source + 2u] * tint + 127u) / 255u),
                    atlas.rgba[source + 3u]);
            }
        }
        penX += glyph->width;
    }
}

template<class Font, class GlyphLookup>
inline std::uint32_t PersonaUiTextWidth(const Font&, const std::string& text,
                                      GlyphLookup&& lookupGlyph) {
    std::uint64_t width{};
    for (const unsigned char character : text) {
        const auto* glyph = lookupGlyph(character);
        if (glyph != nullptr) width += glyph->width;
    }
    if (width > 640u) throw std::runtime_error("Persona button caption is too wide");
    return static_cast<std::uint32_t>(width);
}

template<class Font, class GlyphLookup>
inline void DrawPersonaNavigation(PersonaUiImage& canvas, const PersonaUiChrome& chrome,
    const Font& font, PersonaUiPage page, GlyphLookup&& lookupGlyph) {
    AddPersonaNavigationArtwork(canvas, chrome);
    constexpr std::array<const char*, 8> captions{{
        "Inventory", "Health", "Augs", "Skills", "Goals/Notes", "Conversations", "Images", "Logs"}};
    constexpr std::array<int, 8> pageIndices{{0,1,-1,-1,2,-1,-1,3}};
    std::array<std::uint32_t, 8> widths{};
    std::uint32_t total{};
    for (std::size_t index = 0u; index < widths.size(); ++index) {
        widths[index] = std::max(20u, PersonaUiTextWidth(font, captions[index], lookupGlyph) + 18u);
        total += widths[index];
    }
    if (total > 534u) throw std::runtime_error("Original Persona tabs exceed navigation window");
    const std::uint32_t padding = (534u - total) / 8u;
    std::uint32_t x = 23u;
    for (std::size_t index = 0u; index < widths.size(); ++index) {
        widths[index] += padding;
        DrawPersonaButtonArtwork(canvas, chrome, {x,8u,widths[index],16u});
        const std::uint32_t tint = pageIndices[index] < 0 ? 64u :
            (pageIndices[index] == static_cast<int>(page) ? 255u : 200u);
        DrawPersonaUiText(canvas, font, captions[index], {x+10u,11u,widths[index]-18u,10u},
                          tint, lookupGlyph);
        x += widths[index];
    }
    DrawPersonaButtonArtwork(canvas, chrome, {573u,8u,48u,16u});
    DrawPersonaUiText(canvas, font, "Exit", {583u,11u,30u,10u}, 200u, lookupGlyph);
}

template<class Font, class GlyphLookup>
inline void DrawPersonaVrActions(PersonaUiImage& canvas, const PersonaUiChrome& chrome,
    const Font& font, PersonaUiPage page, GlyphLookup&& lookupGlyph) {
    // VR bindings on original action-button artwork. Unsupported original
    // actions are not presented as working controls.
    constexpr std::array<PersonaUiRect,4> bars{{
        {42u,382u,267u,16u}, {38u,444u,92u,16u},
        {25u,450u,179u,16u}, {115u,432u,75u,16u}}};
    const auto index = static_cast<std::size_t>(page);
    if (index >= bars.size()) throw std::runtime_error("Invalid Persona action page");
    const auto& bar = bars[index];
    const std::vector<std::string> captions = page == PersonaUiPage::Inventory
        ? std::vector<std::string>{"A Use", "Y Save", "X Load", "B Close"}
        : (page == PersonaUiPage::GoalsNotes
            ? std::vector<std::string>{"Save", "Load", "Close"}
            : std::vector<std::string>{"B Close"});
    const std::uint32_t width = bar.width / static_cast<std::uint32_t>(captions.size());
    for (std::size_t button = 0u; button < captions.size(); ++button) {
        if (PersonaUiTextWidth(font, captions[button], lookupGlyph) > width - 18u)
            throw std::runtime_error("Persona action caption exceeds button text window");
        const auto x = bar.x + static_cast<std::uint32_t>(button) * width;
        DrawPersonaButtonArtwork(canvas, chrome, {x,bar.y,width,16u});
        DrawPersonaUiText(canvas, font, captions[button], {x+10u,bar.y+3u,width-18u,10u},
                          200u, lookupGlyph);
    }
}

template<class HeaderFont, class BodyFont, class HeaderLookup, class BodyLookup>
inline void DrawPersonaPageText(PersonaUiImage& canvas,
    const HeaderFont& headerFont, const BodyFont& bodyFont, PersonaUiPage page,
    const std::string& left, const std::string& right,
    HeaderLookup&& lookupHeader, BodyLookup&& lookupBody) {
    const auto heading = [&](const std::string& value, const PersonaUiRect& rect) {
        DrawPersonaUiText(canvas, headerFont, value, rect, 255u, lookupHeader);
    };
    const auto body = [&](const std::string& value, const PersonaUiRect& rect) {
        DrawPersonaUiText(canvas, bodyFont, value, rect, 200u, lookupBody);
    };
    switch (page) {
        case PersonaUiPage::Inventory:
            heading(left, {42u,48u,266u,12u});
            body(right, {370u,60u,238u,218u});
            body("UP / DOWN: SELECT\nLEFT / RIGHT: PAGE\nA: EQUIP / USE\nY: SAVE   X: LOAD", {370u,338u,238u,40u});
            break;
        case PersonaUiPage::Health:
            heading(left, {34u,42u,266u,12u});
            body(right, {373u,59u,238u,239u});
            body("LEFT / RIGHT: PAGE\nY: SAVE   X: LOAD\nB / MENU: CLOSE", {373u,347u,238u,60u});
            break;
        case PersonaUiPage::GoalsNotes:
            heading("Goals", {24u,44u,574u,12u});
            heading("Notes", {24u,248u,574u,12u});
            body(left, {31u,60u,574u,154u});
            body(right, {31u,265u,574u,182u});
            body("Y: SAVE  X: LOAD  LEFT / RIGHT: PAGE", {225u,453u,384u,12u});
            break;
        case PersonaUiPage::Logs:
            heading("Logs", {114u,52u,394u,12u});
            body(left, {121u,68u,394u,361u});
            body("LEFT / RIGHT: PAGE\nY: SAVE   X: LOAD", {205u,435u,317u,30u});
            break;
        case PersonaUiPage::Count:
            throw std::runtime_error("Invalid Persona text page");
    }
}

} // namespace QuestVr
