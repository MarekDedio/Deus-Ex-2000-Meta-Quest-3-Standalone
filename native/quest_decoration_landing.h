#pragma once

#include "quest_actor_line_trace.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

// Actual live native fields/ancestry and SAME-root Level/ProcessEvent/setPhysics
// providers. These opaque identities are not map indices or reflected offsets.
// No provider may replace the selected Landed event with Actor's empty body.
struct OriginalDecorationLandingQueries {
    std::function<bool(std::uintptr_t)> pawnFlag,isPawnClass,isDecorationClass,isCarcassClass;
    std::function<std::uintptr_t(std::uintptr_t)> zone,level;
    std::function<bool(std::uintptr_t)> zoneBounceVelocity;
    std::function<float(std::uintptr_t,unsigned)> zoneVelocity,location,velocity;
    std::function<float(std::uintptr_t)> radius,height;
    std::function<std::int32_t(std::uintptr_t)> numLandings;
    std::function<void(std::uintptr_t,std::int32_t)> writeNumLandings;
    std::function<void(std::uintptr_t,unsigned,std::uint32_t)> writeVelocityWord;
    // Original native INT clear result; end precedes start in Level's packet.
    std::function<std::int32_t(std::uintptr_t,OriginalActorLineHit&,std::uintptr_t,
        const PortableModelVec3&,const PortableModelVec3&,std::uint32_t,const PortableModelVec3&,std::int32_t)> singleLineCheck;
    std::function<void(std::uintptr_t,const PortableModelVec3&)> landed;
    std::function<std::uint8_t(std::uintptr_t)> physics;
    std::function<void(std::uintptr_t,std::uint8_t,std::uintptr_t)> setPhysics;
    // Borrow the current caller's work/journal. This coordinator creates none.
    std::function<void()> work;
};

struct OriginalDecorationLandingStats {
    std::size_t steps{},traceCalls{},counterWrites{},velocityWords{},landedCalls{},physicsReads{},setPhysicsCalls{};
    std::array<std::int32_t,4u> clear{};
    bool zoneBounce{},recovery{};
};

namespace DecorationLandingDetail {
template<class Provider,class... Arguments>
inline auto Call(const OriginalDecorationLandingQueries& queries,OriginalDecorationLandingStats& stats,
    const Provider& provider,Arguments&&... arguments) {
    if(!provider)throw std::invalid_argument("Original Decoration landing lacks an active native provider");
    if(queries.work)queries.work();
    ++stats.steps;
    return provider(std::forward<Arguments>(arguments)...);
}
inline bool IsNaN(float value) {
    const auto bits=std::bit_cast<std::uint32_t>(value);
    return (bits&0x7f800000u)==0x7f800000u&&(bits&0x007fffffu)!=0u;
}
// FCOMP/TEST C3 treats unordered as equal. Classify raw bits, including sNaNs,
// so an inactive component need not become a host floating arithmetic input.
inline bool ZeroOrUnordered(float value) {
    return (std::bit_cast<std::uint32_t>(value)&0x7fffffffu)==0u||IsNaN(value);
}
// FCOMP/TEST C0 branches for less OR unordered, unlike C++ '<' or std::clamp.
inline bool LessOrUnordered(float a,float b) {return IsNaN(a)||IsNaN(b)||a<b;}
inline std::int32_t AddInt(std::int32_t a,std::int32_t b) {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a)+std::bit_cast<std::uint32_t>(b));
}
inline std::int32_t SubInt(std::int32_t a,std::int32_t b) {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a)-std::bit_cast<std::uint32_t>(b));
}
inline float IntFloat(std::int32_t value) {
    const auto magnitude=value<0?-static_cast<std::int64_t>(value):static_cast<std::int64_t>(value);
    return ActorTimerDetail::Store32({static_cast<ActorTimerDetail::Wide>(magnitude),0,value<0});
}
inline float Add(float a,float b) {
    return ActorTimerDetail::Store32(NativeWindowClipDetail::Add64(ActorLineTraceDetail::Load(a),ActorLineTraceDetail::Load(b)));
}
inline float Sub(float a,float b) {
    return ActorTimerDetail::Store32(NativeWindowClipDetail::Add64(ActorLineTraceDetail::Load(a),
        NativeWindowClipDetail::Negate(ActorLineTraceDetail::Load(b))));
}
inline float Scale(float value,double scale) {
    return ActorTimerDetail::Store32(ModelFastLineDetail::Multiply64(ActorLineTraceDetail::Load(value),
        ScriptRandom::VectorDetail::DecodeDouble(scale)));
}
inline float HeightRadiusEnd(float height,float radius) {
    return ActorTimerDetail::Store32(NativeWindowClipDetail::Add64(NativeWindowClipDetail::Add64(
        ActorLineTraceDetail::Load(height),ActorLineTraceDetail::Load(radius)),ActorTimerDetail::Decode(8.0f)));
}
inline float Product(float a,float b) {
    return ActorTimerDetail::Store32(ModelFastLineDetail::Multiply64(ActorLineTraceDetail::Load(a),ActorLineTraceDetail::Load(b)));
}
} // namespace DecorationLandingDetail

