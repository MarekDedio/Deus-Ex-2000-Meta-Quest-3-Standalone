#pragma once

#include "portable_model_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace QuestVr {

// Original GOTY UModel::LineCheck's null-owner, zero-extent path with flags6,
// as used by Decal.AttachDecal. Authored UE coordinates, not render triangles.
// This is NOT ULevel::SingleLineCheck, Actor.Trace, mover coordinates, or an
// extent sweep. The native attachment/surface projection remains a consumer.
struct ModelPointTraceLimits {
    std::size_t nodes{1'000'000u};
    std::size_t validationSteps{8'000'000u};
    std::size_t querySteps{16'000'000u};
};
struct ModelPointTraceStats {
    std::size_t validationSteps{}, querySteps{}, nodeVisits{}, leafVisits{};
};
struct ModelPointTraceResult {
    bool clear{true}; // Original LineCheck return: 1=clear, 0=blocked.
    std::int32_t node{-1}; // FCheckResult.Item is a NODE, not a surface.
    float time{1.0f};
    PortableModelVec3 location, normal;
    ModelPointTraceStats stats;
};

namespace ModelPointTraceDetail {
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original model point trace: ") + reason);
}
inline void Step(std::size_t& value, const std::size_t limit) {
    if (value >= limit) Fail("work budget exhausted");
    ++value;
}
inline bool Finite(const PortableModelVec3& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}
inline void Validate(const PortableModelGeometry& model, const ModelPointTraceLimits& limits,
    ModelPointTraceStats& stats) {
    const ModelPointTraceLimits maximum;
    if (!limits.nodes || limits.nodes > maximum.nodes || !limits.validationSteps ||
        limits.validationSteps > maximum.validationSteps || !limits.querySteps || limits.querySteps > maximum.querySteps)
        Fail("invalid limits; callers may only tighten fixed bounds");
    if (model.version != 68u || model.licenseeMode != 0u) Fail("unsupported Model layout");
    if (model.nodes.size() > limits.nodes) Fail("node budget exceeded");
    if (model.rootOutside != 0 && model.rootOutside != 1) Fail("invalid root-outside value");
    for (const auto& node : model.nodes) {
        Step(stats.validationSteps, limits.validationSteps);
        if (!std::isfinite(node.planeX) || !std::isfinite(node.planeY) ||
            !std::isfinite(node.planeZ) || !std::isfinite(node.planeW)) Fail("nonfinite BSP plane");
        for (const auto child : {node.front, node.back})
            if (child != -1 && (child < 0 || static_cast<std::size_t>(child) >= model.nodes.size()))
                Fail("invalid BSP child index");
    }
    // Validate even pruned/unreachable components. Shared children are legal;
    // cycles are not, and their repeated query visits still consume one budget.
    struct Frame { std::size_t node; unsigned next; };
    std::vector<std::uint8_t> colors(model.nodes.size());
    std::vector<Frame> stack;
    for (std::size_t root = 0; root < model.nodes.size(); ++root) {
        if (colors[root]) continue;
        colors[root] = 1u; stack.push_back({root, 0u});
        while (!stack.empty()) {
            Step(stats.validationSteps, limits.validationSteps);
            auto& frame = stack.back();
            if (frame.next == 2u) { colors[frame.node] = 2u; stack.pop_back(); continue; }
            const auto& node = model.nodes[frame.node];
            const auto child = frame.next++ == 0u ? node.front : node.back;
            if (child == -1) continue;
            const auto index = static_cast<std::size_t>(child);
            if (colors[index] == 1u) Fail("cyclic BSP front/back graph");
            if (!colors[index]) { colors[index] = 1u; stack.push_back({index, 0u}); }
        }
    }
}
inline float Distance(const PortableModelVec3& point, const PortableModelNode& node) {
    // The original null-coordinate recursion stores its x87 dot product in a
    // float before comparing with double +/-0.001. Keep that store boundary;
    // exact x87 extended rounding for nearly coplanar inputs is not asserted.
    const float value = static_cast<float>((static_cast<double>(point.z)*node.planeZ +
        static_cast<double>(point.x)*node.planeX) + static_cast<double>(point.y)*node.planeY - node.planeW);
    if (!std::isfinite(value)) Fail("plane distance overflow");
    return value;
}
inline bool Csg(const PortableModelNode& node) {
    // Original FBspNode::IsCsg(flags6): NumVertices>0 && !(NodeFlags & (6|0x21)).
    // No polygon ray/triangle test and no inferred PolyFlags_NotSolid lookup.
    return node.vertexCount != 0u && (node.nodeFlags & 0x27u) == 0u;
}
inline PortableModelVec3 Middle(const PortableModelVec3& start, const PortableModelVec3& end,
    const float startDistance, const float endDistance) {
    const double denominator = static_cast<double>(endDistance) - startDistance;
    if (!std::isfinite(denominator) || denominator == 0.0) Fail("invalid split denominator");
    const double fraction = static_cast<double>(startDistance)/denominator;
    const auto component = [fraction](const float from, const float to) {
        const float direction = from - to;
        const float offset = static_cast<float>(static_cast<double>(direction)*fraction);
        return from + offset;
    };
    const PortableModelVec3 middle{component(start.x,end.x),component(start.y,end.y),component(start.z,end.z)};
    if (!Finite(middle)) Fail("split coordinate overflow");
    return middle;
}
} // namespace ModelPointTraceDetail

