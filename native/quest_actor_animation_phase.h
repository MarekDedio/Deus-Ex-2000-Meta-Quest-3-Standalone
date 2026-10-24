#pragma once

#include "quest_script_random_vector.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace QuestVr {

enum class OriginalActorAnimationFloat {
    Frame, Rate, TweenRate, Last, MinRate, VelocityX, VelocityY, VelocityZ, SimX, SimY
};
enum class OriginalActorAnimationBool { Notify, Loop };

struct OriginalActorAnimationPhaseQueries {
    // All fields are LIVE original Actor properties, not a captured clock.
    // Each store must use the caller's journal/Execution write accounting and
    // update only that field. Never reinstall an old clock after ProcessEvent.
    std::function<float(std::uintptr_t,OriginalActorAnimationFloat)> readFloat;
    std::function<void(std::uintptr_t,OriginalActorAnimationFloat,float)> writeFloat;
    std::function<bool(std::uintptr_t,OriginalActorAnimationBool)> readBool;
    std::function<void(std::uintptr_t,bool)> writeFinished;
    // Actual FName identity; empty or case-insensitive None denotes index0.
    std::function<std::string(std::uintptr_t)> sequence;
    // Actual Actor.Mesh and its selected native sequence. Opaque identities
    // must survive callbacks. No mesh fallback, guessed PlayerViewMesh or
    // preselection handler/probe filtering belongs in these providers.
    std::function<std::uintptr_t(std::uintptr_t)> mesh;
    std::function<std::uintptr_t(std::uintptr_t,const std::string&)> meshSequence;
    std::function<std::size_t(std::uintptr_t)> notifyCount;
    std::function<float(std::uintptr_t,std::size_t)> notifyTime;
    std::function<std::string(std::uintptr_t,std::size_t)> notifyFunction;
    // FindFunction(name,0) is nullable. FindFunctionChecked(AnimEnd,0) can also
    // return null when authentic GIsScriptable is false; that exact result is
    // still passed to ProcessEvent. Its provider must reproduce checked lookup
    // failure when scripting is enabled, not fabricate a null missing handler.
    // ProcessEvent receives that retained declaration (possibly null for the
    // disabled checked path) and a null data/result packet, with real Actor
    // begun-play/Core scriptable/probe/delete gates in the SAME Execution.
    std::function<std::uintptr_t(std::uintptr_t,const std::string&)> findFunction;
    std::function<std::uintptr_t(std::uintptr_t)> findAnimEnd;
    std::function<void(std::uintptr_t,std::uintptr_t)> processEvent;
    std::function<std::uint8_t(std::uintptr_t)> remoteRole;
    std::function<bool(std::uintptr_t)> isWeapon;
    // Native FStateFrame.LatentAction, not a portable latent enum ordinal.
    // Required only by the reached loop-ending branch. Missing frame or an
    // unrepresented native latent action must refuse rather than guess.
    std::function<std::uint32_t(std::uintptr_t)> nativeLatentAction;
    std::function<void()> work;
    // Captured by the preceding Tick context: bIsPawn AND actual APawn AND
    // Role==2. It suppresses AnimEnd, not mesh notifies; do not refresh it in
    // a callback. This helper does not implement that preceding Tick context.
    bool suppressAnimEnd{};
    std::size_t maxNotifySearches{65'536u},maxNameBytes{65'536u};
};

struct OriginalActorAnimationPhaseStats {
    std::size_t steps{},iterations{},sequenceReads{},floatReads{},boolReads{},
        floatWrites{},finishedWrites{},meshReads{},meshSequenceCalls{},
        notifySearches{},notifyFunctionReads{},functionFinds{},animEndFinds{},
        notifyEvents{},animEndEvents{},missingHandlers{},latentReads{},roleReads{},weaponChecks{};
    bool inactive{},iterationCap{};
    // Raw native local `Remaining` at exit, NOT a resumable debt. An ordinary
    // sub-end advance returns with this still equal to the input even though
    // the frame was advanced. Only the retail boundary branches rewrite it.
    float remainingSeconds{};
};

