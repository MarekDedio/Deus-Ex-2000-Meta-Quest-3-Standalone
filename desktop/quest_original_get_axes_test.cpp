#include "quest_original_get_axes.h"
#include "quest_original_sine_table.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Detached Core229 math controls. The independent oracle below uses only local
// x87 PC64 instructions/Float stores, never the production Compose/transform
// helpers, original DLL code or a host-libm Euler formula.
namespace {
using Vec=PortableModelVec3;
using Coords=QuestVr::OriginalModelCoords;
using Rotation=QuestVr::OriginalBrushRotation;
using Result=QuestVr::OriginalGetAxesResult;
using Values=std::array<float,16384u>;
std::size_t checks{},refusals{},hardwareCases{},portableCases{};
void Require(bool value,const char* why) {++checks;if(!value)throw std::runtime_error(why);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
bool Same(Vec a,Vec b){return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
bool Same(const Result& a,const Result& b){return Same(a.xAxis,b.xAxis)&&Same(a.yAxis,b.yAxis)&&Same(a.zAxis,b.zAxis);}
Coords Unit(){return {{0,0,0},{1,0,0},{0,1,0},{0,0,1}};}
Result Axes(Vec x,Vec y,Vec z){Result result;result.xAxis=x;result.yAxis=y;result.zAxis=z;return result;}
template<class F>void Refuse(F operation,const char* why){bool refused{};try{operation();}catch(const std::exception&){refused=true;}Require(refused,why);++refusals;}
std::int32_t Wrap(std::int32_t value,std::uint32_t offset){return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value)+offset);}
Result Actual(Rotation rotation){return QuestVr::ComputeOriginalGetAxes(rotation,Unit(),QuestVr::OriginalSineProfile::Table());}
Values CardinalTable(){Values values{};values[4096u]=1;values[12288u]=-1;return values;}
void Deterministic(){
    const auto table=CardinalTable();
    Require(Same(QuestVr::ComputeOriginalGetAxes({},Unit(),table),Axes({1,0,0},{0,1,0},{0,0,1})),"Zero-angle axes/signs differ");
    Require(Same(QuestVr::ComputeOriginalGetAxes({0,16384,0},Unit(),table),Axes({0,1,0},{-1,0,0},{0,0,1})),"Positive quarter yaw has wrong sign/axis");
    Require(Same(QuestVr::ComputeOriginalGetAxes({16384,0,0},Unit(),table),Axes({0,0,1},{0,1,0},{-1,0,0})),"Positive quarter pitch has wrong sign/axis");
    Require(Same(QuestVr::ComputeOriginalGetAxes({0,0,16384},Unit(),table),Axes({1,0,0},{0,0,-1},{0,1,0})),"Positive quarter roll has wrong sign/axis");
    Require(Same(Actual({}),Axes({1,0,0},{0,1,0},{0,0,1})),"Original profile zero axes differ");
    const auto quarter=Actual({0,16384,0});
    Require(Bits(quarter.xAxis.x)==QuestVr::OriginalSineProfile::Bits[8192u]&&quarter.xAxis.y==1.0f&&quarter.yAxis.x==-1.0f,
        "Original quarter-turn residual was zero-corrected");
    const auto pure=QuestVr::ComputeOriginalGetAxes({1234,5678,9012},Coords{},QuestVr::OriginalSineProfile::Table());
    Require(Same(pure,Axes({0,0,0},{0,0,0},{0,0,0})),"Finite zero captured coordinates were normalized/replaced");
    const auto finite=Actual({1234,5678,9012});
    Require(!Same(finite,Actual({9012,5678,1234})),"Mixed rotation unexpectedly ignores roll/pitch order");
    Require(!Same(finite,Actual({1234,9012,5678})),"Mixed rotation unexpectedly ignores yaw/roll order");
    for(std::uint32_t bucket=0u;bucket<16384u;++bucket){
        const auto base=static_cast<std::int32_t>(bucket*4u);const auto reference=Actual({base,Wrap(base,1024u),Wrap(base,4096u)});
        for(std::uint32_t residue=1u;residue<4u;++residue)
            Require(Same(reference,Actual({Wrap(base,residue),Wrap(base,1024u+residue),Wrap(base,4096u+residue)})),
                "SAR2 table-index residue changed axes");
        if((bucket&255u)==0u){
            Require(Same(reference,Actual({Wrap(base,65536u),Wrap(base,1024u+0x80000000u),Wrap(base,4096u+0xffff0000u)})),
                "Full signed32 angle wrapping changed low16 table indexing");
        }
        ++portableCases;
    }
    constexpr std::array<std::int32_t,14u> limits{std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::min()+1,-65537,-65536,-65535,-4,-1,0,1,3,4,65535,
        std::numeric_limits<std::int32_t>::max()-1,std::numeric_limits<std::int32_t>::max()};
    for(const auto angle:limits){
        const auto low=static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(angle)&0xffffu);
        Require(Same(Actual({angle,angle,angle}),Actual({low,low,low})),"Signed full-range rotation was clamped/overflowed");
    }
}
void Providers(){
    Values values=CardinalTable();values[123u]=std::numeric_limits<float>::quiet_NaN();
    std::vector<std::uint32_t> read;std::size_t units{},work{};
    QuestVr::OriginalGetAxesQueries queries;
    queries.unitCoords=[&]{++units;return Unit();};queries.sineTable=[&](auto index){read.push_back(index);return values[index];};
    queries.work=[&]{++work;};
    const auto result=QuestVr::ComputeOriginalGetAxes({4,8,12},queries);
    Require(read==std::vector<std::uint32_t>{3,4099,4099,3,1,4097,4097,1,4098,4098,2},"Original repeated table-load order changed");
    Require(units==1u&&work==15u&&result.stats.steps==15u&&result.stats.unitReads==1u&&result.stats.tableReads==11u&&result.stats.compositions==3u,
        "Original GetAxes provider/work accounting differs");
    auto changed=Unit();changed.xAxis={2,3,4};changed.yAxis={5,6,7};changed.zAxis={8,9,10};
    const auto captured=changed;
    queries.unitCoords=[&]{++units;return changed;};queries.sineTable=[&](auto index){changed=Coords{};return values[index];};
    Require(Same(QuestVr::ComputeOriginalGetAxes({},queries),Axes(captured.xAxis,captured.yAxis,captured.zAxis)),
        "UnitCoords was reread after table-provider side effects");
    for(std::size_t limit=0u;limit<15u;++limit){
        std::size_t steps{};auto bounded=queries;bounded.maximumSteps=limit;bounded.unitCoords=[] {return Unit();};
        bounded.work=[&]{++steps;};bounded.sineTable=[&](auto index){return values[index];};
        Refuse([&]{QuestVr::ComputeOriginalGetAxes({},bounded);},"Finite local native-work cutoff accepted");
        Require(steps==limit,"Work refusal invoked a later provider");
    }
    auto shared=queries;QuestVr::NativeWork::Budget budget(14u);shared.work=[&]{budget.Step();};
    shared.unitCoords=[] {return Unit();};shared.sineTable=[&](auto index){return values[index];};
    Refuse([&]{QuestVr::ComputeOriginalGetAxes({},shared);},"Shared native work exhaustion accepted");
    Require(budget.Read().used==14u&&budget.Read().exhausted,"Shared native-work failure was not poisoned");
    auto missing=queries;missing.unitCoords={};work=0;missing.work=[&]{++work;};
    Refuse([&]{QuestVr::ComputeOriginalGetAxes({},missing);},"Missing UnitCoords provider accepted");Require(!work,"Missing first provider performed work");
    missing.unitCoords=[] {return Unit();};missing.sineTable={};work=0;
    Refuse([&]{QuestVr::ComputeOriginalGetAxes({},missing);},"Missing reached table provider accepted");Require(work==1u,"Missing table provider reached later math");
    missing=queries;missing.maximumSteps=QuestVr::NativeWork::MaximumLimit+1u;work=0;missing.work=[&]{++work;};
    Refuse([&]{QuestVr::ComputeOriginalGetAxes({},missing);},"Hostile native-work limit accepted");Require(!work,"Invalid work bound invoked providers");
    for(float invalid:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto bad=values;bad[0u]=invalid;Refuse([&]{QuestVr::ComputeOriginalGetAxes({},Unit(),bad);},"Consumed nonfinite sine-table value accepted");
        auto origin=Unit();origin.origin.x=invalid;Refuse([&]{QuestVr::ComputeOriginalGetAxes({},origin,values);},"Consumed nonfinite UnitCoords Origin accepted");
        auto axis=Unit();axis.yAxis.z=invalid;Refuse([&]{QuestVr::ComputeOriginalGetAxes({},axis,values);},"Consumed nonfinite UnitCoords axis accepted");
    }
    auto large=values;large[0u]=std::numeric_limits<float>::max();large[4096u]=std::numeric_limits<float>::max();
    Refuse([&]{QuestVr::ComputeOriginalGetAxes({},Unit(),large);},"Active final Float overflow accepted");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Hardware {
using Real=long double;
struct Precision{
    unsigned short old{};
    Precision(){static_assert(std::numeric_limits<Real>::digits==64);asm volatile("fnstcw %0":"=m"(old));
        const auto mode=static_cast<unsigned short>((old&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
    ~Precision(){asm volatile("fldcw %0"::"m"(old));}
    Precision(const Precision&)=delete;Precision& operator=(const Precision&)=delete;
};
Real Add(Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,z=x*y;return z;}
float Store(Real value){float result;asm volatile("fldt %1; fstps %0":"=m"(result):"m"(value):"st");return result;}
Vec Transform(Vec value,const Coords& rhs,bool point){
    const Real x=point?Sub(value.x,rhs.origin.x):value.x,
        y=point?Sub(value.y,rhs.origin.y):value.y,z=point?Sub(value.z,rhs.origin.z):value.z;
    const auto dot=[&](Vec axis){return Store(Add(Add(Mul(x,axis.x),Mul(y,axis.y)),Mul(z,axis.z)));};
    return {dot(rhs.xAxis),dot(rhs.yAxis),dot(rhs.zAxis)};
}
Coords Compose(const Coords& lhs,const Coords& rhs){return {Transform(lhs.origin,rhs,true),Transform(lhs.xAxis,rhs,false),
    Transform(lhs.yAxis,rhs,false),Transform(lhs.zAxis,rhs,false)};}
float Sin(const Values& table,std::int32_t angle,bool cosine=false){
    const auto low=static_cast<std::uint32_t>(static_cast<std::uint16_t>(angle));
    const auto index=((low+(cosine?16384u:0u))&65535u)/4u;return table[index];
}
Result Oracle(Rotation rotation,const Coords& unit,const Values& table){
    auto current=unit;
    const auto rs=Sin(table,rotation.roll),rc=Sin(table,rotation.roll,true);
    current=Compose(current,{{0,0,0},{1,0,0},{0,rc,rs},{0,-rs,rc}});
    const auto ps=Sin(table,rotation.pitch),pc=Sin(table,rotation.pitch,true);
    current=Compose(current,{{0,0,0},{pc,0,-ps},{0,1,0},{ps,0,pc}});
    const auto ys=Sin(table,rotation.yaw),yc=Sin(table,rotation.yaw,true);
    current=Compose(current,{{0,0,0},{yc,-ys,0},{ys,yc,0},{0,0,1}});
    return Axes(current.xAxis,current.yAxis,current.zAxis);
}
}
void Independent(){
    Hardware::Precision precision;
    const auto& table=QuestVr::OriginalSineProfile::Values;
    std::uint32_t random=0x5e49c27du;
    const auto next=[&](){random=random*1664525u+1013904223u;return std::bit_cast<std::int32_t>(random);};
    for(unsigned i=0u;i<4096u;++i){
        const Rotation rotation{next(),next(),next()};
        Require(Same(Actual(rotation),Hardware::Oracle(rotation,Unit(),table)),"Independent x87 mixed Euler/Float stores differ");++hardwareCases;
    }
    // Small signed-zero/dyadic custom table and arbitrary finite captured
    // coordinates distinguish full compositions from shortcut Euler formulas.
    Values signedTable=CardinalTable();signedTable[0u]=std::bit_cast<float>(0x80000000u);
    for(const auto angle:{0,1,3,4,-1,-4,16384,32768,49152}){
        const Rotation rotation{angle,Wrap(angle,16384u),Wrap(angle,32768u)};
        Require(Same(QuestVr::ComputeOriginalGetAxes(rotation,Unit(),signedTable),Hardware::Oracle(rotation,Unit(),signedTable)),
            "Independent signed-zero native sign/store schedule differs");++hardwareCases;
    }
    Coords custom{{16777216.0f,1,-0.0f},{1,-16777216.0f,0},{0.5f,-3,7},{-0.0f,5,9}};
    for(unsigned i=0u;i<256u;++i){const Rotation rotation{next(),next(),next()};
        Require(Same(QuestVr::ComputeOriginalGetAxes(rotation,custom,table),Hardware::Oracle(rotation,custom,table)),
            "Independent captured UnitCoords composition differs");++hardwareCases;}
}
#else
void Independent(){std::cout<<"SKIP independent hardware x87 PC64 oracle on this platform; portable controls still run\n";}
#endif
}
int main(){try{Deterministic();Providers();Independent();
    std::cout<<"PASS original GetAxes math checks="<<checks<<" refusals="<<refusals<<" portableCases="<<portableCases<<" hardwareCases="<<hardwareCases<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<"FAIL original GetAxes math: "<<error.what()<<'\n';return 1;}}
