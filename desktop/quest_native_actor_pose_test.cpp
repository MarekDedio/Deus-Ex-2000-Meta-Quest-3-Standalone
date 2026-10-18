#include "quest_native_actor_pose.h"

#include <bit>
#include <iostream>
#include <limits>
#include <memory>

namespace {
using Stack=QuestVr::NativeActorPoseStack;
std::size_t checks{},refusals{};
void Require(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
bool Same(const Stack::Pose& a,const Stack::Pose& b) {
    for(std::size_t i=0;i<3u;++i)if(std::bit_cast<std::uint32_t>(a.location[i])!=
        std::bit_cast<std::uint32_t>(b.location[i])||a.rotation[i]!=b.rotation[i])return false;
    return true;
}
template<class F> void Refuse(F run) {
    bool refused{};try {run();}catch(const std::exception&) {refused=true;}
    Require(refused,"Invalid native pose scope silently succeeded");++refusals;
}
void Nest(Stack& stack,std::size_t depth) {
    const Stack::Pose value{{float(depth),-0.0f,1.0f},{std::int32_t(depth),-1,0}};
    Stack::Scope scope(stack,19u,value);
    Require(stack.Depth()==depth+1u&&Same(*stack.Find(19u),value),"Nested native pose did not shadow the same actual actor");
    if(depth+1u<Stack::MaximumDepth) {
        Nest(stack,depth+1u);
        Require(stack.Depth()==depth+1u&&Same(*stack.Find(19u),value),"Nested native pose did not restore its exact parent");
    }else {
        Refuse([&]{Stack::Scope invalid(stack,19u,value);});
        Require(stack.Depth()==Stack::MaximumDepth&&Same(*stack.Find(19u),value),"Depth refusal modified active native pose");
    }
}
void Run() {
    Stack stack;
    Require(stack.Depth()==0u&&!stack.Find(0u)&&!stack.Find(19u),"Native pose stack is not initially empty");
    const Stack::Pose raw{{std::bit_cast<float>(0x7fc12345u),std::bit_cast<float>(0x80000000u),
        std::bit_cast<float>(0x7f812345u)},
        {std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max(),-1}};
    Refuse([&]{Stack::Scope invalid(stack,0u,raw);});
    Require(stack.Depth()==0u&&!stack.Find(19u),"Null identity refusal modified native pose storage");
    {
        Stack::Scope outer(stack,19u,raw);
        Require(Same(*stack.Find(19u),raw),"Native raw pose lost NaN payload, signed zero or int rotation bits");
        const Stack::Pose second{{4,5,6},{7,8,9}};
        {
            Stack::Scope other(stack,23u,second);
            Require(Same(*stack.Find(19u),raw)&&Same(*stack.Find(23u),second),"Native pose leaked into a different actor");
            try {Stack::Scope child(stack,19u,second);throw std::runtime_error("late provider failure");}
            catch(const std::runtime_error&) {}
            Require(Same(*stack.Find(19u),raw)&&Same(*stack.Find(23u),second)&&stack.Depth()==2u,
                "Exception restoration failed or replaced a different actor's pose");
        }
        Require(!stack.Find(23u)&&Same(*stack.Find(19u),raw)&&stack.Depth()==1u,"Normal restoration changed original raw pose bits");
    }
    Require(stack.Depth()==0u&&!stack.Find(19u),"Completed pose scope left temporary data visible");
    Nest(stack,0u);
    Require(stack.Depth()==0u&&!stack.Find(19u),"Maximum-depth scopes did not fully unwind");
    // Fixed owned slots stay safe if an internal guard is released out of
    // order: no dangling linked pointer is followed and holes are not visible.
    {
        auto first=std::make_unique<Stack::Scope>(stack,19u,raw);
        const Stack::Pose next{{2,3,4},{5,6,7}};
        auto second=std::make_unique<Stack::Scope>(stack,23u,next);
        first.reset();Require(!stack.Find(19u)&&Same(*stack.Find(23u),next),"Inactive earlier scope remained visible or invalidated later storage");
        second.reset();Require(!stack.Find(23u)&&stack.Depth()==0u,"Out-of-order release did not collapse inactive tail slots");
    }
    for(unsigned round=0;round<1024u;++round) {
        Stack::Scope reused(stack,19u,raw);
        Require(Same(*stack.Find(19u),raw)&&stack.Depth()==1u,"Released native pose slot failed exact reuse");
    }
    Require(!stack.Find(19u)&&stack.Depth()==0u,"Repeated native pose scopes left a retained override");
}
}
int main() {
    try {Run();std::cout<<"PASS bounded native actor pose scopes "<<checks<<" checks / "<<refusals<<" refusals.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Native actor pose scopes failed: "<<error.what()<<'\n';return 1;}
}
