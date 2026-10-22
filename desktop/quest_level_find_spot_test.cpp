#include "quest_level_find_spot.h"
#include <iostream>
#include <vector>
#include <string>
#include <limits>
namespace {
using namespace QuestVr;
std::size_t checks{},refusals{};
void Need(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
bool Same(PortableModelVec3 a,PortableModelVec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
template<class F>void Refuse(F&& f){bool caught{};try{f();}catch(const std::exception&){caught=true;}Need(caught,"Expected refusal");++refusals;}
void Run(){
    OriginalLevelFindSpotQueries q;std::size_t lines{},points{},info{},work{};
    std::vector<PortableModelVec3> ends,starts;std::vector<std::uintptr_t> worlds;
    q.work=[&]{++work;};q.freshLevelInfo=[&]{return ++info;};
    q.lineCheck=[&](auto& hit,auto source,auto end,auto start,auto extent,auto flags,auto extra){
        Need(!source&&Same(extent,{})&&flags==6&&!extra,"AdjustSpot native query changed");
        ++lines;ends.push_back(end);starts.push_back(start);hit.time=1;return 0;};
    int reply=1;q.pointCheck=[&](auto&,auto,auto,auto flags,auto world,auto actors){
        Need(!flags&&actors==0,"FindSpot point arguments changed");++points;worlds.push_back(world);return reply;};
    PortableModelVec3 at{10,20,30};
    Need(FindOriginalLevelSpot({},at,0,0,q)==1&&lines==0&&points==1&&Same(at,{10,20,30}),"Zero extent must only point-check");
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    Need(FindOriginalLevelSpot({nan,0,nan},at,0,0,q)==1&&lines==0&&points==2,"Unordered/zero extent branch changed");
    reply=2;Need(FindOriginalLevelSpot({},at,0,0,q)==0,"Point result must be exactly1");reply=1;
    lines=points=info=0;ends.clear();starts.clear();worlds.clear();
    Need(FindOriginalLevelSpot({2,3,4},at,0,0,q)==1&&lines==6&&points==1,"FarMove must skip initial point check but do all six axes");
    const std::vector<PortableModelVec3> expected{{8,20,30},{10,17,30},{10,20,26},{12,20,30},{10,23,30},{10,20,34}};
    for(std::size_t i=0;i<6;++i)Need(Same(ends[i],expected[i])&&Same(starts[i],at),"Axis schedule/sign order changed");
    lines=points=0;Need(FindOriginalLevelSpot({2,3,4},at,0,-1,q)==1&&lines==0&&points==1,"AssumeFit fast query omitted");
    lines=points=info=0;ends.clear();starts.clear();worlds.clear();reply=0;
    Need(FindOriginalLevelSpot({2,3,4},at,0,0,q)==0&&lines==14&&points==2&&Same(at,{10,20,30}),"Corner failure mutated caller or skipped search");
    std::size_t n=6;for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})
        Need(Same(ends[n++],{10+2.f*x,20+3.f*y,30+4.f*z}),"Corner schedule is not X/Y/Z nested sign order");
    Need(worlds==std::vector<std::uintptr_t>{1,2},"LevelInfo must be obtained freshly at each point site");
    // First hit shifts S by .55*2 =1.1 along X. Every subsequent endpoint
    // must start at the already adjusted candidate, not the original center.
    lines=points=0;reply=1;ends.clear();starts.clear();
    q.lineCheck=[&](auto& hit,auto,auto end,auto start,auto,auto,auto){
        ++lines;ends.push_back(end);starts.push_back(start);hit.time=lines==1?.5f:1.f;hit.normal={1,0,0};return 1;};
    Need(FindOriginalLevelSpot({2,3,4},at,0,0,q)==1&&lines==6&&at.x>11.09f&&at.x<11.11f,"Hit adjustment/candidate commit wrong");
    Need(starts[1].x==at.x&&ends[1].x==at.x,"Adjusted candidate not fed into next axis");
    OriginalActorLineHit hit;PortableModelVec3 candidate{1,2,3};q.lineCheck={};
    Refuse([&]{AdjustOriginalLevelSpot(hit,candidate,{},1,q);});Need(Same(candidate,{1,2,3}),"Provider refusal changed candidate");
    q.lineCheck=[](auto& h,auto,auto,auto,auto,auto,auto){h.time=std::numeric_limits<float>::quiet_NaN();return 1;};
    Refuse([&]{AdjustOriginalLevelSpot(hit,candidate,{},1,q);});
    Need(work>0,"Missing caller work accounting");
}
}
int main(){try{Run();std::cout<<"Original FindSpot/AdjustSpot: "<<checks<<" checks / "<<refusals<<" refusals; detached providers, not Training acceptance\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
