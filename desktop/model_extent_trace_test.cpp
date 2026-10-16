#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "quest_model_extent_trace.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Vec=PortableModelVec3;
using Model=PortableModelGeometry;
using Node=PortableModelNode;
using Limits=QuestVr::ModelExtentTraceLimits;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& reason) {++checks;if(!value) throw std::runtime_error(reason);}
void Near(float actual,float expected,const std::string& reason,float tolerance=0.00002f) {
    Require(std::isfinite(actual)&&std::isfinite(expected)&&std::fabs(actual-expected)<=tolerance,reason);
}
void Near(Vec actual,Vec expected,const std::string& reason,float tolerance=0.00002f) {
    Near(actual.x,expected.x,reason+" X",tolerance);Near(actual.y,expected.y,reason+" Y",tolerance);
    Near(actual.z,expected.z,reason+" Z",tolerance);
}
bool Bits(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
void Same(Vec actual,Vec expected,const std::string& reason) {
    Require(Bits(actual.x,expected.x)&&Bits(actual.y,expected.y)&&Bits(actual.z,expected.z),reason);
}
template<class Operation> void Refuses(Operation operation,const std::string& reason) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,reason);++refusals;
}
void U32(Bytes& bytes,std::uint32_t word) {
    for(unsigned shift=0u;shift<32u;shift+=8u) bytes.push_back(static_cast<std::uint8_t>(word>>shift));
}
void U64(Bytes& bytes,std::uint64_t word) {U32(bytes,static_cast<std::uint32_t>(word));U32(bytes,static_cast<std::uint32_t>(word>>32u));}
void F32(Bytes& bytes,float value) {U32(bytes,std::bit_cast<std::uint32_t>(value));}
void V32(Bytes& bytes,Vec value) {F32(bytes,value.x);F32(bytes,value.y);F32(bytes,value.z);}
void Index(Bytes& bytes,std::int32_t number) {
    auto magnitude=static_cast<std::uint32_t>(number<0?-static_cast<std::int64_t>(number):number);
    auto first=static_cast<std::uint8_t>((magnitude&63u)|(number<0?128u:0u));magnitude>>=6u;
    bytes.push_back(static_cast<std::uint8_t>(first|(magnitude?64u:0u)));
    while(magnitude) {auto next=static_cast<std::uint8_t>(magnitude&127u);magnitude>>=7u;
        bytes.push_back(static_cast<std::uint8_t>(next|(magnitude?128u:0u)));}
}
Node Plane(Vec normal={1,0,0},float w=0.0f,std::uint8_t flags=0u,std::uint8_t vertices=3u) {
    Node n;n.planeX=normal.x;n.planeY=normal.y;n.planeZ=normal.z;n.planeW=w;n.nodeFlags=flags;
    n.back=n.front=n.plane=n.collisionBound=n.renderBound=n.leaf0=n.leaf1=-1;
    n.vertexPool=n.surface=n.zone0=n.zone1=0;n.vertexCount=vertices;return n;
}
std::int32_t Hull(Model& model,const std::vector<std::int32_t>& planes,Vec minimum,Vec maximum) {
    const auto offset=static_cast<std::int32_t>(model.leafHulls.size());
    model.leafHulls.insert(model.leafHulls.end(),planes.begin(),planes.end());model.leafHulls.push_back(-1);
    for(float value:{minimum.x,minimum.y,minimum.z,maximum.x,maximum.y,maximum.z})
        model.leafHulls.push_back(std::bit_cast<std::int32_t>(value));
    return offset;
}

