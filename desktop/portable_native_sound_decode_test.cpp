#include "Precomp.h"
#include "portable_native_sound_decode.h"
#include "surreal_portable_package_tables.h"
#include "quest_audio_voice_protocol.h"
#include "minimp3_ex.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{},ownedSounds{};
void Require(bool condition,const char* reason){++checks;if(!condition)throw std::runtime_error(reason);}
void U16(Bytes& bytes,std::uint16_t value){bytes.push_back(static_cast<std::uint8_t>(value));bytes.push_back(static_cast<std::uint8_t>(value>>8u));}
void U32(Bytes& bytes,std::uint32_t value){U16(bytes,static_cast<std::uint16_t>(value));U16(bytes,static_cast<std::uint16_t>(value>>16u));}
void Word(Bytes& bytes,std::size_t offset,std::uint32_t value){for(unsigned part=0u;part<4u;++part)bytes.at(offset+part)=static_cast<std::uint8_t>(value>>(part*8u));}
void Half(Bytes& bytes,std::size_t offset,std::uint16_t value){bytes.at(offset)=static_cast<std::uint8_t>(value);bytes.at(offset+1u)=static_cast<std::uint8_t>(value>>8u);}
void Chunk(Bytes& bytes,std::string_view tag,const Bytes& payload){
    Require(tag.size()==4u,"Generated chunk tag must have exactly four bytes");
    for(auto character:tag)bytes.push_back(static_cast<std::uint8_t>(character));
    U32(bytes,static_cast<std::uint32_t>(payload.size()));bytes.insert(bytes.end(),payload.begin(),payload.end());
    if(payload.size()&1u)bytes.push_back(0u);
}
Bytes Riff(){return {'R','I','F','F',0u,0u,0u,0u,'W','A','V','E'};}
void Finish(Bytes& bytes){Word(bytes,4u,static_cast<std::uint32_t>(bytes.size()-8u));}
Bytes Format(std::uint32_t rate,std::uint16_t channels,std::uint16_t bits){
    Bytes payload;U16(payload,1u);U16(payload,channels);U32(payload,rate);
    U32(payload,rate*channels*(bits/8u));U16(payload,static_cast<std::uint16_t>(channels*(bits/8u)));U16(payload,bits);return payload;
}
Bytes Wave(std::uint32_t rate,std::uint16_t channels,std::uint16_t bits,const Bytes& samples){
    auto bytes=Riff();Chunk(bytes,"fmt ",Format(rate,channels,bits));Chunk(bytes,"data",samples);Finish(bytes);return bytes;
}
Bytes Samples(std::initializer_list<std::int16_t> samples){Bytes bytes;for(auto sample:samples)U16(bytes,std::bit_cast<std::uint16_t>(sample));return bytes;}
Bytes Sampler(std::uint32_t begin,std::uint32_t inclusiveEnd,std::uint32_t type=0u,
    std::uint32_t fraction=0u,std::uint32_t repeat=0u){
    Bytes bytes(60u);Word(bytes,28u,1u);Word(bytes,36u,17u);Word(bytes,40u,type);
    Word(bytes,44u,begin);Word(bytes,48u,inclusiveEnd);Word(bytes,52u,fraction);Word(bytes,56u,repeat);return bytes;
}
Bytes LoopWave(std::uint32_t begin=1u,std::uint32_t inclusiveEnd=2u){
    auto bytes=Wave(22'050u,1u,16u,Samples({100,200,300,400}));Chunk(bytes,"smpl",Sampler(begin,inclusiveEnd));Finish(bytes);return bytes;
}
void Refused(std::string_view format,const Bytes& bytes){
    const auto before=bytes;
    bool rejected{};try{static_cast<void>(DecodePortableNativeSoundPcm(format,bytes));}catch(const std::exception&){rejected=true;}
    Require(rejected,"Malformed or unsupported native sound accepted");++refusals;
    Require(before==bytes,"Refused native sound decode mutated input bytes");
}
void Check(std::string_view format,const Bytes& source,std::uint32_t rate,const std::vector<std::int16_t>& expected,
    bool loop=false,std::uint32_t begin=0u,std::uint32_t end=0u){
    const auto before=source;
    const auto decoded=DecodePortableNativeSoundPcm(format,source),again=DecodePortableNativeSoundPcm(format,source);
    Require(decoded.rate==rate&&decoded.stereo==expected,"Native decoder altered source rate, frames or channel samples");
    Require(decoded.loop==loop&&decoded.loopBegin==begin&&decoded.loopEnd==end,"Native decoder changed explicit source loop metadata");
    Require(decoded.stereo==again.stereo&&decoded.rate==again.rate&&decoded.loop==again.loop&&
        decoded.loopBegin==again.loopBegin&&decoded.loopEnd==again.loopEnd,"Repeated native sound preparation was not deterministic");
    Require(source==before,"Native sound preparation mutated source bytes");
}
void PcmFixtures(){
    const auto mono8=Wave(8'000u,1u,8u,{0u,1u,127u,128u,129u,255u});
    Check("wav",mono8,8'000u,{-32768,-32768,-32512,-32512,-256,-256,0,0,256,256,32512,32512});
    Check("WaV",Wave(44'100u,2u,8u,{0u,255u,128u,129u}),44'100u,{-32768,32512,0,256});
    Check("WAV",Wave(192'000u,1u,16u,Samples({-32768,32767,-1,0,1})),192'000u,
        {-32768,-32768,32767,32767,-1,-1,0,0,1,1});
    Check("wav",Wave(11'025u,2u,16u,Samples({-1000,1000,23,-27,32767,-32768})),11'025u,
        {-1000,1000,23,-27,32767,-32768});
    for(auto rate:{8'000u,11'025u,22'050u,44'100u,48'000u,192'000u})
        Check("wav",Wave(rate,2u,16u,Samples({100,-200,300,-400})),rate,{100,-200,300,-400});
    // Chunk order and extension bytes are legal; native rate and sample order
    // survive without the former output-rate resampling.
    auto order=Riff();Chunk(order,"JUNK",{7u,8u,9u});Chunk(order,"data",Samples({-12,34}));
    auto extended=Format(32'000u,1u,16u);U16(extended,0u);Chunk(order,"fmt ",extended);Finish(order);
    Check("wav",order,32'000u,{-12,-12,34,34});
    auto trailing=mono8;trailing.insert(trailing.end(),{'x','y','z'});
    Check("wav",trailing,8'000u,{-32768,-32768,-32512,-32512,-256,-256,0,0,256,256,32512,32512});
    auto noLoop=Wave(16'000u,1u,8u,{128u});Chunk(noLoop,"smpl",Bytes(36u));Finish(noLoop);
    Check("wav",noLoop,16'000u,{0,0});
    Check("wav",LoopWave(),22'050u,{100,100,200,200,300,300,400,400},true,1u,3u);
    Check("wav",LoopWave(0u,3u),22'050u,{100,100,200,200,300,300,400,400},true,0u,4u);
    Check("wav",LoopWave(2u,2u),22'050u,{100,100,200,200,300,300,400,400},true,2u,3u);
    auto smplData=Wave(16'000u,1u,8u,{0u,128u});auto sampler=Sampler(0u,1u);Word(sampler,32u,3u);
    sampler.insert(sampler.end(),{4u,5u,6u});Chunk(smplData,"smpl",sampler);Finish(smplData);
    Check("wav",smplData,16'000u,{-32768,-32768,0,0},true,0u,2u);
}
void WaveRefusals(){
    const auto valid=Wave(24'000u,2u,16u,Samples({-100,200,300,-400}));
    Refused("wav",{});Refused("",valid);Refused("wave",valid);Refused("ogg",valid);Refused(std::string_view("w\0v",3u),valid);
    for(std::size_t cut=0u;cut<valid.size();++cut)Refused("wav",Bytes(valid.begin(),valid.begin()+cut));
    auto malformed=valid;malformed[0u]='X';Refused("wav",malformed);
    malformed=valid;malformed[8u]='X';Refused("wav",malformed);
    malformed=valid;Word(malformed,4u,std::numeric_limits<std::uint32_t>::max());Refused("wav",malformed);
    malformed=valid;Word(malformed,4u,3u);Refused("wav",malformed);
    malformed=valid;Word(malformed,16u,std::numeric_limits<std::uint32_t>::max());Refused("wav",malformed);
    malformed=valid;Word(malformed,40u,std::numeric_limits<std::uint32_t>::max());Refused("wav",malformed);
    malformed=valid;Half(malformed,20u,3u);Refused("wav",malformed);
    for(auto channels:{0u,3u,65535u}){malformed=valid;Half(malformed,22u,static_cast<std::uint16_t>(channels));Refused("wav",malformed);}
    for(auto bits:{0u,4u,24u,32u}){malformed=valid;Half(malformed,34u,static_cast<std::uint16_t>(bits));Refused("wav",malformed);}
    for(auto rate:{0u,7'999u,192'001u,std::numeric_limits<std::uint32_t>::max()}){malformed=valid;Word(malformed,24u,rate);Refused("wav",malformed);}
    for(auto align:{0u,1u,3u,8u}){malformed=valid;Half(malformed,32u,static_cast<std::uint16_t>(align));Refused("wav",malformed);}
    Refused("wav",Wave(48'000u,2u,16u,{1u,2u,3u}));Refused("wav",Wave(48'000u,1u,8u,{}));
    auto duplicate=valid;Chunk(duplicate,"data",{1u,2u,3u,4u});Finish(duplicate);Refused("wav",duplicate);
    duplicate=valid;Chunk(duplicate,"fmt ",Format(48'000u,2u,16u));Finish(duplicate);Refused("wav",duplicate);
    auto missing=Riff();Chunk(missing,"data",{1u,2u});Finish(missing);Refused("wav",missing);
    missing=Riff();Chunk(missing,"fmt ",Format(48'000u,1u,8u));Finish(missing);Refused("wav",missing);
    missing=Riff();Chunk(missing,"fmt ",Bytes(15u));Chunk(missing,"data",{1u,2u});Finish(missing);Refused("wav",missing);
    auto noPad=Wave(48'000u,1u,8u,{128u});noPad.pop_back();Finish(noPad);Refused("wav",noPad);
    auto fragment=valid;fragment.push_back('x');Finish(fragment);Refused("wav",fragment);
    for(const auto& loop:std::array<Bytes,7u>{Sampler(3u,2u),Sampler(0u,4u),Sampler(0u,std::numeric_limits<std::uint32_t>::max()),
        Sampler(0u,3u,1u),Sampler(0u,3u,2u),Sampler(0u,3u,0u,1u),Sampler(0u,3u,0u,0u,1u)}) {
        malformed=valid;Chunk(malformed,"smpl",loop);Finish(malformed);Refused("wav",malformed);
    }
    for(auto size:{0u,35u,36u,59u}) {
        auto sampler=Sampler(0u,1u);sampler.resize(size);malformed=valid;Chunk(malformed,"smpl",sampler);Finish(malformed);Refused("wav",malformed);
    }
    auto sampler=Sampler(0u,1u);Word(sampler,28u,2u);malformed=valid;Chunk(malformed,"smpl",sampler);Finish(malformed);Refused("wav",malformed);
    sampler=Sampler(0u,1u);Word(sampler,32u,1u);malformed=valid;Chunk(malformed,"smpl",sampler);Finish(malformed);Refused("wav",malformed);
    sampler=Bytes(36u);Word(sampler,32u,std::numeric_limits<std::uint32_t>::max());malformed=valid;Chunk(malformed,"smpl",sampler);Finish(malformed);Refused("wav",malformed);
    duplicate=LoopWave();Chunk(duplicate,"smpl",Sampler(0u,1u));Finish(duplicate);Refused("wav",duplicate);
    // Maximum source clip is tested separately from output/transport budgets.
    constexpr std::size_t maximumFrames=4u<<20u;
    auto maximum=Wave(8'000u,1u,8u,Bytes(maximumFrames,128u));
    const auto decoded=DecodePortableNativeSoundPcm("wav",maximum);
    Require(decoded.rate==8'000u&&decoded.stereo.size()==maximumFrames*2u&&
        std::all_of(decoded.stereo.begin(),decoded.stereo.end(),[](auto value){return value==0;}),"Maximum native-rate clip boundary failed");
    Refused("wav",Wave(8'000u,1u,8u,Bytes(maximumFrames+1u,128u)));
    Refused("wav",Bytes((64u<<20u)+1u));
}
// Source-generated MPEG1 Layer III zero side information/spectral data. No
// original/commercial sound bytes are embedded in this test executable.
Bytes SilentFrames(bool mono,std::size_t count=5u,bool halfRate=false){
    const auto frameBytes=halfRate?208u:417u;
    Bytes bytes(count*frameBytes,0u);for(std::size_t frame=0u;frame<count;++frame){const auto at=frame*frameBytes;
        bytes[at]=0xffu;bytes[at+1u]=halfRate?0xf3u:0xfbu;bytes[at+2u]=halfRate?0x80u:0x90u;bytes[at+3u]=mono?0xc0u:0u;}return bytes;
}
void MpegFixtures(){
    for(bool mono:{true,false})for(std::string_view format:{"mp3","Mp3","MP2"}) {
        const auto bytes=SilentFrames(mono);Check(format,bytes,44'100u,std::vector<std::int16_t>(5u*1152u*2u,0));
    }
    for(const auto& bytes:{Bytes{0xffu},Bytes{0xffu,0xfbu},Bytes{0xffu,0xfbu,0x90u},Bytes(4096u,0x55u),Bytes(4096u,0xffu),
        Bytes{'I','D','3',4u,0u,0u,0x7fu,0x7fu,0x7fu,0x7fu}})Refused("mp3",bytes);
    auto forged=SilentFrames(true);constexpr auto tag=4u+17u;
    forged[tag]='X';forged[tag+1u]='i';forged[tag+2u]='n';forged[tag+3u]='g';forged[tag+7u]=1u;
    for(std::size_t offset=8u;offset<12u;++offset)forged[tag+offset]=0xffu;
    mp3dec_ex_t probe{};
    Require(mp3dec_ex_open_buf(&probe,forged.data(),forged.size(),MP3D_SEEK_TO_SAMPLE)==0,"Forged-Xing fixture must be recognized by actual decoder");
    const auto reported=probe.samples;mp3dec_ex_close(&probe);
    Require(reported>(16u<<20u),"Forged-Xing fixture did not exercise the preallocation limit");Refused("mp3",forged);
}
void DecoderBudgetFixtures(){
    const auto wave=LoopWave();const auto compressed=SilentFrames(true);
    for(const auto& source:std::array<Bytes,2u>{wave,compressed}) {
        const std::string_view format=source[0u]=='R'?"wav":"mp3";
        const auto original=source;
        std::vector<std::size_t> admissions;std::size_t units{},steps{};
        const PortableNativeSoundDecodeWork observer{
            [&](std::size_t count){units+=count;++steps;},
            [&](std::size_t bytes){admissions.push_back(bytes);}};
        const auto decoded=DecodePortableNativeSoundPcm(format,source,observer);
        Require(!admissions.empty()&&steps>1u&&units>=source.size()+decoded.stereo.size(),"Decoder failed to report bounded scan/output work");
        const auto maximum=*std::max_element(admissions.begin(),admissions.end());
        Require(maximum>=decoded.stereo.capacity()*sizeof(std::int16_t),"Decoder omitted its actual retained output workspace");
        const PortableNativeSoundDecodeWork exact{{},[&](std::size_t bytes){Require(bytes<=maximum,"Exact decoder workspace ceiling changed");}};
        const auto repeated=DecodePortableNativeSoundPcm(format,source,exact);
        Require(repeated.stereo==decoded.stereo&&repeated.loop==decoded.loop&&source==original,"Budgeted decode changed source samples or loops");
        for(const auto limit:{std::size_t{0u},maximum-1u}) {
            bool refused{};const PortableNativeSoundDecodeWork bounded{{},[&](std::size_t bytes){if(bytes>limit)throw std::runtime_error("Fixture decoder workspace exhausted");}};
            try{static_cast<void>(DecodePortableNativeSoundPcm(format,source,bounded));}catch(const std::exception&){refused=true;}
            Require(refused&&source==original,"Decoder accepted insufficient workspace or modified its borrowed input");++refusals;
        }
        // Refusal at every notification exercises cleanup after index/source
        // preparation too; none may turn a failed preparation into a resource.
        for(std::size_t failAt=1u;failAt<=admissions.size();++failAt) {
            std::size_t calls{};bool refused{};
            const PortableNativeSoundDecodeWork bounded{{},[&](std::size_t){if(++calls==failAt)throw std::runtime_error("Fixture refused decoder stage");}};
            try{static_cast<void>(DecodePortableNativeSoundPcm(format,source,bounded));}catch(const std::exception&){refused=true;}
            Require(refused&&calls==failAt&&source==original,"Decoder ignored a staged admission refusal");++refusals;
        }
        for(std::size_t failAt:{1u,2u}) {
            std::size_t calls{};bool refused{};
            const PortableNativeSoundDecodeWork bounded{[&](std::size_t){if(++calls==failAt)throw std::runtime_error("Fixture decoder work exhausted");},{}};
            try{static_cast<void>(DecodePortableNativeSoundPcm(format,source,bounded));}catch(const std::exception&){refused=true;}
            Require(refused&&calls==failAt&&source==original,"Decoder ignored input/record work refusal");++refusals;
        }
    }
    // The actual codec allocates 4096 index rows even for five frames. MPEG2
    // fixture4097 crosses its real doubling boundary while remaining below
    // the unchanged maximum decoded source-frame limit.
    for(const auto count:{5u,4097u}) {
        const auto source=SilentFrames(true,count,true);std::vector<std::size_t> admissions;
        const PortableNativeSoundDecodeWork observer{{},[&](std::size_t bytes){admissions.push_back(bytes);}};
        const auto decoded=DecodePortableNativeSoundPcm("mp3",source,observer);
        const auto capacity=count<=4096u?4096u:8192u;
        const auto previous=count<=4096u?0u:4096u;
        Require(decoded.rate==22'050u&&decoded.stereo.size()==count*576u*2u,"MPEG2 index-growth fixture did not preserve native frames");
        Require(admissions.size()>=7u&&admissions[2u]-admissions[1u]==
            static_cast<std::size_t>(capacity+previous)*sizeof(mp3dec_frame_t),"MP3 index realloc peak was not derived/admitted before open");
        Require(admissions[3u]>=admissions[1u]+capacity*sizeof(mp3dec_frame_t)+count*576u*sizeof(mp3d_sample_t),
            "MP3 source allocation omitted actual index/source coexistence");
        Require(admissions[5u]>=admissions[3u]+count*576u*2u*sizeof(std::int16_t),
            "MP3 stereo allocation omitted simultaneous source/output storage");
    }
    // Work rejection from inside the C iterator must return to the C++ owner
    // before rethrow; it occurs before mp3dec_ex_open_buf can allocate an index.
    std::size_t steps{},admissions{};bool refused{};
    const PortableNativeSoundDecodeWork stopPreflight{
        [&](std::size_t){if(++steps==2u)throw std::runtime_error("Fixture MP3 preflight exhausted");},
        [&](std::size_t){++admissions;}};
    try{static_cast<void>(DecodePortableNativeSoundPcm("mp3",compressed,stopPreflight));}catch(const std::exception&){refused=true;}
    Require(refused&&steps==2u&&admissions==2u,"MP3 preflight work refusal reached allocating codec open");++refusals;
    auto junk=wave;Chunk(junk,"JUNK",Bytes(8192u));Finish(junk);std::size_t peak{};
    const auto output=DecodePortableNativeSoundPcm("wav",junk,{{},[&](std::size_t bytes){peak=std::max(peak,bytes);}});
    Require(!output.stereo.empty()&&peak<junk.size(),"Decoder workspace callback double-charged borrowed input as an owned copy");
}
void Compact(Bytes& bytes,std::uint32_t value){
    auto first=static_cast<std::uint8_t>(value&0x3fu);value>>=6u;if(value)first|=0x40u;bytes.push_back(first);
    while(value){auto next=static_cast<std::uint8_t>(value&0x7fu);value>>=7u;if(value)next|=0x80u;bytes.push_back(next);}
}
struct SoundExportFixture {
    std::filesystem::path directory,path;
    explicit SoundExportFixture(const Bytes& bytes) {
        const auto serial=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned attempt=0u;attempt<64u;++attempt) {
            auto candidate=std::filesystem::temp_directory_path()/("quest-native-sound-budget-"+std::to_string(serial)+"-"+std::to_string(attempt));
            if(std::filesystem::create_directory(candidate)){directory=std::move(candidate);break;}
        }
        Require(!directory.empty(),"Cannot create isolated generated Sound fixture directory");path=directory/"GeneratedSound.uax";
        std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        Require(static_cast<bool>(output),"Cannot write generated Sound export fixture");
    }
    ~SoundExportFixture(){std::error_code ignored;if(!path.empty())std::filesystem::remove(path,ignored);if(!directory.empty())std::filesystem::remove(directory,ignored);}
};
void SoundExportBudgetFixtures(){
    const auto encoded=Wave(22'050u,1u,8u,{0u,128u,255u});
    for(const auto properties:{0u,3u,65u}) {
        Bytes payload;
        for(unsigned index=0u;index<properties;++index){Compact(payload,2u);payload.push_back(0x22u);U32(payload,index);}
        Compact(payload,0u);Compact(payload,1u);U32(payload,0u);Compact(payload,static_cast<std::uint32_t>(encoded.size()));
        payload.insert(payload.end(),encoded.begin(),encoded.end());
        Bytes fileBytes(4u,0u);fileBytes.insert(fileBytes.end(),payload.begin(),payload.end());
        SoundExportFixture fixture(fileBytes);PortablePackageTables package;package.sourcePath=fixture.path.string();package.version=68u;
        package.names={{NameString("None"),0u},{NameString("wav"),0u},{NameString("BudgetFixture"),0u}};
        ExportTableEntry entry{};entry.ObjClass=-1;entry.ObjOffset=4;entry.ObjSize=static_cast<std::int32_t>(payload.size());package.exports.push_back(entry);
        const auto succeeds=[&](std::size_t budget) {
            try {const auto loaded=LoadPortableSound(package,0u,budget);Require(loaded.format=="wav"&&loaded.data==encoded,"Budgeted Sound extraction altered payload/format");return true;}
            catch(const std::runtime_error&){return false;}
        };
        Require(!succeeds(0u),"Sound extraction accepted zero working allowance");++refusals;
        std::size_t upper=1u;
        const auto bound=4096u+payload.size()*(4u*sizeof(PortableTaggedProperty)+8u);
        while(!succeeds(upper)){Require(upper<=bound/2u,"Generated Sound extraction exceeded its data-derived search bound");upper*=2u;}
        std::size_t lower{};
        while(upper-lower>1u){const auto midpoint=lower+(upper-lower)/2u;if(succeeds(midpoint))upper=midpoint;else lower=midpoint;}
        Require(succeeds(upper)&&!succeeds(upper-1u),"Sound extraction did not enforce an exact working-budget boundary");++refusals;
        for(const auto invalidSize:{-1,0,static_cast<int>(fileBytes.size()+1u)}) {
            package.exports[0u].ObjSize=invalidSize;Require(!succeeds(upper+bound),"Sound extraction accepted negative/empty/out-of-file export extent");++refusals;
        }
        package.exports[0u]=entry;package.exports[0u].ObjOffset=-1;
        Require(!succeeds(upper+bound),"Sound extraction accepted a negative source offset");++refusals;
        package.exports[0u]=entry;
        Require(succeeds(upper),"Refused extraction damaged later independent Sound preparation");
        std::ifstream input(fixture.path,std::ios::binary);Bytes after(fileBytes.size());input.read(reinterpret_cast<char*>(after.data()),static_cast<std::streamsize>(after.size()));
        Require(static_cast<bool>(input)&&after==fileBytes,"Sound budget tests modified their source file");
    }
}
std::uint64_t Fingerprint(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary);Require(static_cast<bool>(input),"Cannot fingerprint original sound source read-only");
    std::uint64_t hash=1469598103934665603ull;std::array<char,16384u> block{};
    while(input){input.read(block.data(),block.size());for(std::streamsize index=0;index<input.gcount();++index){hash^=static_cast<unsigned char>(block[static_cast<std::size_t>(index)]);hash*=1099511628211ull;}}
    Require(input.eof(),"Original sound source fingerprint read failed");return hash;
}
struct SourceStamp {
    std::filesystem::path path;std::uintmax_t bytes;std::filesystem::file_time_type modified;std::uint64_t hash;
    explicit SourceStamp(std::filesystem::path value):path(std::move(value)),bytes(std::filesystem::file_size(path)),
        modified(std::filesystem::last_write_time(path)),hash(Fingerprint(path)){}
    void Unchanged()const{Require(std::filesystem::file_size(path)==bytes&&std::filesystem::last_write_time(path)==modified&&Fingerprint(path)==hash,
        "Original Training/audio source was modified by a decoder-only test");}
};
void OwnedTraining(const std::filesystem::path& root){
    std::vector<SourceStamp> stamps;stamps.emplace_back(root/"Maps"/"00_Training.dx");
    const auto map=LoadPortablePackageTables(stamps.front().path.string());
    std::map<std::string,PortablePackageTables> sources;std::set<std::string> selected;
    for(std::size_t index=0u;index<map.imports.size();++index) {
        const auto& imported=map.imports[index];
        Require(imported.ClassPackage>=0&&static_cast<std::size_t>(imported.ClassPackage)<map.names.size()&&
            imported.ClassName>=0&&static_cast<std::size_t>(imported.ClassName)<map.names.size(),"Training sound import has invalid Class name indices");
        if(map.names[static_cast<std::size_t>(imported.ClassPackage)].Name!="Engine"||
            map.names[static_cast<std::size_t>(imported.ClassName)].Name!="Sound")continue;
        const auto path=GetPortableObjectPath(map,-static_cast<std::int32_t>(index+1u));const auto separator=path.find('.');
        Require(separator!=std::string::npos&&separator>0u&&separator+1u<path.size(),"Training Sound import has no actual qualified asset path");
        Require(path.find_first_of("/\\:")==std::string::npos&&selected.size()<256u,"Training audio import path/count is unsafe");
        if(!selected.insert(path).second)continue;
        const auto packageName=path.substr(0u,separator);auto found=sources.find(packageName);
        if(found==sources.end()) {
            Require(sources.size()<16u,"Training decoder fixture exceeds bounded source count");
            auto source=root/"Sounds"/(packageName+".uax");
            if(!std::filesystem::is_regular_file(source))source=root/"System"/(packageName+".u");
            stamps.emplace_back(source);found=sources.emplace(packageName,LoadPortablePackageTables(source.string())).first;
        }
        const auto& package=found->second;const auto at=FindPortableExport(package,path.substr(separator+1u));
        Require(at<package.exports.size()&&GetPortableObjectPath(package,package.exports[at].ObjClass)=="Engine.Sound",
            "Training imported sound did not resolve to an actual Engine.Sound export");
        const auto sound=LoadPortableSound(package,at);const auto before=sound.data;
        const auto pcm=DecodePortableNativeSoundPcm(sound.format.ToString(),sound.data);
        Require(!pcm.stereo.empty()&&pcm.stereo.size()%2u==0u&&pcm.rate>=8'000u&&pcm.rate<=192'000u&&
            pcm.stereo.size()/2u<=(4u<<20u),"Original Training sound produced invalid bounded native PCM");
        Require(!pcm.loop||(pcm.loopBegin<pcm.loopEnd&&pcm.loopEnd<=pcm.stereo.size()/2u),"Original Training sampler loop lost source positions");
        Require(sound.data==before,"Original sound export compressed bytes were modified");++ownedSounds;
        std::cout<<"OWNED SOUND "<<path<<" format="<<sound.format.ToString()<<" rate="<<pcm.rate<<" frames="<<pcm.stereo.size()/2u
            <<" loop="<<pcm.loop<<" begin="<<pcm.loopBegin<<" end="<<pcm.loopEnd<<'\n';
    }
    Require(ownedSounds!=0u,"First Training map had no actual imported Engine.Sound resources");
    for(const auto& stamp:stamps)stamp.Unchanged();
}
} // namespace
int main(int argc,char** argv){
    try {
        bool owned{};std::filesystem::path root;
        for(int argument=1;argument<argc;++argument){const std::string_view value=argv[argument];
            if(value=="--owned")owned=true;
            else if(value=="--training-root"&&argument+1<argc){owned=true;root=argv[++argument];}
            else throw std::runtime_error("Usage: portable_native_sound_decode_test [--owned | --training-root OWNED_GAME_ROOT]");}
        if(owned&&root.empty()){const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured)root=configured;}
        if(owned&&root.empty()){std::cout<<"SKIP original Training sound decode: supply --training-root or DEUSEX_GAME_ROOT.\n";return 77;}
        if(owned)OwnedTraining(std::filesystem::canonical(root));else{PcmFixtures();WaveRefusals();MpegFixtures();DecoderBudgetFixtures();SoundExportBudgetFixtures();}
        std::cout<<"PASS native-rate sound preparation: "<<checks<<" checks/"<<refusals<<" refusals, "<<ownedSounds
            <<" original first-Training imports. PCM/resource/loop evidence ONLY; not AudioSubsystem parity, audible Quest output or playable startup.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL native sound preparation: "<<error.what()<<'\n';return 1;}
}
