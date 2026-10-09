#include "quest_ground_probe.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
struct Vector { float x{}, y{}, z{}; };
struct Triangle { Vector a, b, c, normal; };
std::size_t checks{};

void Require(bool value, const char* description) {
    ++checks;
    if (!value) throw std::runtime_error(description);
}

void Height(const std::optional<float>& actual, float expected, const char* description) {
    Require(actual.has_value() && std::isfinite(*actual) && std::fabs(*actual - expected) <= 0.000002f, description);
}

Triangle Plane(float height) {
    return {{-2.0f, height, -2.0f}, {2.0f, height, -2.0f}, {0.0f, height, 2.0f}, {0.0f, -1.0f, 0.0f}};
}

void ReadinessAndPartialPlaneRegression() {
    for (bool runtime : {false, true}) for (bool pending : {false, true}) for (bool transition : {false, true})
        Require(QuestVr::GroundCollisionReady(runtime, pending, transition) == (runtime && !pending && !transition),
                "partial/unavailable map collision was marked ready");
    // Structural synthetic regression, not a reproduction against an original
    // map: committing a lower plane before the actual floor arrives can put the
    // latter outside the 45cm recovery band. Loading must not commit either.
    constexpr float completeFloor = -0.198f, partialFloor = -0.887f;
    const Vector feet{0.0f, completeFloor, 0.0f};
    QuestVr::GroundProbe partial(feet.y);
    const auto lower = QuestVr::GroundHeightAtXZ(feet, Plane(partialFloor));
    Require(lower.has_value() && partial.Add(*lower), "partial-plane regression fixture was not accepted geometrically");
    Height(partial.Height(), partialFloor, "partial plane probe changed expected height");
    QuestVr::GroundProbe sunk(partialFloor);
    Require(!sunk.Add(completeFloor), "regression fixture failed to exceed step-up recovery bound");
    float committedFloor = completeFloor;
    for (const auto& loading : {std::pair<bool, bool>{true, false}, {false, true}, {true, true}}) {
        if (QuestVr::GroundCollisionReady(true, loading.first, loading.second)) committedFloor = *partial.Height();
        Require(committedFloor == completeFloor, "incomplete upload committed a lower plane and sank feet");
    }
    QuestVr::GroundProbe complete(committedFloor);
    for (const auto plane : {Plane(partialFloor), Plane(completeFloor)}) {
        if (const auto floor = QuestVr::GroundHeightAtXZ(feet, plane)) complete.Add(*floor);
    }
    Require(QuestVr::GroundCollisionReady(true, false, false), "complete map collision was not enabled");
    if (const auto floor = complete.Height()) committedFloor = *floor;
    Require(committedFloor == completeFloor, "complete collision failed to retain the highest authored floor");
}

void GeometrySignsSlopesAndBounds() {
    const Vector point{0.0f, 5.0f, 0.0f};
    Height(QuestVr::GroundHeightAtXZ(point, Plane(-0.198f)), -0.198f,
           "negative normal floor was rejected or restricted by feet height");
    auto plane = Plane(0.5f); plane.normal.y = 1.0f;
    Height(QuestVr::GroundHeightAtXZ(point, plane), 0.5f, "positive normal plane was rejected");
    const Triangle slope{{-2.0f, -1.0f, -2.0f}, {2.0f, 1.0f, -2.0f}, {0.0f, 0.0f, 2.0f}, {-0.4472136f, 0.8944272f, 0.0f}};
    Height(QuestVr::GroundHeightAtXZ(Vector{0.8f, 0.0f, 0.0f}, slope), 0.4f, "sloped floor interpolation was incorrect");
    for (float sign : {-1.0f, 1.0f}) {
        plane.normal.y = sign * QuestVr::GroundMinimumAbsNormalY;
        Require(QuestVr::GroundHeightAtXZ(point, plane).has_value(), "minimum abs-normal boundary was rejected");
        plane.normal.y = sign * std::nextafter(QuestVr::GroundMinimumAbsNormalY, 0.0f);
        Require(!QuestVr::GroundHeightAtXZ(point, plane), "too-steep plane crossed minimum abs-normal bound");
    }
    const Triangle unit{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}};
    for (const auto vertex : {unit.a, unit.b, unit.c})
        Height(QuestVr::GroundHeightAtXZ(vertex, unit), 0.0f, "triangle vertex was excluded");
    Require(QuestVr::GroundHeightAtXZ(Vector{-QuestVr::GroundBarycentricTolerance, 0.0f, 0.5f}, unit).has_value(),
            "inclusive barycentric tolerance was rejected");
    Require(QuestVr::GroundHeightAtXZ(Vector{-0.9f * QuestVr::GroundBarycentricTolerance, 0.0f, -0.9f * QuestVr::GroundBarycentricTolerance}, unit).has_value(),
            "original lower-only barycentric tolerance was tightened at a corner");
    Require(!QuestVr::GroundHeightAtXZ(Vector{-std::nextafter(QuestVr::GroundBarycentricTolerance, 1.0f), 0.0f, 0.5f}, unit),
            "barycentric tolerance accepted a point beyond its boundary");
    Require(!QuestVr::GroundHeightAtXZ(Vector{2.0f, 0.0f, 2.0f}, unit), "outside-triangle point received a floor");
    const Triangle degenerate{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {2.0f, 2.0f, 2.0f}, {0.0f, 1.0f, 0.0f}};
    Require(!QuestVr::GroundHeightAtXZ(point, degenerate), "collinear projected triangle received a floor");
    const Triangle tiny{{0.0f, 0.0f, 0.0f}, {0.0001f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0001f}, {0.0f, 1.0f, 0.0f}};
    Require(!QuestVr::GroundHeightAtXZ(Vector{}, tiny), "near-degenerate denominator bypassed existing threshold");
}

