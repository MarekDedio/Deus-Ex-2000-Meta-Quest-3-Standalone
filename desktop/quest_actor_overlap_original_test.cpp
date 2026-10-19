#include "quest_actor_overlap_original.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

// Independent synthetic field protocol and hardware-x87 numeric oracle only.
// No original commercial code, runtime map, renderer or production math oracle.
namespace {
using Id=std::uintptr_t;
using Queries=QuestVr::OriginalActorOverlapQueries;
constexpr Id source=11u,other=22u,world=33u;
std::size_t checks{},refusals{},transcripts{},differentials{},overlaps{},clear{},wideCases{},prefixes{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Refuse(F&& run,const char* why="Unsupported original overlap silently succeeded") {
    bool failed{};try{run();}catch(const std::exception&){failed=true;}Require(failed,why);++refusals;
}
enum class Op { Work,Brush,Level,Location,Radius,Height };
struct Event {Op op;Id actor{};unsigned component{};bool operator==(const Event&)const=default;};
Event W(){return {Op::Work};}Event B(Id id){return {Op::Brush,id};}Event L(Id id){return {Op::Level,id};}
Event P(Id id,unsigned component){return {Op::Location,id,component};}
Event R(Id id){return {Op::Radius,id};}Event H(Id id){return {Op::Height,id};}
struct Body {std::array<float,3u> location{};float radius{1.0f},height{1.0f};bool brush{};Id level{world};};
struct Fixture {
    std::array<Body,34u> bodies{};std::vector<Event> trace;
    bool tracing{};std::size_t work{};std::function<void(Event)> before;
    Body& Actor(Id id){if(!id||id>=bodies.size())throw std::runtime_error("Independent overlap actor unavailable");return bodies[id];}
    void Record(Event event){if(tracing)trace.push_back(event);if(before)before(event);}
    Queries Provider(){Queries q;
        q.isBrush=[&](Id id){Record(B(id));return Actor(id).brush;};
        q.level=[&](Id id){Record(L(id));return Actor(id).level;};
        q.location=[&](Id id,unsigned component){Record(P(id,component));return Actor(id).location.at(component);};
        q.radius=[&](Id id){Record(R(id));return Actor(id).radius;};
        q.height=[&](Id id){Record(H(id));return Actor(id).height;};
        q.work=[&]{Record(W());++work;};return q;
    }
};
std::vector<Event> Charged(const std::vector<Event>& fields){std::vector<Event> result;
    for(const auto& field:fields){result.push_back(W());result.push_back(field);}return result;
}
const std::vector<Event> fullFields{B(source),B(other),L(source),P(source,0u),P(other,0u),
    P(source,1u),P(other,1u),R(other),R(source),P(source,2u),P(other,2u),H(other),H(source)};
void SameTrace(Fixture& fixture,const std::vector<Event>& fields){
    Require(fixture.trace==Charged(fields),"Original overlap provider/work read order differs from source transcript");++transcripts;
}
bool Actual(Fixture&,const Queries& queries){return QuestVr::IsOriginalActorOverlapping(source,other,queries);}
void GoldenAndProtocol(){
    Fixture f;auto q=f.Provider();
    Require(Actual(f,q),"Coincident finite cylinders should overlap");
    f.Actor(other).location[0u]=2.0f;Require(!Actual(f,q),"Exact radial tangency is not strict overlap");
    f.Actor(other).location[0u]=std::nextafter(2.0f,0.0f);Require(Actual(f,q),"Inside radial neighbor should overlap");
    f.Actor(other).location[0u]=std::nextafter(2.0f,3.0f);Require(!Actual(f,q),"Outside radial neighbor should clear");
    f.Actor(source).radius=1.0f;f.Actor(other).radius=4.0f;f.Actor(other).location={3.0f,4.0f,0.0f};
    Require(!Actual(f,q),"Exact3/4/5 diagonal radial tangency is not strict overlap");
    f.Actor(other).location[1u]=std::nextafter(4.0f,0.0f);Require(Actual(f,q),"Inside diagonal radial neighbor should overlap");
    f.Actor(other).location[1u]=0.0f;f.Actor(source).radius=f.Actor(other).radius=1.0f;
    f.Actor(other).location[0u]=0.0f;f.Actor(other).location[2u]=2.0f;
    Require(!Actual(f,q),"Exact cap tangency is not strict overlap");
    f.Actor(other).location[2u]=std::nextafter(2.0f,0.0f);Require(Actual(f,q),"Inside cap neighbor should overlap");
    f.Actor(other).location[2u]=std::nextafter(2.0f,3.0f);Require(!Actual(f,q),"Outside cap neighbor should clear");
    f.Actor(other).location={};
    f.Actor(source).radius=f.Actor(other).radius=-1.0f;f.Actor(source).height=f.Actor(other).height=-1.0f;
    Require(Actual(f,q),"Native signed dimension sums must be squared, not rejected");
    f.Actor(other).radius=1.0f;Require(!Actual(f,q),"Opposite radii cancel to strict zero-radius clear");
    f.Actor(source).radius=f.Actor(other).radius=1.0f;f.Actor(other).height=1.0f;
    Require(!Actual(f,q),"Opposite heights cancel to strict zero-height clear");
    f=Fixture{};q=f.Provider();
    Require(QuestVr::IsOriginalActorOverlapping(source,source,q),"Original overlap has no self exclusion");
    f.Actor(source).brush=true;f.tracing=true;
    Require(!QuestVr::IsOriginalActorOverlapping(source,0u,q),"Source brush failed to bypass null/inactive Other");SameTrace(f,{B(source)});
    f.Actor(source).brush=false;f.Actor(other).brush=true;f.trace.clear();
    Require(!Actual(f,q),"Other brush should clear overlap");SameTrace(f,{B(source),B(other)});
    f.Actor(other).brush=false;f.Actor(source).level=other;f.trace.clear();
    Require(!Actual(f,q),"Other==source.Level must clear before geometry");SameTrace(f,{B(source),B(other),L(source)});
    f.Actor(source).level=world;f.Actor(other).location[0u]=2.0f;f.trace.clear();
    Require(!Actual(f,q),"XY-clear transcript control unexpectedly overlaps");
    SameTrace(f,std::vector<Event>(fullFields.begin(),fullFields.begin()+9u));
    f.Actor(other).location[0u]=0.0f;f.trace.clear();Require(Actual(f,q),"Full numeric transcript control did not overlap");SameTrace(f,fullFields);
    // Every provider is required only when its own native field is reached.
    Queries lazy;lazy.isBrush=[](Id){return true;};
    Require(!QuestVr::IsOriginalActorOverlapping(source,0u,lazy),"Inactive missing field provider was eagerly required");
    lazy.isBrush=[](Id id){return id==other;};Require(!QuestVr::IsOriginalActorOverlapping(source,other,lazy),"Other-brush lazy providers failed");
    lazy.isBrush=[](Id){return false;};lazy.level=[](Id){return other;};
    Require(!QuestVr::IsOriginalActorOverlapping(source,other,lazy),"Level lazy providers failed");
    lazy.level=[](Id){return world;};Refuse([&]{QuestVr::IsOriginalActorOverlapping(source,other,lazy);});
    auto missing=q;missing.radius={};Refuse([&]{Actual(f,missing);});
    missing=q;missing.height={};Refuse([&]{Actual(f,missing);});
    Refuse([&]{QuestVr::IsOriginalActorOverlapping(0u,other,q);});
    f.Actor(source).brush=false;Refuse([&]{QuestVr::IsOriginalActorOverlapping(source,0u,q);});
    // A live callback changes a later field; no aggregate snapshot is allowed.
    f=Fixture{};q=f.Provider();f.Actor(other).location[0u]=1.0f;
    f.Actor(source).radius=f.Actor(other).radius=0.0f;
    f.before=[&](Event event){if(event==R(other))f.Actor(source).radius=2.0f;};
    Require(Actual(f,q),"Overlap cached source radius before actual other-radius read");
}
void ActiveInputsAndWork(){
    constexpr std::array exceptional{std::uint32_t{0x7fc12345u},std::uint32_t{0xffc54321u},
        std::uint32_t{0x7f812345u},std::uint32_t{0xff812345u},std::uint32_t{0x7f800000u},std::uint32_t{0xff800000u}};
    for(const auto word:exceptional)for(unsigned field=0u;field<10u;++field){
        Fixture f;auto q=f.Provider();const auto value=std::bit_cast<float>(word);const Id id=field<5u?source:other;
        const auto slot=field%5u;
        if(slot<3u)f.Actor(id).location[slot]=value;else if(slot==3u)f.Actor(id).radius=value;else f.Actor(id).height=value;
        Refuse([&]{Actual(f,q);},"Active nonfinite overlap field was admitted");
    }
    const float nan=std::bit_cast<float>(std::uint32_t{0x7fc12345u});
    Fixture f;auto q=f.Provider();
    f.Actor(source).location[2u]=f.Actor(other).location[2u]=nan;
    f.Actor(source).height=f.Actor(other).height=nan;f.Actor(other).location[0u]=2.0f;
    Require(!Actual(f,q),"XY-clear read inactive Z/heights");q.height={};
    Require(!Actual(f,q),"XY-clear required inactive missing height provider");
    q.location={};q.radius={};q.level={};f.Actor(source).brush=true;
    Require(!Actual(f,q),"Source-brush inspected inactive numeric providers");
    // Callback/work failures propagate at every reached prefix. A rejected
    // provider does not make the next provider or turn an error into false.
    for(std::size_t position=0u;position<fullFields.size()*2u;++position){
        Fixture item;item.tracing=true;auto provider=item.Provider();std::size_t eventIndex{};
        item.before=[&](Event){if(eventIndex++==position)throw std::runtime_error("Expected active prefix refusal");};
        Refuse([&]{Actual(item,provider);});
        const auto expected=Charged(fullFields);
        Require(item.trace==std::vector<Event>(expected.begin(),expected.begin()+position+1u),"Overlap continued after refused provider/work prefix");++prefixes;
    }
    Fixture item;auto provider=item.Provider();std::size_t allowance=26u,charged{};
    provider.work=[&]{if(charged==allowance)throw std::runtime_error("Independent caller allowance exhausted");++charged;};
    Require(Actual(item,provider)&&Actual(item,provider)&&charged==26u,"Two successful overlaps did not share exact13-read work charges");
    Refuse([&]{Actual(item,provider);});Require(charged==26u,"Refusal reset or overcharged the enclosing overlap work ledger");
    provider.work={};Require(Actual(item,provider),"Optional absent work callback changed the numeric result");
}
void RetainedAndExtreme(){
    Fixture f;auto q=f.Provider();const float tiny=std::numeric_limits<float>::denorm_min();
    f.Actor(source).radius=tiny;f.Actor(other).radius=0.0f;
    Require(Actual(f,q),"Positive subnormal radius squared must stay retained, not Float-underflow to zero");
    f.Actor(source).radius=1.0f;f.Actor(other).radius=0.0f;
    f.Actor(source).location[0u]=1.0f;f.Actor(other).location[0u]=std::ldexp(1.0f,-25);
    Require(Actual(f,q),"Retained X subtraction was incorrectly Float-spilled at the radial gate");
    f=Fixture{};q=f.Provider();f.Actor(other).location[0u]=1.0f;
    f.Actor(other).location[1u]=std::ldexp(1.0f,-12);f.Actor(other).radius=std::ldexp(1.0f,-24);
    Require(Actual(f,q),"Retained radius sum was incorrectly Float-spilled before its square");
    f=Fixture{};q=f.Provider();f.Actor(source).height=tiny;f.Actor(other).height=0.0f;
    Require(Actual(f,q),"Positive subnormal height squared must stay retained");
    f.Actor(source).height=1.0f;f.Actor(source).location[2u]=1.0f;
    f.Actor(other).location[2u]=std::ldexp(1.0f,-25);
    Require(Actual(f,q),"Retained Z subtraction was incorrectly Float-spilled at the cap gate");
    f.Actor(source).location[2u]=0.0f;f.Actor(other).location[2u]=1.0f;
    f.Actor(other).height=std::ldexp(1.0f,-24);
    Require(Actual(f,q),"Retained height sum was incorrectly Float-spilled before its square");
    f=Fixture{};q=f.Provider();const float maximum=std::numeric_limits<float>::max();
    f.Actor(source).radius=f.Actor(other).radius=maximum;
    f.Actor(source).height=f.Actor(other).height=maximum;
    f.Actor(other).location={maximum,0.0f,maximum};
    Require(Actual(f,q),"Extreme finite retained sums/squares should not be refused as Float overflow");
    f.Actor(source).location[0u]=-maximum;
    Require(!Actual(f,q),"Extreme finite retained equality must remain strict clear");
    f.Actor(source).radius=f.Actor(other).radius=-maximum;
    Require(!Actual(f,q),"Negative extreme dimension sum has identical squared tangency");
    f.Actor(source).location[0u]=0.0f;f.Actor(source).height=f.Actor(other).height=-maximum;
    Require(Actual(f,q),"Finite negative extreme dimensions were normalized or rejected");
}
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Oracle {
using Real=long double;
struct Precision {
    unsigned short saved{};
    Precision(){static_assert(std::numeric_limits<Real>::digits==64,"Overlap oracle requires actual hardware x87 PC64");
        asm volatile("fnstcw %0":"=m"(saved));const auto mode=static_cast<unsigned short>((saved&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));}
    ~Precision(){asm volatile("fldcw %0"::"m"(saved));}
};
Real Read(float value){if(!std::isfinite(value))throw std::runtime_error("Overlap x87 active Float nonfinite");return static_cast<Real>(value);}
Real Add(Real a,Real b){volatile Real x=a,y=b,result=x+y;return result;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,result=x-y;return result;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,result=x*y;return result;}
bool Query(const Body& a,const Body& b){
    const auto dx=Sub(Read(a.location[0u]),Read(b.location[0u]));
    const auto dy=Sub(Read(a.location[1u]),Read(b.location[1u]));
    const auto radius=Add(Read(b.radius),Read(a.radius));
    const auto dySquared=Mul(dy,dy),dxSquared=Mul(dx,dx);
    const auto distanceSquared=Add(dySquared,dxSquared);
    const auto radiusSquared=Mul(radius,radius);
    if(!(radiusSquared>distanceSquared))return false;
    const auto dz=Sub(Read(a.location[2u]),Read(b.location[2u]));
    const auto height=Add(Read(b.height),Read(a.height));
    const auto dzSquared=Mul(dz,dz),heightSquared=Mul(height,height);
    return heightSquared>dzSquared;
}
} // namespace Oracle
std::uint64_t Next(std::uint64_t& state){state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
float Wide(std::uint64_t& state){auto word=static_cast<std::uint32_t>(Next(state));
    if((word&0x7f800000u)==0x7f800000u)word^=0x00800000u;
    return std::bit_cast<float>(word);
}
void Compare(Fixture& f,const Queries& q){const bool expected=Oracle::Query(f.Actor(source),f.Actor(other));
    const bool actual=Actual(f,q);Require(actual==expected,"Original overlap differs from independent hardware-x87 protocol");
    ++differentials;if(actual)++overlaps;else ++clear;
}
void HardwareDifferential(){
    Oracle::Precision precision;Fixture f;const auto q=f.Provider();
    constexpr std::array values{-4.0f,-2.0f,-1.0f,-0.0f,0.0f,.5f,1.0f,2.0f,4.0f};
    for(const float radiusA:values)for(const float radiusB:values)for(const float dx:values)for(const float dz:values){
        f.Actor(source).radius=radiusA;f.Actor(other).radius=radiusB;
        f.Actor(source).height=radiusB;f.Actor(other).height=radiusA;
        f.Actor(other).location={dx,.5f,dz};Compare(f,q);
    }
    std::uint64_t state=0xa37e0c82d96b4f15ull;
    for(unsigned i=0u;i<100000u;++i){
        for(const Id id:{source,other}){auto& body=f.Actor(id);
            body.location={Wide(state),Wide(state),Wide(state)};body.radius=Wide(state);body.height=Wide(state);}
        Compare(f,q);++wideCases;
        // Translated/coincident cohorts force the cap branch even with very
        // wide signed coordinates and dimensions; random independent XY often
        // returns early. Their native equality cancellation is not approximated.
        if((i%4u)==0u){f.Actor(other).location=f.Actor(source).location;
            Compare(f,q);++wideCases;}
    }
    Require(overlaps&&clear,"Independent overlap numeric cohort lacks both strict outcomes");
}
#else
void HardwareDifferential(){std::cout<<"NOTE: hardware-x87 differential unavailable on this target; deterministic/provider controls remain.\n";}
#endif
} // namespace
int main(){try{GoldenAndProtocol();ActiveInputsAndWork();RetainedAndExtreme();HardwareDifferential();
    std::cout<<"PASS original Actor.IsOverlapping "<<checks<<" checks / "<<refusals<<" explicit refusals; "
        <<transcripts<<" lazy transcripts, "<<prefixes<<" active failure prefixes; "<<differentials
        <<" independent hardware-x87 comparisons ("<<wideCases<<" wide finite), "<<overlaps<<" overlap / "<<clear
        <<" clear. No Move, touching or first-Training gameplay completion claim.\n";return 0;}
    catch(const std::exception& error){std::cerr<<"Original Actor.IsOverlapping failed: "<<error.what()<<'\n';return 1;}}
