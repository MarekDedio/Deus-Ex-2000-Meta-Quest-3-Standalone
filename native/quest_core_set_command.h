#pragma once

#include "quest_input_key_queries.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace QuestVr::CoreSetCommand {
inline constexpr std::size_t TokenCapacity=256u;
inline constexpr std::size_t MaximumCommandUnits=NativeInputKeyQueries::MaximumCommandUnits;

struct Token {
    std::u16string value;
    std::size_t nextUnit{};
    bool present{};
};
struct Packet {
    std::u16string className,propertyName,value;
    std::size_t nextUnit{};
    bool classTokenPresent{},propertyTokenPresent{},propertyParsed{},valueRead{};
};
struct Result {bool recognized{};std::optional<Packet> packet;};

// Operations supplies Step() and Admit(peakBytes). These are shared owner work
// and scratch admission callbacks, not fresh per-token budgets. Admit receives
// the complete helper-owned live+staged footprint for the pending allocation;
// it does not retain memory or establish an engine/provider authority.
namespace Detail {
inline std::size_t Add(std::size_t first,std::size_t second) {
    if(second>std::numeric_limits<std::size_t>::max()-first)
        throw std::runtime_error("Native Core SET scratch size overflow");
    return first+second;
}
inline std::size_t UnitsBytes(std::size_t units) {
    if(units>std::numeric_limits<std::size_t>::max()/sizeof(char16_t))
        throw std::runtime_error("Native Core SET scratch unit size overflow");
    return units*sizeof(char16_t);
}
inline std::size_t Growth(std::size_t units) {
    return std::max<std::size_t>(32u,2u*(units+1u));
}
inline std::size_t PacketBytes(const Packet& packet) {
    auto units=Add(packet.className.capacity(),packet.propertyName.capacity());
    units=Add(Add(units,packet.value.capacity()),3u);
    return Add(sizeof(Packet),UnitsBytes(units));
}
template<class Operations>
std::u16string_view Input(std::u16string_view input,Operations& operations) {
    if(input.size()>MaximumCommandUnits)
        throw std::runtime_error("Native Core SET command unit limit exceeded");
    // Validate admitted source storage BEFORE its CString boundary. Rechecking
    // each phase also bounds all actual helper scans, including ParseCommand.
    operations.Step();std::size_t length{};
    for(;length<input.size();++length) {operations.Step();if(!input[length])break;}
    return input.substr(0u,length);
}
inline bool Space(char16_t value) {return value==u' '||value==u'\t';}
template<class Operations>
void CheckPhaseInput(std::u16string_view input,const Packet& packet,Operations& operations) {
    operations.Step();
    if(packet.nextUnit>input.size())
        throw std::runtime_error("Native Core SET phase cursor exceeds admitted CString");
}
}

// Core ParseToken thunk10102b8f ->10147ba0..10147c81, capacity256 and UseEscape1.
// This is NOT ReadToken: unquoted punctuation is retained; only space/tab end
// the token. Unquoted overflow consumes the entire token, whereas quoted
// overflow stops at the first uncopied unit (but still consumes a closing
// quote when that is the next unit). An absent closing quote is not an error.
template<std::size_t Capacity,class Operations>
Token ParseToken(std::u16string_view supplied,std::size_t cursor,Operations& operations,
    std::size_t retainedBytes=0u,bool useEscape=true) {
    static_assert(Capacity>1u&&Capacity<=TokenCapacity);
    const auto input=Detail::Input(supplied,operations);
    if(cursor>input.size())throw std::runtime_error("Native Core SET token cursor exceeds admitted CString");
    while(cursor<input.size()&&Detail::Space(input[cursor])) {operations.Step();++cursor;}
    operations.Admit(Detail::Add(Detail::Add(retainedBytes,sizeof(Token)),Detail::UnitsBytes(Detail::Growth(Capacity-1u))));
    Token result;result.value.reserve(Capacity-1u);
    if(result.value.capacity()>Detail::Growth(Capacity-1u))
        throw std::runtime_error("Native Core SET token capacity exceeds admitted library growth");
    if(cursor<input.size()&&input[cursor]==u'"') {
        operations.Step();++cursor;
        while(cursor<input.size()) {
            operations.Step();
            const auto unit=input[cursor];
            if(unit==u'"'||result.value.size()==Capacity-1u)break;
            ++cursor;
            if(unit==u'\\'&&useEscape) {
                operations.Step();
                // Retail advances through NUL and then peeks the following
                // word for a quote. That storage is outside this CString;
                // refuse instead of inventing readable zero padding/cursor.
                if(cursor==input.size())
                    throw std::runtime_error("Native Core SET quoted escape crosses admitted CString");
                result.value.push_back(input[cursor++]);
            } else result.value.push_back(unit);
        }
        if(cursor<input.size()&&input[cursor]==u'"') {operations.Step();++cursor;}
    } else {
        while(cursor<input.size()&&!Detail::Space(input[cursor])) {
            operations.Step();
            if(result.value.size()<Capacity-1u)result.value.push_back(input[cursor]);
            ++cursor;
        }
    }
    result.nextUnit=cursor;result.present=!result.value.empty();return result;
}
template<class Operations>
Token ParseToken256(std::u16string_view supplied,std::size_t cursor,Operations& operations,
    std::size_t retainedBytes=0u,bool useEscape=true) {
    return ParseToken<256u>(supplied,cursor,operations,retainedBytes,useEscape);
}

