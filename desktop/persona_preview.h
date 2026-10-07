#pragma once

#include "visual_renderer.h"
#include "persona_ui_canvas.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

struct DesktopPersonaFontProvenance {
    std::string objectPath;
    std::vector<std::string> atlasPaths;
    std::vector<std::array<std::uint32_t,2>> atlasDimensions;
    std::size_t glyphCount{};
    std::uint32_t charactersPerPage{}, lineHeight{};
};

struct DesktopPersonaPreview {
    questvisual::Image image;
    std::vector<std::string> artworkPaths;
    std::vector<std::string> iconPaths;
    std::size_t transparentPixels{}, opaquePixels{}, partialAlphaPixels{};
    std::uint32_t visibleMinX{}, visibleMinY{}, visibleMaxX{}, visibleMaxY{};
    std::uint64_t rgbaHash{}, flattenedHash{};
    std::array<DesktopPersonaFontProvenance,2> fonts;
    std::string fixtureLeftText, fixtureRightText;
};

DesktopPersonaPreview BuildDesktopPersonaPreview(
    const std::filesystem::path& uiPackage,
    const std::vector<std::string>& icons,
    std::size_t selectedIndex,
    QuestVr::PersonaUiPage page = QuestVr::PersonaUiPage::Inventory);
const char* DesktopPersonaPageName(QuestVr::PersonaUiPage page);
void VerifySharedPersonaCanvas();
