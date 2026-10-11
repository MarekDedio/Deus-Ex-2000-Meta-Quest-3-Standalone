#include "quest_model_point_trace.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Independent small BSPs with hand-computed hit planes/distances. These
// exercise the original flags6/null-owner/zero-extent path, not polygon casts,
// actor collisions, mover transforms, AttachDecal, or a second copied oracle.
namespace {
using Vec = PortableModelVec3;
using Model = PortableModelGeometry;
using Node = PortableModelNode;
using Result = QuestVr::ModelPointTraceResult;
using Limits = QuestVr::ModelPointTraceLimits;
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
Node Plane(Vec normal = {1.0f, 0.0f, 0.0f}, float distance = 0.0f,
    std::uint8_t vertices = 1u, std::uint8_t flags = 0u) {
    Node node;
    node.planeX = normal.x; node.planeY = normal.y; node.planeZ = normal.z;
    node.planeW = distance; node.vertexCount = vertices; node.nodeFlags = flags;
    node.front = node.back = node.plane = -1;
    node.surface = 741; // Deliberately unrelated to node0; no surface is read.
    node.collisionBound = node.renderBound = -1;
    return node;
}
Model OnePlane(bool outside = true, Node node = Plane()) {
    Model model; model.rootOutside = outside ? 1 : 0;
    model.nodes.push_back(node); return model;
}
Result Trace(const Model& model, Vec start, Vec end, const Limits& limits = {}) {
    return QuestVr::TraceOriginalModelPoint(model, start, end, limits);
}
void Refused(const std::function<void()>& operation, const std::string& message) {
    bool refused{};
    try { operation(); }
    catch (const std::runtime_error& error) {
        refused = std::string(error.what()).rfind("Original model point trace: ", 0u) == 0u;
    }
    Require(refused, message); ++refusals;
}
void UntouchedHit(const Result& result, const std::string& message) {
    Require(result.node == -1 && result.time == 1.0f, message + " identity/time");
    SameVec(result.location, {}, message + " location");
    SameVec(result.normal, {}, message + " normal");
}

void EmptyAndCsg() {
    Model empty; empty.rootOutside = 1;
    auto result = Trace(empty, {10.0f, 2.0f, 3.0f}, {-10.0f, 2.0f, 3.0f});
    Require(result.clear && result.stats.nodeVisits == 0u && result.stats.querySteps == 0u,
        "Empty outside Model did not return clear without recursion");
    UntouchedHit(result, "Empty outside Model changed hit");
    empty.rootOutside = 0;
    result = Trace(empty, {10.0f, 2.0f, 3.0f}, {-10.0f, 2.0f, 3.0f});
    Require(!result.clear && result.stats.nodeVisits == 0u && result.stats.querySteps == 0u,
        "Empty inside Model did not return RootOutside0");
    UntouchedHit(result, "Empty inside Model invented a blocked node");
    result = Trace(empty, {}, {});
    Require(!result.clear, "Empty inside zero-length path invented a recursive hit");
    UntouchedHit(result, "Empty zero-length path changed hit");

    // IsCsg(flags6) tests ALL mask bits0x27 and NumVertices!=0. Surface
    // PolyFlags, polygon vertices, points, hulls and materials do not enter it.
    for (unsigned flags = 0u; flags <= 255u; ++flags) {
        auto model = OnePlane(true, Plane({1.0f, 0.0f, 0.0f}, 0.0f, 1u,
            static_cast<std::uint8_t>(flags)));
        result = Trace(model, {-2.0f, 0.0f, 0.0f}, {-3.0f, 0.0f, 0.0f});
        const bool solid = (flags & 0x27u) == 0u;
        Require(result.clear != solid, "CSG flags6 mask changed back-side solidity");
        if (solid) {
            Require(result.node == 0 && result.node != model.nodes[0].surface && result.time == 0.0f,
                "Starting inside back leaf lost node identity or start-solid time");
        }
        model.rootOutside = 0;
        result = Trace(model, {2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f});
        Require(result.clear == solid, "CSG flags6 mask failed to carve front-side outside");
        model.rootOutside = 1; model.nodes[0].vertexCount = 0u;
        Require(Trace(model, {-2.0f, 0.0f, 0.0f}, {-3.0f, 0.0f, 0.0f}).clear,
            "Zero-vertex node participated in CSG solidity");
    }
    auto irrelevant = OnePlane();
    irrelevant.nodes[0].surface = -999; irrelevant.nodes[0].vertexPool = -999;
    irrelevant.nodes[0].plane = -999;
    result = Trace(irrelevant, {10.0f, 0.0f, 0.0f}, {-10.0f, 0.0f, 0.0f});
    Require(!result.clear && result.node == 0,
        "Point recursion read irrelevant polygon/surface/coplanar-chain indices");
}

void ThresholdAndFrontPrecedence() {
    const float negative = -0.001f;
    const float negativeInside = std::nextafter(negative, 0.0f);
    const float positive = 0.001f;
    const float positiveInside = std::nextafter(positive, 0.0f);
    Require(static_cast<double>(negative) < -0.001 && static_cast<double>(negativeInside) > -0.001 &&
        static_cast<double>(positive) > 0.001 && static_cast<double>(positiveInside) < 0.001,
        "IEEE threshold fixture does not straddle native double constants");
    const auto model = OnePlane();
    for (const float x : {0.0f, -0.0005f, negativeInside}) {
        Require(Trace(model, {x, 0.0f, 0.0f}, {x, 10.0f, 0.0f}).clear,
            "Front branch lost precedence in overlapping +/-0.001 band");
    }
    for (const float x : {negative, std::nextafter(negative, -1.0f)}) {
        const auto result = Trace(model, {x, 0.0f, 0.0f}, {x, 10.0f, 0.0f});
        Require(!result.clear && result.time == 0.0f,
            "Negative float threshold was compared against a rounded float constant");
    }
    // Root is nonsolid. Front subtree has a solid negative-Y leaf; back is
    // outside. +0.001f is ABOVE the double constant, so it crosses into the
    // blocked front subtree first. Its adjacent smaller float goes back only.
    auto split = OnePlane(true, Plane({1.0f, 0.0f, 0.0f}, 0.0f, 0u));
    split.nodes[0].front = 1;
    split.nodes.push_back(Plane({0.0f, 1.0f, 0.0f}));
    Require(!Trace(split, {positive, -1.0f, 0.0f}, {-0.01f, -1.0f, 0.0f}).clear,
        "Positive float threshold failed to retain near-front crossing");
    Require(Trace(split, {positiveInside, -1.0f, 0.0f}, {-0.01f, -1.0f, 0.0f}).clear,
        "Adjacent smaller positive float failed strict back-only threshold");
}

void CrossingAndNormals() {
    auto result = Trace(OnePlane(), {10.0f, 0.0f, 0.0f}, {-10.0f, 0.0f, 0.0f});
    Require(!result.clear && result.node == 0, "Crossing plane lost first blocked node");
    Near(result.time, 0.475f, "20-unit crossing did not back off by0.5 units");
    SameVec(result.location, {0.5f, 0.0f, 0.0f}, "20-unit crossing backed-off location");
    SameVec(result.normal, {1.0f, 0.0f, 0.0f}, "Front-to-back normal");
    result = Trace(OnePlane(), {-10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
    Require(!result.clear && result.node == 0 && result.time == 0.0f,
        "Start-solid segment discarded its first inside leaf");
    SameVec(result.location, {-10.0f, 0.0f, 0.0f}, "Start-solid backoff was not clamped");
    SameVec(result.normal, {-1.0f, 0.0f, 0.0f}, "Normal did not flip against travel");
    result = Trace(OnePlane(true, Plane({2.0f, 0.0f, 0.0f})),
        {10.0f, 0.0f, 0.0f}, {-10.0f, 0.0f, 0.0f});
    SameVec(result.normal, {2.0f, 0.0f, 0.0f}, "Authored plane normal was silently normalized");
    SameVec(result.location, {0.5f, 0.0f, 0.0f}, "Nonunit plane changed geometric backoff");
    result = Trace(OnePlane(true, Plane({0.0f, 0.0f, 1.0f}, 3.0f)),
        {2.0f, 4.0f, 8.0f}, {2.0f, 4.0f, -2.0f});
    Near(result.time, 0.45f, "Z-plane distance or full-segment projection changed");
    SameVec(result.location, {2.0f, 4.0f, 3.5f}, "Z-plane backed-off location");
    SameVec(result.normal, {0.0f, 0.0f, 1.0f}, "Z-plane normal");
    result = Trace(OnePlane(), {0.1f, 0.0f, 0.0f}, {-0.1f, 0.0f, 0.0f});
    Require(!result.clear && result.time == 0.0f, "Short-segment backoff did not clamp negative time");
    SameVec(result.location, {0.1f, 0.0f, 0.0f}, "Short-segment clamped location");

    // x=0 partitions space but is NOT solid. The near side contains x=5
    // (or x=-5 when reversed), and must win before the far side is queried.
    auto ordered = OnePlane(true, Plane({1.0f, 0.0f, 0.0f}, 0.0f, 0u));
    ordered.nodes[0].front = 1; ordered.nodes[0].back = 2;
    ordered.nodes.push_back(Plane({1.0f, 0.0f, 0.0f}, 5.0f));
    ordered.nodes.push_back(Plane({-1.0f, 0.0f, 0.0f}, 5.0f));
    ordered.nodes[1].surface = 701; ordered.nodes[2].surface = 702;
    result = Trace(ordered, {10.0f, 0.0f, 0.0f}, {-10.0f, 0.0f, 0.0f});
    Require(!result.clear && result.node == 1, "Far leaf overrode first near-front blocked node");
    Near(result.time, 0.225f, "Nested near-front hit used split-local time instead of full segment");
    SameVec(result.location, {5.5f, 0.0f, 0.0f}, "Nested near-front hit backoff");
    result = Trace(ordered, {-10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
    Require(!result.clear && result.node == 2, "Far leaf overrode first near-back blocked node");
    Near(result.time, 0.225f, "Nested near-back hit used split-local time");
    SameVec(result.location, {-5.5f, 0.0f, 0.0f}, "Nested near-back hit backoff");
    SameVec(result.normal, {-1.0f, 0.0f, 0.0f}, "Nested near-back normal");
    ordered.nodes[0].front = -1;
    ordered.nodes[2] = Plane({1.0f, 0.0f, 0.0f}, -5.0f);
    result = Trace(ordered, {10.0f, 0.0f, 0.0f}, {-10.0f, 0.0f, 0.0f});
    Require(!result.clear && result.node == 2, "Clear near leaf prevented far blocked traversal");
    Near(result.time, 0.725f, "Far hit used split-local time instead of full segment");
    SameVec(result.location, {-4.5f, 0.0f, 0.0f}, "Far hit backoff");

    // Native LastNode is updated by crossing/far, NOT same-side descent.
    // Starting inside a deep leaf without a split still reports initial0.
    auto inside = OnePlane(false, Plane({1.0f, 0.0f, 0.0f}, 0.0f, 0u));
    inside.nodes[0].back = 1; inside.nodes.push_back(Plane({0.0f, 1.0f, 0.0f}));
    result = Trace(inside, {-2.0f, -2.0f, 0.0f}, {-3.0f, -2.0f, 0.0f});
    Require(!result.clear && result.node == 0 && result.time == 0.0f,
        "Same-side descent incorrectly replaced original LastNode0 with deepest node");
    SameVec(result.normal, {1.0f, 0.0f, 0.0f}, "Deep start-solid hit did not use LastNode plane");
    Require(Trace(OnePlane(false), {2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}).clear,
        "RootOutside0 prevented CSG front from becoming outside");
}

void GraphAndLimits() {
    // Longer than an ordinary recursive native-call budget. Every front
    // branch follows the next nonsolid node; validation and query must both
    // remain iterative. Each DFS frame has front/back/exit work in addition
    // to one plane-validation step, and the query ends at one outside leaf.
    constexpr std::size_t depth = 8192u;
    Model deep; deep.rootOutside = 1;
    deep.nodes.assign(depth, Plane({1.0f, 0.0f, 0.0f}, 0.0f, 0u));
    for (std::size_t i = 0u; i + 1u < depth; ++i)
        deep.nodes[i].front = static_cast<std::int32_t>(i + 1u);
    const auto deepResult = Trace(deep, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f});
    Require(deepResult.clear && deepResult.stats.nodeVisits == depth && deepResult.stats.leafVisits == 1u &&
        deepResult.stats.querySteps == depth + 1u && deepResult.stats.validationSteps == depth * 4u,
        "Deep acyclic BSP lost stack-safe iterative traversal or exact per-node work accounting");
    UntouchedHit(deepResult, "Deep clear chain invented a hit");

    auto shared = OnePlane(true, Plane({1.0f, 0.0f, 0.0f}, 0.0f, 0u));
    shared.nodes[0].front = shared.nodes[0].back = 1;
    shared.nodes.push_back(Plane({0.0f, 1.0f, 0.0f}, 0.0f, 0u));
    auto result = Trace(shared, {10.0f, 2.0f, 0.0f}, {-10.0f, 2.0f, 0.0f});
    Require(result.clear && result.stats.nodeVisits == 3u && result.stats.leafVisits == 2u &&
        result.stats.querySteps == 5u && result.stats.validationSteps == 8u,
        "Shared child was rejected/deduplicated instead of visited per segment");
    Limits limits; limits.querySteps = 5u;
    Require(Trace(shared, {10.0f, 2.0f, 0.0f}, {-10.0f, 2.0f, 0.0f}, limits).clear,
        "Exact shared traversal work budget was rejected");
    limits.querySteps = 4u;
    Refused([&] { static_cast<void>(Trace(shared, {10.0f, 2.0f, 0.0f}, {-10.0f, 2.0f, 0.0f}, limits)); },
        "Query budget renewed for a pending/shared branch");
    // Two plane records + front/back/exit on each of two DFS frames =8.
    limits = {}; limits.validationSteps = 8u;
    Require(Trace(shared, {10.0f, 2.0f, 0.0f}, {-10.0f, 2.0f, 0.0f}, limits).clear,
        "Exact measured validation boundary was rejected");
    --limits.validationSteps;
    Refused([&] { static_cast<void>(Trace(shared, {}, {1.0f, 0.0f, 0.0f}, limits)); },
        "Validation budget exhausted only after copying/querying unchecked graph");

    auto unreachable = OnePlane(true, Plane({1.0f, 0.0f, 0.0f}, 0.0f, 0u));
    unreachable.nodes.push_back(Plane()); unreachable.nodes[1].front = 1;
    Refused([&] { static_cast<void>(Trace(unreachable, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f})); },
        "Unreachable BSP self-cycle escaped prevalidation");
    unreachable.nodes[1].front = 2; unreachable.nodes.push_back(Plane()); unreachable.nodes[2].back = 1;
    Refused([&] { static_cast<void>(Trace(unreachable, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f})); },
        "Unreachable multi-node BSP cycle escaped prevalidation");
    unreachable.nodes[2].back = -1;
    Require(Trace(unreachable, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}).clear,
        "Valid unreachable BSP component was rejected");
    for (const int child : {-2, 3}) {
        auto invalid = unreachable; invalid.nodes[2].back = child;
        Refused([&] { static_cast<void>(Trace(invalid, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f})); },
            "Malformed unreachable BSP child escaped validation");
    }
    const Limits maximum;
    for (const auto& member : {&Limits::nodes, &Limits::validationSteps, &Limits::querySteps}) {
        for (const bool excessive : {false, true}) {
            limits = {}; limits.*member = excessive ? maximum.*member + 1u : 0u;
            Refused([&] { static_cast<void>(Trace(shared, {}, {1.0f, 0.0f, 0.0f}, limits)); },
                "Zero/excessive point-trace bound was accepted");
        }
    }
    limits = {}; limits.nodes = 1u;
    Refused([&] { static_cast<void>(Trace(shared, {}, {1.0f, 0.0f, 0.0f}, limits)); },
        "Node-array bound was ignored");
    for (const int outside : {-1, 2}) {
        auto invalid = shared; invalid.rootOutside = outside;
        Refused([&] { static_cast<void>(Trace(invalid, {}, {1.0f, 0.0f, 0.0f})); }, "Invalid RootOutside was accepted");
    }
    auto invalid = shared; invalid.version = 67u;
    Refused([&] { static_cast<void>(Trace(invalid, {}, {1.0f, 0.0f, 0.0f})); }, "Wrong Model version was accepted");
    invalid = shared; invalid.licenseeMode = 1u;
    Refused([&] { static_cast<void>(Trace(invalid, {}, {1.0f, 0.0f, 0.0f})); }, "Wrong Model licensee mode was accepted");
}

