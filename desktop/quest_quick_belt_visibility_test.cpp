#include "quest_quick_belt_visibility.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
using QuestVr::QuickBeltVisibility;
std::size_t checks{}, rejections{};

void Require(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}

void State(const QuickBeltVisibility& value, float alpha, bool visible) {
    Require(value.Alpha() == alpha, "Quickbar opacity differs from deterministic elapsed time");
    Require(value.Visible() == visible, "Quickbar visibility disagrees with its display window");
    Require(std::isfinite(value.Alpha()) && value.Alpha() >= 0.0f && value.Alpha() <= 1.0f,
        "Quickbar opacity escaped its finite unit interval");
}

void FreshAndFade() {
    QuickBeltVisibility value;
    State(value, 0.0f, false);
    value.SetHeld(false);
    value.Tick(0.0f);
    value.Tick(-0.0f);
    value.Tick(std::numeric_limits<float>::max());
    State(value, 0.0f, false);
    value.Reveal();
    State(value, 1.0f, true);
    value.Tick(2.5f);
    State(value, 1.0f, true);
    value.Tick(0.25f);
    State(value, 1.0f, true);
    value.Tick(0.0625f);
    State(value, 0.75f, true);
    value.Tick(0.0625f);
    State(value, 0.5f, true);
    value.Tick(0.0625f);
    State(value, 0.25f, true);
    value.Tick(0.0625f);
    State(value, 0.0f, false);
    value.Tick(1.0f);
    State(value, 0.0f, false);

    value.Reveal();
    value.Tick(QuickBeltVisibility::DisplaySeconds);
    State(value, 0.0f, false);
    value.Reveal();
    value.Tick(std::numeric_limits<float>::max());
    State(value, 0.0f, false);
    value.Reveal();
    value.Tick(3.0f - 0.03125f);
    State(value, 0.125f, true);
    value.Tick(0.125f);
    State(value, 0.0f, false);
}

