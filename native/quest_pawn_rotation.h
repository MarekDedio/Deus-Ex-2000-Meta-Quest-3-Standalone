#pragma once

#include "quest_actor_rotation.h"

namespace QuestVr {

struct OriginalPawnRotationQueries {
    // Native word+1EC, or a relevant-bit lens whose store preserves all bits
    // outside the lens in the provider's actual typed storage. The helper's
    // unconditional edit changes only bit20/40 in its captured word.
    std::function<std::uint32_t(std::uintptr_t)> rotationFlags;
    std::function<void(std::uintptr_t,std::uint32_t)> writeRotationFlags;
    std::function<std::int32_t(std::uintptr_t,unsigned)> rotation,desiredRotation,rotationRate;
    std::function<bool(std::uintptr_t)> isPlayerPawnClass,isPawnClass;
    std::function<std::uint8_t(std::uintptr_t)> physics;
    std::function<std::uintptr_t(std::uintptr_t)> level;
    std::function<std::int32_t(std::uintptr_t,std::uintptr_t,PortableModelVec3,
        std::array<std::int32_t,3u>,OriginalActorLineHit&,std::array<std::int32_t,4u>)> moveActor;
    std::function<void()> work;
};
struct OriginalPawnRotationStats {
    std::size_t steps{},rotationReads{},desiredReads{},rateReads{},flagReads{},flagWrites{},
        playerCasts{},pawnCasts{},physicsReads{},conversions{},fixedTurns{},moveCalls{};
    std::array<std::int32_t,3u> captured{},proposed{};
    std::int32_t yawDelta{},pitchDelta{};
    bool playerPawn{},pitchAdjusted{},pitchClamped{},changed{};
};
namespace PawnRotationDetail {
template<class Provider,class... Args>
inline auto Call(const OriginalPawnRotationQueries& q,OriginalPawnRotationStats& out,
    const Provider& provider,Args&&... args) {
    if(!provider)throw std::invalid_argument("Original Pawn rotation lacks a reached provider");
    if(!q.work)throw std::invalid_argument("Original Pawn rotation lacks shared work authority");
    q.work();++out.steps;return provider(std::forward<Args>(args)...);
}
} // namespace PawnRotationDetail

// Complete owned GOTY APawn.physicsRotation103C9F30..103CA11B, not Actor's
// different rotation coordinator. The native old-Velocity argument is unused.
// Every effect borrows the same caller root. No local undo, direct Rotation
// assignment, EndedRotation probe/event, deletion gate or flag restore exists.
inline OriginalPawnRotationStats RotateOriginalPawn(std::uintptr_t actor,float wholeDelta,
    const std::array<float,3u>& /*unusedOldVelocity*/,const OriginalPawnRotationQueries& q) {
    if(!actor)throw std::invalid_argument("Original Pawn rotation requires a nonnull receiver");
    using PawnRotationDetail::Call;
    OriginalPawnRotationStats out;
    const auto rotation=[&](unsigned axis){++out.rotationReads;return Call(q,out,q.rotation,actor,axis);};
    const auto desired=[&](unsigned axis){++out.desiredReads;return Call(q,out,q.desiredRotation,actor,axis);};
    const auto rate=[&](unsigned axis){++out.rateReads;return Call(q,out,q.rotationRate,actor,axis);};
    const auto flags=[&](){++out.flagReads;return Call(q,out,q.rotationFlags,actor);};
    const auto turn=[&](std::int32_t current,std::int32_t target,std::int32_t delta) {
        ++out.fixedTurns;
        // fixedTurn still runs with delta0 (normalizes current low16), but
        // its actual native zero arm reads no packed flags.
        const auto word=delta?static_cast<std::uint8_t>(flags()):std::uint8_t{};
        return OriginalActorFixedTurn(current,target,delta,word);
    };
    for(unsigned axis=0u;axis<3u;++axis)out.captured[axis]=rotation(axis);
    out.proposed={out.captured[0u],out.captured[1u],0};
    ++out.playerCasts;out.playerPawn=Call(q,out,q.isPlayerPawnClass,actor);
    if(!out.playerPawn) {
        const auto yawRate=rate(1u);
        ++out.conversions;out.yawDelta=OriginalActorRotationDelta(yawRate,wholeDelta);
        const auto updatedFlags=(flags()&~std::uint32_t{0x20u})|std::uint32_t{0x40u};
        ++out.flagWrites;Call(q,out,q.writeRotationFlags,actor,updatedFlags);
        const auto targetYaw=desired(1u);
        if(targetYaw!=out.captured[1u])out.proposed[1u]=turn(out.captured[1u],targetYaw,out.yawDelta);
        if(desired(0u)!=out.captured[0u]) {
            out.pitchAdjusted=true;
            // FILD32768 and FLD Float32768 are exactly the same power of two;
            // reuse the authenticated signed64 CRT/low32 conversion service.
            ++out.conversions;out.pitchDelta=OriginalActorRotationDelta(32768,wholeDelta);
            ++out.physicsReads;const auto mode=Call(q,out,q.physics,actor);
            bool flatten=mode==1u;
            if(mode==2u){++out.pawnCasts;flatten=Call(q,out,q.isPawnClass,actor);}
            const auto targetPitch=flatten?std::int32_t{}:desired(0u);
            auto pitch=turn(out.captured[0u],targetPitch,out.pitchDelta);
            const auto pitchRate=rate(0u);
            if(pitch<32768) {
                if(pitch>pitchRate){pitch=pitchRate;out.pitchClamped=true;}
            } else {
                const auto lower=ActorRotationDetail::Sub(65536,pitchRate);
                if(pitch<lower){pitch=lower;out.pitchClamped=true;}
            }
            out.proposed[0u]=pitch;
        }
    }
    for(unsigned axis=0u;axis<3u;++axis) {
        if(out.proposed[axis]!=rotation(axis)){out.changed=true;break;}
    }
    if(out.changed) {
        OriginalActorLineHit hit;
        const auto level=Call(q,out,q.level,actor);
        if(!level)throw std::invalid_argument("Original Pawn rotation reached null native Level");
        ++out.moveCalls;
        static_cast<void>(Call(q,out,q.moveActor,level,actor,PortableModelVec3{},out.proposed,hit,
            std::array<std::int32_t,4u>{}));
        // Both result and modified hit packet are inactive; return directly.
    }
    return out;
}
} // namespace QuestVr
