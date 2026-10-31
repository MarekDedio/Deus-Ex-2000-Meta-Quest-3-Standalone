#include "quest_brush_poly_filter.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
std::size_t checks{},refusals{};
void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F> void Refuse(F run){bool failed{};try{run();}catch(const std::exception&){failed=true;}Check(failed,"Missing filter refusal");++refusals;}
using Poly=QuestVr::OriginalBrushClipPolygon;
struct Insert {std::int32_t surface,node,coplanar,place;Poly poly;};
PortableModelNode Node(std::uint8_t flags=0x20u) {
    PortableModelNode n;n.planeX=1;n.nodeFlags=flags;n.vertexCount=4;n.front=n.back=n.plane=-1;return n;
}
Poly Triangle(float x){Poly p;p.base={17,23,31};p.normal={1,0,0};p.vertexCount=3;p.polyFlags=0x1234u;
    p.vertices[0]={x,0,0};p.vertices[1]={x,1,0};p.vertices[2]={x,0,1};return p;}
struct Harness {
    std::vector<PortableModelNode> nodes{Node()};std::vector<Insert> inserted;
    std::size_t work{},limit{100000},scratch{},scratchLimit{1u<<20u};
    std::function<void()> afterInsert;
    QuestVr::OriginalBrushPolyFilterQueries Queries(){return {
        [&](std::int32_t i){Check(i>=0&&static_cast<std::size_t>(i)<nodes.size(),"Read outside fixture");return nodes[static_cast<std::size_t>(i)];},
        [&](std::int32_t s,std::int32_t n,std::int32_t c,std::int32_t p,const Poly& v){inserted.push_back({s,n,c,p,v});if(afterInsert)afterInsert();},
        [&]{if(work==limit)throw std::runtime_error("fixture work");++work;},
        [&](std::size_t bytes){if(bytes>scratchLimit-scratch)throw std::runtime_error("fixture scratch");scratch+=bytes;}};}
    void Run(const Poly& p,bool outside=true,std::int32_t coplanar=-1,std::size_t depth=256){
        QuestVr::FilterOriginalBrushPolygon(19,0,coplanar,p,outside,p.normal,nodes.size(),Queries(),depth);}
};
void FlagsOutside(){
    for(unsigned flags=0;flags<256u;++flags)for(unsigned count:{0u,1u,255u})for(bool outside:{false,true})for(float x:{-2.0f,0.0f,2.0f}) {
        Harness h;h.nodes[0]=Node(static_cast<std::uint8_t>(flags));h.nodes[0].vertexCount=static_cast<std::uint8_t>(count);
        auto p=Triangle(x);h.Run(p,outside);
        // Axis-exact independent expected classification; no production numeric
        // helper supplies expected topology/outside/coplanar decisions.
        const bool front=(flags&0x40u)||(!(flags&0x80u)&&x>=0.0f);
        const bool csg=count>0u&&(flags&0x21u)==0u;
        const bool output=front?(outside||csg):(outside&&!csg);
        Check(h.inserted.size()==static_cast<std::size_t>(output),"Flags/CSG outside selection differs");
        if(output){const auto& r=h.inserted[0];Check(r.surface==19&&r.node==0&&r.place==(front?1:0),"Wrong front/back insertion arguments");
            Check(r.coplanar==((x==0.0f&&!(flags&0xc0u))?0:-1),"Coplanar carried index differs");
            Check(r.poly.base.x==17&&r.poly.polyFlags==0x1234u&&r.poly.vertexCount==3,"Nonsplit lost source fields");}
    }
    Harness opposite;auto p=Triangle(0);p.normal={-1,0,0};opposite.Run(p);
    Check(opposite.inserted.size()==1u&&opposite.inserted[0].place==1&&opposite.inserted[0].coplanar==-1,
        "Opposite coplanar did not take FRONT with unchanged carried index");
    Harness zero;auto zeroPoly=Triangle(0);zeroPoly.normal={0,0,0};zero.Run(zeroPoly);
    Check(zero.inserted[0].coplanar==0,"Zero normal dot did not replace coplanar index");
}
void SplitAndMutation(){
    Poly p=Triangle(-2);p.vertices[1]={2,1,0};
    Harness h;h.Run(p);
    Check(h.inserted.size()==2u&&h.inserted[0].place==1&&h.inserted[1].place==0,"Split front-before-back order differs");
    Check(h.inserted[0].poly.vertexCount==3&&h.inserted[1].poly.vertexCount==4,"Split fragment counts differ");
    for(const auto& r:h.inserted)for(std::int32_t i=0;i<r.poly.vertexCount;++i)
        Check(r.place?r.poly.vertices[static_cast<std::size_t>(i)].x>=0:r.poly.vertices[static_cast<std::size_t>(i)].x<=0,"Wrong fragment side");
    Harness changed;changed.nodes.push_back(Node(0xa0u));
    changed.afterInsert=[&]{if(changed.inserted.size()==1u){changed.nodes[0].back=1;changed.nodes[0].nodeFlags=0;}};
    changed.Run(p);
    Check(changed.inserted.size()==1u,"Did not reacquire mutated parent CSG after front insertion");
    Harness linked;linked.nodes.push_back(Node(0xa0u));
    linked.afterInsert=[&]{if(linked.inserted.size()==1u)linked.nodes[0].back=1;};linked.Run(p);
    Check(linked.inserted.size()==2u&&linked.inserted[1].node==1,"Did not reacquire newly linked Back child");
    Harness child;child.nodes.resize(3u,Node(0x40u));child.nodes[0]=Node();child.nodes[0].front=1;child.nodes[0].back=2;
    child.nodes[2].nodeFlags=0xa0u;child.Run(p);
    Check(child.inserted.size()==2u&&child.inserted[0].node==1&&child.inserted[1].node==2,"Split recursion and tail order differs");
    Harness rejected;rejected.Run(p,false);
    Check(rejected.inserted.empty(),"Outside=false NotCsg split inserted fragments");
    Harness csg;csg.nodes[0].nodeFlags=0;csg.Run(p,false);
    Check(csg.inserted.size()==1u&&csg.inserted[0].place==1,"Split CSG front/back outside differs");
}
void HalfAndLimits(){
    for(std::int32_t count=14;count<=16;++count){Harness h;h.nodes[0].nodeFlags=0x40u;
        Poly p=Triangle(5);p.vertexCount=count;for(std::int32_t i=0;i<count;++i)p.vertices[static_cast<std::size_t>(i)]={5,static_cast<float>(i),0};
        h.Run(p);Check(h.inserted.size()==2u,"Large polygon was not halved");const auto middle=count/2;
        Check(h.inserted[0].poly.vertices[0].y==middle&&h.inserted[0].poly.vertices[static_cast<std::size_t>(count-middle)].y==0,
            "Half copied output was not processed first");
        Check(h.inserted[1].poly.vertices[0].y==0&&h.inserted[1].poly.vertexCount==middle+1,"Mutated half sequence differs");
        Check((h.inserted[0].poly.polyFlags&0x80000000u)&&(h.inserted[1].poly.polyFlags&0x80000000u),"Half flags lost");
    }
    Harness tail;tail.nodes.resize(80u,Node(0x40u));for(std::size_t i=0;i+1u<tail.nodes.size();++i)tail.nodes[i].front=static_cast<std::int32_t>(i+1u);
    tail.Run(Triangle(2),true,-1,1);Check(tail.inserted.size()==1u&&tail.inserted[0].node==79,"Tails consumed recursive depth");
    Harness cycle;cycle.nodes[0].front=0;cycle.nodes[0].nodeFlags=0x40u;cycle.limit=23;Refuse([&]{cycle.Run(Triangle(1));});Check(cycle.work==23u,"Cycle did not consume caller work");
    Harness depth;Poly many=Triangle(5);many.vertexCount=14;Refuse([&]{depth.Run(many,true,-1,1);});
    Harness scratch;scratch.scratchLimit=1;Refuse([&]{scratch.Run(Triangle(1));});Check(scratch.inserted.empty(),"Unadmitted scratch inserted geometry");
    Harness malformed;malformed.nodes[0].front=2;malformed.nodes[0].nodeFlags=0x40u;Refuse([&]{malformed.Run(Triangle(1));});
    Harness numeric;numeric.nodes[0].planeX=std::numeric_limits<float>::quiet_NaN();Refuse([&]{numeric.Run(Triangle(1));});
    Harness ignored;ignored.nodes[0].nodeFlags=0xc0u;ignored.nodes[0].planeX=std::numeric_limits<float>::quiet_NaN();ignored.Run(Triangle(1));
    Check(ignored.inserted.size()==1u&&ignored.inserted[0].place==1,"Prefilter FRONT priority consumed inactive plane");
    auto q=ignored.Queries();q.scratch={};Refuse([&]{QuestVr::FilterOriginalBrushPolygon(1,0,-1,Triangle(1),true,{},1,q);});
    Refuse([&]{ignored.Run(Triangle(1),true,-1,0);});Refuse([&]{ignored.Run(Triangle(1),true,-1,257);});
    Harness empty;empty.nodes.clear();Refuse([&]{empty.Run(Triangle(1));});
    Harness coplanar;Refuse([&]{coplanar.Run(Triangle(1),true,1);});
    auto bad=Triangle(0);bad.vertexCount=17;Refuse([&]{coplanar.Run(bad);});
}
}
int main(){try{FlagsOutside();SplitAndMutation();HalfAndLimits();std::cout<<"PASS brush polygon filter checks="<<checks<<" refusals="<<refusals<<"; callback topology/order, not live tracker/startup\n";return 0;}
catch(const std::exception& e){std::cerr<<"FAIL brush polygon filter: "<<e.what()<<'\n';return 1;}}
