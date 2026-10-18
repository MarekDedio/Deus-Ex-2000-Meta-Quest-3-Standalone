#pragma once

#include <cstdint>
#include <stdexcept>

namespace QuestVr::NativePackage {

// This is the closed original Object(28h) -> Package(34h) default-copy
// profile, not a general native CDO initializer or a portable 32-bit memory
// image. The owner must validate these IDs against actual indexed UClasses.
// See ORIGINAL-PACKAGE-LOOKUP-AUTHORITY.md for the owned Core.dll evidence.
enum class StaticConstructorProvenance : std::uint8_t {
    Unavailable,
    ObjectPackageSharedEmptyStaticConstructor
};
struct ClassProfile {
    std::uint32_t objectClassIndex{},packageClassIndex{};
    std::uint32_t packageSuperClassIndex{},packageWithinClassIndex{};
    std::uint32_t objectNativeSize{},packageNativeSize{};
    StaticConstructorProvenance staticConstructor{};
};

// This policy NEVER invokes LoadLibrary or fabricates a successful native
// module binding. A missing legacy library is a normal original Package
// constructor outcome; it does not confer symbol/constructor capability.
struct NoLegacyModuleProvider final {};
enum class AllocationRequest : std::uint8_t {CreatePackage,Transient};
enum class BindOutcome : std::uint8_t {
    MissingLegacyModule,
    NestedOuter,
    AlreadyAttempted
};

class PreparedDefaults;
class Instance;
[[nodiscard]] PreparedDefaults PrepareDefaults(const ClassProfile& profile);
[[nodiscard]] Instance Construct(const PreparedDefaults& defaults,std::uint32_t actualOuterIndex,
    AllocationRequest request,NoLegacyModuleProvider policy);

// Private const storage prevents edits to the prepared class defaults. The
// specific inherited header-only Object profile and equal empty constructor
// establish all three newly extended Package scalar values, rather than an
// arbitrary blanket-zero operation. UObject header/class identity is owned
// separately by the actual RuntimeObject; no legacy vtable/pointer is copied.
class PreparedDefaults final {
public:
    PreparedDefaults(const PreparedDefaults&)=default;
    [[nodiscard]] std::uint32_t ObjectClassIndex() const noexcept {return profile_.objectClassIndex;}
    [[nodiscard]] std::uint32_t ClassIndex() const noexcept {return profile_.packageClassIndex;}
    [[nodiscard]] bool LegacyHandleAbsent() const noexcept {return legacyHandle_==LegacyHandle::Absent;}
    [[nodiscard]] bool BindingAttempted() const noexcept {return bindingAttempted_;}
    [[nodiscard]] std::uint32_t PackageFlags() const noexcept {return packageFlags_;}

private:
    enum class LegacyHandle : std::uint8_t {Absent};
    friend PreparedDefaults PrepareDefaults(const ClassProfile&);
    friend Instance Construct(const PreparedDefaults&,std::uint32_t,AllocationRequest,NoLegacyModuleProvider);
    explicit PreparedDefaults(const ClassProfile& profile) noexcept :profile_(profile),
        legacyHandle_(LegacyHandle::Absent),bindingAttempted_(false),packageFlags_(0u) {}
    const ClassProfile profile_;
    const LegacyHandle legacyHandle_;
    const bool bindingAttempted_;
    const std::uint32_t packageFlags_;
};

inline PreparedDefaults PrepareDefaults(const ClassProfile& profile) {
    if(!profile.objectClassIndex||!profile.packageClassIndex||
        profile.objectClassIndex==profile.packageClassIndex)
        throw std::runtime_error("Native Package defaults require distinct actual Object and Package classes");
    if(profile.packageSuperClassIndex!=profile.objectClassIndex||
        profile.packageWithinClassIndex!=profile.objectClassIndex)
        throw std::runtime_error("Native Package defaults require actual Object Super and Within");
    if(profile.objectNativeSize!=0x28u||profile.packageNativeSize!=0x34u)
        throw std::runtime_error("Native Package default-copy size profile is unavailable");
    if(profile.staticConstructor!=StaticConstructorProvenance::ObjectPackageSharedEmptyStaticConstructor)
        throw std::runtime_error("Native Package shared empty static-constructor provenance is unavailable");
    return PreparedDefaults(profile);
}

// Only Construct creates an instance: exact prepared class defaults are
// copied before the bounded Package constructor applies its explicit bind
// policy. Class/Outer/header flags are identities supplied by the owner, not
// default-tail data. No public setter can inject a faux native handle.
class Instance final {
public:
    Instance(const Instance&)=default;
    [[nodiscard]] std::uint32_t ClassIndex() const noexcept {return classIndex_;}
    [[nodiscard]] std::uint32_t OuterIndex() const noexcept {return outerIndex_;}
    [[nodiscard]] std::uint32_t ObjectFlags() const noexcept {return objectFlags_;}
    [[nodiscard]] bool LegacyHandleAbsent() const noexcept {return legacyHandleAbsent_;}
    [[nodiscard]] bool BindingAttempted() const noexcept {return bindingAttempted_;}
    [[nodiscard]] std::uint32_t PackageFlags() const noexcept {return packageFlags_;}
    [[nodiscard]] BindOutcome ConstructorBindingOutcome() const noexcept {return constructorBindingOutcome_;}

    // Original BindPackage is void: these outcomes describe its bounded
    // portable effects, not a module-gated allocation success/failure result.
    BindOutcome Bind(NoLegacyModuleProvider) noexcept {
        if(outerIndex_)return BindOutcome::NestedOuter;
        if(bindingAttempted_)return BindOutcome::AlreadyAttempted;
        bindingAttempted_=true;
        return BindOutcome::MissingLegacyModule;
    }

private:
    friend Instance Construct(const PreparedDefaults&,std::uint32_t,AllocationRequest,NoLegacyModuleProvider);
    Instance(const PreparedDefaults& defaults,std::uint32_t outerIndex,std::uint32_t objectFlags,
        NoLegacyModuleProvider policy) noexcept :classIndex_(defaults.ClassIndex()),outerIndex_(outerIndex),
        objectFlags_(objectFlags),legacyHandleAbsent_(defaults.LegacyHandleAbsent()),
        bindingAttempted_(defaults.BindingAttempted()),packageFlags_(defaults.PackageFlags()),
        constructorBindingOutcome_(Bind(policy)) {}
    std::uint32_t classIndex_,outerIndex_,objectFlags_;
    bool legacyHandleAbsent_,bindingAttempted_;
    std::uint32_t packageFlags_;
    BindOutcome constructorBindingOutcome_;
};

inline Instance Construct(const PreparedDefaults& defaults,std::uint32_t actualOuterIndex,
    AllocationRequest request,NoLegacyModuleProvider policy) {
    std::uint32_t objectFlags{};
    switch(request) {
    case AllocationRequest::CreatePackage:objectFlags=4u;break;
    case AllocationRequest::Transient:objectFlags=0u;break;
    default:throw std::runtime_error("Native Package allocation request is unavailable");
    }
    return Instance(defaults,actualOuterIndex,objectFlags,policy);
}

} // namespace QuestVr::NativePackage
