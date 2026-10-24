#include "quest_portable_vm.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace Vm=QuestVr::Vm;
using Vm::Value;using Vm::Evaluation;using Vm::Kind;using Vm::ResultLayout;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};
void Require(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
void U32(Bytes& b,std::uint32_t n){for(unsigned i=0u;i<4u;++i)b.push_back(static_cast<std::uint8_t>(n>>(8u*i)));}
Bytes Ref(std::uint8_t op,std::int32_t ref){Bytes b{op};U32(b,static_cast<std::uint32_t>(ref));return b;}
Bytes Join(std::initializer_list<Bytes> parts){Bytes b;for(const auto& p:parts)b.insert(b.end(),p.begin(),p.end());return b;}
Bytes Native(std::uint16_t n,std::initializer_list<Bytes> args={}){
    Bytes b=n<256u?Bytes{static_cast<std::uint8_t>(n)}:Bytes{static_cast<std::uint8_t>(0x60u+(n>>8u)),static_cast<std::uint8_t>(n)};
    for(const auto& a:args)b.insert(b.end(),a.begin(),a.end());
    b.push_back(0x16u);return b;
}
Bytes Final(std::initializer_list<Bytes> args){auto b=Ref(0x1cu,1);for(const auto& a:args)b.insert(b.end(),a.begin(),a.end());b.push_back(0x16u);return b;}
Bytes Context(Bytes receiver,Bytes child,std::uint8_t fill=0u){
    Require(child.size()<65536u,"Context fixture too large");auto b=Join({{0x19u},receiver});
    b.push_back(static_cast<std::uint8_t>(child.size()));b.push_back(static_cast<std::uint8_t>(child.size()>>8u));
    b.push_back(fill);return Join({b,child});
}
Vm::Function Wrapper(Bytes call){Vm::Function f;f.path="Test.Run";f.source="Test";f.bytecode=Join({call,{0x04u,0x0bu}});return f;}
Vm::Function Declaration(){
    Vm::Function f;f.path="Core.Object.GetAxes";f.source="Core";f.flags=0x2401u;f.nativeIndex=229u;
    f.variables.push_back({"A","A",Value::Rotator({}),0x80u,1u,ResultLayout::CoreRotator12});
    for(const auto* name:{"X","Y","Z"})f.variables.push_back({name,name,Value::Vector({}),0x180u,1u,ResultLayout::CoreVector12});
    return f;
}
struct Host final:Vm::Host {
    std::map<std::string,std::vector<Value>> data,saved;
    std::map<std::int32_t,Vm::Property> properties;
    std::vector<std::string> addresses,receivers,writes;
    std::size_t axesCalls{},sideCalls{},outReads{},rotationReads{},captures{},capturedAtEntry{};
    bool entered{},active{},conflict{},unknown{},badInputLayout{},tryHookWrite{};
    Vm::Execution* current{};
    Host(){
        properties[1]={"A","A",Value::Rotator({}),0u,1u,ResultLayout::CoreRotator12};
        properties[2]={"V","V",Value::Vector({}),0u,3u,ResultLayout::CoreVector12};
        properties[3]={"O","O",Value::Text(Kind::Object,{}),0u,1u,ResultLayout::Unknown};
        for(const auto* receiver:{"Self","Other"}){
            data[std::string(receiver)+".A"]={Value::Rotator({1,2,3})};
            data[std::string(receiver)+".V"]={Value::Vector({-1,-1,-1}),Value::Vector({-2,-2,-2}),Value::Vector({-3,-3,-3})};
            data[std::string(receiver)+".O"]={Value::Text(Kind::Object,"Other")};
        }
    }
    void Begin()override{Require(!active,"Nested packet root");saved=data;active=true;entered=false;}
    void Commit()override{active=false;}
    void Rollback()noexcept override{data.swap(saved);active=false;}
    Vm::Property ResolveProperty(const Vm::Function&,std::int32_t n)override{return properties.at(n);}
    std::string ResolveName(const Vm::Function&,std::int32_t)override{throw std::runtime_error("Unexpected name");}
    std::string ResolveObject(const Vm::Function&,std::int32_t)override{throw std::runtime_error("Unexpected object");}
    std::shared_ptr<const Vm::Function> ResolveFunction(const Vm::Function&,const std::string&,const Vm::Invocation&)override{
        return std::make_shared<Vm::Function>(Declaration());
    }
    std::shared_ptr<Vm::Reference> Address(const std::string& key,std::size_t slot=0u){
        const bool rot=key.ends_with(".A"),vec=key.ends_with(".V");
        auto r=std::make_shared<Vm::Reference>();r->zero=rot?Value::Rotator({}):vec?Value::Vector({}):Value::Text(Kind::Object,{});
        r->dimension=vec?3u:1u;r->resultLayout=rot?ResultLayout::CoreRotator12:vec?ResultLayout::CoreVector12:ResultLayout::Unknown;
        r->read=[this,key,slot,rot,vec]{
            if(rot)++rotationReads;
            if(vec){++outReads;Require(entered,"Old OUT contents read before native entry");}
            return data.at(key).at(slot);
        };
        r->write=[this,key,slot](const Value& value){data.at(key).at(slot)=value;writes.push_back(key+std::to_string(slot));};
        r->element=[this,key](std::size_t i){return Address(key,i);};return r;
    }
    std::shared_ptr<Vm::Reference> Variable(const std::string& receiver,const Vm::Property& p,Vm::Scope)override{
        addresses.push_back(receiver+'.'+p.key);return Address(receiver+'.'+p.key);
    }
    std::optional<ResultLayout> NativeArgumentUninitializedLayout(std::uint16_t n,const std::string&,std::size_t slot,const Vm::Function*)override{
        if(n!=229u)return std::nullopt;
        ++captures;
        return slot==0u?std::optional{badInputLayout?ResultLayout::CoreVector12:ResultLayout::CoreRotator12}:std::nullopt;
    }
    std::optional<ResultLayout> NativeArgumentAddressOnlyLayout(std::uint16_t n,const std::string&,std::size_t slot,const Vm::Function*)override{
        if(n!=229u)return std::nullopt;
        ++captures;
        if(tryHookWrite&&current)current->WriteReference(Address("Self.A"),Value::Rotator({9,9,9}));
        return slot?std::optional{unknown?ResultLayout::Unknown:ResultLayout::CoreVector12}:std::nullopt;
    }
    std::optional<Value> NativeArgumentSeed(std::uint16_t n,const std::string&,std::size_t slot,const Vm::Function*)override{
        return conflict&&n==229u&&slot==1u?std::optional{Value::Vector({})}:std::nullopt;
    }
    Evaluation Native(std::uint16_t,const std::string&,const std::vector<Evaluation>&,const Vm::Function*)override{
        throw std::runtime_error("Missing same-root packet execution");
    }
    Evaluation NativeWithExecution(std::uint16_t n,const std::string& receiver,const std::vector<Evaluation>& args,
        const Vm::Function*,Vm::Execution& execution)override{
        if(n==900u){++sideCalls;execution.WriteReference(Address("Self.A"),Value::Rotator({9,8,7}));return {Value::Integer(0),{}};}
        if(n!=229u)throw std::runtime_error("Late packet failure");
        ++axesCalls;entered=true;capturedAtEntry=captures;receivers.push_back(receiver);
        Require(args.size()==4u&&!args[0].reference&&args[0].value.kind==Kind::Rotator,"Rotation was not detached before native entry");
        const auto a=args[0].value.rotation;
        const std::array<Value,3u> axes{Value::Vector({static_cast<float>(a[0]),0,0}),Value::Vector({0,static_cast<float>(a[1]),0}),Value::Vector({0,0,static_cast<float>(a[2])})};
        for(std::size_t i=0u;i<3u;++i)if(args[i+1u].reference)args[i+1u].reference->write(axes[i]);
        return {};
    }
};
Bytes Element(Bytes index){return Join({{0x1au},index,Ref(0x01u,2)});}
Bytes Small(std::uint8_t n){return {0x2cu,n};}
void Passed(const Vm::Result& result){Require(result.passed()&&result.committed,"Mixed OUT packet failed");}
void Refused(Host& host,const std::function<Vm::Result()>& run){
    const auto before=host.data;const auto r=run();Require(!r.passed()&&!r.committed,"Unsafe OUT packet accepted");
    Require(host.data.size()==before.size(),"Packet undo changed storage");
    for(const auto& [key,values]:before)for(std::size_t i=0u;i<values.size();++i)Require(Vm::Equal(host.data.at(key).at(i),values[i]),"Packet refusal retained writes");
    ++refusals;
}
void Run(){
    for(bool final:{false,true}){
        Host host;const auto args=std::initializer_list<Bytes>{Ref(0x01u,1),Element(Native(900u)),Element(Small(1)),Element(Small(2))};
        auto f=Wrapper(final?Final(args):Native(229u,args));Passed(Vm::Execute(host,f,"Self"));
        Require(host.sideCalls==1u&&host.rotationReads==2u&&host.outReads==3u,"Input capture or OUT address evaluation repeated");
        Require(host.axesCalls==1u&&host.capturedAtEntry==8u,"Packet capabilities not captured once before arguments");
        Require(host.data.at("Self.V")[0].vector==std::array<float,3u>{1,0,0}&&host.data.at("Self.V")[2].vector==std::array<float,3u>{0,0,3},"Later OUT side effect changed captured rotation");
    }
    for(bool final:{false,true}){
        Host host;const auto args=std::initializer_list<Bytes>{Ref(0x01u,1),Ref(0x01u,2),Ref(0x01u,2),Ref(0x01u,2)};
        Passed(Vm::Execute(host,Wrapper(final?Final(args):Native(229u,args)),"Self"));
        Require(host.data.at("Self.V")[0].vector==std::array<float,3u>{0,0,3}&&host.outReads==3u,"Aliased OUT final-copy order differs");
    }
    {
        Host host;auto inner=Native(229u,{Ref(0x01u,1),Ref(0x01u,2),{0x0bu},{0x0bu}});
        Passed(Vm::Execute(host,Wrapper(Context(Ref(0x01u,3),inner)),"Self"));
        Require(host.receivers==std::vector<std::string>{"Other"}&&host.data.at("Other.V")[0].vector[0]==-1&&host.data.at("Self.V")[0].vector[0]==1,
            "Native packet used incoming Context instead of Frame.Object for arguments");
    }
    for(std::uint8_t fill:{0u,4u,8u,11u,12u,13u}){
        Host host;const auto input=Context({0x2au},Ref(0x01u,1),fill);
        auto f=Wrapper(Native(229u,{input,Ref(0x01u,2),{0x0bu},Context({0x2au},Ref(0x01u,2),4u)}));
        if(fill==12u){Passed(Vm::Execute(host,f,"Self"));Require(host.data.at("Self.V")[0].vector==std::array<float,3u>{},"Full Rotator null fill was not exact zero");}
        else Refused(host,[&]{return Vm::Execute(host,f,"Self");});
    }
    {
        Host host;auto args=std::vector<Evaluation>{{Value::Rotator({4,5,6}),{}},{Value::Vector({}),host.Address("Self.V")},{},{}};
        Passed(Vm::Execute(host,Declaration(),"Self",args));Require(host.outReads==1u&&host.rotationReads==0u,"Direct native eagerly read OUT");
        Require(host.data.at("Self.V")[0].vector[0]==4,"Direct packet lost actual output alias");
    }
    for(unsigned cut=0u;cut<3u;++cut){
        Host host;Vm::Limits limits;limits.writes=cut;
        auto f=Wrapper(Native(229u,{Ref(0x01u,1),Element(Small(0)),Element(Small(1)),Element(Small(2))}));
        Refused(host,[&]{return Vm::Execute(host,f,"Self",{},limits);});
    }
    {
        Host host;auto f=Wrapper(Join({Native(229u,{Ref(0x01u,1),Ref(0x01u,2),Ref(0x01u,2),Ref(0x01u,2)}),Native(999u)}));
        Refused(host,[&]{return Vm::Execute(host,f,"Self");});Require(host.axesCalls==1u&&host.outReads==3u,"Later failure did not follow real alias writes");
    }
    for(unsigned mode=0u;mode<3u;++mode){
        Host host;host.conflict=mode==0u;host.unknown=mode==1u;host.badInputLayout=mode==2u;
        auto f=Wrapper(Final({Ref(0x01u,1),Ref(0x01u,2),{0x0bu},{0x0bu}}));
        Refused(host,[&]{return Vm::Execute(host,f,"Self");});Require(host.addresses.empty()&&!host.axesCalls,"Hostile packet modes evaluated arguments");
    }
    {
        Host host;host.tryHookWrite=true;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){
            host.current=&execution;try{execution.CallFunction(Declaration(),"Self",{{Value::Rotator({}),{}},{},{},{}});}catch(...){}
            host.current=nullptr;return Value{};
        });
        Require(!result.passed()&&!result.committed&&host.data.at("Self.A")[0].rotation==std::array<std::int32_t,3u>{1,2,3},"Caught read-only mode hook write did not poison/undo root");++refusals;
    }
}
}
int main(){try{Run();std::cout<<"PASS mixed native OUT packet checks="<<checks<<" refusals="<<refusals<<'\n';return 0;}
catch(const std::exception& e){std::cerr<<"FAIL mixed native OUT packet: "<<e.what()<<'\n';return 1;}}