// Owned GOTY processLanded103CEF60..103D04DF: zone shortcut, basic non-Pawn/
// non-Decoration selected-event path, and ordinary non-Carcass Decoration
// fit/recovery/selected-event path. The existing API name is retained.
// Pawn/Carcass branches explicitly refuse WHEN encountered.
// Class reads are not replaced with bIsPawn, spelling or a detached preset.
// Finite active arithmetic uses x87 PC64 dyadics and every original Float
// store. Nonfinite active arithmetic/Float overflow is not implemented and
// explicitly refuses; classification-only NaN/signed-zero reads remain valid.
// No local undo, floor snap, Base write, falling step or world scheduler lives
// here. Exceptions propagate to the SAME caller's whole-root rollback.
inline OriginalDecorationLandingStats ProcessOriginalDecorationLanded(std::uintptr_t actor,
    PortableModelVec3 suppliedNormal,std::uintptr_t suppliedFloor,
    const OriginalDecorationLandingQueries& queries) {
    if(!actor)throw std::invalid_argument("Original Decoration landing requires a nonnull Actor");
    using namespace DecorationLandingDetail;
    OriginalDecorationLandingStats stats;
    const auto read=[&](const auto& provider,auto... arguments){return Call(queries,stats,provider,arguments...);};
    const auto writeCounter=[&](std::int32_t value){++stats.counterWrites;Call(queries,stats,queries.writeNumLandings,actor,value);};
    const auto writeVelocity=[&](const PortableModelVec3& value) {
        ++stats.velocityWords;Call(queries,stats,queries.writeVelocityWord,actor,0u,std::bit_cast<std::uint32_t>(value.x));
        ++stats.velocityWords;Call(queries,stats,queries.writeVelocityWord,actor,1u,std::bit_cast<std::uint32_t>(value.y));
        ++stats.velocityWords;Call(queries,stats,queries.writeVelocityWord,actor,2u,std::bit_cast<std::uint32_t>(value.z));
    };
    if(!read(queries.pawnFlag,actor)) {
        const auto flagZone=read(queries.zone,actor);
        if(!flagZone)throw std::invalid_argument("Original Decoration landing reached null stored Zone");
        if(read(queries.zoneBounceVelocity,flagZone)) {
            const auto compareZone=read(queries.zone,actor);
            if(!compareZone)throw std::invalid_argument("Original Decoration landing reached null compared Zone");
            bool unequal{};
            for(unsigned channel=0u;channel<3u;++channel)
                if(!ZeroOrUnordered(read(queries.zoneVelocity,compareZone,channel))){unequal=true;break;}
            if(unequal) {
                const auto addZone=read(queries.zone,actor);
                if(!addZone)throw std::invalid_argument("Original Decoration landing reached null bounce Zone");
                // Original builds all three Float scratch values Z/Y/X before
                // committing raw Velocity.X/Y/Z; old Velocity is never read.
                const float z=Add(80.0f,read(queries.zoneVelocity,addZone,2u));
                const float y=Add(0.0f,read(queries.zoneVelocity,addZone,1u));
                const float x=Add(0.0f,read(queries.zoneVelocity,addZone,0u));
                writeVelocity({x,y,z});stats.zoneBounce=true;return stats;
            }
        }
    }
    if(read(queries.isPawnClass,actor))throw std::invalid_argument("Original Pawn processLanded is not implemented by Decoration coordinator");
    const bool decoration=read(queries.isDecorationClass,actor);
    // Failed original Decoration ancestry jumps directly to selected Landed
    // at103D02AB. Inventory actors have no Decoration counter/fit providers;
    // do not preflight, read or reset those inactive fields on this branch.
    if(decoration&&read(queries.numLandings,actor)>=5) {
        // This native branch precedes all queries AND the later Carcass cast.
        writeCounter(0);
    } else if(decoration) {
        OriginalActorLineHit hit;
        const float startDrop=Scale(read(queries.height,actor),0.8);
        PortableModelVec3 start;
        start.z=Sub(read(queries.location,actor,2u),startDrop);
        start.y=Sub(read(queries.location,actor,1u),0.0f);
        start.x=Sub(read(queries.location,actor,0u),0.0f);
        const float endHeight=read(queries.height,actor);
        const float endRadius=read(queries.radius,actor);
        const float endDrop=HeightRadiusEnd(endHeight,endRadius);
        PortableModelVec3 end;
        end.z=Sub(read(queries.location,actor,2u),endDrop);
        end.y=Sub(read(queries.location,actor,1u),0.0f);
        end.x=Sub(read(queries.location,actor,0u),0.0f);
        const auto trace=[&](const PortableModelVec3& queryEnd,const PortableModelVec3& queryStart,
            std::uint32_t flags,const PortableModelVec3& extent) {
            const auto level=read(queries.level,actor);
            if(!level)throw std::invalid_argument("Original Decoration landing reached null Level");
            ++stats.traceCalls;
            return Call(queries,stats,queries.singleLineCheck,level,hit,actor,queryEnd,queryStart,flags,extent,std::int32_t{});
        };
        static_cast<void>(trace(end,start,0x37u,{})); // Return INT is inactive.
        if(!hit.actor) {
            // Constructor arguments are read H/R/R, then multiplied Z/Y/X.
            const float extentHeight=read(queries.height,actor);
            const float extentRadiusY=read(queries.radius,actor);
            const float extentRadiusX=read(queries.radius,actor);
            PortableModelVec3 extent;
            extent.z=Scale(extentHeight,0.5);
            extent.y=Scale(extentRadiusY,0.5);
            extent.x=Scale(extentRadiusX,0.5);
            extent.z=Add(extent.z,extent.z);
            // A/B/C/D. Every endpoint rereads its own R/R and live Lz/Ly/Lx;
            // callback changes are real, but extent and hit packet are shared.
            constexpr std::array<std::array<double,2u>,4u> signs{{{0.5,0.5},{-0.5,0.5},{-0.5,-0.5},{0.5,-0.5}}};
            for(std::size_t i=0u;i<signs.size();++i) {
                const auto endpoint=[&](float zOffset) {
                    const float dx=Scale(read(queries.radius,actor),signs[i][0]);
                    const float dy=Scale(read(queries.radius,actor),signs[i][1]);
                    PortableModelVec3 point;
                    point.z=Add(zOffset,read(queries.location,actor,2u));
                    point.y=Add(dy,read(queries.location,actor,1u));
                    point.x=Add(dx,read(queries.location,actor,0u));
                    return point;
                };
                const auto cornerStart=endpoint(0.0f);
                const auto cornerEnd=endpoint(-8.0f);
                stats.clear[i]=trace(cornerEnd,cornerStart,0x17u,extent);
            }
            const auto a=stats.clear[0],b=stats.clear[1],c=stats.clear[2],d=stats.clear[3];
            if(AddInt(AddInt(AddInt(a,b),c),d)>1&&AddInt(a,c)!=0&&AddInt(b,d)!=0) {
                writeCounter(AddInt(read(queries.numLandings,actor),1));
                const float directionX=IntFloat(SubInt(SubInt(AddInt(a,d),b),c));
                const float directionY=IntFloat(SubInt(SubInt(AddInt(a,b),c),d));
                const float downSpeed=Scale(read(queries.velocity,actor,2u),-1.0);
                const float radiusCap=Add(30.0f,read(queries.radius,actor));
                const float selected=LessOrUnordered(downSpeed,30.0f)?30.0f:
                    LessOrUnordered(downSpeed,radiusCap)?downSpeed:radiusCap;
                const float speed=Product(2.0f,selected);
                const float z=Product(0.5f,speed);
                const float y=Product(directionY,speed);
                const float x=Product(directionX,speed);
                writeVelocity({x,y,z});stats.recovery=true;return stats;
            }
        }
        if(read(queries.isCarcassClass,actor))throw std::invalid_argument("Original Carcass processLanded is not implemented by Decoration coordinator");
        writeCounter(0);
    }
    // Preserve by-value caller normal/floor across selected script reentry.
    const PortableModelVec3 normal=suppliedNormal;
    ++stats.landedCalls;Call(queries,stats,queries.landed,actor,normal);
    ++stats.physicsReads;
    if(read(queries.physics,actor)==2u) {
        ++stats.setPhysicsCalls;Call(queries,stats,queries.setPhysics,actor,std::uint8_t{},suppliedFloor);
    }
    return stats;
}

} // namespace QuestVr
