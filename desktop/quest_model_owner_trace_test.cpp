#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "quest_model_line_hit.h"
#include "quest_model_owner_trace_oracle.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
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
using Coords=QuestVr::OriginalModelCoords;
using Limits=QuestVr::ModelExtentTraceLimits;
using Hit=QuestVr::OriginalActorLineHit;
using Bytes=std::vector<std::uint8_t>;
namespace Oracle=QuestVr::ModelOwnerTraceOracle;
std::size_t checks{},refusals{},comparisons{},hits{},mathRefusals{};
void Require(bool value,const std::string& reason) {++checks;if(!value) throw std::runtime_error(reason);}
bool Bits(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec a,Vec b) {return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
bool Same(const Hit& a,const Hit& b) {return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&
    Same(a.location,b.location)&&Same(a.normal,b.normal)&&Bits(a.time,b.time)&&a.item==b.item;}
template<class Operation> void Refuses(Operation operation,const char* reason) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,reason);++refusals;
}
Coords Identity() {return {{},{1,0,0},{0,1,0},{0,0,1}};}
void U32(Bytes& b,std::uint32_t v) {for(unsigned s=0;s<32;s+=8) b.push_back(static_cast<std::uint8_t>(v>>s));}
void F32(Bytes& b,float v) {U32(b,std::bit_cast<std::uint32_t>(v));}
void V32(Bytes& b,Vec v) {F32(b,v.x);F32(b,v.y);F32(b,v.z);}
void Index(Bytes& b,std::int32_t v) {
    auto magnitude=static_cast<std::uint32_t>(v<0?-static_cast<std::int64_t>(v):v);
    auto first=static_cast<std::uint8_t>((magnitude&63u)|(v<0?128u:0u));magnitude>>=6u;
    b.push_back(static_cast<std::uint8_t>(first|(magnitude?64u:0u)));
    while(magnitude) {const auto next=static_cast<std::uint8_t>(magnitude&127u);magnitude>>=7u;
        b.push_back(static_cast<std::uint8_t>(next|(magnitude?128u:0u)));}
}
PortableModelNode Plane(Vec n,float w=0,std::uint8_t flags=0,std::uint8_t vertices=3) {
    PortableModelNode result;result.planeX=n.x;result.planeY=n.y;result.planeZ=n.z;result.planeW=w;
    result.nodeFlags=flags;result.vertexCount=vertices;
    result.back=result.front=result.plane=result.collisionBound=result.renderBound=result.leaf0=result.leaf1=-1;
    return result;
}
std::int32_t Hull(Model& m,const std::vector<std::int32_t>& refs,Vec low,Vec high) {
    const auto offset=static_cast<std::int32_t>(m.leafHulls.size());
    m.leafHulls.insert(m.leafHulls.end(),refs.begin(),refs.end());m.leafHulls.push_back(-1);
    for(const float f:{low.x,low.y,low.z,high.x,high.y,high.z}) m.leafHulls.push_back(std::bit_cast<std::int32_t>(f));
    return offset;
}
Model Box() {
    Model m;m.rootOutside=1;m.nodes={Plane({1,0,0}),Plane({-1,0,0},10),Plane({0,1,0},10),
        Plane({0,-1,0},10),Plane({0,0,1},10),Plane({0,0,-1},10)};
    m.nodes[0].collisionBound=Hull(m,{0,1,2,3,4,5},{-10,-10,-10},{0,10,10});return m;
}
// Independent serialized v68 bodies, routed through the production decoder.
// Not render triangles, spawn-overlap bounds or a second collision provider.
Model Decoded(const Model& m) {
    Bytes b;V32(b,{-100000,-100000,-100000});V32(b,{100000,100000,100000});b.push_back(1);
    V32(b,{});F32(b,100000);Index(b,3);V32(b,{1,0,0});V32(b,{0,1,0});V32(b,{0,0,1});
    Index(b,3);V32(b,{});V32(b,{1,0,0});V32(b,{0,1,0});Index(b,static_cast<std::int32_t>(m.nodes.size()));
    for(const auto& n:m.nodes) {
        V32(b,{n.planeX,n.planeY,n.planeZ});F32(b,n.planeW);
        U32(b,static_cast<std::uint32_t>(n.zoneMask));U32(b,static_cast<std::uint32_t>(n.zoneMask>>32u));b.push_back(n.nodeFlags);
        for(const auto value:{n.vertexPool,n.surface,n.back,n.front,n.plane,n.collisionBound,n.renderBound,n.zone0,n.zone1}) Index(b,value);
        b.push_back(n.vertexCount);U32(b,static_cast<std::uint32_t>(n.leaf0));U32(b,static_cast<std::uint32_t>(n.leaf1));
    }
    Index(b,1);Index(b,0);U32(b,0);for(const auto v:{0,0,1,2,-1,-1}) Index(b,v);
    b.insert(b.end(),4u,0);Index(b,0);Index(b,3);for(int i=0;i<3;++i) {Index(b,i);Index(b,-1);}
    U32(b,0);U32(b,0);Index(b,0);Index(b,0);Index(b,0);Index(b,0);
    Index(b,static_cast<std::int32_t>(m.leafHulls.size()));for(const auto w:m.leafHulls) U32(b,static_cast<std::uint32_t>(w));
    Index(b,0);Index(b,0);U32(b,static_cast<std::uint32_t>(m.rootOutside));U32(b,0);
    auto result=DecodePortableModel68Body(b,0,0);
    Require(result.bodyBytesConsumed==b.size()&&result.leafHulls==m.leafHulls&&result.nodes.size()==m.nodes.size(),
        "Owner fixture did not decode exact v68 native Model body");return result;
}
auto Trace(const Model& m,const Coords& c,Vec start,Vec end,Vec extent={1,1,1},const Limits& limits={}) {
    return QuestVr::TraceOriginalOwnedModelExtent(m,c,start,end,extent,limits);
}
bool MathError(const std::string& error) {
    return error.find("nonfinite")!=std::string::npos||error.find("overflow")!=std::string::npos||
        error.find("zero")!=std::string::npos;
}
void Compare(const Model& m,const Coords& c,Vec start,Vec end,Vec extent={1,1,1}) {
    if constexpr(!Oracle::Available) return;
    QuestVr::ModelExtentTraceResult actual;Oracle::Result expected;std::string aError,bError;
    try {actual=Trace(m,c,start,end,extent);} catch(const std::exception& e) {aError=e.what();}
    try {expected=Oracle::Trace(m,{c.origin,c.xAxis,c.yAxis,c.zAxis},start,end,extent);} catch(const std::exception& e) {bError=e.what();}
    Require(aError.empty()==bError.empty(),"Owner sweep/scalar refusal differs: "+aError+" vs "+bError);
    if(!aError.empty()) {
        Require(MathError(aError)&&MathError(bError),"Unexpected owner validation/budget refusal in oracle comparison");
        ++mathRefusals;return;
    }
    ++comparisons;if(actual.hit) ++hits;
    Require(actual.clear==expected.clear&&actual.hit==expected.hit&&actual.node==expected.node,"Owner sweep hit/clear/parent differs from scalar x87");
    Require(Bits(actual.time,expected.time)&&Same(actual.location,expected.location)&&Same(actual.normal,expected.normal),
        "Owner sweep Time/Location/Normal Float bits differ from scalar x87");
    const auto& a=actual.stats;const auto& b=expected.stats;
    Require(a.validationSteps==b.validationSteps&&a.querySteps==b.querySteps&&a.nodeVisits==b.nodeVisits&&
        a.leafVisits==b.leafVisits&&a.hullVisits==b.hullVisits&&a.planeTests==b.planeTests&&
        a.bevelPairs==b.bevelPairs&&a.bevelTests==b.bevelTests,"Owner walk/read/clip/bevel counters differ from independent recursion");
}
std::array<Coords,7> Poses() {
    return {{Identity(),{{4,-3,2},{1,0,0},{0,1,0},{0,0,1}},{{-2,4,1},{0,1,0},{-1,0,0},{0,0,1}},
        {{1,2,3},{0,0,1},{1,0,0},{0,1,0}},{{2,-1,3},{2,0,0},{0,0.5f,0},{0,0,1.5f}},
        {{-3,2,1},{-1,0,0},{0,1,0},{0,0,-1}},{{3,1,-2},{1,0.25f,0},{0,1,-0.125f},{0.125f,0,1}}}};
}
void Basics() {
    const auto m=Decoded(Box());
    const auto actual=Trace(m,Identity(),{10,0,0},{-20,0,0});
    Require(actual.hit&&!actual.clear&&actual.node==0&&Same(actual.normal,{1,0,0}),"Identity owner missed original solid box");
    for(const auto& c:Poses()) {
        Compare(m,c,{30,0,0},{-30,0,0});Compare(m,c,{0,30,0},{0,-30,0},{1,2,0.5f});
        Compare(m,c,{0,0,30},{0,0,-30},{0,1,2});Compare(m,c,{30,30,30},{-30,-30,-30});
        Compare(m,c,{30,30,30},{30,30,30});Compare(m,c,{-5,0,0},{-5,0,0});
    }
    auto emptyHull=Box();emptyHull.nodes[0].collisionBound=Hull(emptyHull,{}, {-10,-10,-10},{0,10,10});
    emptyHull=Decoded(emptyHull);
    const auto owner=Trace(emptyHull,Identity(),{10,0,0},{-20,0,0});
    const auto world=QuestVr::TraceOriginalModelExtent(emptyHull,{10,0,0},{-20,0,0},{1,1,1});
    Require(!owner.hit&&owner.time==2&&owner.stats.planeTests==0&&world.hit,
        "Owner invented null-owner bounds planes or bounds-only world path regressed");
    auto flipped=Box();flipped.nodes[0].planeX=-1;flipped.nodes[0].planeW=-0.0f;
    flipped.nodes[0].nodeFlags=1;flipped.rootOutside=0;
    flipped.nodes[0].collisionBound=Hull(flipped,{0x40000000,1,2,3,4,5},{-10,-10,-10},{0,10,10});
    flipped=Decoded(flipped);for(const auto& c:Poses()) Compare(flipped,c,{20,3,2},{-20,-3,-2});
    auto tetra=Box();tetra.nodes[0]=Plane({1,1,1},10);tetra.nodes[1]=Plane({1,-1,-1},10);
    tetra.nodes[2]=Plane({-1,1,-1},10);tetra.nodes[3]=Plane({-1,-1,1},10);
    tetra.nodes[0].collisionBound=Hull(tetra,{0,1,2,3},{-15,-15,-15},{15,15,15});tetra=Decoded(tetra);
    for(const auto& c:Poses()) for(const float x:{-20.0f,20.0f}) for(const float y:{-20.0f,20.0f})
        for(const float z:{-20.0f,20.0f}) Compare(tetra,c,{x,y,z},{-x,-y,-z},{1.25f,2.5f,0.75f});
    // Literal CSG(0), not caller flags: every node-flag bit/vertex count.
    for(unsigned flags=0;flags<256;++flags) for(const unsigned vertices:{0u,3u}) {
        auto f=m;f.nodes[0].nodeFlags=static_cast<std::uint8_t>(flags);f.nodes[0].vertexCount=static_cast<std::uint8_t>(vertices);
        Compare(f,Poses()[2],{20,3,2},{-20,-3,-2});
    }
    Coords singular=Identity();singular.yAxis=singular.xAxis;Compare(m,singular,{20,20,0},{-20,-20,0});
    singular={};Compare(m,singular,{20,20,0},{-20,-20,0});
    // Competing identical contact times expose near-first order and strict
    // interval rejection of the later tie, including the LAST hull parent.
    Model competing;competing.rootOutside=1;
    competing.nodes={Plane({0,1,0},0,1,0),Plane({1,0,0}),Plane({1,0,0})};
    competing.nodes[0].front=1;competing.nodes[0].back=2;
    for(int i=1;i<=2;++i) competing.nodes[i].collisionBound=Hull(competing,{i},{-10,-10,-10},{0,10,10});
    competing=Decoded(competing);
    Require(Trace(competing,Identity(),{10,-2,0},{-20,2,0}).node==2,"Owner back-first tied hull did not retain first winner");
    Require(Trace(competing,Identity(),{10,2,0},{-20,-2,0}).node==1,"Owner front-first tied hull did not retain first winner");
    for(const auto& c:Poses()) {
        Compare(competing,c,{10,-2,0},{-20,2,0});Compare(competing,c,{10,2,0},{-20,-2,0});
    }
}
void PacketAndSafety() {
    const auto m=Decoded(Box());const auto c=Identity();
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    const Hit seed{91,92,93,{nan,-0.0f,inf},{-inf,nan,-0.0f},nan,123456};
    for(const int outside:{0,1}) {
        Model empty;empty.rootOutside=outside;Hit packet=seed;
        Coords invalid{{nan,nan,nan},{inf,inf,inf},{nan,nan,nan},{inf,inf,inf}};
        Require(QuestVr::CheckOriginalModelExtent(packet,empty,0,999,&invalid,{nan,inf,nan},{nan,inf,nan},{0,-1,nan})==static_cast<bool>(outside)&&Same(packet,seed),
            "Empty Model read inactive Owner/Extent/Hit or changed caller packet");
        Compare(empty,invalid,{nan,inf,nan},{nan,inf,nan},{0,-1,nan});
    }
    Hit packet=seed;
    QuestVr::ModelExtentTraceStats publishedStats;
    Require(QuestVr::CheckOriginalModelExtent(packet,m,42,43,&c,{-20,0,0},{10,0,0},{1,1,1},{},&publishedStats)==false,
        "Caller packet adapter lost owner blocking result");
    const auto directStats=QuestVr::TraceOriginalOwnedModelExtent(m,c,{10,0,0},{-20,0,0},{1,1,1}).stats;
    Require(publishedStats.validationSteps==directStats.validationSteps&&publishedStats.querySteps==directStats.querySteps&&
        publishedStats.nodeVisits==directStats.nodeVisits&&publishedStats.hullVisits==directStats.hullVisits&&
        publishedStats.planeTests==directStats.planeTests,"Packet adapter did not export the same single trace work statistics");
    Require(packet.next==seed.next&&packet.item==seed.item&&packet.actor==43&&packet.primitive==42&&
        Same(packet.normal,{1,0,0})&&std::isfinite(packet.time)&&std::isfinite(packet.location.x),"Accepted owner packet fields are wrong");
    packet=seed;Require(QuestVr::CheckOriginalModelExtent(packet,m,42,0,nullptr,{-20,0,0},{10,0,0},{1,1,1})==false&&packet.actor==0&&packet.primitive==42&&packet.item==seed.item,
        "Null-owner accepted packet did not preserve caller Item and assign Model");
    // An accepted raw contact beyond the segment can clamp to Time1. Retail
    // returns clear but DOES publish the accepted hull packet in that case.
    Model beyond;beyond.rootOutside=0;beyond.nodes={Plane({0,1,0},0,1,0),Plane({1,0,0})};
    beyond.nodes[0].collisionBound=Hull(beyond,{1},{-10,-10,-10},{0,10,10});beyond=Decoded(beyond);
    packet=seed;
    Require(QuestVr::CheckOriginalModelExtent(packet,beyond,42,43,&c,{1.5f,0,0},{2.5f,0,0},{1,1,1})&&packet.time==1&&
        Same(packet.location,{1.5f,0,0})&&Same(packet.normal,{1,0,0})&&packet.actor==43&&packet.primitive==42&&
        packet.next==seed.next&&packet.item==seed.item,"Accepted Time1 clear was confused with a no-write miss");
    Compare(beyond,c,{2.5f,0,0},{1.5f,0,0});
    packet=seed;auto expected=seed;expected.time=2;
    Require(QuestVr::CheckOriginalModelExtent(packet,m,42,43,&c,{20,30,0},{10,30,0},{1,1,1})&&Same(packet,expected),
        "Nonempty owner miss changed packet fields other than Time2");
    for(const auto invalid:std::array<Coords,2>{{{{nan,0,0},{1,0,0},{0,1,0},{0,0,1}},{{},{inf,0,0},{0,1,0},{0,0,1}}}}) {
        packet=seed;Refuses([&] {QuestVr::CheckOriginalModelExtent(packet,m,42,43,&invalid,{-20,0,0},{10,0,0},{1,1,1});},"Nonfinite active owner coordinates accepted");
        Require(Same(packet,seed),"Coordinate refusal leaked original partial Time/normal writes");
    }
    for(const auto extent:std::array<Vec,4>{{{0,0,0},{-1,1,1},{1,-1,1},{1,1,nan}}}) {
        packet=seed;Refuses([&] {QuestVr::CheckOriginalModelExtent(packet,m,42,43,&c,{-20,0,0},{10,0,0},extent);},"Invalid nonempty extent accepted");
        Require(Same(packet,seed),"Extent refusal mutated caller packet");
    }
    for(unsigned mode=0;mode<3;++mode) {
        packet=seed;Refuses([&] {QuestVr::CheckOriginalModelExtent(packet,m,mode==0?0:42,mode==1?0:43,
            mode==2?nullptr:&c,{-20,0,0},{10,0,0},{1,1,1});},"Missing/mismatched actual owner/model identity accepted");
        Require(Same(packet,seed),"Identity refusal mutated packet");
    }
    // Owner MOV copies don't evaluate bbox floats; world clipping still does.
    auto unused=m;unused.leafHulls[7]=std::bit_cast<std::int32_t>(nan);
    unused.leafHulls[8]=std::bit_cast<std::int32_t>(inf);unused.leafHulls[12]=std::bit_cast<std::int32_t>(-inf);
    Compare(unused,c,{10,0,0},{-20,0,0});Require(Trace(unused,c,{10,0,0},{-20,0,0}).hit,"Inactive owner bbox numeric bits were read");
    Refuses([&] {QuestVr::TraceOriginalModelExtent(unused,{10,0,0},{-20,0,0},{1,1,1});},"Null-owner nonfinite bound admission regressed");
    unused=m;unused.leafHulls[7]=std::bit_cast<std::int32_t>(1000.0f);Compare(unused,c,{10,0,0},{-20,0,0});
    Require(Trace(unused,c,{10,0,0},{-20,0,0}).hit,"Owner rejected structurally present reversed unused bounds");
    unused.leafHulls.pop_back();Refuses([&] {Trace(unused,c,{10,0,0},{-20,0,0});},"Owner omitted structural six-word bound-tail admission");
    // The first primary plane would reject, but native already transformed
    // the entire list; the later Float overflow must not be hidden by laziness.
    Model eager;eager.rootOutside=0;eager.nodes={Plane({0,1,0},0,1,0),Plane({1,0,0}),
        Plane({std::numeric_limits<float>::max(),0,0})};
    eager.nodes[0].collisionBound=Hull(eager,{1,2},{-10,-10,-10},{0,10,10});eager=Decoded(eager);
    auto scaled=c;scaled.xAxis.x=2;packet=seed;
    Refuses([&] {QuestVr::CheckOriginalModelExtent(packet,eager,42,43,&scaled,{11,0,0},{10,0,0},{1,1,1});},
        "Owner clip rejection hid a later primary-plane transform overflow");
    Require(Same(packet,seed),"Eager transform refusal leaked a prefix caller packet");
    using Member=std::size_t Limits::*;
    for(const Member member:{&Limits::nodes,&Limits::hullWords,&Limits::validationSteps,&Limits::querySteps,&Limits::pending}) {
        for(unsigned mode=0;mode<2;++mode) {
            Limits limit;if(mode==0) limit.*member=0;else ++(limit.*member);
            Refuses([&] {Trace(m,c,{10,0,0},{-20,0,0},{1,1,1},limit);},"Owner expanded or disabled a fixed work limit");
        }
    }
    auto late=Limits{};late.querySteps=2;packet=seed;
    publishedStats.validationSteps=123456u;publishedStats.querySteps=654321u;
    Refuses([&] {QuestVr::CheckOriginalModelExtent(packet,m,42,43,&c,{-20,0,0},{10,0,0},{1,1,1},late,&publishedStats);},"Owner late work budget did not refuse");
    Require(Same(packet,seed),"Late owner refusal published prefix packet");
    Require(publishedStats.validationSteps==123456u&&publishedStats.querySteps==654321u,"Late refusal published partial statistics");
    auto deep=m;deep.nodes.insert(deep.nodes.begin(),2048u,Plane({1,0,0},0,1,0));
    for(auto& w:deep.leafHulls) {if(w==-1) break;w+=2048;}
    for(std::size_t i=0;i<2048;++i) deep.nodes[i].front=static_cast<std::int32_t>(i+1);
    Require(Trace(Decoded(deep),c,{20,0,0},{10,0,0}).stats.nodeVisits==2049,"Owner traversal used host recursion or lost deep chain");
}
std::uint32_t Next(std::uint32_t& value) {value^=value<<13u;value^=value>>17u;value^=value<<5u;return value;}
void Random() {
    if constexpr(!Oracle::Available) return;
    std::uint32_t seed=0x491fa129u;const auto m=Decoded(Box());
    for(unsigned i=0;i<2400;++i) {
        const auto n=[&] {return static_cast<float>(static_cast<int>(Next(seed)%2049)-1024)/16.0f;};
        const auto axis=[&] {return static_cast<float>(static_cast<int>(Next(seed)%33)-16)/8.0f;};
        const Coords c{{n(),n(),n()},{axis(),axis(),axis()},{axis(),axis(),axis()},{axis(),axis(),axis()}};
        const Vec start{n(),n(),n()},end{n(),n(),n()},extent{0.125f+std::fabs(axis()),0.125f+std::fabs(axis()),0.125f+std::fabs(axis())};
        Compare(m,c,start,end,extent);
    }
}
Bytes Read(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);Require(size<=256u*1024u*1024u,"Owned map capture exceeds fixed byte budget");
    std::ifstream stream(path,std::ios::binary);Require(static_cast<bool>(stream),"Owned map unavailable");
    Bytes b((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
    Require(!stream.bad()&&b.size()==size,"Owned map capture incomplete");return b;
}
void Owned(const std::filesystem::path& gameRoot) {
    Require(Oracle::Available,"Owned captured-owner test requires independent hardware x87 oracle");
    const auto path=gameRoot/"Maps"/"00_Training.dx";
    const auto bytes=Read(path);const auto stamp=std::filesystem::last_write_time(path);
    const auto package=LoadPortablePackageTables(path.string());const auto m=LoadPortableRootModel68(package);
    std::set<std::int32_t> offsets;for(const auto& n:m.nodes) if(n.collisionBound>=0) offsets.insert(n.collisionBound);
    Require(offsets.size()>=12,"Actual Training has insufficient captured native hulls");
    const std::vector<std::int32_t> ordered(offsets.begin(),offsets.end());const auto poses=Poses();
    const auto prior=comparisons,priorHits=hits;
    for(std::size_t i=0;i<12;++i) {
        auto cursor=static_cast<std::size_t>(ordered[i*(ordered.size()-1)/11]);std::size_t count{};
        while(m.leafHulls.at(cursor++)!=-1) Require(++count<=64,"Owned oracle hull exceeds bounded fixed storage");
        std::array<float,6> b{};for(auto& f:b) f=std::bit_cast<float>(m.leafHulls.at(cursor++));
        const Vec center{(b[0]+b[3])*0.5f,(b[1]+b[4])*0.5f,(b[2]+b[5])*0.5f};
        const float reach=std::max({b[3]-b[0],b[4]-b[1],b[5]-b[2]})+128;
        // Coordinates are explicit supplied fixture values, not falsely
        // reconstructed Actor.ToWorld. Real decoded BSP/hulls remain authority.
        Compare(m,poses[i%poses.size()],{center.x+reach,center.y,center.z},{center.x-reach,center.y,center.z},{2,3,4});
        Compare(m,Identity(),{center.x+reach,center.y,center.z},{center.x-reach,center.y,center.z},{2,3,4});
    }
    Require(comparisons-prior>=16&&hits-priorHits>=4,"Owned sweeps did not exercise enough real native contacts: finite="+
        std::to_string(comparisons-prior)+" hits="+std::to_string(hits-priorHits));
    Require(std::filesystem::last_write_time(path)==stamp&&Read(path)==bytes,"Owned map bytes/mtime changed");
    std::cout<<"PASS owned Training captured-coordinate sweeps: Model="<<m.objectPath<<" nodes="<<m.nodes.size()
        <<" hulls="<<offsets.size()<<" exactComparisons="<<comparisons-prior<<" hits="<<hits-priorHits
        <<"; no original executable, Actor transform or physics claim.\n";
}
} // namespace

int main(int argc,char** argv) {
    try {
        Basics();PacketAndSafety();Random();
        if(argc==2) Owned(argv[1]);else if(argc!=1) throw std::runtime_error("Usage: quest_model_owner_trace_test [GAME_ROOT]");
        Require(!Oracle::Available||comparisons>=2900,"Owner tests did not exercise enough independent sweeps");
        std::cout<<"PASS captured-owner Model extent/hit adapter: "<<checks<<" checks, "<<refusals<<" deliberate refusals, "
            <<comparisons<<" independent x87 sweeps, "<<hits<<" hits, "<<mathRefusals<<" paired finite-math refusals. Not live Move/falling.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL captured-owner Model extent: "<<e.what()<<'\n';return 1;}
}
