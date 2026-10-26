#include "quest_pawn_perform_physics.h"

#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Queries=QuestVr::OriginalPawnPerformPhysicsQueries;
using Dispatch=QuestVr::OriginalPawnPhysicsDispatch;
using Stats=QuestVr::OriginalPawnPerformPhysicsStats;
std::size_t checks{},refusals{},oracleCases{};
void Require(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
float Float(std::uint32_t value){return std::bit_cast<float>(value);}
template<class Function>void Refuses(Function&& fn,const char* message){
    bool failed{};try{fn();}catch(const std::exception&){failed=true;}
    Require(failed,message);++refusals;
}
struct State {
    std::array<float,3u> velocity{1.0f,2.0f,3.0f};
    std::array<std::int32_t,3u> rotation{},desired{},rate{};
    std::array<Id,8u> pending{};
    std::uint8_t physics{};bool player{};
    float timer=1.0f,average=0.5f;
    bool operator==(const State&)const=default;
};
struct Fixture {
    State state;
    std::vector<std::string> events;
    std::vector<Dispatch> dispatches;
    std::vector<std::pair<Id,Id>> posts,pointerWrites;
    std::vector<std::uint32_t> movementDeltas,rotationDeltas;
    std::array<float,3u> rotationVelocity{};
    std::size_t work{},limit=4096u,writes{},writeLimit=4096u;
    std::function<void()> onMovement,onRotation,onTimer,onAverage,onPost,onPending;
    std::function<void(Id,Id)> onPointerWrite;
    void Source(Id a){Require(a==1u,"Pawn outer changed retained source");}
    void Write(){if(writes>=writeLimit)throw std::runtime_error("write limit");++writes;}
    void Pending(Id a,Id b){Require(a>0u&&a<state.pending.size(),"Invalid pending owner");Write();state.pending[a]=b;}
    Queries Query(){
        Queries q;
        q.work=[&]{if(work>=limit)throw std::runtime_error("work limit");++work;};
        q.velocity=[&](Id a,unsigned axis){Source(a);events.push_back("velocity"+std::to_string(axis));return state.velocity.at(axis);};
        q.physics=[&](Id a){Source(a);events.emplace_back("physics");return state.physics;};
        q.falling=[&](Id a,float dt,std::int32_t iterations){Source(a);Require(!iterations,"Pawn falling changed iterations0");
            events.emplace_back("falling");movementDeltas.push_back(Bits(dt));if(onMovement)onMovement();};
        q.otherPhysicsBlock=[&](Id a,Dispatch mode,float dt){Source(a);events.emplace_back("other");dispatches.push_back(mode);
            movementDeltas.push_back(Bits(dt));if(onMovement)onMovement();};
        q.isPlayerPawnClass=[&](Id a){Source(a);events.emplace_back("player");return state.player;};
        q.rotation=[&](Id a,unsigned axis){Source(a);events.push_back("rotation"+std::to_string(axis));return state.rotation.at(axis);};
        q.desiredRotation=[&](Id a,unsigned axis){Source(a);events.push_back("desired"+std::to_string(axis));return state.desired.at(axis);};
        q.rotationRateRoll=[&](Id a){Source(a);events.emplace_back("roll-rate");return state.rate[2];};
        q.physicsRotation=[&](Id a,float dt,std::array<float,3u> oldVelocity){Source(a);events.emplace_back("rotate");
            rotationDeltas.push_back(Bits(dt));rotationVelocity=oldVelocity;if(onRotation)onRotation();};
        q.moveTimer=[&](Id a){Source(a);events.emplace_back("timer");return state.timer;};
        q.averagePhysicsTime=[&](Id a){Source(a);events.emplace_back("average");return state.average;};
        q.writeMoveTimer=[&](Id a,float value){Source(a);events.emplace_back("write-timer");Write();state.timer=value;if(onTimer)onTimer();};
        q.writeAveragePhysicsTime=[&](Id a,float value){Source(a);events.emplace_back("write-average");Write();state.average=value;if(onAverage)onAverage();};
        q.pendingTouch=[&](Id a){Require(a>0u&&a<state.pending.size(),"Invalid pending read");events.push_back("pending"+std::to_string(a));
            const auto value=state.pending[a];if(onPending)onPending();return value;};
        q.postTouch=[&](Id retained,Id other){Source(other);events.emplace_back("post");posts.emplace_back(retained,other);if(onPost)onPost();};
        q.writePendingTouch=[&](Id a,Id next){events.emplace_back("write-pending");Pending(a,next);pointerWrites.emplace_back(a,next);
            if(onPointerWrite)onPointerWrite(a,next);};
        return q;
    }
    Stats Direct(float dt,const Queries& q){return QuestVr::PerformOriginalPawnPhysics(1u,dt,q);}
    Stats Run(float dt=0.25f){const auto before=state;try{return Direct(dt,Query());}catch(...){state=before;throw;}}
};

void DispatchAndFreshRotation(){
    for(unsigned mode=0u;mode<256u;++mode){
        Fixture f;f.state.physics=static_cast<std::uint8_t>(mode);const auto result=f.Run();
        const bool falling=mode==2u,other=mode==1u||mode==3u||mode==4u||mode==8u||mode==10u;
        Require(result.physicsMode==mode&&result.freshPhysicsMode==mode&&result.physicsReads==2u,"Pawn dispatch changed fresh Physics reads");
        Require(result.velocityReads==3u&&result.capturedVelocity==std::array<float,3u>{1,2,3},"Entry Velocity capture lost words");
        Require(result.dispatched==(falling||other)&&result.dispatchCalls==(falling||other?1u:0u),"APawn dispatch table differs from original");
        Require(f.dispatches.size()==(other?1u:0u)&&f.movementDeltas.size()==(falling||other?1u:0u),"APawn used an Actor-only movement mode");
        if(other)Require(static_cast<unsigned>(f.dispatches.front())==mode,"Selected Pawn dispatch mode changed");
        Require(result.rotationSkipped==(mode==10u)&&result.playerCasts==(mode==10u?0u:1u),"Fresh Spider gate changed rotation cast");
        Require(result.timerWrites==1u&&result.averageWrites==1u&&f.writes==2u&&result.steps==f.work,"Native timer stores or work escaped caller");
        Require(f.events[0]=="velocity0"&&f.events[1]=="velocity1"&&f.events[2]=="velocity2"&&f.events[3]=="physics", "Velocity capture did not precede dispatch");
    }
    Fixture fresh;fresh.state.physics=2u;fresh.state.velocity={Float(0x7fa12345u),-0.0f,42.0f};
    fresh.onMovement=[&]{fresh.state.physics=0u;fresh.state.desired[0]=65536;fresh.state.velocity={9,8,7};};
    const auto result=fresh.Run();
    Require(result.rotated&&result.rotationReads==1u&&result.desiredReads==1u&&result.rateReads==0u,"Fresh movement/rotation words were not used");
    Require(Bits(fresh.rotationVelocity[0])==0x7fa12345u&&Bits(fresh.rotationVelocity[1])==0x80000000u&&fresh.rotationVelocity[2]==42,
        "Pawn rotation lost entry raw Velocity or used post-move Velocity");
    Fixture spider;spider.state.physics=2u;spider.onMovement=[&]{spider.state.physics=10u;};
    Require(spider.Run().rotationSkipped,"Rotation gate used stale initial Physics");
    Fixture player;player.state.player=true;player.state.desired={4,5,6};
    Require(player.Run().rotated&&player.events[5]=="player"&&player.rotationDeltas.size()==1u,"Actual PlayerPawn failed unconditional rotation");
    for(unsigned axis=0u;axis<3u;++axis)for(auto word:std::array<std::int32_t,5u>{1,-1,65536,-65536,std::numeric_limits<std::int32_t>::min()}){
        Fixture f;f.state.desired[axis]=word;const auto r=f.Run();
        Require(r.rotated&&r.rotationReads==axis+1u&&r.desiredReads==axis+1u&&!r.rateReads,"Raw full-width Rotator mismatch normalized or reordered");
    }
    for(auto rate:std::array<std::int32_t,7u>{0,-1,1,65536,-65536,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()}){
        Fixture f;f.state.rate={1,2,rate};const auto r=f.Run();
        Require(r.rotated==(rate>0)&&r.rateReads==1u&&r.rotationReads==3u,"Pawn rotation gate used Actor low16 rate rule");
    }
}

void TimersAndPending(){
    Fixture f;f.state.pending[1]=2u;f.state.pending[2]=3u;const auto r=f.Run();
    Require(f.state.timer==0.75f&&Bits(f.state.average)==Bits(0.45f),"Pawn native timer/average arithmetic changed");
    Require(f.posts==std::vector<std::pair<Id,Id>>{{2u,1u}}&&f.pointerWrites==std::vector<std::pair<Id,Id>>{{1u,3u},{2u,0u}},"Pawn pending packet/store order changed");
    Require(r.retained==2u&&r.node==2u&&r.next==3u&&r.pendingReads==3u&&r.processedPendingTouch,"Pawn pending traversal changed");
    Require(f.events==std::vector<std::string>{"velocity0","velocity1","velocity2","physics","physics","player",
        "rotation0","desired0","rotation1","desired1","rotation2","desired2","roll-rate","timer","pending1",
        "write-timer","average","write-average","post","pending1","pending2","write-pending","write-pending"},"Pawn exact source read/store schedule changed");
    Fixture captured;captured.state.pending[1]=2u;captured.state.pending[3]=4u;
    captured.onTimer=[&]{captured.state.pending[1]=3u;captured.state.average=1.0f;};
    const auto c=captured.Run();
    Require(c.retained==2u&&c.node==3u&&c.next==4u&&captured.posts.front().first==2u&&Bits(captured.state.average)==Bits(0.85f),
        "Pending retained after timer store, or average read before it");
    Fixture nullRetained;nullRetained.onAverage=[&]{nullRetained.state.pending[1]=2u;};
    Require(!nullRetained.Run().postTouchCalls&&nullRetained.state.pending[1]==2u,"Fresh timer-created edge replaced captured null");
    Fixture cleared;cleared.state.pending[1]=2u;cleared.onPost=[&]{cleared.Pending(1u,0u);};
    const auto n=cleared.Run();
    Require(n.callbackClearedPending&&!n.processedPendingTouch&&!n.pendingWrites&&cleared.state.pending[1]==0u,"Pawn callback-null wrongly refused or dereferenced");
    Fixture replaced;replaced.state.pending[1]=2u;replaced.state.pending[2]=7u;replaced.state.pending[3]=4u;
    replaced.onPost=[&]{replaced.Pending(1u,3u);};const auto p=replaced.Run();
    Require(p.retained==2u&&p.node==3u&&p.next==4u&&replaced.state.pending[2]==7u&&replaced.state.pending[3]==0u,"Pawn callback changed edge but stale retained node unlinked");
    Fixture alias;alias.state.pending[1]=1u;Require(alias.Run().processedPendingTouch&&alias.state.pending[1]==0u&&
        alias.pointerWrites==std::vector<std::pair<Id,Id>>{{1u,1u},{1u,0u}},"Pending source alias lost ordered stores");
    Fixture heldTimer;heldTimer.onPending=[&]{heldTimer.state.timer=9.0f;};heldTimer.Run();
    Require(heldTimer.state.timer==0.75f,"MoveTimer was reloaded after PendingTouch capture");
    Fixture zero;zero.state.timer=0.0f;zero.state.average=0.0f;
    Require(zero.Run(0).timerWrites==1u&&zero.writes==2u,"Same-bit inactive native stores were optimized away");
}

void Failures(){
    Refuses([]{QuestVr::PerformOriginalPawnPhysics(0u,0.0f,{});},"Null Pawn accepted");
    for(unsigned missing=0u;missing<13u;++missing){
        Fixture f;auto q=f.Query();
        switch(missing){
        case 0:q.work={};break;case 1:q.velocity={};break;case 2:q.physics={};break;
        case 3:q.isPlayerPawnClass={};break;case 4:q.rotation={};break;case 5:q.desiredRotation={};break;
        case 6:q.rotationRateRoll={};break;case 7:q.moveTimer={};break;case 8:q.pendingTouch={};break;
        case 9:q.writeMoveTimer={};break;case 10:q.averagePhysicsTime={};break;case 11:q.writeAveragePhysicsTime={};break;
        case 12:f.state.desired[0]=1;q.physicsRotation={};break;}
        Refuses([&]{f.Direct(0.25f,q);},"Missing reached provider succeeded");
        if(missing==0u)Require(f.events.empty(),"Missing shared work still ran providers");
    }
    for(unsigned mode:{1u,2u,3u,4u,8u,10u}){
        Fixture f;f.state.physics=static_cast<std::uint8_t>(mode);auto q=f.Query();q.falling={};q.otherPhysicsBlock={};
        Refuses([&]{f.Direct(0.25f,q);},"Reached unimplemented Pawn movement silently succeeded");
        Require(!f.writes,"Unimplemented movement continued into timer stores");
    }
    Fixture lazy;lazy.state.physics=0u;auto sparse=lazy.Query();sparse.falling={};sparse.otherPhysicsBlock={};
    sparse.physicsRotation={};sparse.postTouch={};sparse.writePendingTouch={};Require(!lazy.Direct(0.25f,sparse).dispatched,"Inactive missing providers became mandatory");
    const auto configure=[](Fixture& f){f.state.physics=2u;f.state.pending[1]=2u;f.state.pending[3]=4u;
        f.onMovement=[&f]{f.state.physics=0u;f.state.desired[0]=1;};f.onRotation=[&f]{f.state.velocity={6,7,8};};
        f.onPost=[&f]{f.Pending(1u,3u);};};
    Fixture baseline;configure(baseline);const auto complete=baseline.Run();
    for(std::size_t cut=0u;cut<complete.steps;++cut){Fixture f;configure(f);f.limit=cut;const auto before=f.state;
        Refuses([&]{f.Run();},"Shared work cutoff silently succeeded");Require(f.state==before,"Work failure escaped caller rollback");}
    for(std::size_t cut=0u;cut<baseline.writes;++cut){Fixture f;configure(f);f.writeLimit=cut;const auto before=f.state;
        Refuses([&]{f.Run();},"Write cutoff silently succeeded");Require(f.state==before,"Write failure escaped caller rollback");}
    Fixture direct;direct.state.pending[1]=2u;direct.onPost=[&]{direct.Pending(1u,0u);throw std::runtime_error("late callback");};
    Refuses([&]{direct.Direct(0.25f,direct.Query());},"Callback failure was swallowed");
    Require(direct.state.timer==0.75f&&direct.state.pending[1]==0u,"Helper introduced forbidden private rollback");
}

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
struct Precision {
    unsigned short saved{},selected{};
    Precision(){static_assert(std::numeric_limits<long double>::digits==64,"Pawn oracle requires x87 PC64");
        asm volatile("fnstcw %0":"=m"(saved));selected=static_cast<unsigned short>((saved&~0x0f00u)|0x033fu);
        asm volatile("fnclex; fldcw %0"::"m"(selected):"memory");}
    ~Precision(){asm volatile("fnclex; fldcw %0"::"m"(saved):"memory");}
};
std::array<float,2u> Oracle(float timer,float average,float delta){
    std::array<float,2u> out{};const double oldWeight=0.8,newWeight=0.2;
    asm volatile("flds %[timer]; fsubs %[delta]; fstps %[timerOut]; flds %[average]; fmull %[oldWeight]; "
        "flds %[delta]; fmull %[newWeight]; faddp; fstps %[averageOut]":
        [timerOut]"=m"(out[0]),[averageOut]"=m"(out[1]):[timer]"m"(timer),[average]"m"(average),[delta]"m"(delta),
        [oldWeight]"m"(oldWeight),[newWeight]"m"(newWeight):"st","memory");
    return out;
}
void ArithmeticOracle(){Precision precision;
    const auto compare=[](float timer,float average,float delta){const auto expected=Oracle(timer,average,delta);
        Fixture f;f.state.timer=timer;f.state.average=average;const auto result=f.Run(delta);++oracleCases;
        Require(Bits(f.state.timer)==Bits(expected[0])&&Bits(f.state.average)==Bits(expected[1]),"Pawn timer/average differs from independent x87 source schedule");
        Require(result.timerWrites==1u&&result.averageWrites==1u&&f.writes==2u,"Exceptional arithmetic lost unconditional native stores");};
    const std::array<std::uint32_t,14u> words{0u,0x80000000u,1u,0x807fffffu,0x3dcccccd,0x3f800000,0xbf800000,
        0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fa12345u,0x7fc12345u,0xffc54321u};
    for(auto a:words)for(auto b:words)for(auto c:words)compare(Float(a),Float(b),Float(c));
    std::uint64_t seed=0x103c9c20103c9c4eull;
    const auto random=[&]{seed^=seed<<13u;seed^=seed>>7u;seed^=seed<<17u;return static_cast<std::uint32_t>(seed);};
    for(unsigned i=0u;i<32768u;++i){const auto timer=Float(random()),average=Float(random()),delta=Float(random());compare(timer,average,delta);}
}
#else
void ArithmeticOracle(){throw std::runtime_error("Pawn physics acceptance requires independent GNU x87 PC64, no skipped oracle");}
#endif
} // namespace

int main(){try{DispatchAndFreshRotation();TimersAndPending();Failures();ArithmeticOracle();
    std::cout<<"Original APawn performPhysics: "<<checks<<" checks / "<<refusals<<" refusals / "<<oracleCases
        <<" independent x87 cases; outer coordinator only, NOT completed Pawn falling/Walking or playable Training\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
