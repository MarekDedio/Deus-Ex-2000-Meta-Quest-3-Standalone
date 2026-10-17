#include "quest_native_config_import.h"

#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
namespace NI=QuestVr::NativeConfigImport;
using Units=std::u16string;
std::size_t checks{},refusals{};
void Require(bool v,const char* why) {++checks;if(!v)throw std::runtime_error(why);}
template<class F>void Refused(F&& fn,const char* why) {bool refused{};try{fn();}catch(const std::runtime_error&){refused=true;}Require(refused,why);++refusals;}
void Token(std::u16string_view input,bool dotted,std::u16string_view expected,std::size_t next,bool returned=true) {
    const auto actual=NI::ReadToken(input,dotted);Require(actual.result.returned==returned,"ReadToken return differs");
    if(returned)Require(actual.value==expected&&actual.result.nextUnit==next,"ReadToken units/cursor differ");
}
void Tokens() {
    Token(u"",false,u"",0u);Token(u"A_B-1.more",false,u"A_B-1",5u);Token(u"A_B-1.more!",true,u"A_B-1.more",10u);
    Token(u"_hello",true,u"_",0u);Token(u" hello",true,u" ",0u);Token(u"\thello",true,u"\t",0u);
    Token(u";hello",true,u";",0u);Token(u"9abc",true,u"9abc",4u);Token(u"-12",true,u"-",0u);
    Token(u"\u6c34abc",true,u"\u6c34",0u);Token(u"\"\"x",false,u"",2u);Token(u"\"a b\"x",false,u"a b",5u);
    Token(u"\"\\41\\e9\\ZZ\\\\\"tail",false,Units{u'A',u'\u00e9',u'\0',u'\\'},13u);
    Token(u"\"a\\q1b\"",false,Units{u'a',u'\1',u'b'},7u);
    for(const auto input:{u"\"unfinished",u"\"a\nb\"",u"\"a\rb\""})Token(input,true,u"",0u,false);
    const auto maximum=NI::TokenCapacity;
    Token(Units(maximum-2u,u'a'),true,Units(maximum-2u,u'a'),maximum-2u);
    Token(Units(maximum-1u,u'a'),true,u"",0u,false);Token(Units(maximum,u'a'),true,u"",0u,false);
    Token(u"\""+Units(maximum-2u,u'a')+u"\"",true,Units(maximum-2u,u'a'),maximum);
    Token(u"\""+Units(maximum-1u,u'a')+u"\"",true,u"",0u,false);
    Refused([&]{(void)NI::ReadToken(u"a",true,2u);},"Out-of-range token cursor accepted");
    Refused([&]{(void)NI::ReadToken(Units(NI::MaximumInputUnits+1u,u'a'));},"Token input bound ignored");
    Refused([&]{(void)NI::ReadToken(u"\"a\\");},"Quoted escape consumed missing native units");
    Refused([&]{(void)NI::ReadToken(u"\"a\\F");},"Incomplete quoted escape advanced past terminator");
    Token(std::u16string_view(u"abc\0def",7u),true,u"abc",3u);
    const auto offset=NI::ReadToken(u"X'Engine.Actor'",true,2u);Require(offset.result.returned&&offset.value==u"Engine.Actor"&&offset.result.nextUnit==14u,"Absolute token cursor differs");
}
void Integer(std::u16string_view text,std::int32_t expected,std::size_t next,std::int32_t seed=77) {
    auto value=seed;const auto result=NI::Integer(text,value);Require(result.returned&&result.nextUnit==next&&value==expected,"UInt ImportText write/cursor differs");
}
void Integers() {
    Integer(u"",77,0u);Integer(u" 12",77,0u);Integer(u"+12",77,0u);Integer(u"a12",77,0u);
    Integer(u"12x",12,2u);Integer(u"-12x",-12,3u);Integer(u"--12",0,4u);Integer(u"12-34x",12,5u);
    Integer(u"-",0,1u);Integer(u"-a",0,1u);Integer(u"-0",0,2u);Integer(u"0012",12,4u);
    Integer(u"2147483647",2147483647,10u);Integer(u"-2147483648",std::numeric_limits<std::int32_t>::min(),11u);
    for(const auto text:{u"2147483648",u"-2147483649",u"99999999999999999999"}) {
        std::int32_t value=123;Refused([&]{(void)NI::Integer(text,value);},"Unproven _wtoi overflow exported");Require(value==123,"Overflow refusal mutated destination");
    }
    for(std::int32_t value=-10000;value<=10000;++value) {
        const auto units=QuestVr::UnrealString::Decode(std::to_string(value));Integer(units+u"tail",value,units.size());
    }
}
void Booleans() {
    for(const auto mask:{0u,1u,2u,0x80000000u,0x5a5a5a5au})for(const auto seed:{0u,~0u,0xa5a5a5a5u}) {
        for(const auto& [text,wanted,next]:std::vector<std::tuple<Units,bool,std::size_t>>{
            {u"True",true,4u},{u"tRuE",true,4u},{u"1",true,1u},{u"False",false,5u},{u"FALSE",false,5u},
            {u"0",false,1u},{u"True.tail",true,4u},{u"\"True\"",true,6u},{u"\"\\54rue\"",true,8u}}) {
            auto value=seed;const auto result=NI::Boolean(text,mask,value);
            Require(result.returned&&result.nextUnit==next&&value==(wanted?seed|mask:seed&~mask),"UBool bitmask/token/cursor semantics differ");
        }
        for(const auto text:{u"",u" 1",u"yes",u"TRUE_",u"10",u"-1",u"\"True",u"\"\""}) {
            auto value=seed;const auto result=NI::Boolean(text,mask,value);Require(!result.returned&&value==seed,"Invalid Bool input changed destination");
        }
    }
}
struct Resolver {
    // Explicit independent registered object catalog; opaque tokens are not
    // permission for production code to fabricate UObjects from numeric IDs.
    struct Object {std::uintptr_t token,actualClass,base;std::u16string path;bool exactClass;};
    std::vector<Object> objects{{10u,1u,0u,u"Core.Class",true},{11u,1u,0u,u"Engine.Actor",true},
        {12u,1u,11u,u"DeusEx.Player",true},{13u,1u,0u,u"Engine.Texture",true},
        {14u,2u,11u,u"Foreign.ClassSubclass",false},{21u,11u,0u,u"Map.Actor0",false}};
    std::vector<std::pair<std::uintptr_t,Units>> calls;
    std::uintptr_t Find(std::uintptr_t constraint,std::u16string_view path) {
        calls.emplace_back(constraint,path);
        for(const auto& object:objects)if(QuestVr::UnrealString::EqualInsensitive(object.path,path)&&object.actualClass==constraint)return object.token;
        return 0u;
    }
    bool Exact(std::uintptr_t token) const {for(const auto& object:objects)if(object.token==token)return object.exactClass;throw std::runtime_error("Bad catalog token");}
    bool Derived(std::uintptr_t token,std::uintptr_t base) const {
        for(unsigned depth=0u;token&&depth<16u;++depth) {
            if(token==base)return true;
            const auto it=std::find_if(objects.begin(),objects.end(),[&](const auto& o){return o.token==token;});
            if(it==objects.end())return false;
            token=it->base;
        }return false;
    }
    NI::Result Import(std::u16string_view input,std::uintptr_t& value,std::uintptr_t objectClass=1u) {
        return NI::Class(input,objectClass,1u,11u,value,[&](auto cls,auto path){return Find(cls,path);},
            [&](auto token){return Exact(token);},[&](auto token,auto base){return Derived(token,base);});
    }
};
void Classes() {
    for(const auto text:{u"None",u"nOnE",u"None'junk",u"None ignored"}) {
        Resolver r;std::uintptr_t value=99u;const auto result=r.Import(text,value);
        Require(result.returned&&value==0u&&r.calls.empty()&&result.nextUnit==4u,"None did not clear before inspecting quoted suffix");
    }
    for(const auto& [text,expected,next]:std::vector<std::tuple<Units,std::uintptr_t,std::size_t>>{
        {u"DeusEx.Player",12u,14u},{u"Engine.Actor!",11u,13u},{u"Engine.Actor   ",11u,16u},
        {u"Class'Engine.Actor'",11u,19u},{u"\"Engine.Actor\"",11u,15u},{u"Engine.Texture",0u,15u}}) {
        Resolver r;r.objects.push_back({1u,1u,0u,u"Class",true});std::uintptr_t value=99u;
        const auto result=r.Import(text,value);Require(result.returned&&result.nextUnit==next&&value==expected,"Class import successful/constraint-failed cursor or write differs");
    }
    for(const auto text:{u"Missing.Class",u"",u" int",u"int",u"Class'Missing.Class'"}) {
        Resolver r;r.objects.push_back({1u,1u,0u,u"Class",true});std::uintptr_t value=99u;const auto result=r.Import(text,value);
        Require(!result.returned&&value==0u&&!r.calls.empty(),"Actual object lookup failure did not clear destination");
    }
    for(const auto text:{u"Missing'Engine.Actor'",u"Class'Engine.Actor",u"Class'",u"\"Engine.Actor"}) {
        Resolver r;std::uintptr_t value=99u;const auto result=r.Import(text,value);
        Require(!result.returned&&value==99u,"Parse/explicit-class failure changed prior destination");
    }
    Resolver ordered;ordered.objects.push_back({1u,1u,0u,u"Class",true});std::uintptr_t value=99u;
    const auto result=ordered.Import(u"Class'DeusEx.Player'",value);
    Require(result.returned&&value==12u&&ordered.calls==std::vector<std::pair<std::uintptr_t,Units>>{{1u,u"Class"},{1u,u"DeusEx.Player"}},
        "Explicit class lookup was reordered or final constraint used as lookup class");
    Resolver notClass;value=99u;const auto wrong=notClass.Import(u"Map.Actor0",value,11u);
    Require(wrong.returned&&value==0u,"Non-exact Class metaclass was retained");
    Resolver failure;value=99u;
    Refused([&]{(void)NI::Class(u"Engine.Actor",1u,1u,11u,value,[](auto,auto)->std::uintptr_t{throw std::runtime_error("resolver unavailable");},
        [](auto){return true;},[](auto,auto){return true;});},"Resolver failure silently became missing object");
    Require(value==99u,"Pre-write resolver refusal mutated destination");
    for(unsigned mode=0u;mode<2u;++mode) {
        value=99u;
        Refused([&]{(void)NI::Class(u"Engine.Actor",1u,1u,11u,value,[](auto,auto){return std::uintptr_t{11u};},
            [&](auto){if(mode==0u)throw std::runtime_error("metaclass unavailable");return true;},
            [&](auto,auto)->bool{throw std::runtime_error("ancestry unavailable");});},"Unsupported identity/ancestry became a successful None");
        Require(value==99u,"Post-lookup unsupported resolver refusal leaked a prefix write");
    }
}
}
int main() {
    try{Tokens();Integers();Booleans();Classes();std::cout<<"PASS native typed config imports "<<checks<<" checks, "<<refusals
        <<" refusals; original UTF16 token/cursor, integer/bitmask and class lookup/partial-write rules. No UObject resolver/default buffer/instance or gameplay capability.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"Native typed config import failed: "<<error.what()<<'\n';return 1;}
}
