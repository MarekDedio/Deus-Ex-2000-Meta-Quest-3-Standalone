#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Generated serialized packages exercise the real runtime, not a replacement
// ConsoleCommand host. Optional owned packages are read only. No original
// Windows executable or DLL is loaded/executed; no original world is started.
namespace {
using Bytes = std::vector<std::uint8_t>;
using QuestVr::Vm::Evaluation;
using QuestVr::Vm::Kind;
using QuestVr::Vm::Status;
using QuestVr::Vm::Value;
std::size_t checks{}, refusals{};
void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void U16(Bytes& bytes, const std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
}
void U32(Bytes& bytes, const std::uint32_t value) {
    U16(bytes, static_cast<std::uint16_t>(value)); U16(bytes, static_cast<std::uint16_t>(value >> 16u));
}
void U64(Bytes& bytes, const std::uint64_t value) {
    U32(bytes, static_cast<std::uint32_t>(value)); U32(bytes, static_cast<std::uint32_t>(value >> 32u));
}
void Index(Bytes& bytes, const std::int32_t value) {
    auto rest = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((rest & 63u) | (value < 0 ? 128u : 0u));
    rest >>= 6u;
    if (rest) first |= 64u;
    bytes.push_back(first);
    while (rest) {
        auto next = static_cast<std::uint8_t>(rest & 127u); rest >>= 7u;
        if (rest) next |= 128u;
        bytes.push_back(next);
    }
}
void Replace32(Bytes& bytes, const std::size_t offset, const std::uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8u * i));
}
void Append(Bytes& bytes, const Bytes& tail) { bytes.insert(bytes.end(), tail.begin(), tail.end()); }
struct Code { Bytes raw; std::size_t logical{}; };
Code Token(const std::uint8_t op) { return {{op}, 1u}; }
Code Ref(const std::uint8_t op, const std::int32_t reference) {
    Code code{{op}, 5u}; Index(code.raw, reference); return code;
}
Code Int(const std::int32_t value) {
    Code code{{0x1du}, 5u}; U32(code.raw, static_cast<std::uint32_t>(value)); return code;
}
Code Text(const std::string& value) {
    Code code{{0x1fu}, value.size() + 2u}; code.raw.insert(code.raw.end(), value.begin(), value.end());
    code.raw.push_back(0u); return code;
}
Code Join(const std::initializer_list<Code> parts) {
    Code code; for (const auto& part : parts) { Append(code.raw, part.raw); code.logical += part.logical; }
    return code;
}
Code Return(Code value = Token(0x0bu)) { return Join({Token(0x04u), std::move(value)}); }
Code Assign(Code left, Code right) { return Join({Token(0x0fu), std::move(left), std::move(right)}); }
Code Final(const std::int32_t function, const std::initializer_list<Code> arguments = {}) {
    auto code = Ref(0x1cu, function);
    for (const auto& argument : arguments) { Append(code.raw, argument.raw); code.logical += argument.logical; }
    return Join({std::move(code), Token(0x16u)});
}
Code Native(const std::uint16_t index, const std::initializer_list<Code> arguments = {}) {
    Code code;
    if (index >= 256u) { code.raw = {static_cast<std::uint8_t>(0x60u + (index >> 8u)), static_cast<std::uint8_t>(index)}; code.logical = 2u; }
    else { code.raw = {static_cast<std::uint8_t>(index)}; code.logical = 1u; }
    for (const auto& argument : arguments) { Append(code.raw, argument.raw); code.logical += argument.logical; }
    return Join({std::move(code), Token(0x16u)});
}
Code ClassContext(Code selector, Code expression) {
    auto code = Join({Token(0x12u), std::move(selector)});
    Require(expression.logical <= 65535u, "Console fixture ClassContext skip exceeds bound");
    U16(code.raw, static_cast<std::uint16_t>(expression.logical)); code.raw.push_back(0u); code.logical += 3u;
    return Join({std::move(code), std::move(expression)});
}