// Independent serialized v68 UModel bodies. Query fixtures pass through the
// real decoder, including compact node references and packed int32 LeafHulls;
// no renderer triangles or spawn-overlap helper supplies collision authority.
Bytes Encode(const Model& model) {
    Bytes bytes;V32(bytes,{-100000,-100000,-100000});V32(bytes,{100000,100000,100000});bytes.push_back(1u);
    V32(bytes,{});F32(bytes,100000.0f);
    Index(bytes,3);for(Vec v:std::array<Vec,3>{{{1,0,0},{0,1,0},{0,0,1}}}) V32(bytes,v);
    Index(bytes,3);for(Vec v:std::array<Vec,3>{{{0,0,0},{1,0,0},{0,1,0}}}) V32(bytes,v);
    Index(bytes,static_cast<std::int32_t>(model.nodes.size()));
    for(const auto& n:model.nodes) {
        V32(bytes,{n.planeX,n.planeY,n.planeZ});F32(bytes,n.planeW);U64(bytes,n.zoneMask);bytes.push_back(n.nodeFlags);
        for(auto value:{n.vertexPool,n.surface,n.back,n.front,n.plane,n.collisionBound,n.renderBound,n.zone0,n.zone1}) Index(bytes,value);
        bytes.push_back(n.vertexCount);U32(bytes,static_cast<std::uint32_t>(n.leaf0));U32(bytes,static_cast<std::uint32_t>(n.leaf1));
    }
    Index(bytes,1);Index(bytes,0);U32(bytes,0); // Surface: null material, no flags.
    for(auto value:{0,0,1,2,-1,-1}) Index(bytes,value);
    bytes.insert(bytes.end(),4u,0u);Index(bytes,0); // PanU/PanV, null brush.
    Index(bytes,3);for(int i=0;i<3;++i) {Index(bytes,i);Index(bytes,-1);}
    U32(bytes,0);U32(bytes,0);Index(bytes,0); // Shared sides, zone count, null Polys.
    Index(bytes,0);Index(bytes,0);Index(bytes,0); // Light maps/bits/render bounds.
    Index(bytes,static_cast<std::int32_t>(model.leafHulls.size()));
    for(auto word:model.leafHulls) U32(bytes,static_cast<std::uint32_t>(word));
    Index(bytes,0);Index(bytes,0);U32(bytes,static_cast<std::uint32_t>(model.rootOutside));U32(bytes,0);
    return bytes;
}
Model Decoded(const Model& model) {
    const auto bytes=Encode(model);auto decoded=DecodePortableModel68Body(bytes,0u,0u);
    Require(decoded.bodyBytesConsumed==bytes.size()&&decoded.nodes.size()==model.nodes.size()&&
        decoded.leafHulls==model.leafHulls,"Genuine Model fixture did not decode its exact body/hulls");return decoded;
}
Model Box(Vec minimum={-10,-10,-10},Vec maximum={0,10,10}) {
    Model model;model.rootOutside=1;
    model.nodes={Plane({1,0,0},maximum.x),Plane({-1,0,0},-minimum.x),
        Plane({0,1,0},maximum.y),Plane({0,-1,0},-minimum.y),
        Plane({0,0,1},maximum.z),Plane({0,0,-1},-minimum.z)};
    model.nodes[0].collisionBound=Hull(model,{0,1,2,3,4,5},minimum,maximum);return model;
}
auto Trace(const Model& model,Vec start,Vec end,Vec extent={1,1,1},const Limits& limits={}) {
    return QuestVr::TraceOriginalModelExtent(model,start,end,extent,limits);
}

// Scalar reference uses native long-double arithmetic, explicit Float stores,
// a recursive BSP walk and independently assembled plane lists. It never
// calls production integer arithmetic, clipping, hull/bevel or sweep helpers.
using Real=long double;
Real Add(Real a,Real b) {volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b) {volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b) {volatile Real x=a,y=b,z=x*y;return z;}
Real Div(Real a,Real b) {volatile Real x=a,y=b,z=x/y;return z;}
float Store(Real value) {volatile float result=static_cast<float>(value);
    if(!std::isfinite(result)) throw std::runtime_error("Scalar oracle Float overflow");
    return result;}
double Spill(Real value) {volatile double result=static_cast<double>(value);
    if(!std::isfinite(result)) throw std::runtime_error("Scalar oracle Double overflow");
    return result;}
