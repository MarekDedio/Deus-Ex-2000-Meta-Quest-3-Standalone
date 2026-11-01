#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {
using InputInitializeNativeId=std::uintptr_t;
enum class OriginalInputResetPropertyKind {Byte,Float};

// UInput.Init10393D30 and ResetInput10394910..10394A7E. IDs designate the
// caller's actual native objects/fields, not PE32 pointers or guessed offsets.
// The caller authenticates configured override inheritance, declarations,
// owner/class identity, mutation leases and its COMPLETE outer undo journal.
struct OriginalInputInitializeQueries {
    std::function<void()> requireOuterRoot;
    std::function<void()> work;
    std::function<void(std::size_t)> enterScratch;
    // Scope cleanup MUST be nonthrowing/noalloc and cannot charge work that
    // might fail. Only the admitted simultaneous lexical frame is released.
    std::function<void(std::size_t)> leaveScratch;
    std::function<InputInitializeNativeId(InputInitializeNativeId)> inputViewport;
    std::function<void(InputInitializeNativeId,InputInitializeNativeId)> writeInputViewport;
    std::function<void(InputInitializeNativeId,std::size_t,std::uint8_t)> writeKey;
    std::function<InputInitializeNativeId(InputInitializeNativeId)> viewportActor;
    std::function<InputInitializeNativeId(InputInitializeNativeId)> actorClass;
    std::function<InputInitializeNativeId(InputInitializeNativeId)> classChildren,classSuper,fieldNext;
    std::function<bool(InputInitializeNativeId,OriginalInputResetPropertyKind)> propertyIsA;
    std::function<std::uint64_t(InputInitializeNativeId)> propertyFlags;
    // Exactly ONE Byte or DWORD store at the admitted field's original offset;
    // no ArrayDim loop, Bool packing, Int clearing or name-based substitution.
    std::function<void(InputInitializeNativeId,InputInitializeNativeId,OriginalInputResetPropertyKind)> clearProperty;
    std::function<void(InputInitializeNativeId,std::int32_t)> writeAction;
    std::function<void(InputInitializeNativeId,float)> writeDelta;
    std::function<void(InputInitializeNativeId,std::int32_t)> updateInput;
    // Must log using the actual current viewport's Name, after Reset succeeds.
    std::function<void(InputInitializeNativeId)> initializedLog;
};

namespace InputInitializeDetail {
inline constexpr std::size_t MaximumVisits=65'536u;
inline constexpr std::size_t FrameBytes=256u;
struct Context {
    const OriginalInputInitializeQueries& q;
    std::size_t maximum{},visits{};
    void Step() {
        if(visits>=maximum)throw std::runtime_error("Original Input initialization work bound exceeded");
        ++visits;
        if(!q.work)throw std::invalid_argument("Original Input initialization requires borrowed root work");
        q.work();
    }
    template<class Function,class... Arguments>
    auto Call(const Function& function,Arguments&&... arguments) {
        if(!function)throw std::invalid_argument("Original Input initialization lacks a reached provider");
        Step();return function(std::forward<Arguments>(arguments)...);
    }
};
struct Frame {
    const OriginalInputInitializeQueries& q;
    explicit Frame(Context& context):q(context.q) {
        if(!q.leaveScratch)throw std::invalid_argument("Original Input initialization requires lexical scratch cleanup");
        context.Call(q.enterScratch,FrameBytes);
    }
    ~Frame()noexcept {q.leaveScratch(FrameBytes);}
};
inline Context Begin(const OriginalInputInitializeQueries& q,std::size_t maximum) {
    if(!maximum||maximum>MaximumVisits)
        throw std::invalid_argument("Original Input initialization has invalid tightened work bound");
    Context context{q,maximum,0u};context.Call(q.requireOuterRoot);return context;
}
inline InputInitializeNativeId Viewport(Context& context,InputInitializeNativeId input) {
    const auto viewport=context.Call(context.q.inputViewport,input);
    if(!viewport)throw std::runtime_error("Original Input ResetInput requires its actual nonnull native Viewport");
    return viewport;
}
inline InputInitializeNativeId Actor(Context& context,InputInitializeNativeId input) {
    const auto actor=context.Call(context.q.viewportActor,Viewport(context,input));
    if(!actor)throw std::runtime_error("Original Input ResetInput requires its actual viewport Actor");
    return actor;
}
inline void ClearFields(Context& context,InputInitializeNativeId input,OriginalInputResetPropertyKind kind) {
    // Reset starts separate iterators: Float does NOT borrow Byte's captured
    // Actor/Class. Selected stores reread Input.Viewport.Actor yet again.
    auto cls=context.Call(context.q.actorClass,Actor(context,input));
    auto field=cls?context.Call(context.q.classChildren,cls):InputInitializeNativeId{};
    while(cls) {
        while(field) {
            if(context.Call(context.q.propertyIsA,field,kind)&&
                (context.Call(context.q.propertyFlags,field)&std::uint64_t{4}))
                context.Call(context.q.clearProperty,Actor(context,input),field,kind);
            // Read current Next AFTER the selected store/callback returns.
            field=context.Call(context.q.fieldNext,field);
        }
        cls=context.Call(context.q.classSuper,cls);
        if(cls)field=context.Call(context.q.classChildren,cls);
    }
}
inline void Reset(InputInitializeNativeId input,Context& context) {
    Frame frame(context);
    static_cast<void>(Viewport(context,input)); // Native assertion precedes ALL stores.
    for(std::size_t key=0;key<255u;++key)context.Call(context.q.writeKey,input,key,std::uint8_t{});
    ClearFields(context,input,OriginalInputResetPropertyKind::Byte);
    ClearFields(context,input,OriginalInputResetPropertyKind::Float);
    context.Call(context.q.writeAction,input,std::int32_t{});
    context.Call(context.q.writeDelta,input,0.0f);
    // No no-op fallback and no success publication before this provider returns.
    context.Call(context.q.updateInput,Viewport(context,input),std::int32_t{1});
}
} // namespace InputInitializeDetail

inline void ResetOriginalInput(InputInitializeNativeId input,const OriginalInputInitializeQueries& q,
    std::size_t maximumVisits=InputInitializeDetail::MaximumVisits) {
    if(!input)throw std::invalid_argument("Original Input ResetInput requires an actual Input receiver");
    auto context=InputInitializeDetail::Begin(q,maximumVisits);
    InputInitializeDetail::Reset(input,context);
}
inline void InitializeOriginalInput(InputInitializeNativeId input,InputInitializeNativeId viewport,
    const OriginalInputInitializeQueries& q,std::size_t maximumVisits=InputInitializeDetail::MaximumVisits) {
    if(!input)throw std::invalid_argument("Original Input.Init requires an actual Input receiver");
    auto context=InputInitializeDetail::Begin(q,maximumVisits);
    InputInitializeDetail::Frame frame(context);
    context.Call(q.writeInputViewport,input,viewport);
    InputInitializeDetail::Reset(input,context);
    context.Call(q.initializedLog,InputInitializeDetail::Viewport(context,input));
}
} // namespace QuestVr
