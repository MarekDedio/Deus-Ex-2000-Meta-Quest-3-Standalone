#include "quest_native_text_window_setters.h"
#include "quest_native_text_content.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks{},refusals{};
void Require(bool condition,const char* message) {
    ++checks;if(!condition) throw std::runtime_error(message);
}
float Float(std::uint32_t bits) {return std::bit_cast<float>(bits);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
struct State {
    std::uint8_t horizontal{17u},vertical{23u};
    std::uint32_t hMargin{Bits(3.0f)},vMargin{Bits(3.0f)};
    std::int32_t minimum{-1},maximum{-1};
    unsigned revision{},reconfigures{};
    bool operator==(const State&) const=default;
};
// Explicit mock storage/layout provider. These tests prove the original setter
// protocol; they do not certify native UObject/VM rollback or rendered HUDs.
struct Operations {
    State state;
    std::vector<std::string> events;
    std::size_t work{},workLimit{std::numeric_limits<std::size_t>::max()};
    std::size_t writes{},writeLimit{std::numeric_limits<std::size_t>::max()};
    std::function<void(Operations&)> callback;
    void Step() {
        if(work>=workLimit) throw std::runtime_error("Mock root native work exhausted");
        ++work;
    }
    void Write() {
        if(writes>=writeLimit) throw std::runtime_error("Mock root write budget exhausted");
        ++writes;++state.revision;
    }
    void WriteHAlignment(std::uint8_t value) {
        Write();events.emplace_back("writeHAlign");state.horizontal=value;
    }
    void WriteVAlignment(std::uint8_t value) {
        Write();events.emplace_back("writeVAlign");state.vertical=value;
    }
    float ReadHMargin() {events.emplace_back("readHMargin");return Float(state.hMargin);}
    float ReadVMargin() {events.emplace_back("readVMargin");return Float(state.vMargin);}
    void WriteHMargin(float value) {Write();events.emplace_back("writeHMargin");state.hMargin=Bits(value);}
    void WriteVMargin(float value) {Write();events.emplace_back("writeVMargin");state.vMargin=Bits(value);}
    std::int32_t ReadMinimumLines() {events.emplace_back("readMinLines");return state.minimum;}
    std::int32_t ReadMaximumLines() {events.emplace_back("readMaxLines");return state.maximum;}
    void WriteMinimumLines(std::int32_t value) {Write();events.emplace_back("writeMinLines");state.minimum=value;}
    void WriteMaximumLines(std::int32_t value) {Write();events.emplace_back("writeMaxLines");state.maximum=value;}
    void AskForReconfigure() {
        events.emplace_back("reconfigure");++state.reconfigures;
        if(callback) callback(*this);
    }
};
template<class Call> void Refuses(Call&& call,const char* message) {
    bool refused{};
    try {call();}catch(const std::exception&) {refused=true;}
    Require(refused,message);++refusals;
}
void Alignments() {
    for(unsigned h=0u;h<256u;++h) for(unsigned v=0u;v<256u;++v) {
        Operations f;
        QuestVr::NativeTextWindowSetters::SetAlignments(f,static_cast<std::uint8_t>(h),static_cast<std::uint8_t>(v));
        Require(f.state.horizontal==h && f.state.vertical==v,"Full Byte alignment range was clamped");
        Require(f.events==std::vector<std::string>{"writeHAlign","writeVAlign"},"Alignment write order/callback changed");
        Require(f.writes==2u && f.work==2u && !f.state.reconfigures,"Alignment skipped duplicate stores or asked layout");
    }
    Operations same;
    QuestVr::NativeTextWindowSetters::SetAlignments(same,same.state.horizontal,same.state.vertical);
    Require(same.writes==2u,"Alignment invented an equality shortcut");
}
void FloatComparisons() {
    const std::array<std::uint32_t,17u> samples{
        0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,
        Bits(1.0f),Bits(-1.0f),Bits(3.0f),0x7f7fffffu,0xff7fffffu,
        0x7f800000u,0xff800000u,0x7fc01234u,0xffc05678u,0x7f800001u,0xff800001u};
    for(const auto a:samples) for(const auto b:samples) {
        const auto isNan=[](std::uint32_t bits) {return (bits&0x7fffffffu)>0x7f800000u;};
        const bool expected=isNan(a) || isNan(b) || a==b || ((a|b)&0x7fffffffu)==0u;
        Require(QuestVr::NativeTextWindowSetters::CompareEqualC3(Float(a),Float(b))==expected,
            "Stored Float equality differs from x87 equality-or-unordered C3");
    }
    for(const auto oldH:samples) for(const auto incomingH:samples)
        for(const auto oldV:samples) for(const auto incomingV:samples) {
            Operations f;f.state.hMargin=oldH;f.state.vMargin=oldV;const auto before=f.state;
            const auto equal=[](std::uint32_t a,std::uint32_t b) {
                if((a&0x7fffffffu)>0x7f800000u || (b&0x7fffffffu)>0x7f800000u) return true;
                if((a&0x7fffffffu)==0u && (b&0x7fffffffu)==0u) return true;
                return a==b;
            };
            const bool sameH=equal(oldH,incomingH),same= sameH && equal(oldV,incomingV);
            QuestVr::NativeTextWindowSetters::SetMargins(f,Float(incomingH),Float(incomingV));
            std::vector<std::string> expected{"readHMargin"};
            if(sameH) expected.emplace_back("readVMargin");
            if(!same) {
                expected.emplace_back("writeHMargin");expected.emplace_back("writeVMargin");expected.emplace_back("reconfigure");
            }
            Require(f.events==expected,"Margin read/write/reconfigure source order changed");
            Require(f.state.hMargin==(same?before.hMargin:incomingH) &&
                f.state.vMargin==(same?before.vMargin:incomingV),"Margin store lost a NaN payload/signed zero/raw Float");
            Require(f.writes==(same?0u:2u) && f.state.reconfigures==(same?0u:1u) && f.work==expected.size(),
                "Margin equality changed write/callback/work counts");
        }
}
void Lines() {
    const std::array<std::int32_t,9u> samples{
        std::numeric_limits<std::int32_t>::min(),-99,-1,0,1,3,27,99,std::numeric_limits<std::int32_t>::max()};
    for(const auto oldMin:samples) for(const auto oldMax:samples)
        for(const auto incomingMin:samples) for(const auto incomingMax:samples) {
            Operations f;f.state.minimum=oldMin;f.state.maximum=oldMax;
            const auto nonnegativeMin=incomingMin<0?0:incomingMin;
            const auto effectiveMax=incomingMax<0?0:incomingMax;
            const auto effectiveMin=nonnegativeMin<effectiveMax?nonnegativeMin:effectiveMax;
            const bool sameMin=oldMin==effectiveMin,same=sameMin && oldMax==effectiveMax;
            QuestVr::NativeTextWindowSetters::SetLines(f,incomingMin,incomingMax);
            std::vector<std::string> expected{"readMinLines"};
            if(sameMin) expected.emplace_back("readMaxLines");
            if(!same) {
                expected.emplace_back("writeMinLines");expected.emplace_back("writeMaxLines");expected.emplace_back("reconfigure");
            }
            Require(f.events==expected,"Line setter read/write/reconfigure source order changed");
            Require(f.state.minimum==effectiveMin && f.state.maximum==effectiveMax,"Line signed clamp/min semantics changed");
            Require(f.writes==(same?0u:2u) && f.state.reconfigures==(same?0u:1u) && f.work==expected.size(),
                "Line equality changed write/callback/work counts");
        }
}
void CallbacksAndFailures() {
    Operations margins;
    margins.callback=[](Operations& f) {
        Require(f.state.hMargin==Bits(1.0f) && f.state.vMargin==Bits(2.0f),"Layout saw only one margin store");
        f.state.hMargin=Bits(99.0f);f.state.vMargin=Bits(-9.0f);
    };
    QuestVr::NativeTextWindowSetters::SetMargins(margins,1.0f,2.0f);
    Require(margins.state.hMargin==Bits(99.0f) && margins.state.vMargin==Bits(-9.0f),"Setter overwrote reentrant layout changes");
    Operations lines;
    lines.callback=[](Operations& f) {
        Require(f.state.minimum==2 && f.state.maximum==4,"Layout saw only one line store");
        f.state.minimum=7;f.state.maximum=8;
    };
    QuestVr::NativeTextWindowSetters::SetLines(lines,2,4);
    Require(lines.state.minimum==7 && lines.state.maximum==8,"Line setter overwrote reentrant layout changes");
    for(const bool lineCase:{false,true}) {
        Operations thrown;const auto before=thrown.state;
        thrown.callback=[](Operations& f) {
            Require(f.writes==2u,"Throwing layout was entered before both stores");
            throw std::runtime_error("Explicit unsupported visible layout");
        };
        Refuses([&] {
            if(lineCase) QuestVr::NativeTextWindowSetters::SetLines(thrown,2,4);
            else QuestVr::NativeTextWindowSetters::SetMargins(thrown,1.0f,2.0f);
        },"Layout failure was swallowed");
        Require(thrown.writes==2u && thrown.state.reconfigures==1u && thrown.events.back()=="reconfigure",
            "Setter rolled back locally, retried or continued after layout failure");
        // This is only the explicit mock outer-root ownership control.
        thrown.state=before;
        Require(thrown.state==before,"Mock root could not restore its own complete state");
    }
    for(unsigned setter=0u;setter<3u;++setter) {
        Operations complete;
        const auto call=[setter](Operations& f) {
            if(setter==0u) QuestVr::NativeTextWindowSetters::SetAlignments(f,255u,254u);
            else if(setter==1u) QuestVr::NativeTextWindowSetters::SetMargins(f,1.0f,2.0f);
            else QuestVr::NativeTextWindowSetters::SetLines(f,2,4);
        };
        call(complete);
        for(std::size_t cut=0u;cut<complete.work;++cut) {
            Operations f;f.workLimit=cut;
            Refuses([&] {call(f);},"Shared work cut did not propagate");
            Require(f.events==std::vector<std::string>(complete.events.begin(),complete.events.begin()+cut),
                "Work cut exposed reordered or post-refusal operations");
            Require(!f.state.reconfigures,"Work cut executed layout after failure");
        }
        for(std::size_t cut=0u;cut<2u;++cut) {
            Operations f;f.writeLimit=cut;
            Refuses([&] {call(f);},"Shared write cut did not propagate");
            Require(f.writes==cut && !f.state.reconfigures,"Write cut performed layout or extra stores");
        }
    }
    Operations lazy;lazy.state.hMargin=Bits(3.0f);lazy.state.vMargin=Bits(2.0f);lazy.workLimit=1u;
    Refuses([&] {QuestVr::NativeTextWindowSetters::SetMargins(lazy,3.0f,2.0f);},"Equal-H path skipped required V read");
    Require(lazy.events==std::vector<std::string>{"readHMargin"},"V read occurred beyond admitted root work");
}
struct ContentOperations {
    std::u16string text{u"Before"};bool accelerator{true};std::int32_t key{17};
    std::vector<std::string> events;std::size_t work{},workLimit{100000u},writes{},writeLimit{100000u};
    bool dirty{},throwLayout{};
    void Step() {if(work==workLimit)throw std::runtime_error("Mock content work");++work;}
    void Write() {if(writes==writeLimit)throw std::runtime_error("Mock content write");++writes;}
    std::u16string_view ReadText() {events.emplace_back("read");return text;}
    void WriteText(std::u16string_view incoming) {Write();events.emplace_back("text");text=incoming;}
    bool TextIsAccelerator() {events.emplace_back("flag");return accelerator;}
    std::int32_t ReadAcceleratorKey() {events.emplace_back("readKey");return key;}
    void WriteAcceleratorKey(std::int32_t next) {Write();events.emplace_back("key");key=next;}
    void DirtyAccelerators() {events.emplace_back("dirty");dirty=true;}
    void AskForReconfigure() {
        events.emplace_back("layout");if(throwLayout)throw std::runtime_error("Mock content layout");
    }
};
void Content() {
    using namespace QuestVr::NativeTextContent;
    const auto reference=[](std::u16string_view input) {
        input=QuestVr::UnrealString::CString(input);std::u16string out;
        for(std::size_t i=0;i<input.size();++i) {
            if(input[i]!=u'|'||i+1u==input.size())out+=input[i];
            else if(input[++i]==u'n')out+=u'\n';
            else {out+=u'|';out+=input[i];}
        }
        return out;
    };
    constexpr std::array<char16_t,10u> alphabet{u'|',u'n',u'N',u'&',u'A',u'a',u'\0',u'\n',0xd800u,0xdc00u};
    for(std::size_t length=0u;length<=5u;++length) {
        std::size_t count=1u;for(std::size_t i=0u;i<length;++i)count*=alphabet.size();
        for(std::size_t code=0u;code<count;++code) {
            std::u16string input;auto next=code;
            for(std::size_t i=0u;i<length;++i){input+=alphabet[next%alphabet.size()];next/=alphabet.size();}
            const auto expected=reference(input);const auto capacity=input.capacity();std::size_t work{};
            ConvertScriptString(input,[&]{++work;});
            Require(input==expected&&input.capacity()==capacity,"Original |n conversion/pairing/NUL/surrogate behavior changed");
            Require(work<=length,"Original in-place converter reprocessed output or exceeded input work");
        }
    }
    for(const std::size_t size:{1023u,1024u,1025u,2048u,8192u}) {
        std::u16string source(size,u'X');source+=u"|n||n|&A|";
        const auto expected=reference(source);ConvertScriptString(source,[]{});
        Require(source==expected,"Native script conversion lost original chunk-boundary equivalence");
    }
    for(std::uint32_t unit=0u;unit<=0xffffu;++unit) {
        const std::u16string text{u'|',u'&',static_cast<char16_t>(unit),u'|',u'&',u'Z'};
        const auto key=AcceleratorKey(text,[]{});
        Require(key==(unit<0xffu?static_cast<std::int32_t>(unit):0),
            "Accelerator skipped first invalid/empty key or changed unsigned UTF16<255 range");
    }
    for(const auto& sample:std::array<std::pair<std::u16string,std::int32_t>,8u>{
        std::pair{u"||&A",0},std::pair{u"|n|&B",66},std::pair{u"one |&C two |&D",67},
        std::pair{u"|&",0},std::pair{u"|",0},std::pair{u"&A",0},std::pair{u"|N|&a",97},std::pair{u"|||&Q",81}})
        Require(AcceleratorKey(sample.first,[]{})==sample.second,"Accelerator scanner reprocessed an escaped bar pair");
    for(unsigned a=0u;a<256u;++a)for(unsigned b=0u;b<256u;++b) {
        ContentOperations f;f.text={static_cast<char16_t>(a)};const auto previous=f.text;
        const std::u16string incoming{static_cast<char16_t>(b)};
        SetText(f,incoming);
        const auto oldView=QuestVr::UnrealString::CString(previous),newView=QuestVr::UnrealString::CString(incoming);
        const bool equal=QuestVr::UnrealString::EqualInsensitive(oldView,newView);
        Require(f.text==(equal?previous:std::u16string(newView)),"Case-insensitive SetText changed equal case or missed replacement");
        Require(f.writes==(equal?0u:2u)&&f.events.front()=="read"&&
            (equal?f.events.size()==1u:f.events.back()=="layout"),"SetText equality/layout/write protocol changed");
    }
    ContentOperations changed;SetText(changed,u"a |&Q b");
    Require(changed.key==81&&changed.dirty&&changed.events==std::vector<std::string>{
        "read","text","flag","readKey","key","dirty","layout"},"Text/accelerator/layout order changed");
    ContentOperations sameKey;sameKey.key=81;SetText(sameKey,u"|&Q");
    Require(sameKey.writes==2u&&!sameKey.dirty,"Equal accelerator key skipped its store or dirtied unnecessarily");
    ContentOperations disabled;disabled.accelerator=false;SetText(disabled,u"|&Q");
    Require(disabled.key==17&&disabled.writes==1u&&disabled.events==std::vector<std::string>{"read","text","flag","layout"},
        "Disabled text accelerator rewrote its key");
    ContentOperations same;same.text=u"Alpha";SetText(same,u"ALPHA");
    Require(same.text==u"Alpha"&&!same.writes&&same.events==std::vector<std::string>{"read"},
        "Case-only text change performed native writes/layout");
    ContentOperations throwing;throwing.throwLayout=true;
    Refuses([&]{SetText(throwing,u"|&K");},"SetText swallowed layout failure");
    Require(throwing.text==u"|&K"&&throwing.key==75&&throwing.events.back()=="layout",
        "SetText rolled back locally before its caller could observe the failure");
    for(std::size_t cut=0u;cut<changed.work;++cut) {
        ContentOperations f;f.workLimit=cut;
        Refuses([&]{SetText(f,u"a |&Q b");},"SetText work cut did not propagate");
        Require(f.work==cut&&(f.events.empty()||f.events.back()!="layout"),"SetText continued after exhausted native work");
    }
    for(std::size_t cut=0u;cut<2u;++cut) {
        ContentOperations f;f.writeLimit=cut;
        Refuses([&]{SetText(f,u"a |&Q b");},"SetText write cut did not propagate");
        Require(f.writes==cut&&f.events.back()!="layout","SetText performed post-refusal writes/layout");
    }
}
} // namespace

int main() {
    try {
        Alignments();FloatComparisons();Lines();CallbacksAndFailures();Content();
        std::cout<<"PASS original TextWindow setters: "<<checks<<" checks, "<<refusals<<" explicit refusals\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL original TextWindow setters: "<<error.what()<<'\n';return 1;
    }
}
