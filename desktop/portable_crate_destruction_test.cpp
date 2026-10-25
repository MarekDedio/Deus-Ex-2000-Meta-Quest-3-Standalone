#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_engine_collision.h"
#include "portable_object_lookup.h"
#include "quest_save_bundle.h"
#include "quest_script_dispatch.h"
#include "quest_script_state.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Owned-original integration diagnostic. This deliberately calls the real
// inherited Destroy279 entry, never the Destroyed callback by hand. Explicit
// saved begun/startup preconditions isolate native destruction; they are NOT
// evidence that the application has started the original world or player.
namespace {
using Bytes = std::vector<std::uint8_t>;
using Value = QuestVr::Vm::Value;
using Kind = QuestVr::Vm::Kind;
std::size_t checks{}, refusals{};
void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
Bytes ReadBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    Require(bool(stream), "Could not read diagnostic file: " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
std::string Fold(std::string text) {
    for (auto& c : text) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return text;
}
bool Within(const std::filesystem::path& child, const std::filesystem::path& parent) {
    auto a = child.begin(), b = parent.begin();
    for (; b != parent.end(); ++a, ++b)
        if (a == child.end() || Fold(a->string()) != Fold(b->string())) return false;
    return true;
}
struct Temporary {
    std::filesystem::path parent, directory;
    mutable std::vector<std::filesystem::path> files;
    explicit Temporary(const std::filesystem::path& original) {
        parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        Require(parent != parent.root_path() && !Within(parent, std::filesystem::canonical(original)),
            "Generated crate diagnostic temporary parent overlaps original game/root");
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0u; attempt < 20u; ++attempt) {
            const auto candidate = parent / ("quest-crate-destroy-" + std::to_string(stamp) + '-' + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) {
                directory = std::filesystem::canonical(candidate);
                break;
            }
        }
        Require(!directory.empty() && directory.parent_path() == parent,
            "Could not create dedicated crate diagnostic directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();
        std::error_code ignored;
        // Exact recorded files only; no recursive removal or commercial path.
        for (const auto& file : files) if (file.parent_path() == directory)
            std::filesystem::remove(file, ignored);
        std::filesystem::remove(directory, ignored);
    }
    std::filesystem::path Path(const std::string& name) const {
        Require(!name.empty() && name.find_first_of("/\\:") == std::string::npos,
            "Invalid isolated crate checkpoint leaf");
        const auto path = directory / (name + ".sav");
        if (std::find(files.begin(), files.end(), path) == files.end()) files.push_back(path);
        return path;
    }
    Bytes Snapshot(const std::string& name) const {
        const auto path = Path(name);
        Require(SavePortableRuntimeState(path.string()), "Could not save complete crate diagnostic checkpoint");
        return ReadBytes(path);
    }
};
std::uint32_t Word(const Bytes& bytes, const std::size_t offset) {
    Require(offset <= bytes.size() && bytes.size() - offset >= 4u, "Crate checkpoint word exceeds bound");
    std::uint32_t value{};
    for (unsigned i = 0u; i < 4u; ++i) value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8u * i);
    return value;
}
std::size_t PrefixSize(const Bytes& bytes) {
    Require(Word(bytes, 0u) == 0x53515844u && Word(bytes, 4u) >= 3u && Word(bytes, 4u) <= 13u,
        "Unknown crate diagnostic runtime envelope");
    std::size_t cursor = 8u;
    const auto word = [&]() { const auto value = Word(bytes, cursor); cursor += 4u; return value; };
    const auto skip = [&](const std::size_t count) {
        Require(cursor <= bytes.size() && count <= bytes.size() - cursor, "Crate checkpoint prefix exceeds bound");
        cursor += count;
    };
    const auto string = [&]() { const auto count = word(); Require(count <= 1'048'576u, "Crate prefix string exceeds bound"); skip(count); };
    const auto list = [&]() {
        const auto count = word(); Require(count <= 100'000u, "Crate prefix list exceeds bound");
        for (std::uint32_t i = 0u; i < count; ++i) string();
    };
    list(); list(); list(); skip(4u);
    const auto damaged = word(); Require(damaged <= 100'000u, "Crate prefix damage list exceeds bound");
    for (std::uint32_t i = 0u; i < damaged; ++i) { string(); skip(4u); }
    skip(8u); list(); list(); list(); list();
    return cursor;
}
QuestVr::ScriptSavedState Decode(const Bytes& bytes) {
    const auto offset = PrefixSize(bytes);
    const auto size = Word(bytes, offset);
    Require(offset + 4u <= bytes.size() && size == bytes.size() - offset - 4u,
        "Crate script checkpoint does not cover complete trailer");
    return QuestVr::DecodeScriptSavedState(Bytes(bytes.begin() + static_cast<std::ptrdiff_t>(offset + 4u), bytes.end()));
}
Bytes Envelope(const Bytes& baseline, const QuestVr::ScriptSavedState& state) {
    Require(Word(baseline, 4u) == 3u, "Explicit crate fixture needs untouched original v3 prefix");
    auto bytes = baseline;
    const auto blob = QuestVr::EncodeScriptSavedState(state);
    Require(blob.size() > 6u && blob.at(6u) >= 1u && blob.at(6u) <= 10u,
        "Crate fixture selected unknown script codec version");
    bytes[4u] = static_cast<std::uint8_t>(3u + blob.at(6u));
    for (unsigned i = 0u; i < 4u; ++i) bytes.push_back(static_cast<std::uint8_t>(blob.size() >> (8u * i)));
    bytes.insert(bytes.end(), blob.begin(), blob.end());
    return bytes;
}
void LoadFixture(const Temporary& temporary, const std::string& name, const Bytes& baseline,
    const std::string& level, const std::string& levelClass, const bool begun, const bool startup,
    const std::vector<QuestVr::ScriptSavedObject>& extra = {}) {
    QuestVr::ScriptSavedState state;
    state.mapName = "00_Training";
    state.objects.push_back({level, levelClass,
        {{"Engine.LevelInfo.bBegunPlay", "bBegunPlay", 0u, Value::Bool(begun)},
         {"Engine.LevelInfo.bStartup", "bStartup", 0u, Value::Bool(startup)},
         {"Engine.LevelInfo.NetMode", "NetMode", 0u, Value::Byte(0u)}}, {}});
    state.objects.insert(state.objects.end(),extra.begin(),extra.end());
    const auto path = temporary.Path(name);
    Require(QuestVr::WriteDurableSaveFile(path.string(), Envelope(baseline, state)) &&
        ValidatePortableRuntimeState(path.string(), "00_Training") && LoadPortableRuntimeState(path.string()),
        "Could not load schema-validated isolated crate begun/startup fixture");
}
bool Published(const std::string& actor) {
    const auto world = GetPortableRuntimeMapActors(true);
    return std::any_of(world.begin(), world.end(), [&](const auto& item) { return item.objectPath == actor; });
}
void TickTail(const Temporary& temporary,const Bytes& baseline,const std::string& level,const std::string& levelClass,
    const std::string& ini,const std::vector<PortablePackageTables>& packages,const std::string& mapSource) {
    const auto prepare=[&] {
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision(ini).prepared,
            "Actual Tick expiry fixture could not establish its genuine Engine/Level/native lookup services");
    };
    prepare();
    const std::string actor="00_Training.CrateBreakableMedGeneral1";
    const auto load=[&](const std::string& name,float lifespan) {
        LoadFixture(temporary,name,baseline,level,levelClass,true,false,
            {{actor,"DeusEx.CrateBreakableMedGeneral",
                {{"Engine.Actor.LifeSpan","LifeSpan",0u,Value::Float(lifespan)},
                 {"Engine.Actor.Physics","Physics",0u,Value::Byte(0u)}},{}}});
        const auto state=ExecutePortableActorFunction(actor,"GotoState",{{Value::Text(Kind::Name,"None"),{}}});
        Require(state.passed()&&state.committed,
            "Actual Tick fixture could not initialize the existing Actor frame through original GotoState(None): "+state.error);
    };
    load("TickTailBegin",1.0f);
    const auto before=temporary.Snapshot("TickTailBefore");
    const auto revision=GetPortableRuntimeWorldRevision();
    const auto skip=AdvancePortableActorNonPawnTickTail(actor,0.125f,1);
    Require(skip.passed()&&skip.committed&&skip.value.integer==1&&temporary.Snapshot("TickTailSkipped")==before&&
        GetPortableRuntimeWorldRevision()==revision,"Original tickType1 tail did not preserve the complete actual crate root");
    const auto step=AdvancePortableActorNonPawnTickTail(actor,0.125f,0);
    Require(step.passed()&&step.committed&&step.value.integer==1&&Published(actor)&&
        ReadPortableActorScriptProperty(actor,"LifeSpan").floating==0.875f,
        "Original actual-crate post-animation Tick tail failed: "+step.error);
    const auto after=temporary.Snapshot("TickTailAfter");
    Require(before!=after&&LoadPortableRuntimeState(temporary.Path("TickTailAfter").string())&&
        temporary.Snapshot("TickTailReloaded")==after&&ReadPortableActorScriptProperty(actor,"LifeSpan").floating==0.875f,
        "Actual-crate LifeSpan Tick update did not round-trip its authoritative save");
    Require(LoadPortableRuntimeState(temporary.Path("TickTailBefore").string()),"Could not restore actual-crate Tick prefix");
    const auto rollbackRevision=GetPortableRuntimeWorldRevision();
    QuestVr::Vm::Limits cutoff;cutoff.writes=0u;
    const auto refused=AdvancePortableActorNonPawnTickTail(actor,0.125f,0,cutoff);
    Require(!refused.passed()&&!refused.committed&&refused.status==QuestVr::Vm::Status::Budget&&
        temporary.Snapshot("TickTailRefused")==before&&GetPortableRuntimeWorldRevision()==rollbackRevision,
        "Actual-crate Tick write cutoff did not restore the entire caller root");
    ++refusals;
    load("TickTailExpiryBegin",0.125f);
    const auto expiryBefore=temporary.Snapshot("TickTailExpiryBefore");
    const auto ownershipBefore=ReadPortableRuntimeDeferredOwnership();
    QuestVr::Vm::Limits late;late.instructions=step.instructions+8u;const auto revisionBefore=GetPortableRuntimeWorldRevision();
    GC::Collect();const auto gcBefore=GC::GetStats();
    const auto lateRefusal=AdvancePortableActorNonPawnTickTail(actor,0.125f,0,late);
    Require(!lateRefusal.passed()&&!lateRefusal.committed&&lateRefusal.status==QuestVr::Vm::Status::Budget&&
        lateRefusal.instructions==late.instructions&&lateRefusal.writes>0u&&
        temporary.Snapshot("TickTailExpiryRefused")==expiryBefore&&GetPortableRuntimeWorldRevision()==revisionBefore&&Published(actor)&&
        ReadPortableRuntimeDeferredOwnership()==ownershipBefore,
        "Late original expiry refusal leaked countdown/deletion/native ownership or missed its actual budget boundary: "+lateRefusal.error);
    ++refusals;GC::Collect();const auto gcAfter=GC::GetStats();
    Require(gcBefore.numObjects==gcAfter.numObjects&&gcBefore.memoryUsage==gcAfter.memoryUsage,
        "Refused original Tick expiry retained GC-owned actor births");
    const auto expiry=AdvancePortableActorNonPawnTickTail(actor,0.125f,0);
    Require(expiry.passed()&&expiry.committed&&!Published(actor)&&
        QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor,"bDeleteMe"))&&
        ReadPortableActorScriptProperty(actor,"LifeSpan").floating==0.0f,
        "Actual-crate Tick expiry failed original Expired -> DestroyActor -> Containers.Destroyed: "+expiry.error);
    const auto ownership=ReadPortableRuntimeDeferredOwnership();
    Require(ownership.available&&!ownership.firstDeletedPath.empty()&&ownership!=ownershipBefore,
        "Actual expiry did not retain original deferred native Actor ownership");
    Require(!SavePortableRuntimeState(temporary.Path("TickTailDeferredCaptureRefused").string()),
        "Native deferred deletion was serialized without a supported checkpoint/cleanup protocol");
    // Recreate the CPU fixture, rather than bypassing the real native deferred
    // capture/rebind guard, to test failure one instruction before completion.
    ShutdownPortableRuntime();GC::Collect();
    Require(InitializePortableRuntime(packages).passed&&LoadPortableRuntimeMap(LoadPortablePackageTables(mapSource)).passed,
        "Could not recreate the exact owned Training fixture for late native expiry undo");
    prepare();load("TickTailLateExpiryBegin",0.125f);
    const auto replayBefore=temporary.Snapshot("TickTailLateExpiryBefore");
    const auto replayOwnership=ReadPortableRuntimeDeferredOwnership();const auto replayRevision=GetPortableRuntimeWorldRevision();
    GC::Collect();const auto replayGc=GC::GetStats();
    QuestVr::Vm::Limits last;Require(expiry.instructions>1u,"Original expiry ran no callback instructions");
    last.instructions=expiry.instructions-1u;
    const auto lastRefusal=AdvancePortableActorNonPawnTickTail(actor,0.125f,0,last);
    Require(!lastRefusal.passed()&&!lastRefusal.committed&&lastRefusal.status==QuestVr::Vm::Status::Budget&&
        lastRefusal.instructions==last.instructions&&lastRefusal.writes>lateRefusal.writes&&
        temporary.Snapshot("TickTailLastInstructionRefused")==replayBefore&&
        GetPortableRuntimeWorldRevision()==replayRevision&&ReadPortableRuntimeDeferredOwnership()==replayOwnership&&Published(actor),
        "Last-instruction native expiry refusal leaked countdown, contents birth/deletion, native ownership, RNG or map publication: "+lastRefusal.error);
    ++refusals;GC::Collect();const auto replayAfter=GC::GetStats();
    Require(replayGc.numObjects==replayAfter.numObjects&&replayGc.memoryUsage==replayAfter.memoryUsage,
        "Last-instruction original expiry refusal retained native/deleted actor objects");
    std::cout<<"ORIGINAL actual-crate Tick tail: committed state/timer/countdown, Expired/native destruction, save/reload and early/late rollback; "
        <<step.instructions<<"/"<<expiry.instructions<<" script instructions. Countdown saves, deferred-deletion capture explicitly refuses; "
        <<"explicit saved preconditions, NOT full Tick/startup/live gravity.\n";
}
void SourceContracts(const PortablePackageTables& engine, const PortablePackageTables& deusex) {
    const auto destroy = LoadPortableFunctionScript(engine, FindPortableExport(engine, "Actor.Destroy"));
    Require(destroy.nativeIndex == 279u && (destroy.functionFlags & 0x400u),
        "Original Destroy is not native279");
    const auto callback = LoadPortableFunctionScript(deusex, FindPortableExport(deusex, "Containers.Destroyed"));
    Require(callback.bytecode.size() == 919u && callback.bytecode.at(60u) == 0x62u &&
        callback.bytecode.at(61u) == 0x24u && callback.bytecode.at(681u) == 0xfcu &&
        callback.bytecode.at(758u) == 0x61u && callback.bytecode.at(759u) == 0x16u &&
        callback.bytecode.at(911u) == 0x1cu,
        "Original Containers.Destroyed FastTrace/VRand/Spawn/super offsets changed");
    const auto decoration = LoadPortableFunctionScript(deusex, FindPortableExport(deusex, "DeusExDecoration.Destroyed"));
    Require(decoration.bytecode.size() == 176u && decoration.bytecode.at(94u) == 0x62u &&
        decoration.bytecode.at(95u) == 0xd0u, "Original super destruction GetPlayerPawn720 changed");
}
bool Original(const std::filesystem::path& root) {
    static constexpr const char* packageNames[] = {"Core", "Engine", "DeusEx", "Extension", "ConSys",
        "IpDrv", "IpServer", "UBrowser", "UWindow", "Editor", "Fire", "MPCharacters", "DeusExSounds",
        "DeusExItems", "DeusExCharacters", "DeusExDeco"};
    std::vector<std::filesystem::path> sources;
    for (const auto* name : packageNames) sources.push_back(root / "System" / (std::string(name) + ".u"));
    sources.push_back(root / "Maps" / "00_Training.dx");
    for (const auto& path : sources) if (!std::filesystem::is_regular_file(path)) {
        std::cout << "SKIP original crate destruction: missing owned package " << path.string() << '\n'; return false;
    }
    struct Source { Bytes bytes; std::filesystem::file_time_type modified; };
    std::map<std::filesystem::path, Source> originals;
    std::vector<PortablePackageTables> packages;
    for (std::size_t i = 0u; i < sources.size(); ++i) {
        originals.emplace(sources[i], Source{ReadBytes(sources[i]), std::filesystem::last_write_time(sources[i])});
        if (i + 1u < sources.size()) packages.push_back(LoadPortablePackageTables(sources[i].string()));
    }
    const auto ini=root/"System"/"DeusEx.ini";
    Require(std::filesystem::is_regular_file(ini),"Original Tick fixture requires the owned explicit DeusEx.ini");
    originals.emplace(ini,Source{ReadBytes(ini),std::filesystem::last_write_time(ini)});
    SourceContracts(packages.at(1u), packages.at(2u));
    Require(InitializePortableRuntime(packages).passed &&
        LoadPortableRuntimeMap(LoadPortablePackageTables(sources.back().string())).passed,
        "Could not load actual Training crate runtime");
    Temporary temporary(root);
    const auto baseline = temporary.Snapshot("AuthoredBaseline");
    const std::string first = "00_Training.CrateBreakableMedGeneral1";
    const auto levelValue = ReadPortableActorScriptProperty(first, "Level");
    Require(levelValue.kind == Kind::Object && !levelValue.text.empty(), "Original crate has no LevelInfo");
    const auto world = GetPortableRuntimeMapActors(true);
    const auto level = std::find_if(world.begin(), world.end(), [&](const auto& actor) { return actor.objectPath == levelValue.text; });
    Require(level != world.end(), "Original crate's LevelInfo is not world-published");
    for (const auto* leaf : {"CrateBreakableMedGeneral1", "CrateBreakableMedGeneral2"}) {
        const std::string actor = "00_Training." + std::string(leaf);
        Require(ReadPortableActorScriptProperty(actor, "contents").text == "DeusEx.Lockpick" &&
            ReadPortableActorScriptProperty(actor, "content2").text.empty() &&
            ReadPortableActorScriptProperty(actor, "content3").text.empty() &&
            ReadPortableActorScriptProperty(actor, "numThings").integer == 1,
            "Original Training crate contents differ from Lockpick x1");
        Require(!QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor, "bGenerateTrash")) &&
            !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor, "bPushSoundPlaying")) &&
            ReadPortableActorScriptProperty(actor, "flyGen").text.empty(),
            "Original crate does not take the audited no-trash/no-push/no-flies branches");
        const auto context = ReadPortableActorDispatchContext(actor);
        Require(ResolvePortableActorFunction(actor, context.stateName, "Destroyed") ==
            std::optional<std::string>("DeusEx.Containers.Destroyed") &&
            QuestVr::ScriptDispatch::IsEnabled("Destroyed", context.classProbeMask, context.codeMasks,
                context.disabledNames, context.liveProbeMask),
            "Actual crate Destroyed dispatch is suppressed or substituted");
        LoadFixture(temporary, "Dormant", baseline, level->objectPath, level->classPath, false, true);
        const auto dormant = ExecutePortableActorFunction(actor, "Destroy");
        Require(dormant.passed() && dormant.committed && QuestVr::Vm::ToBool(dormant.value) && !Published(actor),
            "Actual dormant Destroy279 failed or retained publication: " + dormant.error);
        Require(Decode(temporary.Snapshot("DormantAfter")).births.empty(),
            "Dormant Destroy executed the contents callback");
        LoadFixture(temporary, "BegunStartup", baseline, level->objectPath, level->classPath, true, true);
        const auto begun = ExecutePortableActorFunction(actor, "Destroy");
        Require(begun.passed() && begun.committed && QuestVr::Vm::ToBool(begun.value) && !Published(actor) &&
            begun.instructions > dormant.instructions,
            "Actual begun Destroy279/Containers.Destroyed failed or callback was suppressed: " + begun.error +
                " at " + begun.function + ':' + std::to_string(begun.offset));
        Require(QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor, "bDeleteMe")) &&
            ReadPortableActorScriptProperty(actor, "HitPoints").integer == 10 &&
            Decode(temporary.Snapshot("BegunStartupAfter")).births.empty(),
            "Startup-suppressed destruction changed health or manufactured loot");
        const auto removed = temporary.Snapshot("RemovedBeforeRepeat");
        const auto removedRevision = GetPortableRuntimeWorldRevision();
        const auto repeated = ExecutePortableActorFunction(actor, "Destroy");
        Require(repeated.passed() && QuestVr::Vm::ToBool(repeated.value) &&
            temporary.Snapshot("RemovedAfterRepeat") == removed && GetPortableRuntimeWorldRevision() == removedRevision,
            "Repeated Destroy279 reran callback/effects or changed committed revision");
        LoadFixture(temporary, "BegunContents", baseline, level->objectPath, level->classPath, true, false);
        // All original content fields are retained, including Lockpick and
        // numThings. VRand252 now runs before Spawn278 in this real callback.
        // Inspect the actual birth/outcome, not merely the absence of a VM error.
        GC::Collect();
        const auto before = temporary.Snapshot("ContentsBefore");
        const auto revision = GetPortableRuntimeWorldRevision();
        const auto inventory=GetPortableRuntimeInventoryItems();
        Require(ReadPortableActorScriptProperty(level->objectPath,"Game").text.empty(),
            "Isolated destruction fixture unexpectedly owns a started GameInfo");
        const auto contents = ExecutePortableActorFunction(actor, "Destroy");
        Require(contents.passed() && contents.committed && QuestVr::Vm::ToBool(contents.value) && !Published(actor) &&
            GetPortableRuntimeWorldRevision()==revision+1u,
            "Actual contents destruction did not complete/commit exactly once: " + contents.error +
                " at " + contents.function + ':' + std::to_string(contents.offset));
        const auto after=temporary.Snapshot("ContentsAfter");const auto state=Decode(after);
        Require(state.randomSeed && state.births.size()==1u && state.births.front().classPath=="DeusEx.Lockpick",
            "Actual original contents callback did not create one genuine Lockpick birth with shared RNG");
        const auto birth=state.births.front().path;
        const auto overlay=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& item) {return item.path==birth;});
        // This is an isolated begun fixture, not a started GameInfo. Actual
        // Actor.PreBeginPlay queries None Game.IsRelevant, gets false and
        // destroys the spawned item. Preserve/report that real lifecycle;
        // never toggle bGameRelevant or directly grant fabricated inventory.
        Require(!Published(birth) && QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(birth,"bDeleteMe")) &&
            overlay!=state.objects.end() && overlay->lifecycle && overlay->lifecycle->worldRemoved &&
            GetPortableRuntimeInventoryItems()==inventory,
            "Unstarted Game=None relevance path manufactured surviving or collected loot");
        Require(LoadPortableRuntimeState(temporary.Path("ContentsAfter").string()) &&
            temporary.Snapshot("ContentsRoundTrip")==after,
            "Crate removal/deleted original contents birth/RNG did not round-trip exactly");
        const auto repeatedContentsRevision=GetPortableRuntimeWorldRevision();
        const auto repeatedContents=ExecutePortableActorFunction(actor,"Destroy");
        Require(repeatedContents.passed() && QuestVr::Vm::ToBool(repeatedContents.value) &&
            temporary.Snapshot("ContentsAfterRepeat")==after && GetPortableRuntimeWorldRevision()==repeatedContentsRevision,
            "Repeated actual crate destruction duplicated a contents birth or consumed RNG");
        Require(LoadPortableRuntimeState(temporary.Path("ContentsBefore").string()) &&
            temporary.Snapshot("ContentsRestoredBefore")==before,"Could not restore full pre-birth destruction checkpoint");
        GC::Collect();
        const auto gcBefore=GC::GetStats();const auto rollbackRevision=GetPortableRuntimeWorldRevision();
        QuestVr::Vm::Limits limit;Require(contents.instructions>1u,"Contents callback had no script budget boundary");
        limit.instructions=contents.instructions-1u;
        const auto refused=ExecutePortableActorFunction(actor,"Destroy",{},limit);
        Require(!refused.passed() && !refused.committed && refused.status==QuestVr::Vm::Status::Budget &&
            refused.instructions==limit.instructions && temporary.Snapshot("ContentsAfterLateRefusal")==before &&
            GetPortableRuntimeWorldRevision()==rollbackRevision && Published(actor) &&
            !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor,"bDeleteMe")),
            "Late contents callback budget failure retained birth/deletion/RNG/state/publication effects");
        GC::Collect();const auto gcAfter=GC::GetStats();
        Require(gcAfter.numObjects == gcBefore.numObjects && gcAfter.memoryUsage == gcBefore.memoryUsage,
            "Refused late destruction leaked original contents birth in the live GC graph");
        ++refusals;
        std::cout << "ORIGINAL " << actor << ": genuine Destroy279 dispatch passes FastTrace548 and super cleanup "
                  << "with explicit begun/startup fixture (" << begun.instructions << " vs dormant " << dormant.instructions
                  << " instructions); Contents=Lockpick creates "<<birth<<" then real Game=None relevance deletes it; "
                  <<contents.instructions<<" instructions, exact deleted-birth save/load/repeat and late rollback. NOT surviving loot/startup.\n";
        Require(LoadPortableRuntimeState(temporary.Path("AuthoredBaseline").string()) &&
            temporary.Snapshot("RestoredAuthoredBaseline") == baseline,
            "Original crate diagnostic did not restore complete authored baseline");
    }
    TickTail(temporary,baseline,level->objectPath,level->classPath,ini.string(),packages,sources.back().string());
    ShutdownPortableRuntime();
    for (const auto& [path, source] : originals)
        Require(ReadBytes(path) == source.bytes && std::filesystem::last_write_time(path) == source.modified,
            "Crate diagnostic changed original source bytes or timestamp: " + path.string());
    return true;
}
} // namespace
int main(const int argc, char** argv) {
    try {
        if (argc > 2) throw std::runtime_error("Usage: portable_crate_destruction_test [original-game-root]");
        if (argc != 2) {
            std::cout << "SKIP owned-original crate destruction diagnostic: supply original-game-root\n"; return 77;
        }
        GC::Collect();
        const auto before = GC::GetStats();
        if (!Original(argv[1])) return 77;
        ShutdownPortableRuntime(); GC::Collect();
        const auto after = GC::GetStats();
        Require(after.numObjects == before.numObjects && after.memoryUsage == before.memoryUsage,
            "Crate destruction diagnostic retained rooted runtime objects");
        std::cout << "PASS owned original native279 crate destruction: " << checks << " checks, " << refusals
                  << " genuine contents refusals. No manual Destroyed dispatch, DLL/EXE execution, world startup, "
                     "live melee destruction, spawned loot or gravity claim.\n";
        return 0;
    } catch (const std::exception& error) {
        ShutdownPortableRuntime();
        std::cerr << "Crate destruction diagnostic failed: " << error.what() << '\n'; return 1;
    }
}
