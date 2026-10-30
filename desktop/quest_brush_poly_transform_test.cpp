#include "quest_brush_poly_transform.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// Detached value-operation tests only. Expected values use independently
// expressed hardware x87PC64 arithmetic, never the production dyadic helpers,
// other production transforms, host trig, or original commercial execution.
// This does not claim a live GMath/table provider or moving-BSP integration.
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace {
using Vec=PortableModelVec3;
using Coords=QuestVr::OriginalModelCoords;
using Scale=QuestVr::OriginalBrushScale;
using Rotation=QuestVr::OriginalBrushRotation;
using Packet=QuestVr::OriginalBrushModelCoords;
using Table=std::array<float,16384u>;
std::size_t checks{},comparisons{},matchingRefusals{},explicitRefusals{},controls{};
void Require(bool good,const std::string& reason){++checks;if(!good)throw std::runtime_error(reason);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
bool Same(float a,float b){return Bits(a)==Bits(b);}
bool Same(Vec a,Vec b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Coords& a,const Coords& b){return Same(a.origin,b.origin)&&Same(a.xAxis,b.xAxis)&&Same(a.yAxis,b.yAxis)&&Same(a.zAxis,b.zAxis);}
bool Same(const Packet& a,const Packet& b){return Same(a.point,b.point)&&Same(a.vector,b.vector)&&Same(a.orientation,b.orientation);}
bool Same(PortableModelSphere a,PortableModelSphere b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&Same(a.radius,b.radius);}
bool Same(const PortableBrushPoly& a,const PortableBrushPoly& b){
    if(!Same(a.base,b.base)||!Same(a.normal,b.normal)||!Same(a.textureU,b.textureU)||!Same(a.textureV,b.textureV))return false;
    for(std::size_t i=0;i<a.vertices.size();++i)if(!Same(a.vertices[i],b.vertices[i]))return false;
    return a.polyFlags==b.polyFlags&&a.actorReference==b.actorReference&&a.textureReference==b.textureReference&&
        a.itemNameReference==b.itemNameReference&&a.vertexCount==b.vertexCount&&a.link==b.link&&a.brushPoly==b.brushPoly&&a.panU==b.panU&&a.panV==b.panV;
}
template<class A,class E>void Compare(A actual,E expected,const char* label){
    using Value=decltype(actual());Value a{},e{};bool failA{},failE{};
    try{a=actual();}catch(const std::exception&){failA=true;}
    try{e=expected();}catch(const std::exception&){failE=true;}
    Require(failA==failE,std::string("Transform finite-domain disagreement: ")+label);
    if(failA){++matchingRefusals;return;}
    Require(Same(a,e),std::string("Independent x87 Float bits differ: ")+label);++comparisons;
}
template<class F>void Refuse(F run,const char* label){bool failed{};try{run();}catch(const std::exception&){failed=true;}
    Require(failed,std::string("Unsupported active data accepted: ")+label);++explicitRefusals;}

namespace Hw {
using Real=long double;
struct Precision {
    unsigned short old{};
    Precision(){static_assert(std::numeric_limits<Real>::digits==64,"Independent oracle requires x87PC64");
        asm volatile("fnstcw %0":"=m"(old));const auto word=static_cast<unsigned short>((old&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(word));}
    ~Precision(){asm volatile("fldcw %0"::"m"(old));}
    Precision(const Precision&)=delete;Precision& operator=(const Precision&)=delete;
};
Real Read(float value){if(!std::isfinite(value))throw std::runtime_error("oracle active Float nonfinite");return value;}
Real Add(Real a,Real b){volatile Real x=a,y=b,r=x+y;return r;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,r=x-y;return r;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,r=x*y;return r;}
Real Div(Real a,Real b){if(b==0.0L)throw std::runtime_error("oracle active zero divisor");volatile Real x=a,y=b,r=x/y;return r;}
Real Neg(Real value){Real result;asm volatile("fldt %1; fchs; fstpt %0":"=m"(result):"m"(value):"st");return result;}
float F(Real value){float result;asm volatile("fldt %1; fstps %0":"=m"(result):"m"(value):"st");
    if(!std::isfinite(result))throw std::runtime_error("oracle Float store overflow");
    return result;}
double D(Real value){double result;asm volatile("fldt %1; fstpl %0":"=m"(result):"m"(value):"st");
    if(!std::isfinite(result))throw std::runtime_error("oracle Double store overflow");
    return result;}
Real Root(double value){Real result;asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(result):"m"(value):"st");return result;}
Coords Unit(){return {{0,0,0},{1,0,0},{0,1,0},{0,0,1}};}
Vec Vector(const std::array<Real,3u>& v,const Coords& c){const auto row=[&](Vec a){return F(Add(Add(Mul(v[0],Read(a.x)),Mul(v[1],Read(a.y))),Mul(v[2],Read(a.z))));};
    return {row(c.xAxis),row(c.yAxis),row(c.zAxis)};}
Vec Vector(Vec v,const Coords& c){return Vector(std::array<Real,3u>{Read(v.x),Read(v.y),Read(v.z)},c);}
Vec Point(Vec v,const Coords& c){return Vector(std::array<Real,3u>{Sub(Read(v.x),Read(c.origin.x)),Sub(Read(v.y),Read(c.origin.y)),Sub(Read(v.z),Read(c.origin.z))},c);}
Coords Compose(const Coords& a,const Coords& b){return {Point(a.origin,b),Vector(a.xAxis,b),Vector(a.yAxis,b),Vector(a.zAxis,b)};}
Real Snap(float source){const auto value=Read(source);
    const auto db=[](std::uint64_t bits){return std::bit_cast<double>(bits);};
    if(value<db(0xbfe4cccccccccccdull))return Add(value,db(0x3fc3333333333333ull));
    if(value>db(0x3fe4cccccccccccdull))return Sub(value,db(0x3fc3333333333333ull));
    if(value<db(0xbfe199999999999aull))return -0.5L;
    if(value>db(0x3fe199999999999aull))return 0.5L;
    if(value<db(0xbfa999999999999aull))return Add(value,db(0x3fa999999999999aull));
    if(value>db(0x3fa999999999999aull))return Sub(value,db(0x3fa999999999999aull));
    return 0.0L;
}
Coords Shear(const Scale& s,bool inverse){auto c=Unit();if(s.sheerAxis>=1u&&s.sheerAxis<=6u){auto retained=Snap(s.sheerRate);if(inverse)retained=Neg(retained);const auto v=F(retained);
    switch(s.sheerAxis){case 1:c.xAxis.y=v;break;case 2:c.xAxis.z=v;break;case 3:c.yAxis.x=v;break;
        case 4:c.yAxis.z=v;break;case 5:c.zAxis.x=v;break;case 6:c.zAxis.y=v;break;}}
    return c;}
Vec Times(Vec v,Vec s){return {F(Mul(Read(v.x),Read(s.x))),F(Mul(Read(v.y),Read(s.y))),F(Mul(Read(v.z),Read(s.z)))};}
Vec Divide(Vec v,Vec s){return {F(Div(Read(v.x),Read(s.x))),F(Div(Read(v.y),Read(s.y))),F(Div(Read(v.z),Read(s.z)))};}
Coords ScaleBy(Coords c,const Scale& s){c=Compose(c,Shear(s,false));c.xAxis=Times(c.xAxis,s.scale);c.yAxis=Times(c.yAxis,s.scale);c.zAxis=Times(c.zAxis,s.scale);c.origin=Divide(c.origin,s.scale);return c;}
Coords InverseScale(Coords c,const Scale& s){c.xAxis=Divide(c.xAxis,s.scale);c.yAxis=Divide(c.yAxis,s.scale);c.zAxis=Divide(c.zAxis,s.scale);c.origin=Times(c.origin,s.scale);return Compose(c,Shear(s,true));}
Table SinTable(){Table table{};const double step=std::bit_cast<double>(std::uint64_t{0x3f3921fb54442d18ull});
    for(std::int32_t i=0;i<16384;++i){double argument{};float result{};
        asm volatile("fildl %1; fmull %2; fstpl %0":"=m"(argument):"m"(i),"m"(step):"st");
        asm volatile("fldl %1; fsin; fstps %0":"=m"(result):"m"(argument):"st");table[static_cast<std::size_t>(i)]=result;}
    return table;}
float Lookup(const Table& table,std::int32_t angle,bool cosine=false){std::int64_t phase=angle;if(cosine)phase+=16384;phase%=65536;if(phase<0)phase+=65536;
    const auto value=table[static_cast<std::size_t>(phase/4)];static_cast<void>(Read(value));return value;}
Coords Yaw(const Table& table,std::int32_t angle,bool inverse){const auto s=Lookup(table,angle),c=Lookup(table,angle,true),minus=F(Neg(Read(s)));
    return inverse?Coords{{0,0,0},{c,minus,0},{s,c,0},{0,0,1}}:Coords{{0,0,0},{c,s,0},{minus,c,0},{0,0,1}};}
Coords Pitch(const Table& table,std::int32_t angle,bool inverse){const auto s=Lookup(table,angle),c=Lookup(table,angle,true),minus=F(Neg(Read(s)));
    return inverse?Coords{{0,0,0},{c,0,minus},{0,1,0},{s,0,c}}:Coords{{0,0,0},{c,0,s},{0,1,0},{minus,0,c}};}
Coords Roll(const Table& table,std::int32_t angle,bool inverse){const auto s=Lookup(table,angle),c=Lookup(table,angle,true),minus=F(Neg(Read(s)));
    return inverse?Coords{{0,0,0},{1,0,0},{0,c,s},{0,minus,c}}:Coords{{0,0,0},{1,0,0},{0,c,minus},{0,s,c}};}
Coords Rotate(Coords c,Rotation r,const Table& t,bool inverse){
    if(inverse){c=Compose(c,Roll(t,r.roll,true));c=Compose(c,Pitch(t,r.pitch,true));return Compose(c,Yaw(t,r.yaw,true));}
    c=Compose(c,Yaw(t,r.yaw,false));c=Compose(c,Pitch(t,r.pitch,false));return Compose(c,Roll(t,r.roll,false));}
Real Sign(Vec v){const auto product=Mul(Mul(Read(v.z),Read(v.y)),Read(v.x));return product>0?1.0L:product<0?-1.0L:0.0L;}
Packet Build(Rotation r,const Scale& main,const Scale& post,const Table& t){Packet p;
    p.point=ScaleBy(Rotate(ScaleBy(Unit(),post),r,t,false),main);
    const auto inverse=InverseScale(Rotate(InverseScale(Unit(),main),r,t,true),post);
    const auto origin=Vector(inverse.origin,inverse);
    p.vector={{F(Neg(Read(origin.x))),F(Neg(Read(origin.y))),F(Neg(Read(origin.z)))},
        {inverse.xAxis.x,inverse.yAxis.x,inverse.zAxis.x},{inverse.xAxis.y,inverse.yAxis.y,inverse.zAxis.y},
        {inverse.xAxis.z,inverse.yAxis.z,inverse.zAxis.z}};
    p.orientation=F(Mul(Sign(main.scale),F(Sign(post.scale))));return p;}
Vec SafeNormal(Vec value){const auto sum=Add(Add(Mul(Read(value.x),Read(value.x)),Mul(Read(value.y),Read(value.y))),Mul(Read(value.z),Read(value.z)));
    const auto stored=D(sum);if(sum<std::bit_cast<double>(std::uint64_t{0x3e45798ee2308c3aull}))return {0,0,0};
    const auto inverse=Div(1.0L,Root(stored));return {F(Mul(Read(value.x),inverse)),F(Mul(Read(value.y),inverse)),F(Mul(Read(value.z),inverse))};}
Vec PolyPoint(Vec v,const Coords& c,Vec pre,Vec post){const Vec delta{F(Sub(Read(v.x),Read(pre.x))),F(Sub(Read(v.y),Read(pre.y))),F(Sub(Read(v.z),Read(pre.z)))};
    const auto transformed=Vector(delta,c);return {F(Add(Read(transformed.x),Read(post.x))),F(Add(Read(transformed.y),Read(post.y))),F(Add(Read(transformed.z),Read(post.z)))};}
PortableBrushPoly Poly(PortableBrushPoly p,const Packet& c,Vec pre,Vec post){if(p.vertexCount<0||p.vertexCount>16)throw std::runtime_error("oracle invalid native vertex count");
    p.textureU=Vector(p.textureU,c.vector);p.textureV=Vector(p.textureV,c.vector);p.base=PolyPoint(p.base,c.point,pre,post);
    for(std::int32_t i=0;i<p.vertexCount;++i)p.vertices[static_cast<std::size_t>(i)]=PolyPoint(p.vertices[static_cast<std::size_t>(i)],c.point,pre,post);
    if(Read(c.orientation)<0)std::reverse(p.vertices.begin(),p.vertices.begin()+p.vertexCount);
    p.normal=SafeNormal(Vector(p.normal,c.vector));return p;}
PortableModelSphere Sphere(std::span<const Vec> points){PortableModelSphere result{};if(points.empty())return result;
    Vec low=points[0],high=points[0];static_cast<void>(Read(low.x));static_cast<void>(Read(low.y));static_cast<void>(Read(low.z));
    const auto fold=[](float value,float& a,float& b){static_cast<void>(Read(value));if(a>value)a=value;if(b<value)b=value;};
    for(std::size_t i=1;i<points.size();++i){fold(points[i].x,low.x,high.x);fold(points[i].y,low.y,high.y);fold(points[i].z,low.z,high.z);}
    result.x=F(Mul(F(Add(Read(low.x),Read(high.x))),0.5f));result.y=F(Mul(F(Add(Read(low.y),Read(high.y))),0.5f));
    result.z=F(Mul(Add(Read(low.z),Read(high.z)),0.5f));float maxSquared{};
    for(const auto p:points){const auto dx=Sub(Read(result.x),Read(p.x)),dy=Sub(Read(result.y),Read(p.y)),dz=Sub(Read(result.z),Read(p.z));
        const auto squared=Add(Add(Mul(dy,dy),Mul(dx,dx)),Mul(dz,dz));if(squared>maxSquared)maxSquared=F(squared);}
    const auto root=Root(static_cast<double>(maxSquared));result.radius=F(Mul(root,std::bit_cast<double>(std::uint64_t{0x3ff004189374bc6aull})));return result;}
} // namespace Hw

PortableBrushPoly Fixture(std::int32_t n=5){PortableBrushPoly p;p.base={3,-5,7};p.normal={0.25f,-0.5f,0.75f};p.textureU={1.5f,-2,3};p.textureV={-4,5,-6};p.vertexCount=n;
    for(std::size_t i=0;i<p.vertices.size();++i)p.vertices[i]={static_cast<float>(i)+0.25f,-static_cast<float>(i)-0.5f,static_cast<float>(i)*2.0f};
    p.polyFlags=0xfedcba99u;p.actorReference=-123;p.textureReference=197;p.itemNameReference=2;p.link=-1;p.brushPoly=std::numeric_limits<std::int32_t>::min();p.panU=-32768;p.panV=32767;return p;}
std::uint64_t Next(std::uint64_t& s){s^=s<<13u;s^=s>>7u;s^=s<<17u;return s;}
float Number(std::uint64_t& s,bool broad){const auto bits=static_cast<std::uint32_t>(Next(s));return broad?
    std::bit_cast<float>((bits&0x807fffffu)|(((bits>>23u)%255u)<<23u)):static_cast<float>(static_cast<int>(bits%65537u)-32768)/64.0f;}
Vec RandomVec(std::uint64_t& s,bool broad){return {Number(s,broad),Number(s,broad),Number(s,broad)};}
Coords RandomCoords(std::uint64_t& s,bool broad){return {RandomVec(s,broad),RandomVec(s,broad),RandomVec(s,broad),RandomVec(s,broad)};}
Rotation RandomRotation(std::uint64_t& s){return {std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(s))),std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(s))),
    std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(s)))};}

