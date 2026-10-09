#pragma once

#include "portable_model_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace QuestVr {

// Isolated prerequisite for Actor.Spawn, NOT actor allocation or native 278.
// Geometry/units stay authored Unreal coordinates. The caller must supply the
// active ULevel's root Model and the prospective CLASS DEFAULT collision size,
// not the spawning actor's size or the headset's player collision capsule.
// Ported from SurrealEngine 677ee14c5b83486e6634687953779aafb7973ad6:
// UActor_Phys.cpp:251, OverlapTest.cpp:8/55, OverlapAABBModel.cpp.
// No actors, movers, rendered triangles, flags, zones or surface records enter
// the pinned world-only query. visibilityOnly is false and unused by the pin.
struct SpawnPlacementLimits {
    std::size_t nodes{1'000'000u};
    std::size_t hullWords{2'000'000u};
    std::size_t validationSteps{16'000'000u};
    // Shared across every probe; never renewed for each candidate or hull.
    std::size_t querySteps{16'000'000u};
};

struct SpawnPlacementStats {
    std::size_t validationSteps{};
    std::size_t querySteps{};
    std::size_t nodeVisits{};
    std::size_t planeTests{};
};

struct SpawnPlacementResult {
    bool found{};
    PortableModelVec3 location;
    // Zero when both collision flags are false, otherwise attempted candidates.
    // Zero-size CheckLocation still tests and accepts its first candidate.
    std::size_t probes{};
    SpawnPlacementStats stats;
};

namespace SpawnPlacementDetail {
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Spawn world placement: ") + reason);
}
inline void ValidateLimits(const SpawnPlacementLimits& limits) {
    const SpawnPlacementLimits maximum;
    if (limits.nodes == 0u || limits.nodes > maximum.nodes ||
        limits.hullWords == 0u || limits.hullWords > maximum.hullWords ||
        limits.validationSteps == 0u || limits.validationSteps > maximum.validationSteps ||
        limits.querySteps == 0u || limits.querySteps > maximum.querySteps)
        Fail("invalid limits; callers may only tighten the fixed bounds");
}
inline void Consume(std::size_t& used, std::size_t bound) {
    if (used >= bound) Fail("work budget exhausted");
    ++used;
}
inline void Finite(const PortableModelVec3& value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
        Fail("nonfinite coordinates/extents");
}
inline void Extents(const PortableModelVec3& value) {
    Finite(value);
    if (value.x < 0.0f || value.y < 0.0f || value.z < 0.0f)
        Fail("negative collision extents");
}
inline bool Zero(const PortableModelVec3& value) {
    return value.x == 0.0f && value.y == 0.0f && value.z == 0.0f;
}
inline void NodeIndex(std::int32_t index, std::size_t count) {
    // The decoder's ValidateIndex(..., allowNone=true) only accepts -1, unlike
    // the pinned recursive query's broader "index < 0 means absent" test.
    // Keep the decoded-layout contract when publicly mutable records are used.
    if (index != -1 && (index < 0 || static_cast<std::size_t>(index) >= count))
        Fail("invalid BSP child index");
}
inline float HullFloat(std::int32_t word) {
    static_assert(sizeof(float) == sizeof(std::int32_t), "UE1 hull float size");
    static_assert(std::numeric_limits<float>::is_iec559, "UE1 IEEE float layout");
    float result;
    // Packed int32 LeafHulls stores raw float bbox words after the terminator.
    // Avoid the pin's potentially unaligned, strict-aliasing reinterpret cast.
    std::memcpy(&result, &word, sizeof(result));
    if (!std::isfinite(result)) Fail("nonfinite leaf-hull bounds");
    return result;
}
struct Box { PortableModelVec3 minimum, maximum; };
struct Hull {
    std::size_t first{}, end{};
    Box bounds;
};
inline Hull ReadHull(const PortableModelGeometry& model, std::int32_t start,
                     std::size_t& used, std::size_t bound) {
    if (start < 0 || static_cast<std::size_t>(start) >= model.leafHulls.size())
        Fail("invalid collision-bound offset");
    Hull result;
    result.first = static_cast<std::size_t>(start);
    result.end = result.first;
    for (;;) {
        Consume(used, bound);
        if (result.end >= model.leafHulls.size()) Fail("missing leaf-hull terminator");
        const std::int32_t raw = model.leafHulls[result.end];
        // The pin accepts ANY negative terminator, not exclusively -1.
        if (raw < 0) break;
        const auto index = static_cast<std::uint32_t>(raw) & ~0x4000'0000u;
        if (index >= model.nodes.size()) Fail("invalid leaf-hull plane-node index");
        ++result.end;
    }
    if (model.leafHulls.size() - result.end <= 6u)
        Fail("truncated leaf-hull bbox after terminator");
    const auto* words = model.leafHulls.data() + result.end + 1u;
    result.bounds.minimum = {HullFloat(words[0]), HullFloat(words[1]), HullFloat(words[2])};
    result.bounds.maximum = {HullFloat(words[3]), HullFloat(words[4]), HullFloat(words[5])};
    if (result.bounds.minimum.x > result.bounds.maximum.x ||
        result.bounds.minimum.y > result.bounds.maximum.y ||
        result.bounds.minimum.z > result.bounds.maximum.z)
        Fail("reversed leaf-hull bbox");
    return result;
}

