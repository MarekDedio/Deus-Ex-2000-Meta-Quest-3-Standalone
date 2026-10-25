#pragma once

#include "quest_unreal_string.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace QuestVr::NativeInputKeyQueries {
inline constexpr std::size_t MaximumCommandUnits=16384u,MaximumEnumNames=65536u;
namespace Detail {
inline bool Letter(char16_t value) {
    return (value>=u'a'&&value<=u'z')||(value>=u'A'&&value<=u'Z');
}
inline bool Digit(char16_t value){return value>=u'0'&&value<=u'9';}
inline bool Space(char16_t value){return value==u' '||value==u'\t';}
inline bool CrtSpace(char16_t value){return Space(value)||value==u'\r'||value==u'\n'||value==u'\v'||value==u'\f';}
inline std::u16string_view Input(std::u16string_view text) {
    // Admit the supplied storage before applying the native CString boundary.
    if(text.size()>MaximumCommandUnits)throw std::runtime_error("Native Input command unit limit exceeded");
    return UnrealString::CString(text);
}
}

// Core10147A20..10147B42. On mismatch/suffix rejection retain skipped leading
// spaces/tabs, but rewind the command itself. No general whitespace/tokenizer.
inline bool ParseCommand(std::u16string_view input,std::size_t& cursor,std::u16string_view command) {
    input=Detail::Input(input);command=Detail::Input(command);
    if(cursor>input.size()||command.empty())throw std::runtime_error("Native Input ParseCommand invalid cursor/token");
    while(cursor<input.size()&&Detail::Space(input[cursor]))++cursor;
    if(command.size()>input.size()-cursor)return false;
    for(std::size_t i=0u;i<command.size();++i)
        if(UnrealString::FoldAscii(input[cursor+i])!=UnrealString::FoldAscii(command[i]))return false;
    const auto end=cursor+command.size();
    if(end<input.size()&&(Detail::Letter(input[end])||Detail::Digit(input[end])))return false;
    cursor=end;while(cursor<input.size()&&Detail::Space(input[cursor]))++cursor;
    return true;
}

// appAtoi -> MSVCRT _wtoi, explicitly ordinary C-locale signed32 domain.
// Overflow is an unestablished CRT profile, never a fabricated index/clamp.
inline std::int32_t AppAtoi(std::u16string_view input) {
    input=Detail::Input(input);std::size_t cursor{};
    while(cursor<input.size()&&Detail::CrtSpace(input[cursor]))++cursor;
    const bool negative=cursor<input.size()&&input[cursor]==u'-';
    if(cursor<input.size()&&(negative||input[cursor]==u'+'))++cursor;
    std::uint64_t magnitude{};const std::uint64_t limit=negative?2147483648ull:2147483647ull;
    while(cursor<input.size()&&Detail::Digit(input[cursor])) {
        const auto digit=static_cast<unsigned>(input[cursor]-u'0');
        if(magnitude>(limit-digit)/10u)throw std::runtime_error("Native Input _wtoi signed overflow profile is not established");
        magnitude=magnitude*10u+digit;++cursor;
    }
    if(negative&&magnitude==2147483648ull)return std::numeric_limits<std::int32_t>::min();
    const auto value=static_cast<std::int32_t>(magnitude);return negative?-value:value;
}

// Ops owns the genuine Input/Enum/FName/config/output-device authorities and
// all work/retained limits. These helpers allocate no UObject or FName and do
// not claim configured Input construction, reset/event dispatch or full FExec.
// Name{} is actual FName None. NameText returns actual spelling (UTF16 units),
// FindName is FNAME_Find only, Binding reads live FString, Log appends CString
// with NO newline under an explicitly admitted ordinary logging profile.
template<class Ops> std::u16string GetKeyName(Ops& operations,std::int32_t key) {
    operations.Step();
    if(key<0||key>=255)return {};
    const auto index=static_cast<std::size_t>(key);
    if(index>=operations.EnumSize())throw std::runtime_error("Native Input key exceeds actual Enum storage");
    const auto first=operations.NameText(operations.EnumName(index));
    if(Detail::Input(first).size()<=3u)return {};
    // Retail rereads Enum+Names+FName after its initial length test. Do not
    // substitute a once-captured display-name table or inspect an IK_ prefix.
    const auto text=operations.NameText(operations.EnumName(index));
    const auto units=Detail::Input(text);
    if(units.size()<3u)throw std::runtime_error("Native Input live name changed beyond supported CString storage");
    return std::u16string(units.substr(3u));
}
template<class Ops> std::optional<std::size_t> FindKeyName(Ops& operations,std::u16string_view key) {
    key=Detail::Input(key);operations.Step();
    // Original appSprintf target is256 UTF16 units including NUL and IK_.
    if(key.size()>252u)throw std::runtime_error("Native Input FindKeyName format buffer limit exceeded");
    std::u16string formatted=u"IK_";formatted.append(key);
    const auto name=operations.FindName(formatted);
    if(name==typename Ops::Name{})return std::nullopt;
    for(std::size_t index=0u;index<operations.EnumSize();++index) {
        operations.Step();
        if(index>=MaximumEnumNames)throw std::runtime_error("Native Input Enum traversal limit exceeded");
        if(operations.EnumName(index)==name)return index;
    }
    return std::nullopt;
}
template<class Ops> bool Exec(Ops& operations,std::u16string_view command) {
    command=Detail::Input(command);operations.Step();std::size_t cursor{};
    if(ParseCommand(command,cursor,u"KEYNAME")) {
        const auto value=GetKeyName(operations,AppAtoi(command.substr(cursor)));
        operations.Log(UnrealString::CString(value));return true;
    }
    if(ParseCommand(command,cursor,u"KEYBINDING")) {
        const auto key=FindKeyName(operations,command.substr(cursor));
        if(key) {
            const auto binding=operations.Binding(*key);
            operations.Log(Detail::Input(binding));
        }
        return true; // A genuine missing name is handled with no output.
    }
    return false; // Caller must continue/refuse unaudited FExec providers.
}
} // namespace QuestVr::NativeInputKeyQueries
