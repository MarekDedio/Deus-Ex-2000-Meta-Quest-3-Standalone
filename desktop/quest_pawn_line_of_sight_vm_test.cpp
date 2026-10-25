#include "quest_pawn_line_of_sight_protocol.h"
#include "quest_portable_vm.h"
#include "quest_script_random.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// EXPLICIT MOCK-HOST transaction evidence. The production LOS protocol runs
// inside real Vm::ExecuteTransaction/CallFunction/NativeWithExecution machinery,
// with the same borrowed Execution, raw journaled references and shared RNG.
// This is not an actual-map/native-Model/Enemy/target-hit or headset test.
namespace {
namespace Vm=QuestVr::Vm;
using Vm::Value;using Vm::Kind;using Vec=PortableModelVec3;
constexpr std::uint16_t FixtureNative=4094u;
std::size_t checks{},refusals{},roots{};
void Require(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
bool Same(Vec a,Vec b){return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)&&
    std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)&&std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z);}
bool Same(const Value& value,Vec point){return value.kind==Kind::Vector&&Same({value.vector[0],value.vector[1],value.vector[2]},point);}
bool SameValue(const Value& a,const Value& b){return a.kind==Kind::Vector&&b.kind==Kind::Vector&&
    Same(a,{b.vector[0u],b.vector[1u],b.vector[2u]});}
