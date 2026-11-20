#pragma once
#include "quest_portable_vm.h"
#include <cstdint>
#include <string>
#include <vector>

struct PortableNativeAudioChannelSnapshot {
    std::uint32_t slot{},id{};
    std::uint64_t voice{};
    std::string actorPath,soundPath;
    float priority{},elapsed{};
};
struct PortableNativeAudioSnapshot {
    bool prepared{},initialized{},ready{},disabled{},retired{};
    std::string classPath,audioPath,enginePath,viewportPath,configSource;
    std::uint32_t classIndex{},audioIndex{},baseDefaultIndex{},defaultIndex{},liveChannels{},freeCounter{};
    std::uint64_t lookupGeneration{},outputGeneration{},consumerEpoch{},voiceSerial{},updates{};
    double previousAudioTime{};
    std::size_t resourceCount{},referenceBytes{},pcmBytes{};
    std::vector<PortableNativeAudioChannelSnapshot> channels;
};
// Explicit Quest concrete provider (not the abstract Engine.AudioSubsystem and
// not a Galaxy DLL impersonation). Requires the real bound output/consumer,
// actual Engine/Client/Viewport ownership and matching read-only engine INI.
PortableNativeAudioSnapshot PreparePortableRuntimeQuestAudioSubsystem(const std::string& engineIniPath);
PortableNativeAudioSnapshot ReadPortableRuntimeQuestAudioSubsystem();
// These are committing engine adapter operations, with their own shared VM
// root. Internal callers already in a root use its native methods instead.
QuestVr::Vm::Result SetPortableRuntimeQuestAudioViewport(std::uint64_t identity,
    const QuestVr::Vm::Limits& limits={});
QuestVr::Vm::Result UpdatePortableRuntimeQuestAudio(const QuestVr::Vm::Limits& limits={});
