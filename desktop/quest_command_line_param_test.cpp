#include "quest_command_line_param.h"

#include <array>
#include <cstddef>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

// Pure static-source contract only. Does not execute a commercial binary,
// supply appCmdLine authority, initialize a player, or certify world startup.
namespace {
std::size_t checks{},refusals{};
void Require(bool value,const char* why) {++checks;if(!value)throw std::runtime_error(why);}
bool Parse(std::u16string_view text,std::u16string_view parameter=u"alladmin") {
    const std::function<void()> work=[]{};return QuestVr::OriginalParseParam(text,parameter,work);
}
template<class Function> void Refuses(Function&& function,const char* why) {
    bool refused{};try {function();}catch(const std::exception&) {refused=true;}
    Require(refused,why);++refusals;
}
void SourceExamples() {
    struct Case {std::u16string_view source;bool result;};
    constexpr std::array<Case,31u> cases{{
        {u"",false},{u"alladmin",false},{u"ALLADMIN",false},
        {u"-alladmin",true},{u"/alladmin",true},{u"-ALLADMIN",true},
        {u"-AllAdMiN",true},{u" -alladmin ",true},{u"\t/alladmin\r\n",true},
        {u"-alladministrator",true},{u"/alladmin=0",true},{u"-alladminfalse",true},
        {u"-alladmi",false},{u"-alladmiN",true},{u"-allad min",false},
        {u"?alladmin",false},{u"alladmin /alladmin",true},
        {u"xalladmin",false},{u"-xalladmin",false},{u"/xalladmin",false},
        {u"x-alladmin",true},{u"1/alladmin",true},{u"---alladmin",true},
        {u"\"-alladmin\"",true},{u"/alladmin-other",true},
        {u"alladminx -alladmin",true},{u"-alladmi -alladmin",true},
        {u"xalladmin/alladmin",true},{u"\u03a9-alladmin",true},
        {u"-\u03a9",false},{u"-x\u03a9alladmin",false}
    }};
    for(const auto& row:cases)Require(Parse(row.source)==row.result,"Original ParseParam source boundary/prefix example differs");
    Require(!Parse(u"-alladmin",u""),"Empty parameter fabricated a match");
    Require(Parse(u"-x",u"x"),"One-unit parameter did not preserve zero-tail comparison");
    Require(!Parse(u"x",u"x"),"ParseParam searched initial Stream unit instead of Stream+1");
    Require(Parse(u"-\u03a9x",u"\u03a9x"),"appStrfind first-unit literal identity was confused with its CRT tail");
    Require(!Parse(u"-x",u"y\u03a9"),"Unreached non-ASCII parameter tail changed a first-unit mismatch");
}
void CStringsAndLocale() {
    const std::u16string before{u"ignored\0-alladmin",17u};
    Require(!Parse(before),"Command text after first NUL was parsed");
    const std::u16string after{u"-alladmin\0ignored",17u};
    Require(Parse(after),"NUL-terminated matching command was lost");
    const std::u16string parameter{u"alladmin\0ignored",16u};
    Require(Parse(u"-alladmin",parameter),"Parameter text after first NUL was compared");
    Require(!Parse(std::u16string_view{u"\0-alladmin",10u}),"Initial command NUL was not authoritative");
    Refuses([]{static_cast<void>(Parse(u"-alla\u03a9min"));},"Reached non-ASCII command comparison silently guessed CRT locale");
    Refuses([]{static_cast<void>(Parse(u"-alladmin",u"alla\u03a9min"));},"Reached non-ASCII parameter comparison silently guessed CRT locale");
    const std::function<void()> absent;
    Refuses([&]{static_cast<void>(QuestVr::OriginalParseParam(u"",u"alladmin",absent));},"Empty command bypassed missing borrowed work");
}
char16_t Fold(char16_t unit) {return unit>=u'A'&&unit<=u'Z'?static_cast<char16_t>(unit+(u'a'-u'A')):unit;}
bool AcceptedOracle(std::u16string_view text,std::u16string_view parameter) {
    // Independent accepted-result oracle: a preceding sign is already a
    // non-alphanumeric delimiter, so every such prefix will be encountered
    // by the source's repeated word-start appStrfind scans. No token splitting.
    const auto end=text.find(u'\0');text=text.substr(0u,end);
    const auto paramEnd=parameter.find(u'\0');parameter=parameter.substr(0u,paramEnd);
    if(parameter.empty())return false;
    for(std::size_t at=1u;at<text.size();++at) {
        if(text[at-1u]!=u'-'&&text[at-1u]!=u'/')continue;
        if(parameter.size()>text.size()-at)continue;
        bool same=true;
        for(std::size_t i=0u;i<parameter.size();++i)if(Fold(text[at+i])!=Fold(parameter[i])) {same=false;break;}
        if(same)return true;
    }
    return false;
}
void ExhaustiveAscii() {
    constexpr std::u16string_view alphabet=u"aA-/ x1";
    std::array<char16_t,6u> text{};
    std::size_t combinations=1u;
    for(std::size_t length=0u;length<=text.size();++length) {
        for(std::size_t value=0u;value<combinations;++value) {
            auto encoded=value;
            for(std::size_t i=0u;i<length;++i) {text[i]=alphabet[encoded%alphabet.size()];encoded/=alphabet.size();}
            const std::u16string_view source{text.data(),length};
            for(const auto parameter:{u"a",u"aa",u"aA",u"-a",u"1"})
                Require(Parse(source,parameter)==AcceptedOracle(source,parameter),"Repeated appStrfind differs from accepted ASCII signed-prefix oracle");
        }
        combinations*=alphabet.size();
    }
}
void WorkCuts() {
    for(const auto source:{u"",u"-alladmin",u"alladmin xalladmin /ALLADMINsuffix",u"-alladmi"}) {
        std::size_t full{};
        const std::function<void()> count=[&]{++full;};
        const auto expected=QuestVr::OriginalParseParam(source,u"alladmin",count);
        Require(full>0u,"Original parser did not charge bounded reads");
        for(std::size_t limit=0u;limit<full;++limit) {
            std::size_t used{};
            const std::function<void()> cut=[&]{if(used==limit)throw std::runtime_error("cut");++used;};
            Refuses([&]{static_cast<void>(QuestVr::OriginalParseParam(source,u"alladmin",cut));},"Work cutoff returned an unverified parser result");
            Require(used==limit,"Parser reset or exceeded its borrowed work cutoff");
        }
        std::size_t used{};
        const std::function<void()> exact=[&]{if(used==full)throw std::runtime_error("excess");++used;};
        Require(QuestVr::OriginalParseParam(source,u"alladmin",exact)==expected&&used==full,"Exact parser work allowance changed result or accounting");
    }
}
}
int main() {
    try {SourceExamples();CStringsAndLocale();ExhaustiveAscii();WorkCuts();
        std::cout<<"Original command-line ParseParam: "<<checks<<" checks / "<<refusals<<" explicit refusals\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
