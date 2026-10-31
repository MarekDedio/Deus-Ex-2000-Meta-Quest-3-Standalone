#include "quest_brush_bsp_insert.h"
#include <iostream>
#include <stdexcept>

namespace {
std::size_t checks{},refusals{};
void Check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
template<class F>void Refuse(F f){bool failed{};try{f();}catch(const std::exception&){failed=true;}Check(failed,"Expected insertion refusal");++refusals;}
struct Owner {int id;};
PortableModelNode Node(){PortableModelNode n;n.front=n.back=n.plane=n.collisionBound=n.renderBound=-1;n.planeX=1;n.vertexCount=4;return n;}
QuestVr::OriginalBrushClipPolygon Polygon(){QuestVr::OriginalBrushClipPolygon p;p.normal={1,0,0};p.base={3,4,5};p.vertexCount=3;
    p.vertices[0]={3,0,0};p.vertices[1]={3,1,0};p.vertices[2]={3,0,1};return p;}
struct Harness {
    Owner owner{1};PortableModelGeometry model;QuestVr::OriginalBrushTrackerPools<Owner> pool;
    QuestVr::OriginalBrushRootDestination destination;std::int32_t root{-1};std::size_t work{},attached{},extraLimit{1u<<20u};
    Harness(){model.nodes.resize(3u,Node());model.surfaces.resize(1u);model.nodes[0].zone0=1;model.nodes[0].zone1=2;
        model.nodes[0].leaf0=7;model.nodes[0].leaf1=8;model.nodes[1].zone0=3;model.nodes[1].zone1=4;model.nodes[1].zoneMask=16;
        model.nodes[1].leaf0=9;model.nodes[1].leaf1=10;model.nodes[2].zoneMask=32;model.nodes[2].zone0=5;model.nodes[2].zone1=6;
        pool.original=QuestVr::OriginalBrushCounts(model);QuestVr::InitializeOriginalBrushPools(model,pool,PoolQueries());}
    QuestVr::OriginalBrushPoolQueries PoolQueries(){return {[&]{if(++work>100000u)throw std::runtime_error("work");},
        [&](std::size_t bytes){if(bytes>extraLimit)throw std::runtime_error("bytes");}};}
    QuestVr::OriginalBrushInsertionQueries Queries(){return {PoolQueries(),[&](std::int32_t index){
        if(index<0||static_cast<std::size_t>(index)>=model.nodes.size())throw std::runtime_error("index");
        if(static_cast<std::size_t>(index)>=pool.original.nodes&&!pool.nodes[static_cast<std::size_t>(index)-pool.original.nodes].owner)
            throw std::runtime_error("free node read");
        return model.nodes[static_cast<std::size_t>(index)];},
        [&](std::int32_t value){root=value;},[&](std::int32_t){++attached;}};}
    void Run(const QuestVr::OriginalBrushClipPolygon& p=Polygon(),std::int32_t n=0,std::int32_t c=-1,std::int32_t place=1){
        QuestVr::InsertOriginalBrushPolygon(model,pool,&owner,destination,0,n,c,place,p,Queries());}
};
void Stores(){for(std::uint32_t flags:{0u,1u,2u,8u,0x04000000u,0x0400000bu}){
    Harness h;auto p=Polygon();p.polyFlags=flags;h.Run(p);const auto& n=h.model.nodes[3];
    Check(h.root==3&&h.attached==1&&h.model.nodes[0].front==3&&h.destination.tail==3,"Missing original parent/root attachment");
    Check(n.planeX==1&&n.planeY==0&&n.planeZ==0&&n.planeW==3&&n.surface==0,"Plane/surface stores differ");
    Check(n.back==-1&&n.front==-1&&n.plane==-1&&n.renderBound==-1&&n.collisionBound==-1,"Node child/root initial stores differ");
    const auto expected=static_cast<std::uint8_t>(((flags&8u)?0x21u:0x20u)|((flags&0x04000001u)?4u:0u)|((flags&2u)?2u:0u));
    Check(n.nodeFlags==expected&&n.vertexCount==3&&n.zone0==2&&n.zone1==2&&n.leaf0==8&&n.leaf1==8,"Flags/count/zone inheritance differs");
    Check(n.zoneMask==0&&h.model.nodes[0].zoneMask==4u,"New mask was recopied after original node OR");
    for(std::size_t i=0;i<3u;++i){const auto& v=h.model.vertices[static_cast<std::size_t>(n.vertexPool)+i];
        Check(v.side==-1&&h.model.points[static_cast<std::size_t>(v.point)].x==3&&h.pool.points[static_cast<std::size_t>(v.point)-h.pool.postSetupPoints].owner==&h.owner,"Typed point/vertex stores differ");}
    h.Run(p,1,-1,0);Check(h.model.nodes[3].renderBound==4&&h.root==3&&h.model.nodes[1].back==4,"Mover chain confused with BSP plane link");
}}
void Coplanar(){Harness h;h.model.nodes[1].plane=2;h.Run(Polygon(),0,1,1);const auto& n=h.model.nodes[3];
    Check(h.pool.nodes[0].parent==2&&h.model.nodes[2].plane==3,"Coplanar did not select LAST T parent");
    Check(n.zoneMask==32&&n.zone0==2&&n.zone1==2&&n.leaf0==8&&h.model.nodes[0].zoneMask==4&&h.model.nodes[2].zoneMask==32,
        "Different C/N lost original P/N zone mask targets");
    for(bool reverse:{false,true}){Harness same;same.model.nodes[0].plane=1;auto p=Polygon();if(reverse)p.normal.x=-1;same.Run(p,0,0,0);
        const auto& row=same.model.nodes[3];Check(row.zone0==(reverse?4:3)&&row.zone1==(reverse?3:4)&&row.leaf0==(reverse?10:9),"Coplanar orientation inherited wrong last-T sides");
        Check(row.zoneMask==16&&same.model.nodes[0].zoneMask==24&&same.model.nodes[1].zoneMask==16,"Same initial C with later T mask target differs");}
}
void Failure(){Harness noNode;for(auto& row:noNode.pool.nodes)row={&noNode.owner,1};noNode.Run();
    Check(noNode.root==-1&&noNode.attached==0&&noNode.model.nodes[0].zoneMask==0,"Node exhaustion wrote geometry");
    Harness noVerts;for(auto& row:noVerts.pool.vertices)row=&noVerts.owner;noVerts.Run();const auto& n=noVerts.model.nodes[3];
    Check(noVerts.root==-1&&noVerts.attached==0&&!noVerts.pool.nodes[0].owner&&noVerts.pool.nodes[0].parent==0,"Vertex failure did not clear only owner pair");
    Check(n.vertexPool==-1&&n.planeW==3&&n.zone0==2&&noVerts.model.nodes[0].zoneMask==4,"Vertex failure discarded prior node/mask writes");
    Harness corrupt;corrupt.model.nodes[0].zone1=64;Refuse([&]{corrupt.Run();});
    Harness bad;Refuse([&]{bad.Run(Polygon(),0,-1,2);});
    Harness free;Refuse([&]{free.Run(Polygon(),3);});
    Harness bytes;bytes.extraLimit=1;Refuse([&]{bytes.Run();});Check(bytes.root==-1&&bytes.attached==0&&bytes.pool.nodes[0].owner==&bytes.owner,"Budget refusal was fabricated as native allocation failure");
}
}
int main(){try{Stores();Coplanar();Failure();std::cout<<"PASS brush BSP insertion checks="<<checks<<" refusals="<<refusals<<"; selected writes and failure controls, not live tracker/startup\n";return 0;}
catch(const std::exception& e){std::cerr<<"FAIL brush BSP insertion: "<<e.what()<<'\n';return 1;}}
