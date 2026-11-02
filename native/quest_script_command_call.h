#pragma once
#include "quest_unreal_string.h"
#include <exception>
#include <optional>
#include <stdexcept>

namespace QuestVr::ScriptCommandCall {
struct Result {bool eventOffered{},requiredFailure{},optionalFailure{},executorInjected{};
    std::size_t imported{},destroyed{},cursor{};};
template<class Ops> void ParseNext(Ops& ops,std::u16string_view text,std::size_t& cursor){
    if(cursor>text.size())throw std::runtime_error("Script command cursor reads beyond its CString terminator");
    while(cursor<text.size()){
        ops.Step();const auto ch=text[cursor];
        if(ch==u' '||ch==u'\t'||ch==u'\r'||ch==u'\n'){++cursor;continue;}
        if(ch!=u';')break;
        while(cursor<text.size()&&text[cursor]!=u'\r'&&text[cursor]!=u'\n'){ops.Step();++cursor;}
    }
}
// Ops retains the actual declaration, owns the zero packet and supplies reached
// selected ImportText/ProcessEvent/DestroyValue services in the SAME outer root.
template<class Ops> Result Call(Ops& ops,std::u16string_view supplied,std::size_t cursor){
    const auto text=UnrealString::CString(supplied);ops.EnterFrame(512u);
    struct Leave {Ops& ops;~Leave()noexcept{ops.LeaveFrame(512u);}} leave{ops};
    auto packet=ops.ZeroPacket();Result result;result.cursor=cursor;std::exception_ptr primary,cleanup;
    try{
        ops.Validate(packet);bool first=true,call=true;
        for(auto property=ops.FirstProperty(packet);property;property=ops.NextProperty(packet,property)){
            ops.Step();if((ops.PropertyFlags(packet,property)&0x480u)!=0x80u)break;ops.Validate(packet);
            if(first&&ops.IsObjectProperty(packet,property)&&ops.HasExecutor()&&ops.ExecutorCompatible(packet,property)){
                ops.InjectExecutor(packet,property);result.executorInjected=true;
            }else{
                ParseNext(ops,text,result.cursor);const auto next=ops.ImportText(packet,property,text,result.cursor,1u);ops.Validate(packet);
                if(!next){
                    if(ops.PropertyFlags(packet,property)&0x10u)result.optionalFailure=true;
                    else {ops.BadProperty(packet,property);result.requiredFailure=true;call=false;}
                    break;
                }
                // UObject ImportText may return one past NUL. A later read is
                // refused by ParseNext; a final returned pointer is not clamped.
                if(*next<result.cursor||*next>text.size()+1u)throw std::runtime_error("Script command ImportText returned an invalid cursor");
                result.cursor=*next;++result.imported;
            }
            first=false;ops.Validate(packet);
        }
        if(call){ops.Validate(packet);ops.ProcessEvent(packet);result.eventOffered=true;ops.Validate(packet);}
    }catch(...){primary=std::current_exception();}
    // The retained cleanup plan is safe with an exhausted work ledger. Selected
    // packet-owned destruction must finish without allocating on unwind.
    ops.BeginCleanup(packet,bool(primary));
    for(auto property=ops.FirstProperty(packet);property;property=ops.NextProperty(packet,property)){
        if((ops.PropertyFlags(packet,property)&0x480u)!=0x80u)break;
        try{ops.DestroyValue(packet,property,bool(primary||cleanup));++result.destroyed;}
        catch(...){if(!cleanup)cleanup=std::current_exception();try{ops.DestroyValue(packet,property,true);}catch(...){}}
    }
    ops.EndCleanup(packet);
    if(primary&&cleanup)throw std::runtime_error("Script command execution and selected parameter cleanup both failed");
    if(primary)std::rethrow_exception(primary);
    if(cleanup)std::rethrow_exception(cleanup);
    return result;
}
} // namespace QuestVr::ScriptCommandCall
