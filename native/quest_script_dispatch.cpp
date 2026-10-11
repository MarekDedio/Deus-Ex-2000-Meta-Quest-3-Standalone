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
bool ClassAncestor(const Graph& graph,const Class* receiver,const std::string& path) {
    for(auto* cls=receiver;cls;cls=cls->parentPath.empty()?nullptr:graph.FindClass(cls->parentPath))
        if(FoldName(cls->path)==FoldName(path)) return true;
    return false;
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
            if(!state.parentPath.empty()) CheckName(state.parentPath,limits_,true);
            Retain(bytes, sizeof(State)+256u, limits_); Retain(bytes, (name.size()+state.name.size()+state.path.size()+state.parentPath.size())*3u, limits_);
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
        if(cls.stateChildOrder.size()!=cls.states.size())
            throw std::runtime_error("Script dispatch state child order is incomplete");
        std::set<std::string> children;
        for(auto& path:cls.stateChildOrder) {
            CheckName(path,limits_,true);Retain(bytes,sizeof(std::string)+path.size()*3u,limits_);
            const auto dot=path.find_last_of('.');
            const auto state=cls.states.find(FoldName(path.substr(dot+1u)));
            if(state==cls.states.end()||FoldName(state->second.path)!=FoldName(path)||!children.insert(FoldName(path)).second)
                throw std::runtime_error("Script dispatch state child order has a foreign, missing or duplicate child");
            path=state->second.path;
        }
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
    // State SuperField can refer to a differently named state. Validate actual
    // links rather than manufacturing same-name superclass inheritance.
    for(const auto& [key,cls]:classes_) for(const auto& [name,state]:cls.states) {
        if(FindClass(state.path))
            throw std::runtime_error("Script dispatch class/state path identity is duplicated");
        std::set<std::string> visited;
        std::string path=state.path;
        while(!path.empty()) {
            if(visited.size()>=limits_.classDepth||!visited.insert(FoldName(path)).second)
                throw std::runtime_error("Script dispatch state inheritance is cyclic/deep");
            if(const auto* parentClass=FindClass(path)) {
                if(!ClassAncestor(*this,&cls,parentClass->path))
                    throw std::runtime_error("Script dispatch selected class is outside its owner's ancestry");
                path=parentClass->parentPath;
            } else {
                const auto* node=FindState(path);
                if(!node||!ClassAncestor(*this,&cls,node->path.substr(0u,node->path.find_last_of('.'))))
                    throw std::runtime_error("Script dispatch state parent is unavailable or has a foreign owner");
                path=node->parentPath;
            }
        }
    }
}
const Class* Graph::FindClass(const std::string& path) const {
    CheckQueryName(path);
    const auto found = classes_.find(FoldName(path));
    return found == classes_.end() ? nullptr : &found->second;
}
const State* Graph::FindState(const std::string& path) const {
    CheckQueryName(path);
    const auto dot=path.find_last_of('.');
    if(dot==std::string::npos) return nullptr;
    const auto* cls=FindClass(path.substr(0u,dot));
    if(!cls) return nullptr;
    const auto found=cls->states.find(FoldName(path.substr(dot+1u)));
    return found==cls->states.end()||FoldName(found->second.path)!=FoldName(path)?nullptr:&found->second;
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
            for(const auto& path:cls->stateChildOrder)
                if(const auto* state=graph.FindState(path);state->flags&2u) return state->path;
        }
        return {};
    }
    for (auto* cls = receiver; cls; cls = cls->parentPath.empty() ? nullptr : graph.FindClass(cls->parentPath)) {
        const auto found = cls->states.find(key);
        if (found != cls->states.end()) return found->second.path;
    }
    return {};
}
std::optional<std::string> ResolveFunction(const Graph& graph, const std::string& classPath,
    const std::string& stateName, const std::string& functionName, const LookupKind kind) {
    const auto* receiver=Receiver(graph,classPath);
    graph.CheckQueryName(stateName); graph.CheckQueryName(functionName);
    // This is a current state NAME, not a GotoState request. In particular, a
    // state literally named Auto must not trigger the Auto-state iterator.
    std::optional<std::string> selected;
    if(kind==LookupKind::Virtual) {
        const auto key=FoldName(stateName);
        for(auto* cls=receiver;cls;cls=cls->parentPath.empty()?nullptr:graph.FindClass(cls->parentPath)) {
            const auto found=cls->states.find(key);
            if(found!=cls->states.end()) {selected=found->second.path;break;}
        }
    }
    return ResolveFunctionAtSelectedNode(graph,classPath,selected.value_or(std::string{}),functionName,kind);
}
std::vector<std::string> SelectedNodeAncestry(const Graph& graph,const std::string& classPath,
    const std::string& selectedPath) {
    const auto* receiver=Receiver(graph,classPath);graph.CheckQueryName(selectedPath);
    std::vector<std::string> result;
    auto path=selectedPath;
    // Graph construction already validated every link's depth/cycle/ownership.
    while(!path.empty()) {
        if(const auto* cls=graph.FindClass(path)) {
            if(!ClassAncestor(graph,receiver,cls->path))
                throw std::runtime_error("Script dispatch selected class is not a receiver ancestor");
            result.push_back(cls->path);path=cls->parentPath;
        } else {
            const auto* state=graph.FindState(path);
            if(!state||!ClassAncestor(graph,receiver,state->path.substr(0u,state->path.find_last_of('.'))))
                throw std::runtime_error("Script dispatch selected state is unavailable or foreign to receiver");
            result.push_back(state->path);path=state->parentPath;
        }
    }
    return result;
}
std::optional<std::string> ResolveFunctionAtSelectedNode(const Graph& graph,const std::string& classPath,
    const std::string& selectedPath,const std::string& functionName,const LookupKind kind) {
    const auto* receiver=Receiver(graph,classPath);
    graph.CheckQueryName(selectedPath);graph.CheckQueryName(functionName);
    const auto name=FoldName(functionName);
    if(kind==LookupKind::Virtual) for(const auto& path:SelectedNodeAncestry(graph,classPath,selectedPath)) {
        const auto* state=graph.FindState(path);
        const auto& functions=state?state->functions:graph.FindClass(path)->functions;
        const auto found=functions.find(name);
        if(found!=functions.end()) return found->second;
    }
    for (auto* cls = receiver; cls; cls = cls->parentPath.empty() ? nullptr : graph.FindClass(cls->parentPath)) {
        const auto function = cls->functions.find(name);
        if (function != cls->functions.end()) return function->second;
    }
    return {};
}
} // namespace QuestVr::ScriptDispatch
