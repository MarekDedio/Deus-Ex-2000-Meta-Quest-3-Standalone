#include "quest_sound_protocol.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::size_t checks{}, refusals{};
using QuestVr::PlaySoundProtocolPacket;
using QuestVr::CheckHearSoundProtocolResult;
using QuestVr::CheckHearSoundProtocolDistanceSquared;

void Require(const bool condition, const char* reason) {
    ++checks;
    if (!condition) throw std::runtime_error(reason);
}
void SameFloat(const float actual, const float expected, const char* reason) {
    Require(std::bit_cast<std::uint32_t>(actual) == std::bit_cast<std::uint32_t>(expected), reason);
}
void SameDouble(const double actual, const double expected, const char* reason) {
    Require(std::bit_cast<std::uint64_t>(actual) == std::bit_cast<std::uint64_t>(expected), reason);
}
template<class Action> void Reject(Action action) {
    bool refused{};
    try { action(); } catch (const std::invalid_argument&) { refused = true; }
    Require(refused, "Malformed protocol input did not fail with the documented portable refusal");
    ++refusals;
}

// Independent scalar oracle: no production helpers/constants, and channel
// arithmetic is widened then explicitly reduced rather than left shifted.
PlaySoundProtocolPacket PacketOracle(const std::uint32_t index, const unsigned slot,
    const float volume, const bool noOverride, const float radius, const float pitch) {
    const auto widened = static_cast<std::uint64_t>(index) * 16u +
        static_cast<std::uint64_t>(slot) * 2u + (noOverride ? 1u : 0u);
    const auto bits = static_cast<std::uint32_t>(widened & 0xffffffffull);
    const auto signedValue = bits < 0x80000000u ? static_cast<std::int64_t>(bits) :
        static_cast<std::int64_t>(bits) - 0x100000000ll;
    const float effectiveRadius = radius > 0.0f ? radius : 800.0f;
    return {static_cast<std::int32_t>(signedValue),
        {{volume * 100.0f, effectiveRadius, pitch * 100.0f}}, effectiveRadius * effectiveRadius};
}
CheckHearSoundProtocolResult HearOracle(const PlaySoundProtocolPacket& packet,
    const CheckHearSoundProtocolDistanceSquared distance, const bool blocked, const bool instigator) {
    CheckHearSoundProtocolResult result;
    result.parameters = packet.parameters;
    result.initialThreshold = packet.squaredRadius * 0.7692308f;
    result.effectiveThreshold = result.initialThreshold;
    result.comparisonThreshold = static_cast<double>(result.initialThreshold);
    if (!(distance.initial < static_cast<long double>(result.initialThreshold))) return result;
    result.initialRangeAccepted = true;
    if (!blocked) result.audible = true;
    else {
        if (!instigator) {
            // Exact two-float product, independent long-double oracle.
            const long double exact = static_cast<long double>(result.initialThreshold) *
                static_cast<long double>(0.6f);
            result.comparisonThreshold = static_cast<double>(exact);
            result.effectiveThreshold = static_cast<float>(exact);
        }
        result.parameters[1u] = packet.parameters[1u] * 0.35f;
        result.audible = static_cast<long double>(distance.secondary) <=
            static_cast<long double>(result.comparisonThreshold);
    }
    return result;
}
CheckHearSoundProtocolResult HearOracle(const PlaySoundProtocolPacket& packet,
    const float distance, const bool blocked, const bool instigator) {
    return HearOracle(packet, CheckHearSoundProtocolDistanceSquared{static_cast<long double>(distance), distance},
        blocked, instigator);
}
void SamePacket(const PlaySoundProtocolPacket& actual, const PlaySoundProtocolPacket& expected) {
    Require(actual.channel == expected.channel, "Channel differs from independent wrapping scalar oracle");
    SameFloat(actual.squaredRadius, expected.squaredRadius, "Squared radius differs from oracle");
    for (std::size_t i = 0u; i < actual.parameters.size(); ++i)
        SameFloat(actual.parameters[i], expected.parameters[i], "Packed sound Vector differs from oracle");
}
void SameHear(const CheckHearSoundProtocolResult& actual, const CheckHearSoundProtocolResult& expected) {
    Require(actual.initialRangeAccepted == expected.initialRangeAccepted,
        "Initial strict range comparison differs from oracle");
    Require(actual.audible == expected.audible, "Final CheckHearSound range decision differs from oracle");
    SameFloat(actual.initialThreshold, expected.initialThreshold, "Initial threshold differs from oracle");
    SameFloat(actual.effectiveThreshold, expected.effectiveThreshold, "Secondary threshold differs from oracle");
    SameDouble(actual.comparisonThreshold, expected.comparisonThreshold,
        "Unrounded secondary comparison threshold differs from independent extended oracle");
    for (std::size_t i = 0u; i < actual.parameters.size(); ++i)
        SameFloat(actual.parameters[i], expected.parameters[i], "Occluded packed Vector differs from oracle");
}

