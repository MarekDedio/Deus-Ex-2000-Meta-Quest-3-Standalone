#pragma once

#include "quest_unreal_string.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace QuestVr::NativeConfigImport {

// Original Core ReadToken/UInt/UBool/UObject/UClass ImportText semantics, not
// ParseToken, generic VM coercion or permission to load/construct a UObject.
// Offsets count UTF16 units into the admitted CString. `returned=false` means
// original nullptr, NOT 'no write': failed object lookup can publish None.
struct Result {bool returned{};std::size_t nextUnit{};};
struct Token {Result result;std::u16string value;};
inline constexpr std::size_t MaximumInputUnits=16384u,TokenCapacity=1024u;
namespace Detail {
[[noreturn]] inline void Fail(const char* why) {throw std::runtime_error(std::string("Native config import: ")+why);}
inline std::u16string_view Input(std::u16string_view value) {
    if(value.size()>MaximumInputUnits)Fail("input unit limit exceeded");
    return UnrealString::CString(value);
}
inline bool Letter(char16_t c) {return (c>=u'a'&&c<=u'z')||(c>=u'A'&&c<=u'Z');}
inline bool Digit(char16_t c) {return c>=u'0'&&c<=u'9';}
inline unsigned Hex(char16_t c) {
    if(Digit(c))return static_cast<unsigned>(c-u'0');
    if(c>=u'a'&&c<=u'f')return static_cast<unsigned>(c-u'a'+10u);
    if(c>=u'A'&&c<=u'F')return static_cast<unsigned>(c-u'A'+10u);
    return 0u; // Native quoted escapes map every other nibble to literal0.
}
inline char16_t At(std::u16string_view text,std::size_t i) {return i<text.size()?text[i]:u'\0';}
inline bool Equal(std::u16string_view a,std::u16string_view b) {return UnrealString::EqualInsensitive(a,b);}
}

inline Token ReadToken(std::u16string_view input,bool dotted=false,std::size_t offset=0u,std::size_t capacity=TokenCapacity) {
    using namespace Detail;input=Input(input);
    if(capacity<2u||capacity>4096u)Fail("token capacity is outside audited buffers");
    if(offset>input.size())Fail("token cursor is outside input");
    Token result;auto cursor=offset;const auto first=At(input,cursor);
    if(first==u'"') {
        ++cursor;
        while(At(input,cursor)!=u'\0'&&At(input,cursor)!=u'"'&&At(input,cursor)!=u'\r'&&At(input,cursor)!=u'\n') {
            if(result.value.size()>=capacity-1u)return result;
            if(input[cursor]!=u'\\')result.value.push_back(input[cursor++]);
            else {
                ++cursor;
                if(At(input,cursor)==u'\\') {result.value.push_back(u'\\');++cursor;}
                else {
                    // Original reads both nibble positions even if the first
                    // is NUL. Refuse only a read past the existing terminator,
                    // not an ordinary malformed/unterminated token result.
                    if(cursor>=input.size())Fail("quoted hex escape reads beyond terminator");
                    result.value.push_back(static_cast<char16_t>((Hex(input[cursor])<<4u)|Hex(At(input,cursor+1u))));
                    cursor+=2u;
                    if(cursor>input.size())Fail("quoted hex escape advances beyond terminator");
                }
            }
        }
        // Native rejects a token at exactly capacity-1 even if a quote follows.
        if(result.value.size()==capacity-1u||At(input,cursor)!=u'"')return result;
        result.result={true,cursor+1u};return result;
    }
    if(Letter(first)||Digit(first)) {
        while(Letter(At(input,cursor))||Digit(At(input,cursor))||At(input,cursor)==u'_'||At(input,cursor)==u'-'||
            (dotted&&At(input,cursor)==u'.')) {
            if(result.value.size()==capacity-1u)return result;
            result.value.push_back(input[cursor++]);
        }
        if(result.value.size()==capacity-1u)return result;
        result.result={true,cursor};return result;
    }
    // Retail copies one nonalphanumeric unit but DOES NOT advance the input
    // pointer, including whitespace, '_' and NUL. No leading-space skipping.
    if(first)result.value.push_back(first);
    result.result={true,cursor};return result;
}

