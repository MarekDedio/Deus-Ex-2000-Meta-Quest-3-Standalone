#include "quest_actor_perform_physics.h"

#include <array>
#include <bit>
#include <cstddef>
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
using Dispatch=QuestVr::OriginalActorPhysicsDispatch;
using Queries=QuestVr::OriginalActorPerformPhysicsQueries;
using Stats=QuestVr::OriginalActorPerformPhysicsStats;
std::size_t checks{},refusals{};
void Require(bool value,const char* text) {if(!value)throw std::logic_error(text);++checks;}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
struct State {
    std::array<Id,8u> pending{};
    std::array<std::int32_t,3u> rate{};
    std::uint8_t physics{};
    bool deleted{};
    bool operator==(const State&)const=default;
};
struct Fixture {
    State state;
    std::vector<std::string> order;
    std::vector<std::pair<Id,Id>> postPackets,writes;
    std::vector<std::pair<Dispatch,std::uint32_t>> otherPackets;
    std::vector<std::uint32_t> fallingDeltas,rotationDeltas;
    std::function<void()> onFalling,onOther,onRotation,onPostTouch;
    std::function<void(Id,Id)> afterPointerWrite;
    std::size_t work{},workLimit{4096u},writeCount{},writeLimit{4096u};
    static void Actor(Id actor) {Require(actor==1u,"performPhysics changed original Actor receiver");}
    void Write() {if(writeCount>=writeLimit)throw std::runtime_error("write");++writeCount;}
    void Pending(Id actor,Id value) {
        Require(actor>0u&&actor<state.pending.size(),"PendingTouch store lost actual node identity");
        Write();state.pending[actor]=value;writes.emplace_back(actor,value);
    }
    void Mode(std::uint8_t value) {Write();state.physics=value;}
    void Rate(unsigned axis,std::int32_t value) {Write();state.rate.at(axis)=value;}
    void Deleted(bool value) {Write();state.deleted=value;}
    Queries GetQueries() {
        Queries q;
        q.work=[&]{if(work>=workLimit)throw std::runtime_error("work");++work;};
        q.physics=[&](Id actor) {Actor(actor);order.push_back("physics");return state.physics;};
        q.falling=[&](Id actor,float dt,std::int32_t iterations) {
            Actor(actor);Require(iterations==0,"performPhysics changed original falling iteration argument");
            order.push_back("falling");fallingDeltas.push_back(Bits(dt));if(onFalling)onFalling();
        };
        q.otherPhysicsBlock=[&](Id actor,Dispatch selected,float dt) {
            Actor(actor);order.push_back("other-"+std::to_string(static_cast<unsigned>(selected)));
            otherPackets.emplace_back(selected,Bits(dt));if(onOther)onOther();
        };
        q.rotationRate=[&](Id actor,unsigned axis) {
            Actor(actor);Require(axis<3u,"performPhysics read an invalid Rotator channel");
            order.push_back("rate-"+std::to_string(axis));return state.rate[axis];
        };
        q.physicsRotation=[&](Id actor,float dt) {
            Actor(actor);order.push_back("rotate");rotationDeltas.push_back(Bits(dt));if(onRotation)onRotation();
        };
        q.pendingTouch=[&](Id actor) {
            Require(actor>0u&&actor<state.pending.size(),"performPhysics dereferenced a null/invalid pending node");
            order.push_back("pending-"+std::to_string(actor));return state.pending[actor];
        };
        q.postTouch=[&](Id retained,Id other) {
            Require(retained>0u&&retained<state.pending.size(),"PostTouch lost retained Actor receiver");Actor(other);
            order.push_back("post-"+std::to_string(retained));postPackets.emplace_back(retained,other);if(onPostTouch)onPostTouch();
        };
        q.writePendingTouch=[&](Id actor,Id next) {
            order.push_back("write-"+std::to_string(actor)+"-"+std::to_string(next));Pending(actor,next);
            if(afterPointerWrite)afterPointerWrite(actor,next);
        };
        return q;
    }
    Stats Direct(float dt,const Queries& q) {return QuestVr::PerformOriginalActorPhysics(1u,dt,q);}
    Stats RunWith(float dt,const Queries& q) {
        const auto before=state;try{return Direct(dt,q);}catch(...){state=before;throw;}
    }
    Stats Run(float dt=0.0625f) {return RunWith(dt,GetQueries());}
};
template<class Function> void Refuses(Function function,const char* text) {
    bool refused{};try{function();}catch(const std::invalid_argument&){refused=true;++refusals;}
    Require(refused,text);
}
template<class Function> void Fails(Function function,const char* text) {
    bool refused{};try{function();}catch(const std::runtime_error&){refused=true;++refusals;}
    Require(refused,text);
}

