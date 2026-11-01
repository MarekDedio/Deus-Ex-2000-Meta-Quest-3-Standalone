#pragma once

#include "quest_command_line_param.h"
#include "quest_core_set_command.h"
#include "quest_native_scalar.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace QuestVr {
struct OriginalInputAxisCell {
    std::uintptr_t actor{},property{};
    bool operator==(const OriginalInputAxisCell&) const = default;
};
struct OriginalInputAxisQueries {
    std::function<void()> requireOuterRoot,work;
    std::function<void(std::size_t)> enterScratch,leaveScratch;
    std::function<std::uintptr_t()> viewportActor;
    std::function<OriginalInputAxisCell(std::uintptr_t,std::u16string_view)> findAxis;
    std::function<std::int32_t()> action;
    std::function<float()> delta;
    std::function<float(OriginalInputAxisCell)> readFloat;
    std::function<void(OriginalInputAxisCell,float)> writeFloat;
    // Actual supplied output-device Logf(CString), with no format arguments.
    std::function<void(std::u16string_view)> logf;
};

namespace InputAxisCommandDetail {
inline constexpr std::size_t FrameBytes=256u,AnsiCapacity=1024u;
// appToAnsi admits at most1023 decimal characters. 176 limbs cover the largest
// exact mantissa/denominator plus the binary64 rounding shifts for that input.
// No process locale, host extended register or platform strtod is consulted.
inline constexpr std::size_t LimbCount=176u;
struct Big {
    std::array<std::uint32_t,LimbCount> words{};
    std::size_t used{};
};
struct Context {
    const OriginalInputAxisQueries& q;
    std::size_t reserved{};
    void Step() {
        if(!q.work)throw std::invalid_argument("Original Input AXIS requires borrowed root work");
        q.work();
    }
    template<class Function,class... Arguments>
    auto Call(const Function& function,Arguments&&... arguments) {
        if(!function)throw std::invalid_argument("Original Input AXIS lacks a reached typed provider");
        Step();return function(std::forward<Arguments>(arguments)...);
    }
    void Admit(std::size_t peak) {
        if(peak<=reserved)return;
        Call(q.enterScratch,peak-reserved);reserved=peak;
    }
    ~Context()noexcept {if(reserved)q.leaveScratch(reserved);}
};
inline void Normalize(Big& value,Context& c) {
    while(value.used) {c.Step();if(value.words[value.used-1u])break;--value.used;}
}
inline unsigned Bits(const Big& value,Context& c) {
    c.Step();return value.used?static_cast<unsigned>((value.used-1u)*32u+
        (32u-std::countl_zero(value.words[value.used-1u]))):0u;
}
inline void DecimalDigit(Big& value,unsigned digit,Context& c) {
    std::uint64_t carry=digit;
    for(std::size_t i=0u;i<value.used;++i) {
        c.Step();const auto next=std::uint64_t{value.words[i]}*10u+carry;
        value.words[i]=static_cast<std::uint32_t>(next);carry=next>>32u;
    }
    if(carry) {
        c.Step();if(value.used==LimbCount)throw std::runtime_error("Original Input AXIS decimal workspace exceeded");
        value.words[value.used++]=static_cast<std::uint32_t>(carry);
    }
}
inline Big Shift(const Big& value,unsigned amount,Context& c) {
    Big out;if(!value.used)return out;
    const auto whole=static_cast<std::size_t>(amount/32u);const unsigned part=amount%32u;
    if(whole>=LimbCount||value.used>LimbCount-whole)
        throw std::runtime_error("Original Input AXIS binary workspace exceeded");
    out.used=value.used+whole;
    for(std::size_t i=0u;i<value.used;++i) {
        c.Step();const auto product=std::uint64_t{value.words[i]}<<part;
        out.words[i+whole]|=static_cast<std::uint32_t>(product);
        if(product>>32u) {
            if(i+whole+1u>=LimbCount)throw std::runtime_error("Original Input AXIS binary workspace exceeded");
            out.words[i+whole+1u]|=static_cast<std::uint32_t>(product>>32u);
            out.used=std::max(out.used,i+whole+2u);
        }
    }
    return out;
}
inline int Compare(const Big& a,const Big& b,Context& c) {
    c.Step();if(a.used!=b.used)return a.used<b.used?-1:1;
    for(std::size_t i=a.used;i--;) {c.Step();if(a.words[i]!=b.words[i])return a.words[i]<b.words[i]?-1:1;}
    return 0;
}
inline void Subtract(Big& a,const Big& b,Context& c) {
    std::uint64_t borrow{};
    for(std::size_t i=0u;i<a.used;++i) {
        c.Step();const auto sub=(i<b.used?std::uint64_t{b.words[i]}:0u)+borrow;
        const auto before=std::uint64_t{a.words[i]};a.words[i]=static_cast<std::uint32_t>(before-sub);
        borrow=before<sub;
    }
    if(borrow)throw std::runtime_error("Original Input AXIS exact remainder became negative");
    Normalize(a,c);
}
inline double RoundedDouble(Big numerator,Big denominator,bool negative,Context& c) {
    // Rational operands are positive; sign is installed after one exact
    // nearest-even binary64 rounding. Their nonzero/range proof is upstream.
    int top=static_cast<int>(Bits(numerator,c))-static_cast<int>(Bits(denominator,c));
    if(top>=0) {if(Compare(numerator,Shift(denominator,static_cast<unsigned>(top),c),c)<0)--top;}
    else if(Compare(Shift(numerator,static_cast<unsigned>(-top),c),denominator,c)<0)--top;
    const int quantum=std::max(top-52,-1074);
    if(quantum>=0)denominator=Shift(denominator,static_cast<unsigned>(quantum),c);
    else numerator=Shift(numerator,static_cast<unsigned>(-quantum),c);
    std::uint64_t quotient{};
    const int difference=static_cast<int>(Bits(numerator,c))-static_cast<int>(Bits(denominator,c));
    if(difference>53)throw std::runtime_error("Original Input AXIS exact quotient exceeds binary64 profile");
    for(int bit=difference;bit>=0;--bit) {
        const auto selected=Shift(denominator,static_cast<unsigned>(bit),c);
        if(Compare(numerator,selected,c)>=0) {Subtract(numerator,selected,c);quotient|=std::uint64_t{1u}<<bit;}
    }
    const auto rounding=Compare(Shift(numerator,1u,c),denominator,c);
    if(rounding>0||(rounding==0&&(quotient&1u)))++quotient;
    int storedQuantum=quantum;
    if(quotient>=0x0020000000000000ull) {quotient>>=1u;++storedQuantum;}
    const auto sign=negative?0x8000000000000000ull:0ull;
    if(storedQuantum>971)return std::bit_cast<double>(sign|0x7ff0000000000000ull);
    if(quotient<0x0010000000000000ull&&storedQuantum==-1074)
        return std::bit_cast<double>(sign|quotient);
    if(!quotient)return std::bit_cast<double>(sign);
    return std::bit_cast<double>(sign|(static_cast<std::uint64_t>(storedQuantum+1075)<<52u)|
        (quotient&0x000fffffffffffffull));
}
inline bool Space(unsigned char ch) {return ch==' '||ch=='\t'||ch=='\r'||ch=='\n'||ch=='\v'||ch=='\f';}
inline bool Digit(unsigned char ch) {return ch>='0'&&ch<='9';}
inline char Upper(char ch) {return ch>='a'&&ch<='z'?static_cast<char>(ch-('a'-'A')):ch;}
inline bool Begins(const std::array<char,AnsiCapacity>& text,std::size_t at,std::string_view needle,Context& c) {
    if(needle.size()>AnsiCapacity-at)return false;
    for(std::size_t i=0u;i<needle.size();++i) {c.Step();if(Upper(text[at+i])!=needle[i])return false;}
    return true;
}
inline float Atof(std::u16string_view text,Context& c) {
    // Keep the property-name token admitted simultaneously with this bounded
    // ANSI buffer and all exact-rational workspaces (including value copies).
    c.Admit(CoreSetCommand::Detail::Add(c.reserved,AnsiCapacity+12u*sizeof(Big)+256u));
    std::array<char,AnsiCapacity> ansi{};std::size_t length{};
    for(;length<AnsiCapacity-1u;++length) {
        c.Step();const auto unit=length<text.size()?text[length]:char16_t{};
        if(!unit)break;
        ansi[length]=unit<256u?static_cast<char>(static_cast<unsigned char>(unit)):char{0x7f};
    }
    std::size_t cursor{};
    while(cursor<length) {c.Step();if(!Space(static_cast<unsigned char>(ansi[cursor])))break;++cursor;}
    const bool negative=cursor<length&&ansi[cursor]=='-';
    if(cursor<length&&(negative||ansi[cursor]=='+')) {c.Step();++cursor;}
    if(Begins(ansi,cursor,"INF",c)||Begins(ansi,cursor,"NAN",c)||Begins(ansi,cursor,"0X",c)||
        Begins(ansi,cursor,"1.#",c))
        throw std::runtime_error("Original Input AXIS textual special/hex CRT numeric profile is unestablished");
    const auto first=cursor;bool dotted{},digits{},nonzero{};std::size_t significant{},fractional{};
    for(;cursor<length;++cursor) {
        c.Step();const auto ch=static_cast<unsigned char>(ansi[cursor]);
        if(Digit(ch)) {digits=true;if(dotted)++fractional;if(ch!='0')nonzero=true;if(nonzero)++significant;}
        else if(ch=='.'&&!dotted)dotted=true;
        else break;
    }
    const auto mantissaEnd=cursor;
    if(!digits)return 0.0f; // No conversion: not a fabricated negative zero.
    std::int64_t exponent{};
    if(cursor<length&&(ansi[cursor]=='e'||ansi[cursor]=='E')) {
        c.Step();std::size_t at=cursor+1u;const bool minus=at<length&&ansi[at]=='-';
        if(at<length&&(minus||ansi[at]=='+')) {c.Step();++at;}
        const auto exponentStart=at;
        for(;at<length;++at) {
            c.Step();if(!Digit(static_cast<unsigned char>(ansi[at])))break;
            exponent=std::min<std::int64_t>(1000000,exponent*10+(ansi[at]-'0'));
        }
        if(at==exponentStart)exponent=0; // "1e+tail" converts the prefix1.
        else if(minus)exponent=-exponent;
    }
    const auto sign=negative?0x80000000u:0u;
    if(!nonzero)return std::bit_cast<float>(sign);
    const auto decimalExponent=exponent-static_cast<std::int64_t>(fractional);
    const auto order=decimalExponent+static_cast<std::int64_t>(significant)-1;
    // Exact binary64 overflow/underflow classification, not Float clamping.
    if(order>308)return std::bit_cast<float>(sign|0x7f800000u);
    if(order< -324)return std::bit_cast<float>(sign);
    Big numerator,denominator;denominator.words[0]=1u;denominator.used=1u;
    for(auto at=first;at<mantissaEnd;++at) {
        c.Step();if(Digit(static_cast<unsigned char>(ansi[at])))DecimalDigit(numerator,static_cast<unsigned>(ansi[at]-'0'),c);
    }
    if(decimalExponent>=0)for(std::int64_t i=0;i<decimalExponent;++i)DecimalDigit(numerator,0u,c);
    else for(std::int64_t i=0;i< -decimalExponent;++i)DecimalDigit(denominator,0u,c);
    return NativeScalar::StoreFloat(NativeScalar::Load(RoundedDouble(numerator,denominator,negative,c)));
}
inline float Speed(std::u16string_view input,std::size_t cursor,Context& c) {
    if(cursor>input.size())throw std::runtime_error("Original Input AXIS SPEED cursor exceeds its CString");
    const auto remaining=input.substr(cursor);
    const auto found=CommandLineParamDetail::Find(remaining,0u,u"SPEED=",c.q.work);
    return found?Atof(remaining.substr(*found+6u),c):1.0f;
}
} // namespace InputAxisCommandDetail