void FiniteAndDegenerate() {
    const auto model = OnePlane();
    for (const float value : {std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        for (unsigned axis = 0u; axis < 3u; ++axis) {
            Vec point{}; if (axis == 0u) point.x = value; else if (axis == 1u) point.y = value; else point.z = value;
            Refused([&] { static_cast<void>(Trace(model, point, {})); }, "Nonfinite segment start was accepted");
            Refused([&] { static_cast<void>(Trace(model, {}, point)); }, "Nonfinite segment end was accepted");
        }
        for (unsigned field = 0u; field < 4u; ++field) {
            auto invalid = model; invalid.nodes.push_back(Plane());
            if (field == 0u) invalid.nodes[1].planeX = value;
            else if (field == 1u) invalid.nodes[1].planeY = value;
            else if (field == 2u) invalid.nodes[1].planeZ = value;
            else invalid.nodes[1].planeW = value;
            Refused([&] { static_cast<void>(Trace(invalid, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f})); },
                "Nonfinite unreachable plane escaped validation");
        }
    }
    const auto clearPoint = Trace(model, {1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f});
    Require(clearPoint.clear, "Clear zero-length segment was rejected");
    UntouchedHit(clearPoint, "Clear zero-length segment invented hit");
    Refused([&] { static_cast<void>(Trace(model, {-1.0f, 2.0f, 3.0f}, {-1.0f, 2.0f, 3.0f})); },
        "Blocked zero-length segment fabricated finite hit time");
    Refused([&] { static_cast<void>(Trace(model, {1.0e20f, 0.0f, 0.0f}, {-1.0e20f, 0.0f, 0.0f})); },
        "FLOAT squared-length overflow was hidden by double storage");
    auto hugePlane = model; hugePlane.nodes[0].planeX = std::numeric_limits<float>::max();
    Refused([&] { static_cast<void>(Trace(hugePlane, {2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f})); },
        "Finite inputs produced unchecked plane-distance overflow");
    Require(Trace(OnePlane(true, Plane({}, 0.0f)), {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}).clear,
        "Literal finite zero normal was rejected/normalized instead of using original distance tests");
}
} // namespace

int main() {
    try {
        EmptyAndCsg(); ThresholdAndFrontPrecedence(); CrossingAndNormals(); GraphAndLimits(); FiniteAndDegenerate();
        std::cout << "Original model point trace: " << checks << " checks, " << refusals
            << " rejection controls; flags6/null-owner/zero-extent BSP, double threshold precedence, ordered leaf/node identity,"
               " full-segment half-unit backoff, native LastNode, normal facing, complete graph/work/finite bounds."
               " No actor/mover sweep, native decal attachment or campaign startup claim.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Original model point trace failed: " << error.what() << '\n'; return 1;
    }
}
