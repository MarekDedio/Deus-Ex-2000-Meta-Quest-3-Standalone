#include "Precomp.h"
#include "portable_unreal_runtime.h"
#include "quest_equipped_item_visual.h"
#include "quest_held_item_assets.h"
#include "quest_save_bundle.h"
#include "quest_script_state.h"
#include "GC/GC.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Production-runtime controls with generated UE1 packages, not a mock reader
// or class-name/icon heuristic. Optional owned-data mode reads original packages without writes and uses a
// generated map importing actual inventory classes. Owned mode additionally
// checks native equipment selection against predecoded original POV metadata;
// it does not prove tracked rendering or original Fire-state execution.
namespace {
using Bytes = std::vector<std::uint8_t>;
using Value = QuestVr::Vm::Value;
using Kind = QuestVr::Vm::Kind;
using Status = PortableHeldItemDescriptorStatus;
using Origin = PortableHeldItemValueOrigin;
using FieldStatus = PortableHeldItemValueStatus;
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
        // Genuine serialized Children/Next membership, not orphan fields found
        // by name. Schema-checked overlay restoration traverses these links.
        std::vector<std::int32_t> fields;
        for (std::size_t index = 0u; index < exports.size(); ++index)
            if (exports[index].ObjOuter == reference) fields.push_back(static_cast<std::int32_t>(index+1u));
        if (firstChild == 0 && !fields.empty()) firstChild = fields.front();
        for (std::size_t index = 0u; index < fields.size(); ++index)
            PropertyNext(fields[index],index+1u < fields.size() ? fields[index+1u] : 0);
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
        // Relinking can replace a multi-byte compact Next reference.
        std::size_t tail=3u;
        if ((body.at(2u)&64u)!=0u) {
            while ((body.at(tail++)&128u)!=0u) {}
        }
        Bytes linked{0u}; Index(linked, 0); Index(linked, next);
        linked.insert(linked.end(),body.begin()+static_cast<std::ptrdiff_t>(tail),body.end()); body = std::move(linked);
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
            const auto candidate = temporaryParent / ("deusex-held-item-descriptor-test-" + std::to_string(stamp) + '-' + std::to_string(i));
            if (!std::filesystem::create_directory(candidate)) continue;
            directory = std::filesystem::canonical(candidate); break;
        }
        Require(!directory.empty() && directory.parent_path() == temporaryParent &&
            directory.filename().string().rfind("deusex-held-item-descriptor-test-", 0u) == 0u, "Fixture escaped temporary parent");
        std::filesystem::create_directory(directory / "System"); std::filesystem::create_directory(directory / "Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime(); std::error_code ignored;
        // The exact owned temporary child is revalidated before recursive cleanup.
        const auto actual = std::filesystem::weakly_canonical(directory, ignored);
        if (!ignored && !directory.empty() && actual == directory && actual.parent_path() == temporaryParent &&
            actual.filename().string().rfind("deusex-held-item-descriptor-test-", 0u) == 0u) std::filesystem::remove_all(actual, ignored);
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


void RealTag(Package& package, Bytes& body, const char* name, const float value) {
    std::uint32_t bits{}; std::memcpy(&bits,&value,4u);
    Index(body,package.Name(name)); body.push_back(0x24u); U32(body,bits);
}
void ObjectSlot(Package& package, Bytes& body, const char* name, const std::int32_t reference,
    const std::uint8_t slot, const std::uint8_t type = 5u) {
    Bytes encoded; Index(encoded,reference); Index(body,package.Name(name));
    body.push_back(static_cast<std::uint8_t>(0xd0u|type)); body.push_back(static_cast<std::uint8_t>(encoded.size()));
    body.push_back(slot); body.insert(body.end(),encoded.begin(),encoded.end());
}
Package Engine(const bool localAssetClasses) {
    Package package; package.stem = "Engine";
    const auto core = package.ImportPackage("Core"), object = package.Import("Object",core);
    const auto actor = package.Export("Actor",0,object), inventory = package.Export("Inventory",0,actor), weapon = package.Export("Weapon",0,inventory);
    std::int32_t mesh{}, texture{}, primitive{}, lod{}, bitmap{};
    if (localAssetClasses) {
        primitive = package.Export("Primitive",0,object); mesh = package.Export("Mesh",0,primitive); lod = package.Export("LodMesh",0,mesh);
        bitmap = package.Export("Bitmap",0,object); texture = package.Export("Texture",0,bitmap);
        for (const auto reference : {primitive,mesh,lod,bitmap,texture}) package.Body(reference,package.Class(reference,{}));
    } else {
        const auto engine = package.ImportPackage("Engine");
        mesh = package.Import("Mesh",engine); texture = package.Import("Texture",engine);
    }
    for (const char* name : {"PlayerViewMesh","PickupViewMesh","ThirdPersonMesh"}) package.Property(name,"ObjectProperty",inventory,core,1u,mesh);
    for (const char* name : {"PlayerViewScale","PickupViewScale","ThirdPersonScale"}) package.Property(name,"FloatProperty",inventory,core);
    package.Property("Skin","ObjectProperty",actor,core,1u,texture);
    package.Property("MultiSkins","ObjectProperty",actor,core,8u,texture);
    for (const char* name : {"HitDamage","AccurateRange","MaxRange"}) package.Property(name,"IntProperty",weapon,core);
    package.Property("bHandToHand","BoolProperty",weapon,core);
    package.Body(actor,package.Class(actor,{}));
    Bytes defaults;
    for (const char* name : {"PlayerViewScale","PickupViewScale","ThirdPersonScale"}) RealTag(package,defaults,name,1.0f);
    package.Body(inventory,package.Class(inventory,defaults)); package.Body(weapon,package.Class(weapon,{}));
    return package;
}
Package Assets() {
    Package package; package.stem = "HeldAssets"; const auto engine = package.ImportPackage("Engine");
    const auto mesh = package.Import("Mesh",engine), lod = package.Import("LodMesh",engine), texture = package.Import("Texture",engine);
    for (const char* name : {"View","Pickup","Third","Override"}) package.Export(name,mesh);
    package.Export("LodView",lod);
    for (const char* name : {"Skin0","Skin1","Skin2"}) package.Export(name,texture);
    package.Export("WrongPackageMesh",package.Import("Mesh",package.ImportPackage("NotEngine")));
    package.Export("NotMesh",texture);
    const auto cls = package.Export("ClassNotMesh"); package.Body(cls,package.Class(cls,{}));
    return package; // Metadata-only objects, never decoded to geometry/textures.
}
Package Items() {
    Package package; package.stem = "HeldItems";
    const auto core = package.ImportPackage("Core"), engine = package.ImportPackage("Engine"), assets = package.ImportPackage("HeldAssets");
    const auto weapon = package.Import("Weapon",engine), inventory = package.Import("Inventory",engine);
    const auto mesh = package.Import("Mesh",engine), lod = package.Import("LodMesh",engine), texture = package.Import("Texture",engine);
    const auto view = package.Import("View",assets,"Mesh","Engine"), pickup = package.Import("Pickup",assets,"Mesh","Engine");
    const auto third = package.Import("Third",assets,"Mesh","Engine"), skin0 = package.Import("Skin0",assets,"Texture","Engine");
    const auto skin1 = package.Import("Skin1",assets,"Texture","Engine");
    const auto base = package.Export("BaseWeapon",0,weapon);
    Bytes defaults;
    package.Object(defaults,"PlayerViewMesh",view); package.Object(defaults,"PickupViewMesh",pickup); package.Object(defaults,"ThirdPersonMesh",third);
    package.Object(defaults,"Skin",skin0); ObjectSlot(package,defaults,"MultiSkins",skin0,0u); ObjectSlot(package,defaults,"MultiSkins",skin1,1u);
    package.Int(defaults,"HitDamage",6); package.Int(defaults,"AccurateRange",80); package.Int(defaults,"MaxRange",80);
    package.Bool(defaults,"bHandToHand",true); package.Body(base,package.Class(base,defaults));
    const auto child = package.Export("Child",0,base); package.Body(child,package.Class(child,{}));
    const auto ordinary = package.Export("OrdinaryInventory",0,inventory); package.Body(ordinary,package.Class(ordinary,{}));
    const auto shadow = package.Export("ShadowScale",0,base); package.Property("PlayerViewScale","FloatProperty",shadow,core); package.Body(shadow,package.Class(shadow,{}));
    const auto wrongRange = package.Export("WrongRange",0,base); package.Property("MaxRange","FloatProperty",wrongRange,core); package.Body(wrongRange,package.Class(wrongRange,{}));
    const auto wrongMesh = package.Export("WrongConstraint",0,base); package.Property("PlayerViewMesh","ObjectProperty",wrongMesh,core,1u,texture);
    package.Body(wrongMesh,package.Class(wrongMesh,{}));
    const auto narrow = package.Export("NarrowLod",0,base); package.Property("PlayerViewMesh","ObjectProperty",narrow,core,1u,lod);
    Bytes narrowDefaults; package.Object(narrowDefaults,"PlayerViewMesh",view); package.Body(narrow,package.Class(narrow,narrowDefaults));
    const auto validLod = package.Export("ValidLod",0,narrow); Bytes lodDefaults;
    package.Object(lodDefaults,"PlayerViewMesh",package.Import("LodView",assets,"LodMesh","Engine")); package.Body(validLod,package.Class(validLod,lodDefaults));
    const auto wrongArray = package.Export("WrongArray",0,base); package.Property("MultiSkins","ObjectProperty",wrongArray,core,7u,texture);
    package.Body(wrongArray,package.Class(wrongArray,{}));
    (void)mesh;
    return package;
}
Package Map() {
    Package package; package.stem = "HeldMap";
    const auto items = package.ImportPackage("HeldItems"), assets = package.ImportPackage("HeldAssets");
    for (const char* name : {"Default","Child","OrdinaryInventory","ShadowScale","WrongRange","WrongConstraint","NarrowLod","ValidLod","WrongArray",
                            "Override","NoneView","BadType","BadScalarIndex","MissingAsset","WrongAsset","WrongPackage","ClassAsset","NanScale","BadSkinIndex"}) {
        const char* cls = std::string(name) == "Default" || std::string(name) == "Override" || std::string(name) == "NoneView" ||
            std::string(name) == "BadType" || std::string(name) == "BadScalarIndex" || std::string(name) == "MissingAsset" ||
            std::string(name) == "WrongAsset" || std::string(name) == "WrongPackage" || std::string(name) == "ClassAsset" ||
            std::string(name) == "NanScale" || std::string(name) == "BadSkinIndex" ? "BaseWeapon" : name;
        const auto ref = package.Export(name,package.Import(cls,items)); Bytes body;
        if (std::string(name) == "Override") {
            package.Object(body,"PlayerViewMesh",package.Import("Override",assets,"Mesh","Engine")); RealTag(package,body,"PlayerViewScale",0.75f);
            ObjectSlot(package,body,"MultiSkins",0,0u); ObjectSlot(package,body,"MultiSkins",package.Import("Skin2",assets,"Texture","Engine"),2u);
            package.Int(body,"HitDamage",9);
        } else if (std::string(name) == "NoneView") package.Object(body,"PlayerViewMesh",0);
        else if (std::string(name) == "BadType") ObjectSlot(package,body,"PlayerViewMesh",package.Import("View",assets,"Mesh","Engine"),0u,8u);
        else if (std::string(name) == "BadScalarIndex") ObjectSlot(package,body,"PlayerViewMesh",package.Import("View",assets,"Mesh","Engine"),1u);
        else if (std::string(name) == "MissingAsset") package.Object(body,"PlayerViewMesh",package.Import("AbsentMesh",assets,"Mesh","Engine"));
        else if (std::string(name) == "WrongAsset") package.Object(body,"PlayerViewMesh",package.Import("NotMesh",assets,"Texture","Engine"));
        else if (std::string(name) == "WrongPackage") package.Object(body,"PlayerViewMesh",package.Import("WrongPackageMesh",assets,"Mesh","NotEngine"));
        else if (std::string(name) == "ClassAsset") package.Object(body,"PlayerViewMesh",package.Import("ClassNotMesh",assets));
        else if (std::string(name) == "NanScale") RealTag(package,body,"PlayerViewScale",std::numeric_limits<float>::quiet_NaN());
        else if (std::string(name) == "BadSkinIndex") ObjectSlot(package,body,"MultiSkins",package.Import("Skin0",assets,"Texture","Engine"),8u);
        Index(body,0); package.Body(ref,std::move(body));
    }
    const auto nonActor = package.Export("NotActor",package.Import("Object",package.ImportPackage("Core")));
    package.Body(nonActor,Bytes{0u});
    return package;
}
void RequireField(const PortableHeldItemValue<std::string>& field, const std::string& expected, const std::string& label) {
    Require(field.status == FieldStatus::Available && field.value && *field.value == expected,label);
}
void Synthetic(const bool localAssetClasses) {
    Fixture fixture; auto engine = Engine(localAssetClasses), assets = Assets(), items = Items(), map = Map();
    const auto engineTable = fixture.Write(engine), assetsTable = fixture.Write(assets), itemsTable = fixture.Write(items), mapTable = fixture.Write(map,true);
    InitializePortableRuntime(std::vector<PortablePackageTables>{engineTable,assetsTable,itemsTable});
    Require(LoadPortableRuntimeMap(mapTable).passed,"Generated held-item map failed to bind production classes");
    Require(ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.Default").status == Status::NotCollected,"World pickup was falsely represented as held/equipped");
    for (const auto& entry : map.exports) {
        const auto name = map.names.at(static_cast<std::size_t>(entry.ObjName));
        if (name != "NotActor") Require(InteractPortableRuntimeActor("HeldMap."+name).action == "pickup","Generated identity was not collected through actual interaction");
    }
    const auto before = fixture.Snapshot("Before"); const auto revision = GetPortableRuntimeWorldRevision(); const auto stats = GC::GetStats();
    auto base = ReadPortableRuntimeHeldItemVisualDescriptor("heldmap.dEFAULT");
    Require(base.status == Status::Available && base.collected && !base.active && base.weapon && base.actorPath == "HeldMap.Default" &&
        base.classPath == "HeldItems.BaseWeapon" && base.actorSourcePath == mapTable.sourcePath && base.classSourcePath == itemsTable.sourcePath,
        "Inactive collected identity/class provenance was lost or inferred from the request spelling");
    RequireField(base.playerViewMesh,"HeldAssets.View","Actual inherited PlayerViewMesh was replaced by a POV-name heuristic");
    RequireField(base.pickupViewMesh,"HeldAssets.Pickup","Actual PickupViewMesh reference was lost");
    RequireField(base.thirdPersonMesh,"HeldAssets.Third","Actual ThirdPersonMesh reference was lost");
    Require(base.playerViewMesh.provenance.origin == Origin::ClassDefault && base.playerViewMesh.provenance.ownerPath == "HeldItems.BaseWeapon" &&
        base.playerViewMesh.provenance.ownerSourcePath == itemsTable.sourcePath && base.playerViewMesh.provenance.declarationPath == "Engine.Inventory.PlayerViewMesh" &&
        base.playerViewMesh.provenance.declarationSourcePath == engineTable.sourcePath,"Mesh property declaration/effective-owner provenance was guessed");
    Require(base.hitDamage.value == 6 && base.accurateRange.value == 80 && base.maxRange.value == 80 && base.handToHand.value == true &&
        base.playerViewScale.value == 1.0f,"Typed original damage/range/scale/hand-to-hand values were coerced or fabricated");
    const auto child = ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.Child");
    Require(child.classPath == "HeldItems.Child" && child.playerViewMesh.value == base.playerViewMesh.value &&
        child.playerViewMesh.provenance.ownerPath == "HeldItems.BaseWeapon","Inherited mesh owner collapsed to the concrete child class");
    const auto instance = ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.Override");
    RequireField(instance.playerViewMesh,"HeldAssets.Override","Instance mesh did not override class default");
    Require(instance.playerViewMesh.provenance.origin == Origin::Instance && instance.playerViewMesh.provenance.ownerPath == "HeldMap.Override" &&
        instance.playerViewScale.value == 0.75f && instance.hitDamage.value == 9,"Instance typed value/owner precedence was lost");
    RequireField(instance.multiSkins[0],"","Explicit None at MultiSkins[0] did not suppress just that inherited slot");
    RequireField(instance.multiSkins[1],"HeldAssets.Skin1","Unassigned MultiSkins[1] did not inherit independently");
    RequireField(instance.multiSkins[2],"HeldAssets.Skin2","Array cache collapsed MultiSkins[2] to the last scalar reference");
    RequireField(instance.multiSkins[3],"","Declared typed-zero texture slot was confused with absent metadata");
    Require(instance.multiSkins[3].provenance.origin == Origin::Zero,"Typed-zero texture acquired invented source owner");
    const auto none = ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.NoneView");
    RequireField(none.playerViewMesh,"","Explicit None was replaced by pickup or third-person mesh");
    const auto ordinary = ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.OrdinaryInventory");
    Require(ordinary.status == Status::Available && !ordinary.weapon && ordinary.hitDamage.status == FieldStatus::MissingPropertyMetadata &&
        !ordinary.hitDamage.value && ordinary.maxRange.status == FieldStatus::MissingPropertyMetadata,"Ordinary inventory acquired fake weapon metadata");
    const auto shadow = ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.ShadowScale");
    Require(shadow.playerViewScale.status == FieldStatus::Available && shadow.playerViewScale.value == 0.0f &&
        shadow.playerViewScale.provenance.origin == Origin::Zero && shadow.playerViewScale.provenance.declarationPath == "HeldItems.ShadowScale.PlayerViewScale",
        "Parent scale tag leaked into a newly declared typed-zero property");
    for (const char* name : {"WrongRange","WrongConstraint","NarrowLod","WrongArray","BadType","BadScalarIndex","WrongAsset","WrongPackage","ClassAsset","BadSkinIndex"}) {
        const auto value = ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap."+std::string(name));
        const auto status = std::string(name) == "WrongRange" ? value.maxRange.status :
            std::string(name) == "WrongArray" || std::string(name) == "BadSkinIndex" ? value.multiSkins[0].status : value.playerViewMesh.status;
        Require(value.status == Status::Available && status == FieldStatus::MalformedProperty,"Optional typed/reference failure did not fail closed independently: "+std::string(name));
    }
    RequireField(ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.ValidLod").playerViewMesh,"HeldAssets.LodView",
        "Actual LodMesh failed exact Mesh/native ancestry and narrow declared-class constraint");
    const auto missing = ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.MissingAsset");
    Require(missing.playerViewMesh.status == FieldStatus::MissingReferencedObject && !missing.playerViewMesh.value && missing.hitDamage.value == 6,
        "Missing referenced mesh became a fabricated asset or spoiled independent damage");
    Require(ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.NanScale").playerViewScale.status == FieldStatus::InvalidValue,
        "Nonfinite scale was returned as a valid render input");
    Require(ReadPortableRuntimeHeldItemVisualDescriptor("HeldItems.BaseWeapon").status == Status::NotInventoryActor &&
        ReadPortableRuntimeHeldItemVisualDescriptor("HeldItems.BaseWeapon@event:1").status == Status::UnindexedIdentity &&
        ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.NotActor").status == Status::NotInventoryActor &&
        ReadPortableRuntimeHeldItemVisualDescriptor("Missing.Actor").status == Status::UnindexedIdentity,
        "Class/token/non-inventory/missing identities were accepted as collected instances");
    for (const auto& invalid : {std::string{},std::string("a\0b",3u),std::string(4097u,'x')})
        Require(ReadPortableRuntimeHeldItemVisualDescriptor(invalid).status == Status::InvalidIdentity,"Malformed item identity was retained or substituted");
    for (unsigned mode = 0u; mode < 8u; ++mode) {
        PortableHeldItemDescriptorLimits limits;
        if (mode == 0u) limits.retainedBytes = 0u;
        if (mode == 1u) limits.retainedBytes = 1u;
        if (mode == 2u) limits.stringBytes = 0u;
        if (mode == 3u) limits.hierarchy = 0u;
        if (mode == 4u) limits.hierarchy = 129u;
        if (mode == 5u) limits.inventoryEntries = 0u;
        if (mode == 6u) limits.inventoryEntries = 1u;
        if (mode == 7u) limits.retainedBytes = 1024u*1024u+1u;
        Reject([&] { (void)ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.Default",limits); },"Invalid/exhausted held descriptor bounds accepted");
    }
    PortableHeldItemDescriptorLimits shallow; shallow.hierarchy = 1u;
    Require(ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.Default",shallow).status == Status::MalformedIdentity,"Actor ancestry depth bound was bypassed");
    base.playerViewMesh.value = "Detached";
    RequireField(ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.Default").playerViewMesh,"HeldAssets.View","Returned mesh path aliases runtime state");
    Require(fixture.Snapshot("After") == before && GetPortableRuntimeWorldRevision() == revision &&
        GC::GetStats().numObjects == stats.numObjects && GC::GetStats().memoryUsage == stats.memoryUsage,
        "Read-only held metadata changed checkpoint/world revision/GC graph");

    QuestVr::ScriptSavedState saved; saved.mapName = "HeldMap";
    QuestVr::ScriptSavedObject actor; actor.path = "HeldMap.Default"; actor.classPath = "HeldItems.BaseWeapon";
    actor.properties.push_back({"Engine.Inventory.PlayerViewMesh","PlayerViewMesh",0u,Value::Text(Kind::Object,"HeldAssets.LodView")});
    actor.properties.push_back({"Engine.Inventory.PlayerViewScale","PlayerViewScale",0u,Value::Float(0.5f)});
    actor.properties.push_back({"Engine.Weapon.HitDamage","HitDamage",0u,Value::Integer(11)});
    actor.properties.push_back({"Engine.Actor.MultiSkins","MultiSkins",1u,Value::Text(Kind::Object,"HeldAssets.Skin2")});
    saved.objects.push_back(std::move(actor));
    // CDO patches are class-owned storage, not retroactive instance changes.
    // Map actors retain serialized defaults; new births separately freeze the
    // concrete CDO. Do not let the detached held reader leak current CDO state.
    saved.classDefaults.push_back({"HeldItems.BaseWeapon",{
        {"Engine.Inventory.PlayerViewMesh","PlayerViewMesh",0u,Value::Text(Kind::Object,"HeldAssets.Override")},
        {"Engine.Inventory.PlayerViewScale","PlayerViewScale",0u,Value::Float(0.25f)},
        {"Engine.Weapon.HitDamage","HitDamage",0u,Value::Integer(99)},
        {"Engine.Actor.MultiSkins","MultiSkins",1u,Value::Text(Kind::Object,"HeldAssets.Skin2")}}});
    saved.classDefaults.push_back({"HeldItems.ShadowScale",{
        {"HeldItems.ShadowScale.PlayerViewScale","PlayerViewScale",0u,Value::Float(0.375f)}}});
    auto envelope = before; const auto blob = QuestVr::EncodeScriptSavedState(saved); Replace32(envelope,4u,3u+blob.at(6u));
    U32(envelope,static_cast<std::uint32_t>(blob.size())); envelope.insert(envelope.end(),blob.begin(),blob.end());
    const auto path = fixture.directory/"Overlay.sav";
    Require(QuestVr::WriteDurableSaveFile(path.string(),envelope) && LoadPortableRuntimeState(path.string()),"Typed held-visual overlay fixture failed schema restore");
    const auto overlayBefore = fixture.Snapshot("OverlayBefore"); const auto overlayRevision = GetPortableRuntimeWorldRevision();
    const auto overlay = ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.Default");
    RequireField(overlay.playerViewMesh,"HeldAssets.LodView","Actor overlay did not override inactive held-view mesh");
    Require(overlay.playerViewMesh.provenance.origin == Origin::Overlay && overlay.playerViewMesh.provenance.ownerPath == "HeldMap.Default" &&
        overlay.playerViewScale.value == 0.5f && overlay.hitDamage.value == 11,"Typed overlay scalar/source provenance was lost");
    RequireField(overlay.multiSkins[1],"HeldAssets.Skin2","Per-slot overlay failed texture lookup");
    Require(ReadPortableClassDefault("HeldItems.BaseWeapon","HitDamage").integer==99 &&
        ReadPortableClassDefault("HeldItems.ShadowScale","PlayerViewScale").floating==0.375f,
        "CDO isolation control did not restore actual independent mutable defaults");
    const auto unchangedChild=ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.Child");
    RequireField(unchangedChild.playerViewMesh,"HeldAssets.View","Live CDO mesh patch leaked into an existing collected map actor");
    Require(unchangedChild.hitDamage.value==6 && unchangedChild.playerViewScale.value==1.0f &&
        unchangedChild.playerViewMesh.provenance.origin==Origin::ClassDefault,
        "Current CDO scalar patch replaced an existing actor's authored defaults");
    RequireField(unchangedChild.multiSkins[1],"HeldAssets.Skin1","Live CDO skin slot leaked into an existing map actor");
    Require(ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.ShadowScale").playerViewScale.value==0.0f,
        "Mutable shadow CDO patch replaced the existing actor's correctly qualified typed zero");
    Require(fixture.Snapshot("OverlayAfter") == overlayBefore && GetPortableRuntimeWorldRevision() == overlayRevision,
        "Held-visual overlay inspection mutated persistence/publication");
    Require(ConsumePortableRuntimeInventoryItem("HeldMap.Default") &&
        ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.Default").status == Status::NotCollected,
        "Consumed identity was still represented as a collected item");
    fixture.SourcesUnchanged();
    std::cout << "PASS generated held-item metadata with " << (localAssetClasses ? "indexed local" : "exact imported native")
              << " Mesh/LodMesh/Texture constraints; no equipment/render/startup claim\n";
}
std::uint64_t SourceHash(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary); Require(static_cast<bool>(file),"Cannot inspect owned source bytes");
    std::uint64_t hash = 1469598103934665603ull; std::array<char,16384u> buffer{};
    std::uintmax_t total{};
    while (file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
        const auto count = file.gcount(); total += static_cast<std::uintmax_t>(count);
        Require(total <= 512u*1024u*1024u,"Owned source inspection exceeds bound");
        for (std::streamsize index = 0; index < count; ++index) { hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]); hash *= 1099511628211ull; }
    }
    Require(file.eof(),"Owned source read failed"); return hash;
}
void Original(const std::filesystem::path& root) {
    struct Source { std::filesystem::path path; std::uintmax_t size; std::filesystem::file_time_type time; std::uint64_t hash; };
    std::vector<Source> sources; std::vector<PortablePackageTables> packages;
    for (const char* name : {"Core","Engine","DeusEx","DeusExItems"}) {
        const auto path = std::filesystem::canonical(root/"System"/(std::string(name)+".u"));
        sources.push_back({path,std::filesystem::file_size(path),std::filesystem::last_write_time(path),SourceHash(path)});
        packages.push_back(LoadPortablePackageTables(path.string()));
    }
    // Decode owned art before equipment selection, just as the preparation
    // worker does. No selector call below is allowed to open an asset package.
    const auto artwork=QuestVr::LoadOriginalHeldItemAssets(root.string());
    Require(artwork.passed,"Original held artwork preparation failed: "+artwork.error);
    std::array<QuestVr::PreparedHeldVisualKey,3u> prepared;
    for (std::size_t index=0u;index<artwork.variants.size();++index) {
        const auto& asset=artwork.variants[index];auto& key=prepared[index];
        key.classPath=asset.classPath;key.playerViewMesh=asset.meshPath;
        key.playerViewScale=asset.playerViewScale;key.skin=asset.materialOverrides.skin.path;
        key.supportsNativeStarterKeyRing=asset.supportsNativeStarterKeyRing;
        for (std::size_t slot=0u;slot<key.multiSkins.size();++slot)
            key.multiSkins[slot]=asset.materialOverrides.multiSkins[slot].path;
        Require(!asset.rightHandAndItem.triangles.empty() && !asset.textures.empty(),
            "Prepared binding is not backed by an actual original hand/item assembly");
    }
    Fixture fixture; Package map; map.stem = "OriginalHeldMetadata";
    const auto deusEx=map.ImportPackage("DeusEx");
    for (const auto& [name,className]:std::array<std::pair<const char*,const char*>,2u>{{
        {"ActualCrowbar0","WeaponCrowbar"},{"ActualCombatKnife0","WeaponCombatKnife"}}}) {
        const auto ref=map.Export(name,map.Import(className,deusEx));
        Bytes actorProperties;map.Int(actorProperties,"invPosX",-1);map.Int(actorProperties,"invPosY",-1);Index(actorProperties,0);
        map.Body(ref,std::move(actorProperties));
    }
    const auto table = fixture.Write(map,true); InitializePortableRuntime(packages);
    Require(LoadPortableRuntimeMap(table).passed,"Original Crowbar/CombatKnife classes did not bind generated actors");
    Require(ReadPortableRuntimeHeldItemVisualDescriptor("OriginalHeldMetadata.ActualCrowbar0").status == Status::NotCollected,
        "Uncollected owned-data actor acquired a held/equipped claim");
    Require(ReadPortableRuntimeHeldItemVisualDescriptor("OriginalHeldMetadata.ActualCombatKnife0").status == Status::NotCollected,
        "Uncollected actual CombatKnife acquired a held/equipped claim");
    Require(GetPortableRuntimeEquippedItem()==QuestVr::kQuickBeltKeyRingIdentity && PortableRuntimeKeyRingEquipped(),
        "Fresh equipment did not start with the functional native keyring");
    Require(ReadPortableRuntimeHeldItemVisualDescriptor(QuestVr::kQuickBeltKeyRingIdentity).status==Status::UnindexedIdentity,
        "Native starter ring was fabricated as an indexed original UObject");
    const auto starter=QuestVr::SelectEquippedItemVisual(GetPortableRuntimeEquippedItem(),nullptr,prepared);
    Require(starter.status==QuestVr::EquippedItemVisualStatus::Available && starter.preparedIndex==0u &&
        starter.source==QuestVr::EquippedItemVisualSource::NativeStarterKeyRing &&
        prepared[0].classPath=="DeusEx.NanoKeyRing" && prepared[0].playerViewMesh=="DeusExItems.NanoKeyRingPOV",
        "Functional native ring did not select its explicit original POV assembly without an actor descriptor");
    Require(InteractPortableRuntimeActor("OriginalHeldMetadata.ActualCrowbar0").action == "pickup","Actual owned Crowbar class was not collectible");
    Require(InteractPortableRuntimeActor("OriginalHeldMetadata.ActualCombatKnife0").action == "pickup",
        "Actual owned CombatKnife class was not collectible");
    Require(EquipPortableRuntimeInventoryItem("OriginalHeldMetadata.ActualCrowbar0") &&
        GetPortableRuntimeEquippedItem()=="OriginalHeldMetadata.ActualCrowbar0",
        "Collected actual Crowbar could not be equipped independently of the Persona cursor");
    const auto before = fixture.Snapshot("OriginalBefore"); const auto revision = GetPortableRuntimeWorldRevision(); const auto stats = GC::GetStats();
    const auto value = ReadPortableRuntimeHeldItemVisualDescriptor("originalheldmetadata.actualcrowbar0");
    Require(value.status == Status::Available && value.collected && !value.active && value.weapon &&
        value.actorPath == "OriginalHeldMetadata.ActualCrowbar0" && value.classPath == "DeusEx.WeaponCrowbar","Original Crowbar actor identity was fabricated");
    Require(value.hitDamage.status == FieldStatus::Available && value.hitDamage.value == 6 &&
        value.accurateRange.status == FieldStatus::Available && value.accurateRange.value == 80 &&
        value.maxRange.status == FieldStatus::Available && value.maxRange.value == 80 && value.handToHand.value == true,
        "Original Crowbar Int damage6/AccurateRange80/MaxRange80 were coerced or replaced with VR convenience values");
    RequireField(value.playerViewMesh,"DeusExItems.Crowbar","Original PlayerViewMesh is not the exact declared asset");
    RequireField(value.pickupViewMesh,"DeusExItems.CrowbarPickup","Original pickup mesh reference changed");
    RequireField(value.thirdPersonMesh,"DeusExItems.Crowbar3rd","Original third-person mesh reference changed");
    Require(value.playerViewMesh.provenance.declarationPath == "Engine.Inventory.PlayerViewMesh" &&
        value.playerViewMesh.provenance.declarationSourcePath == packages[1].sourcePath &&
        value.playerViewMesh.provenance.ownerPath == "DeusEx.WeaponCrowbar" &&
        value.playerViewMesh.provenance.ownerSourcePath == packages[2].sourcePath &&
        value.playerViewMesh.provenance.origin == Origin::ClassDefault,"Original Crowbar mesh declaration/effective-owner provenance was guessed");
    const auto knife=ReadPortableRuntimeHeldItemVisualDescriptor("OriginalHeldMetadata.ActualCombatKnife0");
    Require(knife.status==Status::Available && knife.collected && !knife.active && knife.weapon &&
        knife.actorPath=="OriginalHeldMetadata.ActualCombatKnife0" && knife.classPath=="DeusEx.WeaponCombatKnife",
        "Actual inactive collected CombatKnife identity/class was fabricated or lost");
    RequireField(knife.playerViewMesh,"DeusExItems.CombatKnife","Actual CombatKnife POV mesh was guessed from a filename");
    RequireField(knife.pickupViewMesh,"DeusExItems.CombatKnifePickup","Original CombatKnife pickup mesh reference changed");
    RequireField(knife.thirdPersonMesh,"DeusExItems.CombatKnife3rd","Original CombatKnife third-person mesh reference changed");
    const auto requireFingerprint=[&](const PortableHeldItemVisualDescriptor& descriptor,std::size_t index) {
        const auto& key=prepared[index];
        Require(descriptor.classPath==key.classPath && descriptor.playerViewScale.status==FieldStatus::Available &&
            descriptor.playerViewScale.value && *descriptor.playerViewScale.value==key.playerViewScale &&
            std::isfinite(*descriptor.playerViewScale.value) && *descriptor.playerViewScale.value>0.0f,
            "Actual collected view scale/class differs from the prepared original metadata");
        RequireField(descriptor.skin,key.skin,"Actual collected Skin differs from the effective prepared original material");
        for (std::size_t slot=0u;slot<key.multiSkins.size();++slot)
            RequireField(descriptor.multiSkins[slot],key.multiSkins[slot],
                "Actual collected MultiSkins slot differs from prepared original typed effective material");
        const auto selected=QuestVr::SelectEquippedItemVisual(descriptor.actorPath,&descriptor,prepared);
        Require(selected.status==QuestVr::EquippedItemVisualStatus::Available && selected.preparedIndex==index &&
            selected.source==QuestVr::EquippedItemVisualSource::CollectedActor,
            "Exact live collected descriptor did not select the already prepared original hand/item assembly");
        auto changed=descriptor;*changed.playerViewScale.value*=0.5f;
        const auto mismatch=QuestVr::SelectEquippedItemVisual(descriptor.actorPath,&changed,prepared);
        Require(mismatch.status==QuestVr::EquippedItemVisualStatus::PreparedAssetMismatch && !mismatch.preparedIndex,
            "Changed effective live view scale selected stale predecoded geometry");
    };
    requireFingerprint(value,2u);requireFingerprint(knife,1u);
    const auto melee=QuestVr::ResolveEquippedMeleeProfile(value.actorPath,&value);
    Require(melee==QuestVr::EquippedMeleeProfile{6,80,80},
        "Actual Crowbar melee profile changed original integer damage/range units");
    Require(knife.hitDamage.status==FieldStatus::Available && knife.hitDamage.value &&
        knife.accurateRange.status==FieldStatus::Available && knife.accurateRange.value &&
        knife.maxRange.status==FieldStatus::Available && knife.maxRange.value &&
        knife.handToHand.status==FieldStatus::Available && knife.handToHand.value==true &&
        QuestVr::ResolveEquippedMeleeProfile(knife.actorPath,&knife)==QuestVr::EquippedMeleeProfile{
            *knife.hitDamage.value,*knife.accurateRange.value,*knife.maxRange.value},
        "Actual CombatKnife typed melee fields are unavailable or differ from its detached profile");
    Require(fixture.Snapshot("OriginalAfter") == before && GetPortableRuntimeWorldRevision() == revision &&
        GC::GetStats().numObjects == stats.numObjects && GC::GetStats().memoryUsage == stats.memoryUsage,
        "Owned-data metadata read changed runtime checkpoint/revision/GC");
    // The presentation cache must compare with reconciled live equipment,
    // never reuse a previous descriptor after an equip, consume or map restore.
    Require(EquipPortableRuntimeInventoryItem(knife.actorPath),"Actual collected CombatKnife could not be equipped");
    const auto wrongItem=QuestVr::SelectEquippedItemVisual(GetPortableRuntimeEquippedItem(),&value,prepared);
    Require(wrongItem.status==QuestVr::EquippedItemVisualStatus::StaleDescriptor && !wrongItem.preparedIndex,
        "Equipping CombatKnife reused the previous Crowbar descriptor/model");
    const auto equippedKnife=QuestVr::SelectEquippedItemVisual(GetPortableRuntimeEquippedItem(),&knife,prepared);
    Require(equippedKnife.preparedIndex==1u,"Actually equipped CombatKnife did not select its original assembly");
    Require(ConsumePortableRuntimeInventoryItem(knife.actorPath),"Owned CombatKnife consume control failed");
    const auto removed=ReadPortableRuntimeHeldItemVisualDescriptor(knife.actorPath);
    Require(removed.status==Status::NotCollected,"Consumed actual CombatKnife still reports collected ownership");
    const auto refused=QuestVr::SelectEquippedItemVisual(knife.actorPath,&removed,prepared);
    Require(refused.status==QuestVr::EquippedItemVisualStatus::DescriptorUnavailable && !refused.preparedIndex,
        "Fresh consumed descriptor retained the old held knife model");
    const auto fallbackIdentity=GetPortableRuntimeEquippedItem();
    const auto fallback=QuestVr::SelectEquippedItemVisual(fallbackIdentity,&knife,prepared);
    Require(fallbackIdentity==QuestVr::kQuickBeltKeyRingIdentity && fallback.preparedIndex==0u &&
        fallback.source==QuestVr::EquippedItemVisualSource::NativeStarterKeyRing,
        "Consumption kept the removed knife assembly instead of reconciling to the explicit native keyring");
    Require(!QuestVr::ResolveEquippedMeleeProfile(fallbackIdentity,&knife),
        "Native ring inherited stale consumed-knife melee behavior");
    for (const auto& source : sources) Require(std::filesystem::file_size(source.path) == source.size &&
        std::filesystem::last_write_time(source.path) == source.time && SourceHash(source.path) == source.hash,
        "Owned package bytes or timestamps changed");
    fixture.SourcesUnchanged();
    std::cout << "PASS owned Crowbar+CombatKnife actual collected/equipped actors, exact prepared original mesh/scale/Skin/MultiSkins selection; "
        "explicit native ring without fake UObject; stale/consumed model refusal; source bytes unchanged; not tracked render/original Fire evidence\n";
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc != 1 && !(argc == 3 && std::string(argv[1]) == "--game-root"))
            throw std::runtime_error("Usage: portable_held_item_descriptor_test [--game-root ORIGINAL_GAME_ROOT]");
        ShutdownPortableRuntime();
        Require(ReadPortableRuntimeHeldItemVisualDescriptor("HeldMap.Default").status == Status::RuntimeUnavailable,
            "Unavailable runtime was represented as a collected actor");
        Synthetic(false); Synthetic(true);
        if (argc == 3) Original(argv[2]);
        std::cout << "PASS held-item descriptor " << checks << " checks, " << rejections << " bounds refusals\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; ShutdownPortableRuntime(); return 1;
    }
}

