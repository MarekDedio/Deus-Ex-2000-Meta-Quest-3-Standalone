#include "quest_player_posture.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::size_t checks{};

void Require(bool value, const char* description) {
    ++checks;
    if (!value) throw std::runtime_error(description);
}

void Near(float actual, float expected, const char* description, float tolerance = 0.000002f) {
    Require(std::isfinite(actual) && std::fabs(actual - expected) <= tolerance, description);
}

void Unchanged(const QuestVr::SeatedPosture& actual, const QuestVr::SeatedPosture& before,
               const char* description) {
    Require(actual.seated == before.seated && actual.calibrated == before.calibrated &&
        std::memcmp(&actual.virtualFloorY, &before.virtualFloorY, sizeof(float)) == 0, description);
}

void SeatedCalibrationAndHeadDeltas() {
    QuestVr::SeatedPosture posture;
    Require(!posture.seated && !posture.calibrated && posture.virtualFloorY == 0.0f && posture.Ready(),
            "default posture was not ready standing mode");
    const float physicalHeadY = 0.92f;
    Require(posture.SetSeated(true, physicalHeadY) && posture.seated && posture.calibrated && posture.Ready(),
            "valid seated height did not calibrate");
    Near(posture.virtualFloorY, -0.73f, "0.92m seated head did not produce -0.73m virtual floor");
    Near(physicalHeadY - posture.virtualFloorY, 1.65f, "seated virtual eye missed comfort target");
    const auto calibrated = posture;
    for (int sample = -40; sample <= 40; ++sample) {
        const float delta = static_cast<float>(sample) / 100.0f;
        const float headY = physicalHeadY + delta;
        Near(headY - posture.virtualFloorY, QuestVr::SeatedTargetEyeHeight + delta,
             "seated floor suppressed physical lean/crouch delta");
        Require(posture.SetSeated(true, headY), "valid repeated mode selection failed");
        Unchanged(posture, calibrated, "repeated seated selection continuously recalibrated floor");
    }
    Near(physicalHeadY, 0.92f, "posture changed physical head pose");
}

void ExplicitRecalibrationAndStanding() {
    QuestVr::SeatedPosture posture;
    Require(!posture.RecalibrateSeated(0.92f), "standing mode accepted seated recalibration");
    Require(posture.SetSeated(true, 0.92f) && posture.RecalibrateSeated(1.2f),
            "explicit seated recalibration failed");
    Near(posture.virtualFloorY, -0.45f, "explicit recalibration retained old floor");
    Near(1.2f - posture.virtualFloorY, 1.65f, "explicit recalibration missed target eye");
    Near(0.92f - posture.virtualFloorY, 1.37f, "post-recalibration head delta was lost");
    Require(posture.SetSeated(false, 1.2f) && !posture.seated && !posture.calibrated && posture.Ready(),
            "standing transition retained seated calibration");
    Require(posture.virtualFloorY == 0.0f, "standing transition retained a virtual floor offset");
    const auto standing = posture;
    Require(!posture.RecalibrateSeated(1.2f), "standing recalibration modified floor");
    Unchanged(posture, standing, "standing failed recalibration changed state");
    Require(posture.SetSeated(false, 0.5f), "repeat standing selection failed");
    Unchanged(posture, standing, "standing floor followed tracked head height");
}

void InvalidCalibrationIsAtomic() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (float invalid : {nan, infinity, -infinity, -1.0f, 0.0f,
                           std::nextafter(QuestVr::SeatedMinimumHeadHeight, 0.0f),
                           std::nextafter(QuestVr::SeatedMaximumHeadHeight, infinity),
                           std::numeric_limits<float>::max()}) {
        QuestVr::SeatedPosture standing;
        const auto standingBefore = standing;
        Require(!standing.SetSeated(true, invalid), "invalid head height enabled seated mode");
        Unchanged(standing, standingBefore, "invalid enable wrote standing state");
        Require(!standing.SetSeated(false, invalid), "invalid head height accepted standing selection");
        Unchanged(standing, standingBefore, "invalid standing selection changed state");
        QuestVr::SeatedPosture seated;
        Require(seated.SetSeated(true, 0.92f), "invalid-input fixture could not calibrate");
        const auto seatedBefore = seated;
        Require(!seated.SetSeated(true, invalid), "invalid repeat enable accepted head height");
        Unchanged(seated, seatedBefore, "invalid repeat enable wrote calibration state");
        Require(!seated.SetSeated(false, invalid), "invalid head height disabled seated mode");
        Unchanged(seated, seatedBefore, "invalid disable wrote calibration state");
        Require(!seated.RecalibrateSeated(invalid), "invalid height recalibrated seated floor");
        Unchanged(seated, seatedBefore, "invalid recalibration partially wrote state");
    }
    QuestVr::SeatedPosture minimum, maximum;
    Require(minimum.SetSeated(true, QuestVr::SeatedMinimumHeadHeight), "minimum head-height boundary rejected");
    Require(maximum.SetSeated(true, QuestVr::SeatedMaximumHeadHeight), "maximum head-height boundary rejected");
    Near(minimum.virtualFloorY, -1.4f, "minimum seated floor incorrect");
    Near(maximum.virtualFloorY, 0.85f, "maximum seated floor incorrect");
}

