#include "quest_multi_line_trace.h"
#include "quest_actor_line_trace_x87_oracle.h"

#include <iostream>
#include <limits>
#include <unordered_set>

namespace {
using Vec=PortableModelVec3;
using Hit=QuestVr::OriginalActorLineHit;
using Arena=QuestVr::OriginalLineHitArena;
using Reply=QuestVr::OriginalWorldLineReply;
using Queries=QuestVr::OriginalMultiLineQueries;
using Result=QuestVr::OriginalMultiLineResult;
std::size_t checks{},refusals{},differential{},corrections{},nearWorld{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(float a,float b) {return Bits(a)==Bits(b);}
bool Same(const Vec& a,const Vec& b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Hit& a,const Hit& b,bool next=true) {return (!next||a.next==b.next)&&a.actor==b.actor&&a.primitive==b.primitive&&
    a.item==b.item&&Same(a.time,b.time)&&Same(a.location,b.location)&&Same(a.normal,b.normal);}
Hit Packet(std::uintptr_t actor,float time,Vec location={},Vec normal={0,0,1}) {
    return {0,actor,actor+900u,location,normal,time,static_cast<std::int32_t>(actor+100u)};
}
Arena Chain(std::vector<Hit> records) {
    for(std::size_t i=0;i<records.size();++i) records[i].next=i+1u<records.size()?i+2u:0u;
    const auto first=records.empty()?0u:1u;return {std::move(records),first};
}
template<class F> void Refuse(F run) {
    bool failed{};try {run();} catch(const std::exception&) {failed=true;}
    Require(failed,"Malformed/nonfinite MultiLine query silently succeeded");++refusals;
}
struct Fixture {
    Reply world;
    Arena actors;
    std::unordered_set<std::uintptr_t> movers;
    unsigned worlds{},hashes{},classes{};
    Vec seenEnd{},seenStart{},seenExtent{};
    std::uint32_t seenWorldFlags{};std::uint8_t seenActorFlags{};
    std::vector<unsigned> order;
    Queries Providers() {return {
        [this](std::uintptr_t identity,const Vec& end,const Vec& start,const Vec& extent,std::uint32_t flags) {
            Require(identity==99u,"Collector redirected supplied native LevelInfo");
            ++worlds;order.push_back(1);seenEnd=end;seenStart=start;seenExtent=extent;seenWorldFlags=flags;return world;},
        [this](const Vec& end,const Vec& start,const Vec& extent,std::uint8_t flags) {
            ++hashes;order.push_back(2);seenEnd=end;seenStart=start;seenExtent=extent;seenActorFlags=flags;return actors;},
        [this](std::uintptr_t identity) {++classes;order.push_back(3);return movers.contains(identity);}};}
    Result Run(Vec end={10,0,0},Vec start={},Vec extent={},std::int32_t checkActors=1,std::uintptr_t worldActor=99,
        const QuestVr::OriginalMultiLineLimits& limits={}) {
        return QuestVr::CollectOriginalMultiLineHits(end,start,extent,checkActors,worldActor,255u,Providers(),limits);
    }
};
void Links(const Result& result) {
    Require(result.hits.first==(result.hits.records.empty()?0u:1u),"Detached output first link differs");
    for(std::size_t i=0;i<result.hits.records.size();++i)
        Require(result.hits.records[i].next==(i+1u<result.hits.records.size()?i+2u:0u),"Sorted output links retain stale input handles");
}
void Controls() {
    const float nan=std::numeric_limits<float>::quiet_NaN();
    Fixture inactive;auto providers=inactive.Providers();providers.actors={};
    auto result=QuestVr::CollectOriginalMultiLineHits({nan,nan,nan},{nan,nan,nan},{nan,nan,nan},1,0,0,providers);
    Require(result.hits.records.empty()&&!inactive.worlds&&!inactive.hashes&&!inactive.classes,
        "No native world/hash query read inactive endpoints or called providers");
    inactive.world.hit.time=nan;inactive.actors=Chain({Packet(1,.5f)});
    result=inactive.Run({10,0,0},{},{},0);
    Require(result.hits.records.empty()&&inactive.worlds==1u&&!inactive.hashes&&Same(result.scale,1),
        "Clear world read unused hit data or nonzero actor flag was invented");
    Fixture clear;clear.actors=Chain({Packet(1,.8f),Packet(2,.2f)});result=clear.Run({10,1,2},{1,2,3},{4,5,6});
    Require(clear.order==std::vector<unsigned>{1,2}&&clear.seenWorldFlags==255u&&clear.seenActorFlags==255u&&
        Same(clear.seenEnd,Vec{10,1,2})&&Same(clear.seenStart,Vec{1,2,3})&&Same(clear.seenExtent,Vec{4,5,6}),
        "Provider order/byte flags/unchanged clear-world query arguments differ");
    Require(result.hits.records.size()==2u&&result.hits.records[0].actor==2u&&!clear.classes,
        "No-world collector invented class filtering or lost sorted hits");Links(result);
    Vec mutableEnd{10,1,2},mutableStart{1,2,3},mutableExtent{4,5,6};
    auto copiedInputs=clear.Providers();
    copiedInputs.world=[&](std::uintptr_t,const Vec& receivedEnd,const Vec& receivedStart,const Vec& receivedExtent,std::uint32_t)->Reply {
        mutableEnd={90,91,92};mutableStart={80,81,82};mutableExtent={70,71,72};
        Require(Same(receivedEnd,Vec{10,1,2})&&Same(receivedStart,Vec{1,2,3})&&Same(receivedExtent,Vec{4,5,6}),
            "Native by-value inputs aliased mutable provider/caller storage");return {};
    };
    result=QuestVr::CollectOriginalMultiLineHits(mutableEnd,mutableStart,mutableExtent,1,99,255,copiedInputs);
    Require(Same(clear.seenEnd,Vec{10,1,2})&&Same(clear.seenStart,Vec{1,2,3})&&Same(clear.seenExtent,Vec{4,5,6}),
        "World provider changed later actor-query by-value inputs");
    Fixture world;world.world={false,Packet(123,.4f,{4,0,0})};world.actors=Chain({Packet(1,.1f),Packet(2,.8f)});
    const auto original=world.actors;result=world.Run();
    Require(result.hasWorldHit&&!result.nearWorldHit&&result.scale>.8999f&&result.scale<.9f&&
        result.actorQueryEnd.x>8.999f&&result.actorQueryEnd.x<9.0f&&Same(world.seenEnd,result.actorQueryEnd),
        "World collision did not shorten the actor query using original scaling");
    Require(result.hits.records.size()==3u&&result.hits.records[0].actor==1u&&result.hits.records[1].actor==99u&&
        Same(result.hits.records[1].time,.4f)&&result.hits.records[1].primitive==1023u&&result.hits.records[1].item==223,
        "World Time/label/payload changed while actor times were renormalized");
    for(std::size_t i=0;i<original.records.size();++i) {
        Require(Same(world.actors.records[i],original.records[i]),"Collector mutated external actor arena rather than detached temporary storage");
    }
    Links(result);
    Fixture close;close.world={false,Packet(0,.009f,{2,0,0})};close.actors=Chain({Packet(1,nan,{nan,nan,nan}),Packet(2,.3f,{100,0,0})});
    close.movers.insert(2);result=close.Run({100,0,0});
    Require(result.nearWorldHit&&result.stats.skipped==1u&&result.hits.records.size()==2u&&close.classes==3u,
        "Near-world filter read skipped packet fields or failed actual mover-only admission");
    for(float time:{.01f,std::nextafter(.01f,1.0f)}) {
        Fixture threshold;threshold.world={false,Packet(0,time,{2,0,0})};result=threshold.Run();
        Require(result.nearWorldHit==(time==.01f),"Near-world Time compared against Float instead of Double.01");
    }
    Fixture distance;distance.world={false,Packet(0,.001f,{30,0,0})};result=distance.Run({100,0,0});
    Require(!result.nearWorldHit,"Near-world distance30 equality was accepted");
    Fixture unclamped;unclamped.world={false,Packet(0,-.25f,{4,0,0})};result=unclamped.Run();
    Require(result.scale<0&&result.actorQueryEnd.x<0&&Same(result.hits.records.front().time,-.25f),
        "Negative world fraction acquired an invented lower scale clamp");
    for(float x:{4.0f,std::ldexp(1.0f,100)}) {
        unclamped.world.hit=Packet(0,1,{x,0,0});result=unclamped.Run();
        Require(Same(result.scale,1)&&Same(result.actorQueryEnd,Vec{10,0,0}),"Scale exactly/above1 did not retain original min1 policy");
    }
    Fixture mover;mover.world={false,Packet(0,.4f,{4,0,0},{-1,0,0})};mover.actors=Chain({Packet(2,.45f,{4,1,0},{-1,-0.0f,0})});
    mover.movers.insert(2);result=mover.Run();
    Require(result.stats.corrected==1u&&result.hits.records.front().actor==2&&Same(result.hits.records.front().location,Vec{2,0,0})&&
        result.hits.records.front().time>.1999f&&result.hits.records.front().time<.2001f,
        "Coplanar mover correction did not replace location and recompute scaled fraction");
    mover.actors.records[0].location={4,2,0};result=mover.Run();Require(result.stats.corrected==0u,"Mover squared-distance4 equality was accepted");
    mover.actors.records[0].location={4,std::nextafter(2.0f,0.0f),0};result=mover.Run();
    Require(result.stats.corrected==1u,"Mover squared-distance below4 was rejected");
    mover.actors.records[0].normal.x=std::nextafter(-1.0f,0.0f);result=mover.Run();
    Require(result.stats.corrected==0u,"Mover normal mismatch passed exact original equality");
    mover.actors.records[0].location={nan,nan,nan};
    for(const Vec mismatch:{Vec{1,nan,nan},Vec{-1,1,nan},Vec{-1,0,1}}) {
        mover.actors.records[0].normal=mismatch;result=mover.Run();
        Require(result.stats.corrected==0u,"Normal XYZ mismatch read inactive later components or mover location");
    }
    mover.actors.records[0].normal={nan,0,0};Refuse([&]{mover.Run();});
    mover.movers.clear();mover.world.hit.normal={nan,nan,nan};result=mover.Run();
    Require(result.stats.corrected==0u,"Non-mover admission read inactive world/candidate normals or locations");
    Fixture zero;zero.world={false,Packet(0,.4f,{0,0,0})};zero.actors=Chain({Packet(2,.5f)});zero.movers.insert(2);
    Refuse([&]{zero.Run({},{});});
    zero.actors.records[0].normal.x=1;result=zero.Run({},{});Require(result.stats.corrected==0u,"Zero-length query refused inactive mover-correction path");
    Fixture cap;std::vector<Hit> many;for(unsigned i=0;i<70;++i) many.push_back(Packet(i+1u,static_cast<float>(70u-i)/100));
    cap.actors=Chain(std::move(many));cap.actors.records[63].next=10'000u;cap.actors.records[64].time=nan;
    result=cap.Run({},{},{},-7,0);Require(result.hits.records.size()==64u&&result.stats.actorVisits==64u,
        "64-record cap inspected malformed/inactive tail or changed nonzero actor flag");Links(result);
    cap.world={false,Packet(0,.4f,{4,0,0})};cap.actors.records[62].next=10'000u;
    result=cap.Run();Require(result.hits.records.size()==64u&&result.stats.actorVisits==63u,"World hit did not consume one collector slot");
    Fixture tied;std::vector<Hit> equals;for(unsigned i=0;i<9u;++i) equals.push_back(Packet(i+1u,1));
    tied.actors=Chain(std::move(equals));result=tied.Run({},{},{},1,0);
    const std::array<std::uintptr_t,9u> nine{5,2,3,4,1,6,7,8,9};
    for(std::size_t i=0;i<nine.size();++i) Require(result.hits.records[i].actor==nine[i],"Collector did not retain bundled CRT equal-Time permutation");
    tied.actors=Chain({Packet(1,.5f),Packet(2,.5f),Packet(3,.5f)});tied.actors.first=3u;
    tied.actors.records[2].next=1u;tied.actors.records[1].next=0u;result=tied.Run({},{},{},1,0);
    Require(result.hits.records.size()==3u&&result.hits.records[0].actor==1u&&result.hits.records[1].actor==2u&&result.hits.records[2].actor==3u,
        "Collector scanned arena storage order instead of original temporary Next chain");
    Fixture cycle;cycle.world={false,Packet(0,.2f,{2,0,0})};cycle.actors=Chain({Packet(1,.8f)});cycle.actors.records[0].next=1u;
    result=cycle.Run();Require(result.hits.records.size()==64u&&result.stats.actorVisits==63u&&result.stats.skipped==0u,
        "Admitted repeated temporary hit was deduplicated or rejected before collector cap");
    Require(result.hits.records[0].time<result.hits.records[1].time,"Repeated temporary record did not retain successive Time scaling");
    cycle.world.hit.time=.001f;QuestVr::OriginalMultiLineLimits budget;budget.querySteps=16u;
    Refuse([&]{cycle.Run({10,0,0},{},{},1,99,budget);});
    Fixture bad;bad.actors=Chain({Packet(1,.2f)});bad.actors.first=2u;Refuse([&]{bad.Run();});
    bad.actors.first=1u;bad.actors.records[0].next=2u;Refuse([&]{bad.Run();});
    bad.actors.records[0].next=0u;bad.actors.records[0].time=nan;Refuse([&]{bad.Run();});
    budget={};budget.actorRecords=QuestVr::OriginalMultiLineLimits{}.actorRecords+1u;
    Refuse([&]{bad.Run({},{},{},0,0,budget);});
    budget={};budget.querySteps=0u;Refuse([&]{bad.Run({},{},{},0,0,budget);});
    budget={};budget.actorRecords=1u;bad.actors=Chain({Packet(1,.1f),Packet(2,.2f)});
    Refuse([&]{bad.Run({},{},{},1,0,budget);});
    Queries unavailable;
    Refuse([&]{QuestVr::CollectOriginalMultiLineHits({},{},{},0,99,0,unavailable);});
    Fixture missing;missing.world={false,Packet(0,.4f,{4,0,0})};missing.actors=Chain({Packet(1,.2f)});
    unavailable=missing.Providers();unavailable.isMover={};
    Refuse([&]{QuestVr::CollectOriginalMultiLineHits({10,0,0},{},{},1,99,0,unavailable);});
    unavailable=missing.Providers();unavailable.actors=[](const Vec&,const Vec&,const Vec&,std::uint8_t)->Arena {
        throw std::runtime_error("Explicit native actor-query refusal");};
    Refuse([&]{QuestVr::CollectOriginalMultiLineHits({10,0,0},{},{},1,99,0,unavailable);});
}

void SharedWorkControls() {
    const auto sameResult=[](const Result& actual,const Result& expected) {
        Require(actual.hits.first==expected.hits.first&&actual.hits.records.size()==expected.hits.records.size()&&
            Same(actual.actorQueryEnd,expected.actorQueryEnd)&&Same(actual.scale,expected.scale)&&
            actual.hasWorldHit==expected.hasWorldHit&&actual.nearWorldHit==expected.nearWorldHit,
            "Work callback changed collector result metadata");
        Require(actual.stats.steps==expected.stats.steps&&actual.stats.actorVisits==expected.stats.actorVisits&&
            actual.stats.moverTests==expected.stats.moverTests&&actual.stats.skipped==expected.stats.skipped&&
            actual.stats.corrected==expected.stats.corrected,"Work callback changed collector statistics");
        for(std::size_t index=0u;index<actual.hits.records.size();++index)
            Require(Same(actual.hits.records[index],expected.hits.records[index]),"Work callback changed a complete hit packet");
    };
    Fixture fixture;
    fixture.world={false,Packet(0,.4f,{4,0,0},{-1,0,0})};
    fixture.actors=Chain({Packet(2,.45f,{4,1,0},{-1,0,0}),Packet(3,.25f),Packet(4,.8f)});
    fixture.movers.insert(2u);
    auto providers=fixture.Providers();
    const auto collect=[&](const QuestVr::OriginalMultiLineLimits& limits=QuestVr::OriginalMultiLineLimits{}) {
        return QuestVr::CollectOriginalMultiLineHits({10,0,0},{},{},1,99u,255u,providers,limits);
    };
    const auto baseline=collect();
    Require(baseline.stats.steps>3u&&baseline.stats.corrected==1u,
        "Shared-work fixture does not exercise a multi-step collector and mover correction");
    std::size_t calls{};
    providers.work=[&] {++calls;};
    const auto charged=collect();
    Require(calls==charged.stats.steps,"Successful collector callback count differs from every admitted local step");
    sameResult(charged,baseline);
    fixture.world.hit.time=.001f;
    const auto beforeNear=calls;
    const auto near=collect();
    Require(calls-beforeNear==near.stats.steps&&near.nearWorldHit&&near.stats.skipped==2u,
        "Near-world skip/repeated mover tests bypassed shared work accounting");
    fixture.world.hit.time=.4f;
    providers.work={};
    sameResult(collect(),baseline);

    struct SharedRefusal final:std::runtime_error {SharedRefusal():std::runtime_error("Independent shared collector allowance exhausted") {}};
    const auto allowance=baseline.stats.steps+baseline.stats.steps/2u;
    std::size_t consumed{},attempts{};
    providers.work=[&] {
        ++attempts;
        if(consumed>=allowance) throw SharedRefusal();
        ++consumed;
    };
    auto published=collect();
    sameResult(published,baseline);
    Require(consumed==baseline.stats.steps&&attempts==consumed,
        "First collector did not consume the shared outer allowance exactly");
    bool returned{},refused{};
    try {published=collect();returned=true;} catch(const SharedRefusal&) {refused=true;}
    Require(refused&&!returned&&consumed==allowance&&attempts==allowance+1u,
        "Repeated collector reset its shared allowance or swallowed callback refusal");
    sameResult(published,baseline);++refusals;
    const auto worlds=fixture.worlds,hashes=fixture.hashes,classes=fixture.classes;
    refused=false;
    try {published=collect();} catch(const SharedRefusal&) {refused=true;}
    Require(refused&&consumed==allowance&&attempts==allowance+2u&&fixture.worlds==worlds&&
        fixture.hashes==hashes&&fixture.classes==classes,
        "Failed shared collector work was reset or ran providers after immediate exhausted allowance");
    sameResult(published,baseline);++refusals;

    // The collector's own tighter bound must reject a step before asking the
    // enclosing caller to charge it. Neither refusal yields a partial result.
    calls=0u;providers.work=[&] {++calls;};
    QuestVr::OriginalMultiLineLimits tight;tight.querySteps=1u;
    refused=false;
    try {published=collect(tight);} catch(const std::runtime_error& error) {
        refused=std::string(error.what())=="Original MultiLineCheck: shared collector work budget exhausted";
    }
    Require(refused&&calls==1u,"Local collector refusal invoked shared work before admitting the rejected step");
    sameResult(published,baseline);++refusals;
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Hardware=QuestVr::ActorLineX87Oracle;
namespace Math=Hardware::Detail;
using Real=Hardware::Real;
Real OracleRoot(double value) {return value==0.0?static_cast<Real>(value):Math::Root(value);}
Real Square(Vec a,Vec b,bool stored=false,bool reverse=false) {
    Real x=Math::Sub(a.x,b.x),y=Math::Sub(a.y,b.y),z=Math::Sub(a.z,b.z);
    if(stored) {x=Math::Store(x);y=Math::Store(y);z=Math::Store(z);}
    return reverse?Math::Add(Math::Add(Math::Mul(z,z),Math::Mul(y,y)),Math::Mul(x,x)):
        Math::Add(Math::Add(Math::Mul(x,x),Math::Mul(y,y)),Math::Mul(z,z));
}
// Independent hardware arithmetic and control flow. Tie ordering is verified
// exhaustively in the independent sort test; this oracle compares tied groups
// as complete packet multisets, not a second invocation of production sort.
Result Oracle(const Fixture& fixture,Vec end,Vec start,bool world) {
    using namespace Math;Result result;result.actorQueryEnd=end;
    if(world&&!fixture.world.clear) {
        auto packet=fixture.world.hit;packet.actor=99;result.hasWorldHit=true;
        const auto distance=OracleRoot(Spill(Square(packet.location,start)));
        const auto scale=Div(Mul(Add(distance,5.0f),packet.time),Add(distance,std::bit_cast<float>(std::uint32_t{0x38d1b717u})));
        result.scale=scale<1?Store(scale):1;
        const auto dx=Store(Sub(end.x,start.x)),dy=Store(Sub(end.y,start.y));
        result.actorQueryEnd={Store(Add(Store(Mul(dx,result.scale)),start.x)),Store(Add(Store(Mul(dy,result.scale)),start.y)),
            Store(Add(Mul(Sub(end.z,start.z),result.scale),start.z))};
        result.nearWorldHit=packet.time<std::bit_cast<double>(std::uint64_t{0x3f847ae147ae147bull})&&distance<30;
        result.hits.records.push_back(packet);
    }
    auto records=fixture.actors.records;auto cursor=fixture.actors.first;
    while(cursor&&result.hits.records.size()<64u) {
        auto& packet=records.at(cursor-1u);++result.stats.actorVisits;
        if(result.nearWorldHit&&!fixture.movers.contains(packet.actor)) {++result.stats.skipped;cursor=packet.next;continue;}
        const auto& boundary=fixture.world.hit;
        if(result.hasWorldHit&&fixture.movers.contains(packet.actor)&&packet.normal.x==boundary.normal.x&&
            packet.normal.y==boundary.normal.y&&packet.normal.z==boundary.normal.z&&Square(packet.location,boundary.location)<4) {
            const auto dx=Sub(result.actorQueryEnd.x,start.x);
            const auto storedX=Store(dx),storedY=Store(Sub(result.actorQueryEnd.y,start.y)),storedZ=Store(Sub(result.actorQueryEnd.z,start.z));
            const auto length=OracleRoot(Spill(Add(Add(Mul(dx,storedX),Mul(storedY,storedY)),Mul(storedZ,storedZ))));
            const auto savedLength=Store(length);if(length==0) throw std::runtime_error("Oracle zero mover length");
            const auto inverse=Div(1.0,length);const auto nx=Mul(storedX,inverse);
            const auto ny=Store(Mul(storedY,inverse)),nz=Store(Mul(storedZ,inverse));
            packet.location={Store(Sub(boundary.location.x,Store(Add(nx,nx)))),Store(Sub(boundary.location.y,Store(Add(ny,ny)))),
                Store(Sub(boundary.location.z,Store(Add(nz,nz))))};
            packet.time=Store(Div(OracleRoot(Spill(Square(packet.location,start,true,true))),savedLength));++result.stats.corrected;
        }
        packet.time=Store(Mul(result.scale,packet.time));result.hits.records.push_back(packet);cursor=packet.next;
    }
    std::sort(result.hits.records.begin(),result.hits.records.end(),[](const Hit& a,const Hit& b){return a.time<b.time;});
    return result;
}
std::uint64_t Next(std::uint64_t& seed) {seed^=seed<<13u;seed^=seed>>7u;seed^=seed<<17u;return seed;}
float Number(std::uint64_t& seed,bool broad=false) {
    const auto raw=static_cast<std::uint32_t>(Next(seed));
    return broad?std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u)):
        static_cast<float>(static_cast<int>(raw%65537u)-32768)/128.0f;
}
void Differential() {
    Hardware::Precision precision;std::uint64_t seed=0x9385bc0d2741a6efull;
    for(unsigned iteration=0;iteration<14'000u;++iteration) {
        const auto number=[&]{return Number(seed,iteration<2'000u);};
        Fixture fixture;const Vec start{number(),number(),number()},end{number(),number(),number()};
        const bool hasWorld=iteration%4u!=0u;
        const Vec normal{number(),number(),number()};
        Vec worldLocation{number(),number(),number()};
        if(iteration%3u==0u) worldLocation={start.x+1,start.y+1,start.z+1};
        fixture.world={false,Packet(123,iteration%5u==0u?.001f:static_cast<float>(Next(seed)%1000u)/1000.0f,worldLocation,normal)};
        std::vector<Hit> records;const auto count=static_cast<unsigned>(Next(seed)%71u);
        for(unsigned i=0;i<count;++i) {
            const bool mover=Next(seed)%2u!=0u;Vec location{number(),number(),number()};
            if(iteration%2u==0u) location={worldLocation.x+1,worldLocation.y,worldLocation.z};
            records.push_back(Packet(i+1u,static_cast<float>(Next(seed)%10000u)/8192.0f,location,normal));
            if(mover) fixture.movers.insert(i+1u);
        }
        fixture.actors=Chain(std::move(records));bool failed{},oracleFailed{};Result actual,expected;
        try {actual=fixture.Run(end,start,{},1,hasWorld?99u:0u);} catch(const std::exception&) {failed=true;}
        try {expected=Oracle(fixture,end,start,hasWorld);} catch(const std::exception&) {oracleFailed=true;}
        Require(failed==oracleFailed,"MultiLine refusal differs from independent hardware arithmetic");
        if(failed) {++refusals;continue;}
        Require(Same(actual.scale,expected.scale)&&Same(actual.actorQueryEnd,expected.actorQueryEnd)&&
            actual.hasWorldHit==expected.hasWorldHit&&actual.nearWorldHit==expected.nearWorldHit,
            "World shortening/near predicates differ from independent x87");
        Require(actual.stats.actorVisits==expected.stats.actorVisits&&actual.stats.skipped==expected.stats.skipped&&
            actual.stats.corrected==expected.stats.corrected&&actual.hits.records.size()==expected.hits.records.size(),
            "Collector admission/cap/mover correction count differs from independent oracle");
        auto remaining=expected.hits.records;
        float previous=-std::numeric_limits<float>::infinity();
        for(const auto& packet:actual.hits.records) {
            Require(packet.time>=previous,"Collector output is not ordered by Float Time");previous=packet.time;
            const auto found=std::find_if(remaining.begin(),remaining.end(),[&](const Hit& v){return Same(packet,v,false);});
            Require(found!=remaining.end(),"Corrected/scaled complete packet differs from independent x87");remaining.erase(found);
        }
        Links(actual);++differential;corrections+=actual.stats.corrected;nearWorld+=actual.nearWorldHit?1u:0u;
    }
    Require(differential>10'000u&&corrections>10'000u&&nearWorld>500u,"Independent MultiLine sweeps missed shortening/mover branches");
}
#else
void Differential() {std::cout<<"SKIP independent x87 oracle on non-x86 compiler; deterministic collector controls still run.\n";}
#endif
} // namespace
int main() {
    try {Controls();SharedWorkControls();Differential();std::cout<<"PASS original MultiLine collector "<<checks<<" checks, "<<refusals<<" safety refusals, "
        <<differential<<" independent queries, "<<corrections<<" mover corrections, "<<nearWorld<<" near-world filters.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Original MultiLine collector failed: "<<error.what()<<'\n';return 1;}
}
