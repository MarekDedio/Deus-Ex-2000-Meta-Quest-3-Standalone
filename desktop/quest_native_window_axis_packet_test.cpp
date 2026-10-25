#include "quest_portable_vm.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// EXPLICIT MOCK-HOST packet/transaction proof. The callback echoes its copied
// constraint to expose VM input/result semantics; it does NOT implement or
// prove native preferred sizing, an owned Window graph, startup or Quest UI.
namespace {
namespace Vm=QuestVr::Vm;
using Vm::Value;using Vm::Kind;using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},cases{},refusals{};
void Require(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
void U32(Bytes& bytes,std::uint32_t value){for(unsigned i=0u;i<4u;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(8u*i)));}
Bytes Join(std::initializer_list<Bytes> parts){Bytes result;for(const auto& part:parts)result.insert(result.end(),part.begin(),part.end());return result;}
Bytes Ref(std::uint8_t opcode,std::int32_t index){Bytes bytes{opcode};U32(bytes,std::bit_cast<std::uint32_t>(index));return bytes;}
Bytes Real(float value){Bytes bytes{0x1eu};U32(bytes,Bits(value));return bytes;}
Bytes Native(std::uint16_t index,const std::vector<Bytes>& arguments={}){
    Bytes bytes{static_cast<std::uint8_t>(0x60u+(index>>8u)),static_cast<std::uint8_t>(index)};
    for(const auto& argument:arguments)bytes.insert(bytes.end(),argument.begin(),argument.end());
    bytes.push_back(0x16u);return bytes;
}
Bytes Final(std::int32_t index,const std::vector<Bytes>& arguments={}){auto bytes=Ref(0x1cu,index);
    for(const auto& argument:arguments)bytes.insert(bytes.end(),argument.begin(),argument.end());
    bytes.push_back(0x16u);return bytes;}
Bytes Return(Bytes value={0x0bu}){return Join({{0x04u},value});}
Bytes Let(Bytes left,Bytes right){return Join({{0x0fu},left,right});}
Bytes Context(Bytes object,Bytes body,std::uint8_t clear=0u){Require(body.size()<65536u,"Oversized mock Context body");
    auto bytes=Join({{0x19u},object});bytes.push_back(static_cast<std::uint8_t>(body.size()));
    bytes.push_back(static_cast<std::uint8_t>(body.size()>>8u));bytes.push_back(clear);return Join({bytes,body});}
Vm::Function Script(const std::string& name,Bytes bytes){Vm::Function function;function.path="Mock."+name;
    function.source="Mock";function.bytecode=std::move(bytes);return function;}
