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
}

// This standalone test denies allocation at precise scope boundaries. The
// production pool has no test allocator hook and no mutable numeric release API.
void* operator new(std::size_t bytes) {
    if (AllocationFixture::refuse) throw std::bad_alloc();
    if (void* memory=std::malloc(bytes==0u?1u:bytes)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory,std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory,std::size_t) noexcept { std::free(memory); }

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
void MoveAssign(Lease& destination,Lease& source) {
    destination=std::move(source);
}

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
}

int main() {
    try {
        LifecycleAndBounds();MoveAndPoolLifetime();ManyObjectsAndContainerMoves();AllocationFailuresAndExceptionUnwind();
        std::cout<<"Engine-owned UObject index leases: "<<checks<<" checks and "<<refusals<<" refusal controls passed.\n";
        return 0;
    } catch(const std::exception& error) {
        AllocationFixture::refuse=false;std::cerr<<error.what()<<"\n";return 1;
    }
}
