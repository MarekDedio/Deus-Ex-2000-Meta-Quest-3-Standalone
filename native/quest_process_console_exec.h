#pragma once
#include "quest_core_set_command.h"

namespace QuestVr::ProcessConsoleExec {
// Core10150860..10150911. Receiver/native virtual authority belongs to Ops;
// this is only the shared scriptability/token/FindFunction/Exec selection.
// Match must run the actual command-parameter/call service or throw, not
// fabricate a handled command. Missing/non-Exec functions genuinely decline.
template<class Operations>
bool Exec(Operations& operations,std::u16string_view supplied) {
    operations.Step();if(!operations.IsScriptable())return false;
    const auto token=CoreSetCommand::ParseToken<64u>(supplied,0u,operations);
    if(!token.present)return false;
    operations.Step();const auto name=operations.FindName(token.value);
    if(name==typename Operations::Name{})return false;
    operations.Step();const auto function=operations.FindFunction(name,false);
    if(!function)return false;
    operations.Step();if(!operations.IsFunction(function))return false;
    operations.Step();if(!(operations.FunctionFlags(function)&0x200u))return false;
    operations.Match(function,token.nextUnit);return true;
}
} // namespace QuestVr::ProcessConsoleExec
