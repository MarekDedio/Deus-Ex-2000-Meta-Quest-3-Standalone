#include "async_result_epoch.h"

#include <chrono>
#include <future>
#include <iostream>
#include <limits>
#include <utility>

int main() {
    int failures{};
    const auto check = [&](const bool passed, const char* message) {
        if (!passed) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    QuestVr::AsyncResultEpoch epoch;
    const auto first = epoch.Capture();
    check(epoch.IsCurrent(first), "newly dispatched result is current");
    epoch.Invalidate(first, true);
    check(!epoch.IsCurrent(first), "restoring state rejects prior dialogue result");
    const auto second = epoch.Capture();
    check(epoch.IsCurrent(second), "new state accepts its own dispatch");
    epoch.Invalidate(first, true);
    check(!epoch.IsCurrent(first) && !epoch.IsCurrent(second),
          "repeated map/save invalidation keeps old results stale");

    // The worker is deliberately unfinished while the exact production token
    // logic invalidates it. No timeout/sleep is needed to let it finish.
    std::promise<void> release;
    auto gate = release.get_future();
    auto worker = std::async(std::launch::async, [gate = std::move(gate)]() mutable {
        gate.wait();
        return 7;
    });
    const auto workerToken = epoch.Capture();
    epoch.Invalidate(workerToken, worker.valid());
    check(worker.valid() && worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready,
          "invalidation preserves the unfinished worker handle without waiting");
    check(!epoch.IsCurrent(workerToken), "unfinished result becomes stale immediately");
    release.set_value();
    check(worker.get() == 7 && !epoch.IsCurrent(workerToken),
          "worker result can be consumed without applying abandoned speech");
    const auto replacement = epoch.Capture();
    check(epoch.IsCurrent(replacement), "replacement speech dispatch remains usable");

    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    QuestVr::AsyncResultEpoch wrap(maximum);
    wrap.Invalidate(0u, true);
    check(wrap.Capture() == 1u && !wrap.IsCurrent(0u),
          "wrap skips an outstanding zero-valued token");
    QuestVr::AsyncResultEpoch nextToWrap(maximum - 1u);
    nextToWrap.Invalidate(maximum, true);
    check(nextToWrap.Capture() == 0u && !nextToWrap.IsCurrent(maximum),
          "advance skips a still-live maximum-valued token");
    QuestVr::AsyncResultEpoch idleWrap(maximum);
    idleWrap.Invalidate(0u, false);
    check(idleWrap.Capture() == 0u && idleWrap.IsCurrent(idleWrap.Capture()),
          "idle wrap permits a fresh token");
    if (failures != 0) return 1;
    std::cout << "Async result epoch checks passed: abandoned speech, unfinished worker, replacement and wrap guards\n";
    return 0;
}