struct Package {
    std::string stem;
    std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
    std::vector<std::function<Bytes(std::int32_t, std::int32_t)>> builders;
    std::map<std::int32_t, std::vector<std::int32_t>> children;
    std::int32_t Name(const std::string& name) {
        const auto found = std::find(names.begin(), names.end(), name);
        if (found != names.end()) return static_cast<std::int32_t>(found - names.begin());
        names.push_back(name); return static_cast<std::int32_t>(names.size() - 1u);
    }
    std::int32_t Import(const std::string& name, const std::int32_t outer, const std::string& type = "Class") {
        imports.push_back({Name("Core"), Name(type), outer, Name(name)});
        return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) { return Import(name, 0, "Package"); }
    std::int32_t Export(const std::string& name, const std::int32_t cls = 0,
        const std::int32_t base = 0, const std::int32_t outer = 0, const bool field = false) {
        exports.push_back({cls, base, outer, Name(name), ObjectFlags{}, 0, -1}); builders.emplace_back();
        const auto ref = static_cast<std::int32_t>(exports.size());
        if (field) children[outer].push_back(ref);
        return ref;
    }
    void ClassBody(const std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref - 1)) = [this, ref](const std::int32_t next, const std::int32_t child) {
            Bytes body; const auto& entry = exports.at(static_cast<std::size_t>(ref - 1));
            Index(body, entry.ObjBase); Index(body, next); Index(body, 0); Index(body, child); Index(body, entry.ObjName);
            U32(body, 0); U32(body, 0); U32(body, 0); U64(body, ~std::uint64_t{}); U64(body, ~std::uint64_t{});
            U16(body, 0xffffu); U32(body, 0); U32(body, 0); body.resize(body.size() + 16u, 0u);
            for (unsigned i = 0u; i < 5u; ++i) Index(body, 0);
            return body;
        };
    }
    std::int32_t Property(const std::string& name, const std::string& type, const std::int32_t owner,
        const std::int32_t core, const std::int32_t target = 0, const std::uint32_t flags = 0u,
        const std::uint32_t dimension = 1u) {
        const auto ref = Export(name, Import(type, core), 0, owner, true);
        builders.at(static_cast<std::size_t>(ref - 1)) = [type, target, flags, dimension](const std::int32_t next, std::int32_t) {
            Bytes body{0u}; Index(body, 0); Index(body, next); U32(body, dimension); U32(body, flags); Index(body, 0);
            if (type == "ObjectProperty" || type == "ByteProperty") Index(body, target);
            else if (type == "ClassProperty") { Index(body, 0); Index(body, target); }
            return body;
        }; return ref;
    }
    std::int32_t Function(const std::string& name, const std::int32_t owner, const std::int32_t core,
        Code code = {}, const std::uint16_t native = 0u, const std::uint32_t flags = 2u) {
        const auto ref = Export(name, Import("Function", core), 0, owner, true);
        builders.at(static_cast<std::size_t>(ref - 1)) = [this, ref, native, flags, code = std::move(code)](const std::int32_t next, const std::int32_t child) {
            Bytes body{0u}; Index(body, 0); Index(body, next); Index(body, 0); Index(body, child);
            Index(body, exports.at(static_cast<std::size_t>(ref - 1)).ObjName); U32(body, 0); U32(body, 0);
            U32(body, static_cast<std::uint32_t>(code.logical)); Append(body, code.raw);
            U16(body, native); body.push_back(0u); U32(body, flags); return body;
        }; return ref;
    }
    void Body(const std::int32_t ref, Bytes body) {
        builders.at(static_cast<std::size_t>(ref - 1)) = [body = std::move(body)](std::int32_t, std::int32_t) { return body; };
    }
    void ObjectTag(Bytes& bytes, const std::string& name, const std::int32_t value) {
        Bytes payload; Index(payload, value); Index(bytes, Name(name)); bytes.push_back(0x55u);
        bytes.push_back(static_cast<std::uint8_t>(payload.size())); Append(bytes, payload);
    }
    void BoolTag(Bytes& bytes, const std::string& name, const bool value) {
        Index(bytes, Name(name)); bytes.push_back(static_cast<std::uint8_t>(3u | (value ? 0x80u : 0u)));
    }
    void ActorBody(const std::int32_t ref, Bytes properties = {}) { Index(properties, 0); Body(ref, std::move(properties)); }
    Bytes Serialize() {
        std::map<std::int32_t, std::int32_t> next;
        for (const auto& [owner, list] : children) {
            (void)owner; for (std::size_t i = 1u; i < list.size(); ++i) next[list[i - 1u]] = list[i];
        }
        Bytes bytes; U32(bytes, 0x9e2a83c1u); U16(bytes, 68u); U16(bytes, 0u); bytes.resize(56u, 0u);
        std::vector<Bytes> bodies; std::vector<std::int32_t> offsets;
        for (std::size_t i = 0u; i < exports.size(); ++i) {
            const auto ref = static_cast<std::int32_t>(i + 1u); const auto found = children.find(ref);
            auto body = builders[i] ? builders[i](next[ref], found == children.end() || found->second.empty() ? 0 : found->second.front()) : Bytes{};
            offsets.push_back(static_cast<std::int32_t>(bytes.size())); Append(bytes, body); bodies.push_back(std::move(body));
        }
        Replace32(bytes, 12u, static_cast<std::uint32_t>(names.size())); Replace32(bytes, 16u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& name : names) {
            Index(bytes, static_cast<std::int32_t>(name.size() + 1u)); bytes.insert(bytes.end(), name.begin(), name.end());
            bytes.push_back(0u); U32(bytes, 0);
        }
        Replace32(bytes, 20u, static_cast<std::uint32_t>(exports.size())); Replace32(bytes, 24u, static_cast<std::uint32_t>(bytes.size()));
        for (std::size_t i = 0u; i < exports.size(); ++i) {
            const auto& entry = exports[i]; Index(bytes, entry.ObjClass); Index(bytes, entry.ObjBase);
            U32(bytes, static_cast<std::uint32_t>(entry.ObjOuter)); Index(bytes, entry.ObjName);
            U32(bytes, static_cast<std::uint32_t>(entry.ObjFlags)); Index(bytes, static_cast<std::int32_t>(bodies[i].size()));
            if (!bodies[i].empty()) Index(bytes, offsets[i]);
        }
        Replace32(bytes, 28u, static_cast<std::uint32_t>(imports.size())); Replace32(bytes, 32u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& entry : imports) {
            Index(bytes, entry.ClassPackage); Index(bytes, entry.ClassName); U32(bytes, static_cast<std::uint32_t>(entry.ObjOuter)); Index(bytes, entry.ObjName);
        }
        return bytes;
    }
};

