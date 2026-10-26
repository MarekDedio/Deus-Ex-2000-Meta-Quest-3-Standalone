#pragma once

#include "portable_model_geometry.h"

#include <atomic>
#include <memory>
#include <utility>

namespace QuestVr {
struct ModelExtentTraceLimits;
struct ModelExtentTraceStats;
struct ModelExtentTraceResult;
struct ModelPointOverlapLimits;
struct ModelPointOverlapStats;
struct OriginalActorLineHit;

// An actual const allocation, not a const view of caller-owned mutable data.
// Successful null-owner hull validation is asset state, independent of script
// transaction rollback. No constructor accepts a public pointer or move whose
// vector elements could still have mutable aliases. Raw geometry APIs retain
// their full validation contract.
class OriginalImmutableCollisionModel final {
public:
    static std::shared_ptr<const OriginalImmutableCollisionModel> CopyOf(const PortableModelGeometry& model) {
        return Create(std::make_shared<const PortableModelGeometry>(model));
    }
    static std::shared_ptr<const OriginalImmutableCollisionModel> LoadWorld(
        const PortablePackageTables& package,std::size_t index,std::size_t scratch=64u*1024u*1024u) {
        return Create(std::make_shared<const PortableModelGeometry>(LoadPortableModel68(package,index,scratch)));
    }
    static std::shared_ptr<const OriginalImmutableCollisionModel> LoadCollision(
        const PortablePackageTables& package,std::size_t index,std::size_t scratch=64u*1024u*1024u,
        std::size_t decoded=16u*1024u*1024u) {
        return Create(std::make_shared<const PortableModelGeometry>(LoadPortableCollisionModel68(package,index,scratch,decoded)));
    }
    const PortableModelGeometry& Geometry()const noexcept{return *geometry_;}
    std::shared_ptr<const PortableModelGeometry> GeometryLease()const noexcept{return geometry_;}
    bool HasNullHullValidation()const noexcept{return nullHullValidated_.load(std::memory_order_acquire);}
private:
    explicit OriginalImmutableCollisionModel(std::shared_ptr<const PortableModelGeometry> geometry)
        :geometry_(std::move(geometry)){}
    static std::shared_ptr<const OriginalImmutableCollisionModel> Create(std::shared_ptr<const PortableModelGeometry> geometry) {
        return std::shared_ptr<const OriginalImmutableCollisionModel>(new const OriginalImmutableCollisionModel(std::move(geometry)));
    }
    void ValidateNullHull(const ModelExtentTraceLimits&,ModelExtentTraceStats&)const;
    friend ModelExtentTraceResult TraceOriginalModelExtent(const OriginalImmutableCollisionModel&,
        const PortableModelVec3&,const PortableModelVec3&,const PortableModelVec3&,const ModelExtentTraceLimits&);
    template<class Capture>
    friend bool CheckOriginalModelPointOverlap(OriginalActorLineHit&,const OriginalImmutableCollisionModel&,
        std::uintptr_t,std::uintptr_t,const PortableModelVec3&,const PortableModelVec3&,std::uint32_t,Capture&&,
        const ModelPointOverlapLimits&,ModelPointOverlapStats*);
    const std::shared_ptr<const PortableModelGeometry> geometry_;
    mutable std::atomic<bool> nullHullValidated_{false};
};
} // namespace QuestVr
