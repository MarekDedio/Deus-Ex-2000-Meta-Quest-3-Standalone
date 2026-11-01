#pragma once

#include "quest_input_key_queries.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace QuestVr {
struct OriginalInputLine256 {
    std::array<char16_t,256u> storage{};
    std::size_t length{};
    std::u16string_view Text() const {return {storage.data(),length};}
};

// Core10147E60..10147F4C, BUFFER ParseLine(256,Exact0), not ParseToken or the
// FString overload. The caller owns source storage/cursor and the root budget.
// Each selected read is charged; end-of-view represents the CString terminator.
inline bool ParseOriginalInputLine256(std::u16string_view source,std::size_t& cursor,
    OriginalInputLine256& line,const std::function<void()>& work) {
    if(!work)throw std::invalid_argument("Original Input ParseLine requires caller work");
    if(source.size()>NativeInputKeyQueries::MaximumCommandUnits||cursor>source.size())
        throw std::invalid_argument("Original Input ParseLine source/cursor exceeds admitted storage");
    const auto read=[&](std::size_t index) {
        work();return index<source.size()?source[index]:char16_t{};
    };
    line.length=0u;line.storage[0u]=0;
    bool gotStream{},quoted{},ignore{};std::size_t remaining=256u;
    for(;;) {
        const auto unit=read(cursor);
        if(!unit||unit==u'\r'||unit==u'\n')break;
        if(--remaining==0u)break; // SCANNED units, including ignored comments.
        if(!quoted) {
            if(unit==u'/'&&read(cursor+1u)==u'/')ignore=true;
            if(unit==u'|')break;
        }
        if(unit==u'"')quoted=!quoted; // No escapes; also toggles inside comments.
        gotStream=true;
        if(!ignore)line.storage[line.length++]=unit;
        ++cursor;
    }
    for(;;) {
        const auto unit=read(cursor);
        if(unit!=u'\r'&&unit!=u'\n'&&unit!=u'|')break;
        ++cursor;
    }
    line.storage[line.length]=0;
    return read(cursor)!=0||gotStream;
}

using OriginalInputExecIdentity=std::uintptr_t;
struct OriginalInputExecQueries {
    // IDs are caller-authenticated native owners/output device, not PE offsets
    // or authority invented by this coordinator. Every nested call retains the
    // same outer root, work ledger, native COW and VM undo journal.
    std::function<void(OriginalInputExecIdentity,OriginalInputExecIdentity)> requireOuterRoot;
    std::function<void()> work;
    std::function<void(std::size_t)> enterScratch,leaveScratch; // leave: noexcept/noalloc.
    std::function<bool()> running;
    std::function<std::int32_t(OriginalInputExecIdentity)> action;
    std::function<OriginalInputExecIdentity(OriginalInputExecIdentity)> inputViewport;
    std::function<bool(OriginalInputExecIdentity,OriginalInputExecIdentity,std::u16string_view)> viewportExec,inputExec;
};
namespace InputExecLinesDetail {
template<class Function,class... Args> auto Call(const OriginalInputExecQueries& q,
    const Function& function,Args&&... args) {
    if(!function)throw std::runtime_error("Original Input ExecInputCommands lacks a selected provider");
    if(!q.work)throw std::invalid_argument("Original Input ExecInputCommands requires caller work");
    q.work();return function(std::forward<Args>(args)...);
}
struct Frame {
    // Portable lexical storage profile: actual fixed line plus256 bytes for
    // cursor/dispatch/control temporaries. NOT an original PE stack-size claim.
    static constexpr std::size_t Bytes=sizeof(OriginalInputLine256)+256u;
    const OriginalInputExecQueries& q;
    explicit Frame(const OriginalInputExecQueries& queries):q(queries) {
        if(!q.leaveScratch)throw std::runtime_error("Original Input ExecInputCommands lacks scratch cleanup");
        Call(q,q.enterScratch,Bytes);
    }
    ~Frame()noexcept {q.leaveScratch(Bytes);}
};
}

// UInput.ExecInputCommands10393BF0. Genuine GIsRunning is read ONCE, Action
// reread EACH returned line. Child handled values are ignored. Source belongs
// to the caller and may be updated by a synchronous child without reallocation;
// do not replace it by a pre-split or trimmed command-list copy here.
inline void ExecuteOriginalInputCommands(OriginalInputExecIdentity input,OriginalInputExecIdentity output,
    std::u16string_view source,const OriginalInputExecQueries& q) {
    using namespace InputExecLinesDetail;
    if(!input||!output)throw std::invalid_argument("Original Input ExecInputCommands requires native Input/output identities");
    Call(q,q.requireOuterRoot,input,output);Frame frame(q);
    if(!Call(q,q.running))return;
    OriginalInputLine256 line;std::size_t stream{};
    while(ParseOriginalInputLine256(source,stream,line,q.work)) {
        std::size_t current{};
        const auto action=Call(q,q.action,input);
        bool viewport=action==1;
        if(action==3) {
            // Charge the bounded actual ParseCommand scan (including the
            // possible CString pass) before using the already audited parser.
            // No fresh work allowance is created for this or nested commands.
            for(std::size_t unit=0u;unit<=line.length;++unit)q.work();
            viewport=NativeInputKeyQueries::ParseCommand(line.Text(),current,u"OnRelease");
        }
        const auto command=line.Text().substr(current);
        if(viewport) {
            const auto owner=Call(q,q.inputViewport,input);
            if(!owner)throw std::runtime_error("Original Input ExecInputCommands selected a null native Viewport");
            static_cast<void>(Call(q,q.viewportExec,owner,output,command));
        }else static_cast<void>(Call(q,q.inputExec,input,output,command));
    }
}
} // namespace QuestVr
