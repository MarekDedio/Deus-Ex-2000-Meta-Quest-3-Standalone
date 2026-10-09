#pragma once

#include <algorithm>
#include <cmath>

namespace QuestVr {

// This is a VR comfort target, not an authored Deus Ex player-eye default.
inline constexpr float SeatedTargetEyeHeight = 1.65f;
inline constexpr float SeatedMinimumHeadHeight = 0.25f;
inline constexpr float SeatedMaximumHeadHeight = 2.5f;
inline constexpr float SeatedMaximumFloorOffset = 4.0f;
inline constexpr float SeatedMaximumRecenterOffset = 4.0f;

// The physical OpenXR head/controller poses remain unchanged. Callers use this
// one virtual tracking-space floor for map grounding, body feet and save feet.
struct SeatedPosture {
    bool seated{};
    bool calibrated{};
    float virtualFloorY{};

    bool Ready() const {
        return seated ? calibrated && ValidFloor(virtualFloorY) : virtualFloorY == 0.0f;
    }

    bool SetSeated(bool enabled, float trackedHeadY) {
        if (!ValidHead(trackedHeadY)) return false;
        if (!enabled) {
            seated = false;
            calibrated = false;
            virtualFloorY = 0.0f;
            return true;
        }
        // Repeated mode selection must not erase a lean/crouch. Recalibration
        // is a separate explicit action, not a per-frame height adjustment.
        if (seated && Ready()) return true;
        const float floor = trackedHeadY - SeatedTargetEyeHeight;
        if (!ValidFloor(floor)) return false;
        seated = true;
        calibrated = true;
        virtualFloorY = floor;
        return true;
    }

    bool RecalibrateSeated(float trackedHeadY) {
        if (!seated || !ValidHead(trackedHeadY)) return false;
        const float floor = trackedHeadY - SeatedTargetEyeHeight;
        if (!ValidFloor(floor)) return false;
        virtualFloorY = floor;
        calibrated = true;
        return true;
    }

    // Caller must already have validated the reference-space pose and its
    // gravity alignment. The new physical head Y is oldHeadY - originY.
    bool ApplyRecenter(float originY) {
        if (!std::isfinite(originY) || std::fabs(originY) > SeatedMaximumRecenterOffset || !Ready())
            return false;
        if (!seated) return true; // Standing retains the real tracking floor.
        const double floor = static_cast<double>(virtualFloorY) - static_cast<double>(originY);
        if (!std::isfinite(floor) || std::fabs(floor) > static_cast<double>(SeatedMaximumFloorOffset))
            return false;
        // Validate the entire change before writing any calibration state.
        virtualFloorY = static_cast<float>(floor);
        return true;
    }

private:
    static bool ValidHead(float height) {
        return std::isfinite(height) && height >= SeatedMinimumHeadHeight && height <= SeatedMaximumHeadHeight;
    }

    static bool ValidFloor(float floor) {
        return std::isfinite(floor) && std::fabs(floor) <= SeatedMaximumFloorOffset;
    }
};

inline constexpr float SeatedCalibrationStableSeconds = 0.75f;
inline constexpr float SeatedCalibrationMaximumHeightRange = 0.08f;
inline constexpr float SeatedCalibrationMaximumDeltaSeconds = 0.1f;

// Automatic startup calibration must wait for a complete collision world and
// a quiet sequence of actual tracked heights. This gate never invents a pose
// or changes SeatedPosture; the caller calibrates using the completing sample.
class SeatedCalibrationGate {
public:
    void Reset() {
        hasWindow_ = false;
        complete_ = false;
        minimumHeight_ = maximumHeight_ = 0.0f;
        elapsedSeconds_ = 0.0;
    }

    bool Observe(float headY, float deltaSeconds, bool ready) {
        // One completion per reset. A caller that accidentally keeps observing
        // after its pending flag clears must not recalibrate a real lean/crouch.
        if (complete_) return false;
        if (!ready || !std::isfinite(headY) || headY < SeatedMinimumHeadHeight || headY > SeatedMaximumHeadHeight ||
            !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f || deltaSeconds > SeatedCalibrationMaximumDeltaSeconds) {
            Reset();
            return false;
        }
        if (!hasWindow_) {
            StartWindow(headY);
            return false;
        }
        const float minimum = std::min(minimumHeight_, headY);
        const float maximum = std::max(maximumHeight_, headY);
        if (maximum - minimum > SeatedCalibrationMaximumHeightRange) {
            StartWindow(headY);
            return false;
        }
        minimumHeight_ = minimum;
        maximumHeight_ = maximum;
        elapsedSeconds_ += static_cast<double>(deltaSeconds);
        if (elapsedSeconds_ < static_cast<double>(SeatedCalibrationStableSeconds)) return false;
        complete_ = true;
        return true;
    }

    bool Complete() const { return complete_; }

private:
    void StartWindow(float headY) {
        hasWindow_ = true;
        minimumHeight_ = maximumHeight_ = headY;
        // Only intervals between valid observed poses count, not the interval
        // preceding the first valid sample after loading/tracking/recenter.
        elapsedSeconds_ = 0.0;
    }

    bool hasWindow_{};
    bool complete_{};
    float minimumHeight_{}, maximumHeight_{};
    double elapsedSeconds_{};
};

} // namespace QuestVr