void ChannelWrapping() {
    for (const auto index : std::array<std::uint32_t, 13u>{{0u, 1u, 2u, 0x07ffffffu, 0x08000000u,
        0x0fffffffu, 0x10000000u, 0x7fffffffu, 0x80000000u, 0xfffffff0u,
        0xfffffffeu, 0xffffffffu, 0x12345678u}})
        for (unsigned slot = 0u; slot <= 255u; ++slot)
            for (const bool noOverride : {false, true}) {
                const auto expected = PacketOracle(index, slot, 1.0f, noOverride, 1.0f, 1.0f);
                Require(QuestVr::PlaySoundProtocolChannel(index, static_cast<std::uint8_t>(slot), noOverride)
                    == expected.channel, "Native slot/identity/channel wrap boundary mismatch");
            }
    Require(QuestVr::PlaySoundProtocolChannel(0x08000000u, 0u, false) ==
        std::numeric_limits<std::int32_t>::min(), "Return channel lost sign-bit reinterpretation");
    Require(QuestVr::PlaySoundProtocolChannel(0x0fffffffu, 255u, true) == 495,
        "Final slot/no-override addition failed to wrap DWORD storage");
}

void PacketBoundaries() {
    Require(std::bit_cast<std::uint32_t>(QuestVr::kCheckHearSoundInitialFactor) == 0x3f44ec4fu,
        "CheckHearSound factor lost audited float bit identity");
    for (const float radius : std::array<float, 12u>{{-std::numeric_limits<float>::max(), -800.0f,
        -1.0f, -std::numeric_limits<float>::denorm_min(), -0.0f, 0.0f,
        std::numeric_limits<float>::denorm_min(), 0.125f, 1.0f, 80.0f, 800.0f, 16384.0f}})
        for (const float volume : std::array<float, 6u>{{-2.0f, -0.0f, 0.0f, 0.125f, 1.0f, 4.0f}})
            for (const float pitch : std::array<float, 6u>{{-2.0f, -0.0f, 0.0f, 0.5f, 1.0f, 4.0f}}) {
                const auto actual = QuestVr::PreparePlaySoundProtocolPacket(123u, 1u, volume, false, radius, pitch);
                SamePacket(actual, PacketOracle(123u, 1u, volume, false, radius, pitch));
                if (radius <= 0.0f) SameFloat(actual.parameters[1u], 800.0f,
                    "Nonpositive radius was not replaced by original float800");
            }
    const auto muted = QuestVr::PreparePlaySoundProtocolPacket(7u, 1u, 0.0f, false, 512.0f, 1.0f);
    SameFloat(muted.squaredRadius, 512.0f * 512.0f,
        "Zero volume incorrectly fabricated a special squared radius");
    SameFloat(muted.parameters[0u], 0.0f, "Zero volume changed before event packet delivery");
}

