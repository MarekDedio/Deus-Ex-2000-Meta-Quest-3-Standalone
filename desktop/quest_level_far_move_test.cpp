#include "quest_level_far_move.h"
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <limits>
namespace {
using namespace QuestVr;std::size_t checks{},refusals{};
void Need(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
template<class F>void Refuse(F&& f){bool caught{};try{f();}catch(const std::exception&){caught=true;}Need(caught,"Expected refusal");++refusals;}
bool Same(PortableModelVec3 a,PortableModelVec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
struct Fixture {
    OriginalLevelFarMoveQueries q;std::vector<std::string> events;
    std::uint32_t eligible=0x80000;std::uint8_t collision=3,standing{},mode{};bool placing{},editor{};
    std::int32_t found=1,encroached{};std::uintptr_t hash=55;std::size_t work{};
    PortableModelVec3 location{},old{};std::vector<std::uintptr_t> actors{10,0,11};std::map<std::uintptr_t,std::uintptr_t> bases{{10,2},{11,2}};
    Fixture(){
        q.work=[&]{++work;};q.eligibilityFlags=[&](auto a){Need(a==2,"Source changed");events.push_back("eligibility");return eligible;};
        q.editor=[&]{events.push_back("editor");return editor?1:0;};
        q.collisionFlags=[&](auto){events.push_back("flags");return collision;};q.collideWhenPlacing=[&](auto){events.push_back("placing");return placing;};
        q.levelHash=[&](auto l){Need(l==1,"Level changed");events.push_back("hash");return hash;};
        q.hashRemove=[&](auto h,auto a){Need(h==hash&&a==2,"Bad Remove");events.push_back("remove");};
        q.hashAdd=[&](auto h,auto a){Need(h==hash&&a==2,"Bad Add");events.push_back("add");};
        q.radius=[&](auto){events.push_back("radius");return 4.f;};q.height=[&](auto){events.push_back("height");return 8.f;};
        q.levelActor0=[&](auto){events.push_back("slot0");return std::uintptr_t{10};};
        q.requireLevelInfo=[&](auto a){Need(a==10,"Bad LevelInfo cast");events.push_back("info");};
        q.netMode=[&](auto a){Need(a==10,"Bad NetMode receiver");events.push_back("mode");return mode;};
        q.findSpot=[&](auto l,auto extent,auto& at,auto a,auto b){Need(l==1&&Same(extent,{4,4,8})&&!a&&!b,"FindSpot args changed");
            events.push_back("find");at.z+=1;return found;};
        q.rotation=[&](auto,unsigned i){events.push_back("rotation");return static_cast<std::int32_t>(i+7);};
        q.encroachment=[&](auto l,auto s,auto at,auto r,auto touch){Need(l==1&&s==2&&Same(at,{20,30,41})&&r==std::array<std::int32_t,3>{7,8,9}&&touch==1,"Encroachment args changed");
            events.push_back("encroach");return encroached;};
        q.standingCount=[&](auto){events.push_back("standing");return standing;};
        q.actorCount=[&](auto){events.push_back("count");return static_cast<std::int32_t>(actors.size());};
        q.actorAt=[&](auto,std::int32_t i){events.push_back("slot");return actors.at(i);};q.base=[&](auto a){events.push_back("base");return bases[a];};
        q.setBase=[&](auto child,auto base,auto send){Need(!base&&send==1,"Child SetBase args changed");events.push_back("detach");bases[child]=0;
            if(child==10){actors.push_back(12);bases[12]=2;}};
        q.justTeleported=[&](auto){events.push_back("teleported");};
        q.writeLocation=[&](auto,unsigned c,float v){events.push_back("location");if(c==0)location.x=v;else if(c==1)location.y=v;else location.z=v;};
        q.writeOldLocation=[&](auto,unsigned c,float v){events.push_back("old");if(c==0)old.x=v;else if(c==1)old.y=v;else old.z=v;};
        q.setZone=[&](auto l,auto a,auto,auto force){Need(l==1&&a==2&&!force,"Bad zone args");events.push_back("zone");};
    }
    int Run(int test=0,int noCheck=0){return MoveOriginalLevelFarActor(1,2,{20,30,40},test,noCheck,q);}
};
void Run(){
    Fixture f;Need(f.Run()==1&&Same(f.location,{20,30,41})&&Same(f.old,f.location),"FarMove did not publish both absolute vectors");
    const std::vector<std::string> expected{"eligibility","flags","hash","remove","flags","radius","height","radius","find",
        "rotation","rotation","rotation","encroach","standing","teleported","location","location","location","old","old","old","flags","hash","add","zone"};
    Need(f.events==expected,"Native FarMove callback transcript changed");Need(f.work==f.events.size(),"Work omitted a native provider call");
    Fixture fixed;fixed.eligible|=1;Need(!fixed.Run()&&fixed.events==std::vector<std::string>({"eligibility","editor"}),"Static refusal touched inactive hash/pose");
    Fixture inactive;inactive.eligible|=1;
    Need(MoveOriginalLevelFarActor(0,2,{std::numeric_limits<float>::quiet_NaN(),0,0},0,0,inactive.q)==0,
        "Native static eligibility must not inspect inactive Level or location");
    Fixture veto;veto.found=0;Need(!veto.Run()&&Same(veto.location,{})&&veto.events.back()=="add","Placement veto must re-add without teleport/zone");
    Need(std::find(veto.events.begin(),veto.events.end(),"encroach")==veto.events.end(),"Veto called encroachment");
    Fixture enc;enc.encroached=-1;Need(!enc.Run()&&enc.events.back()=="add"&&Same(enc.old,{}),"Encroachment veto changed pose or omitted Add");
    Fixture test;test.found=-7;Need(test.Run(-1)==-7&&Same(test.location,{20,30,41})&&Same(test.old,test.location),"Test must preserve integer result and write both vectors");
    Need(std::find(test.events.begin(),test.events.end(),"encroach")==test.events.end()&&std::find(test.events.begin(),test.events.end(),"teleported")==test.events.end(),"Test called ordinary encroach/teleport tail");
    Fixture bypass;Need(bypass.Run(0,-1)==1&&Same(bypass.location,{20,30,40}),"NoCheck path did not retain input");
    Need(std::find(bypass.events.begin(),bypass.events.end(),"find")==bypass.events.end()&&std::find(bypass.events.begin(),bypass.events.end(),"encroach")==bypass.events.end(),"NoCheck queried inactive services");
    Fixture children;children.standing=1;Need(children.Run()==1&&children.bases[10]==0&&children.bases[11]==0&&children.bases[12]==0,"Live appended based child not detached");
    Fixture changed;changed.q.encroachment=[&](auto,auto,auto,auto,auto){changed.collision=0;changed.events.push_back("encroach");return 0;};
    Need(changed.Run()==1&&std::find(changed.events.begin(),changed.events.end(),"add")==changed.events.end(),"Add failed to reread changed collision flags");
    Fixture placement;placement.collision=0;placement.placing=true;placement.mode=3;
    placement.q.encroachment=[](auto,auto,auto at,auto,auto){Need(Same(at,{20,30,40}),"Skipped FindSpot changed candidate");return 0;};
    Need(placement.Run(0,0)==1&&std::find(placement.events.begin(),placement.events.end(),"find")==placement.events.end(),"Client NetMode must skip placement-only FindSpot");
    Fixture failed;failed.q.findSpot={};Refuse([&]{failed.Run();});Need(Same(failed.location,{}),"Missing provider fabricated success");
    Fixture bound;bound.standing=1;Refuse([&]{MoveOriginalLevelFarActor(1,2,{20,30,40},0,0,bound.q,1);});
}
}
int main(){try{Run();std::cout<<"Original FarMoveActor: "<<checks<<" checks / "<<refusals<<" refusals; detached complete provider transcript\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
