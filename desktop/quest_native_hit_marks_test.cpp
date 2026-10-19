#include "quest_native_hit_marks.h"
#include "quest_level_move_actor.h"

#include <bit>
#include <cstdlib>
#include <iostream>
#include <new>
#include <type_traits>

namespace {bool forbidAllocations{};}
#if defined(__GNUC__)
#define HIT_ALLOC_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define HIT_ALLOC_NOINLINE __declspec(noinline)
#else
#define HIT_ALLOC_NOINLINE
#endif
// Keep the test allocator observation boundary opaque to allocator-folding
// optimization (including GCC's false-positive mismatched-new-delete inlining).
HIT_ALLOC_NOINLINE void* operator new(std::size_t bytes){if(forbidAllocations)throw std::bad_alloc();if(auto* p=std::malloc(bytes?bytes:1u))return p;throw std::bad_alloc();}
HIT_ALLOC_NOINLINE void* operator new[](std::size_t bytes){return ::operator new(bytes);}
HIT_ALLOC_NOINLINE void operator delete(void* p)noexcept{std::free(p);}
HIT_ALLOC_NOINLINE void operator delete[](void* p)noexcept{std::free(p);}
HIT_ALLOC_NOINLINE void operator delete(void* p,std::size_t)noexcept{std::free(p);}
HIT_ALLOC_NOINLINE void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
#undef HIT_ALLOC_NOINLINE

