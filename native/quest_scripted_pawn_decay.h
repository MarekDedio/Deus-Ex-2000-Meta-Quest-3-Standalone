#pragma once

#include "quest_native_scalar.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

enum class OriginalScriptedPawnDecayField {
    AgitationCheckTimer,AgitationTimer,AgitationDecayRate,
    FearTimer,FearDecayRate,FearLevel
};
struct OriginalScriptedPawnDecayQueries {
    std::function<float(std::uintptr_t,OriginalScriptedPawnDecayField)> readFloat;
    std::function<void(std::uintptr_t,OriginalScriptedPawnDecayField,float)> writeFloat;
    std::function<bool(std::uintptr_t)> alliancesChanged;
    // The host updates the actual declared Bool while preserving all other
    // packed fields. This is not a complete native DWORD replacement.
    std::function<void(std::uintptr_t,bool)> writeAlliancesChanged;
    // Original AllianceInfoEx.AllianceName FName != None, NOT a UObject
    // pointer: the native DWORD at AgitationLevel-8 is an FName index.
    std::function<bool(std::uintptr_t,std::size_t)> allianceNamed,alliancePermanent;
    std::function<float(std::uintptr_t,std::size_t)> allianceAgitation;
    std::function<void(std::uintptr_t,std::size_t,float)> writeAllianceAgitation;
    std::function<void()> work;
};
struct OriginalScriptedPawnDecayStats {
    std::size_t steps{},floatReads{},floatWrites{},flagReads{},flagWrites{};
    std::size_t entriesVisited{},nameReads{},permanentReads{},agitationReads{},agitationWrites{};
    bool delayOnlyReturn{};
};
namespace ScriptedPawnDecayDetail {
template<class Provider,class... Args>
inline auto Call(const OriginalScriptedPawnDecayQueries& q,OriginalScriptedPawnDecayStats& result,
    const Provider& provider,Args&&... args) {
    if(!provider)throw std::invalid_argument("Original ScriptedPawn decay lacks a reached provider");
    if(!q.work)throw std::invalid_argument("Original ScriptedPawn decay lacks caller work authority");
    q.work();
    ++result.steps;
    return provider(std::forward<Args>(args)...);
}
inline void RequireActor(std::uintptr_t actor) {
    if(!actor)throw std::invalid_argument("Original ScriptedPawn decay requires a nonnull Actor");
}
} // namespace ScriptedPawnDecayDetail

