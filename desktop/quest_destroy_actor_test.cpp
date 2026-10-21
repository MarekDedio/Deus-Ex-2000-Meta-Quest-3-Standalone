#include "quest_destroy_actor.h"

#include <algorithm>
#include <array>
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

// ALL lower services in this executable are explicit mocks. This verifies the
// complete original ordering contract, NOT a runtime DestroyActor provider,
// executed commercial code, real-map startup, loot, physics or UObject teardown.
namespace {
using Pointer=std::uintptr_t;
using Queries=QuestVr::OriginalDestroyActorQueries;
using Event=QuestVr::OriginalDestroyActorEvent;
constexpr Pointer Level=100u,Source=10u,A=11u,B=12u,C=13u,D=14u;
constexpr Pointer Player=20u,ConnectionTarget=21u,Frame=30u,Code=31u;
constexpr Pointer Hash=40u,OtherHash=41u,Net=50u,Demo=51u,Engine=60u,Audio=61u,Undo=70u;
std::size_t checks{},cases{},refusals{},callbackCases{},providerFailures{},workFailures{};
void Require(bool value,const std::string& reason) {
    ++checks;if(!value)throw std::runtime_error(reason);
}
enum class Op {
    Valid,Editor,Flags,Role,PlayerPawn,Player,NetConnection,ConnectionTargetE64,ConnectionState60,ConnectionVirtual5C,
    GetIndex,Undo,SaveItem,Modify,Frame,Code,Probe,FindFunction,EndState,Destroyed,LostChild,Pending,
    Base,SetBase,Standing,Count,Slot,Hash,Collide,Remove,CheckNotReferenced,Owner,SetOwner,Touch,EndTouch,
    Net,Demo,ServerConnection,Notify,ClearSlot,SetPending,Engine,Audio,AudioNotify,Conditional,
    FirstDeleted,WriteDeleted,WriteFirstDeleted,Cleanup
};
struct Record {
    Op op{};Pointer a{},b{},c{},d{};
    bool operator==(const Record&)const=default;
};
struct Actor {
    std::uint32_t flags{},objectFlags{};
    std::uint8_t role{4u},standing{};
    Pointer base{},owner{},frame{},player{},deleted{};
    std::array<Pointer,4u> touching{};
    std::int32_t collide{};
    bool operator==(const Actor&)const=default;
};
struct Mock {
    std::map<Pointer,Actor> actors{{Source,{}},{A,{}},{B,{}},{C,{}},{D,{}}};
    std::vector<Pointer> slots{0u,Source,0u,A};
    std::int32_t valid{1},editor{},pawn{},connection{},connectionState{1},probe{},conditionalResult{1};
    Pointer connectionTarget{},frameCode{},undo{},hash{},net{},demo{},server{},engine{Engine},audio{},firstDeleted{};
    std::optional<std::int32_t> indexOverride,countOverride;
    bool nullFunction{},reference{},nested{};
    unsigned mutation{},editorReads{},conditionalCalls{};
    std::vector<QuestVr::OriginalDestroyActorUndoItem> savedItems;
    std::vector<std::pair<Pointer,Pointer>> driverNotifications,audioNotifications;
    std::vector<std::int32_t> cleanups;
    std::vector<Record> trace;
    std::function<void(Mock&,const Record&)> hook;
    std::optional<std::size_t> failProvider;
    std::size_t workUsed{},workLimit{std::numeric_limits<std::size_t>::max()};
    void Enter(Record record) {
        trace.push_back(record);
        if(failProvider&&trace.size()==*failProvider)throw std::runtime_error("Injected native-provider refusal");
    }
    void After(const Record& record) {if(hook)hook(*this,record);}
    template<class Value> Value Read(Op op,Value value,Pointer a=0u,Pointer b=0u,Pointer c=0u) {
        const Record record{op,a,b,c,0u};Enter(record);After(record);return value;
    }
    template<class Action> void Write(Op op,Pointer a,Pointer b,Pointer c,Action&& action,Pointer d=0u) {
        const Record record{op,a,b,c,d};Enter(record);action();After(record);
    }
    std::int32_t Index(Pointer actor)const {
        if(indexOverride)return *indexOverride;
        for(std::size_t slot=0u;slot<slots.size();++slot)if(slots[slot]==actor)return static_cast<std::int32_t>(slot);
        return -1;
    }
    bool SameData(const Mock& other)const {
        const auto item=[](const QuestVr::OriginalDestroyActorUndoItem& a,const QuestVr::OriginalDestroyActorUndoItem& b){
            return a.index==b.index&&a.count==b.count&&a.operation==b.operation&&a.originalElementBytes==b.originalElementBytes;
        };
        return std::tie(actors,slots,valid,editor,pawn,connection,connectionState,probe,conditionalResult,
            connectionTarget,frameCode,undo,hash,net,demo,server,engine,audio,firstDeleted,indexOverride,countOverride,
            nullFunction,nested,mutation,editorReads,conditionalCalls,driverNotifications,audioNotifications,cleanups)==
            std::tie(other.actors,other.slots,other.valid,other.editor,other.pawn,other.connection,other.connectionState,
            other.probe,other.conditionalResult,other.connectionTarget,other.frameCode,other.undo,other.hash,other.net,
            other.demo,other.server,other.engine,other.audio,other.firstDeleted,other.indexOverride,other.countOverride,
            other.nullFunction,other.nested,other.mutation,other.editorReads,other.conditionalCalls,
            other.driverNotifications,other.audioNotifications,other.cleanups)&&
            savedItems.size()==other.savedItems.size()&&std::equal(savedItems.begin(),savedItems.end(),other.savedItems.begin(),item);
    }
};
Pointer Word(std::int32_t value) {return static_cast<Pointer>(static_cast<std::uint32_t>(value));}
Queries Providers(Mock& mock) {
    Queries q;
    q.isValid=[&](Pointer a){return mock.Read(Op::Valid,mock.valid,a);};
    q.editor=[&]{++mock.editorReads;return mock.Read(Op::Editor,mock.editor);};
    q.actorFlags=[&](Pointer a){return mock.Read(Op::Flags,mock.actors.at(a).flags,a);};
    q.role=[&](Pointer a){return mock.Read(Op::Role,mock.actors.at(a).role,a);};
    q.playerPawn=[&](Pointer a){return mock.Read(Op::PlayerPawn,mock.pawn,a);};
    q.player=[&](Pointer a){return mock.Read(Op::Player,mock.actors.at(a).player,a);};
    q.netConnection=[&](Pointer a){return mock.Read(Op::NetConnection,mock.connection,a);};
    q.connectionTargetE64=[&](Pointer a){return mock.Read(Op::ConnectionTargetE64,mock.connectionTarget,a);};
    q.connectionState60=[&](Pointer a){return mock.Read(Op::ConnectionState60,mock.connectionState,a);};
    q.connectionTargetVirtual5C=[&](Pointer a){mock.Write(Op::ConnectionVirtual5C,a,0u,0u,[]{});};
    q.getActorIndex=[&](Pointer l,Pointer a){return mock.Read(Op::GetIndex,mock.Index(a),l,a);};
    q.undo=[&]{return mock.Read(Op::Undo,mock.undo);};
    q.saveActorArrayItem=[&](Pointer u,Pointer l,QuestVr::OriginalDestroyActorUndoItem item){
        Require(item.count==1&&item.operation==0&&item.originalElementBytes==4u,"Undo lost literal one-item0/4-byte source contract");
        mock.Write(Op::SaveItem,u,l,Word(item.index),[&]{mock.savedItems.push_back(item);});
    };
    q.modify=[&](Pointer a){mock.Write(Op::Modify,a,0u,0u,[]{});};
    q.stateFrame=[&](Pointer a){return mock.Read(Op::Frame,mock.actors.at(a).frame,a);};
    q.stateCode=[&](Pointer f){return mock.Read(Op::Code,mock.frameCode,f);};
    q.isProbing=[&](Pointer a,std::uint32_t name){return mock.Read(Op::Probe,mock.probe,a,name);};
    q.findFunctionChecked=[&](Pointer a,Event e){
        const auto function=mock.nullFunction?0u:a*16u+static_cast<Pointer>(e)+1u;
        return mock.Read(Op::FindFunction,function,a,static_cast<Pointer>(e));
    };
    q.processEvent=[&](Pointer a,Pointer function,Pointer argument){
        Require(function/16u==a,"ProcessEvent receiver changed after selected-function capture");
        const auto event=static_cast<Event>((function-1u)%16u);
        const auto op=event==Event::EndState?Op::EndState:event==Event::Destroyed?Op::Destroyed:Op::LostChild;
        Require(event==Event::LostChild?argument==Source:argument==0u,"Selected event parameter packet differs from original signature");
        mock.Write(op,a,function,argument,[]{});
    };
    q.pendingDelete=[&](Pointer a){return mock.Read(Op::Pending,(mock.actors.at(a).flags&0x80u)?-7:0,a);};
    q.base=[&](Pointer a){return mock.Read(Op::Base,mock.actors.at(a).base,a);};
    q.setBase=[&](Pointer a,Pointer b,std::int32_t notify){
        Require(b==0u&&notify==1,"Destroy SetBase did not pass None/true1");
        mock.Write(Op::SetBase,a,b,Word(notify),[&]{mock.actors.at(a).base=b;});
    };
    q.standingCount=[&](Pointer a){return mock.Read(Op::Standing,mock.actors.at(a).standing,a);};
    q.actorCount=[&](Pointer l){return mock.Read(Op::Count,mock.countOverride.value_or(static_cast<std::int32_t>(mock.slots.size())),l);};
    q.actorSlot=[&](Pointer l,std::int32_t slot){return mock.Read(Op::Slot,mock.slots.at(static_cast<std::size_t>(slot)),l,Word(slot));};
    q.levelHash=[&](Pointer l){return mock.Read(Op::Hash,mock.hash,l);};
    q.collideActors=[&](Pointer a){return mock.Read(Op::Collide,mock.actors.at(a).collide,a);};
    q.removeFromHash=[&](Pointer h,Pointer a){mock.Write(Op::Remove,h,a,0u,[]{});};
    q.checkActorNotReferenced=[&](Pointer h,Pointer a){mock.Write(Op::CheckNotReferenced,h,a,0u,[]{});};
    q.owner=[&](Pointer a){return mock.Read(Op::Owner,mock.actors.at(a).owner,a);};
    q.setOwner=[&](Pointer a,Pointer owner){mock.Write(Op::SetOwner,a,owner,0u,[&]{mock.actors.at(a).owner=owner;});};
    q.touching=[&](Pointer a,unsigned slot){return mock.Read(Op::Touch,mock.actors.at(a).touching.at(slot),a,slot);};
    q.endTouch=[&](Pointer a,Pointer b,std::int32_t suppress){
        Require(a==Source&&suppress==1,"Destroy EndTouch receiver/direction/suppressSelf1 changed");
        mock.Write(Op::EndTouch,a,b,Word(suppress),[&]{
            for(auto& slot:mock.actors.at(a).touching)if(slot==b){slot=0u;break;}
            for(auto& slot:mock.actors.at(b).touching)if(slot==a){slot=0u;break;}
        });
    };
    q.netDriver=[&](Pointer l){return mock.Read(Op::Net,mock.net,l);};
    q.demoRecDriver=[&](Pointer l){return mock.Read(Op::Demo,mock.demo,l);};
    q.serverConnection=[&](Pointer driver){return mock.Read(Op::ServerConnection,mock.server,driver);};
    q.notifyActorDestroyed=[&](Pointer driver,Pointer a){
        mock.Write(Op::Notify,driver,a,0u,[&]{mock.driverNotifications.emplace_back(driver,a);});
    };
    q.clearActorSlot=[&](Pointer l,std::int32_t slot){
        mock.Write(Op::ClearSlot,l,Word(slot),0u,[&]{mock.slots.at(static_cast<std::size_t>(slot))=0u;});
    };
    q.setPendingDelete=[&](Pointer a){mock.Write(Op::SetPending,a,0u,0u,[&]{mock.actors.at(a).flags|=0x80u;});};
    q.engine=[&](Pointer l){return mock.Read(Op::Engine,mock.engine,l);};
    q.audio=[&](Pointer engine){return mock.Read(Op::Audio,mock.audio,engine);};
    q.audioActorDestroyed=[&](Pointer audio,Pointer a){
        mock.Write(Op::AudioNotify,audio,a,0u,[&]{mock.audioNotifications.emplace_back(audio,a);});
    };
    q.conditionalDestroy=[&](Pointer a){
        const auto result=mock.conditionalResult;
        mock.Write(Op::Conditional,a,0u,0u,[&]{++mock.conditionalCalls;mock.actors.at(a).objectFlags|=0x80800000u;});return result;
    };
    q.firstDeleted=[&](Pointer l){return mock.Read(Op::FirstDeleted,mock.firstDeleted,l);};
    q.writeDeleted=[&](Pointer a,Pointer next){mock.Write(Op::WriteDeleted,a,next,0u,[&]{mock.actors.at(a).deleted=next;});};
    q.writeFirstDeleted=[&](Pointer l,Pointer a){mock.Write(Op::WriteFirstDeleted,l,a,0u,[&]{mock.firstDeleted=a;});};
    q.cleanupDestroyed=[&](Pointer l,std::int32_t force){mock.Write(Op::Cleanup,l,Word(force),0u,[&]{mock.cleanups.push_back(force);});};
    q.work=[&]{if(mock.workUsed>=mock.workLimit)throw std::runtime_error("Shared caller work allowance exhausted");++mock.workUsed;};
    return q;
}

// Separate assembly-shaped PC interpreter. It does NOT call any coordinator,
// helper Detail routine or a copied actor-array snapshot. Labels correspond to
// the original admission/scan/callback jump sites; native services stay mocks.
template<class Function,class... Arguments>
auto RefCall(const Queries& q,const Function& f,Arguments... arguments) {
    if(!f)throw std::invalid_argument("Reference reached missing native provider");
    if(q.work)q.work();
    return f(arguments...);
}
void RefEvent(const Queries& q,Pointer receiver,Event e,Pointer argument=0u) {
    const auto function=RefCall(q,q.findFunctionChecked,receiver,e);
    if(!function)throw std::runtime_error("Reference FindFunctionChecked assertion");
    RefCall(q,q.processEvent,receiver,function,argument);
}
std::int32_t Reference(Pointer level,Pointer source,std::int32_t force,const Queries& q) {
    std::uint32_t flags{};std::int32_t saved{},count{};std::uint64_t index{};unsigned touch{};
    Pointer player{},target{},undo{},frame{},candidate{},hash{},owner{},driver{},engine{},audio{};
    if(!level||!source)throw std::invalid_argument("Reference null native assertion");
    if(!RefCall(q,q.isValid,source))throw std::runtime_error("Reference IsValid assertion");
    if(RefCall(q,q.editor))goto Common;
    flags=RefCall(q,q.actorFlags,source);
    if(flags&5u)return 0;
    if(flags&128u)return 1;
    if(RefCall(q,q.role,source)==4u)goto PlayerClass;
    if(force!=0)goto PlayerClass;
    if(!(flags&0x01000000u))return 0;
PlayerClass:
    if(!RefCall(q,q.playerPawn,source))goto Common;
    player=RefCall(q,q.player,source);if(!player)goto Common;
    if(!RefCall(q,q.netConnection,player))goto Common;
    target=RefCall(q,q.connectionTargetE64,player);if(!target)goto Common;
    if(RefCall(q,q.connectionState60,player)==1)goto Common;
    RefCall(q,q.connectionTargetVirtual5C,target);return 0;
Common:
    saved=RefCall(q,q.getActorIndex,level,source);
    if(saved<0)throw std::runtime_error("Reference GetActorIndex assertion");
    undo=RefCall(q,q.undo);
    if(undo)RefCall(q,q.saveActorArrayItem,undo,level,QuestVr::OriginalDestroyActorUndoItem{saved,1,0,4u});
    RefCall(q,q.modify,source);
    frame=RefCall(q,q.stateFrame,source);if(!frame)goto SourceBase;
    if(!RefCall(q,q.stateCode,frame))goto SourceBase;
    if(!RefCall(q,q.isProbing,source,0x13du))goto SourceBase;
    RefEvent(q,source,Event::EndState);
    if(RefCall(q,q.pendingDelete,source))return 1;
SourceBase:
    if(!RefCall(q,q.base,source))goto Standing;
    RefCall(q,q.setBase,source,Pointer{},1);
    if(RefCall(q,q.pendingDelete,source))return 1;
Standing:
    if(!RefCall(q,q.standingCount,source))goto HashEntry;
    index=0u;
BaseTest:
    count=RefCall(q,q.actorCount,level);if(count<0)throw std::runtime_error("Reference invalid array count");
    if(index>=static_cast<std::uint64_t>(count))goto HashEntry;
    candidate=RefCall(q,q.actorSlot,level,static_cast<std::int32_t>(index));
    if(!candidate)goto BaseNext;
    if(RefCall(q,q.base,candidate)==source)RefCall(q,q.setBase,candidate,Pointer{},1);
BaseNext:
    ++index;goto BaseTest;
HashEntry:
    hash=RefCall(q,q.levelHash,level);if(!hash)goto Destroyed;
    if(RefCall(q,q.collideActors,source))RefCall(q,q.removeFromHash,hash,source);
    hash=RefCall(q,q.levelHash,level);
    if(!hash)throw std::runtime_error("Reference vanished Hash assertion");
    RefCall(q,q.checkActorNotReferenced,hash,source);
Destroyed:
    RefEvent(q,source,Event::Destroyed);
    if(RefCall(q,q.pendingDelete,source))return 1;
    index=0u;
OwnerTest:
    count=RefCall(q,q.actorCount,level);if(count<0)throw std::runtime_error("Reference invalid array count");
    if(index>=static_cast<std::uint64_t>(count))goto SourceOwner;
    candidate=RefCall(q,q.actorSlot,level,static_cast<std::int32_t>(index));
    if(!candidate)goto OwnerNext;
    if(RefCall(q,q.owner,candidate)!=source)goto TouchStart;
    RefCall(q,q.setOwner,candidate,Pointer{});
    if(RefCall(q,q.pendingDelete,source))return 1;
    goto OwnerNext;
TouchStart:
    touch=0u;
TouchTest:
    if(touch>=4u)goto OwnerNext;
    if(RefCall(q,q.touching,candidate,touch)!=source)goto TouchNext;
    RefCall(q,q.endTouch,source,candidate,1);
    if(RefCall(q,q.pendingDelete,source))return 1;
TouchNext:
    ++touch;goto TouchTest;
OwnerNext:
    ++index;goto OwnerTest;
SourceOwner:
    owner=RefCall(q,q.owner,source);
    if(!owner)goto Drivers;
    RefEvent(q,owner,Event::LostChild,source);
    if(RefCall(q,q.pendingDelete,source))return 1;
Drivers:
    driver=RefCall(q,q.netDriver,level);
    if(driver)RefCall(q,q.notifyActorDestroyed,driver,source);
    driver=RefCall(q,q.demoRecDriver,level);if(!driver)goto Slot;
    if(!RefCall(q,q.serverConnection,driver))RefCall(q,q.notifyActorDestroyed,driver,source);
Slot:
    if(RefCall(q,q.actorSlot,level,saved)!=source)throw std::runtime_error("Reference saved-slot assertion");
    RefCall(q,q.clearActorSlot,level,saved);
    RefCall(q,q.setPendingDelete,source);
    engine=RefCall(q,q.engine,level);if(!engine)throw std::runtime_error("Reference native Engine assertion");
    audio=RefCall(q,q.audio,engine);
    if(audio)RefCall(q,q.audioActorDestroyed,audio,source);
    static_cast<void>(RefCall(q,q.conditionalDestroy,source));
    if(RefCall(q,q.editor))goto EditorTail;
    candidate=RefCall(q,q.firstDeleted,level);
    RefCall(q,q.writeDeleted,source,candidate);
    RefCall(q,q.writeFirstDeleted,level,source);return 1;
EditorTail:
    RefCall(q,q.cleanupDestroyed,level,1);return 1;
}
struct Result {Mock state;std::optional<std::int32_t> value;bool refused{};};
using Customize=std::function<void(Queries&)>;
Result Execute(Mock input,std::int32_t force,bool reference,const Customize& customize={},Pointer level=Level,Pointer source=Source) {
    input.reference=reference;auto q=Providers(input);if(customize)customize(q);
    Result result;
    try {result.value=reference?Reference(level,source,force,q):QuestVr::DestroyOriginalLevelActor(level,source,force,q);}
    catch(const std::exception&) {result.refused=true;}
    result.state=std::move(input);return result;
}
Result Compare(const Mock& input,std::int32_t force=0,const Customize& customize={},Pointer level=Level,Pointer source=Source) {
    auto actual=Execute(input,force,false,customize,level,source);auto reference=Execute(input,force,true,customize,level,source);
    Require(actual.refused==reference.refused&&actual.value==reference.value,"Coordinator result/refusal differs from independent PC interpreter");
    if(actual.state.trace!=reference.state.trace) {
        std::size_t index{};while(index<actual.state.trace.size()&&index<reference.state.trace.size()&&actual.state.trace[index]==reference.state.trace[index])++index;
        throw std::runtime_error("Coordinator native-provider order/arguments differ from PC interpreter at trace"+std::to_string(index));
    }
    Require(actual.state.SameData(reference.state),"Coordinator native callback/slot/deferred graph differs from independent PC interpreter");
    Require(actual.state.workUsed==reference.state.workUsed,"Coordinator acquired/reset a different shared work allowance");
    ++cases;if(actual.refused)++refusals;return actual;
}
std::size_t Calls(const Mock& state,Op op,Pointer receiver=0u) {
    return static_cast<std::size_t>(std::count_if(state.trace.begin(),state.trace.end(),[&](const Record& record){return record.op==op&&(!receiver||record.a==receiver);}));
}
Mock Full() {
    Mock state;state.slots={0u,A,Source,B,0u,C};
    state.undo=Undo;state.actors.at(Source).frame=Frame;state.frameCode=Code;state.probe=-7;
    state.actors.at(Source).base=D;state.actors.at(Source).standing=255u;
    state.actors.at(A).base=Source;state.actors.at(B).base=Source;
    state.hash=Hash;state.actors.at(Source).collide=-9;
    state.actors.at(A).owner=Source;state.actors.at(A).touching={Source,Source,Source,Source};
    state.actors.at(B).touching={Source,0u,Source,0u};state.actors.at(Source).touching={B,B,0u,0u};
    state.actors.at(Source).owner=D;state.net=Net;state.demo=Demo;state.audio=Audio;state.firstDeleted=C;
    state.pawn=-1;state.actors.at(Source).player=Player;state.connection=-7;state.connectionTarget=ConnectionTarget;
    return state;
}

void AdmissionMatrix() {
    for(const auto flags:{0u,1u,4u,5u,0x80u,0x81u,0x84u,0x01000000u,0x01000080u,0x80000000u,0xffffffffu})
        for(const auto role:{0u,1u,3u,4u,5u,255u})
            for(const auto force:{0,1,-1,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()})
                for(const auto editor:{0,1,-7}) {
                    Mock input;input.editor=editor;input.actors.at(Source).flags=flags;input.actors.at(Source).role=static_cast<std::uint8_t>(role);
                    const auto result=Compare(input,force);
                    const auto expected=editor?1:(flags&5u)?0:(flags&128u)?1:(role==4u||force||(flags&0x01000000u))?1:0;
                    Require(!result.refused&&result.value==expected,"Raw flags/role/force/editor admission differs from original mask ordering");
                    if(!editor&&!(flags&0x85u)&&force)Require(Calls(result.state,Op::Role)==1u,"Force nonzero skipped the original Role byte read");
                }
    for(const auto valid:{0,1,-7}) {
        Mock input;input.valid=valid;const auto result=Compare(input);
        Require(result.refused==(valid==0),"UObject.IsValid treated nonzero native Bool as invalid or accepted zero");
    }
    const auto minimal=[](Queries& q){
        const auto valid=q.isValid;const auto editor=q.editor;const auto flags=q.actorFlags;
        q={};q.isValid=valid;q.editor=editor;q.actorFlags=flags;
    };
    Mock blocked;blocked.actors.at(Source).flags=0x85u;
    const auto early=Compare(blocked,0,minimal);
    Require(!early.refused&&early.value==0&&early.state.trace.size()==3u,"Static/no-delete mask did not precede pending idempotence lazily");
    blocked.actors.at(Source).flags=0x80u;const auto pending=Compare(blocked,0,minimal);
    Require(!pending.refused&&pending.value==1&&pending.state.trace.size()==3u,"Already-pending path demanded inactive native providers");
}
void NetworkMatrix() {
    for(const auto pawn:{0,1,-1})for(const auto player:{Pointer{},Player})for(const auto connection:{0,1,-7})
        for(const auto target:{Pointer{},ConnectionTarget})
            for(const auto state:{std::numeric_limits<std::int32_t>::min(),-1,0,1,2,std::numeric_limits<std::int32_t>::max()}) {
                Mock input;input.pawn=pawn;input.actors.at(Source).player=player;input.connection=connection;
                input.connectionTarget=target;input.connectionState=state;const auto result=Compare(input);
                const bool close=pawn&&player&&connection&&target&&state!=1;
                Require(!result.refused&&result.value==(close?0:1)&&Calls(result.state,Op::ConnectionVirtual5C)==(close?1u:0u),
                    "Native Player/NetConnection/target/state branch confused Viewport/null/class or raw state values");
                if(!pawn)Require(Calls(result.state,Op::Player)==0u,"Non-PlayerPawn read the inactive Player slot");
                if(!target)Require(Calls(result.state,Op::ConnectionState60)==0u,"Null connection target read an inactive connection state");
            }
    auto input=Full();input.connectionState=-7;
    input.hook=[](Mock& state,const Record& record){
        if(record.op==Op::ConnectionState60)state.connectionTarget=ConnectionTarget+1u;
        if(record.op==Op::ConnectionVirtual5C){state.actors.at(Source).owner=A;state.mutation=17u;}
    };
    const auto result=Compare(input);++callbackCases;
    Require(!result.refused&&result.value==0&&Calls(result.state,Op::ConnectionVirtual5C,ConnectionTarget)==1u&&
        result.state.actors.at(Source).owner==A&&result.state.mutation==17u&&Calls(result.state,Op::GetIndex)==0u,
        "Network ordinary-false result retargeted the captured target or privately rolled back its native callback effects");
}
void FrameAndPendingGates() {
    for(const auto frame:{Pointer{},Frame})for(const auto code:{Pointer{},Code})for(const auto probe:{0,1,-7}) {
        Mock input;input.actors.at(Source).frame=frame;input.frameCode=code;input.probe=probe;
        const auto result=Compare(input);const bool end=frame&&code&&probe;
        Require(!result.refused&&Calls(result.state,Op::EndState)==(end?1u:0u),"EndState did not use actual frame.Code plus selected probe");
        Require(Calls(result.state,Op::Pending)==(end?2u:1u),"Uncalled EndState invented an extra pending-delete gate");
        if(end)for(const auto& record:result.state.trace)if(record.op==Op::Probe)
            Require(record.b==0x13du,"EndState probe lost primary original FName13D");
    }
    for(const auto stop:{Op::EndState,Op::SetBase,Op::Destroyed,Op::SetOwner,Op::EndTouch,Op::LostChild}) {
        auto input=Full();input.hook=[stop](Mock& state,const Record& record){
            if(record.op==stop&&(stop!=Op::SetBase||record.a==Source))state.actors.at(Source).flags|=0x80u;
        };
        const auto result=Compare(input);++callbackCases;
        Require(!result.refused&&result.value==1&&Calls(result.state,Op::ClearSlot)==0u&&Calls(result.state,Op::Conditional)==0u,
            "Pending-delete callback did not stop exactly before outer native slot/conditional teardown");
    }
    for(const auto probe:{0,1}) {
        Mock input;input.actors.at(Source).frame=Frame;input.frameCode=Code;input.probe=probe;
        input.hook=[](Mock& state,const Record& record){if(record.op==Op::Modify)state.actors.at(Source).flags|=0x80u;};
        const auto result=Compare(input);++callbackCases;
        Require(!result.refused&&Calls(result.state,Op::Destroyed)==(probe?0u:1u),
            "Pending bit after Modify was globally checked instead of only after an admitted EndState callback");
    }
}
void LiveScansAndRetainedIdentity() {
    {
        Mock input;input.slots={Source,A,Source};input.actors.at(Source).owner=D;
        const auto result=Compare(input);
        Require(!result.refused&&result.state.slots==std::vector<Pointer>{0u,A,Source}&&Calls(result.state,Op::GetIndex)==1u,
            "DestroyActor compacted/cleared all duplicate aliases instead of retaining and clearing only the original first-match slot");
    }
    {
        Mock input;input.slots={Source,A};input.actors.at(Source).standing=2u;
        input.actors.at(A).base=Source;input.actors.at(B).base=Source;input.hash=Hash;input.actors.at(Source).collide=1;
        input.hook=[](Mock& state,const Record& record){if(record.op==Op::SetBase&&record.a==A){
            state.slots.push_back(B);state.actors.at(Source).standing=0u;state.actors.at(Source).flags|=0x80u;
        }};
        const auto result=Compare(input);++callbackCases;
        Require(!result.refused&&Calls(result.state,Op::Standing)==1u&&Calls(result.state,Op::SetBase,B)==1u,
            "Standing gate was resampled or live newly appended based child was skipped");
        Require(Calls(result.state,Op::Remove)==1u&&Calls(result.state,Op::CheckNotReferenced)==1u&&
            Calls(result.state,Op::Destroyed)==1u&&Calls(result.state,Op::Pending)==1u,
            "Child support detach invented a pending-delete return before original hash/Destroyed ordering");
    }
    {
        Mock input;input.slots={Source,A};input.actors.at(A).owner=Source;input.actors.at(B).owner=Source;
        input.actors.at(A).touching={Source,Source,0u,0u};input.actors.at(B).touching={Source,0u,0u,0u};
        input.hook=[](Mock& state,const Record& record){if(record.op==Op::SetOwner&&record.a==A){
            state.actors.at(A).owner=Source;state.slots.push_back(0u);state.slots.push_back(B);
        }};
        const auto result=Compare(input);++callbackCases;
        Require(!result.refused&&Calls(result.state,Op::SetOwner,A)==1u&&Calls(result.state,Op::SetOwner,B)==1u,
            "Ownership scan cached actor count or failed to visit a newly appended owned candidate");
        Require(Calls(result.state,Op::Touch,A)==0u&&Calls(result.state,Op::Touch,B)==0u&&result.state.actors.at(A).owner==Source,
            "Owned-candidate branch rescanned a callback-retargeted owner or failed to skip its Touching slots");
    }
    {
        Mock input;input.slots={Source,A};input.actors.at(A).touching={Source,Source,0u,0u};input.actors.at(Source).touching={A,0u,0u,0u};
        input.hook=[](Mock& state,const Record& record){if(record.op==Op::EndTouch&&record.b==A&&state.mutation++==0u){
            state.actors.at(A).touching[3u]=Source;state.slots[1u]=B;
        }};
        const auto result=Compare(input);++callbackCases;
        Require(!result.refused&&Calls(result.state,Op::EndTouch)==3u&&Calls(result.state,Op::Touch,A)==4u&&Calls(result.state,Op::Touch,B)==0u,
            "Touch scan cached/compacted duplicate slots or lost its retained candidate after actor-array replacement");
    }
    {
        Mock input;input.actors.at(Source).owner=A;
        input.hook=[](Mock& state,const Record& record){if(record.op==Op::FindFunction&&record.a==A&&record.b==static_cast<Pointer>(Event::LostChild))
            state.actors.at(Source).owner=B;};
        const auto result=Compare(input);++callbackCases;
        Require(!result.refused&&Calls(result.state,Op::LostChild,A)==1u&&Calls(result.state,Op::LostChild,B)==0u&&
            result.state.actors.at(Source).owner==B&&Calls(result.state,Op::SetOwner,Source)==0u,
            "Captured source.Owner LostChild was retargeted or replaced with source.SetOwner(None)");
    }
    {
        Mock input;input.actors.at(Source).owner=Source;input.actors.at(Source).touching[0u]=Source;
        const auto result=Compare(input);
        Require(!result.refused&&Calls(result.state,Op::SetOwner,Source)==1u&&Calls(result.state,Op::Touch,Source)==0u&&Calls(result.state,Op::LostChild)==0u,
            "Pathological self-owned candidate was filtered instead of taking the original owner branch and skipping Touching");
    }
    for(const auto collide:{0,1,-7}) {
        Mock input;input.hash=Hash;input.actors.at(Source).collide=collide;
        input.hook=[](Mock& state,const Record& record){if(record.op==Op::Hash&&state.mutation++==0u)state.hash=OtherHash;};
        const auto result=Compare(input);++callbackCases;
        Require(!result.refused&&Calls(result.state,Op::Hash)==2u&&Calls(result.state,Op::CheckNotReferenced,OtherHash)==1u&&
            Calls(result.state,Op::Remove)==(collide?1u:0u),
            "Original Hash reload/CheckActorNotReferenced was omitted for collidefalse or replaced by the retained earlier Hash");
    }
    {
        Mock input;input.hash=Hash;input.actors.at(Source).collide=1;
        input.hook=[](Mock& state,const Record& record){if(record.op==Op::Remove)state.hash=0u;};
        const auto result=Compare(input);++callbackCases;
        Require(result.refused&&result.state.hash==0u&&Calls(result.state,Op::Destroyed)==0u&&Calls(result.state,Op::CheckNotReferenced)==0u,
            "Vanished live Hash was silently skipped or refusal privately rolled back a native callback's effects");
    }
    {
        Mock input;input.slots={A,Source};input.net=Net;
        input.hook=[](Mock& state,const Record& record){if(record.op==Op::Notify){state.slots[1u]=B;state.slots.push_back(Source);}};
        const auto result=Compare(input);++callbackCases;
        Require(result.refused&&Calls(result.state,Op::GetIndex)==1u&&Calls(result.state,Op::ClearSlot)==0u&&Calls(result.state,Op::SetPending)==0u,
            "Driver-time saved-slot replacement triggered a rescan/clear of another slot instead of the exact original assertion");
    }
    {
        Mock input;input.net=Net;input.demo=Demo;
        input.hook=[](Mock& state,const Record& record){if(record.op==Op::Notify&&record.a==Net){
            state.demo=Demo+1u;state.actors.at(Source).flags|=0x80u;
        }};
        const auto result=Compare(input);++callbackCases;
        Require(!result.refused&&Calls(result.state,Op::Notify,Demo+1u)==1u&&Calls(result.state,Op::ClearSlot)==1u&&Calls(result.state,Op::Conditional)==1u,
            "Driver callbacks did not retain live demo lookup or invented a late pending-delete early return");
    }
    for(const auto server:{Pointer{},Player}) {
        Mock input;input.demo=Demo;input.server=server;const auto result=Compare(input);
        Require(!result.refused&&Calls(result.state,Op::Notify,Demo)==(server?0u:1u),"DemoRecDriver ServerConnection predicate was reversed or omitted");
    }
    {
        Mock input;input.actors.at(Source).role=1u;input.actors.at(Source).flags=0x01000000u;
        input.hook=[](Mock& state,const Record& record){if(record.op==Op::Role)state.actors.at(Source).flags=0u;};
        const auto result=Compare(input);++callbackCases;
        Require(!result.refused&&result.value==1&&Calls(result.state,Op::Flags)==1u,
            "Role-time mutation replaced the original retained admission flag DWORD");
    }
}
void ConditionalAndTailMatrix() {
    for(const auto conditional:{0,1,-1,std::numeric_limits<std::int32_t>::min()})for(const auto initialEditor:{0,-7})
        for(const auto finalEditor:{0,1,-9})for(const auto audio:{Pointer{},Audio})for(const auto head:{Pointer{},C,Source}) {
            Mock input;input.editor=initialEditor;input.conditionalResult=conditional;input.audio=audio;input.firstDeleted=head;
            input.hook=[finalEditor](Mock& state,const Record& record){
                if(record.op==Op::Destroyed)Require(!(state.actors.at(Source).flags&0x80u),"Destroy coordinator set pending before selected Destroyed");
                if(record.op==Op::AudioNotify||record.op==Op::Conditional)Require(state.slots[1u]==0u&&
                    (state.actors.at(Source).flags&0x80u),"Audio/ConditionalDestroy ran before slot-hole/pending stores");
                if(record.op==Op::Conditional)state.editor=finalEditor;
            };
            const auto result=Compare(input);++callbackCases;
            Require(!result.refused&&result.value==1&&result.state.conditionalCalls==1u,
                "ConditionalDestroy's ignored native result changed normal DestroyActor success");
            Require(Calls(result.state,Op::Editor)==2u&&Calls(result.state,Op::AudioNotify)==(audio?1u:0u),
                "Native Engine.Audio presence or final editor reread was replaced with an initial cached decision");
            if(finalEditor)Require(result.state.cleanups==std::vector<std::int32_t>{1}&&Calls(result.state,Op::FirstDeleted)==0u,
                "Editor tail did not invoke exact CleanupDestroyed(1) or also mutated deferred list");
            else Require(result.state.cleanups.empty()&&result.state.firstDeleted==Source&&result.state.actors.at(Source).deleted==head,
                "Ordinary tail did not unconditionally prepend source to the actual deferred list");
        }
}
void ReentrantCallbacks() {
    for(const bool nestedReject:{false,true}) {
        Mock input;input.slots={Source,A};
        input.hook=[nestedReject](Mock& state,const Record& record){if(record.op==Op::Destroyed&&record.a==Source&&!state.nested){
            state.nested=true;if(nestedReject)state.actors.at(Source).flags|=4u;
            auto q=Providers(state);const auto result=state.reference?Reference(Level,Source,0,q):QuestVr::DestroyOriginalLevelActor(Level,Source,0,q);
            Require(result==(nestedReject?0:1),"Nested original DestroyActor returned an invented idempotence disposition");
        }};
        const auto result=Compare(input);++callbackCases;
        Require(!result.refused&&result.value==1&&Calls(result.state,Op::ClearSlot)==1u&&Calls(result.state,Op::Conditional)==1u,
            "Reentrant destruction double-cleared/tore down source or re-applied admission after an ordinary nested false result");
        Require(Calls(result.state,Op::Destroyed)==(nestedReject?1u:2u),"Reentrant callback event selection was suppressed by a broad native guard");
    }
}
void MissingAndInvalidProviders() {
    const std::vector<std::pair<const char*,Customize>> missing{
#define MISSING(name) {#name,[](Queries& q){q.name={};}}
        MISSING(isValid),MISSING(editor),MISSING(actorFlags),MISSING(role),MISSING(playerPawn),MISSING(player),
        MISSING(netConnection),MISSING(connectionTargetE64),MISSING(connectionState60),MISSING(connectionTargetVirtual5C),
        MISSING(getActorIndex),MISSING(undo),MISSING(saveActorArrayItem),MISSING(modify),MISSING(stateFrame),MISSING(stateCode),
        MISSING(isProbing),MISSING(findFunctionChecked),MISSING(processEvent),MISSING(pendingDelete),MISSING(base),MISSING(setBase),
        MISSING(standingCount),MISSING(actorCount),MISSING(actorSlot),MISSING(levelHash),MISSING(collideActors),MISSING(removeFromHash),
        MISSING(checkActorNotReferenced),MISSING(owner),MISSING(setOwner),MISSING(touching),MISSING(endTouch),MISSING(netDriver),
        MISSING(demoRecDriver),MISSING(serverConnection),MISSING(notifyActorDestroyed),MISSING(clearActorSlot),MISSING(setPendingDelete),
        MISSING(engine),MISSING(audio),MISSING(audioActorDestroyed),MISSING(conditionalDestroy),MISSING(firstDeleted),MISSING(writeDeleted),
        MISSING(writeFirstDeleted),MISSING(cleanupDestroyed)
#undef MISSING
    };
    std::array<Mock,3u> paths{Full(),Full(),Full()};paths[1u].connectionState=0;
    paths[2u].hook=[](Mock& state,const Record& record){if(record.op==Op::Conditional)state.editor=1;};
    for(const auto& [name,erase]:missing) {
        bool reached{};
        for(const auto& input:paths) {const auto result=Compare(input,0,erase);reached|=result.refused;}
        Require(reached,std::string("Required native provider has no positive reached-path refusal control: ")+name);
    }
    auto optional=Compare(Full(),0,[](Queries& q){q.work={};});
    Require(!optional.refused&&optional.state.workUsed==0u,"Detached controls require an invented private native-work ledger");
    Mock plain;Require(Compare(plain,0,{},0u,Source).refused&&Compare(plain,0,{},Level,0u).refused,"Null native input was accepted");
    plain.indexOverride=-1;Require(Compare(plain).refused,"Missing first native Actor slot was accepted");
    plain={};plain.countOverride=-1;Require(Compare(plain).refused,"Invalid native Actor count was treated as an empty valid array");
    plain={};plain.nullFunction=true;Require(Compare(plain).refused,"Null selected Function was accepted as a no-op event");
    plain={};plain.engine=0u;Require(Compare(plain).refused,"Missing actual native Engine was treated as absent Audio");
}
void FailurePrefixesAndCallerRollback() {
    const auto baseline=Compare(Full());Require(!baseline.refused,"Positive all-service mock path failed");
    Require(baseline.state.trace.size()==baseline.state.workUsed,"Reached native providers did not borrow exactly one shared work step");
    for(std::size_t failure=1u;failure<=baseline.state.trace.size();++failure) {
        auto input=Full();input.failProvider=failure;auto result=Compare(input);++providerFailures;
        Require(result.refused&&result.state.trace.size()==failure,"Injected reached-provider failure did not propagate to the caller");
        // EXPLICIT MOCK CALLER TRANSACTION: the coordinator deliberately owns
        // no rollback. This snapshot restoration is not a runtime/GC claim or
        // an allocation-free native journal implementation.
        const auto consumed=result.state.workUsed;const auto trace=result.state.trace;
        result.state=input;result.state.workUsed=consumed;result.state.trace=trace;
        Require(result.state.SameData(input)&&result.state.workUsed==consumed,"Mock caller rollback lost graph state or reset shared work");
    }
    for(std::size_t limit=0u;limit<=baseline.state.workUsed;++limit) {
        auto input=Full();input.workLimit=limit;auto result=Compare(input);++workFailures;
        Require(result.refused==(limit<baseline.state.workUsed)&&result.state.workUsed==limit,
            "Shared native-work N/N-1 denial did not preserve exact cumulative provider prefix");
        if(result.refused) {
            const auto consumed=result.state.workUsed;result.state=input;result.state.workUsed=consumed;
            Require(result.state.SameData(input)&&result.state.workUsed==consumed,"Mock work-denial rollback reset caller's spent ledger");
        }
    }
    {
        Mock input;input.slots={Source,A};input.actors.at(Source).standing=1u;input.actors.at(A).base=Source;input.workLimit=256u;
        input.hook=[](Mock& state,const Record& record){if(record.op==Op::SetBase){
            const auto id=static_cast<Pointer>(1000u+state.mutation++);state.actors.emplace(id,Actor{});state.actors.at(id).base=Source;state.slots.push_back(id);
        }};
        const auto result=Compare(input);++callbackCases;
        Require(result.refused&&result.state.workUsed==input.workLimit&&result.state.slots.size()>input.slots.size()&&Calls(result.state,Op::Destroyed)==0u,
            "Endlessly appended based actors got a fresh/private budget or returned a truncated successful scan");
    }
}
} // namespace
int main() {
    try {
        AdmissionMatrix();NetworkMatrix();FrameAndPendingGates();LiveScansAndRetainedIdentity();ConditionalAndTailMatrix();
        ReentrantCallbacks();MissingAndInvalidProviders();FailurePrefixesAndCallerRollback();
        std::cout<<"Original DestroyActor coordinator: "<<checks<<" checks, "<<cases<<" independent PC/provider cases, "
            <<refusals<<" explicit refusals, "<<callbackCases<<" callback/reentrance controls, "<<providerFailures
            <<" provider-failure prefixes and "<<workFailures<<" shared-work prefixes passed. ALL native services MOCK; "
            <<"no runtime binding, real-map startup, conditional UObject teardown, loot or gravity claim.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"Original DestroyActor protocol failed: "<<error.what()<<'\n';return 1;}
}
