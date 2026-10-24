#include "quest_pawn_pick_target.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

// Native531 candidate scalar only. Test-owned x87 PC64 instructions below
// independently reproduce the audited instruction/store order; neither an
// original commercial binary nor production dyadic arithmetic is the oracle.
namespace {
using Vec=PortableModelVec3;
using Candidate=QuestVr::OriginalPawnPickTargetCandidate;
using Result=std::optional<Candidate>;
std::size_t checks{},refusals{},hardwareCases{},portableCases{},lazyReads{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
bool Same(const Result& a,const Result& b){return bool(a)==bool(b)&&(!a||
    (Bits(a->aim)==Bits(b->aim)&&Bits(a->distance)==Bits(b->distance)));}
template<class F>void Refuse(F operation,const char* why){bool refused{};try{operation();}catch(const std::exception&){refused=true;}
    Require(refused,why);++refusals;}
Result Actual(Vec candidate,Vec start,Vec direction,float best,std::size_t& reads){
    return QuestVr::EvaluateOriginalPawnPickTargetCandidate(candidate,start,direction,[&]{++reads;return best;});
}
void Deterministic(){
    std::size_t reads{};
    const auto forward=Actual({100,0,0},{},{1,0,0},0.5f,reads);
    Require(Same(forward,Result{Candidate{1.0f,100.0f}})&&reads==1u,
        "Exact forward candidate did not preserve scalar aim/distance or one lazy read");
    ++portableCases;
    const auto rejectBeforeBest=[&](Vec point,Vec start,Vec direction){
        const auto result=QuestVr::EvaluateOriginalPawnPickTargetCandidate(point,start,direction,[&]() -> float {
            ++reads;throw std::runtime_error("Unreached bestAim reader invoked");});
        Require(!result&&reads==1u,"Behind/zero/range candidate eagerly read bestAim");++portableCases;
    };
    rejectBeforeBest({-1,0,0},{},{1,0,0});
    rejectBeforeBest({0,1,0},{},{1,0,0});
    rejectBeforeBest({},{},{1,1,1});
    rejectBeforeBest({-0.0f,0,-0.0f},{0,-0.0f,0},{-1,1,-1});
    rejectBeforeBest({2000,0,0},{},{1,0,0});
    rejectBeforeBest({std::nextafter(2000.0f,3000.0f),0,0},{},{1,0,0});
    // A mandatory Float angle store can round a positive retained dot to +0.
    rejectBeforeBest({std::numeric_limits<float>::denorm_min(),0,0},{},{0.25f,0,0});
    reads=0u;
    Require(!Actual({1,0,0},{},{1,0,0},1.0f,reads)&&reads==1u,"Exact equal bestAim must fail strict >");
    Require(Actual({1,0,0},{},{1,0,0},std::nextafter(1.0f,0.0f),reads).has_value(),"Just-lower bestAim must qualify");
    Require(!Actual({1,0,0},{},{1,0,0},std::nextafter(1.0f,2.0f),reads),"Just-higher bestAim must reject");
    portableCases+=3u;
    // Unspilled x=(1-2^-25) is not Float(1). Early narrowing would erase this
    // positive cancellation dot and return before ever reading bestAim.
    reads=0u;const auto delta=Actual({1,1,0},{std::ldexp(1.0f,-25),0,0},{-1,1,0},-1.0f,reads);
    Require(delta&&delta->aim>0.0f&&reads==1u,"PickTarget prematurely Float-spilled its location difference");++portableCases;
    // (Z+Y)+X=1, whereas (X+Y)+Z=0 in PC64 for these Float terms.
    reads=0u;const auto order=Actual({1,1,1},{},{1,-std::ldexp(1.0f,66),std::ldexp(1.0f,66)},0.0f,reads);
    Require(order&&order->aim>0.5f&&order->aim<0.6f&&reads==1u,"PickTarget changed the audited Z/Y/X dot order");++portableCases;
    // The retained square is just below4,000,000, though a Float spill of that
    // square would round to4,000,000 and wrongly reject the candidate.
    reads=0u;const auto range=Actual({std::nextafter(2000.0f,0.0f),0.625f,0},{},{1,0,0},0.0f,reads);
    Require(range&&range->distance==2000.0f&&reads==1u,"PickTarget narrowed square before strict range comparison");++portableCases;
    // FST candidateAim followed by FCOMP retains the wider quotient. This
    // positive result has storedAim==bestAim but retained quotient>bestAim.
    reads=0u;const float rounded=std::bit_cast<float>(std::uint32_t{0x3f3504f3u});
    const auto retained=Actual({1,1,0},{},{1,0,0},rounded,reads);
    Require(retained&&Bits(retained->aim)==Bits(rounded)&&reads==1u,"PickTarget compared the rounded candidateAim instead of retained quotient");++portableCases;
    // The other rounding side must not turn an exact stored Float tie into >.
    reads=0u;Require(!Actual({1,2,0},{},{1,0,0},std::bit_cast<float>(std::uint32_t{0x3ee4f92eu}),reads)&&reads==1u,
        "PickTarget rounded quotient upward before its strict live comparison");++portableCases;
    reads=0u;const auto tiny=Actual({std::numeric_limits<float>::denorm_min(),0,0},{},{1,0,0},0.0f,reads);
    Require(tiny&&Bits(tiny->aim)==Bits(1.0f)&&Bits(tiny->distance)==1u&&reads==1u,
        "PickTarget lost a nonzero subnormal distance at the Double square/root boundary");++portableCases;
}
void LazyAndSafety(){
    for(const float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()}){
        std::size_t reads{};
        const auto skipped=Actual({-1,0,0},{},{1,0,0},invalid,reads);
        Require(!skipped&&!reads,"Inactive nonfinite bestAim was validated eagerly");++portableCases;
        Refuse([&]{static_cast<void>(Actual({1,0,0},{},{1,0,0},invalid,reads));},"Reached nonfinite bestAim was accepted");
        Require(reads==1u,"Reached nonfinite bestAim must be read exactly once");
        for(unsigned field=0u;field<9u;++field){
            Vec point{1,2,3},start{},direction{1,1,1};
            auto* selected=field<3u?&point:field<6u?&start:&direction;
            const auto channel=field%3u;
            if(channel==0u)selected->x=invalid;else if(channel==1u)selected->y=invalid;else selected->z=invalid;
            reads=0u;Refuse([&]{static_cast<void>(Actual(point,start,direction,0.0f,reads));},"Nonfinite active candidate/Start/Dir channel accepted");
            Require(!reads,"Invalid active numeric data read old bestAim");
        }
    }
    std::size_t reads{};
    Refuse([&]{static_cast<void>(QuestVr::EvaluateOriginalPawnPickTargetCandidate(Vec{1,0,0},Vec{},Vec{1,0,0},[&]() -> float {
        ++reads;throw std::runtime_error("Expected live bestAim provider failure");}));},"PickTarget swallowed a reached bestAim provider failure");
    Require(reads==1u,"Live bestAim failure was retried");
    reads=0u;const auto maximum=std::numeric_limits<float>::max();
    Refuse([&]{static_cast<void>(Actual({2,0,0},{},{maximum,0,0},0.0f,reads));},"Mandatory Float angle overflow was silently accepted");
    Require(!reads,"Angle store overflow read inactive bestAim");
    // Finite opposite huge positions can yield a PC64 delta outside Float
    // range. It is legal until the mandatory angle spill; never pre-spill it.
    reads=0u;const auto far=Actual({maximum,0,0},{-maximum,0,0},{std::numeric_limits<float>::denorm_min(),0,0},0.0f,reads);
    Require(!far&&!reads,"Huge finite unspilled delta did not reject at the retained range gate");++portableCases;
    const auto minimum=std::numeric_limits<float>::denorm_min();
    reads=0u;Refuse([&]{static_cast<void>(Actual({minimum,minimum,minimum},{},
        {maximum,maximum,maximum},0.0f,reads));},"Mandatory Float candidateAim overflow was silently accepted");
    Require(!reads,"CandidateAim overflow read inactive bestAim");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Hardware {
using Real=long double;
struct Precision {
    unsigned short original{};
    Precision(){static_assert(std::numeric_limits<Real>::digits==64,"Independent oracle needs x87 PC64");
        asm volatile("fnstcw %0":"=m"(original));const auto mode=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));}
    ~Precision(){asm volatile("fldcw %0"::"m"(original));}
};
Real Read(float value){if(!std::isfinite(value))throw std::runtime_error("Independent active Float nonfinite");return value;}
Real Add(Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,z=x*y;return z;}
Real Div(Real a,Real b){if(b==0)throw std::runtime_error("Independent zero division");volatile Real x=a,y=b,z=x/y;return z;}
float Store(Real value){float result;asm volatile("fldt %1; fstps %0":"=m"(result):"m"(value):"st");
    if(!std::isfinite(result))throw std::runtime_error("Independent Float overflow");
    return result;}
double Spill(Real value){double result;asm volatile("fldt %1; fstpl %0":"=m"(result):"m"(value):"st");
    if(!std::isfinite(result))throw std::runtime_error("Independent Double overflow");
    return result;}
Real Root(double value){if(!std::isfinite(value)||value<=0)throw std::runtime_error("Independent invalid positive root");
    Real result;asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(result):"m"(value):"st");return result;}
