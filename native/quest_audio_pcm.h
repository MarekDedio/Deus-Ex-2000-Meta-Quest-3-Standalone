#pragma once
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace QuestVr {
inline constexpr std::size_t MaximumAmbientWaveBytes=64u*1024u*1024u;
inline constexpr std::size_t MaximumAmbientStereoFrames=8u*1024u*1024u;
struct PreparedAmbientPcm {
    bool passed{};
    std::uint32_t sourceRate{},targetRate{},sourceChannels{},sourceBits{};
    std::vector<std::int16_t> stereo;
};
// Optional legacy ambience WAV -> bounded actual-output-rate stereo. No file,
// NameString, global runtime, thread, device or callback access.
inline PreparedAmbientPcm PrepareAmbientWavePcm(std::span<const std::uint8_t> bytes,
    std::uint32_t targetRate) {
    PreparedAmbientPcm result;result.targetRate=targetRate;
    if(bytes.size()<12u||bytes.size()>MaximumAmbientWaveBytes||targetRate<8'000u||targetRate>192'000u||
        std::memcmp(bytes.data(),"RIFF",4u)||std::memcmp(bytes.data()+8u,"WAVE",4u))return result;
    const auto word16=[&](std::size_t offset){return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[offset])|static_cast<std::uint16_t>(bytes[offset+1u])<<8u);};
    const auto word32=[&](std::size_t offset){return static_cast<std::uint32_t>(word16(offset))|
        static_cast<std::uint32_t>(word16(offset+2u))<<16u;};
    const auto riffSize=word32(4u);
    if(riffSize<4u||static_cast<std::uint64_t>(riffSize)+8u>bytes.size())return result;
    const auto end=static_cast<std::size_t>(riffSize)+8u;
    bool haveFormat{},haveData{};std::uint16_t encoding{},channels{},bits{},alignment{};
    std::uint32_t sourceRate{};std::size_t pcmOffset{},pcmBytes{};
    for(std::size_t offset=12u;offset<end;) {
        if(end-offset<8u)return result;
        const auto count=word32(offset+4u);const auto payload=offset+8u;
        if(count>end-payload)return result;
        if(!std::memcmp(bytes.data()+offset,"fmt ",4u)) {
            if(haveFormat||count<16u)return result;
            haveFormat=true;encoding=word16(payload);channels=word16(payload+2u);
            sourceRate=word32(payload+4u);alignment=word16(payload+12u);bits=word16(payload+14u);
        } else if(!std::memcmp(bytes.data()+offset,"data",4u)) {
            if(haveData)return result;
            haveData=true;pcmOffset=payload;pcmBytes=count;
        }
        const auto consumed=static_cast<std::size_t>(count)+(count&1u);
        if(consumed>end-payload)return result;
        offset=payload+consumed;
    }
    if(!haveFormat||!haveData||encoding!=1u||(channels!=1u&&channels!=2u)||
        (bits!=8u&&bits!=16u)||sourceRate<8'000u||sourceRate>192'000u||!pcmBytes)return result;
    const auto frameBytes=static_cast<std::size_t>(channels)*(bits/8u);
    if(alignment!=frameBytes||pcmBytes%frameBytes)return result;
    const auto sourceFrames=pcmBytes/frameBytes;
    // Both factors are already bounded (64MiB input and192kHz), before multiply.
    const auto frames64=static_cast<std::uint64_t>(sourceFrames)*targetRate/sourceRate;
    if(!frames64||frames64>MaximumAmbientStereoFrames)return result;
    const auto frames=static_cast<std::size_t>(frames64);result.stereo.resize(frames*2u);
    const auto sample=[&](std::size_t frame,std::size_t channel)->float {
        const auto sourceChannel=channels==1u?0u:channel;
        const auto offset=pcmOffset+frame*frameBytes+sourceChannel*(bits/8u);
        if(bits==8u)return static_cast<float>((static_cast<std::int32_t>(bytes[offset])-128)*256);
        return static_cast<float>(std::bit_cast<std::int16_t>(word16(offset)));
    };
    for(std::size_t frame=0u;frame<frames;++frame) {
        const double position=static_cast<double>(frame)*sourceRate/targetRate;
        const auto first=std::min(static_cast<std::size_t>(position),sourceFrames-1u);
        const auto second=std::min(first+1u,sourceFrames-1u);const float fraction=static_cast<float>(position-first);
        for(std::size_t channel=0u;channel<2u;++channel) {
            const float a=sample(first,channel),b=sample(second,channel);
            result.stereo[frame*2u+channel]=static_cast<std::int16_t>(a+(b-a)*fraction);
        }
    }
    result.sourceRate=sourceRate;result.sourceChannels=channels;result.sourceBits=bits;result.passed=true;
    return result;
}
} // namespace QuestVr
