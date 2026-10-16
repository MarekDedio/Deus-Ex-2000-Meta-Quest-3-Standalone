#include "quest_make_noise.h"

#include <bit>
#include <iostream>
#include <limits>
#include <vector>

// Detached arithmetic controls only: no original DLL execution, Pawn-list
// routing, gameplay clock, callbacks, BSP implementation or playable-world claim.
namespace {
using Vec=PortableModelVec3;
using Cache=QuestVr::OriginalNoiseCache;
using CacheDecision=QuestVr::OriginalNoiseCacheDecision;
using HearDecision=QuestVr::OriginalCanHearDecision;
using QuestVr::DecideOriginalNoiseCaches;
using QuestVr::EvaluateOriginalCanHear;
std::size_t checks{},refusals{};
void Require(const bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
std::uint32_t Bits(const float value) {return std::bit_cast<std::uint32_t>(value);}
template<class Operation> void Refuse(Operation operation) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,"Unsafe MakeNoise/CanHear scalar arithmetic silently succeeded");++refusals;
}
void Same(const CacheDecision& actual,const CacheDecision& expected) {
    Require(actual.suppressed==expected.suppressed&&actual.replacement==expected.replacement,
        "Original MakeNoise cache suppression/replacement differs");
}
void Same(const HearDecision& actual,const HearDecision& expected) {
    Require(actual.eligible==expected.eligible&&actual.needsBsp==expected.needsBsp&&
        actual.writeStimulus==expected.writeStimulus&&Bits(actual.stimulus)==Bits(expected.stimulus),
        "Original CanHear scalar gates/Stimulus differ");
}
void LazyReaders() {
    std::vector<unsigned> reads;
    const auto suppressed=QuestVr::DecideOriginalNoiseCachesLazy(1.0,1.0f,{},
        [&](std::size_t i) {Require(i==0u,"Suppression read cache2 time");reads.push_back(0u);return 1.0f;},
        [&](std::size_t i) {Require(i==0u,"Suppression read cache2 spot");reads.push_back(1u);return Vec{};},
        [&](std::size_t i) {Require(i==0u,"Suppression read cache2 loudness");reads.push_back(2u);return 1.0f;});
    Same(suppressed,{true,{}});Require(reads==std::vector<unsigned>{0u,1u,2u},"Cache1 suppression did not preserve lazy read order");
    reads.clear();
    const auto stale=QuestVr::DecideOriginalNoiseCachesLazy(1.0,1.0f,{},
        [&](std::size_t i) {reads.push_back(static_cast<unsigned>(i));return 0.0f;},
        [&](std::size_t) -> Vec {throw std::runtime_error("Old timestamp evaluated unused spot");},
        [&](std::size_t) -> float {throw std::runtime_error("Old timestamp evaluated unused loudness");});
    Same(stale,{false,0u});Require(reads==std::vector<unsigned>{0u,1u},"Stale replacement reread timestamp or evaluated unrelated fields");
}
void CacheBoundaries() {
    std::array<Cache,2u> caches{{{1.0f,1.0f,{}},{1.0f,2.0f,{100.0f,0.0f,0.0f}}}};
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{true,{}});
    caches[0u].spot.x=100.0f;caches[1u].spot={};caches[1u].loudness=1.0f;
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{true,{}}); // cache2 suppresses.
    caches[1u].spot.x=100.0f;caches[1u].loudness=2.0f;caches[0u].spot.x=50.0f;
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{}); // strict distance2500.
    caches[0u].spot.x=std::nextafter(50.0f,0.0f);
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{true,{}});
    // The retained sum is just below2500 but would Float-round to2500.
    // Cache proximity must not acquire CanHear's unrelated Float spill.
    caches[0u].spot={0.0f,std::nextafter(50.0f,0.0f),9.0f/512.0f};
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{true,{}});
    caches[0u]={0.0f,1.0f,{}};caches[1u]={0.0f,2.0f,{100.0f,0.0f,0.0f}};
    const auto recent=std::bit_cast<double>(std::uint64_t{0x3fc999999999999aull});
    Same(DecideOriginalNoiseCaches(recent,1.0f,{},caches),{false,0u});
    Same(DecideOriginalNoiseCaches(std::nextafter(recent,0.0),1.0f,{},caches),{true,{}});
    Same(DecideOriginalNoiseCaches(std::nextafter(recent,1.0),1.0f,{},caches),{false,0u});
    caches[0u].time=1.0f;
    Same(DecideOriginalNoiseCaches(1.2,1.0f,{},caches),{true,{}});
    const volatile float incorrectFloatCutoff=static_cast<float>(1.2)-.2f;
    Require(incorrectFloatCutoff==caches[0u].time,
        "Native Double time boundary does not distinguish an incorrect Float clock/window");
    caches[0u]={1.0f,.9f,{}};caches[1u].time=1.0f;
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{false,0u});
    caches[0u].loudness=std::nextafter(.9f,1.0f);
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{true,{}});
    caches[0u].loudness=-.9f;
    Same(DecideOriginalNoiseCaches(1.0,-1.0f,{},caches),{true,{}});
    caches[0u].loudness=0.0f;
    Same(DecideOriginalNoiseCaches(1.0,0.0f,{},caches),{true,{}});
    for(auto& cache:caches) cache={0.0f,2.0f,{100.0f,0.0f,0.0f}};
    const auto stale=std::bit_cast<double>(std::uint64_t{0x3fc70a3d70a3d70aull});
    Same(DecideOriginalNoiseCaches(stale,1.0f,{},caches),{}); // strict stale equality.
    Same(DecideOriginalNoiseCaches(std::nextafter(stale,1.0),1.0f,{},caches),{false,0u});
    caches[0u].time=1.0f;
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{false,1u});
    caches[1u].time=1.0f;caches[0u].spot={};caches[0u].loudness=.5f;
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{false,0u});
    caches[0u].spot.x=100.0f;caches[0u].loudness=2.0f;caches[1u].loudness=1.0f;
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{false,0u}); // cache2 loudness writes cache1!
    caches[1u].loudness=2.0f;
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{}); // no write, still route.
    const auto maximum=std::numeric_limits<float>::max();
    for(auto& cache:caches) cache.spot.x=-maximum;
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{maximum,0.0f,0.0f},caches),{});
    // Cache geometry has no Float spill: a huge finite squared distance is a
    // valid not-near result, unlike CanHear's mandatory Float distance store.
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    caches[0u]={1.0f,1.0f,{}};caches[1u].time=nan;
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{true,{}});
    caches[0u]={0.0f,nan,{nan,nan,nan}};caches[1u]={0.0f,nan,{nan,nan,nan}};
    Same(DecideOriginalNoiseCaches(1.0,1.0f,{},caches),{false,0u});
}
void HearingBoundaries() {
    Same(EvaluateOriginalCanHear({}, {},0.0f,0.0f,1000.0f,false),{true,false,false,0.0f});
    Same(EvaluateOriginalCanHear({}, {},0.0f,0.0f,1000.0f,true),{true,true,false,0.0f});
    const auto tiny=std::numeric_limits<float>::denorm_min();
    Same(EvaluateOriginalCanHear({}, {tiny,tiny,tiny},0.0f,0.0f,1000.0f,false),{true,false,false,0.0f});
    const auto atRange=EvaluateOriginalCanHear({}, {2000.0f,0.0f,0.0f},1.0f,0.0f,0.0f,false);
    Require(atRange.eligible&&atRange.needsBsp&&atRange.writeStimulus,"Inclusive general range rejected equality");
    Same(EvaluateOriginalCanHear({}, {std::nextafter(2000.0f,3000.0f),0.0f,0.0f},1.0f,0.0f,0.0f,false),{});
    // Retained geometry is >4000000, but its mandatory Float spill is equal.
    Same(EvaluateOriginalCanHear({}, {2000.0f,.25f,0.0f},1.0f,0.0f,0.0f,false),atRange);
    Same(EvaluateOriginalCanHear({}, {4000.0f,0.0f,0.0f},1.0f,0.0f,1000.0f,true),{true,true,false,0.0f});
    Same(EvaluateOriginalCanHear({}, {std::nextafter(4000.0f,5000.0f),0.0f,0.0f},1.0f,0.0f,0.0f,true),{});
    Same(EvaluateOriginalCanHear({}, {3000.0f,0.0f,0.0f},1.0f,0.0f,0.0f,false),{});
    Same(EvaluateOriginalCanHear({}, {1.0f,0.0f,0.0f},1.0f,2.0f,3.0f,false),{true,true,true,3.0f});
    Same(EvaluateOriginalCanHear({}, {1.0f,0.0f,0.0f},1.0f,2.0f,std::nextafter(3.0f,4.0f),false),{false,false,true,3.0f});
    Same(EvaluateOriginalCanHear({}, {1.0f,0.0f,0.0f},-1.0f,0.0f,-2.0f,false),{true,true,true,-2.0f});
    // q is retained1.2, but p must reload its Float spill before multiplication.
    Same(EvaluateOriginalCanHear({}, {1000.0f,0.0f,0.0f},1.0f,0.0f,1.2f,false),{true,true,true,1.2f});
    const auto loudness=std::nextafter(1.0f,0.0f);
    const auto alertness=std::nextafter(std::ldexp(1.0f,-22),0.0f);
    const auto below=EvaluateOriginalCanHear({}, {1.0f,0.0f,0.0f},loudness,alertness,2.0f,false);
    Same(below,{false,false,true,2.0f}); // retained2-2^-47, stored2: compare BEFORE reload.
    Same(EvaluateOriginalCanHear({}, {1.0f,0.0f,0.0f},loudness,alertness,
        std::nextafter(2.0f,0.0f),false),{true,true,true,2.0f});
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    Same(EvaluateOriginalCanHear({}, {},1.0f,nan,nan,false),{true,false,false,0.0f});
    Same(EvaluateOriginalCanHear({}, {1.0f,0.0f,0.0f},1.0f,nan,nan,true),{true,true,false,0.0f});
    Same(EvaluateOriginalCanHear({}, {3000.0f,0.0f,0.0f},1.0f,nan,nan,false),{});
}
void Safety() {
    const auto maximum=std::numeric_limits<float>::max();
    std::array<Cache,2u> caches{{{1.0f,1.0f,{}},{1.0f,1.0f,{}}}};
    for(const auto invalid:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()}) {
        Refuse([&]{static_cast<void>(DecideOriginalNoiseCaches(1.0,invalid,{},caches));});
        Refuse([&]{static_cast<void>(EvaluateOriginalCanHear({}, {},invalid,0.0f,0.0f,false));});
        for(unsigned slot=0u;slot<6u;++slot) {
            Vec source{},target{};float* fields[]{&source.x,&source.y,&source.z,&target.x,&target.y,&target.z};
            *fields[slot]=invalid;
            Refuse([&]{static_cast<void>(EvaluateOriginalCanHear(source,target,1.0f,0.0f,0.0f,false));});
        }
        Refuse([&]{static_cast<void>(EvaluateOriginalCanHear({}, {1.0f,0.0f,0.0f},1.0f,invalid,0.0f,false));});
        Refuse([&]{static_cast<void>(EvaluateOriginalCanHear({}, {1.0f,0.0f,0.0f},1.0f,0.0f,invalid,false));});
        auto broken=caches;broken[0u].time=invalid;
        Refuse([&]{static_cast<void>(DecideOriginalNoiseCaches(1.0,1.0f,{},broken));});
        broken=caches;broken[0u].loudness=invalid;
        Refuse([&]{static_cast<void>(DecideOriginalNoiseCaches(1.0,1.0f,{},broken));});
        broken=caches;broken[0u].spot.x=invalid;
        Refuse([&]{static_cast<void>(DecideOriginalNoiseCaches(1.0,1.0f,{},broken));});
    }
    for(const auto invalid:{-1.0,std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()})
        Refuse([&]{static_cast<void>(DecideOriginalNoiseCaches(invalid,1.0f,{},caches));});
    Refuse([&]{static_cast<void>(DecideOriginalNoiseCaches(std::numeric_limits<double>::max(),1.0f,{},caches));});
    Refuse([&]{static_cast<void>(EvaluateOriginalCanHear({}, {maximum,0.0f,0.0f},1.0f,0.0f,0.0f,false));});
    Refuse([&]{static_cast<void>(EvaluateOriginalCanHear({}, {1.0f,0.0f,0.0f},maximum,maximum,0.0f,false));});
    // A nonzero stored distance can make the mandatory q Float spill overflow
    // even though the later q>2 branch would otherwise choose a finite cap.
    Refuse([&]{static_cast<void>(EvaluateOriginalCanHear({}, {std::ldexp(1.0f,-70),0.0f,0.0f},1.0f,0.0f,0.0f,false));});
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct Precision {
    unsigned short original{};
    Precision() {
        static_assert(std::numeric_limits<long double>::digits==64);
        asm volatile("fnstcw %0":"=m"(original));
        const unsigned short mode=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fldcw %0"::"m"(original));}
};
// Independent hardware PC64 operations, deliberately not production dyadics.
// volatile long-double operands/results force each x87 instruction boundary.
long double Add(long double a,long double b) {volatile long double x=a,y=b,z=x+y;return z;}
long double Sub(long double a,long double b) {volatile long double x=a,y=b,z=x-y;return z;}
long double Mul(long double a,long double b) {volatile long double x=a,y=b,z=x*y;return z;}
long double Div(long double a,long double b) {volatile long double x=a,y=b,z=x/y;return z;}
float Store(long double value) {
    volatile float result=static_cast<float>(value);
    if(!std::isfinite(result)) throw std::runtime_error("Independent x87 Float overflow");
    return result;
}
void Finite(float value) {if(!std::isfinite(value)) throw std::runtime_error("Independent nonfinite input");}
long double Distance(const Vec& target,const Vec& source) {
    for(const auto value:{target.x,target.y,target.z,source.x,source.y,source.z}) Finite(value);
    const auto x=Sub(target.x,source.x),y=Sub(target.y,source.y),z=Sub(target.z,source.z);
    return Add(Add(Mul(x,x),Mul(y,y)),Mul(z,z));
}
CacheDecision CacheOracle(const double now,const float loudness,const Vec& source,const std::array<Cache,2u>& caches) {
    if(!std::isfinite(now)||now<0.0) throw std::runtime_error("Independent bad native time");
    Finite(loudness);for(const auto value:{source.x,source.y,source.z}) Finite(value);
    const auto recent=Sub(now,std::bit_cast<double>(std::uint64_t{0x3fc999999999999aull}));
    const auto factor=Mul(loudness,std::bit_cast<double>(std::uint64_t{0x3feccccccccccccdull}));
    const auto near=[&](std::size_t i) {return Distance(caches[i].spot,source)<2500.0f;};
    for(std::size_t i=0u;i<2u;++i) {
        Finite(caches[i].time);
        if(caches[i].time>recent&&near(i)) {
            Finite(caches[i].loudness);
            if(caches[i].loudness>=factor) return {true,{}};
        }
    }
    const auto replacement=[&](std::size_t i) {static_cast<void>(Store(now));return CacheDecision{false,i};};
    const auto stale=Sub(now,std::bit_cast<double>(std::uint64_t{0x3fc70a3d70a3d70aull}));
    if(caches[0u].time<stale) return replacement(0u);
    if(caches[1u].time<stale) return replacement(1u);
    if(near(0u)) {Finite(caches[0u].loudness);if(caches[0u].loudness<=loudness) return replacement(0u);}
    Finite(caches[1u].loudness);
    return caches[1u].loudness<=loudness?replacement(0u):CacheDecision{};
}
HearDecision HearOracle(const Vec& noise,const Vec& listener,float loudness,float alertness,float threshold,bool team) {
    Finite(loudness);
    const auto distance=Store(Distance(listener,noise));
    const auto range=Mul(Mul(loudness,loudness),team?16000000.0f:4000000.0f);
    if(distance>range) return {};
    if(team) return {true,true,false,0.0f};
    if(distance==0.0f) return {true,false,false,0.0f};
    const auto q=Div(1200000.0f,distance);const float stored=Store(q);
    const auto p=q>2.0f?2.0f:stored,a=p>0.5f?0.5f:p;
    Finite(alertness);Finite(threshold);
    const auto alert=Mul(a,alertness),strength=Mul(p,loudness),sum=Add(alert,strength);
    const auto stimulus=Store(sum);const bool accepted=sum>=threshold;
    return {accepted,accepted,true,stimulus};
}
std::uint64_t Next(std::uint64_t& state) {state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
float Number(std::uint64_t& state) {
    const auto raw=static_cast<std::uint32_t>(Next(state));
    return std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u));
}
float Bounded(std::uint64_t& state,const float divisor=64.0f) {
    return static_cast<float>(static_cast<int>(Next(state)%16385u)-8192)/divisor;
}
void CheckCache(const double now,const float loudness,const Vec& source,const std::array<Cache,2u>& caches) {
    bool actualRefused{},oracleRefused{};CacheDecision actual,expected;
    try {actual=DecideOriginalNoiseCaches(now,loudness,source,caches);} catch(const std::exception&) {actualRefused=true;}
    try {expected=CacheOracle(now,loudness,source,caches);} catch(const std::exception&) {oracleRefused=true;}
    Require(actualRefused==oracleRefused,"Cache arithmetic refusal differs from independent x87");
    if(!actualRefused) Same(actual,expected);else ++refusals;
}
void CheckHear(const Vec& noise,const Vec& listener,float loudness,float alertness,float threshold,bool team) {
    bool actualRefused{},oracleRefused{};HearDecision actual,expected;
    try {actual=EvaluateOriginalCanHear(noise,listener,loudness,alertness,threshold,team);} catch(const std::exception&) {actualRefused=true;}
    try {expected=HearOracle(noise,listener,loudness,alertness,threshold,team);} catch(const std::exception&) {oracleRefused=true;}
    Require(actualRefused==oracleRefused,"CanHear arithmetic refusal differs from independent x87");
    if(!actualRefused) Same(actual,expected);else ++refusals;
}
void X87Oracle() {
    Precision precision;
    CheckHear({}, {1.0f,0.0f,0.0f},std::nextafter(1.0f,0.0f),
        std::nextafter(std::ldexp(1.0f,-22),0.0f),2.0f,false);
    CheckHear({}, {1000.0f,0.0f,0.0f},1.0f,0.0f,1.2f,false);
    CheckHear({}, {2000.0f,.25f,0.0f},1.0f,0.0f,0.0f,false);
    CheckCache(1.2,1.0f,{},{{{1.0f,1.0f,{}},{1.0f,2.0f,{100.0f,0.0f,0.0f}}}});
    const Vec order{1.0f,std::ldexp(1.0f,-32),std::ldexp(1.0f,-32)};
    const auto xyz=Distance(order,{});
    const auto zy=Add(Mul(order.z,order.z),Mul(order.y,order.y));
    Require(xyz!=Add(zy,Mul(order.x,order.x)),"Distance regression fails to distinguish x/y/z from z/y/x");
    const auto production=QuestVr::OriginalMakeNoiseDetail::DistanceSquared(order,{});
    auto retained=std::ldexp(static_cast<long double>(static_cast<std::uint64_t>(production.significand)),production.exponent);
    if(production.negative) retained=-retained;
    Require(retained==xyz,"Retained cache geometry uses the wrong addition order");
    std::uint64_t state=0x168b6e52d037ad94ull;
    for(unsigned i=0u;i<60'000u;++i) {
        const bool broad=i<20'000u;
        const auto coordinate=[&] {return broad?Number(state):Bounded(state);};
        const Vec source{coordinate(),coordinate(),coordinate()},listener{coordinate(),coordinate(),coordinate()};
        const auto loudness=broad?Number(state):Bounded(state,2048.0f);
        const auto alertness=broad?Number(state):Bounded(state,1024.0f);
        const auto threshold=broad?Number(state):Bounded(state,1024.0f);
        for(const bool team:{false,true}) CheckHear(source,listener,loudness,alertness,threshold,team);
        std::array<Cache,2u> caches;
        double now{};
        if(broad) {
            const auto raw=Next(state);
            now=std::bit_cast<double>((raw&0x000fffffffffffffull)|(((raw>>52u)%2047ull)<<52u));
            for(auto& cache:caches) cache={Number(state),Number(state),{coordinate(),coordinate(),coordinate()}};
        } else {
            now=static_cast<double>(Next(state)%65537u)/64.0;
            for(auto& cache:caches) {
                cache.time=static_cast<float>(now-static_cast<double>(Next(state)%33u)/64.0);
                cache.loudness=Bounded(state,2048.0f);
                cache.spot={source.x+Bounded(state),source.y+Bounded(state),source.z+Bounded(state)};
            }
        }
        CheckCache(now,loudness,source,caches);
    }
    // Concentrated cap/range/threshold neighborhoods, including Float-spilled
    // distances and quotient rounding: broad randoms alone rarely reach them.
    for(const auto base:{std::sqrt(600000.0f),std::sqrt(2400000.0f),1000.0f,2000.0f,4000.0f}) {
        float coordinate=base;
        for(unsigned below=0u;below<12u;++below) coordinate=std::nextafter(coordinate,0.0f);
        for(unsigned step=0u;step<25u;++step) {
            for(const auto loudness:{-2.0f,-1.0f,0.0f,.5f,1.0f,2.0f})
                for(const auto alertness:{-2.0f,0.0f,.5f,2.0f})
                    for(const auto threshold:{-2.0f,0.0f,.5f,1.2f,2.0f,3.0f})
                        for(const bool team:{false,true})
                            CheckHear({}, {coordinate,0.0f,0.0f},loudness,alertness,threshold,team);
            coordinate=std::nextafter(coordinate,std::numeric_limits<float>::infinity());
        }
    }
}
#else
void X87Oracle() {std::cout<<"SKIP independent x87 oracle on non-x86 compiler; deterministic scalar controls still run.\n";}
#endif
} // namespace
int main() {
    try {
        LazyReaders();CacheBoundaries();HearingBoundaries();Safety();X87Oracle();
        std::cout<<"PASS original MakeNoise/CanHear scalar arithmetic: "<<checks<<" checks, "<<refusals
            <<" refusals; native Double windows, unspilled x/y/z caches, replacement1 quirk, Float distance/q/Stimulus stores and retained threshold. No live AI/BSP/world claim.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"Original MakeNoise scalar arithmetic failed: "<<error.what()<<'\n';return 1;}
}
