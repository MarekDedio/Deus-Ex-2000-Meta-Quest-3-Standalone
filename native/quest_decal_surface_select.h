#pragma once

#include "portable_model_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace QuestVr {

// Authored UModel polygon selection used by original GOTY AttachDecal.
// This is not a raycast: the caller supplies the node returned by LineCheck,
// or a point to test against that node and its iPlane chain. No front/back
// traversal, fixed winding, triangulation, edge epsilon or plane proximity
// test is substituted for the original helper at Engine.dll 0x103E9090.
struct DecalSurfaceSelectLimits {
    std::size_t nodes{1'000'000u}, surfaces{1'000'000u};
    std::size_t points{1'000'000u}, vectors{1'000'000u}, vertices{2'000'000u};
    std::size_t validationSteps{16'000'000u}, querySteps{8'000'000u};
};
struct DecalSurfaceSelectStats {
    std::size_t validationSteps{}, querySteps{}, nodeVisits{}, edgeTests{};
    std::size_t clipPlanes{}, clippedPointTests{}, clipCrossings{}, maximumClipVertices{};
};
struct DecalCoplanarSurfaceResult {
    std::int32_t surface{-1}, node{-1};
    DecalSurfaceSelectStats stats;
};
struct DecalPrimarySurfaceResult {
    std::int32_t surface{-1}, node{-1};
    PortableModelVec3 center, surfaceBase, surfaceNormal;
    DecalSurfaceSelectStats stats;
};
struct DecalSurfaceNodeCacheResult {
    std::vector<std::vector<std::int32_t>> surfaces;
    DecalSurfaceSelectStats stats;
};
struct DecalNodeMembershipResult {
    std::vector<std::int32_t> nodes;
    DecalSurfaceSelectStats stats;
};

