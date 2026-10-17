#include "quest_mesh_line_trace.h"

#include <array>
#include <cfenv>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

// Wrapper/adapter tests only. The expected packet comes from the separately
// verified cylinder core after independently selecting literal extent bits;
// this is not an independent numeric oracle for cylinder collision arithmetic.
namespace {
using Vec=PortableModelVec3;
using Hit=QuestVr::OriginalActorLineHit;
using Cylinder=QuestVr::OriginalActorCylinder;
std::size_t checks{},refusals{},blocked{},clear{},partial{},normalized{},forwarded{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
float Float(std::uint32_t bits) {return std::bit_cast<float>(bits);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(float a,float b) {return Bits(a)==Bits(b);}
bool Same(const Vec& a,const Vec& b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Hit& a,const Hit& b) {
    return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&a.item==b.item&&
        Same(a.time,b.time)&&Same(a.location,b.location)&&Same(a.normal,b.normal);
}
bool Same(const std::optional<Cylinder>& a,const std::optional<Cylinder>& b) {
    if(bool(a)!=bool(b)) return false;
    return !a||(a->identity==b->identity&&Same(a->location,b->location)&&Same(a->radius,b->radius)&&Same(a->height,b->height));
}
Hit Seed() {return {0x1234u,0x5678u,0xabcdu,{71,72,73},{-4,5,-6},-9.0f,99};}
Vec ExpectedExtent(const Vec& value) {
    // Independent ordering of IEEE Float magnitude bits: all NaN encodings
    // are strictly above +Infinity; either zero has magnitude zero.
    const std::array<std::uint32_t,3u> magnitude{
        Bits(value.x)&0x7fffffffu,Bits(value.y)&0x7fffffffu,Bits(value.z)&0x7fffffffu};
    for(const auto bits:magnitude) if(bits!=0u&&bits<=0x7f800000u) return value;
    return {Float(0u),Float(0u),Float(0u)};
}
void ExtentIs(const Vec& input,const Vec& expected) {
    const auto saved=input;
    std::feclearexcept(FE_ALL_EXCEPT);
    const auto actual=QuestVr::MeshLineTraceDetail::Extent(input);
    Require(std::fetestexcept(FE_INVALID)==0,"Extent bit classification compared a signaling NaN arithmetically");
    Require(Same(actual,expected),"Mesh wrapper extent bits differ from independent literal expectation");
    Require(Same(input,saved),"Mesh wrapper canonicalization mutated its caller vector");
    if(Bits(expected.x)==0u&&Bits(expected.y)==0u&&Bits(expected.z)==0u) ++normalized;
    else ++forwarded;
}
struct Outcome {Hit hit;bool refused{},clear{};std::string error;};
Outcome Core(const Hit& seed,const std::optional<Cylinder>& owner,const Vec& end,const Vec& start,const Vec& extent) {
    Outcome result;result.hit=seed;
    try {result.clear=QuestVr::CheckOriginalActorCylinder(result.hit,owner,end,start,extent);}
    catch(const std::exception& error) {result.refused=true;result.error=error.what();}
    return result;
}
Outcome Check(const std::optional<Cylinder>& owner,const Vec& end,const Vec& start,const Vec& extent,
    std::uint32_t flags=0u,const Hit& seed=Seed()) {
    const auto originalOwner=owner;const auto originalEnd=end,originalStart=start,originalExtent=extent;
    const auto expected=Core(seed,owner,end,start,ExpectedExtent(extent));
    Outcome actual;actual.hit=seed;
    try {actual.clear=QuestVr::OriginalMeshLineCheck(actual.hit,owner,end,start,extent,flags);}
    catch(const std::exception& error) {actual.refused=true;actual.error=error.what();}
    Require(actual.refused==expected.refused,"Mesh wrapper changed core refusal/active-read branch");
    Require(Same(actual.hit,expected.hit),"Mesh wrapper caller packet differs bitwise from normalized cylinder adapter oracle");
    Require(Same(owner,originalOwner)&&Same(end,originalEnd)&&Same(start,originalStart)&&Same(extent,originalExtent),
        "Mesh wrapper changed owner/end/start/extent input data");
    if(actual.refused) {
        Require(actual.error==expected.error,"Mesh wrapper changed core refusal diagnostic/order");
        Require(Same(actual.hit,seed),"Mesh wrapper refusal leaked partial packet writes");++refusals;
    } else {
        Require(actual.clear==expected.clear,"Mesh wrapper changed normalized cylinder clear/blocked result");
        if(actual.clear) {++clear;if(!Same(actual.hit.normal,seed.normal)) ++partial;}
        else ++blocked;
    }
    return actual;
}
void Boundaries() {
    const auto seed=Seed();const Cylinder owner{17u,{0,0,0},2,3};
    const float positiveZero=Float(0u),negativeZero=Float(0x80000000u);
    const float quietNaN=Float(0x7fc12345u),signalingNaN=Float(0x7f800001u),negativeNaN=Float(0xff812345u);
    const float infinity=Float(0x7f800000u),negativeInfinity=Float(0xff800000u);
    const Vec zero{positiveZero,positiveZero,positiveZero};
    for(const auto input:std::array<Vec,7u>{{{-0.0f,0.0f,-0.0f},{quietNaN,0,negativeZero},
        {0,signalingNaN,negativeZero},{negativeZero,positiveZero,negativeNaN},{quietNaN,signalingNaN,negativeNaN},
        {signalingNaN,signalingNaN,signalingNaN},{negativeNaN,negativeNaN,negativeNaN}}}) {
        ExtentIs(input,zero);
        const auto result=Check(owner,{5,0,0},{-5,0,0},input);
        Require(!result.refused&&!result.clear&&result.hit.actor==owner.identity&&result.hit.primitive==0u,
            "All-zero-or-NaN extent did not become a valid point cylinder query");
    }
    for(const auto input:std::array<Vec,9u>{{{1,negativeZero,quietNaN},{quietNaN,1,negativeZero},
        {quietNaN,negativeZero,1},{infinity,quietNaN,negativeZero},{quietNaN,infinity,negativeZero},
        {quietNaN,negativeZero,infinity},{Float(1u),negativeZero,positiveZero},
        {negativeZero,Float(0x80000001u),positiveZero},{negativeZero,positiveZero,-1}}}) ExtentIs(input,input);

    // A real null owner reads no endpoints, dimensions or caller packet, even
    // when extents survive the wrapper unchanged and include Infinity/NaNs.
    auto arbitrary=seed;arbitrary.normal={signalingNaN,infinity,negativeInfinity};
    arbitrary.location={negativeNaN,quietNaN,signalingNaN};arbitrary.time=signalingNaN;
    std::feclearexcept(FE_ALL_EXCEPT);
    auto nullHit=arbitrary;
    Require(QuestVr::OriginalMeshLineCheck(nullHit,std::nullopt,{signalingNaN,infinity,negativeNaN},
        {negativeNaN,negativeInfinity,quietNaN},{signalingNaN,negativeNaN,-0.0f},0xffffffffu)&&Same(nullHit,arbitrary),
        "Null-owner wrapper read inactive packet/endpoints or failed to clear");
    Require(std::fetestexcept(FE_INVALID)==0,"Null-owner wrapper arithmetically inspected discarded signaling NaNs");
    Check(std::nullopt,{infinity,quietNaN,signalingNaN},{negativeInfinity,negativeNaN,quietNaN},
        {infinity,negativeZero,signalingNaN},0x80000000u,arbitrary);

    auto ignored=owner;ignored.location.y=quietNaN;ignored.location.z=infinity;
    const auto broad=Check(ignored,{5,signalingNaN,infinity},{4,quietNaN,negativeInfinity},
        {signalingNaN,-0.0f,negativeNaN});
    Require(!broad.refused&&broad.clear&&Same(broad.hit,seed),"Canonicalized extent lost lazy upper-X clear");
    ignored=owner;ignored.location.z=quietNaN;
    const auto yClear=Check(ignored,{0,5,signalingNaN},{0,4,negativeNaN},{quietNaN,signalingNaN,0});
    Require(!yClear.refused&&yClear.clear&&Same(yClear.hit,seed),"Canonicalized extent lost lazy upper-Y clear");

    const auto capPartial=Check(owner,{-4,0,2.9f},{4,0,5},{quietNaN,-0.0f,signalingNaN});
    auto capExpected=seed;capExpected.normal={0,0,1};
    Require(!capPartial.refused&&capPartial.clear&&Same(capPartial.hit,capExpected),
        "Mesh clear lost exact partial cap Normal or changed another field");
    const auto radialPartial=Check(Cylinder{23u,{0,0,0},1,3},{5,0,5},{-5,0,2},{0,negativeNaN,-0.0f});
    auto radialExpected=seed;radialExpected.normal={-1,0,0};
    Require(!radialPartial.refused&&radialPartial.clear&&Same(radialPartial.hit,radialExpected),
        "Mesh clear lost exact partial radial Normal or changed another field");

    const auto lateRefusal=Check(owner,{infinity,0,0},{0,0,5},{signalingNaN,quietNaN,-0.0f});
    Require(lateRefusal.refused&&Same(lateRefusal.hit,seed),"Late endpoint refusal leaked a preceding cap Normal write");
    for(const auto flags:std::array<std::uint32_t,8u>{0u,1u,0x21u,0x80u,0x100u,0x7fffffffu,0x80000000u,0xffffffffu}) {
        const auto result=Check(owner,{5,0,0},{-5,0,0},{quietNaN,-0.0f,signalingNaN},flags,arbitrary);
        Require(!result.refused&&!result.clear&&result.hit.next==arbitrary.next&&result.hit.item==arbitrary.item,
            "Ignored original Flags changed hit or preserved packet fields");
    }
    // Identity precedes dimension reads; dimensions precede X broadphase.
    // The shared core is the oracle for this order, not an eager wrapper check.
    auto invalid=owner;invalid.identity=0u;invalid.radius=quietNaN;invalid.height=-1;
    Require(Check(invalid,{infinity,0,0},{infinity,0,0},{quietNaN,1,signalingNaN}).refused,
        "Present owner with null identity was treated as absent");
    for(unsigned field=0u;field<9u;++field) {
        invalid=owner;Vec extent{};Vec start{-5,0,0},end{5,0,0};
        if(field==0u) invalid.height=-1;
        else if(field==1u) invalid.radius=quietNaN;
        else if(field==2u) extent.z=-1;
        else if(field==3u) extent.y=infinity;
        else if(field==4u) extent.x=-1;
        else if(field==5u) invalid.location.x=quietNaN;
        else if(field==6u) start.x=quietNaN;
        else if(field==7u) end.x=infinity;
        else {extent.z=std::numeric_limits<float>::max();invalid.height=std::numeric_limits<float>::max();}
        Require(Check(invalid,end,start,extent).refused,"Invalid active owner/dimension/endpoint silently succeeded");
    }
    invalid=owner;invalid.height=quietNaN;
    Require(Check(invalid,{5,0,0},{4,0,0},{0,0,-1}).refused,"Negative active extent was skipped for X broadphase");
    invalid=owner;invalid.height=-1;
    Require(Check(invalid,{5,0,0},{4,0,0},{0,1,signalingNaN}).refused,"Forwarded NaN extent was discarded before core dimension order");
    Require(Check(owner,{5,0,0},{4,0,0},{quietNaN,1,0}).refused,"Earlier NaN was normalized despite later nonzero extent");
}
void ExtentGrid() {
    constexpr std::array<std::uint32_t,16u> values{0u,0x80000000u,0x7fc00000u,0xffc00000u,
        0x7f800001u,0xff800001u,0x7fffffffu,0xffffffffu,0x00000001u,0x80000001u,
        0x3f800000u,0xbf800000u,0x7f800000u,0xff800000u,0x7f7fffffu,0xff7fffffu};
    for(const auto x:values) for(const auto y:values) for(const auto z:values) {
        const Vec extent{Float(x),Float(y),Float(z)};ExtentIs(extent,ExpectedExtent(extent));
        Check(std::nullopt,{Float(0x7f800001u),0,0},{Float(0xff800001u),0,0},extent,0xffffffffu);
        Check(Cylinder{17u,{0,0,0},2,3},{5,0,0},{-5,0,0},extent);
    }
}
std::uint64_t Next(std::uint64_t& state) {state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
float Finite(std::uint64_t& state,bool broad) {
    const auto raw=static_cast<std::uint32_t>(Next(state));
    if(broad) return Float((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u));
    return static_cast<float>(static_cast<int>(raw%8193u)-4096)/256.0f;
}
void Samples() {
    std::uint64_t state=0x9e7a613b24c850dfull;
    constexpr std::array<std::uint32_t,12u> special{0u,0x80000000u,0x7fc01234u,0xffc05678u,
        0x7f800001u,0xff800001u,0x7f800000u,0xff800000u,1u,0x80000001u,0x3f800000u,0xbf800000u};
    for(unsigned i=0u;i<12'000u;++i) {
        const bool broad=i<1'000u;const auto number=[&] {return Finite(state,broad);};
        const auto nonnegative=[&] {return Float(Bits(number())&0x7fffffffu);};
        std::optional<Cylinder> owner=Cylinder{17u,{number(),number(),number()},nonnegative(),nonnegative()};
        Vec start{number(),number(),number()},end{number(),number(),number()},extent{};
        if(i%4u==0u) extent={nonnegative(),nonnegative(),nonnegative()};
        if(i%4u==1u) extent={Float(special[Next(state)%special.size()]),Float(special[Next(state)%special.size()]),
            Float(special[Next(state)%special.size()])};
        if(i%4u==2u) extent={Float(static_cast<std::uint32_t>(Next(state))),Float(static_cast<std::uint32_t>(Next(state))),
            Float(static_cast<std::uint32_t>(Next(state)))};
        if(i%13u==0u) owner.reset();
        else if(i%29u==0u) owner->identity=0u;
        else if(i%31u==0u) owner->height=Float(0x7f800001u);
        else if(i%37u==0u) owner->location.y=Float(0x7fc01234u);
        if(i%41u==0u) end.z=Float(0x7f800000u);
        ExtentIs(extent,ExpectedExtent(extent));
        Check(owner,end,start,extent,static_cast<std::uint32_t>(Next(state)));
    }
    Require(blocked>100u&&clear>1'000u&&refusals>1'000u&&partial>=2u&&normalized>100u&&forwarded>1'000u,
        "Mesh adapter cases missed normalization/forwarding/hit/clear/refusal/partial-write branches");
}
} // namespace
int main() {
    try {
        Boundaries();ExtentGrid();Samples();
        std::cout<<"PASS original Mesh LineCheck extent wrapper "<<checks<<" checks, "<<refusals<<" safety refusals, "
            <<blocked<<" blocked, "<<clear<<" clear, "<<partial<<" partial-Normal clears, "<<normalized<<" normalized extents, "
            <<forwarded<<" forwarded extents. Adapter/core reuse evidence, not independent cylinder arithmetic or native instance authority.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"Original Mesh LineCheck wrapper failed: "<<error.what()<<'\n';return 1;}
}
