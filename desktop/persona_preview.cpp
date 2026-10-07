#include "persona_preview.h"

#include "persona_ui_canvas.h"
#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string("Persona compositor regression: ")+message);
}
template<class Action>
void RequireRejected(Action&& action, const char* message) {
    bool rejected{};
    try { action(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected,message);
}
PortableTextureImage DecodeArtwork(const PortablePackageTables& package,
                                  const std::string& group, const std::string& name,
                                  std::string& actualPath) {
    actualPath = group+"."+name;
    try { (void)FindPortableTextureExport(package,actualPath); }
    catch (const std::runtime_error& error) {
        if (std::string(error.what()).rfind("UE1 texture export was not found:",0) != 0u) throw;
        actualPath = name;
        (void)FindPortableTextureExport(package,actualPath);
    }
    return DecodePortableIndexedTexture(package,actualPath,true);
}
std::uint64_t Hash(const std::vector<std::uint8_t>& pixels) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto byte : pixels) { hash ^= byte; hash *= 1099511628211ull; }
    return hash;
}
QuestVr::PersonaUiImage Solid(std::uint32_t width, std::uint32_t height,
                              std::array<std::uint8_t,4> color) {
    QuestVr::PersonaUiImage image{width,height,
        std::vector<std::uint8_t>(static_cast<std::size_t>(width)*height*4u)};
    for (std::size_t offset = 0; offset < image.rgba.size(); offset += 4u)
        std::copy(color.begin(),color.end(),image.rgba.begin()+offset);
    return image;
}
QuestVr::PersonaUiImage UiImage(PortableTextureImage image) {
    return {image.width,image.height,std::move(image.rgba)};
}
DesktopPersonaFontProvenance FontProvenance(const PortableBitmapFont& font) {
    DesktopPersonaFontProvenance result;
    result.objectPath = font.objectPath;
    result.atlasPaths = font.texturePaths;
    result.glyphCount = font.glyphs.size();
    result.charactersPerPage = font.charactersPerPage;
    result.lineHeight = font.lineHeight;
    for (const auto& atlas : font.pages)
        result.atlasDimensions.push_back({atlas.width,atlas.height});
    return result;
}
} // namespace

