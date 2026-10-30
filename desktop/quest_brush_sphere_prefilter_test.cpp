#include "quest_brush_sphere_prefilter.h"

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
#include <vector>

// Independent detached hardware oracle. This never executes original modules
// or invokes production arithmetic in its expected path. Mutable arrays below
// are explicit test-owned capability/transaction fixtures, not live Model proof.
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace {
using Node=PortableModelNode;
using Sphere=PortableModelSphere;
using Queries=QuestVr::OriginalBrushSphereFilterQueries;
std::size_t checks{},comparisons{},explicitRefusals{},controls{};
void Require(bool good,const std::string& why){++checks;if(!good)throw std::runtime_error(why);}
std::uint32_t Bits(float v){return std::bit_cast<std::uint32_t>(v);}
bool Same(float a,float b){return Bits(a)==Bits(b);}
bool Same(const Node& a,const Node& b){return Same(a.planeX,b.planeX)&&Same(a.planeY,b.planeY)&&Same(a.planeZ,b.planeZ)&&Same(a.planeW,b.planeW)&&
    a.zoneMask==b.zoneMask&&a.nodeFlags==b.nodeFlags&&a.vertexPool==b.vertexPool&&a.surface==b.surface&&a.back==b.back&&a.front==b.front&&a.plane==b.plane&&
    a.collisionBound==b.collisionBound&&a.renderBound==b.renderBound&&a.zone0==b.zone0&&a.zone1==b.zone1&&a.vertexCount==b.vertexCount&&a.leaf0==b.leaf0&&a.leaf1==b.leaf1;}
bool Same(const std::vector<Node>& a,const std::vector<Node>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!Same(a[i],b[i]))return false;return true;}
template<class F>void Refuse(F run,const char* label){bool refused{};try{run();}catch(const std::exception&){refused=true;}
    Require(refused,std::string("Invalid selected prefilter input succeeded: ")+label);++explicitRefusals;}