void RecenterPreservesVirtualEyeAndHeadDeltas() {
    for (float originY : {-2.0f, -1.0f, -0.1f, 0.0f, 0.1f, 1.0f, 2.0f}) {
        QuestVr::SeatedPosture posture;
        Require(posture.SetSeated(true, 0.92f), "recenter fixture could not calibrate");
        const float oldHeadY = 0.92f;
        const float oldEyeY = oldHeadY - posture.virtualFloorY;
        Require(posture.ApplyRecenter(originY), "valid seated reference-space origin was rejected");
        const float newHeadY = oldHeadY - originY;
        Near(newHeadY - posture.virtualFloorY, oldEyeY, "recenter changed virtual seated eye height");
        Near(newHeadY + 0.18f - posture.virtualFloorY, oldEyeY + 0.18f,
             "recenter suppressed subsequent physical head delta");
        Require(posture.seated && posture.calibrated && posture.Ready(), "recenter lost valid seated calibration");
        Near(oldHeadY, 0.92f, "recenter helper changed physical old head pose");
    }
    QuestVr::SeatedPosture posture;
    Require(posture.SetSeated(true, 0.92f), "sequential recenter fixture could not calibrate");
    float headY = 0.92f;
    for (int sample = 0; sample < 32; ++sample) {
        const float originY = sample % 2 == 0 ? 0.05f : -0.025f;
        Require(posture.ApplyRecenter(originY), "valid sequential recenter was rejected");
        headY -= originY;
        Near(headY - posture.virtualFloorY, 1.65f, "sequential recenter accumulated virtual eye displacement");
    }
}

void RecenterRejectsInvalidStateAtomically() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (float invalid : {nan, infinity, -infinity, std::numeric_limits<float>::max(),
                           std::nextafter(QuestVr::SeatedMaximumRecenterOffset, infinity),
                           -std::nextafter(QuestVr::SeatedMaximumRecenterOffset, infinity)}) {
        QuestVr::SeatedPosture posture;
        Require(posture.SetSeated(true, 0.92f), "invalid recenter fixture could not calibrate");
        const auto before = posture;
        Require(!posture.ApplyRecenter(invalid), "invalid recenter origin was accepted");
        Unchanged(posture, before, "invalid recenter partially wrote calibration state");
        QuestVr::SeatedPosture standing;
        const auto standingBefore = standing;
        Require(!standing.ApplyRecenter(invalid), "standing accepted invalid recenter origin");
        Unchanged(standing, standingBefore, "invalid recenter wrote standing state");
    }
    QuestVr::SeatedPosture posture;
    Require(posture.SetSeated(true, 0.25f), "floor-bound fixture could not calibrate");
    const auto before = posture;
    Require(!posture.ApplyRecenter(4.0f), "recenter exceeded virtual-floor bound");
    Unchanged(posture, before, "floor-bound rejection partially wrote state");
    for (float originY : {-4.0f, 4.0f}) {
        QuestVr::SeatedPosture edge;
        Require(edge.SetSeated(true, QuestVr::SeatedTargetEyeHeight) && edge.ApplyRecenter(originY),
                "valid floor/recenter bound was rejected");
        Near(edge.virtualFloorY, -originY, "bounded recenter floor incorrect");
        const auto edgeBefore = edge;
        Require(!edge.ApplyRecenter(std::copysign(0.01f, originY)), "recenter moved beyond floor boundary");
        Unchanged(edge, edgeBefore, "boundary rejection wrote floor state");
    }
    QuestVr::SeatedPosture unready;
    unready.seated = true;
    Require(!unready.Ready(), "uncalibrated seated mode was declared ready");
    const auto unreadyBefore = unready;
    Require(!unready.ApplyRecenter(0.1f), "uncalibrated recenter invented calibration");
    Unchanged(unready, unreadyBefore, "uncalibrated recenter changed state");
    Require(unready.RecalibrateSeated(0.92f) && unready.Ready(), "explicit calibration did not recover readiness");
}