void DispatchTable() {
    for(unsigned mode=0u;mode<256u;++mode) {
        Fixture f;f.state.physics=static_cast<std::uint8_t>(mode);
        const auto result=f.Run();
        const bool falling=mode==2u;
        const bool other=mode==6u||mode==7u||mode==8u||mode==9u||mode==11u;
        Require(result.physicsMode==mode&&result.physicsReads==1u,"performPhysics did not capture actual Byte once");
        Require(result.dispatched==(falling||other)&&result.dispatchCalls==(falling||other?1u:0u),
            "Original Actor dispatch table differs from owned GOTY");
        Require(f.fallingDeltas.size()==(falling?1u:0u)&&f.otherPackets.size()==(other?1u:0u),
            "performPhysics dispatched a Pawn-only/unused mode or missed an Actor mode");
        if(other)Require(static_cast<unsigned>(f.otherPackets[0].first)==mode&&f.otherPackets[0].second==Bits(0.0625f),
            "Other physics block lost exact selected mode/whole time packet");
        Require(result.rateReads==3u&&result.rotationCalls==0u&&result.pendingReads==1u&&
            result.pendingWrites==0u&&result.postTouchCalls==0u,"No-dispatch physics skipped/changed common epilogue");
        Require(result.steps==f.work&&f.order.back()=="pending-1","performPhysics escaped same-root work or epilogue order");
    }
    for(unsigned mode : {6u,7u,8u,9u,11u}) {
        Fixture f;f.state.physics=static_cast<std::uint8_t>(mode);auto q=f.GetQueries();q.otherPhysicsBlock={};
        Refuses([&]{f.RunWith(0.25f,q);},"Missing active original physics block did not refuse");
        Require(f.order==std::vector<std::string>{"physics"},"Unimplemented movement incorrectly proceeded into epilogue");
    }
    Fixture f;f.state.physics=2u;auto q=f.GetQueries();q.falling={};
    Refuses([&]{f.RunWith(0.25f,q);},"Missing active falling did not refuse");
    Require(f.order==std::vector<std::string>{"physics"},"Missing falling callback was silently empty");
    Fixture inactive;auto sparse=inactive.GetQueries();sparse.falling={};sparse.otherPhysicsBlock={};
    sparse.physicsRotation={};sparse.postTouch={};sparse.writePendingTouch={};
    Require(!inactive.RunWith(0.0f,sparse).dispatched,"Inactive native providers became mandatory");
}

