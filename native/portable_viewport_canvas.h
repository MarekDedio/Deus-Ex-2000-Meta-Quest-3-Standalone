#pragma once
#include "quest_portable_vm.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
struct PortableCanvasFieldSnapshot {std::string declaration;QuestVr::Vm::Value value;};
struct PortableViewportCanvasSnapshot {
    bool prepared{},initialized{};
    std::string objectPath,classPath,outerPath,viewportPath;
    std::uint32_t objectIndex{},classIndex{},outerIndex{},viewportIndex{},objectFlags{};
    std::int32_t uniqueCounter{};
    std::size_t inventoryCapacity{},inventoryBytes{},propertyRootBytes{};
    std::vector<PortableCanvasFieldSnapshot> fields;
};
struct PortableViewportCanvasResult {QuestVr::Vm::Result execution;PortableViewportCanvasSnapshot canvas;};
// Actual configured constructor and Init only, before Input construction.
// No Update/Reset, drawing, Console or platform OpenWindow is fabricated.
PortableViewportCanvasResult PreparePortableRuntimeViewportCanvas(std::uint64_t viewportIdentity,
    const std::string& engineIni,bool commit=true,const QuestVr::Vm::Limits& limits={});
PortableViewportCanvasSnapshot ReadPortableRuntimeViewportCanvas(std::uint64_t viewportIdentity);
