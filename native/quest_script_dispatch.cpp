#include "quest_script_dispatch.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace QuestVr::ScriptDispatch {
namespace {
constexpr std::array<const char*, 64> probes{{
    "Spawned", "Destroyed", "GainedChild", "LostChild", "Probe4", "Probe5", "Trigger", "UnTrigger",
    "Timer", "HitWall", "Falling", "Landed", "ZoneChange", "Touch", "UnTouch", "Bump",
    "BeginState", "EndState", "BaseChange", "Attach", "Detach", "ActorEntered", "ActorLeaving", "KillCredit",
    "AnimEnd", "EndedRotation", "InterpolateEnd", "EncroachingOn", "EncroachedBy", "FootZoneChange", "HeadZoneChange", "PainTimer",
    "SpeechTimer", "MayFall", "Probe34", "Die", "Tick", "PlayerTick", "Expired", "Probe39",
    "SeePlayer", "EnemyNotVisible", "HearNoise", "UpdateEyeHeight", "SeeMonster", "SeeFriend", "SpecialHandling", "BotDesireability",
    "Probe48", "Probe49", "Probe50", "Probe51", "Probe52", "Probe53", "Probe54", "Probe55",
    "Probe56", "Probe57", "Probe58", "Probe59", "Probe60", "Probe61", "Probe62", "All"
}};
void CheckName(const std::string& name, const Limits& limits, const bool path = false) {
    if (name.empty() || name.size() > limits.nameBytes || FoldName(name) == "none" ||
        name.find('\0') != std::string::npos || (path && name.find('.') == std::string::npos))
        throw std::runtime_error("Invalid script dispatch identity");
}
void Count(std::size_t& used, const std::size_t limit) {
    if (used >= limit) throw std::runtime_error("Script dispatch graph count budget exceeded");
    ++used;
}
void Retain(std::size_t& used, const std::size_t bytes, const Limits& limits) {
    if (bytes > limits.retainedBytes || used > limits.retainedBytes - bytes)
        throw std::runtime_error("Script dispatch retained byte budget exceeded");
    used += bytes;
}
void Functions(std::map<std::string, std::string>& functions, const std::string& owner,
    const Limits& limits, std::size_t& count, std::size_t& bytes) {
    std::map<std::string, std::string> normalized;
    for (const auto& [name, path] : functions) {
        Count(count, limits.functions); CheckName(name, limits); CheckName(path, limits, true);
        Retain(bytes, 256u, limits); Retain(bytes, name.size()*3u + path.size()*3u, limits);
        const auto key = FoldName(name);
        if (FoldName(path) != FoldName(owner + '.' + name) || !normalized.emplace(key, path).second)
            throw std::runtime_error("Script dispatch function ownership/identity mismatch");
    }
    functions.swap(normalized);
}
const Class* Receiver(const Graph& graph, const std::string& path) {
    const auto* cls = graph.FindClass(path);
    if (!cls) throw std::runtime_error("Script dispatch receiver class is unavailable: " + path);
    return cls;
}
} // namespace

std::string FoldName(const std::string& name) {
    if (name.empty()) return "none";
    auto result = name;
    for (char& c : result) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return result;
}
std::optional<std::uint8_t> EventProbeIndex(const std::string& name) {
    const auto key = FoldName(name);
    for (std::size_t index = 0; index < probes.size(); ++index)
        if (FoldName(probes[index]) == key) return static_cast<std::uint8_t>(index);
    return {};
}
std::string ProbeEventName(const std::uint8_t index) {
    if (index >= probes.size()) throw std::runtime_error("Script probe index is outside 64-bit mask");
    return probes[index];
}
bool IsEnabled(const std::string& name, const std::uint64_t classMask,
    const std::optional<CodeMasks> code, const std::set<std::string>& disabled,
    const std::optional<std::uint64_t> liveMask) {
    const auto index = EventProbeIndex(name);
    // Original Core ProcessEvent/Enable/Disable use only FName300..363.
    // Legacy saves may retain arbitrary disabled names; they cannot suppress
    // a nonprobe event such as LongFall and must not become a new gate.
    if (!index) return true;
    const auto bit = std::uint64_t{1} << *index;
    if (liveMask) {
        // Original IsProbing tests the live positive bit, not a fresh
        // class/state-mask expression. The optional fallback remains for
        // detached authored-mask callers, not original FStateFrame proof.
        if (!(*liveMask & bit)) return false;
    } else {
        if (code && !(code->ignoreMask & bit)) return false;
        if (!((code ? code->probeMask : 0u) & bit) && !(classMask & bit)) return false;
    }
    const auto key = FoldName(name);
    // Accept authored spellings, not only pre-normalized caller-owned sets.
    return std::none_of(disabled.begin(), disabled.end(), [&](const auto& value) { return FoldName(value) == key; });
}
bool MayCallEvent(const std::string& name, const bool enabled, const bool begun,
    const bool deleted, const bool enumDispatch) {
    return enabled && begun && (!deleted || (enumDispatch && FoldName(name) == "destroyed"));
}

