#include "quest_actor_last_rendered.h"

#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Queries=QuestVr::OriginalActorLastRenderedQueries;
constexpr Id actor=0x100u,level=0x200u,otherLevel=0x300u;
std::size_t checks{},refusals{},oracleCases{};
void Require(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
template<class Function> void Refuses(Function&& function,const char* message) {
    bool refused{};try{function();}catch(const std::exception&){refused=true;}
    Require(refused,message);++refusals;
}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
float Float(std::uint32_t word) {return std::bit_cast<float>(word);}
double Double(std::uint64_t word) {return std::bit_cast<double>(word);}
struct Fixture {
    Id edge=level;
    float last=-10.0f;
    std::map<Id,double> times{{level,0.0},{otherLevel,1000.0}};
    std::vector<std::string> events;
    std::size_t work{},limit=std::numeric_limits<std::size_t>::max();
    std::size_t failProvider=std::numeric_limits<std::size_t>::max(),effects{};
    std::function<void(const std::string&)> onEvent;
    void Event(std::string event) {
        events.push_back(std::move(event));++effects;
        if(onEvent)onEvent(events.back());
        if(events.size()-1u==failProvider)throw std::runtime_error("caller native query failed");
    }
    Queries Query() {
        Queries q;
        q.work=[&]{if(work++>=limit)throw std::runtime_error("same caller work exhausted");};
        q.nativeLevel=[&](Id source) {
            Require(source==actor,"LastRendered native edge changed Actor");Event("level");return edge;
        };
        q.lastRenderTime=[&](Id source) {
            Require(source==actor,"LastRendered timestamp changed Actor");Event("last");return last;
        };
        q.nativeTime=[&](Id source) {
            Require(source==level||source==otherLevel,"LastRendered native time used an invented ULevel");
            Event("time"+std::to_string(source));return times.at(source);
        };
        return q;
    }
    QuestVr::OriginalActorLastRenderedResult Run() {return QuestVr::QueryOriginalActorLastRendered(actor,Query());}
};
void ExactOrderAndArithmetic() {
    Fixture f;const auto initial=f.Run();
    Require(initial.returned==10.0f&&!initial.clamped&&initial.steps==3u&&initial.levelReads==1u&&
        initial.lastRenderReads==1u&&initial.nativeTimeReads==1u&&f.work==3u&&
        f.events==std::vector<std::string>{"level","last","time512"},"LastRendered source order/counters changed");
    Require(f.last==-10.0f&&f.times.at(level)==0.0,"LastRendered marked the actor rendered or advanced its clock");
    f=Fixture{};f.last=4.0f;f.times[level]=2.0;auto result=f.Run();
    Require(result.clamped&&Bits(result.returned)==0u,"Negative elapsed time was not clamped to positive zero");
    f=Fixture{};f.last=2.0f;f.times[level]=2.0;result=f.Run();
    Require(!result.clamped&&Bits(result.returned)==0u,"Equal timestamps were treated as strictly negative");
    f=Fixture{};f.last=0.0f;f.times[level]=-0.0;result=f.Run();
    Require(!result.clamped&&Bits(result.returned)==0x80000000u,"Nonnegative test erased native negative zero result");
    f=Fixture{};f.last=-0.0f;f.times[level]=0.0;result=f.Run();
    Require(!result.clamped&&Bits(result.returned)==0u,"Positive-minus-negative zero sign changed");
    f=Fixture{};f.last=0.0f;f.times[level]=-Double(1u);result=f.Run();
    Require(result.clamped&&Bits(result.returned)==0u,"Negative retained Double subnormal was Float-spilled before C0 clamp");
    f=Fixture{};f.last=0.0f;f.times[level]=Double(1u);result=f.Run();
    Require(!result.clamped&&Bits(result.returned)==0u,"Positive retained Double subnormal was treated as negative");
    f=Fixture{};f.last=-0x1p-63f;f.times[level]=1.0+0x1p-24;result=f.Run();
    Require(!result.clamped&&Bits(result.returned)==0x3f800001u,
        "PC64 age was rounded through a host Double before final Float store");
    f=Fixture{};f.last=0.0f;f.times[level]=std::numeric_limits<double>::max();result=f.Run();
    Require(!result.clamped&&Bits(result.returned)==0x7f800000u,"Original positive Float overflow was refused or saturated");
    for(std::uint32_t word:{0x7fc12345u,0xffc12345u,0x7f812345u,0xff812345u}) {
        f=Fixture{};f.last=Float(word);result=f.Run();
        Require(result.clamped&&Bits(result.returned)==0u&&f.events.back()=="time512",
            "NaN LastRenderTime skipped native time access or did not C0-clamp to positive zero");
    }
    for(std::uint64_t word:{0x7ff8000000012345ull,0xfff8000000012345ull,0x7ff0000000012345ull,0xfff0000000012345ull}) {
        f=Fixture{};f.times[level]=Double(word);result=f.Run();
        Require(result.clamped&&Bits(result.returned)==0u,"NaN native Double clock was rejected or escaped C0 clamp");
    }
    f=Fixture{};
    f.onEvent=[&](const std::string& event) {
        if(event=="last") {f.edge=otherLevel;f.last=-2.0f;}
        if(event=="time512") {f.last=1000.0f;f.times[level]=4.0;}
    };
    result=f.Run();
    Require(result.returned==6.0f&&!result.clamped&&f.events==std::vector<std::string>{"level","last","time512"},
        "LastRendered refreshed captured native XLevel/last value or eagerly captured the clock");
}
void LazyErrorsAndSharedRoot() {
    Fixture null;
    Refuses([&]{QuestVr::QueryOriginalActorLastRendered(0u,null.Query());},"Null LastRendered Actor accepted");
    Require(null.events.empty()&&!null.work,"Null LastRendered Actor demanded providers/work");
    Fixture noWork;auto q=noWork.Query();q.work={};
    Refuses([&]{QuestVr::QueryOriginalActorLastRendered(actor,q);},"Unbudgeted LastRendered accepted");
    Require(noWork.events.empty()&&!noWork.effects,"Missing caller work ran LastRendered providers");
    const std::vector<std::string> expected{"level","last","time512"};
    for(std::size_t missing=0u;missing<3u;++missing) {
        Fixture f;q=f.Query();
        if(missing==0u)q.nativeLevel={};
        if(missing==1u)q.lastRenderTime={};
        if(missing==2u)q.nativeTime={};
        Refuses([&]{QuestVr::QueryOriginalActorLastRendered(actor,q);},"Missing reached LastRendered provider accepted");
        Require(f.work==missing&&f.events==std::vector<std::string>(expected.begin(),expected.begin()+
            static_cast<std::ptrdiff_t>(missing)),"Missing LastRendered provider was demanded eagerly or skipped prior reads");
    }
    Fixture nullLevel;nullLevel.edge=0u;q=nullLevel.Query();q.nativeTime={};
    Refuses([&]{QuestVr::QueryOriginalActorLastRendered(actor,q);},"Null actual native XLevel accepted");
    Require(nullLevel.work==2u&&nullLevel.events==std::vector<std::string>{"level","last"},
        "Null native edge skipped preceding FLD timestamp or attempted an invented native clock");
    Fixture nan;nan.last=Float(0x7fc12345u);q=nan.Query();q.nativeTime={};
    Refuses([&]{QuestVr::QueryOriginalActorLastRendered(actor,q);},"NaN timestamp bypassed reached native clock authority");
    Require(nan.events==std::vector<std::string>{"level","last"},"NaN timestamp eagerly returned before the native Double memory operand");
    for(std::size_t cut=0u;cut<expected.size();++cut) {
        Fixture f;f.limit=cut;
        Refuses([&]{f.Run();},"LastRendered caller work failure was swallowed");
        Require(f.work==cut+1u&&f.events==std::vector<std::string>(expected.begin(),expected.begin()+
            static_cast<std::ptrdiff_t>(cut)),"LastRendered did not charge work before its exact provider");
        Fixture provider;provider.failProvider=cut;
        Refuses([&]{provider.Run();},"LastRendered reached provider failure was swallowed");
        Require(provider.work==cut+1u&&provider.effects==cut+1u&&provider.events==std::vector<std::string>(expected.begin(),
            expected.begin()+static_cast<std::ptrdiff_t>(cut+1u)),"LastRendered lost the failed provider's observable prefix");
    }
    // This deliberately models an outer CALLER root, not production-journal
    // proof. Prior provider effects must not be rolled back by this helper.
    Fixture local;local.limit=2u;
    local.onEvent=[&](const std::string& event){if(event=="last"){local.last=8.0f;local.times[level]=123.0;}};
    Refuses([&]{local.Run();},"Late LastRendered work failure accepted");
    Require(local.last==8.0f&&local.times.at(level)==123.0&&local.effects==2u,
        "LastRendered invented local rollback of caller-owned provider effects");
    for(bool providerFailure:{false,true}) {
        Fixture root;const auto beforeLast=root.last;const auto beforeTimes=root.times;
        if(providerFailure)root.failProvider=2u;else root.limit=2u;
        root.onEvent=[&](const std::string& event){if(event=="last"){root.last=8.0f;root.times[level]=123.0;}};
        bool failed{};
        try{root.Run();}catch(const std::exception&) {
            failed=true;root.last=beforeLast;root.times=beforeTimes;root.effects=0u;
        }
        Require(failed&&Bits(root.last)==Bits(beforeLast)&&root.times==beforeTimes&&!root.effects,
            "Mock caller root did not restore a late LastRendered provider/work failure");
    }
}

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
struct Precision {
    unsigned short saved{};
    Precision() {
        static_assert(std::numeric_limits<long double>::digits==64,"LastRendered oracle requires actual GNU x87 PC64");
        asm volatile("fnstcw %0":"=m"(saved));
        const auto selected=static_cast<unsigned short>((saved&~0x0f00u)|0x033fu);
        asm volatile("fnclex; fldcw %0"::"m"(selected):"memory");
    }
    ~Precision(){asm volatile("fnclex; fldcw %0"::"m"(saved):"memory");}
};
struct OracleResult {float returned{};bool clamped{};};
OracleResult Oracle(double now,float last) {
    // Independent own machine instructions, never commercial executable code
    // and never the production scalar helper. FSUBR consumes raw m64 time.
    const float zero=0.0f;long double age;unsigned short status;
    asm volatile("flds %3; fsubrl %2; fcoms %4; fnstsw %%ax; fstpt %0":
        "=m"(age),"=a"(status):"m"(now),"m"(last),"m"(zero):"st","memory");
    OracleResult out;out.clamped=(status&0x0100u)!=0u;
    if(!out.clamped)asm volatile("fldt %1; fstps %0":"=m"(out.returned):"m"(age):"st","memory");
    return out;
}
void CompareOracle(double now,float last) {
    Fixture f;f.last=last;f.times[level]=now;
    const auto expected=Oracle(now,last);const auto actual=f.Run();++oracleCases;
    if(Bits(actual.returned)!=Bits(expected.returned)||actual.clamped!=expected.clamped) {
        std::cerr<<"LastRendered oracle mismatch now=0x"<<std::hex<<std::bit_cast<std::uint64_t>(now)
            <<" last=0x"<<Bits(last)<<" actual=0x"<<Bits(actual.returned)<<" expected=0x"<<Bits(expected.returned)
            <<std::dec<<'\n';throw std::runtime_error("LastRendered differs from independent x87 PC64 oracle");
    }
    Require(actual.steps==3u&&f.work==3u&&f.events==std::vector<std::string>{"level","last","time512"},
        "LastRendered numeric path skipped reached caller work/providers");
    ++checks;
}
void ArithmeticOracle() {
    Precision precision;
    const std::array<std::uint32_t,24u> lastWords{0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,
        0x3f000000u,0xbf000000u,0x3f7fffffu,0x3f800000u,0x3f800001u,0xbf800000u,0xc1200000u,
        0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00000u,0xffc12345u,0x7f800001u,
        0xff812345u,0x20000000u,0xa0000000u,0x4b800001u};
    const std::array<std::uint64_t,26u> timeWords{0ull,0x8000000000000000ull,1ull,0x8000000000000001ull,
        0x000fffffffffffffull,0x0010000000000000ull,0x3fe0000000000000ull,0xbfe0000000000000ull,
        0x3fefffffffffffffull,0x3ff0000000000000ull,0x3ff0000000000001ull,0xbff0000000000000ull,
        0x4024000000000000ull,0xc024000000000000ull,0x7fefffffffffffffull,0xffefffffffffffffull,
        0x7ff0000000000000ull,0xfff0000000000000ull,0x7ff8000000000000ull,0xfff8123456789abcull,
        0x7ff0000000000001ull,0xfff0123456789abcull,0x3ff0000010000000ull,0x3690000000000000ull,
        0x47efffffe0000000ull,0x47effffff0000000ull};
    for(auto now:timeWords)for(auto last:lastWords)CompareOracle(Double(now),Float(last));
    CompareOracle(1.0+0x1p-24,-0x1p-63f);
    CompareOracle(-(1.0+0x1p-24),0x1p-63f);
    std::uint64_t seed=0x1036de30103021daull;
    const auto random=[&](){seed^=seed<<13u;seed^=seed>>7u;seed^=seed<<17u;return seed;};
    for(unsigned i=0u;i<32768u;++i) {
        const auto now=random();const auto last=static_cast<std::uint32_t>(random());
        CompareOracle(Double(now),Float(last));
    }
}
#else
void ArithmeticOracle(){throw std::runtime_error("LastRendered acceptance requires independent GNU x87 PC64; no skipped oracle");}
#endif
} // namespace
int main() {
    try {
        ExactOrderAndArithmetic();LazyErrorsAndSharedRoot();ArithmeticOracle();
        std::cout<<"Original LastRendered723: "<<checks<<" checks / "<<refusals<<" refusals / "<<oracleCases
            <<" independent x87 cases; native query foundation only, NOT renderer publication, reached NPC Tick or first Training acceptance\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
