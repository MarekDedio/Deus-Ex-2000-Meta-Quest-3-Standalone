#include "quest_actor_rotation.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Rotator=std::array<std::int32_t,3u>;
using Queries=QuestVr::OriginalActorRotationQueries;
using Stats=QuestVr::OriginalActorRotationStats;
std::size_t checks{},refusals{};
void Require(bool value,const char* text) {if(!value)throw std::logic_error(text);++checks;}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
std::int32_t Signed(std::uint32_t value) {return std::bit_cast<std::int32_t>(value);}
std::uint32_t Word(std::int32_t value) {return std::bit_cast<std::uint32_t>(value);}

// Instruction/register oracle, following independently read original control
// flow103C9DC0..103C9EDE. Register arithmetic is unsigned32 throughout; signed
// jump predicates reinterpret register bits instead of invoking C++ overflow.
std::int32_t FixedTurnOracle(std::int32_t current,std::int32_t desired,std::int32_t delta,std::uint8_t flags) {
    std::uint32_t eax=Word(current),edx=Word(desired),esi=Word(delta),edi{},ecx{};
    if(esi==0u)goto finish;
    eax&=0xffffu;edx&=0xffffu;edi=eax;
    if((flags&0x20u)==0u)goto branch9e48;
    if((flags&0x40u)==0u){eax+=esi;goto finish;}
    if(Signed(esi)<=0)goto branch9e1e;
    if(Signed(eax)>Signed(edx))edx+=0x10000u;
    edx-=eax;
    if(Signed(esi)<=Signed(edx))edx=esi;
    eax+=edx;goto finish;
branch9e1e:
    if(Signed(eax)<Signed(edx))edi=eax+0x10000u;
    edx-=edi;
    if(Signed(esi)>=Signed(edx))edx=esi;
    eax+=edx;goto finish;
branch9e48:
    if((flags&0x40u)==0u)goto finish;
    if(Signed(eax)<=Signed(edx))goto branch9e99;
    ecx=eax-edx;
    if(Signed(ecx)>=0x8000)goto branch9e79;
    if(Signed(esi)<0)esi=0u-esi;
    if(Signed(ecx)>Signed(esi))ecx=esi;
    eax-=ecx;goto finish;
branch9e79:
    if(Signed(esi)<0)esi=0u-esi;
    edx-=eax;edx+=0x10000u;
    if(Signed(edx)>Signed(esi))edx=esi;
    eax+=edx;goto finish;
branch9e99:
    ecx=edx-eax;
    if(Signed(ecx)>=0x8000)goto branch9ebd;
    if(Signed(esi)<0)esi=0u-esi;
    if(Signed(ecx)>Signed(esi))ecx=esi;
    eax+=ecx;goto finish;
branch9ebd:
    if(Signed(esi)<0)esi=0u-esi;
    ecx=eax-edx;ecx+=0x10000u;
    if(Signed(ecx)>Signed(esi))ecx=esi;
    eax-=ecx;
finish:
    return static_cast<std::int32_t>(eax&0xffffu);
}

// Independent bit-column multiplication/truncation oracle. This does not use
// the production conversion's integer multiply, exponent shifting or host FP.
// Columns retain every bit of signed-INT32 * exact Float32, then only integral
// columns contribute to original signed64 _ftol and its low32 EAX result.
struct Converted {bool refused{};std::int32_t value{};};
Converted ConversionOracle(std::int32_t rate,std::uint32_t floatWord) {
    const auto exponent=(floatWord>>23u)&255u;
    if(exponent==255u)return {true,0};
    const bool negative=(rate<0)!=((floatWord>>31u)!=0u);
    const std::uint32_t magnitude=rate<0?0u-Word(rate):Word(rate);
    const std::uint32_t mantissa=(floatWord&0x7fffffu)|(exponent?0x800000u:0u);
    const int scale=exponent?static_cast<int>(exponent)-150:-149;
    std::array<unsigned,200u> columns{};
    for(unsigned a=0u;a<32u;++a)if((magnitude&(std::uint32_t{1u}<<a))!=0u)
        for(unsigned b=0u;b<24u;++b)if((mantissa&(std::uint32_t{1u}<<b))!=0u)++columns[a+b];
    for(std::size_t i=0u;i+1u<columns.size();++i){columns[i+1u]+=columns[i]>>1u;columns[i]&=1u;}
    std::uint64_t integer{};bool high{};
    for(std::size_t i=0u;i<columns.size();++i)if(columns[i]) {
        const int position=static_cast<int>(i)+scale;
        if(position>=64)high=true;
        else if(position>=0)integer|=std::uint64_t{1u}<<static_cast<unsigned>(position);
    }
    if(high||(!negative&&integer>=0x8000000000000000ull)||
        (negative&&integer>0x8000000000000000ull))return {true,0};
    const auto low=static_cast<std::uint32_t>(integer);
    return {false,Signed(negative?0u-low:low)};
}

