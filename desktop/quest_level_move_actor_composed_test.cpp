#include "quest_level_move_actor.h"
#include "quest_actor_child_carry.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

// Composition of the real detached MoveActor and child-carry coordinators.
// Actor storage, collision/encroachment/zone services and enclosing world are
// explicit mocks. No map, real native266 provider or Training physics is run.
namespace {
using Id=std::uintptr_t;
using Vec=PortableModelVec3;
using Rot=std::array<std::int32_t,3u>;
using Hit=QuestVr::OriginalActorLineHit;
using Move=QuestVr::OriginalActorChildMove;
using CarryResult=QuestVr::OriginalActorCarryResult;
using CarryFlow=QuestVr::OriginalActorCarryFlow;
constexpr Id Level=100u,Parent=1u,Child=2u,Grandchild=3u;
std::size_t checks{},cases{},refusals{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
bool Same(Vec a,Vec b){return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
bool Fresh(const Hit& hit){return !hit.next&&!hit.actor&&!hit.primitive&&Same(hit.location,{})&&
    Same(hit.normal,{})&&Bits(hit.time)==Bits(1.0f)&&hit.item==-1;}
float Channel(Vec value,unsigned axis){return axis==0u?value.x:axis==1u?value.y:value.z;}
void Store(Vec& value,unsigned axis,float word){if(axis==0u)value.x=word;else if(axis==1u)value.y=word;else value.z=word;}

enum class Op { Carry,Standing,Count,Slot,Base,Rotation,Location,Math,Unit,ChildMove,
    Pawn,View,WriteView,MoveLocation,MoveRotation,Mark,Pop,Zone };
struct Event {Op op{};Id actor{};unsigned axis{};std::uint64_t value{};std::size_t work{},depth{};};
struct Actor {Id base{};std::uint8_t standing{};bool pawn{true};Vec location{};Rot rotation{},view{};};
struct Carried {Id actor{};CarryResult result{};};
struct World {
    std::array<Actor,8u> actors{};
    std::vector<Id> slots{Parent,Child};
    std::vector<Event> trace;
    std::vector<Move> childMoves;
    std::vector<Carried> carried;
    std::vector<Id> marks,popped;
    // Only these cardinal phases are used; this is an explicit fixture table,
    // not a claim about initialization of original GMath on Quest.
    std::array<float,16384u> table{};
    std::size_t work{},limit=std::numeric_limits<std::size_t>::max(),nextMark{1u},maximumDepth{};
    bool gateOnly{},appendAtChildZone{},shrinkAtChildZone{},mutated{};
    World(){
        trace.reserve(4096u);marks.reserve(32u);popped.reserve(32u);
        slots.reserve(16u);childMoves.reserve(16u);carried.reserve(16u);
        actors[Parent].location={10,20,30};actors[Child].location={20,30,40};
        actors[Child].base=Parent;actors[Child].view={4,5,6};
        table[0u]=0.0f;table[4096u]=1.0f;table[8192u]=0.0f;table[12288u]=-1.0f;
    }
    Actor& At(Id id){if(!id||id>=actors.size())throw std::runtime_error("Composed fixture actor identity refused");return actors[id];}
    void CheckLevel(Id value){Require(value==Level,"A nested service substituted the actor's own Level for the retained parent Level");}
    void Charge(){if(++work>limit)throw std::runtime_error("Composed MoveActor shared work refused");}
    void Record(Op op,Id actor=0u,unsigned axis=0u,std::uint64_t value=0u){
        if(trace.size()+8u>=trace.capacity())throw std::runtime_error("Composed fixture trace bound exhausted");
        trace.push_back({op,actor,axis,value,work,marks.size()});
    }
    Id Mark(){
        Require(marks.size()<marks.capacity(),"Mock mark capacity exhausted");
        const auto token=nextMark++;marks.push_back(token);maximumDepth=std::max(maximumDepth,marks.size());
        Record(Op::Mark,token);return token;
    }
    void Pop(Id token)noexcept{
        // Storage is reserved before execution. Mandatory cleanup allocates
        // nothing, consumes no shared work, and validates its strict LIFO order.
        if(marks.empty()||marks.back()!=token||trace.size()==trace.capacity()||popped.size()==popped.capacity())std::terminate();
        trace.push_back({Op::Pop,token,0u,0u,work,marks.size()});popped.push_back(token);marks.pop_back();
    }
    std::uint8_t Standing(Id id){Record(Op::Standing,id);return At(id).standing;}
    std::int32_t Rotation(Id id,unsigned axis){Record(Op::Rotation,id,axis);return At(id).rotation.at(axis);}
    float Location(Id id,unsigned axis){Record(Op::Location,id,axis);return Channel(At(id).location,axis);}
    bool MoveActor(Id id,Vec delta,Rot rotation,std::array<std::int32_t,4u> options,Hit& hit){
        return QuestVr::MoveOriginalLevelActor(Level,id,delta,rotation,hit,options,MoveQueries());
    }
    void Zone(Id level,Id id,std::int32_t test,std::int32_t force){
        CheckLevel(level);Require(force==0,"Composed movement used birth-style zone force refresh");
        Record(Op::Zone,id,0u,std::bit_cast<std::uint32_t>(test));
        if(id!=Child||mutated)return;
        if(appendAtChildZone){
            mutated=true;
            // Replace the visited slot and remove its Base. The carry helper
            // must still update the retained Child's current Pawn view words.
            slots[1u]=4u;actors[Child].base=0u;actors[Child].view={100,200,300};
            actors[Grandchild].base=Parent;actors[Grandchild].location={10,20,30};
            actors[Grandchild].rotation={8,9,10};actors[Grandchild].view={1,2,3};
            slots.push_back(Grandchild);
        }else if(shrinkAtChildZone){mutated=true;slots.resize(1u);}
    }
    QuestVr::OriginalActorChildCarryQueries ChildQueries(Id owner){
        QuestVr::OriginalActorChildCarryQueries q;
        q.standingCount=[this](Id id){return Standing(id);};
        q.work=[this]{Charge();};
        if(gateOnly)return q; // Both real helper admission branches must stay lazy.
        q.actorCount=[this](Id level){CheckLevel(level);Record(Op::Count,level);return static_cast<std::int32_t>(slots.size());};
        q.actorSlot=[this](Id level,std::int32_t index){CheckLevel(level);Record(Op::Slot,level,static_cast<unsigned>(index));return slots.at(static_cast<std::size_t>(index));};
        q.base=[this](Id id){Record(Op::Base,id);return At(id).base;};
        q.rotation=[this](Id id,unsigned axis){return Rotation(id,axis);};
        q.location=[this](Id id,unsigned axis){return Location(id,axis);};
        q.mathRoot=[this]{Record(Op::Math);return Id{99u};};
        q.unitCoords=[this](Id root){Require(root==99u,"Wrong fixture GMath root");Record(Op::Unit,root);return QuestVr::IdentityOriginalModelCoords();};
        q.moveActor=[this,owner](Move move,Hit& hit){
            CheckLevel(move.level);Require(move.options==std::array<std::int32_t,4u>{0,0,1,0},"Real carry helper lost original child MoveActor options");
            Require(Fresh(hit),"Real carry helper reused or failed to initialize a child's full hit packet");
            Require(!marks.empty(),"Recursive child move escaped the enclosing parent's mark/root");
            Record(Op::ChildMove,move.actor,0u,owner);childMoves.push_back(move);
            return static_cast<std::int32_t>(MoveActor(move.actor,move.delta,move.rotation,move.options,hit));
        };
        q.pawn=[this](Id id){Record(Op::Pawn,id);return At(id).pawn;};
        q.viewRotation=[this](Id id,unsigned axis){Record(Op::View,id,axis);return At(id).view.at(axis);};
        q.writeViewRotation=[this](Id id,unsigned axis,std::int32_t value){Record(Op::WriteView,id,axis,std::bit_cast<std::uint32_t>(value));At(id).view.at(axis)=value;};
        return q;
    }
    QuestVr::OriginalLevelMoveActorQueries MoveQueries(){
        QuestVr::OriginalLevelMoveActorQueries q;
        q.eligibilityFlags=[this](Id id){At(id);return std::uint32_t{0x80000u};};
        q.staticFlag=[this](Id id){At(id);return false;};
        q.hasBrush=[this](Id id){At(id);return false;};
        q.isA=[this](Id id,QuestVr::OriginalMoveActorClass cls){return cls==QuestVr::OriginalMoveActorClass::Pawn&&At(id).pawn;};
        q.collisionFlags=[this](Id id){At(id);return std::uint8_t{};}; // Explicit no-sweep collision mock.
        q.standingCount=[this](Id id){return Standing(id);};
        q.rotation=[this](Id id,unsigned axis){return Rotation(id,axis);};
        q.writeRotation=[this](Id id,unsigned axis,std::int32_t value){Record(Op::MoveRotation,id,axis,std::bit_cast<std::uint32_t>(value));At(id).rotation.at(axis)=value;};
        q.location=[this](Id id,unsigned axis){return Location(id,axis);};
        q.writeLocation=[this](Id id,unsigned axis,float value){Record(Op::MoveLocation,id,axis,Bits(value));Store(At(id).location,axis,value);};
        q.carryChildren=[this](Id level,Id id,Vec delta,Rot rotation,std::int32_t test){
            CheckLevel(level);Record(Op::Carry,id,0u,std::bit_cast<std::uint32_t>(test));
            const auto result=QuestVr::CarryOriginalActorChildren(level,id,delta,rotation,test,table,ChildQueries(id));
            carried.push_back({id,result});
        };
        q.mark=[this]{return Mark();};
        q.popMark=[this](Id token)noexcept{Pop(token);};
        q.touching=[this](Id id,unsigned slot){At(id);Require(slot<4u,"Coordinator read beyond four Touching slots");return Id{};};
        q.setZone=[this](Id level,Id id,std::int32_t test,std::int32_t force){Zone(level,id,test,force);};
        q.work=[this]{Charge();};
        // No sweep/hash/event/encroachment fallback is installed. All actors
        // are mock Pawns and collision flags0; an unexpected branch refuses.
        return q;
    }
};

std::size_t Count(const World& world,Op op,Id id){return static_cast<std::size_t>(std::count_if(world.trace.begin(),world.trace.end(),
    [&](const Event& e){return e.op==op&&e.actor==id;}));}
const CarryResult& CarriedBy(const World& world,Id id){
    const auto found=std::find_if(world.carried.begin(),world.carried.end(),[&](const Carried& item){return item.actor==id;});
    Require(found!=world.carried.end(),"Actual child-carry helper was not called for this coordinator");return found->result;
}
std::size_t Position(const World& world,Op op,Id actor){
    const auto found=std::find_if(world.trace.begin(),world.trace.end(),[&](const Event& e){return e.op==op&&e.actor==actor;});
    Require(found!=world.trace.end(),"Composed transcript lacks its distinguishing event");return static_cast<std::size_t>(found-world.trace.begin());
}
void Finish(const World& world,std::size_t depth){
    Require(world.marks.empty()&&world.maximumDepth==depth,"Composed moves did not share/release the expected nested marks");
    Require(world.nextMark==depth+1u&&world.popped.size()==depth,"Composed mark acquisition/release count differs");
    for(std::size_t i=0u;i<depth;++i)Require(world.popped[i]==depth-i,"Nested MoveActor mark cleanup is not LIFO");
}
void GatedChildren(){
    for(const std::int32_t test:{0,-7}){
        World world;world.gateOnly=true;world.actors[Parent].standing=test?255u:0u;
        const auto childLocation=world.actors[Child].location;Hit hit;
        Require(world.MoveActor(Parent,{1,2,3},{},{test,0,0,0},hit),"Composed no-dependant/test branch refused an inactive child provider");
        const auto& result=CarriedBy(world,Parent);
        Require(result.flow==(test?CarryFlow::TestBypass:CarryFlow::NoDependants)&&!result.slotVisits&&!result.moves,
            "Actual carry helper did not preserve its StandingCount0 or bTest branch");
        Require(Count(world,Op::Standing,Parent)==1u&&world.childMoves.empty()&&Same(world.actors[Child].location,childLocation),
            "Inactive child carry read children, moved them or reread StandingCount unexpectedly");
        Require(std::none_of(world.trace.begin(),world.trace.end(),[](const Event& e){return e.op==Op::Count||e.op==Op::Slot||
            e.op==Op::Base||e.op==Op::Math||e.op==Op::Unit||e.op==Op::Pawn||e.op==Op::View||e.op==Op::WriteView;}),
            "Actual carry gate touched an absent inactive child provider");
        Require(Same(world.actors[Parent].location,{11,22,33})&&Count(world,Op::Zone,Parent)==1u,
            "bTest/no-dependants turned the outer movement into a read-only preview");
        Finish(world,1u);++cases;
    }
}
void TranslationAndQuarterTurn(){
    {
        World world;world.actors[Parent].standing=1u;world.actors[Child].rotation={100,200,300};Hit hit;
        Require(world.MoveActor(Parent,{3,4,5},{},{},hit),"Composed translating parent failed");
        Require(world.childMoves.size()==1u&&Same(world.childMoves[0u].delta,{3,4,5})&&world.childMoves[0u].rotation==Rot{100,200,300},
            "Real carry-to-coordinator binding changed the exact translation/child Rotation packet");
        Require(Same(world.actors[Child].location,{23,34,45})&&Same(world.actors[Parent].location,{13,24,35})&&
            world.actors[Child].view==Rot{4,5,6},"Recursive coordinator did not actually publish child translation/current Pawn view");
        Require(Count(world,Op::Math,0u)==0u&&CarriedBy(world,Parent).moves==1u,
            "No-rotation composed carry unnecessarily consumed GMath or failed to record its real child move");
        Require(Position(world,Op::Zone,Child)<Position(world,Op::MoveLocation,Parent),"Parent pose changed before the recursive child completed");
        Finish(world,2u);++cases;
    }
    {
        World world;world.actors[Parent].standing=1u;world.actors[Child].location={14,20,30};
        world.actors[Child].rotation={101,202,303};Hit hit;
        Require(world.MoveActor(Parent,{2,3,0},{0,16384,0},{},hit),"Composed quarter-turn parent failed");
        // Explicit cardinal fixture geometry, not a copy of the carry code:
        // A child four units east of its parent rotates four units north.
        // It therefore moves(-4,+4,0), plus parent translation(2,3,0).
        Require(world.childMoves.size()==1u&&Same(world.childMoves[0u].delta,{-2,7,0})&&
            world.childMoves[0u].rotation==Rot{101,16586,303},"Quarter-turn carry packet differs from the independent cardinal expectation");
        Require(Same(world.actors[Child].location,{12,27,30})&&world.actors[Child].rotation==Rot{101,16586,303}&&
            world.actors[Child].view==Rot{4,16389,6},"Recursive child pose or post-call Pawn yaw did not follow the actual carry helper");
        Require(Same(world.actors[Parent].location,{12,23,30})&&world.actors[Parent].rotation==Rot{0,16384,0}&&
            Count(world,Op::Math,0u)==1u&&Count(world,Op::Unit,99u)==1u,"Quarter-turn composition did not use its supplied coordinate dependency exactly once");
        Finish(world,2u);++cases;
    }
}
void LiveSlots(){
    {
        World world;world.actors[Parent].standing=1u;world.actors[Child].location={10,20,30};world.appendAtChildZone=true;Hit hit;
        Require(world.MoveActor(Parent,{1,2,3},{0,16384,0},{},hit),"Composed live-append movement failed");
        Require(world.childMoves.size()==2u&&world.childMoves[0u].actor==Child&&world.childMoves[1u].actor==Grandchild&&
            CarriedBy(world,Parent).slotVisits==3u&&CarriedBy(world,Parent).moves==2u,
            "Parent carry snapshotted the initial slot count or revisited the replacement of its current slot");
        Require(world.slots==std::vector<Id>{Parent,4u,Grandchild}&&!world.actors[Child].base&&
            world.actors[Child].view==Rot{100,16584,300}&&world.actors[4u].view==Rot{},
            "Post-recursive Pawn adjustment lost the retained child when its slot/Base changed");
        Require(Same(world.childMoves[1u].delta,{1,2,3})&&world.childMoves[1u].rotation==Rot{8,16393,10}&&
            Same(world.actors[Grandchild].location,{11,22,33})&&world.actors[Grandchild].view==Rot{1,16386,3},
            "Appended real carry recipient failed recursive movement/current yaw propagation");
        Require(world.marks.empty()&&world.maximumDepth==2u&&world.popped==std::vector<Id>{2u,3u,1u},
            "Sibling nested moves did not keep their common parent mark alive");++cases;
    }
    {
        World world;world.actors[Parent].standing=1u;world.slots.push_back(Grandchild);
        world.actors[Grandchild].base=Parent;world.actors[Grandchild].location={30,40,50};world.shrinkAtChildZone=true;Hit hit;
        Require(world.MoveActor(Parent,{1,2,3},{},{},hit)&&world.childMoves.size()==1u&&
            CarriedBy(world,Parent).slotVisits==2u&&Same(world.actors[Grandchild].location,{30,40,50}),
            "Actual composed carry used a stale count/slot after a recursive callback shrank the list");
        Finish(world,2u);++cases;
    }
}
World NestedFixture(){
    World world;world.actors[Parent].standing=1u;world.actors[Child].standing=1u;
    world.slots.push_back(Grandchild);world.actors[Grandchild].base=Child;world.actors[Grandchild].location={30,40,50};
    return world;
}
void RecursiveRootAndRefusal(){
    auto world=NestedFixture();Hit hit;
    Require(world.MoveActor(Parent,{1,2,3},{},{},hit),"Two-level real carry/coordinator recursion failed");
    Require(world.childMoves.size()==2u&&world.childMoves[0u].actor==Child&&world.childMoves[1u].actor==Grandchild&&
        world.childMoves[0u].level==Level&&world.childMoves[1u].level==Level,
        "Recursive carry lost current Level or attempted an unrelated child twice");
    Require(Same(world.actors[Parent].location,{11,22,33})&&Same(world.actors[Child].location,{21,32,43})&&
        Same(world.actors[Grandchild].location,{31,42,53}),"Actual nested coordinators did not propagate translation through both Base levels");
    Require(Position(world,Op::Zone,Grandchild)<Position(world,Op::MoveLocation,Child)&&
        Position(world,Op::Zone,Child)<Position(world,Op::MoveLocation,Parent),"Nested pose publication order is not grandchild, child, parent");
    Finish(world,3u);++cases;

    const auto thirdMark=std::find_if(world.trace.begin(),world.trace.end(),[](const Event& e){return e.op==Op::Mark&&e.actor==3u;});
    Require(thirdMark!=world.trace.end(),"Nested transcript lacks third mark for the shared-budget refusal witness");
    auto failed=NestedFixture();failed.limit=thirdMark->work+1u;bool refused{};
    try{Hit denied;static_cast<void>(failed.MoveActor(Parent,{1,2,3},{},{},denied));}catch(const std::exception&){refused=true;}
    Require(refused&&failed.work==failed.limit+1u&&failed.maximumDepth==3u&&failed.nextMark==4u,
        "Recursive coordinator/helper opened a fresh allowance or never reached the denied nested operation");
    Require(failed.marks.empty()&&failed.popped==std::vector<Id>{3u,2u,1u}&&Count(failed,Op::Zone,Parent)==0u,
        "Nested shared-work refusal stranded marks or continued parent publication");
    // No automatic gameplay rollback is asserted: the real runtime still
    // needs its enclosing journal. These are locally guaranteed mark effects.
    ++refusals;++cases;
}
} // namespace

int main(){try{
    GatedChildren();TranslationAndQuarterTurn();LiveSlots();RecursiveRootAndRefusal();
    std::cout<<"PASS composed original MoveActor/child carry "<<checks<<" checks / "<<cases<<" cases / "<<refusals
        <<" shared-work refusals. Collision, zones and world storage are mocks; no runtime native266 or Training physics claim.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"Composed original MoveActor/child carry failed: "<<error.what()<<'\n';return 1;}}