namespace DecalSurfaceSelectDetail {
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original decal surface selection: ") + reason);
}
inline bool Finite(const PortableModelVec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
inline void Step(std::size_t& value, std::size_t limit) {
    if (value >= limit) Fail("work budget exhausted");
    ++value;
}
inline void Index(std::int32_t value, std::size_t count, const char* reason,
    bool allowNone = false) {
    if (allowNone && value == -1) return;
    if (value < 0 || static_cast<std::size_t>(value) >= count) Fail(reason);
}
inline void Validate(const PortableModelGeometry& model,
    const DecalSurfaceSelectLimits& limits, DecalSurfaceSelectStats& stats) {
    const DecalSurfaceSelectLimits maximum;
    const auto bounded = [](std::size_t value, std::size_t fixed) { return value && value <= fixed; };
    if (!bounded(limits.nodes, maximum.nodes) || !bounded(limits.surfaces, maximum.surfaces) ||
        !bounded(limits.points, maximum.points) || !bounded(limits.vectors, maximum.vectors) ||
        !bounded(limits.vertices, maximum.vertices) || !bounded(limits.validationSteps, maximum.validationSteps) ||
        !bounded(limits.querySteps, maximum.querySteps))
        Fail("invalid limits; callers may only tighten fixed bounds");
    if (model.version != 68u || model.licenseeMode != 0u) Fail("unsupported Model layout");
    if (model.nodes.size() > limits.nodes || model.surfaces.size() > limits.surfaces ||
        model.points.size() > limits.points || model.vectors.size() > limits.vectors ||
        model.vertices.size() > limits.vertices) Fail("geometry record budget exceeded");
    for (const auto& point : model.points) {
        Step(stats.validationSteps, limits.validationSteps);
        if (!Finite(point)) Fail("nonfinite model point");
    }
    for (const auto& vector : model.vectors) {
        Step(stats.validationSteps, limits.validationSteps);
        if (!Finite(vector)) Fail("nonfinite model vector");
    }
    for (const auto& surface : model.surfaces) {
        Step(stats.validationSteps, limits.validationSteps);
        Index(surface.basePoint, model.points.size(), "invalid surface base point");
        Index(surface.normalVector, model.vectors.size(), "invalid surface normal vector");
    }
    for (const auto& node : model.nodes) {
        Step(stats.validationSteps, limits.validationSteps);
        Index(node.plane, model.nodes.size(), "invalid coplanar node index", true);
        // Empty polygons never dereference their surface/vertex pool in the
        // original helper. Free vertex-pool slots are likewise not consumed.
        if (!node.vertexCount) continue;
        Index(node.surface, model.surfaces.size(), "invalid polygon surface index");
        if (node.vertexPool < 0 || static_cast<std::size_t>(node.vertexPool) > model.vertices.size() ||
            node.vertexCount > model.vertices.size() - static_cast<std::size_t>(node.vertexPool))
            Fail("invalid polygon vertex span");
        for (std::size_t offset = 0; offset < node.vertexCount; ++offset) {
            Step(stats.validationSteps, limits.validationSteps);
            Index(model.vertices[static_cast<std::size_t>(node.vertexPool) + offset].point,
                model.points.size(), "invalid polygon point index");
        }
    }
    // Validate every iPlane component, including unreachable ones. Each node
    // has only one next link, so this iterative three-color walk is linear,
    // permits shared tails and cannot consume the machine call stack.
    std::vector<std::uint8_t> colors(model.nodes.size());
    std::vector<std::size_t> pending;
    for (std::size_t root = 0; root < model.nodes.size(); ++root) {
        if (colors[root]) continue;
        std::int32_t current = static_cast<std::int32_t>(root);
        while (current != -1 && colors[static_cast<std::size_t>(current)] == 0u) {
            Step(stats.validationSteps, limits.validationSteps);
            const auto index = static_cast<std::size_t>(current);
            colors[index] = 1u; pending.push_back(index);
            current = model.nodes[index].plane;
        }
        if (current != -1 && colors[static_cast<std::size_t>(current)] == 1u)
            Fail("cyclic coplanar node graph");
        while (!pending.empty()) {
            Step(stats.validationSteps, limits.validationSteps);
            colors[pending.back()] = 2u; pending.pop_back();
        }
    }
}
inline PortableModelVec3 Difference(const PortableModelVec3& left, const PortableModelVec3& right) {
    // Original Core.dll FVector subtraction returns separately FLOAT-stored
    // components (0x10117780), even though its calculations use x87 registers.
    const PortableModelVec3 result{static_cast<float>(static_cast<double>(left.x) - right.x),
        static_cast<float>(static_cast<double>(left.y) - right.y),
        static_cast<float>(static_cast<double>(left.z) - right.z)};
    if (!Finite(result)) Fail("vector subtraction overflow");
    return result;
}
inline float StoredDot(const PortableModelVec3& left, const PortableModelVec3& right) {
    // Core.dll 0x10117710 adds Z then Y then X. Attach stores the return
    // value as FLOAT before computing the intersection ratio.
    const float result = static_cast<float>((static_cast<double>(left.z) * right.z +
        static_cast<double>(left.y) * right.y) + static_cast<double>(left.x) * right.x);
    if (!std::isfinite(result)) Fail("line-plane dot product overflow");
    return result;
}
inline double EdgeDistance(const PortableModelVec3& previous, const PortableModelVec3& current,
    const PortableModelVec3& normal, const PortableModelVec3& point) {
    // Engine.dll 0x103E9149..9195 retains edge differences in registers,
    // FLOAT-stores cross-product Z and Y, but keeps cross-product X unrounded.
    // Cross here is Normal x (current-previous). Global winding is not fixed.
    // Double models the unrounded intermediates; x87 boundary bit parity is
    // explicitly not claimed. No final FLOAT store before the CURRENT sign.
    const double ex = static_cast<double>(current.x) - previous.x;
    const double ey = static_cast<double>(current.y) - previous.y;
    const double ez = static_cast<double>(current.z) - previous.z;
    const float crossZ = static_cast<float>(ey * normal.x - ex * normal.y);
    const float crossY = static_cast<float>(ex * normal.z - ez * normal.x);
    const double crossX = ez * normal.y - ey * normal.z;
    if (!std::isfinite(crossZ) || !std::isfinite(crossY) || !std::isfinite(crossX))
        Fail("polygon edge cross-product overflow");
    const double planeDot = (static_cast<double>(current.y) * crossY +
        static_cast<double>(current.z) * crossZ) + static_cast<double>(current.x) * crossX;
    const double queryDot = (static_cast<double>(point.y) * crossY +
        static_cast<double>(point.z) * crossZ) + static_cast<double>(point.x) * crossX;
    const double distance = queryDot - planeDot;
    if (!std::isfinite(distance) || !std::isfinite(static_cast<float>(distance)))
        Fail("polygon edge distance overflow");
    return distance;
}
inline DecalCoplanarSurfaceResult FindValidated(const PortableModelGeometry& model,
    std::int32_t startNode, const PortableModelVec3& point,
    const DecalSurfaceSelectLimits& limits, DecalSurfaceSelectStats stats) {
    DecalCoplanarSurfaceResult result; result.stats = stats;
    for (std::int32_t currentNode = startNode; currentNode != -1;
         currentNode = model.nodes[static_cast<std::size_t>(currentNode)].plane) {
        Step(result.stats.querySteps, limits.querySteps); ++result.stats.nodeVisits;
        const auto& node = model.nodes[static_cast<std::size_t>(currentNode)];
        if (!node.vertexCount) continue;
        const auto& normal = model.vectors[static_cast<std::size_t>(
            model.surfaces[static_cast<std::size_t>(node.surface)].normalVector)];
        const auto pool = static_cast<std::size_t>(node.vertexPool);
        auto previous = model.points[static_cast<std::size_t>(model.vertices[pool + node.vertexCount - 1u].point)];
        float previousDistance{};
        bool accepted = true;
        for (std::size_t edge = 0; edge < node.vertexCount; ++edge) {
            Step(result.stats.querySteps, limits.querySteps); ++result.stats.edgeTests;
            const auto& current = model.points[static_cast<std::size_t>(model.vertices[pool + edge].point)];
            const double distance = EdgeDistance(previous, current, normal, point);
            // Original 0x103E91CA..9202 compares IMMEDIATELY previous
            // FLOAT-stored distance, initialized zero. Do not retain a first
            // nonzero sign: intervening zeros can bridge opposing signs.
            if ((distance < 0.0 && previousDistance > 0.0f) ||
                (distance > 0.0 && previousDistance < 0.0f)) {
                accepted = false; break;
            }
            previousDistance = static_cast<float>(distance);
            previous = current;
        }
        if (accepted) { result.node = currentNode; result.surface = node.surface; return result; }
    }
    return result;
}
struct ClipPlane { PortableModelVec3 normal; float w{}; };
inline ClipPlane MakeClipPlane(const PortableModelVec3& previous,
    const PortableModelVec3& current, const PortableModelVec3& surfaceNormal) {
    const double ex = static_cast<double>(current.x) - previous.x;
    const double ey = static_cast<double>(current.y) - previous.y;
    const double ez = static_cast<double>(current.z) - previous.z;
    const double crossX = ez * surfaceNormal.y - ey * surfaceNormal.z;
    ClipPlane plane;
    plane.normal = {static_cast<float>(crossX),
        static_cast<float>(ex * surfaceNormal.z - ez * surfaceNormal.x),
        static_cast<float>(ey * surfaceNormal.x - ex * surfaceNormal.y)};
    // 0x103E94C3 uses fsts for X: its rounded copy becomes plane.X, but
    // the unrounded register remains for W's current-point multiply.
    plane.w = static_cast<float>((static_cast<double>(current.y) * plane.normal.y +
        static_cast<double>(current.z) * plane.normal.z) + static_cast<double>(current.x) * crossX);
    if (!Finite(plane.normal) || !std::isfinite(plane.w)) Fail("clip plane overflow");
    return plane;
}
inline float ClipDistance(const PortableModelVec3& point, const ClipPlane& plane) {
    const float distance = static_cast<float>((static_cast<double>(plane.normal.z) * point.z +
        static_cast<double>(plane.normal.y) * point.y) +
        static_cast<double>(plane.normal.x) * point.x - plane.w);
    if (!std::isfinite(distance)) Fail("clip distance overflow");
    return distance;
}
inline PortableModelVec3 ClipIntersection(const PortableModelVec3& from,
    const PortableModelVec3& to, const ClipPlane& plane) {
    // 0x103E95AF..963D stores all deltas, but retains unrounded Z for
    // the final scale. The numerator is not FLOAT-stored; the ratio is.
    const double unroundedZ = static_cast<double>(to.z) - from.z;
    const auto delta = Difference(to, from);
    const double numerator = static_cast<double>(plane.w) -
        ((static_cast<double>(plane.normal.z) * from.z +
          static_cast<double>(plane.normal.y) * from.y) +
          static_cast<double>(plane.normal.x) * from.x);
    const double denominator = (static_cast<double>(plane.normal.y) * delta.y +
        static_cast<double>(plane.normal.z) * delta.z) +
        static_cast<double>(plane.normal.x) * delta.x;
    if (!std::isfinite(numerator) || !std::isfinite(denominator) || denominator == 0.0)
        Fail("singular clip intersection");
    const float ratio = static_cast<float>(numerator / denominator);
    if (!std::isfinite(ratio)) Fail("clip intersection ratio overflow");
    const auto component = [ratio](float origin, double direction) {
        const float offset = static_cast<float>(direction * ratio);
        if (!std::isfinite(offset)) Fail("clip intersection offset overflow");
        return static_cast<float>(static_cast<double>(origin) + offset);
    };
    const PortableModelVec3 result{component(from.x, delta.x), component(from.y, delta.y),
        component(from.z, unroundedZ)};
    if (!Finite(result)) Fail("clip intersection coordinate overflow");
    return result;
}
inline std::vector<PortableModelVec3> ClipValidated(const PortableModelGeometry& model,
    const PortableModelNode& node, const PortableModelVec3& normal,
    const std::array<PortableModelVec3, 4>& worldCorners,
    const DecalSurfaceSelectLimits& limits, DecalSurfaceSelectStats& stats) {
    std::vector<PortableModelVec3> clipped(worldCorners.begin(), worldCorners.end());
    stats.maximumClipVertices = std::max(stats.maximumClipVertices, clipped.size());
    const auto pool = static_cast<std::size_t>(node.vertexPool);
    auto previous = model.points[static_cast<std::size_t>(model.vertices[pool + node.vertexCount - 1u].point)];
    for (std::size_t edge = 0; edge < node.vertexCount; ++edge) {
        Step(stats.querySteps, limits.querySteps); ++stats.clipPlanes;
        const auto& current = model.points[static_cast<std::size_t>(model.vertices[pool + edge].point)];
        const auto plane = MakeClipPlane(previous, current, normal);
        std::vector<float> distances;
        distances.reserve(clipped.size());
        for (const auto& point : clipped) {
            Step(stats.querySteps, limits.querySteps); ++stats.clippedPointTests;
            distances.push_back(ClipDistance(point, plane));
        }
        // Native first inserts every strict-sign crossing, with distance zero,
        // then removes negative vertices. The <24 assertion applies to that
        // intermediate array, BEFORE pruning, not just the final polygon.
        const std::size_t count = clipped.size();
        std::vector<PortableModelVec3> expanded;
        std::vector<float> expandedDistances;
        expanded.reserve(23u); expandedDistances.reserve(23u);
        std::size_t temporaryCount = count;
        for (std::size_t index = 0; index < count; ++index) {
            Step(stats.querySteps, limits.querySteps);
            const auto next = (index + 1u) % count;
            expanded.push_back(clipped[index]); expandedDistances.push_back(distances[index]);
            if ((distances[index] > 0.0f && distances[next] < 0.0f) ||
                (distances[index] < 0.0f && distances[next] > 0.0f)) {
                if (++temporaryCount >= 24u) Fail("native clip polygon <24 bound exceeded");
                ++stats.clipCrossings;
                expanded.push_back(ClipIntersection(clipped[index], clipped[next], plane));
                expandedDistances.push_back(0.0f);
            }
        }
        stats.maximumClipVertices = std::max(stats.maximumClipVertices, temporaryCount);
        clipped.clear();
        for (std::size_t index = 0; index < expanded.size(); ++index) {
            Step(stats.querySteps, limits.querySteps);
            if (expandedDistances[index] >= 0.0f) clipped.push_back(expanded[index]);
        }
        if (clipped.empty()) return clipped;
        previous = current;
    }
    return clipped;
}
} // namespace DecalSurfaceSelectDetail

