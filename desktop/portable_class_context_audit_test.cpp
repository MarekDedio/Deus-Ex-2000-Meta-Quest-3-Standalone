#include "Precomp.h"
#include "quest_portable_vm.h"
#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Read-only structural audit of the owned GOTY packages, not VM execution.
// No runtime initialization, original EXE/DLL loading, temporary files, saves,
// native calls, actor startup or class-default mutation occurs in this test.
// Reference corpus SHA256 (provenance, not a claim that this test hashes files):
// Engine.u:    1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA
// DeusEx.u:    257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480
// Extension.u: 9984FAEDA55FB2100B1A15A37BFEB8CDB4878248B39AFF832002A799F1D787F7
namespace {
namespace Vm = QuestVr::Vm;
using Bytes = std::vector<std::uint8_t>;
using Counts = std::map<unsigned, std::size_t>;
std::size_t checks{};
constexpr std::size_t noParent = std::numeric_limits<std::size_t>::max();

void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
Bytes ReadSource(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    Require(size <= 64u * 1024u * 1024u, "Original audit source exceeds 64 MiB bound");
    Bytes bytes(static_cast<std::size_t>(size));
    std::ifstream stream(path, std::ios::binary);
    Require(static_cast<bool>(stream), "Cannot open read-only audit source: " + path.string());
    if (!bytes.empty()) stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(stream), "Original audit source is truncated: " + path.string());
    return bytes;
}
std::string Qualified(const PortablePackageTables& package, const std::int32_t reference) {
    const auto path = GetPortableObjectPath(package, reference);
    return reference > 0 ? std::filesystem::path(package.sourcePath).stem().string() + '.' + path : path;
}
std::string MetaClass(const PortablePackageTables& package, const ExportTableEntry& entry) {
    return entry.ObjClass == 0 ? "Core.Class" : Qualified(package, entry.ObjClass);
}

// AnalyzeProgram may inspect structural identities in THIS source table only.
// Anything requiring transactions, typed bindings, dispatch or execution fails.
class StructuralHost final : public Vm::Host {
public:
    explicit StructuralHost(const PortablePackageTables& source) : source_(source) {}
    std::size_t forbiddenCalls{}, nameReads{}, objectReads{};
    void Begin() override { Forbidden(); }
    void Commit() override { Forbidden(); }
    void Rollback() noexcept override { ++forbiddenCalls; }
    bool CanCall(const Vm::Function&, const std::string&) override { Forbidden(); }
    std::shared_ptr<const Vm::Function> PrepareFunction(const Vm::Function&, const std::string&) override { Forbidden(); }
    std::shared_ptr<const Vm::Function> ResolveEvent(const std::string&, const std::string&, bool) override { Forbidden(); }
    Vm::Property ResolveProperty(const Vm::Function&, std::int32_t) override { Forbidden(); }
    std::string ResolveName(const Vm::Function& function, const std::int32_t index) override {
        Source(function); ++nameReads;
        Require(index >= 0 && static_cast<std::size_t>(index) < source_.names.size(), "Structural name is outside source table");
        return source_.names[static_cast<std::size_t>(index)].Name.ToString();
    }
    std::string ResolveObject(const Vm::Function& function, const std::int32_t reference) override {
        Source(function); ++objectReads;
        Require(reference == 0 || (reference > 0 && static_cast<std::size_t>(reference) <= source_.exports.size()) ||
            (reference < 0 && static_cast<std::uint64_t>(-static_cast<std::int64_t>(reference)) <= source_.imports.size()),
            "Structural object is outside source table");
        const auto path = Qualified(source_, reference);
        Require(reference == 0 || !path.empty(), "Structural object has no source identity");
        return path;
    }
    std::string ResolveCastClass(const Vm::Function&, std::int32_t) override { Forbidden(); }
    Vm::Value CastObject(const std::string&, const Vm::Value&, bool) override { Forbidden(); }
    std::shared_ptr<const Vm::Function> ResolveFunction(const Vm::Function&, const std::string&, const Vm::Invocation&) override { Forbidden(); }
    std::shared_ptr<Vm::Reference> Variable(const std::string&, const Vm::Property&, Vm::Scope) override { Forbidden(); }
    Vm::Evaluation Native(std::uint16_t, const std::string&, const std::vector<Vm::Evaluation>&, const Vm::Function*) override { Forbidden(); }
private:
    const PortablePackageTables& source_;
    void Source(const Vm::Function& function) {
        Require(function.source == source_.sourcePath, "Structural lookup crossed the declaring function's source table");
    }
    [[noreturn]] void Forbidden() {
        ++forbiddenCalls;
        throw std::runtime_error("Read-only ClassContext analysis attempted execution/binding/dispatch");
    }
};

