#pragma once

#include "quest_portable_vm.h"
#include <array>
#include <string>

enum class PortableWindowNavigationProbeMode {
    RoundTrip,LateFailure,WrongReceiver,WrongArgument,MissingClientArgument,CyclicHiddenChain
};
struct PortableWindowNavigationProbeResult {
    QuestVr::Vm::Result execution;
    // Detached observations from an isolated original base Window fixture;
    // never a configured HUD, published UI tree or replacement startup.
    std::array<std::string,4u> windows;
    std::array<std::string,20u> selected;
    std::array<std::string,3u> clients;
    std::array<std::string,2u> liveWalk;
    std::size_t queryCount{},walkCount{};
    bool clientRooted{},clientCleared{},topologyPreserved{},completed{};
};
PortableWindowNavigationProbeResult ProbePortableNativeWindowNavigation(
    const std::string& actualOuterPath,PortableWindowNavigationProbeMode mode,
    const QuestVr::Vm::Limits& limits = {});
