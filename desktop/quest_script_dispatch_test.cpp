#include "quest_script_dispatch.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
namespace Dispatch = QuestVr::ScriptDispatch;
std::size_t checks{}, rejections{};
void Require(const bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
    ++checks;
}
template<class Action> void Reject(Action action, const char* reason) {
    bool rejected{};
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected, reason); ++rejections;
}
std::vector<Dispatch::Class> Classes() {
    Dispatch::Class base{"Test.Base", {}, 0x1234u, {}, {{"Run", "Test.Base.Run"}, {"BaseOnly", "Test.Base.BaseOnly"}}};
    base.states.emplace("Idle", Dispatch::State{"Test.Base.Idle", "Idle", 90u, 2u,
        {{"Tick", "Test.Base.Idle.Tick"}, {"Run", "Test.Base.Idle.Run"}}});
    base.states.emplace("BaseAuto", Dispatch::State{"Test.Base.BaseAuto", "BaseAuto", 50u, 2u, {}});
    Dispatch::Class derived{"Test.Derived", "Test.Base", 0u, {}, {{"Run", "Test.Derived.Run"}}};
    derived.states.emplace("Idle", Dispatch::State{"Test.Derived.Idle", "IDLE", 90u, 0u,
        {{"Run", "Test.Derived.Idle.Run"}}});
    derived.states.emplace("Alpha", Dispatch::State{"Test.Derived.Alpha", "Alpha", 200u, 2u, {}});
    derived.states.emplace("Zulu", Dispatch::State{"Test.Derived.Zulu", "Zulu", 100u, 0x80000002u, {}});
    return {base, derived};
}
void Eligibility() {
    constexpr auto all = std::numeric_limits<std::uint64_t>::max();
    for (std::uint8_t index = 0u; index < 64u; ++index) {
        const auto bit = std::uint64_t{1} << index;
        const auto name = Dispatch::ProbeEventName(index);
        Require(Dispatch::EventProbeIndex(name) == index && Dispatch::EventProbeIndex(Dispatch::FoldName(name)) == index,
            "Probe bit mapping or case identity changed");
        for (const bool cls : {false, true}) for (const bool state : {false, true}) for (const bool ignore : {false, true}) {
            Require(Dispatch::IsEnabled(name, cls ? bit : 0u, Dispatch::CodeMasks{state ? bit : 0u, ignore ? bit : 0u}, {}) ==
                (ignore && (cls || state)), "Compiled state eligibility diverged from pinned Ignore/Probe rule");
        }
        Require(!Dispatch::IsEnabled(name, 0u, {}, {}) && Dispatch::IsEnabled(name, bit, {}, {}),
            "Class-only probes require the exact receiver mask");
        Require(!Dispatch::IsEnabled(name, all, Dispatch::CodeMasks{all, all}, {name}),
            "Serialized disabled probe was ignored");
        Require(!Dispatch::IsEnabled(name, bit, {}, {Dispatch::FoldName(name)}),
            "Dynamic disabled name is not case-insensitive");
        Require(Dispatch::IsEnabled("PreBeginPlay", 0u, Dispatch::CodeMasks{0u, 0u}, {}),
            "Nonprobe startup incorrectly consults compiled masks");
    }
    Require(Dispatch::EventProbeIndex("AnimEnd") == 24u && Dispatch::EventProbeIndex("Tick") == 36u &&
        Dispatch::EventProbeIndex("All") == 63u && !Dispatch::EventProbeIndex("PreBeginPlay") &&
        !Dispatch::EventProbeIndex("PostPostBeginPlay"), "Critical event bit mapping changed");
    Require(Dispatch::IsEnabled("Custom", 0u, {}, {}) && !Dispatch::IsEnabled("Custom", all, {}, {"cUSTOM"}),
        "Arbitrary name disabled gating changed");
    Require(Dispatch::FoldName("") == "none" && Dispatch::FoldName("NONE") == "none" &&
        Dispatch::FoldName(std::string(1u, char(0xe0))) == std::string(1u, char(0xe0)),
        "Name identity folding diverged from pinned ASCII-only table");
    for (const bool enabled : {false, true}) for (const bool begun : {false, true})
        for (const bool deleted : {false, true}) for (const bool enumDispatch : {false, true}) {
            Require(Dispatch::MayCallEvent("Tick", enabled, begun, deleted, enumDispatch) == (enabled && begun && !deleted),
                "Normal event lifecycle gate changed");
            Require(Dispatch::MayCallEvent("dESTROYED", enabled, begun, deleted, enumDispatch) ==
                (enabled && begun && (!deleted || enumDispatch)), "Destroyed exception belongs only to enum dispatch");
        }
    Reject([] { Dispatch::ProbeEventName(64u); }, "Probe mask overflow index accepted");
}
void Lookup() {
    const Dispatch::Graph graph(Classes());
    Require(Dispatch::ResolveState(graph, "test.derived", "iDLE") == "Test.Derived.Idle", "Named state not derived-first");
    Require(Dispatch::ResolveState(graph, "Test.Derived", "Auto") == "Test.Derived.Zulu",
        "Auto selection used alphabetical/child/flag order instead of CompareIndex");
    Require(Dispatch::ResolveState(graph, "Test.Base", "AUTO") == "Test.Base.BaseAuto", "Base Auto selection changed");
    Require(!Dispatch::ResolveState(graph, "Test.Derived", "") && !Dispatch::ResolveState(graph, "Test.Derived", "None") &&
        !Dispatch::ResolveState(graph, "Test.Derived", "Missing"), "None/unresolved states did not select null");
    Require(Dispatch::ResolveFunction(graph, "Test.Derived", "Idle", "Run") == "Test.Derived.Idle.Run",
        "State override lost priority over class function");
    Require(Dispatch::ResolveFunction(graph, "Test.Derived", "Idle", "Tick") == "Test.Base.Idle.Tick",
        "Inherited same-name state callback unavailable");
    Require(Dispatch::ResolveFunction(graph, "Test.Derived", "Idle", "Run", Dispatch::LookupKind::Global) == "Test.Derived.Run",
        "Global call incorrectly resolved state override");
    Require(Dispatch::ResolveFunction(graph, "Test.Derived", "Idle", "BaseOnly") == "Test.Base.BaseOnly",
        "Class fallback lookup did not walk ancestors");
    Require(Dispatch::ResolveFunction(graph, "Test.Derived", "Missing", "Run") == "Test.Derived.Run" &&
        !Dispatch::ResolveFunction(graph, "Test.Derived", "Idle", "Absent"), "Absent callback should resolve None");
    auto classes = Classes();
    classes[1].states["Alpha"].flags = classes[1].states["Zulu"].flags = 0u;
    Require(Dispatch::ResolveState(Dispatch::Graph(classes), "Test.Derived", "Auto") == "Test.Base.BaseAuto",
        "Parent Auto candidate not selected after derived scan");
    classes[0].states["BaseAuto"].flags = 0u;
    Require(Dispatch::ResolveState(Dispatch::Graph(classes), "Test.Derived", "Auto") == "Test.Derived.Idle",
        "Parent Auto name did not re-resolve to derived non-Auto override");
    classes[0].states["Idle"].flags = classes[0].states["BaseAuto"].flags = 0u;
    classes[1].states.emplace("Auto", Dispatch::State{"Test.Derived.Auto", "Auto", 300u, 0u, {}});
    Require(Dispatch::ResolveState(Dispatch::Graph(classes), "Test.Derived", "Auto") == "Test.Derived.Auto",
        "Literal Auto state fallback lost");
    Reject([&] { Dispatch::ResolveState(graph, "Test.Absent", "Idle"); }, "Unavailable receiver class accepted");
    const auto mutate = [](auto action) { auto values = Classes(); action(values); Dispatch::Graph result(std::move(values)); };
    Reject([&] { mutate([](auto& v) { v[0].parentPath = v[1].path; }); }, "Class cycle accepted");
    Reject([&] { mutate([](auto& v) { v[1].parentPath = "Test.Missing"; }); }, "Missing parent accepted");
    Reject([&] { mutate([](auto& v) { v.push_back(v[0]); }); }, "Duplicate class identity accepted");
    Reject([&] { mutate([](auto& v) { v[1].states["Idle"].compareIndex = 91u; }); }, "Same name inconsistent global index accepted");
    Reject([&] { mutate([](auto& v) { v[1].states["Alpha"].compareIndex = 90u; }); }, "Distinct names sharing index accepted");
    Reject([&] { mutate([](auto& v) { v[1].states["Idle"].compareIndex = 0u; }); }, "None comparison index accepted as named state");
    Reject([&] { mutate([](auto& v) { v[1].states["Idle"].path = "Test.Base.Idle"; }); }, "State ownership mismatch accepted");
    Reject([&] { mutate([](auto& v) { v[1].functions["Run"] = "Test.Base.Run"; }); }, "Function ownership mismatch accepted");
    Reject([&] { mutate([](auto& v) { v[1].states.emplace("IDLE", v[1].states["Idle"]); }); }, "Case alias duplicate state accepted");
    Reject([&] { mutate([](auto& v) { v[1].functions.emplace("RUN", "Test.Derived.RUN"); }); }, "Case alias duplicate function accepted");
    Reject([&] { mutate([](auto& v) { v[1].path = ""; }); }, "Empty class identity accepted");
    Reject([&] { mutate([](auto& v) { v[1].states["Idle"].name = "Other"; }); }, "State map/name mismatch accepted");
    for (const unsigned budget : {0u, 1u, 2u, 3u, 4u}) {
        auto limits = Dispatch::Limits{};
        if (budget == 0u) limits.classes = 1u;
        if (budget == 1u) limits.states = 1u;
        if (budget == 2u) limits.functions = 1u;
        if (budget == 3u) limits.classDepth = 1u;
        if (budget == 4u) limits.retainedBytes = 1u;
        Reject([&] { Dispatch::Graph value(Classes(), limits); }, "Dispatch graph budget not enforced");
    }
    Reject([&] { Dispatch::ResolveState(graph,"Test.Derived",std::string(8193u,'A')); }, "Unbounded state query accepted");
}
} // namespace
int main() {
    try {
        Eligibility(); Lookup();
        std::cout << "PASS pinned script callback gating/selection: " << checks << " checks, " << rejections << " rejection controls\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