Value Vector(Vec point){return Value::Vector({point.x,point.y,point.z});}
struct Ray {Vec end,start;};
struct MockHost final:Vm::Host {
    std::array<Value,2u> positions{Vector({-0.0f,-7,19}),Vector({-31,45,-0.0f})},beforePositions;
    std::optional<std::uint32_t> seed{0x13579bdfu},beforeSeed;
    QuestVr::NativeWork::Budget work;
    Vm::Execution* expectedExecution{};
    bool enemy{true},player{},losFlag{},distantPlayer{},defaultClear{true},active{},positionsRestoredBeforeHostRollback{};
    Vec source{},target{20,0,0},freshSource{7,8,9},currentEnemy{31,32,33};
    float eyeHeight{40},height{0},radius{0};std::uint8_t visibility{255u};
    std::vector<bool> replies;std::vector<Ray> rays;std::vector<std::string> fields,stores;
    std::size_t begins{},commits{},rollbacks{},nativeEntries{},sourceReads{},draws{},positionReads{},positionWrites{};
    std::optional<std::size_t> faultQuery,faultRead,faultWrite;
    bool faultAfterStored{},freshSourceOnHistory{true};
    Vm::Function method;
    MockHost(){method.path="Mock.InternalPawnLineOfSight";method.source="Mock";method.nativeIndex=FixtureNative;method.flags=0x401u;
        method.variables.push_back({method.path+".ReturnValue","ReturnValue",Value::Bool(false),0x580u});}
    void Begin()override{BeginRoot({});}
    void BeginRoot(const Vm::Limits& limits)override{
        Require(!active,"Mock LOS opened a nested host root");active=true;++begins;++roots;beforeSeed=seed;beforePositions=positions;
        work.Reset(limits.nativeWork);
    }
    std::optional<QuestVr::NativeWork::Usage> ReadNativeWork()const noexcept override{return work.Read();}
    void Commit()override{work.Check();Require(active,"Mock LOS commit without root");++commits;active=false;}
    void Rollback()noexcept override{
        if(!active)return;
        positionsRestoredBeforeHostRollback=SameValue(positions[0u],beforePositions[0u])&&SameValue(positions[1u],beforePositions[1u]);
        // Deliberately DO NOT restore positions here: the real VM reference
        // journal must have undone them before host rollback begins.
        seed=beforeSeed;++rollbacks;active=false;
    }
    Vm::Property ResolveProperty(const Vm::Function&,std::int32_t)override{throw std::runtime_error("Unexpected mock property lookup");}
    std::string ResolveName(const Vm::Function&,std::int32_t)override{throw std::runtime_error("Unexpected mock name lookup");}
    std::string ResolveObject(const Vm::Function&,std::int32_t)override{throw std::runtime_error("Unexpected mock object lookup");}
    std::shared_ptr<const Vm::Function> ResolveFunction(const Vm::Function&,const std::string&,const Vm::Invocation&)override{
        throw std::runtime_error("Unexpected mock function lookup");}
    std::shared_ptr<Vm::Reference> Variable(const std::string&,const Vm::Property&,Vm::Scope)override{
        throw std::runtime_error("Unexpected mock script variable lookup");}
    Vm::Evaluation Native(std::uint16_t,const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*)override{
        throw std::runtime_error("LOS fixture must borrow NativeWithExecution");}
    std::shared_ptr<Vm::Reference> Position(std::size_t slot){
        auto reference=std::make_shared<Vm::Reference>();reference->zero=Value::Vector({});reference->resultLayout=Vm::ResultLayout::CoreVector12;
        reference->read=[this,slot]{++positionReads;
            if(faultRead&&*faultRead==slot){faultRead.reset();throw std::runtime_error("Mock LOS position read fault");}
            return positions[slot];};
        reference->write=[this,slot](const Value& value){++positionWrites;positions[slot]=value;
            stores.push_back(slot==0u?"LastSeeingPos":"LastSeenPos");
            if(faultWrite&&*faultWrite==slot){faultWrite.reset();throw std::runtime_error("Mock LOS position store fault");}};
        return reference;
    }
    struct Ops {
        MockHost& h;Vm::Execution& execution;
        struct Flags {bool losFlag{},player{};};
        bool TargetIsEnemy(){h.fields.push_back("Enemy");return h.enemy;}
        Vec TargetLocation(){h.fields.push_back("Target.Location");return h.target;}
        Vec SourceLocation(){h.fields.push_back("Source.Location");++h.sourceReads;
            return h.freshSourceOnHistory&&h.sourceReads>=3u?h.freshSource:h.source;}
        bool IsPlayer(){h.fields.push_back("bIsPlayer");return h.player;}
        std::uint8_t Visibility(){h.fields.push_back("Visibility");return h.visibility;}
        float BaseEyeHeight(){h.fields.push_back("BaseEyeHeight");return h.eyeHeight;}
        float CollisionHeight(){h.fields.push_back("CollisionHeight");return h.height;}
        float CollisionRadius(){h.fields.push_back("CollisionRadius");return h.radius;}
        bool SideFlag(){h.fields.push_back("bLOSflag");return h.losFlag;}
        Flags DistantFlags(){h.fields.push_back("DistantFlags");return {h.losFlag,h.distantPlayer};}
        std::uint32_t Draw15(){h.work.Step();h.fields.push_back("Draw15");++h.draws;
            if(!h.seed)h.seed=QuestVr::ScriptRandom::InitialSeed;
            return QuestVr::ScriptRandom::Draw15(*h.seed);}
        bool FastLine(const Vec& end,const Vec& start){h.work.Step();h.rays.push_back({end,start});
            if(h.faultQuery&&h.rays.size()==*h.faultQuery)throw std::runtime_error("Mock LOS query fault "+std::to_string(*h.faultQuery));
            return h.rays.size()<=h.replies.size()?h.replies[h.rays.size()-1u]:h.defaultClear;}
        void WriteLastSeeingPos(const Vec& point){h.fields.push_back("WriteLastSeeingPos");execution.WriteReference(h.Position(0u),Vector(point));}
        void WriteLastSeenPosFromCurrentEnemy(){h.fields.push_back("CurrentEnemy.Location");execution.WriteReference(h.Position(1u),Vector(h.currentEnemy));}
    };
    Vm::Evaluation NativeWithExecution(std::uint16_t index,const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,
        const Vm::Function* declaration,Vm::Execution& execution)override{
        Require(index==FixtureNative&&receiver=="MockSource"&&arguments.empty()&&declaration&&declaration->path==method.path&&
            &execution==expectedExecution,"Mock LOS changed native declaration or borrowed another Execution/root");
        ++nativeEntries;work.Step();Ops ops{*this,execution};const bool visible=QuestVr::CheckOriginalPawnLineOfSightToPawn(ops);
        if(faultAfterStored)throw std::runtime_error("Mock LOS late native failure");
        return {Value::Bool(visible),{}};
    }
    bool Saw(const char* field)const{return std::find(fields.begin(),fields.end(),field)!=fields.end();}
};
Vm::Result Run(MockHost& host,const Vm::Limits& limits={},bool late=false,bool catchFailure=false,unsigned repeats=1u){
    return Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){
        host.expectedExecution=&execution;Value result;
        for(unsigned i=0u;i<repeats;++i){
            try{result=execution.CallFunction(host.method,"MockSource");}
            catch(...){if(!catchFailure)throw;}
        }
        if(late)throw std::runtime_error("Mock LOS late coordinator failure");
        return catchFailure?Value::Bool(true):result;
    },limits);
}
void Committed(const Vm::Result& result,const MockHost& host,bool visible,std::size_t writes){
    Require(result.passed()&&result.committed&&result.value.kind==Kind::Bool&&result.value.boolean==visible&&result.writes==writes&&
        result.nativeWork&&!result.nativeWork->exhausted&&host.begins==1u&&host.commits==1u&&!host.rollbacks&&!host.active,
        "Mock LOS failed successful single-root commit");
}
void RolledBack(const Vm::Result& result,const MockHost& host,const std::string& error={},Vm::Status status=Vm::Status::Unsupported){
    Require(!result.passed()&&!result.committed&&result.status==status&&(!error.empty()?result.error==error:!result.error.empty())&&
        result.value.kind==Kind::Nothing&&host.begins==1u&&!host.commits&&host.rollbacks==1u&&!host.active&&
        host.seed==host.beforeSeed&&SameValue(host.positions[0u],host.beforePositions[0u])&&SameValue(host.positions[1u],host.beforePositions[1u])&&
        host.positionsRestoredBeforeHostRollback,"Mock LOS failed complete one-root RNG/position journal rollback");++refusals;
}
void EnemyHistory(){
    {
        MockHost host;const auto seed=host.seed;const auto result=Run(host);Committed(result,host,true,2u);
        Require(host.nativeEntries==1u&&host.rays.size()==1u&&host.positionReads==2u&&host.positionWrites==2u&&!host.draws&&host.seed==seed&&
            host.stores==std::vector<std::string>{"LastSeeingPos","LastSeenPos"}&&Same(host.positions[0u],host.freshSource)&&
            Same(host.positions[1u],host.currentEnemy)&&Same(host.rays[0u].end,host.target)&&Same(host.rays[0u].start,{0,0,40}),
            "Enemy centre success did not use fresh source/current Enemy or exact ordered two history stores");
    }
    {
        MockHost host;host.replies={false,true};const auto result=Run(host);Committed(result,host,true,2u);
        Require(host.rays.size()==2u&&Same(host.rays[1u].end,host.target)&&Same(host.rays[1u].start,host.freshSource)&&
            Same(host.positions[0u],host.freshSource)&&Same(host.positions[1u],host.currentEnemy)&&!host.Saw("CollisionHeight")&&
            host.stores==std::vector<std::string>{"LastSeeingPos","LastSeenPos"},
            "Second enemy centre query lost fresh ground source/current Enemy or used inactive head geometry");
    }
    {
        MockHost host;host.replies={false,false,true};const auto result=Run(host);Committed(result,host,true,0u);
        Require(host.rays.size()==3u&&!host.positionReads&&!host.positionWrites&&host.Saw("CollisionHeight")&&
            !host.Saw("CollisionRadius"),"Enemy head-only success incorrectly wrote centre-only history or read corners");
    }
    for(std::size_t cut=0u;cut<2u;++cut){MockHost host;Vm::Limits limits;limits.writes=cut;const auto result=Run(host,limits);
        RolledBack(result,host,"VM write limit",Vm::Status::Budget);
        Require(result.writes==cut&&host.positionReads==cut&&host.positionWrites==2u*cut&&host.rays.size()==1u,
            "Enemy history write cutoff did not admit/store/undo exactly its reached prefix");}
    for(const bool nativeLate:{false,true}){
        MockHost host;host.faultAfterStored=nativeLate;const auto result=Run(host,{},!nativeLate);
        RolledBack(result,host,nativeLate?"Mock LOS late native failure":"Mock LOS late coordinator failure");
        Require(result.writes==2u&&host.positionWrites==4u,"Late LOS failure did not undo both completed history writes");
    }
    for(std::size_t slot=0u;slot<2u;++slot)for(const bool storeFault:{false,true}){
        MockHost host;if(storeFault)host.faultWrite=slot;else host.faultRead=slot;
        const auto result=Run(host);RolledBack(result,host,storeFault?"Mock LOS position store fault":"Mock LOS position read fault");
        Require(result.writes==slot+1u&&host.rays.size()==1u,"History provider fault changed its genuine reached write admission");
    }
    {
        MockHost host;Vm::Limits limits;limits.writes=1u;const auto result=Run(host,limits,false,true,2u);
        RolledBack(result,host,"VM write limit",Vm::Status::Budget);
        Require(host.nativeEntries==1u&&result.writes==1u,"Caught history failure reopened native execution or committed its prefix");
    }
    {
        MockHost host;host.faultAfterStored=true;const auto result=Run(host,{},false,true,2u);
        RolledBack(result,host,"Mock LOS late native failure");
        Require(host.nativeEntries==1u&&result.writes==2u,"Caught post-write native failure failed to poison its root");
    }
    {
        MockHost host;Vm::Limits limits;limits.writes=1u;const auto result=Run(host,limits,true,true,2u);
        RolledBack(result,host,"VM write limit",Vm::Status::Budget);
        Require(host.nativeEntries==1u&&result.writes==1u,"Later coordinator exception replaced the first caught history-write failure");
    }
    {
        MockHost host;host.freshSourceOnHistory=false;Vm::Limits limits;limits.writes=3u;const auto result=Run(host,limits,false,false,2u);
        RolledBack(result,host,"VM write limit",Vm::Status::Budget);
        Require(host.nativeEntries==2u&&result.writes==3u&&host.rays.size()==2u,"Second LOS callback acquired fresh writes/root allowance");
    }
}
std::uint32_t DrawnSeed(std::uint32_t value){return value*214013u+2531011u;}
std::uint32_t SeedFor(bool accepted){
    for(std::uint32_t seed=1u;seed<100000u;++seed) {
        if((((DrawnSeed(seed)>>16u)&0x7fffu)>=16384u)==accepted)return seed;
    }
    throw std::runtime_error("Cannot find bounded deterministic CRT seed witness");
}
void Far(MockHost& host,std::uint32_t seed){host.enemy=false;host.target={1500,0,0};host.seed=seed;host.freshSourceOnHistory=false;}
void RandomAndGates(){
    for(const bool accepted:{false,true}){
        MockHost host;const auto seed=SeedFor(accepted);Far(host,seed);const auto result=Run(host);Committed(result,host,accepted,0u);
        Require(host.seed==DrawnSeed(seed)&&host.draws==1u&&host.rays.size()==(accepted?1u:0u)&&!host.positionReads&&!host.positionWrites&&
            !host.Saw("CollisionHeight")&&!host.Saw("CollisionRadius"),"Far nonplayer LOS failed single-draw lazy branch or persisted wrong CRT state");
    }
    {
        MockHost host;Far(host,SeedFor(true));const auto result=Run(host,{},true);RolledBack(result,host,"Mock LOS late coordinator failure");
        Require(host.draws==1u&&host.rays.size()==1u&&!result.writes,"Late far-centre failure did not exercise a successful random/query prefix");
    }
    {
        MockHost host;Far(host,1u);host.seed.reset();const auto result=Run(host);Committed(result,host,false,0u);
        Require(host.draws==1u&&host.seed==DrawnSeed(1u)&&host.rays.empty(),"Reached absent RNG did not lazily initialize/draw the original default stream");
    }
    {
        MockHost host;Far(host,1u);host.seed.reset();const auto result=Run(host,{},true);RolledBack(result,host,"Mock LOS late coordinator failure");
        Require(host.draws==1u&&!host.seed,"Rollback did not restore absent RNG optional after its reached initialization");
    }
    for(const bool outside:{false,true}){
        MockHost host;host.enemy=false;host.target={outside?5000.0f:3500.0f,0,0};host.seed.reset();
        const auto result=Run(host);Committed(result,host,false,0u);
        Require(!host.seed&&!host.draws&&host.rays.empty()&&!host.positionReads&&!host.positionWrites&&
            !host.Saw("CollisionRadius")&&!host.Saw("CollisionHeight")&&
            (outside?!host.Saw("BaseEyeHeight")&&!host.Saw("DistantFlags"):host.Saw("DistantFlags")),
            "Early range/half-range gate consumed inactive RNG/query/history/geometry fields");
    }
    {
        MockHost host;host.enemy=false;host.target={20,0,0};host.seed.reset();const auto result=Run(host);Committed(result,host,true,0u);
        Require(!host.seed&&!host.draws&&host.rays.size()==1u&&host.Saw("CollisionHeight")&&!host.Saw("CollisionRadius")&&
            !host.positionReads&&!host.positionWrites,"Near nonenemy head success read inactive random/corner/history fields");
    }
    {
        MockHost host;Far(host,SeedFor(true));host.faultQuery=1u;const auto result=Run(host,{},false,true,2u);
        RolledBack(result,host,"Mock LOS query fault 1");
        Require(host.draws==1u&&host.nativeEntries==1u&&!result.writes,"Caught post-RNG query failure committed its draw or reopened native execution");
    }
    {
        MockHost host;Far(host,SeedFor(true));Vm::Limits limits;limits.nativeWork=2u;
        const auto result=Run(host,limits,false,true,2u);RolledBack(result,host,{},Vm::Status::Budget);
        Require(host.draws==1u&&host.rays.empty()&&host.nativeEntries==1u&&result.nativeWork&&result.nativeWork->exhausted&&
            result.nativeWork->used==2u,"Post-draw query admission failure did not undo RNG or poison the shared root");
    }
    {
        MockHost host;const auto seed=SeedFor(true);Far(host,seed);const auto result=Run(host,{},false,false,2u);Committed(result,host,
            ((DrawnSeed(DrawnSeed(seed))>>16u)&0x7fffu)>=16384u,0u);
        Require(host.draws==2u&&host.nativeEntries==2u&&host.seed==DrawnSeed(DrawnSeed(seed)),"Two LOS calls reset their shared native CRT stream");
    }
    {
        MockHost host;const auto seed=SeedFor(true);Far(host,seed);const auto result=Run(host,{},true,false,2u);
        RolledBack(result,host,"Mock LOS late coordinator failure");Require(host.draws==2u&&host.nativeEntries==2u,"Two-draw late rollback did not exercise both calls");
    }
}
void QueryAndBudgetFailures(){
    {
        MockHost host;host.defaultClear=false;const auto result=Run(host);Committed(result,host,false,0u);
        Require(host.rays.size()==6u&&!host.draws&&!host.positionReads&&!host.positionWrites&&host.Saw("CollisionRadius")&&host.Saw("bLOSflag"),
            "Degenerate equal-extrema corners did not establish the genuine six-query maximum route");
    }
    for(std::size_t index=1u;index<=6u;++index){MockHost host;host.defaultClear=false;host.faultQuery=index;
        const auto result=Run(host,{},false,true,2u);RolledBack(result,host,"Mock LOS query fault "+std::to_string(index));
        Require(host.rays.size()==index&&host.nativeEntries==1u&&!result.writes&&!host.draws,
            "Per-query failure was swallowed, continued, redrawn or escaped its original root");}
    MockHost complete;complete.defaultClear=false;const auto success=Run(complete);Committed(success,complete,false,0u);
    const auto work=success.nativeWork->used;Require(work==7u,"Mock maximum route omitted native entry or six shared query admissions");
    for(std::size_t cut=0u;cut<work;++cut){MockHost host;host.defaultClear=false;Vm::Limits limits;limits.nativeWork=cut;
        const auto result=Run(host,limits,false,true,2u);RolledBack(result,host,{},Vm::Status::Budget);
        Require(result.nativeWork&&result.nativeWork->exhausted&&result.nativeWork->used==cut&&host.nativeEntries==1u&&!result.writes,
            "Caught LOS native-work cutoff reset/ignored the shared root allowance");}
    {
        MockHost host;host.defaultClear=false;Vm::Limits limits;limits.nativeWork=work;const auto result=Run(host,limits);Committed(result,host,false,0u);
        Require(result.nativeWork->used==work,"Exact N maximum-query allowance changed its shared work ledger");
    }
    {
        MockHost host;host.defaultClear=false;Vm::Limits limits;limits.nativeWork=work;const auto result=Run(host,limits,false,false,2u);
        RolledBack(result,host,{},Vm::Status::Budget);
        Require(host.nativeEntries==2u&&host.rays.size()==6u&&result.nativeWork->used==work,"Second LOS callback obtained a fresh native-work ledger");
    }
}
} // namespace
int main(){try{EnemyHistory();RandomAndGates();QueryAndBudgetFailures();
    std::cout<<"PASS EXPLICIT MOCK-HOST original Pawn LOS protocol in real VM transactions: "<<checks<<" checks / "<<refusals
        <<" refusals / "<<roots<<" single roots; borrowed Execution, exact ordered history-write undo, shared optional CRT rollback/persistence, "
        "six-query faults, caught-failure poisoning and shared native-work limits. NOT actual-map LOS/Enemy/target-hit or headset evidence\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL mock-host Pawn LOS VM: "<<error.what()<<'\n';return 1;}}
