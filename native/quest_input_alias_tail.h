#pragma once
#include "quest_core_set_command.h"

namespace QuestVr::NativeInputAliasTail {
// UInput.Exec10394132..1039422A, only after the earlier action/key branches
// genuinely declined. The callback is ExecInputCommands(+7C), NOT Exec.
// Ops supplies the actual current forty aliases, FNAME_Find (no interning),
// shared owner work/admission and the process-wide synchronous recursion guard.
template<class Operations> bool Exec(Operations& operations,std::u16string_view supplied) {
    operations.Step();auto& recursing=operations.AliasGuard();
    if(recursing)return false;
    const auto token=CoreSetCommand::ParseToken256(supplied,0u,operations,0u,false);
    if(!token.present)return false;
    const auto name=operations.FindAliasName(token.value);
    if(name==typename Operations::AliasName{})return false;
    for(std::size_t index=0u;index<40u;++index) {
        operations.Step();if(operations.AliasAt(index)!=name)continue;
        struct Guard {
            bool& value;explicit Guard(bool& flag):value(flag){value=true;}
            ~Guard(){value=false;}
        } guard(recursing);
        // A matching alias is handled even if its command list is empty or
        // its individual commands return unhandled. Exceptions still propagate.
        operations.ExecInputCommands(index);return true;
    }
    return false;
}
} // namespace QuestVr::NativeInputAliasTail
