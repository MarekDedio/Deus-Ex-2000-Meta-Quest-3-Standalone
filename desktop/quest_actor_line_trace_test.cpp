#include "quest_actor_line_trace.h"
#include "quest_actor_line_trace_x87_oracle.h"

#include <iostream>
#include <limits>

namespace {
using Vec=PortableModelVec3;
using Hit=QuestVr::OriginalActorLineHit;
using Cylinder=QuestVr::OriginalActorCylinder;
std::size_t checks{},refusals{},blocked{},clear{},partial{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(float a,float b) {return Bits(a)==Bits(b);}
bool Same(const Vec& a,const Vec& b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Hit& a,const Hit& b) {
    return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&a.item==b.item&&
        Same(a.time,b.time)&&Same(a.location,b.location)&&Same(a.normal,b.normal);
}
Hit Seed() {return {0x1234u,0x5678u,0xabcdu,{71,72,73},{-4,5,-6},-9.0f,99};}
void UnchangedExceptNormal(const Hit& a,const Hit& b) {
    auto copy=a;copy.normal=b.normal;Require(Same(copy,b),"Clear changed fields other than original partial Normal");
}
template<class F> void Refuse(F run) {
    bool failed{};try {run();} catch(const std::exception&) {failed=true;}
    Require(failed,"Invalid actor cylinder data silently succeeded");++refusals;
}
void Boundaries() {
    const Cylinder owner{17,{0,0,0},2,3};const auto seed=Seed();auto hit=seed;
    const float nan=std::numeric_limits<float>::quiet_NaN(),infinity=std::numeric_limits<float>::infinity();
    Require(QuestVr::CheckOriginalActorCylinder(hit,std::nullopt,{nan,nan,nan},{nan,nan,nan},{nan,nan,nan})&&Same(hit,seed),
        "Null owner read inactive data or changed caller hit");
    auto ignored=owner;ignored.location.y=nan;ignored.location.z=nan;
    Require(QuestVr::CheckOriginalActorCylinder(hit,ignored,{5,nan,nan},{4,nan,nan})&&Same(hit,seed),
        "Upper-X clear read inactive Y/Z data or changed hit");
    Require(!QuestVr::CheckOriginalActorCylinder(hit,owner,{5,0,0},{-5,0,0}),"Central side crossing was clear");
    Require(hit.actor==owner.identity&&hit.primitive==0&&hit.next==seed.next&&hit.item==seed.item,
        "Cylinder hit native identity/untouched packet fields differ");
    Require(hit.time>.2989f&&hit.time<.2991f&&hit.location.x< -2.0f&&hit.location.x> -2.02f&&
        Same(hit.normal,Vec{-1,0,0}),"Central side normal/backoff differs");
    auto otherSeed=Seed();otherSeed.time=0.0f;
    Require(!QuestVr::CheckOriginalActorCylinder(otherSeed,owner,{5,0,0},{-5,0,0})&&Same(otherSeed,hit),
        "Caller Hit.Time incorrectly clipped cylinder query");
    hit=seed;
    Require(!QuestVr::CheckOriginalActorCylinder(hit,owner,{0,0,3},{0,0,3})&&Same(hit.time,0)&&
        Same(hit.normal,seed.normal),"Exact upper-cap contact was incorrectly rejected by broadphase");
    hit=seed;
    Require(QuestVr::CheckOriginalActorCylinder(hit,owner,{1,1,4},{-1,-1,4})&&Same(hit,seed),
        "Strict upper-Z broadphase clear changed caller packet");
    hit=seed;
    Require(!QuestVr::CheckOriginalActorCylinder(hit,owner,{0,0,-5},{0,0,5})&&Same(hit.normal,Vec{0,0,1}),
        "Downward cap sweep/near-vertical C<=0 path differs");
    Require(hit.location.z>3.0f&&hit.location.z<3.02f,"Cap parametric backoff is wrong");
    hit=seed;
    Require(!QuestVr::CheckOriginalActorCylinder(hit,owner,{0,0,5},{0,0,-5})&&Same(hit.normal,Vec{0,0,-1}),
        "Upward lower-cap crossing differs");
    hit=seed;
    Require(QuestVr::CheckOriginalActorCylinder(hit,owner,{-4,0,2.9f},{4,0,5})&&Same(hit.normal,Vec{0,0,1}),
        "Rejected cap/radial interval lost partial cap Normal");
    UnchangedExceptNormal(hit,seed);++partial;
    hit=seed;const Cylinder thin{23,{0,0,0},1,3};
    Require(QuestVr::CheckOriginalActorCylinder(hit,thin,{5,0,5},{-5,0,2})&&Same(hit.normal,Vec{-1,0,0}),
        "Rejected cap/radial interval lost partial radial Normal");
    UnchangedExceptNormal(hit,seed);++partial;
    hit=seed;
    Require(!QuestVr::CheckOriginalActorCylinder(hit,owner,{-1,0,0},{1,0,0})&&Same(hit.time,0)&&
        Same(hit.location,Vec{1,0,0})&&Same(hit.normal,Vec{1,0,0}),"Initial overlap moving inward was not blocked at Start");
    hit=seed;
    Require(QuestVr::CheckOriginalActorCylinder(hit,owner,{3,0,0},{1,0,0})&&Same(hit,seed),
        "Initial overlap moving outward should be clear/untouched");
    hit=seed;
    Require(QuestVr::CheckOriginalActorCylinder(hit,owner,{0,0,-5},{0,0,0})&&Same(hit,seed),
        "Initial overlap stationary XY was replaced by ordinary vertical collision");
    hit=seed;
    // Original uses C<1, not C<0: the start may be outside the exact radius.
    Require(!QuestVr::CheckOriginalActorCylinder(hit,thin,{-1,0,0},{1.25f,0,0})&&Same(hit.time,0),
        "Original initial-overlap C<1 tolerance was lost");
    hit=seed;
    Require(QuestVr::CheckOriginalActorCylinder(hit,owner,{5,2.5f,0},{-5,2.5f,0},{0,100,0})&&Same(hit,seed),
        "Y extent incorrectly inflated the narrowphase radial radius");
    hit=seed;
    Require(!QuestVr::CheckOriginalActorCylinder(hit,owner,{5,2.5f,0},{-5,2.5f,0},{1,100,0}),
        "X extent did not inflate the original radial radius");
    hit=seed;const Cylinder unit{17,{0,0,0},1,1};
    Require(QuestVr::CheckOriginalActorCylinder(hit,unit,{1,1,0},{1,1,2})&&Same(hit.normal,Vec{0,0,1}),
        "Near-vertical A=0/C>0 clear lost cap Normal");
    UnchangedExceptNormal(hit,seed);++partial;
    hit=seed;
    Require(QuestVr::CheckOriginalActorCylinder(hit,unit,{0,0,2},{2,0,0})&&Same(hit.normal,Vec{1,0,0}),
        "Exact Enter==Exit contact did not retain radial Normal on clear");
    UnchangedExceptNormal(hit,seed);++partial;
    hit=seed;
    Require(!QuestVr::CheckOriginalActorCylinder(hit,unit,{0,0,-1},{0,0,-1})&&Same(hit.time,0)&&
        Same(hit.location,Vec{0,0,-1})&&Same(hit.normal,seed.normal),
        "Zero-length lower-cap original hit/seeded Normal asymmetry differs");
    // Finite refusal is atomic even after a cap Normal has already changed.
    hit=seed;
    Refuse([&]{QuestVr::CheckOriginalActorCylinder(hit,owner,{infinity,0,0},{0,0,5});});
    Require(Same(hit,seed),"Refused active endpoint retained partial caller writes");
    for(const unsigned field:{0u,1u,2u,3u,4u}) {
        auto invalid=owner;Vec extent{};
        if(field==0u) invalid.radius=-1;else if(field==1u) invalid.height=-1;
        else if(field==2u) extent.x=-1;else if(field==3u) extent.y=-1;else extent.z=-1;
        hit=seed;Refuse([&]{QuestVr::CheckOriginalActorCylinder(hit,invalid,{5,0,0},{-5,0,0},extent);});
        Require(Same(hit,seed),"Negative-dimension refusal changed caller hit");
    }
    auto invalid=owner;invalid.identity=0;
    Refuse([&]{QuestVr::CheckOriginalActorCylinder(hit,invalid,{},{});});
    invalid=owner;invalid.height=std::numeric_limits<float>::max();
    Refuse([&]{QuestVr::CheckOriginalActorCylinder(hit,invalid,{}, {},{0,0,std::numeric_limits<float>::max()});});
    hit=seed;hit.normal={nan,infinity,-infinity};hit.location={nan,nan,nan};hit.time=nan;
    const auto arbitrary=hit;
    Require(QuestVr::CheckOriginalActorCylinder(hit,owner,{6,0,0},{5,0,0})&&Same(hit,arbitrary),
        "Early clear read/normalized unused arbitrary caller hit fields");
    Require(!QuestVr::CheckOriginalActorCylinder(hit,owner,{5,0,0},{-5,0,0})&&Same(hit.normal,Vec{-1,0,0})&&
        std::isfinite(hit.time),"Hit incorrectly validated overwritten seed packet data");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Oracle=QuestVr::ActorLineX87Oracle;
Oracle::Hit ToOracle(const Hit& v) {
    Oracle::Hit result;result.next=v.next;result.actor=v.actor;result.primitive=v.primitive;result.time=v.time;
    result.item=v.item;result.location={v.location.x,v.location.y,v.location.z};result.normal={v.normal.x,v.normal.y,v.normal.z};return result;
}
Hit FromOracle(const Oracle::Hit& v) {return {v.next,v.actor,v.primitive,{v.location.x,v.location.y,v.location.z},
    {v.normal.x,v.normal.y,v.normal.z},v.time,v.item};}
std::uint64_t Next(std::uint64_t& value) {value^=value<<13u;value^=value>>7u;value^=value<<17u;return value;}
float Number(std::uint64_t& seed,bool broad) {
    const auto raw=static_cast<std::uint32_t>(Next(seed));
    return broad?std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u)):
        static_cast<float>(static_cast<int>(raw%65537u)-32768)/256.0f;
}
void DifferentialOne(const Cylinder& owner,const Vec& end,const Vec& start,const Vec& extent) {
    const auto seed=Seed();auto actual=seed;auto expected=ToOracle(seed);bool failed{},oracleFailed{},a{},b{};
    const Oracle::Cylinder independent{owner.identity,{owner.location.x,owner.location.y,owner.location.z},owner.radius,owner.height};
    try {a=QuestVr::CheckOriginalActorCylinder(actual,owner,end,start,extent);} catch(const std::exception&) {failed=true;}
    try {b=Oracle::Check(expected,independent,{end.x,end.y,end.z},{start.x,start.y,start.z},{extent.x,extent.y,extent.z});}
    catch(const std::exception&) {oracleFailed=true;}
    Require(failed==oracleFailed,"Cylinder finite refusal differs from independent hardware x87");
    if(failed) {Require(Same(actual,seed),"Cylinder refusal did not restore seeded caller hit");++refusals;return;}
    Require(a==b,"Cylinder clear/blocked branch differs from independent hardware x87");
    if(!Same(actual,FromOracle(expected))) throw std::runtime_error("Cylinder packet differs from independent hardware x87: time "+
        std::to_string(Bits(actual.time))+"/"+std::to_string(Bits(expected.time))+" normal "+
        std::to_string(Bits(actual.normal.x))+","+std::to_string(Bits(actual.normal.y))+","+std::to_string(Bits(actual.normal.z))+
        " / "+std::to_string(Bits(expected.normal.x))+","+std::to_string(Bits(expected.normal.y))+","+std::to_string(Bits(expected.normal.z))+
        " location "+std::to_string(Bits(actual.location.x))+","+std::to_string(Bits(actual.location.y))+","+std::to_string(Bits(actual.location.z))+
        " / "+std::to_string(Bits(expected.location.x))+","+std::to_string(Bits(expected.location.y))+","+std::to_string(Bits(expected.location.z)));
    ++checks;if(a) {++clear;if(!Same(actual.normal,seed.normal)) ++partial;} else ++blocked;
}
void Differential() {
    Oracle::Precision precision;std::uint64_t seed=0x47a691e87c543210ull;
    for(unsigned i=0;i<24'000u;++i) {
        const bool broad=i<4'000u;const auto number=[&]{return Number(seed,broad);};
        const Cylinder owner{17,{number(),number(),number()},std::fabs(number()),std::fabs(number())};
        const Vec start{number(),number(),number()},end{number(),number(),number()};
        const Vec extent=i%5u?Vec{}:Vec{std::fabs(number()),std::fabs(number()),std::fabs(number())};
        DifferentialOne(owner,end,start,extent);
    }
    // Dense exact contacts and nextafter neighbors at cap/radial and initial
    // overlap boundaries, including signed zero and subnormal dimensions.
    const std::array<float,13u> edges{-5,-3,-2,std::nextafter(-2.0f,0.0f),-0.0f,0,
        std::numeric_limits<float>::denorm_min(),std::nextafter(2.0f,0.0f),2,
        std::nextafter(2.0f,3.0f),3,4,5};
    for(float sx:edges) for(float sy:edges) for(float sz:edges) {
        DifferentialOne({17,{0,0,0},2,3},{-sx,-sy,-sz},{sx,sy,sz},{});
        DifferentialOne({19,{0,0,0},2,3},{-sx,sy,sz},{sx,sy,sz},{.125f,.25f,.5f});
    }
    for(float value:{0.0f,-0.0f,.0001f,std::nextafter(.0001f,0.0f),std::nextafter(.0001f,1.0f),
        .1f,std::nextafter(.1f,0.0f),std::nextafter(.1f,1.0f),1.0f,std::nextafter(1.0f,0.0f),std::nextafter(1.0f,2.0f)}) {
        DifferentialOne({17,{0,0,0},1,3},{-value,0,0},{value,0,0},{});
        DifferentialOne({17,{0,0,0},1,3},{1.0f-value,0,0},{1,0,0},{});
        DifferentialOne({17,{0,0,0},1,3},{value,0,-5},{0,0,5},{});
    }
    Require(blocked>1'000u&&clear>10'000u&&partial>10u,"Differential cylinder cases missed hit/clear/partial-write branches");
}
#else
void Differential() {std::cout<<"SKIP independent x87 cylinder oracle on non-x86 compiler; deterministic controls still run.\n";}
#endif
} // namespace
int main() {
    try {Boundaries();Differential();std::cout<<"PASS original actor cylinder "<<checks<<" checks, "<<refusals
        <<" safety refusals, "<<blocked<<" blocked, "<<clear<<" clear, "<<partial<<" partial-Normal clears.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Original actor cylinder failed: "<<error.what()<<'\n';return 1;}
}
