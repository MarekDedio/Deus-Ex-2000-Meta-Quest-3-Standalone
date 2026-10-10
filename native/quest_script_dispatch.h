#pragma once

// Pure selection/gating against pinned UE1 UObject/ScriptCall semantics.
// This does not enter states, execute bytecode or enable automatic callbacks.
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace QuestVr::ScriptDispatch {

struct CodeMasks {
    std::uint64_t probeMask{}, ignoreMask{};
};

// ASCII case folding only, matching NameString's identity table. Empty names
// and any case of "None" share the canonical "none" identity.
std::string FoldName(const std::string& name);
std::optional<std::uint8_t> EventProbeIndex(const std::string& eventName);
std::string ProbeEventName(std::uint8_t index);
// Only the original 64 FName300..363 probes use these masks/disabled records.
// Nonprobe names are always enabled here, including legacy arbitrary disabled
// entries. Actor lifecycle gates remain separate in MayCallEvent.
// A supplied positive live mask takes precedence over authored class/code
// masks, matching original IsProbing. Nullopt retains the detached mask model.
bool IsEnabled(const std::string& eventName, std::uint64_t classProbeMask,
    std::optional<CodeMasks> currentCode, const std::set<std::string>& disabledNames,
    std::optional<std::uint64_t> liveProbeMask = std::nullopt);
// Actor CallEvent gate, separate from Frame.Call's eligibility check. Only
// enum dispatch grants Destroyed's post-deletion exception.
bool MayCallEvent(const std::string& eventName, bool enabled,
    bool levelBegunPlay, bool deleted, bool enumDispatch);

struct State {
    std::string path, name;
    // REQUIRED actual NameString.GetCompareIndex(), not a package name index,
    // alphabetical rank, child order or a freshly invented per-graph index.
    std::uint32_t compareIndex{};
    std::uint32_t flags{};
    std::map<std::string, std::string> functions; // Name -> qualified path.
};
struct Class {
    std::string path, parentPath;
    std::uint64_t probeMask{}; // Already-authored receiver-class mask only.
    std::map<std::string, State> states; // Name -> owned named state.
    std::map<std::string, std::string> functions; // Name -> qualified path.
};
struct Limits {
    std::size_t classes{65536u}, states{131072u}, functions{524288u};
    std::size_t classDepth{256u}, nameBytes{8192u};
    std::size_t retainedBytes{64u << 20u};
};

// Owns a bounded, validated snapshot. Construction rejects missing parents,
// cycles, case-insensitive duplicate identities, bad ownership and inconsistent
// global comparison indices. Query results are detached path values.
class Graph {
public:
    explicit Graph(std::vector<Class> classes, const Limits& limits = {});
    const Class* FindClass(const std::string& path) const;
    void CheckQueryName(const std::string& name) const;
private:
    std::map<std::string, Class> classes_;
    Limits limits_;
};

enum class LookupKind { Virtual, Global };
// Missing named states/functions return nullopt. Unknown receiver classes are
// invalid metadata and throw. Auto uses the exact supplied comparison order;
// without an Auto-flag candidate it still tries a state literally named Auto.
std::optional<std::string> ResolveState(const Graph& graph,
    const std::string& classPath, const std::string& requestedState);
std::optional<std::string> ResolveFunction(const Graph& graph,
    const std::string& classPath, const std::string& currentStateName,
    const std::string& functionName, LookupKind kind = LookupKind::Virtual);
// Final-call target selection remains the caller's responsibility.

} // namespace QuestVr::ScriptDispatch
