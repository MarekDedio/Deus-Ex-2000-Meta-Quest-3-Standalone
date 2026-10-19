#pragma once

#include "quest_actor_line_trace.h"

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

enum class OriginalActorRegionSlot { Main,Foot,Head };
enum class OriginalActorZoneEvent { Leaving,Change,Entered,FootChange,HeadChange };
struct OriginalActorRegion {
    std::uintptr_t zone{};
    std::int32_t leaf{-1};
    std::uint8_t number{};
};
struct OriginalActorZoneQueries {
    std::function<bool(std::uintptr_t)> deleted,pawn;
    std::function<std::uintptr_t(std::uintptr_t)> levelActor0;
    std::function<void(std::uintptr_t)> requireLevelInfo;
    std::function<std::int32_t(std::uintptr_t)> levelActorCount;
    // Raw Float words, channel X=0/Y=1/Z=2. No entry-time pose snapshot.
    std::function<float(std::uintptr_t,unsigned)> location;
    std::function<float(std::uintptr_t)> collisionHeight,eyeHeight;
    // Complete original PointRegion on THIS native Level.Model with by-value
    // point and the supplied fallback. Result Zone must be nonnull. The lower
    // dependency owns active numeric/BSP admission, not a render polygon scan.
    std::function<OriginalActorRegion(std::uintptr_t,std::uintptr_t,PortableModelVec3)> pointRegion;
    // Actual stored tuples, never a computed read-only Actor.Region facade.
    std::function<std::uintptr_t(std::uintptr_t,OriginalActorRegionSlot)> regionZone;
    std::function<void(std::uintptr_t,OriginalActorRegionSlot,std::uintptr_t)> writeZone;
    std::function<void(std::uintptr_t,OriginalActorRegionSlot,std::int32_t)> writeLeaf;
    std::function<void(std::uintptr_t,OriginalActorRegionSlot,std::uint8_t)> writeNumber;
    // Authentic selected-event dispatch in the SAME caller VM execution.
    std::function<void(std::uintptr_t,OriginalActorZoneEvent,std::uintptr_t)> event;
    std::function<std::uint8_t(std::uintptr_t)> netMode;
    std::function<std::uintptr_t(std::uintptr_t)> replicationInfo;
    std::function<void(std::uintptr_t,std::uintptr_t)> writePlayerZone;
    // Every ordinary provider call charges this enclosing ledger BEFORE the
    // provider. Nested queries/events do not receive a new allowance.
    std::function<void()> work;
};

namespace ActorZoneDetail {
template<class Function,class... Arguments>
inline auto Call(const OriginalActorZoneQueries& q,const Function& f,Arguments... args){
    if(!f)throw std::invalid_argument("Original SetActorZone lacks an active native provider");
    if(q.work)q.work();
    return f(std::forward<Arguments>(args)...);
}
inline std::uintptr_t Info(std::uintptr_t level,const OriginalActorZoneQueries& q){
    if(!Call(q,q.levelActor0,level))
        throw std::invalid_argument("Original SetActorZone requires native Level.Actors[0]");
    const auto checked=Call(q,q.levelActor0,level);
    Call(q,q.requireLevelInfo,checked);
    return Call(q,q.levelActor0,level);
}
inline void Store(std::uintptr_t actor,OriginalActorRegionSlot slot,const OriginalActorRegion& region,
    const OriginalActorZoneQueries& q){
    Call(q,q.writeZone,actor,slot,region.zone);
    Call(q,q.writeLeaf,actor,slot,region.leaf);
    Call(q,q.writeNumber,actor,slot,region.number);
}
inline OriginalActorRegion Query(std::uintptr_t level,std::uintptr_t fallback,PortableModelVec3 point,
    const OriginalActorZoneQueries& q){
    const auto region=Call(q,q.pointRegion,level,fallback,point);
    if(!region.zone)throw std::invalid_argument("Original PointRegion provider returned a null Zone");
    return region;
}
inline void Event(std::uintptr_t receiver,OriginalActorZoneEvent event,std::uintptr_t argument,
    const OriginalActorZoneQueries& q){
    if(!receiver)throw std::invalid_argument("Original zone event has an unsafe null receiver");
    Call(q,q.event,receiver,event,argument);
}
inline float FootZ(float z,float height){
    return ActorTimerDetail::Store32(ActorLineTraceDetail::Sub(ActorLineTraceDetail::Load(z),ActorLineTraceDetail::Load(height)));
}
inline float HeadChannel(float offset,float location){
    return ActorTimerDetail::Store32(NativeWindowClipDetail::Add64(ActorLineTraceDetail::Load(offset),ActorLineTraceDetail::Load(location)));
}
} // namespace ActorZoneDetail

