#include "quest_level_move_actor.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace {
using Id=std::uintptr_t;using Vec=PortableModelVec3;using Rot=std::array<std::int32_t,3u>;
using Hit=QuestVr::OriginalActorLineHit;using Sweep=QuestVr::OriginalMoveActorSweep;using Class=QuestVr::OriginalMoveActorClass;
using Q=QuestVr::OriginalLevelMoveActorQueries;
constexpr Id Level=100,Source=1,Info=8;
std::size_t checks{},refusals{},cases{},prefixes{},reentrant{},readOrders{};
void Require(bool x,const char* why){++checks;if(!x)throw std::runtime_error(why);}
std::uint32_t Bits(float x){return std::bit_cast<std::uint32_t>(x);}
bool Same(Vec a,Vec b){return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
bool Same(const Hit& a,const Hit& b){return a.actor==b.actor&&a.next==b.next&&a.primitive==b.primitive&&a.item==b.item&&Bits(a.time)==Bits(b.time)&&Same(a.location,b.location)&&Same(a.normal,b.normal);}
enum class Op{Flags,Editor,Static,Brush,Class,Collision,Standing,Rotation,WriteRotation,Location,WriteLocation,Radius,Height,
    First,Info,Mark,Pop,Sweep,HitActor,Next,Time,Packet,Base,Blocked,Overlap,Carry,Encroach,Hash,Remove,Add,Wall,Bump,Touching,Begin,End,Zone};
struct Trace{Op op;Id a{},b{};std::uint64_t c{};bool operator==(const Trace&)const=default;};
struct Actor{std::uint32_t flags{0x80000u};std::uint8_t collision{15},standing{};bool brush{},brushClass{true},pawn{},decoration{};
    Vec location{10,20,30};Rot rotation{7,8,9};float radius{4},height{8};std::array<Id,4u> touching{};};
struct Geometry{float size{};Vec direction{},extended{};};
struct DependencyCall{Id level{},actor{};Vec point{};Rot rotation{};std::int32_t flag{};};
struct WallPacket{Id actor{};Vec location{},normal{};};
struct World{
    std::array<Actor,16u> actors{};std::array<Hit,6u> hits{};std::array<bool,16u> blocking{},overlapping{};
    std::array<std::array<bool,16u>,16u> bases{};std::vector<Trace> trace;std::vector<Sweep> sweeps;std::vector<Id> marks;
    std::size_t calls{},work{},failure{},limit{100000},nextMark{1},carryEffects{},zoneCalls{};
    Id first{Info},hash{90},head{1};std::int32_t editor{},veto{};unsigned mutation{};bool oracle{},recurse{},inside{};
    std::vector<DependencyCall> carried,encroachments;
    std::vector<WallPacket> walls;
    std::array<unsigned,4u> sampledCounts{};
    // Stateful getters return their sampled value BEFORE this fixture hook
    // changes the next read. Hooks contain no production arithmetic/control.
    std::function<void(World&,Trace)> afterSample;
    Hit* caller{};
    World(){
        for(unsigned i=1u;i<=4u;++i){hits[i].actor=i+1u;hits[i].next=i==4u?0u:i+1u;hits[i].time=float(i)/5;
            hits[i].location={float(i),float(i+10u),float(i+20u)};hits[i].normal={1,0,0};hits[i].primitive=60u+i;hits[i].item=int(i+40u);}
        blocking[4]=true;actors[2].pawn=true;actors[3].decoration=true;actors[5].brush=true;actors[Info].flags=1;
        for(auto& a:actors)a.touching={};
        actors[Source].touching={6,0,7,0};overlapping[6]=true;
        actors[12].collision=0;actors[12].pawn=true;actors[12].rotation={1,2,3};
    }
    void Record(Trace t){if(++calls==failure)throw std::runtime_error("mock provider refusal");
        if(trace.capacity()<trace.size()+8u)trace.reserve(trace.size()+64u);
        trace.push_back(t);}
    void Charge(){if(++work>limit)throw std::runtime_error("shared root work refused");}
    Actor& A(Id a){if(!a||a>=actors.size())throw std::runtime_error("unsafe mock Actor");return actors[a];}
    Hit& H(Id h){if(!h||h>=hits.size())throw std::runtime_error("unsafe mock hit handle");return hits[h];}
    void CheckLevel(Id l){if(l!=Level)throw std::runtime_error("wrong retained native Level");}
    std::uint32_t Flags(Id a){Record({Op::Flags,a});return A(a).flags;}
    std::int32_t Editor(){Record({Op::Editor});return editor;}
    bool Static(Id a){Record({Op::Static,a});return (A(a).flags&1u)!=0;}
    bool Brush(Id a){Record({Op::Brush,a});return A(a).brush;}
    bool IsA(Id a,Class c){Record({Op::Class,a,0u,static_cast<unsigned>(c)});const auto& v=A(a);
        return c==Class::Brush?v.brushClass:c==Class::Pawn?v.pawn:v.decoration;}
    std::uint8_t Collision(Id a){Record({Op::Collision,a});return A(a).collision;}
    std::uint8_t Standing(Id a){Record({Op::Standing,a});return A(a).standing;}
    std::int32_t Rotation(Id a,unsigned i){Record({Op::Rotation,a,i});return A(a).rotation.at(i);}
    void WriteRotation(Id a,unsigned i,std::int32_t v){Record({Op::WriteRotation,a,i,std::bit_cast<std::uint32_t>(v)});A(a).rotation.at(i)=v;}
    float Location(Id a,unsigned i){const Trace t{Op::Location,a,i};Record(t);auto p=A(a).location;
        const auto value=i==0u?p.x:i==1u?p.y:p.z;if(afterSample)afterSample(*this,t);return value;}
    void WriteLocation(Id a,unsigned i,float v){Record({Op::WriteLocation,a,i,Bits(v)});auto& p=A(a).location;(i==0u?p.x:i==1u?p.y:p.z)=v;}
    float Radius(Id a){const Trace t{Op::Radius,a};Record(t);const auto value=A(a).radius;if(afterSample)afterSample(*this,t);return value;}
    float Height(Id a){const Trace t{Op::Height,a};Record(t);const auto value=A(a).height;if(afterSample)afterSample(*this,t);return value;}
    Id First(Id l){const Trace t{Op::First,l};Record(t);CheckLevel(l);const auto value=first;if(afterSample)afterSample(*this,t);return value;}
    void RequireInfo(Id a){const Trace t{Op::Info,a};Record(t);if(a!=Info)throw std::runtime_error("not native slot0 LevelInfo");if(afterSample)afterSample(*this,t);}
    Id Mark(){Record({Op::Mark});if(marks.capacity()<64u)marks.reserve(64u);const auto token=nextMark++;marks.push_back(token);return token;}
    void Pop(Id token)noexcept{
        // Mandatory cleanup is allocation-free and cannot be failed/metered.
        // Record() always leaves eight trace slots BEFORE the next provider.
        if(marks.empty()||marks.back()!=token||trace.size()==trace.capacity())std::terminate();
        trace.push_back({Op::Pop,token});marks.pop_back();
        if((mutation&128u)&&marks.empty()&&caller)caller->time=-1;
    }
    Id Multi(Sweep s){Record({Op::Sweep,s.level,s.world,static_cast<std::uint32_t>(s.checkActors)});CheckLevel(s.level);
        Require(s.extraFlags==0u,"Outer sweep changed literal byte flags0");sweeps.push_back(s);return head;}
    Id HitActor(Id h){Record({Op::HitActor,h});return H(h).actor;}
    Id Next(Id h){Record({Op::Next,h});return H(h).next;}
    float Time(Id h){Record({Op::Time,h});return H(h).time;}
    Hit Packet(Id h){Record({Op::Packet,h});return H(h);}
    bool Based(Id a,Id b){const Trace t{Op::Base,a,b};Record(t);A(a);if(b>=actors.size())throw std::runtime_error("bad Base target");
        const bool value=b&&(a==b||bases[a][b]);if(afterSample)afterSample(*this,t);return value;}
    bool Blocked(Id a,Id b){Record({Op::Blocked,a,b});A(a);A(b);return blocking[b];}
    bool Overlap(Id a,Id b){Record({Op::Overlap,a,b});A(a);A(b);if(mutation&32u)A(a).touching[2u]=9;return overlapping[b];}
    void Carry(Id l,Id a,Vec delta,Rot r,std::int32_t test);
    std::int32_t Encroach(Id l,Id a,Vec p,Rot r,std::int32_t touch){
        Record({Op::Encroach,l,a,static_cast<std::uint32_t>(touch)});CheckLevel(l);
        Require(touch==0,"MoveActor higher encroachment changed literal touch0");encroachments.push_back({l,a,p,r,touch});return veto;}
    Id Hash(Id l){Record({Op::Hash,l});CheckLevel(l);return hash;}
    void Remove(Id h,Id a){Record({Op::Remove,h,a});if(h!=hash)throw std::runtime_error("wrong current hash");}
    void Add(Id h,Id a){Record({Op::Add,h,a});if(h!=hash)throw std::runtime_error("wrong current hash");}
    void Wall(Id a,Vec location,Vec normal){Record({Op::Wall,a});
        Require(caller&&Same(location,caller->location)&&Same(normal,caller->normal),"BumpWall swapped Location/Normal or changed packet");
        walls.push_back({a,location,normal});}
    void Bump(Id a,Id b){Record({Op::Bump,a,b});A(a);A(b);
        if(caller&&(mutation&2u)&&a!=Source)caller->actor=9;
        if(mutation&4u){actors[Source].collision=0;hits[1].next=4;hits[4].actor=11;}
        if(caller&&(mutation&8u))caller->time=std::numeric_limits<float>::quiet_NaN();
        if(afterSample)afterSample(*this,{Op::Bump,a,b});
    }
    Id Touching(Id a,unsigned i){Record({Op::Touching,a,i});return A(a).touching.at(i);}
    void Begin(Id a,Id b){Record({Op::Begin,a,b});A(a);A(b);if(mutation&16u)H(1).next=0;}
    void End(Id a,Id b,std::int32_t suppress){Record({Op::End,a,b,static_cast<std::uint32_t>(suppress)});A(a);A(b);
        Require(!suppress,"MoveActor EndTouch changed literal suppressSelf0");for(auto& slot:A(a).touching)if(slot==b){slot=0;break;}}
    void Zone(Id l,Id a,std::int32_t test,std::int32_t force){Record({Op::Zone,l,a,std::uint64_t{std::bit_cast<std::uint32_t>(test)}<<32u|std::bit_cast<std::uint32_t>(force)});
        CheckLevel(l);Require(!force,"MoveActor substituted birth force-refresh for movement zone update");++zoneCalls;
        if(caller&&(mutation&64u)&&a==Source)caller->time=0;
    }
    Q Bind(){Q q;
        q.eligibilityFlags=[&](Id a){return Flags(a);};q.editor=[&]{return Editor();};q.staticFlag=[&](Id a){return Static(a);};
        q.hasBrush=[&](Id a){return Brush(a);};q.isA=[&](Id a,Class c){return IsA(a,c);};q.collisionFlags=[&](Id a){return Collision(a);};
        q.standingCount=[&](Id a){return Standing(a);};q.rotation=[&](Id a,unsigned c){return Rotation(a,c);};
        q.writeRotation=[&](Id a,unsigned c,std::int32_t v){WriteRotation(a,c,v);};q.location=[&](Id a,unsigned c){return Location(a,c);};
        q.writeLocation=[&](Id a,unsigned c,float v){WriteLocation(a,c,v);};q.radius=[&](Id a){return Radius(a);};q.height=[&](Id a){return Height(a);};
        q.levelActor0=[&](Id l){return First(l);};q.requireLevelInfo=[&](Id a){RequireInfo(a);};q.mark=[&]{return Mark();};q.popMark=[&](Id t)noexcept{Pop(t);};
        q.multiLine=[&](Sweep s){return Multi(s);};q.hitActor=[&](Id h){return HitActor(h);};q.hitNext=[&](Id h){return Next(h);};
        q.hitTime=[&](Id h){return Time(h);};q.hitPacket=[&](Id h){return Packet(h);};q.basedOn=[&](Id a,Id b){return Based(a,b);};
        q.blockedBy=[&](Id a,Id b){return Blocked(a,b);};q.overlaps=[&](Id a,Id b){return Overlap(a,b);};
        q.carryChildren=[&](Id l,Id a,Vec d,Rot r,std::int32_t t){Carry(l,a,d,r,t);};
        q.encroachment=[&](Id l,Id a,Vec p,Rot r,std::int32_t t){return Encroach(l,a,p,r,t);};q.levelHash=[&](Id l){return Hash(l);};
        q.hashRemove=[&](Id h,Id a){Remove(h,a);};q.hashAdd=[&](Id h,Id a){Add(h,a);};
        q.bumpWall=[&](Id a,Vec l,Vec n){Wall(a,l,n);};q.bump=[&](Id a,Id b){Bump(a,b);};
        q.touching=[&](Id a,unsigned i){return Touching(a,i);};q.beginTouch=[&](Id a,Id b){Begin(a,b);};
        q.endTouch=[&](Id a,Id b,std::int32_t s){End(a,b,s);};q.setZone=[&](Id l,Id a,std::int32_t t,std::int32_t f){Zone(l,a,t,f);};
        q.work=[&]{Charge();};return q;
    }
};

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Oracle {
using Real=long double;
Real Read(float x){if(!std::isfinite(x))throw std::runtime_error("oracle nonfinite arithmetic");return x;}
Real Add(Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,z=x*y;return z;}
Real Div(Real a,Real b){if(b==0)throw std::runtime_error("oracle zero divisor");volatile Real x=a,y=b,z=x/y;return z;}
float Store(Real v){volatile float x=static_cast<float>(v);if(!std::isfinite(x))throw std::runtime_error("oracle Float overflow");return x;}
bool Small(Vec d){const double threshold=std::bit_cast<double>(std::uint64_t{0x3f1a36e2eb1c432dull});
    for(auto x:{d.x,d.y,d.z}){auto v=Read(x);if(v<0)v=-v;if(!(v<threshold))return false;}return true;}
Geometry Prepare(Vec d){Geometry g;if(Small(d))g.direction=d;else{
        const auto zz=Mul(Read(d.z),Read(d.z)),yy=Mul(Read(d.y),Read(d.y)),zy=Add(zz,yy),sum=Add(zy,Mul(Read(d.x),Read(d.x)));
        volatile double stored=static_cast<double>(sum);Real root;asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(root):"m"(stored):"st");
        volatile double ignored=static_cast<double>(root);(void)ignored;g.size=Store(root);const auto inverse=Store(Div(1,root));
        const auto z=Mul(Read(d.z),Read(inverse)),y=Mul(Read(d.y),Read(inverse)),x=Mul(Read(inverse),Read(d.x));
        g.direction={Store(x),Store(y),Store(z)};}
    const auto z=Add(Read(g.direction.z),Read(g.direction.z));const float y=Store(Add(Read(g.direction.y),Read(g.direction.y))),x=Store(Add(Read(g.direction.x),Read(g.direction.x)));
    g.extended.z=Store(Add(Read(d.z),z));g.extended.y=Store(Add(Read(d.y),Read(y)));g.extended.x=Store(Add(Read(x),Read(d.x)));return g;
}
void Backoff(Vec& d,Hit& h,const Geometry& g,std::int32_t noFail){if((!std::isnan(h.time)&&h.time>=1)||noFail)return;
    const auto dist=Mul(Add(2,Read(g.size)),Read(h.time));const float stored=Store(dist);
    if(dist<=2){d={};h.time=0;return;}
    const float bz=Store(Mul(Read(g.direction.z),2)),by=Store(Mul(Read(g.direction.y),2)),bx=Store(Mul(Read(g.direction.x),2));
    const float z=Store(Mul(Read(g.extended.z),Read(h.time))),y=Store(Mul(Read(g.extended.y),Read(h.time))),x=Store(Mul(Read(h.time),Read(g.extended.x)));
    d.z=Store(Sub(Read(z),Read(bz)));d.y=Store(Sub(Read(y),Read(by)));d.x=Store(Sub(Read(x),Read(bx)));h.time=Store(Div(Sub(Read(stored),2),Read(g.size)));
}
template<class F,class... Args>auto Call(World& w,F f,Args... args){w.Charge();return (w.*f)(args...);}
bool Brush(World& w,Id a){return Call(w,&World::Brush,a)&&Call(w,&World::IsA,a,Class::Brush)&&!Call(w,&World::Static,a);}
Id InfoAt(World& w){if(!Call(w,&World::First,Level))throw std::runtime_error("missing slot0");const auto a=Call(w,&World::First,Level);Call(w,&World::RequireInfo,a);return Call(w,&World::First,Level);}
bool Run(World& w,Id source,Vec delta,Rot requested,Hit& hit,std::array<std::int32_t,4u> o,std::size_t maximum){
    if(!source)throw std::runtime_error("null Actor");
    const auto flags=Call(w,&World::Flags,source);
    if(((flags&1u)||!(flags&0x80000u))&&!Call(w,&World::Editor))return false;
    if(Small(delta)){bool equal=true;for(unsigned c=0;c<3;++c)if(Call(w,&World::Rotation,source,c)!=requested[c]){equal=false;break;}
        if(equal)return true;
        if(!Call(w,&World::Standing,source)&&!Brush(w,source)){for(unsigned c=0;c<3;++c)Call(w,&World::WriteRotation,source,c,requested[c]);return true;}}
    hit=Hit{};const auto mark=Call(w,&World::Mark);
    struct Cleanup{World& w;Id id;bool active{true};~Cleanup(){if(active)w.Pop(id);}void Close(){if(active){active=false;w.Pop(id);}}} cleanup{w,mark};
    const auto g=Prepare(delta);Id head{};bool possibleTouch{};std::size_t visits{};
    const auto visit=[&]{if(visits>=maximum)throw std::runtime_error("oracle list bound");++visits;};
    if((Call(w,&World::Collision,source)&3u)&&!Brush(w,source)&&(delta.x!=0||delta.y!=0||delta.z!=0)){
        Sweep s;s.level=Level;if((Call(w,&World::Collision,source)&2u)&&!Brush(w,source))s.world=InfoAt(w);
        if((Call(w,&World::Collision,source)&1u)&&!Brush(w,source))s.checkActors=1;
        const auto h=Call(w,&World::Height,source),ry=Call(w,&World::Radius,source),rx=Call(w,&World::Radius,source);
        s.extent.x=rx;s.extent.y=Store(Read(ry));s.extent.z=Store(Read(h));
        const auto z=Call(w,&World::Location,source,2u);const auto ez=Add(Read(g.extended.z),Read(z));
        const auto y=Call(w,&World::Location,source,1u);const auto ey=Add(Read(g.extended.y),Read(y));
        const auto x=Call(w,&World::Location,source,0u);const auto ex=Add(Read(g.extended.x),Read(x));
        s.start.x=Call(w,&World::Location,source,0u);s.end.x=Store(ex);s.start.y=Call(w,&World::Location,source,1u);s.end.y=Store(ey);
        s.start.z=Call(w,&World::Location,source,2u);s.end.z=Store(ez);head=Call(w,&World::Multi,s);
        if(Call(w,&World::Collision,source)&14u){Id link=head;while(link){visit();bool skip{};
            if(o[1]){const auto a=Call(w,&World::HitActor,link);if(!Call(w,&World::Static,a)){
                skip=Call(w,&World::IsA,a,Class::Pawn);if(!skip){const auto b=Call(w,&World::HitActor,link);skip=Call(w,&World::IsA,b,Class::Decoration);}}}
            if(!skip&&o[2]){const auto a=Call(w,&World::HitActor,link);skip=Call(w,&World::Based,source,a);}
            if(!skip){const auto a=Call(w,&World::HitActor,link);skip=Call(w,&World::Based,a,source);}
            if(!skip){const auto a=Call(w,&World::HitActor,link);possibleTouch=true;if(Call(w,&World::Blocked,source,a)){hit=Call(w,&World::Packet,link);break;}}
            link=Call(w,&World::Next,link);
        }}
    }
    Backoff(delta,hit,g,o[3]);Call(w,&World::Carry,Level,source,delta,requested,o[0]);
    if(!o[0]&&!o[3]&&!Call(w,&World::IsA,source,Class::Pawn)){
        Vec p;const auto z=Call(w,&World::Location,source,2u);p.z=Store(Add(Read(delta.z),Read(z)));
        const auto y=Call(w,&World::Location,source,1u);p.y=Store(Add(Read(delta.y),Read(y)));
        const auto x=Call(w,&World::Location,source,0u);p.x=Store(Add(Read(delta.x),Read(x)));
        if(Call(w,&World::Encroach,Level,source,p,requested,0))return false;
    }
    if(Call(w,&World::Collision,source)&1u){const auto hash=Call(w,&World::Hash,Level);if(hash)Call(w,&World::Remove,hash,source);}
    for(unsigned c=0;c<3;++c){const auto from=Call(w,&World::Location,source,c);const float amount=c==0?delta.x:c==1?delta.y:delta.z;
        const auto next=Store(Add(Read(amount),Read(from)));Call(w,&World::WriteLocation,source,c,next);}
    for(unsigned c=0;c<3;++c)Call(w,&World::WriteRotation,source,c,requested[c]);
    if(Call(w,&World::Collision,source)&1u){const auto hash=Call(w,&World::Hash,Level);if(hash)Call(w,&World::Add,hash,source);}
    if(!o[0]){
        if(hit.actor){const auto info=InfoAt(w);if(hit.actor==info)Call(w,&World::Wall,source,hit.location,hit.normal);
            else if(!Call(w,&World::Based,source,hit.actor)){const auto peer=hit.actor;Call(w,&World::Bump,peer,source);Call(w,&World::Bump,source,hit.actor);}}
        bool touches=possibleTouch;if(!touches){const auto f=Call(w,&World::Collision,source);touches=(f&12u)!=12u;}
        if(touches){Id link=head;while(link){visit();const auto t=Call(w,&World::Time,link);
            if(!std::isnan(t)&&!std::isnan(hit.time)&&!(t<hit.time))break;
            const auto a=Call(w,&World::HitActor,link);bool skip=Call(w,&World::Based,a,source);
            if(!skip&&o[2]){const auto b=Call(w,&World::HitActor,link);skip=Call(w,&World::Based,source,b);}
            if(!skip){const auto b=Call(w,&World::HitActor,link);if(!Call(w,&World::Blocked,source,b)){const auto c=Call(w,&World::HitActor,link);Call(w,&World::Begin,source,c);}}
            link=Call(w,&World::Next,link);
        }}
        for(unsigned i=0;i<4;++i){const auto a=Call(w,&World::Touching,source,i);if(a&&!Call(w,&World::Overlap,source,a)){
            const auto b=Call(w,&World::Touching,source,i);Call(w,&World::End,source,b,0);}}
    }
    Call(w,&World::Zone,Level,source,o[0],0);cleanup.Close();return !std::isnan(hit.time)&&hit.time>0;
}
} // namespace Oracle
#endif

