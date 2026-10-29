#include "quest_original_string_to_name.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace {
std::size_t checks{},refusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Refused(F&& action,const char* why){bool refused{};try{action();}catch(const std::runtime_error&){refused=true;}Require(refused,why);++refusals;}
void Alphabet(){
    // Exhaustive UTF16 code units, not locale/UTF8-byte classification.
    for(std::uint32_t unit=0u;unit<=0xffffu;++unit){
        bool interned{},error{};std::size_t work{};const std::u16string input{u'P',static_cast<char16_t>(unit),u'Q'};
        QuestVr::OriginalStringToNameQueries query{
            [&](std::size_t amount){work+=amount;},[&](char16_t bad,std::u16string_view original){
                Require(!interned&&bad==unit&&original==input,"GError did not receive the original invalid unit/CString");error=true;
            }};
        const auto result=QuestVr::RunOriginalStringToName(input,query,[&](std::u16string_view value){interned=true;return std::u16string(value);});
        const bool valid=unit==0u||(unit>='a'&&unit<='z')||(unit>='A'&&unit<='Z')||
            (unit>='0'&&unit<='9')||unit=='_'||unit=='-';
        Require(interned&&error!=valid,"Original ASCII-only lexical/error gate changed");
        Require(result==(unit==0u?std::u16string(u"P"):input),"Returning error provider sanitized or truncated the invalid CString");
        Require(work>0u,"UTF16 traversal escaped work accounting");
    }
}
void FailureAndLifetime(){
    bool interned{};const auto intern=[&](std::u16string_view value){interned=true;return std::u16string(value);};
    QuestVr::OriginalStringToNameQueries missing;
    Refused([&]{static_cast<void>(QuestVr::RunOriginalStringToName(u"Bad.Name",missing,intern));},"Invalid source bypassed missing GError");
    Require(!interned,"Missing error provider interned a partial name");
    QuestVr::OriginalStringToNameQueries fatal{{},[](char16_t,std::u16string_view){throw std::runtime_error("Actual error provider refusal");}};
    Refused([&]{static_cast<void>(QuestVr::RunOriginalStringToName(u"Bad Name",fatal,intern));},"Throwing GError returned a name");
    Require(!interned,"GError refusal already mutated the name arena");
    QuestVr::OriginalStringToNameQueries cut{[](std::size_t){throw std::runtime_error("Work cut");},{}};
    Refused([&]{static_cast<void>(QuestVr::RunOriginalStringToName(u"Valid",cut,intern));},"Work cut returned a name");
    Require(!interned,"Work cut invoked the interner");
    Require(QuestVr::RunOriginalStringToName(u"",missing,intern).empty(),"Empty source did not invoke original FName empty path");
    Require(QuestVr::RunOriginalStringToName(u"nOnE",missing,intern)==u"nOnE","Lexical helper changed case/None before real FName constructor");
    const std::u16string longName(4096u,u'A');
    Require(QuestVr::RunOriginalStringToName(longName,missing,intern)==longName,"Invented 63-character name limit");
    const std::u16string trailing{u'O',u'K',u'\0',u'.',static_cast<char16_t>(0xd800u)};
    Require(QuestVr::RunOriginalStringToName(trailing,fatal,intern)==u"OK","Validation inspected after native CString terminator");
}
}
int main(){try{Alphabet();FailureAndLifetime();std::cout<<"PASS quest_original_string_to_name checks="<<checks<<" refusals="<<refusals<<'\n';return 0;}
catch(const std::exception& error){std::cerr<<"FAIL quest_original_string_to_name: "<<error.what()<<'\n';return 1;}}
