#include "quest_actor_timer_phase.h"

#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Queries=QuestVr::OriginalActorTimerPhaseQueries;
using Real=QuestVr::NativeScalar::Real;
constexpr Id actor=0x269bu;
std::size_t checks{},refusals{},oracleCases{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Refuses(F&& action,const char* why){bool refused{};try{action();}catch(const std::exception&){refused=true;}
    Require(refused,why);++refusals;}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
float Float(std::uint32_t value){return std::bit_cast<float>(value);}
struct Fixture {
    float rate=2.0f,counter=0.0f;bool loop{};std::int32_t periods=3;
    std::vector<float> rates;std::size_t rateAt{},work{},limit=std::numeric_limits<std::size_t>::max();
    std::size_t failProvider=std::numeric_limits<std::size_t>::max(),effects{};
    std::vector<std::string> events;std::vector<std::uint32_t> counters;
    std::function<void(const std::string&)> after;
    std::function<void(const Real&)> checkQuotient;
    void Event(std::string name){events.push_back(std::move(name));++effects;
        if(after)after(events.back());
        if(events.size()-1u==failProvider)throw std::runtime_error("caller timer provider refused");}
    Queries Query(){Queries q;
        q.work=[&]{if(work++>=limit)throw std::runtime_error("shared caller work exhausted");};
        q.rate=[&](Id a){Require(a==actor,"Timer rate changed Actor");Event("rate"+std::to_string(rateAt));
            const auto index=rateAt++;return rates.empty()?rate:rates.at(index);};
        q.counter=[&](Id a){Require(a==actor,"Timer counter changed Actor");Event("counter");return counter;};
        q.loop=[&](Id a){Require(a==actor,"Timer loop changed Actor");Event("loop");return loop;};
        q.writeCounter=[&](Id a,float value){Require(a==actor,"Timer counter store changed Actor");
            counter=value;counters.push_back(Bits(value));Event("writeCounter");};
        q.writeRate=[&](Id a,float value){Require(a==actor,"Timer rate store changed Actor");rate=value;Event("writeRate");};
        q.convertPeriods=[&](Real value){Event("convert");if(checkQuotient)checkQuotient(value);return periods;};
        q.timer=[&](Id a){Require(a==actor,"Timer callback changed Actor");Event("timer");};return q;}
    QuestVr::OriginalActorTimerPhaseStats Run(float delta=7.0f){return QuestVr::AdvanceOriginalActorTimerPhase(actor,delta,Query());}
};
const std::vector<std::string> expiredOrder{"rate0","counter","rate1","writeCounter","rate2","convert",
    "rate3","writeCounter","loop","writeRate","timer"};
void ExactSourceSchedule(){
    Fixture f;const auto out=f.Run();
    Require(f.events==expiredOrder,"Original timer source service/store order changed");
    Require(out.active&&out.expired&&out.periods==3&&out.steps==11u&&f.work==11u&&out.rateReads==4u&&
        out.counterReads==1u&&out.counterWrites==2u&&out.loopReads==1u&&out.rateWrites==1u&&
        out.conversionCalls==1u&&out.timerCalls==1u,"Original timer source counters changed");
    Require(f.counters==std::vector<std::uint32_t>{Bits(7.0f),Bits(1.0f)}&&Bits(f.rate)==0u,
        "Timer did not preserve first spill then exact remainder/nonloop rate store");
    Fixture below;below.counter=0.0f;const auto shortStep=below.Run(1.0f);
    Require(!shortStep.expired&&below.events==std::vector<std::string>{"rate0","counter","rate1","writeCounter"}&&
        below.counters==std::vector<std::uint32_t>{Bits(1.0f)},"Unexpired timer lost unconditional Counter FST");
    Fixture same;same.counter=1.0f;same.Run(0.0f);
    Require(same.counters==std::vector<std::uint32_t>{Bits(1.0f)},"SameBits Counter incorrectly suppressed native FST");
    Fixture fresh;fresh.rates={2.0f,3.0f,4.0f,5.0f};fresh.periods=2;
    fresh.checkQuotient=[](const Real& value){Require(QuestVr::NativeScalar::StoreDouble(value)==2.5,
        "Timer conversion did not receive retained sum/fresh FDIV rate");};
    fresh.Run(10.0f);Require(fresh.counter==0.0f&&fresh.rateAt==4u,"Timer cached Rate across FCOM/FDIV/FMUL");
    Fixture compare;compare.periods=0;compare.after=[&](const std::string& name){if(name=="writeCounter")compare.rate=100.0f;};
    compare.Run(5.0f);Require(compare.events.back()=="timer"&&compare.counters.size()==2u,
        "Timer recomputed FCOM after first Counter store");
    Fixture zero;zero.periods=0;auto q=zero.Query();q.loop={};q.writeRate={};
    const auto zeroOut=QuestVr::AdvanceOriginalActorTimerPhase(actor,7.0f,q);
    Require(zeroOut.expired&&!zeroOut.loopReads&&!zeroOut.rateWrites&&zeroOut.timerCalls==1u&&zero.counter==7.0f,
        "EAX0 incorrectly demanded loop/rate or skipped Timer");
    Fixture negative;negative.periods=-2;negative.Run(5.0f);
    Require(negative.counter==9.0f&&negative.events==expiredOrder,"Signed negative EAX result word lost FILD32 semantics");
    Fixture large;large.rate=1.0f;large.periods=16'777'217;large.Run(16'777'218.0f);
    Require(large.counter==1.0f,"FILD32 count was rounded through binary32");
    Fixture looping;looping.loop=true;const auto loopOut=looping.Run();
    Require(loopOut.loopReads==1u&&!loopOut.rateWrites&&looping.rate==2.0f&&looping.events.back()=="timer",
        "Looping timer incorrectly clears Rate");
    Fixture rearmed;rearmed.after=[&](const std::string& name){if(name=="timer"){rearmed.rate=9.0f;rearmed.counter=4.0f;}};
    rearmed.Run();Require(rearmed.rate==9.0f&&rearmed.counter==4.0f&&rearmed.events==expiredOrder,
        "Timer wrote again after callback rearming");
    for(auto word:{0u,0x80000000u,0xbf800000u,0x7fc12345u,0x7f800001u,0xff800000u}){
        Fixture inactive;inactive.rate=Float(word);auto lazy=inactive.Query();lazy.counter={};lazy.loop={};lazy.writeCounter={};
        lazy.writeRate={};lazy.convertPeriods={};lazy.timer={};const auto result=QuestVr::AdvanceOriginalActorTimerPhase(actor,Float(0x7fc00001u),lazy);
        Require(!result.active&&!result.expired&&inactive.events==std::vector<std::string>{"rate0"}&&inactive.work==1u,
            "Inactive/unordered Rate eagerly demanded Counter/loop/delta arithmetic");}
    Fixture nan;nan.counter=Float(0x7fc12345u);auto nanQ=nan.Query();nanQ.convertPeriods={};nanQ.loop={};nanQ.timer={};
    const auto nanResult=QuestVr::AdvanceOriginalActorTimerPhase(actor,1.0f,nanQ);
    Require(nanResult.active&&!nanResult.expired&&nanResult.counterWrites==1u&&nan.events.size()==4u&&
        (Bits(nan.counter)&0x7fc00000u)==0x7fc00000u,"Unordered sum did not spill then leave without conversion");
}
void FailurePrefixesAndCallerUndo(){
    Fixture null;Refuses([&]{QuestVr::AdvanceOriginalActorTimerPhase(0u,1.0f,null.Query());},"Null timer Actor accepted");
    Require(null.events.empty()&&!null.work,"Null timer Actor demanded providers/work");
    Fixture noWork;auto q=noWork.Query();q.work={};Refuses([&]{QuestVr::AdvanceOriginalActorTimerPhase(actor,1.0f,q);},
        "Unbudgeted timer phase accepted");Require(noWork.events.empty(),"Missing work ran a timer provider");
    for(std::size_t cut=0u;cut<expiredOrder.size();++cut){
        Fixture budget;budget.limit=cut;Refuses([&]{budget.Run();},"Timer swallowed caller work exhaustion");
        Require(budget.events==std::vector<std::string>(expiredOrder.begin(),expiredOrder.begin()+static_cast<std::ptrdiff_t>(cut)),
            "Timer work refusal skipped or invented a native prefix");
        Fixture provider;provider.failProvider=cut;Refuses([&]{provider.Run();},"Timer swallowed a provider failure");
        Require(provider.events==std::vector<std::string>(expiredOrder.begin(),expiredOrder.begin()+static_cast<std::ptrdiff_t>(cut+1u)),
            "Timer provider failure lost its observable native prefix");
        Fixture caller;caller.failProvider=cut;const auto oldRate=Bits(caller.rate),oldCounter=Bits(caller.counter);
        bool failed{};try{caller.Run();}catch(const std::exception&){failed=true;caller.rate=Float(oldRate);caller.counter=Float(oldCounter);caller.effects=0u;}
        Require(failed&&Bits(caller.rate)==oldRate&&Bits(caller.counter)==oldCounter&&!caller.effects,
            "Mock outer caller root did not restore timer provider effects");}
    Fixture late;late.failProvider=expiredOrder.size()-1u;Refuses([&]{late.Run();},"Late Timer callback failure accepted");
    Require(Bits(late.rate)==0u&&late.counter==1.0f,"Timer invented its own local undo on late failure");
    for(unsigned missing=0u;missing<7u;++missing){Fixture f;auto absent=f.Query();
        switch(missing){case 0:absent.rate={};break;case 1:absent.counter={};break;case 2:absent.writeCounter={};break;
        case 3:absent.convertPeriods={};break;case 4:absent.loop={};break;case 5:absent.writeRate={};break;case 6:absent.timer={};break;}
        Refuses([&]{QuestVr::AdvanceOriginalActorTimerPhase(actor,7.0f,absent);},"Missing reached timer provider accepted");}
}

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
struct Precision {
    unsigned short saved{},selected{};
    Precision(){static_assert(std::numeric_limits<long double>::digits==64,"Timer oracle requires GNU x87 PC64");
        asm volatile("fnstcw %0":"=m"(saved));selected=static_cast<unsigned short>((saved&~0x0f00u)|0x033fu);
        asm volatile("fnclex; fldcw %0"::"m"(selected):"memory");}
    ~Precision(){asm volatile("fnclex; fldcw %0"::"m"(saved):"memory");}
};
struct OracleResult {float first{},remainder{};long double quotient{};std::int32_t periods{};bool expired{};};
OracleResult Oracle(float rate,float counter,float delta,const Precision& precision){
    OracleResult out;long double sum;unsigned short status;
    // Independent own instructions, including FCOM before FST and no pop of
    // the retained PC64 sum. No commercial code or production scalar helper.
    asm volatile("flds %[delta]; fadds %[counter]; fcoms %[rate]; fsts %[first]; fnstsw %%ax; fstpt %[sum]":
        [first]"=m"(out.first),[sum]"=m"(sum),"=a"(status):
        [delta]"m"(delta),[counter]"m"(counter),[rate]"m"(rate):"st","memory");
    out.remainder=out.first;if(status&0x0100u)return out;out.expired=true;
    asm volatile("fldt %1; fdivs %2; fstpt %0":"=m"(out.quotient):"m"(sum),"m"(rate):"st","memory");
    const auto truncate=static_cast<unsigned short>((precision.selected&~0x0c00u)|0x0c00u);
    // For this independent finite, signed32-domain oracle, truncating FIST32
    // equals the original _ftol's low result word. Wider/exceptional CRT
    // conversions deliberately belong to the explicit conversion provider.
    asm volatile("fldcw %2; fldt %1; fistpl %0; fldcw %3":"=m"(out.periods):"m"(out.quotient),"m"(truncate),
        "m"(precision.selected):"st","memory");
    // Exact original opcode D8 E9 is FSUBR ST0,ST1. Encoding it explicitly
    // avoids GAS's historical AT&T FSUB/FSUBR register-operand reversal.
    asm volatile("fldt %1; fildl %2; fmuls %3; .byte 0xd8,0xe9; fstps %0; fstp %%st(0)":
        "=m"(out.remainder):"m"(sum),"m"(out.periods),"m"(rate):"st","memory");
    return out;
}
void ExactRetained(const Real& actual,long double expected){
    std::array<unsigned char,sizeof(long double)> bytes{};std::memcpy(bytes.data(),&expected,sizeof(expected));
    std::uint64_t significand{};std::uint16_t signExponent{};
    std::memcpy(&significand,bytes.data(),sizeof(significand));std::memcpy(&signExponent,bytes.data()+8u,sizeof(signExponent));
    Require(actual.kind==QuestVr::NativeScalar::Kind::Finite&&((signExponent&0x7fffu)!=0x7fffu),
        "Finite timer oracle received exceptional quotient");
    auto own=actual.finite.significand;auto ownExponent=actual.finite.exponent;
    int expectedExponent=(signExponent&0x7fffu)?static_cast<int>(signExponent&0x7fffu)-16383-63:-16445;
    while(own&&!(own&1u)){own>>=1u;++ownExponent;}
    while(significand&&!(significand&1u)){significand>>=1u;++expectedExponent;}
    Require(own==significand&&(!own||ownExponent==expectedExponent)&&actual.finite.negative==((signExponent&0x8000u)!=0u),
        "Timer FDIV quotient differs from independently retained x87 PC64");
}
void CompareOracle(float rate,float counter,float delta,bool loop,const Precision& precision){
    const auto expected=Oracle(rate,counter,delta,precision);Fixture f;f.rate=rate;f.counter=counter;f.loop=loop;f.periods=expected.periods;
    f.checkQuotient=[&](const Real& value){ExactRetained(value,expected.quotient);};const auto actual=f.Run(delta);++oracleCases;
    Require(actual.active&&actual.expired==expected.expired&&Bits(f.counter)==Bits(expected.remainder),
        "Timer Counter differs from independent x87 source schedule");
    Require(f.counters.size()==(expected.expired?2u:1u)&&f.counters.front()==Bits(expected.first),
        "Timer first Counter FST or expiry second store differs from x87");
    Require(Bits(f.rate)==Bits(expected.expired&&!loop&&expected.periods!=0?0.0f:rate),
        "Timer nonloop rate store differs from independent signed conversion word");
}
void ArithmeticOracle(){Precision precision;
    for(float rate:{0.001f,0.1f,0.5f,1.0f,2.0f,3.0f,100.0f,10000.0f})
        for(float counter:{-100.0f,-0.0f,0.0f,0.1f,0.99999994f,1.0f,1.00000012f,100.0f})
            for(float delta:{0.0f,0.00000001f,0.016666667f,0.5f,1.0f,7.0f,1000.0f})
                for(bool loop:{false,true})CompareOracle(rate,counter,delta,loop,precision);
    std::uint64_t seed=0x103a269b103a2704ull;const auto random=[&](){seed^=seed<<13u;seed^=seed>>7u;seed^=seed<<17u;return seed;};
    for(unsigned i=0u;i<8192u;++i){
        const auto a=static_cast<std::uint32_t>(random()),b=static_cast<std::uint32_t>(random()),c=static_cast<std::uint32_t>(random());
        const auto rate=Float(((115u+(a%24u))<<23u)|(a&0x7fffffu));
        const auto counter=Float((b&0x80000000u)|((100u+(b%38u))<<23u)|(b&0x7fffffu));
        const auto delta=Float(((100u+(c%38u))<<23u)|(c&0x7fffffu));
        CompareOracle(rate,counter,delta,(i&1u)!=0u,precision);
    }
}
#else
void ArithmeticOracle(){throw std::runtime_error("Timer acceptance requires independent GNU x87 PC64, no skipped oracle");}
#endif
} // namespace
int main(){try{ExactSourceSchedule();FailurePrefixesAndCallerUndo();ArithmeticOracle();
    std::cout<<"Original Actor timer phase: "<<checks<<" checks / "<<refusals<<" refusals / "<<oracleCases
        <<" independent x87 cases; same-root native phase only, NOT completed Pawn physics or playable Training\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
