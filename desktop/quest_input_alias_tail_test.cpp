#include "quest_input_alias_tail.h"
#include <array>
#include <functional>
#include <iostream>
#include <map>
#include <vector>

namespace {
std::size_t checks{},refusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class Callback>void Refuse(Callback callback){bool refused{};try{callback();}catch(const std::runtime_error&){refused=true;}Require(refused,"Expected alias-tail refusal");++refusals;}
struct Operations {
    using AliasName=int;
    bool& guard;std::size_t work{},workLimit{65536u},byteLimit{65536u},finds{},lookups{},peak{};
    std::array<int,40u> aliases{};std::map<std::u16string,int> names;
    std::vector<std::size_t> calls;std::function<void()> callback;
    void Step(){if(work==workLimit)throw std::runtime_error("Shared native work exhausted");++work;}
    void Admit(std::size_t bytes){if(bytes>byteLimit)throw std::runtime_error("Shared scratch exhausted");peak=std::max(peak,bytes);}
    bool& AliasGuard(){return guard;}
    int FindAliasName(std::u16string_view text){++finds;const auto found=names.find(std::u16string(text));return found==names.end()?0:found->second;}
    int AliasAt(std::size_t index){++lookups;if(index>=aliases.size())throw std::runtime_error("Alias fixed array escaped");return aliases[index];}
    void ExecInputCommands(std::size_t index){Require(guard,"Alias command callback ran without global recursion guard");calls.push_back(index);if(callback)callback();}
};
bool Exec(Operations& operations,std::u16string_view command){return QuestVr::NativeInputAliasTail::Exec(operations,command);}
Operations Make(bool& guard){Operations result{guard,0u,65536u,65536u,0u,0u,0u,{},{},{},{}};result.names={{u"Fire",1},{u"SET",2},{u"a\\b",3},{u"a\nb",4}};return result;}
}
int main(){try {
    bool guard{};
    for(std::size_t index=0;index<40u;++index){auto operations=Make(guard);operations.aliases[index]=1;
        Require(Exec(operations,u"Fire ignored arguments"),"Actual alias did not handle");
        Require(operations.calls==std::vector<std::size_t>{index}&&operations.lookups==index+1u&&!guard,"Alias ordering/guard differed");}
    for(const auto command:{u"Fire",u" \tFire",u"\"Fire\" tail",u"Fire\t",u"\"Fire"}){auto operations=Make(guard);operations.aliases[39]=1;Require(Exec(operations,command)&&operations.calls[0]==39u&&!guard,"Alias original token grammar changed");}
    {auto operations=Make(guard);operations.aliases[0]=1;operations.aliases[1]=1;Require(Exec(operations,u"Fire")&&operations.calls.size()==1u&&operations.calls[0]==0u,"Duplicate alias did not select first");}
    for(const auto command:{u"",u" \t",u"UnknownNeverIntern",u"None",u"SET",u"FireSuffix"}){auto operations=Make(guard);operations.aliases[0]=1;const auto names=operations.names;Require(!Exec(operations,command)&&operations.calls.empty()&&operations.names==names&&!guard,"Missing alias handled/interned/changed guard");}
    {auto operations=Make(guard);operations.aliases[0]=3;Require(Exec(operations,u"\"a\\b\""),"Alias token unexpectedly consumed backslash escape");}
    {auto operations=Make(guard);operations.aliases[0]=4;Require(Exec(operations,u"a\nb"),"Unquoted newline became a token separator");}
    {auto operations=Make(guard);operations.aliases[0]=1;Require(Exec(operations,std::u16string(u"Fire\0Other",10u)),"CString boundary changed alias");}
    {auto operations=Make(guard);const std::u16string longName(255u,u'x');operations.names[longName]=7;operations.aliases[0]=7;
        Require(Exec(operations,longName+std::u16string(100u,u'y')),"Unquoted native256 truncation did not retain prefix");
        Require(Exec(operations,u"\""+longName+u"remaining"),"Quoted native256 truncation failed");}
    {auto operations=Make(guard);operations.aliases[0]=1;bool otherGuardObserved{};
        operations.callback=[&]{auto other=Make(guard);other.aliases[0]=1;Require(!Exec(other,u"Fire")&&other.finds==0u&&other.lookups==0u,"Recursion guard did not precede token/alias reads");otherGuardObserved=guard;};
        Require(Exec(operations,u"Fire")&&otherGuardObserved&&!guard,"Cross-input guard lifetime changed");}
    {auto operations=Make(guard);operations.aliases[0]=1;operations.callback=[]{throw std::runtime_error("Native callback failed");};Refuse([&]{Exec(operations,u"Fire");});Require(!guard,"Exception retained alias guard");}
    {guard=true;auto operations=Make(guard);Require(!Exec(operations,u"Fire")&&guard&&operations.finds==0u,"Nested decline cleared outer guard");guard=false;}
    auto baseline=Make(guard);baseline.aliases[39]=1;Require(Exec(baseline,u"Fire"),"Budget baseline failed");
    for(std::size_t work=0;work<baseline.work;++work){auto operations=Make(guard);operations.aliases[39]=1;operations.workLimit=work;Refuse([&]{Exec(operations,u"Fire");});Require(!guard,"Work failure retained alias guard");}
    {auto operations=Make(guard);operations.byteLimit=0;Refuse([&]{Exec(operations,u"Fire");});Require(!guard&&operations.finds==0u,"Scratch failed after unauthenticated alias lookup");}
    std::cout<<"PASS original Input alias tail: "<<checks<<" checks / "<<refusals<<" refusals; actual40-slot order, ParseToken256 UseEscape0, no interning, shared recursion guard, void ExecInputCommands dispatch and callback/budget undo. Not complete Input/FExec routing.\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