Real Abs(Real value) {return value<0?-value:value;}
struct P {Vec normal;float w;};
P ReadPlane(const Model& model,std::int32_t raw) {
    const auto bits=static_cast<std::uint32_t>(raw);const auto& node=model.nodes.at(bits&~0x40000000u);
    P p{{node.planeX,node.planeY,node.planeZ},node.planeW};
    if(bits&0x40000000u) {p.normal={-p.normal.x,-p.normal.y,-p.normal.z};p.w=-p.w;}return p;
}
std::array<float,3> Components(Vec value) {return {value.x,value.y,value.z};}
Vec Cross(Vec a,Vec b) {
    return {Store(Sub(Mul(a.y,b.z),Mul(a.z,b.y))),Store(Sub(Mul(a.z,b.x),Mul(a.x,b.z))),
        Store(Sub(Mul(a.x,b.y),Mul(a.y,b.x)))};
}
Real Dot(Vec a,Vec b) {return Add(Add(Mul(a.z,b.z),Mul(a.y,b.y)),Mul(a.x,b.x));}
Vec Normalized(Vec v,bool guarded) {
    const auto square=Add(Add(Mul(v.x,v.x),Mul(v.y,v.y)),Mul(v.z,v.z));
    const auto stored=Spill(square);if(guarded&&square<1e-8) return v;
    if(stored<=0) throw std::runtime_error("Scalar unsafe normal is nonfinite");
    const auto inverse=Div(1.0,std::sqrt(static_cast<Real>(stored)));
    return {Store(Mul(v.x,inverse)),Store(Mul(v.y,inverse)),Store(Mul(v.z,inverse))};
}
P Bevel(P first,P second,Vec axis) {
    auto c=Cross(first.normal,second.normal);
    const auto determinant=Add(Add(Mul(c.x,c.x),Mul(c.y,c.y)),Mul(c.z,c.z));
    Vec point{};
    if(determinant<1e-6) c={};
    else {
        // X/Y cross and W products spill separately. Both Z terms remain
        // retained through their sum, as recovered in original intersection.
        const auto a=Cross(c,first.normal),b=Cross(second.normal,c);
        const auto az=Sub(Mul(c.x,first.normal.y),Mul(c.y,first.normal.x));
        const auto bz=Sub(Mul(second.normal.x,c.y),Mul(second.normal.y,c.x));
        const float ax=Store(Mul(a.x,second.w)),ay=Store(Mul(a.y,second.w));
        const float bx=Store(Mul(b.x,first.w)),by=Store(Mul(b.y,first.w));
        const auto sx=Store(Add(ax,bx)),sy=Store(Add(ay,by));
        const auto sz=Add(Mul(az,second.w),Mul(bz,first.w));const auto inverse=Store(Div(1.0,determinant));
        point={Store(Mul(sx,inverse)),Store(Mul(sy,inverse)),Store(Mul(sz,inverse))};c=Normalized(c,true);
    }
    auto normal=Normalized(Cross(axis,c),false);
    if(Dot(normal,first.normal)<0) normal={Store(Mul(normal.x,-1.0f)),Store(Mul(normal.y,-1.0f)),Store(Mul(normal.z,-1.0f))};
    const auto w=Store(Add(Add(Mul(point.y,normal.y),Mul(point.z,normal.z)),Mul(point.x,normal.x)));
    return {normal,w};
}
struct Interval {float enter{-1},exit{2};Vec normal{};};
bool Clip(Interval& interval,P p,Vec start,Vec end,Vec extents) {
    const auto n=p.normal;
    const float e=Store(Add(Add(Abs(Mul(extents.z,n.z)),Abs(Mul(extents.y,n.y))),Abs(Mul(extents.x,n.x))));
    const auto distance=[&](Vec v) {return Store(Sub(Add(Add(Mul(v.z,n.z),Mul(v.y,n.y)),Mul(v.x,n.x)),p.w));};
    const float ds=distance(start),de=distance(end);auto q=Sub(ds,e);
    if(ds>de&&q>=-static_cast<Real>(e)&&q<0) q=0.0L;
    const float denominator=Store(Sub(ds,de));
    // Execute retail's unused quotient too. Its FP status is not observable
    // gameplay; no NaN quotient is fed into the interval in parallel branches.
    const auto t=Div(q,denominator);
    if(static_cast<Real>(denominator)<-1e-5) {if(t<interval.exit) interval.exit=Store(t);}
    else if(static_cast<Real>(denominator)>1e-5) {
        if(t>interval.enter) {interval.enter=Store(t);interval.normal=n;}
    } else if(ds>e&&de>e) return false;
    return interval.enter<interval.exit;
}
std::vector<P> HullPlanes(const Model& model,std::int32_t offset) {
    std::vector<P> result;auto position=static_cast<std::size_t>(offset);
    while(model.leafHulls.at(position)!=-1) result.push_back(ReadPlane(model,model.leafHulls.at(position++)));
    ++position;std::array<float,6> bounds{};for(auto& value:bounds) value=std::bit_cast<float>(model.leafHulls.at(position++));
    result.push_back({{0,0,-1},Store(Sub(0.1,bounds[2]))});result.push_back({{0,0,1},Store(Add(bounds[5],0.1))});
    result.push_back({{-1,0,0},Store(Sub(0.1,bounds[0]))});result.push_back({{1,0,0},Store(Sub(bounds[3],0.1))});
    result.push_back({{0,-1,0},Store(Sub(0.1,bounds[1]))});result.push_back({{0,1,0},Store(Sub(bounds[4],0.1))});
    return result;
}
struct OracleResult {bool clear{true},hit{};float time{2};Vec location{},normal{};std::int32_t node{-1};
    std::size_t nodeVisits{},leafVisits{},hullVisits{};};
