#include "quest_actor_stasis.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Queries=QuestVr::OriginalActorStasisQueries;
using Stats=QuestVr::OriginalActorStasisStats;
constexpr std::uintptr_t Actor=17u;
constexpr std::uint32_t Stasis=0x00200000u,Force=0x00400000u;
std::size_t checks{},refusals{};

void Require(bool condition,const char* message) {
    ++checks;if(!condition)throw std::runtime_error(message);
}
template<class Action> void Refuses(Action action,const char* needle) {
    bool failed{};
    try {action();}catch(const std::exception& error) {
        failed=std::string(error.what()).find(needle)!=std::string::npos;
    }
    ++refusals;Require(failed,"Stasis did not produce the expected explicit refusal");
}

struct Fixture {
    float last{},zoneLast{},distance{};
    std::array<double,2u> times{10.0,10.0};
    std::uint32_t flags{Stasis};
    std::uint8_t physics{},zone{7u},mode{};
    Queries query;
    std::vector<std::string> calls;
    std::size_t timeReads{},work{},cutoff{std::numeric_limits<std::size_t>::max()};
    std::function<void()> afterFlags;
    Fixture() {
        query.work=[this] {
            if(work==cutoff)throw std::runtime_error("injected stasis shared-work cutoff");
            ++work;
        };
        query.lastRenderTime=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("last");return last;
        };
        query.nativeTime=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("time"+std::to_string(timeReads));
            return times.at(timeReads++);
        };
        query.stasisFlags=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("flags");const auto captured=flags;
            if(afterFlags)afterFlags();
            return captured;
        };
        query.physics=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("physics");return physics;
        };
        query.zoneNumber=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("zone");return zone;
        };
        query.zoneLastRenderTime=[this](std::uintptr_t actor,std::uint8_t selected) {
            Identity(actor);Require(selected==zone,"Stasis changed the retained stored zone number");
            calls.emplace_back("zoneLast");return zoneLast;
        };
        query.distanceFromPlayer=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("distance");return distance;
        };
        query.netMode=[this](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("netMode");return mode;
        };
    }
    void Identity(std::uintptr_t actor) {Require(actor==Actor,"Stasis changed its retained Actor identity");}
    Stats Run() {return QuestVr::QueryOriginalActorStasis(Actor,query);}
    void Trace(std::initializer_list<const char*> expected) const {
        std::vector<std::string> names;for(const auto* name:expected)names.emplace_back(name);
        Require(calls==names,"Stasis provider order or lazy gate changed");
    }
    void Consistent(const Stats& stats) const {
        const auto present=[&](const char* name) {return std::find(calls.begin(),calls.end(),name)!=calls.end();};
        Require(stats.steps==calls.size()&&stats.nativeTimeReads==timeReads,
            "Stasis stats changed provider/shared native-time accounting");
        Require(stats.physicsRead==present("physics")&&stats.zoneRead==present("zoneLast")&&
            stats.distanceRead==present("distance")&&stats.netModeRead==present("netMode"),
            "Stasis stats disagree with the reached native fields");
    }
};

