#include "quest_static_find_object.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace AllocationFault {
// This executable owns the allocator override; the production kernel has no
// injection hook. Enable only around one operation and disable before checks.
thread_local std::size_t remaining=std::numeric_limits<std::size_t>::max();
void Disable() noexcept {remaining=std::numeric_limits<std::size_t>::max();}
}
void* operator new(std::size_t bytes) {
    if(AllocationFault::remaining!=std::numeric_limits<std::size_t>::max()) {
        if(!AllocationFault::remaining)throw std::bad_alloc();
        --AllocationFault::remaining;
    }
    if(auto* memory=std::malloc(bytes?bytes:1u))return memory;
    throw std::bad_alloc();
}
// GCC's optimized mismatched-new/delete diagnostic cannot see that this test's
// replacement new is malloc-backed if it only inlines the delete half. Keep
// the genuine matching test-only deallocator boundary; do not disable warnings.
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}

namespace {
namespace Lookup=QuestVr::ObjectLookup;
std::size_t checks{},refusals{};
void Require(bool value,const char* why) {++checks;if(!value)throw std::runtime_error(why);}
template<class Function>void Refused(Function&& function,const char* why) {
    bool refused{};try {function();}catch(const std::exception&){refused=true;}
    Require(refused,why);++refusals;
}
auto Accept=[](std::uintptr_t){return true;};
Lookup::Record Object(std::uintptr_t identity,std::uint32_t index,int name,
    std::uintptr_t outer=0u,std::uint32_t outerIndex=0u) {return {identity,outer,index,outerIndex,name};}

void MembershipAndOrder() {
    Lookup::Index index(16u);
    index.Publish(Object(101u,1u,7));index.Publish(Object(102u,2u,7));index.Publish(Object(103u,3u,7));
    Require(index.Exact(0u,0u,7,Accept)==103u,"Newest hash did not win exact lookup");
    Require(index.Any(7,Accept)==101u,"Fallback did not start at lowest actual index");
    Require(index.Exact(0u,0u,7,[](auto identity){return identity!=103u;})==102u,"Rejected class stopped bucket traversal");
    Require(index.Rehash(1u,101u,0u,0u,7),"Same-name rehash refused");
    Require(index.Exact(0u,0u,7,Accept)==101u,"Same-name rehash did not change hash precedence");
    Require(index.Any(7,Accept)==101u,"Rehash changed indexed fallback order");
    Lookup::Index copy(index);
    Require(copy.Exact(0u,0u,7,Accept)==101u,"Copy reconstructed hash order from slots");
    Require(copy.Remove(1u,101u),"Copy head removal failed");
    Require(copy.Exact(0u,0u,7,Accept)==103u,"Copy links still referenced source nodes");
    Require(index.Exact(0u,0u,7,Accept)==101u,"Copy mutation changed source chain");
    Lookup::Index assigned;assigned=index;
    Require(assigned.Exact(0u,0u,7,Accept)==101u,"Copy assignment lost rehash order");
    const auto* assignedAlias=&assigned;assigned=*assignedAlias;
    Require(assigned.Exact(0u,0u,7,Accept)==101u,"Self assignment changed ordering");
    Lookup::Index moved(std::move(copy));
    Require(moved.Exact(0u,0u,7,Accept)==103u&&copy.Size()==0u,"Move left duplicate live weak state");
    copy.Publish(Object(109u,9u,9));copy=std::move(moved);
    Require(copy.Exact(0u,0u,7,Accept)==103u&&moved.Size()==0u,"Move assignment did not replace old state");
    Require(index.Remove(3u,103u),"Middle removal failed");
    Require(index.Exact(0u,0u,7,[](auto identity){return identity!=101u;})==102u,"Middle unlink changed remaining chain");
    Require(index.Remove(2u,102u),"Tail removal failed");
    Require(index.Remove(1u,101u)&&index.Exact(0u,0u,7,Accept)==0u,"Final head unlink retained stale pointer");
    Require(!index.Remove(1u,101u)&&!index.Remove(0u,101u),"Absent removal succeeded");

    // Same bucket, different actual name and different actual Outer.
    index.Publish(Object(111u,11u,1));
    index.Publish(Object(112u,12u,4097));
    index.Publish(Object(113u,13u,3,900u,2u));
    Require(index.Exact(0u,0u,1,Accept)==111u,"Hash collision was mistaken for actual Name/Outer match");
    Require(index.Exact(900u,2u,3,Accept)==113u,"Actual Outer collision lookup failed");
    Require(index.Exact(901u,2u,3,Accept)==0u,"Equal Outer index replaced pointer identity");
    Require(index.Remove(12u,112u)&&index.Exact(0u,0u,1,Accept)==111u,"Colliding middle unlink lost unrelated node");
    Require(index.Remove(11u,111u)&&index.Remove(13u,113u),"Colliding chain cleanup failed");

    index.Publish(Object(301u,3u,8));index.Publish(Object(302u,8u,8));
    Require(index.Remove(3u,301u),"Replacement release failed");index.Publish(Object(303u,3u,8));
    Require(!index.Remove(3u,301u)&&index.RecordAt(3u)->identity==303u,"Stale cleanup removed reused index");
    Require(index.Exact(0u,0u,8,Accept)==303u&&index.Any(8,Accept)==303u,"Slot reuse changed either ordering authority");
    Require(index.Rehash(8u,302u,0u,0u,8),"Higher index rehash failed");
    Require(index.Exact(0u,0u,8,Accept)==302u&&index.Any(8,Accept)==303u,"Hash order collapsed into index order");
    const auto records=index.RecordsAscending();
    Require(records.size()==2u&&records[0].index==3u&&records[1].index==8u,"Ascending snapshot order differs");
    std::vector<Lookup::Record> visited;index.ForEachAscending([&](const auto& record){visited.push_back(record);});
    Require(records==visited,"Ascending visitor and snapshot disagree");
    Require(index.RecordForIdentity(302u)==index.RecordAt(8u)&&!index.RecordForIdentity(999u),"Identity index differs from actual slot");
    Require(index.RetainedBytes()>=sizeof(Lookup::Index)+records.size()*sizeof(Lookup::Record),"Weak index bytes omitted allocation storage");
    Lookup::Index other;other.Publish(Object(500u,1u,50));index.Swap(other);
    Require(index.Size()==1u&&index.Exact(0u,0u,50,Accept)==500u&&other.Exact(0u,0u,8,Accept)==302u,"Swap lost actual bucket state");
}

void RefusalAndCapacity() {
    Refused([]{Lookup::Index bad(0u);},"Zero live capacity accepted");
    Refused([]{Lookup::Index bad(Lookup::Index::MaximumObjects+1u);},"Excessive live capacity accepted");
    Lookup::Index index(2u);
    index.Publish(Object(11u,Lookup::Index::MaximumObjectIndex,1));
    index.Publish(Object(12u,Lookup::Index::MaximumObjectIndex-1u,1));
    Require(index.Any(1,Accept)==12u,"Live limit incorrectly limited numeric index");
    const auto stable=index.RecordsAscending();const auto before=index.Exact(0u,0u,1,Accept);
    const std::vector<Lookup::Record> invalid{
        Object(0u,1u,1),Object(21u,0u,1),Object(21u,Lookup::Index::MaximumObjectIndex+1u,1),
        Object(21u,1u,-1),Object(21u,1u,1,22u,0u),Object(21u,1u,1,0u,2u),
        Object(21u,1u,1,21u,2u),Object(21u,1u,1,22u,1u),
        Object(21u,1u,1,22u,Lookup::Index::MaximumObjectIndex+1u),
        Object(21u,Lookup::Index::MaximumObjectIndex,1),Object(11u,1u,1)};
    for(const auto& record:invalid) {
        Refused([&]{index.Publish(record);},"Invalid publication was accepted");
        Require(index.RecordsAscending()==stable&&index.Exact(0u,0u,1,Accept)==before,"Refusal changed weak membership/hash order");
    }
    Refused([&]{index.Publish(Object(30u,3u,3));},"Live capacity was ignored");
    Require(index.RecordsAscending()==stable,"Capacity refusal partially published an identity");
    Require(!index.Rehash(Lookup::Index::MaximumObjectIndex,99u,0u,0u,3),"Stale identity rehash succeeded");
    Require(!index.Rehash(Lookup::Index::MaximumObjectIndex,11u,11u,2u,3),"Self Outer rehash succeeded");
    Require(index.RecordsAscending()==stable&&index.Exact(0u,0u,1,Accept)==before,"Invalid rehash changed state");
    Refused([&]{(void)index.Exact(1u,0u,1,Accept);},"Incomplete Outer search authority accepted");
    Refused([&]{(void)index.Exact(0u,0u,-1,Accept);},"Negative exact Name accepted");
    Refused([&]{(void)index.Any(-1,Accept);},"Negative fallback Name accepted");
    Require(index.Exact(0u,0u,0,[](auto)->bool{throw std::runtime_error("None callback");})==0u,"None Name called predicate");
    Require(index.Any(0,[](auto)->bool{throw std::runtime_error("None callback");})==0u,"None fallback called predicate");
    Refused([&]{(void)index.Exact(0u,0u,1,[](auto)->bool{throw std::runtime_error("Class refusal");});},"Exact callback refusal swallowed");
    Refused([&]{(void)index.Any(1,[](auto)->bool{throw std::runtime_error("Class refusal");});},"Fallback callback refusal swallowed");
    Require(index.RecordsAscending()==stable,"Read-only callback refusal changed publication");
}

void AllocationFailureAtomicity() {
    Lookup::Index index;index.Publish(Object(101u,1u,1));index.Publish(Object(102u,2u,1));
    Require(index.Rehash(1u,101u,0u,0u,1),"Allocation fixture rehash failed");
    const auto stable=index.RecordsAscending();const auto bytes=index.RetainedBytes();
    for(std::size_t allowed=0u;allowed<2u;++allowed) {
        bool failed{};AllocationFault::remaining=allowed;
        try {index.Publish(Object(103u,3u,1));}catch(const std::bad_alloc&){failed=true;}
        AllocationFault::Disable();
        Require(failed,"Expected map-node publication allocation did not fail");++refusals;
        Require(index.RecordsAscending()==stable&&index.RetainedBytes()==bytes&&
            index.Exact(0u,0u,1,Accept)==101u&&!index.RecordForIdentity(103u),"Publication allocation failure changed weak state");
    }
    // A subsequent successful publish proves no stale secondary identity entry.
    index.Publish(Object(103u,3u,1));Require(index.Remove(3u,103u),"Failure rollback leaked identity reservation");
    Lookup::Index destination;destination.Publish(Object(500u,5u,5));
    const auto oldDestination=destination.RecordsAscending();
    // Two records require two slots-tree plus two identities-tree nodes. Each
    // possible map allocation failure must leave both source and target intact.
    for(std::size_t allowed=0u;allowed<4u;++allowed) {
        bool failed{};AllocationFault::remaining=allowed;
        try {destination=index;}catch(const std::bad_alloc&){failed=true;}
        AllocationFault::Disable();
        Require(failed,"Expected copied map-node allocation did not fail");++refusals;
        Require(destination.RecordsAscending()==oldDestination&&destination.Exact(0u,0u,5,Accept)==500u,
            "Copy assignment allocation failure changed destination");
        Require(index.RecordsAscending()==stable&&index.Exact(0u,0u,1,Accept)==101u,
            "Copy assignment allocation failure changed source");
    }
    destination=index;
    Require(destination.Exact(0u,0u,1,Accept)==101u,"Copy after allocation failures lost hash ordering");
}

void StagedAdoption() {
    Lookup::Index live(8u);live.Publish(Object(11u,5u,7));live.Publish(Object(12u,6u,7));
    Lookup::Index staged;staged.Publish(Object(21u,4u,7));staged.Publish(Object(22u,2u,7));
    Require(staged.Rehash(4u,21u,0u,0u,7),"Staged same-name rehash failed");
    staged.Publish(Object(23u,3u,4097));
    const auto* address=staged.RecordAt(4u);
    bool adopted{};AllocationFault::remaining=0u;
    const auto admitted=live.CanAdopt(staged);if(admitted) adopted=live.Adopt(staged);
    AllocationFault::Disable();
    Require(admitted&&adopted&&staged.Size()==0u&&live.Size()==5u,"Detached records did not transfer without allocation");
    Require(live.RecordAt(4u)==address,"Node transfer changed stored record addresses");
    Require(live.Exact(0u,0u,7,Accept)==21u&&live.Any(7,Accept)==22u,"Adoption collapsed hash order into slot order");
    Require(live.Remove(4u,21u)&&live.Exact(0u,0u,7,Accept)==22u,"Adopted head removal lost staged successor");
    Require(live.Remove(2u,22u)&&live.Exact(0u,0u,7,Accept)==12u,"Adopted tail removal lost original live head");
    Require(staged.Exact(0u,0u,7,Accept)==0u,"Moved staging buckets retained stale nodes");
    staged.Publish(Object(31u,1u,7));Require(live.Adopt(staged),"Emptied staging index could not be reused");
    Require(live.Exact(0u,0u,7,Accept)==31u,"Reused staging publication was not newest");
    const auto stable=live.RecordsAscending();
    for(const auto record:{Object(41u,5u,9),Object(12u,7u,9)}) {
        Lookup::Index conflict;conflict.Publish(record);const auto before=conflict.RecordsAscending();
        Require(!live.CanAdopt(conflict)&&!live.Adopt(conflict),"Occupied actual index or identity was adopted");
        Require(live.RecordsAscending()==stable&&conflict.RecordsAscending()==before,
            "Refused staged adoption changed either live or staged records");
    }
    Lookup::Index full(1u);full.Publish(Object(50u,1u,1));Lookup::Index extra;extra.Publish(Object(51u,2u,1));
    Require(!full.CanAdopt(extra)&&!full.Adopt(extra)&&extra.Size()==1u,"Adoption exceeded live object capacity");
    Require(!live.CanAdopt(live)&&!live.Adopt(live)&&live.RecordsAscending()==stable,"Self-adoption changed weak state");
    Lookup::Index empty;Require(live.Adopt(empty)&&live.RecordsAscending()==stable,"Empty adoption changed weak state");
}

struct Fixture {
    static constexpr std::uintptr_t objectClass=100u,packageClass=101u,packageSubclass=102u,
        itemClass=103u,childItemClass=104u;
    Lookup::Index index;
    std::map<std::string,int,std::less<>> names{{"Core",1},{"Engine",2},{"Container",3},{"Mixed",4},
        {"Leaf",5},{"Nested",6},{"Missing",7},{"Classless",8},{"None",0},{"Cycle",9}};
    std::map<std::uintptr_t,std::uintptr_t> actualClass;
    std::map<std::uintptr_t,std::uintptr_t> super{{objectClass,0u},{packageClass,objectClass},
        {packageSubclass,packageClass},{itemClass,objectClass},{childItemClass,itemClass}};
    std::vector<std::string> nameCalls;
    std::vector<std::pair<std::uintptr_t,std::uintptr_t>> classCalls;
    void Add(std::uintptr_t identity,std::uint32_t slot,int name,std::uintptr_t cls,
        std::uintptr_t outer=0u,std::uint32_t outerSlot=0u) {
        index.Publish(Object(identity,slot,name,outer,outerSlot));actualClass.emplace(identity,cls);
    }
    std::optional<int> Name(std::string_view name) {
        nameCalls.emplace_back(name);const auto found=names.find(name);
        return found==names.end()?std::nullopt:std::optional<int>(found->second);
    }
    bool Matches(std::uintptr_t identity,std::uintptr_t requested,bool exact) {
        classCalls.emplace_back(identity,requested);
        const auto found=actualClass.find(identity);
        if(found==actualClass.end()||!found->second)throw std::runtime_error("Missing actual Class authority");
        auto current=found->second;if(exact)return current==requested;
        std::vector<std::uintptr_t> visited;
        while(current) {
            if(std::find(visited.begin(),visited.end(),current)!=visited.end())throw std::runtime_error("Actual Super cycle");
            visited.push_back(current);if(current==requested)return true;
            const auto base=super.find(current);if(base==super.end())throw std::runtime_error("Unknown actual Super");
            current=base->second;
        }
        return false;
    }
    std::uintptr_t Find(std::string_view name,std::uintptr_t requested=0u,bool any=false,bool exact=false,
        std::uintptr_t outer=0u,std::uint32_t outerSlot=0u) {
        return Lookup::Find(index,{requested,outer,outerSlot,any,exact,name},objectClass,packageClass,
            [&](auto part){return Name(part);},[&](auto identity,auto cls,bool isExact){return Matches(identity,cls,isExact);});
    }
};

void Resolution() {
    Fixture fixture;
    fixture.Add(1000u,10u,1,Fixture::packageClass);
    fixture.Add(2000u,20u,2,Fixture::packageSubclass);
    fixture.Add(3000u,30u,3,Fixture::itemClass);
    fixture.Add(4000u,40u,4,Fixture::packageClass);
    fixture.Add(4001u,41u,4,Fixture::itemClass); // newer hash than Package
    fixture.Add(5000u,50u,5,Fixture::itemClass,4000u,40u);
    fixture.Add(5001u,51u,5,Fixture::itemClass,4001u,41u);
    fixture.Add(6000u,60u,6,Fixture::itemClass,3000u,30u);
    fixture.Add(6001u,61u,5,Fixture::childItemClass,6000u,60u);
    fixture.Add(7000u,70u,5,Fixture::itemClass,2000u,20u);
    fixture.Add(7001u,71u,5,Fixture::childItemClass,2000u,20u);
    Require(fixture.Find("Mixed.Leaf")==5000u,"ResolveName did not prefer actual Package over newer nonpackage");
    Require(fixture.Find("Container.Nested.Leaf")==6001u,"Actual non-Package prefix was not resolved");
    Require(fixture.Find("Engine.Leaf",Fixture::itemClass)==7001u,"Derived class hash filter failed");
    Require(fixture.Find("Engine.Leaf",Fixture::itemClass,false,true)==7000u,"Exact concrete class filter admitted a subclass");
    Require(fixture.Find("Leaf",Fixture::itemClass,false,false,2000u,20u)==7001u,"Supplied actual Outer was ignored");
    Require(fixture.Find("Core.Leaf",Fixture::itemClass,true)==5000u,"Qualified ANY fallback incorrectly enforced resolved Outer");
    Require(fixture.Find("Missing.Leaf",Fixture::itemClass,true)==0u,"Missing prefix fell back globally to leaf");
    const auto count=fixture.names.size();
    Require(fixture.Find("Unknown.Leaf",0u,true)==0u&&fixture.names.size()==count,"Unknown FName was interned or leaf fallback ran");
    Require(fixture.Find("Engine.Unknown",0u,true)==0u&&fixture.names.size()==count,"Unknown leaf FName was interned");
    Require(fixture.Find("None")==0u,"None FName resolved an object");
    fixture.Add(8000u,80u,5,Fixture::itemClass);
    Require(fixture.Find("Leaf",Fixture::itemClass,true)==8000u,"Null-Outer exact hash did not precede ascending ANY fallback");
    Require(fixture.index.Remove(80u,8000u),"Null-Outer fixture removal failed");
    Require(fixture.Find("Leaf",Fixture::itemClass,true)==5000u,"ANY fallback did not use lowest live slot");
    fixture.Add(9000u,5u,5,Fixture::itemClass,1000u,10u);
    Require(fixture.Find("Leaf",Fixture::itemClass,true)==9000u,"Lower-index later publication did not win indexed fallback");
    Require(fixture.index.Rehash(5u,9000u,2000u,20u,5),"Fixture Outer rehash failed");
    Require(fixture.Find("Engine.Leaf",Fixture::itemClass)==9000u,"Actual Outer rehash did not change exact selection");

    // Classless exact/null constraint is intentionally unlike ANY admission.
    Fixture classless;classless.Add(11u,1u,8,0u);
    Require(classless.Find("Classless")==11u&&classless.classCalls.empty(),"Null requested exact phase read Class");
    classless.Add(22u,2u,1,Fixture::packageClass);
    Require(classless.index.Rehash(1u,11u,22u,2u,8),"Classless fixture rehash failed");
    const auto stable=classless.index.RecordsAscending();
    Refused([&]{(void)classless.Find("Classless",0u,true);},"Null requested ANY phase skipped Core.Object admission");
    Require(classless.index.RecordsAscending()==stable,"Class authority refusal changed weak index");
    Fixture irrelevant;irrelevant.Add(11u,1u,8,0u);irrelevant.Add(22u,2u,5,Fixture::itemClass,99u,99u);
    Require(irrelevant.Find("Leaf",0u,true)==22u,"Irrelevant-name metadata blocked bounded pure lookup");
    Require(irrelevant.classCalls.size()==1u&&irrelevant.classCalls.front().first==22u,"Name-first optimization admitted irrelevant names");
    Fixture cycle;cycle.Add(11u,1u,9,500u,99u,99u);cycle.super.emplace(500u,501u);cycle.super.emplace(501u,500u);
    Refused([&]{(void)cycle.Find("Cycle",0u,true);},"Actual Super cycle was swallowed by fallback");
    Require(cycle.index.Size()==1u,"Class callback cycle refusal changed registry");
}

void MalformedQueries() {
    Fixture fixture;fixture.Add(1000u,10u,1,Fixture::packageClass);
    fixture.Add(2000u,20u,5,Fixture::itemClass,1000u,10u);
    const auto stable=fixture.index.RecordsAscending();
    std::vector<std::string> bad{"",".",".Core","Core.","Core..Leaf","ini:Engine.Leaf","USR:Engine.Leaf",
        std::string("Core\0.Leaf",10u),std::string(Lookup::MaximumInputBytes+1u,'a')};
    std::string tooMany="Core";for(std::size_t i=1u;i<=Lookup::MaximumSegments;++i)tooMany+=".Core";bad.push_back(tooMany);
    for(const auto& name:bad) {
        fixture.nameCalls.clear();
        Refused([&]{(void)fixture.Find(name);},"Malformed or unsupported lookup profile accepted");
        Require(fixture.nameCalls.empty()&&fixture.index.RecordsAscending()==stable,"Malformed query ran a resolver or changed state");
    }
    Refused([&]{(void)fixture.Find("Leaf",0u,true,false,1000u,10u);},"ANY_PACKAGE accepted nonnull initial Outer");
    Refused([&]{(void)fixture.Find("Leaf",0u,false,false,1000u,0u);},"Query accepted incomplete actual Outer");
    fixture.nameCalls.clear();
    Refused([&]{(void)fixture.Find("Unknown",0u,false,false,1000u,Lookup::Index::MaximumObjectIndex+1u);},"Unknown name hid invalid Outer index");
    Require(fixture.nameCalls.empty(),"Invalid Outer index reached name resolver");
    Refused([&]{(void)Lookup::Find(fixture.index,{0u,0u,0u,false,false,"Core.Leaf"},0u,Fixture::packageClass,
        [&](auto part){return fixture.Name(part);},[&](auto identity,auto cls,bool exact){return fixture.Matches(identity,cls,exact);});},"Missing actual Object class token accepted");
    Refused([&]{(void)Lookup::Find(fixture.index,{0u,0u,0u,false,false,"Core.Leaf"},Fixture::objectClass,Fixture::packageClass,
        [](auto)->std::optional<int>{throw std::runtime_error("Name authority refusal");},[](auto,auto,bool){return true;});},"Name resolver exception swallowed");
    Refused([&]{(void)Lookup::Find(fixture.index,{0u,0u,0u,false,false,"Core.Leaf"},Fixture::objectClass,Fixture::packageClass,
        [](auto)->std::optional<int>{return -1;},[](auto,auto,bool){return true;});},"Negative resolver Name admitted");
    Require(fixture.index.RecordsAscending()==stable,"Query refusal changed indexed authority");
}

// Independent vector oracle exercises publication/removal/rehash, including
// hash collisions. Its exact ordering is an explicit event sequence, never a
// read of kernel links/buckets or a second use of the kernel's implementation.
void DifferentialSequence() {
    struct Expected {Lookup::Record record;std::uint64_t lastHash;};
    Lookup::Index index(128u);std::vector<Expected> oracle;std::uint64_t serial{};
    std::uint32_t random=0x4729ace1u;
    auto next=[&]{random=random*1664525u+1013904223u;return random;};
    for(unsigned step=0u;step<4000u;++step) {
        const auto slot=next()%96u+1u;const auto found=std::find_if(oracle.begin(),oracle.end(),[&](const auto& entry){return entry.record.index==slot;});
        if(found==oracle.end()) {
            const auto name=static_cast<int>(next()%4u+1u)+(next()%2u?4096:0);
            const auto outer=next()%3u;const auto record=Object(10000u+slot,slot,name,outer?9000u+outer:0u,outer?100u+outer:0u);
            index.Publish(record);oracle.push_back({record,++serial});
        }else if(next()%3u==0u) {
            Require(index.Remove(slot,found->record.identity),"Differential live removal failed");oracle.erase(found);
        }else {
            const auto name=static_cast<int>(next()%4u+1u)+(next()%2u?4096:0);const auto outer=next()%3u;
            const auto record=Object(found->record.identity,slot,name,outer?9000u+outer:0u,outer?100u+outer:0u);
            Require(index.Rehash(slot,record.identity,record.outerIdentity,record.outerIndex,record.nameIndex),"Differential live rehash failed");
            *found={record,++serial};
        }
        for(int name:{1,2,3,4,4097,4098,4099,4100})for(std::uint32_t outer=0u;outer<3u;++outer) {
            const auto identity=outer?9000u+outer:0u;const auto outerSlot=outer?100u+outer:0u;
            const Expected* exact{};const Expected* any{};
            for(const auto& entry:oracle)if(entry.record.nameIndex==name&&entry.record.identity%3u!=0u) {
                if(!any||entry.record.index<any->record.index)any=&entry;
                if(entry.record.outerIdentity==identity&&(!exact||entry.lastHash>exact->lastHash))exact=&entry;
            }
            const auto predicate=[](auto candidate){return candidate%3u!=0u;};
            Require(index.Exact(identity,outerSlot,name,predicate)==(exact?exact->record.identity:0u),"Independent hash-order sequence mismatch");
            Require(index.Any(name,predicate)==(any?any->record.identity:0u),"Independent ascending-index sequence mismatch");
        }
        if(step%97u==0u) {
            Lookup::Index copied(index);
            for(int name:{1,2,3,4,4097,4098,4099,4100})
                Require(copied.Exact(0u,0u,name,Accept)==index.Exact(0u,0u,name,Accept),"Sequence copy lost hash ordering");
        }
    }
}
}

int main() {
    try {
        MembershipAndOrder();RefusalAndCapacity();AllocationFailureAtomicity();StagedAdoption();Resolution();MalformedQueries();DifferentialSequence();
        std::cout<<"Actual weak object lookup: "<<checks<<" checks / "<<refusals<<" refusals\n";return 0;
    }catch(const std::exception& error) {std::cerr<<"Object lookup test failed: "<<error.what()<<'\n';return 1;}
}
