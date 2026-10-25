#include "quest_input_key_queries.h"

#include <array>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
namespace Query=QuestVr::NativeInputKeyQueries;
std::size_t checks{},refusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Refuse(F action){bool failed{};try{action();}catch(const std::runtime_error&){failed=true;}Require(failed,"Expected bounded Input refusal");++refusals;}
struct Operations {
    using Name=unsigned;
    std::vector<Name> names=std::vector<Name>(255u,0u);
    std::map<Name,std::u16string> spelling{{0u,u"None"},{1u,u"IK_A"},{2u,u"IK_F1"},{3u,u"xx"},{4u,u"ABC"},{5u,u"BAD_Prefix"}};
    std::array<std::u16string,255u> bindings{};
    std::u16string output;
    std::vector<std::string> calls;
    std::size_t work{},limit{10000u};
    std::function<void()> onText;
    void Step(){if(work>=limit)throw std::runtime_error("Mock shared Input work limit");++work;}
    std::size_t EnumSize(){calls.push_back("count");return names.size();}
    Name EnumName(std::size_t index){calls.push_back("name:"+std::to_string(index));return names.at(index);}
    std::u16string NameText(Name name){calls.push_back("text:"+std::to_string(name));if(onText)onText();return spelling.at(name);}
    Name FindName(std::u16string_view text){calls.push_back("find");
        for(const auto& [name,value]:spelling)if(QuestVr::UnrealString::EqualInsensitive(text,value))return name;
        return {};}
    std::u16string Binding(std::size_t index){calls.push_back("binding:"+std::to_string(index));return bindings.at(index);}
    void Log(std::u16string_view text){calls.push_back("log");output.append(text);}
};
Operations Fixture(){Operations result;result.names[65]=1u;result.names[112]=2u;result.bindings[65]=u"Jump";result.bindings[112]=u"BuySkills";return result;}
void Grammar(){
    struct Case {std::u16string text,token;bool matched;std::size_t cursor;};
    for(const auto& c:std::vector<Case>{{u" KEYNAME 1",u"KEYNAME",true,9},{u"\tKeYnAmE\t\t1",u"KEYNAME",true,10},
        {u"KEYNAME",u"KEYNAME",true,7},{u"KEYNAMEX 1",u"KEYNAME",false,0},{u"  KEYNAME1",u"KEYNAME",false,2},
        {u" KEYNAME_1",u"KEYNAME",true,8},{u"\nKEYNAME 1",u"KEYNAME",false,0},
        {u"  KE",u"KEYNAME",false,2},{u" KEYNAMEé",u"KEYNAME",true,8},{u" X",u"KEYNAME",false,1}}){
        std::size_t cursor{};Require(Query::ParseCommand(c.text,cursor,c.token)==c.matched&&cursor==c.cursor,"Literal original ParseCommand cursor changed");}
    for(unsigned suffix=0u;suffix<128u;++suffix){
        std::u16string input=u"\tKEYNAME";input.push_back(static_cast<char16_t>(suffix));input.push_back(u'!');std::size_t cursor{};
        const bool rejected=(suffix>='a'&&suffix<='z')||(suffix>='A'&&suffix<='Z')||(suffix>='0'&&suffix<='9');
        Require(Query::ParseCommand(input,cursor,u"KEYNAME")==!rejected,"ASCII command suffix classification changed");
    }
    Refuse([]{std::size_t cursor=4;Query::ParseCommand(u"a",cursor,u"KEYNAME");});
    Refuse([]{std::size_t cursor{};Query::ParseCommand(u"a",cursor,u"");});
    for(const auto& [text,value]:std::vector<std::pair<std::u16string,std::int32_t>>{{u"",0},{u"nonsense",0},{u"+",0},
        {u"\v\f\r\n\t +65tail",65},{u"-2147483648",std::numeric_limits<std::int32_t>::min()},
        {u"2147483647",2147483647},{u"--12",0},{u"12-34",12},{u"+00112 5",112},{u"\u00a065",0}})
        Require(Query::AppAtoi(text)==value,"Literal _wtoi input result changed");
    for(int integer=-10000;integer<=10000;++integer)
        Require(Query::AppAtoi(QuestVr::UnrealString::Decode(std::to_string(integer)))==integer,"Decimal signed32 roundtrip changed");
    Refuse([]{Query::AppAtoi(u"2147483648");});Refuse([]{Query::AppAtoi(u"-2147483649");});
    Refuse([]{Query::AppAtoi(std::u16string(16385u,u'0'));});
}
void Queries(){
    for(const auto& [command,expected,handled]:std::vector<std::tuple<std::u16string,std::u16string,bool>>{
        {u"KEYNAME 65",u"A",true},{u"KEYNAME 112",u"F1",true},{u"KEYNAME 0",u"e",true},
        {u"KEYNAME -1",u"",true},{u"KEYNAME 255",u"",true},{u"KEYNAME 2147483647",u"",true},
        {u"KEYBINDING A",u"Jump",true},{u"keybinding f1",u"BuySkills",true},{u"KEYBINDING Unknown",u"",true},
        {u"KEYBINDING A ",u"",true},{u"KEYBINDING A extra",u"",true},{u"KEYBINDING",u"",true},
        {u"KEYNAMEX 65",u"",false},{u"KEYBINDING1 A",u"",false},{u"BUTTON Fire",u"",false}}){
        auto operations=Fixture();const auto before=operations.spelling;
        Require(Query::Exec(operations,command)==handled&&operations.output==expected,"Input key-handler output/handled result changed");
        Require(operations.spelling==before,"Input FNAME_Find interned an unknown spelling");
    }
    {auto operations=Fixture();Require(Query::Exec(operations,u"KEYBINDING Unknown")&&operations.calls==
        std::vector<std::string>{"find"},"Unknown FNAME_Find result traversed Enum or logged output");}
    {auto operations=Fixture();operations.names[65]=5u;Require(Query::GetKeyName(operations,65)==u"_Prefix","GetKeyName validated an invented IK_ prefix");}
    for(unsigned name:{3u,4u}){auto operations=Fixture();operations.names[65]=name;Require(Query::GetKeyName(operations,65).empty(),"Short enum name did not return empty");}
    {auto operations=Fixture();operations.names[4]=1u;operations.bindings[4]=u"First duplicate";
        Require(Query::Exec(operations,u"KEYBINDING A")&&operations.output==u"First duplicate","FindKeyName did not return first actual FName match");}
    {auto operations=Fixture();operations.spelling[6]=u"IK_External";
        Require(Query::Exec(operations,u"KEYBINDING External")&&operations.output.empty(),"Known FName outside Enum fabricated binding");}
    {auto operations=Fixture();operations.spelling[1]=u"IK_"+std::u16string(1u,0xd800u);
        Require(Query::GetKeyName(operations,65)==std::u16string(1u,0xd800u),"GetKeyName lost UTF16 surrogate unit");}
    {auto operations=Fixture();std::u16string text=u"KEYNAME 65";text.push_back(0);text.append(u"garbage");
        Require(Query::Exec(operations,text)&&operations.output==u"A","Input ignored original CString boundary");}
    {auto operations=Fixture();operations.bindings[65]=std::u16string{u'J',0,u'X'};
        Query::Exec(operations,u"KEYBINDING A");Query::Exec(operations,u"KEYNAME 112");
        Require(operations.output==u"JF1","FString sink added newline or copied beyond NUL");}
    {auto operations=Fixture();Require(Query::GetKeyName(operations,65)==u"A"&&operations.calls==std::vector<std::string>{
        "count","name:65","text:1","name:65","text:1"},"GetKeyName omitted live Enum/name reread");}
    {auto operations=Fixture();unsigned reads{};operations.onText=[&]{if(++reads==1u)operations.names[65]=2u;};
        Require(Query::GetKeyName(operations,65)==u"F1","GetKeyName cached first Enum identity instead of rereading");}
    {auto operations=Fixture();operations.names.resize(2u);Refuse([&]{Query::GetKeyName(operations,65);});}
    {auto operations=Fixture();Refuse([&]{Query::FindKeyName(operations,std::u16string(253u,u'X'));});}
    {auto operations=Fixture();Query::FindKeyName(operations,std::u16string(252u,u'X'));
        Require(operations.spelling.size()==6u,"Max format boundary interned a name");}
    {auto operations=Fixture();operations.onText=[]{throw std::runtime_error("Actual provider failed");};
        Refuse([&]{Query::Exec(operations,u"KEYNAME 65");});Require(operations.output.empty(),"Provider failure emitted a fabricated result");}
    {auto warm=Fixture();Query::Exec(warm,u"KEYBINDING F1");
        for(std::size_t cut=0;cut<warm.work;++cut){auto operations=Fixture();operations.limit=cut;
            Refuse([&]{Query::Exec(operations,u"KEYBINDING F1");});Require(operations.work==cut&&operations.output.empty(),"Shared key query work cut reset or emitted output");}}
}
}
int main(){try{Grammar();Queries();std::cout<<"PASS Input key query protocol: "<<checks<<" checks / "<<refusals
    <<" refusals; explicit mock name/enum/binding/output authorities, not native Input initialization or live startup\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL Input key query protocol: "<<error.what()<<'\n';return 1;}}