void Deterministic(const Table& table){const Coords coords{{3,-5,7},{2,1,0},{0,-3,0.5f},{4,0,1}};
    for(unsigned axis=0;axis<256u;++axis){const Scale scale{{2,-3,0.25f},0.6f,static_cast<std::uint8_t>(axis)};
        Compare([&]{return QuestVr::InverseScaleOriginalModelCoords(coords,scale);},[&]{return Hw::InverseScale(coords,scale);},"InverseScale all axis bytes");}
    for(double threshold:{-0.65,-0.55,-0.05,0.05,0.55,0.65}){const auto f=static_cast<float>(threshold);
        for(const auto value:{std::nextafter(f,-std::numeric_limits<float>::infinity()),f,std::nextafter(f,std::numeric_limits<float>::infinity())})
            for(std::uint8_t axis=1;axis<=6u;++axis){const Scale s{{3,-7,11},value,axis};
                Compare([&]{return QuestVr::InverseScaleOriginalModelCoords(coords,s);},[&]{return Hw::InverseScale(coords,s);},"Inverse sheer threshold neighbour");}}
    const std::array<std::int32_t,17u> angles{std::numeric_limits<std::int32_t>::min(),-65537,-16385,-16384,-5,-4,-3,-2,-1,0,1,2,3,16383,16384,65535,std::numeric_limits<std::int32_t>::max()};
    const Scale main{{-2,3,0.25f},0.7f,3},post{{0.75f,1.25f,-4},-0.6f,6};
    for(const auto angle:angles){const Rotation r{angle,angle+static_cast<std::int32_t>(angle!=std::numeric_limits<std::int32_t>::max()),angle};
        Compare([&]{return QuestVr::InverseRotateOriginalModelCoords(coords,r,table);},[&]{return Hw::Rotate(coords,r,table,true);},"Inverse Roll Pitch Yaw angle edges");
        Compare([&]{return QuestVr::BuildOriginalBrushModelCoords(r,main,post,table);},[&]{return Hw::Build(r,main,post,table);},"Full BuildCoords angle edges");}
    // Table quantization follows modulo-before-division even for negative low
    // bits; negating an integer angle before lookup is not the same operation.
    for(std::int32_t phase=0;phase<16384;phase+=17){const Rotation r{-phase*4-1,phase*4+2,-phase*4-3};
        Compare([&]{return QuestVr::InverseRotateOriginalModelCoords(coords,r,table);},[&]{return Hw::Rotate(coords,r,table,true);},"Inverse full phase sweep");}
    Packet packet{Hw::Unit(),Hw::Unit(),-1};
    for(std::int32_t n=0;n<=16;++n){const auto poly=Fixture(n);const auto actual=QuestVr::TransformOriginalBrushPoly(poly,packet,{0,0,0},{0,0,0});
        Require(Same(actual,Hw::Poly(poly,packet,{0,0,0},{0,0,0})),"Full mirrored polygon differs");
        for(std::int32_t i=0;i<n;++i)Require(Same(actual.vertices[static_cast<std::size_t>(i)],poly.vertices[static_cast<std::size_t>(n-1-i)]),"Mirrored reversal preserved fan vertex0");
        for(std::size_t i=static_cast<std::size_t>(n);i<16u;++i)Require(Same(actual.vertices[i],poly.vertices[i]),"Inactive vertex slot changed");}
    for(const auto orientation:{0.0f,-0.0f,1.0f,-1.0f}){packet.orientation=orientation;const auto p=Fixture();
        Compare([&]{return QuestVr::TransformOriginalBrushPoly(p,packet,{1,-2,3},{-4,5,-6});},[&]{return Hw::Poly(p,packet,{1,-2,3},{-4,5,-6});},"Orientation signed zeros");}
    packet={Hw::Unit(),Hw::Unit(),1};const auto normalThreshold=static_cast<float>(0.0001);
    for(const auto length:{0.0f,-0.0f,std::numeric_limits<float>::denorm_min(),std::nextafter(normalThreshold,0.0f),normalThreshold,
        std::nextafter(normalThreshold,1.0f),1.0f,16777216.0f}){auto p=Fixture();p.normal={length,-0.0f,0.0f};
        Compare([&]{return QuestVr::TransformOriginalBrushPoly(p,packet,{},{ });},[&]{return Hw::Poly(p,packet,{},{ });},"SafeNormal strict threshold");}
    const std::array<Vec,4u> zeros{{{-0.0f,0.0f,-0.0f},{0.0f,-0.0f,0.0f},{-0.0f,-0.0f,-0.0f},{0.0f,0.0f,0.0f}}};
    for(std::size_t i=0;i<=zeros.size();++i){const auto points=std::span(zeros).first(i);
        Compare([&]{return QuestVr::BuildOriginalBrushSphere(points);},[&]{return Hw::Sphere(points);},"Sphere empty/equal signed zeros");}
    // A sphere center's X/Y sum spills can overflow while identical Z sum does
    // not: it remains PC64 until multiplied by0.5. Do not symmetrize this code.
    const auto huge=std::numeric_limits<float>::max();const std::array<Vec,1u> zHuge{{{0,0,huge}}},xHuge{{{huge,0,0}}},yHuge{{{0,huge,0}}};
    Compare([&]{return QuestVr::BuildOriginalBrushSphere(zHuge);},[&]{return Hw::Sphere(zHuge);},"Sphere retained Z center sum");
    Compare([&]{return QuestVr::BuildOriginalBrushSphere(xHuge);},[&]{return Hw::Sphere(xHuge);},"Sphere X early sum overflow");
    Compare([&]{return QuestVr::BuildOriginalBrushSphere(yHuge);},[&]{return Hw::Sphere(yHuge);},"Sphere Y early sum overflow");
    Require(Hw::Sphere(zHuge).z==huge&&Bits(Hw::Sphere(zHuge).radius)==0u,"Sphere Z asymmetric control ineffective");++controls;
}

