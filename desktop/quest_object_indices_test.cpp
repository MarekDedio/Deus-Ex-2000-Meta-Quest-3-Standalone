#include "quest_object_indices.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace AllocationFixture {
bool refuse{};
std::size_t remaining=std::numeric_limits<std::size_t>::max();
}

// This standalone test denies allocation at precise scope boundaries. The
// production pool has no test allocator hook and no mutable numeric release API.
#if defined(__GNUC__)
#define INDEX_TEST_ALLOCATION_NOINLINE __attribute__((noinline))
#else
#define INDEX_TEST_ALLOCATION_NOINLINE
#endif
// Keep the matching malloc/free replacement pair opaque to GCC's optimized
// mismatched-new-delete heuristic; these are deliberate global replacements.
INDEX_TEST_ALLOCATION_NOINLINE void* operator new(std::size_t bytes) {
    if (AllocationFixture::refuse || AllocationFixture::remaining==0u) throw std::bad_alloc();
    if (AllocationFixture::remaining!=std::numeric_limits<std::size_t>::max()) --AllocationFixture::remaining;
    if (void* memory=std::malloc(bytes==0u?1u:bytes)) return memory;
    throw std::bad_alloc();
}
INDEX_TEST_ALLOCATION_NOINLINE void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
INDEX_TEST_ALLOCATION_NOINLINE void operator delete(void* memory) noexcept { std::free(memory); }
INDEX_TEST_ALLOCATION_NOINLINE void operator delete[](void* memory) noexcept { std::free(memory); }
INDEX_TEST_ALLOCATION_NOINLINE void operator delete(void* memory,std::size_t) noexcept { std::free(memory); }
INDEX_TEST_ALLOCATION_NOINLINE void operator delete[](void* memory,std::size_t) noexcept { std::free(memory); }
#undef INDEX_TEST_ALLOCATION_NOINLINE