inline Result Integer(std::u16string_view input,std::int32_t& destination) {
    using namespace Detail;input=Input(input);
    if(input.empty()||(!Digit(input.front())&&input.front()!=u'-'))return {true,0u};
    std::size_t digits=input.front()==u'-'?1u:0u;const bool negative=digits!=0u;
    std::uint64_t magnitude{};const auto maximum=negative?std::uint64_t{2147483648u}:std::uint64_t{2147483647u};
    while(digits<input.size()&&Digit(input[digits])) {
        const auto digit=static_cast<unsigned>(input[digits]-u'0');
        if(magnitude>(maximum-digit)/10u)Fail("_wtoi signed overflow profile is not established");
        magnitude=magnitude*10u+digit;++digits;
    }
    std::int32_t value{};
    if(negative)value=magnitude==2147483648u?std::numeric_limits<std::int32_t>::min():-static_cast<std::int32_t>(magnitude);
    else value=static_cast<std::int32_t>(magnitude);
    // UInt ImportText consumes ANY run of '-' and digits after calling appAtoi
    // once; e.g. --1 writes0 but consumes3, and1-2 writes1 but consumes3.
    std::size_t cursor{};while(cursor<input.size()&&(Digit(input[cursor])||input[cursor]==u'-'))++cursor;
    destination=value;return {true,cursor};
}
inline Result Boolean(std::u16string_view input,std::uint32_t bitMask,std::uint32_t& destination) {
    const auto token=ReadToken(input);if(!token.result.returned)return {};
    if(Detail::Equal(token.value,u"1")||Detail::Equal(token.value,u"True"))destination|=bitMask;
    else if(Detail::Equal(token.value,u"0")||Detail::Equal(token.value,u"False"))destination&=~bitMask;
    else return {};
    return token.result;
}

// Core101674D0..1016755B: names always ReadToken(1024, dotted0), then
// FNAME_Add1. The callback supplies the real FName arena, not a string map.
template<class NameId,class AddName>
Result Name(std::u16string_view input,NameId& destination,AddName&& addName) {
    const auto token=ReadToken(input);if(!token.result.returned)return {};
    const auto value=addName(UnrealString::CString(token.value));
    destination=value;return token.result;
}

// Core10167980..10167AE0. Bit2 selects ReadToken(4096,dotted0); flags0
// copies the ENTIRE CString and returns its original pointer, not end-of-text.
inline Result String(std::u16string_view input,std::uint32_t flags,std::u16string& destination) {
    input=Detail::Input(input);
    if(flags&2u) {
        const auto token=ReadToken(input,false,0u,4096u);if(!token.result.returned)return {};
        destination=std::u16string(UnrealString::CString(token.value));return token.result;
    }
    destination=std::u16string(input);return {true,0u};
}

// Find must implement StaticFindObject(class,ANY_PACKAGE,path,Exact0) using
// actual registered object identity. Required-class tokens/derived constraints
// come from the declaring property's native metadata, not strings in the INI.
// Typed quoted syntax first finds an actual UClass (classClass), then uses it
// to find its object. ClassProperty finally requires EXACT Core.Class metaclass
// and actual Super ancestry. The callbacks supply those separate authorities.
template<class Find,class IsExactClass,class Derives>
Result Class(std::u16string_view input,std::uintptr_t objectClass,std::uintptr_t classClass,
    std::uintptr_t metaClass,std::uintptr_t& destination,Find&& find,IsExactClass&& isExactClass,Derives&& derives) {
    using namespace Detail;input=Input(input);const auto first=ReadToken(input,true);
    if(!first.result.returned)return {};
    if(Equal(first.value,u"None")) {destination=0u;return first.result;}
    auto cursor=first.result.nextUnit;while(At(input,cursor)==u' ')++cursor;
    std::uintptr_t found{};
    if(At(input,cursor)!=u'\'') {
        found=find(objectClass,UnrealString::CString(first.value));
        // UObject.ImportText has already advanced past one delimiter here,
        // even NUL. Its return is only exposed for a successful lookup.
        ++cursor;
    }else {
        const auto second=ReadToken(input,true,cursor+1u);if(!second.result.returned)return {};
        cursor=second.result.nextUnit;if(At(input,cursor)!=u'\'')return {};
        ++cursor;const auto explicitClass=find(classClass,UnrealString::CString(first.value));
        if(!explicitClass)return {};
        found=find(explicitClass,UnrealString::CString(second.value));
    }
    // A real lookup miss writes None, even though the result is nullptr. Parse
    // errors and unknown explicit class above leave the prior value unchanged.
    if(!found) {destination=0u;return {};}
    const auto prepared=isExactClass(found)&&derives(found,metaClass)?found:0u;
    destination=prepared; // Unsupported resolver refusal publishes no prefix.
    return {true,cursor}; // Class constraint failure still returns token cursor.
}

} // namespace QuestVr::NativeConfigImport
