#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace QuestVr {

// Original LoadMap1038DCD5..1038DF46, NOT ULevel.CompactActors. The actual
// runtime joins its caller's Level/VM root and native-work/scratch ledgers.
// Providers own native identity/schema, GUndo/native-array admission and exact
// rollback. A detached mock is not native storage, startup or save support.
struct OriginalWorldActorOrderQueries {
    std::function<std::int32_t(std::uintptr_t)> actorCount;
    std::function<std::uintptr_t(std::uintptr_t,std::int32_t)> actorSlot;
    std::function<bool(std::uintptr_t)> isStatic;
    std::function<void(std::uintptr_t,std::int32_t)> writeFirstDynamic;
    // The span borrows the temporary buffer only for this provider call.
    // Replacement must stage/admit its own native storage and join the same
    // root. It does not transfer this buffer or retire any UObject identity.
    std::function<void(std::uintptr_t,std::span<const std::uintptr_t>)> replaceSlots;
    // Validator measures CURRENT borrowed scratch ledger plus this extra.
    // It must not mutate the actor array or leave its own scratch unbalanced.
    // Allocation policy here is portable, not recovered original FArray slack.
    std::function<void(std::size_t)> admitTemporaryBytes;
    std::function<void()> work; // REQUIRED borrowed root, never reset here.
};

namespace WorldActorOrderDetail {
inline constexpr std::size_t MaximumSlots=1'000'000u;
template<class Function,class... Arguments>
inline auto Call(const OriginalWorldActorOrderQueries& query,const Function& function,Arguments&&... arguments) {
    if(!function)throw std::invalid_argument("Original world Actor ordering lacks a reached provider");
    if(!query.work)throw std::invalid_argument("Original world Actor ordering requires borrowed root work");
    query.work();return function(std::forward<Arguments>(arguments)...);
}
inline std::int32_t Count(std::uintptr_t level,const OriginalWorldActorOrderQueries& query) {
    const auto count=Call(query,query.actorCount,level);
    if(count<0||static_cast<std::size_t>(count)>MaximumSlots)
        throw std::runtime_error("Original world Actor array exceeds its bounded signed count");
    return count;
}
class Temporary final {
    std::size_t& scratch_;
    const std::size_t baseline_;
    const OriginalWorldActorOrderQueries& query_;
    std::vector<std::uintptr_t> values_;
    std::size_t Bytes(std::size_t capacity) const {
        if(capacity>std::numeric_limits<std::size_t>::max()/sizeof(std::uintptr_t))
            throw std::runtime_error("Original world Actor temporary capacity overflows");
        return capacity*sizeof(std::uintptr_t);
    }
    void Ledger(std::size_t capacity) {
        const auto bytes=Bytes(capacity);
        if(bytes>std::numeric_limits<std::size_t>::max()-baseline_-sizeof(values_))
            throw std::runtime_error("Original world Actor borrowed scratch ledger overflows");
        scratch_=baseline_+sizeof(values_)+bytes;
    }
public:
    Temporary(std::size_t& scratch,const OriginalWorldActorOrderQueries& query)
        :scratch_(scratch),baseline_(scratch),query_(query) {
        if(sizeof(values_)>std::numeric_limits<std::size_t>::max()-baseline_)
            throw std::runtime_error("Original world Actor temporary header overflows");
        Call(query_,query_.admitTemporaryBytes,sizeof(values_));Ledger(0u);
    }
    ~Temporary() noexcept {scratch_=baseline_;}
    Temporary(const Temporary&)=delete;
    Temporary& operator=(const Temporary&)=delete;
    void PrepareAppend() {
        if(values_.size()>=MaximumSlots)
            throw std::runtime_error("Original world Actor output exceeds its bounded slot count");
        if(values_.size()<values_.capacity())return;
        const auto next=std::min(MaximumSlots,std::max<std::size_t>(8u,values_.capacity()*2u));
        // Old capacity remains charged while reserve can own old+new buffers.
        // Confirm actual capacity (including implementation excess) against
        // that SAME old ledger before publishing its replacement charge.
        Call(query_,query_.admitTemporaryBytes,Bytes(next));
        values_.reserve(next);
        Call(query_,query_.admitTemporaryBytes,Bytes(values_.capacity()));
        Ledger(values_.capacity());
    }
    void AppendPrepared(std::uintptr_t actor) {values_.push_back(actor);}
    std::size_t Size()const noexcept{return values_.size();}
    std::span<const std::uintptr_t> View()const noexcept{return values_;}
};
} // namespace WorldActorOrderDetail

// This coordinator owns NO gameplay transaction or separate work allowance.
// A failure after writeFirstDynamic deliberately leaves that caller mutation
// visible for the OUTER root to roll back. Slot0/1 are retained even if None;
// subsequent holes disappear, and pending-delete actors are not filtered.
inline void ReorderOriginalWorldActors(std::uintptr_t level,const OriginalWorldActorOrderQueries& query,
    std::size_t& borrowedScratchLedger) {
    using WorldActorOrderDetail::Call;
    using WorldActorOrderDetail::Count;
    if(!level)throw std::invalid_argument("Original world Actor ordering requires an actual Level");
    // Original code dereferences both reserved slots without a count guard.
    // The portable safety gate refuses an invalid native array before access.
    if(Count(level,query)<2)throw std::runtime_error("Original world Actor array lacks its two reserved slots");
    WorldActorOrderDetail::Temporary output(borrowedScratchLedger,query);
    for(std::int32_t slot=0;slot<2;++slot) {
        output.PrepareAppend();output.AppendPrepared(Call(query,query.actorSlot,level,slot));
    }
    for(std::int32_t slot=2;slot<Count(level,query);++slot) {
        const auto actor=Call(query,query.actorSlot,level,slot);
        if(!actor||!Call(query,query.isStatic,actor))continue;
        output.PrepareAppend();
        // The original retains the source slot, then rereads its pointer
        // after possible temporary-array growth (1038DD9A/1038DE02). Native
        // admission providers must not replace source array storage here.
        output.AppendPrepared(Call(query,query.actorSlot,level,slot));
    }
    Call(query,query.writeFirstDynamic,level,static_cast<std::int32_t>(output.Size()));
    for(std::int32_t slot=2;slot<Count(level,query);++slot) {
        const auto actor=Call(query,query.actorSlot,level,slot);
        if(!actor||Call(query,query.isStatic,actor))continue;
        output.PrepareAppend();output.AppendPrepared(Call(query,query.actorSlot,level,slot));
    }
    Call(query,query.replaceSlots,level,output.View());
}

} // namespace QuestVr
