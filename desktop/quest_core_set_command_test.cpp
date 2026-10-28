#include "quest_core_set_command.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {
namespace Set=QuestVr::CoreSetCommand;
std::size_t checks{},refusals{};
void Require(bool condition,const char* why) {++checks;if(!condition)throw std::runtime_error(why);}
template<class Action>void Refuse(Action&& action) {
    bool refused{};try{action();}catch(const std::runtime_error&){refused=true;}
    Require(refused,"Expected bounded SET parser refusal");++refusals;
}
struct Operations {
    std::size_t work{},workLimit{1u<<20u},admissions{},peak{},byteLimit{1u<<20u};
    void Step() {if(work==workLimit)throw std::runtime_error("Mock shared native work exhausted");++work;}
    void Admit(std::size_t bytes) {
        ++admissions;if(bytes>byteLimit)throw std::runtime_error("Mock native scratch admission failed");
        peak=std::max(peak,bytes);
    }
};
Set::Result Parse(std::u16string_view command) {Operations operations;return Set::Parse(command,operations);}

void Grammar() {
    for(const auto& [command,className,propertyName,value]:std::vector<std::tuple<std::u16string,std::u16string,std::u16string,std::u16string>>{
        {u"SET InputExt T Talk",u"InputExt",u"T",u"Talk"},
        {u" \tSeT\tInputExt  T   Talk ",u"InputExt",u"T",u"Talk "},
        {u"SET \"Extension.InputExt\" \"T\" Talk",u"Extension.InputExt",u"T",u"Talk"},
        {u"SET \"InputExt\"\"T\"Talk",u"InputExt",u"T",u"Talk"},
        {u"SET InputExt T\tTalk",u"InputExt",u"T",u"\tTalk"},
        {u"SET InputExt T \t Talk",u"InputExt",u"T",u"\t Talk"},
        {u"SET InputExt T",u"InputExt",u"T",u""},
        {u"SET InputExt T \"Talk\"",u"InputExt",u"T",u"\"Talk\""},
        {u"SET InputExt Aliases (Alias=Talk,Command=\"Say hello\")",u"InputExt",u"Aliases",u"(Alias=Talk,Command=\"Say hello\")"},
        {u"SET InputExt Aliases[3] Value",u"InputExt",u"Aliases[3]",u"Value"},
        {u"SET Input.Ext A=B One;Two",u"Input.Ext",u"A=B",u"One;Two"},
        {u"SET_ InputExt T Talk",u"_",u"InputExt",u"T Talk"},
        {u"SET\n InputExt T Talk",u"\n",u"InputExt",u"T Talk"},
        {u"SET InputExt\r\nT Talk",u"InputExt\r\nT",u"Talk",u""},
        {u"SET \u00a0InputExt T V",u"\u00a0InputExt",u"T",u"V"}}) {
        const auto result=Parse(command);
        Require(result.recognized&&result.packet.has_value(),"SET packet was not recognized");
        const auto& packet=*result.packet;
        Require(packet.classTokenPresent&&packet.propertyTokenPresent&&packet.propertyParsed&&packet.valueRead,
            "Complete SET packet lost token/phase presence");
        Require(packet.className==className&&packet.propertyName==propertyName&&packet.value==value,
            "SET packet changed original token/value grammar");
    }
    for(const auto command:{u"",u"GET InputExt T",u"SETX C P V",u"SET1 C P V",u"\nSET C P V"}) {
        const auto result=Parse(command);
        Require(!result.recognized&&!result.packet,"Non-SET command became handled SET");
    }
    for(unsigned unit=0u;unit<128u;++unit) {
        std::u16string command=u"SET";command.push_back(static_cast<char16_t>(unit));command.append(u" C P V");
        const bool alphanumeric=(unit>='A'&&unit<='Z')||(unit>='a'&&unit<='z')||(unit>='0'&&unit<='9');
        Require(Parse(command).recognized==!alphanumeric,"SET command suffix differs from actual ParseCommand");
    }
}