Bytes ReadBytes(const std::filesystem::path& path) {
    Bytes bytes; Require(QuestVr::ReadBoundedSaveFile(path.string(), QuestVr::kMaximumSaveRuntimeBytes, bytes),
        "Cannot inspect bounded console fixture/checkpoint"); return bytes;
}
struct Fixture {
    std::filesystem::path parent, directory;
    std::map<std::filesystem::path, Bytes> sources;
    Fixture() {
        parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0u; attempt < 20u; ++attempt) {
            const auto candidate = parent / ("deusex-player-console-test-" + std::to_string(stamp) + '-' + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) { directory = std::filesystem::canonical(candidate); break; }
        }
        Require(!directory.empty() && directory.parent_path() == parent, "Console fixture escaped its owned temporary parent");
        std::filesystem::create_directory(directory / "System"); std::filesystem::create_directory(directory / "Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime(); std::error_code error;
        const auto actual = std::filesystem::weakly_canonical(directory, error);
        if (!error && !directory.empty() && actual == directory && actual.parent_path() == parent &&
            actual.filename().string().rfind("deusex-player-console-test-", 0u) == 0u) std::filesystem::remove_all(actual, error);
    }
    PortablePackageTables Write(Package& package, const bool map = false) {
        const auto path = directory / (map ? "Maps" : "System") / (package.stem + (map ? ".dx" : ".u"));
        const auto bytes = package.Serialize();
        Require(QuestVr::WriteDurableSaveFile(path.string(), bytes), "Cannot write generated console package");
        sources.emplace(path, bytes); return LoadPortablePackageTables(path.string());
    }
    Bytes Snapshot(const std::string& label) const {
        const auto path = directory / (label + ".sav");
        Require(SavePortableRuntimeState(path.string()), "Cannot capture console runtime checkpoint: " + label);
        return ReadBytes(path);
    }
    void SourcesUnchanged() const {
        for (const auto& [path, bytes] : sources) Require(ReadBytes(path) == bytes, "Console control changed a source package");
    }
};
Evaluation String(const std::string& text) { return {Value::Text(Kind::String, text), {}}; }
QuestVr::Vm::Result Call(const std::string& receiver, const std::string& function,
    const std::vector<Evaluation>& arguments = {}) {
    const auto result = ExecutePortableActorFunction(receiver, function, arguments);
    Require(result.passed() && result.committed, function + " failed: " + result.error + " at " + result.function);
    return result;
}
QuestVr::Vm::Result Refusal(const Fixture& fixture, const std::string& label,
    const std::function<QuestVr::Vm::Result()>& action) {
    const auto before = fixture.Snapshot("BeforeRefusal");
    const auto revision = GetPortableRuntimeWorldRevision(); const auto stats = GC::GetStats();
    const auto actors = GetPortableRuntimeMapActors(true).size();
    const auto result = action();
    Require(!result.passed() && !result.committed, "Unsupported console operation committed: " + label);
    Require(GetPortableRuntimeWorldRevision() == revision && GetPortableRuntimeMapActors(true).size() == actors &&
        GC::GetStats().numObjects == stats.numObjects && GC::GetStats().memoryUsage == stats.memoryUsage &&
        fixture.Snapshot("AfterRefusal") == before, "Console refusal changed save/revision/actors/GC: " + label);
    ++refusals; return result;
}
void EmptyUnchanged(const Fixture& fixture, const std::string& receiver, const std::string& function,
    const std::vector<Evaluation>& arguments) {
    const auto before = fixture.Snapshot("BeforeConsole"); const auto revision = GetPortableRuntimeWorldRevision();
    const auto stats = GC::GetStats(); const auto actors = GetPortableRuntimeMapActors(true).size();
    const auto result = Call(receiver, function, arguments);
    Require(result.value.kind == Kind::String && result.value.text.empty() && result.writes == 0u,
        "Unbound PlayerPawn.ConsoleCommand fabricated output/effects");
    Require(GetPortableRuntimeWorldRevision() == revision && GetPortableRuntimeMapActors(true).size() == actors &&
        GC::GetStats().numObjects == stats.numObjects && GC::GetStats().memoryUsage == stats.memoryUsage &&
        fixture.Snapshot("AfterConsole") == before, "Unbound PlayerPawn.ConsoleCommand mutated save/revision/actors/GC");
}
struct Tables { std::vector<PortablePackageTables> packages; PortablePackageTables map; };
Tables Build(Fixture& fixture, const unsigned malformed = 0u) {
    Package core; core.stem = "Core"; const auto cp = core.ImportPackage("Core"), object = core.Export("Object"), meta = core.Export("Class", 0, object);
    core.Property("Class", "ClassProperty", object, cp, object, 2u); core.Property("Name", "NameProperty", object, cp, 0, 2u);
    core.Property("Outer", "ObjectProperty", object, cp, object, 2u); core.Property("ObjectFlags", "IntProperty", object, cp, 0, 2u);
    core.ClassBody(object); core.ClassBody(meta); const auto coreTable = fixture.Write(core);
    Package engine; engine.stem = "Engine"; const auto ec = engine.ImportPackage("Core"), eo = engine.Import("Object", ec);
    const auto actor = engine.Export("Actor", 0, eo), pawn = engine.Export("Pawn", 0, actor), playerPawn = engine.Export("PlayerPawn", 0, pawn);
    const auto player = engine.Export("Player", 0, eo), levelInfo = engine.Export("LevelInfo", 0, actor);
    const auto level = engine.Export("Level", 0, eo), model = engine.Export("Model", 0, eo);
    engine.Property("Level", "ObjectProperty", actor, ec, levelInfo); engine.Property("XLevel", "ObjectProperty", actor, ec, level);
    engine.Property("bDeleteMe", "BoolProperty", actor, ec);
    const auto begun = engine.Property("bBegunPlay", "BoolProperty", levelInfo, ec);
    (void)begun;
    engine.Property("Actor", "ObjectProperty", player, ec, playerPawn, 2u);
    engine.Property("Player", malformed == 6u ? "IntProperty" : "ObjectProperty", playerPawn, ec,
        malformed == 7u ? actor : player, 2u, malformed == 8u ? 2u : 1u);
    const auto console = engine.Function("ConsoleCommand", playerPawn, ec, {}, malformed == 4u ? 999u : 0u,
        malformed == 5u ? 0x401u : 0x400u);
    engine.Property(malformed == 9u ? "WrongCommand" : "Command", malformed == 1u ? "IntProperty" : "StrProperty",
        console, ec, 0, malformed == 2u ? 0x80u : 0x400080u);
    engine.Property("ReturnValue", "StrProperty", console, ec, 0, malformed == 3u ? 0x580u : 0x400580u);
    const auto actorConsole = engine.Function("ConsoleCommand", actor, ec, {}, 0u, 0x400u);
    engine.Property("Command", "StrProperty", actorConsole, ec, 0, 0x400080u);
    engine.Property("ReturnValue", "StrProperty", actorConsole, ec, 0, 0x400580u);
    engine.Function("SaveConfig", actor, ec, {}, 0u, 0x400u);
    const auto getPlayer = engine.Function("GetPlayerPawn", actor, ec, {}, 720u, 0x401u);
    engine.Property("ReturnValue", "ObjectProperty", getPlayer, ec, playerPawn, 0x480u);
    engine.Function("Possess", playerPawn, ec, Return(), 0u, 0x800u);
    for (const auto ref : {actor, pawn, playerPawn, player, levelInfo, level, model}) engine.ClassBody(ref);
    const auto engineTable = fixture.Write(engine);
    Package controls; controls.stem = "ConsoleControls"; const auto cc = controls.ImportPackage("Core"), ce = controls.ImportPackage("Engine");
    const auto importedPawn = controls.Import("PlayerPawn", ce), importedPlayer = controls.Import("Player", ce);
    const auto driver = controls.Export("Driver", 0, importedPawn);
    const auto count = controls.Property("ArgumentCount", "IntProperty", driver, cc);
    const auto playerField = controls.Import("Player", importedPawn, "ObjectProperty");
    const auto importedConsole = controls.Import("ConsoleCommand", importedPawn, "Function");
    const auto argument = controls.Function("CommandArgument", driver, cc,
        Join({Assign(Ref(0x01u, count), Native(146u, {Ref(0x01u, count), Int(1)})), Return(Text("GETPING"))}));
    controls.Property("ReturnValue", "StrProperty", argument, cc, 0, 0x580u);
    const auto wrapper = controls.Function("EvaluatedCommand", driver, cc, Return(Final(importedConsole, {Final(argument)})));
    controls.Property("ReturnValue", "StrProperty", wrapper, cc, 0, 0x580u);
    controls.Function("EvaluatedCommandThenFail", driver, cc,
        Join({Final(importedConsole, {Final(argument)}), Native(999u), Return()}));
    controls.Function("DefaultConsole", driver, cc,
        Return(ClassContext(Ref(0x20u, importedPawn), Final(importedConsole, {Text("GETPING")}))));
    const auto setPlayer = controls.Function("SetPlayerReference", driver, cc);
    const auto input = controls.Property("Input", "ObjectProperty", setPlayer, cc, importedPlayer, 0x80u);
    controls.builders.at(static_cast<std::size_t>(setPlayer - 1)) = [&, setPlayer, input, playerField](const std::int32_t next, const std::int32_t child) {
        const auto code = Join({Assign(Ref(0x01u, playerField), Ref(0x00u, input)), Return()});
        Bytes body{0u}; Index(body, 0); Index(body, next); Index(body, 0); Index(body, child); Index(body, controls.exports.at(static_cast<std::size_t>(setPlayer - 1)).ObjName);
        U32(body, 0); U32(body, 0); U32(body, static_cast<std::uint32_t>(code.logical)); Append(body, code.raw); U16(body, 0u); body.push_back(0u); U32(body, 2u); return body;
    };
    controls.ClassBody(driver); const auto controlTable = fixture.Write(controls);
    Package map; map.stem = "ConsoleFixture"; const auto me = map.ImportPackage("Engine"), mc = map.ImportPackage("ConsoleControls");
    const auto world = map.Export("MyLevel", map.Import("Level", me)), worldModel = map.Export("Model0", map.Import("Model", me));
    const auto info = map.Export("Level0", map.Import("LevelInfo", me)), camera = map.Export("Camera5", map.Import("Driver", mc));
    const auto otherActor = map.Export("Actor0", map.Import("Actor", me)), playerObject = map.Export("PlayerObject", map.Import("Player", me));
    for (const auto ref : {info, camera, otherActor}) {
        Bytes tags; map.ObjectTag(tags, "Level", info); map.ObjectTag(tags, "XLevel", world);
        // This authored generated flag/empty Possess body is ONLY a native
        // pairing fixture. It does not bypass or claim owned-game startup.
        if (ref == info) map.BoolTag(tags, "bBegunPlay", true);
        map.ActorBody(ref, std::move(tags));
    }
    map.ActorBody(playerObject);
    Bytes worldBytes; Index(worldBytes, 0); U32(worldBytes, 3u); U32(worldBytes, 3u);
    for (const auto ref : {info, camera, otherActor}) Index(worldBytes, ref);
    for (unsigned i = 0u; i < 5u; ++i) Index(worldBytes, 0);
    U32(worldBytes, 7777u); U32(worldBytes, 0u); Index(worldBytes, worldModel); Index(worldBytes, 0);
    U32(worldBytes,0u); Index(worldBytes,0); // Original serialized Float time and preliminary +100.
    for(unsigned i=0u;i<16u;++i)Index(worldBytes,0); // +9C pointer sequence, including final +100 alias.
    map.Body(world, std::move(worldBytes));
    Bytes modelBytes; Index(modelBytes, 0); for (unsigned i = 0u; i < 6u; ++i) U32(modelBytes, 0u);
    modelBytes.push_back(0u); for (unsigned i = 0u; i < 4u; ++i) U32(modelBytes, 0u);
    for (unsigned i = 0u; i < 5u; ++i) Index(modelBytes, 0);
    U32(modelBytes, 0u); U32(modelBytes, 1u); Index(modelBytes, info); U64(modelBytes, 0u); U64(modelBytes, 0u);
    for (unsigned i = 0u; i < 7u; ++i) Index(modelBytes, 0);
    U32(modelBytes, 1u); U32(modelBytes, 0u); map.Body(worldModel, std::move(modelBytes));
    return {{coreTable, engineTable, controlTable}, fixture.Write(map, true)};
}
void Generated() {
    {
        Fixture fixture; const auto tables = Build(fixture);
        Require(InitializePortableRuntime(tables.packages).passed && LoadPortableRuntimeMap(tables.map).passed,
            "Generated console runtime/map initialization failed");
        const std::string camera = "ConsoleFixture.Camera5";
        const auto baseline = fixture.Snapshot("Baseline");
        Require(ReadPortableActorDeclaredScriptProperty(camera, "Engine.PlayerPawn.Player").text.empty(),
            "Generated unbound camera has a Player reference");
        const auto unboundPlayer = Call(camera, "GetPlayerPawn").value;
        Require(unboundPlayer.kind == Kind::Object && unboundPlayer.text.empty() &&
            fixture.Snapshot("AfterGetPlayer") == baseline, "Generated GetPlayerPawn fabricated a bound player or changed state");
        for (const auto* command : {"GETPING", "GETLOSS", "OPEN 01_NYC_UNATCOIsland", "SET Engine.Actor bHidden True", "", "arbitrary unknown command"})
            EmptyUnchanged(fixture, camera, "Engine.PlayerPawn.ConsoleCommand", {String(command)});
        EmptyUnchanged(fixture, camera, "ConsoleCommand", {String("GETPING")});
        Refusal(fixture, "actor-qualified native0", [&] { return ExecutePortableActorFunction(camera, "Engine.Actor.ConsoleCommand", {String("GETPING")}); });
        Refusal(fixture, "wrong Actor receiver", [&] { return ExecutePortableActorFunction("ConsoleFixture.Actor0", "Engine.PlayerPawn.ConsoleCommand", {String("GETPING")}); });
        Refusal(fixture, "different native0 declaration", [&] { return ExecutePortableActorFunction(camera, "Engine.Actor.SaveConfig"); });
        Refusal(fixture, "missing required command", [&] { return ExecutePortableActorFunction(camera, "ConsoleCommand"); });
        Refusal(fixture, "excess command", [&] { return ExecutePortableActorFunction(camera, "ConsoleCommand", {String("GETPING"), String("GETLOSS")}); });
        for (const auto& value : {Value::Integer(1), Value::Text(Kind::Name, "GETPING"), Value::Text(Kind::Object, {}), Value{}})
            Refusal(fixture, "non-String command", [&] { return ExecutePortableActorFunction(camera, "ConsoleCommand", {{value, {}}}); });
        Refusal(fixture, "class-default receiver", [&] { return ExecutePortableActorFunction(camera, "DefaultConsole"); });
        Require(fixture.Snapshot("AfterDirectControls") == baseline, "Direct console controls changed baseline");
        const auto evaluated = Call(camera, "EvaluatedCommand");
        Require(evaluated.value.kind == Kind::String && evaluated.value.text.empty() &&
            ReadPortableActorScriptProperty(camera, "ArgumentCount").integer == 1,
            "PlayerNone guard skipped original command-expression effects");
        Refusal(fixture, "later failure after evaluated command", [&] { return ExecutePortableActorFunction(camera, "EvaluatedCommandThenFail"); });
        Require(ReadPortableActorScriptProperty(camera, "ArgumentCount").integer == 1,
            "Console later failure retained nested argument effects");
        Call(camera, "SetPlayerReference", {{Value::Text(Kind::Object, "ConsoleFixture.PlayerObject"), {}}});
        const auto bound = Refusal(fixture, "nonnull declared Player requires real FExec", [&] {
            return ExecutePortableActorFunction(camera, "ConsoleCommand", {String("GETPING")});
        });
        Require(bound.status == Status::Unsupported, "Bound-player console path claimed support");
        Call(camera, "SetPlayerReference", {{Value::Text(Kind::Object, {}), {}}});
        auto lifetime = std::make_shared<QuestVr::NativeClientLifetime>(); BindPortableRuntimeClient(lifetime, 51u);
        RegisterPortableRuntimeViewport(51u, {52u, 1440u, 1440u, 3u, 2u});
        PreparePortableRuntimePlayerObjects(52u);
        const auto paired = SetPortableRuntimePlayer(52u, camera);
        Require(paired.passed() && paired.committed && ReadPortableRuntimePlayerSession(52u).bound,
            "Generated original SetPlayer pairing failed before console bound/stale controls: " + paired.error);
        Refusal(fixture, "native-bound Player requires real FExec", [&] {
            return ExecutePortableActorFunction(camera, "ConsoleCommand", {String("GETPING")});
        });
        lifetime->Revoke();
        const auto stale = Refusal(fixture, "revoked native Player is not PlayerNone", [&] {
            return ExecutePortableActorFunction(camera, "ConsoleCommand", {String("GETPING")});
        });
        Require(stale.status == Status::Unsupported, "Revoked native Player masqueraded as unbound support");
        fixture.SourcesUnchanged();
    }
    for (unsigned malformed = 1u; malformed <= 9u; ++malformed) {
        Fixture fixture; const auto tables = Build(fixture, malformed);
        Require(InitializePortableRuntime(tables.packages).passed && LoadPortableRuntimeMap(tables.map).passed,
            "Malformed-console fixture failed before native schema control");
        Refusal(fixture, "malformed declaration/Player schema " + std::to_string(malformed), [&] {
            return ExecutePortableActorFunction("ConsoleFixture.Camera5", "Engine.PlayerPawn.ConsoleCommand", {String("GETPING")});
        });
        fixture.SourcesUnchanged();
    }
    std::cout << "GENERATED PlayerPawn.ConsoleCommand PlayerNone/schema/native0/argument/native-bound/revoked controls passed\n";
}
void Owned(const std::filesystem::path& root) {
    Fixture fixture;
    std::vector<PortablePackageTables> scripts;
    for (const auto* name : {"ConSys", "Core", "DeusEx", "DeusExCharacters", "DeusExConAudioAIBarks",
        "DeusExConAudioEndGame", "DeusExConAudioHK_Shared", "DeusExConAudioIntro", "DeusExConAudioMission00",
        "DeusExConAudioMission01", "DeusExConAudioMission02", "DeusExConAudioMission03", "DeusExConAudioMission04",
        "DeusExConAudioMission05", "DeusExConAudioMission08", "DeusExConAudioMission09", "DeusExConAudioMission10",
        "DeusExConAudioMission11", "DeusExConAudioMission12", "DeusExConAudioMission14", "DeusExConAudioMission15",
        "DeusExConAudioNYShared", "DeusExConText", "DeusExConversations", "DeusExDeco", "DeusExItems", "DeusExSounds",
        "DeusExText", "DeusExUI", "Editor", "Engine", "Extension", "Fire", "IpDrv", "IpServer", "MPCharacters", "UBrowser", "UWindow"})
        scripts.push_back(LoadPortablePackageTables((root / "System" / (std::string(name) + ".u")).string()));
    const auto engine = std::find_if(scripts.begin(), scripts.end(), [](const auto& table) {
        return std::filesystem::path(table.sourcePath).stem().string() == "Engine";
    });
    Require(engine != scripts.end(), "Original console control lacks Engine metadata");
    const auto declaration = LoadPortableFunctionScript(*engine, FindPortableExport(*engine, "PlayerPawn.ConsoleCommand"));
    const auto command = LoadPortablePropertyDescriptor(*engine, FindPortableExport(*engine, "PlayerPawn.ConsoleCommand.Command"));
    const auto returned = LoadPortablePropertyDescriptor(*engine, FindPortableExport(*engine, "PlayerPawn.ConsoleCommand.ReturnValue"));
    const auto player = LoadPortablePropertyDescriptor(*engine, FindPortableExport(*engine, "PlayerPawn.Player"));
    Require(declaration.nativeIndex == 0u && declaration.functionFlags == 0x400u &&
        command.type == "StrProperty" && command.arrayDimension == 1 && command.flags == 0x400080u &&
        returned.type == "StrProperty" && returned.arrayDimension == 1 && returned.flags == 0x400580u &&
        player.type == "ObjectProperty" && player.arrayDimension == 1 && GetPortableObjectPath(*engine, player.referencedType) == "Player",
        "Original PlayerPawn.ConsoleCommand or Player schema differs from audited GOTY declaration");
    Require(InitializePortableRuntime(scripts).passed, "Original console runtime initialization failed");
    const auto map = LoadPortablePackageTables((root / "Maps" / "00_Training.dx").string());
    Require(LoadPortableRuntimeMap(map).passed, "Original console Training load failed");
    const auto actors = GetPortableRuntimeMapActors(true);
    const auto camera = std::find_if(actors.begin(), actors.end(), [](const auto& actor) { return actor.objectPath == "00_Training.Camera5"; });
    Require(camera != actors.end(), "Original Training Camera5 is not an actual indexed actor");
    const auto playerValue = ReadPortableActorDeclaredScriptProperty(camera->objectPath, "Engine.PlayerPawn.Player");
    Require(playerValue.kind == Kind::Object && playerValue.text.empty() && !HasPortableRuntimeClient(),
        "Original dormant camera is not genuinely PlayerNone/unbound");
    const auto baseline = fixture.Snapshot("OwnedBaseline");
    const auto getPlayer = Call(camera->objectPath, "GetPlayerPawn").value;
    Require(getPlayer.kind == Kind::Object && getPlayer.text.empty() && fixture.Snapshot("OwnedAfterGetPlayer") == baseline,
        "Original unbound GetPlayerPawn returned a camera/PlayerStart or changed state");
    for (const auto* text : {"GETPING", "GETLOSS", "OPEN 01_NYC_UNATCOIsland", "arbitrary unknown command", ""})
        EmptyUnchanged(fixture, camera->objectPath, "Engine.PlayerPawn.ConsoleCommand", {String(text)});
    EmptyUnchanged(fixture, camera->objectPath, "ConsoleCommand", {String("GETPING")});
    Refusal(fixture, "original Actor.ConsoleCommand is different native0", [&] {
        return ExecutePortableActorFunction(camera->objectPath, "Engine.Actor.ConsoleCommand", {String("GETPING")});
    });
    Require(fixture.Snapshot("OwnedAfter") == baseline &&
        ReadPortableActorDeclaredScriptProperty(camera->objectPath, "Engine.PlayerPawn.Player").text.empty() &&
        !GetPortableRuntimeScriptStatePresent(), "Original console checks started/mutated camera/world/player state");
    const auto stats = GC::GetStats(); GC::Collect();
    Require(GC::GetStats().numObjects == stats.numObjects && GC::GetStats().memoryUsage == stats.memoryUsage,
        "Original console checks retained unrooted UObject garbage");
    std::cout << "ORIGINAL38package Training Camera5 PlayerNone ConsoleCommand GETPING/GETLOSS/arbitrary Stringempty; Actor native0 distinct; save/revision/GC unchanged; NOT real FExec/world/player completion\n";
}
} // namespace

int main(const int argc, char** argv) {
    try {
        if (argc == 1) Generated();
        else if (argc == 2 && std::string(argv[1]) == "--owned") {
            std::cout << "SKIP: supply --owned GAME_ROOT for original PlayerPawn.ConsoleCommand controls\n"; return 77;
        } else if (argc == 3 && std::string(argv[1]) == "--owned") Owned(std::filesystem::path(argv[2]));
        else throw std::runtime_error("Usage: portable_player_console_test [--owned [GAME_ROOT]]");
        std::cout << "PlayerPawn.ConsoleCommand: " << checks << " checks, " << refusals << " refusals passed\n";
        return 0;
    } catch (const std::exception& error) {
        ShutdownPortableRuntime(); std::cerr << "PlayerPawn.ConsoleCommand control failed: " << error.what() << '\n'; return 1;
    }
}