void StandingRecenterKeepsPhysicalFloor() {
    QuestVr::SeatedPosture posture;
    const auto before = posture;
    for (float originY : {-4.0f, -0.73f, 0.0f, 0.73f, 4.0f}) {
        Require(posture.ApplyRecenter(originY), "valid standing recenter was rejected");
        Unchanged(posture, before, "standing recenter shifted real tracking floor");
        Near(1.7f - posture.virtualFloorY, 1.7f, "standing eye was lifted toward seated target");
    }
}

void CompleteFreshWindow(QuestVr::SeatedCalibrationGate& gate, float height) {
    Require(!gate.Observe(height, 0.05f, true), "fresh calibration window completed on its first sample");
    for (int sample = 0; sample < 14; ++sample)
        Require(!gate.Observe(height, 0.05f, true), "fresh calibration completed before 0.75s of observed stability");
    Require(gate.Observe(height, 0.05f, true) && gate.Complete(), "stable tracked height did not complete at 0.75s");
}

void PrimeIncompleteWindow(QuestVr::SeatedCalibrationGate& gate) {
    for (int sample = 0; sample < 8; ++sample)
        Require(!gate.Observe(0.92f, 0.1f, true), "incomplete 0.7s calibration window completed early");
}

void AutomaticCalibrationRequiresReadyAndRealHeight() {
    QuestVr::SeatedCalibrationGate gate;
    Require(!gate.Complete(), "new calibration gate was already complete");
    for (int sample = 0; sample < 64; ++sample)
        Require(!gate.Observe(1.604f, 0.05f, false) && !gate.Complete(), "loading accumulated calibration stability");
    CompleteFreshWindow(gate, 0.943f);
    QuestVr::SeatedPosture posture;
    Require(posture.SetSeated(true, 0.943f), "automatic completing height did not calibrate posture");
    Near(0.943f - posture.virtualFloorY, 1.65f, "automatic calibration retained an early loading head pose");
    gate.Reset();
    PrimeIncompleteWindow(gate);
    Require(!gate.Observe(0.92f, 0.1f, false), "collision-unready transition completed calibration");
    CompleteFreshWindow(gate, 0.92f);
}

void HeightMovementRestartsQuietWindow() {
    QuestVr::SeatedCalibrationGate gate;
    PrimeIncompleteWindow(gate);
    Require(!gate.Observe(1.1f, 0.1f, true) && !gate.Complete(), "height movement did not restart quiet window");
    for (int sample = 0; sample < 7; ++sample)
        Require(!gate.Observe(1.1f, 0.1f, true), "movement restart retained previous stability time");
    Require(gate.Observe(1.1f, 0.05f, true), "quiet height after movement could not complete");
    gate.Reset();
    Require(!gate.Observe(0.92f, 0.05f, true), "jitter window completed on first pose");
    for (int sample = 0; sample < 14; ++sample)
        Require(!gate.Observe(sample % 2 == 0 ? 0.89f : 0.95f, 0.05f, true), "bounded jitter completed too early");
    Require(gate.Observe(0.92f, 0.05f, true), "quiet bounded height jitter never completed");
    gate.Reset();
    Require(!gate.Observe(0.5f, 0.05f, true), "range-boundary window completed on first pose");
    const float withinRange = 0.5f + QuestVr::SeatedCalibrationMaximumHeightRange;
    for (int sample = 0; sample < 14; ++sample)
        Require(!gate.Observe(withinRange, 0.05f, true), "height-range boundary completed early");
    Require(gate.Observe(withinRange, 0.05f, true), "inclusive height-range boundary was rejected");
    gate.Reset();
    Require(!gate.Observe(0.5f, 0.1f, true), "outside-range fixture completed on first pose");
    for (int sample = 0; sample < 7; ++sample)
        Require(!gate.Observe(0.5f, 0.1f, true), "outside-range fixture completed too early");
    const float outsideRange = std::nextafter(withinRange, 1.0f);
    Require(!gate.Observe(outsideRange, 0.1f, true), "out-of-range height did not restart window");
    Require(!gate.Observe(outsideRange, 0.1f, true), "out-of-range restart accumulated old stability time");
}

