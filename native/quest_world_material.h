#pragma once
#include "quest_world_surface_stream.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace QuestVr {
enum class OriginalMaterialBlend { Opaque, Translucent, Modulated, Highlighted };
struct OriginalMaterialState {
    std::uint32_t flags{};
    OriginalMaterialBlend blend{};
    bool masked{}, depthWrite{};
    bool Blended() const { return blend != OriginalMaterialBlend::Opaque; }
};
// Pinned UModel.h ApplyPrecedenceRules and D3D11RenderDevice::GetPipeline.
// Translucency is screen blending, NOT guessed texture alpha/50% opacity.
inline OriginalMaterialState ResolveOriginalMaterialState(std::uint32_t flags) {
    if ((flags&(4u|64u)) == 0u) flags |= 0x80000000u;
    else if ((flags&4u) != 0u) flags &= ~2u;
    const auto blend = (flags&4u) ? OriginalMaterialBlend::Translucent :
        (flags&64u) ? OriginalMaterialBlend::Modulated :
        (flags&0x10000000u) ? OriginalMaterialBlend::Highlighted : OriginalMaterialBlend::Opaque;
    return {flags,blend,(flags&2u) != 0u,(flags&0x80000000u) != 0u};
}
inline std::array<float,3> BlendOriginalMaterialRgb(OriginalMaterialBlend mode,
    std::array<float,3> source, std::array<float,3> destination, float alpha = 1.0f) {
    std::array<float,3> out;
    for (std::size_t i=0u; i<out.size(); ++i) {
        float value = source[i];
        if (mode == OriginalMaterialBlend::Translucent) value += destination[i]*(1.0f-source[i]);
        else if (mode == OriginalMaterialBlend::Modulated) value = 2.0f*source[i]*destination[i];
        else if (mode == OriginalMaterialBlend::Highlighted) value += destination[i]*(1.0f-alpha);
        out[i] = std::clamp(value,0.0f,1.0f);
    }
    return out;
}

struct WorldMaterialRange { std::size_t begin{}, end{}; std::uint32_t polyFlags{}; };
// DXQM writes one front triangle followed by its reverse winding. Preserve
// these pairs and exact DXQS surface ownership; do not change either format.
// Blended faces split by surface for meaningful back-to-front depth sorting.
inline std::vector<WorldMaterialRange> BuildWorldMaterialRanges(
    const std::vector<WorldSurfaceRecord>& records, const std::vector<std::uint32_t>& surfaceFlags,
    std::size_t maximumVertices = 4800u) {
    if (records.empty() || records.size()%6u != 0u || maximumVertices < 6u ||
        maximumVertices%6u != 0u || surfaceFlags.empty())
        throw std::runtime_error("Invalid world material paired-triangle range");
    std::vector<WorldMaterialRange> ranges;
    constexpr std::uint32_t stateMask = 2u|4u|8u|64u|0x00400000u|0x10000000u|0x80000000u;
    for (std::size_t i=0u; i<records.size(); i+=6u) {
        const auto surface = records[i].surface;
        if (surface < 0 || static_cast<std::size_t>(surface) >= surfaceFlags.size())
            throw std::runtime_error("World material surface outside original model");
        for (std::size_t j=0u; j<6u; ++j)
            if (records[i+j].surface != surface) throw std::runtime_error("World material triangle pair mixes surfaces");
        const auto state = ResolveOriginalMaterialState(surfaceFlags[static_cast<std::size_t>(surface)]);
        const auto flags = state.flags&stateMask;
        if (ranges.empty() || ranges.back().polyFlags != flags || i-ranges.back().begin >= maximumVertices ||
            (state.Blended() && records[ranges.back().begin].surface != surface))
            ranges.push_back({i,i+6u,flags});
        else ranges.back().end = i+6u;
    }
    return ranges;
}
} // namespace QuestVr