// Actual native UpdateAgitation10019E30..10019F55. This is the native helper
// called by ScriptedPawn.Tick, not the similarly named compiled script body
// (whose Float locals have different spill boundaries). All writes and errors
// belong to the caller's SAME root; no local transaction/rollback is created.
inline OriginalScriptedPawnDecayStats UpdateOriginalScriptedPawnAgitation(std::uintptr_t actor,
    float delta,const OriginalScriptedPawnDecayQueries& q) {
    using namespace NativeScalar;
    using Field=OriginalScriptedPawnDecayField;
    using ScriptedPawnDecayDetail::Call;
    ScriptedPawnDecayDetail::RequireActor(actor);
    OriginalScriptedPawnDecayStats result;
    const auto read=[&](Field field){++result.floatReads;return Call(q,result,q.readFloat,actor,field);};
    const auto write=[&](Field field,float value){++result.floatWrites;Call(q,result,q.writeFloat,actor,field,value);};
    if(Compare(177u,Load(read(Field::AgitationCheckTimer)),Memory(0.0f))) {
        const auto remaining=Subtract(Load(read(Field::AgitationCheckTimer)),Memory(delta));
        write(Field::AgitationCheckTimer,StoreFloat(remaining)); // FST, not pop/reload.
        if(Compare(176u,remaining,Memory(0.0f)))write(Field::AgitationCheckTimer,0.0f);
    }
    auto decay=Load(0.0f);
    if(Compare(177u,Load(read(Field::AgitationTimer)),Memory(0.0f))) {
        if(Compare(176u,Load(read(Field::AgitationTimer)),Memory(delta))) {
            const auto ratio=Divide(Load(read(Field::AgitationTimer)),Memory(delta));
            write(Field::AgitationTimer,0.0f);
            const auto fraction=Subtract(Memory(1.0),ratio); // FSUBR Double1.
            const auto product=Multiply(Load(delta),Memory(read(Field::AgitationDecayRate)));
            decay=Multiply(product,fraction); // Original FMULP, no Float temporary.
        } else {
            const auto remaining=Subtract(Load(read(Field::AgitationTimer)),Memory(delta));
            write(Field::AgitationTimer,StoreFloat(remaining));
        }
    } else decay=Multiply(Load(delta),Memory(read(Field::AgitationDecayRate)));
    ++result.flagReads;
    if(!Call(q,result,q.alliancesChanged,actor)||!Compare(177u,decay,Memory(0.0f)))return result;
    ++result.flagWrites;Call(q,result,q.writeAlliancesChanged,actor,false);
    for(std::size_t index=16u;index--;) {
        ++result.entriesVisited;
        ++result.nameReads;if(!Call(q,result,q.allianceNamed,actor,index))continue;
        ++result.permanentReads;if(Call(q,result,q.alliancePermanent,actor,index))continue;
        ++result.agitationReads;
        if(!Compare(177u,Load(Call(q,result,q.allianceAgitation,actor,index)),Memory(0.0f)))continue;
        ++result.flagWrites;Call(q,result,q.writeAlliancesChanged,actor,true);
        ++result.agitationReads;
        const auto remaining=Subtract(Load(Call(q,result,q.allianceAgitation,actor,index)),decay);
        ++result.agitationWrites;Call(q,result,q.writeAllianceAgitation,actor,index,StoreFloat(remaining));
        if(Compare(176u,remaining,Memory(0.0f))) {
            ++result.agitationWrites;Call(q,result,q.writeAllianceAgitation,actor,index,0.0f);
        }
    }
    return result;
}

// Actual native UpdateFear10019F60..1001A005. Sustained-delay subtraction
// returns immediately. The short-delay ratio and final level subtraction stay
// at PC64 across the original Float stores and comparisons.
inline OriginalScriptedPawnDecayStats UpdateOriginalScriptedPawnFear(std::uintptr_t actor,
    float delta,const OriginalScriptedPawnDecayQueries& q) {
    using namespace NativeScalar;
    using Field=OriginalScriptedPawnDecayField;
    using ScriptedPawnDecayDetail::Call;
    ScriptedPawnDecayDetail::RequireActor(actor);
    OriginalScriptedPawnDecayStats result;
    const auto read=[&](Field field){++result.floatReads;return Call(q,result,q.readFloat,actor,field);};
    const auto write=[&](Field field,float value){++result.floatWrites;Call(q,result,q.writeFloat,actor,field,value);};
    Real decay;
    if(Compare(177u,Load(read(Field::FearTimer)),Memory(0.0f))) {
        const bool shorter=Compare(176u,Load(read(Field::FearTimer)),Memory(delta));
        // Native FLD occurs before FNSTSW/branch: keep this third fresh load
        // before either arithmetic path, not behind a lazy branch provider.
        const auto timer=Load(read(Field::FearTimer));
        if(shorter) {
            const auto ratio=Divide(timer,Memory(delta));
            write(Field::FearTimer,0.0f);
            const auto fraction=Subtract(Memory(1.0),ratio);
            const auto product=Multiply(Load(delta),Memory(read(Field::FearDecayRate)));
            decay=Multiply(product,fraction);
        } else {
            write(Field::FearTimer,StoreFloat(Subtract(timer,Memory(delta))));
            result.delayOnlyReturn=true;return result;
        }
    } else decay=Multiply(Load(delta),Memory(read(Field::FearDecayRate)));
    if(!Compare(177u,decay,Memory(0.0f)))return result;
    if(!Compare(177u,Load(read(Field::FearLevel)),Memory(0.0f)))return result;
    const auto remaining=Subtract(Memory(read(Field::FearLevel)),decay); // FSUBR m32.
    write(Field::FearLevel,StoreFloat(remaining));
    if(Compare(176u,remaining,Memory(0.0f)))write(Field::FearLevel,0.0f);
    return result;
}
} // namespace QuestVr
