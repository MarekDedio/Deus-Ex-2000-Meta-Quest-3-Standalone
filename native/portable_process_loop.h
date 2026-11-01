#pragma once
#include "quest_process_loop.h"
#include "quest_portable_vm.h"
#include <functional>
#include <memory>

struct PortableProcessLoopSnapshot {
    std::uint64_t identity{},lookupGeneration{};
    QuestVr::ProcessLoopControl::State state{QuestVr::ProcessLoopControl::State::Ready};
    std::size_t retainedBytes{};
    bool running{};
};
// Exclusive CPU-owner bind after the real lookup generation exists. Same
// pointer is idempotent; replacement requires runtime shutdown/reinitialization.
void BindPortableRuntimeProcessLoop(std::shared_ptr<QuestVr::ProcessLoopControl> control);
PortableProcessLoopSnapshot ReadPortableRuntimeProcessLoop();
struct PortableProcessLoopProbeResult {
    QuestVr::Vm::Result execution;
    bool first{},second{},completed{};
    std::uint64_t identity{},lookupGeneration{};
};
// Explicit transaction diagnostic; the callback is an external test producer,
// not native/input dispatch authority. A failed root cannot rewind that source.
PortableProcessLoopProbeResult ProbePortableRuntimeProcessLoop(
    const std::function<void()>& afterFirst={},bool lateFailure=false,
    const QuestVr::Vm::Limits& limits={},const std::function<void()>& beforeCommit={});
