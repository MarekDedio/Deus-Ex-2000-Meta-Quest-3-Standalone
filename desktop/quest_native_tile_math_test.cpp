#include "quest_native_tile_math.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

// Independent test-owned x87 instructions/arithmetic only. No original EXE or
// DLL is loaded or executed. This tests arithmetic, not Tile callbacks/layout.
namespace {
namespace Math=QuestVr::NativeTileMath;
using Wide=QuestVr::ActorTimerDetail::Wide;
std::size_t checks{},refusals{},portableChecks{},hardwareCases{};
void Require(bool value,const char* why) {
    ++checks;if(!value)throw std::runtime_error(why);
}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
template<class F>void Refuse(F operation,const char* why) {
    bool refused{};try{operation();}catch(const std::runtime_error&){refused=true;}
    Require(refused,why);++refusals;
}
void Deterministic() {
    const auto minimum=std::numeric_limits<std::int32_t>::min();
    const auto maximum=std::numeric_limits<std::int32_t>::max();
    for(float zero:{0.0f,-0.0f}) {
        Require(Bits(Math::Store(Math::Load(zero)))==Bits(zero),"Load/store changed zero sign");
        Require(Math::Truncate(Math::Load(zero))==0,"Signed zero did not truncate to integer zero");
    }
    Require(Math::Compare(Math::Load(-0.0f),Math::Load(0.0f))==0,"Signed zero compares unequal");
    Require(Bits(Math::Store(Math::Add(Math::Load(-0.0f),Math::Load(-0.0f))))==0x80000000u,
        "Negative-zero addition lost its sign");
    Require(Bits(Math::Store(Math::Subtract(Math::Load(-0.0f),Math::Load(0.0f))))==0x80000000u,
        "Negative-zero subtraction lost its sign");
    Require(Bits(Math::Store(Math::Subtract(Math::Load(-0.0f),Math::Load(-0.0f))))==0u,
        "Exact cancellation must produce positive zero under nearest-even");
    for(std::int32_t integer:{minimum,minimum+1,-16777217,-1,0,1,16777217,maximum-1,maximum}) {
        Require(Math::Truncate(Math::Integer(integer))==integer,"Exact INT conversion lost signed32 information");
        Require(Math::Compare(Math::Integer(integer),Math::Integer(integer))==0,"INT compares unequal to itself");
    }
    Require(Math::Compare(Math::Integer(16777217),Math::Load(16777216.0f))>0,
        "Integer was prematurely narrowed to Float");
    Require(Math::Compare(Math::Integer(minimum),Math::Integer(maximum))<0,"Signed INT comparison changed");
    const Math::Real big{1u,66,false};
    Require(Math::Compare(Math::Add(big,Math::Integer(1)),big)==0,"PC64 addition failed a small-operand tie");
    Require(Math::Compare(Math::Subtract(big,Math::Integer(1)),big)==0,"PC64 subtraction failed power-of-two tie");
    const auto tie=Math::Add(Math::Load(1.0f),{1u,-24,false});
    Require(Bits(Math::Store(tie))==0x3f800000u&&Math::Compare(tie,Math::Load(1.0f))>0,
        "Float FST incorrectly replaced the retained PC64 value");
    Require(Bits(Math::Store(Math::Add(Math::Load(std::bit_cast<float>(0x3f800001u)),{1u,-24,false})))==0x3f800002u,
        "Odd Float tie did not round to even");
    Require(Bits(Math::Store({1u,-150,false}))==0u,"Least-subnormal half tie did not round to zero");
    Require(Bits(Math::Store({1u,-150,true}))==0x80000000u,"Negative underflow lost its zero sign");
    Require(Bits(Math::Store({3u,-151,false}))==1u,"Above-half subnormal did not round upward");
    Require(Bits(Math::Store({1u,-10000,true}))==0x80000000u,"Tiny finite dyadic did not store signed zero");
    Require(Math::Truncate(Math::Load(2.75f))==2&&Math::Truncate(Math::Load(-2.75f))==-2,
        "_ftol domain did not truncate toward zero");
    Require(Math::Truncate(Math::Add(Math::Integer(maximum),Math::Load(0.75f)))==maximum,
        "MAX plus fractional tail must retain the supported truncated result");
    Require(Math::Truncate(Math::Subtract(Math::Integer(minimum),Math::Load(0.75f)))==minimum,
        "MIN minus fractional tail must retain the supported truncated result");
    Require(Math::Truncate(Math::Subtract(Math::Integer(0),Math::Load(0.75f)))==0,
        "Negative fractional truncation rounded away from zero");
    // Retained comparison, not the rounded Float store, drives the fill clamp.
    const auto fill=Math::Subtract(Math::Load(1.0f),Math::Add(Math::Load(0.5f),
        Math::Add(Math::Load(0.5f),{1u,-65,false})));
    Require(Math::Compare(fill,Math::Integer(0))==0,"PC64 half-ULP retained tie changed");
    // Integer center division is a second, signed truncation after _ftol.
    Require(Math::Truncate(Math::Subtract(Math::Integer(3),Math::Load(8.75f)))/2==-2,
        "Negative odd center gap did not truncate twice toward zero");
}
void Safety() {
    for(float invalid:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
        std::bit_cast<float>(std::uint32_t{0x7fc12345u})})
        Refuse([&]{static_cast<void>(Math::Load(invalid));},"Nonfinite active Float passed");
    Refuse([]{static_cast<void>(Math::Store({1u,128,false}));},"Float overflow passed");
    Refuse([]{static_cast<void>(Math::Store({0x1ffffffu,103,false}));},"Rounded Float overflow passed");
    Refuse([]{static_cast<void>(Math::Truncate({1u,31,false}));},"Positive signed32 overflow passed");
    Refuse([]{static_cast<void>(Math::Truncate({0x80000001u,0,true}));},"Negative signed32 overflow passed");
    Refuse([]{static_cast<void>(Math::Truncate({1u,10000,true}));},"Huge integer conversion passed");
    const std::array<Math::Real,3u> malformed{{{Wide{1u}<<64u,0,false},
        {1u,std::numeric_limits<int>::min(),false},{1u,std::numeric_limits<int>::max(),false}}};
    for(const auto value:malformed) {
        Refuse([&]{static_cast<void>(Math::Add(value,Math::Integer(0)));},"Malformed add operand passed");
        Refuse([&]{static_cast<void>(Math::Subtract(Math::Integer(0),value));},"Malformed subtract operand passed");
        Refuse([&]{static_cast<void>(Math::Compare(value,Math::Integer(0)));},"Malformed compare operand passed");
        Refuse([&]{static_cast<void>(Math::Store(value));},"Malformed store operand passed");
        Refuse([&]{static_cast<void>(Math::Truncate(value));},"Malformed truncation operand passed");
    }
}

