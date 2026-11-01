#include "quest_input_axis_command.h"
#include "quest_input_button_commands.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace {
using Cell=QuestVr::OriginalInputAxisCell;
using Queries=QuestVr::OriginalInputAxisQueries;
std::size_t checks{},refusals{};
void Check(bool good,const char* why) {++checks;if(!good)throw std::runtime_error(why);}
template<class Run> void Refuse(Run&& run,const char* expected="") {
    bool refused{};try {run();}catch(const std::exception& error){refused=std::string(error.what()).find(expected)!=std::string::npos;}
    Check(refused,"Expected selected original Input AXIS refusal");++refusals;
}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
float Float(std::uint32_t value) {return std::bit_cast<float>(value);}
struct Harness {
    static constexpr std::uintptr_t A=10u,B=20u,Turn=100u,Other=200u;
    std::uintptr_t actor{A};std::int32_t action{2};float delta{1.0f};bool rooted{true};
    std::map<std::pair<std::uintptr_t,std::uintptr_t>,float> values{{{A,Turn},0.0f},{{B,Other},0.0f}};
    std::vector<std::string> events;std::vector<std::u16string> tokens,logs;
    std::vector<std::uintptr_t> selectedActors;std::vector<std::tuple<Cell,float>> writes;
    std::size_t work{},maximumWork{5000000u},scratch{},peak{},maximumScratch{100000u},enters{},leaves{},reads{},actorReads{},actionReads{},deltaReads{};
    std::function<void()> afterActor,afterFind,afterAction,afterDelta,afterWrite,workHook;
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
        q.findAxis=[this](std::uintptr_t selected,std::u16string_view token) {
            events.emplace_back("find");selectedActors.push_back(selected);tokens.emplace_back(token);
            const auto result=finder?finder(selected,token):token==u"aTurn"?Cell{selected,selected==A?Turn:Other}:Cell{};
            if(afterFind)afterFind();
            return result;
        };
        q.action=[this]{events.emplace_back("action");++actionReads;const auto value=action;if(afterAction)afterAction();return value;};
        q.delta=[this]{events.emplace_back("delta");++deltaReads;const auto value=delta;if(afterDelta)afterDelta();return value;};
        q.readFloat=[this](Cell cell){events.emplace_back("read");++reads;return values.at({cell.actor,cell.property});};
        q.writeFloat=[this](Cell cell,float value){events.emplace_back("write");values.at({cell.actor,cell.property})=value;
            writes.emplace_back(cell,value);if(afterWrite)afterWrite();};
        q.logf=[this](std::u16string_view text){events.emplace_back("logf");logs.emplace_back(text);};return q;
    }
    void Exec(std::u16string_view command) {
        std::size_t cursor{};
        if(!QuestVr::NativeInputKeyQueries::ParseCommand(command,cursor,u"AXIS"))throw std::runtime_error("Fixture command is not AXIS");
        QuestVr::ExecuteOriginalInputAxisCommand(command,cursor,Bind());
    }
    float Value()const{return values.at({A,Turn});}
};
void DecimalAndSearch() {
    struct Numeric {std::u16string_view text;std::uint32_t expected;};
    const Numeric cases[]={
        {u"",0u},{u"junk",0u},{u"+",0u},{u"-junk",0u},{u".",0u},{u"e3",0u},
        {u"0",0u},{u"-0",0x80000000u},{u"-0.000e999",0x80000000u},{u"+1",0x3f800000u},
        {u"  \t\r\n\v\f-2.5tail",0xc0200000u},{u".5",0x3f000000u},{u"-.125",0xbe000000u},
        {u"1.",0x3f800000u},{u"1.5.9",0x3fc00000u},{u"2e",0x40000000u},{u"2e+",0x40000000u},
        {u"2e-tail",0x40000000u},{u"2e3junk",0x44fa0000u},{u"2e-3!",0x3b03126fu},
        {u"3.1415926535897932384626433832795",0x40490fdbu},
        {u"1.000000059604644775390625",0x3f800000u},
        // atof's binary64 result rounds this infinitesimal excess back to the
        // exact Float midpoint before the original Parse Float store.
        {u"1.000000059604644775390626",0x3f800000u},
        {u"1.000000178813934326171875",0x3f800002u},
        {u"1e39",0x7f800000u},{u"-1e39",0xff800000u},{u"1e309",0x7f800000u},
        {u"-1e99999999999999999999999",0xff800000u},{u"1e-99999999999",0u},
        {u"-1e-99999999999",0x80000000u},{u"1e-46",0u},{u"-1e-46",0x80000000u},
        {u"1.40129846432481707092372958328991613128026194187651577175706828388979108268586060148663818836212158203125e-45",1u},
        {u"7.00649232162408535461864791644958065640130970938257885878534141944895541342930300743319094181060791015625e-46",0u},
        {u"2.8025969286496341418474591665798322625605238837530315435141365677795821653717212029732763767242431640625e-45",2u},
        {u"2,5",0x40000000u},{u"2_5",0x40000000u},{u"2\u01005",0x40000000u},
        {u"2\u00e95",0x40000000u},{u"\u00a02",0u},{u"\u01002",0u}
    };
    for(const auto& test:cases) {
        Harness h;h.values[{Harness::A,Harness::Turn}]=-0.0f;
        h.Exec(std::u16string{u"AXIS aTurn SPEED="}+std::u16string{test.text});
        Check(Bits(h.Value())==test.expected,"Ordinary C-locale decimal prefix Float bits differ");
        Check(h.writes.size()==1u&&h.logs.empty()&&h.scratch==0u&&h.actorReads==2u&&h.actionReads==1u,
            "Successful decimal path changed provider/store/scratch profile");
    }
    struct Search {std::u16string_view tail;float expected;};
    const Search searches[]={
        {u"",1.0f},{u" extra",1.0f},{u" SPEED=2",2.0f},{u" speed=2",2.0f},
        {u" xSPEED=2",1.0f},{u" 0SPEED=2",1.0f},{u" _SPEED=2",2.0f},
        {u" -SPEED=2",2.0f},{u" /SPEED=2",2.0f},{u" \"SPEED=2\"",2.0f},
        {u" SPEED=3 SPEED=7",3.0f},{u" xSPEED=7 SPEED=3",3.0f},{u" SPEED=",0.0f},
        {u" SPEED=+2e2x",200.0f},{u" SPEED =2",1.0f},{u" SPEED=1xSPEED=9",1.0f}
    };
    for(const auto& test:searches) {Harness h;h.Exec(std::u16string{u"AXIS aTurn"}+std::u16string{test.tail});
        Check(Bits(h.Value())==Bits(test.expected)&&h.scratch==0u,"Core Parse SPEED search incorrectly tokenized/filter-matched input");}
    for(const auto* special:{u"inf",u"INFINITY",u"nan",u"NaN(1)",u"0x1p2",u"-0X1",u"1.#INF",u"-1.#QNAN"}) {
        Harness h;Refuse([&]{h.Exec(std::u16string{u"AXIS aTurn SPEED="}+special);},"numeric profile is unestablished");
        Check(h.writes.empty()&&h.actionReads==0u&&h.logs.empty()&&h.scratch==0u,
            "Unestablished CRT domain became no-store/bad-command/fallthrough success");
    }
    Harness last;last.Exec(std::u16string{u"AXIS aTurn SPEED="}+std::u16string(1022u,u'0')+u"1");
    Check(last.Value()==1.0f,"appToAnsi failed to include its1023rd unit");
    Harness cut;cut.Exec(std::u16string{u"AXIS aTurn SPEED="}+std::u16string(1023u,u'0')+u"1");
    Check(cut.Value()==0.0f,"appToAnsi read beyond1023 copied units");
    Harness huge;huge.Exec(std::u16string{u"AXIS aTurn SPEED="}+std::u16string(1000u,u'9')+u"e-1000");
    Check(huge.Value()==1.0f&&huge.scratch==0u,"Large balanced decimal mantissa lost exact-rational rounding");
    Harness balanced;balanced.Exec(std::u16string{u"AXIS aTurn SPEED=0."}+std::u16string(990u,u'0')+u"125e993");
    Check(balanced.Value()==125.0f,"Large balanced decimal denominator lost exponent/significant-digit alignment");
    const std::u16string nul={u'A',u'X',u'I',u'S',u' ',u'a',u'T',u'u',u'r',u'n',u' ',u'S',u'P',u'E',u'E',u'D',u'=',u'2',u'\0',u'9'};
    Harness terminated;terminated.Exec(nul);Check(terminated.Value()==2.0f,"AXIS SPEED crossed its CString boundary");
}
void Arithmetic() {
    const auto pos=[](const Harness& h,const char* event){return std::find(h.events.begin(),h.events.end(),event)-h.events.begin();};
    for(const auto action:{std::int32_t{-1},0,1,2,3,4,std::numeric_limits<std::int32_t>::max()}) {
        Harness h;h.action=action;h.delta=2.0f;h.values[{Harness::A,Harness::Turn}]=3.0f;h.Exec(u"AXIS aTurn SPEED=5");
        const auto expected=action==2?13.0f:action==4?3.1f:3.0f;
        Check(Bits(h.Value())==Bits(expected)&&h.writes.size()==static_cast<std::size_t>(action==2||action==4),
            "Original Hold/Axis action scale/store selection differs");
        Check(h.deltaReads==h.writes.size()&&h.reads==h.writes.size()&&h.actionReads==1u,"Inactive action read Delta/Axis or active action omitted access");
        if(!h.writes.empty())Check(pos(h,"find")<pos(h,"action")&&pos(h,"action")<pos(h,"delta")&&
            pos(h,"delta")<pos(h,"read")&&pos(h,"read")<pos(h,"write"),"AXIS lookup/action/delta/current/store order differs");
    }
    Harness retained;retained.delta=Float(0x3f800001u);retained.values[{Harness::A,Harness::Turn}]=-1.0f;
    retained.Exec(u"AXIS aTurn SPEED=0.999999940395355224609375");
    Check(Bits(retained.Value())==0x337ffffeu,"AXIS prematurely Float-rounded Delta*Speed before current-axis addition");
    const std::array<std::uint32_t,17u> patterns{{0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,
        0x3f800001u,0xbf800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,
        0x7fc12345u,0xffc23456u,0x7fa12345u,0xffa23456u,0x4b800001u}};
    for(const auto delta:patterns)for(const auto current:patterns)for(const auto action:{2,4}) {
        Harness h;h.action=action;h.delta=Float(delta);h.values[{Harness::A,Harness::Turn}]=Float(current);h.Exec(u"AXIS aTurn SPEED=1.5");
        auto product=QuestVr::NativeScalar::Multiply(QuestVr::NativeScalar::Load(Float(delta)),QuestVr::NativeScalar::Memory(1.5f));
        if(action==4)product=QuestVr::NativeScalar::Multiply(product,QuestVr::NativeScalar::Memory(std::bit_cast<double>(0x3f847ae147ae147bull)));
        const auto expected=QuestVr::NativeScalar::StoreFloat(QuestVr::NativeScalar::Add(product,QuestVr::NativeScalar::Memory(Float(current))));
        Check(Bits(h.Value())==Bits(expected)&&h.writes.size()==1u&&h.logs.empty()&&h.scratch==0u,
            "AXIS did not retain the established PC64 nonfinite/subnormal/zero-sign profile");
    }
    Harness minusZero;minusZero.delta=-0.0f;minusZero.values[{Harness::A,Harness::Turn}]=-0.0f;minusZero.Exec(u"AXIS aTurn");
    Check(Bits(minusZero.Value())==0x80000000u,"AXIS discarded the genuine signed-zero arithmetic result");
}
void NativeOrderingAndBadCommands() {
    for(const auto* command:{u"AXIS",u"AXIS \"\"",u"AXIS missing SPEED=inf"}) {
        Harness h;h.Exec(command);Check(h.logs==std::vector<std::u16string>{u"Bad Axis command"}&&h.writes.empty()&&
            h.actionReads==0u&&h.deltaReads==0u&&h.scratch==0u,"Bad AXIS changed output/order or parsed an unreached SPEED value");
    }
    Harness none;none.actor=0u;auto q=none.Bind();q.findAxis={};q.action={};q.delta={};q.readFloat={};q.writeFloat={};
    QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS aTurn SPEED=nan",5u,q);
    Check(none.logs.size()==1u&&none.actorReads==1u&&none.tokens.empty()&&none.peak==QuestVr::InputAxisCommandDetail::FrameBytes,
        "Initial Actor=None parsed/required later providers or allocated a property token");
    Harness changed;changed.afterActor=[&]{changed.actor=Harness::B;};changed.Exec(u"AXIS aTurn");
    Check(changed.selectedActors==std::vector<std::uintptr_t>{Harness::B}&&changed.values.at({Harness::B,Harness::Other})==1.0f&&
        changed.Value()==0.0f,"FindAxisName retained Actor from before ParseToken");
    Harness removed;removed.afterActor=[&]{removed.actor=0u;};
    Refuse([&]{removed.Exec(u"AXIS aTurn");},"post-token Actor");Check(removed.logs.empty()&&removed.scratch==0u,"Post-token Actor loss became a genuine bad command");
    Harness later;later.afterFind=[&]{later.actor=Harness::B;};later.Exec(u"AXIS aTurn");
    Check(later.Value()==1.0f&&later.values.at({Harness::B,Harness::Other})==0.0f,"Selected axis address was rebound after FindAxisName");
    Harness action;action.action=0;action.afterFind=[&]{action.action=4;};action.delta=100.0f;action.Exec(u"AXIS aTurn");
    Check(action.Value()==1.0f&&action.writes.size()==1u,"Action was cached before FindAxisName/SPEED");
    Harness delta;delta.delta=0.0f;delta.afterAction=[&]{delta.delta=2.0f;};delta.Exec(u"AXIS aTurn");
    Check(delta.Value()==2.0f,"Delta was cached before actual Action selection");
    Harness current;current.afterDelta=[&]{current.values[{Harness::A,Harness::Turn}]=4.0f;};current.Exec(u"AXIS aTurn");
    Check(current.Value()==5.0f,"Current selected axis was captured before Delta multiplication");
    Harness partial;partial.finder=[](auto actor,std::u16string_view){return Cell{actor,0u};};
    Refuse([&]{partial.Exec(u"AXIS aTurn");},"partial or foreign");
    Harness foreign;foreign.finder=[](auto,std::u16string_view){return Cell{Harness::B,Harness::Other};};
    Refuse([&]{foreign.Exec(u"AXIS aTurn");},"partial or foreign");
    for(const auto* command:{u" \tAxIs \"aTurn\" SPEED=2",u"AXIS\taTurn SPEED=2"}) {
        Harness h;h.Exec(command);Check(h.Value()==2.0f&&h.tokens==std::vector<std::u16string>{u"aTurn"},"AXIS command/token case/space/quote profile differs");
    }
    Harness escape;escape.Exec(u"AXIS \"a\\Turn\"");Check(escape.tokens==std::vector<std::u16string>{u"a\\Turn"}&&escape.logs.size()==1u,
        "AXIS ParseToken enabled escape processing");
    Harness longName;longName.Exec(std::u16string{u"AXIS \""}+std::u16string(300u,u'A')+u"\" SPEED=2");
    Check(longName.tokens==std::vector<std::u16string>{std::u16string(255u,u'A')}&&longName.logs.size()==1u,
        "AXIS name token did not preserve its255-unit quoted truncation");
}
void MissingAndBudgets() {
    Harness noRoot;noRoot.rooted=false;Refuse([&]{noRoot.Exec(u"AXIS aTurn");},"missing root");
    Check(noRoot.peak==0u&&noRoot.actorReads==0u,"Missing root allocated or accessed native state");
    Harness cleanup;auto clean=cleanup.Bind();clean.leaveScratch={};Refuse([&]{QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS aTurn",5u,clean);},"scratch cleanup");
    Check(cleanup.events.empty(),"Missing cleanup called providers");
    Harness noWork;auto work=noWork.Bind();work.work={};Refuse([&]{QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS aTurn",5u,work);},"borrowed root work");
    Harness noEnter;auto enter=noEnter.Bind();enter.enterScratch={};Refuse([&]{QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS aTurn",5u,enter);},"reached typed provider");
    Harness noFind;auto find=noFind.Bind();find.findAxis={};Refuse([&]{QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS aTurn",5u,find);},"reached typed provider");
    Harness noAction;auto action=noAction.Bind();action.action={};Refuse([&]{QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS aTurn SPEED=2",5u,action);},"reached typed provider");
    Harness noDelta;auto delta=noDelta.Bind();delta.delta={};Refuse([&]{QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS aTurn",5u,delta);},"reached typed provider");
    Harness noRead;auto read=noRead.Bind();read.readFloat={};Refuse([&]{QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS aTurn",5u,read);},"reached typed provider");
    Harness noStore;auto store=noStore.Bind();store.writeFloat={};Refuse([&]{QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS aTurn",5u,store);},"reached typed provider");
    Harness noLog;auto log=noLog.Bind();log.logf={};Refuse([&]{QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS missing",5u,log);},"reached typed provider");
    Harness inactive;inactive.action=0;auto unused=inactive.Bind();unused.delta={};unused.readFloat={};unused.writeFloat={};
    QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS aTurn SPEED=2",5u,unused);Check(inactive.writes.empty()&&inactive.actionReads==1u,
        "Inactive AXIS demanded unused Delta/read/write providers");
    Harness earlySpecial;earlySpecial.action=0;Refuse([&]{earlySpecial.Exec(u"AXIS aTurn SPEED=inf");},"unestablished");
    Check(earlySpecial.actionReads==0u,"Inactive AXIS skipped the preceding SPEED parse");
    Harness oversized;const std::u16string excessive(QuestVr::CoreSetCommand::MaximumCommandUnits+1u,u'\0');
    Refuse([&]{QuestVr::ExecuteOriginalInputAxisCommand(excessive,0u,oversized.Bind());},"unit limit");
    Check(oversized.actorReads==0u&&oversized.scratch==0u,"Storage limit occurred after native access");
    Harness cursor;Refuse([&]{QuestVr::ExecuteOriginalInputAxisCommand(u"AXIS",9u,cursor.Bind());},"cursor exceeds");
    Check(cursor.actorReads==0u&&cursor.scratch==0u,"Invalid cursor accessed native authority");
    Harness partial;partial.afterWrite=[]{throw std::runtime_error("after store");};Refuse([&]{partial.Exec(u"AXIS aTurn SPEED=2");},"after store");
    Check(partial.Value()==2.0f&&partial.scratch==0u,"Pure helper erased a prefix store or leaked scratch after failure");
    Harness baseline;baseline.Exec(u"AXIS aTurn SPEED=2");
    for(std::size_t cut=0u;cut<baseline.work;++cut) {
        Harness h;h.maximumWork=cut;Refuse([&]{h.Exec(u"AXIS aTurn SPEED=2");},"shared work");
        Check(h.scratch==0u&&h.writes.empty(),"Shared work refusal leaked lexical storage or ran a later axis write");
    }
    Harness exact;exact.maximumWork=baseline.work;exact.Exec(u"AXIS aTurn SPEED=2");
    Check(exact.work==baseline.work&&exact.Value()==2.0f&&exact.scratch==0u,"Exact root work bound changed AXIS execution");
    for(const auto cap:{std::size_t{0u},QuestVr::InputAxisCommandDetail::FrameBytes,baseline.peak-1u}) {
        Harness h;h.maximumScratch=cap;Refuse([&]{h.Exec(u"AXIS aTurn SPEED=2");},"scratch admission");
        Check(h.scratch==0u&&h.writes.empty(),"Scratch refusal leaked lexical storage or selected a native write");
    }
    Harness admitted;admitted.maximumScratch=baseline.peak;admitted.Exec(u"AXIS aTurn SPEED=2");
    Check(admitted.Value()==2.0f&&admitted.scratch==0u,"Exact simultaneous scratch bound changed AXIS");
    Harness nested;QuestVr::OriginalInputButtonQueries outer;
    outer.requireOuterRoot=[&]{if(!nested.rooted)throw std::runtime_error("root");};outer.work=[&]{nested.Step();};
    outer.enterScratch=[&](std::size_t n){nested.Enter(n);};outer.leaveScratch=[&](std::size_t n)noexcept{nested.Leave(n);};
    outer.axis=[&](auto text,auto at){Check(nested.scratch==QuestVr::InputButtonCommandsDetail::FrameBytes,
        "Selected AXIS lost its enclosing Input branch frame");QuestVr::ExecuteOriginalInputAxisCommand(text,at,nested.Bind());};
    Check(QuestVr::ExecuteOriginalInputButtonCommands(u"AXIS aTurn SPEED=2",outer)&&nested.Value()==2.0f&&nested.scratch==0u&&
        nested.peak==baseline.peak+QuestVr::InputButtonCommandsDetail::FrameBytes,"AXIS dispatcher replaced rather than shared simultaneous lexical storage");
}
} // namespace
int main() {try {
    DecimalAndSearch();Arithmetic();NativeOrderingAndBadCommands();MissingAndBudgets();
    std::cout<<"PASS original Input AXIS coordinator checks="<<checks<<" refusals="<<refusals
        <<"; generated typed fixtures and bounded ordinary C-locale decimal profile only; NOT full CRT/actual Pawn/Quest/Init/startup/headset proof\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL original Input AXIS: "<<error.what()<<'\n';return 1;}}
