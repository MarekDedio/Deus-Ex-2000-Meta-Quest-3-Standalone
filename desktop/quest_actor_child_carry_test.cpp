#include "quest_actor_child_carry.h"
#include "quest_original_sine_table.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

namespace {
using Id=std::uintptr_t;using Vec=PortableModelVec3;using Rot=std::array<std::int32_t,3u>;
using Coords=QuestVr::OriginalModelCoords;using Move=QuestVr::OriginalActorChildMove;using Hit=QuestVr::OriginalActorLineHit;
using Q=QuestVr::OriginalActorChildCarryQueries;using Result=QuestVr::OriginalActorCarryResult;
constexpr Id Level=100,Source=1;std::size_t checks{},refusals{},cases{},prefixes{},numeric{},mutated{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
std::uint32_t Bits(float x){return std::bit_cast<std::uint32_t>(x);}
bool Same(Vec a,Vec b){return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
enum class Op{Standing,Count,Slot,Base,Rotation,Location,Math,Unit,Move,Pawn,View,Write};
struct Trace{Op op;Id actor{};unsigned channel{};std::uint64_t value{};bool operator==(const Trace&)const=default;};
struct Actor{Id base{};bool pawn{};Vec location{};Rot rotation{},view{};};
struct World{
    std::array<Actor,16u> actors{};std::vector<Id> slots{0,2,3};std::vector<Trace> trace;std::vector<Move> moves;
    std::uint8_t standing{1};int countOverride{std::numeric_limits<int>::max()};unsigned mutation{};
    std::size_t calls{},work{},failure{},limit{10000};bool oracle{},nested{};Coords unit=QuestVr::IdentityOriginalModelCoords();
    World(){actors[2].base=Source;actors[3].base=Source;actors[2].pawn=true;actors[3].pawn=true;
        actors[Source].location={-0.0f,0,3};actors[2].location={20,30,40};actors[3].location={-30,20,10};
        actors[2].view={7,8,9};actors[3].view={17,18,19};}
    void Record(Trace t){if(++calls==failure)throw std::runtime_error("provider refused");trace.push_back(t);}
    void Charge(){if(++work>limit)throw std::runtime_error("shared work refused");}
    Actor& A(Id id){if(!id||id>=actors.size())throw std::runtime_error("unsafe actor");return actors[id];}
    std::uint8_t Standing(Id a){Record({Op::Standing,a});return standing;}
    std::int32_t Count(Id l){Record({Op::Count,l});if(l!=Level)throw std::runtime_error("wrong Level");
        return countOverride==std::numeric_limits<int>::max()?static_cast<std::int32_t>(slots.size()):countOverride;}
    Id Slot(Id l,std::int32_t i){Record({Op::Slot,l,static_cast<unsigned>(i)});return slots.at(static_cast<std::size_t>(i));}
    Id Base(Id a){Record({Op::Base,a});return A(a).base;}
    std::int32_t Rotation(Id a,unsigned c){Record({Op::Rotation,a,c});return A(a).rotation.at(c);}
    float Location(Id a,unsigned c){Record({Op::Location,a,c});const auto v=A(a).location;return c==0u?v.x:c==1u?v.y:v.z;}
    Id Math(){Record({Op::Math});return 99;}
    Coords Unit(Id root){Record({Op::Unit,root});return unit;}
    bool Pawn(Id a){Record({Op::Pawn,a});return A(a).pawn;}
    std::int32_t View(Id a,unsigned c){Record({Op::View,a,c});return A(a).view.at(c);}
    void Write(Id a,unsigned c,std::int32_t value){Record({Op::Write,a,c,std::bit_cast<std::uint32_t>(value)});A(a).view.at(c)=value;}
    std::int32_t Moving(Move move,Hit& hit);
    Q Bind(){Q q;q.standingCount=[&](Id a){return Standing(a);};q.actorCount=[&](Id l){return Count(l);};
        q.actorSlot=[&](Id l,std::int32_t i){return Slot(l,i);};q.base=[&](Id a){return Base(a);};q.rotation=[&](Id a,unsigned c){return Rotation(a,c);};
        q.location=[&](Id a,unsigned c){return Location(a,c);};q.mathRoot=[&]{return Math();};q.unitCoords=[&](Id r){return Unit(r);};
        q.moveActor=[&](Move m,Hit& h){return Moving(m,h);};q.pawn=[&](Id a){return Pawn(a);};q.viewRotation=[&](Id a,unsigned c){return View(a,c);};
        q.writeViewRotation=[&](Id a,unsigned c,std::int32_t v){Write(a,c,v);};q.work=[&]{Charge();};return q;}
};

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Oracle {
using Real=long double;using Table=std::array<float,16384u>;
Real Add(Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,z=x*y;return z;}
Real Read(float x){if(!std::isfinite(x))throw std::runtime_error("oracle active nonfinite");return x;}
float Store(Real x){volatile float value=static_cast<float>(x);if(!std::isfinite(value))throw std::runtime_error("oracle Float overflow");return value;}
Table table{};
void Initialize(){const double step=std::bit_cast<double>(std::uint64_t{0x3f3921fb54442d18ull});
    for(std::int32_t i=0;i<16384;++i){double angle;float value;
        asm volatile("fildl %1; fmull %2; fstpl %0":"=m"(angle):"m"(i),"m"(step):"st");
        asm volatile("fldl %1; fsin; fstps %0":"=m"(value):"m"(angle):"st");table[static_cast<std::size_t>(i)]=value;
        Require(Bits(value)==Bits(QuestVr::OriginalSineProfile::Table()[static_cast<std::size_t>(i)]),"Local independent hardware table differs from the explicit pinned profile");}}
std::int32_t Wrap(std::int64_t x){x%=0x100000000ll;if(x<0)x+=0x100000000ll;return x>0x7fffffffll?static_cast<std::int32_t>(x-0x100000000ll):static_cast<std::int32_t>(x);}
float Sin(int angle,bool cosine=false){std::int64_t phase=angle;if(cosine)phase+=16384;phase%=65536;if(phase<0)phase+=65536;return table[static_cast<std::size_t>(phase/4)];}
Vec Vector(Vec v,const Coords& m,bool point){
    const auto x=point?Sub(Read(v.x),Read(m.origin.x)):Read(v.x),y=point?Sub(Read(v.y),Read(m.origin.y)):Read(v.y),z=point?Sub(Read(v.z),Read(m.origin.z)):Read(v.z);
    const auto row=[&](Vec a){return Add(Add(Mul(x,Read(a.x)),Mul(y,Read(a.y))),Mul(z,Read(a.z)));};
    const auto a=row(m.xAxis),b=row(m.yAxis),c=row(m.zAxis);return {Store(a),Store(b),Store(c)};
}
void Compose(Coords& m,Coords by){m.origin=Vector(m.origin,by,true);m.xAxis=Vector(m.xAxis,by,false);m.yAxis=Vector(m.yAxis,by,false);m.zAxis=Vector(m.zAxis,by,false);}
Coords Rotate(Coords value,int yaw){const auto s=Sin(yaw),c=Sin(yaw,true);Compose(value,{{},{c,s,0},{-s,c,0},{0,0,1}});
    const auto sp=Sin(0),cp=Sin(0,true);Compose(value,{{},{cp,0,sp},{0,1,0},{-sp,0,cp}});
    const auto sr=Sin(0),cr=Sin(0,true);Compose(value,{{},{1,0,0},{0,cr,-sr},{0,sr,cr}});return value;}
Vec Rotational(Vec offset,const Coords& m){const auto v=Vector(offset,m,false);const auto z=Sub(Read(offset.z),Read(v.z)),y=Sub(Read(offset.y),Read(v.y)),x=Sub(Read(offset.x),Read(v.x));
    return {Store(x),Store(y),Store(z)};}
template<class F,class... Args>auto Call(World& w,F f,Args... args){w.Charge();return (w.*f)(args...);}
Result Run(World& w,Id source,Vec actual,Rot requested,std::int32_t test,std::size_t max){
    if(!source)throw std::runtime_error("null source");
    Result result;
    if(!Call(w,&World::Standing,source))return result;
    if(test){result.flow=QuestVr::OriginalActorCarryFlow::TestBypass;return result;}
    result.flow=QuestVr::OriginalActorCarryFlow::Scanned;
    std::int32_t index{};
    while(true){const auto count=Call(w,&World::Count,Level);if(index>=count)return result;
        if(result.slotVisits==max)throw std::runtime_error("oracle slot budget");
        ++result.slotVisits;
        const auto child=Call(w,&World::Slot,Level,index);++index;if(!child)continue;
        if(Call(w,&World::Base,child)!=source)continue;
        const auto yaw=Call(w,&World::Rotation,source,1u),pitch=Call(w,&World::Rotation,source,0u);
        const auto raw=Wrap(std::int64_t{requested[1u]}-yaw);Vec rotational{};bool rotate=requested[0u]!=pitch||requested[1u]!=yaw;
        if(!rotate)rotate=requested[2u]!=Call(w,&World::Rotation,source,2u);
        if(rotate){
            const auto sz=Call(w,&World::Location,source,2u),cz=Call(w,&World::Location,child,2u);const auto z=Sub(Read(sz),Read(cz));
            const auto sy=Call(w,&World::Location,source,1u),cy=Call(w,&World::Location,child,1u);const auto y=Sub(Read(sy),Read(cy));
            const auto sx=Call(w,&World::Location,source,0u),cx=Call(w,&World::Location,child,0u);const auto x=Sub(Read(sx),Read(cx));
            const auto root=Call(w,&World::Math);const Vec offset{Store(x),Store(y),Store(z)};if(!root)throw std::runtime_error("null math");
            const auto unit=Call(w,&World::Unit,root);
            const auto quantized=[](std::int32_t v){std::int64_t phase=v;phase%=65536;if(phase<0)phase+=65536;return phase-phase%4;};
            rotational=Rotational(offset,Rotate(unit,static_cast<int>(quantized(requested[1u])-quantized(yaw))));
        }
        Hit hit;Move move;move.level=Level;move.actor=child;
        const auto roll=Call(w,&World::Rotation,child,2u),childYaw=Call(w,&World::Rotation,child,1u),childPitch=Call(w,&World::Rotation,child,0u);
        move.rotation={childPitch,Wrap(std::int64_t{childYaw}+raw),roll};
        move.delta.z=Store(Add(Read(actual.z),Read(rotational.z)));move.delta.y=Store(Add(Read(actual.y),Read(rotational.y)));move.delta.x=Store(Add(Read(actual.x),Read(rotational.x)));
        static_cast<void>(Call(w,&World::Moving,move,std::ref(hit)));++result.moves;
        if(!Call(w,&World::Pawn,child))continue;
        const auto vy=Call(w,&World::View,child,1u),vp=Call(w,&World::View,child,0u),vr=Call(w,&World::View,child,2u);
        Call(w,&World::Write,child,0u,vp);Call(w,&World::Write,child,1u,Wrap(std::int64_t{vy}+raw));Call(w,&World::Write,child,2u,vr);
    }
}
}
#endif

std::int32_t World::Moving(Move move,Hit& hit){
    Record({Op::Move,move.actor});Require(move.level==Level&&move.options==std::array<std::int32_t,4u>{0,0,1,0},"Child call changed native Level/options");
    Require(!hit.next&&!hit.actor&&!hit.primitive&&Same(hit.location,{})&&Same(hit.normal,{})&&Bits(hit.time)==Bits(1.0f)&&hit.item==-1,
        "A child reused an earlier hit or lost the fresh full native packet seed");
    moves.push_back(move);hit.next=999;hit.actor=777;hit.primitive=888;hit.location={NAN,INFINITY,-INFINITY};hit.time=NAN;hit.item=123;
    if(mutation&&moves.size()==1u){++mutated;
        if(mutation&1u){slots[1u]=4;A(move.actor).base=0;A(move.actor).view={101,102,103};}
        if(mutation&2u){slots.push_back(5);A(5).base=Source;A(5).pawn=true;A(5).rotation={3,4,5};}
        if(mutation&4u){A(Source).rotation={300,400,500};A(Source).location={11,12,13};}
        if(mutation&8u){slots.resize(1u);}
        if(mutation&16u){A(move.actor).pawn=false;A(3).base=0;}
    }
    if(mutation&32u)slots.push_back(2u); // Deliberate endless live append, no dedup.
    if(nested){
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        if(oracle)static_cast<void>(Oracle::Run(*this,Source,{},Rot{},0,65536));
        else
#endif
        static_cast<void>(QuestVr::CarryOriginalActorChildren(Level,Source,{},Rot{},0,QuestVr::OriginalSineProfile::Table(),Bind()));
    }
    return moves.size()%2u?0:std::numeric_limits<std::int32_t>::min(); // BOTH ignored, as is the poisoned hit.
}
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
bool Same(const Actor& a,const Actor& b){return a.base==b.base&&a.pawn==b.pawn&&Same(a.location,b.location)&&a.rotation==b.rotation&&a.view==b.view;}
#endif
template<class F>void Refuse(F run){bool failed{};try{run();}catch(const std::exception&){failed=true;}Require(failed,"Unsafe active child-carry input succeeded");++refusals;}

void Controls(){
    World world;world.standing=0;Q q;q.standingCount=world.Bind().standingCount;
    auto table=QuestVr::OriginalSineProfile::Table();
    Require(QuestVr::CarryOriginalActorChildren(0,Source,{NAN,INFINITY,NAN},{},0,table,q,0).flow==QuestVr::OriginalActorCarryFlow::NoDependants,
        "Zero StandingCount read inactive providers/Delta/Level/slot budget");
    world.standing=255;
    for(std::int32_t test:{1,-1,std::numeric_limits<std::int32_t>::min()})
        Require(QuestVr::CarryOriginalActorChildren(0,Source,{NAN,INFINITY,NAN},{},test,table,q,0).flow==QuestVr::OriginalActorCarryFlow::TestBypass,
            "Any-nonzero test failed to bypass both carrying/encroachment segment");
    Refuse([&]{QuestVr::CarryOriginalActorChildren(Level,0,{},Rot{},0,table,Q{});});
    world=World{};q=world.Bind();q.mathRoot={};q.unitCoords={};q.location={};
    Require(QuestVr::CarryOriginalActorChildren(Level,Source,{-0.0f,-0.0f,-0.0f},{},0,table,q).moves==2u,
        "Exact rotation equality read inactive Location/GMath/coordinate data");
    Require(Same(world.moves.front().delta,{}),"No-rotation additions copied negative zeros instead of adding positive zero");
    world=World{};q=world.Bind();q.moveActor={};Refuse([&]{QuestVr::CarryOriginalActorChildren(Level,Source,{},Rot{},0,table,q);});
    // The internal provider adapter must preserve a real Hit& alias rather
    // than quietly passing a copy to the complete recursive native dependency.
    Hit external;Q alias;
    alias.moveActor=[&](Move,Hit& observed){Require(&observed==&external,"Recursive child hit reference was copied by its provider adapter");observed.item=77;return 0;};
    static_cast<void>(QuestVr::ActorChildCarryDetail::Call(alias,alias.moveActor,Move{},external));
    Require(external.item==77,"Recursive provider did not write the actual caller-owned hit");
    std::array<float,16384u> poison;poison.fill(std::numeric_limits<float>::quiet_NaN());
    world=World{};
    Require(QuestVr::CarryOriginalActorChildren(Level,Source,{},Rot{},0,poison,world.Bind()).moves==2u,
        "Inactive sine table was eagerly validated on exact raw rotation equality");
    world=World{};
    Refuse([&]{QuestVr::CarryOriginalActorChildren(Level,Source,{},Rot{1,0,0},0,poison,world.Bind());});
    world=World{};
    Refuse([&]{QuestVr::CarryOriginalActorChildren(Level,Source,{},Rot{0,65536,0},0,poison,world.Bind());});
}
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
World Compare(World input,Vec delta={},Rot requested={},std::int32_t test=0,std::size_t maximum=65536){
    World actual=input,expected=input;expected.oracle=true;Result a,b;bool refused{},oracleRefused{};
    try{a=QuestVr::CarryOriginalActorChildren(Level,Source,delta,requested,test,QuestVr::OriginalSineProfile::Table(),actual.Bind(),maximum);}catch(const std::exception&){refused=true;}
    try{b=Oracle::Run(expected,Source,delta,requested,test,maximum);}catch(const std::exception&){oracleRefused=true;}
    Require(refused==oracleRefused,"Child carry success/refusal differs from independent interpreter");
    Require(actual.trace==expected.trace&&actual.calls==expected.calls&&actual.work==expected.work&&actual.moves.size()==expected.moves.size(),"Child carry live provider/work/move trace differs");
    for(unsigned i=0u;i<actual.actors.size();++i)Require(Same(actual.actors[i],expected.actors[i]),"Retained child/view or callback-mutated state differs");
    for(std::size_t i=0u;i<actual.moves.size();++i){const auto& x=actual.moves[i];const auto& y=expected.moves[i];
        Require(x.level==y.level&&x.actor==y.actor&&x.rotation==y.rotation&&Same(x.delta,y.delta)&&x.options==y.options,"Child by-value call packet differs from independent arithmetic/provider model");}
    if(!refused)Require(a.flow==b.flow&&a.slotVisits==b.slotVisits&&a.moves==b.moves,"Child scan result/gates differ");
    ++cases;if(refused)++refusals;return actual;
}
std::uint64_t Next(std::uint64_t& x){x^=x<<13u;x^=x>>7u;x^=x<<17u;return x;}
void Hardware(){
    struct Precision{unsigned short previous{};Precision(){static_assert(std::numeric_limits<long double>::digits==64);asm volatile("fnstcw %0":"=m"(previous));
        const auto mode=static_cast<unsigned short>((previous&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}~Precision(){asm volatile("fldcw %0"::"m"(previous));}} precision;
    Oracle::Initialize();
    const std::array<std::int32_t,12u> angles{0,1,3,4,-1,-4,16384,32768,65536,-65536,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()};
    for(auto current:angles)for(auto requested:angles)for(unsigned axis=0u;axis<3u;++axis){World w;w.actors[Source].rotation[axis]=current;Rot r{};r[axis]=requested;
        Compare(w,{2,3,4},r);}
    for(unsigned mutation=0u;mutation<32u;++mutation){World w;w.mutation=mutation;Compare(w,{3,4,5},{1,4,7});}
    for(int count:{-1,0,1,2,3}){World w;w.countOverride=count;Compare(w,{},Rot{},0,3u);}
    for(std::size_t max:{0u,1u,2u})Compare(World{},Vec{},Rot{},0,max);
    World empty;empty.countOverride=0;Compare(empty,{},Rot{},0,0u);
    World append;append.mutation=32u;const auto bounded=Compare(append,{},Rot{},0,8u);
    Require(bounded.moves.size()>2u&&bounded.slots.size()>8u,"Live append fixture was truncated to initial count or deduplicated");
    World custom;custom.unit={{3,4,5},{1.25f,.5f,-.25f},{-.3f,.75f,.1f},{.2f,.3f,1.1f}};
    Compare(custom,{3,4,5},{3,4,5}); // Must consume actual UnitCoords, not replace it by identity.
    custom.unit.origin.x=std::numeric_limits<float>::quiet_NaN();Compare(custom,{},Rot{1,0,0});
    World overflow;overflow.actors[Source].location.x=std::numeric_limits<float>::max();
    overflow.actors[2].location.x=-std::numeric_limits<float>::max();const auto over=Compare(overflow,{},Rot{1,0,0});
    Require(std::any_of(over.trace.begin(),over.trace.end(),[](auto t){return t.op==Op::Math;})&&
        std::none_of(over.trace.begin(),over.trace.end(),[](auto t){return t.op==Op::Unit;}),
        "Offset overflow moved GMath-pointer/UnitCoords-copy admission across native Float stores");
    const auto whole=Compare(World{},{3,4,5},{1,4,7});
    for(std::size_t i=1u;i<=whole.calls;++i){World w;w.failure=i;Compare(w,{3,4,5},{1,4,7});++prefixes;}
    for(std::size_t i=0u;i<whole.work;++i){World w;w.limit=i;Compare(w,{3,4,5},{1,4,7});++prefixes;}
    World self;self.slots={Source};self.actors[Source].base=Source;self.nested=true;self.limit=200;const auto failed=Compare(self);
    Require(failed.work==201u&&failed.moves.size()>1u,"Self-Base recursion was skipped or reset its shared work budget");
    // Explicit MOCK outer rollback, not a production MoveActor/VM proof.
    World root;root.mutation=7u;const auto completeRoot=Compare(root,{3,4,5},{1,4,7});
    root.failure=completeRoot.calls;const auto originalActors=root.actors;const auto originalSlots=root.slots;
    bool refused{};try{static_cast<void>(QuestVr::CarryOriginalActorChildren(Level,Source,{3,4,5},{1,4,7},0,
        QuestVr::OriginalSineProfile::Table(),root.Bind()));}catch(const std::exception&){refused=true;root.actors=originalActors;root.slots=originalSlots;}
    Require(refused,"Mock root failure did not exercise a partial carrying prefix");
    for(unsigned i=0u;i<root.actors.size();++i)Require(Same(root.actors[i],originalActors[i]),"Mock enclosing root failed actor/view restoration");
    Require(root.slots==originalSlots,"Mock enclosing root failed live array restoration");
    std::uint64_t state=0x21cb4a95ef796308ull;
    for(unsigned i=0u;i<12000u;++i){const auto number=[&]{return static_cast<float>(static_cast<std::int32_t>(Next(state)%131073u)-65536)/64.0f;};
        const Vec offset{number(),number(),number()};Coords c{{NAN,INFINITY,-INFINITY},{number(),number(),number()},{number(),number(),number()},{number(),number(),number()}};
        const auto actual=QuestVr::ActorChildCarryDetail::RotationalDelta(offset,c),expected=Oracle::Rotational(offset,c);
        Require(Same(actual,expected),"Retained row/difference Float words differ from independent hardware x87");++numeric;
    }
    for(unsigned i=0u;i<3000u;++i){World w;w.actors[Source].rotation={std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(state))),
            std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(state))),0};
        for(auto& a:w.actors){a.location={static_cast<float>(Next(state)%65536u)/3,static_cast<float>(Next(state)%65536u)/7,static_cast<float>(Next(state)%65536u)/11};
            for(auto& r:a.rotation)r=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(state)));}
        Compare(w,{1,-0.0f,3},{std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(state))),std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(state))),0});
    }
    for(unsigned i=0u;i<300u;++i){World w;const auto scalar=[&]{return static_cast<float>(static_cast<std::int32_t>(Next(state)%4097u)-2048)/2048.0f;};
        w.unit={{scalar(),scalar(),scalar()},{scalar(),scalar(),scalar()},{scalar(),scalar(),scalar()},{scalar(),scalar(),scalar()}};
        Compare(w,{scalar(),scalar(),scalar()},{0,std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Next(state))),0});}
}
#else
void Hardware(){std::cout<<"SKIP independent x87 carry oracle on non-x86 compiler; deterministic controls still run.\n";}
#endif
} // namespace
int main(){try{Controls();Hardware();std::cout<<"PASS original based-child carry "<<checks<<" checks / "<<refusals<<" refusals, "<<cases
    <<" independent interpreter cases, "<<prefixes<<" failure prefixes, "<<numeric<<" row arithmetic cases and "<<mutated
    <<" callback mutation executions. Recursive Move is MOCK; not runtime movement, physics or first-map completion.\n";return 0;
}catch(const std::exception& error){std::cerr<<"Original child carry failed: "<<error.what()<<'\n';return 1;}}
