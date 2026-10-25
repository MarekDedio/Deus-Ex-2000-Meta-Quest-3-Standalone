#pragma once

#include "quest_actor_rotation.h"

namespace QuestVr {

struct OriginalMoverInterpolationQueries {
    std::function<bool(std::uintptr_t)> isMoverClass,interpolating;
    std::function<std::uint8_t(std::uintptr_t)> keyNum,glideType;
    std::function<float(std::uintptr_t)> physAlpha,physRate;
    std::function<std::int32_t(std::uintptr_t,unsigned,unsigned)> keyRot;
    std::function<std::int32_t(std::uintptr_t,unsigned)> baseRot,oldRot;
    std::function<float(std::uintptr_t,unsigned,unsigned)> keyPos;
    std::function<float(std::uintptr_t,unsigned)> basePos,oldPos,location;
    std::function<std::uintptr_t(std::uintptr_t)> level;
    std::function<std::int32_t(std::uintptr_t,std::uintptr_t,PortableModelVec3,
        std::array<std::int32_t,3u>,OriginalActorLineHit&,std::array<std::int32_t,4u>)> moveActor;
    std::function<void(std::uintptr_t,float)> writePhysAlpha;
    std::function<void(std::uintptr_t,bool)> writeInterpolating;
    std::function<void(std::uintptr_t)> interpolateEnd;
    std::function<void()> work;
};

struct OriginalMoverInterpolationStats {
    std::size_t steps{},iterations{},moveCalls{},alphaWrites{},flagWrites{},eventCalls{};
    unsigned key{};
    bool mover{},entered{};
    float remaining{},alpha{},glide{};
};

namespace MoverInterpolationDetail {
using ActorLineTraceDetail::Load;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Compare64;
using NativeWindowClipDetail::Negate;
using ActorRotationDetail::Add;
using ActorRotationDetail::Sub;
template<class Provider,class... Args>
inline auto Call(const OriginalMoverInterpolationQueries& q,OriginalMoverInterpolationStats& s,
    const Provider& provider,Args&&... args) {
    if(!provider)throw std::invalid_argument("Original MovingBrush interpolation lacks an active native provider");
    if(q.work)q.work();
    ++s.steps;return provider(std::forward<Args>(args)...);
}
} // namespace MoverInterpolationDetail

// Owned GOTY AActor::physMovingBrush103DE690..103DEA40. This is the INNER
// native method: the Physics9 caller's OldLocation/Velocity work is separate.
// KeyNum is captured/clamped LOCALLY once, including the native adjacent-field
// aliases at index8. All movement and selected events borrow the caller root.
// No deletion filter, forced interpolation, presentation clock or local undo.
inline OriginalMoverInterpolationStats InterpolateOriginalMover(std::uintptr_t actor,float delta,
    const OriginalMoverInterpolationQueries& q) {
    if(!actor)throw std::invalid_argument("Original MovingBrush requires a nonnull Actor");
    using namespace MoverInterpolationDetail;
    OriginalMoverInterpolationStats s;s.remaining=delta;
    const auto call=[&](const auto& provider,auto... args){return Call(q,s,provider,args...);};
    s.mover=call(q.isMoverClass,actor);if(!s.mover)return s;
    s.key=std::min<unsigned>(call(q.keyNum,actor),8u);
    const auto one=Decode(1.0f);
    while(call(q.interpolating,actor)) {
        // The original unordered/nonpositive comparison returns without using
        // any alpha/rate/pose operand. Do not eagerly admit inactive arithmetic.
        if(!(s.remaining>0.0f))return s;
        ++s.iterations;s.entered=true;
        const auto elapsed=Load(s.remaining);
        const auto rate=Load(call(q.physRate,actor));
        const auto product=Multiply64(elapsed,rate);
        auto alpha=Add64(product,Load(call(q.physAlpha,actor)));
        s.alpha=Store32(alpha); // FST does not reload the retained PC64 alpha.
        if(Compare64(alpha,one)>0) {
            const auto numerator=Add64(alpha,Negate(one));
            const auto denominator=Add64(alpha,Negate(Load(call(q.physAlpha,actor))));
            s.remaining=Store32(Multiply64(ModelFastLineDetail::Divide64(numerator,denominator),Load(s.remaining)));
            alpha=one;s.alpha=1.0f;
        } else s.remaining=0.0f;
        if(call(q.glideType,actor)==1u) {
            const auto square=Multiply64(alpha,alpha);
            const auto triple=Multiply64(square,Decode(3.0f));
            const auto cube=Multiply64(alpha,square);
            s.glide=Store32(Add64(triple,Negate(Add64(cube,cube))));
        } else s.glide=Store32(alpha);

        // Preserve literal native INT wrap and load order before _ftol. These
        // are raw Unreal rotation units, never shortest-arc/degree interpolation.
        const auto baseRoll=call(q.baseRot,actor,2u);
        const auto baseYaw=call(q.baseRot,actor,1u);
        const auto keyRoll=call(q.keyRot,actor,s.key,2u);
        const auto targetRoll=Add(keyRoll,baseRoll);
        const auto keyPitch=call(q.keyRot,actor,s.key,0u);
        const auto keyYaw=call(q.keyRot,actor,s.key,1u);
        const auto targetYaw=Add(keyYaw,baseYaw);
        const auto targetPitch=Add(call(q.baseRot,actor,0u),keyPitch);
        const auto rollDelta=Sub(targetRoll,call(q.oldRot,actor,2u));
        const auto oldPitch=call(q.oldRot,actor,0u);
        const auto oldYaw=call(q.oldRot,actor,1u);
        const auto yawDelta=Sub(targetYaw,oldYaw);
        const auto pitchDelta=Sub(targetPitch,oldPitch);
        const auto rollStep=OriginalActorRotationDelta(rollDelta,s.glide);
        const auto yawStep=OriginalActorRotationDelta(yawDelta,s.glide);
        const auto pitchStep=OriginalActorRotationDelta(pitchDelta,s.glide);
        std::array<std::int32_t,3u> rotation;
        rotation[2u]=Add(call(q.oldRot,actor,2u),rollStep);
        rotation[1u]=Add(call(q.oldRot,actor,1u),yawStep);
        rotation[0u]=Add(call(q.oldRot,actor,0u),pitchStep);

        std::array<float,3u> target{},difference{},scaled{},position{},translation{};
        for(unsigned i=3u;i>0u;--i) {
            const auto c=i-1u;
            // X reverses the load order in the original vector addition.
            if(c==0u) {
                const auto key=Load(call(q.keyPos,actor,s.key,c));
                target[c]=Store32(Add64(key,Load(call(q.basePos,actor,c))));
            } else {
                const auto base=Load(call(q.basePos,actor,c));
                target[c]=Store32(Add64(base,Load(call(q.keyPos,actor,s.key,c))));
            }
        }
        for(unsigned i=3u;i>0u;--i) {const auto c=i-1u;
            difference[c]=Store32(Add64(Load(target[c]),Negate(Load(call(q.oldPos,actor,c)))));}
        for(unsigned i=3u;i>0u;--i) {const auto c=i-1u;
            scaled[c]=Store32(Multiply64(Load(difference[c]),Load(s.glide)));}
        for(unsigned i=3u;i>0u;--i) {const auto c=i-1u;
            position[c]=Store32(Add64(Load(scaled[c]),Load(call(q.oldPos,actor,c))));}
        const auto level=call(q.level,actor);
        // Imported Core FVector subtraction10117780: X/Y/Z Float stores.
        for(unsigned c=0u;c<3u;++c) {
            translation[c]=Store32(Add64(Load(position[c]),Negate(Load(call(q.location,actor,c)))));}
        OriginalActorLineHit hit;
        ++s.moveCalls;
        if(!call(q.moveActor,level,actor,PortableModelVec3{translation[0u],translation[1u],translation[2u]},
            rotation,hit,std::array<std::int32_t,4u>{}))continue;
        ++s.alphaWrites;call(q.writePhysAlpha,actor,s.alpha);
        // This compares the captured Float, not retained alpha nor a fresh
        // PhysAlpha that a MoveActor callback may have altered.
        if(s.alpha==1.0f) {
            ++s.flagWrites;call(q.writeInterpolating,actor,false);
            ++s.eventCalls;call(q.interpolateEnd,actor);
        }
    }
    return s;
}

} // namespace QuestVr