struct Script {
    std::string path, package, kind;
    Vm::ProgramLayout layout;
    std::vector<std::size_t> parents;
};
struct PackageCounts { std::size_t functions{}, states{}, classes{}, contexts{}, contextFunctions{}; };
struct Audit {
    std::map<std::string, PortablePackageTables> packages;
    std::map<std::filesystem::path, Bytes> sources;
    std::map<std::string, Script> scripts;
    std::map<std::string, PackageCounts> packageCounts;
    Counts selectors, bodies, zeroSizes, skips;
    std::size_t sections{}, contexts{}, assignments{}, bodySelf{}, nestedBodyContext{}, literalNull{}, selectorCalls{};

    const Vm::ExpressionLayout& At(const Script& script, const std::size_t pc) const {
        const auto& nodes = script.layout.expressions;
        const auto found = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) { return node.offset == pc; });
        Require(found != nodes.end(), "Missing decoded expression: " + script.path + ':' + std::to_string(pc));
        return *found;
    }
    const Script& Function(const std::string& path) const {
        const auto found = scripts.find(path);
        Require(found != scripts.end(), "Missing ClassContext-bearing function: " + path);
        return found->second;
    }
    const Vm::ExpressionLayout& Child(const Script& script, const Vm::ExpressionLayout& node, const std::size_t index) const {
        Require(index < node.children.size(), "Missing decoded child in " + script.path);
        const auto child = node.children[index];
        Require(child < script.layout.expressions.size(), "Decoded child index outside expression snapshot");
        return script.layout.expressions[child];
    }
    std::string Identity(const Script& script, const Vm::ExpressionLayout& node) const {
        const auto& package = packages.at(script.package);
        if (node.opcode == 0x1bu || node.opcode == 0x38u || node.opcode == 0x21u) {
            Require(node.reference >= 0 && static_cast<std::size_t>(node.reference) < package.names.size(), "Decoded name operand outside package");
            return package.names[static_cast<std::size_t>(node.reference)].Name.ToString();
        }
        return Qualified(package, node.reference);
    }
    std::pair<const PortablePackageTables*, std::size_t> Export(const std::string& path) const {
        const auto dot = path.find('.');
        Require(dot != std::string::npos, "Reflected audit path lacks package: " + path);
        const auto package = packages.find(path.substr(0u, dot));
        Require(package != packages.end(), "Reflected audit path escaped four-package metadata scope: " + path);
        return {&package->second, FindPortableExport(package->second, path.substr(dot + 1u))};
    }
    PortablePropertyDescriptor Property(const std::string& path) const {
        const auto [package, index] = Export(path);
        return LoadPortablePropertyDescriptor(*package, index);
    }
    bool Derived(const std::string& path, const std::string& base) const {
        std::set<std::string> visited;
        auto current = path;
        while (!current.empty()) {
            Require(visited.size() < 64u && visited.insert(current).second, "Cyclic/deep original class ancestry");
            if (current == base) return true;
            const auto [package, index] = Export(current);
            const auto& entry = package->exports[index];
            Require(entry.ObjClass == 0, "Original ancestry contains a non-UClass identity");
            current = Qualified(*package, entry.ObjBase);
        }
        return false;
    }
    template<class Callback> void Walk(const Script& script, const std::size_t index, Callback&& callback) const {
        const auto& node = script.layout.expressions.at(index);
        callback(node);
        for (const auto child : node.children) Walk(script, child, callback);
    }
    void Analyze(const PortablePackageTables& package, const std::size_t index, const std::string& kind, Bytes bytecode) {
        Vm::Function function;
        function.path = Qualified(package, static_cast<std::int32_t>(index + 1u));
        function.source = package.sourcePath; function.bytecode = std::move(bytecode);
        StructuralHost host(package);
        Script script{function.path, std::filesystem::path(package.sourcePath).stem().string(), kind,
            Vm::AnalyzeProgram(host, function), {}};
        Require(host.forbiddenCalls == 0u, "Structural audit performed a forbidden host operation");
        const auto& nodes = script.layout.expressions;
        script.parents.assign(nodes.size(), noParent);
        for (std::size_t n = 0u; n < nodes.size(); ++n) {
            Require(nodes[n].offset < function.bytecode.size(), "Decoded node PC outside normalized script");
            if (n) Require(nodes[n - 1u].offset < nodes[n].offset, "Expression snapshot is not flat source-order preorder");
            for (const auto child : nodes[n].children) {
                Require(child > n && child < nodes.size(), "Expression child is not a bounded preorder descendant");
                Require(script.parents[child] == noParent, "Decoded expression has multiple parents");
                script.parents[child] = n;
            }
        }
        ++sections;
        auto& tally = packageCounts[script.package];
        if (kind == "Function") ++tally.functions;
        else if (kind == "State") ++tally.states;
        else ++tally.classes;
        std::size_t inFunction{};
        for (std::size_t n = 0u; n < nodes.size(); ++n) {
            const auto& node = nodes[n];
            if (node.opcode != 0x12u) continue;
            Require(kind == "Function", "Reference corpus unexpectedly contains ClassContext in a Class/State script section");
            Require(node.children.size() == 2u, "ClassContext lacks exact selector/body structure");
            const auto& selector = Child(script, node, 0u); const auto& body = Child(script, node, 1u);
            ++contexts; ++inFunction; ++tally.contexts;
            ++selectors[selector.opcode]; ++bodies[body.opcode]; ++zeroSizes[node.zeroFillSize]; ++skips[node.target];
            literalNull += selector.opcode == 0x2au;
            Walk(script, node.children[0u], [&](const auto& descendant) {
                selectorCalls += descendant.opcode == 0x1bu || descendant.opcode == 0x1cu ||
                    descendant.opcode == 0x38u || descendant.opcode >= 0x60u;
            });
            Walk(script, node.children[1u], [&](const auto& descendant) {
                bodySelf += descendant.opcode == 0x17u;
                nestedBodyContext += descendant.opcode == 0x12u || descendant.opcode == 0x19u;
            });
            for (auto descendant = n; script.parents[descendant] != noParent; descendant = script.parents[descendant]) {
                const auto& parent = nodes[script.parents[descendant]];
                if ((parent.opcode == 0x0fu || parent.opcode == 0x14u) && parent.children.at(0u) == descendant) {
                    ++assignments; break;
                }
            }
            std::cout << "SITE " << function.path << ':' << node.offset << " selector=" << unsigned(selector.opcode) <<
                " child=" << unsigned(body.opcode) << " nullSkip=" << node.target << " zeroFill=" << unsigned(node.zeroFillSize) << '\n';
        }
        if (inFunction) {
            ++tally.contextFunctions;
            Require(scripts.emplace(function.path, std::move(script)).second, "Duplicate original script identity");
        }
    }
};

