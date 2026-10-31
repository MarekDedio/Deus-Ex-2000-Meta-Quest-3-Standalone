#include "quest_brush_tracker_pools.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
std::size_t checks{},refusals{};
void Check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
template<class F>void Refuse(F f){bool failed{};try{f();}catch(const std::exception&){failed=true;}Check(failed,"Missing pool refusal");++refusals;}
struct Owner {int identity;};
using Pool=QuestVr::OriginalBrushTrackerPools<Owner>;
struct Harness {
    PortableModelGeometry model;Pool pool;std::size_t work{},limit{1000000},extra{},extraLimit{64u<<20u};
    QuestVr::OriginalBrushPoolQueries Queries(){return {[&]{if(work==limit)throw std::runtime_error("work");++work;},
        [&](std::size_t n){if(n>extraLimit)throw std::runtime_error("bytes");extra+=n;}};}
};
void Initialization(){
    for(std::size_t count:{0u,1u,13u,100u,32768u,65536u}){
        Harness h;h.model.nodes.resize(count);h.model.vertices.resize(7u);h.model.points.resize(5u);h.model.vectors.resize(4u);h.model.surfaces.resize(3u);
        h.pool.original=QuestVr::OriginalBrushCounts(h.model);
        h.model.points.resize(8u);h.model.vectors.reserve(99u);h.model.surfaces.reserve(99u);
        QuestVr::InitializeOriginalBrushPools(h.model,h.pool,h.Queries());
        const auto target=std::min(2u*count+512u,std::size_t{65536u});
        Check(h.model.nodes.size()==target&&h.model.nodes.capacity()==target&&h.pool.nodes.size()==target-count,"Actual node pool Num/Max differs");
        Check(h.model.vertices.size()==526u&&h.model.vertices.capacity()==526u&&h.pool.vertices.size()==519u,"Actual vertex Num/Max differs");
        Check(h.model.vectors.capacity()==4u&&h.model.surfaces.capacity()==3u&&h.pool.postSetupPoints==8u,"Setup shrink/count order differs");
        for(std::size_t i=count;i<target;++i){const auto& n=h.model.nodes[i];Check(n.back==0&&n.front==0&&n.plane==0&&n.collisionBound==0&&n.renderBound==0&&n.nodeFlags==0,"Free nodes were patched into valid authored nodes");}
        for(const auto& n:h.pool.nodes)Check(!n.owner&&n.parent==0,"Node owner pool not zero");
        Check(h.pool.nodeCursor==static_cast<std::int32_t>(count)&&h.pool.vertexCursor==7&&h.pool.freePoint==-1,"Initial cursors differ");
    }
    Harness tight;tight.pool.original=QuestVr::OriginalBrushCounts(tight.model);tight.extraLimit=1;
    Refuse([&]{QuestVr::InitializeOriginalBrushPools(tight.model,tight.pool,tight.Queries());});Check(tight.model.nodes.empty(),"Unadmitted pool allocated");
}
// Independent explicit cursor-address oracle: model the assembly's two scans,
// byte-owner tests and backwards writes, without production allocator helpers.
std::int32_t VertexOracle(std::vector<Owner*>& slots,std::int32_t base,std::int32_t& cursor,Owner* owner,std::int32_t count){
    const auto stop=cursor;std::int32_t consecutive=0;
    for(int pass=0;pass<2;++pass){const auto boundary=pass?stop:base+static_cast<std::int32_t>(slots.size());
        if(pass){cursor=base;consecutive=0;}
        while(cursor+count<boundary){const auto address=cursor-base;
            if(slots[static_cast<std::size_t>(address)]==nullptr){++consecutive;
                if(consecutive>=count){for(std::int32_t j=consecutive;j>0;--j)slots[static_cast<std::size_t>(address-(consecutive-j))]=owner;
                    return cursor-count+1;}}
            else consecutive=0;
            ++cursor;}
    }
    return -1;
}
void Allocation(){Owner a{1},b{2};
    for(std::size_t count=1;count<=9u;++count)for(std::size_t cursor=0;cursor<=count;++cursor)for(std::uint32_t mask=0;mask<(1u<<count);++mask)for(std::int32_t n=0;n<=5;++n){
        Harness h;h.model.vertices.resize(3u+count);h.pool.original.vertices=3u;h.pool.vertices.resize(count);h.pool.vertexCursor=static_cast<std::int32_t>(3u+cursor);
        for(std::size_t i=0;i<count;++i)if(mask&(1u<<i))h.pool.vertices[i]=&a;
        auto expected=h.pool.vertices;auto expectedCursor=h.pool.vertexCursor;
        const auto want=VertexOracle(expected,3,expectedCursor,&b,n);
        const auto got=QuestVr::AllocateOriginalBrushVertices(h.model,h.pool,&b,n,h.Queries());
        Check(want==got&&expectedCursor==h.pool.vertexCursor&&expected==h.pool.vertices,"Vertex strict/wrap/backwards owner semantics differ");
    }
    for(std::size_t count=0;count<9u;++count)for(std::size_t cursor=0;cursor<=count;++cursor)for(std::uint32_t mask=0;mask<(1u<<count);++mask){
        Harness h;h.model.nodes.resize(2u+count);h.pool.original.nodes=2u;h.pool.nodes.resize(count);h.pool.nodeCursor=static_cast<std::int32_t>(2u+cursor);
        for(std::size_t i=0;i<count;++i)if(mask&(1u<<i))h.pool.nodes[i]={&a,17};
        std::int32_t want=-1,expectedCursor=static_cast<std::int32_t>(2u+cursor);
        for(std::size_t i=cursor;i<count;++i)if(!(mask&(1u<<i))){want=static_cast<std::int32_t>(2u+i);break;}
        if(want==-1){expectedCursor=static_cast<std::int32_t>(2u+cursor);for(std::size_t i=0;i<cursor;++i)if(!(mask&(1u<<i))){want=static_cast<std::int32_t>(2u+i);break;}}
        if(want!=-1)expectedCursor=want;
        const auto got=QuestVr::AllocateOriginalBrushNode(h.model,h.pool,&b,0,h.Queries());
        Check(got==want&&h.pool.nodeCursor==expectedCursor,"Node wrap/cursor semantics differ");
        if(got!=-1)Check(h.pool.nodes[static_cast<std::size_t>(got)-2u].owner==&b&&h.pool.nodes[static_cast<std::size_t>(got)-2u].parent==0,"Node owner/parent missing");
    }
}
void PointsAndRefusals(){Owner a{1},b{2};Harness h;h.model.points.resize(4u);h.pool.postSetupPoints=4u;
    const auto first=QuestVr::AllocateOriginalBrushPoint(h.model,h.pool,&a,h.Queries());const auto second=QuestVr::AllocateOriginalBrushPoint(h.model,h.pool,&a,h.Queries());
    Check(first==4&&second==5&&h.model.points.size()==6u&&h.pool.points.size()==2u,"Point append order/count differs");
    h.model.points[4]={7,8,9};QuestVr::FreeOriginalBrushPoint(h.model,h.pool,first,&a,h.Queries());
    QuestVr::FreeOriginalBrushPoint(h.model,h.pool,second,&a,h.Queries());
    Check(h.pool.freePoint==5&&h.pool.points[1].freeNext==4&&h.pool.points[0].freeNext==-1,"Point free union/list order differs");
    Check(QuestVr::AllocateOriginalBrushPoint(h.model,h.pool,&b,h.Queries())==5&&QuestVr::AllocateOriginalBrushPoint(h.model,h.pool,&b,h.Queries())==4,"Point reuse is not LIFO");
    Check(h.model.points.size()==6u&&h.model.points[4].x==7&&h.pool.points[0].owner==&b&&h.pool.freePoint==-1,"Point reuse zeroed XYZ/lost owner");
    Refuse([&]{QuestVr::FreeOriginalBrushPoint(h.model,h.pool,4,&a,h.Queries());});
    Refuse([&]{QuestVr::FreeOriginalBrushPoint(h.model,h.pool,3,&b,h.Queries());});
    h.pool.freePoint=4;Refuse([&]{QuestVr::AllocateOriginalBrushPoint(h.model,h.pool,&b,h.Queries());});
    Harness broken;broken.model.nodes.resize(2u);broken.pool.original.nodes=1;Refuse([&]{QuestVr::AllocateOriginalBrushNode(broken.model,broken.pool,&a,0,broken.Queries());});
    Refuse([&]{QuestVr::AllocateOriginalBrushVertices(broken.model,broken.pool,&a,17,broken.Queries());});
    Harness noWork;noWork.limit=0;noWork.model.nodes.resize(1u);noWork.pool.nodes.resize(1u);Refuse([&]{QuestVr::AllocateOriginalBrushNode(noWork.model,noWork.pool,&a,0,noWork.Queries());});
    Harness tight;tight.extraLimit=1;Refuse([&]{QuestVr::AllocateOriginalBrushPoint(tight.model,tight.pool,&a,tight.Queries());});Check(tight.model.points.empty(),"Point allocated before byte admission");
}
}
int main(){try{Initialization();Allocation();PointsAndRefusals();std::cout<<"PASS brush tracker pools checks="<<checks<<" refusals="<<refusals<<"; exact allocation controls, not live tracker/startup\n";return 0;}
catch(const std::exception& e){std::cerr<<"FAIL brush tracker pools: "<<e.what()<<'\n';return 1;}}
