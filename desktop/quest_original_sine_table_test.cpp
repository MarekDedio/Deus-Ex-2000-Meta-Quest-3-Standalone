#include "quest_original_sine_table.h"
#include "quest_brush_coords.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
#include <cpuid.h>
#endif

// Portable profile integrity and explicit-profile consumer controls. The
// optional hardware comparison executes our own audited scalar instructions,
// never original DLL/EXE code. Exact FSIN comparison is CPU-profile gated.
namespace {
namespace Profile=QuestVr::OriginalSineProfile;
using TableValues=std::array<float,16'384u>;
using Coords=QuestVr::OriginalModelCoords;
using Vec=PortableModelVec3;
std::size_t checks{},hardwareComparisons{},profileBuilderCases{};
// Complete independently generated canonical little-endian payload.
inline constexpr std::uint64_t GoldenFingerprint=0xefa5f38af2167897ull;
void Require(bool value,const char* why) {++checks;if(!value) throw std::runtime_error(why);}
std::uint32_t FloatBits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(Vec a,Vec b) {return FloatBits(a.x)==FloatBits(b.x)&&FloatBits(a.y)==FloatBits(b.y)&&FloatBits(a.z)==FloatBits(b.z);}
bool Same(const Coords& a,const Coords& b) {
    return Same(a.origin,b.origin)&&Same(a.xAxis,b.xAxis)&&Same(a.yAxis,b.yAxis)&&Same(a.zAxis,b.zAxis);
}
std::uint64_t Fingerprint(const TableValues& values) {
    std::uint64_t hash=14695981039346656037ull;
    for(const auto value:values) {
        const auto bits=FloatBits(value);
        for(unsigned byte=0u;byte<4u;++byte) {hash^=(bits>>(byte*8u))&255u;hash*=1099511628211ull;}
    }
    return hash;
}
void Integrity() {
    static_assert(Profile::Bits.size()==16'384u&&Profile::Values.size()==16'384u);
    static_assert(sizeof(float)==4u&&std::numeric_limits<float>::is_iec559);
    const auto table=Profile::Table();Require(table.size()==16'384u,"Profile span has wrong native table length");
    for(std::size_t i=0u;i<table.size();++i) {
        Require(FloatBits(table[i])==Profile::Bits[i]&&FloatBits(Profile::Values[i])==Profile::Bits[i],
            "Profile constexpr Float values differ from stored source bits");
        Require(std::isfinite(table[i])&&table[i]>=-1.0f&&table[i]<=1.0f,"Profile contains nonfinite/out-of-sine-range entry");
    }
    Require(FloatBits(table[0])==0u,"Original first sine entry is not positive zero");
    Require(FloatBits(table[4096])==0x3f800000u&&FloatBits(table[12288])==0xbf800000u,
        "Original exact quarter-turn sine entries differ");
    Require(table[8192]!=0.0f&&std::fabs(table[8192])<1.0e-12f,
        "Original half-turn residual was replaced with a fabricated exact zero");
    Require(GoldenFingerprint!=0u&&Fingerprint(Profile::Values)==GoldenFingerprint,
        "Complete profile canonical little-endian fingerprint differs from generated golden");
}
void Consumers() {
    const auto table=Profile::Table();
    const auto identity=QuestVr::IdentityOriginalModelCoords();
    Require(Same(QuestVr::BuildOriginalActorCoords({0,0,0},{0,0,0},table),identity),
        "Explicit profile did not support zero-rotation Actor construction");
    auto translated=identity;translated.origin={-2,3,-4};
    Require(Same(QuestVr::BuildOriginalActorCoords({2,-3,4},{0,0,0},table),translated),
        "Explicit profile Actor translation is not original Origin-Location");
    const auto quarter=QuestVr::BuildOriginalActorCoords({0,0,0},{0,16'384,0},table);
    Require(FloatBits(quarter.xAxis.x)==FloatBits(table[8192])&&quarter.xAxis.y==-1.0f&&
        quarter.yAxis.x==1.0f&&FloatBits(quarter.yAxis.y)==FloatBits(table[8192])&&quarter.zAxis.z==1.0f,
        "Explicit profile quarter-yaw lost native table residual/axis convention");
    const QuestVr::OriginalBrushScale unit{};
    Require(Same(QuestVr::BuildOriginalBrushCoords({2,-3,4},{0,0,0},{1,2,3},unit,unit,table),
        {{-1,5,-1},{1,0,0},{0,1,0},{0,0,1}}),"Explicit profile Brush pivot/builder functionality differs");
    for(const auto angle:{std::int32_t{0},std::int32_t{1},std::int32_t{-1},std::int32_t{3},std::int32_t{4},
        std::int32_t{16'383},std::int32_t{16'384},std::int32_t{65'535},std::int32_t{65'536},
        std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()}) {
        const auto built=QuestVr::BuildOriginalActorCoords({2,-3,4},{angle,angle,angle},table);
        QuestVr::ValidateOriginalModelCoords(built);++profileBuilderCases;
        Require(Same(built,QuestVr::BuildOriginalActorCoords({2,-3,4},{angle,angle,angle},Profile::Values)),
            "Explicit profile span versus explicit full-array consumer differs");
    }
    // A separate caller-supplied table must actually be consumed: builders
    // have no hidden global/default table or automatic profile selection.
    auto alternate=Profile::Values;alternate[0]=0.25f;
    Require(!Same(QuestVr::BuildOriginalActorCoords({2,-3,4},{0,0,0},alternate),translated),
        "Builder ignored its explicitly supplied sine table");
    Require(FloatBits(Profile::Table()[0])==0u&&Fingerprint(Profile::Values)==GoldenFingerprint,
        "Caller table mutation changed immutable profile storage");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct Cpu {std::array<char,13u> vendor{};std::uint32_t signature{};bool available{};};
Cpu ReadCpu() {
    Cpu result;unsigned eax{},ebx{},ecx{},edx{};
    if(!__get_cpuid(0u,&eax,&ebx,&ecx,&edx)) return result;
    const std::array<std::uint32_t,3u> words{ebx,edx,ecx};
    for(std::size_t word=0u;word<words.size();++word) for(unsigned byte=0u;byte<4u;++byte)
        result.vendor[word*4u+byte]=static_cast<char>((words[word]>>(byte*8u))&255u);
    if(!__get_cpuid(1u,&eax,&ebx,&ecx,&edx)) return result;
    result.signature=eax;result.available=true;return result;
}
struct Precision {
    unsigned short original{};
    explicit Precision(unsigned precision) {
        asm volatile("fnstcw %0":"=m"(original));
        const auto mode=static_cast<unsigned short>((original&~0x0f00u)|precision|0x003fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fldcw %0"::"m"(original));}
    Precision(const Precision&)=delete;Precision& operator=(const Precision&)=delete;
};
TableValues HardwareTable(unsigned precision) {
    // Independent transcription of Core1014431E..346 and appSin10123240:
    // FILD integerIndex; FMUL actualDoubleStep; FSTP Double angle;
    // FLD Double angle; FSIN; FSTP Float table entry. Never std::sin, symmetry,
    // production table data, cardinal replacement or production arithmetic.
    Precision selected(precision);
    const double step=std::bit_cast<double>(std::uint64_t{0x3f3921fb54442d18ull});
    TableValues result{};
    for(std::int32_t index=0;index<16'384;++index) {
        double angle{};float output{};
        asm volatile("fildl %1; fmull %2; fstpl %0":"=m"(angle):"m"(index),"m"(step):"st");
        asm volatile("fldl %1; fsin; fstps %0":"=m"(output):"m"(angle):"st");
        Require(std::isfinite(output)&&output>=-1.0f&&output<=1.0f,"Independent FSIN generated invalid value");
        result[static_cast<std::size_t>(index)]=output;
    }
    return result;
}
void Hardware() {
    const auto cpu=ReadCpu();
    if(!cpu.available||std::string(cpu.vendor.data())!="GenuineIntel"||cpu.signature!=0x000306a9u) {
        std::cout<<"SKIP exact FSIN hardware comparison: requires GenuineIntel CPUID306A9, detected "
            <<(cpu.available?cpu.vendor.data():"unavailable")<<" signature=0x"<<std::hex<<cpu.signature<<std::dec
            <<". Portable full-table integrity and explicit-profile builders still checked.\n";return;
    }
    const auto pc64=HardwareTable(0x0300u),pc53=HardwareTable(0x0200u);
    std::size_t precisionDifferences{},pc53ProfileDifferences{};
    for(std::size_t index=0u;index<pc64.size();++index) {
        Require(FloatBits(pc64[index])==Profile::Bits[index],"Complete independent PC64 FSIN profile differs at an entry");
        ++hardwareComparisons;
        if(FloatBits(pc64[index])!=FloatBits(pc53[index])) ++precisionDifferences;
        if(FloatBits(pc53[index])!=Profile::Bits[index]) ++pc53ProfileDifferences;
    }
    Require(Fingerprint(pc64)==GoldenFingerprint,"Independent complete PC64 FSIN fingerprint differs");
    std::cout<<"PASS GenuineIntel CPUID306A9 independent FSIN profile: "<<hardwareComparisons
        <<" exact entries; PC53-vs-PC64 differences="<<precisionDifferences
        <<", PC53-vs-profile differences="<<pc53ProfileDifferences
        <<". Original static-initializer precision/lifecycle remains separately audited.\n";
}
#else
void Hardware() {
    std::cout<<"SKIP exact FSIN hardware comparison: non-GNU-x86 target; portable full-table integrity "
        <<"and explicit-profile builders still checked.\n";
}
#endif
} // namespace
int main() {
    try {
        Integrity();Consumers();Hardware();
        std::cout<<"PASS explicit original sine profile: "<<checks<<" checks, "<<profileBuilderCases
            <<" platform-independent angle cases, "<<hardwareComparisons<<" CPU-profile-gated hardware entries. "
            <<"No original executable or live engine integration claim.\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"Original sine profile failed: "<<error.what()<<'\n';return 1;}
}
