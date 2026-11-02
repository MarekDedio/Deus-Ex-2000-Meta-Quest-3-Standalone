#include "quest_script_command_call.h"
#include <iostream>
#include <memory>
#include <vector>
#include <limits>

namespace {
std::size_t checks{},refusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F> void Refuse(F f){bool caught{};try{f();}catch(const std::exception&){caught=true;}Require(caught,"Expected command-tail refusal");++refusals;}
struct Ops {
    struct Cell {std::uint32_t flags{0x80u};bool object{},compatible{},fail{};std::u16string value;bool destroyed{};
        Cell(std::uint32_t f=0x80u,bool o=false,bool c=false,bool x=false):flags(f),object(o),compatible(c),fail(x){}
    };
    using Packet=std::shared_ptr<std::vector<Cell>>;
    std::vector<Cell> declaration;Packet last;bool executor{},event{},bad{},throwEvent{},invalidated{},invalidCursor{},throwDestroy{};
    std::size_t work{},limit{100000u},frame{},destroyed{},imports{},validates{};
    void Step(){if(++work>limit)throw std::runtime_error("work exhausted");}
    void EnterFrame(std::size_t n){frame+=n;}void LeaveFrame(std::size_t n)noexcept{frame-=n;}
    Packet ZeroPacket(){last=std::make_shared<std::vector<Cell>>(declaration);for(auto& c:*last)c.value.clear();return last;}
    void Validate(const Packet&){++validates;if(invalidated)throw std::runtime_error("owner revoked");}
    std::size_t FirstProperty(const Packet& p){return p->empty()?0u:1u;}
    std::size_t NextProperty(const Packet& p,std::size_t i){return i<p->size()?i+1u:0u;}
    std::uint32_t PropertyFlags(const Packet& p,std::size_t i){return p->at(i-1u).flags;}
    bool IsObjectProperty(const Packet& p,std::size_t i){return p->at(i-1u).object;}
    bool HasExecutor(){return executor;}bool ExecutorCompatible(const Packet& p,std::size_t i){return p->at(i-1u).compatible;}
    void InjectExecutor(const Packet& p,std::size_t i){p->at(i-1u).value=u"Executor";}
    std::optional<std::size_t> ImportText(const Packet& p,std::size_t i,std::u16string_view t,std::size_t c,std::uint32_t flags){
        Require(flags==1u,"Command import changed original flags");++imports;if(p->at(i-1u).fail)return {};
        if(invalidCursor)return t.size()+2u;
        auto end=c;while(end<t.size()&&t[end]!=u' '&&t[end]!=u'\r'&&t[end]!=u'\n')++end;
        p->at(i-1u).value=t.substr(c,end-c);return end;
    }
    void BadProperty(const Packet&,std::size_t){bad=true;}
    void ProcessEvent(const Packet&){event=true;if(throwEvent)throw std::runtime_error("event failure");}
    void BeginCleanup(const Packet&,bool)noexcept{}void EndCleanup(const Packet&)noexcept{}
    void DestroyValue(const Packet& p,std::size_t i,bool unwind){auto& cell=p->at(i-1u);if(cell.destroyed)return;
        if(!unwind)Step();
        if(throwDestroy&&!unwind)throw std::runtime_error("cleanup failed");
        cell.value.clear();cell.destroyed=true;++destroyed;}
    auto Call(std::u16string_view t=u"",std::size_t c=0u){return QuestVr::ScriptCommandCall::Call(*this,t,c);}
};
void Contracts(){
    {Ops o;auto r=o.Call();Require(r.eventOffered&&o.event&&!o.frame&&!r.imported&&!r.destroyed,"Zero-arg event tail differs");}
    for(auto flags:{0x80u,0x90u,0x400080u}){
        Ops o;o.declaration={{flags},{0x580u},{0x80u}};auto r=o.Call(u" ; comment\r\n \tvalue");
        Require(r.eventOffered&&r.imported==1u&&r.destroyed==1u&&o.destroyed==1u&&!o.last->at(2).destroyed&&!o.frame,
            "Leading input/return termination or ParseNext comments changed");}
    for(bool optional:{false,true}){Ops o;o.declaration={{optional?0x90u:0x80u,false,false,true},{0x80u}};
        const auto r=o.Call(u"bad rest");Require(r.requiredFailure!=optional&&r.optionalFailure==optional&&o.event==optional&&o.bad!=optional&&
            r.destroyed==2u&&!o.frame,"Required/optional failure did not clean all zero/input cells or changed handled event behavior");}
    for(bool compatible:{false,true}){Ops o;o.executor=true;o.declaration={{0x80u,true,compatible},{0x80u,true,true}};
        auto r=o.Call(u"word next");Require(r.executorInjected==compatible&&r.imported==(compatible?1u:2u)&&r.destroyed==2u,
            "Executor injection escaped first compatible Object parameter");}
    {Ops o;o.declaration={{},{}};o.throwEvent=true;Refuse([&]{o.Call(u"one two");});Require(o.destroyed==2u&&!o.frame,"Event failure lost selected cleanup");}
    {Ops o;o.declaration={{},{}};o.invalidCursor=true;Refuse([&]{o.Call(u"one");});Require(o.destroyed==2u&&!o.frame,"Bad import cursor skipped cleanup");}
    {Ops o;o.declaration={{},{}};o.throwDestroy=true;Refuse([&]{o.Call(u"one two");});Require(o.destroyed==2u&&!o.frame,"Cleanup failure did not finish retained cells on unwind");}
    {Ops o;o.declaration={{},{}};o.invalidated=true;Refuse([&]{o.Call(u"one two");});Require(o.destroyed==2u&&!o.frame,"Revoked owner prevented owned packet cleanup");}
    {Ops o;o.declaration={{}};Refuse([&]{o.Call(u"",1u);});Require(o.destroyed==1u&&!o.frame,"Beyond-terminator ParseNext skipped cleanup");}
    for(std::size_t cut=0u;cut<18u;++cut){Ops o;o.declaration={{},{}};o.limit=cut;
        bool failed{};try{o.Call(u" ;comment\r\n one two");}catch(const std::exception&){failed=true;}
        Require(!o.frame&&o.destroyed==2u,"Shared-work failure prevented owned cleanup/frame release");if(failed)++refusals;}
    for(char16_t ch:{u' ',u'\t',u'\r',u'\n'}){Ops o;o.declaration={{}};const std::u16string text{ch,ch,u'x'};
        const auto r=o.Call(text);Require(r.imported==1u&&r.cursor==3u,"ParseNext whitespace profile differs");}
}
}
int main(){try{Contracts();std::cout<<"PASS selected ScriptConsoleExec tail checks="<<checks<<" refusals="<<refusals
    <<"; selection/import/cleanup coordinator only, NOT owned native event or headset acceptance\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL ScriptConsoleExec tail: "<<e.what()<<'\n';return 1;}}