template<class Function> void NativeRefuses(Function function,const char* text) {
    bool refused{};
    try{function();}catch(const std::invalid_argument&){refused=true;++refusals;}
    catch(const std::runtime_error&){refused=true;++refusals;}
    Require(refused,text);
}
enum class Stop {Work,Write,Read,Move,Probe,Event};
struct ExpectedStop final : std::exception {Stop kind;explicit ExpectedStop(Stop value):kind(value){};};
template<class Function> void Stops(Function function,Stop expected,const char* text) {
    bool stopped{};try{function();}catch(const ExpectedStop& stop){Require(stop.kind==expected,"Failure changed expected sentinel reason");stopped=true;++refusals;}
    Require(stopped,text);
}

void FixedTurnControls() {
    constexpr auto min=std::numeric_limits<std::int32_t>::min();
    constexpr auto max=std::numeric_limits<std::int32_t>::max();
    const std::array<std::int32_t,13u> delta{min,min+1,-65537,-32769,-32768,-1,0,1,32767,32768,65535,65536,max};
    const std::array<std::uint8_t,8u> flags{0u,0x10u,0x20u,0x40u,0x60u,0x9fu,0xbfu,0xffu};
    const std::array<std::uint32_t,6u> offset{0u,1u,32767u,32768u,32769u,65535u};
    // Every low16 current angle, all half-turn/equality edges, both wrap
    // directions and extreme signed deltas. Raw upper16 bits vary separately.
    for(std::uint32_t current=0u;current<65536u;++current)for(auto distance:offset)
        for(auto change:delta)for(auto packed:flags) {
            const auto c=Signed(current|((current&1u)?0xdead0000u:0u));
            const auto d=Signed(((current+distance)&0xffffu)|((current&2u)?0xbeef0000u:0u));
            Require(QuestVr::OriginalActorFixedTurn(c,d,change,packed)==FixedTurnOracle(c,d,change,packed),
                "fixedTurn differs from independent original register/jump oracle");
        }
    Require(QuestVr::OriginalActorFixedTurn(32768,0,1,0x40u)==32769&&
        QuestVr::OriginalActorFixedTurn(0,32768,1,0x40u)==65535,"Original asymmetric half-turn tie changed");
    Require(QuestVr::OriginalActorFixedTurn(100,500,min,0x40u)==100&&
        QuestVr::OriginalActorFixedTurn(500,100,min,0x40u)==500,"INT_MIN abs was widened or saturated");
    Require(QuestVr::OriginalActorFixedTurn(65535,0,5,0x60u)==0&&
        QuestVr::OriginalActorFixedTurn(0,65535,-5,0x60u)==65535,"Fixed-direction target overshoot clamp changed");
}

