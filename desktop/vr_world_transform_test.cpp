#include "vr_world_transform.h"
#include "quest_player_posture.h"
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

void UserRecenterAndHud() {
    const Vector oldHead{3.2f, 1.7f, -5.5f};
    for (const Vector origin : {Vector{3.2f,0.0f,-5.5f},Vector{-4.1f,0.2f,8.7f}}) {
    for (float oldYaw : {-2.4f, 0.0f, 1.7f}) {
        for (float originYaw : {-2.0f, 0.0f, 0.9f}) {
            Vector world{-4.2f, -0.2f, 7.1f};
            Vector head = oldHead;
            const Vector mapHead = QuestVr::StageToLocal(head, world, oldYaw);
            const Vector mapForward = QuestVr::StageDirectionToLocal(
                Yaw(originYaw).Rotate(Vector{0,0,-1}), oldYaw);
            QuestVr::RecenterReferenceSpace(origin, originYaw, world, oldYaw, head);
            RequireNear(head, Yaw(originYaw).Inverted().Rotate(oldHead-origin),
                "user recenter did not transform the previous tracking head");
            RequireNear(QuestVr::StageToLocal(head,world,oldYaw),mapHead,
                "user recenter teleported the player in the map");
            const Vector afterForward = QuestVr::StageDirectionToLocal(Vector{0,0,-1},oldYaw);
            if (std::fabs(originYaw) > 0.01f)
                Require((mapForward-afterForward).Length() > 0.1f,
                    "map compensation canceled user recenter orientation");
        }
    }
    }
    for (float yaw : {-2.4f,0.0f,1.7f}) {
        for (float pitch : {-0.8f,0.0f,0.7f}) {
            const OVR::Posef head(Yaw(yaw)*OVR::Quatf(Vector{1,0,0},pitch),oldHead);
            const OVR::Posef hud = QuestVr::HeadLockedHudPose(head);
            RequireNear(head.Rotation.Inverted().Rotate(hud.Translation-head.Translation),
                Vector{0,0,-QuestVr::DebugHudDistance},
                "debug HUD moved away from the view center at an off-origin head pose");
            RequireNear(hud.Rotation.Rotate(Vector{0,0,-1}),head.Rotation.Rotate(Vector{0,0,-1}),
                "debug HUD orientation disagrees with head orientation");
        }
    }
}

void OriginalHandGripBasis() {
    for (float yaw : {-2.7f,0.0f,1.6f}) {
        for (float pitch : {-1.1f,0.0f,0.8f}) {
            for (float roll : {-2.0f,0.0f,2.4f}) {
                const auto rotation = Yaw(yaw) * OVR::Quatf(Vector{1,0,0},pitch) *
                    OVR::Quatf(Vector{0,0,1},roll);
                for (const Vector origin : {Vector{0,0,0},Vector{6.2f,1.4f,-3.8f}}) {
                    const OVR::Posef grip{rotation,origin};
                    const auto hand = QuestVr::OriginalHandGripPose(grip);
                    RequireNear(hand.Translation,origin,"Source hand basis moved the grip centroid");
                    RequireNear(hand.Rotation.Rotate(Vector{1,0,0}),rotation.Rotate(Vector{1,0,0}),
                        "Source palm normal disagrees with OpenXR grip X");
                    RequireNear(hand.Rotation.Rotate(Vector{0,1,0}),rotation.Rotate(Vector{0,0,-1}),
                        "Source thumbward grasp tube disagrees with OpenXR grip -Z");
                    RequireNear(hand.Rotation.Rotate(Vector{0,0,-1}),rotation.Rotate(Vector{0,-1,0}),
                        "Source weapon forward disagrees with grip-local -Y");
                    const Vector point{0.04f,0.03f,-0.08f};
                    const auto rightLocal = OVR::Quatf(Vector{1,0,0},-QuestVr::Pi*0.5f).Rotate(point);
                    const auto mirroredLocal = OVR::Quatf(Vector{1,0,0},-QuestVr::Pi*0.5f).Rotate(
                        Vector{-point.x,point.y,point.z});
                    RequireNear(mirroredLocal,Vector{-rightLocal.x,rightLocal.y,rightLocal.z},
                        "Grip basis fails to commute with original left-hand reflection");
                    RequireNear(hand.Transform(point),grip.Transform(rightLocal),
                        "Hand basis was applied in tracking rather than controller-local space");
                }
            }
        }
    }
}