Result Oracle(Vec point,Vec start,Vec direction,float best,std::size_t& reads){
    const auto x=Sub(Read(point.x),Read(start.x)),y=Sub(Read(point.y),Read(start.y)),z=Sub(Read(point.z),Read(start.z));
    const float angle=Store(Add(Add(Mul(z,Read(direction.z)),Mul(y,Read(direction.y))),Mul(x,Read(direction.x))));
    if(!(angle>0))return {};
    const auto sx=Sub(Read(point.x),Read(start.x)),sy=Sub(Read(point.y),Read(start.y)),sz=Sub(Read(point.z),Read(start.z));
    const auto square=Add(Add(Mul(sz,sz),Mul(sy,sy)),Mul(sx,sx));
    if(!(square<4000000.0f))return {};
    const float distance=Store(Root(Spill(square)));
    const auto quotient=Div(angle,distance);const float aim=Store(quotient);
    ++reads;if(!(quotient>Read(best)))return {};
    return Candidate{aim,distance};
}
} // namespace Hardware
std::uint64_t Next(std::uint64_t& state){state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
float Broad(std::uint64_t& state){const auto raw=static_cast<std::uint32_t>(Next(state));
    return std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u));}
float Bounded(std::uint64_t& state,float divisor){return static_cast<float>(static_cast<int>(Next(state)%65537u)-32768)/divisor;}
void Check(Vec point,Vec start,Vec direction,float best){
    bool actualRefused{},oracleRefused{};Result actual,expected;std::size_t actualReads{},expectedReads{};
    try{actual=Actual(point,start,direction,best,actualReads);}catch(const std::exception&){actualRefused=true;}
    try{expected=Hardware::Oracle(point,start,direction,best,expectedReads);}catch(const std::exception&){oracleRefused=true;}
    Require(actualRefused==oracleRefused,"PickTarget numeric refusal differs from independent x87");
    Require(actualReads==expectedReads,"PickTarget live bestAim read boundary differs from independent x87");
    if(!actualRefused)Require(Same(actual,expected),"PickTarget candidate bits/gates differ from independent x87");else ++refusals;
    lazyReads+=actualReads;++hardwareCases;
}
void Independent(){
    Hardware::Precision precision;std::uint64_t state=0x9ce26a504e7d83b1ull;
    for(unsigned i=0u;i<50'000u;++i){
        const bool broad=i<15'000u;
        const auto coordinate=[&]{return broad?Broad(state):Bounded(state,16.0f);};
        const Vec point{coordinate(),coordinate(),coordinate()},start{coordinate(),coordinate(),coordinate()};
        const Vec direction{broad?Broad(state):Bounded(state,8192.0f),broad?Broad(state):Bounded(state,8192.0f),
            broad?Broad(state):Bounded(state,8192.0f)};
        Check(point,start,direction,broad?Broad(state):Bounded(state,16384.0f));
    }
    // Dense neighborhoods of strict dot/range/quotient boundaries. The live
    // threshold is deliberately equal to the rounded candidateAim, then one
    // Float below/above, to distinguish FST from FSTP/reload comparisons.
    for(const float center:{std::numeric_limits<float>::denorm_min(),std::ldexp(1.0f,-100),1.0f,1999.0f,2000.0f}){
        float coordinate=center;for(unsigned i=0u;i<12u;++i)coordinate=std::nextafter(coordinate,0.0f);
        for(unsigned i=0u;i<25u;++i){
            for(const float y:{0.0f,0.625f,1.0f,16.0f})for(const float dz:{-1.0f,0.0f,1.0f}){
                const Vec point{coordinate,y,0.0f},direction{1,0,dz};std::size_t reads{};
                const auto result=Hardware::Oracle(point,{},direction,-2.0f,reads);
                const float threshold=result?result->aim:0.0f;
                for(const float best:{std::nextafter(threshold,-std::numeric_limits<float>::infinity()),threshold,
                    std::nextafter(threshold,std::numeric_limits<float>::infinity())})Check(point,{},direction,best);
            }
            coordinate=std::nextafter(coordinate,std::numeric_limits<float>::infinity());
        }
    }
    Check({1,1,0},{std::ldexp(1.0f,-25),0,0},{-1,1,0},0.0f);
    Check({1,1,1},{},{1,-std::ldexp(1.0f,66),std::ldexp(1.0f,66)},0.0f);
    for(unsigned mask=0u;mask<512u;++mask){
        std::array<float,9u> channels{};for(unsigned i=0u;i<channels.size();++i)channels[i]=(mask&(1u<<i))?-0.0f:0.0f;
        Check({channels[0],channels[1],channels[2]},{channels[3],channels[4],channels[5]},
            {channels[6],channels[7],channels[8]},std::numeric_limits<float>::quiet_NaN());
    }
}
#else
void Independent(){std::cout<<"SKIP independent hardware x87 PC64 oracle on this platform; portable scalar controls still run\n";}
#endif
} // namespace
int main(){try{Deterministic();LazyAndSafety();Independent();
    std::cout<<"PASS original Pawn.PickTarget candidate scalar: "<<checks<<" checks / "<<portableCases<<" portable cases / "
        <<hardwareCases<<" independent hardware x87 cases / "<<lazyReads<<" oracle-matched lazy reads / "<<refusals
        <<" explicit refusals; not Pawn traversal, LOS, target OUTs, Training damage or headset evidence\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL PickTarget candidate scalar: "<<error.what()<<'\n';return 1;}}
