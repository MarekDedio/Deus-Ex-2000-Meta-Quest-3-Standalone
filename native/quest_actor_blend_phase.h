#pragma once

#include "quest_actor_animation_phase.h"

namespace QuestVr {

enum class OriginalActorBlendFloat { Frame,Rate,TweenRate,Last,MinRate,SimX,SimY };
struct OriginalActorBlendPhaseQueries {
    std::function<std::string(std::uintptr_t)> mainSequence;
    std::function<float(std::uintptr_t,OriginalActorAnimationFloat)> readMainFloat;
    std::function<std::string(std::uintptr_t,unsigned)> sequence;
    std::function<float(std::uintptr_t,unsigned,OriginalActorBlendFloat)> readFloat;
    std::function<void(std::uintptr_t,unsigned,OriginalActorBlendFloat,float)> writeFloat;
    // Actual global Velocity channels; native loads Z,Y,X in that order.
    std::function<float(std::uintptr_t,unsigned)> velocity;
    std::function<std::uint8_t(std::uintptr_t)> remoteRole;
    std::function<bool(std::uintptr_t)> isWeapon;
    std::function<void()> work;
    std::size_t maxNameBytes{65'536u};
};
struct OriginalActorBlendPhaseStats {
    std::size_t steps{},iterations{},slotVisits{},floatReads{},floatWrites{},
        sequenceReads{},roleReads{},weaponChecks{},initialCounter{},nativeCounter{};
    bool inactive{},iterationCap{};
    float remainingSeconds{};
};
namespace OriginalActorBlendPhaseDetail {
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Store32;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Negate;
using NativeWindowClipDetail::Compare64;
using ModelFastLineDetail::Multiply64;
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original Actor blend animation: ")+reason);
}
inline float Finite(float value) {
    if(!std::isfinite(value))Fail("active nonfinite Float refused");
    return value;
}
template<class Provider,class... Arguments>
inline auto Call(const OriginalActorBlendPhaseQueries& q,OriginalActorBlendPhaseStats& stats,
    const Provider& provider,Arguments&&... arguments) {
    if(!provider)Fail("active native provider is missing");
    if(q.work)q.work();
    ++stats.steps;return provider(std::forward<Arguments>(arguments)...);
}
inline Dyadic Difference(float a,float b) {return Add64(Decode(a),Negate(Decode(b)));}
} // namespace OriginalActorBlendPhaseDetail

// The ordinary MAIN outcome is the actual continuation authority: native
// Remaining and counter are NOT reset at103A1FA1. This is the original blend
// block103A1FA1/103A21BC..103A23CE, NOT the old corrected per-slot clock. It
// rechecks MAIN gates, admits at most four total MAIN+blend iterations, scans
// slots0..3 and shares residual elapsed across slots AND later passes. There
// are no blend script callbacks. Services borrow the caller's root/journal.
inline OriginalActorBlendPhaseStats AdvanceOriginalActorBlendAnimation(std::uintptr_t actor,
    const OriginalActorAnimationPhaseStats& main,const OriginalActorBlendPhaseQueries& q) {
    using namespace OriginalActorBlendPhaseDetail;
    using F=OriginalActorBlendFloat;
    using M=OriginalActorAnimationFloat;
    if(!actor)Fail("nonnull Actor is required");
    if(main.iterations>4u||(main.iterationCap&&main.iterations!=4u))
        Fail("MAIN continuation has an invalid original iteration count");
    OriginalActorBlendPhaseStats stats;
    stats.initialCounter=main.iterationCap?5u:main.iterations;
    stats.nativeCounter=stats.initialCounter;stats.remainingSeconds=main.remainingSeconds;
    const auto name=[&](std::string value) {
        if(value.size()>q.maxNameBytes)Fail("name byte budget exceeded");
        return value;
    };
    const auto readMain=[&](M field) {
        ++stats.floatReads;return Finite(Call(q,stats,q.readMainFloat,actor,field));
    };
    const auto read=[&](unsigned slot,F field) {
        ++stats.floatReads;return Finite(Call(q,stats,q.readFloat,actor,slot,field));
    };
    const auto write=[&](unsigned slot,F field,float value) {
        ++stats.floatWrites;Call(q,stats,q.writeFloat,actor,slot,field,value);
    };
    for(;;) {
        ++stats.sequenceReads;
        if(OriginalActorAnimationPhaseDetail::None(name(Call(q,stats,q.mainSequence,actor))))break;
        if(readMain(M::Frame)>=0.0f) {
            if(readMain(M::Rate)==0.0f)break;
        }else if(readMain(M::TweenRate)==0.0f)break;
        if(Finite(stats.remainingSeconds)<=0.0f)break;
        // Retail increments BEFORE comparing >4, including the failed cap
        // increment inherited from MAIN. No preemptive counter clamp.
        if(++stats.nativeCounter>4u) {stats.iterationCap=true;break;}
        ++stats.iterations;
        for(unsigned slot=0u;slot<4u;++slot) {
            ++stats.slotVisits;++stats.sequenceReads;
            if(OriginalActorAnimationPhaseDetail::None(name(Call(q,stats,q.sequence,actor,slot))))continue;
            const auto comparedFrame=read(slot,F::Frame),comparedLast=read(slot,F::Last);
            if(comparedFrame>=comparedLast)continue;
            const auto oldFrame=read(slot,F::Frame);
            if(read(slot,F::Frame)<0.0f) {
                const auto tween=read(slot,F::TweenRate),frame=read(slot,F::Frame);
                const auto tentative=Add64(Multiply64(Decode(stats.remainingSeconds),Decode(tween)),Decode(frame));
                const bool negative=Compare64(tentative,Decode(0.0f))<0;
                // FST keeps the PC64 tentative value for the crossing test
                // and fraction. It is NOT reloaded from the rounded Frame.
                write(slot,F::Frame,Store32(tentative));
                if(negative)continue;
                const auto numerator=Multiply64(tentative,Decode(stats.remainingSeconds));
                const auto denominator=Add64(tentative,Negate(Decode(oldFrame)));
                write(slot,F::Frame,0.0f);
                stats.remainingSeconds=Store32(ModelFastLineDetail::Divide64(numerator,denominator));
                // Normal playback is not advanced in this slot's tween pass.
                continue;
            }
            Dyadic advance;
            if(read(slot,F::Rate)>=0.0f) {
                advance=Multiply64(Decode(stats.remainingSeconds),Decode(read(slot,F::Rate)));
            }else {
                const auto velocity=[&](unsigned axis) {
                    ++stats.floatReads;return Decode(Finite(Call(q,stats,q.velocity,actor,axis)));
                };
                const auto z=velocity(2u),y=velocity(1u),x=velocity(0u);
                const auto squares=Add64(Add64(Multiply64(x,x),Multiply64(y,y)),Multiply64(z,z));
                const auto argument=ScriptRandom::VectorDetail::StoreDouble(squares);
                const auto root=squares.significand?ScriptRandom::VectorDetail::SqrtStoredDouble64(argument):Dyadic{};
                // Unlike MAIN, FSTP Double at103A2299 rounds the sqrt return
                // before it is multiplied. The adjusted negative rate is
                // itself FSTP Float at22B2 BEFORE the max(MinRate,rate) test.
                const auto speed=ScriptRandom::VectorDetail::DecodeDouble(ScriptRandom::VectorDetail::StoreDouble(root));
                const auto minimum=Decode(read(slot,F::MinRate));
                const auto adjusted=Decode(Store32(Negate(Multiply64(speed,Decode(read(slot,F::Rate))))));
                advance=Multiply64(Compare64(minimum,adjusted)<0?adjusted:minimum,Decode(stats.remainingSeconds));
            }
            const auto tentative=Add64(advance,Decode(read(slot,F::Frame)));
            write(slot,F::Frame,Store32(tentative));
            const auto storedFrame=read(slot,F::Frame),last=read(slot,F::Last);
            if(storedFrame<last)continue;
            const auto numeratorFrame=read(slot,F::Frame),numeratorLast=read(slot,F::Last);
            const auto numerator=Multiply64(Difference(numeratorFrame,numeratorLast),Decode(stats.remainingSeconds));
            const auto copiedLast=read(slot,F::Last),denominatorFrame=read(slot,F::Frame);
            const auto denominator=Difference(denominatorFrame,oldFrame);
            write(slot,F::Frame,copiedLast);write(slot,F::Rate,0.0f);
            ++stats.roleReads;const auto role=Call(q,stats,q.remoteRole,actor);
            // Prefix stores and the native Role load precede this division.
            stats.remainingSeconds=Store32(ModelFastLineDetail::Divide64(numerator,denominator));
            if(role>=2u)continue;
            ++stats.weaponChecks;if(Call(q,stats,q.isWeapon,actor))continue;
            write(slot,F::SimX,Store32(Multiply64(Decode(read(slot,F::Frame)),Decode(10000.0f))));
            const auto simulated=Multiply64(Decode(read(slot,F::Rate)),Decode(5000.0f));
            const bool cap=Compare64(simulated,Decode(32767.0f))>0;
            write(slot,F::SimY,Store32(simulated));
            if(cap)write(slot,F::SimY,32767.0f);
        }
    }
    stats.inactive=stats.iterations==0u;return stats;
}

} // namespace QuestVr