void MissingAndCString() {
    for(const auto command:{u"SET",u"SET \t",u"SET \"\" T Talk"}) {
        const auto result=Parse(command);
        Require(result.recognized&&result.packet&&!result.packet->classTokenPresent&&
            result.packet->className.empty()&&!result.packet->propertyParsed&&!result.packet->valueRead,
            "Recognized SET with absent/empty Class did not retain its handled empty-token branch");
    }
    for(const auto command:{u"SET InputExt",u"SET InputExt \"\" Talk"}) {
        const auto result=Parse(command);
        Require(result.recognized&&result.packet&&result.packet->classTokenPresent&&
            result.packet->propertyParsed&&!result.packet->propertyTokenPresent&&result.packet->propertyName.empty()&&
            !result.packet->valueRead,"Recognized SET with absent/empty property parsed a value or lost its branch");
    }
    std::u16string embedded=u"SET InputExt T Talk";embedded.push_back(0);embedded.append(u"ignored tail");
    Require(Parse(embedded).packet->value==u"Talk","SET packet read past the admitted CString terminator");
    const auto unterminated=Parse(u"SET \"InputExt T Talk");
    Require(unterminated.packet->className==u"InputExt T Talk"&&!unterminated.packet->propertyTokenPresent,
        "Unclosed quoted token was rejected or split as ordinary words");
    const auto maximum=std::u16string(u"SET C P ")+std::u16string(Set::MaximumCommandUnits-8u,u'V');
    Require(maximum.size()==Set::MaximumCommandUnits&&Parse(maximum).packet->value.size()==Set::MaximumCommandUnits-8u,
        "Bounded maximum command/value was truncated or refused");
    Refuse([] {Parse(std::u16string(Set::MaximumCommandUnits+1u,u'X'));});
    Refuse([] {std::u16string input(Set::MaximumCommandUnits+1u,u'X');input[0]=0;Parse(input);});
}

void TokenBoundaries() {
    for(std::size_t length=0u;length<=600u;++length) {
        Operations plainOperations,quotedOperations;
        const auto value=std::u16string(length,u'A');
        const auto plain=Set::ParseToken256(value,0u,plainOperations);
        Require(plain.value==value.substr(0u,255u)&&plain.nextUnit==length&&plain.present==(length!=0u),
            "Unquoted ParseToken256 overflow did not consume the whole token");
        const auto quoted=Set::ParseToken256(u"\""+value+u"\" rest",0u,quotedOperations);
        Require(quoted.value==value.substr(0u,255u)&&quoted.nextUnit==(length<=255u?length+2u:256u)&&
            quoted.present==(length!=0u),"Quoted ParseToken256 overflow/closing-quote cursor changed");
        Require(plainOperations.admissions==1u&&quotedOperations.admissions==1u&&
            plainOperations.peak>=plain.value.capacity()*sizeof(char16_t),"Token allocation was not admitted");
    }
    for(const auto& [source,expected]:std::vector<std::pair<std::u16string,std::u16string>>{
        {u"\"a\\ b\"",u"a b"},{u"\"a\\\"b\"",u"a\"b"},{u"\"a\\\\b\"",u"a\\b"},
        {u"\"a\\tb\"",u"atb"},{u"a\\b",u"a\\b"},{u"a\"b",u"a\"b"},{u"\"a\tb\"",u"a\tb"}}) {
        Operations operations;const auto token=Set::ParseToken256(source,0u,operations);
        Require(token.value==expected,"ParseToken quote/backslash profile changed");
    }
    {Operations operations;const auto token=Set::ParseToken256(u"\"a\\\"b\"",0u,operations,0u,false);
        Require(token.value==u"a\\"&&token.nextUnit==4u,"UseEscape0 still consumed a quoted escape");}
    {Operations operations;const auto token=Set::ParseToken256(u" \tA B",0u,operations);
        Require(token.value==u"A"&&token.nextUnit==3u,"Token leading whitespace or unconsumed delimiter changed");}
    {Operations operations;std::u16string source=u"\""+std::u16string(254u,u'A');
        source.push_back(0xd800u);source.push_back(0xdc00u);source.push_back(u'"');
        const auto token=Set::ParseToken256(source,0u,operations);
        Require(token.value.size()==255u&&token.value.back()==0xd800u&&token.nextUnit==256u,
            "Native capacity truncation expanded/repaired a split UTF16 surrogate");}
    {Operations operations;const auto token=Set::ParseToken256(u"\""+std::u16string(255u,u'A')+u"\\",0u,operations);
        Require(token.value.size()==255u&&token.nextUnit==256u,"Quoted capacity check read a later terminal escape");}
    Refuse([] {Operations operations;Set::ParseToken256(u"\"a\\",0u,operations);});
    Refuse([] {Operations operations;Set::ParseToken256(u"A",2u,operations);});
    Refuse([] {Operations operations;Set::ParseToken256(u"A",0u,operations,std::numeric_limits<std::size_t>::max());});
}

