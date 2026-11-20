#pragma once

#include "quest_unreal_string.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace QuestVr {

struct OriginalLoadMapUrl {
    std::u16string protocol,host,map,portal;
    std::vector<std::u16string> options;
    std::int32_t port{};
    bool valid{true};

    bool operator==(const OriginalLoadMapUrl&) const=default;
};

struct OriginalLoadMapOptions {
    std::u16string initGame,gameClass;
};

// The owned portable FURL limit also bounds SpawnPlayActor's ordered options.
// SaveURLConfig additionally copies each option to a native 1024-wide buffer,
// including its terminating NUL. Do not truncate to make an oversized fit.
inline constexpr std::size_t OriginalSaveUrlConfigMaxOptions=1024u;
inline constexpr std::size_t OriginalSaveUrlConfigMaxOptionUnits=1023u;
inline constexpr std::size_t OriginalLoadMapInitGameMaxUnits=1023u;
inline constexpr std::size_t OriginalLoadMapGameClassMaxUnits=255u;

namespace SaveUrlConfigDetail {
template<class Step>
void ValidateOptions(std::span<const std::u16string> options,Step& step) {
    if(options.size()>OriginalSaveUrlConfigMaxOptions)
        throw std::runtime_error("SaveURLConfig exceeds the portable FURL option count");
    for(const auto& option:options) {
        if(option.size()>OriginalSaveUrlConfigMaxOptionUnits)
            throw std::runtime_error("SaveURLConfig option exceeds its native wide-buffer bound");
        step(option.size()+1u);
        if(option.find(u'\0')!=std::u16string::npos)
            throw std::runtime_error("SaveURLConfig option contains an embedded NUL");
    }
}

inline bool IsAsciiAlnum(char16_t unit) {
    const auto folded=UnrealString::FoldAscii(unit);
    return (folded>=u'a'&&folded<=u'z')||(unit>=u'0'&&unit<=u'9');
}

inline bool IsUnquotedParseStop(char16_t unit) {
    return unit==u' '||unit==u'\r'||unit==u'\n'||unit==u'\t'||unit==u',';
}

// Core.Parse(TCHAR const*,TCHAR const*,TCHAR*,INT) (101028D8 -> 10146BC0),
// on LoadMap's GAME= literal. Core.appStrfind bars a preceding ASCII alnum,
// rather than requiring an option prefix or a dash. No-match must not clear
// the caller's destination; an engaged empty view represents a real write.
template<class Step>
std::optional<std::u16string_view> ParseGame(std::u16string_view option,Step& step) {
    constexpr std::u16string_view token=u"game=";
    bool previousAlnum{};
    for(std::size_t at=0u;at<option.size();++at) {
        step(1u);
        const auto unit=option[at];
        if(!previousAlnum&&UnrealString::FoldAscii(unit)==token.front()&&option.size()-at>=token.size()) {
            bool matches=true;
            for(std::size_t i=1u;i<token.size();++i) {
                step(1u);
                if(UnrealString::FoldAscii(option[at+i])!=token[i]){matches=false;break;}
            }
            if(matches) {
                auto payload=option.substr(at+token.size());
                const bool quoted=!payload.empty()&&payload.front()==u'"';
                if(quoted)payload.remove_prefix(1u);
                std::size_t count{};
                const auto limit=std::min(payload.size(),OriginalLoadMapGameClassMaxUnits);
                for(;count<limit;++count) {
                    step(1u);
                    if(quoted?payload[count]==u'"':IsUnquotedParseStop(payload[count]))break;
                }
                return payload.substr(0u,count);
            }
        }
        previousAlnum=IsAsciiAlnum(unit);
    }
    return std::nullopt;
}
}

// LoadMap 1038D3AD..1038D4B3 uses one 1024-wide zeroed InitGame buffer
// and one 256-wide GAME= destination. Append '?' + each FURL.Op in order
// (including an empty option); each match replaces the previous GAME= value.
// The borrowed FURL is immutable. Validate and parse it without allocations,
// then admit the derived object's additional byte peak before allocating.
template<class Step,class Admit>
OriginalLoadMapOptions DeriveOriginalLoadMapOptions(std::span<const std::u16string> options,
    Step&& step,Admit&& admit) {
    SaveUrlConfigDetail::ValidateOptions(options,step);
    std::size_t joinedUnits{};
    for(const auto& option:options) {
        step(1u);
        const auto units=option.size()+1u;
        if(units>OriginalLoadMapInitGameMaxUnits-joinedUnits)
            throw std::runtime_error("LoadMap InitGame options exceed its native wide-buffer bound");
        joinedUnits+=units;
    }

    std::u16string_view gameClass;
    for(const std::u16string_view option:options) {
        step(1u);
        if(const auto parsed=SaveUrlConfigDetail::ParseGame(option,step))gameClass=*parsed;
    }
    const auto payloadBytes=[](std::size_t initUnits,std::size_t classUnits) {
        return sizeof(OriginalLoadMapOptions)+sizeof(char16_t)*(initUnits+classUnits+2u);
    };
    step(joinedUnits+gameClass.size());
    admit(payloadBytes(joinedUnits,gameClass.size()));
    OriginalLoadMapOptions result;
    result.initGame.reserve(joinedUnits);
    result.gameClass.reserve(gameClass.size());
    admit(payloadBytes(result.initGame.capacity(),result.gameClass.capacity()));
    for(const auto& option:options) {
        result.initGame.push_back(u'?');
        result.initGame.append(option);
    }
    if(!gameClass.empty())result.gameClass.assign(gameClass);
    return result;
}

// LoadMap 1038E329..1038E3C5 invokes FURL.SaveURLConfig 103F9CB0 once
// per key for [DefaultPlayer] in User, in the order below. Each call
// visits every matching Op, not just the first or last. Borrowed
// views point into the actual FURL option backing storage; callbacks
// must not mutate that storage. The caller authenticates GIsClient and
// config/cache ownership and handles its root transaction and mutation.
// This traversal does not know about config files, dirty state or Flush.
template<class Step,class Visit>
std::size_t VisitOriginalLoadMapUserOptions(std::span<const std::u16string> options,
    Step&& step,Visit&& visit) {
    SaveUrlConfigDetail::ValidateOptions(options,step);
    constexpr std::array<std::u16string_view,7u> keys{
        u"Name",u"Team",u"Class",u"Skin",u"Face",u"Voice",u"OverrideClass"};
    std::size_t matches{};
    for(const auto requested:keys) {
        step(1u);
        for(const std::u16string_view option:options) {
            step(option.size()+1u);
            const auto equals=option.find(u'=');
            if(equals==std::u16string_view::npos)continue;
            const auto key=option.substr(0u,equals);
            step(2u*(key.size()+requested.size())+1u);
            if(!UnrealString::EqualInsensitive(key,requested))continue;
            visit(key,option.substr(equals+1u));
            ++matches;
        }
    }
    return matches;
}

} // namespace QuestVr
