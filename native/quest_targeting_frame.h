#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace QuestVr {

struct TargetFramePoint { float x{}, y{}, z{}; };
struct TargetFrameRect { float x{}, y{}, width{}, height{}; };
struct TargetFrameProjection {
    float width{640.0f}, height{480.0f};
    float focalX{320.0f}, focalY{240.0f};
    float centerX{320.0f}, centerY{240.0f};
};

// DrawWindow PC302..349 deliberately replaces a mover's brush extents with
// (16,16,16), after retaining its current brush center. It is not a door outline.
inline constexpr float OriginalFrobMoverHalfExtentUnits = 16.0f;
struct OriginalFrobFrameLayout { TargetFrameRect bounds; float corner{}; };

inline std::optional<OriginalFrobFrameLayout> LayoutOriginalFrobFrame(
    TargetFrameRect box, float radiusUnits = 0.0f, float heightUnits = 0.0f,
    float width = 640.0f, float height = 480.0f) {
    if (!std::isfinite(box.x) || !std::isfinite(box.y) || !std::isfinite(box.width) ||
        !std::isfinite(box.height) || !std::isfinite(width) || !std::isfinite(height) ||
        !std::isfinite(radiusUnits) || !std::isfinite(heightUnits) || box.width < 0.0f ||
        box.height < 0.0f || radiusUnits < 0.0f || heightUnits < 0.0f ||
        width < 140.0f || height < 140.0f) return {};
    // Non-movers use the authored Unreal-unit radius/height divided by four
    // as SCREEN pixels (PC858..969), not as metres or a fraction of the box.
    float left = std::clamp(box.x+radiusUnits/4.0f,70.0f,width-70.0f);
    float top = std::clamp(box.y+heightUnits/4.0f,70.0f,height-70.0f);
    float right = std::clamp(box.x+box.width-radiusUnits/4.0f,70.0f,width-70.0f);
    float bottom = std::clamp(box.y+box.height-heightUnits/4.0f,70.0f,height-70.0f);
    const float corner = std::clamp(((right-left)+(bottom-top))*0.1f,4.0f,40.0f);
    if (right-left < corner) { left -= corner+4.0f; right += corner+4.0f; }
    if (bottom-top < corner) { top -= corner+4.0f; bottom += corner+4.0f; }
    return OriginalFrobFrameLayout{{left,top,right-left,bottom-top},corner};
}

// Original infoX/infoY=boxTL+10, measured text extent+8, then screen clamp.
inline std::optional<TargetFrameRect> LayoutOriginalFrobLabel(
    const OriginalFrobFrameLayout& frame, float textWidth, float textHeight,
    float width=640.0f,float height=480.0f) {
    if (!std::isfinite(textWidth) || !std::isfinite(textHeight) || textWidth < 0.0f ||
        textHeight <= 0.0f || !std::isfinite(width) || !std::isfinite(height) ||
        width < textWidth+28.0f || height < textHeight+28.0f) return {};
    const float w=textWidth+8.0f,h=textHeight+8.0f;
    const float x=std::clamp(frame.bounds.x+10.0f,w*0.5f+10.0f,width-10.0f-w*0.5f);
    const float y=std::clamp(frame.bounds.y+10.0f,h*0.5f+10.0f,height-10.0f-h*0.5f);
    return TargetFrameRect{x,y,w,h};
}

using TargetFrameMatrixPacking = std::array<std::array<float,16>,4>;

// The pinned SDK honors Count for FLOAT_MATRIX4, but uploads only one value
// for FLOAT_VECTOR4. Pack four rectangles into the columns of each logical
// row-major matrix. After the SDK's matrix transpose/upload, GLSL's
// StripMatrices[index/4][index%4] yields precisely (x,y,width,height).
inline TargetFrameMatrixPacking PackTargetFrameMatrices(
    const std::array<TargetFrameRect,16>& strips) {
    TargetFrameMatrixPacking matrices{};
    for (std::size_t i=0u; i<strips.size(); ++i) {
        const float components[]{strips[i].x,strips[i].y,strips[i].width,strips[i].height};
        for (std::size_t component=0u; component<4u; ++component)
            matrices[i/4u][component*4u+i%4u] = components[component];
    }
    return matrices;
}

