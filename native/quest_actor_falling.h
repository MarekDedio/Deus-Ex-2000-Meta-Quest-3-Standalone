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

// Live original fields and SAME-root native movement/event providers. These
// identities are opaque Actor/Level/Zone handles, not detached map presets.
struct OriginalActorFallingQueries {
    std::function<bool(std::uintptr_t)> isPawnClass,isPlayerPawnClass,isDecorationClass;
    std::function<bool(std::uintptr_t)> pawnFlag,bounce,justTeleported,deleted,bobbing,zoneWater;
    std::function<std::uint8_t(std::uintptr_t)> zoneNumber;
    std::function<std::uintptr_t(std::uintptr_t)> zone,level;
    std::function<float(std::uintptr_t,unsigned)> location,oldLocation,velocity,acceleration,zoneGravity,zoneVelocity;
    std::function<float(std::uintptr_t)> terminal;
    // Literal native DWORD stores preserve inactive raw IEEE data. There is
    // no whole-Vector assignment, implicit animation rebuild or local undo.
    std::function<void(std::uintptr_t,unsigned,std::uint32_t)> writeOldLocationWord,writeVelocityWord,writeAccelerationWord;
    std::function<void(std::uintptr_t,bool)> writeJustTeleported;
    std::function<std::int32_t(std::uintptr_t)> numLandings;
    std::function<void(std::uintptr_t,std::int32_t)> writeNumLandings;
    std::function<std::array<std::int32_t,3u>(std::uintptr_t)> currentRotation;
    std::function<std::int32_t(std::uintptr_t,std::uintptr_t,PortableModelVec3,
        std::array<std::int32_t,3u>,OriginalActorLineHit&,std::array<std::int32_t,4u>)> moveActor;
    std::function<void(std::uintptr_t,PortableModelVec3,std::uintptr_t,float,std::int32_t)> processLanded;
    std::function<void(std::uintptr_t)> fellOutOfWorld;
    std::function<void()> work;
};

struct OriginalActorFallingStats {
    std::size_t steps{},substeps{},apexSplits{},moveCalls{},oldLocationWords{},velocityWords{},
        accelerationWords{},teleportWrites{},counterWrites{},landedCalls{},outOfWorldCalls{};
    bool landed{},deleted{},outOfWorld{};
    float remaining{};
    std::int32_t iterations{};
};

namespace ActorFallingDetail {
using ActorLineTraceDetail::Load;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;
using ScriptRandom::VectorDetail::DecodeDouble;

template<class Provider,class... Arguments>
inline auto Call(const OriginalActorFallingQueries& queries,OriginalActorFallingStats& stats,
    const Provider& provider,Arguments&&... arguments) {
    if(!provider)throw std::invalid_argument("Original actor falling lacks an active native provider");
    if(queries.work)queries.work();
    ++stats.steps;
    return provider(std::forward<Arguments>(arguments)...);
}
inline Dyadic Sub(const Dyadic& a,const Dyadic& b){return Add64(a,Negate(b));}
inline Dyadic Absolute(Dyadic value){value.negative=false;return value;}
inline std::int32_t AddInt(std::int32_t a,std::int32_t b) {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a)+std::bit_cast<std::uint32_t>(b));
}
inline float Channel(const PortableModelVec3& value,unsigned channel) {
    return channel==0u?value.x:channel==1u?value.y:value.z;
}
inline void Channel(PortableModelVec3& value,unsigned channel,float scalar) {
    if(channel==0u)value.x=scalar;else if(channel==1u)value.y=scalar;else value.z=scalar;
}
} // namespace ActorFallingDetail