void OrderedGates() {
    {
        Fixture f;const auto s=f.Run();Require(s.inStasis,"Old actor in an old zone did not enter stasis");
        f.Trace({"last","time0","flags","physics","zone","zoneLast","time1","netMode"});
        f.Consistent(s);Require(f.work==8u,"Old-zone stasis has the wrong shared-work cost");
    }
    {
        Fixture f;f.times[0]=5.0;f.query.stasisFlags={};f.query.physics={};f.query.zoneNumber={};
        f.query.zoneLastRenderTime={};f.query.distanceFromPlayer={};f.query.netMode={};
        const auto s=f.Run();Require(!s.inStasis,"Exact five-second actor age was not strict");
        f.Trace({"last","time0"});f.Consistent(s);
    }
    {
        Fixture f;f.flags=0u;f.query.physics={};f.query.zoneNumber={};f.query.zoneLastRenderTime={};
        f.query.distanceFromPlayer={};f.query.netMode={};const auto s=f.Run();
        Require(!s.inStasis,"Actor without bStasis entered stasis");
        f.Trace({"last","time0","flags"});f.Consistent(s);
    }
    {
        Fixture f;f.physics=2u;f.query.zoneNumber={};f.query.zoneLastRenderTime={};
        f.query.distanceFromPlayer={};f.query.netMode={};const auto s=f.Run();
        Require(!s.inStasis,"Ordinary falling actor incorrectly reached zone stasis");
        f.Trace({"last","time0","flags","physics"});f.Consistent(s);
    }
    {
        Fixture f;f.flags=Stasis|Force;f.query.physics={};f.query.distanceFromPlayer={};
        const auto s=f.Run();Require(s.inStasis,"ForceStasis did not bypass only the Physics provider");
        f.Trace({"last","time0","flags","zone","zoneLast","time1","netMode"});f.Consistent(s);
    }
    {
        Fixture f;f.zoneLast=5.0f;f.distance=1200.0f;f.query.netMode={};const auto s=f.Run();
        Require(!s.inStasis,"Exact five-second zone age or distance1200 was not strict");
        f.Trace({"last","time0","flags","physics","zone","zoneLast","time1","distance"});f.Consistent(s);
    }
    {
        Fixture f;f.zoneLast=10.0f;f.distance=1201.0f;const auto s=f.Run();
        Require(s.inStasis,"Far actor in a recently rendered zone did not reach NetMode");
        f.Trace({"last","time0","flags","physics","zone","zoneLast","time1","distance","netMode"});f.Consistent(s);
    }
    {
        Fixture f;f.query.work={};const auto s=f.Run();f.Consistent(s);
        Require(s.inStasis&&f.work==0u&&s.steps==8u,"Optional work provider changed the native branch result");
    }
}

void FlagsAndPhysics() {
    // All Physics bytes, both native flag bits, and unrelated packed bits.
    // Force alone must not replace bStasis or bypass the earlier age check.
    constexpr std::array<std::uint32_t,4u> bits{0u,Stasis,Force,Stasis|Force};
    for(const auto unrelated:{0u,0xff9fffffu})for(const auto selected:bits)
        for(unsigned physics=0u;physics<256u;++physics) {
            Fixture f;f.flags=selected|unrelated;f.physics=static_cast<std::uint8_t>(physics);
            const auto s=f.Run();const bool hasStasis=(selected&Stasis)!=0u,forced=(selected&Force)!=0u;
            const bool eligible=hasStasis&&(forced||physics==0u||physics==5u);
            Require(s.inStasis==eligible,"Packed stasis flags or an arbitrary Physics byte changed native eligibility");
            Require(s.physicsRead==(hasStasis&&!forced),"ForceStasis bypassed more or less than the Physics read");
            Require(s.zoneRead==eligible&&s.netModeRead==eligible&&!s.distanceRead,
                "Flag/Physics short circuit eagerly demanded later providers");f.Consistent(s);
        }
    {
        Fixture f;f.flags=Stasis|Force;f.afterFlags=[&]{f.flags=0u;};f.query.physics={};
        const auto s=f.Run();Require(s.inStasis&&!s.physicsRead,
            "Original packed read did not retain both stasis bits after the provider returned");
        Require(std::count(f.calls.begin(),f.calls.end(),"flags")==1,
            "Stasis reread packed flags instead of retaining the single native load");
    }
    {
        Fixture f;f.flags=Stasis;f.physics=2u;f.afterFlags=[&]{f.flags=Stasis|Force;};
        const auto s=f.Run();Require(!s.inStasis&&s.physicsRead&&!s.zoneRead,
            "A later packed-bit change incorrectly bypassed the captured Physics gate");
    }
    for(const auto selected:bits) {
        Fixture f;f.flags=selected;f.times[0]=5.0;
        const auto s=f.Run();Require(!s.inStasis&&f.calls.size()==2u,
            "ForceStasis or bStasis bypassed the actor-age gate");
    }
}

