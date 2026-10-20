#include "quest_conversation_bind.h"
#include "quest_native_work.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Queries=QuestVr::OriginalConversationBindQueries;
using Limits=QuestVr::OriginalConversationBindLimits;
using Stats=QuestVr::OriginalConversationBindStats;
std::size_t checks{},refusals{},cases{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void Refuse(F run){bool failed{};try{run();}catch(const std::exception&){failed=true;}
    Require(failed,"Unsupported conversation-binding control did not refuse");++refusals;}
struct Conversation {std::string name,owner;std::int32_t count{};bool operator==(const Conversation&)const=default;};
struct Node {Id con{},next{},outer{};std::uint32_t flags{};bool alive{true};bool operator==(const Node&)const=default;};
struct Source {Id con{},next{};bool operator==(const Source&)const=default;};
struct State {
    Id head{},sourceHead{},nextId{1000u};std::string bind,bark;
    std::map<Id,Conversation> conversations;
    std::map<Id,Node> nodes;
    std::map<Id,Source> source;
    bool operator==(const State&)const=default;
};
struct Fixture {
    static constexpr Id list=1u,actor=2u,outer=3u;
    State state;
    std::vector<std::string> log;
    std::size_t calls{},failAt{};
    std::function<void(std::string_view,Id)> effect;
    QuestVr::NativeWork::Budget work;
    void Event(std::string_view name,Id id=0u){
        log.push_back(std::string(name)+":"+std::to_string(id));
        ++calls;if(failAt&&calls==failAt)throw std::runtime_error("injected provider failure");
        if(effect)effect(name,id);
    }
    Node& Live(Id node){auto& value=state.nodes.at(node);Require(value.alive,"Provider read destroyed ConListItem");return value;}
    Queries Query(){
        Queries q;
        q.work=[this]{work.Step();};
        q.sourceHead=[this](Id id){Event("sourceHead",id);Require(id==list,"Wrong source list");return state.sourceHead;};
        q.sourceConversation=[this](Id id){Event("sourceCon",id);return state.source.at(id).con;};
        q.sourceNext=[this](Id id){Event("sourceNext",id);return state.source.at(id).next;};
        q.actorHead=[this](Id id){Event("head",id);Require(id==actor,"Wrong head receiver");return state.head;};
        q.nodeConversation=[this](Id id){Event("nodeCon",id);return Live(id).con;};
        q.nodeNext=[this](Id id){Event("nodeNext",id);return Live(id).next;};
        q.outer=[this](Id id){Event("outer",id);Require(id==list,"Outer queried on actor instead of list");return outer;};
        q.conversationName=[this](Id id){Event("name",id);return state.conversations.at(id).name;};
        q.ownerName=[this](Id id){Event("owner",id);return state.conversations.at(id).owner;};
        q.bindName=[this](Id id){Event("bind",id);Require(id==actor,"Wrong BindName receiver");return state.bind;};
        q.barkName=[this](Id id){Event("bark",id);Require(id==actor,"Wrong BarkBindName receiver");return state.bark;};
        q.ownerRefCount=[this](Id id){Event("count",id);return state.conversations.at(id).count;};
        q.writeOwnerRefCount=[this](Id id,std::int32_t count){Event("writeCount",id);state.conversations.at(id).count=count;};
        q.destroyNode=[this](Id id){Event("destroy",id);Live(id).alive=false;};
        q.createNode=[this](Id parent,std::uint32_t flags){Event("create",parent);
            Require(parent==outer&&flags==0u,"ConListItem requested wrong Outer or explicit flags");
            const auto id=state.nextId++;state.nodes.emplace(id,Node{0u,0u,parent,flags,true});return id;};
        q.flags=[this](Id id){Event("flags",id);return Live(id).flags;};
        q.writeFlags=[this](Id id,std::uint32_t flags){Event("writeFlags",id);Live(id).flags=flags;};
        q.writeNodeConversation=[this](Id id,Id con){Event("writeCon",id);Live(id).con=con;};
        q.writeNodeNext=[this](Id id,Id next){Event("writeNext",id);Live(id).next=next;};
        q.writeActorHead=[this](Id id,Id head){Event("writeHead",id);Require(id==actor,"Wrong head store receiver");state.head=head;};
        return q;
    }
    Stats Run(const Limits& limits={}){return QuestVr::BindOriginalConversations(list,actor,Query(),limits);}
    void RollbackRefusal(const Limits& limits={}){
        const auto before=state;
        Refuse([&]{try{Run(limits);}catch(...){state=before;throw;}});
        Require(state==before,"Enclosing transaction did not restore count/nodes/unique-name state");
    }
    std::vector<Id> Bound()const{
        std::vector<Id> result;for(auto at=state.head;at;at=state.nodes.at(at).next){
            Require(result.size()<10000u&&state.nodes.at(at).alive,"Invalid output binding chain");result.push_back(state.nodes.at(at).con);
        }return result;
    }
};
Fixture Basic(){
    Fixture f;f.state.bind="JaimeReyes";f.state.bark="Jaime";
    f.state.sourceHead=200u;f.state.source={{200u,{100u,201u}},{201u,{101u,0u}}};
    f.state.conversations={{100u,{"Conversation", "JAIMEREYES",0}},{101u,{"Jaime_BarkTest","jaime",0}},
        {102u,{"Old","OldOwner",1}}};
    f.state.head=900u;f.state.nodes.emplace(900u,Node{102u,0u,Fixture::outer,4u,true});return f;
}
std::string CString(std::string value){value.resize(value.find('\0')==std::string::npos?value.size():value.find('\0'));return value;}
bool Equal(std::string a,std::string b){
    a=CString(a);b=CString(b);
    for(auto* s:{&a,&b})for(auto& c:*s)if(c>='A'&&c<='Z')c=static_cast<char>(c+32);
    return a==b;
}
bool Accepted(const Conversation& con,const std::string& bind,const std::string& bark){
    const bool isBark=CString(con.name).find("_Bark")!=std::string::npos;
    // Independent truth table rather than production's lazy provider body.
    if(isBark)return Equal(con.owner,bark)||(CString(bark).empty()&&Equal(con.owner,bind));
    return Equal(con.owner,bind);
}
void BasicAndOrder(){
    auto f=Basic();const auto first=f.Run();
    Require(first.purged==1u&&first.visited==2u&&first.accepted==2u&&first.created==2u,
        "Binding lost purge, original one-pass matches or new-node creation");
    Require(f.Bound()==std::vector<Id>{101u,100u},"Binding output is not reverse accepted source order");
    Require(!f.state.nodes.at(900u).alive&&f.state.conversations.at(102u).count==0&&
        f.state.conversations.at(100u).count==1&&f.state.conversations.at(101u).count==1,
        "Purge/new binding did not balance original ownerRefCount fields");
    const std::vector<std::string> prefix{"sourceHead:1","head:2","nodeCon:900","count:102","writeCount:102",
        "nodeNext:900","destroy:900","writeHead:2","sourceCon:200","name:100","owner:100","bark:2",
        "owner:100","bind:2","bark:2","bark:2","count:100","writeCount:100","outer:1","create:3",
        "flags:1000","writeCon:1000","writeFlags:1000","head:2","writeNext:1000","writeHead:2","sourceNext:200"};
    Require(f.log.size()>prefix.size()&&std::equal(prefix.begin(),prefix.end(),f.log.begin()),
        "Original captured-head/purge/lazy reads/counter-before-allocation/flag store order differs");
    const auto oldHead=f.state.head;f.log.clear();f.calls=0u;const auto second=f.Run();
    Require(second.purged==2u&&f.state.head!=oldHead&&f.state.nextId==1004u&&f.Bound()==std::vector<Id>{101u,100u},
        "Rebind reused nodes, reset class-name sequence or changed list direction");
    Require(f.state.conversations.at(100u).count==1&&f.state.conversations.at(101u).count==1,
        "Repeated binding accumulated owner references");
    for(const auto& [id,node]:f.state.nodes)if(node.alive)
        Require(id>=1002u&&node.flags==4u&&node.outer==Fixture::outer,"Live binding node has wrong class creation envelope");
    f.state.sourceHead=0u;const auto empty=f.Run();
    Require(empty.purged==2u&&!empty.visited&&!f.state.head&&f.state.conversations.at(100u).count==0,
        "Empty mission list failed to purge old nodes");
    ++cases;
}
void NamesAndCounts(){
    const std::array<std::string,7u> names{"Plain","_Bark","prefix_BarkSuffix","_bark","Bark","PLAIN_BARK",std::string("x\0_Bark",7u)};
    const std::array<std::string,6u> owners{"","A","a","B",std::string("A\0junk",6u),"\xc3\x84"};
    for(const auto& name:names)for(const auto& owner:owners)for(const auto& bind:owners)for(const auto& bark:owners){
        Fixture f;f.state.bind=bind;f.state.bark=bark;f.state.sourceHead=200u;
        f.state.source.emplace(200u,Source{100u,0u});f.state.conversations.emplace(100u,Conversation{name,owner,0});
        const bool expected=Accepted(f.state.conversations.at(100u),bind,bark);
        const auto result=f.Run();Require(result.accepted==unsigned(expected)&&result.created==unsigned(expected)&&
            f.state.conversations.at(100u).count==int(expected),"Original bark/empty/CI/C-string truth table differs");++cases;
    }
    Fixture duplicate;duplicate.state.bind="A";duplicate.state.sourceHead=200u;
    duplicate.state.conversations.emplace(100u,Conversation{"A_Bark","A",std::numeric_limits<std::int32_t>::max()});
    duplicate.state.source={{200u,{100u,201u}},{201u,{100u,0u}}};const auto r=duplicate.Run();
    Require(r.created==2u&&duplicate.Bound()==std::vector<Id>{100u,100u}&&
        duplicate.state.conversations.at(100u).count==std::numeric_limits<std::int32_t>::min()+1,
        "Duplicate source entries were deduplicated or reference INC failed DWORD wrap");
    duplicate.state.sourceHead=0u;duplicate.Run();
    Require(duplicate.state.conversations.at(100u).count==std::numeric_limits<std::int32_t>::max(),
        "Purge reference DEC failed DWORD wrap/balance");
    auto counted=Basic();counted.state.conversations.at(102u).count=std::numeric_limits<std::int32_t>::min();counted.Run();
    Require(counted.state.conversations.at(102u).count==std::numeric_limits<std::int32_t>::max(),"Purge clamped inconsistent negative count");
    auto accented=Basic();accented.state.source.at(200u).next=0u;accented.state.bind="\xc3\xa4";
    accented.state.conversations.at(100u).owner="\xc3\x84";
    Require(!accented.Run().accepted,"Original C-locale comparison gained Unicode case folding");
}
void CapturedAndLive(){
    auto f=Basic();f.effect=[&](std::string_view event,Id id){
        if(event=="destroy")f.state.sourceHead=0u; // Cached source must still run.
        if(event=="writeHead"&&id==Fixture::actor&&f.state.nextId==1001u)f.state.source.at(200u).next=0u;
        if(event=="writeCon")f.state.nodes.at(id).flags=0x4000u; // Flags load precedes this store.
    };
    const auto r=f.Run();Require(r.visited==1u&&r.created==1u&&f.Bound()==std::vector<Id>{100u}&&
        f.state.nodes.at(f.state.head).flags==4u,"Captured source/live Next or captured flag ordering changed");
    auto count=Basic();count.effect=[&](std::string_view event,Id){if(event=="create")
        Require(count.state.conversations.at(100u).count==1,"Reference increment did not precede allocator");};
    // Restrict to one source; original bark match short-circuits BindName.
    count.state.source.at(200u).next=0u;count.Run();
    auto bark=Basic();bark.state.sourceHead=201u;bark.Run();
    Require(std::none_of(bark.log.begin(),bark.log.end(),[](const auto& s){return s=="bind:2";}),
        "Accepted bark eagerly read ordinary BindName");
    auto fresh=Basic();fresh.state.source.at(200u).next=0u;std::size_t barkReads{};
    fresh.effect=[&](std::string_view event,Id){if(event=="bark"&&++barkReads==3u)fresh.state.bark.clear();};
    Require(!fresh.Run().accepted&&barkReads==3u,"Original repeated bark-empty comparison was cached away");
}
void FailureAndWork(){
    auto complete=Basic();const auto baseline=complete.Run();const auto calls=complete.calls;
    Require(calls==baseline.steps&&complete.work.Read().used==baseline.steps,"Protocol did not share every provider work step");
    for(std::size_t cut=1u;cut<=calls;++cut){auto f=Basic();f.failAt=cut;f.RollbackRefusal();}
    auto noUndo=Basic();noUndo.effect=[&](std::string_view event,Id){if(event=="create")throw std::runtime_error("allocator refused");};
    Refuse([&]{noUndo.Run();});
    Require(!noUndo.state.nodes.at(900u).alive&&!noUndo.state.head&&noUndo.state.conversations.at(100u).count==1,
        "Detached helper silently rolled back only part of its caller's native effects");
    auto exact=Basic();exact.work.Reset(baseline.steps);const auto enough=exact.Run();
    Require(enough.steps==baseline.steps&&!exact.work.Read().exhausted,"Exact shared native work did not complete");
    auto shortWork=Basic();shortWork.work.Reset(baseline.steps-1u);shortWork.RollbackRefusal();
    Require(shortWork.work.Read().exhausted,"Native work refusal did not poison borrowed root allowance");
    auto limited=Basic();Limits limits;limits.steps=baseline.steps-1u;limited.RollbackRefusal(limits);
    for(auto field:{&Limits::steps,&Limits::links,&Limits::textUnits})for(bool expand:{false,true}){
        auto f=Basic();Limits bad;if(expand)++(bad.*field);else bad.*field=0u;f.RollbackRefusal(bad);
    }
    auto cycle=Basic();cycle.state.source.at(201u).next=200u;Limits small;small.links=3u;cycle.RollbackRefusal(small);
    auto null=Basic();null.state.source.at(200u).con=0u;null.RollbackRefusal();
    auto badText=Basic();badText.state.bind="\xff";badText.RollbackRefusal();
    auto large=Basic();large.state.bind.assign(8193u,'a');large.RollbackRefusal();
    auto absent=Basic();auto q=absent.Query();q.createNode={};Refuse([&]{QuestVr::BindOriginalConversations(1u,2u,q);});
    Refuse([&]{QuestVr::BindOriginalConversations(0u,2u,Queries{});});
    Refuse([&]{QuestVr::BindOriginalConversations(1u,0u,Queries{});});
}
void Wrapper(){
    using Result=QuestVr::OriginalConversationMissionResult;using Disposition=Result::Disposition;
    QuestVr::OriginalConversationMissionQueries q;
    std::vector<Id> slots{0u,20u,30u,40u};std::int32_t mission{};std::string package="DeusExConversations",loaded;
    std::vector<std::string> log;std::size_t logs{},binds{};Id loadedList=100u;
    q.xlevel=[&](Id id){Require(id==2u,"Wrapper used reflected Level receiver");log.push_back("level");return Id{10u};};
    q.actorCount=[&](Id id){Require(id==10u,"Wrapper count lost native XLevel");log.push_back("count");return slots.size();};
    q.actorAt=[&](Id,std::size_t i){log.push_back("slot"+std::to_string(i));return slots.at(i);};
    q.isDeusExLevelInfo=[&](Id id){log.push_back("class"+std::to_string(id));return id>=30u;};
    q.missingLevelInfoLog=[&]{++logs;};q.missionNumber=[&](Id id){Require(id==30u,"Wrapper did not choose FIRST actual class match");return mission;};
    q.conversationPackage=[&](Id id){Require(id==30u,"Wrapper package is not selected info");return package;};
    q.loadList=[&](const std::string& path,std::uint32_t flags){Require(flags==2u,"Wrong StaticLoadObject flags");loaded=path;return loadedList;};
    q.bind=[&](Id list,Id actor){Require(list==100u&&actor==2u,"Wrapper changed binding receiver");++binds;};
    auto r=QuestVr::BindOriginalActorConversationMission(2u,q);
    Require(r.disposition==Disposition::Bound&&r.info==30u&&r.list==100u&&r.slots==3u&&binds==1u&&
        loaded=="DeusExConText.ConList_Mission00"&&!logs,"Wrapper used wrong mission selection/alias/path");
    for(const auto& [input,expected]:std::vector<std::pair<std::string,std::string>>{
        {"dEuSeXcOnVeRsAtIoNs","DeusExConText.ConList_Mission00"},{"Mod","ModText.ConList_Mission00"},
        {"","Text.ConList_Mission00"},{std::string("Custom\0ignored",14u),"CustomText.ConList_Mission00"}}){
        package=input;QuestVr::BindOriginalActorConversationMission(2u,q);Require(loaded==expected,"Wrapper alias/empty/C-string package formatting differs");
    }
    package="DeusExConversations";
    for(const auto& [number,suffix]:std::vector<std::pair<std::int32_t,std::string>>{{9,"09"},{10,"10"},{100,"100"},{2147483647,"2147483647"}}){
        mission=number;QuestVr::BindOriginalActorConversationMission(2u,q);
        Require(loaded=="DeusExConText.ConList_Mission"+suffix,"Mission formatting imposed a two-digit ceiling");
    }
    const auto before=binds;mission=-1;auto guarded=q;guarded.conversationPackage={};guarded.loadList={};guarded.bind={};
    r=QuestVr::BindOriginalActorConversationMission(2u,guarded);
    Require(r.disposition==Disposition::NegativeMission&&r.path.empty()&&binds==before,"Negative mission touched existing bindings or package loader");
    mission=0;loadedList=0u;guarded=q;guarded.bind={};r=QuestVr::BindOriginalActorConversationMission(2u,guarded);
    Require(r.disposition==Disposition::MissingList&&binds==before,"Missing list purged prior bindings");
    slots={0u,20u};guarded=q;guarded.missionNumber={};guarded.loadList={};r=QuestVr::BindOriginalActorConversationMission(2u,guarded);
    Require(r.disposition==Disposition::MissingLevelInfo&&logs==1u&&r.slots==2u,"Missing mission info was guessed or not logged");
    slots={0u,20u,30u};package.assign(110u,'a');Refuse([&]{QuestVr::BindOriginalActorConversationMission(2u,q);});
    package="Mod";Limits limits;limits.links=2u;Refuse([&]{QuestVr::BindOriginalActorConversationMission(2u,q,limits);});
    guarded=q;guarded.xlevel=[](Id){return Id{};};Refuse([&]{QuestVr::BindOriginalActorConversationMission(2u,guarded);});
    // The wrapper rereads native count/array; no captured-count shortcut.
    slots={20u};guarded=q;guarded.actorAt=[&](Id,std::size_t i){if(!i)slots.push_back(30u);return slots.at(i);};
    loadedList=100u;r=QuestVr::BindOriginalActorConversationMission(2u,guarded);
    Require(r.disposition==Disposition::Bound&&r.slots==2u,"Wrapper ignored live native actor-array growth");
    QuestVr::NativeWork::Budget work;guarded.work=[&]{work.Step();};r=QuestVr::BindOriginalActorConversationMission(2u,guarded);
    Require(work.Read().used==r.steps,"Wrapper did not charge shared root work");
    work.Reset(r.steps-1u);Refuse([&]{QuestVr::BindOriginalActorConversationMission(2u,guarded);});
    Require(work.Read().exhausted,"Wrapper gave a child query fresh work allowance");
}
void Composition(){
    const auto run=[](Fixture& fixture) {
        QuestVr::OriginalConversationMissionQueries q;
        q.work=[&]{fixture.work.Step();};
        q.xlevel=[](Id){return Id{10u};};q.actorCount=[](Id){return std::size_t{1u};};
        q.actorAt=[](Id,std::size_t){return Id{30u};};q.isDeusExLevelInfo=[](Id){return true;};
        q.missionNumber=[](Id){return 0;};q.conversationPackage=[](Id){return std::string("DeusExConversations");};
        q.loadList=[](const std::string& path,std::uint32_t flags){
            Require(path=="DeusExConText.ConList_Mission00"&&flags==2u,"Composed wrapper changed list authority");return Fixture::list;
        };
        Stats nested;
        q.bind=[&](Id list,Id actor){Require(list==Fixture::list&&actor==Fixture::actor,"Composed binding changed identities");nested=fixture.Run();};
        const auto result=QuestVr::BindOriginalActorConversationMission(Fixture::actor,q);
        Require(result.disposition==QuestVr::OriginalConversationMissionResult::Disposition::Bound&&
            fixture.work.Read().used==result.steps+nested.steps,"Wrapper and binding did not borrow one work ledger");
        return fixture.work.Read().used;
    };
    auto baseline=Basic();const auto total=run(baseline);
    auto exact=Basic();exact.work.Reset(total);Require(run(exact)==total,"Exact composed work changed root result");
    auto shortWork=Basic();const auto original=shortWork.state;shortWork.work.Reset(total-1u);
    Refuse([&]{try{run(shortWork);}catch(...){shortWork.state=original;throw;}});
    Require(shortWork.work.Read().exhausted&&shortWork.state==original,"Composed N-1 refusal lost complete enclosing rollback");
}
void SharedConversationOwners(){
    auto fixture=Basic();auto q=fixture.Query();std::map<Id,Id> heads{{2u,900u},{4u,0u}};
    q.actorHead=[&](Id actor){fixture.Event("sharedHead",actor);return heads.at(actor);};
    q.writeActorHead=[&](Id actor,Id head){fixture.Event("sharedWriteHead",actor);heads.at(actor)=head;};
    q.bindName=[&](Id){fixture.Event("sharedBind");return fixture.state.bind;};
    q.barkName=[&](Id){fixture.Event("sharedBark");return fixture.state.bark;};
    QuestVr::BindOriginalConversations(1u,2u,q);QuestVr::BindOriginalConversations(1u,4u,q);
    Require(fixture.state.conversations.at(100u).count==2&&fixture.state.conversations.at(101u).count==2&&
        heads.at(2u)!=heads.at(4u),"Two Actors did not share conversation counters with separate fresh chains");
    const auto other=heads.at(4u);fixture.state.sourceHead=0u;
    QuestVr::BindOriginalConversations(1u,2u,q);
    Require(!heads.at(2u)&&heads.at(4u)==other&&fixture.state.conversations.at(100u).count==1&&
        fixture.state.nodes.at(other).alive,"One Actor purge damaged another owner's nodes/counts");
    QuestVr::BindOriginalConversations(1u,4u,q);
    Require(!heads.at(4u)&&fixture.state.conversations.at(100u).count==0&&fixture.state.conversations.at(101u).count==0,
        "Final owner purge did not balance shared references");
}
}
int main(){try{BasicAndOrder();NamesAndCounts();CapturedAndLive();FailureAndWork();Wrapper();Composition();SharedConversationOwners();
    std::cout<<"PASS original conversation binding protocol "<<checks<<" checks / "<<cases<<" matching cases / "<<refusals
        <<" refusals; mock allocation/deletion/load providers, shared work and caller rollback controls. NOT actual UObject construction, native2101 startup or gameplay.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL original conversation binding protocol: "<<error.what()<<'\n';return 1;}}