OracleResult Oracle(const Model& model,Vec start,Vec end,Vec extents,bool bevels=true) {
    OracleResult result;
    if(model.nodes.empty()) {result.clear=model.rootOutside!=0;result.time=1;return result;}
    const float ey=Store(Mul(extents.y,1.1f)),ez=Store(Mul(extents.z,1.1f));const auto ex=Mul(extents.x,1.1f);
    std::function<void(std::int32_t,std::int32_t,bool)> visit=[&](std::int32_t index,std::int32_t parent,bool outside) {
        if(index==-1) {
            ++result.leafVisits;if(outside) return;
            const auto hull=model.nodes.at(static_cast<std::size_t>(parent)).collisionBound;if(hull==-1) return;
            ++result.hullVisits;Interval interval;interval.exit=result.time;
            const auto planes=HullPlanes(model,hull);const auto count=planes.size()-6u;
            for(const auto& plane:planes) if(!Clip(interval,plane,start,end,extents)) return;
            // Bevel arithmetic is evaluated only after original/bound planes
            // survive, and each additional plane is built immediately before
            // clipping. An earlier rejection must not execute a later unsafe
            // intersection/normalization that retail never reaches.
            if(bevels) for(std::size_t i=0u;i<count;++i) for(std::size_t j=0u;j<i;++j) {
                const auto a=Components(planes[i].normal),b=Components(planes[j].normal);
                for(std::size_t axisIndex=0u;axisIndex<3u;++axisIndex) {
                    if(!((a[axisIndex]<0&&b[axisIndex]>0)||(a[axisIndex]>0&&b[axisIndex]<0))) continue;
                    const Vec axis{axisIndex==0u?1.0f:0.0f,axisIndex==1u?1.0f:0.0f,axisIndex==2u?1.0f:0.0f};
                    if(Dot(Cross(axis,planes[i].normal),Cross(axis,planes[j].normal))>0.001&&
                        !Clip(interval,Bevel(planes[i],planes[j],axis),start,end,extents)) return;
                }
            }
            if(interval.enter>-1&&interval.enter<interval.exit&&interval.exit>0) {
                result.hit=true;result.time=interval.enter;result.normal=interval.normal;result.node=parent;
            }return;
        }
        ++result.nodeVisits;const auto& node=model.nodes.at(static_cast<std::size_t>(index));
        const auto ds=Sub(Add(Add(Mul(node.planeZ,start.z),Mul(node.planeY,start.y)),Mul(node.planeX,start.x)),node.planeW);
        const auto de=Sub(Add(Add(Mul(node.planeX,end.x),Mul(node.planeY,end.y)),Mul(node.planeZ,end.z)),node.planeW);
        const auto radius=Add(Add(Abs(Mul(ez,node.planeZ)),Abs(Mul(ey,node.planeY))),Abs(Mul(ex,node.planeX)));
        const bool csg=node.vertexCount!=0u&&(node.nodeFlags&0x21u)==0u;
        const auto front=[&] {if(!(ds<-radius&&de<-radius)) visit(node.front,index,outside||csg);};
        const auto back=[&] {if(!(ds>radius&&de>radius)) visit(node.back,index,outside&&!csg);};
        if(ds<de) {back();front();} else {front();back();}
    };
    visit(0,0,model.rootOutside!=0);
    if(result.hit) {
        const Vec delta{Store(Sub(end.x,start.x)),Store(Sub(end.y,start.y)),Store(Sub(end.z,start.z))};
        const auto square=Add(Add(Mul(delta.x,delta.x),Mul(delta.y,delta.y)),Mul(delta.z,delta.z));
        const float size=Store(std::sqrt(static_cast<Real>(Spill(square))));
        const auto backoff=std::max(static_cast<Real>(0.1f),Div(0.1f,size));
        result.time=Store(std::clamp(Sub(result.time,backoff),0.0L,1.0L));
        result.location={Store(Add(start.x,Store(Mul(delta.x,result.time)))),
            Store(Add(start.y,Store(Mul(delta.y,result.time)))),Store(Add(start.z,Store(Mul(delta.z,result.time))))};
        result.clear=result.time==1.0f;
    }return result;
}
void CompareResults(const QuestVr::ModelExtentTraceResult& actual,const OracleResult& expected,bool exact=false) {
    Require(actual.clear==expected.clear&&actual.hit==expected.hit&&actual.node==expected.node,"Extent result/parent identity differs from independent scalar oracle");
    Require(actual.stats.nodeVisits==expected.nodeVisits&&actual.stats.leafVisits==expected.leafVisits&&actual.stats.hullVisits==expected.hullVisits,
        "Near-first full-segment BSP traversal differs from recursive scalar oracle");
    if(exact) {
        Require(Bits(actual.time,expected.time),"Extent wrapper Time spill differs from independent x87");
        Same(actual.location,expected.location,"Extent wrapper Location spills differ from independent x87");
        Same(actual.normal,expected.normal,"Literal selected normal differs from independent x87");
    } else {Near(actual.time,expected.time,"Extent wrapper time");Near(actual.location,expected.location,"Extent wrapper location");Near(actual.normal,expected.normal,"Extent normal");}
}
void Compare(const Model& model,Vec start,Vec end,Vec extents,bool exact=false) {
    CompareResults(Trace(model,start,end,extents),Oracle(model,start,end,extents),exact);
}

void Basics() {
    for(int outside:{0,1}) {
        Model empty;empty.rootOutside=outside;const auto result=Trace(Decoded(empty),{3,4,5},{6,7,8});
        Require(result.clear==static_cast<bool>(outside)&&!result.hit&&result.time==1.0f&&result.node==-1,
            "Empty Model changed Hit or failed native RootOutside return");
        Require(result.stats.nodeVisits==0u&&result.stats.leafVisits==0u,"Empty Model entered BSP recursion");
    }
    auto model=Decoded(Box());auto result=Trace(model,{10,0,0},{-20,0,0});
    Require(result.hit&&!result.clear&&result.node==0,"Decoded box crossing missed original solid hull");
    Near(result.time,0.2033333333f,"Native maximum0.1 time backoff was replaced by minimum distance backoff");
    Near(result.location,{3.9f,0,0},"Native full-segment/backed-off box location");
    Same(result.normal,{1,0,0},"Axis max-X normal changed");
    Compare(model,{10,0,0},{-20,0,0},{1,1,1});Compare(model,{-20,0,0},{10,0,0},{1,1,1});
    Compare(model,{10,20,0},{-20,20,0},{1,1,1});Compare(model,{10,0,0},{10,0,0},{1,1,1});
    result=Trace(model,{-5,0,0},{-5,0,0});Require(!result.hit&&result.clear&&result.time==2.0f,
        "Zero-size inside query invented an entering plane or an always-blocked shortcut");
    model.nodes[0].collisionBound=-1;result=Trace(model,{10,0,0},{-20,0,0});
    Require(result.clear&&!result.hit&&result.time==2.0f&&result.node==-1,"Solid leaf without hull invented a hit");
    model=Box();model.nodes[0].collisionBound=Hull(model,{}, {-10,-10,-10},{0,10,10});model=Decoded(model);
    result=Trace(model,{10,0,0},{-20,0,0});Require(result.hit,"Empty original plane list omitted six null-owner bound planes");
    Near(result.location,{3.9f,0,0},"Bounds-only hull max-X shaving");
    result=Trace(model,{-5,0,20},{-5,0,0});Near(result.location,{-5,0,13.1f},"Retail positive-Z expansion was uniformly shaved");
    result=Trace(model,{-5,20,0},{-5,0,0});Near(result.location,{-5,12.9f,0},"Retail positive-Y maximum shaving changed");
    result=Trace(model,{-5,0,-20},{-5,0,0});Near(result.location,{-5,0,-13.1f},"Retail minimum-Z Double0.1 expansion changed");
    model.nodes[0].planeX=0.0f;model.rootOutside=0;
    Compare(model,{10,0,0},{-20,0,0},{0,1,0}); // Zero authored normal/partially zero extent remain legal.
    auto flipped=Box();flipped.nodes[0].planeX=-1;flipped.nodes[0].planeW=-0.0f;
    flipped.nodes[0].nodeFlags=1;flipped.rootOutside=0;
    flipped.nodes[0].collisionBound=Hull(flipped,{0x40000000,1,2,3,4,5},{-10,-10,-10},{0,10,10});
    flipped=Decoded(flipped);Compare(flipped,{10,0,0},{-20,0,0},{1,1,1});
    auto nonunit=Decoded(Box());nonunit.nodes[0].planeX=2;Compare(nonunit,{10,0,0},{-20,0,0},{1,1,1});
    Require(Trace(nonunit,{10,0,0},{-20,0,0}).hit,"Authored nonunit plane was rejected or silently normalized");
}

