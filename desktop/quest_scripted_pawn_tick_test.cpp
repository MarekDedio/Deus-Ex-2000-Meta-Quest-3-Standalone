#include "quest_scripted_pawn_tick.h"

#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {
using Id=std::uintptr_t;
using F=QuestVr::OriginalScriptedPawnTickFloat;
using B=QuestVr::OriginalScriptedPawnTickFlag;
using Y=QuestVr::OriginalScriptedPawnTickByte;
using V=QuestVr::OriginalScriptedPawnTickVector;
using I=QuestVr::OriginalScriptedPawnTickInt;
using O=QuestVr::OriginalScriptedPawnTickObject;
using Queries=QuestVr::OriginalScriptedPawnTickQueries;
constexpr Id actor=0x100u,derivedClass=0x200u,scriptedClass=0x300u,pawnClass=0x400u,playerClass=0x500u;
constexpr Id deusExPlayerClass=0x600u,extensionPlayerClass=0x700u;
std::size_t checks{},refusals{};
void Require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class Function> void Refuses(Function&& fn,const char* message) {
    bool refused{};try{fn();}catch(const std::exception&){refused=true;}Require(refused,message);++refusals;
}
std::uint32_t Bits(float x){return std::bit_cast<std::uint32_t>(x);}
float Float(std::uint32_t x){return std::bit_cast<float>(x);}
template<class Enum> std::string Tag(const char* prefix,Enum value) {return prefix+std::to_string(static_cast<unsigned>(value));}
struct Fixture {
    std::map<F,float> floats;
    std::map<B,bool> flags;
    std::map<Y,std::uint8_t> bytes{{Y::Role,4u},{Y::RemoteRole,2u}};
    std::map<V,std::array<float,3u>> vectors;
    std::map<I,std::int32_t> ints;
    std::map<O,Id> objects;
    std::map<Id,Id> supers{{derivedClass,scriptedClass},{scriptedClass,pawnClass},{pawnClass,0u},{playerClass,pawnClass},
        {deusExPlayerClass,extensionPlayerClass},{extensionPlayerClass,playerClass}};
    Id cls=derivedClass;
    std::uint32_t field5D4{},alliance=123u;
    double time{};
    bool stasis{};
    int baseReturn=7;
    std::size_t work{},limit=std::numeric_limits<std::size_t>::max(),blood{},cloak{},fire{},destroy{},agitation{},fear{},base{};
    std::vector<std::string> events;
    std::function<void()> onAgitation,onFear,onCloak,onFire,onBlood;
    bool cloakValue{};
    float seenDelta{};std::uint32_t seenType{};
    Fixture(){floats[F::DistressTimer]=-1.0f;floats[F::ClotPeriod]=10.0f;}
    void Source(Id source){Require(source==actor,"Provider changed the captured receiver");}
    Queries Query() {
        Queries q;
        q.work=[&]{if(work++>=limit)throw std::runtime_error("caller work exhausted");};
        q.readFloat=[&](Id a,F f){Source(a);events.push_back(Tag("rf",f));return floats[f];};
        q.writeFloat=[&](Id a,F f,float x){Source(a);events.push_back(Tag("wf",f));floats[f]=x;};
        q.readFlag=[&](Id a,B b){Source(a);events.push_back(Tag("rb",b));return flags[b];};
        q.writeFlag=[&](Id a,B b,bool x){Source(a);events.push_back(Tag("wb",b));flags[b]=x;};
        q.readByte=[&](Id a,Y b){Source(a);events.push_back(Tag("ry",b));return bytes[b];};
        q.writeByte=[&](Id a,Y b,std::uint8_t x){Source(a);events.push_back(Tag("wy",b));bytes[b]=x;};
        q.readVectorComponent=[&](Id a,V v,unsigned c){Source(a);Require(c<3u,"Invalid vector channel");
            events.push_back(Tag("rv",v)+":"+std::to_string(c));return vectors[v][c];};
        q.writeVectorComponent=[&](Id a,V v,unsigned c,float x){Source(a);Require(c<3u,"Invalid vector channel store");
            events.push_back(Tag("wv",v)+":"+std::to_string(c));vectors[v][c]=x;};
        q.readInt=[&](Id a,I i){Source(a);events.push_back(Tag("ri",i));return ints[i];};
        q.readObject=[&](Id a,O o){Source(a);events.push_back(Tag("ro",o));return objects[o];};
        q.clearObject=[&](Id a,O o){Source(a);events.push_back(Tag("co",o));objects[o]=0u;};
        q.clearPotentialEnemyAlliance=[&](Id a){Source(a);events.push_back("alliance=None");alliance=0u;};
        q.nativeTime=[&](Id a){Source(a);events.push_back("time");return time;};
        q.actorClass=[&](Id a){Source(a);events.push_back("class");return cls;};
        q.superClass=[&](Id c){events.push_back("super"+std::to_string(c));return supers.at(c);};
        q.nativePlayerPawnClass=[&]{events.push_back("playerClass");return playerClass;};
        q.nativeDeusExPlayerClass=[&]{events.push_back("deusExPlayerClass");return deusExPlayerClass;};
        q.readPlayerPawnField5D4=[&](Id a){Source(a);events.push_back("5D4");return field5D4;};
        q.inStasis=[&](Id a){Source(a);events.push_back("stasis");return stasis;};
        q.destroyActor=[&](Id a,bool force){Source(a);Require(!force,"Original disappear forced destruction");
            events.push_back("destroy");++destroy;};
        q.updateAgitation=[&](Id a,float dt){Source(a);seenDelta=dt;events.push_back("agitation");++agitation;if(onAgitation)onAgitation();};
        q.updateFear=[&](Id a,float dt){Source(a);Require(Bits(dt)==Bits(seenDelta),"Decay callbacks changed delta");
            events.push_back("fear");++fear;if(onFear)onFear();};
        q.enableCloak=[&](Id a,bool x){Source(a);events.push_back("cloak");++cloak;cloakValue=x;if(onCloak)onCloak();};
        q.extinguishFire=[&](Id a){Source(a);events.push_back("fire");++fire;if(onFire)onFire();};
        q.spurtBlood=[&](Id a){Source(a);events.push_back("blood");++blood;if(onBlood)onBlood();};
        q.appSqrt=[&](double square){events.push_back("sqrt");return QuestVr::NativeScalar::Sqrt(square);};
        q.baseTick=[&](Id a,float dt,std::uint32_t type){Source(a);events.push_back("base");++base;
            seenDelta=dt;seenType=type;return baseReturn;};
        return q;
    }
    auto Tick(float dt=0.25f,std::uint32_t type=0u){return QuestVr::TickOriginalScriptedPawn(actor,dt,type,Query());}
    std::size_t At(const std::string& event)const {
        const auto found=std::find(events.begin(),events.end(),event);
        Require(found!=events.end(),"Expected provider event is absent");return static_cast<std::size_t>(found-events.begin());
    }
};
void GatesAndClassWalks() {
    for(unsigned remote=0u;remote<5u;++remote)for(unsigned role=0u;role<5u;++role)
        for(unsigned type=0u;type<3u;++type)for(unsigned simulated=0u;simulated<2u;++simulated) {
            Fixture f;f.bytes[Y::RemoteRole]=static_cast<std::uint8_t>(remote);
            f.bytes[Y::Role]=static_cast<std::uint8_t>(role);f.flags[B::SimulatedPawn]=simulated!=0u;
            auto q=f.Query();const bool custom=simulated||(remote!=3u&&role>=2u&&type!=1u);
            if(!custom){q.readFloat={};q.actorClass={};q.nativeTime={};q.updateFear={};q.updateAgitation={};}
            const auto result=QuestVr::TickOriginalScriptedPawn(actor,0.125f,type,q);
            Require(result.customPrefix==custom&&result.returned==7&&result.baseCalls==1u,"Role/tickType native gate changed");
            Require(f.agitation==static_cast<std::size_t>(custom)&&f.fear==static_cast<std::size_t>(custom),"Custom gate skipped or invented decay calls");
            Require(f.events.front()==Tag("ry",Y::RemoteRole),"RemoteRole was not the first receiver read");
            if(remote==3u)Require(std::find(f.events.begin(),f.events.end(),Tag("ry",Y::Role))==f.events.end(),"RemoteRole3 eagerly read Role");
            Require(result.steps==f.work,"Reached provider work/counter changed");
        }
    Fixture actual;auto q=actual.Query();q.readPlayerPawnField5D4={};q.nativeDeusExPlayerClass={};
    const auto done=QuestVr::TickOriginalScriptedPawn(actor,0.25f,0u,q);
    Require(done.customPrefix&&actual.At("class")<actual.At("playerClass")&&actual.At("playerClass")<actual.At("super512"),"Actual compatibility Class walk order changed");
    Fixture player;player.cls=playerClass;player.field5D4=1u;player.Tick();
    Require(player.At("5D4")<player.At("deusExPlayerClass")&&std::count(player.events.begin(),player.events.end(),"class")==2,
        "Actual PlayerPawn arm omitted +5D4 or the fresh second Class walk");
    Fixture dxPlayer;dxPlayer.cls=deusExPlayerClass;dxPlayer.field5D4=1u;dxPlayer.Tick();
    Require(dxPlayer.At("5D4")<dxPlayer.At("deusExPlayerClass")&&
        std::count(dxPlayer.events.begin(),dxPlayer.events.end(),"super1536")==1&&
        std::count(dxPlayer.events.begin(),dxPlayer.events.end(),"class")==2,
        "Compatibility second walk did not target the actual DeusExPlayer Class through PlayerPawnExt ancestry");
    Fixture noField;noField.cls=playerClass;q=noField.Query();q.nativeDeusExPlayerClass={};
    QuestVr::TickOriginalScriptedPawn(actor,0.25f,0u,q);
    Require(noField.At("5D4")<noField.At(Tag("rb",B::Disappear)),"Zero +5D4 did not end the readonly compatibility arm");
    Fixture cycle;cycle.supers[derivedClass]=derivedClass;
    Refuses([&]{cycle.Tick();},"Cyclic native ancestry accepted");Require(!cycle.agitation&&!cycle.base,"Cycle refusal ran later custom/base code");
}
void DisappearanceAndCopies() {
    Fixture f;f.flags[B::Disappear]=true;f.stasis=true;auto q=f.Query();q.readVectorComponent={};q.updateAgitation={};q.baseTick={};
    const auto early=QuestVr::TickOriginalScriptedPawn(actor,0.25f,0u,q);
    Require(early.earlyDestroyed&&early.returned==1&&!early.baseCalls&&f.destroy==1u,"InStasis disappearance did not return1 before base");
    Fixture age;age.flags[B::Disappear]=true;age.time=5.0+std::ldexp(1.0,-25);age.Tick();
    Require(!age.destroy&&age.base==1u,"Disappearance compared retained age instead of its native Float spill");
    Fixture old;old.flags[B::Disappear]=true;old.time=5.000001;old.Tick();Require(old.destroy==1u&&!old.base,"Spilled age>5 did not destroy early");
    for(double time:{-100.0,std::numeric_limits<double>::quiet_NaN()}) {
        Fixture negative;negative.flags[B::Disappear]=true;negative.time=time;negative.Tick();
        Require(!negative.destroy&&negative.base==1u,"Negative/unordered age was not clamped before disappearance");
    }
    Fixture copy;copy.floats[F::PrePivotTime]=0.25f;
    copy.vectors[V::DesiredPrePivot]={Float(0x7f800001u),Float(0x80000000u),Float(0xffc12345u)};
    copy.Tick();
    for(unsigned c=0u;c<3u;++c)Require(Bits(copy.vectors[V::PrePivot][c])==Bits(copy.vectors[V::DesiredPrePivot][c]),"Completed PrePivot copy quieted/changed raw words");
    Require(Bits(copy.floats[F::PrePivotTime])==0u,"Completed PrePivot did not store positive zero");
    Require(copy.At("rv1:0")<copy.At("wv0:0")&&copy.At("wv0:0")<copy.At("rv1:1")&&copy.At("wv0:1")<copy.At("rv1:2"),
        "Completed PrePivot eagerly captured the entire vector instead of interleaved raw copies");
}
void TimersAndCallbacks() {
    Fixture f;f.objects[O::Weapon]=0x99u;
    f.floats[F::AlarmTimer]=0.125f;f.floats[F::WeaponTimer]=2.0f;
    f.floats[F::FireTimer]=0.25f;f.floats[F::SpecialTimer]=0.125f;f.floats[F::ReloadTimer]=0.125f;
    for(F field:{F::AvoidWallTimer,F::AvoidBumpTimer,F::ObstacleTimer,F::CloakEMPTimer,F::TakeHitTimer,F::CarcassCheckTimer,
        F::PotentialEnemyTimer,F::BeamCheckTimer,F::FutzTimer,F::PlayerAgitationTimer})f.floats[field]=0.25f;
    f.floats[F::DistressTimer]=0.25f;f.floats[F::FearSustainTime]=0.375f;
    const auto done=f.Tick();
    Require(f.floats[F::AlarmTimer]==0.0f&&f.floats[F::WeaponTimer]==2.25f&&f.floats[F::FireTimer]==0.0f&&
        f.floats[F::SpecialTimer]==0.0f&&f.floats[F::ReloadTimer]==-0.125f,"Native timer increment/decrement/clamp rules changed");
    Require(!f.alliance&&f.floats[F::DistressTimer]==-1.0f,"PotentialEnemy equality/Distress strict threshold changed");
    Require(std::count(f.events.begin(),f.events.end(),Tag("wf",F::FireTimer))==1&&
        std::count(f.events.begin(),f.events.end(),Tag("wf",F::PotentialEnemyTimer))==2,"Strict negative vs nonpositive timer clamp store order changed");
    std::vector<F> order{F::AlarmTimer,F::WeaponTimer,F::FireTimer,F::SpecialTimer,F::ReloadTimer,F::AvoidWallTimer,
        F::AvoidBumpTimer,F::ObstacleTimer,F::CloakEMPTimer,F::TakeHitTimer,F::CarcassCheckTimer,F::PotentialEnemyTimer,
        F::BeamCheckTimer,F::FutzTimer,F::PlayerAgitationTimer,F::DistressTimer};
    for(std::size_t i=1u;i<order.size();++i)Require(f.At(Tag("wf",order[i-1u]))<f.At(Tag("wf",order[i])),"Native timer store sequence changed");
    Require(done.timerStores>order.size()&&f.At("agitation")<f.At("fear")&&f.At("fear")<f.At(Tag("rf",F::AlarmTimer)),"Decay/base callback or timer counters changed");
    Fixture fresh;fresh.onAgitation=[&]{fresh.floats[F::AlarmTimer]=4.0f;};
    fresh.onFear=[&]{fresh.flags[B::HasCloak]=true;fresh.ints[I::Health]=25;fresh.ints[I::CloakThreshold]=25;};
    fresh.onCloak=[&]{fresh.flags[B::OnFire]=true;fresh.floats[F::BurnTimer]=0.875f;fresh.floats[F::BurnPeriod]=1.0f;};
    fresh.onFire=[&]{fresh.floats[F::BleedRate]=-1.0f;fresh.floats[F::DropCounter]=10.0f;};
    fresh.baseReturn=0;const auto freshDone=fresh.Tick();
    Require(fresh.floats[F::AlarmTimer]==3.75f&&fresh.cloakValue&&fresh.cloak==1u&&fresh.fire==1u&&
        fresh.floats[F::BurnTimer]==1.125f&&!fresh.floats[F::BleedRate]&&!fresh.floats[F::DropCounter],"Reached callbacks did not expose fresh later fields");
    Require(freshDone.returned==0&&freshDone.baseCalls==1u&&fresh.events.back()=="base","Genuine false base return was replaced by custom success");
    Fixture nan;nan.floats[F::WeaponTimer]=Float(0x7fc12345u);nan.floats[F::ReloadTimer]=Float(0x7fc12345u);
    nan.floats[F::FireTimer]=-1.0f;nan.Tick();
    Require(Bits(nan.floats[F::WeaponTimer])==0x7fc12345u&&!nan.floats[F::ReloadTimer]&&nan.floats[F::FireTimer]==-1.0f,
        "Native C3 unordered WeaponTimer/reload or positive-only timer predicate changed");
}
void TacticsAndBleeding() {
    Fixture keep;keep.flags[B::AdvancedTactics]=true;keep.vectors[V::Acceleration]={0.0f,1.0f,0.0f};
    keep.bytes[Y::Physics]=1u;keep.bytes[Y::TurnDirection]=2u;keep.Tick();
    Require(keep.flags[B::AdvancedTactics]&&keep.bytes[Y::TurnDirection]==2u,"Moving walking tactical turn was cleared");
    Require(std::find(keep.events.begin(),keep.events.end(),"rv2:2")==keep.events.end(),"Nonzero acceleration failed to short circuit later vector loads");
    Fixture clear;clear.flags[B::AdvancedTactics]=true;clear.bytes[Y::TurnDirection]=3u;clear.bytes[Y::NextDirection]=2u;
    clear.floats[F::MoveTimer]=7.0f;clear.objects[O::ActorAvoiding]=0x88u;clear.Tick();
    Require(!clear.flags[B::AdvancedTactics]&&clear.flags[B::ClearedObstacle]&&clear.floats[F::MoveTimer]==3.0f&&
        !clear.objects[O::ActorAvoiding]&&!clear.bytes[Y::TurnDirection]&&!clear.bytes[Y::NextDirection],"Native tactical teardown fields changed");
    Require(clear.At(Tag("wb",B::AdvancedTactics))<clear.At(Tag("wf",F::MoveTimer))&&
        clear.At(Tag("rb",B::ClearedObstacle))<clear.At(Tag("co",O::ActorAvoiding))&&
        clear.At(Tag("wy",Y::TurnDirection))<clear.At(Tag("wb",B::ClearedObstacle)),"Tactical capture/store sequence changed");
    Fixture hidden;hidden.flags[B::TickVisibleOnly]=true;hidden.floats[F::DistanceFromPlayer]=1201.0f;
    hidden.floats[F::BleedRate]=-1.0f;hidden.floats[F::DropCounter]=5.0f;auto q=hidden.Query();q.appSqrt={};q.spurtBlood={};
    QuestVr::TickOriginalScriptedPawn(actor,0.25f,0u,q);
    Require(hidden.floats[F::BleedRate]==-1.0f&&hidden.floats[F::DropCounter]==5.0f&&hidden.agitation==1u,
        "Invisible bleed gate incorrectly skipped prefix or ran final bleed cleanup");
    Fixture blood;blood.flags[B::CanBleed]=true;blood.floats[F::BleedRate]=1.0f;blood.floats[F::ClotPeriod]=10.0f;
    blood.vectors[V::Velocity]={512.0f,0.0f,0.0f};blood.onBlood=[&]{if(blood.blood==1u)blood.floats[F::DropCounter]=0.125f;};
    const auto result=blood.Tick();
    Require(blood.blood==1u&&result.bloodEvents==1u&&Bits(blood.floats[F::DropCounter])==Bits(0.125f-0.1f)&&
        Bits(blood.floats[F::BleedRate])==Bits(0.975f),"SpurtBlood did not reread counter after callback or clot afterwards");
    Require(blood.At("rv3:2")<blood.At("rv3:1")&&blood.At("rv3:1")<blood.At("rv3:0")&&blood.At("sqrt")<blood.At("blood"),"Native speed load/provider order changed");
    Fixture loop;loop.flags[B::CanBleed]=true;loop.floats[F::BleedRate]=1.0f;loop.vectors[V::Velocity]={512.0f,0.0f,0.0f};
    loop.onBlood=[&]{loop.floats[F::DropCounter]=1.0f;};loop.limit=300u;
    Refuses([&]{loop.Tick();},"Nonterminating callback-mutated blood loop ignored caller work");
    Require(loop.blood>1u&&!loop.base,"Blood loop used an invented cap or continued to base after caller refusal");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct PrecisionOracle {
    unsigned short original{};
    PrecisionOracle(){asm volatile("fnstcw %0":"=m"(original));
        const unsigned short profile=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(profile));}
    ~PrecisionOracle(){asm volatile("fldcw %0"::"m"(original));}
};
// Independent host x87 arithmetic only. No original DLL is loaded/executed.
std::array<float,3u> PivotOracle(std::array<float,3u> desired,std::array<float,3u> current,float time,float dt) {
    volatile long double z=static_cast<long double>(desired[2])-static_cast<long double>(current[2]);
    volatile long double yd=static_cast<long double>(desired[1])-static_cast<long double>(current[1]);float y=static_cast<float>(yd);
    volatile long double xd=static_cast<long double>(desired[0])-static_cast<long double>(current[0]);float x=static_cast<float>(xd);
    z=z*static_cast<long double>(dt);volatile long double yp=static_cast<long double>(y)*static_cast<long double>(dt);y=static_cast<float>(yp);
    volatile long double xp=static_cast<long double>(x)*static_cast<long double>(dt);x=static_cast<float>(xp);
    volatile long double quotient=1.0L/static_cast<long double>(time);const float reciprocal=static_cast<float>(quotient);
    z=z*static_cast<long double>(reciprocal);volatile long double ys=static_cast<long double>(y)*static_cast<long double>(reciprocal);y=static_cast<float>(ys);
    volatile long double xs=static_cast<long double>(x)*static_cast<long double>(reciprocal);x=static_cast<float>(xs);
    z=z+static_cast<long double>(current[2]);volatile long double yf=static_cast<long double>(y)+static_cast<long double>(current[1]);
    volatile long double xf=static_cast<long double>(x)+static_cast<long double>(current[0]);
    return {static_cast<float>(xf),static_cast<float>(yf),static_cast<float>(z)};
}
void IndependentPivotOracle() {
    PrecisionOracle profile;
    std::uint32_t seed=0x61c88647u;
    const auto next=[&](){seed^=seed<<13u;seed^=seed>>17u;seed^=seed<<5u;return seed;};
    for(unsigned trial=0u;trial<2000u;++trial) {
        Fixture f;const float time=Float(0x3f800000u|(next()&0x7fffffu));
        const float dt=Float(0x3e000000u|(next()&0x7fffffu));f.floats[F::PrePivotTime]=time;
        for(unsigned c=0u;c<3u;++c) {
            f.vectors[V::DesiredPrePivot][c]=Float((next()&0x807fffffu)|((48u+next()%160u)<<23u));
            f.vectors[V::PrePivot][c]=Float((next()&0x807fffffu)|((48u+next()%160u)<<23u));
        }
        const auto expected=PivotOracle(f.vectors[V::DesiredPrePivot],f.vectors[V::PrePivot],time,dt);f.Tick(dt);
        for(unsigned c=0u;c<3u;++c)Require(Bits(f.vectors[V::PrePivot][c])==Bits(expected[c]),"Native PC64 PrePivot spill/retained-Z oracle mismatch");
        if(!trial) {
            Require(f.At("rv1:2")<f.At("rv0:2")&&f.At("rv0:2")<f.At("rv1:1")&&
                f.At("rv1:1")<f.At("rv0:1")&&f.At("rv0:1")<f.At("rv1:0")&&
                f.At("wv0:0")<f.At("wv0:1")&&f.At("wv0:1")<f.At("wv0:2"),"Partial PrePivot receiver read/store order changed");
        }
    }
}
#endif
void RefusalsAndSharedRoot() {
    Fixture null;Refuses([&]{QuestVr::TickOriginalScriptedPawn(0u,0.25f,0u,null.Query());},"Null receiver accepted");
    Require(null.events.empty()&&!null.work,"Null receiver demanded field/work authority");
    Fixture missing;auto q=missing.Query();q.updateFear={};
    Refuses([&]{QuestVr::TickOriginalScriptedPawn(actor,0.25f,0u,q);},"Missing reached decay provider became a successful no-op");
    Require(missing.agitation==1u&&!missing.fear&&!missing.base,"Provider refusal skipped earlier callback or ran later code");
    Fixture noWork;q=noWork.Query();q.work={};
    Refuses([&]{QuestVr::TickOriginalScriptedPawn(actor,0.25f,0u,q);},"Missing shared-root work authority accepted");
    Require(noWork.events.empty(),"Missing work invoked a provider");
    Fixture store;q=store.Query();q.writeFloat=[&](Id,F,float){throw std::runtime_error("store refusal");};
    Refuses([&]{QuestVr::TickOriginalScriptedPawn(actor,0.25f,0u,q);},"Store refusal swallowed");
    Require(store.agitation==1u&&store.fear==1u&&!store.base,"Store refusal ran base or invented local rollback");
    Fixture caller;caller.onAgitation=[&]{caller.floats[F::AlarmTimer]=9.0f;};q=caller.Query();q.updateFear={};
    const auto original=caller.floats;
    Refuses([&]{QuestVr::TickOriginalScriptedPawn(actor,0.25f,0u,q);},"Same-root callback refusal swallowed");
    Require(caller.floats[F::AlarmTimer]==9.0f,"Helper invented local callback undo");
    caller.floats=original;Require(!caller.floats[F::AlarmTimer],"Caller root could not undo all custom effects together");
}
} // namespace
int main() {
    try {
        GatesAndClassWalks();DisappearanceAndCopies();TimersAndCallbacks();TacticsAndBleeding();
        RefusalsAndSharedRoot();
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        IndependentPivotOracle();
#endif
        std::cout<<"original ScriptedPawn.Tick checks="<<checks<<" refusals="<<refusals<<'\n';return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