void MapLocalSaveRestoration() {
    const Vector savedFeet{8.0f, 2.5f, -12.0f};
    for (float savedHeading : {-2.4f, 0.0f, 1.1f}) {
        for (float newHeading : {-1.5f, 0.0f, 2.0f}) {
            for (const Vector newHead : {Vector{0.0f, 1.65f, 0.0f}, Vector{15.0f, 1.9f, -21.0f}}) {
                Vector world{};
                float yaw{};
                QuestVr::RestoreSavedMapPose(savedFeet, savedHeading, newHead, newHeading, world, yaw);
                const Vector localFeet = QuestVr::StageToLocal(Vector{newHead.x, 0.0f, newHead.z}, world, yaw);
                RequireNear(localFeet, savedFeet, "save changed map position after physical walk/recenter");
                const Vector forward = Yaw(newHeading).Rotate(Vector{0.0f, 0.0f, -1.0f});
                RequireNear(QuestVr::StageDirectionToLocal(forward, yaw),
                    Yaw(savedHeading).Rotate(Vector{0.0f, 0.0f, -1.0f}),
                    "save changed map heading after physical turn/recenter");
                Require(std::fabs(QuestVr::StageToLocal(newHead, world, yaw).y -
                    savedFeet.y - newHead.y) < 0.0001f, "save forced the old wearer's eye height");
            }
        }
    }
}

void SeatedFloorSaveAndRecenter() {
    for (float floor : {-1.1f,-0.73f,0.0f}) {
        for (float headYaw : {-2.0f,0.0f,1.4f}) {
            const Vector head{6.2f,floor+1.65f,-3.8f};
            const Vector savedFeet{4.1f,2.3f,-8.7f};
            Vector world{};
            float yaw{};
            QuestVr::RestoreSavedMapPose(savedFeet,0.7f,head,headYaw,world,yaw,floor);
            RequireNear(QuestVr::StageToLocal(Vector{head.x,floor,head.z},world,yaw),savedFeet,
                "Seated restore placed feet under the map floor");
            Require(std::fabs(QuestVr::StageToLocal(head,world,yaw).y-savedFeet.y-1.65f)<0.0001f,
                "Seated restore lost calibrated eye height");
            for (float originY : {-0.2f,0.0f,0.3f}) {
                const Vector origin{1.2f,originY,-0.9f};
                Vector newWorld=world,newHead=head;
                QuestVr::RecenterReferenceSpace(origin,0.4f,newWorld,yaw,newHead);
                Require(std::fabs((newHead.y-(floor-originY))-1.65f)<0.0001f,
                    "Reference change lost seated eye-height calibration");
                Require(std::fabs((floor-originY)-newWorld.y-savedFeet.y)<0.0001f,
                    "Reference change moved seated feet through map floor");
            }
            // Explicit seated/standing switch translates the world by the
            // same floor delta, keeping the prior map-local feet unchanged.
            const float newFloor=floor-0.6f;
            Vector changedWorld=world;
            changedWorld.y=QuestVr::WorldYAfterVirtualFloorChange(world.y,floor,newFloor);
            RequireNear(QuestVr::StageToLocal(Vector{head.x,newFloor,head.z},changedWorld,yaw),savedFeet,
                "Posture switch moved existing map feet before ground probing");
        }
    }
}

