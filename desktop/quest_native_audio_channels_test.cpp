#include "quest_native_audio_channels.h"
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace QuestVr;
using Point=std::array<float,3u>;
using State=NativeAudioChannels;
using Slot=NativeAudioChannel;
std::size_t checks{},refusals{},oracleCases{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void Refuse(F&& action,const char* reason) {
    bool rejected{};try{action();}catch(const std::runtime_error&){rejected=true;}
    ++refusals;Require(rejected,reason);
}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
[[maybe_unused]] float Float(std::uint32_t word){return std::bit_cast<float>(word);}
bool Same(const Slot& a,const Slot& b) {
    if(a.actor!=b.actor||a.sound!=b.sound||a.id!=b.id||a.voice!=b.voice)return false;
    for(std::size_t axis=0u;axis<3u;++axis)if(Bits(a.location[axis])!=Bits(b.location[axis]))return false;
    return Bits(a.volume)==Bits(b.volume)&&Bits(a.radius)==Bits(b.radius)&&Bits(a.pitch)==Bits(b.pitch)&&
        Bits(a.priority)==Bits(b.priority)&&Bits(a.elapsed)==Bits(b.elapsed);
}
bool Same(const State& a,const State& b) {
    if(a.freeCounter!=b.freeCounter||a.voiceSerial!=b.voiceSerial||a.liveCount!=b.liveCount)return false;
    for(std::size_t slot=0u;slot<a.slots.size();++slot)if(!Same(a.slots[slot],b.slots[slot]))return false;
    return true;
}
struct Stops {
    std::vector<std::pair<std::size_t,std::uint64_t>> values;
    void operator()(std::size_t slot,std::uint64_t voice){values.emplace_back(slot,voice);}
};
Slot Filled(std::uint32_t id,float priority=0.0f,std::uint64_t voice=0u,std::uintptr_t actor=101u) {
    Slot out;out.actor=actor;out.sound=201u;out.id=id;out.location={11.0f,12.0f,13.0f};
    out.volume=0.75f;out.radius=900.0f;out.pitch=1.25f;out.priority=priority;out.elapsed=2.5f;out.voice=voice;return out;
}
bool Play(State& state,Stops& stops,std::uint32_t id=2u,float volume=1.0f,float radius=100.0f,
    std::uintptr_t sound=301u,Point location={0,0,0},Point listener={0,0,0},bool viewport=true,float pitch=1.0f) {
    return PlayNativeAudioChannel(state,viewport,101u,id,sound,location,volume,radius,pitch,listener,stops);
}

void ProtocolOrderAndCounter() {
    State state;Stops stops;
    Require(!Play(state,stops,1u)&&state.freeCounter==0xffffffffu,"SLOT_None zero-channel call failed its counter mutation");
    Require(!Play(state,stops,0u)&&state.freeCounter==0xfffffffeu,"Zero channels silently disabled successive SLOT_None counter writes");
    state.liveCount=1u;
    Require(Play(state,stops,1u)&&state.freeCounter==0xfffffffdu&&state.slots[0u].id==0xffffffd0u,
        "SLOT_None did not replace the entire incoming ID including no-override bit");
    Require(!state.slots[0u].voice&&state.voiceSerial==0u&&stops.values.empty(),"PlaySound selection fabricated backend playback");
    State fresh;fresh.liveCount=1u;
    Require(Play(fresh,stops,0u)&&fresh.slots[0u].id==0xfffffff0u,"Fresh default/null-template counter did not generate first ID -16");
    fresh.freeCounter=0x10000001u;
    Require(Play(fresh,stops,0u)&&fresh.slots[0u].id==0u&&fresh.freeCounter==0x10000000u,
        "Source DWORD shift/counter wrap was normalized to a nonzero channel");
    fresh.freeCounter=0x80000000u;
    Require(Play(fresh,stops,0u)&&fresh.freeCounter==0x7fffffffu&&fresh.slots[0u].id==0xfffffff0u,
        "Signed overflow assumptions replaced exact DWORD decrement/shift");
    const auto before=fresh;
    Refuse([&]{Play(fresh,stops,0u,1.0f,0.0f,0u,{},{},false);},"Radius assertion skipped before null viewport/sound exits");
    Refuse([&]{Play(fresh,stops,0u,1.0f,-0.0f,0u,{},{},false);},"Negative zero radius bypassed native assertion");
    Require(Same(fresh,before),"Rejected radius mutated channel state before counter selection");
    Require(!Play(fresh,stops,0u,1.0f,100.0f,301u,{},{},false)&&Same(fresh,before),"Null viewport advanced counter or changed source state");
    Require(!Play(fresh,stops,0u,1.0f,100.0f,0u)&&Same(fresh,before),"Null sound advanced counter or changed source state");
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    Require(!Play(fresh,stops,0u,nan,100.0f,0u,{nan,0,0},{},false,nan)&&Same(fresh,before),
        "Finite-profile checks overrode genuine earlier null viewport/sound exits");
    Refuse([&]{Play(fresh,stops,2u,1.0f,100.0f,301u,{},{},true,nan);},"Detached native caller accepted nonfinite pitch");
    State malformed;malformed.liveCount=static_cast<std::uint32_t>(AudioVoiceCapacity+1u);
    Refuse([&]{Play(malformed,stops);},"Invalid live count bypassed bounded native inventory");
    Require(stops.values.empty(),"Early source/admission controls staged audible stop operations");
}

void SelectionAndStopSemantics() {
    State state;state.liveCount=4u;Stops stops;
    for(std::size_t slot=0u;slot<4u;++slot)state.slots[slot]=Filled(static_cast<std::uint32_t>(18u+slot*16u),0.5f,slot+1u);
    Require(Play(state,stops,2u)&&state.slots[3u].id==2u&&stops.values==std::vector<std::pair<std::size_t,std::uint64_t>>{{3u,4u}},
        "Later equal-priority candidate did not win ascending scan");
    Require(state.slots[3u].priority==1.0f&&state.slots[3u].actor==101u&&state.slots[3u].sound==301u&&
        state.slots[3u].elapsed==0.0f&&!state.slots[3u].voice,
        "Replacement stored scan minimum/old backend instead of initial priority and fresh fields");
    stops.values.clear();
    for(std::size_t slot=0u;slot<4u;++slot)state.slots[slot]=Filled(static_cast<std::uint32_t>(18u+slot*16u),-5.0f,slot+11u);
    state.slots[2u].id=3u;
    Require(Play(state,stops,2u)&&state.slots[2u].id==2u&&stops.values.front().first==2u&&stops.values.size()==1u,
        "Later same-ID ignoring bit0 did not override earlier priority candidates immediately");
    stops.values.clear();state.slots[2u]=Filled(2u,-5.0f,29u);const auto before=state;
    Require(!Play(state,stops,3u)&&Same(state,before)&&stops.values.empty(),
        "Incoming noOverride failed to reject same-ID match after earlier priority candidate");
    state.slots[0u]=Filled(2u,99.0f,31u);state.slots[2u]=Filled(2u,-99.0f,32u);
    Require(Play(state,stops,2u)&&stops.values.front().first==0u&&state.slots[2u].voice==32u,
        "First same-ID was skipped in favor of a later lower priority match");
    stops.values.clear();
    for(std::size_t slot=0u;slot<4u;++slot)state.slots[slot]=Filled(static_cast<std::uint32_t>(18u+slot*16u),2.0f,slot+40u);
    const auto allHigher=state;
    Require(!Play(state,stops,2u)&&Same(state,allHigher)&&stops.values.empty(),"Ineligible priority request stole a channel");
    state.slots[1u].priority=-4.0f;state.slots[2u].priority=-2.0f;
    Require(Play(state,stops,2u,1.0f,100.0f,301u,{200,0,0})&&state.slots[1u].id==2u&&
        state.slots[1u].priority==-1.0f,"Negative unclamped priority/minimum scan semantics changed");
    stops.values.clear();state.slots[0u]=Filled(2u,5.0f,51u);
    Require(Play(state,stops,2u,1.0f,100.0f,std::numeric_limits<std::uintptr_t>::max())&&
        Same(state.slots[0u],Slot{})&&stops.values==std::vector<std::pair<std::size_t,std::uint64_t>>{{0u,51u}},
        "Internal Sound=-1 sentinel failed to stop selected channel without filling replacement");
    stops.values.clear();state.slots[0u]=Filled(18u,1.0f,0u);
    StopNativeAudioChannel(state,0u,stops);
    Require(Same(state.slots[0u],Slot{})&&stops.values.empty(),"Handle-less StopSound failed semantic reset or invented backend stop");
    state.slots[0u]=Filled(18u,1.0f,61u);state.slots[1u]=Filled(19u,1.0f,62u);
    Require(StopNativeAudioId(state,19u,stops)&&!state.slots[0u].id&&state.slots[1u].voice==62u&&stops.values.size()==1u,
        "StopSoundId did not stop only first ID match ignoring bit0");
    const auto noMatch=state;stops.values.clear();
    Require(!StopNativeAudioId(state,211u,stops)&&Same(state,noMatch)&&stops.values.empty(),"Missing StopSoundId mutated channel inventory");
    Refuse([&]{StopNativeAudioChannel(state,state.liveCount,stops);},"Stop accepted an index outside actual configured live count");
}

void NoteDestroyAndCallerJournal() {
    State state;state.liveCount=6u;state.freeCounter=79u;state.voiceSerial=99u;Stops stops;
    state.slots[0u]=Filled(8u,1.0f,101u,11u);state.slots[1u]=Filled(10u,2.0f,102u,11u);
    state.slots[2u]=Filled(25u,3.0f,103u,11u);state.slots[3u]=Filled(8u,4.0f,104u,12u);
    state.slots[4u]=Filled(0u,5.0f,0u,11u);state.slots[5u]=Filled(24u,6.0f,0u,11u);
    const auto before=state;
    NoteNativeAudioActorDestroyed(state,11u,stops);
    auto detached=before.slots[1u];detached.actor=0u;
    Require(Same(state.slots[0u],Slot{})&&Same(state.slots[1u],detached)&&Same(state.slots[2u],Slot{})&&
        Same(state.slots[3u],before.slots[3u])&&Same(state.slots[5u],Slot{}),
        "NoteDestroy blanket-stopped one-shots, changed another owner or filtered handle-less ambient state");
    detached=before.slots[4u];detached.actor=0u;
    Require(Same(state.slots[4u],detached)&&state.freeCounter==79u&&state.voiceSerial==99u&&
        stops.values==std::vector<std::pair<std::size_t,std::uint64_t>>{{0u,101u},{2u,103u}},
        "NoteDestroy failed ascending ambient stops or active-handle-free ownership scan");
    const auto after=state;
    Refuse([&]{NoteNativeAudioActorDestroyed(state,0u,stops);},"NoteDestroy accepted null Actor");
    Require(Same(state,after),"Null NoteDestroy mutated state");

    // The helper stages caller effects and does not pretend each suboperation
    // is an independent transaction. A later callback failure can follow an
    // earlier detach/stop or SLOT_None counter write; caller root must undo all.
    state=before;auto& first=state.slots[0u];first.id=10u;
    const auto rollback=state;std::vector<std::pair<std::size_t,std::uint64_t>> staged;
    Refuse([&]{NoteNativeAudioActorDestroyed(state,11u,[&](auto slot,auto voice) {
        staged.emplace_back(slot,voice);throw std::runtime_error("late reserved-stop admission failure");
    });},"Late NoteDestroy staging failure did not propagate to caller journal");
    Require(state.slots[0u].actor==0u&&state.slots[1u].actor==0u&&state.slots[2u].actor==11u&&staged.size()==1u,
        "Failed staging callback lost original reached detach prefix/order");
    state=rollback;staged.clear();
    Require(Same(state,rollback)&&staged.empty(),"Caller rollback did not restore audio before-state and unpublished commands");
    state=State{};state.liveCount=1u;state.slots[0u]=Filled(2u,0.0f,401u);const auto counterBefore=state;
    Refuse([&]{PlayNativeAudioChannel(state,true,11u,0u,201u,{},1.0f,100.0f,1.0f,{},
        [&](auto slot,auto voice){staged.emplace_back(slot,voice);throw std::runtime_error("reserve failed");});},
        "Failed selected-stop staging did not propagate");
    Require(state.freeCounter==0xffffffffu&&state.slots[0u].voice==401u&&staged.size()==1u,
        "Failed selected stop changed record before callback or discarded already reached counter mutation");
    state=counterBefore;staged.clear();Require(Same(state,counterBefore),"Caller rollback failed counter/record before-state");
    state.voiceSerial=AudioVoiceIdentityMaximum-1u;
    Require(NextNativeAudioVoice(state)==AudioVoiceIdentityMaximum,"Final valid opaque voice ID refused");
    Refuse([&]{NextNativeAudioVoice(state);},"Opaque voice serial overflow was not refused");
    Require(state.voiceSerial==AudioVoiceIdentityMaximum,"Refused voice overflow reused an old backend identity");
    state.voiceSerial=std::numeric_limits<std::uint64_t>::max();
    Refuse([&]{NextNativeAudioVoice(state);},"Malformed opaque voice serial wrapped into a reused identity");
    Require(state.voiceSerial==std::numeric_limits<std::uint64_t>::max(),"Malformed serial refusal wrote a valid-looking replacement ID");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct X87Profile {
    unsigned short saved{};
    X87Profile() {
        asm volatile("fnstcw %0":"=m"(saved));
        const unsigned short ordinary=static_cast<unsigned short>((saved&~0x0f00u)|0x0300u|0x003fu);
        asm volatile("fldcw %0"::"m"(ordinary));
    }
    ~X87Profile(){asm volatile("fldcw %0"::"m"(saved));}
};
struct OracleValue {long double retained;float stored;};
OracleValue PriorityOracle(Point source,Point listener,float volume,float radius) {
    // Host instructions only. Never executes/loads an original DLL. Every
    // volatile extended spill preserves PC64 rounding of original operations.
    volatile long double x=static_cast<long double>(source[0u])-listener[0u];
    volatile long double y=static_cast<long double>(source[1u])-listener[1u];
    volatile long double z=static_cast<long double>(source[2u])-listener[2u];
    volatile long double xx=x*x,yy=y*y,zz=z*z;
    volatile long double xy=xx+yy,sum=xy+zz;
    const double argument=static_cast<double>(sum);long double root{};
    asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(root):"m"(argument):"st");
    volatile long double quotient=root/static_cast<long double>(radius);
    volatile long double distanceFactor=1.0L-quotient;
    volatile long double value=distanceFactor*static_cast<long double>(volume);
    const float stored=static_cast<float>(value);return {value,stored};
}
NativeScalar::Real OracleReal(long double value) {
    static_assert(sizeof(long double)>=10u&&std::numeric_limits<long double>::digits==64);
    std::uint64_t significand{};std::uint16_t exponent{};
    std::memcpy(&significand,&value,8u);std::memcpy(&exponent,reinterpret_cast<const char*>(&value)+8u,2u);
    const bool negative=(exponent&0x8000u)!=0u;const auto biased=exponent&0x7fffu;
    if(biased==0x7fffu) {
        if((significand&0x7fffffffffffffffull)==0u)return NativeScalar::Infinity(negative);
        return NativeScalar::NaN(significand&0x7fffffffffffffffull,negative);
    }
    return {{static_cast<NativeScalar::Wide>(significand),biased?static_cast<int>(biased)-16383-63:-16445,negative},
        NativeScalar::Kind::Finite,0u,false};
}
std::size_t OracleSelect(const State& state,std::uint32_t id,long double initial) {
    std::size_t selected=AudioVoiceCapacity;volatile long double running=initial;
    for(std::size_t slot=0u;slot<state.liveCount;++slot) {
        const auto& old=state.slots[slot];
        if(((old.id^id)&0xfffffffeu)==0u)return id&1u?AudioVoiceCapacity:slot;
        if(running>=static_cast<long double>(old.priority)){selected=slot;running=old.priority;}
    }
    return selected;
}
void CheckOracle(Point source,Point listener,float volume,float radius,std::uint32_t seed) {
    const auto original=PriorityOracle(source,listener,volume,radius);
    const auto actual=OriginalAudioPriority(source,listener,volume,radius);
    if(Bits(actual.stored)!=Bits(original.stored)||
        !NativeScalar::Compare(180u,actual.retained,OracleReal(original.retained))) {
        std::cerr<<"Priority oracle mismatch seed="<<seed<<" volume="<<volume<<" radius="<<radius
            <<" actualBits="<<std::hex<<Bits(actual.stored)<<" expectedBits="<<Bits(original.stored)<<std::dec<<'\n';
        throw std::runtime_error("Native priority did not match independent retained x87 oracle");
    }
    State state;state.liveCount=8u;Stops stops;
    for(std::size_t slot=0u;slot<state.liveCount;++slot) {
        float priority=std::nextafter(original.stored,(slot&1u)?std::numeric_limits<float>::infinity():-
            std::numeric_limits<float>::infinity());
        if(slot%3u==0u)priority=original.stored;
        if(std::isnan(priority))priority=0.0f;
        state.slots[slot]=Filled(static_cast<std::uint32_t>(18u+slot*16u),priority,slot+1u);
    }
    if(seed%3u==1u)state.slots[seed%state.liveCount].id=2u;
    const std::uint32_t id=seed%3u==2u?3u:2u;const auto expected=OracleSelect(state,id,original.retained);
    const auto before=state;
    const auto accepted=Play(state,stops,id,volume,radius,301u,source,listener);
    Require(accepted==(expected!=AudioVoiceCapacity),"Native initial candidate acceptance differs from x87 scan");
    if(accepted) {
        Require(stops.values.size()==1u&&stops.values[0u].first==expected&&state.slots[expected].id==id&&
            Bits(state.slots[expected].priority)==Bits(original.stored),"Native channel selected wrong exact x87 candidate or stored wrong initial priority");
    }else Require(Same(state,before)&&stops.values.empty(),"Rejected x87 request mutated inventory/staged stops");
    ++oracleCases;
}
void PriorityAndSelectionOracle() {
    X87Profile profile;
    const auto tiny=std::numeric_limits<float>::denorm_min(),small=std::numeric_limits<float>::min();
    const auto maximum=std::numeric_limits<float>::max();
    const std::array<float,12u> values{{0.0f,-0.0f,1.0f,-1.0f,100.0f,-100.0f,tiny,-tiny,small,maximum,-maximum,Float(0x3f800001u)}};
    std::uint32_t seed{};
    for(std::size_t at=0u;at<values.size();++at)for(float volume:{-1.0f,0.0f,1.0f,maximum})
        for(float radius:{-1.0f,1.0f,1000.0f,tiny,maximum})
            CheckOracle({values[at],values[(at+1u)%values.size()],values[(at+2u)%values.size()]},
                {values[(at+3u)%values.size()],values[(at+4u)%values.size()],values[(at+5u)%values.size()]},volume,radius,seed++);
    std::uint32_t random=0x91e10da5u;
    const auto next=[&](){random=random*1664525u+1013904223u;return random;};
    const auto coordinate=[&](){return static_cast<float>(static_cast<std::int32_t>(next()))/65536.0f;};
    bool witnessedUnspilled{};
    for(std::size_t at=0u;at<4096u;++at) {
        Point source{coordinate(),coordinate(),coordinate()},listener{coordinate(),coordinate(),coordinate()};
        const float volume=static_cast<float>((next()&0xffffu)+1u)/32768.0f;
        const float radius=static_cast<float>((next()&0xfffffu)+1u)/16.0f;
        CheckOracle(source,listener,volume,radius,seed++);
        const auto oracle=PriorityOracle(source,listener,volume,radius);
        // Select one slot exactly at the Float spill. If live priority rounded
        // upward into it, a spill/reload implementation incorrectly accepts.
        if(oracle.retained<static_cast<long double>(oracle.stored)&&std::isfinite(oracle.stored)) {
            State state;state.liveCount=1u;state.slots[0u]=Filled(18u,oracle.stored,91u);Stops stops;
            Require(!Play(state,stops,2u,volume,radius,301u,source,listener)&&stops.values.empty(),
                "Initial scan reloaded the rounded Float priority and accepted a too-high channel");
            witnessedUnspilled=true;
        }
    }
    Require(witnessedUnspilled,"Priority oracle never exercised a live-register versus Float-spill selection boundary");
}
#endif

void FiniteProfileGuards() {
    const auto nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for(float value:{nan,inf,-inf}) {
        Refuse([&]{OriginalAudioPriority({}, {},value,100.0f);},"Nonfinite priority volume entered supported finite profile");
        Refuse([&]{OriginalAudioPriority({}, {},1.0f,value);},"Nonfinite priority radius entered supported finite profile");
        for(std::size_t axis=0u;axis<3u;++axis) {
            Point point{};point[axis]=value;
            Refuse([&]{OriginalAudioPriority(point,{},1.0f,100.0f);},"Nonfinite source coordinate accepted");
            Refuse([&]{OriginalAudioPriority({},point,1.0f,100.0f);},"Nonfinite listener coordinate accepted");
        }
    }
    for(float radius:{0.0f,-0.0f})Refuse([&]{OriginalAudioPriority({}, {},1.0f,radius);},"Zero priority radius was silently clamped");
    const auto beyond=OriginalAudioPriority({200,0,0},{},1.0f,100.0f);
    Require(beyond.stored==-1.0f,"Out-of-radius source priority was clamped instead of remaining negative");
    const auto negativeRadius=OriginalAudioPriority({200,0,0},{},1.0f,-100.0f);
    Require(negativeRadius.stored==3.0f,"Finite negative source radius was silently replaced/clamped");
}
} // namespace
int main() {
    try {
        ProtocolOrderAndCounter();SelectionAndStopSemantics();NoteDestroyAndCallerJournal();FiniteProfileGuards();
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        PriorityAndSelectionOracle();
#endif
        std::cout<<"PASS original native audio finite channel policy: "<<checks<<" checks/"<<refusals
            <<" refusals, "<<oracleCases<<" independent host x87 priority/selection cases. "
              "Not AudioSubsystem UObject initialization, AAudio hardware, committed startup or crate gameplay.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL native audio channels: "<<error.what()<<'\n';return 1;}
}
