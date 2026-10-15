#include "quest_sound_hearing.h"

#include <iostream>
#include <limits>

namespace {
using Vec=PortableModelVec3;
namespace Math=QuestVr::ModelFastLineDetail;
std::size_t checks{},refusals{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
template<class Operation> void Refuses(Operation operation) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,"Unsafe sound geometry did not refuse");++refusals;
}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
QuestVr::OriginalSoundSquaredDistance Exact(float value) {return {Math::Decode(value),value};}
void HandComputed() {
    const auto distance=QuestVr::OriginalSoundDistanceSquared({1,2,3},{4,6,15});
    Require(distance.stored==169.0f,"Geometric squared distance is not 3^2+4^2+12^2");
    Require(QuestVr::NativeWindowClipDetail::Compare64(distance.initial,Math::Decode(169.0f))==0,
        "Retained geometric squared distance is not exact");
    const auto zero=QuestVr::OriginalSoundDistanceSquared({-0.0f,0,-0.0f},{0,-0.0f,0});
    Require(Bits(zero.stored)==0u&&!zero.initial.significand,"Squared signed zeros did not become positive zero");
    const auto tiny=QuestVr::OriginalSoundDistanceSquared({}, {std::numeric_limits<float>::denorm_min(),0,0});
    Require(tiny.initial.significand&&tiny.stored==0.0f,"Retained subnormal square was prematurely spilled");
    auto packet=QuestVr::PreparePlaySoundProtocolPacket(4u,1u,.5f,false,100.0f,1.2f);
    const auto threshold=QuestVr::OriginalSoundHearingDetail::InitialThreshold(packet);
    Require(!QuestVr::OriginalSoundInitialRange(packet,Exact(threshold)),"Initial range incorrectly permits equality");
    auto below=QuestVr::OriginalSoundSquaredDistance{Math::Normalize64(Math::Decode(threshold)),threshold};
    --below.initial.significand;
    Require(Math::Store32(below.initial)==threshold&&QuestVr::OriginalSoundInitialRange(packet,below),
        "Initial range compared rounded Float instead of retained PC64");
    auto above=QuestVr::OriginalSoundSquaredDistance{Math::Normalize64(Math::Decode(threshold)),threshold};
    ++above.initial.significand;
    Require(!QuestVr::OriginalSoundInitialRange(packet,above),"Initial range accepted retained sum above threshold");
    const auto clear=QuestVr::EvaluateOriginalSoundHearing(packet,Exact(100.0f),false,false);
    Require(clear.initialRangeAccepted&&clear.audible&&clear.parameters==packet.parameters,
        "Clear path unexpectedly attenuated sound");
    const auto blocked=QuestVr::EvaluateOriginalSoundHearing(packet,Exact(100.0f),true,false);
    Require(blocked.audible&&Bits(blocked.parameters[1u])==Bits(35.0f)&&
        blocked.parameters[0u]==packet.parameters[0u]&&blocked.parameters[2u]==packet.parameters[2u],
        "Blocked path did not attenuate only packed Radius");
    const auto far=Exact(threshold*.8f);
    Require(!QuestVr::EvaluateOriginalSoundHearing(packet,far,true,false).audible,
        "Blocked non-instigator escaped reduced range");
    const auto instigator=QuestVr::EvaluateOriginalSoundHearing(packet,far,true,true);
    Require(instigator.audible&&Bits(instigator.parameters[1u])==Bits(35.0f),
        "Instigator lost range exemption or incorrectly escaped radius attenuation");
    const double reduced=static_cast<double>(threshold)*static_cast<double>(.6f);
    const float rounded=static_cast<float>(reduced);
    for (float value : {std::nextafter(rounded,0.0f),rounded,std::nextafter(rounded,std::numeric_limits<float>::infinity())}) {
        const auto heard=QuestVr::EvaluateOriginalSoundHearing(packet,Exact(value),true,false);
        Require(heard.audible==(static_cast<double>(value)<=reduced),"Secondary comparison rounded threshold before inclusive test");
    }
    auto equalityPacket=packet;equalityPacket.squaredRadius=1.3f;
    Require(QuestVr::OriginalSoundHearingDetail::InitialThreshold(equalityPacket)==1.0f,
        "Hand-computed initial threshold did not round to one");
    Require(QuestVr::EvaluateOriginalSoundHearing(equalityPacket,Exact(.6f),true,false).audible,
        "Secondary comparison rejected exact equality");
    Require(!QuestVr::EvaluateOriginalSoundHearing(equalityPacket,
        Exact(std::nextafter(.6f,1.0f)),true,false).audible,"Secondary comparison admitted a Float above equality");
    packet.squaredRadius=0.0f;
    Require(!QuestVr::OriginalSoundInitialRange(packet,zero)&&!QuestVr::OriginalSoundInitialRange(packet,tiny),
        "Zero squared radius incorrectly admits coincident or underflow-spilled geometry");
}
void Safety() {
    const auto packet=QuestVr::PreparePlaySoundProtocolPacket(1,1,1,false,10,1);
    for (const float invalid : {std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()})
        for (unsigned slot=0;slot<6u;++slot) {
            Vec source{},target{};float* fields[]{&source.x,&source.y,&source.z,&target.x,&target.y,&target.z};
            *fields[slot]=invalid;Refuses([&]{static_cast<void>(QuestVr::OriginalSoundDistanceSquared(source,target));});
        }
    Refuses([&]{static_cast<void>(QuestVr::OriginalSoundDistanceSquared({}, {std::numeric_limits<float>::max(),0,0}));});
    Refuses([&]{static_cast<void>(QuestVr::OriginalSoundInitialRange(packet,{Math::Decode(4),5}));});
    Refuses([&]{static_cast<void>(QuestVr::OriginalSoundInitialRange(packet,Exact(-1)));});
    Refuses([&]{auto value=Exact(1);value.initial.exponent=std::numeric_limits<int>::max();
        static_cast<void>(QuestVr::OriginalSoundInitialRange(packet,value));});
    Refuses([&]{auto value=Exact(1);value.initial.significand=QuestVr::ActorTimerDetail::Wide{1}<<100u;
        static_cast<void>(QuestVr::OriginalSoundInitialRange(packet,value));});
    Refuses([&]{auto invalid=packet;invalid.squaredRadius=-1;static_cast<void>(QuestVr::OriginalSoundInitialRange(invalid,Exact(0)));});
    Refuses([&]{auto invalid=packet;invalid.parameters[2u]=std::numeric_limits<float>::infinity();
        static_cast<void>(QuestVr::EvaluateOriginalSoundHearing(invalid,Exact(0),true,false));});
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct Precision {
    unsigned short original{};
    Precision() {
        static_assert(std::numeric_limits<long double>::digits==64);
        asm volatile("fnstcw %0":"=m"(original));
        const unsigned short mode=static_cast<unsigned short>((original&~0x0f00u)|0x0300u);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fldcw %0"::"m"(original));}
};
long double Add(long double a,long double b) {volatile long double x=a,y=b,z=x+y;return z;}
long double Sub(long double a,long double b) {volatile long double x=a,y=b,z=x-y;return z;}
long double Mul(long double a,long double b) {volatile long double x=a,y=b,z=x*y;return z;}
float Store(long double value) {
    volatile float result=static_cast<float>(value);
    if(!std::isfinite(result)) throw std::runtime_error("Oracle Float overflow");
    return result;
}
long double Extended(Math::Dyadic value) {
    const auto magnitude=std::ldexp(static_cast<long double>(static_cast<std::uint64_t>(value.significand)),value.exponent);
    return value.negative?-magnitude:magnitude;
}
std::uint32_t Next(std::uint32_t& state) {state^=state<<13u;state^=state>>17u;state^=state<<5u;return state;}
float Number(std::uint32_t& state) {
    const auto bits=Next(state);
    return std::bit_cast<float>((bits&0x807fffffu)|((1u+((bits>>23u)%254u))<<23u));
}
void Oracle() {
    Precision precision;std::uint32_t state=0xe1bf83a5u;
    for (unsigned iteration=0u;iteration<30000u;++iteration) {
        const auto coordinate=[&] {
            return iteration<20000u?Number(state):
                static_cast<float>(static_cast<int>(Next(state)%32769u)-16384)/64.0f;
        };
        const Vec source{coordinate(),coordinate(),coordinate()},target{coordinate(),coordinate(),coordinate()};
        const auto x=Sub(target.x,source.x),y=Sub(target.y,source.y),z=Sub(target.z,source.z);
        const auto sum=Add(Add(Mul(z,z),Mul(y,y)),Mul(x,x));
        bool actualRefused{},oracleRefused{};QuestVr::OriginalSoundSquaredDistance actual{};float stored{};
        try {actual=QuestVr::OriginalSoundDistanceSquared(source,target);} catch(const std::exception&) {actualRefused=true;}
        try {stored=Store(sum);} catch(const std::exception&) {oracleRefused=true;}
        Require(actualRefused==oracleRefused,"PC64 geometry overflow policy differs from independent x87");
        if(actualRefused) continue;
        Require(Extended(actual.initial)==sum&&Bits(actual.stored)==Bits(stored),
            "Retained sum or Float spill differs from independent x87 z/y/x oracle");
        const float radius=iteration<20000u?
            std::ldexp(1.0f+static_cast<float>(Next(state)&1023u)/1024.0f,static_cast<int>(Next(state)%120u)-60):
            1.0f+static_cast<float>(Next(state)%1024u);
        const auto packet=QuestVr::PreparePlaySoundProtocolPacket(Next(state),static_cast<std::uint8_t>(Next(state)),
            .37f,false,radius,1.3f);
        const auto threshold=Store(Mul(packet.squaredRadius,std::bit_cast<float>(std::uint32_t{0x3f44ec4fu})));
        Require(QuestVr::OriginalSoundInitialRange(packet,actual)==(sum<threshold),
            "Strict initial range differs from independent x87");
        for (bool blocked:{false,true}) for (bool instigator:{false,true}) {
            const auto result=QuestVr::EvaluateOriginalSoundHearing(packet,actual,blocked,instigator);
            const auto secondary=blocked&&!instigator?Mul(threshold,.6f):static_cast<long double>(threshold);
            const bool expected=sum<threshold&&(!blocked||static_cast<long double>(stored)<=secondary);
            const float expectedRadius=sum<threshold&&blocked?Store(Mul(packet.parameters[1u],.35f)):packet.parameters[1u];
            Require(result.audible==expected&&Bits(result.parameters[1u])==Bits(expectedRadius)&&
                result.parameters[0u]==packet.parameters[0u]&&result.parameters[2u]==packet.parameters[2u],
                "Occluded hearing decision/packed parameters differ from independent x87");
        }
    }
}
#endif
} // namespace
int main() {
    try {
        HandComputed();Safety();
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        Oracle();
#endif
        std::cout<<"PASS original sound hearing: "<<checks<<" checks, "<<refusals
            <<" refusals; PC64 coordinate geometry, dual distance stores, strict/inclusive range and occlusion parameters.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL original sound hearing: "<<error.what()<<'\n';return 1;}
}
