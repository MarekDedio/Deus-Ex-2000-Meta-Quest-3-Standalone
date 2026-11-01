#include "quest_model_fast_line_check.h"
#include "quest_model_point_line_hit.h"
#include "quest_model_point_overlap.h"
#include "quest_model_point_region.h"

#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

// GENERATED GENERIC-KERNEL CONTROLS ONLY. This deliberately manufactured
// proxy is NOT RuntimeObject/Level/GC/tracker authority, a production factory,
// immutable validation proof, active-world integration or a headset test.
namespace {
using Model=PortableModelGeometry;
using Vec=PortableModelVec3;
using Hit=QuestVr::OriginalActorLineHit;
std::size_t checks{},refusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Refuse(F run){bool failed{};try{run();}catch(const std::exception&){failed=true;}
    Require(failed,"Expected selected-view refusal");++refusals;}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec a,Vec b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
bool Same(const Hit& a,const Hit& b){return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&a.item==b.item&&
    Bits(a.time,b.time)&&Same(a.location,b.location)&&Same(a.normal,b.normal);}
Hit Seed(){return {17u,19u,23u,{-0.0f,37,-41},{7,-0.0f,11},0.625f,29};}
PortableModelNode Node(Vec normal,std::uint8_t flags=0x20u){PortableModelNode n;
    n.planeX=normal.x;n.planeY=normal.y;n.planeZ=normal.z;n.nodeFlags=flags;n.vertexCount=4;
    n.front=n.back=n.plane=n.collisionBound=n.renderBound=-1;return n;}
Model Fixture(){Model m;m.rootOutside=1;m.nodes={Node({1,0,0}),Node({1,0,0}),Node({0,1,0}),Node({0,0,1},0)};
    m.nodes[0].back=2;m.nodes[2].front=3;m.nodes[3].collisionBound=0;
    m.nodes[3].zone0=1;m.nodes[3].zone1=2;m.nodes[3].leaf0=17;m.nodes[3].leaf1=18;
    m.zones.resize(3u);m.zones[0].actorReference=100;m.zones[1].actorReference=101;m.zones[2].actorReference=102;
    m.leafHulls={3,-1};for(float value:{-10.0f,-10.0f,-10.0f,10.0f,10.0f,0.0f}) {
        m.leafHulls.push_back(std::bit_cast<std::int32_t>(value));}
    return m;}
const Vec Start{-1,1,2},End{-1,1,-2},Extent{0.2f,0.2f,0.2f},Inside{-1,1,-0.05f};
QuestVr::OriginalModelCoords Identity(){return {{},{1,0,0},{0,1,0},{0,0,1}};}

struct State {
    const Model& model;
    std::size_t original{1u};std::vector<bool> owned;
    mutable std::vector<std::size_t> reads;
    mutable std::size_t work{},checkCalls{},readCalls{};
    std::size_t maximumWork{1000000u};
    std::uint64_t expectedRevision{7u},expectedRoot{11u};
    mutable std::uint64_t revision{7u},root{11u};
    mutable bool alive{true};bool expireEmpty{};
    std::size_t expireReadIndex{std::numeric_limits<std::size_t>::max()},expireReadOrdinal{};
    explicit State(const Model& value):model(value),owned(value.nodes.size(),true),reads(value.nodes.size()){}
    void Check()const{
        // The CALLER owns this finite ledger; no query/helper resets it.
        if(work==maximumWork)throw std::runtime_error("fixture shared access work exhausted");
        ++work;++checkCalls;
        if(!alive||revision!=expectedRevision||root!=expectedRoot)throw std::runtime_error("fixture expired/replaced lease");
        if(owned.size()!=model.nodes.size()||original>model.nodes.size())throw std::runtime_error("fixture owner-array shape");
    }
};
struct Nodes {
    State* state;
    void check()const{state->Check();}
    std::size_t size()const{check();return state->model.nodes.size();}
    bool empty()const{check();const auto result=state->model.nodes.empty();
        if(result&&state->expireEmpty)state->alive=false;
        return result;}
    bool active(std::size_t i)const{check();if(i>=state->owned.size())throw std::runtime_error("fixture active index");
        return i<state->original||state->owned[i];}
    PortableModelNode at(std::size_t i)const{check();if(i>=state->owned.size()||!(i<state->original||state->owned[i]))
            throw std::runtime_error("fixture selected free pool record");
        ++state->reads[i];++state->readCalls;const auto result=state->model.nodes.at(i);
        if(i==state->expireReadIndex&&state->reads[i]==state->expireReadOrdinal)++state->revision;
        return result;}
    PortableModelNode operator[](std::size_t i)const{return at(i);}
};
struct View {
    Nodes nodes;
    const std::uint16_t& version;const std::uint16_t& licenseeMode;
    const std::int32_t& rootOutside;
    const std::vector<std::int32_t>& leafHulls;
    const std::vector<PortableModelZone>& zones;
    explicit View(State& s):nodes{&s},version(s.model.version),licenseeMode(s.model.licenseeMode),
        rootOutside(s.model.rootOutside),leafHulls(s.model.leafHulls),zones(s.model.zones){}
};
struct Results {
    bool fast{},point{},extent{},zero{},box{};
    Hit pointHit=Seed(),extentHit=Seed(),zeroHit=Seed(),boxHit=Seed();
    QuestVr::ModelPointTraceStats fastStats,pointStats;
    QuestVr::ModelExtentTraceStats extentStats;
    QuestVr::ModelPointOverlapStats zeroStats,boxStats;
    QuestVr::OriginalModelPointRegion region;
};
Results Raw(const Model& m){Results r;const auto fast=QuestVr::CheckOriginalModelFastLine(m,End,Start);
    r.fast=fast.clear;r.fastStats=fast.stats;
    r.point=QuestVr::CheckOriginalModelPoint(r.pointHit,m,101u,0u,End,Start,6u,[]{return Identity();},{},&r.pointStats);
    r.extent=QuestVr::CheckOriginalModelExtent(r.extentHit,m,101u,0u,nullptr,End,Start,Extent,{},&r.extentStats);
    r.zero=QuestVr::CheckOriginalModelPointOverlap(r.zeroHit,m,101u,0u,End,{},0u,[]{return Identity();},{},&r.zeroStats);
    r.box=QuestVr::CheckOriginalModelPointOverlap(r.boxHit,m,101u,0u,Inside,Extent,0u,[]{return Identity();},{},&r.boxStats);
    r.region=QuestVr::QueryOriginalModelPointRegion(m,End);return r;}
Results Through(const View& m){Results r;const auto fast=QuestVr::ModelFastLineDetail::Check(m,End,Start);
    r.fast=fast.clear;r.fastStats=fast.stats;
    r.point=QuestVr::ModelPointLineDetail::Check(r.pointHit,m,101u,0u,End,Start,6u,[]{return Identity();},{},&r.pointStats);
    r.extent=QuestVr::ModelExtentTraceDetail::Check(r.extentHit,m,101u,0u,nullptr,End,Start,Extent,{},&r.extentStats);
    r.zero=QuestVr::ModelPointOverlapDetail::Check(r.zeroHit,m,101u,0u,End,{},0u,[]{return Identity();},{},&r.zeroStats);
    r.box=QuestVr::ModelPointOverlapDetail::Check(r.boxHit,m,101u,0u,Inside,Extent,0u,[]{return Identity();},{},&r.boxStats);
    r.region=QuestVr::ModelPointRegionDetail::Query(m,End);return r;}
template<class S>void Stats(const S& a,const S& b,std::size_t skipped){
    Require(a.validationSteps+skipped==b.validationSteps&&a.querySteps==b.querySteps&&a.nodeVisits==b.nodeVisits&&a.leafVisits==b.leafVisits,
        "Proxy changed numeric/query/validation work accounting");}
void ExtentStats(const QuestVr::ModelExtentTraceStats& a,const QuestVr::ModelExtentTraceStats& b,std::size_t skipped){
    Stats(a,b,skipped);Require(a.hullVisits==b.hullVisits&&a.planeTests==b.planeTests&&a.bevelPairs==b.bevelPairs&&a.bevelTests==b.bevelTests,
        "Proxy changed extent hull/bevel schedule");}
void OverlapStats(const QuestVr::ModelPointOverlapStats& a,const QuestVr::ModelPointOverlapStats& b,std::size_t skipped){
    Stats(a,b,skipped);Require(a.hullVisits==b.hullVisits&&a.planeTests==b.planeTests&&a.bevelPairs==b.bevelPairs&&a.bevelTests==b.bevelTests,
        "Proxy changed occupancy hull/bevel schedule");}
void Compare(const Results& a,const Results& b,std::size_t skipped){
    Require(a.fast==b.fast&&a.point==b.point&&a.extent==b.extent&&a.zero==b.zero&&a.box==b.box,"Proxy changed clear/blocked results");
    Require(Same(a.pointHit,b.pointHit)&&Same(a.extentHit,b.extentHit)&&Same(a.zeroHit,b.zeroHit)&&Same(a.boxHit,b.boxHit),
        "Proxy changed exact caller packet bits or original node indices");
    Require(a.region.zoneActorReference==b.region.zoneActorReference&&a.region.leaf==b.region.leaf&&
        a.region.zoneNumber==b.region.zoneNumber&&a.region.nodeVisits==b.region.nodeVisits,"Proxy changed original Region result");
    Stats(a.fastStats,b.fastStats,skipped);Stats(a.pointStats,b.pointStats,skipped);
    ExtentStats(a.extentStats,b.extentStats,skipped);OverlapStats(a.zeroStats,b.zeroStats,skipped);OverlapStats(a.boxStats,b.boxStats,skipped);
}
void EquivalentAndSparse(){
    const auto reference=Fixture();const auto expected=Raw(reference);
    Require(!expected.fast&&!expected.point&&!expected.extent&&!expected.zero&&!expected.box&&expected.pointHit.item==3,
        "Independent raw fixture no longer hits original index3");
    {State state(reference);Compare(Through(View(state)),expected,0u);}
    for(unsigned kind=0;kind<2u;++kind){auto sparse=reference;
        if(kind==0)sparse.nodes[1].planeX=std::numeric_limits<float>::quiet_NaN();
        else sparse.nodes[1].front=1;
        Refuse([&]{QuestVr::CheckOriginalModelFastLine(sparse,End,Start);});
        Refuse([&]{Hit h=Seed();QuestVr::CheckOriginalModelPoint(h,sparse,101u,0u,End,Start,6u,[]{return Identity();});});
        Refuse([&]{Hit h=Seed();QuestVr::CheckOriginalModelExtent(h,sparse,101u,0u,nullptr,End,Start,Extent);});
        Refuse([&]{Hit h=Seed();QuestVr::CheckOriginalModelPointOverlap(h,sparse,101u,0u,End,{},0u,[]{return Identity();});});
        State state(sparse);state.owned[1]=false;Compare(Through(View(state)),expected,4u);
        Require(state.reads[1]==0u,"Free inactive bytes were numerically read");
        // Region's raw contract is selected-path-only even without a proxy.
        const auto region=QuestVr::QueryOriginalModelPointRegion(sparse,End);
        Require(region.leaf==17&&region.nodeVisits==3u,"Raw Region accidentally acquired full malformed-component validation");
    }
    auto noHull=reference;noHull.nodes[3].collisionBound=-1;noHull.leafHulls.clear();const auto noHullExpected=Raw(noHull);
    auto sparse=noHull;sparse.nodes[1].collisionBound=0;
    Refuse([&]{Hit h=Seed();QuestVr::CheckOriginalModelExtent(h,sparse,101u,0u,nullptr,End,Start,Extent);});
    Refuse([&]{Hit h=Seed();QuestVr::CheckOriginalModelPointOverlap(h,sparse,101u,0u,Inside,Extent,0u,[]{return Identity();});});
    State state(sparse);state.owned[1]=false;Compare(Through(View(state)),noHullExpected,4u);
}
void AllActiveMalformed(){
    for(bool original:{false,true}){auto m=Fixture();m.nodes[0].back=-1;m.nodes[1].planeW=std::numeric_limits<float>::infinity();
        State state(m);state.original=original?2u:1u;state.owned[1]=!original;View view(state);
        Refuse([&]{QuestVr::ModelFastLineDetail::Check(view,End,Start);});
        Refuse([&]{Hit h=Seed();QuestVr::ModelPointLineDetail::Check(h,view,101u,0u,End,Start,6u,[]{return Identity();});});
        Refuse([&]{Hit h=Seed();QuestVr::ModelExtentTraceDetail::Check(h,view,101u,0u,nullptr,End,Start,Extent);});
        Refuse([&]{Hit h=Seed();QuestVr::ModelPointOverlapDetail::Check(h,view,101u,0u,End,{},0u,[]{return Identity();});});
        Refuse([&]{Hit h=Seed();QuestVr::ModelPointOverlapDetail::Check(h,view,101u,0u,Inside,Extent,0u,[]{return Identity();});});
        Require(QuestVr::ModelPointRegionDetail::Query(view,End).nodeVisits==1u,"Region inspected an unselected malformed component");
    }
    auto m=Fixture();m.nodes[0].back=1;State state(m);state.owned[1]=false;View view(state);
    Refuse([&]{QuestVr::ModelFastLineDetail::Check(view,End,Start);});
    Refuse([&]{Hit h=Seed();QuestVr::ModelPointLineDetail::Check(h,view,101u,0u,End,Start,6u,[]{return Identity();});});
    Refuse([&]{Hit h=Seed();QuestVr::ModelExtentTraceDetail::Check(h,view,101u,0u,nullptr,End,Start,Extent);});
    Refuse([&]{Hit h=Seed();QuestVr::ModelPointOverlapDetail::Check(h,view,101u,0u,End,{},0u,[]{return Identity();});});
    Refuse([&]{QuestVr::ModelPointRegionDetail::Query(view,End);});
    Require(QuestVr::ModelPointRegionDetail::Query(view,{1,0,0}).nodeVisits==1u,"Region pruned edge was globally validated");
    auto hull=Fixture();hull.leafHulls[0]=1;State hullState(hull);hullState.owned[1]=false;View hullView(hullState);
    Refuse([&]{Hit h=Seed();QuestVr::ModelExtentTraceDetail::Check(h,hullView,101u,0u,nullptr,End,Start,Extent);});
    Refuse([&]{Hit h=Seed();QuestVr::ModelPointOverlapDetail::Check(h,hullView,101u,0u,Inside,Extent,0u,[]{return Identity();});});
    Hit zero=Seed();Require(!QuestVr::ModelPointOverlapDetail::Check(zero,hullView,101u,0u,End,{},0u,[]{return Identity();}),
        "Zero occupancy consumed inactive leaf hull references");
}
void RequiredReads(){const auto m=Fixture();
    {State s(m);s.owned[1]=false;View v(s);Hit h=Seed();QuestVr::ModelPointLineDetail::Check(h,v,101u,0u,End,Start,6u,[]{return Identity();});
        Require(h.item==3&&s.reads[0]==4u&&s.reads[2]==4u&&s.reads[3]==5u,"Point hit normal reload bypassed node proxy");}
    {State s(m);s.owned[1]=false;View v(s);Hit h=Seed();QuestVr::ModelExtentTraceStats stats;
        QuestVr::ModelExtentTraceDetail::Check(h,v,101u,0u,nullptr,End,Start,Extent,{},&stats);
        Require(stats.hullVisits==1u&&s.reads[3]==7u,"Extent terminal parent/hull plane bypassed node proxy");}
    {State s(m);s.owned[1]=false;View v(s);Hit h=Seed();QuestVr::ModelPointOverlapStats stats;
        QuestVr::ModelPointOverlapDetail::Check(h,v,101u,0u,Inside,Extent,0u,[]{return Identity();},{},&stats);
        Require(stats.hullVisits==1u&&s.reads[3]==7u,"Occupancy terminal parent/hull plane bypassed node proxy");}
}
void LeaseAndAtomicPublication(){const auto m=Fixture();
    {State s(m);View v(s);s.alive=false;Refuse([&]{QuestVr::ModelFastLineDetail::Check(v,End,Start);});}
    {State s(m);View v(s);++s.root;Refuse([&]{QuestVr::ModelPointRegionDetail::Query(v,End);});}
    {State s(m);s.maximumWork=0u;View v(s);Refuse([&]{QuestVr::ModelFastLineDetail::Check(v,End,Start);});Require(s.work==0u,"Kernel reset exhausted caller work");}
    for(bool hit:{false,true}){State s(m);View v(s);Hit h=Seed();const auto before=h;QuestVr::ModelPointTraceStats stats;stats.querySteps=123u;
        unsigned captures{};const auto capture=[&]{++captures;if(captures==(hit?2u:1u))++s.revision;return Identity();};
        const auto end=hit?End:Vec{-1,1,3};
        Refuse([&]{QuestVr::ModelPointLineDetail::Check(h,v,101u,202u,end,Start,6u,capture,{},&stats);});
        Require(captures==(hit?2u:1u)&&Same(h,before)&&stats.querySteps==123u,"Point capture drift published partial packet/stats");
    }
    for(bool box:{false,true}){State s(m);View v(s);Hit h=Seed();const auto before=h;QuestVr::ModelPointOverlapStats stats;stats.querySteps=127u;
        unsigned captures{};Refuse([&]{QuestVr::ModelPointOverlapDetail::Check(h,v,101u,202u,box?Inside:End,box?Extent:Vec{},0u,
            [&]{++captures;++s.revision;return Identity();},{},&stats);});
        Require(captures==1u&&Same(h,before)&&stats.querySteps==127u,"Occupancy capture drift published partial packet/stats");
    }
    // Expire immediately AFTER the final selected node was copied. No later
    // node read can accidentally substitute for the required final lease check.
    {State s(m);s.owned[1]=false;s.expireReadIndex=3;s.expireReadOrdinal=4;View v(s);
        Refuse([&]{QuestVr::ModelFastLineDetail::Check(v,{-1,1,3},Start);});}
    {State s(m);s.owned[1]=false;s.expireReadIndex=3;s.expireReadOrdinal=4;View v(s);Hit h=Seed();const auto before=h;
        QuestVr::ModelPointTraceStats stats;stats.querySteps=141u;
        Refuse([&]{QuestVr::ModelPointLineDetail::Check(h,v,101u,0u,{-1,1,3},Start,6u,[]{return Identity();},{},&stats);});
        Require(Same(h,before)&&stats.querySteps==141u,"Clear point final-read drift published partial packet/stats");}
    {State s(m);s.owned[1]=false;s.expireReadIndex=0;s.expireReadOrdinal=4;View v(s);Hit h=Seed();const auto before=h;
        QuestVr::ModelExtentTraceStats stats;stats.querySteps=143u;
        Refuse([&]{QuestVr::ModelExtentTraceDetail::Check(h,v,101u,0u,nullptr,{1,1,3},{1,1,2},Extent,{},&stats);});
        Require(Same(h,before)&&stats.querySteps==143u,"Miss extent final-read drift published partial packet/stats");}
    for(bool box:{false,true}){State s(m);s.owned[1]=false;s.expireReadIndex=3;s.expireReadOrdinal=box?5u:4u;
        View v(s);Hit h=Seed();const auto before=h;QuestVr::ModelPointOverlapStats stats;stats.querySteps=149u;
        Refuse([&]{QuestVr::ModelPointOverlapDetail::Check(h,v,101u,0u,{-1,1,3},box?Extent:Vec{},0u,[]{return Identity();},{},&stats);});
        Require(Same(h,before)&&stats.querySteps==149u,"Clear occupancy final-read drift published partial packet/stats");}
    {State s(m);s.expireReadIndex=3;s.expireReadOrdinal=1;View v(s);Refuse([&]{QuestVr::ModelPointRegionDetail::Query(v,End);});}
    Model empty;empty.rootOutside=1;const auto nan=std::numeric_limits<float>::quiet_NaN();
    {State s(empty);s.original=0;s.expireEmpty=true;View v(s);Refuse([&]{QuestVr::ModelFastLineDetail::Check(v,{},{});});}
    {State s(empty);s.original=0;s.expireEmpty=true;View v(s);Refuse([&]{QuestVr::ModelPointRegionDetail::Query(v,{nan,nan,nan});});}
    {State s(empty);s.original=0;s.expireEmpty=true;View v(s);Hit h=Seed();const auto before=h;QuestVr::ModelPointTraceStats stats;stats.querySteps=131u;
        unsigned captures{};
        Refuse([&]{QuestVr::ModelPointLineDetail::Check(h,v,0u,202u,{nan,nan,nan},{nan,nan,nan},6u,
            [&]()->QuestVr::OriginalModelCoords{++captures;throw std::runtime_error("inactive capture");},{},&stats);});
        Require(!captures&&Same(h,before)&&stats.querySteps==131u,"Empty point lease drift published or captured inactive Owner");}
    {State s(empty);s.original=0;s.expireEmpty=true;View v(s);Hit h=Seed();const auto before=h;QuestVr::ModelExtentTraceStats stats;stats.querySteps=137u;
        Refuse([&]{QuestVr::ModelExtentTraceDetail::Check(h,v,0u,202u,nullptr,{nan,nan,nan},{nan,nan,nan},{nan,nan,nan},{},&stats);});
        Require(Same(h,before)&&stats.querySteps==137u,"Empty extent lease drift published");}
    {State s(empty);s.original=0;s.expireEmpty=true;View v(s);Hit h=Seed();const auto before=h;QuestVr::ModelPointOverlapStats stats;stats.querySteps=139u;
        unsigned captures{};
        Refuse([&]{QuestVr::ModelPointOverlapDetail::Check(h,v,101u,202u,{nan,nan,nan},{nan,nan,nan},0u,
            [&]()->QuestVr::OriginalModelCoords{++captures;throw std::runtime_error("inactive capture");},{},&stats);});
        Require(!captures&&Same(h,before)&&stats.querySteps==139u,"Empty occupancy lease drift published or captured inactive Owner");}
}
} // namespace
int main(){try{EquivalentAndSparse();AllActiveMalformed();RequiredReads();LeaseAndAtomicPublication();
    std::cout<<"PASS generated active-model kernel checks="<<checks<<" refusals="<<refusals
        <<"; test-only proxy, unchanged raw contracts, no RuntimeObject/GC/root authority or gameplay proof\n";return 0;}
catch(const std::exception& e){std::cerr<<"FAIL generated active-model kernel: "<<e.what()<<'\n';return 1;}}
