#include "quest_actor_falling.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Vec=PortableModelVec3;
using Rotation=std::array<std::int32_t,3u>;
using QuestVr::OriginalActorFallingQueries;
using QuestVr::OriginalActorFallingStats;
using QuestVr::OriginalActorLineHit;
std::size_t checks{},refusals{};
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);++checks;}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
bool Equal(const Vec& a,const Vec& b){return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
float& Channel(Vec& v,unsigned channel){if(channel>2u)throw std::runtime_error("Invalid Vector channel");return channel==0u?v.x:channel==1u?v.y:v.z;}
float FloatAdd(float a,float b){return static_cast<float>(static_cast<double>(a)+static_cast<double>(b));}
float FloatMultiply(float a,float b){return static_cast<float>(static_cast<double>(a)*static_cast<double>(b));}
struct State {
    Vec location{},oldLocation{9,8,7},velocity{0,0,-16},acceleration{},gravity{0,0,-512},drift{};
    Rotation rotation{1,2,3};Id zone{3u};float terminal{10000.0f};std::int32_t counter{5};
    std::uint8_t zoneNumber{1u},physics{2u};bool teleported{true},deleted{},bounce{},water{},bobbing{},pawnFlag{};
    bool operator==(const State& other)const {
        return Equal(location,other.location)&&Equal(oldLocation,other.oldLocation)&&Equal(velocity,other.velocity)&&
            Equal(acceleration,other.acceleration)&&Equal(gravity,other.gravity)&&Equal(drift,other.drift)&&
            rotation==other.rotation&&zone==other.zone&&Bits(terminal)==Bits(other.terminal)&&counter==other.counter&&
            zoneNumber==other.zoneNumber&&physics==other.physics&&teleported==other.teleported&&deleted==other.deleted&&bounce==other.bounce&&
            water==other.water&&bobbing==other.bobbing&&pawnFlag==other.pawnFlag;
    }
};
struct Move {
    Vec delta{},before{},oldLocation{},velocity{},acceleration{};Rotation rotation{};OriginalActorLineHit hitBefore{};
};
struct Landing {Vec normal{},velocity{};Id floor{};float remaining{};std::int32_t iterations{};};
struct Wall {Vec normal{},velocity{};Id actor{};};
struct Fixture {
    State state;
    bool pawnClass{},playerPawnClass{},decorationClass{},hitPlayerPawn{};
    float hitTime{1.0f};Vec hitNormal{0,0,1};Id hitFloor{7u};
    std::vector<Move> moves;std::vector<Landing> landings;std::vector<Wall> walls;std::vector<std::string> order;
    std::function<void(std::size_t,OriginalActorLineHit&)> onMove;
    std::function<void()> onLanded,onOutOfWorld;
    std::function<void(Vec,Id)> onHitWall;
    std::size_t work{},workLimit{65536u},writes{},writeLimit{65536u},accelerationRestores{};
    OriginalActorLineHit* hitPacket{};
    void Write(){if(writes>=writeLimit)throw std::runtime_error("write");++writes;}
    void Store(Vec& v,unsigned channel,std::uint32_t word){Write();Channel(v,channel)=std::bit_cast<float>(word);}
    static void Actor(Id actor){Require(actor==1u,"Falling changed original source Actor identity");}
    OriginalActorFallingQueries Queries() {
        OriginalActorFallingQueries q;
        q.work=[&]{if(work>=workLimit)throw std::runtime_error("work");++work;};
        q.isPawnClass=[&](Id actor){Actor(actor);order.push_back("pawn-class");return pawnClass;};
        q.isPlayerPawnClass=[&](Id actor){Require(actor==1u||actor==hitFloor,"Falling changed actual PlayerPawn ancestry receiver");
            order.push_back(actor==1u?"player-class":"hit-player-class");return actor==1u?playerPawnClass:hitPlayerPawn;};
        q.isDecorationClass=[&](Id actor){Actor(actor);order.push_back("decoration-class");return decorationClass;};
        q.pawnFlag=[&](Id actor){Actor(actor);order.push_back("pawn-flag");return state.pawnFlag;};
        q.bounce=[&](Id actor){Actor(actor);order.push_back("bounce");return state.bounce;};
        q.justTeleported=[&](Id actor){Actor(actor);order.push_back("teleported");return state.teleported;};
        q.deleted=[&](Id actor){Actor(actor);order.push_back("deleted");return state.deleted;};
        q.bobbing=[&](Id actor){Actor(actor);order.push_back("bobbing");return state.bobbing;};
        q.zoneNumber=[&](Id actor){Actor(actor);order.push_back("zone-number");return state.zoneNumber;};
        q.zone=[&](Id actor){Actor(actor);order.push_back("zone");return state.zone;};
        q.level=[&](Id actor){Actor(actor);order.push_back("level");return Id{2u};};
        const auto zone=[&](Id id){Require(id==3u||id==4u,"Falling lost actual stored Zone identity");};
        q.zoneWater=[&,zone](Id id){zone(id);order.push_back("water");return state.water;};
        q.zoneGravity=[&,zone](Id id,unsigned channel){zone(id);order.push_back("gravity-"+std::to_string(channel));return Channel(state.gravity,channel);};
        q.zoneVelocity=[&,zone](Id id,unsigned channel){zone(id);order.push_back("drift-"+std::to_string(channel));return Channel(state.drift,channel);};
        q.terminal=[&,zone](Id id){zone(id);order.push_back("terminal");return state.terminal;};
        q.location=[&](Id actor,unsigned channel){Actor(actor);order.push_back("location-"+std::to_string(channel));return Channel(state.location,channel);};
        q.oldLocation=[&](Id actor,unsigned channel){Actor(actor);order.push_back("old-location-"+std::to_string(channel));return Channel(state.oldLocation,channel);};
        q.velocity=[&](Id actor,unsigned channel){Actor(actor);order.push_back("velocity-"+std::to_string(channel));return Channel(state.velocity,channel);};
        q.acceleration=[&](Id actor,unsigned channel){Actor(actor);order.push_back("acceleration-"+std::to_string(channel));return Channel(state.acceleration,channel);};
        q.writeOldLocationWord=[&](Id actor,unsigned channel,std::uint32_t word){Actor(actor);order.push_back("old-write-"+std::to_string(channel));Store(state.oldLocation,channel,word);};
        q.writeVelocityWord=[&](Id actor,unsigned channel,std::uint32_t word){Actor(actor);order.push_back("velocity-write-"+std::to_string(channel));Store(state.velocity,channel,word);};
        q.writeAccelerationWord=[&](Id actor,unsigned channel,std::uint32_t word){Actor(actor);order.push_back("acceleration-write-"+std::to_string(channel));
            Store(state.acceleration,channel,word);++accelerationRestores;};
        q.writeJustTeleported=[&](Id actor,bool value){Actor(actor);order.push_back("teleport-write");Write();state.teleported=value;};
        q.numLandings=[&](Id actor){Actor(actor);order.push_back("counter-read");return state.counter;};
        q.writeNumLandings=[&](Id actor,std::int32_t value){Actor(actor);order.push_back("counter-write");Write();state.counter=value;};
        q.currentRotation=[&](Id actor){Actor(actor);order.push_back("rotation");return state.rotation;};
        q.moveActor=[&](Id level,Id actor,Vec delta,Rotation rotation,OriginalActorLineHit& hit,std::array<std::int32_t,4u> options) {
            Actor(actor);Require(level==2u&&rotation==state.rotation&&options==std::array<std::int32_t,4u>{},
                "Falling sweep lost actual Level/Rotation or four-zero options");
            if(!hitPacket) {
                hitPacket=&hit;
                Require(hit.next==0u&&hit.actor==0u&&hit.primitive==0u&&hit.time==1.0f&&hit.item==-1&&
                    Equal(hit.location,{})&&Equal(hit.normal,{}),"Falling changed original initial hit packet");
            }
            Require(hitPacket==&hit,"Falling replaced the original shared native hit packet");
            const auto index=moves.size();moves.push_back({delta,state.location,state.oldLocation,state.velocity,state.acceleration,rotation,hit});
            order.push_back("move");hit.time=hitTime;hit.actor=hitTime<1.0f?hitFloor:0u;hit.normal=hitNormal;
            for(unsigned channel=0u;channel<3u;++channel) {
                const float distance=FloatMultiply(Channel(delta,channel),hitTime);
                const float value=FloatAdd(Channel(state.location,channel),distance);
                Store(state.location,channel,Bits(value));
            }
            if(onMove)onMove(index,hit);
            return hitTime<1.0f?0:1;
        };
        q.processLanded=[&](Id actor,Vec normal,Id floor,float remaining,std::int32_t iterations) {
            Actor(actor);order.push_back("landed");landings.push_back({normal,state.velocity,floor,remaining,iterations});if(onLanded)onLanded();
        };
        q.hitWall=[&](Id actor,Vec normal,Id wall) {
            Actor(actor);order.push_back("hit-wall");walls.push_back({normal,state.velocity,wall});if(onHitWall)onHitWall(normal,wall);
        };
        q.physics=[&](Id actor){Actor(actor);order.push_back("physics");return state.physics;};
        q.fellOutOfWorld=[&](Id actor){Actor(actor);order.push_back("out-of-world");if(onOutOfWorld)onOutOfWorld();};
        return q;
    }
    OriginalActorFallingStats Direct(float dt,std::int32_t iterations,const OriginalActorFallingQueries& q) {
        return QuestVr::StepOriginalActorFalling(1u,dt,iterations,q);
    }
    OriginalActorFallingStats RunWith(float dt,std::int32_t iterations,const OriginalActorFallingQueries& q) {
        const auto before=state;try{return Direct(dt,iterations,q);}catch(...){state=before;throw;}
    }
    OriginalActorFallingStats Run(float dt=0.0625f,std::int32_t iterations=0) {
        return RunWith(dt,iterations,Queries());
    }
};
void DryArithmeticContracts() {
    {
        Fixture f;f.state.velocity={16,-32,-16};f.state.acceleration={32,64,128};f.state.gravity={32,-64,-640};f.state.drift={8,4,-8};
        const auto originalAcceleration=f.state.acceleration;const auto result=f.Run();
        Require(result.substeps==1u&&result.moveCalls==1u&&result.apexSplits==0u&&result.remaining==0.0f&&result.iterations==1,
            "Dry dyadic falling step changed timestep/iteration accounting");
        Require(Equal(f.moves[0].velocity,{18,-32,-32})&&Equal(f.moves[0].delta,{1.625f,-1.75f,-2.5f})&&
            Equal(f.state.location,{1.625f,-1.75f,-2.5f})&&Equal(f.state.oldLocation,{})&&Equal(f.state.velocity,{20,-32,-48}),
            "Original dry half-step acceleration/gravity/drift/displacement reconstruction changed");
        Require(result.oldLocationWords==3u&&result.teleportWrites==1u&&!f.state.teleported&&result.accelerationWords==3u&&
            Equal(f.state.acceleration,originalAcceleration)&&result.steps==f.work,
            "Falling lost raw OldLocation, teleport clear, final acceleration restore or shared work ledger");
        const auto old=std::find(f.order.begin(),f.order.end(),"old-write-0");
        Require(old!=f.order.end()&&f.order.end()-old>=6&&*(old+1)=="location-1"&&*(old+2)=="old-write-1"&&
            *(old+3)=="location-2"&&*(old+4)=="old-write-2"&&*(old+5)=="teleport-write",
            "Falling reordered raw OldLocation XYZ stores and teleport clear");
    }
    {
        Fixture f;f.state.velocity={0,0,-16};f.state.acceleration={16777216.0f,0,0};f.state.gravity={1,0,0};f.state.terminal=500000;
        const auto result=f.Run(0.046875f);
        Require(result.substeps==1u&&f.moves[0].velocity.x==393216.0f&&f.moves[0].delta.x==18432.0f,
            "Dry falling retained extended Acceleration+Gravity across the original binary32 constructor spill");
    }
    {
        Fixture f;f.state.velocity={16777216.0f,0,-16};f.state.gravity={};f.state.drift={1,0,0};f.state.terminal=1.0e9f;
        static_cast<void>(f.Run(0.046875f));
        Require(f.moves[0].delta.x==786432.0f,
            "Dry falling retained extended Velocity+ZoneVelocity across original movement Vector spill");
    }
    {
        Fixture f;f.state.gravity={};f.state.velocity={0,0,-16};
        f.onMove=[&](std::size_t,OriginalActorLineHit&){f.Store(f.state.location,2u,Bits(-0.5f));};
        const auto result=f.Run();
        Require(result.moveCalls==1u&&Equal(f.state.velocity,{0,0,-8}),
            "Negative no-hit reconstruction always doubled velocity despite newVz>=oldVz and oldVz<0");
    }
    {
        Fixture f;f.state.gravity={};f.state.velocity={0,0,16};
        f.onMove=[&](std::size_t,OriginalActorLineHit&){f.Store(f.state.location,2u,Bits(0.5f));};
        static_cast<void>(f.Run());Require(Equal(f.state.velocity,{}),
            "Nonnegative oldVz skipped original 2*displacementVelocity-oldVelocity branch");
    }
    {
        Fixture f;f.state.velocity={0,0,-64};f.state.gravity={};f.state.terminal=16;
        static_cast<void>(f.Run());Require(f.moves[0].delta.z==-4.0f&&Equal(f.state.location,{0,0,-4})&&Equal(f.state.velocity,{0,0,-16}),
            "Terminal speed was clamped before actual sweep or failed to normalize/scale after reconstruction");
    }
    for(const auto speed:{200.0f,201.0f}) {
        Fixture f;f.state.gravity={};f.state.drift={speed,0,0};f.state.pawnFlag=true;
        static_cast<void>(f.Run());
        Require(f.moves[0].delta.x==(speed==200.0f?0.0f:12.5625f)&&f.state.velocity.x==0.0f,
            "Original bIsPawn/nonPlayerPawn drift gate lost strict zoneSpeedSquared>40000 condition");
    }
}
void AdaptiveAndApexContracts() {
    for(const auto& entry:std::array<std::pair<float,std::int32_t>,4u>{{{0.125f,0},{1.0f,0},{1.0f,7},{0.125f,8}}}) {
        Fixture f;f.state.gravity={};f.state.velocity={1,0,-1};
        f.onMove=[&](std::size_t,OriginalActorLineHit&){f.Write();f.state.teleported=true;};
        const auto result=f.Run(entry.first,entry.second);
        const std::size_t expected=entry.second>=8?0u:entry.first==0.125f?2u:entry.second==7?1u:8u;
        Require(result.substeps==expected&&f.moves.size()==expected&&result.iterations==entry.second+static_cast<std::int32_t>(expected),
            "Original adaptive loop lost remainingTime condition or shared eight-iteration bound");
        if(entry.first==0.125f&&expected)Require(f.moves[0].delta.x==0.0625f&&f.moves[1].delta.x==0.0625f&&result.remaining==0.0f,
            "Original >0.1 adaptive split was replaced by fixed0.1/0.02 steps");
        if(entry.first==1.0f)for(const auto& move:f.moves)Require(Bits(move.delta.x)==0x3dcccccdu,
            "Large remainingTime lost original stored Float0.1 cap");
        Require(result.accelerationWords==3u&&f.accelerationRestores==3u,"Loop completion did not restore original raw acceleration");
    }
    {
        Fixture f;f.state.gravity={};f.state.velocity={1,0,-1};
        f.onMove=[&](std::size_t,OriginalActorLineHit&){f.Write();f.state.teleported=true;};
        const auto result=f.Run(0.1f);
        Require(result.substeps==2u&&f.moves[0].delta.x==0.05f&&f.moves[1].delta.x==0.05f,
            "Adaptive comparison conflated stored Float0.1 with original extended Double0.1 threshold");
    }
    {
        Fixture f;f.state.gravity={};f.state.velocity={1,0,-1};
        f.onMove=[&](std::size_t,OriginalActorLineHit&){f.Write();f.state.teleported=true;};
        const float below=std::bit_cast<float>(0x3dccccccu);const auto result=f.Run(below);
        Require(result.substeps==1u&&Bits(f.moves[0].delta.x)==Bits(below),
            "Adaptive comparison split the stored Float immediately below original Double0.1 threshold");
    }
    {
        Fixture f;f.state.velocity={0,0,8};f.state.gravity={0,0,-512};const auto result=f.Run();
        Require(result.apexSplits==1u&&result.substeps==2u&&result.moveCalls==2u&&result.remaining==0.0f&&
            Equal(f.moves[0].velocity,{})&&Equal(f.moves[0].delta,{})&&f.moves[1].velocity.z==-16.0f&&
            f.moves[1].delta.z==-0.5f&&f.state.velocity.z==-24.0f,
            "Original apex resplit failed to recompute first half, retain unused duration or suppress immediate repeated split");
    }
    for(const auto oldSpeed:{4.0f,5.0f}) {
        Fixture f;f.state.velocity.z=oldSpeed;const auto result=f.Run();
        Require(result.apexSplits==0u&&result.substeps==1u,"Apex split ignored strict old/new speed magnitude>5 condition");
    }
    {
        Fixture f;f.state.velocity.z=8;const auto result=f.Run(0.03125f);
        Require(result.apexSplits==0u&&result.substeps==1u,"Non-crossing midpoint zero was treated as two-sided apex split");
    }
}
void LandingAndFreshReadContracts() {
    {
        Fixture f;f.hitTime=0.5f;f.state.acceleration={8,16,24};
        f.onLanded=[&]{for(unsigned i=0;i<3u;++i){f.Store(f.state.acceleration,i,0u);f.Store(f.state.velocity,i,0u);}};
        const auto result=f.Run();
        Require(result.landed&&result.landedCalls==1u&&result.moveCalls==1u&&result.accelerationWords==0u&&f.accelerationRestores==0u&&
            f.landings.size()==1u&&Equal(f.landings[0].normal,{0,0,1})&&f.landings[0].floor==7u&&
            f.landings[0].remaining==0.03125f&&f.landings[0].iterations==1&&Equal(f.state.acceleration,{})&&Equal(f.state.velocity,{}),
            "True swept landing lost original hit packet/unused time or incorrectly restored acceleration after selected landing");
        Require(Equal(f.landings[0].velocity,f.moves[0].velocity),
            "Landing reconstructed impact velocity with full dt instead of hitTime*dt");
    }
    {
        Fixture f;f.state.acceleration={8,16,24};
        f.onMove=[&](std::size_t,OriginalActorLineHit&){f.Write();f.state.deleted=true;f.Store(f.state.acceleration,0u,Bits(99.0f));};
        auto q=f.Queries();q.bounce={};q.justTeleported={};q.processLanded={};q.terminal={};q.writeAccelerationWord={};
        const auto result=f.Direct(0.0625f,0,q);
        Require(result.deleted&&result.moveCalls==1u&&!result.landed&&result.accelerationWords==0u&&f.accelerationRestores==0u&&
            f.state.acceleration.x==99.0f&&f.order.back()=="deleted",
            "Deleted-after-sweep path reached inactive providers or restored original acceleration");
    }
    {
        Fixture f;f.state.velocity={0,0,-16};f.state.gravity={};f.state.acceleration={};
        f.onMove=[&](std::size_t index,OriginalActorLineHit&){if(index==0u){f.Store(f.state.acceleration,0u,Bits(32.0f));f.Write();f.state.rotation={4,5,6};}};
        const auto result=f.Run(0.125f);
        Require(result.substeps==2u&&f.moves[0].velocity.x==0.0f&&f.moves[1].velocity.x==1.0f&&f.moves[1].rotation==Rotation{4,5,6}&&
            Equal(f.state.acceleration,{}),"Next substep cached acceleration/Rotation across real move callbacks or lost original final acceleration restore");
    }
    {
        Fixture f;f.state.velocity={0,0,-16};f.state.gravity={};f.state.terminal=1000;
        f.onMove=[&](std::size_t,OriginalActorLineHit&){f.Write();f.state.zone=4u;f.Write();f.state.terminal=4.0f;};
        auto q=f.Queries();const auto terminal=q.terminal;
        q.terminal=[&,terminal](Id zone){Require(zone==4u,"Terminal clamp used cached pre-move Zone");return terminal(zone);};
        static_cast<void>(f.Direct(0.0625f,0,q));Require(Equal(f.state.velocity,{0,0,-4}),"Terminal clamp used cached pre-move terminal speed");
    }
    {
        Fixture f;f.state.gravity={};f.state.drift={8,0,0};
        f.onMove=[&](std::size_t index,OriginalActorLineHit&){if(index==0u)f.Store(f.state.drift,0u,Bits(16.0f));};
        static_cast<void>(f.Run(0.125f));
        Require(f.moves.size()==2u&&f.moves[0].delta.x==0.5f&&f.moves[1].delta.x==1.0f&&f.state.velocity.x==0.0f,
            "Falling reread callback-mutated drift in old displacement correction or cached drift into next substep");
    }
    for(const auto& count:std::array<std::pair<std::int32_t,std::int32_t>,4u>{{{5,4},{1,0},{0,0},
        {std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()}}}) {
        Fixture f;f.hitTime=0.5f;f.decorationClass=true;f.hitPlayerPawn=true;f.state.counter=count.first;
        const auto result=f.Run();Require(result.counterWrites==1u&&f.state.counter==count.second&&result.landed,
            "Decoration collision with actual PlayerPawn failed to decrement live landing counter before selected landing");
    }
}
void BounceContracts() {
    {
        Fixture f;f.hitTime=0.5f;f.state.bounce=true;f.state.acceleration={8,16,24};
        const Vec supplied{std::bit_cast<float>(0x7f812345u),-0.0f,1.0f};f.hitNormal=supplied;
        auto q=f.Queries();
        f.onHitWall=[&](Vec normal,Id wall) {
            Require(Equal(normal,supplied)&&wall==7u,"Selected HitWall lost raw by-value Normal or original hit Actor");
            // Reentrant changes to the shared native hit packet cannot alias
            // the event's already copied Vector/Actor arguments.
            f.hitPacket->normal={99,88,77};f.hitPacket->actor=99u;
            Require(Equal(normal,supplied)&&wall==7u,"HitWall event packet aliased the shared native hit result");
            normal={-3,-2,-1};wall=55u;
            Require(Equal(normal,{-3,-2,-1})&&wall==55u&&Equal(f.hitPacket->normal,{99,88,77})&&f.hitPacket->actor==99u,
                "Mutating by-value event locals changed the original native hit packet");
            f.Store(f.state.acceleration,0u,Bits(77.0f));
            const auto nested=f.Direct(0.0f,8,q);
            Require(nested.substeps==0u&&nested.accelerationWords==3u&&f.state.acceleration.x==77.0f,
                "HitWall reentry lost same live Actor fields or the nested raw acceleration restoration");
            f.Write();f.state.physics=0u;f.Write();f.state.deleted=true;
        };
        q.processLanded={};q.terminal={};q.justTeleported={};
        const auto result=f.Direct(0.0625f,0,q);
        Require(result.hitWallCalls==1u&&result.stoppedByBounce&&!result.deleted&&!result.landed&&
            result.remaining==0.0f&&result.accelerationWords==0u&&f.accelerationRestores==3u&&
            f.walls.size()==1u&&Equal(f.walls[0].normal,supplied)&&f.walls[0].actor==7u&&
            f.state.acceleration.x==77.0f&&f.state.deleted&&f.order.back()=="physics",
            "Bounce Physics0 failed its fresh early return, restored outer acceleration, reread deletion or reached inactive tail providers");
        Require(std::count(f.order.begin(),f.order.end(),"deleted")==1,
            "Bounce introduced a post-HitWall deletion guard absent from original physFalling");
    }
    for(const auto mode:std::array<std::uint8_t,4u>{1u,2u,3u,255u}) {
        Fixture f;f.hitTime=0.5f;f.state.bounce=true;f.state.gravity={};f.state.velocity={8,0,-16};
        f.onHitWall=[&](Vec,Id){f.Write();f.state.physics=mode;};
        auto q=f.Queries();q.processLanded={};q.justTeleported={};q.terminal={};
        const auto result=f.Direct(0.0625f,0,q);
        Require(result.hitWallCalls==3u&&result.substeps==3u&&result.moveCalls==3u&&result.iterations==3&&
            result.remaining==0.0f&&!result.stoppedByBounce&&!result.deleted&&!result.landed&&f.state.physics==mode&&
            f.moves[0].delta.x==0.5f&&f.moves[1].delta.x==0.25f&&f.moves[2].delta.x==0.125f&&
            Equal(f.state.location,{0.4375f,0,-0.875f})&&Equal(f.state.velocity,{8,0,-16})&&
            result.velocityWords==9u&&result.accelerationWords==3u,
            "Bounce rejected a nonzero live mode, restored unused time after its first two hits or failed the fresh bounce/collision tail skip");
        Require(std::count(f.order.begin(),f.order.end(),"physics")==3&&
            std::count(f.order.begin(),f.order.end(),"teleported")==0&&std::count(f.order.begin(),f.order.end(),"terminal")==0,
            "Live bounce tail consulted teleport/clamp providers before its original collision short-circuit");
    }
    {
        Fixture f;f.hitTime=std::bit_cast<float>(0x33000000u);f.state.bounce=true;f.state.gravity={};
        const auto result=f.Run(0.09f,7);
        // This small hit.Time is NOT a subtraction-spill discriminator: the
        // exact loss is less than half a Float ulp at the original 0.09f.
        Require(result.hitWallCalls==1u&&result.iterations==8&&Bits(result.remaining)==0x3db851ecu,
            "Bounce near-zero hit time incorrectly forced a downward Float rounding step");
    }
    {
        Fixture f;f.hitTime=std::bit_cast<float>(0x3e00000bu);f.state.bounce=true;f.state.gravity={};
        const auto result=f.Run(0.09f,7);
        // Exact original product 709316836621020*2^-53 fits retained PC64.
        // Premature Float(1-hit.Time) would instead produce 0x3da147ac.
        Require(result.hitWallCalls==1u&&result.iterations==8&&Bits(result.remaining)==0x3da147adu,
            "Bounce unused time spilled Double1-hit.Time to Float before multiplying by the current timestep");
    }
    {
        Fixture f;f.hitTime=std::bit_cast<float>(0x3e000033u);f.state.bounce=true;f.state.gravity={};
        const auto result=f.Run(0.25f,7);
        // Delta 0x3dcccccd, remaining 0x3e19999a: exact combined numerator
        // 2139209203921705*2^-53; a product Float spill gives 0x3e73332e.
        Require(result.hitWallCalls==1u&&result.iterations==8&&Bits(result.remaining)==0x3e73332fu,
            "Bounce unused time spilled its product to Float before adding the existing remaining duration");
    }
    {
        Fixture f;f.hitTime=0.5f;f.state.bounce=true;f.state.gravity={};
        f.onHitWall=[&](Vec,Id){f.Write();f.state.bounce=false;f.hitTime=1.0f;};
        const auto result=f.Run();
        Require(result.hitWallCalls==1u&&result.moveCalls==2u&&!result.stoppedByBounce&&result.landedCalls==0u&&
            Equal(f.state.velocity,{0,0,-8})&&std::count(f.order.begin(),f.order.end(),"terminal")==2&&
            std::count(f.order.begin(),f.order.end(),"teleported")==2,
            "HitWall clearing bBounce failed the fresh common-tail correction and terminal/teleport reads");
    }
    {
        Fixture f;f.hitTime=0.5f;f.state.bounce=true;f.state.gravity={};
        f.onHitWall=[&](Vec,Id){f.Write();f.state.bounce=false;f.Write();f.state.teleported=true;
            f.Store(f.state.velocity,0u,Bits(64.0f));};
        auto q=f.Queries();q.terminal={};q.processLanded={};
        const auto result=f.Direct(0.0625f,7,q);
        Require(result.hitWallCalls==1u&&result.remaining==0.03125f&&result.iterations==8&&
            f.state.velocity.x==64.0f&&result.velocityWords==3u&&result.accelerationWords==3u&&
            std::count(f.order.begin(),f.order.end(),"teleported")==1,
            "Fresh callback bJustTeleported failed to skip displacement correction and terminal clamp after bBounce was cleared");
    }
    {
        Fixture f;f.hitTime=0.5f;f.state.gravity={};
        f.onMove=[&](std::size_t,OriginalActorLineHit&){f.Write();f.state.bounce=true;};
        f.onHitWall=[&](Vec,Id){f.Write();f.state.physics=0u;};
        const auto result=f.Run();
        Require(result.hitWallCalls==1u&&result.stoppedByBounce&&result.landedCalls==0u,
            "Falling cached bBounce before movement callback reentry instead of selecting the live post-sweep branch");
    }
    {
        Fixture f;f.hitTime=0.5f;f.state.bounce=true;f.state.acceleration={8,16,24};
        f.onHitWall=[&](Vec,Id){f.Write();f.state.deleted=true;f.Write();f.state.physics=3u;
            f.Store(f.state.acceleration,0u,Bits(99.0f));};
        auto q=f.Queries();q.justTeleported={};q.terminal={};q.processLanded={};
        const auto result=f.Direct(0.0625f,7,q);
        Require(!result.deleted&&!result.stoppedByBounce&&result.hitWallCalls==1u&&result.accelerationWords==3u&&
            f.state.deleted&&f.state.physics==3u&&Equal(f.state.acceleration,{8,16,24})&&
            std::count(f.order.begin(),f.order.end(),"deleted")==1,
            "Nonzero bounce callback deletion introduced an early return or bypassed normal loop-completion acceleration restoration");
    }
    for(unsigned missing=0u;missing<2u;++missing) {
        Fixture f;f.hitTime=0.5f;f.state.bounce=true;auto q=f.Queries();
        if(missing==0u)q.hitWall={};else q.physics={};
        const auto before=f.state;bool refused{};
        try{static_cast<void>(f.RunWith(0.0625f,0,q));}catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.state==before&&f.moves.size()==1u&&f.walls.size()==missing&&f.accelerationRestores==0u,
            "Missing reached HitWall/Physics provider was swallowed, preflighted before its real prefix or escaped root rollback");++refusals;
    }
    {
        Fixture f;f.hitTime=0.5f;f.state.bounce=true;
        f.onHitWall=[&](Vec,Id){f.Store(f.state.acceleration,0u,Bits(77.0f));throw std::runtime_error("wall-callback");};
        const auto before=f.state;bool refused{};
        try{static_cast<void>(f.Run());}catch(const std::runtime_error& e){refused=std::string(e.what())=="wall-callback";}
        Require(refused&&f.state==before&&f.walls.size()==1u&&f.accelerationRestores==0u&&
            std::count(f.order.begin(),f.order.end(),"physics")==0,
            "Selected HitWall failure was hidden, reached a later mode test or escaped same-root rollback");++refusals;
    }
    {
        Fixture f;f.hitTime=0.5f;f.state.bounce=true;f.writeLimit=11u;
        f.onHitWall=[&](Vec,Id){f.Store(f.state.acceleration,0u,Bits(77.0f));f.Write();f.state.physics=0u;};
        bool refused{};
        try{static_cast<void>(f.Direct(0.0625f,0,f.Queries()));}catch(const std::runtime_error& e){refused=std::string(e.what())=="write";}
        Require(refused&&f.writes==11u&&f.walls.size()==1u&&f.state.acceleration.x==77.0f&&f.state.physics==2u&&
            Equal(f.state.oldLocation,{})&&!f.state.teleported&&f.accelerationRestores==0u,
            "Bounce coordinator locally erased the selected callback's late pending write prefix");++refusals;
    }
    const auto initialize=[](Fixture& f,unsigned branch) {
        f.hitTime=0.5f;f.state.bounce=true;f.state.acceleration={8,16,24};
        f.onMove=[&f,branch](std::size_t,OriginalActorLineHit&){f.Store(f.state.acceleration,0u,Bits(77.0f));
            if(branch==3u){f.Write();f.state.teleported=true;}};
        f.onHitWall=[&f,branch](Vec,Id) {
            f.Store(f.state.acceleration,1u,Bits(88.0f));f.Write();f.state.physics=branch==0u?0u:255u;
            if(branch>=2u){f.Write();f.state.bounce=false;f.hitTime=1.0f;}
            if(branch==3u){f.Write();f.state.teleported=true;}
        };
    };
    for(unsigned branch=0u;branch<4u;++branch) {
        Fixture full;initialize(full,branch);const auto usage=full.Run();
        Require(usage.steps==full.work&&usage.hitWallCalls>0u&&usage.stoppedByBounce==(branch==0u),
            "Bounce root controls did not actually reach their selected stop/continue branch or shared work ledger");
        for(std::size_t limit=0u;limit<usage.steps;++limit) {
            Fixture f;initialize(f,branch);f.workLimit=limit;const auto before=f.state;bool refused{};
            try{static_cast<void>(f.Run());}catch(const std::runtime_error& e){refused=std::string(e.what())=="work";}
            Require(refused&&f.work==limit&&f.state==before,
                "Shared bounce work cutoff was reset/swallowed or escaped caller-owned whole-root rollback");++refusals;
        }
        for(std::size_t limit=0u;limit<full.writes;++limit) {
            Fixture f;initialize(f,branch);f.writeLimit=limit;const auto before=f.state;bool refused{};
            try{static_cast<void>(f.Run());}catch(const std::runtime_error& e){refused=std::string(e.what())=="write";}
            Require(refused&&f.writes==limit&&f.state==before,
                "Shared bounce write cutoff was reset/swallowed or escaped caller-owned whole-root rollback");++refusals;
        }
    }
}
void ActiveBranchesAndRootContracts() {
    for(unsigned branch=0u;branch<3u;++branch) {
        Fixture f;if(branch==0u)f.pawnClass=true;else if(branch==1u)f.state.water=true;
        else {f.decorationClass=true;f.state.bobbing=true;}
        const auto before=f.state;bool refused{};try{static_cast<void>(f.Run());}catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.state==before,"Active unsupported Pawn/water/bobbing branch silently succeeded or escaped caller root rollback");++refusals;
    }
    {
        Fixture f;f.hitTime=0.5f;f.hitNormal={1,0,0};const auto before=f.state;bool refused{};
        try{static_cast<void>(f.Run());}catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.moves.size()==1u&&f.landings.empty()&&f.state==before,
            "Active swept wall/slide branch was treated as landing or no-hit continuation");++refusals;
    }
    {
        Fixture f;auto q=f.Queries();q.bobbing={};q.numLandings={};q.writeNumLandings={};q.processLanded={};q.fellOutOfWorld={};
        q.hitWall={};q.physics={};
        Require(f.Direct(0.0625f,0,q).moveCalls==1u,"Dry nonDecoration/no-hit falling preflighted inactive branch providers");
    }
    {
        Fixture f;f.hitTime=0.5f;auto q=f.Queries();q.hitWall={};q.physics={};
        Require(f.Direct(0.0625f,0,q).landedCalls==1u,
            "Ordinary non-bounce landing preflighted inactive HitWall/Physics providers");
    }
    for(unsigned missing=0u;missing<6u;++missing) {
        Fixture f;auto q=f.Queries();
        if(missing==0u)q.isPawnClass={};
        else if(missing==1u)q.writeOldLocationWord={};
        else if(missing==2u)q.moveActor={};
        else if(missing==3u)q.deleted={};
        else if(missing==4u)q.terminal={};
        else {f.hitTime=0.5f;q.processLanded={};}
        const auto before=f.state;bool refused{};
        try{static_cast<void>(f.RunWith(0.0625f,0,q));}catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.state==before&&(missing<2u?f.writes==0u:f.writes>0u),
            "Missing active falling provider silently succeeded, hid pending prefix or escaped caller rollback");++refusals;
    }
    {
        Fixture f;f.state.zoneNumber=0;f.onOutOfWorld=[&]{f.Write();f.state.deleted=true;};auto q=f.Queries();
        q.acceleration={};q.writeAccelerationWord={};q.location={};q.zone={};q.level={};q.moveActor={};q.processLanded={};
        const auto result=f.Direct(0.0625f,0,q);
        Require(result.outOfWorld&&result.outOfWorldCalls==1u&&result.substeps==0u&&result.accelerationWords==0u&&
            f.state.deleted&&f.order.back()=="out-of-world","Out-of-world branch read inactive acceleration/zone or skipped genuine selected callback");
    }
    for(const auto dt:{0.0f,-1.0f}) {
        Fixture f;f.state.acceleration={std::bit_cast<float>(0x7f812345u),-0.0f,std::bit_cast<float>(0xffc12345u)};
        const auto before=f.state.acceleration;auto q=f.Queries();q.zone={};q.location={};q.moveActor={};q.zoneGravity={};q.zoneVelocity={};q.terminal={};
        const auto result=f.Direct(dt,0,q);
        Require(result.substeps==0u&&result.accelerationWords==3u&&Equal(f.state.acceleration,before),
            "Inactive raw acceleration restore coerced/rejected NaN payload or signed zero");
    }
    {
        Fixture f;f.state.gravity.z=std::numeric_limits<float>::infinity();const auto before=f.state;bool refused{};
        try{static_cast<void>(f.Run());}catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.state==before&&f.writes==4u&&f.moves.empty(),
            "Active nonfinite gravity was coerced or its real old-location/teleport prefix escaped caller rollback");++refusals;
    }
    const auto initialize=[](Fixture& f,unsigned branch) {
        f.hitTime=branch==1u?0.5f:1.0f;
        if(branch==2u)f.state.velocity.z=8;
        if(branch==3u){f.state.velocity.z=-64;f.state.gravity={};f.state.terminal=16;}
        f.onMove=[&f](std::size_t,OriginalActorLineHit&){f.Store(f.state.acceleration,0u,Bits(77.0f));};
        f.onLanded=[&f]{f.Store(f.state.acceleration,0u,0u);f.Store(f.state.velocity,2u,0u);};
    };
    for(unsigned branch=0u;branch<4u;++branch) {
        Fixture full;initialize(full,branch);const auto usage=full.Run();
        for(std::size_t limit=0u;limit<usage.steps;++limit) {
            Fixture f;initialize(f,branch);f.workLimit=limit;const auto before=f.state;bool refused{};
            try{static_cast<void>(f.Run());}catch(const std::runtime_error& e){refused=std::string(e.what())=="work";}
            Require(refused&&f.work==limit&&f.state==before,"Shared falling work cutoff reset/swallowed pending prefix or escaped caller rollback");++refusals;
        }
        for(std::size_t limit=0u;limit<full.writes;++limit) {
            Fixture f;initialize(f,branch);f.writeLimit=limit;const auto before=f.state;bool refused{};
            try{static_cast<void>(f.Run());}catch(const std::runtime_error& e){refused=std::string(e.what())=="write";}
            Require(refused&&f.writes==limit&&f.state==before,"Shared falling write cutoff reset/swallowed pending prefix or escaped caller rollback");++refusals;
        }
    }
    {
        Fixture f;f.writeLimit=5u;bool refused{};
        try{static_cast<void>(f.Direct(0.0625f,0,f.Queries()));}catch(const std::runtime_error& e){refused=std::string(e.what())=="write";}
        Require(refused&&f.writes==5u&&Equal(f.state.oldLocation,{})&&!f.state.teleported&&f.state.velocity.z==-16.0f&&f.moves.empty(),
            "Falling coordinator locally erased raw pending prefix or swept before all provisional Velocity stores");++refusals;
    }
    {
        Fixture f;f.onMove=[&](std::size_t,OriginalActorLineHit&){f.Store(f.state.acceleration,0u,Bits(77.0f));throw std::runtime_error("move-callback");};
        const auto before=f.state;bool refused{};try{static_cast<void>(f.Run());}catch(const std::runtime_error& e){refused=std::string(e.what())=="move-callback";}
        Require(refused&&f.state==before&&f.moves.size()==1u&&f.accelerationRestores==0u,
            "Move callback failure was hidden, locally restored acceleration or escaped root rollback");++refusals;
    }
    {
        Fixture f;f.hitTime=0.5f;f.onLanded=[&]{f.Store(f.state.acceleration,0u,0u);throw std::runtime_error("landing-callback");};
        const auto before=f.state;bool refused{};try{static_cast<void>(f.Run());}catch(const std::runtime_error& e){refused=std::string(e.what())=="landing-callback";}
        Require(refused&&f.state==before&&f.landings.size()==1u&&f.accelerationRestores==0u,
            "Selected landing failure was hidden or escaped same-root rollback");++refusals;
    }
    {
        Fixture f;bool refused{};try{static_cast<void>(QuestVr::StepOriginalActorFalling(0u,0.0625f,0,f.Queries()));}
        catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.work==0u&&f.writes==0u,"Null falling Actor reached actual native providers");++refusals;
    }
}
void PawnSharedContracts() {
    {
        Fixture f;f.pawnClass=true;auto q=f.Queries();
        q.pawnPreprocess=[&](Id a,float dt,std::int32_t it){f.Actor(a);Require(dt==0.0625f&&it==3,"Pawn preprocessor lost whole delta/iterations");
            f.order.emplace_back("pawn-preprocess");return true;};
        q.zoneNumber={};const auto before=f.state;const auto result=f.RunWith(0.0625f,3,q);
        Require(result.capturedPawn&&result.pawnPreprocessed&&f.state==before&&f.order==std::vector<std::string>{"pawn-class","pawn-preprocess"},
            "Handled Pawn preprocessing read Zone/captured acceleration or fell through");
    }
    const auto bind=[](Fixture& f,OriginalActorFallingQueries& q,QuestVr::OriginalPawnFallingSetupQueries& setup) {
        f.pawnClass=true;f.state.pawnFlag=true;
        setup.work=q.work;setup.acceleration=q.acceleration;setup.velocity=q.velocity;
        setup.writeAccelerationWord=q.writeAccelerationWord;setup.writeVelocityWord=q.writeVelocityWord;
        setup.airControl=[](Id){return 0.05f;};setup.accelRate=[](Id){return 128.0f;};setup.groundSpeed=[](Id){return 1000.0f;};
        setup.safeNormal=[](Vec v){return QuestVr::ActorLineTraceDetail::Normal(v,true);};
        q.pawnPreprocess=[&f](Id,float,std::int32_t){f.order.emplace_back("pawn-preprocess");return false;};
        q.pawnSetup=[&setup,&f](Id a,float dt){f.order.emplace_back("pawn-setup");return QuestVr::SetupOriginalPawnFalling(a,dt,setup);};
        q.pawnVelocityCap=[&setup,&f](Id a,float cap){f.order.emplace_back("pawn-cap");return QuestVr::CapOriginalPawnFallingVelocity(a,cap,setup);};
    };
    {
        Fixture f;f.state.velocity={10,0,-16};f.state.acceleration={8,0,7};const auto before=f.state.acceleration;
        auto q=f.Queries();QuestVr::OriginalPawnFallingSetupQueries setup;bind(f,q,setup);
        const auto result=f.RunWith(0.0625f,0,q);
        const auto setupAt=std::find(f.order.begin(),f.order.end(),"pawn-setup"),capAt=std::find(f.order.begin(),f.order.end(),"pawn-cap");
        Require(result.capturedPawn&&!result.pawnPreprocessed&&result.moveCalls==1u&&result.accelerationWords==10u&&
            Equal(f.state.acceleration,before)&&setupAt!=f.order.end()&&capAt!=f.order.end(),
            "Shared Pawn falling did not run real setup, count stores and restore captured acceleration");
        Require(f.moves[0].acceleration.z==0.0f&&Bits(f.moves[0].acceleration.x)==Bits(6.4f)&&
            std::find(f.order.begin(),setupAt,"acceleration-2")!=setupAt&&
            std::find(capAt,f.order.end(),"move")!=f.order.end(),
            "Pawn setup/cap ran before acceleration capture or after movement");
    }
    {
        Fixture f;f.state.acceleration={0,0,7};f.state.velocity={10,0,-16};f.onMove=[&](std::size_t,OriginalActorLineHit&){f.state.physics=3u;};
        auto q=f.Queries();QuestVr::OriginalPawnFallingSetupQueries setup;bind(f,q,setup);
        bool swimming{};q.startSwimming=[&](Id a,Vec old,float dt,float remaining,std::int32_t iterations) {
            f.Actor(a);Require(Equal(old,{10,0,-16})&&dt==0.0625f&&remaining==0.0f&&iterations==1,"Swimming handoff lost old Velocity/time/iterations");
            swimming=true;
        };
        const auto result=f.RunWith(0.0625f,0,q);
        Require(result.swimming&&swimming&&!result.landed&&f.landings.empty()&&f.state.acceleration.z==0.0f&&result.accelerationWords==1u,
            "Pawn swimming handoff fell through into collision/tail or restored original acceleration");
    }
    {
        Fixture f;f.state.acceleration={0,0,7};f.state.velocity={10,0,-16};f.hitTime=0.5f;
        f.onMove=[&](std::size_t,OriginalActorLineHit&){f.state.physics=3u;};
        auto q=f.Queries();QuestVr::OriginalPawnFallingSetupQueries setup;bind(f,q,setup); // no swimming provider
        const auto before=f.state;bool refused{};try{f.RunWith(0.0625f,0,q);}catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.state==before&&f.landings.empty(),"Missing reached Pawn swimming silently used landing or escaped caller undo");++refusals;
    }
}
} // namespace

int main() {
    try {
        DryArithmeticContracts();AdaptiveAndApexContracts();LandingAndFreshReadContracts();BounceContracts();ActiveBranchesAndRootContracts();PawnSharedContracts();
        std::cout<<"Original dry Actor/Pawn falling coordinator: "<<checks<<" checks, "<<refusals
            <<" refusal controls. Synthetic native providers; NOT a world Tick or on-headset gravity acceptance.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
