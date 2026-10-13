#pragma once
#include "portable_unreal_runtime.h"
#include "portable_model_geometry.h"
#include "quest_music_stream.h"
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace QuestVr {
struct MapMusicPreparation {
    std::string actorPath,songPath,format,error;
    std::uint8_t section{};
    std::shared_ptr<MusicStream> stream;
};
// Runtime-owning CPU worker only. Select the actual slot-zero LevelInfo's
// typed Song/SongSection, not a map-name soundtrack heuristic. Dynamic player
// music events/combat sections remain separate original gameplay work.
inline MapMusicPreparation PrepareOriginalMapMusic(const std::string& root,
    const std::string& mapName,std::uint32_t sampleRate) {
    MapMusicPreparation result;
    try {
        const auto safe=[](const std::string& text) {
            return !text.empty() && text.size()<=128u &&
                text.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")==std::string::npos;
        };
        if (!safe(mapName)) throw std::runtime_error("Music map identity is invalid");
        const auto map=LoadPortablePackageTables((std::filesystem::path(root)/"Maps"/(mapName+".dx")).string());
        const auto order=ReadPortableLevel68ActorOrder(map);
        if (order.empty() || order.front()<=0 || static_cast<std::size_t>(order.front())>map.exports.size())
            throw std::runtime_error("Map music has no actual slot-zero LevelInfo");
        result.actorPath=std::filesystem::path(map.sourcePath).stem().string()+'.'+GetPortableObjectPath(map,order.front());
        const auto song=ReadPortableActorDeclaredScriptProperty(result.actorPath,"Engine.LevelInfo.Song");
        const auto section=ReadPortableActorDeclaredScriptProperty(result.actorPath,"Engine.LevelInfo.SongSection");
        if (song.kind!=Vm::Kind::Object || section.kind!=Vm::Kind::Byte)
            throw std::runtime_error("Map Song/SongSection are not authored Object/Byte fields");
        if (section.integer<0 || section.integer>255)
            throw std::runtime_error("Authored SongSection is outside the Byte range");
        result.section=static_cast<std::uint8_t>(section.integer);
        if (song.text.empty()) return result; // Actual None means silence.
        const auto dot=song.text.find('.');
        if (dot==std::string::npos || !safe(song.text.substr(0u,dot)) || song.text.size()>4096u ||
            song.text.find('\0')!=std::string::npos)
            throw std::runtime_error("Authored Song identity is invalid");
        result.songPath=song.text;
        const auto source=LoadPortablePackageTables((std::filesystem::path(root)/"Music"/(song.text.substr(0u,dot)+".umx")).string());
        auto music=LoadPortableMusic(source,FindPortableExport(source,song.text.substr(dot+1u)));
        result.format=music.format;
        result.stream=std::make_shared<MusicStream>(std::move(music.data),sampleRate,result.section);
    } catch (const std::exception& error) {result.error=error.what();result.stream.reset();}
    return result;
}
}
