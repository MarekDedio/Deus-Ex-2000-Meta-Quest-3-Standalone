#include "quest_scripted_pawn_decay.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Field=QuestVr::OriginalScriptedPawnDecayField;
using Queries=QuestVr::OriginalScriptedPawnDecayQueries;
using Stats=QuestVr::OriginalScriptedPawnDecayStats;
constexpr Id actor=0x1234u;
std::size_t checks{},refusals{},oracleCases{};
void Require(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
template<class Function> void Refuses(Function&& function,const char* message) {
    bool refused{};try{function();}catch(const std::exception&){refused=true;}
    Require(refused,message);++refusals;
}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
float Float(std::uint32_t word) {return std::bit_cast<float>(word);}
std::size_t Index(Field field) {return static_cast<std::size_t>(field);}
struct Alliance {bool named{},permanent{};float agitation{};};
struct State {
    std::array<float,6u> values{};
    bool changed=true;
    std::uint32_t unrelated=0xa5a50000u;
    std::array<Alliance,16u> alliances{};
};
using Stores=std::vector<std::pair<std::uint32_t,std::uint32_t>>;
struct Fixture {
    State state;
    std::vector<std::string> events;
    Stores stores;
    std::size_t work{},limit=std::numeric_limits<std::size_t>::max();
    std::size_t failProvider=std::numeric_limits<std::size_t>::max(),effects{};
    std::function<void(const std::string&)> onEvent;
    void Event(std::string event) {
        events.push_back(std::move(event));
        ++effects;
        if(onEvent)onEvent(events.back());
        if(events.size()-1u==failProvider)throw std::runtime_error("caller provider/store failure");
    }
    Queries Query() {
        Queries q;
        q.work=[&]{if(work++>=limit)throw std::runtime_error("same caller work exhausted");};
        q.readFloat=[&](Id a,Field field) {
            Require(a==actor,"Float read changed Actor");Event("r"+std::to_string(Index(field)));
            return state.values.at(Index(field));
        };
        q.writeFloat=[&](Id a,Field field,float value) {
            Require(a==actor,"Float store changed Actor");Event("w"+std::to_string(Index(field)));
            state.values.at(Index(field))=value;stores.emplace_back(static_cast<std::uint32_t>(Index(field)),Bits(value));
        };
        q.alliancesChanged=[&](Id a) {
            Require(a==actor,"Alliances flag read changed Actor");Event("flag");return state.changed;
        };
        q.writeAlliancesChanged=[&](Id a,bool value) {
            Require(a==actor,"Alliances flag store changed Actor");Event(value?"flag=1":"flag=0");
            state.changed=value; // Preserve every unrelated packed field.
        };
        q.allianceNamed=[&](Id a,std::size_t index) {
            Require(a==actor&&index<16u,"Alliance name query escaped native bounds");
            Event("n"+std::to_string(index));return state.alliances.at(index).named;
        };
        q.alliancePermanent=[&](Id a,std::size_t index) {
            Require(a==actor&&index<16u,"Alliance flag query escaped native bounds");
            Event("p"+std::to_string(index));return state.alliances.at(index).permanent;
        };
        q.allianceAgitation=[&](Id a,std::size_t index) {
            Require(a==actor&&index<16u,"Alliance agitation read escaped native bounds");
            Event("a"+std::to_string(index));return state.alliances.at(index).agitation;
        };
        q.writeAllianceAgitation=[&](Id a,std::size_t index,float value) {
            Require(a==actor&&index<16u,"Alliance agitation store escaped native bounds");
            Event("s"+std::to_string(index));state.alliances.at(index).agitation=value;
            stores.emplace_back(static_cast<std::uint32_t>(16u+index),Bits(value));
        };
        return q;
    }
    Stats Run(bool fear,float delta) {
        return fear?QuestVr::UpdateOriginalScriptedPawnFear(actor,delta,Query()):
            QuestVr::UpdateOriginalScriptedPawnAgitation(actor,delta,Query());
    }
};
Fixture Active(bool fear) {
    Fixture f;
    if(fear) {
        f.state.values[Index(Field::FearTimer)]=0.25f;
        f.state.values[Index(Field::FearDecayRate)]=2.0f;
        f.state.values[Index(Field::FearLevel)]=0.25f;
    } else {
        f.state.values[Index(Field::AgitationCheckTimer)]=0.25f;
        f.state.values[Index(Field::AgitationTimer)]=0.25f;
        f.state.values[Index(Field::AgitationDecayRate)]=2.0f;
        f.state.alliances[15u]={true,false,0.25f};
        f.state.alliances[14u]={true,true,5.0f};
        f.state.alliances[13u]={true,false,0.0f};
    }
    return f;
}
bool Equal(const State& a,const State& b) {
    if(a.changed!=b.changed||a.unrelated!=b.unrelated)return false;
    for(std::size_t i=0u;i<a.values.size();++i)if(Bits(a.values[i])!=Bits(b.values[i]))return false;
    for(std::size_t i=0u;i<16u;++i) {
        if(a.alliances[i].named!=b.alliances[i].named||a.alliances[i].permanent!=b.alliances[i].permanent||
            Bits(a.alliances[i].agitation)!=Bits(b.alliances[i].agitation))return false;
    }
    return true;
}
void ExactOrderAndLazyPaths() {
    auto f=Active(false);const auto result=f.Run(false,1.0f);
    std::vector<std::string> expected{"r0","r0","w0","w0","r1","r1","r1","w1","r2","flag","flag=0",
        "n15","p15","a15","flag=1","a15","s15","s15","n14","p14","n13","p13","a13"};
    for(int i=12;i>=0;--i)expected.push_back("n"+std::to_string(i));
    Require(f.events==expected,"Agitation native fresh-load/store/descending-entry order changed");
    Require(result.steps==36u&&result.floatReads==6u&&result.floatWrites==3u&&result.flagReads==1u&&
        result.flagWrites==2u&&result.entriesVisited==16u&&result.nameReads==16u&&result.permanentReads==3u&&
        result.agitationReads==3u&&result.agitationWrites==2u&&f.work==result.steps,
        "Agitation provider/work counters changed");
    Require(f.state.values[0u]==0.0f&&f.state.values[1u]==0.0f&&f.state.changed&&f.state.alliances[15u].agitation==0.0f&&
        f.state.alliances[14u].agitation==5.0f&&f.state.unrelated==0xa5a50000u,"Agitation clamp/permanent/packed-field effect changed");
    f=Active(true);const auto fear=f.Run(true,1.0f);
    Require(f.events==std::vector<std::string>{"r3","r3","r3","w3","r4","r5","r5","w5","w5"},
        "Fear native third fresh timer load/store/rate/fresh level order changed");
    Require(fear.steps==9u&&fear.floatReads==6u&&fear.floatWrites==3u&&!fear.delayOnlyReturn&&
        !fear.flagReads&&!fear.entriesVisited&&f.work==fear.steps,"Fear short-delay counters changed");
    Fixture sustained;sustained.state.values[3u]=2.0f;auto q=sustained.Query();
    q.readFloat=[&](Id a,Field field) {
        Require(a==actor&&field==Field::FearTimer,"Sustained fear queried an unreachable rate/level");
        sustained.Event("r3");return sustained.state.values[3u];
    };
    const auto delayed=QuestVr::UpdateOriginalScriptedPawnFear(actor,1.0f,q);
    Require(delayed.delayOnlyReturn&&delayed.steps==4u&&sustained.state.values[3u]==1.0f&&
        sustained.events==std::vector<std::string>{"r3","r3","r3","w3"},"Sustained fear did not return before decay/level work");
    Fixture inactive;inactive.state.values[4u]=-1.0f;q=inactive.Query();q.writeFloat={};
    q.readFloat=[&](Id a,Field field) {
        Require(a==actor&&(field==Field::FearTimer||field==Field::FearDecayRate),"Nonpositive fear decay eagerly queried level");
        inactive.Event("r"+std::to_string(Index(field)));return inactive.state.values[Index(field)];
    };
    const auto stopped=QuestVr::UpdateOriginalScriptedPawnFear(actor,1.0f,q);
    Require(stopped.steps==2u&&!stopped.floatWrites,"Nonpositive fear decay demanded stores/level");
    Fixture clear;clear.state.values[2u]=2.0f;clear.state.changed=false;q=clear.Query();
    q.writeFloat={};q.writeAlliancesChanged={};q.allianceNamed={};q.alliancePermanent={};
    q.allianceAgitation={};q.writeAllianceAgitation={};
    const auto absent=QuestVr::UpdateOriginalScriptedPawnAgitation(actor,1.0f,q);
    Require(absent.steps==4u&&clear.events==std::vector<std::string>{"r0","r1","r2","flag"},
        "Clear alliances flag demanded unreachable entry/store providers");
    Fixture zero;zero.state.values[1u]=2.0f;q=zero.Query();q.allianceNamed={};q.writeAlliancesChanged={};
    q.readFloat=[&](Id a,Field field) {
        Require(a==actor&&field!=Field::AgitationDecayRate,"Sustained agitation eagerly queried decay rate");
        zero.Event("r"+std::to_string(Index(field)));return zero.state.values[Index(field)];
    };
    const auto noDecay=QuestVr::UpdateOriginalScriptedPawnAgitation(actor,1.0f,q);
    Require(!noDecay.entriesVisited&&noDecay.flagReads==1u&&zero.events.back()=="flag"&&zero.state.values[1u]==1.0f,
        "Sustained agitation skipped the native flag read or entered entries with zero decay");
    Fixture noNamed;noNamed.state.values[2u]=1.0f;q=noNamed.Query();q.alliancePermanent={};
    q.allianceAgitation={};q.writeAllianceAgitation={};
    const auto empty=QuestVr::UpdateOriginalScriptedPawnAgitation(actor,1.0f,q);
    Require(empty.entriesVisited==16u&&!empty.permanentReads&&!empty.agitationReads&&!noNamed.state.changed,
        "None AllianceName entries were treated as Actor pointers or demanded later entry fields");
}
void FreshQueriesAndRetainedValues() {
    Fixture f;f.state.values[4u]=2.0f;unsigned timerReads{},levelReads{};
    f.onEvent=[&](const std::string& event) {
        if(event=="r3") {
            const std::array<float,3u> reads{2.0f,0.25f,0.75f};
            f.state.values[3u]=reads.at(timerReads++);
        }
        if(event=="r4")Require(f.state.values[3u]==0.0f,"Fear rate was read before the timer-zero store");
        if(event=="r5")f.state.values[5u]=levelReads++?3.0f:10.0f;
    };
    f.Run(true,1.0f);
    Require(timerReads==3u&&levelReads==2u&&f.state.values[5u]==2.5f,
        "Fear reused captured timer/level instead of the original fresh memory loads");
    Fixture agitation;agitation.state.values[2u]=0.5f;agitation.state.alliances[15u]={true,false,1.0f};
    agitation.onEvent=[&](const std::string& event) {
        if(event=="flag=1")agitation.state.alliances[15u].agitation=4.0f;
    };
    agitation.Run(false,1.0f);
    Require(agitation.state.alliances[15u].agitation==3.5f,"Alliance agitation was cached before the flag write");
    // The native retained decay is 1-2^-25. A Float temporary rounds to1,
    // incorrectly losing the positive2^-25 remainder in both actual helpers.
    for(bool fear:{false,true}) {
        Fixture retained;
        retained.state.values[fear?3u:1u]=0x1p-25f;
        retained.state.values[fear?4u:2u]=1.0f;
        retained.state.values[5u]=1.0f;retained.state.alliances[0u]={true,false,1.0f};
        retained.Run(fear,1.0f);
        Require(Bits(fear?retained.state.values[5u]:retained.state.alliances[0u].agitation)==Bits(0x1p-25f),
            "Native short-delay decay was spilled to Float before final subtraction");
    }
    Fixture unordered;unordered.state.values[0u]=1.0f;unordered.state.changed=false;
    unordered.Run(false,Float(0x7fc12345u));
    Require(Bits(unordered.state.values[0u])==0u&&unordered.stores.size()==2u&&
        unordered.stores[0u].second==0x7fc12345u&&unordered.stores[1u].second==0u,
        "Unordered retained check-timer result did not Float-store then C0-clamp to positive zero");
}
void RefusalsAndCallerRoot() {
    Fixture null;
    Refuses([&]{QuestVr::UpdateOriginalScriptedPawnFear(0u,1.0f,null.Query());},"Null fear Actor accepted");
    Refuses([&]{QuestVr::UpdateOriginalScriptedPawnAgitation(0u,1.0f,null.Query());},"Null agitation Actor accepted");
    Require(null.events.empty()&&!null.work,"Null Actor demanded provider/work authority");
    for(bool fear:{false,true}) {
        auto unbudgeted=Active(fear);auto q=unbudgeted.Query();q.work={};
        Refuses([&]{if(fear)QuestVr::UpdateOriginalScriptedPawnFear(actor,1.0f,q);
            else QuestVr::UpdateOriginalScriptedPawnAgitation(actor,1.0f,q);},"Unbudgeted native decay accepted");
        Require(unbudgeted.events.empty()&&!unbudgeted.work&&!unbudgeted.effects,
            "Missing caller work authority ran a reached provider");
    }
    auto baseline=Active(false);baseline.Run(false,1.0f);
    for(unsigned provider=0u;provider<8u;++provider) {
        auto f=Active(false);auto q=f.Query();std::string first;
        switch(provider) {
        case 0u:q.readFloat={};first="r0";break;
        case 1u:q.writeFloat={};first="w0";break;
        case 2u:q.alliancesChanged={};first="flag";break;
        case 3u:q.writeAlliancesChanged={};first="flag=0";break;
        case 4u:q.allianceNamed={};first="n15";break;
        case 5u:q.alliancePermanent={};first="p15";break;
        case 6u:q.allianceAgitation={};first="a15";break;
        case 7u:q.writeAllianceAgitation={};first="s15";break;
        }
        const auto found=std::find(baseline.events.begin(),baseline.events.end(),first);
        Require(found!=baseline.events.end(),"Missing-provider test did not reach its provider");
        const auto prefix=static_cast<std::size_t>(found-baseline.events.begin());
        Refuses([&]{QuestVr::UpdateOriginalScriptedPawnAgitation(actor,1.0f,q);},"Missing reached decay provider accepted");
        Require(f.work==prefix&&f.events==std::vector<std::string>(baseline.events.begin(),found),
            "Missing provider was demanded eagerly, charged work or skipped a reached prior effect");
    }
    for(bool fear:{false,true}) {
        auto complete=Active(fear);complete.Run(fear,1.0f);
        for(std::size_t cut=0u;cut<complete.events.size();++cut) {
            auto f=Active(fear);f.limit=cut;
            Refuses([&]{f.Run(fear,1.0f);},"Shared caller work cutoff was swallowed");
            Require(f.work==cut+1u&&f.events==std::vector<std::string>(complete.events.begin(),complete.events.begin()+
                static_cast<std::ptrdiff_t>(cut)),"Work was not charged before the exact reached decay provider");
            // Explicit MOCK CALLER root: pure helper effects are not undone
            // locally; restoring the shared root is the actual host's job.
            auto root=Active(fear);const auto before=root.state;const auto beforeEffects=root.effects;
            root.limit=cut;bool failed{};
            try {root.Run(fear,1.0f);}catch(const std::exception&) {
                failed=true;root.state=before;root.effects=beforeEffects;root.stores.clear();
            }
            Require(failed&&Equal(root.state,before)&&root.effects==beforeEffects&&root.stores.empty(),
                "Mock caller rollback failed to restore decay/provider effects");
        }
        for(std::size_t cut=0u;cut<complete.events.size();++cut) {
            auto f=Active(fear);f.failProvider=cut;
            Refuses([&]{f.Run(fear,1.0f);},"Reached decay provider/store failure was swallowed");
            Require(f.work==cut+1u&&f.events==std::vector<std::string>(complete.events.begin(),complete.events.begin()+
                static_cast<std::ptrdiff_t>(cut+1u)),"Provider/store failure lost the exact shared-root prefix");
            auto root=Active(fear);const auto before=root.state;root.failProvider=cut;bool failed{};
            try {root.Run(fear,1.0f);}catch(const std::exception&) {
                failed=true;root.state=before;root.effects=0u;root.stores.clear();
            }
            Require(failed&&Equal(root.state,before)&&!root.effects&&root.stores.empty(),
                "Mock caller root failed to roll back a late provider/store refusal");
        }
    }
    auto local=Active(false);local.limit=4u;const auto before=local.state;
    Refuses([&]{local.Run(false,1.0f);},"Late work failure accepted");
    Require(!Equal(local.state,before)&&local.state.values[0u]==0.0f&&local.effects==4u&&local.stores.size()==2u,
        "Helper invented a nested rollback instead of leaving earlier stores to the caller root");
}

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
// This oracle executes our own tiny x87 operations, never retail executable
// code. Explicit memory instructions preserve the original m32/m64 operands
// (including raw signaling NaNs), PC64 registers and named Float stores.
namespace X87 {
using Real=long double;
struct Precision {
    unsigned short saved{};
    Precision() {
        static_assert(std::numeric_limits<Real>::digits==64,"Decay oracle requires actual GNU x87 PC64");
        asm volatile("fnstcw %0":"=m"(saved));
        const auto selected=static_cast<unsigned short>((saved&~0x0f00u)|0x033fu);
        asm volatile("fnclex; fldcw %0"::"m"(selected):"memory");
    }
    ~Precision(){asm volatile("fnclex; fldcw %0"::"m"(saved):"memory");}
};
Real Load(float value) {
    Real out;asm volatile("flds %1; fstpt %0":"=m"(out):"m"(value):"st","memory");return out;
}
Real SubtractMemory(Real value,float memory) {
    Real out;asm volatile("fldt %1; fsubs %2; fstpt %0":"=m"(out):"m"(value),"m"(memory):"st","memory");return out;
}
Real ReverseMemory(Real value,float memory) {
    Real out;asm volatile("fldt %1; fsubrs %2; fstpt %0":"=m"(out):"m"(value),"m"(memory):"st","memory");return out;
}
Real Fraction(Real value) {
    const double one=1.0;Real out;
    asm volatile("fldt %1; fsubrl %2; fstpt %0":"=m"(out):"m"(value),"m"(one):"st","memory");return out;
}
Real DivideMemory(Real value,float memory) {
    Real out;asm volatile("fldt %1; fdivs %2; fstpt %0":"=m"(out):"m"(value),"m"(memory):"st","memory");return out;
}
Real MultiplyMemory(Real value,float memory) {
    Real out;asm volatile("fldt %1; fmuls %2; fstpt %0":"=m"(out):"m"(value),"m"(memory):"st","memory");return out;
}
Real MultiplyRegisters(Real top,Real below) {
    Real out;asm volatile("fldt %2; fldt %1; fmulp %%st,%%st(1); fstpt %0":
        "=m"(out):"m"(top),"m"(below):"st","st(1)","memory");return out;
}
Real SubtractRegisters(Real current,Real decay) {
    Real out;asm volatile("fldt %2; fldt %1; fsub %%st(1),%%st; fstpt %0; fstp %%st(0)":
        "=m"(out):"m"(current),"m"(decay):"st","st(1)","memory");return out;
}
float Store(Real value) {
    float out;asm volatile("fldt %1; fstps %0":"=m"(out):"m"(value):"st","memory");return out;
}
unsigned short Status(Real value,float memory) {
    unsigned short status;
    asm volatile("fldt %1; fcomps %2; fnstsw %%ax":"=a"(status):"m"(value),"m"(memory):"st","memory");return status;
}
bool Positive(Real value) {return !(Status(value,0.0f)&0x4100u);}
bool C0(Real value,float memory) {return (Status(value,memory)&0x0100u)!=0u;}
} // namespace X87
struct Oracle {
    State state;
    Stores stores;
    void Write(std::size_t field,float value) {
        state.values[field]=value;stores.emplace_back(static_cast<std::uint32_t>(field),Bits(value));
    }
    void WriteAlliance(std::size_t index,float value) {
        state.alliances[index].agitation=value;stores.emplace_back(static_cast<std::uint32_t>(16u+index),Bits(value));
    }
    void Run(bool fear,float delta) {
        using namespace X87;
        if(fear) {
            Real decay;
            if(Positive(Load(state.values[3u]))) {
                const bool shorter=C0(Load(state.values[3u]),delta);
                const auto timer=Load(state.values[3u]);
                if(!shorter) {Write(3u,Store(SubtractMemory(timer,delta)));return;}
                const auto ratio=DivideMemory(timer,delta);Write(3u,0.0f);
                const auto fraction=Fraction(ratio);
                decay=MultiplyRegisters(MultiplyMemory(Load(delta),state.values[4u]),fraction);
            } else decay=MultiplyMemory(Load(delta),state.values[4u]);
            if(!Positive(decay)||!Positive(Load(state.values[5u])))return;
            const auto remaining=ReverseMemory(decay,state.values[5u]);
            Write(5u,Store(remaining));if(C0(remaining,0.0f))Write(5u,0.0f);
            return;
        }
        if(Positive(Load(state.values[0u]))) {
            const auto remaining=SubtractMemory(Load(state.values[0u]),delta);
            Write(0u,Store(remaining));if(C0(remaining,0.0f))Write(0u,0.0f);
        }
        auto decay=Load(0.0f);
        if(Positive(Load(state.values[1u]))) {
            if(C0(Load(state.values[1u]),delta)) {
                const auto ratio=DivideMemory(Load(state.values[1u]),delta);Write(1u,0.0f);
                const auto fraction=Fraction(ratio);
                decay=MultiplyRegisters(MultiplyMemory(Load(delta),state.values[2u]),fraction);
            } else Write(1u,Store(SubtractMemory(Load(state.values[1u]),delta)));
        } else decay=MultiplyMemory(Load(delta),state.values[2u]);
        if(!state.changed||!Positive(decay))return;
        state.changed=false;
        for(std::size_t index=16u;index--;) {
            auto& entry=state.alliances[index];
            if(!entry.named||entry.permanent||!Positive(Load(entry.agitation)))continue;
            state.changed=true;
            const auto remaining=SubtractRegisters(Load(entry.agitation),decay);
            WriteAlliance(index,Store(remaining));if(C0(remaining,0.0f))WriteAlliance(index,0.0f);
        }
    }
};
void CompareOracle(const State& initial,float delta) {
    for(bool fear:{false,true}) {
        Fixture actual;actual.state=initial;Oracle oracle{initial,{}};
        oracle.Run(fear,delta);actual.Run(fear,delta);++oracleCases;
        if(!Equal(actual.state,oracle.state)||actual.stores!=oracle.stores) {
            std::cerr<<"Decay x87 mismatch: fear="<<fear<<" delta=0x"<<std::hex<<Bits(delta)
                <<" timer=0x"<<Bits(initial.values[fear?3u:1u])<<" rate=0x"<<Bits(initial.values[fear?4u:2u])
                <<std::dec<<" case="<<oracleCases<<'\n';
            throw std::runtime_error("Full native decay helper/Float-store trace differs from independent x87");
        }
        Require(actual.work==actual.events.size(),"Numeric path skipped custom caller work");
        ++checks;
    }
}
void ArithmeticOracle() {
    X87::Precision precision;
    const std::array<std::uint32_t,24u> words{0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,
        0x3f000000u,0x3f7fffffu,0x3f800000u,0x3f800001u,0x40000000u,0xbf800000u,
        0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00000u,0xffc12345u,
        0x7f800001u,0xff812345u,0x33000000u,0x33800000u,0x00800001u,0x4b800001u};
    for(std::size_t i=0u;i<words.size();++i)for(std::size_t j=0u;j<words.size();++j)
        for(std::size_t k=0u;k<words.size();++k) {
            State state;state.values={Float(words[(i+1u)%words.size()]),Float(words[i]),Float(words[j]),
                Float(words[i]),Float(words[j]),Float(words[(k+7u)%words.size()])};
            state.changed=((i+j+k)%5u)!=0u;
            for(std::size_t n=0u;n<16u;++n)state.alliances[n]={n%3u!=0u,n%7u==0u,Float(words[(k+n)%words.size()])};
            CompareOracle(state,Float(words[k]));
        }
    std::uint32_t seed=0x831d9e30u;
    const auto random=[&]() {seed^=seed<<13u;seed^=seed>>17u;seed^=seed<<5u;return seed;};
    for(unsigned i=0u;i<4096u;++i) {
        State state;for(auto& field:state.values)field=Float(random());
        state.changed=(random()&1u)!=0u;
        for(auto& entry:state.alliances) {
            const auto flags=random();entry={bool(flags&1u),bool(flags&2u),Float(random())};
        }
        CompareOracle(state,Float(random()));
    }
}
#else
void ArithmeticOracle() {throw std::runtime_error("Decay tests require GNU x87 PC64 independent oracle; no skipped acceptance");}
#endif
} // namespace
int main() {
    try {
        ExactOrderAndLazyPaths();FreshQueriesAndRetainedValues();RefusalsAndCallerRoot();ArithmeticOracle();
        std::cout<<"Original ScriptedPawn decay: "<<checks<<" checks / "<<refusals<<" refusals / "<<oracleCases
            <<" independent x87 full-helper cases; helper/provider foundation only, NOT complete NPC Tick or first Training acceptance\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