inline PortableModelVec3 IntersectOriginalDecalSurfacePlane(const PortableModelVec3& start,
    const PortableModelVec3& end, const PortableModelVec3& surfaceBase,
    const PortableModelVec3& surfaceNormal) {
    using namespace DecalSurfaceSelectDetail;
    if (!Finite(start) || !Finite(end) || !Finite(surfaceBase) || !Finite(surfaceNormal))
        Fail("nonfinite line-plane input");
    const auto numeratorVector = Difference(surfaceBase, start);
    const auto delta = Difference(end, start);
    const float numerator = StoredDot(numeratorVector, surfaceNormal);
    const float denominator = StoredDot(delta, surfaceNormal);
    if (denominator == 0.0f) Fail("singular line-plane denominator");
    const float ratio = static_cast<float>(static_cast<double>(numerator) / denominator);
    if (!std::isfinite(ratio)) Fail("line-plane ratio overflow");
    const auto component = [ratio](float origin, float direction) {
        const float offset = static_cast<float>(static_cast<double>(direction) * ratio);
        if (!std::isfinite(offset)) Fail("line-plane offset overflow");
        return static_cast<float>(static_cast<double>(origin) + offset);
    };
    const PortableModelVec3 center{component(start.x, delta.x), component(start.y, delta.y),
        component(start.z, delta.z)};
    if (!Finite(center)) Fail("line-plane center overflow");
    return center;
}

