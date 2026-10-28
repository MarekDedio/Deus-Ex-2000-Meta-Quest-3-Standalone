#include "quest_process_console_exec.h"
#include "quest_viewport_exec_commands.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

namespace {
std::size_t checks{},refusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class Callback>void Refuse(Callback callback) {
    bool threw{};try{callback();}catch(const std::exception&){threw=true;}
    Require(threw,"Missing shared budget/callback refusal");++refusals;
}
struct Function {bool actual{true};std::uint32_t flags{0x200u};int identity{};};
struct Operations {
    using Name=int;
    bool scriptable{true},throwMatch{},sawGlobal{};
    std::map<std::u16string,int> names{{u"Exec",1},{u"Ordinary",2},{u"WrongClass",3},{u"Missing",4}};
    Function state{true,0x200u,1},ordinary{true,0u,2},wrong{false,0x200u,3};
    std::size_t work{},maximumWork{10000u},scratch{},maximumScratch{10000u},nameReads{},functionReads{},matches{},next{};
    Function* selected{};
    void Step(){if(work==maximumWork)throw std::runtime_error("work");++work;}
    void Admit(std::size_t bytes){scratch=std::max(scratch,bytes);if(bytes>maximumScratch)throw std::runtime_error("scratch");}
    bool IsScriptable(){return scriptable;}
    Name FindName(std::u16string_view text){++nameReads;const auto found=names.find(std::u16string(text));return found==names.end()?0:found->second;}
    Function* FindFunction(Name name,bool global){++functionReads;sawGlobal=global;return name==1?&state:name==2?&ordinary:name==3?&wrong:nullptr;}
    bool IsFunction(Function* function){return function->actual;}
    std::uint32_t FunctionFlags(Function* function){return function->flags;}
    void Match(Function* function,std::size_t cursor){++matches;selected=function;next=cursor;if(throwMatch)throw std::runtime_error("callback");}
};
struct ViewportOperations {
    std::size_t work{},maximumWork{100u},matches{},next{};std::u16string selected;
    void Step(){if(work==maximumWork)throw std::runtime_error("work");++work;}
    void Match(std::u16string_view command,std::size_t cursor){++matches;selected=command;next=cursor;}
};
void Selection() {
    Operations disabled;disabled.scriptable=false;disabled.maximumScratch=0u;
    Require(!QuestVr::ProcessConsoleExec::Exec(disabled,std::u16string(20000u,u'x'))&&
        disabled.work==1u&&!disabled.scratch&&!disabled.nameReads&&!disabled.functionReads,"Scriptability gate parsed input");
    for(const auto* command:{u"",u" \t",u"\"\"",u"Unknown",u"None",u"Missing",u"Ordinary",u"WrongClass"}) {
        Operations operations;const auto names=operations.names;
        Require(!QuestVr::ProcessConsoleExec::Exec(operations,command)&&!operations.matches&&operations.names==names,
            "Missing/non-Function/non-Exec did not decline without interning");
    }
    Operations successful;
    Require(QuestVr::ProcessConsoleExec::Exec(successful,u" \t\"Exec\"  remaining")&&successful.matches==1u&&
        successful.selected==&successful.state&&!successful.sawGlobal&&successful.next==8u,"Original token/Global0/state selection changed");
    Operations quoted;Require(QuestVr::ProcessConsoleExec::Exec(quoted,u"\"Ex\\ec\" tail")&&quoted.next==7u,"UseEscape1 was ignored");
    Operations unicode;unicode.names[u"Łódź"]=1;
    Require(QuestVr::ProcessConsoleExec::Exec(unicode,u"Łódź rest"),"UTF16 name lookup changed");
    Operations nul;const std::u16string supplied{u'E',u'x',u'e',u'c',0,u'x'};
    Require(QuestVr::ProcessConsoleExec::Exec(nul,supplied)&&nul.next==4u,"Native CString boundary changed");
    Operations nonExecState;nonExecState.state.flags=0u;
    Require(!QuestVr::ProcessConsoleExec::Exec(nonExecState,u"Exec")&&!nonExecState.matches,"State non-Exec fell back to another function");
    Operations throwing;throwing.throwMatch=true;Refuse([&]{static_cast<void>(QuestVr::ProcessConsoleExec::Exec(throwing,u"Exec"));});
    Require(throwing.matches==1u,"Matching command callback was skipped");
    for(std::size_t budget=0u;budget<successful.work;++budget) {
        Operations cut;cut.maximumWork=budget;Refuse([&]{static_cast<void>(QuestVr::ProcessConsoleExec::Exec(cut,u" \t\"Exec\"  remaining"));});
    }
    Operations noScratch;noScratch.maximumScratch=0u;Refuse([&]{static_cast<void>(QuestVr::ProcessConsoleExec::Exec(noScratch,u"Exec"));});
    Operations tooLong;Refuse([&]{static_cast<void>(QuestVr::ProcessConsoleExec::Exec(tooLong,std::u16string(16385u,u'x')));});
    Operations trailingEscape;Refuse([&]{static_cast<void>(QuestVr::ProcessConsoleExec::Exec(trailingEscape,u"\"Exec\\"));});
}
void Tokens() {
    for(const auto size:{62u,63u,64u,255u})for(const bool quoted:{false,true}) {
        Operations operations;const auto token=std::u16string(size,u'x');
        operations.names[std::u16string(std::min(size,63u),u'x')]=1;
        const auto text=(quoted?u"\""+token+u"\"":token)+u" remaining";
        Require(QuestVr::ProcessConsoleExec::Exec(operations,text),"64-unit native token did not resolve");
        const auto expected=quoted?(size<=63u?size+2u:64u):size;
        Require(operations.next==expected,"64-unit quoted/unquoted overflow cursor changed");
    }
    Operations unterminated;Require(QuestVr::ProcessConsoleExec::Exec(unterminated,u"\"Exec"),"Missing closing quote changed token semantics");
    Operations newline;newline.names[u"Exec\nother"]=1;
    Require(QuestVr::ProcessConsoleExec::Exec(newline,u"Exec\nother"),"ParseToken invented newline whitespace");
}
void Viewport() {
    for(const auto command:QuestVr::ViewportExecCommands::Commands) {
        ViewportOperations operations;std::size_t cursor{};
        Require(QuestVr::ViewportExecCommands::Exec(operations,u" \t"+std::u16string(command)+u" arg",cursor)&&
            operations.matches==1u&&operations.selected==command&&cursor==command.size()+3u,"Viewport native command branch missing");
        ViewportOperations suffix;cursor=0u;
        Require(!QuestVr::ViewportExecCommands::Exec(suffix,std::u16string(command)+u"X",cursor)&&!suffix.matches,
            "Viewport accepted alphabetic command suffix");
        ViewportOperations underscore;cursor=0u;
        Require(QuestVr::ViewportExecCommands::Exec(underscore,std::u16string(command)+u"_",cursor),"Viewport invented underscore suffix rejection");
    }
    ViewportOperations decline;std::size_t cursor{};
    Require(!QuestVr::ViewportExecCommands::Exec(decline,u" \tSET InputExt T Talk",cursor)&&cursor==2u&&!decline.matches,
        "Viewport missed command altered the advancing native pointer");
    for(std::size_t work=0u;work<11u;++work) {
        ViewportOperations cut;cut.maximumWork=work;cursor=0u;
        Refuse([&]{static_cast<void>(QuestVr::ViewportExecCommands::Exec(cut,u"SET InputExt T Talk",cursor));});
    }
}
}
int main(){try{Selection();Tokens();Viewport();std::cout<<"PASS original viewport/ScriptConsoleExec prefix: "<<checks
    <<" checks / "<<refusals<<" refusals; eleven native branches,64-unit UseEscape1 tokens,scriptability/name/Function/Exec selection and shared budgets. Not full parameter execution or UPlayer routing.\n";
    return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
