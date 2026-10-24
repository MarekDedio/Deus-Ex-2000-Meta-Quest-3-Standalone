#include "quest_portable_vm.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace Vm=QuestVr::Vm;
using Vm::Evaluation;using Vm::Kind;using Vm::ResultLayout;using Vm::Value;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};
void Require(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
void U32(Bytes& b,std::uint32_t n){for(unsigned i=0u;i<4u;++i)b.push_back(static_cast<std::uint8_t>(n>>(8u*i)));}
Bytes Ref(std::uint8_t op,std::int32_t n){Bytes b{op};U32(b,static_cast<std::uint32_t>(n));return b;}
Bytes Real(float n){Bytes b{0x1eu};U32(b,std::bit_cast<std::uint32_t>(n));return b;}
Bytes Join(std::initializer_list<Bytes> parts){Bytes b;for(const auto& p:parts)b.insert(b.end(),p.begin(),p.end());return b;}
Bytes Native(std::uint16_t n,std::initializer_list<Bytes> args={}){
    Bytes b=n<256u?Bytes{static_cast<std::uint8_t>(n)}:Bytes{static_cast<std::uint8_t>(0x60u+(n>>8u)),static_cast<std::uint8_t>(n)};
    for(const auto& a:args)b.insert(b.end(),a.begin(),a.end());
    b.push_back(0x16u);return b;
}
Bytes Final(std::initializer_list<Bytes> args){auto b=Ref(0x1cu,1);for(const auto& a:args)b.insert(b.end(),a.begin(),a.end());b.push_back(0x16u);return b;}
Bytes Context(Bytes object,Bytes expression,std::uint8_t fill=0u){
    Require(expression.size()<65536u,"Oversize Context fixture");auto b=Join({{0x19u},object});
    b.push_back(static_cast<std::uint8_t>(expression.size()));b.push_back(static_cast<std::uint8_t>(expression.size()>>8u));
    b.push_back(fill);return Join({b,expression});
}
Bytes ClassContext(Bytes object,Bytes expression){auto b=Context(std::move(object),std::move(expression));b[0u]=0x12u;return b;}
Bytes Skip(Bytes expression){Require(expression.size()<65536u,"Oversize Skip fixture");
    Bytes b{0x18u,static_cast<std::uint8_t>(expression.size()),static_cast<std::uint8_t>(expression.size()>>8u)};
    return Join({b,expression});
}
Vm::Function Declaration(){
    Vm::Function f;f.path="Engine.Pawn.PickTarget";f.source="Engine";f.flags=0x401u;f.nativeIndex=531u;
    f.variables={{"bestAim","bestAim",Value::Float(0),0x180u,1u,ResultLayout::Unknown},
        {"bestDist","bestDist",Value::Float(0),0x180u,1u,ResultLayout::Unknown},
        {"FireDir","FireDir",Value::Vector({}),0x80u,1u,ResultLayout::CoreVector12},
        {"projStart","projStart",Value::Vector({}),0x80u,1u,ResultLayout::CoreVector12},
        {"ReturnValue","ReturnValue",Value::Text(Kind::Object,{}),0x580u,1u,ResultLayout::Unknown}};
    return f;
}
Vm::Function Wrapper(Bytes expression){Vm::Function f;f.path="Test.CallPickTarget";f.source="Test";
    f.bytecode=Join({{0x04u},expression});return f;}
struct Host final:Vm::Host {
    std::map<std::string,Value> data,saved;
    std::map<std::int32_t,Vm::Property> properties;
    std::vector<std::string> events,receivers;
    std::vector<Value> steps;
    std::array<Value,2u> vectors{};
    std::shared_ptr<Vm::Reference> retainedAim;
    Vm::Execution* current{};
    std::size_t calls{},effects{},aimReads{},distReads{},bodyAimReads{},bodyDistReads{};
    bool active{},inBody{},secondStoreFault{},catchStoreFault{},badSeed{},noCandidates{},hookWrite{};
    Host(){
        properties[1]={"A","A",Value::Float(0),0u,1u,ResultLayout::Unknown};
        properties[2]={"D","D",Value::Float(0),0u,1u,ResultLayout::Unknown};
        properties[3]={"F","F",Value::Vector({}),0u,1u,ResultLayout::CoreVector12};
        properties[4]={"P","P",Value::Vector({}),0u,1u,ResultLayout::CoreVector12};
        properties[5]={"O","O",Value::Text(Kind::Object,{}),0u,1u,ResultLayout::Unknown};
        for(const auto* receiver:{"Self","Other","Other.Defaults"}){
            data[std::string(receiver)+".A"]=Value::Float(0.5f);data[std::string(receiver)+".D"]=Value::Float(-1.0f);
            data[std::string(receiver)+".F"]=Value::Vector({1,2,3});data[std::string(receiver)+".P"]=Value::Vector({4,5,6});
            data[std::string(receiver)+".O"]=Value::Text(Kind::Object,"Other");
        }
    }
    void Begin()override{Require(!active,"Nested PickTarget root");saved=data;active=true;}
    void Commit()override{active=false;}
    void Rollback()noexcept override{data.swap(saved);active=false;}
    Vm::Property ResolveProperty(const Vm::Function&,std::int32_t n)override{return properties.at(n);}
    std::string ResolveName(const Vm::Function&,std::int32_t)override{throw std::runtime_error("Unexpected name");}
    std::string ResolveObject(const Vm::Function&,std::int32_t)override{throw std::runtime_error("Unexpected object");}
    std::string ClassDefaultObject(const std::string& object)override{
        Require(object=="Other","ClassContext lost its actual class selector");return "Other.Defaults";
    }
    std::shared_ptr<const Vm::Function> ResolveFunction(const Vm::Function&,const std::string&,const Vm::Invocation&)override{
        return std::make_shared<Vm::Function>(Declaration());
    }
    std::shared_ptr<Vm::Reference> Address(const std::string& key){
        auto ref=std::make_shared<Vm::Reference>();const bool vector=key.ends_with(".F")||key.ends_with(".P");
        ref->zero=vector?Value::Vector({}):key.ends_with(".O")?Value::Text(Kind::Object,{}):Value::Float(0);
        ref->resultLayout=vector?ResultLayout::CoreVector12:ResultLayout::Unknown;
        ref->read=[this,key]{
            events.push_back("read:"+key);
            if(key.ends_with(".A")){++aimReads;if(inBody)++bodyAimReads;}
            if(key.ends_with(".D")){++distReads;if(inBody)++bodyDistReads;}
            return data.at(key);
        };
        ref->write=[this,key](const Value& value){events.push_back("write:"+key);data.at(key)=value;
            if(inBody&&secondStoreFault&&key.ends_with(".D"))throw std::runtime_error("PickTarget second store fault");};
        return ref;
    }
    std::shared_ptr<Vm::Reference> Variable(const std::string& object,const Vm::Property& p,Vm::Scope)override{
        return Address(object+'.'+p.key);
    }
    std::optional<ResultLayout> NativeArgumentUninitializedLayout(std::uint16_t n,const std::string&,std::size_t slot,const Vm::Function*)override{
        if(n!=531u)return std::nullopt;
        events.push_back("layout"+std::to_string(slot));
        return slot>=2u?std::optional{ResultLayout::CoreVector12}:std::nullopt;
    }
    std::optional<Value> NativeArgumentSeed(std::uint16_t n,const std::string&,std::size_t slot,const Vm::Function*)override{
        if(n!=531u)return std::nullopt;
        events.push_back("seed"+std::to_string(slot));
        if(hookWrite&&current){try{current->WriteReference(Address("Self.A"),Value::Float(99));}catch(...) {}}
        return slot<2u?std::optional{Value::Float(badSeed?std::numeric_limits<float>::quiet_NaN():0.0f)}:std::nullopt;
    }
    bool MaterializeNativeArgumentOmission(std::uint16_t n,const std::string&,std::size_t slot,const Vm::Function*)override{
        return n==531u&&slot<2u;
    }
    Evaluation Native(std::uint16_t,const std::string&,const std::vector<Evaluation>&,const Vm::Function*)override{
        throw std::runtime_error("Missing synchronous native execution");
    }
    Evaluation NativeWithExecution(std::uint16_t n,const std::string& object,const std::vector<Evaluation>& args,
        const Vm::Function*,Vm::Execution& execution)override{
        if(n==900u||n==901u){
            Require(object=="Self","PickTarget argument side effects lost Frame.Object");++effects;
            execution.WriteReference(Address("Self.A"),Value::Float(0.75f));
            if(n==901u)execution.WriteReference(Address("Self.F"),Value::Vector({99,99,99}));
            return {Value::Vector({7,8,9}),{}};
        }
        if(n==902u)return {{},{},Vm::ResultDisposition::NoWrite};
        if(n!=531u)throw std::runtime_error("PickTarget later root failure");
        ++calls;receivers.push_back(object);Require(args.size()==4u,"PickTarget packet count changed");
        Require(args[0].value.kind==Kind::Float&&args[1].value.kind==Kind::Float,
            "PickTarget omitted its copied Float Step temporary");
        Require(!args[2].reference&&!args[3].reference&&args[2].value.kind==Kind::Vector&&args[3].value.kind==Kind::Vector,
            "PickTarget input Vector was not copied at its own Step");
        steps={args[0].value,args[1].value};vectors={args[2].value,args[3].value};retainedAim=args[0].reference;
        // This fixture models only the packet's live alias contract, NOT the
        // production candidate gates, PC64 aiming operation or visibility test.
        float fallbackAim=args[0].value.floating;inBody=true;std::string selected;
        try{
            if(!noCandidates)for(const auto& candidate:std::array<std::pair<float,float>,3u>{{{0.6f,0.1f},{0.8f,0.2f},{0.7f,0.3f}}}){
                const auto threshold=args[0].reference?args[0].reference->read().floating:fallbackAim;
                if(candidate.first<=threshold)continue;
                selected="Candidate"+std::to_string(candidate.second);
                if(args[0].reference)args[0].reference->write(Value::Float(candidate.first));else fallbackAim=candidate.first;
                if(args[1].reference)args[1].reference->write(Value::Float(candidate.second));
            }
            inBody=false;
        }catch(...){inBody=false;if(!catchStoreFault)throw;}
        return {Value::Text(Kind::Object,std::move(selected)),{}};
    }
};
void Passed(const Vm::Result& r){Require(r.passed()&&r.committed,"PickTarget packet unexpectedly failed");}
void Refused(Host& host,const std::function<Vm::Result()>& run){const auto before=host.data;const auto result=run();
    Require(!result.passed()&&!result.committed,"Unsafe PickTarget packet passed");
    for(const auto& [key,value]:before)Require(Vm::Equal(host.data.at(key),value),"PickTarget refusal retained a write");
    ++refusals;
}
void Run(){
    const auto input=std::initializer_list<Bytes>{Ref(0x01u,1),Ref(0x01u,2),Ref(0x01u,3),Native(901u)};
    for(bool declared:{false,true}){
        Host host;Passed(Vm::Execute(host,Wrapper(declared?Final(input):Native(531u,input)),"Self"));
        Require(host.calls==1u&&host.effects==1u&&host.steps[0].floating==0.5f&&host.steps[1].floating==-1.0f,
            "Later argument effects changed captured OUT Float Step temporaries");
        Require(Vm::Equal(host.vectors[0],Value::Vector({1,2,3}))&&Vm::Equal(host.vectors[1],Value::Vector({7,8,9})),
            "Later argument changed the copied FireDir Vector");
        Require(host.data.at("Self.A").floating==0.8f&&host.data.at("Self.D").floating==0.2f&&host.bodyAimReads==4u,
            "PickTarget threshold used detached Step input instead of live bestAim alias");
        // Three live threshold reads plus one journal undo-read for accepted
        // aim; bestDist has only its single accepted-store undo-read in body.
        Require(host.bodyDistReads==1u&&host.aimReads==6u&&host.distReads==2u,
            "PickTarget eagerly reloaded copied OUTs or read bestDist as candidate input");
        const auto firstRead=std::find(host.events.begin(),host.events.end(),"read:Self.A");
        Require(firstRead!=host.events.end()&&static_cast<std::size_t>(firstRead-host.events.begin())==8u,
            "PickTarget seed/layout authority was not captured before every expression");
    }
    for(bool declared:{false,true}){
        Host host;const auto args=std::initializer_list<Bytes>{Ref(0x01u,1),Ref(0x01u,1),Ref(0x01u,3),Ref(0x01u,4)};
        Passed(Vm::Execute(host,Wrapper(declared?Final(args):Native(531u,args)),"Self"));
        Require(host.data.at("Self.A").floating==0.3f&&host.bodyAimReads==9u,
            "Aliased PickTarget outputs did not publish aim then distance for each candidate");
    }
    for(bool declared:{false,true})for(bool directWrapper:{false,true})for(unsigned mode=0u;mode<7u;++mode)
        for(bool noCandidates:{false,true}){
        Host host;host.noCandidates=noCandidates;
        const auto forward=[&](Bytes expression){
            switch(mode){
            case 0u:return Context(Ref(0x01u,5),std::move(expression));
            case 1u:return Context(Ref(0x01u,5),Context(Ref(0x01u,5),std::move(expression)));
            case 2u:return Skip(std::move(expression));
            case 3u:return Skip(Context(Ref(0x01u,5),Skip(std::move(expression))));
            case 4u:return ClassContext(Ref(0x01u,5),std::move(expression));
            case 5u:return ClassContext(Ref(0x01u,5),ClassContext(Ref(0x01u,5),std::move(expression)));
            default:return Context(Ref(0x01u,5),Skip(ClassContext(Ref(0x01u,5),Skip(std::move(expression)))));
            }
        };
        const auto args=std::initializer_list<Bytes>{forward(Ref(0x01u,1)),forward(Ref(0x01u,2)),Ref(0x01u,3),Ref(0x01u,4)};
        const auto f=Wrapper(declared?Final(args):Native(531u,args));
        const auto result=directWrapper?Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){
            return execution.CallFunction(f,"Self");}):Vm::Execute(host,f,"Self");
        Passed(result);
        Require(host.aimReads==(noCandidates?1u:6u)&&host.distReads==(noCandidates?1u:3u),
            "Forwarding Context/ClassContext/Skip reloaded a captured OUT Float Step");
        Require(host.steps[0].floating==0.5f&&host.steps[1].floating==-1.0f,
            "Forwarded Float capture lost its detached Step temporary");
        const auto target=mode==2u?"Self":mode>=4u?"Other.Defaults":"Other";
        Require(host.data.at(std::string(target)+".A").floating==(noCandidates?0.5f:0.8f)&&
            host.data.at(std::string(target)+".D").floating==(noCandidates?-1.0f:0.2f),
            "Forwarded Float capture detached/rebound its live OUT alias");
        Require(host.receivers==std::vector<std::string>{"Self"},"Forwarded argument changed native receiver");
    }
    for(bool declared:{false,true}){
        Host host;host.noCandidates=true;
        const auto args=std::initializer_list<Bytes>{{0x0bu},Native(902u),Ref(0x01u,3),Ref(0x01u,4)};
        Passed(Vm::Execute(host,Wrapper(declared?Final(args):Native(531u,args)),"Self"));
        Require(host.steps[0].floating==0&&host.steps[1].floating==0&&!host.aimReads&&!host.distReads,
            "Nothing/NoWrite fallback Float did not preserve initialized positive zero");
    }
    for(bool declared:{false,true}){
        Host host;const auto args=std::initializer_list<Bytes>{Real(0.65f),Real(100.0f),Ref(0x01u,3),Ref(0x01u,4)};
        const auto result=Vm::Execute(host,Wrapper(declared?Final(args):Native(531u,args)),"Self");Passed(result);
        Require(host.steps[0].floating==0.65f&&host.steps[1].floating==100.0f&&result.value.text=="Candidate0.200000"&&
            !host.aimReads&&!host.distReads&&result.writes==0u,
            "Numeric non-lvalue fallback did not supply/update the native threshold temporary");
    }
    for(bool declared:{false,true})for(std::uint8_t fill:{0u,1u,2u,3u,4u,5u}){
        Host host;host.noCandidates=true;
        const auto args=std::initializer_list<Bytes>{Context({0x2au},Ref(0x01u,1),fill),{0x0bu},Ref(0x01u,3),Ref(0x01u,4)};
        const auto f=Wrapper(declared?Final(args):Native(531u,args));
        if(fill<=4u){Passed(Vm::Execute(host,f,"Self"));Require(host.steps[0].floating==0,"Float null fill changed positive-zero seed");}
        else Refused(host,[&]{return Vm::Execute(host,f,"Self");});
    }
    for(bool declared:{false,true})for(std::uint8_t fill:{0u,4u,8u,11u,12u,13u}){
        Host host;const auto args=std::initializer_list<Bytes>{{0x0bu},{0x0bu},Context({0x2au},Ref(0x01u,3),fill),Ref(0x01u,4)};
        const auto f=Wrapper(declared?Final(args):Native(531u,args));
        if(fill==12u){Passed(Vm::Execute(host,f,"Self"));Require(Vm::Equal(host.vectors[0],Value::Vector({})),"Vector full-fill changed");}
        else Refused(host,[&]{return Vm::Execute(host,f,"Self");});
    }
    {
        Host host;const auto args=std::initializer_list<Bytes>{Ref(0x01u,1),Ref(0x01u,2),Ref(0x01u,3),Native(900u)};
        Passed(Vm::Execute(host,Wrapper(Context(Ref(0x01u,5),Native(531u,args))),"Self"));
        Require(host.receivers==std::vector<std::string>{"Other"}&&host.effects==1u&&host.data.at("Other.A").floating==0.5f,
            "PickTarget confused opcode receiver with argument Frame.Object");
    }
    {
        Host host;const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){
            return execution.CallFunction(Declaration(),"Self",{{{},host.Address("Self.A")},{{},host.Address("Self.D")},
                {{},host.Address("Self.F")},{{},host.Address("Self.P")}});
        });Passed(result);
        Require(host.steps[0].floating==0.5f&&host.data.at("Self.A").floating==0.8f&&host.data.at("Self.D").floating==0.2f,
            "Direct PickTarget call lost copied inputs/live aliases");
        bool expired{};try{host.retainedAim->write(Value::Float(9));}catch(const std::exception&){expired=true;}
        Require(expired&&host.data.at("Self.A").floating==0.8f,"PickTarget OUT guard survived execution");
    }
    for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill}){
        Host host;host.noCandidates=true;
        const std::vector<Evaluation> args{{Value::Float(99),host.Address("Self.A"),disposition,4u},
            {Value::Float(99),host.Address("Self.D"),disposition,4u},
            {Value::Vector({1,2,3}),{}},{Value::Vector({4,5,6}),{}}};
        Passed(Vm::Execute(host,Declaration(),"Self",args));
        Require(host.steps[0].floating==0&&host.steps[1].floating==0&&!host.aimReads&&!host.distReads,
            "Direct unwritten Float Step read stale aliases instead of its own initialized temporary");
    }
    for(unsigned cut=0u;cut<4u;++cut){
        Host host;Vm::Limits limits;limits.writes=cut;
        auto f=Wrapper(Native(531u,{Ref(0x01u,1),Ref(0x01u,2),Ref(0x01u,3),Ref(0x01u,4)}));
        Refused(host,[&]{return Vm::Execute(host,f,"Self",{},limits);});
    }
    for(bool caught:{false,true}){
        Host host;host.secondStoreFault=true;host.catchStoreFault=caught;
        auto f=Wrapper(Native(531u,{Ref(0x01u,1),Ref(0x01u,2),Ref(0x01u,3),Ref(0x01u,4)}));
        Refused(host,[&]{return Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){
            try{execution.CallFunction(f,"Self");}catch(...){}return Value{};});});
    }
    {
        Host host;auto f=Wrapper(Join({Native(531u,{Ref(0x01u,1),Ref(0x01u,2),Ref(0x01u,3),Ref(0x01u,4)}),Native(999u)}));
        // A normal statement wrapper permits a later failing opcode after the
        // completed native; neither accepted candidate may survive that root.
        f.bytecode=Join({Native(531u,{Ref(0x01u,1),Ref(0x01u,2),Ref(0x01u,3),Ref(0x01u,4)}),Native(999u),{0x04u,0x0bu}});
        Refused(host,[&]{return Vm::Execute(host,f,"Self");});Require(host.calls==1u,"Late root failure skipped original packet");
    }
    {
        Host host;host.badSeed=true;auto f=Wrapper(Native(531u,{Ref(0x01u,1),Ref(0x01u,2),Ref(0x01u,3),Ref(0x01u,4)}));
        Refused(host,[&]{return Vm::Execute(host,f,"Self");});Require(!host.calls&&!host.aimReads,"Bad native seed read an argument before refusal");
    }
    {
        Host host;host.hookWrite=true;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){host.current=&execution;
            try{execution.CallFunction(Declaration(),"Self",{{},{},{Value::Vector({}),{}},{Value::Vector({}),{}}});}catch(...){}
            host.current=nullptr;return Value{};});
        Require(!result.passed()&&!result.committed&&!host.calls&&host.data.at("Self.A").floating==0.5f,
            "Caught read-only PickTarget seed write did not poison root");++refusals;
    }
}
}
int main(){try{Run();std::cout<<"PASS PickTarget native packet checks="<<checks<<" refusals="<<refusals<<'\n';return 0;}
catch(const std::exception& e){std::cerr<<"FAIL PickTarget native packet: "<<e.what()<<'\n';return 1;}}