void RotationGateAndTimeBits() {
    const std::array<std::int32_t,8u> rates{0,1,-1,65535,65536,-65536,std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()};
    for(unsigned axis=0u;axis<3u;++axis)for(auto rate:rates) {
        Fixture f;f.state.rate[axis]=rate;const auto result=f.Run(0.4375f);
        const bool selected=(static_cast<std::uint32_t>(rate)&0xffffu)!=0u;
        Require(result.rateReads==(selected?axis+1u:3u)&&result.rotationCalls==(selected?1u:0u)&&result.rotated==selected,
            "Rotation gate changed low16 mask, channel order or short circuit");
        Require(f.rotationDeltas.size()==(selected?1u:0u),"Rotation gate repeated/missed native rotation");
        if(selected)Require(f.rotationDeltas[0]==Bits(0.4375f),"Rotation did not receive original whole delta");
    }
    for(auto word:std::array<std::uint32_t,6u>{0u,0x80000000u,0xbf800000u,0x7f800000u,0x7fc12345u,0xffcabcdeu}) {
        Fixture f;f.state.physics=2u;f.state.rate[0]=1;
        const auto result=f.Run(std::bit_cast<float>(word));
        Require(result.rotated&&f.fallingDeltas==std::vector<std::uint32_t>{word}&&
            f.rotationDeltas==std::vector<std::uint32_t>{word},"Dispatcher converted/validated an untouched native time packet");
        Fixture inactive;Require(inactive.Run(std::bit_cast<float>(word)).pendingReads==1u,
            "Inactive time packet was spuriously admitted as arithmetic");
    }
    Fixture fresh;fresh.state.physics=2u;
    fresh.onFalling=[&]{fresh.Mode(0u);fresh.Rate(1u,3);fresh.Deleted(true);fresh.Pending(1u,2u);};
    fresh.state.pending[2]=3u;
    const auto result=fresh.Run();
    Require(result.dispatched&&result.physicsReads==1u&&result.rotated&&result.rateReads==2u&&result.processedPendingTouch,
        "Falling mode0/deletion return skipped unconditional fresh epilogue");
    Require(fresh.state.deleted&&fresh.state.physics==0u&&fresh.state.pending[1]==3u&&fresh.state.pending[2]==0u,
        "Common epilogue invented a post-falling deletion guard or redispatch");
    Fixture other;other.state.physics=11u;other.onOther=[&]{other.Rate(2u,1);other.Pending(1u,2u);};
    Require(other.Run().rotated&&other.postPackets.size()==1u,"Other dispatch callback lost fresh epilogue state");
    Fixture rotation;rotation.state.rate[0]=1;rotation.onRotation=[&]{rotation.Pending(1u,3u);};
    Require(rotation.Run().retained==3u&&rotation.postPackets==std::vector<std::pair<Id,Id>>{{3u,1u}},
        "PendingTouch read happened before native rotation returned");
}