inline ModelPointTraceResult TraceOriginalModelPoint(const PortableModelGeometry& model,
    const PortableModelVec3& start, const PortableModelVec3& end,
    const ModelPointTraceLimits& limits = {}) {
    using namespace ModelPointTraceDetail;
    if (!Finite(start) || !Finite(end)) Fail("nonfinite segment");
    ModelPointTraceResult result;
    Validate(model, limits, result.stats);
    // Original empty Model returns RootOutside without touching the hit.
    if (model.nodes.empty()) { result.clear = model.rootOutside != 0; return result; }
    struct Segment {
        std::int32_t node{}, lastNode{};
        PortableModelVec3 start, end;
        bool outside{};
    };
    Segment current{0,0,start,end,model.rootOutside != 0};
    std::vector<Segment> pending;
    pending.reserve(std::min<std::size_t>(model.nodes.size(),64u));
    for (;;) {
        Step(result.stats.querySteps, limits.querySteps);
        if (current.node == -1) {
            ++result.stats.leafVisits;
            if (!current.outside) {
                result.clear = false; result.node = current.lastNode;
                result.location = current.start;
                const auto& hit = model.nodes.at(static_cast<std::size_t>(result.node));
                result.normal = {hit.planeX,hit.planeY,hit.planeZ};
                break;
            }
            if (pending.empty()) return result;
            current = pending.back(); pending.pop_back();
            continue;
        }
        ++result.stats.nodeVisits;
        const auto& node = model.nodes.at(static_cast<std::size_t>(current.node));
        const float startDistance = Distance(current.start,node), endDistance = Distance(current.end,node);
        const bool csg = Csg(node);
        if (static_cast<double>(startDistance) > -0.001 && static_cast<double>(endDistance) > -0.001) {
            current.node = node.front; current.outside = current.outside || csg;
        } else if (static_cast<double>(startDistance) < 0.001 && static_cast<double>(endDistance) < 0.001) {
            current.node = node.back; current.outside = current.outside && !csg;
        } else {
            const auto middle = Middle(current.start,current.end,startDistance,endDistance);
            const bool front = startDistance > 0.0f;
            const bool nearOutside = front ? current.outside || csg : current.outside && !csg;
            const bool farOutside = front ? current.outside && !csg : current.outside || csg;
            pending.push_back({front ? node.back : node.front,current.node,middle,current.end,farOutside});
            current.node = front ? node.front : node.back;
            current.end = middle; current.outside = nearOutside;
        }
    }
    // Original wrapper projects the raw leaf-entry point onto the FULL input
    // segment, backs it off by0.5 UE units, then faces the normal against travel.
    const PortableModelVec3 delta{end.x-start.x,end.y-start.y,end.z-start.z};
    if (!Finite(delta)) Fail("segment delta overflow");
    // Native stores this sum as FLOAT before division and sqrt. Its hit-point
    // differences, in contrast, remain in x87 registers through the dot sum.
    const float square = static_cast<float>(static_cast<double>(delta.x)*delta.x +
        static_cast<double>(delta.y)*delta.y + static_cast<double>(delta.z)*delta.z);
    if (!std::isfinite(square) || square <= 0.0) Fail("blocked zero-length segment has no finite hit time");
    const float projected = static_cast<float>(((static_cast<double>(result.location.x)-start.x)*delta.x +
        (static_cast<double>(result.location.y)-start.y)*delta.y +
        (static_cast<double>(result.location.z)-start.z)*delta.z)/square);
    const double time = static_cast<double>(projected) - 0.5/std::sqrt(static_cast<double>(square));
    if (!std::isfinite(time)) Fail("nonfinite hit time");
    const double clampedTime = std::clamp(time,0.0,1.0);
    result.time = static_cast<float>(clampedTime);
    // Native fsts publishes Time without popping the unrounded register; that
    // value is used for each offset, which IS stored as FLOAT before addition.
    const auto hitComponent = [clampedTime](const float from, const float direction) {
        const float offset = static_cast<float>(static_cast<double>(direction)*clampedTime);
        return from + offset;
    };
    result.location = {hitComponent(start.x,delta.x),hitComponent(start.y,delta.y),hitComponent(start.z,delta.z)};
    if (!Finite(result.location)) Fail("hit location overflow");
    const double facing = static_cast<double>(-delta.z)*result.normal.z +
        static_cast<double>(-delta.y)*result.normal.y + static_cast<double>(-delta.x)*result.normal.x;
    if (facing < 0.0) result.normal = {-result.normal.x,-result.normal.y,-result.normal.z};
    return result;
}

} // namespace QuestVr
