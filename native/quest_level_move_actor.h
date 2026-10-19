#pragma once

#include "quest_actor_move_geometry.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

enum class OriginalMoveActorClass { Brush,Pawn,Decoration };
struct OriginalMoveActorSweep {
    std::uintptr_t level{},world{};
    PortableModelVec3 end{},start{},extent{};
    std::int32_t checkActors{};
    std::uint8_t extraFlags{};
};
struct OriginalLevelMoveActorQueries {
    // Original Actor+28 word: Static bit1, Movable bit00080000. The separate
    // staticFlag provider is used by later byte-only/static predicate sites.
    std::function<std::uint32_t(std::uintptr_t)> eligibilityFlags;
    std::function<std::int32_t()> editor;
    std::function<bool(std::uintptr_t)> staticFlag,hasBrush;
    std::function<bool(std::uintptr_t,OriginalMoveActorClass)> isA;
    // Low original Actor+1DC byte, including CollideActors1/CollideWorld2/
    // BlockActors4/BlockPlayers8. Different source reads are NOT cached.
    std::function<std::uint8_t(std::uintptr_t)> collisionFlags;
    std::function<std::uint8_t(std::uintptr_t)> standingCount;
    std::function<std::int32_t(std::uintptr_t,unsigned)> rotation;
    std::function<void(std::uintptr_t,unsigned,std::int32_t)> writeRotation;
    std::function<float(std::uintptr_t,unsigned)> location;
    std::function<void(std::uintptr_t,unsigned,float)> writeLocation;
    std::function<float(std::uintptr_t)> radius,height;
    std::function<std::uintptr_t(std::uintptr_t)> levelActor0;
    std::function<void(std::uintptr_t)> requireLevelInfo;
    // Original live temporary lists belong to this ONE native mark. Returned
    // opaque handles remain valid through callbacks and recursive movement.
    // Pop MUST be noexcept/allocation-free/unmetered. Mandatory exceptional
    // and ordinary-veto cleanup here is stronger portable resource safety;
    // retail's veto/EH do NOT contain a proven Pop or actor rollback.
    std::function<std::uintptr_t()> mark;
    std::function<void(std::uintptr_t)> popMark;
    std::function<std::uintptr_t(OriginalMoveActorSweep)> multiLine;
    std::function<std::uintptr_t(std::uintptr_t)> hitActor,hitNext;
    std::function<float(std::uintptr_t)> hitTime;
    std::function<OriginalActorLineHit(std::uintptr_t)> hitPacket;
    std::function<bool(std::uintptr_t,std::uintptr_t)> basedOn,blockedBy,overlaps;
    // COMPLETE based-child protocol, with COMPLETE recursive MoveActor in
    // this SAME caller root. Not a local transform setter or one fake child.
    std::function<void(std::uintptr_t,std::uintptr_t,PortableModelVec3,
        std::array<std::int32_t,3u>,std::int32_t)> carryChildren;
    // COMPLETE higher CheckEncroachment. Its literal final int is ALWAYS0.
    std::function<std::int32_t(std::uintptr_t,std::uintptr_t,PortableModelVec3,
        std::array<std::int32_t,3u>,std::int32_t)> encroachment;
    std::function<std::uintptr_t(std::uintptr_t)> levelHash;
    std::function<void(std::uintptr_t,std::uintptr_t)> hashRemove,hashAdd;
    // Correct native packet order: BumpWall(Location,Normal), NOT HitWall.
    std::function<void(std::uintptr_t,PortableModelVec3,PortableModelVec3)> bumpWall;
    std::function<void(std::uintptr_t,std::uintptr_t)> bump,beginTouch;
    std::function<std::uintptr_t(std::uintptr_t,unsigned)> touching;
    std::function<void(std::uintptr_t,std::uintptr_t,std::int32_t)> endTouch;
    // COMPLETE movement-time zone operation on the retained native Level.
    std::function<void(std::uintptr_t,std::uintptr_t,std::int32_t,std::int32_t)> setZone;
    std::function<void()> work;
};

