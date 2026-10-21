#include "quest_object_destroy.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// Every native service is a mock. This checks the complete normal coordinator
// protocol against a separately written original-address PC oracle. It does NOT
// execute/load original binaries or claim runtime cleanup, Training, or physics.
namespace {
using Pointer=std::uintptr_t;
constexpr Pointer Source=10u,Aux=11u,Outer=12u,Other=13u,Class=20u,Linker=30u,NewLinker=31u;
std::size_t checks{},cases{},callbackCases{},providerFailures{},workFailures{},missingCases{};
void Require(bool ok,const std::string& message) {
    ++checks;if(!ok)throw std::runtime_error(message);
}
Pointer Word(std::int32_t value) {return static_cast<Pointer>(static_cast<std::uint32_t>(value));}
enum class Mode { Conditional,Actor,Object,SetLinker };
struct Task {Mode mode{Mode::Conditional};Pointer object{Source},newLinker{};std::int32_t newIndex{-1};};
enum class Op { Index,Flags,WriteFlags,Native,FullName,Log,Auxiliary,AuxConditional,ClearAuxiliary,Base,
    Class,ExitProperties,SetLinker,Outer,Linker,LinkerIndex,ExportObject,ClearExport,WriteLinker,WriteIndex };
struct Record {Op op{};Pointer a{},b{},c{};bool operator==(const Record&)const=default;};
struct Object {
    std::int32_t index{7},linkerIndex{2};
    std::uint32_t flags{};
    Pointer auxiliary{},outer{},linker{},actualClass{Class};
    std::string payload{"owned text"},sourcePath{"immutable package"};
    std::int32_t sourceExport{77};
    bool operator==(const Object&)const=default;
};
struct Pack {
    QuestVr::OriginalConditionalDestroyQueries conditional;
    QuestVr::OriginalActorDestroyQueries actor;
    QuestVr::OriginalObjectDestroyQueries object;
    QuestVr::OriginalSetLinkerQueries linker;
};
struct Context {
    std::map<Pointer,Object> objects{{Source,{}},{Aux,{}},{Outer,{}},{Other,{}}};
    std::map<std::pair<Pointer,std::int32_t>,Pointer> exports{{{Linker,2},Source}};
    std::vector<Record> trace;
    std::vector<std::string> logs;
    std::function<void(Context&,const Record&)> hook;
    std::optional<std::size_t> failProvider;
    std::size_t workUsed{},workLimit{std::numeric_limits<std::size_t>::max()};
    bool oracle{},integratedAux{},integratedBase{},nested{},noWork{};
    unsigned nativeKind{1u}; // 0 missing Super, 1 marker mock, 2 UObject, 3 Actor.
    std::int32_t auxResult{};
    unsigned mutations{};
    Pack q;

