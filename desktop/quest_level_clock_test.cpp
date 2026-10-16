#include "quest_level_clock.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

// Pure native clock arithmetic controls. Independent x87 instructions below
// do not call the original DLL or the production integer dyadic operations.
// This is not original world startup, a Tick scheduler or persistence proof.
namespace {
using QuestVr::AdvanceOriginalLevelClock;
using QuestVr::OriginalLevelClockStep;
std::size_t checks{}, refusals{};
void Require(const bool condition, const char* reason) {
    ++checks; if (!condition) throw std::runtime_error(reason);
}
std::uint32_t Bits(const float value) { return std::bit_cast<std::uint32_t>(value); }
std::uint64_t Bits(const double value) { return std::bit_cast<std::uint64_t>(value); }
void Same(const OriginalLevelClockStep& actual, const OriginalLevelClockStep& expected) {
    Require(Bits(actual.timeSeconds) == Bits(expected.timeSeconds), "Level native Double time differs");
    Require(Bits(actual.timeMirror) == Bits(expected.timeMirror), "LevelInfo Float time mirror differs");
    Require(Bits(actual.scaledDelta) == Bits(expected.scaledDelta), "Stored scaled Float delta differs");
    Require(Bits(actual.actorDelta) == Bits(expected.actorDelta), "Post-clock clamped actor delta differs");
}
template<class Operation> void Refuse(Operation operation) {
    bool failed{}; try { operation(); } catch (const std::exception&) { failed = true; }
    Require(failed, "Unsafe native Level clock arithmetic silently succeeded"); ++refusals;
}
void Deterministic() {
    Require(Bits(QuestVr::OriginalLevelMinimumActorDelta) == 0x3ba3d70au &&
        Bits(QuestVr::OriginalLevelMaximumActorDelta) == 0x3ecccccdu, "Retail actor-delta clamp constants changed");
    Same(AdvanceOriginalLevelClock(0.0, 0.0f, 1.0f), {0.0, 0.0f, 0.0f, QuestVr::OriginalLevelMinimumActorDelta});
    Same(AdvanceOriginalLevelClock(2.0, .25f, 2.0f), {2.5, 2.5f, .5f, QuestVr::OriginalLevelMaximumActorDelta});
    Same(AdvanceOriginalLevelClock(2.0, 1.0f, -1.0f), {1.0, 1.0f, -1.0f, QuestVr::OriginalLevelMinimumActorDelta});
    Same(AdvanceOriginalLevelClock(1.0, 1.0f, -1.0f), {0.0, 0.0f, -1.0f, QuestVr::OriginalLevelMinimumActorDelta});
    Same(AdvanceOriginalLevelClock(-0.0, -0.0f, 1.0f), {-0.0, -0.0f, -0.0f, QuestVr::OriginalLevelMinimumActorDelta});
    Same(AdvanceOriginalLevelClock(0.0, -0.0f, 1.0f), {0.0, 0.0f, -0.0f, QuestVr::OriginalLevelMinimumActorDelta});
    Same(AdvanceOriginalLevelClock(-0.0, 0.0f, -1.0f), {-0.0, -0.0f, -0.0f, QuestVr::OriginalLevelMinimumActorDelta});
    Same(AdvanceOriginalLevelClock(-0.0, -0.0f, -1.0f), {0.0, 0.0f, 0.0f, QuestVr::OriginalLevelMinimumActorDelta});
    const auto low = QuestVr::OriginalLevelMinimumActorDelta, high = QuestVr::OriginalLevelMaximumActorDelta;
    for (const auto value : {std::bit_cast<float>(Bits(low) - 1u), low, std::bit_cast<float>(Bits(low) + 1u),
        std::bit_cast<float>(Bits(high) - 1u), high, std::bit_cast<float>(Bits(high) + 1u)}) {
        const auto step = AdvanceOriginalLevelClock(0.0, value, 1.0f);
        const auto actor = value < low ? low : value >= high ? high : value;
        Same(step, {static_cast<double>(value), value, value, actor});
    }
    const auto factor = std::bit_cast<float>(0x3f800001u);
    const auto retainedProduct = 1.0 + std::ldexp(1.0, -22) + std::ldexp(1.0, -46);
    const auto productStep = AdvanceOriginalLevelClock(0.0, factor, factor);
    Same(productStep, {retainedProduct, std::bit_cast<float>(0x3f800002u),
        std::bit_cast<float>(0x3f800002u), high});
    Require(productStep.timeSeconds != static_cast<double>(productStep.scaledDelta),
        "Clock regression does not distinguish retained scaled product from its Float spill");
    // The retained sum lies one PC64 ULP above a Float halfway value. FST
    // Double rounds back to the halfway value, but the later FSTP Float still
    // sees retained ST0 and rounds UP, not the twice-rounded lower endpoint.
    const double halfway = 1.0 + std::ldexp(1.0, -24);
    const auto tinyDelta = std::ldexp(1.0f, -63);
    const auto mirrorStep = AdvanceOriginalLevelClock(halfway, tinyDelta, 1.0f);
    Same(mirrorStep, {halfway, std::bit_cast<float>(0x3f800001u), tinyDelta, low});
    Require(Bits(mirrorStep.timeMirror) != Bits(static_cast<float>(mirrorStep.timeSeconds)),
        "Clock regression does not distinguish retained mirror from Double-to-Float double rounding");
    const auto tiny = std::numeric_limits<float>::denorm_min();
    const auto underflow = AdvanceOriginalLevelClock(0.0, tiny, tiny);
    Same(underflow, {std::ldexp(1.0, -298), 0.0f, 0.0f, low});
    // Float delta underflow does not erase retained native time advance.
    Require(underflow.timeSeconds > 0.0 && underflow.scaledDelta == 0.0f,
        "Stored delta underflow incorrectly froze native time");
    const auto subnormal = std::numeric_limits<double>::denorm_min();
    Same(AdvanceOriginalLevelClock(subnormal, 0.0f, 0.0f), {subnormal, 0.0f, 0.0f, low});
    const auto maximum = std::numeric_limits<float>::max();
    Same(AdvanceOriginalLevelClock(0.0, maximum, 1.0f), {static_cast<double>(maximum), maximum, maximum, high});
}
void Safety() {
    for (const double prior : {-1.0, -std::numeric_limits<double>::denorm_min(),
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        Refuse([&] { static_cast<void>(AdvanceOriginalLevelClock(prior, .01f, 1.0f)); });
    for (const float realDelta : {-1.0f, -std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        Refuse([&] { static_cast<void>(AdvanceOriginalLevelClock(1.0, realDelta, 1.0f)); });
    for (const float dilation : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()})
        Refuse([&] { static_cast<void>(AdvanceOriginalLevelClock(1.0, .01f, dilation)); });
    Refuse([] { static_cast<void>(AdvanceOriginalLevelClock(0.0, 1.0f, -1.0f)); });
    Refuse([] { static_cast<void>(AdvanceOriginalLevelClock(.5, 1.0f, -1.0f)); });
    Refuse([] { static_cast<void>(AdvanceOriginalLevelClock(0.0, std::numeric_limits<float>::max(), 2.0f)); });
    Refuse([] { static_cast<void>(AdvanceOriginalLevelClock(static_cast<double>(std::numeric_limits<float>::max()),
        std::numeric_limits<float>::max(), 1.0f)); });
    Refuse([] { static_cast<void>(AdvanceOriginalLevelClock(std::numeric_limits<double>::max(), 0.0f, 0.0f)); });
    // An overflowing stored delta may not be hidden by a canceling native
    // Double time or the separate actor-delta clamp.
    Refuse([] { static_cast<void>(AdvanceOriginalLevelClock(std::ldexp(1.0, 129),
        std::ldexp(1.0f, 127), -4.0f)); });
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct Precision {
    unsigned short original{};
    Precision() {
        asm volatile("fnstcw %0" : "=m"(original));
        // Ordinary simulation PC64, nearest-even. Keep test overflow and
        // underflow masked; all other original control state is restored.
        const unsigned short mode = static_cast<unsigned short>((original & ~0x0f00u) | 0x033fu);
        asm volatile("fldcw %0" :: "m"(mode));
    }
    ~Precision() { asm volatile("fldcw %0" :: "m"(original)); }
};
OriginalLevelClockStep Oracle(const double prior, const float realDelta, const float dilation) {
    OriginalLevelClockStep result;
    // Mirror the AUDITED instruction/store sequence independently. In
    // particular, neither FST output is reloaded into the retained ST0 chain.
    asm volatile("flds %3; fmuls %4; fsts %2; faddl %5; fstl %0; fstps %1"
        : "=m"(result.timeSeconds), "=m"(result.timeMirror), "=m"(result.scaledDelta)
        : "m"(realDelta), "m"(dilation), "m"(prior) : "st");
    const auto low = std::bit_cast<float>(std::uint32_t{0x3ba3d70au});
    const auto high = std::bit_cast<float>(std::uint32_t{0x3ecccccdu});
    result.actorDelta = result.scaledDelta < low ? low : result.scaledDelta >= high ? high : result.scaledDelta;
    return result;
}
std::uint64_t Next(std::uint64_t& state) { state ^= state << 13u; state ^= state >> 7u; state ^= state << 17u; return state; }
void CheckOracle(const double prior, const float delta, const float dilation) {
    const auto expected = Oracle(prior, delta, dilation);
    const bool safe = expected.timeSeconds >= 0.0 && std::isfinite(expected.timeSeconds) &&
        std::isfinite(expected.timeMirror) && std::isfinite(expected.scaledDelta);
    bool refused{}; OriginalLevelClockStep actual;
    try { actual = AdvanceOriginalLevelClock(prior, delta, dilation); } catch (const std::exception&) { refused = true; }
    Require(refused != safe, "Native clock finite/negative/overflow policy differs from independent x87");
    if (safe) Same(actual, expected); else ++refusals;
}
void X87Oracle() {
    Precision precision;
    const std::array<float, 18u> values{0.0f, -0.0f, std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(), std::ldexp(1.0f, -63), std::bit_cast<float>(0x3ba3d709u),
        std::bit_cast<float>(0x3ba3d70au), std::bit_cast<float>(0x3ba3d70bu), .125f, .25f,
        std::bit_cast<float>(0x3ecccccdu), 1.0f, std::bit_cast<float>(0x3f800001u), 2.0f,
        std::ldexp(1.0f, 64), std::ldexp(1.0f, 127), std::numeric_limits<float>::max(), -1.0f};
    for (const double prior : {0.0, -0.0, std::numeric_limits<double>::denorm_min(), std::ldexp(1.0, -128),
        1.0, 1.0 + std::ldexp(1.0, -24), std::ldexp(1.0, 64), std::numeric_limits<double>::max()})
        for (const float delta : values) if (!(delta < 0.0f))
            for (const float dilation : values) CheckOracle(prior, delta, dilation);
    std::uint64_t state = 0x386c571ae4329dbfull;
    for (unsigned i = 0u; i < 100'000u; ++i) {
        const auto p = Next(state);
        const auto d = static_cast<std::uint32_t>(Next(state)), t = static_cast<std::uint32_t>(Next(state));
        // Broad exponents include legal subnormals and Float-spill overflow.
        const auto priorBits = (p & 0x000fffffffffffffull) | ((p >> 52u) % 2047u << 52u);
        const auto deltaBits = (d & 0x007fffffu) | (((d >> 23u) % 255u) << 23u);
        const auto dilationBits = (t & 0x807fffffu) | (((t >> 23u) % 255u) << 23u);
        CheckOracle(std::bit_cast<double>(priorBits), std::bit_cast<float>(deltaBits), std::bit_cast<float>(dilationBits));
    }
    for (unsigned i = 0u; i < 50'000u; ++i) {
        const auto p = Next(state);
        const auto d = static_cast<std::uint32_t>(Next(state)), t = static_cast<std::uint32_t>(Next(state));
        // Nearby clock/delta scales exercise actual retained additions instead
        // of only obvious huge-time Float-overflow or negligible-delta cases.
        const auto priorBits = (p & 0x000fffffffffffffull) | ((1010ull + (p >> 52u) % 25ull) << 52u);
        const auto deltaBits = (d & 0x007fffffu) | ((115u + (d >> 23u) % 25u) << 23u);
        const auto dilationBits = (t & 0x807fffffu) | ((115u + (t >> 23u) % 25u) << 23u);
        CheckOracle(std::bit_cast<double>(priorBits), std::bit_cast<float>(deltaBits), std::bit_cast<float>(dilationBits));
    }
}
#else
void X87Oracle() { std::cout << "SKIP independent x87 oracle on this non-x86 compiler; deterministic arithmetic controls still run.\n"; }
#endif
} // namespace
int main() {
    try {
        Deterministic(); Safety(); X87Oracle();
        std::cout << "PASS original native Level clock arithmetic: " << checks << " checks, " << refusals
                  << " safety refusals; retained product/sum, independent Float/Double stores and post-clock actor clamp. "
                     "No scheduler, startup or native-clock persistence claim.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Original Level clock arithmetic failed: " << error.what() << '\n'; return 1;
    }
}