namespace LevelMoveActorDetail {
using ActorLineTraceDetail::Load;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Store32;
using NativeWindowClipDetail::Add64;
template<class Function,class... Arguments>
inline auto Call(const OriginalLevelMoveActorQueries& q,const Function& f,Arguments&&... args){
    if(!f)throw std::invalid_argument("Original MoveActor lacks an active native provider");
    if(q.work)q.work();
    return f(std::forward<Arguments>(args)...);
}
inline bool MovingBrush(std::uintptr_t actor,const OriginalLevelMoveActorQueries& q){
    return Call(q,q.hasBrush,actor)&&Call(q,q.isA,actor,OriginalMoveActorClass::Brush)&&!Call(q,q.staticFlag,actor);
}
inline std::uintptr_t Info(std::uintptr_t level,const OriginalLevelMoveActorQueries& q){
    if(!Call(q,q.levelActor0,level))throw std::invalid_argument("Original MoveActor requires actual Level.Actors[0]");
    const auto checked=Call(q,q.levelActor0,level);Call(q,q.requireLevelInfo,checked);
    return Call(q,q.levelActor0,level);
}
class MarkScope {
    const OriginalLevelMoveActorQueries& q_;std::uintptr_t token_{};bool active_{};
public:
    explicit MarkScope(const OriginalLevelMoveActorQueries& q):q_(q){
        if(!q_.popMark)throw std::invalid_argument("Original MoveActor lacks mandatory mark cleanup");
        token_=Call(q_,q_.mark);active_=true;
    }
    ~MarkScope()noexcept{Close();}
    MarkScope(const MarkScope&)=delete;MarkScope& operator=(const MarkScope&)=delete;
    void Close()noexcept{if(active_){active_=false;q_.popMark(token_);}}
};
inline bool Nan(float x){const auto bits=std::bit_cast<std::uint32_t>(x);return (bits&0x7f800000u)==0x7f800000u&&(bits&0x7fffffu);}
inline bool BelowOrUnordered(float a,float b){return Nan(a)||Nan(b)||a<b;}
inline bool PositiveOrdered(float x){return !Nan(x)&&x>0.0f;}
inline bool NonzeroDelta(const PortableModelVec3& d){
    // Initial geometry already supplies finite active arithmetic admission.
    // Original equality is numeric: either signed zero is equal to zero.
    return d.x!=0.0f||d.y!=0.0f||d.z!=0.0f;
}
inline void Visit(std::size_t& visits,std::size_t maximum){
    if(visits>=maximum)throw std::runtime_error("Original MoveActor cumulative live hit-list budget exceeded");
    ++visits;
}
inline OriginalMoveActorSweep Sweep(std::uintptr_t level,std::uintptr_t source,const PortableModelVec3& extended,
    const OriginalLevelMoveActorQueries& q){
    OriginalMoveActorSweep sweep;sweep.level=level;
    if((Call(q,q.collisionFlags,source)&2u)&&!MovingBrush(source,q))sweep.world=Info(level,q);
    if((Call(q,q.collisionFlags,source)&1u)&&!MovingBrush(source,q))sweep.checkActors=1;
    const auto height=Call(q,q.height,source),radiusY=Call(q,q.radius,source),radiusX=Call(q,q.radius,source);
    sweep.extent.x=radiusX; // Native raw X copy, independent second radius read.
    sweep.extent.y=Store32(Load(radiusY));sweep.extent.z=Store32(Load(height));
    const auto z=Call(q,q.location,source,2u);const auto ez=Add64(Decode(extended.z),Load(z));
    const auto y=Call(q,q.location,source,1u);const auto ey=Add64(Decode(extended.y),Load(y));
    const auto x=Call(q,q.location,source,0u);const auto ex=Add64(Decode(extended.x),Load(x));
    // Start's raw XYZ reads interleave the end XYZ Float spills.
    sweep.start.x=Call(q,q.location,source,0u);sweep.end.x=Store32(ex);
    sweep.start.y=Call(q,q.location,source,1u);sweep.end.y=Store32(ey);
    sweep.start.z=Call(q,q.location,source,2u);sweep.end.z=Store32(ez);
    return sweep;
}
inline PortableModelVec3 Proposed(std::uintptr_t source,const PortableModelVec3& delta,const OriginalLevelMoveActorQueries& q){
    PortableModelVec3 p;
    const auto z=Call(q,q.location,source,2u);p.z=Store32(Add64(Decode(delta.z),Load(z)));
    const auto y=Call(q,q.location,source,1u);p.y=Store32(Add64(Decode(delta.y),Load(y)));
    const auto x=Call(q,q.location,source,0u);p.x=Store32(Add64(Decode(delta.x),Load(x)));
    return p;
}
inline void Publish(std::uintptr_t level,std::uintptr_t source,const PortableModelVec3& delta,
    const std::array<std::int32_t,3u>& rotation,const OriginalLevelMoveActorQueries& q){
    if(Call(q,q.collisionFlags,source)&1u){const auto hash=Call(q,q.levelHash,level);if(hash)Call(q,q.hashRemove,hash,source);}
    for(unsigned i=0u;i<3u;++i){const auto from=Call(q,q.location,source,i);
        const auto by=i==0u?delta.x:i==1u?delta.y:delta.z;
        const auto to=Store32(Add64(Decode(by),Load(from)));Call(q,q.writeLocation,source,i,to);}
    for(unsigned i=0u;i<3u;++i)Call(q,q.writeRotation,source,i,rotation[i]);
    if(Call(q,q.collisionFlags,source)&1u){const auto hash=Call(q,q.levelHash,level);if(hash)Call(q,q.hashAdd,hash,source);}
}
} // namespace LevelMoveActorDetail

