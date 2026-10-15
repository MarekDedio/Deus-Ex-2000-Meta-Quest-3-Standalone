#pragma once

#include "quest_held_item_visual.h"
#include "quest_quick_belt.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace QuestVr {

// Detached fingerprint of already decoded original artwork. It owns no runtime
// actor and performs no asset access. Skin/MultiSkins retain actual None as an
// empty string; unavailable typed metadata is not interchangeable with None.
struct PreparedHeldVisualKey {
    std::string classPath, playerViewMesh;
    float playerViewScale{};
    std::string skin;
    std::array<std::string,8u> multiSkins;
    bool supportsNativeStarterKeyRing{};
};

enum class EquippedItemVisualSource : std::uint8_t {
    None, CollectedActor, NativeStarterKeyRing
};
enum class EquippedItemVisualStatus : std::uint8_t {
    Available, Empty, InvalidIdentity, DescriptorUnavailable, StaleDescriptor,
    InvalidDescriptor, UnsupportedItem, PreparedAssetMismatch,
    InvalidPreparedAsset, AmbiguousPreparedAsset, ResourceLimit
};
struct EquippedItemVisualSelection {
    EquippedItemVisualStatus status{EquippedItemVisualStatus::Empty};
    std::optional<std::size_t> preparedIndex;
    EquippedItemVisualSource source{EquippedItemVisualSource::None};
};
struct EquippedItemVisualLimits {
    std::size_t preparedAssets{64u}, stringBytes{4096u}, inspectedBytes{256u*1024u};
};

inline constexpr std::string_view kStarterKeyRingOriginalClass = "DeusEx.NanoKeyRing";
inline constexpr std::string_view kStarterKeyRingOriginalPovMesh = "DeusExItems.NanoKeyRingPOV";

namespace EquippedItemVisualDetail {
inline bool Same(std::string_view first, std::string_view second) {
    if (first.size()!=second.size()) return false;
    for (std::size_t index=0u;index<first.size();++index) {
        const auto fold=[](unsigned char value) {
            return value>='A' && value<='Z' ? static_cast<unsigned char>(value-'A'+'a') : value;
        };
        if (fold(static_cast<unsigned char>(first[index]))!=
            fold(static_cast<unsigned char>(second[index]))) return false;
    }
    return true;
}
inline bool Text(std::string_view value, std::size_t limit, bool empty=false) {
    return (empty || !value.empty()) && value.size()<=limit &&
        value.find('\0')==std::string_view::npos;
}
template<class Value> inline bool Available(const PortableHeldItemValue<Value>& value) {
    return value.status==PortableHeldItemValueStatus::Available && value.value.has_value();
}
inline bool LiveCollectedIdentity(std::string_view equipped,
    const PortableHeldItemVisualDescriptor& descriptor, std::size_t stringBytes) {
    return Text(equipped,stringBytes) && Text(descriptor.requestedPath,stringBytes) &&
        Text(descriptor.actorPath,stringBytes) && Text(descriptor.classPath,stringBytes) &&
        descriptor.status==PortableHeldItemDescriptorStatus::Available && descriptor.collected &&
        Same(equipped,descriptor.requestedPath) && Same(equipped,descriptor.actorPath);
}
}