const Vm::ExpressionLayout& CheckContext(const Audit& audit, const std::string& function, const std::size_t pc,
    const unsigned selector, const unsigned body, const unsigned skip, const unsigned zero) {
    const auto& script = audit.Function(function); const auto& node = audit.At(script, pc);
    Require(node.opcode == 0x12u && node.children.size() == 2u && node.target == skip && node.zeroFillSize == zero,
        "Original ClassContext metadata changed: " + function + ':' + std::to_string(pc));
    Require(audit.Child(script, node, 0u).opcode == selector && audit.Child(script, node, 1u).opcode == body,
        "Original ClassContext selector/body shape changed: " + function + ':' + std::to_string(pc));
    return node;
}
void CheckIdentity(const Audit& audit, const Script& script, const Vm::ExpressionLayout& node, const std::string& expected) {
    Require(audit.Identity(script, node) == expected, "Qualified reflected/name identity changed: " + script.path + ':' + std::to_string(node.offset));
}
void CheckProperty(const Audit& audit, const std::string& path, const std::string& type, const int dimension,
    const std::string& referenced = {}) {
    const auto descriptor = audit.Property(path);
    Require(descriptor.type == type && descriptor.arrayDimension == dimension, "Original property type/dimension changed: " + path);
    if (!referenced.empty()) {
        const auto [package, index] = audit.Export(path); (void)index;
        Require(Qualified(*package, descriptor.referencedType) == referenced, "Original property's referenced type changed: " + path);
    }
}
void Representatives(const Audit& audit) {
    {
        const std::string function = "Engine.Pawn.InitPlayerReplicationInfo";
        const auto& node = CheckContext(audit, function, 36u, 0x20u, 0x02u, 5u, 0u);
        const auto& script = audit.Function(function);
        CheckIdentity(audit, script, audit.Child(script, node, 0u), "Engine.GameInfo");
        CheckIdentity(audit, script, audit.Child(script, node, 1u), "Engine.GameInfo.DefaultPlayerName");
        Require(audit.Child(script, node, 0u).offset == 37u && audit.Child(script, node, 1u).offset == 45u,
            "Exact original startup selector/default PCs changed");
        Require(audit.At(script, 21u).opcode == 0x0fu && audit.At(script, 22u).opcode == 0x19u,
            "Original startup assignment lost its separate PRI instance context");
        CheckProperty(audit, "Engine.GameInfo.DefaultPlayerName", "StrProperty", 1);
    }
    {
        const std::string function = "Engine.GameInfo.Login";
        const auto& node = CheckContext(audit, function, 729u, 0x00u, 0x2du, 6u, 4u);
        const auto& script = audit.Function(function); const auto& body = audit.Child(script, node, 1u);
        CheckIdentity(audit, script, audit.Child(script, node, 0u), "Engine.GameInfo.Login.SpawnClass");
        Require(body.children.size() == 1u && audit.Child(script, body, 0u).opcode == 0x02u,
            "Original BoolVariable is not a wrapper around DefaultVariable");
        CheckIdentity(audit, script, audit.Child(script, body, 0u), "Engine.Pawn.bIsHuman");
        CheckProperty(audit, "Engine.Pawn.bIsHuman", "BoolProperty", 1);
    }
    {
        const std::string function = "Engine.TestInfo.test";
        const auto& node = CheckContext(audit, function, 1u, 0x20u, 0x02u, 5u, 12u);
        const auto& script = audit.Function(function); const auto& assignment = audit.At(script, 0u);
        Require(assignment.opcode == 0x0fu && audit.Child(script, assignment, 0u).offset == node.offset &&
            audit.Child(script, assignment, 1u).opcode == 0x23u, "Original vector ClassContext is not an actual assignment LHS");
        CheckIdentity(audit, script, audit.Child(script, node, 1u), "Engine.TestInfo.v1");
        CheckProperty(audit, "Engine.TestInfo.v1", "StructProperty", 1, "Core.Object.Vector");
    }
    {
        const std::string function = "DeusEx.TeamDMGame.ChangeTeam"; const auto& script = audit.Function(function);
        for (const auto& [pc, arrayPC] : std::array<std::pair<std::size_t, std::size_t>, 2>{{{212u,206u},{354u,348u}}}) {
            const auto& node = CheckContext(audit, function, pc, 0x20u, 0x02u, 5u, 32u);
            CheckIdentity(audit, script, audit.Child(script, node, 0u), pc == 212u ? "DeusEx.Mpunatco" : "DeusEx.mpnsf");
            CheckIdentity(audit, script, audit.Child(script, node, 1u), "Engine.Actor.MultiSkins");
            const auto& array = audit.At(script, arrayPC);
            Require(array.opcode == 0x1au && audit.Child(script, array, 1u).offset == pc,
                "Original MultiSkins array indexing is not outside ClassContext");
        }
        CheckProperty(audit, "Engine.Actor.MultiSkins", "ObjectProperty", 8, "Engine.Texture");
    }
    {
        const std::string function = "DeusEx.ScriptedPawn.InitializeInventory";
        const auto& node = CheckContext(audit, function, 205u, 0x13u, 0x02u, 5u, 4u);
        const auto& script = audit.Function(function); const auto& selector = audit.Child(script, node, 0u);
        CheckIdentity(audit, script, selector, "Engine.Ammo");
        const auto& member = audit.Child(script, selector, 0u);
        Require(member.opcode == 0x36u && audit.Child(script, member, 0u).opcode == 0x1au,
            "Original inventory selector lost MetaCast/StructMember/fixed-array structure");
        CheckIdentity(audit, script, member, "DeusEx.ScriptedPawn.InventoryItem.Inventory");
        CheckIdentity(audit, script, audit.Child(script, node, 1u), "Engine.Ammo.AmmoAmount");
    }
    {
        const std::string function = "DeusEx.ScriptedPawn.AICanShoot";
        for (const auto pc : {999u,1033u,1067u}) {
            const auto& node = CheckContext(audit, function, pc, 0x19u, 0x02u, 5u, 4u);
            const auto& script = audit.Function(function); const auto& selector = audit.Child(script, node, 0u);
            Require(audit.Child(script, selector, 0u).opcode == 0x00u && audit.Child(script, selector, 1u).opcode == 0x01u,
                "Original selector Context lost its instance property lookup");
            CheckIdentity(audit, script, audit.Child(script, selector, 1u), "Engine.Weapon.ProjectileClass");
        }
        CheckContext(audit, "DeusEx.DeusExWeapon.UpdateInfo", 859u, 0x1au, 0x02u, 5u, 0u);
        CheckContext(audit, "Engine.Weapon.PostBeginPlay", 47u, 0x01u, 0x02u, 5u, 4u);
        CheckContext(audit, "DeusEx.PawnGenerator.GeneratePawn", 151u, 0x36u, 0x02u, 5u, 4u);
    }
    {
        const std::string function = "Engine.PlayerPawn.ReceiveLocalizedMessage";
        const auto& node = CheckContext(audit, function, 0u, 0x00u, 0x1bu, 27u, 0u);
        const auto& script = audit.Function(function); const auto& call = audit.Child(script, node, 1u);
        CheckIdentity(audit, script, call, "ClientReceive");
        Require(call.offset == 9u && call.children.size() == 5u && audit.Child(script, call, 0u).opcode == 0x17u &&
            audit.Child(script, call, 0u).offset == 14u, "Actual ClassContext static-call Self argument changed");
        const auto [package, index] = audit.Export("Engine.LocalMessage.ClientReceive");
        const auto declaration = LoadPortableFunctionScript(*package, index);
        Require(declaration.nativeIndex == 0u && declaration.functionFlags == 0x2002u,
            "Original ClientReceive is no longer an actual Defined|Static non-native script function");
        const auto [messagePackage, messageIndex] = audit.Export("Engine.PlayerPawn.ReceiveLocalizedMessage.Message");
        const auto message = LoadPortablePropertyDescriptor(*messagePackage, messageIndex);
        Require(message.type == "ClassProperty" && Qualified(*messagePackage, message.secondaryType) == "Engine.LocalMessage",
            "Original Message selector's class constraint changed");
        const std::string tickPath="Engine.TestInfo.Tick";
        for (const auto pc : {875u,892u}) {
            const auto& staticNode = CheckContext(audit, tickPath, pc, pc == 875u ? 0x00u : 0x13u, 0x1bu, 8u, 4u);
            const auto& tick = audit.Function(tickPath);
            CheckIdentity(audit, tick, audit.Child(tick, staticNode, 1u), "TestStatic");
        }
        const auto [staticPackage, staticIndex] = audit.Export("Engine.TestInfo.TestStatic");
        const auto testStatic = LoadPortableFunctionScript(*staticPackage, staticIndex);
        Require(testStatic.nativeIndex == 0u && testStatic.functionFlags == 0x2002u, "Original TestStatic declaration changed");
        CheckProperty(audit, "Engine.TestInfo.TestStatic.ReturnValue", "IntProperty", 1);
    }
    {
        const std::string function = "DeusEx.DeusExBaseWindow.CanPushScreen";
        const auto& script = audit.Function(function);
        for (const auto pc : {22u,45u,81u,212u}) {
            const auto& node = CheckContext(audit, function, pc, 0x00u, 0x02u, 5u, 1u);
            CheckIdentity(audit, script, audit.Child(script, node, 0u), "DeusEx.DeusExBaseWindow.CanPushScreen.newScreen");
            CheckIdentity(audit, script, audit.Child(script, node, 1u), "DeusEx.DeusExBaseWindow.ScreenType");
        }
        CheckProperty(audit, "DeusEx.DeusExBaseWindow.ScreenType", "ByteProperty", 1, "DeusEx.DeusExBaseWindow.EScreenType");
        const auto [package, index] = audit.Export("DeusEx.DeusExBaseWindow.CanPushScreen.newScreen");
        const auto selector = LoadPortablePropertyDescriptor(*package, index);
        Require(selector.type == "ClassProperty" && Qualified(*package, selector.secondaryType) == "DeusEx.DeusExBaseWindow",
            "Original UI selector's actual class constraint changed");
        Require(audit.Derived("DeusEx.DeusExBaseWindow", "Extension.ModalWindow") &&
            audit.Derived("DeusEx.DeusExBaseWindow", "Core.Object") && !audit.Derived("DeusEx.DeusExBaseWindow", "Engine.Actor"),
            "Original UI defaults were incorrectly classified as Actor-only");
    }
}

