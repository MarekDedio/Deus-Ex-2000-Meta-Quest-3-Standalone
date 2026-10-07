#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct PortableModelGeometry;

namespace QuestVr {

// Plain CPU records. All positions, texture vectors and radii are in authored
// Unreal units; converting to Quest metres before baking changes falloff.
struct LightmapVec3 { float x{}, y{}, z{}; };
struct LightmapUv { float u{}, v{}; };

struct PortableLightmapBasis {
    LightmapVec3 origin, textureU, textureV, normal;
    float panX{}, panY{}, uScale{}, vScale{};
    std::uint32_t width{}, height{};
};

struct PortableLightmapZoneAmbient {
    std::uint8_t hue{}, saturation{255}, brightness{};
};

struct PortableStaticLight {
    std::int32_t objectReference{};
    bool available{};
    LightmapVec3 position;
    std::int32_t yaw{}, pitch{};
    std::uint8_t type{1}, effect{}, hue{}, saturation{255}, brightness{};
    std::uint8_t radius{}, cone{};
};

struct PortableLightmapLimits {
    std::uint32_t maximumDimension{4096};
    std::size_t maximumPixels{1024u * 1024u};
    std::size_t maximumLights{4096};
    std::size_t maximumSampleLightOperations{64u * 1024u * 1024u};
};

struct PortableLightmapBakeStats {
    std::size_t listedLights{}, addedLights{}, disabledLights{}, missingLights{};
    std::size_t unsupportedTypes{}, unsupportedEffects{}, coincidentSamples{};
    std::array<std::size_t, 256> unsupportedTypeCounts{};
    std::array<std::size_t, 256> unsupportedEffectCounts{};
};

struct PortableSurfaceLightmap {
    std::uint32_t width{}, height{};
    // Linear float RGB, matching pinned LightmapBuilder. No ambient floor,
    // final sum clamp, display gamma, artificial exposure or invented lights.
    std::vector<LightmapVec3> pixels;
    PortableLightmapBakeStats stats;
};

LightmapVec3 PortableLightmapHsbColor(std::uint8_t hue, std::uint8_t saturation,
                                    std::uint8_t brightness) noexcept;

// Exact ACTIVE optimized Shadowmap::Load branch from the pinned engine:
// LSB-first, byte-padded rows, 3x3 weights summing to 2, duplicated terminal
// columns and its original left-edge/sliding-window behavior. The alternate
// #if 0 clamped Gaussian is intentionally NOT substituted. Width <=2 or height
// <=2 uses unblurred bits as pinned. Invalid spans fail before allocation.
bool DecodePortableShadowMask(const std::vector<std::uint8_t>& bits,
    std::size_t byteOffset, std::uint32_t width, std::uint32_t height,
    std::vector<float>& result, std::string& error,
    const PortableLightmapLimits& limits = {}) noexcept;

// Analytic inverse of LightmapBuilder::CalcWorldLocations. It retains the
// pinned asymmetric half-texel convention (U=x-.5, V=y), but does not divide
// by scanline slopes that become zero for ordinary orthogonal texture axes.
bool BuildPortableLightmapWorldLocations(const PortableLightmapBasis& basis,
    std::vector<LightmapVec3>& result, std::string& error,
    const PortableLightmapLimits& limits = {}) noexcept;

bool GetPortableSurfaceLightmapBasis(const PortableModelGeometry& model,
    std::size_t surfaceIndex, PortableLightmapBasis& basis, std::string& error,
    const PortableLightmapLimits& limits = {}) noexcept;

// Original GLRenderDevice surface UV: (dot(U,P-Origin)-PanX+.5*UScale)
// divided by (UScale*width), with the equivalent equation for V. This is not
// an artificial inverse correction for CalcWorldLocations' asymmetric U:
// at authored sample (x,y), it returns (x/width,(y+.5)/height). Clamp only
// when sampling/uploading the texture, not here; vertices may be outside.
bool SurfaceLightmapUv(const PortableModelGeometry& model,
    std::size_t surfaceIndex, const LightmapVec3& worldPoint,
    LightmapUv& uv, std::string& error,
    const PortableLightmapLimits& limits = {}) noexcept;

// Pinned static LightEffect math. Time-dependent effects are not frozen into
// fabricated steady illumination. Unsupported effects return false; caller
// reports them. Type/color dispatch occurs in the bake rather than here.
bool EvaluatePortableStaticLight(const PortableStaticLight& light,
    const LightmapVec3& location, const LightmapVec3& unitNormal,
    float shadow, float& illumination, bool& coincident) noexcept;

// orderedLights MUST correspond one-for-one to the model's null-terminated
// light list, including disabled actors. Mask ordinal is never compressed.
// Structural/missing-actor errors leave output untouched; unsupported dynamic
// types/effects produce an explicit incomplete-static bake in output.stats.
bool BakeStaticSurfaceLightmap(const PortableModelGeometry& model,
    std::size_t surfaceIndex, const PortableLightmapZoneAmbient& zoneAmbient,
    const std::vector<PortableStaticLight>& orderedLights,
    PortableSurfaceLightmap& output, std::string& error,
    const PortableLightmapLimits& limits = {}) noexcept;

} // namespace QuestVr