// Allocation-free bounded selection only. Caller must obtain the current live
// equipment identity after inventory reconciliation; cached actor descriptors
// are invalidated on map/state publication, not merely when its spelling changes.
// A failure always returns no prepared index: it must never keep a stale model.
inline EquippedItemVisualSelection SelectEquippedItemVisual(
    std::string_view equippedIdentity,
    const PortableHeldItemVisualDescriptor* descriptor,
    std::span<const PreparedHeldVisualKey> prepared,
    const EquippedItemVisualLimits& limits={}) {
    using Status=EquippedItemVisualStatus;
    using Source=EquippedItemVisualSource;
    const auto fail=[](Status status) { return EquippedItemVisualSelection{status,{},Source::None}; };
    if (limits.preparedAssets==0u || limits.preparedAssets>256u ||
        limits.stringBytes==0u || limits.stringBytes>8192u ||
        limits.inspectedBytes==0u || limits.inspectedBytes>4u*1024u*1024u ||
        prepared.size()>limits.preparedAssets) return fail(Status::ResourceLimit);
    if (equippedIdentity.empty()) return fail(Status::Empty);
    if (!EquippedItemVisualDetail::Text(equippedIdentity,limits.stringBytes))
        return fail(Status::InvalidIdentity);
    std::size_t inspected{};
    const auto charge=[&](std::string_view value) {
        if (value.size()>limits.inspectedBytes || inspected>limits.inspectedBytes-value.size()) return false;
        inspected+=value.size(); return true;
    };
    if (!charge(equippedIdentity)) return fail(Status::ResourceLimit);
    const bool nativeRing=EquippedItemVisualDetail::Same(equippedIdentity,kQuickBeltKeyRingIdentity);
    if (!nativeRing) {
        if (!descriptor || descriptor->status!=PortableHeldItemDescriptorStatus::Available)
            return fail(Status::DescriptorUnavailable);
        if (!EquippedItemVisualDetail::Text(descriptor->requestedPath,limits.stringBytes) ||
            !EquippedItemVisualDetail::Text(descriptor->actorPath,limits.stringBytes) ||
            !EquippedItemVisualDetail::Text(descriptor->classPath,limits.stringBytes) || !descriptor->collected)
            return fail(Status::InvalidDescriptor);
        if (!EquippedItemVisualDetail::Same(equippedIdentity,descriptor->requestedPath) ||
            !EquippedItemVisualDetail::Same(equippedIdentity,descriptor->actorPath))
            return fail(Status::StaleDescriptor);
        if (!charge(descriptor->requestedPath) || !charge(descriptor->actorPath) || !charge(descriptor->classPath))
            return fail(Status::ResourceLimit);
        if (!EquippedItemVisualDetail::Available(descriptor->playerViewMesh) ||
            !EquippedItemVisualDetail::Available(descriptor->playerViewScale) ||
            !EquippedItemVisualDetail::Available(descriptor->skin)) return fail(Status::InvalidDescriptor);
        if (!EquippedItemVisualDetail::Text(*descriptor->playerViewMesh.value,limits.stringBytes,true) ||
            !EquippedItemVisualDetail::Text(*descriptor->skin.value,limits.stringBytes,true) ||
            !std::isfinite(*descriptor->playerViewScale.value) || *descriptor->playerViewScale.value<=0.0f)
            return fail(Status::InvalidDescriptor);
        if (!charge(*descriptor->playerViewMesh.value) || !charge(*descriptor->skin.value))
            return fail(Status::ResourceLimit);
        for (const auto& field:descriptor->multiSkins) {
            if (!EquippedItemVisualDetail::Available(field) ||
                !EquippedItemVisualDetail::Text(*field.value,limits.stringBytes,true)) return fail(Status::InvalidDescriptor);
            if (!charge(*field.value)) return fail(Status::ResourceLimit);
        }
        if (descriptor->playerViewMesh.value->empty()) return fail(Status::UnsupportedItem);
    }
    std::optional<std::size_t> selected;
    bool supportedClass{};
    for (std::size_t index=0u;index<prepared.size();++index) {
        const auto& key=prepared[index];
        if (!EquippedItemVisualDetail::Text(key.classPath,limits.stringBytes) ||
            !EquippedItemVisualDetail::Text(key.playerViewMesh,limits.stringBytes) ||
            !EquippedItemVisualDetail::Text(key.skin,limits.stringBytes,true) ||
            !std::isfinite(key.playerViewScale) || key.playerViewScale<=0.0f)
            return fail(Status::InvalidPreparedAsset);
        if (!charge(key.classPath) || !charge(key.playerViewMesh) || !charge(key.skin))
            return fail(Status::ResourceLimit);
        for (const auto& skin:key.multiSkins) {
            if (!EquippedItemVisualDetail::Text(skin,limits.stringBytes,true)) return fail(Status::InvalidPreparedAsset);
            if (!charge(skin)) return fail(Status::ResourceLimit);
        }
        if (key.supportsNativeStarterKeyRing &&
            (!EquippedItemVisualDetail::Same(key.classPath,kStarterKeyRingOriginalClass) ||
             !EquippedItemVisualDetail::Same(key.playerViewMesh,kStarterKeyRingOriginalPovMesh)))
            return fail(Status::InvalidPreparedAsset);
        bool match{};
        if (nativeRing) {
            supportedClass=supportedClass || key.supportsNativeStarterKeyRing;
            match=key.supportsNativeStarterKeyRing;
        } else if (EquippedItemVisualDetail::Same(key.classPath,descriptor->classPath)) {
            supportedClass=true;
            match=EquippedItemVisualDetail::Same(key.playerViewMesh,*descriptor->playerViewMesh.value) &&
                key.playerViewScale==*descriptor->playerViewScale.value &&
                EquippedItemVisualDetail::Same(key.skin,*descriptor->skin.value);
            for (std::size_t slot=0u;slot<key.multiSkins.size() && match;++slot)
                match=EquippedItemVisualDetail::Same(key.multiSkins[slot],*descriptor->multiSkins[slot].value);
        }
        if (!match) continue;
        if (selected) return fail(Status::AmbiguousPreparedAsset);
        selected=index;
    }
    if (!selected) return fail(supportedClass ? Status::PreparedAssetMismatch : Status::UnsupportedItem);
    return {Status::Available,selected,nativeRing ? Source::NativeStarterKeyRing : Source::CollectedActor};
}

struct EquippedMeleeProfile {
    std::int32_t hitDamageOriginal{}, accurateRangeOriginal{}, maxRangeOriginal{};
    bool operator==(const EquippedMeleeProfile&) const=default;
};

// Original typed fields only, not executed Fire/TraceFire behavior or skill
// multipliers. Keep Unreal range units intact; world conversion belongs to the
// caller. Firearms and the native keyring deliberately have no melee profile.
inline std::optional<EquippedMeleeProfile> ResolveEquippedMeleeProfile(
    std::string_view equippedIdentity, const PortableHeldItemVisualDescriptor* descriptor) {
    using namespace EquippedItemVisualDetail;
    if (!descriptor || Same(equippedIdentity,kQuickBeltKeyRingIdentity) ||
        !LiveCollectedIdentity(equippedIdentity,*descriptor,4096u) || !descriptor->weapon ||
        !Available(descriptor->handToHand) || !*descriptor->handToHand.value ||
        !Available(descriptor->hitDamage) || !Available(descriptor->accurateRange) ||
        !Available(descriptor->maxRange) || *descriptor->hitDamage.value<=0 ||
        *descriptor->accurateRange.value<0 || *descriptor->maxRange.value<=0) return {};
    return EquippedMeleeProfile{*descriptor->hitDamage.value,*descriptor->accurateRange.value,*descriptor->maxRange.value};
}

} // namespace QuestVr
