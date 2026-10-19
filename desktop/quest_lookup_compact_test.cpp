#include "quest_static_find_object.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Synthetic opaque identities only: no UObject, map, DLL or game asset is
// loaded. The reference owns a chronological vector, not intrusive links,
// hash heads or production selection/accounting helpers.
namespace CompactLookupAllocationFault {
thread_local std::size_t remaining=std::numeric_limits<std::size_t>::max();
thread_local std::size_t attempts{};
void Arm(std::size_t allowed) noexcept {remaining=allowed;attempts=0u;}
void Disable() noexcept {remaining=std::numeric_limits<std::size_t>::max();}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void* Allocate(std::size_t bytes) {
    if (remaining!=std::numeric_limits<std::size_t>::max()) {
        ++attempts;if (!remaining) throw std::bad_alloc();--remaining;
    }
    if (auto* memory=std::malloc(bytes?bytes:1u)) return memory;
    throw std::bad_alloc();
}
}
void* operator new(std::size_t bytes) {return CompactLookupAllocationFault::Allocate(bytes);}
void* operator new[](std::size_t bytes) {return CompactLookupAllocationFault::Allocate(bytes);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* memory) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* memory,std::size_t) noexcept {std::free(memory);}

namespace {
namespace Lookup=QuestVr::ObjectLookup;
using Record=Lookup::Record;
std::size_t checks{},mixedOperations{},faultCases{},nonallocCases{},refusals{};
void Require(bool okay,const std::string& reason) {++checks;if (!okay) throw std::runtime_error(reason);}
template<class Operation> void Refuse(Operation operation) {
    bool refused{};try {operation();} catch (const std::exception&) {refused=true;}
    Require(refused,"Invalid compact lookup operation was accepted");++refusals;
}
template<class Operation> void NoAllocate(Operation operation) {
    CompactLookupAllocationFault::Arm(0u);
    try {operation();} catch (...) {CompactLookupAllocationFault::Disable();throw;}
    const auto attempts=CompactLookupAllocationFault::attempts;CompactLookupAllocationFault::Disable();
    Require(attempts==0u,"Nonallocating compact lookup operation attempted allocation");++nonallocCases;
}

// Every pair of successive twelve-bit groups cancels in the identity hash.
// On64-bit hosts ALL fixture identities have the same low32 bits but distinct
// high32 bits. A32-bit link must never become32-bit UObject authority.
std::uintptr_t Identity(std::uint32_t serial) {
    const auto wide=(static_cast<std::uint64_t>(serial)<<48u)|
        (static_cast<std::uint64_t>(serial)<<36u)|0x2002ull;
    return sizeof(std::uintptr_t)==8u?static_cast<std::uintptr_t>(wide):static_cast<std::uintptr_t>(serial)*4097u;
}
std::uintptr_t Outer(std::uint32_t index) {
    if (!index) return 0u;
    return sizeof(std::uintptr_t)==8u?static_cast<std::uintptr_t>(0xa5b6000000000000ull|index):
        static_cast<std::uintptr_t>(0xa0000000u|index);
}
Record Object(std::uint32_t serial,std::uint32_t index,unsigned group=0u) {
    constexpr std::array<std::uint32_t,4u> outerIndices{0u,0u,4096u,1u};
    constexpr std::array<int,4u> names{77,4173,77,76};
    const auto selected=group%4u;
    return {Identity(serial),Outer(outerIndices[selected]),index,outerIndices[selected],names[selected]};
}
bool Accept(std::uintptr_t identity,unsigned policy) {
    if (policy==0u) return true;
    if (policy==1u) return false;
    const auto wide=static_cast<std::uint64_t>(identity);
    return ((wide>>48u)^(wide>>12u)^wide)%3u==0u;
}

struct Oracle {
    std::uint32_t capacity{32u};
    std::map<std::uint32_t,Record> ascending;
    std::vector<std::uint32_t> newest;
    explicit Oracle(std::uint32_t maximum=32u):capacity(maximum) {}
    const Record* At(std::uint32_t index) const {
        const auto found=ascending.find(index);return found==ascending.end()?nullptr:&found->second;
    }
    const Record* ByIdentity(std::uintptr_t identity) const {
        for (const auto& [index,record]:ascending) {(void)index;if (record.identity==identity) return &record;}
        return nullptr;
    }
    bool CanPublish(const Record& record) const {
        return ascending.size()<capacity&&!At(record.index)&&!ByIdentity(record.identity);
    }
    void Publish(Record record) {
        Require(CanPublish(record),"Independent fixture published a duplicate/full record");
        ascending.emplace(record.index,record);newest.insert(newest.begin(),record.index);
    }
    bool Remove(std::uint32_t index,std::uintptr_t identity) {
        const auto* record=At(index);
        if (!record||!identity||record->identity!=identity) return false;
        ascending.erase(index);newest.erase(std::find(newest.begin(),newest.end(),index));return true;
    }
    bool Rehash(std::uint32_t index,std::uintptr_t identity,std::uintptr_t outer,std::uint32_t outerIndex,int name) {
        auto found=ascending.find(index);
        if (found==ascending.end()||found->second.identity!=identity||!identity||name<0||outerIndex>4'000'000u||
            ((outer==0u)!=(outerIndex==0u))||identity==outer||(outer&&index==outerIndex)) return false;
        found->second={identity,outer,index,outerIndex,name};
        newest.erase(std::find(newest.begin(),newest.end(),index));newest.insert(newest.begin(),index);return true;
    }
    bool CanAdopt(const Oracle& other) const {
        if (this==&other||other.ascending.size()>capacity-ascending.size()) return false;
        for (const auto& [index,record]:other.ascending) if (At(index)||ByIdentity(record.identity)) return false;
        return true;
    }
    bool Adopt(Oracle& other) {
        if (!CanAdopt(other)) return false;
        ascending.insert(other.ascending.begin(),other.ascending.end());
        newest.insert(newest.begin(),other.newest.begin(),other.newest.end());
        other.ascending.clear();other.newest.clear();return true;
    }
    std::vector<Record> Records() const {
        std::vector<Record> out;for (const auto& [index,record]:ascending) {(void)index;out.push_back(record);}return out;
    }
    std::pair<std::uintptr_t,std::vector<std::uintptr_t>> Exact(std::uintptr_t outer,std::uint32_t outerIndex,int name,unsigned policy) const {
        std::vector<std::uintptr_t> called;
        const auto selectedBucket=(outerIndex%4096u)^(static_cast<std::uint32_t>(name)%4096u);
        if (name) for (const auto index:newest) {
            const auto& record=ascending.at(index);
            const auto recordBucket=(record.outerIndex%4096u)^(static_cast<std::uint32_t>(record.nameIndex)%4096u);
            if (recordBucket!=selectedBucket) continue;
            if (record.outerIdentity!=outer||record.nameIndex!=name) continue;
            called.push_back(record.identity);if (Accept(record.identity,policy)) return {record.identity,std::move(called)};
        }
        return {0u,std::move(called)};
    }
    std::pair<std::uintptr_t,std::vector<std::uintptr_t>> Any(int name,unsigned policy) const {
        std::vector<std::uintptr_t> called;
        if (name) for (const auto& [index,record]:ascending) {
            (void)index;if (record.nameIndex!=name) continue;
            called.push_back(record.identity);if (Accept(record.identity,policy)) return {record.identity,std::move(called)};
        }
        return {0u,std::move(called)};
    }
};

// A separately declared representation model uses only public Record and STL
// layout. The conservative allocator/tree allowance remains64B on64-bit, not
// an unmeasured reduction in accounting policy.
struct CompactNodeModel {Record record;std::uint32_t next{},identityNext{};};
struct PointerNodeModel {Record record;void *next{},*identityNext{};};
struct CompactOwnerModel {
    std::uint32_t maximum{};std::map<std::uint32_t,CompactNodeModel> slots;
    std::array<std::uint32_t,4096u> heads{},identityHeads{};
};
struct PointerOwnerModel {
    std::uint32_t maximum{};std::map<std::uint32_t,PointerNodeModel> slots;
    std::array<void*,4096u> heads{},identityHeads{};
};
constexpr auto CompactStride=sizeof(std::pair<const std::uint32_t,CompactNodeModel>)+8u*sizeof(void*);
constexpr auto PointerStride=sizeof(std::pair<const std::uint32_t,PointerNodeModel>)+8u*sizeof(void*);
std::size_t ExpectedBytes(std::size_t count) {return sizeof(CompactOwnerModel)+count*CompactStride;}
void Verify(const Lookup::Index& actual,const Oracle& expected) {
    Require(actual.Size()==expected.ascending.size(),"Compact lookup membership count differs from chronological oracle");
    Require(actual.RetainedBytes()==ExpectedBytes(expected.ascending.size()),"Retained accounting differs from independent compact-node/owner/tree model");
    Require(actual.RecordsAscending()==expected.Records(),"Compact lookup no longer iterates ascending sparse object indices");
    std::vector<Record> visited;actual.ForEachAscending([&](const Record& record){visited.push_back(record);});
    Require(visited==expected.Records(),"Ascending visitor disagrees with independent records");
    Require(!actual.RecordAt(0u)&&!actual.RecordAt(4'000'001u)&&!actual.RecordForIdentity(0u)&&!actual.RecordForIdentity(Identity(4000u)),
        "Missing/None/out-of-domain records acquired weak identity membership");
    std::vector<std::pair<std::uintptr_t,std::uint32_t>> outers{{0u,0u},{Outer(4096u),4096u},{Outer(1u),1u},
        {Outer(4096u),4097u},{Outer(1u),4097u},{Outer(11u),4096u}};
    std::vector<int> names{0,77,4173,76,8191};
    for (const auto& [index,record]:expected.ascending) {
        const auto* found=actual.RecordAt(index);
        Require(found&&*found==record&&found==actual.RecordForIdentity(record.identity),"RecordAt/RecordForIdentity narrowed identity or followed a foreign index link");
        if (std::find(outers.begin(),outers.end(),std::pair{record.outerIdentity,record.outerIndex})==outers.end())
            outers.emplace_back(record.outerIdentity,record.outerIndex);
        if (std::find(names.begin(),names.end(),record.nameIndex)==names.end()) names.push_back(record.nameIndex);
    }
    for (const auto& [outer,outerIndex]:outers) for (const auto name:names) for (unsigned policy=0u;policy<3u;++policy) {
        const auto predicted=expected.Exact(outer,outerIndex,name,policy);std::vector<std::uintptr_t> calls;
        const auto selected=actual.Exact(outer,outerIndex,name,[&](std::uintptr_t identity){calls.push_back(identity);return Accept(identity,policy);});
        Require(selected==predicted.first&&calls==predicted.second,"Exact selection/predicate order differs from independent newest-first history");
    }
    for (const auto name:names) for (unsigned policy=0u;policy<3u;++policy) {
        const auto predicted=expected.Any(name,policy);std::vector<std::uintptr_t> calls;
        const auto selected=actual.Any(name,[&](std::uintptr_t identity){calls.push_back(identity);return Accept(identity,policy);});
        Require(selected==predicted.first&&calls==predicted.second,"Any selection/predicate order differs from independent ascending sparse indices");
    }
}
void Publish(Lookup::Index& actual,Oracle& expected,const Record& record) {actual.Publish(record);expected.Publish(record);}
void Rehash(Lookup::Index& actual,Oracle& expected,std::uint32_t index) {
    const auto record=*expected.At(index);
    Require(actual.Rehash(index,record.identity,record.outerIdentity,record.outerIndex,record.nameIndex)==
        expected.Rehash(index,record.identity,record.outerIdentity,record.outerIndex,record.nameIndex),"Same-key rehash disagrees");
}

void RepresentationAndSparseIndices() {
    static_assert(Lookup::Index::MaximumObjectIndex==4'000'000u&&Lookup::Index::MaximumObjects==1'000'000u&&Lookup::Index::BucketCount==4096u);
    Require(sizeof(Lookup::Index)==sizeof(CompactOwnerModel),"Object lookup still owns pointer-sized heads or unaccounted representation fields");
    if (sizeof(void*)==8u) {
        Require(sizeof(PointerOwnerModel)-sizeof(CompactOwnerModel)==32'768u&&PointerStride-CompactStride==8u,
            "Compact representation did not physically remove32768 head bytes and8 bytes per live node on64-bit");
        Require(8u*sizeof(void*)==64u,"Tree allowance changed during representation compaction");
    }
    Lookup::Index actual(4u);Oracle expected(4u);Verify(actual,expected);
    for (const auto index:{4'000'000u,1u,3'999'999u,2'000'003u}) {
        Publish(actual,expected,Object(static_cast<std::uint32_t>(expected.ascending.size()+1u),index));Verify(actual,expected);
    }
    if (sizeof(void*)==8u) {
        const auto oldBytes=sizeof(PointerOwnerModel)+4u*PointerStride;
        Require(oldBytes-actual.RetainedBytes()==32'768u+4u*8u,"Retained reduction is not the actual owner/node representation saving");
    }
    const auto old=*expected.At(4'000'000u);
    Require(actual.Remove(old.index,old.identity)&&expected.Remove(old.index,old.identity),"Maximum sparse index removal failed");
    Publish(actual,expected,Object(19u,4'000'000u));
    Require(!actual.Remove(old.index,old.identity)&&!actual.Rehash(old.index,old.identity,0u,0u,1),"Stale identity altered a reused maximum index");
    Verify(actual,expected);
    Refuse([&]{actual.Publish(Object(20u,4'000'001u));});
    Refuse([&]{actual.Publish(Object(21u,0u));});
    Refuse([&]{actual.Publish(Object(22u,33u));});Verify(actual,expected);
}

void CopiesTransfersAndStableRecords() {
    Lookup::Index live(128u),staging(128u);Oracle liveOracle(128u),stagingOracle(128u);
    std::map<std::uint32_t,const Record*> addresses;
    std::vector<std::uint32_t> stagedIndices;
    for (std::uint32_t serial=1u;serial<=48u;++serial) {
        const auto record=Object(serial,4'000'000u-serial*4093u,(serial/2u)%4u);
        auto& target=serial%2u?live:staging;auto& oracle=serial%2u?liveOracle:stagingOracle;
        Publish(target,oracle,record);addresses.emplace(record.index,target.RecordAt(record.index));
        if (!(serial%2u)) stagedIndices.push_back(record.index);
    }
    // ALL name/Outer groups share a name-hash bucket and all opaque identities
    // collide in the other intrusive chain. Rehash creates order different
    // from publication, ascending indices, and the source tree-copy order.
    Rehash(live,liveOracle,4'000'000u-9u*4093u);Rehash(staging,stagingOracle,4'000'000u-18u*4093u);
    const auto oldLiveOracle=liveOracle;
    Verify(live,liveOracle);Verify(staging,stagingOracle);
    bool can{},adopted{};
    NoAllocate([&]{can=live.CanAdopt(staging);adopted=live.Adopt(staging);});
    Require(can&&adopted&&liveOracle.Adopt(stagingOracle),"Detached collision-heavy adoption was refused");
    Verify(live,liveOracle);Verify(staging,stagingOracle);
    for (const auto& [index,pointer]:addresses)
        Require(live.RecordAt(index)==pointer&&live.RecordForIdentity(pointer->identity)==pointer,"Adopt moved/reallocated a live or staged Record address");
    Publish(staging,stagingOracle,Object(90u,2'900'000u,2u));Verify(staging,stagingOracle);

    Lookup::Index copy(live);Oracle copyOracle=liveOracle;Verify(copy,copyOracle);
    for (const auto& [index,pointer]:addresses) Require(copy.RecordAt(index)!=pointer,"Copy retained source tree-node addresses");
    Lookup::Index assigned(1u);assigned.Publish(Object(91u,700u));assigned=live;Verify(assigned,liveOracle);
    const auto* self=&assigned;assigned=*self;Verify(assigned,liveOracle);
    const auto* movedPointer=copy.RecordAt(copyOracle.newest.front());
    const auto movedIndex=copyOracle.newest.front();
    Lookup::Index moved(std::move(copy));Verify(moved,copyOracle);Verify(copy,Oracle(128u));
    Require(moved.RecordAt(movedIndex)==movedPointer,"Move construction invalidated the stable map Record");
    Oracle reusedCopy(128u);Publish(copy,reusedCopy,Object(92u,2'800'000u,1u));Verify(copy,reusedCopy);
    copy=std::move(moved);Verify(copy,copyOracle);Verify(moved,Oracle(128u));
    Require(copy.RecordAt(movedIndex)==movedPointer,"Move assignment invalidated the stable transferred Record");
    auto* moveSelf=&copy;copy=std::move(*moveSelf);Verify(copy,copyOracle);
    const auto livePointers=addresses;
    NoAllocate([&]{copy.Swap(staging);});std::swap(copyOracle,stagingOracle);
    Verify(copy,copyOracle);Verify(staging,stagingOracle);
    Require(staging.RecordAt(movedIndex)==movedPointer,"Swap changed transferred Record addresses");
    for (const auto& [index,pointer]:livePointers) Require(live.RecordAt(index)==pointer,"Unrelated copy/move/swap mutated original Records");

    // Remove former staged head/middle/tail from a copied-and-transferred
    // collision chain. Each surviving exact order is checked, not just winner.
    const std::array removal{4'000'000u-18u*4093u,4'000'000u-24u*4093u,4'000'000u-2u*4093u};
    for (const auto index:removal) {
        const auto identity=stagingOracle.At(index)->identity;bool removed{};
        NoAllocate([&]{removed=staging.Remove(index,identity);});
        Require(removed&&stagingOracle.Remove(index,identity),"Transferred index-link head/middle/tail removal failed");Verify(staging,stagingOracle);
    }
    for (const auto index:stagedIndices) if (const auto* record=stagingOracle.At(index)) {
        const auto identity=record->identity;bool removed{};
        NoAllocate([&]{removed=staging.Remove(index,identity);});
        Require(removed&&stagingOracle.Remove(index,identity),"Draining adopted records broke a surviving cross-index link");Verify(staging,stagingOracle);
    }
    Verify(staging,oldLiveOracle);
}

void NonallocatingAndRefusedAdoption() {
    Lookup::Index live(8u),additions(8u);Oracle liveOracle(8u),additionOracle(8u);
    Publish(live,liveOracle,Object(101u,4'000'000u));Publish(live,liveOracle,Object(102u,3u));
    Publish(additions,additionOracle,Object(103u,17u));Publish(additions,additionOracle,Object(104u,27u));
    const auto* retained=live.RecordAt(3u);const auto* incoming=additions.RecordAt(17u);
    bool changed{},missing{},wrong{},badRehash{},can{},adopted{};
    NoAllocate([&]{changed=live.Rehash(3u,Identity(102u),Outer(1u),1u,76);});
    Require(changed&&liveOracle.Rehash(3u,Identity(102u),Outer(1u),1u,76)&&live.RecordAt(3u)==retained,
        "Rehash changed Record address or lost updated identity/key");
    NoAllocate([&]{missing=live.Remove(555u,Identity(102u));wrong=live.Remove(3u,Identity(101u));});
    Require(!missing&&!wrong,"Absent/stale cleanup succeeded under allocation denial");
    NoAllocate([&]{badRehash=live.Rehash(3u,Identity(102u),Outer(1u),1u,-1);});
    Require(!badRehash&&live.RecordAt(3u)==retained,"Invalid nonallocating rehash changed stable ownership");
    NoAllocate([&]{can=live.CanAdopt(additions);adopted=live.Adopt(additions);});
    Require(can&&adopted&&liveOracle.Adopt(additionOracle)&&live.RecordAt(17u)==incoming&&live.RecordAt(3u)==retained,
        "No-allocation adoption failed to preserve actual tree-node storage");Verify(live,liveOracle);Verify(additions,additionOracle);
    NoAllocate([&]{can=live.CanAdopt(additions);adopted=live.Adopt(additions);});
    Require(can&&adopted&&liveOracle.Adopt(additionOracle),"Empty detached adoption changed its successful admission contract");
    Verify(live,liveOracle);Verify(additions,additionOracle);
    NoAllocate([&]{can=live.CanAdopt(live);adopted=live.Adopt(live);});Require(!can&&!adopted,"Self-adoption accepted");
    for (unsigned reason=0u;reason<3u;++reason) {
        Lookup::Index target(reason==2u?1u:8u),donor(8u);Oracle targetOracle(reason==2u?1u:8u),donorOracle(8u);
        Publish(target,targetOracle,Object(111u,111u));
        auto incomingRecord=Object(reason==1u?111u:112u,reason==0u?111u:112u);
        Publish(donor,donorOracle,incomingRecord);
        const auto* targetPointer=target.RecordAt(111u);const auto* donorPointer=donor.RecordAt(incomingRecord.index);
        NoAllocate([&]{can=target.CanAdopt(donor);adopted=target.Adopt(donor);});
        Require(!can&&!adopted&&!targetOracle.CanAdopt(donorOracle)&&target.RecordAt(111u)==targetPointer&&
            donor.RecordAt(incomingRecord.index)==donorPointer,"Refused index/identity/capacity adoption mutated stable ownership");
        Verify(target,targetOracle);Verify(donor,donorOracle);++refusals;
    }
    bool removed{};NoAllocate([&]{removed=live.Remove(4'000'000u,Identity(101u));});
    Require(removed&&liveOracle.Remove(4'000'000u,Identity(101u)),"Nonallocating maximum-index unlink failed");Verify(live,liveOracle);
}

void AllocationFailures() {
    Lookup::Index source(32u);Oracle oracle(32u);
    for (std::uint32_t serial=151u;serial<159u;++serial) Publish(source,oracle,Object(serial,(serial-150u)*100'003u,serial%4u));
    Rehash(source,oracle,3u*100'003u);Rehash(source,oracle,7u*100'003u);
    std::vector<const Record*> pointers;for (const auto& [index,record]:oracle.ascending) {(void)record;pointers.push_back(source.RecordAt(index));}
    const auto bytes=source.RetainedBytes();
    bool publicationFailed{},published{};
    for (std::size_t allowed=0u;allowed<8u&&!published;++allowed) {
        bool bad{};CompactLookupAllocationFault::Arm(allowed);
        try {source.Publish(Object(160u,4'000'000u,1u));published=true;} catch (const std::bad_alloc&) {bad=true;}
        CompactLookupAllocationFault::Disable();
        if (bad) {
            publicationFailed=true;++faultCases;Verify(source,oracle);
            Require(source.RetainedBytes()==bytes&&!source.RecordForIdentity(Identity(160u)),"Failed Publish partially installed links or retained storage");
        }
    }
    Require(publicationFailed&&published,"Publication failure injection did not reach both allocation refusal and successful recovery");
    oracle.Publish(Object(160u,4'000'000u,1u));Verify(source,oracle);
    Require(source.Remove(4'000'000u,Identity(160u))&&oracle.Remove(4'000'000u,Identity(160u)),"Failed publication recovery leaked its identity reservation");
    std::size_t copyAssignmentFailures{},copyConstructionFailures{};
    for (unsigned kind=0u;kind<2u;++kind) {
        bool copied{};
        for (std::size_t allowed=0u;allowed<32u&&!copied;++allowed) {
            Lookup::Index destination(2u);Oracle destinationOracle(2u);
            Publish(destination,destinationOracle,Object(170u,3'999'999u,3u));const auto* old=destination.RecordAt(3'999'999u);
            std::optional<Lookup::Index> constructed;bool bad{};CompactLookupAllocationFault::Arm(allowed);
            try {
                if (!kind) destination=source;else constructed.emplace(source);
                copied=true;
            } catch (const std::bad_alloc&) {bad=true;}
            CompactLookupAllocationFault::Disable();
            if (bad) {
                ++faultCases;if (!kind) ++copyAssignmentFailures;else ++copyConstructionFailures;
                Verify(destination,destinationOracle);Require(destination.RecordAt(3'999'999u)==old,
                    "Failed copy assignment invalidated pre-existing destination Record");
            } else Verify(kind?*constructed:destination,oracle);
            Verify(source,oracle);
            std::size_t position{};for (const auto& [index,record]:oracle.ascending) {
                (void)record;Require(source.RecordAt(index)==pointers[position++],"Faulted/correct copy changed source Record storage");
            }
        }
        Require(copied,"Copy failure sweep never recovered to a complete copied index");
    }
    Require(copyAssignmentFailures>=oracle.ascending.size()&&copyConstructionFailures>=oracle.ascending.size(),
        "Copy failure sweep did not exercise each owning tree-node allocation");
}

std::uint64_t Next(std::uint64_t& random) {random^=random<<13u;random^=random>>7u;random^=random<<17u;return random;}
void MixedStress() {
    std::array<Lookup::Index,3u> actual{Lookup::Index(24u),Lookup::Index(24u),Lookup::Index(24u)};
    std::array<Oracle,3u> oracle{Oracle(24u),Oracle(24u),Oracle(24u)};
    std::uint64_t random=0x1234abcd5678ef90ull;std::uint32_t serial=250u;
    for (unsigned operation=0u;operation<1'536u;++operation) {
        const auto word=Next(random);const auto a=static_cast<std::size_t>(word%3u),b=static_cast<std::size_t>((word>>8u)%3u);
        const auto index=static_cast<std::uint32_t>(17u+((word>>16u)%48u)*79'191u);
        switch ((word>>32u)%13u) {
        case 0u: case 1u: case 2u: {
            auto record=Object(++serial,index,static_cast<unsigned>(word>>40u));
            if (word&0x100u) record.nameIndex=static_cast<int>((word>>48u)%9u);
            if (oracle[a].CanPublish(record)) Publish(actual[a],oracle[a],record);
            else Refuse([&]{actual[a].Publish(record);});
            break;
        }
        case 3u: case 4u: {
            const auto* record=oracle[a].At(index);const auto identity=record&&(word&0x80u)?record->identity:Identity(3999u);
            const auto expected=oracle[a].Remove(index,identity);
            Require(actual[a].Remove(index,identity)==expected,"Mixed stale/correct Remove differs from reference membership");break;
        }
        case 5u: case 6u: {
            const auto* record=oracle[a].At(index);const auto identity=record&&(word&0x80u)?record->identity:Identity(3999u);
            auto replacement=Object(1u,index,static_cast<unsigned>(word>>40u));
            if (word&0x100u) replacement.nameIndex=static_cast<int>((word>>48u)%9u);
            const auto expected=oracle[a].Rehash(index,identity,replacement.outerIdentity,replacement.outerIndex,replacement.nameIndex);
            Require(actual[a].Rehash(index,identity,replacement.outerIdentity,replacement.outerIndex,replacement.nameIndex)==expected,
                "Mixed same/cross-key Rehash differs from chronological reference");break;
        }
        case 7u:actual[b]=actual[a];oracle[b]=oracle[a];break;
        case 8u:
            if (a!=b) {actual[b]=std::move(actual[a]);oracle[b]=oracle[a];oracle[a].ascending.clear();oracle[a].newest.clear();}
            else {auto* alias=&actual[a];actual[a]=std::move(*alias);}
            break;
        case 9u:actual[a].Swap(actual[b]);std::swap(oracle[a],oracle[b]);break;
        case 10u: {
            const auto expected=oracle[a].CanAdopt(oracle[b]);
            Require(actual[a].CanAdopt(actual[b])==expected,"Mixed adoption admission differs");
            Require(actual[a].Adopt(actual[b])==expected,"Mixed adoption publication differs");
            Require(oracle[a].Adopt(oracle[b])==expected,"Independent mixed adoption mutated inconsistent fixtures");break;
        }
        case 11u: {
            Lookup::Index temporary(actual[a]);Verify(temporary,oracle[a]);actual[b].Swap(temporary);oracle[b]=oracle[a];break;
        }
        default: {const auto* alias=&actual[a];actual[a]=*alias;break;}
        }
        for (std::size_t i=0u;i<actual.size();++i) Verify(actual[i],oracle[i]);
        ++mixedOperations;
    }
}
}

int main() {
    try {
        RepresentationAndSparseIndices();CopiesTransfersAndStableRecords();NonallocatingAndRefusedAdoption();AllocationFailures();MixedStress();
        std::cout<<"PASS compact lookup "<<checks<<" checks / "<<mixedOperations<<" mixed operations / "<<nonallocCases
            <<" nonallocating controls / "<<faultCases<<" allocation-failure points / "<<refusals<<" refusals.\n";return 0;
    } catch (const std::exception& error) {
        CompactLookupAllocationFault::Disable();std::cerr<<"Compact lookup controls failed: "<<error.what()<<'\n';return 1;
    }
}
