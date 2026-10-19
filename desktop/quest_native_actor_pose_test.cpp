#include "quest_native_actor_pose.h"

#include <bit>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <vector>

namespace {bool forbidPoseAllocation{};}
void* operator new(std::size_t size) {
    if(forbidPoseAllocation)throw std::bad_alloc();
    if(auto* value=std::malloc(size?size:1u))return value;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {return ::operator new(size);}
void operator delete(void* value) noexcept {std::free(value);}
void operator delete[](void* value) noexcept {std::free(value);}
void operator delete(void* value,std::size_t) noexcept {std::free(value);}
void operator delete[](void* value,std::size_t) noexcept {std::free(value);}

namespace {
using Stack=QuestVr::NativeActorPoseStack;
std::size_t checks{},refusals{};
static_assert(!std::is_copy_constructible_v<Stack> && !std::is_move_constructible_v<Stack>);
static_assert(!std::is_copy_constructible_v<Stack::Scope> && !std::is_move_constructible_v<Stack::Scope>);
static_assert(!std::is_copy_constructible_v<Stack::LocationScope> && !std::is_move_constructible_v<Stack::LocationScope>);
static_assert(std::is_nothrow_destructible_v<Stack::Scope> && std::is_nothrow_destructible_v<Stack::LocationScope>);
static_assert(noexcept(std::declval<Stack&>().UndoLocation({},0u,0.0f)));
void Require(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
bool SameLocation(const std::array<float,3u>& a,const std::array<float,3u>& b) {
    for(unsigned i=0u;i<3u;++i)if(std::bit_cast<std::uint32_t>(a[i])!=std::bit_cast<std::uint32_t>(b[i]))return false;
    return true;
}
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
void PartialFields() {
    Stack stack;
    Stack::Pose durable{{11.0f,12.0f,13.0f},{101,102,103}};
    const auto original=durable;
    const auto read=[&] {
        Stack::Pose result=durable;
        if(const auto* location=stack.FindLocation(19u))result.location=*location;
        if(const auto* rotation=stack.FindRotation(19u))result.rotation=*rotation;
        return result;
    };
    const auto durableRotation=[&](std::array<std::int32_t,3u> rotation) {
        stack.RequireDurableRotationWrite(19u);durable.rotation=rotation;
    };
    Require(!stack.CaptureLocationWrite(19u),"Absent Location authority did not select durable write path");
    Refuse([&]{static_cast<void>(stack.CaptureLocationWrite(0u));});
    Refuse([&]{stack.RequireDurableRotationWrite(0u);});
    const std::array<float,3u> proposed{21.0f,22.0f,23.0f};
    Stack::LocationWriteToken expired;
    {
        Stack::LocationScope position(stack,19u,proposed);
        Require(stack.Depth()==1u&&stack.FindLocation(19u)&&!stack.FindRotation(19u)&&!stack.Find(19u),
            "Location-only scope invented a whole/Rotation view");
        Require(SameLocation(read().location,proposed)&&read().rotation==original.rotation,
            "Partial pose failed independent durable Rotation fallback");
        durableRotation({201,202,203});
        Require(read().rotation==durable.rotation,"Partial Location froze live durable Rotation");
        const auto authority=stack.CaptureLocationWrite(19u);expired=authority;
        Require(bool(authority),"Mutable Location did not issue its write authority");
        stack.WriteLocation(authority,1u,-0.0f);
        Require(std::bit_cast<std::uint32_t>(read().location[1u])==0x80000000u&&SameLocation(durable.location,original.location),
            "Temporary Location write changed durable data or lost signed zero");
        Refuse([&]{stack.WriteLocation(authority,3u,1.0f);});
        const Stack::Pose query{{31,32,33},{301,302,303}};
        {
            Stack::Scope full(stack,19u,query);
            Require(Same(read(),query)&&stack.Find(19u)&&Same(*stack.Find(19u),query),
                "Full lower-query view failed to shadow both independent fields");
            Refuse([&]{static_cast<void>(stack.CaptureLocationWrite(19u));});
            Refuse([&]{durableRotation({901,902,903});});
            Refuse([&]{stack.WriteLocation(authority,0u,91.0f);});
            Require(stack.UndoLocation(authority,0u,25.0f)&&Same(read(),query),
                "Raw undo of hidden layer leaked into readonly query pose");
            Require(SameLocation(durable.location,original.location)&&durable.rotation==std::array<std::int32_t,3u>{201,202,203},
                "Readonly rejection leaked a durable native field write");
            {
                Stack::LocationScope higherPosition(stack,19u,{41,42,43});
                const auto higher=stack.CaptureLocationWrite(19u);
                stack.WriteLocation(higher,2u,44.0f);
                Require(read().location==std::array<float,3u>{41,42,44}&&read().rotation==query.rotation&&!stack.Find(19u),
                    "Partial-over-full did not resolve the nearest authority separately per field");
                Refuse([&]{stack.RequireDurableRotationWrite(19u);});
            }
            Require(Same(read(),query),"Partial-over-full release changed readonly parent query");
        }
        Require(read().location==std::array<float,3u>{25,-0.0f,23}&&read().rotation==durable.rotation,
            "Full-query release failed to restore mutable Location/live durable Rotation");
        {
            Stack::LocationScope nested(stack,19u,{51,52,53});
            const auto child=stack.CaptureLocationWrite(19u);
            stack.WriteLocation(child,2u,54.0f);
            Refuse([&]{stack.ValidateLocationWrite(authority);});
            Require(stack.UndoLocation(authority,2u,26.0f)&&read().location==std::array<float,3u>{51,52,54},
                "Hidden mutable parent undo overwrote nearest child Location");
            durableRotation({401,402,403});
        }
        Require(read().location==std::array<float,3u>{25,-0.0f,26}&&read().rotation==durable.rotation,
            "Mutable nested Location release lost parent stores or newer durable Rotation");
    }
    Require(SameLocation(read().location,original.location)&&read().rotation==std::array<std::int32_t,3u>{401,402,403},
        "Reverse Location release leaked temporary position or undid durable Rotation");
    Require(!stack.UndoLocation(expired,0u,99.0f),"Expired Location undo remained writable");
    Refuse([&]{stack.ValidateLocationWrite(expired);});
    Require(!stack.CaptureLocationWrite(19u)&&!stack.FindLocation(19u)&&!stack.FindRotation(19u),
        "Completed partial scopes left native field authority visible");
}

void DelayedUndoAndReuse() {
    Stack stack,foreign;
    Stack::LocationWriteToken expired;
    {
        Stack::LocationScope view(stack,19u,{1,2,3});
        expired=stack.CaptureLocationWrite(19u);
        Require(!foreign.UndoLocation(expired,0u,99.0f),"Foreign host accepted a Location undo token");
        Refuse([&]{foreign.WriteLocation(expired,0u,99.0f);});
    }
    for(unsigned round=0u;round<1024u;++round) {
        // Same actor AND exact fixed slot are reused. The generation must
        // defeat ABA, not merely compare the actor pointer/index again.
        Stack::LocationScope current(stack,19u,{float(round),2,3});
        const auto valid=stack.CaptureLocationWrite(19u);
        Require(!stack.UndoLocation(expired,0u,99.0f),"Delayed undo aliased a reused same-actor Location slot");
        Refuse([&]{stack.ValidateLocationWrite(expired);});
        stack.WriteLocation(valid,1u,float(round));
        Require((*stack.FindLocation(19u))[0u]==float(round)&&(*stack.FindLocation(19u))[1u]==float(round),
            "Reused active generation lost its own Location write");
        expired=valid;
    }
    Require(!stack.UndoLocation({},0u,99.0f)&&!stack.UndoLocation(expired,3u,99.0f),
        "Empty/invalid-channel stale undo changed native storage");
    Refuse([&]{stack.WriteLocation({},0u,99.0f);});
    std::function<void()> delayed;
    {
        Stack::LocationScope view(stack,19u,{10,11,12});
        const auto token=stack.CaptureLocationWrite(19u);
        stack.ValidateLocationWrite(token);
        // Like the VM journal, this closure outlives the stack-local scope.
        delayed=[&stack,token]{static_cast<void>(stack.UndoLocation(token,1u,11.0f));};
        stack.WriteLocation(token,1u,81.0f);
    }
    {
        Stack::LocationScope replacement(stack,23u,{20,21,22});
        delayed();
        Require(!stack.FindLocation(19u)&&(*stack.FindLocation(23u))[1u]==21.0f,
            "Delayed VM journal closure targeted another actor's reused slot");
    }
    delayed();
    Require(!stack.Depth()&&!stack.FindLocation(19u)&&!stack.FindLocation(23u),
        "Delayed undo resurrected an inactive native pose");
}

void GenerationAndDepth() {
    const Stack::Pose full{{1,2,3},{4,5,6}};
    Stack bounded(2u);
    Refuse([&]{Stack::LocationScope invalid(bounded,0u,{1,2,3});});
    Stack::LocationWriteToken first;
    {
        Stack::LocationScope live(bounded,19u,{1,2,3});first=bounded.CaptureLocationWrite(19u);
        {
            Stack::Scope second(bounded,23u,full);
            Refuse([&]{Stack::LocationScope exhausted(bounded,29u,{7,8,9});});
            Require(bounded.Depth()==2u&&Same(*bounded.Find(23u),full)&&(*bounded.FindLocation(19u))[0u]==1.0f,
                "Generation exhaustion modified already-active field authority");
        }
        Refuse([&]{Stack::Scope exhausted(bounded,23u,full);});
        bounded.WriteLocation(first,0u,7.0f);
        Require((*bounded.FindLocation(19u))[0u]==7.0f,"Exhausted generation invalidated an existing mutable token");
    }
    Refuse([&]{Stack::LocationScope exhausted(bounded,19u,{1,2,3});});
    Require(!bounded.Depth()&&!bounded.UndoLocation(first,0u,1.0f),"Generation release refreshed allowance or revived expired token");
    Stack zero(0u);
    Refuse([&]{Stack::Scope exhausted(zero,19u,full);});
    Require(!zero.Depth()&&!zero.FindLocation(19u),"Zero lifetime allowance published a native pose");

    Stack depth(Stack::MaximumDepth+1u);
    std::vector<std::unique_ptr<Stack::LocationScope>> scopes;
    scopes.reserve(Stack::MaximumDepth);
    for(std::size_t i=0u;i<Stack::MaximumDepth;++i) {
        scopes.push_back(std::make_unique<Stack::LocationScope>(depth,19u,std::array<float,3u>{float(i),-0.0f,3.0f}));
        Require(depth.Depth()==i+1u&&(*depth.FindLocation(19u))[0u]==float(i),"Mutable fixed-depth nesting failed");
    }
    Refuse([&]{Stack::LocationScope exhausted(depth,19u,{1,2,3});});
    scopes.back().reset();
    {
        // Failed depth admission did NOT consume the last generation.
        Stack::Scope final(depth,23u,full);
        Require(depth.Depth()==Stack::MaximumDepth&&Same(*depth.Find(23u),full),
            "Depth refusal consumed a successful scope generation");
    }
    scopes.clear();
    Refuse([&]{Stack::Scope exhausted(depth,23u,full);});
    Require(!depth.Depth()&&!depth.FindLocation(19u),"Depth/generation refusal left active slots");
}

void RawWordsAndNoAllocation() {
    constexpr std::array<std::uint32_t,18u> words{0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x807fffffu,
        0x00800000u,0x80800000u,0x3f800000u,0xbf800000u,0x7f7fffffu,0xff7fffffu,
        0x7f800000u,0xff800000u,0x7fc12345u,0xffc54321u,0x7f812345u,0xff854321u};
    Stack stack;
    for(const auto word:words)for(unsigned channel=0u;channel<3u;++channel) {
        const auto value=std::bit_cast<float>(word);
        const std::array<float,3u> raw{value,value,value};
        {
            Stack::LocationScope scope(stack,19u,raw);
            const auto token=stack.CaptureLocationWrite(19u);
            Require(SameLocation(*stack.FindLocation(19u),raw),"Partial scope changed initial raw Float words");
            stack.WriteLocation(token,channel,std::bit_cast<float>(word^0x80000000u));
            Require(std::bit_cast<std::uint32_t>((*stack.FindLocation(19u))[channel])==(word^0x80000000u),
                "Trusted Location channel store changed raw native bits");
            Require(stack.UndoLocation(token,channel,value)&&SameLocation(*stack.FindLocation(19u),raw),
                "Raw Location undo canonicalized NaN/infinity/signed zero");
            Require(!stack.UndoLocation(token,3u,value)&&SameLocation(*stack.FindLocation(19u),raw),
                "Invalid undo channel modified a valid active slot");
        }
        Require(!stack.FindLocation(19u),"Raw partial scope did not restore durable fallback");
    }

    auto partial=std::make_unique<Stack::LocationScope>(stack,19u,std::array<float,3u>{1,2,3});
    const auto token=stack.CaptureLocationWrite(19u);
    auto full=std::make_unique<Stack::Scope>(stack,23u,Stack::Pose{{4,5,6},{7,8,9}});
    forbidPoseAllocation=true;
    bool undo{},stale{};
    try {
        stack.WriteLocation(token,0u,6.0f);
        undo=stack.UndoLocation(token,0u,-0.0f);
        partial.reset(); // Out-of-order release through an active other actor.
        stale=stack.UndoLocation(token,0u,1.0f);
        full.reset(); // Collapse the inactive fixed slots; no allocations.
    }catch(...) {forbidPoseAllocation=false;throw;}
    forbidPoseAllocation=false;
    Require(undo&&!stale&&!stack.Depth()&&!stack.FindLocation(19u)&&!stack.FindRotation(23u),
        "Native scope store/undo/release needed allocation or retained inactive fields");

    for(unsigned point=0u;point<4u;++point) {
        const Stack::Pose outer{{1,2,3},{4,5,6}};
        Stack::Scope parent(stack,23u,outer);
        try {
            Stack::LocationScope live(stack,19u,{11,12,13});
            const auto authority=stack.CaptureLocationWrite(19u);
            if(point==0u)throw std::runtime_error("before write");
            stack.WriteLocation(authority,0u,21.0f);
            if(point==1u)throw std::runtime_error("after write");
            {
                Stack::Scope query(stack,19u,{{31,32,33},{34,35,36}});
                if(point==2u)throw std::runtime_error("inside readonly query");
            }
            throw std::runtime_error("after query");
        }catch(const std::runtime_error&) {}
        Require(!stack.FindLocation(19u)&&Same(*stack.Find(23u),outer)&&stack.Depth()==1u,
            "Exceptional mutable/query restoration corrupted a surviving actor scope");
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
    PartialFields();DelayedUndoAndReuse();GenerationAndDepth();RawWordsAndNoAllocation();
}
}
int main() {
    try {Run();std::cout<<"PASS bounded native actor pose field scopes "<<checks<<" checks / "<<refusals<<" refusals.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Native actor pose scopes failed: "<<error.what()<<'\n';return 1;}
}