namespace Hw {
using Real=long double;
struct Precision {unsigned short old{};Precision(){static_assert(std::numeric_limits<Real>::digits==64,"Sphere oracle needs actual x87PC64");
    asm volatile("fnstcw %0":"=m"(old));const auto mode=static_cast<unsigned short>((old&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
    ~Precision(){asm volatile("fldcw %0"::"m"(old));}Precision(const Precision&)=delete;Precision& operator=(const Precision&)=delete;};
Real Read(float value){if(!std::isfinite(value))throw std::runtime_error("oracle active Float nonfinite");return value;}
Real Add(Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,z=x*y;return z;}
Real Neg(Real value){Real result;asm volatile("fldt %1; fchs; fstpt %0":"=m"(result):"m"(value):"st");return result;}
float Store(Real value){float result;asm volatile("fldt %1; fstps %0":"=m"(result):"m"(value):"st");return result;}
Real Distance(const Node& n,const Sphere& s,bool helper){
    if(helper)return Sub(Add(Add(Mul(Read(n.planeY),Read(s.y)),Mul(Read(n.planeZ),Read(s.z))),Mul(Read(n.planeX),Read(s.x))),Read(n.planeW));
    return Sub(Add(Add(Mul(Read(n.planeZ),Read(s.z)),Mul(Read(n.planeY),Read(s.y))),Mul(Read(s.x),Read(n.planeX))),Read(n.planeW));
}
} // namespace Hw

struct Fixture {
    std::vector<Node> nodes;
    std::vector<bool> owned;
    std::vector<std::pair<std::int32_t,std::uint8_t>> writes;
    std::vector<std::int32_t> reads;
    std::size_t work{},workLimit{100000u};
    bool retargetFront{},retargeted{},throwAfterClear{};
    explicit Fixture(std::vector<Node> source):nodes(std::move(source)),owned(nodes.size(),true){}
    void Work(){if(++work>workLimit)throw std::runtime_error("caller shared native work exhausted");}
    Node Read(std::int32_t i){if(i<0||static_cast<std::size_t>(i)>=nodes.size()||!owned[static_cast<std::size_t>(i)])throw std::runtime_error("fixture unowned/free or invalid live node");
        reads.push_back(i);return nodes[static_cast<std::size_t>(i)];}
    void Write(std::int32_t i,std::uint8_t flags){if(i<0||static_cast<std::size_t>(i)>=nodes.size()||!owned[static_cast<std::size_t>(i)])throw std::runtime_error("fixture invalid flags write");
        nodes[static_cast<std::size_t>(i)].nodeFlags=flags;writes.emplace_back(i,flags);
        if(retargetFront&&!retargeted&&i==1){nodes[0].front=2;retargeted=true;}
        if(throwAfterClear&&writes.size()==1u)throw std::runtime_error("injected caller failure after authoritative flag write");}
    Queries Callbacks(){return {[this](std::int32_t i){return Read(i);},[this](std::int32_t i,std::uint8_t f){Write(i,f);},[this]{Work();}};}
};

void Oracle(std::size_t count,const Sphere& sphere,Fixture& fixture,std::size_t maximumDepth=256u){
    if(!count)return;
    if(!maximumDepth||maximumDepth>256u)throw std::runtime_error("oracle invalid caller recursion bound");
    const auto check=[&](std::int32_t i){if(i<0||static_cast<std::size_t>(i)>=count)throw std::runtime_error("oracle selected link outside Nodes.Num");};
    const auto edge=[&](std::int32_t i,bool front){fixture.Work();const auto n=fixture.Read(i);const auto child=front?n.front:n.back;
        if(child!=-1)check(child);
        return child;};
    const auto visit=[&](auto&& self,std::int32_t index,bool helper,std::size_t depth)->void{
        if(depth>maximumDepth)throw std::runtime_error("oracle caller recursion exhausted");
        while(index!=-1){fixture.Work();check(index);const auto node=fixture.Read(index);const auto low=static_cast<std::uint8_t>(node.nodeFlags&0x3fu);
            fixture.Write(index,low);const auto distance=Hw::Distance(node,sphere,helper);const auto radius=Hw::Read(sphere.radius);
            if(distance<Hw::Neg(radius)){fixture.Write(index,static_cast<std::uint8_t>(low|0x80u));index=edge(index,false);}
            else if(distance>radius){fixture.Write(index,static_cast<std::uint8_t>(low|0x40u));index=edge(index,true);}
            else{const auto back=edge(index,false);if(back!=-1)self(self,back,true,depth+1u);index=edge(index,true);}
        }
    };
    visit(visit,0,false,1u);
}

Node Leaf(float x=1,float y=0,float z=0,float w=0,std::uint8_t flags=0xe5u){Node n;n.planeX=x;n.planeY=y;n.planeZ=z;n.planeW=w;n.nodeFlags=flags;n.front=-1;n.back=-1;
    // Poison fields the prefilter does not consume. Their exact preservation
    // prevents accidentally substituting a broad all-model validation/reset.
    n.plane=std::numeric_limits<std::int32_t>::max();n.collisionBound=-800;n.renderBound=937;n.vertexPool=-3;n.surface=889;n.zoneMask=0xfedcba9876543210ull;
    n.zone0=72;n.zone1=81;n.vertexCount=0;n.leaf0=-912;n.leaf1=1800;return n;}
void Compare(std::vector<Node> nodes,const Sphere& sphere,const char* label,std::size_t maximumDepth=256u){Fixture actual(nodes),expected(std::move(nodes));bool failedA{},failedE{};
    try{const auto q=actual.Callbacks();QuestVr::PrecomputeOriginalBrushSphereFilter(actual.nodes.size(),sphere,q,maximumDepth);}catch(const std::exception&){failedA=true;}
    try{Oracle(expected.nodes.size(),sphere,expected,maximumDepth);}catch(const std::exception&){failedE=true;}
    Require(failedA==failedE,std::string("Independent sphere prefilter refusal differs: ")+label);
    Require(actual.writes==expected.writes,std::string("Sphere prefilter visit/write order differs: ")+label);
    Require(Same(actual.nodes,expected.nodes),std::string("Sphere prefilter flags/untouched fields differ: ")+label);
    ++comparisons;
}

void Classifications(){
    const auto nextUp=std::nextafter(1.0f,std::numeric_limits<float>::infinity()),nextDown=std::nextafter(-1.0f,-std::numeric_limits<float>::infinity());
    for(const auto d:{nextDown,-1.0f,std::nextafter(-1.0f,0.0f),-0.0f,0.0f,std::nextafter(1.0f,0.0f),1.0f,nextUp})
        for(const auto radius:{1.0f,0.0f,-0.0f,-1.0f})Compare({Leaf()},Sphere{d,0,0,radius},"Strict sphere boundaries/radius priority");
    for(unsigned flags=0;flags<256u;++flags){for(const auto x:{-2.0f,0.0f,2.0f})Compare({Leaf(1,0,0,0,static_cast<std::uint8_t>(flags))},Sphere{x,0,0,1},"Every low-six/previous-prefilter flag combination");}
    Fixture negative({Leaf()});const auto q=negative.Callbacks();QuestVr::PrecomputeOriginalBrushSphereFilter(1u,{0,0,0,-1},q);
    Require(negative.writes==std::vector<std::pair<std::int32_t,std::uint8_t>>{{0,0x25u},{0,0xa5u}},"Negative radius lost strict back-first priority");++controls;
    Fixture exact({Leaf()});const auto eq=exact.Callbacks();QuestVr::PrecomputeOriginalBrushSphereFilter(1u,{1,0,0,1},eq);
    Require(exact.writes.size()==1u&&exact.nodes[0].nodeFlags==0x25u,"Sphere equality was made one-sided");
    Require(exact.work==3u,"Overlap leaf must charge visit and both selected terminal edges");
}

void Traversal(){auto root=Leaf(0,0,0,0),back=Leaf(0,0,0,0,0xc2),front=Leaf(0,0,0,0,0xd4),tail=Leaf(0,0,0,0,0xef);
    root.back=1;root.front=2;back.front=3;std::vector<Node> tree{root,back,front,tail};Compare(tree,{0,0,0,0},"Back recursive first then front tail");
    Fixture f(tree);const auto q=f.Callbacks();QuestVr::PrecomputeOriginalBrushSphereFilter(f.nodes.size(),{0,0,0,0},q);
    const std::vector<std::pair<std::int32_t,std::uint8_t>> order{{0,0x25},{1,0x02},{3,0x2f},{2,0x14}};
    Require(f.writes==order,"Prefilter did not preserve native back-first visitation order");
    // Shared children are not a cycle and are deliberately revisited/recleared.
    root.back=1;root.front=1;Fixture dag({root,back,front,tail});const auto dq=dag.Callbacks();QuestVr::PrecomputeOriginalBrushSphereFilter(dag.nodes.size(),{0,0,0,0},dq);
    Require(std::count_if(dag.writes.begin(),dag.writes.end(),[](const auto& p){return p.first==1;})==2,"Shared subtree was deduplicated");
    Compare({root,back,front,tail},{0,0,0,0},"DAG repeated selected edges");
    // An unselected malformed sibling and a free slot remain untouched; this
    // is NOT permission for the public raw-source kernels to skip validation.
    root=Leaf();root.back=999;root.front=-1;auto free=Leaf();free.planeX=std::numeric_limits<float>::quiet_NaN();free.back=0;free.front=0;
    Fixture pruned({root,free});pruned.owned[1]=false;const auto pq=pruned.Callbacks();QuestVr::PrecomputeOriginalBrushSphereFilter(2u,{3,0,0,1},pq);
    Require(pruned.nodes[1].nodeFlags==free.nodeFlags&&pruned.reads==std::vector<std::int32_t>(pruned.reads.size(),0),"Unreachable free/malformed slot was scanned");
    // The previously copied Front link is invalid. Back traversal changes the
    // authoritative parent before its front edge is actually read.
    root=Leaf(0,0,0,0);root.back=1;root.front=999;back=Leaf(0,0,0,0);front=Leaf(0,0,0,0);
    Fixture mutableA({root,back,front}),mutableE({root,back,front});mutableA.retargetFront=true;mutableE.retargetFront=true;
    const auto mq=mutableA.Callbacks();QuestVr::PrecomputeOriginalBrushSphereFilter(3u,{0,0,0,0},mq);Oracle(3u,{0,0,0,0},mutableE);
    Require(mutableA.writes==mutableE.writes&&Same(mutableA.nodes,mutableE.nodes)&&mutableA.retargeted,"Parent Front was not reacquired after back callback");++controls;
    // All-front tail calls do not consume recursive depth.
    std::vector<Node> chain(700u,Leaf());for(std::size_t i=0;i+1u<chain.size();++i)chain[i].front=static_cast<std::int32_t>(i+1u);
    Fixture linear(chain);const auto lq=linear.Callbacks();QuestVr::PrecomputeOriginalBrushSphereFilter(chain.size(),{3,0,0,1},lq,1u);
    Require(linear.writes.size()==2u*chain.size()&&linear.work==2u*chain.size(),"Tail chain consumed recursion depth or skipped selected-edge work");
    Compare(chain,{3,0,0,1},"Long tail under depth1",1u);
}

void Arithmetic(){
    // A Float distance spill would round16777217 to16777216 and classify as
    // overlap instead of front. Exercise both the root and helper bodies.
    auto node=Leaf(1,1,0,0);const Sphere sphere{16777216.0f,1,0,16777216.0f};
    const auto retained=Hw::Distance(node,sphere,false);
    const auto spilled=Hw::Store(retained);
    Require(retained>sphere.radius&&spilled==sphere.radius,"Retained-distance discriminator ineffective");++controls;
    Fixture root({node});const auto rq=root.Callbacks();QuestVr::PrecomputeOriginalBrushSphereFilter(1u,sphere,rq);
    Require((root.nodes[0].nodeFlags&0xc0u)==0x40u,"Root distance was Float-spilled");
    auto parent=Leaf(0,0,0,0);parent.back=1;Compare({parent,node},sphere,"Helper retained distance (YZ)+X");
    Fixture helper({parent,node});const auto hq=helper.Callbacks();QuestVr::PrecomputeOriginalBrushSphereFilter(2u,sphere,hq);
    Require((helper.nodes[1].nodeFlags&0xc0u)==0x40u,"Recursive helper distance was Float-spilled");
    for(const auto a:{0.0f,-0.0f})for(const auto b:{0.0f,-0.0f})for(const auto c:{0.0f,-0.0f}){
        node=Leaf(a,b,c,-0.0f);parent.back=1;parent.front=2;
        Compare({parent,node,node},{a,b,c,-0.0f},"Root/helper signed-zero products and strict zero equality");}
}

std::uint64_t Next(std::uint64_t& s){s^=s<<13u;s^=s>>7u;s^=s<<17u;return s;}
float Number(std::uint64_t& s,bool broad){const auto b=static_cast<std::uint32_t>(Next(s));return broad?std::bit_cast<float>((b&0x807fffffu)|(((b>>23u)%255u)<<23u)):
    static_cast<float>(static_cast<int>(b%65537u)-32768)/128.0f;}
void Random(){std::uint64_t seed=0x842d1763ba09ef5cull;
    for(unsigned sample=0;sample<2400u;++sample){const bool broad=sample<800u;const auto count=static_cast<std::size_t>(Next(seed)%32u)+1u;std::vector<Node> nodes;
        nodes.reserve(count);for(std::size_t i=0;i<count;++i){auto n=Leaf(Number(seed,broad),Number(seed,broad),Number(seed,broad),Number(seed,broad),static_cast<std::uint8_t>(Next(seed)));
            if(2u*i+1u<count)n.back=static_cast<std::int32_t>(2u*i+1u);
            if(2u*i+2u<count)n.front=static_cast<std::int32_t>(2u*i+2u);
            nodes.push_back(n);}
        const Sphere s{Number(seed,broad),Number(seed,broad),Number(seed,broad),Number(seed,broad)};
        Compare(std::move(nodes),s,"Random finite/subnormal/extreme branching sphere");}
}

void FailuresAndJournal(){const auto nan=std::numeric_limits<float>::quiet_NaN();Queries absent;
    QuestVr::PrecomputeOriginalBrushSphereFilter(0u,{nan,nan,nan,nan},absent,0u);++checks;
    Fixture singleton({Leaf()});auto q=singleton.Callbacks();
    for(unsigned missing=0;missing<3u;++missing){auto bad=q;if(missing==0u)bad.readNode={};if(missing==1u)bad.writeFlags={};if(missing==2u)bad.work={};
        Refuse([&]{QuestVr::PrecomputeOriginalBrushSphereFilter(1u,{0,0,0,1},bad);},"Missing required caller query/work");}
    for(const auto depth:{0u,257u})Refuse([&]{QuestVr::PrecomputeOriginalBrushSphereFilter(1u,{0,0,0,1},q,depth);},"Unsupported recursion bound");
    // Caller rollback, not provider-autoundo: numeric rejection occurs AFTER
    // the original clear. Root transaction restores an independently captured
    // snapshot and proves all node fields/flags identical.
    for(const auto bad:{nan,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        for(unsigned field=0;field<8u;++field){Fixture fixture({Leaf()});const auto saved=fixture.nodes;Sphere s{0,0,0,1};
            if(field==0u)s.x=bad;
            if(field==1u)s.y=bad;
            if(field==2u)s.z=bad;
            if(field==3u)s.radius=bad;
            if(field==4u)fixture.nodes[0].planeX=bad;
            if(field==5u)fixture.nodes[0].planeY=bad;
            if(field==6u)fixture.nodes[0].planeZ=bad;
            if(field==7u)fixture.nodes[0].planeW=bad;
            const auto before=fixture.nodes;const auto callbacks=fixture.Callbacks();Refuse([&]{QuestVr::PrecomputeOriginalBrushSphereFilter(1u,s,callbacks);},"Active nonfinite after clear");
            Require(fixture.writes.size()==1u&&fixture.nodes[0].nodeFlags==0x25u,"Numeric admission moved before original flags clear or invented undo");
            fixture.nodes=before;Require(Same(fixture.nodes,before),"Fixture journal failed to restore numeric-refusal node");
            static_cast<void>(saved);
        }
    }
    auto root=Leaf();root.front=9;Fixture invalid({root});const auto iq=invalid.Callbacks();Refuse([&]{QuestVr::PrecomputeOriginalBrushSphereFilter(1u,{3,0,0,1},iq);},"Selected out-of-range child");
    Require(invalid.writes.size()==2u,"Selected child refusal erased earlier native writes");
    root.front=-2;Fixture negative({root});const auto nq=negative.Callbacks();Refuse([&]{QuestVr::PrecomputeOriginalBrushSphereFilter(1u,{3,0,0,1},nq);},"Selected negative non-sentinel child");
    root.front=1;Fixture free({root,Leaf()});free.owned[1]=false;const auto fq=free.Callbacks();Refuse([&]{QuestVr::PrecomputeOriginalBrushSphereFilter(2u,{3,0,0,1},fq);},"Selected unowned/free dynamic slot");
    root=Leaf(0,0,0,0);root.back=1;auto middle=root;middle.back=2;Fixture nested({root,middle,Leaf(0,0,0,0)});const auto dq=nested.Callbacks();
    Refuse([&]{QuestVr::PrecomputeOriginalBrushSphereFilter(3u,{0,0,0,0},dq,2u);},"Active nested recursion bound");
    Require(!nested.writes.empty(),"Depth fixture did not reach partial original writes");
    root=Leaf();root.front=0;Fixture cycle({root});cycle.workLimit=20u;const auto cq=cycle.Callbacks();const auto snapshot=cycle.nodes;
    Refuse([&]{QuestVr::PrecomputeOriginalBrushSphereFilter(1u,{3,0,0,1},cq,1u);},"Shared caller work exhausts tail cycle");
    Require(cycle.writes.size()>2u&&cycle.work==21u,"Cycle was deduplicated or shared work reset");cycle.nodes=snapshot;
    Require(Same(cycle.nodes,snapshot),"Caller snapshot did not undo partial cyclic flag writes");
    Fixture injected({Leaf()});injected.throwAfterClear=true;const auto jq=injected.Callbacks();const auto previous=injected.nodes;
    Refuse([&]{QuestVr::PrecomputeOriginalBrushSphereFilter(1u,{0,0,0,1},jq);},"Caller failure after first actual write");
    Require(injected.writes.size()==1u&&!Same(injected.nodes,previous),"Fixture did not observe partial external write");
    injected.nodes=previous;Require(Same(injected.nodes,previous),"Caller journal failed authoritative flag undo");
}
} // namespace
#endif

int main(){
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    try{Hw::Precision precision;Classifications();Traversal();Arithmetic();Random();FailuresAndJournal();
        std::cout<<"PASS original brush sphere prefilter "<<checks<<" checks, "<<comparisons<<" independent x87/order comparisons, "<<explicitRefusals
            <<" explicit refusals, "<<controls<<" effective retained/live-order controls.\n"
            <<"Caller-owned fixture writes/undo only; live Model capability, tracker/BSP/startup/headset not established.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL original brush sphere prefilter: "<<error.what()<<'\n';return 1;}
#else
    std::cout<<"SKIP independent sphere prefilter oracle: GCC x86 x87PC64 required; no host-math substitute.\n";return 77;
#endif
}
