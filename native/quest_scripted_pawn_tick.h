#pragma once

#include "quest_native_scalar.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

// These are authenticated property identities, not offsets into a host object.
// Raw float reads/copies must preserve their 32-bit representation, including
// signaling NaNs. Numeric FLD and memory-operand conversions happen below.
enum class OriginalScriptedPawnTickFloat {
    PrePivotTime, LastRenderTime, DistanceFromPlayer, AlarmTimer, WeaponTimer,
    FireTimer, SpecialTimer, ReloadTimer, AvoidWallTimer, AvoidBumpTimer,
    ObstacleTimer, CloakEMPTimer, TakeHitTimer, CarcassCheckTimer,
    PotentialEnemyTimer, BeamCheckTimer, FutzTimer, PlayerAgitationTimer,
    DistressTimer, FearSustainTime, MoveTimer, BurnTimer, BurnPeriod,
    BleedRate, DropCounter, ClotPeriod
};
enum class OriginalScriptedPawnTickFlag {
    SimulatedPawn, Disappear, TickVisibleOnly, HasCloak, AdvancedTactics,
    ClearedObstacle, OnFire, CanBleed
};
enum class OriginalScriptedPawnTickByte { Role, RemoteRole, Physics, TurnDirection, NextDirection };
enum class OriginalScriptedPawnTickVector { PrePivot, DesiredPrePivot, Acceleration, Velocity };
enum class OriginalScriptedPawnTickInt { Health, CloakThreshold };
enum class OriginalScriptedPawnTickObject { Weapon, ActorAvoiding };

struct OriginalScriptedPawnTickQueries {
    std::function<float(std::uintptr_t,OriginalScriptedPawnTickFloat)> readFloat;
    std::function<void(std::uintptr_t,OriginalScriptedPawnTickFloat,float)> writeFloat;
    std::function<bool(std::uintptr_t,OriginalScriptedPawnTickFlag)> readFlag;
    std::function<void(std::uintptr_t,OriginalScriptedPawnTickFlag,bool)> writeFlag;
    std::function<std::uint8_t(std::uintptr_t,OriginalScriptedPawnTickByte)> readByte;
    std::function<void(std::uintptr_t,OriginalScriptedPawnTickByte,std::uint8_t)> writeByte;
    std::function<float(std::uintptr_t,OriginalScriptedPawnTickVector,unsigned)> readVectorComponent;
    std::function<void(std::uintptr_t,OriginalScriptedPawnTickVector,unsigned,float)> writeVectorComponent;
    std::function<std::int32_t(std::uintptr_t,OriginalScriptedPawnTickInt)> readInt;
    std::function<std::uintptr_t(std::uintptr_t,OriginalScriptedPawnTickObject)> readObject;
    std::function<void(std::uintptr_t,OriginalScriptedPawnTickObject)> clearObject;
    std::function<void(std::uintptr_t)> clearPotentialEnemyAlliance;
    std::function<double(std::uintptr_t)> nativeTime;
    // Literal compatibility walks at1001960E..1001964E are read-only but are
    // not optional. +5D4 is reached ONLY after an actual APlayerPawn cast.
    std::function<std::uintptr_t(std::uintptr_t)> actorClass,superClass;
    std::function<std::uintptr_t()> nativePlayerPawnClass,nativeDeusExPlayerClass;
    std::function<std::uint32_t(std::uintptr_t)> readPlayerPawnField5D4;
    std::function<bool(std::uintptr_t)> inStasis;
    std::function<void(std::uintptr_t,bool)> destroyActor;
    std::function<void(std::uintptr_t,float)> updateAgitation,updateFear;
    std::function<void(std::uintptr_t,bool)> enableCloak;
    std::function<void(std::uintptr_t)> extinguishFire,spurtBlood;
    std::function<NativeScalar::Real(double)> appSqrt;
    std::function<int(std::uintptr_t,float,std::uint32_t)> baseTick;
    // Shared caller-root authority is required, including for the native blood
    // loop. No fixed iteration cap or successful no-op provider is substituted.
    std::function<void()> work;
};
struct OriginalScriptedPawnTickResult {
    int returned{};
    bool customPrefix{},earlyDestroyed{};
    std::size_t steps{},floatReads{},floatWrites{},flagReads{},flagWrites{},
        byteReads{},byteWrites{},vectorReads{},vectorWrites{},classReads{},
        timerStores{},bloodEvents{},cloakEvents{},fireEvents{},baseCalls{};
};
namespace ScriptedPawnTickDetail {
template<class Provider,class... Args>
inline auto Call(const OriginalScriptedPawnTickQueries& q,OriginalScriptedPawnTickResult& result,
    const Provider& provider,Args&&... args) {
    if(!provider)throw std::invalid_argument("Original ScriptedPawn.Tick lacks a reached provider");
    if(!q.work)throw std::invalid_argument("Original ScriptedPawn.Tick lacks shared work authority");
    q.work();++result.steps;
    return provider(std::forward<Args>(args)...);
}
} // namespace ScriptedPawnTickDetail

