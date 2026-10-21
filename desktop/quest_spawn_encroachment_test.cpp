#include "quest_spawn_encroachment.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Detached protocol tests ONLY. Mock check/destroy callbacks are not the
// runtime's collision/DestroyActor implementation or playable crate evidence.
namespace {
using Outcome=QuestVr::OriginalSpawnEncroachmentOutcome;
using Queries=QuestVr::OriginalSpawnEncroachmentQueries;
using Rotation=std::array<std::int32_t,3u>;
using Location=std::array<std::uint32_t,3u>;
constexpr std::uintptr_t level=0x101u,actor=0x203u;
constexpr std::size_t unlimited=std::numeric_limits<std::size_t>::max();
std::size_t checks{},refusals{},cases{};
void Require(bool condition,const char* message) {
    ++checks;if(!condition)throw std::runtime_error(message);
}
struct Refusal {std::size_t step{};};
struct Packet {
    std::uintptr_t level{},actor{};
    Location location{};
    Rotation rotation{};
    std::int32_t touch{};
    bool operator==(const Packet&)const=default;
};
struct Fixture {
    Rotation rotation{17,-9001,std::numeric_limits<std::int32_t>::min()};
    Location location{0x80000000u,0x7fc12345u,0xff800000u};
    std::int32_t checkReturn{},destroyReturn{};
    std::size_t failAt=unlimited,workMaximum=unlimited,work{},checkCount{},destroyCount{};
    std::vector<std::string> trace;
    std::vector<Packet> packets;
    std::function<void(Fixture&,const std::string&)> after;
    void Reached(const std::string& name) {
        const auto ordinal=trace.size();trace.push_back(name);
        if(ordinal==failAt)throw Refusal{ordinal};
    }
    void After(const std::string& name) {if(after)after(*this,name);}
    Queries Providers() {
        Queries q;
        q.work=[this] {
            if(work==workMaximum)throw Refusal{work};
            ++work;
        };
        q.rotationChannel=[this](std::uintptr_t source,unsigned channel) {
            Require(source==actor&&channel<3u,"Spawn Rotation read changed its retained Actor or channel");
            const auto name="R"+std::to_string(channel);Reached(name);
            const auto value=rotation[channel];After(name);return value;
        };
        q.locationChannel=[this](std::uintptr_t source,unsigned channel) {
            Require(source==actor&&channel<3u,"Spawn Location read changed its retained Actor or channel");
            const auto name="L"+std::to_string(channel);Reached(name);
            const auto value=location[channel];After(name);return std::bit_cast<float>(value);
        };
        q.check=[this](std::uintptr_t retainedLevel,std::uintptr_t source,PortableModelVec3 where,Rotation angles,std::int32_t touch) {
            Require(retainedLevel==level&&source==actor&&touch==0,
                "Spawn encroachment changed retained Level/Actor or literal touch0");
            Reached("C");++checkCount;
            packets.push_back({retainedLevel,source,{std::bit_cast<std::uint32_t>(where.x),
                std::bit_cast<std::uint32_t>(where.y),std::bit_cast<std::uint32_t>(where.z)},angles,touch});
            After("C");return checkReturn;
        };
        q.destroy=[this](std::uintptr_t retainedLevel,std::uintptr_t source,std::int32_t force) {
            Require(retainedLevel==level&&source==actor&&force==0,
                "Spawn veto destruction changed retained Level/Actor or literal bNetForce0");
            Reached("D");++destroyCount;After("D");return destroyReturn;
        };
        return q;
    }
};
const std::vector<std::string> fullTrace{"R0","R1","R2","L0","L1","L2","C","D"};

void StaticBranchesAndRawFields() {
    for(const auto skip:{1,-1,2,-17,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()}) {
        Fixture f;f.workMaximum=0u;f.failAt=0u;
        auto q=f.Providers();q.rotationChannel={};q.locationChannel={};q.check={};q.destroy={};
        Require(QuestVr::CheckOriginalSpawnEncroachment(level,actor,skip,q)==Outcome::Continue&&
            f.trace.empty()&&f.work==0u&&f.packets.empty(),
            "Nonzero native arg+34 evaluated/required an inactive pose/query/destruction provider");++cases;
    }
    for(const auto returned:{0,1,-1,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()})
        for(const auto destroyed:{0,1,-1,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()}) {
            Fixture f;f.checkReturn=returned;f.destroyReturn=destroyed;auto q=f.Providers();
            if(!returned)q.destroy={};
            const auto outcome=QuestVr::CheckOriginalSpawnEncroachment(level,actor,0,q);
            const auto expected=returned?fullTrace:std::vector<std::string>(fullTrace.begin(),fullTrace.end()-1);
            Require(f.trace==expected&&f.work==expected.size()&&f.checkCount==1u&&f.destroyCount==static_cast<unsigned>(returned!=0),
                "Spawn encroachment changed ordered pose reads or invoked an inactive extra provider");
            Require(outcome==(returned?Outcome::ReturnNone:Outcome::Continue),
                "Spawn veto used DestroyActor's return instead of unconditionally returning None");
            Require(f.packets==std::vector<Packet>{{level,actor,f.location,f.rotation,0}},
                "Spawn check did not receive raw current Location/Rotation field copies");++cases;
        }
    // Every Float32 classification is transported without active arithmetic,
    // including signaling/quiet NaN payloads, infinities and signed zero.
    constexpr std::array<std::uint32_t,12u> words{0u,0x80000000u,0x3f800000u,0xbf800000u,
        1u,0x80000001u,0x7f800000u,0xff800000u,0x7fc12345u,0xffc54321u,0x7f812345u,0xff812345u};
    for(const auto x:words)for(const auto y:words)for(const auto z:words) {
        Fixture f;f.location={x,y,z};auto q=f.Providers();
        Require(QuestVr::CheckOriginalSpawnEncroachment(level,actor,0,q)==Outcome::Continue&&
            f.packets.front().location==Location{x,y,z},
            "Spawn encroachment normalized/rejected a raw Float word before native CheckEncroachment");++cases;
    }
}

void LiveReadAndRetainedArguments() {
    Fixture f;f.checkReturn=1;
    const auto originalRotation=f.rotation;
    // Changing unread fields after each provider proves this is not an early
    // whole-pose snapshot. Changing already-read fields must not alter copies.
    f.after=[](Fixture& live,const std::string& name) {
        if(name=="R0") {live.rotation[0u]=777;live.rotation[1u]=101;}
        if(name=="R1") {live.rotation[1u]=888;live.rotation[2u]=202;}
        if(name=="R2") {live.rotation.fill(999);live.location[0u]=0x3f800001u;}
        if(name=="L0") {live.location[0u]=0u;live.location[1u]=0x7f800000u;}
        if(name=="L1") {live.location[1u]=0u;live.location[2u]=0xffc54321u;}
        if(name=="L2") live.location.fill(0u);
        if(name=="C") {live.location.fill(0x7fc00000u);live.rotation.fill(-1);}
    };
    auto q=f.Providers();
    Require(QuestVr::CheckOriginalSpawnEncroachment(level,actor,0,q)==Outcome::ReturnNone&&f.trace==fullTrace,
        "Live field callbacks changed the original encroachment branch");
    Require(f.packets.front()==Packet{level,actor,{0x3f800001u,0x7f800000u,0xffc54321u},
        {originalRotation[0u],101,202},0},
        "Ordered current field sampling reread captured channels or used an earlier whole-pose snapshot");

    // Ordinary caller aliases may change during check. Destroy still receives
    // the retained native arguments, never a callback-mutated external alias.
    std::uintptr_t levelAlias=level,actorAlias=actor;std::int32_t skipAlias=0;
    Fixture aliases;aliases.checkReturn=1;auto a=aliases.Providers();
    aliases.after=[&](Fixture&,const std::string& name) {
        if(name=="C") {levelAlias=0u;actorAlias=0u;skipAlias=-1;}
    };
    Require(QuestVr::CheckOriginalSpawnEncroachment(levelAlias,actorAlias,skipAlias,a)==Outcome::ReturnNone&&
        aliases.destroyCount==1u&&levelAlias==0u&&actorAlias==0u&&skipAlias==-1,
        "Check callback redirected retained Spawn Level/Actor/skip arguments or suppressed required destruction");
}

void EveryProviderFailureAndMissingDependency() {
    for(const bool veto:{false,true}) {
        const auto steps=veto?8u:7u;
        for(std::size_t stop=0u;stop<steps;++stop) {
            Fixture f;f.checkReturn=veto?1:0;f.failAt=stop;auto q=f.Providers();bool refused{};
            try {static_cast<void>(QuestVr::CheckOriginalSpawnEncroachment(level,actor,0,q));}
            catch(const Refusal& failure) {refused=failure.step==stop;}
            Require(refused&&f.trace==std::vector<std::string>(fullTrace.begin(),fullTrace.begin()+stop+1u)&&f.work==stop+1u,
                "Active provider refusal was swallowed, retried, or reached later original operations");
            ++refusals;
        }
    }
    for(unsigned missing=0u;missing<4u;++missing) {
        Fixture f;f.checkReturn=1;auto q=f.Providers();std::size_t prefix{};
        switch(missing) {
        case 0u:q.rotationChannel={};prefix=0u;break;
        case 1u:q.locationChannel={};prefix=3u;break;
        case 2u:q.check={};prefix=6u;break;
        default:q.destroy={};prefix=7u;break;
        }
        bool refused{};try {static_cast<void>(QuestVr::CheckOriginalSpawnEncroachment(level,actor,0,q));}
        catch(const std::invalid_argument&) {refused=true;}
        Require(refused&&f.trace==std::vector<std::string>(fullTrace.begin(),fullTrace.begin()+prefix)&&f.work==prefix,
            "Missing active provider was preflighted eagerly, charged, or silently skipped");++refusals;
    }
    for(const auto identity:std::array<std::array<std::uintptr_t,2u>,3u>{{{0u,actor},{level,0u},{0u,0u}}})
        for(const auto skip:{0,-1}) {
            Fixture f;auto q=f.Providers();bool refused{};
            try {static_cast<void>(QuestVr::CheckOriginalSpawnEncroachment(identity[0u],identity[1u],skip,q));}
            catch(const std::invalid_argument&) {refused=true;}
            Require(refused&&f.trace.empty()&&f.work==0u,
                "Portable null retained native identity was admitted or dispatched a provider");++refusals;
        }
}

void SharedWorkAndCallerRollback() {
    for(const bool veto:{false,true}) {
        const auto steps=veto?8u:7u;
        Fixture exact;exact.checkReturn=veto?1:0;exact.workMaximum=steps;
        auto q=exact.Providers();
        Require(QuestVr::CheckOriginalSpawnEncroachment(level,actor,0,q)==
            (veto?Outcome::ReturnNone:Outcome::Continue)&&exact.work==steps,
            "Exact shared native-work allowance refused its final active original operation");
        // A second invocation borrows, rather than resets, that same ledger.
        bool refused{};try {static_cast<void>(QuestVr::CheckOriginalSpawnEncroachment(level,actor,0,q));}
        catch(const Refusal& failure) {refused=failure.step==steps;}
        Require(refused&&exact.work==steps&&exact.trace.size()==steps,
            "Repeated Spawn fragment reset caller work or invoked a provider after work refusal");++refusals;
        for(std::size_t allowance=0u;allowance<steps;++allowance) {
            Fixture f;f.checkReturn=veto?1:0;f.workMaximum=allowance;auto limited=f.Providers();refused=false;
            try {static_cast<void>(QuestVr::CheckOriginalSpawnEncroachment(level,actor,0,limited));}
            catch(const Refusal& failure) {refused=failure.step==allowance;}
            Require(refused&&f.work==allowance&&f.trace==
                std::vector<std::string>(fullTrace.begin(),fullTrace.begin()+allowance),
                "Shared work failure did not precede the precise denied provider");++refusals;
        }
    }
    // Helper does not own a transaction. A throwing real provider leaves any
    // earlier callback effects for the enclosing host to undo, not a helper's
    // partial local snapshot. This explicit mock root proves that distinction.
    for(std::size_t stop=0u;stop<8u;++stop) {
        Fixture f;f.checkReturn=1;f.failAt=stop;int callerState=41;
        f.after=[&](Fixture&,const std::string&) {++callerState;};
        const auto saved=callerState;auto q=f.Providers();bool refused{};
        try {static_cast<void>(QuestVr::CheckOriginalSpawnEncroachment(level,actor,0,q));}
        catch(const Refusal&) {refused=true;}
        Require(refused&&callerState==saved+static_cast<int>(stop),
            "Spawn helper silently undid earlier provider state or bypassed a provider on its failure prefix");
        callerState=saved;
        Require(callerState==41,"Detached caller rollback control did not restore its independently owned state");
    }
    Fixture ordinary;ordinary.checkReturn=1;ordinary.destroyReturn=0;int effects{};
    ordinary.after=[&](Fixture&,const std::string& name) {if(name=="C"||name=="D")++effects;};
    auto q=ordinary.Providers();
    Require(QuestVr::CheckOriginalSpawnEncroachment(level,actor,0,q)==Outcome::ReturnNone&&effects==2,
        "Ordinary veto/destroyfalse result rolled back callbacks or substituted a thrown refusal");
}
}

int main() {
    try {
        StaticBranchesAndRawFields();LiveReadAndRetainedArguments();EveryProviderFailureAndMissingDependency();
        SharedWorkAndCallerRollback();
        std::cout<<"Original Spawn post-PostBeginPlay encroachment: "<<checks<<" checks, "<<cases
            <<" branch/raw-pose cases and "<<refusals<<" refusal controls passed.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 1;}
    catch(...) {std::cerr<<"Uncaught Spawn encroachment provider refusal.\n";return 1;}
}
