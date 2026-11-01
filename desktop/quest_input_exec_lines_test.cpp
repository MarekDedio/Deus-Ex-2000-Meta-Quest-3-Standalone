#include "quest_input_exec_lines.h"
#include "quest_input_alias_tail.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

namespace {
using Id=QuestVr::OriginalInputExecIdentity;
constexpr Id Input=11u,Viewport=22u,OtherViewport=23u,Output=33u;
std::size_t checks{},refusals{};
void Require(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
template<class F> void Refuse(F&& f,const char* contains="") {
    bool failed{};try{f();}catch(const std::exception& error){failed=std::string(error.what()).find(contains)!=std::string::npos;}
    Require(failed,"Expected original Input command refusal");++refusals;
}
struct Parsed {bool returned{};std::u16string text;std::size_t cursor{};};
// Independent literal instruction oracle. Models integer registers and the
// exact0 tail, not a call back to the production helper/tokenizer.
Parsed Reference(std::u16string_view source,std::size_t start=0u) {
    const auto at=[&](std::size_t i){return i<source.size()?source[i]:char16_t{};};
    Parsed r;r.cursor=start;int capacity=256;unsigned quoted{},ignored{},scanned{};
    while(at(r.cursor)!=0) {
        const auto ax=at(r.cursor);
        if(ax==10||ax==13)break;
        --capacity;if(capacity<=0)break;
        if(quoted==0u) {
            if(ax==47&&at(r.cursor+1u)==47)ignored=1u;
            if(ax==124)break;
        }
        quoted^=ax==34?1u:0u;scanned=1u;
        if(ignored==0u)r.text.push_back(ax);
        ++r.cursor;
    }
    while(at(r.cursor)==10||at(r.cursor)==13||at(r.cursor)==124)++r.cursor;
    r.returned=at(r.cursor)!=0||scanned!=0u;return r;
}
Parsed Parse(std::u16string_view source,std::size_t start=0u) {
    QuestVr::OriginalInputLine256 line;line.storage.fill(char16_t{0x5555});line.length=99u;
    std::size_t work{},cursor=start;
    const bool returned=QuestVr::ParseOriginalInputLine256(source,cursor,line,[&]{++work;});
    Require(work>0u&&line.length<=255u&&line.storage[line.length]==0,"Parser omitted work/terminator or exceeded buffer");
    return {returned,std::u16string(line.Text()),cursor};
}
void Compare(std::u16string_view source,std::size_t start=0u) {
    const auto a=Parse(source,start),b=Reference(source,start);
    Require(a.returned==b.returned&&a.text==b.text&&a.cursor==b.cursor,"ParseLine differs from independent instruction oracle");
}
void ExamplesAndGenerated() {
    const auto example=[&](std::u16string_view source,std::u16string_view expected,std::size_t cursor,bool result=true){
        const auto r=Parse(source);Require(r.returned==result&&r.text==expected&&r.cursor==cursor,"Original ParseLine literal example differs");};
    example(u"",u"",0u,false);example(u"A|B",u"A",2u);example(u"|B",u"",1u);
    example(u"\r\n||",u"",4u,false);example(u"//comment",u"",9u);
    example(u"A//comment|B",u"A",11u);example(u"\"A|B\"",u"\"A|B\"",5u);
    example(u"\"A\nB",u"\"A",3u);example(u"A//\"|B\"|C",u"A",8u);
    example(u"A\\\"|B",u"A\\\"|B",5u);
    const std::u16string nul{u'A',0,u'|',u'B'};example(nul,u"A",1u);
    constexpr std::array<char16_t,10u> alphabet{u'A',u'/',u'|',u'"',u'\\',u'\r',u'\n',0,u'\t',char16_t{0x263a}};
    std::u16string source;
    const auto visit=[&](const auto& self,unsigned remaining)->void {
        Compare(source);
        if(!remaining)return;
        for(const auto c:alphabet){source.push_back(c);self(self,remaining-1u);source.pop_back();}
    };
    visit(visit,5u);
    for(const auto count:{0u,1u,253u,254u,255u,256u,257u,510u,511u,512u,1024u,16384u}) {
        for(const std::u16string prefix:{u"",u"//",u"\"",u"A//\"",u"\\\""}) {
            if(count+prefix.size()>16384u)continue;
            const auto text=prefix+std::u16string(count,u'x');std::size_t offset{};
            for(std::size_t n=0u;n<=text.size()/255u+2u;++n) {
                Compare(text,offset);const auto r=Parse(text,offset);
                if(!r.returned)break;
                Require(r.cursor>offset,"Capacity parse failed to advance its stream");offset=r.cursor;
            }
        }
    }
    auto longComment=std::u16string(u"//")+std::u16string(254u,u'x');
    const auto first=Parse(longComment),second=Parse(longComment,first.cursor);
    Require(first.text.empty()&&first.cursor==255u&&second.text==u"x","Comment suffix was skipped or kept ignored after fresh capacity chunk");
    auto quoted=std::u16string(u"\"")+std::u16string(254u,u'x')+u"|Y";
    const auto q1=Parse(quoted),q2=Parse(quoted,q1.cursor);
    Require(q1.cursor==256u&&q1.text.size()==255u&&q2.text==u"Y","Capacity tail did not skip pipe independent of prior quote state");
}
struct Event {Id owner,output;std::u16string line;bool viewport;};
struct Fixture {
    bool running{true},root{true};std::int32_t action{1};Id viewport{Viewport};
    std::size_t work{},cut{std::numeric_limits<std::size_t>::max()},runningReads{},actionReads{},viewportReads{};
    std::size_t scratch{},peak{},scratchLimit{16384u},leaves{};std::vector<Event> events;
    std::function<void(Fixture&)> child;
    QuestVr::OriginalInputExecQueries Queries() {
        QuestVr::OriginalInputExecQueries q;
        q.work=[this]{if(work++>=cut)throw std::runtime_error("borrowed work exhausted");};
        q.requireOuterRoot=[this](Id input,Id output){Require(input==Input&&output==Output,"Root changed native input/output");if(!root)throw std::runtime_error("outer root missing");};
        q.enterScratch=[this](std::size_t bytes){if(bytes>scratchLimit-scratch)throw std::runtime_error("scratch exhausted");scratch+=bytes;peak=std::max(peak,scratch);};
        q.leaveScratch=[this](std::size_t bytes)noexcept{if(bytes>scratch)std::terminate();scratch-=bytes;++leaves;};
        q.running=[this]{++runningReads;return running;};
        q.action=[this](Id input){Require(input==Input,"Action read substituted native Input");++actionReads;return action;};
        q.inputViewport=[this](Id input){Require(input==Input,"Viewport read substituted native Input");++viewportReads;return viewport;};
        q.viewportExec=[this](Id owner,Id output,std::u16string_view line){events.push_back({owner,output,std::u16string(line),true});if(child)child(*this);return false;};
        q.inputExec=[this](Id owner,Id output,std::u16string_view line){events.push_back({owner,output,std::u16string(line),false});if(child)child(*this);return false;};
        return q;
    }
    void Exec(std::u16string_view source){QuestVr::ExecuteOriginalInputCommands(Input,Output,source,Queries());}
};
void RoutingAndLiveReads() {
    const std::u16string source=u"  FIRST|\tOnRelease  SECOND|//empty| onrelease_X|OnReleaseX|OnRelease";
    for(const auto action:{-1,0,1,2,3,4,std::numeric_limits<int>::max()}) {
        Fixture f;f.action=action;f.Exec(source);
        Require(f.runningReads==1u&&f.actionReads==6u&&f.events.size()==6u&&f.scratch==0u&&f.leaves==1u,"Exec command scan/live Action/frame count differs");
        const std::array<std::u16string,6u> release{u"FIRST",u"SECOND",u"",u"_X",u"OnReleaseX",u""};
        const std::array<std::u16string,6u> ordinary{u"  FIRST",u"\tOnRelease  SECOND",u"",u" onrelease_X",u"OnReleaseX",u"OnRelease"};
        for(std::size_t n=0u;n<f.events.size();++n) {
            const bool viewport=action==1||(action==3&&(n==1u||n==3u||n==5u));
            Require(f.events[n].viewport==viewport&&f.events[n].owner==(viewport?Viewport:Input)&&f.events[n].output==Output&&
                f.events[n].line==(action==3?release[n]:ordinary[n]),"Action/OnRelease/empty command routing differs");
        }
    }
    Fixture live;
    live.child=[](Fixture& f){if(f.events.size()==1u){f.running=false;f.action=3;f.viewport=OtherViewport;}else if(f.events.size()==2u)f.action=4;};
    live.Exec(u"FIRST|OnRelease SECOND|  THIRD");
    Require(live.runningReads==1u&&live.events.size()==3u&&live.events[1].owner==OtherViewport&&
        live.events[1].line==u"SECOND"&&!live.events[2].viewport&&live.events[2].line==u"  THIRD",
        "Running was reread or Action/Viewport was captured before a child");
    std::u16string mutableSource=u"FIRST|SECOND";Fixture mutation;
    mutation.child=[&](Fixture& f){if(f.events.size()==1u)mutableSource[6u]=u'X';};mutation.Exec(mutableSource);
    Require(mutation.events[1].line==u"XECOND","Coordinator eagerly split/copied the caller-owned live command list");
    Fixture nested;bool active{};
    nested.child=[&](Fixture& f){if(!active){active=true;f.Exec(u"INNER");active=false;}};nested.Exec(u"OUTER");
    Require(nested.events.size()==2u&&nested.runningReads==2u&&nested.peak==2u*QuestVr::InputExecLinesDetail::Frame::Bytes&&nested.scratch==0u,
        "Nested invocation did not retain both lexical frames/shared owner budget");
    Fixture off;off.running=false;auto missing=off.Queries();missing.action={};missing.inputExec={};missing.viewportExec={};
    QuestVr::ExecuteOriginalInputCommands(Input,Output,u"SHOULD NOT PARSE",missing);
    Require(off.runningReads==1u&&!off.actionReads&&off.events.empty()&&off.scratch==0u,"Genuine false running selected later providers");
}
void RefusalsAndPrefixes() {
    Fixture base;base.Exec(u"A|OnRelease B|C");const auto total=base.work;
    for(std::size_t cut=0u;cut<total;++cut) {
        Fixture f;f.cut=cut;Refuse([&]{f.Exec(u"A|OnRelease B|C");},"borrowed work");
        Require(f.scratch==0u&&f.events.size()<=base.events.size(),"Failed work prefix leaked lexical scratch or invented dispatch");
        for(std::size_t n=0u;n<f.events.size();++n)Require(f.events[n].line==base.events[n].line,"Failed work prefix changed earlier original dispatch");
    }
    Fixture scratch;scratch.scratchLimit=QuestVr::InputExecLinesDetail::Frame::Bytes-1u;
    Refuse([&]{scratch.Exec(u"A");},"scratch");Require(scratch.scratch==0u&&!scratch.runningReads,"Scratch refusal selected running or released unowned frame");
    Fixture root;root.root=false;Refuse([&]{root.Exec(u"A");},"outer root");Require(root.peak==0u,"Missing root admitted scratch");
    Fixture late;late.child=[](Fixture&){throw std::runtime_error("actual child failure");};Refuse([&]{late.Exec(u"A|B");},"actual child");
    Require(late.events.size()==1u&&late.scratch==0u,"Child failure was swallowed or later line dispatched");
    Fixture null;null.viewport=0u;Refuse([&]{null.Exec(u"A");},"null native Viewport");Require(null.events.empty()&&null.scratch==0u,"Null viewport fabricated dispatch");
    for(unsigned selected=0;selected<5u;++selected) {
        Fixture f;auto q=f.Queries();
        if(selected==0u)q.running={};else if(selected==1u)q.action={};else if(selected==2u)q.inputViewport={};
        else if(selected==3u)q.viewportExec={};else{f.action=2;q.inputExec={};}
        Refuse([&]{QuestVr::ExecuteOriginalInputCommands(Input,Output,u"A",q);},"selected provider");
        Require(f.events.empty()&&f.scratch==0u,"Missing selected provider became success or leaked scratch");
    }
    Fixture f;auto q=f.Queries();q.leaveScratch={};Refuse([&]{QuestVr::ExecuteOriginalInputCommands(Input,Output,u"A",q);},"cleanup");
    q=f.Queries();q.work={};Refuse([&]{QuestVr::ExecuteOriginalInputCommands(Input,Output,u"A",q);},"caller work");
    Refuse([&]{QuestVr::ExecuteOriginalInputCommands(0u,Output,u"A",f.Queries());},"identities");
    Refuse([&]{QuestVr::ExecuteOriginalInputCommands(Input,0u,u"A",f.Queries());},"identities");
    QuestVr::OriginalInputLine256 line;std::size_t cursor{};const auto work=[]{};
    Refuse([&]{QuestVr::ParseOriginalInputLine256(std::u16string(16385u,u'x'),cursor,line,work);},"admitted storage");
    cursor=2u;Refuse([&]{QuestVr::ParseOriginalInputLine256(u"x",cursor,line,work);},"admitted storage");
    cursor=0u;Refuse([&]{QuestVr::ParseOriginalInputLine256(u"x",cursor,line,{});},"caller work");
}
}
int main(){try {
    ExamplesAndGenerated();RoutingAndLiveReads();RefusalsAndPrefixes();
    std::cout<<"PASS original ParseLine256/ExecInputCommands checks="<<checks<<" refusals="<<refusals
        <<"; generated parser/routing fixtures only, NOT runtime GIsRunning/FExec/Quest input/startup/headset proof\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL original input command lines: "<<error.what()<<'\n';return 1;}}
