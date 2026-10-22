#include "quest_world_actor_order.h"
#include "quest_native_work.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// MOCK actual-Level providers only. Independent literal/stable-list oracles
// check this normal coordinator, not live runtime save/cleanup or Training.
namespace {
using Id=std::uintptr_t;
constexpr Id Level=900u;
constexpr auto Unlimited=std::numeric_limits<std::size_t>::max();
std::size_t checks{},cases{},providerRefusals{},workRefusals{},admissionRefusals{},writeRefusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Injected {};
struct Event {
    char kind{};std::size_t a{},b{};
    bool operator==(const Event&)const=default;
};
struct Actor {bool stat{},pending{},birth{};bool operator==(const Actor&)const=default;};
struct State {
    std::vector<Id> slots{101u,0u,201u,301u,0u,202u,302u,401u,402u};
    std::map<Id,Actor> actors{{101u,{}},{201u,{}},{301u,{true,false,false}},
        {202u,{false,true,false}},{302u,{true,true,false}},
        {401u,{true,false,true}},{402u,{false,false,true}}};
    std::int32_t split{77};
    std::vector<Id> objectLookup{101u,201u,301u,202u,302u,401u,402u};
    std::map<Id,std::pair<unsigned,unsigned>> objectIndices{{101u,{11u,1011u}},{201u,{12u,1012u}},{301u,{13u,1013u}}};
    std::vector<Id> hashLinks{402u,301u,202u,201u};
    bool operator==(const State&)const=default;
};
struct Context {
    State state;
    QuestVr::NativeWork::Budget work{QuestVr::NativeWork::MaximumLimit};
    std::size_t scratch{37u},scratchBaseline{37u},peak{},byteLimit{Unlimited};
    std::size_t providerLimit{Unlimited},admissionLimit{Unlimited},writeLimit{Unlimited};
    std::size_t providers{},admissions{},writes{};
    std::vector<Event> trace;
    std::function<void(Context&,const Event&)> after;
    bool replaced{};
    void Enter(const Event& event){trace.push_back(event);if(providers++==providerLimit)throw Injected{};}
    void After(const Event& event){if(after)after(*this,event);}
    void Receiver(Id value){Require(value==Level,"Provider received a different retained Level");}
    QuestVr::OriginalWorldActorOrderQueries Bind(){
        QuestVr::OriginalWorldActorOrderQueries q;
        q.work=[this]{work.Step();};
        q.actorCount=[this](Id level){Receiver(level);const Event event{'N',state.slots.size(),0u};Enter(event);
            const auto count=static_cast<std::int32_t>(state.slots.size());After(event);return count;};
        q.actorSlot=[this](Id level,std::int32_t slot){Receiver(level);Require(slot>=0&&static_cast<std::size_t>(slot)<state.slots.size(),"Coordinator accessed an invalid native slot");
            const auto value=state.slots[static_cast<std::size_t>(slot)];const Event event{'R',static_cast<std::size_t>(slot),value};Enter(event);After(event);return value;};
        q.isStatic=[this](Id actor){Require(actor!=0u,"Coordinator asked for None's static bit");const auto value=state.actors.at(actor).stat;
            const Event event{'S',actor,value};Enter(event);After(event);return value;};
        q.writeFirstDynamic=[this](Id level,std::int32_t split){Receiver(level);const Event event{'D',static_cast<std::size_t>(split),0u};Enter(event);
            if(writes==writeLimit)throw Injected{};
            ++writes;state.split=split;After(event);};
        q.replaceSlots=[this](Id level,std::span<const Id> slots){Receiver(level);const Event event{'P',slots.size(),0u};Enter(event);
            Require(scratch>scratchBaseline,"Temporary storage vanished before native replacement provider");
            if(writes==writeLimit)throw Injected{};
            ++writes;state.slots.assign(slots.begin(),slots.end());replaced=true;After(event);};
        q.admitTemporaryBytes=[this](std::size_t extra){const Event event{'A',scratch,extra};Enter(event);
            Require(extra<=Unlimited-scratch,"Temporary admission arithmetic overflowed");peak=std::max(peak,scratch+extra);
            if(admissions++==admissionLimit||scratch>byteLimit||extra>byteLimit-scratch)throw Injected{};
            After(event);};
        return q;
    }
    void Run(){auto q=Bind();QuestVr::ReorderOriginalWorldActors(Level,q,scratch);}
    bool Refuses(){try{Run();}catch(const Injected&){return true;}catch(const QuestVr::NativeWork::Exhausted&){return true;}
        catch(const std::runtime_error&){return true;}return false;}
};
std::pair<std::vector<Id>,std::int32_t> Expected(const State& state){
    // Independent simple list oracle: group stable identities by their values,
    // deliberately preserving pending-delete/birth metadata outside selection.
    std::vector<Id> result{state.slots.at(0u),state.slots.at(1u)};
    for(const bool selected:{true,false})for(std::size_t i=2u;i<state.slots.size();++i){const auto value=state.slots[i];
        if(value&&state.actors.at(value).stat==selected)result.push_back(value);}
    std::int32_t split=2;for(std::size_t i=2u;i<state.slots.size();++i)if(state.slots[i]&&state.actors.at(state.slots[i]).stat)++split;
    return {result,split};
}
void Preserved(const State& before,const Context& context){
    Require(context.state.actors==before.actors&&context.state.objectLookup==before.objectLookup&&
        context.state.objectIndices==before.objectIndices&&context.state.hashLinks==before.hashLinks,
        "Slot coordinator changed fields, UObject/export/index/lookup or collision hash identities");
    Require(context.scratch==context.scratchBaseline,"Borrowed scratch ledger did not restore exactly");
}
void Ordinary(){
    Context context;const auto before=context.state;context.work.Step(19u);context.Run();
    Require(context.state.slots==std::vector<Id>({101u,0u,301u,302u,401u,201u,202u,402u})&&context.state.split==5,
        "Literal static/dynamic/authored/birth/pending-delete order differs from original");
    Require(context.work.Read().used>19u&&context.writes==2u&&context.replaced,"Coordinator renewed work or omitted actual split/replacement providers");
    std::size_t splitTrace=Unlimited,replaceTrace=Unlimited;for(std::size_t i=0;i<context.trace.size();++i){
        if(context.trace[i].kind=='D')splitTrace=i;
        if(context.trace[i].kind=='P')replaceTrace=i;
    }
    Require(splitTrace<replaceTrace&&context.trace[splitTrace+1u].kind=='N',"Native split store did not precede the second live scan");
    Require(std::none_of(context.trace.begin(),context.trace.end(),[](const Event& event){return event.kind=='S'&&event.a==101u;}),
        "Reserved slot entered static selection");Preserved(before,context);++cases;
    // Exhaustive finite tail census, including holes and pending dynamic actors.
    for(unsigned pattern=0u;pattern<1024u;++pattern){Context value;value.state.slots={101u,pattern&1u?Id{}:101u};value.state.actors={{101u,{true,true,false}}};
        unsigned bits=pattern;for(unsigned i=0;i<5u;++i){const auto kind=bits&3u;bits>>=2u;const Id id=200u+i;
            value.state.slots.push_back(kind?id:Id{});value.state.actors.emplace(id,Actor{kind==1u,kind==3u,i>=3u});}
        const auto input=value.state;const auto expected=Expected(input);value.Run();
        Require(value.state.slots==expected.first&&value.state.split==expected.second,"Independent exhaustive stable-list oracle differs");
        Preserved(input,value);++cases;
    }
    Context empty;empty.state.slots={0u,0u};empty.state.actors.clear();empty.Run();
    Require(empty.state.slots==std::vector<Id>({0u,0u})&&empty.state.split==2,"Reserved None pair was filtered");++cases;
}
void LiveReads(){
    Context flags;flags.state.slots={101u,0u,301u,201u};
    flags.after=[](Context& c,const Event& event){if(event.kind=='D'){c.state.actors.at(301u).stat=false;c.state.actors.at(201u).stat=true;}};
    flags.Run();Require(flags.state.slots==std::vector<Id>({101u,0u,301u,301u})&&flags.state.split==3,
        "Two-pass static values were cached, deduplicated or reclassified after selection");++cases;
    Context growing;growing.state.slots={101u,0u,201u};growing.after=[](Context& c,const Event& event){if(event.kind=='D'){
        c.state.slots.push_back(401u);c.state.slots.push_back(402u);}};
    growing.Run();Require(growing.state.slots==std::vector<Id>({101u,0u,201u,402u})&&growing.state.split==2,
        "Second pass used a captured first-pass count or re-ran the static phase for later births");++cases;
    Context during;during.state.slots={101u,0u,201u};unsigned countCalls{};
    during.after=[&](Context& c,const Event& event){if(event.kind=='N'&&++countCalls==2u){c.state.slots.push_back(301u);}};
    during.Run();Require(during.state.slots==std::vector<Id>({101u,0u,301u,201u})&&during.state.split==3,
        "Static pass did not re-read the live growing count");++cases;
    Context reread;reread.state.slots={101u,0u,301u};bool selected{};
    reread.after=[&](Context& c,const Event& event){if(!selected&&event.kind=='S'){selected=true;c.state.slots[2u]=302u;}};
    reread.Run();Require(reread.state.slots==std::vector<Id>({101u,0u,302u}),"Selected source slot was frozen across the original retained-slot reread");++cases;
}
void RefusalPrefixes(){
    Context complete;complete.Run();const auto used=complete.work.Read().used;
    const auto root=[](Context& context){const auto before=context.state;const bool failed=context.Refuses();
        Require(failed,"Injected original ordering prefix unexpectedly completed");
        Require(context.scratch==context.scratchBaseline,"Refused prefix leaked temporary scratch ownership");
        // Mock caller root owns rollback, not the coordinator. No fake native
        // transaction is smuggled into the production helper.
        context.state=before;Require(context.state==before,"Mock caller root could not restore its before-image");};
    for(std::size_t prefix=0u;prefix<complete.providers;++prefix){Context context;context.providerLimit=prefix;root(context);++providerRefusals;++cases;}
    for(std::size_t prefix=0u;prefix<used;++prefix){Context context;context.work.Reset(prefix);root(context);++workRefusals;++cases;}
    for(std::size_t prefix=0u;prefix<complete.admissions;++prefix){Context context;context.admissionLimit=prefix;root(context);++admissionRefusals;++cases;}
    for(std::size_t prefix=0u;prefix<2u;++prefix){Context context;context.writeLimit=prefix;root(context);++writeRefusals;++cases;}
    for(const auto selected:{'D','P'}){Context context;const auto before=context.state;
        context.after=[=](Context&,const Event& event){if(event.kind==selected)throw Injected{};};
        Require(context.Refuses()&&context.state.split==5,"Provider's reached split mutation was erased by a helper-local rollback");
        Require(context.replaced==(selected=='P'),"Refusal did not retain exactly its reached replacement prefix");
        context.state=before;Require(context.state==before&&context.scratch==context.scratchBaseline,"Outer mock root rollback failed");++cases;}
    // Old+new reserve peaks must be admitted while old storage is retained.
    Context many;many.state.slots={101u,0u};for(Id id=2u;id<80u;++id){many.state.slots.push_back(1000u+id);many.state.actors.emplace(1000u+id,Actor{id%2u==0u,false,true});}
    const auto before=many.state;many.Run();Require(many.peak>many.scratchBaseline+80u*sizeof(Id),"Reserve growth failed to charge simultaneous old/new pointer capacities");
    std::vector<Event> admitted;for(const auto& event:many.trace)if(event.kind=='A')admitted.push_back(event);
    Require(admitted.size()>=5u&&(admitted.size()%2u)==1u&&admitted.front()==Event{'A',many.scratchBaseline,sizeof(std::vector<Id>)},
        "Temporary vector header or owning capacity admission sequence was omitted");
    auto retained=many.scratchBaseline+sizeof(std::vector<Id>);
    for(std::size_t i=1u;i<admitted.size();i+=2u) {
        Require(admitted[i].a==retained&&admitted[i+1u].a==retained&&admitted[i+1u].b>=admitted[i].b,
            "Requested/actual reserve peak was not admitted against the unchanged OLD capacity ledger");
        retained=many.scratchBaseline+sizeof(std::vector<Id>)+admitted[i+1u].b;
    }
    Preserved(before,many);++cases;
    for(const auto cap:{std::size_t{0u},many.peak-1u}){Context context;context.state=before;context.byteLimit=cap;root(context);++admissionRefusals;++cases;}
}
void InvalidBindings(){
    for(const auto count:{-1,0,1,1'000'001}){Context context;auto q=context.Bind();q.actorCount=[=](Id){return count;};bool failed{};
        try{QuestVr::ReorderOriginalWorldActors(Level,q,context.scratch);}catch(const std::runtime_error&){failed=true;}
        Require(failed&&context.writes==0u&&context.scratch==context.scratchBaseline,"Invalid original count was read/allocated/published");++cases;}
    for(const unsigned missing:{0u,1u,2u,3u,4u,5u,6u}){Context context;auto q=context.Bind();switch(missing){
        case 0u:q.actorCount={};break;case 1u:q.actorSlot={};break;case 2u:q.isStatic={};break;
        case 3u:q.writeFirstDynamic={};break;case 4u:q.replaceSlots={};break;case 5u:q.admitTemporaryBytes={};break;case 6u:q.work={};break;}
        bool failed{};try{QuestVr::ReorderOriginalWorldActors(Level,q,context.scratch);}catch(const std::invalid_argument&){failed=true;}
        Require(failed&&context.scratch==context.scratchBaseline,"Missing reached provider bypassed native ownership or leaked scratch");++cases;}
    Context inactive;inactive.state.slots={101u,0u};auto q=inactive.Bind();q.isStatic={};QuestVr::ReorderOriginalWorldActors(Level,q,inactive.scratch);
    Require(inactive.state.split==2&&inactive.state.slots==std::vector<Id>({101u,0u}),"Unreached static provider was required");++cases;
    Context noLevel;auto none=noLevel.Bind();bool failed{};try{QuestVr::ReorderOriginalWorldActors(0u,none,noLevel.scratch);}catch(const std::invalid_argument&){failed=true;}
    Require(failed&&noLevel.providers==0u,"Null retained Level invoked native providers");++cases;
    Context overflow;overflow.scratch=Unlimited;auto huge=overflow.Bind();failed=false;
    try{QuestVr::ReorderOriginalWorldActors(Level,huge,overflow.scratch);}catch(const std::runtime_error&){failed=true;}
    Require(failed&&overflow.scratch==Unlimited&&overflow.writes==0u,"Scratch header overflow bypassed bounded admission");++cases;
}
}
int main(){try{Ordinary();LiveReads();RefusalPrefixes();InvalidBindings();
    std::cout<<"PASS original LoadMap Actor-order MOCK coordinator: "<<cases<<" cases / "<<checks<<" checks; "
        <<providerRefusals<<" provider, "<<workRefusals<<" borrowed-work, "<<admissionRefusals<<" admission and "<<writeRefusals
        <<" write-prefix refusals. No runtime/world/save/physics acceptance claim.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL world Actor order: "<<error.what()<<'\n';return 1;}}
