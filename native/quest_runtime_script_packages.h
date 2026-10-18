#pragma once

#include <array>

namespace QuestVr {

// Exact shared script-source set used by the Quest bootstrap. This is not a
// claim that every campaign map is integrated or a proven minimal Training
// dependency closure. Keep production and owned startup-scope tests identical.
inline constexpr std::array<const char*,38u> RuntimeScriptPackages{{
    "ConSys",
    "Core",
    "DeusEx",
    "DeusExCharacters",
    "DeusExConAudioAIBarks",
    "DeusExConAudioEndGame",
    "DeusExConAudioHK_Shared",
    "DeusExConAudioIntro",
    "DeusExConAudioMission00",
    "DeusExConAudioMission01",
    "DeusExConAudioMission02",
    "DeusExConAudioMission03",
    "DeusExConAudioMission04",
    "DeusExConAudioMission05",
    "DeusExConAudioMission08",
    "DeusExConAudioMission09",
    "DeusExConAudioMission10",
    "DeusExConAudioMission11",
    "DeusExConAudioMission12",
    "DeusExConAudioMission14",
    "DeusExConAudioMission15",
    "DeusExConAudioNYShared",
    "DeusExConText",
    "DeusExConversations",
    "DeusExDeco",
    "DeusExItems",
    "DeusExSounds",
    "DeusExText",
    "DeusExUI",
    "Editor",
    "Engine",
    "Extension",
    "Fire",
    "IpDrv",
    "IpServer",
    "MPCharacters",
    "UBrowser",
    "UWindow"
}};

} // namespace QuestVr
