#pragma once

#include "quest_unreal_string.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
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
// Each operation owns no host state. The supplied callbacks join an enclosing
// native transaction's work/scratch budget; a throw leaves its source document
// unchanged. admit receives an ABSOLUTE additional peak, excluding the already
// retained source document, and runs before the corresponding allocations.
struct Work {
    std::function<void(std::size_t)> step;
    std::function<void(std::size_t)> admit;
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
inline std::size_t Sum(std::size_t a,std::size_t b) {
    if(b>std::numeric_limits<std::size_t>::max()-a)Fail("operation byte count overflow");
    return a+b;
}
inline std::size_t Product(std::size_t count,std::size_t size) {
    if(size&&count>std::numeric_limits<std::size_t>::max()/size)Fail("operation byte count overflow");
    return count*size;
}
struct Operation {
    const Limits& limits;const Work& callbacks;std::size_t work{},peak{};
    void Step(std::size_t amount=1u) {
        if(!amount)return;
        if(amount>limits.comparisonBytes-work)Fail("operation work limit exceeded");
        work+=amount;if(callbacks.step)callbacks.step(amount);
    }
    void Admit(std::size_t bytes) {
        if(bytes<=peak)return;
        if(callbacks.admit)callbacks.admit(bytes);
        peak=bytes;
    }
    bool Equal(std::string_view a,std::string_view b) {
        Step();if(a.size()!=b.size())return false;
        for(std::size_t i=0u;i<a.size();++i) {Step();if(Fold(static_cast<unsigned char>(a[i]))!=Fold(static_cast<unsigned char>(b[i])))return false;}
        return true;
    }
};
// Allocation-free UTF8/WTF8 traversal. It deliberately accepts isolated UTF16
// surrogate units, exactly like the shared FString transport decoder.
template<class Visitor>
void Units(std::string_view text,Operation& operation,Visitor&& visitor) {
    for(std::size_t i=0u;i<text.size();) {
        operation.Step();const auto first=static_cast<std::uint8_t>(text[i++]);
        std::uint32_t c=first,minimum{};unsigned count{};
        if(first>=0xc2u&&first<=0xdfu) {c=first&31u;count=1u;minimum=0x80u;}
        else if(first>=0xe0u&&first<=0xefu) {c=first&15u;count=2u;minimum=0x800u;}
        else if(first>=0xf0u&&first<=0xf4u) {c=first&7u;count=3u;minimum=0x10000u;}
        else if(first>=0x80u)Fail("VM String is not UTF8/WTF8 transport");
        if(count>text.size()-i)Fail("truncated VM String transport");
        for(unsigned part=0u;part<count;++part) {
            operation.Step();const auto next=static_cast<std::uint8_t>(text[i++]);
            if((next&0xc0u)!=0x80u)Fail("invalid VM String continuation");
            c=(c<<6u)|(next&63u);
        }
        if(c<minimum||c>0x10ffffu)Fail("noncanonical VM String transport");
        if(c<=0xffffu)visitor(static_cast<char16_t>(c));
        else {c-=0x10000u;visitor(static_cast<char16_t>(0xd800u+(c>>10u)));visitor(static_cast<char16_t>(0xdc00u+(c&1023u)));}
    }
}
inline std::string MutationText(std::string_view transport,Operation& operation,std::size_t retained) {
    if(transport.size()>operation.limits.lineUnits*3u)Fail("mutation text unit/transport limit exceeded");
    std::size_t units{};bool ended{};
    Units(transport,operation,[&](char16_t c) {
        if(c==u'\0')ended=true;
        if(!ended&&++units>operation.limits.lineUnits)Fail("mutation text unit/transport limit exceeded");
    });
    // Decode and Encode may overlap and grow geometrically. This bounded
    // transport reservation includes their headers, SSO and growth capacity.
    const auto scratch=Sum(2u*sizeof(std::string)+sizeof(std::u16string),Product(transport.size()+1u,8u));
    operation.Admit(Sum(retained,scratch));
    const auto decoded=UnrealString::Decode(transport);
    auto result=UnrealString::Encode(UnrealString::CString(decoded));
    if(Sum(Product(decoded.capacity()+1u,sizeof(char16_t)),result.capacity()+1u)>scratch)
        Fail("mutation text library exceeded admitted capacity");
    return result;
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
    // Explicit Editor SetString1021bf90 profile: update the NEWEST duplicate,
    // ignore case-only value changes, preserve all earlier pairs/key spelling.
    // nullopt means no dirty document; otherwise this immutable source remains
    // alive and the returned independently owned replacement may be staged.
    std::optional<OrderedIni> WithString(std::string_view section,std::string_view key,
        std::string_view value,const Work& callbacks={}) const {
        Detail::ValidateLimits(limits_);Detail::Operation operation{limits_,callbacks};
        auto retained=3u*sizeof(std::string)+sizeof(Detail::Operation)+2u*sizeof(std::optional<std::size_t>);
        auto sectionName=Detail::MutationText(section,operation,retained);
        retained=Detail::Sum(retained,sectionName.capacity()+1u);
        auto keyName=Detail::MutationText(key,operation,retained);
        retained=Detail::Sum(retained,keyName.capacity()+1u);
        auto text=Detail::MutationText(value,operation,retained);
        retained=Detail::Sum(retained,text.capacity()+1u);
        std::optional<std::size_t> selected,match;
        for(std::size_t i=0u;i<sections_.size();++i)if(operation.Equal(sections_[i],sectionName)){selected=i;break;}
        if(selected)for(std::size_t i=entries_.size();i>0u;--i) {
            operation.Step();const auto& entry=entries_[i-1u];
            if(entry.section==*selected&&operation.Equal(entry.key,keyName)) {match=i-1u;break;}
        }
        if(match&&operation.Equal(entries_[*match].value,text))return std::nullopt;
        if(!selected&&sections_.size()==limits_.sections)Detail::Fail("section count limit exceeded");
        if(!match&&entries_.size()==limits_.entries)Detail::Fail("entry count limit exceeded");
        const auto sourceBytes=RetainedBytes();operation.Admit(Detail::Sum(retained,sourceBytes));
        operation.Step(sections_.size()+entries_.size());
        for(const auto& name:sections_)operation.Step(name.size()+1u);
        for(const auto& entry:entries_)operation.Step(entry.key.size()+entry.value.size()+2u);
        OrderedIni result=*this;
        // The supported standard libraries copy vector/string capacity no
        // larger than the actual source capacity charged above.
        if(result.RetainedBytes()>sourceBytes)Detail::Fail("document copy exceeded admitted capacity");
        if(match)result.entries_[*match].value.swap(text);
        else {
            if(!selected) {
                const auto capacity=result.sections_.size()+1u;
                if(capacity>result.sections_.capacity()) {
                    operation.Admit(Detail::Sum(Detail::Sum(retained,result.RetainedBytes()),Detail::Product(capacity,sizeof(std::string))));
                    result.sections_.reserve(capacity);
                    if(result.sections_.capacity()!=capacity)Detail::Fail("section growth exceeded admitted capacity");
                }
                result.sections_.push_back(std::move(sectionName));selected=result.sections_.size()-1u;
            }
            const auto capacity=result.entries_.size()+1u;
            if(capacity>result.entries_.capacity()) {
                operation.Admit(Detail::Sum(Detail::Sum(retained,result.RetainedBytes()),Detail::Product(capacity,sizeof(Entry))));
                result.entries_.reserve(capacity);
                if(result.entries_.capacity()!=capacity)Detail::Fail("entry growth exceeded admitted capacity");
            }
            result.entries_.push_back({*selected,std::move(keyName),std::move(text)});
        }
        static_cast<void>(result.RetainedBytes());
        return std::optional<OrderedIni>(std::in_place,std::move(result));
    }
    // Raw FConfigFile.Write1021a770 record shape, deliberately without quoting
    // or escaping. Sections group entries stably; earlier duplicates survive.
    // The readable portable transport is zero-extended bytes when every unit
    // fits8bit, otherwise BOM UTF16LE. Never emit UTF8 into this byte reader.
    // Literal outer quotes/newlines retain the ORIGINAL lossy reload behavior.
    std::string Serialize(const Work& callbacks={}) const {
        Detail::ValidateLimits(limits_);Detail::Operation operation{limits_,callbacks};
        const auto planBytes=Detail::Sum(2u*sizeof(std::vector<std::size_t>),
            Detail::Product(Detail::Sum(sections_.size()+1u,entries_.size()),sizeof(std::size_t)));
        operation.Admit(planBytes);
        std::vector<std::size_t> stops,order;stops.reserve(sections_.size()+1u);order.reserve(entries_.size());
        if(stops.capacity()!=sections_.size()+1u||order.capacity()!=entries_.size())Detail::Fail("serialization plan exceeded admitted capacity");
        stops.resize(sections_.size()+1u);order.resize(entries_.size());
        for(const auto& entry:entries_) {operation.Step();if(entry.section>=sections_.size())Detail::Fail("entry section identity is invalid");++stops[entry.section+1u];}
        for(std::size_t i=1u;i<stops.size();++i){operation.Step();stops[i]+=stops[i-1u];}
        for(std::size_t i=0u;i<entries_.size();++i){operation.Step();order[stops[entries_[i].section]++]=i;}
        const auto records=[&](auto&& emit) {
            const auto literal=[&](std::u16string_view text){for(char16_t c:text){operation.Step();emit(c);}};
            for(std::size_t s=0u;s<sections_.size();++s) {
                operation.Step();literal(u"[");Detail::Units(sections_[s],operation,emit);literal(u"]\r\n");
                for(std::size_t i=s?stops[s-1u]:0u;i<stops[s];++i) {
                    operation.Step();const auto& entry=entries_[order[i]];
                    Detail::Units(entry.key,operation,emit);literal(u"=");Detail::Units(entry.value,operation,emit);literal(u"\r\n");
                }
                literal(u"\r\n");
            }
        };
        std::size_t units{},line{};bool wide{};
        records([&](char16_t c) {
            if(units==limits_.fileBytes)Detail::Fail("serialized file byte limit exceeded");
            ++units;
            if(c==u'\r'||c==u'\n')line=0u;
            else if(++line>limits_.lineUnits)Detail::Fail("serialized line unit limit exceeded");
            wide=wide||c>255u;
        });
        if(wide&&(limits_.fileBytes<2u||units>(limits_.fileBytes-2u)/2u))Detail::Fail("serialized file byte limit exceeded");
        const auto bytes=wide?2u+2u*units:units;
        const auto outputBound=Detail::Sum(sizeof(std::string),Detail::Product(bytes+1u,2u));
        operation.Admit(Detail::Sum(planBytes,outputBound));std::string result;result.reserve(bytes);
        if(result.capacity()+1u>outputBound)Detail::Fail("serialized output exceeded admitted capacity");
        if(wide){result.push_back(static_cast<char>(255u));result.push_back(static_cast<char>(254u));}
        records([&](char16_t c) {
            result.push_back(static_cast<char>(c&255u));if(wide)result.push_back(static_cast<char>((c>>8u)&255u));
        });
        if(result.size()!=bytes)Detail::Fail("serialized output size changed between passes");
        return result;
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