    void Work() {
        ++workUsed;if(workUsed>workLimit)throw std::runtime_error("borrowed root work exhausted");
    }
    void Enter(const Record& record) {
        trace.push_back(record);
        if(failProvider&&trace.size()==*failProvider)throw std::runtime_error("injected provider failure");
    }
    void After(const Record& record) {if(hook)hook(*this,record);}
    template<class Value> Value Read(Op op,Value value,Pointer a=0u,Pointer b=0u) {
        const Record record{op,a,b,0u};Enter(record);After(record);return value;
    }
    template<class Action> void Write(Op op,Pointer a,Pointer b,Pointer c,Action&& action) {
        const Record record{op,a,b,c};Enter(record);action();After(record);
    }
    void Bind() {
        const auto index=[this](Pointer a){return Read(Op::Index,objects.at(a).index,a);};
        const auto flags=[this](Pointer a){return Read(Op::Flags,objects.at(a).flags,a);};
        const auto writeFlags=[this](Pointer a,std::uint32_t f){
            Write(Op::WriteFlags,a,f,0u,[&]{objects.at(a).flags=f;});
        };
        const auto writeIndex=[this](Pointer a,std::int32_t i){
            Write(Op::WriteIndex,a,Word(i),0u,[&]{objects.at(a).linkerIndex=i;});
        };
        q.conditional.objectIndex=index;q.conditional.objectFlags=flags;q.conditional.writeObjectFlags=writeFlags;
        q.conditional.nativeDestroy=[this](Pointer a){
            Write(Op::Native,a,0u,0u,[&]{
                if(nativeKind==1u)objects.at(a).flags|=0x80000000u;
                else if(nativeKind==2u)Run({Mode::Object,a});
                else if(nativeKind==3u)Run({Mode::Actor,a});
            });
        };
        q.conditional.fullName=[this](Pointer a){return Read(Op::FullName,"Object"+std::to_string(a),a);};
        q.conditional.logMissingDebugDestroy=[this](const std::string& name){
            Write(Op::Log,0u,0u,0u,[&]{logs.push_back(name);});
        };
        q.actor.auxiliary=[this](Pointer a){return Read(Op::Auxiliary,objects.at(a).auxiliary,a);};
        q.actor.conditionalDestroy=[this](Pointer a){
            const Record record{Op::AuxConditional,a,0u,0u};Enter(record);
            const auto result=integratedAux?Run({Mode::Conditional,a}):auxResult;
            After(record);return result;
        };
        q.actor.clearAuxiliary=[this](Pointer a){
            Write(Op::ClearAuxiliary,a,0u,0u,[&]{objects.at(a).auxiliary=0u;});
        };
        q.actor.baseObjectDestroy=[this](Pointer a){
            Write(Op::Base,a,0u,0u,[&]{if(integratedBase)Run({Mode::Object,a});});
        };
        q.object.objectFlags=flags;q.object.writeObjectFlags=writeFlags;
        q.object.actualClass=[this](Pointer a){return Read(Op::Class,objects.at(a).actualClass,a);};
        q.object.exitProperties=[this](Pointer a,Pointer c){
            Write(Op::ExitProperties,a,c,0u,[&]{objects.at(a).payload.clear();});
        };
        q.object.setLinker=[this](Pointer a,Pointer l,std::int32_t i){
            Write(Op::SetLinker,a,l,Word(i),[&]{Run({Mode::SetLinker,a,l,i});});
        };
        q.object.outer=[this](Pointer a){return Read(Op::Outer,objects.at(a).outer,a);};
        q.object.objectIndex=index;q.object.writeLinkerIndex=writeIndex;
        q.linker.linker=[this](Pointer a){return Read(Op::Linker,objects.at(a).linker,a);};
        q.linker.linkerIndex=[this](Pointer a){return Read(Op::LinkerIndex,objects.at(a).linkerIndex,a);};
        q.linker.exportObject=[this](Pointer l,std::int32_t i){
            return Read(Op::ExportObject,exports.at({l,i}),l,Word(i));
        };
        q.linker.clearExportObject=[this](Pointer l,std::int32_t i){
            Write(Op::ClearExport,l,Word(i),0u,[&]{exports.at({l,i})=0u;});
        };
        q.linker.writeLinker=[this](Pointer a,Pointer l){
            Write(Op::WriteLinker,a,l,0u,[&]{objects.at(a).linker=l;});
        };
        q.linker.writeLinkerIndex=writeIndex;
        if(!noWork) {
            const auto work=[this]{Work();};q.conditional.work=work;q.actor.work=work;
            q.object.work=work;q.linker.work=work;
        }
    }