// Owned DeusEx.dll100195A0..10019DF5, selected by the receiver's genuine
// ScriptedPawn native Tick slot. This is its entire custom prefix, followed by
// the caller's genuine AActor.Tick continuation. No APawn suffix or physics is
// implemented here. All callbacks/field writes belong to the SAME caller root;
// exceptions propagate without local undo or a fabricated successful return.
inline OriginalScriptedPawnTickResult TickOriginalScriptedPawn(std::uintptr_t actor,float delta,
    std::uint32_t tickType,const OriginalScriptedPawnTickQueries& q) {
    if(!actor)throw std::invalid_argument("Original ScriptedPawn.Tick requires a nonnull receiver");
    using namespace NativeScalar;
    using F=OriginalScriptedPawnTickFloat;using B=OriginalScriptedPawnTickFlag;
    using Y=OriginalScriptedPawnTickByte;using V=OriginalScriptedPawnTickVector;
    using I=OriginalScriptedPawnTickInt;using O=OriginalScriptedPawnTickObject;
    using ScriptedPawnTickDetail::Call;
    OriginalScriptedPawnTickResult out;
    const auto rf=[&](F f){++out.floatReads;return Call(q,out,q.readFloat,actor,f);};
    const auto wf=[&](F f,float v){++out.floatWrites;Call(q,out,q.writeFloat,actor,f,v);};
    const auto rb=[&](B f){++out.flagReads;return Call(q,out,q.readFlag,actor,f);};
    const auto wb=[&](B f,bool v){++out.flagWrites;Call(q,out,q.writeFlag,actor,f,v);};
    const auto ry=[&](Y f){++out.byteReads;return Call(q,out,q.readByte,actor,f);};
    const auto wy=[&](Y f,std::uint8_t v){++out.byteWrites;Call(q,out,q.writeByte,actor,f,v);};
    const auto rv=[&](V f,unsigned c){++out.vectorReads;return Call(q,out,q.readVectorComponent,actor,f,c);};
    const auto wv=[&](V f,unsigned c,float v){++out.vectorWrites;Call(q,out,q.writeVectorComponent,actor,f,c,v);};
    const auto base=[&](){++out.baseCalls;out.returned=Call(q,out,q.baseTick,actor,delta,tickType);};
    const auto zero=Memory(0.0f);
    // Actual unsigned role branch order; Role and simulated flag are lazy.
    bool specialRole=ry(Y::RemoteRole)==3u;
    if(!specialRole)specialRole=ry(Y::Role)<2u;
    if((specialRole||tickType==1u)&&!rb(B::SimulatedPawn)){base();return out;}
    out.customPrefix=true;

    // FSUBR retains PC64 age while the first FST spills only the local copy.
    const auto lastRender=Load(rf(F::LastRenderTime));
    const auto age=Subtract(Memory(Call(q,out,q.nativeTime,actor)),lastRender);
    float localAge=StoreFloat(age);
    if(Compare(176u,age,zero))localAge=0.0f;
    ++out.classReads;auto cls=Call(q,out,q.actorClass,actor);
    const auto playerClass=Call(q,out,q.nativePlayerPawnClass);
    if(!playerClass)throw std::runtime_error("Original ScriptedPawn.Tick lost native APlayerPawn Class");
    const auto walk=[&](std::uintptr_t current,std::uintptr_t target) {
        std::array<std::uintptr_t,128u> seen{};std::size_t depth{};
        while(current&&current!=target) {
            if(depth==seen.size()||std::find(seen.begin(),seen.begin()+depth,current)!=seen.begin()+depth)
                throw std::runtime_error("Original ScriptedPawn.Tick Class ancestry cycles or exceeds its bound");
            seen[depth++]=current;++out.classReads;
            current=Call(q,out,q.superClass,current);
        }
        return current==target;
    };
    if(walk(cls,playerClass)&&Call(q,out,q.readPlayerPawnField5D4,actor)) {
        ++out.classReads;cls=Call(q,out,q.actorClass,actor);
        const auto playerSubclass=Call(q,out,q.nativeDeusExPlayerClass);
        if(!playerSubclass)throw std::runtime_error("Original ScriptedPawn.Tick lost native DeusExPlayer Class");
        (void)walk(cls,playerSubclass);
    }
    if(rb(B::Disappear)) {
        const bool stasis=Call(q,out,q.inStasis,actor);
        if(stasis||Compare(177u,Load(localAge),Memory(5.0))) {
            Call(q,out,q.destroyActor,actor,false);
            out.earlyDestroyed=true;out.returned=1;return out;
        }
    }
    if(Compare(177u,Load(rf(F::PrePivotTime)),zero)) {
        if(Compare(176u,Load(delta),Memory(rf(F::PrePivotTime)))) {
            // Original compiler deliberately retains Z through each phase,
            // spills X/Y, then writes X before spilling Z and writing Y/Z.
            const auto dz=Load(rv(V::DesiredPrePivot,2u));
            auto z=Subtract(dz,Memory(rv(V::PrePivot,2u)));
            const auto dy=Load(rv(V::DesiredPrePivot,1u));
            float y=StoreFloat(Subtract(dy,Memory(rv(V::PrePivot,1u))));
            const auto dx=Load(rv(V::DesiredPrePivot,0u));
            float x=StoreFloat(Subtract(dx,Memory(rv(V::PrePivot,0u))));
            z=Multiply(z,Memory(delta));
            y=StoreFloat(Multiply(Load(y),Memory(delta)));
            x=StoreFloat(Multiply(Load(x),Memory(delta)));
            const float reciprocal=StoreFloat(Divide(Memory(1.0),Load(rf(F::PrePivotTime))));
            z=Multiply(z,Memory(reciprocal));
            y=StoreFloat(Multiply(Load(y),Memory(reciprocal)));
            x=StoreFloat(Multiply(Load(x),Memory(reciprocal)));
            z=Add(z,Memory(rv(V::PrePivot,2u)));
            const auto finalY=Add(Load(y),Memory(rv(V::PrePivot,1u)));
            const auto finalX=Add(Load(x),Memory(rv(V::PrePivot,0u)));
            x=StoreFloat(finalX);y=StoreFloat(finalY);
            wv(V::PrePivot,0u,x);
            const float finalZ=StoreFloat(z);
            wv(V::PrePivot,1u,y);wv(V::PrePivot,2u,finalZ);
            wf(F::PrePivotTime,StoreFloat(Subtract(Load(rf(F::PrePivotTime)),Memory(delta))));
        } else {
            for(unsigned c=0u;c<3u;++c)wv(V::PrePivot,c,rv(V::DesiredPrePivot,c));
            wf(F::PrePivotTime,0.0f);
        }
    }
    bool bleedVisible=true;
    if(rb(B::TickVisibleOnly)&&Compare(177u,Load(rf(F::DistanceFromPlayer)),Memory(1200.0f)))bleedVisible=false;
    Call(q,out,q.updateAgitation,actor,delta);
    Call(q,out,q.updateFear,actor,delta);
    const auto decrement=[&](F f,bool onlyPositive) {
        if(onlyPositive&&!Compare(177u,Load(rf(f)),zero))return;
        const auto value=Subtract(Load(rf(f)),Memory(delta));
        wf(f,StoreFloat(value));++out.timerStores;
        if(Compare(176u,value,zero)){wf(f,0.0f);++out.timerStores;}
    };
    decrement(F::AlarmTimer,false);
    if(Call(q,out,q.readObject,actor,O::Weapon)) {
        wf(F::WeaponTimer,StoreFloat(Add(Load(delta),Memory(rf(F::WeaponTimer)))));++out.timerStores;
    } else if(Compare(181u,Load(rf(F::WeaponTimer)),zero)) {
        wf(F::WeaponTimer,0.0f);++out.timerStores;
    }
    decrement(F::FireTimer,true);decrement(F::SpecialTimer,true);
    if(Compare(177u,Load(rf(F::ReloadTimer)),zero)&&Call(q,out,q.readObject,actor,O::Weapon)) {
        wf(F::ReloadTimer,StoreFloat(Subtract(Load(rf(F::ReloadTimer)),Memory(delta))));++out.timerStores;
    } else {wf(F::ReloadTimer,0.0f);++out.timerStores;}
    for(F f:{F::AvoidWallTimer,F::AvoidBumpTimer,F::ObstacleTimer,F::CloakEMPTimer,F::TakeHitTimer,F::CarcassCheckTimer})
        decrement(f,true);
    if(Compare(177u,Load(rf(F::PotentialEnemyTimer)),zero)) {
        const auto value=Subtract(Load(rf(F::PotentialEnemyTimer)),Memory(delta));
        wf(F::PotentialEnemyTimer,StoreFloat(value));++out.timerStores;
        if(Compare(178u,value,zero)) {
            wf(F::PotentialEnemyTimer,0.0f);++out.timerStores;
            Call(q,out,q.clearPotentialEnemyAlliance,actor);
        }
    }
    decrement(F::BeamCheckTimer,true);decrement(F::FutzTimer,true);decrement(F::PlayerAgitationTimer,true);
    if(Compare(179u,Load(rf(F::DistressTimer)),zero)) {
        const auto value=Add(Load(delta),Memory(rf(F::DistressTimer)));
        wf(F::DistressTimer,StoreFloat(value));++out.timerStores;
        if(Compare(177u,value,Memory(rf(F::FearSustainTime)))){wf(F::DistressTimer,-1.0f);++out.timerStores;}
    }
    if(rb(B::HasCloak)) {
        const auto health=Call(q,out,q.readInt,actor,I::Health);
        const auto threshold=Call(q,out,q.readInt,actor,I::CloakThreshold);
        Call(q,out,q.enableCloak,actor,health<=threshold);++out.cloakEvents;
    }
    if(rb(B::AdvancedTactics)) {
        bool acceleration=false;
        for(unsigned c=0u;c<3u&&!acceleration;++c)
            acceleration=Compare(181u,Load(rv(V::Acceleration,c)),zero);
        bool preserve=false;
        if(acceleration&&ry(Y::Physics)==1u)preserve=ry(Y::TurnDirection)!=0u;
        if(!preserve) {
            const auto turn=ry(Y::TurnDirection);
            wb(B::AdvancedTactics,false);
            if(turn)wf(F::MoveTimer,StoreFloat(Subtract(Load(rf(F::MoveTimer)),Memory(4.0))));
            // The original fetches the bClearedObstacle word here, before the
            // object/byte stores, then ORs its captured word at the last store.
            (void)rb(B::ClearedObstacle);
            Call(q,out,q.clearObject,actor,O::ActorAvoiding);
            wy(Y::NextDirection,0u);wy(Y::TurnDirection,0u);
            wb(B::ClearedObstacle,true);wf(F::ObstacleTimer,0.0f);
        }
    }
    if(rb(B::OnFire)) {
        const auto value=Add(Load(delta),Memory(rf(F::BurnTimer)));
        wf(F::BurnTimer,StoreFloat(value));
        if(Compare(179u,value,Memory(rf(F::BurnPeriod)))) {
            Call(q,out,q.extinguishFire,actor);++out.fireEvents;
        }
    }
    if(bleedVisible) {
        if(Compare(177u,Load(rf(F::BleedRate)),zero)&&rb(B::CanBleed)) {
            auto rate=Load(rf(F::BleedRate));
            if(Compare(176u,rate,zero))rate=Load(0.0f);
            else if(!Compare(176u,rate,Memory(1.0f)))rate=Load(1.0f);
            const float numerator=StoreFloat(Add(Subtract(Memory(1.0),rate),Memory(0.1)));
            const auto vz=Load(rv(V::Velocity,2u));
            const auto vy=Load(rv(V::Velocity,1u));
            const auto vx=Load(rv(V::Velocity,0u));
            const auto xy=Add(Multiply(vx,vx),Multiply(vy,vy));
            const double square=StoreDouble(Add(xy,Multiply(vz,vz)));
            auto speed=Call(q,out,q.appSqrt,square);
            // appSqrt has a dead FST m64; its retained return, not that rounded
            // spill, is the operand of the original FMUL m64(1/512).
            (void)StoreDouble(speed);
            speed=Multiply(speed,Memory(0.001953125));
            if(Compare(176u,speed,Memory(0.05f)))speed=Load(0.05f);
            else if(!Compare(176u,speed,Memory(1.0f)))speed=Load(1.0f);
            const float interval=StoreFloat(Divide(Load(numerator),speed));
            wf(F::DropCounter,StoreFloat(Add(Load(delta),Memory(rf(F::DropCounter)))));
            while(Compare(179u,Load(rf(F::DropCounter)),Memory(interval))) {
                Call(q,out,q.spurtBlood,actor);++out.bloodEvents;
                wf(F::DropCounter,StoreFloat(Subtract(Load(rf(F::DropCounter)),Memory(interval))));
            }
            const auto clot=Divide(Load(delta),Memory(rf(F::ClotPeriod)));
            wf(F::BleedRate,StoreFloat(Subtract(Memory(rf(F::BleedRate)),clot)));
        }
        if(Compare(178u,Load(rf(F::BleedRate)),zero)) {
            wf(F::DropCounter,0.0f);wf(F::BleedRate,0.0f);
        }
    }
    base();return out;
}

} // namespace QuestVr