struct ChainInput {
    float width{},height{},hMargin{},vMargin{},minor{},major{},childMain{},childCross{},
        cursorX{},cursorY{},childWidth{},childHeight{};
    std::int32_t running{},cross{};
};
struct ChainResult {
    std::array<float,16u> words{};
    std::array<std::int32_t,4u> integers{};
    std::array<bool,2u> comparisons{};
};
constexpr std::array<std::int32_t,3u> rowCross{16777217,3,-7};
constexpr std::array<std::int32_t,3u> rowExtent{16777217,16777219,-9};
ChainResult SourceChains(const ChainInput& in) {
    ChainResult out;
    const auto hMargin=Math::Load(in.hMargin),vMargin=Math::Load(in.vMargin);
    const auto fill=Math::Subtract(Math::Load(in.width),Math::Add(hMargin,hMargin));
    out.words[0]=Math::Store(fill);
    out.comparisons[0]=Math::Compare(fill,Math::Integer(0))<0;
    if(out.comparisons[0])out.words[0]=0.0f;
    out.words[1]=Math::Store(Math::Subtract(Math::Load(in.height),Math::Add(vMargin,vMargin)));
    out.comparisons[1]=Math::Compare(Math::Add(Math::Integer(in.running),Math::Load(in.childMain)),
        Math::Load(out.words[1]))>0; // Equality does not wrap; spacing is absent here.
    out.integers[0]=Math::Truncate(Math::Add(Math::Add(Math::Integer(in.running),Math::Load(in.minor)),
        Math::Load(in.childMain)));
    out.integers[1]=Math::Truncate(Math::Subtract(Math::Integer(in.running),Math::Load(in.minor)));
    out.integers[2]=Math::Compare(Math::Integer(in.cross),Math::Load(in.childCross))<0 ?
        Math::Truncate(Math::Load(in.childCross)) : in.cross;
    out.integers[3]=Math::Truncate(Math::Subtract(Math::Integer(in.cross),Math::Load(in.childWidth)))/2;
    out.words[2]=Math::Store(Math::Subtract(Math::Load(in.height),
        Math::Add(Math::Load(in.cursorY),Math::Load(in.childHeight))));
    out.words[3]=Math::Store(Math::Subtract(Math::Load(in.width),
        Math::Add(Math::Integer(in.cross),Math::Load(in.cursorX))));
    out.words[4]=Math::Store(Math::Add(Math::Load(out.words[3]),Math::Integer(out.integers[3])));
    out.words[5]=Math::Store(Math::Add(Math::Load(out.words[3]),
        Math::Subtract(Math::Integer(in.cross),Math::Load(in.childWidth))));
    out.words[6]=Math::Store(Math::Integer(in.cross));
    out.words[7]=Math::Store(Math::Add(Math::Add(Math::Load(in.minor),Math::Load(in.childMain)),
        Math::Load(in.cursorY)));
    out.words[8]=Math::Store(Math::Add(Math::Add(Math::Integer(in.cross),Math::Load(in.major)),
        Math::Load(in.cursorX)));
    float verticalWidth=0.0f,horizontalWidth=0.0f;
    auto verticalHeight=Math::Integer(0),horizontalHeight=Math::Integer(0);
    for(std::size_t i=0u;i<rowCross.size();++i) {
        verticalWidth=Math::Store(Math::Add(Math::Load(verticalWidth),Math::Integer(rowCross[i])));
        if(i)verticalWidth=Math::Store(Math::Add(Math::Load(verticalWidth),Math::Load(in.major)));
        const float spilledExtent=Math::Store(Math::Integer(rowExtent[i]));
        if(Math::Compare(Math::Load(spilledExtent),verticalHeight)>0)verticalHeight=Math::Load(spilledExtent);
        if(Math::Compare(Math::Integer(rowExtent[i]),Math::Load(horizontalWidth))>0)
            horizontalWidth=Math::Store(Math::Integer(rowExtent[i]));
        horizontalHeight=Math::Add(horizontalHeight,Math::Integer(rowCross[i]));
        if(i)horizontalHeight=Math::Add(horizontalHeight,Math::Load(in.major));
    }
    out.words[9]=verticalWidth;
    out.words[10]=Math::Store(verticalHeight);
    out.words[11]=horizontalWidth;
    out.words[12]=Math::Store(horizontalHeight);
    out.words[13]=Math::Store(Math::Add(Math::Add(hMargin,hMargin),Math::Load(verticalWidth)));
    out.words[14]=Math::Store(Math::Add(Math::Add(vMargin,vMargin),horizontalHeight));
    out.words[15]=Math::Store(Math::Subtract(Math::Load(in.width),
        Math::Add(Math::Load(in.cursorX),Math::Load(in.childWidth))));
    return out;
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Hardware {
using Real=long double;
struct Precision {
    unsigned short original{};
    Precision() {
        static_assert(std::numeric_limits<Real>::digits==64,"Independent oracle requires x87 PC64");
        asm volatile("fnstcw %0":"=m"(original));
        const auto mode=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision(){asm volatile("fldcw %0"::"m"(original));}
};
Real Load(float value) {
    if(!std::isfinite(value))throw std::runtime_error("Independent nonfinite Float");
    Real result;asm volatile("flds %1; fstpt %0":"=m"(result):"m"(value):"st");return result;
}
Real Integer(std::int32_t value) {
    Real result;asm volatile("fildl %1; fstpt %0":"=m"(result):"m"(value):"st");return result;
}
Real Add(Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;}
Real Subtract(Real a,Real b){volatile Real x=a,y=b,z=x-y;return z;}
int Compare(Real a,Real b){return a<b?-1:a>b?1:0;}
float Store(Real value) {
    float result;asm volatile("fldt %1; fstps %0":"=m"(result):"m"(value):"st");
    if(!std::isfinite(result))throw std::runtime_error("Independent Float overflow");
    return result;
}
std::int32_t Truncate(Real value) {
    // These exact integer thresholds describe the supported RESULT domain;
    // fractional tails beyond INT_MAX/INT_MIN can still truncate in range.
    if(!std::isfinite(value)||value>=2147483648.0L||value<=-2147483649.0L)
        throw std::runtime_error("Independent signed32 truncation overflow");
    unsigned short original;asm volatile("fnstcw %0":"=m"(original));
    const auto chop=static_cast<unsigned short>((original&~0x0c00u)|0x0c00u);
    std::int64_t integer;
    asm volatile("fldcw %2; fldt %3; fistpq %0; fldcw %1":"=m"(integer):
        "m"(original),"m"(chop),"m"(value):"st");
    return static_cast<std::int32_t>(integer);
}
Real Representation(Math::Real value) {
    Require((value.significand>>64u)==0u,"Production arithmetic exceeded PC64 precision");
    const auto magnitude=std::ldexp(static_cast<Real>(static_cast<std::uint64_t>(value.significand)),value.exponent);
    return value.negative?-magnitude:magnitude;
}
bool SameReal(Math::Real actual,Real expected) {
    const auto represented=Representation(actual);
    return represented==expected&&(represented!=0||std::signbit(represented)==std::signbit(expected));
}
ChainResult Chains(const ChainInput& in) {
    // Spell the audited source chains separately; no production dyadic math or
    // shared generic arithmetic algorithm supplies these expected values.
    ChainResult out;
    const auto hm=Load(in.hMargin),vm=Load(in.vMargin);
    const auto fill=Subtract(Load(in.width),Add(hm,hm));
    out.words[0]=Store(fill);out.comparisons[0]=fill<0;
    if(fill<0)out.words[0]=0.0f;
    out.words[1]=Store(Subtract(Load(in.height),Add(vm,vm)));
    out.comparisons[1]=Add(Integer(in.running),Load(in.childMain))>Load(out.words[1]);
    out.integers[0]=Truncate(Add(Add(Integer(in.running),Load(in.minor)),Load(in.childMain)));
    out.integers[1]=Truncate(Subtract(Integer(in.running),Load(in.minor)));
    out.integers[2]=Integer(in.cross)<Load(in.childCross)?Truncate(Load(in.childCross)):in.cross;
    out.integers[3]=Truncate(Subtract(Integer(in.cross),Load(in.childWidth)))/2;
    out.words[2]=Store(Subtract(Load(in.height),Add(Load(in.cursorY),Load(in.childHeight))));
    out.words[3]=Store(Subtract(Load(in.width),Add(Integer(in.cross),Load(in.cursorX))));
    out.words[4]=Store(Add(Load(out.words[3]),Integer(out.integers[3])));
    out.words[5]=Store(Add(Load(out.words[3]),Subtract(Integer(in.cross),Load(in.childWidth))));
    out.words[6]=Store(Integer(in.cross));
    out.words[7]=Store(Add(Add(Load(in.minor),Load(in.childMain)),Load(in.cursorY)));
    out.words[8]=Store(Add(Add(Integer(in.cross),Load(in.major)),Load(in.cursorX)));
    float vw=0.0f,hw=0.0f;Real vh=0.0L,hh=0.0L;
    for(std::size_t i=0u;i<rowCross.size();++i) {
        vw=Store(Add(Load(vw),Integer(rowCross[i])));
        if(i)vw=Store(Add(Load(vw),Load(in.major)));
        const float extent=Store(Integer(rowExtent[i]));
        if(Load(extent)>vh)vh=Load(extent);
        if(Integer(rowExtent[i])>Load(hw))hw=Store(Integer(rowExtent[i]));
        hh=Add(hh,Integer(rowCross[i]));
        if(i)hh=Add(hh,Load(in.major));
    }
    out.words[9]=vw;out.words[10]=Store(vh);out.words[11]=hw;out.words[12]=Store(hh);
    out.words[13]=Store(Add(Add(hm,hm),Load(vw)));
    out.words[14]=Store(Add(Add(vm,vm),hh));
    out.words[15]=Store(Subtract(Load(in.width),Add(Load(in.cursorX),Load(in.childWidth))));
    return out;
}
} // namespace Hardware

template<class A,class B,class Equal>void Compare(A actualOperation,B oracleOperation,Equal equal,const char* why) {
    using Actual=decltype(actualOperation());using Expected=decltype(oracleOperation());
    bool actualRefused{},expectedRefused{};Actual actual{};Expected expected{};
    try{actual=actualOperation();}catch(const std::runtime_error&){actualRefused=true;}
    try{expected=oracleOperation();}catch(const std::runtime_error&){expectedRefused=true;}
    Require(actualRefused==expectedRefused,"Tile arithmetic refusal differs from independent x87");
    if(!actualRefused)Require(equal(actual,expected),why);else ++refusals;
    ++hardwareCases;
}
void CheckValue(Math::Real actual,Hardware::Real expected) {
    Require(Hardware::SameReal(actual,expected),"Retained Tile value differs from independent x87");
    Compare([&]{return Math::Store(actual);},[&]{return Hardware::Store(expected);},
        [](float a,float b){return Bits(a)==Bits(b);},"Tile Float store differs from independent x87");
    Compare([&]{return Math::Truncate(actual);},[&]{return Hardware::Truncate(expected);},
        [](std::int32_t a,std::int32_t b){return a==b;},"Tile _ftol result differs from independent x87");
}
void Pair(float a,float b,std::int32_t integer) {
    const auto x=Math::Load(a),y=Math::Load(b),n=Math::Integer(integer);
    const auto hx=Hardware::Load(a),hy=Hardware::Load(b),hn=Hardware::Integer(integer);
    CheckValue(x,hx);CheckValue(y,hy);CheckValue(n,hn);
    CheckValue(Math::Add(x,y),Hardware::Add(hx,hy));
    CheckValue(Math::Subtract(x,y),Hardware::Subtract(hx,hy));
    CheckValue(Math::Add(n,x),Hardware::Add(hn,hx));
    CheckValue(Math::Subtract(n,y),Hardware::Subtract(hn,hy));
    CheckValue(Math::Add(Math::Add(n,x),y),Hardware::Add(Hardware::Add(hn,hx),hy));
    Require(Math::Compare(x,y)==Hardware::Compare(hx,hy),"Float-PC64 comparison differs from independent x87");
    Require(Math::Compare(n,x)==Hardware::Compare(hn,hx),"INT/Float comparison differs from independent x87");
    ++hardwareCases;
}
void Chain(const ChainInput& in) {
    Compare([&]{return SourceChains(in);},[&]{return Hardware::Chains(in);},
        [](const ChainResult& a,const ChainResult& b) {
            for(std::size_t i=0u;i<a.words.size();++i)if(Bits(a.words[i])!=Bits(b.words[i]))return false;
            return a.integers==b.integers&&a.comparisons==b.comparisons;
        },"Tile margin/row/placement/output chain differs from independent x87");
}
std::uint64_t Next(std::uint64_t& state) {
    state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;
}
float Broad(std::uint64_t& state) {
    const auto bits=static_cast<std::uint32_t>(Next(state));
    return std::bit_cast<float>((bits&0x807fffffu)|(((bits>>23u)%255u)<<23u));
}
float Bounded(std::uint64_t& state) {
    return static_cast<float>(static_cast<int>(Next(state)%65537u)-32768)/128.0f;
}
void Independent() {
    Hardware::Precision precision;
    const auto minimum=std::numeric_limits<std::int32_t>::min();
    const auto maximum=std::numeric_limits<std::int32_t>::max();
    for(float a:{-0.0f,0.0f})for(float b:{-0.0f,0.0f})Pair(a,b,0);
    const std::array<float,14u> extrema{-std::numeric_limits<float>::max(),
        -2147483648.0f,-16777216.0f,-1.0f,-std::numeric_limits<float>::min(),
        -std::numeric_limits<float>::denorm_min(),-0.0f,0.0f,std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(),1.0f,16777216.0f,2147483648.0f,std::numeric_limits<float>::max()};
    for(float a:extrema)for(float b:extrema)for(std::int32_t integer:{minimum,-1,0,1,maximum})Pair(a,b,integer);
    for(int exponent:{-151,-150,-149,-100,-65,-64,-63,-25,-24,-1,0,30,31,63,64,65,66,104,127})
        for(std::uint64_t sig:{1ull,3ull,0x8000000000000000ull,0xffffffffffffffffull})
            for(bool negative:{false,true}) {
                const Math::Real value{sig,exponent,negative};
                const auto expected=Hardware::Representation(value);
                CheckValue(value,expected);
                for(std::int32_t integer:{-1,0,1}) {
                    CheckValue(Math::Add(value,Math::Integer(integer)),Hardware::Add(expected,Hardware::Integer(integer)));
                    CheckValue(Math::Subtract(value,Math::Integer(integer)),Hardware::Subtract(expected,Hardware::Integer(integer)));
                }
            }
    for(std::int32_t integer:{minimum,minimum+1,-16777217,-3,-1,0,1,3,16777217,maximum-1,maximum})
        for(float tail:{-1.0f,-0.99999994f,-0.75f,-0.5f,-0.0f,0.0f,0.5f,0.75f,0.99999994f,1.0f})
            CheckValue(Math::Add(Math::Integer(integer),Math::Load(tail)),
                Hardware::Add(Hardware::Integer(integer),Hardware::Load(tail)));
    for(float zero:{-0.0f,0.0f})Chain({zero,zero,zero,zero,zero,zero,zero,zero,zero,zero,zero,zero,0,0});
    Chain({320,240,3.25f,-2.75f,1.75f,-0.75f,12.875f,5.25f,-2,3,8.75f,12.875f,11,3});
    Chain({1,1,0.5f,0.5f,-0.0f,0.0f,1,1,0,0,1,1,0,1});
    Chain({16777216,16777216,0.25f,-0.25f,0.75f,-0.75f,0.5f,16777216,1,1,0.5f,0.5f,16777217,16777217});
    std::uint64_t state=0x67132b8da52f09c3ull;
    for(unsigned i=0u;i<12000u;++i) {
        const bool broad=i<4000u;
        const float a=broad?Broad(state):Bounded(state),b=broad?Broad(state):Bounded(state);
        const auto integer=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(state)));
        Pair(a,b,integer);
        ChainInput in;
        in.width=Bounded(state);in.height=Bounded(state);in.hMargin=Bounded(state);in.vMargin=Bounded(state);
        in.minor=Bounded(state);in.major=Bounded(state);in.childMain=Bounded(state);in.childCross=Bounded(state);
        in.cursorX=Bounded(state);in.cursorY=Bounded(state);in.childWidth=Bounded(state);in.childHeight=Bounded(state);
        in.running=static_cast<std::int32_t>(static_cast<int>(Next(state)%65537u)-32768);
        in.cross=static_cast<std::int32_t>(static_cast<int>(Next(state)%65537u)-32768);
        Chain(in);
    }
}
#else
void Independent() {
    std::cout<<"SKIP independent hardware x87 PC64 oracle on this platform; portable Tile controls still run\n";
}
#endif
} // namespace

int main() {
    try {
        Deterministic();Safety();
        const auto chain=SourceChains({320,240,3.25f,-2.75f,1.75f,-0.75f,12.875f,5.25f,-2,3,8.75f,12.875f,11,3});
        Require(chain.words[0]==313.5f&&chain.integers==std::array<std::int32_t,4u>{25,9,5,-2},
            "Portable Tile literal margin/row/center chain changed");
        portableChecks=checks;Independent();
        std::cout<<"PASS original Tile arithmetic: "<<checks<<" checks / "<<portableChecks
            <<" portable checks / "<<hardwareCases<<" independent hardware x87 cases / "<<refusals
            <<" explicit refusals; arithmetic only, not Tile layout/callbacks, startup, live HUD or headset acceptance\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL original Tile arithmetic: "<<error.what()<<'\n';return 1;
    }
}
