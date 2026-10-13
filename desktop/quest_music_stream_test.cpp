#include "quest_music_stream.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void Require(bool b,const char* text) {if(!b) throw std::runtime_error(text);}
// Original test composition, not commercial game audio. One ProTracker pattern
// with a looped generated waveform and a single authored C3 note.
std::vector<std::uint8_t> Module() {
    std::vector<std::uint8_t> bytes(1084u+1024u+64u,0u);
    const std::string name="Quest stream test";
    std::copy(name.begin(),name.end(),bytes.begin());
    bytes[42]=0;bytes[43]=32;bytes[45]=64;
    bytes[48]=0;bytes[49]=32; // Whole generated sample loops.
    bytes[950]=1;bytes[1080]='M';bytes[1081]='.';bytes[1082]='K';bytes[1083]='.';
    bytes[1084]=1;bytes[1085]=172;bytes[1086]=31;bytes[1087]=1; // F01: short loop.
    for(std::size_t i=0u;i<64u;++i) bytes[1084u+1024u+i]=static_cast<std::uint8_t>(i<32u?96:-96);
    return bytes;
}
template<class F> void Refuse(F action) {bool rejected{};try{action();}catch(const std::exception&){rejected=true;}Require(rejected,"Malformed music stream was accepted");}
}
int main() {
    try {
        const auto bytes=Module();
        Refuse([]{QuestVr::MusicStream({},48000u,0u);});
        Refuse([&]{QuestVr::MusicStream(bytes,0u,0u);});
        Refuse([&]{QuestVr::MusicStream(bytes,48000u,254u);});
        Refuse([]{QuestVr::MusicStream(std::vector<std::uint8_t>(64u,0u),48000u,0u);});
        QuestVr::MusicStream music(bytes,48000u,0u);
        Require(music.SampleRate()==48000u && music.DurationSeconds()>0.0 && music.Title()=="Quest stream test",
            "Generated tracker metadata was not retained");
        std::array<std::int16_t,4096u*2u> pcm{};
        Require(music.Mix(pcm.data(),4096u,0.35f)==4096u,"Prefill failed to supply first callback without waiting");
        Require(std::any_of(pcm.begin(),pcm.end(),[](auto n){return n!=0;}),"Generated note decoded to silence");
        const auto before=pcm;
        Require(music.Mix(pcm.data(),4096u,std::numeric_limits<float>::quiet_NaN())==0u && pcm==before,
            "Invalid music gain modified callback output");
        Require(music.Mix(pcm.data(),4096u,-0.1f)==0u && pcm==before &&
            music.Mix(pcm.data(),4096u,1.1f)==0u && pcm==before,
            "Out-of-range music gain modified callback output");
        Require(music.Mix(nullptr,32u,0.35f)==0u,"Null callback output accepted");
        Require(music.Mix(pcm.data(),32769u,0.35f)==0u,"Oversized callback request accepted");
        pcm.fill(123);
        Require(music.Mix(pcm.data(),256u,0.0f)==256u &&
            std::all_of(pcm.begin(),pcm.end(),[](auto n){return n==123;}),"Zero gain altered existing audio");
        // Exercise wrap and refill through the actual producer; consumer code
        // never sleeps. Test harness waits outside the callback between blocks.
        for(unsigned i=0u;i<160u;++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            pcm.fill(0);
            Require(music.Mix(pcm.data(),512u,0.35f)==512u,"PCM ring lost a refill block at wrap");
        }
        Require(music.UnderrunFrames()==0u,"Prefilled stream reported unexpected underruns");
        Require(music.DurationSeconds()*music.SampleRate()<4096u+256u+160u*512u,
            "Generated test did not consume across the authored song loop");
        std::cout<<"PASS generated tracker: bounded input, authored section, nonzero PCM, fixed-ring wrap/refill, song loop, zero/invalid gain and callback bounds\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
