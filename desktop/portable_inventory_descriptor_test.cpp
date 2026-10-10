#include "Precomp.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"
#include "quest_script_state.h"
#include "GC/GC.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Production-runtime controls with generated UE1 packages, not a mock reader
// or class-name/icon heuristic. Optional original-data mode reads only four
// original script packages and uses a generated map importing their classes.
namespace {
using Bytes = std::vector<std::uint8_t>;
using Value = QuestVr::Vm::Value;
using Kind = QuestVr::Vm::Kind;
using Status = PortableInventoryDescriptorStatus;
using Origin = PortableInventoryValueOrigin;
std::size_t checks{}, rejections{};
void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
template<class Action> void Reject(Action action, const std::string& message) {
    bool rejected{};
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected, message); ++rejections;
}
void U16(Bytes& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value)); bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
}
void U32(Bytes& bytes, std::uint32_t value) { U16(bytes, static_cast<std::uint16_t>(value)); U16(bytes, static_cast<std::uint16_t>(value >> 16u)); }
void U64(Bytes& bytes, std::uint64_t value) { U32(bytes, static_cast<std::uint32_t>(value)); U32(bytes, static_cast<std::uint32_t>(value >> 32u)); }
void Replace32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8u * i));
}
void Index(Bytes& bytes, std::int32_t value) {
    auto magnitude = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((value < 0 ? 0x80u : 0u) | (magnitude & 63u));
    magnitude >>= 6u; if (magnitude) first |= 64u; bytes.push_back(first);
    while (magnitude) {
        auto next = static_cast<std::uint8_t>(magnitude & 127u);
        magnitude >>= 7u; if (magnitude) next |= 128u; bytes.push_back(next);
    }
}
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    Require(size <= 4u * 1024u * 1024u, "Generated fixture/checkpoint exceeds read bound");
    Bytes bytes(static_cast<std::size_t>(size)); std::ifstream file(path, std::ios::binary);
    Require(static_cast<bool>(file), "Cannot read generated fixture/checkpoint");
    if (!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "Generated fixture/checkpoint short read"); return bytes;
}
struct Package {
    std::string stem;
    std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
    std::vector<Bytes> bodies;
    std::int32_t Name(const std::string& name) {
        const auto found = std::find(names.begin(), names.end(), name);
        if (found != names.end()) return static_cast<std::int32_t>(found - names.begin());
        names.push_back(name); return static_cast<std::int32_t>(names.size() - 1u);
    }
    std::int32_t Import(const std::string& name, std::int32_t outer, const std::string& type = "Class", const std::string& source = "Core") {
        imports.push_back({Name(source), Name(type), outer, Name(name)}); return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) { return Import(name, 0, "Package"); }
    std::int32_t Export(const std::string& name, std::int32_t cls = 0, std::int32_t base = 0, std::int32_t outer = 0) {
        exports.push_back({cls, base, outer, Name(name), ObjectFlags{}, 0, -1}); bodies.emplace_back();
        return static_cast<std::int32_t>(exports.size());
    }
    void Body(std::int32_t reference, Bytes body) { bodies.at(static_cast<std::size_t>(reference - 1)) = std::move(body); }
    void Int(Bytes& body, const char* name, std::int32_t value, std::uint8_t arrayIndex = 0u) {
        Index(body, Name(name)); body.push_back(arrayIndex ? 0xa2u : 0x22u);
        if (arrayIndex) body.push_back(arrayIndex);
        U32(body, static_cast<std::uint32_t>(value));
    }
    void Bool(Bytes& body, const char* name, bool value) { Index(body, Name(name)); body.push_back(value ? 0x83u : 0x03u); }
    void Object(Bytes& body, const char* name, std::int32_t reference) {
        Bytes encoded; Index(encoded, reference); Index(body, Name(name));
        body.push_back(0x55u); body.push_back(static_cast<std::uint8_t>(encoded.size()));
        body.insert(body.end(), encoded.begin(), encoded.end());
    }
    Bytes Class(std::int32_t reference, const Bytes& defaults, std::int32_t firstChild = 0) {
        Bytes body; const auto& entry = exports.at(static_cast<std::size_t>(reference - 1));
        Index(body, entry.ObjBase); Index(body, 0); Index(body, 0); Index(body, firstChild); Index(body, entry.ObjName);
        U32(body, 0); U32(body, 0); U32(body, 0); U64(body, 0); U64(body, 0); U16(body, 0xffffu); U32(body, 0);
        U32(body, 0); body.resize(body.size() + 16u, 0u);
        Index(body, 0); Index(body, 0); Index(body, 0); Index(body, 0);
        body.insert(body.end(), defaults.begin(), defaults.end()); Index(body, 0); return body;
    }
    std::int32_t Property(const char* name, const char* type, std::int32_t owner, std::int32_t core,
        std::uint32_t dimension = 1u, std::int32_t target = 0) {
        const auto reference = Export(name, Import(type, core), 0, owner);
        Bytes body; Index(body, 0); Index(body, 0); Index(body, 0); U32(body, dimension); U32(body, 0); Index(body, 0);
        if (std::string(type) == "ObjectProperty" || std::string(type) == "ByteProperty" || std::string(type) == "StructProperty") Index(body, target);
        Body(reference, std::move(body)); return reference;
    }
    void PropertyNext(std::int32_t reference, std::int32_t next) {
        auto& body = bodies.at(static_cast<std::size_t>(reference - 1));
        // Property() emits an empty tagged prefix and zero Base/Next links.
        Bytes linked{0u}; Index(linked, 0); Index(linked, next);
        linked.insert(linked.end(), body.begin() + 3, body.end()); body = std::move(linked);
    }
    void Struct(std::int32_t reference, std::int32_t firstChild) {
        Bytes body{0u}; Index(body, 0); Index(body, 0); Index(body, 0); Index(body, firstChild);
        Index(body, exports.at(static_cast<std::size_t>(reference - 1)).ObjName);
        U32(body, 0); U32(body, 0); U32(body, 0); Body(reference, std::move(body));
    }
    Bytes Serialize() const {
        Bytes bytes; U32(bytes, 0x9e2a83c1u); U16(bytes, 68u); U16(bytes, 0u);
        bytes.resize(56u, 0u); // Full v68 header/GUID/zero generations.
        std::vector<std::int32_t> offsets;
        for (const auto& body : bodies) {
            offsets.push_back(static_cast<std::int32_t>(bytes.size())); bytes.insert(bytes.end(), body.begin(), body.end());
        }
        Replace32(bytes, 12u, static_cast<std::uint32_t>(names.size())); Replace32(bytes, 16u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& name : names) {
            Index(bytes, static_cast<std::int32_t>(name.size() + 1u)); bytes.insert(bytes.end(), name.begin(), name.end()); bytes.push_back(0u); U32(bytes, 0u);
        }
        Replace32(bytes, 20u, static_cast<std::uint32_t>(exports.size())); Replace32(bytes, 24u, static_cast<std::uint32_t>(bytes.size()));
        for (std::size_t i = 0u; i < exports.size(); ++i) {
            const auto& entry = exports[i]; Index(bytes, entry.ObjClass); Index(bytes, entry.ObjBase);
            U32(bytes, static_cast<std::uint32_t>(entry.ObjOuter)); Index(bytes, entry.ObjName); U32(bytes, static_cast<std::uint32_t>(entry.ObjFlags));
            Index(bytes, static_cast<std::int32_t>(bodies[i].size())); if (!bodies[i].empty()) Index(bytes, offsets[i]);
        }
        Replace32(bytes, 28u, static_cast<std::uint32_t>(imports.size())); Replace32(bytes, 32u, static_cast<std::uint32_t>(bytes.size()));
        for (const auto& entry : imports) { Index(bytes, entry.ClassPackage); Index(bytes, entry.ClassName); U32(bytes, static_cast<std::uint32_t>(entry.ObjOuter)); Index(bytes, entry.ObjName); }
        return bytes;
    }
};
struct Fixture {
    std::filesystem::path directory, temporaryParent;
    std::map<std::filesystem::path, Bytes> sourceBytes;
    Fixture() {
        temporaryParent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned i = 0u; i < 20u; ++i) {
            const auto candidate = temporaryParent / ("deusex-inventory-descriptor-test-" + std::to_string(stamp) + '-' + std::to_string(i));
            if (!std::filesystem::create_directory(candidate)) continue;
            directory = std::filesystem::canonical(candidate); break;
        }
        Require(!directory.empty() && directory.parent_path() == temporaryParent &&
            directory.filename().string().rfind("deusex-inventory-descriptor-test-", 0u) == 0u, "Fixture escaped temporary parent");
        std::filesystem::create_directory(directory / "System"); std::filesystem::create_directory(directory / "Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime(); std::error_code ignored;
        // The exact owned temporary child is revalidated before recursive cleanup.
        const auto actual = std::filesystem::weakly_canonical(directory, ignored);
        if (!ignored && !directory.empty() && actual == directory && actual.parent_path() == temporaryParent &&
            actual.filename().string().rfind("deusex-inventory-descriptor-test-", 0u) == 0u) std::filesystem::remove_all(actual, ignored);
    }
    PortablePackageTables Write(const Package& package, bool map = false) {
        const auto path = directory / (map ? "Maps" : "System") / (package.stem + (map ? ".dx" : ".u"));
        const auto bytes = package.Serialize();
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc); Require(static_cast<bool>(file), "Cannot create generated UE1 package");
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); Require(static_cast<bool>(file), "Cannot write generated UE1 package");
        }
        sourceBytes[path] = bytes; auto table = LoadPortablePackageTables(path.string());
        Require(table.version == 68u && table.exports.size() == package.exports.size() && table.names.size() == package.names.size(), "Fixture did not pass production table reader");
        for (std::size_t i = 0u; i < table.exports.size(); ++i)
            Require(table.exports[i].ObjClass == package.exports[i].ObjClass && table.exports[i].ObjBase == package.exports[i].ObjBase &&
                table.exports[i].ObjOuter == package.exports[i].ObjOuter && table.exports[i].ObjSize == static_cast<std::int32_t>(package.bodies[i].size()), "Fixture export metadata mismatch");
        return table;
    }
    Bytes Snapshot(const std::string& name) const {
        const auto path = directory / (name + ".sav"); Require(SavePortableRuntimeState(path.string()), "Cannot capture runtime checkpoint"); return ReadBytes(path);
    }
    void SourcesUnchanged() const { for (const auto& [path, bytes] : sourceBytes) Require(ReadBytes(path) == bytes, "Descriptor mutated generated source package"); }
};
Package Engine(bool localTextureClass = false) {
    Package package; package.stem = "Engine";
    const auto core = package.ImportPackage("Core"), object = package.Import("Object", core);
    const auto actor = package.Export("Actor", 0, object), inventory = package.Export("Inventory", 0, actor);
    std::int32_t bitmap{}, texture{};
    if (localTextureClass) {
        bitmap = package.Export("Bitmap", 0, object);
        texture = package.Export("Texture", 0, bitmap);
    } else {
        texture = package.Import("Texture", package.ImportPackage("Engine"));
    }
    std::int32_t firstInventory{};
    for (const char* name : {"invSlotsX", "invSlotsY", "invPosX", "invPosY", "largeIconWidth", "largeIconHeight"}) {
        const auto ref = package.Property(name, "IntProperty", inventory, core); if (!firstInventory) firstInventory = ref;
    }
    package.Property("bDisplayableInv", "BoolProperty", inventory, core);
    package.Property("largeIcon", "ObjectProperty", inventory, core, 1u, texture); package.Property("Icon", "ObjectProperty", inventory, core, 1u, texture);
    // Real typed clock schema permits a saved non-zero clock/state control;
    // no animation command, script, fake AI/startup or texture decode is run.
    for (const char* name : {"AnimSequence", "BlendAnimSequence"}) package.Property(name, "NameProperty", actor, core, name[0] == 'B' ? 4u : 1u);
    for (const char* name : {"AnimFrame", "AnimRate", "AnimLast", "AnimMinRate", "TweenRate", "OldAnimRate", "BlendAnimFrame", "BlendAnimRate", "BlendAnimLast", "BlendAnimMinRate", "BlendTweenRate", "OldBlendAnimRate"})
        package.Property(name, "FloatProperty", actor, core, std::string(name).find("Blend") != std::string::npos ? 4u : 1u);
    for (const char* name : {"bAnimLoop", "bAnimNotify", "bAnimFinished"}) package.Property(name, "BoolProperty", actor, core);
    package.Property("Fatness", "ByteProperty", actor, core); package.Property("RemoteRole", "ByteProperty", actor, core);
    package.Property("QuestBudgetPadding", "FloatProperty", actor, core, 65'536u);
    const auto plane = package.Export("Plane", package.Import("Struct", core));
    std::array<std::int32_t, 4u> planeFields{};
    for (std::size_t i = 0u; i < planeFields.size(); ++i)
        planeFields[i] = package.Property(std::array<const char*, 4u>{"X", "Y", "Z", "W"}[i], "FloatProperty", plane, core);
    for (std::size_t i = 1u; i < planeFields.size(); ++i) package.PropertyNext(planeFields[i - 1u], planeFields[i]);
    package.Struct(plane, planeFields.front()); package.Property("SimBlendAnim", "StructProperty", actor, core, 4u, plane);
    Bytes defaults; package.Int(defaults, "invSlotsX", 1); package.Int(defaults, "invSlotsY", 1);
    package.Int(defaults, "invPosX", -1); package.Int(defaults, "invPosY", -1); package.Bool(defaults, "bDisplayableInv", true);
    package.Body(actor, package.Class(actor, {})); package.Body(inventory, package.Class(inventory, defaults, firstInventory));
    if (localTextureClass) {
        package.Body(bitmap, package.Class(bitmap, {})); package.Body(texture, package.Class(texture, {}));
    }
    return package;
}
Package Icons() {
    Package package; package.stem = "DescriptorIcons"; const auto texture = package.Import("Texture", package.ImportPackage("Engine"));
    for (const char* name : {"LargeRifle", "LargeAssault", "LargeGEP", "Small", "Override"}) package.Export(name, texture);
    package.Export("NotTexture", package.Import("Actor", package.ImportPackage("Engine")));
    package.Export("ClassNotTexture");
    // Same leaf name is not the exact native Engine.Texture registration.
    package.Export("WrongPackageTexture", package.Import("Texture", package.ImportPackage("Unregistered")));
    return package; // Metadata-only assets: the descriptor does not open/decode them.
}
Package Items() {
    Package package; package.stem = "DescriptorItems";
    const auto inventory = package.Import("Inventory", package.ImportPackage("Engine"));
    const auto icons = package.ImportPackage("DescriptorIcons"); const auto small = package.Import("Small", icons, "Texture", "Engine");
    struct Item { const char* name; const char* icon; int x, y, width, height; };
    for (const auto& item : std::array<Item, 3>{{{"Rifle", "LargeRifle", 4, 1, 159, 47}, {"Assault", "LargeAssault", 2, 2, 94, 65}, {"GEP", "LargeGEP", 4, 2, 203, 77}}}) {
        const auto ref = package.Export(item.name, 0, inventory); const auto icon = package.Import(item.icon, icons, "Texture", "Engine");
        Bytes defaults; package.Int(defaults, "invSlotsX", item.x); if (item.y != 1) package.Int(defaults, "invSlotsY", item.y);
        package.Int(defaults, "largeIconWidth", item.width); package.Int(defaults, "largeIconHeight", item.height);
        package.Object(defaults, "largeIcon", icon); package.Object(defaults, "Icon", small); package.Body(ref, package.Class(ref, defaults));
    }
    const auto fallback = package.Export("Fallback", 0, inventory); Bytes defaults; package.Object(defaults, "Icon", small); package.Body(fallback, package.Class(fallback, defaults));
    const auto child = package.Export("RifleChild", 0, 1); package.Body(child, package.Class(child, {})); return package;
}
Package Map() {
    Package package; package.stem = "DescriptorMap";
    const auto items = package.ImportPackage("DescriptorItems"), engine = package.ImportPackage("Engine"), icons = package.ImportPackage("DescriptorIcons");
    const auto rifle = package.Import("Rifle", items), assault = package.Import("Assault", items), gep = package.Import("GEP", items), fallback = package.Import("Fallback", items), child = package.Import("RifleChild", items);
    const auto icon = package.Import("Override", icons, "Texture", "Engine");
    const auto add = [&](const char* name, std::int32_t cls, Bytes properties = {}) {
        const auto ref = package.Export(name, cls); Index(properties, 0); package.Body(ref, std::move(properties));
    };
    Bytes position; package.Int(position, "invPosX", 0); package.Int(position, "invPosY", 0); add("Rifle0", rifle, position);
    add("Assault0", assault); add("GEP0", gep); add("Fallback0", fallback); add("Child0", child);
    Bytes override; package.Int(override, "invSlotsX", 3); package.Int(override, "largeIconWidth", 73); package.Int(override, "largeIconHeight", 41);
    package.Object(override, "largeIcon", icon); add("Override0", rifle, override);
    Bytes hidden; package.Bool(hidden, "bDisplayableInv", false); package.Int(hidden, "invSlotsX", 0); add("Hidden0", rifle, hidden);
    Bytes partial; package.Int(partial, "invPosX", 1); add("Partial0", rifle, partial);
    Bytes badSlots; package.Int(badSlots, "invSlotsX", 6); add("BadSlots0", rifle, badSlots);
    Bytes badDimensions; package.Int(badDimensions, "largeIconWidth", 0); add("BadDimensions0", rifle, badDimensions);
    Bytes wrongType; Index(wrongType, package.Name("invSlotsX")); wrongType.push_back(0x24u); U32(wrongType, 1u); add("WrongType0", rifle, wrongType);
    Bytes wrongIndex; package.Int(wrongIndex, "invSlotsX", 2, 1u); add("WrongIndex0", rifle, wrongIndex);
    Bytes noneLarge; package.Object(noneLarge, "largeIcon", 0); add("NoneLarge0", rifle, noneLarge);
    Bytes classTag; Index(classTag, package.Name("largeIcon")); classTag.push_back(0x08u); Index(classTag, 0); add("ClassTag0", rifle, classTag);
    Bytes badIcon; package.Object(badIcon, "largeIcon", package.Import("NotTexture", icons, "Actor", "Engine")); add("BadIcon0", rifle, badIcon);
    Bytes classIcon; package.Object(classIcon, "largeIcon", package.Import("ClassNotTexture", icons)); add("ClassIcon0", rifle, classIcon);
    Bytes wrongPackage; package.Object(wrongPackage, "largeIcon", package.Import("WrongPackageTexture", icons, "Texture", "Unregistered")); add("WrongPackageIcon0", rifle, wrongPackage);
    Bytes absentIcon; package.Object(absentIcon, "largeIcon", package.Import("Absent", icons, "Texture", "Engine")); add("AbsentIcon0", rifle, absentIcon);
    Bytes actorProperties; package.Int(actorProperties, "invPosX", 0); add("NotInventory0", package.Import("Actor", engine), actorProperties);
    return package;
}
std::vector<QuestVr::ScriptSavedProperty> ClockProperties(const QuestVr::ActorAnimationClock& clock) {
    std::vector<QuestVr::ScriptSavedProperty> values;
    const auto add = [&](const char* name, Value value, std::uint32_t index = 0u) { values.push_back({"Engine.Actor." + std::string(name), name, index, std::move(value)}); };
    add("AnimSequence", Value::Text(Kind::Name, clock.pose.main.sequence)); add("AnimFrame", Value::Float(clock.pose.main.normalizedFrame));
    add("AnimRate", Value::Float(clock.main.rate)); add("AnimLast", Value::Float(clock.main.last)); add("AnimMinRate", Value::Float(clock.main.minRate));
    add("TweenRate", Value::Float(clock.main.tweenRate)); add("OldAnimRate", Value::Float(clock.main.oldRate));
    add("bAnimLoop", Value::Bool(clock.main.loop)); add("bAnimNotify", Value::Bool(clock.main.notify)); add("bAnimFinished", Value::Bool(clock.main.finished));
    add("Fatness", Value::Byte(clock.pose.fatness)); add("RemoteRole", Value::Byte(clock.remoteRole));
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const auto& rates = clock.blends[i]; add("BlendAnimSequence", Value::Text(Kind::Name, clock.pose.blends[i].sequence), i);
        add("BlendAnimFrame", Value::Float(clock.pose.blends[i].normalizedFrame), i); add("BlendAnimRate", Value::Float(rates.rate), i);
        add("BlendAnimLast", Value::Float(rates.last), i); add("BlendAnimMinRate", Value::Float(rates.minRate), i);
        add("BlendTweenRate", Value::Float(rates.tweenRate), i); add("OldBlendAnimRate", Value::Float(rates.oldRate), i);
        Value simulated; simulated.kind = Kind::Struct;
        for (std::size_t field = 0u; field < rates.simulated.size(); ++field)
            simulated.fields[std::array<const char*, 4u>{"x", "y", "z", "w"}[field]] = Value::Float(rates.simulated[field]);
        add("SimBlendAnim", std::move(simulated), i);
    }
    return values;
}
void LegacySimBlendImportBudget(const Fixture& fixture, const Bytes& legacy) {
    QuestVr::ScriptStateLimits limits;
    Require(legacy.size() + 4u < QuestVr::kMaximumSaveRuntimeBytes, "Legacy prefix exhausts runtime budget");
    limits.maxBytes = QuestVr::kMaximumSaveRuntimeBytes - legacy.size() - 4u;
    QuestVr::ScriptSavedState saved; saved.mapName = "DescriptorMap";
    QuestVr::ScriptSavedObject actor; actor.path = "DescriptorMap.Rifle0"; actor.classPath = "DescriptorItems.Rifle";
    actor.clock.emplace(); actor.clock->pose.main.sequence = "None";
    for (std::size_t slot = 0u; slot < actor.clock->blends.size(); ++slot) {
        actor.clock->pose.blends[slot].sequence = "None";
        actor.clock->blends[slot].simulated = {float(slot + 1u), 2.5f, -3.25f, 4.75f};
    }
    actor.properties = ClockProperties(*actor.clock);
    actor.properties.erase(std::remove_if(actor.properties.begin(), actor.properties.end(),
        [](const auto& property) { return property.name == "SimBlendAnim"; }), actor.properties.end());
    saved.objects.push_back(std::move(actor));
    const auto bytesFor = [&](const QuestVr::ScriptSavedState& state) {
        const auto blob = QuestVr::EncodeScriptSavedState(state, limits); auto bytes = legacy;
        Require(bytes.size() >= 8u && bytes[4] == 3u, "Import budget control needs an exact legacy checkpoint");
        Replace32(bytes, 4u, 4u); U32(bytes, static_cast<std::uint32_t>(blob.size()));
        bytes.insert(bytes.end(), blob.begin(), blob.end()); return bytes;
    };
    const auto path = fixture.directory / "LegacySimBlendImportBudget.sav";
    // Establish that the complete typed clock is valid before adding padding;
    // no fake property, malformed graph or exhausted input byte budget is used.
    Require(QuestVr::WriteDurableSaveFile(path.string(), bytesFor(saved)) &&
        ValidatePortableRuntimeState(path.string()), "Small legacy SimBlendAnim import is not valid");
    auto& properties = saved.objects.front().properties;
    const auto padding = limits.maxProperties - properties.size();
    for (std::uint32_t index = 0u; index < padding; ++index)
        properties.push_back({"Engine.Actor.QuestBudgetPadding", "QuestBudgetPadding", index, Value::Float(0.125f)});
    QuestVr::ScriptStateDetail::Writer input(limits, nullptr); input.State(saved);
    Require(input.measuredBudget().properties == limits.maxProperties &&
        input.measuredBudget().retained < limits.maxBytes && input.size() < limits.maxBytes,
        "Legacy padding must fit the actual 16 MiB trailer limits before importing four Planes");
    const auto before = fixture.Snapshot("BeforeLegacySimBlendImportBudget"); const auto stats = GC::GetStats();
    const auto actorCount = GetPortableRuntimeMapActors(true).size();
    Require(QuestVr::WriteDurableSaveFile(path.string(), bytesFor(saved)) &&
        !ValidatePortableRuntimeState(path.string()) && !LoadPortableRuntimeState(path.string()),
        "Legacy SimBlendAnim import bypassed the cumulative property limit");
    ++rejections;
    Require(fixture.Snapshot("AfterLegacySimBlendImportBudget") == before &&
        !GetPortableRuntimeScriptStatePresent() && GetPortableRuntimeMapActors(true).size() == actorCount &&
        GC::GetStats().numObjects == stats.numObjects && GC::GetStats().memoryUsage == stats.memoryUsage,
        "Rejected legacy Plane import mutated checkpoint, script state, actor index or GC");
    // Exactly four free slots permit the four derived records. Validation stays
    // read-only and proves this is a cumulative migration limit, not a blanket
    // refusal of legacy records or this large fixed-array declaration.
    properties.resize(limits.maxProperties - 4u);
    Require(QuestVr::WriteDurableSaveFile(path.string(), bytesFor(saved)) &&
        ValidatePortableRuntimeState(path.string()), "Exact legacy Plane-import property boundary was rejected");
    Require(fixture.Snapshot("AfterLegacySimBlendImportBoundary") == before &&
        GC::GetStats().numObjects == stats.numObjects && GC::GetStats().memoryUsage == stats.memoryUsage,
        "Boundary import validation mutated checkpoint or GC");
    std::cout << "PASS legacy SimBlendAnim import cumulative budget: legal input " << input.measuredBudget().retained
        << '/' << limits.maxBytes << " retained bytes,65536 input properties rejected before four-Plane publication;65532 accepted\n";
}
void OverlayClockAndState(const Fixture& fixture, const Bytes& legacy) {
    QuestVr::ScriptSavedState saved; saved.mapName = "DescriptorMap";
    QuestVr::ScriptSavedObject actor; actor.path = "DescriptorMap.Rifle0"; actor.classPath = "DescriptorItems.Rifle";
    const auto add = [&](const char* name, Value value) { actor.properties.push_back({"Engine.Inventory." + std::string(name), name, 0u, std::move(value)}); };
    add("invSlotsX", Value::Integer(2)); add("invSlotsY", Value::Integer(3)); add("invPosX", Value::Integer(1)); add("invPosY", Value::Integer(2));
    add("largeIcon", Value::Text(Kind::Object, "DescriptorIcons.Override")); add("largeIconWidth", Value::Integer(73)); add("largeIconHeight", Value::Integer(41));
    actor.clock.emplace(); actor.clock->simulationTime = 13.25; actor.clock->pose.main.sequence = "None";
    actor.clock->pose.main.normalizedFrame = 0.125f; actor.clock->main.last = 0.5f; actor.clock->main.finished = true;
    for (auto& channel : actor.clock->pose.blends) channel.sequence = "None";
    for (auto& property : ClockProperties(*actor.clock)) actor.properties.push_back(std::move(property));
    actor.state.emplace(); actor.state->hasStack = true; actor.state->frameOverride = true; actor.state->frame.emplace();
    actor.state->frame->statementIndex = 17u; actor.state->frame->latent = QuestVr::StateLatent::Stop; // Cleared code, not fabricated execution.
    saved.objects.push_back(std::move(actor));
    const auto bytesFor = [&](const QuestVr::ScriptSavedState& state) {
        const auto blob = QuestVr::EncodeScriptSavedState(state); auto bytes = legacy;
        Require(bytes.size() >= 8u && bytes[4] == 3u, "Overlay control needs an exact legacy checkpoint");
        Replace32(bytes, 4u, 5u); U32(bytes, static_cast<std::uint32_t>(blob.size())); bytes.insert(bytes.end(), blob.begin(), blob.end()); return bytes;
    };
    // Generic restoration already refuses wrong-class/unindexed overlays.
    // Keep that transactional guard; do not inject private runtime values.
    for (const char* bad : {"DescriptorIcons.NotTexture", "DescriptorIcons.Absent"}) {
        auto invalid = saved;
        for (auto& property : invalid.objects[0].properties) if (property.name == "largeIcon") property.value.text = bad;
        const auto path = fixture.directory / "RejectedIconOverlay.sav";
        Require(QuestVr::WriteDurableSaveFile(path.string(), bytesFor(invalid)) && !ValidatePortableRuntimeState(path.string()) &&
            !LoadPortableRuntimeState(path.string()), "Wrong-class/unindexed icon overlay bypassed transactional schema validation");
        Require(fixture.Snapshot("AfterRejectedIconOverlay") == legacy, "Rejected icon overlay mutated checkpoint");
    }
    const auto path = fixture.directory / "OverlayClockState.sav";
    Require(QuestVr::WriteDurableSaveFile(path.string(), bytesFor(saved)) && LoadPortableRuntimeState(path.string()), "Schema-checked generated overlay/clock/state did not restore");
}
void Synthetic() {
    ShutdownPortableRuntime();
    Require(ReadPortableRuntimeInventoryDescriptors({"DescriptorMap.Rifle0"}).at(0).status == Status::RuntimeUnavailable, "Unavailable runtime was invented");
    Fixture fixture; auto engine = Engine(); const auto icons = Icons(), items = Items(), map = Map();
    const auto engineTable = fixture.Write(engine), iconTable = fixture.Write(icons), itemTable = fixture.Write(items), mapTable = fixture.Write(map, true);
    const auto summary = InitializePortableRuntime(std::vector<PortablePackageTables>{engineTable, iconTable, itemTable});
    Require(!summary.passed && summary.functions == 0u && summary.normalizedBytecodeBytes == 0u && summary.properties > 9u,
        "Generated metadata-only runtime gained fake script/startup success");
    Require(LoadPortableRuntimeMap(mapTable).passed, "Generated map did not load production actors/properties");
    const auto pickup = InteractPortableRuntimeActor("DescriptorMap.Rifle0");
    Require(pickup.handled && pickup.action == "pickup" && GetPortableRuntimeInventoryItems() == std::vector<std::string>{"DescriptorMap.Rifle0"}, "Inactive fixture must be a real collected actor identity");
    const auto legacy = fixture.Snapshot("BeforeDescriptors"); const auto stats = GC::GetStats();
    const auto actorsBefore = GetPortableRuntimeMapActors(true).size();
    const std::vector<std::string> requests{"descriptormap.RIFLE0", "DescriptorMap.Assault0", "DescriptorMap.GEP0", "DescriptorMap.Fallback0", "DescriptorMap.Child0",
        "DescriptorMap.Override0", "DescriptorMap.Hidden0", "DescriptorMap.Partial0", "DescriptorMap.BadSlots0", "DescriptorMap.BadDimensions0",
        "DescriptorMap.WrongType0", "DescriptorMap.WrongIndex0", "DescriptorMap.NoneLarge0", "DescriptorMap.NotInventory0", "DescriptorItems.Rifle",
        "DescriptorItems.Rifle@event:1", "Missing.Actor", "", std::string("x\0y", 3u), std::string(4097u, 'x'), "DescriptorMap.Rifle0",
        "DescriptorMap.ClassTag0", "DescriptorMap.BadIcon0", "DescriptorMap.ClassIcon0", "DescriptorMap.WrongPackageIcon0", "DescriptorMap.AbsentIcon0"};
    auto values = ReadPortableRuntimeInventoryDescriptors(requests); Require(values.size() == requests.size(), "Batch lost request order/count");
    const auto& rifle = values[0];
    Require(rifle.status == Status::Available && !rifle.active && rifle.actorPath == "DescriptorMap.Rifle0" && rifle.classPath == "DescriptorItems.Rifle" &&
        rifle.actorSourcePath == mapTable.sourcePath && rifle.classSourcePath == itemTable.sourcePath && rifle.invSlotsX == 4 && rifle.invSlotsY == 1 &&
        rifle.invPosX == 0 && rifle.invPosY == 0 && rifle.positionAssigned && rifle.bDisplayableInv && rifle.largeIconWidth == 159 && rifle.largeIconHeight == 47 &&
        rifle.iconPath == "DescriptorIcons.LargeRifle" && rifle.displayWidth == 159 && rifle.displayHeight == 47 && rifle.usesLargeIcon, "Inactive authored rifle/default/layout/provenance mismatch");
    Require(rifle.largeIconProvenance.origin == Origin::ClassDefault && rifle.largeIconProvenance.ownerPath == "DescriptorItems.Rifle" &&
        rifle.largeIconProvenance.ownerSourcePath == itemTable.sourcePath && rifle.largeIconProvenance.declarationPath == "Engine.Inventory.largeIcon" &&
        rifle.largeIconProvenance.declarationSourcePath == engineTable.sourcePath, "Icon provenance guessed from an item name");
    Require(values[1].status == Status::Available && values[1].invSlotsX == 2 && values[1].invSlotsY == 2 && values[1].displayWidth == 94 && values[1].displayHeight == 65,
        "Assault metadata wrong");
    Require(values[2].status == Status::Available && values[2].invSlotsX == 4 && values[2].invSlotsY == 2 && values[2].displayWidth == 203 && values[2].displayHeight == 77,
        "GEP metadata wrong");
    Require(values[3].status == Status::Available && !values[3].usesLargeIcon && values[3].largeIconPath.empty() && values[3].iconPath == "DescriptorIcons.Small" &&
        values[3].displayWidth == 40 && values[3].displayHeight == 35 && values[3].invPosX == -1 && values[3].invPosY == -1 && !values[3].positionAssigned,
        "Fallback must be original40x35, not padded texture dimensions or fabricated positions");
    Require(values[4].status == Status::Available && values[4].classPath == "DescriptorItems.RifleChild" && values[4].largeIconProvenance.ownerPath == "DescriptorItems.Rifle" && values[4].displayWidth == 159,
        "Inherited icon effective owner collapsed to leaf class");
    Require(values[5].status == Status::Available && values[5].invSlotsX == 3 && values[5].iconPath == "DescriptorIcons.Override" && values[5].displayWidth == 73 &&
        values[5].largeIconProvenance.origin == Origin::Instance && values[5].largeIconProvenance.ownerSourcePath == mapTable.sourcePath, "Instance override/default precedence wrong");
    Require(values[6].status == Status::Available && !values[6].bDisplayableInv && values[6].invSlotsX == 0, "Hidden item wrongly coerced to a displayable grid item");
    Require(values[7].status == Status::InvalidLayout && values[7].invPosX == 1 && values[7].invPosY == -1 && !values[7].positionAssigned, "Partial assigned position was silently repacked");
    Require(values[8].status == Status::InvalidLayout && values[8].invSlotsX == 6, "Invalid slot footprint was silently clamped");
    Require(values[9].status == Status::InvalidIconDimensions && values[9].usesLargeIcon && values[9].displayWidth == 0 && values[9].iconPath == "DescriptorIcons.LargeRifle", "Malformed large icon silently switched to fallback");
    Require(values[10].status == Status::MalformedProperty && values[11].status == Status::MalformedProperty, "Wrong typed/indexed scalar accepted");
    Require(values[12].status == Status::Available && values[12].largeIconPath.empty() && !values[12].usesLargeIcon && values[12].displayWidth == 40 && values[12].displayHeight == 35 &&
        values[12].largeIconProvenance.origin == Origin::Instance, "Explicit instance None did not suppress inherited largeIcon");
    Require(values[13].status == Status::NotInventoryActor && values[14].status == Status::NotInventoryActor, "Noninventory actor/class was accepted as an inventory instance");
    Require(values[15].status == Status::UnindexedIdentity && values[16].status == Status::UnindexedIdentity, "Synthetic/missing identity became a fake class item");
    for (std::size_t i = 17u; i < 20u; ++i) Require(values[i].status == Status::InvalidIdentity && values[i].requestedPath.empty(), "Malformed identity retained or substituted");
    Require(values[20].actorPath == rifle.actorPath && values[20].iconPath == rifle.iconPath, "Duplicate explicit requests were deduplicated/reordered");
    for (std::size_t i = 21u; i < 25u; ++i) Require(values[i].status == Status::MalformedProperty,
        "Icon accepted a ClassProperty tag/class object/non-Texture actor or wrong native class package");
    Require(values[25].status == Status::MissingIconMetadata, "Unindexed icon was guessed to be a Texture");
    values[0].iconPath = "changed detached data"; values[0].invSlotsX = 1;
    Require(ReadPortableRuntimeInventoryDescriptors({"DescriptorMap.Rifle0"})[0].invSlotsX == 4, "Returned descriptor aliases runtime state");
    Reject([] { (void)ReadPortableActorScriptProperty("DescriptorMap.Rifle0", "invSlotsX"); }, "Generic active-actor API was loosened for picked-up instances");
    for (const auto count : {0u, 1025u}) { PortableInventoryDescriptorLimits limits; limits.count = count; Reject([&] { (void)ReadPortableRuntimeInventoryDescriptors({"DescriptorMap.Rifle0"}, limits); }, "Invalid count budget accepted"); }
    Reject([] { (void)ReadPortableRuntimeInventoryDescriptors(std::vector<std::string>(1025u, "DescriptorMap.Rifle0")); }, "Oversized request count accepted");
    for (const auto bytes : {0u, 1u, 8u * 1024u * 1024u + 1u}) { PortableInventoryDescriptorLimits limits; limits.retainedBytes = bytes; Reject([&] { (void)ReadPortableRuntimeInventoryDescriptors({"DescriptorMap.Rifle0"}, limits); }, "Invalid/exhausted aggregate budget accepted"); }
    for (const auto size : {0u, 8193u}) { PortableInventoryDescriptorLimits limits; limits.stringBytes = size; Reject([&] { (void)ReadPortableRuntimeInventoryDescriptors({"DescriptorMap.Rifle0"}, limits); }, "Invalid string budget accepted"); }
    for (const auto depth : {0u, 129u}) { PortableInventoryDescriptorLimits limits; limits.hierarchy = depth; Reject([&] { (void)ReadPortableRuntimeInventoryDescriptors({"DescriptorMap.Rifle0"}, limits); }, "Invalid hierarchy budget accepted"); }
    PortableInventoryDescriptorLimits shallow; shallow.hierarchy = 1u;
    Require(ReadPortableRuntimeInventoryDescriptors({"DescriptorMap.Rifle0"}, shallow)[0].status == Status::MalformedProperty, "Ancestry bound bypassed");
    Require(fixture.Snapshot("AfterDescriptors") == legacy && !GetPortableRuntimeScriptStatePresent() && GetPortableRuntimeMapActors(true).size() == actorsBefore &&
        GC::GetStats().numObjects == stats.numObjects && GC::GetStats().memoryUsage == stats.memoryUsage, "Read-only descriptor/rejection mutated checkpoint/index/GC/script state");
    LegacySimBlendImportBudget(fixture, legacy);
    OverlayClockAndState(fixture, legacy); const auto overlay = fixture.Snapshot("BeforeOverlayDescriptors"); const auto overlayStats = GC::GetStats();
    const auto overlayItem = ReadPortableRuntimeInventoryDescriptors({"DescriptorMap.Rifle0"})[0];
    Require(overlayItem.status == Status::Available && !overlayItem.active && overlayItem.invSlotsX == 2 && overlayItem.invSlotsY == 3 && overlayItem.invPosX == 1 && overlayItem.invPosY == 2 &&
        overlayItem.iconPath == "DescriptorIcons.Override" && overlayItem.displayWidth == 73 && overlayItem.displayHeight == 41 && overlayItem.largeIconProvenance.origin == Origin::Overlay &&
        overlayItem.largeIconProvenance.ownerPath == "DescriptorMap.Rifle0" && overlayItem.largeIconProvenance.ownerSourcePath == mapTable.sourcePath, "Committed typed overlay did not override inactive actor defaults");
    Require(fixture.Snapshot("AfterOverlayDescriptors") == overlay && GetPortableRuntimeScriptStatePresent() &&
        GC::GetStats().numObjects == overlayStats.numObjects && GC::GetStats().memoryUsage == overlayStats.memoryUsage, "Descriptor mutated v5 overlay/nonzero-clock/cleared-stop state checkpoint");
    // Metadata absence is per-input unavailability, not a guessed declaration.
    ShutdownPortableRuntime();
    for (std::size_t i = 0u; i < engine.exports.size(); ++i) if (engine.names.at(static_cast<std::size_t>(engine.exports[i].ObjName)) == "largeIcon") {
        engine.exports[i].ObjClass = 0; engine.bodies[i].clear();
    }
    const auto missingTable = fixture.Write(engine);
    InitializePortableRuntime(std::vector<PortablePackageTables>{missingTable, iconTable, itemTable}); LoadPortableRuntimeMap(mapTable);
    const auto missing = ReadPortableRuntimeInventoryDescriptors({"DescriptorMap.Rifle0", "Missing.Actor"});
    Require(missing[0].status == Status::MissingPropertyMetadata && missing[1].status == Status::UnindexedIdentity, "Missing declaration fabricated a valid icon/aborted other rows");
    fixture.SourcesUnchanged();
    std::cout << "PASS generated source-backed inventory descriptors " << checks << " checks, " << rejections << " rejections; detached inactive/instance/default/overlay icons, exact5x6 layout, byte-identical v3/v5 clock-state checkpoints\n";
}
void LocalClassReferenceMetadata() {
    Fixture fixture; const auto engine = Engine(true), icons = Icons(), items = Items(), map = Map();
    const auto engineTable = fixture.Write(engine), iconTable = fixture.Write(icons), itemTable = fixture.Write(items), mapTable = fixture.Write(map, true);
    // Original Engine.Inventory.largeIcon references its same-package Texture
    // export, not the negative imported native constraint used by Synthetic().
    bool positiveTextureConstraint{};
    for (std::size_t i = 0u; i < engineTable.exports.size(); ++i) {
        if (engineTable.names.at(static_cast<std::size_t>(engineTable.exports[i].ObjName)).Name.ToString() != "largeIcon") continue;
        const auto property = LoadPortablePropertyDescriptor(engineTable, i);
        positiveTextureConstraint = property.referencedType > 0 &&
            GetPortableObjectPath(engineTable, property.referencedType) == "Texture";
    }
    Require(positiveTextureConstraint, "Local-class regression fixture lacks a positive Texture constraint");
    InitializePortableRuntime(std::vector<PortablePackageTables>{engineTable, iconTable, itemTable});
    Require(LoadPortableRuntimeMap(mapTable).passed, "Local-class regression map did not load");
    Require(InteractPortableRuntimeActor("DescriptorMap.Rifle0").action == "pickup", "Local-class fixture inventory was not collectible");
    const auto before = fixture.Snapshot("LocalClassBefore"); const auto stats = GC::GetStats();
    const auto values = ReadPortableRuntimeInventoryDescriptors({"DescriptorMap.Rifle0", "DescriptorMap.Assault0", "DescriptorMap.GEP0",
        "DescriptorMap.NoneLarge0", "DescriptorMap.ClassTag0", "DescriptorMap.BadIcon0", "DescriptorMap.ClassIcon0",
        "DescriptorMap.WrongPackageIcon0", "DescriptorMap.AbsentIcon0"});
    const std::array<std::array<int, 4>, 3> expected{{{{4, 1, 159, 47}}, {{2, 2, 94, 65}}, {{4, 2, 203, 77}}}};
    Require(values.size() == 9u, "Local-class descriptor count changed");
    for (std::size_t i = 0u; i < expected.size(); ++i) {
        Require(values[i].status == Status::Available && values[i].invSlotsX == expected[i][0] && values[i].invSlotsY == expected[i][1] &&
            values[i].displayWidth == expected[i][2] && values[i].displayHeight == expected[i][3] && values[i].usesLargeIcon &&
            !values[i].iconPath.empty() && values[i].largeIconProvenance.declarationPath == "Engine.Inventory.largeIcon" &&
            values[i].largeIconProvenance.declarationSourcePath == engineTable.sourcePath,
            "Positive same-package Texture constraint was rejected or lost authored icon metadata");
    }
    Require(!values[0].active && values[3].status == Status::Available && !values[3].usesLargeIcon &&
        values[3].displayWidth == 40 && values[3].displayHeight == 35, "Local-class validation changed inactive/None fallback behavior");
    for (std::size_t i = 4u; i < 8u; ++i) Require(values[i].status == Status::MalformedProperty,
        "Positive Texture constraint accepted type8/class object/non-Texture actor/wrong package");
    Require(values[8].status == Status::MissingIconMetadata, "Positive Texture constraint invented an unindexed icon");
    Require(fixture.Snapshot("LocalClassAfter") == before && GC::GetStats().numObjects == stats.numObjects &&
        GC::GetStats().memoryUsage == stats.memoryUsage, "Local-class descriptors mutated checkpoint/GC metadata");
    fixture.SourcesUnchanged();
    std::cout << "PASS generated positive same-package Texture class constraints; authored3 weapon windows, inactive/None fallback and5 failclosed icon controls; no source/checkpoint/GC mutation\n";
}
void OriginalMetadata(const std::filesystem::path& root) {
    struct Stamp { std::filesystem::path path; std::uintmax_t size; std::filesystem::file_time_type time; };
    std::vector<Stamp> sources; std::vector<PortablePackageTables> packages;
    for (const char* name : {"Core", "Engine", "DeusEx", "DeusExUI"}) {
        const auto path = std::filesystem::canonical(root / "System" / (std::string(name) + ".u"));
        sources.push_back({path, std::filesystem::file_size(path), std::filesystem::last_write_time(path)}); packages.push_back(LoadPortablePackageTables(path.string()));
    }
    Fixture fixture; Package map; map.stem = "OriginalInventoryMetadata";
    const auto deus = map.ImportPackage("DeusEx");
    const std::array<const char*, 3> classes{{"WeaponRifle", "WeaponAssaultGun", "WeaponGEPGun"}};
    std::vector<std::string> paths;
    for (const auto* cls : classes) {
        const auto ref = map.Export(std::string(cls) + "0", map.Import(cls, deus)); Bytes body;
        map.Int(body, "invPosX", -1); map.Int(body, "invPosY", -1); Index(body, 0); map.Body(ref, std::move(body)); paths.push_back(map.stem + '.' + cls + "0");
    }
    const auto table = fixture.Write(map, true); InitializePortableRuntime(packages); Require(LoadPortableRuntimeMap(table).passed, "Original class imports did not bind in generated metadata map");
    for (const auto& path : paths) Require(InteractPortableRuntimeActor(path).action == "pickup", "Original class fixture not collectible inventory");
    const auto before = fixture.Snapshot("OriginalBefore"); const auto stats = GC::GetStats(); const auto items = ReadPortableRuntimeInventoryDescriptors(paths);
    const std::array<std::array<int, 4>, 3> expected{{{{4, 1, 159, 47}}, {{2, 2, 94, 65}}, {{4, 2, 203, 77}}}};
    const std::array<const char*, 10> statusNames{{"Available", "InvalidIdentity", "RuntimeUnavailable", "UnindexedIdentity",
        "NotInventoryActor", "MissingPropertyMetadata", "MalformedProperty", "InvalidLayout", "InvalidIconDimensions", "MissingIconMetadata"}};
    bool originalMetadataMatches = true;
    for (std::size_t i = 0u; i < items.size(); ++i) {
        const auto& item = items[i]; const std::string expectedClass = std::string("DeusEx.") + classes[i];
        const bool matches = item.status == Status::Available && !item.active && item.classPath == expectedClass &&
            item.invSlotsX == expected[i][0] && item.invSlotsY == expected[i][1] && item.displayWidth == expected[i][2] && item.displayHeight == expected[i][3] &&
            item.usesLargeIcon && !item.iconPath.empty() && item.largeIconProvenance.origin == Origin::ClassDefault &&
            item.classSourcePath == packages[2].sourcePath && item.largeIconProvenance.declarationSourcePath == packages[1].sourcePath;
        if (!matches) {
            const auto statusIndex = static_cast<std::size_t>(item.status);
            std::cerr << "ORIGINAL DESCRIPTOR mismatch requested=" << item.requestedPath << " expected=" << paths[i]
                << "; status=" << (statusIndex < statusNames.size() ? statusNames[statusIndex] : "Unknown") << '(' << statusIndex << ") expected=Available(0)"
                << "; active=" << item.active << " expected=0; class=" << item.classPath << " expected=" << expectedClass
                << "; slots=" << item.invSlotsX << 'x' << item.invSlotsY << " expected=" << expected[i][0] << 'x' << expected[i][1]
                << "; window=" << item.displayWidth << 'x' << item.displayHeight << " expected=" << expected[i][2] << 'x' << expected[i][3]
                << "; usesLargeIcon=" << item.usesLargeIcon << " expected=1; icon=" << item.iconPath << " expected=nonempty"
                << "; largeIcon=" << item.largeIconPath << "; largeWindow=" << item.largeIconWidth << 'x' << item.largeIconHeight
                << "; iconOrigin=" << static_cast<unsigned int>(item.largeIconProvenance.origin) << " expected=ClassDefault(1)"
                << "; classSource=" << item.classSourcePath << " expected=" << packages[2].sourcePath
                << "; declaration=" << item.largeIconProvenance.declarationPath
                << "; declarationSource=" << item.largeIconProvenance.declarationSourcePath << " expected=" << packages[1].sourcePath << '\n';
        }
        originalMetadataMatches = originalMetadataMatches && matches;
    }
    Require(originalMetadataMatches, "Original inherited rifle/assault/GEP logical footprint/icon metadata mismatch");
    Require(fixture.Snapshot("OriginalAfter") == before && GC::GetStats().numObjects == stats.numObjects && GC::GetStats().memoryUsage == stats.memoryUsage,
        "Original metadata inspection mutated checkpoint/index/GC");
    for (const auto& source : sources) Require(std::filesystem::file_size(source.path) == source.size && std::filesystem::last_write_time(source.path) == source.time,
        "Original script source size/mtime changed (best-effort read-only guard)");
    fixture.SourcesUnchanged();
    std::cout << "PASS optional original4-package metadata on generated actor map:3inactive classes, logical windows159x47/94x65/203x77; no campaign/script/texture/device test\n";
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc > 2) throw std::runtime_error("Usage: portable_inventory_descriptor_test [original-game-root]");
        Synthetic(); LocalClassReferenceMetadata(); if (argc == 2) OriginalMetadata(argv[1]); return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; ShutdownPortableRuntime(); return 1; }
}
