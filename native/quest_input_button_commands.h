#pragma once

#include "quest_core_set_command.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace QuestVr {
// A caller-authenticated native address expressed as its actual Actor and
// declaring ByteProperty, NOT a field name, PE offset or independent mirror.
// Both zero means genuine FindButtonName nullptr. Partial identities refuse.
struct OriginalInputButtonCell {
    std::uintptr_t actor{},property{};
    bool operator==(const OriginalInputButtonCell&) const = default;
};
struct OriginalInputButtonQueries {
    std::function<void()> requireOuterRoot,work;
    // Admit an ADDITIONAL simultaneous lexical reservation before allocating.
    // leaveScratch must be noexcept/noalloc/unmetered. Failed enterScratch
    // must leave the caller's ledger unchanged.
    std::function<void(std::size_t)> enterScratch,leaveScratch;
    // Reread actual Input.Viewport.Actor each call. Missing/stale Viewport is
    // a refusal, not genuine Actor=None. No initialized-state guess is added.
    std::function<std::uintptr_t()> viewportActor;
    // Implements actual FNAME_Find, flags4 inherited UProperty cache/order,
    // ByteProperty ancestry and one index0 address. Wrong-type same-name
    // candidates are SKIPPED. The result retains the supplied actual Actor.
    std::function<OriginalInputButtonCell(std::uintptr_t,std::u16string_view)> findButton;
    std::function<std::int32_t()> action;
    std::function<std::uint8_t(OriginalInputButtonCell)> readByte;
    std::function<void(OriginalInputButtonCell,std::uint8_t)> writeByte;
    // The ACTUAL supplied command output's Log(CString), no invented sink,
    // formatting, newline or exception-to-warning fallback.
    std::function<void(std::u16string_view)> log;
    // Optional complete typed AXIS branch provider, selected before later
    // key queries/aliases. It owns the actual source-proven actor/token/
    // Float lookup/SPEED/arithmetic/Logf path and SAME command output/root.
    // A missing provider REFUSES; no numeric/no-op substitute lives here.
    std::function<void(std::u16string_view,std::size_t)> axis;
};

namespace InputButtonCommandsDetail {
inline constexpr std::size_t FrameBytes=256u;
struct Context {
    const OriginalInputButtonQueries& q;
    std::size_t reserved{};
    void Step() {
        if(!q.work)throw std::invalid_argument("Original Input action requires borrowed root work");
        q.work();
    }
    template<class Function,class... Arguments>
    auto Call(const Function& function,Arguments&&... arguments) {
        if(!function)throw std::invalid_argument("Original Input action lacks a reached typed provider");
        Step();return function(std::forward<Arguments>(arguments)...);
    }
    // ParseToken receives a complete helper-owned footprint, not a fresh
    // budget. Keep it admitted through Find/Action/Log/store and recursive AXIS.
    void Admit(std::size_t peak) {
        if(peak<=reserved)return;
        Call(q.enterScratch,peak-reserved);reserved=peak;
    }
    ~Context()noexcept {if(reserved)q.leaveScratch(reserved);}
};
inline bool Parse(Context& context,std::u16string_view input,std::size_t& cursor,std::u16string_view word) {
    context.Step();return NativeInputKeyQueries::ParseCommand(input,cursor,word);
}
inline void Bad(Context& context,bool toggle) {
    context.Call(context.q.log,toggle?std::u16string_view{u"Bad Toggle command"}:
        std::u16string_view{u"Bad Button command"});
}
} // namespace InputButtonCommandsDetail

// Original UInput.Exec10393E10..1039409D selected prefix. False means NONE of
// BUTTON/PULSE/TOGGLE/AXIS matched, allowing the original later key/alias tail.
// Recognized bad/no-store commands return true. The owning runtime journals
// every callback store; exceptions retain prefix writes for that OUTER root.
// This helper does not supply native Class/Input/Actor/output authority.
inline bool ExecuteOriginalInputButtonCommands(std::u16string_view supplied,const OriginalInputButtonQueries& q) {
    using namespace InputButtonCommandsDetail;
    if(!q.leaveScratch)throw std::invalid_argument("Original Input action requires lexical scratch cleanup");
    Context context{q};context.Call(q.requireOuterRoot);context.Admit(FrameBytes);
    const auto input=CoreSetCommand::Detail::Input(supplied,context);
    std::size_t cursor{};enum class Command {Button,Pulse,Toggle};Command command{};
    if(Parse(context,input,cursor,u"BUTTON"))command=Command::Button;
    else if(Parse(context,input,cursor,u"PULSE"))command=Command::Pulse;
    else if(Parse(context,input,cursor,u"TOGGLE"))command=Command::Toggle;
    else if(Parse(context,input,cursor,u"AXIS")) {
        context.Call(q.axis,input,cursor);return true;
    }else return false;
    const bool toggle=command==Command::Toggle;
    if(!context.Call(q.viewportActor)) {Bad(context,toggle);return true;}
    const auto token=CoreSetCommand::ParseToken256(input,cursor,context,FrameBytes,false);
    if(!token.present) {Bad(context,toggle);return true;}
    const auto actor=context.Call(q.viewportActor); // AFTER actual ParseToken.
    if(!actor)throw std::runtime_error("Original Input FindButtonName requires the actual post-token Actor");
    const auto cell=context.Call(q.findButton,actor,std::u16string_view{token.value});
    if(!cell.actor&&!cell.property) {Bad(context,toggle);return true;}
    if(!cell.actor||!cell.property||cell.actor!=actor)
        throw std::runtime_error("Original Input FindButtonName returned a partial or foreign Actor address");
    const auto action=context.Call(q.action); // Not cached before lookup.
    if(command==Command::Button) {
        if(action==1)context.Call(q.writeByte,cell,std::uint8_t{1u});
        else if(action==3&&context.Call(q.readByte,cell)!=0u)
            context.Call(q.writeByte,cell,std::uint8_t{0u});
    }else if(command==Command::Pulse) {
        if(action==1)context.Call(q.writeByte,cell,std::uint8_t{1u});
    }else if(action==1) {
        const auto value=context.Call(q.readByte,cell);
        context.Call(q.writeByte,cell,static_cast<std::uint8_t>(value^0x80u));
    }
    return true;
}
} // namespace QuestVr