void ConversionControls() {
    constexpr auto min=std::numeric_limits<std::int32_t>::min();
    constexpr auto max=std::numeric_limits<std::int32_t>::max();
    const std::array<std::int32_t,14u> rates{min,min+1,-16777217,-3,-1,0,1,3,16777215,16777216,16777217,max-1,max,65537};
    const std::array<std::uint32_t,29u> values{0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,
        0x3effffffu,0x3f000000u,0x3f000001u,0x3f7fffffu,0x3f800000u,0x3f800001u,0xbf000000u,0xbf800001u,
        0x4f000000u,0x4f800000u,0xcf800000u,0x5effffffu,0x5f000000u,0xdf000000u,0x5f000001u,0xdf000001u,
        0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc12345u,0xffc23456u,0x7f812345u};
    for(auto rate:rates)for(auto word:values) {
        const auto oracle=ConversionOracle(rate,word);
        if(oracle.refused)NativeRefuses([&]{static_cast<void>(QuestVr::OriginalActorRotationDelta(rate,std::bit_cast<float>(word)));},
            "Reached exceptional/out-of-signed64 conversion did not refuse");
        else Require(QuestVr::OriginalActorRotationDelta(rate,std::bit_cast<float>(word))==oracle.value,
            "Rotation delta differs from independent exact bit-column product oracle");
    }
    // Dense bounded integer/dyadic conversions also exercise truncation signs.
    for(std::int32_t rate=-1024;rate<=1024;++rate)for(std::int32_t quarter=-20;quarter<=20;++quarter) {
        const auto expected=static_cast<std::int32_t>((static_cast<std::int64_t>(rate)*quarter)/4);
        Require(QuestVr::OriginalActorRotationDelta(rate,static_cast<float>(quarter)*0.25f)==expected,
            "Finite negative/signed dyadic product did not truncate toward zero");
    }
    Require(QuestVr::OriginalActorRotationDelta(16777217,1.0f)==16777217&&
        QuestVr::OriginalActorRotationDelta(max,0.5f)==1073741823&&
        QuestVr::OriginalActorRotationDelta(-max,0.5f)==-1073741823,
        "Rate was converted/spilled to Float32 before _ftol");
    Require(QuestVr::OriginalActorRotationDelta(max,std::bit_cast<float>(0x3f800001u))==-2147483394,
        "Unspilled INT32*Float32 discriminator changed");
    Require(QuestVr::OriginalActorRotationDelta(1,std::bit_cast<float>(0x4f800000u))==0&&
        QuestVr::OriginalActorRotationDelta(3,std::bit_cast<float>(0x4f000000u))==min,
        "Original signed64 conversion low32 result was incorrectly limited to INT32");
    Require(QuestVr::OriginalActorRotationDelta(min,std::bit_cast<float>(0x4f800000u))==0&&
        QuestVr::OriginalActorRotationDelta(-1,std::bit_cast<float>(0x5f000000u))==0,
        "Exact negative signed64 boundary did not remain admissible");
    NativeRefuses([&]{static_cast<void>(QuestVr::OriginalActorRotationDelta(min,std::bit_cast<float>(0xcf800000u)));},
        "Positive signed64 boundary incorrectly admitted");
}