void HearingEdges() {
    const auto packet = QuestVr::PreparePlaySoundProtocolPacket(17u, 3u, 0.75f, true, 800.0f, 1.25f);
    const float initial = packet.squaredRadius * 0.7692308f;
    const float secondary = initial * 0.6f;
    for (const auto distance : std::array<float, 10u>{{0.0f, -0.0f,
        std::nextafter(secondary, 0.0f), secondary, std::nextafter(secondary, initial),
        std::nextafter(initial, 0.0f), initial, std::nextafter(initial, std::numeric_limits<float>::infinity()),
        initial * 2.0f, std::numeric_limits<float>::max()}})
        for (const bool blocked : {false, true})
            for (const bool instigator : {false, true})
                SameHear(QuestVr::EvaluateCheckHearSoundProtocol(packet, distance, blocked, instigator),
                    HearOracle(packet, distance, blocked, instigator));
    for (const bool blocked : {false, true})
        for (const bool instigator : {false, true})
            Require(!QuestVr::EvaluateCheckHearSoundProtocol(packet, initial, blocked, instigator).audible,
                "Equality at initial range must refuse, including source instigator and clear BSP");
    Require(QuestVr::EvaluateCheckHearSoundProtocol(packet, secondary, true, false).audible ==
        (static_cast<double>(secondary) <= static_cast<double>(initial) * static_cast<double>(0.6f)),
        "Rounded diagnostic threshold was substituted for exact secondary comparison");
    Require(!QuestVr::EvaluateCheckHearSoundProtocol(packet, std::nextafter(secondary, initial), true, false).audible,
        "One float beyond secondary range was incorrectly accepted");
    const auto instigator = QuestVr::EvaluateCheckHearSoundProtocol(packet, std::nextafter(initial, 0.0f), true, true);
    Require(instigator.audible, "Source instigator incorrectly received secondary range reduction");
    SameFloat(instigator.parameters[1u], 800.0f * 0.35f, "Instigator exception incorrectly disabled Radius attenuation");
    SameFloat(instigator.parameters[0u], packet.parameters[0u], "Occlusion changed packed Volume");
    SameFloat(instigator.parameters[2u], packet.parameters[2u], "Occlusion changed packed Pitch");
    const auto outside = QuestVr::EvaluateCheckHearSoundProtocol(packet, initial, true, false);
    SameFloat(outside.parameters[1u], packet.parameters[1u],
        "Initial range refusal applied an unexecuted occlusion effect");
    const auto zero = QuestVr::EvaluateCheckHearSoundProtocol({{0.0f, 0.0f, 100.0f}}, 0.0f, 0.0f, false, false);
    Require(!zero.initialRangeAccepted && !zero.audible, "Zero squared radius passed its strict comparison");
    const auto huge = QuestVr::EvaluateCheckHearSoundProtocol({{1.0f, 1.0f, 1.0f}},
        std::numeric_limits<float>::max(), 0.0f, true, false);
    Require(huge.audible && std::isfinite(huge.initialThreshold) && std::isfinite(huge.effectiveThreshold),
        "Largest finite squared radius overflowed attenuation factors");
}

