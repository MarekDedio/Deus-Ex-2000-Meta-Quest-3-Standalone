#pragma once
#include "portable_unreal_runtime.h"

// Non-publishing diagnostic of original InitRootWindow INCLUDING effective
// visibility, init-count and layout. Captures actual state at success/failure,
// then rolls the entire transaction back. It does not invoke WindowReady:
// that is a separate, later Root.PreRender operation.
PortableRootWindowProbeResult ProbePortableRootWindowActivation(
    const std::string& playerActorPath,const QuestVr::Vm::Limits& limits = {});
