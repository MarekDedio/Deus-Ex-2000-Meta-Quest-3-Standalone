#include "quest_actor_pawn_tick.h"

#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Queries=QuestVr::OriginalActorPawnTickQueries;
using Context=QuestVr::OriginalActorPawnContext;
using LocalResult=QuestVr::OriginalPawnLocalTickResult;
constexpr Id actor=0x100u,nativePlayer=0x200u,nativeWeapon=0x300u;
std::size_t checks{},refusals{};
void Require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class Function> void Refuses(Function&& fn,const char* message) {
    bool refused{};try{fn();}catch(const std::exception&){refused=true;}
    Require(refused,message);++refusals;
}
[[maybe_unused]] std::uint32_t Bits(float x){return std::bit_cast<std::uint32_t>(x);}
[[maybe_unused]] float Float(std::uint32_t bits){return std::bit_cast<float>(bits);}
struct Fixture {
    std::uint8_t remote=2u,role=4u,physics{};
    Id cast{},player{},weapon{};
    bool probe=true,isPlayer{},viewTarget{};
    float life{};
    std::vector<float> lifeReads;
    std::size_t readLife{};
    LocalResult local=LocalResult::ContinueState;
    std::size_t work{},limit=std::numeric_limits<std::size_t>::max(),writes{},effects{};
    std::array<std::int32_t,3u> rotation{11,22,33},viewRotation{};
    std::array<float,3u> location{1,2,3},weaponLocation{};
    std::vector<std::string> events;
    std::function<void()> onSimulated,onRemote,onLocal,onTick,onState,onTimer,onPhysics,onExpired,onEye,onNetwork,onWeapon;
    void Source(Id a){Require(a==actor,"Continuation changed its captured source Actor/Pawn");}
    void Event(const char* name){events.emplace_back(name);}
    Queries Query() {
        Queries q;
        q.work=[&]{if(work++>=limit)throw std::runtime_error("caller work exhausted");};
        q.remoteRole=[&](Id a){Source(a);Event("remote");return remote;};
        q.role=[&](Id a){Source(a);Event("role");return role;};
        q.physics=[&](Id a){Source(a);Event("physics");return physics;};
        q.playerPawnCast=[&](Id a){Source(a);Event("playerCast");return cast;};
        q.player=[&](Id a){Source(a);Event("player");return player;};
        q.simulatedPawnMovement=[&](Id a,Id pawn,float dt){Source(a);Source(pawn);(void)dt;Event("simulated");++effects;if(onSimulated)onSimulated();};
        q.remotePlayerViewCopy=[&](Id a,Id pawn){Source(a);Source(pawn);Event("remoteView");++effects;if(onRemote)onRemote();};
        q.localPlayerTick=[&](Id a,Id pawn,Id p,float dt,std::int32_t type){Source(a);Source(pawn);(void)dt;(void)type;
            Require(p==nativePlayer,"Local-player service changed the actual Player identity");Event("local");++effects;if(onLocal)onLocal();return local;};
        q.isProbingTick=[&](Id a){Source(a);Event("probeTick");return probe;};
        q.tick=[&](Id a,float dt){Source(a);(void)dt;Event("tick");++effects;if(onTick)onTick();};
        q.processState=[&](Id a,float dt){Source(a);(void)dt;Event("state");++effects;if(onState)onState();};
        q.timer=[&](Id a,float dt){Source(a);(void)dt;Event("timer");++effects;if(onTimer)onTimer();};
        q.performPhysics=[&](Id a,float dt){Source(a);(void)dt;Event("physicsCall");++effects;if(onPhysics)onPhysics();};
        q.lifeSpan=[&](Id a){Source(a);Event("life");return readLife<lifeReads.size()?lifeReads[readLife++]:life;};
        q.writeLifeSpan=[&](Id a,float x){Source(a);Event("writeLife");life=x;++writes;};
        q.expired=[&](Id a){Source(a);Event("expired");++effects;if(onExpired)onExpired();};
        q.destroy=[&](Id a,std::int32_t force){Source(a);Require(force==0,"Expiry changed the original force0 destruction packet");Event("destroy");++effects;};
        q.networkTail=[&](Id a,Id pawn){Source(a);Require(!pawn||pawn==actor,"Network tail changed retained Pawn");Event("network");if(onNetwork)onNetwork();};
        q.pawnIsPlayer=[&](Id pawn){Source(pawn);Event("isPlayer");return isPlayer;};
        q.pawnViewTarget=[&](Id pawn){Source(pawn);Event("viewTarget");return viewTarget;};
        q.updateEyeHeight=[&](Id pawn,float dt){Source(pawn);(void)dt;Event("eye");++effects;if(onEye)onEye();};
        q.copyRotationToView=[&](Id a,Id pawn){Source(a);Source(pawn);Event("copyView");viewRotation=rotation;++writes;};
        q.weapon=[&](Id pawn){Source(pawn);Event("weapon");return weapon;};
        q.farMoveWeapon=[&](Id a,Id w){Source(a);Require(w==nativeWeapon,"Weapon FarMove changed receiver identity");
            Event("farWeapon");weaponLocation=location;++effects;if(onWeapon)onWeapon();};
        q.longPawnTail=[&](Id a,Id pawn,float dt){Source(a);Source(pawn);(void)dt;Event("long");++effects;};
        return q;
    }
    auto Tick(Context context=Context{actor,false},float dt=0.25f,std::int32_t type=0) {
        return QuestVr::AdvanceOriginalActorPawnTick(actor,context,dt,type,Query());
    }
};
void OrdinarySourceOrder() {
    Fixture f;const auto result=f.Tick();
    Require(f.events==std::vector<std::string>{"remote","role","playerCast","probeTick","tick","state","timer","life",
        "physics","network","isPlayer","weapon","role"},"Ordinary Jaime non-player Pawn source order changed");
    Require(result.returned==1&&result.playerCasts==1u&&result.tickCalls==1u&&result.stateCalls==1u&&result.timerCalls==1u&&
        !result.physicsCalls&&result.shortPawnTail&&result.networkTailCalls==1u&&result.steps==f.work,
        "Ordinary Pawn continuation fabricated a return/callback or lost work accounting");
    Fixture none;auto q=none.Query();q.playerPawnCast={};q.pawnIsPlayer={};q.pawnViewTarget={};q.weapon={};q.longPawnTail={};
    const auto null=QuestVr::AdvanceOriginalActorPawnTick(actor,Context{},0.25f,0,q);
    Require(null.nullPawnTail&&null.returned==1&&!null.playerCasts&&none.events.back()=="network",
        "Null captured Pawn demanded PlayerPawn/Pawn fields or skipped actual network tail");
    Fixture type1;q=type1.Query();q.isProbingTick={};q.tick={};q.processState={};q.timer={};q.lifeSpan={};q.physics={};q.networkTail={};
    const auto skipped=QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,false},0.25f,1,q);
    Require(skipped.tickTypeSkipped&&skipped.returned==1&&type1.events==std::vector<std::string>{"remote","role","playerCast"},
        "Literal type1 ran ordinary callbacks or retained-Pawn tail");
    Fixture noPlayer;noPlayer.cast=actor;q=noPlayer.Query();q.localPlayerTick={};q.networkTail={};
    const auto noLocal=QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,false},0.25f,1,q);
    Require(noLocal.tickTypeSkipped&&!noLocal.localPlayerBranch&&noPlayer.events.back()=="player",
        "Actual PlayerPawn with null Player entered local native input or bypassed type1 return");
    Fixture suppressed;suppressed.probe=false;suppressed.physics=0u;q=suppressed.Query();q.tick={};
    const auto noTick=QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,false},0.25f,0,q);
    Require(!noTick.tickCalls&&noTick.stateCalls==1u&&noTick.timerCalls==1u,"Tick probe suppressed state/timer or demanded selected Tick");
}
void NativeBranchesAndFrozenContext() {
    Fixture simulated;simulated.role=4u;simulated.remote=3u;auto q=simulated.Query();
    q.remoteRole={};q.playerPawnCast={};q.processState={};q.timer={};q.lifeSpan={};q.physics={};q.performPhysics={};
    const auto sim=QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,true},0.25f,1,q);
    Require(sim.simulatedBranch&&sim.returned==1&&sim.tickCalls==1u&&!sim.stateCalls&&!sim.timerCalls&&
        simulated.events==std::vector<std::string>{"simulated","probeTick","tick","network","isPlayer","weapon","role"},
        "Captured Role2 was refreshed from live Role or simulated path acquired ordinary/type1 suffix");
    Fixture freshRole2;freshRole2.role=2u;freshRole2.onTick=[&]{freshRole2.role=4u;};
    const auto ordinary=freshRole2.Tick(Context{actor,false});
    Require(!ordinary.simulatedBranch&&ordinary.stateCalls==1u&&ordinary.shortPawnTail,
        "Fresh Role2 incorrectly replaced the frozen non-simulated entry context");
    Fixture remote;remote.remote=3u;remote.cast=actor;q=remote.Query();q.player={};q.isProbingTick={};q.lifeSpan={};q.physics={};
    const auto network=QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,false},0.25f,1,q);
    Require(network.remoteRoleBranch&&network.stateCalls==1u&&network.timerCalls==1u&&!network.tickCalls&&
        remote.events==std::vector<std::string>{"remote","role","playerCast","remoteView","state","timer","network","isPlayer","weapon","role"},
        "RemoteRole3/Role4 source block omitted real player view copy or acquired normal expiry/physics");
    Fixture low;low.remote=2u;low.role=1u;low.physics=2u;q=low.Query();q.playerPawnCast={};q.isProbingTick={};q.processState={};q.timer={};q.lifeSpan={};
    const auto falling=QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,false},0.25f,1,q);
    Require(falling.lowRoleBranch&&falling.physicsCalls==1u&&!falling.tickTypeSkipped&&
        low.events==std::vector<std::string>{"remote","role","physics","physicsCall","network","isPlayer","weapon","role"},
        "Low-role Physics2 was suppressed by type1 or borrowed the ordinary suffix");
    Fixture local;local.cast=actor;local.player=nativePlayer;local.life=2.0f;q=local.Query();q.isProbingTick={};q.tick={};
    const auto input=QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,false},0.25f,1,q);
    Require(input.localPlayerBranch&&!input.tickTypeSkipped&&input.stateCalls==1u&&input.timerCalls==1u&&
        input.lifeSpanWrites==1u&&local.life==1.75f&&!input.tickCalls&&local.events[4u]=="local"&&local.events[5u]=="state",
        "Real local-player continuation did not join shared State after bypassing ordinary Tick/type1 gate");
    Fixture early;early.cast=actor;early.player=nativePlayer;early.local=LocalResult::ReturnOne;q=early.Query();q.networkTail={};q.processState={};
    const auto returned=QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,false},0.25f,0,q);
    Require(returned.localPlayerReturned&&returned.returned==1&&early.events.back()=="local"&&!returned.stateCalls,
        "Native local-player early return incorrectly fell through state/retained tail");
    for(unsigned remoteRole=0u;remoteRole<5u;++remoteRole)for(unsigned role=0u;role<5u;++role)
        for(unsigned physics=0u;physics<4u;++physics)for(std::int32_t type=0;type<3;++type) {
            Fixture packet;packet.remote=static_cast<std::uint8_t>(remoteRole);packet.role=static_cast<std::uint8_t>(role);
            packet.physics=static_cast<std::uint8_t>(physics);const auto branch=packet.Tick(Context{actor,false},0.25f,type);
            const bool normal=remoteRole!=3u&&role>=2u&&type!=1;
            const bool state=(remoteRole==3u&&role==4u)||normal;
            const bool physical=remoteRole!=3u&&((role<2u&&physics==2u)||(normal&&physics!=0u&&role!=3u));
            Require(branch.returned==1&&branch.stateCalls==static_cast<std::size_t>(state)&&branch.timerCalls==static_cast<std::size_t>(state)&&
                branch.tickCalls==static_cast<std::size_t>(normal)&&branch.physicsCalls==static_cast<std::size_t>(physical),
                "Native unsigned role/remote/physics/type branch table changed");
        }
}
void FreshCallbacksAndShortTail() {
    Fixture f;f.life=4.0f;f.physics=1u;
    f.onTick=[&]{f.life=3.0f;f.role=2u;f.isPlayer=true;};
    f.onState=[&]{f.life=2.0f;f.physics=2u;};
    f.onTimer=[&]{f.life=1.0f;f.role=4u;};
    f.onPhysics=[&]{f.role=3u;f.viewTarget=false;f.rotation={101,202,303};f.weapon=nativeWeapon;f.location={8,9,10};};
    f.onWeapon=[&]{f.role=4u;};
    const auto result=f.Tick();
    Require(f.life==0.75f&&result.physicsCalls==1u&&result.viewCopies==1u&&result.weaponMoves==1u&&result.shortPawnTail&&
        f.viewRotation==f.rotation&&f.weaponLocation==f.location,
        "Callbacks did not expose fresh LifeSpan/Physics/Role/player-view/weapon state in native order");
    Require(f.events==std::vector<std::string>{"remote","role","playerCast","probeTick","tick","state","timer","life","life",
        "writeLife","physics","role","physicsCall","network","isPlayer","role","viewTarget","copyView","weapon","farWeapon","role"},
        "Native fresh field and callback order changed");
    Fixture eye;eye.isPlayer=true;eye.viewTarget=true;eye.onEye=[&]{eye.weapon=nativeWeapon;eye.location={31,37,41};eye.role=1u;};
    const auto eyeResult=eye.Tick(Context{actor,false},0.25f,2);
    Require(eyeResult.eyeHeightCalls==1u&&eyeResult.weaponMoves==1u&&!eyeResult.longTailCalls&&eyeResult.shortPawnTail,
        "UpdateEyeHeight callback did not expose fresh weapon/Role before literal long-tail gate");
    Fixture below;below.isPlayer=true;below.role=2u;auto q=below.Query();q.pawnViewTarget={};q.copyRotationToView={};q.updateEyeHeight={};
    const auto noView=QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,false},0.25f,0,q);
    Require(!noView.eyeHeightCalls&&!noView.viewCopies,"Fresh Role<3 demanded player-view fields");
    Fixture longTail;const auto longResult=longTail.Tick(Context{actor,false},0.25f,2);
    Require(longResult.longTailCalls==1u&&!longResult.shortPawnTail&&longTail.events.back()=="long",
        "Fresh Role4/type2 failed to demand real long Pawn native service");
    Fixture stopped;stopped.life=0.125f;stopped.physics=2u;stopped.onExpired=[&]{stopped.life=100.0f;stopped.role=3u;};
    q=stopped.Query();q.performPhysics={};q.networkTail={};
    const auto expired=QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,false},0.25f,0,q);
    Require(expired.expired&&expired.returned==1&&expired.expiredCalls==1u&&expired.destroyCalls==1u&&stopped.life==100.0f&&
        stopped.events[stopped.events.size()-2u]=="expired"&&stopped.events.back()=="destroy",
        "Expired callback reset/deletion suppressed native force0 destroy or acquired later suffix");
}
void RefusalAndSameRoot() {
    Fixture null;Refuses([&]{QuestVr::AdvanceOriginalActorPawnTick(0u,Context{},0.25f,0,null.Query());},"Null Actor accepted");
    Refuses([&]{QuestVr::AdvanceOriginalActorPawnTick(actor,Context{nativeWeapon,false},0.25f,0,null.Query());},"Different captured Pawn accepted");
    Refuses([&]{QuestVr::AdvanceOriginalActorPawnTick(actor,Context{0u,true},0.25f,0,null.Query());},"Simulated flag without captured Pawn accepted");
    Require(null.events.empty()&&!null.work,"Invalid context demanded provider/work authority");
    Fixture noWork;auto q=noWork.Query();q.work={};
    Refuses([&]{QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,false},0.25f,0,q);},"Missing shared work accepted");
    Require(noWork.events.empty(),"Missing caller work invoked provider");
    Fixture badCast;badCast.cast=nativeWeapon;
    Refuses([&]{badCast.Tick();},"Actual PlayerPawn cast returned a different Actor");
    Fixture base;const auto complete=base.Tick();
    for(std::size_t cut=0u;cut<complete.steps;++cut) {
        Fixture bounded;bounded.limit=cut;
        Refuses([&]{bounded.Tick();},"Native continuation ignored caller work cutoff");
        Require(bounded.events.size()==cut&&bounded.work==cut+1u,"Work cutoff reordered or ran later provider");
    }
    for(unsigned service=0u;service<9u;++service) {
        Fixture absent;auto missing=absent.Query();Context capture{actor,false};std::int32_t type{};
        if(service==0u)missing.networkTail={};
        if(service==1u){absent.physics=2u;missing.performPhysics={};}
        if(service==2u){capture.suppressAnimEnd=true;missing.simulatedPawnMovement={};}
        if(service==3u){absent.remote=3u;absent.cast=actor;missing.remotePlayerViewCopy={};}
        if(service==4u){absent.cast=actor;absent.player=nativePlayer;missing.localPlayerTick={};}
        if(service==5u){absent.isPlayer=true;absent.viewTarget=true;missing.updateEyeHeight={};}
        if(service==6u){absent.isPlayer=true;missing.copyRotationToView={};}
        if(service==7u){absent.weapon=nativeWeapon;missing.farMoveWeapon={};}
        if(service==8u){type=2;missing.longPawnTail={};}
        Refuses([&]{QuestVr::AdvanceOriginalActorPawnTick(actor,capture,0.25f,type,missing);},"Missing reached native service became empty success");
    }
    Fixture caller;caller.life=2.0f;caller.onTick=[&]{caller.life=3.0f;};q=caller.Query();q.networkTail={};
    Refuses([&]{QuestVr::AdvanceOriginalActorPawnTick(actor,Context{actor,false},0.25f,0,q);},"Late native service refusal swallowed");
    Require(caller.life==2.75f&&caller.effects==3u&&caller.writes==1u,"Helper invented local undo or skipped earlier shared-root effects");
    caller.life=2.0f;caller.effects=0u;caller.writes=0u;
    Require(caller.life==2.0f&&!caller.effects&&!caller.writes,"Mock caller could not undo all continuation effects together");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct PrecisionOracle {
    unsigned short original{};
    PrecisionOracle(){asm volatile("fnstcw %0":"=m"(original));
        const unsigned short profile=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(profile));}
    ~PrecisionOracle(){asm volatile("fldcw %0"::"m"(original));}
};
struct LifeOracleResult {float stored{};bool written{},expired{};};
// Independent masked x87 instructions reproduce the native initial C3 test,
// fresh FLD, m32 subtraction, Float FST and retained m64 threshold comparison.
// No original commercial DLL is loaded or executed.
LifeOracleResult LifeOracle(float initial,float fresh,float delta) {
    const float zero=0.0f;const double threshold=0.0001;unsigned short status{};
    asm volatile("flds %1; fcomps %2; fnstsw %%ax":"=a"(status):"m"(initial),"m"(zero):"st");
    if(status&0x4000u)return {fresh,false,false};
    float stored{};
    asm volatile("flds %2; fsubs %3; fsts %0; fcompl %4; fnstsw %%ax":
        "=m"(stored),"=a"(status):"m"(fresh),"m"(delta),"m"(threshold):"st");
    return {stored,true,(status&0x4100u)!=0u};
}
void IndependentLifeOracle() {
    PrecisionOracle precision;
    const auto check=[](float initial,float fresh,float delta) {
        const auto expected=LifeOracle(initial,fresh,delta);
        Fixture f;f.life=fresh;f.lifeReads={initial,fresh};const auto result=f.Tick(Context{actor,false},delta,0);
        Require(result.lifeSpanWrites==static_cast<std::size_t>(expected.written)&&result.expired==expected.expired,
            "Native LifeSpan IEEE/C0-C3/retained-expiry oracle predicate mismatch");
        Require(Bits(f.life)==Bits(expected.stored),"Native LifeSpan PC64 Float spill oracle mismatch");
        Require(result.lifeSpanReads==(expected.written?2u:1u),"Active LifeSpan lost its second fresh source FLD");
        Require(result.networkTailCalls==static_cast<std::size_t>(!expected.expired),"Native expiry did not return before retained-Pawn/network tail");
    };
    constexpr std::array<std::uint32_t,16u> values{0u,0x80000000u,1u,0x80000001u,0x3f800000u,0xbf800000u,
        0x38d1b717u,0x38d1b718u,0x38d1b719u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,
        0x7f800001u,0x7fc12345u,0xff800001u};
    for(const auto a:values)for(const auto d:values)check(Float(a),Float(a),Float(d));
    // Initial probe and actual arithmetic are distinct fresh reads.
    check(1.0f,0.0f,0.25f);check(0.0f,1.0f,0.25f);check(1.0f,Float(0x7f800001u),0.25f);
    std::uint32_t seed=0x9e3779b9u;
    const auto next=[&](){seed^=seed<<13u;seed^=seed>>17u;seed^=seed<<5u;return seed;};
    for(unsigned i=0u;i<4096u;++i){const auto a=next();const auto d=next();check(Float(a),Float(a),Float(d));}
}
#endif
} // namespace
int main() {
    try {
        OrdinarySourceOrder();NativeBranchesAndFrozenContext();FreshCallbacksAndShortTail();RefusalAndSameRoot();
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        IndependentLifeOracle();
#endif
        std::cout<<"original Actor Pawn Tick continuation checks="<<checks<<" refusals="<<refusals<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