struct AgeCase {double now;float last;bool older;};
void ArithmeticAndRereads() {
    const double infinity=std::numeric_limits<double>::infinity();
    const float floatInfinity=std::numeric_limits<float>::infinity();
    const double nan=std::numeric_limits<double>::quiet_NaN();
    const float floatNan=std::numeric_limits<float>::quiet_NaN();
    const std::array cases{
        AgeCase{0.0,0.0f,false},AgeCase{5.0,0.0f,false},AgeCase{-5.0,0.0f,false},
        AgeCase{std::nextafter(5.0,infinity),0.0f,true},
        AgeCase{std::nextafter(5.0,-infinity),0.0f,false},
        // Around five, an x87 PC64 ULP is2^-61, not binary64's2^-50.
        AgeCase{5.0,-std::ldexp(1.0f,-61),true},
        AgeCase{5.0,-std::ldexp(1.0f,-62),false}, // exact half, ties to even5
        AgeCase{5.0,-std::ldexp(3.0f,-63),true},
        AgeCase{5.0,-std::ldexp(1.0f,-63),false},
        AgeCase{5.0,std::ldexp(1.0f,-61),false},
        AgeCase{5.0,-std::numeric_limits<float>::denorm_min(),false},
        AgeCase{5.0,0.0f,false},AgeCase{5.0,-0.0f,false},
        AgeCase{0.0,-6.0f,true},AgeCase{0.0,-5.0f,false},
        AgeCase{16777221.0,16777216.0f,false},AgeCase{16777222.0,16777216.0f,true},
        AgeCase{double(std::numeric_limits<float>::max()),std::numeric_limits<float>::max(),false},
        AgeCase{std::numeric_limits<double>::max(),std::numeric_limits<float>::max(),true},
        AgeCase{-std::numeric_limits<double>::max(),-std::numeric_limits<float>::max(),false},
        AgeCase{0.0,-std::numeric_limits<float>::max(),true},
        AgeCase{infinity,0.0f,true},AgeCase{-infinity,0.0f,false},
        AgeCase{0.0,floatInfinity,false},AgeCase{0.0,-floatInfinity,true},
        AgeCase{infinity,floatInfinity,false},AgeCase{-infinity,-floatInfinity,false},
        AgeCase{infinity,-floatInfinity,true},AgeCase{-infinity,floatInfinity,false},
        AgeCase{nan,0.0f,false},AgeCase{nan,floatInfinity,false},
        AgeCase{0.0,floatNan,false},AgeCase{infinity,floatNan,false},AgeCase{nan,floatNan,false}
    };
    for(const auto& test:cases) {
        Fixture initial;initial.last=test.last;initial.times[0]=test.now;
        const auto first=initial.Run();initial.Consistent(first);
        Require(first.inStasis==test.older,"Initial native Double-minus-Float PC64/ordered comparison changed");
        Require(first.zoneRead==test.older&&first.netModeRead==test.older,
            "Initial unordered/equality gate read fields outside its native prefix");
        Fixture zone;zone.zoneLast=test.last;zone.times[1]=test.now;zone.distance=0.0f;
        const auto second=zone.Run();zone.Consistent(second);
        Require(second.inStasis==test.older&&second.distanceRead==!test.older&&second.netModeRead==test.older,
            "Zone native Double-minus-Float comparison did not use the same ordered PC64 semantics");
        Require(second.nativeTimeReads==2u,"Reached zone stasis reused rather than reread native Level time");
        if(!test.older) {
            Fixture far;far.zoneLast=test.last;far.times[1]=test.now;far.distance=1201.0f;
            const auto third=far.Run();Require(third.inStasis&&third.distanceRead&&third.netModeRead,
                "Unordered/recent zone age incorrectly suppressed the original far-distance alternative");
        }
    }
    Require(5.0-double(-std::ldexp(1.0f,-61))==5.0,
        "PC64 witness no longer differs from an eager binary64 subtraction spill");
    Require(static_cast<float>(std::nextafter(5.0,infinity))==5.0f,
        "Native Double witness no longer differs from an eager Float time conversion");
    {
        Fixture f;f.times={10.0,3.0};const auto s=f.Run();
        Require(!s.inStasis&&s.distanceRead&&s.nativeTimeReads==2u,
            "Zone-age query reused the captured actor-age native time");
    }
    {
        Fixture f;f.times={10.0,100.0};f.zoneLast=94.0f;const auto s=f.Run();
        Require(s.inStasis&&!s.distanceRead&&s.nativeTimeReads==2u,
            "Updated native zone time was replaced by the earlier actor-age load");
    }
}

