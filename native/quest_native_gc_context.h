#pragma once

#include <array>
#include <bit>
#include <cstdint>

namespace QuestVr::NativeGcContext {

// Original Extension.dll10027760..100278C6. These are semantic property
// identities, not Win32 offsets and not an assignment of the whole UObject.
// The adapter authenticates each actual declaration and preserves raw scalar,
// Color/Plane/ClipRect words, object references and unrelated packed flag bits.
inline constexpr std::array<const char*,27u> ContextFields{
    "Canvas","gcClipRect","Style","bSmoothed","bDrawEnabled","bMasked",
    "bTranslucent","bModulated","bTextTranslucent","PolyFlags","textPolyFlags",
    "tileColor","tilePlane","TextColor","textPlane","normalFont","boldFont",
    "underlineTexture","underlineHeight","baselineOffset","textVSpacing",
    "HAlign","VAlign","bWordWrap","bParseMetachars","hMultiplier","vMultiplier"};

// Real machine-word increment/decrement, including INT_MAX/INT_MIN rollover.
// Neither signed overflow UB nor an invented minimum-one count is appropriate.
inline std::int32_t Increment(const std::int32_t value) noexcept {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value)+1u);
}
inline std::int32_t Decrement(const std::int32_t value) noexcept {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value)-1u);
}

// Operations owns the actual authenticated objects and enclosing VM/root.
// Every Step shares that root's native-work budget. Every write/allocation
// participates in its existing journal/admission; exceptions propagate without
// local cleanup, compensating pops, fabricated contexts or a second journal.
// Node{} is None. ReadContextField returns a detached value for that one field;
// WriteContextField does not normalize it or call public style/color setters.
template<class Operations>
void Copy(Operations& operations,typename Operations::Node destination,
    typename Operations::Node source) {
    for(const auto* field:ContextFields) {
        operations.Step();const auto value=operations.ReadContextField(source,field);
        operations.Step();operations.WriteContextField(destination,field,value);
    }
}

template<class Operations>
std::int32_t Push(Operations& operations,typename Operations::Node receiver) {
    using Node=typename Operations::Node;
    operations.Step();const auto owner=operations.ReadOwner(receiver);
    operations.Step();auto result=operations.ReadCount(receiver);
    if(owner!=Node{}) return result;
    operations.Step();auto node=operations.ReadFree(receiver);
    if(node!=Node{}) {
        operations.Step();const auto next=operations.ReadStack(node);
        operations.Step();operations.WriteFree(receiver,next);
        operations.Step();operations.CopyContext(node,receiver);
    } else {
        // The provider must run the copying ctor on a genuine GC allocation
        // with Outer=receiver. A default ctor followed by Copy is not this path.
        operations.Step();node=operations.AllocateCopy(receiver);
    }
    if(node==Node{}) return result; // Original failed native allocation branch.
    operations.Step();operations.WriteOwner(node,receiver);
    operations.Step();operations.WriteFree(node,Node{});
    operations.Step();operations.WriteFreeFlag(node,false);
    operations.Step();const auto savedCount=operations.ReadCount(receiver);
    operations.Step();operations.WriteCount(node,savedCount);
    operations.Step();const auto previous=operations.ReadStack(receiver);
    operations.Step();operations.WriteStack(node,previous);
    // Original rereads the live count after saving node fields.
    operations.Step();result=operations.ReadCount(receiver);
    operations.Step();operations.WriteStack(receiver,node);
    operations.Step();operations.WriteCount(receiver,Increment(result));
    return result;
}

// Literal PopGC(Int), not the script wrapper's special -1 argument policy.
template<class Operations>
void PopTo(Operations& operations,typename Operations::Node receiver,const std::int32_t target) {
    using Node=typename Operations::Node;
    operations.Step();const auto current=operations.ReadCount(receiver);
    if(target>=current) return;
    operations.Step();const auto owner=operations.ReadOwner(receiver);
    if(owner!=Node{}) return;
    Node last{};
    bool matched{};
    while(!matched) {
        operations.Step();const auto node=operations.ReadStack(receiver);
        if(node==Node{}) break;
        operations.Step();const auto count=operations.ReadCount(node);
        if(count<target) break;
        matched=count==target;
        last=node;
        operations.Step();const auto next=operations.ReadStack(node);
        operations.Step();operations.WriteStack(receiver,next);
        operations.Step();const auto savedCount=operations.ReadCount(node);
        operations.Step();operations.WriteCount(receiver,savedCount);
        operations.Step();const auto freeHead=operations.ReadFree(receiver);
        auto freeCount=std::int32_t{1};
        if(freeHead!=Node{}) {
            operations.Step();freeCount=Increment(operations.ReadCount(freeHead));
        }
        operations.Step();operations.WriteCount(node,freeCount);
        operations.Step();operations.WriteOwner(node,receiver);
        operations.Step();operations.WriteFreeFlag(node,true);
        // Retail rereads gcFree after owner/free-bit stores.
        operations.Step();const auto previousFree=operations.ReadFree(receiver);
        operations.Step();operations.WriteStack(node,previousFree);
        operations.Step();operations.WriteFree(node,Node{});
        operations.Step();operations.WriteFree(receiver,node);
    }
    if(last!=Node{}) {
        operations.Step();operations.CopyContext(receiver,last);
    }
}

template<class Operations>
void Pop(Operations& operations,typename Operations::Node receiver) {
    using Node=typename Operations::Node;
    operations.Step();const auto head=operations.ReadStack(receiver);
    std::int32_t target{};
    if(head!=Node{}) {
        operations.Step();target=operations.ReadCount(head);
    } else {
        operations.Step();target=Decrement(operations.ReadCount(receiver));
    }
    PopTo(operations,receiver,target);
}

// Native1272 initializes its optional INT to-1; explicit-1 has the same effect
// as omission. The host's VM argument seed must match, not invent zero.
template<class Operations>
void PopScript(Operations& operations,typename Operations::Node receiver,const std::int32_t argument=-1) {
    if(argument==-1) Pop(operations,receiver);
    else PopTo(operations,receiver,argument);
}

} // namespace QuestVr::NativeGcContext
