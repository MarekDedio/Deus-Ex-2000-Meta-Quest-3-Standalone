#pragma once

#include "quest_unreal_string.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace QuestVr::NativeConfig {

// Explicit portable binding of the shipped Editor FConfigFile/GetString
// profile. This does NOT prove the packed game launcher's GConfig factory, nor
// reuse the deliberately narrower Root/GameEngine selector parser. The initial
// matching profile is the original CRT default C locale (ASCII fold, no Unicode
// expansion). Text transport is the shared UTF8/WTF8 FString transport. Resource
// limits below are portable policy, not retail maxima.
struct Limits {
    std::size_t fileBytes{1u<<20u},lineUnits{16u<<10u},sections{1024u},entries{16384u};
    std::size_t retainedBytes{8u<<20u},comparisonBytes{8u<<20u};
};
namespace Detail {
[[noreturn]] inline void Fail(const char* why) {throw std::runtime_error(std::string("Native config: ")+why);}
inline void ValidateLimits(const Limits& v) {
    const Limits maximum;
    if(!v.fileBytes||v.fileBytes>maximum.fileBytes||!v.lineUnits||v.lineUnits>maximum.lineUnits||
        !v.sections||v.sections>maximum.sections||!v.entries||v.entries>maximum.entries||
        !v.retainedBytes||v.retainedBytes>maximum.retainedBytes||!v.comparisonBytes||v.comparisonBytes>maximum.comparisonBytes)
        Fail("callers may only tighten positive resource limits");
}
inline unsigned char Fold(unsigned char c) {return c>='A'&&c<='Z'?static_cast<unsigned char>(c+('a'-'A')):c;}
inline bool Equal(std::string_view a,std::string_view b,std::size_t& work,const Limits& limits) {
    if(work==limits.comparisonBytes)Fail("comparison work limit exceeded");
    ++work;if(a.size()!=b.size())return false;
    for(std::size_t i=0u;i<a.size();++i) {
        if(work==limits.comparisonBytes)Fail("comparison work limit exceeded");
        ++work;if(Fold(static_cast<unsigned char>(a[i]))!=Fold(static_cast<unsigned char>(b[i])))return false;
    }
    return true;
}
inline std::size_t Add(std::size_t total,std::size_t count,std::size_t size,const Limits& limits) {
    if(total>limits.retainedBytes||!size||count>limits.retainedBytes/size||count*size>limits.retainedBytes-total)
        Fail("retained byte limit exceeded");
    return total+count*size;
}
inline std::u16string FileText(std::string_view bytes) {
    std::u16string result;result.reserve(bytes.size());
    const auto first=bytes.size()>=2u?static_cast<unsigned char>(bytes[0u]):0u;
    const auto second=bytes.size()>=2u?static_cast<unsigned char>(bytes[1u]):0u;
    if((bytes.size()%2u)==0u&&((first==255u&&second==254u)||(first==254u&&second==255u))) {
        for(std::size_t i=2u;i<bytes.size();i+=2u) {
            const auto a=static_cast<unsigned char>(bytes[i]),b=static_cast<unsigned char>(bytes[i+1u]);
            result.push_back(static_cast<char16_t>(first==255u?a|(b<<8u):(a<<8u)|b));
        }
    }else for(unsigned char c:bytes)result.push_back(static_cast<char16_t>(c));
    // appLoadFileToString decodes the complete file; FConfigFile.Read stops at
    // the first resulting NUL. This is not UTF8 or a strict BOM decoder.
    result.resize(UnrealString::CString(result).size());return result;
}
inline std::string Name(std::string_view transport,const Limits& limits) {
    if(transport.size()>limits.lineUnits*3u)Fail("lookup name unit/transport limit exceeded");
    const auto units=UnrealString::Decode(transport);const auto cstring=UnrealString::CString(units);
    if(cstring.size()>limits.lineUnits)Fail("lookup name unit/transport limit exceeded");
    return UnrealString::Encode(cstring);
}
}

