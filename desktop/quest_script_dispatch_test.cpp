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
    Dispatch::Class base{"Test.Base", {}, 0x1234u, {}, {{"Run", "Test.Base.Run"}, {"BaseOnly", "Test.Base.BaseOnly"}}, {}};
    base.states.emplace("Idle", Dispatch::State{"Test.Base.Idle", "Idle", 90u, 2u,
        {{"Tick", "Test.Base.Idle.Tick"}, {"Run", "Test.Base.Idle.Run"}}, {}});
    base.states.emplace("BaseAuto", Dispatch::State{"Test.Base.BaseAuto", "BaseAuto", 50u, 2u, {}, {}});
    base.stateChildOrder={"Test.Base.Idle","Test.Base.BaseAuto"};
    Dispatch::Class derived{"Test.Derived", "Test.Base", 0u, {}, {{"Run", "Test.Derived.Run"}}, {}};
    derived.states.emplace("Idle", Dispatch::State{"Test.Derived.Idle", "IDLE", 90u, 0u,
        {{"Run", "Test.Derived.Idle.Run"}}, "Test.Base.Idle"});
    derived.states.emplace("Alpha", Dispatch::State{"Test.Derived.Alpha", "Alpha", 200u, 2u, {}, {}});
    derived.states.emplace("Zulu", Dispatch::State{"Test.Derived.Zulu", "Zulu", 100u, 0x80000002u, {}, {}});
    derived.stateChildOrder={"Test.Derived.Idle","Test.Derived.Alpha","Test.Derived.Zulu"};
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
    Require(Dispatch::ResolveState(graph, "Test.Derived", "Auto") == "Test.Derived.Alpha",
        "Auto selection did not use exact Children order independent of CompareIndex");
    Require(Dispatch::ResolveState(graph, "Test.Base", "AUTO") == "Test.Base.Idle", "Base Auto child order changed");
    Require(!Dispatch::ResolveState(graph, "Test.Derived", "") && !Dispatch::ResolveState(graph, "Test.Derived", "None") &&
        !Dispatch::ResolveState(graph, "Test.Derived", "Missing"), "None/unresolved states did not select null");
    Require(Dispatch::ResolveFunction(graph, "Test.Derived", "Idle", "Run") == "Test.Derived.Idle.Run",
        "State override lost priority over class function");
    Require(Dispatch::ResolveFunction(graph, "Test.Derived", "Idle", "Tick") == "Test.Base.Idle.Tick",
        "Explicit inherited state callback unavailable");
    Require(Dispatch::ResolveFunction(graph, "Test.Derived", "Idle", "Run", Dispatch::LookupKind::Global) == "Test.Derived.Run",
        "Global call incorrectly resolved state override");
    Require(Dispatch::ResolveFunction(graph, "Test.Derived", "Idle", "BaseOnly") == "Test.Base.BaseOnly",
        "Class fallback lookup did not walk ancestors");
    Require(Dispatch::ResolveFunction(graph, "Test.Derived", "Missing", "Run") == "Test.Derived.Run" &&
        !Dispatch::ResolveFunction(graph, "Test.Derived", "Idle", "Absent"), "Absent callback should resolve None");
    auto classes = Classes();
    classes[1].states["Alpha"].flags = classes[1].states["Zulu"].flags = 0u;
    Require(Dispatch::ResolveState(Dispatch::Graph(classes), "Test.Derived", "Auto") == "Test.Base.Idle",
        "Parent Auto candidate not selected after derived scan");
    classes[0].stateChildOrder={"Test.Base.BaseAuto","Test.Base.Idle"};
    Require(Dispatch::ResolveState(Dispatch::Graph(classes), "Test.Derived", "Auto") == "Test.Base.BaseAuto",
        "Reordered actual parent Children did not affect first Auto candidate");
    classes[0].states["Idle"].flags = classes[0].states["BaseAuto"].flags = 0u;
    classes[1].states.emplace("Auto", Dispatch::State{"Test.Derived.Auto", "Auto", 300u, 0u, {}, {}});
    classes[1].stateChildOrder.push_back("Test.Derived.Auto");
    Require(!Dispatch::ResolveState(Dispatch::Graph(classes), "Test.Derived", "Auto"),
        "Absent Auto flag incorrectly selected a literal Auto-named state");
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
void OriginalSelectedNodeInheritance() {
    auto values=Classes();
    values[0].states.emplace("Old",Dispatch::State{"Test.Base.Old","Old",401u,0u,
        {{"Tick","Test.Base.Old.Tick"}}, {}});
    values[0].states.emplace("New",Dispatch::State{"Test.Base.New","New",402u,0u,
        {{"Tick","Test.Base.New.Tick"}}, {}});
    values[0].stateChildOrder.insert(values[0].stateChildOrder.end(),{"Test.Base.Old","Test.Base.New"});
    values[1].functions.emplace("Tick","Test.Derived.Tick");
    values[1].states.emplace("Old",Dispatch::State{"Test.Derived.Old","Old",401u,0u,
        {{"Tick","Test.Derived.Old.Tick"}}, {}});
    values[1].states.emplace("New",Dispatch::State{"Test.Derived.New","New",402u,0u,{},"Test.Base.Old"});
    values[1].stateChildOrder.insert(values[1].stateChildOrder.end(),{"Test.Derived.New","Test.Derived.Old"});
    const Dispatch::Graph graph(values);
    Require(Dispatch::ResolveFunction(graph,"Test.Derived","New","Tick")=="Test.Base.Old.Tick",
        "Differently named explicit State SuperField was replaced by receiver same-name traversal");
    Require(Dispatch::ResolveFunctionAtSelectedNode(graph,"Test.Derived","Test.Base.Idle","Run")=="Test.Base.Idle.Run",
        "Exact inherited selected node was intercepted by derived same-name state");
    Require(Dispatch::ResolveFunctionAtSelectedNode(graph,"Test.Derived","Test.Derived.New","Run")=="Test.Derived.Run",
        "Absent state member must fall back from receiver class, not selected state's owner");
    Require(Dispatch::ResolveFunctionAtSelectedNode(graph,"Test.Derived","Test.Derived.New","Tick",Dispatch::LookupKind::Global)=="Test.Derived.Tick",
        "Global callback consulted selected state");
    Require(Dispatch::SelectedNodeAncestry(graph,"Test.Derived","Test.Derived.New")==
        std::vector<std::string>{"Test.Derived.New","Test.Base.Old"},"Explicit selected-node ancestry changed");
    Require(Dispatch::SelectedNodeAncestry(graph,"Test.Derived","Test.Derived")==
        std::vector<std::string>{"Test.Derived","Test.Base"},"Class-backed selected-node ancestry changed");
    Require(Dispatch::SelectedNodeAncestry(graph,"Test.Derived",{}).empty(),"Absent selected frame manufactured ancestry");
    Require(Dispatch::ResolveFunctionAtSelectedNode(graph,"Test.Derived","Test.Derived","Tick")=="Test.Derived.Tick" &&
        Dispatch::ResolveFunctionAtSelectedNode(graph,"Test.Derived","test.DERIVED","Tick")=="Test.Derived.Tick",
        "Receiver Class-backed selection lost its own callback or case-insensitive identity");
    Require(Dispatch::ResolveFunctionAtSelectedNode(graph,"Test.Derived",{},"Tick")=="Test.Derived.Tick",
        "Absent selected node failed to use receiver class fallback");
    Require(Dispatch::ResolveFunctionAtSelectedNode(graph,"Test.Derived","Test.Base","Run")=="Test.Base.Run" &&
        Dispatch::SelectedNodeAncestry(graph,"Test.Derived","Test.Base")==std::vector<std::string>{"Test.Base"},
        "Detached ancestor-Class selection lost its explicit ancestry priority");
    Require(Dispatch::ResolveFunctionAtSelectedNode(graph,"Test.Derived","Test.Base","Run",Dispatch::LookupKind::Global)=="Test.Derived.Run",
        "Global lookup was intercepted by an ancestor-Class selected node");
    auto classParent=values;classParent[1].states["New"].parentPath="Test.Base";
    const Dispatch::Graph classParentGraph(classParent);
    Require(Dispatch::SelectedNodeAncestry(classParentGraph,"Test.Derived","Test.Derived.New")==
        std::vector<std::string>{"Test.Derived.New","Test.Base"} &&
        Dispatch::ResolveFunctionAtSelectedNode(classParentGraph,"Test.Derived","Test.Derived.New","Run")=="Test.Base.Run",
        "Explicit State-to-Class SuperField failed to preserve detached metadata ancestry");
    Require(Dispatch::ResolveFunctionAtSelectedNode(classParentGraph,"Test.Derived","Test.Derived.New","Tick")=="Test.Derived.Tick",
        "Missing explicit ancestor-Class callback did not fall back to receiver class");
    auto literalAuto=values;
    literalAuto[1].states.emplace("Auto",Dispatch::State{"Test.Derived.Auto","Auto",300u,0u,
        {{"Tick","Test.Derived.Auto.Tick"}}, {}});
    literalAuto[1].stateChildOrder.push_back("Test.Derived.Auto");
    const Dispatch::Graph literalAutoGraph(literalAuto);
    Require(Dispatch::ResolveState(literalAutoGraph,"Test.Derived","Auto")=="Test.Derived.Alpha",
        "Auto request control did not select the first flagged state");
    Require(Dispatch::ResolveFunction(literalAutoGraph,"Test.Derived","Auto","Tick")=="Test.Derived.Auto.Tick" &&
        Dispatch::ResolveFunction(literalAutoGraph,"Test.Derived","aUTO","Tick")=="Test.Derived.Auto.Tick",
        "Current Auto-named state was incorrectly treated as an Auto-selection request");
    Require(Dispatch::ResolveFunction(literalAutoGraph,"Test.Derived","Auto","Tick",Dispatch::LookupKind::Global)=="Test.Derived.Tick",
        "Global lookup consulted the current Auto-named state");
    auto noParent=values;noParent[1].states["Idle"].parentPath.clear();
    Require(Dispatch::ResolveFunction(Dispatch::Graph(noParent),"Test.Derived","Idle","Tick")=="Test.Derived.Tick",
        "Uninherited state acquired same-name base callback");
    // Children Zulu before Alpha; both alphabetical and comparison-index order
    // place Alpha before Zulu after the comparison-index mutation below.
    auto order=values;order[1].stateChildOrder={"Test.Derived.Zulu","Test.Derived.Alpha","Test.Derived.Idle","Test.Derived.New","Test.Derived.Old"};
    order[1].states["Alpha"].compareIndex=80u;
    Require(Dispatch::ResolveState(Dispatch::Graph(order),"Test.Derived","Auto")=="Test.Derived.Zulu",
        "Auto used either alphabetical or comparison-index order instead of actual child order");
    const auto mutate=[&](auto action) {auto changed=values;action(changed);Dispatch::Graph invalid(std::move(changed));};
    Reject([&]{mutate([](auto& v){v[1].stateChildOrder.pop_back();});},"Incomplete state child order accepted");
    Reject([&]{mutate([](auto& v){v[1].stateChildOrder[0]=v[1].stateChildOrder[1];});},"Duplicate state child order accepted");
    Reject([&]{mutate([](auto& v){v[1].stateChildOrder[0]="test.derived.ALPHA";});},"Case-duplicate state child accepted");
    Reject([&]{mutate([](auto& v){v[1].stateChildOrder[0]="Test.Base.Idle";});},"Foreign state child accepted");
    Reject([&]{mutate([](auto& v){v[1].stateChildOrder[0]="Test.Derived.Missing";});},"Missing state child accepted");
    Reject([&]{mutate([](auto& v){v.push_back(Dispatch::Class{"Test.Base.Idle","Test.Base",0u,{}, {}, {}});});},
        "Class and State with the same qualified path were accepted");
    Reject([&]{mutate([](auto& v){v.push_back(Dispatch::Class{"test.BASE.idle","Test.Base",0u,{}, {}, {}});});},
        "Case-insensitive Class/State qualified-path collision was accepted");
    Reject([&]{mutate([](auto& v){v[1].states["New"].parentPath="Test.Missing.State";});},"Missing State SuperField accepted");
    Reject([&]{mutate([](auto& v){v[1].states["New"].parentPath="Test.Base.Run";});},"Function used as State SuperField");
    Reject([&]{mutate([](auto& v){v[1].states["New"].parentPath="Test.Derived.Old";v[1].states["Old"].parentPath="Test.Derived.New";});},"Cyclic State SuperField accepted");
    Reject([&]{mutate([](auto& v){v[0].states["Old"].parentPath="Test.Derived.Old";});},"Descendant-owner State SuperField accepted");
    Reject([&]{Dispatch::SelectedNodeAncestry(graph,"Test.Base","Test.Derived.New");},"Foreign selected state accepted");
    Reject([&]{Dispatch::SelectedNodeAncestry(graph,"Test.Base","Test.Derived");},"Descendant selected Class accepted for base receiver");
    Reject([&]{Dispatch::ResolveFunctionAtSelectedNode(graph,"Test.Derived","Test.Missing.Idle","Tick");},"Unknown selected state accepted");
    Reject([&]{Dispatch::SelectedNodeAncestry(graph,"Test.Derived",std::string(8193u,'X'));},"Unbounded selected path accepted");
    auto deep=values;deep[1].states["New"].parentPath="Test.Derived.Old";
    deep[1].states["Old"].parentPath="Test.Base.Old";auto limits=Dispatch::Limits{};limits.classDepth=2u;
    Reject([&]{Dispatch::Graph invalid(deep,limits);},"Over-depth explicit state ancestry accepted");
    // No internal pointer index: copied/moved graphs must still own their data.
    auto copy=graph;
    Require(Dispatch::ResolveFunctionAtSelectedNode(copy,"Test.Derived","Test.Derived.New","Tick")=="Test.Base.Old.Tick",
        "Copied selected-node graph lost its owned ancestry");
}
} // namespace
int main() {
    try {
        Eligibility(); OriginalPositiveLiveProbeMask(); NonprobeLegacyRecords(); Lookup(); OriginalSelectedNodeInheritance();
        std::cout << "PASS original live probe masks / compatible script callback selection: " << checks << " checks, " << rejections << " rejection controls\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
