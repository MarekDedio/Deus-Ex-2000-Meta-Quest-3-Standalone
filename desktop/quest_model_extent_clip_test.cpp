#include "quest_model_extent_clip.h"

#include <iostream>
#include <limits>

namespace {
using Vec=PortableModelVec3;
using Plane=QuestVr::OriginalExtentPlane;
using State=QuestVr::OriginalExtentClipState;
std::size_t checks{},refusals{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
void Same(const State& a,const State& b) {
    Require(Bits(a.enter)==Bits(b.enter)&&Bits(a.exit)==Bits(b.exit)&&
        Bits(a.normal.x)==Bits(b.normal.x)&&Bits(a.normal.y)==Bits(b.normal.y)&&Bits(a.normal.z)==Bits(b.normal.z),
        "Original extent clip interval/normal Float bits differ");
}
template<class F> void Refuse(F run) {
    bool refused{};try {run();} catch(const std::exception&) {refused=true;}
    Require(refused,"Invalid original extent clip silently succeeded");++refusals;
}
void Boundaries() {
    const auto box=QuestVr::BuildOriginalExtentBoundsPlanes({0,0,0},{1,1,1});
    Require(box[0u].z==-1&&box[1u].z==1&&box[2u].x==-1&&box[3u].x==1&&box[4u].y==-1&&box[5u].y==1,
        "Retail bounding planes lost -Z,+Z,-X,+X,-Y,+Y order");
    Require(Bits(box[0u].w)==Bits(.1f)&&Bits(box[1u].w)==Bits(1.1f)&&Bits(box[2u].w)==Bits(.1f)&&
        Bits(box[3u].w)==Bits(.9f)&&Bits(box[4u].w)==Bits(.1f)&&Bits(box[5u].w)==Bits(.9f),
        "Retail positive-Z expansion was replaced by uniform bounding-box shaving");
    const auto doubleMargin=QuestVr::BuildOriginalExtentBoundsPlanes({.1f,.1f,.1f},{.1f,.1f,.1f});
    Require(Bits(doubleMargin[0u].w)==Bits(static_cast<float>(.1-static_cast<double>(.1f)))&&doubleMargin[0u].w!=0,
        "Retail Double0.1 margin was replaced by Float0.1");
    Refuse([]{QuestVr::BuildOriginalExtentBoundsPlanes({1,0,0},{0,1,1});});
    State state;
    Require(QuestVr::ClipOriginalExtentPlane(state,{1,0,0,0},{10,0,0},{-10,0,0},{1,1,1}),"Entering axis plane refused");
    Same(state,{.45f,2,{1,0,0}});
    state={};Require(QuestVr::ClipOriginalExtentPlane(state,{1,0,0,0},{.5f,0,0},{-1,0,0},{1,1,1}),"Contact-inside plane refused");
    Same(state,{0,2,{1,0,0}});
    state={};Require(QuestVr::ClipOriginalExtentPlane(state,{1,0,0,0},{-.5f,0,0},{2,0,0},{1,1,1}),"Leaving axis plane refused");
    Same(state,{-1,.6f,{}});
    state={};Require(!QuestVr::ClipOriginalExtentPlane(state,{1,0,0,0},{2,0,0},{2,10,0},{1,1,1}),"Parallel outside plane accepted");
    Same(state,{});
    state={};Require(QuestVr::ClipOriginalExtentPlane(state,{1,0,0,0},{1,0,0},{1,10,0},{1,1,1}),"Parallel touching plane was rejected by unused 0/0");
    Same(state,{});
    state={.5f,2,{0,1,0}};Require(QuestVr::ClipOriginalExtentPlane(state,{1,0,0,0},{10,0,0},{-10,0,0},{0,0,0}),"Equal-enter clip refused");
    Same(state,{.5f,2,{0,1,0}}); // Strict comparison must not replace equal normal.
    state={-1,.45f,{}};Require(!QuestVr::ClipOriginalExtentPlane(state,{1,0,0,0},{10,0,0},{-10,0,0},{1,1,1}),"Collapsed stored interval accepted");
    Same(state,{.45f,.45f,{1,0,0}}); // Writes precede final strict rejection.
    Require(Bits(QuestVr::ModelExtentClipDetail::Pushout({0,0,0},{-1,-1,-1,0}))==0x80000000u,
        "Retail conditional absolute value erased negative zero");
    state={};Require(QuestVr::ClipOriginalExtentPlane(state,{1,-0.0f,-0.0f,0},{10,0,0},{-10,0,0},{1,1,1}),"Signed-zero plane normal refused");
    Same(state,{.45f,2,{1,-0.0f,-0.0f}});
    for(const auto invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        Refuse([&]{State s;QuestVr::ClipOriginalExtentPlane(s,{invalid,0,0,0},{},{},{1,1,1});});
        Refuse([&]{State s;QuestVr::ClipOriginalExtentPlane(s,{1,0,0,0},{invalid,0,0},{},{1,1,1});});
        Refuse([&]{State s;QuestVr::ClipOriginalExtentPlane(s,{1,0,0,0},{},{},{invalid,1,1});});
    }
    Refuse([]{State s;QuestVr::ClipOriginalExtentPlane(s,{1,0,0,0},{},{},{-1,1,1});});
    Refuse([]{State s;const auto m=std::numeric_limits<float>::max();QuestVr::ClipOriginalExtentPlane(s,{m,m,m,0},{},{},{m,m,m});});
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct Precision {
    unsigned short saved{};
    Precision() {static_assert(std::numeric_limits<long double>::digits==64);asm volatile("fnstcw %0":"=m"(saved));
        const auto mode=static_cast<unsigned short>((saved&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
    ~Precision() {asm volatile("fldcw %0"::"m"(saved));}
};
long double Add(long double a,long double b) {volatile long double x=a,y=b,z=x+y;return z;}
long double Sub(long double a,long double b) {volatile long double x=a,y=b,z=x-y;return z;}
long double Mul(long double a,long double b) {volatile long double x=a,y=b,z=x*y;return z;}
long double Div(long double a,long double b) {volatile long double x=a,y=b,z=x/y;return z;}
long double Abs(long double a) {return a<0?-a:a;}
float Store(long double a) {volatile float f=static_cast<float>(a);if(!std::isfinite(f)) throw std::runtime_error("Independent x87 Float overflow");return f;}
bool Oracle(State& s,const Plane& p,const Vec& start,const Vec& end,const Vec& extent) {
    const auto e=Store(Add(Add(Abs(Mul(extent.z,p.z)),Abs(Mul(extent.y,p.y))),Abs(Mul(extent.x,p.x))));
    const auto distance=[&](const Vec& v) {return Store(Sub(Add(Add(Mul(v.z,p.z),Mul(v.y,p.y)),Mul(v.x,p.x)),p.w));};
    const auto from=distance(start),to=distance(end);auto numerator=Sub(from,e);
    if(from>to&&numerator>=-static_cast<long double>(e)&&numerator<0) numerator=0;
    const auto denominator=Store(Sub(from,to));
    // Independent x87 division also executes on parallel/zero, as retail does;
    // the unused quotient is never stored/validated in its parallel branch.
    const auto t=Div(numerator,denominator);
    if(denominator<std::bit_cast<double>(std::uint64_t{0xbee4f8b588e368f1ull})) {if(t<s.exit) s.exit=Store(t);}
    else if(denominator>std::bit_cast<double>(std::uint64_t{0x3ee4f8b588e368f1ull})) {if(t>s.enter) {s.enter=Store(t);s.normal={p.x,p.y,p.z};}}
    else if(from>e&&to>e) return false;
    return s.enter<s.exit;
}
std::uint64_t Next(std::uint64_t& s) {s^=s<<13u;s^=s>>7u;s^=s<<17u;return s;}
float Number(std::uint64_t& s,bool broad) {
    const auto raw=static_cast<std::uint32_t>(Next(s));
    return broad?std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u)):
        static_cast<float>(static_cast<int>(raw%65537u)-32768)/1024.0f;
}
void Differential() {
    Precision mode;std::uint64_t seed=0x98128dbce79f256aull;
    for(unsigned i=0;i<80'000u;++i) {
        const bool broad=i<20'000u;const auto n=[&]{return Number(seed,broad);};
        const Plane p{n(),n(),n(),n()};const Vec start{n(),n(),n()},end{n(),n(),n()},extent{std::fabs(n()),std::fabs(n()),std::fabs(n())};
        State actual{n(),n(),{n(),n(),n()}},expected=actual;bool failed{},oracleFailed{},accepted{},oracleAccepted{};
        try {accepted=QuestVr::ClipOriginalExtentPlane(actual,p,start,end,extent);} catch(const std::exception&) {failed=true;}
        try {oracleAccepted=Oracle(expected,p,start,end,extent);} catch(const std::exception&) {oracleFailed=true;}
        Require(failed==oracleFailed,"Original extent clip refusal differs from independent x87");
        if(!failed) {Require(accepted==oracleAccepted,"Original extent clip acceptance differs from independent x87");Same(actual,expected);} else ++refusals;
        if(i<20'000u) {
            const Vec minimum{std::min(start.x,end.x),std::min(start.y,end.y),std::min(start.z,end.z)},
                maximum{std::max(start.x,end.x),std::max(start.y,end.y),std::max(start.z,end.z)};
            const auto bounds=QuestVr::BuildOriginalExtentBoundsPlanes(minimum,maximum);
            const auto margin=std::bit_cast<double>(std::uint64_t{0x3fb999999999999aull});
            const std::array<float,6u> expectedW{Store(Sub(margin,minimum.z)),Store(Add(maximum.z,margin)),
                Store(Sub(margin,minimum.x)),Store(Sub(maximum.x,margin)),Store(Sub(margin,minimum.y)),Store(Sub(maximum.y,margin))};
            for(std::size_t j=0;j<bounds.size();++j) Require(Bits(bounds[j].w)==Bits(expectedW[j]),"Retail hull plane W differs from independent x87");
        }
    }
    for(const float value:{0.0f,-0.0f,1e-5f,std::nextafter(1e-5f,0.0f),std::nextafter(1e-5f,1.0f),-1e-5f,
            std::nextafter(-1e-5f,0.0f),std::nextafter(-1e-5f,-1.0f)}) {
        State actual,expected;const Plane p{1,0,0,0};const Vec start{value,0,0},end{};
        const auto result=QuestVr::ClipOriginalExtentPlane(actual,p,start,end,{});
        Require(result==Oracle(expected,p,start,end,{}),"Double parallel threshold boundary differs");Same(actual,expected);
    }
}
#else
void Differential() {std::cout<<"SKIP independent x87 oracle on non-x86 compiler; deterministic clip controls still run.\n";}
#endif
}
int main() {
    try {Boundaries();Differential();std::cout<<"PASS original extent-plane clip "<<checks<<" checks, "<<refusals
        <<" safety refusals; retained PC64/stored Float interval. This is not a complete Model sweep or Move266.\n";return 0;}
    catch(const std::exception& e) {std::cerr<<"Original extent-plane clip failed: "<<e.what()<<'\n';return 1;}
}