inline DecalCoplanarSurfaceResult FindOriginalDecalCoplanarSurface(
    const PortableModelGeometry& model, std::int32_t startNode,
    const PortableModelVec3& point, const DecalSurfaceSelectLimits& limits = {}) {
    using namespace DecalSurfaceSelectDetail;
    if (!Finite(point)) Fail("nonfinite polygon test point");
    DecalSurfaceSelectStats stats;
    Validate(model, limits, stats);
    Index(startNode, model.nodes.size(), "invalid starting node index", true);
    return FindValidated(model, startNode, point, limits, stats);
}

inline DecalPrimarySurfaceResult SelectOriginalDecalPrimarySurface(
    const PortableModelGeometry& model, std::int32_t hitNode,
    const PortableModelVec3& start, const PortableModelVec3& end,
    const DecalSurfaceSelectLimits& limits = {}) {
    using namespace DecalSurfaceSelectDetail;
    if (!Finite(start) || !Finite(end)) Fail("nonfinite primary segment");
    DecalPrimarySurfaceResult result;
    Validate(model, limits, result.stats);
    Index(hitNode, model.nodes.size(), "invalid hit node index", true);
    if (hitNode == -1) return result;
    const auto hitSurface = model.nodes[static_cast<std::size_t>(hitNode)].surface;
    Index(hitSurface, model.surfaces.size(), "hit node has no surface plane");
    const auto& initial = model.surfaces[static_cast<std::size_t>(hitSurface)];
    const auto initialCenter = IntersectOriginalDecalSurfacePlane(start, end,
        model.points[static_cast<std::size_t>(initial.basePoint)],
        model.vectors[static_cast<std::size_t>(initial.normalVector)]);
    const auto selected = FindValidated(model, hitNode, initialCenter, limits, result.stats);
    result.stats = selected.stats;
    if (selected.surface == -1) return result;
    const auto& surface = model.surfaces[static_cast<std::size_t>(selected.surface)];
    result.surface = selected.surface; result.node = selected.node;
    result.surfaceBase = model.points[static_cast<std::size_t>(surface.basePoint)];
    result.surfaceNormal = model.vectors[static_cast<std::size_t>(surface.normalVector)];
    // Attach intersects AGAIN using the selected surface's plane. In
    // particular, it does not use LineCheck's backed-off hit position.
    result.center = IntersectOriginalDecalSurfacePlane(start, end, result.surfaceBase, result.surfaceNormal);
    return result;
}