void World::Carry(Id l,Id a,Vec delta,Rot r,std::int32_t test){
    Record({Op::Carry,l,a,static_cast<std::uint32_t>(test)});CheckLevel(l);carried.push_back({l,a,delta,r,test});
    if(test)return;
    ++carryEffects;
    if((mutation&1u)&&a==Source){actors[Source].location={50,60,70};actors[Source].rotation={100,200,300};}
    if(recurse&&a==Source&&!inside){inside=true;auto* outer=caller;Hit inner;caller=&inner;
        try{
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
            if(oracle)static_cast<void>(Oracle::Run(*this,12,{1,2,3},{4,5,6},inner,std::array<std::int32_t,4u>{},65536));else
#endif
            static_cast<void>(QuestVr::MoveOriginalLevelActor(Level,12,{1,2,3},{4,5,6},inner,{},Bind()));
        }catch(...){caller=outer;inside=false;throw;}caller=outer;inside=false;
    }
}
template<class F>void Refuse(F run){bool failed{};try{run();}catch(const std::exception&){failed=true;}Require(failed,"Missing/unsafe MoveActor provider succeeded");++refusals;}
Hit Seed(){Hit h;h.actor=14;h.next=999;h.primitive=777;h.item=123;h.time=-INFINITY;h.location={NAN,-0.0f,INFINITY};h.normal={-INFINITY,NAN,0};return h;}
void Controls(){
    World w;Hit h=Seed();const auto before=h;Q q;q.eligibilityFlags=w.Bind().eligibilityFlags;q.editor=w.Bind().editor;w.actors[Source].flags=1;
    Require(!QuestVr::MoveOriginalLevelActor(0,Source,{NAN,INFINITY,NAN},{},h,{},q)&&Same(h,before),"Ineligible branch inspected inactive Level/Delta/Hit/mark providers");
    w=World{};h=Seed();q=w.Bind();q.mark={};q.rotation=[&](Id,unsigned i){return w.actors[Source].rotation[i];};
    Require(QuestVr::MoveOriginalLevelActor(0,Source,{},w.actors[Source].rotation,h,{},q)&&Same(h,before),"Small equal Rotation changed Hit or acquired a mark");
    w=World{};h=Seed();q=w.Bind();q.mark={};auto changed=w.actors[Source].rotation;++changed[1u];const auto original=w.actors[Source].location;
    Require(QuestVr::MoveOriginalLevelActor(0,Source,{1.0e-5f,-0.0f,0},changed,h,{},q)&&Same(h,before)&&
        w.actors[Source].rotation==changed&&Same(w.actors[Source].location,original),
        "Genuine small Rotation-only exit changed Hit/Location or required inactive Level/mark providers");
    Refuse([&]{QuestVr::MoveOriginalLevelActor(Level,0,{},Rot{},h,{},Q{});});
    w=World{};h=Seed();q=w.Bind();q.popMark={};Refuse([&]{QuestVr::MoveOriginalLevelActor(Level,Source,{3,4,0},{},h,{},q);});
    Require(w.marks.empty()&&std::none_of(w.trace.begin(),w.trace.end(),[](auto t){return t.op==Op::Mark;}),"Move acquired a mark without guaranteed cleanup");
    Require(QuestVr::LevelMoveActorDetail::BelowOrUnordered(NAN,1)&&QuestVr::LevelMoveActorDetail::BelowOrUnordered(1,NAN)&&
        !QuestVr::LevelMoveActorDetail::BelowOrUnordered(1,1)&&!QuestVr::LevelMoveActorDetail::PositiveOrdered(NAN),
        "Raw x87 C0/positive ordered comparison policy differs");
}
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
bool Same(const Actor& a,const Actor& b){return a.flags==b.flags&&a.collision==b.collision&&a.standing==b.standing&&a.brush==b.brush&&
    a.brushClass==b.brushClass&&a.pawn==b.pawn&&a.decoration==b.decoration&&Same(a.location,b.location)&&a.rotation==b.rotation&&Bits(a.radius)==Bits(b.radius)&&
    Bits(a.height)==Bits(b.height)&&a.touching==b.touching;}
