#pragma once

#include <cstddef>
#include <cstdint>
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
    struct Control {
        explicit Control(std::uint32_t maximum) : maximum(maximum) {}
        std::vector<std::uint32_t> nextFree;
        std::uint32_t maximum;
        std::uint32_t firstFree{};
        std::uint32_t live{};
    };

public:
    static constexpr std::uint32_t MaximumSlots = 4'000'000u;

    // Only Acquire creates a lease, and only its private release path can
    // return an index. Move transfers that authority; no public numeric release
    // API can accidentally free a stale, duplicate or foreign object's index.
    class Lease {
    public:
        Lease() noexcept = default;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&& other) noexcept
            : control_(std::move(other.control_)), index_(std::exchange(other.index_,0u)) {}
        Lease& operator=(Lease&& other) noexcept {
            if (this!=&other) {
                Release();
                control_=std::move(other.control_);
                index_=std::exchange(other.index_,0u);
            }
            return *this;
        }
        ~Lease() { Release(); }

        [[nodiscard]] std::uint32_t Index() const noexcept { return index_; }
        explicit operator bool() const noexcept { return index_!=0u; }

    private:
        friend class ObjectIndexPool;
        Lease(std::shared_ptr<Control> control,std::uint32_t index) noexcept
            : control_(std::move(control)), index_(index) {}
        void Release() noexcept {
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
    };

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
        std::uint32_t index;
        if (control.firstFree!=0u) {
            index=control.firstFree;
            control.firstFree=control.nextFree[static_cast<std::size_t>(index-1u)];
        } else {
            if (control.nextFree.size()>=control.maximum)
                throw std::length_error("UObject index pool capacity exhausted");
            // vector<uint32_t>::push_back has the strong exception guarantee.
            // Change no live/free state until growth has succeeded. Shared_ptr
            // copying and lease construction cannot allocate or throw afterward.
            control.nextFree.push_back(0u);
            index=static_cast<std::uint32_t>(control.nextFree.size());
        }
        ++control.live;
        return Lease(control_,index);
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
