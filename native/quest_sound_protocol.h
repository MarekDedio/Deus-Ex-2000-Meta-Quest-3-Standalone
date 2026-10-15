#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace QuestVr {

// Pure scalar packet preparation from the read-only retail Engine.dll audit
// (execPlaySound 0x103e1f60, CheckHearSound 0x103e1b00). This is NOT a native
// implementation, a recipient selector, an audio queue or an audible backend.
// The adapter must evaluate the original arguments/defaults in their original
// order, preserve Sound=None's no-write return, provide a stable UObject index
// and dispatch the actual ClientHearSound event synchronously inside the root
// transaction. Only external audio delivery publishes after successful commit.
// Object indices must never be derived from a host pointer.
struct PlaySoundProtocolPacket {
    std::int32_t channel{};
    // Original Parameters Vector: X=Volume*100, Y=Radius, Z=Pitch*100.
    std::array<float, 3u> parameters{};
    float squaredRadius{};
};

namespace SoundProtocolDetail {
inline void RequireFinite(const float value, const char* reason) {
    if (!std::isfinite(value)) throw std::invalid_argument(reason);
}
inline void ValidateParameters(const std::array<float, 3u>& parameters) {
    for (const auto value : parameters)
        RequireFinite(value, "Sound protocol packed parameters must be finite");
}
} // namespace SoundProtocolDetail

inline std::int32_t PlaySoundProtocolChannel(const std::uint32_t stableObjectIndex,
    const std::uint8_t slot, const bool noOverride) noexcept {
    // Retail DWORD arithmetic deliberately wraps, including the addition.
    // No smaller object-index ceiling was established by the native audit.
    const std::uint32_t bits = (stableObjectIndex << 4u) +
        static_cast<std::uint32_t>(slot) * 2u + static_cast<std::uint32_t>(noOverride);
    return std::bit_cast<std::int32_t>(bits);
}

inline PlaySoundProtocolPacket PreparePlaySoundProtocolPacket(
    const std::uint32_t stableObjectIndex, const std::uint8_t slot,
    const float volume, const bool noOverride, float radius, const float pitch) {
    using SoundProtocolDetail::RequireFinite;
    // Finite/overflow refusals are explicit portable safety policy, not
    // invented clamps or a claim that retail guarded its x87 arithmetic.
    RequireFinite(volume, "Sound protocol volume must be finite");
    RequireFinite(radius, "Sound protocol radius must be finite");
    RequireFinite(pitch, "Sound protocol pitch must be finite");
    if (radius <= 0.0f) radius = 800.0f;
    PlaySoundProtocolPacket packet{PlaySoundProtocolChannel(stableObjectIndex, slot, noOverride),
        {{volume * 100.0f, radius, pitch * 100.0f}}, radius * radius};
    SoundProtocolDetail::ValidateParameters(packet.parameters);
    RequireFinite(packet.squaredRadius, "Sound protocol squared radius overflow");
    return packet;
}

struct CheckHearSoundProtocolResult {
    bool initialRangeAccepted{};
    bool audible{};
    // Retain both thresholds so adapters/tests cannot confuse the strict
    // first check with the inclusive occluded secondary check.
    float initialThreshold{};
    // Diagnostic float only. Retail keeps the secondary multiplication in
    // x87 precision until comparison, so this rounded value must NOT gate it.
    float effectiveThreshold{};
    double comparisonThreshold{};
    std::array<float, 3u> parameters{};
};

struct CheckHearSoundProtocolDistanceSquared {
    // Caller supplies the original unrounded geometric sum for the strict
    // first comparison and its later stored float for the occluded check.
    // This helper does not reconstruct retail x87 coordinate arithmetic.
    long double initial{};
    float secondary{};
};

inline constexpr float kCheckHearSoundInitialFactor =
    std::bit_cast<float>(std::uint32_t{0x3f44ec4fu});

inline CheckHearSoundProtocolResult EvaluateCheckHearSoundProtocol(
    const std::array<float, 3u>& packedParameters, const float squaredRadius,
    const CheckHearSoundProtocolDistanceSquared distanceSquared,
    const bool bspBlocked, const bool listenerIsSourceOwner) {
    using SoundProtocolDetail::RequireFinite;
    SoundProtocolDetail::ValidateParameters(packedParameters);
    RequireFinite(squaredRadius, "CheckHearSound squared radius must be finite");
    if (!std::isfinite(distanceSquared.initial))
        throw std::invalid_argument("CheckHearSound initial squared distance must be finite");
    RequireFinite(distanceSquared.secondary, "CheckHearSound stored squared distance must be finite");
    if (squaredRadius < 0.0f || distanceSquared.initial < 0.0L || distanceSquared.secondary < 0.0f)
        throw std::invalid_argument("CheckHearSound squared inputs must be nonnegative");

    CheckHearSoundProtocolResult result;
    result.parameters = packedParameters;
    result.initialThreshold = squaredRadius * kCheckHearSoundInitialFactor;
    result.effectiveThreshold = result.initialThreshold;
    result.comparisonThreshold = static_cast<double>(result.initialThreshold);
    result.initialRangeAccepted = distanceSquared.initial < static_cast<long double>(result.initialThreshold);
    if (!result.initialRangeAccepted) return result;
    if (!bspBlocked) {
        result.audible = true;
        return result;
    }
    // Owning the source exempts the listener from secondary range reduction,
    // NOT from the packed Radius attenuation. Volume/Pitch remain unchanged.
    if (!listenerIsSourceOwner) {
        // A product of two binary32 values is exactly representable in
        // binary64. Do not round to float before comparing the stored
        // distance: retail FLD/FMUL does not store this intermediate.
        result.comparisonThreshold *= static_cast<double>(0.6f);
        result.effectiveThreshold = static_cast<float>(result.comparisonThreshold);
    }
    result.parameters[1u] *= 0.35f;
    result.audible = static_cast<double>(distanceSquared.secondary) <= result.comparisonThreshold;
    return result;
}

inline CheckHearSoundProtocolResult EvaluateCheckHearSoundProtocol(
    const std::array<float, 3u>& packedParameters, const float squaredRadius,
    const float distanceSquared, const bool bspBlocked, const bool listenerIsSourceOwner) {
    // Convenience for an already-rounded caller-owned scalar. It cannot
    // establish equivalence to retail's unrounded initial geometric sum.
    return EvaluateCheckHearSoundProtocol(packedParameters, squaredRadius,
        CheckHearSoundProtocolDistanceSquared{static_cast<long double>(distanceSquared), distanceSquared},
        bspBlocked, listenerIsSourceOwner);
}

inline CheckHearSoundProtocolResult EvaluateCheckHearSoundProtocol(
    const PlaySoundProtocolPacket& packet, const CheckHearSoundProtocolDistanceSquared distanceSquared,
    const bool bspBlocked, const bool listenerIsSourceOwner) {
    return EvaluateCheckHearSoundProtocol(packet.parameters, packet.squaredRadius,
        distanceSquared, bspBlocked, listenerIsSourceOwner);
}

inline CheckHearSoundProtocolResult EvaluateCheckHearSoundProtocol(
    const PlaySoundProtocolPacket& packet, const float distanceSquared,
    const bool bspBlocked, const bool listenerIsSourceOwner) {
    return EvaluateCheckHearSoundProtocol(packet.parameters, packet.squaredRadius,
        distanceSquared, bspBlocked, listenerIsSourceOwner);
}

} // namespace QuestVr
