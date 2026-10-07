#pragma once

#include <cstdint>

namespace QuestVr {

// Main-thread-only validity token for one outstanding asynchronous result.
// Invalidating a token does not wait for, cancel, or destroy the worker. A
// completed result must still be consumed and discarded when no longer current.
class AsyncResultEpoch {
public:
    explicit constexpr AsyncResultEpoch(const std::uint64_t initial = 1u) noexcept
        : current_(initial) {}

    constexpr std::uint64_t Capture() const noexcept { return current_; }
    constexpr bool IsCurrent(const std::uint64_t token) const noexcept {
        return token == current_;
    }

    constexpr void Invalidate(const std::uint64_t pendingToken,
                              const bool hasPendingResult) noexcept {
        ++current_;
        // Unsigned wrap is defined. Never accidentally revalidate the single
        // live worker, including when its token predates many invalidations.
        if (hasPendingResult && current_ == pendingToken) ++current_;
    }

private:
    std::uint64_t current_;
};

} // namespace QuestVr
