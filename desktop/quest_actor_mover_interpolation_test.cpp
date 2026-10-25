#include "quest_actor_mover_interpolation.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Vec=std::array<float,3u>;
using Rot=std::array<std::int32_t,3u>;
using Queries=QuestVr::OriginalMoverInterpolationQueries;
std::size_t checks{};
void Require(bool value,const char* text){if(!value)throw std::logic_error(text);++checks;}
std::uint32_t Bits(float x){return std::bit_cast<std::uint32_t>(x);}
std::int32_t Signed(std::uint32_t x){return std::bit_cast<std::int32_t>(x);}
[[maybe_unused]] std::int32_t Add(std::int32_t a,std::int32_t b){return Signed(std::uint32_t(a)+std::uint32_t(b));}
[[maybe_unused]] std::int32_t Sub(std::int32_t a,std::int32_t b){return Signed(std::uint32_t(a)-std::uint32_t(b));}
struct Stop final : std::exception {};

struct State {
    bool mover{true},interpolating{true};std::uint8_t key{2u},glide{};
    float alpha{0.25f},rate{1.0f};
    std::array<Vec,8u> keyPos{};std::array<Rot,8u> keyRot{};
    Vec basePos{10,20,30},oldPos{5,6,7},location{1,2,3};
    Rot baseRot{100,200,300},oldRot{10,20,30};
    unsigned events{},effects{};
    bool operator==(const State&)const=default;
};
struct Packet {Vec delta;Rot rotation;};
struct Fixture {
    State state;std::vector<std::string> order;std::vector<Packet> moves;
    std::vector<unsigned> positionKeys,rotationKeys;
    std::function<void()> onMove,onEnd;
    std::size_t work{},limit{100000u};unsigned keyReads{},alphaReads{};
    std::int32_t result{1};bool applyMove{true};
    Fixture(){state.keyPos[2u]={90,80,70};state.keyRot[2u]={1000,2000,3000};}
    static void Actor(Id actor){Require(actor==1u,"MovingBrush changed actor identity");}
    void Read(const char* field,unsigned channel){order.push_back(std::string(field)+std::to_string(channel));}
    Queries Get(){
        Queries q;q.work=[&]{if(work++>=limit)throw Stop{};};
        q.isMoverClass=[&](Id a){Actor(a);order.push_back("class");return state.mover;};
        q.keyNum=[&](Id a){Actor(a);++keyReads;order.push_back("key");return state.key;};
        q.interpolating=[&](Id a){Actor(a);order.push_back("flag");return state.interpolating;};
        q.physRate=[&](Id a){Actor(a);order.push_back("rate");return state.rate;};
        q.physAlpha=[&](Id a){Actor(a);++alphaReads;order.push_back("alpha");return state.alpha;};
        q.glideType=[&](Id a){Actor(a);order.push_back("glide");return state.glide;};
        q.keyRot=[&](Id a,unsigned k,unsigned c){Actor(a);Read("kr",c);rotationKeys.push_back(k);
            return k==8u?std::bit_cast<std::int32_t>(state.basePos.at(c)):state.keyRot.at(k).at(c);};
        q.baseRot=[&](Id a,unsigned c){Actor(a);Read("br",c);return state.baseRot.at(c);};
        q.oldRot=[&](Id a,unsigned c){Actor(a);Read("or",c);return state.oldRot.at(c);};
        q.keyPos=[&](Id a,unsigned k,unsigned c){Actor(a);Read("kp",c);positionKeys.push_back(k);
            return k==8u?std::bit_cast<float>(state.keyRot[0u].at(c)):state.keyPos.at(k).at(c);};
        q.basePos=[&](Id a,unsigned c){Actor(a);Read("bp",c);return state.basePos.at(c);};
        q.oldPos=[&](Id a,unsigned c){Actor(a);Read("op",c);return state.oldPos.at(c);};
        q.location=[&](Id a,unsigned c){Actor(a);Read("loc",c);return state.location.at(c);};
        q.level=[&](Id a){Actor(a);order.push_back("level");return Id{9u};};
        q.moveActor=[&](Id level,Id a,PortableModelVec3 delta,Rot rotation,
            QuestVr::OriginalActorLineHit& hit,std::array<std::int32_t,4u> options){
            Actor(a);Require(level==9u,"MovingBrush changed captured native Level");
            Require(options==std::array<std::int32_t,4u>{},"MovingBrush changed four zero Move options");
            Require(hit.next==0u&&hit.actor==0u&&hit.primitive==0u&&hit.item==-1&&Bits(hit.time)==Bits(1.0f)&&
                Bits(hit.location.x)==0u&&Bits(hit.location.y)==0u&&Bits(hit.location.z)==0u&&
                Bits(hit.normal.x)==0u&&Bits(hit.normal.y)==0u&&Bits(hit.normal.z)==0u,"MovingBrush hit seed changed");
            order.push_back("move");moves.push_back({{delta.x,delta.y,delta.z},rotation});
            if(applyMove)for(unsigned c=0u;c<3u;++c)state.location[c]+=moves.back().delta[c];
            ++state.effects;if(onMove)onMove();return result;
        };
        q.writePhysAlpha=[&](Id a,float alpha){Actor(a);order.push_back("writeAlpha");state.alpha=alpha;};
        q.writeInterpolating=[&](Id a,bool value){Actor(a);order.push_back("writeFlag");state.interpolating=value;};
        q.interpolateEnd=[&](Id a){Actor(a);order.push_back("end");++state.events;if(onEnd)onEnd();};
        return q;
    }
    auto Run(float delta,const Queries& q){return QuestVr::InterpolateOriginalMover(1u,delta,q);}
    auto Run(float delta=0.25f){return Run(delta,Get());}
    auto Transaction(float delta,const Queries& q){const auto before=state;try{return Run(delta,q);}catch(...){state=before;throw;}}
};