namespace {
using Store=QuestVr::NativeHitMarks;using Stamp=QuestVr::NativeHitRootStamp;
using Limits=QuestVr::NativeHitMarkLimits;using Arena=QuestVr::OriginalLineHitArena;
using Hit=QuestVr::OriginalActorLineHit;using Id=std::uintptr_t;
constexpr Stamp Root{19u,23u,29u,31u};
std::size_t checks{},refusals{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void Refuse(F&& f){bool caught{};try{f();}catch(const std::exception&){caught=true;}
    Require(caught,"Invalid native hit storage silently succeeded");++refusals;}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(const Hit& a,const Hit& b){return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&
    Same(a.location.x,b.location.x)&&Same(a.location.y,b.location.y)&&Same(a.location.z,b.location.z)&&
    Same(a.normal.x,b.normal.x)&&Same(a.normal.y,b.normal.y)&&Same(a.normal.z,b.normal.z)&&Same(a.time,b.time)&&a.item==b.item;}
Arena List(std::size_t count,Id actor=100u){Arena a;a.records.reserve(count+5u);a.records.resize(count);a.first=count?1u:0u;
    for(std::size_t i=0;i<count;++i){auto& h=a.records[i];h.next=i+1u<count?i+2u:0u;h.actor=actor+i;h.primitive=200u+i;
        h.location={float(i),-0.0f,std::bit_cast<float>(0x7f812345u)};h.normal={1.0f,0.0f,std::bit_cast<float>(0xffc54321u)};
        h.time=std::bit_cast<float>(static_cast<std::uint32_t>(0x3e800000u+i));h.item=-7-static_cast<int>(i);}
    return a;}
Id Adopt(Store& s,Arena&& a){return s.Adopt(Root,std::move(a),[](std::size_t){});}
void BasicAndRaw(){
    Store s;const auto fixed=s.RetainedBytes();
    Require(!s.Active()&&!s.PayloadBytes()&&!s.MarkDepth()&&!s.ArenaCount(),"Hit store not initially empty");
    Refuse([&]{s.Mark(Root);});Refuse([&]{s.Packet(Root,2u);});
    for(unsigned field=0;field<4u;++field){auto bad=Root;
        if(field==0)bad.level=0;
        if(field==1)bad.lookupControl=0;
        if(field==2)bad.mapGeneration=0;
        if(field==3)bad.lookupGeneration=0;
        Refuse([&]{s.BeginRoot(bad);});}
    s.BeginRoot(Root);const auto root=s.RootSerial();Refuse([&]{s.BeginRoot(Root);});
    Refuse([&]{auto a=List(1);Adopt(s,std::move(a));});
    const auto mark=s.Mark(Root);Require((mark&1u)&&!s.Pop(mark+2u),"Wrong-kind/unknown mark accepted");
    auto a=List(3);const auto expected=a.records;const auto capacity=a.records.capacity()*sizeof(Hit);
    unsigned preflights{};const auto head=s.Adopt(Root,std::move(a),[&](std::size_t bytes){++preflights;
        Require(bytes==capacity&&s.RetainedBytes()==fixed&&!s.ArenaCount(),"Adoption preflight didn't observe exact current/additional capacities");});
    Require(preflights==1u&&a.records.empty()&&!a.first&&s.ArenaCount()==1u&&s.IssuedRecords()==3u&&
        s.PayloadBytes()==capacity&&s.RetainedBytes()==fixed+capacity,"Hit arena didn't transfer/account its owning vector");
    Require(head&&!(head&1u)&&(head>>32u)==root&&static_cast<std::uint32_t>(head)==2u,"Record handle isn't root-qualified even ordinal1");
    auto cursor=head;
    for(std::size_t i=0;i<expected.size();++i){auto packet=expected[i];packet.next=i+1u<expected.size()?head+2u*(i+1u):0u;
        Require(Same(s.Packet(Root,cursor),packet)&&s.Actor(Root,cursor)==expected[i].actor&&Same(s.Time(Root,cursor),expected[i].time),
            "Native hit adoption changed raw packet words or failed to rebase Next");cursor=s.Next(Root,cursor);}
    Require(!cursor,"Rebased hit chain has an invented tail");
    // The store exposes packet COPIES, never a retained mutable hit pointer.
    auto packet=s.Packet(Root,head);packet.actor=999u;
    Require(packet.actor!=s.Actor(Root,head)&&s.Actor(Root,head)==100u,"Detached packet write mutated the live list");
    for(auto bad:{Id{},mark,head+1u,head+6u,head^(Id{1u}<<32u)})Refuse([&]{s.Packet(Root,bad);});
    for(unsigned field=0;field<4u;++field){auto foreign=Root;
        if(field==0)++foreign.level;
        if(field==1)++foreign.lookupControl;
        if(field==2)++foreign.mapGeneration;
        if(field==3)++foreign.lookupGeneration;
        Refuse([&]{s.Actor(foreign,head);});}
    Require(s.Pop(mark)&&!s.Pop(mark)&&!s.ArenaCount()&&s.RetainedBytes()==fixed&&s.IssuedRecords()==3u,
        "Pop failed exact suffix cleanup or replenished cumulative ordinals");
    Refuse([&]{s.Next(Root,head);});Require(s.Clear()&&!s.Active(),"Clear failed to retire native hit root");
    s.BeginRoot(Root);Require(s.RootSerial()>root,"New root reused old hit capability generation");
    const auto next=s.Mark(Root);const auto newHead=Adopt(s,List(3));Require(newHead!=head,"New root aliased expired hit integers");
    Refuse([&]{s.Packet(Root,head);});Require(!s.Pop(mark)&&s.Pop(next),"Old root mark retired a new mark");
}
void NestedAndCycles(){
    Store s;s.BeginRoot(Root);const auto parent=s.Mark(Root);const auto old=Adopt(s,List(2));
    const auto saved=s.Packet(Root,old);const auto parentBytes=s.PayloadBytes();
    const auto child=s.Mark(Root);auto reversed=List(4,300);reversed.first=4;reversed.records[3].next=2;
    reversed.records[1].next=4; // Valid local cycle and unreachable records are NOT repaired.
    const auto head=Adopt(s,std::move(reversed));const auto second=s.Next(Root,head);
    Require(s.Next(Root,second)==head&&s.Actor(Root,head)==303u&&Same(s.Packet(Root,old),saved),
        "Adoption repaired a cycle/order or invalidated an earlier arena");
    const auto grandchild=s.Mark(Root);const auto grand=Adopt(s,List(3,400));
    Require(!s.Pop(parent)&&!s.Pop(child)&&s.MarkDepth()==3u&&s.Actor(Root,grand)==400u,
        "Non-LIFO cleanup changed retained storage");
    Require(s.Pop(grandchild)&&s.Actor(Root,head)==303u,"Nested cleanup lost parent's list");Refuse([&]{s.Actor(Root,grand);});
    Require(s.Pop(child)&&s.PayloadBytes()==parentBytes&&Same(s.Packet(Root,old),saved),"Child cleanup lost original list/charge");
    Refuse([&]{s.Time(Root,head);});const auto sibling=s.Mark(Root);const auto fresh=Adopt(s,List(4,500));
    Require(fresh!=head&&s.Actor(Root,fresh)==500u&&Same(s.Packet(Root,old),saved),"Sibling arena reused popped handles");
    Refuse([&]{s.Actor(Root,head);});Require(s.Pop(sibling)&&s.Pop(parent)&&!s.ArenaCount(),"Nested mark cleanup stranded a list");
    // Independent numeric-handle model: many sibling suffixes and every old
    // handle remain invalid while the original parent allocation survives.
    std::vector<Id> expired;expired.reserve(1000);const auto outer=s.Mark(Root);const auto keep=Adopt(s,List(1,777));
    std::uint32_t issued=s.IssuedRecords();
    for(unsigned i=0;i<1000u;++i){const auto token=s.Mark(Root);const auto n=1u+i%7u;const auto h=Adopt(s,List(n,1000u+i));
        Require(static_cast<std::uint32_t>(h)==2u*(issued+1u),"New arena didn't consume monotonic record ordinal range");
        issued+=n;Require(s.IssuedRecords()==issued&&s.Actor(Root,keep)==777u,"Sibling root reset accounting or corrupted parent");
        Require(s.Pop(token),"Sibling cleanup refused its actual top mark");expired.push_back(h);}
    for(const auto h:expired)Refuse([&]{s.Packet(Root,h);});
    Require(s.Pop(outer),"Final model parent cleanup failed");
}
void AtomicRefusal(){
    Store s;s.BeginRoot(Root);const auto mark=s.Mark(Root);const auto old=Adopt(s,List(2));const auto packet=s.Packet(Root,old);
    const auto bytes=s.PayloadBytes(),arenas=s.ArenaCount();const auto issued=s.IssuedRecords();
    for(unsigned fault=0u;fault<3u;++fault){auto input=List(3);if(fault==0u)input.first=4u;if(fault==1u)input.records[1].next=4u;
        const auto snapshot=input.records;const auto first=input.first;unsigned validations{};
        Refuse([&]{s.Adopt(Root,std::move(input),[&](std::size_t){++validations;if(fault==2u)throw std::runtime_error("late peak refused");});});
        Require(input.first==first&&input.records.size()==snapshot.size()&&s.PayloadBytes()==bytes&&s.ArenaCount()==arenas&&
            s.IssuedRecords()==issued&&Same(s.Packet(Root,old),packet),"Failed adoption published partial storage/accounting");
        for(std::size_t i=0;i<snapshot.size();++i)Require(Same(input.records[i],snapshot[i]),"Failed adoption remapped incoming local links");
        Require(validations==(fault==2u?1u:0u),"Invalid input reached preflight or late refusal skipped it");}
    for(unsigned attack=0u;attack<4u;++attack){auto input=List(3);const auto local=input.records;
        Refuse([&]{s.Adopt(Root,std::move(input),[&](std::size_t){
            if(attack==0u)Require(!s.Pop(mark),"Reentrant preflight popped a live mark");
            if(attack==1u)Require(!s.Clear(),"Reentrant preflight cleared root");
            if(attack==2u){try{s.Mark(Root);}catch(const std::exception&) {}}
            if(attack==3u){auto nested=List(1);try{Adopt(s,std::move(nested));}catch(const std::exception&) {}}
        });});
        Require(s.ArenaCount()==arenas&&s.PayloadBytes()==bytes&&s.IssuedRecords()==issued&&s.MarkDepth()==1u,
            "Caught reentrant preflight mutation bypassed adoption guard");
        for(std::size_t i=0;i<local.size();++i)Require(Same(input.records[i],local[i]),"Reentrant preflight changed incoming arena");}
    auto empty=List(0);Require(!Adopt(s,std::move(empty))&&s.ArenaCount()==arenas&&s.PayloadBytes()==bytes&&s.IssuedRecords()==issued,
        "Empty adopted list consumed arena/record capability or retained an empty capacity");
    Require(s.Pop(mark),"Atomic refusal cleanup failed");
}
void LimitsAndNoAlloc(){
    for(unsigned limit=0u;limit<5u;++limit){Limits l;
        if(limit==0u)l.marks=0;
        if(limit==1u)l.arenas=0;
        if(limit==2u)l.records=0;
        if(limit==3u)l.payloadBytes=0;
        if(limit==4u)l.markSerials=0;
        Store s(l);s.BeginRoot(Root);
        if(limit==0u||limit==4u)Refuse([&]{s.Mark(Root);});else{const auto m=s.Mark(Root);Refuse([&]{Adopt(s,List(1));});Require(s.Pop(m),"Tight cap stranded mark");}
        Require(!s.ArenaCount()&&!s.PayloadBytes(),"Tight cap published retained storage");}
    for(unsigned field=0u;field<5u;++field){Limits l;
        if(field==0)++l.marks;
        if(field==1)++l.arenas;
        if(field==2)++l.records;
        if(field==3)++l.payloadBytes;
        if(field==4)++l.markSerials;
        Refuse([&]{Store bad(l);});}
    {Limits l;l.records=3u;l.arenas=1u;l.markSerials=2u;Store s(l);s.BeginRoot(Root);const auto m=s.Mark(Root);const auto h=Adopt(s,List(2));
        Refuse([&]{Adopt(s,List(1));});Require(s.Pop(m),"Arena cap cleanup failed");
        const auto n=s.Mark(Root);Adopt(s,List(1));Refuse([&]{Adopt(s,List(1));});Refuse([&]{s.Packet(Root,h);});
        Require(s.Pop(n),"Cumulative record cap cleanup failed");Refuse([&]{s.Mark(Root);});}
    {Store s;s.BeginRoot(Root);std::array<Id,64u> marks{};
        for(auto& m:marks)m=s.Mark(Root);
        Refuse([&]{s.Mark(Root);});
        for(auto i=marks.size();i;--i)Require(s.Pop(marks[i-1u]),"Fixed depth cleanup not LIFO");}
    {Store s;s.BeginRoot(Root);const auto m=s.Mark(Root);auto input=List(4);Id h{};
        bool popped{},cleared{},packetRead{};forbidAllocations=true;
        try{h=Adopt(s,std::move(input));packetRead=s.Packet(Root,h).actor==100u;
            popped=s.Pop(m);cleared=s.Clear();}catch(...){forbidAllocations=false;throw;}forbidAllocations=false;
        Require(h&&packetRead&&popped&&cleared&&!s.PayloadBytes()&&!s.Active(),
            "Owning vector adoption/access/mandatory cleanup allocated or retained storage");
        Refuse([&]{s.Packet(Root,h);});}
    // A rejected new root cannot reuse the process-lifetime capability stream.
    {Limits l;l.rootSerial=0;Store exhausted(l);Refuse([&]{exhausted.BeginRoot(Root);});Require(!exhausted.Active(),"Root serial exhaustion partially published");}
}
void CoordinatorComposition(){
    // REAL retained store + REAL full movement coordinator, explicit MOCK
    // world/pose/collision/events. Both queries initially use local handles1..3;
    // nested conversion must not alias the parent's still-live list.
    const auto run=[](std::size_t limit){
        Store s;s.BeginRoot(Root);
        std::array<PortableModelVec3,3u> positions{};
        std::array<std::array<std::int32_t,3u>,3u> rotations{};
        std::vector<std::pair<Id,Id>> touches;touches.reserve(8u);
        std::vector<Id> issuedHeads;issuedHeads.reserve(8u);
        std::vector<Id> pops;pops.reserve(8u);
        std::size_t work{},marks{},bumpCalls{};Hit parent,child;
        QuestVr::OriginalLevelMoveActorQueries q;
        q.work=[&]{if(++work>limit)throw std::runtime_error("composed retained-hit work refused");};
        q.eligibilityFlags=[](Id){return std::uint32_t{0x80000u};};
        q.collisionFlags=[](Id){return std::uint8_t{5u};};
        q.hasBrush=[](Id){return false;};
        q.isA=[](Id,QuestVr::OriginalMoveActorClass c){return c==QuestVr::OriginalMoveActorClass::Pawn;};
        q.radius=[](Id){return 1.0f;};q.height=[](Id){return 1.0f;};
        q.location=[&](Id a,unsigned i){const auto v=positions.at(a);return i==0?v.x:i==1?v.y:v.z;};
        q.writeLocation=[&](Id a,unsigned i,float v){auto& p=positions.at(a);if(i==0)p.x=v;else if(i==1)p.y=v;else p.z=v;};
        q.rotation=[&](Id a,unsigned i){return rotations.at(a).at(i);};
        q.writeRotation=[&](Id a,unsigned i,std::int32_t v){rotations.at(a).at(i)=v;};
        q.levelHash=[](Id){return Id{};};
        q.mark=[&]{++marks;return s.Mark(Root);};
        q.popMark=[&](Id token)noexcept{if(!s.Pop(token)||pops.size()==pops.capacity())std::terminate();pops.push_back(token);};
        q.multiLine=[&](QuestVr::OriginalMoveActorSweep sweep){
            Require(sweep.level==Root.level&&s.MarkDepth()==marks-pops.size(),"Query escaped retained native mark/root");
            auto local=List(3,marks==1u?42u:52u);local.records[0].time=.2f;local.records[1].time=.5f;local.records[2].time=.8f;
            const auto head=Adopt(s,std::move(local));issuedHeads.push_back(head);return head;
        };
        q.hitActor=[&](Id h){return s.Actor(Root,h);};q.hitNext=[&](Id h){return s.Next(Root,h);};
        q.hitTime=[&](Id h){return s.Time(Root,h);};q.hitPacket=[&](Id h){return s.Packet(Root,h);};
        q.basedOn=[](Id a,Id b){return a==b;};
        q.blockedBy=[](Id,Id a){return a==43u||a==53u;};
        q.levelActor0=[](Id){return Id{8u};};q.requireLevelInfo=[](Id a){Require(a==8u,"Wrong mock LevelInfo");};
        q.bump=[&](Id,Id){++bumpCalls;};
        q.touching=[](Id,unsigned){return Id{};};
        q.beginTouch=[&](Id a,Id b){touches.emplace_back(a,b);};
        q.setZone=[&](Id,Id a,std::int32_t test,std::int32_t force){
            Require(!test&&!force,"Composed store movement changed zone flags");const auto& hit=a==1u?parent:child;
            Require(hit.next&&s.Actor(Root,hit.next)==(a==1u?44u:54u),"Caller full packet lost global Next while its mark was live");
        };
        q.carryChildren=[&](Id level,Id source,PortableModelVec3,std::array<std::int32_t,3u>,std::int32_t){
            if(source!=1u)return;
            Require(s.MarkDepth()==1u&&s.Actor(Root,parent.next)==44u,"Parent list absent before recursive move");
            Require(QuestVr::MoveOriginalLevelActor(level,2u,{10,0,0},{},child,{},q),"Nested actual coordinator refused");
            Require(s.MarkDepth()==1u&&s.Actor(Root,parent.next)==44u,"Child Pop invalidated retained parent hit packet");
            Refuse([&]{s.Packet(Root,child.next);});
        };
        bool failed{};try{Require(QuestVr::MoveOriginalLevelActor(Root.level,1u,{10,0,0},{},parent,{},q),"Parent actual coordinator refused");}
        catch(const std::exception&){failed=true;}
        Require(!s.MarkDepth()&&!s.ArenaCount()&&!s.PayloadBytes(),"Coordinator refusal/success stranded retained arenas or marks");
        for(const auto head:issuedHeads)Refuse([&]{s.Packet(Root,head);});
        if(limit==std::numeric_limits<std::size_t>::max()){
            Require(!failed&&marks==2u&&pops.size()==2u&&issuedHeads.size()==2u&&issuedHeads[0]!=issuedHeads[1],
                "Full composed coordinator/store did not complete two independent query/mark lifetimes");
            Require(touches.size()==2u&&touches[0u]==std::pair<Id,Id>{2u,52u}&&
                touches[1u]==std::pair<Id,Id>{1u,42u}&&bumpCalls==4u,
                "Nested retained lists changed original parent/child bump and touch recipients");
            Require(positions[1u].x==4.0f&&positions[2u].x==4.0f&&parent.actor==43u&&child.actor==53u,
                "Composed store changed full coordinator blocking/backoff publication");
        }else Require(failed&&work==limit+1u,"Nested retained movement reset shared work or returned truncated success");
        return work;
    };
    const auto work=run(std::numeric_limits<std::size_t>::max());
    for(std::size_t limit=0;limit<work;++limit)run(limit);
}
}
int main(){try{BasicAndRaw();NestedAndCycles();AtomicRefusal();LimitsAndNoAlloc();CoordinatorComposition();
    std::cout<<"PASS native retained hit marks "<<checks<<" checks / "<<refusals
        <<" refusals. Host storage only: no Move266/runtime/Training physics claim.\n";return 0;
}catch(const std::exception& e){forbidAllocations=false;std::cerr<<"Native retained hit marks failed: "<<e.what()<<'\n';return 1;}}