void ZoneDistanceAndMode() {
    for(unsigned zone=0u;zone<256u;++zone) {
        Fixture f;f.zone=static_cast<std::uint8_t>(zone);
        if(zone<64u) {
            const auto s=f.Run();Require(s.inStasis&&s.zoneRead,"Valid native zone0..63 was rejected");f.Consistent(s);
        }else {
            Refuses([&]{f.Run();},"64 entries");
            f.Trace({"last","time0","flags","physics","zone"});
        }
    }
    const float infinity=std::numeric_limits<float>::infinity();
    const std::array distances{
        -infinity,-std::numeric_limits<float>::max(),-1.0f,-0.0f,0.0f,
        std::nextafter(1200.0f,-infinity),1200.0f,std::nextafter(1200.0f,infinity),
        1201.0f,std::numeric_limits<float>::max(),infinity,std::numeric_limits<float>::quiet_NaN()
    };
    for(const auto distance:distances) {
        Fixture f;f.zoneLast=10.0f;f.distance=distance;
        const auto s=f.Run();f.Consistent(s);const bool expected=distance>1200.0;
        Require(s.inStasis==expected&&s.distanceRead&&s.netModeRead==expected,
            "Distance1200 strict/ordered Float-to-Double comparison changed");
    }
    // Native comparison is byte==0, not validation against named enum values.
    for(unsigned mode=0u;mode<256u;++mode)for(const bool viaDistance:{false,true}) {
        Fixture f;f.mode=static_cast<std::uint8_t>(mode);
        if(viaDistance){f.zoneLast=10.0f;f.distance=1201.0f;}
        const auto s=f.Run();f.Consistent(s);
        Require(s.inStasis==(mode==0u)&&s.netModeRead&&s.distanceRead==viaDistance,
            "Final named Actor.Level NetMode gate changed the native zero-byte condition");
    }
}

void RefusalsAndWork() {
    {
        Fixture f;Refuses([&]{QuestVr::QueryOriginalActorStasis(0u,f.query);},"nonnull Actor");
        Require(f.calls.empty()&&f.work==0u,"Null Actor charged work or invoked a provider");
    }
    for(unsigned missing=0u;missing<8u;++missing) {
        Fixture f;f.zoneLast=10.0f;f.distance=1201.0f;
        switch(missing) {
        case 0u:f.query.lastRenderTime={};break;
        case 1u:f.query.nativeTime={};break;
        case 2u:f.query.stasisFlags={};break;
        case 3u:f.query.physics={};break;
        case 4u:f.query.zoneNumber={};break;
        case 5u:f.query.zoneLastRenderTime={};break;
        case 6u:f.query.distanceFromPlayer={};break;
        case 7u:f.query.netMode={};break;
        default:throw std::runtime_error("Unreachable missing-provider selector");
        }
        Refuses([&]{f.Run();},"reached provider");
        Require(f.work==f.calls.size(),"Missing active provider incorrectly charged shared work before refusing");
    }
    {
        Fixture f;const auto original=f.query.zoneLastRenderTime;
        f.query.zoneLastRenderTime=[&](std::uintptr_t actor,std::uint8_t zone) {
            const auto value=original(actor,zone);f.query.nativeTime={};return value;
        };
        Refuses([&]{f.Run();},"reached provider");
        f.Trace({"last","time0","flags","physics","zone","zoneLast"});
        Require(f.timeReads==1u,"Missing second native-time provider was replaced with the first captured load");
    }
    Fixture whole;whole.zoneLast=10.0f;whole.distance=1201.0f;
    const auto full=whole.Run();Require(full.inStasis&&full.steps==9u&&whole.work==9u,
        "Shared-work control did not reach every stasis provider");
    for(std::size_t prefix=0u;prefix<full.steps;++prefix) {
        Fixture f;f.zoneLast=10.0f;f.distance=1201.0f;f.cutoff=prefix;
        Refuses([&]{f.Run();},"shared-work cutoff");
        Require(f.work==prefix&&f.calls.size()==prefix&&
            std::equal(f.calls.begin(),f.calls.end(),whole.calls.begin()),
            "Shared-work cutoff did not preserve the exact reached provider prefix");
    }
    {
        Fixture f;f.zoneLast=10.0f;f.distance=1201.0f;f.cutoff=full.steps;
        const auto exact=f.Run();Require(exact.inStasis&&f.calls==whole.calls&&f.work==full.steps,
            "Exact successful shared-work budget refused or invoked an extra provider");
    }
}
}

int main() {
    try {
        OrderedGates();FlagsAndPhysics();ArithmeticAndRereads();ZoneDistanceAndMode();RefusalsAndWork();
        std::cout<<"PASS original Actor stasis: "<<checks<<" checks, "<<refusals
            <<" refusals; ordered lazy providers, PC64 time ages, captured packed flags, native zones/distance/NetMode and shared work; "
              "NOT renderer publication, world Tick or live Training gravity.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL original Actor stasis: "<<error.what()<<'\n';return 1;}
}