void GatesAndOrder(){
    Fixture cast;cast.state.mover=false;auto sparse=Queries{};
    sparse.isMoverClass=[](Id){return false;};
    const auto nonMover=cast.Run(std::numeric_limits<float>::quiet_NaN(),sparse);
    Require(!nonMover.mover&&nonMover.steps==1u,"Failed AMover cast read Mover fields");
    Fixture idle;idle.state.interpolating=false;auto q=idle.Get();q.physAlpha={};q.physRate={};q.glideType={};
    Require(!idle.Run(std::numeric_limits<float>::quiet_NaN(),q).entered&&
        idle.order==std::vector<std::string>{"class","key","flag"},"Idle branch consumed arithmetic or changed entry order");
    for(float dt:{0.0f,-1.0f,std::numeric_limits<float>::quiet_NaN()}){
        Fixture f;q=f.Get();q.physAlpha={};q.physRate={};
        Require(!f.Run(dt,q).entered&&f.moves.empty(),"Nonpositive/unordered delta consumed active providers");
    }
    Fixture f;const auto result=f.Run();
    const std::vector<std::string> expected{"class","key","flag","rate","alpha","glide",
        "br2","br1","kr2","kr0","kr1","br0","or2","or0","or1","or2","or1","or0",
        "bp2","kp2","bp1","kp1","kp0","bp0","op2","op1","op0","op2","op1","op0",
        "level","loc0","loc1","loc2","move","writeAlpha","flag"};
    Require(f.order==expected,"Original MovingBrush field/provider order changed");
    Require(result.iterations==1u&&result.moveCalls==1u&&result.alphaWrites==1u&&result.eventCalls==0u&&
        f.keyReads==1u&&f.alphaReads==1u&&Bits(f.state.alpha)==Bits(0.5f),"Partial interpolation changed effects");
    for(std::uint8_t key:{std::uint8_t{0u},std::uint8_t{7u},std::uint8_t{8u},std::uint8_t{255u}}){
        Fixture k;k.state.key=key;k.Run();const auto clamped=std::min<unsigned>(key,8u);
        Require(k.positionKeys==std::vector<unsigned>(3u,clamped)&&k.rotationKeys==std::vector<unsigned>(3u,clamped)&&
            k.state.key==key,"KeyNum clamp wrote source or capped original index8 to7");
    }
}

