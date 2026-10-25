#include "quest_level_player_distance.h"
#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {
using Q=QuestVr::OriginalLevelPlayerDistanceQueries;
constexpr std::uintptr_t Level=1u,Player=2u,A=3u,B=4u,C=5u;
std::size_t checks{},refusals{};
void Require(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
template<class F>void Refuses(F action){bool yes{};try{action();}catch(const std::exception&){yes=true;}++refusals;Require(yes,"Missing explicit refusal");}
struct Fixture {
    Q q;std::vector<std::uintptr_t> slots{Player,A,0u,B};
    std::map<std::uintptr_t,std::array<float,3u>> positions{{Player,{1,2,3}},{A,{4,6,3}},{B,{1,2,15}},{C,{1,2,3}}};
    std::map<std::uintptr_t,float> distances;
    std::vector<std::string> trace;
    std::uint8_t mode{};std::uintptr_t player{Player};bool pawn{true},hasPlayer{true};
    std::int32_t first{1};std::size_t work{},cut{~std::size_t{}},writeCut{~std::size_t{}};
    std::function<void(std::uintptr_t)> afterWrite;
    std::function<void(std::int32_t)> afterSlot;
    Fixture() {
        q.work=[&]{if(work==cut)throw std::runtime_error("work");++work;};
        q.netMode=[&](auto l){Require(l==Level,"Bad level");trace.push_back("mode");return mode;};
        q.firstViewportActor=[&](auto l){Require(l==Level,"Bad level");trace.push_back("viewport");return player;};
        q.isPlayerPawn=[&](auto p){Require(p==Player,"Bad player");trace.push_back("pawn");return pawn;};
        q.hasPlayer=[&](auto p){Require(p==Player,"Bad player");trace.push_back("hasPlayer");return hasPlayer;};
        q.firstDynamic=[&](auto l){Require(l==Level,"Bad level");trace.push_back("first");return first;};
        q.actorCount=[&](auto l){Require(l==Level,"Bad level");trace.push_back("count");return static_cast<std::int32_t>(slots.size());};
        q.actorSlot=[&](auto l,std::int32_t i){Require(l==Level,"Bad level");trace.push_back("slot"+std::to_string(i));
            const auto value=slots.at(static_cast<std::size_t>(i));if(afterSlot)afterSlot(i);return value;};
        q.location=[&](auto a,unsigned c){trace.push_back(std::to_string(a)+"loc"+std::to_string(c));return positions.at(a).at(c);};
        q.writeDistance=[&](auto a,float d){if(distances.size()==writeCut)throw std::runtime_error("writes");
            trace.push_back("write"+std::to_string(a));distances[a]=d;if(afterWrite)afterWrite(a);};
    }
    auto Run(){return QuestVr::AdvanceOriginalLevelPlayerDistance(Level,q);}
};
void Protocol() {
    Fixture f;const auto s=f.Run();
    Require(s.writes==2u&&s.visits==3u&&s.nullSlots==1u&&s.firstDynamic==1&&s.player==Player,"Incorrect pass census");
    Require(f.distances.at(A)==5.0f&&f.distances.at(B)==12.0f,"Wrong actual distances");
    const std::vector<std::string> expected{"mode","viewport","pawn","hasPlayer","first","2loc0","2loc1","2loc2",
        "count","slot1","3loc2","3loc1","3loc0","slot1","write3","count","slot2",
        "count","slot3","4loc2","4loc1","4loc0","slot3","write4","count"};
    Require(f.trace==expected&&s.steps==expected.size()&&f.work==s.steps,"Native provider order changed");
    for(unsigned mode=1u;mode<256u;++mode){Fixture n;n.mode=static_cast<std::uint8_t>(mode);n.q.firstViewportActor={};
        const auto v=n.Run();Require(v.networkSkipped&&v.steps==1u&&v.writes==0u,"Nonzero NetMode did not lazily skip");}
    {Fixture n;n.player=0u;n.q.isPlayerPawn={};Require(n.Run().noActor&&n.trace.size()==2u,"Null viewport Actor did not skip");}
    {Fixture n;n.pawn=false;n.q.hasPlayer={};Require(n.Run().notPlayerPawn&&n.trace.size()==3u,"NonPlayerPawn did not skip");}
    {Fixture n;n.hasPlayer=false;n.q.firstDynamic={};Require(n.Run().noPlayer&&n.trace.size()==4u,"Null Player did not skip");}
    {Fixture n;n.first=4;const auto v=n.Run();Require(v.visits==0u&&v.writes==0u&&v.steps==9u,"Empty range skipped capture or overran array");}
    {Fixture n;n.positions[Player][0]=std::numeric_limits<float>::quiet_NaN();n.first=4;
        Require(n.Run().writes==0u,"Inactive arithmetic eagerly rejected native captured bits");}
    {Fixture n;n.afterWrite=[&](auto a){if(a==A){n.positions[Player]={100,200,300};n.slots.push_back(C);}};
        const auto v=n.Run();Require(v.writes==3u&&n.distances.at(B)==12.0f&&n.distances.at(C)==0.0f&&v.capturedLocation[0]==1.0f,
            "Player location was not captured once or appended live Actor was omitted");}
    {Fixture n;unsigned visits{};n.afterSlot=[&](auto i){if(i==1&&++visits==1u)n.slots[1u]=C;};
        const auto v=n.Run();Require(v.writes==2u&&!n.distances.contains(A)&&n.distances.at(C)==5.0f,
            "Distance store did not reread its actual destination slot after source calculation");}
    {Fixture n;unsigned visits{};n.afterSlot=[&](auto i){if(i==1&&++visits==1u)n.slots[1u]=0u;};Refuses([&]{n.Run();});}
    {Fixture n;n.afterWrite=[&](auto){n.slots.resize(2u);};Require(n.Run().writes==1u,"Loop cached original live Actor count");}
}
void Arithmetic() {
    using QuestVr::LevelPlayerDistanceDetail::Distance;
    for(int x=-20;x<=20;++x)for(int y=-20;y<=20;++y) {
        const std::array<float,3u> origin{10000,-20000,30000},point{10000.0f+x,-20000.0f+y,30000.0f};
        const float actual=Distance(point,origin);
        const float expected=static_cast<float>(std::sqrt(static_cast<double>(x*x+y*y)));
        Require(actual==expected,"Integer-distance numeric oracle disagrees");
    }
    Require(std::bit_cast<std::uint32_t>(Distance({-0.0f,0.0f,-0.0f},{0.0f,-0.0f,0.0f}))==0u,"Zero distance is not positive zero");
    Require(Distance({std::numeric_limits<float>::denorm_min(),0,0},{0,0,0})==std::numeric_limits<float>::denorm_min(),
        "Subnormal native distance was lost");
    Refuses([&]{Distance({std::numeric_limits<float>::max(),0,0},{-std::numeric_limits<float>::max(),0,0});});
    for(float invalid:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        Refuses([&]{Distance({invalid,0,0},{0,0,0});});Refuses([&]{Distance({0,0,0},{0,invalid,0});});
    }
}
void Boundaries() {
    {Fixture f;Refuses([&]{QuestVr::AdvanceOriginalLevelPlayerDistance(0u,f.q);});Require(!f.work,"Null Level charged work");}
    {Fixture f;Refuses([&]{QuestVr::AdvanceOriginalLevelPlayerDistance(Level,f.q,65537u);});}
    {Fixture f;f.first=-1;Refuses([&]{f.Run();});}
    {Fixture f;f.first=65537;Refuses([&]{f.Run();});}
    for(auto count:{-1,65537}){Fixture f;f.q.actorCount=[&](auto){return count;};Refuses([&]{f.Run();});}
    for(unsigned missing=0u;missing<9u;++missing) {
        Fixture f;
        switch(missing){case 0:f.q.netMode={};break;case 1:f.q.firstViewportActor={};break;
        case 2:f.q.isPlayerPawn={};break;case 3:f.q.hasPlayer={};break;case 4:f.q.firstDynamic={};break;
        case 5:f.q.actorCount={};break;case 6:f.q.actorSlot={};break;case 7:f.q.location={};break;
        case 8:f.q.writeDistance={};break;}
        Refuses([&]{f.Run();});
    }
    Fixture all;const auto full=all.Run();
    for(std::size_t cut=0;cut<full.steps;++cut) {
        Fixture f;f.cut=cut;Refuses([&]{f.Run();});
        Require(f.work==cut&&std::equal(f.trace.begin(),f.trace.end(),all.trace.begin()),"Shared-work prefix changed provider order");
        f.distances.clear();Require(f.distances.empty(),"Caller journal failed to undo stores");
    }
    for(std::size_t prefix=0u;prefix<2u;++prefix){Fixture f;f.writeCut=prefix;Refuses([&]{f.Run();});Require(f.distances.size()==prefix,"Write prefix escaped");}
    {Fixture f;f.cut=full.steps;Require(f.Run().writes==2u,"Exact successful shared-work allowance refused");}
}
}
int main(){try{Protocol();Arithmetic();Boundaries();std::cout<<"PASS original Level player-distance: "<<checks<<" checks, "<<refusals
    <<" refusals; actual lazy gates/capture/live source and destination slots/PC64 arithmetic/work prefixes; NOT world Tick or live gravity.\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL original Level player-distance: "<<e.what()<<'\n';return 1;}}