struct State {
    Rotator rotation{100,200,300},desired{110,220,330},rate{10,20,30};
    std::uint8_t flags{0x20u};Id level{2u},support{7u};bool deleted{},probe{true};
    std::uint32_t random{123u},sounds{},clock{456u},events{};
    bool operator==(const State&)const=default;
};
struct MovePacket {Rotator proposed{};std::int32_t result{};};
struct Fixture {
    State state;
    std::vector<std::string> order;
    std::vector<MovePacket> moves;
    std::array<unsigned,3u> rotationReads{},desiredReads{},rateReads{};
    unsigned flagReads{},probeReads{};
    std::function<void(const std::string&,unsigned)> afterRead;
    std::function<void(Rotator)> onMove;
    std::function<void()> onProbe,onEnded;
    std::size_t work{},workLimit{4096u},writes{},writeLimit{4096u};
    std::int32_t moveResult{1};bool applyMove{true};
    static void Actor(Id actor){Require(actor==1u,"physicsRotation changed original Actor receiver");}
    void Write(){if(writes>=writeLimit)throw ExpectedStop(Stop::Write);++writes;}
    void Rotate(unsigned axis,std::int32_t value){Write();state.rotation.at(axis)=value;}
    void Desired(unsigned axis,std::int32_t value){Write();state.desired.at(axis)=value;}
    void Flags(std::uint8_t value){Write();state.flags=value;}
    void Effects(){Write();state.support=9u;Write();++state.random;Write();++state.sounds;Write();++state.clock;}
    void Read(const std::string& field,unsigned axis){order.push_back(field+(field=="flags"||field=="probe"?"":"-"+std::to_string(axis)));if(afterRead)afterRead(field,axis);}
    Queries GetQueries() {
        Queries q;
        q.work=[&]{if(work>=workLimit)throw ExpectedStop(Stop::Work);++work;};
        q.rotationFlags=[&](Id actor){Actor(actor);const auto value=state.flags;++flagReads;Read("flags",0u);return value;};
        q.rotation=[&](Id actor,unsigned axis){Actor(actor);Require(axis<3u,"Invalid current Rotator channel");
            const auto value=state.rotation[axis];++rotationReads[axis];Read("rotation",axis);return value;};
        q.desiredRotation=[&](Id actor,unsigned axis){Actor(actor);Require(axis<3u,"Invalid desired Rotator channel");
            const auto value=state.desired[axis];++desiredReads[axis];Read("desired",axis);return value;};
        q.rotationRate=[&](Id actor,unsigned axis){Actor(actor);Require(axis<3u,"Invalid rate Rotator channel");
            const auto value=state.rate[axis];++rateReads[axis];Read("rate",axis);return value;};
        q.level=[&](Id actor){Actor(actor);order.push_back("level");return state.level;};
        q.moveActor=[&](Id level,Id actor,PortableModelVec3 delta,Rotator rotation,QuestVr::OriginalActorLineHit& hit,
            std::array<std::int32_t,4u> options) {
            Actor(actor);Require(level==state.level,"Rotation sweep lost actual current Level identity");
            Require(Bits(delta.x)==0u&&Bits(delta.y)==0u&&Bits(delta.z)==0u&&options==std::array<std::int32_t,4u>{},
                "Rotation MoveActor changed zero translation/four zero options");
            Require(hit.next==0u&&hit.actor==0u&&hit.primitive==0u&&hit.item==-1&&Bits(hit.time)==Bits(1.0f)&&
                Bits(hit.location.x)==0u&&Bits(hit.location.y)==0u&&Bits(hit.location.z)==0u&&
                Bits(hit.normal.x)==0u&&Bits(hit.normal.y)==0u&&Bits(hit.normal.z)==0u,
                "Rotation MoveActor changed original native hit seed");
            order.push_back("move");moves.push_back({rotation,moveResult});
            if(applyMove)for(unsigned axis=0u;axis<3u;++axis)Rotate(axis,rotation[axis]);
            if(onMove)onMove(rotation);
            hit.time=0.125f;hit.normal={1,2,3};hit.actor=6u;return moveResult;
        };
        q.isProbingEndedRotation=[&](Id actor){Actor(actor);++probeReads;Read("probe",0u);if(onProbe)onProbe();return state.probe;};
        q.endedRotation=[&](Id actor){Actor(actor);order.push_back("ended");Write();++state.events;if(onEnded)onEnded();};
        return q;
    }
    Stats Direct(float delta,const Queries& q){return QuestVr::RotateOriginalActor(1u,delta,q);}
    Stats RunWith(float delta,const Queries& q){const auto before=state;try{return Direct(delta,q);}catch(...){state=before;throw;}}
    Stats Run(float delta=1.0f){return RunWith(delta,GetQueries());}
};

