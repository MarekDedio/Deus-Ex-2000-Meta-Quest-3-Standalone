#include "quest_input_button_commands.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace {
using Cell=QuestVr::OriginalInputButtonCell;
using Queries=QuestVr::OriginalInputButtonQueries;
std::size_t checks{},refusals{};
void Check(bool good,const char* why) {++checks;if(!good)throw std::runtime_error(why);}
template<class Run> void Refuse(Run&& run,const char* expected="") {
    bool refused{};try {run();}catch(const std::exception& error){refused=std::string(error.what()).find(expected)!=std::string::npos;}
    Check(refused,"Expected selected original Input action refusal");++refusals;
}
struct Harness {
    static constexpr std::uintptr_t A=10u,B=20u,Fire=100u,Other=200u;
    std::uintptr_t actor{A};std::int32_t action{1};bool rooted{true};
    std::map<std::pair<std::uintptr_t,std::uintptr_t>,std::uint8_t> bytes{{{A,Fire},0u},{{B,Other},0u}};
    std::vector<std::string> events;std::vector<std::u16string> tokens,logs,axisText;
    std::vector<std::uintptr_t> selectedActors;
    std::vector<std::tuple<Cell,std::uint8_t>> writes;
    std::size_t work{},maximumWork{100000u},scratch{},peak{},maximumScratch{100000u},enters{},leaves{},reads{},actorReads{};
    std::size_t axisCursor{},axisCalls{};
    std::function<void()> afterActor,afterFind,afterWrite,workHook;
    std::function<Cell(std::uintptr_t,std::u16string_view)> finder;
    void Step(){if(workHook)workHook();if(work++>=maximumWork)throw std::runtime_error("shared work exhausted");}
    void Enter(std::size_t value) {
        if(value>maximumScratch-scratch)throw std::runtime_error("scratch admission refused");
        scratch+=value;peak=std::max(peak,scratch);++enters;
    }
    void Leave(std::size_t value)noexcept {if(value>scratch)std::terminate();scratch-=value;++leaves;}
    Queries Bind() {
        Queries q;
        q.requireOuterRoot=[this]{events.emplace_back("root");if(!rooted)throw std::runtime_error("missing root");};
        q.work=[this]{Step();};q.enterScratch=[this](std::size_t n){Enter(n);};
        q.leaveScratch=[this](std::size_t n)noexcept{Leave(n);};
        q.viewportActor=[this]{events.emplace_back("actor");++actorReads;const auto value=actor;if(afterActor)afterActor();return value;};
        q.findButton=[this](std::uintptr_t selected,std::u16string_view token) {
            events.emplace_back("find");selectedActors.push_back(selected);tokens.emplace_back(token);
            const auto result=finder?finder(selected,token):token==u"bFire"?Cell{selected,selected==A?Fire:Other}:Cell{};
            if(afterFind)afterFind();
            return result;
        };
        q.action=[this]{events.emplace_back("action");return action;};
        q.readByte=[this](Cell cell){events.emplace_back("read");++reads;return bytes.at({cell.actor,cell.property});};
        q.writeByte=[this](Cell cell,std::uint8_t value){events.emplace_back("write");bytes.at({cell.actor,cell.property})=value;
            writes.emplace_back(cell,value);if(afterWrite)afterWrite();};
        q.log=[this](std::u16string_view text){events.emplace_back("log");logs.emplace_back(text);};
        q.axis=[this](std::u16string_view text,std::size_t cursor){events.emplace_back("axis");++axisCalls;
            axisText.emplace_back(text);axisCursor=cursor;};
        return q;
    }
    bool Exec(std::u16string_view command){return QuestVr::ExecuteOriginalInputButtonCommands(command,Bind());}
    std::uint8_t Value()const{return bytes.at({A,Fire});}
};
void ByteTransitions() {
    for(const auto* command:{u"BUTTON bFire",u"PULSE bFire",u"TOGGLE bFire"})
        for(const auto action:{std::int32_t{-1},0,1,2,3,4,std::numeric_limits<std::int32_t>::max()})
            for(unsigned byte=0u;byte<256u;++byte) {
                Harness h;h.action=action;h.bytes[{Harness::A,Harness::Fire}]=static_cast<std::uint8_t>(byte);
                Check(h.Exec(command),"Recognized button action declined");
                const bool button=std::u16string_view{command}.starts_with(u"BUTTON");
                const bool toggle=std::u16string_view{command}.starts_with(u"TOGGLE");
                const bool wrote=action==1||(button&&action==3&&byte!=0u);
                const auto expected=action==1?(toggle?byte^0x80u:1u):button&&action==3?0u:byte;
                Check(h.Value()==expected&&h.writes.size()==static_cast<std::size_t>(wrote),"Exact byte value/selective write differs");
                const bool read=(button&&action==3)||(toggle&&action==1);
                Check(h.reads==static_cast<std::size_t>(read)&&h.logs.empty()&&h.actorReads==2u&&h.tokens==std::vector<std::u16string>{u"bFire"},
                    "Button command read/order/error profile differs");
                Check(h.scratch==0u&&h.peak>=QuestVr::InputButtonCommandsDetail::FrameBytes&&h.leaves==1u,
                    "Button command lexical scratch was not released");
            }
}
void RecognitionAndTokens() {
    for(const auto* command:{u"",u"COUNT bFire",u"BUTTONX bFire",u"PULSE9 bFire",u"TOGGLEZ bFire",u"AXIS4 aTurn",u"KEYNAME 0",u"SET X Y Z",u"\rBUTTON bFire"}) {
        Harness h;Check(!h.Exec(command)&&h.actorReads==0u&&h.writes.empty()&&h.logs.empty()&&!h.axisCalls&&h.scratch==0u,
            "Unrecognized action entered a selected provider or fabricated COUNT");
    }
    for(const auto* command:{u" \tBuTtOn \tbFire extra",u"PULSE \"bFire\" tail",u"TOGGLE\tbFire"}) {
        Harness h;Check(h.Exec(command)&&h.tokens==std::vector<std::u16string>{u"bFire"}&&h.writes.size()==1u,
            "ParseCommand/token whitespace/case/quote profile differs");
    }
    Harness punctuation;Check(punctuation.Exec(u"BUTTON_bFire")&&punctuation.tokens==std::vector<std::u16string>{u"_bFire"}&&
        punctuation.logs==std::vector<std::u16string>{u"Bad Button command"},"ParseCommand incorrectly rejected underscore suffix");
    Harness escapes;Check(escapes.Exec(u"BUTTON \"b\\Fire\"")&&escapes.tokens==std::vector<std::u16string>{u"b\\Fire"},
        "Input action ParseToken enabled escapes");
    Harness newline;Check(newline.Exec(u"BUTTON bFire\n")&&newline.tokens==std::vector<std::u16string>{u"bFire\n"}&&
        newline.logs.size()==1u,"Input token treated newline as ordinary space delimiter");
    const std::u16string longName(300u,u'Q');
    for(const bool quoted:{false,true}) {
        Harness h;const auto command=std::u16string{u"BUTTON "}+(quoted?u"\"":u"")+longName+(quoted?u"\"":u"")+u" tail";
        Check(h.Exec(command)&&h.tokens.size()==1u&&h.tokens.front()==std::u16string(255u,u'Q')&&h.logs.size()==1u,
            "Action name token failed original255-unit truncation");
    }
    const std::u16string embedded={u'B',u'U',u'T',u'T',u'O',u'N',u' ',u'b',u'F',u'i',u'r',u'e',u'\0',u'x'};
    Harness nul;Check(nul.Exec(embedded)&&nul.tokens==std::vector<std::u16string>{u"bFire"},"Action input crossed CString boundary");
    Harness bounded;const std::u16string excessive(QuestVr::CoreSetCommand::MaximumCommandUnits+1u,u'\0');
    Refuse([&]{bounded.Exec(excessive);},"unit limit");Check(bounded.scratch==0u&&bounded.actorReads==0u,"Storage bound applied after selected native access");
}
void BadAndLiveReads() {
    for(const auto* command:{u"BUTTON",u"BUTTON \"\"",u"PULSE None",u"TOGGLE absent"}) {
        Harness h;Check(h.Exec(command)&&h.logs==std::vector<std::u16string>{std::u16string_view{command}.starts_with(u"TOGGLE")?
            u"Bad Toggle command":u"Bad Button command"}&&h.writes.empty()&&h.reads==0u,
            "Recognized bad command declined or changed original output");
    }
    Harness none;none.actor=0u;auto q=none.Bind();q.findButton={};q.action={};q.readByte={};q.writeByte={};
    Check(QuestVr::ExecuteOriginalInputButtonCommands(u"BUTTON bFire",q)&&none.logs.size()==1u&&none.actorReads==1u&&none.tokens.empty(),
        "Actor=None parsed or required a later unused provider");
    Check(none.peak==QuestVr::InputButtonCommandsDetail::FrameBytes,"Actor=None allocated a property-name token");
    Harness changed;changed.afterActor=[&]{changed.actor=Harness::B;};
    Check(changed.Exec(u"BUTTON bFire")&&changed.selectedActors==std::vector<std::uintptr_t>{Harness::B}&&
        changed.bytes.at({Harness::B,Harness::Other})==1u&&changed.Value()==0u,"Lookup cached Actor from before ParseToken");
    Harness removed;removed.afterActor=[&]{removed.actor=0u;};
    Refuse([&]{removed.Exec(u"BUTTON bFire");},"post-token Actor");
    Check(removed.logs.empty()&&removed.tokens.empty()&&removed.actorReads==2u&&removed.scratch==0u,
        "Post-token Actor loss became a genuine initial Actor=None bad command");
    Harness action;action.action=0;action.afterFind=[&]{action.action=1;};
    Check(action.Exec(u"TOGGLE bFire")&&action.Value()==128u,"Action was captured before actual property lookup");
    Harness afterFind;afterFind.afterFind=[&]{afterFind.actor=Harness::B;};
    Check(afterFind.Exec(u"BUTTON bFire")&&afterFind.Value()==1u&&afterFind.bytes.at({Harness::B,Harness::Other})==0u,
        "Selected address was re-resolved after FindButtonName");
    const auto pos=[&](const Harness& h,const char* event){return std::find(h.events.begin(),h.events.end(),event)-h.events.begin();};
    Check(pos(action,"find")<pos(action,"action")&&pos(action,"action")<pos(action,"read")&&pos(action,"read")<pos(action,"write"),
        "Toggle lookup/action/read/store order differs");
    Harness foreign;foreign.finder=[](auto,std::u16string_view){return Cell{Harness::B,Harness::Other};};
    Refuse([&]{foreign.Exec(u"BUTTON bFire");},"partial or foreign");
    Check(foreign.writes.empty()&&foreign.logs.empty()&&foreign.scratch==0u,"Foreign cell became a bad-command success");
    Harness partial;partial.finder=[](auto actor,std::u16string_view){return Cell{actor,0u};};
    Refuse([&]{partial.Exec(u"BUTTON bFire");},"partial or foreign");
}
void AxisBoundary() {
    Harness h;Check(h.Exec(u" \tAXIS \taTurn SPEED=2")&&h.axisCalls==1u&&h.axisCursor==8u&&
        h.axisText==std::vector<std::u16string>{u" \tAXIS \taTurn SPEED=2"}&&h.actorReads==0u&&h.tokens.empty()&&h.logs.empty(),
        "AXIS selected wrong provider/cursor or fabricated partial numeric work");
    Harness absent;auto q=absent.Bind();q.axis={};
    Refuse([&]{QuestVr::ExecuteOriginalInputButtonCommands(u"AXIS aTurn",q);},"reached typed provider");
    Check(absent.actorReads==0u&&absent.logs.empty()&&absent.scratch==0u,"Missing typed AXIS fell through or fabricated Bad Axis output");
    Harness failure;auto throwing=failure.Bind();throwing.axis=[](auto,auto){throw std::runtime_error("actual axis refusal");};
    Refuse([&]{QuestVr::ExecuteOriginalInputButtonCommands(u"AXIS aTurn",throwing);},"actual axis refusal");
    Check(failure.scratch==0u&&failure.logs.empty(),"Axis refusal leaked scratch or became handled fallback");
}
void MissingAndBudgets() {
    Harness noRoot;noRoot.rooted=false;Refuse([&]{noRoot.Exec(u"BUTTON bFire");},"missing root");
    Check(!noRoot.scratch&&!noRoot.peak&&noRoot.actorReads==0u,"Missing outer root allocated or mutated");
    Harness noCleanup;auto clean=noCleanup.Bind();clean.leaveScratch={};
    Refuse([&]{QuestVr::ExecuteOriginalInputButtonCommands(u"BUTTON bFire",clean);},"scratch cleanup");
    Check(noCleanup.events.empty()&&noCleanup.peak==0u,"Missing cleanup ran providers");
    Harness noFind;auto find=noFind.Bind();find.findButton={};
    Refuse([&]{QuestVr::ExecuteOriginalInputButtonCommands(u"BUTTON bFire",find);},"reached typed provider");
    Check(noFind.actorReads==2u&&noFind.logs.empty()&&noFind.writes.empty()&&!noFind.scratch,"Missing Find became nullptr/success");
    Harness noAction;auto actionProvider=noAction.Bind();actionProvider.action={};
    Refuse([&]{QuestVr::ExecuteOriginalInputButtonCommands(u"BUTTON bFire",actionProvider);},"reached typed provider");
    Check(noAction.tokens.size()==1u&&noAction.writes.empty()&&!noAction.scratch,"Missing Action defaulted to a no-store value");
    Harness noEnter;auto enter=noEnter.Bind();enter.enterScratch={};
    Refuse([&]{QuestVr::ExecuteOriginalInputButtonCommands(u"BUTTON bFire",enter);},"reached typed provider");
    Check(noEnter.actorReads==0u&&!noEnter.scratch,"Missing scratch provider ran the command");
    Harness noWork;auto work=noWork.Bind();work.work={};
    Refuse([&]{QuestVr::ExecuteOriginalInputButtonCommands(u"BUTTON bFire",work);},"borrowed root work");
    Check(noWork.events.empty()&&!noWork.scratch,"Missing work source ran callbacks");
    Harness noRead;noRead.action=1;auto read=noRead.Bind();read.readByte={};
    Check(QuestVr::ExecuteOriginalInputButtonCommands(u"BUTTON bFire",read)&&noRead.Value()==1u,"Press demanded unused byte read");
    noRead.action=3;Refuse([&]{QuestVr::ExecuteOriginalInputButtonCommands(u"BUTTON bFire",read);},"reached typed provider");
    Harness noStore;noStore.action=0;auto store=noStore.Bind();store.writeByte={};
    Check(QuestVr::ExecuteOriginalInputButtonCommands(u"BUTTON bFire",store)&&noStore.writes.empty(),"No-store Action demanded store provider");
    noStore.action=1;Refuse([&]{QuestVr::ExecuteOriginalInputButtonCommands(u"BUTTON bFire",store);},"reached typed provider");
    Harness noLog;auto log=noLog.Bind();log.log={};
    Refuse([&]{QuestVr::ExecuteOriginalInputButtonCommands(u"TOGGLE absent",log);},"reached typed provider");
    Harness noActor;auto actor=noActor.Bind();actor.viewportActor={};
    Refuse([&]{QuestVr::ExecuteOriginalInputButtonCommands(u"BUTTON bFire",actor);},"reached typed provider");
    Check(noActor.logs.empty()&&!noActor.scratch,"Missing actor provider became genuine Actor=None");
    Harness partial;partial.afterWrite=[]{throw std::runtime_error("after store");};
    Refuse([&]{partial.Exec(u"TOGGLE bFire");},"after store");
    Check(partial.Value()==128u&&partial.writes.size()==1u&&partial.scratch==0u,"Exception internally erased original prefix store");
    Harness baseline;Check(baseline.Exec(u"TOGGLE bFire"),"Cannot measure generated action work");
    for(std::size_t cut=0u;cut<baseline.work;++cut) {
        Harness h;h.maximumWork=cut;Refuse([&]{h.Exec(u"TOGGLE bFire");},"shared work");
        Check(h.scratch==0u&&h.writes.empty(),"Work-prefix refusal leaked scratch or ran later write");
    }
    Harness exact;exact.maximumWork=baseline.work;Check(exact.Exec(u"TOGGLE bFire")&&exact.work==baseline.work&&exact.Value()==128u,
        "Exact root work bound changed the command");
    for(const auto cap:{std::size_t{0u},QuestVr::InputButtonCommandsDetail::FrameBytes,baseline.peak-1u}) {
        Harness h;h.maximumScratch=cap;Refuse([&]{h.Exec(u"BUTTON bFire");},"scratch admission");
        Check(h.scratch==0u&&h.writes.empty(),"Lexical admission failure leaked ledger or performed action write");
    }
    Harness scratch;scratch.maximumScratch=baseline.peak;Check(scratch.Exec(u"BUTTON bFire")&&scratch.scratch==0u,
        "Exact simultaneous scratch admission failed");
    Harness shared;auto q=shared.Bind();q.axis=[&](auto,auto){
        Check(shared.scratch>=QuestVr::InputButtonCommandsDetail::FrameBytes,"Axis call lost its outer lexical frame");
        Check(shared.Exec(u"BUTTON bFire"),"Nested generated branch declined");};
    Check(QuestVr::ExecuteOriginalInputButtonCommands(u"AXIS aTurn",q)&&shared.Value()==1u&&shared.scratch==0u&&
        shared.peak>baseline.peak,"Nested branch reset cumulative scratch instead of admitting overlap");
}
} // namespace
int main() {try {
    ByteTransitions();RecognitionAndTokens();BadAndLiveReads();AxisBoundary();MissingAndBudgets();
    std::cout<<"PASS original Input BUTTON/PULSE/TOGGLE coordinator checks="<<checks<<" refusals="<<refusals
        <<"; generated typed callback fixtures only, NOT actual RuntimeObject properties/Quest input/AXIS arithmetic/startup/headset proof\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL original Input button commands: "<<error.what()<<'\n';return 1;}}
