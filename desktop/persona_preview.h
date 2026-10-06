#pragma once

#include "visual_renderer.h"
#include "persona_ui_canvas.h"

#include <filesystem>
#include <string>
#include <vector>

struct DesktopPersonaPreview {
    questvisual::Image image;
    std::vector<std::string> artworkPaths;
    std::vector<std::string> iconPaths;
    std::size_t transparentPixels{}, opaquePixels{}, partialAlphaPixels{};
    std::uint32_t visibleMinX{}, visibleMinY{}, visibleMaxX{}, visibleMaxY{};
    std::uint64_t rgbaHash{}, flattenedHash{};
};

DesktopPersonaPreview BuildDesktopPersonaPreview(
    const std::filesystem::path& uiPackage,
    const std::vector<std::string>& icons,
    std::size_t selectedIndex,
    QuestVr::PersonaUiPage page = QuestVr::PersonaUiPage::Inventory);
const char* DesktopPersonaPageName(QuestVr::PersonaUiPage page);
void VerifySharedPersonaCanvas();
