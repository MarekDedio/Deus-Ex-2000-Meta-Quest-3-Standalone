#include "vr_world_transform.h"
#include "OVR_Math.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using Vector = OVR::Vector3f;

void Require(bool value, const char* description) {
    if (!value) throw std::runtime_error(description);
}

void RequireNear(const Vector& actual, const Vector& expected,
                 const char* description, float tolerance = 0.0001f) {
    Require((actual - expected).Length() < tolerance, description);
}

OVR::Quatf Yaw(float angle) {
    return OVR::Quatf(Vector(0.0f, 1.0f, 0.0f), angle);
}

void RendererTransformAgreement() {
    for (int i = -240; i <= 240; ++i) {
        const float yaw = static_cast<float>(i) * 0.037f;
        const Vector world{8.7f + static_cast<float>(i) * 0.03f, -1.2f, -10.9f};
        const Vector local{-3.2f, 1.5f, 7.1f};
        const Vector renderedStage = Yaw(yaw).Rotate(local) + world;
        RequireNear(QuestVr::StageToLocal(renderedStage, world, yaw), local,
                    "map transform disagrees with Meta renderer rotation");
    }
}

void SpawnAtTrackingHead() {
    for (const Vector head : {Vector{0.0f, 1.7f, 0.0f}, Vector{4.2f, 1.7f, -7.8f},
                              Vector{-5.8f, 1.1f, 9.6f}}) {
        Vector world{0.0f, -0.4f, 0.0f};
        QuestVr::AnchorSpawnToHead(head, world);
        const Vector local = QuestVr::StageToLocal(head, world, 0.0f);
        RequireNear(local, Vector{0.0f, head.y + 0.4f, 0.0f},
                    "nonzero tracking origin displaced map spawn");
    }
}

void SnapAwayFromOrigin() {
    const Vector head{3.4f, 1.67f, -2.9f};
    Vector world{-8.0f, -0.8f, 4.2f};
    float yaw = 0.42f;
    const Vector originalLocal = QuestVr::StageToLocal(head, world, yaw);
    for (int turn = 0; turn < 1200; ++turn) {
        QuestVr::SnapTurnAroundHead(QuestVr::Pi / 6.0f, head, world, yaw);
        world = QuestVr::LocomotionCandidate(world, Vector{1.0f, 0.0f, 0.0f},
                                             0.0f, 0.0f, 2.2f, 1.0f / 72.0f);
        RequireNear(QuestVr::StageToLocal(head, world, yaw), originalLocal,
                    "snap-turn movement candidate overwrote headset pivot", 0.002f);
        Require(std::fabs(yaw) <= QuestVr::Pi + 0.0001f,
                "repeated snap turns accumulated an unbounded yaw");
    }
    QuestVr::SnapTurnAroundHead(-QuestVr::Pi / 6.0f, head, world, yaw);
    RequireNear(QuestVr::StageToLocal(head, world, yaw), originalLocal,
                "left snap turn moved headset pivot", 0.002f);
}

void TurnAndMoveTogether() {
    const Vector head{3.4f, 1.7f, -2.9f};
    for (float worldYaw : {-2.1f, 0.0f, 1.7f}) {
        for (float headYaw : {-2.5f, -0.6f, 0.0f, 0.9f, 2.8f}) {
            Vector world{-6.0f, -0.7f, 9.2f};
            float yaw = worldYaw;
            const Vector originalLocal = QuestVr::StageToLocal(head, world, yaw);
            QuestVr::SnapTurnAroundHead(QuestVr::Pi / 6.0f, head, world, yaw);
            const Vector forward = Yaw(headYaw).Rotate(Vector{0.0f, 0.0f, -1.0f});
            const Vector right = Yaw(headYaw).Rotate(Vector{1.0f, 0.0f, 0.0f});
            const Vector stageRight = QuestVr::HorizontalHeadRight(forward, right);
            const float dt = 1.0f / 72.0f;
            const Vector intendedStep = (right * 0.6f + forward * 0.8f) * (2.2f * dt);
            const Vector movedWorld = QuestVr::LocomotionCandidate(
                world, stageRight, 0.6f, 0.8f, 2.2f, dt);
            const Vector expected = originalLocal + Yaw(yaw).Inverted().Rotate(intendedStep);
            RequireNear(QuestVr::StageToLocal(head, movedWorld, yaw), expected,
                        "turning and walking mixed map-space and tracking-space movement");
        }
    }
}

void TurnDirectionAndAudioBasis() {
    const Vector head{5.0f, 1.7f, -4.0f};
    for (float direction : {-1.0f, 1.0f}) {
        Vector world{1.0f, 0.0f, 3.0f};
        float yaw{};
        QuestVr::SnapTurnAroundHead(direction * QuestVr::Pi / 6.0f, head, world, yaw);
        const Vector mapForward = QuestVr::StageDirectionToLocal(Vector{0.0f, 0.0f, -1.0f}, yaw);
        Require(mapForward.x * direction > 0.0f,
                "right stick rotated the map-space view left or vice versa");
        const Vector mapRight = QuestVr::StageDirectionToLocal(Vector{1.0f, 0.0f, 0.0f}, yaw);
        RequireNear(mapRight, Yaw(yaw).Inverted().Rotate(Vector{1.0f, 0.0f, 0.0f}),
                    "spatial audio right axis disagreed with rendered headset view");
    }
}

