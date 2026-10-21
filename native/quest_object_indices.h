#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace QuestVr {

// Engine-owned, process-local UObject indices. An index is an allocation-slot
// identity, never an address, package/export/world slot, path hash or save ID.
// Zero denotes no lease. The four-million bound is portable resource policy,
// not a claim about the retail UObject array's capacity or absolute indices.
// All access is serialized by the owning engine; this is not a concurrent pool.
class ObjectIndexPool {
public:
    class Lease;
    class Transaction;
    static constexpr std::uint32_t MaximumSlots = 4'000'000u;
    // Borrowed from the owning execution root. It admits additional live
    // allocation bytes, not a private budget, and must not mutate this pool.
    // The context must outlive the transaction; undo never invokes it.
    struct JournalAllocationAdmission {
        void* context{};
        void (*reserve)(void*,std::size_t){};
    };

private:
    static constexpr std::uint32_t RetiredEntry = 0x80000000u;
    static_assert(MaximumSlots<RetiredEntry);
    struct JournalEntry {
        Lease* owner{};
        std::uint32_t index{};
        // Free links are bounded below RetiredEntry, so the otherwise unused
        // high bit carries the entry kind without growing every journal row.
        std::uint32_t previousLinkAndKind{};
        [[nodiscard]] bool Retired() const noexcept { return (previousLinkAndKind&RetiredEntry)!=0u; }
        [[nodiscard]] std::uint32_t PreviousLink() const noexcept { return previousLinkAndKind&~RetiredEntry; }
    };
    struct TransactionState {
        static constexpr std::uint32_t PageEntries=64u;
        static constexpr std::uint32_t PageCount(std::uint32_t maximum) noexcept {
            return (maximum+PageEntries-1u)/PageEntries;
        }
        TransactionState(std::uint32_t maximum,std::uint32_t first,std::uint32_t live,std::uint32_t slots)
            : entries(std::make_unique<JournalEntry[]>(maximum)),maximum(maximum),
              initialFirst(first),initialLive(live),initialSlots(slots),allocatedEntries(maximum) {}
        TransactionState(std::uint32_t maximum,std::uint32_t first,std::uint32_t live,std::uint32_t slots,
            JournalAllocationAdmission borrowed)
            : pages(std::make_unique<JournalEntry*[]>(PageCount(maximum))),
              admission(borrowed),maximum(maximum),initialFirst(first),initialLive(live),initialSlots(slots) {}
        ~TransactionState() {
            if(pages)for(std::uint32_t i=0u;i<PageCount(maximum);++i)delete[] pages[i];
        }
        [[nodiscard]] JournalEntry& EntryAt(std::uint32_t index) noexcept {
            if(index>=count)std::terminate();
            if(entries)return entries[index];
            const auto& page=pages[index/PageEntries];
            if(!page)std::terminate();
            return page[index%PageEntries];
        }
        // Admission and page allocation precede all pool/lease/count writes.
        // A later slot-vector failure may retain this capacity, but it cannot
        // consume an ordinal or alter the free list and is fully measured.
        void PrepareNextEntry() {
            if(count==maximum)throw std::length_error("UObject index transaction journal exhausted");
            if(entries)return; // Existing fixed mode retains its eager contract.
            if(admitting)throw std::logic_error("Reentrant UObject index journal allocation admission");
            const auto pageIndex=count/PageEntries;
            if(pages[pageIndex])return;
            const auto remaining=maximum-pageIndex*PageEntries;
            const auto capacity=remaining<PageEntries?remaining:PageEntries;
            struct Scope {bool& active;~Scope(){active=false;}} scope{admitting};
            admitting=true;
            admission.reserve(admission.context,static_cast<std::size_t>(capacity)*sizeof(JournalEntry));
            auto page=std::make_unique<JournalEntry[]>(capacity);
            pages[pageIndex]=page.release();allocatedEntries+=capacity;
        }
        [[nodiscard]] std::size_t RetainedBytes() const noexcept {
            return sizeof(TransactionState)+static_cast<std::size_t>(allocatedEntries)*sizeof(JournalEntry)+
                (pages?static_cast<std::size_t>(PageCount(maximum))*sizeof(JournalEntry*):0u);
        }
        std::unique_ptr<JournalEntry[]> entries;
        // Trivial pointer elements avoid a nontrivial-array cookie. This state
        // owns every nonnull page; the explicit destructor frees them all.
        std::unique_ptr<JournalEntry*[]> pages;
        JournalAllocationAdmission admission;
        std::uint32_t maximum{},count{},initialFirst{},initialLive{},initialSlots{},allocatedEntries{};
        bool admitting{};
    };
    struct Control {
        explicit Control(std::uint32_t maximum) : maximum(maximum) {}
        std::vector<std::uint32_t> nextFree;
        std::uint32_t maximum;
        std::uint32_t firstFree{};
        std::uint32_t live{};
        TransactionState* transaction{};
    };

public:
    // Only Acquire creates a lease, and only its private release path can
    // return an index. Move transfers that authority; no public numeric release
    // API can accidentally free a stale, duplicate or foreign object's index.
    class Lease {
    public:
        Lease() noexcept = default;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&& other) noexcept { Take(other); }
        Lease& operator=(Lease&& other) noexcept {
            if (this!=&other) {
                Release();
                Take(other);
            }
            return *this;
        }
        ~Lease() { Release(); }

