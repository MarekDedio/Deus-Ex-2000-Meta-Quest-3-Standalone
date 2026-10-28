#pragma once
#include "quest_input_key_queries.h"
#include <array>

namespace QuestVr::ViewportExecCommands {
// UViewport.Exec10373AE5..10374267, before optional Console. A recognized
// branch must execute its own service (or refuse); it cannot reach UPlayer.
inline constexpr std::array<std::u16string_view,11u> Commands{{
    u"GETPING",u"INJECT",u"NETSPEED",u"LANSPEED",u"SHOWALL",u"REPORT",u"SHOT",
    u"SHOWACTORS",u"HIDEACTORS",u"RMODE",u"EXEC"
}};
template<class Operations>
bool Exec(Operations& operations,std::u16string_view supplied,std::size_t& cursor) {
    supplied=NativeInputKeyQueries::Detail::Input(supplied);
    for(const auto unit:supplied){static_cast<void>(unit);operations.Step();}
    for(const auto command:Commands) {
        operations.Step();
        if(NativeInputKeyQueries::ParseCommand(supplied,cursor,command)) {
            operations.Match(command,cursor);return true;
        }
    }
    return false;
}
} // namespace QuestVr::ViewportExecCommands
