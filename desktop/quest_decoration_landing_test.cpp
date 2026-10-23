#include "quest_decoration_landing.h"

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
using QuestVr::OriginalActorLineHit;
using QuestVr::OriginalDecorationLandingQueries;
std::size_t checks{},refusals{};
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
bool Equal(const PortableModelVec3& a,const PortableModelVec3& b){return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
struct State {
    PortableModelVec3 location{100.0f,200.0f,300.0f},zoneVelocity{},velocity{1.0f,2.0f,-100.0f};
    float radius{34.0f},height{24.0f};
    std::int32_t counter{};std::uint8_t physics{2u};std::uintptr_t base{4u};bool deleted{};
    bool operator==(const State& other)const {
        return Equal(location,other.location)&&Equal(zoneVelocity,other.zoneVelocity)&&Equal(velocity,other.velocity)&&
            Bits(radius)==Bits(other.radius)&&Bits(height)==Bits(other.height)&&counter==other.counter&&
            physics==other.physics&&base==other.base&&deleted==other.deleted;
    }
};
struct Trace {
    PortableModelVec3 end{},start{},extent{};std::uint32_t flags{};OriginalActorLineHit before{};
};
struct Fixture {
    State state;
    bool pawnFlag{},pawnClass{},decorationClass{true},carcassClass{},bounce{};
    std::uintptr_t firstHit{42u};std::int32_t firstClear{1};
    std::array<std::int32_t,4u> clear{1,1,1,1};
    std::vector<std::string> order;std::vector<Trace> traces;
    std::function<void(std::size_t)> onTrace;std::function<void()> onLanded,onSetPhysics;
    std::size_t work{},workLimit{1024u},writes{},writeLimit{1024u},zoneReads{};
    OriginalActorLineHit* packet{};
    void Write(){if(writes>=writeLimit)throw std::runtime_error("write");++writes;}
    OriginalDecorationLandingQueries Queries() {
        OriginalDecorationLandingQueries q;
        const auto actor=[](std::uintptr_t self){Require(self==1u,"Changed original landing Actor identity");};
        q.work=[&]{if(work>=workLimit)throw std::runtime_error("work");++work;};
        q.pawnFlag=[&,actor](auto self){actor(self);order.push_back("pawn-flag");return pawnFlag;};
        q.isPawnClass=[&,actor](auto self){actor(self);order.push_back("pawn-class");return pawnClass;};
        q.isDecorationClass=[&,actor](auto self){actor(self);order.push_back("decoration-class");return decorationClass;};
        q.isCarcassClass=[&,actor](auto self){actor(self);order.push_back("carcass-class");return carcassClass;};
        q.zone=[&,actor](auto self){actor(self);order.push_back("zone");++zoneReads;return std::uintptr_t{3u};};
        q.zoneBounceVelocity=[&](auto zone){Require(zone==3u,"Changed actual stored Zone identity");order.push_back("zone-bounce");return bounce;};
        q.zoneVelocity=[&](auto zone,unsigned channel){Require(zone==3u&&channel<3u,"Changed zone vector receiver/channel");
            order.push_back("zone-"+std::to_string(channel));return channel==0u?state.zoneVelocity.x:channel==1u?state.zoneVelocity.y:state.zoneVelocity.z;};
        q.level=[&,actor](auto self){actor(self);order.push_back("level");return std::uintptr_t{2u};};
        q.location=[&,actor](auto self,unsigned channel){actor(self);Require(channel<3u,"Invalid location channel");
            order.push_back("location-"+std::to_string(channel));return channel==0u?state.location.x:channel==1u?state.location.y:state.location.z;};
        q.velocity=[&,actor](auto self,unsigned channel){actor(self);Require(channel==2u,"Landing read inactive old Velocity component");order.push_back("velocity-z");return state.velocity.z;};
        q.radius=[&,actor](auto self){actor(self);order.push_back("radius");return state.radius;};
        q.height=[&,actor](auto self){actor(self);order.push_back("height");return state.height;};
        q.numLandings=[&,actor](auto self){actor(self);order.push_back("counter-read");return state.counter;};
        q.writeNumLandings=[&,actor](auto self,std::int32_t value){actor(self);Write();order.push_back("counter-write");state.counter=value;};
        q.writeVelocityWord=[&,actor](auto self,unsigned channel,std::uint32_t word){actor(self);Require(channel<3u,"Invalid landing write channel");Write();
            order.push_back("velocity-write-"+std::to_string(channel));
            (channel==0u?state.velocity.x:channel==1u?state.velocity.y:state.velocity.z)=std::bit_cast<float>(word);};
        q.singleLineCheck=[&,actor](auto level,OriginalActorLineHit& hit,auto self,const auto& end,const auto& start,
            std::uint32_t flags,const auto& extent,std::int32_t extra) {
            actor(self);Require(level==2u&&extra==0,"Lost actual Level/source or extra-zero packet");
            if(!packet)packet=&hit;
            Require(packet==&hit,"Recovery created a different hit packet");
            const auto index=traces.size();Require(index<5u,"Landing exceeded original five-query bound");
            traces.push_back({end,start,extent,flags,hit});order.push_back("trace-"+std::to_string(index));
            if(!index) {
                Require(hit.next==0u&&hit.actor==0u&&hit.primitive==0u&&hit.time==1.0f&&hit.item==-1&&
                    Equal(hit.location,{})&&Equal(hit.normal,{}),"Initial original native hit packet changed");
                hit.actor=firstHit;
            } else {
                Require(Bits(hit.normal.x)==Bits(static_cast<float>(index)),"Recovery reset previous hit writes");
                hit.actor=99u; // Must not suppress B/C/D after a recovery query.
            }
            hit.normal.x=static_cast<float>(index+1u);
            if(onTrace)onTrace(index);
            return index?clear[index-1u]:firstClear;
        };
        q.landed=[&,actor](auto self,const auto& normal){actor(self);Require(Equal(normal,{0.25f,-0.5f,1.0f}),"Selected Landed lost supplied normal");
            order.push_back("landed");if(onLanded)onLanded();};
        q.physics=[&,actor](auto self){actor(self);order.push_back("physics");return state.physics;};
        q.setPhysics=[&,actor](auto self,std::uint8_t mode,auto floor){actor(self);Require(mode==0u&&floor==7u,"Landed used fit hit instead of original supplied floor");
            order.push_back("set-physics");Write();state.physics=mode;state.base=floor;if(onSetPhysics)onSetPhysics();};
        return q;
    }
    auto Direct(const OriginalDecorationLandingQueries& q){return QuestVr::ProcessOriginalDecorationLanded(1u,{0.25f,-0.5f,1.0f},7u,q);}
    auto Run(){const auto before=state;try{return Direct(Queries());}catch(...){state=before;throw;}}
};
void BasicContracts() {
    {
        Fixture f;const auto result=f.Run();
        Require(result.traceCalls==1u&&result.counterWrites==1u&&result.landedCalls==1u&&result.physicsReads==1u&&result.setPhysicsCalls==1u,
            "Ordinary support landing lost event/reset/fresh-mode/setter sequence");
        Require(f.state.counter==0&&f.state.physics==0u&&f.state.base==7u,"Ordinary landing did not use supplied support setter");
        Require(f.traces[0].flags==0x37u&&Equal(f.traces[0].extent,{})&&Equal(f.traces[0].start,{100.0f,200.0f,280.8f})&&
            Equal(f.traces[0].end,{100.0f,200.0f,234.0f}),"Initial fit flags/extent/Float endpoint stores changed");
        Require(f.order[f.order.size()-4u]=="counter-write"&&f.order[f.order.size()-3u]=="landed"&&
            f.order[f.order.size()-2u]=="physics"&&f.order.back()=="set-physics","Normal landing reordered reset, selected callback and mode reread");
        Require(result.steps==f.work,"Landing did not borrow one caller-root work ledger");
    }
    for(const auto count:{5,6,std::numeric_limits<std::int32_t>::max()}) {
        Fixture f;f.state.counter=count;f.carcassClass=true;auto q=f.Queries();q.level={};q.singleLineCheck={};q.height={};q.radius={};q.location={};q.isCarcassClass={};
        const auto result=f.Direct(q);Require(result.traceCalls==0u&&result.counterWrites==1u&&f.state.counter==0&&result.landedCalls==1u,
            "Counter>=5 reached inactive fit/Carcass providers or skipped original reset");
    }
    for(const auto mode:{0u,1u,5u,2u,255u}) {
        Fixture f;f.onLanded=[&]{f.state.physics=static_cast<std::uint8_t>(mode);f.state.base=19u;f.state.deleted=true;};
        const auto result=f.Run();Require(result.physicsReads==1u&&result.setPhysicsCalls==(mode==2u?1u:0u)&&
            f.state.physics==(mode==2u?0u:mode)&&f.state.base==(mode==2u?7u:19u)&&f.state.deleted,
            "Post-Landed did not preserve callback mode/deletion/Base or reread live Physics");
    }
    {
        Fixture f;f.onSetPhysics=[&]{f.state.physics=5u;f.state.base=23u;};const auto result=f.Run();
        Require(result.setPhysicsCalls==1u&&f.state.physics==5u&&f.state.base==23u,"Landing overwrote reentrant SetPhysics/support effects");
    }
    {
        Fixture f;PortableModelVec3 normal{0.25f,-0.5f,1.0f};
        f.onTrace=[&](std::size_t){normal={9,8,7};};
        const auto result=QuestVr::ProcessOriginalDecorationLanded(1u,normal,7u,f.Queries());
        Require(result.landedCalls==1u&&Equal(normal,{9,8,7}),
            "Landing did not capture the original by-value normal before query reentry");
    }
    for(const auto clear:{std::numeric_limits<std::int32_t>::min(),0,1,27}) {
        Fixture f;f.firstClear=clear;Require(f.Run().traceCalls==1u,"Initial query used its inactive INT instead of hit.Actor");
    }
    {
        Fixture f;f.pawnFlag=true;auto q=f.Queries();q.zone={};q.zoneBounceVelocity={};q.zoneVelocity={};
        Require(f.Direct(q).landedCalls==1u&&f.zoneReads==0u,"bIsPawn flag did not bypass entire zone shortcut before actual class test");
    }
}
void RecoveryContracts() {
    for(unsigned mask=0u;mask<16u;++mask) {
        Fixture f;f.firstHit=0u;
        for(unsigned i=0u;i<4u;++i)f.clear[i]=static_cast<std::int32_t>((mask>>i)&1u);
        const auto a=f.clear[0],b=f.clear[1],c=f.clear[2],d=f.clear[3];
        const bool recover=a+b+c+d>1&&a+c!=0&&b+d!=0;const auto result=f.Run();
        Require(result.traceCalls==5u&&result.clear==f.clear&&result.recovery==recover&&result.landedCalls==(recover?0u:1u),
            "Four original clear INTs were replaced by hit packet, early exit or any-support predicate");
        for(std::size_t i=0u;i<4u;++i) {
            constexpr std::array<PortableModelVec3,4u> offsets{{{17,17,0},{-17,17,0},{-17,-17,0},{17,-17,0}}};
            const auto& t=f.traces[i+1u];const auto& o=offsets[i];
            Require(t.flags==0x17u&&Equal(t.extent,{17,17,24})&&Equal(t.start,{100+o.x,200+o.y,300})&&
                Equal(t.end,{100+o.x,200+o.y,292}),"Recovery corner order/vertical eight sweep or retained extent changed");
        }
        if(recover)Require(f.state.counter==1&&f.state.physics==2u&&f.state.base==4u&&
            Equal(f.state.velocity,{static_cast<float>((a+d-b-c)*128),static_cast<float>((a+b-c-d)*128),64}),
            "Recovery normalized direction, changed Physics/Base or called ordinary landing");
    }
    for(const float speed:{0.0f,-10.0f,-30.0f,-50.0f,-64.0f,-100.0f,80.0f}) {
        Fixture f;f.firstHit=0u;f.clear={1,1,1,0};f.state.velocity.z=speed;const auto result=f.Run();
        const float down=-speed;const float selected=down<30?30:down<64?down:64;
        Require(result.recovery&&Equal(f.state.velocity,{-2*selected,2*selected,selected}),"Recovery original nested speed comparison/store changed");
    }
    {
        Fixture f;f.firstHit=0u;f.clear={1,1,1,0};f.state.counter=-9;
        f.onTrace=[&](std::size_t i){if(i==4u)f.state.counter=std::numeric_limits<std::int32_t>::max();};
        const auto result=f.Run();Require(result.recovery&&f.state.counter==std::numeric_limits<std::int32_t>::min(),
            "Recovery used stale count or signed-overflow UB instead of live wrapped INT increment");
    }
    {
        Fixture f;f.firstHit=0u;f.clear={std::numeric_limits<std::int32_t>::max(),1,1,1};
        Require(!f.Run().recovery,"Wrapped signed total was replaced by unbounded sum");
    }
    {
        Fixture f;f.firstHit=0u;f.clear={std::numeric_limits<std::int32_t>::max(),1,-std::numeric_limits<std::int32_t>::max(),1};
        Require(!f.Run().recovery,"Recovery ignored original wrapped opposite-corner sum");
    }
    {
        Fixture f;f.firstHit=0u;f.clear={1,1,1,0};
        f.onTrace=[&](std::size_t i){if(i==1u){f.state.location={10,20,30};f.state.radius=50;f.state.height=60;}};
        const auto result=f.Run();Require(result.recovery&&Equal(f.traces[2].start,{-15,45,30})&&Equal(f.traces[2].end,{-15,45,22})&&
            Equal(f.traces[2].extent,{17,17,24}),"Recovery cached live radius/location or reread retained extent after callback");
    }
    {
        Fixture f;f.firstHit=0u;f.clear={1,1,1,0};f.carcassClass=true;auto q=f.Queries();q.isCarcassClass={};q.landed={};q.physics={};q.setPhysics={};
        Require(f.Direct(q).recovery,"Recovery reached inactive later Carcass/event/mode providers");
    }
}
void BounceAndNumericContracts() {
    using namespace QuestVr::DecorationLandingDetail;
    const float nan=std::bit_cast<float>(std::uint32_t{0x7fc12345u});
    const float signaling=std::bit_cast<float>(std::uint32_t{0xff812345u});
    for(const float value:{0.0f,-0.0f,nan,signaling})Require(ZeroOrUnordered(value),"Original C3 equality lost signed zero/unordered input");
    for(const float value:{1.0f,-1.0f,std::numeric_limits<float>::infinity()})Require(!ZeroOrUnordered(value),"Original C3 equality accepted ordered nonzero");
    Require(LessOrUnordered(nan,30)&&LessOrUnordered(30,signaling)&&!LessOrUnordered(30,30)&&LessOrUnordered(-1,30),
        "Original C0 less-or-unordered comparison was changed to ordered clamp");
    {
        Fixture f;f.bounce=true;f.state.zoneVelocity={1,-0.0f,3};f.state.velocity={nan,signaling,nan};auto q=f.Queries();
        q.isPawnClass={};q.isDecorationClass={};q.isCarcassClass={};q.height={};q.radius={};q.velocity={};q.physics={};q.landed={};q.setPhysics={};
        const auto result=f.Direct(q);Require(result.zoneBounce&&result.traceCalls==0u&&result.counterWrites==0u&&result.velocityWords==3u&&
            f.zoneReads==3u&&Equal(f.state.velocity,{1,0.0f,83}),"Zone shortcut read old Velocity or bypassed positive-zero arithmetic/return");
        const std::vector<std::string> expected{"pawn-flag","zone","zone-bounce","zone","zone-0","zone","zone-2","zone-1","zone-0",
            "velocity-write-0","velocity-write-1","velocity-write-2"};
        Require(f.order==expected,"Zone pointer/component short circuit or Z/Y/X scratch then X/Y/Z store order changed");
    }
    {
        Fixture f;f.bounce=true;f.state.zoneVelocity={signaling,-0.0f,nan};f.state.counter=5;auto q=f.Queries();
        q.location={};q.height={};q.radius={};q.singleLineCheck={};const auto result=f.Direct(q);
        Require(!result.zoneBounce&&result.landedCalls==1u&&f.zoneReads==2u,"Unordered-only zone vector became a spurious bounce/arithmetic refusal");
    }
    {
        Fixture f;f.bounce=true;f.state.zoneVelocity={1,nan,3};const auto before=f.state;bool refused{};
        try{static_cast<void>(f.Run());}catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.writes==0u&&f.state==before,"Unsupported active NaN bounce arithmetic silently coerced or leaked writes");++refusals;
    }
    {
        Fixture f;f.firstHit=0u;f.clear={1,1,1,0};f.state.velocity.z=nan;const auto before=f.state;bool refused{};
        try{static_cast<void>(f.Run());}catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.writes==1u&&f.state==before,"Active NaN recovery arithmetic did not retain prefix for caller-root undo");++refusals;
    }
    {
        Fixture f;f.firstHit=0u;f.clear={1,1,1,0};
        f.state.radius=std::numeric_limits<float>::max();f.state.velocity.z=-std::numeric_limits<float>::max();
        const auto before=f.state;bool refused{};
        try{static_cast<void>(f.Run());}catch(const std::exception&){refused=true;}
        Require(refused&&f.writes==1u&&f.state==before,
            "Unsupported overflowing recovery arithmetic was coerced or escaped caller-root undo");++refusals;
    }
    Require(Bits(Add(0.0f,-0.0f))==0u&&Bits(Sub(-0.0f,0.0f))==0x80000000u&&Bits(Scale(-0.0f,0.5))==0x80000000u,
        "Binary32 stores lost original signed-zero arithmetic");
    Require(IntFloat(std::numeric_limits<std::int32_t>::max())==2147483648.0f&&IntFloat(std::numeric_limits<std::int32_t>::min())==-2147483648.0f,
        "Original FILD-to-binary32 rounded signed INT incorrectly");
}
void RefusalAndRootContracts() {
    for(unsigned kind=0u;kind<3u;++kind) {
        Fixture f;f.pawnClass=kind==0u;f.decorationClass=kind!=1u;f.carcassClass=kind==2u;
        const auto before=f.state;bool refused{};try{static_cast<void>(f.Run());}catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.state==before&&f.writes==0u&&f.traces.size()==(kind==2u?1u:0u),
            "Unsupported Pawn/non-Decoration/Carcass path was treated as complete ordinary landing");++refusals;
    }
    {
        Fixture f;f.state.counter=3;auto q=f.Queries();q.landed={};bool refused{};
        try{static_cast<void>(f.Direct(q));}catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.writes==1u&&f.state.counter==0&&f.state.physics==2u,"Missing selected Landed provider was hidden or coordinator locally undid prefix");++refusals;
    }
    for(const bool recovery:{false,true}) {
        Fixture full;full.firstHit=recovery?0u:42u;full.clear={1,1,1,0};const auto usage=full.Run();
        for(std::size_t limit=0u;limit<usage.steps;++limit) {
            Fixture f;f.firstHit=full.firstHit;f.clear=full.clear;f.workLimit=limit;const auto before=f.state;bool refused{};
            try{static_cast<void>(f.Run());}catch(const std::runtime_error& e){refused=std::string(e.what())=="work";}
            Require(refused&&f.state==before&&f.work==limit,"Shared-work prefix was reset, swallowed or escaped caller rollback");++refusals;
        }
        for(std::size_t limit=0u;limit<full.writes;++limit) {
            Fixture f;f.firstHit=full.firstHit;f.clear=full.clear;f.writeLimit=limit;const auto before=f.state;bool refused{};
            try{static_cast<void>(f.Run());}catch(const std::runtime_error& e){refused=std::string(e.what())=="write";}
            Require(refused&&f.state==before&&f.writes==limit,"Shared-write prefix was reset, swallowed or escaped caller rollback");++refusals;
        }
    }
    {
        Fixture f;f.onLanded=[&]{f.Write();f.state.counter=9;f.state.physics=5;f.state.base=23;throw std::runtime_error("callback");};
        const auto before=f.state;bool refused{};try{static_cast<void>(f.Run());}catch(const std::runtime_error&){refused=true;}
        Require(refused&&f.state==before&&f.writes==2u,"Selected callback failure did not propagate its whole pending prefix to root undo");++refusals;
    }
    {
        Fixture f;bool refused{};try{static_cast<void>(QuestVr::ProcessOriginalDecorationLanded(0u,{},0u,f.Queries()));}
        catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.work==0u&&f.writes==0u,"Null Actor reached actual field providers");++refusals;
    }
}
} // namespace

int main() {
    try {
        BasicContracts();RecoveryContracts();BounceAndNumericContracts();RefusalAndRootContracts();
        std::cout<<"Original Decoration landing coordinator: "<<checks<<" checks, "<<refusals
            <<" refusal controls. NOT integrated falling, world Tick or on-headset gravity.\n";return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