namespace OriginalActorAnimationPhaseDetail {
using ActorTimerDetail::Dyadic;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Store32;
using NativeWindowClipDetail::Add64;
using NativeWindowClipDetail::Negate;
using NativeWindowClipDetail::Compare64;
using ModelFastLineDetail::Multiply64;
using ModelFastLineDetail::Divide64;

[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original Actor main animation: ")+reason);
}
inline float Finite(float value) {
    if(!std::isfinite(value))Fail("active nonfinite Float refused");
    return value;
}
inline bool None(const std::string& name) {
    if(name.empty())return true;
    if(name.size()!=4u)return false;
    return (name[0]=='N'||name[0]=='n') && (name[1]=='O'||name[1]=='o') &&
        (name[2]=='N'||name[2]=='n') && (name[3]=='E'||name[3]=='e');
}
inline void Name(const std::string& name,const OriginalActorAnimationPhaseQueries& queries) {
    if(name.size()>queries.maxNameBytes)Fail("name byte budget exceeded");
}
template<class Provider,class... Arguments>
inline auto Call(const OriginalActorAnimationPhaseQueries& queries,
    OriginalActorAnimationPhaseStats& stats,const Provider& provider,Arguments&&... arguments) {
    if(!provider)Fail("active native provider is missing");
    if(queries.work)queries.work();
    ++stats.steps;
    return provider(std::forward<Arguments>(arguments)...);
}
inline Dyadic Difference(float a,float b) {return Add64(Decode(a),Negate(Decode(b)));}
} // namespace OriginalActorAnimationPhaseDetail

