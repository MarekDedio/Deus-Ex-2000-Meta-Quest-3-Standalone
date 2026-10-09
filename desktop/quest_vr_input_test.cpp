#include "quest_vr_input.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::size_t checks{};

void Require(bool value, const char* description) {
    ++checks;
    if (!value) throw std::runtime_error(description);
}

void Near(float actual, float expected, const char* description,
          float tolerance = 0.000002f) {
    Require(std::fabs(actual - expected) <= tolerance, description);
}

float Magnitude(const QuestVr::Stick& stick) {
    return std::hypot(stick.x, stick.y);
}

void DeadzoneAndAnalogRange() {
    for (float axis : {0.0f, -0.0f, 0.01f, -0.01f,
                       QuestVr::StickDeadzone, -QuestVr::StickDeadzone}) {
        const auto horizontal = QuestVr::ApplyRadialDeadzone(axis, 0.0f);
        const auto vertical = QuestVr::ApplyRadialDeadzone(0.0f, axis);
        Require(horizontal.x == 0.0f && horizontal.y == 0.0f,
                "resting/threshold horizontal stick moved");
        Require(vertical.x == 0.0f && vertical.y == 0.0f,
                "resting/threshold vertical stick moved");
    }
    const auto radialRest = QuestVr::ApplyRadialDeadzone(0.12f, -0.12f);
    Require(radialRest.x == 0.0f && radialRest.y == 0.0f,
            "radial input below deadzone moved");
    const auto justOutside = QuestVr::ApplyRadialDeadzone(
        std::nextafter(QuestVr::StickDeadzone, 1.0f), 0.0f);
    Require(justOutside.x > 0.0f && justOutside.x < 0.000001f && justOutside.y == 0.0f,
            "deadzone edge was discontinuous");
    const auto half = QuestVr::ApplyRadialDeadzone(0.59f, 0.0f);
    Near(half.x, 0.5f, "remaining stick range was not remapped");
    Near(half.y, 0.0f, "horizontal remapping introduced vertical movement");
    const auto negativeHalf = QuestVr::ApplyRadialDeadzone(0.0f, -0.59f);
    Near(negativeHalf.y, -0.5f, "negative remaining range was not remapped");
}

void RadialDirectionAndDiagonalBound() {
    constexpr double pi = 3.14159265358979323846;
    for (int direction = 0; direction < 32; ++direction) {
        const double angle = 2.0 * pi * static_cast<double>(direction) / 32.0;
        const float unitX = static_cast<float>(std::cos(angle));
        const float unitY = static_cast<float>(std::sin(angle));
        float previousMagnitude{};
        for (int sample = 0; sample <= 64; ++sample) {
            const float radius = static_cast<float>(sample) / 32.0f;
            const auto filtered = QuestVr::ApplyRadialDeadzone(unitX * radius, unitY * radius);
            const float expectedMagnitude = radius <= QuestVr::StickDeadzone ? 0.0f :
                (std::min(radius, 1.0f) - QuestVr::StickDeadzone) /
                    (1.0f - QuestVr::StickDeadzone);
            const float actualMagnitude = Magnitude(filtered);
            Require(std::isfinite(filtered.x) && std::isfinite(filtered.y),
                    "radial remapping produced nonfinite input");
            Require(actualMagnitude <= 1.0000002f, "diagonal input exceeded unit speed");
            Near(actualMagnitude, expectedMagnitude, "radial magnitude remapping was incorrect");
            Require(actualMagnitude + 0.000002f >= previousMagnitude,
                    "analog magnitude was not monotonic");
            Near(filtered.x, unitX * expectedMagnitude, "radial filtering changed x direction");
            Near(filtered.y, unitY * expectedMagnitude, "radial filtering changed y direction");
            previousMagnitude = actualMagnitude;
        }
    }
    const auto diagonal = QuestVr::ApplyRadialDeadzone(1.0f, 1.0f);
    Near(diagonal.x, std::sqrt(0.5f), "full diagonal x was not normalized");
    Near(diagonal.y, std::sqrt(0.5f), "full diagonal y was not normalized");
    const auto forward = QuestVr::ApplyRadialDeadzone(0.0f, 1.0f);
    const auto right = QuestVr::ApplyRadialDeadzone(1.0f, 0.0f);
    Require(forward.x == 0.0f && forward.y == 1.0f, "forward stick direction changed");
    Require(right.x == 1.0f && right.y == 0.0f, "right stick direction changed");
}