void PendingTouchProtocol() {
    Fixture f;f.state.pending[1]=2u;f.state.pending[2]=3u;f.state.pending[3]=4u;
    const auto result=f.Run();
    Require(result.retained==2u&&result.node==2u&&result.next==3u&&result.pendingReads==3u&&
        result.postTouchCalls==1u&&result.pendingWrites==2u&&result.processedPendingTouch,
        "PendingTouch did not execute exactly one original dequeue");
    Require(f.postPackets==std::vector<std::pair<Id,Id>>{{2u,1u}}&&
        f.writes==std::vector<std::pair<Id,Id>>{{1u,3u},{2u,0u}},"PostTouch receiver/Other or final pointer order differs");
    Require(f.state.pending[1]==3u&&f.state.pending[2]==0u&&f.state.pending[3]==4u,
        "PendingTouch drained/cleared an additional node");
    Require(f.order==std::vector<std::string>{"physics","rate-0","rate-1","rate-2","pending-1","post-2",
        "pending-1","pending-2","write-1-3","write-2-0"},"PendingTouch native read/callback/write ordering changed");

    Fixture replacement;replacement.state.pending[1]=2u;replacement.state.pending[2]=6u;
    replacement.state.pending[3]=4u;replacement.state.pending[4]=5u;
    replacement.onPostTouch=[&]{replacement.Pending(1u,3u);replacement.Deleted(true);};
    const auto replaced=replacement.Run();
    Require(replaced.retained==2u&&replaced.node==3u&&replaced.next==4u&&replacement.state.pending[2]==6u&&
        replacement.state.pending[3]==0u&&replacement.state.pending[1]==4u&&replacement.state.deleted,
        "PostTouch reentry unlinked retained/stale-next or added a deletion guard");

    Fixture freshNext;freshNext.state.pending[1]=2u;freshNext.state.pending[2]=3u;
    freshNext.onPostTouch=[&]{freshNext.Pending(2u,5u);};
    Require(freshNext.Run().next==5u&&freshNext.state.pending[1]==5u,"PendingTouch cached next before selected callback");

    Fixture self;self.state.pending[1]=2u;self.state.pending[2]=2u;
    Require(self.Run().processedPendingTouch&&self.state.pending[1]==2u&&self.state.pending[2]==0u&&self.postPackets.size()==1u,
        "PendingTouch peer self-link was normalized or loop-drained");
    Fixture sourceSelf;sourceSelf.state.pending[1]=1u;
    Require(sourceSelf.Run().processedPendingTouch&&sourceSelf.state.pending[1]==0u&&
        sourceSelf.writes==std::vector<std::pair<Id,Id>>{{1u,1u},{1u,0u}},"PendingTouch source alias changed the two ordered stores");
    Fixture sourceReplacement;sourceReplacement.state.pending[1]=2u;
    sourceReplacement.onPostTouch=[&]{sourceReplacement.Pending(1u,1u);};
    const auto sourceResult=sourceReplacement.Run();
    Require(sourceResult.retained==2u&&sourceResult.node==1u&&sourceResult.next==1u&&sourceReplacement.state.pending[1]==0u,
        "Fresh node equal to source did not preserve original alias semantics");

    Fixture afterFirst;afterFirst.state.pending[1]=2u;afterFirst.state.pending[2]=3u;
    afterFirst.afterPointerWrite=[&](Id actor,Id next){if(actor==1u&&next==3u)afterFirst.Pending(2u,6u);};
    Require(afterFirst.Run().processedPendingTouch&&afterFirst.state.pending[1]==3u&&afterFirst.state.pending[2]==0u,
        "Second PendingTouch store failed to clear the retained fresh node after write reentry");

    Fixture nullFresh;nullFresh.state.pending[1]=2u;
    nullFresh.onPostTouch=[&]{nullFresh.Pending(1u,0u);};const auto before=nullFresh.state;
    Refuses([&]{nullFresh.Run();},"Null freshly reread PendingTouch was invented as successful dequeue");
    Require(nullFresh.state==before&&nullFresh.postPackets.size()==1u&&nullFresh.writes.size()==1u,
        "Null fresh PendingTouch did not preserve callback-before-refusal/root rollback");
    Fixture direct;direct.state.pending[1]=2u;direct.onPostTouch=[&]{direct.Pending(1u,0u);};
    Refuses([&]{direct.Direct(0.0625f,direct.GetQueries());},"Direct null fresh PendingTouch unexpectedly succeeded");
    Require(direct.state.pending[1]==0u,"Coordinator hid a callback effect through forbidden local rollback");
}

void MissingProvidersAndFailures() {
    Refuses([&]{QuestVr::PerformOriginalActorPhysics(0u,0.0f,{});},"Null source Actor accepted");
    Refuses([&]{QuestVr::PerformOriginalActorPhysics(1u,0.0f,{});},"Missing active Physics read accepted");
    Fixture rate;auto rateQueries=rate.GetQueries();rateQueries.rotationRate={};
    Refuses([&]{rate.RunWith(0.0f,rateQueries);},"Missing active RotationRate read accepted");
    Fixture rotate;rotate.state.rate[0]=1;auto rotateQueries=rotate.GetQueries();rotateQueries.physicsRotation={};
    Refuses([&]{rotate.RunWith(0.0f,rotateQueries);},"Missing reached physicsRotation accepted");
    Require(rotate.order==std::vector<std::string>{"physics","rate-0"},"Failed rotation proceeded to PendingTouch");
    Fixture pending;auto pendingQueries=pending.GetQueries();pendingQueries.pendingTouch={};
    Refuses([&]{pending.RunWith(0.0f,pendingQueries);},"Missing active PendingTouch read accepted");
    Fixture post;post.state.pending[1]=2u;auto postQueries=post.GetQueries();postQueries.postTouch={};
    Refuses([&]{post.RunWith(0.0f,postQueries);},"Missing selected PostTouch accepted");
    Require(post.writes.empty(),"Missing selected event still unlinked pending node");
    Fixture write;write.state.pending[1]=2u;auto writeQueries=write.GetQueries();writeQueries.writePendingTouch={};
    Refuses([&]{write.RunWith(0.0f,writeQueries);},"Missing active pointer store accepted");
    Require(write.postPackets.size()==1u&&write.writes.empty(),"Pointer admission changed selected event ordering");
    Fixture noWork;auto noWorkQueries=noWork.GetQueries();noWorkQueries.work={};
    Require(noWork.RunWith(0.0f,noWorkQueries).steps==5u&&noWork.work==0u,"Optional work hook changed native protocol");

    for(unsigned point=0u;point<4u;++point) {
        Fixture f;f.state.physics=point==1u?11u:2u;f.state.rate[0]=1;f.state.pending[1]=2u;f.state.pending[2]=3u;
        const auto callback=[&]{f.Deleted(true);throw std::runtime_error("callback");};
        if(point==0u)f.onFalling=callback;else if(point==1u)f.onOther=callback;
        else if(point==2u)f.onRotation=callback;else f.onPostTouch=callback;
        const auto before=f.state;
        Fails([&]{f.Run();},"Native callback failure was swallowed");
        Require(f.state==before,"Callback failure escaped caller root rollback");
        Require(f.writes.empty(),"Callback failure continued into pending pointer writes");
    }
}