// ONLY the retail ordinary MAIN animation phase, Engine.dll103A1BF0..103A1F9F
// (AActor::Tick body103A1AA0). Not blends, state/latent polling, timers, physics,
// world eligibility, actor Tick as a whole, or the older portable corrected
// animation-clock policy. No commercial binary is loaded or executed here.
//
// Audit source: owned GOTY Engine.dll SHA256
// 0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4.
// Reproduce: objdump -d -Mintel --start-address=0x103A1BF0
// --stop-address=0x103A1FA1 <owned System/Engine.dll>. Arithmetic is x87 PC64,
// nearest-even, with EACH original Float spill retained, including the notify
// search's Float best-distance and residual fractions. Negative finite elapsed
// exits at the original remaining<=0 gate; invalid active nonfinite/storage or
// zero-divisor paths explicitly refuse in the caller's existing root.
//
// Further evidence: execFinishAnim thunk103037BA ->103DFF60..103DFFCE stores
// native LatentAction385 at103DFFC4. PollFinishAnim thunk1030240A ->103E00A0
// clears that action when bAnimFinished. Thus103A1E20's [Actor+0C]+24 test is
// that latent action, NOT a class name or a fabricated new native index.
inline OriginalActorAnimationPhaseStats AdvanceOriginalActorMainAnimation(
    std::uintptr_t actor,float wholeDelta,const OriginalActorAnimationPhaseQueries& queries) {
    using namespace OriginalActorAnimationPhaseDetail;
    using F=OriginalActorAnimationFloat;
    using B=OriginalActorAnimationBool;
    OriginalActorAnimationPhaseStats stats;
    stats.remainingSeconds=wholeDelta;
    auto read=[&](F field) {
        ++stats.floatReads;
        return Finite(Call(queries,stats,queries.readFloat,actor,field));
    };
    auto write=[&](F field,float value) {
        ++stats.floatWrites;
        Call(queries,stats,queries.writeFloat,actor,field,value);
    };
    auto flag=[&](B field) {
        ++stats.boolReads;return Call(queries,stats,queries.readBool,actor,field);
    };
    auto sequence=[&] {
        ++stats.sequenceReads;
        auto value=Call(queries,stats,queries.sequence,actor);
        Name(value,queries);return value;
    };
    auto finished=[&] {
        ++stats.finishedWrites;Call(queries,stats,queries.writeFinished,actor,true);
    };
    auto animEnd=[&] {
        if(queries.suppressAnimEnd)return;
        ++stats.animEndFinds;
        const auto function=Call(queries,stats,queries.findAnimEnd,actor);
        ++stats.animEndEvents;
        Call(queries,stats,queries.processEvent,actor,function);
    };
    for(;;) {
        //103A1BF9..103A1C60: gates are lazy and precede the literal4 cap.
        if(None(sequence())) {stats.inactive=stats.iterations==0u;return stats;}
        if(read(F::Frame)>=0.0f) {
            if(read(F::Rate)==0.0f) {stats.inactive=stats.iterations==0u;return stats;}
        } else if(read(F::TweenRate)==0.0f) {
            stats.inactive=stats.iterations==0u;return stats;
        }
        if(Finite(stats.remainingSeconds)<=0.0f) {
            stats.inactive=stats.iterations==0u;return stats;
        }
        if(stats.iterations==4u) {stats.iterationCap=true;return stats;}
        ++stats.iterations;
        const bool tween=read(F::Frame)<0.0f; //103A1C66 comparison
        const float oldFrame=read(F::Frame); //103A1C6F raw captured DWORD
        if(tween) {
            //103A1F26: compare the live PC64 sum, NOT its stored Float.
            const auto tweenRate=read(F::TweenRate);
            const auto frame=read(F::Frame);
            const auto tentative=Add64(Multiply64(Decode(stats.remainingSeconds),Decode(tweenRate)),Decode(frame));
            const auto negative=Compare64(tentative,Decode(0.0f))<0;
            write(F::Frame,Store32(tentative));
            if(negative)return stats;
            const auto numerator=Multiply64(tentative,Decode(stats.remainingSeconds));
            const auto denominator=Add64(tentative,Negate(Decode(oldFrame)));
            write(F::Frame,0.0f);
            stats.remainingSeconds=Store32(ModelFastLineDetail::Divide64(numerator,denominator));
            if(read(F::Rate)==0.0f) {finished();animEnd();}
            continue;
        }

        Dyadic advance;
        if(read(F::Rate)>=0.0f) {
            advance=Multiply64(Decode(stats.remainingSeconds),Decode(read(F::Rate)));
        } else {
            //103A1C98: source loads z,y,x, then sums x*x+y*y+z*z.
            const auto z=Decode(read(F::VelocityZ));
            const auto y=Decode(read(F::VelocityY));
            const auto x=Decode(read(F::VelocityX));
            const auto sum=Add64(Add64(Multiply64(x,x),Multiply64(y,y)),Multiply64(z,z));
            // Core.appSqrt receives the original binary64 argument spill;
            // its `fld Double; fsqrt; ret` result remains PC64 despite the
            // unused diagnostic Double store at103A1CCC. Not host sqrtf.
            const auto stored=ScriptRandom::VectorDetail::StoreDouble(sum);
            const auto speed=sum.significand ? ScriptRandom::VectorDetail::SqrtStoredDouble64(stored) : Dyadic{};
            const auto scaled=Negate(Multiply64(speed,Decode(read(F::Rate))));
            const auto minimum=Decode(read(F::MinRate));
            const auto rate=Compare64(minimum,scaled)<0 ? scaled : minimum;
            advance=Multiply64(rate,Decode(stats.remainingSeconds));
        }
        const auto tentative=Add64(advance,Decode(read(F::Frame)));
        const bool notify=flag(B::Notify); //103A1CF7, BEFORE Frame FSTP
        write(F::Frame,Store32(tentative));
        if(notify) {
            ++stats.meshReads;
            const auto mesh=Call(queries,stats,queries.mesh,actor);
            if(mesh) {
                const auto name=sequence();
                ++stats.meshSequenceCalls;
                const auto selected=Call(queries,stats,queries.meshSequence,mesh,name);
                if(selected) {
                    const auto count=Call(queries,stats,queries.notifyCount,selected);
                    if(count>queries.maxNotifySearches-stats.notifySearches)
                        Fail("notify search budget exceeded");
                    bool found{};
                    std::size_t chosen{};
                    float chosenTime{},bestDistance=100'000.0f;
                    for(std::size_t index=0u;index<count;++index) {
                        ++stats.notifySearches;
                        const auto time=Finite(Call(queries,stats,queries.notifyTime,selected,index));
                        if(!(oldFrame<time))continue;
                        if(read(F::Frame)<time)continue;
                        const auto difference=Difference(time,oldFrame);
                        //103A1D59 tests the selected pointer before comparing:
                        // the first reached candidate is unconditional, even
                        // if its delta exceeds the100000 initializer. A later
                        // *identical* time can win if Float(best) rounded up.
                        if(!found || Compare64(difference,Decode(bestDistance))<0) {
                            bestDistance=Store32(difference);chosen=index;chosenTime=time;found=true;
                        }
                    }
                    if(found) {
                        const auto distance=Difference(read(F::Frame),chosenTime);
                        const auto numerator=Multiply64(distance,Decode(stats.remainingSeconds));
                        const auto denominator=Difference(read(F::Frame),oldFrame);
                        //103A1D8A: commit selected boundary BEFORE looking up
                        // even the notification's function NAME or handler.
                        write(F::Frame,chosenTime);
                        ++stats.notifyFunctionReads;
                        auto name=Call(queries,stats,queries.notifyFunction,selected,chosen);
                        Name(name,queries);
                        stats.remainingSeconds=Store32(ModelFastLineDetail::Divide64(numerator,denominator));
                        ++stats.functionFinds;
                        const auto function=Call(queries,stats,queries.findFunction,actor,name);
                        if(function) {
                            ++stats.notifyEvents;
                            Call(queries,stats,queries.processEvent,actor,function);
                        } else ++stats.missingHandlers;
                        continue; // No pre-handler eligibility or new delete gate.
                    }
                }
            }
        }

        //103A1DB7: only after the full tentative notify search, compare Last.
        const auto frame=read(F::Frame);
        if(frame<read(F::Last))return stats;
        if(flag(B::Loop)) {
            if(read(F::Frame)<1.0f)stats.remainingSeconds=0.0f;
            else {
                const auto distance=Difference(read(F::Frame),1.0f);
                const auto numerator=Multiply64(distance,Decode(stats.remainingSeconds));
                const auto denominator=Difference(read(F::Frame),oldFrame);
                write(F::Frame,0.0f);
                stats.remainingSeconds=Store32(ModelFastLineDetail::Divide64(numerator,denominator));
            }
            if(oldFrame<read(F::Last)) {
                ++stats.latentReads;
                if(Call(queries,stats,queries.nativeLatentAction,actor)==385u)finished();
                animEnd();
            }
            continue;
        }

        //103A1E60: no corrected-past-end policy. A preexisting past-end
        // frame still takes these stores and dispatches selected AnimEnd.
        const auto terminalFrame=read(F::Frame);
        const auto terminalLast=read(F::Last);
        const auto distance=Difference(terminalFrame,terminalLast);
        const auto last=read(F::Last);
        write(F::Rate,0.0f);
        const auto numerator=Multiply64(distance,Decode(stats.remainingSeconds));
        const auto denominator=Difference(read(F::Frame),oldFrame);
        write(F::Frame,last);
        finished();
        stats.remainingSeconds=Store32(ModelFastLineDetail::Divide64(numerator,denominator));
        animEnd();
        // Callback may replace any of these values; use fresh live reads.
        ++stats.roleReads;
        if(Call(queries,stats,queries.remoteRole,actor)<2u) {
            ++stats.weaponChecks;
            if(!Call(queries,stats,queries.isWeapon,actor)) {
                write(F::SimX,Store32(Multiply64(Decode(read(F::Frame)),Decode(10'000.0f))));
                const auto simulatedRate=Multiply64(Decode(read(F::Rate)),Decode(5'000.0f));
                write(F::SimY,Store32(simulatedRate));
                //103A1F00 FST retains PC64 for the following upper clamp.
                if(Compare64(simulatedRate,Decode(32'767.0f))>0)write(F::SimY,32'767.0f);
            }
        }
    }
}
} // namespace QuestVr