void CsgAndTraversal() {
    for(unsigned flags=0u;flags<256u;++flags) for(unsigned vertices:{0u,3u}) for(int outside:{0,1}) {
        auto model=Box();model.nodes[0].nodeFlags=static_cast<std::uint8_t>(flags);
        model.nodes[0].vertexCount=static_cast<std::uint8_t>(vertices);model.rootOutside=outside;
        Compare(model,{10,0,0},{-20,0,0},{1,1,1});
    }
    // Both halves are visited on a crossing; each solid leaf uses its LAST
    // parent, not root0, polygon/surface indexes or a cached nearest bound.
    auto model=Box();model.nodes.insert(model.nodes.begin(),Plane({0,1,0},0,1,0));
    for(auto& word:model.leafHulls) {if(word==-1) break;++word;}
    model.nodes[1].collisionBound=0;model.nodes[0].front=1;model.nodes[0].back=7;
    model.nodes.push_back(Plane({-1,0,0},-5));
    model.nodes[7].collisionBound=Hull(model,{7}, {5,-10,-10},{15,10,10});
    model=Decoded(model);
    Compare(model,{20,-2,0},{-20,2,0},{1,3,1});Compare(model,{-20,2,0},{20,-2,0},{1,3,1});
    Compare(model,{20,0,0},{-20,0,0},{1,1,1}); // dStart==dEnd: Front first.
    Compare(model,{20,-1.1f,0},{-20,-1.1f,0},{1,1,1}); // padded equality visits both.
    auto deep=Box();const auto hull=deep.nodes[0].collisionBound;
    deep.nodes.insert(deep.nodes.begin(),4096u,Plane({1,0,0},0,1,0));
    for(auto& word:deep.leafHulls) {if(word==-1) break;word+=4096;}
    for(std::size_t i=0u;i<4096u;++i) deep.nodes[i].front=static_cast<std::int32_t>(i+1u);
    deep.nodes[4096].collisionBound=hull;
    const auto result=Trace(Decoded(deep),{20,0,0},{10,0,0});
    Require(result.clear&&!result.hit&&result.stats.nodeVisits==4097u,"Iterative extent traversal lost deep front chain or recursed on host stack");
}

void SlantedAndBevels() {
    // A convex diamond in XY. Opposite X signs with a positive projected
    // cross-dot produce actual edge bevels; axis boxes alone cannot cover it.
    auto model=Box({-10,-10,-10},{10,10,10});
    model.nodes[0]=Plane({1,1,0},10);model.nodes[1]=Plane({-1,1,0},10);
    model.nodes[2]=Plane({-1,-1,0},10);model.nodes[3]=Plane({1,-1,0},10);
    model.nodes[0].collisionBound=0;model=Decoded(model);
    bool bevelChanged{};
    for(int x=-16;x<=16;x+=2) for(int y=-16;y<=16;y+=2) {
        const Vec start{static_cast<float>(x),static_cast<float>(y),2},end{-static_cast<float>(x),-static_cast<float>(y),-2};
        Compare(model,start,end,{2,0.5f,1});
        const auto with=Oracle(model,start,end,{2,0.5f,1});const auto without=Oracle(model,start,end,{2,0.5f,1},false);
        if(with.hit!=without.hit||(with.hit&&!Bits(with.time,without.time))) bevelChanged=true;
    }
    Require(bevelChanged,"Slanted sweep fixtures never exercised a result-changing bevel");
    // Fully three-dimensional tetrahedral planes force X/Y/Z bevel axes and
    // nontrivial Float-normalization/intersection stages, not only XY edges.
    auto tetra=Box({-15,-15,-15},{15,15,15});
    tetra.nodes[0]=Plane({1,1,1},10);tetra.nodes[1]=Plane({1,-1,-1},10);
    tetra.nodes[2]=Plane({-1,1,-1},10);tetra.nodes[3]=Plane({-1,-1,1},10);
    tetra.nodes[0].collisionBound=Hull(tetra,{0,1,2,3},{-15,-15,-15},{15,15,15});tetra=Decoded(tetra);
    for(Vec from:std::array<Vec,8>{{{20,20,20},{-20,20,20},{20,-20,20},{20,20,-20},
        {-20,-20,20},{-20,20,-20},{20,-20,-20},{-20,-20,-20}}}) Compare(tetra,from,{-from.x,-from.y,-from.z},{1.25f,2.5f,0.75f});
}