// Core101532da recognizes SET, then parses Class. The real caller must perform
// actual StaticFindObject BEFORE ReadProperty, and actual inherited FindField
// BEFORE ReadValue. Thus an unknown Class/field can log/return handled without
// allocating/parsing later arguments. A recognized SET remains handled even
// when either ParseToken returned false; its buffer is then the empty string.
// All phases require the same immutable, caller-owned command storage.
template<class Operations>
Result Begin(std::u16string_view supplied,Operations& operations) {
    const auto input=Detail::Input(supplied,operations);std::size_t cursor{};
    // ParseCommand's scan is bounded by the just-charged source and shared
    // input limit; use its genuine suffix and leading/trailing-space profile.
    if(!NativeInputKeyQueries::ParseCommand(input,cursor,u"SET"))return {};
    operations.Admit(sizeof(Result)+sizeof(Packet)+sizeof(Token)+Detail::Growth(TokenCapacity-1u)*sizeof(char16_t));
    Result result;result.recognized=true;result.packet.emplace();
    auto token=ParseToken256(input,cursor,operations,sizeof(Result)+Detail::PacketBytes(*result.packet));
    result.packet->className=std::move(token.value);result.packet->classTokenPresent=token.present;
    result.packet->nextUnit=token.nextUnit;return result;
}
template<class Operations>
void ReadProperty(std::u16string_view supplied,Packet& packet,Operations& operations) {
    const auto input=Detail::Input(supplied,operations);Detail::CheckPhaseInput(input,packet,operations);
    if(!packet.classTokenPresent||packet.propertyParsed||packet.valueRead)
        throw std::runtime_error("Native Core SET property phase has invalid ordering");
    auto token=ParseToken256(input,packet.nextUnit,operations,Detail::PacketBytes(packet));
    packet.propertyName=std::move(token.value);packet.propertyTokenPresent=token.present;
    packet.nextUnit=token.nextUnit;packet.propertyParsed=true;
}
template<class Operations>
void ReadValue(std::u16string_view supplied,Packet& packet,Operations& operations) {
    const auto input=Detail::Input(supplied,operations);Detail::CheckPhaseInput(input,packet,operations);
    if(!packet.classTokenPresent||!packet.propertyParsed||!packet.propertyTokenPresent||packet.valueRead)
        throw std::runtime_error("Native Core SET value phase has invalid ordering");
    auto cursor=packet.nextUnit;
    while(cursor<input.size()&&input[cursor]==u' ') {operations.Step();++cursor;}
    const auto remaining=input.substr(cursor);
    operations.Admit(Detail::Add(Detail::Add(Detail::PacketBytes(packet),sizeof(std::u16string)),
        Detail::UnitsBytes(Detail::Growth(remaining.size()))));
    std::u16string value;value.reserve(remaining.size());
    if(value.capacity()>Detail::Growth(remaining.size()))
        throw std::runtime_error("Native Core SET value capacity exceeds admitted library growth");
    for(const auto unit:remaining) {operations.Step();value.push_back(unit);}
    packet.value=std::move(value);packet.nextUnit=cursor;packet.valueRead=true;
}

// Parsing-only convenience. It intentionally does not resolve a Class/field,
// execute SET, fall through FExec providers, mutate a UObject or persist config.
// Actual Core.StaticExec services must use the phased functions above.
template<class Operations>
Result Parse(std::u16string_view supplied,Operations& operations) {
    auto result=Begin(supplied,operations);
    if(!result.recognized||!result.packet->classTokenPresent)return result;
    ReadProperty(supplied,*result.packet,operations);
    if(result.packet->propertyTokenPresent)ReadValue(supplied,*result.packet,operations);
    return result;
}
} // namespace QuestVr::CoreSetCommand
