#include "quest_brush_poly_split.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

// Independent detached controls for the selected native fast splitter. No
// original module execution, production arithmetic in the expected path, live
// Model/UPolys writes, tracker publication or headset acceptance is claimed.
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace {
using Vec=PortableModelVec3;
using Plane=QuestVr::OriginalExtentPlane;
using Poly=QuestVr::OriginalBrushClipPolygon;
using Split=QuestVr::OriginalBrushPlaneSplit;
std::size_t checks{},comparisons{},matchingRefusals{},explicitRefusals{},controls{};
void Require(bool good,const std::string& why){++checks;if(!good)throw std::runtime_error(why);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
bool Same(float a,float b){return Bits(a)==Bits(b);}
bool Same(Vec a,Vec b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool SameStored(const Poly& a,const Poly& b){
    if(a.vertexCount!=b.vertexCount||a.polyFlags!=b.polyFlags||!Same(a.base,b.base)||!Same(a.normal,b.normal))return false;
    for(std::size_t i=0;i<a.vertices.size();++i)if(!Same(a.vertices[i],b.vertices[i]))return false;
    return true;
}
bool SameKnownFragment(const Poly& a,const Poly& b){
    if(a.vertexCount!=b.vertexCount||a.vertexCount<0||a.vertexCount>16||a.polyFlags!=b.polyFlags||
        !Same(a.base,b.base)||!Same(a.normal,b.normal))return false;
    for(std::int32_t i=0;i<a.vertexCount;++i)if(!Same(a.vertices[static_cast<std::size_t>(i)],b.vertices[static_cast<std::size_t>(i)]))return false;
    // Unused fragment slots have no established native initialization contract.
    return true;
}
bool Same(const Split& a,const Split& b){
    return a.side==b.side&&bool(a.front)==bool(b.front)&&bool(a.back)==bool(b.back)&&
        (!a.front||SameKnownFragment(*a.front,*b.front))&&(!a.back||SameKnownFragment(*a.back,*b.back));
}
bool Same(const std::pair<Poly,Poly>& a,const std::pair<Poly,Poly>& b){return SameStored(a.first,b.first)&&SameStored(a.second,b.second);}
template<class A,class E>void Compare(A actual,E expected,const char* label){
    using Value=decltype(actual());Value a{},e{};bool failedA{},failedE{};
    try{a=actual();}catch(const std::exception&){failedA=true;}
    try{e=expected();}catch(const std::exception&){failedE=true;}
    Require(failedA==failedE,std::string("Fast split numeric/refusal domain differs: ")+label);
    if(failedA){++matchingRefusals;return;}
    Require(Same(a,e),std::string("Independent x87 fast split bits/order differ: ")+label);++comparisons;
}
template<class F>void Refuse(F run,const char* label){bool refused{};try{run();}catch(const std::exception&){refused=true;}
    Require(refused,std::string("Unsupported split data accepted: ")+label);++explicitRefusals;}

namespace Hw {
using Real=long double;
struct Precision {
    unsigned short original{};
    Precision(){static_assert(std::numeric_limits<Real>::digits==64,"Fast split oracle needs actual x87PC64");
        asm volatile("fnstcw %0":"=m"(original));const auto mode=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));}
    ~Precision(){asm volatile("fldcw %0"::"m"(original));}
    Precision(const Precision&)=delete;Precision& operator=(const Precision&)=delete;
};
Real Read(float value){if(!std::isfinite(value))throw std::runtime_error("oracle active Float nonfinite");return value;}
Real Add(Real a,Real b){volatile Real x=a,y=b,result=x+y;return result;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,result=x-y;return result;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,result=x*y;return result;}
Real Div(Real a,Real b){if(b==0.0L)throw std::runtime_error("oracle active zero denominator");
    volatile Real x=a,y=b,result=x/y;return result;}
float F(Real value){float result;asm volatile("fldt %1; fstps %0":"=m"(result):"m"(value):"st");
    if(!std::isfinite(result))throw std::runtime_error("oracle Float output overflow");
    return result;}
void Count(const Poly& value){if(value.vertexCount<0||value.vertexCount>16)throw std::runtime_error("oracle native vertex bound");}
Real Distance(Vec v,Plane p){
    return Sub(Add(Add(Mul(Read(p.z),Read(v.z)),Mul(Read(p.x),Read(v.x))),Mul(Read(p.y),Read(v.y))),Read(p.w));
}
Vec Intersection(Vec previous,Vec current,Plane p,bool loseRetainedX=false,bool loseOffsetStores=false){
    const auto dxExtended=Sub(Read(current.x),Read(previous.x));
    const float dx=F(dxExtended),dy=F(Sub(Read(current.y),Read(previous.y))),dz=F(Sub(Read(current.z),Read(previous.z)));
    const auto numerator=Sub(Read(p.w),Add(Add(Mul(Read(p.x),Read(previous.x)),Mul(Read(p.z),Read(previous.z))),
        Mul(Read(p.y),Read(previous.y))));
    const auto denominator=Add(Add(Mul(Read(p.z),Read(dz)),Mul(Read(p.y),Read(dy))),Mul(Read(dx),Read(p.x)));
    const float t=F(Div(numerator,denominator));
    const auto xOffset=Mul(loseRetainedX?Read(dx):dxExtended,Read(t));
    const auto yProduct=Mul(Read(dy),Read(t)),zProduct=Mul(Read(dz),Read(t));
    const auto yOffset=loseOffsetStores?yProduct:Read(F(yProduct));
    const auto zOffset=loseOffsetStores?zProduct:Read(F(zProduct));
    return {F(Add(xOffset,Read(previous.x))),F(Add(yOffset,Read(previous.y))),F(Add(zOffset,Read(previous.z)))};
}
void Append(Poly& value,Vec vertex){if(value.vertexCount==16)throw std::runtime_error("oracle split output exceeds native storage");
    value.vertices[static_cast<std::size_t>(value.vertexCount++)]=vertex;}
Split SplitPlane(const Poly& source,Plane plane,bool build=true){
    Count(source);Split result;std::array<bool,16u> negative{};bool front{},back{};
    for(std::int32_t i=0;i<source.vertexCount;++i){const auto distance=Distance(source.vertices[static_cast<std::size_t>(i)],plane);
        negative[static_cast<std::size_t>(i)]=distance<0.0L;front=front||distance>0.25L;back=back||distance<-0.25L;}
    result.side=front?(back?3:1):(back?2:0);
    if(result.side!=3||!build)return result;
    Poly a{},b{};
    std::int32_t previous=source.vertexCount-1;
    for(std::int32_t current=0;current<source.vertexCount;++current){
        const auto p=static_cast<std::size_t>(previous),c=static_cast<std::size_t>(current);
        if(negative[p]!=negative[c]){const auto cross=Intersection(source.vertices[p],source.vertices[c],plane);Append(a,cross);Append(b,cross);}
        Append(negative[c]?b:a,source.vertices[c]);previous=current;
    }
    // Only these scalar fields are initialized by the original constructing
    // path after vertices/counts; texture/actor/FName/link/pans do not exist in
    // this selected typed subset and must not be fabricated as native defaults.
    a.base=b.base=source.base;a.normal=b.normal=source.normal;a.polyFlags=b.polyFlags=source.polyFlags;
    result.front=a;result.back=b;return result;
}
std::pair<Poly,Poly> Half(const Poly& source){
    if(source.vertexCount<=3||source.vertexCount>16)throw std::runtime_error("oracle missing original SplitInHalf error provider");
    const auto middle=source.vertexCount/2;auto a=source,b=source;
    a.vertexCount=middle+1;b.vertexCount=source.vertexCount-middle+1;
    for(std::int32_t i=0;i<source.vertexCount-middle;++i)b.vertices[static_cast<std::size_t>(i)]=source.vertices[static_cast<std::size_t>(middle+i)];
    b.vertices[static_cast<std::size_t>(source.vertexCount-middle)]=source.vertices[0];
    a.polyFlags|=0x80000000u;b.polyFlags|=0x80000000u;return {a,b};
}
} // namespace Hw

Poly Fixture(std::int32_t n=4){Poly p;p.base={3,-5,7};p.normal={0,0,1};p.vertexCount=n;p.polyFlags=0x456789abu;
    for(std::size_t i=0;i<p.vertices.size();++i)p.vertices[i]={static_cast<float>(i)+0.125f,-static_cast<float>(i)-0.5f,static_cast<float>(i)*2.0f};
    if(n==4){p.vertices[0]={-2,-3,0};p.vertices[1]={2,-3,0};p.vertices[2]={2,3,0};p.vertices[3]={-2,3,0};}
    return p;}
std::uint64_t Next(std::uint64_t& s){s^=s<<13u;s^=s>>7u;s^=s<<17u;return s;}
float Number(std::uint64_t& s,bool broad){const auto bits=static_cast<std::uint32_t>(Next(s));return broad?
    std::bit_cast<float>((bits&0x807fffffu)|(((bits>>23u)%255u)<<23u)):
    static_cast<float>(static_cast<int>(bits%131073u)-65536)/128.0f;}
Vec RandomVec(std::uint64_t& s,bool broad){return {Number(s,broad),Number(s,broad),Number(s,broad)};}

void ConversionAndHalves(){
    const auto poison=std::bit_cast<float>(0x7fc12345u);
    PortableBrushPoly source;source.base={poison,-0.0f,7};source.normal={0,poison,1};source.vertexCount=16;source.polyFlags=0x7edcba99u;
    source.textureU={poison,poison,poison};source.textureV=source.textureU;source.actorReference=-123;source.textureReference=197;
    source.itemNameReference=2;source.link=-1;source.brushPoly=std::numeric_limits<std::int32_t>::min();source.panU=-32768;source.panV=32767;
    for(std::size_t i=0;i<source.vertices.size();++i)source.vertices[i]={static_cast<float>(i),poison,-0.0f};
    const auto converted=QuestVr::MakeOriginalBrushClipPolygon(source);
    Require(converted.vertexCount==source.vertexCount&&converted.polyFlags==source.polyFlags&&Same(converted.base,source.base)&&Same(converted.normal,source.normal),
        "Typed clip conversion interpreted/canonicalized copied scalar fields");
    for(std::size_t i=0;i<16u;++i)Require(Same(converted.vertices[i],source.vertices[i]),"Typed clip conversion lost portable vertex storage bits");
    for(std::int32_t n=4;n<=16;++n){auto p=Fixture(n);p.base={poison,-0.0f,9};p.normal={poison,0,1};
        for(std::size_t i=static_cast<std::size_t>(n);i<16u;++i)p.vertices[i]={poison,-0.0f,static_cast<float>(i)};
        const auto before=p;const auto actual=QuestVr::HalveOriginalBrushClipPolygon(p);const auto expected=Hw::Half(p);
        Require(Same(actual,expected)&&SameStored(p,before),"Half splitter lost full selected storage/copy or mutated input");
        const auto m=n/2;Require(actual.first.vertexCount==m+1&&actual.second.vertexCount==n-m+1,
            "Half splitter invented midpoint/counts");
        Require(Same(actual.first.vertices[0],p.vertices[0])&&Same(actual.first.vertices[static_cast<std::size_t>(m)],p.vertices[static_cast<std::size_t>(m)])&&
            Same(actual.second.vertices[0],p.vertices[static_cast<std::size_t>(m)])&&Same(actual.second.vertices[static_cast<std::size_t>(n-m)],p.vertices[0]),
            "Half splitter lost original vertex0/vertexm shared diagonal");
        Require(actual.first.polyFlags==(p.polyFlags|0x80000000u)&&actual.second.polyFlags==(p.polyFlags|0x80000000u),"Half flags not set on both outputs");
    }
    for(const auto n:{std::numeric_limits<std::int32_t>::min(),-1,0,1,2,3,17,std::numeric_limits<std::int32_t>::max()}){
        auto p=Fixture();p.vertexCount=n;Refuse([&]{QuestVr::HalveOriginalBrushClipPolygon(p);},"Half invalid count must request/refuse original error path");}
}

void ClassificationsAndOrder(){const Plane xPlane{1,0,0,0};
    const auto quarter=0.25f;const std::array<float,9u> values{-1.0f,std::nextafter(-quarter,-1.0f),-quarter,std::nextafter(-quarter,0.0f),
        -0.0f,0.0f,std::nextafter(quarter,0.0f),quarter,std::nextafter(quarter,1.0f)};
    for(const auto x:values){auto p=Fixture(1);p.vertices[0]={x,-0.0f,0};
        for(bool build:{false,true})Compare([&]{return QuestVr::SplitOriginalBrushClipPolygon(p,xPlane,build);},[&]{return Hw::SplitPlane(p,xPlane,build);},"Strict quarter threshold neighbors");
        const auto expected=x>quarter?1:x<-quarter?2:0;const auto actual=QuestVr::SplitOriginalBrushClipPolygon(p,xPlane);
        Require(actual.side==expected&&!actual.front&&!actual.back,"Nonsplit path constructed fragments or used inclusive quarter tolerance");}
    auto p=Fixture();const auto before=p;const auto split=QuestVr::SplitOriginalBrushClipPolygon(p,xPlane);
    Require(Same(split,Hw::SplitPlane(p,xPlane))&&SameStored(p,before),"Rectangle edge traversal order or input storage differs");
    Require(split.side==3&&split.front&&split.back&&split.front->vertexCount==4&&split.back->vertexCount==4,"Rectangle did not split into both four-vertex pieces");
    const std::array<Vec,4u> front{{{0,-3,0},{2,-3,0},{2,3,0},{0,3,0}}};
    const std::array<Vec,4u> back{{{-2,-3,0},{0,-3,0},{0,3,0},{-2,3,0}}};
    // Crossings are appended to both fragments at their ORIGINAL edge visits,
    // not rotated to an arbitrary canonical first vertex.
    for(std::size_t i=0;i<4u;++i){Require(Same(split.front->vertices[i],front[i]),"Front fragment first/current edge order changed");
        Require(Same(split.back->vertices[i],back[i]),"Back fragment first/current edge order changed");}
    auto near=Fixture();near.vertices[0]={-1,0,0};near.vertices[1]={-0.125f,1,0};near.vertices[2]={0.125f,2,0};near.vertices[3]={1,3,0};
    const auto nearSplit=QuestVr::SplitOriginalBrushClipPolygon(near,xPlane);
    Require(Same(nearSplit,Hw::SplitPlane(near,xPlane))&&nearSplit.front&&nearSplit.back&&nearSplit.front->vertexCount==4&&nearSplit.back->vertexCount==4,
        "Tolerance classification was substituted for zero-sign edge side bits");
    Require(Same(nearSplit.front->vertices[2],near.vertices[2])&&Same(nearSplit.back->vertices[2],near.vertices[1]),"Near-plane original points were welded/discarded");
    const auto probe=QuestVr::SplitOriginalBrushClipPolygon(p,xPlane,false);
    Require(probe.side==3&&!probe.front&&!probe.back,"Classification-only split constructed output fragments");
    for(std::int32_t n:{0,1,2,3,16}){auto small=Fixture(n);if(n>1){small.vertices[0]={-2,0,0};small.vertices[1]={2,0,0};}
        Compare([&]{return QuestVr::SplitOriginalBrushClipPolygon(small,xPlane);},[&]{return Hw::SplitPlane(small,xPlane);},"Native zero/small/full vertex counts");}
}

void RoundingControls(){const Plane plane{1,0,0,0};
    const auto huge=std::bit_cast<float>(std::uint32_t{(127u+70u)<<23u});auto association=Fixture(1);association.vertices[0]={huge,1,-huge};
    const Plane diagonal{1,1,1,0};const auto correct=Hw::Distance(association.vertices[0],diagonal);
    const auto wrong=Hw::Add(Hw::Add(Hw::Read(huge),1.0L),Hw::Read(-huge));
    Require(correct==1.0L&&wrong==0.0L,"Plane Z/X/Y retained association control ineffective");
    Require(QuestVr::SplitOriginalBrushClipPolygon(association,diagonal).side==1,"Plane distance used XYZ association/early Float stores");++controls;
    auto retained=Fixture(3);retained.vertices[0]={-1,0,0};retained.vertices[1]={-1,4,0};retained.vertices[2]={16777216.0f,0,0};
    const auto cross=Hw::Intersection(retained.vertices[2],retained.vertices[0],plane);
    const auto wrongX=Hw::Intersection(retained.vertices[2],retained.vertices[0],plane,true);
    Require(cross.x==-1.0f&&wrongX.x==0.0f&&!Same(cross,wrongX),"Retained dx versus stored dx control ineffective");
    const auto retainedSplit=QuestVr::SplitOriginalBrushClipPolygon(retained,plane);
    Require(Same(retainedSplit,Hw::SplitPlane(retained,plane))&&retainedSplit.front&&Same(retainedSplit.front->vertices[0],cross),
        "Fast splitter lost retained X delta or welded its nonzero intersection");++controls;
    auto spilled=Fixture(3);spilled.vertices[0]={7,-1,-1};spilled.vertices[1]={7,2,3};spilled.vertices[2]={-3,-16777216.0f,-16777216.0f};
    const auto stored=Hw::Intersection(spilled.vertices[2],spilled.vertices[0],plane);
    const auto noStores=Hw::Intersection(spilled.vertices[2],spilled.vertices[0],plane,false,true);
    Require(stored.y==-11744052.0f&&noStores.y==-11744051.0f&&stored.z==stored.y&&noStores.z==noStores.y,
        "Y/Z offset Float-store discriminator ineffective");
    const auto spilledSplit=QuestVr::SplitOriginalBrushClipPolygon(spilled,plane);
    Require(Same(spilledSplit,Hw::SplitPlane(spilled,plane))&&spilledSplit.front&&Same(spilledSplit.front->vertices[0],stored),
        "Fast splitter fused Y/Z offset and previous additions");++controls;
    Require(Bits(stored.x)!=0u,"Rounded t/no-plane-welding control ineffective");
    Require(Same(spilledSplit.front->vertices[0].x,stored.x),"Fast splitter projected/welded t-rounded X back to plane");++controls;
}

void Differential(){std::uint64_t state=0xad7ec0387b1295f6ull;
    for(unsigned iteration=0;iteration<3600u;++iteration){const bool broad=iteration<900u;auto source=Fixture(static_cast<std::int32_t>(Next(state)%17u));
        source.base=RandomVec(state,broad);source.normal=RandomVec(state,broad);source.polyFlags=static_cast<std::uint32_t>(Next(state));
        for(std::int32_t i=0;i<source.vertexCount;++i)source.vertices[static_cast<std::size_t>(i)]=RandomVec(state,broad);
        const Plane plane{Number(state,broad),Number(state,broad),Number(state,broad),Number(state,broad)};const auto before=source;
        for(bool build:{false,true})Compare([&]{return QuestVr::SplitOriginalBrushClipPolygon(source,plane,build);},[&]{return Hw::SplitPlane(source,plane,build);},"Random finite/subnormal/extreme fast split");
        Require(SameStored(source,before),"Random split mutated source values");
        if(source.vertexCount>3)Compare([&]{return QuestVr::HalveOriginalBrushClipPolygon(source);},[&]{return Hw::Half(source);},"Random half full-storage copy/order");
    }
    const std::array<float,9u> edges{0.0f,-0.0f,std::numeric_limits<float>::denorm_min(),-std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(),-std::numeric_limits<float>::min(),0.25f,std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};
    for(const auto x:edges)for(const auto y:edges){auto p=Fixture(3);p.vertices[0]={x,y,x};p.vertices[1]={y,x,y};p.vertices[2]={-x,-y,x};
        const Plane plane{x,y,1,0};Compare([&]{return QuestVr::SplitOriginalBrushClipPolygon(p,plane);},[&]{return Hw::SplitPlane(p,plane);},"Signedzero/subnormal/extreme split edges");}
}

void Admission(){const Plane plane{1,0,0,0};const auto poison=std::bit_cast<float>(0x7fc12345u);
    auto empty=Fixture(0);empty.base=empty.normal={poison,poison,poison};for(auto& v:empty.vertices)v={poison,poison,poison};
    const auto noReads=QuestVr::SplitOriginalBrushClipPolygon(empty,{poison,poison,poison,poison});
    Require(noReads.side==0&&!noReads.front&&!noReads.back,"Empty polygon consumed plane/vertices/copied metadata");
    for(const auto bad:{poison,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        auto p=Fixture();p.base={bad,bad,bad};p.normal=p.base;
        Compare([&]{return QuestVr::SplitOriginalBrushClipPolygon(p,plane);},[&]{return Hw::SplitPlane(p,plane);},"Nonfinite metadata bit-copy, not numeric read");
        p=Fixture();p.vertices[15]={bad,bad,bad};Compare([&]{return QuestVr::SplitOriginalBrushClipPolygon(p,plane);},[&]{return Hw::SplitPlane(p,plane);},"Inactive vertex bits ignored");
        p=Fixture();p.vertices[0].z=bad;Refuse([&]{QuestVr::SplitOriginalBrushClipPolygon(p,plane);},"Active zero-plane-coefficient vertex still consumed");
        for(unsigned at=0;at<4u;++at){Plane invalid=plane;std::array<float*,4u> fields{&invalid.x,&invalid.y,&invalid.z,&invalid.w};*fields[at]=bad;
            Refuse([&]{QuestVr::SplitOriginalBrushClipPolygon(Fixture(),invalid);},"Active plane component");}
    }
    for(const auto n:{-1,17,std::numeric_limits<std::int32_t>::max()}){auto p=Fixture();p.vertexCount=n;
        Refuse([&]{QuestVr::SplitOriginalBrushClipPolygon(p,plane);},"Native source vertex bound");
        PortableBrushPoly raw;raw.vertexCount=n;Refuse([&]{QuestVr::MakeOriginalBrushClipPolygon(raw);},"Typed source clip conversion count bound");}
    auto overflowing=Fixture(16);for(std::size_t i=0;i<16u;++i)overflowing.vertices[i]={(i&1u)?1.0f:-1.0f,static_cast<float>(i),0};
    Refuse([&]{QuestVr::SplitOriginalBrushClipPolygon(overflowing,plane);},"Nonconvex split exceeds fixed output capacity");
    const auto classifyOnly=QuestVr::SplitOriginalBrushClipPolygon(overflowing,plane,false);
    Require(classifyOnly.side==3&&!classifyOnly.front&&!classifyOnly.back,"Classification-only split invented fragment capacity refusal");
}
} // namespace
#endif

int main(){
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    try{Hw::Precision precision;ConversionAndHalves();ClassificationsAndOrder();RoundingControls();Differential();Admission();
        std::cout<<"PASS original brush fast split/half "<<checks<<" checks; "<<comparisons<<" independent x87 comparisons, "<<matchingRefusals
            <<" matching numeric/capacity refusals, "<<explicitRefusals<<" explicit refusals, "<<controls<<" effective rounding controls.\n"
            <<"Selected typed clip subset only; no native unspecified-field initialization, FilterPoly/BSP publication/startup/headset proof.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL original brush split: "<<error.what()<<'\n';return 1;}
#else
    std::cout<<"SKIP independent brush split oracle: GCC x86 x87PC64 hardware required; no host-math substitute.\n";return 77;
#endif
}
