#include "quest_spawn_placement.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <string>

namespace {
using namespace QuestVr;
std::size_t checks{}, rejections{};
void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Reject(const std::function<void()>& call, const std::string& message) {
    bool rejected{};
    try { call(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, message);
    ++rejections;
}
bool Same(const PortableModelVec3& a, const PortableModelVec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
std::int32_t Bits(float value) {
    std::int32_t word;
    std::memcpy(&word, &value, sizeof(word));
    return word;
}
PortableModelNode Node() {
    PortableModelNode node;
    node.front = node.back = node.plane = node.collisionBound = -1;
    return node;
}
void Bounds(std::vector<std::int32_t>& hull, const PortableModelVec3& minimum,
            const PortableModelVec3& maximum) {
    hull.push_back(-1);
    for (const float value : {minimum.x, minimum.y, minimum.z, maximum.x, maximum.y, maximum.z})
        hull.push_back(Bits(value));
}
struct Fixture {
    PortableModelGeometry model;
    struct Box { PortableModelVec3 minimum, maximum; };
    std::vector<Box> boxes;
    std::vector<std::size_t> hullNodes;
    Fixture() { model.nodes.push_back(Node()); }
    std::size_t BoxHull(const PortableModelVec3& minimum, const PortableModelVec3& maximum) {
        const std::size_t hullNode = model.nodes.size();
        model.nodes.push_back(Node());
        model.nodes[hullNode].collisionBound = static_cast<std::int32_t>(model.leafHulls.size());
        const float low[]{minimum.x, minimum.y, minimum.z};
        const float high[]{maximum.x, maximum.y, maximum.z};
        for (unsigned axis = 0; axis < 3; ++axis) for (const bool flip : {false, true}) {
            auto plane = Node();
            if (axis == 0u) plane.planeX = 1.0f;
            if (axis == 1u) plane.planeY = 1.0f;
            if (axis == 2u) plane.planeZ = 1.0f;
            plane.planeW = flip ? low[axis] : high[axis];
            const auto index = static_cast<std::uint32_t>(model.nodes.size());
            model.nodes.push_back(plane);
            model.leafHulls.push_back(static_cast<std::int32_t>(index | (flip ? 0x4000'0000u : 0u)));
        }
        Bounds(model.leafHulls, minimum, maximum);
        if (hullNodes.empty()) model.nodes[0].front = static_cast<std::int32_t>(hullNode);
        else model.nodes[hullNodes.back()].front = static_cast<std::int32_t>(hullNode);
        hullNodes.push_back(hullNode);
        boxes.push_back({minimum, maximum});
        return hullNode;
    }
    bool BoxOracle(const PortableModelVec3& center, const PortableModelVec3& extent) const {
        // Independent axis-aligned solid oracle, not the helper's plane/BSP code.
        for (const auto& box : boxes) {
            if (center.x - extent.x <= box.maximum.x && center.x + extent.x >= box.minimum.x &&
                center.y - extent.y <= box.maximum.y && center.y + extent.y >= box.minimum.y &&
                center.z - extent.z <= box.maximum.z && center.z + extent.z >= box.minimum.z)
                return true;
        }
        return false;
    }
};
Fixture Gap(const PortableModelVec3& desired, float radius, float height) {
    Fixture fixture;
    const float c[]{desired.x, desired.y, desired.z};
    const float extent[]{radius, radius, height};
    for (unsigned axis = 0; axis < 3; ++axis) for (const bool positive : {false, true}) {
        PortableModelVec3 minimum{-100.0f, -100.0f, -100.0f};
        PortableModelVec3 maximum{100.0f, 100.0f, 100.0f};
        const float boundary = c[axis] + (positive ? 1.0f : -1.0f) * (extent[axis] + 0.125f);
        if (axis == 0u) (positive ? minimum.x : maximum.x) = boundary;
        if (axis == 1u) (positive ? minimum.y : maximum.y) = boundary;
        if (axis == 2u) (positive ? minimum.z : maximum.z) = boundary;
        fixture.BoxHull(minimum, maximum);
    }
    return fixture;
}
void CandidateOrderAndDefaults() {
    const int offsets[]{0, 1, -1};
    for (const auto dimensions : {PortableModelVec3{1.0f, 1.0f, 1.0f},
                                  PortableModelVec3{0.25f, 0.25f, 2.0f},
                                  PortableModelVec3{2.0f, 2.0f, 0.25f}}) {
        const PortableModelVec3 original{7.0f, -3.0f, 5.0f};
        const float scale = std::max(dimensions.x, dimensions.z);
        std::size_t ordinal{};
        for (const int z : offsets) for (const int y : offsets) for (const int x : offsets) {
            const PortableModelVec3 desired{original.x + x * scale,
                                            original.y + y * scale,
                                            original.z + z * scale};
            auto fixture = Gap(desired, dimensions.x, dimensions.z);
            // Six axis half-boxes create one feasible candidate and block all
            // other grid points. This independently checks every one of the 27
            // positions, nesting, shift signs, dimension order and first match.
            for (const auto& flags : {std::pair<bool, bool>{true, false}, {false, true}, {true, true}}) {
                const auto result = FindSpawnPlacement(fixture.model, original, dimensions.x,
                                                       dimensions.z, flags.first, flags.second);
                Require(result.found && Same(result.location, desired), "Incorrect placement candidate");
                Require(result.probes == ordinal + 1u, "27-probe ordering or first-match behavior changed");
                Require(result.stats.validationSteps > 0u && result.stats.nodeVisits > 0u,
                        "World query was not actually performed");
            }
            ++ordinal;
        }
        Require(ordinal == 27u, "Fixture did not check every candidate");
    }
    Fixture solid;
    solid.BoxHull({-100.0f, -100.0f, -100.0f}, {100.0f, 100.0f, 100.0f});
    const PortableModelVec3 original{7.0f, -3.0f, 5.0f};
    const auto failed = FindSpawnPlacement(solid.model, original, 1.0f, 2.0f, true, false);
    Require(!failed.found && failed.probes == 27u && Same(failed.location, original),
            "Exhaustion did not preserve requested location and report real failure");
    // No active collision flag and zero extents intentionally never touch even
    // an invalid/empty Model, matching CheckLocation and OverlapTester.
    PortableModelGeometry empty;
    empty.version = 999u;
    const auto bypass = FindSpawnPlacement(empty, original, 2.0f, 1.0f, false, false);
    Require(bypass.found && Same(bypass.location, original) && bypass.probes == 0u &&
            bypass.stats.validationSteps == 0u, "No-collision flags did not bypass world query");
    const auto zero = FindSpawnPlacement(empty, original, -0.0f, 0.0f, true, true);
    Require(zero.found && Same(zero.location, original) && zero.probes == 1u &&
            zero.stats.validationSteps == 0u && zero.stats.nodeVisits == 0u,
            "Zero extents did not preserve the pinned first-candidate/no-BSP path");
    Require(!ProbeSpawnWorldOverlap(empty, original, {}), "Zero-extents world probe overlapped");
    Reject([&] { FindSpawnPlacement(empty, original, 0.0f, 0.25f, true, false); },
            "Nonzero height on empty Model was silently considered clear");
    Fixture clear;
    const auto clearResult = FindSpawnPlacement(clear.model, original, 1.0f, 1.0f, true, false);
    Require(clearResult.found && clearResult.probes == 1u, "BSP without collision hulls was blocked");
}
void HullGeometryAndContacts() {
    Fixture fixture;
    fixture.BoxHull({-2.0f, -1.0f, -3.0f}, {2.0f, 1.0f, 3.0f});
    for (int z = -8; z <= 8; ++z) for (int y = -6; y <= 6; ++y) for (int x = -6; x <= 6; ++x) {
        const PortableModelVec3 center{x * 0.5f, y * 0.5f, z * 0.5f};
        for (const auto extent : {PortableModelVec3{0.25f, 0.5f, 0.75f},
                                  PortableModelVec3{0.0f, 0.0f, 0.5f},
                                  PortableModelVec3{0.5f, 0.5f, 0.0f}}) {
            Require(ProbeSpawnWorldOverlap(fixture.model, center, extent) == fixture.BoxOracle(center, extent),
                    "Convex hull differs from independent box oracle");
        }
    }
    Require(ProbeSpawnWorldOverlap(fixture.model, {2.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}),
            "Exact bbox/plane contact was incorrectly considered free");
    Require(!ProbeSpawnWorldOverlap(fixture.model, {2.75f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}),
            "Separated bbox incorrectly overlapped");
    // Slanted authored plane: its projected AABB extent is 1.5, not the
    // cylinder/sphere distance or one selected radius component.
    PortableModelGeometry slanted;
    auto plane = Node(); plane.planeX = 1.0f; plane.planeY = 1.0f;
    plane.planeZ = -1.0f; plane.planeW = 2.0f; plane.collisionBound = 0;
    slanted.nodes.push_back(plane);
    slanted.leafHulls.push_back(0);
    Bounds(slanted.leafHulls, {-100.0f, -100.0f, -100.0f}, {100.0f, 100.0f, 100.0f});
    Require(ProbeSpawnWorldOverlap(slanted, {3.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}),
            "Slanted plane boundary was missed");
    Require(!ProbeSpawnWorldOverlap(slanted, {3.75f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}),
            "Slanted plane outside side was not clipped");
    slanted.leafHulls[0] = 0x4000'0000;
    Require(ProbeSpawnWorldOverlap(slanted, {0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}),
            "Flip-bit plane boundary was missed");
    Require(!ProbeSpawnWorldOverlap(slanted, {0.25f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}),
            "Flip-bit plane outside side was not clipped");
    // Literal coefficient scaling remains equivalent without normalizing.
    for (const float scale : {0.5f, 2.0f, 16.0f}) {
        slanted.nodes[0].planeX = scale; slanted.nodes[0].planeY = scale;
        slanted.nodes[0].planeZ = -scale; slanted.nodes[0].planeW = 2.0f * scale;
        Require(ProbeSpawnWorldOverlap(slanted, {0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}),
                "Non-unit authored plane changed contact");
    }
    // Empty plane lists still produce a bbox-only hit in the pin.
    PortableModelGeometry boxOnly;
    auto boxNode = Node(); boxNode.collisionBound = 0;
    boxOnly.nodes.push_back(boxNode);
    Bounds(boxOnly.leafHulls, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f});
    for (const std::int32_t terminator : {-1, -2, std::numeric_limits<std::int32_t>::min()}) {
        boxOnly.leafHulls[0] = terminator;
        Require(ProbeSpawnWorldOverlap(boxOnly, {}, {0.5f, 0.5f, 0.5f}),
                "Valid negative terminator or empty plane list was rejected");
    }
    // Surface/mover/vertex metadata is not part of this world-hull query.
    fixture.model.nodes[0].plane = 123456;
    fixture.model.nodes[0].surface = -999;
    fixture.model.nodes[0].nodeFlags = 255u;
    fixture.model.rootOutside = 123456;
    fixture.model.points.push_back({std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f});
    Require(ProbeSpawnWorldOverlap(fixture.model, {}, {0.5f, 0.5f, 0.5f}),
            "Unrelated rendered geometry/flags entered world placement");
}
void BspTraversal() {
    Fixture fixture;
    const auto front = fixture.BoxHull({2.5f, -1.0f, -1.0f}, {3.5f, 1.0f, 1.0f});
    const auto back = fixture.BoxHull({-3.5f, -1.0f, -1.0f}, {-2.5f, 1.0f, 1.0f});
    fixture.model.nodes[front].front = -1;
    fixture.model.nodes[0].planeX = 1.0f;
    fixture.model.nodes[0].front = static_cast<std::int32_t>(front);
    fixture.model.nodes[0].back = static_cast<std::int32_t>(back);
    for (const float x : {-3.0f, 3.0f})
        Require(ProbeSpawnWorldOverlap(fixture.model, {x, 0.0f, 0.0f}, {0.25f, 0.25f, 0.25f}),
                "BSP side sign selected the wrong branch");
    Fixture padded;
    const auto hull = padded.BoxHull({1.0f, -1.0f, -1.0f}, {1.1f, 1.0f, 1.0f});
    padded.model.nodes[0].planeX = 1.0f;
    padded.model.nodes[0].front = -1;
    padded.model.nodes[0].back = static_cast<std::int32_t>(hull);
    Require(ProbeSpawnWorldOverlap(padded.model, {1.05f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}),
            "1.1 extent padding was omitted from BSP traversal");
    SpawnPlacementStats stats;
    Require(!ProbeSpawnWorldOverlap(padded.model, {1.2f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, &stats) &&
            stats.nodeVisits == 1u, "BSP far-side pruning was not preserved");
    Fixture order;
    const auto direct = order.BoxHull({-0.25f, -0.25f, -0.25f}, {0.25f, 0.25f, 0.25f});
    const auto indirect = order.BoxHull({-0.25f, -0.25f, -0.25f}, {0.25f, 0.25f, 0.25f});
    order.model.nodes[direct].front = -1;
    const auto bridge = order.model.nodes.size();
    auto bridgeNode = Node(); bridgeNode.front = static_cast<std::int32_t>(indirect);
    order.model.nodes.push_back(bridgeNode);
    order.model.nodes[0].back = static_cast<std::int32_t>(bridge);
    Require(ProbeSpawnWorldOverlap(order.model, {}, {1.0f, 1.0f, 1.0f}, &stats) && stats.nodeVisits == 2u,
            "Front recursion was not evaluated before back recursion");
    // Shared descendants are legal; only actual ancestor cycles are rejected.
    order.model.nodes[0].front = order.model.nodes[0].back = static_cast<std::int32_t>(bridge);
    Require(ProbeSpawnWorldOverlap(order.model, {}, {1.0f, 1.0f, 1.0f}), "Shared BSP child was mistaken for a cycle");
    PortableModelGeometry chain;
    chain.nodes.assign(20'000u, Node());
    for (std::size_t i = 0u; i + 1u < chain.nodes.size(); ++i)
        chain.nodes[i].front = static_cast<std::int32_t>(i + 1u);
    Require(!ProbeSpawnWorldOverlap(chain, {}, {0.25f, 0.25f, 0.25f}),
            "Deep valid BSP required recursive C++ stack or produced a hit");
}
void MalformedAndBounds() {
    Fixture valid;
    valid.BoxHull({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f});
    const auto probe = [&](const PortableModelGeometry& model) {
        return ProbeSpawnWorldOverlap(model, {}, {0.25f, 0.25f, 0.25f});
    };
    for (const auto value : {std::numeric_limits<float>::infinity(),
                             -std::numeric_limits<float>::infinity(),
                             std::numeric_limits<float>::quiet_NaN()}) {
        for (unsigned field = 0; field < 4u; ++field) {
            auto model = valid.model;
            auto& node = model.nodes.back();
            if (field == 0u) node.planeX = value;
            if (field == 1u) node.planeY = value;
            if (field == 2u) node.planeZ = value;
            if (field == 3u) node.planeW = value;
            Reject([&] { probe(model); }, "Nonfinite plane was hidden by early root hit");
        }
        for (unsigned field = 0; field < 6u; ++field) {
            auto model = valid.model;
            model.leafHulls[7u + field] = Bits(value);
            Reject([&] { probe(model); }, "Nonfinite packed bbox was accepted");
        }
        Reject([&] { FindSpawnPlacement(valid.model, {value, 0.0f, 0.0f}, 1.0f, 1.0f, false, false); },
                "Nonfinite bypass location accepted");
        Reject([&] { FindSpawnPlacement(valid.model, {}, value, 1.0f, false, false); },
                "Nonfinite default radius accepted");
        Reject([&] { FindSpawnPlacement(valid.model, {}, 1.0f, value, true, false); },
                "Nonfinite default height accepted");
    }
    Reject([&] { FindSpawnPlacement(valid.model, {}, -1.0f, 1.0f, false, false); }, "Negative radius accepted");
    Reject([&] { FindSpawnPlacement(valid.model, {}, 1.0f, -1.0f, true, false); }, "Negative height accepted");
    for (const std::int32_t index : {-2, std::numeric_limits<std::int32_t>::min(),
                                    static_cast<std::int32_t>(valid.model.nodes.size()),
                                    std::numeric_limits<std::int32_t>::max()}) {
        for (const bool front : {false, true}) {
            auto model = valid.model;
            (front ? model.nodes.back().front : model.nodes.back().back) = index;
            Reject([&] { probe(model); }, "Invalid child index was hidden by pruning/early hit");
        }
    }
    for (const std::int32_t start : {-2, std::numeric_limits<std::int32_t>::min(),
                                    static_cast<std::int32_t>(valid.model.leafHulls.size()),
                                    std::numeric_limits<std::int32_t>::max()}) {
        auto model = valid.model; model.nodes[1].collisionBound = start;
        Reject([&] { probe(model); }, "Invalid hull offset accepted");
    }
    for (const std::int32_t index : {static_cast<std::int32_t>(valid.model.nodes.size()),
                                    static_cast<std::int32_t>(0x4000'0000u | valid.model.nodes.size()),
                                    std::numeric_limits<std::int32_t>::max()}) {
        auto model = valid.model; model.leafHulls[0] = index;
        Reject([&] { probe(model); }, "Invalid plane index/flip-bit index accepted");
    }
    for (std::size_t length = 0u; length < valid.model.leafHulls.size(); ++length) {
        auto model = valid.model; model.leafHulls.resize(length);
        Reject([&] { probe(model); }, "Truncated plane list/terminator/bbox accepted");
    }
    for (unsigned axis = 0; axis < 3u; ++axis) {
        auto model = valid.model; model.leafHulls[7u + axis] = Bits(2.0f);
        Reject([&] { probe(model); }, "Reversed bbox accepted");
    }
    auto missing = valid.model;
    missing.leafHulls.assign(12u, 0);
    Reject([&] { probe(missing); }, "Hull with no negative terminator accepted");
    auto cycle = valid.model;
    cycle.nodes[0].front = 0;
    Reject([&] { probe(cycle); }, "Self-cycle accepted");
    cycle = valid.model;
    cycle.nodes[1].back = 0;
    Reject([&] { probe(cycle); }, "Parent back-edge hidden by hit accepted");
    cycle = valid.model;
    cycle.nodes.back().front = static_cast<std::int32_t>(cycle.nodes.size() - 1u);
    Reject([&] { probe(cycle); }, "Unreachable cycle accepted");
    auto layout = valid.model; layout.version = 67u;
    Reject([&] { probe(layout); }, "Wrong version accepted");
    layout = valid.model; layout.licenseeMode = 1u;
    Reject([&] { probe(layout); }, "Wrong licensee layout accepted");
    PortableModelGeometry empty;
    Reject([&] { probe(empty); }, "Empty nonzero-extent BSP silently passed");
    for (unsigned field = 0; field < 4u; ++field) for (const bool oversize : {false, true}) {
        SpawnPlacementLimits limits;
        auto& value = field == 0u ? limits.nodes : field == 1u ? limits.hullWords :
                      field == 2u ? limits.validationSteps : limits.querySteps;
        value = oversize ? value + 1u : 0u;
        Reject([&] { ProbeSpawnWorldOverlap(valid.model, {}, {1.0f, 1.0f, 1.0f}, nullptr, limits); },
                "Invalid/increased limits accepted");
    }
    SpawnPlacementLimits limits;
    limits.nodes = valid.model.nodes.size() - 1u;
    Reject([&] { ProbeSpawnWorldOverlap(valid.model, {}, {1.0f, 1.0f, 1.0f}, nullptr, limits); },
            "Node budget was ignored");
    limits = {}; limits.hullWords = valid.model.leafHulls.size() - 1u;
    Reject([&] { ProbeSpawnWorldOverlap(valid.model, {}, {1.0f, 1.0f, 1.0f}, nullptr, limits); },
            "Hull-word budget was ignored");
    limits = {}; limits.validationSteps = 1u;
    Reject([&] { ProbeSpawnWorldOverlap(valid.model, {}, {1.0f, 1.0f, 1.0f}, nullptr, limits); },
            "Validation work budget was ignored");
    limits = {}; limits.querySteps = 1u;
    Reject([&] { ProbeSpawnWorldOverlap(valid.model, {}, {1.0f, 1.0f, 1.0f}, nullptr, limits); },
            "Query work budget was ignored");
    // Shared hull scans cannot amplify a bounded array into unbounded work.
    auto reused = valid.model;
    for (unsigned i = 0; i < 50u; ++i) {
        auto node = Node(); node.collisionBound = 0;
        reused.nodes.push_back(node);
    }
    limits = {}; limits.validationSteps = 100u;
    Reject([&] { ProbeSpawnWorldOverlap(reused, {}, {1.0f, 1.0f, 1.0f}, nullptr, limits); },
            "Repeated hull scans bypassed aggregate validation budget");
    // A valid DAG can cause exponential recursive visits; aggregate query work
    // bounds that too rather than confusing shared children with a cycle.
    PortableModelGeometry dag;
    dag.nodes.assign(30u, Node());
    for (std::size_t i = 0; i + 1u < dag.nodes.size(); ++i)
        dag.nodes[i].front = dag.nodes[i].back = static_cast<std::int32_t>(i + 1u);
    limits = {}; limits.querySteps = 100u;
    Reject([&] { ProbeSpawnWorldOverlap(dag, {}, {1.0f, 1.0f, 1.0f}, nullptr, limits); },
            "Repeated DAG visits bypassed aggregate query budget");
    auto gap = Gap({1.0f, 1.0f, 1.0f}, 1.0f, 1.0f);
    const auto full = FindSpawnPlacement(gap.model, {}, 1.0f, 1.0f, true, false);
    limits = {}; limits.querySteps = full.stats.querySteps - 1u;
    Reject([&] { FindSpawnPlacement(gap.model, {}, 1.0f, 1.0f, true, false, limits); },
            "Work budget was reset for each placement candidate");
    limits.querySteps = full.stats.querySteps;
    Require(FindSpawnPlacement(gap.model, {}, 1.0f, 1.0f, true, false, limits).found,
            "Exact inclusive aggregate work budget rejected valid search");
    const float maximum = std::numeric_limits<float>::max();
    Fixture clear;
    Require(!ProbeSpawnWorldOverlap(clear.model, {maximum, 0.0f, 0.0f}, {0.0f, 1.0f, 1.0f}),
            "Exact positive float-max endpoint was rejected");
    Require(!ProbeSpawnWorldOverlap(clear.model, {-maximum, 0.0f, 0.0f}, {0.0f, 1.0f, 1.0f}),
            "Exact negative float-max endpoint was rejected");
    const float previous = std::nextafter(maximum, 0.0f);
    const float gapToMaximum = maximum - previous;
    Require(!ProbeSpawnWorldOverlap(clear.model, {previous, 0.0f, 0.0f}, {gapToMaximum, 1.0f, 1.0f}),
            "Nonzero-extent exact positive float-max endpoint was rejected");
    Require(!ProbeSpawnWorldOverlap(clear.model, {-previous, 0.0f, 0.0f}, {gapToMaximum, 1.0f, 1.0f}),
            "Nonzero-extent exact negative float-max endpoint was rejected");
    // This is one DOUBLE representable step above float-max, far less than a
    // float ULP. A premature float cast can round it back to finite float-max,
    // concealing that the double endpoint already exceeded the supported range.
    const float endpointStep = std::ldexp(1.0f, 75);
    Require(static_cast<double>(maximum) + endpointStep ==
                std::nextafter(static_cast<double>(maximum), std::numeric_limits<double>::infinity()),
            "Next-above endpoint fixture is not one double step above float-max");
    Require(-static_cast<double>(maximum) - endpointStep ==
                std::nextafter(-static_cast<double>(maximum), -std::numeric_limits<double>::infinity()),
            "Next-below endpoint fixture is not one double step below negative float-max");
    Reject([&] { ProbeSpawnWorldOverlap(clear.model, {maximum, 0.0f, 0.0f}, {endpointStep, 1.0f, 1.0f}); },
            "Double endpoint just above float-max was rounded down and accepted");
    Reject([&] { ProbeSpawnWorldOverlap(clear.model, {-maximum, 0.0f, 0.0f}, {endpointStep, 1.0f, 1.0f}); },
            "Double endpoint just below negative float-max was rounded up and accepted");
    Reject([&] { ProbeSpawnWorldOverlap(clear.model, {maximum, 0.0f, 0.0f}, {maximum, 1.0f, 1.0f}); },
            "Finite-input bbox float overflow silently passed");
    // Force the first candidate to hit without overflowing its bbox, then
    // shifting by max(radius,height) overflows candidate.x on the second probe.
    PortableModelGeometry overflow;
    auto node = Node(); node.collisionBound = 0; overflow.nodes.push_back(node);
    Bounds(overflow.leafHulls, {0.0f, -maximum, -maximum}, {maximum, maximum, maximum});
    Reject([&] { FindSpawnPlacement(overflow, {maximum, 0.0f, 0.0f}, 0.0f, maximum, true, false); },
            "Finite-input float candidate overflow silently passed");
}
}
int main() {
    try {
        CandidateOrderAndDefaults(); HullGeometryAndContacts(); BspTraversal(); MalformedAndBounds();
        std::cout << "PASS: isolated Spawn world BSP/AABB placement candidate: " << checks
                  << " checks, " << rejections << " bounded refusals. No native 278/allocation/startup enabled.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
