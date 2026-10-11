#include "quest_decal_surface_select.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Small authored polygons with independently computed planes/edge signs.
// These tests do not substitute renderer triangles for the original iPlane
// chain or claim that a surface selection constitutes native Attach success.
namespace {
using Vec = PortableModelVec3;
using Model = PortableModelGeometry;
using Limits = QuestVr::DecalSurfaceSelectLimits;
std::size_t checks{}, refusals{};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Near(float actual, float expected, const std::string& message) {
    Require(std::isfinite(actual) && std::fabs(actual - expected) <= 0.000002f, message);
}
void SameVec(const Vec& actual, const Vec& expected, const std::string& message) {
    Near(actual.x, expected.x, message + " X");
    Near(actual.y, expected.y, message + " Y");
    Near(actual.z, expected.z, message + " Z");
}
void Refused(const std::function<void()>& operation, const std::string& message) {
    bool refused{};
    try { operation(); }
    catch (const std::runtime_error& error) {
        refused = std::string(error.what()).rfind("Original decal surface selection: ", 0u) == 0u;
    }
    Require(refused, message); ++refusals;
}
void AddPolygon(Model& model, const std::vector<Vec>& polygon, Vec normal = {0, 0, 1}) {
    Require(!polygon.empty() && polygon.size() <= 255u, "Invalid generated polygon fixture");
    PortableModelSurface surface;
    surface.basePoint = static_cast<std::int32_t>(model.points.size());
    surface.normalVector = static_cast<std::int32_t>(model.vectors.size());
    model.vectors.push_back(normal);
    PortableModelNode node;
    node.surface = static_cast<std::int32_t>(model.surfaces.size());
    node.vertexPool = static_cast<std::int32_t>(model.vertices.size());
    node.vertexCount = static_cast<std::uint8_t>(polygon.size());
    node.front = node.back = node.plane = -1;
    for (const auto& point : polygon) {
        model.vertices.push_back({static_cast<std::int32_t>(model.points.size()), -1});
        model.points.push_back(point);
    }
    model.surfaces.push_back(surface); model.nodes.push_back(node);
}
std::vector<Vec> Square(float x = 0, float y = 0, float z = 0) {
    return {{x, y, z}, {x + 2, y, z}, {x + 2, y + 2, z}, {x, y + 2, z}};
}
Model OneSquare() { Model model; AddPolygon(model, Square()); return model; }
auto Find(const Model& model, Vec point, std::int32_t node = 0, const Limits& limits = {}) {
    return QuestVr::FindOriginalDecalCoplanarSurface(model, node, point, limits);
}

void BoundariesAndWinding() {
    for (bool reverse : {false, true}) {
        for (std::size_t rotation = 0; rotation < 4u; ++rotation) {
            for (float normalZ : {1.0f, -1.0f, 2.0f, -2.0f}) {
                auto polygon = Square();
                if (reverse) std::reverse(polygon.begin(), polygon.end());
                std::rotate(polygon.begin(), polygon.begin() + rotation, polygon.end());
                Model model; AddPolygon(model, polygon, {0, 0, normalZ});
                for (const Vec point : {Vec{1, 1, 0}, Vec{0, 1, 0}, Vec{2, 1, 0}, Vec{1, 0, 0},
                        Vec{1, 2, 0}, Vec{0, 0, 0}, Vec{2, 2, 0}, Vec{1, 1, 100}}) {
                    const auto selected = Find(model, point);
                    Require(selected.surface == 0 && selected.node == 0 && selected.stats.edgeTests == 4u,
                        "Winding/normal/edge boundary or added plane-proximity test changed selection");
                }
                for (const Vec point : {Vec{-1, 1, 0}, Vec{3, 1, 0}, Vec{1, -1, 0},
                        Vec{1, 3, 0}, Vec{3, 3, 0}, Vec{-1, -1, 0}})
                    Require(Find(model, point).surface == -1, "Outside square selected a surface");
            }
        }
    }
    const auto model = OneSquare();
    Require(Find(model, {std::nextafter(0.0f, -1.0f), 1, 0}).surface == -1,
        "Negative adjacent float was hidden by an edge epsilon");
    Require(Find(model, {std::nextafter(2.0f, 3.0f), 1, 0}).surface == -1,
        "Positive adjacent float was hidden by an edge epsilon");
    Require(Find(model, {std::nextafter(0.0f, 1.0f), 1, 0}).surface == 0,
        "Smallest positive interior float failed exact edge signs");

    // The native does not remember the first nonzero sign. The sequence for
    // this degenerate authored polygon is 0,+,0,0,-,0: zeros bridge signs.
    Model bridged;
    AddPolygon(bridged, {{1, 0, 0}, {0, 1, 0}, {}, {1, 0, 0}, {0, -1, 0}, {}});
    Require(Find(bridged, {}).surface == 0,
        "Zero edge stopped bridging opposite signs by retaining a first sign");
    Model adjacent;
    AddPolygon(adjacent, {{1, 0, 0}, {0, 1, 0}, {2, 0, 0}});
    Require(Find(adjacent, {}).surface == -1, "Adjacent opposite signs were not rejected");

    // Original cross Y/Z and PREVIOUS distance are FLOAT stores, while cross
    // X and CURRENT distance remain unrounded. These min-float products have
    // current opposite signs but both round to previous +/-zero.
    const float tiny = std::numeric_limits<float>::denorm_min();
    Model roundedPrevious;
    AddPolygon(roundedPrevious, {{0, 0, 0}, {tiny, 0, 1}, {-tiny, 0, 2}, {0, 0, 3}}, {0, tiny, 0});
    const auto rounded = Find(roundedPrevious, {});
    Require(rounded.surface == 0 && rounded.stats.edgeTests == 4u,
        "Previous distance was not FLOAT-stored after current sign comparison");
    Model single; AddPolygon(single, {{1, 2, 3}});
    Require(Find(single, {100, 200, 300}).surface == 0, "Single-vertex zero edge invented a rejection");
    Model zeroNormal; AddPolygon(zeroNormal, Square(), {});
    Require(Find(zeroNormal, {100, 200, 300}).surface == 0,
        "Zero normal in standalone helper invented a fallback or plane guard");
}

void ChainAndIdentity() {
    Model model;
    AddPolygon(model, Square(10, 10));
    AddPolygon(model, Square(20, 20));
    AddPolygon(model, Square());
    model.nodes[0].plane = 1;
    model.nodes[0].front = model.nodes[0].back = 2;
    Require(Find(model, {1, 1, 0}).surface == -1,
        "Selector traversed front/back nodes rather than only iPlane");
    model.nodes[1].plane = 2;
    const auto result = Find(model, {1, 1, 0});
    Require(result.surface == 2 && result.node == 2 && result.stats.nodeVisits == 3u,
        "Coplanar-chain visit order changed");
    model.nodes[0].vertexCount = 0;
    model.nodes[0].surface = -99; model.nodes[0].vertexPool = -99;
    model.nodes[0].plane = 2;
    const auto emptyHead = Find(model, {1, 1, 0});
    Require(emptyHead.surface == 2 && emptyHead.stats.nodeVisits == 2u && emptyHead.stats.edgeTests == 4u,
        "Empty polygon did not follow iPlane without dereferencing its surface/span");
    model.nodes[2].surface = 0;
    Require(Find(model, {1, 1, 0}).surface == 0 && Find(model, {1, 1, 0}).node == 2,
        "Returned BSP node index in place of authored surface identity");
    model.nodes[2].plane = 1; model.nodes[1].plane = -1;
    model.nodes[1].surface = 1;
    Require(Find(model, {1, 1, 0}).surface == 0, "First accepted surface was not retained");

    Model shared;
    AddPolygon(shared, Square(10, 10)); AddPolygon(shared, Square(20, 20)); AddPolygon(shared, Square());
    shared.nodes[0].plane = shared.nodes[1].plane = 2;
    Require(Find(shared, {1, 1, 0}, 0).surface == 2 && Find(shared, {1, 1, 0}, 1).surface == 2,
        "Shared coplanar tail was mistaken for a cycle");
    const auto missing = Find(shared, {}, -1);
    Require(missing.surface == -1 && missing.node == -1 && missing.stats.querySteps == 0u,
        "INDEX_NONE query invented node/surface state");
    Model empty;
    Require(Find(empty, {}, -1).surface == -1, "Empty model's null start failed");
}

void PlaneCenters() {
    using QuestVr::IntersectOriginalDecalSurfacePlane;
    SameVec(IntersectOriginalDecalSurfacePlane({1, 1, 10}, {1, 1, -10}, {0, 0, 2}, {0, 0, 1}),
        {1, 1, 2}, "Exact floor center used backed-off ray hit");
    SameVec(IntersectOriginalDecalSurfacePlane({1, 1, -10}, {1, 1, 10}, {0, 0, 2}, {0, 0, -7}),
        {1, 1, 2}, "Reversed segment or nonunit normal changed center");
    SameVec(IntersectOriginalDecalSurfacePlane({1, 1, 10}, {1, 1, -10}, {0, 0, 30}, {0, 0, 1}),
        {1, 1, 30}, "Line-plane helper improperly clamped to segment");
    SameVec(IntersectOriginalDecalSurfacePlane({0, 0, 0}, {2, 4, 6}, {2, 0, 0}, {1, 1, 1}),
        {1.0f / 3.0f, 2.0f / 3.0f, 1.0f}, "Oblique authored plane intersection");
    const auto roundedRatio = IntersectOriginalDecalSurfacePlane({}, {3, 7, 0}, {1, 0, 0}, {1, 0, 0});
    Require(roundedRatio.y == 2.333333492279052734375f,
        "Intersection ratio was not FLOAT-stored before vector multiply");

    Model model; AddPolygon(model, Square(0, 0, 2));
    auto polygon = model.nodes[0];
    PortableModelSurface initial; initial.basePoint = static_cast<std::int32_t>(model.points.size());
    initial.normalVector = static_cast<std::int32_t>(model.vectors.size());
    model.vectors.push_back({0, 0, 1}); model.points.push_back({0, 0, 0}); model.surfaces.push_back(initial);
    PortableModelNode emptyHit; emptyHit.surface = 1; emptyHit.vertexCount = 0; emptyHit.plane = 1;
    emptyHit.vertexPool = -1; emptyHit.front = emptyHit.back = -1;
    model.nodes = {emptyHit, polygon};
    const auto result = QuestVr::SelectOriginalDecalPrimarySurface(model, 0, {1, 1, 10}, {1, 1, -10});
    Require(result.surface == 0 && result.node == 1, "Primary selection confused node/surface identity");
    SameVec(result.center, {1, 1, 2}, "Selected surface's own plane was not re-intersected");
    SameVec(result.surfaceBase, {0, 0, 2}, "Selected surface base identity lost");
    SameVec(result.surfaceNormal, {0, 0, 1}, "Selected authored normal changed");
    Require(QuestVr::SelectOriginalDecalPrimarySurface(model, -1, {}, {0, 0, 1}).surface == -1,
        "Missing ray item generated a primary attachment");
    Require(QuestVr::SelectOriginalDecalPrimarySurface(model, 0, {3, 3, 10}, {3, 3, -10}).surface == -1,
        "Outside primary polygon generated a selected center");
    const auto secondary = QuestVr::SelectOriginalDecalSecondarySurface(model, 0, {1, 1, 10}, {1, 1, -10});
    Require(secondary.surface == 0 && secondary.node == 1, "Secondary surface identity changed");
    SameVec(secondary.center, {1, 1, 0}, "Secondary wrongly re-intersected selected surface plane");
    SameVec(secondary.surfaceBase, {0, 0, 2}, "Secondary selected base identity lost");
    // With a selected X-normal surface, this vertical ray cannot intersect
    // its plane. Native secondary still resolves it before the later angle
    // skip; using primary's extra intersection would spuriously refuse it.
    model.vectors[0] = {1, 0, 0};
    Refused([&] { QuestVr::SelectOriginalDecalPrimarySurface(model, 0, {1, 1, 10}, {1, 1, -10}); },
        "Primary selected singular plane was not explicitly refused");
    const auto singularSecondary = QuestVr::SelectOriginalDecalSecondarySurface(model, 0,
        {1, 1, 10}, {1, 1, -10});
    Require(singularSecondary.surface == 0 && singularSecondary.node == 1,
        "Secondary prematurely rejected selected singular plane before angle skip");
    SameVec(singularSecondary.center, {1, 1, 0}, "Secondary initial center changed at singular selected plane");
    SameVec(singularSecondary.surfaceNormal, {1, 0, 0}, "Secondary selected normal was not resolved");
    Require(QuestVr::SelectOriginalDecalSecondarySurface(model, -1, {}, {0, 0, 1}).surface == -1,
        "Missing secondary hit item generated a selection");
    model.nodes[0].surface = -1;
    Refused([&] { QuestVr::SelectOriginalDecalPrimarySurface(model, 0, {}, {0, 0, 1}); },
        "Initial hit node with no plane was not safely refused");
}

void MalformedAndBounds() {
    const auto original = OneSquare();
    auto fail = [&](const std::function<void(Model&)>& mutation, const std::string& message) {
        auto model = original; mutation(model); Refused([&] { Find(model, {1, 1, 0}); }, message);
    };
    fail([](Model& m) { m.version = 69; }, "Unsupported version accepted");
    fail([](Model& m) { m.licenseeMode = 1; }, "Unsupported licensee mode accepted");
    for (const std::int32_t index : {-2, 1, std::numeric_limits<std::int32_t>::max()}) {
        fail([&](Model& m) { m.nodes[0].plane = index; }, "Invalid coplanar index accepted");
        fail([&](Model& m) { m.nodes[0].surface = index; }, "Invalid polygon surface accepted");
        fail([&](Model& m) { m.surfaces[0].normalVector = index; }, "Invalid normal-vector index accepted");
    }
    for (const std::int32_t index : {-1, 4, std::numeric_limits<std::int32_t>::max()}) {
        fail([&](Model& m) { m.vertices[0].point = index; }, "Invalid referenced point accepted");
        fail([&](Model& m) { m.surfaces[0].basePoint = index; }, "Invalid surface base accepted");
        fail([&](Model& m) { m.nodes[0].vertexPool = index; }, "Invalid vertex span accepted");
    }
    fail([](Model& m) { m.nodes[0].plane = 0; }, "Self-cycle accepted");
    fail([](Model& m) {
        PortableModelNode a, b; a.vertexCount = b.vertexCount = 0;
        a.plane = 2; b.plane = 1; m.nodes.push_back(a); m.nodes.push_back(b);
    }, "Unreachable coplanar cycle accepted");
    fail([](Model& m) { m.nodes[0].vertexCount = 255; }, "Overflowing polygon span accepted");
    for (const float bad : {std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        fail([&](Model& m) { m.points[0].x = bad; }, "Nonfinite model point accepted");
        fail([&](Model& m) { m.vectors[0].y = bad; }, "Nonfinite model vector accepted");
        Refused([&] { Find(original, {bad, 1, 0}); }, "Nonfinite query point accepted");
        Refused([&] { QuestVr::IntersectOriginalDecalSurfacePlane({}, {0, 0, 1}, {}, {bad, 0, 1}); },
            "Nonfinite plane input accepted");
    }
    for (const std::int32_t index : {-2, 1, std::numeric_limits<std::int32_t>::max()})
        Refused([&] { Find(original, {}, index); }, "Invalid start node accepted");
    Model freeSlots = original;
    freeSlots.vertices.push_back({std::numeric_limits<std::int32_t>::max(), -99});
    freeSlots.nodes[0].front = freeSlots.nodes[0].back = -99;
    Require(Find(freeSlots, {1, 1, 0}).surface == 0,
        "Selector consumed free vertex slots or unused BSP children");

    Limits limits;
    limits.validationSteps = 1;
    Refused([&] { Find(original, {}, 0, limits); }, "Validation work bound ignored");
    limits = {}; limits.querySteps = 4;
    Refused([&] { Find(original, {1, 1, 0}, 0, limits); }, "Edge query work bound ignored");
    limits.querySteps = 5;
    Require(Find(original, {1, 1, 0}, 0, limits).stats.querySteps == 5u,
        "Exact query bound incorrectly refused or miscounted");
    for (auto field : {&Limits::nodes, &Limits::surfaces, &Limits::points, &Limits::vectors,
            &Limits::vertices, &Limits::validationSteps, &Limits::querySteps}) {
        limits = {}; limits.*field = 0;
        Refused([&] { Find(original, {}, 0, limits); }, "Zero bound accepted");
        limits = {}; ++(limits.*field);
        Refused([&] { Find(original, {}, 0, limits); }, "Raised fixed bound accepted");
    }
    limits = {}; limits.points = 3;
    Refused([&] { Find(original, {}, 0, limits); }, "Point count bound ignored");
    limits = {}; limits.vertices = 3;
    Refused([&] { Find(original, {}, 0, limits); }, "Vertex count bound ignored");
    Model larger = original; AddPolygon(larger, Square(10, 10));
    for (auto field : {&Limits::nodes, &Limits::surfaces, &Limits::vectors}) {
        limits = {}; limits.*field = 1;
        Refused([&] { Find(larger, {}, 0, limits); }, "Geometry count bound ignored");
    }

    using QuestVr::IntersectOriginalDecalSurfacePlane;
    Refused([&] { IntersectOriginalDecalSurfacePlane({}, {}, {}, {0, 0, 1}); }, "Zero line accepted");
    Refused([&] { IntersectOriginalDecalSurfacePlane({}, {1, 0, 0}, {}, {0, 0, 1}); }, "Parallel line accepted");
    Refused([&] { IntersectOriginalDecalSurfacePlane({}, {1, 0, 0}, {}, {}); }, "Zero plane normal accepted");
    const float huge = std::numeric_limits<float>::max();
    Refused([&] { IntersectOriginalDecalSurfacePlane({-huge, 0, 0}, {huge, 0, 0}, {}, {1, 0, 0}); },
        "Vector difference overflow accepted");
    Refused([&] { IntersectOriginalDecalSurfacePlane({}, {huge, 0, 0}, {1, 0, 0}, {huge, 0, 0}); },
        "Stored dot overflow accepted");
    fail([&](Model& m) { m.vectors[0].z = huge; m.points[1].x = huge; },
        "Polygon edge cross overflow accepted");
}

void DeepIterativeChain() {
    Model model = OneSquare();
    constexpr std::size_t count = 8192;
    const auto last = model.nodes[0]; model.nodes.clear();
    for (std::size_t i = 0; i < count; ++i) {
        PortableModelNode node;
        node.vertexCount = 0; node.surface = node.vertexPool = -99;
        node.plane = static_cast<std::int32_t>(i + 1u);
        model.nodes.push_back(node);
    }
    model.nodes.push_back(last);
    const auto result = Find(model, {1, 1, 0});
    Require(result.surface == 0 && result.node == static_cast<std::int32_t>(count) &&
        result.stats.nodeVisits == count + 1u && result.stats.querySteps == count + 5u,
        "Long chain failed iterative selection or deterministic work accounting");
    Limits limits; limits.querySteps = count;
    Refused([&] { Find(model, {1, 1, 0}, 0, limits); }, "Deep query bound ignored");
    model.nodes.back().plane = static_cast<std::int32_t>(count - 1u);
    Refused([&] { Find(model, {1, 1, 0}); }, "Deep terminal cycle accepted");
}

void NodeMembershipClipping() {
    using Quad = std::array<Vec, 4>;
    using QuestVr::BuildOriginalDecalNodeMembership;
    auto quad = [](float x0, float y0, float x1, float y1) -> Quad {
        return {{{x0, y0, 0}, {x1, y0, 0}, {x1, y1, 0}, {x0, y1, 0}}};
    };
    const auto model = OneSquare();
    const auto insideQuad = quad(.5f, .5f, 1.5f, 1.5f);
    const auto inside = BuildOriginalDecalNodeMembership(model, 0, insideQuad);
    Require(inside.nodes == std::vector<std::int32_t>{0} && inside.stats.clipCrossings == 0u &&
        inside.stats.clipPlanes == 4u && inside.stats.clippedPointTests == 16u,
        "Inside whole quad failed native polygon membership");
    const auto partialQuad = quad(-1, .5f, 1, 1.5f);
    const auto partial = BuildOriginalDecalNodeMembership(model, 0, partialQuad);
    Require(partial.nodes == std::vector<std::int32_t>{0} && partial.stats.clipCrossings == 2u &&
        partial.stats.maximumClipVertices == 6u,
        "Strict crossing insertion did not precede negative-vertex pruning");
    Require(BuildOriginalDecalNodeMembership(model, 0, quad(3, .5f, 4, 1.5f)).nodes.empty(),
        "Entirely outside quad gained node membership");
    const auto boundary = BuildOriginalDecalNodeMembership(model, 0, quad(2, .5f, 3, 1.5f));
    Require(boundary.nodes == std::vector<std::int32_t>{0} && boundary.stats.clipCrossings == 0u,
        "Zero boundary was removed or treated as a strict crossing");
    Require(BuildOriginalDecalNodeMembership(model, 0, quad(2, 2, 3, 3)).nodes ==
        std::vector<std::int32_t>{0}, "A nonempty single-point clipped boundary was removed");
    const Quad duplicates{{{1, 1, 0}, {1, 1, 0}, {1, 1, 0}, {1, 1, 0}}};
    const auto duplicateResult = BuildOriginalDecalNodeMembership(model, 0, duplicates);
    Require(duplicateResult.nodes == std::vector<std::int32_t>{0} &&
        duplicateResult.stats.clippedPointTests == 16u,
        "Repeated boundary/corner vertices were deduplicated");
    Require(insideQuad[0].x == .5f && insideQuad[2].y == 1.5f &&
        partialQuad[0].x == -1 && partialQuad[3].x == -1,
        "Membership replaced whole retained corners with clipped geometry");

    Model reversed; auto reversedPolygon = Square();
    std::reverse(reversedPolygon.begin(), reversedPolygon.end());
    AddPolygon(reversed, reversedPolygon);
    Require(BuildOriginalDecalNodeMembership(reversed, 0, insideQuad).nodes.empty(),
        "Membership silently made its clip winding independent like surface selection");
    reversed.vectors[0].z = -1;
    Require(BuildOriginalDecalNodeMembership(reversed, 0, insideQuad).nodes == std::vector<std::int32_t>{0},
        "Reversed authored normal/winding lost the positive clip halfspaces");

    Model multiple; AddPolygon(multiple, Square()); AddPolygon(multiple, Square(2, 0));
    multiple.nodes[1].surface = 0;
    PortableModelNode empty; empty.vertexCount = 0; empty.surface = 0; empty.vertexPool = -1;
    empty.front = empty.back = empty.plane = -1; multiple.nodes.push_back(empty);
    AddPolygon(multiple, Square());
    multiple.nodes[0].front = 3; multiple.nodes[1].nodeFlags = 255;
    const auto cache = QuestVr::BuildOriginalDecalFreshSurfaceNodeCache(multiple);
    Require(cache.surfaces.size() == 3u && cache.surfaces[0] == std::vector<std::int32_t>({0, 1, 2}) &&
        cache.surfaces[1].empty() && cache.surfaces[2] == std::vector<std::int32_t>{3},
        "Fresh PostLoad cache lost ascending table order or empty-node membership");
    const auto members = BuildOriginalDecalNodeMembership(multiple, 0, quad(1, .5f, 3, 1.5f));
    Require(members.nodes == std::vector<std::int32_t>({0, 1}),
        "Membership lost surface cache order, filtered node flags or included empty/other-surface polygons");
    multiple.nodes[2].surface = -1;
    Refused([&] { QuestVr::BuildOriginalDecalFreshSurfaceNodeCache(multiple); },
        "Fresh cache dereferenced empty node's missing surface");
    Refused([&] { BuildOriginalDecalNodeMembership(multiple, 0, insideQuad); },
        "Membership silently skipped invalid fresh-cache surface identity");
    for (std::int32_t index : {-1, 1})
        Refused([&] { BuildOriginalDecalNodeMembership(model, index, insideQuad); },
            "Invalid membership surface accepted");
    auto badQuad = insideQuad; badQuad[0].z = std::numeric_limits<float>::quiet_NaN();
    Refused([&] { BuildOriginalDecalNodeMembership(model, 0, badQuad); }, "Nonfinite corner accepted");
    Limits limits; limits.querySteps = 2;
    Refused([&] { BuildOriginalDecalNodeMembership(model, 0, insideQuad, limits); },
        "Clipping work bound ignored");
    limits = {}; limits.querySteps = 1;
    multiple.nodes[2].surface = 0;
    Refused([&] { QuestVr::BuildOriginalDecalFreshSurfaceNodeCache(multiple, limits); },
        "Fresh cache work bound ignored");
    Model manyEdges; std::vector<Vec> ring;
    for (unsigned i = 0; i < 32u; ++i) {
        const double angle = i * (6.28318530717958647692 / 32.0);
        ring.push_back({static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)), 0});
    }
    AddPolygon(manyEdges, ring);
    Refused([&] { BuildOriginalDecalNodeMembership(manyEdges, 0, quad(-10, -10, 10, 10)); },
        "Original native clip intermediate <24 bound ignored");
}
} // namespace

int main() {
    try {
        BoundariesAndWinding(); ChainAndIdentity(); PlaneCenters(); MalformedAndBounds(); DeepIterativeChain();
        NodeMembershipClipping();
        std::cout << "Original decal surface selection: " << checks << " checks, " << refusals
                  << " expected bounded refusals\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