// Owned GOTY AActor::physFalling103D0A50..103D248E. This coordinator implements
// the finite dry non-Pawn/non-bobbing branch, including adaptive subdivision,
// apex resplitting, actual swept movement, Decoration collision-counter edits,
// ordinary landing and final velocity/terminal-speed correction. Core PC64
// arithmetic and EVERY original Float spill are retained. Active nonfinite
// arithmetic/Float overflow explicitly refuses through the caller's root.
//
// The reached Pawn, water, bobbing, bounce-collision and wall/slide branches
// explicitly refuse; they are not empty implementations. There is no forced
// floor, local transaction/rollback, world scheduler, performPhysics rotation
// or PendingTouch epilogue here. Selected callbacks may mutate/delete/reenter
// the actor synchronously. The original early returns DO NOT restore captured
// Acceleration; only exhaustion of the time/iteration loop does so.
inline OriginalActorFallingStats StepOriginalActorFalling(std::uintptr_t actor,float suppliedDelta,
    std::int32_t suppliedIterations,const OriginalActorFallingQueries& queries) {
    if(!actor)throw std::invalid_argument("Original actor falling requires a nonnull Actor");
    using namespace ActorFallingDetail;
    OriginalActorFallingStats stats;
    stats.remaining=suppliedDelta;stats.iterations=suppliedIterations;
    const auto read=[&](const auto& provider,auto... arguments){return Call(queries,stats,provider,arguments...);};
    const auto words=[&](const auto& provider,const PortableModelVec3& value,std::size_t& counter) {
        for(unsigned channel=0u;channel<3u;++channel) {
            ++counter;Call(queries,stats,provider,actor,channel,std::bit_cast<std::uint32_t>(Channel(value,channel)));
        }
    };
    const auto readVector=[&](const auto& provider,std::uintptr_t owner,bool descending) {
        PortableModelVec3 value;
        if(descending)for(unsigned i=3u;i>0u;--i)Channel(value,i-1u,read(provider,owner,i-1u));
        else for(unsigned i=0u;i<3u;++i)Channel(value,i,read(provider,owner,i));
        return value;
    };
    const auto storedZone=[&] {
        const auto result=read(queries.zone,actor);
        if(!result)throw std::invalid_argument("Original actor falling reached null stored Zone");
        return result;
    };
    // Actual ancestry cast and Pawn preprocessing precede the ZoneNumber test.
    if(read(queries.isPawnClass,actor))throw std::invalid_argument("Original Pawn physFalling preprocessing is not implemented");
    if(read(queries.zoneNumber,actor)==0u) {
        ++stats.outOfWorldCalls;Call(queries,stats,queries.fellOutOfWorld,actor);
        stats.outOfWorld=true;return stats;
    }
    // Raw capture: a nonfinite Acceleration can remain inactive on a zero-time
    // step, and the final stores must preserve all original words exactly.
    const auto originalAcceleration=readVector(queries.acceleration,actor,false);
    OriginalActorLineHit hit;
    bool apexFlag{};
    float delta=suppliedDelta;
    while(Compare64(Load(stats.remaining),Decode(0.0f))>0&&stats.iterations<8) {
        stats.iterations=AddInt(stats.iterations,1);++stats.substeps;
        if(Compare64(Load(stats.remaining),DecodeDouble(0.1))>0) {
            const auto half=Multiply64(Load(stats.remaining),Decode(0.5f));
            delta=Compare64(Decode(0.1f),half)>0?Store32(half):0.1f;
        } else delta=stats.remaining;
        // Original scalar copy/store order, not one Vector snapshot/write.
        for(unsigned channel=0u;channel<3u;++channel) {
            const float value=read(queries.location,actor,channel);
            ++stats.oldLocationWords;
            Call(queries,stats,queries.writeOldLocationWord,actor,channel,std::bit_cast<std::uint32_t>(value));
        }
        ++stats.teleportWrites;Call(queries,stats,queries.writeJustTeleported,actor,false);
        const auto oldVelocity=readVector(queries.velocity,actor,false);
        stats.remaining=Store32(Sub(Load(stats.remaining),Load(delta)));

        // Repeat this selection after a successful apex split: it reads the
        // actual current stored Zone/class/flags, not a cached Training profile.
        const auto provisional=[&](float step) {
            const auto flagZone=storedZone();
            if(read(queries.zoneWater,flagZone))throw std::invalid_argument("Original water physFalling is not implemented");
            if(read(queries.isDecorationClass,actor)&&read(queries.bobbing,actor))
                throw std::invalid_argument("Original bobbing Decoration physFalling is not implemented");
            // For a genuine non-Pawn this cast is false. Keep the read, since
            // the original routine has a separate PlayerPawn foot-water path.
            if(read(queries.isPlayerPawnClass,actor))
                throw std::invalid_argument("Original PlayerPawn foot-water physFalling is not implemented");
            const auto gravityZone=storedZone();
            PortableModelVec3 sum,half,impulse,result;
            for(unsigned i=3u;i>0u;--i) {
                const unsigned channel=i-1u;
                const auto gravity=Load(read(queries.zoneGravity,gravityZone,channel));
                const auto acceleration=Load(read(queries.acceleration,actor,channel));
                Channel(sum,channel,Store32(Add64(gravity,acceleration)));
            }
            for(unsigned i=3u;i>0u;--i)Channel(half,i-1u,Store32(Multiply64(Load(Channel(sum,i-1u)),Decode(0.5f))));
            for(unsigned i=3u;i>0u;--i)Channel(impulse,i-1u,Store32(Multiply64(Load(Channel(half,i-1u)),Load(step))));
            for(unsigned i=3u;i>0u;--i)Channel(result,i-1u,Store32(Add64(Load(Channel(impulse,i-1u)),Load(Channel(oldVelocity,i-1u)))));
            words(queries.writeVelocityWord,result,stats.velocityWords);
        };
        provisional(delta);

        bool splitCandidate{};
        if(!apexFlag) {
            const auto oldZ=Load(oldVelocity.z);
            const auto newZ=Load(read(queries.velocity,actor,2u));
            if((Compare64(oldZ,Decode(0.0f))>0)!=(Compare64(newZ,Decode(0.0f))>0)&&
                Compare64(Absolute(oldZ),Decode(5.0f))>0) {
                const auto magnitude=Absolute(Load(read(queries.velocity,actor,2u)));
                splitCandidate=Compare64(magnitude,Decode(5.0f))>0;
            }
        }
        if(splitCandidate) {
            apexFlag=true;
            const auto oldMagnitude=Absolute(Load(oldVelocity.z));
            const auto newMagnitude=Absolute(Load(read(queries.velocity,actor,2u)));
            const auto ratio=ModelFastLineDetail::Divide64(oldMagnitude,Add64(newMagnitude,oldMagnitude));
            const auto first=Multiply64(ratio,Load(delta));
            const float firstStored=Store32(first); // FST does not pop retained PC64 value.
            if(Compare64(first,DecodeDouble(0.015))>0) {
                const auto second=Multiply64(Sub(Decode(1.0f),ratio),Load(delta));
                if(Compare64(second,DecodeDouble(0.015))>0) {
                    delta=firstStored;
                    stats.remaining=Store32(Add64(second,Load(stats.remaining)));
                    ++stats.apexSplits;provisional(delta);
                }
            }
        } else apexFlag=false;

        PortableModelVec3 drift{};
        bool includeDrift=!read(queries.pawnFlag,actor);
        if(!includeDrift) {
            includeDrift=read(queries.isPlayerPawnClass,actor);
            if(!includeDrift) {
                const auto speedZone=storedZone();
                const auto zoneSpeed=readVector(queries.zoneVelocity,speedZone,true);
                includeDrift=Compare64(ActorLineTraceDetail::Squared(zoneSpeed),Decode(40000.0f))>0;
            }
        }
        if(includeDrift)drift=readVector(queries.zoneVelocity,storedZone(),false);
        PortableModelVec3 sum,move;
        for(unsigned i=3u;i>0u;--i)Channel(sum,i-1u,Store32(Add64(Load(Channel(drift,i-1u)),Load(read(queries.velocity,actor,i-1u)))));
        for(unsigned i=3u;i>0u;--i)Channel(move,i-1u,Store32(Multiply64(Load(Channel(sum,i-1u)),Load(delta))));
        const auto rotation=read(queries.currentRotation,actor);
        const auto level=read(queries.level,actor);
        if(!level)throw std::invalid_argument("Original actor falling reached null Level");
        ++stats.moveCalls;
        static_cast<void>(Call(queries,stats,queries.moveActor,level,actor,move,rotation,hit,std::array<std::int32_t,4u>{}));
        if(read(queries.deleted,actor)){stats.deleted=true;return stats;}

        const bool collided=Compare64(Load(hit.time),DecodeDouble(1.0))<0;
        if(collided) {
            if(!hit.actor)throw std::invalid_argument("Original actor falling collision has null hit Actor");
            if(read(queries.isPlayerPawnClass,hit.actor)&&read(queries.isDecorationClass,actor)) {
                auto count=AddInt(read(queries.numLandings,actor),-1);
                if(count<=0)count=0;
                ++stats.counterWrites;Call(queries,stats,queries.writeNumLandings,actor,count);
            }
            if(read(queries.bounce,actor))throw std::invalid_argument("Original bounce-collision physFalling is not implemented");
            if(Compare64(Load(hit.normal.z),DecodeDouble(0.7))<=0)
                throw std::invalid_argument("Original wall/slide physFalling is not implemented");
            stats.remaining=Store32(Add64(Multiply64(Sub(DecodeDouble(1.0),Load(hit.time)),Load(delta)),Load(stats.remaining)));
            if(!read(queries.justTeleported,actor)&&Compare64(Load(hit.time),DecodeDouble(0.1))>0&&
                Compare64(Multiply64(Load(hit.time),Load(delta)),Decode(0.003f))>0) {
                PortableModelVec3 displacement,value;
                for(unsigned i=3u;i>0u;--i) {
                    const unsigned channel=i-1u;
                    const auto position=Load(read(queries.location,actor,channel));
                    const auto old=Load(read(queries.oldLocation,actor,channel));
                    Channel(displacement,channel,Store32(Sub(position,old)));
                }
                const auto inverse=ModelFastLineDetail::Divide64(DecodeDouble(1.0),Multiply64(Load(hit.time),Load(delta)));
                for(unsigned i=3u;i>0u;--i)Channel(value,i-1u,Store32(Multiply64(Load(Channel(displacement,i-1u)),inverse)));
                words(queries.writeVelocityWord,value,stats.velocityWords);
            }
            ++stats.landedCalls;
            Call(queries,stats,queries.processLanded,actor,hit.normal,hit.actor,stats.remaining,stats.iterations);
            stats.landed=true;return stats;
        }

        // Original common tail, including independent live flag rereads after
        // movement callbacks. A teleport skips BOTH correction and clamp.
        if(read(queries.bounce,actor)&&collided)continue;
        if(read(queries.justTeleported,actor))continue;
        PortableModelVec3 displacement,scaled,value;
        for(unsigned i=3u;i>0u;--i) {
            const unsigned channel=i-1u;
            const auto position=Load(read(queries.location,actor,channel));
            const auto old=Load(read(queries.oldLocation,actor,channel));
            Channel(displacement,channel,Store32(Sub(position,old)));
        }
        const auto inverse=ModelFastLineDetail::Divide64(DecodeDouble(1.0),Load(delta));
        for(unsigned i=3u;i>0u;--i)Channel(scaled,i-1u,Store32(Multiply64(Load(Channel(displacement,i-1u)),inverse)));
        for(unsigned channel=0u;channel<3u;++channel)Channel(value,channel,Store32(Sub(Load(Channel(scaled,channel)),Load(Channel(drift,channel)))));
        words(queries.writeVelocityWord,value,stats.velocityWords);
        const auto newZ=Load(read(queries.velocity,actor,2u));
        if(Compare64(newZ,Load(oldVelocity.z))<0||Compare64(Load(oldVelocity.z),Decode(0.0f))>=0) {
            const auto current=readVector(queries.velocity,actor,true);
            PortableModelVec3 doubled,corrected;
            for(unsigned i=3u;i>0u;--i)Channel(doubled,i-1u,Store32(Multiply64(Decode(2.0f),Load(Channel(current,i-1u)))));
            for(unsigned channel=0u;channel<3u;++channel)Channel(corrected,channel,Store32(Sub(Load(Channel(doubled,channel)),Load(Channel(oldVelocity,channel)))));
            words(queries.writeVelocityWord,corrected,stats.velocityWords);
        }
        const auto current=readVector(queries.velocity,actor,true);
        const float speedSquared=Store32(ActorLineTraceDetail::Squared(current));
        const auto terminalZone=storedZone();
        const auto terminal=Load(read(queries.terminal,terminalZone));
        if(Compare64(Load(speedSquared),Multiply64(terminal,terminal))>0) {
            const auto normalized=ActorLineTraceDetail::Normal(readVector(queries.velocity,actor,true),true);
            words(queries.writeVelocityWord,normalized,stats.velocityWords);
            const auto clampZone=storedZone();
            const float limit=read(queries.terminal,clampZone);
            // Original imported operator*= performs X/Y/Z scalar stores, not
            // a precomputed Vector followed by one visible assignment.
            for(unsigned channel=0u;channel<3u;++channel) {
                const float component=read(queries.velocity,actor,channel);
                const float product=Store32(Multiply64(Load(limit),Load(component)));
                ++stats.velocityWords;
                Call(queries,stats,queries.writeVelocityWord,actor,channel,std::bit_cast<std::uint32_t>(product));
            }
        }
    }
    words(queries.writeAccelerationWord,originalAcceleration,stats.accelerationWords);
    return stats;
}

} // namespace QuestVr
