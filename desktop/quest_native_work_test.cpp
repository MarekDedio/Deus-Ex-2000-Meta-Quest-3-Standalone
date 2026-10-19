#include "quest_native_work.h"
#include "quest_portable_vm.h"
#include "quest_state_frame.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Generated CPU-only controls. No original package, map or executable is
// opened. Work units are independently predicted, not measured in bytes or
// inferred from the VM's instruction/write counts.
namespace NativeWorkAllocationControl {
thread_local bool denied{};
thread_local std::size_t attempts{};
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void* Allocate(std::size_t size) {
    if (denied) {++attempts;throw std::bad_alloc();}
    if (auto* memory=std::malloc(size?size:1u)) return memory;
    throw std::bad_alloc();
}
}
void* operator new(std::size_t size) {return NativeWorkAllocationControl::Allocate(size);}
void* operator new[](std::size_t size) {return NativeWorkAllocationControl::Allocate(size);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* memory) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* memory,std::size_t) noexcept {std::free(memory);}

namespace {
namespace Vm=QuestVr::Vm;
namespace Work=QuestVr::NativeWork;
using Bytes=std::vector<std::uint8_t>;
using Value=Vm::Value;
std::size_t checks{},ledgerCases{},vmCases{},refusals{};
void Require(bool condition,const std::string& message) {
    ++checks;if (!condition) throw std::runtime_error(message);
}
template<class Operation> void Invalid(Operation operation) {
    bool rejected{};
    try {operation();} catch (const std::invalid_argument&) {rejected=true;}
    Require(rejected,"Invalid native-work configuration was not rejected with invalid_argument");++refusals;
}
template<class Operation> void Exhaust(Operation operation,const Work::Usage& before,std::size_t requested) {
    bool rejected{};
    try {operation();} catch (const Work::Exhausted& failure) {
        rejected=true;
        Require(failure.Used()==before.used&&failure.Limit()==before.limit&&failure.Requested()==requested,
            "Typed exhaustion lost used/limit/requested accounting");
    }
    Require(rejected,"Expected sticky native-work exhaustion was not typed Exhausted");++refusals;
}

void Ledger() {
    static_assert(Work::DefaultLimit==65'536u&&Work::MaximumLimit==16'777'216u);
    Work::Budget defaultBudget;
    Require(defaultBudget.Read()==Work::Usage{0u,65'536u,false},"Unchanged default native-work limit is not65536");
    for (const std::size_t limit:{0u,1u,2u,31u,65'536u,65'537u,16'777'216u}) {
        Work::Budget budget(limit);
        budget.Step(0u);budget.Check();
        Require(budget.Read()==Work::Usage{0u,limit,false},"Zero charge consumed work or exhausted a zero root");
        budget.Step(limit);budget.Step(0u);budget.Check();
        Require(budget.Read()==Work::Usage{limit,limit,false},"Exact bulk charge did not admit its final work unit");
        Exhaust([&]{budget.Step();},budget.Read(),1u);
        Require(budget.Read()==Work::Usage{limit,limit,true},"Failed charge advanced or wrapped used work");
        for (const auto requested:{std::size_t{0u},std::size_t{1u},std::numeric_limits<std::size_t>::max()}) {
            const auto before=budget.Read();Exhaust([&]{budget.Step(requested);},before,requested);
            Require(budget.Read()==before,"A charge after exhaustion mutated the exhausted ledger");
        }
        const auto exhausted=budget.Read();Exhaust([&]{budget.Check();},exhausted,0u);
        Require(budget.Read()==exhausted,"Sticky Check mutated accounting");
        for (const auto invalid:{Work::MaximumLimit+1u,std::numeric_limits<std::size_t>::max()}) {
            Invalid([&]{budget.Reset(invalid);});
            Require(budget.Read()==exhausted,"Invalid Reset erased usage/exhaustion before validating the limit");
        }
        budget.Reset(limit);Require(budget.Read()==Work::Usage{0u,limit,false},"Valid owner Reset failed to begin a fresh root");
        ++ledgerCases;
    }
    for (const auto invalid:{Work::MaximumLimit+1u,std::numeric_limits<std::size_t>::max()})
        Invalid([&]{Work::Budget budget(invalid);static_cast<void>(budget);});

    // Deliberately different predictor: a remaining-unit counter performs no
    // production subtraction/addition or Budget calls. Once a request is not
    // affordable, every later request (including0) must fail without changes.
    for (std::size_t limit=0u;limit<=48u;++limit) for (std::size_t first=0u;first<=52u;++first) {
        Work::Budget budget(limit);std::size_t remaining=limit;bool sticky{};
        const std::array requests{first,std::size_t{0u},std::size_t{1u},limit/2u,
            std::numeric_limits<std::size_t>::max(),std::size_t{0u},std::size_t{2u}};
        for (const auto requested:requests) {
            const bool fits=!sticky&&requested<=remaining;
            if (fits) {budget.Step(requested);remaining-=requested;}
            else {const auto before=budget.Read();Exhaust([&]{budget.Step(requested);},before,requested);sticky=true;}
            Require(budget.Read()==Work::Usage{limit-remaining,limit,sticky},"Independent remaining-unit ledger differs");
        }
        ++ledgerCases;
    }
    {
        Work::Budget budget(16'777'216u);
        NativeWorkAllocationControl::attempts=0u;NativeWorkAllocationControl::denied=true;
        try {
            budget.Step(0u);budget.Step(1u);budget.Step(1'234'567u);budget.Step(15'542'648u);
            budget.Check();static_cast<void>(budget.Read());
        } catch (...) {NativeWorkAllocationControl::denied=false;throw;}
        NativeWorkAllocationControl::denied=false;
        Require(NativeWorkAllocationControl::attempts==0u&&budget.Read()==Work::Usage{16'777'216u,16'777'216u,false},
            "Successful native-work charge/read/check allocated or charged bytes instead of units");
        ++ledgerCases;
    }
}

void U16(Bytes& bytes,std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));bytes.push_back(static_cast<std::uint8_t>(value>>8u));
}
void U32(Bytes& bytes,std::uint32_t value) {
    for (unsigned i=0u;i<4u;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8u*i)));
}
void Append(Bytes& bytes,const Bytes& suffix) {bytes.insert(bytes.end(),suffix.begin(),suffix.end());}
Bytes Join(std::initializer_list<Bytes> pieces) {Bytes out;for (const auto& piece:pieces) Append(out,piece);return out;}
Bytes Integer(std::int32_t value) {Bytes out{0x1du};U32(out,static_cast<std::uint32_t>(value));return out;}
Bytes Variable() {Bytes out{0x01u};U32(out,1u);return out;}
Bytes Let(std::int32_t value) {return Join({{0x0fu},Variable(),Integer(value)});}
Bytes Return(std::int32_t value=9) {return Join({{0x04u},Integer(value)});}
Bytes Native(std::uint16_t index,std::initializer_list<Bytes> arguments={}) {
    Bytes out;
    if (index>=0x70u&&index<=0xffu) out.push_back(static_cast<std::uint8_t>(index));
    else {out.push_back(static_cast<std::uint8_t>(0x60u+(index>>8u)));out.push_back(static_cast<std::uint8_t>(index));}
    for (const auto& argument:arguments) Append(out,argument);
    out.push_back(0x16u);return out;
}
Vm::Function Script(std::string name,Bytes code) {
    Vm::Function out;out.path="Generated.NativeWork."+name;out.source="Generated.NativeWorkSource";out.bytecode=std::move(code);return out;
}
Bytes IteratorLoop(Bytes body) {
    Bytes code{0x2fu};Append(code,Native(1040u));const auto target=code.size();U16(code,0u);
    Append(code,body);code.push_back(0x31u);const auto pop=code.size();
    Require(pop<=65'535u,"Generated iterator target overflow");
    code[target]=static_cast<std::uint8_t>(pop);code[target+1u]=static_cast<std::uint8_t>(pop>>8u);
    code.push_back(0x30u);Append(code,Return());return code;
}

// This host intentionally implements ONLY legacy Begin(). The new default
// BeginRoot(const Limits&) must dispatch to it for every public root route.
struct LegacyHost:Vm::Host {
    std::size_t begins{},commits{},rollbacks{},effects{},savedEffects{},polls{},nativeCalls{},eventResolutions{};
    std::size_t iteratorCreates{},iteratorNext{},iteratorDestroys{},iteratorItems{2u},caught{};
    std::size_t pollCost{},bridgeCost{1u},factoryCost{1u},nextCost{1u};
    std::int32_t marker{7},savedMarker{};
    bool active{},catchInner{},writeExternal{},plainFailure{},commitFailure{};
    std::shared_ptr<std::int32_t> external=std::make_shared<std::int32_t>(13);
    QuestVr::StateObject state,savedState;
    std::shared_ptr<const Vm::Function> stateProgram,callback;
    Vm::Property markerProperty{"Generated.Marker","Marker",Value::Integer(0),0u,1u};
    virtual void Charge(std::size_t) {}
    void Begin() override {
        Require(!active,"Generated native-work host nested a root");
        ++begins;active=true;savedMarker=marker;savedEffects=effects;savedState=state;
    }
    void Commit() override {
        Require(active,"Generated native-work host committed an absent root");
        if (commitFailure) throw std::runtime_error("Generated commit failure");
        ++commits;active=false;
    }
    void Rollback() noexcept override {
        marker=savedMarker;effects=savedEffects;state=std::move(savedState);active=false;++rollbacks;
    }
    Vm::Property ResolveProperty(const Vm::Function&,std::int32_t index) override {
        Require(index==1,"Generated fixture resolved an unknown property");return markerProperty;
    }
    std::string ResolveName(const Vm::Function&,std::int32_t) override {throw std::runtime_error("Unexpected name resolution");}
    std::string ResolveObject(const Vm::Function&,std::int32_t) override {throw std::runtime_error("Unexpected object resolution");}
    std::shared_ptr<const Vm::Function> ResolveFunction(const Vm::Function&,const std::string&,const Vm::Invocation&) override {
        throw std::runtime_error("Unexpected generated function resolution");
    }
    std::shared_ptr<Vm::Reference> Variable(const std::string& receiver,const Vm::Property& property,Vm::Scope scope) override {
        Require(receiver=="Self"&&property.key==markerProperty.key&&scope==Vm::Scope::Instance,
            "Generated native-work property lost its exact receiver/schema");
        auto reference=std::make_shared<Vm::Reference>();reference->zero=Value::Integer(0);
        reference->read=[this]{return Value::Integer(marker);};
        reference->write=[this](const Value& value){marker=Vm::ToInt(value);};return reference;
    }
    std::shared_ptr<Vm::Reference> ExternalReference() {
        auto reference=std::make_shared<Vm::Reference>();reference->zero=Value::Integer(0);
        reference->read=[storage=external]{return Value::Integer(*storage);};
        reference->write=[storage=external](const Value& value){*storage=Vm::ToInt(value);};return reference;
    }
    Vm::Evaluation Native(std::uint16_t index,const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*) override {
        Require(index==1000u&&arguments.size()==1u,"Unexpected generated native work call");++nativeCalls;
        const auto cost=Vm::ToInt(arguments[0].Load());Require(cost>=0,"Generated work cost is negative");
        if (plainFailure) throw std::runtime_error("Native actor lifecycle work budget exceeded");
        Charge(static_cast<std::size_t>(cost));++effects;return {Value::Integer(cost),{}};
    }
    Vm::Evaluation NativeWithExecution(std::uint16_t index,const std::string& receiver,
        const std::vector<Vm::Evaluation>& arguments,const Vm::Function* declaration,Vm::Execution& execution) override {
        if (index!=1001u) return Native(index,receiver,arguments,declaration);
        Require(arguments.empty(),"Generated callback bridge acquired unexpected arguments");
        Charge(bridgeCost);++effects;
        if (writeExternal) execution.WriteReference(ExternalReference(),Value::Integer(55));
        try {return {execution.CallEvent(receiver,"Nested",true),{}};}
        catch (const Work::Exhausted&) {if (!catchInner) throw;++caught;return {Value::Integer(-9),{}};}
    }
    std::shared_ptr<const Vm::Function> ResolveEvent(const std::string& receiver,const std::string& name,bool enumDispatch) override {
        Require(receiver=="Self"&&name=="Nested"&&enumDispatch,"Generated callback lost real dispatch arguments");
        ++eventResolutions;return callback;
    }
    struct Cursor final:Vm::Iterator {
        LegacyHost& owner;std::size_t index{};
        explicit Cursor(LegacyHost& host):owner(host) {}
        ~Cursor() override {++owner.iteratorDestroys;}
        bool Next() override {
            ++owner.iteratorNext;owner.Charge(owner.nextCost);
            return index++<owner.iteratorItems;
        }
        std::size_t RetainedBytes() const override {return sizeof(Cursor);}
    };
    std::unique_ptr<Vm::Iterator> CreateIterator(std::uint16_t index,const std::string& receiver,
        const std::vector<Vm::Evaluation>& arguments,const Vm::Function*) override {
        Require(index==1040u&&receiver=="Self"&&arguments.empty(),"Generated iterator factory changed arguments");
        Charge(factoryCost);++iteratorCreates;return std::make_unique<Cursor>(*this);
    }
    void SetState(Bytes code) {
        stateProgram=std::make_shared<Vm::Function>(Script("State",std::move(code)));
        state={};state.hasStack=true;state.frameOverride=true;state.frame.emplace();
        state.frame->codePath=stateProgram->path;state.frame->localsCodePath=stateProgram->path;
    }
    const QuestVr::StateObject* ReadState(const std::string&) override {return &state;}
    QuestVr::StateObject* MutableState(const std::string&) override {return &state;}
    std::shared_ptr<const Vm::Function> StateProgram(const std::string& receiver,const std::string& path) override {
        Require(receiver=="Self"&&stateProgram&&path==stateProgram->path,"Generated state continuation lost its identity");return stateProgram;
    }
    bool PollState(const std::string& receiver,float elapsed,Vm::Execution&) override {
        Require(receiver=="Self"&&elapsed==0.125f,"Generated state poll changed elapsed/receiver");
        ++polls;Charge(pollCost);return true;
    }
};
struct WorkHost final:LegacyHost {
    Work::Budget budget;
    Vm::Limits delivered{};
    std::size_t resets{};
    mutable std::size_t usageReads{};
    mutable bool observedTerminal{};
    void BeginRoot(const Vm::Limits& limits) override {
        delivered=limits;budget.Reset(limits.nativeWork);++resets;LegacyHost::Begin();
    }
    void Charge(std::size_t count) override {budget.Step(count);}
    void Commit() override {budget.Check();LegacyHost::Commit();}
    std::optional<Work::Usage> ReadNativeWork() const noexcept override {
        ++usageReads;if (!active&&(commits||rollbacks)) observedTerminal=true;return budget.Read();
    }
};
enum class Route {Execute,Transaction,Resume,Advance};
constexpr std::array Routes{Route::Execute,Route::Transaction,Route::Resume,Route::Advance};
Vm::Result Run(LegacyHost& host,Route route,std::int32_t cost,const Vm::Limits& limits={}) {
    const auto call=Native(1000u,{Integer(cost)});
    if (route==Route::Execute) return Vm::Execute(host,Script("Entry",Join({call,Return()})),"Self",{},limits);
    if (route==Route::Transaction) {
        const auto function=Script("Entry",Join({call,Return()}));
        return Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){return execution.CallFunction(function,"Self");},limits);
    }
    host.SetState(Join({call,{0x08u}}));
    return route==Route::Resume?Vm::ResumeState(host,"Self",limits):Vm::AdvanceState(host,"Self",0.125f,limits);
}
void Passed(const LegacyHost& host,const Vm::Result& result) {
    Require(result.passed()&&result.committed&&result.error.empty()&&host.begins==1u&&host.commits==1u&&!host.rollbacks&&!host.active,
        "Generated native-work root did not commit: "+result.error);
}
void Failed(const LegacyHost& host,const Vm::Result& result,Vm::Status expected=Vm::Status::Budget) {
    Require(result.status==expected&&!result.committed&&!result.passed()&&!result.error.empty()&&host.begins==1u&&
        !host.commits&&host.rollbacks==1u&&!host.active,"Generated native-work root did not roll back with the expected classification: "+result.error);
    Require(result.value.kind==Vm::Kind::Nothing,"Failed native-work root leaked a returned value");++refusals;
}
void Usage(const WorkHost& host,const Vm::Result& result,std::size_t used,std::size_t limit,bool exhausted) {
    Require(result.nativeWork&&*result.nativeWork==Work::Usage{used,limit,exhausted},"Final VM result lost native used/limit/exhausted");
    Require(host.budget.Read()==*result.nativeWork&&host.observedTerminal&&host.usageReads==1u,
        "VM did not capture the preserved owner ledger after commit/rollback");
}

