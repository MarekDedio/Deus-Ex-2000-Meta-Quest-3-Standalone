#pragma once

#include "quest_native_scalar.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

// Literal AActor.Tick timer block, Engine103A269B..103A2709. The _ftol
// conversion is a separate native service: its actual EAX result word, not an
// invented host floor or a timer-period validation rule, feeds signed FILD32.
struct OriginalActorTimerPhaseQueries {
    std::function<float(std::uintptr_t)> rate,counter;
    std::function<bool(std::uintptr_t)> loop;
    std::function<void(std::uintptr_t,float)> writeRate,writeCounter;
    std::function<std::int32_t(NativeScalar::Real)> convertPeriods;
    std::function<void(std::uintptr_t)> timer;
    std::function<void()> work;
};
struct OriginalActorTimerPhaseStats {
    std::size_t steps{},rateReads{},counterReads{},loopReads{},counterWrites{},
        rateWrites{},conversionCalls{},timerCalls{};
    bool active{},expired{};
    std::int32_t periods{};
};
namespace ActorTimerPhaseDetail {
template<class Provider,class... Args>
inline auto Call(const OriginalActorTimerPhaseQueries& q,OriginalActorTimerPhaseStats& out,
    const Provider& provider,Args&&... args) {
    if(!provider)throw std::invalid_argument("Original Actor timer phase lacks a reached provider");
    if(!q.work)throw std::invalid_argument("Original Actor timer phase lacks caller work authority");
    q.work();++out.steps;
    return provider(std::forward<Args>(args)...);
}
} // namespace ActorTimerPhaseDetail

// Same caller root/journal as Tick, state and later lifespan/physics. Native
// stores are not conditional on changed bits; no local undo or ProcessEvent
// gate belongs here. The timer provider owns FindFunctionChecked followed by
// the actual selected ProcessEvent gates AFTER all native timer stores.
inline OriginalActorTimerPhaseStats AdvanceOriginalActorTimerPhase(std::uintptr_t actor,
    float delta,const OriginalActorTimerPhaseQueries& q) {
    if(!actor)throw std::invalid_argument("Original Actor timer phase requires a nonnull Actor");
    using namespace NativeScalar;
    using ActorTimerPhaseDetail::Call;
    OriginalActorTimerPhaseStats out;
    const auto rate=[&]{++out.rateReads;return Call(q,out,q.rate,actor);};
    const auto counter=[&]{++out.counterReads;return Call(q,out,q.counter,actor);};
    const auto writeCounter=[&](float value){++out.counterWrites;Call(q,out,q.writeCounter,actor,value);};

    // FCOMP/Test(C0|C3): zero, negative and unordered rates are lazy exits.
    if(!Compare(177u,Load(rate()),Memory(0.0)))return out;
    out.active=true;
    const auto sum=Add(Load(delta),Memory(counter()));
    // FCOM happens BEFORE FST Counter; this comparison remains the retained
    // x87 result even if the Float spill overflows or rounds across Rate.
    const bool belowOrUnordered=Compare(176u,sum,Memory(rate()));
    writeCounter(StoreFloat(sum));
    if(belowOrUnordered)return out;
    out.expired=true;
    const auto quotient=Divide(sum,Memory(rate()));
    ++out.conversionCalls;out.periods=Call(q,out,q.convertPeriods,quotient);
    // Every signed32 integer is exactly representable by binary64. This is
    // an exact FILD32 input, never a lossy binary32 cast of the EAX word.
    const auto count=Load(static_cast<double>(out.periods));
    const auto remainder=Subtract(sum,Multiply(count,Memory(rate())));
    writeCounter(StoreFloat(remainder));
    if(out.periods!=0) {
        ++out.loopReads;
        if(!Call(q,out,q.loop,actor)) {
            ++out.rateWrites;Call(q,out,q.writeRate,actor,0.0f);
        }
    }
    ++out.timerCalls;Call(q,out,q.timer,actor);
    return out;
}

} // namespace QuestVr
