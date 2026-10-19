#include "quest_level_encroachment.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Detached protocol only: every actor, linked hit, memory mark, dependency
// operation and caller root below is synthetic. No real MoveActor, accepted
// Training movement, gravity or destruction is supplied by these fixtures.
namespace {
using Id=std::uintptr_t;
using Vec=PortableModelVec3;
using Rotation=std::array<std::int32_t,3u>;
constexpr Id source=1u,levelInfo=2u,level=31u,hash=41u;
constexpr Vec proposed{11.0f,22.0f,33.0f};
constexpr Rotation rotation{101,-202,303};
std::size_t checks{},cases{},refusals{},staticCases{},mutationCases{},failurePrefixes{},mockRoots{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
bool Same(float a,float b){return Bits(a)==Bits(b);}
bool Same(Vec a,Vec b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
struct Call {
    std::string name;
    std::array<Id,4u> args{};
    std::array<std::uint32_t,3u> words{};
    Rotation angles{};
    bool operator==(const Call&)const=default;
};
Call Named(std::string name,Id a=0u,Id b=0u,Id c=0u,Id d=0u){return {std::move(name),{a,b,c,d},{},{}};}
Call VectorCall(std::string name,Id a,Vec value,Id b=0u){return {std::move(name),{a,b,0u,0u},{Bits(value.x),Bits(value.y),Bits(value.z)},{}};}
struct Actor {
    std::uint32_t flags{1u};bool brush{},brushClass{},isStatic{};
    Vec location{1.0f,2.0f,3.0f};
    Rotation angles{7,8,9};
    std::array<Id,4u> touching{};
};
bool Same(const Actor& a,const Actor& b){return a.flags==b.flags&&a.brush==b.brush&&a.brushClass==b.brushClass&&a.isStatic==b.isStatic&&
    Same(a.location,b.location)&&a.angles==b.angles&&a.touching==b.touching;}
struct Hit {Id actor{},next{};bool operator==(const Hit&)const=default;};
struct World {
    std::array<Actor,12u> actors{};
    std::array<Hit,24u> hits{};
    std::array<bool,12u> blocked{},overlaps{};
    std::vector<Id> heads{1u};
    std::vector<Call> trace,events;
    std::vector<std::array<std::size_t,2u>> marks;
    Id currentHash{hash},actualLevelInfo{levelInfo};
    Id markToken{71u};
    bool validLevelInfo{true},withWork{true};
    unsigned queryCount{},smoothCount{},aux{};
    std::size_t top{20u},chunk{30u},markDepth{},work{},limit=std::numeric_limits<std::size_t>::max(),
        failAt=std::numeric_limits<std::size_t>::max();
    std::int32_t smoothReturn{-71},veto{};
    std::function<void(World&,const Call&)> before,after;
    World(){actors[0u].flags=0u;hits[1u]={3u,0u};blocked.fill(true);overlaps.fill(true);}
    Actor& At(Id id){if(!id||id>=actors.size())throw std::runtime_error("Synthetic actor provider refused null/outside actor");return actors[static_cast<std::size_t>(id)];}
    Hit& H(Id id){if(!id||id>=hits.size())throw std::runtime_error("Synthetic hit provider refused null/outside hit");return hits[static_cast<std::size_t>(id)];}
    void Work(){if(++work>limit)throw std::runtime_error("Shared mock encroachment work exhausted");}
    void Enter(const Call& call){
        // Ordinary work may allocate; mandatory cleanup may not. Retain room
        // for this record plus BOTH possible unmetered cleanup records even
        // when the next work hook refuses before another Enter can reserve.
        trace.reserve(trace.size()+3u);
        const auto index=trace.size();trace.push_back(call);
        if(index==failAt)throw std::runtime_error("Injected active encroachment provider refusal");
        if(before)before(*this,call);
    }
    void Leave(const Call& call){if(after)after(*this,call);}
    template<class Value,class Read>Value Get(const Call& call,Read read){Enter(call);const auto value=read();Leave(call);return value;}
    template<class Write>void Set(const Call& call,Write write){Enter(call);write();Leave(call);}
    std::uint32_t Flags(Id id){return Get<std::uint32_t>(Named("flags",id),[&]{return At(id).flags;});}
    bool Moving(Id id){return Get<bool>(Named("moving",id),[&]{const auto& a=At(id);return a.brush&&a.brushClass&&!a.isStatic;});}
    Id Hash(){return Get<Id>(Named("hash",level),[&]{return currentHash;});}
    Id ActorAt(Id hit){return Get<Id>(Named("actor",hit),[&]{return H(hit).actor;});}
    Id Next(Id hit){return Get<Id>(Named("next",hit),[&]{return H(hit).next;});}
    Id Actor0(){return Get<Id>(Named("actor0",level),[&]{return actualLevelInfo;});}
    void RequireInfo(Id id){Set(Named("requireInfo",id),[&]{if(!id||!validLevelInfo)throw std::runtime_error("Actual Actors[0] is not original LevelInfo");});}
    bool Blocked(Id id){return Get<bool>(Named("blocked",source,id),[&]{At(id);return blocked[static_cast<std::size_t>(id)];});}
    float Location(Id id,unsigned axis){return Get<float>(Named("location",id,axis),[&]{const auto p=At(id).location;return axis==0u?p.x:axis==1u?p.y:p.z;});}
    void StoreChannel(Id id,unsigned axis,std::uint32_t word){Set(Named("storeChannel",id,axis,word),[&]{auto& p=At(id).location;
        const auto value=std::bit_cast<float>(word);if(axis==0u)p.x=value;else if(axis==1u)p.y=value;else p.z=value;});}
    void Restore(Id id,std::array<std::uint32_t,3u> words)noexcept{
        trace.push_back({"restore",{id,0u,0u,0u},words,{}});auto& p=actors[static_cast<std::size_t>(id)].location;
        p={std::bit_cast<float>(words[0u]),std::bit_cast<float>(words[1u]),std::bit_cast<float>(words[2u])};
    }
    Id Mark(){return Get<Id>(Named("mark"),[&]{marks.push_back({top,chunk});++markDepth;return markToken;});}
    void Pop(Id token)noexcept{trace.push_back(Named("pop",token));const auto saved=marks.back();marks.pop_back();top=saved[0u];chunk=saved[1u];--markDepth;}
    Id Touching(Id id,unsigned slot){return Get<Id>(Named("touching",id,slot),[&]{if(slot>=4u)throw std::runtime_error("More than four original Touching slots");return At(id).touching[slot];});}
    bool Overlap(Id id){return Get<bool>(Named("overlap",source,id),[&]{At(id);return overlaps[static_cast<std::size_t>(id)];});}
    void End(Id id,std::int32_t suppress){Set(Named("endTouch",source,id,static_cast<Id>(static_cast<std::uint32_t>(suppress))),[&]{At(id);events.push_back(Named("endTouch",source,id));});}
    void Begin(Id id){Set(Named("beginTouch",source,id),[&]{At(id);events.push_back(Named("beginTouch",source,id));});}
    void Encroached(Id id){Set(Named("encroached",id,source),[&]{At(id);events.push_back(Named("encroached",id,source));});}
    std::int32_t Encroaching(Id id){return Get<std::int32_t>(Named("encroaching",source,id),[&]{At(id);events.push_back(Named("encroaching",source,id));return veto;});}
    std::int32_t Smooth(Id id,Vec delta){return Get<std::int32_t>(VectorCall("smooth",id,delta),[&]{auto& a=At(id);++smoothCount;
        a.location={a.location.x+delta.x,a.location.y+delta.y,a.location.z+delta.z};return smoothReturn;});}
    Id Query(Id identity,Vec location,Rotation angles,std::uint32_t flags){
        auto call=VectorCall("query",identity,location,source);call.angles=angles;call.args[2u]=flags;
        return Get<Id>(call,[&]{if(!identity)throw std::runtime_error("Absent active hash cannot be queried");
            if(queryCount>=heads.size())throw std::runtime_error("Extra inactive hash query");
            ++top;++chunk;return heads[queryCount++];});
    }
};
bool Same(const World& a,const World& b){for(std::size_t i=0u;i<a.actors.size();++i)if(!Same(a.actors[i],b.actors[i]))return false;
    return a.hits==b.hits&&a.blocked==b.blocked&&a.overlaps==b.overlaps&&a.currentHash==b.currentHash&&a.actualLevelInfo==b.actualLevelInfo&&
        a.validLevelInfo==b.validLevelInfo&&a.queryCount==b.queryCount&&a.smoothCount==b.smoothCount&&a.aux==b.aux&&a.top==b.top&&a.chunk==b.chunk&&a.markDepth==b.markDepth&&a.events==b.events&&a.marks==b.marks;}
bool Has(const World& w,const std::string& name,Id a,Id b=0u){return std::any_of(w.trace.begin(),w.trace.end(),[&](const Call& c){return c.name==name&&c.args[0u]==a&&c.args[1u]==b;});}
std::size_t Count(const World& w,const std::string& name){return static_cast<std::size_t>(std::count_if(w.trace.begin(),w.trace.end(),[&](const Call& c){return c.name==name;}));}

using Queries=QuestVr::OriginalLevelEncroachmentQueries;
Queries Providers(World& w){
    Queries q;
    q.admissionFlags=[&](Id id){return w.Flags(id);};q.movingBrush=[&](Id id){return w.Moving(id);};
    q.levelHash=[&](Id id){Require(id==level,"Wrong native Level.Hash receiver");return w.Hash();};
    q.mark=[&]{return w.Mark();};q.popMark=[&](Id token)noexcept{w.Pop(token);};
    q.hashEncroachment=[&](Id identity,Id id,Vec location,Rotation angles,std::uint32_t flags){
        Require(id==source,"Hash query used candidate as source");return w.Query(identity,location,angles,flags);};
    q.hitActor=[&](Id id){return w.ActorAt(id);};q.hitNext=[&](Id id){return w.Next(id);};
    q.levelActor0=[&](Id id){Require(id==level,"Wrong Level Actors[0] receiver");return w.Actor0();};
    q.requireLevelInfo=[&](Id id){w.RequireInfo(id);};
    q.blockedBy=[&](Id a,Id b){Require(a==source,"IsBlockedBy direction was reversed");return w.Blocked(b);};
    q.locationChannel=[&](Id id,unsigned axis){Require(axis<3u,"Invalid source Location channel");return w.Location(id,axis);};
    q.writeLocationChannel=[&](Id id,unsigned axis,std::uint32_t word){Require(axis<3u,"Invalid source Location write channel");w.StoreChannel(id,axis,word);};
    q.restoreLocation=[&](Id id,std::array<std::uint32_t,3u> words)noexcept{w.Restore(id,words);};
    q.moveSmooth=[&](Id id,Vec delta){return w.Smooth(id,delta);};
    q.encroachingOn=[&](Id a,Id b){Require(a==source,"EncroachingOn receiver changed");return w.Encroaching(b);};
    q.encroachedBy=[&](Id a,Id b){Require(b==source,"EncroachedBy Other was not source");w.Encroached(a);};
    q.touchSlot=[&](Id id,unsigned slot){return w.Touching(id,slot);};
    q.overlaps=[&](Id a,Id b){Require(a==source,"Overlap receiver changed");return w.Overlap(b);};
    q.endTouch=[&](Id a,Id b,std::int32_t suppress){Require(a==source,"EndTouch receiver changed");w.End(b,suppress);};
    q.beginTouch=[&](Id a,Id b){Require(a==source,"BeginTouch receiver changed");w.Begin(b);};
    if(w.withWork)q.work=[&]{w.Work();};
    return q;
}
double Difference(float a,float b){
    if(!std::isfinite(a)||!std::isfinite(b))throw std::runtime_error("Independent active push operand is unsafe");
    // A subtraction of two IEEE Float inputs needs no production arithmetic
    // helper: binary64 independently retains sufficient precision to round
    // its result to Float. This is not an oracle for MoveSmooth internals.
    volatile double x=a,y=b,result=x-y;return result;
}
float Spill(double value){
    volatile float result=static_cast<float>(value);
    if(!std::isfinite(result))throw std::runtime_error("Independent push Float overflow");
    return result;
}
float Reverse(float value){return std::bit_cast<float>(Bits(value)^0x80000000u);}
struct RetailMachine {
    World& w;std::size_t maximum{65536u},visited{};
    void Charge(){if(w.withWork)w.Work();}
    template<class F>auto Read(F f){Charge();return f();}
    void Visit(){if(visited==maximum)throw std::runtime_error("Portable cumulative link safety refused a cycle/limit");++visited;}
    Id Actor(Id id){return Read([&]{return w.ActorAt(id);});}
    Id Next(Id id){return Read([&]{return w.Next(id);});}
    Id Slot0(){return Read([&]{return w.Actor0();});}
    void Info(){const auto first=Slot0();if(!first)throw std::runtime_error("Original Actors[0] assertion");
        const auto second=Slot0();Read([&]{w.RequireInfo(second);});}
    bool Check(Id id,Vec p,Rotation r,std::int32_t touch){
        if(!level||!id)throw std::invalid_argument("Portable Level/source admission");
        const auto flags=Read([&]{return w.Flags(id);});
        if(!(flags&0x0du)&&!Read([&]{return w.Moving(id);}))return false;
        const auto token=Read([&]{return w.Mark();});bool marked=true;
        try {
            const auto result=Body(id,p,r,touch);w.Pop(token);marked=false;return result;
        }catch(...){if(marked)w.Pop(token);throw;}
    }
    bool Body(Id id,Vec p,Rotation r,std::int32_t touch){
        Id head{},hit{},candidate{},identity{},query{},peer{};Vec delta{};
        identity=Read([&]{return w.Hash();});if(identity)head=Read([&]{return w.Query(identity,p,r,0u);});hit=head;
    first_pass:
        if(!hit)goto stale_touches;
        Visit();candidate=Actor(hit);if(candidate==id)goto first_next;
        Info();candidate=Actor(hit);identity=Slot0();if(candidate==identity)goto first_next;
        if(!Read([&]{return w.Blocked(candidate);}))goto first_next;
        if(!Read([&]{return w.Moving(id);}))goto veto_event;
        candidate=Actor(hit);if(Read([&]{return w.Moving(candidate);}))goto veto_event;
        {
            if(!std::isfinite(p.z))throw std::runtime_error("Portable unsafe proposed Z boundary");
            const auto z=Difference(p.z,Read([&]{return w.Location(id,2u);}));
            if(!std::isfinite(p.y))throw std::runtime_error("Portable unsafe proposed Y boundary");
            const auto y=Difference(p.y,Read([&]{return w.Location(id,1u);}));
            if(!std::isfinite(p.x))throw std::runtime_error("Portable unsafe proposed X boundary");
            const auto x=Difference(p.x,Read([&]{return w.Location(id,0u);}));
            delta={Spill(x),Spill(y),Spill(z)};
        }
        candidate=Actor(hit);Read([&]{return w.Smooth(candidate,delta);});
        identity=Read([&]{return w.Hash();});if(!identity)throw std::runtime_error("Portable lost requery hash safety refusal");
        query=Read([&]{return w.Query(identity,p,r,0u);});
    requery_scan:
        if(!query)goto first_next;
        Visit();peer=Actor(query);candidate=Actor(hit);
        if(peer==candidate)goto reverse_push;
        query=Next(query);goto requery_scan;
    reverse_push:
        {
            const Vec reverse{Reverse(delta.x),Reverse(delta.y),Reverse(delta.z)};
            std::array<std::uint32_t,3u> saved{};
            for(unsigned axis=0u;axis<3u;++axis)saved[axis]=Bits(Read([&]{return w.Location(id,axis);}));
            // Stronger portable exception policy, NOT inferred retail EH:
            // once raw snapshot exists, even denied/throwing first write must
            // be unable to prevent nonthrowing unmetered restoration.
            try {
                Read([&]{w.StoreChannel(id,0u,Bits(p.x));});Read([&]{w.StoreChannel(id,1u,Bits(p.y));});
                Read([&]{w.StoreChannel(id,2u,Bits(p.z));});candidate=Actor(hit);Read([&]{return w.Smooth(candidate,reverse);});
                w.Restore(id,saved);
            }catch(...){w.Restore(id,saved);throw;}
        }
    veto_event:
        candidate=Actor(hit);if(Read([&]{return w.Encroaching(candidate);})!=0)return true;
    first_next:
        hit=Next(hit);goto first_pass;
    stale_touches:
        if(touch)for(unsigned slot=0u;slot<4u;++slot){peer=Read([&]{return w.Touching(id,slot);});
            if(peer&&!Read([&]{return w.Overlap(peer);})){peer=Read([&]{return w.Touching(id,slot);});Read([&]{w.End(peer,0);});}}
        hit=head;
    second_pass:
        if(!hit)return false;
        Visit();candidate=Actor(hit);if(candidate==id)goto second_next;
        Info();identity=Slot0();candidate=Actor(hit);if(candidate==identity)goto second_next;
        if(Read([&]{return w.Blocked(candidate);})){
            candidate=Actor(hit);Read([&]{w.Encroached(candidate);});
        }else if(touch){candidate=Actor(hit);Read([&]{w.Begin(candidate);});}
    second_next:
        hit=Next(hit);goto second_pass;
    }
};
struct Outcome {bool refused{},veto{};bool operator==(const Outcome&)const=default;};
Outcome Run(World& w,bool reference,Vec p=proposed,Rotation r=rotation,std::int32_t touch=0,std::size_t maximum=65536u,Id id=source,Id levelId=level){
    Outcome result;try{
        if(reference){if(!levelId||!id)throw std::invalid_argument("Portable Level/source admission");
            RetailMachine machine{w,maximum};result.veto=machine.Check(id,p,r,touch);}
        else result.veto=QuestVr::CheckOriginalLevelEncroachment(levelId,id,p,r,touch,Providers(w),maximum);
    }catch(const std::exception&){result.refused=true;}return result;
}
struct Result {World world;Outcome outcome;};
Result Compare(const World& seed,Vec p=proposed,Rotation r=rotation,std::int32_t touch=0,std::size_t maximum=65536u,Id id=source,Id levelId=level){
    World actual=seed,expected=seed;const auto truth=Run(expected,true,p,r,touch,maximum,id,levelId),got=Run(actual,false,p,r,touch,maximum,id,levelId);
    Require(got==truth,"Level encroachment result/refusal differs from independent original protocol");
    if(actual.trace!=expected.trace){
        std::size_t index{};while(index<actual.trace.size()&&index<expected.trace.size()&&actual.trace[index]==expected.trace[index])++index;
        throw std::runtime_error("Level provider transcript differs at "+std::to_string(index)+": "+
            (index<actual.trace.size()?actual.trace[index].name:"<end>")+" vs "+(index<expected.trace.size()?expected.trace[index].name:"<end>"));
    }
    ++checks;
    Require(Same(actual,expected),"Level encroachment changed callback/world/mark outcomes from independent interpreter");
    Require(actual.work==expected.work,"Level protocol reset/misordered the shared caller work allowance");
    ++cases;if(got.refused)++refusals;return {std::move(actual),got};
}
World Moving(){World w;w.actors[source].brush=w.actors[source].brushClass=true;return w;}
void StaticControlFlow(){
    const auto direct=Compare(World{});
    Require(!direct.outcome.refused&&!direct.outcome.veto&&Count(direct.world,"encroaching")==1u&&Count(direct.world,"encroached")==1u&&
        Count(direct.world,"mark")==1u&&Count(direct.world,"pop")==1u&&direct.world.top==20u&&direct.world.chunk==30u,
        "Direct nonveto blocker failed both ordered event passes or normal memory-mark release");
    Require(direct.world.trace==std::vector<Call>{Named("flags",source),Named("mark"),Named("hash",level),
        [&]{auto c=VectorCall("query",hash,proposed,source);c.angles=rotation;return c;}(),
        Named("actor",1u),Named("actor0",level),Named("actor0",level),Named("requireInfo",levelInfo),Named("actor",1u),Named("actor0",level),
        Named("blocked",source,3u),Named("moving",source),Named("actor",1u),Named("encroaching",source,3u),Named("next",1u),
        Named("actor",1u),Named("actor0",level),Named("actor0",level),Named("requireInfo",levelInfo),Named("actor0",level),Named("actor",1u),
        Named("blocked",source,3u),Named("actor",1u),Named("encroached",3u,source),Named("next",1u),Named("pop",71u)},
        "Explicit complete direct-blocker golden transcript differs from original body");
    for(unsigned flags=0u;flags<16u;++flags)for(unsigned form=0u;form<8u;++form){
        World seed;seed.actors[source].flags=flags;seed.actors[source].brush=(form&1u)!=0u;
        seed.actors[source].brushClass=(form&2u)!=0u;seed.actors[source].isStatic=(form&4u)!=0u;
        seed.currentHash=0u;seed.heads.clear();const auto result=Compare(seed);
        const bool admitted=(flags&0x0du)||((form&3u)==3u&&!(form&4u));
        Require(!result.outcome.refused&&!result.outcome.veto&&Count(result.world,"mark")==static_cast<std::size_t>(admitted)&&
            Count(result.world,"hash")==static_cast<std::size_t>(admitted),"Admission substituted CollideWorld/static/class-name policy for original mask0D or moving Brush");
        ++staticCases;
    }
    for(bool moving:{false,true})for(bool candidateMoving:{false,true})for(bool blocking:{false,true})
        for(bool stillHit:{false,true})for(std::int32_t touch:{0,-7})for(std::int32_t veto:{0,-1})for(std::int32_t returned:{0,std::numeric_limits<std::int32_t>::min()}){
            World seed=moving?Moving():World{};seed.actors[3u].brush=seed.actors[3u].brushClass=candidateMoving;
            seed.blocked[3u]=blocking;seed.veto=veto;seed.smoothReturn=returned;seed.hits[2u]={stillHit?3u:8u,0u};seed.heads={1u,2u};
            const auto result=Compare(seed,proposed,rotation,touch);
            const bool pushed=blocking&&moving&&!candidateMoving,offered=blocking&&(!pushed||stillHit),vetoed=offered&&veto!=0;
            Require(!result.outcome.refused&&result.outcome.veto==vetoed&&result.world.queryCount==1u+static_cast<unsigned>(pushed)&&
                result.world.smoothCount==(pushed?(stillHit?2u:1u):0u)&&Count(result.world,"encroaching")==static_cast<std::size_t>(offered)&&
                Count(result.world,"encroached")==static_cast<std::size_t>(blocking&&!vetoed)&&
                Count(result.world,"beginTouch")==static_cast<std::size_t>(!blocking&&touch!=0),
                "Static branch truth table changed push escape/reverse/veto/second-pass direction or used Smooth return");
            Require(!Has(result.world,"encroached",8u,source)&&!Has(result.world,"beginTouch",source,8u),
                "Push requery candidates replaced original second-pass hit list");++staticCases;
        }
    for(std::int32_t touch:{0,1,2,-1,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()}){
        World seed;seed.blocked[3u]=false;seed.actors[source].touching={4u,0u,5u,0u};seed.overlaps[4u]=false;
        const auto result=Compare(seed,proposed,rotation,touch);
        Require(!result.outcome.refused&&Count(result.world,"touching")==(touch?5u:0u)&&Count(result.world,"overlap")==(touch?2u:0u)&&
            Count(result.world,"endTouch")==static_cast<std::size_t>(touch!=0)&&Count(result.world,"beginTouch")==static_cast<std::size_t>(touch!=0),
            "Touch integer was treated as a bit mask or affected blocking passes");++staticCases;
    }
    World skip;skip.hits[1u]={source,2u};skip.hits[2u]={levelInfo,3u};skip.hits[3u]={3u,0u};
    const auto excluded=Compare(skip);
    Require(Count(excluded.world,"blocked")==2u&&Count(excluded.world,"encroaching")==1u&&Count(excluded.world,"encroached")==1u,
        "Self or exact current Actors[0] identity entered blocking/event dependencies");
    World onlySelf;onlySelf.hits[1u].actor=source;onlySelf.validLevelInfo=false;
    const auto noInfo=Compare(onlySelf);Require(!noInfo.outcome.refused&&Count(noInfo.world,"actor0")==0u,
        "Self exclusion required inactive LevelInfo authority");
    World noHash;noHash.currentHash=0u;noHash.actors[source].touching[0u]=3u;noHash.overlaps[3u]=false;
    const auto cleanup=Compare(noHash,proposed,rotation,2);
    Require(!cleanup.outcome.refused&&cleanup.world.queryCount==0u&&Count(cleanup.world,"endTouch")==1u&&Count(cleanup.world,"pop")==1u,
        "Initial absent hash skipped mandatory touch cleanup or fabricated query failure");
    World zeroToken;zeroToken.markToken=0u;const auto opaque=Compare(zeroToken);
    Require(!opaque.outcome.refused&&Has(opaque.world,"pop",0u),"Opaque zero memory-mark token was treated as no mark");
    const float nan=std::bit_cast<float>(0x7fc12345u),inf=std::numeric_limits<float>::infinity();
    World inactive;inactive.heads={0u};const auto rawPose=Compare(inactive,{nan,-inf,inf});
    Require(!rawPose.outcome.refused&&Count(rawPose.world,"location")==0u,
        "No-push operation eagerly rejected inactive proposed pose or source coordinates");
}
void CallbackControlFlow(){
    auto escaped=Moving();escaped.heads={1u,2u};escaped.hits[2u]={8u,3u};escaped.hits[3u]={9u,0u};
    escaped.after=[](World& w,const Call& c){if(c.name=="smooth")w.currentHash=42u;};
    const auto requery=Compare(escaped);
    Require(requery.world.smoothCount==1u&&!Has(requery.world,"encroaching",source,3u)&&Has(requery.world,"encroached",3u,source)&&
        Has(requery.world,"query",42u,source)&&!Has(requery.world,"encroached",8u,source),
        "Escaped candidate, fresh Level.Hash or original-list second-pass authority differs");++mutationCases;
    auto still=Moving();still.heads={1u,2u};still.hits[2u]={3u,0u};
    const Vec saved{std::bit_cast<float>(0x7fc12345u),std::bit_cast<float>(0x80000000u),std::bit_cast<float>(1u)};
    still.veto=-1;still.before=[=](World& w,const Call& c){
        if(c.name=="smooth"&&w.smoothCount==1u)Require(Same(w.actors[source].location,proposed)&&w.actors[source].angles==Rotation{7,8,9},
            "Reverse Smooth did not see source proposed Location with ORIGINAL Rotation");
        if(c.name=="encroaching")Require(Same(w.actors[source].location,saved),"Raw source pose was not restored before veto callback");};
    still.after=[=](World& w,const Call& c){if(c.name=="smooth"){
        if(w.smoothCount==1u){w.actors[source].location=saved;w.actors[3u].location={101,102,103};}
        else {w.actors[source].location={71,72,73};w.actors[3u].location={201,202,203};}}};
    const auto restored=Compare(still);
    Require(!restored.outcome.refused&&restored.outcome.veto&&Same(restored.world.actors[source].location,saved)&&
        Same(restored.world.actors[3u].location,Vec{201,202,203})&&Count(restored.world,"restore")==1u&&Count(restored.world,"pop")==1u,
        "Ordinary native veto rolled back side effects or reverse push pretended to restore candidate coordinates");++mutationCases;
    auto originalActor=Moving();originalActor.heads={1u,2u};originalActor.hits[2u]={4u,0u};
    originalActor.after=[](World& w,const Call& c){if(c.name=="smooth"&&w.smoothCount==1u)w.hits[1u].actor=4u;};
    const auto current=Compare(originalActor);
    Require(Has(current.world,"smooth",3u)&&Has(current.world,"smooth",4u)&&Has(current.world,"encroaching",source,4u)&&
        Has(current.world,"encroached",4u,source),"Original hit Actor was cached through forward/requery/reverse/events");++mutationCases;
    auto eachCompare=Moving();eachCompare.heads={1u,2u};eachCompare.hits[2u]={3u,3u};eachCompare.hits[3u]={4u,0u};
    eachCompare.after=[](World& w,const Call& c){if(c.name=="actor"&&c.args[0u]==2u)w.hits[1u].actor=4u;};
    const auto compared=Compare(eachCompare);
    Require(Has(compared.world,"next",2u)&&Has(compared.world,"actor",3u)&&Has(compared.world,"smooth",4u),
        "Requery compared stale original Actor instead of rereading it AFTER each requery Actor");++mutationCases;
    auto liveLocation=Moving();liveLocation.heads={1u,2u};liveLocation.hits[2u]={4u,0u};
    liveLocation.after=[](World& w,const Call& c){
        if(c.name=="blocked")w.hits[1u].actor=4u;
        if(c.name=="location"&&c.args[1u]==2u&&w.smoothCount==0u)w.actors[source].location.x=5;
    };
    const auto ordered=Compare(liveLocation);
    Require(Has(ordered.world,"moving",4u)&&Has(ordered.world,"smooth",4u)&&
        std::find(ordered.world.trace.begin(),ordered.world.trace.end(),VectorCall("smooth",4u,{6,20,30}))!=ordered.world.trace.end(),
        "Forward push cached candidate or source Location before live Z,Y,X reads");++mutationCases;
    World changedNext;changedNext.hits[1u]={3u,2u};changedNext.hits[2u]={4u,3u};changedNext.hits[3u]={5u,0u};
    changedNext.after=[](World& w,const Call& c){if(c.name=="encroaching"&&c.args[1u]==3u)w.hits[1u].next=3u;};
    const auto next=Compare(changedNext);
    Require(!Has(next.world,"encroaching",source,4u)&&!Has(next.world,"encroached",4u,source)&&Has(next.world,"encroached",5u,source),
        "Original first/second traversal snapshotted Next before callback");++mutationCases;
    World changedFlag;changedFlag.after=[](World& w,const Call& c){if(c.name=="encroaching")w.blocked[3u]=false;};
    const auto flags=Compare(changedFlag,proposed,rotation,1);
    Require(Has(flags.world,"beginTouch",source,3u)&&!Has(flags.world,"encroached",3u,source),
        "Second-pass IsBlockedBy reused first-pass state after veto callback");++mutationCases;
    World secondActor;secondActor.after=[](World& w,const Call& c){if(c.name=="blocked"&&++w.aux==2u)w.hits[1u].actor=4u;};
    const auto eventReceiver=Compare(secondActor);
    Require(Has(eventReceiver.world,"encroached",4u,source)&&!Has(eventReceiver.world,"encroached",3u,source),
        "Second-pass event receiver cached hit Actor before live IsBlockedBy callback");++mutationCases;
    World secondNext;secondNext.hits[1u]={3u,2u};secondNext.hits[2u]={4u,0u};
    secondNext.after=[](World& w,const Call& c){if(c.name=="encroached"&&c.args[0u]==3u)w.hits[1u].next=0u;};
    const auto passTwoNext=Compare(secondNext);
    Require(Has(passTwoNext.world,"encroaching",source,4u)&&!Has(passTwoNext.world,"encroached",4u,source),
        "Second-pass event did not govern next original link after callback");++mutationCases;
    World slots;slots.heads={0u};slots.actors[source].touching={3u,4u,0u,0u};slots.overlaps.fill(false);
    slots.after=[](World& w,const Call& c){
        if(c.name=="overlap"&&c.args[1u]==3u)w.actors[source].touching[0u]=5u;
        if(c.name=="endTouch"&&c.args[1u]==5u){w.actors[source].touching[1u]=0u;w.actors[source].touching[2u]=6u;w.actors[source].touching[3u]=0u;}
    };
    const auto liveSlots=Compare(slots,proposed,rotation,-1);
    Require(Has(liveSlots.world,"endTouch",source,5u)&&Has(liveSlots.world,"endTouch",source,6u)&&
        !Has(liveSlots.world,"endTouch",source,3u)&&!Has(liveSlots.world,"overlap",source,4u),
        "Stale-touch cleanup cached tested peer or restarted instead of live next slot");++mutationCases;
    World slotsHit;slotsHit.blocked[3u]=false;slotsHit.actors[source].touching[0u]=5u;slotsHit.overlaps[5u]=false;
    slotsHit.after=[](World& w,const Call& c){if(c.name=="endTouch"){w.hits[1u].actor=4u;w.blocked[4u]=false;}};
    const auto cleanupHit=Compare(slotsHit,proposed,rotation,2);
    Require(Has(cleanupHit.world,"beginTouch",source,4u)&&!Has(cleanupHit.world,"beginTouch",source,3u),
        "Second original pass snapshotted Actor before touch-cleanup callbacks");++mutationCases;
    World info;info.after=[](World& w,const Call& c){
        if(c.name=="requireInfo"){++w.aux;w.actualLevelInfo=4u;w.hits[1u].actor=w.aux==1u?4u:5u;}
        if(c.name=="actor0"&&w.aux==1u&&w.hits[1u].actor==4u)w.hits[1u].actor=5u;
        if(c.name=="actor0"&&w.aux==2u)w.hits[1u].actor=4u;
    };
    const auto infoOrder=Compare(info);
    Require(!infoOrder.outcome.refused&&Count(infoOrder.world,"blocked")==0u,
        "First/second Actor-versus-third-slot0 raw read order was collapsed");++mutationCases;
    auto perCandidate=Moving();perCandidate.hits[1u]={3u,2u};perCandidate.hits[2u]={4u,0u};perCandidate.heads={1u,0u,0u};
    perCandidate.after=[](World& w,const Call& c){if(c.name=="smooth"&&c.args[0u]==3u)w.actors[source].location={5,6,7};};
    const auto liveDelta=Compare(perCandidate);
    Require(std::find(liveDelta.world.trace.begin(),liveDelta.world.trace.end(),VectorCall("smooth",4u,{6,16,26}))!=liveDelta.world.trace.end(),
        "A later candidate reused entry source Location or a prior push Delta");++mutationCases;
    const auto copied=[](bool reference){
        auto w=Moving();w.heads={1u,2u};w.hits[2u]={3u,0u};Vec callerPosition=proposed;Rotation callerRotation=rotation;
        w.after=[&](World&,const Call& c){if(c.name=="query"){callerPosition={-100,-200,-300};callerRotation={-1,-2,-3};}};
        const auto outcome=Run(w,reference,callerPosition,callerRotation);w.after={};
        Require(!outcome.refused&&Same(callerPosition,Vec{-100,-200,-300}),"By-value fixture did not mutate caller inputs");
        for(const auto& c:w.trace)if(c.name=="query")Require(c.words==std::array<std::uint32_t,3u>{Bits(proposed.x),Bits(proposed.y),Bits(proposed.z)}&&c.angles==rotation&&c.args[2u]==0u,
            "Callbacks changed by-value proposed pose/Rotation or forwarded touch as hash flags");
        return Result{std::move(w),outcome};
    };
    const auto copyExpected=copied(true),copyActual=copied(false);
    Require(copyActual.outcome==copyExpected.outcome&&copyActual.world.trace==copyExpected.world.trace&&Same(copyActual.world,copyExpected.world),
        "By-value pose protocol differs from original reference");++mutationCases;
}
std::vector<Call> Ordinary(const World& w){std::vector<Call> result;for(const auto& c:w.trace)if(c.name!="pop"&&c.name!="restore")result.push_back(c);return result;}
bool Cleanup(const Call& c){return c.name=="pop"||c.name=="restore";}
void FailureAndSafety(){
    World nonblock;nonblock.blocked[3u]=false;
    auto push=Moving();push.heads={1u,2u};push.hits[2u]={3u,0u};push.actors[source].touching[0u]=5u;push.overlaps[5u]=false;
    World veto;veto.veto=1;
    World noHash;noHash.currentHash=0u;
    auto rawPush=push;rawPush.after=[](World& w,const Call& c){if(c.name=="smooth"&&w.smoothCount==1u)
        w.actors[source].location={std::bit_cast<float>(0x7fc6789au),std::bit_cast<float>(0x80000000u),std::bit_cast<float>(1u)};};
    for(const auto& seed:std::vector<World>{World{},nonblock,push,rawPush,veto,noHash}){
        const auto golden=Compare(seed,proposed,rotation,1);Require(!golden.outcome.refused,"Failure cohort failed before injection");
        const auto ordinary=Ordinary(golden.world);std::size_t charged{};
        for(std::size_t index=0u;index<golden.world.trace.size();++index){
            if(Cleanup(golden.world.trace[index]))continue;
            auto refusal=seed;refusal.failAt=index;const auto failed=Compare(refusal,proposed,rotation,1);
            const auto prefix=Ordinary(failed.world);
            Require(failed.outcome.refused&&prefix.size()==charged+1u&&std::equal(prefix.begin(),prefix.end(),ordinary.begin()),
                "Provider refusal continued/altered the ordinary original transcript beyond its active prefix");
            if(golden.world.trace[index].name!="flags"&&golden.world.trace[index].name!="mark")
                Require(failed.world.markDepth==0u&&Count(failed.world,"pop")==1u,"Failure stranded acquired original/requery memory mark");
            ++failurePrefixes;
            refusal=seed;refusal.limit=charged;const auto exhausted=Compare(refusal,proposed,rotation,1);
            const auto workPrefix=Ordinary(exhausted.world);
            Require(exhausted.outcome.refused&&exhausted.world.work==charged+1u&&workPrefix.size()==charged&&
                std::equal(workPrefix.begin(),workPrefix.end(),ordinary.begin()),
                "Shared work failure ran its denied provider or reset allowance in nested protocol");
            if(std::any_of(exhausted.world.trace.begin(),exhausted.world.trace.end(),[](const Call& c){return c.name=="storeChannel";}))
                Require(Count(exhausted.world,"restore")==1u,"Exhausted work blocked raw source Location restoration");
            ++failurePrefixes;++charged;
        }
        auto optional=seed;optional.withWork=false;const auto noLedger=Compare(optional,proposed,rotation,1);
        Require(!noLedger.outcome.refused&&noLedger.world.work==0u&&noLedger.world.trace==golden.world.trace&&Same(noLedger.world,golden.world),
            "Absent optional detached work hook changed original normal protocol");
    }
    auto lost=Moving();lost.heads={1u};lost.after=[](World& w,const Call& c){if(c.name=="smooth")w.currentHash=0u;};
    const auto requiredHash=Compare(lost);
    Require(requiredHash.outcome.refused&&requiredHash.world.queryCount==1u&&Count(requiredHash.world,"pop")==1u&&Count(requiredHash.world,"storeChannel")==0u,
        "Lost push-requery hash became a retail false/empty result rather than portable safety refusal");
    World invalidInfo;invalidInfo.validLevelInfo=false;const auto invalid=Compare(invalidInfo);
    Require(invalid.outcome.refused&&Count(invalid.world,"blocked")==0u&&Count(invalid.world,"pop")==1u,
        "Unauthentic current Actors[0] authority was replaced by a census fallback");
    World nullInfo;nullInfo.actualLevelInfo=0u;const auto nil=Compare(nullInfo);
    Require(nil.outcome.refused&&Count(nil.world,"actor0")==1u&&Count(nil.world,"requireInfo")==0u,
        "Null first raw Actors[0] did not refuse at original assertion boundary");
    World empty;empty.heads={0u};Require(!Compare(empty,proposed,rotation,0,0u).outcome.refused,
        "Zero link allowance rejected a no-list operation");
    Require(Compare(World{},proposed,rotation,0,1u).outcome.refused&&!Compare(World{},proposed,rotation,0,2u).outcome.refused,
        "Local link cap reset between first and second ORIGINAL passes");
    Require(Compare(push,proposed,rotation,0,2u).outcome.refused&&!Compare(push,proposed,rotation,0,3u).outcome.refused,
        "Requery and both original passes did not share cumulative linked-hit admission");
    World cycle;cycle.blocked[3u]=false;cycle.hits[1u].next=1u;const auto originalCycle=Compare(cycle,proposed,rotation,0,7u);
    Require(originalCycle.outcome.refused&&Count(originalCycle.world,"actor")==14u&&Count(originalCycle.world,"pop")==1u,
        "Original cycle was silently skipped/truncated instead of a bounded refusal");
    auto queryCycle=Moving();queryCycle.heads={1u,2u};queryCycle.hits[2u]={8u,2u};const auto requeries=Compare(queryCycle,proposed,rotation,0,5u);
    Require(requeries.outcome.refused&&queryCycle.actors[source].angles==requeries.world.actors[source].angles&&Count(requeries.world,"pop")==1u,
        "Requery cycle escaped shared link safety or changed source rotation");
    for(const auto pair:std::array<std::array<Id,2u>,3u>{{{0u,source},{level,0u},{0u,0u}}}){
        const auto result=Compare(World{},proposed,rotation,0,65536u,pair[1u],pair[0u]);
        Require(result.outcome.refused&&result.world.trace.empty()&&result.world.work==0u,
            "Portable null Level/source admission invoked inactive providers");
    }
    // Self is not a blanket exclusion/admission error: only each matching
    // hit is skipped. Source itself may legitimately be Actors[0].
    World sourceInfo;sourceInfo.actualLevelInfo=source;const auto legitimate=Compare(sourceInfo);
    Require(!legitimate.outcome.refused&&Has(legitimate.world,"blocked",source,3u),"Portable admission invented source!=Actors[0]");
    const auto golden=Compare(push,proposed,rotation,1);
    for(const std::string name:{"flags","moving","hash","mark","query","actor","next","actor0","requireInfo","blocked","location","storeChannel",
        "smooth","encroaching","encroached","touching","overlap","endTouch"}){
        World actual=push;auto q=Providers(actual);
        if(name=="flags")q.admissionFlags={};else if(name=="moving")q.movingBrush={};else if(name=="hash")q.levelHash={};
        else if(name=="mark")q.mark={};else if(name=="query")q.hashEncroachment={};else if(name=="actor")q.hitActor={};
        else if(name=="next")q.hitNext={};else if(name=="actor0")q.levelActor0={};else if(name=="requireInfo")q.requireLevelInfo={};
        else if(name=="blocked")q.blockedBy={};else if(name=="location")q.locationChannel={};else if(name=="storeChannel")q.writeLocationChannel={};
        else if(name=="smooth")q.moveSmooth={};else if(name=="encroaching")q.encroachingOn={};else if(name=="encroached")q.encroachedBy={};
        else if(name=="touching")q.touchSlot={};else if(name=="overlap")q.overlaps={};else q.endTouch={};
        const auto ordinary=Ordinary(golden.world);const auto stop=std::find_if(ordinary.begin(),ordinary.end(),[&](const Call& c){return c.name==name;});
        Require(stop!=ordinary.end(),"Missing active provider cohort did not activate its target");
        bool refused{};try{static_cast<void>(QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,1,q));}catch(const std::exception&){refused=true;}
        const auto prefix=Ordinary(actual);const auto size=static_cast<std::size_t>(stop-ordinary.begin());
        Require(refused&&prefix.size()==size&&actual.work==size&&std::equal(prefix.begin(),prefix.end(),ordinary.begin()),
            "Missing active provider was preflighted eagerly or charged despite its absence");++refusals;
    }
    World noPop;auto pop=Providers(noPop);pop.popMark={};bool refused{};
    try{static_cast<void>(QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,0,pop));}catch(const std::exception&){refused=true;}
    Require(refused&&noPop.trace==std::vector<Call>{Named("flags",source)}&&noPop.work==1u,
        "Missing mandatory pop provider acquired a mark or charged inactive acquisition");++refusals;
    World noRestore=push;auto restore=Providers(noRestore);restore.restoreLocation={};refused=false;
    try{static_cast<void>(QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,1,restore));}catch(const std::exception&){refused=true;}
    Require(refused&&Count(noRestore,"storeChannel")==0u&&Count(noRestore,"restore")==0u&&Count(noRestore,"pop")==1u&&Same(noRestore.actors[source].location,Vec{1,2,3}),
        "Missing raw restore provider wrote source pose or stranded memory mark");++refusals;
    World dormant;dormant.actors[source].flags=0u;auto d=Providers(dormant);d.popMark={};d.mark={};d.levelHash={};
    Require(!QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,0,d)&&Count(dormant,"mark")==0u,
        "Early retail false required inactive resource/hash providers");
    World absent;absent.currentHash=0u;auto a=Providers(absent);a.hashEncroachment={};a.hitActor={};a.hitNext={};a.levelActor0={};a.requireLevelInfo={};
    a.blockedBy={};a.locationChannel={};a.restoreLocation={};a.moveSmooth={};a.encroachingOn={};a.encroachedBy={};a.touchSlot={};
    Require(!QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,0,a)&&Count(absent,"pop")==1u,
        "Initial nohash/noTouch path preflighted inactive movement/event/link providers");
    World begin;begin.blocked[3u]=false;auto b=Providers(begin);b.beginTouch={};refused=false;
    try{static_cast<void>(QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,-1,b));}catch(const std::exception&){refused=true;}
    Require(refused&&Count(begin,"beginTouch")==0u&&Count(begin,"pop")==1u,"Missing active BeginTouch silently skipped original second pass");++refusals;
}
std::uint64_t NextRandom(std::uint64_t& state){state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
void NumericAndLayouts(){
    for(unsigned word=0u;word<64u;++word)for(unsigned mask=0u;mask<4u;++mask)for(std::int32_t touch:{0,-1}){
        World seed;auto code=word;for(unsigned h=1u;h<=3u;++h){seed.hits[h]={1u+code%4u,h==3u?0u:h+1u};code/=4u;}
        seed.blocked[3u]=(mask&1u)!=0u;seed.blocked[4u]=(mask&2u)!=0u;const auto result=Compare(seed,proposed,rotation,touch);
        unsigned blockers{},nonblockers{};for(unsigned h=1u;h<=3u;++h){const auto id=seed.hits[h].actor;if(id<=levelInfo)continue;
            if(seed.blocked[static_cast<std::size_t>(id)])++blockers;else ++nonblockers;}
        Require(!result.outcome.refused&&Count(result.world,"encroaching")==blockers&&Count(result.world,"encroached")==blockers&&
            Count(result.world,"beginTouch")==static_cast<std::size_t>(touch?nonblockers:0u),"Three-record layout changed duplicate/self/LevelInfo encounter event multiplicity");++staticCases;
    }
    std::uint64_t state=0x274d036b251ca9f1ull;
    for(unsigned iteration=0u;iteration<512u;++iteration){
        const auto number=[&]{const auto bits=static_cast<std::uint32_t>(NextRandom(state));return std::bit_cast<float>((bits&0x807fffffu)|(((bits>>23u)%255u)<<23u));};
        auto seed=Moving();seed.actors[source].location={number(),number(),number()};const Vec p{number(),number(),number()};
        seed.heads={1u,2u};seed.hits[2u]={iteration&1u?3u:8u,0u};const auto result=Compare(seed,p);
        if(!result.outcome.refused)Require(Count(result.world,"location")==((iteration&1u)?6u:3u),
            "Finite push projection did not keep exact source channel count");
        ++staticCases;
    }
    for(unsigned axis=0u;axis<3u;++axis)for(float unsafe:{std::bit_cast<float>(0x7fc12345u),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        auto seed=Moving();seed.heads={1u,0u};Vec p=proposed;
        if(axis==0u)p.x=unsafe;else if(axis==1u)p.y=unsafe;else p.z=unsafe;
        const auto proposedUnsafe=Compare(seed,p);
        Require(proposedUnsafe.outcome.refused&&Count(proposedUnsafe.world,"location")==2u-axis&&Count(proposedUnsafe.world,"smooth")==0u,
            "Active unsafe proposed component crossed its explicit portable before-load boundary");
        p=proposed;if(axis==0u)seed.actors[source].location.x=unsafe;else if(axis==1u)seed.actors[source].location.y=unsafe;else seed.actors[source].location.z=unsafe;
        const auto sourceUnsafe=Compare(seed,p);
        Require(sourceUnsafe.outcome.refused&&Count(sourceUnsafe.world,"location")==3u-axis&&Count(sourceUnsafe.world,"smooth")==0u,
            "Active unsafe source component did not refuse after exact raw channel load");
    }
    auto overflow=Moving();overflow.actors[source].location.z=-std::numeric_limits<float>::max();
    const auto spilled=Compare(overflow,{11,22,std::numeric_limits<float>::max()});
    Require(spilled.outcome.refused&&Count(spilled.world,"location")==3u&&Count(spilled.world,"smooth")==0u,
        "Float Z overflow was rejected before original X/Y/Z spill phase");
}
void NestedMarks(){
    for(bool failInner:{false,true}){
        World world;world.heads={1u,0u};auto q=Providers(world);
        const auto original=q.encroachingOn;bool entered{};
        q.encroachingOn=[&](Id a,Id b){
            Require(!entered&&world.markDepth==1u&&world.marks.size()==1u&&world.top==21u&&world.chunk==31u,
                "Reentrant Level fixture did not retain its outer list mark");
            entered=true;const auto used=world.work;
            Require(!QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,0,q),
                "Empty inner original query unexpectedly vetoed");
            Require(world.markDepth==1u&&world.marks.size()==1u&&world.top==21u&&world.chunk==31u&&world.work>used,
                "Inner Level cleanup popped/restored the outer mark or reset its shared work");
            return original(a,b);
        };
        if(failInner)world.before=[](World& state,const Call& call){
            if(call.name=="query"&&state.queryCount==1u)throw std::runtime_error("Injected nested query refusal");
        };
        bool failed{};try{Require(!QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,0,q),
            "Ordinary nested fixture vetoed");}catch(const std::exception&){failed=true;}
        Require(entered&&failed==failInner&&world.markDepth==0u&&world.marks.empty()&&world.top==20u&&world.chunk==30u&&
            Count(world,"mark")==2u&&Count(world,"pop")==2u,
            "Nested Level success/refusal failed exact LIFO mark cleanup");
        if(failInner)++refusals;
    }
}
void CallerOwnedRoot(){
    struct Root{
        World world,before;bool poisoned{};
        explicit Root(World seed):world(seed),before(std::move(seed)){}
        void Call(const std::function<void()>& f){try{f();}catch(...){poisoned=true;throw;}}
        void Commit(){if(poisoned)throw std::runtime_error("Mock Level caller root is poisoned");}
        void Rollback(){const auto work=world.work;world=before;world.work=work;}
    };
    auto seed=Moving();seed.heads={1u,2u};seed.hits[2u]={3u,0u};seed.before=[](World&,const Call& c){if(c.name=="encroached")throw std::runtime_error("Late event cannot execute");};
    Root failed(seed);auto q=Providers(failed.world);bool refused{};
    try{failed.Call([&]{static_cast<void>(QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,0,q));});}catch(const std::exception&){refused=true;}
    Require(refused&&failed.poisoned&&failed.world.smoothCount==2u&&failed.world.events.size()==1u&&failed.world.markDepth==0u&&Same(failed.world.actors[source].location,Vec{1,2,3}),
        "Late Level failure hid coupled side effects or stranded locally guarded source pose/mark");
    bool rejected{};try{failed.Commit();}catch(const std::exception&){rejected=true;}Require(rejected,"Mock caller published a poisoned Level root");
    const auto work=failed.world.work;failed.Rollback();Require(Same(failed.world,failed.before)&&failed.world.work==work,
        "Mock caller failed full state rollback or rewound shared work");++mockRoots;
    auto veto=Moving();veto.heads={1u,0u};veto.hits[1u]={3u,2u};veto.hits[2u]={4u,0u};veto.actors[4u].brush=veto.actors[4u].brushClass=true;veto.veto=1;
    Root ordinary(veto);auto v=Providers(ordinary.world);bool result{};ordinary.Call([&]{result=QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,0,v);});ordinary.Commit();
    Require(result&&!ordinary.poisoned&&ordinary.world.smoothCount==1u&&Same(ordinary.world.actors[3u].location,Vec{11,22,33})&&ordinary.world.events.size()==1u,
        "Ordinary veto incorrectly rolled back a successfully escaped earlier candidate");++mockRoots;
    Root shared(World{});auto s=Providers(shared.world);shared.Call([&]{static_cast<void>(QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,0,s));});
    shared.world.limit=shared.world.work;const auto used=shared.world.work;refused=false;
    try{shared.Call([&]{static_cast<void>(QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,0,s));});}catch(const std::exception&){refused=true;}
    Require(refused&&shared.poisoned&&shared.world.work==used+1u&&shared.world.queryCount==1u,
        "Second higher Level operation reset caller allowance or reached a denied hash query");shared.Rollback();
    Require(Same(shared.world,shared.before),"Later work refusal did not permit caller rollback of successful earlier operation");++mockRoots;
}

} // namespace

int main(){try{StaticControlFlow();CallbackControlFlow();FailureAndSafety();NumericAndLayouts();NestedMarks();CallerOwnedRoot();
    std::cout<<"PASS original Level encroachment "<<checks<<" checks, "<<cases<<" independent protocol cases, "<<staticCases
        <<" static/numeric/layout cohorts, "<<mutationCases<<" live callback mutations, "<<failurePrefixes<<" provider/work prefixes, "
        <<refusals<<" explicit refusals, "<<mockRoots<<" caller-owned mock roots. No runtime MoveActor or map-physics acceptance.\n";return 0;
    }catch(const std::exception& e){std::cerr<<"Original Level encroachment failed: "<<e.what()<<'\n';return 1;}}
