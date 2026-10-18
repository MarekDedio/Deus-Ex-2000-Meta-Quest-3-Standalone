#pragma once

#include "quest_collision_hash_math.h"
#include "quest_multi_line_trace.h"

#include <memory>
#include <type_traits>
#include <unordered_map>

namespace QuestVr {

// Detached retail hash storage. IDs are stable handles, not native addresses,
// saved UObjects or an authorization to use renderer bounding boxes.
struct OriginalHashActor {
    std::uintptr_t identity{};
    PortableModelVec3 location{},colLocation{};
    bool collideActors{},pendingDelete{};
    std::uint32_t collisionTag{};
    std::uint64_t storageGeneration{};
};
struct OriginalHashLink {
    std::uintptr_t actor{};
    std::size_t next{};
    std::uint32_t cell{};
    bool initialized{}; // Native fresh chunk Actor/cell bytes are undefined.
    bool operator==(const OriginalHashLink&) const=default;
};
struct OriginalHashStorage {
    // Fixed-count heap storage avoids making each transaction/test snapshot
    // consume128KiB of the Windows/Android call stack. Count never changes.
    std::vector<std::size_t> heads=std::vector<std::size_t>(OriginalCollisionHashTableSize);
    std::vector<OriginalHashLink> links{1u}; // Link0=None; chunks have1024 slots.
    std::size_t free{},activeLinks{};
    std::uint64_t generation{};
    bool operator==(const OriginalHashStorage&) const=default;
};
struct OriginalHashLimits {
    std::size_t actors{65'536u},hashes{64u},links{1'048'576u},cells{262'144u};
    std::size_t steps{4'194'304u},hits{1'048'576u},depth{64u};
};
struct OriginalHashStats {std::size_t steps{},cells{},links{},primitiveCalls{},hits{};};
struct OriginalHashUsage {
    std::size_t actors{},hashes{},allocatedLinks{},activeLinks{};
    std::size_t snapshotActors{},snapshotHashes{},snapshotAllocatedLinks{},depth{};
    std::uint32_t tag{};
    std::uint64_t nextGeneration{};
    bool prepared{},poisoned{};
};
struct OriginalHashQueries {
    std::function<OriginalCollisionHashBox(std::uintptr_t)> bounds;
    std::function<bool(OriginalActorLineHit&,std::uintptr_t,const PortableModelVec3&,
        const PortableModelVec3&,const PortableModelVec3&,std::uint32_t)> line;
};

// One explicitly owned process collision domain: immutable genuine permutation
// input, shared32-bit tag, actors and independent hash instances. No guessed RNG
// initialization/map lifecycle, brush approximation, VM property or save bridge.
// Every operation joins ONE outer snapshot and work ledger. A caught nested
// refusal poisons that root; all owned actors/tags/hash/free storage roll back.
// Provider-side external writes must join the eventual SAME caller transaction.
class OriginalCollisionHashWorld {
    struct LeaseLifetime {OriginalCollisionHashWorld* owner{};};
public:
    class RootLease;
private:
    struct State {
        std::unordered_map<std::uintptr_t,OriginalHashActor> actors;
        std::unordered_map<std::uint64_t,OriginalHashStorage> hashes;
        std::uint32_t tag{};
        std::uint64_t nextGeneration{};
        std::size_t allocatedLinks{};
    } state_;
    OriginalCollisionHashTables tables_;
    OriginalHashLimits limits_;
    std::unique_ptr<State> before_;
    OriginalHashStats stats_;
    std::size_t depth_{};
    mutable bool poisoned_{};
    std::shared_ptr<LeaseLifetime> leaseLifetime_;
    RootLease* activeLease_{};
    bool prepared_{},abandoned_{};
    [[noreturn]] static void Fail(const char* reason) {
        throw std::runtime_error(std::string("Original collision hash: ")+reason);
    }
    [[noreturn]] void ReadFailure(const char* reason) const {
        if(depth_) poisoned_=true;
        Fail(reason);
    }
    void RestoreRoot() noexcept {
        static_assert(std::is_nothrow_swappable_v<State>);
        using std::swap;swap(state_,*before_);before_.reset();
        depth_=0u;activeLease_=nullptr;prepared_=abandoned_=false;
    }
    void FinishAbandonedRoot() noexcept {
        // A caller can destroy its lease inside a provider. Do not invalidate
        // that provider's active references: poison now, restore only after
        // the LAST nested operation has unwound to the abandoned root frame.
        if(abandoned_&&depth_==1u) RestoreRoot();
    }
    static std::size_t AddBytes(std::size_t a,std::size_t b) noexcept {
        return b>std::numeric_limits<std::size_t>::max()-a?std::numeric_limits<std::size_t>::max():a+b;
    }
    static std::size_t MultiplyBytes(std::size_t a,std::size_t b) noexcept {
        return b&&a>std::numeric_limits<std::size_t>::max()/b?std::numeric_limits<std::size_t>::max():a*b;
    }
    static std::size_t DynamicStateBytes(const State& state) noexcept {
        const auto allocation=[](std::size_t count,std::size_t width) noexcept {
            return AddBytes(MultiplyBytes(count,width),count?4u*sizeof(void*):0u);
        };
        auto bytes=AddBytes(allocation(state.actors.bucket_count(),sizeof(void*)),
            allocation(state.hashes.bucket_count(),sizeof(void*)));
        // Standard-allocator node allowance includes next/hash metadata,
        // alignment and allocation bookkeeping on supported host/Android.
        bytes=AddBytes(bytes,MultiplyBytes(state.actors.size(),sizeof(decltype(state.actors)::value_type)+4u*sizeof(void*)));
        bytes=AddBytes(bytes,MultiplyBytes(state.hashes.size(),sizeof(decltype(state.hashes)::value_type)+4u*sizeof(void*)));
        for(const auto& [id,hash]:state.hashes) {
            static_cast<void>(id);bytes=AddBytes(bytes,allocation(hash.heads.capacity(),sizeof(std::size_t)));
            bytes=AddBytes(bytes,allocation(hash.links.capacity(),sizeof(OriginalHashLink)));
        }
        return bytes;
    }
    class Transaction {
        OriginalCollisionHashWorld& owner_;bool outer_{},closed_{};
    public:
        explicit Transaction(OriginalCollisionHashWorld& owner):owner_(owner),outer_(owner.depth_==0u) {
            if(owner_.prepared_) owner_.ReadFailure("operation attempted after root commit preparation");
            if(outer_) {owner_.before_=std::make_unique<State>(owner_.state_);owner_.stats_={};owner_.poisoned_=false;}
            if(owner_.depth_>=owner_.limits_.depth) {owner_.poisoned_=true;Fail("nested operation depth budget exhausted");}
            ++owner_.depth_;
        }
        void Commit() {
            if(owner_.poisoned_) Fail("nested refusal poisoned the shared root transaction");
            --owner_.depth_;closed_=true;if(outer_) owner_.before_.reset();
        }
        ~Transaction() {
            if(closed_) return;
            owner_.poisoned_=true;--owner_.depth_;
            if(outer_) owner_.RestoreRoot();else owner_.FinishAbandonedRoot();
        }
        Transaction(const Transaction&)=delete;
        Transaction& operator=(const Transaction&)=delete;
    };
    void Step() {if(stats_.steps>=limits_.steps) Fail("shared root work budget exhausted");++stats_.steps;}
    std::uint64_t Generation() {
        if(state_.nextGeneration==std::numeric_limits<std::uint64_t>::max()) Fail("storage generation exhausted");
        return ++state_.nextGeneration;
    }
    OriginalHashStorage& Hash(std::uint64_t identity) {
        const auto found=state_.hashes.find(identity);if(found==state_.hashes.end()) Fail("native hash storage is unavailable");
        return found->second;
    }
    OriginalHashActor& Actor(std::uintptr_t identity) {
        const auto found=state_.actors.find(identity);if(found==state_.actors.end()) Fail("native actor storage is unavailable");
        return found->second;
    }
    OriginalHashLink& Link(std::uint64_t hash,std::size_t identity) {
        auto& storage=Hash(hash);if(!identity||identity>=storage.links.size()) Fail("active link is outside hash storage");
        return storage.links[identity];
    }
    void StillLive(std::uint64_t hash,std::uint64_t hashGeneration,std::uintptr_t actor,std::uint64_t actorGeneration) {
        if(Hash(hash).generation!=hashGeneration||Actor(actor).storageGeneration!=actorGeneration)
            Fail("provider destroyed/replaced active native hash or owner storage");
    }
    OriginalCollisionHashRange Bounds(std::uint64_t hash,std::uintptr_t actor,const OriginalHashQueries& query) {
        if(!query.bounds) Fail("actual primitive has no collision bounding-box provider");
        const auto hg=Hash(hash).generation,ag=Actor(actor).storageGeneration;Step();
        const auto box=query.bounds(actor);StillLive(hash,hg,actor,ag);
        return OriginalCollisionHashBoxRange(box);
    }
    template<class F> void Cells(const OriginalCollisionHashRange& range,F run) {
        const auto count=CheckedOriginalCollisionHashCellCount(range,limits_.cells);
        if(!count) return;
        for(auto x=range.minimum[0];x<=range.maximum[0];++x)
            for(auto y=range.minimum[1];y<=range.maximum[1];++y)
                for(auto z=range.minimum[2];z<=range.maximum[2];++z) {
                    if(stats_.cells>=limits_.cells) Fail("shared root cell-visit budget exhausted");
                    Step();++stats_.cells;
                    run(OriginalCollisionHashBucket(tables_,x,y,z),OriginalCollisionHashEncodedCell(x,y,z));
                }
    }
    std::size_t Allocate(std::uint64_t identity) {
        auto& hash=Hash(identity);
        if(!hash.free) {
            constexpr std::size_t chunk=1024u;
            if(state_.allocatedLinks>limits_.links||chunk>limits_.links-state_.allocatedLinks)
                Fail("native chunk allocation budget exhausted");
            const auto first=hash.links.size();hash.links.resize(first+chunk);
            for(auto i=first;i<first+chunk;++i) {Step();hash.links[i].next=i+1u<first+chunk?i+1u:0u;}
            hash.free=first;state_.allocatedLinks+=chunk;
        }
        const auto result=hash.free;hash.free=Link(identity,result).next;return result;
    }
public:
    // An explicit owning caller root survives between native calls and through
    // every throwing VM publication preparation. The lease is unforgeable and
    // nonmovable; move its unique_ptr, never a raw caller-supplied token. All
    // ordinary operations/WithRoot join this one snapshot and work ledger.
    class RootLease {
        friend class OriginalCollisionHashWorld;
        std::shared_ptr<LeaseLifetime> lifetime_;
        bool closed_{};
        explicit RootLease(std::shared_ptr<LeaseLifetime> lifetime) noexcept:lifetime_(std::move(lifetime)) {}
        OriginalCollisionHashWorld* Owner() const noexcept {return lifetime_?lifetime_->owner:nullptr;}
    public:
        RootLease(const RootLease&)=delete;
        RootLease& operator=(const RootLease&)=delete;
        RootLease(RootLease&&)=delete;
        RootLease& operator=(RootLease&&)=delete;
        ~RootLease() noexcept {
            if(closed_) return;
            auto* owner=Owner();
            if(!owner) return;
            if(!Rollback()&&owner->activeLease_==this) {
                owner->poisoned_=true;owner->prepared_=false;owner->abandoned_=true;
                owner->activeLease_=nullptr;owner->FinishAbandonedRoot();
            }
        }
        bool Active() const noexcept {
            const auto* owner=Owner();return !closed_&&owner&&owner->activeLease_==this&&owner->depth_!=0u;
        }
        void PrepareCommit() {
            auto* owner=Owner();if(!owner) Fail("root lease outlived its collision domain");
            if(closed_||owner->activeLease_!=this) owner->ReadFailure("root lease is closed or not the active owner");
            if(owner->depth_!=1u) owner->ReadFailure("root commit attempted with a live child operation");
            if(owner->poisoned_) Fail("nested refusal poisoned the shared root transaction");
            if(owner->prepared_) owner->ReadFailure("root commit was already prepared");
            owner->prepared_=true;
        }
        // PrepareCommit is the LAST throwing hash call. This frozen, CPU-owner
        // publication step never allocates, calls a provider or throws. False
        // means protocol misuse: nothing was published and the live root is
        // poisoned. A VM must not publish any other state on that result.
        [[nodiscard]] bool CommitPrepared() noexcept {
            auto* owner=Owner();
            if(closed_||!owner||owner->activeLease_!=this||owner->depth_!=1u||!owner->prepared_||owner->poisoned_) {
                if(owner&&owner->depth_) owner->poisoned_=true;
                return false;
            }
            owner->before_.reset();owner->depth_=0u;owner->activeLease_=nullptr;owner->prepared_=false;
            closed_=true;return true;
        }
        // No live child may be invalidated by swapping its backing storage.
        // A rejected nested rollback poisons the root; destruction defers the
        // actual restore until the child's Transaction has unwound safely.
        [[nodiscard]] bool Rollback() noexcept {
            auto* owner=Owner();
            if(closed_||!owner||owner->activeLease_!=this) {
                if(owner&&owner->depth_) owner->poisoned_=true;
                return false;
            }
            if(owner->depth_!=1u) {owner->poisoned_=true;return false;}
            owner->RestoreRoot();closed_=true;return true;
        }
    };
    explicit OriginalCollisionHashWorld(OriginalCollisionHashTables tables,std::uint32_t initialTag=0u,
        const OriginalHashLimits& limits={}):tables_(std::move(tables)),limits_(limits) {
        const OriginalHashLimits max;
        if(!limits.actors||limits.actors>max.actors||!limits.hashes||limits.hashes>max.hashes||
            !limits.links||limits.links>max.links||!limits.cells||limits.cells>max.cells||
            !limits.steps||limits.steps>max.steps||!limits.hits||limits.hits>max.hits||!limits.depth||limits.depth>max.depth)
            Fail("callers may only tighten fixed positive limits");
        ValidateOriginalCollisionHashTables(tables_);state_.tag=initialTag;
    }
    ~OriginalCollisionHashWorld() noexcept {if(leaseLifetime_) leaseLifetime_->owner=nullptr;}
    OriginalCollisionHashWorld(const OriginalCollisionHashWorld&)=delete;
    OriginalCollisionHashWorld& operator=(const OriginalCollisionHashWorld&)=delete;
    [[nodiscard]] std::unique_ptr<RootLease> BeginRoot() {
        if(depth_||activeLease_||before_) ReadFailure("explicit root requires an unowned caller boundary");
        // Allocate EVERYTHING before publishing ownership/depth. Any failed
        // copy/allocation leaves the original state, ledger and owner intact.
        auto lifetime=leaseLifetime_;
        if(!lifetime) lifetime=std::make_shared<LeaseLifetime>(LeaseLifetime{this});
        auto before=std::make_unique<State>(state_);
        auto lease=std::unique_ptr<RootLease>(new RootLease(lifetime));
        before_=std::move(before);leaseLifetime_=std::move(lifetime);activeLease_=lease.get();
        stats_={};poisoned_=prepared_=abandoned_=false;depth_=1u;return lease;
    }
    // Honest retained ownership, including immutable tables, real container
    // capacities/buckets, conservative node/allocator overhead and the entire
    // active undo snapshot. Saturation cannot wrap a caller's byte budget.
    // Closed leases retained by an external caller are that caller's storage.
    std::size_t RetainedBytes() const noexcept {
        auto bytes=AddBytes(sizeof(*this)+4u*sizeof(void*),DynamicStateBytes(state_));
        if(before_) bytes=AddBytes(bytes,AddBytes(sizeof(State)+4u*sizeof(void*),DynamicStateBytes(*before_)));
        if(leaseLifetime_) bytes=AddBytes(bytes,sizeof(LeaseLifetime)+4u*sizeof(void*));
        if(activeLease_) bytes=AddBytes(bytes,sizeof(RootLease)+4u*sizeof(void*));
        return bytes;
    }
    // Preflight this EXTRA amount before BeginRoot allocates its snapshot.
    // Copies cannot require more element capacity than this retained source
    // envelope on the supported standard-container implementations.
    std::size_t AdditionalRootBytes() const noexcept {
        auto bytes=AddBytes(sizeof(State)+4u*sizeof(void*),DynamicStateBytes(state_));
        bytes=AddBytes(bytes,sizeof(RootLease)+4u*sizeof(void*));
        if(!leaseLifetime_) bytes=AddBytes(bytes,sizeof(LeaseLifetime)+4u*sizeof(void*));
        return bytes;
    }
    // Membership is a non-poisoning preflight, not a fabricated actor/hash
    // record. Reads requiring actual storage still use checked ReadActor/Hash.
    bool ContainsActor(std::uintptr_t actor) const noexcept {return state_.actors.find(actor)!=state_.actors.end();}
    bool ContainsHash(std::uint64_t hash) const noexcept {return state_.hashes.find(hash)!=state_.hashes.end();}
    OriginalHashUsage ReadUsage() const noexcept {
        OriginalHashUsage result;result.actors=state_.actors.size();result.hashes=state_.hashes.size();
        result.allocatedLinks=state_.allocatedLinks;result.tag=state_.tag;result.nextGeneration=state_.nextGeneration;
        result.depth=depth_;result.prepared=prepared_;result.poisoned=poisoned_;
        for(const auto& [id,hash]:state_.hashes) {static_cast<void>(id);result.activeLinks+=hash.activeLinks;}
        if(before_) {
            result.snapshotActors=before_->actors.size();result.snapshotHashes=before_->hashes.size();
            result.snapshotAllocatedLinks=before_->allocatedLinks;
        }
        return result;
    }
    // Group a complete caller operation, not just each native subcall. This
    // lets a future VM root retain hash changes until its LAST script effect
    // succeeds. It does not itself journal external script/RNG/render state.
    template<class F> auto WithRoot(F&& run) {
        Transaction transaction(*this);
        if constexpr(std::is_void_v<std::invoke_result_t<F,OriginalCollisionHashWorld&>>) {
            std::invoke(std::forward<F>(run),*this);transaction.Commit();
        } else {
            using Result=std::invoke_result_t<F,OriginalCollisionHashWorld&>;
            static_assert(!std::is_reference_v<Result>&&std::is_nothrow_move_constructible_v<Result>&&
                std::is_nothrow_destructible_v<Result>,"Collision root results must materialize without failure after commit");
            auto result=std::invoke(std::forward<F>(run),*this);transaction.Commit();return result;
        }
    }
    std::uint64_t CreateHash() {
        Transaction transaction(*this);Step();
        if(state_.hashes.size()>=limits_.hashes) Fail("hash count budget exhausted");
        const auto id=Generation();OriginalHashStorage hash;hash.generation=id;
        state_.hashes.emplace(id,std::move(hash));transaction.Commit();return id;
    }
    void DestroyHash(std::uint64_t hash) {
        Transaction transaction(*this);Step();
        state_.allocatedLinks-=Hash(hash).links.size()-1u;state_.hashes.erase(hash);transaction.Commit();
    }
    void DefineActor(OriginalHashActor actor) {
        Transaction transaction(*this);Step();
        if(!actor.identity||state_.actors.contains(actor.identity)) Fail("actor storage identity is null or already present");
        if(state_.actors.size()>=limits_.actors) Fail("actor count budget exhausted");
        actor.storageGeneration=Generation();state_.actors.emplace(actor.identity,actor);transaction.Commit();
    }
    void SetActorLive(std::uintptr_t actor,PortableModelVec3 location,bool collideActors,bool pendingDelete) {
        Transaction transaction(*this);Step();auto& current=Actor(actor);
        current.location=location;current.collideActors=collideActors;current.pendingDelete=pendingDelete;transaction.Commit();
    }
    void DestroyActorStorage(std::uintptr_t actor) {
        Transaction transaction(*this);Step();static_cast<void>(Actor(actor));state_.actors.erase(actor);transaction.Commit();
    }
    OriginalHashActor ReadActor(std::uintptr_t actor) const {
        if(prepared_) ReadFailure("actor read attempted after root commit preparation");
        const auto found=state_.actors.find(actor);if(found==state_.actors.end()) ReadFailure("actor read has no storage");return found->second;
    }
    OriginalHashStorage ReadHash(std::uint64_t hash) const {
        if(prepared_) ReadFailure("hash snapshot read attempted after root commit preparation");
        const auto found=state_.hashes.find(hash);if(found==state_.hashes.end()) ReadFailure("hash read has no storage");
        try {return found->second;}
        catch(...) {if(depth_) poisoned_=true;throw;}
    }
    std::uint32_t ReadTag() const {return state_.tag;}
    OriginalHashStats ReadStats() const {return stats_;}
    void Add(std::uint64_t hash,std::uintptr_t actor,const OriginalHashQueries& query) {
        Transaction transaction(*this);Step();static_cast<void>(Hash(hash));
        if(!Actor(actor).collideActors) Fail("AddActor requires bCollideActors before pending-delete gate");
        if(Actor(actor).pendingDelete) {transaction.Commit();return;}
        const auto range=Bounds(hash,actor,query);
        Cells(range,[&](std::uint32_t bucket,std::uint32_t cell) {
            const auto slot=Allocate(hash);auto& storage=Hash(hash);
            storage.links[slot]={actor,storage.heads[bucket],cell,true};storage.heads[bucket]=slot;++storage.activeLinks;
        });
        Actor(actor).colLocation=Actor(actor).location;transaction.Commit();
    }
    void Remove(std::uint64_t hash,std::uintptr_t actor,const OriginalHashQueries& query) {
        Transaction transaction(*this);Step();static_cast<void>(Hash(hash));
        if(!Actor(actor).collideActors) Fail("RemoveActor requires bCollideActors before pending-delete gate");
        if(Actor(actor).pendingDelete) {transaction.Commit();return;}
        // Location!=ColLocation is only an original diagnostic, no repair or
        // cached-box substitution. This domain omits diagnostic counters/logs.
        const auto range=Bounds(hash,actor,query);
        Cells(range,[&](std::uint32_t bucket,std::uint32_t) {
            auto& storage=Hash(hash);std::size_t previous{},current=storage.heads[bucket];
            while(current) {
                Step();++stats_.links;auto& link=Link(hash,current);
                if(!link.initialized) Fail("removal observed undefined fresh link metadata");
                const auto next=link.next;
                if(link.actor==actor) {
                    if(previous) storage.links[previous].next=next;else storage.heads[bucket]=next;
                    link.next=storage.free;storage.free=current;--storage.activeLinks;
                } else previous=current;
                current=next;
            }
        });
        transaction.Commit();
    }
    OriginalLineHitArena ActorLineCheck(std::uint64_t hash,PortableModelVec3 end,PortableModelVec3 start,
        PortableModelVec3 extent,std::uint8_t flags,const OriginalHashQueries& query) {
        Transaction transaction(*this);Step();const auto hashGeneration=Hash(hash).generation;
        ++state_.tag; // Unsigned32 native wrap; NEVER reset actor tags on wrap.
        const auto range=OriginalCollisionHashSweptRange(end,start,extent);OriginalLineHitArena result;
        Cells(range,[&](std::uint32_t bucket,std::uint32_t cell) {
            auto current=Hash(hash).heads[bucket];
            while(current) {
                Step();++stats_.links;const auto link=Link(hash,current);
                if(!link.initialized) Fail("query observed undefined fresh link metadata");
                auto& actor=Actor(link.actor);
                if(actor.collisionTag!=state_.tag&&link.cell==cell) {
                    if(!query.line) Fail("actual primitive has no native LineCheck provider");
                    OriginalActorLineHit hit;hit.time=0.0f; // Original hash seed, not defaultTime1.
                    const auto actorGeneration=actor.storageGeneration;
                    actor.collisionTag=state_.tag;Step();++stats_.primitiveCalls;
                    const bool clear=query.line(hit,link.actor,end,start,extent,static_cast<std::uint32_t>(flags));
                    StillLive(hash,hashGeneration,link.actor,actorGeneration);
                    if(!clear) {
                        if(stats_.hits>=limits_.hits) Fail("shared query hit allocation budget exhausted");
                        ++stats_.hits;hit.next=result.first;result.records.push_back(hit);result.first=result.records.size();
                    }
                }
                // Native re-reads this SAME link address AFTER callback, even
                // when it is now free or reused. Never cache pre-call Next.
                current=Link(hash,current).next;
            }
        });
        transaction.Commit();return result;
    }
};
} // namespace QuestVr