void UnroundedComparisonEdges() {
    bool foundRoundUp{}, foundRoundDown{};
    for (unsigned squared = 1u; squared < 4096u && !(foundRoundUp && foundRoundDown); ++squared) {
        const PlaySoundProtocolPacket packet{0, {{100.0f, 800.0f, 100.0f}}, static_cast<float>(squared)};
        const float initial = packet.squaredRadius * 0.7692308f;
        const long double exact = static_cast<long double>(initial) * static_cast<long double>(0.6f);
        const float rounded = static_cast<float>(exact);
        const bool roundedUp = static_cast<long double>(rounded) > exact;
        const bool roundedDown = static_cast<long double>(rounded) < exact;
        if (!roundedUp && !roundedDown) continue;
        const auto result = QuestVr::EvaluateCheckHearSoundProtocol(packet, rounded, true, false);
        SameHear(result, HearOracle(packet, rounded, true, false));
        Require(result.initialRangeAccepted, "Secondary boundary fixture failed the preceding strict range gate");
        Require(result.audible == roundedDown,
            "Secondary comparison rounded its x87 intermediate early at a float boundary");
        if (roundedUp) foundRoundUp = true;
        if (roundedDown) foundRoundDown = true;
    }
    Require(foundRoundUp && foundRoundDown, "Independent threshold sweep lacks both rounding directions");

    const auto packet = QuestVr::PreparePlaySoundProtocolPacket(1u, 1u, 1.0f, false, 800.0f, 1.0f);
    const float initial = packet.squaredRadius * 0.7692308f;
    const long double below = std::nextafter(static_cast<long double>(initial), 0.0L);
    const long double above = std::nextafter(static_cast<long double>(initial),
        std::numeric_limits<long double>::infinity());
    Require(below < initial && above > initial && static_cast<float>(below) == initial &&
        static_cast<float>(above) == initial, "Split distance boundary fixture does not differ before float storage");
    for (const auto unrounded : {below, static_cast<long double>(initial), above})
        for (const bool blocked : {false, true})
            for (const bool instigator : {false, true}) {
                const CheckHearSoundProtocolDistanceSquared distance{unrounded, initial};
                SameHear(QuestVr::EvaluateCheckHearSoundProtocol(packet, distance, blocked, instigator),
                    HearOracle(packet, distance, blocked, instigator));
            }
    const auto accepted = QuestVr::EvaluateCheckHearSoundProtocol(packet,
        CheckHearSoundProtocolDistanceSquared{below, initial}, true, true);
    Require(accepted.initialRangeAccepted && accepted.audible,
        "Strict unrounded initial distance or inclusive stored instigator threshold was rounded incorrectly");
    Require(!QuestVr::EvaluateCheckHearSoundProtocol(packet, initial, false, true).initialRangeAccepted,
        "Rounded scalar convenience silently fabricated an unrounded geometric distance");
}

void RefusalPolicy() {
    const auto normal = QuestVr::PreparePlaySoundProtocolPacket(1u, 1u, 1.0f, false, 800.0f, 1.0f);
    for (const float invalid : std::array<float, 3u>{{std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}}) {
        Reject([&] { static_cast<void>(QuestVr::PreparePlaySoundProtocolPacket(1u, 1u, invalid, false, 1.0f, 1.0f)); });
        Reject([&] { static_cast<void>(QuestVr::PreparePlaySoundProtocolPacket(1u, 1u, 1.0f, false, invalid, 1.0f)); });
        Reject([&] { static_cast<void>(QuestVr::PreparePlaySoundProtocolPacket(1u, 1u, 1.0f, false, 1.0f, invalid)); });
        Reject([&] { static_cast<void>(QuestVr::EvaluateCheckHearSoundProtocol(normal.parameters, invalid, 0.0f, false, false)); });
        Reject([&] { static_cast<void>(QuestVr::EvaluateCheckHearSoundProtocol(normal, invalid, false, false)); });
        for (std::size_t slot = 0u; slot < 3u; ++slot) {
            auto parameters = normal.parameters;
            parameters[slot] = invalid;
            Reject([&] { static_cast<void>(QuestVr::EvaluateCheckHearSoundProtocol(parameters, 1.0f, 0.0f, false, false)); });
        }
    }
    for (const float negative : std::array<float, 3u>{{-1.0f, -std::numeric_limits<float>::denorm_min(),
        -std::numeric_limits<float>::max()}}) {
        Reject([&] { static_cast<void>(QuestVr::EvaluateCheckHearSoundProtocol(normal.parameters, negative, 0.0f, false, false)); });
        Reject([&] { static_cast<void>(QuestVr::EvaluateCheckHearSoundProtocol(normal, negative, false, false)); });
    }
    for (const float overflow : std::array<float, 2u>{{std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()}}) {
        Reject([&] { static_cast<void>(QuestVr::PreparePlaySoundProtocolPacket(1u, 1u, overflow, false, 1.0f, 1.0f)); });
        Reject([&] { static_cast<void>(QuestVr::PreparePlaySoundProtocolPacket(1u, 1u, 1.0f, false, 1.0f, overflow)); });
    }
    Reject([&] { static_cast<void>(QuestVr::PreparePlaySoundProtocolPacket(1u, 1u, 1.0f, false,
        std::numeric_limits<float>::max(), 1.0f)); });
    for (const auto invalid : std::array<long double, 4u>{{-1.0L,
        std::numeric_limits<long double>::quiet_NaN(), std::numeric_limits<long double>::infinity(),
        -std::numeric_limits<long double>::infinity()}})
        Reject([&] { static_cast<void>(QuestVr::EvaluateCheckHearSoundProtocol(normal,
            CheckHearSoundProtocolDistanceSquared{invalid, 0.0f}, false, false)); });
    for (const auto invalid : std::array<float, 4u>{{-1.0f, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}})
        Reject([&] { static_cast<void>(QuestVr::EvaluateCheckHearSoundProtocol(normal,
            CheckHearSoundProtocolDistanceSquared{0.0L, invalid}, true, false)); });
    // Rejection receives only values/const references. No caller-owned packet
    // or backend state can have partially changed when a guard refuses.
    const auto after = QuestVr::EvaluateCheckHearSoundProtocol(normal, 0.0f, false, false);
    SamePacket(normal, PacketOracle(1u, 1u, 1.0f, false, 800.0f, 1.0f));
    SameHear(after, HearOracle(normal, 0.0f, false, false));
}