// Complete ordinary 1039b940..1039be94 protocol. No local transaction is opened
// or restored here: partial stores and synchronous callbacks belong to the SAME
// caller VM root, which MUST roll them back on refusal. This is not MoveActor,
// support/falling or a publishing world-start shortcut.
inline void SetOriginalActorZone(std::uintptr_t level,std::uintptr_t actor,std::int32_t test,
    std::int32_t force,const OriginalActorZoneQueries& q){
    using namespace ActorZoneDetail;
    using Slot=OriginalActorRegionSlot;using EventKind=OriginalActorZoneEvent;
    if(!actor)throw std::invalid_argument("Original SetActorZone requires a nonnull Actor");
    if(Call(q,q.deleted,actor))return;
    if(!level)throw std::invalid_argument("Original SetActorZone requires its native Level");
    const auto firstInfo=Info(level,q);
    if(actor==firstInfo){
        // The native special branch rechecks the already compared pointer,
        // classifies a NEW slot0 load, then reloads once more for the write.
        if(!firstInfo)throw std::invalid_argument("Original LevelInfo special receiver is null");
        const auto checked=Call(q,q.levelActor0,level);Call(q,q.requireLevelInfo,checked);
        const auto info=Call(q,q.levelActor0,level);
        Store(actor,Slot::Main,{info,-1,0},q);return;
    }
    const bool isPawn=Call(q,q.pawn,actor);
    if(force){
        Store(actor,Slot::Main,{Info(level,q),-1,0},q);
        if(isPawn){
            const auto info=Info(level,q);
            // Force-reset stores INTERLEAVE, rather than copying Head then
            // Foot as two whole structs. No callbacks run between them.
            Call(q,q.writeZone,actor,Slot::Head,info);Call(q,q.writeZone,actor,Slot::Foot,info);
            Call(q,q.writeLeaf,actor,Slot::Head,std::int32_t{-1});Call(q,q.writeLeaf,actor,Slot::Foot,std::int32_t{-1});
            Call(q,q.writeNumber,actor,Slot::Head,std::uint8_t{});Call(q,q.writeNumber,actor,Slot::Foot,std::uint8_t{});
        }
    }
    std::uintptr_t fallback=actor;
    if(Call(q,q.levelActorCount,level))fallback=Info(level,q);
    PortableModelVec3 point;
    point.x=Call(q,q.location,actor,0u);point.y=Call(q,q.location,actor,1u);point.z=Call(q,q.location,actor,2u);
    const auto main=Query(level,fallback,point,q);
    const auto old=Call(q,q.regionZone,actor,Slot::Main);
    if(main.zone!=old&&!test){
        Event(old,EventKind::Leaving,actor,q);Event(actor,EventKind::Change,main.zone,q);
        Store(actor,Slot::Main,main,q);
        const auto entered=Call(q,q.regionZone,actor,Slot::Main);
        Event(entered,EventKind::Entered,actor,q);
    }else Store(actor,Slot::Main,main,q);
    if(!isPawn)return;

    // Sample after main events. Raw foot X/Y are copied only AFTER the Z
    // subtraction/store. Numeric admission applies to active arithmetic only.
    const auto height=Call(q,q.collisionHeight,actor);
    const auto locationZ=Call(q,q.location,actor,2u);
    point.z=FootZ(locationZ,height);
    point.y=Call(q,q.location,actor,1u);point.x=Call(q,q.location,actor,0u);
    const auto foot=Query(level,Info(level,q),point,q);
    const auto oldFoot=Call(q,q.regionZone,actor,Slot::Foot);
    if(foot.zone!=oldFoot&&!test)Event(actor,EventKind::FootChange,foot.zone,q);
    // Original EyeHeight read is BEFORE the foot tuple's raw publication.
    const auto eyes=Call(q,q.eyeHeight,actor);
    Store(actor,Slot::Foot,foot,q);
    const auto x=Call(q,q.location,actor,0u);point.x=HeadChannel(0.0f,x);
    const auto y=Call(q,q.location,actor,1u);point.y=HeadChannel(0.0f,y);
    const auto z=Call(q,q.location,actor,2u);point.z=HeadChannel(eyes,z);
    const auto head=Query(level,Info(level,q),point,q);
    const auto oldHead=Call(q,q.regionZone,actor,Slot::Head);
    if(head.zone!=oldHead&&!test)Event(actor,EventKind::HeadChange,head.zone,q);
    Store(actor,Slot::Head,head,q);
    if(Call(q,q.netMode,Info(level,q))==3u)return;
    const auto pri=Call(q,q.replicationInfo,actor);
    if(pri){
        const auto current=Call(q,q.regionZone,actor,Slot::Main);
        Call(q,q.writePlayerZone,pri,current);
    }
}
} // namespace QuestVr
