#include "quest_input_reconcile.h"
#include <iostream>
#include <tuple>
#include <vector>

namespace {
using namespace QuestVr;
std::size_t checks{},refusals{};
void Require(bool okay,const char* message){++checks;if(!okay)throw std::runtime_error(message);}
template<class Callback> void Refuse(Callback call){bool caught{};try{call();}catch(const std::exception&){caught=true;}
    Require(caught,"Expected reconciliation refusal");++refusals;}
using Event=std::tuple<std::int32_t,std::int32_t,float>;
const QuestInputMappingKeys Mapping{1,2,73,224,225}; // Explicit test key profile, not a metadata provider.
struct Fixture {
    std::shared_ptr<NativeClientLifetime> lifetime=std::make_shared<NativeClientLifetime>();
    QuestInputSource source{lifetime,1u,2u};QuestInputPhysicalState physical;QuestInputCursor cursor;
    QuestInputObservation row;std::vector<Event> events;std::array<std::uint8_t,255u> gameKeys{};
    std::size_t work{},workLimit{1000000u},scratch{},peak{},dispatches{},failAt{};bool consumed{},revoked{};
    Fixture(){row.focused=row.mounted=row.headValid=row.headTracked=true;row.hands[0].tracked=row.hands[1].tracked=true;}
    void Publish(){row.displayTime+=1.0;row.deltaSeconds=0.01f;source.Publish(row);}
    QuestInputReconcileQueries Queries(){QuestInputReconcileQueries q;
        q.validate=[&]{if(revoked||!source.Matches(lifetime,1u,2u))throw std::runtime_error("fixture lease revoked");};
        q.work=[&]{if(++work>workLimit)throw std::runtime_error("fixture shared work exhausted");};
        q.enterFrame=[&](std::size_t bytes){scratch+=bytes;peak=std::max(peak,scratch);};
        q.leaveFrame=[&](std::size_t bytes)noexcept{if(bytes>scratch)std::terminate();scratch-=bytes;};
        q.process=[&](std::int32_t key,std::int32_t action,float delta){
            events.emplace_back(key,action,delta);++dispatches;if(action==1)gameKeys[static_cast<std::size_t>(key)]=1u;
            if(action==3)gameKeys[static_cast<std::size_t>(key)]=0u;
            if(consumed)gameKeys.fill(0u); // Explicit model of Root consuming gameplay-held keys.
            if(failAt&&dispatches==failAt)throw std::runtime_error("fixture callback failed after prefix");
            return consumed;
        };return q;}
    QuestInputReconcileResult Poll(bool reset=false,const QuestInputMappingKeys& map=Mapping){
        const auto result=ReconcileQuestInput(source.Stage(cursor,reset),physical,cursor,map,row.deltaSeconds,Queries());
        Require(!scratch&&peak>=QuestInputReconcileDetail::Frame::Bytes,"Shared scratch did not unwind");return result;}
    void Clear(){events.clear();dispatches=0u;}
};
void Basic(){Fixture f;Refuse([&]{f.Poll(true);});Require(!f.cursor.source&&!f.physical.version,"Empty reset invented a baseline");
    f.Publish();const auto initial=f.Poll(true);Require(initial.reset&&initial.rows==1u&&!initial.pressed&&!initial.released&&initial.axes==2u,
        "Neutral initial reset differs");Require(f.cursor.sequence==1u&&f.physical.version==1u,"Valid reset not staged");
    f.Clear();const auto empty=f.Poll();Require(empty.empty&&!empty.rows&&empty.axes==2u&&f.events.size()==2u&&
        f.events[0]==Event{224,4,0.0f}&&f.events[1]==Event{225,4,0.0f}&&f.cursor.sequence==1u,"Empty poll invented transitions/history");
    f.Clear();f.row.hands[1].trigger=0.8f;f.Publish();auto result=f.Poll();Require(result.pressed==1u&&!result.released&&
        f.events[0]==Event{1,1,0.0f}&&f.physical.held[1],"Trigger not mapped to original Press");
    f.consumed=true;f.gameKeys.fill(0u);f.Clear();f.Publish();result=f.Poll();Require(!result.pressed&&!result.released&&
        f.events.size()==2u&&f.physical.held[1],"Root consumption manufactured a duplicate physical Press");
    f.Clear();f.row.hands[1].trigger=0.0f;f.Publish();result=f.Poll();Require(result.released==1u&&f.events[0]==Event{1,3,0.0f},
        "Physical release lost after Root consumption");
    f.row.hands[1].trigger=.75f;f.Publish();Require(!f.Poll().pressed,"Threshold changed from accepted strict greater-than");
    f.row.hands[1].trigger=.751f;f.Publish();Require(f.Poll().pressed==1u,"Strict trigger threshold did not engage");
    f.Clear();result=f.Poll(true);Require(result.released==1u&&result.pressed==1u&&f.events[0]==Event{1,3,0.0f}&&f.events[1]==Event{1,1,0.0f},
        "Reset of SAME physical sample did not release and repress held state");
    f.row.buttons=1u|16u;f.Publish();result=f.Poll();Require(result.pressed==2u&&f.physical.held[2]&&f.physical.held[73],"A/Menu mapping differs");
    f.Clear();f.row.hands[1].tracked=false;f.Publish();result=f.Poll();Require(result.released==2u&&f.physical.held[73]&&!f.physical.held[1]&&!f.physical.held[2],
        "Right tracking loss did not release just the right-hand controls");
    f.row.hands[0].tracked=false;f.Publish();Require(f.Poll().released==1u,"Left tracking loss retained Menu");
}
void Gates(){for(int which=0;which<5;++which){Fixture f;f.row.hands[1].trigger=1;f.row.buttons=17u;f.Publish();f.Poll(true);
    if(which==0)f.row.focused=false;
    if(which==1)f.row.mounted=false;
    if(which==2)f.row.headTracked=false;
    if(which==3)f.row.headValid=f.row.headTracked=false;
    if(which==4)f.row.hands[0].tracked=f.row.hands[1].tracked=false;
    f.Clear();f.Publish();const auto result=f.Poll();Require(result.released==3u&&!result.pressed&&
        std::get<2>(f.events[f.events.size()-2u])==0.0f&&std::get<2>(f.events.back())==0.0f,"Focus/mount/head/hand gate retained actions");}
    Fixture reserved;reserved.row.buttons=2u|4u|8u|32u|64u;reserved.row.hands[0].grip=reserved.row.hands[1].grip=1;
    reserved.row.hands[1].stickX=reserved.row.hands[1].stickY=1;reserved.Publish();Require(!reserved.Poll(true).pressed,
        "Reserved VR controls were duplicated as original gameplay input");
}
void AxesAndHistory(){Fixture f;f.Publish();f.Poll(true);f.Clear();
    f.row.hands[0].stickX=1;f.row.hands[1].trigger=1;f.Publish();
    f.row.hands[0].stickX=-1;f.row.hands[1].trigger=0;f.Publish();
    f.row.hands[0].stickX=0;f.row.hands[0].stickY=1;f.Publish();const auto result=f.Poll();
    Require(result.rows==3u&&result.pressed==1u&&result.released==1u&&result.axes==2u&&f.events.size()==4u,
        "Button history or latest-only axes were lost/replayed");
    Require(f.events[0]==Event{1,1,0.0f}&&f.events[1]==Event{1,3,0.0f}&&f.events[2]==Event{224,4,0.0f}&&f.events[3]==Event{225,4,150.0f},
        "Newest axis units/direction/order differs");
    f.Clear();f.row.hands[0].stickX=f.row.hands[0].stickY=1;f.Publish();f.Poll();
    const auto x=std::get<2>(f.events[0]),y=std::get<2>(f.events[1]);Require(x>106&&x<107&&x==y&&std::hypot(x,y)<150.001f,
        "Radial deadzone/clamped diagonal changed");
    f.row.hands[0].stickX=.1f;f.row.hands[0].stickY=0;f.Publish();f.Clear();f.Poll();Require(f.events[0]==Event{224,4,0.0f},"Deadzone invented motion");
}
void MappingMatrix(){for(unsigned flags=0u;flags<16u;++flags)for(unsigned hands=0u;hands<4u;++hands)
    for(unsigned buttons=0u;buttons<128u;++buttons)for(float trigger:{0.0f,0.75f,0.8f}){
        QuestInputObservation row;row.focused=(flags&1u)!=0u;row.mounted=(flags&2u)!=0u;
        row.headValid=(flags&4u)!=0u;row.headTracked=(flags&8u)!=0u;
        if(row.headTracked&&!row.headValid)continue;
        row.hands[0].tracked=(hands&1u)!=0u;row.hands[1].tracked=(hands&2u)!=0u;
        row.buttons=static_cast<std::uint8_t>(buttons);row.hands[1].trigger=trigger;row.hands[0].stickX=1;
        const auto mapped=QuestInputReconcileDetail::Map(row,Mapping,0.01f);
        const bool active=row.focused&&row.mounted&&row.headValid&&row.headTracked;
        Require(mapped.held[1]==(active&&row.hands[1].tracked&&trigger>.75f),"Matrix Fire gate differs");
        Require(mapped.held[2]==(active&&row.hands[1].tracked&&(buttons&1u)),"Matrix A gate differs");
        Require(mapped.held[73]==(active&&row.hands[0].tracked&&(buttons&16u)),"Matrix Menu gate differs");
        Require(mapped.strafe==(active&&row.hands[0].tracked?150.0f:0.0f)&&mapped.forward==0.0f,"Matrix axes gates differ");
    }
}
void PollTiming(){QuestInputObservation row;row.focused=row.mounted=row.headValid=row.headTracked=true;
    row.hands[0].tracked=true;row.hands[0].stickY=1.0f;row.deltaSeconds=1.0f/72.0f;
    for(float elapsed:{0.01f,0.02f,1.0f/72.0f,1.0f/90.0f,1.0f/120.0f}){
        const auto mapped=QuestInputReconcileDetail::Map(row,Mapping,elapsed);
        const double scaled=double(mapped.forward)*0.02*20.0/double(elapsed);
        Require(std::fabs(scaled-6000.0)<.01,"Port axis/read-input magnitude depends on polling frequency");}
    Fixture f;f.Publish();f.Poll(true);f.row.hands[0].stickY=1;f.Publish();f.Clear();
    ReconcileQuestInput(f.source.Stage(f.cursor),f.physical,f.cursor,Mapping,.025f,f.Queries());
    Require(std::get<2>(f.events.back())==375.0f,"Regular poll used the render row period instead of actual CPU elapsed");
    const auto held=f.physical;const auto heldCursor=f.cursor;
    for(float elapsed:{.005f,.02f,.025f}){f.Clear();const auto empty=ReconcileQuestInput(f.source.Stage(f.cursor),f.physical,f.cursor,Mapping,elapsed,f.Queries());
        Require(empty.empty&&!empty.rows&&empty.axes==2u&&!empty.pressed&&!empty.released&&f.events.size()==2u&&
            std::fabs(double(std::get<2>(f.events.back()))*.4/double(elapsed)-6000.0)<.01&&f.physical==held&&
            QuestInputReconcileDetail::Same(f.cursor,heldCursor),"Faster CPU poll lost held analog level or invented history");}
    for(float elapsed:{-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::max()}){
        const auto saved=f.physical;const auto cursor=f.cursor;
        Refuse([&]{ReconcileQuestInput(f.source.Stage(f.cursor,true),f.physical,f.cursor,Mapping,elapsed,f.Queries());});
        Require(f.physical==saved&&QuestInputReconcileDetail::Same(f.cursor,cursor)&&!f.scratch,"Invalid elapsed advanced baseline");}
    f.row.hands[0].tracked=false;f.Publish();f.Poll();f.Clear();f.Poll();
    Require(!f.physical.strafeLevel&&!f.physical.forwardLevel&&f.events.back()==Event{225,4,0.0f},"Loss row did not replace retained analog level");
}
void Faults(){Fixture f;f.row.hands[1].trigger=1;f.Publish();f.Poll(true);const auto before=f.physical;const auto cursor=f.cursor;
    f.Clear();f.failAt=1u;Refuse([&]{f.Poll(true);});Require(f.physical==before&&QuestInputReconcileDetail::Same(f.cursor,cursor)&&!f.scratch&&f.events.size()==1u,
        "Failure advanced physical state/cursor or repaired/erased the actual callback prefix");f.failAt=0;f.Clear();
    auto bad=Mapping;bad.frob=bad.fire;Refuse([&]{f.Poll(true,bad);});bad=Mapping;bad.fire=255;Refuse([&]{f.Poll(true,bad);});
    bad=Mapping;bad.fire=3;Refuse([&]{f.Poll(false,bad);});const auto remapped=f.Poll(true,bad);Require(remapped.released==1u&&remapped.pressed==1u&&
        f.physical.held[3]&&!f.physical.held[1],"Explicit mapping reset did not reconcile old/new slots");
    auto batch=f.source.Stage(f.cursor,true);batch.before.sequence=0;Refuse([&]{ReconcileQuestInput(batch,f.physical,f.cursor,bad,.01f,f.Queries());});
    batch=f.source.Stage(f.cursor,true);batch.rows[0].sequence=0;Refuse([&]{ReconcileQuestInput(batch,f.physical,f.cursor,bad,.01f,f.Queries());});
    batch=f.source.Stage(f.cursor,true);batch.after.sequence+=1;Refuse([&]{ReconcileQuestInput(batch,f.physical,f.cursor,bad,.01f,f.Queries());});
    batch=f.source.Stage(f.cursor,true);batch.rows[0].observation.hands[0].stickX=std::numeric_limits<float>::quiet_NaN();
    Refuse([&]{ReconcileQuestInput(batch,f.physical,f.cursor,bad,.01f,f.Queries());});
    Fixture overrun;overrun.Publish();overrun.Poll(true);for(std::size_t i=0;i<QuestInputSource::Capacity+1u;++i)overrun.Publish();
    Refuse([&]{overrun.Poll();});Require(overrun.Poll(true).rows==1u,"Overflow reset pretended to replay overwritten rows");
    auto invalid=overrun.row;invalid.deltaSeconds=-1;Refuse([&]{overrun.source.Publish(invalid);});Refuse([&]{overrun.Poll(true);});
    overrun.Publish();Require(overrun.Poll(true).rows==1u,"New valid post-rejection baseline refused");
    Fixture budget;budget.Publish();budget.workLimit=10u;Refuse([&]{budget.Poll(true);});Require(!budget.scratch&&!budget.cursor.source&&!budget.physical.version,
        "Shared work cutoff published physical state");
    Fixture lease;lease.Publish();lease.revoked=true;Refuse([&]{lease.Poll(true);});Require(!lease.cursor.source&&!lease.scratch,"Revoked lease advanced source");
    Fixture dead;dead.Publish();dead.lifetime->Revoke();Refuse([&]{dead.Poll(true);});
    for(std::size_t cut=1u;cut<=4u;++cut){Fixture late;late.row.hands[1].trigger=1;late.Publish();late.Poll(true);
        const auto saved=late.physical;const auto savedCursor=late.cursor;late.Clear();late.failAt=cut;
        Refuse([&]{late.Poll(true);});Require(late.physical==saved&&QuestInputReconcileDetail::Same(late.cursor,savedCursor)&&!late.scratch,
            "Late callback failure advanced baseline or retained scratch");}
    Fixture mid;mid.Publish();auto staged=mid.source.Stage(mid.cursor,true);auto queries=mid.Queries();const auto process=queries.process;
    queries.process=[&](auto key,auto action,float delta){const auto handled=process(key,action,delta);mid.revoked=true;return handled;};
    Refuse([&]{ReconcileQuestInput(staged,mid.physical,mid.cursor,Mapping,.01f,queries);});
    Require(!mid.physical.version&&!mid.cursor.source&&!mid.scratch,"Mid-dispatch lease revocation published a cursor");
    Fixture corrupt;corrupt.Publish();corrupt.Poll(true);corrupt.physical.held[99]=1u;
    Refuse([&]{corrupt.Poll(true);});corrupt.physical.held[99]=0u;corrupt.physical.version=99u;
    Refuse([&]{corrupt.Poll(true);});
    Fixture mismatch;mismatch.Publish();mismatch.Poll(true);mismatch.cursor={};Refuse([&]{mismatch.Poll(true);});
    Fixture empty;empty.Publish();empty.Poll(true);const auto saved=empty.physical;const auto savedCursor=empty.cursor;
    auto emptyQueries=empty.Queries();const auto validate=emptyQueries.validate;std::size_t validations{};
    emptyQueries.validate=[&]{validate();if(++validations==6u)throw std::runtime_error("fixture revoked at empty completion");};
    Refuse([&]{ReconcileQuestInput(empty.source.Stage(empty.cursor),empty.physical,empty.cursor,Mapping,.01f,emptyQueries);});
    Require(validations==6u&&empty.physical==saved&&QuestInputReconcileDetail::Same(empty.cursor,savedCursor)&&!empty.scratch,
        "Empty poll skipped final lease validation or mutated state");
    Fixture analog;analog.row.hands[0].stickX=1;analog.Publish();analog.Poll(true);analog.physical.strafeLevel=std::numeric_limits<float>::quiet_NaN();
    Refuse([&]{analog.Poll();});analog.physical.strafeLevel=2.0f;Refuse([&]{analog.Poll();});
}
}
int main(){try{Basic();Gates();AxesAndHistory();MappingMatrix();PollTiming();Faults();std::cout<<"PASS Quest physical reconciliation checks="<<checks<<" refusals="<<refusals
    <<"; port mapping/transitions/reset/consumption/tracking/history/budgets. NOT actual Input ownership, initialized startup or hardware acceptance\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL Quest physical reconciliation: "<<error.what()<<'\n';return 1;}}
