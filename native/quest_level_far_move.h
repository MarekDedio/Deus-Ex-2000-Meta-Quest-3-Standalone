#pragma once

#include "quest_actor_move_geometry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

struct OriginalLevelFarMoveQueries {
    // Original Actor+28 word: Static1 / Movable00080000. Placement's
    // separate lazy read must not use this eligibility snapshot.
    std::function<std::uint32_t(std::uintptr_t)> eligibilityFlags;
    std::function<bool(std::uintptr_t)> collideWhenPlacing;
    std::function<std::int32_t()> editor;
    // Original Actor+1DC low byte: CollideActors1 / CollideWorld2.
    std::function<std::uint8_t(std::uintptr_t)> collisionFlags,standingCount;
    std::function<float(std::uintptr_t)> radius,height;
    std::function<std::uintptr_t(std::uintptr_t)> levelActor0;
    std::function<void(std::uintptr_t)> requireLevelInfo;
    std::function<std::uint8_t(std::uintptr_t)> netMode;
    std::function<std::int32_t(std::uintptr_t,unsigned)> rotation;
    std::function<std::uintptr_t(std::uintptr_t)> levelHash;
    std::function<void(std::uintptr_t,std::uintptr_t)> hashRemove,hashAdd;
    // COMPLETE original FindSpot, not the separate Spawn placement heuristic.
    // It changes candidate only according to its original native contract.
    std::function<std::int32_t(std::uintptr_t,PortableModelVec3,
        PortableModelVec3&,std::int32_t,std::int32_t)> findSpot;
    // COMPLETE higher encroachment, with the literal final integer1.
    std::function<std::int32_t(std::uintptr_t,std::uintptr_t,PortableModelVec3,
        std::array<std::int32_t,3u>,std::int32_t)> encroachment;
    // LIVE native Level.Actor slots, including holes and appended slots.
    std::function<std::int32_t(std::uintptr_t)> actorCount;
    std::function<std::uintptr_t(std::uintptr_t,std::int32_t)> actorAt;
    std::function<std::uintptr_t(std::uintptr_t)> base;
    // COMPLETE SetBase (including callbacks), not a direct Base write.
    std::function<void(std::uintptr_t,std::uintptr_t,std::int32_t)> setBase;
    std::function<void(std::uintptr_t)> justTeleported;
    std::function<void(std::uintptr_t,unsigned,float)> writeLocation,writeOldLocation;
    // COMPLETE native SetActorZone with literal force-refresh0.
    std::function<void(std::uintptr_t,std::uintptr_t,std::int32_t,std::int32_t)> setZone;
    std::function<void()> work;
};

namespace LevelFarMoveDetail {
template<class Function,class... Arguments>
inline auto Call(const OriginalLevelFarMoveQueries& q,const Function& f,Arguments&&... args){
    if(!f)throw std::invalid_argument("Original FarMoveActor lacks an active native provider");
    if(q.work)q.work();
    return f(std::forward<Arguments>(args)...);
}
inline std::uintptr_t Info(std::uintptr_t level,const OriginalLevelFarMoveQueries& q){
    if(!Call(q,q.levelActor0,level))
        throw std::invalid_argument("Original FarMoveActor requires actual Level.Actors[0]");
    const auto checked=Call(q,q.levelActor0,level);Call(q,q.requireLevelInfo,checked);
    return Call(q,q.levelActor0,level);
}
} // namespace LevelFarMoveDetail

// Original retail10398BA0..10398E35 coordinator. This absolute relocation is
// NOT ordinary MoveActor with a calculated delta. Providers must share the
// enclosing VM root, live native coordinates/hash and cumulative work budget.
// An ordinary false result still re-adds the freshly collidable source. An
// exception is not a native false result: enclosing transaction ownership must
// restore all earlier writes/callbacks, rather than run new metered cleanup.
// The native integer result is retained (FindSpot need not return literal1).
inline std::int32_t MoveOriginalLevelFarActor(std::uintptr_t level,std::uintptr_t source,
    PortableModelVec3 candidate,std::int32_t test,std::int32_t noCheck,
    const OriginalLevelFarMoveQueries& q,std::size_t maximumActorVisits=65'536u){
    using namespace LevelFarMoveDetail;
    if(!source)throw std::invalid_argument("Original FarMoveActor requires a nonnull source Actor");
    const auto eligibility=Call(q,q.eligibilityFlags,source);
    if(((eligibility&1u)||!(eligibility&0x80000u))&&!Call(q,q.editor))return 0;
    if(!level)throw std::invalid_argument("Original FarMoveActor requires its retained native Level");
    if(Call(q,q.collisionFlags,source)&1u){
        const auto hash=Call(q,q.levelHash,level);if(hash)Call(q,q.hashRemove,hash,source);
    }
    std::int32_t result=1;
    if(!noCheck){
        bool place=(Call(q,q.collisionFlags,source)&2u)!=0u;
        if(!place&&Call(q,q.collideWhenPlacing,source))
            place=Call(q,q.netMode,Info(level,q))!=3u;
        if(place){
            // Separate raw radiusX, height and x87-spilled radiusY reads in
            // exactly this order. No arithmetic occurs on candidate itself.
            const auto radiusX=Call(q,q.radius,source);
            const auto height=Call(q,q.height,source);
            const auto radiusY=Call(q,q.radius,source);
            PortableModelVec3 extent;extent.x=radiusX;
            extent.y=ActorTimerDetail::Store32(ActorLineTraceDetail::Load(radiusY));
            extent.z=ActorTimerDetail::Store32(ActorLineTraceDetail::Load(height));
            result=Call(q,q.findSpot,level,extent,candidate,std::int32_t{},std::int32_t{});
        }
    }
    if(result&&!test&&!noCheck){
        std::array<std::int32_t,3u> rotation;
        for(unsigned i=0u;i<3u;++i)rotation[i]=Call(q,q.rotation,source,i);
        result=Call(q,q.encroachment,level,source,candidate,rotation,std::int32_t{1})==0?1:0;
    }
    if(result){
        if(!test){
            if(Call(q,q.standingCount,source)){
                std::size_t visits{};
                for(std::int32_t index=0;index<Call(q,q.actorCount,level);++index){
                    if(visits>=maximumActorVisits)
                        throw std::runtime_error("Original FarMoveActor cumulative live Actor-slot budget exceeded");
                    ++visits;
                    const auto child=Call(q,q.actorAt,level,index);
                    if(child&&Call(q,q.base,child)==source)
                        Call(q,q.setBase,child,std::uintptr_t{},std::int32_t{1});
                }
            }
            Call(q,q.justTeleported,source);
        }
        // Test mode also publishes BOTH vectors. Source Base is untouched.
        Call(q,q.writeLocation,source,0u,candidate.x);
        Call(q,q.writeLocation,source,1u,candidate.y);
        Call(q,q.writeLocation,source,2u,candidate.z);
        Call(q,q.writeOldLocation,source,0u,candidate.x);
        Call(q,q.writeOldLocation,source,1u,candidate.y);
        Call(q,q.writeOldLocation,source,2u,candidate.z);
    }
    // Read flags and the native hash anew even on ordinary placement veto.
    if(Call(q,q.collisionFlags,source)&1u){
        const auto hash=Call(q,q.levelHash,level);if(hash)Call(q,q.hashAdd,hash,source);
    }
    if(result)Call(q,q.setZone,level,source,test,std::int32_t{});
    return result;
}
} // namespace QuestVr