Graph::Graph(std::vector<Class> classes, const Limits& limits) : limits_(limits) {
    if (classes.size() > limits_.classes) throw std::runtime_error("Script dispatch class budget exceeded");
    std::size_t stateCount{}, functionCount{}, bytes{};
    std::map<std::string, std::uint32_t> nameIndices;
    std::map<std::uint32_t, std::string> indexNames;
    for (auto& cls : classes) {
        CheckName(cls.path, limits_, true);
        if (!cls.parentPath.empty()) CheckName(cls.parentPath, limits_, true);
        Retain(bytes, sizeof(Class)+256u, limits_); Retain(bytes, cls.path.size()*3u + cls.parentPath.size()*3u, limits_);
        Functions(cls.functions, cls.path, limits_, functionCount, bytes);
        std::map<std::string, State> normalized;
        for (auto& [name, state] : cls.states) {
            Count(stateCount, limits_.states); CheckName(name, limits_); CheckName(state.name, limits_);
            CheckName(state.path, limits_, true);
            Retain(bytes, sizeof(State)+256u, limits_); Retain(bytes, (name.size()+state.name.size()+state.path.size())*3u, limits_);
            const auto key = FoldName(name);
            if (key != FoldName(state.name) || FoldName(state.path) != FoldName(cls.path + '.' + name) ||
                state.compareIndex == 0u || state.compareIndex > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
                throw std::runtime_error("Script dispatch state ownership/comparison identity mismatch");
            const auto [nameIt, newName] = nameIndices.emplace(key, state.compareIndex);
            const auto [indexIt, newIndex] = indexNames.emplace(state.compareIndex, key);
            if ((!newName && nameIt->second != state.compareIndex) || (!newIndex && indexIt->second != key))
                throw std::runtime_error("Script dispatch global comparison identity is inconsistent");
            Functions(state.functions, state.path, limits_, functionCount, bytes);
            if (!normalized.emplace(key, std::move(state)).second)
                throw std::runtime_error("Script dispatch duplicate state identity");
        }
        cls.states.swap(normalized);
        const auto key = FoldName(cls.path);
        if (!classes_.emplace(key, std::move(cls)).second)
            throw std::runtime_error("Script dispatch duplicate class identity");
    }
    // Validate the complete parent graph once; queries never follow unbounded
    // or partially validated chains. None is only represented by an empty path.
    for (const auto& [key, cls] : classes_) {
        std::set<std::string> visited;
        const Class* current = &cls;
        while (current) {
            if (visited.size() >= limits_.classDepth || !visited.insert(FoldName(current->path)).second)
                throw std::runtime_error("Script dispatch class inheritance is cyclic/deep");
            if (current->parentPath.empty()) break;
            current = FindClass(current->parentPath);
            if (!current) throw std::runtime_error("Script dispatch parent class is unavailable");
        }
    }
}
const Class* Graph::FindClass(const std::string& path) const {
    CheckQueryName(path);
    const auto found = classes_.find(FoldName(path));
    return found == classes_.end() ? nullptr : &found->second;
}
void Graph::CheckQueryName(const std::string& name) const {
    if (name.size() > limits_.nameBytes || name.find('\0') != std::string::npos)
        throw std::runtime_error("Script dispatch query identity budget exceeded");
}
std::optional<std::string> ResolveState(const Graph& graph, const std::string& classPath,
    const std::string& requested) {
    const auto* receiver = Receiver(graph, classPath);
    graph.CheckQueryName(requested);
    auto key = FoldName(requested);
    if (key == "none") return {};
    if (key == "auto") {
        for (auto* cls = receiver; cls; cls = cls->parentPath.empty() ? nullptr : graph.FindClass(cls->parentPath)) {
            const State* selected{};
            for (const auto& [name, state] : cls->states) {
                if ((state.flags & 2u) && (!selected || state.compareIndex < selected->compareIndex)) selected = &state;
            }
            if (selected) { key = FoldName(selected->name); break; }
        }
    }
    for (auto* cls = receiver; cls; cls = cls->parentPath.empty() ? nullptr : graph.FindClass(cls->parentPath)) {
        const auto found = cls->states.find(key);
        if (found != cls->states.end()) return found->second.path;
    }
    return {};
}
std::optional<std::string> ResolveFunction(const Graph& graph, const std::string& classPath,
    const std::string& stateName, const std::string& functionName, const LookupKind kind) {
    const auto* receiver = Receiver(graph, classPath);
    graph.CheckQueryName(stateName); graph.CheckQueryName(functionName);
    const auto name = FoldName(functionName);
    const auto state = FoldName(stateName);
    if (kind == LookupKind::Virtual && state != "none") {
        for (auto* cls = receiver; cls; cls = cls->parentPath.empty() ? nullptr : graph.FindClass(cls->parentPath)) {
            const auto found = cls->states.find(state);
            if (found == cls->states.end()) continue;
            const auto function = found->second.functions.find(name);
            if (function != found->second.functions.end()) return function->second;
        }
    }
    for (auto* cls = receiver; cls; cls = cls->parentPath.empty() ? nullptr : graph.FindClass(cls->parentPath)) {
        const auto function = cls->functions.find(name);
        if (function != cls->functions.end()) return function->second;
    }
    return {};
}
} // namespace QuestVr::ScriptDispatch