inline void ValidateModel(const PortableModelGeometry& model,
                          const SpawnPlacementLimits& limits, SpawnPlacementStats& stats) {
    if (model.version != 68u || model.licenseeMode != 0u)
        Fail("unsupported portable Model layout");
    if (model.nodes.empty())
        Fail("empty BSP Model with nonzero collision extents");
    if (model.nodes.size() > limits.nodes || model.leafHulls.size() > limits.hullWords)
        Fail("Model arrays exceed bounded placement limits");
    for (const auto& node : model.nodes) {
        Consume(stats.validationSteps, limits.validationSteps);
        if (!std::isfinite(node.planeX) || !std::isfinite(node.planeY) ||
            !std::isfinite(node.planeZ) || !std::isfinite(node.planeW))
            Fail("nonfinite BSP plane");
        // Do not normalize or impose a unit normal: the pinned dot products
        // use the authored coefficients literally (including a zero normal).
        NodeIndex(node.front, model.nodes.size());
        NodeIndex(node.back, model.nodes.size());
        // Same strict decoded-layout -1 sentinel contract as front/back;
        // the pin's collision query alone would skip any negative value.
        if (node.collisionBound < -1) Fail("invalid negative collision bound");
        if (node.collisionBound >= 0)
            ReadHull(model, node.collisionBound, stats.validationSteps, limits.validationSteps);
    }
    // Iterative three-color DFS validates ALL front/back components before any
    // early hit or BSP pruning can hide a cycle. Shared children are permitted;
    // their repeated query visits remain budgeted just as in the recursive pin.
    struct Frame { std::size_t node; unsigned next; };
    std::vector<unsigned char> colors(model.nodes.size(), 0u);
    std::vector<Frame> stack;
    for (std::size_t root = 0u; root < model.nodes.size(); ++root) {
        if (colors[root] != 0u) continue;
        stack.push_back({root, 0u});
        colors[root] = 1u;
        while (!stack.empty()) {
            Consume(stats.validationSteps, limits.validationSteps);
            auto& frame = stack.back();
            if (frame.next == 2u) {
                colors[frame.node] = 2u;
                stack.pop_back();
                continue;
            }
            const auto& node = model.nodes[frame.node];
            const auto child = frame.next++ == 0u ? node.front : node.back;
            if (child < 0) continue;
            const auto index = static_cast<std::size_t>(child);
            if (colors[index] == 1u) Fail("cyclic BSP front/back graph");
            if (colors[index] == 0u) {
                colors[index] = 1u;
                stack.push_back({index, 0u});
            }
        }
    }
}

