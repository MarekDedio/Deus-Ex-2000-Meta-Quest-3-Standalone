#pragma once

#include <cstddef>

struct PortableGcProbeResult {
    bool passed{};
    std::size_t baselineObjects{};
    std::size_t peakObjects{};
    std::size_t finalObjects{};
    std::size_t destroyedObjects{};
};

PortableGcProbeResult RunPortableGcProbe();