// AXIS has ALREADY matched in the original BUTTON/PULSE/TOGGLE/AXIS prefix.
// The cursor is ParseCommand's actual adjusted remainder, not a new tokenized
// command. This coordinator supplies no native Actor/property/output authority.
inline void ExecuteOriginalInputAxisCommand(std::u16string_view supplied,std::size_t cursor,
    const OriginalInputAxisQueries& q) {
    using namespace InputAxisCommandDetail;
    if(!q.leaveScratch)throw std::invalid_argument("Original Input AXIS requires lexical scratch cleanup");
    Context c{q};c.Call(q.requireOuterRoot);c.Admit(FrameBytes);
    const auto input=CoreSetCommand::Detail::Input(supplied,c);
    if(cursor>input.size())throw std::runtime_error("Original Input AXIS cursor exceeds its CString");
    const auto bad=[&]{c.Call(q.logf,std::u16string_view{u"Bad Axis command"});};
    if(!c.Call(q.viewportActor)) {bad();return;}
    const auto token=CoreSetCommand::ParseToken256(input,cursor,c,FrameBytes,false);
    if(!token.present) {bad();return;}
    const auto actor=c.Call(q.viewportActor);
    if(!actor)throw std::runtime_error("Original Input FindAxisName requires the actual post-token Actor");
    const auto cell=c.Call(q.findAxis,actor,std::u16string_view{token.value});
    if(!cell.actor&&!cell.property) {bad();return;}
    if(!cell.actor||!cell.property||cell.actor!=actor)
        throw std::runtime_error("Original Input FindAxisName returned a partial or foreign Actor address");
    const auto speed=Speed(input,token.nextUnit,c); // Always BEFORE Action test.
    const auto action=c.Call(q.action);
    if(action!=2&&action!=4)return;
    const auto delta=c.Call(q.delta);
    auto product=NativeScalar::Multiply(NativeScalar::Load(delta),NativeScalar::Memory(speed));
    if(action==4)product=NativeScalar::Multiply(product,NativeScalar::Memory(std::bit_cast<double>(0x3f847ae147ae147bull)));
    const auto current=c.Call(q.readFloat,cell);
    const auto next=NativeScalar::StoreFloat(NativeScalar::Add(product,NativeScalar::Memory(current)));
    c.Call(q.writeFloat,cell,next);
}
} // namespace QuestVr
