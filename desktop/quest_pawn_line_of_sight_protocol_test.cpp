#include "quest_pawn_line_of_sight_protocol.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Vec=PortableModelVec3;
std::size_t checks{},refusals{},calls{};
void Require(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
bool Same(const Vec& a,const Vec& b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
struct Ray{Vec end,start;};
struct Ops {
    struct Flags{bool losFlag{},player{};};
    Vec source{},target{100,0,0},currentEnemy{200,300,400},lastSeeing{},lastSeen{};
    float eye{10.0f},height{50.0f},radius{20.0f};
    bool player{true},enemy{},side{};
    Flags flags{false,true};
    std::uint8_t visibility{255u};std::uint32_t draw{32767u};
    std::vector<bool> answers,enemyAnswers;
    std::vector<Ray> rays;
    std::vector<std::string> events;
    std::map<std::string,std::size_t> reads;
    std::string fault;std::size_t faultOccurrence{1u};
    std::function<void(Ops&,std::size_t)> afterQuery;
    std::function<void(Ops&)> afterLastSeeing;
    void Mark(const char* event){events.emplace_back(event);const auto count=++reads[event];
        if(fault==event&&count==faultOccurrence)throw std::runtime_error("fixture:"+fault);
    }
    bool TargetIsEnemy(){Mark("enemy");const auto slot=reads["enemy"]-1u;return slot<enemyAnswers.size()?enemyAnswers[slot]:enemy;}
    Vec TargetLocation(){Mark("target");return target;}
    Vec SourceLocation(){Mark("source");return source;}
    bool IsPlayer(){Mark("player");return player;}
    std::uint8_t Visibility(){Mark("visibility");return visibility;}
    float BaseEyeHeight(){Mark("eye");return eye;}
    float CollisionHeight(){Mark("height");return height;}
    float CollisionRadius(){Mark("radius");return radius;}
    Flags DistantFlags(){Mark("flags");return flags;}
    bool SideFlag(){Mark("side");return side;}
    std::uint32_t Draw15(){Mark("draw");return draw;}
    bool FastLine(const Vec& end,const Vec& start){
        Mark("line");Require(rays.size()<6u,"Protocol exceeded original maximum six visibility queries");
        const auto slot=rays.size();rays.push_back({end,start});
        const bool result=slot<answers.size()?answers[slot]:false;
        if(afterQuery)afterQuery(*this,slot);
        return result;
    }
    void WriteLastSeeingPos(const Vec& value){Mark("seeing");lastSeeing=value;if(afterLastSeeing)afterLastSeeing(*this);}
    void WriteLastSeenPosFromCurrentEnemy(){Mark("seen");lastSeen=currentEnemy;}
};
bool Run(Ops& ops){++calls;return QuestVr::CheckOriginalPawnLineOfSightToPawn(ops);}
void Throws(Ops& ops,const char* expected=nullptr){
    bool threw{};try{static_cast<void>(Run(ops));}catch(const std::exception& error){threw=true;
        if(expected)Require(std::string(error.what())==expected,"Protocol replaced the original provider failure");}
    Require(threw,"Reached failed visibility provider became a fabricated true/false result");++refusals;
}
void BasicOrder(){
    Ops ops;ops.answers={true};
    Require(Run(ops),"Near non-enemy head visibility failed");
    Require(ops.events==std::vector<std::string>{"enemy","target","source","player","visibility",
        "source","enemy","eye","target","height","line"},"Near head lazy field/query order changed");
    Require(ops.rays.size()==1u&&Same(ops.rays[0].end,{100,0,40})&&Same(ops.rays[0].start,{0,0,10}),
        "Head query reversed End/Start or changed head/eye endpoint");
    Require(!ops.reads["seeing"]&&!ops.reads["seen"]&&!ops.reads["radius"]&&!ops.reads["flags"]&&!ops.reads["draw"],
        "Near head success read an inactive branch or wrote enemy history");
    for(const auto& enemyAnswers:{std::vector<bool>{true,false},std::vector<bool>{false,true}}){
        Ops changed;changed.enemyAnswers=enemyAnswers;changed.answers={true};
        Require(Run(changed)&&changed.reads["enemy"]==2u,"Entry Enemy read replaced required later live Enemy read");
        Require(changed.reads["height"]==(enemyAnswers[1]?0u:1u)&&changed.reads["seeing"]==(enemyAnswers[1]?1u:0u),
            "Protocol used redundant entry Enemy result instead of eye-snapshot branch result");
    }
}
void EnemyQueriesAndWrites(){
    for(unsigned clear=0u;clear<=6u;++clear){
        Ops ops;ops.enemy=true;ops.radius=0;ops.answers.assign(6u,false);
        if(clear)ops.answers[clear-1u]=true;
        Require(Run(ops)==(clear!=0u),"Enemy visibility acceptance or blocked result changed");
        Require(ops.rays.size()==(clear?clear:6u),"Enemy query prefix/max bound changed");
        Require(ops.reads["seeing"]==(clear==1u||clear==2u?1u:0u)&&
            ops.reads["seen"]==ops.reads["seeing"],"Head/side success invented enemy history stores");
        if(clear==1u||clear==2u){
            Require(ops.events[ops.events.size()-3u]=="source"&&ops.events[ops.events.size()-2u]=="seeing"&&
                ops.events.back()=="seen","Enemy history source read/store order changed");
        }
        if(!clear){
            Require(ops.reads["side"]==1u&&ops.reads["radius"]==1u,"Fixed sight0 skipped required side-flag read");
            for(std::size_t index=3u;index<6u;++index)Require(Same(ops.rays[index].end,ops.target),
                "Zero-radius tied corners changed endpoint or skipped more than min/max index0");
        }
    }
    Ops changed;changed.enemy=true;changed.answers={false,true};
    changed.afterQuery=[](Ops& ops,std::size_t index){
        if(index==0u){ops.source={1,2,3};ops.target={40,50,60};}
        else{ops.source={7,8,9};ops.currentEnemy={70,80,90};}
    };
    changed.afterLastSeeing=[](Ops& ops){ops.currentEnemy={700,800,900};};
    Require(Run(changed),"Second enemy centre visibility failed");
    Require(Same(changed.rays[0].end,{100,0,0})&&Same(changed.rays[0].start,{0,0,10})&&
        Same(changed.rays[1].end,{40,50,60})&&Same(changed.rays[1].start,{1,2,3}),
        "Enemy second query reused a previous target/source snapshot");
    Require(Same(changed.lastSeeing,{7,8,9})&&Same(changed.lastSeen,{700,800,900}),
        "Enemy history stores cached source/Enemy rather than reading after clear/first write");
    const auto first=std::find(changed.events.begin(),changed.events.end(),"line");
    Require(first!=changed.events.end()&&std::vector<std::string>(first,first+4)==
        std::vector<std::string>{"line","source","target","line"},"Second enemy query read target before source Start");
}
void FarBranches(){
    for(bool isPlayer:{false,true})for(bool los:{false,true})for(const auto draw:{0u,16383u,16384u,32767u}){
        Ops ops;ops.target={2000,0,0};ops.flags={los,isPlayer};ops.draw=draw;ops.answers={true};
        const auto expected=isPlayer||QuestVr::PawnLineOfSightDetail::RandomVisible(draw);
        Require(Run(ops)==expected,"Far branch player/RNG visibility gate changed");
        Require(ops.reads["draw"]==(isPlayer?0u:1u)&&ops.rays.size()==(expected?1u:0u),
            "Far branch RNG was eager, omitted or queried after failed draw");
        Require(!ops.reads["height"]&&!ops.reads["radius"]&&!ops.reads["side"]&&!ops.reads["seeing"],
            "Far centre path fell through to head/corners or enemy stores");
        if(expected)Require(Same(ops.rays[0].end,ops.target)&&Same(ops.rays[0].start,{0,0,10}),
            "Far centre query changed directed target/eye endpoints");
    }
    {
        Ops ops;ops.target={4000,0,0};ops.flags={false,false};ops.fault="draw";
        Require(!Run(ops)&&ops.rays.empty()&&!ops.reads["draw"],"Half-range rejection consumed RNG or queried BSP");
    }
    for(const float x:{2000.0f,std::nextafter(2000.0f,std::numeric_limits<float>::infinity())}){
        Ops ops;ops.player=false;ops.flags={false,true};ops.target={x,2000,0};ops.answers={true};
        const bool accepted=x==2000.0f;
        Require(Run(ops)==accepted&&ops.rays.size()==(accepted?1u:0u),"Half retained-range equality is not inclusive");
    }
    for(bool enemy:{false,true}){
        Ops ops;ops.enemy=enemy;ops.target={2000,0,0};ops.flags={true,true};ops.answers={false,false};
        Require(!Run(ops)&&ops.rays.size()==(enemy?2u:1u)&&!ops.reads["height"]&&!ops.reads["radius"],
            "Blocked far ray incorrectly reached head/corner fallback");
    }
    {
        Ops ops;ops.target={2000,0,0};ops.player=true;ops.flags={false,false};ops.draw=0u;ops.fault="line";
        Require(!Run(ops)&&ops.reads["draw"]==1u&&!ops.reads["line"],
            "Far branch reused earlier bIsPlayer instead of its live paired flags read");
    }
}
void BoundaryAndCornerOrder(){
    for(const float x:{1000.0f,std::nextafter(1000.0f,std::numeric_limits<float>::infinity())}){
        Ops ops;ops.target={x,0,0};ops.answers={true};
        Require(Run(ops),"Million-square boundary fixture unexpectedly blocked");
        Require(ops.reads["height"]==(x==1000.0f?1u:0u)&&ops.reads["flags"]==(x==1000.0f?0u:1u),
            "Million-square head/far boundary changed");
    }
    for(const float x:{500.0f,std::nextafter(500.0f,std::numeric_limits<float>::infinity())}){
        Ops ops;ops.target={x,0,0};
        Require(!Run(ops),"Blocked corner boundary fixture passed");
        Require(ops.reads["radius"]==(x==500.0f?1u:0u)&&ops.reads["side"]==ops.reads["radius"],
            "Quarter-million-square corner boundary changed");
    }
    for(bool player:{false,true})for(bool beyond:{false,true}){
        Ops ops;ops.player=player;ops.flags={true,true};const float limit=player?5000.0f:4000.0f;
        ops.target={beyond?std::nextafter(limit,std::numeric_limits<float>::infinity()):limit,0,0};ops.answers={true};
        Require(Run(ops)==!beyond&&ops.rays.size()==(beyond?0u:1u)&&ops.reads["eye"]==(beyond?0u:1u),
            "Visibility retained-range equality or lazy eye boundary changed");
    }
    Ops ops;ops.target={100,40,7};ops.source={0,0,0};ops.radius=10;
    const auto corners=QuestVr::PawnLineOfSightDetail::SelectCorners(ops.target,ops.radius);
    Require(!Run(ops),"Fully blocked corner fixture passed");
    std::size_t ray=1u;
    for(std::size_t index=0u;index<4u;++index)if(index!=corners.minimum&&index!=corners.maximum){
        Require(ray<ops.rays.size()&&Same(ops.rays[ray].end,corners.points[index]),"Corner pruning/order changed");++ray;
    }
    Require(ray==ops.rays.size(),"Unexpected extra side ray");
    const auto radius=std::find(ops.events.begin(),ops.events.end(),"radius");
    Require(radius!=ops.events.end()&&radius+2<ops.events.end()&&radius[1]=="target"&&radius[2]=="side",
        "Corner read order changed or side flag was read before selection");
}
void LazyFailures(){
    {
        Ops ops;ops.target={6000,0,0};ops.eye=ops.height=ops.radius=std::numeric_limits<float>::quiet_NaN();ops.fault="eye";
        Require(!Run(ops)&&ops.events==std::vector<std::string>{"enemy","target","source","player","visibility"},
            "Out-of-range query read inactive geometry/eye fields");
    }
    {
        Ops ops;ops.target.x=std::numeric_limits<float>::quiet_NaN();ops.fault="player";
        Throws(ops);Require(!ops.reads["player"]&&!ops.reads["visibility"]&&!ops.reads["eye"],
            "Undefined mandatory distance read later fields before refusal");
    }
    {
        Ops ops;ops.target.x=std::numeric_limits<float>::max();ops.fault="player";
        Throws(ops);Require(!ops.reads["player"],"Distance Float overflow was deferred past range fields");
    }
    {
        Ops ops;ops.enemy=true;ops.height=ops.radius=std::numeric_limits<float>::quiet_NaN();ops.answers={true};
        Require(Run(ops)&&!ops.reads["height"]&&!ops.reads["radius"],"Enemy centre success eagerly validated inactive head/radius");
    }
    for(const auto* field:{"radius","flags","draw","side"}){
        Ops ops;ops.answers={true};ops.fault=field;
        Require(Run(ops)&&!ops.reads[field],"Near head success evaluated an inactive provider");
    }
    for(const auto* field:{"enemy","target","source","player","visibility","eye","height","line","radius","side"}){
        Ops ops;ops.fault=field;Throws(ops,(std::string("fixture:")+field).c_str());
    }
    {
        Ops ops;ops.radius=std::numeric_limits<float>::quiet_NaN();Throws(ops);
        Require(!ops.reads["side"],"Corner finite refusal happened after unused side-flag read");
    }
    for(unsigned query=1u;query<=6u;++query){
        Ops ops;ops.enemy=true;ops.radius=0;ops.fault="line";ops.faultOccurrence=query;
        Throws(ops,"fixture:line");Require(ops.reads["line"]==query&&!ops.reads["seeing"],
            "Failed native query was converted to blocked or continued another ray");
    }
    for(const auto* field:{"seeing","seen"}){
        Ops ops;ops.enemy=true;ops.answers={true};ops.fault=field;
        Throws(ops,(std::string("fixture:")+field).c_str());
        Require(ops.reads["seeing"]==1u&&ops.reads["seen"]==(ops.fault=="seen"?1u:0u),
            "Enemy write fault skipped earlier write or continued after first failure");
    }
    {
        Ops ops;ops.target={2000,0,0};ops.flags={false,false};ops.draw=32768u;Throws(ops);
        Require(!ops.reads["line"],"Invalid reached RNG draw was fabricated into visibility");
    }
}
}
int main(){try{BasicOrder();EnemyQueriesAndWrites();FarBranches();BoundaryAndCornerOrder();LazyFailures();
    std::cout<<"PASS original Pawn LOS protocol calls="<<calls<<" checks="<<checks<<" refusals="<<refusals<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<"FAIL original Pawn LOS protocol: "<<error.what()<<'\n';return 1;}}
