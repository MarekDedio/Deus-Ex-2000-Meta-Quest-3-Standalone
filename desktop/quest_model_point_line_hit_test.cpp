#include "quest_model_point_line_hit.h"
#include "quest_model_owner_trace_oracle.h"

#include <iostream>

namespace {
using Vec=PortableModelVec3;
using Model=PortableModelGeometry;
using Hit=QuestVr::OriginalActorLineHit;
using Coords=QuestVr::OriginalModelCoords;
using Limits=QuestVr::ModelPointTraceLimits;
namespace O=QuestVr::ModelOwnerTraceOracle;
namespace D=O::Detail;
std::size_t checks{},refusals{},comparisons{};
void Require(bool value,const char* why) {++checks;if(!value)throw std::runtime_error(why);}
bool Bits(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec a,Vec b) {return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
bool Same(const Hit& a,const Hit& b) {return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&
    Same(a.location,b.location)&&Same(a.normal,b.normal)&&Bits(a.time,b.time)&&a.item==b.item;}
template<class F>void Refuses(F f) {bool refused{};try{f();}catch(const std::exception&){refused=true;}Require(refused,"Expected explicit point refusal");++refusals;}
Hit Seed() {Hit h;h.next=0x1357u;h.actor=0x2468u;h.primitive=0x9753u;
    h.location={71,-72,73};h.normal={-81,82,-83};h.time=.125f;h.item=741;return h;}
Coords Identity() {return {{},{1,0,0},{0,1,0},{0,0,1}};}
PortableModelNode Plane(Vec normal={1,0,0},float w=0) {
    PortableModelNode n;n.planeX=normal.x;n.planeY=normal.y;n.planeZ=normal.z;n.planeW=w;
    n.vertexCount=3;n.back=n.front=n.plane=-1;n.surface=9999;return n;
}
Model One() {Model m;m.rootOutside=1;m.nodes.push_back(Plane());return m;}
bool Trace(Hit& hit,const Model& m,Vec start,Vec end,std::uint32_t flags=0,const Coords* coords=nullptr,
    const Limits& limits={},QuestVr::ModelPointTraceStats* stats=nullptr) {
    return QuestVr::CheckOriginalModelPoint(hit,m,0x1234u,coords?0x5678u:0u,end,start,flags,
        [&]{return *coords;},limits,stats);
}

// Independent recursive point oracle. Hardware x87 PC64 arithmetic is supplied
// by the existing TEST-only scalar oracle, never production dyadic/coordinate/
// point/extent helpers. No commercial machine code is loaded or executed.
struct OracleResult {bool clear{};Hit hit;};
OracleResult Oracle(const Model& m,Vec start,Vec end,unsigned flags,const Coords* captured) {
    O::Precision precision;auto hit=Seed();bool wasOutside{};std::size_t work{};
    const auto csg=[&](const PortableModelNode& n,bool sameSide) {
        const auto ignored=(flags|(sameSide?0u:0x10u))&0x10u;
        const auto filter=static_cast<unsigned>((flags|0x21u)&(sameSide?~ignored:~0u));
        return n.vertexCount>0 && !(static_cast<unsigned>(n.nodeFlags)&filter);
    };
    const auto distance=[&](Vec p,const PortableModelNode& n) {
        const Vec local{n.planeX,n.planeY,n.planeZ};
        if(!captured)return D::Store(D::Sub(D::Add(D::Add(D::Mul(p.z,local.z),D::Mul(p.x,local.x)),D::Mul(p.y,local.y)),n.planeW));
        const O::Coords c{captured->origin,captured->xAxis,captured->yAxis,captured->zAxis};
        const float z=D::Store(D::DotZYX(local,c.zAxis)),y=D::Store(D::DotZYX(local,c.yAxis)),x=D::Store(D::DotZYX(local,c.xAxis));
        const Vec normal{x,y,z};const auto origin=D::VectorByCoords(c.origin,c);
        const auto shifted=D::Sub(n.planeW,D::DotZYX(origin,normal));
        return D::Store(D::Sub(D::DotZYX(p,normal),shifted));
    };
    std::function<bool(int,int,Vec,Vec,bool,unsigned)> walk;
    walk=[&](int index,int last,Vec a,Vec b,bool outside,unsigned depth) {
        if(++work>100000u||depth>128u)throw std::runtime_error("Oracle point recursion bound");
        if(index<0) {
            if(outside) {wasOutside=true;return true;}
            if(!wasOutside&&(flags&16u))return true;
            hit.location=a;const auto& n=m.nodes.at(static_cast<std::size_t>(last));
            hit.normal={n.planeX,n.planeY,n.planeZ};hit.primitive=0x1234u;hit.item=last;return false;
        }
        const auto& n=m.nodes.at(static_cast<std::size_t>(index));const auto from=distance(a,n),to=distance(b,n);
        if(from>-0.001&&to>-0.001)return walk(n.front,last,a,b,outside||csg(n,true),depth+1u);
        if(from<0.001&&to<0.001)return walk(n.back,last,a,b,outside&&!csg(n,true),depth+1u);
        const auto ratio=D::Div(from,D::Sub(to,from));
        const auto midpoint=[&](float p,float q) {return D::Store(D::Add(p,D::Store(D::Mul(D::Store(D::Sub(p,q)),ratio))));};
        const Vec middle{midpoint(a.x,b.x),midpoint(a.y,b.y),midpoint(a.z,b.z)};
        const bool first=from>0;const bool solid=csg(n,false);
        if(!walk(first?n.front:n.back,last,a,middle,first?(outside||solid):(outside&&!solid),depth+1u))return false;
        return walk(first?n.back:n.front,index,middle,b,first?(outside&&!solid):(outside||solid),depth+1u);
    };
    const auto clear=walk(0,0,start,end,m.rootOutside!=0,0u);
    if(clear)return {true,hit};
    const Vec delta{D::Store(D::Sub(end.x,start.x)),D::Store(D::Sub(end.y,start.y)),D::Store(D::Sub(end.z,start.z))};
    const float square=D::Store(D::Add(D::Add(D::Mul(delta.x,delta.x),D::Mul(delta.y,delta.y)),D::Mul(delta.z,delta.z)));
    if(square<=0)throw std::runtime_error("Oracle point zero segment");
    const auto projected=D::Store(D::Div(D::Add(D::Add(D::Mul(D::Sub(hit.location.x,start.x),delta.x),
        D::Mul(D::Sub(hit.location.y,start.y),delta.y)),D::Mul(D::Sub(hit.location.z,start.z),delta.z)),square));
    auto time=D::Sub(projected,D::Div(0.5L,D::Root(square)));time=std::max(0.0L,std::min(1.0L,time));
    hit.time=D::Store(time);hit.actor=captured?0x5678u:0u;
    const auto position=[&](float p,float q){return D::Store(D::Add(p,D::Store(D::Mul(q,time))));};
    hit.location={position(start.x,delta.x),position(start.y,delta.y),position(start.z,delta.z)};
    if(captured)hit.normal=D::VectorByCoords(hit.normal,{captured->origin,captured->xAxis,captured->yAxis,captured->zAxis});
    else if(D::DotZYX({-delta.x,-delta.y,-delta.z},hit.normal)<0)hit.normal={-hit.normal.x,-hit.normal.y,-hit.normal.z};
    return {false,hit};
}
void Compare(const Model& m,Vec start,Vec end,unsigned flags=0,const Coords* c=nullptr) {
    if constexpr(!O::Available)return;
    auto actual=Seed();bool clear{};OracleResult expected;std::string ae,be;
    try{clear=Trace(actual,m,start,end,flags,c);}catch(const std::exception& e){ae=e.what();}
    try{expected=Oracle(m,start,end,flags,c);}catch(const std::exception& e){be=e.what();}
    Require(ae.empty()==be.empty(),"Point result/refusal differs from independent recursive x87");
    if(!ae.empty()){Require(Same(actual,Seed()),"Failed point arithmetic published a packet prefix");return;}
    Require(clear==expected.clear&&Same(actual,expected.hit),"Point packet Float bits differ from recursive x87");++comparisons;
}
void Literal() {
    auto m=One();auto h=Seed();Require(!Trace(h,m,{10,0,0},{-10,0,0}),"Literal plane crossing missed");
    Require(h.next==Seed().next&&h.actor==0u&&h.primitive==0x1234u&&h.item==0&&Bits(h.time,0.475f)&&
        Same(h.location,{0.5f,0,0})&&Same(h.normal,{1,0,0}),"Literal world point packet differs");
    auto c=Identity();h=Seed();Require(!Trace(h,m,{-10,0,0},{10,0,0},0,&c)&&Same(h.normal,{1,0,0}),
        "Owned start-solid normal was incorrectly faced against travel");
    h=Seed();Require(!Trace(h,m,{-10,0,0},{10,0,0})&&Same(h.normal,{-1,-0.0f,-0.0f}),
        "Null-owner start-solid normal failed native facing");
    h=Seed();Require(Trace(h,m,{10,0,0},{20,0,0})&&Same(h,Seed()),"Clear point wrote any caller field");
    h=Seed();Require(Trace(h,m,{-10,0,0},{10,0,0},16)&&Same(h,Seed()),"Initial-solid suppression invented a hit");
    Model reentry;reentry.rootOutside=0;reentry.nodes={Plane(),Plane({-1,0,0},-10)};reentry.nodes[0].front=1;
    h=Seed();Require(!Trace(h,reentry,{-5,0,0},{15,0,0},16)&&h.item==1&&Bits(h.time,0.725f)&&Same(h.location,{9.5f,0,0}),
        "Initial-solid flag suppressed later genuine reentry or lost LastNode");
    for(unsigned flags=0;flags<256;++flags)for(unsigned node=0;node<256;++node) {
        auto n=Plane();n.nodeFlags=static_cast<std::uint8_t>(node);
        Require(QuestVr::ModelPointLineDetail::Csg(n,flags,true)==((node&((flags&~16u)|33u))==0u),"Same-side CSG mask differs");
        Require(QuestVr::ModelPointLineDetail::Csg(n,flags,false)==((node&(flags|33u))==0u),"Crossing CSG mask differs");
    }
    for(unsigned flags=0;flags<256;++flags) {
        m.nodes[0].nodeFlags=static_cast<std::uint8_t>((flags*37u)&255u);
        for(const auto& pair:std::array<std::pair<float,float>,4>{{{10,-10},{-10,10},{10,20},{-10,-20}}})Compare(m,{pair.first,0,0},{pair.second,0,0},flags);
        Compare(reentry,{-5,0,0},{15,0,0},flags);
    }
    // A point-null dot must NOT reuse FastLine's Z/Y/X schedule.
    const auto cancellation=Plane({-1.0e20f,3.0f,1.0e20f});
    Require(Bits(QuestVr::ModelPointLineDetail::Distance({1,1,1},cancellation,nullptr),3.0f),
        "Null point dot lost its original (Z+X)+Y cancellation boundary");
    const auto negative=-.001f,positive=.001f;
    for(float a:{negative,std::nextafter(negative,0.0f),-0.0f,0.0f,std::nextafter(positive,0.0f),positive})
        for(float b:{-.002f,0.0f,.002f})Compare(One(),{a,0,0},{b,0,0});
}
void Coordinates() {
    const std::array<Coords,7> poses{{Identity(),{{4,-3,2},{1,0,0},{0,1,0},{0,0,1}},
        {{-2,4,1},{0,1,0},{-1,0,0},{0,0,1}},{{1,2,3},{0,0,1},{1,0,0},{0,1,0}},
        {{2,-1,3},{2,0,0},{0,.5f,0},{0,0,1.5f}},{{-3,2,1},{-1,0,0},{0,1,0},{0,0,-1}},
        {{3,1,-2},{1,.25f,0},{0,1,-.125f},{.125f,0,1}}}};
    Model slab;slab.rootOutside=1;slab.nodes={Plane({1,0,0},5),Plane({-1,0,0},5)};slab.nodes[0].back=1;
    std::uint32_t random=71173u;const auto number=[&] {random=random*1664525u+1013904223u;return static_cast<float>(static_cast<int>((random>>8u)%10001u)-5000)/128.0f;};
    for(const auto& c:poses)for(unsigned i=0;i<160;++i) {
        const Vec a{number(),number(),number()},b{number(),number(),number()};
        Compare(slab,a,b,i&31u,&c);Compare(slab,a,b,i&31u);
    }
    auto m=One();m.nodes[0]=Plane({.71875f,-1.21875f,.8125f},.03125f);
    for(const auto& c:poses)for(unsigned i=0;i<80;++i)Compare(m,{number(),number(),number()},{number(),number(),number()},i&31u,&c);
    auto cancellation=Identity();cancellation.origin={16777216.0f,0,0};m=One();m.nodes[0].planeW=.5f;
    Require(Bits(QuestVr::ModelPointLineDetail::Distance({-16777216.0f,0,0},m.nodes[0],&cancellation),-.5f),
        "Owned point W-dot was incorrectly spilled to Float before distance subtraction");
    Compare(m,{-16777216.0f,0,0},{-16777214.0f,0,0},0,&cancellation);
}
void CapturesAndRefusals() {
    auto m=One();auto h=Seed();unsigned captures{};const auto capture=[&]{++captures;return Identity();};
    Require(!QuestVr::CheckOriginalModelPoint(h,m,4,8,{-10,0,0},{10,0,0},0,capture)&&captures==2u,"Owner hit did not capture ToWorld exactly twice");
    h=Seed();captures=0;Require(QuestVr::CheckOriginalModelPoint(h,m,4,8,{20,0,0},{10,0,0},0,capture)&&captures==1u&&Same(h,Seed()),"Owner miss capture count/packet differs");
    const float nan=std::bit_cast<float>(0x7f800001u);Model empty;
    for(int outside:{0,1}) {
        empty.rootOutside=outside;h=Seed();captures=0;QuestVr::ModelPointTraceStats stats;
        Require(QuestVr::CheckOriginalModelPoint(h,empty,0,8,{nan,nan,nan},{nan,nan,nan},~0u,capture,{},&stats)==(outside!=0)&&
            captures==0u&&Same(h,Seed())&&stats.querySteps==0u,"Empty Model inspected inactive data/capture/packet");
    }
    h=Seed();captures=0;Refuses([&]{QuestVr::CheckOriginalModelPoint(h,m,4,8,{-10,0,0},{10,0,0},0,[&]{if(++captures==2u)throw std::runtime_error("second capture");return Identity();});});
    Require(captures==2u&&Same(h,Seed()),"Second ToWorld failure published partial point hit");
    h=Seed();captures=0;Require(!QuestVr::CheckOriginalModelPoint(h,m,4,8,{-10,0,0},{10,0,0},0,[&]{auto c=Identity();if(++captures==2u)c.origin={nan,nan,nan};return c;}),"Final normal transform read inactive Origin");
    h=Seed();captures=0;Require(!QuestVr::CheckOriginalModelPoint(h,m,4,8,{-10,0,0},{10,0,0},0,[&]{auto c=Identity();if(++captures==2u)c.xAxis.x=2.0f;return c;})&&
        captures==2u&&Same(h.normal,{2,0,0})&&Bits(h.time,.475f),"Hit normal reused first captured axes or normalized the second result");
    for(unsigned failure=0;failure<8;++failure) {
        auto broken=m;auto limits=Limits{};h=Seed();QuestVr::ModelPointTraceStats stats{123,456,789,987};
        if(failure==0)broken.nodes[0].back=10;
        if(failure==1)broken.nodes[0].front=0;
        if(failure==2)broken.nodes.push_back(Plane({nan,0,0}));
        if(failure==3)limits.validationSteps=1;
        if(failure==4)limits.querySteps=1;
        if(failure==5)limits.nodes=0;
        if(failure==6)broken.rootOutside=2;
        Refuses([&]{Trace(h,broken,failure==7?Vec{nan,0,0}:Vec{10,0,0},{-10,0,0},0,nullptr,limits,&stats);});
        Require(Same(h,Seed())&&stats.validationSteps==123u&&stats.querySteps==456u,"Refusal published packet/statistics");
    }
    h=Seed();Refuses([&]{Trace(h,m,{-1,0,0},{-1,0,0});});Require(Same(h,Seed()),"Blocked zero-length refusal published hit");
    h=Seed();Require(Trace(h,m,{1,0,0},{1,0,0})&&Same(h,Seed()),"Clear zero-length segment refused/changed packet");
    Model deep;deep.rootOutside=1;deep.nodes.resize(8192u,Plane());
    for(std::size_t i=0;i+1u<deep.nodes.size();++i)deep.nodes[i].front=static_cast<std::int32_t>(i+1u);
    h=Seed();QuestVr::ModelPointTraceStats used;Require(Trace(h,deep,{10,0,0},{20,0,0},0,nullptr,{},&used)&&
        Same(h,Seed())&&used.nodeVisits==8192u&&used.leafVisits==1u,"Deep iterative point walk lost nodes/packet or required native recursion");
}
}
int main() {
    try {Literal();Coordinates();CapturesAndRefusals();Require(comparisons>4000u||!O::Available,"Independent oracle coverage missing");
        std::cout<<"PASS original Model point caller/owner bridge: "<<checks<<" checks, "<<refusals<<" refusals, "<<comparisons
            <<" independent x87 recursive comparisons; exact packets/flags/ToWorld captures/PC64 and atomic failures. Not native engine lifetime, Move, falling or crate gameplay.\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL original Model point bridge: "<<e.what()<<'\n';return 1;}
}