Vm::Function Declaration(std::uint16_t index){
    Require(index==1463u||index==1464u,"Unknown scalar preferred-size mock declaration");
    Vm::Function function;function.path=index==1463u?"Extension.Window.QueryPreferredWidth":"Extension.Window.QueryPreferredHeight";
    function.source="Extension";function.flags=0x401u;function.nativeIndex=index;
    const std::string parameter=index==1463u?"queryHeight":"queryWidth";
    function.variables={{function.path+'.'+parameter,parameter,Value::Float(0.0f),0x80u,1u},
        {function.path+".ReturnValue","ReturnValue",Value::Float(0.0f),0x580u,1u}};
    return function;
}
enum class Entry {Raw,Declared,Direct};
const std::array<Entry,3u> Entries{Entry::Raw,Entry::Declared,Entry::Direct};
struct Host final:Vm::Host {
    std::uint16_t axis;std::map<std::string,Value> data,before;
    std::map<std::int32_t,Vm::Property> properties;
    std::map<std::int32_t,std::shared_ptr<const Vm::Function>> functions;
    std::vector<std::string> reads,writes,seeds,nativeReceivers;
    std::vector<Value> inputs;
    Vm::Evaluation childResult;
    std::size_t begins{},commits{},rollbacks{},axisCalls{},childCalls{},effects{},beforeEffects{};
    bool active{},journalRestored{},seedFailure{},mutateSource{},twoStores{},failSecondStore{},catchStoreFailure{};
    std::function<Value(const std::string&,const Value&)> onRead;
    explicit Host(std::uint16_t index):axis(index){
        properties[1]={"Constraint","Constraint",Value::Float(0.0f),0u,1u};
        properties[2]={"Result","Result",Value::Float(0.0f),0u,1u};
        for(const auto* receiver:{"Self","Other"}){
            data[std::string(receiver)+".Constraint"]=Value::Float(receiver==std::string("Self")?44.0f:77.0f);
            data[std::string(receiver)+".Result"]=Value::Float(-12.5f);
            data[std::string(receiver)+".CacheA"]=Value::Float(-0.0f);
            data[std::string(receiver)+".CacheB"]=Value::Float(-23.25f);
        }
        functions[7]=std::make_shared<Vm::Function>(Declaration(index));
    }
    void Begin()override{Require(!active,"Mock axis query nested its root");active=true;++begins;before=data;beforeEffects=effects;}
    void Commit()override{Require(active,"Mock axis query commit without root");active=false;++commits;}
    void Rollback()noexcept override{
        journalRestored=data.size()==before.size();
        for(const auto& [key,value]:before){const auto found=data.find(key);
            if(found==data.end()||found->second.kind!=value.kind||Bits(found->second.floating)!=Bits(value.floating))journalRestored=false;}
        // No property restoration here: the real VM journal must already have
        // restored every Float word, including the initial negative-zero cache.
        effects=beforeEffects;active=false;++rollbacks;
    }
    Vm::Property ResolveProperty(const Vm::Function&,std::int32_t index)override{return properties.at(index);}
    std::string ResolveName(const Vm::Function&,std::int32_t)override{throw std::runtime_error("Unexpected mock axis name lookup");}
    std::string ResolveObject(const Vm::Function&,std::int32_t index)override{Require(index==1,"Unknown mock axis receiver");return "Other";}
    std::shared_ptr<const Vm::Function> ResolveFunction(const Vm::Function&,const std::string&,const Vm::Invocation& invocation)override{
        return functions.at(invocation.reference);}
    std::shared_ptr<Vm::Reference> Address(const std::string& key){
        auto reference=std::make_shared<Vm::Reference>();reference->zero=Value::Float(0.0f);
        reference->read=[this,key]{reads.push_back(key);const auto value=data.at(key);return onRead?onRead(key,value):value;};
        reference->write=[this,key](const Value& value){
            if(value.kind!=Kind::Float||!std::isfinite(value.floating))throw std::runtime_error("Mock axis storage requires finite Float");
            writes.push_back(key);data.at(key)=value;
            if(failSecondStore&&key.ends_with(".CacheB")){failSecondStore=false;throw std::runtime_error("Mock axis partial second-store failure");}
        };return reference;
    }
    std::shared_ptr<Vm::Reference> Variable(const std::string& receiver,const Vm::Property& property,Vm::Scope)override{
        return Address(receiver+'.'+property.key);}
    std::optional<Value> NativeArgumentSeed(std::uint16_t index,const std::string& receiver,std::size_t slot,const Vm::Function* declaration)override{
        if(index!=axis)return std::nullopt;
        if(slot!=0u)throw std::runtime_error("Mock axis wrapper has exactly one required input");
        Require(!declaration||(declaration->nativeIndex==axis&&declaration->path==Declaration(axis).path),
            "Mock axis seed received the wrong declaration");
        seeds.push_back(receiver);if(seedFailure)throw std::runtime_error("Mock axis seed lookup failure");return Value::Float(0.0f);
    }
    Vm::Evaluation Native(std::uint16_t,const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*)override{
        throw std::runtime_error("Mock axis query lost its synchronous Execution");}
    Vm::Evaluation NativeWithExecution(std::uint16_t index,const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,
        const Vm::Function*,Vm::Execution& execution)override{
        if(index==4094u){++childCalls;Require(arguments.empty(),"Unexpected mock child packet");return childResult;}
        if(index==4093u){++effects;return {Value::Integer(1),{}};}
        if(index==4092u)throw std::runtime_error("Mock late preferred-axis fault");
        Require(index==axis,"Unexpected mock preferred-axis native");
        if(arguments.size()!=1u)throw std::runtime_error("Mock axis wrapper requires exactly one argument expression");
        const auto& argument=arguments[0u];
        Require(!argument.reference&&argument.disposition==Vm::ResultDisposition::Written&&!argument.zeroFillBytes,
            "Preferred-axis ordinary input retained an OUT alias or unwritten temporary");
        auto input=argument.value;if(input.kind==Kind::Nothing)input=Value::Float(0.0f);
        if(input.kind!=Kind::Float||!std::isfinite(input.floating)||!input.text.empty()||!input.fields.empty()||!input.intArray.empty())
            throw std::runtime_error("Mock preferred-axis input is not canonical finite Float storage");
        ++axisCalls;nativeReceivers.push_back(receiver);inputs.push_back(input);
        if(mutateSource){execution.WriteReference(Address("Self.Constraint"),Value::Float(99.0f));
            Require(argument.value.kind==Kind::Nothing||Bits(argument.value.floating)==Bits(input.floating),
                "Preferred-axis input changed after its source was written");}
        const auto stores=[&]{
            execution.WriteReference(Address(receiver+".CacheA"),Value::Float(12.5f));
            execution.WriteReference(Address(receiver+".CacheB"),Value::Float(-6.75f));
        };
        if(twoStores){if(catchStoreFailure){try{stores();}catch(...) {}}else stores();}
        // An always-Written Float result, not an OUT mutation. This echo is a
        // packet witness only; production QueryNativeWindowPreferredSize is
        // separately responsible for real cache/callback/fallback semantics.
        return {input,{}};
    }
};
Bytes Call(Entry entry,std::uint16_t index,const std::vector<Bytes>& arguments){
    return entry==Entry::Declared?Final(7,arguments):Native(index,arguments);}
