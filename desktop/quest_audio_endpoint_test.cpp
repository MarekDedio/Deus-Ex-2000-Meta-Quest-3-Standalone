#include "quest_audio_endpoint.h"
#include "quest_audio_pcm.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace {
using Bytes=std::vector<std::uint8_t>;std::size_t checks{},refusals{};
void Require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
template<class F>void Refused(F&& operation){bool refused{};try{operation();}catch(const std::exception&){refused=true;}Require(refused,"Invalid endpoint accepted");++refusals;}
void U16(Bytes& bytes,std::uint16_t value){bytes.push_back(static_cast<std::uint8_t>(value));bytes.push_back(static_cast<std::uint8_t>(value>>8u));}
void U32(Bytes& bytes,std::uint32_t value){U16(bytes,static_cast<std::uint16_t>(value));U16(bytes,static_cast<std::uint16_t>(value>>16u));}
void Word(Bytes& bytes,std::size_t offset,std::uint32_t value){for(unsigned n=0u;n<4u;++n)bytes.at(offset+n)=static_cast<std::uint8_t>(value>>(n*8u));}
Bytes Wave(std::uint32_t rate,std::uint16_t channels,std::uint16_t bits,const Bytes& pcm){
    Bytes bytes{'R','I','F','F'};U32(bytes,0u);for(auto c:std::string("WAVEfmt "))bytes.push_back(static_cast<std::uint8_t>(c));
    U32(bytes,16u);U16(bytes,1u);U16(bytes,channels);U32(bytes,rate);U32(bytes,rate*channels*(bits/8u));
    U16(bytes,static_cast<std::uint16_t>(channels*(bits/8u)));U16(bytes,bits);
    for(auto c:std::string("data")){bytes.push_back(static_cast<std::uint8_t>(c));}
    U32(bytes,static_cast<std::uint32_t>(pcm.size()));
    bytes.insert(bytes.end(),pcm.begin(),pcm.end());if(pcm.size()&1u)bytes.push_back(0u);Word(bytes,4u,static_cast<std::uint32_t>(bytes.size()-8u));return bytes;
}
void Endpoint(){
    auto client=std::make_shared<QuestVr::NativeClientLifetime>();
    auto first=std::make_shared<QuestVr::AudioEndpointControl>(1u,client,7u,8u,QuestVr::AudioOutputFormat{48'000u,2u});
    Require(!first->Ready()&&!first->ConfirmStartedOnOwner(false),"Constructor/request without start evidence became ready");
    Refused([&]{(void)first->RequireReady(client,7u,8u);});
    first->ObserveDataCallback(0);Require(!first->ConfirmStartedOnOwner(false),"Zero-frame callback became readiness proof");
    first->ObserveDataCallback(64);Require(first->ConfirmStartedOnOwner(false)&&first->Ready(),"Positive callback protocol not accepted");
    Require(first->RequireReady(client,7u,8u).sampleRate==48'000u,"Actual output metadata changed");
    Refused([&]{(void)first->RequireReady(client,7u,9u);});
    Refused([&]{(void)first->RequireReady(std::make_shared<QuestVr::NativeClientLifetime>(),7u,8u);});
    auto current=std::make_shared<QuestVr::AudioEndpointControl>(2u,client,7u,8u,QuestVr::AudioOutputFormat{48'000u,2u});
    Require(current->ConfirmStartedOnOwner(true),"STARTED protocol not accepted");
    first->Revoke(-899);first->ObserveDataCallback(128);
    Require(!first->ConfirmStartedOnOwner(true)&&first->Error()==-899&&current->Ready(),"Old callback/error revived old or revoked new endpoint");
    Require(!QuestVr::SameReadyAudioEndpoint(current,first)&&QuestVr::SameReadyAudioEndpoint(current,current),"Wrong generation accepted");
    auto failed=std::make_shared<QuestVr::AudioEndpointControl>(3u,client,7u,8u,QuestVr::AudioOutputFormat{48'000u,2u});
    failed->Revoke(-900);failed->Revoke(-901);Require(!failed->ConfirmStartedOnOwner(true)&&failed->Error()==-900,"Error-before-ready or first-error policy changed");
    client->Revoke();Require(!current->Ready()&&!current->ConfirmStartedOnOwner(true),"Revoked client retained audio readiness");
    Refused([&]{(void)current->RequireReady(client,7u,8u);});
    auto live=std::make_shared<QuestVr::NativeClientLifetime>();
    Refused([&]{(void)QuestVr::AudioEndpointControl(0u,live,7u,8u,{48'000u,2u});});
    Refused([&]{(void)QuestVr::AudioEndpointControl(1u,{},7u,8u,{48'000u,2u});});
    Refused([&]{(void)QuestVr::AudioEndpointControl(1u,live,0u,8u,{48'000u,2u});});
    Refused([&]{(void)QuestVr::AudioEndpointControl(1u,live,7u,0u,{48'000u,2u});});
    for(const auto rate:{7'999u,192'001u})
        Refused([&]{(void)QuestVr::AudioEndpointControl(1u,live,7u,8u,{rate,2u});});
    for(const auto channels:{1u,3u})
        Refused([&]{(void)QuestVr::AudioEndpointControl(1u,live,7u,8u,{48'000u,channels});});
    for(unsigned iteration=0u;iteration<64u;++iteration) {
        auto raced=std::make_shared<QuestVr::AudioEndpointControl>(iteration+10u,live,7u,8u,QuestVr::AudioOutputFormat{48'000u,2u});
        std::thread start([raced] {raced->ObserveDataCallback(32);(void)raced->ConfirmStartedOnOwner(true);});
        raced->Revoke(-901);start.join();
        Require(!raced->Ready()&&!raced->ConfirmStartedOnOwner(true)&&raced->Error()==-901,
            "Concurrent start/error revived an irreversibly revoked endpoint");
    }
}
void Pcm(){
    const auto mono=Wave(48'000u,1u,8u,{0u,128u,255u});
    const auto decoded=QuestVr::PrepareAmbientWavePcm(mono,48'000u);
    Require(decoded.passed&&decoded.stereo==std::vector<std::int16_t>{-32768,-32768,0,0,32512,32512},"8-bit mono PCM changed");
    Bytes stereo;for(const auto sample:std::array<std::int16_t,4u>{-10000,10000,500,-500})U16(stereo,static_cast<std::uint16_t>(sample));
    const auto input=Wave(24'000u,2u,16u,stereo);const auto up=QuestVr::PrepareAmbientWavePcm(input,48'000u);
    Require(up.passed&&up.stereo==std::vector<std::int16_t>{-10000,10000,-4750,4750,500,-500,500,-500},"Stereo resampler mixed channels or changed interpolation");
    const auto same=QuestVr::PrepareAmbientWavePcm(input,24'000u);
    Require(same.passed&&same.stereo==std::vector<std::int16_t>{-10000,10000,500,-500},"Equal-rate stereo identity changed");
    Bytes extremes;
    for(const auto sample:std::array<std::int16_t,8u>{-32768,32767,100,-100,0,0,32767,-32768})
        U16(extremes,static_cast<std::uint16_t>(sample));
    const auto down=QuestVr::PrepareAmbientWavePcm(Wave(48'000u,2u,16u,extremes),24'000u);
    Require(down.passed&&down.stereo==std::vector<std::int16_t>{-32768,32767,0,0},
        "Stereo downsampling changed channel identity or signed 16-bit extrema");
    for(const auto rate:{22'050u,44'100u}){
        const auto resampled=QuestVr::PrepareAmbientWavePcm(Wave(rate,2u,16u,stereo),48'000u);
        Require(resampled.passed&&resampled.stereo.size()==(2ull*48'000u/rate)*2u,"Resampled stereo frame count changed");
        for(std::size_t frame=0u;frame<resampled.stereo.size()/2u;++frame)
            Require(resampled.stereo[frame*2u]==-resampled.stereo[frame*2u+1u],"Stereo channels collapsed");
    }
    for(const auto rate:{0u,7'999u,192'001u,std::numeric_limits<std::uint32_t>::max()})
        Require(!QuestVr::PrepareAmbientWavePcm(input,rate).passed,"Invalid target rate accepted");
    for(std::size_t cut=0u;cut<input.size();++cut)
        Require(!QuestVr::PrepareAmbientWavePcm(std::span(input).first(cut),48'000u).passed,"Truncated WAV accepted");
    auto malformed=input;Word(malformed,4u,std::numeric_limits<std::uint32_t>::max());
    Require(!QuestVr::PrepareAmbientWavePcm(malformed,48'000u).passed,"RIFF length overflow accepted");
    malformed=input;Word(malformed,40u,std::numeric_limits<std::uint32_t>::max());
    Require(!QuestVr::PrepareAmbientWavePcm(malformed,48'000u).passed,"Chunk length overflow accepted");
    malformed=input;malformed[32u]=1u;Require(!QuestVr::PrepareAmbientWavePcm(malformed,48'000u).passed,"Bad PCM alignment accepted");
    Require(!QuestVr::PrepareAmbientWavePcm(Wave(8'000u,1u,8u,Bytes(400'000u,128u)),192'000u).passed,"Output allocation budget ignored");
    Require(!QuestVr::PrepareAmbientWavePcm(Wave(48'000u,2u,16u,Bytes{0u,1u,2u}),48'000u).passed,"Partial PCM frame accepted");
}
} // namespace
int main(){try{Endpoint();Pcm();std::cout<<"PASS audio endpoint protocol/stereo WAV: "<<checks<<" checks/"<<refusals<<" refusals. Host protocol ONLY; not real AAudio/hardware readiness.\n";return 0;}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
