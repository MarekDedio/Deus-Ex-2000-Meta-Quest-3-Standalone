#include "quest_pawn_rotation.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Rotation=std::array<std::int32_t,3u>;
using Queries=QuestVr::OriginalPawnRotationQueries;
using Stats=QuestVr::OriginalPawnRotationStats;
std::size_t checks{},refusals{},oracleCases{};
void Require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class Function>void Refuses(Function&& fn,const char* message){bool failed{};
    try{fn();}catch(const std::exception&){failed=true;}Require(failed,message);++refusals;}
struct State {
    Rotation rotation{},desired{},rate{};
    std::uint32_t flags=0xabc00120u;
    std::uint8_t physics{};bool player{},pawn=true;
    bool operator==(const State&)const=default;
};
struct Fixture {
    State state;
    std::vector<std::string> events;
    std::vector<std::uint32_t> flagWords;
    std::vector<Rotation> moves;
    std::function<void()> onFlags,onMove,onPlayer;
    std::function<void(unsigned)> onDesired,onCurrent,onRate;
    std::size_t work{},limit=4096u,writes{},writeLimit=4096u;
    Id level=2u;std::int32_t moveResult{};
    void Source(Id a){Require(a==1u,"Pawn rotation changed retained source");}
    void Write(){if(writes>=writeLimit)throw std::runtime_error("write limit");++writes;}
    Queries Query(){Queries q;
        q.work=[&]{if(work>=limit)throw std::runtime_error("work limit");++work;};
        q.rotation=[&](Id a,unsigned c){Source(a);events.push_back("r"+std::to_string(c));
            const auto value=state.rotation.at(c);if(onCurrent)onCurrent(c);return value;};
        q.desiredRotation=[&](Id a,unsigned c){Source(a);events.push_back("d"+std::to_string(c));
            const auto value=state.desired.at(c);if(onDesired)onDesired(c);return value;};
        q.rotationRate=[&](Id a,unsigned c){Source(a);events.push_back("rate"+std::to_string(c));
            const auto value=state.rate.at(c);if(onRate)onRate(c);return value;};
        q.rotationFlags=[&](Id a){Source(a);events.emplace_back("flags");return state.flags;};
        q.writeRotationFlags=[&](Id a,std::uint32_t word){Source(a);events.emplace_back("write-flags");Write();
            flagWords.push_back(word);state.flags=word;if(onFlags)onFlags();};
        q.isPlayerPawnClass=[&](Id a){Source(a);events.emplace_back("player");const bool value=state.player;if(onPlayer)onPlayer();return value;};
        q.isPawnClass=[&](Id a){Source(a);events.emplace_back("pawn");return state.pawn;};
        q.physics=[&](Id a){Source(a);events.emplace_back("physics");return state.physics;};
        q.level=[&](Id a){Source(a);events.emplace_back("level");return level;};
        q.moveActor=[&](Id l,Id a,PortableModelVec3 delta,Rotation proposal,QuestVr::OriginalActorLineHit& hit,std::array<std::int32_t,4u> options){
            Source(a);Require(l==2u&&delta.x==0&&delta.y==0&&delta.z==0&&options==std::array<std::int32_t,4u>{},"Pawn rotation changed native movement packet");
            Require(!hit.next&&!hit.actor&&!hit.primitive&&hit.time==1.0f&&hit.item==-1&&hit.location.x==0&&hit.location.y==0&&hit.location.z==0&&
                hit.normal.x==0&&hit.normal.y==0&&hit.normal.z==0,"Pawn rotation changed initial hit packet");
            events.emplace_back("move");moves.push_back(proposal);Write();state.rotation=proposal;
            hit.time=0;hit.actor=7u;hit.item=123;if(onMove)onMove();return moveResult;};
        return q;
    }
    static std::array<float,3u> Old(){return {std::bit_cast<float>(0x7fa12345u),std::bit_cast<float>(0xff800000u),-0.0f};}
    Stats Direct(float dt,const Queries& q){return QuestVr::RotateOriginalPawn(1u,dt,Old(),q);}
    Stats Run(float dt=0.25f){const auto before=state;try{return Direct(dt,Query());}catch(...){state=before;throw;}}
};
void SourceSchedule(){
    Fixture unchanged;const auto a=unchanged.Run();
    Require(a.flagWrites==1u&&!a.moveCalls&&!a.changed&&a.conversions==1u&&a.flagReads==1u&&unchanged.writes==1u,
        "Nonplayer equal angles skipped unconditional yaw conversion or native flag store");
    Require(unchanged.state.flags==0xabc00140u&&unchanged.events==std::vector<std::string>{"r0","r1","r2","player","rate1","flags",
        "write-flags","d1","d0","r0","r1","r2"},"Pawn source schedule, unrelated flags or no-move lazy admission changed");
    unchanged.state.flags=0xabc00140u;Require(unchanged.Run().flagWrites==1u&&unchanged.writes==2u,"Same-bit native flag store optimized away");
    Fixture zero;zero.state.rotation={0,65537,0};zero.state.desired={0,2,0};const auto z=zero.Run(0);
    Require(z.fixedTurns==1u&&z.flagReads==1u&&zero.state.rotation[1]==1&&z.moveCalls==1u,
        "Zero-delta fixedTurn was skipped, failed low16 normalization or read flags");
    Fixture yaw;yaw.state.desired[1]=1000;yaw.state.rate[1]=128;
    yaw.onDesired=[&](unsigned axis){if(axis==1u)yaw.state.desired[1]=2000;};const auto y=yaw.Run();
    Require(y.proposed[1]==32&&y.flagReads==2u&&std::count(yaw.events.begin(),yaw.events.end(),"d1")==1,
        "DesiredYaw gate was reread for target or actual fixedTurn flags not fresh");
    Require(yaw.events.back()=="move"&&yaw.moveResult==0&&yaw.moves.size()==1u,"False movement result triggered extra completion/event work");
    Fixture fresh;fresh.state.desired[1]=1000;fresh.state.rate[1]=128;
    fresh.onFlags=[&]{fresh.state.flags&=~0x40u;};const auto f=fresh.Run();
    Require(f.fixedTurns==1u&&f.proposed[1]==0&&!f.moveCalls,"FixedTurn cached pre-store/initial flags instead of fresh word");
    Fixture player;player.state.player=true;player.state.rotation={123,456,0};auto sparse=player.Query();
    sparse.rotationFlags={};sparse.writeRotationFlags={};sparse.rotationRate={};sparse.desiredRotation={};sparse.physics={};sparse.isPawnClass={};sparse.level={};sparse.moveActor={};
    const auto p=player.Direct(std::bit_cast<float>(0x7fa23456u),sparse);
    Require(p.playerPawn&&!p.flagWrites&&!p.conversions&&!p.moveCalls&&p.rotationReads==6u,"PlayerPawn consumed unused time/rates/flags or moved equal pose");
    Fixture roll;roll.state.player=true;roll.state.rotation={123,456,7};const auto r=roll.Run(std::numeric_limits<float>::infinity());
    Require(r.moveCalls==1u&&r.proposed==Rotation{123,456,0}&&!r.flagWrites,"PlayerPawn did not clear Roll0 while skipping nonplayer math");
    Fixture required;Refuses([&]{required.Run(std::numeric_limits<float>::infinity());},"Nonplayer skipped unconditional exceptional yaw conversion");
    Require(required.flagWords.empty(),"Exceptional yaw conversion wrote flags before refusal");
}
void PitchAndFreshReads(){
    for(unsigned mode:{0u,1u,2u,3u})for(bool pawn:{false,true}){
        Fixture f;f.state.physics=static_cast<std::uint8_t>(mode);f.state.pawn=pawn;f.state.desired[0]=5000;f.state.rate[0]=10000;
        const auto r=f.Run(0.125f);const bool flat=mode==1u||(mode==2u&&pawn);
        Require(r.pitchAdjusted&&r.pitchDelta==4096&&r.proposed[0]==(flat?0:4096)&&r.conversions==2u&&
            r.pawnCasts==(mode==2u?1u:0u)&&std::count(f.events.begin(),f.events.end(),"d0")== (flat?1:2),
            "Pawn Pitch32768 conversion, actual mode/ancestry flattening or fresh target read changed");
    }
    for(auto rate:std::array<std::int32_t,6u>{-1,0,100,32768,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()}){
        Fixture low;low.state.desired[0]=5000;low.state.rate[0]=rate;const auto l=low.Run(0.25f);
        Require(l.proposed[0]==(5000>rate?rate:5000),"Low signed Pawn pitch clamp normalized/saturated the raw rate");
        Fixture high;high.state.rotation[0]=60000;high.state.desired[0]=55000;high.state.rate[0]=rate;const auto h=high.Run(0.25f);
        const auto bound=std::bit_cast<std::int32_t>(std::uint32_t{65536u}-static_cast<std::uint32_t>(rate));
        Require(h.proposed[0]==(55000<bound?bound:55000),"High signed Pawn pitch clamp lost wrapping32 bound");
    }
    Fixture changedTarget;changedTarget.state.desired[0]=500;changedTarget.state.rate[0]=10000;
    changedTarget.onDesired=[&](unsigned axis){if(axis==0u)changedTarget.state.desired[0]=1000;};
    Require(changedTarget.Run().proposed[0]==1000,"Pitch argument target reused first gate instead of second fresh read");
    Fixture equalPitch;equalPitch.state.rotation[0]=900;equalPitch.state.desired[0]=900;auto sparse=equalPitch.Query();sparse.physics={};sparse.isPawnClass={};
    Require(!equalPitch.Direct(0.25f,sparse).pitchAdjusted,"Equal raw Pitch consumed inactive mode/ancestry providers");
    Fixture mutation;mutation.state.desired[1]=32;mutation.state.rate[1]=128;mutation.onFlags=[&]{mutation.state.rotation[0]=7;};
    const auto m=mutation.Run();Require(m.moveCalls==1u&&m.proposed[0]==0&&m.rotationReads==4u,"Final comparison did not use fresh current Pitch/short-circuit");
}
void Failures(){
    Refuses([]{QuestVr::RotateOriginalPawn(0u,0,{},{});},"Null Pawn rotation accepted");
    for(unsigned missing=0u;missing<7u;++missing){Fixture f;auto q=f.Query();
        switch(missing){case 0:q.work={};break;case 1:q.rotation={};break;case 2:q.isPlayerPawnClass={};break;case 3:q.rotationRate={};break;
        case 4:q.rotationFlags={};break;case 5:q.writeRotationFlags={};break;case 6:q.desiredRotation={};break;}
        Refuses([&]{f.Direct(0.25f,q);},"Missing reached rotation provider silently succeeded");
        if(!missing)Require(f.events.empty(),"Missing caller work still ran providers");
    }
    for(unsigned missing=0u;missing<2u;++missing){Fixture f;f.state.desired[1]=1000;f.state.rate[1]=128;auto q=f.Query();
        if(missing)q.moveActor={};else q.level={};Refuses([&]{f.Direct(0.25f,q);},"Reached missing movement service silently succeeded");
        Require(f.writes==1u&&f.state.flags==0xabc00140u,"Direct helper hid prior native flag store through local rollback");
    }
    Fixture null;null.state.player=true;null.state.rotation[2]=7;null.level=0;Refuses([&]{null.Run();},"Reached null native Level accepted");
    const auto configure=[](Fixture& f){f.state.rotation={2000,1000,3};f.state.desired={4000,2000,9};f.state.rate={10000,128,999};};
    Fixture baseline;configure(baseline);const auto done=baseline.Run();
    for(std::size_t cut=0u;cut<done.steps;++cut){Fixture f;configure(f);f.limit=cut;const auto before=f.state;
        Refuses([&]{f.Run();},"Shared work failure was swallowed");Require(f.state==before,"Work failure escaped caller rollback");}
    for(std::size_t cut=0u;cut<baseline.writes;++cut){Fixture f;configure(f);f.writeLimit=cut;const auto before=f.state;
        Refuses([&]{f.Run();},"Shared write failure was swallowed");Require(f.state==before,"Write failure escaped caller rollback");}
    Fixture late;configure(late);late.onMove=[] {throw std::runtime_error("late movement");};const auto before=late.state;
    Refuses([&]{late.Run();},"Movement callback failure swallowed");Require(late.state==before,"Movement failure escaped caller undo");
}
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
struct Precision {
    unsigned short saved{},truncation{};
    Precision(){static_assert(std::numeric_limits<long double>::digits==64);asm volatile("fnstcw %0":"=m"(saved));
        truncation=static_cast<unsigned short>((saved&~0x0f00u)|0x0f3fu);asm volatile("fnclex; fldcw %0"::"m"(truncation):"memory");}
    ~Precision(){asm volatile("fnclex; fldcw %0"::"m"(saved):"memory");}
};
std::int32_t OracleDelta(std::int32_t rate,float dt){std::int64_t value{};
    // INT*Float has <=55 significant bits, exactly retained at PC64. Only
    // truncating integer conversion needs the selected control-word rounding.
    asm volatile("fildl %1; fmuls %2; fistpq %0":"=m"(value):"m"(rate),"m"(dt):"st","memory");
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}
std::int32_t OracleTurn(std::int32_t current,std::int32_t desired,std::int32_t delta){
    const auto c=static_cast<std::uint32_t>(current)&65535u,d=static_cast<std::uint32_t>(desired)&65535u;
    if(!delta)return static_cast<std::int32_t>(c);
    const auto forward=(d+65536u-c)&65535u,backward=(c+65536u-d)&65535u;
    const bool positive=forward<backward||(forward==backward&&c>d);
    const auto magnitude=static_cast<std::uint32_t>(delta<0?-static_cast<std::int64_t>(delta):delta);
    const auto distance=positive?forward:backward,step=std::min(distance,magnitude);
    return static_cast<std::int32_t>((positive?c+step:c-step)&65535u);
}
void NumericOracle(){Precision precision;std::uint64_t seed=0x103c9f30103ca11bull;
    const auto random=[&]{seed^=seed<<13u;seed^=seed>>7u;seed^=seed<<17u;return static_cast<std::uint32_t>(seed);};
    for(unsigned n=0u;n<32768u;++n){Fixture f;
        const auto a=random(),b=random(),c=random(),d=random();f.state.rotation={static_cast<int>(a&65535u),static_cast<int>(b&65535u),static_cast<int>(c&15u)};
        f.state.desired={static_cast<int>(c&65535u),static_cast<int>(d&65535u),123};
        f.state.rate={static_cast<std::int32_t>(random()),static_cast<std::int32_t>(static_cast<int>(random()%2000001u)-1000000),333};
        f.state.physics=static_cast<std::uint8_t>(n%4u);f.state.pawn=(n&8u)!=0u;
        const float dt=std::bit_cast<float>((static_cast<std::uint32_t>(n&1u)<<31u)|((120u+n%12u)<<23u)|(random()&0x7fffffu));
        const auto before=f.state;auto expected=before.rotation;expected[2]=0;
        const auto yawDelta=OracleDelta(before.rate[1],dt);
        if(before.desired[1]!=before.rotation[1])expected[1]=OracleTurn(before.rotation[1],before.desired[1],yawDelta);
        if(before.desired[0]!=before.rotation[0]){const auto pitchDelta=OracleDelta(32768,dt);
            const auto target=before.physics==1u||(before.physics==2u&&before.pawn)?0:before.desired[0];
            auto pitch=OracleTurn(before.rotation[0],target,pitchDelta);
            const auto bound=std::bit_cast<std::int32_t>(65536u-static_cast<std::uint32_t>(before.rate[0]));
            if(pitch<32768)pitch=std::min(pitch,before.rate[0]);else pitch=std::max(pitch,bound);expected[0]=pitch;}
        const auto actual=f.Run(dt);++oracleCases;
        Require(actual.proposed==expected&&f.state.rotation==expected&&actual.yawDelta==yawDelta&&actual.flagWrites==1u&&
            f.state.flags==((before.flags&~0x20u)|0x40u),"Pawn rotation differs from independent x87/integer oracle");
    }
}
#else
void NumericOracle(){throw std::runtime_error("Pawn rotation acceptance requires own GNU x87/integer oracle, no skipped acceptance");}
#endif
} // namespace
int main(){try{SourceSchedule();PitchAndFreshReads();Failures();NumericOracle();
    std::cout<<"Original Pawn rotation: "<<checks<<" checks / "<<refusals<<" refusals / "<<oracleCases
        <<" independent x87/integer cases; NOT full Pawn movement/Training or headset acceptance\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