inline DecalPrimarySurfaceResult SelectOriginalDecalSecondarySurface(
    const PortableModelGeometry& model, std::int32_t hitNode,
    const PortableModelVec3& start, const PortableModelVec3& end,
    const DecalSurfaceSelectLimits& limits = {}) {
    using namespace DecalSurfaceSelectDetail;
    if (!Finite(start) || !Finite(end)) Fail("nonfinite secondary segment");
    DecalPrimarySurfaceResult result;
    Validate(model, limits, result.stats);
    Index(hitNode, model.nodes.size(), "invalid secondary hit node index", true);
    if (hitNode == -1) return result;
    const auto hitSurface = model.nodes[static_cast<std::size_t>(hitNode)].surface;
    Index(hitSurface, model.surfaces.size(), "secondary hit node has no surface plane");
    const auto& initial = model.surfaces[static_cast<std::size_t>(hitSurface)];
    const auto initialCenter = IntersectOriginalDecalSurfacePlane(start, end,
        model.points[static_cast<std::size_t>(initial.basePoint)],
        model.vectors[static_cast<std::size_t>(initial.normalVector)]);
    const auto selected = FindValidated(model, hitNode, initialCenter, limits, result.stats);
    result.stats = selected.stats;
    if (selected.surface == -1) return result;
    const auto& surface = model.surfaces[static_cast<std::size_t>(selected.surface)];
    result.surface = selected.surface; result.node = selected.node;
    result.surfaceBase = model.points[static_cast<std::size_t>(surface.basePoint)];
    result.surfaceNormal = model.vectors[static_cast<std::size_t>(surface.normalVector)];
    // Secondary 0x103E8147..8289 intersects ONLY the initial hit surface.
    // The selected surface's base/normal are resolved for the subsequent angle
    // rejection/reprojection; no selected-plane intersection occurs here.
    // Thus .center is the INITIAL selector point, not a selected-plane point.
    result.center = initialCenter;
    return result;
}