void RoundingControls(){
    const Scale three{{3,1,1},0,0};auto divideInput=Hw::Unit();divideInput.xAxis.x=7;
    const auto divided=Hw::InverseScale(divideInput,three);
    const auto reciprocalProduct=Hw::F(Hw::Mul(7.0f,Hw::F(Hw::Div(1.0f,3.0f))));
    Require(!Same(divided.xAxis.x,reciprocalProduct),"Inverse division-vs-Float-reciprocal control ineffective");++controls;
    Require(Same(QuestVr::InverseScaleOriginalModelCoords(divideInput,three),divided),"Inverse helper used Float reciprocal multiply");
    Packet packet{Hw::Unit(),Hw::Unit(),1};packet.point.xAxis.x=1.5f;
    auto p=Fixture(1);p.base={16777216.0f,0,0};p.vertices[0]=p.base;p.normal={0,0,1};const Vec pre{-1,0,0},post{-25165824.0f,0,0};
    const auto early=Hw::Poly(p,packet,pre,post);
    const auto wrongNoDelta=Hw::F(Hw::Add(Hw::Mul(Hw::Sub(p.base.x,pre.x),1.5f),post.x));
    Require(!Same(early.base.x,wrongNoDelta),"Poly early Float delta discriminator is ineffective");++controls;
    Require(Same(QuestVr::TransformOriginalBrushPoly(p,packet,pre,post),early),"Poly silently retained unspilled point delta");
    p.base={16777216.0f,0,0};p.vertices[0]=p.base;const Vec pre2{1,0,0},post2{-25165822.0f,0,0};
    const auto stored=Hw::Poly(p,packet,pre2,post2);const auto delta=Hw::F(Hw::Sub(p.base.x,pre2.x));
    const auto wrongNoVector=Hw::F(Hw::Add(Hw::Mul(delta,1.5f),post2.x));
    Require(!Same(stored.base.x,wrongNoVector),"Poly intermediate VectorBy store discriminator ineffective");++controls;
    Require(Same(QuestVr::TransformOriginalBrushPoly(p,packet,pre2,post2),stored),"Poly fused VectorBy and PostAdd");
    // Independent searches produce a witnessed root-spill-sensitive case,
    // instead of asserting every arbitrary sqrt changes after Double spill.
    bool found{};std::uint64_t state=0x1d79a62e7cb4f803ull;
    for(unsigned i=0;i<50000u&&!found;++i){const auto squared=static_cast<float>((Next(state)%16777215u)+1u);
        const auto root=Hw::Root(static_cast<double>(squared));const double factor=std::bit_cast<double>(std::uint64_t{0x3ff004189374bc6aull});
        const auto retained=Hw::F(Hw::Mul(root,factor)),spilled=Hw::F(Hw::Mul(Hw::D(root),factor));
        if(!Same(retained,spilled))found=true;}
    // Float differences here can be very rare. This search is informative,
    // not an invented requirement that this finite sample contain a witness.
    std::cout<<"Sphere retained-vs-Double-root spill witness in50000 positive Float squares: "<<(found?"observed":"not observed")<<"\n";
}

