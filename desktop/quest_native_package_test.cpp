#include "quest_native_package.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
namespace NP=QuestVr::NativePackage;
std::size_t checks{},refusals{};
void Require(bool value,const char* why) {++checks;if(!value)throw std::runtime_error(why);}
template<class F> void Refused(F&& operation,const char* why) {
    bool refused{};try {operation();}catch(const std::runtime_error&) {refused=true;}
    Require(refused,why);++refusals;
}
NP::ClassProfile Profile(std::uint32_t object=7u,std::uint32_t package=19u) {
    // Independent original literals, not profile constants copied from the
    // implementation under test: Object28h -> Package34h, Super/WithinObject.
    return {object,package,object,object,0x28u,0x34u,
        NP::StaticConstructorProvenance::ObjectPackageSharedEmptyStaticConstructor};
}
void Defaults(const NP::PreparedDefaults& defaults,std::uint32_t object,std::uint32_t package) {
    Require(defaults.ObjectClassIndex()==object&&defaults.ClassIndex()==package,
        "Prepared Package default class identities changed");
    Require(defaults.LegacyHandleAbsent(),"Fresh Package defaults contain a fake native module handle");
    Require(!defaults.BindingAttempted(),"Preparing Package defaults ran its instance constructor");
    Require(defaults.PackageFlags()==0u,"Fresh original PackageFlags are not zero");
}
void Construction() {
    const auto defaults=NP::PrepareDefaults(Profile());Defaults(defaults,7u,19u);
    for(const auto request:{NP::AllocationRequest::CreatePackage,NP::AllocationRequest::Transient}) {
        const auto flags=request==NP::AllocationRequest::CreatePackage?4u:0u;
        for(const auto outer:{0u,1u,31u,4'000'000u}) {
            auto instance=NP::Construct(defaults,outer,request,NP::NoLegacyModuleProvider{});
            Require(instance.ClassIndex()==19u&&instance.OuterIndex()==outer&&instance.ObjectFlags()==flags,
                "Native Package construction lost actual class/Outer or requested flags");
            Require(instance.LegacyHandleAbsent(),"NoLegacyModuleProvider fabricated a native handle");
            Require(instance.BindingAttempted()==(outer==0u),"Package bind attempt ignored actual Outer");
            Require(instance.PackageFlags()==0u,"Package bind changed its copied scalar flags");
            Require(instance.ConstructorBindingOutcome()==(outer==0u?NP::BindOutcome::MissingLegacyModule:
                NP::BindOutcome::NestedOuter),"Bounded Package constructor outcome differs");
            for(unsigned repeat=0u;repeat<17u;++repeat) {
                Require(instance.Bind(NP::NoLegacyModuleProvider{})==(outer==0u?NP::BindOutcome::AlreadyAttempted:
                    NP::BindOutcome::NestedOuter),"Repeated binding did not preserve original no-op guard");
                Require(instance.LegacyHandleAbsent()&&instance.PackageFlags()==0u&&
                    instance.BindingAttempted()==(outer==0u),"Repeated binding mutated Package tail");
            }
            const auto copy=instance;
            Require(copy.ClassIndex()==instance.ClassIndex()&&copy.OuterIndex()==instance.OuterIndex()&&
                copy.ObjectFlags()==instance.ObjectFlags()&&copy.BindingAttempted()==instance.BindingAttempted()&&
                copy.LegacyHandleAbsent()&&copy.PackageFlags()==0u,"Typed Package state copy lost exact constructor state");
            Defaults(defaults,7u,19u);
        }
    }
    auto first=NP::Construct(defaults,0u,NP::AllocationRequest::CreatePackage,NP::NoLegacyModuleProvider{});
    auto second=NP::Construct(defaults,0u,NP::AllocationRequest::CreatePackage,NP::NoLegacyModuleProvider{});
    Require(first.ConstructorBindingOutcome()==NP::BindOutcome::MissingLegacyModule&&
        second.ConstructorBindingOutcome()==NP::BindOutcome::MissingLegacyModule,
        "One instance constructor mutated shared defaults for later instances");
    Defaults(defaults,7u,19u);
    Refused([&]{(void)NP::Construct(defaults,0u,static_cast<NP::AllocationRequest>(255u),NP::NoLegacyModuleProvider{});},
        "Unknown allocation request silently changed CreatePackage/Transient flags");
    Defaults(defaults,7u,19u);
}
void InvalidProfiles() {
    const auto check=[](const NP::ClassProfile& profile) {
        Refused([&]{(void)NP::PrepareDefaults(profile);},"Unproven native Package defaults were prepared");
    };
    auto profile=Profile();profile.objectClassIndex=0u;check(profile);
    profile=Profile();profile.packageClassIndex=0u;check(profile);
    profile=Profile();profile.packageClassIndex=profile.objectClassIndex;check(profile);
    for(const auto id:{0u,19u,20u,std::numeric_limits<std::uint32_t>::max()}) {
        profile=Profile();profile.packageSuperClassIndex=id;check(profile);
        profile=Profile();profile.packageWithinClassIndex=id;check(profile);
    }
    for(const auto size:{0u,1u,0x27u,0x29u,0x33u,0x34u,0x35u,std::numeric_limits<std::uint32_t>::max()}) {
        profile=Profile();profile.objectNativeSize=size;check(profile);
    }
    for(const auto size:{0u,1u,0x27u,0x28u,0x29u,0x33u,0x35u,std::numeric_limits<std::uint32_t>::max()}) {
        profile=Profile();profile.packageNativeSize=size;check(profile);
    }
    profile=Profile();profile.staticConstructor=NP::StaticConstructorProvenance::Unavailable;check(profile);
    profile=Profile();profile.staticConstructor=static_cast<NP::StaticConstructorProvenance>(255u);check(profile);
    Refused([]{(void)NP::PrepareDefaults({});},"Blanket-zero defaults bypassed audited Object/Package profile");
}
void IndependentIdentities() {
    // The class IDs are actual embedding identities, not fixed export slots,
    // hashes or retail absolute UObject indices. Exercise many fresh pairs.
    std::uint32_t seed=0x93ac1257u;
    for(unsigned i=0u;i<4096u;++i) {
        seed=seed*1664525u+1013904223u;const auto object=1u+seed%4'000'000u;
        seed=seed*1664525u+1013904223u;auto package=1u+seed%4'000'000u;
        if(package==object)package=package==4'000'000u?1u:package+1u;
        const auto defaults=NP::PrepareDefaults(Profile(object,package));Defaults(defaults,object,package);
        auto instance=NP::Construct(defaults,0u,NP::AllocationRequest::CreatePackage,NP::NoLegacyModuleProvider{});
        Require(instance.ClassIndex()==package&&instance.OuterIndex()==0u&&instance.ObjectFlags()==4u&&
            instance.BindingAttempted()&&instance.LegacyHandleAbsent()&&instance.PackageFlags()==0u,
            "Prepared class identity or bounded missing-module state depends on a fixed slot");
        Require(instance.Bind(NP::NoLegacyModuleProvider{})==NP::BindOutcome::AlreadyAttempted,
            "Independent native Package repeated a completed bind attempt");
        Defaults(defaults,object,package);
    }
}
}

int main() {
    static_assert(!std::is_default_constructible_v<NP::PreparedDefaults>);
    static_assert(!std::is_aggregate_v<NP::PreparedDefaults>);
    static_assert(!std::is_copy_assignable_v<NP::PreparedDefaults>);
    static_assert(!std::is_default_constructible_v<NP::Instance>);
    static_assert(!std::is_aggregate_v<NP::Instance>);
    static_assert(std::is_empty_v<NP::NoLegacyModuleProvider>);
    try {
        Construction();InvalidProfiles();IndependentIdentities();
        std::cout<<"Native Package audited default-copy and no-legacy-module policy: "<<checks
            <<" checks, "<<refusals<<" refusals passed\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"Native Package policy failed: "<<error.what()<<'\n';return 1;
    }
}