struct Outcome{World world;Hit hit;bool returned{},failed{};};
Outcome Compare(World input,Vec delta={3,4,0},Rot requested={7,8,9},std::array<std::int32_t,4u> options={},std::size_t maximum=65536){
    Outcome actual{input,Seed()},expected{input,Seed()};expected.world.oracle=true;actual.world.caller=&actual.hit;expected.world.caller=&expected.hit;
    try{actual.returned=QuestVr::MoveOriginalLevelActor(Level,Source,delta,requested,actual.hit,options,actual.world.Bind(),maximum);}catch(const std::exception&){actual.failed=true;}
    try{expected.returned=Oracle::Run(expected.world,Source,delta,requested,expected.hit,options,maximum);}catch(const std::exception&){expected.failed=true;}
    Require(actual.failed==expected.failed&&actual.returned==expected.returned,"MoveActor result/refusal differs from independent interpreter");
    Require(actual.world.trace==expected.world.trace&&actual.world.calls==expected.world.calls&&actual.world.work==expected.world.work,
        "MoveActor live provider/event/hash/cleanup trace differs from independent interpreter");
    Require(Same(actual.hit,expected.hit),"MoveActor live caller Hit differs from independent hardware arithmetic/protocol");
    Require(actual.world.marks.empty()&&expected.world.marks.empty()&&actual.world.nextMark==expected.world.nextMark&&
        actual.world.carryEffects==expected.world.carryEffects&&actual.world.zoneCalls==expected.world.zoneCalls,"MoveActor mark/shared-root effects differ");
    for(unsigned i=0;i<actual.world.actors.size();++i)Require(Same(actual.world.actors[i],expected.world.actors[i]),"MoveActor partial actor/pose/touch writes differ");
    Require(actual.world.sweeps.size()==expected.world.sweeps.size(),"MoveActor sweep count differs");
    for(std::size_t i=0;i<actual.world.sweeps.size();++i){const auto& a=actual.world.sweeps[i];const auto& b=expected.world.sweeps[i];
        Require(a.level==b.level&&a.world==b.world&&a.checkActors==b.checkActors&&a.extraFlags==b.extraFlags&&Same(a.end,b.end)&&Same(a.start,b.start)&&Same(a.extent,b.extent),
            "MoveActor sweep packet differs from independent retained/store calculation");}
    for(unsigned i=0;i<actual.world.hits.size();++i)Require(Same(actual.world.hits[i],expected.world.hits[i]),"Live hit-list callback changes differ");
    const auto packets=[&](const auto& a,const auto& b){Require(a.size()==b.size(),"Movement dependency call count differs");
        for(std::size_t i=0;i<a.size();++i)Require(a[i].level==b[i].level&&a[i].actor==b[i].actor&&Same(a[i].point,b[i].point)&&
            a[i].rotation==b[i].rotation&&a[i].flag==b[i].flag,"Movement child/encroachment by-value packet differs");};
    packets(actual.world.carried,expected.world.carried);packets(actual.world.encroachments,expected.world.encroachments);
    Require(actual.world.sampledCounts==expected.world.sampledCounts&&actual.world.walls.size()==expected.world.walls.size(),
        "State-changing read counters or BumpWall packet count differ");
    for(std::size_t i=0u;i<actual.world.walls.size();++i)Require(actual.world.walls[i].actor==expected.world.walls[i].actor&&
        Same(actual.world.walls[i].location,expected.world.walls[i].location)&&Same(actual.world.walls[i].normal,expected.world.walls[i].normal),
        "BumpWall argument words differ after state-changing reads");
    ++cases;if(actual.failed)++refusals;actual.world.caller=nullptr;return actual;
}
void Cohorts(){
    struct Precision{unsigned short previous{};Precision(){static_assert(std::numeric_limits<long double>::digits==64);asm volatile("fnstcw %0":"=m"(previous));
        const auto mode=static_cast<unsigned short>((previous&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}~Precision(){asm volatile("fldcw %0"::"m"(previous));}} precision;
    for(auto flags:{0u,1u,0x80000u,0x80001u})for(int editor:{0,-1})for(std::uint8_t collide:{0u,1u,2u,3u,4u,8u,12u,15u})
        for(bool brush:{false,true})for(bool pawn:{false,true}){World w;w.actors[Source].flags=flags;w.editor=editor;w.actors[Source].collision=collide;
            w.actors[Source].brush=brush;w.actors[Source].pawn=pawn;Compare(w);}
    for(unsigned axis=0;axis<3;++axis)for(std::uint8_t standing:{0u,1u,255u})for(bool brush:{false,true}){
        World w;w.actors[Source].standing=standing;w.actors[Source].brush=brush;Rot r=w.actors[Source].rotation;++r[axis];Compare(w,{1.0e-5f,-0.0f,0},r);}
    for(unsigned mask=0;mask<16;++mask)for(std::int32_t truth:{1,-1,std::numeric_limits<std::int32_t>::min()}){
        World w;std::array<std::int32_t,4u> o{};for(unsigned i=0;i<4;++i)if(mask&(1u<<i))o[i]=truth;Compare(w,{3,4,0},{7,8,9},o);}
    for(unsigned mutation=0;mutation<256;++mutation){World w;w.mutation=mutation;Compare(w);}
    for(auto veto:{1,-1,std::numeric_limits<std::int32_t>::min()}){World w;w.veto=veto;w.mutation=1;const auto result=Compare(w);
        Require(!result.failed&&!result.returned&&result.world.carryEffects==1&&Same(result.world.actors[Source].location,{50,60,70})&&result.world.zoneCalls==0,
            "Ordinary encroachment veto undid prior child effects or ran parent/zone publication");}
    for(float time:{-INFINITY,-1.0f,-0.0f,0.0f,.01f,.5f,1.0f,INFINITY,NAN})for(int noFail:{0,1}){
        World w;w.hits[3].time=time;Compare(w,{3,4,0},{7,8,9},{0,0,0,noFail});}
    World wall;wall.hits[3].actor=Info;wall.blocking[4]=false;wall.blocking[Info]=true;Compare(wall);
    World notBrush;notBrush.actors[Source].brush=true;notBrush.actors[Source].brushClass=false;
    Require(!Compare(notBrush).world.sweeps.empty(),"A nonnull Brush on a non-Brush actor incorrectly suppressed its sweep");
    const auto testing=Compare(World{},{3,4,0},{100,200,300},{-7,0,0,0});
    Require(!testing.failed&&testing.world.zoneCalls==1u&&testing.world.actors[Source].rotation==Rot{100,200,300}&&
        std::count_if(testing.world.trace.begin(),testing.world.trace.end(),[](auto t){return t.op==Op::WriteLocation;})==3&&
        std::count_if(testing.world.trace.begin(),testing.world.trace.end(),[](auto t){return t.op==Op::Remove||t.op==Op::Add;})==2&&
        std::none_of(testing.world.trace.begin(),testing.world.trace.end(),[](auto t){return t.op==Op::Bump||t.op==Op::Wall||t.op==Op::Begin||t.op==Op::End;}),
        "Native bTest became a read-only preview or failed to suppress only its original event/touch work");
    World absent;absent.hash=0;absent.head=0;const auto unhashed=Compare(absent);
    Require(!unhashed.failed&&unhashed.world.zoneCalls==1&&std::none_of(unhashed.world.trace.begin(),unhashed.world.trace.end(),
        [](auto t){return t.op==Op::Add||t.op==Op::Remove;}),"Absent native hash prevented pose/zone work or invoked hash mutation");
    for(Id first:{Id{},Id{9}}){World w;w.first=first;Compare(w);}
    World based;based.bases[Source][4]=true;based.bases[3][Source]=true;Compare(based,{3,4,0},{7,8,9},{0,1,1,0});
    World nonblocking;nonblocking.actors[Source].collision=1;std::fill(nonblocking.blocking.begin(),nonblocking.blocking.end(),false);
    nonblocking.hits[1].time=1;const auto stop=Compare(nonblocking);
    Require(std::none_of(stop.world.trace.begin(),stop.world.trace.end(),[](auto t){return t.op==Op::Next||t.op==Op::Begin;}),"Touch equality did not terminate the whole query list");
    nonblocking.hits[1].time=NAN;Compare(nonblocking);
    World unordered;unordered.mutation=8;const auto nanAfterBump=Compare(unordered);
    Require(!nanAfterBump.failed&&!nanAfterBump.returned&&std::count_if(nanAfterBump.world.trace.begin(),nanAfterBump.world.trace.end(),
        [](auto t){return t.op==Op::Time;})==4,"Unordered callback-mutated H.Time did not continue the native C0 touch scan then return false");
    World released;released.mutation=128;const auto finalRead=Compare(released);
    Require(!finalRead.failed&&!finalRead.returned&&Bits(finalRead.hit.time)==Bits(-1.0f),"Final Hit.Time was captured before mark release");
    World cycle;cycle.blocking.fill(false);cycle.hits[4].next=1;Compare(cycle,{3,4,0},{7,8,9},{},10);
    for(std::size_t cap:{0u,1u,2u,3u,4u,5u})Compare(World{},{3,4,0},{7,8,9},{},cap);
    for(float bad:{NAN,INFINITY,-INFINITY})for(unsigned axis=0;axis<3;++axis){World w;Vec d{3,4,5};(axis==0?d.x:axis==1?d.y:d.z)=bad;Compare(w,d);
        w.actors[Source].location={bad,bad,bad};Compare(w);}
    const auto complete=Compare(World{});
    for(std::size_t i=1;i<=complete.world.calls;++i){World w;w.failure=i;Compare(w);++prefixes;}
    for(std::size_t i=0;i<complete.world.work;++i){World w;w.limit=i;Compare(w);++prefixes;}
    World nested;nested.recurse=true;nested.mutation=3;const auto done=Compare(nested);++reentrant;
    Require(done.world.nextMark==3u,"Nested full MoveActor did not acquire/release both real detached marks");
    nested.limit=done.world.work/2u;const auto failed=Compare(nested);++reentrant;
    Require(failed.failed&&failed.world.work==nested.limit+1u,"Nested MoveActor reset its enclosing work allowance");
    std::size_t markCount{},secondMark{};
    for(std::size_t i=0;i<done.world.trace.size();++i)if(done.world.trace[i].op==Op::Mark&&++markCount==2u){secondMark=i;break;}
    Require(markCount==2u,"Nested trace lacks its second mark");
    World nestedPrefix;nestedPrefix.recurse=true;nestedPrefix.limit=secondMark+2u;const auto nestedFailure=Compare(nestedPrefix);++reentrant;
    Require(nestedFailure.failed&&nestedFailure.world.nextMark==3u&&std::count_if(nestedFailure.world.trace.begin(),nestedFailure.world.trace.end(),
        [](auto t){return t.op==Op::Pop;})==2,"Failure inside the nested root did not clean both marks in LIFO order");
    // Explicit MOCK enclosing transaction: unsupported work restores all of
    // its actor/Hit state, unlike the ordinary native false/veto case above.
    World root;root.mutation=1;root.failure=complete.world.calls;const auto actors=root.actors;Hit hit=Seed();const auto seed=hit;
    root.caller=&hit;const auto savedRoot=root;bool refused{};
    try{static_cast<void>(QuestVr::MoveOriginalLevelActor(Level,Source,{3,4,0},{7,8,9},hit,{},root.Bind()));}
    catch(const std::exception&){refused=true;root=savedRoot;hit=seed;}
    Require(refused&&Same(hit,seed)&&root.marks.empty()&&!root.carryEffects&&!root.zoneCalls,
        "Mock outer rollback did not restore caller Hit/mark/child-effect state");
    for(unsigned i=0;i<actors.size();++i)Require(Same(root.actors[i],actors[i]),"Mock outer rollback retained actor/pose/touch state");
    std::uint64_t rng=0xa8b3914edc76025full;
    const auto next=[&]{rng^=rng<<13u;rng^=rng>>7u;rng^=rng<<17u;return rng;};
    const auto scalar=[&]{return float(static_cast<int>(next()%65537u)-32768)/1024.0f;};
    for(unsigned i=0;i<3000u;++i){World w;w.actors[Source].location={scalar(),scalar(),scalar()};
        w.actors[Source].collision=static_cast<std::uint8_t>(next()%16u);w.actors[Source].pawn=(next()&1u)!=0;
        w.hits[3].time=float(next()%150001u)/100000.0f-.25f;w.mutation=static_cast<unsigned>(next()%128u);
        Compare(w,{scalar(),scalar(),scalar()},{std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(next())),
            std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(next())),std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(next()))},
            {0,static_cast<std::int32_t>(next()%2u),static_cast<std::int32_t>(next()%2u),static_cast<std::int32_t>(next()%2u)});
    }
}
#else
void Cohorts(){std::cout<<"SKIP independent MoveActor hardware/interpreter oracle on non-x86 compiler; deterministic controls run.\n";}
#endif
struct DirectOutcome{World world;Hit hit;bool returned{},failed{};};
DirectOutcome Direct(World input,Vec delta={3,0,0},std::array<std::int32_t,4u> options={1,0,0,1}){
    DirectOutcome out{std::move(input),Seed()};out.world.caller=&out.hit;
    try{out.returned=QuestVr::MoveOriginalLevelActor(Level,Source,delta,{7,8,9},out.hit,options,out.world.Bind());}
    catch(const std::exception&){out.failed=true;}
    out.world.caller=nullptr;return out;
}
std::vector<Trace> Select(const World& w,std::initializer_list<Op> operations){
    std::vector<Trace> selected;for(const auto& t:w.trace)if(std::find(operations.begin(),operations.end(),t.op)!=operations.end())selected.push_back(t);
    return selected;
}
void ReadOrderControls(){
    // These literal packets/traces are separate semantic assertions, not a
    // second copy of the implementation or a call to its numeric helpers.
    // Exact Delta(3,0,0) gives extended(5,0,0) without a rounding ambiguity.
    World slot0;slot0.head=0;slot0.first=9;
    slot0.afterSample=[](World& w,Trace t){if(t.op==Op::First){const auto n=++w.sampledCounts[0u];
        if(n==1u)w.first=Info;else if(n==2u)w.first=10;}};
    const auto sweepInfo=Direct(slot0);
    Require(!sweepInfo.failed&&sweepInfo.returned&&sweepInfo.world.sweeps.size()==1u&&sweepInfo.world.sweeps[0u].world==10u,
        "Sweep used cached first/validated slot0 instead of the THIRD raw slot0 return");
    Require(Select(sweepInfo.world,{Op::First,Op::Info})==std::vector<Trace>{{Op::First,Level},{Op::First,Level},{Op::Info,Info},{Op::First,Level}},
        "Sweep collapsed separately loaded nonnull/asserted/authenticated/returned Actors[0] reads");
    Require(sweepInfo.world.sampledCounts[0u]==3u,"Sweep overread or skipped a live Actors[0] getter");++readOrders;
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    Compare(slot0,{3,0,0},{7,8,9},{1,0,0,1});
#endif
    World badValidation;badValidation.first=9;
    badValidation.afterSample=[](World& w,Trace t){if(t.op==Op::First&&++w.sampledCounts[0u]==1u)w.first=0u;};
    const auto secondNil=Direct(badValidation);
    Require(secondNil.failed&&secondNil.world.sweeps.empty()&&secondNil.world.marks.empty()&&
        Select(secondNil.world,{Op::First,Op::Info})==std::vector<Trace>{{Op::First,Level},{Op::First,Level},{Op::Info,0u}},
        "Nonnull initial slot0 hid a separately reloaded null validation pointer or used the first value");++readOrders;
    World firstNil;firstNil.first=0u;const auto nil=Direct(firstNil);
    Require(nil.failed&&Select(nil.world,{Op::First,Op::Info})==std::vector<Trace>{{Op::First,Level}},
        "Null initial slot0 invoked inactive class-validation/return getters");++readOrders;
    World wall;wall.actors[Source].collision=5u;wall.first=9;
    wall.afterSample=[](World& w,Trace t){
        if(t.op==Op::First){const auto n=++w.sampledCounts[0u];if(n==1u)w.first=Info;else if(n==2u)w.first=10u;}
        if(t.op==Op::Info){Require(w.caller&&w.caller->actor==4u,"World-hit fixture did not begin validation with selected H.Actor4");w.caller->actor=10u;}
    };
    const auto liveWall=Direct(wall,{3,0,0},{0,0,0,1});
    Require(!liveWall.failed&&liveWall.returned&&liveWall.hit.actor==10u&&liveWall.world.sweeps.size()==1u&&liveWall.world.sweeps[0u].world==0u,
        "Actor-only sweep unexpectedly validated world before the Bump branch or lost callback-mutated H.Actor");
    Require(liveWall.world.walls.size()==1u&&liveWall.world.walls[0u].actor==Source&&
        Same(liveWall.world.walls[0u].location,Vec{3,13,23})&&Same(liveWall.world.walls[0u].normal,Vec{1,0,0})&&
        Select(liveWall.world,{Op::First,Op::Info,Op::Wall,Op::Bump})==std::vector<Trace>{{Op::First,Level},{Op::First,Level},{Op::Info,Info},{Op::First,Level},{Op::Wall,Source}},
        "Bump branch cached entry H.Actor/validated slot0, reversed BumpWall vectors, or sent actor Bump instead");++readOrders;
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    Compare(wall,{3,0,0},{7,8,9},{0,0,0,1});
#endif
    World bump;bump.actors[Source].collision=5u;
    bump.afterSample=[](World& w,Trace t){
        if(t.op==Op::Base&&t.a==Source&&t.b==4u){Require(w.caller!=nullptr,"Missing live caller Hit");w.caller->actor=9u;}
        if(t.op==Op::Bump&&t.a==9u&&t.b==Source)w.caller->actor=10u;
    };
    const auto liveBump=Direct(bump,{3,0,0},{0,0,0,1});
    Require(!liveBump.failed&&liveBump.returned&&liveBump.hit.actor==10u&&liveBump.world.walls.empty()&&
        Select(liveBump.world,{Op::Bump})==std::vector<Trace>{{Op::Bump,9u,Source},{Op::Bump,Source,10u}},
        "Bump receiver cached H.Actor before IsBasedOn or second Bump cached H.Actor before first event");
    const auto baseCall=std::find(liveBump.world.trace.begin(),liveBump.world.trace.end(),Trace{Op::Base,Source,4u});
    const auto bumpCall=std::find(liveBump.world.trace.begin(),liveBump.world.trace.end(),Trace{Op::Bump,9u,Source});
    Require(baseCall!=liveBump.world.trace.end()&&bumpCall!=liveBump.world.trace.end()&&baseCall<bumpCall,
        "Distinct tested Base peer and later live first-Bump receiver were not observable");++readOrders;
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    Compare(bump,{3,0,0},{7,8,9},{0,0,0,1});
#endif
    World dimensions;dimensions.head=0u;
    dimensions.afterSample=[](World& w,Trace t){
        if(t.op==Op::Height){w.actors[Source].height=99;w.actors[Source].radius=13;}
        if(t.op==Op::Radius){const auto n=++w.sampledCounts[1u];w.actors[Source].radius=n==1u?17.0f:99.0f;}
        if(t.op==Op::Radius&&w.sampledCounts[1u]==2u)w.actors[Source].location={100,200,300};
        if(t.op==Op::Location){const auto n=++w.sampledCounts[2u];
            if(n==1u)w.actors[Source].location={110,220,330};else if(n==2u)w.actors[Source].location={111,222,333};
            else if(n==3u)w.actors[Source].location={1001,2002,3003};else if(n==4u)w.actors[Source].location={1004,2005,3006};
            else if(n==5u)w.actors[Source].location={1007,2008,3009};}
    };
    const auto distinct=Direct(dimensions);
    Require(!distinct.failed&&distinct.returned&&distinct.world.sweeps.size()==1u&&
        Same(distinct.world.sweeps[0u].extent,Vec{17,13,8})&&Same(distinct.world.sweeps[0u].end,Vec{116,220,300})&&
        Same(distinct.world.sweeps[0u].start,Vec{1001,2005,3009}),
        "Sweep collapsed height/radiusY/radiusX reads, endpoint ZYX reads, or independent raw start XYZ reads");
    auto inputs=Select(distinct.world,{Op::Height,Op::Radius,Op::Location});
    const std::vector<Trace> expected{{Op::Height,Source},{Op::Radius,Source},{Op::Radius,Source},
        {Op::Location,Source,2u},{Op::Location,Source,1u},{Op::Location,Source,0u},
        {Op::Location,Source,0u},{Op::Location,Source,1u},{Op::Location,Source,2u}};
    Require(inputs.size()>=expected.size()&&std::equal(expected.begin(),expected.end(),inputs.begin())&&distinct.world.sampledCounts[1u]==2u,
        "Sweep's exact nine-input getter transcript differs from literal source order");++readOrders;
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    Compare(dimensions,{3,0,0},{7,8,9},{1,0,0,1});
#endif
    const float payload=std::bit_cast<float>(0x7fc13579u);
    World raw;raw.head=0u;raw.actors[Source].radius=13;
    raw.afterSample=[=](World& w,Trace t){
        if(t.op==Op::Radius&&++w.sampledCounts[1u]==1u)w.actors[Source].radius=payload;
        if(t.op==Op::Location){const auto n=++w.sampledCounts[2u];
            if(n==3u)w.actors[Source].location={payload,20,30};
            else if(n==4u)w.actors[Source].location.y=-0.0f;
            else if(n==5u)w.actors[Source].location.z=std::numeric_limits<float>::infinity();
            else if(n==6u)w.actors[Source].location={2,3,4};}
    };
    const auto rawWords=Direct(raw);
    Require(!rawWords.failed&&rawWords.returned&&rawWords.world.sweeps.size()==1u&&
        Same(rawWords.world.sweeps[0u].extent,Vec{payload,13,8})&&Same(rawWords.world.sweeps[0u].end,Vec{15,20,30})&&
        Same(rawWords.world.sweeps[0u].start,Vec{payload,-0.0f,std::numeric_limits<float>::infinity()}),
        "Raw second radius/start words were cached, normalized, eagerly rejected or confused with active endpoint arithmetic");++readOrders;
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    Compare(raw,{3,0,0},{7,8,9},{1,0,0,1});
#endif
}
} // namespace
int main(){try{Controls();Cohorts();ReadOrderControls();std::cout<<"PASS complete ordinary MoveActor coordinator "<<checks<<" checks / "<<refusals<<" refusals, "
    <<cases<<" independent provider/hardware cases, "<<prefixes<<" provider/work failure prefixes and "<<reentrant
    <<" nested controls, "<<readOrders<<" literal state-changing read-order controls. Lower world/hash/movement services are MOCK: not native266/startup/crate physics.\n";return 0;
}catch(const std::exception& error){std::cerr<<"Original MoveActor coordinator failed: "<<error.what()<<'\n';return 1;}}
