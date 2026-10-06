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
}