// Complete ordinary103990E0..10399F5F coordinator. Profiling counters/timestamps
// are omitted, not repurposed as gameplay state. Providers MUST be complete,
// native-authoritative and share the SAME VM/hash/root budget. This function
// does not manufacture those services, start a transaction or claim native266
// is integrated merely because a mocked provider can complete its protocol.
inline bool MoveOriginalLevelActor(std::uintptr_t level,std::uintptr_t source,PortableModelVec3 delta,
    std::array<std::int32_t,3u> requested,OriginalActorLineHit& hit,std::array<std::int32_t,4u> options,
    const OriginalLevelMoveActorQueries& q,std::size_t maximumHitVisits=65'536u){
    using namespace LevelMoveActorDetail;
    if(!source)throw std::invalid_argument("Original MoveActor requires a nonnull source Actor");
    const auto eligibility=Call(q,q.eligibilityFlags,source);
    if(((eligibility&1u)||!(eligibility&0x80000u))&&!Call(q,q.editor))return false;
    if(IsOriginalActorMoveSmallDelta(delta)){
        bool same=true;
        for(unsigned i=0u;i<3u;++i)if(Call(q,q.rotation,source,i)!=requested[i]){same=false;break;}
        if(same)return true;
        if(!Call(q,q.standingCount,source)&&!MovingBrush(source,q)){
            for(unsigned i=0u;i<3u;++i)Call(q,q.writeRotation,source,i,requested[i]);
            return true; // Hit remains bit-for-bit untouched; tiny Delta discarded.
        }
    }
    hit=OriginalActorLineHit{};
    if(!level)throw std::invalid_argument("Original full MoveActor requires its retained native Level");
    MarkScope mark(q);
    const auto geometry=PrepareOriginalActorMoveGeometry(delta);
    std::uintptr_t head{};bool potentialTouch{};std::size_t visits{};
    if((Call(q,q.collisionFlags,source)&3u)&&!MovingBrush(source,q)&&NonzeroDelta(delta)){
        head=Call(q,q.multiLine,Sweep(level,source,geometry.extended,q));
        if(Call(q,q.collisionFlags,source)&0x0eu){
            for(auto link=head;link;link=Call(q,q.hitNext,link)){
                Visit(visits,maximumHitVisits);
                if(options[1u]){
                    const auto classified=Call(q,q.hitActor,link);
                    if(!Call(q,q.staticFlag,classified)){
                        if(Call(q,q.isA,classified,OriginalMoveActorClass::Pawn))continue;
                        const auto decoration=Call(q,q.hitActor,link);
                        if(Call(q,q.isA,decoration,OriginalMoveActorClass::Decoration))continue;
                    }
                }
                if(options[2u]){const auto peer=Call(q,q.hitActor,link);if(Call(q,q.basedOn,source,peer))continue;}
                const auto based=Call(q,q.hitActor,link);if(Call(q,q.basedOn,based,source))continue;
                const auto peer=Call(q,q.hitActor,link);potentialTouch=true;
                if(Call(q,q.blockedBy,source,peer)){hit=Call(q,q.hitPacket,link);break;}
            }
        }
    }
    const auto backed=BackoffOriginalActorMove(delta,geometry,hit.time,options[3u]);
    delta=backed.delta;hit.time=backed.time;
    Call(q,q.carryChildren,level,source,delta,requested,options[0u]);
    if(!options[0u]&&!options[3u]&&!Call(q,q.isA,source,OriginalMoveActorClass::Pawn)){
        if(Call(q,q.encroachment,level,source,Proposed(source,delta,q),requested,std::int32_t{}))return false;
        // Mark cleanup on this veto is DELIBERATELY stronger than retail;
        // earlier child/event effects and established Hit are NOT rolled back.
    }
    Publish(level,source,delta,requested,q);
    if(!options[0u]){
        if(hit.actor){
            const auto info=Info(level,q);
            if(hit.actor==info)Call(q,q.bumpWall,source,hit.location,hit.normal);
            else if(!Call(q,q.basedOn,source,hit.actor)){
                const auto receiver=hit.actor;Call(q,q.bump,receiver,source);
                Call(q,q.bump,source,hit.actor); // Live H.Actor AFTER the first callback.
            }
        }
        bool scan=potentialTouch;
        if(!scan){const auto flags=Call(q,q.collisionFlags,source);scan=!(flags&4u)||!(flags&8u);}
        if(scan)for(auto link=head;link;link=Call(q,q.hitNext,link)){
            Visit(visits,maximumHitVisits);
            const auto time=Call(q,q.hitTime,link);
            if(!BelowOrUnordered(time,hit.time))break; // Ordered equality stops the WHOLE list.
            const auto based=Call(q,q.hitActor,link);if(Call(q,q.basedOn,based,source))continue;
            if(options[2u]){const auto peer=Call(q,q.hitActor,link);if(Call(q,q.basedOn,source,peer))continue;}
            const auto blocked=Call(q,q.hitActor,link);if(Call(q,q.blockedBy,source,blocked))continue;
            const auto peer=Call(q,q.hitActor,link);Call(q,q.beginTouch,source,peer);
        }
        for(unsigned i=0u;i<4u;++i){const auto peer=Call(q,q.touching,source,i);
            if(peer&&!Call(q,q.overlaps,source,peer)){const auto current=Call(q,q.touching,source,i);Call(q,q.endTouch,source,current,std::int32_t{});}}
    }
    Call(q,q.setZone,level,source,options[0u],std::int32_t{});
    mark.Close();
    return PositiveOrdered(hit.time); // Read AFTER callbacks and mark release.
}
} // namespace QuestVr
