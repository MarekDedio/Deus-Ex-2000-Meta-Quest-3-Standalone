#include "quest_script_dispatch.h"

#include <array>
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
    // Compatibility controls for callers without exact live-frame evidence.
    // These authored-mask rules are not the original IsProbing contract.
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
    Require(Dispatch::IsEnabled("Custom", 0u, {}, {}) && Dispatch::IsEnabled("Custom", all, {}, {"cUSTOM"}),
        "Nonprobe Custom was suppressed by a legacy arbitrary disabled record");
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
void OriginalPositiveLiveProbeMask() {
    constexpr auto all = std::numeric_limits<std::uint64_t>::max();
    // Independent fixed FName300..363 oracle. Do not derive the expected bit
    // from EventProbeIndex/ProbeEventName: a mutually wrong production table
    // must not silently pass a positive-mask test.
    constexpr std::array<const char*,64> names{{
        "Spawned","Destroyed","GainedChild","LostChild","Probe4","Probe5","Trigger","UnTrigger",
        "Timer","HitWall","Falling","Landed","ZoneChange","Touch","UnTouch","Bump",
        "BeginState","EndState","BaseChange","Attach","Detach","ActorEntered","ActorLeaving","KillCredit",
        "AnimEnd","EndedRotation","InterpolateEnd","EncroachingOn","EncroachedBy","FootZoneChange","HeadZoneChange","PainTimer",
        "SpeechTimer","MayFall","Probe34","Die","Tick","PlayerTick","Expired","Probe39",
        "SeePlayer","EnemyNotVisible","HearNoise","UpdateEyeHeight","SeeMonster","SeeFriend","SpecialHandling","BotDesireability",
        "Probe48","Probe49","Probe50","Probe51","Probe52","Probe53","Probe54","Probe55",
        "Probe56","Probe57","Probe58","Probe59","Probe60","Probe61","Probe62","All"
    }};
    const std::array<std::optional<Dispatch::CodeMasks>,5> authored{{
        {},Dispatch::CodeMasks{0u,0u},Dispatch::CodeMasks{0u,all},
        Dispatch::CodeMasks{all,0u},Dispatch::CodeMasks{all,all}
    }};
    struct DisabledControl { std::set<std::string> names; bool blocks{}; };
    for (std::size_t index=0u;index<names.size();++index) {
        const std::uint64_t bit = std::uint64_t{1} << index;
        const std::string name = names[index];
        std::string upper=name,mixed=name;
        for (char& character : upper)
            if (character>='a' && character<='z') character=static_cast<char>(character-'a'+'A');
        for (std::size_t character=0u;character<mixed.size();++character) {
            auto& value=mixed[character];
            if (character%2u==0u && value>='a' && value<='z') value=static_cast<char>(value-'a'+'A');
            if (character%2u!=0u && value>='A' && value<='Z') value=static_cast<char>(value-'A'+'a');
        }
        Require(Dispatch::EventProbeIndex(name)==static_cast<std::uint8_t>(index) &&
            Dispatch::ProbeEventName(static_cast<std::uint8_t>(index))==name,
            "Original fixed probe name/positive bit oracle changed");
        const std::array<std::uint64_t,6> liveMasks{{
            0u,all,0xAAAAAAAAAAAAAAAAull,0x5555555555555555ull,bit,all^bit
        }};
        const std::array<DisabledControl,5> disabled{{
            {{},false},
            {{name},true},
            {{upper},true},
            {{mixed,"LongFall","Custom"},true},
            {{names[(index+1u)%names.size()],"LongFall","Custom","None",""},false}
        }};
        for (const auto liveMask : liveMasks) for (const std::uint64_t classMask : {std::uint64_t{0},all})
            for (const auto& code : authored) for (const auto& control : disabled) {
                const bool expected=(liveMask&bit)!=0u && !control.blocks;
                for (const auto& spelling : {name,upper,mixed}) {
                    Require(Dispatch::IsEnabled(spelling,classMask,code,control.names,liveMask)==expected,
                        "Exact positive live probe mask was inverted, recomputed from authored masks, or lost its disabled-name overlay");
                }
            }
        // Original no-frame IsProbing is equivalent to the host supplying an
        // all-one live mask. Zero authored masks/IgnoreMask must not suppress it.
        Require(Dispatch::IsEnabled(name,0u,Dispatch::CodeMasks{0u,0u},{},all) &&
            Dispatch::IsEnabled(name,0u,{}, {},all),
            "Frameless all-one live eligibility incorrectly required authored class/state probe evidence");
        Require(!Dispatch::IsEnabled(name,all,Dispatch::CodeMasks{all,all},{},std::uint64_t{0}),
            "Present zero live mask was treated as absent optional evidence");
        for (const std::uint64_t classMask : {std::uint64_t{0},all}) for (const auto& code : authored)
            for (const auto& control : disabled) {
                Require(Dispatch::IsEnabled(name,classMask,code,control.names,std::nullopt)==
                    Dispatch::IsEnabled(name,classMask,code,control.names),
                    "Absent live-mask evidence changed the retained abstract authored-mask compatibility model");
            }
    }
    const std::array<std::uint64_t,7> nonprobeMasks{{
        0u,all,0xAAAAAAAAAAAAAAAAull,0x5555555555555555ull,
        std::uint64_t{1},std::uint64_t{1}<<63u,(std::uint64_t{1}<<36u)
    }};
    const std::set<std::string> arbitraryDisabled{
        "LongFall","LONGFALL","PreBeginPlay","POSTPOSTBEGINPLAY","Custom","CUSTOM","None","NONE","","Tick","All"
    };
    for (const auto* name : {"LongFall","longfall","LONGFALL","PreBeginPlay","prebeginplay",
        "PostPostBeginPlay","Custom","CUSTOM","None","NONE",""}) {
        Require(!Dispatch::EventProbeIndex(name),"Nonprobe live-mask control unexpectedly mapped to a fixed probe");
        for (const auto liveMask : nonprobeMasks) for (const std::uint64_t classMask : {std::uint64_t{0},all})
            for (const auto& code : authored) {
                Require(Dispatch::IsEnabled(name,classMask,code,arbitraryDisabled,liveMask),
                    "Live positive mask or legacy disabled records suppressed a nonprobe function");
            }
    }
}
void NonprobeLegacyRecords() {
    constexpr auto all = std::numeric_limits<std::uint64_t>::max();
    const std::set<std::string> legacy{"LongFall","LONGFALL","preBEGINplay","PostPostBeginPlay","cUSTOM","None","NONE","","Tick","Unrelated"};
    const std::vector<std::optional<Dispatch::CodeMasks>> masks{
        {},Dispatch::CodeMasks{0u,0u},Dispatch::CodeMasks{0u,all},Dispatch::CodeMasks{all,0u},Dispatch::CodeMasks{all,all}};
    for (const auto* name : {"LongFall","longfall","LONGFALL","lOnGfAlL","PreBeginPlay","prebeginplay","PREBEGINPLAY",
        "PostPostBeginPlay","Custom","custom","cUSTOM","None","none","NONE","","Unrelated","unRELATED"}) {
        Require(!Dispatch::EventProbeIndex(name),"Nonprobe event was accidentally inserted into the original 64-bit table");
        for (const auto classMask : {std::uint64_t{0},all}) for (const auto& code : masks) {
            for (const auto& disabled : std::vector<std::set<std::string>>{{},{name},{Dispatch::FoldName(name)},legacy}) {
                Require(Dispatch::IsEnabled(name,classMask,code,disabled),
                    "A class/state mask or legacy disabled-name record suppressed an original nonprobe name");
            }
        }
    }
    const std::set<std::string> nonprobes{"LongFall","PREBEGINPLAY","Custom","None","","Unrelated"};
    for (std::uint8_t index = 0u;index < 64u;++index) {
        const auto name = Dispatch::ProbeEventName(index), folded = Dispatch::FoldName(name);
        Require(Dispatch::IsEnabled(name,all,Dispatch::CodeMasks{all,all},nonprobes),
            "Legacy nonprobe records incorrectly disabled an unrelated original probe");
        auto disabled = nonprobes;disabled.insert(folded);
        Require(!Dispatch::IsEnabled(name,all,Dispatch::CodeMasks{all,all},disabled),
            "Nonprobe correction bypassed a real dynamically disabled probe");
        Require(!Dispatch::IsEnabled(name,0u,Dispatch::CodeMasks{0u,0u},nonprobes),
            "Nonprobe correction bypassed original zero-mask probe eligibility");
    }
    Require(!Dispatch::MayCallEvent("LongFall",Dispatch::IsEnabled("LongFall",0u,Dispatch::CodeMasks{0u,0u},legacy),false,false,false) &&
        !Dispatch::MayCallEvent("LongFall",true,true,true,false),"Nonprobe eligibility bypassed begun-play or pending-kill event gates");
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
        Eligibility(); OriginalPositiveLiveProbeMask(); NonprobeLegacyRecords(); Lookup();
        std::cout << "PASS original live probe masks / compatible script callback selection: " << checks << " checks, " << rejections << " rejection controls\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
