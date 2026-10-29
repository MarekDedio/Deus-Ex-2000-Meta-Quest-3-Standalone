#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace QuestVr {

namespace CommandLineParamDetail {
inline void Step(const std::function<void()>& work) {
    if(!work)throw std::invalid_argument("Original ParseParam requires borrowed root work");
    work();
}
inline char16_t Unit(std::u16string_view text,std::size_t index,const std::function<void()>& work) {
    Step(work);return index<text.size()?text[index]:char16_t{};
}
inline char16_t Upper(char16_t unit) {
    return unit>=u'a'&&unit<=u'z'?static_cast<char16_t>(unit-(u'a'-u'A')):unit;
}
inline bool Alnum(char16_t unit) {
    return (unit>=u'A'&&unit<=u'Z')||(unit>=u'0'&&unit<=u'9');
}
inline std::size_t Length(std::u16string_view text,const std::function<void()>& work) {
    std::size_t length{};
    while(Unit(text,length,work))++length;
    return length;
}
inline bool EqualTail(std::u16string_view text,std::size_t at,std::u16string_view parameter,
    std::size_t count,const std::function<void()>& work) {
    // Core.appStrfind delegates only the remaining units to _wcsnicmp. The
    // portable contract establishes the original default C ASCII comparison,
    // not arbitrary CRT locale mappings for a reached non-ASCII comparison.
    for(std::size_t i=0u;i<count;++i) {
        if(i>std::numeric_limits<std::size_t>::max()-at)
            throw std::runtime_error("Original ParseParam bounded comparison index overflows");
        const auto left=Unit(text,at+i,work),right=Unit(parameter,i+1u,work);
        if(left>127u||right>127u)
            throw std::runtime_error("Original ParseParam reached an unestablished non-ASCII CRT comparison");
        if(Upper(left)!=Upper(right))return false;
        if(!left)return true;
    }
    return true;
}
inline std::optional<std::size_t> Find(std::u16string_view text,std::size_t start,
    std::u16string_view parameter,const std::function<void()>& work) {
    // Original Core10145CA0..10145D7B: ASCII-upcase first unit, strlen-1
    // remaining units, fresh previous-is-alnum=false for EACH appStrfind.
    const auto first=Upper(Unit(parameter,0u,work));
    const auto length=Length(parameter,work);
    bool previousAlnum{};
    for(std::size_t at=start;;++at) {
        const auto unit=Unit(text,at,work);
        if(!unit)return std::nullopt;
        const auto folded=Upper(unit);
        if(!previousAlnum&&folded==first&&length&&EqualTail(text,at+1u,parameter,length-1u,work))return at;
        previousAlnum=Alnum(folded);
    }
}
} // namespace CommandLineParamDetail

// Source-matched Core ParseParam thunk10102C5C ->10146D00..10146DB2.
// Read-only, allocation-free normal path over borrowed UTF16 CStrings. It is
// NOT a command-line tokenizer: matches are searched from Stream+1, require
// an immediately preceding '-' or '/', and need NO trailing word boundary.
// appCmdLine's owning embedding and its lifetime are the caller's authority.
// Every bounded read shares the caller's work budget; no local reset/bypass.
inline bool OriginalParseParam(std::u16string_view commandLine,std::u16string_view parameter,
    const std::function<void()>& work) {
    using CommandLineParamDetail::Unit;
    if(!Unit(commandLine,0u,work))return false;
    std::size_t previous{};
    for(;;) {
        const auto found=CommandLineParamDetail::Find(commandLine,previous+1u,parameter,work);
        if(!found)return false;
        const auto before=Unit(commandLine,*found-1u,work);
        if(before==u'-'||before==u'/')return true;
        previous=*found;
    }
}

} // namespace QuestVr