void ReferenceResetAndInvalidTrackingDiscardProgress() {
    QuestVr::SeatedCalibrationGate gate;
    PrimeIncompleteWindow(gate);
    gate.Reset(); // Calling reference-space event owns this reset.
    Require(!gate.Complete(), "reference reset retained completion state");
    CompleteFreshWindow(gate, 0.943f);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (float invalid : {nan, infinity, -infinity, 0.0f,
                           std::nextafter(QuestVr::SeatedMinimumHeadHeight, 0.0f),
                           std::nextafter(QuestVr::SeatedMaximumHeadHeight, infinity)}) {
        gate.Reset();
        PrimeIncompleteWindow(gate);
        Require(!gate.Observe(invalid, 0.1f, true) && !gate.Complete(), "invalid tracking completed automatic calibration");
        CompleteFreshWindow(gate, 0.92f);
    }
    for (float validBoundary : {QuestVr::SeatedMinimumHeadHeight, QuestVr::SeatedMaximumHeadHeight}) {
        gate.Reset();
        CompleteFreshWindow(gate, validBoundary);
    }
}

void InvalidTimeAndHitchesDoNotAdvanceCalibration() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (float invalid : {nan, infinity, -infinity, -0.1f, 0.0f,
                           std::nextafter(QuestVr::SeatedCalibrationMaximumDeltaSeconds, infinity),
                           1.0f, std::numeric_limits<float>::max()}) {
        QuestVr::SeatedCalibrationGate gate;
        PrimeIncompleteWindow(gate);
        Require(!gate.Observe(0.92f, invalid, true) && !gate.Complete(), "invalid/hitch timestep advanced calibration");
        CompleteFreshWindow(gate, 0.92f);
    }
    QuestVr::SeatedCalibrationGate gate;
    PrimeIncompleteWindow(gate);
    Require(gate.Observe(0.92f, 0.05f, true), "bounded 0.1s timesteps did not accumulate expected time");
}

void CompletedGateDoesNotRecalibratePhysicalDeltas() {
    QuestVr::SeatedCalibrationGate gate;
    CompleteFreshWindow(gate, 0.92f);
    QuestVr::SeatedPosture posture;
    Require(posture.SetSeated(true, 0.92f), "one-shot fixture could not calibrate");
    const auto calibrated = posture;
    for (int sample = 0; sample < 240; ++sample) {
        const float headY = sample % 2 == 0 ? 0.65f : 1.2f;
        Require(!gate.Observe(headY, 0.05f, true) && gate.Complete(), "completed gate recalibrated a real lean/crouch");
        Unchanged(posture, calibrated, "calibration gate modified posture state");
        Near(headY - posture.virtualFloorY, QuestVr::SeatedTargetEyeHeight + headY - 0.92f,
             "completed calibration suppressed a physical head-height delta");
    }
    Require(!gate.Observe(std::numeric_limits<float>::quiet_NaN(), 1.0f, false) && gate.Complete(),
            "completed gate rearmed without an explicit reset");
    gate.Reset();
    Require(!gate.Complete(), "explicit reset could not rearm completed gate");
    CompleteFreshWindow(gate, 1.2f);
}
} // namespace

int main() {
    try {
        SeatedCalibrationAndHeadDeltas();
        ExplicitRecalibrationAndStanding();
        InvalidCalibrationIsAtomic();
        RecenterPreservesVirtualEyeAndHeadDeltas();
        RecenterRejectsInvalidStateAtomically();
        StandingRecenterKeepsPhysicalFloor();
        AutomaticCalibrationRequiresReadyAndRealHeight();
        HeightMovementRestartsQuietWindow();
        ReferenceResetAndInvalidTrackingDiscardProgress();
        InvalidTimeAndHitchesDoNotAdvanceCalibration();
        CompletedGateDoesNotRecalibratePhysicalDeltas();
        std::cout << "PASS: 11 Quest player posture regression groups; " << checks
                  << " checks. Synthetic heights only; 1.65m is a VR comfort target, not an authored engine default.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
