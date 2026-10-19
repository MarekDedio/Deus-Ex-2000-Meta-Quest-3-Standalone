#include "quest_portable_vm.h"

#include <array>
#include <bit>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Generated VM fixtures only. No commercial code is loaded/executed and no
// map is opened. The host's trusted layout mocks are NOT Core.u schema proof.
namespace {
namespace Vm=QuestVr::Vm;
using Value=Vm::Value;
using Rotation=std::array<std::int32_t,3u>;
using Bytes=std::vector<std::uint8_t>;
constexpr auto Layout=Vm::ResultLayout::CoreRotator12;
std::size_t checks{},arithmetic{},protocol{},refusals{},rollbackCases{};
void Require(bool condition,const std::string& reason){++checks;if(!condition)throw std::runtime_error(reason);}
void U32(Bytes& bytes,std::uint32_t word){for(unsigned i=0u;i<4u;++i)bytes.push_back(static_cast<std::uint8_t>(word>>(i*8u)));}
Bytes Join(std::initializer_list<Bytes> pieces){Bytes out;for(const auto& p:pieces)out.insert(out.end(),p.begin(),p.end());return out;}
Bytes Ref(std::uint8_t opcode,std::int32_t index){Bytes out{opcode};U32(out,std::bit_cast<std::uint32_t>(index));return out;}
Bytes Rot(const Rotation& value){Bytes out{0x22};for(const auto word:value)U32(out,std::bit_cast<std::uint32_t>(word));return out;}
Bytes Native(std::uint16_t index,std::initializer_list<Bytes> args={}){
    Bytes out;if(index>=0x70u&&index<=0xffu)out.push_back(static_cast<std::uint8_t>(index));
    else{out.push_back(static_cast<std::uint8_t>(0x60u+(index>>8u)));out.push_back(static_cast<std::uint8_t>(index));}
    for(const auto& p:args)out.insert(out.end(),p.begin(),p.end());
    out.push_back(0x16u);return out;
}
Bytes Final(std::initializer_list<Bytes> args){auto out=Ref(0x1cu,7);for(const auto& p:args)out.insert(out.end(),p.begin(),p.end());out.push_back(0x16u);return out;}
Bytes Return(Bytes value){return Join({{0x04u},value});}
Bytes Let(Bytes lhs,Bytes rhs){return Join({{0x0fu},lhs,rhs});}
Bytes Int(std::int32_t value){return Ref(0x1du,value);}
Bytes Context(Bytes object,Bytes value,std::uint8_t opcode=0x19u,std::uint8_t clear=0u){Bytes out{opcode};
    out.insert(out.end(),object.begin(),object.end());out.push_back(static_cast<std::uint8_t>(value.size()));out.push_back(0u);out.push_back(clear);
    return Join({out,value});}
Bytes Skip(Bytes value){Bytes out{0x18u};out.push_back(static_cast<std::uint8_t>(value.size()));out.push_back(0u);return Join({out,value});}
Bytes Element(Bytes index,Bytes array){return Join({{0x1au},index,array});}
Bytes Member(Bytes parent){return Join({Ref(0x36u,5),parent});}
Bytes NullContext(std::uint8_t count){const auto child=Native(4095u);Bytes out{0x19u,0x2au};
    out.push_back(static_cast<std::uint8_t>(child.size()));out.push_back(0u);out.push_back(count);return Join({out,child});}
Vm::Function Script(Bytes code){Vm::Function out;out.path="Generated.RotatorAdd";out.source="Generated";out.bytecode=std::move(code);return out;}
Vm::Function Declaration(){
    Vm::Function out;out.path="Core.Object.Add_RotatorRotator";out.source="Core";out.nativeIndex=316u;out.flags=0x3401u;
    out.variables={{out.path+".A","A",Value::Rotator({}),0x80u,1u,Layout},
        {out.path+".B","B",Value::Rotator({}),0x80u,1u,Layout},
        {out.path+".ReturnValue","ReturnValue",Value::Rotator({}),0x580u,1u,Layout}};return out;
}

// Independent schoolbook byte addition. It never invokes the production
// arithmetic, signed C++ overflow, angle normalization or floating point.
std::uint32_t AddWord(std::uint32_t a,std::uint32_t b){
    std::uint32_t out{};std::uint16_t carry{};
    for(unsigned i=0u;i<4u;++i){const auto sum=static_cast<std::uint16_t>(
        static_cast<std::uint16_t>((a>>(8u*i))&255u)+static_cast<std::uint16_t>((b>>(8u*i))&255u)+carry);
        out|=static_cast<std::uint32_t>(sum&255u)<<(8u*i);carry=static_cast<std::uint16_t>(sum>>8u);}
    return out;
}
Rotation Expected(const Rotation& a,const Rotation& b){Rotation out;
    for(unsigned i=0u;i<3u;++i)out[i]=std::bit_cast<std::int32_t>(AddWord(std::bit_cast<std::uint32_t>(a[i]),std::bit_cast<std::uint32_t>(b[i])));
    return out;}
void Same(const Value& actual,const Rotation& expected,const std::string& reason){
    Require(actual.kind==Vm::Kind::Rotator&&actual.rotation==expected&&actual.text.empty()&&actual.fields.empty()&&actual.intArray.empty(),reason);
}

struct Host final:Vm::Host {
    std::map<std::string,Value> state,saved;
    std::map<std::int32_t,Vm::Property> properties;
    Vm::Function declaration=Declaration();
    std::vector<std::string> trace;
    std::size_t begins{},commits{},rollbacks{},effects{},savedEffects{},readsA{},readsAtB{},customCalls{},delegated316{};
    std::size_t arrayBaseReads{},indexReads{},parentReads{},classSelections{},staleReads{},staleElements{};
    std::array<std::size_t,2u> elementReads{};
    std::size_t selectorReads{},selectorCalls{},indexCalls{},selectorAliasReads{},indexAliasReads{},childEffects{},arrayVariables{},pairVariables{};
    unsigned sourceStorage{};
    bool active{},layoutHook{true},seedConflict{},failB{},mutateB{},changingRead{};
    Vm::ResultLayout suppliedLayout{Layout};
    Value replacement=Value::Rotator({-77,88,-99}),bResult=Value::Rotator({7,-8,9}),customResult=Value::Rotator({});
    Value laterRead=Value::Rotator({-19001,29002,-39003});
    Vm::ResultDisposition selectorDisposition{Vm::ResultDisposition::NoWrite},indexDisposition{Vm::ResultDisposition::NoWrite};
    std::uint8_t selectorClear{},indexClear{};
    Value selectorPayload=Value::Text(Vm::Kind::Object,"PlainObject"),writtenSelector=Value::Text(Vm::Kind::Object,"PlainObject");
    Value indexPayload=Value::Integer(1),childResult=Value::Rotator({71,-82,93});
    Host(){
        properties.emplace(1,Vm::Property{"Generated.A","A",Value::Rotator({}),0u,1u,Layout});
        properties.emplace(2,Vm::Property{"Generated.Out","Out",Value::Rotator({}),0u,1u,Layout});
        properties.emplace(3,Vm::Property{"Generated.Array","Array",Value::Rotator({}),0u,2u,Layout});
        Value pair;pair.kind=Vm::Kind::Struct;pair.fields={{"value",Value::Rotator({})},{"sentinel",Value::Integer(93)}};
        properties.emplace(4,Vm::Property{"Generated.Pair","Pair",pair,0u,1u,Vm::ResultLayout::Unknown});
        properties.emplace(5,Vm::Property{"Generated.Pair.Value","Value",Value::Rotator({}),0u,1u,Layout});
        properties.emplace(6,Vm::Property{"Generated.Index","Index",Value::Integer(0),0u,1u,Vm::ResultLayout::Unknown});
        properties.emplace(7,Vm::Property{"Generated.Selector","Selector",Value::Text(Vm::Kind::Object,{}),0u,1u,Vm::ResultLayout::Unknown});
        state.emplace("Generated.A",Value::Rotator({11,22,33}));state.emplace("Generated.Out",Value::Rotator({101,202,303}));
        state.emplace("Generated.Array0",Value::Rotator({901,902,903}));state.emplace("Generated.Array1",Value::Rotator({11,22,33}));
        pair.fields.at("value")=Value::Rotator({11,22,33});state.emplace("Generated.Pair",pair);state.emplace("Generated.Index",Value::Integer(1));
    }
    void Begin()override{Require(!active,"Generated host nested a root");saved=state;savedEffects=effects;active=true;++begins;}
    void Commit()override{Require(active,"Generated host committed absent root");active=false;++commits;}
    void Rollback()noexcept override{state.swap(saved);effects=savedEffects;active=false;++rollbacks;}
    Vm::Property ResolveProperty(const Vm::Function&,std::int32_t index)override{return properties.at(index);}
    std::string ResolveName(const Vm::Function&,std::int32_t)override{return "Unused";}
    std::string ResolveObject(const Vm::Function&,std::int32_t index)override{
        if(index==9)return "GeneratedClass";
        throw std::runtime_error("Unexpected generated object lookup");}
    std::string ClassDefaultObject(const std::string& selected)override{
        Require(selected=="GeneratedClass","ClassContext selected an unrequested class");++classSelections;return "GeneratedClassDefault";}
    std::shared_ptr<const Vm::Function> ResolveFunction(const Vm::Function&,const std::string&,const Vm::Invocation& call)override{
        if(call.kind!=Vm::CallKind::Final||call.reference!=7)throw std::runtime_error("Unexpected generated function lookup");
        return std::make_shared<Vm::Function>(declaration);
    }
    Value SampleSource(Value value){++readsA;trace.push_back("readA");return changingRead&&readsA>1u?laterRead:value;}
    std::shared_ptr<Vm::Reference> ArrayElement(std::size_t index){
        Require(index<2u,"Generated fixed-array index exceeded its actual dimension");
        const auto key=index?"Generated.Array1":"Generated.Array0";
        auto out=std::make_shared<Vm::Reference>();out->zero=Value::Rotator({});out->resultLayout=Layout;
        out->read=[this,key,index]{++elementReads[index];return index?SampleSource(state.at(key)):state.at(key);};
        out->write=[this,key](const Value& value){state.at(key)=value;};return out;
    }
    Value ReadPair(){++parentReads;auto out=state.at("Generated.Pair");out.fields.at("value")=SampleSource(out.fields.at("value"));return out;}
    std::shared_ptr<Vm::Reference> SourceReference(){
        if(sourceStorage==1u)return ArrayElement(1u);
        if(sourceStorage==2u){auto out=std::make_shared<Vm::Reference>();out->zero=Value::Rotator({});out->resultLayout=Layout;
            out->read=[this]{return ReadPair().fields.at("value");};
            out->write=[this](const Value& value){state.at("Generated.Pair").fields.at("value")=value;};return out;}
        return Reference(properties.at(1));
    }
    std::shared_ptr<Vm::Reference> Reference(const Vm::Property& p){
        auto out=std::make_shared<Vm::Reference>();out->zero=p.zero;out->dimension=p.arrayDimension;out->resultLayout=p.resultLayout;
        out->read=[this,key=p.key]{
            if(key=="Generated.A")return SampleSource(state.at(key));
            if(key=="Generated.Array"){++arrayBaseReads;return state.at("Generated.Array0");}
            if(key=="Generated.Pair")return ReadPair();
            if(key=="Generated.Index"){++indexReads;return indexReads==1u?Value::Integer(1):Value::Integer(0);}
            if(key=="Generated.Selector"){++selectorReads;return selectorReads==1u?writtenSelector:Value::Text(Vm::Kind::Object,{});}
            return state.at(key);
        };
        out->write=[this,key=p.key](const Value& v){trace.push_back("write:"+key);state.at(key)=v;};return out;
    }
    std::shared_ptr<Vm::Reference> Variable(const std::string& receiver,const Vm::Property& p,Vm::Scope scope)override{
        Require((receiver=="PlainObject"||receiver=="GeneratedClassDefault")&&scope==Vm::Scope::Instance,
            "Core operator fixture became Actor/context-specific");auto out=Reference(p);
        if(p.key=="Generated.Array"){++arrayVariables;out->element=[this](std::size_t index){return ArrayElement(index);};}
        if(p.key=="Generated.Pair")++pairVariables;
        return out;
    }
    std::optional<Vm::ResultLayout> NativeArgumentUninitializedLayout(std::uint16_t index,const std::string& receiver,
        std::size_t slot,const Vm::Function*)override{
        if(index!=316u)return std::nullopt;
        Require(receiver=="PlainObject","Rotator operator lost non-Actor UObject receiver");
        trace.push_back("layout:"+std::to_string(slot));return layoutHook?std::optional<Vm::ResultLayout>{suppliedLayout}:std::nullopt;
    }
    std::optional<Value> NativeArgumentSeed(std::uint16_t index,const std::string&,std::size_t,const Vm::Function*)override{
        if(index==316u&&seedConflict)return Value::Rotator({});
        return std::nullopt;
    }
    Vm::Evaluation Native(std::uint16_t index,const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*)override{
        if(index==316u){++delegated316;throw std::runtime_error("Rotator arithmetic delegated to test Host");}
        if(index==1001u){++customCalls;return {customResult,{}};}
        if(index==1002u){auto stale=std::make_shared<Vm::Reference>();stale->zero=Value::Rotator({});stale->resultLayout=Layout;stale->dimension=2u;
            stale->read=[this]()->Value{++staleReads;throw std::runtime_error("Stale NoWrite alias was read");};
            stale->element=[this](std::size_t)->std::shared_ptr<Vm::Reference>{++staleElements;throw std::runtime_error("Stale NoWrite array address was selected");};
            return {Value::Rotator({4,5,6}),stale,Vm::ResultDisposition::NoWrite};}
        if(index==1003u||index==1004u){
            const bool selector=index==1003u;
            if(selector)++selectorCalls;else ++indexCalls;
            auto stale=std::make_shared<Vm::Reference>();
            stale->zero=selector?Value::Text(Vm::Kind::Object,{}):Value::Integer(0);
            stale->read=[this,selector]()->Value{
                if(selector)++selectorAliasReads;else ++indexAliasReads;
                throw std::runtime_error("Initialized selector/index read stale attached alias");
            };
            return {selector?selectorPayload:indexPayload,stale,selector?selectorDisposition:indexDisposition,selector?selectorClear:indexClear};
        }
        if(index==1005u){++childEffects;return {childResult,{}};}
        throw std::runtime_error("Generated deliberate unknown native "+std::to_string(index));
    }
    Vm::Evaluation NativeWithExecution(std::uint16_t index,const std::string& receiver,const std::vector<Vm::Evaluation>& args,
        const Vm::Function* fn,Vm::Execution& execution)override{
        if(index!=1000u)return Native(index,receiver,args,fn);
        Require(args.empty(),"Generated B mutation acquired unrequested arguments");trace.push_back("B");readsAtB=readsA;++effects;
        if(mutateB)execution.WriteReference(SourceReference(),replacement);
        if(failB)throw std::runtime_error("Generated B failure after side effect");
        return {bResult,{}};
    }
};
bool SameState(const std::map<std::string,Value>& a,const std::map<std::string,Value>& b){
    if(a.size()!=b.size())return false;
    for(const auto& [key,value]:a){const auto found=b.find(key);if(found==b.end()||!Vm::Equal(value,found->second))return false;}return true;
}
enum class Route{Opcode,Final,Direct,Execution};
const std::array<Route,4u> Routes{Route::Opcode,Route::Final,Route::Direct,Route::Execution};
Bytes AddExpression(Route route,Bytes a,Bytes b){return route==Route::Opcode?Native(316u,{a,b}):Final({a,b});}
Vm::Result Inputs(Host& host,Route route,Vm::Evaluation a,Vm::Evaluation b,const Vm::Limits& limits={}){
    if(route==Route::Direct)return Vm::Execute(host,host.declaration,"PlainObject",{a,b},limits);
    if(route==Route::Execution)return Vm::ExecuteTransaction(host,[&](Vm::Execution& e){return e.CallFunction(host.declaration,"PlainObject",{a,b});},limits);
    throw std::runtime_error("Generated Inputs requires direct route");
}
Vm::Result Literals(Host& host,Route route,const Rotation& a,const Rotation& b,const Vm::Limits& limits={}){
    if(route==Route::Direct||route==Route::Execution)return Inputs(host,route,{Value::Rotator(a),{}},{Value::Rotator(b),{}},limits);
    return Vm::Execute(host,Script(Return(AddExpression(route,Rot(a),Rot(b)))),"PlainObject",{},limits);
}
void Returned(const Host& h,const Vm::Result& r,const std::string& why){
    Require(r.passed()&&r.committed&&h.begins==1u&&h.commits==1u&&!h.rollbacks&&!h.active,
        why+": "+r.error);Require(h.delegated316==0u,"Native316 bypassed VM numeric implementation");
}
void Refused(const Host& h,const Vm::Result& r,const std::string& why){
    Require(!r.passed()&&!r.committed&&!r.error.empty()&&h.begins==1u&&!h.commits&&h.rollbacks==1u&&!h.active,
        why+": accepted or wrong root outcome: "+r.error);Require(h.delegated316==0u,"Refusal used Host arithmetic");++refusals;
}

void Arithmetic(){
    static_assert(sizeof(std::int32_t)==4u&&sizeof(Rotation)==12u);
    constexpr std::array<std::uint32_t,16u> words{0u,1u,0xffffffffu,0x7fffffffu,0x80000000u,0x0000ffffu,0x00010000u,
        0x00007fffu,0x00008000u,0xffff0000u,0x12345678u,0x89abcdefu,0x40000000u,0xc0000000u,0x00ffffffu,0xff000001u};
    const auto test=[](const Rotation& a,const Rotation& b){
        const auto expected=Expected(a,b);
        for(const auto route:Routes){Host host;const auto result=Literals(host,route,a,b);Returned(host,result,"Full-width signed32 mixed rotator sum");
            Same(result.value,expected,"Independent byte-carry Rotator addition differs");
            Require(result.writes==0u&&host.effects==0u,"Pure Rotator addition wrote world/script state");++arithmetic;}
    };
    for(std::size_t a=0u;a<words.size();++a)for(std::size_t b=0u;b<words.size();++b){
        Rotation left{},right{};for(unsigned i=0u;i<3u;++i){left[i]=std::bit_cast<std::int32_t>(words[(a+5u*i)%words.size()]);
            right[i]=std::bit_cast<std::int32_t>(words[(b+7u*i)%words.size()]);}test(left,right);
    }
    std::uint64_t random=0xa1b2c3d4e5f60718ull;
    const auto word=[&]{random^=random<<13u;random^=random>>7u;random^=random<<17u;return static_cast<std::uint32_t>(random>>19u);};
    for(unsigned n=0u;n<4096u;++n){Rotation a{},b{};for(unsigned i=0u;i<3u;++i){a[i]=std::bit_cast<std::int32_t>(word());b[i]=std::bit_cast<std::int32_t>(word());}test(a,b);}
    Require(AddWord(0x7fffffffu,1u)==0x80000000u&&AddWord(0xffffffffu,1u)==0u&&AddWord(0x80000000u,0x80000000u)==0u,
        "Independent byte-carry oracle lost signed overflow/full carry controls");
}

void CopiedInputs(){
    const Rotation a{std::numeric_limits<std::int32_t>::max(),-123456789,70000},b{9,10,-11};
    const Rotation mutated{-4444,5555,-6666};
    for(const auto route:Routes){Host host;host.state.at("Generated.A")=Value::Rotator(a);host.replacement=Value::Rotator(mutated);
        host.bResult=Value::Rotator(b);host.mutateB=true;Vm::Result result;
        if(route==Route::Opcode||route==Route::Final){
            result=Vm::Execute(host,Script(Return(AddExpression(route,Ref(0x01u,1),Native(1000u)))),"PlainObject");
            Require(result.writes==1u,"B expression mutation escaped ordinary VM write accounting");
        }else{
            auto first=host.Reference(host.properties.at(1));auto second=std::make_shared<Vm::Reference>();second->zero=Value::Rotator({});second->resultLayout=Layout;
            second->read=[&]{host.trace.push_back("B");host.readsAtB=host.readsA;++host.effects;host.state.at("Generated.A")=host.replacement;return host.bResult;};
            // Direct arguments are already evaluated. The deliberately live B
            // getter mutates A during copying, independently of script Steps.
            result=Inputs(host,route,{{},first},{{},second});
        }
        Returned(host,result,"A copied before live B mutation");Same(result.value,Expected(a,b),"B retroactively changed copied A");
        Same(host.state.at("Generated.A"),mutated,"B mutation positive control did not actually change original A");
        // The scripted B mutation legitimately reads its previous A once for
        // the VM undo journal. That is distinct from reloading input A.
        const auto expectedReads=route==Route::Opcode||route==Route::Final?2u:1u;
        Require(host.effects==1u&&host.readsAtB==1u&&host.readsA==expectedReads,
            "Ordinary input retained/reloaded an A alias after B evaluation");
        const auto position=[&](const char* text){for(std::size_t i=0u;i<host.trace.size();++i)if(host.trace[i]==text)return i;return host.trace.size();};
        Require(position("layout:0")<position("readA")&&position("layout:1")<position("readA")&&position("readA")<position("B"),
            "Uninitialized layout capture/first Step/second Step order differs");++protocol;
    }
}

Bytes SourceExpression(unsigned form){
    const auto scalar=Ref(0x01u,1);
    const auto array=Element(Ref(0x01u,6),Ref(0x01u,3));
    const auto member=Member(Ref(0x01u,4));
    switch(form){
    case 0u:return scalar;
    case 1u:return Context({0x17u},scalar);
    case 2u:return Context(Ref(0x20u,9),scalar,0x12u);
    case 3u:return Skip(scalar);
    case 4u:return array;
    case 5u:return member;
    case 6u:return Context({0x17u},Skip(array));
    case 7u:return Skip(Context(Ref(0x20u,9),member,0x12u));
    default:throw std::runtime_error("Generated source form is outside fixed cohort");
    }
}
Bytes StaleExpression(unsigned form){
    const auto stale=Native(1002u);
    switch(form){
    case 0u:return stale;
    case 1u:return Context({0x17u},stale);
    case 2u:return Context(Ref(0x20u,9),stale,0x12u);
    case 3u:return Skip(stale);
    case 4u:return Element(Int(1),stale);
    case 5u:return Member(stale);
    case 6u:return Context({0x17u},Skip(Element(Int(1),stale)));
    case 7u:return Skip(Context(Ref(0x20u,9),Member(stale),0x12u));
    default:throw std::runtime_error("Generated stale source form is outside fixed cohort");
    }
}
void ConfigureSource(Host& host,unsigned form,const Rotation& value){
    host.changingRead=true;
    if(form==4u||form==6u){host.sourceStorage=1u;host.state.at("Generated.Array1")=Value::Rotator(value);}
    else if(form==5u||form==7u){host.sourceStorage=2u;host.state.at("Generated.Pair").fields.at("value")=Value::Rotator(value);}
    else host.state.at("Generated.A")=Value::Rotator(value);
}
void TransparentCopiedInputs(){
    const Rotation initial{0x12345678,std::numeric_limits<std::int32_t>::min(),-987654321},second{-101,202,-303};
    const Rotation changed{4444,-5555,6666};
    for(const auto route:{Route::Opcode,Route::Final})for(unsigned form=0u;form<8u;++form){
        Host host;ConfigureSource(host,form,initial);host.mutateB=true;host.replacement=Value::Rotator(changed);host.bResult=Value::Rotator(second);
        // A second read would return a DIFFERENT first operand, not merely
        // increment a counter. The ordinary B journal read occurs only later.
        const auto result=Vm::Execute(host,Script(Return(AddExpression(route,SourceExpression(form),Native(1000u)))),"PlainObject");
        Returned(host,result,"Transparent Context/ClassContext/Skip/array/member copied source");
        Same(result.value,Expected(initial,second),"A source was reloaded through a transparent/lvalue wrapper");
        Require(result.writes==1u&&host.effects==1u&&host.readsAtB==1u&&host.readsA==2u,
            "Native316 source was not copied ONCE before B plus its independent undo-journal read");
        Require(host.arrayBaseReads==0u,"Fixed-array address selection read its unrelated base value");
        Require(host.indexReads==(host.sourceStorage==1u?1u:0u),
            "Fixed-array index getter was not copied once before selected element addressing");
        Require(host.parentReads==(host.sourceStorage==2u?2u:0u),
            "StructMember redundantly loaded parent before its final source/undo-journal reads");
        Require(host.classSelections==((form==2u||form==7u)?1u:0u),
            "ClassContext selected its default block more than once or skipped it");
        const Value& published=host.sourceStorage==1u?host.state.at("Generated.Array1"):
            host.sourceStorage==2u?host.state.at("Generated.Pair").fields.at("value"):host.state.at("Generated.A");
        Same(published,changed,"Wrapped-source B mutation did not actually change the selected original storage");
        Require(host.state.at("Generated.Pair").fields.at("sentinel").integer==93,
            "StructMember mutation disturbed another authored member");++protocol;
    }
    for(const auto route:{Route::Opcode,Route::Final})for(unsigned form=0u;form<8u;++form)for(unsigned slot=0u;slot<2u;++slot){
        Host host;ConfigureSource(host,form,initial);host.mutateB=true;
        const auto first=slot?SourceExpression(form):StaleExpression(form);
        const auto secondInput=slot?StaleExpression(form):Native(1000u);
        const auto result=Vm::Execute(host,Script(Return(AddExpression(route,first,secondInput))),"PlainObject");
        Refused(host,result,"NoWrite aliases through transparent Context/array/member inputs");
        Require(host.staleReads==0u&&host.staleElements==0u&&host.effects==0u&&result.writes==0u,
            "NoWrite copied/address destination read a stale alias, selected its array element, or evaluated later B");
        Require(host.readsA==(slot?1u:0u)&&host.arrayBaseReads==0u&&host.parentReads==(slot&&host.sourceStorage==2u?1u:0u),
            "NoWrite wrapped refusal loaded an inactive source or redundantly loaded a defined A");++protocol;
    }
}

Bytes NestedInput(Bytes value,unsigned nesting){
    if(nesting==0u)return value;
    if(nesting==1u)return Skip(value);
    return Context({0x17u},Skip(value));
}
void InitializedSelectorsAndIndices(){
    const Rotation other{17,-19,23};
    // These are ORIGINAL initialized selector destinations, unlike native316's
    // uninitialized12-byte operands. Object selectors begin as None. Native
    // mock results deliberately carry stale nonnull payloads and aliases.
    for(const auto route:{Route::Opcode,Route::Final})for(unsigned slot=0u;slot<2u;++slot)
        for(unsigned nesting=0u;nesting<3u;++nesting)for(const auto opcode:{std::uint8_t{0x19u},std::uint8_t{0x12u}})
        for(unsigned address=0u;address<3u;++address)for(const int clear:{-1,0,1,2,3,4,5,8,12,255}){
            Host host;host.selectorPayload=Value::Text(Vm::Kind::Object,opcode==0x12u?"GeneratedClass":"PlainObject");
            host.selectorDisposition=clear<0?Vm::ResultDisposition::NoWrite:Vm::ResultDisposition::ZeroFill;
            host.selectorClear=static_cast<std::uint8_t>(clear<0?0:clear);
            // In Copy, a null selector clears the operand's entire12-byte
            // result and does NOT evaluate its side-effecting child. In
            // Address, no array/struct base exists, hence no final source.
            const auto child=address==0u?Native(1005u):address==1u?Ref(0x01u,3):Ref(0x01u,4);
            auto input=Context(NestedInput(Native(1003u),nesting),child,opcode,12u);
            if(address==1u)input=Element(Int(1),input);
            if(address==2u)input=Member(input);
            input=NestedInput(input,nesting);
            const auto result=Vm::Execute(host,Script(Return(AddExpression(route,slot?Rot(other):input,slot?input:Rot(other)))),"PlainObject");
            const bool completeSelector=clear<0||clear==0||clear==4;
            if(completeSelector&&address==0u){
                Returned(host,result,"None-initialized Object selector leaves full operand zero");
                Same(result.value,other,"NoWrite/full4 Object selector reused stale nonnull identity or Rotator payload");
            }else Refused(host,result,address?"Missing initialized-selector address cannot supply native Rotator":
                "Partial/excess Object pointer clear is explicitly unsupported");
            Require(host.selectorCalls==1u&&host.selectorAliasReads==0u&&host.indexAliasReads==0u&&
                host.childEffects==0u&&host.classSelections==0u&&host.arrayVariables==0u&&host.pairVariables==0u&&
                host.arrayBaseReads==0u&&host.elementReads==std::array<std::size_t,2u>{}&&host.parentReads==0u&&result.writes==0u,
                "Unwritten/cleared Object selector evaluated a child, selected a class/base, or read stale storage");
            ++protocol;
        }

    // Written selectors are the positive controls: exactly one selector read,
    // a real selected child/default block, and actual array/member values.
    for(const auto route:{Route::Opcode,Route::Final})for(unsigned slot=0u;slot<2u;++slot)
        for(unsigned nesting=0u;nesting<3u;++nesting)for(const auto opcode:{std::uint8_t{0x19u},std::uint8_t{0x12u}})
        for(unsigned address=0u;address<3u;++address)for(const bool none:{false,true}){
            Host host;host.writtenSelector=Value::Text(Vm::Kind::Object,none?"":opcode==0x12u?"GeneratedClass":"PlainObject");
            const auto child=address==0u?Native(1005u):address==1u?Ref(0x01u,3):Ref(0x01u,4);
            auto input=Context(NestedInput(Ref(0x01u,7),nesting),child,opcode,12u);
            if(address==1u)input=Element(Int(1),input);
            if(address==2u)input=Member(input);
            input=NestedInput(input,nesting);
            const auto result=Vm::Execute(host,Script(Return(AddExpression(route,slot?Rot(other):input,slot?input:Rot(other)))),"PlainObject");
            if(none&&address!=0u)Refused(host,result,"Written None cannot supply an array/struct address");
            else{
                Returned(host,result,"Written Context/ClassContext selector selects its real child");
                const Rotation selected=none?Rotation{}:address==0u?host.childResult.rotation:
                    address==1u?host.state.at("Generated.Array1").rotation:host.state.at("Generated.Pair").fields.at("value").rotation;
                Same(result.value,Expected(selected,other),"Written selector chose stale second read or wrong lvalue source");
            }
            Require(host.selectorReads==1u&&host.classSelections==(!none&&opcode==0x12u?1u:0u)&&
                host.childEffects==(!none&&address==0u?1u:0u)&&host.arrayVariables==(!none&&address==1u?1u:0u)&&
                host.pairVariables==(!none&&address==2u?1u:0u)&&host.arrayBaseReads==0u&&
                host.elementReads[1u]==(!none&&address==1u?1u:0u)&&host.elementReads[0u]==0u&&
                host.parentReads==(!none&&address==2u?1u:0u)&&result.writes==0u,
                "Written selector was reloaded, child was repeated/skipped, or address construction read extra source data");
            ++protocol;
        }

    // ArrayElement's INT temporary starts at0. Therefore NoWrite, clear0,
    // and partial1..3-byte clears all KEEP0; this is not the object-pointer
    // partial-write policy above. Stale payload1 would select another item.
    for(const auto route:{Route::Opcode,Route::Final})for(unsigned slot=0u;slot<2u;++slot)
        for(unsigned nesting=0u;nesting<3u;++nesting)for(unsigned base=0u;base<3u;++base)
        for(const int clear:{-1,0,1,2,3,4,5,8,12,255}){
            Host host;host.indexDisposition=clear<0?Vm::ResultDisposition::NoWrite:Vm::ResultDisposition::ZeroFill;
            host.indexClear=static_cast<std::uint8_t>(clear<0?0:clear);
            auto array=Ref(0x01u,3);
            if(base==1u)array=Context({0x17u},Skip(array));
            if(base==2u)array=Context(Ref(0x20u,9),Skip(array),0x12u);
            auto input=NestedInput(Element(NestedInput(Native(1004u),nesting),array),nesting);
            const auto result=Vm::Execute(host,Script(Return(AddExpression(route,slot?Rot(other):input,slot?input:Rot(other)))),"PlainObject");
            const bool valid=clear<0||clear<=4;
            if(valid){
                Returned(host,result,"Zero-initialized index selects actual element0");
                Same(result.value,Expected(host.state.at("Generated.Array0").rotation,other),
                    "Initialized index reused stale payload1 instead of actual element0");
            }else Refused(host,result,"Index clear exceeds original four-byte temporary");
            Require(host.indexCalls==1u&&host.indexAliasReads==0u&&host.selectorAliasReads==0u&&
                host.arrayBaseReads==0u&&host.arrayVariables==(valid?1u:0u)&&
                host.elementReads[0u]==(valid?1u:0u)&&host.elementReads[1u]==0u&&
                host.classSelections==(valid&&base==2u?1u:0u)&&result.writes==0u,
                "Initialized index read stale alias, evaluated an inactive base, or chose/reloaded the wrong element");
            ++protocol;
        }
    for(const auto route:{Route::Opcode,Route::Final})for(unsigned slot=0u;slot<2u;++slot)
        for(unsigned nesting=0u;nesting<3u;++nesting)for(unsigned base=0u;base<3u;++base){
            Host host;auto array=Ref(0x01u,3);
            if(base==1u)array=Context({0x17u},Skip(array));
            if(base==2u)array=Context(Ref(0x20u,9),Skip(array),0x12u);
            auto input=NestedInput(Element(NestedInput(Ref(0x01u,6),nesting),array),nesting);
            const auto result=Vm::Execute(host,Script(Return(AddExpression(route,slot?Rot(other):input,slot?input:Rot(other)))),"PlainObject");
            Returned(host,result,"Written index1 copied before a second getter could change it to0");
            Same(result.value,Expected(host.state.at("Generated.Array1").rotation,other),"Written index1 did not select actual different element1");
            Require(host.indexReads==1u&&host.elementReads[1u]==1u&&host.elementReads[0u]==0u&&host.arrayBaseReads==0u&&
                host.arrayVariables==1u&&host.classSelections==(base==2u?1u:0u)&&result.writes==0u,
                "Written index/source was not copied once through nested address selection");++protocol;
        }
}

void DefinedDestinations(){
    const Rotation value{0x12345678,-77777,std::numeric_limits<std::int32_t>::min()};
    for(const bool hooked:{false,true})for(const auto route:Routes)for(unsigned slot=0u;slot<2u;++slot){
        // A complete12-byte null Context is a written zero. Every other clear
        // width, and NoWrite/Nothing, leaves an undefined native destination.
        for(const int width:{-2,-1,0,1,2,3,4,7,8,11,12,13,16,255}){
            Host host;host.layoutHook=hooked;bool staleRead{};Vm::Result result;
            if(route==Route::Opcode||route==Route::Final){
                const auto input=width<0?Bytes{0x0bu}:NullContext(static_cast<std::uint8_t>(width));
                result=Vm::Execute(host,Script(Return(AddExpression(route,slot?Rot(value):input,slot?input:Rot(value)))),"PlainObject");
            }else{
                auto stale=std::make_shared<Vm::Reference>();stale->zero=Value::Rotator({});stale->resultLayout=Layout;
                stale->read=[&]()->Value{staleRead=true;throw std::runtime_error("Unwritten Rotator stale alias read");};
                Vm::Evaluation input{Value::Vector({9,8,7}),stale,width==-2?Vm::ResultDisposition::Written:
                    width==-1?Vm::ResultDisposition::NoWrite:Vm::ResultDisposition::ZeroFill,
                    static_cast<std::uint8_t>(width<0?0:width)};
                if(width==-2)input={}; // Written Nothing is not three defined INTs either.
                result=Inputs(host,route,slot?Vm::Evaluation{Value::Rotator(value),{}}:input,
                    slot?input:Vm::Evaluation{Value::Rotator(value),{}});
            }
            Require(!staleRead,"Incomplete/full zero-fill destination consulted a stale attached alias");
            if(width==12){Returned(host,result,"Exactly12 bytes establish zero Rotator");Same(result.value,value,"Full zero-fill did not materialize canonical zero Rotator");}
            else Refused(host,result,"NoWrite/Nothing/partial/excess Rotator destination");
            ++protocol;
        }
    }
    // B must not be evaluated after A is already known undefined. Conversely,
    // a side effect in A remains inside the same rollback root if B is unwritten.
    for(const auto route:{Route::Opcode,Route::Final}){
        Host host;host.layoutHook=false;host.mutateB=true;
        const auto badFirst=Vm::Execute(host,Script(Return(AddExpression(route,{0x0bu},Native(1000u)))),"PlainObject");
        Refused(host,badFirst,"Unwritten A before B expression");Require(host.effects==0u,"B evaluated after undefined A refusal");
        Host late;late.layoutHook=false;late.mutateB=true;const auto before=late.state;
        const auto badSecond=Vm::Execute(late,Script(Return(AddExpression(route,Native(1000u),{0x0bu}))),"PlainObject");
        Refused(late,badSecond,"Unwritten B after A mutation");Require(badSecond.writes==1u&&late.effects==0u&&SameState(late.state,before),
            "Unwritten B failed to restore prior A-expression write/effect");++rollbackCases;
    }
}

void TypedStorage(){
    std::vector<Value> wrong{Value{},Value::Integer(1),Value::Vector({1,2,3}),Value::IntArray({1,2,3})};
    Value lookalike;lookalike.kind=Vm::Kind::Struct;lookalike.fields={{"pitch",Value::Integer(1)},{"yaw",Value::Integer(2)},{"roll",Value::Integer(3)}};
    wrong.push_back(lookalike);
    auto decorated=Value::Rotator({1,2,3});decorated.text="not-12-byte-storage";wrong.push_back(decorated);
    decorated=Value::Rotator({1,2,3});decorated.fields.emplace("extra",Value::Integer(4));wrong.push_back(decorated);
    decorated=Value::Rotator({1,2,3});decorated.intArray={4};wrong.push_back(decorated);
    for(const auto route:Routes)for(unsigned slot=0u;slot<2u;++slot)for(const auto& bad:wrong){Host host;host.customResult=bad;Vm::Result result;
        if(route==Route::Opcode||route==Route::Final){const auto operand=Native(1001u);
            result=Vm::Execute(host,Script(Return(AddExpression(route,slot?Rot({5,6,7}):operand,slot?operand:Rot({5,6,7})))),"PlainObject");}
        else result=Inputs(host,route,slot?Vm::Evaluation{Value::Rotator({5,6,7}),{}}:Vm::Evaluation{bad,{}},
            slot?Vm::Evaluation{bad,{}}:Vm::Evaluation{Value::Rotator({5,6,7}),{}});
        Refused(host,result,"Noncanonical/incorrect12-byte Rotator argument");Require(result.writes==0u,"Typed argument refusal wrote state");++protocol;
    }
    for(const auto layout:{Vm::ResultLayout::Unknown,Vm::ResultLayout::CoreVector12,Vm::ResultLayout::CoreColor4})for(const auto route:Routes){
        Host host;host.suppliedLayout=layout;const auto result=Literals(host,route,{1,2,3},{4,5,6});
        Refused(host,result,"Conflicting engaged uninitialized layout capability");++protocol;
    }
    for(const auto route:Routes){Host host;host.seedConflict=true;
        const auto result=Literals(host,route,{1,2,3},{4,5,6});Refused(host,result,"Initialized seed cannot establish uninitialized native316 packet");++protocol;
    }
    // Direct/reflected declarations may not relabel a Rotator destination as
    // a Vector even though both are12 bytes. This is capability, not shape.
    for(const auto route:{Route::Final,Route::Direct,Route::Execution})for(unsigned field=0u;field<3u;++field){
        Host host;host.declaration.variables[field].resultLayout=Vm::ResultLayout::CoreVector12;
        const auto result=Literals(host,route,{1,2,3},{4,5,6});Refused(host,result,"Wrong declaration result/input layout");++protocol;
    }
    for(const auto route:Routes)for(const unsigned count:{0u,1u,3u}){Host host;Vm::Result result;
        if(route==Route::Opcode||route==Route::Final){Bytes expression;
            if(count==0u)expression=route==Route::Opcode?Native(316u):Final({});
            if(count==1u)expression=route==Route::Opcode?Native(316u,{Rot({1,2,3})}):Final({Rot({1,2,3})});
            if(count==3u)expression=route==Route::Opcode?Native(316u,{Rot({1,2,3}),Rot({4,5,6}),Rot({7,8,9})}):Final({Rot({1,2,3}),Rot({4,5,6}),Rot({7,8,9})});
            result=Vm::Execute(host,Script(Return(expression)),"PlainObject");
        }else{std::vector<Vm::Evaluation> args(count,{Value::Rotator({1,2,3}),{}});
            result=route==Route::Direct?Vm::Execute(host,host.declaration,"PlainObject",args):
                Vm::ExecuteTransaction(host,[&](Vm::Execution& e){return e.CallFunction(host.declaration,"PlainObject",args);});}
        Refused(host,result,"Native316 exact two-input arity");++protocol;
    }
}

void BudgetsAndRollback(){
    for(const auto route:Routes)for(unsigned budget=0u;budget<6u;++budget){Host host;Vm::Limits limits;
        switch(budget){case 0u:limits.instructions=0u;break;case 1u:limits.arguments=1u;break;
        case 2u:limits.retainedBytes=1u;break;case 3u:limits.callDepth=0u;break;
        case 4u:limits.operations=0u;break;default:limits.localElements=0u;break;}
        const auto result=Literals(host,route,{1,2,3},{4,5,6},limits);
        if(budget==4u&&route!=Route::Execution){
            // operations counts explicit Execution entries, not pure builtin
            // arithmetic or read-only metadata hooks in ordinary Vm::Execute.
            Returned(host,result,"Pure native316 needs no explicit operation entry");
            Same(result.value,Expected({1,2,3},{4,5,6}),"Operation0 control changed pure native result");
        }else{
            Refused(host,result,"Ordinary shared VM budget");
            Require(result.status==Vm::Status::Budget,"Rotator budget failure has wrong classification");
        }
        ++protocol;
    }
    for(const auto route:{Route::Opcode,Route::Final})for(unsigned failure=0u;failure<3u;++failure){
        Host host;host.mutateB=true;host.failB=failure==2u;const auto before=host.state;
        const auto sum=AddExpression(route,Ref(0x01u,1),Native(1000u));
        Bytes code=Let(Ref(0x01u,2),sum);if(failure==0u)code=Join({code,Return(Native(4095u))});else code=Join({code,Return(Ref(0x01u,2))});
        Vm::Limits limits;if(failure==1u)limits.writes=1u;
        const auto result=Vm::Execute(host,Script(code),"PlainObject",{},limits);
        Refused(host,result,"Late/shared-root Rotator failure");
        Require(SameState(host.state,before)&&host.effects==0u&&result.writes==(failure==0u?2u:1u),
            "Late Rotator failure retained first-input/return write or effect, or failed too early");
        Require(host.readsA>=1u,"Late rollback fixture never read genuine first input");++rollbackCases;
    }
    for(const bool caught:{false,true}){Host host;const auto before=host.state;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& e){
            e.WriteReference(host.Reference(host.properties.at(1)),Value::Rotator({77,88,99}));++host.effects;
            const auto invoke=[&]{return e.CallFunction(host.declaration,"PlainObject",{{Value::Rotator({1,2,3}),{}},
                {Value::Rotator({4,5,6}),{},Vm::ResultDisposition::NoWrite}});};
            if(caught){try{static_cast<void>(invoke());}catch(const std::exception&){}return Value::Bool(true);}return invoke();
        });
        Refused(host,result,"Nested invalid native316 poisons even caught caller root");
        Require(result.writes==1u&&SameState(host.state,before)&&host.effects==0u,"Nested Rotator refusal reset budgets or retained prior write/effect");++rollbackCases;
    }
}
} // namespace

int main(){try{
    Arithmetic();CopiedInputs();TransparentCopiedInputs();InitializedSelectorsAndIndices();DefinedDestinations();TypedStorage();BudgetsAndRollback();
    std::cout<<"PASS original Core native316: "<<checks<<" checks, "<<arithmetic<<" independent byte-carry arithmetic cases, "
        <<protocol<<" copied-input/layout/protocol cases, "<<refusals<<" refusals, "<<rollbackCases
        <<" mock-host shared-root rollback cases; raw opcode, reflected final, direct Function and Execution entry. "
        "Canonical12-byte Rotator only; no commercial execution or actual-map/startup claim.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
