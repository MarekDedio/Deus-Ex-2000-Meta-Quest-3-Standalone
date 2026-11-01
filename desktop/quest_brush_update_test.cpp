#include "quest_brush_update.h"

#include <algorithm>
#include <bit>
#include <iostream>
#include <stdexcept>
#include <tuple>

namespace {
std::size_t checks{},refusals{};
void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F> void Refuse(F&& run,const char* expected="") {
    bool refused{};
    try{run();}catch(const std::exception& error){refused=std::string(error.what()).find(expected)!=std::string::npos;}
    Check(refused,"Expected selected brush-update refusal");++refusals;
}
struct Owner {int id{};bool assimilated{};std::int32_t root{-1};};
PortableModelNode Node(std::int32_t seed=0) {
    PortableModelNode n;n.front=n.back=n.plane=n.collisionBound=n.renderBound=-1;
    n.planeX=static_cast<float>(seed)+0.25f;n.planeY=-0.5f;n.planeZ=1.0f;n.planeW=3.5f;
    n.zoneMask=0x123456789abcdef0ull;n.nodeFlags=2u;n.surface=seed;n.vertexPool=1;
    n.zone0=3;n.zone1=4;n.leaf0=5;n.leaf1=6;return n;
}
auto NodeValue(const PortableModelNode& n) {
    return std::tuple{std::bit_cast<std::uint32_t>(n.planeX),std::bit_cast<std::uint32_t>(n.planeY),
        std::bit_cast<std::uint32_t>(n.planeZ),std::bit_cast<std::uint32_t>(n.planeW),n.zoneMask,n.nodeFlags,
        n.vertexPool,n.surface,n.back,n.front,n.plane,n.collisionBound,n.renderBound,n.zone0,n.zone1,n.vertexCount,n.leaf0,n.leaf1};
}
bool SamePoint(const PortableModelVec3& a,const PortableModelVec3& b) {
    return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)&&
        std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)&&
        std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z);
}
struct Harness {
    Owner a{1},b{2},c{3},d{4},e{5};
    PortableModelGeometry model;QuestVr::OriginalBrushTrackerPools<Owner> pool;
    std::vector<bool> released;std::vector<int> enqueued,offered;
    std::vector<std::int32_t> reads,releases,postReleaseReads;
    std::size_t work{},maximumWork{100000u},frames{},peakFrames{},maximumFrames{65536u},rootWrites{};
    std::function<void()> workHook;
    std::function<void(Owner*)> enqueueHook;
    std::function<void(std::int32_t)> releasedHook,postReleaseHook;
    std::function<void()> rootWriteHook;
    Harness() {
        for(std::int32_t i=0;i<9;++i)model.nodes.push_back(Node(i));
        model.vertices.resize(13u);model.points.resize(16u);model.surfaces.resize(9u);model.vectors.resize(4u);
        for(std::size_t i=0;i<model.points.size();++i)model.points[i]={static_cast<float>(i),-0.0f,static_cast<float>(i)+0.75f};
        for(std::size_t i=0;i<model.vertices.size();++i)model.vertices[i]={0,static_cast<std::int32_t>(i)+40};
        pool.original={model.surfaces.size(),model.vectors.size(),2u,1u,1u};pool.postSetupPoints=2u;
        pool.nodes.resize(model.nodes.size()-1u);pool.vertices.resize(model.vertices.size()-1u);
        pool.points.resize(model.points.size()-2u);pool.nodeCursor=3;pool.vertexCursor=5;released.resize(model.nodes.size());
    }
    void Step() {if(workHook)workHook();if(work++>=maximumWork)throw std::runtime_error("injected shared work refusal");}
    void Own(std::int32_t index,Owner& owner,std::int32_t parent=0,std::int32_t first=1,std::uint8_t count=0u,std::int32_t point=2) {
        const auto i=static_cast<std::size_t>(index);pool.nodes.at(i-1u)={&owner,parent};
        model.nodes.at(i).vertexPool=first;model.nodes.at(i).vertexCount=count;
        for(std::size_t v=0;v<count;++v) {
            const auto vertex=static_cast<std::size_t>(first)+v;const auto selected=static_cast<std::size_t>(point)+v;
            model.vertices.at(vertex).point=static_cast<std::int32_t>(selected);
            pool.vertices.at(vertex-1u)=&owner;pool.points.at(selected-2u).owner=&owner;
        }
    }
    PortableModelNode Read(std::int32_t index) {
        if(index<1||static_cast<std::size_t>(index)>=model.nodes.size())throw std::runtime_error("unowned selected node");
        const auto i=static_cast<std::size_t>(index);auto* owner=pool.nodes.at(i-1u).owner;
        if(!owner)throw std::runtime_error("free selected node");
        const auto& n=model.nodes.at(i);
        if(n.vertexPool<1||static_cast<std::size_t>(n.vertexPool)>model.vertices.size()||
            n.vertexCount>model.vertices.size()-static_cast<std::size_t>(n.vertexPool))throw std::runtime_error("bad selected vertex run");
        for(std::size_t v=0;v<n.vertexCount;++v) {
            const auto vertex=static_cast<std::size_t>(n.vertexPool)+v;const auto point=model.vertices.at(vertex).point;
            if(pool.vertices.at(vertex-1u)!=owner||point<2||static_cast<std::size_t>(point)>=model.points.size()||
                pool.points.at(static_cast<std::size_t>(point)-2u).owner!=owner)throw std::runtime_error("partially released selected node");
        }
        reads.push_back(index);return n;
    }
    void Enqueue(Owner* owner) {
        offered.push_back(owner->id);if(owner->assimilated)return;owner->assimilated=true;
        enqueued.insert(enqueued.begin(),owner->id);if(enqueueHook)enqueueHook(owner);
    }
    QuestVr::OriginalBrushDependencyQueries<Owner> DependencyQueries() {
        return {[this](std::int32_t index){return Read(index);},[this](Owner* owner){Enqueue(owner);},[this]{Step();},
            [this](std::size_t bytes){if(bytes>maximumFrames-frames)throw std::runtime_error("injected frame admission refusal");
                frames+=bytes;peakFrames=std::max(peakFrames,frames);},
            [this](std::size_t bytes){if(frames<bytes)std::terminate();frames-=bytes;}};
    }
    QuestVr::OriginalBrushPoolQueries PoolQueries() {
        return {[this]{Step();},[](std::size_t){throw std::runtime_error("unexpected flush allocation");}};
    }
    QuestVr::OriginalBrushFlushQueries<Owner> FlushQueries() {
        return {PoolQueries(),[this](std::int32_t index){return Read(index);},[](Owner* owner){return owner->root;},
            [this](Owner* owner,std::int32_t value){owner->root=value;++rootWrites;if(rootWriteHook)rootWriteHook();},
            [this](std::int32_t parent,std::int32_t child){
                if(child<1||pool.nodes.at(static_cast<std::size_t>(child)-1u).parent!=parent||parent<0||
                    static_cast<std::size_t>(parent)>=model.nodes.size()||
                    (parent!=0&&!pool.nodes.at(static_cast<std::size_t>(parent)-1u).owner&&!released.at(static_cast<std::size_t>(parent))))
                    throw std::runtime_error("unauthenticated recorded parent");
            },[this](std::int32_t index){
                const auto i=static_cast<std::size_t>(index);Check(!pool.nodes.at(i-1u).owner&&pool.nodes.at(i-1u).parent==0,"release callback preceded owner-pair clear");
                released.at(i)=true;releases.push_back(index);if(releasedHook)releasedHook(index);
            },[this](std::int32_t index){
                const auto i=static_cast<std::size_t>(index);if(!released.at(i)||pool.nodes.at(i-1u).owner)throw std::runtime_error("unauthenticated released node");
                postReleaseReads.push_back(index);if(postReleaseHook)postReleaseHook(index);
            }};
    }
    void Dependency(std::int32_t index=1,std::size_t depth=256u) {
        QuestVr::PropagateOriginalBrushDependency(model,pool,index,DependencyQueries(),depth);
    }
    void Flush(Owner& owner){QuestVr::FlushOriginalBrushMover(model,pool,&owner,FlushQueries());}
};
void DependencyOrder() {
    Harness h;h.Own(1,h.a);h.Own(2,h.b);h.Own(3,h.c);h.Own(4,h.d);h.Own(5,h.e);
    h.model.nodes[1].front=2;h.model.nodes[1].back=3;h.model.nodes[1].plane=4;h.model.nodes[2].front=5;
    h.Dependency();Check(h.offered==std::vector<int>({5,2,3,4,1}),"Dependency was not F/B/Plane postorder");
    Check(h.enqueued==std::vector<int>({1,4,3,2,5}),"Dependency pending prepend order differs");
    for(std::size_t i=1;i<=5u;++i)Check((h.model.nodes[i].nodeFlags&0x20u)!=0u,"Dependency visited flag missing");
    Check(h.frames==0u&&h.peakFrames==3u*256u,"Dependency recursive scratch lifetime differs");
    const auto offered=h.offered;const auto count=h.reads.size();h.Dependency();
    Check(h.offered==offered&&h.reads.size()==count+1u&&h.frames==0u,"Visited node did not return before children/owner");
    Harness duplicate;duplicate.Own(1,duplicate.a);duplicate.Own(2,duplicate.b);duplicate.Own(3,duplicate.b);
    duplicate.model.nodes[1].front=2;duplicate.model.nodes[1].back=3;duplicate.Dependency();
    Check(duplicate.offered==std::vector<int>({2,2,1})&&duplicate.enqueued==std::vector<int>({1,2}),"Assimilation and node visited guards were conflated");
    Harness assimilated;assimilated.Own(1,assimilated.a);assimilated.Own(2,assimilated.b);assimilated.Own(3,assimilated.e);
    assimilated.b.assimilated=true;assimilated.model.nodes[1].front=2;assimilated.model.nodes[2].front=3;assimilated.Dependency();
    Check(assimilated.enqueued==std::vector<int>({1,5})&&(assimilated.model.nodes[3].nodeFlags&0x20u),"Assimilated owner incorrectly pruned child traversal");
}
void DependencyLiveFieldsAndFailure() {
    Harness live;live.Own(1,live.a);live.Own(2,live.b);live.Own(3,live.c);live.model.nodes[1].front=2;
    live.enqueueHook=[&](Owner* owner){if(owner==&live.b)live.model.nodes[1].back=3;};live.Dependency();
    Check(live.offered==std::vector<int>({2,3,1}),"Dependency cached Back before front recursive return");
    Harness budget;budget.Own(1,budget.a);budget.maximumWork=0;Refuse([&]{budget.Dependency();},"shared work");
    Check(!(budget.model.nodes[1].nodeFlags&0x20u)&&budget.frames==0u,"Work refusal leaked frame or wrote visited flag early");
    Harness depth;depth.Own(1,depth.a);depth.Own(2,depth.b);depth.Own(3,depth.c);
    depth.model.nodes[1].front=2;depth.model.nodes[2].front=3;Refuse([&]{depth.Dependency(1,2u);},"depth");
    Check((depth.model.nodes[1].nodeFlags&0x20u)&&(depth.model.nodes[2].nodeFlags&0x20u)&&
        !(depth.model.nodes[3].nodeFlags&0x20u)&&depth.frames==0u&&depth.offered.empty(),"Depth refusal lost original partial visited writes");
    Harness bytes;bytes.Own(1,bytes.a);bytes.Own(2,bytes.b);bytes.model.nodes[1].front=2;bytes.maximumFrames=256u;
    Refuse([&]{bytes.Dependency();},"frame admission");
    Check((bytes.model.nodes[1].nodeFlags&0x20u)&&!(bytes.model.nodes[2].nodeFlags&0x20u)&&bytes.frames==0u,
        "Nested frame admission refusal released an unowned frame or discarded prior flag");
    Harness enqueue;enqueue.Own(1,enqueue.a);enqueue.enqueueHook=[](Owner*){throw std::runtime_error("after enqueue");};
    Refuse([&]{enqueue.Dependency();},"after enqueue");
    Check(enqueue.a.assimilated&&enqueue.enqueued==std::vector<int>({1})&&(enqueue.model.nodes[1].nodeFlags&0x20u)&&enqueue.frames==0u,
        "Dependency failure silently rolled back caller-owned partial actor/pending writes");
    Harness bad;bad.Own(1,bad.a);bad.model.nodes[1].front=0;Refuse([&]{bad.Dependency();},"dynamic pool");
    Check((bad.model.nodes[1].nodeFlags&0x20u)&&bad.frames==0u,"Invalid selected child discarded prior flag or leaked recursion");
    Refuse([&]{bad.Dependency(1,0u);},"depth bound");Refuse([&]{bad.Dependency(1,257u);},"depth bound");
    auto missing=bad.DependencyQueries();missing.enqueueIfUnassimilated={};
    Refuse([&]{QuestVr::PropagateOriginalBrushDependency(bad.model,bad.pool,1,missing);},"callbacks");
}
void FlushStores() {
    Harness h;h.Own(1,h.a,0,1,2u,2);h.Own(2,h.a,0,3,2u,4);h.a.root=1;
    h.model.nodes[0].front=1;h.model.nodes[0].back=2;h.model.nodes[1].renderBound=2;
    const auto nodes=h.model.nodes;const auto points=h.model.points;const auto vertices=h.model.vertices;
    const auto counts=QuestVr::OriginalBrushCounts(h.model);const auto nodeCursor=h.pool.nodeCursor,vertexCursor=h.pool.vertexCursor;
    h.Flush(h.a);Check(h.a.root==-1&&h.rootWrites==1u&&h.releases==std::vector<std::int32_t>({1,2})&&
        h.postReleaseReads==h.releases,"Flush root chain/order/postrelease read differs");
    Check(h.model.nodes[0].front==-1&&h.model.nodes[0].back==-1,"Flush did not unlink actual parent sides");
    for(std::size_t i=1;i<=2u;++i)Check(NodeValue(h.model.nodes[i])==NodeValue(nodes[i]),"Flush changed freed dynamic geometry bytes/flags/masks");
    for(std::size_t i=0;i<points.size();++i)Check(SamePoint(h.model.points[i],points[i]),"Flush changed point XYZ bits");
    for(std::size_t i=0;i<vertices.size();++i)Check(h.model.vertices[i].point==vertices[i].point&&h.model.vertices[i].side==vertices[i].side,"Flush changed vertex bytes");
    for(std::size_t i=0;i<4u;++i)Check(!h.pool.vertices[i]&&!h.pool.points[i].owner,"Flush retained typed vertex/point owners");
    Check(h.pool.freePoint==5&&h.pool.points[3].freeNext==4&&h.pool.points[2].freeNext==3&&
        h.pool.points[1].freeNext==2&&h.pool.points[0].freeNext==-1,"Ascending point release did not produce original LIFO chain");
    Check(h.pool.nodeCursor==nodeCursor&&h.pool.vertexCursor==vertexCursor,"Flush reset allocation cursors");
    const auto after=QuestVr::OriginalBrushCounts(h.model);
    Check(std::tie(after.surfaces,after.vectors,after.points,after.nodes,after.vertices)==
        std::tie(counts.surfaces,counts.vectors,counts.points,counts.nodes,counts.vertices),"Flush shrank Model arrays");
    Refuse([&]{static_cast<void>(h.Read(1));},"free selected");
    const auto reused=QuestVr::AllocateOriginalBrushPoint(h.model,h.pool,&h.b,h.PoolQueries());
    Check(reused==5&&h.pool.freePoint==4&&SamePoint(h.model.points[5],points[5]),"Point reuse lost free-next or retained XYZ");
}
void FlushParentAndReadPhases() {
    for(unsigned branch=0;branch<4u;++branch) {
        Harness h;h.Own(1,h.a);h.a.root=1;
        auto& parent=h.model.nodes[0];parent.front=branch==0u?1:-1;parent.back=branch<=1u?1:-1;parent.plane=branch<=2u?1:-1;
        h.Flush(h.a);
        Check(parent.front==-1&&parent.back==(branch==0u?1:-1)&&parent.plane==(branch<=1u?1:-1),"Flush did not use single Front-else-Back-else-Plane unlink");
    }
    Harness released;released.Own(1,released.a);released.Own(2,released.b,1);released.a.root=1;released.b.root=2;
    released.model.nodes[0].front=1;released.model.nodes[1].back=2;released.Flush(released.a);released.Flush(released.b);
    Check(released.model.nodes[1].back==-1&&released.releases==std::vector<std::int32_t>({1,2}),"Previously released same-pass parent could not be selected");
    Harness foreign;foreign.Own(2,foreign.b,1);foreign.b.root=2;Refuse([&]{foreign.Flush(foreign.b);},"recorded parent");
    Check(foreign.pool.nodes[1].owner==&foreign.b&&foreign.b.root==2,"Unauthenticated free parent was mutated");
    Harness post;post.Own(1,post.a);post.Own(2,post.a);post.a.root=1;post.model.nodes[0].front=1;post.model.nodes[0].back=2;
    post.postReleaseHook=[&](std::int32_t index){if(index==1)post.model.nodes[1].renderBound=2;};post.Flush(post.a);
    Check(post.releases==std::vector<std::int32_t>({1,2}),"Flush cached root-chain next before owner release");
    Harness empty;empty.pool.nodes.clear();empty.pool.vertices.clear();empty.Flush(empty.a);
    Check(empty.rootWrites==1u&&empty.reads.empty()&&empty.releases.empty(),"Root -1 short path inspected invalid pool contents or skipped store");
}
void FlushPartialFailure() {
    Harness point;point.Own(1,point.a,0,1,2u,2);point.a.root=1;point.model.nodes[0].front=1;
    point.workHook=[&]{if(!point.pool.points[0].owner)throw std::runtime_error("after first point");};
    Refuse([&]{point.Flush(point.a);},"after first point");
    Check(point.model.nodes[0].front==-1&&!point.pool.points[0].owner&&point.pool.points[1].owner==&point.a&&
        point.pool.vertices[0]==&point.a&&point.pool.nodes[0].owner==&point.a&&point.a.root==1,"Point fault discarded or overran selected partial writes");
    Harness vertex;vertex.Own(1,vertex.a,0,1,2u,2);vertex.a.root=1;vertex.model.nodes[0].front=1;
    vertex.workHook=[&]{if(!vertex.pool.vertices[0])throw std::runtime_error("after first vertex");};
    Refuse([&]{vertex.Flush(vertex.a);},"after first vertex");
    Check(!vertex.pool.points[0].owner&&!vertex.pool.points[1].owner&&!vertex.pool.vertices[0]&&
        vertex.pool.vertices[1]==&vertex.a&&vertex.pool.nodes[0].owner==&vertex.a,"Vertex fault did not retain exact ascending-release prefix");
    Harness node;node.Own(1,node.a);node.a.root=1;node.model.nodes[0].front=1;
    node.releasedHook=[](std::int32_t){throw std::runtime_error("after node release");};Refuse([&]{node.Flush(node.a);},"after node release");
    Check(!node.pool.nodes[0].owner&&node.pool.nodes[0].parent==0&&node.a.root==1&&node.postReleaseReads.empty(),"Node-release fault invented internal rollback or next read");
    Harness root;root.Own(1,root.a);root.a.root=1;root.model.nodes[0].front=1;
    root.rootWriteHook=[](){throw std::runtime_error("after root write");};Refuse([&]{root.Flush(root.a);},"after root write");
    Check(root.a.root==-1&&!root.pool.nodes[0].owner,"Root fault discarded caller-owned completed writes");
    Harness work;work.Own(1,work.a);work.a.root=1;work.maximumWork=0;Refuse([&]{work.Flush(work.a);},"shared work");
    Check(work.pool.nodes[0].owner==&work.a&&work.a.root==1,"Initial flush work refusal mutated owners");
    Harness owner;owner.Own(1,owner.b);owner.a.root=1;Refuse([&]{owner.Flush(owner.a);},"foreign/free mover");
    Harness negative;negative.a.root=-2;Refuse([&]{negative.Flush(negative.a);},"dynamic pool");
    auto missing=negative.FlushQueries();missing.admitReleasedNode={};
    Refuse([&]{QuestVr::FlushOriginalBrushMover(negative.model,negative.pool,&negative.a,missing);},"callbacks");
}
} // namespace
int main(){try{
    DependencyOrder();DependencyLiveFieldsAndFailure();FlushStores();FlushParentAndReadPhases();FlushPartialFailure();
    std::cout<<"PASS original brush dependency/flush checks="<<checks<<" refusals="<<refusals
        <<"; typed mutation helper ordering/partial writes only, NOT runtime journals/noninitial gameplay/startup/headset\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL original brush dependency/flush: "<<error.what()<<'\n';return 1;}}
