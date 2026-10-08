#include "portable_lightmap_math.h"
#include "portable_model_geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace QuestVr {
namespace {

bool Finite(const LightmapVec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

float Dot(const LightmapVec3& a, const LightmapVec3& b) noexcept {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

bool Dimensions(std::uint32_t width, std::uint32_t height,
                const PortableLightmapLimits& limits, std::size_t& count) noexcept {
    if (!width || !height || width > limits.maximumDimension ||
        height > limits.maximumDimension || width > limits.maximumPixels / height)
        return false;
    count = static_cast<std::size_t>(width) * height;
    return true;
}

const std::array<float, 256>& BrightnessTable() noexcept {
    // hsb.cpp's table generator uses DOUBLE sqrt and prints six decimals.
    // Reproduce the stored float literals rather than an approximate float
    // sqrt. The host test compares all entries with the actual pinned table.
    static const auto values = [] {
        std::array<float, 256> table{};
        for (std::size_t i=0; i<table.size(); ++i)
            table[i] = static_cast<float>(std::round(
                6.512735 * std::sqrt(static_cast<double>(i)) * 1000000.0) / 1000000.0);
        return table;
    }();
    return values;
}

const std::array<float, 512>& BlurTable() noexcept {
    static const auto values = [] {
        std::array<float, 512> table{};
        constexpr float weights[9] = {.125f,.25f,.125f,.25f,.5f,.25f,.125f,.25f,.125f};
        for (std::size_t i=0; i<table.size(); ++i)
            for (unsigned bit=0; bit<9; ++bit)
                table[i] += static_cast<float>((i >> bit) & 1u) * weights[bit];
        return table;
    }();
    return values;
}

bool StaticEffect(std::uint8_t effect) noexcept {
    switch (effect) {
    case 0: case 7: case 8: case 11: case 12: case 13:
    case 14: case 15: case 17: case 19: return true;
    default: return false;
    }
}

float DistanceFalloff(float distanceSquared) noexcept {
    const float v = std::sqrt(distanceSquared + .0001f);
    // Same polynomial as pinned 1+2*v^3-3*v^2, factored to avoid
    // catastrophic cancellation at the radius edge. ARM fused operations can
    // turn the expanded expression slightly negative and invalidate an entire
    // black-ambient atlas. This keeps +.0001, the radius cutoff and max1 intact.
    const float edge = 1.0f-v;
    return std::min((edge*edge*(1.0f+2.0f*v)) / v, 1.0f);
}

} // namespace

LightmapVec3 PortableLightmapHsbColor(std::uint8_t hue, std::uint8_t saturation,
                                    std::uint8_t brightness) noexcept {
    const float v = BrightnessTable()[brightness];
    constexpr float scale = 1.0f/255.0f;
    if (saturation >= 250u) return {v*scale,v*scale,v*scale};
    if (brightness == 0u) return {};
    float s = saturation*(1.0f/2.5f);
    if (s > 32.0f) s += 2.0f;
    const float sectorPosition = hue*(1.0f/85.0f);
    const float fraction = sectorPosition-static_cast<int>(sectorPosition);
    const float p = s*v*(1.0f/104.0f);
    const float q = (1.0f-fraction)*v + p*fraction;
    const float t = fraction*v + p*(1.0f-fraction);
    if (hue < 85u) return {q*scale,t*scale,p*scale};
    if (hue < 170u) return {p*scale,q*scale,t*scale};
    return {t*scale,p*scale,q*scale};
}

bool DecodePortableShadowMask(const std::vector<std::uint8_t>& bits,
    std::size_t byteOffset, std::uint32_t width, std::uint32_t height,
    std::vector<float>& result, std::string& error,
    const PortableLightmapLimits& limits) noexcept {
    try {
        std::size_t count{};
        if (!Dimensions(width,height,limits,count)) {
            error = "shadow dimensions exceed the pixel budget"; return false;
        }
        const std::size_t pitch = (static_cast<std::size_t>(width)+7u)/8u;
        const std::size_t bytes = pitch*height;
        if (byteOffset > bits.size() || bytes > bits.size()-byteOffset) {
            error = "shadow mask extends beyond authored LightBits"; return false;
        }
        std::vector<float> decoded(count);
        const auto* source = bits.data()+byteOffset;
        if (width > 2u && height > 2u) {
            const auto& blur = BlurTable();
            for (std::size_t y=0; y<height; ++y) {
                const auto middleOffset=y*pitch;
                const auto topOffset=y ? middleOffset-pitch : middleOffset;
                const auto bottomOffset=y+1u<height ? middleOffset+pitch : middleOffset;
                std::uint32_t top=source[topOffset]&7u;
                std::uint32_t middle=source[middleOffset]&7u;
                std::uint32_t bottom=source[bottomOffset]&7u;
                decoded[y*width]=blur[top|(middle<<3u)|(bottom<<6u)];
                for (std::size_t x=2; x<width; ++x) {
                    const auto byteIndex=x>>3u, bitIndex=x&7u;
                    top=((top<<1u)|((source[topOffset+byteIndex]>>bitIndex)&1u))&7u;
                    middle=((middle<<1u)|((source[middleOffset+byteIndex]>>bitIndex)&1u))&7u;
                    bottom=((bottom<<1u)|((source[bottomOffset+byteIndex]>>bitIndex)&1u))&7u;
                    decoded[y*width+x-1u]=blur[top|(middle<<3u)|(bottom<<6u)];
                }
                decoded[(y+1u)*width-1u]=blur[top|(middle<<3u)|(bottom<<6u)];
            }
        } else {
            for (std::size_t y=0; y<height; ++y)
                for (std::size_t x=0; x<width; ++x)
                    decoded[y*width+x]=static_cast<float>(
                        (source[y*pitch+(x>>3u)]>>(x&7u))&1u);
        }
        result=std::move(decoded); error.clear(); return true;
    } catch (...) { error="shadow mask allocation failed"; return false; }
}

bool BuildPortableLightmapWorldLocations(const PortableLightmapBasis& basis,
    std::vector<LightmapVec3>& result, std::string& error,
    const PortableLightmapLimits& limits) noexcept {
    try {
        std::size_t count{};
        if (!Dimensions(basis.width,basis.height,limits,count) ||
            !Finite(basis.origin) || !Finite(basis.textureU) || !Finite(basis.textureV) ||
            !Finite(basis.normal) || !std::isfinite(basis.panX) ||
            !std::isfinite(basis.panY) || !std::isfinite(basis.uScale) ||
            !std::isfinite(basis.vScale) || basis.uScale == 0.0f || basis.vScale == 0.0f) {
            error="invalid lightmap world basis or dimensions"; return false;
        }
        const auto dDot=[](const LightmapVec3& a,const LightmapVec3& b) {
            return static_cast<double>(a.x)*b.x+static_cast<double>(a.y)*b.y+
                static_cast<double>(a.z)*b.z;
        };
        const double uu=dDot(basis.textureU,basis.textureU);
        const double vv=dDot(basis.textureV,basis.textureV);
        const double uv=dDot(basis.textureU,basis.textureV);
        const double determinant=uu*vv-uv*uv;
        if (!std::isfinite(determinant) || uu <= 0.0 || vv <= 0.0 ||
            determinant <= 1e-12*uu*vv) {
            error="singular lightmap texture basis"; return false;
        }
        std::vector<LightmapVec3> locations(count);
        for (std::size_t y=0; y<basis.height; ++y) {
            const double targetV=basis.panY+static_cast<double>(y)*basis.vScale;
            for (std::size_t x=0; x<basis.width; ++x) {
                const double targetU=basis.panX+(static_cast<double>(x)-.5)*basis.uScale;
                const double a=(targetU*vv-targetV*uv)/determinant;
                const double b=(targetV*uu-targetU*uv)/determinant;
                auto& point=locations[y*basis.width+x];
                point={static_cast<float>(basis.origin.x+a*basis.textureU.x+b*basis.textureV.x),
                       static_cast<float>(basis.origin.y+a*basis.textureU.y+b*basis.textureV.y),
                       static_cast<float>(basis.origin.z+a*basis.textureU.z+b*basis.textureV.z)};
                if (!Finite(point)) { error="lightmap world point overflow"; return false; }
            }
        }
        result=std::move(locations); error.clear(); return true;
    } catch (...) { error="lightmap location allocation failed"; return false; }
}

bool EvaluatePortableStaticLight(const PortableStaticLight& light,
    const LightmapVec3& location, const LightmapVec3& unitNormal,
    float shadow, float& illumination, bool& coincident) noexcept {
    illumination=0.0f; coincident=false;
    if (!StaticEffect(light.effect) || !Finite(light.position) || !Finite(location) ||
        !Finite(unitNormal) || !std::isfinite(shadow) || shadow < 0.0f || shadow > 2.0f)
        return false;
    if (light.effect == 15u) return true; // Pinned blacklight effect.
    const LightmapVec3 offset{light.position.x-location.x,
                             light.position.y-location.y,light.position.z-location.z};
    const float lengthSquared=Dot(offset,offset);
    if (!std::isfinite(lengthSquared)) return false;
    const float radius=(static_cast<float>(light.radius)+1.0f)*25.0f;
    const float inverseRadius=1.0f/radius;
    const float inverseRadiusSquared=inverseRadius*inverseRadius;
    if (light.effect == 13u) {
        illumination=shadow*std::max(1.0f-std::sqrt(lengthSquared)*inverseRadius,0.0f);
        return true;
    }
    if (light.effect == 17u) {
        illumination=shadow*std::max(1.0f-(offset.x*offset.x+offset.y*offset.y)*
                                     inverseRadiusSquared,0.0f);
        return true;
    }
    if (light.effect == 14u) {
        const float distance=std::sqrt(lengthSquared)*inverseRadius;
        illumination=shadow*((distance > .8f && distance < 1.0f) ?
            1.0f-10.0f*std::fabs(distance-.9f) : 0.0f);
        return true;
    }
    // Pinned normalize(0) would leak NaN. The bounded implementation makes
    // that singular sample explicitly dark and counts it in bake diagnostics.
    if (lengthSquared == 0.0f) { coincident=true; return true; }
    const float distanceSquared=lengthSquared*inverseRadiusSquared;
    if (distanceSquared >= 1.0f) return true;
    const float inverseLength=1.0f/std::sqrt(lengthSquared);
    const LightmapVec3 direction{offset.x*inverseLength,offset.y*inverseLength,
                                offset.z*inverseLength};
    const float angle=std::fabs(Dot(direction,unitNormal));
    float value=shadow*DistanceFalloff(distanceSquared)*angle;
    if (light.effect == 8u || light.effect == 12u) {
        constexpr float angleScale=6.28318530717958647692f/65536.0f;
        const float yaw=static_cast<float>(static_cast<std::uint32_t>(light.yaw)&65535u)*angleScale;
        const float pitch=static_cast<float>(static_cast<std::uint32_t>(light.pitch)&65535u)*angleScale;
        // Coords::Rotation().XAxis is (cosP cosY,cosP sinY,sinP);
        // SpotlightEffect uses its negative, not a generic forward vector.
        const LightmapVec3 spotDirection{-std::cos(pitch)*std::cos(yaw),
                                        -std::cos(pitch)*std::sin(yaw),-std::sin(pitch)};
        const float outer=1.0f-light.cone*(1.0f/255.0f);
        if (outer >= 1.0f) return true;
        const float alignment=Dot(direction,spotDirection);
        float spot=1.0f-std::min((1.0f-alignment)/(1.0f-outer),1.0f);
        spot*=spot;
        value*=spot;
    }
    if (!std::isfinite(value)) return false;
    illumination=value; return true;
}

// Model adapter is implemented below the shared scalar operations so tests
// can verify bit order, blur, coordinates and effect math independently.

bool GetPortableSurfaceLightmapBasis(const PortableModelGeometry& model,
    std::size_t surfaceIndex, PortableLightmapBasis& basis, std::string& error,
    const PortableLightmapLimits& limits) noexcept {
    try {
        if (surfaceIndex >= model.surfaces.size()) {
            error="lightmap surface index is outside model"; return false;
        }
        const auto& surface=model.surfaces[surfaceIndex];
        if (surface.lightMap < 0 || static_cast<std::size_t>(surface.lightMap)>=model.lightMaps.size() ||
            surface.basePoint < 0 || static_cast<std::size_t>(surface.basePoint)>=model.points.size() ||
            surface.normalVector < 0 || static_cast<std::size_t>(surface.normalVector)>=model.vectors.size() ||
            surface.textureU < 0 || static_cast<std::size_t>(surface.textureU)>=model.vectors.size() ||
            surface.textureV < 0 || static_cast<std::size_t>(surface.textureV)>=model.vectors.size()) {
            error="lightmap surface has invalid basis references"; return false;
        }
        const auto& lm=model.lightMaps[static_cast<std::size_t>(surface.lightMap)];
        if (lm.uClamp <= 0 || lm.vClamp <= 0) {
            error="lightmap has non-positive dimensions"; return false;
        }
        const auto convert=[](const PortableModelVec3& v) { return LightmapVec3{v.x,v.y,v.z}; };
        PortableLightmapBasis candidate;
        candidate.origin=convert(model.points[static_cast<std::size_t>(surface.basePoint)]);
        candidate.textureU=convert(model.vectors[static_cast<std::size_t>(surface.textureU)]);
        candidate.textureV=convert(model.vectors[static_cast<std::size_t>(surface.textureV)]);
        candidate.normal=convert(model.vectors[static_cast<std::size_t>(surface.normalVector)]);
        candidate.panX=lm.panX; candidate.panY=lm.panY;
        candidate.uScale=lm.uScale; candidate.vScale=lm.vScale;
        candidate.width=static_cast<std::uint32_t>(lm.uClamp);
        candidate.height=static_cast<std::uint32_t>(lm.vClamp);
        std::size_t count{};
        if (!Dimensions(candidate.width,candidate.height,limits,count) ||
            !Finite(candidate.origin) || !Finite(candidate.textureU) || !Finite(candidate.textureV) ||
            !Finite(candidate.normal) || !std::isfinite(candidate.panX) ||
            !std::isfinite(candidate.panY) || !std::isfinite(candidate.uScale) ||
            !std::isfinite(candidate.vScale) || candidate.uScale == 0.0f || candidate.vScale == 0.0f) {
            error="invalid lightmap basis or dimensions"; return false;
        }
        const float normalSquared=Dot(candidate.normal,candidate.normal);
        if (!std::isfinite(normalSquared) || normalSquared <= 0.0f) {
            error="invalid lightmap surface normal"; return false;
        }
        const auto dDot=[](const LightmapVec3& a,const LightmapVec3& b) {
            return static_cast<double>(a.x)*b.x+static_cast<double>(a.y)*b.y+
                static_cast<double>(a.z)*b.z;
        };
        const double uu=dDot(candidate.textureU,candidate.textureU);
        const double vv=dDot(candidate.textureV,candidate.textureV);
        const double uv=dDot(candidate.textureU,candidate.textureV);
        const double determinant=uu*vv-uv*uv;
        if (!std::isfinite(determinant) || uu <= 0.0 || vv <= 0.0 ||
            determinant <= 1e-12*uu*vv) {
            error="singular lightmap texture basis"; return false;
        }
        const float inverseNormalLength=1.0f/std::sqrt(normalSquared);
        candidate.normal.x*=inverseNormalLength;
        candidate.normal.y*=inverseNormalLength;
        candidate.normal.z*=inverseNormalLength;
        basis=candidate; error.clear(); return true;
    } catch (...) { error="cannot read lightmap basis"; return false; }
}

bool SurfaceLightmapUv(const PortableModelGeometry& model,
    std::size_t surfaceIndex, const LightmapVec3& worldPoint,
    LightmapUv& uv, std::string& error,
    const PortableLightmapLimits& limits) noexcept {
    PortableLightmapBasis basis;
    if (!GetPortableSurfaceLightmapBasis(model,surfaceIndex,basis,error,limits)) return false;
    if (!Finite(worldPoint)) { error="non-finite lightmap vertex"; return false; }
    const double dx=static_cast<double>(worldPoint.x)-basis.origin.x;
    const double dy=static_cast<double>(worldPoint.y)-basis.origin.y;
    const double dz=static_cast<double>(worldPoint.z)-basis.origin.z;
    const double u=(dx*basis.textureU.x+dy*basis.textureU.y+dz*basis.textureU.z-basis.panX)/basis.uScale;
    const double v=(dx*basis.textureV.x+dy*basis.textureV.y+dz*basis.textureV.z-basis.panY)/basis.vScale;
    const LightmapUv candidate{static_cast<float>((u+.5)/basis.width),
                              static_cast<float>((v+.5)/basis.height)};
    if (!std::isfinite(candidate.u) || !std::isfinite(candidate.v)) {
        error="lightmap UV overflow"; return false;
    }
    uv=candidate; error.clear(); return true;
}

bool BakeStaticSurfaceLightmap(const PortableModelGeometry& model,
    std::size_t surfaceIndex, const PortableLightmapZoneAmbient& zoneAmbient,
    const std::vector<PortableStaticLight>& orderedLights,
    PortableSurfaceLightmap& output, std::string& error,
    const PortableLightmapLimits& limits) noexcept {
    try {
        PortableLightmapBasis basis;
        if (!GetPortableSurfaceLightmapBasis(model,surfaceIndex,basis,error,limits)) return false;
        const auto& surface=model.surfaces[surfaceIndex];
        const auto lmIndex=static_cast<std::size_t>(surface.lightMap);
        const auto& lm=model.lightMaps[lmIndex];
        const auto lightCount=GetPortableModelStaticLightCount(model,lmIndex);
        if (lightCount != orderedLights.size() || lightCount > limits.maximumLights) {
            error="lightmap ordered actor list mismatch or budget exceeded"; return false;
        }
        const std::size_t count=static_cast<std::size_t>(basis.width)*basis.height;
        if (lightCount && count > limits.maximumSampleLightOperations/lightCount) {
            error="lightmap sample/light operation budget exceeded"; return false;
        }
        // Each mask has the same padded stride. Validating the terminal span
        // validates the whole contiguous allocation, including disabled and
        // unsupported-light masks, without rescanning the list per pixel/light.
        std::size_t maskBytes{};
        if (lightCount) maskBytes=GetPortableModelShadowSpan(model,lmIndex,lightCount-1u).byteCount;
        PortableSurfaceLightmap candidate;
        candidate.width=basis.width; candidate.height=basis.height;
        candidate.stats.listedLights=lightCount;
        const auto ambient=PortableLightmapHsbColor(zoneAmbient.hue,zoneAmbient.saturation,
                                                  zoneAmbient.brightness);
        candidate.pixels.assign(count,ambient);
        std::vector<LightmapVec3> locations;
        if (!BuildPortableLightmapWorldLocations(basis,locations,error,limits)) return false;
        std::vector<float> shadow;
        for (std::size_t ordinal=0; ordinal<lightCount; ++ordinal) {
            const auto& light=orderedLights[ordinal];
            const auto reference=model.lights[static_cast<std::size_t>(lm.lightActors)+ordinal];
            if (light.objectReference != reference) {
                error="lightmap actor reference differs from authored mask ordinal"; return false;
            }
            if (!light.available) {
                ++candidate.stats.missingLights;
                error="lightmap actor snapshot missing"; return false;
            }
            if (light.type == 0u || light.brightness == 0u) {
                ++candidate.stats.disabledLights; continue;
            }
            bool supported=true;
            if (light.type != 1u && light.type != 6u) {
                ++candidate.stats.unsupportedTypes;
                ++candidate.stats.unsupportedTypeCounts[light.type]; supported=false;
            }
            if (!StaticEffect(light.effect)) {
                ++candidate.stats.unsupportedEffects;
                ++candidate.stats.unsupportedEffectCounts[light.effect]; supported=false;
            }
            if (!supported) continue;
            const auto shadowOffset=static_cast<std::size_t>(lm.dataOffset)+ordinal*maskBytes;
            if (!DecodePortableShadowMask(model.lightBits,shadowOffset,basis.width,basis.height,
                                          shadow,error,limits)) return false;
            const auto color=PortableLightmapHsbColor(light.hue,light.saturation,light.brightness);
            for (std::size_t i=0; i<count; ++i) {
                float illumination{}; bool coincident{};
                if (!EvaluatePortableStaticLight(light,locations[i],basis.normal,
                                                  shadow[i],illumination,coincident)) {
                    error="invalid lightmap light/sample data"; return false;
                }
                if (coincident) ++candidate.stats.coincidentSamples;
                // Pinned builder clamps each contribution, not the sum.
                auto& pixel=candidate.pixels[i];
                pixel.x+=std::min(illumination*color.x,1.0f);
                pixel.y+=std::min(illumination*color.y,1.0f);
                pixel.z+=std::min(illumination*color.z,1.0f);
            }
            ++candidate.stats.addedLights;
        }
        output=std::move(candidate); error.clear(); return true;
    } catch (const std::exception& failure) {
        error=failure.what(); return false;
    } catch (...) { error="lightmap allocation failed"; return false; }
}

} // namespace QuestVr
