#pragma once
#include "portable_model_geometry.h"
#include "quest_portable_vm.h"
#include <cstddef>
#include <cstdint>
#include <string>

// Original Model-owned mutation, always rolled back by this observation API.
// Not a tracker, query proof, save schema or published render frame.
struct PortableBrushGeometryProbe {
    QuestVr::Vm::Result execution;
    std::string modelPath;
    std::size_t nodes{},surfaces{},writes{},retainedBytes{};
    std::uint64_t generation{},revision{},beforeFlags{},afterFlags{};
    bool mutableOwnerObserved{},queryUsesMutable{},immutableProofRefused{};
};
PortableBrushGeometryProbe ProbePortableRuntimeBrushSphereFilter(const PortableModelSphere& sphere,
    std::size_t failAfterWrites=0u,const QuestVr::Vm::Limits& limits={});
bool HasPortableRuntimeMutableBrushGeometry();