namespace {
using Pool=QuestVr::ObjectIndexPool;
using Lease=Pool::Lease;
std::size_t checks{},refusals{};

void Require(bool condition,const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class Exception,class Action>
void Refuse(Action action,const char* message) {
    bool refused{};
    try { action(); } catch (const Exception&) { refused=true; }
    Require(refused,message);++refusals;
}
struct DenyAllocation {
    bool previous=AllocationFixture::refuse;
    DenyAllocation() { AllocationFixture::refuse=true; }
    DenyAllocation(const DenyAllocation&)=delete;
    DenyAllocation& operator=(const DenyAllocation&)=delete;
    ~DenyAllocation() { AllocationFixture::refuse=previous; }
};
struct FailAllocationAfter {
    std::size_t previous=AllocationFixture::remaining;
    explicit FailAllocationAfter(std::size_t successful) {AllocationFixture::remaining=successful;}
    ~FailAllocationAfter() {AllocationFixture::remaining=previous;}
};
void MoveAssign(Lease& destination,Lease& source) {
    destination=std::move(source);
}
struct AdmissionRejected {};
struct PagedAdmission {
    std::array<std::size_t,128u> requests{};
    std::size_t calls{},approved{},rejectCall=std::numeric_limits<std::size_t>::max();
    static void Reserve(void* context,std::size_t bytes) {
        auto& self=*static_cast<PagedAdmission*>(context);
        if(self.calls>=self.requests.size())throw std::logic_error("Generated journal admission trace exceeded bound");
        self.requests[self.calls++]=bytes;
        if(self.calls==self.rejectCall)throw AdmissionRejected{};
        self.approved+=bytes;
    }
    Pool::JournalAllocationAdmission Borrow(){return {this,&Reserve};}
};

void LifecycleAndBounds() {
    static_assert(!std::is_copy_constructible_v<Lease> && !std::is_copy_assignable_v<Lease>);
    static_assert(std::is_nothrow_move_constructible_v<Lease> && std::is_nothrow_move_assignable_v<Lease>);
    static_assert(std::is_nothrow_destructible_v<Lease> && std::is_nothrow_default_constructible_v<Lease>);
    static_assert(!std::is_copy_constructible_v<Pool> && !std::is_copy_assignable_v<Pool>);
    static_assert(std::is_nothrow_move_constructible_v<Pool> && std::is_nothrow_move_assignable_v<Pool>);
    static_assert(std::is_same_v<decltype(std::declval<const Lease&>().Index()),std::uint32_t>);
    static_assert(noexcept(std::declval<const Lease&>().Index()));
    Lease empty;
    Require(!empty && empty.Index()==0u,"Default lease claims a live object index");
    Pool pool(2u);
    Require(pool.LiveCount()==0u && pool.AllocatedSlots()==0u,"Empty pool eagerly allocated index slots");
    auto first=pool.Acquire();const auto firstIndex=first.Index();
    Require(first && firstIndex!=0u && pool.LiveCount()==1u && pool.AllocatedSlots()==1u,
        "First UObject lease did not create one live nonzero index");
    auto second=pool.Acquire();const auto secondIndex=second.Index();
    Require(secondIndex!=firstIndex && pool.LiveCount()==2u && pool.AllocatedSlots()==2u,
        "Two simultaneously live objects share one index");
    Refuse<std::length_error>([&] { static_cast<void>(pool.Acquire()); },"Tight pool exceeded its capacity");
    Require(first.Index()==firstIndex && second.Index()==secondIndex && pool.LiveCount()==2u && pool.AllocatedSlots()==2u,
        "Capacity failure changed existing live indices or pool state");
    first=Lease{};
    Require(!first && first.Index()==0u && pool.LiveCount()==1u && second.Index()==secondIndex,
        "Lease destruction freed another object's index or retained ownership");
    auto reused=pool.Acquire();
    Require(reused.Index()==firstIndex && reused.Index()!=secondIndex && pool.AllocatedSlots()==2u,
        "Pool did not reuse a destroyed object's slot or reused a live slot");
    reused=Lease{};second=Lease{};
    Require(pool.LiveCount()==0u && pool.AllocatedSlots()==2u,"Released leases retained live ownership");
    Refuse<std::invalid_argument>([] { Pool invalid(0u); },"Pool accepted zero usable object slots");
    Refuse<std::invalid_argument>([] { Pool invalid(Pool::MaximumSlots+1u); },"Pool exceeded explicit portable resource policy");
    Refuse<std::invalid_argument>([] { Pool invalid(std::numeric_limits<std::uint32_t>::max()); },"Pool accepted unbounded uint32 capacity");
    Pool maximum(Pool::MaximumSlots);
    Require(maximum.AllocatedSlots()==0u,"Maximum configured capacity eagerly allocated all slots");
    {
        const auto temporary=maximum.Acquire();
        Require(temporary.Index()!=0u && maximum.LiveCount()==1u,
            "Maximum configured capacity could not acquire an ordinary live object");
    }
    Require(maximum.LiveCount()==0u,"Maximum-capacity pool retained a temporary object");
}

void MoveAndPoolLifetime() {
    Pool firstPool(3u),secondPool(2u);
    auto first=firstPool.Acquire();const auto firstIndex=first.Index();
    auto second=secondPool.Acquire();const auto secondIndex=second.Index();
    Lease moved(std::move(first));
    Require(!first && first.Index()==0u && moved.Index()==firstIndex && firstPool.LiveCount()==1u,
        "Lease move construction released or duplicated its source ownership");
    MoveAssign(moved,moved); // Deliberate self-move through the public operation.
    Require(moved.Index()==firstIndex && firstPool.LiveCount()==1u,"Lease self-move lost its live index");
    moved=std::move(second);
    Require(!second && moved.Index()==secondIndex && firstPool.LiveCount()==0u && secondPool.LiveCount()==1u,
        "Cross-pool move assignment released the foreign pool or retained overwritten ownership");
    auto replacement=firstPool.Acquire();
    Require(replacement.Index()==firstIndex && secondPool.LiveCount()==1u,
        "Cross-pool lease release corrupted the other pool's active index");
    auto secondLive=secondPool.Acquire();
    Require(secondLive.Index()!=moved.Index(),"Cross-pool move left duplicate live ownership in its source pool");
    Pool owner(std::move(firstPool));
    Require(firstPool.LiveCount()==0u && firstPool.AllocatedSlots()==0u && owner.LiveCount()==1u &&
        replacement.Index()==firstIndex,"Moving a pool invalidated its outstanding object's lease");
    Refuse<std::logic_error>([&] { static_cast<void>(firstPool.Acquire()); },"Moved-from pool allocated new indices");
    auto old=owner.Acquire();const auto oldIndex=old.Index();
    owner=std::move(secondPool);
    Require(!secondPool.LiveCount() && owner.LiveCount()==2u && replacement.Index()==firstIndex && old.Index()==oldIndex,
        "Pool move assignment destroyed outstanding leases from either control block");
    replacement=Lease{};old=Lease{};
    Require(owner.LiveCount()==2u,"Orphan control block releases changed the replacement pool");
    moved=Lease{};secondLive=Lease{};
    Require(owner.LiveCount()==0u,"Leases did not follow their control block through pool move assignment");
    Lease survivor;
    {
        Pool temporary(1u);survivor=temporary.Acquire();
        Require(survivor && temporary.LiveCount()==1u,"Surviving lease fixture failed");
    }
    Require(survivor && survivor.Index()!=0u,"Pool wrapper destruction invalidated an outstanding UObject index");
    {
        DenyAllocation deny;
        survivor=Lease{};
    }
    Require(!survivor,"Last lease could not destroy its orphaned control block allocation-free");
}

void ManyObjectsAndContainerMoves() {
    constexpr std::size_t count=65536u;
    Pool pool(static_cast<std::uint32_t>(count));
    std::vector<Lease> objects;
    std::set<std::uint32_t> live;
    // Deliberately do not reserve objects: every vector reallocation must move
    // leases without releasing/reassigning their engine-owned object indices.
    for(std::size_t index=0u;index<count;++index) {
        objects.push_back(pool.Acquire());
        Require(objects.back().Index()!=0u && live.insert(objects.back().Index()).second,
            "Live UObject index collision during container/pool growth");
        Require(pool.LiveCount()==index+1u,"Container growth released or duplicated a live index");
    }
    Require(pool.LiveCount()==count && pool.AllocatedSlots()==count,"Large pool did not retain each live object");
    Refuse<std::length_error>([&] {static_cast<void>(pool.Acquire());},"Large pool capacity overflow was accepted");
    std::vector<std::uint32_t> freed;
    for(std::size_t index=0u;index<count;index+=2u) {
        const auto identity=objects[index].Index();freed.push_back(identity);live.erase(identity);objects[index]=Lease{};
    }
    Require(pool.LiveCount()==count/2u,"Partial object destruction freed wrong number of index slots");
    for(std::size_t index=0u;index<count;index+=2u) {
        objects[index]=pool.Acquire();
        Require(live.insert(objects[index].Index()).second,"Reused object slot collided with a surviving live UObject");
        Require(objects[index].Index()==freed.back(),"Freed slot link order was corrupted by container moves");freed.pop_back();
    }
    Require(pool.LiveCount()==count && pool.AllocatedSlots()==count && live.size()==count,
        "Slot reuse grew unnecessary indices or lost live uniqueness");
    objects.clear();
    Require(pool.LiveCount()==0u,"Container destruction did not release every UObject index exactly once");
    for(std::size_t index=0u;index<count;++index) {
        objects.push_back(pool.Acquire());
        Require(objects.back().Index()!=0u,"Reused index wrapped through reserved None zero");
    }
    Require(pool.AllocatedSlots()==count,"Repopulation allocated extra slots instead of reusing destroyed objects");
}

void AllocationFailuresAndExceptionUnwind() {
    Pool pool(128u);
    bool failed{};
    {
        DenyAllocation deny;
        try {static_cast<void>(pool.Acquire());}catch(const std::bad_alloc&) {failed=true;}
    }
    Require(failed && pool.LiveCount()==0u && pool.AllocatedSlots()==0u,
        "Failed first slot allocation changed the pool's free/live state");++refusals;
    auto first=pool.Acquire();
    Require(first.Index()==1u && pool.LiveCount()==1u,"Retry after allocation failure skipped or duplicated the first index");
    std::array<Lease,128u> retained;
    std::size_t retainedCount{};
    failed=false;
    std::uint32_t previousLive{},previousSlots{};
    {
        DenyAllocation deny;
        for(std::size_t index=0u;index<retained.size();++index) {
            previousLive=pool.LiveCount();previousSlots=pool.AllocatedSlots();
            try {retained[retainedCount]=pool.Acquire();++retainedCount;}
            catch(const std::bad_alloc&) {failed=true;break;}
        }
    }
    Require(failed && pool.LiveCount()==previousLive && pool.AllocatedSlots()==previousSlots && first.Index()==1u,
        "Growth allocation failure changed live slots or an existing object's index");++refusals;
    auto retry=pool.Acquire();
    Require(retry.Index()==previousSlots+1u && pool.LiveCount()==previousLive+1u,
        "Failed growth consumed an index or corrupted retry allocation");
    const auto reusable=retry.Index();
    {
        DenyAllocation deny;
        retry=Lease{};
        retry=pool.Acquire();
        first=Lease{};
        for(auto& lease:retained) lease=Lease{};
    }
    Require(retry.Index()==reusable && pool.LiveCount()==1u,
        "Release/free-list acquisition allocated or incorrectly released another object's index");
    struct ThrowingObject {
        Lease index;
        explicit ThrowingObject(Pool& source):index(source.Acquire()) {throw std::runtime_error("Generated constructor failure");}
    };
    const auto before=pool.LiveCount();
    Refuse<std::runtime_error>([&] {ThrowingObject object(pool);},"Generated object constructor did not throw");
    Require(pool.LiveCount()==before && retry.Index()==reusable,
        "Failed UObject construction leaked its leased index or released a surviving object");
    auto after=pool.Acquire();
    Require(after.Index()!=retry.Index() && pool.LiveCount()==before+1u,
        "Constructor unwind made a live slot available for reuse");
    Refuse<std::runtime_error>([&] {
        std::vector<Lease> temporary;
        for(unsigned index=0u;index<31u;++index) temporary.push_back(pool.Acquire());
        throw std::runtime_error("Generated enclosing operation failure");
    },"Generated enclosing operation did not throw");
    Require(pool.LiveCount()==before+1u && after.Index()!=retry.Index(),
        "Exception unwind through a relocated lease container leaked or double-released indices");
}

void TransactionRetirementAndExactRollback() {
    static_assert(!std::is_copy_constructible_v<Pool::Transaction>);
    static_assert(std::is_nothrow_move_constructible_v<Pool::Transaction>);
    static_assert(std::is_nothrow_move_assignable_v<Pool::Transaction>);
    static_assert(std::is_nothrow_destructible_v<Pool::Transaction>);
    Pool pool(32u);
    std::array<Lease,8u> originals;
    for (auto& object:originals) object=pool.Acquire();
    originals[2]=Lease{};originals[5]=Lease{};originals[0]=Lease{};
    auto transaction=pool.BeginTransaction(32u);
    Require(transaction.Active() && transaction.EntryCount()==0u && transaction.EntryCapacity()==32u &&
        transaction.RetainedBytes()==Pool::TransactionRetainedBytes(32u),
        "Index transaction did not report its complete fixed bounded journal storage");
    transaction.Retire(originals[1]);
    Require(!originals[1] && pool.LiveCount()==4u && transaction.EntryCount()==1u,
        "Pre-root retirement retained a live numeric index or failed to journal ownership");
    auto replacement=pool.Acquire();
    Require(replacement.Index()==2u,"Retirement did not immediately release its exact slot for same-root allocation");
    auto priorFree=pool.Acquire();
    Require(priorFree.Index()==1u,"Same-root reuse changed the pre-root free-list ordering");
    transaction.Retire(replacement);
    Require(!replacement && transaction.EntryCount()==3u,
        "Retiring a provisional lease added a redundant undo row or retained its index");
    replacement=pool.Acquire();
    Require(replacement.Index()==2u,"Repeated same-root deletion did not reuse its numeric slot");
    auto reusedFree=pool.Acquire();auto lastFree=pool.Acquire();auto appended=pool.Acquire();
    Require(reusedFree.Index()==6u && lastFree.Index()==3u && appended.Index()==9u && pool.AllocatedSlots()==9u,
        "Transaction fixture did not exercise existing free links and appended slots");
    Lease relocatedOld(std::move(originals[1]));
    Require(!originals[1] && !relocatedOld,"Moving a retired lease exposed its numeric slot before rollback");
    Lease relocatedNew(std::move(appended));
    {
        DenyAllocation deny;
        transaction.Rollback();
    }
    Require(!transaction.Active() && transaction.RetainedBytes()==0u && relocatedOld.Index()==2u &&
        !replacement && !priorFree && !reusedFree && !lastFree && !appended && !relocatedNew,
        "Rollback failed to restore relocated old ownership or invalidate all surviving provisional leases");
    Require(pool.LiveCount()==5u && pool.AllocatedSlots()==8u,
        "Rollback retained appended slots or changed the original live count");
    auto first=pool.Acquire();auto second=pool.Acquire();auto third=pool.Acquire();auto fresh=pool.Acquire();
    Require(first.Index()==1u && second.Index()==6u && third.Index()==3u && fresh.Index()==9u,
        "Rollback did not restore the exact original free-list order and allocated-slot frontier");
    {
        DenyAllocation deny;
        transaction.Rollback();transaction.Commit();
    }
    Refuse<std::logic_error>([&] {transaction.Retire(first);},"Inactive transaction retired a surviving lease");
}

void TransactionCommitMovesAndLifetime() {
    Pool pool(16u);
    auto original=pool.Acquire();const auto oldIndex=original.Index();
    auto transaction=pool.BeginTransaction(16u);
    transaction.Retire(original);
    std::vector<Lease> provisional;
    for (unsigned index=0u;index<8u;++index) provisional.push_back(pool.Acquire());
    Require(provisional.front().Index()==oldIndex && pool.LiveCount()==8u,
        "Transaction did not track provisional leases through container relocations");
    auto movedTransaction=std::move(transaction);
    Require(!transaction.Active() && movedTransaction.Active(),"Moving a transaction lost its active control domain");
    provisional[1]=std::move(provisional[2]);
    Require(!provisional[2] && pool.LiveCount()==7u,
        "Provisional lease move assignment retained overwritten allocation ownership");
    Lease retiredOwner;
    retiredOwner=std::move(original);
    auto replacement=pool.Acquire();
    Require(replacement.Index()==2u,"Provisional overwrite did not return its slot within the transaction");
    {
        DenyAllocation deny;
        movedTransaction.Commit();
    }
    Require(!retiredOwner && !original && !movedTransaction.Active() && pool.LiveCount()==8u &&
        provisional.front().Index()==oldIndex && replacement.Index()==2u,
        "Commit restored a deleted old lease or invalidated a new allocation");
    {
        DenyAllocation deny;
        provisional.clear();replacement=Lease{};retiredOwner=Lease{};
    }
    Require(pool.LiveCount()==0u,"Committed lease destruction retained transaction registration or leaked a live slot");

    Lease survivor;
    std::optional<Pool::Transaction> survivingTransaction;
    {
        Pool temporary(2u);
        survivor=temporary.Acquire();
        survivingTransaction.emplace(temporary.BeginTransaction(2u));
        survivingTransaction->Retire(survivor);
        Pool movedPool(std::move(temporary));
        Require(!temporary.LiveCount() && movedPool.LiveCount()==0u,
            "Moving a pool wrapper invalidated an active retirement domain");
    }
    {
        DenyAllocation deny;
        survivingTransaction.reset();
    }
    Require(survivor.Index()==1u,"Transaction rollback after pool-wrapper destruction lost retained old lease authority");
    {
        DenyAllocation deny;
        survivor=Lease{};
    }

    Pool firstPool(2u),secondPool(2u);
    auto firstOld=firstPool.Acquire();auto secondOld=secondPool.Acquire();
    auto firstTransaction=firstPool.BeginTransaction(2u);
    auto secondTransaction=secondPool.BeginTransaction(2u);
    firstTransaction.Retire(firstOld);secondTransaction.Retire(secondOld);
    firstTransaction=std::move(secondTransaction);
    Require(firstOld.Index()==1u && !secondOld && firstTransaction.Active() && !secondTransaction.Active(),
        "Transaction move assignment did not rollback its former domain before transferring the new one");
    firstTransaction.Rollback();
    Require(secondOld.Index()==1u,"Moved transaction no longer restored its original control domain");
}

void TransactionFailuresAndConstructorUnwind() {
    Pool pool(8u),foreign(1u);
    auto original=pool.Acquire();auto foreignLease=foreign.Acquire();
    Refuse<std::invalid_argument>([&] {static_cast<void>(pool.BeginTransaction(0u));},
        "Index transaction accepted an empty journal bound");
    Refuse<std::invalid_argument>([&] {static_cast<void>(pool.BeginTransaction(Pool::MaximumSlots+1u));},
        "Index transaction accepted an unbounded journal");
    bool failed{};
    {
        DenyAllocation deny;
        try {static_cast<void>(pool.BeginTransaction(2u));} catch(const std::bad_alloc&) {failed=true;}
    }
    Require(failed && original.Index()==1u && pool.LiveCount()==1u && pool.AllocatedSlots()==1u,
        "Failed journal admission activated a transaction or changed live pool state");++refusals;
    failed=false;
    {
        FailAllocationAfter secondAllocation(1u);
        try {static_cast<void>(pool.BeginTransaction(2u));} catch(const std::bad_alloc&) {failed=true;}
    }
    Require(failed && original.Index()==1u && pool.LiveCount()==1u && pool.AllocatedSlots()==1u,
        "Failed fixed-journal allocation published a partial transaction or changed pool state");++refusals;
    auto tight=pool.BeginTransaction(1u);
    Refuse<std::logic_error>([&] {static_cast<void>(pool.BeginTransaction(1u));},
        "Nested transaction replaced the active root's journal");
    Refuse<std::invalid_argument>([&] {tight.Retire(foreignLease);},
        "Transaction accepted another pool's allocation authority");
    Lease empty;
    Refuse<std::invalid_argument>([&] {tight.Retire(empty);},"Transaction retired a nonexistent lease");
    tight.Retire(original);
    Refuse<std::invalid_argument>([&] {tight.Retire(original);},"Transaction retired the same old ownership twice");
    Refuse<std::length_error>([&] {static_cast<void>(pool.Acquire());},
        "Journal overflow allocated an untracked UObject index");
    Require(pool.LiveCount()==0u && pool.AllocatedSlots()==1u && tight.EntryCount()==1u,
        "Journal overflow modified the free-list or allocation frontier");
    {
        DenyAllocation deny;tight.Rollback();
    }
    Require(original.Index()==1u && pool.LiveCount()==1u,"Tight-journal failure did not restore retired ownership");

    auto transaction=pool.BeginTransaction(16u);
    struct ThrowingObject {
        Lease index;
        explicit ThrowingObject(Pool& source):index(source.Acquire()) {throw std::runtime_error("Constructor failure in root");}
    };
    Refuse<std::runtime_error>([&] {ThrowingObject object(pool);},
        "Provisional constructor fixture did not exercise lease destruction during unwind");
    Require(pool.LiveCount()==1u && transaction.EntryCount()==1u,
        "Constructor unwind leaked or removed an untracked live lease");
    auto after=pool.Acquire();
    Require(after.Index()==2u,"Constructor unwind did not immediately release the provisional object's slot");
    transaction.Retire(original);
    auto reused=pool.Acquire();
    Require(reused.Index()==1u,"Pre-root retirement did not remain available after provisional constructor unwind");
    {
        DenyAllocation deny;
        after=Lease{};transaction.Rollback();
    }
    Require(original.Index()==1u && !after && !reused && pool.LiveCount()==1u && pool.AllocatedSlots()==1u,
        "Constructor unwind plus later failure lost exact root allocation state");

    auto growth=pool.BeginTransaction(16u);
    std::array<Lease,8u> retained;
    std::uint32_t priorLive{},priorSlots{},priorEntries{};std::size_t retainedCount{};
    failed=false;
    {
        DenyAllocation deny;
        while(retainedCount<retained.size()) {
            priorLive=pool.LiveCount();priorSlots=pool.AllocatedSlots();priorEntries=growth.EntryCount();
            try {retained[retainedCount]=pool.Acquire();++retainedCount;}
            catch(const std::bad_alloc&) {failed=true;break;}
        }
    }
    Require(failed && pool.LiveCount()==priorLive && pool.AllocatedSlots()==priorSlots && growth.EntryCount()==priorEntries,
        "Failed slot growth during an active transaction consumed a journal row or changed pool state");++refusals;
    {
        DenyAllocation deny;growth.Rollback();
    }
    Require(pool.LiveCount()==1u && pool.AllocatedSlots()==1u && original.Index()==1u,
        "Active slot-growth failure changed the exact root allocation frontier");

    auto secondOld=pool.Acquire();
    auto retireOverflow=pool.BeginTransaction(1u);
    auto onlyProvisional=pool.Acquire();
    Refuse<std::length_error>([&] {retireOverflow.Retire(secondOld);},
        "Journal overflow retired an old object without preserving its undo authority");
    Require(secondOld.Index()==2u && pool.LiveCount()==3u && retireOverflow.EntryCount()==1u,
        "Failed old-lease retirement changed existing ownership or live count");
    retireOverflow.Rollback();
    Require(!onlyProvisional && original.Index()==1u && secondOld.Index()==2u && pool.LiveCount()==2u,
        "Late retirement admission failure prevented complete root rollback");
}

void RepeatedTransactionsAndSlotReuse() {
    // Independent numeric oracle: reserve eight live slots, free 3/6/1, then
    // retire one live slot. Every rollback must recover the same LIFO chain,
    // regardless of repeated releases and moves of a reused numeric slot.
    for (std::uint32_t repeat=0u;repeat<96u;++repeat) {
        Pool pool(32u);std::array<Lease,8u> originals;
        for (auto& lease:originals) lease=pool.Acquire();
        originals[2]=Lease{};originals[5]=Lease{};originals[0]=Lease{};
        Lease current;
        {
            auto transaction=pool.BeginTransaction(128u);
            transaction.Retire(originals[1]);
            for (std::uint32_t turn=0u;turn<=repeat;++turn) {
                current=pool.Acquire();
                Require(current.Index()==2u,"Repeated same-root release/reacquire changed the retired native slot");
                if (turn!=repeat) {
                    Lease intermediate(std::move(current));
                    transaction.Retire(intermediate);
                }
            }
            Require(transaction.EntryCount()==repeat+2u,"Repeated reuse omitted an Acquire from the active journal");
            if ((repeat&1u)!=0u) {
                DenyAllocation deny;transaction.Rollback();
            }
        }
        Require(!current && originals[1].Index()==2u && pool.LiveCount()==5u && pool.AllocatedSlots()==8u,
            "Explicit/RAII rollback after repeated reuse lost old ownership or retained a provisional lease");
        auto first=pool.Acquire();auto second=pool.Acquire();auto third=pool.Acquire();
        Require(first.Index()==1u && second.Index()==6u && third.Index()==3u,
            "Repeated reuse rollback changed an unrelated pre-root free-list link");
    }
}

void PagedBoundariesMovesAndExactRollback() {
    Pool pool(128u);auto original=pool.Acquire();const auto originalIndex=original.Index();
    PagedAdmission admission;auto transaction=pool.BeginPagedTransaction(65u,admission.Borrow());
    Require(transaction.Active()&&transaction.EntryCapacity()==65u&&transaction.EntryCount()==0u&&
        transaction.AllocatedEntryCapacity()==0u&&transaction.RetainedBytes()==Pool::PagedTransactionInitialRetainedBytes(65u)&&
        admission.calls==1u&&admission.approved==transaction.RetainedBytes(),
        "Paged transaction did not admit only actual initial state/pointer table");
    transaction.PrepareNextEntryCapacity();transaction.PrepareNextEntryCapacity();
    Require(transaction.EntryCount()==0u&&transaction.AllocatedEntryCapacity()==64u&&admission.calls==2u&&
        admission.approved==transaction.RetainedBytes()&&original.Index()==originalIndex&&pool.LiveCount()==1u,
        "Explicit next-row capacity preparation consumed an ordinal/identity or repeated its admission");
    transaction.Retire(original);std::array<Lease,64u> provisional;
    for(std::size_t i=0;i<provisional.size();++i){
        provisional[i]=pool.Acquire();
        const auto entries=static_cast<std::uint32_t>(i+2u);
        Require(transaction.EntryCount()==entries&&transaction.EntryCapacity()==65u&&
            transaction.AllocatedEntryCapacity()==(entries<=64u?64u:65u)&&
            transaction.RetainedBytes()==admission.approved,
            "Paged journal changed operation ceiling, owning page capacity or retained-byte admission");
    }
    Require(admission.calls==3u&&admission.requests[1u]==64u*admission.requests[2u]&&
        transaction.RetainedBytes()<Pool::TransactionRetainedBytes(65u)+3u*sizeof(void*),
        "Final partial page allocated a full extra page or duplicated hidden journal storage");
    const auto beforeCalls=admission.calls;
    Refuse<std::length_error>([&]{static_cast<void>(pool.Acquire());},"Paged operation ceiling silently grew");
    Require(admission.calls==beforeCalls&&transaction.EntryCount()==65u,
        "Exhausted paged journal invoked allocation admission or consumed an ordinal");
    {DenyAllocation deny;transaction.Retire(provisional.front());}
    Require(transaction.EntryCount()==65u,"Provisional retirement added a paged undo row");
    Lease movedOld(std::move(original));Lease movedNew(std::move(provisional.back()));
    auto moved=std::move(transaction);
    Require(!transaction.Active()&&moved.Active(),"Moving paged transaction lost the stable row domain");
    {DenyAllocation deny;moved.Rollback();}
    Require(admission.calls==beforeCalls&&movedOld.Index()==originalIndex&&!movedNew&&
        !moved.Active()&&!moved.RetainedBytes()&&pool.LiveCount()==1u&&pool.AllocatedSlots()==1u,
        "Paged allocation-free rollback lost moved old identity or retained provisional slots");
    for(const auto& lease:provisional)Require(!lease,"Paged rollback left a provisional lease alive");
    Refuse<std::logic_error>([&]{moved.PrepareNextEntryCapacity();},"Inactive paged capacity preparation succeeded");

    PagedAdmission commitAdmission;auto committed=pool.BeginPagedTransaction(130u,commitAdmission.Borrow());
    committed.Retire(movedOld);std::vector<Lease> survivors;
    for(unsigned i=0u;i<70u;++i)survivors.push_back(pool.Acquire());
    Require(survivors.front().Index()==originalIndex&&committed.AllocatedEntryCapacity()==128u,
        "Paged commit fixture did not exercise stable rows across page and lease-container growth");
    Lease finalOld(std::move(movedOld));const auto calls=commitAdmission.calls;
    {DenyAllocation deny;committed.Commit();}
    Require(!finalOld&&pool.LiveCount()==70u&&commitAdmission.calls==calls,
        "Paged commit restored retired ownership or called allocation admission");
    {DenyAllocation deny;survivors.clear();finalOld=Lease{};}
    Require(pool.LiveCount()==0u,"Committed paged rows remained registered during lease destruction");
}

void PagedAdmissionAndAllocationFailures() {
    Pool pool(256u);auto original=pool.Acquire();const auto initial=original.Index();
    PagedAdmission invalid;
    Refuse<std::invalid_argument>([&]{static_cast<void>(pool.BeginPagedTransaction(0u,invalid.Borrow()));},
        "Paged transaction admitted zero operation ceiling");
    Refuse<std::invalid_argument>([&]{static_cast<void>(pool.BeginPagedTransaction(Pool::MaximumSlots+1u,invalid.Borrow()));},
        "Paged transaction admitted an unbounded operation ceiling");
    Refuse<std::invalid_argument>([&]{static_cast<void>(pool.BeginPagedTransaction(65u,{}));},
        "Paged transaction silently omitted owning-root allocation admission");
    Require(invalid.calls==0u,"Invalid paged request reached allocation admission");
    PagedAdmission denied;denied.rejectCall=1u;
    Refuse<AdmissionRejected>([&]{static_cast<void>(pool.BeginPagedTransaction(65u,denied.Borrow()));},
        "Initial paged header/table bypassed borrowed admission");
    for(const auto allocation:{0u,1u}){
        PagedAdmission allowed;bool failed{};
        {FailAllocationAfter failure(allocation);try{static_cast<void>(pool.BeginPagedTransaction(65u,allowed.Borrow()));}
            catch(const std::bad_alloc&){failed=true;}}
        Require(failed&&original.Index()==initial&&pool.LiveCount()==1u&&pool.AllocatedSlots()==1u,
            "Failed paged header/pointer-table allocation published a partial transaction");++refusals;
    }
    PagedAdmission admission;auto transaction=pool.BeginPagedTransaction(130u,admission.Borrow());
    const auto base=transaction.RetainedBytes();
    Refuse<std::logic_error>([&]{static_cast<void>(pool.BeginPagedTransaction(1u,admission.Borrow()));},
        "Nested paged root replaced the active ownership journal");
    admission.rejectCall=admission.calls+1u;
    Refuse<AdmissionRejected>([&]{static_cast<void>(pool.Acquire());},"First page bypassed allocation admission");
    admission.rejectCall=admission.calls+1u;
    Refuse<AdmissionRejected>([&]{transaction.Retire(original);},"First retired-owner page bypassed admission");
    Require(original.Index()==initial&&pool.LiveCount()==1u&&pool.AllocatedSlots()==1u&&
        transaction.EntryCount()==0u&&transaction.AllocatedEntryCapacity()==0u&&transaction.RetainedBytes()==base,
        "First-page admission failure mutated pool/lease/ordinal or owning capacity");
    admission.rejectCall=std::numeric_limits<std::size_t>::max();bool failed{};
    {DenyAllocation deny;try{static_cast<void>(pool.Acquire());}catch(const std::bad_alloc&){failed=true;}}
    Require(failed&&transaction.EntryCount()==0u&&transaction.RetainedBytes()==base&&pool.AllocatedSlots()==1u,
        "First-page allocator failure consumed an index or retained unowned capacity");++refusals;
    auto first=pool.Acquire();
    for(unsigned i=1u;i<64u;++i){auto temporary=pool.Acquire();}
    Require(transaction.EntryCount()==64u&&transaction.AllocatedEntryCapacity()==64u,
        "Paged failure fixture did not stop at the exact first-page boundary");
    const auto live=pool.LiveCount(),slots=pool.AllocatedSlots();const auto bytes=transaction.RetainedBytes();
    admission.rejectCall=admission.calls+1u;
    Refuse<AdmissionRejected>([&]{static_cast<void>(pool.Acquire());},"Second-page acquisition bypassed admission");
    admission.rejectCall=admission.calls+1u;
    Refuse<AdmissionRejected>([&]{transaction.Retire(original);},"Second-page retirement bypassed admission");
    admission.rejectCall=std::numeric_limits<std::size_t>::max();failed=false;
    {DenyAllocation deny;try{transaction.Retire(original);}catch(const std::bad_alloc&){failed=true;}}
    Require(failed&&original.Index()==initial&&pool.LiveCount()==live&&pool.AllocatedSlots()==slots&&
        transaction.EntryCount()==64u&&transaction.RetainedBytes()==bytes,
        "Later-page admission/allocation failure changed old ownership or exact pool state");++refusals;
    for(unsigned i=64u;i<128u;++i){auto temporary=pool.Acquire();}
    const auto beforeLast=admission.calls;
    transaction.Retire(original);auto reused=pool.Acquire();
    Require(transaction.EntryCount()==130u&&transaction.AllocatedEntryCapacity()==130u&&reused.Index()==initial&&
        admission.calls==beforeLast+1u&&admission.requests[beforeLast]*32u==admission.requests[1u],
        "Last two-row page used full-page storage or changed immediate retired-slot reuse");
    transaction.Retire(reused);const auto calls=admission.calls;
    {DenyAllocation deny;transaction.Rollback();}
    Require(original.Index()==initial&&!first&&!reused&&pool.LiveCount()==1u&&pool.AllocatedSlots()==1u&&admission.calls==calls,
        "Late paged failures/retirement prevented allocation-free exact root rollback");

    PagedAdmission growthAdmission;auto growth=pool.BeginPagedTransaction(256u,growthAdmission.Borrow());
    {auto warm=pool.Acquire();}
    std::array<Lease,128u> retained;std::size_t count{};failed=false;
    std::uint32_t priorLive{},priorSlots{},priorEntries{};
    {DenyAllocation deny;while(count<retained.size()){
        priorLive=pool.LiveCount();priorSlots=pool.AllocatedSlots();priorEntries=growth.EntryCount();
        try{retained[count]=pool.Acquire();++count;}catch(const std::bad_alloc&){failed=true;break;}}}
    Require(failed&&pool.LiveCount()==priorLive&&pool.AllocatedSlots()==priorSlots&&growth.EntryCount()==priorEntries,
        "Slot-vector failure with admitted paged capacity consumed an undo ordinal or index");++refusals;
    {DenyAllocation deny;growth.Rollback();}
    Require(original.Index()==initial&&pool.LiveCount()==1u&&pool.AllocatedSlots()==1u,
        "Paged slot-vector failure did not restore the allocation frontier");
}

void PagedConstructorUnwindAndDomainMoves() {
    Pool pool(16u);auto original=pool.Acquire();PagedAdmission admission;
    auto transaction=pool.BeginPagedTransaction(16u,admission.Borrow());
    struct ThrowingObject {
        Lease lease;
        explicit ThrowingObject(Pool& owner):lease(owner.Acquire()) {throw std::runtime_error("Paged constructor unwind");}
    };
    Refuse<std::runtime_error>([&]{ThrowingObject value(pool);},"Paged constructor did not unwind its provisional lease");
    Require(transaction.EntryCount()==1u&&pool.LiveCount()==1u,"Paged constructor unwind lost its row or leaked ownership");
    transaction.Retire(original);auto reused=pool.Acquire();
    Require(reused.Index()==1u,"Paged constructor unwind changed subsequent retired-slot reuse");
    const auto calls=admission.calls;
    {DenyAllocation deny;transaction.Rollback();}
    Require(original.Index()==1u&&!reused&&pool.LiveCount()==1u&&pool.AllocatedSlots()==1u&&admission.calls==calls,
        "Paged constructor unwind prevented allocation-free full rollback");

    PagedAdmission lifetimeAdmission;Lease survivor;std::optional<Pool::Transaction> surviving;
    {Pool temporary(2u);survivor=temporary.Acquire();
        surviving.emplace(temporary.BeginPagedTransaction(2u,lifetimeAdmission.Borrow()));surviving->Retire(survivor);
        Pool movedPool(std::move(temporary));Require(movedPool.LiveCount()==0u,"Moved pool wrapper changed paged retired state");}
    {DenyAllocation deny;surviving.reset();}
    Require(survivor.Index()==1u,"Paged transaction outliving pool wrapper lost its old lease");
    {DenyAllocation deny;survivor=Lease{};}

    Pool first(2u),second(2u);auto oldFirst=first.Acquire();auto oldSecond=second.Acquire();
    PagedAdmission firstAdmission,secondAdmission;
    auto firstRoot=first.BeginPagedTransaction(2u,firstAdmission.Borrow());
    auto secondRoot=second.BeginPagedTransaction(2u,secondAdmission.Borrow());
    firstRoot.Retire(oldFirst);secondRoot.Retire(oldSecond);
    const auto firstCalls=firstAdmission.calls,secondCalls=secondAdmission.calls;
    {DenyAllocation deny;firstRoot=std::move(secondRoot);}
    Require(oldFirst.Index()==1u&&!oldSecond&&firstRoot.Active()&&!secondRoot.Active(),
        "Paged move assignment did not restore former domain before transferring the new root");
    {DenyAllocation deny;firstRoot.Rollback();}
    Require(oldSecond.Index()==1u&&firstAdmission.calls==firstCalls&&secondAdmission.calls==secondCalls,
        "Paged moved-root undo invoked admission or lost original ownership");
}

void MixedTransactionOracle(bool paged=false) {
    // This oracle stores only abstract live identities and a LIFO vector; it
    // does not reproduce the pool's link journal or owner-registration logic.
    for (std::uint32_t seed=1u;seed<=128u;++seed) {
        Pool pool(64u);std::array<Lease,16u> originals;std::array<Lease,32u> provisional;
        std::array<std::uint32_t,16u> originalIndices{};
        for (std::size_t index=0u;index<originals.size();++index) {
            originals[index]=pool.Acquire();originalIndices[index]=originals[index].Index();
        }
        std::vector<std::uint32_t> free;
        for (const auto index:{2u,5u,0u}) {
            free.push_back(originals[index].Index());originals[index]=Lease{};originalIndices[index]=0u;
        }
        std::uint32_t oracleLive=13u,oracleSlots=16u,random=seed;
        PagedAdmission admission;
        auto transaction=paged?pool.BeginPagedTransaction(512u,admission.Borrow()):pool.BeginTransaction(512u);
        for (unsigned step=0u;step<192u;++step) {
            random=random*1664525u+1013904223u;
            const auto slot=static_cast<std::size_t>((random>>8u)%provisional.size());
            switch((random>>24u)%4u) {
            case 0u:
                if (!provisional[slot]) {
                    const auto expected=free.empty()?++oracleSlots:free.back();
                    if (!free.empty()) free.pop_back();
                    provisional[slot]=pool.Acquire();++oracleLive;
                    Require(provisional[slot].Index()==expected,"Mixed root allocation disagreed with independent LIFO identity oracle");
                }
                break;
            case 1u:
                if (provisional[slot]) {
                    free.push_back(provisional[slot].Index());provisional[slot]=Lease{};--oracleLive;
                }
                break;
            case 2u: {
                const auto oldSlot=static_cast<std::size_t>((random>>12u)%originals.size());
                if (originals[oldSlot]) {
                    free.push_back(originals[oldSlot].Index());transaction.Retire(originals[oldSlot]);--oracleLive;
                }
                break;
            }
            case 3u: {
                const auto destination=(slot+1u)%provisional.size();
                if (provisional[destination]) {
                    free.push_back(provisional[destination].Index());--oracleLive;
                }
                provisional[destination]=std::move(provisional[slot]);
                break;
            }
            }
            Require(pool.LiveCount()==oracleLive && pool.AllocatedSlots()==oracleSlots,
                "Mixed retire/release/move root changed live ownership or allocated slots relative to independent oracle");
        }
        {
            DenyAllocation deny;transaction.Rollback();
        }
        for (std::size_t index=0u;index<originals.size();++index)
            Require(originals[index].Index()==originalIndices[index],"Mixed root rollback did not restore every exact original lease");
        for (const auto& lease:provisional) Require(!lease,"Mixed root rollback left a provisional identity live");
        Require(pool.LiveCount()==13u && pool.AllocatedSlots()==16u,"Mixed root rollback did not restore original scalar pool state");
        auto first=pool.Acquire();auto second=pool.Acquire();auto third=pool.Acquire();auto appended=pool.Acquire();
        Require(first.Index()==1u && second.Index()==6u && third.Index()==3u && appended.Index()==17u,
            "Mixed root rollback did not restore original free-list sequence and slot frontier");
    }
}
}

int main() {
    try {
        LifecycleAndBounds();MoveAndPoolLifetime();ManyObjectsAndContainerMoves();AllocationFailuresAndExceptionUnwind();
        TransactionRetirementAndExactRollback();TransactionCommitMovesAndLifetime();
        TransactionFailuresAndConstructorUnwind();RepeatedTransactionsAndSlotReuse();MixedTransactionOracle();
        PagedBoundariesMovesAndExactRollback();PagedAdmissionAndAllocationFailures();
        PagedConstructorUnwindAndDomainMoves();MixedTransactionOracle(true);
        std::cout<<"Engine-owned UObject index leases: "<<checks<<" checks and "<<refusals<<" refusal controls passed.\n";
        return 0;
    } catch(const std::exception& error) {
        AllocationFixture::refuse=false;std::cerr<<error.what()<<"\n";return 1;
    }
}
