#pragma once
#include "quest_input_key_queries.h"
#include <array>

namespace QuestVr::EngineExecCommands {
// Original native discriminator prefixes. Matching bodies belong to Ops;
// an unsupported match must refuse, never become an invented provider decline.
inline constexpr std::array<std::u16string_view,2u> Level{{u"DEMOREC",u"DEMOPLAY"}};
inline constexpr std::array<std::u16string_view,2u> DeusExGameEngine{{u"SAVEGAME",u"DELETEGAME"}};
inline constexpr std::array<std::u16string_view,11u> GameEngine{{
    u"OPEN",u"START",u"SERVERTRAVEL",u"SAY",u"DISCONNECT",u"RECONNECT",
    u"GETCURRENTTICKRATE",u"GETMAXTICKRATE",u"GSPYLITE",u"SAVEGAME",u"CANCEL"
}};
template<class Operations,std::size_t Count>
bool Exec(Operations& operations,std::u16string_view supplied,std::size_t& cursor,
    const std::array<std::u16string_view,Count>& commands) {
    supplied=NativeInputKeyQueries::Detail::Input(supplied);
    for(const auto unit:supplied){static_cast<void>(unit);operations.Step();}
    for(const auto command:commands) {
        operations.Step();
        if(NativeInputKeyQueries::ParseCommand(supplied,cursor,command)) {
            operations.Match(command,cursor);return true;
        }
    }
    return false;
}
} // namespace QuestVr::EngineExecCommands