void RemaindersAndCallbacks(){
    Fixture f;f.state.alpha=0.75f;
    f.onMove=[&]{f.state.alpha=0.125f;};
    f.onEnd=[&]{Require(!f.state.interpolating&&Bits(f.state.alpha)==Bits(1.0f),"InterpolateEnd ran before original alpha/flag stores");
        f.state.interpolating=true;f.state.alpha=0.0f;f.state.key=3u;f.state.oldPos=f.state.location;};
    const auto result=f.Run(0.5f);
    Require(result.iterations==2u&&result.moveCalls==2u&&result.eventCalls==1u&&f.keyReads==1u&&
        f.positionKeys==std::vector<unsigned>(6u,2u)&&f.rotationKeys==std::vector<unsigned>(6u,2u),
        "Callback remainder reselected KeyNum, stopped early, or failed to reenter");
    Require(Bits(f.state.alpha)==Bits(0.25f)&&f.state.interpolating&&f.state.events==1u,
        "Captured alpha failed to overwrite Move callback mutation or residual was lost");
    Fixture failure;failure.state.alpha=0.75f;failure.result=0;failure.applyMove=false;
    const auto failed=failure.Run(0.5f);
    Require(failed.iterations==2u&&failure.moves.size()==2u&&failed.alphaWrites==0u&&failed.eventCalls==0u&&
        Bits(failure.state.alpha)==Bits(0.75f)&&failure.state.effects==2u,"Move failure lost residual or fabricated successful stores/local undo");
    Fixture partial;partial.result=0;partial.applyMove=false;const auto p=partial.Run();
    Require(p.iterations==1u&&partial.moves.size()==1u&&partial.state.alpha==0.25f,"Failed partial move retried consumed delta");
    Fixture limited;limited.limit=29u;const auto before=limited.state;bool stopped{};
    try{limited.Transaction(0.5f,limited.Get());}catch(const Stop&){stopped=true;}
    Require(stopped&&limited.state==before,"Caller work exception did not propagate for complete-root rollback");
    Fixture event;event.state.alpha=0.75f;event.onEnd=[] {throw Stop{};};const auto old=event.state;stopped=false;
    try{event.Transaction(0.5f,event.Get());}catch(const Stop&){stopped=true;}
    Require(stopped&&event.state==old,"Selected event failure did not roll back all caller-owned movement/effects");
}

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
// Independent native x87 PC64 oracle. Each volatile operation is a separate
// instruction-precision boundary, not the production software arithmetic.
using Real=long double;
struct Precision {
    unsigned short prior{};
    Precision(){static_assert(std::numeric_limits<Real>::digits==64);asm volatile("fnstcw %0":"=m"(prior));
        const auto mode=static_cast<unsigned short>((prior&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
    ~Precision(){asm volatile("fldcw %0"::"m"(prior));}
};
Real A(Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;}
Real S(Real a,Real b){volatile Real x=a,y=b,z=x-y;return z;}
Real M(Real a,Real b){volatile Real x=a,y=b,z=x*y;return z;}
Real D(Real a,Real b){volatile Real x=a,y=b,z=x/y;return z;}
float F(Real a){volatile float result=static_cast<float>(a);return result;}
struct Oracle {float remaining{},alpha{},glide{};Packet move;};
Oracle Evaluate(const State& s,float dt){
    Oracle out;Real a=A(M(dt,s.rate),s.alpha);out.alpha=F(a);
    if(a>1.0L){out.remaining=F(M(D(S(a,1.0L),S(a,s.alpha)),dt));a=1.0L;out.alpha=1.0f;}
    if(s.glide==1u){const Real square=M(a,a),triple=M(square,3.0L),cube=M(a,square);out.glide=F(S(triple,A(cube,cube)));}
    else out.glide=F(a);
    const auto key=std::min<unsigned>(s.key,8u);
    for(unsigned c=0u;c<3u;++c){
        const auto keyRotation=key==8u?std::bit_cast<std::int32_t>(s.basePos[c]):s.keyRot[key][c];
        const auto keyPosition=key==8u?std::bit_cast<float>(s.keyRot[0u][c]):s.keyPos[key][c];
        const auto delta=Sub(Add(keyRotation,s.baseRot[c]),s.oldRot[c]);
        const auto step=static_cast<std::int64_t>(M(delta,out.glide));
        out.move.rotation[c]=Add(s.oldRot[c],Signed(static_cast<std::uint32_t>(step)));
        const float target=F(A(keyPosition,s.basePos[c]));
        const float diff=F(S(target,s.oldPos[c]));const float scaled=F(M(diff,out.glide));
        const float position=F(A(scaled,s.oldPos[c]));out.move.delta[c]=F(S(position,s.location[c]));
    }
    return out;
}
void Compare(Fixture& f,float dt){
    const auto expected=Evaluate(f.state,dt);f.onMove=[&]{f.state.interpolating=false;};
    const auto actual=f.Run(dt);
    Require(Bits(actual.remaining)==Bits(expected.remaining)&&Bits(actual.alpha)==Bits(expected.alpha)&&
        Bits(actual.glide)==Bits(expected.glide),"Active alpha/residual/glide differs from independent x87 PC64 oracle");
    Require(f.moves.size()==1u&&f.moves[0u].rotation==expected.move.rotation,"Rotation wrap/FILD/_ftol differs from independent oracle");
    for(unsigned c=0u;c<3u;++c)Require(Bits(f.moves[0u].delta[c])==Bits(expected.move.delta[c]),
        "MovingBrush vector stage spill/subtraction differs from independent x87 oracle");
}
void ArithmeticOracle(){
    Precision precision;
    Fixture below;below.state.alpha=std::bit_cast<float>(0x3f7fffffu);below.state.rate=1.0f;
    Compare(below,std::ldexp(1.0f,-25));
    Require(below.state.events==1u,"Captured Float1 below retained1 did not complete");
    Fixture above;above.state.alpha=1.0f;Compare(above,std::ldexp(1.0f,-26));
    Require(above.state.events==1u,"Captured Float1 above retained1 did not complete");
    Fixture adjacent;adjacent.state.key=255u;adjacent.state.keyRot[0u]={Signed(Bits(2.0f)),Signed(Bits(-3.0f)),Signed(Bits(4.0f))};
    Compare(adjacent,0.25f);
    std::uint32_t random=0x8ab7d321u;unsigned glideDiscriminators{};
    const auto next=[&]{random^=random<<13u;random^=random>>17u;random^=random<<5u;return random;};
    for(unsigned n=0u;n<5000u;++n){
        Fixture f;f.state.alpha=std::bit_cast<float>(0x3e000000u+(next()%0x01800000u));
        f.state.rate=std::bit_cast<float>(0x3d800000u+(next()%0x01800000u));
        const float dt=std::bit_cast<float>(0x3d000000u+(next()%0x02000000u));
        f.state.glide=static_cast<std::uint8_t>(n%3u);
        for(unsigned c=0u;c<3u;++c){
            f.state.baseRot[c]=Signed(next());f.state.keyRot[2u][c]=Signed(next());f.state.oldRot[c]=Signed(next());
            f.state.keyPos[2u][c]=static_cast<float>(static_cast<std::int32_t>(next()%200000u)-100000)*0.125f;
            f.state.basePos[c]=static_cast<float>(static_cast<std::int32_t>(next()%200000u)-100000)*0.25f;
            f.state.oldPos[c]=static_cast<float>(static_cast<std::int32_t>(next()%200000u)-100000)*0.5f;
            f.state.location[c]=static_cast<float>(static_cast<std::int32_t>(next()%200000u)-100000)*0.0625f;
        }
        if(f.state.glide==1u){const auto oracle=Evaluate(f.state,dt);const Real spilled=oracle.alpha;
            const Real square=M(spilled,spilled),cube=M(spilled,square);
            if(Bits(oracle.glide)!=Bits(F(S(M(square,3.0L),A(cube,cube)))))++glideDiscriminators;}
        Compare(f,dt);
    }
    Require(glideDiscriminators>0u,"Oracle corpus lost retained-alpha versus Float-reload glide discriminator");
}
#else
void ArithmeticOracle(){throw std::runtime_error("Independent MovingBrush oracle requires GNU x87 PC64 long double");}
#endif
} // namespace

int main(){try{GatesAndOrder();RemaindersAndCallbacks();ArithmeticOracle();
    std::cout<<"Original MovingBrush interpolation: "<<checks<<" checks; entry/read-order, key8, residual/callback/failure, caller rollback and independent x87 PC64 oracle passed\n";
    return 0;}catch(const std::exception& error){std::cerr<<"Original MovingBrush interpolation test failed: "<<error.what()<<'\n';return 1;}}
