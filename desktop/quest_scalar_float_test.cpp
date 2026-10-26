#include "quest_scalar_float.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace QuestVr::ScalarFloat;
std::size_t checks{};
void Require(bool condition,const char* message) {++checks;if(!condition)throw std::runtime_error(message);}
void Same(float actual,float expected,const char* message) {
    if(Bits(actual)!=Bits(expected)) {
        std::cerr<<message<<" actual="<<std::hex<<Bits(actual)<<" expected="<<Bits(expected)<<std::dec<<'\n';
        throw std::runtime_error(message);
    }
    ++checks;
}
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
constexpr std::array<Operation,4u> Operations{Operation::Multiply,Operation::Divide,Operation::Add,Operation::Subtract};
struct Precision {
    unsigned short prior{};
    Precision() {
        asm volatile("fnstcw %0":"=m"(prior));
        const auto mode=static_cast<unsigned short>((prior&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fnclex\n\tfldcw %0"::"m"(prior));}
};
// Own independent x87 instructions, never the original game's code. Preserve
// exact retail FLD operand order and memory operation, especially for sNaNs.
float Oracle(Operation op,float a,float b) {
    float result{};
    switch(op) {
    case Operation::Multiply:asm volatile("flds %2; fmuls %1; fstps %0":"=m"(result):"m"(a),"m"(b):"st");break;
    case Operation::Divide:asm volatile("flds %1; fdivs %2; fstps %0":"=m"(result):"m"(a),"m"(b):"st");break;
    case Operation::Add:asm volatile("flds %2; fadds %1; fstps %0":"=m"(result):"m"(a),"m"(b):"st");break;
    case Operation::Subtract:asm volatile("flds %1; fsubs %2; fstps %0":"=m"(result):"m"(a),"m"(b):"st");break;
    }
    return result;
}
unsigned short ComparisonOracle(float a,float b) {
    unsigned short flags{};
    asm volatile("flds %1; fcomps %2; fnstsw %0":"=a"(flags):"m"(a),"m"(b):"st");return flags;
}
void Check(float a,float b) {
    for(auto op:Operations)Same(Binary(op,a,b),Oracle(op,a,b),"Scalar PC64/binary32 arithmetic oracle mismatch");
    const auto flags=ComparisonOracle(a,b);
    const std::array<bool,6u> expected{(flags&0x0100u)!=0u,(flags&0x4100u)==0u,
        (flags&0x4100u)!=0u,(flags&0x0100u)==0u,(flags&0x4000u)!=0u,(flags&0x4000u)==0u};
    for(std::uint16_t i=0u;i<6u;++i)Require(Compare(176u+i,a,b)==expected[i],"Scalar x87 C0/C3 comparison mismatch");
}
#endif
}
int main() {
    try {
        Same(Binary(Operation::Divide,-1.0f,0.0f),Word(0xff800000u),"Division infinity was rejected/clamped");
        Same(Binary(Operation::Divide,1.0f,Word(0x80000000u)),Word(0xff800000u),"Division lost signed zero");
        Same(Binary(Operation::Divide,0.0f,0.0f),Word(0xffc00000u),"Invalid division lost indefinite NaN");
        Same(Binary(Operation::Multiply,Word(0x7f800000u),0.0f),Word(0xffc00000u),"Invalid multiplication lost indefinite NaN");
        Same(Binary(Operation::Add,Word(0x7f800000u),Word(0xff800000u)),Word(0xffc00000u),"Opposite infinities did not produce indefinite");
        Same(Binary(Operation::Add,Word(0x7f7fffffu),Word(0x7f7fffffu)),Word(0x7f800000u),"Finite overflow was clamped/refused");
        Same(Binary(Operation::Multiply,Word(1u),0.5f),0.0f,"Subnormal tie did not round to even zero");
        Same(Binary(Operation::Multiply,Word(3u),0.5f),Word(2u),"Subnormal tie did not round to even significand");
        Same(Load(Word(0xff800001u)),Word(0xffc00001u),"FloatConst FLD did not quiet signaling NaN");
        Same(Negate(Word(0x7f800001u)),Word(0xffc00001u),"Negate did not FLD-quiet then flip sign");
        for(const auto native:{176u,178u,180u})Require(Compare(native,Word(0x7fc00001u),1.0f),"Unordered true branch lost");
        for(const auto native:{177u,179u,181u})Require(!Compare(native,Word(0x7fc00001u),1.0f),"Unordered false branch lost");
        Require(Compare(180u,0.0f,Word(0x80000000u)),"Signed zeros no longer compare equal");
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        Precision precision;
        const std::vector<std::uint32_t> words{0u,0x80000000u,1u,0x80000001u,2u,3u,0x007fffffu,0x00800000u,
            0x00800001u,0x80800000u,0x3f800000u,0xbf800000u,0x3f000000u,0x40000000u,0x7f7fffffu,
            0xff7fffffu,0x7f800000u,0xff800000u,0x7f800001u,0xff800001u,0x7fbfffffu,0xffbfffffu,
            0x7fc00000u,0xffc00000u,0x7fc00001u,0xffc00001u,0x7fffffffu,0xffffffffu,0x7fc12345u,0xffc12345u};
        for(auto a:words)for(auto b:words)Check(Word(a),Word(b));
        std::uint32_t stream=0x12345678u;
        const auto draw=[&] {stream=stream*1664525u+1013904223u;return stream;};
        for(unsigned i=0u;i<20000u;++i) {const auto a=draw(),b=draw();Check(Word(a),Word(b));}
        // Every Float exponent separation, signs, and sparse significands.
        for(unsigned a=0u;a<255u;++a)for(unsigned b=0u;b<255u;b+=7u)
            Check(Word((a<<23u)|0x400001u),Word(0x80000000u|(b<<23u)|0x123456u));
        std::cout<<"Independent original-order masked PC64 x87 scalar arithmetic/comparisons verified.\n";
#endif
        std::cout<<"PASS scalar IEEE Float: "<<checks<<" checks. No original DLL execution or gameplay claim.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
