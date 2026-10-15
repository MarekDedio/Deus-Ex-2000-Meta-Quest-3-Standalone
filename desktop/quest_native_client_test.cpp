#include "quest_native_client.h"

#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace {
using Registry = QuestVr::NativeClientRegistry;
using Lifetime = QuestVr::NativeClientLifetime;
using Viewport = QuestVr::NativeViewport;
std::size_t checks{}, rejections{};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
bool Same(const Viewport& a, const Viewport& b) {
    return a.identity == b.identity && a.width == b.width && a.height == b.height &&
        a.swapchainImages == b.swapchainImages && a.eyeCount == b.eyeCount;
}
struct Snapshot {
    bool hasClient{}, hasViewport{}, bound{};
    std::uint64_t clientIdentity{};
    std::size_t count{};
    std::vector<Viewport> viewports;
};
Snapshot Read(const Registry& registry) {
    const auto view = registry.ReadOnlyView();
    return {registry.HasClient(), registry.HasViewport(), registry.IsBound(), registry.ClientIdentity(),
        registry.Count(), {view.begin(), view.end()}};
}
bool Same(const Snapshot& a, const Snapshot& b) {
    if (a.hasClient != b.hasClient || a.hasViewport != b.hasViewport || a.bound != b.bound ||
        a.clientIdentity != b.clientIdentity || a.count != b.count ||
        a.viewports.size() != b.viewports.size()) return false;
    for (std::size_t index = 0u; index < a.viewports.size(); ++index)
        if (!Same(a.viewports[index], b.viewports[index])) return false;
    return true;
}
void Reject(Registry& registry, const std::function<void()>& operation,
            const std::string& message) {
    const auto before = Read(registry);
    bool rejected{};
    try { operation(); }
    catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).rfind("Native client registry: ", 0u) == 0u;
    }
    Require(rejected, message);
    Require(Same(before, Read(registry)), message + ": failed mutation changed state");
    ++rejections;
}
Viewport View(std::uint64_t identity) { return {identity, 2064u, 2208u, 3u, 2u}; }

void EmptyAndIdentity() {
    Registry registry;
    const auto absent = Read(registry);
    Require(!absent.hasClient && !absent.hasViewport && !absent.bound && !absent.clientIdentity &&
        !absent.count && absent.viewports.empty(), "Fresh registry is not absent");
    registry.Clear();
    Require(Same(absent, Read(registry)), "Clearing absent registry changed it");
    Reject(registry, [&] { registry.Bind({}, 1u); }, "Null lifetime accepted");
    const auto live = std::make_shared<Lifetime>();
    Reject(registry, [&] { registry.Bind(live, 0u); }, "Zero client identity accepted");
    const auto dead = std::make_shared<Lifetime>();
    dead->Revoke();
    Reject(registry, [&] { registry.Bind(dead, 1u); }, "Revoked token accepted");
    Reject(registry, [&] { registry.Register(1u, View(1u)); }, "Absent client registered target");
    Reject(registry, [&] { registry.Unregister(1u, 1u); }, "Absent client removed target");
    registry.Bind(live, 11u);
    Require(registry.HasClient() && registry.IsBound() && registry.ClientIdentity() == 11u &&
        !registry.HasViewport() && registry.Count() == 0u && registry.ReadOnlyView().empty(),
        "Bound empty client was conflated with absence");
    Reject(registry, [&] { registry.Bind(live, 11u); }, "Duplicate client binding accepted");
    Reject(registry, [&] { registry.Bind(std::make_shared<Lifetime>(), 12u); },
        "Live client replaced without clear");
    for (const auto stale : {0u, 10u, 12u}) {
        Reject(registry, [&] { registry.Register(stale, View(1u)); }, "Wrong client registered target");
        Reject(registry, [&] { registry.Unregister(stale, 1u); }, "Wrong client removed target");
    }
    Reject(registry, [&] { registry.Unregister(11u, 1u); }, "Missing viewport removed");
    registry.Clear();
    Require(Same(absent, Read(registry)) && live->Alive(), "Clear revoked publisher token or retained data");
}

