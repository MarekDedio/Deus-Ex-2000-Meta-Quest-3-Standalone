#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

// Test-only independent state/order oracle for the statically audited retail
// collision hash. It deliberately represents membership as vectors, NOT a
// duplicate of the production singly linked slot pool. No production header,
// hash arithmetic helper, RNG or query is called. The test driver supplies the
// independently determined bucket/encoded-cell visits and owns shared tag/actor
// state; this class tests exact membership, duplicate Add and free/reuse order.
namespace QuestVr::CollisionHashOracle {

struct Node {
    std::uintptr_t actor{};
    std::uint32_t cell{};
    // Newly allocated native links have only Next initialized. Actor/cell are
    // undefined until first Add, and remain stale (defined) after Remove.
    bool initialized{};
    bool operator==(const Node&) const =default;
};

class Oracle {
public:
    using Id=std::size_t;
    using Cell=std::pair<std::uint32_t,std::uint32_t>; // bucket, encoded cell
    using Buckets=std::array<std::vector<Id>,16'384u>;
    static constexpr std::size_t ChunkSize=1'024u;

    Oracle():slots_(1u) {} // ID0 is None, not an allocated link.

    // Cell order is actual inclusive X/Y/Z iteration supplied by the driver.
    // A repeated Add intentionally keeps earlier links, including equal cells.
    // Like the native ordinary Add body this does not change actor ColLocation;
    // that actor-side write/gates are the driver's independently tested scope.
    void Add(std::uintptr_t actor,const std::vector<Cell>& cells) {
        if (!actor) Fail("Add requires a non-null actor identity");
        for (const auto& [bucket,encoded]:cells) {
            CheckBucket(bucket);
            if (free_.empty()) AllocateChunk();
            const auto id=free_.front();free_.erase(free_.begin());
            slots_[id]={actor,encoded,true};
            buckets_[bucket].insert(buckets_[bucket].begin(),id);
        }
    }

    // This list contains buckets visited by CURRENT recomputed live bounds,
    // not registration snapshots. Repeated buckets are permitted. Cell values
    // are deliberately absent: native Remove does NOT compare encoded cells.
    void Remove(std::uintptr_t actor,const std::vector<std::uint32_t>& visits) {
        if (!actor) Fail("Remove requires a non-null actor identity");
        for (const auto bucket:visits) {
            CheckBucket(bucket);auto& members=buckets_[bucket];
            for (std::size_t at=0;at<members.size();) {
                const auto id=members[at];
                if (slots_[id].actor!=actor) {++at;continue;}
                members.erase(members.begin()+static_cast<std::ptrdiff_t>(at));
                // Actor/cell are NOT cleared. Removal order head-to-tail and
                // prepend-to-free makes the freed subsequence reverse order.
                free_.insert(free_.begin(),id);
            }
        }
    }

    Id Head(std::uint32_t bucket) const {
        CheckBucket(bucket);return buckets_[bucket].empty()?0u:buckets_[bucket].front();
    }

    // Locate the node in its CURRENT membership sequence every time. This is
    // essential for callbacks removing/reusing the very link being traversed:
    // original ActorLineCheck reads that same link address's Next AFTER return.
    // A freed slot's Next is its current free-list successor, not an old bucket
    // successor. No active-only shortcut or cached traversal Next is valid.
    Id Next(Id id) const {
        CheckId(id);
        for (const auto& members:buckets_) {
            const auto found=std::find(members.begin(),members.end(),id);
            if (found!=members.end()) return Following(members,found);
        }
        const auto found=std::find(free_.begin(),free_.end(),id);
        if (found!=free_.end()) return Following(free_,found);
        Fail("allocated slot is absent from both bucket and free membership");
    }

    const Node& Slot(Id id) const {CheckId(id);return slots_[id];}
    const Buckets& BucketsView() const noexcept {return buckets_;}
    const std::vector<Id>& FreeView() const noexcept {return free_;}
    const std::vector<Node>& SlotsView() const noexcept {return slots_;}
    std::size_t ChunkCount() const noexcept {return (slots_.size()-1u)/ChunkSize;}
    std::size_t LinkCount() const noexcept {return slots_.size()-1u-free_.size();}
    bool SameState(const Oracle& other) const {
        return buckets_==other.buckets_&&free_==other.free_&&slots_==other.slots_;
    }
    bool operator==(const Oracle& other) const {return SameState(other);}

    // Independent structural invariant proof, useful after every randomized
    // operation/callback. Every allocated slot occurs exactly once, active
    // members have defined actor/cell, and chunks remain wholly allocated.
    void Validate() const {
        if (slots_.empty()||(slots_.size()-1u)%ChunkSize) Fail("partial or absent native-size slot chunk");
        if (slots_[0]!=Node{}) Fail("None sentinel payload changed");
        std::vector<bool> seen(slots_.size());seen[0]=true;
        for (const auto& members:buckets_)
            for (const auto id:members) {
                CheckId(id);
                if (seen[id]) Fail("slot occurs in more than one membership position");
                seen[id]=true;
                if (!slots_[id].initialized||!slots_[id].actor) Fail("active link has undefined actor/cell payload");
            }
        for (const auto id:free_) {
            CheckId(id);
            if (seen[id]) Fail("free slot occurs in more than one membership position");
            seen[id]=true;
        }
        if (std::find(seen.begin(),seen.end(),false)!=seen.end()) Fail("allocated slot has no membership");
    }

private:
    Buckets buckets_;
    std::vector<Id> free_;
    std::vector<Node> slots_;

    [[noreturn]] static void Fail(const char* reason) {
        throw std::runtime_error(reason);
    }
    static void CheckBucket(std::uint32_t bucket) {
        if (bucket>=16'384u) Fail("oracle bucket outside native 16384 heads");
    }
    void CheckId(Id id) const {
        if (!id||id>=slots_.size()) Fail("oracle link handle outside allocated native-size chunks");
    }
    static Id Following(const std::vector<Id>& members,std::vector<Id>::const_iterator at) {
        ++at;return at==members.end()?0u:*at;
    }
    void AllocateChunk() {
        const auto begin=slots_.size();slots_.resize(begin+ChunkSize);
        // Allocation only happens with an empty free list. Consecutive native
        // addresses are linked increasing, final Next=None. IDs preserve that
        // order while intentionally not pretending to be native pointers.
        for (auto id=begin;id<begin+ChunkSize;++id) free_.push_back(id);
    }
};

} // namespace QuestVr::CollisionHashOracle
