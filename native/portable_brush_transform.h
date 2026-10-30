#pragma once

#include "quest_brush_poly_transform.h"
#include <string>

// Copied selected ABrush.BuildCoords output, not an installed brush tracker,
// native pointer, mutable Model authority or proof of completed world startup.
struct PortableBrushModelCoordsSnapshot {
    std::string actorPath;
    QuestVr::OriginalBrushModelCoords coords;
};
PortableBrushModelCoordsSnapshot ReadPortableRuntimeBrushModelCoords(const std::string& actorPath);
