#include "quest_actor_move_geometry.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Vec=PortableModelVec3;
using Geometry=QuestVr::OriginalActorMoveGeometry;
using Backoff=QuestVr::OriginalActorMoveBackoff;
std::size_t checks{},refusals{},preparations{},backoffs{},rootSpillWitnesses{},distanceSpillWitnesses{},retainedGateWitnesses{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
bool Same(float a,float b){return Bits(a)==Bits(b);}
bool Same(Vec a,Vec b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
float& Channel(Vec& value,unsigned channel){return channel==0u?value.x:channel==1u?value.y:value.z;}
template<class F>void Refuse(F run){bool failed{};try{run();}catch(const std::exception&){failed=true;}
    Require(failed,"Active MoveActor numeric refusal unexpectedly succeeded");++refusals;}

void Controls(){
    const float nan=std::bit_cast<float>(0x7fc12345u),inf=std::numeric_limits<float>::infinity();
    const float tiny=std::bit_cast<float>(1u),below=1.0e-4f,above=std::nextafter(below,1.0f);
    for(const float sign:{-1.0f,1.0f})for(unsigned channel=0u;channel<3u;++channel){
        Vec a{};Channel(a,channel)=sign*below;
        Require(QuestVr::IsOriginalActorMoveSmallDelta(a),"Strict Double threshold rejected its lower Float neighbor");
        Channel(a,channel)=sign*above;
        Require(!QuestVr::IsOriginalActorMoveSmallDelta(a),"Strict Double threshold admitted its upper Float neighbor");
    }
    Require(!QuestVr::IsOriginalActorMoveSmallDelta({1,nan,inf}),"Small test read inactive Y/Z after an outside X");
    Require(!QuestVr::IsOriginalActorMoveSmallDelta({0,1,nan}),"Small test read inactive Z after an outside Y");
    Refuse([&]{QuestVr::IsOriginalActorMoveSmallDelta({nan,1,1});});
    Refuse([&]{QuestVr::IsOriginalActorMoveSmallDelta({0,inf,1});});
    Refuse([&]{QuestVr::IsOriginalActorMoveSmallDelta({0,0,-inf});});
    for(const auto delta:std::array<Vec,5u>{{{0,-0.0f,0},{-0.0f,0,-0.0f},{tiny,-tiny,0},{below,below,below},{3,4,0}}}){
        const auto result=QuestVr::PrepareOriginalActorMoveGeometry(delta);
        if(result.small)Require(Same(result.size,0.0f)&&Same(result.direction,delta),"Small geometry did not preserve raw direction words/+0 size");
        else Require(Same(result.size,5.0f)&&Same(result.direction,{.6f,.8f,0}),"3-4-5 stored inverse/direction differs");
    }
    Geometry poison;poison.size=nan;poison.direction={nan,inf,-inf};poison.extended={inf,nan,-inf};
    const Vec raw{nan,inf,-inf};
    for(const float time:{1.0f,2.0f,inf}){
        const auto result=QuestVr::BackoffOriginalActorMove(raw,poison,time,0);
        Require(Same(result.delta,raw)&&Same(result.time,time),"Clear Time read inactive geometry or changed raw Delta/Time");
    }
    for(const std::int32_t noFail:{1,-1,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()})
        for(const float time:{nan,-inf,-1.0f,-0.0f,.5f}){
            const auto result=QuestVr::BackoffOriginalActorMove(raw,poison,time,noFail);
            Require(Same(result.delta,raw)&&Same(result.time,time),"Nonzero bNoFail read inactive packet or clamped a raw Time");
        }
    Geometry zeroGate;zeroGate.size=0;zeroGate.direction={nan,inf,nan};zeroGate.extended={nan,inf,-inf};
    const auto zero=QuestVr::BackoffOriginalActorMove(raw,zeroGate,.5f,0);
    Require(Same(zero.delta,Vec{})&&Same(zero.time,0.0f),"Distance<=2 did not zero exact positive words or read inactive direction");
    Refuse([&]{QuestVr::BackoffOriginalActorMove({},poison,.5f,0);});
    auto geometry=QuestVr::PrepareOriginalActorMoveGeometry({10,0,0});
    const auto half=QuestVr::BackoffOriginalActorMove({10,0,0},geometry,.5f,0);
    Require(Same(half.delta,{4,0,0})&&Same(half.time,.4f),"Two-unit backoff of ten units at half Time differs");
    for(const float time:{nan,-inf})Refuse([&]{QuestVr::BackoffOriginalActorMove({},geometry,time,0);});
    for(unsigned channel=0u;channel<3u;++channel){
        Vec bad{3,4,5};Channel(bad,channel)=nan;Refuse([&]{QuestVr::PrepareOriginalActorMoveGeometry(bad);});
        auto badGeometry=geometry;Channel(badGeometry.direction,channel)=nan;
        Refuse([&]{QuestVr::BackoffOriginalActorMove({},badGeometry,.5f,0);});
        badGeometry=geometry;Channel(badGeometry.extended,channel)=nan;
        Refuse([&]{QuestVr::BackoffOriginalActorMove({},badGeometry,.5f,0);});
    }
    Refuse([&]{QuestVr::PrepareOriginalActorMoveGeometry({std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),0});});
    // An overwritten distance scratch is still a real Float store: even a
    // later zero gate does not excuse its overflow in the portable domain.
    geometry.size=std::numeric_limits<float>::max();
    Refuse([&]{QuestVr::BackoffOriginalActorMove({},geometry,-std::numeric_limits<float>::max(),0);});
    // Retained distance is just ABOVE2 although its Float scratch equals2.
    // Poisoned later fields distinguish this branch without inventing a
    // successful geometric packet from inconsistent caller-owned scratch.
    Geometry retained;retained.size=std::ldexp(1.0f,-13);retained.direction={nan,0,0};
    Refuse([&]{QuestVr::BackoffOriginalActorMove({},retained,std::bit_cast<float>(0x3f7ffc01u),0);});
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Oracle {
using Real=long double;
struct Precision {
    unsigned short previous{};
    Precision(){static_assert(std::numeric_limits<Real>::digits==64);asm volatile("fnstcw %0":"=m"(previous));
        const auto mode=static_cast<unsigned short>((previous&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
    ~Precision(){asm volatile("fldcw %0"::"m"(previous));}
};
Real Read(float x){if(!std::isfinite(x))throw std::runtime_error("Oracle active nonfinite");return x;}
Real Add(Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,z=x*y;return z;}
Real Div(Real a,Real b){if(b==0)throw std::runtime_error("Oracle divide zero");volatile Real x=a,y=b,z=x/y;return z;}
float Store(Real x){volatile float y=static_cast<float>(x);if(!std::isfinite(y))throw std::runtime_error("Oracle Float overflow");return y;}
Real Root(double x){Real root;asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(root):"m"(x):"st");return root;}
bool Small(Vec d){const double threshold=std::bit_cast<double>(std::uint64_t{0x3f1a36e2eb1c432dull});
    for(float x:{d.x,d.y,d.z}){auto value=Read(x);if(value<0)value=-value;if(!(value<threshold))return false;}return true;}
struct Prepared {bool small{};float size{},inverse{},wrongInverse{};Vec direction{},extended{};};
Prepared Prepare(Vec d){
    Prepared out;out.small=Small(d);
    if(out.small){out.size=0;out.direction=d;}
    else{
        const auto zz=Mul(Read(d.z),Read(d.z)),yy=Mul(Read(d.y),Read(d.y)),zy=Add(zz,yy);
        const auto total=Add(zy,Mul(Read(d.x),Read(d.x)));
        volatile double stored=static_cast<double>(total);const auto root=Root(stored);
        volatile double unused=static_cast<double>(root);static_cast<void>(unused);
        out.size=Store(root);out.inverse=Store(Div(1.0L,root));out.wrongInverse=Store(Div(1.0L,Read(out.size)));
        const auto z=Mul(Read(d.z),Read(out.inverse)),y=Mul(Read(d.y),Read(out.inverse)),x=Mul(Read(out.inverse),Read(d.x));
        out.direction={Store(x),Store(y),Store(z)};
    }
    const auto doubleZ=Add(Read(out.direction.z),Read(out.direction.z));
    const float doubleY=Store(Add(Read(out.direction.y),Read(out.direction.y))),doubleX=Store(Add(Read(out.direction.x),Read(out.direction.x)));
    out.extended.z=Store(Add(Read(d.z),doubleZ));out.extended.y=Store(Add(Read(d.y),Read(doubleY)));
    out.extended.x=Store(Add(Read(doubleX),Read(d.x)));return out;
}
struct Correction {Vec delta{};float time{};bool retainedGate{},distanceSpill{};};
Correction Correct(Vec requested,const Prepared& g,float time,std::int32_t noFail){
    if((!std::isnan(time)&&time>=1)||noFail)return {requested,time,false,false};
    const auto distance=Mul(Add(2.0L,Read(g.size)),Read(time));const float stored=Store(distance);
    if(distance<=2)return {{},0,false,false};
    const float bz=Store(Mul(Read(g.direction.z),2.0L)),by=Store(Mul(Read(g.direction.y),2.0L)),bx=Store(Mul(Read(g.direction.x),2.0L));
    const float z=Store(Mul(Read(g.extended.z),Read(time))),y=Store(Mul(Read(g.extended.y),Read(time))),x=Store(Mul(Read(time),Read(g.extended.x)));
    Correction out;out.delta.z=Store(Sub(Read(z),Read(bz)));out.delta.y=Store(Sub(Read(y),Read(by)));out.delta.x=Store(Sub(Read(x),Read(bx)));
    out.time=Store(Div(Sub(Read(stored),2.0L),Read(g.size)));out.retainedGate=stored<=2;
    out.distanceSpill=!Same(out.time,Store(Div(Sub(distance,2.0L),Read(g.size))));return out;
}
} // namespace Oracle
std::uint64_t Next(std::uint64_t& state){state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
float Number(std::uint64_t& state,bool broad){const auto raw=static_cast<std::uint32_t>(Next(state));
    return broad?std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u)):
        static_cast<float>(static_cast<int>(raw%65537u)-32768)/1024.0f;}
void Compare(Vec delta,float time){
    Geometry actual;Oracle::Prepared expected;bool failed{},oracleFailed{};
    try{actual=QuestVr::PrepareOriginalActorMoveGeometry(delta);}catch(const std::exception&){failed=true;}
    try{expected=Oracle::Prepare(delta);}catch(const std::exception&){oracleFailed=true;}
    Require(failed==oracleFailed,"MoveActor prequery refusal differs from independent hardware x87");
    if(failed){++refusals;return;}
    Require(actual.small==expected.small&&Same(actual.size,expected.size)&&Same(actual.direction,expected.direction)&&Same(actual.extended,expected.extended),
        "MoveActor prequery packet differs from independent hardware x87");++preparations;
    if(!expected.small&&!Same(expected.inverse,expected.wrongInverse))++rootSpillWitnesses;
    Backoff moved;Oracle::Correction predicted;failed=false;oracleFailed=false;
    try{moved=QuestVr::BackoffOriginalActorMove(delta,actual,time,0);}catch(const std::exception&){failed=true;}
    try{predicted=Oracle::Correct(delta,expected,time,0);}catch(const std::exception&){oracleFailed=true;}
    Require(failed==oracleFailed,"MoveActor correction refusal differs from independent hardware x87");
    if(failed){++refusals;return;}
    Require(Same(moved.delta,predicted.delta)&&Same(moved.time,predicted.time),"MoveActor correction packet differs from independent hardware x87");++backoffs;
    if(predicted.retainedGate)++retainedGateWitnesses;
    if(predicted.distanceSpill)++distanceSpillWitnesses;
}
void Hardware(){
    Oracle::Precision precision;std::uint64_t state=0x6b4c930d725e81a7ull;
    for(unsigned i=0u;i<40'000u;++i){const bool broad=i<15'000u;const Vec d{Number(state,broad),Number(state,broad),Number(state,broad)};
        const float time=i%5u==0u?Number(state,broad):static_cast<float>(Next(state)%200001u)/100000.0f-0.5f;Compare(d,time);}
    const std::array<std::uint32_t,14u> words{0u,0x80000000u,1u,0x80000001u,0x38d1b717u,0x38d1b718u,0x3f000000u,
        0x3f800000u,0xbf800000u,0x40000000u,0x4b800000u,0x00800000u,0x7f7fffffu,0xff7fffffu};
    for(auto x:words)for(auto y:words)for(float time:{-1.0f,-0.0f,.25f,.5f,std::nextafter(1.0f,0.0f),1.0f,2.0f})
        Compare({std::bit_cast<float>(x),std::bit_cast<float>(y),0},time);
    for(unsigned i=0u;i<12'000u;++i){const Vec delta{Number(state,false),Number(state,false),Number(state,false)};
        const auto g=Oracle::Prepare(delta);const float time=static_cast<float>(2.0L/(2.0L+g.size));Compare(delta,time);}
    Require(preparations>35'000u&&backoffs>25'000u&&rootSpillWitnesses>1000u&&distanceSpillWitnesses>1000u&&retainedGateWitnesses>100u,
        "Independent movement geometry cohorts missed finite/retained-root/stored-distance/retained-gate witnesses");
}
#else
void Hardware(){std::cout<<"SKIP independent hardware-x87 MoveActor geometry oracle on non-x86 compiler; deterministic controls still run.\n";}
#endif
} // namespace

int main(){try{Controls();Hardware();std::cout<<"PASS original MoveActor geometry "<<checks<<" checks / "<<refusals
    <<" explicit refusals, "<<preparations<<" independent x87 preparations / "<<backoffs<<" corrections, "<<rootSpillWitnesses
    <<" retained-root / "<<distanceSpillWitnesses<<" stored-distance / "<<retainedGateWitnesses
    <<" retained-gate witnesses. Numeric prerequisite only, not runtime MoveActor or Training crate physics.\n";return 0;}
    catch(const std::exception& error){std::cerr<<"Original MoveActor geometry failed: "<<error.what()<<'\n';return 1;}}
