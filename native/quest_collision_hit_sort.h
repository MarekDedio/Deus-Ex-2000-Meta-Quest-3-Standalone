#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace QuestVr {

// Detached ordering component of retail ULevel::MultiLineCheck, NOT a collision
// collector or Move implementation. Engine 1039b70f passes 44-byte FCheckResult
// packets through Core.appQsort to the bundled MSVCRT.dll qsort at 7802543d.
// Its comparator (1039c060) uses Float Time alone: equal Times have NO secondary
// actor/world/pointer key, and this particular qsort is deliberately unstable.
// Pin: bundled MSVCRT SHA256
// e390b0529fb9ee1214bb68302a4658b578134d92ab76bc5eb48f3badd843d210.
// A different system/KnownDLL CRT is not claimed to have this permutation.
//
// Finite, bounded policy: accept at most 64 packets (the retail collector cap).
// Count 0/1 does not read Time, exactly as qsort. For >=2, invoke the pure Float
// getter once per packet and validate ALL Times before mutation. That is an
// explicit safety admission, not retail's lazy comparator-access policy. The
// complete packet, and its cached Time, always move together. Getter must return
// the immutable Float Time, not depend on its packet's current vector position.
template<class T,class Time>
void SortOriginalCollisionHits(std::vector<T>& hits,Time time) {
    using Result=std::remove_cvref_t<std::invoke_result_t<Time&,const T&>>;
    static_assert(std::is_same_v<Result,float>,"Collision-hit Time getter must return Float");
    static_assert(std::is_nothrow_swappable_v<T>,"Collision-hit packets must swap without throwing");
    constexpr std::size_t maximum=64u;
    if (hits.size()>maximum)
        throw std::invalid_argument("Original collision-hit sort exceeds its 64-packet collector cap");
    if (hits.size()<2u) return;
    std::array<float,maximum> times{};
    for (std::size_t i=0;i<hits.size();++i) {
        times[i]=std::invoke(time,std::as_const(hits[i]));
        if (!std::isfinite(times[i]))
            throw std::invalid_argument("Original collision-hit sort requires finite active Float Times");
    }
    const auto swapPackets=[&](std::ptrdiff_t a,std::ptrdiff_t b) noexcept {
        if (a==b) return; // Retail's byte-swap also returns immediately here.
        using std::swap;
        swap(hits[static_cast<std::size_t>(a)],hits[static_cast<std::size_t>(b)]);
        swap(times[static_cast<std::size_t>(a)],times[static_cast<std::size_t>(b)]);
    };
    struct Range {std::ptrdiff_t low{},high{};};
    // Original has 32 pending slots; <=64 input cannot exhaust them because
    // it pushes the larger child and processes the smaller child first.
    std::array<Range,32u> pending{};
    std::size_t pendingCount{};
    std::ptrdiff_t low=0,high=static_cast<std::ptrdiff_t>(hits.size())-1;
    for (;;) {
        const auto count=high-low+1;
        if (count<=8) {
            // 780255e2: first STRICT maximum is swapped to the current end.
            // Replacing this with insertion/stable/std::sort changes ties.
            for (auto end=high;end>low;--end) {
                auto maximumAt=low;
                for (auto candidate=low+1;candidate<=end;++candidate)
                    if (times[candidate]>times[maximumAt]) maximumAt=candidate;
                swapPackets(maximumAt,end);
            }
        } else {
            swapPackets(low+(count>>1),low);
            auto left=low,right=high+1;
            for (;;) {
                do {++left;} while (left<=high && times[left]<=times[low]);
                do {--right;} while (right>low && times[right]>=times[low]);
                if (right<left) break;
                swapPackets(left,right);
            }
            swapPackets(right,low);
            const bool haveLeft=low+1<right,haveRight=left<high;
            // 7802555a..78025564 compares signed BYTE distances:
            //   (rightByte-lowByte-1) >= (highByte-leftByte).
            // For 44-byte packets this is STRICT '>' at index granularity.
            if (right-low>high-left) {
                if (haveLeft) pending[pendingCount++]={low,right-1};
                if (haveRight) {low=left;continue;}
            } else {
                if (haveRight) pending[pendingCount++]={left,high};
                if (haveLeft) {high=right-1;continue;}
            }
        }
        if (!pendingCount) return;
        const auto next=pending[--pendingCount];low=next.low;high=next.high;
    }
}

template<class T>
void SortOriginalCollisionHits(std::vector<T>& hits) {
    SortOriginalCollisionHits(hits,[](const T& hit)->float {return hit.time;});
}

} // namespace QuestVr