inline int PlaneSide(const PortableModelVec3& center, double ex, double ey, double ez,
                     const PortableModelNode& plane) {
    const double e = ex * std::abs(static_cast<double>(plane.planeX)) +
                     ey * std::abs(static_cast<double>(plane.planeY)) +
                     ez * std::abs(static_cast<double>(plane.planeZ));
    const double s = static_cast<double>(center.x) * plane.planeX +
                     static_cast<double>(center.y) * plane.planeY +
                     static_cast<double>(center.z) * plane.planeZ - plane.planeW;
    if (!std::isfinite(e) || !std::isfinite(s)) Fail("plane query arithmetic overflow");
    // Signs are deliberately the pin's inverse convention, NOT conventional
    // positive-side/outside labels. Exact boundary contact reports side zero.
    return s - e > 0.0 ? -1 : s + e < 0.0 ? 1 : 0;
}
inline Box QueryBox(const PortableModelVec3& center, const PortableModelVec3& extent) {
    // The pin calculates in double, then rounds bbox2 endpoints back to float.
    // Refuse unrepresentable endpoints BEFORE conversion rather than relying
    // on implementation-specific out-of-range floating conversion behavior.
    const auto endpoint = [](double value) {
        const double maximum = std::numeric_limits<float>::max();
        if (!std::isfinite(value) || value < -maximum || value > maximum)
            Fail("query bbox endpoint exceeds finite float range");
        return static_cast<float>(value);
    };
    Box box;
    box.minimum = {endpoint(static_cast<double>(center.x) - extent.x),
                   endpoint(static_cast<double>(center.y) - extent.y),
                   endpoint(static_cast<double>(center.z) - extent.z)};
    box.maximum = {endpoint(static_cast<double>(center.x) + extent.x),
                   endpoint(static_cast<double>(center.y) + extent.y),
                   endpoint(static_cast<double>(center.z) + extent.z)};
    Finite(box.minimum); Finite(box.maximum);
    return box;
}
inline bool Overlap(const Box& a, const Box& b) {
    return !(a.minimum.x > b.maximum.x || b.minimum.x > a.maximum.x ||
             a.minimum.y > b.maximum.y || b.minimum.y > a.maximum.y ||
             a.minimum.z > b.maximum.z || b.minimum.z > a.maximum.z);
}
inline bool QueryValidatedWorld(const PortableModelGeometry& model,
                                const PortableModelVec3& center,
                                const PortableModelVec3& extent,
                                const SpawnPlacementLimits& limits,
                                SpawnPlacementStats& stats) {
    const auto bbox = QueryBox(center, extent);
    std::vector<std::size_t> stack{0u};
    while (!stack.empty()) {
        Consume(stats.querySteps, limits.querySteps);
        ++stats.nodeVisits;
        const auto index = stack.back(); stack.pop_back();
        const auto& node = model.nodes[index];
        if (node.collisionBound >= 0) {
            const auto hull = ReadHull(model, node.collisionBound, stats.querySteps, limits.querySteps);
            if (Overlap(hull.bounds, bbox)) {
                bool outside{};
                for (std::size_t word = hull.first; word < hull.end; ++word) {
                    Consume(stats.querySteps, limits.querySteps);
                    ++stats.planeTests;
                    const auto raw = static_cast<std::uint32_t>(model.leafHulls[word]);
                    const bool flip = (raw & 0x4000'0000u) != 0u;
                    const auto& plane = model.nodes[raw & ~0x4000'0000u];
                    if (PlaneSide(center, extent.x, extent.y, extent.z, plane) == (flip ? 1 : -1)) {
                        outside = true;
                        break;
                    }
                }
                // Only existence matters to CheckLocation. Complete validation
                // already ran; stopping on the first hit cannot hide bad data.
                if (!outside) return true;
            }
        }
        const int side = PlaneSide(center, static_cast<double>(extent.x) * 1.1,
                                   static_cast<double>(extent.y) * 1.1,
                                   static_cast<double>(extent.z) * 1.1, node);
        // Stack order preserves front recursion before back recursion.
        if (node.back >= 0 && side >= 0) stack.push_back(static_cast<std::size_t>(node.back));
        if (node.front >= 0 && side <= 0) stack.push_back(static_cast<std::size_t>(node.front));
        if (stack.size() > limits.nodes) Fail("BSP traversal stack exceeds node bound");
    }
    return false;
}
}

// Direct world-only overlap inspection. This never performs actor collision.
// Unlike the pin's unsafe Nodes.front(), empty nonzero-extent Models refuse.
// Repeated/shared hull scans and BSP visits consume explicit aggregate budgets.
inline bool ProbeSpawnWorldOverlap(const PortableModelGeometry& worldModel,
                                  const PortableModelVec3& center,
                                  const PortableModelVec3& extents,
                                  SpawnPlacementStats* outputStats = nullptr,
                                  const SpawnPlacementLimits& limits = {}) {
    SpawnPlacementDetail::ValidateLimits(limits);
    SpawnPlacementDetail::Finite(center); SpawnPlacementDetail::Extents(extents);
    SpawnPlacementStats stats;
    bool result{};
    // OverlapTester intentionally never dereferences the Model for zero extents.
    if (!SpawnPlacementDetail::Zero(extents)) {
        SpawnPlacementDetail::ValidateModel(worldModel, limits, stats);
        result = SpawnPlacementDetail::QueryValidatedWorld(worldModel, center, extents, limits, stats);
    }
    if (outputStats) *outputStats = stats;
    return result;
}

inline SpawnPlacementResult FindSpawnPlacement(const PortableModelGeometry& worldModel,
                                               const PortableModelVec3& requestedLocation,
                                               float classDefaultRadius,
                                               float classDefaultHeight,
                                               bool classCollideWorld,
                                               bool classCollideWhenPlacing,
                                               const SpawnPlacementLimits& limits = {}) {
    SpawnPlacementDetail::ValidateLimits(limits);
    SpawnPlacementDetail::Finite(requestedLocation);
    const PortableModelVec3 extents{classDefaultRadius, classDefaultRadius, classDefaultHeight};
    SpawnPlacementDetail::Extents(extents);
    SpawnPlacementResult result;
    result.location = requestedLocation;
    if (!classCollideWorld && !classCollideWhenPlacing) {
        result.found = true;
        return result;
    }
    const bool zero = SpawnPlacementDetail::Zero(extents);
    if (!zero) SpawnPlacementDetail::ValidateModel(worldModel, limits, result.stats);
    const float scale = std::max(classDefaultRadius, classDefaultHeight);
    const int offsets[]{0, 1, -1};
    for (const int z : offsets) for (const int y : offsets) for (const int x : offsets) {
        // Preserve the pin's float scale, multiplication and vector addition.
        const PortableModelVec3 candidate{requestedLocation.x + x * scale,
                                          requestedLocation.y + y * scale,
                                          requestedLocation.z + z * scale};
        SpawnPlacementDetail::Finite(candidate);
        ++result.probes;
        if (zero || !SpawnPlacementDetail::QueryValidatedWorld(worldModel, candidate, extents,
                                                               limits, result.stats)) {
            result.found = true;
            result.location = candidate;
            return result;
        }
    }
    // Exhaustion preserves the requested location, as CheckLocation does.
    return result;
}

}
