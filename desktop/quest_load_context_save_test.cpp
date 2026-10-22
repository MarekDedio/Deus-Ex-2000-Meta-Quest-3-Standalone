#include "quest_load_context_save.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

// Structural byte-codec controls only. No game package, native runtime, source
// load/restore, original DLL execution, filesystem write or Training acceptance.
namespace {
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},cases{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class Function> bool Refuses(Function&& function){try{function();}catch(const std::exception&){return true;}return false;}
void Word(Bytes& bytes,std::size_t offset,std::uint32_t word){
    for(unsigned i=0u;i<4u;++i)bytes.at(offset+i)=static_cast<std::uint8_t>(word>>(i*8u));
}
// Independent slow bitwise CRC oracle, not the production table helper.
std::uint32_t Crc(std::span<const std::uint8_t> bytes){
    std::uint32_t crc=0xffffffffu;
    for(const auto byte:bytes){crc^=byte;for(unsigned bit=0u;bit<8u;++bit)
        crc=(crc&1u)?(crc>>1u)^0xedb88320u:crc>>1u;}
    return crc^0xffffffffu;
}
void Checksum(Bytes& bytes){Word(bytes,bytes.size()-4u,Crc({bytes.data(),bytes.size()-4u}));}
Bytes Literal(std::uint32_t mask,std::span<const std::uint8_t> inner){
    Bytes bytes{'D','X','L','C',1u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u};
    Word(bytes,8u,mask);Word(bytes,12u,static_cast<std::uint32_t>(inner.size()));
    bytes.insert(bytes.end(),inner.begin(),inner.end());bytes.resize(bytes.size()+4u,0u);Checksum(bytes);return bytes;
}
void ProfilesAndLiteralOrder(){
    constexpr std::array<std::uint8_t,9u> inner{'D','X','Q','S',3u,0u,0u,0u,0xa5u};
    for(unsigned bits=0u;bits<8u;++bits){
        ++cases;const QuestVr::OriginalLinkerLoadProfile profile{(bits&4u)!=0u,(bits&1u)!=0u,(bits&2u)!=0u};
        const std::uint32_t expectedMask=(bits&1u?0x10000u:0u)|(bits&2u?0x20000u:0u)|(bits&4u?0x40000u:0u);
        const auto expected=Literal(expectedMask,inner);
        const auto encoded=QuestVr::EncodeLoadContextSave(profile,inner);
        Require(encoded==expected,"Independent literal byte/CRC oracle differs from encoded profile envelope");
        const auto decoded=QuestVr::DecodeLoadContextSave(encoded);
        Require(decoded.status==QuestVr::LoadContextSaveStatus::Wrapped&&decoded.profile&&*decoded.profile==profile,
            "One of eight explicit profiles was lost or reclassified raw");
        Require(std::equal(decoded.inner.begin(),decoded.inner.end(),inner.begin(),inner.end())&&
            decoded.inner.data()==encoded.data()+16u,"Decode did not return exact borrowed opaque inner bytes");
        Require(QuestVr::EncodeLoadContextSave(*decoded.profile,decoded.inner)==encoded,"Repeated encoding was not canonical/deterministic");
        const auto original=encoded;
        for(unsigned repeat=0u;repeat<3u;++repeat){const auto again=QuestVr::DecodeLoadContextSave(encoded);
            Require(again.profile==decoded.profile&&encoded==original,"Decode mutated input or produced nondeterministic profile");}
    }
    ++cases;constexpr std::array<std::uint8_t,9u> crcFixture{'1','2','3','4','5','6','7','8','9'};
    Require(Crc(crcFixture)==0xcbf43926u&&QuestVr::SaveBundleDetail::Crc32(crcFixture.data(),crcFixture.size())==0xcbf43926u,
        "CRC polynomial/convention differs from independent published known vector");
    ++cases;const auto empty=QuestVr::EncodeLoadContextSave({},{});
    const auto decoded=QuestVr::DecodeLoadContextSave(empty);
    Require(empty.size()==20u&&decoded.profile.has_value()&&decoded.profile->Mask()==0u&&decoded.inner.empty(),
        "Explicit all-false empty opaque profile became absent raw mode");
}
void LegacyAndPrefixes(){
    const std::array<Bytes,7u> legacy{{{}, {'D','X','Q','S',3u,0u,0u,0u}, {'D','X','Q'},
        {'N'}, {'D','Y'}, {'D','X','L','X'}, {0u,0xffu,1u}}};
    for(const auto& bytes:legacy){++cases;const auto original=bytes;const auto decoded=QuestVr::DecodeLoadContextSave(bytes);
        Require(decoded.status==QuestVr::LoadContextSaveStatus::NotWrapped&&!decoded.profile&&
            decoded.inner.data()==bytes.data()&&decoded.inner.size()==bytes.size()&&bytes==original,
            "Unrelated legacy bytes were wrapped, copied, mutated or treated as explicit mask0");}
    for(const Bytes& bytes:std::array<Bytes,3u>{{{'D'},{'D','X'},{'D','X','L'}}}){
        ++cases;Require(Refuses([&]{QuestVr::DecodeLoadContextSave(bytes);}),"Matching truncated reserved magic prefix fell back to legacy raw");}
}
void Malformed(){
    const Bytes payload{'D','X','Q','S',3u,0u,0u,0u,9u};const auto valid=Literal(0x30000u,payload);
    for(std::size_t length=1u;length<valid.size();++length){
        ++cases;const Bytes truncated(valid.begin(),valid.begin()+length);
        Require(Refuses([&]{QuestVr::DecodeLoadContextSave(truncated);}),"Matching envelope truncation became valid/raw fallback");}
    for(const auto mask:{1u,2u,4u,0x80000u,0x100u,0x30001u,0xffffffffu}){
        ++cases;auto bytes=valid;Word(bytes,8u,mask);Checksum(bytes);
        Require(Refuses([&]{QuestVr::DecodeLoadContextSave(bytes);}),"Unknown/noncanonical profile mask was admitted with valid CRC");}
    for(const auto version:{0u,2u,0x100u,0xffffffffu}){
        ++cases;auto bytes=valid;Word(bytes,4u,version);Checksum(bytes);
        Require(Refuses([&]{QuestVr::DecodeLoadContextSave(bytes);}),"Unsupported profile version fell back to raw or was accepted");}
    for(const auto length:{0u,1u,8u,10u,0xffffffffu}){
        ++cases;auto bytes=valid;Word(bytes,12u,length);Checksum(bytes);
        Require(Refuses([&]{QuestVr::DecodeLoadContextSave(bytes);}),"Declared inner length ignored exact payload/checksum boundary");}
    for(std::size_t offset=4u;offset<valid.size();++offset){
        ++cases;auto bytes=valid;bytes[offset]^=0x01u;
        Require(Refuses([&]{QuestVr::DecodeLoadContextSave(bytes);}),"Header/profile/payload/checksum corruption was not bound");}
    ++cases;auto profileChanged=valid;Word(profileChanged,8u,0x70000u);
    Require(Refuses([&]{QuestVr::DecodeLoadContextSave(profileChanged);}),"CRC did not bind a different otherwise canonical profile");
    ++cases;auto trailing=valid;trailing.push_back(0u);
    Require(Refuses([&]{QuestVr::DecodeLoadContextSave(trailing);}),"Trailing bytes were accepted as a second or unbound payload");
    ++cases;auto counted=valid;counted.insert(counted.end()-4,0u);Checksum(counted);
    Require(Refuses([&]{QuestVr::DecodeLoadContextSave(counted);}),"Valid recomputed CRC bypassed exact inner byte count");
}
void Bounds(){
    const Bytes payload{1u,2u,3u,4u,5u,6u,7u};const auto valid=Literal(0u,payload);
    ++cases;Require(QuestVr::LoadContextSaveInnerLimit(27u)==7u&&
        QuestVr::EncodeLoadContextSave({},payload,27u)==valid,"Header overhead did not reduce the same whole-file allowance");
    ++cases;Require(QuestVr::DecodeLoadContextSave(valid,27u).inner.size()==7u,"Exact whole-file bound refused valid payload");
    ++cases;Require(Refuses([&]{QuestVr::EncodeLoadContextSave({},payload,26u);})&&
        Refuses([&]{QuestVr::DecodeLoadContextSave(valid,26u);}),"One-byte under-bound codec admitted a larger whole file");
    for(const std::size_t maximum:{0u,1u,19u}){
        ++cases;Require(Refuses([&]{QuestVr::LoadContextSaveInnerLimit(maximum);})&&
            Refuses([&]{QuestVr::EncodeLoadContextSave({}, {},maximum);}),"Undersized file policy raised itself to fit wrapper");}
    for(const auto maximum:{QuestVr::kMaximumSaveRuntimeBytes+1u,std::numeric_limits<std::size_t>::max()}){
        ++cases;Require(Refuses([&]{QuestVr::LoadContextSaveInnerLimit(maximum);})&&
            Refuses([&]{QuestVr::EncodeLoadContextSave({},payload,maximum);})&&
            Refuses([&]{QuestVr::DecodeLoadContextSave(valid,maximum);}),"Caller raised the original whole-file policy");}
    ++cases;const Bytes raw{0u};Require(QuestVr::DecodeLoadContextSave(raw,1u).status==QuestVr::LoadContextSaveStatus::NotWrapped&&
        Refuses([&]{QuestVr::DecodeLoadContextSave(raw,0u);}),"Unwrapped passthrough bypassed lower whole-file bounds");
    ++cases;Require(QuestVr::LoadContextSaveInnerLimit()==QuestVr::kMaximumSaveRuntimeBytes-20u,
        "Default max inner bytes raised the preexisting16MiB file cap");
}
} // namespace
int main(){try{ProfilesAndLiteralOrder();LegacyAndPrefixes();Malformed();Bounds();
    std::cout<<"Load-context save structural controls: "<<checks<<" checks / "<<cases<<" cases; no runtime/map acceptance\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
