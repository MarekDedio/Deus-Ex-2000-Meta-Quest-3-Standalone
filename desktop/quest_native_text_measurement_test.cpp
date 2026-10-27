#include "quest_native_text_measurement.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace Text=QuestVr::NativeTextMeasurement;
std::size_t checks{},refusals{};
void Require(const bool condition,const char* message) {
    ++checks;if(!condition)throw std::runtime_error(message);
}
std::uint32_t Word(const float value) {return std::bit_cast<std::uint32_t>(value);}
float Float(const std::uint32_t value) {return std::bit_cast<float>(value);}
bool Same(const Text::State& a,const Text::State& b) {
    return a.bold==b.bold&&a.explicitColor==b.explicitColor&&a.linefeed==b.linefeed&&
        a.accelerator==b.accelerator&&a.planeWords==b.planeWords&&a.planeDefinedMask==b.planeDefinedMask;
}
template<class Function>
void Refuses(Function function,const char* message) {
    bool threw{};try {function();}catch(const std::exception&) {threw=true;}
    Require(threw,message);++refusals;
}
struct Operations {
    using Font=int;
    Font normal{1},bold{2};bool parse{true},trace{};
    float spacing{1.0f};
    std::array<std::uint32_t,4u> plane{0x3e800000u,0x3f000000u,0x3f400000u,0x80000000u};
    std::map<std::pair<Font,char16_t>,Text::Glyph> overrides;
    std::size_t work{},limit{std::numeric_limits<std::size_t>::max()},calls{},faultCall{std::numeric_limits<std::size_t>::max()};
    std::vector<std::string> events;
    void Step() {if(work==limit)throw std::runtime_error("Fixture native work exhausted");++work;}
    void Call(const char* name) {
        if(calls++==faultCall)throw std::runtime_error("Fixture actual provider failed");
        if(trace)events.emplace_back(name);
    }
    Font NormalFont() {Call("normal");return normal;}
    Font BoldFont() {Call("bold");return bold;}
    bool ParseMetachars() {Call("parse");return parse;}
    auto TextPlaneWords() {Call("plane");return plane;}
    float TextVSpacing() {Call("spacing");return spacing;}
    Text::Glyph Glyph(const Font font,const char16_t character) {
        Call("glyph");
        const auto found=overrides.find({font,character});if(found!=overrides.end())return found->second;
        if(!font)throw std::runtime_error("A missing Font was passed to glyph authority");
        if(character==u' ')return {font==1?2:4,font==1?7:13};
        if(character==u'\t'||character==u'\r'||character==u'\v'||character==u'\f')return {3,6};
        if(character==u'\n')return {};
        if(character<128u)return {font==1?5:9,font==1?7:13};
        return {};
    }
};
Text::State FixtureState() {
    Text::State state;state.bold=0u;state.explicitColor=1u;state.linefeed=5u;state.accelerator=9u;
    state.planeWords={0x7f801234u,0x80000000u,0x7f800000u,0xffc01234u};state.planeDefinedMask=15u;return state;
}
std::pair<char16_t,std::size_t> Scan(Operations& operations,const std::u16string_view input,Text::State& state) {
    std::size_t cursor{};const auto c=Text::NextChar(operations,input,cursor,input.size(),state);return {c,cursor};
}
void Scanner() {
    for(const auto input:{u"",u"|",u"|!"}) {
        Operations op;auto state=FixtureState();const auto before=state;
        const auto [character,cursor]=Scan(op,input,state);
        Require(character==0&&cursor==std::u16string_view(input).size(),"Trailing control did not terminate at its exact bound");
        auto expected=before;expected.accelerator=0u;
        Require(Same(state,expected),"Trailing pipe/prefix unexpectedly applied a command");
    }
    for(const auto input:{u"|ba",u"|Ba",u"|b|ba",u"|B|Ba"}) {
        Operations op;Text::State state;const auto [character,cursor]=Scan(op,input,state);
        Require(character==u'a'&&cursor==std::u16string_view(input).size()&&state.bold==1u,
            "Bold command toggled or failed ASCII command folding");
    }
    for(const auto input:{u"|!ba",u"|!Ba",u"|!b"}) {
        Operations op;Text::State state;state.bold=255u;const auto [character,cursor]=Scan(op,input,state);
        Require(character==(std::u16string_view(input).back()==u'a'?u'a':0)&&
            cursor==std::u16string_view(input).size()&&!state.bold,"Bold clearing did not apply at terminal command");
    }
    for(const auto input:{u"|qa",u"|Qa",u"|!Qa",u"||a"}) {
        Operations op;Text::State state;const auto [character,cursor]=Scan(op,input,state);
        const std::u16string_view view(input);const auto index=view[1u]==u'!'?2u:1u;
        Require(character==view[index]&&cursor==index+1u,"Unknown command did not return the original command unit");
    }
    {
        Operations op;op.parse=false;auto state=FixtureState();const auto before=state;
        const auto [character,cursor]=Scan(op,u"|ba",state);auto expected=before;expected.accelerator=0u;
        Require(character==u'|'&&cursor==1u&&Same(state,expected),"Disabled metachar parsing still consumed a control");
    }
    {
        Operations op;Text::State state;std::size_t cursor{};const std::u16string input=u"|&|baZ";
        Require(Text::NextChar(op,input,cursor,input.size(),state)==u'a'&&state.bold&&state.accelerator,
            "Accelerator did not survive subsequent same-call bold control");
        Require(Text::NextChar(op,input,cursor,input.size(),state)==u'Z'&&!state.accelerator,
            "Accelerator flag was not per-call cleared");
    }
    constexpr std::array<std::array<std::uint8_t,3u>,8u> palette{{
        {{0u,0u,0u}},{{255u,255u,255u}},{{255u,0u,0u}},{{0u,255u,0u}},
        {{255u,255u,0u}},{{0u,0u,255u}},{{255u,0u,255u}},{{0u,255u,255u}}}};
    const auto normalized=[](const std::uint8_t byte) {
        // Every channel rounds identically to this independently formed exact
        // rational at binary32 precision; the helper retains the actual retail
        // slightly different binary64 input and PC64 multiplication.
        return Word(static_cast<float>(static_cast<double>(byte)/255.0));
    };
    for(std::size_t digit=0u;digit<palette.size();++digit) {
        Operations op;auto state=FixtureState();const auto w=state.planeWords[3u];
        const std::u16string input{u'|',u'p',static_cast<char16_t>(u'0'+digit),u'X'};
        const auto [character,cursor]=Scan(op,input,state);
        Require(character==u'X'&&cursor==4u&&state.explicitColor==1u&&state.planeDefinedMask==15u,
            "Palette command did not consume exactly its one valid digit");
        for(std::size_t channel=0u;channel<3u;++channel)
            Require(state.planeWords[channel]==normalized(palette[digit][channel]),"Original palette RGB differs");
        Require(state.planeWords[3u]==w,"Palette wrote the preserved Plane W word");
    }
    for(const auto input:{u"|!c",u"|!cX",u"|!pX"}) {
        Operations op;auto state=FixtureState();const auto [character,cursor]=Scan(op,input,state);
        Require(character==(std::u16string_view(input).back()==u'X'?u'X':0)&&
            cursor==std::u16string_view(input).size()&&!state.explicitColor&&state.planeWords==op.plane&&
            state.planeDefinedMask==15u,"Color restore failed to copy all four raw GC words");
    }
    {
        Operations op;auto state=FixtureState();const auto before=state;const auto [character,cursor]=Scan(op,u"|!p",state);
        auto expected=before;expected.accelerator=0u;
        Require(!character&&cursor==3u&&Same(state,expected),"Terminal !p incorrectly restored the context color");
    }
    for(const auto input:{u"|p8",u"|pQ",u"|p-"}) {
        Operations op;auto state=FixtureState();const auto before=state;const auto [character,cursor]=Scan(op,input,state);
        auto expected=before;expected.accelerator=0u;
        Require(character==std::u16string_view(input).back()&&cursor==3u&&Same(state,expected),
            "Invalid palette digit was consumed as an invisible control");
    }
    constexpr std::array<char16_t,16u> hex{u'0',u'1',u'2',u'3',u'4',u'5',u'6',u'7',u'8',u'9',u'A',u'B',u'C',u'D',u'E',u'F'};
    for(std::uint16_t channel=0u;channel<256u;++channel) {
        Operations op;Text::State state;state.planeWords[3u]=0x7f801234u;
        std::u16string input=u"|c";
        for(unsigned c=0u;c<3u;++c) {input+=hex[channel>>4u];input+=hex[channel&15u];}
        input+=u'X';const auto [character,cursor]=Scan(op,input,state);
        Require(character==u'X'&&cursor==9u&&state.explicitColor&&state.planeDefinedMask==7u,
            "Six-digit RGB control changed cursor, state or unknown-W metadata");
        for(std::size_t c=0u;c<3u;++c)Require(state.planeWords[c]==normalized(static_cast<std::uint8_t>(channel)),
            "Hex RGB normalization changed an actual channel");
        Require(state.planeWords[3u]==0x7f801234u,"Hex parsing changed an unwritten W word");
    }
    struct Partial {const char16_t* input;char16_t returned;std::size_t cursor;std::array<std::uint8_t,3u> rgb;};
    const std::array partials{
        Partial{u"|c",0u,2u,{0u,0u,0u}},Partial{u"|c1",0u,3u,{16u,0u,0u}},
        Partial{u"|c1Z",u'Z',4u,{16u,0u,0u}},Partial{u"|cZZ",u'Z',3u,{0u,0u,0u}},
        Partial{u"|c12",0u,4u,{18u,0u,0u}},Partial{u"|c123",0u,5u,{18u,48u,0u}},
        Partial{u"|c123Z",u'Z',6u,{18u,48u,0u}},Partial{u"|c12345",0u,7u,{18u,52u,80u}},
        Partial{u"|c12345Z",u'Z',8u,{18u,52u,80u}},Partial{u"|caBcDeFX",u'X',9u,{171u,205u,239u}}};
    for(const auto& fixture:partials) {
        Operations op;Text::State state;const auto [character,cursor]=Scan(op,fixture.input,state);
        Require(character==fixture.returned&&cursor==fixture.cursor&&state.explicitColor,
            "Partial RGB parsing lost original prefix consumption");
        for(std::size_t c=0u;c<3u;++c)Require(state.planeWords[c]==normalized(fixture.rgb[c]),"Partial RGB byte differs");
    }
}
void Lines() {
    struct Fixture {const char16_t* text;float width;std::size_t next,length;float measured;bool present,linefeed;};
    const std::array fixtures{
        Fixture{u"",0,0u,0u,0,false,false},Fixture{u"a",0,1u,1u,5,true,false},
        Fixture{u"ab",5,1u,1u,5,true,false},Fixture{u"a b",5,2u,1u,5,true,false},
        Fixture{u"a b",7,2u,1u,5,true,false},Fixture{u" ab",5,1u,1u,2,true,false},
        Fixture{u"aa bb",12,3u,2u,10,true,false},Fixture{u"aa  ",11,4u,2u,10,true,false},
        Fixture{u"a ",7,2u,2u,7,true,false},Fixture{u"a ",5,2u,1u,5,true,false},
        Fixture{u"a\nb",100,2u,1u,5,true,true},Fixture{u"a\n",100,2u,1u,5,true,true},
        Fixture{u"\n",100,1u,0u,0,true,true},Fixture{u"a\r",100,2u,2u,8,true,false},
        Fixture{u"a\t",100,2u,2u,8,true,false},Fixture{u"a\v",100,2u,2u,8,true,false},
        Fixture{u"a\f",100,2u,2u,8,true,false},Fixture{u"ab",4,1u,1u,5,true,false},
        Fixture{u"a|b",100,3u,1u,5,true,false},Fixture{u"|b",100,2u,0u,0,true,false}};
    for(const auto& fixture:fixtures) {
        Operations op;Text::State state;const std::u16string_view text(fixture.text);
        const auto line=Text::GetLine(op,text,0u,text.size(),fixture.width,state);
        Require(line.start==0u&&line.next==fixture.next&&line.length==fixture.length&&
            Word(line.width)==Word(fixture.measured)&&line.present==fixture.present&&
            (state.linefeed!=0u)==fixture.linefeed,"Original line boundary/length/width fixture differs");
    }
    {
        Operations op;Text::State state;const std::u16string_view text=u"a\n";
        auto line=Text::GetLine(op,text,0u,text.size(),100.0f,state);
        line=Text::GetLine(op,text,line.next,text.size(),100.0f,state);
        Require(line.present&&line.next==text.size()&&!line.length&&!state.linefeed,"Terminal LF lost its one empty line");
        line=Text::GetLine(op,text,line.next,text.size(),100.0f,state);
        Require(!line.present,"Terminal LF produced infinite empty lines");
    }
    for(const auto width:{0.0f,-0.0f,-1.0f,Float(0x7fc01234u),Float(0x7f801234u),Float(0xff800000u)}) {
        Operations op;Text::State state;const auto line=Text::GetLine(op,u"ab",0u,2u,width,state);
        Require(line.length==2u&&line.next==2u&&line.width==10.0f,"Nonpositive/unordered width was not unrestricted");
    }
    {
        Operations op;op.overrides[{1,u'a'}]={16777216,7};op.overrides[{1,u'b'}]={1,7};Text::State state;
        const auto line=Text::GetLine(op,u"ab",0u,2u,16777216.0f,state);
        Require(line.next==1u&&line.length==1u&&line.width==16777216.0f,
            "Line compared an eagerly rounded Float rather than retained PC64 glyph sum");
    }
    {
        Operations op;op.normal=op.bold=0;Text::State state;const auto line=Text::GetLine(op,u"ab cd",0u,5u,0.001f,state);
        Require(line.length==5u&&line.next==5u&&line.width==0.0f,"Font None invented width or wrapping");
    }
    {
        Operations op;Text::State state;const std::u16string text(100001u,u'a');
        const auto line=Text::GetLine(op,text,0u,text.size(),0.0f,state);
        Require(line.length==100000u&&line.next==100000u&&line.width==500000.0f,
            "Unrestricted width was not original exact500000 inside GetLine");
    }
}
void Extents() {
    struct Fixture {const char16_t* text;float width,x,y;};
    const std::array fixtures{
        Fixture{u"",0,0,0},Fixture{u"a",0,5,8},Fixture{u"ab",0,10,8},
        Fixture{u"ab",5,5,16},Fixture{u"a b",5,5,16},Fixture{u"a\nb",0,5,16},
        Fixture{u"a\n",0,5,16},Fixture{u"\n",0,0,16},Fixture{u"|b",0,0,8},
        Fixture{u"a|b",0,5,14},Fixture{u"a|b\n",0,5,28},Fixture{u"|bA",0,9,14},
        Fixture{u"a|bB|!b",0,14,14},Fixture{u"|",0,0,8},Fixture{u"|!",0,0,8}};
    for(const auto& fixture:fixtures) {
        Operations op;float x=-123.0f,y=-456.0f;Text::GetTextExtent(op,fixture.text,fixture.width,x,y);
        Require(Word(x)==Word(fixture.x)&&Word(y)==Word(fixture.y),"Original extent ending-state/font-cache fixture differs");
    }
    {
        Operations op;float both=-1.0f;Text::GetTextExtent(op,u"a",0.0f,both,both);
        Require(both==8.0f,"Aliased extent outputs did not store X before Y");
    }
    {
        Operations op;op.overrides[{1,0xd83du}]={3,8};op.overrides[{1,0xde00u}]={4,9};
        float x{},y{};Text::GetTextExtent(op,u"\xd83d\xde00",0.0f,x,y);
        Require(x==7.0f&&y==10.0f,"Surrogate pair was normalized into one glyph rather than two UTF16 units");
        const std::u16string hidden{u'a',u'\0',u'b'};Text::GetTextExtent(op,hidden,0.0f,x,y);
        Require(x==5.0f&&y==8.0f,"CString extent measured hidden post-NUL suffix");
    }
    {
        Operations op;op.overrides[{1,u'z'}]={};op.overrides[{1,u'Z'}]={99,99};float x{},y{};
        Text::GetTextExtent(op,u"z",0.0f,x,y);
        Require(x==0.0f&&y==8.0f,"Missing glyph used uppercase/space width fallback");
        op.overrides[{1,u' '}]={2,-3};Text::GetTextExtent(op,u"z",0.0f,x,y);
        Require(y==-2.0f,"Extent space fallback was incorrectly clamped nonnegative");
    }
    for(const auto spacing:{Float(0x7f800000u),Float(0xff800000u),Float(0x7f801234u),Float(0x7fc05678u)}) {
        Operations op;op.spacing=spacing;float x{},y{};Text::GetTextExtent(op,u"a",0.0f,x,y);
        const auto expected=QuestVr::NativeScalar::StoreFloat(QuestVr::NativeScalar::Add(
            QuestVr::NativeScalar::Load(7.0f),QuestVr::NativeScalar::Load(spacing)));
        Require(x==5.0f&&Word(y)==Word(expected),"Spacing nonfinite/raw payload did not follow PC64 arithmetic");
    }
    {
        Operations op;op.normal=op.bold=0;op.spacing=-3.5f;float x{},y{};Text::GetTextExtent(op,u"a\n",0.0f,x,y);
        Require(x==0.0f&&y==-7.0f,"No-Font lines fabricated glyph height or dropped real spacing");
    }
    {
        Operations op;op.trace=true;op.overrides[{1,u'X'}]={3,99};op.overrides[{2,u'X'}]={3,101};
        Require(Text::GetFontHeight(op,false)==13.0f&&Text::GetFontHeight(op,true)==14.0f,
            "GetFontHeight did not use maximum actual normal/bold SPACE glyph");
        op.normal=op.bold=0;Require(Text::GetFontHeight(op,false)==0.0f&&Text::GetFontHeight(op,true)==1.0f,
            "GetFontHeight fabricated a default font height");
    }
}
void BoundsAndFailures() {
    Operations complete;float x{},y{};Text::GetTextExtent(complete,u"a |bB\n|c12ZQ",7.0f,x,y);
    for(std::size_t cut=0u;cut<complete.work;++cut) {
        Operations op;op.limit=cut;float a=123.0f,b=-456.0f;
        Refuses([&] {
            try {Text::GetTextExtent(op,u"a |bB\n|c12ZQ",7.0f,a,b);}
            catch(...) {a=123.0f;b=-456.0f;throw;} // Explicit enclosing-root fixture undo.
        },"Native measurement work cut failed to propagate");
        Require(op.work==cut&&a==123.0f&&b==-456.0f,"Root fixture undo/work admission failed");
    }
    for(std::size_t call=0u;call<complete.calls;++call) {
        Operations op;op.faultCall=call;float a=123.0f,b=-456.0f;
        Refuses([&] {
            try {Text::GetTextExtent(op,u"a |bB\n|c12ZQ",7.0f,a,b);}
            catch(...) {a=123.0f;b=-456.0f;throw;}
        },"Native font/context provider fault was swallowed");
        Require(a==123.0f&&b==-456.0f,"Provider fault did not preserve enclosing fixture outputs");
    }
    for(const auto& bounds:{std::pair{2u,1u},std::pair{0u,2u}}) {
        Operations op;Text::State state;
        Refuses([&] {auto cursor=static_cast<std::size_t>(bounds.first);
            static_cast<void>(Text::NextChar(op,u"a",cursor,bounds.second,state));},"Scanner accepted invalid owned offsets");
        Refuses([&] {static_cast<void>(Text::GetLine(op,u"a",bounds.first,bounds.second,5.0f,state));},
            "Line reader accepted invalid owned offsets");
    }
}
void Generated() {
    constexpr std::array<char16_t,5u> alphabet{u'a',u' ',u'\n',u'|',u'b'};
    std::size_t combinations=1u;
    for(std::size_t length=0u;length<=5u;++length) {
        for(std::size_t encoded=0u;encoded<combinations;++encoded) {
            std::u16string input(length,u' ');auto remainder=encoded;
            for(auto& c:input) {c=alphabet[remainder%alphabet.size()];remainder/=alphabet.size();}
            for(const auto width:{0.0f,5.0f,11.0f})for(const bool parse:{false,true}) {
                Operations first;first.parse=parse;float x{},y{};Text::GetTextExtent(first,input,width,x,y);
                Operations second;second.parse=parse;float againX{},againY{};
                Text::GetTextExtent(second,input,width,againX,againY);
                Require(Word(x)==Word(againX)&&Word(y)==Word(againY)&&first.work==second.work&&first.calls==second.calls,
                    "Repeated pure text measurement retained process-global parse state");
                Text::State state;std::size_t next{},lines{};
                for(;;) {
                    const auto line=Text::GetLine(first,input,next,input.size(),width,state);
                    Require(line.start==next&&line.next>=next&&line.next<=input.size()&&line.length<=line.next-next,
                        "Generated line exposed out-of-range raw offsets");
                    if(!line.present)break;
                    Require(++lines<=input.size()+1u,"Generated line parser failed bounded progress");next=line.next;
                }
            }
        }
        combinations*=alphabet.size();
    }
}
} // namespace

int main() {
    try {
        Scanner();Lines();Extents();BoundsAndFailures();Generated();
        std::cout<<"PASS original native GC text measurement: "<<checks<<" checks / "<<refusals
            <<" refusal controls; bounded UTF16/control/partial-RGB/word-wrap/ending-state/space-font/PC64/output-order policy. "
            "Explicit mock context/fonts only; not genuine Font provider, GC lifetime, original HUD startup or headset acceptance.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL original native GC text measurement: "<<error.what()<<'\n';return 1;}
}
