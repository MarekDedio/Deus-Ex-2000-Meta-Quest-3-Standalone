#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace QuestVr {

// An engine client exists independently of a possessed PlayerPawn. The render
// owner revokes this token before tearing down its targets. Only this token is
// shared between threads; registry storage is owned by the runtime CPU worker.
// Revocation is irreversible, so a new render session needs a new token.
class NativeClientLifetime final {
public:
    NativeClientLifetime() = default;
    NativeClientLifetime(const NativeClientLifetime&) = delete;
    NativeClientLifetime& operator=(const NativeClientLifetime&) = delete;
    NativeClientLifetime(NativeClientLifetime&&) = delete;
    NativeClientLifetime& operator=(NativeClientLifetime&&) = delete;

    void Revoke() noexcept { alive_.store(false, std::memory_order_release); }
    bool Alive() const noexcept { return alive_.load(std::memory_order_acquire); }

private:
    std::atomic<bool> alive_{true};
};

// One logical viewport may have two stereo eyes. Dimensions and image counts
// are per eye; the publisher validates that both eyes agree. These are metadata,
// not GL/OpenXR handles or pointers to render resources. The publisher supplies
// nonzero generation identities, never reusing an identity for another target.
struct NativeViewport {
    std::uint64_t identity{};
    std::uint32_t width{}, height{}, swapchainImages{};
    std::uint8_t eyeCount{};
};

// This is a live engine-session registry, not actor state or a save artifact.
// Bind/Register/Unregister/Clear and all storage queries run on the CPU owner.
// Only NativeClientLifetime::Revoke may run concurrently from the render owner.
// Every query checks revocation; a cached span must not be used as proof of a
// live client. Descriptors themselves never permit target dereferencing.
class NativeClientRegistry final {
public:
    static constexpr std::size_t MaximumViewports = 16u;
    static constexpr std::uint32_t MaximumDimension = 16'384u;
    static constexpr std::uint32_t MaximumSwapchainImages = 16u;

    NativeClientRegistry() = default;
    NativeClientRegistry(const NativeClientRegistry&) = delete;
    NativeClientRegistry& operator=(const NativeClientRegistry&) = delete;
    NativeClientRegistry(NativeClientRegistry&&) = delete;
    NativeClientRegistry& operator=(NativeClientRegistry&&) = delete;

    void Bind(std::shared_ptr<NativeClientLifetime> lifetime,
              std::uint64_t clientIdentity) {
        if (!lifetime || !lifetime->Alive() || !clientIdentity)
            Fail("cannot bind an absent, revoked or unidentified client");
        // Even a revoked binding must be explicitly cleared before replacement.
        if (lifetime_ || clientIdentity_ || count_)
            Fail("client replacement requires Clear");
        lifetime_ = std::move(lifetime);
        clientIdentity_ = clientIdentity;
    }

    void Register(std::uint64_t clientIdentity, const NativeViewport& viewport) {
        RequireClient(clientIdentity);
        if (!viewport.identity || !viewport.width || !viewport.height ||
            viewport.width > MaximumDimension || viewport.height > MaximumDimension ||
            !viewport.swapchainImages || viewport.swapchainImages > MaximumSwapchainImages ||
            !viewport.eyeCount || viewport.eyeCount > 2u)
            Fail("invalid viewport descriptor");
        for (std::size_t index = 0u; index < count_; ++index)
            if (viewports_[index].identity == viewport.identity)
                Fail("duplicate viewport identity");
        if (count_ == MaximumViewports) Fail("viewport capacity exhausted");
        viewports_[count_] = viewport;
        ++count_;
    }

    void Unregister(std::uint64_t clientIdentity, std::uint64_t viewportIdentity) {
        RequireClient(clientIdentity);
        if (!viewportIdentity) Fail("unidentified viewport");
        std::size_t index = 0u;
        while (index < count_ && viewports_[index].identity != viewportIdentity) ++index;
        if (index == count_) Fail("unknown or stale viewport identity");
        // Trivially copied metadata makes the validated mutation nonthrowing.
        for (; index + 1u < count_; ++index) viewports_[index] = viewports_[index + 1u];
        --count_;
        viewports_[count_] = {};
    }

    void Clear() noexcept {
        viewports_.fill({});
        count_ = 0u;
        clientIdentity_ = 0u;
        lifetime_.reset();
    }

    bool HasClient() const noexcept {
        return clientIdentity_ && lifetime_ && lifetime_->Alive();
    }
    // Retain the distinction between absence and a revoked binding. Native
    // consumers must not turn revocation into a legitimate empty recipient set.
    bool IsBound() const noexcept { return static_cast<bool>(lifetime_); }
    bool HasViewport() const noexcept { return count_ && HasClient(); }
    std::size_t Count() const noexcept { return HasClient() ? count_ : 0u; }
    std::uint64_t ClientIdentity() const noexcept { return HasClient() ? clientIdentity_ : 0u; }
    std::span<const NativeViewport> ReadOnlyView() const noexcept {
        if (!HasClient()) return {};
        return {viewports_.data(), count_};
    }

private:
    [[noreturn]] static void Fail(const char* reason) {
        throw std::runtime_error(std::string("Native client registry: ") + reason);
    }
    void RequireClient(std::uint64_t identity) const {
        if (!identity || identity != clientIdentity_ || !HasClient())
            Fail("absent, revoked or stale client identity");
    }

    std::shared_ptr<NativeClientLifetime> lifetime_;
    std::uint64_t clientIdentity_{};
    std::array<NativeViewport, MaximumViewports> viewports_{};
    std::size_t count_{};
};

} // namespace QuestVr