// Camera-space coordinates use OpenXR's -Z forward. A single central-camera
// rectangle becomes a world-space billboard at the object's depth; both eyes
// then project the same geometry, rather than sharing a mono screen overlay.
inline std::optional<TargetFrameRect> ProjectTargetFrameBounds(
    const std::array<TargetFramePoint,8>& points, const TargetFrameProjection& p) {
    if (!std::isfinite(p.width) || !std::isfinite(p.height) ||
        !std::isfinite(p.focalX) || !std::isfinite(p.focalY) ||
        !std::isfinite(p.centerX) || !std::isfinite(p.centerY) ||
        p.width <= 0.0f || p.height <= 0.0f || p.focalX <= 0.0f || p.focalY <= 0.0f)
        return {};
    float left = std::numeric_limits<float>::infinity(), top = left;
    float right = -left, bottom = -left;
    for (const auto point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            !std::isfinite(point.z) || point.z >= -0.03f) return {};
        const float x = p.centerX+p.focalX*point.x/-point.z;
        const float y = p.centerY-p.focalY*point.y/-point.z;
        if (!std::isfinite(x) || !std::isfinite(y)) return {};
        left = std::min(left,x); right = std::max(right,x);
        top = std::min(top,y); bottom = std::max(bottom,y);
    }
    if (right < 0.0f || bottom < 0.0f || left > p.width || top > p.height) return {};
    return TargetFrameRect{left,top,right-left,bottom-top};
}

// Pinned to DeusEx.FrobDisplayWindow.DrawWindow: margin=70 at 640x480,
// corner=FClamp((boxW+boxH)*0.1,4,40), Offset=int(24*(TimeSeconds%0.3)).
// No custom bitmap substitutes: the PC draws Extension.Solid one-pixel strips.
// First eight strips are the black +1/+1 shadow, last eight HUD header color.
inline std::optional<std::array<TargetFrameRect,16>> BuildOriginalFrobCornerStrips(
    TargetFrameRect box, float timeSeconds, float width = 640.0f, float height = 480.0f,
    float radiusUnits=0.0f,float heightUnits=0.0f) {
    if (!std::isfinite(box.x) || !std::isfinite(box.y) || !std::isfinite(box.width) ||
        !std::isfinite(box.height) || !std::isfinite(timeSeconds) ||
        !std::isfinite(width) || !std::isfinite(height) || box.width < 0.0f || box.height < 0.0f ||
        width < 140.0f || height < 140.0f || timeSeconds < 0.0f) return {};
    const auto layout=LayoutOriginalFrobFrame(box,radiusUnits,heightUnits,width,height);
    if (!layout) return {};
    const float left=layout->bounds.x,top=layout->bounds.y;
    const float right=left+layout->bounds.width,bottom=top+layout->bounds.height;
    const float corner=layout->corner;
    const float offset = std::floor(24.0f*std::fmod(timeSeconds,0.3f));
    std::array<TargetFrameRect,16> result;
    for (std::size_t pass=0u; pass<2u; ++pass) {
        const float i = pass == 0u ? 1.0f : 0.0f;
        const auto at = pass*8u;
        result[at+0u] = {left+i+offset,top+i+offset,corner,1.0f};
        result[at+1u] = {left+i+offset,top+i+offset,1.0f,corner};
        result[at+2u] = {right+i-corner-offset,top+i+offset,corner,1.0f};
        result[at+3u] = {right+i-offset,top+i+offset,1.0f,corner};
        result[at+4u] = {left+i+offset,bottom+i-offset,corner,1.0f};
        result[at+5u] = {left+i+offset,bottom+i-corner-offset,1.0f,corner};
        result[at+6u] = {right+i-corner+1.0f-offset,bottom+i-offset,corner,1.0f};
        result[at+7u] = {right+i-offset,bottom+i-corner+1.0f-offset,1.0f,corner};
    }
    return result;
}

inline std::optional<float> RayTargetFrameBounds(
    TargetFramePoint origin, TargetFramePoint direction,
    TargetFramePoint minimum, TargetFramePoint maximum, float maximumDistance) {
    if (!std::isfinite(maximumDistance) || maximumDistance <= 0.0f) return {};
    float nearDistance = 0.0f, farDistance = maximumDistance;
    const float origins[]{origin.x,origin.y,origin.z}, directions[]{direction.x,direction.y,direction.z};
    const float minima[]{minimum.x,minimum.y,minimum.z}, maxima[]{maximum.x,maximum.y,maximum.z};
    for (unsigned axis=0u; axis<3u; ++axis) {
        if (!std::isfinite(origins[axis]) || !std::isfinite(directions[axis]) ||
            !std::isfinite(minima[axis]) || !std::isfinite(maxima[axis]) || minima[axis] > maxima[axis]) return {};
        if (std::fabs(directions[axis]) < 0.000001f) {
            if (origins[axis] < minima[axis] || origins[axis] > maxima[axis]) return {};
        } else {
            float a = (minima[axis]-origins[axis])/directions[axis];
            float b = (maxima[axis]-origins[axis])/directions[axis];
            if (a > b) std::swap(a,b);
            nearDistance = std::max(nearDistance,a); farDistance = std::min(farDistance,b);
            if (nearDistance > farDistance) return {};
        }
    }
    return nearDistance;
}

} // namespace QuestVr