void EntryAndOrdering() {
    Fixture inactive;inactive.state.flags=0u;auto sparse=inactive.GetQueries();
    sparse.rotation={};sparse.desiredRotation={};sparse.rotationRate={};sparse.level={};sparse.moveActor={};
    sparse.isProbingEndedRotation={};sparse.endedRotation={};
    const auto none=inactive.RunWith(std::bit_cast<float>(0x7fc12345u),sparse);
    Require(none.inactive&&none.steps==1u&&inactive.order==std::vector<std::string>{"flags"},
        "Inactive rotation consumed exceptional dt or inactive fields/providers");
    Fixture equal;equal.state.flags=0x40u;equal.state.desired=equal.state.rotation;auto equalQueries=equal.GetQueries();
    equalQueries.rotationRate={};equalQueries.level={};equalQueries.moveActor={};equalQueries.isProbingEndedRotation={};equalQueries.endedRotation={};
    const auto same=equal.RunWith(std::bit_cast<float>(0x7f800000u),equalQueries);
    Require(same.alreadyDesired&&same.conversions==0u&&equal.state.events==0u&&
        equal.order==std::vector<std::string>{"flags","rotation-0","desired-0","rotation-1","desired-1","rotation-2","desired-2"},
        "Initial raw equality did not short-circuit before conversion/completion callback");
    Fixture upper;upper.state.flags=0x40u;upper.state.rotation={65536,0,0};upper.state.desired={0,0,0};upper.state.rate={0,0,0};
    const auto raw=upper.Run();
    Require(!raw.alreadyDesired&&raw.conversions==3u&&raw.proposed==upper.state.rotation&&upper.moves.empty()&&upper.state.events==0u,
        "Raw upper16 initial equality/zero-Delta retention was normalized modulo65536");
    Fixture fixed;const auto fixedResult=fixed.Run();
    Require(fixedResult.proposed==Rotator{110,220,330}&&fixed.state.rotation==fixedResult.proposed&&fixedResult.conversions==3u&&
        fixedResult.fixedTurns==3u&&fixed.moves.size()==1u&&fixed.state.events==0u&&fixedResult.steps==fixed.work,
        "Fixed-direction rotation failed original whole phase");
    Require(fixed.order==std::vector<std::string>{"flags","rate-2","rotation-0","rotation-1","rotation-2","rate-1","rate-0",
        "desired-1","flags","flags","desired-0","flags","flags","desired-2","flags","rotation-0","level","move","flags"},
        "Original conversion Roll/Yaw/Pitch or update Yaw/Pitch/Roll ordering changed");
    Fixture desired;desired.state.flags=0x40u;const auto desiredResult=desired.Run();
    Require(desiredResult.proposed==Rotator{110,220,330}&&desired.state.events==1u&&desiredResult.probeCalls==1u&&desiredResult.eventCalls==1u,
        "Reached desired rotation failed live probe/selected completion event");
    Require(desired.order==std::vector<std::string>{"flags","rotation-0","desired-0","rate-2","rotation-0","rotation-1","rotation-2","rate-1","rate-0",
        "desired-1","desired-1","flags","flags","desired-0","desired-0","flags","flags","desired-2","desired-2","flags",
        "rotation-0","level","move","flags","rotation-0","desired-0","rotation-1","desired-1","rotation-2","desired-2","probe","ended"},
        "Desired rotation fresh equality/argument/flag/completion protocol changed");
    Fixture zero;zero.state.rate={0,0,0};const auto zeroResult=zero.Run();
    Require(zeroResult.conversions==3u&&zeroResult.fixedTurns==0u&&zero.moves.empty()&&zero.rotationReads==std::array<unsigned,3u>{2,2,2}&&
        zero.desiredReads==std::array<unsigned,3u>{0,0,0},"Zero converted deltas consumed inactive desired/axis fields or moved");
}