void ProbeStepDropAndMaximum() {
    QuestVr::GroundProbe probe(0.0f);
    Require(!probe.Height(), "empty ground probe invented a floor");
    Require(probe.Add(-QuestVr::GroundMaximumDrop), "inclusive drop boundary was rejected");
    Height(probe.Height(), -2.0f, "first accepted floor was lost");
    Require(probe.Add(0.1f) && probe.Add(0.05f), "valid floor candidate was rejected");
    Height(probe.Height(), 0.1f, "lower candidate replaced highest floor");
    Require(probe.Add(QuestVr::GroundMaximumStepUp), "inclusive step-up boundary was rejected");
    Height(probe.Height(), 0.45f, "step-up boundary floor was lost");
    Require(!probe.Add(std::nextafter(QuestVr::GroundMaximumStepUp, 1.0f)), "floor above step-up bound was accepted");
    Require(!probe.Add(std::nextafter(-QuestVr::GroundMaximumDrop, -3.0f)), "floor below drop bound was accepted");
    Height(probe.Height(), 0.45f, "rejected candidate mutated highest floor");
    QuestVr::GroundProbe elevated(10.0f);
    Require(elevated.Add(10.0f + QuestVr::GroundMaximumStepUp) && elevated.Add(10.0f - QuestVr::GroundMaximumDrop),
            "translated step/drop interval changed original float bounds");
    Height(elevated.Height(), 10.45f, "translated probe selected wrong floor");
}

void NonfiniteGeometryAndProbeFailClosed() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (float invalid : {nan, infinity, -infinity}) {
        for (int component = 0; component < 3; ++component) {
            Vector point{};
            (component == 0 ? point.x : component == 1 ? point.y : point.z) = invalid;
            Require(!QuestVr::GroundHeightAtXZ(point, Plane(0.0f)), "nonfinite point received a ground height");
            for (int member = 0; member < 4; ++member) {
                auto triangle = Plane(0.0f);
                auto& vector = member == 0 ? triangle.a : member == 1 ? triangle.b : member == 2 ? triangle.c : triangle.normal;
                (component == 0 ? vector.x : component == 1 ? vector.y : vector.z) = invalid;
                Require(!QuestVr::GroundHeightAtXZ(Vector{}, triangle), "nonfinite triangle received a ground height");
            }
        }
        QuestVr::GroundProbe invalidFeet(invalid);
        Require(!invalidFeet.Add(0.0f) && !invalidFeet.Height(), "nonfinite feet created probe state");
        QuestVr::GroundProbe probe(0.0f);
        Require(probe.Add(0.1f), "invalid-floor fixture failed");
        Require(!probe.Add(invalid), "nonfinite floor entered bounded probe");
        Height(probe.Height(), 0.1f, "nonfinite floor changed accumulated state");
    }
    auto overflow = Plane(0.0f);
    overflow.a.x = -std::numeric_limits<float>::max(); overflow.b.x = std::numeric_limits<float>::max();
    Require(!QuestVr::GroundHeightAtXZ(Vector{}, overflow), "overflowed geometric denominator returned a height");
    QuestVr::GroundProbe ordinary(0.0f);
    Require(!ordinary.Add(std::numeric_limits<float>::max()) && !ordinary.Height(), "finite extreme floor bypassed step/drop bound");
}
} // namespace

int main() {
    try {
        ReadinessAndPartialPlaneRegression();
        GeometrySignsSlopesAndBounds();
        ProbeStepDropAndMaximum();
        NonfiniteGeometryAndProbeFailClosed();
        std::cout << "PASS: 4 Quest ground-probe regression groups; " << checks
                  << " checks. Partial-upload case is synthetic structural coverage, not an original-map reproduction or full physics validation.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
