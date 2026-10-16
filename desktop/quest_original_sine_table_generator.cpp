#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
#include <cpuid.h>
#endif

// Own, standalone deterministic PROFILE generator. It never loads or calls
// original Deus Ex binaries, libm sine, runtime helpers or test oracles. Static
// authority: Core FGlobalMath1014431E..346 passes a Double-spilled product to
// appSin10123240 (FLDL;FSIN;RET), then stores Float. This artifact is explicitly
// pinned to the machine/FP profile below, not asserted CPU-independent GMath
// initialization or a live native object/table lifecycle.
namespace {
constexpr std::size_t Count=16'384u,BlockSize=512u,BlockCount=Count/BlockSize;
using Words=std::array<std::uint32_t,Count>;

int Block(std::string_view text) {
    if(text.empty()||text.size()>2u) throw std::invalid_argument("block number must be0..31");
    unsigned result{};
    for(const char value:text) {
        if(value<'0'||value>'9') throw std::invalid_argument("block number must be0..31");
        result=result*10u+static_cast<unsigned>(value-'0');
    }
    if(result>=BlockCount) throw std::invalid_argument("block number must be0..31");
    return static_cast<int>(result);
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
constexpr std::uint32_t Signature=0x000306a9u;
constexpr std::uint64_t StepBits=0x3f3921fb54442d18ull;
constexpr unsigned short Pc64=0x037fu,Pc53=0x027fu;
struct Cpu {std::string vendor;std::uint32_t signature{};};
Cpu ReadCpu() {
    unsigned eax{},ebx{},ecx{},edx{};
    if(!__get_cpuid(0u,&eax,&ebx,&ecx,&edx)||eax<1u)
        throw std::runtime_error("CPUID identity is unavailable");
    std::array<char,13u> vendor{};
    std::memcpy(vendor.data(),&ebx,4u);std::memcpy(vendor.data()+4u,&edx,4u);std::memcpy(vendor.data()+8u,&ecx,4u);
    if(!__get_cpuid(1u,&eax,&ebx,&ecx,&edx)) throw std::runtime_error("CPUID signature is unavailable");
    return {vendor.data(),eax};
}
struct Control {
    unsigned short previous{};
    Control() {asm volatile("fnstcw %0":"=m"(previous));}
    ~Control() {asm volatile("fldcw %0"::"m"(previous));}
    static void Set(unsigned short word) {
        asm volatile("fldcw %0"::"m"(word));
        unsigned short actual{};asm volatile("fnstcw %0":"=m"(actual));
        if(actual!=word) throw std::runtime_error("x87 control-word profile did not take effect");
    }
    Control(const Control&)=delete;
    Control& operator=(const Control&)=delete;
};
Words Generate(unsigned short precision) {
    static_assert(sizeof(float)==4u&&std::numeric_limits<float>::is_iec559);
    static_assert(std::numeric_limits<long double>::digits==64,"Generator requires hardware x87");
    Control::Set(precision);const double step=std::bit_cast<double>(StepBits);Words result{};
    for(std::int32_t index=0;index<static_cast<std::int32_t>(Count);++index) {
        double argument{};float output{};
        // Exact original numeric boundaries; no C++ multiply, sin or Float
        // intermediary can be substituted here. FILD reads a signed32 index.
        asm volatile("fildl %1; fmull %2; fstpl %0":"=m"(argument):"m"(index),"m"(step):"st");
        asm volatile("fldl %1; fsin; fstps %0":"=m"(output):"m"(argument):"st");
        if(!std::isfinite(output)||output< -1.0f||output>1.0f)
            throw std::runtime_error("x87 table produced an invalid Float");
        result[static_cast<std::size_t>(index)]=std::bit_cast<std::uint32_t>(output);
    }
    return result;
}
std::uint64_t Fingerprint(const Words& words) {
    std::uint64_t hash=0xcbf29ce484222325ull;
    // Canonical byte order is LITTLE ENDIAN independent of the generator's
    // host object representation. Hash covers exactly65536 payload bytes.
    for(const auto word:words) for(unsigned shift=0u;shift<32u;shift+=8u) {
        hash^=static_cast<std::uint8_t>(word>>shift);hash*=0x100000001b3ull;
    }
    return hash;
}
void EmitBlock(const Words& words,int block) {
    const auto first=static_cast<std::size_t>(block)*BlockSize;
    std::cout<<std::hex<<std::nouppercase<<std::setfill('0');
    for(std::size_t position=first;position<first+BlockSize;++position) {
        std::cout<<"0x"<<std::setw(8)<<words[position]<<"u,";
        std::cout<<((position-first)%8u==7u?'\n':' ');
    }
}
void Profile(const Cpu& cpu,const Words& words) {
    std::cout<<"profile=retail-core-x87-fsin-intel-306a9-nearest-pc64-v1\n"
        <<"vendor="<<cpu.vendor<<"\n"<<std::hex<<std::nouppercase<<std::setfill('0')
        <<"signature=0x"<<std::setw(8)<<cpu.signature<<"\n"
        <<"control_word_pc64=0x"<<std::setw(4)<<Pc64<<"\n"
        <<"control_word_pc53=0x"<<std::setw(4)<<Pc53<<"\n"
        <<"step_double_bits=0x"<<std::setw(16)<<StepBits<<"\n"
        <<"payload_fnv1a64_le=0x"<<std::setw(16)<<Fingerprint(words)<<"\n"<<std::dec
        <<"entries="<<Count<<"\n"<<"canonical_payload_bytes="<<Count*4u<<"\n"
        <<"blocks="<<BlockCount<<"\n"<<"words_per_block="<<BlockSize<<"\n"
        <<"pc53_pc64_float_bit_differences=0\n"
        <<"original_static_initializer_control_word=unproven\n"
        <<"original_binaries_executed=none\n";
}
#endif
} // namespace

int main(int argc,char** argv) {
    try {
        bool profile{};int block=-1;
        if(argc==2&&std::string_view(argv[1])=="--profile") profile=true;
        else if(argc==2&&std::string_view(argv[1]).starts_with("--block")) block=Block(std::string_view(argv[1]).substr(7u));
        else if(argc==3&&std::string_view(argv[1])=="--block") block=Block(argv[2]);
        else {
            std::cerr<<"Usage: quest_original_sine_table_generator --profile | --block0..31 | --block N\n";return 2;
        }
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        const auto cpu=ReadCpu();
        if(cpu.vendor!="GenuineIntel"||cpu.signature!=Signature) {
            std::cerr<<"Unsupported generation CPU: "<<cpu.vendor<<" signature0x"<<std::hex<<cpu.signature
                <<"; pinned GenuineIntel/000306a9 required. No table emitted.\n";return 77;
        }
        Control restore;const auto words=Generate(Pc64),alternative=Generate(Pc53);Control::Set(Pc64);
        std::size_t mismatches{};
        for(std::size_t i=0;i<Count;++i) if(words[i]!=alternative[i]) ++mismatches;
        if(mismatches) {
            std::cerr<<"Refused: PC53 and PC64 differ in "<<mismatches<<" Float entries. No table emitted.\n";return 1;
        }
        if(profile) Profile(cpu,words);else EmitBlock(words,block);
        if(!std::cout) throw std::runtime_error("Generator stdout write failed");
        return 0;
#else
        static_cast<void>(profile);static_cast<void>(block);
        std::cerr<<"Unsupported generation architecture/compiler: x86 GCC hardware x87 is required. No table emitted.\n";return 77;
#endif
    } catch(const std::exception& error) {
        std::cerr<<"Original sine profile generator refused: "<<error.what()<<'\n';return 1;
    }
}