void Differential(const Table& table){std::uint64_t state=0xace1397b8d0246f5ull;
    for(unsigned i=0;i<2400u;++i){const bool broad=i<600u;const auto c=RandomCoords(state,broad);const auto r=RandomRotation(state);
        const Scale s{RandomVec(state,broad),Number(state,broad),static_cast<std::uint8_t>(Next(state))};
        Compare([&]{return QuestVr::InverseScaleOriginalModelCoords(c,s);},[&]{return Hw::InverseScale(c,s);},"Random inverse scale finite/subnormal/extreme");
        Compare([&]{return QuestVr::InverseRotateOriginalModelCoords(c,r,table);},[&]{return Hw::Rotate(c,r,table,true);},"Random inverse rotation");
        const Scale post{RandomVec(state,broad),Number(state,broad),static_cast<std::uint8_t>(Next(state))};
        Compare([&]{return QuestVr::BuildOriginalBrushModelCoords(r,s,post,table);},[&]{return Hw::Build(r,s,post,table);},"Random full BuildCoords");
    }
    for(unsigned i=0;i<3200u;++i){const bool broad=i<800u;Packet packet{RandomCoords(state,broad),RandomCoords(state,broad),(Next(state)&1u)?-1.0f:1.0f};
        auto p=Fixture(static_cast<std::int32_t>(Next(state)%17u));p.base=RandomVec(state,broad);p.normal=RandomVec(state,broad);p.textureU=RandomVec(state,broad);p.textureV=RandomVec(state,broad);
        for(std::int32_t n=0;n<p.vertexCount;++n)p.vertices[static_cast<std::size_t>(n)]=RandomVec(state,broad);
        const auto pre=RandomVec(state,broad),post=RandomVec(state,broad);
        Compare([&]{return QuestVr::TransformOriginalBrushPoly(p,packet,pre,post);},[&]{return Hw::Poly(p,packet,pre,post);},"Random FPoly full packet");
        std::array<Vec,16u> points{};const auto count=static_cast<std::size_t>(Next(state)%17u);for(std::size_t n=0;n<count;++n)points[n]=RandomVec(state,broad);
        const auto span=std::span(points).first(count);Compare([&]{return QuestVr::BuildOriginalBrushSphere(span);},[&]{return Hw::Sphere(span);},"Random sphere finite/subnormal/extreme");
    }
    const std::array<float,9u> edges{0.0f,-0.0f,std::numeric_limits<float>::denorm_min(),-std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(),-std::numeric_limits<float>::min(),1.0f,std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};
    for(const auto x:edges)for(const auto y:edges){const Coords c{{x,y,x},{x,y,x},{y,x,y},{x,x,y}};const Scale s{{x,y,1},y,1};
        Compare([&]{return QuestVr::InverseScaleOriginalModelCoords(c,s);},[&]{return Hw::InverseScale(c,s);},"Inverse edge products/divides");
        const std::array<Vec,3u> points{{{x,y,x},{y,x,y},{x,x,y}}};
        Compare([&]{return QuestVr::BuildOriginalBrushSphere(points);},[&]{return Hw::Sphere(points);},"Sphere edge products/stores");}
    Table synthetic{};for(std::size_t i=0;i<synthetic.size();++i)synthetic[i]=static_cast<float>(static_cast<int>(i)-8192)/8192.0f;
    for(unsigned i=0;i<128u;++i){const auto c=RandomCoords(state,false);const auto r=RandomRotation(state);
        Compare([&]{return QuestVr::InverseRotateOriginalModelCoords(c,r,synthetic);},[&]{return Hw::Rotate(c,r,synthetic,true);},"Supplied synthetic table, not cached trig");}
}

