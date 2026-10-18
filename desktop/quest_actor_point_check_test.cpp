#include "quest_actor_point_check.h"

#include <iostream>
#include <limits>

namespace {
using Vec=PortableModelVec3;
using Hit=QuestVr::OriginalActorLineHit;
using Cylinder=QuestVr::OriginalActorCylinder;
std::size_t checks{},refusals{},blocked{},clear{},cap{},radial{},zeroNormal{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(float a,float b) {return Bits(a)==Bits(b);}
bool Same(const Vec& a,const Vec& b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Hit& a,const Hit& b) {
    return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&a.item==b.item&&
        Same(a.time,b.time)&&Same(a.location,b.location)&&Same(a.normal,b.normal);
}
Hit Seed() {return {0x1234u,0x5678u,0xabcdu,{71,72,73},{-4,5,-6},-9.0f,99};}
void UnchangedInactive(const Hit& hit,const Hit& before) {
    Require(hit.next==before.next&&hit.primitive==before.primitive&&hit.item==before.item&&Same(hit.time,before.time),
        "Point hit changed inactive Next, Primitive, Item or Time");
}
template<class F> void Refuse(F run) {
    bool failed{};try {run();}catch(const std::exception&) {failed=true;}
    Require(failed,"Invalid active point geometry silently succeeded");++refusals;
}

void Boundaries() {
    const auto seed=Seed();auto hit=seed;
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    const Cylinder owner{17,{10,20,30},2,3};
    Require(QuestVr::CheckOriginalActorCylinderPoint(hit,std::nullopt,{nan,inf,-inf},{nan,nan,nan},~0u)&&Same(hit,seed),
        "Null owner read inactive geometry/flags or changed packet");
    auto invalid=owner;invalid.identity=0u;
    Refuse([&]{QuestVr::CheckOriginalActorCylinderPoint(hit,invalid,{},{});});
    Require(Same(hit,seed),"Missing identity refusal changed packet");
    invalid=owner;invalid.location.x=nan;invalid.location.y=nan;invalid.radius=nan;
    Require(QuestVr::CheckOriginalActorCylinderPoint(hit,invalid,{nan,nan,34},{nan,nan,0})&&Same(hit,seed),
        "Z-clear query read inactive XY/radius or changed packet");
    Require(QuestVr::CheckOriginalActorCylinderPoint(hit,owner,{10,20,33})&&Same(hit,seed),
        "Exact upper-cap contact must be clear/untouched");
    Require(QuestVr::CheckOriginalActorCylinderPoint(hit,owner,{10,20,27})&&Same(hit,seed),
        "Exact lower-cap contact must be clear/untouched");
    Require(QuestVr::CheckOriginalActorCylinderPoint(hit,owner,{12,20,30},{0,nan,0})&&Same(hit,seed),
        "Exact radial contact must be clear/untouched and Extent.y inactive");
    Require(QuestVr::CheckOriginalActorCylinderPoint(hit,owner,{10,22,30},{0,10000,0})&&Same(hit,seed),
        "Extent.y incorrectly inflates radial overlap");
    Require(!QuestVr::CheckOriginalActorCylinderPoint(hit,owner,{11,20,30},{1,nan,0},~0u),
        "Finite X extent overlap incorrectly cleared");
    Require(hit.actor==17u&&Same(hit.normal,Vec{1,0,0})&&Same(hit.location,Vec{9,20,60}),
        "Radial source output must preserve ownerZ+queryZ, not a repaired surface projection");
    UnchangedInactive(hit,seed);
    for(const float z:{28.0f,32.0f}) {
        hit=seed;
        Require(!QuestVr::CheckOriginalActorCylinderPoint(hit,owner,{10,20,z},{.5f,nan,.75f}),
            "Cap interior incorrectly cleared");
        Require(Same(hit.location,Vec{10,20,29.25f})&&Same(hit.normal,Vec{0,0,z<30?-1.0f:1.0f}),
            "Both original cap branches must subtract Extent.z");
        UnchangedInactive(hit,seed);
    }
    hit=seed;
    Require(!QuestVr::CheckOriginalActorCylinderPoint(hit,owner,owner.location)&&
        Same(hit.normal,Vec{})&&Same(hit.location,Vec{10,20,60}),
        "Centered query lost zero SafeNormal or doubled source Z output");
    hit=seed;
    Require(!QuestVr::CheckOriginalActorCylinderPoint(hit,owner,{11,21,30.5f},{.5f,nan,.25f}),
        "Angled radial interior incorrectly cleared");
    Require(hit.normal.z>0&&hit.normal.z<.5f&&hit.location.z==60.5f,
        "Radial temporary SafeNormal overwrote retained full 3D Hit.Normal");
    UnchangedInactive(hit,seed);
    // All non-geometric caller fields can contain arbitrary bits. Neither the
    // success path nor the all-clear path treats Time as a bound or resets it.
    hit=seed;hit.time=nan;hit.normal={nan,inf,-inf};hit.location={inf,nan,-inf};
    const auto arbitrary=hit;
    Require(QuestVr::CheckOriginalActorCylinderPoint(hit,owner,{13,20,30},{0,nan,0})&&Same(hit,arbitrary),
        "Clear path inspected or changed arbitrary packet fields");
    Require(!QuestVr::CheckOriginalActorCylinderPoint(hit,owner,{11,20,30},{0,nan,0})&&Same(hit.time,nan),
        "Blocked path inspected inactive seeded Time or unused Y extent");
    UnchangedInactive(hit,arbitrary);
    // Portable safety policy is active-read-only, not native NaN emulation or
    // a Mesh zero-or-NaN wrapper. PointCheck has no such Mesh wrapper at all.
    for(const unsigned field:{0u,1u,2u,3u}) {
        invalid=owner;Vec extent{};
        if(field==0u) invalid.height=-1;
        else if(field==1u) invalid.radius=-1;
        else if(field==2u) extent.z=-1;
        else extent.x=-1;
        hit=seed;Refuse([&]{QuestVr::CheckOriginalActorCylinderPoint(hit,invalid,owner.location,extent);});
        Require(Same(hit,seed),"Negative active dimension refusal changed packet");
    }
    for(const unsigned field:{0u,1u,2u,3u,4u,5u,6u,7u}) {
        invalid=owner;Vec point=owner.location,extent{};
        if(field==0u) point.z=nan;else if(field==1u) invalid.location.z=inf;
        else if(field==2u) extent.z=nan;else if(field==3u) invalid.height=inf;
        else if(field==4u) point.x=nan;else if(field==5u) invalid.location.y=inf;
        else if(field==6u) extent.x=nan;else invalid.radius=inf;
        hit=seed;Refuse([&]{QuestVr::CheckOriginalActorCylinderPoint(hit,invalid,point,extent);});
        Require(Same(hit,seed),"Nonfinite active input refusal changed packet");
    }
    const auto huge=std::numeric_limits<float>::max();
    invalid={21,{nan,nan,-huge},nan,huge};hit=seed;
    Require(QuestVr::CheckOriginalActorCylinderPoint(hit,invalid,{nan,nan,huge},{nan,nan,huge})&&Same(hit,seed),
        "Unspilled height sum was incorrectly rejected at Float range");
    invalid={21,{huge,0,0},huge,1};hit=seed;
    Require(QuestVr::CheckOriginalActorCylinderPoint(hit,invalid,{-huge,0,0},{huge,nan,0})&&Same(hit,seed),
        "Unspilled radial sum was incorrectly rejected at Float range");
    invalid={21,{huge,0,0},huge,1};hit=seed;
    Refuse([&]{QuestVr::CheckOriginalActorCylinderPoint(hit,invalid,{-huge*.5f,0,0},{huge,nan,0});});
    Require(Same(hit,seed),"Late normal Float overflow leaked Actor or partial packet writes");
    invalid={21,{0,0,huge},1,huge};hit=seed;
    Refuse([&]{QuestVr::CheckOriginalActorCylinderPoint(hit,invalid,{0,0,huge});});
    Require(Same(hit,seed),"Late radial output overflow leaked partial packet writes");
}

void IntegerOverlapOracle() {
    // Independent exact integer classification: these small integer squares
    // have no rounding at any native PC64 operation. This checks branch and
    // strict-contact semantics without the floating hardware oracle or any
    // production arithmetic helper. Deliberately vary the ignored Y extent.
    for(std::int64_t radius=0;radius<=3;++radius) for(std::int64_t height=0;height<=3;++height)
        for(std::int64_t x=-4;x<=4;++x) for(std::int64_t y=-4;y<=4;++y) for(std::int64_t z=-4;z<=4;++z)
            for(std::int64_t extentX=0;extentX<=1;++extentX) for(std::int64_t extentZ=0;extentZ<=1;++extentZ) {
                const auto expandedHeight=height+extentZ,expandedRadius=radius+extentX;
                const bool expected=z*z>=expandedHeight*expandedHeight||x*x+y*y>=expandedRadius*expandedRadius;
                const Cylinder owner{23,{5,-7,11},static_cast<float>(radius),static_cast<float>(height)};
                const Vec point{static_cast<float>(5+x),static_cast<float>(-7+y),static_cast<float>(11+z)};
                const Vec extent{static_cast<float>(extentX),-std::numeric_limits<float>::infinity(),
                    static_cast<float>(extentZ)};
                const auto before=Seed();auto hit=before;
                Require(QuestVr::CheckOriginalActorCylinderPoint(hit,owner,point,extent)==expected,
                    "Point classification differs from independent exact integer overlap oracle");
                if(expected) Require(Same(hit,before),"Integer clear/contact query changed caller packet");
                else {Require(hit.actor==23u,"Integer overlap query lost real owner identity");UnchangedInactive(hit,before);}
            }
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
// Independent test-only hardware x87 oracle. It calls no production math,
// dyadic, cylinder, trace or coordinate helper and never executes retail code.
namespace Oracle {
using Real=long double;
struct Precision {
    unsigned short original{};
    Precision() {
        static_assert(std::numeric_limits<Real>::digits==64,"Oracle needs actual x87 precision");
        asm volatile("fnstcw %0":"=m"(original));
        const auto mode=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fldcw %0"::"m"(original));}
    Precision(const Precision&)=delete;
    Precision& operator=(const Precision&)=delete;
};
Real Load(float value) {
    if(!std::isfinite(value)) throw std::runtime_error("Oracle active Float nonfinite");
    return value;
}
Real Dimension(float value) {
    const auto result=Load(value);
    if(value<0) throw std::runtime_error("Oracle negative active dimension");
    return result;
}
Real Add(Real a,Real b) {volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b) {volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b) {volatile Real x=a,y=b,z=x*y;return z;}
Real Divide(Real a,Real b) {
    if(b==0) throw std::runtime_error("Oracle active division by zero");
    volatile Real x=a,y=b,z=x/y;return z;
}
float Store(Real value) {
    volatile float stored=static_cast<float>(value);
    if(!std::isfinite(stored)) throw std::runtime_error("Oracle Float overflow");
    return stored;
}
double Spill(Real value) {
    volatile double stored=static_cast<double>(value);
    if(!std::isfinite(stored)) throw std::runtime_error("Oracle Double overflow");
    return stored;
}
Real Root(double value) {
    Real result;
    asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(result):"m"(value):"st");
    return result;
}
Vec SafeNormal(Vec v) {
    const auto square=Add(Add(Mul(Load(v.x),Load(v.x)),Mul(Load(v.y),Load(v.y))),Mul(Load(v.z),Load(v.z)));
    const auto saved=Spill(square);
    constexpr double threshold=std::bit_cast<double>(std::uint64_t{0x3e45798ee2308c3aull});
    if(square<threshold) return {};
    const auto scale=Divide(1.0,Root(saved));
    return {Store(Mul(v.x,scale)),Store(Mul(v.y,scale)),Store(Mul(v.z,scale))};
}
bool Check(Hit& hit,const Cylinder& owner,const Vec& point,const Vec& extent) {
    if(!owner.identity) throw std::runtime_error("Oracle owner lacks identity");
    const auto zDifference=Sub(Load(owner.location.z),Load(point.z));
    const auto zDimension=Add(Dimension(extent.z),Dimension(owner.height));
    if(Mul(zDimension,zDimension)<=Mul(zDifference,zDifference)) return true;
    const auto xDifference=Sub(Load(owner.location.x),Load(point.x));
    const auto yDifference=Sub(Load(owner.location.y),Load(point.y));
    const auto xyDimension=Add(Dimension(extent.x),Dimension(owner.radius));
    if(Mul(xyDimension,xyDimension)<=Add(Mul(yDifference,yDifference),Mul(xDifference,xDifference))) return true;
    hit.actor=owner.identity;
    const auto z=Sub(Load(point.z),Load(owner.location.z));
    const auto y=Sub(Load(point.y),Load(owner.location.y));
    const auto x=Sub(Load(point.x),Load(owner.location.x));
    hit.normal=SafeNormal({Store(x),Store(y),Store(z)});
    if(hit.normal.z<-.5f||hit.normal.z>.5f) {
        hit.location={point.x,point.y,Store(Sub(Load(owner.location.z),Dimension(extent.z)))};
        return false;
    }
    const auto n=SafeNormal({hit.normal.x,hit.normal.y,0.0f});
    const auto px=Mul(Dimension(extent.x),Load(n.x));
    const auto py=Store(Mul(Dimension(extent.x),Load(n.y)));
    const auto pz=Store(Mul(Dimension(extent.x),Load(n.z)));
    const auto outputY=Store(Sub(Load(owner.location.y),Load(py)));
    const auto outputZ=Store(Sub(Load(owner.location.z),Load(pz)));
    const auto outputX=Store(Sub(Load(owner.location.x),px));
    hit.location={outputX,outputY,Store(Add(Load(outputZ),Load(point.z)))};
    return false;
}
} // namespace Oracle

std::uint64_t Next(std::uint64_t& value) {value^=value<<13u;value^=value>>7u;value^=value<<17u;return value;}
float Number(std::uint64_t& seed,bool broad) {
    const auto raw=static_cast<std::uint32_t>(Next(seed));
    return broad?std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u)):
        static_cast<float>(static_cast<int>(raw%65537u)-32768)/256.0f;
}
void DifferentialOne(const Cylinder& owner,const Vec& point,const Vec& extent) {
    const auto seed=Seed();auto actual=seed,expected=seed;
    bool failed{},oracleFailed{},a{},b{};
    try {a=QuestVr::CheckOriginalActorCylinderPoint(actual,owner,point,extent,~0u);}
    catch(const std::exception&) {failed=true;}
    try {b=Oracle::Check(expected,owner,point,extent);}
    catch(const std::exception&) {oracleFailed=true;}
    Require(failed==oracleFailed,"Point finite safety refusal differs from independent hardware x87");
    if(failed) {Require(Same(actual,seed),"Point refusal leaked caller writes");++refusals;return;}
    Require(a==b,"Point clear/blocked branch differs from independent hardware x87");
    if(!Same(actual,expected)) throw std::runtime_error("Point packet differs from independent x87: location "+
        std::to_string(Bits(actual.location.x))+","+std::to_string(Bits(actual.location.y))+","+std::to_string(Bits(actual.location.z))+
        " / "+std::to_string(Bits(expected.location.x))+","+std::to_string(Bits(expected.location.y))+","+
        std::to_string(Bits(expected.location.z))+" normal "+std::to_string(Bits(actual.normal.x))+","+
        std::to_string(Bits(actual.normal.y))+","+std::to_string(Bits(actual.normal.z))+" / "+
        std::to_string(Bits(expected.normal.x))+","+std::to_string(Bits(expected.normal.y))+","+
        std::to_string(Bits(expected.normal.z)));
    ++checks;
    if(a) ++clear;
    else {++blocked;if(actual.normal.z<-.5f||actual.normal.z>.5f) ++cap;else ++radial;
        if(actual.normal.x==0&&actual.normal.y==0&&actual.normal.z==0) ++zeroNormal;}
}
void Differential() {
    Oracle::Precision precision;
    std::uint64_t seed=0x4c71c2ab8925ee73ull;
    for(unsigned i=0;i<32'000u;++i) {
        const bool broad=i<8'000u;const auto number=[&]{return Number(seed,broad);};
        const Cylinder owner{17,{number(),number(),number()},std::fabs(number()),std::fabs(number())};
        Vec point{number(),number(),number()};
        if(i%4u==0u) point=owner.location;
        const Vec extent=i%5u?Vec{0,std::numeric_limits<float>::quiet_NaN(),0}:
            Vec{std::fabs(number()),std::numeric_limits<float>::quiet_NaN(),std::fabs(number())};
        DifferentialOne(owner,point,extent);
    }
    const std::array<float,15u> edges{-5,-3,-2,std::nextafter(-2.0f,0.0f),-.0001f,-0.0f,0,
        std::numeric_limits<float>::denorm_min(),.0001f,std::nextafter(2.0f,0.0f),2,
        std::nextafter(2.0f,3.0f),3,4,5};
    for(float x:edges) for(float y:edges) for(float z:edges) {
        DifferentialOne({17,{0,0,0},2,3},{x,y,z},{});
        DifferentialOne({19,{3,-7,9},2,3},{3+x,-7+y,9+z},{.125f,-std::numeric_limits<float>::infinity(),.5f});
    }
    // Adjacent first SafeNormal threshold and cap branch values. Products,
    // sums, normalization and output spills all remain independently checked.
    for(const float value:{0.0f,-0.0f,std::numeric_limits<float>::denorm_min(),.0001f,
        std::nextafter(.0001f,0.0f),std::nextafter(.0001f,1.0f),.5f,
        std::nextafter(.5f,0.0f),std::nextafter(.5f,1.0f),1.0f,std::nextafter(1.0f,0.0f)}) {
        DifferentialOne({17,{0,0,0},2,3},{value,0,0},{1,0,1});
        DifferentialOne({17,{0,0,0},2,3},{0,0,value},{1,0,1});
        DifferentialOne({17,{0,0,0},2,3},{value,value,value},{1,0,1});
    }
    Require(blocked>5'000u&&clear>10'000u&&cap>100u&&radial>1'000u&&zeroNormal>1'000u,
        "Point differential cases missed clear, cap, radial or zero-normal paths");
}
#else
void Differential() {std::cout<<"SKIP independent hardware-x87 oracle on non-x86 compiler; deterministic controls still run.\n";}
#endif
} // namespace

int main() {
    try {Boundaries();IntegerOverlapOracle();Differential();std::cout<<"PASS original actor point check "<<checks<<" checks, "<<refusals
        <<" safety refusals, "<<blocked<<" blocked ("<<cap<<" cap/"<<radial<<" radial/"<<zeroNormal
        <<" zero-normal), "<<clear<<" clear.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Original actor point check failed: "<<error.what()<<'\n';return 1;}
}
