#pragma once
#include "portable_model_geometry.h"
#include "quest_portable_vm.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct PortableBrushSetupSurface {
    std::string moverPath,polysPath,texturePath;
    std::size_t polygon{},surface{};
    std::int32_t polygonLink{};
    PortableModelSurface value;
    std::array<PortableModelVec3,4u> placeholders;
};
struct PortableBrushSetupMover {
    std::string actorPath,modelPath,polysPath;
    std::size_t polygons{},firstSurface{};
    std::array<float,3u> oldPos;
    std::array<std::int32_t,3u> oldRot;
    std::int32_t brushRoot{};
    bool assimilated{},specialLit{};
};
struct PortableBrushSetupProbe {
    QuestVr::Vm::Result execution;
    std::vector<PortableBrushSetupMover> movers;
    std::vector<PortableBrushSetupSurface> surfaces;
    std::size_t originalSurfaces{},originalVectors{},originalPoints{},completedPolygons{};
    bool modelPolysEdges{},textureEdges{},actorEdges{},gcOwnershipChecked{};
};
// Actual native setup writes, observed then rolled back in the same VM root.
// Not tracker publication, pool growth, initial Update or successful startup.
PortableBrushSetupProbe ProbePortableRuntimeBrushSetup(std::size_t failAfterPolygons=0u,
    const QuestVr::Vm::Limits& limits={});
