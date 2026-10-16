#include "quest_brush_coords.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

// Detached FCoords composition, FScale and sheer-snap controls. Expected math
// uses independent local hardware x87 instructions; no original DLL execution,
// production coordinate helpers, Actor builder or runtime identity is an oracle.
namespace {
using Vec=PortableModelVec3;
using Coords=QuestVr::OriginalModelCoords;
using Scale=QuestVr::OriginalBrushScale;
std::size_t checks{},refusals{},comparisons{},matchingRefusals{};
void Require(bool value,const std::string& why) {++checks;if(!value) throw std::runtime_error(why);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(float a,float b) {return Bits(a)==Bits(b);}
bool Same(Vec a,Vec b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Coords& a,const Coords& b) {
    return Same(a.origin,b.origin)&&Same(a.xAxis,b.xAxis)&&Same(a.yAxis,b.yAxis)&&Same(a.zAxis,b.zAxis);
}
Coords Identity() {return {{0,0,0},{1,0,0},{0,1,0},{0,0,1}};}
std::array<float*,12u> Fields(Coords& value) {
    return {&value.origin.x,&value.origin.y,&value.origin.z,&value.xAxis.x,&value.xAxis.y,&value.xAxis.z,
        &value.yAxis.x,&value.yAxis.y,&value.yAxis.z,&value.zAxis.x,&value.zAxis.y,&value.zAxis.z};
}
template<class F> void Refuse(F operation,const char* why) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,why);++refusals;
}
void Deterministic() {
    const auto identity=Identity();
    Require(Same(QuestVr::IdentityOriginalModelCoords(),identity),"Native UnitCoords literal differs");
    const Coords c{{3,-5,7},{2,1,0},{0,-3,0.5f},{4,0,1}};
    Require(Same(QuestVr::ComposeOriginalModelCoords(c,identity),c),"Composition with right identity differs");
    const Coords translated{{1,2,3},{1,0,0},{0,1,0},{0,0,1}};
    auto expected=c;expected.origin={2,-7,4};
    Require(Same(QuestVr::ComposeOriginalModelCoords(c,translated),expected),"Composition did not PointBy Origin and VectorBy axes");
    const Coords rotation{{0,0,0},{0,-1,0},{1,0,0},{0,0,1}};
    Require(Same(QuestVr::ComposeOriginalModelCoords(c,rotation),
        {{5,3,7},{-1,2,0},{3,0,0.5f},{0,4,1}}),"Composition transposed axes or used wrong multiplication order");
    Require(!Same(QuestVr::ComposeOriginalModelCoords(c,translated),QuestVr::ComposeOriginalModelCoords(translated,c)),
        "Composition silently became commutative");
    const Scale normal{{2,-4,0.5f},0,0};
    Require(Same(QuestVr::ScaleOriginalModelCoords(c,normal),
        {{1.5f,1.25f,14},{4,-4,0},{0,12,0.25f},{8,-0.0f,0.5f}}),
        "FScale did not componentwise multiply all axes then divide Origin");
    for(unsigned axis=0;axis<256u;++axis) {
        auto shear=identity;
        // Native stores the source shear coefficient at X.y/X.z/Y.x/Y.z/
        // Z.x/Z.y, then TRANSFORMS the receiver's axis vectors through it.
        // With identity receiver the resulting stored axes are transposed.
        switch(axis) {
            case 1: shear.yAxis.x=0.5f;break;case 2: shear.zAxis.x=0.5f;break;
            case 3: shear.xAxis.y=0.5f;break;case 4: shear.zAxis.y=0.5f;break;
            case 5: shear.xAxis.z=0.5f;break;case 6: shear.yAxis.z=0.5f;break;
            default: break;
        }
        Require(Same(QuestVr::ScaleOriginalModelCoords(identity,{{1,1,1},0.6f,static_cast<std::uint8_t>(axis)}),shear),
            "Original sheer-axis byte mapping/discard differs");
    }
    const auto minusZero=std::bit_cast<float>(0x80000000u);
    Require(Bits(QuestVr::SnapOriginalBrushSheer(minusZero))==0u,"Snapped deadzone did not return positive zero");
    Require(Bits(QuestVr::SnapOriginalBrushSheer(0.0f))==0u,"Snapped positive-zero deadzone differs");
    Require(QuestVr::SnapOriginalBrushSheer(-0.6f)==-0.5f&&QuestVr::SnapOriginalBrushSheer(0.6f)==0.5f,
        "Original half-step snap plateaus differ");
    Require(QuestVr::SnapOriginalBrushSheer(1.0f)==0.85f&&QuestVr::SnapOriginalBrushSheer(-1.0f)==-0.85f,
        "Original outer-range retained Double0.15 offset differs");
    Require(QuestVr::SnapOriginalBrushSheer(0.0f)==0.0f,"Original snap invented shear in deadzone");
    // Finite singular/zero coordinates are allowed by composition: no inverse,
    // determinant, orthonormality, axis normalization or substitute identity.
    Require(Same(QuestVr::ComposeOriginalModelCoords(Coords{},identity),Coords{}),"Finite singular lhs was refused/repaired");
    Require(Same(QuestVr::ComposeOriginalModelCoords(c,Coords{}),Coords{}),"Finite singular rhs was refused/repaired");
    Require(Same(QuestVr::ScaleOriginalModelCoords(Coords{},{{1,1,1},0,0}),Coords{}),"Finite zero coordinates were refused during scale");
    // Point-Origin remains retainedPC64 before products, not Float3.
    const Coords left{{16777216.0f,1,0},{1,0,0},{0,1,0},{0,0,1}};
    auto right=identity;right.origin.x=-1;right.xAxis={1,-16777216.0f,0};
    Require(QuestVr::ComposeOriginalModelCoords(left,right).origin.x==1.0f,
        "Composition prematurely Float-spilled PointBy delta");
    const auto huge=std::ldexp(1.0f,70);auto cancellation=identity;cancellation.xAxis={1,1,1};
    Require(QuestVr::ComposeOriginalModelCoords({{huge,-huge,1},{},{},{}},cancellation).origin.x==1.0f,
        "Composition reused ZYX plane dots instead of PointBy XYZ association");
    const auto maximum=std::numeric_limits<float>::max();
    auto overflow=identity;overflow.xAxis.x=maximum;
    Refuse([&]{QuestVr::ComposeOriginalModelCoords(overflow,overflow);},"Composition output Float overflow accepted");
    Refuse([&]{QuestVr::ScaleOriginalModelCoords(overflow,{{2,1,1},0,0});},"Axis scaling Float overflow accepted");
    auto divideOverflow=identity;divideOverflow.origin.x=maximum;
    Refuse([&]{QuestVr::ScaleOriginalModelCoords(divideOverflow,{{std::numeric_limits<float>::denorm_min(),1,1},0,0});},
        "Origin division Float overflow accepted");
    for(float zero:{0.0f,minusZero}) for(unsigned axis=0;axis<3u;++axis) {
        Scale invalid{{1,1,1},0,0};std::array<float*,3> fields{&invalid.scale.x,&invalid.scale.y,&invalid.scale.z};*fields[axis]=zero;
        Refuse([&]{QuestVr::ScaleOriginalModelCoords(c,invalid);},"Active zero Scale divisor was repaired/accepted");
        Refuse([&]{QuestVr::ScaleOriginalModelCoords(Coords{},invalid);},"ZeroOrigin/zeroScale0div0 was silently accepted");
    }
    for(float bad:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()}) {
        Refuse([&]{QuestVr::SnapOriginalBrushSheer(bad);},"Nonfinite active sheer snap accepted");
        for(std::size_t at=0;at<12u;++at) {
            auto invalid=identity;*Fields(invalid)[at]=bad;
            Refuse([&]{QuestVr::ComposeOriginalModelCoords(invalid,identity);},"Composition nonfinite lhs accepted");
            Refuse([&]{QuestVr::ComposeOriginalModelCoords(identity,invalid);},"Composition nonfinite rhs accepted");
            Refuse([&]{QuestVr::ScaleOriginalModelCoords(invalid,normal);},"Scale nonfinite captured coordinates accepted");
        }
        for(unsigned axis=0;axis<3u;++axis) {
            Scale invalid=normal;std::array<float*,3> fields{&invalid.scale.x,&invalid.scale.y,&invalid.scale.z};*fields[axis]=bad;
            Refuse([&]{QuestVr::ScaleOriginalModelCoords(c,invalid);},"Scale nonfinite multiplier/divisor accepted");
        }
        for(std::uint8_t axis:{std::uint8_t{1},std::uint8_t{6}})
            Refuse([&]{QuestVr::ScaleOriginalModelCoords(c,{{1,1,1},bad,axis});},"Scale ignored active nonfinite Snap comparison");
        for(std::uint8_t axis:{std::uint8_t{0},std::uint8_t{7},std::uint8_t{255}})
            Require(Same(QuestVr::ScaleOriginalModelCoords(c,{{1,1,1},bad,axis}),
                QuestVr::ScaleOriginalModelCoords(c,{{1,1,1},0,axis})),
                "Discarded unknown-axis Snap result invented a numeric refusal/output change");
    }
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Hardware {
using Real=long double;
struct Precision {
    unsigned short old{};
    Precision() {
        static_assert(std::numeric_limits<Real>::digits==64,"Brush oracle requires hardware x87PC64");
        asm volatile("fnstcw %0":"=m"(old));const auto mode=static_cast<unsigned short>((old&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fldcw %0"::"m"(old));}
    Precision(const Precision&)=delete;Precision& operator=(const Precision&)=delete;
};
Real Read(float value) {if(!std::isfinite(value)) throw std::runtime_error("x87 brush active Float nonfinite");return value;}
Real Add(Real a,Real b) {volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b) {volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b) {volatile Real x=a,y=b,z=x*y;return z;}
Real Div(Real a,Real b) {
    if(b==0.0L) throw std::runtime_error("x87 brush active zero divisor");
    volatile Real x=a,y=b,z=x/y;return z;
}
float Store(Real value) {
    float result;asm volatile("fldt %1; fstps %0":"=m"(result):"m"(value):"st");
    if(!std::isfinite(result)) throw std::runtime_error("x87 brush Float store overflow");
    return result;
}
Vec Vector(std::array<Real,3u> value,const Coords& rhs) {
    const auto component=[&](Vec axis) {
        return Store(Add(Add(Mul(value[0],Read(axis.x)),Mul(value[1],Read(axis.y))),Mul(value[2],Read(axis.z))));
    };
    return {component(rhs.xAxis),component(rhs.yAxis),component(rhs.zAxis)};
}
Vec Vector(Vec value,const Coords& rhs) {return Vector(std::array<Real,3u>{Read(value.x),Read(value.y),Read(value.z)},rhs);}
Vec Point(Vec value,const Coords& rhs) {
    return Vector(std::array<Real,3u>{Sub(Read(value.x),Read(rhs.origin.x)),Sub(Read(value.y),Read(rhs.origin.y)),
        Sub(Read(value.z),Read(rhs.origin.z))},rhs);
}
Coords Compose(const Coords& lhs,const Coords& rhs) {
    // FCoords*=FCoords1011A710..A91E. Each complete Float3 is stored before
    // the next sourcevector; rhs here is an independent captured VALUE.
    Coords result;result.origin=Point(lhs.origin,rhs);result.xAxis=Vector(lhs.xAxis,rhs);
    result.yAxis=Vector(lhs.yAxis,rhs);result.zAxis=Vector(lhs.zAxis,rhs);return result;
}
Real Snap(float value) {
    const auto s=Read(value);
    // Original compares Float input to actual Double constants. DecimalFloat
    // threshold literals would classify their nearest Float neighbours wrongly.
    const auto negative65=std::bit_cast<double>(std::uint64_t{0xbfe4cccccccccccdull});
    const auto positive65=std::bit_cast<double>(std::uint64_t{0x3fe4cccccccccccdull});
    const auto positive15=std::bit_cast<double>(std::uint64_t{0x3fc3333333333333ull});
    const auto negative55=std::bit_cast<double>(std::uint64_t{0xbfe199999999999aull});
    const auto positive55=std::bit_cast<double>(std::uint64_t{0x3fe199999999999aull});
    const auto negative05=std::bit_cast<double>(std::uint64_t{0xbfa999999999999aull});
    const auto positive05=std::bit_cast<double>(std::uint64_t{0x3fa999999999999aull});
    if(s<negative65) return Add(s,positive15);
    if(s>positive65) return Sub(s,positive15);
    if(s<negative55) return -0.5f;
    if(s>positive55) return 0.5f;
    if(s<negative05) return Add(s,positive05);
    if(s>positive05) return Sub(s,positive05);
    return 0.0f;
}
Coords ScaleBy(const Coords& input,const Scale& scale) {
    auto shear=Identity();
    // For none/unknown axes the native computed Snap is discarded without a
    // Floatstore or numeric use. Portable finite-domain deliberately omits
    // that unused masked-FP status; still compose full unit coordinates.
    if(scale.sheerAxis>=1u&&scale.sheerAxis<=6u) {
        const auto retained=Snap(scale.sheerRate);
        switch(scale.sheerAxis) {
            case 1: shear.xAxis.y=Store(retained);break;case 2: shear.xAxis.z=Store(retained);break;
            case 3: shear.yAxis.x=Store(retained);break;case 4: shear.yAxis.z=Store(retained);break;
            case 5: shear.zAxis.x=Store(retained);break;case 6: shear.zAxis.y=Store(retained);break;
        }
    }
    auto result=Compose(input,shear);
    const auto multiply=[&](Vec value) {
        return Vec{Store(Mul(Read(value.x),Read(scale.scale.x))),Store(Mul(Read(value.y),Read(scale.scale.y))),
            Store(Mul(Read(value.z),Read(scale.scale.z)))};
    };
    result.xAxis=multiply(result.xAxis);result.yAxis=multiply(result.yAxis);result.zAxis=multiply(result.zAxis);
    result.origin={Store(Div(Read(result.origin.x),Read(scale.scale.x))),Store(Div(Read(result.origin.y),Read(scale.scale.y))),
        Store(Div(Read(result.origin.z),Read(scale.scale.z)))};return result;
}
} // namespace Hardware
template<class A,class E> void Compare(A actual,E expected,const std::string& label) {
    using Value=decltype(actual());Value a{},b{};bool refused{},expectedRefusal{};
    try {a=actual();} catch(const std::exception&) {refused=true;}
    try {b=expected();} catch(const std::exception&) {expectedRefusal=true;}
    Require(refused==expectedRefusal,"Brush finite-domain refusal differs: "+label);
    if(refused) {++matchingRefusals;return;}
    Require(Same(a,b),"Brush exact independent x87 result differs: "+label);++comparisons;
}
std::uint64_t Next(std::uint64_t& state) {state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
float Number(std::uint64_t& state,bool broad) {
    const auto value=static_cast<std::uint32_t>(Next(state));
    return broad?std::bit_cast<float>((value&0x807fffffu)|(((value>>23u)%255u)<<23u)):
        static_cast<float>(static_cast<int>(value%16'777'217u)-8'388'608)/32.0f;
}
void Differential() {
    Hardware::Precision precision;
    // Test immediately adjacent Float representations on BOTH sides of every
    // Double threshold. The Float rounded decimal itself is NOT the boundary.
    for(double threshold:{-0.65,-0.55,-0.05,0.05,0.55,0.65}) {
        const auto rounded=static_cast<float>(threshold);
        for(float s:{std::nextafter(rounded,-std::numeric_limits<float>::infinity()),rounded,
            std::nextafter(rounded,std::numeric_limits<float>::infinity())}) {
            Compare([&]{return QuestVr::SnapOriginalBrushSheer(s);},[&]{return Hardware::Store(Hardware::Snap(s));},"Snap threshold neighbour");
            for(std::uint8_t axis=0u;axis<8u;++axis) {
                const Scale scale{{2,-3,0.25f},s,axis};const Coords c{{3,-5,7},{2,1,0},{0,-3,0.5f},{4,0,1}};
                Compare([&]{return QuestVr::ScaleOriginalModelCoords(c,scale);},[&]{return Hardware::ScaleBy(c,scale);},"Scale threshold neighbour");
            }
        }
    }
    const std::array<float,9> edge{0.0f,std::bit_cast<float>(0x80000000u),std::numeric_limits<float>::denorm_min(),
        -std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::min(),-std::numeric_limits<float>::min(),
        1.0f,std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};
    for(float x:edge) for(float y:edge) {
        const Coords lhs{{x,y,x},{x,y,x},{y,x,y},{x,x,y}},rhs{{y,x,y},{y,x,y},{x,y,x},{y,y,x}};
        Compare([&]{return QuestVr::ComposeOriginalModelCoords(lhs,rhs);},[&]{return Hardware::Compose(lhs,rhs);},"Compose signedzero/subnormal/extreme");
        Compare([&]{return QuestVr::SnapOriginalBrushSheer(x);},[&]{return Hardware::Store(Hardware::Snap(x));},"Snap signedzero/subnormal/extreme");
        for(std::uint8_t axis:{std::uint8_t{0},std::uint8_t{1},std::uint8_t{6},std::uint8_t{255}}) {
            const Scale scale{{x,y,1},y,axis};
            Compare([&]{return QuestVr::ScaleOriginalModelCoords(lhs,scale);},[&]{return Hardware::ScaleBy(lhs,scale);},"Scale signedzero/subnormal/extreme");
        }
    }
    std::uint64_t state=0xa81357c6de9024bfull;
    for(unsigned index=0;index<14'000u;++index) {
        const bool broad=index<4'000u;const auto number=[&]{return Number(state,broad);};
        const auto vector=[&]{return Vec{number(),number(),number()};};
        const Coords lhs{vector(),vector(),vector(),vector()},rhs{vector(),vector(),vector(),vector()};
        const Scale scale{vector(),number(),static_cast<std::uint8_t>(Next(state))};
        const auto label=std::to_string(index);
        Compare([&]{return QuestVr::ComposeOriginalModelCoords(lhs,rhs);},[&]{return Hardware::Compose(lhs,rhs);},"Compose"+label);
        Compare([&]{return QuestVr::ScaleOriginalModelCoords(lhs,scale);},[&]{return Hardware::ScaleBy(lhs,scale);},"Scale"+label);
        Compare([&]{return QuestVr::SnapOriginalBrushSheer(scale.sheerRate);},
            [&]{return Hardware::Store(Hardware::Snap(scale.sheerRate));},"Snap"+label);
    }
    Require(comparisons>30'000u,"Independent brush tests lack enough successful finite results");
    Require(matchingRefusals>100u,"Independent brush broad arithmetic missed finite-store/division refusals");
}
#else
void Differential() {std::cout<<"SKIP independent brush hardware x87 oracle on non-x86/GNU target.\n";}
#endif
} // namespace
int main() {
    try {
        Deterministic();Differential();
        std::cout<<"PASS original brush Compose/Scale/Snap: "<<checks<<" checks, "<<refusals<<" explicit safety refusals, "
            <<comparisons<<" exact independent x87 comparisons, "<<matchingRefusals<<" matching finite-math refusals. "
            <<"Not Actor/Brush builder or runtime acceptance.\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"Original brush coordinates failed: "<<error.what()<<'\n';return 1;}
}