void CapturedAndFreshReads() {
    Fixture captured;captured.state.flags=0x40u;captured.state.desired={100,200,301};
    captured.afterRead=[&](const std::string& field,unsigned axis){if(field=="rate"&&axis==0u)captured.Flags(0x20u);};
    const auto capturedResult=captured.Run();
    Require(capturedResult.proposed==Rotator{110,200,330}&&capturedResult.fixedTurns==2u,
        "Yaw failed captured initial rotate flag or Pitch/Roll failed live flag reads");
    Fixture live;live.state.desired={100,200,310};
    live.afterRead=[&](const std::string& field,unsigned axis){if(field=="rate"&&axis==0u)live.Flags(0x40u);};
    const auto liveResult=live.Run();
    Require(liveResult.proposed==Rotator{100,200,310}&&liveResult.fixedTurns==2u&&live.state.events==1u,
        "Captured false Yaw gate was replaced by live flag or live Pitch equality gate was ignored");
    Fixture argument;argument.state.flags=0x40u;argument.state.rotation={0,0,0};argument.state.desired={0,10,0};argument.state.rate={0,15,0};
    argument.afterRead=[&](const std::string& field,unsigned axis){if(field=="desired"&&axis==1u&&argument.desiredReads[1]==2u)argument.Desired(1u,20);};
    Require(argument.Run().proposed==Rotator{0,15,0}&&argument.desiredReads[1]>=3u,
        "Axis call cached pre-callback DesiredYaw instead of fresh reached argument");
    Fixture freshFlags;freshFlags.state.flags=0x40u;freshFlags.state.rotation={0,0,0};freshFlags.state.desired={0,10,0};freshFlags.state.rate={0,15,0};
    freshFlags.afterRead=[&](const std::string& field,unsigned axis){if(field=="desired"&&axis==1u&&freshFlags.desiredReads[1]==3u)freshFlags.Flags(0u);};
    Require(freshFlags.Run().proposed==Rotator{0,0,0}&&freshFlags.moves.empty(),"fixedTurn reused stale axis-gate rotation flags");
    Fixture capturedPose;capturedPose.afterRead=[&](const std::string& field,unsigned axis){if(field=="rotation"&&axis==0u&&capturedPose.rotationReads[0]==1u)capturedPose.Rotate(0u,500);};
    Require(capturedPose.Run().proposed==Rotator{110,220,330},"Axis updates reloaded current pose rather than captured original current channels");
    Fixture unchangedComplete;unchangedComplete.state.rate={0,0,0};
    unchangedComplete.afterRead=[&](const std::string& field,unsigned axis){if(field=="rotation"&&axis==2u&&unchangedComplete.rotationReads[2]==2u){
        unchangedComplete.Flags(0x40u);for(unsigned i=0u;i<3u;++i)unchangedComplete.Desired(i,unchangedComplete.state.rotation[i]);}};
    const auto completed=unchangedComplete.Run();
    Require(completed.moveCalls==0u&&completed.eventCalls==1u,"Unchanged proposal skipped fresh actual completion state");
}

void MoveAndCompletion() {
    Fixture blocked;blocked.state.flags=0x40u;blocked.applyMove=false;blocked.moveResult=0;
    const auto blockedResult=blocked.Run();
    Require(blockedResult.moveCalls==1u&&blocked.state.rotation==Rotator{100,200,300}&&blocked.state.events==0u,
        "False/blocked MoveActor result caused fallback Rotation store or proposed-pose completion");
    Fixture falseAccepted;falseAccepted.state.flags=0x40u;falseAccepted.moveResult=0;
    Require(falseAccepted.Run().eventCalls==1u&&falseAccepted.state.rotation==Rotator{110,220,330},
        "False MoveActor result was incorrectly promoted to exception or completion veto");
    Fixture altered;altered.state.flags=0x40u;altered.onMove=[&](Rotator){altered.Rotate(1u,999);};
    Require(altered.Run().probeCalls==0u&&altered.state.events==0u,"Completion used proposal instead of callback-modified actual pose");
    Fixture disable;disable.state.flags=0x40u;disable.onMove=[&](Rotator){disable.Flags(0u);};
    Require(disable.Run().probeCalls==0u&&disable.state.events==0u,"Completion reused stale initial rotate flag");
    Fixture changedDesired;changedDesired.state.flags=0x40u;changedDesired.onMove=[&](Rotator){changedDesired.Desired(2u,331);};
    Require(changedDesired.Run().probeCalls==0u,"Completion cached initial DesiredRotation");
    Fixture probeOff;probeOff.state.flags=0x40u;probeOff.state.probe=false;auto noEvent=probeOff.GetQueries();noEvent.endedRotation={};
    Require(probeOff.RunWith(1.0f,noEvent).probeCalls==1u&&probeOff.state.events==0u,"Inactive selected EndedRotation provider became mandatory");
    Fixture deleted;deleted.state.flags=0x40u;deleted.onMove=[&](Rotator){deleted.Write();deleted.state.deleted=true;deleted.Effects();};
    Require(deleted.Run().eventCalls==1u&&deleted.state.deleted&&deleted.state.support==9u&&deleted.state.sounds==1u,
        "Rotation body invented deletion shortcut or discarded same-root native move effects");
    Fixture reentry;reentry.state.flags=0x40u;reentry.onMove=[&](Rotator){
        reentry.Flags(0u);const auto nested=reentry.Direct(std::bit_cast<float>(0x7fc12345u),reentry.GetQueries());
        Require(nested.inactive,"Nested original rotation did not observe actual live flags");reentry.Flags(0x40u);reentry.Effects();};
    reentry.onEnded=[&]{reentry.Desired(0u,999);};
    Require(reentry.Run().eventCalls==1u&&reentry.state.desired[0]==999&&reentry.state.sounds==1u,
        "Synchronous rotation reentry or final selected event mutation was normalized/undone");
}