void ActionWindows() {
    QuickBeltVisibility value;
    value.Reveal();
    value.Tick(2.875f);
    State(value, 0.5f, true);
    value.Reveal();
    State(value, 1.0f, true);
    value.Tick(2.75f);
    State(value, 1.0f, true);
    value.Tick(0.125f);
    State(value, 0.5f, true);
    value.SetHeld(false); // Per-frame idle input is not another action.
    value.Tick(0.125f);
    State(value, 0.0f, false);
    for (std::size_t action = 0u; action < 100'000u; ++action) value.Reveal();
    value.Tick(3.0f);
    State(value, 0.0f, false);

    value.SetHeld(true);
    State(value, 1.0f, true);
    value.Tick(std::numeric_limits<float>::max());
    State(value, 1.0f, true);
    for (std::size_t frame = 0u; frame < 1'000u; ++frame) {
        value.SetHeld(true);
        value.Tick(128.0f);
        State(value, 1.0f, true);
    }
    value.SetHeld(false);
    value.Tick(2.875f);
    State(value, 0.5f, true);
    value.SetHeld(false);
    value.Tick(0.125f);
    State(value, 0.0f, false);
    value.Reveal();
    value.SetHeld(true);
    value.Reset();
    State(value, 0.0f, false);
    value.Tick(0.125f);
    State(value, 0.0f, false);
    value.SetHeld(false);
    State(value, 0.0f, false);
    value.Reset();
    State(value, 0.0f, false);
}

void InvalidDeltas() {
    const std::array<float, 6u> invalid{{-1.0f, -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}};
    for (unsigned phase = 0u; phase < 4u; ++phase) {
        QuickBeltVisibility initial;
        if (phase != 0u) initial.Reveal();
        if (phase == 2u) initial.Tick(2.875f);
        if (phase == 3u) initial.SetHeld(true);
        for (float delta : invalid) {
            auto value = initial;
            auto unchanged = initial;
            bool refused{};
            try { value.Tick(delta); }
            catch (const std::invalid_argument&) { refused = true; }
            Require(refused, "Invalid quickbar delta was accepted or raised a different exception");
            ++rejections;
            // Continuations distinguish hidden/full/fading/held state, not just
            // identical opacity at the time of a rejected mutation.
            for (float elapsed : std::array<float, 8u>{{0.0f, 0.0625f, 0.125f, 0.25f,
                1.0f, 1.0f, 1.0f, std::numeric_limits<float>::max()}}) {
                value.Tick(elapsed);
                unchanged.Tick(elapsed);
                Require(value.Alpha() == unchanged.Alpha() && value.Visible() == unchanged.Visible(),
                    "Rejected quickbar delta changed later presentation state");
            }
            value.SetHeld(false);
            unchanged.SetHeld(false);
            value.Tick(2.875f);
            unchanged.Tick(2.875f);
            Require(value.Alpha() == unchanged.Alpha() && value.Visible() == unchanged.Visible(),
                "Rejected quickbar delta changed the stored held state");
        }
    }
}

void DeterministicReplay() {
    // Integer 1/64-second reference model is independent of the timer's double
    // implementation. Exact binary deltas make every boundary reproducible.
    QuickBeltVisibility first, second;
    std::uint32_t seed = 0x51bf7329u;
    unsigned remaining{};
    bool held{};
    for (std::size_t step = 0u; step < 100'000u; ++step) {
        seed = seed * 1664525u + 1013904223u;
        const auto operation = (seed >> 24u) % 8u;
        if (operation == 0u) {
            first.Reveal(); second.Reveal(); remaining = 192u;
        } else if (operation == 1u) {
            first.SetHeld(true); second.SetHeld(true); held = true; remaining = 192u;
        } else if (operation == 2u) {
            first.SetHeld(false); second.SetHeld(false);
            if (held) remaining = 192u;
            held = false;
        } else if (operation == 3u) {
            first.Reset(); second.Reset(); held = false; remaining = 0u;
        } else {
            const auto ticks = (seed >> 8u) % 257u;
            const float delta = static_cast<float>(ticks) / 64.0f;
            first.Tick(delta); second.Tick(delta);
            remaining = held ? 192u : ticks >= remaining ? 0u : remaining - ticks;
        }
        const float alpha = held || remaining >= 16u ? 1.0f : static_cast<float>(remaining) / 16.0f;
        State(first, alpha, held || remaining != 0u);
        Require(first.Alpha() == second.Alpha() && first.Visible() == second.Visible(),
            "Identical quickbar action replay is nondeterministic");
    }
}

void FrameRates() {
    for (unsigned frequency : std::array<unsigned, 5u>{{60u, 72u, 80u, 90u, 120u}}) {
        QuickBeltVisibility value;
        value.Reveal();
        const float delta = 1.0f / static_cast<float>(frequency);
        float previous = 1.0f;
        for (unsigned frame = 0u; frame < 3u * frequency + 1u; ++frame) {
            value.Tick(delta);
            Require(value.Alpha() <= previous, "Idle quickbar opacity increased between frames");
            Require(value.Visible() == (value.Alpha() > 0.0f), "Ordinary frame opacity and visibility disagree");
            previous = value.Alpha();
        }
        State(value, 0.0f, false);
    }
    QuickBeltVisibility value;
    value.Reveal();
    value.Tick(0.000001f);
    value.Tick(3.0f - 0.000001f);
    // Source floats need not sum to exactly three. One ordinary frame must
    // expire any rounding remainder without adding a new presentation window.
    value.Tick(1.0f / 120.0f);
    State(value, 0.0f, false);
}
} // namespace

int main() {
    try {
        static_assert(std::is_trivially_copyable_v<QuickBeltVisibility>);
        static_assert(sizeof(QuickBeltVisibility) <= 16u);
        static_assert(QuickBeltVisibility::DisplaySeconds == 3.0f);
        static_assert(QuickBeltVisibility::FadeSeconds == 0.25f);
        FreshAndFade();
        ActionWindows();
        InvalidDeltas();
        DeterministicReplay();
        FrameRates();
        std::cout << "PASS quickbar visibility " << checks << " checks / " << rejections
            << " expected invalid-delta refusals; hidden idle, bounded action/hold windows, deterministic fade\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL quickbar visibility after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