void Safety() {
    const auto original=Decoded(Box());
    const auto badModel=[&](const std::function<void(Model&)>& mutate,const std::string& reason) {
        auto model=original;mutate(model);Refuses([&] {Trace(model,{10,0,0},{-20,0,0});},reason);
    };
    badModel([](Model& m) {m.version=69;},"Unsupported Model version accepted");
    badModel([](Model& m) {m.licenseeMode=1;},"Unsupported licensee Model layout accepted");
    badModel([](Model& m) {m.rootOutside=2;},"Nonboolean RootOutside accepted");
    badModel([](Model& m) {m.nodes.back().front=99;},"Unreachable invalid child escaped complete graph validation");
    badModel([](Model& m) {m.nodes.back().back=-2;},"Negative child other than-1 accepted");
    badModel([](Model& m) {m.nodes.back().front=static_cast<std::int32_t>(m.nodes.size()-1u);},"Unreachable cycle escaped complete validation");
    badModel([](Model& m) {m.nodes.back().planeW=std::numeric_limits<float>::infinity();},"Unreachable nonfinite BSP plane accepted");
    badModel([](Model& m) {m.nodes.back().collisionBound=-2;},"Invalid negative collision bound accepted");
    badModel([](Model& m) {m.nodes.back().collisionBound=static_cast<std::int32_t>(m.leafHulls.size());},"Unreachable out-of-range hull accepted");
    badModel([](Model& m) {m.leafHulls[0]=-2;},"Nonliteral negative hull terminator accepted");
    badModel([](Model& m) {m.leafHulls[0]=0x20000000;},"Unrecognized hull plane-node bits were silently masked");
    badModel([](Model& m) {m.leafHulls[0]=0x40000077;},"Flipped out-of-range plane-node accepted");
    badModel([](Model& m) {m.leafHulls.resize(6);},"Missing literal hull terminator accepted");
    badModel([](Model& m) {m.leafHulls.pop_back();},"Truncated six-word hull bounds accepted");
    badModel([](Model& m) {m.leafHulls[7]=std::bit_cast<std::int32_t>(1.0f);},"Reversed packed hull min/max accepted");
    badModel([](Model& m) {m.leafHulls[7]=std::bit_cast<std::int32_t>(std::numeric_limits<float>::quiet_NaN());},"Nonfinite packed hull bound accepted");
    auto oversized=original;oversized.leafHulls.clear();oversized.nodes[0].collisionBound=Hull(oversized,std::vector<std::int32_t>(65u,0),{-10,-10,-10},{0,10,10});
    Refuses([&] {Trace(oversized,{10,0,0},{-20,0,0});},"65-plane hull exceeded retail fixed storage without refusal");
    oversized=original;oversized.leafHulls.clear();oversized.nodes[0].collisionBound=Hull(oversized,std::vector<std::int32_t>(64u,0),{-10,-10,-10},{0,10,10});
    Require(Trace(Decoded(oversized),{10,0,0},{-20,0,0}).hit,"Exactly64 repeated planes were incorrectly deduplicated or refused");
    for(Vec extent:std::array<Vec,5>{{{0,0,0},{-1,1,1},{1,-1,1},{1,1,-1},{std::numeric_limits<float>::infinity(),1,1}}})
        Refuses([&] {Trace(original,{10,0,0},{-20,0,0},extent);},"Invalid/point-branch collision extent accepted");
    Refuses([&] {Trace(original,{std::numeric_limits<float>::quiet_NaN(),0,0},{-20,0,0});},"Nonfinite Start accepted");
    Refuses([&] {Trace(original,{10,0,0},{std::numeric_limits<float>::infinity(),0,0});},"Nonfinite End accepted");
    // Limits are closed fixed maxima. Tighten each independently and verify
    // the query refuses instead of returning a successfully validated prefix.
    using Member=std::size_t Limits::*;
    for(Member field:{&Limits::nodes,&Limits::hullWords,&Limits::validationSteps,&Limits::querySteps,&Limits::pending}) {
        auto limits=Limits{};limits.*field=0u;
        Refuses([&] {Trace(original,{10,0,0},{-20,0,0},{1,1,1},limits);},"Zero work bound accepted");
        limits=Limits{};++(limits.*field);
        Refuses([&] {Trace(original,{10,0,0},{-20,0,0},{1,1,1},limits);},"Caller expanded a fixed safety maximum");
    }
    for(Member field:{&Limits::nodes,&Limits::hullWords,&Limits::validationSteps,&Limits::querySteps}) {
        auto limits=Limits{};limits.*field=1u;
        Refuses([&] {Trace(original,{10,0,0},{-20,0,0},{1,1,1},limits);},"Tight work/array bound failed to refuse");
    }
    Model branching;branching.rootOutside=1;branching.nodes.assign(7u,Plane({1,0,0},0,1,0));
    for(std::size_t i=0u;i<3u;++i) {branching.nodes[i].front=static_cast<std::int32_t>(2u*i+1u);
        branching.nodes[i].back=static_cast<std::int32_t>(2u*i+2u);}
    branching=Decoded(branching);Require(Trace(branching,{},{}).stats.nodeVisits==7u,"Wide zero-size BSP fixture did not visit all eligible nodes");
    auto pendingLimit=Limits{};pendingLimit.pending=1u;
    Refuses([&] {Trace(branching,{},{},{1,1,1},pendingLimit);},"Tight pending-branch bound did not refuse before growing its queue");
    // Acyclic shared children are legal but consume repeated query work.
    auto dag=original;dag.nodes[0]=Plane({0,1,0},0,1,0);dag.nodes[0].front=dag.nodes[0].back=1;
    dag.nodes[1].collisionBound=0;dag=Decoded(dag);Compare(dag,{10,0,0},{-20,0,0},{1,1,1});
}

Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);
    Require(size<=256u*1024u*1024u,"Owned map exceeds bounded test file capture");
    std::ifstream stream(path,std::ios::binary);Require(static_cast<bool>(stream),"Cannot read owned Training map");
    Bytes bytes((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
    Require(!stream.bad()&&bytes.size()==size,"Owned Training map read was incomplete");return bytes;
}
std::size_t CheckOracleDepth(const Model& model) {
    // The production walk is iterative. Refuse unsafe reference recursion
    // before invoking it; validate every component and compute actual DAG
    // depth independently, rather than trusting a previously printed depth.
    std::vector<unsigned char> colors(model.nodes.size());std::vector<std::size_t> depths(model.nodes.size());
    struct Frame {std::size_t node;unsigned next;};std::vector<Frame> pending;std::size_t maximum{};
    for(std::size_t root=0u;root<model.nodes.size();++root) {
        if(colors[root]) continue;
        colors[root]=1;pending.push_back({root,0});
        while(!pending.empty()) {
            auto& frame=pending.back();const auto& node=model.nodes.at(frame.node);
            if(frame.next<2u) {
                const auto child=frame.next++?node.back:node.front;if(child==-1) continue;
                Require(child>=0&&static_cast<std::size_t>(child)<model.nodes.size(),"Unsafe owned oracle BSP child");
                const auto index=static_cast<std::size_t>(child);Require(colors[index]!=1u,"Unsafe owned oracle BSP cycle");
                if(!colors[index]) {colors[index]=1;pending.push_back({index,0});}
            } else {
                const auto childDepth=[&](std::int32_t child) {return child==-1?0u:depths.at(static_cast<std::size_t>(child));};
                const auto depth=1u+std::max(childDepth(node.front),childDepth(node.back));
                Require(depth<=512u,"Actual Training BSP exceeds safe independent recursive-oracle depth");
                depths[frame.node]=depth;maximum=std::max(maximum,depth);colors[frame.node]=2u;pending.pop_back();
            }
        }
    }
    return maximum;
}
bool FiniteRefusal(const std::string& error) {
    return error.find("nonfinite")!=std::string::npos||error.find("overflow")!=std::string::npos;
}
void OwnedTraining(const std::filesystem::path& root) {
    const auto path=root/"Maps"/"00_Training.dx";const auto before=ReadBytes(path);
    const auto modified=std::filesystem::last_write_time(path);
    const auto package=LoadPortablePackageTables(path.string());const auto model=LoadPortableRootModel68(package);
    Require(!model.nodes.empty()&&model.bodyBytesConsumed>41u&&!model.objectPath.empty(),"Owned root Model did not decode genuine Training geometry");
    const auto depth=CheckOracleDepth(model);std::set<std::int32_t> offsets;
    for(const auto& node:model.nodes) if(node.collisionBound>=0) offsets.insert(node.collisionBound);
    Require(offsets.size()>=24u,"Owned Training lacks sufficient distinct native collision hulls");
    const std::vector<std::int32_t> hulls(offsets.begin(),offsets.end());
    std::size_t successes{},hits{},finiteRefusals{},queries{};
    for(std::size_t sample=0u;sample<24u;++sample) {
        const auto offset=hulls.at(sample*(hulls.size()-1u)/23u);auto word=static_cast<std::size_t>(offset);
        std::size_t planes{};while(model.leafHulls.at(word)!=-1) {++word;Require(++planes<=64u,"Owned hull exceeds retail64-plane storage");}
        ++word;std::array<float,6> bounds{};for(auto& value:bounds) value=std::bit_cast<float>(model.leafHulls.at(word++));
        const Vec center{Store(Mul(Add(bounds[0],bounds[3]),0.5f)),Store(Mul(Add(bounds[1],bounds[4]),0.5f)),
            Store(Mul(Add(bounds[2],bounds[5]),0.5f))};
        for(unsigned direction=0u;direction<4u;++direction) {
            ++queries;const auto axis=direction%3u;const float extent=direction==3u?2.0f:24.0f;
            auto from=Components(center),to=from;
            const auto reach=Store(Add(Add(Sub(bounds[axis+3u],bounds[axis]),extent),64.0f));
            from[axis]=Store(Add(from[axis],direction==3u?-reach:reach));
            to[axis]=Store(Add(to[axis],direction==3u?reach:-reach));
            const Vec start{from[0],from[1],from[2]},end{to[0],to[1],to[2]},extents{extent,extent,extent};
            QuestVr::ModelExtentTraceResult actual;OracleResult expected;std::string actualError,expectedError;
            try {actual=Trace(model,start,end,extents);} catch(const std::exception& error) {actualError=error.what();}
            try {expected=Oracle(model,start,end,extents);} catch(const std::exception& error) {expectedError=error.what();}
            Require(actualError.empty()==expectedError.empty(),"Owned production/scalar refusal differs at hull"+std::to_string(offset)+
                " axis"+std::to_string(axis)+": production="+actualError+" oracle="+expectedError);
            if(!actualError.empty()) {
                Require(FiniteRefusal(actualError)&&FiniteRefusal(expectedError),"Owned query hit a validation/work error, not a documented finite-math refusal: "+actualError);
                ++finiteRefusals;++refusals;
                if(finiteRefusals<=4u) std::cout<<"EXPECTED finite-math refusal hull="<<offset<<" axis="<<axis<<": "<<actualError<<'\n';
                continue;
            }
            ++successes;if(actual.hit) ++hits;
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
            CompareResults(actual,expected,true);
#else
            CompareResults(actual,expected,false);
#endif
        }
    }
    Require(queries==96u&&successes>=64u&&hits>=12u,"Owned query set did not prove enough actual non-skipped sweeps/hits: successes="+
        std::to_string(successes)+" hits="+std::to_string(hits)+" refusals="+std::to_string(finiteRefusals));
    Require(std::filesystem::last_write_time(path)==modified&&ReadBytes(path)==before,"Owned Training package bytes or mtime changed");
    std::cout<<"PASS owned00_Training native rootModel="<<model.objectPath<<" nodes="<<model.nodes.size()<<" collisionHulls="<<hulls.size()
        <<" maxOracleDepth="<<depth<<" queries="<<queries<<" successes="<<successes<<" hits="<<hits<<" explicitFiniteRefusals="<<finiteRefusals
        <<"; original map bytes/mtime unchanged, no DLL/EXE execution or native Move/physics claim.\n";
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct Precision {
    unsigned short prior{};
    Precision() {static_assert(std::numeric_limits<Real>::digits==64);asm volatile("fnstcw %0":"=m"(prior));
        const unsigned short mode=static_cast<unsigned short>((prior&~0x0f00u)|0x0300u);asm volatile("fldcw %0"::"m"(mode));}
    ~Precision() {asm volatile("fldcw %0"::"m"(prior));}
};
std::uint32_t Next(std::uint32_t& value) {value^=value<<13u;value^=value>>17u;value^=value<<5u;return value;}
void RandomOracle() {
    Precision precision;std::uint32_t seed=0x6128a5d3u;
    for(unsigned iteration=0u;iteration<4000u;++iteration) {
        const auto number=[&] {return static_cast<float>(static_cast<int>(Next(seed)%2049u)-1024)/16.0f;};
        const Vec center{number(),number(),number()};
        const Vec half{1.0f+static_cast<float>(Next(seed)%128u)/8.0f,1.0f+static_cast<float>(Next(seed)%128u)/8.0f,
            1.0f+static_cast<float>(Next(seed)%128u)/8.0f};
        const Vec minimum{center.x-half.x,center.y-half.y,center.z-half.z},maximum{center.x+half.x,center.y+half.y,center.z+half.z};
        auto model=Decoded(Box(minimum,maximum));
        const Vec start{number(),number(),number()},end{number(),number(),number()};
        const Vec extent{static_cast<float>(1u+Next(seed)%32u)/16.0f,static_cast<float>(1u+Next(seed)%32u)/16.0f,
            static_cast<float>(1u+Next(seed)%32u)/16.0f};
        Compare(model,start,end,extent,true);
    }
}
#endif
} // namespace

int main(int argc,char** argv) {
    try {
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        Precision precision;
#endif
        Basics();CsgAndTraversal();SlantedAndBevels();Safety();
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        RandomOracle();
#endif
        if(argc==2) OwnedTraining(argv[1]);
        else if(argc==3&&std::string(argv[1])=="--game-root") OwnedTraining(argv[2]);
        else if(argc!=1) throw std::runtime_error("Usage: model_extent_trace_test [GAME_ROOT | --game-root GAME_ROOT]");
        std::cout<<"PASS original null-owner Model extent kernel: "<<checks<<" checks, "<<refusals
            <<" deliberate safety refusals; decoded v68 hulls, literal terminator/64-plane bound, CSG/near-first BSP, axis/slanted/bevel contacts and independent scalar oracle. Not native Move/FindBase/physics.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL original Model extent kernel: "<<error.what()<<'\n';return 1;}
}