        [[nodiscard]] std::uint32_t Index() const noexcept { return index_; }
        explicit operator bool() const noexcept { return index_!=0u; }

    private:
        friend class ObjectIndexPool;
        friend class Transaction;
        Lease(std::shared_ptr<Control> control,std::uint32_t index,std::uint32_t journalEntry=0u) noexcept
            : control_(std::move(control)), index_(index),journalEntry_(journalEntry) {
            if (journalEntry_!=0u) Registration().owner=this;
        }
        [[nodiscard]] JournalEntry& Registration() const noexcept {
            if (!control_ || !control_->transaction || journalEntry_==0u ||
                journalEntry_>control_->transaction->count) std::terminate();
            return control_->transaction->EntryAt(journalEntry_-1u);
        }
        void Take(Lease& other) noexcept {
            // A live pre-root owner's field is part of the retained object
            // state, not a new provisional constructor temporary. Moving that
            // unregistered owner would bypass the rollback address journal.
            if (other.index_!=0u && other.journalEntry_==0u && other.control_->transaction) std::terminate();
            control_=std::move(other.control_);
            index_=std::exchange(other.index_,0u);
            journalEntry_=std::exchange(other.journalEntry_,0u);
            if (journalEntry_!=0u) {
                auto& entry=Registration();
                if (entry.owner!=&other) std::terminate();
                entry.owner=this;
            }
        }
        void Release() noexcept {
            if (journalEntry_!=0u) {
                auto& entry=Registration();
                // A retired pre-root UObject must remain alive, retaining this
                // undo authority until commit/rollback. Destroying/overwriting
                // it cannot be made reversible by any numeric index journal.
                if (entry.owner!=this || entry.Retired() || index_!=entry.index) std::terminate();
                entry.owner=nullptr;
                journalEntry_=0u;
            } else if (index_!=0u && control_->transaction) {
                // Existing objects cannot be destructed inside a reversible
                // root. Explicit Retire, with retained object lifetime, is the
                // only reversible release of a pre-root allocation authority.
                std::terminate();
            }
            if (index_==0u) return;
            // The slot's existing link storage is the free list. Destruction
            // never allocates, throws or depends on the pool wrapper's lifetime.
            auto& control=*control_;
            control.nextFree[static_cast<std::size_t>(index_-1u)]=control.firstFree;
            control.firstFree=index_;
            --control.live;
            index_=0u;
            control_.reset();
        }
        std::shared_ptr<Control> control_;
        std::uint32_t index_{};
        // On the supported 64-bit targets this uses Lease's former tail
        // padding. An ordinal, not an allocation or independently owned lease.
        std::uint32_t journalEntry_{};
    };

    // Pool-local reversible allocation domain. All Acquires in the domain are
    // registered, including unrelated Actor/Window construction. Provisional
    // leases may move or destruct on constructor unwind. Pre-root leases may
    // be explicitly retired and moved into empty leases, but must remain alive
    // and must not be overwritten until this transaction ends. Engine GC must
    // therefore stay outside the root. No rollback operation allocates.
    class Transaction {
    public:
        Transaction(const Transaction&)=delete;
        Transaction& operator=(const Transaction&)=delete;
        Transaction(Transaction&& other) noexcept
            : control_(std::move(other.control_)),state_(std::move(other.state_)) {}
        Transaction& operator=(Transaction&& other) noexcept {
            if (this!=&other) {
                Rollback();control_=std::move(other.control_);state_=std::move(other.state_);
            }
            return *this;
        }
        ~Transaction() { Rollback(); }

        [[nodiscard]] bool Active() const noexcept { return state_!=nullptr; }
        [[nodiscard]] std::uint32_t EntryCount() const noexcept { return state_?state_->count:0u; }
        // The operation ceiling stays independent of paged owning capacity.
        [[nodiscard]] std::uint32_t EntryCapacity() const noexcept { return state_?state_->maximum:0u; }
        [[nodiscard]] std::uint32_t AllocatedEntryCapacity() const noexcept { return state_?state_->allocatedEntries:0u; }
        [[nodiscard]] std::size_t RetainedBytes() const noexcept {
            return state_?state_->RetainedBytes():0u;
        }
        // Pre-admit the next row before a caller admits/allocates an owning
        // UObject. This exposes the combined page+object peak to that same
        // root budget without consuming an ordinal or mutating pool/leases.
        // Acquire and old-lease Retire still self-prepare for every caller.
        void PrepareNextEntryCapacity() {
            if(!state_)throw std::logic_error("Cannot prepare an inactive UObject index transaction");
            state_->PrepareNextEntry();
        }

        void Retire(Lease& lease) {
            if (!state_) throw std::logic_error("Cannot retire through an inactive UObject index transaction");
            if (lease.control_.get()!=control_.get() || lease.index_==0u)
                throw std::invalid_argument("UObject index retirement requires this pool's live lease");
            if (lease.journalEntry_!=0u) {
                // A new object's Acquire already captured its link; retirement
                // needs no second journal row and must never resurrect it.
                lease.Release();return;
            }
            state_->PrepareNextEntry();
            const auto index=lease.index_;
            const auto ordinal=state_->count++;
            state_->EntryAt(ordinal)={&lease,index,
                control_->nextFree[static_cast<std::size_t>(index-1u)]|RetiredEntry};
            lease.journalEntry_=state_->count;
            control_->nextFree[static_cast<std::size_t>(index-1u)]=control_->firstFree;
            control_->firstFree=index;--control_->live;lease.index_=0u;
        }

        void Commit() noexcept {
            if (!state_) return;
            for (std::uint32_t ordinal=1u;ordinal<=state_->count;++ordinal) {
                auto& entry=state_->EntryAt(ordinal-1u);
                if (!entry.owner) continue;
                auto& lease=*entry.owner;
                ValidateOwner(lease,entry,ordinal);
                lease.journalEntry_=0u;
                if (entry.Retired()) lease.control_.reset();
            }
            Finish();
        }
        void Rollback() noexcept {
            if (!state_) return;
            // Invalidate all surviving provisional owners before restoring old
            // leases: the same numeric slot can legitimately have both rows.
            for (std::uint32_t ordinal=1u;ordinal<=state_->count;++ordinal) {
                auto& entry=state_->EntryAt(ordinal-1u);
                if (!entry.owner) continue;
                auto& lease=*entry.owner;
                ValidateOwner(lease,entry,ordinal);
                lease.journalEntry_=0u;
                if (entry.Retired()) lease.index_=entry.index;
                else {lease.index_=0u;lease.control_.reset();}
            }
            for (auto remaining=state_->count;remaining!=0u;--remaining) {
                const auto& entry=state_->EntryAt(remaining-1u);
                control_->nextFree[static_cast<std::size_t>(entry.index-1u)]=entry.PreviousLink();
            }
            control_->nextFree.resize(state_->initialSlots);
            control_->firstFree=state_->initialFirst;control_->live=state_->initialLive;
            Finish();
        }

    private:
        friend class ObjectIndexPool;
        Transaction(std::shared_ptr<Control> control,std::uint32_t maximum)
            : control_(std::move(control)) {
            if (control_->transaction)
                throw std::logic_error("Nested UObject index transactions are not supported");
            // Allocate the complete bounded journal before publishing its
            // pointer or touching any pool/lease state.
            state_=std::make_unique<TransactionState>(maximum,control_->firstFree,
                control_->live,static_cast<std::uint32_t>(control_->nextFree.size()));
            control_->transaction=state_.get();
        }
        Transaction(std::shared_ptr<Control> control,std::uint32_t maximum,JournalAllocationAdmission admission)
            : control_(std::move(control)) {
            if(control_->transaction)
                throw std::logic_error("Nested UObject index transactions are not supported");
            if(!admission.reserve)
                throw std::invalid_argument("Paged UObject index journal lacks owning-root allocation admission");
            // No active transaction is published until both header/table
            // allocations succeed. The table never grows or relocates pages.
            admission.reserve(admission.context,PagedTransactionInitialRetainedBytes(maximum));
            state_=std::make_unique<TransactionState>(maximum,control_->firstFree,control_->live,
                static_cast<std::uint32_t>(control_->nextFree.size()),admission);
            control_->transaction=state_.get();
        }
        void ValidateOwner(const Lease& lease,const JournalEntry& entry,std::uint32_t ordinal) const noexcept {
            if (control_->transaction!=state_.get() || lease.control_.get()!=control_.get() ||
                lease.journalEntry_!=ordinal || lease.index_!=(entry.Retired()?0u:entry.index)) std::terminate();
        }
        void Finish() noexcept {
            control_->transaction=nullptr;state_.reset();control_.reset();
        }
        std::shared_ptr<Control> control_;
        std::unique_ptr<TransactionState> state_;
    };

    [[nodiscard]] static std::size_t TransactionRetainedBytes(std::uint32_t maximumEntries) {
        if (maximumEntries==0u || maximumEntries>MaximumSlots)
            throw std::invalid_argument("UObject index journal capacity is outside portable policy");
        return sizeof(TransactionState)+static_cast<std::size_t>(maximumEntries)*sizeof(JournalEntry);
    }

    [[nodiscard]] Transaction BeginTransaction(std::uint32_t maximumEntries) {
        if (!control_) throw std::logic_error("Cannot transact a moved-from UObject index pool");
        static_cast<void>(TransactionRetainedBytes(maximumEntries));
        return Transaction(control_,maximumEntries);
    }

    [[nodiscard]] static std::size_t PagedTransactionInitialRetainedBytes(std::uint32_t maximumEntries) {
        static_cast<void>(TransactionRetainedBytes(maximumEntries));
        return sizeof(TransactionState)+static_cast<std::size_t>(TransactionState::PageCount(maximumEntries))*
            sizeof(JournalEntry*);
    }
    // Optional representation only: the SAME maximum operation count and
    // identity/free-list/undo semantics, with actual pages admitted on demand.
    // BeginTransaction above deliberately keeps its complete-upfront contract.
    [[nodiscard]] Transaction BeginPagedTransaction(std::uint32_t maximumEntries,JournalAllocationAdmission admission) {
        if(!control_)throw std::logic_error("Cannot transact a moved-from UObject index pool");
        static_cast<void>(PagedTransactionInitialRetainedBytes(maximumEntries));
        return Transaction(control_,maximumEntries,admission);
    }

    explicit ObjectIndexPool(std::uint32_t maximum=MaximumSlots) {
        if (maximum==0u || maximum>MaximumSlots)
            throw std::invalid_argument("UObject index pool capacity is outside portable policy");
        control_=std::make_shared<Control>(maximum);
    }
    ObjectIndexPool(const ObjectIndexPool&) = delete;
    ObjectIndexPool& operator=(const ObjectIndexPool&) = delete;
    ObjectIndexPool(ObjectIndexPool&&) noexcept = default;
    ObjectIndexPool& operator=(ObjectIndexPool&&) noexcept = default;

    [[nodiscard]] Lease Acquire() {
        if (!control_) throw std::logic_error("Cannot acquire from a moved-from UObject index pool");
        auto& control=*control_;
        auto* transaction=control.transaction;
        if (transaction && transaction->count==transaction->maximum)
            throw std::length_error("UObject index transaction journal exhausted");
        if(control.firstFree==0u && control.nextFree.size()>=control.maximum)
            throw std::length_error("UObject index pool capacity exhausted");
        if(transaction)transaction->PrepareNextEntry();
        std::uint32_t index;
        if (control.firstFree!=0u) {
            index=control.firstFree;
        } else {
            if (control.nextFree.size()>=control.maximum)
                throw std::length_error("UObject index pool capacity exhausted");
            // vector<uint32_t>::push_back has the strong exception guarantee.
            // Change no live/free state until growth has succeeded. Shared_ptr
            // copying and lease construction cannot allocate or throw afterward.
            control.nextFree.push_back(0u);
            index=static_cast<std::uint32_t>(control.nextFree.size());
        }
        std::uint32_t journalEntry{};
        if (transaction) {
            const auto ordinal=transaction->count++;
            transaction->EntryAt(ordinal)={nullptr,index,
                control.nextFree[static_cast<std::size_t>(index-1u)]};
            journalEntry=transaction->count;
        }
        if (control.firstFree!=0u)
            control.firstFree=control.nextFree[static_cast<std::size_t>(index-1u)];
        ++control.live;
        return Lease(control_,index,journalEntry);
    }

    [[nodiscard]] std::uint32_t LiveCount() const noexcept { return control_?control_->live:0u; }
    // Created index slots, including currently free slots, not vector capacity.
    [[nodiscard]] std::uint32_t AllocatedSlots() const noexcept {
        return control_?static_cast<std::uint32_t>(control_->nextFree.size()):0u;
    }

private:
    std::shared_ptr<Control> control_;
};

} // namespace QuestVr
