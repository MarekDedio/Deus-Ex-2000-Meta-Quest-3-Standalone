#pragma once
#include "portable_brush_setup.h"
#include "quest_brush_tracker_pools.h"
#include <string>
#include <vector>

struct PortableBrushTrackerMover {
    std::string actorPath;
    std::int32_t root{-1};
    std::size_t nodes{},vertices{};
};
struct PortableBrushTrackerProbe {
    QuestVr::Vm::Result execution;
    PortableBrushSetupProbe setup;
    QuestVr::OriginalBrushModelCounts original,finalCounts;
    std::vector<PortableBrushTrackerMover> movers;
    std::size_t attachments{},dynamicPoints{},activeNodes{},activeVertices{};
    bool levelOwned{},gcChecked{},topologyChecked{};
};
// Real fresh constructor Setup/pools/mandatory initial Update and insertion,
// observed under one root then rolled back. Not later runtime mover updates,
// travel teardown, query proof, committed startup or playable APK acceptance.
PortableBrushTrackerProbe ProbePortableRuntimeBrushTracker(std::size_t failAfterAttachments=0u,
    const QuestVr::Vm::Limits& limits={});
