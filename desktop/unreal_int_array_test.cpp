#include "quest_unreal_int_array.h"

#include <iostream>

namespace {
using Bytes=std::vector<std::uint8_t>;
using Values=std::vector<std::int32_t>;
std::size_t checks{},rejections{};

void Require(bool condition,const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Reject(const Bytes& bytes,const std::string& message,std::size_t cap=262144u) {
    bool rejected{};
    try { (void)QuestVr::DecodeAuthoredIntArray(bytes,cap); }
    catch (const std::runtime_error& error) {
        rejected=std::string(error.what()).rfind("Authored IntArray: ",0u)==0u;
    }
    Require(rejected,message); ++rejections;
}
Bytes Index(std::uint32_t magnitude,bool negative=false) {
    Bytes result;
    auto first=static_cast<std::uint8_t>(magnitude&0x3fu);
    magnitude>>=6u;
    if (negative) first|=0x80u;
    if (magnitude) first|=0x40u;
    result.push_back(first);
    while (magnitude) {
        auto byte=static_cast<std::uint8_t>(magnitude&0x7fu);
        magnitude>>=7u;
        if (magnitude) byte|=0x80u;
        result.push_back(byte);
    }
    return result;
}
void Append(Bytes& bytes,std::int32_t value) {
    std::uint32_t bits{};
    std::memcpy(&bits,&value,sizeof(value));
    for (unsigned byte=0u; byte<4u; ++byte)
        bytes.push_back(static_cast<std::uint8_t>(bits>>(byte*8u)));
}
Bytes Payload(const Bytes& prefix,const Values& values) {
    Bytes result=prefix;
    for (const auto value : values) Append(result,value);
    return result;
}
void ScalarWireValues() {
    // Independent literal fixture demonstrates little-endian payload values,
    // signed extremes, authored order, duplicates, and no per-item CompactIndex.
    const Bytes bytes{6u,0x78u,0x56u,0x34u,0x12u,
        0xffu,0xffu,0xffu,0xffu,0u,0u,0u,0x80u,
        0xffu,0xffu,0xffu,0x7fu,0u,0u,0u,0u,0xffu,0xffu,0xffu,0xffu};
    const Values expected{0x12345678,-1,std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max(),0,-1};
    Require(QuestVr::DecodeAuthoredIntArray(bytes)==expected,"Signed LE32 values or authored order changed");
    Require(QuestVr::DecodeAuthoredIntArray(bytes,6u)==expected,"Exact element budget rejected");
    Reject(bytes,"Element cap was ignored",5u);
    Reject(bytes,"Zero element cap admitted a nonempty array",0u);
    for (std::size_t size=0u; size<bytes.size(); ++size)
        Reject(Bytes(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(size)),
            "Truncated element payload accepted");
    auto trailing=bytes;
    for (unsigned extra=0u; extra<5u; ++extra) {
        trailing.push_back(0u);
        Reject(trailing,"Trailing bytes or surplus whole elements accepted");
    }
}
void NonminimalCounts() {
    for (const auto& prefix : {Bytes{0u},Bytes{0x80u},Bytes{0x40u,0u},Bytes{0xc0u,0u},
            Bytes{0x40u,0x80u,0u},Bytes{0xc0u,0x80u,0u},
            Bytes{0x40u,0x80u,0x80u,0u},Bytes{0xc0u,0x80u,0x80u,0u},
            Bytes{0x40u,0x80u,0x80u,0x80u,0u},Bytes{0xc0u,0x80u,0x80u,0x80u,0u}}) {
        Require(QuestVr::DecodeAuthoredIntArray(prefix,0u).empty(),"Valid nonminimal/signed zero count rejected");
        auto trailing=prefix; trailing.push_back(0u);
        Reject(trailing,"Zero count accepted trailing payload");
    }
    const Values one{-123456789};
    for (const auto& prefix : {Bytes{1u},Bytes{0x41u,0u},Bytes{0x41u,0x80u,0u},
            Bytes{0x41u,0x80u,0x80u,0u},Bytes{0x41u,0x80u,0x80u,0x80u,0u}})
        Require(QuestVr::DecodeAuthoredIntArray(Payload(prefix,one),1u)==one,
            "Valid nonminimal positive count rejected or element boundary changed");
    const Values sixtyFour(64u,-17);
    Require(QuestVr::DecodeAuthoredIntArray(Payload(Bytes{0x40u,0x81u,0u},sixtyFour))==sixtyFour,
        "Nonminimal count lost a continuation magnitude bit");
}
void CountBoundariesAndBudgets() {
    for (const std::uint32_t count : {1u,63u,64u,8191u,8192u,262144u}) {
        Values expected(count);
        for (std::size_t i=0u; i<expected.size(); ++i)
            expected[i]=static_cast<std::int32_t>(i)-static_cast<std::int32_t>(count/2u);
        const auto bytes=Payload(Index(count),expected);
        Require(QuestVr::DecodeAuthoredIntArray(bytes)==expected,"Canonical count transition or default cap boundary failed");
        Require(QuestVr::DecodeAuthoredIntArray(bytes,count)==expected,"Explicit exact cap boundary failed");
        Reject(bytes,"Below-count cap admitted payload",count-1u);
    }
    const Values beyondDefault(262145u,19);
    const auto bytes=Payload(Index(static_cast<std::uint32_t>(beyondDefault.size())),beyondDefault);
    Reject(bytes,"Default element cap was ignored");
    Require(QuestVr::DecodeAuthoredIntArray(bytes,beyondDefault.size())==beyondDefault,
        "Explicit caller budget was silently clamped to the default");
    // A raised budget still cannot turn a missing/mismatched payload into a
    // potentially enormous result allocation. All these counts are valid wire.
    for (const std::uint32_t count : {1048575u,1048576u,134217727u,134217728u,0x7fffffffu})
        Reject(Index(count),"Large count allocated before exact payload validation",
            std::numeric_limits<std::size_t>::max());
}
void InvalidCompactCounts() {
    Reject({},"Missing count accepted");
    for (const auto& bytes : {Bytes{0x40u},Bytes{0x40u,0x80u},Bytes{0x40u,0x80u,0x80u},
            Bytes{0x40u,0x80u,0x80u,0x80u},Bytes{0xc0u},Bytes{0xc0u,0x80u,0x80u}})
        Reject(bytes,"Truncated continuation accepted");
    for (const auto magnitude : {1u,63u,64u,8192u,0x7fffffffu,0x80000000u})
        Reject(Index(magnitude,true),"Negative count accepted",std::numeric_limits<std::size_t>::max());
    // Fifth-byte high magnitude bits cannot wrap around to zero or another
    // small valid count. Fifth-byte continuation is never a sixth-byte escape.
    for (const auto& bytes : {Bytes{0x40u,0x80u,0x80u,0x80u,0x10u},
            Bytes{0x40u,0x80u,0x80u,0x80u,0x20u},Bytes{0x40u,0x80u,0x80u,0x80u,0x7fu},
            Bytes{0xc0u,0x80u,0x80u,0x80u,0x11u},Bytes{0xc0u,0x80u,0x80u,0x80u,0x20u},
            Bytes{0x40u,0x80u,0x80u,0x80u,0x80u},Bytes{0x40u,0x80u,0x80u,0x80u,0x80u,0u},
            Bytes{0xc0u,0x80u,0x80u,0x80u,0x80u,0u},Bytes{0x40u,0x80u,0x80u,0x80u,0x81u,0u}})
        Reject(bytes,"Overflow/overlong count accepted",std::numeric_limits<std::size_t>::max());
    Reject(Payload(Index(2u),Values{1}),"Declared count exceeded whole-element payload");
    Reject(Payload(Index(1u),Values{1,2}),"Declared count ignored surplus whole-element payload");
    // Positive minimum-overflow magnitude with enough bytes for the count it
    // could incorrectly alias is still rejected as an overflow, not redecoded.
    Reject(Bytes{0x41u,0x80u,0x80u,0x80u,0x20u,0u,0u,0u,0u},
        "High magnitude bits aliased a small array count");
}
} // namespace

int main() {
    try {
        ScalarWireValues(); NonminimalCounts(); CountBoundariesAndBudgets(); InvalidCompactCounts();
        std::cout << "Authored IntArray tests passed: " << checks << " checks, " << rejections << " rejections\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Authored IntArray test failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