DesktopPersonaPreview BuildDesktopPersonaPreview(
    const std::filesystem::path& uiPackage,
    const std::vector<std::string>& requestedIcons,
    std::size_t selectedIndex,
    QuestVr::PersonaUiPage page) {
    const auto package = LoadPortablePackageTables(uiPackage.string());
    std::array<PortableTextureImage,6> backgrounds, borders;
    const auto& layout = QuestVr::GetPersonaUiLayout(page);
    DesktopPersonaPreview result;
    for (std::size_t i = 0; i < layout.backgroundCount; ++i) {
        std::string path;
        backgrounds[i] = DecodeArtwork(package,"UserInterface",
            std::string(layout.backgroundPrefix)+std::to_string(i+1u),path);
        result.artworkPaths.push_back(path);
    }
    for (std::size_t i = 0; i < borders.size(); ++i) {
        std::string path;
        borders[i] = DecodeArtwork(package,"UserInterface",
            std::string(layout.borderPrefix)+std::to_string(i+1u),path);
        result.artworkPaths.push_back(path);
    }
    auto canvas = QuestVr::BuildPersonaUiCanvas(backgrounds,borders,layout);
    QuestVr::PersonaUiChrome chrome;
    const auto chromeImage = [&](const std::string& name) {
        std::string path;
        auto image = DecodeArtwork(package,"UserInterface",name,path);
        result.artworkPaths.push_back(path);
        return UiImage(std::move(image));
    };
    for (std::size_t index = 0; index < chrome.navigationBackgrounds.size(); ++index) {
        chrome.navigationBackgrounds[index] = chromeImage("PersonaNavBarBackground_"+std::to_string(index+1u));
        chrome.navigationBorders[index] = chromeImage("PersonaNavBarBorder_"+std::to_string(index+1u));
    }
    constexpr std::array<const char*,3> buttonNames{{
        "PersonaActionButtonNormal_Left","PersonaActionButtonNormal_Center","PersonaActionButtonNormal_Right"}};
    for (std::size_t index = 0; index < buttonNames.size(); ++index)
        chrome.normalButton[index] = chromeImage(buttonNames[index]);
    chrome.filler = chromeImage("PersonaButtonFiller");
    const auto headers = DecodePortableBitmapFont(package,"FontMenuHeaders");
    const auto bodyFont = DecodePortableBitmapFont(package,"FontMenuSmall");
    result.fonts = {{FontProvenance(headers),FontProvenance(bodyFont)}};
    if (page == QuestVr::PersonaUiPage::Health) {
        std::array<PortableTextureImage,2> body, overlays;
        for (std::size_t i = 0; i < body.size(); ++i) {
            std::string path;
            body[i] = DecodeArtwork(package,"UserInterface","HealthBody_"+std::to_string(i+1u),path);
            result.artworkPaths.push_back(path);
            overlays[i] = DecodeArtwork(package,"UserInterface","HealthOverlays_"+std::to_string(i+1u),path);
            result.artworkPaths.push_back(path);
        }
        QuestVr::AddPersonaHealthBody(canvas,body,overlays);
    }
    std::vector<PortableTextureImage> icons;
    icons.reserve(requestedIcons.size());
    for (const auto& name : requestedIcons) {
        std::string path;
        const auto dot = name.find('.');
        if (dot == std::string::npos) icons.push_back(DecodeArtwork(package,"Icons",name,path));
        else {
            path = name;
            icons.push_back(DecodePortableIndexedTexture(package,name,true));
        }
        result.iconPaths.push_back(path);
    }
    if (page == QuestVr::PersonaUiPage::Inventory) {
        QuestVr::DrawPersonaInventoryGrid(canvas,icons.size(),selectedIndex,
            [&](std::size_t index) -> const PortableTextureImage* {
                return index < icons.size() ? &icons[index] : nullptr;
            });
    } else if (!requestedIcons.empty()) {
        throw std::runtime_error("Icon fixtures apply only to Inventory previews");
    }
    // Deliberately explicit sample strings: this is the same CPU text composer
    // as Quest, but it does not substitute for live inventory/mission evidence.
    switch (page) {
        case QuestVr::PersonaUiPage::Inventory:
            result.fixtureLeftText = "Inventory (asset fixture)";
            result.fixtureRightText = "PISTOL\n\n10mm ammunition\n\nA compact semi-automatic handgun.\n\nThis item description is preview text, not a saved inventory.";
            break;
        case QuestVr::PersonaUiPage::Health:
            result.fixtureLeftText = "Health";
            result.fixtureRightText = "JC DENTON\n\nHealth: 100 / 100\n\nEnergy: 100 / 100\n\nNeutral body artwork shown. Limb damage is not simulated by this preview.";
            break;
        case QuestVr::PersonaUiPage::GoalsNotes:
            result.fixtureLeftText = "Primary objective\n\nProceed through the training course.\n\nFixture text for checking original glyphs and wrapping. This is not a live campaign objective.";
            result.fixtureRightText = "Training notes\n\nExplore the environment and use your equipment.\n\nThe original bitmap fonts and panel artwork are rendered by the shared Quest CPU composer.";
            break;
        case QuestVr::PersonaUiPage::Logs:
            result.fixtureLeftText = "TRAINING LOG\n\nUNATCO transmission\n\nWelcome to the training facility.\n\nThis conversation is illustrative fixture text.\n\nIt verifies original glyph positions, line spacing and window clipping, not in-game conversation execution.";
            break;
        case QuestVr::PersonaUiPage::Count:
            throw std::runtime_error("Invalid Persona fixture page");
    }
    const auto headerGlyph = [&](std::uint32_t code) { return GetPortableBitmapGlyph(headers,code); };
    const auto bodyGlyph = [&](std::uint32_t code) { return GetPortableBitmapGlyph(bodyFont,code); };
    QuestVr::DrawPersonaNavigation(canvas,chrome,headers,page,headerGlyph);
    QuestVr::DrawPersonaVrActions(canvas,chrome,headers,page,headerGlyph);
    QuestVr::DrawPersonaPageText(canvas,headers,bodyFont,page,
        result.fixtureLeftText,result.fixtureRightText,headerGlyph,bodyGlyph);
    result.image = {canvas.width,canvas.height,
        std::vector<std::uint8_t>(static_cast<std::size_t>(canvas.width)*canvas.height*3u)};
    result.visibleMinX = result.visibleMinY = std::numeric_limits<std::uint32_t>::max();
    for (std::uint32_t y = 0; y < canvas.height; ++y) {
        for (std::uint32_t x = 0; x < canvas.width; ++x) {
            const auto pixel = static_cast<std::size_t>(y)*canvas.width+x;
            const auto alpha = canvas.rgba[pixel*4u+3u];
            if (alpha == 0u) ++result.transparentPixels;
            else {
                if (alpha == 255u) ++result.opaquePixels; else ++result.partialAlphaPixels;
                result.visibleMinX = std::min(result.visibleMinX,x);
                result.visibleMinY = std::min(result.visibleMinY,y);
                result.visibleMaxX = std::max(result.visibleMaxX,x);
                result.visibleMaxY = std::max(result.visibleMaxY,y);
            }
            const std::uint32_t checker = ((x/16u+y/16u)%2u) != 0 ? 92u : 58u;
            for (std::size_t channel = 0; channel < 3u; ++channel) {
                result.image.rgb[pixel*3u+channel] = static_cast<std::uint8_t>(
                    (canvas.rgba[pixel*4u+channel]*alpha+checker*(255u-alpha)+127u)/255u);
            }
        }
    }
    if (result.transparentPixels == static_cast<std::size_t>(canvas.width)*canvas.height)
        throw std::runtime_error("Original Persona artwork produced an empty canvas");
    result.rgbaHash = Hash(canvas.rgba);
    result.flattenedHash = Hash(result.image.rgb);
    return result;
}

