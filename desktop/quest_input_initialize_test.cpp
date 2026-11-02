#include "quest_input_initialize.h"

#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Test providers only. These controls do NOT authenticate RuntimeObject fields,
// root journals, a Quest platform provider, physical controller acceptance,
// original InputExt.Process/ReadInput, startup or playable gameplay.
namespace {
using Id=QuestVr::InputInitializeNativeId;
using Kind=QuestVr::OriginalInputResetPropertyKind;
constexpr Id Input=1u,Viewport=2u,OtherViewport=3u,Actor=4u,OtherActor=5u,ChildClass=100u,ParentClass=200u,OtherClass=300u;
constexpr auto Unlimited=std::numeric_limits<std::size_t>::max();
std::size_t checks{},refusals{};
void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class Function> void Refuse(Function&& function,const char* expected="") {
    bool refused{};
    try{function();}catch(const std::exception& error){refused=std::string(error.what()).find(expected)!=std::string::npos;}
    Check(refused,"Expected selected Input initialization refusal");++refusals;
}
enum class Type {Byte,ByteDerived,Float,FloatDerived,Bool,Int,Other};
struct Field {Type type;std::uint64_t flags;Id next;std::size_t dimension;};
struct Store {Id actor,field;Kind kind;};
struct Mock {
    Id viewport{Viewport},viewportActor{Actor},otherViewportActor{OtherActor};
    std::array<std::uint8_t,255u> keys;
    std::int32_t action{7};float delta{-3.0f};
    std::map<Id,Field> fields{{11u,{Type::Byte,4u,12u,3u}},{12u,{Type::Bool,4u,13u,1u}},
        {13u,{Type::Float,4u,14u,2u}},{14u,{Type::Byte,0u,15u,1u}},{15u,{Type::ByteDerived,4u,16u,1u}},
        {16u,{Type::Float,0u,17u,1u}},{17u,{Type::Int,4u,0u,1u}},
        {21u,{Type::FloatDerived,4u,22u,1u}},{22u,{Type::Byte,4u,23u,1u}},{23u,{Type::Other,4u,0u,1u}},
        {31u,{Type::Float,4u,32u,1u}},{32u,{Type::Byte,4u,0u,1u}}};
    std::map<Id,Id> children{{ChildClass,11u},{ParentClass,21u},{OtherClass,31u}};
    std::map<Id,Id> supers{{ChildClass,ParentClass},{ParentClass,0u},{OtherClass,0u}};
    std::map<Id,Id> classes{{Actor,ChildClass},{OtherActor,OtherClass}};
    std::map<std::pair<Id,Id>,std::vector<std::uint32_t>> storage;
    std::vector<std::string> events;std::vector<std::size_t> keyStores;std::vector<Store> stores;
    std::vector<Id> classReads,platformViewports,logs;
    std::size_t work{},maximumWork{Unlimited},scratch{},maximumScratch{1024u},peakScratch{},leaves{},platformCalls{},pointerWrites{};
    bool root{true};
    std::function<void(Mock&,Id,Kind)> afterProperty;
    std::function<void(Mock&)> platformHook;
    Mock() {
        keys.fill(0x55u);
        for(Id actor:{Actor,OtherActor})for(const auto& [id,field]:fields)
            storage[{actor,id}]=std::vector<std::uint32_t>(field.dimension,0x12345678u+static_cast<std::uint32_t>(id));
    }
    void Event(const std::string& event){events.push_back(event);}
    QuestVr::OriginalInputInitializeQueries Bind() {
        QuestVr::OriginalInputInitializeQueries q;
        q.requireOuterRoot=[this]{Event("root");if(!root)throw std::runtime_error("missing outer root");};
        q.work=[this]{if(work++>=maximumWork)throw std::runtime_error("shared work exhausted");};
        q.enterScratch=[this](std::size_t bytes){Event("frame");
            if(bytes>maximumScratch-scratch)throw std::runtime_error("scratch admission");
            scratch+=bytes;peakScratch=std::max(peakScratch,scratch);};
        q.leaveScratch=[this](std::size_t bytes)noexcept{if(bytes>scratch)std::terminate();scratch-=bytes;++leaves;};
        q.inputViewport=[this](Id input){Event("viewport");Check(input==Input,"Viewport read substituted Input receiver");return viewport;};
        q.writeInputViewport=[this](Id input,Id selected){Event("pointer");Check(input==Input,"Init pointer write used another Input");viewport=selected;++pointerWrites;};
        q.writeKey=[this](Id input,std::size_t index,std::uint8_t value){Event("key:"+std::to_string(index));
            Check(input==Input&&index<255u&&value==0u,"Reset key write changed index/receiver/value");keys.at(index)=value;keyStores.push_back(index);};
        q.viewportActor=[this](Id selected){Event("actor:"+std::to_string(selected));
            if(selected==Viewport)return viewportActor;
            if(selected==OtherViewport)return otherViewportActor;
            throw std::runtime_error("foreign viewport");};
        q.actorClass=[this](Id actor){Event("class:"+std::to_string(actor));classReads.push_back(actor);return classes.at(actor);};
        q.classChildren=[this](Id cls){Event("children:"+std::to_string(cls));return children.at(cls);};
        q.classSuper=[this](Id cls){Event("super:"+std::to_string(cls));return supers.at(cls);};
        q.fieldNext=[this](Id field){Event("next:"+std::to_string(field));return fields.at(field).next;};
        q.propertyIsA=[this](Id field,Kind kind){Event(std::string(kind==Kind::Byte?"byte?:":"float?:")+std::to_string(field));
            const auto type=fields.at(field).type;return kind==Kind::Byte?(type==Type::Byte||type==Type::ByteDerived):(type==Type::Float||type==Type::FloatDerived);};
        q.propertyFlags=[this](Id field){Event("flags:"+std::to_string(field));return fields.at(field).flags;};
        q.clearProperty=[this](Id actor,Id field,Kind kind){Event("clear:"+std::to_string(actor)+":"+std::to_string(field));
            stores.push_back({actor,field,kind});auto& word=storage.at({actor,field}).at(0u);
            if(kind==Kind::Byte)word&=0xffffff00u;else word=0u;
            if(afterProperty)afterProperty(*this,field,kind);};
        q.writeAction=[this](Id input,std::int32_t value){Event("action");Check(input==Input&&value==0,"Reset changed Action word");action=value;};
        q.writeDelta=[this](Id input,float value){Event("delta");Check(input==Input&&std::bit_cast<std::uint32_t>(value)==0u,"Reset Delta was not positive FLOAT zero");delta=value;};
        q.updateInput=[this](Id selected,std::int32_t reset){Event("platform");Check(reset==1,"Reset did not call UpdateInput with literal1");
            ++platformCalls;platformViewports.push_back(selected);if(platformHook)platformHook(*this);};
        q.initializedLog=[this](Id selected){Event("log");logs.push_back(selected);};
        return q;
    }
    void Reset(std::size_t bound=QuestVr::InputInitializeDetail::MaximumVisits){QuestVr::ResetOriginalInput(Input,Bind(),bound);}
    void Init(Id selected=Viewport){QuestVr::InitializeOriginalInput(Input,selected,Bind());}
};
std::size_t Position(const Mock& mock,const std::string& event) {
    const auto found=std::find(mock.events.begin(),mock.events.end(),event);
    Check(found!=mock.events.end(),"Missing required original initialization event");
    return static_cast<std::size_t>(found-mock.events.begin());
}
bool Has(const Mock& mock,const std::string& event){return std::find(mock.events.begin(),mock.events.end(),event)!=mock.events.end();}
void ExactStoresAndOrder() {
    Mock m;const auto before=m.storage;m.Reset();
    Check(m.keyStores.size()==255u,"Reset did not clear all255 held key bytes");
    for(std::size_t i=0;i<255u;++i)Check(m.keyStores[i]==i&&m.keys[i]==0u,"Key stores were not ascending0..254");
    Check(m.stores.size()==5u&&m.stores[0].field==11u&&m.stores[1].field==15u&&m.stores[2].field==22u&&
        m.stores[3].field==13u&&m.stores[4].field==21u,"Reset changed Byte-before-Float inherited declaration order");
    for(std::size_t i=0;i<m.stores.size();++i)Check(m.stores[i].actor==Actor&&m.stores[i].kind==(i<3u?Kind::Byte:Kind::Float),"Reset substituted actual Actor/type");
    Check(m.classReads==std::vector<Id>({Actor,Actor}),"Reset did not independently reread each iterator's Actor.Class");
    for(const auto& [cell,values]:m.storage) {
        const bool selected=cell.first==Actor&&(cell.second==11u||cell.second==15u||cell.second==22u||cell.second==13u||cell.second==21u);
        for(std::size_t i=0;i<values.size();++i) {
            auto expected=before.at(cell)[i];
            if(selected&&i==0u)expected=(cell.second==11u||cell.second==15u||cell.second==22u)?expected&0xffffff00u:0u;
            Check(values[i]==expected,"Reset changed store width, looped ArrayDim or cleared Bool/Int/noninput/other-Actor storage");
        }
    }
    Check(Position(m,"key:254")<Position(m,"class:4")&&Position(m,"clear:4:22")<Position(m,"float?:11")&&
        Position(m,"clear:4:21")<Position(m,"action")&&Position(m,"action")<Position(m,"delta")&&
        Position(m,"delta")<Position(m,"platform"),"Reset native operation order differs");
    Check(!Has(m,"flags:12")&&!Has(m,"flags:17")&&!Has(m,"flags:23"),"Reset queried flags on nonselected property types");
    Check(m.action==0&&std::bit_cast<std::uint32_t>(m.delta)==0u&&m.platformCalls==1u&&
        m.platformViewports==std::vector<Id>({Viewport})&&m.logs.empty()&&m.scratch==0u&&m.leaves==1u&&m.peakScratch==256u,
        "Reset accumulator/platform/lexical-frame state differs");
}
void LiveReadsAndInit() {
    Mock moved;moved.afterProperty=[](Mock& m,Id field,Kind){if(field==11u)m.viewport=OtherViewport;};moved.Reset();
    Check(moved.stores.size()==4u&&moved.stores[0].actor==Actor&&moved.stores[1].actor==OtherActor&&
        moved.stores[2].actor==OtherActor&&moved.stores[3].actor==OtherActor&&moved.stores[3].field==31u,
        "Selected stores did not reread Input.Viewport.Actor or Float borrowed Byte's class");
    Check(moved.classReads==std::vector<Id>({Actor,OtherActor})&&moved.platformViewports==std::vector<Id>({OtherViewport}),
        "Reset cached viewport/class across separate native operations");
    Mock next;next.afterProperty=[](Mock& m,Id field,Kind){if(field==11u)m.fields.at(field).next=15u;};next.Reset();
    Check(next.stores.size()==4u&&next.stores[0].field==11u&&next.stores[1].field==15u&&
        next.stores[2].field==22u&&next.stores[3].field==21u,"Iterator cached Next before selected write");
    Mock init;init.viewport=0u;init.Init();
    Check(init.pointerWrites==1u&&init.logs==std::vector<Id>({Viewport})&&Position(init,"pointer")<Position(init,"key:0")&&
        Position(init,"platform")<Position(init,"log")&&init.peakScratch==512u&&init.scratch==0u&&init.leaves==2u,
        "Init did not store Viewport then fully Reset then log with nested lexical frames");
    const auto firstKeys=init.keyStores.size();init.keys.fill(1u);init.action=9;init.delta=7.0f;init.Init();
    Check(init.pointerWrites==2u&&init.platformCalls==2u&&init.logs.size()==2u&&init.keyStores.size()==firstKeys+255u,
        "Repeated Init invented an idempotent shortcut");
    Mock logged;logged.platformHook=[](Mock& m){m.viewport=OtherViewport;};logged.Init();
    Check(logged.logs==std::vector<Id>({OtherViewport}),"Init log cached the originally supplied viewport instead of current native Viewport");
    Mock noClass;noClass.classes[Actor]=0u;noClass.Reset();
    Check(noClass.stores.empty()&&noClass.classReads.size()==2u&&noClass.platformCalls==1u,
        "Null iterator class invented field writes or omitted accumulator/platform work");
    Mock resetOnly;auto resetQ=resetOnly.Bind();resetQ.writeInputViewport={};resetQ.initializedLog={};
    QuestVr::ResetOriginalInput(Input,resetQ);
    Check(resetOnly.pointerWrites==0u&&resetOnly.logs.empty()&&resetOnly.viewport==Viewport&&resetOnly.platformCalls==1u,
        "Later ResetInput required Init-only pointer/log providers or replaced its established Viewport");
    Mock repress;repress.platformHook=[](Mock& m){
        Check(std::all_of(m.keys.begin(),m.keys.end(),[](auto value){return value==0u;})&&m.action==0&&
            std::bit_cast<std::uint32_t>(m.delta)==0u,"Platform reset saw uncleared key/accumulator prefix");
        m.keys[7u]=1u;m.action=4;m.delta=.25f;};repress.Reset();
    Check(repress.keys[7u]==1u&&repress.action==4&&repress.delta==.25f&&repress.pointerWrites==0u&&repress.logs.empty(),
        "Later ResetInput erased platform re-press state after UpdateInput1 or ran Init work");
}
void RefusalsAndPartialWrites() {
    Mock missing;missing.viewport=0u;Refuse([&]{missing.Reset();},"nonnull native Viewport");
    Check(missing.keyStores.empty()&&missing.action==7&&missing.scratch==0u&&missing.leaves==1u,"Viewport assertion did not precede stores");
    Mock nullInit;nullInit.viewport=Viewport;Refuse([&]{nullInit.Init(0u);},"nonnull native Viewport");
    Check(nullInit.pointerWrites==1u&&nullInit.viewport==0u&&nullInit.keyStores.empty()&&nullInit.scratch==0u,
        "Failed Init silently restored its original pointer or performed reset stores");
    Mock actor;actor.viewportActor=0u;Refuse([&]{actor.Reset();},"viewport Actor");
    Check(actor.keyStores.size()==255u&&actor.stores.empty()&&actor.action==7&&actor.platformCalls==0u,"Actor refusal lost key-clear prefix");
    Mock root;root.root=false;Refuse([&]{root.Init();},"outer root");Check(root.pointerWrites==0u&&root.peakScratch==0u,"Missing root mutated Init payload");
    Mock key;auto keyQ=key.Bind();const auto keyWriter=keyQ.writeKey;
    keyQ.writeKey=[keyWriter](Id input,std::size_t index,std::uint8_t value){keyWriter(input,index,value);if(index==17u)throw std::runtime_error("key prefix");};
    Refuse([&]{QuestVr::ResetOriginalInput(Input,keyQ);},"key prefix");
    Check(key.keyStores.size()==18u&&key.keys[17u]==0u&&key.keys[18u]==0x55u&&key.stores.empty()&&key.scratch==0u,"Key failure discarded or overran prefix");
    Mock property;property.afterProperty=[](Mock&,Id field,Kind){if(field==15u)throw std::runtime_error("field prefix");};
    Refuse([&]{property.Reset();},"field prefix");Check(property.stores.size()==2u&&(property.storage.at({Actor,15u})[0u]&0xffu)==0u&&property.action==7,
        "Property failure invented helper rollback or continued to accumulators");
    Mock action;auto actionQ=action.Bind();const auto actionWriter=actionQ.writeAction;
    actionQ.writeAction=[actionWriter](Id input,std::int32_t value){actionWriter(input,value);throw std::runtime_error("action prefix");};
    Refuse([&]{QuestVr::ResetOriginalInput(Input,actionQ);},"action prefix");
    Check(action.action==0&&action.delta==-3.0f&&action.platformCalls==0u&&action.scratch==0u,"Action failure lost exact prefix");
    Mock delta;auto deltaQ=delta.Bind();const auto deltaWriter=deltaQ.writeDelta;
    deltaQ.writeDelta=[deltaWriter](Id input,float value){deltaWriter(input,value);throw std::runtime_error("delta prefix");};
    Refuse([&]{QuestVr::ResetOriginalInput(Input,deltaQ);},"delta prefix");
    Check(delta.action==0&&std::bit_cast<std::uint32_t>(delta.delta)==0u&&delta.platformCalls==0u,"Delta failure invented successful platform update");
    Mock platform;platform.platformHook=[](Mock&){throw std::runtime_error("platform refusal");};
    Refuse([&]{platform.Init();},"platform refusal");Check(platform.pointerWrites==1u&&platform.action==0&&platform.platformCalls==1u&&platform.logs.empty()&&platform.scratch==0u,
        "Failed platform callback was counted as completed Init or rolled back its caller's prefix");
    Mock log;auto logQ=log.Bind();logQ.initializedLog=[&](Id){log.Event("log");throw std::runtime_error("log refusal");};
    Refuse([&]{QuestVr::InitializeOriginalInput(Input,Viewport,logQ);},"log refusal");
    Check(log.platformCalls==1u&&log.action==0&&log.scratch==0u&&log.leaves==2u,"Log failure lost completed reset prefix or lexical cleanup");
}
void BoundsAndMissingProviders() {
    Mock scratch;scratch.maximumScratch=256u;Refuse([&]{scratch.Init();},"scratch admission");
    Check(scratch.pointerWrites==1u&&scratch.keyStores.empty()&&scratch.scratch==0u&&scratch.leaves==1u,
        "Nested scratch failure released an unowned frame or discarded pointer prefix");
    Mock noScratch;auto noScratchQ=noScratch.Bind();noScratchQ.leaveScratch={};
    Refuse([&]{QuestVr::ResetOriginalInput(Input,noScratchQ);},"scratch cleanup");Check(noScratch.keyStores.empty(),"Missing cleanup performed stores");
    Mock work;work.maximumWork=0u;Refuse([&]{work.Init();},"shared work");Check(work.events.empty()&&work.scratch==0u,"Initial work refusal ran providers");
    Mock cycle;cycle.fields[17u].next=11u;Refuse([&]{cycle.Reset(400u);},"work bound");
    Check(cycle.keyStores.size()==255u&&cycle.platformCalls==0u&&cycle.scratch==0u,"Cycle bound did not retain prefix/cleanup");
    Mock missing;auto missingQ=missing.Bind();missingQ.updateInput={};
    Refuse([&]{QuestVr::ResetOriginalInput(Input,missingQ);},"reached provider");
    Check(missing.action==0&&std::bit_cast<std::uint32_t>(missing.delta)==0u&&missing.platformCalls==0u&&missing.scratch==0u,
        "Missing platform provider became empty success or erased its reset prefix");
    Mock noLog;auto noLogQ=noLog.Bind();noLogQ.initializedLog={};
    Refuse([&]{QuestVr::InitializeOriginalInput(Input,Viewport,noLogQ);},"reached provider");
    Check(noLog.platformCalls==1u&&noLog.logs.empty(),"Missing log provider failed before reached original reset");
    Mock invalid;Refuse([&]{invalid.Reset(0u);},"tightened work");
    Refuse([&]{invalid.Reset(QuestVr::InputInitializeDetail::MaximumVisits+1u);},"tightened work");
    Refuse([&]{QuestVr::ResetOriginalInput(0u,invalid.Bind());},"actual Input receiver");
    Refuse([&]{QuestVr::InitializeOriginalInput(0u,Viewport,invalid.Bind());},"actual Input receiver");
    Mock exact;exact.Reset();const auto consumed=exact.work;
    Mock exactAgain;exactAgain.Reset(consumed);Check(exactAgain.work==consumed&&exactAgain.platformCalls==1u,"Exact shared helper visit bound changed behavior");
    Mock oneLess;Refuse([&]{oneLess.Reset(consumed-1u);},"work bound");
    Check(oneLess.platformCalls==0u&&oneLess.action==0&&oneLess.scratch==0u,"One-below visit bound omitted late original refusal/cleanup");
    Mock initBaseline;initBaseline.Init();const auto initWork=initBaseline.work;
    for(std::size_t stop=0u;stop<initWork;++stop) {
        Mock prefix;prefix.maximumWork=stop;
        Refuse([&]{prefix.Init();},"shared work");
        Check(prefix.scratch==0u&&prefix.logs.empty()&&prefix.pointerWrites<=1u&&prefix.platformCalls<=1u,
            "A refused work prefix leaked scratch or manufactured completed Init");
        for(std::size_t i=0u;i<prefix.keyStores.size();++i)
            Check(prefix.keyStores[i]==i&&prefix.keys[i]==0u,"Work-prefix failure discarded an earlier ascending key write");
        if(prefix.keyStores.size()<255u)
            Check(prefix.keys[prefix.keyStores.size()]==0x55u&&prefix.stores.empty()&&prefix.action==7,
                "Work-prefix failure performed a later key/property/Action write");
        if(prefix.platformCalls)
            Check(prefix.keyStores.size()==255u&&prefix.stores.size()==5u&&prefix.action==0&&
                std::bit_cast<std::uint32_t>(prefix.delta)==0u,"Platform call preceded required original reset stores");
    }
}
} // namespace
int main(){try{
    ExactStoresAndOrder();LiveReadsAndInit();RefusalsAndPartialWrites();BoundsAndMissingProviders();
    std::cout<<"PASS original Input.Init/ResetInput coordinator checks="<<checks<<" refusals="<<refusals
        <<"; fixture providers only, NOT RuntimeObject journals/Quest platform/InputExt processing/startup/headset proof\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL original Input initialization: "<<error.what()<<'\n';return 1;}}