    // Provider mechanics only; the independent oracle below selects every PC,
    // operand, branch and call. No production coordinator/detail helper is used.
    template<class Query,class Function,class... Args>
    static auto O(const Query& query,const Function& function,Args&&... args) {
        if(!function)throw std::invalid_argument("oracle reached missing provider");
        if(query.work)query.work();
        return function(std::forward<Args>(args)...);
    }
    std::int32_t PcConditional(Pointer a) {
        if(!a)throw std::invalid_argument("oracle null object");
        std::uint32_t eax{};std::string name;unsigned pc=0x1014fdd6u;
        for(;;)switch(pc) {
        case 0x1014fdd6u:pc=O(q.conditional,q.conditional.objectIndex,a)==-1?0x1014fe8du:0x1014fde9u;break;
        case 0x1014fde9u:eax=O(q.conditional,q.conditional.objectFlags,a);
            pc=(eax&0x00800000u)?0x1014fe8du:0x1014fdf7u;break;
        case 0x1014fdf7u:eax&=0x7f7fffffu;eax|=0x00800000u;
            O(q.conditional,q.conditional.writeObjectFlags,a,eax);pc=0x1014fe04u;break;
        case 0x1014fe04u:O(q.conditional,q.conditional.nativeDestroy,a);pc=0x1014fe09u;break;
        case 0x1014fe09u:pc=(O(q.conditional,q.conditional.objectFlags,a)&0x80000000u)?
            0x1014fe30u:0x1014fe12u;break;
        case 0x1014fe12u:name=O(q.conditional,q.conditional.fullName,a);pc=0x1014fe1bu;break;
        case 0x1014fe1bu:O(q.conditional,q.conditional.logMissingDebugDestroy,name);pc=0x1014fe30u;break;
        case 0x1014fe30u:return 1;
        case 0x1014fe8du:return 0;
        default:throw std::logic_error("unknown ConditionalDestroy oracle PC");
        }
    }
    void PcActor(Pointer a) {
        if(!a)throw std::invalid_argument("oracle null actor");
        Pointer ecx{};unsigned pc=0x10368f8au;
        for(;;)switch(pc) {
        case 0x10368f8au:ecx=O(q.actor,q.actor.auxiliary,a);pc=ecx?0x10368f94u:0x10368fa4u;break;
        case 0x10368f94u:static_cast<void>(O(q.actor,q.actor.conditionalDestroy,ecx));pc=0x10368f9au;break;
        case 0x10368f9au:O(q.actor,q.actor.clearAuxiliary,a);pc=0x10368fa4u;break;
        case 0x10368fa4u:O(q.actor,q.actor.baseObjectDestroy,a);return;
        default:throw std::logic_error("unknown AActor.Destroy oracle PC");
        }
    }
    void PcLinker(const Task& task) {
        const auto a=task.object;if(!a)throw std::invalid_argument("oracle null object");
        Pointer ecx{};std::int32_t index{};unsigned pc=0x1014fb46u;
        for(;;)switch(pc) {
        case 0x1014fb46u:ecx=O(q.linker,q.linker.linker,a);pc=ecx?0x1014fb54u:0x1014fba4u;break;
        case 0x1014fb54u:index=O(q.linker,q.linker.linkerIndex,a);pc=0x1014fb64u;break;
        case 0x1014fb64u:if(!O(q.linker,q.linker.exportObject,ecx,index)) {
                throw std::runtime_error("oracle null old export");
            }
            pc=0x1014fb83u;break;
        case 0x1014fb83u:if(O(q.linker,q.linker.exportObject,ecx,index)!=a) {
                throw std::runtime_error("oracle wrong old export");
            }
            pc=0x1014fb9eu;break;
        case 0x1014fb9eu:O(q.linker,q.linker.clearExportObject,ecx,index);pc=0x1014fba4u;break;
        case 0x1014fba4u:O(q.linker,q.linker.writeLinker,a,task.newLinker);pc=0x1014fbb0u;break;
        case 0x1014fbb0u:O(q.linker,q.linker.writeLinkerIndex,a,task.newIndex);return;
        default:throw std::logic_error("unknown SetLinker oracle PC");
        }
    }
    void PcObject(Pointer a) {
        if(!a)throw std::invalid_argument("oracle null object");
        std::uint32_t eax{};Pointer c{},outer{};std::int32_t index{};unsigned pc=0x1014fa76u;
        for(;;)switch(pc) {
        case 0x1014fa76u:eax=O(q.object,q.object.objectFlags,a);pc=0x1014fa80u;break;
        case 0x1014fa80u:eax|=0x80000000u;O(q.object,q.object.writeObjectFlags,a,eax);pc=0x1014fa88u;break;
        case 0x1014fa88u:c=O(q.object,q.object.actualClass,a);pc=0x1014fa8du;break;
        case 0x1014fa8du:O(q.object,q.object.exitProperties,a,c);pc=0x1014fa95u;break;
        case 0x1014fa95u:O(q.object,q.object.setLinker,a,Pointer{},std::int32_t{-1});pc=0x1014faa0u;break;
        case 0x1014faa0u:outer=O(q.object,q.object.outer,a);pc=outer?0x1014faa7u:0x1014fabeu;break;
        case 0x1014faa7u:index=O(q.object,q.object.objectIndex,outer);pc=0x1014faaau;break;
        case 0x1014fabeu:index=0;pc=0x1014faaau;break;
        case 0x1014faaau:O(q.object,q.object.writeLinkerIndex,a,index);return;
        default:throw std::logic_error("unknown UObject.Destroy oracle PC");
        }
    }
    std::int32_t Run(const Task& task) {
        switch(task.mode) {
        case Mode::Conditional:return oracle?PcConditional(task.object):
            QuestVr::ConditionalDestroyOriginalObject(task.object,q.conditional);
        case Mode::Actor:if(oracle)PcActor(task.object);else QuestVr::DestroyOriginalActor(task.object,q.actor);break;
        case Mode::Object:if(oracle)PcObject(task.object);else QuestVr::DestroyOriginalObject(task.object,q.object);break;
        case Mode::SetLinker:
            if(oracle)PcLinker(task);
            else QuestVr::SetOriginalObjectLinker(task.object,task.newLinker,task.newIndex,q.linker);
            break;
        }
        return 0;
    }
    bool SameData(const Context& b)const {
        return std::tie(objects,exports,logs,nested,mutations)==std::tie(b.objects,b.exports,b.logs,b.nested,b.mutations);
    }
};
struct Outcome {int kind{},value{};bool operator==(const Outcome&)const=default;};
Outcome Execute(Context& c,const Task& task) {
    try{return {0,c.Run(task)};}catch(const std::invalid_argument&){return {1,0};}
    catch(const std::runtime_error&){return {2,0};}
}
using Strip=std::function<void(Pack&)>;
void Compare(const Context& seed,const Task& task,const std::string& label,const Strip& strip={}) {
    Context actual=seed,expected=seed;actual.oracle=false;expected.oracle=true;
    actual.Bind();expected.Bind();if(strip){strip(actual.q);strip(expected.q);}
    const auto a=Execute(actual,task),b=Execute(expected,task);++cases;
    Require(a==b,label+": return/exception differs from original PC oracle");
    Require(actual.trace==expected.trace,label+": provider order/arguments differ from original PC oracle");
    Require(actual.SameData(expected),label+": effects differ from original PC oracle");
    Require(actual.workUsed==expected.workUsed,label+": borrowed root work differs");
}
std::vector<Strip> MissingProviders() {
    return {
        [](Pack& q){q.conditional.objectIndex={};},[](Pack& q){q.conditional.objectFlags={};},
        [](Pack& q){q.conditional.writeObjectFlags={};},[](Pack& q){q.conditional.nativeDestroy={};},
        [](Pack& q){q.conditional.fullName={};},[](Pack& q){q.conditional.logMissingDebugDestroy={};},
        [](Pack& q){q.actor.auxiliary={};},[](Pack& q){q.actor.conditionalDestroy={};},
        [](Pack& q){q.actor.clearAuxiliary={};},[](Pack& q){q.actor.baseObjectDestroy={};},
        [](Pack& q){q.object.objectFlags={};},[](Pack& q){q.object.writeObjectFlags={};},
        [](Pack& q){q.object.actualClass={};},[](Pack& q){q.object.exitProperties={};},
        [](Pack& q){q.object.setLinker={};},[](Pack& q){q.object.outer={};},
        [](Pack& q){q.object.objectIndex={};},[](Pack& q){q.object.writeLinkerIndex={};},
        [](Pack& q){q.linker.linker={};},[](Pack& q){q.linker.linkerIndex={};},
        [](Pack& q){q.linker.exportObject={};},[](Pack& q){q.linker.clearExportObject={};},
        [](Pack& q){q.linker.writeLinker={};},[](Pack& q){q.linker.writeLinkerIndex={};}
    };
}
void Exercise(const Context& seed,const Task& task,const std::string& label) {
    Compare(seed,task,label);
    Context baseline=seed;baseline.Bind();static_cast<void>(Execute(baseline,task));
    // A refused provider leaves only the exact executed prefix; helpers never
    // catch it, erase earlier effects, complete later work, or invent rollback.
    for(std::size_t call=1u;call<=baseline.trace.size();++call) {
        auto failure=seed;failure.failProvider=call;Compare(failure,task,label+" provider-prefix");++providerFailures;
    }
    for(std::size_t prefix=0u;prefix<baseline.workUsed;++prefix) {
        auto failure=seed;failure.workLimit=prefix;Compare(failure,task,label+" root-work-prefix");++workFailures;
    }
    for(const auto& strip:MissingProviders()) {Compare(seed,task,label+" missing/inactive provider",strip);++missingCases;}
}
void BasicCases() {
    Context seed;
    seed.objects.at(Source).index=-1;Exercise(seed,{},"index -1 precedes any flags read");
    seed.objects.at(Source).index=-2;Exercise(seed,{},"index -2 is not -1");
    seed.objects.at(Source).index=0;
    for(const auto flags:{0u,0x80000000u,0x00800000u,0x80800000u,0xffffffffu,0x7f7fffffu,0x12345678u}) {
        seed.objects.at(Source).flags=flags;
        for(const auto kind:{0u,1u,2u,3u}) {
            seed.nativeKind=kind;seed.integratedBase=true;
            Exercise(seed,{},"Conditional flags/native-dispatch branches");
        }
    }
    seed=Context{};Exercise(seed,{Mode::Actor},"Actor null auxiliary");
    seed.objects.at(Source).auxiliary=Aux;
    for(const auto result:{0,1,-7}) {seed.auxResult=result;Exercise(seed,{Mode::Actor},"Actor ignores auxiliary ordinary result");}
    seed.integratedAux=true;seed.integratedBase=true;seed.nativeKind=2u;
    Exercise(seed,{Mode::Actor},"Actor nested full auxiliary then base");
    for(const auto outer:{Pointer{},Outer})for(const auto old:{Pointer{},Linker}) {
        seed=Context{};seed.objects.at(Source).outer=outer;seed.objects.at(Source).linker=old;
        seed.objects.at(Outer).index=-17;seed.objects.at(Source).flags=0x12345678u;
        Exercise(seed,{Mode::Object},"Object live Outer/cache and detach branch");
    }
    for(const auto old:{Pointer{},Linker})for(const auto target:{Pointer{},NewLinker,Linker}) {
        seed=Context{};seed.objects.at(Source).linker=old;
        Exercise(seed,{Mode::SetLinker,Source,target,-12},"SetLinker detach/rebind branches");
    }
    for(const auto occupant:{Pointer{},Other}) {
        seed=Context{};seed.objects.at(Source).linker=Linker;seed.exports.at({Linker,2})=occupant;
        Exercise(seed,{Mode::SetLinker,Source,NewLinker,3},"SetLinker native assertion refusal");
    }
    seed=Context{};seed.noWork=true;Exercise(seed,{},"optional mock work absent");
    for(const auto mode:{Mode::Conditional,Mode::Actor,Mode::Object,Mode::SetLinker})
        Exercise(seed,{mode,0u},"nonnull native identity precondition");
}
void CallbackCases() {
    Context seed;seed.nativeKind=0u;
    seed.hook=[](Context& c,const Record& r){
        if(r.op==Op::Native&&!c.nested) {
            c.nested=true;
            Require(c.Run({Mode::Conditional,r.a})==0,"same-root reentrant ConditionalDestroy was not RF_Destroyed gated");
            c.objects.at(r.a).flags|=0x80000000u;++c.mutations;
        }
    };
    Exercise(seed,{},"native callback same-root reentrance");++callbackCases;
    seed=Context{};seed.objects.at(Source).auxiliary=Aux;
    seed.hook=[](Context& c,const Record& r){if(r.op==Op::AuxConditional){c.objects.at(Source).auxiliary=Other;++c.mutations;}};
    Exercise(seed,{Mode::Actor},"auxiliary callback replacement still cleared after captured call");++callbackCases;
    seed=Context{};seed.objects.at(Source).linker=Linker;
    seed.hook=[](Context& c,const Record& r){if(r.op==Op::ExitProperties){
        c.objects.at(Source).outer=Other;c.objects.at(Other).index=-22;++c.mutations;
    }};
    Exercise(seed,{Mode::Object},"ExitProperties callback changes current Outer");++callbackCases;
    seed.hook=[](Context& c,const Record& r){if(r.op==Op::SetLinker){
        c.objects.at(Source).outer=Outer;c.objects.at(Outer).index=54;++c.mutations;
    }};
    Exercise(seed,{Mode::Object},"SetLinker callback changes later Outer cache");++callbackCases;
    seed=Context{};seed.objects.at(Source).linker=Linker;
    seed.hook=[](Context& c,const Record& r){if(r.op==Op::ExportObject&&c.mutations++==0u)c.exports.at({Linker,2})=Other;};
    Exercise(seed,{Mode::SetLinker,Source,NewLinker,3},"second export read sees changed equality operand");++callbackCases;
    seed.hook=[](Context& c,const Record& r){if(r.op==Op::ExportObject){
        c.objects.at(Source).linker=NewLinker;c.objects.at(Source).linkerIndex=99;++c.mutations;
    }};
    Exercise(seed,{Mode::SetLinker,Source,NewLinker,3},"retained old export slot survives callback binding mutation");++callbackCases;
    seed=Context{};seed.nativeKind=0u;seed.objects.at(Source).flags=0x80000000u;
    seed.hook=[](Context& c,const Record& r){if(r.op==Op::Log){c.objects.at(Source).flags=0x42u;++c.mutations;}};
    Exercise(seed,{},"missing Super diagnostic ordinary mutation remains and returns true");++callbackCases;
}
void DirectAssertions() {
    Context c;c.nativeKind=0u;c.objects.at(Source).flags=0x80000040u;c.Bind();
    Require(c.Run({})==1,"missing DebugDestroy must still return true");
    Require(c.objects.at(Source).flags==0x00800040u,"ConditionalDestroy exact mask must clear old debug marker");
    Require(c.logs==std::vector<std::string>{"Object10"},"missing Super must reach GetFullName and diagnostic");
    c=Context{};c.objects.at(Source).linker=Linker;c.objects.at(Source).outer=Outer;
    c.objects.at(Outer).index=85;c.Bind();c.Run({Mode::Object});
    Require(c.exports.at({Linker,2})==0u,"real live old export attachment must be cleared");
    Require(c.objects.at(Source).linker==0u&&c.objects.at(Source).linkerIndex==85,
        "detach None/-1 must precede current Outer-index cache overwrite");
    Require(c.objects.at(Source).sourcePath=="immutable package"&&c.objects.at(Source).sourceExport==77,
        "mutable linker detach must not erase authored provenance");
    Require(c.objects.at(Source).index==7,"UObject Destroy must not release native object-index ownership");
    Require(c.objects.at(Source).payload.empty(),"explicit lower ExitProperties mock was not reached");
    c=Context{};c.objects.at(Source).linker=Linker;c.Bind();c.Run({Mode::SetLinker,Source,Linker,2});
    Require(c.exports.at({Linker,2})==0u,"same-binding SetLinker still detaches old slot; never synthetic reattach");
}
} // namespace

int main() {
    try {
        BasicCases();CallbackCases();DirectAssertions();
        std::cout<<"Original object destruction mock protocol PASS: "<<cases<<" oracle cases, "<<checks
            <<" checks, "<<callbackCases<<" callback/reentrant cases, "<<providerFailures
            <<" provider prefixes, "<<workFailures<<" borrowed-root work prefixes, "<<missingCases
            <<" reached/inactive-provider cases. No original binary executed; no runtime cleanup/physics claim.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
