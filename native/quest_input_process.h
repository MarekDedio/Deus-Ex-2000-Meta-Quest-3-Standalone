#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace QuestVr {
// Exact selected coordinators, not a source of native Class/Viewport/Root
// authority. The actual runtime host authenticates those owners, journals all
// writes and supplies the same outer root's work/scratch and command services.
struct OriginalInputProcessQueries {
    std::function<void()> work;
    std::function<void(std::size_t)> enterFrame,leaveFrame; // leave: noexcept/noalloc/unmetered.
    std::function<bool()> hasRoot;
    std::function<bool(std::int32_t,std::int32_t,float)> rootProcess;
    std::function<std::uint8_t(std::int32_t)> key;
    std::function<void(std::int32_t,std::uint8_t)> writeKey;
    std::function<bool(std::int32_t)> bindingNonempty;
    std::function<void(std::int32_t)> writeAction;
    std::function<void(float)> writeDelta;
    // Select the CURRENT binding after Action/Delta stores. The actual host
    // owns command capture/admission/ExecInputCommands; no guessed Exec success.
    std::function<void(std::int32_t)> execBinding;
};
struct OriginalRootInputQueries {
    std::function<void()> work;
    std::function<void(std::size_t)> enterFrame,leaveFrame;
    std::function<std::uint8_t(std::int32_t)> keyDown;
    std::function<void(std::int32_t,std::uint8_t)> writeKeyDown;
    std::function<std::int32_t()> mouseReferences,keyboardReferences;
    std::function<bool()> hasParentPawn;
    // Each provider rereads the actual parentPawn, as the original body does
    // between the two writes; no captured/fabricated parent mirror.
    std::function<void()> clearParentFire,clearParentAltFire;
    std::function<void(std::int32_t,std::int32_t)> handleButtons;
    std::function<void(std::int32_t,std::int32_t,bool)> handleVirtualKeyboard;
};
namespace InputProcessDetail {
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original input Process: ")+reason);
}
inline void Key(std::int32_t key) {if(key<0||key>=255)Fail("key outside actual255-key domain");}
template<class Queries,class Function,class... Args>
inline auto Call(const Queries& q,const Function& function,Args&&... arguments) {
    if(!function)Fail("selected service is missing");
    q.work();return function(std::forward<Args>(arguments)...);
}
struct Frame {
    const std::function<void(std::size_t)>& leave;
    static constexpr std::size_t Bytes=256u;
    template<class Queries> explicit Frame(const Queries& q):leave(q.leaveFrame) {
        if(!q.work||!q.enterFrame||!q.leaveFrame)Fail("requires caller work/frame services");
        q.enterFrame(Bytes);
    }
    ~Frame()noexcept {leave(Bytes);}
};
inline bool MouseButton(std::int32_t key) noexcept {
    return key==1||key==2||key==4||(key>=228&&key<=231)||key==236||key==237;
}
} // namespace InputProcessDetail

// Extension.dll XRootWindow::Process1003B3E0. Delta is an input argument but
// is not read by this body. It is NOT the later HandleMouse/axis service.
inline bool ProcessOriginalRootInput(std::int32_t key,std::int32_t action,float delta,
    const OriginalRootInputQueries& q) {
    using namespace InputProcessDetail;Key(key);Frame frame(q);static_cast<void>(delta);
    bool duplicatePress{},duplicateRelease{};
    if(action==1) {
        duplicatePress=Call(q,q.keyDown,key)!=0u;
        Call(q,q.writeKeyDown,key,std::uint8_t{1});
    }else if(action==3) {
        duplicateRelease=Call(q,q.keyDown,key)==0u;
        Call(q,q.writeKeyDown,key,std::uint8_t{0});
    }
    if(MouseButton(key)) {
        if(Call(q,q.mouseReferences)==0)return false;
        if(Call(q,q.hasParentPawn)) {
            Call(q,q.clearParentFire);
            Call(q,q.clearParentAltFire);
        }
        if(!duplicateRelease)Call(q,q.handleButtons,key,action);
    }else {
        if(Call(q,q.keyboardReferences)==0)return false;
        if(!duplicateRelease)Call(q,q.handleVirtualKeyboard,key,action,duplicatePress);
    }
    // Handler return values do not determine this result: the selected grab
    // reference makes Root consume even an ignored duplicate-release event.
    return true;
}

// Extension.dll XInputExt::Process1002D0B0. No internal transaction/repair:
// command exceptions retain original prefix writes for the owning root to undo.
inline bool ProcessOriginalInputExt(std::int32_t key,std::int32_t action,float delta,
    const OriginalInputProcessQueries& q) {
    using namespace InputProcessDetail;Key(key);Frame frame(q);
    if(Call(q,q.hasRoot)&&Call(q,q.rootProcess,key,action,delta)) {
        for(std::int32_t current=0;current<255;++current) {
            if(Call(q,q.key,current)==0u)continue;
            Call(q,q.writeKey,current,std::uint8_t{0});
            if(!Call(q,q.bindingNonempty,current))continue;
            Call(q,q.writeAction,std::int32_t{3});Call(q,q.writeDelta,0.0f);
            Call(q,q.execBinding,current);
            Call(q,q.writeAction,std::int32_t{0});Call(q,q.writeDelta,0.0f);
        }
        return true;
    }
    if(action==1) {
        if(Call(q,q.key,key)!=0u)return false;
        Call(q,q.writeKey,key,std::uint8_t{1});
    }else if(action==3) {
        if(Call(q,q.key,key)==0u)return false;
        Call(q,q.writeKey,key,std::uint8_t{0});
    }
    if(!Call(q,q.bindingNonempty,key))return false;
    Call(q,q.writeAction,action);Call(q,q.writeDelta,delta);
    Call(q,q.execBinding,key);
    Call(q,q.writeAction,std::int32_t{0});Call(q,q.writeDelta,0.0f);
    return true;
}
} // namespace QuestVr
