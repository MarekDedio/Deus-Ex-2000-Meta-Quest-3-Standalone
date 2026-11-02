#pragma once
#include "quest_portable_vm.h"
#include <array>
#include <string>

enum class PortableRootInputProbeMode {RoundTrip,LateFailure,Navigation,MouseGrab,InvalidKey,FocusChild,DestroyedRoot};
struct PortableRootInputProbeResult {
    QuestVr::Vm::Result execution;
    std::string pawnPath,rootPath;
    std::size_t writesBeforeInput{},events{},rawCallbacks{},virtualCallbacks{};
    std::string lastRawFunction,lastVirtualFunction;
    bool completed{},declinedWithoutGrab{},consumedWithGrab{},duplicateReleaseSkipped{},physicalDelivery{},mouseFireCleared{};
    std::array<std::uint8_t,4u> keyStates{};
};
// Same-root original player/FlagBase/configured Root construction followed by
// actual Root.Process/keyboard events. Rollback-only, no physical event source
// or completed Input.Init is fabricated by this explicit primitive diagnostic.
PortableRootInputProbeResult ProbePortableRuntimeRootInput(const std::string& levelInfoPath,
    PortableRootInputProbeMode mode,const QuestVr::Vm::Limits& limits={});
