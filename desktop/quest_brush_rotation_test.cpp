#include "quest_brush_coords.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>

// Rotation/captured-coordinate construction controls only. Original DLL/EXE is
// never loaded/executed. The table is independently synthesized using the
// statically audited hardware instructions, NOT std::sin or production math.
// The supplied table's lifecycle/CPU provenance is deliberately not fabricated
// as an authoritative native GMath bridge by this detached test.
namespace {
using Vec=PortableModelVec3;
using Rotation=QuestVr::OriginalBrushRotation;
using Coords=QuestVr::OriginalModelCoords;
using Table=std::array<float,16'384u>;
std::size_t checks{},samples{},refusals{},aliases{};
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(Vec a,Vec b) {return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
bool Same(const Coords& a,const Coords& b) {
    return Same(a.origin,b.origin)&&Same(a.xAxis,b.xAxis)&&Same(a.yAxis,b.yAxis)&&Same(a.zAxis,b.zAxis);
}

using Real=long double;
struct Precision {
    unsigned short original{};
    Precision() {
        static_assert(std::numeric_limits<Real>::digits==64,"Rotation oracle requires actual x87 extended precision");
        asm volatile("fnstcw %0":"=m"(original));
        const auto mode=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fldcw %0"::"m"(original));}
    Precision(const Precision&)=delete;
    Precision& operator=(const Precision&)=delete;
};
Real Add(Real a,Real b) {volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b) {volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b) {volatile Real x=a,y=b,z=x*y;return z;}
Real Div(Real a,Real b) {volatile Real x=a,y=b,z=x/y;return z;}
float Store(Real value) {
    volatile float result=static_cast<float>(value);
    if(!std::isfinite(result)) throw std::runtime_error("Rotation x87 oracle Float overflow");
    return result;
}
Table HardwareTable() {
    // FGlobalMath1014431e..346: i*Double(3F3921FB54442D18) is spilled
    // Double before appSin10123240 performs FLDL;FSIN;RET. The caller stores
    // Float directly from that retained result. Every entry is independently
    // generated; do not force symmetry or exact-zero cardinals afterward.
    const double step=std::bit_cast<double>(std::uint64_t{0x3f3921fb54442d18ull});
    Table result{};
    for(std::int32_t i=0;i<static_cast<std::int32_t>(result.size());++i) {
        double angle{};float value{};
        asm volatile("fildl %1; fmull %2; fstpl %0":"=m"(angle):"m"(i),"m"(step):"st");
        asm volatile("fldl %1; fsin; fstps %0":"=m"(value):"m"(angle):"st");
        Require(std::isfinite(value)&&value>=-1.0f&&value<=1.0f,"Hardware sine table entry is invalid");
        result[static_cast<std::size_t>(i)]=value;
    }
    Require(Bits(result[0])==0u&&result[4096]==1.0f&&result[12288]==-1.0f,
        "Independent FSIN table missed exact initial/quarter entries");
    Require(result[8192]!=0.0f,"Table half-turn residual was replaced by an invented exact zero");
    return result;
}
void CompareTablePrecision(const Table& pc64) {
    // FGlobalMath's STATIC-initializer control word has not been proven by
    // this audit. Compare the chosen PC64 profile with a complete fresh PC53
    // generation; report observed Float differences rather than assuming CRT
    // startup mode or declaring one universal initialized retail table.
    unsigned short previous{};asm volatile("fnstcw %0":"=m"(previous));
    struct Restore {
        unsigned short value;
        ~Restore() {asm volatile("fldcw %0"::"m"(value));}
    } restore{previous};
    const auto pc53=static_cast<unsigned short>((previous&~0x0f00u)|0x023fu);
    asm volatile("fldcw %0"::"m"(pc53));
    const auto alternative=HardwareTable();
    asm volatile("fldcw %0"::"m"(previous));
    std::size_t mismatches{};
    for(std::size_t i=0;i<pc64.size();++i) {
        ++checks;if(Bits(pc64[i])!=Bits(alternative[i])) ++mismatches;
    }
    std::cout<<"Hardware sine table PC53-vs-PC64 Float-bit differences: "<<mismatches
        <<" /16384; original static-initializer control word remains unproven.\n";
}
std::size_t Index(std::int32_t angle,bool cosine=false) {
    // Independent PERIOD arithmetic, not the native SAR/AND implementation:
    // normalize the signed full-range angle (and quarter-turn) modulo65536,
    // then divide its nonnegative residue by4. Correct for negative/overflow
    // inputs without signed right-shift or signed addition overflow.
    std::int64_t phase=angle;if(cosine) phase+=16'384;
    phase%=65'536;if(phase<0) phase+=65'536;
    return static_cast<std::size_t>(phase/4);
}
float Lookup(const Table& table,std::int32_t angle,bool cosine=false) {return table[Index(angle,cosine)];}
struct ScalarCoords {Vec origin{},x{},y{},z{};};
ScalarCoords Unit() {return {{0,0,0},{1,0,0},{0,1,0},{0,0,1}};}
Vec Vector(Vec value,const ScalarCoords& by) {
    const auto component=[&](Vec row) {
        return Store(Add(Add(Mul(value.x,row.x),Mul(value.y,row.y)),Mul(value.z,row.z)));
    };
    return {component(by.x),component(by.y),component(by.z)};
}
Vec Point(Vec value,const ScalarCoords& by) {
    // PointBy subtraction remains retained until axis products; it is NOT a
    // Float3 delta. Stage Origins are +0 but still participate in the x87
    // subtraction/additions, making signed zeros observable in output.
    const auto x=Sub(value.x,by.origin.x),y=Sub(value.y,by.origin.y),z=Sub(value.z,by.origin.z);
    const auto component=[&](Vec row) {return Store(Add(Add(Mul(x,row.x),Mul(y,row.y)),Mul(z,row.z)));};
    return {component(by.x),component(by.y),component(by.z)};
}
void Compose(ScalarCoords& value,const ScalarCoords& by) {
    // Core FCoords *= FCoords transforms/stores Origin, XAxis, YAxis, ZAxis
    // separately; later stages consume these Float intermediates.
    value.origin=Point(value.origin,by);
    value.x=Vector(value.x,by);value.y=Vector(value.y,by);value.z=Vector(value.z,by);
}
ScalarCoords Yaw(const Table& table,std::int32_t angle) {
    const auto s=Lookup(table,angle),c=Lookup(table,angle,true);
    return {{0,0,0},{c,s,0},{-s,c,0},{0,0,1}};
}
ScalarCoords Pitch(const Table& table,std::int32_t angle) {
    const auto s=Lookup(table,angle),c=Lookup(table,angle,true);
    return {{0,0,0},{c,0,s},{0,1,0},{-s,0,c}};
}
ScalarCoords Roll(const Table& table,std::int32_t angle) {
    const auto s=Lookup(table,angle),c=Lookup(table,angle,true);
    return {{0,0,0},{1,0,0},{0,c,-s},{0,s,c}};
}
void Rotate(ScalarCoords& result,Rotation rotation,const Table& table) {
    Compose(result,Yaw(table,rotation.yaw));Compose(result,Pitch(table,rotation.pitch));Compose(result,Roll(table,rotation.roll));
}
ScalarCoords Translated(Vec location) {
    auto result=Unit();
    result.origin={Store(Sub(0.0f,location.x)),Store(Sub(0.0f,location.y)),Store(Sub(0.0f,location.z))};
    return result;
}
Coords ActorOracle(Vec location,Rotation rotation,const Table& table) {
    auto result=Translated(location);Rotate(result,rotation,table);
    return {result.origin,result.x,result.y,result.z};
}
void UnitScale(ScalarCoords& coords) {
    // Even unit scales execute full composition/products/divisions in retail.
    // Keep this extra signed-zero/store behavior separate from Actor.ToWorld.
    Compose(coords,Unit());
    const auto scale=[](Vec v) {return Vec{Store(Mul(v.x,1.0f)),Store(Mul(v.y,1.0f)),Store(Mul(v.z,1.0f))};};
    coords.x=scale(coords.x);coords.y=scale(coords.y);coords.z=scale(coords.z);
    coords.origin={Store(Div(coords.origin.x,1.0f)),Store(Div(coords.origin.y,1.0f)),Store(Div(coords.origin.z,1.0f))};
}
Coords UnitBrushOracle(Vec location,Rotation rotation,Vec pivot,const Table& table) {
    const Vec negativePivot{Store(-static_cast<Real>(pivot.x)),Store(-static_cast<Real>(pivot.y)),Store(-static_cast<Real>(pivot.z))};
    auto result=Translated(location);UnitScale(result);Rotate(result,rotation,table);UnitScale(result);
    result.origin={Store(Sub(result.origin.x,negativePivot.x)),Store(Sub(result.origin.y,negativePivot.y)),
        Store(Sub(result.origin.z,negativePivot.z))};
    return {result.origin,result.x,result.y,result.z};
}
void Check(Vec location,Rotation rotation,const Table& table) {
    const auto expected=ActorOracle(location,rotation,table);
    const auto actual=QuestVr::BuildOriginalActorCoords(location,rotation,std::span<const float,16'384u>(table));
    Require(Same(actual,expected),"Actor ToWorld differs from independent stage-by-stage hardware x87 oracle");++samples;
}
void Quantization(const Table& table) {
    // All16384 distinct table phases for EACH rotator axis, plus all two-bit
    // residue aliases in every64th cell. Native quantization drops two bits.
    for(std::int32_t phase=0;phase<16'384;++phase) {
        const std::int32_t angle=phase*4;const Vec location{11.25f,-7.75f,3.125f};
        Check(location,{angle,0,0},table);Check(location,{0,angle,0},table);Check(location,{0,0,angle},table);
        if(phase%64==0) {
            const auto baseline=QuestVr::BuildOriginalActorCoords(location,{angle,angle,angle},table);
            for(std::int32_t low=1;low<4;++low) {
                const auto actual=QuestVr::BuildOriginalActorCoords(location,{angle+low,angle+low,angle+low},table);
                Require(Same(actual,baseline),"Lower-two-bit rotator residue was not discarded");++aliases;
            }
        }
    }
    const std::array<std::int32_t,28u> edges{std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::min()+1,
        std::numeric_limits<std::int32_t>::min()+3,std::numeric_limits<std::int32_t>::min()+4,
        -131'073,-131'072,-65'537,-65'536,-65'535,-16'385,-16'384,-16'383,-5,-4,-3,-2,-1,0,1,2,3,4,5,
        65'535,65'536,std::numeric_limits<std::int32_t>::max()-3,std::numeric_limits<std::int32_t>::max()-1,
        std::numeric_limits<std::int32_t>::max()};
    for(const auto a:edges) {
        Check({-0.0f,0.0f,-0.0f},{a,a,a},table);
        Check({1,2,3},{a,0,0},table);Check({1,2,3},{0,a,0},table);Check({1,2,3},{0,0,a},table);
        const auto wrapped=static_cast<std::int32_t>(static_cast<std::uint32_t>(a)&0xffffu);
        Require(Same(QuestVr::BuildOriginalActorCoords({3,-5,7},{a,a,a},table),
            QuestVr::BuildOriginalActorCoords({3,-5,7},{wrapped,wrapped,wrapped},table)),"Full-range rotator wrap differs");
    }
}
std::uint32_t Next(std::uint32_t& seed) {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
void Mixed(const Table& table) {
    std::uint32_t seed=0xf39a72c1u;
    for(unsigned i=0;i<4096u;++i) {
        const auto p=std::bit_cast<std::int32_t>(Next(seed)),y=std::bit_cast<std::int32_t>(Next(seed)),
            r=std::bit_cast<std::int32_t>(Next(seed));
        const auto number=[&] {return static_cast<float>(static_cast<int>(Next(seed)%65'537u)-32'768)/128.0f;};
        const Vec location{number(),number(),number()};Check(location,{p,y,r},table);
        if(i%32u==0u) {
            const Vec pivot{number(),number(),number()};const QuestVr::OriginalBrushScale unit;
            const auto actual=QuestVr::BuildOriginalBrushCoords(location,{p,y,r},pivot,unit,unit,table);
            Require(Same(actual,UnitBrushOracle(location,{p,y,r},pivot,table)),
                "Unit-scale brush rotation/PrePivot differs from independent full-stage oracle");
        }
    }
    // Distinct arbitrary entries prove the consumer uses the SUPPLIED table,
    // exact axis/sign/order/quantized indices rather than host trig or a cached
    // magic table. These are explicit synthetic fixtures, not a real GMath.
    Table synthetic{};
    for(std::size_t i=0;i<synthetic.size();++i)
        synthetic[i]=static_cast<float>(static_cast<int>(i)-8192)/8192.0f;
    for(unsigned i=0;i<1024u;++i) {
        const auto p=std::bit_cast<std::int32_t>(Next(seed)),y=std::bit_cast<std::int32_t>(Next(seed)),
            r=std::bit_cast<std::int32_t>(Next(seed));
        Check({-1.25f,2.5f,-3.75f},{p,y,r},synthetic);
    }
    for(const float zero:{0.0f,-0.0f}) for(const auto p:{0,1,-1,16'384,-16'384,32'768})
        Check({zero,-zero,zero},{p,p,p},table);
}
template<class F> void Refuse(F run) {
    bool failed{};try {run();} catch(const std::exception&) {failed=true;}
    Require(failed,"Nonfinite active rotation-coordinate input silently succeeded");++refusals;
}
void Admission(const Table& table) {
    for(const auto bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()}) {
        for(unsigned component=0;component<3u;++component) {
            Vec location{};if(component==0) location.x=bad;if(component==1) location.y=bad;if(component==2) location.z=bad;
            Refuse([&]{QuestVr::BuildOriginalActorCoords(location,{0,0,0},table);});
        }
        auto poisoned=table;poisoned[0]=bad;
        Refuse([&]{QuestVr::BuildOriginalActorCoords({0,0,0},{0,0,0},poisoned);});
    }
    auto sparse=table;
    for(std::size_t i=0;i<sparse.size();++i)
        if(i!=0u&&i!=4096u) sparse[i]=std::numeric_limits<float>::quiet_NaN();
    Require(Same(QuestVr::BuildOriginalActorCoords({3,4,5},{0,0,0},sparse),ActorOracle({3,4,5},{0,0,0},table)),
        "Actor rotation validated inactive table entries instead of its six sampled lookups");
}
void Hardware() {
    Precision precision;const auto table=HardwareTable();CompareTablePrecision(table);
    Quantization(table);Mixed(table);Admission(table);
}
#else
void Hardware() {
    std::cout<<"SKIP independent rotation/table hardware controls: x86 GCC x87 required; no host-trig substitute.\n";
}
#endif
} // namespace
int main() {
    try {
        Hardware();std::cout<<"PASS original actor rotation "<<checks<<" checks, "<<samples
            <<" independent x87 actor cases, "<<aliases<<" quantization aliases, "<<refusals<<" explicit refusals.\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"Original actor rotation failed: "<<error.what()<<'\n';return 1;}
}