void RootRoutes() {
    Require(Vm::Limits{}.nativeWork==65'536u,"VM ordinary-root default allowance changed");
    for (const auto route:Routes) {
        LegacyHost legacy;auto legacyLimits=Vm::Limits{};legacyLimits.nativeWork=0u;
        const auto old=Run(legacy,route,3,legacyLimits);Passed(legacy,old);
        Require(!old.nativeWork&&legacy.effects==1u,"Legacy Begin compatibility invented native-work ownership/accounting");++vmCases;
        for (const auto limit:{std::size_t{0u},std::size_t{3u},std::size_t{65'536u},std::size_t{65'537u},Work::MaximumLimit}) {
            WorkHost host;auto limits=Vm::Limits{};limits.nativeWork=limit;limits.operations=73u;
            const auto cost=limit==0u?0:static_cast<std::int32_t>(limit);
            const auto result=Run(host,route,cost,limits);Passed(host,result);Usage(host,result,limit,limit,false);
            Require(host.resets==1u&&host.delivered.nativeWork==limit&&host.delivered.operations==73u&&host.effects==1u&&
                host.polls==(route==Route::Advance?1u:0u),"One of four root routes did not deliver the caller's actual Limits exactly once");++vmCases;
        }
        for (const auto invalid:{Work::MaximumLimit+1u,std::numeric_limits<std::size_t>::max()}) {
            WorkHost host;auto limits=Vm::Limits{};limits.nativeWork=invalid;
            const auto result=Run(host,route,1,limits);
            Require(result.status==Vm::Status::Invalid&&!result.committed&&!result.passed()&&!result.error.empty()&&
                !host.begins&&!host.commits&&!host.rollbacks&&!host.resets&&!host.active&&!host.effects&&!host.polls&&!host.nativeCalls&&
                !host.usageReads&&!result.nativeWork,
                "Invalid native-work configuration reached Begin/state polling/script/native effects");++vmCases;++refusals;
        }
        WorkHost exact;auto limits=Vm::Limits{};limits.nativeWork=2u;
        const auto exhausted=Run(exact,route,3,limits);Failed(exact,exhausted);Usage(exact,exhausted,0u,2u,true);
        Require(exact.resets==1u&&exact.effects==0u,"Root work refusal mutated effect state or reset nested work");++vmCases;
    }
    // Explicit larger finite opt-in succeeds; no implicit global default bump.
    WorkHost ordinary;const auto limited=Run(ordinary,Route::Execute,65'537);Failed(ordinary,limited);Usage(ordinary,limited,0u,65'536u,true);
    WorkHost opted;auto raised=Vm::Limits{};raised.nativeWork=65'537u;
    const auto admitted=Run(opted,Route::Execute,65'537,raised);Passed(opted,admitted);Usage(opted,admitted,65'537u,65'537u,false);vmCases+=2u;
    // A zero remaining native budget is not an exhausted ledger until another
    // positive unit is requested, and does not consume script instructions.
    WorkHost empty;auto zero=Vm::Limits{};zero.nativeWork=0u;
    const auto noWork=Vm::Execute(empty,Script("NoNative",Return(71)),"Self",{},zero);Passed(empty,noWork);Usage(empty,noWork,0u,0u,false);
    Require(noWork.value.integer==71&&noWork.instructions>0u&&!empty.nativeCalls,"Native work budget was substituted for VM instructions");++vmCases;
}

void NestedAndIterator() {
    for (const bool iterator:{false,true}) for (const bool tight:{false,true}) {
        WorkHost host;host.callback=std::make_shared<Vm::Function>(Script("Nested",Join({Native(1000u,{Integer(2)}),Return()})));
        const auto body=Native(1001u);
        const auto function=Script(iterator?"IteratorRoot":"NestedRoot",iterator?IteratorLoop(body):Join({Native(1000u,{Integer(2)}),body,Return()}));
        // Non-iterator:2 direct +1 bridge +2 event =5. Iterator:1 factory,
        // three Next calls (including exhaustion), two (1 bridge+2 event) =10.
        const std::size_t full=iterator?10u:5u;auto limits=Vm::Limits{};limits.nativeWork=full-(tight?1u:0u);
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        if (!tight) {
            Passed(host,result);Usage(host,result,full,full,false);
            Require(host.effects==(iterator?4u:3u)&&host.eventResolutions==(iterator?2u:1u),"Nested callback work/effects did not actually run");
        } else {
            Failed(host,result);Usage(host,result,iterator?9u:3u,full-1u,true);
            Require(host.effects==0u,"Later nested/iterator work refusal kept earlier effects");
            if (!iterator) Require(result.function==host.callback->path&&result.offset==0u&&result.opcode==0x63u,
                "Nested native exhaustion lost the actual callback source PC/opcode");
        }
        Require(host.begins==1u&&host.resets==1u,"Nested function/callback/iterator opened a fresh native-work root");
        Require(host.iteratorCreates==(iterator?1u:0u)&&host.iteratorDestroys==host.iteratorCreates&&
            host.iteratorNext==(iterator?3u:0u),"Iterator work failure leaked a cursor or skipped a Next charge");++vmCases;
    }
    // Poll and state bytecode consume one owner allowance, not two budgets.
    for (const auto limit:{std::size_t{4u},std::size_t{5u}}) {
        WorkHost host;host.pollCost=2u;auto limits=Vm::Limits{};limits.nativeWork=limit;
        const auto result=Run(host,Route::Advance,3,limits);
        if (limit==5u) {Passed(host,result);Usage(host,result,5u,5u,false);}
        else {Failed(host,result);Usage(host,result,2u,4u,true);Require(host.effects==0u&&host.state.frame->statementIndex==0u,
            "State work exhaustion did not restore the original continuation");}
        Require(host.polls==1u&&host.resets==1u,"State poll/resume reset or omitted native work");++vmCases;
    }
}

void RollbackAndLocations() {
    for (const bool caught:{false,true}) for (const bool transaction:{false,true}) {
        WorkHost host;host.catchInner=caught;host.writeExternal=true;
        const auto prefix=Let(202);const auto failedOffset=prefix.size();
        host.callback=std::make_shared<Vm::Function>(Script("FailingCallback",Join({prefix,Native(1000u,{Integer(4)}),Return()})));
        const auto function=Script("PriorWrite",Join({Let(101),Native(1001u),Return(33)}));
        auto limits=Vm::Limits{};limits.nativeWork=3u;Vm::Result result;
        if (transaction) result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            try {return execution.CallFunction(function,"Self");}
            catch (const Work::Exhausted&) {++host.caught;return Value::Integer(88);}
        },limits);
        else result=Vm::Execute(host,function,"Self",{},limits);
        Failed(host,result);Usage(host,result,1u,3u,true);
        Require(host.marker==7&&*host.external==13&&host.effects==0u&&result.writes==3u,
            "Native exhaustion/caught inner failure failed to undo prior reflected and external journal writes");
        Require(result.function==host.callback->path&&result.offset==failedOffset&&result.opcode==0x63u&&
            !result.callStack.empty()&&result.callStack.back()==host.callback->path,
            "Caught inner native work failure was blamed on the later caller/coordinator PC");
        Require(host.caught==(caught?1u:0u)+(transaction?1u:0u)&&host.resets==1u,
            "Caught inner failure fixture did not catch the intended boundaries or reset shared work");++vmCases;
    }
    // Classification is by the dedicated exception type, not its message.
    WorkHost textOnly;textOnly.plainFailure=true;
    const auto unsupported=Run(textOnly,Route::Execute,1);Failed(textOnly,unsupported,Vm::Status::Unsupported);Usage(textOnly,unsupported,0u,65'536u,false);++vmCases;
    WorkHost late;late.commitFailure=true;const auto commitFailure=Run(late,Route::Execute,3);
    Failed(late,commitFailure,Vm::Status::Unsupported);Usage(late,commitFailure,3u,65'536u,false);
    Require(late.effects==0u,"Failed commit retained earlier charged native effects");++vmCases;
    // Final diagnostics must be captured AFTER rollback: a host may publish a
    // terminal usage snapshot there. Its work ledger is not world/save state.
    struct RollbackSnapshotHost final:LegacyHost {
        Work::Usage finalUsage{0u,5u,false};bool published{};mutable bool sampled{};
        void BeginRoot(const Vm::Limits& limits) override {finalUsage={0u,limits.nativeWork,false};LegacyHost::Begin();}
        void Charge(std::size_t count) override {finalUsage.used+=count;}
        void Rollback() noexcept override {LegacyHost::Rollback();published=true;finalUsage.exhausted=true;}
        std::optional<Work::Usage> ReadNativeWork() const noexcept override {
            sampled=published&&!active;return finalUsage;
        }
    } snapshot;
    snapshot.commitFailure=true;auto limits=Vm::Limits{};limits.nativeWork=5u;
    const auto result=Run(snapshot,Route::Execute,2,limits);Failed(snapshot,result,Vm::Status::Unsupported);
    Require(snapshot.sampled&&result.nativeWork&&*result.nativeWork==Work::Usage{2u,5u,true},
        "Result usage was sampled before terminal rollback publication");++vmCases;
}
}

int main() {
    try {
        Ledger();RootRoutes();NestedAndIterator();RollbackAndLocations();
        std::cout<<"PASS native work "<<checks<<" checks / "<<ledgerCases<<" independent ledger cases / "
            <<vmCases<<" VM roots / "<<refusals<<" refusals.\n";return 0;
    } catch (const std::exception& error) {
        NativeWorkAllocationControl::denied=false;
        std::cerr<<"Native work controls failed: "<<error.what()<<'\n';return 1;
    }
}
