#include "Precomp.h"
#include "quest_map_music.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
std::uint64_t Fingerprint(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);if(!file) throw std::runtime_error("Cannot fingerprint owned music data");
    std::uint64_t hash=1469598103934665603ull;std::array<char,16384u> block{};
    while(file) {file.read(block.data(),block.size());for(std::streamsize i=0;i<file.gcount();++i) {
        hash^=static_cast<unsigned char>(block[static_cast<std::size_t>(i)]);hash*=1099511628211ull;
    }}
    if(!file.eof()) throw std::runtime_error("Music fingerprint read failed");
    return hash;
}
template<class F> void Refuse(F action) {bool rejected{};try{action();}catch(const std::exception&){rejected=true;}
    if(!rejected) throw std::runtime_error("Malformed UMusic descriptor was accepted");}
}
int main(int argc,char** argv) {
    try {
        if(argc==1) {std::cout<<"SKIP: supply owned original Deus Ex installation for map music integration\n";return 77;}
        if(argc!=2) throw std::runtime_error("Usage: portable_music_test GAME_ROOT");
        const auto root=std::filesystem::path(argv[1]);
        std::vector<PortablePackageTables> scripts;
        for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire","MPCharacters"})
            scripts.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
        if(!InitializePortableRuntime(scripts).passed) throw std::runtime_error("Original music runtime metadata failed");
        const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
        const auto musicPath=root/"Music"/"Training_Music.umx";
        const auto musicHash=Fingerprint(musicPath),mapHash=Fingerprint(map.sourcePath);
        if(!LoadPortableRuntimeMap(map).passed) throw std::runtime_error("Original Training map failed");
        auto prepared=QuestVr::PrepareOriginalMapMusic(root.string(),"00_Training",48000u);
        if(!prepared.stream) throw std::runtime_error("Original map music preparation failed: "+prepared.error);
        std::array<std::int16_t,4096u*2u> pcm{};
        bool audible{};
        for(unsigned block=0u;block<32u && !audible;++block) {
            pcm.fill(0);
            if(prepared.stream->Mix(pcm.data(),4096u,0.35f)!=4096u)
                throw std::runtime_error("Original music producer did not refill its PCM ring");
            audible=std::any_of(pcm.begin(),pcm.end(),[](auto n){return n!=0;});
            // Test harness only: the actual audio consumer never waits.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if(!audible)
            throw std::runtime_error("Actual Training module did not deliver audible PCM");
        const auto source=LoadPortablePackageTables(musicPath.string());
        const auto musicExport=FindPortableExport(source,"Training_Music");
        Refuse([&]{static_cast<void>(LoadPortableMusic(source,source.exports.size()));});
        for(const auto budget:{0u,8u,32u*1024u*1024u+1u})
            Refuse([&]{static_cast<void>(LoadPortableMusic(source,musicExport,budget));});
        auto malformed=source;malformed.exports.at(musicExport).ObjClass=0;
        Refuse([&]{static_cast<void>(LoadPortableMusic(malformed,musicExport));});
        malformed=source;--malformed.exports.at(musicExport).ObjSize;
        Refuse([&]{static_cast<void>(LoadPortableMusic(malformed,musicExport));});
        if(Fingerprint(musicPath)!=musicHash || Fingerprint(map.sourcePath)!=mapHash)
            throw std::runtime_error("Original music/map bytes changed during read-only decoding");
        std::cout<<"PASS actual slot-zero "<<prepared.actorPath<<" Song="<<prepared.songPath<<
            " format="<<prepared.format<<" section="<<unsigned(prepared.section)<<" duration="<<
            prepared.stream->DurationSeconds()<<"s title="<<prepared.stream->Title()<<"; actual original UMX decoded off callback\n";
        prepared={};ShutdownPortableRuntime();return 0;
    } catch(const std::exception& e) {ShutdownPortableRuntime();std::cerr<<e.what()<<'\n';return 1;}
}