void PhasedAndBudgets() {
    // These do NOT resolve Class/UProperty. An actual service can stop here
    // on a real lookup miss without parsing the later deliberately unsafe text.
    {Operations operations;const std::u16string command=u"SET Unknown \"T\\";
        const auto result=Set::Begin(command,operations);
        Require(result.recognized&&result.packet->className==u"Unknown"&&!result.packet->propertyParsed,
            "Class phase parsed a property before actual class lookup could decline it");
        Refuse([&] {auto packet=*result.packet;Set::ReadProperty(command,packet,operations);});}
    {Operations operations;const std::u16string command=u"SET InputExt Missing "+std::u16string(4000u,u'V');
        auto result=Set::Begin(command,operations);Set::ReadProperty(command,*result.packet,operations);
        Require(result.packet->propertyName==u"Missing"&&!result.packet->valueRead&&result.packet->value.empty()&&operations.peak<4096u,
            "Property phase allocated value before actual field lookup could decline it");}
    const std::u16string command=u"SET \"InputExt\" T   Talk with spaces";
    Operations complete;const auto full=Set::Parse(command,complete);
    Require(full.packet->value==u"Talk with spaces"&&complete.peak>0u,"Complete mock-budget packet is wrong");
    for(std::size_t limit=0u;limit<complete.work;++limit) {
        Operations bounded;bounded.workLimit=limit;
        Refuse([&] {Set::Parse(command,bounded);});
        Require(bounded.work==limit,"SET parser reset or exceeded caller's shared work budget");
    }
    Refuse([&] {Operations bounded;bounded.byteLimit=0u;Set::Parse(command,bounded);});
    {Operations bounded;bounded.byteLimit=0u;Require(!Set::Parse(u"SETX C P V",bounded).recognized&&bounded.admissions==0u,
        "An unrecognized SET command allocated a packet");}
    {Operations operations;auto result=Set::Begin(command,operations);
        Refuse([&] {Set::ReadValue(command,*result.packet,operations);});
        Set::ReadProperty(command,*result.packet,operations);
        Refuse([&] {Set::ReadProperty(command,*result.packet,operations);});
        Set::ReadValue(command,*result.packet,operations);
        Refuse([&] {Set::ReadValue(command,*result.packet,operations);});}
    // Admission/work failure in a later phase must not replace its existing
    // packet fields. The real caller remains responsible for UObject rollback.
    {Operations operations;auto result=Set::Begin(command,operations);const auto before=*result.packet;
        operations.byteLimit=0u;Refuse([&] {Set::ReadProperty(command,*result.packet,operations);});
        Require(result.packet->className==before.className&&result.packet->nextUnit==before.nextUnit&&
            !result.packet->propertyParsed&&result.packet->propertyName.empty(),"Failed property admission changed prior phase storage");}
}
} // namespace

int main() {
    try {
        Grammar();MissingAndCString();TokenBoundaries();PhasedAndBudgets();
        std::cout<<"Core SET packet parser tests passed: "<<checks<<" checks / "<<refusals<<
            " refusals; explicit mock shared budgets, no Class resolution, UObject mutation, FExec or persistence claim\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