void CutoffsAndReentry() {
    const auto configure=[](Fixture& f) {
        f.state.physics=2u;f.state.pending[1]=2u;f.state.pending[2]=5u;f.state.pending[3]=4u;
        f.onFalling=[&f]{f.Mode(0u);f.Rate(1u,7);};
        f.onRotation=[&f]{f.Deleted(true);};
        f.onPostTouch=[&f]{f.Pending(1u,3u);};
    };
    Fixture baseline;configure(baseline);const auto completed=baseline.Run();
    Require(completed.processedPendingTouch&&baseline.writeCount==6u&&baseline.state.pending[1]==4u&&baseline.state.pending[3]==0u,
        "Budget control did not exercise whole callback/write chain");
    for(std::size_t limit=0u;limit<baseline.work;++limit) {
        Fixture f;configure(f);f.workLimit=limit;const auto before=f.state;
        Fails([&]{f.Run();},"Work cutoff failed to abort original performPhysics");
        Require(f.state==before&&f.work==limit,"Work cutoff did not retain caller budget/full root rollback");
    }
    for(std::size_t limit=0u;limit<baseline.writeCount;++limit) {
        Fixture f;configure(f);f.writeLimit=limit;const auto before=f.state;
        Fails([&]{f.Run();},"Write cutoff failed to abort original performPhysics");
        Require(f.state==before&&f.writeCount==limit,"Late write cutoff did not undo original callbacks and pointer prefixes");
    }
    Fixture direct;configure(direct);direct.writeLimit=5u;
    Fails([&]{direct.Direct(0.0625f,direct.GetQueries());},"Direct late pointer cutoff unexpectedly succeeded");
    Require(direct.state.physics==0u&&direct.state.deleted&&direct.state.pending[1]==4u&&direct.state.pending[3]==4u,
        "performPhysics introduced its own partial undo instead of borrowing caller root");

    Fixture nested;nested.state.pending[1]=2u;nested.state.pending[2]=3u;nested.state.pending[3]=4u;
    bool reentered{};
    nested.onPostTouch=[&]{
        if(!reentered) {reentered=true;const auto inner=nested.Direct(0.125f,nested.GetQueries());
            Require(inner.retained==2u&&inner.node==2u&&inner.next==3u,"Reentered native dequeue lost its own retained packet");}
    };
    const auto outer=nested.Run();
    Require(outer.retained==2u&&outer.node==3u&&outer.next==4u&&nested.state.pending[1]==4u&&
        nested.state.pending[2]==0u&&nested.state.pending[3]==0u&&nested.postPackets.size()==2u,
        "Synchronous same-root reentry did not reread original source head");
}
} // namespace

int main() {
    try {
        DispatchTable();RotationGateAndTimeBits();PendingTouchProtocol();MissingProvidersAndFailures();CutoffsAndReentry();
        std::cout<<"Original Actor performPhysics: "<<checks<<" checks, "<<refusals<<" expected refusals\n";return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Original Actor performPhysics test failed: "<<error.what()<<'\n';return 1;
    }
}