const char* DesktopPersonaPageName(QuestVr::PersonaUiPage page) {
    static constexpr std::array<const char*,4> names{"Inventory","Health","GoalsNotes","Logs"};
    const auto index = static_cast<std::size_t>(page);
    if (index >= names.size()) throw std::runtime_error("Invalid desktop Persona page");
    return names[index];
}

void VerifySharedPersonaCanvas() {
    std::array<QuestVr::PersonaUiImage,6> backgrounds, borders;
    for (auto& image : backgrounds) image = Solid(256,256,{255u,128u,0u,255u});
    for (auto& image : borders) image = Solid(256,256,{0u,0u,0u,0u});
    auto canvas = QuestVr::BuildPersonaUiCanvas(backgrounds,borders);
    Require(canvas.width == 640u && canvas.height == 480u,"canvas is not original 4:3 size");
    const auto at = [&](std::uint32_t x,std::uint32_t y,std::size_t channel) {
        return canvas.rgba[(static_cast<std::size_t>(y)*canvas.width+x)*4u+channel];
    };
    Require(at(33,43,0) == 127u && at(33,43,1) == 64u,"background gray tint changed");
    Require(at(32,43,3) == 0u && at(33,42,3) == 0u,"background escaped client origin");
    Require(at(617,403,3) == 255u && at(618,403,3) == 0u && at(617,404,3) == 0u,
            "background escaped original 585x361 client clip");
    Require(at(639,479,3) == 0u,"padding lost transparency");
    backgrounds[0].rgba[4u+3u] = 0u;
    backgrounds[0].rgba[8u] = backgrounds[0].rgba[9u] = backgrounds[0].rgba[10u] = 0u;
    canvas = QuestVr::BuildPersonaUiCanvas(backgrounds,borders);
    Require(at(34,43,3) == 0u,"alpha-zero texel was painted opaque");
    Require(at(35,43,0) == 0u && at(35,43,3) == 255u,"opaque black was incorrectly color-keyed");
    borders[0].rgba[0] = 220u; borders[0].rgba[1] = 140u;
    borders[0].rgba[2] = 75u; borders[0].rgba[3] = 255u;
    canvas = QuestVr::BuildPersonaUiCanvas(backgrounds,borders);
    Require(at(0,33,0) == 220u && at(0,33,1) == 140u && at(0,33,2) == 75u,
            "white border tint altered source colors");
    auto invalid = backgrounds;
    invalid[0].rgba.pop_back();
    bool invalidRejected{};
    try { (void)QuestVr::BuildPersonaUiCanvas(invalid,borders); }
    catch (const std::runtime_error&) { invalidRejected = true; }
    Require(invalidRejected,"malformed source pixel count accepted");
    Require(QuestVr::PersonaFirstVisibleItem(40u,39u) == 10u,"inventory window scrolling changed");
    const auto icon = Solid(64,32,{255u,20u,10u,255u});
    QuestVr::DrawPersonaInventoryGrid(canvas,1u,0u,
        [&](std::size_t index) -> const QuestVr::PersonaUiImage* { return index == 0u ? &icon : nullptr; });
    Require(at(42,62,0) == 255u && at(95,62,0) == 100u,
            "selected/unselected inventory grid edges changed");
    Require(at(46,89,0) == 255u && at(46,89,1) == 20u,
            "original icon colors/aspect positioning changed");
    Require(at(46,66,0) == 18u,"wide icon stretched instead of preserving aspect ratio");

    for (const auto& layout : QuestVr::kPersonaPageLayouts) {
        for (auto& image : backgrounds) image = {};
        for (auto& image : borders) image = Solid(256,256,{0,0,0,0});
        for (std::size_t i = 0; i < layout.backgroundCount; ++i)
            backgrounds[i] = Solid(256,256,{255,128,0,255});
        canvas = QuestVr::BuildPersonaUiCanvas(backgrounds,borders,layout);
        const auto& client = layout.client;
        Require(at(client.x,client.y,0) == 127u,"page client origin/tint changed");
        Require(at(client.x-1u,client.y,3) == 0u && at(client.x,client.y-1u,3) == 0u,
                "page client escaped its origin");
        Require(at(client.x+client.width-1u,client.y+client.height-1u,3) == 255u,
                "page tiles did not cover its serialized client dimensions");
        Require(at(client.x+client.width,client.y,3) == 0u &&
                at(client.x,client.y+client.height,3) == 0u,"page tiles escaped client clip");
    }
    // Logs has only four valid background tiles arranged in two columns.
    const auto& logs = QuestVr::GetPersonaUiLayout(QuestVr::PersonaUiPage::Logs);
    backgrounds[0] = Solid(256,256,{255,0,0,255});
    backgrounds[1] = Solid(256,256,{0,255,0,255});
    backgrounds[2] = Solid(256,256,{0,0,255,255});
    backgrounds[3] = Solid(256,256,{255,255,0,255});
    canvas = QuestVr::BuildPersonaUiCanvas(backgrounds,borders,logs);
    Require(at(105,47,0) == 127u && at(361,47,1) == 127u &&
            at(105,303,2) == 127u && at(361,303,0) == 127u && at(361,303,1) == 127u,
            "Logs four tiles were arranged in three columns or wrong rows");
    canvas = Solid(640,480,{0,0,0,0});
    std::array<QuestVr::PersonaUiImage,2> body{
        Solid(256,256,{255,0,0,255}),Solid(256,128,{0,255,0,255})};
    std::array<QuestVr::PersonaUiImage,2> overlays{
        Solid(256,256,{0,0,255,255}),Solid(256,128,{0,0,255,255})};
    body[0].rgba[3u] = 0u;
    QuestVr::AddPersonaHealthBody(canvas,body,overlays);
    Require(at(49,73,2) == 127u && at(50,73,0) == 255u,"health overlay/body masking or tint changed");
    Require(at(267,73,0) == 255u && at(268,73,3) == 0u,"health body exceeded 219-pixel window width");
    Require(at(49,328,0) == 255u && at(49,329,1) == 255u,"health lower body tile offset changed");
    Require(at(49,429,1) == 255u && at(49,430,3) == 0u,"health body exceeded 357-pixel window height");

    // Synthetic atlas: one exact-advance 3x2 glyph (first texel masked), plus
    // a 2-pixel spacing glyph. This tests the production font compositor rather
    // than a separate drawing implementation or an approximate system font.
    PortableBitmapFont font;
    font.objectPath = "SyntheticFont";
    const auto white = Solid(3u,2u,{255u,255u,255u,255u});
    font.pages.push_back({white.width,white.height,white.rgba});
    font.pages[0].rgba[3u] = 0u;
    font.glyphs.assign(256u,{0u,0u,0u,3u,2u});
    font.glyphs[static_cast<std::size_t>(' ')] = {0u,0u,0u,2u,0u};
    font.lineHeight = 3u;
    font.charactersPerPage = 256u;
    const auto glyph = [&](std::uint32_t code) { return GetPortableBitmapGlyph(font,code); };
    canvas = Solid(640u,480u,{12u,34u,56u,255u});
    QuestVr::DrawPersonaUiText(canvas,font,"AA",{10u,10u,8u,2u},200u,glyph);
    Require(at(10u,10u,0) == 12u && at(11u,10u,0) == 200u,
            "font mask did not preserve destination or text tint changed");
    Require(at(13u,10u,0) == 12u && at(14u,10u,0) == 200u && at(16u,10u,0) == 12u,
            "font advance added spacing or stretched glyphs");
    Require(QuestVr::PersonaUiTextWidth(font,"AA A",glyph) == 11u,
            "caption measurement did not use original glyph widths");
    canvas = Solid(640u,480u,{12u,34u,56u,255u});
    QuestVr::DrawPersonaUiText(canvas,font,"A\nA",{10u,10u,4u,5u},255u,glyph);
    Require(at(11u,10u,0) == 255u && at(11u,13u,0) == 255u && at(11u,14u,0) == 255u &&
            at(11u,15u,0) == 12u,"font newline height or vertical clipping changed");
    canvas = Solid(640u,480u,{12u,34u,56u,255u});
    QuestVr::DrawPersonaUiText(canvas,font,"A A",{10u,10u,5u,5u},255u,glyph);
    Require(at(11u,13u,0) == 255u && at(14u,10u,0) == 12u,
            "word wrapping painted in spacing or failed to start the next line");
    canvas = Solid(640u,480u,{12u,34u,56u,255u});
    QuestVr::DrawPersonaUiText(canvas,font,"AAA",{10u,10u,4u,8u},255u,glyph);
    Require(at(11u,10u,0) == 255u && at(11u,13u,0) == 255u && at(11u,16u,0) == 255u &&
            at(14u,10u,0) == 12u,"oversized word hard-wrapping escaped text window");
    canvas = Solid(640u,480u,{12u,34u,56u,255u});
    QuestVr::DrawPersonaUiText(canvas,font,"A",{10u,10u,2u,1u},255u,glyph);
    Require(at(11u,10u,0) == 255u && at(9u,10u,0) == 12u &&
            at(12u,10u,0) == 12u && at(11u,11u,0) == 12u,
            "oversized first glyph was skipped or escaped its narrow text clip");
    RequireRejected([&] { QuestVr::DrawPersonaUiText(canvas,font,"A",{639u,0u,2u,1u},255u,glyph); },
                    "out-of-canvas text rectangle accepted");
    RequireRejected([&] { QuestVr::DrawPersonaUiText(canvas,font,"A",{0xffffffffu,0u,2u,1u},255u,glyph); },
                    "overflowing text rectangle accepted");
    RequireRejected([&] { QuestVr::DrawPersonaUiText(canvas,font,"A",{0u,0u,4u,4u},256u,glyph); },
                    "out-of-byte font tint accepted");
    font.glyphs[static_cast<std::size_t>('A')].pageIndex = 1u;
    RequireRejected([&] { QuestVr::DrawPersonaUiText(canvas,font,"A",{0u,0u,4u,4u},255u,glyph); },
                    "invalid glyph atlas page accepted");
    font.glyphs[static_cast<std::size_t>('A')].pageIndex = 0u;
    font.glyphs[static_cast<std::size_t>('A')].x = 1u;
    RequireRejected([&] { QuestVr::DrawPersonaUiText(canvas,font,"A",{0u,0u,4u,4u},255u,glyph); },
                    "glyph extending beyond atlas accepted");
    font.glyphs[static_cast<std::size_t>('A')].x = 0u;
    font.pages[0].rgba.pop_back();
    RequireRejected([&] { QuestVr::DrawPersonaUiText(canvas,font,"A",{0u,0u,4u,4u},255u,glyph); },
                    "truncated font atlas accepted");
    font.pages[0].rgba = white.rgba;
    font.lineHeight = 0u;
    RequireRejected([&] { QuestVr::DrawPersonaUiText(canvas,font,"A",{0u,0u,4u,4u},255u,glyph); },
                    "zero font line height accepted");
    font.lineHeight = 3u;

    QuestVr::PersonaUiChrome chrome;
    for (auto& image : chrome.navigationBackgrounds) image = Solid(256u,21u,{255u,255u,255u,255u});
    chrome.navigationBackgrounds[2] = Solid(128u,21u,{255u,255u,255u,255u});
    for (auto& image : chrome.navigationBorders) image = Solid(256u,64u,{0u,0u,0u,0u});
    chrome.navigationBorders[2] = Solid(128u,64u,{0u,0u,0u,0u});
    chrome.normalButton[0] = Solid(4u,16u,{255u,0u,0u,255u});
    chrome.normalButton[1] = Solid(2u,16u,{0u,255u,0u,255u});
    chrome.normalButton[1].rgba[3u] = 0u;
    chrome.normalButton[2] = Solid(8u,16u,{0u,0u,255u,255u});
    chrome.filler = Solid(2u,2u,{255u,255u,255u,255u});
    canvas = Solid(640u,480u,{12u,34u,56u,255u});
    QuestVr::DrawPersonaButtonArtwork(canvas,chrome,{10u,10u,31u,16u});
    Require(at(10u,10u,0) == 127u && at(14u,10u,0) == 127u && at(14u,10u,1) == 127u &&
            at(15u,10u,1) == 127u && at(15u,10u,0) == 0u && at(33u,10u,2) == 127u,
            "button caps, center repetition, filler masking or face tint changed");
    Require(at(9u,10u,0) == 12u && at(41u,10u,0) == 12u && at(10u,26u,0) == 12u,
            "button repeated strips escaped original window");
    RequireRejected([&] { QuestVr::DrawPersonaButtonArtwork(canvas,chrome,{639u,0u,31u,16u}); },
                    "out-of-canvas button rectangle accepted");
    RequireRejected([&] { QuestVr::DrawPersonaButtonArtwork(canvas,chrome,{0xffffffffu,0u,31u,16u}); },
                    "overflowing button rectangle accepted");
    RequireRejected([&] { QuestVr::DrawPersonaButtonArtwork(canvas,chrome,{0u,0u,11u,16u}); },
                    "button narrower than original caps accepted");
    auto invalidChrome = chrome;
    invalidChrome.normalButton[0] = Solid(5u,16u,{0u,0u,0u,0u});
    RequireRejected([&] { QuestVr::DrawPersonaButtonArtwork(canvas,invalidChrome,{0u,0u,31u,16u}); },
                    "malformed original button cap dimensions accepted");
    canvas = Solid(640u,480u,{12u,34u,56u,255u});
    QuestVr::AddPersonaNavigationArtwork(canvas,chrome);
    Require(at(17u,6u,0) == 127u && at(625u,26u,0) == 127u && at(16u,6u,0) == 12u &&
            at(626u,6u,0) == 12u && at(17u,27u,0) == 12u,
            "navigation background escaped original 609x21 client rectangle");
    QuestVr::DrawPersonaNavigation(canvas,chrome,font,QuestVr::PersonaUiPage::Inventory,glyph);
    constexpr std::array<const char*,8> tabNames{{
        "Inventory","Health","Augs","Skills","Goals/Notes","Conversations","Images","Logs"}};
    std::array<std::uint32_t,8> widths{};
    std::uint32_t total{};
    for (std::size_t index = 0u; index < widths.size(); ++index) {
        widths[index] = std::max(20u,QuestVr::PersonaUiTextWidth(font,tabNames[index],glyph)+18u);
        total += widths[index];
    }
    const auto padding = (534u-total)/8u;
    std::uint32_t tabX = 23u;
    for (std::size_t index = 0u; index < widths.size(); ++index) {
        const auto expected = index == 0u ? 255u :
            (index == 2u || index == 3u || index == 5u || index == 6u ? 64u : 200u);
        Require(at(tabX+11u,11u,0) == expected && at(tabX+11u,11u,1) == expected,
                "current, available or unsupported tab text tint changed");
        tabX += widths[index]+padding;
    }
    Require(at(584u,11u,0) == 200u,"original Exit caption was not drawn in its button");
    for (const auto page : {QuestVr::PersonaUiPage::Inventory,QuestVr::PersonaUiPage::Health,
                           QuestVr::PersonaUiPage::GoalsNotes,QuestVr::PersonaUiPage::Logs}) {
        canvas = Solid(640u,480u,{12u,34u,56u,255u});
        QuestVr::DrawPersonaVrActions(canvas,chrome,font,page,glyph);
        const auto position = page == QuestVr::PersonaUiPage::Inventory ? std::array<std::uint32_t,2>{42u,382u} :
            page == QuestVr::PersonaUiPage::Health ? std::array<std::uint32_t,2>{38u,444u} :
            page == QuestVr::PersonaUiPage::GoalsNotes ? std::array<std::uint32_t,2>{25u,450u} :
            std::array<std::uint32_t,2>{115u,432u};
        Require(at(position[0],position[1],0) == 127u && at(position[0]+11u,position[1]+3u,0) == 200u,
                "VR action button artwork or original font escaped its original action bar");
    }
}
