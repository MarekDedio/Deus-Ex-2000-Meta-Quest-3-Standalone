#include "quest_actor_timer.h"
#include <iostream>
#include <string>

namespace {
unsigned checks{};
void Require(bool value,const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
std::uint32_t Raw(float value) { std::uint32_t bits; std::memcpy(&bits,&value,4u); return bits; }
float Float(std::uint32_t bits) { float value; std::memcpy(&value,&bits,4u); return value; }
bool Same(const QuestVr::ActorTimerStep& a,const QuestVr::ActorTimerStep& b) {
    return Raw(a.rate)==Raw(b.rate) && Raw(a.counter)==Raw(b.counter) && a.loop==b.loop && a.expired==b.expired;
}
void Refuses(float rate,float counter,bool loop,float elapsed) {
    const auto rateBits=Raw(rate), counterBits=Raw(counter), elapsedBits=Raw(elapsed);
    bool refused{};
    try { static_cast<void>(QuestVr::AdvanceActorTimer(rate,counter,loop,elapsed)); }
    catch (const std::runtime_error&) { refused=true; }
    Require(refused,"Unsafe timer input was accepted");
    Require(Raw(rate)==rateBits && Raw(counter)==counterBits && Raw(elapsed)==elapsedBits,
        "Refusal mutated input fields");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct RetailPrecisionOracle {
    unsigned short original{};
    RetailPrecisionOracle() {
        asm volatile("fnstcw %0" : "=m"(original));
        const unsigned short pc64Nearest=static_cast<unsigned short>((original&~0x0f00u)|0x0300u);
        asm volatile("fldcw %0" : : "m"(pc64Nearest));
    }
    ~RetailPrecisionOracle() { asm volatile("fldcw %0" : : "m"(original)); }
};
// Independent host x87 operations, not original DLL execution or helper math.
// Volatile extended stores keep the original PC64 sum alive across the float
// store, then perform actual x87 division/multiply/subtract at that precision.
QuestVr::ActorTimerStep Oracle(float rate,float counter,bool loop,float elapsed) {
    QuestVr::ActorTimerStep out{rate,counter,loop,false};
    if (rate <= 0.0f) return out;
    volatile long double r=rate, c=counter, e=elapsed;
    volatile long double sum=c+e;
    out.counter=static_cast<float>(sum);
    if (!std::isfinite(out.counter)) throw std::runtime_error("Oracle counter overflow");
    if (sum < r) return out;
    volatile long double quotient=sum/r;
    if (quotient >= 2147483648.0L || quotient < 1.0L) throw std::runtime_error("Oracle count overflow");
    const auto count=static_cast<std::int32_t>(quotient);
    volatile long double product=static_cast<long double>(count)*r;
    volatile long double remainder=sum-product;
    out.counter=static_cast<float>(remainder);
    if (!std::isfinite(out.counter)) throw std::runtime_error("Oracle remainder overflow");
    if (!loop) out.rate=0.0f;
    out.expired=true;
    return out;
}
void CheckOracle(float rate,float counter,bool loop,float elapsed) {
    bool actualRefused{}, oracleRefused{};
    QuestVr::ActorTimerStep actual, expected;
    try { actual=QuestVr::AdvanceActorTimer(rate,counter,loop,elapsed); } catch (const std::runtime_error&) { actualRefused=true; }
    try { expected=Oracle(rate,counter,loop,elapsed); } catch (const std::runtime_error&) { oracleRefused=true; }
    if (actualRefused != oracleRefused || (!actualRefused && !Same(actual,expected))) {
        std::cerr << std::hex << "Oracle mismatch rate=" << Raw(rate) << " counter=" << Raw(counter)
            << " elapsed=" << Raw(elapsed) << " actual=" << Raw(actual.counter) << " expected=" << Raw(expected.counter) << std::dec << '\n';
        throw std::runtime_error("PC64 x87 oracle mismatch");
    }
    ++checks;
}
#endif
} // namespace

int main() {
    try {
        using QuestVr::AdvanceActorTimer;
        auto step=AdvanceActorTimer(1.0f,0.75f,true,0.25f);
        Require(step.expired && step.rate==1.0f && step.counter==0.0f && step.loop,"Exact equality did not fire looping timer");
        step=AdvanceActorTimer(1.0f,0.25f,false,0.25f);
        Require(!step.expired && step.counter==0.5f && step.rate==1.0f,"Subthreshold timer changed rate or fired");
        step=AdvanceActorTimer(1.0f,0.75f,true,2.5f);
        Require(step.expired && step.counter==0.25f && step.rate==1.0f,"Missed periods were not removed/coalesced");
        step=AdvanceActorTimer(1.0f,0.75f,false,2.5f);
        Require(step.expired && step.rate==0.0f && step.counter==0.25f && !step.loop,"One-shot callback-ready state lost remainder/disable");
        step=AdvanceActorTimer(1.0f,2.5f,false,0.0f);
        Require(step.expired && step.rate==0.0f && step.counter==0.5f,"Already-due zero elapsed timer did not fire");
        const auto below=Float(0x3f7fffffu), halfGap=Float(0x33000000u); // 1-2^-24, 2^-25.
        step=AdvanceActorTimer(1.0f,below,true,halfGap);
        Require(!step.expired && Raw(step.counter)==Raw(1.0f),"Binary32 store was wrongly used for live expiration comparison");
        step=AdvanceActorTimer(1.0f,1.0f,false,halfGap);
        Require(step.expired && Raw(step.counter)==Raw(halfGap) && step.rate==0.0f,
            "Remainder was computed from rounded stored counter instead of live PC64 sum");
        const float pc64Half=Float(0x1f800000u); // 2^-64, half the PC64 ULP at1.
        Require(Raw(AdvanceActorTimer(1.0f,1.0f,true,pc64Half).counter)==0u,
            "PC64 addition half-ULP did not round to even");
        Require(Raw(AdvanceActorTimer(1.0f,1.0f,true,Float(0x1f800001u)).counter)==0x20000000u,
            "PC64 addition above half-ULP did not retain the extended residual");
        Require(Raw(AdvanceActorTimer(1.0f,1.0f,true,3.0f*pc64Half).counter)==0x20800000u,
            "PC64 odd-significand half tie did not round to the even neighbor");
        Require(AdvanceActorTimer(1.0f,-0.75f,true,0.5f).counter==-0.25f,"Negative reflected counter was clamped");
        step=AdvanceActorTimer(1.0f,-1.0f,true,1.0f);
        Require(!step.expired && Raw(step.counter)==0u,"Exact cancellation did not produce positive zero");
        Require(Raw(AdvanceActorTimer(1.0f,-below,true,1.0f).counter)==0x33800000u,
            "Negative reflected counter near cancellation lost exact residual");
        Require(Raw(AdvanceActorTimer(1.0f,-1.0f,true,Float(0x3f800001u)).counter)==0x34000000u,
            "Opposite near-cancellation lost exact residual");
        step=AdvanceActorTimer(1.0f,Float(0x80000000u),true,Float(0x80000000u));
        Require(Raw(step.counter)==0x80000000u,"Negative-zero plus negative-zero lost sign");
        Require(Raw(AdvanceActorTimer(1.0f,Float(0x80000000u),false,0.0f).counter)==0u,
            "Opposite signed zeros did not produce positive zero");
        for (float rate : {0.0f,Float(0x80000000u),-0.25f,-100.0f}) {
            const QuestVr::ActorTimerStep raw{rate,Float(0x80000000u),true,false};
            Require(Same(AdvanceActorTimer(rate,raw.counter,true,100.0f),raw),"Inactive signed rate/counter/loop raw bits changed");
        }
        const float tiny=std::numeric_limits<float>::denorm_min();
        Require(AdvanceActorTimer(tiny,0.0f,false,tiny).expired,"Smallest subnormal exact equality failed");
        Require(Raw(AdvanceActorTimer(1.0f,tiny,true,tiny).counter)==2u,"Subnormal counter sum changed");
        Require(Raw(AdvanceActorTimer(2.0f,1.0f,true,tiny).counter)==Raw(1.0f),"Disparate exponent sum was not PC64 rounded");
        step=AdvanceActorTimer(1.0f,2147483520.0f,true,127.0f);
        Require(step.expired && step.counter==0.0f,"Signed32 maximum whole period count was refused/changed");
        Refuses(1.0f,2147483520.0f,true,128.0f);
        Refuses(tiny,0.0f,true,std::numeric_limits<float>::max());
        Refuses(1.0f,std::numeric_limits<float>::max(),true,std::numeric_limits<float>::max());
        Refuses(1.0f,0.0f,true,-tiny);
        for (float bad : {std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()}) {
            Refuses(bad,0.0f,false,0.0f); Refuses(1.0f,bad,false,0.0f); Refuses(0.0f,0.0f,false,bad);
        }
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        static_assert(std::numeric_limits<long double>::digits == 64,"x87 oracle requires extended significand64");
        RetailPrecisionOracle precision;
        CheckOracle(1.0f,1.0f,true,pc64Half);
        CheckOracle(1.0f,1.0f,true,Float(0x1f800001u));
        CheckOracle(1.0f,1.0f,true,3.0f*pc64Half);
        CheckOracle(3.0f,-3.0f*Float(0x20000000u),true,6.0f);
        CheckOracle(1.0f,-std::numeric_limits<float>::max(),true,std::numeric_limits<float>::max());
        for (float rate : {tiny,Float(0x007fffffu),0.1f,1.0f,3.0f,Float(0x7f000000u)})
            for (float counter : {-3.0f,-tiny,Float(0x80000000u),0.0f,tiny,below,1.0f,3.0f})
                for (float elapsed : {Float(0x80000000u),0.0f,tiny,halfGap,0.5f,3.0f})
                    CheckOracle(rate,counter,true,elapsed);
        std::uint32_t random=0x5eed0212u;
        const auto next=[&]() { random^=random<<13u; random^=random>>17u; random^=random<<5u; return random; };
        for (unsigned i=0u;i<20000u;++i) {
            const float rate=Float((next()&0x7fffffffu)%0x7f800000u);
            const auto rawCounter=next();
            const float counter=Float((rawCounter&0x80000000u)|((rawCounter&0x7fffffffu)%0x7f800000u));
            const float elapsed=Float((next()&0x7fffffffu)%0x7f800000u);
            CheckOracle(rate,counter,(i&1u)!=0u,elapsed);
        }
        std::cout << "Independent PC64 x87 oracle triples: 20293\n";
#else
        std::cout << "Independent x87 oracle unavailable on this architecture; deterministic fixtures only\n";
#endif
        std::cout << "Retail actor timer tests passed: " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