void DescriptorsAndOrder() {
    Registry registry;
    registry.Bind(std::make_shared<Lifetime>(), 71u);
    const auto rejectDescriptor = [&](const Viewport& viewport) {
        Reject(registry, [&] { registry.Register(71u, viewport); }, "Invalid viewport accepted");
    };
    auto invalid = View(1u); invalid.identity = 0u; rejectDescriptor(invalid);
    for (const auto dimension : {0u, 16'385u, std::numeric_limits<std::uint32_t>::max()}) {
        invalid = View(1u); invalid.width = dimension; rejectDescriptor(invalid);
        invalid = View(1u); invalid.height = dimension; rejectDescriptor(invalid);
    }
    for (const auto images : {0u, 17u, std::numeric_limits<std::uint32_t>::max()}) {
        invalid = View(1u); invalid.swapchainImages = images; rejectDescriptor(invalid);
    }
    for (const auto eyes : {0u, 3u, 255u}) {
        invalid = View(1u); invalid.eyeCount = static_cast<std::uint8_t>(eyes); rejectDescriptor(invalid);
    }
    const Viewport minimum{90u, 1u, 1u, 1u, 1u};
    const Viewport maximum{30u, 16'384u, 16'384u, 16u, 2u};
    registry.Register(71u, minimum);
    registry.Register(71u, maximum);
    registry.Register(71u, View(5u));
    Require(registry.HasViewport() && registry.Count() == 3u &&
        Same(registry.ReadOnlyView()[0], minimum) && Same(registry.ReadOnlyView()[1], maximum) &&
        Same(registry.ReadOnlyView()[2], View(5u)), "Insertion order or exact descriptor changed");
    Reject(registry, [&] { registry.Register(71u, View(30u)); }, "Duplicate identity with new geometry accepted");
    Reject(registry, [&] { registry.Unregister(71u, 0u); }, "Zero viewport identity removed");
    Reject(registry, [&] { registry.Unregister(71u, 4u); }, "Unknown viewport identity removed");
    registry.Unregister(71u, 30u);
    Require(registry.Count() == 2u && Same(registry.ReadOnlyView()[0], minimum) &&
        Same(registry.ReadOnlyView()[1], View(5u)), "Middle removal reordered survivors");
    Reject(registry, [&] { registry.Unregister(71u, 30u); }, "Stale removed viewport accepted");
    registry.Register(71u, View(99u));
    Require(registry.ReadOnlyView()[2].identity == 99u, "New viewport did not append after removal");
    registry.Unregister(71u, 90u);
    Require(registry.Count() == 2u && registry.ReadOnlyView()[0].identity == 5u &&
        registry.ReadOnlyView()[1].identity == 99u, "First removal reordered survivors");
    registry.Unregister(71u, 99u);
    registry.Unregister(71u, 5u);
    Require(registry.HasClient() && !registry.HasViewport() && registry.Count() == 0u,
        "Removing last target destroyed empty client identity");
}

void CapacityAndReset() {
    Registry registry;
    const auto lifetime = std::make_shared<Lifetime>();
    registry.Bind(lifetime, 101u);
    for (std::size_t index = 0u; index < Registry::MaximumViewports; ++index)
        registry.Register(101u, View(1000u + index));
    Require(registry.Count() == Registry::MaximumViewports, "Exact capacity was rejected");
    for (std::size_t index = 0u; index < Registry::MaximumViewports; ++index)
        Require(registry.ReadOnlyView()[index].identity == 1000u + index, "Full registry order changed");
    Reject(registry, [&] { registry.Register(101u, View(9999u)); }, "Capacity overflow accepted");
    Reject(registry, [&] { registry.Register(101u, View(1000u)); }, "Full registry duplicate accepted");
    registry.Unregister(101u, 1007u);
    registry.Register(101u, View(9999u));
    Require(registry.Count() == Registry::MaximumViewports &&
        registry.ReadOnlyView().back().identity == 9999u, "Capacity slot not reusable after removal");
    registry.Clear();
    Require(!registry.HasClient() && !registry.HasViewport() && registry.ReadOnlyView().empty(),
        "Clear retained full client registry");
    registry.Bind(std::make_shared<Lifetime>(), 102u);
    registry.Register(102u, View(2000u));
    Reject(registry, [&] { registry.Register(101u, View(2001u)); }, "Old session registered in new client");
    Reject(registry, [&] { registry.Unregister(101u, 2000u); }, "Old session removed new client target");
    Require(registry.Count() == 1u && registry.ReadOnlyView()[0].identity == 2000u,
        "Rebinding retained old targets");
    registry.Clear();
    registry.Clear();
    Require(!registry.HasClient() && lifetime->Alive(), "Repeated clear is not idempotent");
}

void Revocation() {
    Registry registry;
    const auto lifetime = std::make_shared<Lifetime>();
    registry.Bind(lifetime, 501u);
    registry.Register(501u, View(801u));
    std::thread renderer([lifetime] { lifetime->Revoke(); lifetime->Revoke(); });
    renderer.join();
    Require(!lifetime->Alive(), "Renderer revocation was not visible to CPU owner");
    Require(!registry.HasClient() && registry.IsBound() && !registry.HasViewport() && registry.Count() == 0u &&
        registry.ClientIdentity() == 0u && registry.ReadOnlyView().empty(),
        "Revoked client or viewport remained observable");
    Reject(registry, [&] { registry.Register(501u, View(802u)); }, "Revoked session registered target");
    Reject(registry, [&] { registry.Unregister(501u, 801u); }, "Revoked session removed target");
    Reject(registry, [&] { registry.Bind(std::make_shared<Lifetime>(), 502u); },
        "Revoked binding replaced without explicit clear");
    registry.Clear();
    Reject(registry, [&] { registry.Bind(lifetime, 503u); }, "Clear resurrected revoked token");
    const auto replacement = std::make_shared<Lifetime>();
    registry.Bind(replacement, 504u);
    registry.Register(504u, View(804u));
    lifetime->Revoke();
    Require(registry.HasClient() && registry.HasViewport() && registry.ClientIdentity() == 504u,
        "Old publisher token revoked replacement session");
    replacement->Revoke();
    Require(registry.ReadOnlyView().empty(), "Replacement revocation did not hide targets");

    registry.Clear();
    auto owned = std::make_shared<Lifetime>();
    const std::weak_ptr<Lifetime> weak = owned;
    registry.Bind(owned, 505u);
    owned.reset();
    Require(!weak.expired() && registry.HasClient(), "Registry did not retain lifetime lease");
    weak.lock()->Revoke();
    Require(!registry.HasClient() && !weak.expired(), "Revoked lease vanished before owner reset");
    registry.Clear();
    Require(weak.expired(), "Clear retained lifetime lease");

    const auto concurrent = std::make_shared<Lifetime>();
    registry.Bind(concurrent, 506u);
    registry.Register(506u, View(806u));
    std::atomic<unsigned> phase{0u};
    std::thread revoker([concurrent, &phase] {
        while (phase.load(std::memory_order_acquire) == 0u) std::this_thread::yield();
        concurrent->Revoke();
        phase.store(2u, std::memory_order_release);
    });
    phase.store(1u, std::memory_order_release);
    bool sawRevocation{}, resurrected{};
    do {
        const bool visible = registry.HasViewport();
        if (sawRevocation && visible) resurrected = true;
        if (!visible) sawRevocation = true;
        // No mutation or cached storage read crosses threads: only the lifetime
        // token is shared. Visibility may change between separate queries.
        (void)registry.Count();
        (void)registry.ReadOnlyView();
        std::this_thread::yield();
    } while (phase.load(std::memory_order_acquire) != 2u);
    revoker.join();
    Require(!resurrected && !registry.HasViewport() && registry.ReadOnlyView().empty(),
        "Concurrent revocation resurrected or exposed targets after completion");
}
} // namespace

int main() {
    try {
        static_assert(std::is_trivially_copyable_v<Viewport>);
        static_assert(!std::is_copy_constructible_v<Lifetime> && !std::is_move_constructible_v<Lifetime>);
        static_assert(!std::is_copy_constructible_v<Registry> && !std::is_move_constructible_v<Registry>);
        EmptyAndIdentity();
        DescriptorsAndOrder();
        CapacityAndReset();
        Revocation();
        std::cout << "Native client registry: " << checks << " checks, " << rejections
                  << " rejected mutations passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Native client registry test failed: " << error.what() << '\n';
        return 1;
    }
}