void Admission(const Table& table){const auto unit=Hw::Unit();Packet packet{unit,unit,1};const auto fixture=Fixture();
    for(const auto bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        for(std::uint8_t axis:{std::uint8_t{1},std::uint8_t{6}}){const Scale s{{1,1,1},bad,axis};
            Refuse([&]{QuestVr::InverseScaleOriginalModelCoords(unit,s);},"Active inverse sheer");}
        for(std::uint8_t axis:{std::uint8_t{0},std::uint8_t{7},std::uint8_t{255}}){const Scale s{{1,1,1},bad,axis};
            Compare([&]{return QuestVr::InverseScaleOriginalModelCoords(unit,s);},[&]{return Hw::InverseScale(unit,s);},"Inactive inverse sheer ignored");}
        auto poisoned=table;poisoned[0]=bad;Refuse([&]{QuestVr::InverseRotateOriginalModelCoords(unit,{0,0,0},poisoned);},"Active table lookup");
        auto p=fixture;p.base.x=bad;Refuse([&]{QuestVr::TransformOriginalBrushPoly(p,packet,{},{ });},"Active polygon base");
        p=fixture;p.normal.x=bad;Refuse([&]{QuestVr::TransformOriginalBrushPoly(p,packet,{},{ });},"Active polygon normal");
        p=fixture;p.textureU.y=bad;Refuse([&]{QuestVr::TransformOriginalBrushPoly(p,packet,{},{ });},"Active polygon texture vector");
        p=fixture;p.vertices[0].z=bad;Refuse([&]{QuestVr::TransformOriginalBrushPoly(p,packet,{},{ });},"Active polygon vertex");
        p=fixture;p.vertices[15]={bad,bad,bad};Compare([&]{return QuestVr::TransformOriginalBrushPoly(p,packet,{},{ });},[&]{return Hw::Poly(p,packet,{},{ });},"Unused nonfinite vertex bits preserved");
        auto inactive=packet;inactive.point.origin={bad,bad,bad};inactive.vector.origin={bad,bad,bad};
        Compare([&]{return QuestVr::TransformOriginalBrushPoly(fixture,inactive,{},{ });},[&]{return Hw::Poly(fixture,inactive,{},{ });},"FPoly does not consume either coords Origin");
        auto orientation=packet;orientation.orientation=bad;Refuse([&]{QuestVr::TransformOriginalBrushPoly(fixture,orientation,{},{ });},"Nonfinite orientation");
        const std::array<Vec,1u> points{{{0,bad,0}}};Refuse([&]{QuestVr::BuildOriginalBrushSphere(points);},"Nonfinite sphere source");
    }
    for(const auto n:{-1,17}){auto p=fixture;p.vertexCount=n;Refuse([&]{QuestVr::TransformOriginalBrushPoly(p,packet,{},{ });},"Native vertex count bound");}
    for(const auto zero:{0.0f,-0.0f}){const Scale s{{1,zero,1},0,0};Refuse([&]{QuestVr::InverseScaleOriginalModelCoords(unit,s);},"Active inverse zero divisor");}
    auto sparse=table;for(std::size_t i=0;i<sparse.size();++i)if(i!=0u&&i!=4096u)sparse[i]=std::numeric_limits<float>::quiet_NaN();
    Compare([&]{return QuestVr::InverseRotateOriginalModelCoords(unit,{0,0,0},sparse);},[&]{return Hw::Rotate(unit,{0,0,0},sparse,true);},"Only selected table entries consumed");
}
} // namespace
#endif

int main(){
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    try{Hw::Precision precision;const auto table=Hw::SinTable();
        Require(Bits(table[0])==0u&&table[4096]==1.0f&&table[12288]==-1.0f&&table[8192]!=0.0f,"Independent FSIN profile table invalid");
        Deterministic(table);RoundingControls();Differential(table);Admission(table);
        std::cout<<"PASS original brush model/poly/sphere "<<checks<<" checks; "<<comparisons<<" independent x87 comparisons, "<<matchingRefusals
            <<" matching numeric refusals, "<<explicitRefusals<<" explicit refusals, "<<controls<<" effective rounding controls.\n"
            <<"PC64 supplied-table value profile only; original static-initializer CW/table lifecycle, tracker/BSP/startup/headset remain separate.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL original brush transform: "<<error.what()<<'\n';return 1;}
#else
    std::cout<<"SKIP independent brush transform oracle: GCC x86 x87PC64 hardware required; no host-math substitute.\n";return 77;
#endif
}