std::uint32_t Next(std::uint32_t& value) {
    value ^= value << 13u; value ^= value >> 17u; value ^= value << 5u;
    return value;
}
void DeterministicSweep() {
    std::uint32_t state = 0x95c3a176u;
    for (std::size_t iteration = 0u; iteration < 25000u; ++iteration) {
        const auto index = Next(state);
        const auto slot = static_cast<std::uint8_t>(Next(state));
        const bool noOverride = (Next(state) & 1u) != 0u;
        const float volume = (static_cast<int>(Next(state) % 8193u) - 4096) / 32.0f;
        const float radius = (static_cast<int>(Next(state) % 65537u) - 4096) / 4.0f;
        const float pitch = (static_cast<int>(Next(state) % 4097u) - 2048) / 64.0f;
        const auto expected = PacketOracle(index, slot, volume, noOverride, radius, pitch);
        const auto actual = QuestVr::PreparePlaySoundProtocolPacket(index, slot, volume, noOverride, radius, pitch);
        SamePacket(actual, expected);
        const float distance = expected.squaredRadius * (static_cast<float>(Next(state) % 2049u) / 1024.0f);
        for (const bool blocked : {false, true})
            for (const bool instigator : {false, true}) {
                const auto hear = QuestVr::EvaluateCheckHearSoundProtocol(actual, distance, blocked, instigator);
                SameHear(hear, HearOracle(expected, distance, blocked, instigator));
                SameHear(QuestVr::EvaluateCheckHearSoundProtocol(actual.parameters, actual.squaredRadius,
                    distance, blocked, instigator), hear);
            }
        SamePacket(actual, expected); // Listening must not mutate the shared source packet.
    }
}
} // namespace

int main() {
    try {
        ChannelWrapping(); PacketBoundaries(); HearingEdges(); UnroundedComparisonEdges(); RefusalPolicy(); DeterministicSweep();
        std::cout << "PASS retail sound packet scalar protocol: " << checks << " checks, " << refusals
            << " portable safety refusals; stable-index DWORD channel, radius normalization, packed Vector,"
               " split unrounded/stored distance, exact secondary product, strict initial/inclusive occluded thresholds,"
               " instigator exception. No geometric x87/native dispatch/audio/crate claim.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Sound protocol test failed: " << error.what() << '\n';
        return 1;
    }
}
