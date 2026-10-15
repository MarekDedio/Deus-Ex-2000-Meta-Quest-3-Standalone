#include "quest_equipped_item_visual.h"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
using namespace QuestVr;
std::size_t checks{};
void Require(bool value,const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class Value> PortableHeldItemValue<Value> Field(Value value) {
    PortableHeldItemValue<Value> result;
    result.status=PortableHeldItemValueStatus::Available;
    result.value=std::move(value);
    return result;
}
PreparedHeldVisualKey Ring() {
    PreparedHeldVisualKey result;
    result.classPath=std::string(kStarterKeyRingOriginalClass);
    result.playerViewMesh=std::string(kStarterKeyRingOriginalPovMesh);
    result.playerViewScale=0.25f;
    result.supportsNativeStarterKeyRing=true;
    return result;
}
PreparedHeldVisualKey Crowbar() {
    PreparedHeldVisualKey result;
    result.classPath="DeusEx.WeaponCrowbar";
    result.playerViewMesh="DeusExItems.Crowbar";
    result.playerViewScale=0.75f;
    result.skin="DeusExItems.Skins.TestOriginalCrowbar";
    result.multiSkins[2]="DeusExItems.Skins.TestOriginalGlove";
    return result;
}
PortableHeldItemVisualDescriptor Descriptor(const PreparedHeldVisualKey& key=Crowbar(),
    std::string identity="00_Training.WeaponCrowbar1") {
    PortableHeldItemVisualDescriptor result;
    result.status=PortableHeldItemDescriptorStatus::Available;
    result.requestedPath=identity;
    result.actorPath=std::move(identity);
    result.classPath=key.classPath;
    result.collected=true;
    // Collected world actors are inactive. Never equate world-hidden to not owned.
    result.active=false;
    result.weapon=true;
    result.playerViewMesh=Field(key.playerViewMesh);
    result.playerViewScale=Field(key.playerViewScale);
    result.skin=Field(key.skin);
    for (std::size_t slot=0u;slot<key.multiSkins.size();++slot)
        result.multiSkins[slot]=Field(key.multiSkins[slot]);
    result.hitDamage=Field<std::int32_t>(6);
    result.accurateRange=Field<std::int32_t>(80);
    result.maxRange=Field<std::int32_t>(80);
    result.handToHand=Field(true);
    return result;
}
void Expect(const EquippedItemVisualSelection& result,EquippedItemVisualStatus expected) {
    Require(result.status==expected,"Selection status differs from expected refusal/success");
    if (expected!=EquippedItemVisualStatus::Available) {
        Require(!result.preparedIndex,"Refusal retained a stale held model index");
        Require(result.source==EquippedItemVisualSource::None,"Refusal claims an attachment source");
    }
}
void NormalSelection() {
    const std::array keys{Ring(),Crowbar()};
    auto descriptor=Descriptor();
    auto selected=SelectEquippedItemVisual(descriptor.actorPath,&descriptor,keys);
    Expect(selected,EquippedItemVisualStatus::Available);
    Require(selected.preparedIndex==1u && selected.source==EquippedItemVisualSource::CollectedActor,
        "Actual collected crowbar selected wrong prepared model/source");
    Require(!descriptor.active,"Fixture no longer covers inactive collected world actor");
    descriptor.requestedPath="00_training.weaponcrowbar1";
    descriptor.actorPath="00_TRAINING.WEAPONCROWBAR1";
    descriptor.classPath="DEUSEX.WEAPONCROWBAR";
    *descriptor.playerViewMesh.value="DEUSEXITEMS.CROWBAR";
    *descriptor.skin.value="DEUSEXITEMS.SKINS.TESTORIGINALCROWBAR";
    *descriptor.multiSkins[2].value="DEUSEXITEMS.SKINS.TESTORIGINALGLOVE";
    selected=SelectEquippedItemVisual("00_Training.WeaponCrowbar1",&descriptor,keys);
    Expect(selected,EquippedItemVisualStatus::Available);
    Require(selected.preparedIndex==1u,"Original case-insensitive identities did not match");
    selected=SelectEquippedItemVisual("questvr.nanokeyring",nullptr,keys);
    Expect(selected,EquippedItemVisualStatus::Available);
    Require(selected.preparedIndex==0u && selected.source==EquippedItemVisualSource::NativeStarterKeyRing,
        "Native starter ring requires a fabricated original actor descriptor");
    descriptor.status=PortableHeldItemDescriptorStatus::MalformedIdentity;
    selected=SelectEquippedItemVisual(kQuickBeltKeyRingIdentity,&descriptor,keys);
    Expect(selected,EquippedItemVisualStatus::Available);
    Require(selected.source==EquippedItemVisualSource::NativeStarterKeyRing,
        "Native keyring incorrectly interpreted a supplied UObject descriptor");
    auto actualRing=Descriptor(keys[0],"00_Training.NanoKeyRing7");
    actualRing.weapon=false;
    selected=SelectEquippedItemVisual(actualRing.actorPath,&actualRing,keys);
    Expect(selected,EquippedItemVisualStatus::Available);
    Require(selected.source==EquippedItemVisualSource::CollectedActor,
        "Genuine collected ring was mislabeled a native synthetic component");
}
void DescriptorRefusals() {
    const std::array keys{Ring(),Crowbar()};
    const auto original=Descriptor();
    using Status=EquippedItemVisualStatus;
    Expect(SelectEquippedItemVisual("",nullptr,keys),Status::Empty);
    Expect(SelectEquippedItemVisual("unindexed",nullptr,keys),Status::DescriptorUnavailable);
    Expect(SelectEquippedItemVisual(std::string(4097u,'x'),&original,keys),Status::InvalidIdentity);
    Expect(SelectEquippedItemVisual(std::string("id\0tail",7u),&original,keys),Status::InvalidIdentity);
    for (auto status:{PortableHeldItemDescriptorStatus::InvalidIdentity,
        PortableHeldItemDescriptorStatus::RuntimeUnavailable,PortableHeldItemDescriptorStatus::UnindexedIdentity,
        PortableHeldItemDescriptorStatus::NotInventoryActor,PortableHeldItemDescriptorStatus::NotCollected,
        PortableHeldItemDescriptorStatus::MalformedIdentity}) {
        auto descriptor=original; descriptor.status=status;
        Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::DescriptorUnavailable);
    }
    for (unsigned field=0u;field<3u;++field) {
        auto descriptor=original;
        auto& text=field==0u ? descriptor.requestedPath : field==1u ? descriptor.actorPath : descriptor.classPath;
        text.clear();
        Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::InvalidDescriptor);
        text=std::string(4097u,'z');
        Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::InvalidDescriptor);
        text=std::string("invalid\0identity",16u);
        Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::InvalidDescriptor);
    }
    auto descriptor=original; descriptor.collected=false;
    Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::InvalidDescriptor);
    descriptor=original; descriptor.actorPath="00_Training.WeaponCrowbar2";
    Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::StaleDescriptor);
    descriptor=original; descriptor.requestedPath="00_Training.WeaponCombatKnife0";
    Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::StaleDescriptor);
    descriptor=original; descriptor.classPath="Mod.WeaponCrowbar";
    Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::UnsupportedItem);
    descriptor=original; *descriptor.playerViewMesh.value="DeusExItems.DifferentPOV";
    Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::PreparedAssetMismatch);
    descriptor=original; *descriptor.playerViewMesh.value="";
    descriptor.pickupViewMesh=Field(keys[1].playerViewMesh);
    descriptor.thirdPersonMesh=Field(keys[1].playerViewMesh);
    Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::UnsupportedItem);
    for (auto value:{0.0f,-1.0f,std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        descriptor=original; *descriptor.playerViewScale.value=value;
        Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::InvalidDescriptor);
    }
    descriptor=original; *descriptor.playerViewScale.value=0.5f;
    Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::PreparedAssetMismatch);
    for (auto status:{PortableHeldItemValueStatus::Unavailable,PortableHeldItemValueStatus::MissingPropertyMetadata,
        PortableHeldItemValueStatus::MalformedProperty,PortableHeldItemValueStatus::InvalidValue,
        PortableHeldItemValueStatus::MissingReferencedObject}) {
        for (unsigned field=0u;field<11u;++field) {
            descriptor=original;
            if (field==0u) descriptor.playerViewScale.status=status;
            else (field==1u ? descriptor.playerViewMesh : field==2u ? descriptor.skin :
                descriptor.multiSkins[field-3u]).status=status;
            Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::InvalidDescriptor);
        }
    }
    for (unsigned field=0u;field<11u;++field) {
        descriptor=original;
        if (field==0u) descriptor.playerViewScale.value.reset();
        else (field==1u ? descriptor.playerViewMesh : field==2u ? descriptor.skin :
            descriptor.multiSkins[field-3u]).value.reset();
        Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::InvalidDescriptor);
    }
    for (unsigned field=0u;field<9u;++field) {
        descriptor=original;
        auto& value=field==0u ? descriptor.skin : descriptor.multiSkins[field-1u];
        *value.value="Other.ActualTexture";
        Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::PreparedAssetMismatch);
        *value.value=std::string(4097u,'z');
        Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::InvalidDescriptor);
        *value.value=std::string("bad\0texture",11u);
        Expect(SelectEquippedItemVisual(original.actorPath,&descriptor,keys),Status::InvalidDescriptor);
    }
}
void CatalogAndBudgets() {
    using Status=EquippedItemVisualStatus;
    const auto descriptor=Descriptor();
    const std::vector<PreparedHeldVisualKey> empty;
    Expect(SelectEquippedItemVisual(descriptor.actorPath,&descriptor,empty),Status::UnsupportedItem);
    std::vector keys{Ring(),Crowbar()};
    keys.push_back(keys.back());
    Expect(SelectEquippedItemVisual(descriptor.actorPath,&descriptor,keys),Status::AmbiguousPreparedAsset);
    keys={Ring(),Ring()};
    Expect(SelectEquippedItemVisual(kQuickBeltKeyRingIdentity,nullptr,keys),Status::AmbiguousPreparedAsset);
    keys={Crowbar()};
    Expect(SelectEquippedItemVisual(kQuickBeltKeyRingIdentity,nullptr,keys),Status::UnsupportedItem);
    keys[0].supportsNativeStarterKeyRing=true;
    Expect(SelectEquippedItemVisual(kQuickBeltKeyRingIdentity,nullptr,keys),Status::InvalidPreparedAsset);
    for (unsigned field=0u;field<12u;++field) {
        keys={Ring(),Crowbar()};
        if (field==0u) keys[1].classPath.clear();
        else if (field==1u) keys[1].playerViewMesh.clear();
        else if (field==2u) keys[1].playerViewScale=std::numeric_limits<float>::quiet_NaN();
        else if (field==3u) keys[1].skin=std::string(4097u,'x');
        else keys[1].multiSkins[field-4u]=std::string("bad\0texture",11u);
        Expect(SelectEquippedItemVisual(descriptor.actorPath,&descriptor,keys),Status::InvalidPreparedAsset);
    }
    keys={Ring(),Crowbar()};
    for (auto limits:{EquippedItemVisualLimits{0u,4096u,256u*1024u},
        EquippedItemVisualLimits{257u,4096u,256u*1024u},EquippedItemVisualLimits{1u,4096u,256u*1024u},
        EquippedItemVisualLimits{64u,0u,256u*1024u},EquippedItemVisualLimits{64u,8193u,256u*1024u},
        EquippedItemVisualLimits{64u,4096u,0u},EquippedItemVisualLimits{64u,4096u,4u*1024u*1024u+1u},
        EquippedItemVisualLimits{64u,4096u,1u},EquippedItemVisualLimits{64u,4096u,120u}})
        Expect(SelectEquippedItemVisual(descriptor.actorPath,&descriptor,keys,limits),Status::ResourceLimit);
    std::size_t minimum{};
    for (std::size_t budget=1u;budget<4096u;++budget) {
        const auto selected=SelectEquippedItemVisual(descriptor.actorPath,&descriptor,keys,{64u,4096u,budget});
        if (selected.status==Status::Available) { minimum=budget; break; }
        Expect(selected,Status::ResourceLimit);
    }
    Require(minimum>120u && minimum<4096u,"Inspection budget boundary was not reached within fixture bounds");
    Expect(SelectEquippedItemVisual(descriptor.actorPath,&descriptor,keys,{64u,4096u,minimum-1u}),Status::ResourceLimit);
    Expect(SelectEquippedItemVisual(descriptor.actorPath,&descriptor,keys,{64u,4096u,minimum}),Status::Available);
    auto maximumKey=Crowbar();
    maximumKey.classPath=std::string(4096u,'c');
    maximumKey.playerViewMesh=std::string(4096u,'m');
    maximumKey.skin=std::string(4096u,'s');
    for (auto& skin:maximumKey.multiSkins) skin=std::string(4096u,'t');
    auto maximumDescriptor=Descriptor(maximumKey,std::string(4096u,'a'));
    const std::array maximumKeys{maximumKey};
    Expect(SelectEquippedItemVisual(maximumDescriptor.actorPath,&maximumDescriptor,maximumKeys),Status::Available);
    keys.assign(65u,Crowbar());
    Expect(SelectEquippedItemVisual(descriptor.actorPath,&descriptor,keys),Status::ResourceLimit);
}
void Melee() {
    auto descriptor=Descriptor();
    auto profile=ResolveEquippedMeleeProfile(descriptor.actorPath,&descriptor);
    Require(profile==EquippedMeleeProfile{6,80,80},"Melee substituted name-based damage or metre ranges");
    Require(!ResolveEquippedMeleeProfile(descriptor.actorPath,nullptr),"Absent descriptor fabricated a melee profile");
    Require(!ResolveEquippedMeleeProfile(kQuickBeltKeyRingIdentity,&descriptor),"Native keyring fabricated original weapon fields");
    Require(!ResolveEquippedMeleeProfile("Other.CollectedCrowbar",&descriptor),"Stale descriptor granted another actor melee damage");
    const auto original=descriptor;
    for (unsigned field=0u;field<9u;++field) {
        descriptor=original;
        if (field==0u) descriptor.status=PortableHeldItemDescriptorStatus::NotCollected;
        else if (field==1u) descriptor.collected=false;
        else if (field==2u) descriptor.weapon=false;
        else if (field==3u) descriptor.handToHand=Field(false);
        else if (field==4u) descriptor.handToHand.value.reset();
        else if (field==5u) descriptor.hitDamage.status=PortableHeldItemValueStatus::MalformedProperty;
        else if (field==6u) descriptor.accurateRange.value.reset();
        else if (field==7u) descriptor.maxRange.value.reset();
        else descriptor.classPath.clear();
        Require(!ResolveEquippedMeleeProfile(original.actorPath,&descriptor),"Unavailable/malformed/non-melee fields granted melee profile");
    }
    for (auto bad:{std::int32_t{-1},std::int32_t{0}}) {
        descriptor=original; *descriptor.hitDamage.value=bad;
        Require(!ResolveEquippedMeleeProfile(original.actorPath,&descriptor),"Nonpositive original damage granted melee profile");
        descriptor=original; *descriptor.maxRange.value=bad;
        Require(!ResolveEquippedMeleeProfile(original.actorPath,&descriptor),"Nonpositive original range granted melee profile");
    }
    descriptor=original; *descriptor.accurateRange.value=-1;
    Require(!ResolveEquippedMeleeProfile(original.actorPath,&descriptor),"Negative accurate range granted melee profile");
    descriptor=original; *descriptor.accurateRange.value=0;
    Require(ResolveEquippedMeleeProfile(original.actorPath,&descriptor).has_value(),"Actual zero accurate range invalidated positive max range");
    descriptor=original; *descriptor.hitDamage.value=std::numeric_limits<std::int32_t>::max();
    Require(ResolveEquippedMeleeProfile(original.actorPath,&descriptor)->hitDamageOriginal==std::numeric_limits<std::int32_t>::max(),
        "Typed original damage was silently clamped/coerced");
}
void Lifecycle() {
    static_assert(std::is_trivially_copyable_v<EquippedItemVisualSelection>);
    const std::array keys{Ring(),Crowbar()};
    const auto actual=Descriptor();
    auto stale=actual; stale.actorPath="00_Training.WeaponCrowbar2";
    for (std::size_t cycle=0u;cycle<10000u;++cycle) {
        const auto ring=SelectEquippedItemVisual(kQuickBeltKeyRingIdentity,nullptr,keys);
        const auto item=SelectEquippedItemVisual(actual.actorPath,&actual,keys);
        const auto removed=SelectEquippedItemVisual("",&actual,keys);
        const auto replacement=SelectEquippedItemVisual(actual.actorPath,&stale,keys);
        Require(ring.preparedIndex==0u && ring.source==EquippedItemVisualSource::NativeStarterKeyRing,
            "Repeated equip cycle lost native starter ring");
        Require(item.preparedIndex==1u && item.source==EquippedItemVisualSource::CollectedActor,
            "Repeated equip cycle lost current actual item");
        Require(!removed.preparedIndex && !replacement.preparedIndex,
            "Inventory removal/map replacement retained previous model");
        Require(ResolveEquippedMeleeProfile(actual.actorPath,&actual)==EquippedMeleeProfile{6,80,80},
            "Repeated descriptor use changed original melee profile");
    }
    Require(actual.actorPath=="00_Training.WeaponCrowbar1" &&
        actual.playerViewMesh.value==keys[1].playerViewMesh && !actual.active && actual.collected,
        "Detached selection mutated runtime descriptor inputs");
}
}

int main() {
    try {
        NormalSelection(); DescriptorRefusals(); CatalogAndBudgets(); Melee(); Lifecycle();
        std::cout<<"Equipped item visual selection checks: "<<checks<<" passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"Equipped item visual selection failure after "<<checks<<" checks: "<<error.what()<<'\n';
        return 1;
    }
}
