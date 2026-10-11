#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace QuestVr::UnrealString {

// VM text transport is UTF-8, extended with three-byte encodings of isolated
// surrogate units (WTF-8). Original FString indexing is UTF-16 CODE UNITS, not
// UTF-8 bytes or Unicode scalars. Slicing must preserve split surrogates.
inline std::string Encode(const std::u16string_view units) {
    std::string out;
    for (std::size_t i=0; i<units.size(); ++i) {
        std::uint32_t c=units[i];
        if (c>=0xd800u && c<=0xdbffu && i+1u<units.size() && units[i+1u]>=0xdc00u && units[i+1u]<=0xdfffu)
            c=0x10000u+((c-0xd800u)<<10u)+(units[++i]-0xdc00u);
        if (c<0x80u) out.push_back(static_cast<char>(c));
        else if (c<0x800u) {
            out.push_back(static_cast<char>(0xc0u|(c>>6u)));
            out.push_back(static_cast<char>(0x80u|(c&63u)));
        } else if (c<0x10000u) {
            out.push_back(static_cast<char>(0xe0u|(c>>12u)));
            out.push_back(static_cast<char>(0x80u|((c>>6u)&63u)));
            out.push_back(static_cast<char>(0x80u|(c&63u)));
        } else {
            out.push_back(static_cast<char>(0xf0u|(c>>18u)));
            out.push_back(static_cast<char>(0x80u|((c>>12u)&63u)));
            out.push_back(static_cast<char>(0x80u|((c>>6u)&63u)));
            out.push_back(static_cast<char>(0x80u|(c&63u)));
        }
    }
    return out;
}
inline std::u16string Decode(const std::string_view text) {
    std::u16string out;
    for (std::size_t i=0; i<text.size();) {
        const auto first=static_cast<std::uint8_t>(text[i++]);
        std::uint32_t c=first, minimum=0u;
        unsigned count=0u;
        if (first>=0xc2u && first<=0xdfu) { c=first&31u; count=1u; minimum=0x80u; }
        else if (first>=0xe0u && first<=0xefu) { c=first&15u; count=2u; minimum=0x800u; }
        else if (first>=0xf0u && first<=0xf4u) { c=first&7u; count=3u; minimum=0x10000u; }
        else if (first>=0x80u) throw std::runtime_error("VM String is not UTF-8/WTF-8 transport");
        if (count>text.size()-i) throw std::runtime_error("Truncated VM String transport");
        for (unsigned j=0; j<count; ++j) {
            const auto next=static_cast<std::uint8_t>(text[i++]);
            if ((next&0xc0u)!=0x80u) throw std::runtime_error("Invalid VM String continuation");
            c=(c<<6u)|(next&63u);
        }
        if (c<minimum || c>0x10ffffu) throw std::runtime_error("Noncanonical VM String transport");
        if (c<=0xffffu) out.push_back(static_cast<char16_t>(c));
        else {
            c-=0x10000u;
            out.push_back(static_cast<char16_t>(0xd800u+(c>>10u)));
            out.push_back(static_cast<char16_t>(0xdc00u+(c&1023u)));
        }
    }
    return out;
}
inline std::string FromAnsi(const std::string_view bytes) {
    std::u16string units;
    units.reserve(bytes.size());
    for (const unsigned char c:bytes) units.push_back(static_cast<char16_t>(c));
    return Encode(units); // Original appFromAnsi/Serialize zero-extend bytes.
}
inline std::u16string_view CString(const std::u16string_view units) {
    const auto end=units.find(u'\0');
    return units.substr(0,end);
}
inline char16_t FoldAscii(const char16_t c) { return c>=u'A' && c<=u'Z' ? c+(u'a'-u'A') : c; }
inline bool EqualInsensitive(const std::u16string_view a, const std::u16string_view b) {
    // Original default CRT C-locale _wcsicmp. No locale/Unicode case expansion.
    const auto x=CString(a), y=CString(b);
    if (x.size()!=y.size()) return false;
    for (std::size_t i=0; i<x.size(); ++i) if (FoldAscii(x[i])!=FoldAscii(y[i])) return false;
    return true;
}

} // namespace QuestVr::UnrealString
