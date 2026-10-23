#include "quest_actor_set_physics.h"

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using QuestVr::OriginalPhysicsVector;
std::size_t checks{},refusals{};
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
struct State {
    std::uint8_t physics{2u};std::uintptr_t base{4u};
    std::array<std::uint32_t,3u> velocity{0x80000000u,0x7fc12345u,0x7f800000u};
    std::array<std::uint32_t,3u> acceleration{1u,0xff800000u,0xffc34567u};
    bool operator==(const State&)const=default;
};
struct Fixture {
    State state;std::vector<std::string> order;std::function<void()> callback;
    std::size_t work{},limit{64u},writes{},writeLimit{64u};
    QuestVr::OriginalActorPhysicsQueries Queries() {
        QuestVr::OriginalActorPhysicsQueries q;
        q.work=[&]{if(work>=limit)throw std::runtime_error("work");++work;};
        const auto actor=[](std::uintptr_t value){Require(value==1u,"Changed original receiver identity");};
        q.physics=[&,actor](auto self){actor(self);order.push_back("read-physics");return state.physics;};
        q.writePhysics=[&,actor](auto self,std::uint8_t value){actor(self);Write();order.push_back("write-physics");state.physics=value;};
        q.base=[&,actor](auto self){actor(self);order.push_back("read-base");return state.base;};
        q.setSupportBase=[&,actor](auto self,auto floor){actor(self);Require(floor==7u,"Changed floor identity");
            Write();order.push_back("support");state.base=floor;if(callback)callback();};
        q.findBase=[&,actor](auto self){actor(self);Write();order.push_back("find");state.base=7u;if(callback)callback();};
        q.setBase=[&,actor](auto self,auto floor,std::int32_t send){actor(self);Require(floor==0u&&send==1,"Detach lost None/true arguments");
            Write();order.push_back("detach");state.base=0u;if(callback)callback();};
        q.writeVectorWord=[&,actor](auto self,OriginalPhysicsVector vector,unsigned channel,std::uint32_t bits) {
            actor(self);Require(channel<3u&&bits==0u,"Zero store changed raw bits/channel");Write();
            order.push_back(std::string(vector==OriginalPhysicsVector::Velocity?"v":"a")+char('x'+channel));
            (vector==OriginalPhysicsVector::Velocity?state.velocity:state.acceleration)[channel]=bits;
        };
        return q;
    }
    void Write(){if(writes>=writeLimit)throw std::runtime_error("write");++writes;}
    auto Run(std::uint8_t mode,std::uintptr_t floor=0u) {
        const auto before=state;
        try{return QuestVr::SetOriginalActorPhysics(1u,mode,floor,Queries());}
        catch(...){state=before;throw;} // Mock caller-root undo, NOT coordinator behavior.
    }
};
void Contracts() {
    {
        Fixture f;auto q=f.Queries();q.base={};q.writePhysics={};q.findBase={};q.writeVectorWord={};
        const auto before=f.state;const auto result=QuestVr::SetOriginalActorPhysics(1u,2u,7u,q);
        Require(result.unchanged&&result.steps==1u&&result.physicsReads==1u&&f.state==before&&f.writes==0u,
            "Same Physics did not ignore floor, absent inactive providers and old nonfinite vector words");
    }
    for(unsigned raw=0u;raw<256u;++raw) {
        Fixture f;f.state.physics=static_cast<std::uint8_t>((raw+1u)&255u);const auto before=f.state;
        const bool support=raw==0u||raw==1u||raw==5u||raw==7u||raw==10u;
        const auto result=f.Run(static_cast<std::uint8_t>(raw));
        Require(result.findCalls==(support?1u:0u)&&result.detachCalls==(support?0u:1u)&&result.supportCalls==0u,
            "Raw byte dispatch differs from original exact support-mode predicates");
        Require(f.state.physics==raw&&f.state.base==(support?7u:0u)&&result.physicsReads==2u,
            "Physics store/support order or post-callback reread changed");
        Require(result.zeroWords==((raw==0u||raw==5u)?6u:0u),"Wrong final mode clears vectors");
        if(raw!=0u&&raw!=5u)Require(f.state.velocity==before.velocity&&f.state.acceleration==before.acceleration,
            "Nonzeroing mode touched inactive/nonfinite old vector bits");
        else Require(f.state.velocity==std::array<std::uint32_t,3u>{}&&f.state.acceleration==std::array<std::uint32_t,3u>{},
            "Final zeroing mode did not overwrite all six raw words");
    }
    for(const auto mode:{0u,1u,5u,7u,10u})for(const auto oldBase:{0u,4u,7u}) {
        Fixture f;f.state.base=oldBase;const auto result=f.Run(static_cast<std::uint8_t>(mode),7u);
        Require(result.supportCalls==(oldBase==7u?0u:1u)&&result.findCalls==0u&&result.detachCalls==0u&&f.state.base==7u,
            "Explicit same/new support floor took the wrong original branch");
    }
    for(const auto mode:{0u,1u,5u,7u,10u}) {
        Fixture f;f.state.base=0u;Require(f.Run(static_cast<std::uint8_t>(mode)).findCalls==1u,
            "None floor incorrectly skipped FindBase when Base was alreadyNone");
    }
    {
        Fixture f;f.state.base=0u;const auto before=f.state;f.state.physics=1u;
        const auto result=f.Run(2u);Require(result.detachCalls==0u&&result.baseReads==1u&&f.state.velocity==before.velocity,
            "Falling with None Base invented a detach/reset callback");
    }
    for(const auto replacement:{0u,1u,5u,2u,255u}) {
        Fixture f;f.state.physics=1u;f.callback=[&]{f.state.physics=static_cast<std::uint8_t>(replacement);};
        const auto result=f.Run(2u);
        Require(f.state.physics==replacement&&result.zeroWords==((replacement==0u||replacement==5u)?6u:0u),
            "Detach callback's replacement Physics was ignored");
    }
    {
        Fixture f;f.callback=[&]{f.state.physics=2u;};const auto before=f.state;const auto result=f.Run(0u);
        Require(result.findCalls==1u&&result.zeroWords==0u&&f.state.velocity==before.velocity,
            "FindBase replacement Physics did not suppress original zero block");
    }
    {
        Fixture f;f.callback=[&]{f.state.physics=0u;};const auto result=f.Run(1u,7u);
        Require(result.supportCalls==1u&&result.zeroWords==6u&&f.state.physics==0u,
            "Explicit SupportActor replacement Physics was not reread before zeroing");
    }
    {
        Fixture f;f.state.physics=1u;bool nested{};
        f.callback=[&] {
            if(nested)return;
            nested=true;const auto result=QuestVr::SetOriginalActorPhysics(1u,5u,7u,f.Queries());
            Require(result.supportCalls==1u&&result.zeroWords==6u,"Nested setter lost its own current-mode semantics");
        };
        const auto result=f.Run(2u);
        Require(nested&&result.detachCalls==1u&&result.zeroWords==6u&&f.state.physics==5u&&f.state.base==7u,
            "Outer setter overwrote reentrant Physics/Base or skipped final reread");
        Require(f.work==22u&&f.writes==16u,"Nested setter reset the caller's shared work/write ledger");
    }
    {
        Fixture f;auto q=f.Queries();q.findBase={};const auto before=f.state;bool refused{};
        try{static_cast<void>(QuestVr::SetOriginalActorPhysics(1u,0u,0u,q));}
        catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.state.physics==0u&&f.writes==1u,
            "Missing active provider was hidden or coordinator invented its own local rollback");
        f.state=before;Require(f.state==before,"Mock root failed to undo the pending prefix");++refusals;
    }
    {
        Fixture f;bool refused{};try{static_cast<void>(QuestVr::SetOriginalActorPhysics(0u,0u,0u,f.Queries()));}
        catch(const std::invalid_argument&){refused=true;}
        Require(refused&&f.work==0u&&f.writes==0u,"Null Actor reached native providers");++refusals;
    }
    {
        Fixture f;const auto result=f.Run(0u);
        const std::vector<std::string> expected{"read-physics","write-physics","find","read-physics","vx","vy","ax","vz","ay","az"};
        Require(result.steps==expected.size()&&f.order==expected,"Changed original interleaved raw store order");
    }
    Fixture full;const auto usage=full.Run(0u);
    for(std::size_t limit=0u;limit<usage.steps;++limit) {
        Fixture f;const auto before=f.state;f.limit=limit;bool refused{};
        try{static_cast<void>(f.Run(0u));}catch(const std::runtime_error& e){refused=std::string(e.what())=="work";}
        Require(refused&&f.state==before,"Partial shared-work prefix escaped caller rollback");++refusals;
    }
    for(std::size_t limit=0u;limit<full.writes;++limit) {
        Fixture f;const auto before=f.state;f.writeLimit=limit;bool refused{};
        try{static_cast<void>(f.Run(0u));}catch(const std::runtime_error& e){refused=std::string(e.what())=="write";}
        Require(refused&&f.state==before,"Partial shared-write prefix escaped caller rollback");++refusals;
    }
    {
        Fixture f;f.state.physics=1u;
        f.callback=[&]{f.state.base=19u;f.state.physics=5u;throw std::runtime_error("callback");};
        const auto before=f.state;bool refused{};try{static_cast<void>(f.Run(2u));}catch(const std::runtime_error&){refused=true;}
        Require(refused&&f.state==before,"Reentrant provider error did not propagate to whole caller undo");++refusals;
    }
}
}
int main() {
    try{Contracts();std::cout<<"Original SetPhysics coordinator: "<<checks<<" checks, "<<refusals<<" refusal controls. NOT runtime physics integration.\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
