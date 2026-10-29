#pragma once
#include "portable_core_static_exec.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
struct PortableFlagBaseSnapshot {
    std::string path,classPath,outerPath;
    std::uint32_t objectIndex{},objectFlags{};
    std::int32_t defaultExpiration{};
    std::size_t occupiedBuckets{};
    bool constructorCompleted{},defaultCopyVerified{};
    bool operator==(const PortableFlagBaseSnapshot&) const = default;
};
struct PortableFlagBaseClassSnapshot {bool prepared{};std::size_t nativeBytes{},buckets{};};
PortableFlagBaseClassSnapshot PreparePortableRuntimeFlagBaseClasses();
std::vector<PortableFlagBaseSnapshot> ReadPortableRuntimeFlagBases();
struct PortableFlagBaseProbeResult {
    QuestVr::Vm::Result execution;
    PortableFlagBaseSnapshot object;
    std::vector<bool> matches;
    bool completed{};
    // Explicit caller-owned diagnostic GC lease in Commit mode. A standalone
    // object has no Pawn edge; releasing this lease permits normal collection.
    // Production construction uses the actual pawn edge, never this token.
    std::shared_ptr<void> diagnosticLifetime;
};
// Explicit actual FlagBase allocation+constructor/query diagnostic in one
// caller root, not a replacement pawn or successful gameplay startup. Outer
// must be an indexed actual UObject. No fake flags are inserted by this probe.
PortableFlagBaseProbeResult ExecutePortableRuntimeFlagBaseProbe(const std::string& outerPath,
    const std::vector<std::pair<std::string,std::uint8_t>>& names,
    PortableCoreExecMode mode=PortableCoreExecMode::RollbackOnly,const QuestVr::Vm::Limits& limits={});
