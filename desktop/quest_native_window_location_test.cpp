#include "quest_native_window_location.h"
#include <array>
#include <iostream>
#include <string>

namespace {
unsigned checks{};
void Require(bool value,const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
std::uint32_t Raw(float value) { std::uint32_t bits; std::memcpy(&bits,&value,4u); return bits; }
float Float(std::uint32_t bits) { float value; std::memcpy(&value,&bits,4u); return value; }
bool Same(const QuestVr::NativeWindowLocation& a,const QuestVr::NativeWindowLocation& b) {
    return Raw(a.x)==Raw(b.x) && Raw(a.y)==Raw(b.y) && a.changed==b.changed;
}
QuestVr::NativeWindowLocation Compute(const std::array<float,6u>& values) {
    return QuestVr::ComputeNativeWindowLocation(values[0u],values[1u],values[2u],values[3u],values[4u],values[5u]);
}
void Refuses(const std::array<float,6u>& values) {
    std::array<std::uint32_t,6u> before{};
    for (std::size_t slot=0u;slot<values.size();++slot) before[slot]=Raw(values[slot]);
    bool refused{};
    try { static_cast<void>(Compute(values)); } catch (const std::runtime_error&) { refused=true; }
    Require(refused,"Unsafe native window location input was accepted");
    for (std::size_t slot=0u;slot<values.size();++slot)
        Require(Raw(values[slot])==before[slot],"Refusal mutated a location input");
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
// Independent host x87 arithmetic; no original executable or DLL is loaded.
QuestVr::NativeWindowLocation Oracle(const std::array<float,6u>& values) {
    for (float value:values)
        if (!std::isfinite(value)) throw std::runtime_error("Oracle requires finite inputs");
    volatile long double originX=values[0u],localX=values[1u],originY=values[2u],localY=values[3u];
    volatile long double x=originX+localX;
    volatile long double extendedY=originY+localY;
    const float y=static_cast<float>(extendedY);
    if (!std::isfinite(y)) throw std::runtime_error("Oracle Y overflow");
    if (x==static_cast<long double>(values[4u]) && y==values[5u])
        return {values[4u],values[5u],false};
    const float storedX=static_cast<float>(x);
    if (!std::isfinite(storedX)) throw std::runtime_error("Oracle X overflow");
    return {storedX,y,true};
}
void CheckOracle(const std::array<float,6u>& values) {
    bool actualRefused{},oracleRefused{};
    QuestVr::NativeWindowLocation actual,expected;
    try { actual=Compute(values); } catch (const std::runtime_error&) { actualRefused=true; }
    try { expected=Oracle(values); } catch (const std::runtime_error&) { oracleRefused=true; }
    if (actualRefused!=oracleRefused || (!actualRefused && !Same(actual,expected))) {
        std::cerr << std::hex << "Location oracle mismatch inputs=";
        for (float value:values) std::cerr << Raw(value) << ',';
        std::cerr << " actual=" << Raw(actual.x) << ',' << Raw(actual.y) << ',' << actual.changed
            << " expected=" << Raw(expected.x) << ',' << Raw(expected.y) << ',' << expected.changed << std::dec << '\n';
        throw std::runtime_error("Native window PC64 x87 oracle mismatch");
    }
    ++checks;
}
#endif
} // namespace

int main() {
    try {
        using QuestVr::ComputeNativeWindowLocation;
        auto location=ComputeNativeWindowLocation(2.0f,3.0f,-4.0f,1.0f,5.0f,-3.0f);
        Require(!location.changed && location.x==5.0f && location.y==-3.0f,"Equal native location changed");
        location=ComputeNativeWindowLocation(2.0f,3.0f,-4.0f,1.0f,4.0f,-3.0f);
        Require(location.changed && location.x==5.0f && location.y==-3.0f,"Changed X sum was not stored");
        location=ComputeNativeWindowLocation(16777216.0f,1.0f,0.0f,0.0f,16777216.0f,0.0f);
        Require(location.changed && location.x==16777216.0f,"Live PC64 X was compared after binary32 rounding");
        location=ComputeNativeWindowLocation(0.0f,0.0f,16777216.0f,1.0f,0.0f,16777216.0f);
        Require(!location.changed && location.y==16777216.0f,"Y was compared before its binary32 store");
        for (int exponent:{-54,-63}) {
            const float residual=std::ldexp(1.0f,exponent);
            location=ComputeNativeWindowLocation(1.0f,residual,0.0f,0.0f,1.0f,0.0f);
            Require(location.changed && location.x==1.0f,"X lost a residual present in PC64 but absent in stored Float");
        }
        location=ComputeNativeWindowLocation(1.0f,std::ldexp(1.0f,-64),0.0f,0.0f,1.0f,0.0f);
        Require(!location.changed,"PC64 positive half-ULP tie did not round to even");
        location=ComputeNativeWindowLocation(1.0f,Float(0x1f800001u),0.0f,0.0f,1.0f,0.0f);
        Require(location.changed && location.x==1.0f,"PC64 above-half-ULP residual was lost");
        location=ComputeNativeWindowLocation(-1.0f,-std::ldexp(1.0f,-64),0.0f,0.0f,-1.0f,0.0f);
        Require(!location.changed,"Negative PC64 half-ULP tie did not round to even");
        location=ComputeNativeWindowLocation(-1.0f,-std::ldexp(1.0f,-63),0.0f,0.0f,-1.0f,0.0f);
        Require(location.changed && location.x==-1.0f,"Negative live PC64 X residual was lost");
        location=ComputeNativeWindowLocation(-1.0f,std::ldexp(1.0f,-64),0.0f,0.0f,-1.0f,0.0f);
        Require(location.changed && location.x==-1.0f,"PC64 binade boundary used the wrong half-ULP");
        const float tiny=std::numeric_limits<float>::denorm_min(),negativeZero=Float(0x80000000u);
        location=ComputeNativeWindowLocation(tiny,tiny,-tiny,-tiny,2.0f*tiny,-2.0f*tiny);
        Require(!location.changed && Raw(location.x)==2u && Raw(location.y)==0x80000002u,"Subnormal location sums changed");
        location=ComputeNativeWindowLocation(1.0f,-Float(0x3f7fffffu),-1.0f,Float(0x3f7fffffu),0.0f,0.0f);
        Require(location.changed && Raw(location.x)==0x33800000u && Raw(location.y)==0xb3800000u,
            "Near cancellation lost the exact signed residual");
        location=ComputeNativeWindowLocation(std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),tiny,-tiny,negativeZero,negativeZero);
        Require(!location.changed && Raw(location.x)==0x80000000u && Raw(location.y)==0x80000000u,
            "Equal cancellation did not preserve old signed-zero storage");
        location=ComputeNativeWindowLocation(negativeZero,negativeZero,negativeZero,negativeZero,0.0f,0.0f);
        Require(!location.changed && Raw(location.x)==0u && Raw(location.y)==0u,"Equal negative-zero sums rewrote old positive zeros");
        location=ComputeNativeWindowLocation(0.0f,0.0f,0.0f,0.0f,negativeZero,negativeZero);
        Require(!location.changed && Raw(location.x)==0x80000000u && Raw(location.y)==0x80000000u,
            "No-candidate zeros rewrote old negative-zero storage");
        location=ComputeNativeWindowLocation(0.0f,0.0f,negativeZero,negativeZero,negativeZero,1.0f);
        Require(location.changed && Raw(location.x)==0u && Raw(location.y)==0x80000000u,
            "Changed location did not store both newly computed zero signs");
        location=ComputeNativeWindowLocation(0.0f,0.0f,0.0f,0.0f,1.0f,-1.0f);
        Require(location.changed && Raw(location.x)==0u && Raw(location.y)==0u,"No eligible candidate did not reset both coordinates");
        location=ComputeNativeWindowLocation(1.0f,tiny,-1.0f,-tiny,1.0f,-1.0f);
        Require(!location.changed,"Disparate exponents were not rounded at PC64 precision");
        const auto maximum=std::numeric_limits<float>::max();
        Refuses({maximum,maximum,0.0f,0.0f,0.0f,0.0f});
        Refuses({-maximum,-maximum,0.0f,0.0f,0.0f,0.0f});
        Refuses({0.0f,0.0f,maximum,maximum,0.0f,0.0f});
        Refuses({0.0f,0.0f,-maximum,-maximum,0.0f,0.0f});
        for (float bad:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()})
            for (std::size_t slot=0u;slot<6u;++slot) {
                std::array<float,6u> values{};values[slot]=bad;Refuses(values);
            }
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        static_assert(std::numeric_limits<long double>::digits==64,"x87 oracle requires extended significand64");
        RetailPrecisionOracle precision;
        for (float origin:{negativeZero,0.0f,tiny,-tiny,Float(0x007fffffu),Float(0x00800000u),
            1.0f,-1.0f,16777216.0f,-16777216.0f,maximum,-maximum})
            for (float local:{negativeZero,0.0f,tiny,-tiny,std::ldexp(1.0f,-64),-std::ldexp(1.0f,-64),
                std::ldexp(1.0f,-63),-std::ldexp(1.0f,-63),std::ldexp(1.0f,-54),1.0f,-1.0f,maximum,-maximum}) {
                CheckOracle({origin,local,0.0f,0.0f,origin,negativeZero});
                CheckOracle({0.0f,0.0f,origin,local,negativeZero,origin});
                CheckOracle({origin,local,origin,local,local,local});
            }
        std::uint32_t random=0x5eed4500u;
        const auto next=[&]() { random^=random<<13u;random^=random>>17u;random^=random<<5u;return random; };
        const auto finite=[&]() {
            const auto raw=next();return Float((raw&0x80000000u)|((raw&0x7fffffffu)%0x7f800000u));
        };
        for (unsigned index=0u;index<20000u;++index) {
            std::array<float,6u> values{};for (auto& value:values) value=finite();
            if ((index&3u)==0u) {values[4u]=values[0u];values[5u]=values[2u];}
            CheckOracle(values);
        }
        std::cout << "Independent native window PC64 x87 oracle tuples: 20468\n";
#else
        std::cout << "Independent x87 oracle unavailable on this architecture; deterministic fixtures only\n";
#endif
        std::cout << "Native window location tests passed: " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n';return 1; }
}
