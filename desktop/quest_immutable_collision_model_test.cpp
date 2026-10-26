#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "quest_model_point_overlap.h"

#include <bit>
#include <filesystem>
#include <iostream>
#include <type_traits>

namespace {
using Source=QuestVr::OriginalImmutableCollisionModel;
using Model=PortableModelGeometry;
using Vec=PortableModelVec3;
using Hit=QuestVr::OriginalActorLineHit;
using Limits=QuestVr::ModelExtentTraceLimits;
using PointLimits=QuestVr::ModelPointOverlapLimits;
using Stats=QuestVr::ModelPointOverlapStats;
std::size_t checks{},refusals{};
void Require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F> void Refuses(F&& call) {
    bool refused{};try{call();}catch(const std::exception&){refused=true;}
    Require(refused,"Expected immutable-source refusal did not occur");++refusals;
}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec a,Vec b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Hit& a,const Hit& b) {
    return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&a.item==b.item&&Same(a.time,b.time)&&
        Same(a.location,b.location)&&Same(a.normal,b.normal);
}
template<class A,class B> bool SameQuery(const A& a,const B& b) {
    return a.querySteps==b.querySteps&&a.nodeVisits==b.nodeVisits&&a.leafVisits==b.leafVisits&&
        a.hullVisits==b.hullVisits&&a.planeTests==b.planeTests&&a.bevelPairs==b.bevelPairs&&a.bevelTests==b.bevelTests;
}
Hit Seed(){return {13u,17u,19u,{1,-0.0f,3},{4,5,6},0.25f,23};}
auto Capture(){return QuestVr::OriginalModelCoords{{},{1,0,0},{0,1,0},{0,0,1}};}
Model Box() {
    Model m;m.objectPath="SameFilename.Model";m.rootOutside=1;
    const Vec normals[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const float distances[]={0,10,10,10,10,10};
    for(std::size_t i=0;i<6u;++i) {
        PortableModelNode n;n.planeX=normals[i].x;n.planeY=normals[i].y;n.planeZ=normals[i].z;n.planeW=distances[i];
        n.front=n.back=n.plane=n.collisionBound=n.renderBound=n.leaf0=n.leaf1=-1;n.vertexCount=3;
        m.nodes.push_back(n);
    }
    m.nodes[0].collisionBound=0;m.leafHulls={0,1,2,3,4,5,-1};
    for(float f:{-10.0f,-10.0f,-10.0f,0.0f,10.0f,10.0f})m.leafHulls.push_back(std::bit_cast<std::int32_t>(f));
    return m;
}
auto Trace(const Source& source,const Limits& limits={}) {
    return QuestVr::TraceOriginalModelExtent(source,{10,0,0},{-20,0,0},{1,1,1},limits);
}
auto Raw(const Model& model,const Limits& limits={}) {
    return QuestVr::TraceOriginalModelExtent(model,{10,0,0},{-20,0,0},{1,1,1},limits);
}
bool Point(const Source& source,Hit& hit,Stats& stats,const PointLimits& limits={},std::uintptr_t owner=0u,Vec extent={1,1,1}) {
    return QuestVr::CheckOriginalModelPointOverlap(hit,source,37u,owner,{-2,0,0},extent,0u,Capture,limits,&stats);
}
void Results(const Model& model) {
    const auto raw=Raw(model);Require(raw.hit,"Test fixture did not hit actual hull");
    for(bool pointFirst:{false,true}) {
        const auto source=Source::CopyOf(model);Require(!source->HasNullHullValidation(),"New source started warm");
        Hit expected=Seed();Stats expectedStats;
        const bool expectedClear=QuestVr::CheckOriginalModelPointOverlap(expected,model,37u,0u,{-2,0,0},{1,1,1},0u,Capture,{},&expectedStats);
        if(pointFirst) {
            auto hit=Seed();Stats stats;const auto clear=Point(*source,hit,stats);
            Require(clear==expectedClear&&Same(hit,expected)&&SameQuery(stats,expectedStats)&&stats.validationSteps==expectedStats.validationSteps,
                "Cold PointCheck changed packet or work");
        }
        const auto result=Trace(*source);
        Require(result.hit==raw.hit&&result.clear==raw.clear&&result.node==raw.node&&Same(result.time,raw.time)&&
            Same(result.location,raw.location)&&Same(result.normal,raw.normal)&&SameQuery(result.stats,raw.stats),
            "Immutable trace changed original result/query words");
        Require(result.stats.validationSteps==(pointFirst?0u:raw.stats.validationSteps),"Cold/warm validation charge is wrong");
        Require(source->HasNullHullValidation(),"Successful validation did not publish its proof");
        auto zero=Limits{};zero.validationSteps=0u;
        const auto warm=Trace(*source,zero);Require(warm.stats.validationSteps==0u&&SameQuery(warm.stats,raw.stats),"Warm0 changed query work");
        auto hit=Seed();Stats stats;auto pointZero=PointLimits{};pointZero.validationSteps=0u;
        Require(Point(*source,hit,stats,pointZero)==expectedClear&&Same(hit,expected)&&stats.validationSteps==0u&&SameQuery(stats,expectedStats),
            "Shared extent/PointCheck proof changed packet or charged repeated validation");
        auto rawHit=Seed(),cachedHit=Seed();QuestVr::ModelExtentTraceStats a,b;
        const auto rawClear=QuestVr::CheckOriginalModelExtent(rawHit,model,37u,0u,nullptr,{-20,0,0},{10,0,0},{1,1,1},{},&a);
        const auto clear=QuestVr::CheckOriginalModelExtent(cachedHit,*source,37u,{-20,0,0},{10,0,0},{1,1,1},zero,&b);
        Require(clear==rawClear&&Same(rawHit,cachedHit)&&b.validationSteps==0u&&SameQuery(a,b),"Immutable hit adapter changed seeded fields");
        for(auto cut:{std::size_t{1u},raw.stats.querySteps/2u,raw.stats.querySteps-1u}) {
            auto tight=zero;tight.querySteps=cut;
            bool refused{};try{static_cast<void>(Trace(*source,tight));}
            catch(const QuestVr::ModelExtentTraceWorkExhausted& fault){refused=!fault.Validation()&&fault.Used()==cut&&fault.Limit()==cut;}
            Require(refused,"Warm query evaded its independent query limit");++refusals;
        }
        for(auto field:{&Limits::nodes,&Limits::hullWords,&Limits::pending,&Limits::querySteps}) {
            auto tight=zero;tight.*field=0u;Refuses([&]{Trace(*source,tight);});
        }
        auto tight=zero;tight.nodes=model.nodes.size()-1u;Refuses([&]{Trace(*source,tight);});
        tight=zero;tight.hullWords=model.leafHulls.size()-1u;Refuses([&]{Trace(*source,tight);});
        pointZero.querySteps=1u;hit=Seed();const auto before=hit;
        Refuses([&]{Point(*source,hit,stats,pointZero);});Require(Same(hit,before),"Refused PointCheck published partial hit");
        // Raw geometry, even a const lease of a warm source, still validates.
        Require(Raw(source->Geometry()).stats.validationSteps==raw.stats.validationSteps,"Raw API borrowed an implicit proof");
        Refuses([&]{Raw(source->Geometry(),zero);});
    }
}
void Safety() {
    static_assert(!std::is_default_constructible_v<Source> && !std::is_copy_constructible_v<Source>);
    static_assert(std::is_same_v<decltype(std::declval<const Source&>().Geometry()),const Model&>);
    auto model=Box();auto* alias=model.nodes.data();const auto source=Source::CopyOf(model);
    alias[0].front=0;model.leafHulls.clear();
    Require(Trace(*source).hit,"CopyOf retained mutable vector aliases");
    auto lease=source->GeometryLease();Require(lease.get()==&source->Geometry(),"Geometry lease changed asset identity");
    const auto replacement=Source::CopyOf(Box());Require(!replacement->HasNullHullValidation(),"Same filename replacement borrowed retired proof");
    auto raw=Box();const auto baseline=Raw(raw);
    for(auto cut:{std::size_t{0u},std::size_t{1u},baseline.stats.validationSteps/2u,baseline.stats.validationSteps-1u}) {
        const auto cold=Source::CopyOf(raw);auto tight=Limits{};tight.validationSteps=cut;
        bool refused{};try{Trace(*cold,tight);}catch(const QuestVr::ModelExtentTraceWorkExhausted& fault){refused=fault.Validation()&&fault.Used()==cut;}
        Require(refused&&!cold->HasNullHullValidation(),"Failed validation published a proof");++refusals;
        Require(Trace(*cold).stats.validationSteps==baseline.stats.validationSteps,"Cold retry skipped failed validation");
    }
    const auto queryFailure=Source::CopyOf(raw);auto tight=Limits{};tight.querySteps=1u;
    Refuses([&]{Trace(*queryFailure,tight);});Require(queryFailure->HasNullHullValidation(),"Completed validation was lost after query refusal");
    for(unsigned variant=0u;variant<4u;++variant) {
        auto malformed=Box();
        if(variant==0u)malformed.nodes.back().front=6;
        if(variant==1u)malformed.nodes.back().front=5;
        if(variant==2u)malformed.leafHulls[0]=99;
        if(variant==3u)malformed.leafHulls.resize(8u);
        const auto cold=Source::CopyOf(malformed);Refuses([&]{Trace(*cold);});
        Require(!cold->HasNullHullValidation(),"Malformed pruned graph/hull acquired proof");
        Refuses([&]{Trace(*cold);});
    }
    auto inactive=Box();inactive.leafHulls[7]=std::bit_cast<std::int32_t>(std::numeric_limits<float>::quiet_NaN());
    const auto owned=Source::CopyOf(inactive);auto hit=Seed();Stats stats;
    static_cast<void>(Point(*owned,hit,stats,{},41u));
    Require(!owned->HasNullHullValidation(),"Owned inactive bounds warmed a null-owner proof");
    Refuses([&]{Trace(*owned);});Require(!owned->HasNullHullValidation(),"Invalid null bounds acquired proof");
    const auto zeroSource=Source::CopyOf(inactive);hit=Seed();static_cast<void>(Point(*zeroSource,hit,stats,{},0u,{0,0,0}));
    Require(!zeroSource->HasNullHullValidation(),"Zero occupancy activated or warmed hull validation");
    std::weak_ptr<const Model> weak;{auto temporary=Source::CopyOf(Box());weak=temporary->GeometryLease();auto keep=temporary->GeometryLease();temporary.reset();
        Require(!weak.expired()&&Raw(*keep).hit,"Const geometry lease did not retain lifetime");}
    Require(weak.expired(),"Immutable geometry leaked after all owners retired");
}
void Owned(const std::filesystem::path& root) {
    const auto file=root/"Maps"/"00_Training.dx";const auto time=std::filesystem::last_write_time(file);
    const auto size=std::filesystem::file_size(file);
    const auto tables=LoadPortablePackageTables(file.string());const auto index=FindPortableRootModel68Export(tables);
    const auto world=Source::LoadWorld(tables,index),collision=Source::LoadCollision(tables,index);
    Require(world->Geometry().exportIndex==index&&collision->Geometry().exportIndex==index,"Fresh loader source changed export identity");
    for(const auto& src:{world,collision}) {
        const auto raw=Raw(src->Geometry());const auto cold=Trace(*src);const auto warm=Trace(*src);
        Require(raw.hit==cold.hit&&raw.clear==cold.clear&&Same(raw.time,cold.time)&&Same(raw.location,cold.location)&&
            Same(raw.normal,cold.normal)&&SameQuery(raw.stats,cold.stats)&&cold.stats.validationSteps==raw.stats.validationSteps&&
            warm.stats.validationSteps==0u&&SameQuery(warm.stats,raw.stats),"Owned first Training cold/warm trace disagrees");
    }
    Require(std::filesystem::last_write_time(file)==time&&std::filesystem::file_size(file)==size,"Owned source changed");
}
} // namespace
int main(int argc,char** argv) {
    try{Results(Box());Safety();if(argc==2)Owned(argv[1]);else Require(argc==1,"Usage: quest_immutable_collision_model_test [OWNED_GAME_ROOT]");
        std::cout<<"PASS immutable Model validation: "<<checks<<" checks / "<<refusals<<" refusals; shared extent/box proof, cold/warm budgets, "
            "mutable alias isolation, raw/owned/zero separation, malformed pruned data and asset lifetime. NOT full physics/startup/headset acceptance.\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL immutable Model: "<<e.what()<<'\n';return 1;}
}