Vm::Result Run(Host& host,Entry entry,Bytes operand,Vm::Evaluation argument){
    if(entry==Entry::Direct)return Vm::Execute(host,Declaration(host.axis),"Self",{std::move(argument)});
    return Vm::Execute(host,Script("AxisPacket",Return(Call(entry,host.axis,{std::move(operand)}))),"Self");
}
void Passed(const Vm::Result& result,const Host& host,float expected,std::size_t stores=0u){
    ++cases;Require(result.passed()&&result.committed&&result.value.kind==Kind::Float&&Bits(result.value.floating)==Bits(expected)&&
        result.writes==stores&&host.begins==1u&&host.commits==1u&&!host.rollbacks&&!host.active,
        "Preferred-axis packet/result/root mismatch: "+result.error);
}
void Failed(const Vm::Result& result,const Host& host){
    ++cases;++refusals;Require(!result.passed()&&!result.committed&&!result.error.empty()&&result.value.kind==Kind::Nothing&&
        host.begins==1u&&!host.commits&&host.rollbacks==1u&&!host.active&&host.journalRestored&&host.effects==host.beforeEffects,
        "Preferred-axis refusal did not completely undo its shared root: "+result.error);
}
void CopiedInputs(std::uint16_t index){
    for(const auto entry:Entries)for(const float input:{-91.5f,-0.0f,0.0f,44.0f,std::numeric_limits<float>::denorm_min()}){
        Host host(index);const auto result=Run(host,entry,Real(input),{Value::Float(input),{}});Passed(result,host,input);
        Require(host.axisCalls==1u&&host.seeds==std::vector<std::string>{"Self"}&&host.writes.empty(),
            "Preferred-axis input clamped, repeated its seed or wrote its input as an OUT");
    }
    for(const auto entry:Entries){
        Host host(index);std::size_t reads{};
        host.onRead=[&](const std::string& key,const Value& value){
            if(key=="Self.Constraint"&&++reads>1u)throw std::runtime_error("Preferred-axis input alias reloaded after its copied Step");
            return value;};
        const auto result=Run(host,entry,Ref(0x01u,1),{{},host.Address("Self.Constraint")});Passed(result,host,44.0f);
        Require(reads==1u&&host.data.at("Self.Constraint").floating==44.0f,"Preferred-axis input was not copied once/by value");
    }
    for(const auto entry:{Entry::Raw,Entry::Declared}){
        Host host(index);const auto result=Vm::Execute(host,Script("ChangedReceiver",Return(Context(Ref(0x20u,1),
            Call(entry,index,{Ref(0x01u,1)})))),"Self");Passed(result,host,44.0f);
        Require(host.nativeReceivers==std::vector<std::string>{"Other"}&&host.reads==std::vector<std::string>{"Self.Constraint"}&&
            host.seeds==std::vector<std::string>{"Other"},"Preferred-axis operand lost FFrame.Object or called the wrong Window receiver");
        Host nested(index);nested.functions[8]=std::make_shared<Vm::Function>(Script("Nested",Return(Call(entry,index,{Ref(0x01u,1)}))));
        const auto result2=Vm::Execute(nested,Script("Outer",Return(Context(Ref(0x20u,1),Final(8)))),"Self");Passed(result2,nested,77.0f);
        Require(nested.reads==std::vector<std::string>{"Other.Constraint"},"Preferred-axis nested call retained its caller's frame object");
    }
    for(const auto entry:Entries){
        Host host(index);host.mutateSource=true;
        const auto result=Run(host,entry,Ref(0x01u,1),{{},host.Address("Self.Constraint")});Passed(result,host,44.0f,1u);
        Require(host.data.at("Self.Constraint").floating==99.0f&&host.inputs.size()==1u&&host.inputs[0u].floating==44.0f,
            "Native callback source mutation retroactively changed the copied constraint");
    }
}
void InitializedFloat(std::uint16_t index){
    for(const auto entry:Entries){Host host(index);const auto result=Run(host,entry,{0x0bu},{});Passed(result,host,0.0f);}
    for(const auto entry:{Entry::Raw,Entry::Declared})for(unsigned clear=0u;clear<=4u;++clear){
        Host host(index);const auto operand=Context({0x2au},Native(4093u),static_cast<std::uint8_t>(clear));
        const auto result=Run(host,entry,operand,{});Passed(result,host,0.0f);
        Require(!host.childCalls&&!host.effects,"Null preferred-axis Context evaluated its skipped expression");
    }
    for(const auto entry:Entries)for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill})
        for(unsigned clear=0u;clear<=4u;++clear){
        Host host(index);std::size_t staleReads{};auto stale=std::make_shared<Vm::Reference>();stale->zero=Value::Float(0.0f);
        stale->read=[&]() ->Value {++staleReads;throw std::runtime_error("Preferred-axis unwritten stale alias read");};
        const Vm::Evaluation input{Value::Integer(999),stale,disposition,static_cast<std::uint8_t>(clear)};host.childResult=input;
        const auto result=Run(host,entry,Native(4094u),input);Passed(result,host,0.0f);
        Require(!staleReads&&host.axisCalls==1u&&host.childCalls==(entry==Entry::Direct?0u:1u),
            "Preferred-axis initialized Float read stale payload/reference or duplicated operand evaluation");
    }
    // Assignment is a legitimate operand side effect but returns NoWrite; the
    // native temporary stays +0 rather than becoming the assigned Float.
    for(const auto entry:{Entry::Raw,Entry::Declared}){
        Host host(index);const auto result=Run(host,entry,Let(Ref(0x01u,1),Real(-5.0f)),{});Passed(result,host,0.0f,1u);
        Require(host.data.at("Self.Constraint").floating==-5.0f,"Preferred-axis NoWrite operand skipped its actual assignment");
    }
    for(const auto entry:{Entry::Raw,Entry::Declared}){
        Host host(index);const auto script=Script("AlwaysWritesResult",Join({Let(Ref(0x01u,2),Call(entry,index,{{0x0bu}})),Return(Ref(0x01u,2))}));
        const auto result=Vm::Execute(host,script,"Self");Passed(result,host,0.0f,1u);
        Require(Bits(host.data.at("Self.Result").floating)==0u,"Preferred-axis NoWrite input suppressed its Float result store");
    }
}
void Refusals(std::uint16_t index){
    for(const auto entry:Entries)for(const unsigned clear:{5u,8u,255u}){
        Host host(index);std::size_t staleReads{};auto stale=std::make_shared<Vm::Reference>();stale->zero=Value::Float(0.0f);
        stale->read=[&]{++staleReads;return Value::Float(99.0f);};
        const Vm::Evaluation input{Value::Float(99.0f),stale,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(clear)};
        const auto caller=Script("OverWidth",Return(Call(entry,index,{Context({0x2au},Native(4093u),static_cast<std::uint8_t>(clear))})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){
            execution.CallFunction(Script("EarlierEffect",Join({Native(4093u),Return()})),"Self");
            try{if(entry==Entry::Direct)execution.CallFunction(Declaration(index),"Self",{input});else execution.CallFunction(caller,"Self");}
            catch(...){}return Value{};});Failed(result,host);
        Require(result.status==Vm::Status::Unsupported&&!staleReads&&!host.axisCalls,"Oversized axis clear read stale input, entered body or lost refusal");
    }
    std::vector<Value> invalid{Value::Integer(1),Value::Bool(true),Value::Byte(1),Value::Text(Kind::Object,{}),
        Value::Text(Kind::String,"bad"),Value::Vector({1,2,3}),Value::Rotator({1,2,3}),Value::Float(std::numeric_limits<float>::infinity()),
        Value::Float(-std::numeric_limits<float>::infinity()),Value::Float(std::numeric_limits<float>::quiet_NaN())};
    Value structure;structure.kind=Kind::Struct;structure.fields["height"]=Value::Float(12.0f);invalid.push_back(structure);
    auto malformed=Value::Float(12.0f);malformed.fields["height"]=Value::Float(12.0f);invalid.push_back(malformed);
    for(const auto entry:Entries)for(const auto& input:invalid){Host host(index);host.childResult={input,{}};
        const auto result=Run(host,entry,Native(4094u),{input,{}});Failed(result,host);Require(!host.axisCalls,"Malformed preferred-axis input entered accepted body");}
    for(const auto entry:Entries)for(const bool extra:{false,true}){
        Host host(index);std::vector<Vm::Evaluation> incoming;std::vector<Bytes> arguments;
        if(extra){incoming={{Value::Float(1.0f),{}},{Value::Float(2.0f),{}}};arguments={Real(1.0f),Real(2.0f)};}
        const auto result=entry==Entry::Direct?Vm::Execute(host,Declaration(index),"Self",incoming):
            Vm::Execute(host,Script("WrongCount",Return(Call(entry,index,arguments))),"Self");Failed(result,host);
        Require(!host.axisCalls,"Preferred-axis wrapper accepted a missing or excess argument");
    }
    for(const auto entry:Entries){
        Host host(index);host.seedFailure=true;std::size_t reads{};auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Float(0.0f);
        alias->read=[&]{++reads;return Value::Float(44.0f);};
        const auto result=Run(host,entry,Ref(0x01u,1),{{},alias});Failed(result,host);
        Require(!reads&&host.reads.empty()&&!host.axisCalls,"Preferred-axis seed failure evaluated or read its pending input");
    }
}
void Rollback(std::uint16_t index){
    for(const auto entry:Entries)for(const std::size_t cut:{0u,1u,2u}){
        Host host(index);host.twoStores=true;Vm::Limits limits;limits.writes=cut;
        const auto caller=Script("TwoWrites",Return(Call(entry,index,{Real(44.0f)})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){
            if(entry==Entry::Direct)return execution.CallFunction(Declaration(index),"Self",{{Value::Float(44.0f),{}}});
            return execution.CallFunction(caller,"Self");},limits);
        if(cut==2u){Passed(result,host,44.0f,2u);Require(host.data.at("Self.CacheA").floating==12.5f&&
            host.data.at("Self.CacheB").floating==-6.75f,"Successful same-root axis callback lost its stores");}
        else{Failed(result,host);Require(result.status==Vm::Status::Budget&&result.writes==cut,"Axis callback reset or miscounted its root write budget");}
    }
    for(const auto entry:Entries)for(const bool nativeFault:{false,true}){
        Host host(index);host.twoStores=true;const auto caller=Script("BeforeLateFailure",Return(Call(entry,index,{Real(44.0f)})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) ->Value {
            if(entry==Entry::Direct)execution.CallFunction(Declaration(index),"Self",{{Value::Float(44.0f),{}}});
            else execution.CallFunction(caller,"Self");
            if(nativeFault)return execution.CallFunction(Script("LaterFault",Join({Native(4092u),Return()})),"Self");
            throw std::runtime_error("Mock late preferred-axis coordinator failure");});Failed(result,host);
        Require(result.writes==2u&&host.axisCalls==1u,"Late axis failure lost completed same-root callback writes");
    }
    for(const auto entry:Entries)for(const bool caught:{false,true}){
        Host host(index);host.twoStores=true;host.failSecondStore=true;host.catchStoreFailure=caught;
        const auto result=Run(host,entry,Real(44.0f),{Value::Float(44.0f),{}});Failed(result,host);
        Require(host.axisCalls==1u&&result.writes==2u,"Partial/caught axis callback store fault did not poison its real root");
    }
    // Two completed native calls share one root; a later cutoff must undo the
    // first call, not retain an apparently successful preferred-size prefix.
    for(const std::size_t cut:{2u,3u}){
        Host host(index);host.twoStores=true;Vm::Limits limits;limits.writes=cut;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){
            execution.CallFunction(Declaration(index),"Self",{{Value::Float(12.0f),{}}});
            try{execution.CallFunction(Declaration(index),"Other",{{Value::Float(13.0f),{}}});}catch(...){}
            return Value::Float(999.0f);},limits);Failed(result,host);
        Require(result.status==Vm::Status::Budget&&result.writes==cut&&host.axisCalls==2u,
            "Later caught axis write cutoff committed an earlier call or reset its budget");
    }
}
} // namespace
int main(){try{for(const std::uint16_t index:{1463u,1464u}){CopiedInputs(index);InitializedFloat(index);Refusals(index);Rollback(index);}
    std::cout<<"PASS EXPLICIT MOCK-HOST Window1463/1464 VM packets: "<<checks<<" checks / "<<cases<<" cases / "<<refusals
        <<" refusals; raw/declared/direct copied Float0 inputs, Frame.Object, one-read aliases, initialized null prefixes, "
        "always-written by-value Float results and exact same-root write/late/caught-fault rollback. NOT owned sizing/startup/Quest acceptance\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL mock Window preferred-axis packet: "<<error.what()<<'\n';return 1;}}