void Inventory(const Audit& audit) {
    const std::map<std::string, std::vector<std::size_t>> expected{
        {"Engine.Pawn.InitPlayerReplicationInfo", {36}}, {"Engine.PlayerPawn.ReceiveLocalizedMessage", {0}},
        {"Engine.WayBeacon.PostBeginPlay", {60}}, {"Engine.GameInfo.Login", {546,729}}, {"Engine.TestInfo.test", {1}},
        {"Engine.TestInfo.Tick", {562,583,604,625,657,678,699,720,752,778,804,830,875,892}},
        {"Engine.GameInfo.PostBeginPlay", {122,171,220,269}}, {"Engine.Weapon.PostBeginPlay", {47,67,98,118}},
        {"Engine.PlayerPawn.Summon", {134}}, {"DeusEx.DeusExWeapon.WeaponDamageType", {124}},
        {"DeusEx.PersonaAmmoDetailButton.DrawWindow", {406}}, {"DeusEx.PersonaAmmoDetailButton.SetAmmo", {48}},
        {"DeusEx.DeusExWeapon.PlayFiringSound", {94}}, {"DeusEx.DeusExWeapon.GetWeaponRanges", {33,54,75}},
        {"DeusEx.TeamDMGame.ChangeTeam", {212,251,280,354,393,422}}, {"DeusEx.DeusExWeapon.TravelPostAccept", {65}},
        {"DeusEx.DeusExWeapon.LoadAmmo", {165,480,736,786}}, {"DeusEx.DeusExBaseWindow.CanPushScreen", {22,45,81,212}},
        {"DeusEx.ScriptedPawn.AICanShoot", {999,1033,1067}},
        {"DeusEx.ScriptedPawn.AISafeToThrow", {115,141,283,297,374,388,586,637,739,790}},
        {"DeusEx.DeusExWeapon.UpdateInfo", {768,859,3141,3233,3252}}, {"DeusEx.DeusExWeapon.UpdateAmmoInfo", {160,251,329,348}},
        {"DeusEx.PersonaScreenAugmentations.CreateAugCanWindow", {81,110,124}},
        {"DeusEx.PersonaScreenAugmentations.CreateBioCellWindow", {81,110,124}}, {"DeusEx.ScriptedPawn.InitializeInventory", {205}},
        {"DeusEx.PersonaScreenHealth.CreateMedKitWindow", {81,110,124}}, {"DeusEx.DeusExPlayer.MultiplayerTick", {324,347}},
        {"DeusEx.DeusExPlayer.ViewModelGiveWeapon", {148}}, {"DeusEx.DeusExPlayer.ViewModelAdd", {203,246}},
        {"DeusEx.PersonaScreenInventory.CreateNanoKeyRingWindow", {81}}, {"DeusEx.PersonaScreenInventory.CreateAmmoWindow", {81,110,124}},
        {"DeusEx.PersonaScreenInventory.ButtonActivated", {203}}, {"DeusEx.DeusExPlayer.Timer", {51,74}},
        {"DeusEx.DeusExPlayer.CreateKillerProfile", {1331,1831}}, {"DeusEx.DeusExPlayer.GetSkillInfoFromProj", {449}},
        {"DeusEx.PersonaScreenInventory.LoadAmmo", {141}}, {"DeusEx.DeusExPlayer.SpawnMass", {290,367}},
        {"DeusEx.DeusExPlayer.PlayerWalking.ProcessMove", {1245}}, {"DeusEx.DeusExPlayer.CreateDrone", {14}},
        {"DeusEx.DeusExPlayer.PlayFootStep", {1452}}, {"DeusEx.PawnGenerator.GeneratePawn", {151,267,292,391,432}},
        {"DeusEx.PawnGenerator.GetClassPhysics", {6}}
    };
    Require(audit.scripts.size() == expected.size(), "Original ClassContext function inventory changed");
    for (const auto& [path, pcs] : expected) {
        std::vector<std::size_t> actual;
        for (const auto& node : audit.Function(path).layout.expressions) if (node.opcode == 0x12u) actual.push_back(node.offset);
        Require(actual == pcs, "Original structurally decoded ClassContext PC inventory changed: " + path);
    }
}
void Original(const std::filesystem::path& suppliedRoot) {
    const auto root = std::filesystem::canonical(suppliedRoot);
    const auto system = std::filesystem::canonical(root / "System");
    Require(system.parent_path() == root, "Original System source directory escaped supplied game root");
    Audit audit;
    for (const auto* name : {"Core", "Engine", "DeusEx", "Extension"}) {
        const auto path = std::filesystem::canonical(system / (std::string(name) + ".u"));
        Require(path.parent_path() == system && path.extension() == ".u", "Original package source escaped explicit System .u scope");
        audit.sources.emplace(path, ReadSource(path));
        audit.packages.emplace(name, LoadPortablePackageTables(path.string()));
        Require(audit.packages.at(name).version == 68u, "Reference census requires the owned v68 GOTY package corpus");
    }
    // Core is loaded only for representative reflected types/ancestry. It is
    // deliberately excluded from the three-package 8,282-section census.
    for (const auto* name : {"Engine", "DeusEx", "Extension"}) {
        const auto& package = audit.packages.at(name);
        for (std::size_t index = 0u; index < package.exports.size(); ++index) {
            const auto& entry = package.exports[index]; const auto meta = MetaClass(package, entry);
            if (meta != "Core.Function" && meta != "Core.State" && entry.ObjClass != 0) continue;
            try {
                Bytes bytecode;
                const std::string kind = entry.ObjClass == 0 ? "Class" : meta == "Core.State" ? "State" : "Function";
                if (kind == "Class") bytecode = LoadPortableClassDescriptor(package, index).state.bytecode;
                else if (kind == "State") bytecode = LoadPortableStateDescriptor(package, index).bytecode;
                else bytecode = LoadPortableFunctionScript(package, index).bytecode;
                audit.Analyze(package, index, kind, std::move(bytecode));
            } catch (const std::exception& error) {
                throw std::runtime_error(Qualified(package, static_cast<std::int32_t>(index + 1u)) + ": " + error.what());
            }
        }
    }
    const std::map<std::string, PackageCounts> expected{{"Engine",{1263,65,89,29,9}},
        {"DeusEx",{4933,185,1158,81,33}}, {"Extension",{555,0,34,0,0}}};
    for (const auto& [name, counts] : expected) {
        const auto& actual = audit.packageCounts.at(name);
        Require(actual.functions == counts.functions && actual.states == counts.states && actual.classes == counts.classes &&
            actual.contexts == counts.contexts && actual.contextFunctions == counts.contextFunctions,
            "Original package script-section/context totals changed: " + name);
    }
    Require(audit.sections == 8282u && audit.contexts == 110u && audit.scripts.size() == 42u, "Original full census totals changed");
    Require(audit.selectors == Counts{{0x20u,37u},{0x00u,42u},{0x13u,7u},{0x01u,14u},{0x19u,3u},{0x1au,2u},{0x36u,5u}},
        "Original ClassContext selector-root distribution changed");
    Require(audit.bodies == Counts{{0x02u,92u},{0x2du,15u},{0x1bu,3u}}, "Original ClassContext child-root distribution changed");
    Require(audit.zeroSizes == Counts{{0u,20u},{1u,5u},{4u,82u},{12u,1u},{32u,2u}}, "Original zero-fill metadata distribution changed");
    Require(audit.skips == Counts{{5u,92u},{6u,15u},{8u,2u},{27u,1u}}, "Original null-skip metadata distribution changed");
    Require(audit.assignments == 1u && audit.bodySelf == 1u && audit.nestedBodyContext == 0u &&
        audit.literalNull == 0u && audit.selectorCalls == 0u, "Original write/Self/nested/null/selector-call structural census changed");
    Inventory(audit); Representatives(audit);
    for (const auto& [path, bytes] : audit.sources) Require(ReadSource(path) == bytes, "Read-only census source changed: " + path.string());
    std::cout << "PASS original ClassContext structural audit: sections=8282 expressions=110 functions=42 checks=" << checks <<
        "; production loaders/AST, exact PCs/qualified shapes/zero-fill metadata, sources unchanged. "
        "Structural evidence only: no VM execution, CDO identity/null-result correctness, static dispatch, world/player/tutorial completion claim.\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cout << "SKIP original ClassContext structural audit: supply the read-only Deus Ex GOTY installation root.\n";
        return 77;
    }
    if (argc != 2) { std::cerr << "Usage: portable_class_context_audit_test [GAME_ROOT]\n"; return 1; }
    try {
        const std::filesystem::path root(argv[1]);
        for (const auto* name : {"Core", "Engine", "DeusEx", "Extension"}) {
            if (!std::filesystem::is_regular_file(root / "System" / (std::string(name) + ".u"))) {
                std::cout << "SKIP original ClassContext structural audit: required read-only GOTY packages are missing.\n";
                return 77;
            }
        }
        Original(root); return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL original ClassContext structural audit: " << error.what() << '\n'; return 1;
    }
}
