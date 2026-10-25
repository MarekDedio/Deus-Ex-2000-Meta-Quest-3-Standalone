#pragma once

#include "quest_pawn_line_of_sight.h"

#include <cstddef>

namespace QuestVr {

// Internal APawn.LineOfSightTo for an authenticated nonnull Pawn target and
// the exact PickTarget arguments (checkSight=0, ignore=false). Not script514,
// an actor trace, a published world, or a general non-Pawn/sight-check path.
// Ops owns live field authority, directed BSP FastLine, native work/RNG and
// journaled writes in its caller's SAME execution. Failures propagate unchanged.
// Getters intentionally remain lazy and are sequenced separately: later query
// and enemy-history reads must not borrow one eagerly captured actor snapshot.
template<class Ops>
inline bool CheckOriginalPawnLineOfSightToPawn(Ops& ops) {
    using ActorTimerDetail::Decode;
    using NativeWindowClipDetail::Compare64;
    using ModelFastLineDetail::Multiply64;

    // 103BEB9C reads Enemy even though the supplied checkSight is already0.
    static_cast<void>(ops.TargetIsEnemy());
    const auto distanceTarget=ops.TargetLocation();
    const auto distanceSource=ops.SourceLocation();
    const auto distance=PawnLineOfSightDetail::DistanceSquared(distanceTarget,distanceSource);
    const auto player=ops.IsPlayer();
    const auto visibility=ops.Visibility();
    const auto range=PawnLineOfSightDetail::RangeSquared(player,visibility);
    if(Compare64(Decode(distance),range)>0)return false;

    // 103BEE60..8C: copy source Location, read Enemy, THEN add eye height.
    const auto eyeSource=ops.SourceLocation();
    const bool enemy=ops.TargetIsEnemy();
    const auto eyeHeight=ops.BaseEyeHeight();
    const auto eye=PawnLineOfSightDetail::Eye(eyeSource,eyeHeight);
    if(enemy) {
        const auto centre=ops.TargetLocation();
        if(ops.FastLine(centre,eye)) {
            const auto currentSource=ops.SourceLocation();
            ops.WriteLastSeeingPos(currentSource);
            ops.WriteLastSeenPosFromCurrentEnemy();
            return true;
        }
        // Second enemy centre ray uses a fresh source ground Location first,
        // followed by a fresh target Location, not the previous ray's copies.
        const auto ground=ops.SourceLocation();
        const auto centreAgain=ops.TargetLocation();
        if(ops.FastLine(centreAgain,ground)) {
            const auto currentSource=ops.SourceLocation();
            ops.WriteLastSeeingPos(currentSource);
            ops.WriteLastSeenPosFromCurrentEnemy();
            return true;
        }
        if(distance>1000000.0f)return false;
    } else if(distance>1000000.0f) {
        // Both flags come from the live far-branch field read, not IsPlayer's
        // earlier range decision. Equality at half range and at FRand0.5 passes.
        const auto flags=ops.DistantFlags();
        if(!flags.losFlag && Compare64(Decode(distance),Multiply64(range,Decode(0.5f)))>0)return false;
        if(!flags.player) {
            const auto draw=ops.Draw15();
            if(!PawnLineOfSightDetail::RandomVisible(draw))return false;
        }
        const auto centre=ops.TargetLocation();
        return ops.FastLine(centre,eye);
    }

    const auto headTarget=ops.TargetLocation();
    const auto collisionHeight=ops.CollisionHeight();
    const auto head=PawnLineOfSightDetail::Head(headTarget,collisionHeight);
    if(ops.FastLine(head,eye))return true; // No enemy-history stores on this path.
    if(distance>250000.0f)return false;

    const auto radius=ops.CollisionRadius();
    const auto cornerTarget=ops.TargetLocation();
    const auto corners=PawnLineOfSightDetail::SelectCorners(cornerTarget,radius);
    // 103BF414 reads bLOSflag even though checkSight0 cannot alternate rays.
    static_cast<void>(ops.SideFlag());
    for(std::size_t index=0u;index<4u;++index) {
        if(index==corners.minimum || index==corners.maximum)continue;
        if(ops.FastLine(corners.points[index],eye))return true;
    }
    return false;
}

} // namespace QuestVr