void InvalidAndExtremeStickInput() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (float invalid : {nan, infinity, -infinity}) {
        for (const auto filtered : {QuestVr::ApplyRadialDeadzone(invalid, 0.5f),
                                    QuestVr::ApplyRadialDeadzone(0.5f, invalid),
                                    QuestVr::ApplyRadialDeadzone(invalid, invalid)}) {
            Require(filtered.x == 0.0f && filtered.y == 0.0f,
                    "invalid stick sample caused movement");
        }
    }
    const float maximum = std::numeric_limits<float>::max();
    for (float x : {-maximum, maximum}) {
        for (float y : {-maximum, maximum}) {
            const auto filtered = QuestVr::ApplyRadialDeadzone(x, y);
            Require(std::isfinite(filtered.x) && std::isfinite(filtered.y),
                    "finite extreme stick overflowed");
            Near(Magnitude(filtered), 1.0f, "finite extreme stick did not clamp");
            Require(filtered.x * (x > 0.0f ? 1.0f : -1.0f) > 0.0f &&
                    filtered.y * (y > 0.0f ? 1.0f : -1.0f) > 0.0f,
                    "finite extreme stick changed direction");
        }
    }
    const auto subnormal = QuestVr::ApplyRadialDeadzone(
        std::numeric_limits<float>::denorm_min(), 0.0f);
    Require(subnormal.x == 0.0f && subnormal.y == 0.0f,
            "subnormal stick caused movement");
}

void AxisEngageReleaseAndHold() {
    for (float sign : {-1.0f, 1.0f}) {
        bool latched{};
        Require(!QuestVr::UpdateAxis(sign * 0.69f, latched) && !latched,
                "axis engaged below threshold");
        Require(QuestVr::UpdateAxis(sign * QuestVr::AxisEngageThreshold, latched) && latched,
                "axis did not engage at threshold");
        for (int hold = 0; hold < 120; ++hold) {
            Require(!QuestVr::UpdateAxis(sign, latched) && latched,
                    "held axis repeated activation");
        }
        Require(!QuestVr::UpdateAxis(-sign, latched) && latched,
                "opposite axis rearmed without neutral");
        Require(!QuestVr::UpdateAxis(sign * std::nextafter(QuestVr::AxisReleaseThreshold, 1.0f),
                                    latched) && latched,
                "axis released above release band");
        Require(!QuestVr::UpdateAxis(sign * QuestVr::AxisReleaseThreshold, latched) && !latched,
                "axis did not release at threshold");
        Require(QuestVr::UpdateAxis(-sign, latched) && latched,
                "centered axis did not rearm opposite direction");
        Require(!QuestVr::UpdateAxis(0.0f, latched) && !latched,
                "centered axis remained latched");
    }
}

void AxisJitterAndInvalidInput() {
    bool latched{};
    Require(QuestVr::UpdateAxis(1.0f, latched), "jitter fixture did not engage");
    for (int sample = 0; sample < 100; ++sample) {
        Require(!QuestVr::UpdateAxis(sample % 2 == 0 ? 0.69f : 0.71f, latched) && latched,
                "engage-threshold jitter repeated activation");
    }
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (float invalid : {nan, infinity, -infinity}) {
        Require(!QuestVr::UpdateAxis(invalid, latched) && latched,
                "invalid axis sample rearmed held input");
    }
    Require(!QuestVr::UpdateAxis(1.0f, latched) && latched,
            "held input repeated after invalid sample");
    Require(!QuestVr::UpdateAxis(-0.0f, latched) && !latched,
            "negative zero did not center axis");
    for (float invalid : {nan, infinity, -infinity}) {
        Require(!QuestVr::UpdateAxis(invalid, latched) && !latched,
                "invalid idle axis sample activated input");
    }
    Require(QuestVr::UpdateAxis(-1.0f, latched) && latched,
            "valid input could not engage after invalid idle samples");
}
} // namespace

int main() {
    try {
        DeadzoneAndAnalogRange();
        RadialDirectionAndDiagonalBound();
        InvalidAndExtremeStickInput();
        AxisEngageReleaseAndHold();
        AxisJitterAndInvalidInput();
        std::cout << "PASS: 5 Quest VR input regression groups; " << checks
                  << " checks. Synthetic input only; no SDK, headset or game data required.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