class OrderedIni {
public:
    // No native pointers, mutable public records or cached query validation.
    // Repeated sections merge; repeated keys remain in source order. No trim,
    // comment stripping, array-prefix/index syntax, include or escape expansion.
    static OrderedIni Parse(std::string_view bytes,const Limits& limits={}) {
        Detail::ValidateLimits(limits);
        if(bytes.size()>limits.fileBytes)Detail::Fail("source file byte limit exceeded");
        const auto text=Detail::FileText(bytes);const std::u16string_view units=text;
        OrderedIni result;result.limits_=limits;std::optional<std::size_t> current;
        std::size_t position{},work{},stringBytes{};
        const auto retained=[&] {
            auto n=Detail::Add(sizeof(result),result.sections_.capacity(),sizeof(std::string),limits);
            n=Detail::Add(n,result.entries_.capacity(),sizeof(Entry),limits);
            static_cast<void>(Detail::Add(n,stringBytes,1u,limits));
        };
        retained();
        while(position<units.size()) {
            const auto end=units.find_first_of(u"\r\n",position);
            const auto count=(end==std::u16string_view::npos?units.size():end)-position;
            if(count>limits.lineUnits)Detail::Fail("source line unit limit exceeded");
            const auto line=units.substr(position,count);position+=count;
            while(position<units.size()&&(units[position]==u'\r'||units[position]==u'\n'))++position;
            if(line.size()>=2u&&line.front()==u'['&&line.back()==u']') {
                const auto name=UnrealString::Encode(line.substr(1u,line.size()-2u));current=result.Section(name,work);
                if(!current) {
                    if(result.sections_.size()==limits.sections)Detail::Fail("section count limit exceeded");
                    result.sections_.push_back(name);current=result.sections_.size()-1u;
                    stringBytes=Detail::Add(stringBytes,result.sections_.back().capacity()+1u,1u,limits);
                }
            }else if(current) {
                const auto equals=line.find(u'=');if(equals==std::u16string_view::npos)continue;
                auto value=line.substr(equals+1u);
                // Even a lone quote becomes empty: retail advances to its
                // existing terminator and zeros the preceding quote character.
                if(!value.empty()&&value.front()==u'"'&&value.back()==u'"')
                    value=value.size()==1u?std::u16string_view{}:value.substr(1u,value.size()-2u);
                if(result.entries_.size()==limits.entries)Detail::Fail("entry count limit exceeded");
                result.entries_.push_back({*current,UnrealString::Encode(line.substr(0u,equals)),UnrealString::Encode(value)});
                for(const auto* text:{&result.entries_.back().key,&result.entries_.back().value})
                    stringBytes=Detail::Add(stringBytes,text->capacity()+1u,1u,limits);
            }
            retained();
        }
        return result;
    }
    // Borrowed view, valid only while this immutable document stays alive and
    // is not moved/replaced. Reject direct calls on temporary documents.
    std::optional<std::string_view> Scalar(std::string_view section,std::string_view key) const & {
        const auto sectionName=Detail::Name(section,limits_),keyName=Detail::Name(key,limits_);
        std::size_t work{};const auto selected=Section(sectionName,work);
        if(!selected)return std::nullopt;
        // Retail hash chains prepend duplicate pairs. GetString returns the
        // newest match, unlike the pinned IniFile's index-zero first match.
        for(auto i=entries_.rbegin();i!=entries_.rend();++i)
            if(i->section==*selected&&Detail::Equal(i->key,keyName,work,limits_))return i->value;
        return std::nullopt;
    }
    std::optional<std::string_view> Scalar(std::string_view,std::string_view) const &&=delete;
    std::optional<std::vector<std::string>> StringArray(std::string_view section,std::string_view key) const {
        const auto sectionName=Detail::Name(section,limits_),keyName=Detail::Name(key,limits_);
        std::size_t work{};const auto selected=Section(sectionName,work);
        if(!selected)return std::nullopt;
        // Core LoadConfig gathers newest-first matches, then reverses while
        // importing StrProperty(flags0): the whole unquoted string is kept.
        // Existing section/missing key => empty; absent section => no update.
        std::vector<std::string> result;std::size_t retained{};
        for(const auto& entry:entries_)if(entry.section==*selected&&Detail::Equal(entry.key,keyName,work,limits_)) {
            result.push_back(entry.value);
            retained=Detail::Add(retained,result.back().capacity()+1u,1u,limits_);
            static_cast<void>(Detail::Add(retained,result.capacity(),sizeof(std::string),limits_));
        }
        return result;
    }
    std::optional<std::string> PropertyScalar(std::string_view section,std::string_view key) const {
        const auto value=Scalar(section,key);if(!value)return std::nullopt;
        auto units=UnrealString::Decode(*value);
        // LoadConfig supplies GetString a1024-unit destination, forcibly NUL
        // terminated by appStrncpy. Preserve a split surrogate, not a codepoint.
        if(units.size()>1023u)units.resize(1023u);
        return UnrealString::Encode(units);
    }
    std::size_t SectionCount() const noexcept {return sections_.size();}
    std::size_t EntryCount() const noexcept {return entries_.size();}
    std::size_t RetainedBytes() const {
        auto bytes=Detail::Add(sizeof(*this),sections_.capacity(),sizeof(std::string),limits_);
        bytes=Detail::Add(bytes,entries_.capacity(),sizeof(Entry),limits_);
        for(const auto& name:sections_)bytes=Detail::Add(bytes,name.capacity()+1u,1u,limits_);
        for(const auto& entry:entries_)for(const auto* value:{&entry.key,&entry.value})bytes=Detail::Add(bytes,value->capacity()+1u,1u,limits_);
        return bytes;
    }
private:
    struct Entry {std::size_t section{};std::string key,value;};
    std::optional<std::size_t> Section(std::string_view name,std::size_t& work) const {
        for(std::size_t i=0u;i<sections_.size();++i)if(Detail::Equal(sections_[i],name,work,limits_))return i;
        return std::nullopt;
    }
    Limits limits_;
    std::vector<std::string> sections_;
    std::vector<Entry> entries_;
};

// Explicit supplied-file binding; no System.ini/User.ini alias, inheritance,
// command-line path selection, fallback defaults, cache mutation or writeback.
inline std::optional<OrderedIni> ReadOrderedIni(const std::filesystem::path& path,const Limits& limits={}) {
    Detail::ValidateLimits(limits);std::error_code error;const auto status=std::filesystem::status(path,error);
    if(error==std::errc::no_such_file_or_directory||(!error&&status.type()==std::filesystem::file_type::not_found))return std::nullopt;
    if(error||!std::filesystem::is_regular_file(status))Detail::Fail("source is not an accessible regular file");
    const auto size=std::filesystem::file_size(path,error);if(error||size>limits.fileBytes)Detail::Fail("source file byte limit exceeded/unreadable");
    std::ifstream input(path,std::ios::binary);if(!input)Detail::Fail("cannot open source file");
    std::string bytes(static_cast<std::size_t>(size),'\0');
    if(!bytes.empty()) {input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));if(!input||input.gcount()!=static_cast<std::streamsize>(bytes.size()))Detail::Fail("source file changed/truncated while reading");}
    if(input.peek()!=std::char_traits<char>::eof()||input.bad())Detail::Fail("source file grew/unreadable while reading");
    return OrderedIni::Parse(bytes,limits);
}

} // namespace QuestVr::NativeConfig