void VirtualFloorReferenceHeightContinuity() {
    constexpr float mapFloorY = -0.198029f;
    for (bool seated : {false,true}) {
        for (bool rebase : {false,true}) {
            for (float oldYaw : {-2.1f,0.0f,1.4f}) {
                for (float originYaw : {-0.8f,0.0f,1.7f}) {
                    for (float originY : {-0.689f,0.0f,0.3f}) {
                        const Vector oldHead{3.4f,seated ? 1.014f : 1.7f,-2.9f};
                        QuestVr::SeatedPosture posture;
                        Require(posture.SetSeated(seated,oldHead.y),"Reference fixture posture rejected");
                        const float previousFloorY=posture.virtualFloorY;
                        const Vector oldWorld{-0.6f,previousFloorY-mapFloorY,2.5f};
                        const Vector oldFeet=QuestVr::StageToLocal(
                            Vector{oldHead.x,previousFloorY,oldHead.z},oldWorld,oldYaw);
                        const Vector origin{1.2f,originY,-0.9f};
                        Vector world=oldWorld,head=oldHead;
                        float yaw=oldYaw;
                        Require(posture.ApplyRecenter(originY),"Bounded reference fixture rejected");
                        if (rebase) QuestVr::RebaseReferenceSpace(origin,originYaw,world,yaw,head);
                        else QuestVr::RecenterReferenceSpace(origin,originYaw,world,yaw,head);
                        const float genericWorldY=world.y;
                        if (!seated && originY == -0.689f) {
                            const float genericFeetY=posture.virtualFloorY-genericWorldY;
                            Require(std::fabs(genericFeetY-(-0.887029f))<0.0001f &&
                                mapFloorY-genericFeetY>0.45f,
                                "Standing regression no longer reproduces below-floor step-up lockout");
                        }
                        world.y=QuestVr::WorldYAfterVirtualFloorChange(
                            oldWorld.y,previousFloorY,posture.virtualFloorY);
                        RequireNear(head,Yaw(originYaw).Inverted().Rotate(oldHead-origin),
                            "Floor correction modified the raw rebased tracking pose");
                        RequireNear(QuestVr::StageToLocal(
                            Vector{head.x,posture.virtualFloorY,head.z},world,yaw),oldFeet,
                            "Vertical reference change moved map feet through the floor");
                        const float physicalEyeHeight=head.y-posture.virtualFloorY;
                        Require(std::fabs(QuestVr::StageToLocal(head,world,yaw).y-
                            mapFloorY-physicalEyeHeight)<0.0001f,
                            "Reference correction disconnected real head height from map floor");
                        if (seated) {
                            Require(std::fabs(world.y-genericWorldY)<0.0001f,
                                "Virtual floor correction changed calibrated seated reference behavior");
                            Require(std::fabs(physicalEyeHeight-QuestVr::SeatedTargetEyeHeight)<0.0001f,
                                "Seated reference correction lost calibrated eye height");
                        } else {
                            Require(std::fabs(world.y-oldWorld.y)<0.0001f && posture.virtualFloorY==0.0f,
                                "Standing reference correction moved the real floor");
                            Require(std::fabs(physicalEyeHeight-(oldHead.y-originY))<0.0001f,
                                "Standing reference correction forced a prior eye height");
                        }
                        constexpr float pendingWorldY=1.3f;
                        const float pendingFeetY=previousFloorY-pendingWorldY;
                        const float correctedPendingY=QuestVr::WorldYAfterVirtualFloorChange(
                            pendingWorldY,previousFloorY,posture.virtualFloorY);
                        Require(std::fabs(posture.virtualFloorY-correctedPendingY-pendingFeetY)<0.0001f,
                            "Reference correction moved pending legacy save feet");
                    }
                }
            }
        }
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
        UserRecenterAndHud();
        OriginalHandGripBasis();
        MapLocalSaveRestoration();
        SeatedFloorSaveAndRecenter();
        VirtualFloorReferenceHeightContinuity();
        std::cout << "PASS: 15 shared Quest transform regression groups; "
                     "481 renderer rotations, 1201 off-origin turns, 15 simultaneous "
                     "turn/move cases. No headset or game data required.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
