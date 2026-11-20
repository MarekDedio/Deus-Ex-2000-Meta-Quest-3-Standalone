#include "portable_native_sound_decode.h"
#include "quest_audio_transport.h"
#include "portable_mp3_audio.h"
#include <array>
#include <bit>
#include <cstring>
#include <exception>
#include <limits>
#include <stdexcept>

namespace {
[[noreturn]] void Fail(const char* reason){throw std::runtime_error(reason);}
std::size_t Add(std::size_t left,std::size_t right) {
    if(right>std::numeric_limits<std::size_t>::max()-left)Fail("Native sound workspace sum overflow");
    return left+right;
}
std::size_t Multiply(std::size_t count,std::size_t size) {
    if(size&&count>std::numeric_limits<std::size_t>::max()/size)Fail("Native sound workspace product overflow");
    return count*size;
}
struct DecodeWork {
    const PortableNativeSoundDecodeWork& work;
    std::size_t units{};
    void Step(std::size_t count) {
        units=Add(units,count);
        if(work.step)work.step(count);
    }
    void Admit(std::size_t bytes) const {if(work.admit)work.admit(bytes);}
};
struct Mp3FramePreflight {
    DecodeWork& work;
    std::size_t count{};
    std::exception_ptr failure;
    static int Visit(void* user,const std::uint8_t*,int frameSize,int,std::size_t,std::uint64_t,mp3dec_frame_info_t*) noexcept {
        auto& self=*static_cast<Mp3FramePreflight*>(user);
        try {
            self.work.Step(1u);
            if(frameSize<=0)Fail("Native MP3 preflight encountered an invalid frame");
            self.count=Add(self.count,1u);return 0;
        }catch(...) {self.failure=std::current_exception();return MP3D_E_USER;}
    }
};
struct CloseMp3 {
    mp3dec_ex_t& value;
    ~CloseMp3(){mp3dec_ex_close(&value);}
};
std::size_t Mp3IndexCapacity(std::size_t frames) {
    if(!frames)Fail("Native MP3 preflight found no frames");
    std::size_t capacity=4096u;
    while(capacity<frames)capacity=Multiply(capacity,2u);
    return capacity;
}
}
PortableNativeSoundPcm DecodePortableNativeSoundPcm(std::string_view format,
    const std::vector<std::uint8_t>& bytes,const PortableNativeSoundDecodeWork& callbacks) {
    if(bytes.empty()||bytes.size()>(64u<<20u))Fail("Native sound compressed/input byte limit is invalid");
    if(format.size()!=3u)Fail("Native sound resource format name is unsupported");
    std::array<char,3u> normalized{};
    for(std::size_t index=0u;index<normalized.size();++index)normalized[index]=format[index];
    for(auto& character:normalized)if(character>='A'&&character<='Z')character=static_cast<char>(character-'A'+'a');
    const std::string_view type(normalized.data(),normalized.size());
    PortableNativeSoundPcm result;
    DecodeWork work{callbacks};
    constexpr auto fixed=sizeof(DecodeWork)+sizeof(PortableNativeSoundPcm)+sizeof(normalized)+sizeof(std::string_view);
    work.Admit(fixed);work.Step(bytes.size());
    if(type=="mp3"||type=="mp2") {
        constexpr auto mp3Fixed=fixed+sizeof(mp3dec_ex_t)+sizeof(CloseMp3)+sizeof(Mp3FramePreflight)+sizeof(std::vector<mp3d_sample_t>);
        work.Admit(mp3Fixed);
        // mp3dec_ex_open_buf builds an allocating 4096/doubling frame index
        // before it reports decoded sample counts. Count genuine frames with
        // the same allocation-free iterator BEFORE admitting that open.
        Mp3FramePreflight preflight{work,0u,{}};
        const auto scanned=mp3dec_iterate_buf(bytes.data(),bytes.size(),Mp3FramePreflight::Visit,&preflight);
        if(preflight.failure)std::rethrow_exception(preflight.failure);
        if(scanned)Fail("Native MP3 frame preflight failed");
        const auto capacity=Mp3IndexCapacity(preflight.count);
        const auto oldCapacity=capacity==4096u?0u:capacity/2u;
        const auto indexPeak=Multiply(Add(capacity,oldCapacity),sizeof(mp3dec_frame_t));
        work.Admit(Add(mp3Fixed,indexPeak));work.Step(bytes.size());
        mp3dec_ex_t decoder{};
        CloseMp3 close{decoder};
        if(mp3dec_ex_open_buf(&decoder,bytes.data(),bytes.size(),MP3D_SEEK_TO_SAMPLE)!=0)
            Fail("Native MP3 sound resource decode failed");
        if(decoder.index.capacity>capacity||decoder.index.num_frames>preflight.count)
            Fail("Native MP3 decoder index exceeded its actual-frame preflight");
        if(decoder.info.hz<8'000||decoder.info.hz>192'000||(decoder.info.channels!=1&&decoder.info.channels!=2)||
            !decoder.samples||decoder.samples>QuestVr::kMaximumDecodedMp3Samples||
            decoder.samples/static_cast<unsigned>(decoder.info.channels)>QuestVr::AudioMaximumClipFrames)
            Fail("Native MP3 sound exceeds source-rate/channel/frame limits");
        const auto rate=static_cast<std::uint32_t>(decoder.info.hz);
        const auto channels=static_cast<std::uint32_t>(decoder.info.channels);
        const auto retainedIndex=Multiply(decoder.index.capacity,sizeof(mp3dec_frame_t));
        work.Admit(Add(mp3Fixed,Add(retainedIndex,Multiply(static_cast<std::size_t>(decoder.samples),sizeof(mp3d_sample_t)))));
        std::vector<mp3d_sample_t> source(static_cast<std::size_t>(decoder.samples));
        const auto sourceWorkspace=Add(mp3Fixed,Add(retainedIndex,Multiply(source.capacity(),sizeof(mp3d_sample_t))));
        work.Admit(sourceWorkspace);work.Step(bytes.size());
        const auto samples=mp3dec_ex_read(&decoder,source.data(),source.size());
        if(decoder.last_error||decoder.info.hz!=static_cast<int>(rate)||decoder.info.channels!=static_cast<int>(channels)||
            !samples||samples>source.size()||samples%channels)
            Fail("Native MP3 sound decoded a malformed channel tail");
        const auto frames=samples/channels;
        work.Step(Multiply(frames,2u));
        work.Admit(Add(sourceWorkspace,Multiply(Multiply(frames,2u),sizeof(std::int16_t))));
        result.stereo.resize(frames*2u);result.rate=rate;
        work.Admit(Add(sourceWorkspace,Multiply(result.stereo.capacity(),sizeof(std::int16_t))));
        for(std::size_t frame=0u;frame<frames;++frame)for(std::size_t channel=0u;channel<2u;++channel)
            result.stereo[frame*2u+channel]=source[frame*channels+(channels==1u?0u:channel)];
        return result;
    }
    if(type!="wav")Fail("Native sound resource format is unsupported");
    if(bytes.size()<12u||std::memcmp(bytes.data(),"RIFF",4u)||std::memcmp(bytes.data()+8u,"WAVE",4u))
        Fail("Native WAV sound has no RIFF/WAVE header");
    const auto u16=[&](std::size_t p){return static_cast<std::uint16_t>(std::uint16_t{bytes[p]}|std::uint16_t{bytes[p+1u]}<<8u);};
    const auto u32=[&](std::size_t p){return static_cast<std::uint32_t>(u16(p))|static_cast<std::uint32_t>(u16(p+2u))<<16u;};
    const auto riff=u32(4u);
    if(riff<4u||static_cast<std::uint64_t>(riff)+8u>bytes.size())Fail("Native WAV RIFF extent is truncated");
    const auto end=static_cast<std::size_t>(riff)+8u;
    bool haveFormat{},haveData{},haveLoop{};
    std::uint16_t encoding{},channels{},bits{},alignment{};
    std::size_t data{},dataBytes{};
    for(std::size_t at=12u;at<end;) {
        work.Step(1u);
        if(end-at<8u)Fail("Native WAV chunk header is truncated");
        const auto size=u32(at+4u);const auto payload=at+8u;
        if(size>end-payload)Fail("Native WAV chunk extent is truncated");
        if(!std::memcmp(bytes.data()+at,"fmt ",4u)) {
            if(haveFormat||size<16u)Fail("Native WAV format chunk is duplicate/truncated");
            haveFormat=true;encoding=u16(payload);channels=u16(payload+2u);result.rate=u32(payload+4u);
            alignment=u16(payload+12u);bits=u16(payload+14u);
        }else if(!std::memcmp(bytes.data()+at,"data",4u)) {
            if(haveData)Fail("Native WAV data chunk is duplicated");
            haveData=true;data=payload;dataBytes=size;
        }else if(!std::memcmp(bytes.data()+at,"smpl",4u)) {
            if(haveLoop||size<36u)Fail("Native WAV smpl chunk is duplicate/truncated");
            haveLoop=true;const auto loops=u32(payload+28u);
            if(loops>1u)Fail("Native WAV multiple sampler loops are not implemented");
            // Counted loop records and sampler-specific data are independent
            // declared extents; never read a loop or ignore a truncated tail.
            const auto records=static_cast<std::uint64_t>(loops)*24u;
            const auto samplerData=static_cast<std::uint64_t>(u32(payload+32u));
            if(36u+records+samplerData>size)Fail("Native WAV sampler records/data extent is truncated");
            if(loops) {
                if(size<60u||u32(payload+40u)!=0u||u32(payload+52u)!=0u||u32(payload+56u)!=0u)
                    Fail("Native WAV non-forward/fractional/finite/truncated sampler loop is unsupported");
                const auto finish=static_cast<std::uint64_t>(u32(payload+48u))+1u;
                if(finish>QuestVr::AudioMaximumClipFrames)Fail("Native WAV sampler loop end exceeds its source frame limit");
                result.loop=true;result.loopBegin=u32(payload+44u);result.loopEnd=static_cast<std::uint32_t>(finish);
            }
        }
        const auto padded=static_cast<std::size_t>(size)+(size&1u);
        if(padded>end-payload)Fail("Native WAV chunk pad is truncated");
        at=payload+padded;
    }
    if(!haveFormat||!haveData||encoding!=1u||(channels!=1u&&channels!=2u)||(bits!=8u&&bits!=16u)||
        result.rate<8'000u||result.rate>192'000u||!dataBytes||alignment!=channels*(bits/8u)||dataBytes%alignment)
        Fail("Native WAV PCM format/rate/alignment is unsupported");
    const auto frames=dataBytes/alignment;
    if(frames>QuestVr::AudioMaximumClipFrames||
        (result.loop&&(result.loopBegin>=result.loopEnd||result.loopEnd>frames)))
        Fail("Native WAV source frames/loop range are invalid");
    work.Step(Multiply(frames,2u));work.Admit(Add(fixed,Multiply(Multiply(frames,2u),sizeof(std::int16_t))));
    result.stereo.resize(frames*2u);
    work.Admit(Add(fixed,Multiply(result.stereo.capacity(),sizeof(std::int16_t))));
    for(std::size_t frame=0u;frame<frames;++frame)for(std::size_t channel=0u;channel<2u;++channel) {
        const auto at=data+frame*alignment+(channels==1u?0u:channel)*(bits/8u);
        result.stereo[frame*2u+channel]=bits==8u?static_cast<std::int16_t>((int{bytes[at]}-128)*256):
            std::bit_cast<std::int16_t>(u16(at));
    }
    return result;
}
