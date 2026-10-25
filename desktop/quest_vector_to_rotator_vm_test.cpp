#include "quest_portable_vm.h"

#include <array>
#include <bit>
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

// Focused EXPLICIT MOCK-HOST wrapper/transaction proof, not an owned map,
// loaded original DLL, native impact effect or headset acceptance. General
// FVector.Rotation arithmetic has a separate independent x87 oracle test.
namespace {
namespace Vm=QuestVr::Vm;
using Vm::Value;using Vm::Kind;using Bytes=std::vector<std::uint8_t>;
using Point=std::array<float,3u>;using Rotation=std::array<std::int32_t,3u>;
std::size_t checks{},refusals{},cases{};
void Require(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
void U16(Bytes& bytes,std::uint16_t value){bytes.push_back(static_cast<std::uint8_t>(value));bytes.push_back(static_cast<std::uint8_t>(value>>8u));}
void U32(Bytes& bytes,std::uint32_t value){for(unsigned i=0u;i<4u;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(i*8u)));}
Bytes Join(std::initializer_list<Bytes> parts){Bytes out;for(const auto& part:parts)out.insert(out.end(),part.begin(),part.end());return out;}
Bytes Ref(std::uint8_t token,std::int32_t index){Bytes bytes{token};U32(bytes,std::bit_cast<std::uint32_t>(index));return bytes;}
Bytes Vector(Point value){Bytes bytes{0x23u};for(const auto component:value)U32(bytes,std::bit_cast<std::uint32_t>(component));return bytes;}
Bytes Rotator(Rotation value){Bytes bytes{0x22u};for(const auto component:value)U32(bytes,std::bit_cast<std::uint32_t>(component));return bytes;}
Bytes Convert(Bytes value){return Join({{0x50u},value});}
Bytes Return(Bytes value={0x0bu}){return Join({{0x04u},value});}
Bytes Let(Bytes left,Bytes right){return Join({{0x0fu},left,right});}
Bytes Member(std::int32_t property,Bytes parent){return Join({Ref(0x36u,property),parent});}
Bytes Context(Bytes object,Bytes body,std::uint8_t fill=0u){auto bytes=Join({{0x19u},object});U16(bytes,static_cast<std::uint16_t>(body.size()));
    bytes.push_back(fill);return Join({bytes,body});}
Bytes Native(std::uint16_t index){return {static_cast<std::uint8_t>(0x60u+(index>>8u)),static_cast<std::uint8_t>(index),0x16u};}
Bytes Final(std::int32_t index){return Join({Ref(0x1cu,index),{0x16u}});}
Vm::Function Function(const std::string& name,Bytes bytes){Vm::Function fn;fn.path="Mock."+name;fn.source="Mock";fn.bytecode=std::move(bytes);return fn;}
bool SamePoint(Point a,Point b){for(unsigned i=0u;i<3u;++i)if(std::bit_cast<std::uint32_t>(a[i])!=std::bit_cast<std::uint32_t>(b[i]))return false;return true;}
Value Structure(){Value value;value.kind=Kind::Struct;value.fields["direction"]=Value::Vector({0,1,0});
    value.fields["rotation"]=Value::Rotator({91,92,93});value.fields["sibling"]=Value::Integer(37);return value;}
struct Host final:Vm::Host {
    std::map<std::int32_t,Vm::Property> properties;
    std::map<std::pair<std::string,std::string>,Value> storage,before;
    std::map<std::int32_t,std::shared_ptr<const Vm::Function>> functions;
    std::vector<std::string> variableReceivers,nativeReceivers,reads,writes;
    std::size_t begins{},commits{},rollbacks{},nativeCalls{},effects{},beforeEffects{};
    bool active{},journalRestored{};Vm::Evaluation response;
    std::function<Value(const std::string&,const Vm::Property&,const Value&)> customRead;
    Host(){
        Add(1,"Direction",Value::Vector({}),Vm::ResultLayout::CoreVector12);
        Add(2,"Rotation",Value::Rotator({}),Vm::ResultLayout::CoreRotator12);
        Add(3,"Packet",Structure());
        Add(4,"direction",Value::Vector({}),Vm::ResultLayout::CoreVector12);
        Add(5,"rotation",Value::Rotator({}),Vm::ResultLayout::CoreRotator12);
        for(const auto* receiver:{"Self","Other"}){
            Store(receiver,1,Value::Vector(receiver==std::string("Self")?Point{0,1,0}:Point{0,0,1}));
            Store(receiver,2,Value::Rotator({91,92,93}));Store(receiver,3,Structure());
        }
    }
    void Add(std::int32_t index,const std::string& name,Value zero,Vm::ResultLayout layout=Vm::ResultLayout::Unknown){
        properties.emplace(index,Vm::Property{"Mock.Field"+std::to_string(index),name,std::move(zero),0u,1u,layout});}
    void Store(const std::string& receiver,std::int32_t index,Value value){storage[{receiver,properties.at(index).key}]=std::move(value);}
    const Value& Load(const std::string& receiver,std::int32_t index)const{return storage.at({receiver,properties.at(index).key});}
    void Begin()override{Require(!active,"Mock converter nested host root");active=true;++begins;before=storage;beforeEffects=effects;}
    void Commit()override{Require(active,"Mock converter commit without root");active=false;++commits;}
    void Rollback()noexcept override{
        journalRestored=storage.size()==before.size();
        for(const auto& [key,value]:before){const auto current=storage.find(key);if(current==storage.end()||!Vm::Equal(current->second,value))journalRestored=false;}
        // Intentionally no property restore: the real VM reference journal
        // must undo every conversion-related write before host rollback.
        effects=beforeEffects;active=false;++rollbacks;
    }
    Vm::Property ResolveProperty(const Vm::Function&,std::int32_t index)override{return properties.at(index);}
    std::string ResolveName(const Vm::Function&,std::int32_t)override{throw std::runtime_error("Unexpected mock conversion name lookup");}
    std::string ResolveObject(const Vm::Function&,std::int32_t index)override{
        Require(index==1,"Unknown mock changed-context receiver");return "Other";}
    std::shared_ptr<const Vm::Function> ResolveFunction(const Vm::Function&,const std::string&,const Vm::Invocation& call)override{
        return functions.at(call.reference);}
    std::shared_ptr<Vm::Reference> Variable(const std::string& receiver,const Vm::Property& property,Vm::Scope)override{
        variableReceivers.push_back(receiver+":"+property.name);const auto key=std::pair{receiver,property.key};
        auto reference=std::make_shared<Vm::Reference>();reference->zero=property.zero;reference->resultLayout=property.resultLayout;
        reference->read=[this,key,receiver,property]{reads.push_back(receiver+":"+property.name);const auto value=storage.at(key);
            return customRead?customRead(receiver,property,value):value;};
        reference->write=[this,key,receiver,property](const Value& value){writes.push_back(receiver+":"+property.name);storage.at(key)=value;};
        return reference;
    }
    Vm::Evaluation Native(std::uint16_t index,const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*)override{
        Require(arguments.empty(),"Mock converter native unexpectedly received arguments");++nativeCalls;nativeReceivers.push_back(receiver);
        if(index==4093u){++effects;return {Value::Integer(1),{}};}
        if(index==4092u)throw std::runtime_error("Mock later conversion fault");
        Require(index==4094u,"Unexpected mock converter native index");return response;
    }
};
void Returned(const Vm::Result& result,const Host& host,Rotation expected,std::size_t writes=0u){
    ++cases;Require(result.passed()&&result.committed&&result.value.kind==Kind::Rotator&&result.value.rotation==expected&&result.writes==writes&&
        host.begins==1u&&host.commits==1u&&!host.rollbacks&&!host.active,"Mock VectorToRotator result/root/store mismatch: "+result.error);
}
void Failed(const Vm::Result& result,const Host& host){
    ++cases;++refusals;Require(!result.passed()&&!result.committed&&!result.error.empty()&&result.value.kind==Kind::Nothing&&
        host.begins==1u&&!host.commits&&host.rollbacks==1u&&!host.active&&host.journalRestored,
        "Mock VectorToRotator refusal failed complete reference/root rollback");
}
void ConstantsAndReceivers(){
    const std::array<std::pair<Point,Rotation>,10u> axes{{
        {{0,0,0},{0,0,0}},{{1,0,0},{0,0,0}},{{0,1,0},{0,16383,0}},{{0,-1,0},{0,-16383,0}},
        {{0,0,1},{16383,0,0}},{{0,0,-1},{-16383,0,0}},{{-1,0,0},{0,32767,0}},{{-1,-0.0f,0},{0,-32767,0}},
        {{1,1,0},{0,8191,0}},{{1,-1,0},{0,-8191,0}}
    }};
    for(const auto& [point,rotation]:axes){Host host;const auto result=Vm::Execute(host,Function("Axes",Return(Convert(Vector(point)))),"Self");
        Returned(result,host,rotation);Require(!host.nativeCalls,"Scalar conversion invented a native handler dispatch");}
    {
        Host host;const auto result=Vm::Execute(host,Function("Variable",Return(Convert(Ref(0x01u,1)))),"Self");Returned(result,host,{0,16383,0});
        Require(host.variableReceivers==std::vector<std::string>{"Self:Direction"},"Vector converter did not read its reflected operand");
    }
    {
        Host host;const auto result=Vm::Execute(host,Function("ChangedContext",Return(Context(Ref(0x20u,1),Convert(Ref(0x01u,1))))),"Self");
        Returned(result,host,{0,16383,0});Require(host.variableReceivers==std::vector<std::string>{"Self:Direction"},
            "VectorToRotator evaluated operand on changed opcode receiver rather than FFrame.Object");
    }
    {
        Host host;auto callee=Function("Callee",Return(Convert(Ref(0x01u,1))));host.functions[7]=std::make_shared<Vm::Function>(callee);
        const auto result=Vm::Execute(host,Function("Outer",Return(Context(Ref(0x20u,1),Final(7)))),"Self");
        Returned(result,host,{16383,0,0});Require(host.variableReceivers==std::vector<std::string>{"Other:Direction"},
            "Nested converter kept caller frame object instead of callee's actual receiver");
    }
    {
        Host host;std::size_t reads{};host.customRead=[&](const std::string&,const Vm::Property& property,const Value& value){
            if(property.name=="Direction"){++reads;if(reads>1u)throw std::runtime_error("Vector operand alias reloaded after its copied Step");}
            return value;};
        const auto result=Vm::Execute(host,Function("CopyOnce",Return(Convert(Ref(0x01u,1)))),"Self");
        Returned(result,host,{0,16383,0});Require(reads==1u,"VectorToRotator reloaded a stale operand alias");
    }
    {
        Host host;std::size_t reads{};auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Vector({});alias->resultLayout=Vm::ResultLayout::CoreVector12;
        alias->read=[&]{++reads;if(reads>1u)throw std::runtime_error("Written native Vector result alias reloaded after Step");return Value::Vector({0,-1,0});};
        host.response={Value::Vector({0,0,1}),alias};
        const auto result=Vm::Execute(host,Function("NativeCopiedAlias",Return(Context(Ref(0x20u,1),Convert(Native(4094u))))),"Self");
        Returned(result,host,{0,-16383,0});Require(reads==1u&&host.nativeCalls==1u&&host.nativeReceivers==std::vector<std::string>{"Self"},
            "VectorToRotator used stale payload, reloaded a written alias or changed operand Frame.Object for a native Step");
    }
}
void InitializedTemporary(){
    for(const auto& child:{Bytes{0x0bu},Let(Ref(0x01u,1),Vector({0,0,1}))}){
        Host host;const auto result=Vm::Execute(host,Function("NoWrite",Return(Convert(child))),"Self");
        Returned(result,host,{},child.size()==1u?0u:1u);
        if(child.size()>1u)Require(SamePoint(host.Load("Self",1).vector,{0,0,1}),"Unwritten Let operand skipped its legitimate side effect");
    }
    for(unsigned fill=0u;fill<=12u;++fill){
        Host host;const auto result=Vm::Execute(host,Function("NullPrefix",Return(Convert(Context({0x2au},Native(4093u),static_cast<std::uint8_t>(fill))))),"Self");
        Returned(result,host,{});Require(!host.nativeCalls&&!host.effects,"Null Vector prefix evaluated its skipped operand child");
    }
    for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill})for(unsigned fill=0u;fill<=12u;++fill){
        Host host;std::size_t staleReads{};auto stale=std::make_shared<Vm::Reference>();stale->zero=Value::Vector({});stale->resultLayout=Vm::ResultLayout::CoreVector12;
        stale->read=[&]() -> Value {++staleReads;throw std::runtime_error("Read stale unwritten Vector result alias");};
        host.response={Value::Integer(999),stale,disposition,static_cast<std::uint8_t>(fill)};
        const auto result=Vm::Execute(host,Function("StaleUnwritten",Return(Convert(Native(4094u)))),"Self");
        Returned(result,host,{});Require(host.nativeCalls==1u&&!staleReads,"Initialized Vector temporary read an unwritten/partially cleared alias");
    }
    for(const auto fill:{13u,16u,255u}){
        Host host;const auto function=Function("OverWidth",Join({Native(4093u),Return(Convert(Context({0x2au},Vector({1,0,0}),static_cast<std::uint8_t>(fill))))}));
        const auto result=Vm::Execute(host,function,"Self");Failed(result,host);Require(!host.effects&&result.opcode==0x50u,
            "Oversized Vector temporary clear lost converter failure position or leaked prior effects");
    }
}
void TypedMembersAndRefusals(){
    {
        Host host;const auto result=Vm::Execute(host,Function("StructVectorMember",Return(Convert(Member(4,Ref(0x01u,3))))),"Self");
        Returned(result,host,{0,16383,0});
    }
    {
        Host host;const auto function=Function("WriteTypedMember",Join({Let(Member(5,Ref(0x01u,3)),Convert(Member(4,Ref(0x01u,3)))),Return(Member(5,Ref(0x01u,3)))}));
        const auto result=Vm::Execute(host,function,"Self");Returned(result,host,{0,16383,0},1u);
        const auto& packet=host.Load("Self",3);Require(packet.fields.at("rotation").rotation==Rotation{0,16383,0}&&
            packet.fields.at("sibling").integer==37&&SamePoint(packet.fields.at("direction").vector,{0,1,0}),
            "Typed writable Rotator member lost trusted full layout or corrupted sibling Vector/Int fields");
    }
    const std::array<Value,6u> invalid{Value::Integer(1),Value::Float(1),Value::Bool(true),Value::Rotator({}),Structure(),Value::Text(Kind::Object,{})};
    for(const auto& value:invalid){
        Host host;host.response={value,{}};const auto result=Vm::Execute(host,Function("WrongKind",Return(Convert(Native(4094u)))),"Self");Failed(result,host);
    }
    for(const auto& bytes:{Rotator({0,16384,0}),Ref(0x01u,3)}){
        Host host;const auto result=Vm::Execute(host,Function("WrongOperand",Return(Convert(bytes))),"Self");Failed(result,host);
    }
    for(unsigned channel=0u;channel<3u;++channel)for(const auto value:{std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        Host host;Point point{1,2,3};point[channel]=value;const auto result=Vm::Execute(host,Function("Nonfinite",Return(Convert(Vector(point)))),"Self");Failed(result,host);
    }
}
void TransactionControls(){
    const auto assigned=[] {return Function("Assigned",Join({Let(Ref(0x01u,2),Convert(Vector({0,1,0}))),Return(Ref(0x01u,2))}));};
    {
        Host host;const auto fn=assigned();const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){return execution.CallFunction(fn,"Self");});
        Returned(result,host,{0,16383,0},1u);Require(host.Load("Self",2).rotation==Rotation{0,16383,0},"Coordinated conversion did not publish its genuine typed store");
    }
    for(const std::size_t cut:{0u,1u}){
        Host host;const auto fn=assigned();Vm::Limits limits;limits.writes=cut;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){
            execution.CallFunction(fn,"Self");return execution.CallFunction(fn,"Self");},limits);
        Failed(result,host);Require(result.status==Vm::Status::Budget&&result.writes==cut&&host.Load("Self",2).rotation==Rotation{91,92,93},
            "Two conversion calls reset their root write budget or retained earlier publication");
    }
    for(const bool nativeFault:{false,true}){
        Host host;const auto fn=nativeFault?Function("LateNative",Join({Let(Ref(0x01u,2),Convert(Vector({0,0,1}))),Native(4092u),Return()})):assigned();
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) -> Value {
            execution.CallFunction(fn,"Self");throw std::runtime_error("Mock late coordinator conversion fault");});
        Failed(result,host);Require(result.writes==1u&&host.Load("Self",2).rotation==Rotation{91,92,93},
            "Late native/coordinator failure retained a completed conversion output store");
    }
    {
        Host host;auto fn=assigned();Vm::Limits limits;limits.writes=0u;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution){
            try{execution.CallFunction(fn,"Self");}catch(...){}
            try{execution.CallFunction(fn,"Self");}catch(...){}
            return Value::Integer(7);},limits);
        Failed(result,host);Require(result.status==Vm::Status::Budget&&!result.writes&&host.Load("Self",2).rotation==Rotation{91,92,93},
            "Caught conversion-store failure did not poison the original root");
    }
    {
        Host host;const auto function=Function("NoWriteThenInvalid",Join({Let(Ref(0x01u,2),Convert(Let(Ref(0x01u,1),Vector({0,0,1})))),
            Return(Convert(Rotator({1,2,3})))}));
        const auto result=Vm::Execute(host,function,"Self");Failed(result,host);
        Require(result.writes==2u&&host.Load("Self",2).rotation==Rotation{91,92,93}&&SamePoint(host.Load("Self",1).vector,{0,1,0}),
            "Later malformed conversion failed undo of both initialized-NoWrite operand effects and its zero result store");
    }
}
} // namespace
int main(){try{ConstantsAndReceivers();InitializedTemporary();TypedMembersAndRefusals();TransactionControls();
    std::cout<<"PASS EXPLICIT MOCK-HOST VectorToRotator0x50 VM wrapper: "<<checks<<" checks / "<<cases<<" cases / "<<refusals
        <<" refusals; original-axis65535 scale, Frame.Object, copied Vector/typed member results, initialized12-byte NoWrite/null prefixes, "
        "safe malformed-input refusal and same-root write/late/caught-failure rollback. NOT general math-oracle, owned impact or Quest acceptance\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL mock VectorToRotator VM: "<<error.what()<<'\n';return 1;}}