void RoomScaleCollisionCompensation() {
    const Vector previous{2.1f, 1.7f, -3.5f};
    const Vector current{2.3f, 1.72f, -3.1f};
    const Vector world{-8.2f, -0.5f, 2.1f};
    for (float yaw : {-2.0f, 0.0f, 0.9f, 2.5f}) {
        const Vector before = QuestVr::StageToLocal(previous, world, yaw);
        const Vector compensated = QuestVr::RestoreHorizontalHeadPosition(
            world, current, before, yaw);
        const Vector after = QuestVr::StageToLocal(current, compensated, yaw);
        RequireNear(Vector{after.x, 0.0f, after.z}, Vector{before.x, 0.0f, before.z},
                    "blocked physical head movement moved through the wall");
        Require(std::fabs(after.y - before.y - 0.02f) < 0.0001f,
                "collision compensation suppressed real headset height changes");
    }
}

void RoomScaleCollisionWhileTurning() {
    const Vector previous{2.1f, 1.7f, -3.5f};
    const Vector current{2.3f, 1.72f, -3.1f};
    for (float oldYaw : {-1.9f, 0.0f, 2.5f}) {
        for (float turn : {-QuestVr::Pi / 6.0f, QuestVr::Pi / 6.0f}) {
            Vector world{-8.2f, -0.5f, 2.1f};
            float yaw = oldYaw;
            const Vector safe = QuestVr::StageToLocal(previous, world, yaw);
            QuestVr::SnapTurnAroundHead(turn, current, world, yaw);
            world = QuestVr::RestoreHorizontalHeadPosition(world, current, safe, yaw);
            const Vector after = QuestVr::StageToLocal(current, world, yaw);
            RequireNear(Vector{after.x, 0.0f, after.z}, Vector{safe.x, 0.0f, safe.z},
                        "physical wall rollback rotated away from its safe point during snap turn");
        }
    }
}

void BoundedMovementAndVerticalGaze() {
    const Vector origin{0.0f, 0.0f, 0.0f};
    const Vector right{1.0f, 0.0f, 0.0f};
    const Vector moved = QuestVr::LocomotionCandidate(origin, right, 1.0f, 1.0f, 2.2f, 5.0f);
    Require(std::fabs(moved.Length() - 0.22f) < 0.0001f,
            "diagonal stick or long frame moved farther than the collision safety bound");
    RequireNear(QuestVr::LocomotionCandidate(origin, right, 1.0f, 1.0f, 2.2f, -1.0f),
                origin, "negative frame time caused backwards locomotion");
    RequireNear(QuestVr::HorizontalHeadRight(Vector{0.0f, 1.0f, 0.0f}, right),
                right, "vertical gaze produced an invalid movement direction");
}

void SweepAcrossThinWall() {
    const Vector before{-0.7f, 1.7f, 0.0f};
    const Vector after{0.7f, 1.7f, 0.0f};
    const auto touchesThinWall = [](const Vector& point) {
        return std::fabs(point.x) < 0.28f;
    };
    Require(!touchesThinWall(before) && !touchesThinWall(after),
            "thin-wall regression must have clear endpoints");
    Require(QuestVr::HorizontalMotionBlocked(before, after, touchesThinWall),
            "physical head motion tunnelled through a wall between clear endpoints");
    Require(!QuestVr::HorizontalMotionBlocked(
                Vector{-0.7f, 1.7f, 0.0f}, Vector{-0.7f, 1.7f, 0.7f}, touchesThinWall),
            "movement parallel to a wall was blocked");
    Require(QuestVr::HorizontalMotionBlocked(
                before, Vector{100.0f, 1.7f, 0.0f}, [](const Vector&) { return false; }),
            "large tracking-space jump was accepted instead of rebased");
}

void ReferenceSpaceContinuity() {
    const Vector origin{4.0f, 0.0f, -2.0f};
    const Vector previousHead{3.2f, 1.7f, -5.5f};
    const Vector oldWorld{-4.2f, -0.2f, 7.1f};
    const float oldYaw = 0.8f;
    const Vector localHead = QuestVr::StageToLocal(previousHead, oldWorld, oldYaw);
    for (float originYaw : {-2.0f, 0.0f, 0.9f}) {
        Vector world = oldWorld;
        Vector newHead = previousHead;
        float yaw = oldYaw;
        QuestVr::RebaseReferenceSpace(origin, originYaw, world, yaw, newHead);
        const Vector independentHead = Yaw(originYaw).Inverted().Rotate(previousHead - origin);
        RequireNear(newHead, independentHead, "recenter transformed tracking origin incorrectly");
        RequireNear(QuestVr::StageToLocal(newHead, world, yaw), localHead,
                    "OpenXR reference-space change moved the headset through the map");
        const Vector resumedHead{10.4f, 1.7f, -6.0f};
        const Vector rebased = QuestVr::RestoreHorizontalHeadPosition(world, resumedHead, localHead, yaw);
        RequireNear(QuestVr::StageToLocal(resumedHead, rebased, yaw), localHead,
                    "tracking reacquisition or loading movement displaced the saved local head");
    }
}
} // namespace

int main() {
    try {
        RendererTransformAgreement();
        SpawnAtTrackingHead();
        SnapAwayFromOrigin();
        TurnAndMoveTogether();
        TurnDirectionAndAudioBasis();
        RoomScaleCollisionCompensation();
        RoomScaleCollisionWhileTurning();
        BoundedMovementAndVerticalGaze();
        SweepAcrossThinWall();
        ReferenceSpaceContinuity();
        std::cout << "PASS: 10 shared Quest transform regression groups; "
                     "481 renderer rotations, 1201 off-origin turns, 15 simultaneous "
                     "turn/move cases. No headset or game data required.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