inline DecalSurfaceNodeCacheResult BuildOriginalDecalFreshSurfaceNodeCache(
    const PortableModelGeometry& model, const DecalSurfaceSelectLimits& limits = {}) {
    using namespace DecalSurfaceSelectDetail;
    DecalSurfaceNodeCacheResult result;
    Validate(model, limits, result.stats);
    result.surfaces.resize(model.surfaces.size());
    // Original fresh UModel.PostLoad at 0x103ACE90 iterates ALL nodes in
    // ascending table index and appends to iSurf, including empty polygons.
    // Repeated PostLoad appends again natively; this API is expressly fresh.
    for (std::size_t index = 0; index < model.nodes.size(); ++index) {
        Step(result.stats.querySteps, limits.querySteps); ++result.stats.nodeVisits;
        const auto surface = model.nodes[index].surface;
        Index(surface, model.surfaces.size(), "fresh surface cache node has no surface");
        result.surfaces[static_cast<std::size_t>(surface)].push_back(static_cast<std::int32_t>(index));
    }
    return result;
}

inline DecalNodeMembershipResult BuildOriginalDecalNodeMembership(
    const PortableModelGeometry& model, std::int32_t surface,
    const std::array<PortableModelVec3, 4>& worldCorners,
    const DecalSurfaceSelectLimits& limits = {}) {
    using namespace DecalSurfaceSelectDetail;
    DecalNodeMembershipResult result;
    for (const auto& corner : worldCorners)
        if (!Finite(corner)) Fail("nonfinite decal corner");
    Validate(model, limits, result.stats);
    Index(surface, model.surfaces.size(), "invalid membership surface index");
    const auto& normal = model.vectors[static_cast<std::size_t>(
        model.surfaces[static_cast<std::size_t>(surface)].normalVector)];
    // Equivalent order to a fresh cached Nodes list; no BSP traversal or
    // visible-render-polygon reconstruction is used. Whole input corners stay
    // unchanged: native membership stores node indexes, NOT clipped geometry.
    for (std::size_t index = 0; index < model.nodes.size(); ++index) {
        Step(result.stats.querySteps, limits.querySteps); ++result.stats.nodeVisits;
        const auto& node = model.nodes[index];
        Index(node.surface, model.surfaces.size(), "fresh surface cache node has no surface");
        if (node.surface != surface || !node.vertexCount) continue;
        if (!ClipValidated(model, node, normal, worldCorners, limits, result.stats).empty())
            result.nodes.push_back(static_cast<std::int32_t>(index));
    }
    // Empty membership means unrestricted to the original renderer; the
    // consumer must not interpret this result as an invisible attachment.
    return result;
}

// These are geometry prerequisites, not AttachDecal success: client/viewport,
// live texture, X-dot-N, plane residual and PolyFlags gates, quad construction,
// multidecal composition, ownership/GC/save and rendering remain
// caller responsibilities. Singular/nonfinite/budget refusals are explicit
// portable safety policies, not claimed original early-return semantics.
} // namespace QuestVr
