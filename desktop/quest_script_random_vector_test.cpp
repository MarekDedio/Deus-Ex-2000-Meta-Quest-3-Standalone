#include "quest_script_random_vector.h"

#include <iostream>
#include <limits>

namespace {
namespace Random=QuestVr::ScriptRandom;
namespace Detail=Random::VectorDetail;
using Vec=std::array<float,3u>;
std::size_t checks{},refusals{};
void Require(const bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
template<class Operation> void Refuses(Operation operation) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,"Unsafe VRand operation did not refuse");++refusals;
}
std::uint32_t Bits(const float value) {return std::bit_cast<std::uint32_t>(value);}
std::uint32_t OracleDraw(std::uint32_t& seed) {
    const auto wide=static_cast<std::uint64_t>(seed)*214013ull+2531011ull;
    seed=static_cast<std::uint32_t>(wide&0xffffffffull);
    return static_cast<std::uint32_t>((wide>>16u)&32767ull);
}
void SameVector(const Vec& actual,const Vec& expected,const char* reason) {
    for(std::size_t axis=0u;axis<3u;++axis) Require(Bits(actual[axis])==Bits(expected[axis]),reason);
}
void Deterministic() {
    Require(Random::DefaultVRandMaxAttempts==4096u,"VRand portable attempt limit changed");
    const auto boundary=Detail::CandidateFromDraws({16384u,16384u,32767u});
    Require(boundary.accepted&&Bits(boundary.stored[0u])==0x38000000u&&
        Bits(boundary.stored[1u])==0x38000000u&&Bits(boundary.stored[2u])==0x3f800000u,
        "Mixed retained/stored Z boundary was replaced by Float FRand or all-Float rejection");
    const auto allStoredSum=Detail::Add64(Detail::Add64(
        Detail::Multiply64(Detail::Decode(boundary.stored[2u]),Detail::Decode(boundary.stored[2u])),
        Detail::Multiply64(Detail::Decode(boundary.stored[1u]),Detail::Decode(boundary.stored[1u]))),
        Detail::Multiply64(Detail::Decode(boundary.stored[0u]),Detail::Decode(boundary.stored[0u])));
    Require(Detail::Compare64(allStoredSum,Detail::Decode(1.0f))>0,
        "Regression tuple did not distinguish mixed-Z rejection from all stored coordinates");
    Require(!Detail::CandidateFromDraws({0u,0u,0u}).accepted&&
        !Detail::CandidateFromDraws({32767u,32767u,32767u}).accepted,"Cube corners did not reject");
    const auto middle=Detail::CandidateFromDraws({16384u,16384u,16384u});
    Require(middle.accepted,"Small nonzero candidate was incorrectly zero-guarded");
    SameVector(Detail::NormalizeStored(middle.stored),{std::bit_cast<float>(0x3f13cd3au),
        std::bit_cast<float>(0x3f13cd3au),std::bit_cast<float>(0x3f13cd3au)},"Positive equal components did not normalize to 1/sqrt(3)");
    const auto negative=Detail::CandidateFromDraws({16383u,16383u,16383u});
    SameVector(Detail::NormalizeStored(negative.stored),{std::bit_cast<float>(0xbf13cd3au),
        std::bit_cast<float>(0xbf13cd3au),std::bit_cast<float>(0xbf13cd3au)},"Negative equal components did not normalize to -1/sqrt(3)");
    SameVector(Detail::NormalizeStored({0,3,4}),{0,.6f,.8f},"Unsafe normalization changed 3/4/5 proportions");
    SameVector(Detail::NormalizeStored({-0.0f,1.0f,-0.0f}),{-0.0f,1.0f,-0.0f},"Normalization did not preserve signed-zero components");
    const auto tiny=std::numeric_limits<float>::denorm_min();
    SameVector(Detail::NormalizeStored({tiny,0,0}),{1,0,0},"Tiny nonzero vector acquired a guarded-normalization threshold");
    SameVector(Detail::NormalizeStored({std::numeric_limits<float>::max(),0,0}),{1,0,0},
        "Large finite Float coordinate was prematurely spilled to a Float squared length");
    for(std::uint32_t draw=0u;draw<32768u;++draw)
        Require(Detail::RetainedCoordinate(draw).significand!=0u,"MSVCRT VRand grid unexpectedly contains an exact zero coordinate");
    const auto one=Detail::SqrtStoredDouble64(1.0),four=Detail::SqrtStoredDouble64(4.0);
    Require(Detail::Compare64(one,Detail::Decode(1))==0&&Detail::Compare64(four,Detail::Decode(2))==0,
        "Exact square roots changed");
    const auto subnormal=Detail::SqrtStoredDouble64(std::numeric_limits<double>::denorm_min());
    Require(subnormal.significand==(Detail::Wide{1u}<<63u)&&subnormal.exponent==-600,
        "Smallest Double subnormal square root was flushed or rounded via Float");
    const auto tie=Detail::Dyadic{(Detail::Wide{1u}<<63u)+(Detail::Wide{1u}<<10u),-63,false};
    Require(std::bit_cast<std::uint64_t>(Detail::StoreDouble(tie))==0x3ff0000000000000ull,
        "Double spill halfway tie failed to round to even lower endpoint");
    auto oddTie=tie;oddTie.significand+=Detail::Wide{1u}<<11u;
    Require(std::bit_cast<std::uint64_t>(Detail::StoreDouble(oddTie))==0x3ff0000000000002ull,
        "Double spill halfway tie failed to round to even upper endpoint");
    std::uint32_t seed=1u,oracle=1u;
    const auto first=Random::VRand(seed);
    for(std::size_t draw=0u;draw<6u;++draw) static_cast<void>(OracleDraw(oracle));
    Require(first.draws==6u&&seed==oracle,"Seed-1 VRand did not reject first triple and consume exactly six shared-stream draws");
    for(const auto initial:{0u,1u,0x7fffffffu,0x80000000u,0xffffffffu}) {
        seed=initial;oracle=initial;
        for(unsigned iteration=0u;iteration<128u;++iteration) {
            const auto result=Random::VRand(seed);
            Require(result.draws>=3u&&result.draws%3u==0u&&result.draws<=Random::DefaultVRandMaxAttempts*3u,
                "VRand draw accounting is not complete triples");
            for(std::size_t draw=0u;draw<result.draws;++draw) static_cast<void>(OracleDraw(oracle));
            Require(seed==oracle,"VRand did not share the original wrapped MSVCRT stream");
            Require(Random::Rand(seed,32768)==static_cast<std::int32_t>(OracleDraw(oracle)),
                "Rand following VRand diverged from shared draw stream");
            const double length=static_cast<double>(result.vector[0u])*result.vector[0u]+
                static_cast<double>(result.vector[1u])*result.vector[1u]+
                static_cast<double>(result.vector[2u])*result.vector[2u];
            Require(std::abs(length-1.0)<2e-7,"VRand output is not a finite approximately unit Float vector");
        }
    }
}
void Safety() {
    std::uint32_t seed=1u;
    Refuses([&]{static_cast<void>(Random::VRand(seed,0u));});Require(seed==1u,"Zero attempt budget consumed RNG");
    Refuses([&]{static_cast<void>(Random::VRand(seed,Random::DefaultVRandMaxAttempts+1u));});
    Require(seed==1u,"Invalid oversized attempt budget consumed RNG");
    Refuses([&]{static_cast<void>(Random::VRand(seed,1u));});
    std::uint32_t oracle=1u;for(unsigned draw=0u;draw<3u;++draw) static_cast<void>(OracleDraw(oracle));
    Require(seed==oracle,"Attempt exhaustion silently rewound or skipped rejected shared-stream draws");
    // A containing runtime transaction, not this pure helper, restores this
    // seed together with all script writes and any earlier nested draws.
    Refuses([&]{static_cast<void>(Detail::CandidateFromDraws({0,32768u,1}));});
    Refuses([&]{static_cast<void>(Detail::NormalizeStored({0,0,0}));});
    for(const auto value:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()})
        Refuses([&]{static_cast<void>(Detail::NormalizeStored({0,value,0}));});
    for(const auto value:{0.0,-0.0,-1.0,std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()})
        Refuses([&]{static_cast<void>(Detail::SqrtStoredDouble64(value));});
    Refuses([&]{static_cast<void>(Detail::StoreDouble({1u,std::numeric_limits<int>::max(),false}));});
    Refuses([&]{static_cast<void>(Detail::StoreDouble({Detail::Wide{1u}<<100u,0,false}));});
    Refuses([&]{static_cast<void>(Detail::StoreDouble({1u,2048,false}));});
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
long double Div(long double a,long double b) {volatile long double x=a,y=b,z=x/y;return z;}
float StoreFloat(long double value) {volatile float result=static_cast<float>(value);return result;}
double StoreDouble(long double value) {volatile double result=static_cast<double>(value);return result;}
long double Sqrt(double value) {
    long double result;
    asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(result):"m"(value):"st");
    return result;
}
long double Extended(Detail::Dyadic value) {
    const auto magnitude=std::ldexp(static_cast<long double>(static_cast<std::uint64_t>(value.significand)),value.exponent);
    return value.negative?-magnitude:magnitude;
}
long double Coordinate(std::uint32_t draw) {
    return Sub(Mul(Mul(static_cast<long double>(draw),std::bit_cast<float>(0x38000100u)),2.0L),1.0L);
}
Vec Normalize(const Vec& vector) {
    const auto sum=Add(Add(Mul(vector[2u],vector[2u]),Mul(vector[1u],vector[1u])),Mul(vector[0u],vector[0u]));
    const auto reciprocal=Div(1.0L,Sqrt(StoreDouble(sum)));
    return {StoreFloat(Mul(vector[0u],reciprocal)),StoreFloat(Mul(vector[1u],reciprocal)),StoreFloat(Mul(vector[2u],reciprocal))};
}
Random::VRandResult OracleVRand(std::uint32_t& seed,std::size_t maxAttempts) {
    Random::VRandResult result;
    for(std::size_t attempt=0u;attempt<maxAttempts;++attempt) {
        const auto x=Coordinate(OracleDraw(seed)),y=Coordinate(OracleDraw(seed)),z=Coordinate(OracleDraw(seed));
        result.draws+=3u;
        const Vec stored{StoreFloat(x),StoreFloat(y),StoreFloat(z)};
        const auto sum=Add(Add(Mul(z,stored[2u]),Mul(stored[1u],stored[1u])),Mul(stored[0u],stored[0u]));
        if(sum<=1.0L) {result.vector=Normalize(stored);return result;}
    }
    throw std::runtime_error("Oracle rejection budget exhausted");
}
std::uint64_t Next(std::uint64_t& state) {state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
void X87Oracle() {
    Precision precision;
    for(std::uint32_t draw=0u;draw<32768u;++draw) {
        const auto retained=Coordinate(draw);
        Require(Extended(Detail::RetainedCoordinate(draw))==retained,
            "Retained appFrand coordinate differs from independent x87 operation chain");
        Require(Bits(Detail::Store32(Detail::RetainedCoordinate(draw)))==Bits(StoreFloat(retained)),
            "Coordinate Float spill differs from independent x87");
    }
    std::uint64_t state=0xd21b479c315ea6f8ull;
    for(unsigned iteration=0u;iteration<50000u;++iteration) {
        const auto raw=Next(state);
        const auto bits=(raw&0x000fffffffffffffull)|((1ull+(raw>>52u)%2046ull)<<52u);
        const auto value=std::bit_cast<double>(bits);
        const auto actual=Detail::SqrtStoredDouble64(value);
        Require(Extended(actual)==Sqrt(value),"Integer correctly-rounded PC64 square root differs from independent FSQRT");
        const auto extended=Sub(static_cast<long double>(value),Mul(static_cast<long double>(value),0x1p-63L));
        const auto decoded=Detail::Add64(Detail::DecodeDouble(value),QuestVr::NativeWindowClipDetail::Negate(
            Detail::Multiply64(Detail::DecodeDouble(value),Detail::DecodeDouble(0x1p-63))));
        bool oracleOverflow{},actualOverflow{};double expected{},stored{};
        expected=StoreDouble(extended);oracleOverflow=!std::isfinite(expected);
        try {stored=Detail::StoreDouble(decoded);} catch(const std::exception&) {actualOverflow=true;}
        Require(oracleOverflow==actualOverflow,"PC64 to Double spill overflow policy differs from independent x87");
        if(!actualOverflow) Require(std::bit_cast<std::uint64_t>(stored)==std::bit_cast<std::uint64_t>(expected),
            "PC64 to Double nearest-even spill differs from independent x87");
    }
    for(const auto value:{std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::min(),
        std::numeric_limits<double>::max(),std::nextafter(1.0,0.0),std::nextafter(1.0,2.0),2.0,4.0})
        Require(Extended(Detail::SqrtStoredDouble64(value))==Sqrt(value),"Boundary FSQRT differs from integer PC64 helper");
    for(unsigned iteration=0u;iteration<20000u;++iteration) {
        const auto initial=static_cast<std::uint32_t>(Next(state));auto actualSeed=initial,oracleSeed=initial;
        const auto actual=Random::VRand(actualSeed),expected=OracleVRand(oracleSeed,Random::DefaultVRandMaxAttempts);
        Require(actualSeed==oracleSeed&&actual.draws==expected.draws,"VRand rejection count/stream differs from independent x87 sampler");
        SameVector(actual.vector,expected.vector,"VRand normalized component differs from independent x87 sampler");
        const std::array<std::uint32_t,3u> draws{static_cast<std::uint32_t>(Next(state)&32767u),
            static_cast<std::uint32_t>(Next(state)&32767u),static_cast<std::uint32_t>(Next(state)&32767u)};
        const auto candidate=Detail::CandidateFromDraws(draws);
        const Vec stored{StoreFloat(Coordinate(draws[0u])),StoreFloat(Coordinate(draws[1u])),StoreFloat(Coordinate(draws[2u]))};
        const auto sum=Add(Add(Mul(Coordinate(draws[2u]),stored[2u]),Mul(stored[1u],stored[1u])),Mul(stored[0u],stored[0u]));
        Require(candidate.accepted==(sum<=1.0L),"Retained-Z rejection differs from independent x87 candidate");
    }
}
#endif
} // namespace

int main() {
    try {
        Deterministic();Safety();
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        X87Oracle();
#endif
        std::cout<<"PASS original VRand: "<<checks<<" checks, "<<refusals
            <<" refusals; shared CRT stream, retained-Z rejection, Double sum/PC64 FSQRT/reciprocal and exact Float outputs.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL original VRand: "<<error.what()<<'\n';return 1;}
}