void MissingAndRefusalOrdering() {
    NativeRefuses([&]{QuestVr::RotateOriginalActor(0u,1.0f,{});},"Null Actor accepted");
    NativeRefuses([&]{QuestVr::RotateOriginalActor(1u,1.0f,{});},"Missing active flag provider accepted");
    Fixture entry;entry.state.flags=0x40u;auto missingEntry=entry.GetQueries();missingEntry.rotation={};
    NativeRefuses([&]{entry.RunWith(1.0f,missingEntry);},"Missing reached initial Rotation provider accepted");
    Fixture rate;auto missingRate=rate.GetQueries();missingRate.rotationRate={};
    NativeRefuses([&]{rate.RunWith(1.0f,missingRate);},"Missing reached rate provider accepted");
    Fixture desired;auto missingDesired=desired.GetQueries();missingDesired.desiredRotation={};
    NativeRefuses([&]{desired.RunWith(1.0f,missingDesired);},"Missing reached desired argument accepted");
    Fixture level;auto missingLevel=level.GetQueries();missingLevel.level={};
    NativeRefuses([&]{level.RunWith(1.0f,missingLevel);},"Missing reached Level provider accepted");
    Fixture nullLevel;nullLevel.state.level=0u;
    NativeRefuses([&]{nullLevel.Run();},"Null actual Level was invented as successful movement");
    Require(nullLevel.moves.empty(),"Null Level refusal occurred after entering MoveActor");
    Fixture move;auto missingMove=move.GetQueries();missingMove.moveActor={};
    NativeRefuses([&]{move.RunWith(1.0f,missingMove);},"Missing reached MoveActor provider accepted");
    Fixture probe;probe.state.flags=0x40u;auto missingProbe=probe.GetQueries();missingProbe.isProbingEndedRotation={};const auto beforeProbe=probe.state;
    NativeRefuses([&]{probe.RunWith(1.0f,missingProbe);},"Missing reached probe provider accepted");
    Require(probe.state==beforeProbe&&probe.moves.size()==1u,"Late probe refusal did not roll back completed native movement");
    Fixture event;event.state.flags=0x40u;auto missingEvent=event.GetQueries();missingEvent.endedRotation={};
    NativeRefuses([&]{event.RunWith(1.0f,missingEvent);},"Missing reached selected EndedRotation provider accepted");
    Fixture noMove;noMove.state.rate={0,0,0};auto sparse=noMove.GetQueries();sparse.level={};sparse.moveActor={};sparse.desiredRotation={};
    sparse.isProbingEndedRotation={};sparse.endedRotation={};
    Require(noMove.RunWith(1.0f,sparse).moveCalls==0u,"Unchanged/inactive native movement/completion providers became mandatory");
    Fixture badTime;const auto beforeBad=badTime.state;
    NativeRefuses([&]{badTime.Run(std::bit_cast<float>(0x7fc12345u));},"Reached nonfinite rotation time did not refuse");
    Require(badTime.state==beforeBad&&badTime.order==std::vector<std::string>{"flags","rate-2","rotation-0","rotation-1","rotation-2"},
        "Roll conversion refused before original current capture or after later rate/axis work");
    Fixture rollOverflow;rollOverflow.state.rate={0,0,std::numeric_limits<std::int32_t>::max()};
    NativeRefuses([&]{rollOverflow.Run(std::bit_cast<float>(0x5f000000u));},"Reached signed64 overflow conversion accepted");
    Require(rollOverflow.rateReads==std::array<unsigned,3u>{0,0,1},"Overflow incorrectly read later rates before original Roll conversion");
    Fixture yawOverflow;yawOverflow.state.rate={0,1,0};
    NativeRefuses([&]{yawOverflow.Run(std::bit_cast<float>(0x5f000000u));},"Reached Yaw conversion overflow accepted");
    Require(yawOverflow.rateReads==std::array<unsigned,3u>{0,1,1},"Yaw overflow read Pitch before original conversion completed");
    Fixture pitchOverflow;pitchOverflow.state.rate={1,0,0};
    NativeRefuses([&]{pitchOverflow.Run(std::bit_cast<float>(0x5f000000u));},"Reached Pitch conversion overflow accepted");
    Require(pitchOverflow.rateReads==std::array<unsigned,3u>{1,1,1}&&pitchOverflow.moves.empty(),
        "Pitch conversion failure did not precede all original axis updates/movement");
}

