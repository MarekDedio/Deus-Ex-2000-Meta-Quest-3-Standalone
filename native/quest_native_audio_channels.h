#pragma once
#include "quest_native_scalar.h"
#include "quest_audio_voice_protocol.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace QuestVr {
// Shared game-side channel semantics, not Galaxy's PE32 layout or a Windows
// output driver. The concrete Quest subsystem owns/journals this state and
// adapts final spatial samples to its actual platform output separately.
struct NativeAudioChannel {
    std::uintptr_t actor{},sound{};
    std::uint32_t id{};
    std::array<float,3u> location{};
    float volume{},radius{},pitch{},priority{},elapsed{};
    std::uint64_t voice{};
};
struct NativeAudioChannels {
    std::array<NativeAudioChannel,AudioVoiceCapacity> slots;
    std::uint32_t freeCounter{}; // Fresh default/null-template profile only.
    std::uint64_t voiceSerial{}; // Quest backend identity, NOT source sound Id.
    std::uint32_t liveCount{};
};
struct NativeAudioPriority {
    NativeScalar::Real retained;
    float stored{};
};
inline NativeAudioPriority OriginalAudioPriority(const std::array<float,3u>& source,
    const std::array<float,3u>& listener,float volume,float radius) {
    using namespace NativeScalar;
    if(!std::isfinite(volume)||!std::isfinite(radius)||radius==0.0f)
        throw std::runtime_error("Original audio priority requires its supported finite nonzero-radius profile");
    std::array<Real,3u> difference;
    for(std::size_t axis=0u;axis<3u;++axis) {
        if(!std::isfinite(source[axis])||!std::isfinite(listener[axis]))
            throw std::runtime_error("Original audio priority coordinates are not finite");
        difference[axis]=Subtract(Load(source[axis]),Load(listener[axis]));
    }
    // Galaxy10608192..81E6 /10609AA7..9B0E, Core.appSqrt10123290:
    // Double call argument, retained PC64 FSQRT result; neither diagnostic
    // Double FST nor priority Float FST narrows the live selection operand.
    const auto square=Add(Add(Multiply(difference[0u],difference[0u]),Multiply(difference[1u],difference[1u])),
        Multiply(difference[2u],difference[2u]));
    const auto root=Sqrt(StoreDouble(square));
    const auto value=Multiply(Subtract(Load(1.0),Divide(root,Load(radius))),Load(volume));
    return {value,StoreFloat(value)};
}
inline void ValidateNativeAudioChannels(const NativeAudioChannels& state) {
    if(state.liveCount>AudioVoiceCapacity)
        throw std::runtime_error("Quest native audio live count exceeds its source inventory");
}
// Stop callback only stages opaque backend voice retirement in the same VM
// root. It must never stop a real stream/voice before successful outer commit.
template<class Stop>void StopNativeAudioChannel(NativeAudioChannels& state,std::size_t slot,Stop&& stop) {
    ValidateNativeAudioChannels(state);
    if(slot>=state.liveCount)throw std::runtime_error("Native audio stop index is outside actual live count");
    const auto voice=state.slots[slot].voice;
    if(voice)stop(slot,voice);
    // Only observable semantic reset. Dead Location/Volume/Radius/Pitch do not
    // become a claimed copy of uninitialized StopSound temporary padding.
    state.slots[slot]=NativeAudioChannel{};
}
template<class Stop>bool PlayNativeAudioChannel(NativeAudioChannels& state,bool hasViewport,
    std::uintptr_t actor,std::uint32_t id,std::uintptr_t sound,const std::array<float,3u>& location,
    float volume,float radius,float pitch,const std::array<float,3u>& listener,Stop&& stop) {
    ValidateNativeAudioChannels(state);
    // Original assertion precedes the ordinary null viewport/sound exits.
    if(!std::isfinite(radius)||radius==0.0f)
        throw std::runtime_error("Native PlaySound Radius assertion/nonfinite profile is unsupported");
    if(!hasViewport||!sound)return false;
    // An explicit supported-profile guard, not a retail pitch clamp. The
    // actual ClientHearSound wrapper rejects nonfinite packets before this
    // native call; detached callers must not bypass that finite-only scope.
    if(!std::isfinite(pitch))throw std::runtime_error("Native PlaySound nonfinite pitch profile is unsupported");
    if((id&0xEu)==0u)id=(--state.freeCounter)<<4u;
    const auto priority=OriginalAudioPriority(location,listener,volume,radius);
    auto threshold=priority.retained;
    std::size_t selected=AudioVoiceCapacity;
    for(std::size_t slot=0u;slot<state.liveCount;++slot) {
        const auto& old=state.slots[slot];
        if(((old.id^id)&0xFFFFFFFEu)==0u) {
            if(id&1u)return false;
            selected=slot;break;
        }
        if(NativeScalar::Compare(179u,threshold,NativeScalar::Load(old.priority))) {
            selected=slot;threshold=NativeScalar::Load(old.priority);
        }
    }
    if(selected==AudioVoiceCapacity)return false;
    StopNativeAudioChannel(state,selected,stop);
    if(sound==std::numeric_limits<std::uintptr_t>::max())return true; // Internal stop-only sentinel, not script Object.
    auto& channel=state.slots[selected];
    channel.actor=actor;channel.id=id;channel.sound=sound;channel.location=location;
    channel.volume=volume;channel.radius=radius;channel.pitch=pitch;channel.priority=priority.stored;
    // Source selection fills state but starts no voice. Actual Update prepares
    // PCM and issues a fresh backend voice in this same logical transaction.
    return true;
}
template<class Stop>bool StopNativeAudioId(NativeAudioChannels& state,std::uint32_t id,Stop&& stop) {
    ValidateNativeAudioChannels(state);
    for(std::size_t slot=0u;slot<state.liveCount;++slot)
        if(((state.slots[slot].id^id)&0xFFFFFFFEu)==0u) {
            StopNativeAudioChannel(state,slot,stop);return true;
        }
    return false;
}
template<class Stop>void NoteNativeAudioActorDestroyed(NativeAudioChannels& state,std::uintptr_t actor,Stop&& stop) {
    ValidateNativeAudioChannels(state);
    if(!actor)throw std::runtime_error("Native audio NoteDestroy requires a nonnull valid Actor");
    for(std::size_t slot=0u;slot<state.liveCount;++slot) {
        auto& channel=state.slots[slot];
        if(channel.actor!=actor)continue;
        if((channel.id&0xEu)==8u)StopNativeAudioChannel(state,slot,stop);
        else channel.actor=0u; // Detached one-shot keeps Id/Sound/cached position/voice.
    }
}
inline std::uint64_t NextNativeAudioVoice(NativeAudioChannels& state) {
    if(state.voiceSerial>=AudioVoiceIdentityMaximum)
        throw std::runtime_error("Quest native audio backend voice identity exhausted");
    return ++state.voiceSerial;
}
} // namespace QuestVr
