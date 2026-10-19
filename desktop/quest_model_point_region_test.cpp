#include "quest_model_point_region.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Vec=PortableModelVec3;
using Result=QuestVr::OriginalModelPointRegion;
std::size_t checks{},refusals{},hardwareCases{},traversals{},floatWitnesses{},doubleWitnesses{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void Refuse(F run){bool failed{};try{run();}catch(const std::exception&){failed=true;}
    Require(failed,"Original PointRegion unexpectedly admitted an unsafe active path");++refusals;}
bool Same(const Result& a,const Result& b){return a.zoneActorReference==b.zoneActorReference&&a.leaf==b.leaf&&
    a.zoneNumber==b.zoneNumber&&a.nodeVisits==b.nodeVisits;}
PortableModelGeometry Model(){
    PortableModelGeometry model;model.nodes.resize(1u);auto& n=model.nodes.front();
    n.planeX=1;n.back=-1;n.front=-1;n.leaf0=-17;n.leaf1=91;n.zone0=0;n.zone1=1;
    model.zones.resize(2u);model.zones[0u].actorReference=73;model.zones[1u].actorReference=-29;return model;
}
void Controls(){
    const float nan=std::bit_cast<float>(0x7fc12345u),inf=std::numeric_limits<float>::infinity();
    PortableModelGeometry empty;empty.rootOutside=-1;empty.zones.resize(65u);std::size_t work{};
    Require(Same(QuestVr::QueryOriginalModelPointRegion(empty,{nan,inf,-inf},0u,[&]{++work;}),Result{})&&!work,
        "Empty BSP read inactive point/zone table/work budget");
    auto model=Model();
    for(const auto x:{-0.0f,0.0f,1.0f,std::numeric_limits<float>::denorm_min()})
        Require(Same(QuestVr::QueryOriginalModelPointRegion(model,{x,0,0}),{-29,91,1,1}),
            "Nonnegative/signed-zero distance did not use the front leaf and zone");
    Require(Same(QuestVr::QueryOriginalModelPointRegion(model,{-1,0,0}),{73,-17,0,1}),
        "Negative distance did not select back leaf/zone");
    // A missing selected child terminates; an opposite child/plane is inactive.
    model.nodes[0u].back=1999;model.nodes[0u].zone0=-200;
    Require(Same(QuestVr::QueryOriginalModelPointRegion(model,{}),{-29,91,1,1}),
        "Exact equality took/validated the opposite back child or zone");
    for(unsigned flags=0u;flags<256u;++flags)for(const int outside:{-1,0,1,2}){
        model.nodes[0u].nodeFlags=static_cast<std::uint8_t>(flags);model.nodes[0u].vertexCount=static_cast<std::uint8_t>(flags);
        model.rootOutside=outside;
        Require(Same(QuestVr::QueryOriginalModelPointRegion(model,{}),{-29,91,1,1}),
            "CSG/root-outside bookkeeping changed the PointRegion tuple");
    }
    model=Model();model.zones.clear();model.nodes[0u].zone1=999;
    Require(Same(QuestVr::QueryOriginalModelPointRegion(model,{}),{0,91,0,1}),
        "NumZones zero read the inactive node-zone value");
    model=Model();model.zones.resize(1u);model.nodes[0u].zone1=63;
    Require(Same(QuestVr::QueryOriginalModelPointRegion(model,{}),{0,91,63,1}),
        "An unpopulated native zone slot lost its selected zone/leaf or lacked fallback");
    model.zones.resize(64u);model.zones[63u].actorReference=777;
    Require(Same(QuestVr::QueryOriginalModelPointRegion(model,{}),{777,91,63,1}),"Native zone slot63 was not readable");
    model.zones[63u].actorReference=0;
    Require(Same(QuestVr::QueryOriginalModelPointRegion(model,{}),{0,91,63,1}),"A null selected actor did not preserve the zone tuple");
    model.nodes[0u].leaf1=std::numeric_limits<std::int32_t>::min();
    Require(QuestVr::QueryOriginalModelPointRegion(model,{}).leaf==std::numeric_limits<std::int32_t>::min(),
        "Raw terminal leaf was validated against an unused Leaves array");
    for(const auto bad:{-1,64,255}){model.nodes[0u].zone1=bad;Refuse([&]{QuestVr::QueryOriginalModelPointRegion(model,{});});}
    model=Model();model.zones.resize(65u);Refuse([&]{QuestVr::QueryOriginalModelPointRegion(model,{});});
    model=Model();Refuse([&]{QuestVr::QueryOriginalModelPointRegion(model,{},0u);});
    for(const auto child:{-2,std::numeric_limits<std::int32_t>::min(),1,std::numeric_limits<std::int32_t>::max()}){
        model=Model();model.nodes[0u].front=child;Refuse([&]{QuestVr::QueryOriginalModelPointRegion(model,{});});
    }
    model=Model();model.nodes[0u].front=0;work=0;
    Refuse([&]{QuestVr::QueryOriginalModelPointRegion(model,{},7u,[&]{++work;});});
    Require(work==7u,"Cyclic path did not stop at the exact local work bound");
    for(unsigned channel=0u;channel<3u;++channel)for(const float bad:{nan,inf,-inf}){
        Vec point{};(channel==0u?point.x:channel==1u?point.y:point.z)=bad;model=Model();
        Refuse([&]{QuestVr::QueryOriginalModelPointRegion(model,point);});
    }
    for(unsigned channel=0u;channel<4u;++channel)for(const float bad:{nan,inf,-inf}){
        model=Model();auto& n=model.nodes[0u];
        (channel==0u?n.planeX:channel==1u?n.planeY:channel==2u?n.planeZ:n.planeW)=bad;
        Refuse([&]{QuestVr::QueryOriginalModelPointRegion(model,{});});
    }
    model=Model();model.nodes.resize(4u);
    for(unsigned i=0u;i<4u;++i){model.nodes[i]=model.nodes[0u];model.nodes[i].front=i==3u?-1:static_cast<int>(i+1u);}
    for(std::size_t allowance=0u;allowance<4u;++allowance){work=0;
        Refuse([&]{QuestVr::QueryOriginalModelPointRegion(model,{},10u,[&]{if(work++==allowance)throw std::runtime_error("shared root limit");});});
        Require(work==allowance+1u,"Shared-work refusal reset its ledger or continued traversal");
    }
    work=0;
    Require(Same(QuestVr::QueryOriginalModelPointRegion(model,{},4u,[&]{++work;}),{-29,91,1,4})&&work==4u,
        "Exact-budget four-node traversal failed");
    // A Float final spill would turn this negative retained distance into -0.
    model=Model();model.nodes[0u].planeX=-std::numeric_limits<float>::denorm_min();
    Require(QuestVr::QueryOriginalModelPointRegion(model,{std::numeric_limits<float>::denorm_min(),0,0}).leaf==-17,
        "Final Float spill lost a negative subnormal product's side");
    // PC64 retains the -1 between the two powers; binary64 arithmetic loses it.
    model=Model();model.nodes[0u].planeZ=std::ldexp(1.0f,60);model.nodes[0u].planeY=-1;
    model.nodes[0u].planeX=-std::ldexp(1.0f,60);
    Require(QuestVr::QueryOriginalModelPointRegion(model,{1,1,1}).leaf==-17,
        "Premature Double spill changed a cancellation-side decision");
    model=Model();model.nodes[0u].planeX=std::numeric_limits<float>::max();
    Require(QuestVr::QueryOriginalModelPointRegion(model,{std::numeric_limits<float>::max(),0,0}).leaf==91,
        "A finite x87 product was incorrectly limited to Float range");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace HardwareOracle {
struct Precision {
    unsigned short previous{};
    Precision(){static_assert(std::numeric_limits<long double>::digits==64);asm volatile("fnstcw %0":"=m"(previous));
        const auto mode=static_cast<unsigned short>((previous&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
    ~Precision(){asm volatile("fldcw %0"::"m"(previous));}
};
long double Distance(const Vec& p,const PortableModelNode& n){
    long double result;
    // Independent hardware instructions: none of the software Dyadic helpers
    // or production Front/trace/region kernels are used in this oracle.
    asm volatile("flds %[pz]; fmuls %[nz]; flds %[py]; fmuls %[ny]; faddp; "
        "flds %[px]; fmuls %[nx]; faddp; fsubs %[nw]; fstpt %[out]"
        :[out]"=m"(result):[pz]"m"(p.z),[nz]"m"(n.planeZ),[py]"m"(p.y),[ny]"m"(n.planeY),
        [px]"m"(p.x),[nx]"m"(n.planeX),[nw]"m"(n.planeW):"st");return result;
}
Result Walk(const PortableModelGeometry& model,const Vec& point){
    Result out;if(model.nodes.empty())return out;
    std::size_t index{};
    for(;;){Require(index<model.nodes.size()&&out.nodeVisits<model.nodes.size(),"Independent region oracle path is malformed");
        const auto& node=model.nodes[index];++out.nodeVisits;
        const bool front=Distance(point,node)>=0.0L;const auto child=front?node.front:node.back;
        if(child!=-1){Require(child>=0,"Independent region oracle negative child");index=static_cast<std::size_t>(child);continue;}
        out.leaf=front?node.leaf1:node.leaf0;out.zoneNumber=static_cast<std::uint8_t>(model.zones.empty()?0:(front?node.zone1:node.zone0));
        if(out.zoneNumber<model.zones.size())out.zoneActorReference=model.zones[out.zoneNumber].actorReference;
        return out;
    }
}
} // namespace HardwareOracle
std::uint64_t Next(std::uint64_t& state){state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
float Number(std::uint64_t& state,bool broad){const auto raw=static_cast<std::uint32_t>(Next(state));
    return broad?std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u)):
        static_cast<float>(static_cast<int>(raw%65537u)-32768)/1024.0f;}
void Compare(const PortableModelGeometry& model,Vec point){
    const auto actual=QuestVr::QueryOriginalModelPointRegion(model,point);
    const auto expected=HardwareOracle::Walk(model,point);
    Require(Same(actual,expected),"Software PointRegion tuple/path differs from independent hardware x87");++hardwareCases;
    if(actual.nodeVisits>1u)++traversals;
}
void Hardware(){
    HardwareOracle::Precision precision;std::uint64_t state=0x89cc3197e5df4a62ull;
    for(unsigned i=0u;i<40'000u;++i){auto model=Model();const bool broad=i<25'000u;auto& n=model.nodes[0u];
        n.planeX=Number(state,broad);n.planeY=Number(state,broad);n.planeZ=Number(state,broad);n.planeW=Number(state,broad);
        Compare(model,{Number(state,broad),Number(state,broad),Number(state,broad)});
    }
    for(unsigned i=0u;i<12'000u;++i){auto model=Model();model.nodes.resize(31u);model.zones.resize(Next(state)%65u);
        for(std::size_t j=0u;j<model.zones.size();++j)model.zones[j].actorReference=static_cast<int>(j%3u?j+1u:0u);
        for(std::size_t j=0u;j<model.nodes.size();++j){auto& n=model.nodes[j];
            n.planeX=Number(state,true);n.planeY=Number(state,true);n.planeZ=Number(state,true);n.planeW=Number(state,true);
            n.front=j<15u?static_cast<int>(j*2u+1u):-1;n.back=j<15u?static_cast<int>(j*2u+2u):-1;
            n.zone0=static_cast<int>(Next(state)%64u);n.zone1=static_cast<int>(Next(state)%64u);
            n.leaf0=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(state)));
            n.leaf1=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(state)));
            n.nodeFlags=static_cast<std::uint8_t>(Next(state));n.vertexCount=static_cast<std::uint8_t>(Next(state));
        }
        model.rootOutside=static_cast<int>(i%3u)-1;Compare(model,{Number(state,true),Number(state,true),Number(state,true)});
    }
    // Deterministic cancellation cohorts require witnesses against both the
    // previous Float X/Y/Z path and a seemingly safer binary64 replacement.
    for(int exponent=25;exponent<=120;++exponent){auto model=Model();auto& n=model.nodes[0u];
        n.planeZ=std::ldexp(1.0f,exponent);n.planeY=-n.planeZ;n.planeX=-1;
        const Vec point{1,1,1};Compare(model,point);
        volatile float xy=point.x*n.planeX+point.y*n.planeY;
        volatile float wrong=xy+point.z*n.planeZ-n.planeW;
        if((wrong>=0.0f)!=(HardwareOracle::Distance(point,n)>=0.0L))++floatWitnesses;
    }
    for(int exponent=54;exponent<=63;++exponent){auto model=Model();auto& n=model.nodes[0u];
        n.planeZ=std::ldexp(1.0f,exponent);n.planeY=-1;n.planeX=-n.planeZ;const Vec point{1,1,1};Compare(model,point);
        volatile double zy=double(point.z)*n.planeZ+double(point.y)*n.planeY;
        volatile double wrong=zy+double(point.x)*n.planeX-n.planeW;
        if((wrong>=0.0)!=(HardwareOracle::Distance(point,n)>=0.0L))++doubleWitnesses;
    }
    Require(hardwareCases>=52'000u&&traversals==12'000u&&floatWitnesses>=90u&&doubleWitnesses>=9u,
        "Independent PointRegion cohorts missed traversal/Float/Double cancellation witnesses");
}
#else
void Hardware(){std::cout<<"SKIP independent x87 PointRegion oracle on non-x86 compiler; deterministic controls still run.\n";}
#endif
} // namespace

int main(){try{Controls();Hardware();std::cout<<"PASS original PointRegion "<<checks<<" checks / "<<refusals
    <<" explicit refusals, "<<hardwareCases<<" independent hardware-x87 queries, "<<traversals<<" multilevel paths, "
    <<floatWitnesses<<" Float / "<<doubleWitnesses<<" Double cancellation witnesses. Not SetActorZone, Move or crate physics.\n";return 0;}
    catch(const std::exception& error){std::cerr<<"Original PointRegion failed: "<<error.what()<<'\n';return 1;}}