void CutoffsAndCallbackFailures() {
    const auto configure=[](Fixture& f){f.state.flags=0x40u;f.onMove=[&f](Rotator){f.Effects();};
        f.onEnded=[&f]{f.Flags(0u);f.Desired(1u,777);};};
    Fixture baseline;configure(baseline);const auto result=baseline.Run();
    Require(result.eventCalls==1u&&baseline.writes==10u&&baseline.state.sounds==1u,"Cutoff control did not reach all original move/event effects");
    for(std::size_t limit=0u;limit<baseline.work;++limit) {
        Fixture f;configure(f);f.workLimit=limit;const auto before=f.state;
        Stops([&]{f.Run();},Stop::Work,"Work cutoff did not abort same-root original rotation");
        Require(f.state==before&&f.work==limit,"Work cutoff lost original full-root rollback/shared allowance");
    }
    for(std::size_t limit=0u;limit<baseline.writes;++limit) {
        Fixture f;configure(f);f.writeLimit=limit;const auto before=f.state;
        Stops([&]{f.Run();},Stop::Write,"Write cutoff did not abort original move/event effects");
        Require(f.state==before&&f.writes==limit,"Late scalar write cutoff did not undo pose/support/random/audio/clock/event state");
    }
    Fixture direct;configure(direct);direct.writeLimit=9u;
    Stops([&]{direct.Direct(1.0f,direct.GetQueries());},Stop::Write,"Direct late event cutoff unexpectedly succeeded");
    Require(direct.state.rotation==Rotator{110,220,330}&&direct.state.support==9u&&direct.state.sounds==1u&&
        direct.state.events==1u&&direct.state.flags==0u&&direct.state.desired[1]==220,
        "Helper introduced forbidden local partial undo instead of preserving caller-owned prefix");
    for(auto point:std::array<Stop,4u>{Stop::Read,Stop::Move,Stop::Probe,Stop::Event}) {
        Fixture f;configure(f);const auto fail=[&]{f.Effects();throw ExpectedStop(point);};
        if(point==Stop::Read)f.afterRead=[&](const std::string& field,unsigned axis){if(field=="rate"&&axis==1u)fail();};
        else if(point==Stop::Move)f.onMove=[&](Rotator){fail();};
        else if(point==Stop::Probe)f.onProbe=fail;else f.onEnded=fail;
        const auto before=f.state;Stops([&]{f.Run();},point,"Read/native callback failure was swallowed");
        Require(f.state==before,"Callback/reflected read failure lost whole caller-root undo");
    }
}
} // namespace

int main() {
    try {
        FixedTurnControls();ConversionControls();EntryAndOrdering();CapturedAndFreshReads();MoveAndCompletion();
        MissingAndRefusalOrdering();CutoffsAndCallbackFailures();
        std::cout<<"Original Actor physicsRotation: "<<checks<<" checks, "<<refusals<<" expected refusals\n";return 0;
    }catch(const std::exception& error){std::cerr<<"Original Actor physicsRotation test failed: "<<error.what()<<'\n';return 1;}
}
