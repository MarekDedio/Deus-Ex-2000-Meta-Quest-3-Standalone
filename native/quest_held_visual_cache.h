#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace QuestVr {

// Scheduling/fingerprints only; no actor pointers, selection decisions, runtime
// queries, clocks or I/O. Normal operations retain no heap-allocated storage.
class HeldVisualCache {
public:
    static constexpr float RefreshSeconds = 0.10f;
    static constexpr std::size_t MaximumIdentityBytes = 4096u;
    static constexpr std::size_t MaximumMapBytes = 4096u;

    void Reset() noexcept {
        populated_ = false;
        invalidated_ = true;
        identityBytes_ = mapBytes_ = 0u;
        revision_ = 0u;
        remainingSeconds_ = 0.0;
    }

    // Use on save restoration or whenever another thread owns the runtime.
    // Keep the last revision available for diagnostics, but never reuse it as
    // proof that the corresponding renderer selection is still current.
    void Invalidate() noexcept { invalidated_ = true; }

    void Tick(float deltaSeconds) {
        if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f)
            throw std::invalid_argument("Held visual cache elapsed time must be finite and nonnegative");
        const auto elapsed = static_cast<double>(deltaSeconds);
        remainingSeconds_ = elapsed >= remainingSeconds_ ? 0.0 : remainingSeconds_ - elapsed;
    }

    [[nodiscard]] bool NeedsRefresh(std::string_view identity, std::string_view map,
        std::uint64_t worldRevision) const {
        Validate(identity, MaximumIdentityBytes);
        Validate(map, MaximumMapBytes);
        // Door animation can advance this every frame. Conversely, a material
        // overlay need not advance it. Neither case changes the 10Hz cadence;
        // only identity/map changes and explicit invalidation bypass the timer.
        (void)worldRevision;
        return !populated_ || invalidated_ || !Same(identity, identity_, identityBytes_) ||
            !Same(map, map_, mapBytes_) || remainingSeconds_ <= 0.0;
    }

    // Record every attempted metadata resolution, including an explicit
    // unsupported/failure outcome, to avoid retrying that outcome every frame.
    // Validate both keys before changing any cache state.
    void Refreshed(std::string_view identity, std::string_view map, std::uint64_t worldRevision) {
        Validate(identity, MaximumIdentityBytes);
        Validate(map, MaximumMapBytes);
        std::copy(identity.begin(), identity.end(), identity_.begin());
        std::copy(map.begin(), map.end(), map_.begin());
        identityBytes_ = identity.size();
        mapBytes_ = map.size();
        revision_ = worldRevision;
        remainingSeconds_ = RefreshSeconds;
        populated_ = true;
        invalidated_ = false;
    }

    [[nodiscard]] std::optional<std::uint64_t> LastRevision() const noexcept {
        return populated_ ? std::optional<std::uint64_t>{revision_} : std::nullopt;
    }

private:
    static void Validate(std::string_view text, std::size_t maximum) {
        if (text.size() > maximum || text.find('\0') != std::string_view::npos)
            throw std::invalid_argument("Held visual cache key exceeds its bounded text representation");
    }

    template<std::size_t Count>
    static bool Same(std::string_view text, const std::array<char, Count>& stored,
        std::size_t bytes) noexcept {
        if (text.size() != bytes) return false;
        const auto fold = [](unsigned char value) {
            return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value - 'A' + 'a') : value;
        };
        for (std::size_t index = 0u; index < bytes; ++index)
            if (fold(static_cast<unsigned char>(text[index])) !=
                fold(static_cast<unsigned char>(stored[index]))) return false;
        return true;
    }

    std::array<char, MaximumIdentityBytes> identity_{};
    std::array<char, MaximumMapBytes> map_{};
    std::size_t identityBytes_{}, mapBytes_{};
    std::uint64_t revision_{};
    double remainingSeconds_{};
    bool populated_{}, invalidated_{true};
};

} // namespace QuestVr
