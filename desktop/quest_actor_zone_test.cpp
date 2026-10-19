#include "quest_actor_zone.h"

#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using Id=std::uintptr_t;using Region=QuestVr::OriginalActorRegion;using Slot=QuestVr::OriginalActorRegionSlot;
using Event=QuestVr::OriginalActorZoneEvent;using Queries=QuestVr::OriginalActorZoneQueries;
constexpr Id Level=100,Source=1,Info=2,Nested=12;
std::size_t checks{},refusals{},cases{},prefixes{},mutations{},numeric{},reentrant{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
bool Same(Region a,Region b){return a.zone==b.zone&&a.leaf==b.leaf&&a.number==b.number;}
enum class Op{Deleted,Pawn,Slot0,Class,Count,Location,Height,Eyes,Point,OldZone,Zone,Leaf,Number,Event,Mode,Pri,PlayerZone};
struct Trace{Op op;Id actor{};unsigned selector{};std::uint64_t a{},b{},c{};bool operator==(const Trace&)const=default;};
struct Actor{
    bool deleted{},pawn{},info{};std::uint8_t mode{};
    std::array<float,3u> location{3,-0.0f,30};float height{4},eyes{7};
    std::array<Region,3u> regions{{{6,90,10},{7,91,11},{8,92,12}}};Id pri{10},playerZone{};
};
struct World{
    std::array<Actor,16u> actors{};std::vector<Trace> trace;Id slot0{Info};std::int32_t count{16};
    std::array<unsigned,16u> queries{};std::size_t calls{},work{},failure{},limit{10000u};
    unsigned mutation{};bool nested{},inside{},oracle{},pointUpdatesOld{},writeUpdatesEyes{};
    World(){actors[Source].pawn=true;actors[Info].info=true;actors[Nested].pawn=false;}
    void Record(Trace row){if(++calls==failure)throw std::runtime_error("provider refusal");trace.push_back(row);}
    void Charge(){if(++work>limit)throw std::runtime_error("shared caller work refused");}
    Actor& A(Id id){if(!id||id>=actors.size())throw std::runtime_error("bad actor identity");return actors[id];}
    bool Deleted(Id id){Record({Op::Deleted,id});return A(id).deleted;}
    bool Pawn(Id id){Record({Op::Pawn,id});return A(id).pawn;}
    Id First(Id level){Record({Op::Slot0,level});if(level!=Level)throw std::runtime_error("bad level");return slot0;}
    void Class(Id id){Record({Op::Class,id});if(!A(id).info)throw std::runtime_error("not LevelInfo");}
    std::int32_t Count(Id level){Record({Op::Count,level});if(level!=Level)throw std::runtime_error("bad level");return count;}
    float Location(Id id,unsigned channel){Record({Op::Location,id,channel});return A(id).location.at(channel);}
    float Height(Id id){Record({Op::Height,id});return A(id).height;}
    float Eyes(Id id){Record({Op::Eyes,id});return A(id).eyes;}
    Region Point(Id level,Id fallback,PortableModelVec3 p){
        Record({Op::Point,level,static_cast<unsigned>(fallback),Bits(p.x),Bits(p.y),Bits(p.z)});
        if(!fallback||level!=Level)throw std::runtime_error("bad PointRegion fallback/level");
        const Id subject=inside?Nested:Source;const auto i=queries.at(subject)++;
        Region answer{subject==Nested?Id{4}:Id{3u+i},static_cast<std::int32_t>(101u+i),static_cast<std::uint8_t>(21u+i)};
        if(i>=3u)throw std::runtime_error("unexpected query count");
        if(pointUpdatesOld&&i==0u)A(subject).regions[0u].zone=answer.zone;
        return answer;
    }
    Id Old(Id id,Slot slot){Record({Op::OldZone,id,static_cast<unsigned>(slot)});return A(id).regions.at(static_cast<unsigned>(slot)).zone;}
    void Zone(Id id,Slot slot,Id value){Record({Op::Zone,id,static_cast<unsigned>(slot),value});
        A(id).regions.at(static_cast<unsigned>(slot)).zone=value;
        if(writeUpdatesEyes&&slot==Slot::Foot)A(id).eyes=100;
    }
    void Leaf(Id id,Slot slot,std::int32_t value){Record({Op::Leaf,id,static_cast<unsigned>(slot),std::bit_cast<std::uint32_t>(value)});
        A(id).regions.at(static_cast<unsigned>(slot)).leaf=value;}
    void Number(Id id,Slot slot,std::uint8_t value){Record({Op::Number,id,static_cast<unsigned>(slot),value});
        A(id).regions.at(static_cast<unsigned>(slot)).number=value;}
    void EventCall(Id receiver,Event selected,Id argument);
    std::uint8_t Mode(Id info){Record({Op::Mode,info});return A(info).mode;}
    Id Pri(Id id){Record({Op::Pri,id});return A(id).pri;}
    void PlayerZone(Id pri,Id zone){Record({Op::PlayerZone,pri,0u,zone});A(pri).playerZone=zone;}
    Queries Bind(){Queries q;
        q.deleted=[&](Id a){return Deleted(a);};q.pawn=[&](Id a){return Pawn(a);};
        q.levelActor0=[&](Id l){return First(l);};q.requireLevelInfo=[&](Id a){Class(a);};q.levelActorCount=[&](Id l){return Count(l);};
        q.location=[&](Id a,unsigned c){return Location(a,c);};q.collisionHeight=[&](Id a){return Height(a);};q.eyeHeight=[&](Id a){return Eyes(a);};
        q.pointRegion=[&](Id l,Id f,PortableModelVec3 p){return Point(l,f,p);};q.regionZone=[&](Id a,Slot s){return Old(a,s);};
        q.writeZone=[&](Id a,Slot s,Id z){Zone(a,s,z);};q.writeLeaf=[&](Id a,Slot s,std::int32_t v){Leaf(a,s,v);};
        q.writeNumber=[&](Id a,Slot s,std::uint8_t v){Number(a,s,v);};q.event=[&](Id a,Event e,Id b){EventCall(a,e,b);};
        q.netMode=[&](Id a){return Mode(a);};q.replicationInfo=[&](Id a){return Pri(a);};
        q.writePlayerZone=[&](Id a,Id z){PlayerZone(a,z);};q.work=[&]{Charge();};return q;
    }
};

// Independent execution model: labelled native phases, own provider/work
// dispatch and hardware Float stores, no production protocol/numeric helpers.
template<class F,class... Args>auto Call(World& w,F f,Args... args){w.Charge();return (w.*f)(args...);}
float FootMath(float z,float h){
    float out;
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    asm volatile("flds %1; fsubs %2; fstps %0":"=m"(out):"m"(z),"m"(h):"st");
#else
    volatile float result=z-h;out=result;
#endif
    if(!std::isfinite(z)||!std::isfinite(h)||!std::isfinite(out))throw std::runtime_error("oracle foot numeric refusal");
    return out;
}
float HeadMath(float offset,float x){
    float out;
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    asm volatile("flds %1; fadds %2; fstps %0":"=m"(out):"m"(offset),"m"(x):"st");
#else
    volatile float result=offset+x;out=result;
#endif
    if(!std::isfinite(offset)||!std::isfinite(x)||!std::isfinite(out))throw std::runtime_error("oracle head numeric refusal");
    return out;
}
Id GetInfo(World& w){if(!Call(w,&World::First,Level))throw std::runtime_error("no first slot");
    const auto check=Call(w,&World::First,Level);Call(w,&World::Class,check);return Call(w,&World::First,Level);}
void Store(World& w,Id a,Slot s,Region r){Call(w,&World::Zone,a,s,r.zone);Call(w,&World::Leaf,a,s,r.leaf);Call(w,&World::Number,a,s,r.number);}
void Notify(World& w,Id a,Event e,Id b){if(!a)throw std::runtime_error("unsafe event receiver");Call(w,&World::EventCall,a,e,b);}
void Interpret(World& w,Id actor,std::int32_t test,std::int32_t force){
    if(!actor)throw std::runtime_error("null actor");
    if(Call(w,&World::Deleted,actor))return;
    const auto first=GetInfo(w);
    if(first==actor){const auto check=Call(w,&World::First,Level);Call(w,&World::Class,check);
        const auto value=Call(w,&World::First,Level);Store(w,actor,Slot::Main,{value,-1,0});return;}
    const bool pawn=Call(w,&World::Pawn,actor);
    if(force){const auto info=GetInfo(w);Store(w,actor,Slot::Main,{info,-1,0});
        if(pawn){const auto paired=GetInfo(w);Call(w,&World::Zone,actor,Slot::Head,paired);Call(w,&World::Zone,actor,Slot::Foot,paired);
            Call(w,&World::Leaf,actor,Slot::Head,std::int32_t{-1});Call(w,&World::Leaf,actor,Slot::Foot,std::int32_t{-1});
            Call(w,&World::Number,actor,Slot::Head,std::uint8_t{});Call(w,&World::Number,actor,Slot::Foot,std::uint8_t{});}}
    Id fallback=actor;if(Call(w,&World::Count,Level))fallback=GetInfo(w);
    PortableModelVec3 point;point.x=Call(w,&World::Location,actor,0u);point.y=Call(w,&World::Location,actor,1u);point.z=Call(w,&World::Location,actor,2u);
    const auto main=Call(w,&World::Point,Level,fallback,point);const auto old=Call(w,&World::Old,actor,Slot::Main);
    const bool events=main.zone!=old&&!test;
    if(events){Notify(w,old,Event::Leaving,actor);Notify(w,actor,Event::Change,main.zone);}
    Store(w,actor,Slot::Main,main);
    if(events){const auto entered=Call(w,&World::Old,actor,Slot::Main);Notify(w,entered,Event::Entered,actor);}
    if(!pawn)return;
    const auto height=Call(w,&World::Height,actor),z=Call(w,&World::Location,actor,2u);point.z=FootMath(z,height);
    point.y=Call(w,&World::Location,actor,1u);point.x=Call(w,&World::Location,actor,0u);
    const auto footInfo=GetInfo(w);const auto foot=Call(w,&World::Point,Level,footInfo,point);
    const auto oldFoot=Call(w,&World::Old,actor,Slot::Foot);
    if(!test&&foot.zone!=oldFoot)Notify(w,actor,Event::FootChange,foot.zone);
    const auto eyes=Call(w,&World::Eyes,actor);Store(w,actor,Slot::Foot,foot);
    const auto x=Call(w,&World::Location,actor,0u);point.x=HeadMath(0,x);
    const auto y=Call(w,&World::Location,actor,1u);point.y=HeadMath(0,y);
    const auto headZ=Call(w,&World::Location,actor,2u);point.z=HeadMath(eyes,headZ);
    const auto headInfo=GetInfo(w);const auto head=Call(w,&World::Point,Level,headInfo,point);
    const auto oldHead=Call(w,&World::Old,actor,Slot::Head);
    if(!test&&head.zone!=oldHead)Notify(w,actor,Event::HeadChange,head.zone);
    Store(w,actor,Slot::Head,head);
    const auto last=GetInfo(w);if(Call(w,&World::Mode,last)==3u)return;
    const auto pri=Call(w,&World::Pri,actor);if(pri){const auto current=Call(w,&World::Old,actor,Slot::Main);Call(w,&World::PlayerZone,pri,current);}
}
void World::EventCall(Id receiver,Event selected,Id argument){
    Record({Op::Event,receiver,static_cast<unsigned>(selected),argument});
    if(receiver==Nested||inside)return;
    auto& source=A(Source);
    if(selected==Event::Leaving&&(mutation&1u)){source.deleted=true;source.location={12,13,40};source.regions[0u].zone=9;}
    if(selected==Event::Change&&(mutation&2u)){source.location={-0.0f,7,20};source.height=9;source.regions[0u].leaf=999;}
    if(selected==Event::Entered&&(mutation&4u)){source.location={-0.0f,-0.0f,50};source.height=12;}
    if(selected==Event::FootChange&&(mutation&8u)){source.location={-0.0f,-0.0f,80};source.eyes=9;source.regions[1u].zone=9;}
    if(selected==Event::HeadChange&&(mutation&16u)){source.regions[0u].zone=8;A(Info).mode=0;source.pri=11;}
    if(nested&&selected==Event::Change){inside=true;
        try{if(oracle)Interpret(*this,Nested,0,0);else QuestVr::SetOriginalActorZone(Level,Nested,0,0,Bind());}
        catch(...){inside=false;throw;}inside=false;
    }
}
bool Same(const Actor& a,const Actor& b){
    if(a.deleted!=b.deleted||a.pawn!=b.pawn||a.info!=b.info||a.mode!=b.mode||a.pri!=b.pri||a.playerZone!=b.playerZone||
        Bits(a.height)!=Bits(b.height)||Bits(a.eyes)!=Bits(b.eyes))return false;
    for(unsigned i=0u;i<3u;++i)if(Bits(a.location[i])!=Bits(b.location[i])||!Same(a.regions[i],b.regions[i]))return false;
    return true;
}
bool Run(World& w,Id actor,std::int32_t test,std::int32_t force){try{
    if(w.oracle)Interpret(w,actor,test,force);else QuestVr::SetOriginalActorZone(Level,actor,test,force,w.Bind());return true;
}catch(const std::exception&){return false;}}
World Compare(World input,Id actor=Source,std::int32_t test=0,std::int32_t force=0){
    World actual=input,expected=input;expected.oracle=true;
    const bool a=Run(actual,actor,test,force),b=Run(expected,actor,test,force);
    Require(a==b,"Original SetActorZone success/refusal differs from the independent provider interpreter");
    Require(actual.trace==expected.trace&&actual.calls==expected.calls&&actual.work==expected.work&&actual.queries==expected.queries,
        "Original SetActorZone provider/event/work trace differs from the independent interpreter");
    for(unsigned i=0u;i<actual.actors.size();++i)Require(Same(actual.actors[i],expected.actors[i]),"Original zone tuple/callback mutation differs");
    ++cases;if(!a)++refusals;return actual;
}
template<class F>void Refuse(F run){bool failed{};try{run();}catch(const std::exception&){failed=true;}Require(failed,"Unsafe active zone dependency succeeded");++refusals;}
void Controls(){
    World w;auto q=w.Bind();w.actors[Source].deleted=true;
    Queries deletedOnly;deletedOnly.deleted=q.deleted;QuestVr::SetOriginalActorZone(0,Source,0,0,deletedOnly);
    Require(w.trace.size()==1u&&w.trace[0u].op==Op::Deleted,"Initial deletion gate touched inactive Level/Pawn providers");
    Refuse([&]{QuestVr::SetOriginalActorZone(Level,0,0,0,Queries{});});
    w=World{};w.slot0=0;Compare(w);w=World{};w.actors[Info].info=false;Compare(w);
    Compare(World{},Info,0,-1); // No point, location, Pawn, test or force access.
    for(bool pawn:{false,true})for(std::int32_t test:{0,1,-1})for(std::int32_t force:{0,1,std::numeric_limits<std::int32_t>::min()})
        for(std::uint8_t mode:{0u,3u,255u})for(unsigned same=0u;same<8u;++same){w=World{};w.actors[Source].pawn=pawn;w.actors[Info].mode=mode;
            for(unsigned i=0u;i<3u;++i)if(same&(1u<<i))w.actors[Source].regions[i].zone=3u+i;
            Compare(w,Source,test,force);
        }
    for(unsigned mutation=0u;mutation<32u;++mutation)for(std::int32_t test:{0,-7})for(std::int32_t force:{0,2}){
        w=World{};w.mutation=mutation;Compare(w,Source,test,force);++mutations;
    }
    for(const std::int32_t count:{0,-1,16}){w=World{};w.count=count;const auto actual=Compare(w);
        const auto point=std::find_if(actual.trace.begin(),actual.trace.end(),[](auto row){return row.op==Op::Point;});
        Require(point!=actual.trace.end()&&point->selector==(count?Info:Source),"Main PointRegion fallback ignored the live Level count");
    }
    w=World{};w.pointUpdatesOld=true;const auto postquery=Compare(w);
    Require(postquery.trace.size()>0u&&std::none_of(postquery.trace.begin(),postquery.trace.end(),[](auto row){return row.op==Op::Event&&
        (row.selector==static_cast<unsigned>(Event::Leaving)||row.selector==static_cast<unsigned>(Event::Change));}),
        "Old main Zone was sampled before the PointRegion dependency");
    w=World{};w.writeUpdatesEyes=true;Compare(w); // Explicit mock-only write-order adversary.
    w=World{};w.actors[Source].pri=0;Compare(w);w.actors[Source].height=-4;Compare(w);
    for(auto slot:{Slot::Main,Slot::Foot,Slot::Head}){w=World{};w.actors[Source].regions[static_cast<unsigned>(slot)].zone=0;Compare(w);}
    for(std::int32_t force:{0,1}){w=World{};const auto completed=Compare(w,Source,0,force);
        for(std::size_t i=1u;i<=completed.calls;++i){w=World{};w.failure=i;Compare(w,Source,0,force);++prefixes;}
        for(std::size_t i=0u;i<completed.work;++i){w=World{};w.limit=i;Compare(w,Source,0,force);++prefixes;}
    }
    w=World{};const auto special=Compare(w,Info,0,1);
    for(std::size_t i=1u;i<=special.calls;++i){w=World{};w.failure=i;Compare(w,Info,0,1);++prefixes;}
    w=World{};w.nested=true;w.mutation=31u;Compare(w);++reentrant;
    const auto nested=Compare(w);
    const auto nestedStart=std::find_if(nested.trace.begin(),nested.trace.end(),[](auto row){return row.op==Op::Deleted&&row.actor==Nested;});
    const auto nestedPoint=std::find_if(nestedStart,nested.trace.end(),[](auto row){return row.op==Op::Point;});
    Require(nestedStart!=nested.trace.end()&&nestedPoint!=nested.trace.end(),"Reentrant fixture never queried its nested actor");
    w.limit=static_cast<std::size_t>(nestedPoint-nested.trace.begin())+1u;const auto partial=Compare(w);++reentrant;
    Require(partial.work==w.limit+1u&&partial.queries[Nested]>0u,"Nested zone query reset the shared root work allowance");
    // Explicit MOCK outer root: the detached kernel propagates refusal, while
    // this caller snapshot restores both actors. It is not a runtime adapter.
    const auto before=w.actors;auto root=w;
    if(!Run(root,Source,0,0))root.actors=before;
    for(unsigned i=0u;i<before.size();++i)Require(Same(root.actors[i],before[i]),"Mock caller root did not restore its zone/callback mutations");
    const float nan=std::bit_cast<float>(0x7fc12345u),inf=std::numeric_limits<float>::infinity();
    for(float bad:{nan,inf,-inf}){w=World{};w.actors[Source].height=bad;Compare(w);w=World{};w.actors[Source].eyes=bad;Compare(w);}
    w=World{};w.actors[Source].location={-0.0f,-0.0f,30};const auto zeros=Compare(w);
    std::vector<Trace> points;for(auto row:zeros.trace)if(row.op==Op::Point)points.push_back(row);
    Require(points.size()==3u&&points[1u].a==0x80000000u&&points[1u].b==0x80000000u&&points[2u].a==0u&&points[2u].b==0u,
        "Raw foot X/Y and head +0 X/Y lost their distinct signed-zero words");
    for(unsigned missing=0u;missing<4u;++missing){w=World{};q=w.Bind();
        if(missing==0u)q.event={};
        if(missing==1u)q.pointRegion={};
        if(missing==2u)q.writeLeaf={};
        if(missing==3u)q.netMode={};
        Refuse([&]{QuestVr::SetOriginalActorZone(Level,Source,0,1,q);});
    }
}
std::uint64_t Next(std::uint64_t& x){x^=x<<13u;x^=x>>7u;x^=x<<17u;return x;}
void Numbers(){
    std::uint64_t seed=0x98e12d43a56b7c09ull;
    for(unsigned i=0u;i<30000u;++i){const auto number=[&]{const auto x=static_cast<std::uint32_t>(Next(seed));
        return std::bit_cast<float>((x&0x807fffffu)|(((x>>23u)%255u)<<23u));};
        const float a=number(),b=number();
        for(bool head:{false,true}){float actual{},expected{};bool failed{},oracleFailed{};
            try{actual=head?QuestVr::ActorZoneDetail::HeadChannel(a,b):QuestVr::ActorZoneDetail::FootZ(a,b);}catch(const std::exception&){failed=true;}
            try{expected=head?HeadMath(a,b):FootMath(a,b);}catch(const std::exception&){oracleFailed=true;}
            Require(failed==oracleFailed&&(failed||Bits(actual)==Bits(expected)),"Foot/head Float store differs from independent native arithmetic");++numeric;
        }
    }
}
} // namespace
int main(){try{
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    struct Precision{unsigned short previous{};Precision(){asm volatile("fnstcw %0":"=m"(previous));
        const auto mode=static_cast<unsigned short>((previous&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
        ~Precision(){asm volatile("fldcw %0"::"m"(previous));}} precision;
#endif
    Controls();Numbers();std::cout<<"PASS original SetActorZone "<<checks<<" checks / "<<refusals<<" refusals, "<<cases
    <<" independent provider cases, "<<mutations<<" callback-mutation cases, "<<prefixes<<" provider/work prefixes, "<<reentrant
    <<" reentrant controls and "<<numeric<<" numeric comparisons. Detached protocol only; MOCK rollback is not real movement/startup.\n";return 0;
}catch(const std::exception& error){std::cerr<<"Original SetActorZone failed: "<<error.what()<<'\n';return 1;}}
