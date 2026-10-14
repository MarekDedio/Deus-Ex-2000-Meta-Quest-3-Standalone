#include "portable_unreal_runtime.h"

#include "GC/GC.h"

#include "portable_log.h"
#include "quest_script_state.h"
#include "quest_save_bundle.h"
#include "portable_model_geometry.h"
#include "quest_actor_animation_clock.h"
#include "quest_authored_struct_value.h"
#include "quest_object_cast.h"
#include "quest_spawn_placement.h"
#include "quest_actor_overlap.h"
#include "quest_ai_event_state.h"
#include "quest_script_random.h"
#include "quest_actor_latent.h"
#include "quest_actor_timer.h"
#include "quest_actor_state_control.h"
#include "quest_unreal_string.h"
#include "quest_decal_checkpoint.h"
#include "quest_decal_projection.h"
#include "quest_unreal_int_array.h"
#include "quest_mover_motion.h"
#include "quest_object_names.h"
#include "quest_engine_config.h"

#include <memory>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "Math/coords.h"
#include "Math/bbox.h"

namespace {

class RuntimeObject;
class RuntimeAIManager;
class RuntimeMapDecals;
struct RuntimeMoverMotion {
    QuestVr::MoverMotionSpec spec;
    QuestVr::MoverMotionState state;
    std::optional<QuestVr::MoverMotionState> beforeStep;
    bool timedReturn{};
    // Transient render-publication work, not persisted movement state. A
    // command can jump directly to Idle (MoveTime==0) without a ticking delta.
    bool commandPublicationPending{};
};
struct InventoryIconClassConstraint {
    RuntimeObject* indexedClass{}; // Non-owning immutable metadata, not a new GC root.
    std::uint8_t nativeClass{};
    bool valid{};
};
struct RuntimeNativeWindowState {
    // Retail native TArray<Window*> storage is not a script IntArray, nor a
    // serializable pointer-shaped ExtensionObject.DynamicArray struct.
    std::vector<RuntimeObject*> rowMajor, colMajor, tabGroups;
    bool dirtyTabOrder{}; // Native TabGroup +164 bit4, no script declaration.
};

class RuntimeObject : public GCObject {
public:
    RuntimeObject(
        PortableReflectionObject reflection,
        std::size_t* destroyed)
        : reflection(std::move(reflection)), destroyed_(destroyed) {}

    PortableReflectionObject reflection;
    std::vector<RuntimeObject*> references;
    RuntimeObject* outer{};
    RuntimeObject* base{};
    RuntimeObject* cls{};
    // A class-owned default block has UObject identity but no map/export slot,
    // Actor lifecycle, serialized stack, or runtime-birth identity. Its owner
    // holds the GC edge; mutable reflected data stays on that concrete UClass.
    RuntimeObject* defaultOwner{};
    RuntimeObject* defaultObject{};
    // Engine-session native UObjects/classes are not authored exports or map
    // births. Viewport.Actor / PlayerPawn.Player stay native transient edges,
    // never serialized pointer-shaped script patches or guessed possession.
    bool nativeClientClass{};
    std::uint64_t nativeClientIdentity{}, nativeViewportIdentity{};
    RuntimeObject* nativePlayer{};
    RuntimeObject* nativeViewportActor{};
    bool nativePlayerBinding{};
    // Native Extension window-domain objects are neither Actor births nor
    // package exports. RootWindow is a transient native player edge; its
    // AddToRoot never changes flags; Core still applies CLASS_Transient.
    bool nativeWindowObject{}, nativeRootWindowObject{};
    RuntimeObject* nativeRootWindow{};
    std::optional<RuntimeNativeWindowState> nativeWindowState;
    // Exact reflected slot ownership, including nested Struct Object fields.
    // Replaceable window edges must not accumulate in immutable references.
    std::unordered_map<std::string,
        std::unordered_map<std::uint32_t,std::vector<RuntimeObject*>>> nativeWindowPropertyRoots;
    std::int32_t uniqueObjectCounter{}; // Concrete UClass, survives map changes.
    RuntimeObject* nativeXLevel{}; // Immutable LinkActorsToLevel baseline, not a script write.
    RuntimeAIManager* nativeAiManager{}; // LevelInfo-owned native state, not an actor/export.
    std::unique_ptr<PortableScriptBody> script;
    std::unique_ptr<PortablePropertyDescriptor> property;
    std::unique_ptr<PortableClassDescriptor> classDescriptor;
    std::unique_ptr<PortableStateDescriptor> stateDescriptor;
    std::unique_ptr<PortableStructDescriptor> structDescriptor;
    // Keep the exact loaded record; class-backed offset -1 records are not
    // named-state continuations. No execution state is inferred from this.
    std::optional<PortableObjectStack> serializedStack;
    RuntimeObject* serializedStateCode{}; // Outward GC-linked authored identity only.
    RuntimeObject* serializedFunctionCode{};
    std::optional<PortableFieldLinks> commonFieldLinks;
    // Validated once while the original tables are available. Outer/path
    // coincidence alone cannot authorize a compiled class-property alias.
    // linkedClassOwner aliases the already GC-linked immutable Outer.
    RuntimeObject* linkedClassOwner{};
    bool classFieldLinksValid{};
    // Exact immutable table identities, captured only at load while its source
    // table is already available. A read-only inventory query must not reopen
    // packages or treat reflection's package-stripped metaclass as an IsA proof.
    bool inventoryTableMetadata{}, inventorySerializedClass{}, inventoryClassReferenceValid{}, inventoryBaseReferenceValid{};
    std::uint8_t inventoryNativeClass{}, inventoryNativeBase{};
    std::unique_ptr<InventoryIconClassConstraint> inventoryIconClassConstraint;
    std::vector<PortableTaggedProperty> instanceProperties;
    std::unordered_map<std::string, std::string> objectPropertyPaths;
    // Name indices are package-local. Decode them while their actual package
    // is available, rather than accidentally using a map's name table for an
    // inherited Engine/DeusEx class default. Array slots inherit separately.
    std::unordered_map<std::string,
        std::unordered_map<std::uint32_t, std::string>> namePropertyValues;
    QuestVr::ActorTextureOverrides textureOverrides;
    std::string sourcePath;
    std::size_t exportIndex{};
    std::unique_ptr<PortableLodMesh> lodMesh;
    std::unique_ptr<PortableLodMesh> brushMesh;
    std::unordered_map<std::string,
        std::unordered_map<std::uint32_t, QuestVr::Vm::Value>> scriptValues;
    // Mutable storage belongs to this concrete UClass's copied default block.
    // Existing instances/derived classes keep their immutable authored copy;
    // they must never fall through to later mutations on this or a base CDO.
    decltype(scriptValues) classDefaultValues;
    // A runtime birth shares only immutable authored class metadata. Copy the
    // concrete CDO's complete mutable patch block once; later CDO writes never
    // affect this instance. Births have no serialized map export index.
    decltype(scriptValues) frozenBirthDefaults;
    bool runtimeBirth{};
    std::string birthVmKey;
    std::optional<QuestVr::ActorAnimationClock> animationClock;
    std::optional<QuestVr::StateObject> stateObject;
    // Native AActor latent timer is actor-owned, not a state-frame local.
    // It remains across GotoState, null code and stopped frames.
    std::optional<float> latentTimeLeft;
    // Native renderer timestamp, not an invented UnrealScript property.
    // World-startup probes journal it and never publish an incomplete world.
    std::optional<float> lastRenderTime;
    // Nonserialized local-storage identity. References held by an executing
    // expression cannot silently address replacement state locals.
    std::uint64_t stateLocalsRevision{};
    // Original state-changed / in-EndState flags, journaled with the frame.
    // These control synchronous reentrancy, not a transition generation.
    std::uint32_t stateTransitionFlags{};
    bool committedScriptState{};
    // Native ordered relationships are not inferred from arbitrary Owner/Base
    // assignments. Destroy removes the world slot, not the UObject lifetime.
    std::vector<RuntimeObject*> childActors, basedActors, initialBasedActors;
    std::array<bool, 4u> touchEventSent{};
    std::optional<std::uint8_t> initialStandingCount;
    bool committedLifecycle{}, worldRemoved{};
    std::int32_t worldActorIndex{-1};
    bool active{true};
    bool activated{};
    std::optional<RuntimeMoverMotion> moverMotion;
    bool healthInitialized{};
    float health{100.0f};

protected:
    ~RuntimeObject() override {
        if (destroyed_ != nullptr) ++*destroyed_;
    }

    GCAllocation* Mark(GCAllocation* marklist) override;

private:
    std::size_t* destroyed_{};
};

class RuntimeAIManager final : public GCObject {
public:
    QuestVr::Ai::State state;
    RuntimeObject* owner{};
    RuntimeObject* level{};
    // Includes pending-deletion nodes and previous/current best actors. The
    // symbolic graph is serializable; these edges preserve UObject lifetimes.
    std::vector<RuntimeObject*> actorRoots;
protected:
    GCAllocation* Mark(GCAllocation* marklist) override {
        marklist = GC::MarkObject(marklist, owner);
        marklist = GC::MarkObject(marklist, level);
        for (auto* actor : actorRoots) marklist = GC::MarkObject(marklist, actor);
        return marklist;
    }
};

// Native surface records live with their loaded map, not a temporary VM host.
// Symbolic owner names are checkpoint identities; these actual GC edges keep
// even pending-deletion Decals alive while a surface still references them.
class RuntimeMapDecals final : public GCObject {
public:
    RuntimeMapDecals(std::size_t surfaces, std::size_t nodes) : state(surfaces, nodes) {}
    QuestVr::DecalState state;
    RuntimeObject* level{};
    RuntimeObject* model{};
    std::vector<RuntimeObject*> actorRoots;
protected:
    GCAllocation* Mark(GCAllocation* marklist) override {
        marklist = GC::MarkObject(marklist, level);
        marklist = GC::MarkObject(marklist, model);
        for (auto* actor : actorRoots) marklist = GC::MarkObject(marklist, actor);
        return marklist;
    }
};

std::size_t NativeDecalRetainedBytes(const RuntimeMapDecals& graph) {
    const auto snapshot = QuestVr::DecalSnapshotRetainedBytes(graph.state.ReadOnlySnapshot(),
        graph.state.Usage().surfaces, graph.state.NodeCount());
    return QuestVr::DecalStateDetail::Add(graph.state.TransientRetainedBytes(),
        QuestVr::DecalStateDetail::Add(snapshot, QuestVr::DecalStateDetail::Add(
        sizeof(RuntimeMapDecals) + sizeof(GCAllocation),
        QuestVr::DecalStateDetail::Multiply(graph.actorRoots.capacity(), sizeof(RuntimeObject*)))));
}

GCAllocation* RuntimeObject::Mark(GCAllocation* marklist) {
    marklist = GC::MarkObject(marklist, defaultObject);
    marklist = GC::MarkObject(marklist, defaultOwner);
    marklist = GC::MarkObject(marklist, nativePlayer);
    marklist = GC::MarkObject(marklist, nativeViewportActor);
    marklist = GC::MarkObject(marklist, nativeRootWindow);
    for (auto* reference : references) marklist = GC::MarkObject(marklist, reference);
    for (const auto& [key,slots] : nativeWindowPropertyRoots) {
        static_cast<void>(key);
        for (const auto& [index,roots] : slots) {
            static_cast<void>(index);
            for (auto* root : roots) marklist = GC::MarkObject(marklist,root);
        }
    }
    if(nativeWindowState) {
        for(const auto* list:{&nativeWindowState->rowMajor,&nativeWindowState->colMajor,&nativeWindowState->tabGroups})
            for(auto* window:*list) marklist=GC::MarkObject(marklist,window);
    }
    for (auto* actor : childActors) marklist = GC::MarkObject(marklist, actor);
    for (auto* actor : basedActors) marklist = GC::MarkObject(marklist, actor);
    return GC::MarkObject(marklist, nativeAiManager);
}

class RuntimePackage final : public GCObject {
public:
    explicit RuntimePackage(std::size_t* destroyedPackage)
        : destroyedPackage_(destroyedPackage) {}

    std::vector<RuntimeObject*> exports;
    std::size_t structDescriptorBytes{};
    RuntimeMapDecals* nativeDecals{};

protected:
    ~RuntimePackage() override {
        if (destroyedPackage_ != nullptr) ++*destroyedPackage_;
    }

    GCAllocation* Mark(GCAllocation* marklist) override {
        for (RuntimeObject* object : exports) {
            marklist = GC::MarkObject(marklist, object);
        }
        return GC::MarkObject(marklist, nativeDecals);
    }

private:
    std::size_t* destroyedPackage_{};
};

// UClass owns its default block from load, not from first bytecode access.
// Deterministic allocation prevents a failed/over-budget script transaction
// from introducing an unjournaled new GC root or observable identity lifetime.
std::size_t MaterializeClassDefaultObjects(RuntimePackage* runtime) {
    std::size_t count{};
    for (auto* cls : runtime->exports) {
        if (!cls || !cls->classDescriptor) continue;
        if (!cls->defaultObject) {
            PortableReflectionObject metadata{};
            metadata.objectPath="@ClassDefault:"+cls->reflection.objectPath;
            metadata.metaClass=cls->reflection.objectPath.substr(cls->reflection.objectPath.find_last_of('.')+1u);
            auto* object=GC::Alloc<RuntimeObject>(std::move(metadata),nullptr);
            object->defaultOwner=cls; object->cls=cls;
            object->sourcePath=cls->sourcePath;
            object->exportIndex=std::numeric_limits<std::size_t>::max();
            cls->defaultObject=object;
        }
        ++count;
    }
    return count;
}

std::unique_ptr<GCRoot<RuntimePackage>> persistentRuntime;
// Actual embedding-owned engine client/render-target registrations. No UObject
// or map actor is synthesized; the render owner revokes only its atomic lease.
QuestVr::NativeClientRegistry persistentNativeClient;
struct RuntimePlayerSession final : GCObject {
    RuntimeObject* client{};
    std::vector<RuntimeObject*> classes, viewports;
protected:
    GCAllocation* Mark(GCAllocation* marklist) override {
        marklist=GC::MarkObject(marklist,client);
        for(auto* object:classes) marklist=GC::MarkObject(marklist,object);
        for(auto* object:viewports) marklist=GC::MarkObject(marklist,object);
        return marklist;
    }
};
std::unique_ptr<GCRoot<RuntimePlayerSession>> persistentPlayerSession;
struct RuntimeWindowDomain final : GCObject {
    // Current domain consists only of explicitly rooted windows and their
    // native GC objects. General EX_New/non-root object collection is separate.
    std::vector<RuntimeObject*> objects, roots;
protected:
    GCAllocation* Mark(GCAllocation* marklist) override {
        for(auto* root:roots) marklist=GC::MarkObject(marklist,root);
        return marklist;
    }
};
std::unique_ptr<GCRoot<RuntimeWindowDomain>> persistentWindowDomain;
std::optional<std::string> persistentRootWindowClass;
// One game/script-thread stream, shared by all receivers and retained through
// ordinary map replacement. Absence means the deterministic seed1 baseline.
// Every nonbaseline initial seed is retained before the first draw, so even
// pre-draw checkpoints and map rollback preserve it. Quest supplies time once.
std::optional<std::uint32_t> persistentScriptRandomSeed;
std::unordered_map<std::string, RuntimeObject*> persistentQualifiedObjects;
// UE names are case-insensitive. Keep the VM lookup alongside the owning
// runtime index, rather than copying/lowercasing every export for every call.
// These aliases are not additional GC roots and are removed before collection.
std::unordered_map<std::string, RuntimeObject*> persistentVmObjects;
std::unique_ptr<QuestVr::ScriptDispatch::Graph> persistentDispatchGraph;
PortableScriptDispatchSummary persistentDispatchSummary;
std::unordered_map<std::string, std::vector<RuntimeObject*>> persistentMapTagIndex;
std::size_t persistentScriptExportCount{};
std::string persistentMapPackageName;
std::uint64_t persistentWorldRevision{};
std::vector<RuntimeObject*> persistentWorldActors;
std::size_t persistentAuthoredWorldSlots{};
std::string persistentMapSourcePath;
QuestVr::ActorCollisionRegistry persistentActorCollision;
bool pendingBirthGarbage{};
// Collect only after the outer host/restore staging roots have been destroyed.
// Collecting from a nested native callback could reclaim an in-flight actor.
struct CollectBirthGarbageAtBoundary {
    ~CollectBirthGarbageAtBoundary() {
        if (pendingBirthGarbage) { pendingBirthGarbage = false; GC::Collect(); }
    }
};
std::vector<std::string> persistentInventory;
float persistentPlayerHealth{100.0f};
std::int32_t persistentCredits{};
std::int32_t persistentSkillPoints{};
std::unordered_map<std::string, bool> persistentConversationFlags;
std::vector<std::string> persistentGoals;
std::vector<std::string> persistentNotes;
std::unordered_set<std::string> persistentAppliedDialogueEffects;
PortableActorMeshSummary DecodeRuntimeActorAssets(const std::vector<PortableActorSnapshot>& actors);

bool SameInventoryIdentity(const std::string_view left, const std::string_view right) {
    if (left.size() != right.size()) return false;
    const auto fold = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    for (std::size_t i = 0u; i < left.size(); ++i)
        if (fold(static_cast<unsigned char>(left[i])) != fold(static_cast<unsigned char>(right[i]))) return false;
    return true;
}
const char* InventoryNativeClassPath(const std::uint8_t id) {
    // Exact Texture and held-view Mesh native registrations/bases;
    // no bare metaclass/suffix or package-name guess supplies a class identity.
    static constexpr const char* paths[] = {"", "Core.Object", "Engine.Bitmap", "Engine.Texture", "Engine.FractalTexture",
        "Engine.FireTexture", "Engine.IceTexture", "Engine.WaterTexture", "Engine.WaveTexture", "Engine.WetTexture", "Engine.ScriptedTexture",
        "Engine.Mesh", "Engine.LodMesh", "Engine.Primitive"};
    return id < std::size(paths) ? paths[id] : "";
}
std::uint8_t InventoryNativeClassId(const std::string_view path) {
    for (std::uint8_t id = 1u; id <= 13u; ++id) if (SameInventoryIdentity(path, InventoryNativeClassPath(id))) return id;
    return 0u;
}
std::uint8_t InventoryNativeClassBase(const std::uint8_t id) {
    static constexpr std::uint8_t bases[] = {0u, 0u, 1u, 2u, 3u, 4u, 4u, 4u, 7u, 7u, 3u, 13u, 11u, 1u};
    return id < std::size(bases) ? bases[id] : 0u;
}
void CacheInventoryTableMetadata(RuntimeObject* object, const PortablePackageTables& table, const ExportTableEntry& entry) {
    object->inventoryTableMetadata = true; object->inventorySerializedClass = entry.ObjClass == 0;
    const auto classReference = [&](const std::int32_t reference, bool& valid) {
        valid = false;
        try {
            if (reference == 0 || (reference > 0 &&
                table.exports.at(static_cast<std::size_t>(reference - 1)).ObjClass != 0)) return std::string{};
            auto path = ResolvePortableValueObjectReference(table, reference,
                [](const std::string&, const std::string& importedClass) { return SameInventoryIdentity(importedClass, "Class"); });
            // The value resolver already qualifies same-package exports.
            // Prefixing again would turn Engine.Texture into Engine.Engine.Texture.
            valid = !path.empty() && path.size() <= 8192u && path.find('\0') == std::string::npos;
            return valid ? path : std::string{};
        } catch (const std::exception&) { return std::string{}; }
    };
    if (object->inventorySerializedClass) {
        object->inventoryClassReferenceValid = true;
        if (entry.ObjBase == 0) object->inventoryBaseReferenceValid = true;
        else {
            const auto path = classReference(entry.ObjBase, object->inventoryBaseReferenceValid);
            if (!object->base) object->inventoryNativeBase = InventoryNativeClassId(path);
            if (!object->base && object->inventoryNativeBase == 0u) object->inventoryBaseReferenceValid = false;
        }
    } else {
        const auto path = classReference(entry.ObjClass, object->inventoryClassReferenceValid);
        if (!object->cls) object->inventoryNativeClass = InventoryNativeClassId(path);
        if (!object->cls && object->inventoryNativeClass == 0u) object->inventoryClassReferenceValid = false;
    }
    if (!object->property || object->property->type != "ObjectProperty") return;
    const auto dot = object->reflection.objectPath.find_last_of('.');
    const std::string_view name(object->reflection.objectPath.data() + (dot == std::string::npos ? 0u : dot + 1u),
        object->reflection.objectPath.size() - (dot == std::string::npos ? 0u : dot + 1u));
    if (!SameInventoryIdentity(name, "largeIcon") && !SameInventoryIdentity(name, "Icon") &&
        !SameInventoryIdentity(name, "PlayerViewMesh") && !SameInventoryIdentity(name, "PickupViewMesh") &&
        !SameInventoryIdentity(name, "ThirdPersonMesh") && !SameInventoryIdentity(name, "Skin") &&
        !SameInventoryIdentity(name, "MultiSkins")) return;
    object->inventoryIconClassConstraint = std::make_unique<InventoryIconClassConstraint>();
    auto& constraint = *object->inventoryIconClassConstraint;
    const auto path = classReference(object->property->referencedType, constraint.valid);
    const auto target = object->sourcePath.empty() ? persistentVmObjects.end() :
        persistentVmObjects.find(QuestVr::ObjectCastDetail::Fold(path));
    if (target != persistentVmObjects.end()) constraint.indexedClass = target->second;
    else constraint.nativeClass = InventoryNativeClassId(path);
    if (!constraint.indexedClass && constraint.nativeClass == 0u) constraint.valid = false;
}

struct IndexedDialogueLine {
    std::string eventPath;
    std::string entryLabel;
    std::string text;
    std::int32_t soundId{-1};
    std::string audioPackageName;
    std::vector<PortableDialogueResult::Effect> effects;
    std::vector<PortableDialogueResult::Choice> choices;
    bool invokeFrob{};
};

std::unordered_map<std::string, std::vector<IndexedDialogueLine>> persistentDialogueIndex;
std::unordered_map<std::string, PortableDialogueResult> persistentDataLinkIndex;
std::unordered_map<std::string, std::set<std::int32_t>> persistentSpeakerMissions;

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

RuntimeObject* NativeSessionViewport(const std::uint64_t identity) {
    if (!persistentNativeClient.HasClient() || !persistentPlayerSession ||
        !persistentPlayerSession->get())
        throw std::runtime_error("Player viewport has no live engine client/session objects");
    const auto registered=persistentNativeClient.ReadOnlyView();
    if (std::none_of(registered.begin(),registered.end(),[&](const auto& viewport) {
            return viewport.identity==identity;
        })) throw std::runtime_error("Player viewport identity is absent, revoked or stale");
    for(auto* viewport:persistentPlayerSession->get()->viewports)
        if(viewport && viewport->nativeViewportIdentity==identity &&
            viewport->nativeClientIdentity==persistentNativeClient.ClientIdentity()) return viewport;
    throw std::runtime_error("Registered viewport has no actual native player UObject");
}

void ClearNativePlayerEdges() noexcept {
    if (persistentRuntime && persistentRuntime->get())
        for(auto* object:persistentRuntime->get()->exports) {
            object->nativePlayer=nullptr;
            // Keep an established native-None view authoritative over any
            // older authored patch; clearing a session is not re-possession.
        }
    if(persistentPlayerSession && persistentPlayerSession->get())
        for(auto* viewport:persistentPlayerSession->get()->viewports)
            if(viewport) viewport->nativeViewportActor=nullptr;
}

void ClearNativePlayerObjects() noexcept {
    ClearNativePlayerEdges();
    if(!persistentPlayerSession || !persistentPlayerSession->get()) return;
    const auto unindex=[](RuntimeObject* object) {
        if(!object) return;
        for(auto it=persistentVmObjects.begin();it!=persistentVmObjects.end();)
            if(it->second==object) it=persistentVmObjects.erase(it); else ++it;
        for(auto it=persistentQualifiedObjects.begin();it!=persistentQualifiedObjects.end();)
            if(it->second==object) it=persistentQualifiedObjects.erase(it); else ++it;
    };
    auto* session=persistentPlayerSession->get();
    unindex(session->client);
    for(auto* viewport:session->viewports) unindex(viewport);
    session->client=nullptr;session->viewports.clear();
    // Registered native UClasses outlive every particular renderer client.
    // Keep their engine-owned roots and indexes until engine shutdown, including
    // when script retains an actual UClass reference across session teardown.
    // Never collect inside an in-flight script/OUT callback: its host journal
    // can still reference these objects until the outer boundary is released.
    pendingBirthGarbage=true;
}

std::string DialogueKey(std::int32_t mission, const std::string& speaker) {
    return std::to_string(mission) + "\n" + LowerAscii(speaker);
}

bool IsDerivedFromPath(RuntimeObject* cls, const std::string& path) {
    // UE FName identities are case-insensitive. Their retained display spelling
    // can depend on which package first interned the name (e.g. FrobDisplayWindow
    // mentions 'mover' before Engine's Mover export is loaded). Compare the full
    // qualified identity, without allocations or leaf-name type guesses.
    const auto sameIdentity = [](const std::string& a, const std::string& b) {
        if (a.size()!=b.size()) return false;
        const auto fold = [](unsigned char c) { return c>='A'&&c<='Z' ? static_cast<unsigned char>(c+('a'-'A')) : c; };
        for (std::size_t index=0u;index<a.size();++index)
            if (fold(static_cast<unsigned char>(a[index]))!=fold(static_cast<unsigned char>(b[index]))) return false;
        return true;
    };
    std::array<RuntimeObject*,128u> visited{};
    std::size_t depth{};
    bool matches{};
    for (RuntimeObject* current = cls; current != nullptr; current = current->base) {
        if (depth == visited.size() ||
            std::find(visited.begin(),visited.begin()+depth,current) != visited.begin()+depth)
            throw std::runtime_error("Runtime class hierarchy cycles or exceeds its native work bound");
        visited[depth++] = current;
        matches = matches || sameIdentity(current->reflection.objectPath,path);
    }
    // An early matching class must not conceal malformed later Super links.
    return matches;
}

const QuestVr::Vm::Value* FindScriptOverlay(
    RuntimeObject* object, const char* name, const std::uint32_t index = 0u) {
    if (object == nullptr) return nullptr;
    const auto key = LowerAscii(name);
    for (const auto* values : {&object->scriptValues, &object->frozenBirthDefaults}) {
        const auto found = values->find(key);
        if (found == values->end()) continue;
        const auto slot = found->second.find(index);
        if (slot != found->second.end()) return &slot->second;
    }
    return nullptr;
}

void CacheRuntimeNameProperties(RuntimeObject* object,
    const PortablePackageTables& package,
    const std::vector<PortableTaggedProperty>& properties) {
    for (const auto& property : properties) {
        if (property.type == 6u) {
            object->namePropertyValues[LowerAscii(property.name.ToString())][property.arrayIndex] =
                DecodePortableNameProperty(package, property);
        }
    }
}

const PortableTaggedProperty* FindInheritedRuntimeProperty(
    RuntimeObject* object, const char* name, const std::uint32_t arrayIndex = 0u) {
    if (object == nullptr) return nullptr;
    for (auto property = object->instanceProperties.rbegin();
         property != object->instanceProperties.rend(); ++property) {
        if (property->name == name && property->arrayIndex == arrayIndex) return &*property;
    }
    for (RuntimeObject* cls = object->cls; cls != nullptr; cls = cls->base) {
        if (!cls->classDescriptor) continue;
        for (auto property = cls->classDescriptor->defaults.rbegin();
             property != cls->classDescriptor->defaults.rend(); ++property) {
            if (property->name == name && property->arrayIndex == arrayIndex) return &*property;
        }
    }
    return nullptr;
}

std::string ReadInheritedRuntimeName(
    RuntimeObject* object, const char* name, const std::uint32_t arrayIndex = 0u) {
    if (const auto* value = FindScriptOverlay(object, name, arrayIndex)) return value->text;
    const auto lookup = [&](RuntimeObject* source) -> const std::string* {
        const auto named = source->namePropertyValues.find(LowerAscii(name));
        if (named == source->namePropertyValues.end()) return nullptr;
        const auto slot = named->second.find(arrayIndex);
        return slot == named->second.end() ? nullptr : &slot->second;
    };
    if (const auto* value = lookup(object)) return *value;
    for (RuntimeObject* cls = object->cls; cls != nullptr; cls = cls->base) {
        if (const auto* value = lookup(cls)) return *value;
    }
    return {};
}

float ReadInheritedRuntimeFloat(
    RuntimeObject* object, const char* name, const std::uint32_t arrayIndex = 0u) {
    if (const auto* value = FindScriptOverlay(object, name, arrayIndex)) return QuestVr::Vm::ToFloat(*value);
    const auto* property = FindInheritedRuntimeProperty(object, name, arrayIndex);
    float value{};
    if (property != nullptr && property->type == 4u && property->value.size() == 4u)
        std::memcpy(&value, property->value.data(), sizeof(value));
    // Retain non-finite authored values too. The bounded mesh sampler owns
    // validation and its explicit diagnostic; replacing them here hides bugs.
    return value;
}

void IndexRuntimeVmObject(const std::string& path, RuntimeObject* object) {
    // The legacy runtime may load a larger graph for non-VM inspection, but
    // the manual VM API rejects it. Bound this additional index accordingly.
    if (persistentVmObjects.size() < 1'000'000u)
        persistentVmObjects[LowerAscii(path)] = object;
}

bool ReadInheritedRuntimeBool(RuntimeObject* object, const char* name) {
    if (const auto* value = FindScriptOverlay(object, name)) return QuestVr::Vm::ToBool(*value);
    const auto* property = FindInheritedRuntimeProperty(object, name);
    return property != nullptr && property->type == 3u && property->boolValue;
}

QuestVr::Vm::Value BlendSimulatedValue(const std::array<float, 4u>& fields) {
    QuestVr::Vm::Value value; value.kind = QuestVr::Vm::Kind::Struct;
    for (std::size_t i = 0u; i < fields.size(); ++i)
        value.fields[std::array<const char*, 4u>{"x", "y", "z", "w"}[i]] = QuestVr::Vm::Value::Float(fields[i]);
    return value;
}

PortableActorAnimationSnapshot ReadRuntimeAnimationSnapshot(RuntimeObject* object) {
    PortableActorAnimationSnapshot animation;
    animation.sequence = ReadInheritedRuntimeName(object, "AnimSequence");
    animation.frame = ReadInheritedRuntimeFloat(object, "AnimFrame");
    animation.rate = ReadInheritedRuntimeFloat(object, "AnimRate");
    animation.last = ReadInheritedRuntimeFloat(object, "AnimLast");
    animation.minRate = ReadInheritedRuntimeFloat(object, "AnimMinRate");
    animation.tweenRate = ReadInheritedRuntimeFloat(object, "TweenRate");
    animation.oldRate = ReadInheritedRuntimeFloat(object, "OldAnimRate");
    animation.loop = ReadInheritedRuntimeBool(object, "bAnimLoop");
    animation.notify = ReadInheritedRuntimeBool(object, "bAnimNotify");
    animation.finished = ReadInheritedRuntimeBool(object, "bAnimFinished");
    for (std::uint32_t slot = 0u; slot < animation.blends.size(); ++slot) {
        auto& blend = animation.blends[slot];
        blend.sequence = ReadInheritedRuntimeName(object, "BlendAnimSequence", slot);
        blend.frame = ReadInheritedRuntimeFloat(object, "BlendAnimFrame", slot);
        blend.rate = ReadInheritedRuntimeFloat(object, "BlendAnimRate", slot);
        blend.last = ReadInheritedRuntimeFloat(object, "BlendAnimLast", slot);
        blend.minRate = ReadInheritedRuntimeFloat(object, "BlendAnimMinRate", slot);
        blend.tweenRate = ReadInheritedRuntimeFloat(object, "BlendTweenRate", slot);
        blend.oldRate = ReadInheritedRuntimeFloat(object, "OldBlendAnimRate", slot);
    }
    if (object->animationClock) {
        animation.previous = object->animationClock->pose.main.previous;
        for (std::size_t slot = 0u; slot < animation.blends.size(); ++slot)
            animation.blends[slot].previous = object->animationClock->pose.blends[slot].previous;
    }
    return animation;
}

void BuildPersistentDialogueIndex() {
    persistentDialogueIndex.clear();
    persistentDataLinkIndex.clear();
    persistentSpeakerMissions.clear();
    if (!persistentRuntime || !persistentRuntime->get()) return;
    const auto intProperty = [](RuntimeObject* object, const char* name, std::int32_t fallback) {
        for (const PortableTaggedProperty& property : object->instanceProperties) {
            if (property.name == name && property.value.size() == 4u) {
                std::int32_t value{};
                std::memcpy(&value, property.value.data(), sizeof(value));
                return value;
            }
        }
        return fallback;
    };
    std::unordered_map<std::string, PortablePackageTables> namePackages;
    const auto nameProperty = [&](RuntimeObject* object, const char* name) {
        for (const PortableTaggedProperty& property : object->instanceProperties) {
            if (property.name != name || property.type != 6u) continue;
            auto package = namePackages.find(object->sourcePath);
            if (package == namePackages.end()) {
                package = namePackages.emplace(
                    object->sourcePath, LoadPortablePackageTables(object->sourcePath)).first;
            }
            return DecodePortableNameProperty(package->second, property);
        }
        return std::string();
    };
    std::vector<std::pair<std::string, std::int32_t>> conversationOrder;
    for (RuntimeObject* list : persistentRuntime->get()->exports) {
        if (list == nullptr || !IsDerivedFromPath(list->cls, "ConSys.ConversationList")) continue;
        // Mission00 omits the serialized zero Int. Missing is the original
        // class/zero default, not a synthetic mission -1 bucket.
        const std::int32_t mission = intProperty(list, "missionNumber", 0);
        const auto firstItem = list->objectPropertyPaths.find("conversations");
        if (firstItem == list->objectPropertyPaths.end()) continue;
        std::string itemPath = firstItem->second;
        for (std::size_t guard = 0; !itemPath.empty() && guard < 4096u; ++guard) {
            const auto itemFound = persistentQualifiedObjects.find(itemPath);
            if (itemFound == persistentQualifiedObjects.end() || itemFound->second == nullptr) break;
            RuntimeObject* item = itemFound->second;
            const auto conversation = item->objectPropertyPaths.find("ConObject");
            if (conversation != item->objectPropertyPaths.end()) {
                conversationOrder.emplace_back(conversation->second, mission);
            }
            const auto next = item->objectPropertyPaths.find("Next");
            if (next == item->objectPropertyPaths.end() || next->second == itemPath) break;
            itemPath = next->second;
        }
    }
    std::unordered_map<std::string, std::vector<PortableDialogueResult::Effect>> indexedEffects;
    std::unordered_map<std::string, std::vector<PortableDialogueResult::Choice>> indexedChoices;
    std::size_t transferEvents{};
    std::size_t playerTransferEvents{};
    std::string sampleTransfer;
    for (const auto& conversationEntry : conversationOrder) {
        const auto conversationFound = persistentQualifiedObjects.find(conversationEntry.first);
        if (conversationFound == persistentQualifiedObjects.end() ||
            conversationFound->second == nullptr) continue;
        RuntimeObject* conversation = conversationFound->second;
        std::string audioPackageName;
        bool invokeFrob{};
        for (const PortableTaggedProperty& property : conversation->instanceProperties) {
            if (property.name == "audioPackageName" && property.type == 13u) {
                audioPackageName = DecodePortableStringProperty(property);
            } else if (property.name == "bInvokeFrob" && property.type == 3u) {
                invokeFrob = property.boolValue;
            }
        }
        const auto firstEvent = conversation->objectPropertyPaths.find("eventList");
        if (firstEvent == conversation->objectPropertyPaths.end()) continue;
        std::string eventPath = firstEvent->second;
        std::string precedingSpeech;
        std::string precedingSpeaker;
        std::string branchLabel;
        for (std::size_t guard = 0; !eventPath.empty() && guard < 8192u; ++guard) {
            const auto eventFound = persistentQualifiedObjects.find(eventPath);
            if (eventFound == persistentQualifiedObjects.end() || eventFound->second == nullptr) {
                break;
            }
            RuntimeObject* event = eventFound->second;
            for (const PortableTaggedProperty& property : event->instanceProperties) {
                if (property.name == "Label" && property.type == 13u) {
                    const std::string label = DecodePortableStringProperty(property);
                    if (!label.empty()) branchLabel = label;
                }
            }
            if (IsDerivedFromPath(event->cls, "ConSys.ConEventSpeech")) {
                std::string speaker;
                for (const PortableTaggedProperty& property : event->instanceProperties) {
                    if (property.name == "speakerName" && property.type == 13u) {
                        speaker = DecodePortableStringProperty(property);
                        break;
                    }
                }
                const auto speechPath = event->objectPropertyPaths.find("ConSpeech");
                if (!speaker.empty() && speechPath != event->objectPropertyPaths.end()) {
                    const auto speechFound = persistentQualifiedObjects.find(speechPath->second);
                    if (speechFound != persistentQualifiedObjects.end() &&
                        speechFound->second != nullptr) {
                        RuntimeObject* speech = speechFound->second;
                        IndexedDialogueLine line;
                        line.eventPath = event->reflection.objectPath;
                        line.entryLabel = branchLabel;
                        line.audioPackageName = audioPackageName;
                        line.invokeFrob = invokeFrob;
                        for (const PortableTaggedProperty& property : speech->instanceProperties) {
                            if (property.name == "Speech" && property.type == 13u) {
                                line.text = DecodePortableStringProperty(property);
                            } else if (property.name == "soundID" && property.value.size() == 4u) {
                                std::memcpy(&line.soundId, property.value.data(), sizeof(line.soundId));
                            }
                        }
                        if (!line.text.empty()) {
                            precedingSpeech = line.eventPath;
                            precedingSpeaker = speaker;
                            persistentDialogueIndex[
                                DialogueKey(conversationEntry.second, speaker)].push_back(
                                    std::move(line));
                            persistentSpeakerMissions[LowerAscii(speaker)].insert(
                                conversationEntry.second);
                        }
                    }
                }
            } else {
                PortableDialogueResult::Effect effect;
                effect.eventPath = event->reflection.objectPath;
                bool isEffect{};
                bool clearFollowingEffect{};
                const auto stringProperty = [&](const char* name) {
                    for (const PortableTaggedProperty& property : event->instanceProperties) {
                        if (property.name == name && property.type == 13u) {
                            return DecodePortableStringProperty(property);
                        }
                    }
                    return std::string();
                };
                const auto integerProperty = [&](const char* name) {
                    return intProperty(event, name, 0);
                };
                const auto boolProperty = [&](const char* name) {
                    for (const PortableTaggedProperty& property : event->instanceProperties) {
                        if (property.name == name && property.type == 3u) return property.boolValue;
                    }
                    return false;
                };
                if (IsDerivedFromPath(event->cls, "ConSys.ConEventChoice")) {
                    const auto firstChoice = event->objectPropertyPaths.find("ChoiceList");
                    if (firstChoice != event->objectPropertyPaths.end() &&
                        !precedingSpeech.empty()) {
                        std::string choicePath = firstChoice->second;
                        for (std::size_t choiceGuard = 0u;
                             !choicePath.empty() && choiceGuard < 64u;
                             ++choiceGuard) {
                            const auto choiceFound = persistentQualifiedObjects.find(choicePath);
                            if (choiceFound == persistentQualifiedObjects.end() ||
                                choiceFound->second == nullptr) break;
                            RuntimeObject* choiceObject = choiceFound->second;
                            PortableDialogueResult::Choice choice;
                            choice.objectPath = choicePath;
                            for (const PortableTaggedProperty& property :
                                 choiceObject->instanceProperties) {
                                if (property.name == "choiceText" && property.type == 13u) {
                                    choice.text = DecodePortableStringProperty(property);
                                } else if (property.name == "choiceLabel" &&
                                           property.type == 13u) {
                                    choice.label = DecodePortableStringProperty(property);
                                } else if (property.name == "soundID" &&
                                           property.value.size() == 4u) {
                                    std::memcpy(
                                        &choice.soundId,
                                        property.value.data(),
                                        sizeof(choice.soundId));
                                } else if (property.name == "skillLevelNeeded" &&
                                           property.value.size() == 4u) {
                                    std::memcpy(
                                        &choice.skillLevelNeeded,
                                        property.value.data(),
                                        sizeof(choice.skillLevelNeeded));
                                } else if (property.name == "bDisplayAsSpeech" &&
                                           property.type == 3u) {
                                    choice.displayAsSpeech = property.boolValue;
                                }
                            }
                            choice.conditional =
                                choiceObject->objectPropertyPaths.find("flagRef") !=
                                    choiceObject->objectPropertyPaths.end() ||
                                choiceObject->objectPropertyPaths.find("skillNeeded") !=
                                    choiceObject->objectPropertyPaths.end();
                            const auto choiceFlag =
                                choiceObject->objectPropertyPaths.find("flagRef");
                            if (choiceFlag != choiceObject->objectPropertyPaths.end()) {
                                const auto flagFound =
                                    persistentQualifiedObjects.find(choiceFlag->second);
                                if (flagFound != persistentQualifiedObjects.end() &&
                                    flagFound->second != nullptr) {
                                    choice.flagName = nameProperty(flagFound->second, "FlagName");
                                    for (const PortableTaggedProperty& property :
                                         flagFound->second->instanceProperties) {
                                        if (property.name == "Value" && property.type == 3u) {
                                            choice.requiredFlagValue = property.boolValue;
                                        }
                                    }
                                }
                            }
                            const auto skill =
                                choiceObject->objectPropertyPaths.find("skillNeeded");
                            if (skill != choiceObject->objectPropertyPaths.end()) {
                                choice.skillClassPath = skill->second;
                            }
                            if (!choice.text.empty() && !choice.label.empty()) {
                                std::string scanPath = firstEvent->second;
                                bool reachedLabel{};
                                for (std::size_t scanGuard = 0u;
                                     !scanPath.empty() && scanGuard < 8192u;
                                     ++scanGuard) {
                                    const auto scanFound = persistentQualifiedObjects.find(scanPath);
                                    if (scanFound == persistentQualifiedObjects.end() ||
                                        scanFound->second == nullptr) break;
                                    RuntimeObject* scanEvent = scanFound->second;
                                    for (const PortableTaggedProperty& property :
                                         scanEvent->instanceProperties) {
                                        if (property.name == "Label" && property.type == 13u &&
                                            LowerAscii(DecodePortableStringProperty(property)) ==
                                                LowerAscii(choice.label)) {
                                            reachedLabel = true;
                                        }
                                    }
                                    if (reachedLabel && IsDerivedFromPath(
                                            scanEvent->cls, "ConSys.ConEventSpeech")) {
                                        std::string scanSpeaker;
                                        for (const PortableTaggedProperty& property :
                                             scanEvent->instanceProperties) {
                                            if (property.name == "speakerName" &&
                                                property.type == 13u) {
                                                scanSpeaker = DecodePortableStringProperty(property);
                                            }
                                        }
                                        if (LowerAscii(scanSpeaker) ==
                                            LowerAscii(precedingSpeaker)) {
                                            choice.targetEventPath = scanPath;
                                            break;
                                        }
                                    }
                                    const auto scanNext =
                                        scanEvent->objectPropertyPaths.find("nextEvent");
                                    if (scanNext == scanEvent->objectPropertyPaths.end() ||
                                        scanNext->second == scanPath) break;
                                    scanPath = scanNext->second;
                                }
                                indexedChoices[precedingSpeech].push_back(std::move(choice));
                            }
                            const auto nextChoice =
                                choiceObject->objectPropertyPaths.find("nextChoice");
                            if (nextChoice == choiceObject->objectPropertyPaths.end() ||
                                nextChoice->second == choicePath) break;
                            choicePath = nextChoice->second;
                        }
                    }
                    precedingSpeech.clear();
                    precedingSpeaker.clear();
                } else if (IsDerivedFromPath(event->cls, "ConSys.ConEventSetFlag")) {
                    effect.type = PortableDialogueResult::Effect::Type::SetFlag;
                    const auto flag = event->objectPropertyPaths.find("flagRef");
                    if (flag != event->objectPropertyPaths.end()) {
                        effect.key = flag->second;
                        const auto flagObject = persistentQualifiedObjects.find(flag->second);
                        if (flagObject != persistentQualifiedObjects.end() && flagObject->second) {
                            const std::string flagName = nameProperty(flagObject->second, "FlagName");
                            if (!flagName.empty()) effect.key = flagName;
                            for (const PortableTaggedProperty& property :
                                 flagObject->second->instanceProperties) {
                                if (property.name == "Value" && property.type == 3u) {
                                    effect.value = property.boolValue;
                                }
                            }
                        }
                        isEffect = true;
                    }
                } else if (IsDerivedFromPath(event->cls, "ConSys.ConEventAddGoal")) {
                    effect.type = PortableDialogueResult::Effect::Type::AddGoal;
                    effect.key = nameProperty(event, "goalName");
                    if (effect.key.empty()) effect.key = event->reflection.objectPath;
                    effect.text = stringProperty("goalText");
                    effect.completed = boolProperty("bGoalCompleted");
                    effect.primary = boolProperty("bPrimaryGoal");
                    isEffect = !effect.text.empty();
                } else if (IsDerivedFromPath(event->cls, "ConSys.ConEventAddNote")) {
                    effect.type = PortableDialogueResult::Effect::Type::AddNote;
                    effect.key = event->reflection.objectPath;
                    effect.text = stringProperty("noteText");
                    isEffect = !effect.text.empty();
                } else if (IsDerivedFromPath(event->cls, "ConSys.ConEventAddSkillPoints")) {
                    effect.type = PortableDialogueResult::Effect::Type::AddSkillPoints;
                    effect.key = event->reflection.objectPath;
                    effect.text = stringProperty("awardMessage");
                    effect.amount = integerProperty("pointsToAdd");
                    isEffect = effect.amount != 0;
                } else if (IsDerivedFromPath(event->cls, "ConSys.ConEventAddCredits")) {
                    effect.type = PortableDialogueResult::Effect::Type::AddCredits;
                    effect.key = event->reflection.objectPath;
                    effect.amount = integerProperty("creditsToAdd");
                    isEffect = effect.amount != 0;
                } else if (IsDerivedFromPath(event->cls, "ConSys.ConEventTrigger")) {
                    effect.type = PortableDialogueResult::Effect::Type::Trigger;
                    effect.key = nameProperty(event, "triggerTag");
                    isEffect = !effect.key.empty() && LowerAscii(effect.key) != "none";
                } else if (IsDerivedFromPath(event->cls, "ConSys.ConEventTransferObject")) {
                    ++transferEvents;
                    effect.type = PortableDialogueResult::Effect::Type::TransferObject;
                    effect.source = stringProperty("fromName");
                    effect.target = stringProperty("toName");
                    effect.amount = std::max(1, integerProperty("transferCount"));
                    const auto objectClass = event->objectPropertyPaths.find("giveObject");
                    if (objectClass != event->objectPropertyPaths.end()) {
                        effect.key = objectClass->second;
                    }
                    if (effect.key.empty()) {
                        const std::string objectName = stringProperty("ObjectName");
                        const std::string suffix = "." + LowerAscii(objectName);
                        for (const auto& candidate : persistentQualifiedObjects) {
                            if (candidate.second == nullptr ||
                                candidate.second->reflection.metaClass != "Class") continue;
                            const std::string path = LowerAscii(candidate.first);
                            if (!objectName.empty() && path.size() >= suffix.size() &&
                                path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0) {
                                effect.key = candidate.first;
                                break;
                            }
                        }
                    }
                    const auto playerName = [](const std::string& value) {
                        const std::string lowered = LowerAscii(value);
                        return lowered == "jcdenton" || lowered == "jc denton" ||
                            lowered == "player" || lowered == "playername";
                    };
                    isEffect = !effect.key.empty() &&
                        (playerName(effect.source) || playerName(effect.target));
                    if (isEffect) ++playerTransferEvents;
                    if (sampleTransfer.empty()) {
                        sampleTransfer = effect.source + "->" + effect.target + ":" + effect.key;
                    }
                    clearFollowingEffect = true;
                } else if (IsDerivedFromPath(event->cls, "ConSys.ConEventCheckFlag") ||
                           IsDerivedFromPath(event->cls, "ConSys.ConEventCheckObject") ||
                           IsDerivedFromPath(event->cls, "ConSys.ConEventCheckPersona") ||
                           IsDerivedFromPath(event->cls, "ConSys.ConEventJump") ||
                           IsDerivedFromPath(event->cls, "ConSys.ConEventRandomLabel") ||
                           IsDerivedFromPath(event->cls, "ConSys.ConEventTrade")) {
                    precedingSpeech.clear();
                    precedingSpeaker.clear();
                }
                if (isEffect && !precedingSpeech.empty()) {
                    indexedEffects[precedingSpeech].push_back(std::move(effect));
                }
                if (clearFollowingEffect) {
                    precedingSpeech.clear();
                    precedingSpeaker.clear();
                }
            }
            const auto next = event->objectPropertyPaths.find("nextEvent");
            if (next == event->objectPropertyPaths.end() || next->second == eventPath) break;
            eventPath = next->second;
        }
    }
    std::size_t indexedLines{};
    std::size_t indexedEffectCount{};
    std::size_t indexedChoiceCount{};
    std::size_t frobLineCount{};
    for (auto& entry : persistentDialogueIndex) {
        for (IndexedDialogueLine& line : entry.second) {
            ++indexedLines;
            if (line.invokeFrob) ++frobLineCount;
            const auto effects = indexedEffects.find(line.eventPath);
            if (effects != indexedEffects.end()) {
                line.effects = effects->second;
                indexedEffectCount += line.effects.size();
            }
            const auto choices = indexedChoices.find(line.eventPath);
            if (choices != indexedChoices.end()) {
                line.choices = choices->second;
                indexedChoiceCount += line.choices.size();
            }
        }
    }
    for (const auto& [path, mission] : conversationOrder) {
        const auto found = persistentQualifiedObjects.find(path);
        if (found == persistentQualifiedObjects.end() || !found->second) continue;
        auto* conversation = found->second;
        if (!ReadInheritedRuntimeBool(conversation, "bDataLinkCon")) continue;
        const auto flags = conversation->objectPropertyPaths.find("flagRefList");
        if (flags != conversation->objectPropertyPaths.end() && !flags->second.empty()) continue;
        const auto name = ReadInheritedRuntimeName(conversation, "conName");
        const auto first = conversation->objectPropertyPaths.find("eventList");
        if (name.empty() || first == conversation->objectPropertyPaths.end()) continue;
        const auto event = persistentQualifiedObjects.find(first->second);
        if (event == persistentQualifiedObjects.end() || !event->second ||
            !IsDerivedFromPath(event->second->cls, "ConSys.ConEventSpeech")) continue;
        const auto next = event->second->objectPropertyPaths.find("nextEvent");
        if (next == event->second->objectPropertyPaths.end()) continue;
        const auto end = persistentQualifiedObjects.find(next->second);
        if (end == persistentQualifiedObjects.end() || !end->second ||
            !IsDerivedFromPath(end->second->cls, "ConSys.ConEventEnd")) continue;
        // Resolve this exact event, never an arbitrary speech by its speaker.
        for (const auto& [key, lines] : persistentDialogueIndex) {
            if (key.rfind(std::to_string(mission) + "\n", 0u) != 0u) continue;
            const auto line = std::find_if(lines.begin(), lines.end(), [&](const auto& candidate) {
                return candidate.eventPath == first->second;
            });
            if (line == lines.end()) continue;
            PortableDialogueResult result;
            result.found = true; result.bindName = key.substr(key.find('\n') + 1u);
            for (const auto& property : event->second->instanceProperties)
                if (property.name == "speakerName" && property.type == 13u)
                    result.bindName = DecodePortableStringProperty(property);
            result.eventPath = line->eventPath; result.speech = line->text;
            result.audioPackageName = line->audioPackageName; result.soundId = line->soundId;
            result.matchingLines = 1u;
            const auto dataKey = DialogueKey(mission, name);
            // Ambiguous names are refused, not resolved by hash iteration.
            const auto duplicate = persistentDataLinkIndex.find(dataKey);
            if (duplicate != persistentDataLinkIndex.end()) duplicate->second = {};
            else persistentDataLinkIndex.emplace(dataKey, std::move(result));
            break;
        }
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        "DeusExQuest",
        "DeusExQuest: indexed %zu dialogue lines (%zu frob) with %zu choices and %zu safe linear effects across %zu speaker missions; transfers=%zu player=%zu sample=%s",
        indexedLines,
        frobLineCount,
        indexedChoiceCount,
        indexedEffectCount,
        persistentDialogueIndex.size(),
        transferEvents,
        playerTransferEvents,
        sampleTransfer.c_str());
}

class RuntimeBytecodeReader {
public:
    explicit RuntimeBytecodeReader(const std::vector<std::uint8_t>& bytes)
        : bytes_(bytes) {}

    std::uint8_t Byte() {
        Require(1);
        return bytes_[position_++];
    }
    std::uint32_t Dword() {
        const std::uint32_t a = Byte();
        const std::uint32_t b = Byte();
        const std::uint32_t c = Byte();
        const std::uint32_t d = Byte();
        return a | (b << 8u) | (c << 16u) | (d << 24u);
    }
    std::string AsciiZ() {
        std::string result;
        while (true) {
            const char value = static_cast<char>(Byte());
            if (value == '\0') return result;
            result.push_back(value);
        }
    }

private:
    void Require(std::size_t count) const {
        if (count > bytes_.size() - position_) {
            throw std::runtime_error("Portable VM read past function bytecode");
        }
    }
    const std::vector<std::uint8_t>& bytes_;
    std::size_t position_{};
};

PortableVmValue EvaluateConstant(RuntimeBytecodeReader& reader) {
    PortableVmValue result;
    switch (reader.Byte()) {
        case 0x0b: // Nothing
            return result;
        case 0x1d: // IntConst
            result.type = PortableVmValueType::Integer;
            result.integer = static_cast<std::int32_t>(reader.Dword());
            return result;
        case 0x1e: { // FloatConst
            const std::uint32_t bits = reader.Dword();
            result.type = PortableVmValueType::Float;
            std::memcpy(&result.floating, &bits, sizeof(bits));
            return result;
        }
        case 0x1f: // StringConst
            result.type = PortableVmValueType::String;
            result.string = reader.AsciiZ();
            return result;
        case 0x20: // ObjectConst
            result.type = PortableVmValueType::ObjectReference;
            result.integer = static_cast<std::int32_t>(reader.Dword());
            return result;
        case 0x21: // NameConst
            result.type = PortableVmValueType::NameReference;
            result.integer = static_cast<std::int32_t>(reader.Dword());
            return result;
        case 0x24: // ByteConst
            result.type = PortableVmValueType::Integer;
            result.integer = reader.Byte();
            return result;
        case 0x25: // IntZero
            result.type = PortableVmValueType::Integer;
            return result;
        case 0x26: // IntOne
            result.type = PortableVmValueType::Integer;
            result.integer = 1;
            return result;
        case 0x27: // True
            result.type = PortableVmValueType::Boolean;
            result.boolean = true;
            return result;
        case 0x28: // False
            result.type = PortableVmValueType::Boolean;
            return result;
        case 0x2a: // NoObject
            result.type = PortableVmValueType::ObjectReference;
            return result;
        default:
            throw std::runtime_error("Portable VM constant evaluator encountered unsupported token");
    }
}

RuntimeObject* ResolveLocal(
    std::int32_t reference,
    const std::vector<RuntimeObject*>& exports,
    PortableRuntimeSummary& summary) {
    if (reference > 0 && static_cast<std::size_t>(reference) <= exports.size()) {
        ++summary.resolvedLinks;
        return exports[static_cast<std::size_t>(reference - 1)];
    }
    if (reference < 0) ++summary.unresolvedExternalLinks;
    return nullptr;
}

std::string PackageStem(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t begin = slash == std::string::npos ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    const std::size_t end = dot == std::string::npos || dot < begin ? path.size() : dot;
    return path.substr(begin, end - begin);
}

std::unique_ptr<PortableLodMesh> LoadCompleteRuntimeLodMesh(
    const PortablePackageTables& package, const std::size_t exportIndex, const std::string& sourcePath) {
    auto mesh = std::make_unique<PortableLodMesh>(LoadPortableLodMesh(package, exportIndex));
    mesh->texturePaths.reserve(mesh->textures.size());
    for (const std::int32_t texture : mesh->textures) {
        auto path = GetPortableObjectPath(package, texture);
        if (texture > 0 && !path.empty()) path = PackageStem(sourcePath) + '.' + path;
        mesh->texturePaths.push_back(std::move(path));
    }
    // Geometry, animation and qualified material slots are one immutable cache
    // entry. Resolve all paths before either lazy native queries/commands or
    // map-wide decode can publish it; failure must not leave a partial mesh.
    return mesh;
}

std::string ResolveRuntimePackagePath(
    const std::string& gameRoot,
    const std::string& packageName) {
    static constexpr const char* directories[] = {
        "Textures", "System", "Sounds", "Music", "Maps"};
    static constexpr const char* extensions[] = {"utx", "u", "uax", "umx", "dx"};
    for (const char* directory : directories) {
        for (const char* extension : extensions) {
            const std::string candidate = gameRoot + "/" + directory + "/" +
                packageName + "." + extension;
            std::FILE* file = std::fopen(candidate.c_str(), "rb");
            if (file != nullptr) {
                std::fclose(file);
                return candidate;
            }
        }
    }
    return {};
}

void PopulateRuntime(
    RuntimePackage* runtime,
    const PortablePackageTables& package,
    const PortableReflectionGraph& graph,
    PortableRuntimeSummary& summary,
    std::size_t* destroyedObjects) {
    runtime->exports.reserve(graph.objects.size());
    for (const PortableReflectionObject& reflection : graph.objects) {
        runtime->exports.push_back(
            GC::Alloc<RuntimeObject>(reflection, destroyedObjects));
    }

    for (std::size_t index = 0; index < runtime->exports.size(); ++index) {
        RuntimeObject* object = runtime->exports[index];
        const ExportTableEntry& entry = package.exports[index];
        if (RuntimeObject* outer = ResolveLocal(entry.ObjOuter, runtime->exports, summary)) {
            object->outer = outer;
            object->references.push_back(outer);
        }
        if (RuntimeObject* base = ResolveLocal(entry.ObjBase, runtime->exports, summary)) {
            object->base = base;
            object->references.push_back(base);
        }
        if (RuntimeObject* cls = ResolveLocal(entry.ObjClass, runtime->exports, summary)) {
            object->cls = cls;
            object->references.push_back(cls);
        }

        if (object->reflection.metaClass == "Function") {
            object->script = std::make_unique<PortableScriptBody>(
                LoadPortableFunctionScript(package, index));
            summary.normalizedBytecodeBytes += object->script->bytecode.size();
            ++summary.functions;
        } else if (object->reflection.metaClass == "State") {
            object->stateDescriptor = std::make_unique<PortableStateDescriptor>(
                LoadPortableStateDescriptor(package, index));
            ++summary.states;
            summary.normalizedStateBytecodeBytes += object->stateDescriptor->bytecode.size();
            for (const auto reference : {object->stateDescriptor->baseField,
                    object->stateDescriptor->nextField, object->stateDescriptor->scriptText,
                    object->stateDescriptor->children}) {
                if (auto* target = ResolveLocal(reference, runtime->exports, summary))
                    object->references.push_back(target);
            }
        } else if (object->reflection.metaClass.size() >= 8 &&
            object->reflection.metaClass.compare(
                object->reflection.metaClass.size() - 8, 8, "Property") == 0) {
            object->property = std::make_unique<PortablePropertyDescriptor>(
                LoadPortablePropertyDescriptor(package, index));
            if (RuntimeObject* type = ResolveLocal(
                    object->property->referencedType, runtime->exports, summary)) {
                object->references.push_back(type);
            }
            if (RuntimeObject* type = ResolveLocal(
                    object->property->secondaryType, runtime->exports, summary)) {
                object->references.push_back(type);
            }
            ++summary.properties;
        }
        CacheInventoryTableMetadata(object, package, entry);
        if (object->reflection.metaClass == "Class") ++summary.classes;
    }
    summary.objects = runtime->exports.size();
    summary.peakGcObjects = GC::GetStats().numObjects;
}

const PortableStateDescriptor* StateMetadata(const RuntimeObject* object) {
    if (object->stateDescriptor) return object->stateDescriptor.get();
    return object->classDescriptor ? &object->classDescriptor->state : nullptr;
}

// Follow actual Children/Next chains, including non-callback Enum/Struct
// siblings. Outer-name coincidence alone never creates a callable member.
const QuestVr::ScriptDispatch::Graph& DispatchGraph() {
    if (persistentDispatchGraph) return *persistentDispatchGraph;
    if (!persistentRuntime || persistentVmObjects.empty())
        throw std::runtime_error("Script dispatch has no indexed runtime");
    std::unordered_map<std::string, PortablePackageTables> tables;
    const auto table = [&](const std::string& source) -> const PortablePackageTables& {
        auto found = tables.find(source);
        if (found == tables.end()) {
            if (tables.size() >= 64u || std::filesystem::file_size(source) > 512u * 1024u * 1024u)
                throw std::runtime_error("Dispatch table budget exceeded");
            found = tables.emplace(source, LoadPortablePackageTables(source)).first;
        }
        return found->second;
    };
    const auto referenced = [&](RuntimeObject* source, const std::int32_t reference) -> RuntimeObject* {
        if (!reference) return nullptr;
        const auto& package = table(source->sourcePath);
        auto path = GetPortableObjectPath(package, reference);
        if (reference > 0) path = PackageStem(source->sourcePath) + '.' + path;
        const auto found = persistentVmObjects.find(LowerAscii(path));
        if (found == persistentVmObjects.end()) throw std::runtime_error("Dispatch field reference is unavailable: " + path);
        return found->second;
    };
    PortableScriptDispatchSummary summary;
    const auto children = [&](RuntimeObject* owner, const std::int32_t first) {
        std::vector<RuntimeObject*> fields;
        std::unordered_set<RuntimeObject*> visited;
        RuntimeObject* field = referenced(owner, first);
        while (field) {
            if (visited.size() >= 8192u || !visited.insert(field).second || field->outer != owner)
                throw std::runtime_error("Dispatch Children/Next ownership/cycle budget failed: " + owner->reflection.objectPath);
            fields.push_back(field);
            std::int32_t next{};
            if (field->script) next = field->script->nextField;
            else if (field->property) next = field->property->nextField;
            else if (const auto* state = StateMetadata(field)) next = state->nextField;
            else {
                if (!field->commonFieldLinks) {
                    field->commonFieldLinks = LoadPortableFieldLinks(table(field->sourcePath), field->exportIndex);
                }
                ++summary.commonFields;
                next = field->commonFieldLinks->nextField;
            }
            field = referenced(field, next);
        }
        return fields;
    };
    std::vector<QuestVr::ScriptDispatch::Class> classes;
    for (std::size_t index = 0; index < persistentScriptExportCount; ++index) {
        auto* object = persistentRuntime->get()->exports[index];
        if (!object->classDescriptor) continue;
        QuestVr::ScriptDispatch::Class cls;
        cls.path = object->reflection.objectPath;
        cls.parentPath = object->base ? object->base->reflection.objectPath : std::string{};
        if (object->classDescriptor->state.baseField && !object->base)
            throw std::runtime_error("Dispatch class base is unresolved: " + cls.path);
        cls.probeMask = object->classDescriptor->state.probeMask;
        for (auto* field : children(object, object->classDescriptor->state.children)) {
            const auto& package = table(field->sourcePath);
            const auto name = package.names.at(static_cast<std::size_t>(package.exports[field->exportIndex].ObjName)).Name;
            if (field->script) {
                cls.functions.emplace(name.ToString(), field->reflection.objectPath); ++summary.classFunctions;
            } else if (field->stateDescriptor) {
                QuestVr::ScriptDispatch::State state;
                state.path = field->reflection.objectPath; state.name = name.ToString();
                state.compareIndex = static_cast<std::uint32_t>(name.GetCompareIndex());
                state.flags = field->stateDescriptor->stateFlags;
                if(auto* parent=referenced(field,field->stateDescriptor->baseField)) {
                    if(!StateMetadata(parent)) throw std::runtime_error("Dispatch state SuperField is not State/Class: "+state.path);
                    state.parentPath=parent->reflection.objectPath;
                }
                for (auto* member : children(field, field->stateDescriptor->children)) {
                    if (!member->script) continue;
                    const auto& memberPackage = table(member->sourcePath);
                    const auto functionName = memberPackage.names.at(static_cast<std::size_t>(memberPackage.exports[member->exportIndex].ObjName)).Name;
                    state.functions.emplace(functionName.ToString(), member->reflection.objectPath);
                    ++summary.stateFunctions;
                }
                cls.stateChildOrder.push_back(state.path);
                cls.states.emplace(state.name, std::move(state)); ++summary.states;
            }
        }
        classes.push_back(std::move(cls)); ++summary.classes;
    }
    auto graph = std::make_unique<QuestVr::ScriptDispatch::Graph>(std::move(classes));
    persistentDispatchSummary = summary;
    persistentDispatchGraph = std::move(graph);
    return *persistentDispatchGraph;
}

PortableActorDispatchContext AuthoredDispatchContext(RuntimeObject* actor, const bool inspectRunnable = false) {
    if (!actor->cls || !actor->cls->classDescriptor)
        throw std::runtime_error("Authored dispatch receiver has no serialized Class masks");
    PortableActorDispatchContext result;
    result.classProbeMask = actor->cls->classDescriptor->state.probeMask;
    result.liveProbeMask = ~std::uint64_t{0}; // Original absent-frame IsProbing permits all probes.
    if (!actor->serializedStack) return result;
    const auto& stack = *actor->serializedStack;
    result.framePresent = true;
    result.liveProbeMask = stack.probeMask;
    if (stack.functionReference && stack.stateReference) {
        if (!actor->serializedFunctionCode || !(actor->serializedFunctionCode->script ||
            StateMetadata(actor->serializedFunctionCode) || actor->serializedFunctionCode->reflection.metaClass == "Struct"))
            throw std::runtime_error("Serialized frame function is not a UStruct identity");
        result.codePresent = stack.logicalOffset && *stack.logicalOffset != -1;
        if ((!stack.logicalOffset || result.codePresent) && !inspectRunnable)
            throw std::runtime_error("Runnable serialized state continuation is not implemented");
        const auto* code = actor->serializedStateCode;
        if (!code || !StateMetadata(code))
            throw std::runtime_error("Serialized stopped frame is not a State/Class identity");
        result.codePath = code->reflection.objectPath;
        result.selectedPath = result.codePath;
        result.runningPath = actor->serializedFunctionCode->reflection.objectPath;
        result.exactSelected = true;
        const auto* state = StateMetadata(code);
        result.codeMasks = QuestVr::ScriptDispatch::CodeMasks{state->probeMask, state->ignoreMask};
        result.stateName = result.codePath.substr(result.codePath.find_last_of('.') + 1u);
    }
    for (std::uint8_t index = 0; index < 64u; ++index)
        if (!(stack.probeMask & (std::uint64_t{1} << index)))
            result.disabledNames.insert(QuestVr::ScriptDispatch::ProbeEventName(index));
    return result;
}

PortableActorDispatchContext CurrentDispatchContext(RuntimeObject* actor, const bool inspectRunnable = false) {
    if (actor->stateObject && actor->stateObject->frameOverride &&
        actor->stateObject->frame && actor->stateObject->frame->control) {
        const auto& frame = *actor->stateObject->frame;
        const auto& control = *frame.control;
        if (!actor->cls || !actor->cls->classDescriptor)
            throw std::runtime_error("Modern dispatch receiver has no authored Class masks");
        const auto selected = persistentVmObjects.find(LowerAscii(control.selectedPath));
        if (selected == persistentVmObjects.end() || !StateMetadata(selected->second))
            throw std::runtime_error("Modern selected node is not an authored State/Class");
        PortableActorDispatchContext result;
        result.classProbeMask = actor->cls->classDescriptor->state.probeMask;
        result.selectedPath = selected->second->reflection.objectPath;
        result.runningPath = frame.codePath;
        // Compatibility field: modern code masks/names always describe the
        // selected node. Running code is exposed separately above.
        result.codePath = result.selectedPath;
        result.stateName = result.selectedPath.substr(result.selectedPath.find_last_of('.') + 1u);
        const auto* masks = StateMetadata(selected->second);
        result.codeMasks = QuestVr::ScriptDispatch::CodeMasks{masks->probeMask, masks->ignoreMask};
        result.liveProbeMask = control.liveProbeMask;
        result.framePresent = result.exactSelected = true;
        result.codePresent = control.codePresent;
        return result;
    }
    auto result = AuthoredDispatchContext(actor, inspectRunnable);
    if (!actor->stateObject) return result;
    const auto& live = *actor->stateObject;
    if (live.frameOverride) {
        result.selectedPath.clear(); result.runningPath.clear(); result.exactSelected = false;
        result.framePresent = live.frame.has_value();
        result.codePresent = live.frame && !live.frame->codePath.empty() && live.frame->latent != QuestVr::StateLatent::Stop;
        result.codePath.clear(); result.stateName = "None"; result.codeMasks.reset();
        result.liveProbeMask = result.classProbeMask;
        if (live.frame && !live.frame->codePath.empty()) {
            const auto found = persistentVmObjects.find(LowerAscii(live.frame->codePath));
            if (found == persistentVmObjects.end() || !found->second->stateDescriptor)
                throw std::runtime_error("Portable frame code is not an authored State");
            const auto* code = found->second;
            result.codePath = code->reflection.objectPath;
            result.runningPath = result.codePath;
            result.stateName = result.codePath.substr(result.codePath.find_last_of('.') + 1u);
            result.codeMasks = QuestVr::ScriptDispatch::CodeMasks{
                code->stateDescriptor->probeMask, code->stateDescriptor->ignoreMask};
            result.liveProbeMask = (result.classProbeMask | result.codeMasks->probeMask) & result.codeMasks->ignoreMask;
        }
    } else {
        // The complete negative probe set is copied from the positive raw
        // frame on first mutation. Retain enables of formerly clear bits;
        // ANDing the original mask again would lose those native writes.
        result.liveProbeMask = ~std::uint64_t{0};
    }
    result.disabledNames.clear();
    const auto key = QuestVr::ScriptDispatch::FoldName(result.stateName);
    for (const auto& [state, names] : live.disabled)
        if (QuestVr::ScriptDispatch::FoldName(state) == key) { result.disabledNames = names; break; }
    const bool ownedFrame = live.frameOverride ? live.frame.has_value() : actor->serializedStack.has_value();
    if (!ownedFrame) {
        result.liveProbeMask = ~std::uint64_t{0};
        result.disabledNames.clear(); // No frame means no original probe gate.
    } else {
        for (const auto& name : result.disabledNames)
            if (const auto index = QuestVr::ScriptDispatch::EventProbeIndex(name))
                *result.liveProbeMask &= ~(std::uint64_t{1} << *index);
    }
    return result;
}

// Scoped host for the actual normalized UE1 interpreter. It does not dispatch
// startup/state ticks or silently emulate missing natives. A complete nested
// invocation is one transaction, including native actor topology/deletion and
// animation/tween history. Deleted actors keep their UObject identity.
class PortableActorVmHost final : public QuestVr::Vm::Host {
    using Value = QuestVr::Vm::Value;
    using Kind = QuestVr::Vm::Kind;
    using Property = QuestVr::Vm::Property;
    using Function = QuestVr::Vm::Function;
    using Evaluation = QuestVr::Vm::Evaluation;
    using Reference = QuestVr::Vm::Reference;
public:
    PortableActorVmHost() {
        if (!persistentRuntime || !persistentRuntime->get())
            throw std::runtime_error("Portable actor VM has no initialized runtime");
        if (persistentQualifiedObjects.size() > 1'000'000u)
            throw std::runtime_error("Portable actor VM object lookup budget exceeded");
    }
    RuntimeObject* Object(const std::string& path) const {
        static constexpr std::string_view prefix="@ClassDefault:";
        if (LowerAscii(path).rfind(LowerAscii(std::string(prefix)),0u)==0u) {
            const auto found=persistentVmObjects.find(LowerAscii(path.substr(prefix.size())));
            if (found==persistentVmObjects.end() || !found->second ||
                !found->second->classDescriptor || !found->second->defaultObject)
                throw std::runtime_error("Portable class-default identity is unavailable: "+path);
            return found->second->defaultObject;
        }
        const auto found = persistentVmObjects.find(LowerAscii(path));
        if (found == persistentVmObjects.end() || found->second == nullptr)
            throw std::runtime_error("Portable actor VM object is unavailable: " + path);
        if(found->second->nativeClientIdentity &&
            found->second->nativeClientIdentity!=persistentNativeClient.ClientIdentity())
            throw std::runtime_error("Portable native session UObject is revoked or stale");
        return found->second;
    }
    RuntimeObject* RequireNativeLevelInfo(RuntimeObject* actor) { return NativeLevelInfo(actor); }
    Value SetSessionPlayer(const std::uint64_t viewportIdentity,const std::string& actorPath,
        QuestVr::Vm::Execution& execution) {
        if(!transaction_) throw std::runtime_error("SetPlayer requires its outer native/script transaction");
        auto* viewport=NativeSessionViewport(viewportIdentity);
        auto* actor=Object(actorPath);RequireMapActor(actor);
        if(!actor->active || actor->worldRemoved || ReadInheritedRuntimeBool(actor,"bDeleteMe") ||
            !IsDerivedFromPath(actor->cls,"Engine.PlayerPawn"))
            throw std::runtime_error("SetPlayer receiver is not an actual live PlayerPawn");
        auto* level=NativeLevelInfo(actor);
        if(!QuestVr::Vm::ToBool(Read(level,PropertyNamed(level,"bBegunPlay"),0u)))
            throw std::runtime_error("SetPlayer requires accepted begun world before original Possess");
        const auto playerField=DescribeProperty("Engine.PlayerPawn.Player");
        const auto actorField=DescribeProperty("Engine.Player.Actor");
        const auto scalarObject=[](const Property& property,const char* key) {
            return LowerAscii(property.key)==key && property.zero.kind==Kind::Object && property.arrayDimension==1u;
        };
        if(!scalarObject(playerField,"engine.playerpawn.player") || !scalarObject(actorField,"engine.player.actor") ||
            !OwnsProperty(actor->cls,playerField) || !OwnsProperty(viewport->cls,actorField))
            throw std::runtime_error("SetPlayer fields do not match original Player/PlayerPawn declarations");
        clientDependent_=true;
        const auto pointer=[&](RuntimeObject* owner,const bool viewportActor) {
            auto reference=std::make_shared<Reference>();reference->zero=Value::Text(Kind::Object,{});
            reference->read=[owner,viewportActor] {
                auto* target=viewportActor ? owner->nativeViewportActor : owner->nativePlayer;
                return Value::Text(Kind::Object,target ? target->reflection.objectPath : std::string{});
            };
            reference->write=[this,owner,viewportActor](const Value& value) {
                if(value.kind!=Kind::Object) throw std::runtime_error("Native SetPlayer edge is not an Object");
                auto* target=value.text.empty() ? nullptr : Object(value.text);
                Touch(owner);
                if(viewportActor) owner->nativeViewportActor=target;
                else {owner->nativePlayer=target;owner->nativePlayerBinding=true;}
            };
            return reference;
        };
        // Retail body10368cd7 first detaches the target UPlayer's old Actor,
        // then pairs newPawn.Player / Player.Actor before invoking Possess.
        if(auto* previous=viewport->nativeViewportActor) {
            RequireMapActor(previous);
            execution.WriteReference(pointer(previous,false),Value::Text(Kind::Object,{}));
            execution.WriteReference(pointer(viewport,true),Value::Text(Kind::Object,{}));
        }
        execution.WriteReference(pointer(actor,false),Value::Text(Kind::Object,viewport->reflection.objectPath));
        execution.WriteReference(pointer(viewport,true),Value::Text(Kind::Object,actor->reflection.objectPath));
        static_cast<void>(execution.CallEvent(actor->reflection.objectPath,"Possess",false,{}));
        static_cast<void>(NativeSessionViewport(viewportIdentity));
        return Value::Text(Kind::Object,actor->reflection.objectPath);
    }
    std::string ClassDefaultObject(const std::string& path) override {
        auto* cls=Object(path);
        if (!cls->classDescriptor || cls->defaultOwner || DefaultClass(cls)!=cls)
            throw std::runtime_error("ClassContext selector is not an actual loaded UClass");
        if (!cls->defaultObject)
            throw std::runtime_error("Loaded UClass has no class-owned default-object block");
        return cls->defaultObject->reflection.objectPath;
    }
    std::shared_ptr<const Function> Member(RuntimeObject* receiver, const std::string& name) {
        if (name.find('.') != std::string::npos) return FunctionIdentity(Object(name));
        const auto context = CurrentDispatchContext(receiver);
        // Legacy overrides cannot recover selected identity from running code.
        const auto selected = context.exactSelected ?
            QuestVr::ScriptDispatch::ResolveFunctionAtSelectedNode(DispatchGraph(),
                receiver->cls->reflection.objectPath, context.selectedPath, name) :
            QuestVr::ScriptDispatch::ResolveFunction(DispatchGraph(),
                receiver->cls->reflection.objectPath, context.stateName, name);
        if (selected) return FunctionIdentity(Object(*selected));
        throw std::runtime_error("Portable actor VM member function unavailable: " + name);
    }
    bool CanCall(const Function& function, const std::string& receiver) override {
        const auto context = CurrentDispatchContext(Object(receiver));
        const auto name = function.path.substr(function.path.find_last_of('.') + 1u);
        return QuestVr::ScriptDispatch::IsEnabled(name, context.classProbeMask, context.codeMasks, context.disabledNames, context.liveProbeMask);
    }
    std::shared_ptr<const Function> PrepareFunction(const Function& identity, const std::string&) override {
        return FunctionFor(Object(identity.path));
    }
    std::vector<Property> FunctionParameters(const Function& identity) override {
        const auto declaration=FunctionFor(Object(identity.path));
        if (declaration->source!=identity.source || declaration->nativeIndex!=identity.nativeIndex ||
            declaration->flags!=identity.flags)
            throw std::runtime_error("Function argument schema changed declaration identity");
        return QuestVr::Vm::Host::FunctionParameters(*declaration);
    }
    std::shared_ptr<const Function> ResolveEvent(const std::string& receiver, const std::string& name,
        const bool enumDispatch) override {
        RuntimeObject* actor = Object(receiver);
        if (!IsDerivedFromPath(actor->cls, "Engine.Actor")) throw std::runtime_error("Event receiver is not Engine.Actor");
        const auto context = CurrentDispatchContext(actor);
        const bool enabled = QuestVr::ScriptDispatch::IsEnabled(name, context.classProbeMask, context.codeMasks, context.disabledNames, context.liveProbeMask);
        if (!enabled) return {};
        const auto value = Read(actor, PropertyNamed(actor, "Level"), 0u);
        if (value.kind != Kind::Object || value.text.empty()) throw std::runtime_error("Event receiver has no authored Level binding");
        auto* level = Object(value.text);
        if (!IsDerivedFromPath(level->cls, "Engine.LevelInfo")) throw std::runtime_error("Event receiver Level is not Engine.LevelInfo");
        const auto begun = QuestVr::Vm::ToBool(Read(level, PropertyNamed(level, "bBegunPlay"), 0u));
        if (!QuestVr::ScriptDispatch::MayCallEvent(name, enabled, begun, ReadInheritedRuntimeBool(actor, "bDeleteMe"), enumDispatch)) return {};
        const auto path = context.exactSelected ?
            QuestVr::ScriptDispatch::ResolveFunctionAtSelectedNode(DispatchGraph(),
                actor->cls->reflection.objectPath, context.selectedPath, name) :
            QuestVr::ScriptDispatch::ResolveFunction(DispatchGraph(), actor->cls->reflection.objectPath, context.stateName, name);
        return path ? FunctionIdentity(Object(*path)) : nullptr;
    }
    const QuestVr::StateObject* ReadState(const std::string& receiver) override {
        auto* actor = Object(receiver);
        if (!IsDerivedFromPath(actor->cls, "Engine.Actor")) throw std::runtime_error("State receiver is not Engine.Actor");
        static_cast<void>(CurrentDispatchContext(actor)); // Reject unsupported raw continuations.
        return actor->stateObject ? &*actor->stateObject : nullptr;
    }
    QuestVr::StateObject* MutableState(const std::string& receiver) override {
        auto* actor = Object(receiver);
        if (!actor->stateObject) return nullptr;
        Touch(actor); actor->committedScriptState = true;
        return &*actor->stateObject;
    }
    bool IsStateDeleted(const std::string& receiver) override {
        return ReadInheritedRuntimeBool(Object(receiver), "bDeleteMe");
    }
    bool PollState(const std::string& receiver, const float elapsed, QuestVr::Vm::Execution& execution) override {
        auto* actor = Object(receiver);
        const auto* state = ReadState(receiver);
        // Original ProcessState does not poll null code, nor run dormant raw
        // continuations. Explicit GotoState is required to establish live code.
        if (!state || !state->frameOverride || !state->frame || state->frame->codePath.empty()) return false;
        if (state->frame->control && !state->frame->control->codePresent) return false;
        if (QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor,"bDeleteMe"),0u))) return false;
        const auto role = QuestVr::Vm::ToInt(Read(actor, PropertyNamed(actor,"Role"),0u));
        auto* selected = Object(state->frame->control ? state->frame->control->selectedPath : state->frame->codePath);
        const auto* metadata = StateMetadata(selected);
        if (!metadata) throw std::runtime_error("State poll selection is not an original State/Class");
        if (role < 4 && !(metadata->stateFlags & 4u)) return false;
        if (state->frame->latent == QuestVr::StateLatent::Sleep) {
            if (!actor->latentTimeLeft) throw std::runtime_error("Sleeping actor has no native latent timer");
            float next = *actor->latentTimeLeft;
            const bool released = QuestVr::PollActorSleep(next, elapsed);
            Touch(actor); actor->latentTimeLeft = next; actor->committedScriptState = true;
            if (released) actor->stateObject->frame->latent = QuestVr::StateLatent::Continue;
        } else if (state->frame->latent == QuestVr::StateLatent::WaitForLanding) {
            if (!IsDerivedFromPath(actor->cls,"Engine.Pawn"))
                throw std::runtime_error("Landing wait receiver is not Engine.Pawn");
            if (!actor->latentTimeLeft) throw std::runtime_error("Landing actor has no native latent timer");
            const auto physics = QuestVr::Vm::ToInt(Read(actor,PropertyNamed(actor,"Physics"),0u));
            if (physics != 2) {
                // Original poll528 releases before touching the shared timer.
                Touch(actor); actor->committedScriptState = true;
                actor->stateObject->frame->latent = QuestVr::StateLatent::Continue;
            } else {
                float next = *actor->latentTimeLeft;
                const bool timedOut = QuestVr::PollActorLanding(next, elapsed);
                Touch(actor); actor->latentTimeLeft = next; actor->committedScriptState = true;
                if (timedOut) {
                    // Dynamic, synchronous ProcessEvent; same interpreter/root
                    // transaction. Never rewrite a callback's replacement frame.
                    static_cast<void>(execution.CallEvent(receiver,"LongFall",false,{}));
                    if (QuestVr::Vm::ToBool(Read(actor,PropertyNamed(actor,"bDeleteMe"),0u))) return false;
                }
            }
        } else if (state->frame->latent != QuestVr::StateLatent::Continue &&
                   state->frame->latent != QuestVr::StateLatent::Stop)
            throw std::runtime_error("State latent action requires its runtime handler");
        return true;
    }
    std::shared_ptr<const Function> StateProgram(const std::string&, const std::string& path) override {
        return StateProgram(Object(path));
    }
    std::shared_ptr<const Function> StateProgram(RuntimeObject* object) {
        if (!object->stateDescriptor || LowerAscii(std::filesystem::path(object->sourcePath).extension().string()) == ".dx")
            throw std::runtime_error("Executable state is not an original script State");
        const auto cached = functions_.find(object->reflection.objectPath);
        if (cached != functions_.end()) return cached->second;
        auto program = std::make_shared<Function>();
        program->path = object->reflection.objectPath; program->source = object->sourcePath;
        program->bytecode = object->stateDescriptor->bytecode;
        std::vector<RuntimeObject*> ancestry;
        std::set<RuntimeObject*> visited;
        for (auto* state = object; state;) {
            if (!state->stateDescriptor || visited.size() >= 256u || !visited.insert(state).second)
                throw std::runtime_error("State local inheritance is invalid/cyclic/deep");
            ancestry.push_back(state);
            const auto base = state->stateDescriptor->baseField;
            state = base ? Object(Qualified(Table(state->sourcePath), base)) : nullptr;
        }
        std::set<std::string> properties;
        std::size_t elements{};
        for (auto it = ancestry.rbegin(); it != ancestry.rend(); ++it) {
            auto* owner = *it;
            std::set<RuntimeObject*> fields;
            auto child = owner->stateDescriptor->children;
            while (child) {
                auto* field = Object(Qualified(Table(owner->sourcePath), child));
                if (fields.size() >= 8192u || !fields.insert(field).second || field->outer != owner)
                    throw std::runtime_error("State local Children/Next ownership/cycle budget failed");
                if (field->property) {
                    auto property = Describe(field);
                    if (!properties.insert(LowerAscii(property.key)).second || property.arrayDimension == 0u ||
                        property.arrayDimension > 16384u || elements > 16384u - property.arrayDimension)
                        throw std::runtime_error("State local schema identity/element budget failed");
                    elements += property.arrayDimension;
                    program->variables.push_back(std::move(property)); child = field->property->nextField;
                } else if (field->script) child = field->script->nextField;
                else if (const auto* metadata = StateMetadata(field)) child = metadata->nextField;
                else {
                    if (!field->commonFieldLinks) field->commonFieldLinks = LoadPortableFieldLinks(Table(field->sourcePath), field->exportIndex);
                    child = field->commonFieldLinks->nextField;
                }
            }
        }
        functions_.emplace(program->path, program); return program;
    }
    std::shared_ptr<Reference> StateVariable(const std::string& receiver, const Property& property) override {
        return StateReference(Object(receiver), property, 0u);
    }
    std::uint64_t StateLocalRevision(const std::string& receiver) override {
        return Object(receiver)->stateLocalsRevision;
    }
    void GotoStateLabel(const std::string& receiver, const std::string& label, const bool transition) override {
        auto* actor = Object(receiver);
        auto* frame = ControlFrame(actor);
        if (!frame) return; // Original GotoLabel never manufactures a frame.
        const auto normalized = label.empty() ? std::string{"None"} : label;
        const auto target = transition && QuestVr::ScriptDispatch::FoldName(normalized) == "none" ? "Begin" : normalized;
        if (QuestVr::StateNameEqual(target, "None")) { frame->control->codePresent = false; return; }
        for (const auto& path : QuestVr::ScriptDispatch::SelectedNodeAncestry(DispatchGraph(),
            actor->cls->reflection.objectPath, frame->control->selectedPath)) {
            auto* code = Object(path);
            if (!code->stateDescriptor) {
                // Original GOTY Class scripts have no executable labels. Do
                // not mistake their instance properties for state locals.
                if (!StateMetadata(code)) throw std::runtime_error("Label ancestry is not State/Class");
                continue;
            }
            const auto program = StateProgram(code);
            const auto layout = QuestVr::Vm::AnalyzeProgram(*this, *program);
            const auto found = std::find_if(layout.labels.begin(), layout.labels.end(), [&](const auto& value) {
                return QuestVr::StateNameEqual(value.name, target);
            });
            if (found == layout.labels.end()) continue;
            if (frame->localsCodePath.empty() && (!frame->locals.empty() || !program->variables.empty()))
                throw std::runtime_error("State label requires unsupported absent persistent-local storage");
            const auto statement = std::lower_bound(layout.statementOffsets.begin(), layout.statementOffsets.end(), found->offset);
            frame->codePath = code->reflection.objectPath;
            frame->statementIndex = static_cast<std::uint32_t>(statement - layout.statementOffsets.begin());
            frame->control->codePresent = true;
            ValidateStateBudget();
            return;
        }
        frame->control->codePresent = false; // Miss retains Node, stale PC, locals, mask and timer.
        __android_log_print(ANDROID_LOG_WARN, "quest_main", "DeusExQuest: state label %s not found on %s",
            target.c_str(), receiver.c_str());
    }
    void Begin() override {
        if (transaction_) throw std::runtime_error("Portable actor VM nested host transaction");
        ValidateStateBudget();
        lifecycleOperations_ = 0u;
        beforeExportCount_ = persistentRuntime->get()->exports.size();
        beforeWorldSlots_ = persistentWorldActors.size();
        beforeRandomSeed_ = persistentScriptRandomSeed;
        beforeWindowObjects_=persistentWindowDomain && persistentWindowDomain->get() ?
            persistentWindowDomain->get()->objects.size() : 0u;
        beforeWindowRoots_=persistentWindowDomain && persistentWindowDomain->get() ?
            persistentWindowDomain->get()->roots.size() : 0u;
        clientDependent_ = false;
        transaction_ = true;
    }
    void Commit() override {
        if (clientDependent_ && !persistentNativeClient.HasViewport())
            throw std::runtime_error("Native client lifetime revoked during script transaction");
        ValidateStateBudget();
        // AI-manager-only state already has independent checkpoint/journal
        // ownership and is not actor geometry. Preserve that existing path;
        // coupled AI effects still disqualify the narrow timer-only fast path.
        const bool worldChanged = beforeDecals_.has_value() || beforeCollision_.has_value() ||
            persistentRuntime->get()->exports.size() != beforeExportCount_ ||
            std::any_of(saved_.begin(), saved_.end(), [this](const auto& entry) {
            return !entry.first->classDescriptor && IsDerivedFromPath(entry.first->cls, "Engine.Actor") &&
                (!savedAi_.empty() || !OnlyActorTimerChanged(entry.first,entry.second));
        });
        if (worldChanged && persistentWorldRevision == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Portable world revision budget exceeded");
        // A birth or live Mesh write can introduce assets absent from the map's
        // original actors. Prepare missing immutable CPU geometry before the
        // revision reaches Quest consumers; do not publish a fabricated cube.
        // Previously decoded resources are retained (no XR-thread redecoding).
        if (worldChanged) static_cast<void>(DecodePortableRuntimeActorMeshes());
        decltype(persistentMapTagIndex) tags;
        if (worldChanged) {
            for (auto* actor : persistentWorldActors) {
                if (!actor || actor->worldRemoved) continue;
                const auto tag = LowerAscii(ReadInheritedRuntimeName(actor,"Tag"));
                if (!tag.empty() && tag != "none") tags[tag].push_back(actor);
            }
        }
        // Immutable asset preparation above may take time on the CPU owner.
        // Recheck the render-owner lease at the actual publication boundary.
        if (clientDependent_ && !persistentNativeClient.HasViewport())
            throw std::runtime_error("Native client lifetime revoked before script publication");
        if (worldChanged) persistentMapTagIndex.swap(tags);
        if (worldChanged) ++persistentWorldRevision;
        beforeCollision_.reset();
        saved_.clear(); savedAi_.clear(); provisionalAi_.clear(); provisionalWindows_.clear(); beforeDecals_.reset(); transaction_ = false;
    }
    void Rollback() noexcept override {
        if (!transaction_) return;
        persistentScriptRandomSeed = beforeRandomSeed_;
        // Restore native bindings/edges before provisional actor identities are
        // retired. Swap-only rollback cannot allocate or invoke game scripts.
        if (beforeDecals_) {
            auto* graph = beforeDecals_->graph.get();
            if (graph && beforeDecals_->state) {
                static_cast<void>(graph->state.Swap(*beforeDecals_->state));
                graph->actorRoots.swap(beforeDecals_->actorRoots);
            }
            persistentRuntime->get()->nativeDecals = graph;
        }
        for (auto& [owner, before] : savedAi_) {
            auto* manager = before.manager.get();
            if (manager && before.state) {
                std::swap(manager->state, *before.state);
                manager->actorRoots.swap(before.actorRoots);
            }
            owner->nativeAiManager = manager;
        }
        if (beforeCollision_) persistentActorCollision.Swap(*beforeCollision_);
        for (auto& entry : saved_) {
            entry.first->scriptValues.swap(entry.second.values);
            entry.first->classDefaultValues.swap(entry.second.defaults);
            entry.first->animationClock.swap(entry.second.clock);
            entry.first->latentTimeLeft.swap(entry.second.latentTimeLeft);
            entry.first->lastRenderTime.swap(entry.second.lastRenderTime);
            entry.first->stateObject.swap(entry.second.state);
            entry.first->stateLocalsRevision = entry.second.revision;
            entry.first->stateTransitionFlags = entry.second.transitionFlags;
            entry.first->committedScriptState = entry.second.committed;
            entry.first->childActors.swap(entry.second.children);
            entry.first->basedActors.swap(entry.second.based);
            entry.first->touchEventSent = entry.second.touchSent;
            entry.first->committedLifecycle = entry.second.lifecycle;
            entry.first->worldRemoved = entry.second.removed;
            entry.first->nativePlayerBinding = entry.second.playerBinding;
            entry.first->nativePlayer = persistentNativeClient.HasClient() ? entry.second.player : nullptr;
            entry.first->nativeViewportActor = persistentNativeClient.HasClient() ? entry.second.viewportActor : nullptr;
            entry.first->nativeRootWindow=entry.second.rootWindow;
            entry.first->uniqueObjectCounter=entry.second.uniqueCounter;
            entry.first->nativeWindowPropertyRoots.swap(entry.second.windowPropertyRoots);
            entry.first->nativeWindowState.swap(entry.second.windowState);
        }
        if(persistentWindowDomain && persistentWindowDomain->get()) {
            auto* domain=persistentWindowDomain->get();
            domain->roots.resize(beforeWindowRoots_);
            for(std::size_t index=beforeWindowObjects_;index<domain->objects.size();++index) {
                auto* object=domain->objects[index];
                persistentQualifiedObjects.erase(object->reflection.objectPath);
                persistentVmObjects.erase(LowerAscii(object->reflection.objectPath));
            }
            domain->objects.resize(beforeWindowObjects_);
        }
        // Rooted births are appended during the call so nested bytecode can
        // address them. Remove every provisional identity after restoring old
        // references; no allocating work or callbacks occur during rollback.
        for (std::size_t i = beforeExportCount_; i < persistentRuntime->get()->exports.size(); ++i) {
            auto* object = persistentRuntime->get()->exports[i];
            persistentQualifiedObjects.erase(object->reflection.objectPath);
            persistentVmObjects.erase(object->birthVmKey);
        }
        persistentRuntime->get()->exports.resize(beforeExportCount_);
        persistentWorldActors.resize(beforeWorldSlots_);
        beforeCollision_.reset(); saved_.clear(); savedAi_.clear(); provisionalAi_.clear(); provisionalWindows_.clear(); beforeDecals_.reset(); transaction_ = false;
    }
    Property ResolveProperty(const Function& function, const std::int32_t reference) override {
        RuntimeObject* object = Object(ResolveObject(function, reference));
        if (!object->property) throw std::runtime_error("VM reference is not a property");
        return Describe(object);
    }
    std::string ResolveName(const Function& function, const std::int32_t index) override {
        const auto& table = Table(function.source);
        if (index < 0 || static_cast<std::size_t>(index) >= table.names.size())
            throw std::runtime_error("VM function-local name index is out of range");
        return table.names[static_cast<std::size_t>(index)].Name.ToString();
    }
    std::string ResolveObject(const Function& function, const std::int32_t reference) override {
        if (reference == 0) return {};
        const auto& table = Table(function.source);
        const auto path = GetPortableObjectPath(table, reference);
        if (path.empty()) throw std::runtime_error("VM function-local object reference is out of range");
        return reference > 0 ? PackageStem(table.sourcePath) + '.' + path : path;
    }
    std::string ResolveCastClass(const Function& function, const std::int32_t reference) override {
        return ResolveCastReference(Table(function.source), reference);
    }
    std::string ObjectString(const std::string& path) override {
        auto* object = Object(path); // Validate identity and canonicalize spelling.
        // Original initialized default blocks have Name None and Outer null.
        // This display spelling does not replace their distinct nonnull handle.
        if (object->defaultOwner) return "None";
        if(object->nativeClientIdentity || object->nativeClientClass || object->nativeWindowObject)
            return object->reflection.objectPath;
        // No native Rename/SetOuter exists in this host. Loaded object names
        // and complete package/outer paths are immutable, including nested
        // function/property outers. Validate metadata instead of returning an
        // arbitrary operand spelling or reducing GetPathName to a leaf name.
        if (!object->runtimeBirth) {
            if (object->exportIndex >= Table(object->sourcePath).exports.size() ||
                object->exportIndex >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
                throw std::runtime_error("ObjectToString identity has no authored export");
            const auto authored = Qualified(Table(object->sourcePath),
                static_cast<std::int32_t>(object->exportIndex+1u));
            if (authored != object->reflection.objectPath)
                throw std::runtime_error("ObjectToString authored outer path disagrees with indexed identity");
        } else {
            RequireMapActor(object);
            if (object->reflection.objectPath.rfind(persistentMapPackageName+'.',0u) != 0u)
                throw std::runtime_error("ObjectToString birth has no current-map package identity");
        }
        return object->reflection.objectPath;
    }
    Value CastObject(const std::string& target, const Value& value, const bool meta) override {
        QuestVr::ObjectCastResolvers resolvers;
        resolvers.resolveClass = [this](const std::string& path) { return CastClassIdentity(path); };
        resolvers.resolveObject = [this](const std::string& path) { return CastObjectIdentity(path); };
        return QuestVr::CastAuthoredObject(target, value, meta, resolvers);
    }
    std::shared_ptr<const Function> ResolveFunction(const Function& caller,
        const std::string& receiver, const QuestVr::Vm::Invocation& invocation) override {
        if (invocation.kind == QuestVr::Vm::CallKind::Final)
            return FunctionIdentity(Object(ResolveObject(caller, invocation.reference)));
        if (invocation.kind == QuestVr::Vm::CallKind::Global) {
            auto* object = Object(receiver);
            const auto selected = QuestVr::ScriptDispatch::ResolveFunction(DispatchGraph(),
                object->cls->reflection.objectPath, "None", invocation.name, QuestVr::ScriptDispatch::LookupKind::Global);
            if (!selected) throw std::runtime_error("Portable global function is unavailable: " + invocation.name);
            return FunctionIdentity(Object(*selected));
        }
        return Member(Object(receiver), invocation.name);
    }
    std::shared_ptr<Reference> Variable(const std::string& receiver,
        const Property& property, const QuestVr::Vm::Scope scope) override {
        RuntimeObject* object = Object(receiver);
        if (!object->active && !object->worldRemoved) throw std::runtime_error("VM receiver is inactive");
        if (object->classDescriptor || object->nativeClientClass)
            throw std::runtime_error("UClass reflected receiver requires its actual Core.Class storage");
        const bool defaults = scope == QuestVr::Vm::Scope::Default;
        auto result = ReferenceFor(object, property, 0u, defaults);
        // Native writes do not authorize ordinary compiled script to mutate
        // Const SurfList or the audited scalar Actor.TimerCounter. Keep this
        // explicit port policy narrow; native ReferenceFor remains writable.
        const auto nativeKey=LowerAscii(property.key);
        const bool nativeConst = ((property.flags & 2u) && (property.zero.kind == Kind::IntArray ||
            nativeKey == "engine.actor.timercounter")) ||
            (object->nativeViewportIdentity && nativeKey=="engine.player.actor") ||
            (object->nativePlayerBinding && nativeKey=="engine.playerpawn.player") ||
            nativeKey=="extension.playerpawnext.rootwindow";
        if (nativeConst) result->write = {};
        result->dimension = property.arrayDimension;
        result->element = [this, object, property, defaults, nativeConst](const std::size_t slot) {
            if (slot >= property.arrayDimension || slot > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("VM property fixed-array index is out of range");
            auto element = ReferenceFor(object, property, static_cast<std::uint32_t>(slot), defaults);
            if (nativeConst) element->write = {};
            return element;
        };
        return result;
    }
    Evaluation Native(const std::uint16_t index, const std::string& receiver,
        const std::vector<Evaluation>& arguments, const Function* declaration) override {
        if (index == 280u)
            throw std::runtime_error("SetTimer requires its budgeted VM execution context");
        RuntimeObject* object = Object(receiver);
        if (object->defaultOwner)
            throw std::runtime_error("Native execution on a class-default object requires its audited handler");
        if (index==0u && declaration && LowerAscii(declaration->path)=="engine.playerpawn.consolecommand")
            return PlayerPawnConsoleCommand(object,arguments,*declaration);
        // Native event wrappers have no presentation-active gate. Their actual
        // identity/Level checks remain in AiNative; unrelated native receivers
        // retain the existing explicit inactive guard.
        if (index == 650u || index == 710u || index == 711u ||
            (index >= 713u && index <= 716u))
            return AiNative(index, object, arguments);
        const auto argumentCount = [&](const std::size_t required, const std::size_t maximum) {
            if (arguments.size() < required || arguments.size() > maximum)
                throw std::runtime_error("VM native argument count is invalid for " + std::to_string(index));
        };
        const auto argument = [&](const std::size_t slot) {
            return slot < arguments.size() ? arguments[slot].Load() : Value{};
        };
        const auto optionalFloat = [&](const std::size_t slot, const float fallback) {
            const auto value = argument(slot);
            return value.kind == Kind::Nothing ? fallback : QuestVr::Vm::ToFloat(value);
        };
        const auto sequence = [&]() {
            const auto value = argument(0u);
            if (value.kind != Kind::Name) throw std::runtime_error("Animation native requires a Name argument");
            return value.text;
        };
        if (index == 231u) { // Original Core.Object.Log, fixed-format diagnostic sink.
            argumentCount(1u,2u);
            const auto message = QuestVr::Vm::Coerce(argument(0u),Value::Text(Kind::String,{}));
            const auto category = argument(1u);
            if (category.kind != Kind::Nothing && category.kind != Kind::Name)
                throw std::runtime_error("Log category is not a Name");
            const auto tag = category.kind == Kind::Nothing ? std::string("ScriptLog") :
                (category.text.empty() ? std::string("None") : category.text);
            const auto units = QuestVr::UnrealString::Decode(message.text);
            const auto text = QuestVr::UnrealString::Encode(QuestVr::UnrealString::CString(units));
            __android_log_print(ANDROID_LOG_INFO,"DeusExScript","[%s] %s",tag.c_str(),text.c_str());
            return {};
        }
        if (index == 167u || index == 195u) { // Original Core.Object.Rand / FRand.
            argumentCount(index == 167u ? 1u : 0u, index == 167u ? 1u : 0u);
            const auto maximum = index == 167u ? QuestVr::Vm::ToInt(argument(0u)) : 0;
            if (index == 167u && maximum <= 0) return {Value::Integer(0), {}};
            if (!transaction_) throw std::runtime_error("Script random draw outside host transaction");
            if (!persistentScriptRandomSeed) persistentScriptRandomSeed = QuestVr::ScriptRandom::InitialSeed;
            if (index == 167u) return {Value::Integer(QuestVr::ScriptRandom::Rand(*persistentScriptRandomSeed, maximum)), {}};
            return {Value::Float(QuestVr::ScriptRandom::FRand(*persistentScriptRandomSeed)), {}};
        }
        if (index == 256u) {
            argumentCount(1u,1u);
            const float seconds = QuestVr::Vm::ToFloat(argument(0u));
            if (!std::isfinite(seconds)) throw std::runtime_error("Sleep requires finite Seconds");
            if (!IsDerivedFromPath(object->cls,"Engine.Actor")) throw std::runtime_error("Sleep receiver is not Engine.Actor");
            if (!object->stateObject || !object->stateObject->frameOverride || !object->stateObject->frame)
                throw std::runtime_error("Sleep requires an established portable actor frame");
            Touch(object); object->latentTimeLeft = seconds; object->committedScriptState = true;
            object->stateObject->frame->latent = QuestVr::StateLatent::Sleep;
            return {};
        }
        if (index == 527u) {
            argumentCount(0u,0u);
            if (!IsDerivedFromPath(object->cls,"Engine.Pawn")) throw std::runtime_error("WaitForLanding receiver is not Engine.Pawn");
            const auto physics = QuestVr::Vm::ToInt(Read(object,PropertyNamed(object,"Physics"),0u));
            if (physics == 2 && (!object->stateObject || !object->stateObject->frameOverride || !object->stateObject->frame))
                throw std::runtime_error("Falling WaitForLanding requires an established portable actor frame");
            // Original exec527 always writes the shared native counter, even
            // when nonfalling; only Falling installs the latent action528.
            Touch(object); object->latentTimeLeft = 2.5f; object->committedScriptState = true;
            if (physics == 2) object->stateObject->frame->latent = QuestVr::StateLatent::WaitForLanding;
            return {};
        }
        if (index == 0u && declaration && LowerAscii(declaration->path) == "engine.pawn.stopwaiting") {
            argumentCount(0u,0u);
            if (!IsDerivedFromPath(object->cls,"Engine.Pawn")) throw std::runtime_error("StopWaiting receiver is not Engine.Pawn");
            if (object->stateObject && object->stateObject->frameOverride && object->stateObject->frame &&
                object->stateObject->frame->latent == QuestVr::StateLatent::Sleep) {
                Touch(object); object->latentTimeLeft = -1.0f; object->committedScriptState = true;
            }
            return {};
        }
        if (!object->active && !object->worldRemoved) throw std::runtime_error("VM native receiver is inactive");
        if (index == 720u) { // Actor.GetPlayerPawn, pinned NActor.cpp current-Level fallback.
            argumentCount(0u, 0u);
            // No viewport.Actor/player possession is established yet. The
            // fallback searches actual Level slots, not snapshots, export order,
            // bIsPlayer or the actor nearest the VR camera. A missing player is
            // a real Object None; do not substitute a PlayerStart or an NPC.
            for (auto* candidate : persistentWorldActors) {
                LifecycleStep();
                if (!candidate || candidate->worldRemoved ||
                    !IsDerivedFromPath(candidate->cls, "Engine.PlayerPawn")) continue;
                const auto player = Read(candidate, PropertyNamed(candidate, "Player"), 0u);
                if (player.kind != Kind::Object)
                    throw std::runtime_error("GetPlayerPawn Player binding is not an Object");
                if (!player.text.empty())
                    return {Value::Text(Kind::Object, candidate->reflection.objectPath), {}};
            }
            return {Value::Text(Kind::Object, {}), {}};
        }
        if (index == 262u) { // Actor.SetCollision, optional arguments preserve current flags.
            argumentCount(0u, 3u); RequireMapActor(object);
            std::array<Value,3u> flags;
            const std::array<const char*,3u> names{"bCollideActors","bBlockActors","bBlockPlayers"};
            for (std::size_t i=0u;i<flags.size();++i) {
                flags[i]=argument(i);
                if (flags[i].kind==Kind::Nothing) flags[i]=Read(object,PropertyNamed(object,names[i]),0u);
                if (flags[i].kind!=Kind::Bool) throw std::runtime_error("SetCollision argument is not Bool");
            }
            TouchCollision(); persistentActorCollision.Remove(reinterpret_cast<std::uintptr_t>(object));
            for (std::size_t i=0u;i<flags.size();++i)
                ReferenceFor(object,PropertyNamed(object,names[i]),0u,false)->write(flags[i]);
            RegisterCollisionActor(object,persistentActorCollision);
            return {};
        }
        if (index == 283u) { // Actor.SetCollisionSize, pinned UActor_Phys.cpp.
            argumentCount(2u, 2u);
            if (!IsDerivedFromPath(object->cls, "Engine.Actor"))
                throw std::runtime_error("SetCollisionSize receiver is not Engine.Actor");
            const auto numeric = [&](const std::size_t slot) {
                const auto value = argument(slot);
                if (value.kind != Kind::Byte && value.kind != Kind::Int && value.kind != Kind::Float)
                    throw std::runtime_error("SetCollisionSize requires two numeric floats");
                const auto result = QuestVr::Vm::ToFloat(value);
                if (!std::isfinite(result)) throw std::runtime_error("SetCollisionSize bounds are not finite");
                return result;
            };
            const auto radius = numeric(0u), height = numeric(1u);
            const auto radiusProperty = PropertyNamed(object, "CollisionRadius");
            const auto heightProperty = PropertyNamed(object, "CollisionHeight");
            if (radiusProperty.zero.kind != Kind::Float || heightProperty.zero.kind != Kind::Float ||
                radiusProperty.arrayDimension != 1u || heightProperty.arrayDimension != 1u)
                throw std::runtime_error("SetCollisionSize authored bounds are not scalar floats");
            // The pin's room-fit refusal is TODO: it assigns both fields and
            // returns true. Preserve that rather than inventing a placement
            // test or clamp. Remove using the cached box before publishing the
            // new fields, then register their actual current bounds.
            TouchCollision(); persistentActorCollision.Remove(reinterpret_cast<std::uintptr_t>(object));
            ReferenceFor(object, radiusProperty, 0u, false)->write(Value::Float(radius));
            ReferenceFor(object, heightProperty, 0u, false)->write(Value::Float(height));
            RegisterCollisionActor(object,persistentActorCollision);
            return {Value::Bool(true), {}};
        }
        if (index == 3970u) { // Actor.SetPhysics, pinned UActor_Phys.cpp.
            argumentCount(1u, 2u);
            if (!IsDerivedFromPath(object->cls, "Engine.Actor"))
                throw std::runtime_error("SetPhysics receiver is not Engine.Actor");
            const auto physics = argument(0u);
            if (physics.kind != Kind::Byte && physics.kind != Kind::Int && physics.kind != Kind::Float)
                throw std::runtime_error("SetPhysics requires a numeric physics byte");
            const auto floor = argument(1u);
            if (floor.kind != Kind::Nothing && floor.kind != Kind::Object)
                throw std::runtime_error("SetPhysics optional floor requires an Object");
            // Pinned SetPhysics_Deus leaves the optional floor unused/TODO.
            // Do not invent SetBase, velocity reset or a physics simulation.
            const auto property = PropertyNamed(object, "Physics");
            if (property.zero.kind != Kind::Byte || property.arrayDimension != 1u)
                throw std::runtime_error("SetPhysics authored Physics schema is not a scalar byte");
            ReferenceFor(object, property, 0u, false)->write(QuestVr::Vm::Coerce(physics, property.zero));
            return {};
        }
        if (index == 284u || index == 281u) {
            argumentCount(index == 284u ? 0u : 1u, index == 284u ? 0u : 1u);
            const auto context = CurrentDispatchContext(object);
            if (index == 284u) return {Value::Text(Kind::Name, context.stateName), {}};
            const auto testState = argument(0u);
            if (testState.kind != Kind::Name) throw std::runtime_error("IsInState requires a Name");
            if (context.exactSelected) {
                const auto ancestry = QuestVr::ScriptDispatch::SelectedNodeAncestry(DispatchGraph(),
                    object->cls->reflection.objectPath, context.selectedPath);
                const auto requested = QuestVr::ScriptDispatch::FoldName(testState.text);
                const bool matches = std::any_of(ancestry.begin(), ancestry.end(), [&](const auto& path) {
                    return QuestVr::ScriptDispatch::FoldName(path.substr(path.find_last_of('.') + 1u)) == requested;
                });
                return {Value::Bool(matches), {}};
            }
            return {Value::Bool(QuestVr::ScriptDispatch::FoldName(testState.text) ==
                QuestVr::ScriptDispatch::FoldName(context.stateName)), {}};
        }
        if (index == 303u) { // Core.Object.IsA
            argumentCount(1u, 1u);
            const auto value = argument(0u);
            if (value.kind != Kind::Name) throw std::runtime_error("IsA requires a Name argument");
            bool found{};
            for (RuntimeObject* cls = object->cls; cls != nullptr; cls = cls->base) {
                const auto dot = cls->reflection.objectPath.find_last_of('.');
                if (LowerAscii(cls->reflection.objectPath.substr(dot + 1u)) == LowerAscii(value.text)) {
                    found = true; break;
                }
            }
            return {Value::Bool(found), {}};
        }
        if (index != 259u && index != 260u && index != 294u && index != 263u && index != 293u && index != 282u &&
            index != 1010u && index != 1012u)
            throw std::runtime_error("Unsupported runtime native " + std::to_string(index) +
                (declaration ? " (" + declaration->path + ")" : std::string()));
        if (!IsDerivedFromPath(object->cls, "Engine.Actor"))
            throw std::runtime_error("Animation native receiver is not Engine.Actor");
        const bool blend = index == 1010u || index == 1012u;
        argumentCount(index == 282u ? 0u : index == 294u || index == 1012u ? 2u : 1u,
            index == 260u || index == 1010u ? 4u : index == 259u || index == 1012u ? 3u :
            index == 294u ? 2u : index == 282u ? 0u : 1u);
        if (index == 282u) return {Value::Bool(ReadInheritedRuntimeFloat(object, "AnimRate") != 0.0f), {}};
        QuestVr::ActorAnimationCommand command;
        command.sequence = sequence();
        command.kind = index == 259u ? QuestVr::ActorAnimationCommandKind::PlayAnim : index == 260u ?
            QuestVr::ActorAnimationCommandKind::LoopAnim : index == 1010u ?
            QuestVr::ActorAnimationCommandKind::PlayBlendAnim : index == 1012u ?
            QuestVr::ActorAnimationCommandKind::TweenBlendAnim : QuestVr::ActorAnimationCommandKind::TweenAnim;
        command.rate = optionalFloat(1u, 1.0f);
        command.tweenTime = optionalFloat(index == 294u || index == 1012u ? 1u : 2u, index == 1010u ? -1.0f : 0.0f);
        if (blend) {
            const auto slot = argument(index == 1010u ? 3u : 2u);
            if (slot.kind != Kind::Nothing && slot.kind != Kind::Int && slot.kind != Kind::Byte)
                throw std::runtime_error("Blend animation slot requires an Int");
            command.blendSlot = slot.kind == Kind::Nothing ? 0 : QuestVr::Vm::ToInt(slot);
            // Original signed slot warning/return, not Byte wrapping or an
            // unsupported-native success. All arguments have been evaluated.
            if (command.blendSlot < 0 || command.blendSlot >= 4) return {};
        } else command.minRate = optionalFloat(3u, 0.0f);
        const auto* mesh = AnimationMesh(object);
        if (index == 263u) return {Value::Bool(QuestVr::ActorClockHasAnim(mesh, command.sequence)), {}};
        if (index == 293u) return {Value::Text(Kind::Name, QuestVr::ActorClockGetAnimGroup(mesh, command.sequence)), {}};
        if (blend && QuestVr::ActorAnimationClockDetail::Sequence(mesh, command.sequence, false, {}) == nullptr)
            return {}; // Original no-mesh/missing-sequence branch precedes clock/property access.
        QuestVr::ActorAnimationClock next = object->animationClock ? *object->animationClock : AuthoredClock(object);
        float velocitySpeed{};
        if (index == 1010u && command.tweenTime == -1.0f &&
            !QuestVr::MeshAnimationNamesEqual(next.pose.blends[command.blendSlot].sequence, "None") &&
            next.blends[command.blendSlot].oldRate < 0.0f) {
            const auto velocity = Read(object, PropertyNamed(object, "Velocity"), 0u);
            if (velocity.kind != Kind::Vector) throw std::runtime_error("Blend animation Velocity schema is not Vector");
            velocitySpeed = std::sqrt(velocity.vector[0]*velocity.vector[0] +
                velocity.vector[1]*velocity.vector[1] + velocity.vector[2]*velocity.vector[2]);
        }
        const auto applied = QuestVr::ApplyActorAnimationCommand(mesh, next, command, velocitySpeed);
        if (!applied.error.empty()) throw std::runtime_error(applied.error);
        if (applied.applied) {
            Touch(object); object->animationClock = std::move(next);
            SynchronizeClock(object); object->committedScriptState = true;
        }
        return {};
    }
    std::unique_ptr<QuestVr::Vm::Iterator> CreateIterator(const std::uint16_t index,
        const std::string& receiver, const std::vector<Evaluation>& arguments, const Function* declaration) override {
        if (index == 306u) {
            // Retail GOTY Engine.dll SHA256 0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4:
            // execBasedActors export RVA4048 -> VA103e4800. Unlike the pin's
            // snapshot of BasedActors, retail scans live Level slots, compares
            // actual Class/Super pointers and the candidate's live direct Base.
            const auto validate = [&](const Function& function) {
                if (LowerAscii(function.path) != "engine.actor.basedactors" || function.nativeIndex != 306u ||
                    function.flags != 0x405u || function.variables.size() != 2u)
                    throw std::runtime_error("Native306 is not the authored Engine.Actor.BasedActors declaration");
                const std::array<const char*,2u> keys{"engine.actor.basedactors.baseclass","engine.actor.basedactors.actor"};
                const std::array<std::uint32_t,2u> flags{0x80u,0x180u};
                for (std::size_t i=0u;i<keys.size();++i) {
                    const auto& parameter = function.variables[i];
                    if (LowerAscii(parameter.key) != keys[i] || parameter.zero.kind != Kind::Object ||
                        parameter.arrayDimension != 1u || parameter.flags != flags[i])
                        throw std::runtime_error("BasedActors requires its original Class and OUT Actor parameters");
                    auto* metadata = Object(parameter.key);
                    if (!metadata->property || metadata->property->type != (i == 0u ? "ClassProperty" : "ObjectProperty"))
                        throw std::runtime_error("BasedActors parameter metadata has the wrong reflected type");
                    const auto& table = Table(metadata->sourcePath);
                    if (LowerAscii(Qualified(table, metadata->property->referencedType)) != (i == 0u ? "core.class" : "engine.actor") ||
                        (i == 0u && LowerAscii(Qualified(table, metadata->property->secondaryType)) != "engine.actor"))
                        throw std::runtime_error("BasedActors parameter metadata has the wrong original class constraint");
                }
            };
            const auto authored = FunctionFor(Object("Engine.Actor.BasedActors"));
            validate(*authored);
            if (declaration) {
                validate(*declaration);
                if (declaration->source != authored->source)
                    throw std::runtime_error("BasedActors declaration source is not the authored Actor entry");
            }
            auto* caller = Object(receiver); RequireMapActor(caller);
            if (arguments.size() != 2u) throw std::runtime_error("BasedActors requires Class and writable Actor");
            const auto requested = arguments[0u].Load();
            if (requested.kind != Kind::Object) throw std::runtime_error("BasedActors BaseClass is not an Object/UClass");
            // Retail VA103e4885 defaults None to the real Actor UClass. Do not
            // fabricate a class from a path or reuse Name-based DynamicCast.
            const auto identity = CastClassIdentity(requested.text.empty() ? "Engine.Actor" : requested.text);
            auto* baseClass = Object(identity.path);
            if (!baseClass->classDescriptor) throw std::runtime_error("BasedActors BaseClass is not a loaded serialized UClass");
            const auto& output = arguments[1u].reference;
            if (!output || !output->read || !output->write || output->zero.kind != Kind::Object ||
                output->read().kind != Kind::Object)
                throw std::runtime_error("BasedActors Actor is not a writable Object binding");
            class BasedCursor final : public QuestVr::Vm::Iterator {
            public:
                BasedCursor(PortableActorVmHost& host, RuntimeObject* caller, RuntimeObject* baseClass,
                    std::shared_ptr<Reference> output)
                    : host_(host), caller_(caller), baseClass_(baseClass), output_(std::move(output)) {}
                std::size_t RetainedBytes() const override { return sizeof(*this); }
                bool Next() override {
                    host_.LifecycleStep();
                    // VA103e48a6 clears OUT BEFORE every scan, including a
                    // successful scan. Aliased Base OUT writes are observable.
                    output_->write(Value::Text(Kind::Object, {}));
                    // The retail loop selects the caller's native ULevel,
                    // not its reflected LevelInfo link or a cached based list.
                    // This embedding owns one world: refuse an absent/foreign
                    // native level instead of silently scanning another world.
                    static_cast<void>(host_.NativeLevelInfo(caller_));
                    while (slot_ < persistentWorldActors.size()) {
                        host_.LifecycleStep();
                        auto* actor = persistentWorldActors[slot_++];
                        if (!actor || actor->worldRemoved) continue;
                        bool matches{};
                        std::array<RuntimeObject*,128u> visited{};
                        std::size_t depth{};
                        for (auto* cls=actor->cls;cls;cls=cls->base) {
                            host_.LifecycleStep();
                            if (cls == baseClass_) { matches=true; break; }
                            if (depth == visited.size() || std::find(visited.begin(),visited.begin()+depth,cls) != visited.begin()+depth)
                                throw std::runtime_error("BasedActors Class hierarchy cycles or exceeds its native work bound");
                            visited[depth++]=cls;
                        }
                        if (!matches || host_.ActorLink(actor,"Base") != caller_) continue;
                        // No presentation-active, bDeleteMe, hidden, collision,
                        // recursive Base or cached native based-list filter.
                        output_->write(Value::Text(Kind::Object,actor->reflection.objectPath));
                        return true;
                    }
                    return false;
                }
            private:
                PortableActorVmHost& host_;
                RuntimeObject* caller_;
                RuntimeObject* baseClass_;
                std::shared_ptr<Reference> output_;
                std::size_t slot_{};
            };
            return std::make_unique<BasedCursor>(*this,caller,baseClass,output);
        }
        if (index != 304u) throw std::runtime_error("Unsupported runtime iterator native " + std::to_string(index));
        if (arguments.size() < 2u || arguments.size() > 3u)
            throw std::runtime_error("AllActors requires Class, writable Actor and optional Name");
        const auto base = arguments[0u].Load();
        if (base.kind != Kind::Object || base.text.empty())
            throw std::runtime_error("AllActors BaseClass is not a nonnull UClass");
        // Actual UClass identity is required, but membership is the pin's IsA
        // declaration-Name comparison, not a qualified-path ancestry test.
        const auto cls = CastClassIdentity(base.text);
        const auto& output = arguments[1u].reference;
        if (!output || !output->read || !output->write || output->zero.kind != Kind::Object ||
            output->read().kind != Kind::Object)
            throw std::runtime_error("AllActors Actor is not a writable Object binding");
        std::string tag;
        if (arguments.size() == 3u) {
            const auto value = arguments[2u].Load();
            if (value.kind != Kind::Nothing && value.kind != Kind::Name)
                throw std::runtime_error("AllActors MatchTag is not a Name");
            if (value.kind == Kind::Name) tag = value.text;
        }
        if (tag.size() > 8192u || tag.find('\0') != std::string::npos)
            throw std::runtime_error("AllActors MatchTag is outside its byte budget");
        tag = LowerAscii(tag);
        if (tag == "none") tag.clear();
        class ActorCursor final : public QuestVr::Vm::Iterator {
        public:
            ActorCursor(PortableActorVmHost& host, std::string cls,
                std::shared_ptr<Reference> output, std::string tag)
                : host_(host), classPath_(std::move(cls)), output_(std::move(output)), tag_(std::move(tag)) {}
            std::size_t RetainedBytes() const override {
                return sizeof(*this) + classPath_.capacity() + tag_.capacity();
            }
            bool Next() override {
                // The pin snapshots size for each Next, not at construction:
                // actors appended by a previous loop body are visible now.
                const auto size = persistentWorldActors.size();
                while (index_ < size) {
                    host_.LifecycleStep();
                    auto* actor = persistentWorldActors[index_++];
                    // Native Destroy clears the Level slot after callbacks.
                    // bDeleteMe, presentation active/hidden and collision flags
                    // do not remove an otherwise live actor from AllActors.
                    if (!actor || actor->worldRemoved) continue;
                    const auto value = Value::Text(Kind::Object, actor->reflection.objectPath);
                    if (host_.CastObject(classPath_, value, false).text.empty()) continue;
                    if (!tag_.empty()) {
                        const auto liveTag = host_.Read(actor, host_.PropertyNamed(actor, "Tag"), 0u);
                        if (liveTag.kind != Kind::Name) throw std::runtime_error("AllActors live Tag is not a Name");
                        if (LowerAscii(liveTag.text) != tag_) continue;
                    }
                    output_->write(value);
                    return true;
                }
                output_->write(Value::Text(Kind::Object, {}));
                return false;
            }
        private:
            PortableActorVmHost& host_;
            std::string classPath_;
            std::shared_ptr<Reference> output_;
            std::string tag_;
            std::size_t index_{};
        };
        return std::make_unique<ActorCursor>(*this, cls.path, output, std::move(tag));
    }
    std::optional<Value> CaptureNativeContext(const std::uint16_t index, const std::string& receiver,
        const Function*) override {
        if (index != 113u) return std::nullopt;
        return Value::Text(Kind::Name, TransitionStateName(Object(receiver)));
    }
    Evaluation NativeWithContext(const std::uint16_t index, const std::string& receiver,
        const std::vector<Evaluation>& arguments, const Function* declaration, QuestVr::Vm::Execution& execution,
        const std::optional<Value>& context) override {
        if (Object(receiver)->defaultOwner)
            throw std::runtime_error("Native execution on a class-default object requires its audited handler");
        if (index != 113u) return NativeWithExecution(index, receiver, arguments, declaration, execution);
        auto* actor = Object(receiver);
        if ((!actor->active && !actor->worldRemoved) || !IsDerivedFromPath(actor->cls, "Engine.Actor"))
            throw std::runtime_error("State native receiver is not an active Engine.Actor");
        if (arguments.size() > 2u || !context || context->kind != Kind::Name)
            throw std::runtime_error("GotoState requires its detached pre-argument Name context");
        const auto name = [&](const std::size_t slot, const std::string& fallback) {
            if (slot >= arguments.size()) return fallback;
            const auto value = arguments[slot].Load();
            if (value.kind == Kind::Nothing) return fallback;
            if (value.kind != Kind::Name || value.text.size() > 8192u || value.text.find('\0') != std::string::npos ||
                std::any_of(value.text.begin(), value.text.end(), [](const unsigned char c) { return c < 32u || c > 126u; }))
                throw std::runtime_error("GotoState requires a bounded ASCII Name");
            return value.text.empty() ? std::string{"None"} : value.text;
        };
        const auto requested = name(0u, context->text);
        const auto label = name(1u, "None");
        // execGotoState compares against its PRE-argument capture, not the
        // selection that optional argument callbacks may have left behind.
        bool positionLabel = QuestVr::StateNameEqual(requested, context->text);
        if (!positionLabel) {
            StateControlOperations operations{*this, actor, execution};
            positionLabel = QuestVr::OriginalGotoState(operations, requested) == QuestVr::StateEntryResult::Entered;
        }
        if (positionLabel) GotoStateLabel(receiver, label, true); // AFTER BeginState, unless preempted.
        ValidateStateBudget();
        return {};
    }
    Evaluation NativeWithExecution(const std::uint16_t index, const std::string& receiver,
        const std::vector<Evaluation>& arguments, const Function* declaration, QuestVr::Vm::Execution& execution) override {
        if (Object(receiver)->defaultOwner)
            throw std::runtime_error("Native execution on a class-default object requires its audited handler");
        if(index==1052u) return InitRootWindowNative(Object(receiver),arguments,declaration,execution);
        if(index==1410u) return NewChildWindowNative(Object(receiver),arguments,declaration,execution);
        if(index==1438u) return WindowAlignmentsNative(Object(receiver),arguments,declaration,execution);
        if(index==1425u) return WindowRootGetterNative(Object(receiver),arguments,declaration);
        if(index==1416u) return WindowSensitivityNative(Object(receiver),arguments,declaration,execution);
        if(index==1467u) return WindowChildVisibilityNative(Object(receiver),arguments,declaration,execution);
        if(index==1431u) return WindowSizeNative(Object(receiver),arguments,declaration,execution);
        if(index==1430u) return WindowConfigurationNative(Object(receiver),arguments,declaration,execution);
        if(index==1421u || index==1422u)
            return WindowBackgroundNative(index,Object(receiver),arguments,declaration,execution);
        if(index==1414u) {
            static_cast<void>(WindowNativeDeclaration(index,declaration));
            if(!arguments.empty()) throw std::runtime_error("Window.Hide takes no arguments");
            RequireNativeWindow(Object(receiver));SetHiddenChildVisibility(Object(receiver),false,execution);
            return {};
        }
        if(index==1477u || index==1454u || index==1510u || index==1511u)
            return WindowPropertyNative(index,Object(receiver),arguments,declaration,execution);
        if (index == 280u) {
            auto* object = Object(receiver);
            if (!object->active && !object->worldRemoved)
                throw std::runtime_error("VM native receiver is inactive");
            if (arguments.size() != 2u)
                throw std::runtime_error("VM native argument count is invalid for 280");
            const auto parameters = ActorTimerProperties(object);
            // Native-index bytecode has no declaration pointer. Resolve the
            // real authored entry in that case; never trust index280 alone.
            const auto authored = FunctionFor(Object("Engine.Actor.SetTimer"));
            ValidateSetTimerDeclaration(*authored);
            if (declaration) {
                ValidateSetTimerDeclaration(*declaration);
                if (declaration->source != authored->source)
                    throw std::runtime_error("SetTimer declaration source is not the authored Actor entry");
            }
            const auto rate = arguments[0u].Load(), loop = arguments[1u].Load();
            if (rate.kind != Kind::Float || !std::isfinite(rate.floating) || loop.kind != Kind::Bool)
                throw std::runtime_error("SetTimer requires a finite Float rate and a Bool loop argument");
            // Retail stores the signed argument verbatim and always rearms at
            // Counter0, including an identical Rate or a disabled timer. Native
            // storage remains writable, but all stores share interpreter
            // write/retained budgets and poison the root execution on failure.
            execution.WriteReference(ReferenceFor(object,parameters.counter,0u,false),Value::Float(0.0f));
            execution.WriteReference(ReferenceFor(object,parameters.rate,0u,false),rate);
            execution.WriteReference(ReferenceFor(object,parameters.loop,0u,false),loop);
            return {};
        }
        if (index == 0u && declaration && LowerAscii(declaration->path) == "engine.decal.attachdecal")
            return AttachDecalEntryNative(Object(receiver), arguments, *declaration, execution);
        if (index == 0u && declaration && LowerAscii(declaration->path) == "engine.decal.detachdecal")
            return DetachDecalNative(Object(receiver), arguments, *declaration);
        if (index == 278u) {
            // execSpawn owns evaluated arguments across synchronous native
            // callbacks. In particular, SpawnTag may alias an actor property
            // which SetOwner/SpawnNotify changes before SpawnActor returns.
            std::vector<Evaluation> captured;
            captured.reserve(arguments.size());
            for (const auto& argument : arguments) captured.push_back({argument.Load(), {}});
            auto tag = Value::Text(Kind::Name, "None");
            if (captured.size() > 2u && captured[2].value.kind != Kind::Nothing) {
                if (captured[2].value.kind != Kind::Name) throw std::runtime_error("Spawn Tag is not a Name");
                tag = captured[2].value;
            }
            auto result = SpawnNative(receiver, captured, execution);
            if (!result.value.text.empty()) {
                // Original execSpawn writes AFTER all native callbacks and
                // notifications, to the returned actor (possibly replacement),
                // including an omitted/explicit None. Direct world GameInfo
                // births call SpawnNative itself and bypass this wrapper.
                auto* actor = Object(result.value.text);
                ReferenceFor(actor, PropertyNamed(actor, "Tag"), 0u, false)->write(tag);
            }
            return result;
        }
        if (index == 272u || index == 298u || index == 279u)
            return LifecycleNative(index, receiver, arguments, execution);
        if (index == 529u || index == 530u) return PawnListNative(index, Object(receiver), arguments);
        if (index == 113u) throw std::runtime_error("GotoState bypassed its pre-argument context");
        if (index != 117u && index != 118u) return Native(index, receiver, arguments, declaration);
        auto* actor = Object(receiver);
        if ((!actor->active && !actor->worldRemoved) || !IsDerivedFromPath(actor->cls, "Engine.Actor"))
            throw std::runtime_error("State native receiver is not an active Engine.Actor");
        const auto name = [&](const std::size_t slot, const std::string& fallback) {
            if (slot >= arguments.size()) return fallback;
            const auto value = arguments[slot].Load();
            if (value.kind == Kind::Nothing) return fallback;
            if (value.kind != Kind::Name || value.text.size() > 8192u || value.text.find('\0') != std::string::npos)
                throw std::runtime_error("State native requires a bounded Name");
            for (const unsigned char character : value.text)
                if (character < 32u || character > 126u)
                    throw std::runtime_error("State native Name is outside supported ASCII identities");
            return value.text;
        };
        if (index == 117u || index == 118u) {
            if (arguments.size() != 1u || arguments[0].Load().kind != Kind::Name)
                throw std::runtime_error("Enable/Disable requires one Name");
            const auto event = QuestVr::ScriptDispatch::FoldName(name(0u, "None"));
            // Original Core.dll execEnable/execDisable mutate only fixed
            // probes300..363. Nonprobe names (including LongFall) are no-ops,
            // not arbitrary function-disable storage or fabricated frames.
            if (!QuestVr::ScriptDispatch::EventProbeIndex(event)) return {};
            if (actor->stateObject && actor->stateObject->frameOverride && actor->stateObject->frame &&
                !actor->stateObject->frame->control)
                throw std::runtime_error("Ambiguous legacy probe control requires an explicit fresh-world reset");
            const bool ownedFrame = actor->stateObject && actor->stateObject->frameOverride ?
                actor->stateObject->frame.has_value() : actor->serializedStack.has_value();
            if (!ownedFrame) return {}; // Original native117/118 also require a StateFrame.
            const auto current = CurrentDispatchContext(actor);
            if (actor->stateObject && actor->stateObject->frame && actor->stateObject->frame->control) {
                const auto bit = std::uint64_t{1} << *QuestVr::ScriptDispatch::EventProbeIndex(event);
                const auto allowed = (current.classProbeMask | current.codeMasks->probeMask) & current.codeMasks->ignoreMask;
                if (index == 117u && !(allowed & bit)) return {};
                Touch(actor); actor->committedScriptState = true;
                auto& mask = actor->stateObject->frame->control->liveProbeMask;
                if (index == 118u) mask &= ~bit;
                else mask |= bit & allowed;
                ValidateStateBudget();
                return {};
            }
            if (index == 117u) {
                const auto bit = std::uint64_t{1} << *QuestVr::ScriptDispatch::EventProbeIndex(event);
                const auto allowed = (current.classProbeMask | (current.codeMasks ? current.codeMasks->probeMask : 0u)) &
                    (current.codeMasks ? current.codeMasks->ignoreMask : ~std::uint64_t{0});
                if (!(allowed & bit)) return {}; // Enable cannot add an ineligible original probe.
            }
            auto& state = EnsureStateObject(actor);
            const auto key = QuestVr::ScriptDispatch::FoldName(current.stateName);
            auto& disabled = state.disabled[key];
            if (index == 118u) {
                if (disabled.size() >= 4096u && !disabled.count(event)) throw std::runtime_error("Dynamic disabled-event budget exceeded");
                disabled.insert(event);
            } else disabled.erase(event);
            ValidateStateBudget();
            return {};
        }
        return {};
    }

    Evaluation PlayerPawnConsoleCommand(RuntimeObject* pawn,const std::vector<Evaluation>& arguments,
        const Function& declaration) {
        // Retail Engine.dll execConsoleCommand103ba200 initializes its return
        // FString before checking Player at103ba2c7. An unbound camera returns
        // empty without forwarding to Actor.ConsoleCommand or executing input.
        const auto authored=FunctionFor(Object("Engine.PlayerPawn.ConsoleCommand"));
        const auto validate=[&](const Function& function) {
            if(LowerAscii(function.path)!="engine.playerpawn.consolecommand" || function.nativeIndex!=0u ||
                function.flags!=0x400u || function.variables.size()!=2u)
                throw std::runtime_error("PlayerPawn ConsoleCommand is not its original native declaration");
            for(std::size_t slot=0u;slot<2u;++slot) {
                const auto& field=function.variables[slot];
                const auto key=std::string("engine.playerpawn.consolecommand.")+(slot?"returnvalue":"command");
                auto* metadata=Object(field.key);
                if(LowerAscii(field.key)!=key || field.zero.kind!=Kind::String || field.arrayDimension!=1u ||
                    field.flags!=(slot?0x400580u:0x400080u) || !metadata->property || metadata->property->type!="StrProperty")
                    throw std::runtime_error("PlayerPawn ConsoleCommand requires its authored FString parameters");
            }
        };
        validate(*authored);validate(declaration);
        if(declaration.source!=authored->source)
            throw std::runtime_error("PlayerPawn ConsoleCommand source does not match the owned declaration");
        RequireMapActor(pawn);
        if(!IsDerivedFromPath(pawn->cls,"Engine.PlayerPawn") || arguments.size()!=1u ||
            arguments.front().Load().kind!=Kind::String)
            throw std::runtime_error("PlayerPawn ConsoleCommand requires an actual PlayerPawn and one FString");
        const auto property=DescribeProperty("Engine.PlayerPawn.Player");
        auto* playerMetadata=Object(property.key);
        if(property.zero.kind!=Kind::Object || property.arrayDimension!=1u || !(property.flags&2u) ||
            !OwnsProperty(pawn->cls,property) || !playerMetadata->property ||
            playerMetadata->property->type!="ObjectProperty" ||
            LowerAscii(Qualified(Table(playerMetadata->sourcePath),playerMetadata->property->referencedType))!="engine.player")
            throw std::runtime_error("PlayerPawn ConsoleCommand has no original const Player field");
        // Do not let a revoked native lease turn a nonnull native Player into
        // the valid retail None branch merely because its public view is empty.
        if(pawn->nativePlayerBinding && pawn->nativePlayer &&
            (!persistentNativeClient.HasClient() ||
             pawn->nativePlayer->nativeClientIdentity!=persistentNativeClient.ClientIdentity()))
            throw std::runtime_error("PlayerPawn ConsoleCommand native Player lease is revoked or stale");
        const auto player=Read(pawn,property,0u);
        if(player.kind!=Kind::Object) throw std::runtime_error("PlayerPawn ConsoleCommand Player is not an Object");
        if(player.text.empty()) return {Value::Text(Kind::String,{}),{}};
        static_cast<void>(Object(player.text));
        // Real bound FExec includes KEYNAME/KEYBINDING/config commands used by
        // root initialization. No invented ping/key list or catch-all success.
        throw std::runtime_error("Bound PlayerPawn ConsoleCommand native FExec is not implemented");
    }
    std::shared_ptr<const Function> WindowNativeDeclaration(const std::uint16_t index,const Function* declaration) {
        struct Parameter { const char* name;Kind kind;std::uint32_t flags;const char* type;const char* constraint; };
        const char* path{};std::vector<Parameter> parameters;
        switch(index) {
        case 1414u:path="Extension.Window.Hide";break;
        case 1421u:path="Extension.Window.SetBackground";
            parameters={{"newBackground",Kind::Object,0x80u,"ObjectProperty","Engine.Texture"}};break;
        case 1422u:path="Extension.Window.SetBackgroundStyle";
            parameters={{"NewStyle",Kind::Byte,0x80u,"ByteProperty","Extension.ExtensionObject.EDrawStyle"}};break;
        case 1430u:path="Extension.Window.SetConfiguration";
            for(const auto* name:{"newX","newY","newWidth","NewHeight"})
                parameters.push_back({name,Kind::Float,0x80u,"FloatProperty",""});
            break;
        case 1431u:path="Extension.Window.SetSize";
            parameters={{"newWidth",Kind::Float,0x80u,"FloatProperty",""},
                {"NewHeight",Kind::Float,0x80u,"FloatProperty",""}};break;
        case 1425u:path="Extension.Window.GetRootWindow";
            parameters={{"ReturnValue",Kind::Object,0x580u,"ObjectProperty","Extension.RootWindow"}};break;
        case 1416u:path="Extension.Window.SetSensitivity";
            parameters={{"newSensitivity",Kind::Bool,0x80u,"BoolProperty",""}};break;
        case 1467u:path="Extension.Window.SetChildVisibility";
            parameters={{"bNewVisibility",Kind::Bool,0x80u,"BoolProperty",""}};break;
        case 1438u:path="Extension.Window.SetWindowAlignments";
            parameters={{"HAlign",Kind::Byte,0x80u,"ByteProperty","Extension.ExtensionObject.EHAlign"},
                {"VAlign",Kind::Byte,0x80u,"ByteProperty","Extension.ExtensionObject.EVAlign"}};
            for(const auto* name:{"hMargin0","vMargin0","hMargin1","vMargin1"})
                parameters.push_back({name,Kind::Float,0x90u,"FloatProperty",""});
            break;
        case 1477u:path="Extension.Window.SetFont";
            parameters={{"fn",Kind::Object,0x80u,"ObjectProperty","Engine.Font"}};break;
        case 1454u:path="Extension.Window.SetDefaultCursor";
            parameters={{"tX",Kind::Object,0x80u,"ObjectProperty","Engine.Texture"},
                {"shadowTexture",Kind::Object,0x90u,"ObjectProperty","Engine.Texture"},
                {"HotX",Kind::Float,0x90u,"FloatProperty",""},{"HotY",Kind::Float,0x90u,"FloatProperty",""},
                {"cursorColor",Kind::Struct,0x90u,"StructProperty","Core.Object.Color"}};break;
        case 1510u:path="Extension.RootWindow.SetDefaultEditCursor";
            parameters={{"newEditCursor",Kind::Object,0x90u,"ObjectProperty","Engine.Texture"}};break;
        case 1511u:path="Extension.RootWindow.SetDefaultMovementCursors";
            for(const auto* name:{"newMovementCursor","newHorizontalMovementCursor","newVerticalMovementCursor",
                "newTopLeftMovementCursor","newTopRightMovementCursor"}) {
                parameters.push_back({name,Kind::Object,0x90u,"ObjectProperty","Engine.Texture"});
            }
            break;
        default:throw std::runtime_error("Unregistered native window property declaration");
        }
        const auto authored=FunctionFor(Object(path));
        const auto validate=[&](const Function& function) {
            if(LowerAscii(function.path)!=LowerAscii(path) || function.nativeIndex!=index || function.flags!=0x401u ||
                function.variables.size()!=parameters.size())
                throw std::runtime_error("Native window function does not match its original declaration");
            for(std::size_t slot=0u;slot<parameters.size();++slot) {
                const auto& field=function.variables[slot];const auto& expected=parameters[slot];
                const auto key=LowerAscii(std::string(path)+'.'+expected.name);
                auto* metadata=Object(field.key);
                if(LowerAscii(field.key)!=key || field.zero.kind!=expected.kind || field.arrayDimension!=1u ||
                    field.flags!=expected.flags || !metadata->property || metadata->property->type!=expected.type ||
                    (*expected.constraint && LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!=
                        LowerAscii(expected.constraint)))
                    throw std::runtime_error("Native window parameter is not its authored scalar schema");
            }
        };
        validate(*authored);
        if(declaration) {
            validate(*declaration);
            if(declaration->source!=authored->source)
                throw std::runtime_error("Native window declaration source is not the owned function");
        }
        return authored;
    }
    Value CursorColorSeed() {
        auto color=DescribeProperty("Extension.Window.SetDefaultCursor.cursorColor").zero;
        if(color.kind!=Kind::Struct || color.fields.size()!=4u)
            throw std::runtime_error("Native cursor Color does not have four original Byte channels");
        for(auto& [name,member]:color.fields) {
            if(member.kind!=Kind::Byte || (name!="r" && name!="g" && name!="b" && name!="a"))
                throw std::runtime_error("Native cursor Color channel schema is invalid");
            // Retail initializes RGB white, leaves A undefined. Defined A=0
            // storage is a portable policy, not retail coverage evidence.
            member=Value::Byte(name=="a"?0u:255u);
        }
        return color;
    }
    std::optional<Value> NativeArgumentSeed(const std::uint16_t index,const std::string& receiver,
        const std::size_t slot,const Function* declaration) override {
        if(index!=1410u && index!=1416u && index!=1421u && index!=1422u && index!=1430u && index!=1431u &&
            index!=1454u && index!=1467u && index!=1477u && index!=1510u && index!=1511u)
            return std::nullopt;
        auto* window=Object(receiver);
        if(!window->nativeWindowObject || !IsDerivedFromPath(window->cls,"Extension.Window"))
            throw std::runtime_error("Native window argument seed requires an actual native Window");
        const auto authored=index==1410u?NewChildWindowDeclaration(declaration):WindowNativeDeclaration(index,declaration);
        if(slot>=authored->variables.size()) throw std::runtime_error("Native window argument seed slot exceeds its schema");
        if(index==1410u) return slot==1u?Value::Bool(true):authored->variables[slot].zero;
        if(index==1454u && (slot==2u || slot==3u)) return Value::Float(-1.0f);
        if(index==1454u && slot==4u) return CursorColorSeed();
        return authored->variables[slot].zero;
    }
    bool HasSequentialNativeArgumentSeeds(const std::uint16_t index,const std::string& receiver,
        const Function* declaration) override {
        if(index!=1438u) return false;
        RequireNativeWindow(Object(receiver));
        static_cast<void>(WindowNativeDeclaration(index,declaration));
        return true;
    }
    std::optional<Value> SequentialNativeArgumentSeed(const std::uint16_t index,const std::string& receiver,
        const std::size_t slot,const Function* declaration,const std::vector<Value>& prefix) override {
        if(index!=1438u) return std::nullopt;
        RequireNativeWindow(Object(receiver));
        const auto authored=WindowNativeDeclaration(index,declaration);
        if(slot>=authored->variables.size() || prefix.size()!=slot)
            throw std::runtime_error("Native window alignment seed has no ordered evaluated prefix");
        if(slot<4u) return authored->variables[slot].zero;
        auto seed=prefix[slot-2u]; // h1 from evaluated h0, v1 from evaluated v0.
        if(seed.kind==Kind::Nothing) seed=authored->variables[slot-2u].zero;
        if(seed.kind!=Kind::Float || !std::isfinite(seed.floating))
            throw std::runtime_error("Native window alignment seed requires its copied finite Float");
        return seed;
    }
    void RequireNativeWindow(RuntimeObject* window) {
        if(!window || !window->nativeWindowObject || !IsDerivedFromPath(window->cls,"Extension.Window"))
            throw std::runtime_error("Native Window operation requires an actual constructed Window");
    }
    Evaluation WindowAlignmentsNative(RuntimeObject* window,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        RequireNativeWindow(window);
        const auto authored=WindowNativeDeclaration(1438u,declaration);
        if(arguments.size()<2u || arguments.size()>authored->variables.size())
            throw std::runtime_error("SetWindowAlignments requires its two alignments and up to four margins");
        std::vector<Value> values;values.reserve(authored->variables.size());
        for(std::size_t slot=0u;slot<authored->variables.size();++slot) {
            auto value=slot<arguments.size()?arguments[slot].Load():Value{};
            if(value.kind==Kind::Nothing) value=slot>=4u?values[slot-2u]:authored->variables[slot].zero;
            if(value.kind!=authored->variables[slot].zero.kind ||
                (value.kind==Kind::Float && !std::isfinite(value.floating)))
                throw std::runtime_error("SetWindowAlignments requires its authored Byte/finite Float inputs");
            values.push_back(TypedLiveValue(authored->variables[slot],value));
        }
        // Retail compares H,V,h0,h1,v0,v1 with C3/equality. Portable finite
        // ingress refuses NaN rather than claiming its unordered x87 path.
        const std::array<const char*,6u> fields{"winHAlign","winVAlign","hMargin0","vMargin0","hMargin1","vMargin1"};
        bool changed{};
        for(const auto slot:{0u,1u,2u,4u,3u,5u}) {
            const auto old=Read(window,DescribeProperty("Extension.Window."+std::string(fields[slot])),0u);
            if(old.kind!=values[slot].kind || (old.kind==Kind::Float && !std::isfinite(old.floating)))
                throw std::runtime_error("Native window stored alignment has no finite authored layout");
            if(old.kind==Kind::Byte?old.integer!=values[slot].integer:old.floating!=values[slot].floating) {
                changed=true;break;
            }
        }
        if(!changed) return {}; // Equal signed zeros preserve old raw storage.
        // Actual native store order H,h0,V,v0,h1,v1; all incoming values store
        // even if only one differed. No enum clamp or sorted margin inference.
        for(const auto slot:{0u,2u,1u,3u,4u,5u})
            NativeWindowWrite(window,("Extension.Window."+std::string(fields[slot])).c_str(),values[slot],execution);
        AskHiddenWindowForReconfigure(window,execution);
        return {};
    }
    Evaluation WindowSizeNative(RuntimeObject* window,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        RequireNativeWindow(window);
        const auto authored=WindowNativeDeclaration(1431u,declaration);
        if(arguments.size()!=2u) throw std::runtime_error("SetSize requires its two Float dimensions");
        std::array<Value,2u> values;
        for(std::size_t slot=0u;slot<values.size();++slot) {
            auto value=arguments[slot].Load();
            if(value.kind==Kind::Nothing) value=authored->variables[slot].zero;
            if(value.kind!=Kind::Float || !std::isfinite(value.floating))
                throw std::runtime_error("SetSize requires copied finite Float dimensions");
            if(value.floating<0.0f) value=Value::Float(0.0f); // -0 preserves its bits.
            values[slot]=value;
        }
        const std::array<const char*,2u> fields{"hardcodedWidth","hardcodedHeight"};
        bool changed{};
        for(std::size_t slot=0u;slot<values.size();++slot) {
            const auto old=Read(window,DescribeProperty("Extension.Window."+std::string(fields[slot])),0u);
            if(old.kind!=Kind::Float || !std::isfinite(old.floating))
                throw std::runtime_error("Native requested window size has no finite Float layout");
            if(old.floating!=values[slot].floating) {changed=true;break;}
        }
        if(!changed) return {};
        // Retail Resize1004cf70 updates requested sizes, NOT current geometry
        // or clip extents. A hidden ancestor defers configuration callbacks.
        for(std::size_t slot=0u;slot<values.size();++slot)
            NativeWindowWrite(window,("Extension.Window."+std::string(fields[slot])).c_str(),values[slot],execution);
        AskHiddenWindowForReconfigure(window,execution);
        return {};
    }
    Evaluation WindowConfigurationNative(RuntimeObject* window,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        RequireNativeWindow(window);
        const auto authored=WindowNativeDeclaration(1430u,declaration);
        if(arguments.size()!=4u) throw std::runtime_error("SetConfiguration requires four Float inputs");
        std::array<Value,4u> values;
        for(std::size_t slot=0u;slot<values.size();++slot) {
            auto value=arguments[slot].Load();
            if(value.kind==Kind::Nothing) value=authored->variables[slot].zero;
            if(value.kind!=Kind::Float || !std::isfinite(value.floating))
                throw std::runtime_error("SetConfiguration requires copied finite Float inputs");
            if(slot>=2u && value.floating<0.0f) value=Value::Float(0.0f);
            values[slot]=value;
        }
        const std::array<const char*,4u> fields{"hMargin0","vMargin0","hardcodedWidth","hardcodedHeight"};
        bool changed{};
        for(const auto* name:{"winHAlign","winVAlign"}) {
            const auto alignment=Read(window,DescribeProperty("Extension.Window."+std::string(name)),0u);
            if(alignment.kind!=Kind::Byte) throw std::runtime_error("Native configuration alignment is not Byte");
            changed|=alignment.integer!=0;
        }
        for(std::size_t slot=0u;slot<values.size();++slot) {
            const auto old=Read(window,DescribeProperty("Extension.Window."+std::string(fields[slot])),0u);
            if(old.kind!=Kind::Float || !std::isfinite(old.floating))
                throw std::runtime_error("Native configuration has no finite Float storage");
            changed|=old.floating!=values[slot].floating;
        }
        if(!changed) return {};
        // Configure1004cd40 requests left/top alignment and hardcoded size.
        // Far margins and current geometry/clip remain untouched until layout.
        NativeWindowWrite(window,"Extension.Window.winHAlign",Value::Byte(0u),execution);
        NativeWindowWrite(window,"Extension.Window.winVAlign",Value::Byte(0u),execution);
        for(std::size_t slot=0u;slot<values.size();++slot)
            NativeWindowWrite(window,("Extension.Window."+std::string(fields[slot])).c_str(),values[slot],execution);
        AskHiddenWindowForReconfigure(window,execution);
        return {};
    }
    Evaluation WindowBackgroundNative(const std::uint16_t index,RuntimeObject* window,
        const std::vector<Evaluation>& arguments,const Function* declaration,QuestVr::Vm::Execution& execution) {
        RequireNativeWindow(window);
        const auto authored=WindowNativeDeclaration(index,declaration);
        if(arguments.size()!=1u) throw std::runtime_error("Native Window background setter requires one input");
        auto value=arguments[0u].Load();
        if(value.kind==Kind::Nothing) value=authored->variables[0u].zero;
        if(value.kind!=authored->variables[0u].zero.kind)
            throw std::runtime_error("Native Window background input has the wrong declared type");
        value=TypedLiveValue(authored->variables[0u],value);
        if(index==1421u) {
            const auto background=Read(window,DescribeProperty("Extension.Window.Background"),0u);
            if(background.kind!=Kind::Object) throw std::runtime_error("Native Window background is not Object");
            auto* oldTexture=background.text.empty()?nullptr:Object(background.text);
            auto* newTexture=value.text.empty()?nullptr:Object(value.text);
            if(oldTexture==newTexture) return {}; // UObject pointer equality, not handle spelling.
            NativeWindowWrite(window,"Extension.Window.Background",value,execution);
            AskHiddenWindowForReconfigure(window,execution);
            return {};
        }
        // SetBackgroundStyle1004d670 always writes, even if unchanged. Only the
        // window GC receives XGC::SetStyle; the general-purpose GC is untouched.
        NativeWindowWrite(window,"Extension.Window.backgroundStyle",value,execution);
        const auto context=Read(window,DescribeProperty("Extension.Window.wGc"),0u);
        if(context.kind!=Kind::Object) throw std::runtime_error("Native Window wGc is not Object");
        if(context.text.empty()) return {};
        auto* gc=Object(context.text);
        if(!gc->nativeWindowObject || !IsDerivedFromPath(gc->cls,"Extension.GC"))
            throw std::runtime_error("Native Window background style has no actual GC");
        NativeWindowWrite(gc,"Extension.GC.Style",value,execution);
        NativeWindowWrite(gc,"Extension.GC.bDrawEnabled",Value::Bool(value.integer!=0),execution);
        if(value.integer==0) return {}; // Preserve existing modes/PolyFlags.
        NativeWindowWrite(gc,"Extension.GC.bMasked",Value::Bool(value.integer==2),execution);
        NativeWindowWrite(gc,"Extension.GC.bTranslucent",Value::Bool(value.integer==3),execution);
        NativeWindowWrite(gc,"Extension.GC.bModulated",Value::Bool(value.integer==4),execution);
        const auto smoothed=Read(gc,DescribeProperty("Extension.GC.bSmoothed"),0u);
        if(smoothed.kind!=Kind::Bool) throw std::runtime_error("Native GC smoothing has no Bool layout");
        const auto flags=0x100|(value.integer==2?2:0)|(value.integer==3?4:0)|
            (value.integer==4?0x40:0)|(smoothed.boolean?0:0x800);
        NativeWindowWrite(gc,"Extension.GC.PolyFlags",Value::Integer(flags),execution);
        return {};
    }
    RuntimeObject* NativeWindowTerminal(RuntimeObject* window) {
        RequireNativeWindow(window);
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        for(auto* current=window;;) {
            LifecycleStep();RequireNativeWindow(current);
            if(depth==visited.size() || std::find(visited.begin(),visited.begin()+depth,current)!=visited.begin()+depth)
                throw std::runtime_error("Native window root ancestry cycles or exceeds its bound");
            visited[depth++]=current;
            auto* parent=WindowLink(current,"winParent");
            if(!parent) return current;
            current=parent;
        }
    }
    RuntimeObject* NativeWindowRoot(RuntimeObject* window) {
        auto* terminal=NativeWindowTerminal(window);
        const auto type=Read(terminal,DescribeProperty("Extension.Window.windowType"),0u);
        if(type.kind!=Kind::Byte) throw std::runtime_error("Native window terminal type has no Byte layout");
        if(type.integer<3) return nullptr;
        // Retail returns a raw cast for unsigned type>=3, not the first root
        // ancestor. Refuse an unsafe fabricated cast, never replace it by None.
        if(!IsDerivedFromPath(terminal->cls,"Extension.RootWindow"))
            throw std::runtime_error("Native terminal Window type cannot safely represent RootWindow");
        return terminal;
    }
    Evaluation WindowRootGetterNative(RuntimeObject* window,const std::vector<Evaluation>& arguments,
        const Function* declaration) {
        static_cast<void>(WindowNativeDeclaration(1425u,declaration));
        if(!arguments.empty()) throw std::runtime_error("GetRootWindow takes no arguments");
        auto* root=NativeWindowRoot(window);
        return {Value::Text(Kind::Object,root?root->reflection.objectPath:std::string{}),{}};
    }
    RuntimeObject* NativeWindowModal(RuntimeObject* window) {
        RequireNativeWindow(window);
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        for(auto* current=window;current;current=WindowLink(current,"winParent")) {
            LifecycleStep();RequireNativeWindow(current);
            if(depth==visited.size() || std::find(visited.begin(),visited.begin()+depth,current)!=visited.begin()+depth)
                throw std::runtime_error("Native window modal ancestry cycles or exceeds its bound");
            visited[depth++]=current;
            const auto type=Read(current,DescribeProperty("Extension.Window.windowType"),0u);
            if(type.kind!=Kind::Byte) throw std::runtime_error("Native window modal type has no Byte layout");
            if(type.integer>=2) {
                if(!IsDerivedFromPath(current->cls,"Extension.ModalWindow"))
                    throw std::runtime_error("Native Window type cannot safely represent ModalWindow");
                return current;
            }
        }
        return nullptr;
    }
    bool WindowEffectiveSensitivity(RuntimeObject* window) {
        RequireNativeWindow(window);
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        for(auto* current=window;current;current=WindowLink(current,"winParent")) {
            LifecycleStep();RequireNativeWindow(current);
            if(depth==visited.size() || std::find(visited.begin(),visited.begin()+depth,current)!=visited.begin()+depth)
                throw std::runtime_error("Native sensitivity ancestry cycles or exceeds its bound");
            visited[depth++]=current;
            const auto local=Read(current,DescribeProperty("Extension.Window.bIsSensitive"),0u);
            if(local.kind!=Kind::Bool) throw std::runtime_error("Native sensitivity has no Bool layout");
            if(!local.boolean) return false;
        }
        return true;
    }
    std::shared_ptr<const Function> OptionalNativeWindowEvent(RuntimeObject* receiver,const char* name) {
        RequireNativeWindow(receiver);
        const auto native=NativeWindowAncestor(receiver->cls);
        if(native!="extension.window" && native!="extension.tabgroupwindow" &&
            native!="extension.modalwindow" && native!="extension.rootwindow")
            throw std::runtime_error("Native Window event override is not implemented: "+native);
        const auto context=CurrentDispatchContext(receiver);
        const auto selected=context.exactSelected?
            QuestVr::ScriptDispatch::ResolveFunctionAtSelectedNode(DispatchGraph(),receiver->cls->reflection.objectPath,
                context.selectedPath,name):QuestVr::ScriptDispatch::ResolveFunction(DispatchGraph(),
                receiver->cls->reflection.objectPath,context.stateName,name);
        return selected?FunctionIdentity(Object(*selected)):nullptr;
    }
    void InvokeSensitivityTree(RuntimeObject* window,const bool sensitive,QuestVr::Vm::Execution& execution) {
        // Retail10050ef0: Self event, then locally sensitive children. Read the
        // live firstChild after Self's callback and nextSibling after recursion.
        std::vector<RuntimeObject*> stack{window};std::unordered_set<RuntimeObject*> entered;
        while(!stack.empty()) {
            LifecycleStep();auto* current=stack.back();
            if(!entered.count(current)) {
                if(entered.size()>=4096u || stack.size()>128u)
                    throw std::runtime_error("Native sensitivity callback tree exceeds its bound");
                entered.insert(current);
                if(const auto callback=OptionalNativeWindowEvent(current,"SensitivityChanged"))
                    static_cast<void>(execution.CallFunction(*callback,current->reflection.objectPath,{{Value::Bool(sensitive),{}}}));
                if(Read(current,DescribeProperty("Extension.Window.bBeingDestroyed"),0u).boolean)
                    throw std::runtime_error("Native sensitivity callback destruction is not implemented");
                auto* child=WindowLink(current,"firstChild");
                while(child && !Read(child,DescribeProperty("Extension.Window.bIsSensitive"),0u).boolean) {
                    LifecycleStep();
                    if(!entered.insert(child).second || entered.size()>4096u)
                        throw std::runtime_error("Native sensitivity sibling traversal cycles or exceeds its bound");
                    child=WindowLink(child,"nextSibling");
                }
                if(child) {
                    if(entered.count(child)) throw std::runtime_error("Native sensitivity child tree cycles");
                    stack.push_back(child);continue;
                }
            }
            stack.pop_back();
            if(stack.empty()) break;
            auto* sibling=WindowLink(current,"nextSibling");
            while(sibling && !Read(sibling,DescribeProperty("Extension.Window.bIsSensitive"),0u).boolean) {
                LifecycleStep();
                if(!entered.insert(sibling).second || entered.size()>4096u)
                    throw std::runtime_error("Native sensitivity sibling traversal cycles or exceeds its bound");
                sibling=WindowLink(sibling,"nextSibling");
            }
            if(sibling) {
                if(entered.count(sibling)) throw std::runtime_error("Native sensitivity child tree cycles");
                stack.push_back(sibling);
            }
        }
    }
    Evaluation WindowSensitivityNative(RuntimeObject* window,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        RequireNativeWindow(window);
        static_cast<void>(WindowNativeDeclaration(1416u,declaration));
        if(arguments.size()!=1u) throw std::runtime_error("SetSensitivity requires its single Bool");
        auto value=arguments.front().Load();
        if(value.kind==Kind::Nothing) value=Value::Bool(false);
        if(value.kind!=Kind::Bool) throw std::runtime_error("SetSensitivity requires its authored Bool input");
        const auto old=Read(window,DescribeProperty("Extension.Window.bIsSensitive"),0u);
        if(old.kind!=Kind::Bool) throw std::runtime_error("Native stored sensitivity has no Bool layout");
        if(old.boolean==value.boolean) return {};
        const auto before=WindowEffectiveSensitivity(window);
        NativeWindowWrite(window,"Extension.Window.bIsSensitive",value,execution);
        const auto after=WindowEffectiveSensitivity(window);
        if(before==after) return {};
        // Hidden visibility does NOT suppress this effective-change path.
        // CheckFocus/CheckGrabbed return immediately on their actual null root
        // pointers. Nonnull native focus/grab negotiation remains unsupported.
        if(auto* root=NativeWindowRoot(window)) {
            for(const auto* name:{"FocusWindow","grabbedWindow"}) {
                const auto pointer=Read(root,DescribeProperty("Extension.RootWindow."+std::string(name)),0u);
                if(pointer.kind!=Kind::Object) throw std::runtime_error("Native root focus/grab pointer is not Object");
                if(!pointer.text.empty())
                    throw std::runtime_error("Native sensitivity focus/grab negotiation is not implemented");
            }
        }
        if(auto* modal=NativeWindowModal(window))
            NativeWindowWrite(modal,"Extension.ModalWindow.bDirtyAccelerators",Value::Bool(true),execution);
        InvokeSensitivityTree(window,after,execution);
        return {};
    }
    void AskHiddenWindowForReconfigure(RuntimeObject* window,QuestVr::Vm::Execution& execution) {
        // Retail1004e119 always marks SELF. Any hidden node in the winParent
        // chain defers layout/callbacks; this is the root InitWindow branch.
        NativeWindowWrite(window,"Extension.Window.bNeedsReconfigure",Value::Bool(true),execution);
        NativeWindowWrite(window,"Extension.Window.bNeedsQuery",Value::Bool(true),execution);
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        for(auto* current=window;current;) {
            LifecycleStep();
            if(depth==visited.size() || std::find(visited.begin(),visited.begin()+depth,current)!=visited.begin()+depth)
                throw std::runtime_error("Native window ancestry cycles or exceeds its reconfiguration bound");
            visited[depth++]=current;
            if(!current->nativeWindowObject || !IsDerivedFromPath(current->cls,"Extension.Window"))
                throw std::runtime_error("Native reconfiguration ancestor is not an actual Window");
            const auto visible=Read(current,DescribeProperty("Extension.Window.bIsVisible"),0u);
            if(visible.kind!=Kind::Bool) throw std::runtime_error("Native window visibility is not Bool");
            if(!visible.boolean) return;
            const auto parent=Read(current,DescribeProperty("Extension.Window.winParent"),0u);
            if(parent.kind!=Kind::Object) throw std::runtime_error("Native window parent is not an Object");
            current=parent.text.empty()?nullptr:Object(parent.text);
        }
        throw std::runtime_error("Visible native Window reconfiguration is not implemented");
    }
    Evaluation WindowPropertyNative(const std::uint16_t index,RuntimeObject* window,
        const std::vector<Evaluation>& arguments,const Function* declaration,QuestVr::Vm::Execution& execution) {
        const auto authored=WindowNativeDeclaration(index,declaration);
        if(!window->nativeWindowObject || !IsDerivedFromPath(window->cls,"Extension.Window") ||
            ((index==1510u || index==1511u) && !IsDerivedFromPath(window->cls,"Extension.RootWindow")))
            throw std::runtime_error("Native window property receiver has no original constructed identity");
        const auto required=index==1477u || index==1454u?1u:0u;
        if(arguments.size()<required || arguments.size()>authored->variables.size())
            throw std::runtime_error("Native window property argument count exceeds its declaration");
        std::vector<Value> values;values.reserve(authored->variables.size());
        for(std::size_t slot=0u;slot<authored->variables.size();++slot) {
            auto value=slot<arguments.size()?arguments[slot].Load():Value{};
            if(value.kind==Kind::Nothing) value=*NativeArgumentSeed(index,window->reflection.objectPath,slot,declaration);
            if(value.kind!=authored->variables[slot].zero.kind)
                throw std::runtime_error("Native window property argument has the wrong result type");
            values.push_back(TypedLiveValue(authored->variables[slot],value));
        }
        if(index==1477u) {
            NativeWindowWrite(window,"Extension.Window.normalFont",values[0u],execution);
            NativeWindowWrite(window,"Extension.Window.boldFont",values[0u],execution);
            const auto context=Read(window,DescribeProperty("Extension.Window.wGc"),0u);
            if(context.kind!=Kind::Object) throw std::runtime_error("Native Window wGc has no Object layout");
            if(!context.text.empty()) {
                auto* gc=Object(context.text);
                if(!gc->nativeWindowObject || !IsDerivedFromPath(gc->cls,"Extension.GC"))
                    throw std::runtime_error("Native Window wGc is not its owned original GC instance");
                NativeWindowWrite(gc,"Extension.GC.normalFont",values[0u],execution);
                NativeWindowWrite(gc,"Extension.GC.boldFont",values[0u],execution);
            }
            AskHiddenWindowForReconfigure(window,execution);
        } else if(index==1454u) {
            const std::array<const char*,5u> fields{"defaultCursor","defaultCursorShadow","defaultHotX","defaultHotY","defaultCursorColor"};
            for(std::size_t slot=0u;slot<fields.size();++slot)
                NativeWindowWrite(window,(std::string("Extension.Window.")+fields[slot]).c_str(),values[slot],execution);
        } else if(index==1510u) {
            NativeWindowWrite(window,"Extension.RootWindow.defaultEditCursor",values[0u],execution);
        } else {
            const std::array<const char*,5u> fields{"DefaultMoveCursor","defaultHorizontalMoveCursor","defaultVerticalMoveCursor",
                "defaultTopLeftMoveCursor","defaultTopRightMoveCursor"};
            for(std::size_t slot=0u;slot<fields.size();++slot)
                NativeWindowWrite(window,(std::string("Extension.RootWindow.")+fields[slot]).c_str(),values[slot],execution);
        }
        return {};
    }
    RuntimeObject* ConstructWindowObject(RuntimeObject* cls,RuntimeObject* outer,
        QuestVr::Vm::Execution& execution) {
        if(!transaction_ || !cls || !cls->classDescriptor || !outer || !persistentWindowDomain ||
            !persistentWindowDomain->get() || IsDerivedFromPath(cls,"Engine.Actor") ||
            !(IsDerivedFromPath(cls,"Extension.Window") || IsDerivedFromPath(cls,"Extension.GC")))
            throw std::runtime_error("Native window construction requires an actual non-Actor Window/GC class and Outer");
        if((cls->classDescriptor->classFlags&1u)!=0u)
            throw std::runtime_error("Native window construction class is abstract");
        auto* domain=persistentWindowDomain->get();
        if(domain->objects.size()>=4096u) throw std::runtime_error("Native window object count exceeds its bound");
        // Validate original header declarations before touching the name counter
        // or allocating/publishing any native identity.
        for(const auto& [key,kind]:{
            std::pair{"Core.Object.Class",Kind::Object},std::pair{"Core.Object.Name",Kind::Name},
            std::pair{"Core.Object.Outer",Kind::Object},std::pair{"Core.Object.ObjectFlags",Kind::Int}}) {
            const auto field=DescribeProperty(key);
            if(field.zero.kind!=kind || field.arrayDimension!=1u || !OwnsProperty(cls,field))
                throw std::runtime_error("Native window UObject header schema is malformed");
        }
        Touch(cls);
        const auto className=CastClassIdentity(cls->reflection.objectPath).name;
        const auto name=QuestVr::ObjectNames::MakeUnique(className,cls->uniqueObjectCounter,
            [&](const std::string_view candidate) {
                LifecycleStep();
                return persistentVmObjects.count(LowerAscii(outer->reflection.objectPath+'.'+std::string(candidate)))!=0u;
            });
        const auto path=outer->reflection.objectPath+'.'+name;
        // Charge the copied CDO block before allocation, not only after a
        // potentially large window/default graph has already been duplicated.
        ValidateStateBudget(sizeof(RuntimeObject)+path.size()+sizeof(GCAllocation),cls);
        pendingBirthGarbage=true;
        provisionalWindows_.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{
            path,className,outer->reflection.objectPath,{},0u,0},nullptr));
        auto* object=provisionalWindows_.back().get();
        object->nativeWindowObject=true;object->cls=cls;object->outer=outer;
        object->sourcePath=cls->sourcePath;object->exportIndex=std::numeric_limits<std::size_t>::max();
        object->references={cls,outer};object->frozenBirthDefaults=cls->classDefaultValues;
        // Register in rollback-owned domain before either index can throw.
        domain->objects.push_back(object);
        if(!persistentQualifiedObjects.emplace(path,object).second ||
            !persistentVmObjects.emplace(LowerAscii(path),object).second)
            throw std::runtime_error("Native window identity collides with an existing UObject");
        NativeWindowWrite(object,"Core.Object.Class",Value::Text(Kind::Object,cls->reflection.objectPath),execution);
        NativeWindowWrite(object,"Core.Object.Name",Value::Text(Kind::Name,name),execution);
        NativeWindowWrite(object,"Core.Object.Outer",Value::Text(Kind::Object,outer->reflection.objectPath),execution);
        // Core.StaticAllocateObject10157e9e applies CLASS_Transient independently
        // of the caller's requested0 flags (e.g. original DamageHUDDisplay).
        const auto flags=(cls->classDescriptor->classFlags&8u)?0x4000u:0u;
        object->reflection.flags=flags;
        NativeWindowWrite(object,"Core.Object.ObjectFlags",Value::Integer(static_cast<std::int32_t>(flags)),execution);
        return object;
    }
    std::shared_ptr<const Function> NewChildWindowDeclaration(const Function* declaration) {
        const auto authored=FunctionFor(Object("Extension.Window.NewChild"));
        const auto validate=[&](const Function& function) {
            if(LowerAscii(function.path)!="extension.window.newchild" || function.nativeIndex!=1410u ||
                function.flags!=0x401u || function.variables.size()!=3u)
                throw std::runtime_error("NewChild does not match its original native declaration");
            const std::array<const char*,3u> names{"newclass","bshow","returnvalue"};
            const std::array<Kind,3u> kinds{Kind::Object,Kind::Bool,Kind::Object};
            const std::array<std::uint32_t,3u> flags{0x80u,0x90u,0x580u};
            const std::array<const char*,3u> types{"ClassProperty","BoolProperty","ObjectProperty"};
            for(std::size_t slot=0u;slot<names.size();++slot) {
                const auto& field=function.variables[slot];auto* metadata=Object(field.key);
                if(LowerAscii(field.key)!="extension.window.newchild."+std::string(names[slot]) ||
                    field.zero.kind!=kinds[slot] || field.arrayDimension!=1u || field.flags!=flags[slot] ||
                    !metadata->property || metadata->property->type!=types[slot])
                    throw std::runtime_error("NewChild parameter lacks its exact authored schema");
                if(slot!=1u) {
                    const auto& table=Table(metadata->sourcePath);
                    if(LowerAscii(Qualified(table,metadata->property->referencedType))!=(slot?"extension.window":"core.class") ||
                        (!slot && LowerAscii(Qualified(table,metadata->property->secondaryType))!="core.object"))
                        throw std::runtime_error("NewChild parameter lacks its original Object-class/Window-result constraint");
                }
            }
        };
        validate(*authored);
        if(declaration) {
            validate(*declaration);
            if(declaration->source!=authored->source) throw std::runtime_error("NewChild declaration is not its owned source");
        }
        return authored;
    }
    std::string NativeWindowAncestor(RuntimeObject* cls) {
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        for(auto* current=cls;current;current=current->base) {
            LifecycleStep();
            if(depth==visited.size() || std::find(visited.begin(),visited.begin()+depth,current)!=visited.begin()+depth ||
                !current->classDescriptor)
                throw std::runtime_error("Native Window class ancestry is malformed, cyclic or excessive");
            visited[depth++]=current;
            // Core RF_Native belongs to actual UClass metadata. CLASS bit0x80
            // is RuntimeStatic, not a native constructor registration.
            if(current->reflection.flags&0x04000000u) return LowerAscii(current->reflection.objectPath);
        }
        throw std::runtime_error("Window class has no actual native ancestor");
    }
    Evaluation NewChildWindowNative(RuntimeObject* parent,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        const auto authored=NewChildWindowDeclaration(declaration);
        if(!parent->nativeWindowObject || !IsDerivedFromPath(parent->cls,"Extension.Window") ||
            arguments.empty() || arguments.size()>2u)
            throw std::runtime_error("NewChild requires an actual constructed Window and original arguments");
        auto selected=arguments[0u].Load();
        if(selected.kind==Kind::Nothing) selected=authored->variables[0u].zero;
        selected=TypedLiveValue(authored->variables[0u],selected);
        auto show=arguments.size()>1u?arguments[1u].Load():Value::Bool(true);
        if(show.kind==Kind::Nothing) show=Value::Bool(true);
        if(show.kind!=Kind::Bool) throw std::runtime_error("NewChild bShow is not a Bool");
        if(selected.text.empty()) return {Value::Text(Kind::Object,{}),{}};
        auto* cls=Object(selected.text);
        const auto native=NativeWindowAncestor(cls);
        // Each native subclass has distinct Init/callback/layout semantics.
        // The pure Window constructor is the first ActorDisplayWindow branch.
        if(native!="extension.window")
            throw std::runtime_error("NewChild native subclass Init is not implemented: "+native);
        auto* child=ConstructWindowObject(cls,parent,execution);
        InitializeBaseWindow(child,parent,execution);
        NativeWindowWrite(child,"Extension.Window.lockCount",Value::Integer(1),execution);
        const auto init=Member(child,"InitWindow");
        static_cast<void>(execution.CallFunction(*init,child->reflection.objectPath,{}));
        if(show.boolean) SetHiddenChildVisibility(child,true,execution);
        AskHiddenWindowForReconfigure(child,execution);
        const auto destroyed=Read(child,DescribeProperty("Extension.Window.bBeingDestroyed"),0u);
        const auto lock=Read(child,DescribeProperty("Extension.Window.lockCount"),0u);
        if(destroyed.kind!=Kind::Bool || lock.kind!=Kind::Int || lock.integer!=1)
            throw std::runtime_error("NewChild native lifetime state changed outside its supported locked callback");
        if(destroyed.boolean)
            throw std::runtime_error("NewChild deferred destruction/unlock cleanup is not implemented");
        NativeWindowWrite(child,"Extension.Window.lockCount",Value::Integer(0),execution);
        return {Value::Text(Kind::Object,child->reflection.objectPath),{}};
    }
    bool WindowEffectiveVisibility(RuntimeObject* window) {
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        for(auto* current=window;current;current=WindowLink(current,"winParent")) {
            LifecycleStep();
            if(depth==visited.size() || std::find(visited.begin(),visited.begin()+depth,current)!=visited.begin()+depth)
                throw std::runtime_error("Native visibility ancestry cycles or exceeds its bound");
            visited[depth++]=current;
            const auto local=Read(current,DescribeProperty("Extension.Window.bIsVisible"),0u);
            if(local.kind!=Kind::Bool) throw std::runtime_error("Native visibility has no Bool storage");
            if(!local.boolean) return false;
        }
        return true;
    }
    Evaluation WindowChildVisibilityNative(RuntimeObject* window,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        RequireNativeWindow(window);
        const auto authored=WindowNativeDeclaration(1467u,declaration);
        if(arguments.size()!=1u) throw std::runtime_error("SetChildVisibility requires its one Bool input");
        auto visible=arguments[0u].Load();
        if(visible.kind==Kind::Nothing) visible=authored->variables[0u].zero;
        if(visible.kind!=Kind::Bool) throw std::runtime_error("SetChildVisibility input is not Bool");
        // Native1467 acts on Self directly: no invented parent relationship
        // check or ChildRequestedVisibilityChange dispatch belongs here.
        SetHiddenChildVisibilityDirect(window,visible.boolean,execution);
        return {};
    }
    void SetHiddenChildVisibilityDirect(RuntimeObject* window,const bool visible,QuestVr::Vm::Execution& execution) {
        RequireNativeWindow(window);
        const auto local=Read(window,DescribeProperty("Extension.Window.bIsVisible"),0u);
        if(local.kind!=Kind::Bool) throw std::runtime_error("Native child visibility is not Bool");
        if(local.boolean==visible) return;
        const auto before=WindowEffectiveVisibility(window);
        NativeWindowWrite(window,"Extension.Window.bIsVisible",Value::Bool(visible),execution);
        if(before!=WindowEffectiveVisibility(window))
            throw std::runtime_error("Effective native Window visibility/init/focus/layout is not implemented");
        // SetChildVisibility1004ee20 returns immediately when effective state
        // is unchanged. Reconfiguration on a TabGroup hide is its caller's job.
    }
    void SetHiddenChildVisibility(RuntimeObject* window,const bool visible,QuestVr::Vm::Execution& execution) {
        RequireNativeWindow(window);
        const auto local=Read(window,DescribeProperty("Extension.Window.bIsVisible"),0u);
        if(local.kind!=Kind::Bool) throw std::runtime_error("Native child visibility is not Bool");
        if(local.boolean==visible) return;
        auto* parent=WindowLink(window,"winParent");
        if(!parent) {SetHiddenChildVisibilityDirect(window,visible,execution);return;}
        const auto native=NativeWindowAncestor(parent->cls);
        if(native=="extension.window") {
            // XWindow vtable+84 dispatches the optional most-derived script
            // callback. DeusExHUD's override also runs ConfigurationChanged,
            // even under a hidden root and before NewChild's outer assignment.
            if(const auto callback=OptionalNativeWindowEvent(parent,"ChildRequestedVisibilityChange"))
                static_cast<void>(execution.CallFunction(*callback,parent->reflection.objectPath,
                    {{Value::Text(Kind::Object,window->reflection.objectPath),{}},{Value::Bool(visible),{}}}));
            return; // A missing/overridden callback need not change the child.
        }
        if(native!="extension.tabgroupwindow" && native!="extension.modalwindow" && native!="extension.rootwindow")
            throw std::runtime_error("Native child visibility parent virtual dispatch is not implemented: "+native);
        // TabGroup10045870 (also inherited by Modal/Root) bypasses script.
        SetHiddenChildVisibilityDirect(window,visible,execution);
        if(!visible) AskHiddenWindowForReconfigure(parent,execution);
    }
    void NativeWindowWrite(RuntimeObject* object,const char* key,const Value& value,
        QuestVr::Vm::Execution& execution) {
        const auto property=DescribeProperty(key);
        if(property.arrayDimension!=1u || !OwnsProperty(object->cls,property))
            throw std::runtime_error("Native window field is not its original scalar declaration: "+std::string(key));
        execution.WriteReference(ReferenceFor(object,property,0u,false),value);
    }
    RuntimeObject* WindowLink(RuntimeObject* window,const char* name) {
        const auto key="Extension.Window."+std::string(name);
        const auto property=DescribeProperty(key);
        if(!window->nativeWindowObject || !IsDerivedFromPath(window->cls,"Extension.Window") ||
            property.zero.kind!=Kind::Object || property.arrayDimension!=1u || !OwnsProperty(window->cls,property))
            throw std::runtime_error("Native window link has no original Object declaration");
        const auto value=Read(window,property,0u);
        return value.text.empty()?nullptr:Object(value.text);
    }
    void WindowLinkWrite(RuntimeObject* window,const char* name,RuntimeObject* target,
        QuestVr::Vm::Execution& execution) {
        const auto key="Extension.Window."+std::string(name);
        NativeWindowWrite(window,key.c_str(),Value::Text(Kind::Object,target?target->reflection.objectPath:std::string{}),execution);
    }
    void LinkNewWindowChild(RuntimeObject* parent,RuntimeObject* child,QuestVr::Vm::Execution& execution) {
        // AddChild10050a80(topmost=true): append to lastChild, then notify.
        // A newly constructed child has no previous traversal parent to detach.
        if(WindowLink(child,"winParent")) throw std::runtime_error("New native child already has a traversal parent");
        auto* last=WindowLink(parent,"lastChild");
        if((last==nullptr)!=(WindowLink(parent,"firstChild")==nullptr))
            throw std::runtime_error("Native parent child endpoints disagree");
        WindowLinkWrite(child,"nextSibling",nullptr,execution);
        WindowLinkWrite(child,"prevSibling",last,execution);
        if(last) {
            if(WindowLink(last,"winParent")!=parent || WindowLink(last,"nextSibling"))
                throw std::runtime_error("Native parent last child is not its terminal sibling");
            WindowLinkWrite(last,"nextSibling",child,execution);
        } else WindowLinkWrite(parent,"firstChild",child,execution);
        WindowLinkWrite(parent,"lastChild",child,execution);
        WindowLinkWrite(child,"winParent",parent,execution);
        const auto notify=[&](RuntimeObject* receiver,const char* name) {
            const auto native=NativeWindowAncestor(receiver->cls);
            if(native!="extension.window" && native!="extension.tabgroupwindow" &&
                native!="extension.modalwindow" && native!="extension.rootwindow")
                throw std::runtime_error("Native child notification override is not implemented: "+native);
            // Retail ProcessScript(...,checked=false) does optional lookup on
            // dynamic Self. It does not require a newly fabricated base event.
            const auto context=CurrentDispatchContext(receiver);
            const auto selected=context.exactSelected?
                QuestVr::ScriptDispatch::ResolveFunctionAtSelectedNode(DispatchGraph(),receiver->cls->reflection.objectPath,
                    context.selectedPath,name):QuestVr::ScriptDispatch::ResolveFunction(DispatchGraph(),
                    receiver->cls->reflection.objectPath,context.stateName,name);
            if(selected) {
                const auto callback=FunctionIdentity(Object(*selected));
                static_cast<void>(execution.CallFunction(*callback,receiver->reflection.objectPath,
                    {{Value::Text(Kind::Object,child->reflection.objectPath),{}}}));
            }
            if(Read(child,DescribeProperty("Extension.Window.bBeingDestroyed"),0u).boolean)
                throw std::runtime_error("NewChild destruction during unlocked parent notification is unsupported");
        };
        notify(parent,"ChildAdded");
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        for(auto* current=parent;current;current=WindowLink(current,"winParent")) {
            LifecycleStep();
            if(depth==visited.size() || std::find(visited.begin(),visited.begin()+depth,current)!=visited.begin()+depth)
                throw std::runtime_error("Native descendant notification ancestry cycles or exceeds its bound");
            visited[depth++]=current;notify(current,"DescendantAdded");
        }
    }
    void NativeWindowClip(RuntimeObject* window,QuestVr::Vm::Execution& execution) {
        const auto property=DescribeProperty("Extension.Window.winClipRect");
        auto clip=Read(window,property,0u);
        const std::array<const char*,6u> channels{"originx","originy","clipx","clipy","clipwidth","clipheight"};
        if(clip.kind!=Kind::Struct || clip.fields.size()!=channels.size())
            throw std::runtime_error("Native window ClipRect lacks its exact six-Float schema");
        for(const auto* channel:channels) {
            const auto member=clip.fields.find(channel);
            if(member==clip.fields.end() || member->second.kind!=Kind::Float)
                throw std::runtime_error("Native window ClipRect channel is not its original Float");
            member->second=Value::Float(0.0f);
        }
        if(auto* parent=WindowLink(window,"winParent")) {
            const auto inherited=Read(parent,property,0u);
            if(inherited.kind!=Kind::Struct || inherited.fields.size()!=channels.size())
                throw std::runtime_error("Native window parent ClipRect is malformed");
            for(const auto* channel:{"originx","originy"}) {
                const auto member=inherited.fields.find(channel);
                if(member==inherited.fields.end() || member->second.kind!=Kind::Float)
                    throw std::runtime_error("Native window parent clip origin is malformed");
                clip.fields.at(channel)=member->second;
            }
        }
        clip.fields.at("clipwidth")=Read(window,DescribeProperty("Extension.Window.Width"),0u);
        clip.fields.at("clipheight")=Read(window,DescribeProperty("Extension.Window.Height"),0u);
        NativeWindowWrite(window,"Extension.Window.winClipRect",clip,execution);
    }
    void InitializeBaseWindow(RuntimeObject* window,RuntimeObject* parent,QuestVr::Vm::Execution& execution) {
        // XWindow.Init1004bb30: no script InitWindow here. Derived native Init
        // and CreateNewWindow's later locking/script callback remain separate.
        const auto set=[&](const char* name,const Value& value) {
            const auto key="Extension.Window."+std::string(name);NativeWindowWrite(window,key.c_str(),value,execution);
        };
        set("windowType",Value::Byte(0u));WindowLinkWrite(window,"parentOwner",parent,execution);
        for(const auto* name:{"bIsVisible","bIsSelectable","bBeingDestroyed","bStretchBackground","bSmoothBackground",
            "bConfigured","bTickEnabled"}) set(name,Value::Bool(false));
        for(const auto* name:{"bIsSensitive","bDrawRawBackground","bSpecialText","bNeedsReconfigure","bNeedsQuery",
            "bLastWidthSpecified","bLastHeightSpecified"}) set(name,Value::Bool(true));
        set("lockCount",Value::Integer(0));set("backgroundStyle",Value::Byte(1u));set("textVSpacing",Value::Float(1.0f));
        for(const auto* name:{"X","Y","hMargin0","hMargin1","vMargin0","vMargin1","lastSpecifiedWidth",
            "lastSpecifiedHeight","lastQueryWidth","lastQueryHeight"}) set(name,Value::Float(0.0f));
        for(const auto* name:{"Width","Height"}) set(name,Value::Float(10.0f));
        for(const auto* name:{"hardcodedWidth","hardcodedHeight","defaultHotX","defaultHotY"}) set(name,Value::Float(-1.0f));
        set("winHAlign",Value::Byte(0u));set("winVAlign",Value::Byte(0u));
        for(const auto* name:{"focusSound","unfocusSound","visibleSound","invisibleSound","defaultCursor",
            "defaultCursorShadow","clientObject"}) set(name,Value::Text(Kind::Object,{}));
        set("defaultCursorColor",CursorColorSeed());
        for(const auto* name:{"rowMajorIndex","colMajorIndex"}) set(name,Value::Integer(-1));
        for(const auto* name:{"att","firstTimer"}) set(name,Value::Integer(0));
        InitializeNativeWindowColors(window,"Extension.Window",execution);
        auto* gcClass=DefaultClass(Object("Extension.GC"));
        auto* windowGC=ConstructWindowObject(gcClass,window,execution);InitializeWindowGC(windowGC,execution);
        auto* generalGC=ConstructWindowObject(gcClass,window,execution);InitializeWindowGC(generalGC,execution);
        set("wGc",Value::Text(Kind::Object,windowGC->reflection.objectPath));
        set("gGc",Value::Text(Kind::Object,generalGC->reflection.objectPath));
        for(const auto* name:{"winParent","firstChild","lastChild","prevSibling","nextSibling"}) WindowLinkWrite(window,name,nullptr,execution);
        if(parent) LinkNewWindowChild(parent,window,execution);
        // Native parent notifications happen BEFORE clipping/font inheritance.
        NativeWindowClip(window,execution);
        RuntimeObject* fontParent=WindowLink(window,"winParent");
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        Value normal=Value::Text(Kind::Object,{}),bold=normal;
        while(fontParent) {
            LifecycleStep();
            if(depth==visited.size() || std::find(visited.begin(),visited.begin()+depth,fontParent)!=visited.begin()+depth)
                throw std::runtime_error("Native font inheritance ancestry cycles or exceeds its bound");
            visited[depth++]=fontParent;
            normal=Read(fontParent,DescribeProperty("Extension.Window.normalFont"),0u);
            bold=Read(fontParent,DescribeProperty("Extension.Window.boldFont"),0u);
            if(!normal.text.empty() || !bold.text.empty()) break;
            fontParent=WindowLink(fontParent,"winParent");
        }
        for(const auto& [name,font]:{std::pair{"normalFont",normal},std::pair{"boldFont",bold}}) {
            set(name,font);
            NativeWindowWrite(windowGC,("Extension.GC."+std::string(name)).c_str(),font,execution);
        }
        // bIsInitialized/bTextTranslucent, Background, SoundVolume, accelerator,
        // hold geometry, maxClicks and freeTimer retain original copied CDOs.
    }
    void ValidateNativeWindowList(RuntimeObject* window,const char* key) {
        const auto property=DescribeProperty(key);auto* metadata=Object(property.key);
        if(property.zero.kind!=Kind::Struct || property.arrayDimension!=1u || property.flags!=0x1000u ||
            !OwnsProperty(window->cls,property) || !metadata->property || metadata->property->type!="StructProperty" ||
            LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="extension.extensionobject.dynamicarray")
            throw std::runtime_error("Native window list lacks its original private DynamicArray declaration");
    }
    void InitializeRootTabAndModal(RuntimeObject* window,QuestVr::Vm::Execution& execution) {
        // Root.Init calls Modal.Init(None), which calls TabGroup.Init(None).
        // These native lists must be owned edges, not encoded raw pointers.
        ValidateNativeWindowList(window,"Extension.TabGroupWindow.rowMajorWindowList");
        ValidateNativeWindowList(window,"Extension.TabGroupWindow.colMajorWindowList");
        ValidateNativeWindowList(window,"Extension.ModalWindow.tabGroupWindowList");
        Touch(window);window->nativeWindowState.emplace();window->nativeWindowState->dirtyTabOrder=true;
        NativeWindowWrite(window,"Extension.Window.windowType",Value::Byte(1u),execution);
        NativeWindowWrite(window,"Extension.TabGroupWindow.bSizeChildrenToParent",Value::Bool(false),execution);
        NativeWindowWrite(window,"Extension.TabGroupWindow.bSizeParentToChildren",Value::Bool(true),execution);
        NativeWindowWrite(window,"Extension.TabGroupWindow.tabGroupIndex",Value::Integer(-1),execution);
        NativeWindowWrite(window,"Extension.TabGroupWindow.firstAbsX",Value::Float(0.0f),execution);
        NativeWindowWrite(window,"Extension.TabGroupWindow.firstAbsY",Value::Float(0.0f),execution);
        NativeWindowWrite(window,"Extension.Window.windowType",Value::Byte(2u),execution);
        NativeWindowWrite(window,"Extension.ModalWindow.focusMode",Value::Byte(2u),execution);
        NativeWindowWrite(window,"Extension.ModalWindow.preferredFocus",Value::Text(Kind::Object,{}),execution);
        const auto table=DescribeProperty("Extension.ModalWindow.acceleratorTable");
        if(table.zero.kind!=Kind::Object || table.arrayDimension!=255u || !OwnsProperty(window->cls,table))
            throw std::runtime_error("Native Modal accelerator table lacks its255 original Window slots");
        for(std::size_t slot=0u;slot<table.arrayDimension;++slot)
            execution.WriteReference(ReferenceFor(window,table,static_cast<std::uint32_t>(slot),false),Value::Text(Kind::Object,{}));
        NativeWindowWrite(window,"Extension.ModalWindow.bDirtyAccelerators",Value::Bool(true),execution);
        window->nativeWindowState->tabGroups.push_back(window);
        NativeWindowWrite(window,"Extension.TabGroupWindow.tabGroupIndex",Value::Integer(0),execution);
        ValidateStateBudget();
    }
    void InitializeWindowGC(RuntimeObject* object,QuestVr::Vm::Execution& execution) {
        const auto set=[&](const char* name,const Value& value) {
            const auto key="Extension.GC."+std::string(name);NativeWindowWrite(object,key.c_str(),value,execution);
        };
        for(const auto* field:{"Canvas","normalFont","boldFont","gcStack","gcFree","gcOwner"})
            set(field,Value::Text(Kind::Object,{}));
        set("Style",Value::Byte(1u));set("PolyFlags",Value::Integer(0x900));set("textPolyFlags",Value::Integer(0x802));
        for(const auto* field:{"bSmoothed","bMasked","bTranslucent","bModulated","bTextTranslucent","bFree"})
            set(field,Value::Bool(false));
        for(const auto* field:{"bDrawEnabled","bWordWrap","bParseMetachars"}) set(field,Value::Bool(true));
        for(const auto* field:{"gcCount","hMultiplier","vMultiplier"}) set(field,Value::Integer(1));
        for(const auto* field:{"underlineHeight","textVSpacing"}) set(field,Value::Float(1.0f));
        set("baselineOffset",Value::Float(2.0f));set("HAlign",Value::Byte(0u));set("VAlign",Value::Byte(0u));
        InitializeNativeWindowColors(object,"Extension.GC",execution);
        ZeroNativeWindowFloatStruct(object,"Extension.GC.gcClipRect",execution);
        // Retail preserves the original underlineTexture CDO (Extension.Solid).
        // No Canvas, draw backend or parent context is fabricated here.
    }
    void ZeroNativeWindowFloatStruct(RuntimeObject* object,const char* key,QuestVr::Vm::Execution& execution) {
        const auto field=DescribeProperty(key);auto value=Read(object,field,0u);
        const std::array<const char*,6u> channels{"originx","originy","clipx","clipy","clipwidth","clipheight"};
        if(value.kind!=Kind::Struct || value.fields.size()!=channels.size())
            throw std::runtime_error("Native GC clip lacks its exact six-Float original layout");
        for(const auto* channel:channels) if(!value.fields.count(channel))
            throw std::runtime_error("Native GC clip has a different channel identity");
        for(auto& [name,member]:value.fields) {
            static_cast<void>(name);
            if(member.kind!=Kind::Float) throw std::runtime_error("Native window float struct has a different layout");
            member=Value::Float(0.0f);
        }
        NativeWindowWrite(object,key,value,execution);
    }
    void InitializeNativeWindowColors(RuntimeObject* object,const std::string& base,QuestVr::Vm::Execution& execution) {
        for(const bool tile:{false,true}) {
            const auto key=base+(tile?".tileColor":".TextColor");const auto field=DescribeProperty(key);
            auto color=Read(object,field,0u);
            if(color.kind!=Kind::Struct) throw std::runtime_error("Native window color has no original struct layout");
            for(const auto& [channel,byte]:{
                std::pair{"r",tile?255u:0u},std::pair{"g",255u},std::pair{"b",tile?255u:0u}}) {
                const auto member=color.fields.find(channel);
                if(member==color.fields.end() || member->second.kind!=Kind::Byte)
                    throw std::runtime_error("Native window Color lacks its original Byte channels");
                member->second=Value::Byte(static_cast<std::uint8_t>(byte));
            }
            // Retail writes only RGB; no fixed-alpha initialization is proved.
            // Preserve the copied CDO A instead of inventing pixel coverage.
            NativeWindowWrite(object,key.c_str(),color,execution);
            const auto planeKey=base+(tile?".tilePlane":".textPlane");
            auto plane=Read(object,DescribeProperty(planeKey),0u);
            if(plane.kind!=Kind::Struct || plane.fields.size()!=4u)
                throw std::runtime_error("Native window Plane lacks its original four-Float layout");
            for(auto& [name,member]:plane.fields) {
                if(member.kind!=Kind::Float) throw std::runtime_error("Native window Plane channel is not Float");
                const auto lower=LowerAscii(name);
                if(lower!="x" && lower!="y" && lower!="z" && lower!="w")
                    throw std::runtime_error("Native window Plane channel identity is invalid");
                member=Value::Float(lower=="w"?0.0f:(tile || lower=="y")?1.0f:0.0f);
            }
            NativeWindowWrite(object,planeKey.c_str(),plane,execution);
        }
    }
    Evaluation InitRootWindowNative(RuntimeObject* pawn,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        if(!arguments.empty()) throw std::runtime_error("InitRootWindow takes no arguments");
        const auto authored=FunctionFor(Object("Extension.PlayerPawnExt.InitRootWindow"));
        if(authored->nativeIndex!=1052u || !(authored->flags&0x400u) ||
            (declaration && (LowerAscii(declaration->path)!=LowerAscii(authored->path) || declaration->source!=authored->source)))
            throw std::runtime_error("InitRootWindow native does not match its original declaration");
        RequireMapActor(pawn);
        if(!IsDerivedFromPath(pawn->cls,"Extension.PlayerPawnExt"))
            throw std::runtime_error("InitRootWindow receiver is not an actual PlayerPawnExt");
        if(pawn->nativeRootWindow) return {}; // Retail idempotence before config.
        if(!persistentRootWindowClass)
            throw std::runtime_error("InitRootWindow has no owned Engine.Engine.Root configuration");
        auto* cls=DefaultClass(Object(*persistentRootWindowClass));
        if(!IsDerivedFromPath(cls,"Extension.RootWindow"))
            throw std::runtime_error("Configured Engine.Engine.Root is not an actual RootWindow class");
        const auto pawnField=DescribeProperty("Extension.PlayerPawnExt.RootWindow");
        if(pawnField.zero.kind!=Kind::Object || pawnField.arrayDimension!=1u || !OwnsProperty(pawn->cls,pawnField))
            throw std::runtime_error("Player RootWindow has no original scalar schema");
        auto* root=ConstructWindowObject(cls,pawn,execution);
        root->nativeRootWindowObject=true;
        InitializeBaseWindow(root,nullptr,execution);
        InitializeRootTabAndModal(root,execution);
        NativeWindowWrite(root,"Extension.Window.windowType",Value::Byte(3u),execution);
        NativeWindowWrite(root,"Extension.Window.bTickEnabled",Value::Bool(true),execution);
        const auto rootSet=[&](const char* name,const Value& value) {
            const auto key="Extension.RootWindow."+std::string(name);NativeWindowWrite(root,key.c_str(),value,execution);
        };
        rootSet("parentPawn",Value::Text(Kind::Object,pawn->reflection.objectPath));
        auto* domain=persistentWindowDomain->get();
        rootSet("nextRootWindow",Value::Text(Kind::Object,domain->roots.empty()?std::string{}:domain->roots.back()->reflection.objectPath));
        for(const auto* name:{"MouseX","MouseY"}) rootSet(name,Value::Float(0.0f));
        for(const auto* name:{"prevMouseX","prevMouseY","firstButtonMouseX","firstButtonMouseY"}) rootSet(name,Value::Float(-1.0f));
        for(const auto* name:{"bMouseMoved","bCursorVisible","bRender","bPositionalSound"}) rootSet(name,Value::Bool(true));
        for(const auto* name:{"bMouseMoveLocked","bMouseButtonLocked","bClipRender"}) rootSet(name,Value::Bool(false));
        for(const auto* name:{"bStretchRawBackground","bShowStats","bShowFrames"}) rootSet(name,Value::Bool(false));
        for(const auto* name:{"FocusWindow","lastMouseWindow","grabbedWindow","lastButtonWindow","rawBackground","debugTexture"})
            rootSet(name,Value::Text(Kind::Object,{}));
        for(const auto* name:{"initCount","handleKeyboardRef","handleMouseRef","rootFrame","snapshotWidth","snapshotHeight",
            "clickCount","lastButtonType","paintCycles","TickCycles"}) rootSet(name,Value::Integer(0));
        for(const auto* name:{"renderX","renderY","renderWidth","renderHeight","rawBackgroundWidth","rawBackgroundHeight",
            "lastButtonPress","frameTimer"}) rootSet(name,Value::Float(0.0f));
        auto rawColor=CursorColorSeed();
        for(const auto* channel:{"r","g","b"}) rawColor.fields.at(channel)=Value::Byte(0u);
        // Retail Root.Init sets RGB black; its source local's A is undefined.
        // Defined A0 is the same explicit portable policy as cursor storage.
        rootSet("rawColor",rawColor);
        const auto keyMap=DescribeProperty("Extension.RootWindow.keyDownMap");
        if(keyMap.zero.kind!=Kind::Byte || keyMap.arrayDimension!=255u || !OwnsProperty(root->cls,keyMap))
            throw std::runtime_error("Native Root key map lacks its255 original Byte slots");
        for(std::size_t slot=0u;slot<keyMap.arrayDimension;++slot)
            execution.WriteReference(ReferenceFor(root,keyMap,static_cast<std::uint32_t>(slot),false),Value::Byte(0u));
        rootSet("multiClickTimeout",Value::Float(0.5f));rootSet("maxMouseDist",Value::Float(10.0f));
        // Native AddToRoot is a root-list operation, not RF flag mutation.
        domain->roots.push_back(root);
        auto pointer=std::make_shared<Reference>();pointer->zero=Value::Text(Kind::Object,{});
        pointer->read=[pawn] {return Value::Text(Kind::Object,pawn->nativeRootWindow?pawn->nativeRootWindow->reflection.objectPath:std::string{});};
        pointer->write=[this,pawn](const Value& value) {Touch(pawn);pawn->nativeRootWindow=value.text.empty()?nullptr:Object(value.text);};
        execution.WriteReference(pointer,Value::Text(Kind::Object,root->reflection.objectPath));
        const auto init=Member(root,"InitWindow");
        if(!init) throw std::runtime_error("Native root FindFunctionChecked InitWindow is unavailable");
        static_cast<void>(execution.CallFunction(*init,root->reflection.objectPath,{}));
        // SetVisibility invokes original VisibilityChanged, init-tree and
        // reconfiguration/focus/native destruction. Do not replace it with a
        // successful Bool write while those contracts remain unimplemented.
        throw std::runtime_error("RootWindow native SetVisibility/init-tree/reconfiguration is not implemented");
    }
    Value Read(RuntimeObject* object, const Property& property, const std::uint32_t index,
        const bool defaults = false, const bool immutable = false) {
        if(object->nativeWindowObject && !defaults && !immutable) {
            const auto key=LowerAscii(property.key);
            if(key=="extension.tabgroupwindow.rowmajorwindowlist" || key=="extension.tabgroupwindow.colmajorwindowlist" ||
                key=="extension.modalwindow.tabgroupwindowlist")
                throw std::runtime_error("Native Window private pointer-array storage is not a script-visible value");
        }
        if (object->defaultOwner) {
            const auto key=LowerAscii(property.key);
            if (key=="core.object.objectinternal")
                throw std::runtime_error("Class-default raw UObject pointer/index storage is not established");
            const auto scalar=[&](const Kind kind) {
                if (index!=0u || property.arrayDimension!=1u || property.zero.kind!=kind)
                    throw std::runtime_error("Class-default UObject header property has an invalid reflected layout");
            };
            if (key=="core.object.class") { scalar(Kind::Object);return Value::Text(Kind::Object,object->defaultOwner->reflection.objectPath); }
            if (key=="core.object.name") { scalar(Kind::Name);return Value::Text(Kind::Name,"None"); }
            if (key=="core.object.outer") { scalar(Kind::Object);return Value::Text(Kind::Object,{}); }
            if (key=="core.object.objectflags") { scalar(Kind::Int);return Value::Integer(0); }
        }
        // Internal selected-UClass reads are explicit default-storage queries
        // (Spawn/save/public ReadPortableClassDefault). Bytecode Variable rejects
        // a UClass receiver rather than confusing this with its Core.Class data.
        const bool classDefaults = defaults || object->classDescriptor || object->defaultOwner;
        if(!classDefaults && !immutable && index==0u) {
            const auto key=LowerAscii(property.key);
            if(key=="extension.playerpawnext.rootwindow")
                return Value::Text(Kind::Object,object->nativeRootWindow ?
                    object->nativeRootWindow->reflection.objectPath : std::string{});
            if(key=="engine.playerpawn.player" && object->nativePlayerBinding) {
                auto* player=object->nativePlayer;
                return Value::Text(Kind::Object,player &&
                    player->nativeClientIdentity==persistentNativeClient.ClientIdentity() ?
                        player->reflection.objectPath : std::string{});
            }
            if(key=="engine.player.actor" && object->nativeViewportIdentity)
                return Value::Text(Kind::Object,object->nativeViewportActor ?
                    object->nativeViewportActor->reflection.objectPath : std::string{});
        }
        if(!classDefaults && !immutable) {
            // Hidden inherited fields have distinct native offsets even when
            // the concrete class redeclares their leaf Name with another type.
            // Only the exact declaration can consume its qualified overlay.
            const auto qualified=object->scriptValues.find(LowerAscii(property.key));
            if(qualified!=object->scriptValues.end()) {
                const auto slot=qualified->second.find(index);
                if(slot!=qualified->second.end()) return slot->second;
            }
        }
        RuntimeObject* source = classDefaults ?
            (object->classDescriptor ? DefaultClass(object) : DefaultOwner(object)) : object;
        const auto selectedField = [&](RuntimeObject* storage) {
            auto* cls = storage->classDescriptor ? storage : storage->cls;
            std::array<RuntimeObject*,128u> visited{};
            std::size_t depth{};
            std::optional<bool> selected;
            for(;cls;cls=cls->base) {
                if (depth == visited.size() ||
                    std::find(visited.begin(),visited.begin()+depth,cls) != visited.begin()+depth)
                    throw std::runtime_error("Runtime class hierarchy cycles or exceeds its native work bound");
                visited[depth++] = cls;
                const auto found=persistentVmObjects.find(LowerAscii(cls->reflection.objectPath+'.'+property.name));
                if(!selected && cls->classFieldLinksValid && found!=persistentVmObjects.end() && found->second->property &&
                    found->second->linkedClassOwner==cls)
                    selected = LowerAscii(found->second->reflection.objectPath)==LowerAscii(property.key);
            }
            return selected.value_or(false);
        };
        // Tags and mutable patches use leaf names, but a class may redeclare a
        // inherited field (retail Actor/PlayerPawn.bSinglePlayer does). A tag
        // targets the nearest declaration in ITS source class, not every same-
        // named field on the ancestry. Keep inherited compiled references
        // distinct, and do not leak a parent's tag into a new declaration.
        if (classDefaults) {
            const auto values = source->classDefaultValues.find(LowerAscii(property.name));
            if (selectedField(source) && values != source->classDefaultValues.end()) {
                const auto slot = values->second.find(index);
                if (slot != values->second.end()) return slot->second;
            }
        } else {
            if (!immutable && selectedField(object)) {
                if (const auto* value = FindScriptOverlay(object, property.name.c_str(), index)) return *value;
            } else if (immutable && selectedField(object)) {
                const auto values=object->frozenBirthDefaults.find(LowerAscii(property.name));
                if (values!=object->frozenBirthDefaults.end()) {
                    const auto slot=values->second.find(index);
                    if (slot!=values->second.end()) return slot->second;
                }
            }
            if (LowerAscii(property.key) == "engine.actor.xlevel" && index == 0u && object->nativeXLevel)
                return Value::Text(Kind::Object, object->nativeXLevel->reflection.objectPath);
            if (LowerAscii(property.key) == "engine.actor.standingcount" && index == 0u && object->initialStandingCount)
                return Value::Byte(*object->initialStandingCount);
            if (LowerAscii(property.key) == "engine.actor.region" && property.zero.kind == Kind::Struct)
                return Region(object, property.zero);
        }
        // The instance adds one storage source before the bounded class chain.
        std::array<RuntimeObject*,129u> visitedSources{};
        std::size_t sourceDepth{};
        while (source != nullptr) {
            if (sourceDepth == visitedSources.size() ||
                std::find(visitedSources.begin(),visitedSources.begin()+sourceDepth,source) != visitedSources.begin()+sourceDepth)
                throw std::runtime_error("Runtime property source inheritance cycles or exceeds its native work bound");
            visitedSources[sourceDepth++] = source;
            const auto& properties = source->classDescriptor ? source->classDescriptor->defaults : source->instanceProperties;
            if (selectedField(source))
                for (auto tag = properties.rbegin(); tag != properties.rend(); ++tag) {
                    if (tag->name != property.name || tag->arrayIndex != index) continue;
                    return Decode(*source, *tag, property);
                }
            source = !classDefaults && source == object ? object->cls : source->base;
        }
        return property.zero;
    }
    Property PropertyNamed(RuntimeObject* object, const std::string& name) {
        return ClassProperty(object->classDescriptor ? DefaultClass(object) : object->cls, name);
    }
    struct ActorTimerSchema { Property rate,counter,loop; };
    ActorTimerSchema ActorTimerProperties(RuntimeObject* actor) {
        if (!IsDerivedFromPath(actor->cls,"Engine.Actor") || actor->classDescriptor)
            throw std::runtime_error("Actor timer receiver is not an Engine.Actor instance");
        ActorTimerSchema schema{PropertyNamed(actor,"TimerRate"),PropertyNamed(actor,"TimerCounter"),
            PropertyNamed(actor,"bTimerLoop")};
        const auto valid = [](const Property& property,const char* key,const Kind kind,const std::uint32_t flags) {
            return LowerAscii(property.key)==key && property.zero.kind==kind &&
                property.arrayDimension==1u && property.flags==flags;
        };
        if (!valid(schema.rate,"engine.actor.timerrate",Kind::Float,0u) ||
            !valid(schema.counter,"engine.actor.timercounter",Kind::Float,2u) ||
            !valid(schema.loop,"engine.actor.btimerloop",Kind::Bool,0u))
            throw std::runtime_error("Actor timer fields do not match the original scalar reflected schema");
        return schema;
    }
    static void ValidateSetTimerDeclaration(const Function& declaration) {
        if (LowerAscii(declaration.path)!="engine.actor.settimer" || declaration.nativeIndex!=280u ||
            declaration.flags!=0x401u || declaration.variables.size()!=2u)
            throw std::runtime_error("Native280 is not the authored Engine.Actor.SetTimer declaration");
        const auto& rate=declaration.variables[0];
        const auto& loop=declaration.variables[1];
        if (LowerAscii(rate.key)!="engine.actor.settimer.newtimerrate" || rate.zero.kind!=Kind::Float ||
            rate.flags!=0x80u || rate.arrayDimension!=1u ||
            LowerAscii(loop.key)!="engine.actor.settimer.bloop" || loop.zero.kind!=Kind::Bool ||
            loop.flags!=0x80u || loop.arrayDimension!=1u)
            throw std::runtime_error("SetTimer requires the two original mandatory Float/Bool parameters");
    }
    bool AdvanceTimer(RuntimeObject* actor,const float elapsed,QuestVr::Vm::Execution& execution) {
        RequireMapActor(actor);
        if (!actor->active && !actor->worldRemoved)
            throw std::runtime_error("Actor timer receiver is inactive");
        const auto schema=ActorTimerProperties(actor);
        const auto rate=Read(actor,schema.rate,0u), counter=Read(actor,schema.counter,0u),
            loop=Read(actor,schema.loop,0u);
        if (rate.kind!=Kind::Float || counter.kind!=Kind::Float || loop.kind!=Kind::Bool)
            throw std::runtime_error("Actor timer values do not match their reflected Float/Bool fields");
        const auto step=QuestVr::AdvanceActorTimer(rate.floating,counter.floating,loop.boolean,elapsed);
        if (!SameBits(step.counter,counter.floating))
            execution.WriteReference(ReferenceFor(actor,schema.counter,0u,false),Value::Float(step.counter));
        if (!SameBits(step.rate,rate.floating))
            execution.WriteReference(ReferenceFor(actor,schema.rate,0u,false),Value::Float(step.rate));
        if (!step.expired) return false;

        // Retail ProcessEvent rejects before-begun events before probing the
        // state frame. Consume the timer even on a raw runnable stack whose
        // continuation cannot yet be executed by the portable VM. Deletion
        // also suppresses the callback, not the already completed timer phase.
        const auto levelValue=Read(actor,PropertyNamed(actor,"Level"),0u);
        if (levelValue.kind!=Kind::Object || levelValue.text.empty())
            throw std::runtime_error("Actor timer receiver has no authored Level binding");
        auto* level=Object(levelValue.text);
        if (!IsDerivedFromPath(level->cls,"Engine.LevelInfo"))
            throw std::runtime_error("Actor timer Level is not Engine.LevelInfo");
        const auto begun=Read(level,PropertyNamed(level,"bBegunPlay"),0u);
        const auto deleted=Read(actor,PropertyNamed(actor,"bDeleteMe"),0u);
        if (begun.kind!=Kind::Bool || deleted.kind!=Kind::Bool)
            throw std::runtime_error("Actor timer begun/deletion gates are not reflected Bool values");
        if (!begun.boolean || deleted.boolean) return true;

        // Same interpreter, budgets and root host journal as the field writes.
        // No cached class Timer, nested public transaction or second callback.
        static_cast<void>(execution.CallEvent(actor->reflection.objectPath,"Timer",false,{}));
        return true;
    }
    Property ClassProperty(RuntimeObject* actorClass, const std::string& name) {
        std::array<RuntimeObject*,128u> visited{};
        std::size_t depth{};
        RuntimeObject* selected{};
        for (RuntimeObject* cls = actorClass; cls != nullptr; cls = cls->base) {
            if (depth == visited.size() ||
                std::find(visited.begin(),visited.begin()+depth,cls) != visited.begin()+depth)
                throw std::runtime_error("Runtime class hierarchy cycles or exceeds its native work bound");
            visited[depth++] = cls;
            // Retain the nearest-field/metadata checks, but validate the whole
            // Super chain even after a nearest declaration has been selected.
            if (selected) continue;
            if (cls->classDescriptor && !cls->classFieldLinksValid)
                throw std::runtime_error("Runtime class Children/Next metadata is invalid: " + cls->reflection.objectPath);
            const auto found = persistentVmObjects.find(LowerAscii(cls->reflection.objectPath + '.' + name));
            if (found != persistentVmObjects.end() && found->second->property &&
                found->second->linkedClassOwner == cls) selected = found->second;
        }
        if (selected) return Describe(selected);
        throw std::runtime_error("Runtime property metadata unavailable: " + name);
    }
    Property DescribeProperty(const std::string& key) { return Describe(Object(key)); }
    std::string InstanceStorageKey(RuntimeObject* cls,const Property& property) {
        if(!OwnsProperty(cls,property))
            throw std::runtime_error("Qualified instance property is not owned by its concrete receiver class");
        const auto selected=ClassProperty(cls,property.name);
        return LowerAscii(selected.key)==LowerAscii(property.key)?LowerAscii(property.name):LowerAscii(property.key);
    }
    bool OwnsProperty(RuntimeObject* concreteClass,const Property& property) {
        // Compiled inherited references retain their declaring field even
        // when the concrete class redeclares a field with the same name.
        auto* metadata = Object(property.key);
        if (!metadata->property || !metadata->outer || !metadata->outer->classDescriptor ||
            !metadata->outer->classFieldLinksValid || metadata->linkedClassOwner != metadata->outer)
            return false;
        std::array<RuntimeObject*,128u> visited{};
        std::size_t depth{};
        bool owns{};
        for (auto* cls=concreteClass;cls;cls=cls->base) {
            if (depth == visited.size() ||
                std::find(visited.begin(),visited.begin()+depth,cls) != visited.begin()+depth)
                throw std::runtime_error("Runtime class hierarchy cycles or exceeds its native work bound");
            visited[depth++] = cls;
            owns = owns || cls==metadata->outer;
        }
        return owns;
    }
    RuntimeObject* DefaultClass(RuntimeObject* object) {
        auto* cls = object->classDescriptor ? object : object->cls;
        if (!cls || !cls->classDescriptor || cls->reflection.metaClass != "Class" ||
            object->defaultOwner || !cls->active ||
            LowerAscii(std::filesystem::path(cls->sourcePath).extension().string()) == ".dx" ||
            Table(cls->sourcePath).exports.at(cls->exportIndex).ObjClass != 0)
            throw std::runtime_error("Class-default receiver is not a loaded script UClass");
        return cls;
    }
    RuntimeObject* DefaultOwner(RuntimeObject* object) {
        if (object->defaultOwner) return DefaultClass(object->defaultOwner);
        // A UClass object's actual class is Core.Class, not itself. Native
        // metaclass default storage is not synthesized from its selected class.
        if (object->classDescriptor)
            throw std::runtime_error("UClass receiver Core.Class default storage is not established");
        return DefaultClass(object);
    }
    bool HasPropertyNamed(RuntimeObject* actor, const char* name) const {
        std::array<RuntimeObject*,128u> visited{};
        std::size_t depth{};
        bool present{};
        for (auto* cls=actor->cls;cls;cls=cls->base) {
            if (depth == visited.size() ||
                std::find(visited.begin(),visited.begin()+depth,cls) != visited.begin()+depth)
                throw std::runtime_error("Runtime class hierarchy cycles or exceeds its native work bound");
            visited[depth++] = cls;
            const auto found=persistentVmObjects.find(LowerAscii(cls->reflection.objectPath+'.'+name));
            present = present || (found!=persistentVmObjects.end() && found->second->property);
        }
        return present; // Explicit metadata-only fixtures lack native Actor fields.
    }
    bool HasCollisionSchema(RuntimeObject* actor) const { return HasPropertyNamed(actor,"bCollideActors"); }
    QuestVr::ActorOverlapRecord CollisionRecord(RuntimeObject* actor,
        const std::function<Value(const Property&)>& restored = {}) {
        const auto get=[&](const char* name) {
            const auto property=PropertyNamed(actor,name);
            return restored ? restored(property) : Read(actor,property,0u);
        };
        const auto location=get("Location");
        if (location.kind!=Kind::Vector) throw std::runtime_error("Actor collision Location is not Vector");
        return {reinterpret_cast<std::uintptr_t>(actor),
            {location.vector[0],location.vector[1],location.vector[2]},
            QuestVr::Vm::ToFloat(get("CollisionRadius")),QuestVr::Vm::ToFloat(get("CollisionHeight")),
            !get("Brush").text.empty(),QuestVr::Vm::ToBool(get("bCollideActors"))};
    }
    void RegisterCollisionActor(RuntimeObject* actor, QuestVr::ActorCollisionRegistry& registry,
        const std::function<Value(const Property&)>& restored = {}) {
        if (!HasCollisionSchema(actor)) return;
        const auto record=CollisionRecord(actor,restored);
        std::optional<QuestVr::ActorCollisionBox> bounds;
        if (record.collideActors && record.brush && IsDerivedFromPath(actor->cls,"Engine.Mover")) {
            const auto get=[&](const char* name) {
                const auto property=PropertyNamed(actor,name);
                return restored ? restored(property) : Read(actor,property,0u);
            };
            auto* brush=Object(get("Brush").text);
            if (brush->runtimeBirth) throw std::runtime_error("Mover Brush is not a serialized Model");
            const auto model=LoadPortableModel68(Table(brush->sourcePath),brush->exportIndex);
            const auto rotation=get("Rotation"), pivot=get("PrePivot"), scale=get("MainScale");
            if (rotation.kind!=Kind::Rotator || pivot.kind!=Kind::Vector || scale.kind!=Kind::Struct ||
                scale.fields.at("scale").kind!=Kind::Vector)
                throw std::runtime_error("Mover collision transform has invalid concrete fields");
            const auto& axes=scale.fields.at("scale").vector;
            const auto transform=mat4::translate(record.location.x,record.location.y,record.location.z) *
                Coords::Rotation(Rotator(rotation.rotation[0],rotation.rotation[1],rotation.rotation[2])).ToMatrix() *
                mat4::scale(axes[0],axes[1],axes[2]) *
                mat4::translate(-pivot.vector[0],-pivot.vector[1],-pivot.vector[2]);
            const auto box=BBox({model.bounds.minimum.x,model.bounds.minimum.y,model.bounds.minimum.z},
                {model.bounds.maximum.x,model.bounds.maximum.y,model.bounds.maximum.z}).transform(transform);
            const auto center=box.center();
            const auto extents=box.extents()+vec3(0.1f);
            bounds=QuestVr::ActorCollisionBox{{center.x,center.y,center.z},{extents.x,extents.y,extents.z}};
        }
        static_cast<void>(registry.Add(record,bounds));
    }
    std::shared_ptr<const QuestVr::AuthoredStructSchema> StructPropertySchema(const std::string& key) {
        auto* property = Object(key);
        if (!property->property || property->property->type != "StructProperty")
            throw std::runtime_error("Runtime property is not an authored StructProperty");
        return StructSchema(Object(Qualified(Table(property->sourcePath), property->property->referencedType)));
    }
    Value ProbeWorldStartup(const PortableWorldStartupProbeOptions& options,
        PortableWorldStartupProbeResult& result, QuestVr::Vm::Execution& execution) {
        if (!transaction_) throw std::runtime_error("World startup probe requires its outer VM transaction");
        struct ProbeScope { bool& active; ~ProbeScope() { active = false; } } scope{worldStartupProbe_};
        worldStartupProbe_ = true;
        using Phase = PortableWorldStartupPhase;
        const auto visit = [&](const Phase phase, RuntimeObject* actor, const std::size_t slot) {
            LifecycleStep();
            result.phase = phase; result.actorSlot = slot;
            result.actorPath = actor ? actor->reflection.objectPath : std::string{};
            ++result.visited.at(static_cast<std::size_t>(phase));
        };
        const auto write = [&](RuntimeObject* actor, const char* name, const Value& value) {
            ReferenceFor(actor, PropertyNamed(actor, name), 0u, false)->write(value);
        };
        if (persistentMapSourcePath.empty() || persistentWorldActors.empty() || !persistentWorldActors.front())
            throw std::runtime_error("World startup requires a loaded actual ULevel with slot-zero LevelInfo");
        auto* info = NativeLevelInfo(persistentWorldActors.front());
        visit(Phase::ValidateWorld, info, 0u);
        const auto validText = [](const std::string& value, const std::size_t bound) {
            return value.size() <= bound && value.find('\0') == std::string::npos;
        };
        if (!validText(options.urlGameClass, 4096u) || !validText(options.defaultGameClass, 4096u) ||
            !validText(options.defaultServerGameClass, 4096u) ||
            !validText(options.initGameOptions, 8192u))
            throw std::runtime_error("World startup configuration is oversized or contains an embedded NUL");
        // This diagnostic targets standalone authority. Client-only removal /
        // Role swapping is a separate original branch, never silently emulated.
        if (QuestVr::Vm::ToInt(Read(info, PropertyNamed(info, "NetMode"), 0u)) != 0)
            throw std::runtime_error("World startup probe requires original NM_Standalone authority");
        visit(Phase::SelectGameInfo, info, 0u);
        auto* game = ActorLink(info, "Game");
        if (!game) {
            RuntimeObject* gameClass{};
            if (!options.urlGameClass.empty()) gameClass = Object(options.urlGameClass);
            else {
                const auto mapClass = Read(info, PropertyNamed(info, "DefaultGameType"), 0u);
                if (mapClass.kind != Kind::Object)
                    throw std::runtime_error("LevelInfo.DefaultGameType is not a Class reference");
                if (!mapClass.text.empty()) gameClass = Object(mapClass.text);
                else {
                    // Original LoadMap selects ini DefaultGame by Client
                    // presence, not viewport count, tracking or PlayerPawn.
                    const bool localClient = persistentNativeClient.HasClient();
                    const auto& configured = localClient ? options.defaultGameClass : options.defaultServerGameClass;
                    if (configured.empty()) throw std::runtime_error(localClient ?
                        "World startup has no map GameInfo class or configured local DefaultGame" :
                        "World startup has no map GameInfo class or configured DefaultServerGame without a native client");
                    gameClass = Object(configured);
                }
            }
            if (!gameClass->classDescriptor || !IsDerivedFromPath(gameClass, "Engine.GameInfo"))
                throw std::runtime_error("World startup selected class is not an actual GameInfo UClass");
            // LoadMap directly calls ULevel::SpawnActor with null Owner,
            // Instigator and name, and zero Location/Rotation. Native birth
            // assigns Tag=Class.Name; execSpawn's captured Instigator and
            // post-return SpawnTag assignment do NOT
            // belong to that call. Share native allocation, not that wrapper.
            const auto born = SpawnNative(info->reflection.objectPath,
                {{Value::Text(Kind::Object, gameClass->reflection.objectPath), {}},
                 {Value::Text(Kind::Object, {}), {}}, {Value::Text(Kind::Name, {}), {}},
                 {Value::Vector({0.0f, 0.0f, 0.0f}), {}}, {Value::Rotator({0, 0, 0}), {}}},
                execution, true).Load();
            if (born.kind != Kind::Object || born.text.empty())
                throw std::runtime_error("Original world GameInfo native SpawnActor returned None");
            game = Object(born.text);
            write(info, "Game", born);
        }
        if (!IsDerivedFromPath(game->cls, "Engine.GameInfo") || NativeLevelInfo(game) != info)
            throw std::runtime_error("LevelInfo.Game is not an actual GameInfo in its native Level");
        result.gameInfoPath = game->reflection.objectPath;
        result.restoredWorld = QuestVr::Vm::ToBool(Read(info, PropertyNamed(info, "bBegunPlay"), 0u));
        // Index/count is deliberately re-read after every callback. Nested
        // Spawn can append actors that this SAME pass must subsequently visit.
        const auto each = [&](const Phase phase, const auto& operation) {
            for (std::size_t slot = 0u; slot < persistentWorldActors.size(); ++slot) {
                LifecycleStep();
                auto* actor = persistentWorldActors[slot];
                if (!actor || actor->worldRemoved) continue;
                visit(phase, actor, slot);
                operation(actor);
            }
        };
        const auto events = [&](const Phase phase, const char* name, const bool enumDispatch) {
            each(phase, [&](RuntimeObject* actor) {
                static_cast<void>(execution.CallEvent(actor->reflection.objectPath, name, enumDispatch, {}));
            });
        };
        if (!result.restoredWorld) {
            visit(Phase::ResetTime, info, 0u);
            result.worldTime = 0.0f;
            write(info, "TimeSeconds", Value::Float(0.0f));
            each(Phase::ClearTouching, [&](RuntimeObject* actor) {
                const auto property = PropertyNamed(actor, "Touching");
                if (property.zero.kind != Kind::Object || property.arrayDimension != 4u)
                    throw std::runtime_error("Original world Touching reset requires Actor.Touching[4]");
                for (std::uint32_t slot = 0u; slot < 4u; ++slot)
                    ReferenceFor(actor, property, slot, false)->write(Value::Text(Kind::Object, {}));
            });
            each(Phase::InitializeExecution, [&](RuntimeObject* actor) { InitializeActorExecution(actor); });
            visit(Phase::PublishStartup, info, 0u);
            write(info, "bBegunPlay", Value::Bool(true));
            write(info, "bStartup", Value::Bool(true));
            visit(Phase::InitGame, game, static_cast<std::size_t>(game->worldActorIndex));
            auto error = std::make_shared<Value>(Value::Text(Kind::String, {}));
            auto out = std::make_shared<Reference>(); out->zero = *error;
            // Owned OUT storage stays alive through the outer Machine's
            // rollback, including after this coordinator callback unwinds.
            out->read = [error] { return *error; };
            out->write = [error](const Value& value) { *error = value; };
            static_cast<void>(execution.CallEvent(game->reflection.objectPath, "InitGame", false,
                {{Value::Text(Kind::String, options.initGameOptions), {}}, {*error, out}}));
            if (error->kind != Kind::String) throw std::runtime_error("InitGame OUT Error did not remain a String");
            result.initGameError = error->text;
            // Original LoadMap copies this OUT string but does NOT test it for
            // nonempty here. VM/native failures still abort the transaction.
            events(Phase::PreBeginPlay, "PreBeginPlay", true);
            events(Phase::BeginPlay, "BeginPlay", true);
            each(Phase::SetActorZone, [&](RuntimeObject* actor) { InitializeWorldZone(actor, info); });
            each(Phase::LastRenderTime, [&](RuntimeObject* actor) { Touch(actor); actor->lastRenderTime = -10.0f; });
            events(Phase::PostBeginPlay, "PostBeginPlay", true);
            events(Phase::SetInitialState, "SetInitialState", true);
            each(Phase::WorldBases, [&](RuntimeObject* actor) {
                const auto tagValue = Read(actor, PropertyNamed(actor, "AttachTag"), 0u);
                if (tagValue.kind != Kind::Name) throw std::runtime_error("World AttachTag is not a Name");
                const auto attachTag = QuestVr::ScriptDispatch::FoldName(tagValue.text);
                if (!attachTag.empty() && attachTag != "none") {
                    for (std::size_t slot = 0u; slot < persistentWorldActors.size(); ++slot) {
                        LifecycleStep();
                        auto* candidate = persistentWorldActors[slot];
                        if (!candidate || candidate->worldRemoved) continue;
                        const auto tag = Read(candidate, PropertyNamed(candidate, "Tag"), 0u);
                        if (tag.kind != Kind::Name) throw std::runtime_error("World attachment candidate Tag is not a Name");
                        if (QuestVr::ScriptDispatch::FoldName(tag.text) != attachTag) continue;
                        SetBase(actor, candidate, false, execution); break;
                    }
                    return; // No match still MUST NOT fall through to FindBase.
                }
                const bool physical = IsDerivedFromPath(actor->cls, "Engine.Decoration") ||
                    IsDerivedFromPath(actor->cls, "Engine.Inventory") || IsDerivedFromPath(actor->cls, "Engine.Pawn");
                const auto physics = QuestVr::Vm::ToInt(Read(actor, PropertyNamed(actor, "Physics"), 0u));
                if (physical && !ActorLink(actor, "Base") &&
                    QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, "bCollideWorld"), 0u)) &&
                    (physics == 0 || physics == 5))
                    throw std::runtime_error("Unsupported original world FindBase: extent SingleLineCheck flags0x17 / SupportActor contract is not implemented");
            });
            visit(Phase::ClearStartup, info, 0u);
            write(info, "bStartup", Value::Bool(false));
        } else {
            visit(Phase::RestoreTime, info, 0u);
            result.worldTime = QuestVr::Vm::ToFloat(Read(info, PropertyNamed(info, "TimeSeconds"), 0u));
            if (!std::isfinite(result.worldTime)) throw std::runtime_error("Restored native world time is not finite");
        }
        events(Phase::PostPostBeginPlay, "PostPostBeginPlay", false);
        visit(Phase::CompactActorSlots, info, 0u);
        // The current save/birth manifest assumes an authored-slot prefix and
        // appended births. Original static/dynamic compaction needs a genuine
        // persistent ordering format before it can be published. Always fail
        // this probe, rolling back ALL phases, rather than accept half a world.
        throw std::runtime_error("Unsupported original world slot compaction/save ordering; non-publishing startup probe rolled back");
    }
private:
    void InitializeWorldZone(RuntimeObject* actor, RuntimeObject* info) {
        // Original SetActorZone(actor,1,1): reset old regions first; suppress
        // every zone callback. There is no SpawnZone water/Physics/Base branch.
        if (QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, "bDeleteMe"), 0u))) return;
        const auto store = [&](const char* name, const float offset) {
            const auto property = DescribeProperty(std::string(std::strcmp(name,"Region")==0?"Engine.Actor.":"Engine.Pawn.")+name);
            ReferenceFor(actor, property, 0u, false)->write(Region(actor, property.zero, offset));
        };
        store("Region", 0.0f);
        if (!IsDerivedFromPath(actor->cls, "Engine.Pawn")) return;
        store("FootRegion", -QuestVr::Vm::ToFloat(Read(actor, PropertyNamed(actor, "CollisionHeight"), 0u)));
        store("HeadRegion", QuestVr::Vm::ToFloat(Read(actor, PropertyNamed(actor, "EyeHeight"), 0u)));
        if (QuestVr::Vm::ToInt(Read(info, PropertyNamed(info, "NetMode"), 0u)) == 3) return;
        auto* pri = ActorLink(actor, "PlayerReplicationInfo");
        if (pri) ReferenceFor(pri, PropertyNamed(pri, "PlayerZone"), 0u, false)->write(
            Read(actor, DescribeProperty("Engine.Actor.Region"), 0u).fields.at("zone"));
    }
    void SpawnZone(RuntimeObject* actor, QuestVr::Vm::Execution& execution) {
        const auto regionProperty = DescribeProperty("Engine.Actor.Region");
        auto region = Region(actor, regionProperty.zero);
        ReferenceFor(actor, regionProperty, 0u, false)->write(region);
        auto* zone = Object(region.fields.at("zone").text);
        if (QuestVr::Vm::ToBool(Read(zone, PropertyNamed(zone, "bWaterZone"), 0u)) &&
            !IsDerivedFromPath(actor->cls, "Engine.Projectile")) {
            ReferenceFor(actor, PropertyNamed(actor, "Physics"), 0u, false)->write(Value::Byte(4u));
            SetBase(actor, nullptr, true, execution);
        }
        if (IsDerivedFromPath(actor->cls, "Engine.Pawn")) {
            const auto height = Read(actor, PropertyNamed(actor, "CollisionHeight"), 0u);
            const auto eyes = Read(actor, PropertyNamed(actor, "EyeHeight"), 0u);
            for (const auto& [name, offset] : {std::pair{"FootRegion", -QuestVr::Vm::ToFloat(height)},
                                               std::pair{"HeadRegion", QuestVr::Vm::ToFloat(eyes)}}) {
                const auto property = PropertyNamed(actor, name);
                ReferenceFor(actor, property, 0u, false)->write(Region(actor, property.zero, offset));
            }
            const auto pri = Read(actor, PropertyNamed(actor, "PlayerReplicationInfo"), 0u);
            if (!pri.text.empty()) {
                auto* target = Object(pri.text);
                ReferenceFor(target, PropertyNamed(target, "PlayerZone"), 0u, false)->write(region.fields.at("zone"));
            }
        }
    }
    void SpawnBase(RuntimeObject* actor, QuestVr::Vm::Execution& execution) {
        // Original GOTY SpawnActor (0x10395612--0x103956a1) only invokes
        // FindBase under the conditions below. AttachTag resolution belongs
        // to LoadMap's separate world-startup base pass, not actor births.
        const bool physical = IsDerivedFromPath(actor->cls, "Engine.Decoration") ||
            IsDerivedFromPath(actor->cls, "Engine.Inventory") || IsDerivedFromPath(actor->cls, "Engine.Pawn");
        const auto physics = QuestVr::Vm::ToInt(Read(actor, PropertyNamed(actor, "Physics"), 0u));
        if (physical && !ActorLink(actor, "Base") &&
            QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, "bCollideWorld"), 0u)) &&
            (physics == 0 || physics == 5)) {
            if (worldStartupProbe_)
                throw std::runtime_error("Unsupported original spawned FindBase during world startup probe: extent SingleLineCheck / SupportActor contract is not implemented");
            const auto record=CollisionRecord(actor);
            const auto overlap=persistentActorCollision.Query(record.location,record.height,record.radius);
            std::vector<RuntimeObject*> hits;
            for (const auto identity : overlap.actors) hits.push_back(reinterpret_cast<RuntimeObject*>(identity));
            std::stable_sort(hits.begin(),hits.end(),std::less<RuntimeObject*>{});
            if (!hits.empty()) SetBase(actor,hits.front(),true,execution);
        }
    }
    Evaluation SpawnNative(const std::string& receiver, const std::vector<Evaluation>& arguments,
        QuestVr::Vm::Execution& execution, const bool worldGameInfo = false) {
        LifecycleStep();
        auto* spawner = Object(receiver); RequireMapActor(spawner);
        std::vector<Value> values;
        values.reserve(arguments.size());
        for (const auto& argument : arguments) values.push_back(argument.Load());
        if ((!spawner->active && !spawner->worldRemoved) || arguments.empty() || arguments.size() > 5u ||
            values[0].kind != Kind::Object)
            throw std::runtime_error("Spawn receiver/class/argument count is invalid");
        const auto none = [] { return Evaluation{Value::Text(Kind::Object, {}), {}}; };
        if (values[0].text.empty()) return none();
        auto* cls = Object(values[0].text);
        if (!cls->classDescriptor) throw std::runtime_error("Spawn Class is not an actual UClass");
        cls = DefaultClass(cls);
        if (!IsDerivedFromPath(cls,"Engine.Actor"))
            throw std::runtime_error("Spawn Class is not derived from Engine.Actor");
        if (!cls->classDescriptor || (cls->classDescriptor->classFlags & 1u)) return none();
        if (worldStartupProbe_ && IsDerivedFromPath(cls, "Engine.Mover"))
            throw std::runtime_error("Unsupported original AMover native Spawned transform storage during world startup probe");
        auto* owner = values.size() > 1u && values[1].kind != Kind::Nothing &&
            !values[1].text.empty() ? Object(values[1].text) : nullptr;
        if (values.size() > 1u && values[1].kind != Kind::Nothing && values[1].kind != Kind::Object)
            throw std::runtime_error("Spawn Owner is not an Object");
        if (owner) RequireMapActor(owner);
        if (values.size() > 2u && values[2].kind != Kind::Nothing) {
            if (values[2].kind != Kind::Name) throw std::runtime_error("Spawn Tag is not a Name");
        }
        auto location = values.size() > 3u && values[3].kind != Kind::Nothing ?
            values[3] : Read(spawner, PropertyNamed(spawner, "Location"), 0u);
        auto rotation = values.size() > 4u && values[4].kind != Kind::Nothing ?
            values[4] : Read(spawner, PropertyNamed(spawner, "Rotation"), 0u);
        // execSpawn captures Instigator after optional argument evaluation and
        // passes it into Level::SpawnActor. SetOwner callbacks may mutate the
        // spawner before SpawnActor publishes that captured value on the birth.
        const auto instigator = worldGameInfo ? Value::Text(Kind::Object, {}) :
            Read(spawner, PropertyNamed(spawner, "Instigator"), 0u);
        if (location.kind != Kind::Vector || rotation.kind != Kind::Rotator)
            throw std::runtime_error("Spawn Location/Rotation have invalid concrete kinds");
        for (const auto component : location.vector)
            if (!std::isfinite(component)) throw std::runtime_error("Spawn location is not finite");
        const auto flag = [&](const char* name) { return QuestVr::Vm::ToBool(Read(cls, ClassProperty(cls, name), 0u)); };
        if (flag("bCollideWorld") || flag("bCollideWhenPlacing")) {
            if (!rootModel_) rootModel_ = LoadPortableRootModel68(Table(persistentMapSourcePath));
            const auto placed = QuestVr::FindSpawnPlacement(*rootModel_,
                {location.vector[0],location.vector[1],location.vector[2]},
                QuestVr::Vm::ToFloat(Read(cls, ClassProperty(cls,"CollisionRadius"),0u)),
                QuestVr::Vm::ToFloat(Read(cls, ClassProperty(cls,"CollisionHeight"),0u)), true, false);
            if (!placed.found) return none();
            location = Value::Vector({placed.location.x, placed.location.y, placed.location.z});
        }
        auto* level = worldGameInfo ? NativeLevelInfo(spawner) : ActorLink(spawner, "Level");
        if (!level || !IsDerivedFromPath(level->cls,"Engine.LevelInfo")) throw std::runtime_error("Spawn has no LevelInfo");
        const auto xlevelValue = worldGameInfo ? Value::Text(Kind::Object, spawner->nativeXLevel->reflection.objectPath) :
            Read(spawner, PropertyNamed(spawner,"XLevel"),0u);
        if (xlevelValue.kind != Kind::Object || xlevelValue.text.empty()) throw std::runtime_error("Spawn has no XLevel");
        auto* xlevel = Object(xlevelValue.text);
        if (xlevel->runtimeBirth || xlevel->sourcePath != persistentMapSourcePath ||
            LowerAscii(CastObjectIdentity(xlevelValue.text).classPath) != "engine.level")
            throw std::runtime_error("Spawn XLevel is not the current serialized Level");
        if (worldStartupProbe_ && ActorLink(level, "SpawnNotify"))
            throw std::runtime_error("Unsupported original native SpawnNotify chain during world startup probe");
        if (persistentWorldActors.size() >= 1'000'000u) throw std::runtime_error("Spawn world slot budget exceeded");
        const auto className = cls->reflection.objectPath.substr(cls->reflection.objectPath.find_last_of('.') + 1u);
        std::string path;
        for (std::size_t suffix = 0u; suffix <= 1'000'000u; ++suffix) {
            LifecycleStep(); path = persistentMapPackageName + '.' + className + std::to_string(suffix);
            if (!persistentVmObjects.count(LowerAscii(path))) break;
            if (suffix == 1'000'000u) throw std::runtime_error("Spawn name budget exceeded");
        }
        pendingBirthGarbage = true;
        GCRoot<RuntimeObject> rooted(GC::Alloc<RuntimeObject>(PortableReflectionObject{
            path, className, xlevel->outer ? xlevel->outer->reflection.objectPath : std::string{}, {}, 0x4000u, 0}, nullptr));
        auto* actor = rooted.get(); actor->runtimeBirth = true; actor->birthVmKey = LowerAscii(path);
        actor->sourcePath = persistentMapSourcePath; actor->exportIndex = std::numeric_limits<std::size_t>::max();
        actor->cls = cls; actor->outer = xlevel->outer; actor->nativeXLevel = xlevel;
        actor->inventoryTableMetadata = true; actor->inventoryClassReferenceValid = true;
        actor->references.push_back(cls); if (actor->outer) actor->references.push_back(actor->outer);
        actor->frozenBirthDefaults = cls->classDefaultValues;
        actor->worldActorIndex = static_cast<std::int32_t>(persistentWorldActors.size());
        persistentRuntime->get()->exports.push_back(actor);
        if (!persistentQualifiedObjects.emplace(path,actor).second || !persistentVmObjects.emplace(actor->birthVmKey,actor).second)
            throw std::runtime_error("Spawn identity collision");
        persistentWorldActors.push_back(actor);
        const auto set = [&](const char* name, const Value& value) { ReferenceFor(actor,PropertyNamed(actor,name),0u,false)->write(value); };
        set("Class",Value::Text(Kind::Object,cls->reflection.objectPath));
        set("Name",Value::Text(Kind::Name,path.substr(path.find_last_of('.') + 1u)));
        set("ObjectFlags",Value::Integer(0x4000));
        set("Outer",Value::Text(Kind::Object,actor->reflection.outerPath));
        set("XLevel",xlevelValue); set("Level",Value::Text(Kind::Object,level->reflection.objectPath));
        set("Tag",Value::Text(Kind::Name,className));
        set("bTicked",Read(spawner,PropertyNamed(spawner,"bTicked"),0u));
        set("Brush",Value::Text(Kind::Object,{})); set("Location",location); set("OldLocation",location); set("Rotation",rotation);
        // Native AActor.Region is a fixed declared PointRegion slot. The
        // original GameReplicationInfo also declares an unrelated Int Region;
        // leaf-name lookup must not select that field or fail with map::at.
        const auto regionProperty=DescribeProperty("Engine.Actor.Region");
        auto region = Read(actor,regionProperty,0u,true);
        if(region.kind!=Kind::Struct || region.fields.find("zone")==region.fields.end())
            throw std::runtime_error("Native Spawn Actor.Region has no original PointRegion layout");
        region.fields.at("zone") = Value::Text(Kind::Object,level->reflection.objectPath);
        ReferenceFor(actor,regionProperty,0u,false)->write(region);
        TouchCollision(); RegisterCollisionActor(actor,persistentActorCollision);
        SetOwner(actor,owner,execution);
        set("Instigator",instigator);
        InitializeActorExecution(actor);
        if (!QuestVr::Vm::ToBool(Read(level,PropertyNamed(level,"bBegunPlay"),0u))) {
            if (worldGameInfo) {
                // Original direct GameInfo birth does NOT return at the
                // ProcessEvent begun-play gate. Its native Spawned is empty,
                // but zone(1,1) and LastRenderTime still initialize. No Pawn /
                // Decoration / Inventory FindBase branch applies to GameInfo.
                InitializeWorldZone(actor, level);
                Touch(actor); actor->lastRenderTime = -10.0f;
                if (QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, "bCollideActors"), 0u)) ||
                    QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, "bBlockActors"), 0u)) ||
                    QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, "bBlockPlayers"), 0u)))
                    throw std::runtime_error("Unsupported colliding direct-world GameInfo native CheckEncroachment contract");
            }
            return {Value::Text(Kind::Object,path),{}};
        }
        for (const auto* event : {"Spawned","PreBeginPlay","BeginPlay"})
            static_cast<void>(execution.CallEvent(path,event,true,{}));
        if (QuestVr::Vm::ToBool(Read(actor,PropertyNamed(actor,"bDeleteMe"),0u))) return none();
        if (worldStartupProbe_) {
            InitializeWorldZone(actor, level);
            Touch(actor); actor->lastRenderTime = -10.0f;
            static_cast<void>(execution.CallEvent(path, "PostBeginPlay", true, {}));
            if (QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, "bCollideActors"), 0u)) ||
                QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, "bBlockActors"), 0u)) ||
                QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, "bBlockPlayers"), 0u)))
                throw std::runtime_error("Unsupported original spawned CheckEncroachment during world startup probe");
            static_cast<void>(execution.CallEvent(path, "SetInitialState", true, {}));
            SpawnBase(actor, execution); // Original native base pass precedes PPP.
            static_cast<void>(execution.CallEvent(path, "PostPostBeginPlay", false, {}));
        } else {
            SpawnZone(actor,execution);
            for (const auto* event : {"PostBeginPlay","SetInitialState"}) static_cast<void>(execution.CallEvent(path,event,true,{}));
            static_cast<void>(execution.CallEvent(path,"PostPostBeginPlay",false,{}));
            SpawnBase(actor,execution);
        }
        if (!spawnNotificationLocked_) {
            struct Unlock { bool& flag; ~Unlock() { flag = false; } } unlock{spawnNotificationLocked_};
            spawnNotificationLocked_ = true;
            auto* notify = ActorLink(level,"SpawnNotify"); std::unordered_set<RuntimeObject*> visited;
            while (notify) {
                LifecycleStep();
                if (!IsDerivedFromPath(notify->cls,"Engine.SpawnNotify") || !visited.insert(notify).second)
                    throw std::runtime_error("SpawnNotify chain has invalid identity or a cycle");
                const auto matching = Read(notify,PropertyNamed(notify,"ActorClass"),0u);
                if (!matching.text.empty()) {
                    if (!actor) throw std::runtime_error("SpawnNotify continues after a null replacement");
                    auto* matchingClass = Object(matching.text);
                    if (!matchingClass->classDescriptor) throw std::runtime_error("SpawnNotify ActorClass is not an actual UClass");
                    DefaultClass(matchingClass);
                    const auto shortName = matchingClass->reflection.objectPath.substr(matchingClass->reflection.objectPath.find_last_of('.')+1u);
                    bool isA{};
                    for (auto* candidate=actor->cls;candidate;candidate=candidate->base)
                        if (LowerAscii(candidate->reflection.objectPath.substr(candidate->reflection.objectPath.find_last_of('.')+1u))==LowerAscii(shortName)) isA=true;
                    if (isA) {
                        const auto replacement = execution.CallEvent(notify->reflection.objectPath,"SpawnNotification",true,
                            {{Value::Text(Kind::Object,actor->reflection.objectPath),{}}});
                        if (replacement.kind != Kind::Object && replacement.kind != Kind::Nothing)
                            throw std::runtime_error("SpawnNotification return is not an Object");
                        actor = replacement.text.empty() ? nullptr : Object(replacement.text);
                        if (actor && !IsDerivedFromPath(actor->cls,"Engine.GameInfo"))
                            throw std::runtime_error("Pinned SpawnNotification replacement is not GameInfo");
                    }
                }
                notify=ActorLink(notify,"Next");
            }
        }
        return {Value::Text(Kind::Object,actor ? actor->reflection.objectPath : std::string{}),{}};
    }
    void LifecycleStep() {
        if (++lifecycleOperations_ > 65'536u)
            throw std::runtime_error("Native actor lifecycle work budget exceeded");
    }
    void RequireMapActor(RuntimeObject* actor) {
        if (!actor || actor->defaultOwner || !IsDerivedFromPath(actor->cls, "Engine.Actor") ||
            LowerAscii(std::filesystem::path(actor->sourcePath).extension().string()) != ".dx" ||
            PackageStem(actor->sourcePath) != persistentMapPackageName)
            throw std::runtime_error("Native lifecycle identity is not an actor in the current map");
    }
    RuntimeObject* ActorLink(RuntimeObject* actor, const char* name, const std::uint32_t slot = 0u) {
        const auto property = PropertyNamed(actor, name);
        if (property.zero.kind != Kind::Object || slot >= property.arrayDimension)
            throw std::runtime_error("Actor relationship property schema is invalid");
        const auto value = Read(actor, property, slot);
        if (value.kind != Kind::Object) throw std::runtime_error("Actor relationship value is not an Object");
        if (value.text.empty()) return nullptr;
        auto* target = Object(value.text); RequireMapActor(target); return target;
    }
    void WriteActorLink(RuntimeObject* actor, const char* name, RuntimeObject* target,
        const std::uint32_t slot = 0u) {
        ReferenceFor(actor, PropertyNamed(actor, name), slot, false)->write(
            Value::Text(Kind::Object, target ? target->reflection.objectPath : std::string{}));
    }
    void TouchLifecycle(RuntimeObject* actor) {
        Touch(actor); actor->committedLifecycle = true; actor->committedScriptState = true;
    }
    void RemoveFirst(std::vector<RuntimeObject*>& actors, RuntimeObject* actor) {
        LifecycleStep();
        const auto found = std::find(actors.begin(), actors.end(), actor);
        if (found != actors.end()) actors.erase(found);
    }
    void StandingCount(RuntimeObject* base) {
        const auto property = PropertyNamed(base, "StandingCount");
        if (property.zero.kind != Kind::Byte || property.arrayDimension != 1u)
            throw std::runtime_error("StandingCount is not an authored scalar byte");
        ReferenceFor(base, property, 0u, false)->write(
            Value::Byte(static_cast<std::uint8_t>(std::min<std::size_t>(base->basedActors.size(), 255u))));
    }
    void RelationshipEvent(QuestVr::Vm::Execution& execution, RuntimeObject* target,
        const char* name, RuntimeObject* actor) {
        LifecycleStep();
        static_cast<void>(execution.CallEvent(target->reflection.objectPath, name, true,
            {{Value::Text(Kind::Object, actor->reflection.objectPath), {}}}));
    }
    void SetOwner(RuntimeObject* actor, RuntimeObject* newOwner, QuestVr::Vm::Execution& execution) {
        LifecycleStep(); RequireMapActor(actor);
        if (newOwner) RequireMapActor(newOwner);
        if (auto* owner = ActorLink(actor, "Owner")) {
            RelationshipEvent(execution, owner, "LostChild", actor);
            // The original native deliberately rereads Owner after script.
            // A callback that nulls it would dereference null in the pin. Fail
            // the complete transaction instead of inventing snapshot semantics.
            owner = ActorLink(actor, "Owner");
            if (!owner) throw std::runtime_error("LostChild cleared Owner during native SetOwner");
            TouchLifecycle(owner); RemoveFirst(owner->childActors, actor);
        }
        WriteActorLink(actor, "Owner", newOwner);
        if (auto* owner = ActorLink(actor, "Owner")) {
            RelationshipEvent(execution, owner, "GainedChild", actor);
            owner = ActorLink(actor, "Owner");
            if (!owner) throw std::runtime_error("GainedChild cleared Owner during native SetOwner");
            TouchLifecycle(owner);
            if (owner->childActors.size() >= 65'536u) throw std::runtime_error("Native child count budget exceeded");
            owner->childActors.push_back(actor);
        }
    }
    void SetBase(RuntimeObject* actor, RuntimeObject* newBase, const bool sendEvent,
        QuestVr::Vm::Execution& execution) {
        LifecycleStep(); RequireMapActor(actor);
        if (newBase) RequireMapActor(newBase);
        if (ActorLink(actor, "Base") == newBase) return;
        std::unordered_set<RuntimeObject*> visited;
        for (auto* current = newBase; current; current = ActorLink(current, "Base")) {
            LifecycleStep();
            if (current == actor) return; // Pinned cycle refusal has no callbacks/writes.
            if (visited.size() >= 4096u || !visited.insert(current).second)
                throw std::runtime_error("Prospective Base hierarchy is cyclic or too deep");
        }
        auto* level = ActorLink(actor, "Level");
        if (!level || !IsDerivedFromPath(level->cls, "Engine.LevelInfo"))
            throw std::runtime_error("SetBase has no authored LevelInfo binding");
        if (auto* base = ActorLink(actor, "Base"); base && base != level) {
            TouchLifecycle(base); RemoveFirst(base->basedActors, actor); StandingCount(base);
            RelationshipEvent(execution, base, "Detach", actor);
        }
        WriteActorLink(actor, "Base", newBase);
        if (auto* base = ActorLink(actor, "Base"); base && base != level) {
            TouchLifecycle(base);
            if (base->basedActors.size() >= 65'536u) throw std::runtime_error("Native based actor count budget exceeded");
            base->basedActors.push_back(actor); StandingCount(base);
            RelationshipEvent(execution, base, "Attach", actor);
        }
        if (sendEvent) {
            LifecycleStep(); static_cast<void>(execution.CallEvent(actor->reflection.objectPath, "BaseChange", true, {}));
        }
    }
    void UnTouch(RuntimeObject* actor, RuntimeObject* other, QuestVr::Vm::Execution& execution) {
        RequireMapActor(other);
        const auto unlink = [&](RuntimeObject* side, RuntimeObject* peer) {
            if (QuestVr::Vm::ToBool(Read(side, PropertyNamed(side, "bDeleteMe"), 0u))) return;
            const auto property = PropertyNamed(side, "Touching");
            if (property.arrayDimension != 4u || property.zero.kind != Kind::Object)
                throw std::runtime_error("GOTY Touching is not an authored four-slot actor array");
            for (std::uint32_t slot = 0u; slot < 4u; ++slot) {
                LifecycleStep();
                if (ActorLink(side, "Touching", slot) != peer) continue;
                WriteActorLink(side, "Touching", nullptr, slot);
                if (side->touchEventSent[slot]) {
                    TouchLifecycle(side); side->touchEventSent[slot] = false;
                    RelationshipEvent(execution, side, "UnTouch", peer);
                }
            }
        };
        unlink(actor, other); unlink(other, actor);
    }
    Evaluation LifecycleNative(const std::uint16_t index, const std::string& receiver,
        const std::vector<Evaluation>& arguments, QuestVr::Vm::Execution& execution) {
        auto* actor = Object(receiver); RequireMapActor(actor); LifecycleStep();
        if (!actor->active && !actor->worldRemoved) throw std::runtime_error("Native lifecycle receiver is inactive");
        if (index != 279u) {
            if (arguments.size() != 1u) throw std::runtime_error("SetOwner/SetBase requires one Actor or None");
            const auto value = arguments[0].Load();
            if (value.kind != Kind::Object) throw std::runtime_error("SetOwner/SetBase argument is not an Object");
            auto* target = value.text.empty() ? nullptr : Object(value.text);
            if (target) RequireMapActor(target);
            if (index == 272u) SetOwner(actor, target, execution);
            else SetBase(actor, target, true, execution);
            return {};
        }
        if (!arguments.empty()) throw std::runtime_error("Destroy takes no arguments");
        const auto flag = [&](const char* name) { return QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, name), 0u)); };
        if (flag("bStatic") || flag("bNoDelete")) return {Value::Bool(false), {}};
        if (flag("bDeleteMe")) return {Value::Bool(true), {}};
        ReferenceFor(actor, PropertyNamed(actor, "bDeleteMe"), 0u, false)->write(Value::Bool(true));
        SetBase(actor, nullptr, true, execution);
        TouchCollision(); persistentActorCollision.Remove(reinterpret_cast<std::uintptr_t>(actor));
        static_cast<void>(execution.CallEvent(receiver, "Destroyed", true, {}));
        const auto touching = PropertyNamed(actor, "Touching");
        if (touching.arrayDimension != 4u || touching.zero.kind != Kind::Object)
            throw std::runtime_error("GOTY Touching is not an authored four-slot actor array");
        for (std::uint32_t slot = 0u; slot < 4u; ++slot)
            if (auto* other = ActorLink(actor, "Touching", slot)) UnTouch(actor, other, execution);
        SetOwner(actor, nullptr, execution);
        while (!actor->childActors.empty()) {
            LifecycleStep(); SetOwner(actor->childActors.back(), nullptr, execution);
        }
        while (!actor->basedActors.empty()) {
            LifecycleStep(); SetBase(actor->basedActors.back(), nullptr, true, execution);
        }
        if (actor->worldActorIndex < 0) throw std::runtime_error("Actor index was never set");
        TouchLifecycle(actor); actor->worldRemoved = true;
        return {Value::Bool(true), {}};
    }
    struct Before {
        decltype(RuntimeObject::scriptValues) values;
        decltype(RuntimeObject::classDefaultValues) defaults;
        std::optional<QuestVr::ActorAnimationClock> clock;
        std::optional<QuestVr::StateObject> state;
        std::optional<float> latentTimeLeft;
        std::optional<float> lastRenderTime;
        std::uint64_t revision{};
        std::uint32_t transitionFlags{};
        bool committed{};
        std::vector<RuntimeObject*> children, based;
        std::array<bool, 4u> touchSent{};
        bool lifecycle{}, removed{};
        RuntimeObject* player{};
        RuntimeObject* viewportActor{};
        bool playerBinding{};
        RuntimeObject* rootWindow{};
        std::int32_t uniqueCounter{};
        decltype(RuntimeObject::nativeWindowPropertyRoots) windowPropertyRoots;
        std::optional<RuntimeNativeWindowState> windowState;
    };
    template <typename Number> static bool SameBits(const Number& a,const Number& b) {
        return std::memcmp(&a,&b,sizeof(Number))==0;
    }
    static bool SameStoredValue(const Value& a,const Value& b) {
        // Publication classification is storage identity, not UnrealScript
        // equality: preserve signed zero and exact Name/Object spelling too.
        if (a.kind!=b.kind || a.integer!=b.integer || !SameBits(a.floating,b.floating) ||
            a.boolean!=b.boolean || a.text!=b.text || a.rotation!=b.rotation ||
            a.intArray!=b.intArray || a.fields.size()!=b.fields.size()) return false;
        for (std::size_t i=0u;i<a.vector.size();++i) if (!SameBits(a.vector[i],b.vector[i])) return false;
        for (const auto& [key,value]:a.fields) {
            const auto found=b.fields.find(key);
            if (found==b.fields.end() || !SameStoredValue(value,found->second)) return false;
        }
        return true;
    }
    static bool TimerOverlayName(const std::string& name) {
        return name=="timerrate" || name=="timercounter" || name=="btimerloop";
    }
    static bool SameStoredProperties(const decltype(RuntimeObject::scriptValues)& a,
        const decltype(RuntimeObject::scriptValues)& b,const bool excludeTimer) {
        for (const auto& [name,slots]:a) {
            if (excludeTimer && TimerOverlayName(name)) continue;
            const auto found=b.find(name);
            if (found==b.end() || slots.size()!=found->second.size()) return false;
            for (const auto& [index,value]:slots) {
                const auto other=found->second.find(index);
                if (other==found->second.end() || !SameStoredValue(value,other->second)) return false;
            }
        }
        for (const auto& [name,slots]:b) {
            static_cast<void>(slots);
            if (!(excludeTimer && TimerOverlayName(name)) && a.find(name)==a.end()) return false;
        }
        return true;
    }
    static bool SameOptionalFloat(const std::optional<float>& a,const std::optional<float>& b) {
        return a.has_value()==b.has_value() && (!a || SameBits(*a,*b));
    }
    static bool SameStoredClock(const std::optional<QuestVr::ActorAnimationClock>& a,
        const std::optional<QuestVr::ActorAnimationClock>& b) {
        if (a.has_value()!=b.has_value()) return false;
        if (!a) return true;
        const auto channel=[](const QuestVr::MeshAnimationChannel& x,const QuestVr::MeshAnimationChannel& y) {
            return x.sequence==y.sequence && SameBits(x.normalizedFrame,y.normalizedFrame) &&
                x.previous.vertexOffset0==y.previous.vertexOffset0 && x.previous.vertexOffset1==y.previous.vertexOffset1 &&
                SameBits(x.previous.fraction,y.previous.fraction);
        };
        const auto rates=[](const auto& x,const auto& y) {
            return SameBits(x.rate,y.rate) && SameBits(x.last,y.last) && SameBits(x.minRate,y.minRate) &&
                SameBits(x.tweenRate,y.tweenRate) && SameBits(x.oldRate,y.oldRate);
        };
        if (!channel(a->pose.main,b->pose.main) || !rates(a->main,b->main) ||
            a->main.loop!=b->main.loop || a->main.notify!=b->main.notify || a->main.finished!=b->main.finished ||
            a->main.finishAnimWaiting!=b->main.finishAnimWaiting || !SameBits(a->simulationTime,b->simulationTime) ||
            a->remoteRole!=b->remoteRole || a->pose.fatness!=b->pose.fatness) return false;
        for (std::size_t i=0u;i<a->blends.size();++i) {
            if (!channel(a->pose.blends[i],b->pose.blends[i]) || !rates(a->blends[i],b->blends[i])) return false;
            for (std::size_t j=0u;j<a->blends[i].simulated.size();++j)
                if (!SameBits(a->blends[i].simulated[j],b->blends[i].simulated[j])) return false;
        }
        return true;
    }
    static bool SameStoredState(const std::optional<QuestVr::StateObject>& a,
        const std::optional<QuestVr::StateObject>& b) {
        if (a.has_value()!=b.has_value()) return false;
        if (!a) return true;
        if (a->hasStack!=b->hasStack || a->frameOverride!=b->frameOverride || a->disabled!=b->disabled ||
            a->frame.has_value()!=b->frame.has_value()) return false;
        if (!a->frame) return true;
        const auto& x=*a->frame; const auto& y=*b->frame;
        if (x.codePath!=y.codePath || x.localsCodePath!=y.localsCodePath || x.statementIndex!=y.statementIndex ||
            x.latent!=y.latent || x.locals.size()!=y.locals.size() || x.control.has_value()!=y.control.has_value()) return false;
        if (x.control && (x.control->selectedPath!=y.control->selectedPath ||
            x.control->codePresent!=y.control->codePresent || x.control->liveProbeMask!=y.control->liveProbeMask)) return false;
        for (std::size_t i=0u;i<x.locals.size();++i) {
            if (x.locals[i].key!=y.locals[i].key || x.locals[i].values.size()!=y.locals[i].values.size()) return false;
            for (std::size_t j=0u;j<x.locals[i].values.size();++j)
                if (!SameStoredValue(x.locals[i].values[j],y.locals[i].values[j])) return false;
        }
        return true;
    }
    bool OnlyActorTimerChanged(RuntimeObject* actor,const Before& before) {
        bool hasTimer{};
        const std::array<const decltype(RuntimeObject::scriptValues)*,2u> timerStates{
            &before.values,&actor->scriptValues};
        for (const auto* properties:timerStates)
            for (const auto& [name,slots]:*properties) {
                if (!TimerOverlayName(name)) continue;
                hasTimer=true;
                if (slots.size()!=1u || slots.begin()->first!=0u) return false;
                const auto& value=slots.begin()->second;
                if (name=="btimerloop" ? value.kind!=Kind::Bool :
                    value.kind!=Kind::Float || !std::isfinite(value.floating)) return false;
            }
        if (!hasTimer || !actor->committedScriptState ||
            !SameStoredProperties(before.values,actor->scriptValues,true) ||
            !SameStoredProperties(before.defaults,actor->classDefaultValues,false) ||
            !SameStoredClock(before.clock,actor->animationClock) || !SameStoredState(before.state,actor->stateObject) ||
            !SameOptionalFloat(before.latentTimeLeft,actor->latentTimeLeft) ||
            !SameOptionalFloat(before.lastRenderTime,actor->lastRenderTime) ||
            before.revision!=actor->stateLocalsRevision || before.transitionFlags!=actor->stateTransitionFlags ||
            before.children!=actor->childActors || before.based!=actor->basedActors || before.touchSent!=actor->touchEventSent ||
            before.lifecycle!=actor->committedLifecycle || before.removed!=actor->worldRemoved) return false;
        // No optimistic general simulation-property whitelist: only the exact
        // three original fields, plus their persistent committed-state marker,
        // may bypass expensive geometry/tag publication. All other deltas keep
        // the previous conservative full Actor publication behavior.
        static_cast<void>(ActorTimerProperties(actor));
        return true;
    }
    struct BeforeAI {
        GCRoot<RuntimeAIManager> manager;
        std::optional<QuestVr::Ai::State> state;
        std::vector<RuntimeObject*> actorRoots;
    };
    struct BeforeDecals {
        GCRoot<RuntimeMapDecals> graph;
        std::optional<QuestVr::DecalState> state;
        std::vector<RuntimeObject*> actorRoots;
    };
    std::unordered_map<std::string, PortablePackageTables> tables_;
    std::unordered_map<std::string, std::shared_ptr<const Function>> functions_;
    std::unordered_map<std::string, std::shared_ptr<const QuestVr::AuthoredStructSchema>> structSchemas_;
    std::set<RuntimeObject*> buildingStructs_;
    std::set<RuntimeObject*> chargedStructDescriptors_;
    RuntimeObject* validatedCoreColorLayout_{};
    std::size_t structFields_{};
    std::size_t structSchemaBytes_{};
    std::unordered_map<RuntimeObject*, Before> saved_;
    std::unordered_map<RuntimeObject*, BeforeAI> savedAi_;
    std::vector<GCRoot<RuntimeAIManager>> provisionalAi_;
    std::vector<GCRoot<RuntimeObject>> provisionalWindows_;
    std::optional<BeforeDecals> beforeDecals_;
    bool clientDependent_{};
    std::optional<QuestVr::ActorCollisionRegistry> beforeCollision_;
    std::size_t lifecycleOperations_{};
    std::size_t beforeExportCount_{}, beforeWorldSlots_{};
    std::size_t beforeWindowObjects_{}, beforeWindowRoots_{};
    std::optional<std::uint32_t> beforeRandomSeed_;
    bool spawnNotificationLocked_{};
    bool worldStartupProbe_{};
    std::optional<PortableModelGeometry> rootModel_;
    bool transaction_{};
    const PortablePackageTables& Table(const std::string& source) {
        auto found = tables_.find(source);
        if (found == tables_.end()) {
            if (tables_.size() >= 64u || std::filesystem::file_size(source) > 512u * 1024u * 1024u)
                throw std::runtime_error("VM source package table budget exceeded");
            found = tables_.emplace(source, LoadPortablePackageTables(source)).first;
        }
        return found->second;
    }
    std::string Qualified(const PortablePackageTables& table, const std::int32_t reference) {
        if (reference == 0) return {};
        // GetPortableObjectPath is a display helper. Validate the complete
        // authored outer chain before using its result as a schema/value key.
        std::set<std::int32_t> visited;
        for (auto current = reference; current != 0;) {
            if (visited.size() >= 32u || !visited.insert(current).second)
                throw std::runtime_error("Runtime property object outer chain cycles or exceeds budget");
            std::int32_t name{}, outer{};
            if (current > 0) {
                if (static_cast<std::size_t>(current) > table.exports.size())
                    throw std::runtime_error("Runtime property export reference is out of range");
                const auto& entry = table.exports[static_cast<std::size_t>(current - 1)];
                name = entry.ObjName; outer = entry.ObjOuter;
            } else {
                const auto index = static_cast<std::uint64_t>(-static_cast<std::int64_t>(current) - 1);
                if (index >= table.imports.size())
                    throw std::runtime_error("Runtime property import reference is out of range");
                const auto& entry = table.imports[static_cast<std::size_t>(index)];
                name = entry.ObjName; outer = entry.ObjOuter;
            }
            if (name < 0 || static_cast<std::size_t>(name) >= table.names.size() ||
                table.names[static_cast<std::size_t>(name)].Name.IsNone())
                throw std::runtime_error("Runtime property object name is invalid");
            current = outer;
        }
        const auto path = GetPortableObjectPath(table, reference);
        if (path.empty()) throw std::runtime_error("Runtime property object reference unavailable");
        return reference > 0 ? PackageStem(table.sourcePath) + '.' + path : path;
    }
    const PortableStructDescriptor& StructDescriptor(RuntimeObject* object) {
        if (object->reflection.metaClass != "Struct")
            throw std::runtime_error("Runtime struct schema reference is not Core.Struct");
        constexpr std::size_t maxBytes = 64u * 1024u * 1024u;
        auto& runtimeBytes = persistentRuntime->get()->structDescriptorBytes;
        if (!object->structDescriptor) {
            if (runtimeBytes > maxBytes || structSchemaBytes_ > maxBytes)
                throw std::runtime_error("Runtime retained struct descriptor budget exceeded");
            auto descriptor = LoadPortableStructDescriptor(Table(object->sourcePath), object->exportIndex,
                std::min(maxBytes - runtimeBytes, maxBytes - structSchemaBytes_));
            const auto bytes = PortableStructRetainedBytes(descriptor);
            auto retained = std::make_unique<PortableStructDescriptor>(std::move(descriptor));
            object->structDescriptor = std::move(retained);
            runtimeBytes += bytes;
        }
        if (chargedStructDescriptors_.find(object) == chargedStructDescriptors_.end()) {
            const auto bytes = PortableStructRetainedBytes(*object->structDescriptor);
            if (bytes > maxBytes || structSchemaBytes_ > maxBytes - bytes)
                throw std::runtime_error("Runtime aggregate struct schema/descriptor byte budget exceeded");
            chargedStructDescriptors_.insert(object);
            structSchemaBytes_ += bytes;
        }
        return *object->structDescriptor;
    }
    std::shared_ptr<const QuestVr::AuthoredStructSchema> StructSchema(RuntimeObject* object) {
        if (buildingStructs_.find(object) != buildingStructs_.end())
            throw std::runtime_error("Runtime authored struct schema is recursive");
        const auto key = LowerAscii(object->reflection.objectPath);
        const auto found = structSchemas_.find(key);
        if (found != structSchemas_.end()) return found->second;
        if (buildingStructs_.size() >= 32u || structSchemas_.size() >= 4096u)
            throw std::runtime_error("Runtime authored struct schema depth/count budget exceeded");
        buildingStructs_.insert(object);
        struct Guard {
            std::set<RuntimeObject*>& active; RuntimeObject* object;
            ~Guard() { active.erase(object); }
        } guard{buildingStructs_, object};
        auto schema = std::make_shared<QuestVr::AuthoredStructSchema>();
        schema->path = object->reflection.objectPath;
        const auto& declaration = Table(object->sourcePath);
        const auto nameIndex = declaration.exports.at(object->exportIndex).ObjName;
        if (nameIndex < 0 || static_cast<std::size_t>(nameIndex) >= declaration.names.size())
            throw std::runtime_error("Runtime struct declaration Name is out of range");
        schema->name = declaration.names[static_cast<std::size_t>(nameIndex)].Name.ToString();
        if (key == "core.object.vector") schema->kind = Kind::Vector;
        else if (key == "core.object.rotator") schema->kind = Kind::Rotator;
        const auto& descriptor = StructDescriptor(object);
        if (descriptor.baseField)
            schema->fields = StructSchema(Object(Qualified(Table(object->sourcePath), descriptor.baseField)))->fields;
        std::set<std::string> names;
        for (const auto& field : schema->fields) names.insert(LowerAscii(field.name));
        std::set<RuntimeObject*> visited;
        auto child = descriptor.children;
        while (child) {
            auto* field = Object(Qualified(Table(object->sourcePath), child));
            if (visited.size() >= 8192u || !visited.insert(field).second || field->outer != object)
                throw std::runtime_error("Runtime struct Children/Next ownership/cycle budget failed");
            if (field->property) {
                const auto& member = *field->property;
                if (member.arrayDimension != 1 || member.type == "StringProperty")
                    throw std::runtime_error("Unsupported authored struct array/string member " + field->reflection.objectPath);
                if (structFields_ >= 32768u) throw std::runtime_error("Runtime struct field budget exceeded");
                ++structFields_;
                const auto dot = field->reflection.objectPath.find_last_of('.');
                QuestVr::AuthoredStructField value;
                value.key = field->reflection.objectPath;
                value.name = value.key.substr(dot + 1u);
                if (!names.insert(LowerAscii(value.name)).second)
                    throw std::runtime_error("Runtime struct has duplicate canonical field names");
                if (member.type == "StructProperty") {
                    value.nested = StructSchema(Object(Qualified(Table(field->sourcePath), member.referencedType)));
                    value.zero.kind = value.nested->kind;
                } else value.zero = Zero(field);
                if (member.type == "ObjectProperty" || member.type == "ClassProperty") {
                    value.classReference = member.type == "ClassProperty";
                    value.referenceClassPath = Qualified(Table(field->sourcePath), value.classReference ?
                        member.secondaryType : member.referencedType);
                }
                schema->fields.push_back(std::move(value)); child = member.nextField;
            } else if (field->script) child = field->script->nextField;
            else {
                if (!field->commonFieldLinks)
                    field->commonFieldLinks = LoadPortableFieldLinks(Table(field->sourcePath), field->exportIndex);
                child = field->commonFieldLinks->nextField;
            }
        }
        // Validate aggregate shape/budget before any runtime value construction.
        QuestVr::AuthoredStructLimits limits;
        QuestVr::AuthoredStructDetail::Budget measured{limits};
        std::vector<const QuestVr::AuthoredStructSchema*> stack;
        QuestVr::AuthoredStructDetail::Validate(*schema, measured, 0u, stack);
        constexpr std::size_t maxSchemaBytes = 64u * 1024u * 1024u;
        if (measured.retained > maxSchemaBytes || structSchemaBytes_ > maxSchemaBytes - measured.retained)
            throw std::runtime_error("Runtime aggregate struct schema byte budget exceeded");
        structSchemaBytes_ += measured.retained;
        structSchemas_.emplace(key, schema); return schema;
    }
    Value Zero(RuntimeObject* property) {
        const auto& descriptor = *property->property;
        const auto& type = descriptor.type;
        if (type == "ByteProperty") return Value::Byte(0u);
        if (type == "IntProperty") return Value::Integer(0);
        if (type == "BoolProperty") return Value::Bool(false);
        if (type == "FloatProperty") return Value::Float(0.0f);
        if (type == "NameProperty") return Value::Text(Kind::Name, "None");
        if (type == "StrProperty" || type == "StringProperty") return Value::Text(Kind::String, {});
        if (type == "ObjectProperty" || type == "ClassProperty") return Value::Text(Kind::Object, {});
        if (type == "ArrayProperty") {
            // A dynamic array's inner is an actual UProperty reference. The
            // fixed ArrayDim is NOT its length. Only the audited native INT
            // payload is supported; no generic element opcode is enabled.
            auto* inner = Object(Qualified(Table(property->sourcePath), descriptor.referencedType));
            const auto& arrayTable = Table(property->sourcePath);
            const auto& innerTable = Table(inner->sourcePath);
            if (descriptor.arrayDimension != 1 || !inner->property ||
                inner->property->type != "IntProperty" || inner->property->arrayDimension != 1 ||
                inner->outer != property ||
                LowerAscii(Qualified(arrayTable, arrayTable.exports.at(property->exportIndex).ObjClass)) != "core.arrayproperty" ||
                LowerAscii(Qualified(innerTable, innerTable.exports.at(inner->exportIndex).ObjClass)) != "core.intproperty")
                throw std::runtime_error("Unsupported runtime dynamic array inner schema");
            return Value::IntArray({});
        }
        if (type == "StructProperty") {
            return QuestVr::MakeAuthoredStructZero(*StructPropertySchema(property->reflection.objectPath));
        }
        throw std::runtime_error("Unsupported runtime property type " + type);
    }
    QuestVr::Vm::ResultLayout StructResultLayout(RuntimeObject* property) {
        using Layout = QuestVr::Vm::ResultLayout;
        const auto& member = *property->property;
        if (member.type != "StructProperty") return Layout::Unknown;
        auto* structure = Object(Qualified(Table(property->sourcePath), member.referencedType));
        if (LowerAscii(structure->reflection.objectPath) != "core.object.color") return Layout::Unknown;
        const auto& propertyTable = Table(property->sourcePath);
        if (LowerAscii(Qualified(propertyTable, propertyTable.exports.at(property->exportIndex).ObjClass)) != "core.structproperty")
            throw std::runtime_error("Canonical Color result requires an actual Core.StructProperty");
        if (validatedCoreColorLayout_ == structure) return Layout::CoreColor4;
        const auto& descriptor = StructDescriptor(structure);
        if (!structure->outer || LowerAscii(structure->outer->reflection.objectPath) != "core.object" ||
            descriptor.baseField || descriptor.logicalSize || !descriptor.rawBytes.empty() || !descriptor.bytecode.empty())
            throw std::runtime_error("Canonical Core.Object.Color result layout has an invalid owner/base/script");
        // Retail UStruct::Link and UByteProperty::Link place these four owned
        // fields at R0/G1/B2/A3, with total size4 and no padding. Authored value
        // shape alone is never authority for an arbitrary struct's native ABI.
        const auto schema = StructSchema(structure);
        if (schema->kind != Kind::Struct || schema->fields.size() != 4u)
            throw std::runtime_error("Canonical Core.Object.Color result schema is not four channels");
        static constexpr const char* channels[] = {"r", "g", "b", "a"};
        auto child = descriptor.children;
        for (std::size_t i = 0u; i < 4u; ++i) {
            if (!child) throw std::runtime_error("Canonical Core.Object.Color result channel chain is truncated");
            auto* field = Object(Qualified(Table(structure->sourcePath), child));
            const auto& fieldTable = Table(field->sourcePath);
            if (field->outer != structure || !field->property || field->property->type != "ByteProperty" ||
                field->property->arrayDimension != 1 || field->property->referencedType ||
                LowerAscii(Qualified(fieldTable, fieldTable.exports.at(field->exportIndex).ObjClass)) != "core.byteproperty" ||
                LowerAscii(field->reflection.objectPath) != std::string("core.object.color.") + channels[i] ||
                LowerAscii(schema->fields[i].key) != LowerAscii(field->reflection.objectPath) ||
                schema->fields[i].zero.kind != Kind::Byte || schema->fields[i].nested)
                throw std::runtime_error("Canonical Core.Object.Color result channel declaration is invalid");
            child = field->property->nextField;
        }
        if (child) throw std::runtime_error("Canonical Core.Object.Color result has extra native fields");
        validatedCoreColorLayout_ = structure;
        return Layout::CoreColor4;
    }
    Property Describe(RuntimeObject* property) {
        if (!property->property) throw std::runtime_error("VM object has no property descriptor");
        const auto& descriptor = *property->property;
        const auto dot = property->reflection.objectPath.find_last_of('.');
        Property result{property->reflection.objectPath, property->reflection.objectPath.substr(dot + 1u),
            Zero(property), descriptor.flags, static_cast<std::size_t>(descriptor.arrayDimension)};
        result.resultLayout = StructResultLayout(property);
        return result;
    }
    std::shared_ptr<const Function> FunctionIdentity(RuntimeObject* object) {
        if (!object->script) throw std::runtime_error("VM object is not a compiled function");
        auto identity = std::make_shared<Function>();
        identity->path = object->reflection.objectPath; identity->source = object->sourcePath;
        // Lazy native &&/|| argument handling is decided before Frame.Call;
        // identity resolution must retain these immutable declaration flags.
        identity->nativeIndex = object->script->nativeIndex; identity->flags = object->script->functionFlags;
        return identity;
    }
    std::shared_ptr<const Function> FunctionFor(RuntimeObject* object) {
        if (!object->script) throw std::runtime_error("VM object is not a compiled function");
        const auto cached = functions_.find(object->reflection.objectPath);
        if (cached != functions_.end()) return cached->second;
        auto function = std::make_shared<Function>();
        function->path = object->reflection.objectPath; function->source = object->sourcePath;
        function->bytecode = object->script->bytecode;
        function->nativeIndex = object->script->nativeIndex; function->flags = object->script->functionFlags;
        const auto& table = Table(object->sourcePath);
        std::set<std::int32_t> visited;
        auto child = object->script->children;
        while (child != 0) {
            if (visited.size() >= 4096u || !visited.insert(child).second)
                throw std::runtime_error("VM function property chain cycles or exceeds budget");
            RuntimeObject* field = Object(Qualified(table, child));
            if (!field->property) throw std::runtime_error("Unsupported non-property function local metadata");
            function->variables.push_back(Describe(field)); child = field->property->nextField;
        }
        functions_.emplace(function->path, function); return function;
    }
    Value Decode(RuntimeObject& source, const PortableTaggedProperty& tag, const Property& property) {
        const auto& zero = property.zero;
        if (zero.kind == Kind::IntArray) {
            if (tag.type != 9u) throw std::runtime_error("Authored INT array tag has the wrong type");
            return Value::IntArray(QuestVr::DecodeAuthoredIntArray(tag.value));
        }
        if (zero.kind == Kind::Struct || zero.kind == Kind::Vector || zero.kind == Kind::Rotator) {
            const auto schema = StructPropertySchema(property.key);
            if (tag.type != 10u)
                throw std::runtime_error("Authored struct tag does not match its declared type");
            QuestVr::ValidateAuthoredStructTag(*schema, tag.structName.ToString());
            const auto& table = Table(source.sourcePath);
            QuestVr::AuthoredStructResolvers resolvers;
            resolvers.resolveName = [&table](const std::int32_t index) {
                if (index < 0 || static_cast<std::size_t>(index) >= table.names.size())
                    throw std::runtime_error("Authored struct name reference is out of range");
                return table.names[static_cast<std::size_t>(index)].Name.ToString();
            };
            resolvers.resolveObject = [this, &table](const std::int32_t index, const QuestVr::AuthoredStructField& field) {
                const auto path = ResolvePortableValueObjectReference(table, index,
                    [this](const std::string& resolved, const std::string& importedClass) {
                        auto* object = Object(resolved);
                        auto actual = object->cls ? object->cls->reflection.objectPath :
                            object->reflection.metaClass == "Class" ? std::string("Core.Class") :
                            Qualified(Table(object->sourcePath), Table(object->sourcePath).exports.at(object->exportIndex).ObjClass);
                        const auto wanted = LowerAscii(importedClass);
                        const auto shortName = [](const std::string& name) {
                            return LowerAscii(name.substr(name.find_last_of('.') + 1u));
                        };
                        // The pin's Class lookup requires an actual class
                        // metaclass, not any descendant of Core.Class. Other
                        // imports match an unqualified name anywhere in ancestry.
                        if (wanted == "class") return shortName(actual) == "class";
                        std::set<std::string> visited;
                        for (std::size_t depth = 0u; depth < 128u && !actual.empty(); ++depth) {
                            const auto key = LowerAscii(actual);
                            if (!visited.insert(key).second)
                                throw std::runtime_error("Runtime imported value class hierarchy cycles");
                            if (shortName(actual) == wanted) return true;
                            const auto found = persistentVmObjects.find(key);
                            if (found != persistentVmObjects.end() && found->second->reflection.metaClass == "Class") {
                                auto* cls = found->second;
                                actual = cls->base ? cls->base->reflection.objectPath :
                                    Qualified(Table(cls->sourcePath), Table(cls->sourcePath).exports.at(cls->exportIndex).ObjBase);
                            } else {
                                // Same exact native registrations as ClassDerives.
                                static constexpr std::pair<const char*, const char*> parents[] = {
                                    {"engine.lodmesh", "Engine.Mesh"}, {"engine.mesh", "Engine.Primitive"},
                                    {"engine.model", "Engine.Primitive"}, {"engine.primitive", "Core.Object"},
                                    {"engine.texture", "Engine.Bitmap"}, {"engine.bitmap", "Core.Object"},
                                    {"core.class", "Core.State"}, {"core.state", "Core.Struct"},
                                    {"core.struct", "Core.Field"}, {"core.field", "Core.Object"}
                                };
                                const auto parent = std::find_if(std::begin(parents), std::end(parents),
                                    [&](const auto& item) { return key == item.first; });
                                if (parent == std::end(parents)) return false;
                                actual = parent->second;
                            }
                        }
                        if (!actual.empty()) throw std::runtime_error("Runtime imported value class hierarchy exceeds budget");
                        return false;
                    });
                ValidateObjectValue(Value::Text(Kind::Object, path), field.referenceClassPath, field.classReference);
                return path;
            };
            return QuestVr::DecodeAuthoredStructValue(*schema, tag.value, table.version, resolvers);
        }
        switch (zero.kind) {
            case Kind::Bool:
                if (tag.type == 3u) return Value::Bool(tag.boolValue);
                break;
            case Kind::Byte:
                if (tag.type == 1u && tag.value.size() == 1u) return Value::Byte(tag.value.front());
                break;
            case Kind::Int:
                if (tag.type == 2u && tag.value.size() == 4u) {
                    std::int32_t value{}; std::memcpy(&value, tag.value.data(), 4u); return Value::Integer(value);
                } break;
            case Kind::Float:
                if (tag.type == 4u && tag.value.size() == 4u) {
                    float value{}; std::memcpy(&value, tag.value.data(), 4u); return Value::Float(value);
                } break;
            case Kind::Name:
                if (tag.type == 6u) return Value::Text(Kind::Name, DecodePortableNameProperty(Table(source.sourcePath), tag));
                break;
            case Kind::Object:
                if (tag.type == 5u || tag.type == 8u) return Value::Text(Kind::Object,
                    Qualified(Table(source.sourcePath), DecodePortableObjectReference(tag)));
                break;
            case Kind::String:
                if (tag.type == 13u) return Value::Text(Kind::String, DecodePortableVmStringProperty(tag));
                break;
            default: break;
        }
        throw std::runtime_error("Unsupported/malformed authored property " + tag.name.ToString());
    }
    bool ClassDerives(std::string actual, const std::string& expected) {
        const auto target = LowerAscii(expected);
        std::set<std::string> visited;
        for (std::size_t depth = 0u; depth < 128u && !actual.empty(); ++depth) {
            const auto key = LowerAscii(actual);
            if (key == target) return true;
            if (!visited.insert(key).second) throw std::runtime_error("Runtime value class hierarchy cycles");
            const auto found = persistentVmObjects.find(key);
            if (found != persistentVmObjects.end() && found->second->reflection.metaClass == "Class") {
                auto* cls = found->second;
                actual = cls->base ? cls->base->reflection.objectPath :
                    Qualified(Table(cls->sourcePath), Table(cls->sourcePath).exports.at(cls->exportIndex).ObjBase);
            } else {
                // Exact pinned PackageManager registrations for native classes
                // absent from serialized UClass exports (same save constraints).
                static constexpr std::pair<const char*, const char*> parents[] = {
                    {"engine.lodmesh", "Engine.Mesh"}, {"engine.mesh", "Engine.Primitive"},
                    {"engine.model", "Engine.Primitive"}, {"engine.primitive", "Core.Object"},
                    {"engine.texture", "Engine.Bitmap"}, {"engine.bitmap", "Core.Object"},
                    {"core.class", "Core.State"}, {"core.state", "Core.Struct"},
                    {"core.struct", "Core.Field"}, {"core.field", "Core.Object"}
                };
                const auto parent = std::find_if(std::begin(parents), std::end(parents),
                    [&](const auto& item) { return key == item.first; });
                if (parent == std::end(parents)) return false;
                actual = parent->second;
            }
        }
        return false;
    }
    // Native UClasses absent from package exports are registered by the pin,
    // not fabricated actor instances. Only these independently audited Core
    // metadata and Engine asset registrations supply an absent class identity.
    // Serialized UClasses always take precedence and use their actual ObjBase.
    std::optional<QuestVr::ObjectCastClass> NativeCastClass(const std::string& path) {
        static constexpr std::pair<const char*, const char*> registered[] = {
            {"Core.Object", ""}, {"Core.Package", "Core.Object"},
            {"Core.Field", "Core.Object"}, {"Core.Const", "Core.Field"},
            {"Core.Enum", "Core.Field"}, {"Core.Struct", "Core.Field"},
            {"Core.Function", "Core.Struct"}, {"Core.State", "Core.Struct"},
            {"Core.Class", "Core.State"}, {"Core.Property", "Core.Field"},
            {"Core.PointerProperty", "Core.Property"}, {"Core.ByteProperty", "Core.Property"},
            {"Core.ObjectProperty", "Core.Property"}, {"Core.ClassProperty", "Core.ObjectProperty"},
            {"Core.FixedArrayProperty", "Core.Property"}, {"Core.ArrayProperty", "Core.Property"},
            {"Core.MapProperty", "Core.Property"}, {"Core.StructProperty", "Core.Property"},
            {"Core.IntProperty", "Core.Property"}, {"Core.BoolProperty", "Core.Property"},
            {"Core.FloatProperty", "Core.Property"}, {"Core.NameProperty", "Core.Property"},
            {"Core.StrProperty", "Core.Property"}, {"Core.StringProperty", "Core.Property"},
            {"Core.TextBuffer", "Core.Object"}, {"Core.Subsystem", "Core.Object"},
            {"Core.Language", "Core.Object"},
            {"Engine.Palette", "Core.Object"}, {"Engine.Sound", "Core.Object"},
            {"Engine.Music", "Core.Object"}, {"Engine.Primitive", "Core.Object"},
            {"Engine.Mesh", "Engine.Primitive"}, {"Engine.LodMesh", "Engine.Mesh"},
            {"Engine.SkeletalMesh", "Engine.LodMesh"}, {"Engine.Animation", "Core.Object"},
            {"Engine.Model", "Engine.Primitive"}, {"Engine.LevelBase", "Core.Object"},
            {"Engine.Level", "Engine.LevelBase"}, {"Engine.LevelSummary", "Core.Object"},
            {"Engine.Polys", "Core.Object"}, {"Engine.BspNodes", "Core.Object"},
            {"Engine.BspSurfs", "Core.Object"}, {"Engine.Vectors", "Core.Object"},
            {"Engine.Verts", "Core.Object"}, {"Engine.Bitmap", "Core.Object"},
            {"Engine.Texture", "Engine.Bitmap"}, {"Engine.FractalTexture", "Engine.Texture"},
            {"Engine.FireTexture", "Engine.FractalTexture"}, {"Engine.IceTexture", "Engine.FractalTexture"},
            {"Engine.WaterTexture", "Engine.FractalTexture"}, {"Engine.WaveTexture", "Engine.WaterTexture"},
            {"Engine.WetTexture", "Engine.WaterTexture"}, {"Engine.ScriptedTexture", "Engine.Texture"},
            {"Engine.Font", "Core.Object"},
            {"Engine.Client", "Core.Object"}, {"Engine.Viewport", "Engine.Player"}
        };
        // Runtime operand strings are not authored NameString table entries.
        // Comparing them must not permanently intern arbitrary rejected input.
        const auto key = QuestVr::ObjectCastDetail::Fold(path);
        const auto found = std::find_if(std::begin(registered), std::end(registered),
            [&](const auto& item) { return key == QuestVr::ObjectCastDetail::Fold(item.first); });
        if (found == std::end(registered)) return {};
        const std::string canonical = found->first;
        return QuestVr::ObjectCastClass{canonical, canonical.substr(canonical.find('.') + 1u), found->second};
    }
    std::string CastClassPath(const std::string& path) {
        const auto found = persistentVmObjects.find(LowerAscii(path));
        if (found == persistentVmObjects.end()) {
            if (const auto registered = NativeCastClass(path)) return registered->path;
            throw std::runtime_error("VM cast class identity unavailable: " + path);
        }
        const auto* object = found->second;
        if(object->nativeClientClass) {
            const auto registered=NativeCastClass(object->reflection.objectPath);
            if(!registered) throw std::runtime_error("Native player class is not an audited registration");
            return registered->path;
        }
        const auto& entry = Table(object->sourcePath).exports.at(object->exportIndex);
        if (entry.ObjClass != 0 || object->reflection.metaClass != "Class")
            throw std::runtime_error("VM cast target/base is not a serialized UClass: " + path);
        return object->reflection.objectPath;
    }
    std::string ResolveCastReference(const PortablePackageTables& table, const std::int32_t reference) {
        if (reference == 0) throw std::runtime_error("VM cast class reference is null");
        const auto path = ResolvePortableValueObjectReference(table, reference,
            [this](const std::string& resolved, const std::string& importedClass) {
                // FindObjectReference's non-Class branch excludes ObjClass0
                // exports. Metaclass IsA(Object/Struct/State) cannot make a
                // wrongly declared import resolve to a supported UClass.
                if (NameString(importedClass) != "Class") return false;
                // Class kind/identity only: do not recursively walk bases
                // while resolving an import, which would hide cycle bounds.
                static_cast<void>(CastClassPath(resolved));
                return true;
            });
        return CastClassPath(path);
    }
    QuestVr::ObjectCastClass CastClassIdentity(const std::string& path) {
        const auto found = persistentVmObjects.find(LowerAscii(path));
        if (found == persistentVmObjects.end()) {
            if (const auto registered = NativeCastClass(path)) return *registered;
            throw std::runtime_error("VM cast class identity unavailable: " + path);
        }
        auto* object = found->second;
        if(object->nativeClientClass) {
            const auto registered=NativeCastClass(object->reflection.objectPath);
            if(!registered) throw std::runtime_error("Native player class is not an audited registration");
            return *registered;
        }
        if (object->runtimeBirth || object->nativeWindowObject) throw std::runtime_error("Constructed instance is not a UClass");
        const auto& table = Table(object->sourcePath);
        const auto& entry = table.exports.at(object->exportIndex);
        if (entry.ObjClass != 0 || object->reflection.metaClass != "Class")
            throw std::runtime_error("VM cast target/base is not a serialized UClass: " + path);
        if (entry.ObjName < 0 || static_cast<std::size_t>(entry.ObjName) >= table.names.size())
            throw std::runtime_error("VM cast declaration Name is invalid");
        const auto& name = table.names[static_cast<std::size_t>(entry.ObjName)].Name;
        if (name.IsNone()) throw std::runtime_error("VM cast declaration Name is None");
        std::string base;
        if (entry.ObjBase != 0) {
            base = ResolveCastReference(table, entry.ObjBase);
        } else if (name != "Object") {
            // Package::LoadExportObject gives zero-base classes Core.Object;
            // a missing NONZERO authored reference is not silently defaulted.
            base = "Core.Object";
        }
        return {object->reflection.objectPath, name.ToString(), std::move(base)};
    }
    QuestVr::ObjectCastObject CastObjectIdentity(const std::string& path) {
        if (LowerAscii(path).rfind("@classdefault:",0u)==0u) {
            auto* object=Object(path);
            return {object->reflection.objectPath,object->defaultOwner->reflection.objectPath,false};
        }
        const auto found = persistentVmObjects.find(LowerAscii(path));
        if (found == persistentVmObjects.end()) {
            if (const auto registered = NativeCastClass(path))
                return {registered->path, "Core.Class", true};
            throw std::runtime_error("VM cast object identity unavailable: " + path);
        }
        auto* object = found->second;
        if(object->nativeClientClass)
            return {object->reflection.objectPath,"Core.Class",true};
        if(object->nativeClientIdentity) {
            object=Object(path); // Reject revoked lease before exposing kind.
            if(!object->cls) throw std::runtime_error("Native player UObject lacks actual class");
            return {object->reflection.objectPath,object->cls->reflection.objectPath,false};
        }
        if (object->runtimeBirth || object->nativeWindowObject) {
            if (!object->cls || !object->cls->classDescriptor)
                throw std::runtime_error("Runtime birth has no original class identity");
            return {object->reflection.objectPath, object->cls->reflection.objectPath, false};
        }
        const auto& table = Table(object->sourcePath);
        const auto& entry = table.exports.at(object->exportIndex);
        const bool classObject = entry.ObjClass == 0;
        const auto actualClass = classObject ? std::string("Core.Class") : ResolveCastReference(table, entry.ObjClass);
        if (actualClass.empty()) throw std::runtime_error("VM cast object has no actual Class");
        return {object->reflection.objectPath, actualClass, classObject};
    }
    void ValidateObjectValue(const Value& value, const std::string& expected, const bool classValue) {
        if (value.text.empty()) return;
        auto* object = Object(value.text);
        // Presentation inactivity does not destroy UObject identity. A live
        // Level iterator (and ordinary script assignment) may retain this
        // reference even when direct execution on that receiver is guarded.
        const auto actual = object->cls ? object->cls->reflection.objectPath :
            object->reflection.metaClass == "Class" ? "Core.Class" :
            Qualified(Table(object->sourcePath), Table(object->sourcePath).exports.at(object->exportIndex).ObjClass);
        if (classValue) {
            if (object->defaultOwner || (!object->classDescriptor && !object->nativeClientClass) || LowerAscii(actual) != "core.class" ||
                (!expected.empty() && !ClassDerives(value.text, expected)))
                throw std::runtime_error("Runtime Class property violates its class constraint");
        } else if (!expected.empty() && !ClassDerives(actual, expected))
            throw std::runtime_error("Runtime Object property violates its class constraint");
    }
    Value TypedLiveValue(const Property& property, const Value& value) {
        auto normalized = QuestVr::Vm::Coerce(value, property.zero);
        auto* metadata = Object(property.key);
        const auto& descriptor = *metadata->property;
        if (descriptor.type == "ObjectProperty" || descriptor.type == "ClassProperty") {
            const auto expected = Qualified(Table(metadata->sourcePath), descriptor.type == "ClassProperty" ?
                descriptor.secondaryType : descriptor.referencedType);
            ValidateObjectValue(normalized, expected, descriptor.type == "ClassProperty");
        } else if (normalized.kind == Kind::Struct) {
            for (const auto& field : StructPropertySchema(property.key)->fields)
                normalized.fields.at(LowerAscii(field.name)) =
                    TypedLiveValue(DescribeProperty(field.key), normalized.fields.at(LowerAscii(field.name)));
        }
        return normalized;
    }
    std::shared_ptr<Reference> ReferenceFor(RuntimeObject* object, const Property& property,
        const std::uint32_t index, const bool defaults) {
        // A qualified property reference is not interchangeable with another
        // class's same-named field. Validate before exposing a READ alias too:
        // untrusted bytecode must not retrieve a sibling value/default-zero
        // merely because the fallback storage lookup uses a leaf field name.
        auto* target = defaults || object->defaultOwner ? DefaultOwner(object) : object;
        if (!OwnsProperty(target->classDescriptor ? target : target->cls,property) ||
            index >= property.arrayDimension)
            throw std::runtime_error("VM property is not owned by its concrete receiver class");
        auto result = std::make_shared<Reference>(); result->zero = property.zero;
        result->resultLayout = property.resultLayout;
        result->read = [this, object, property, index, defaults]() { return Read(object, property, index, defaults); };
        result->write = [this, object, property, index, defaults](const Value& value) {
            if (object->defaultOwner) {
                const auto key=LowerAscii(property.key);
                if (key=="core.object.class" || key=="core.object.name" || key=="core.object.outer" ||
                    key=="core.object.objectflags" || key=="core.object.objectinternal")
                    throw std::runtime_error("Class-default UObject header mutation is not established");
            }
            auto* target = defaults || object->defaultOwner ? DefaultOwner(object) : object;
            const auto actual = ClassProperty(target->classDescriptor ? target : target->cls, property.name);
            const bool shadowed=LowerAscii(actual.key) != LowerAscii(property.key);
            if (shadowed && target->classDescriptor)
                throw std::runtime_error("Shadowed runtime field needs qualified writable storage: "+property.key);
            if(index >= property.arrayDimension || !OwnsProperty(target->classDescriptor?target:target->cls,property))
                throw std::runtime_error("VM property is not owned by its concrete receiver class");
            auto normalized = TypedLiveValue(property, value);
            std::vector<RuntimeObject*> windowRoots;
            if(target->nativeWindowObject) {
                const auto collect=[&](const auto& self,const Value& member,const std::size_t depth)->void {
                    LifecycleStep();
                    if(depth>64u || windowRoots.size()>4096u)
                        throw std::runtime_error("Native window reflected edge graph exceeds its bound");
                    if(member.kind==Kind::Object && !member.text.empty()) {
                        auto* root=Object(member.text);
                        if(std::find(windowRoots.begin(),windowRoots.end(),root)==windowRoots.end()) windowRoots.push_back(root);
                    }
                    for(const auto& [name,field]:member.fields) {static_cast<void>(name);self(self,field,depth+1u);}
                };
                collect(collect,normalized,0u);
            }
            Touch(target);
            if (target->classDescriptor) {
                target->classDefaultValues[LowerAscii(property.name)][index] = std::move(normalized);
            } else {
                if(target->nativeWindowObject)
                    target->nativeWindowPropertyRoots[LowerAscii(property.key)][index]=std::move(windowRoots);
                target->scriptValues[shadowed?LowerAscii(property.key):LowerAscii(property.name)][index] = std::move(normalized);
                target->committedScriptState = true;
                // Actor timer fields are independent native storage. Updating
                // them must not reconstruct/reset an unrelated live animation
                // clock merely because the reflected-property path is shared.
                const auto key=LowerAscii(property.key);
                if (key!="engine.actor.timerrate" && key!="engine.actor.timercounter" && key!="engine.actor.btimerloop")
                    UpdateClockProperty(target, property.name, index);
                const auto name=LowerAscii(property.name);
                if (index==0u && (name=="location" || name=="collisionradius" || name=="collisionheight" ||
                    name=="brush" || name=="bcollideactors") && HasCollisionSchema(target)) {
                    TouchCollision(); persistentActorCollision.UpdateLive(CollisionRecord(target));
                }
            }
        };
        return result;
    }
    void Touch(RuntimeObject* object) {
        if (!transaction_) throw std::runtime_error("VM mutation outside a transaction");
        if (saved_.find(object) == saved_.end()) {
            if (saved_.size() >= 4096u) throw std::runtime_error("VM touched actor budget exceeded");
            saved_.emplace(object, Before{object->scriptValues, object->classDefaultValues, object->animationClock, object->stateObject, object->latentTimeLeft, object->lastRenderTime,
                object->stateLocalsRevision, object->stateTransitionFlags, object->committedScriptState, object->childActors, object->basedActors,
                object->touchEventSent, object->committedLifecycle, object->worldRemoved,
                object->nativePlayer, object->nativeViewportActor, object->nativePlayerBinding,
                object->nativeRootWindow,object->uniqueObjectCounter,object->nativeWindowPropertyRoots,object->nativeWindowState});
        }
    }
    void TouchCollision() {
        if (!transaction_) throw std::runtime_error("Actor collision mutation outside a VM transaction");
        if (!beforeCollision_) beforeCollision_.emplace(persistentActorCollision);
    }
    void TouchAI(RuntimeObject* owner) {
        if (!transaction_) throw std::runtime_error("AI mutation outside a VM transaction");
        if (savedAi_.count(owner)) return;
        if (savedAi_.size() >= 64u) throw std::runtime_error("AI manager journal count exceeds budget");
        BeforeAI before;
        before.manager.set(owner->nativeAiManager);
        if (owner->nativeAiManager) {
            before.state = owner->nativeAiManager->state;
            before.actorRoots = owner->nativeAiManager->actorRoots;
        }
        savedAi_.emplace(owner, std::move(before));
    }
    void TouchDecals() {
        if (!transaction_) throw std::runtime_error("Decal mutation outside a VM transaction");
        if (beforeDecals_) return;
        auto* graph = persistentRuntime->get()->nativeDecals;
        // Charge the additional journal before copying any nested geometry.
        if (graph) ValidateStateBudget(NativeDecalRetainedBytes(*graph));
        BeforeDecals before; before.graph.set(graph);
        if (graph) { before.state.emplace(graph->state); before.actorRoots = graph->actorRoots; }
        beforeDecals_.emplace(std::move(before));
    }
    void RefreshDecalRoots(RuntimeMapDecals* graph) {
        std::vector<RuntimeObject*> roots;
        const auto rootCount = graph->state.Usage().records;
        ValidateStateBudget(QuestVr::DecalStateDetail::Multiply(rootCount, sizeof(RuntimeObject*)));
        roots.reserve(rootCount);
        const auto retain = [&](const std::string& path) {
            LifecycleStep();
            auto* actor = Object(path); RequireMapActor(actor);
            if (!IsDerivedFromPath(actor->cls, "Engine.Decal") || actor->nativeXLevel != graph->level ||
                actor->reflection.objectPath != path)
                throw std::runtime_error("Native decal owner is not a canonical Decal in its native Level");
            roots.push_back(actor);
        };
        const auto& snapshot = graph->state.ReadOnlySnapshot();
        for (const auto& surface : snapshot.surfaces) for (const auto& record : surface) retain(record.ownerIdentity);
        graph->actorRoots.swap(roots);
    }
    RuntimeObject* LiveDecalTexture(RuntimeObject* actor) {
        LifecycleStep(); RequireMapActor(actor);
        const auto property = PropertyNamed(actor, "Texture");
        const auto* metadata = Object(property.key);
        if (LowerAscii(property.key) != "engine.actor.texture" || property.zero.kind != Kind::Object ||
            property.arrayDimension != 1u || !metadata->property || metadata->property->type != "ObjectProperty" ||
            LowerAscii(Qualified(Table(metadata->sourcePath), metadata->property->referencedType)) != "engine.texture")
            throw std::runtime_error("AttachDecal Texture schema does not match original GOTY");
        const auto value = TypedLiveValue(property, Read(actor, property, 0u));
        return value.text.empty() ? nullptr : Object(value.text);
    }
    std::int32_t LiveDecalTextureUSize(RuntimeObject* texture) {
        // UTexture's native bitmap fields are authored independently of mip
        // decoding. Never substitute rounded mip dimensions or a guessed size.
        if (const auto* overlay = FindScriptOverlay(texture, "USize", 0u)) {
            if (overlay->kind != Kind::Int) throw std::runtime_error("Native Texture USize is not an INT");
            return overlay->integer;
        }
        const auto properties = LoadPortableExportProperties(Table(texture->sourcePath), texture->exportIndex,
            64u * 1024u * 1024u);
        for (auto property = properties.properties.rbegin(); property != properties.properties.rend(); ++property)
            if (property->name == "USize" && property->arrayIndex == 0u) {
                if (property->type != 2u || property->value.size() != 4u)
                    throw std::runtime_error("Native Texture USize tag is not an INT");
                std::int32_t value{}; std::memcpy(&value, property->value.data(), sizeof(value)); return value;
            }
        throw std::runtime_error("Native Texture has no authored USize field");
    }
    void EnsureDecalRootModel() {
        if (rootModel_) return;
        const auto& map = Table(persistentMapSourcePath);
        constexpr std::size_t scratch = 64u * 1024u * 1024u;
        const auto index = FindPortableRootModel68Export(map, scratch);
        const auto bytes = map.exports.at(index).ObjSize;
        if (bytes <= 0 || QuestVr::DecalStateDetail::Multiply(static_cast<std::size_t>(bytes), 16u) > scratch - 1024u)
            throw std::runtime_error("Native decal Model scratch budget exceeded");
        rootModel_ = LoadPortableModel68(map, index, scratch);
    }
    Property DecalSurfListProperty(RuntimeObject* actor) {
        const auto property = PropertyNamed(actor, "SurfList");
        if (LowerAscii(property.key) != "engine.decal.surflist" || property.flags != 0x00400002u ||
            property.zero.kind != Kind::IntArray || property.arrayDimension != 1u)
            throw std::runtime_error("Native decal SurfList schema does not match original GOTY");
        return property;
    }
    void PrechargeDecalMutation(RuntimeMapDecals* graph, const std::size_t additionalBytes = 0u) {
        auto bytes = QuestVr::DecalStateDetail::Multiply(NativeDecalRetainedBytes(*graph), 3u);
        bytes = QuestVr::DecalStateDetail::Add(bytes,
            QuestVr::DecalStateDetail::Multiply(additionalBytes, 3u));
        ValidateStateBudget(QuestVr::DecalStateDetail::Add(bytes, 4096u));
    }
    Evaluation AttachDecalEntryNative(RuntimeObject* actor, const std::vector<Evaluation>& arguments,
                                     const Function& declaration, QuestVr::Vm::Execution& execution) {
        RequireMapActor(actor);
        if (!IsDerivedFromPath(actor->cls, "Engine.Decal") || declaration.nativeIndex != 0u ||
            declaration.flags != 0x500u || declaration.variables.size() != 3u ||
            arguments.empty() || arguments.size() > 2u)
            throw std::runtime_error("AttachDecal declaration or receiver does not match original GOTY schema");
        bool distance{}, direction{}, returned{};
        for (const auto& variable : declaration.variables) {
            if (variable.arrayDimension != 1u)
                throw std::runtime_error("AttachDecal parameter is not an original scalar");
            const auto key = LowerAscii(variable.key);
            if (key == "engine.decal.attachdecal.tracedistance" &&
                variable.zero.kind == Kind::Float && variable.flags == 0x80u && !distance) distance = true;
            else if (key == "engine.decal.attachdecal.decaldir" &&
                variable.zero.kind == Kind::Vector && variable.flags == 0x90u && !direction) direction = true;
            else if (key == "engine.decal.attachdecal.returnvalue" &&
                variable.zero.kind == Kind::Bool && variable.flags == 0x580u && !returned) returned = true;
            else throw std::runtime_error("AttachDecal parameter/return does not match original GOTY schema");
        }
        if (!distance || !direction || !returned || arguments[0u].Load().kind != Kind::Float ||
            (arguments.size() == 2u && arguments[1u].Load().kind != Kind::Nothing &&
                arguments[1u].Load().kind != Kind::Vector))
            throw std::runtime_error("AttachDecal arguments do not match original GOTY schema");
        // Arguments have already evaluated. The original false guard precedes
        // Texture, MultiDecalLevel, random draws and all native geometry access.
        if (!persistentNativeClient.HasViewport()) return {Value::Bool(false), {}};
        clientDependent_ = true;
        auto* texture = LiveDecalTexture(actor);
        if (!texture) {
            __android_log_print(ANDROID_LOG_WARN, "quest_main", "AttachDecal: No Texture");
            return {Value::Bool(false), {}};
        }
        const auto multiProperty = PropertyNamed(actor, "MultiDecalLevel");
        if (LowerAscii(multiProperty.key) != "engine.decal.multidecallevel" ||
            multiProperty.zero.kind != Kind::Int || multiProperty.flags != 0u || multiProperty.arrayDimension != 1u)
            throw std::runtime_error("AttachDecal MultiDecalLevel schema does not match original GOTY");
        const auto multi = QuestVr::Vm::ToInt(Read(actor, multiProperty, 0u));
        if (multi > 4) ReferenceFor(actor, multiProperty, 0u, false)->write(Value::Integer(4));
        const auto location = Read(actor, PropertyNamed(actor, "Location"), 0u);
        const auto rotation = Read(actor, PropertyNamed(actor, "Rotation"), 0u);
        if (location.kind != Kind::Vector || rotation.kind != Kind::Rotator)
            throw std::runtime_error("AttachDecal Location/Rotation schema is invalid");
        const auto axis = Coords::Rotation(Rotator(rotation.rotation[0], rotation.rotation[1], rotation.rotation[2])).XAxis;
        const auto supplied = arguments.size() == 2u ? arguments[1u].Load() : Value{};
        const auto directionValue = supplied.kind == Kind::Nothing ? std::array<float, 3>{} : supplied.vector;
        EnsureDecalRootModel();
        auto seed = persistentScriptRandomSeed.value_or(QuestVr::ScriptRandom::InitialSeed);
        const auto primary = QuestVr::PrepareOriginalDecalPrimary(*rootModel_,
            {location.vector[0], location.vector[1], location.vector[2]}, {axis.x, axis.y, axis.z},
            arguments[0u].Load().floating, {directionValue[0], directionValue[1], directionValue[2]}, seed);
        // Publish pretrace draws before any mover callback can use the stream.
        if (primary.randomDraws) persistentScriptRandomSeed = seed;
        if (!primary.hasPrimaryGeometry) return {Value::Bool(false), {}};
        const auto surfProperty = DecalSurfListProperty(actor);
        auto list = Read(actor, surfProperty, 0u).intArray;
        for (const auto surface : list)
            if (surface < 0 || static_cast<std::size_t>(surface) >= rootModel_->surfaces.size())
                throw std::runtime_error("AttachDecal SurfList is outside its native Model");
        TouchDecals();
        auto* graph = persistentRuntime->get()->nativeDecals;
        if (!graph) {
            const auto rows = QuestVr::DecalStateDetail::Multiply(rootModel_->surfaces.size(),
                sizeof(std::vector<QuestVr::DecalSurfaceRecord>));
            ValidateStateBudget(QuestVr::DecalStateDetail::Add(rows,
                QuestVr::DecalState::TransientStorageBytes(rootModel_->surfaces.size()) + sizeof(RuntimeMapDecals)));
            graph = GC::Alloc<RuntimeMapDecals>(rootModel_->surfaces.size(), rootModel_->nodes.size());
            pendingBirthGarbage = true;
            graph->level = actor->nativeXLevel;
            graph->model = Object(Qualified(Table(persistentMapSourcePath),
                static_cast<std::int32_t>(rootModel_->exportIndex + 1u)));
            persistentRuntime->get()->nativeDecals = graph;
        }
        if (graph->level != actor->nativeXLevel || graph->state.Usage().surfaces != rootModel_->surfaces.size() ||
            graph->state.NodeCount() != rootModel_->nodes.size())
            throw std::runtime_error("AttachDecal graph belongs to another native Model/Level");
        const auto resolveTexture = [&](const std::string& path) -> std::optional<std::string> {
            auto* owner = Object(path); RequireMapActor(owner);
            if (!IsDerivedFromPath(owner->cls, "Engine.Decal") || owner->nativeXLevel != graph->level)
                throw std::runtime_error("Native decal record owner is not a Decal in its Level");
            auto* live = LiveDecalTexture(owner);
            return live ? std::optional<std::string>(live->reflection.objectPath) : std::nullopt;
        };
        RuntimeObject* mover{};
        QuestVr::DecalStateRotation moverRotation;
        const auto reference = rootModel_->surfaces.at(static_cast<std::size_t>(primary.surface)).brushActorReference;
        if (reference) {
            auto* surfaceActor = Object(Qualified(Table(persistentMapSourcePath), reference));
            if (IsDerivedFromPath(surfaceActor->cls, "Engine.Mover")) {
                RequireMapActor(surfaceActor); mover = surfaceActor;
                const auto value = Read(mover, PropertyNamed(mover, "Rotation"), 0u);
                if (value.kind != Kind::Rotator) throw std::runtime_error("Decal mover Rotation is not a Rotator");
                moverRotation = {value.rotation[0], value.rotation[1], value.rotation[2]};
            }
        }
        PrechargeDecalMutation(graph, actor->reflection.objectPath.size() + list.capacity() * sizeof(std::int32_t));
        graph->state.ReplaceOwnerSurfList(actor->reflection.objectPath, list);
        const auto begun = graph->state.BeginPrimary(primary.surface, actor->reflection.objectPath,
            texture->reflection.objectPath, resolveTexture, moverRotation);
        list.push_back(primary.surface);
        ReferenceFor(actor, surfProperty, 0u, false)->write(Value::IntArray(std::move(list)));
        RefreshDecalRoots(graph); // The provisional owner is rooted before script.
        if (mover) SetBase(actor, ActorLink(actor, "Owner"), true, execution);
        if (persistentRuntime->get()->nativeDecals != graph)
            throw std::runtime_error("Mover callback replaced the native decal graph");
        texture = LiveDecalTexture(actor);
        if (!texture) throw std::runtime_error("Mover callback cleared Texture during native AttachDecal");
        const auto drawScale = Read(actor, PropertyNamed(actor, "DrawScale"), 0u);
        if (drawScale.kind != Kind::Float) throw std::runtime_error("AttachDecal DrawScale is not a Float");
        const auto quad = QuestVr::BuildOriginalDecalPrimaryQuad(primary.center, primary.surfaceBase,
            primary.surfaceNormal, LiveDecalTextureUSize(texture), drawScale.floating, primary.preparedDirection);
        QuestVr::DecalProjectionWork work; work.workSteps = primary.workSteps;
        const auto projected = QuestVr::BuildOriginalDecalPrimaryMembership(*rootModel_, primary.surface, quad, work);
        const auto recordFor = [&](const auto& corners, const std::vector<std::int32_t>& membership) {
            QuestVr::DecalSurfaceRecord record; record.ownerIdentity = actor->reflection.objectPath;
            for (std::size_t corner = 0u; corner < 4u; ++corner)
                record.relativeCorners[corner] = {corners[corner].x, corners[corner].y, corners[corner].z};
            // CompletePrimary preserves its pre-callback mover rotation;
            // secondary zeroed records have no original mover/SetBase block.
            record.nodeMembership = membership; return record;
        };
        PrechargeDecalMutation(graph, projected.nodeMembership.capacity() * sizeof(std::int32_t));
        graph->state.CompletePrimary(begun.token, recordFor(quad.worldCorners, projected.nodeMembership));
        // No clamp here: SetBase may have changed Multi after its entry clamp.
        for (std::int64_t i = 0; i < QuestVr::Vm::ToInt(Read(actor, multiProperty, 0u)); ++i)
            for (std::int64_t j = 0; j < QuestVr::Vm::ToInt(Read(actor, multiProperty, 0u)); ++j) {
                auto currentList = Read(actor, surfProperty, 0u).intArray;
                const auto rawMulti = QuestVr::Vm::ToInt(Read(actor, multiProperty, 0u));
                const auto secondary = QuestVr::ProcessOriginalDecalSecondarySample(*rootModel_, quad.worldCorners,
                    primary.surfaceNormal, rawMulti, static_cast<std::int32_t>(i), static_cast<std::int32_t>(j), currentList, work);
                if (secondary.status != QuestVr::DecalSecondarySampleStatus::Accepted) continue;
                texture = LiveDecalTexture(actor);
                if (!texture) throw std::runtime_error("Native AttachDecal lost live secondary Texture");
                const auto& surface = secondary.projected;
                PrechargeDecalMutation(graph, surface.nodeMembership.capacity() * sizeof(std::int32_t) +
                    currentList.capacity() * sizeof(std::int32_t));
                graph->state.ReplaceOwnerSurfList(actor->reflection.objectPath, currentList);
                graph->state.AttachPrimary(surface.surface, recordFor(surface.relativeCorners, surface.nodeMembership),
                    texture->reflection.objectPath, resolveTexture);
                currentList.push_back(surface.surface);
                ReferenceFor(actor, surfProperty, 0u, false)->write(Value::IntArray(std::move(currentList)));
            }
        PrechargeDecalMutation(graph, projected.nodeMembership.capacity() * sizeof(std::int32_t));
        graph->state.CompletePrimary(begun.token, recordFor(quad.relativeCorners, projected.nodeMembership));
        RefreshDecalRoots(graph); ValidateStateBudget();
        return {Value::Bool(true), {}};
    }
    Evaluation DetachDecalNative(RuntimeObject* actor, const std::vector<Evaluation>& arguments,
                                 const Function& declaration) {
        RequireMapActor(actor);
        if (!IsDerivedFromPath(actor->cls, "Engine.Decal") || !arguments.empty() ||
            declaration.nativeIndex != 0u || declaration.flags != 0x500u || !declaration.variables.empty())
            throw std::runtime_error("DetachDecal declaration or receiver does not match original GOTY schema");
        const auto property = PropertyNamed(actor, "SurfList");
        if (LowerAscii(property.key) != "engine.decal.surflist" || property.flags != 0x00400002u ||
            property.zero.kind != Kind::IntArray || property.arrayDimension != 1u)
            throw std::runtime_error("DetachDecal SurfList schema does not match original GOTY");
        const auto list = Read(actor, property, 0u);
        if (list.intArray.empty()) return {}; // Do not invent native state on an empty original loop.
        if (!rootModel_) rootModel_ = LoadPortableRootModel68(Table(persistentMapSourcePath));
        for (const auto surface : list.intArray)
            if (surface < 0 || static_cast<std::size_t>(surface) >= rootModel_->surfaces.size())
                throw std::runtime_error("DetachDecal SurfList surface is outside its native Model");
        auto* graph = persistentRuntime->get()->nativeDecals;
        if (graph) {
            if (graph->level != actor->nativeXLevel || graph->state.Usage().surfaces != rootModel_->surfaces.size() ||
                graph->state.NodeCount() != rootModel_->nodes.size())
                throw std::runtime_error("DetachDecal graph belongs to another native Model/Level");
            TouchDecals();
            // Standalone mutations stage full candidates before publication.
            // Charge their old/new/overlap buffers in the enclosing host before
            // allocating, in addition to the already retained rollback journal.
            auto extra = QuestVr::DecalStateDetail::Multiply(NativeDecalRetainedBytes(*graph), 3u);
            extra = QuestVr::DecalStateDetail::Add(extra,
                QuestVr::DecalStateDetail::Multiply(list.intArray.capacity(), 3u * sizeof(std::int32_t)));
            extra = QuestVr::DecalStateDetail::Add(extra, actor->reflection.objectPath.size() + 1024u);
            ValidateStateBudget(extra);
            graph->state.ReplaceOwnerSurfList(actor->reflection.objectPath, list.intArray);
            graph->state.Detach(actor->reflection.objectPath);
            RefreshDecalRoots(graph);
        }
        // Native code may update this Const reflected array; ordinary script
        // writes still pass the VM's original Const-property guard.
        ReferenceFor(actor, property, 0u, false)->write(Value::IntArray({}));
        ValidateStateBudget();
        return {};
    }
    void RefreshAIRoots(RuntimeAIManager* manager) {
        std::vector<RuntimeObject*> roots;
        const auto retain = [&](const std::string& path) {
            if (path.empty()) return;
            LifecycleStep();
            auto* actor = Object(path); RequireMapActor(actor);
            if (actor->nativeXLevel != manager->level)
                throw std::runtime_error("AI graph actor belongs to another native Level");
            roots.push_back(actor);
        };
        for (const auto& sender : manager->state.senders) retain(sender.actor);
        for (const auto& receiver : manager->state.receivers) {
            retain(receiver.actor); retain(receiver.previousBestActor); retain(receiver.params.bestActor);
        }
        manager->actorRoots.swap(roots);
    }
    RuntimeObject* NativeLevelInfo(RuntimeObject* actor) {
        RequireMapActor(actor);
        auto* level = actor->nativeXLevel;
        if (!level || level->sourcePath != persistentMapSourcePath ||
            LowerAscii(CastObjectIdentity(level->reflection.objectPath).classPath) != "engine.level")
            throw std::runtime_error("Native receiver has no actual linked native Level");
        // Native wrappers dereference ULevel.Actors[0], not the reflected
        // Actor.Level/XLevel overlays or the first LevelInfo export by name.
        if (persistentWorldActors.empty() || !persistentWorldActors.front() ||
            persistentWorldActors.front()->worldRemoved ||
            !IsDerivedFromPath(persistentWorldActors.front()->cls, "Engine.LevelInfo") ||
            persistentWorldActors.front()->nativeXLevel != level)
            throw std::runtime_error("Native Level slot zero is not its actual LevelInfo");
        return persistentWorldActors.front();
    }
    RuntimeAIManager* ActorAIManager(RuntimeObject* actor) {
        return NativeLevelInfo(actor)->nativeAiManager;
    }
    Evaluation PawnListNative(const std::uint16_t index, RuntimeObject* actor,
        const std::vector<Evaluation>& arguments) {
        LifecycleStep();
        if (!arguments.empty()) throw std::runtime_error("AddPawn/RemovePawn takes no arguments");
        RequireMapActor(actor);
        if (!IsDerivedFromPath(actor->cls, "Engine.Pawn"))
            throw std::runtime_error("AddPawn/RemovePawn receiver is not Engine.Pawn");
        auto* info = NativeLevelInfo(actor);
        const auto headProperty = PropertyNamed(info, "PawnList");
        const auto nextProperty = PropertyNamed(actor, "nextPawn");
        const auto schema = [&](const Property& property, const char* path) {
            auto* metadata = Object(property.key);
            if (LowerAscii(property.key) != path || property.arrayDimension != 1u ||
                property.zero.kind != Kind::Object || !metadata->property ||
                metadata->property->type != "ObjectProperty" ||
                LowerAscii(Qualified(Table(metadata->sourcePath), metadata->property->referencedType)) != "engine.pawn")
                throw std::runtime_error("Native pawn-list property is not its original scalar Pawn field");
        };
        schema(headProperty, "engine.levelinfo.pawnlist");
        schema(nextProperty, "engine.pawn.nextpawn");
        const auto link = [&](RuntimeObject* owner, const Property& property) -> RuntimeObject* {
            const auto value = Read(owner, property, 0u);
            if (value.kind != Kind::Object) throw std::runtime_error("Native pawn-list link is not an Object");
            if (value.text.empty()) return nullptr;
            auto* target = Object(value.text); RequireMapActor(target);
            if (!IsDerivedFromPath(target->cls, "Engine.Pawn") || target->nativeXLevel != actor->nativeXLevel)
                throw std::runtime_error("Native pawn-list link is not a Pawn in its actual Level");
            return target;
        };
        const auto write = [&](RuntimeObject* owner, const Property& property, RuntimeObject* target) {
            ReferenceFor(owner, property, 0u, false)->write(Value::Text(Kind::Object,
                target ? target->reflection.objectPath : std::string{}));
        };
        auto* head = link(info, headProperty);
        // Original GOTY execAddPawn0x103bb430 uses native XLevel.Actors[0],
        // then unconditionally prepends. No begun/Role/deletion/dedup gate.
        if (index == 529u) {
            write(actor, nextProperty, head);
            write(info, headProperty, actor);
            return {};
        }
        // Original execRemovePawn0x103bb570 only splices the first match.
        // Neither branch clears Self.nextPawn (unlike the vendored NPawn).
        if (head == actor) {
            write(info, headProperty, link(actor, nextProperty));
            return {};
        }
        std::unordered_set<RuntimeObject*> visited;
        for (auto* current = head; current;) {
            LifecycleStep();
            if (visited.size() >= 4096u || !visited.insert(current).second)
                throw std::runtime_error("Native pawn-list traversal is cyclic or exceeds its work bound");
            auto* next = link(current, nextProperty);
            if (next == actor) {
                write(current, nextProperty, link(actor, nextProperty));
                return {};
            }
            current = next;
        }
        return {};
    }
    Evaluation AiNative(const std::uint16_t index, RuntimeObject* actor,
        const std::vector<Evaluation>& arguments) {
        LifecycleStep(); RequireMapActor(actor);
        const auto count = [&](const std::size_t minimum, const std::size_t maximum) {
            if (arguments.size() < minimum || arguments.size() > maximum)
                throw std::runtime_error("AI native argument count is invalid");
        };
        const auto arg = [&](const std::size_t slot) {
            return slot < arguments.size() ? arguments[slot].Load() : Value{};
        };
        const auto name = [&](const std::size_t slot, const bool optional = false) {
            const auto value = arg(slot);
            if (optional && value.kind == Kind::Nothing) return std::string{};
            if (value.kind != Kind::Name) throw std::runtime_error("AI native requires a Name");
            return value.text;
        };
        const auto boolean = [&](const std::size_t slot, const bool fallback) {
            const auto value = arg(slot);
            if (value.kind == Kind::Nothing) return fallback;
            if (value.kind != Kind::Bool) throw std::runtime_error("AI perception flag is not Bool");
            return QuestVr::Vm::ToBool(value);
        };
        const auto number = [&](const std::size_t slot, const float fallback) {
            const auto value = arg(slot);
            if (value.kind == Kind::Nothing) return fallback;
            if (value.kind != Kind::Float && value.kind != Kind::Byte && value.kind != Kind::Int)
                throw std::runtime_error("AI intensity/radius is not numeric");
            const auto result = QuestVr::Vm::ToFloat(value);
            if (!std::isfinite(result)) throw std::runtime_error("AI intensity/radius is not finite");
            return result;
        };
        if (index == 650u) {
            count(0u, 0u);
            if (!IsDerivedFromPath(actor->cls, "Engine.LevelInfo") || !actor->nativeXLevel)
                throw std::runtime_error("InitEventManager receiver is not a linked LevelInfo");
            if (actor->nativeAiManager) return {};
            TouchAI(actor);
            pendingBirthGarbage = true;
            provisionalAi_.emplace_back(GC::Alloc<RuntimeAIManager>());
            auto* manager = provisionalAi_.back().get();
            manager->owner = actor; manager->level = actor->nativeXLevel;
            manager->state.ownerPath = actor->reflection.objectPath;
            manager->state.levelPath = manager->level->reflection.objectPath;
            actor->nativeAiManager = manager;
            return {};
        }
        std::string event, callback, scoreCallback;
        QuestVr::Ai::PerceptionFlags flags;
        std::uint8_t channel{};
        float value = 1.0f, radius = 800.0f;
        if (index == 710u) {
            count(2u, 7u); event = name(0u); callback = name(1u); scoreCallback = name(2u, true);
            flags = {boolean(3u, true), boolean(4u, true), boolean(5u, false), boolean(6u, true)};
        } else if (index == 711u || index == 716u) {
            count(1u, 1u); event = name(0u);
        } else {
            count(2u, index == 715u ? 2u : 4u); event = name(0u);
            const auto type = arg(1u);
            if (type.kind != Kind::Byte && type.kind != Kind::Int)
                throw std::runtime_error("AI sensory type is not Byte");
            channel = static_cast<std::uint8_t>(type.integer);
            if (index != 715u) { value = number(2u, 1.0f); radius = number(3u, 800.0f); }
        }
        auto* manager = ActorAIManager(actor);
        if (!manager) return {}; // Original absent-manager branch, after evaluating all arguments.
        TouchAI(manager->owner);
        const auto& path = actor->reflection.objectPath;
        if (index == 710u) QuestVr::Ai::Register(manager->state, path, event, callback, scoreCallback, flags, lifecycleOperations_);
        else if (index == 711u) QuestVr::Ai::ClearReceiver(manager->state, path, event, lifecycleOperations_);
        else if (index == 715u) QuestVr::Ai::End(manager->state, path, event, channel, lifecycleOperations_);
        else if (index == 716u) QuestVr::Ai::ClearSender(manager->state, path, event, lifecycleOperations_);
        else {
            // Original IsPendingKill reads the same packed bit whose cleanup
            // assertion names ActorToKill->bDeleteMe (Engine.dll 103966bd).
            const bool pendingKill = QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, "bDeleteMe"), 0u));
            QuestVr::Ai::Emit(manager->state, path, event, channel, value, radius, index == 713u,
                pendingKill, lifecycleOperations_);
        }
        RefreshAIRoots(manager);
        return {};
    }
    void ValidateStateBudget(const std::size_t extraRetained = 0u,const RuntimeObject* clonedDefaults = nullptr) const {
        // Bound persistent state across independent calls, not just one VM's
        // temporary allocations. Measure without copying live values/sets.
        // State/default storage is bounded without copying live values. The
        // save path additionally applies combined gameplay/property/clock/
        // state/default capture and envelope budgets.
        QuestVr::ScriptStateLimits limits;
        limits.maxBytes = QuestVr::kMaximumSaveRuntimeBytes;
        QuestVr::ScriptStateDetail::Writer measured(limits, nullptr);
        QuestVr::ScriptStateDetail::Budget defaults{limits};
        defaults.Retain(extraRetained);
        if (persistentRuntime->get()->nativeDecals)
            defaults.Retain(NativeDecalRetainedBytes(*persistentRuntime->get()->nativeDecals));
        if (beforeDecals_ && beforeDecals_->state) {
            defaults.Retain(QuestVr::DecalSnapshotRetainedBytes(beforeDecals_->state->ReadOnlySnapshot(),
                beforeDecals_->state->Usage().surfaces, beforeDecals_->state->NodeCount()));
            defaults.Retain(beforeDecals_->state->TransientRetainedBytes());
            defaults.Retain(beforeDecals_->actorRoots.capacity() * sizeof(RuntimeObject*));
        }
        if (persistentScriptRandomSeed) defaults.Retain(sizeof(persistentScriptRandomSeed));
        const auto valueBudget = [&](const auto& self, const Value& value, const std::size_t depth) -> void {
            defaults.Node(depth); defaults.Retain(sizeof(value));
            defaults.IntArrayElements(value.intArray.size());
            defaults.Retain(QuestVr::DecalStateDetail::Multiply(value.intArray.capacity(), sizeof(std::int32_t)));
            QuestVr::ScriptStateDetail::Text(value.text, limits, false, true);
            defaults.Retain(value.text.size() + 1u);
            if (value.kind == Kind::Float && !std::isfinite(value.floating))
                throw std::runtime_error("Persistent class-default float is not finite");
            if (value.kind == Kind::Vector)
                for (const auto component : value.vector) if (!std::isfinite(component))
                    throw std::runtime_error("Persistent class-default vector is not finite");
            for (const auto& [name, member] : value.fields) {
                QuestVr::ScriptStateDetail::Text(name, limits, true);
                defaults.Retain(sizeof(std::string) + 5u * sizeof(void*) + name.size() + 1u);
                self(self, member, depth + 1u);
            }
        };
        if(clonedDefaults)
            for(const auto& [name,slots]:clonedDefaults->classDefaultValues) {
                QuestVr::ScriptStateDetail::Text(name,limits,true);
                defaults.Properties(slots.size());
                defaults.Retain(sizeof(std::string)+5u*sizeof(void*)+name.capacity()+1u);
                defaults.Array(slots.size(),sizeof(QuestVr::ScriptSavedProperty)+5u*sizeof(void*));
                for(const auto& [index,value]:slots) {static_cast<void>(index);valueBudget(valueBudget,value,0u);}
            }
        std::size_t objects{}, totalNativeLinks{};
        const auto& exports=persistentRuntime->get()->exports;
        const auto* windows=persistentWindowDomain && persistentWindowDomain->get() ? persistentWindowDomain->get() : nullptr;
        const auto totalObjects=exports.size()+(windows?windows->objects.size():0u);
        for(std::size_t objectIndex=0u;objectIndex<totalObjects;++objectIndex) {
            const auto* object=objectIndex<exports.size()?exports[objectIndex]:windows->objects[objectIndex-exports.size()];
            if(object->nativeWindowObject) defaults.Retain(sizeof(RuntimeObject)+sizeof(GCAllocation)+
                object->reflection.objectPath.size()+object->references.capacity()*sizeof(RuntimeObject*));
            if(object->nativeWindowState) {
                for(const auto* list:{&object->nativeWindowState->rowMajor,&object->nativeWindowState->colMajor,&object->nativeWindowState->tabGroups}) {
                    if(list->size()>4096u) throw std::runtime_error("Native window list exceeds its bound");
                    defaults.Retain(QuestVr::DecalStateDetail::Multiply(list->capacity(),sizeof(RuntimeObject*)));
                }
            }
            for(const auto& [key,slots]:object->nativeWindowPropertyRoots) {
                defaults.Retain(sizeof(std::string)+5u*sizeof(void*)+key.capacity());
                defaults.Retain(slots.size()*(sizeof(std::uint32_t)+5u*sizeof(void*)+sizeof(std::vector<RuntimeObject*>)));
                for(const auto& [index,roots]:slots) {
                    static_cast<void>(index);
                    defaults.Retain(QuestVr::DecalStateDetail::Multiply(roots.capacity(),sizeof(RuntimeObject*)));
                }
            }
            if (object->nativeAiManager) {
                const auto before = measured.measuredBudget().retained;
                const auto* manager = object->nativeAiManager;
                measured.MeasureAiManager(manager->state);
                const auto accounted = measured.measuredBudget().retained - before;
                const auto actual = QuestVr::Ai::RetainedBytes(manager->state) + sizeof(RuntimeAIManager) -
                    sizeof(QuestVr::Ai::State) + sizeof(GCAllocation) + manager->actorRoots.capacity() * sizeof(RuntimeObject*);
                if (actual > accounted) defaults.Retain(actual - accounted);
            }
            const auto nativeCount = object->childActors.size() + object->basedActors.size();
            if (nativeCount > limits.maxActorLinks || totalNativeLinks > limits.maxActorLinks - nativeCount)
                throw std::runtime_error("Persistent total native actor link budget exceeded");
            totalNativeLinks += nativeCount;
            if ((object->runtimeBirth || object->committedScriptState || object->stateObject || object->committedLifecycle) &&
                ++objects > limits.maxObjects)
                throw std::runtime_error("Persistent script/lifecycle actor count budget exceeded");
            if (object->latentTimeLeft) {
                if (!std::isfinite(*object->latentTimeLeft)) throw std::runtime_error("Persistent actor latent timer is not finite");
                defaults.Retain(sizeof(object->latentTimeLeft));
            }
            if (object->runtimeBirth) defaults.Retain(sizeof(QuestVr::ScriptSavedBirth) +
                object->reflection.objectPath.size() + object->cls->reflection.objectPath.size() + 2u);
            if (object->committedLifecycle) {
                const auto count = object->childActors.size() + object->basedActors.size();
                defaults.ActorLinks(count);
                defaults.Retain(sizeof(QuestVr::ScriptSavedActorLifecycle));
                const auto links = [&](const std::vector<RuntimeObject*>& values) {
                    for (const auto* link : values) {
                        if (!link || !IsDerivedFromPath(link->cls, "Engine.Actor") ||
                            PackageStem(link->sourcePath) != persistentMapPackageName)
                            throw std::runtime_error("Persistent native link is not a current map actor");
                        QuestVr::ScriptStateDetail::Text(link->reflection.objectPath, limits, true);
                        defaults.Retain(sizeof(std::string) + link->reflection.objectPath.size() + 1u);
                    }
                };
                links(object->childActors); links(object->basedActors);
            }
            for (const auto* values : {&object->scriptValues, &object->frozenBirthDefaults})
                for (const auto& [name, slots] : *values) {
                    QuestVr::ScriptStateDetail::Text(name, limits, true);
                    defaults.Properties(slots.size());
                    defaults.Retain(sizeof(std::string) + 5u * sizeof(void*) + name.size() + 1u);
                    defaults.Array(slots.size(), sizeof(QuestVr::ScriptSavedProperty) + 5u * sizeof(void*));
                    for (const auto& [index, value] : slots) { static_cast<void>(index); valueBudget(valueBudget, value, 0u); }
                }
            if (!object->classDefaultValues.empty()) {
                if (++objects > limits.maxObjects) throw std::runtime_error("Persistent default/state object budget exceeded");
                defaults.Retain(sizeof(QuestVr::ScriptSavedClassDefaults) + object->reflection.objectPath.size() + 1u);
                for (const auto& [name, slots] : object->classDefaultValues) {
                    QuestVr::ScriptStateDetail::Text(name, limits, true);
                    defaults.Properties(slots.size());
                    defaults.Retain(sizeof(std::string) + 5u * sizeof(void*) + name.size() + 1u);
                    defaults.Array(slots.size(), sizeof(QuestVr::ScriptSavedProperty) + 5u * sizeof(void*));
                    for (const auto& [index, value] : slots) { static_cast<void>(index); valueBudget(valueBudget, value, 0u); }
                }
            }
            if (!object->stateObject) continue;
            measured.MeasureStateObject(*object->stateObject);
        }
        if (measured.measuredBudget().retained > limits.maxBytes - defaults.retained)
            throw std::runtime_error("Persistent combined default/state byte budget exceeded");
        if (measured.measuredBudget().nodes > limits.totalValueNodes - defaults.nodes)
            throw std::runtime_error("Persistent combined default/state value-node budget exceeded");
    }
    bool HasStateFrame(RuntimeObject* object) const {
        return object->stateObject && object->stateObject->frameOverride ?
            object->stateObject->frame.has_value() : object->serializedStack.has_value();
    }
    std::string TransitionStateName(RuntimeObject* object) {
        const auto current = CurrentDispatchContext(object, true);
        if (!current.framePresent) return "None";
        if (!current.exactSelected)
            throw std::runtime_error("Ambiguous legacy state control requires an explicit fresh-world reset");
        return LowerAscii(current.selectedPath) == LowerAscii(object->cls->reflection.objectPath) ? "None" : current.stateName;
    }
    QuestVr::StateFrame* ControlFrame(RuntimeObject* object) {
        if (!HasStateFrame(object)) return nullptr;
        if (object->stateObject && object->stateObject->frameOverride) {
            if (!object->stateObject->frame->control)
                throw std::runtime_error("Ambiguous legacy state control requires an explicit fresh-world reset");
            Touch(object); object->committedScriptState = true;
            object->stateObject->frame->latent = QuestVr::StateLatent::Continue;
            return &*object->stateObject->frame;
        }
        const auto current = CurrentDispatchContext(object, true);
        if (!current.exactSelected || current.selectedPath.empty() || current.runningPath.empty() ||
            !StateMetadata(Object(current.runningPath)))
            throw std::runtime_error("Original frame control lacks exact State/Class identities");
        if (object->stateObject) {
            for (const auto& [state, names] : object->stateObject->disabled) {
                if (!names.empty() && !QuestVr::StateNameEqual(state, current.stateName))
                    throw std::runtime_error("Legacy dormant probe overlay has an unrelated state identity");
                for (const auto& name : names)
                    if (!QuestVr::ScriptDispatch::EventProbeIndex(name))
                        throw std::runtime_error("Legacy dormant probe overlay has an unrecognized probe");
            }
        }
        // Only audited clearing operations enter here. Raw native latent IDs
        // (including unknown nonzero stopped-frame values) are cancelled, not
        // guessed into portable latent ordinals. Dormant 117/118 retain them.
        QuestVr::StateFrame frame;
        frame.codePath = current.runningPath;
        frame.control = QuestVr::StateControl{current.selectedPath, false, *current.liveProbeMask};
        QuestVr::StateObject state;
        state.hasStack = state.frameOverride = true; state.frame = std::move(frame);
        Touch(object); object->stateObject = std::move(state); object->committedScriptState = true;
        return &*object->stateObject->frame;
    }
    void InitializeActorExecution(RuntimeObject* actor) {
        if (!actor->cls || !actor->cls->classDescriptor)
            throw std::runtime_error("Actor InitExecution requires its authored Class");
        if (ActorLink(actor, "Level") != NativeLevelInfo(actor))
            throw std::runtime_error("Original Actor InitExecution Level binding differs from native XLevel slot zero");
        if (actor->stateLocalsRevision == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("State local revision budget exceeded");
        Touch(actor);
        QuestVr::StateFrame frame;
        frame.codePath = actor->cls->reflection.objectPath;
        frame.control = QuestVr::StateControl{frame.codePath, false, ~std::uint64_t{0}};
        QuestVr::StateObject state;
        state.hasStack = state.frameOverride = true; state.frame = std::move(frame);
        actor->stateObject = std::move(state); ++actor->stateLocalsRevision;
        actor->stateTransitionFlags = 0u; actor->committedScriptState = true;
        const auto property = PropertyNamed(actor, "ObjectFlags");
        ReferenceFor(actor, property, 0u, false)->write(Value::Integer(
            QuestVr::Vm::ToInt(Read(actor, property, 0u)) | 0x02000000));
        // Continue is a portable policy while Code is null, NOT a claim about
        // the original constructor's unassigned native latent slot. Both state
        // entry and relabel clear that slot before enabling executable code.
    }
    struct StateControlOperations {
        PortableActorVmHost& host;
        RuntimeObject* actor;
        QuestVr::Vm::Execution& execution;
        bool HasFrame() const { return host.HasStateFrame(actor); }
        void CancelLatent() { static_cast<void>(host.ControlFrame(actor)); }
        std::string StateName() { return host.TransitionStateName(actor); }
        QuestVr::StateSelection Resolve(const std::string& requested) {
            const auto selected = QuestVr::ScriptDispatch::ResolveState(DispatchGraph(), actor->cls->reflection.objectPath, requested);
            auto* node = selected ? host.Object(*selected) : actor->cls;
            const auto* metadata = StateMetadata(node);
            if (!metadata) throw std::runtime_error("State selection is not State/Class");
            return {node->reflection.objectPath, selected ? node->reflection.objectPath.substr(node->reflection.objectPath.find_last_of('.') + 1u) : "None",
                (actor->cls->classDescriptor->state.probeMask | metadata->probeMask) & metadata->ignoreMask};
        }
        bool IsProbing(const std::string& event) {
            const auto current = CurrentDispatchContext(actor);
            return QuestVr::ScriptDispatch::IsEnabled(event, current.classProbeMask, current.codeMasks, current.disabledNames, current.liveProbeMask);
        }
        std::uint32_t Flags() const { return actor->stateTransitionFlags; }
        void ClearFlags(const std::uint32_t bits) noexcept { actor->stateTransitionFlags &= ~bits; }
        void SetFlags(const std::uint32_t bits) noexcept { actor->stateTransitionFlags |= bits; }
        void CallEvent(const std::string& event) {
            static_cast<void>(execution.CallEvent(actor->reflection.objectPath, event, true, {}));
        }
        void Publish(const QuestVr::StateSelection& selection) {
            auto& frame = *actor->stateObject->frame;
            frame.codePath = selection.path;
            frame.control->selectedPath = selection.path;
            frame.control->codePresent = false;
            frame.control->liveProbeMask = selection.liveProbeMask;
        }
    };
    QuestVr::StateObject& EnsureStateObject(RuntimeObject* object) {
        Touch(object);
        if (!object->stateObject) {
            const auto authored = AuthoredDispatchContext(object);
            QuestVr::StateObject state;
            state.hasStack = AnyFlags(static_cast<ObjectFlags>(object->reflection.flags), ObjectFlags::HasStack);
            auto& disabled = state.disabled[QuestVr::ScriptDispatch::FoldName(authored.stateName)];
            for (const auto& event : authored.disabledNames) disabled.insert(QuestVr::ScriptDispatch::FoldName(event));
            object->stateObject = std::move(state);
        }
        object->committedScriptState = true;
        return *object->stateObject;
    }
    std::shared_ptr<Reference> StateReference(RuntimeObject* object, const Property& property, const std::size_t index) {
        if (!object->stateObject || !object->stateObject->frameOverride || !object->stateObject->frame)
            throw std::runtime_error("State local has no portable frame");
        const auto revision = object->stateLocalsRevision;
        const auto access = [object, property, index, revision]() -> Value& {
            if (object->stateLocalsRevision != revision || !object->stateObject || !object->stateObject->frame)
                throw std::runtime_error("State local reference outlived its storage");
            auto& locals = object->stateObject->frame->locals;
            const auto found = std::find_if(locals.begin(), locals.end(), [&](const auto& local) {
                return LowerAscii(local.key) == LowerAscii(property.key);
            });
            if (found == locals.end() || found->values.size() != property.arrayDimension || index >= found->values.size())
                throw std::runtime_error("State local storage/schema is unavailable");
            return found->values[index];
        };
        static_cast<void>(access());
        auto reference = std::make_shared<Reference>(); reference->zero = property.zero;
        reference->resultLayout = property.resultLayout;
        reference->dimension = property.arrayDimension; reference->read = [access]() { return access(); };
        reference->write = [this, object, property, access](const Value& value) {
            auto normalized = TypedLiveValue(property, value);
            Touch(object); access() = std::move(normalized); object->committedScriptState = true;
            ValidateStateBudget();
        };
        reference->element = [this, object, property, revision](const std::size_t slot) {
            if (object->stateLocalsRevision != revision || slot >= property.arrayDimension)
                throw std::runtime_error("State local array reference outlived its storage or bounds");
            return StateReference(object, property, slot);
        };
        return reference;
    }
    std::string ObjectProperty(RuntimeObject* object, const char* name) {
        if (const auto* value = FindScriptOverlay(object, name)) return value->text;
        const auto own = object->objectPropertyPaths.find(name);
        if (own != object->objectPropertyPaths.end()) return own->second;
        for (RuntimeObject* cls = object->cls; cls != nullptr; cls = cls->base) {
            const auto found = cls->objectPropertyPaths.find(name);
            if (found != cls->objectPropertyPaths.end()) return found->second;
        }
        return {};
    }
    const PortableMeshAnimationData* AnimationMesh(RuntimeObject* object) {
        const auto path = ObjectProperty(object, "Mesh");
        if (path.empty()) return nullptr;
        RuntimeObject* mesh = Object(path);
        if (!mesh->lodMesh) {
            // Decode immutable authored assets lazily. They are not actor state
            // and may remain cached after a failed script transaction.
            mesh->lodMesh = LoadCompleteRuntimeLodMesh(Table(mesh->sourcePath), mesh->exportIndex, mesh->sourcePath);
        }
        return mesh->lodMesh->animation.get();
    }
    QuestVr::ActorAnimationClock AuthoredClock(RuntimeObject* object) {
        const auto authored = ReadRuntimeAnimationSnapshot(object);
        QuestVr::ActorAnimationClock clock;
        clock.pose.fatness = static_cast<std::uint8_t>(QuestVr::Vm::ToInt(Read(object, PropertyNamed(object, "Fatness"), 0u)));
        clock.remoteRole = static_cast<std::uint8_t>(QuestVr::Vm::ToInt(Read(object, PropertyNamed(object, "RemoteRole"), 0u)));
        clock.pose.main.sequence = authored.sequence; clock.pose.main.normalizedFrame = authored.frame;
        clock.main.rate = authored.rate; clock.main.last = authored.last; clock.main.minRate = authored.minRate;
        clock.main.tweenRate = authored.tweenRate; clock.main.oldRate = authored.oldRate;
        clock.main.loop = authored.loop; clock.main.notify = authored.notify; clock.main.finished = authored.finished;
        for (std::size_t slot = 0u; slot < clock.blends.size(); ++slot) {
            const auto& source = authored.blends[slot];
            clock.pose.blends[slot].sequence = source.sequence; clock.pose.blends[slot].normalizedFrame = source.frame;
            clock.blends[slot].rate = source.rate; clock.blends[slot].last = source.last;
            clock.blends[slot].minRate = source.minRate; clock.blends[slot].tweenRate = source.tweenRate;
            clock.blends[slot].oldRate = source.oldRate;
            const auto simulated = Read(object, PropertyNamed(object, "SimBlendAnim"), slot);
            if (simulated.kind != Kind::Struct || simulated.fields.size() != 4u)
                throw std::runtime_error("Authored SimBlendAnim schema is not a four-field Plane");
            for (std::size_t field = 0u; field < 4u; ++field)
                clock.blends[slot].simulated[field] = QuestVr::Vm::ToFloat(
                    simulated.fields.at(std::array<const char*, 4u>{"x", "y", "z", "w"}[field]));
        }
        return clock;
    }
    void Put(RuntimeObject* object, const char* name, const Value& value, const std::uint32_t index = 0u) {
        object->scriptValues[LowerAscii(name)][index] = value;
    }
    void SynchronizeClock(RuntimeObject* object) {
        const auto& clock = *object->animationClock;
        Put(object, "AnimSequence", Value::Text(Kind::Name, clock.pose.main.sequence));
        Put(object, "AnimFrame", Value::Float(clock.pose.main.normalizedFrame));
        Put(object, "AnimRate", Value::Float(clock.main.rate)); Put(object, "AnimLast", Value::Float(clock.main.last));
        Put(object, "AnimMinRate", Value::Float(clock.main.minRate)); Put(object, "TweenRate", Value::Float(clock.main.tweenRate));
        Put(object, "OldAnimRate", Value::Float(clock.main.oldRate)); Put(object, "bAnimLoop", Value::Bool(clock.main.loop));
        Put(object, "bAnimNotify", Value::Bool(clock.main.notify)); Put(object, "bAnimFinished", Value::Bool(clock.main.finished));
        for (std::uint32_t slot = 0u; slot < clock.blends.size(); ++slot) {
            const auto& channel = clock.pose.blends[slot]; const auto& rates = clock.blends[slot];
            Put(object, "BlendAnimSequence", Value::Text(Kind::Name, channel.sequence), slot);
            Put(object, "BlendAnimFrame", Value::Float(channel.normalizedFrame), slot);
            Put(object, "BlendAnimRate", Value::Float(rates.rate), slot); Put(object, "BlendAnimLast", Value::Float(rates.last), slot);
            Put(object, "BlendAnimMinRate", Value::Float(rates.minRate), slot); Put(object, "BlendTweenRate", Value::Float(rates.tweenRate), slot);
            Put(object, "OldBlendAnimRate", Value::Float(rates.oldRate), slot);
            Put(object, "SimBlendAnim", BlendSimulatedValue(rates.simulated), slot);
        }
    }
    void UpdateClockProperty(RuntimeObject* object, const std::string& name, const std::uint32_t index) {
        if (!object->animationClock) return;
        // Preserve captured tween offsets while explicit script assignments
        // update main/blend properties, including explicit SimBlendAnim writes.
        const auto previous = object->animationClock->pose;
        const bool finishWaiting = object->animationClock->main.finishAnimWaiting;
        const auto updated = AuthoredClock(object);
        object->animationClock->main = updated.main;
        object->animationClock->blends = updated.blends;
        object->animationClock->pose = updated.pose;
        object->animationClock->remoteRole = updated.remoteRole;
        object->animationClock->main.finishAnimWaiting = finishWaiting;
        object->animationClock->pose.main.previous = previous.main.previous;
        for (std::size_t slot = 0u; slot < previous.blends.size(); ++slot) {
            object->animationClock->pose.blends[slot].previous = previous.blends[slot].previous;
        }
        (void)name; (void)index;
    }
    Value Region(RuntimeObject* object, const Value& zero, const float verticalOffset = 0.0f) {
        if (object->sourcePath.empty() || PackageStem(object->sourcePath) != persistentMapPackageName)
            throw std::runtime_error("Actor.Region unavailable outside the current authored map");
        auto* levelInfo = NativeLevelInfo(object);
        // Original ULevel::SetActorZone and UModel::PointRegion use the
        // native slot-zero LevelInfo, not the first reflected LevelInfo export.
        if (object == levelInfo) {
            Value result = zero;
            result.fields.at("zone") = Value::Text(Kind::Object, levelInfo->reflection.objectPath);
            result.fields.at("ileaf") = Value::Integer(-1);
            result.fields.at("zonenumber") = Value::Byte(0u);
            return result;
        }
        if (!rootModel_) rootModel_ = LoadPortableRootModel68(Table(object->sourcePath));
        auto location = Read(object, PropertyNamed(object, "Location"), 0u);
        location.vector[2] += verticalOffset;
        if (location.kind != Kind::Vector || !std::isfinite(location.vector[0]) ||
            !std::isfinite(location.vector[1]) || !std::isfinite(location.vector[2]))
            throw std::runtime_error("Actor.Region has an invalid location");
        std::uint32_t zone{}; std::int32_t leaf{-1};
        if (!rootModel_->nodes.empty()) {
            std::int32_t current{}; bool terminal{};
            for (std::size_t steps = 0u; steps <= rootModel_->nodes.size(); ++steps) {
                if (current < 0 || static_cast<std::size_t>(current) >= rootModel_->nodes.size())
                    throw std::runtime_error("Actor.Region BSP reference out of range");
                const auto& node = rootModel_->nodes[static_cast<std::size_t>(current)];
                const float side = location.vector[0]*node.planeX + location.vector[1]*node.planeY +
                    location.vector[2]*node.planeZ - node.planeW;
                // Equality selects side1. If that child is absent the original
                // stops here; it never falls through to the other child.
                const bool front = side >= 0.0f;
                const auto child = front ? node.front : node.back;
                if (child >= 0) current = child;
                else {
                    zone = static_cast<std::uint32_t>(front ? node.zone1 : node.zone0);
                    leaf = front ? node.leaf1 : node.leaf0; terminal = true; break;
                }
            }
            if (!terminal) throw std::runtime_error("Actor.Region BSP traversal budget exceeded");
        }
        if (rootModel_->zones.empty()) zone = 0u;
        if (zone >= 64u) throw std::runtime_error("Actor.Region zone number out of range");
        RuntimeObject* zoneObject{};
        if (zone < rootModel_->zones.size() && rootModel_->zones[zone].actorReference != 0)
            zoneObject = Object(Qualified(Table(object->sourcePath), rootModel_->zones[zone].actorReference));
        if (zoneObject == nullptr) zoneObject = levelInfo;
        Value result = zero;
        result.fields["zone"] = Value::Text(Kind::Object, zoneObject->reflection.objectPath);
        result.fields["ileaf"] = Value::Integer(leaf); result.fields["zonenumber"] = Value::Byte(static_cast<std::uint8_t>(zone));
        return result;
    }
};


// Runtime saves carry stable authored identities, never process pointers or
// package-local references. Prepare every schema/value before touching actors.
struct PreparedScriptObject {
    RuntimeObject* target{};
    decltype(RuntimeObject::scriptValues) values;
    std::optional<QuestVr::ActorAnimationClock> clock;
    std::optional<QuestVr::StateObject> state;
    std::optional<float> latentTimeLeft;
    std::vector<RuntimeObject*> children, based;
    std::array<bool, 4u> touchSent{};
    bool lifecycle{}, removed{};
};
struct PreparedClassDefaults {
    RuntimeObject* target{};
    decltype(RuntimeObject::classDefaultValues) values;
};
struct PreparedScriptState {
    std::vector<PreparedScriptObject> objects;
    std::vector<PreparedClassDefaults> classDefaults;
    std::vector<GCRoot<RuntimeObject>> births;
    std::vector<GCRoot<RuntimeAIManager>> aiManagers;
    GCRoot<RuntimeMapDecals> nativeDecals;
    std::vector<RuntimeObject*> exports, worldActors;
    decltype(persistentQualifiedObjects) qualified;
    decltype(persistentVmObjects) vm;
    decltype(persistentMapTagIndex) tags;
    QuestVr::ActorCollisionRegistry collision;
    bool replaceBirths{};
};

void StageRuntimeBirthReplacement(PreparedScriptState& prepared) {
    prepared.replaceBirths = true;
    prepared.qualified = persistentQualifiedObjects; prepared.vm = persistentVmObjects;
    for (auto* object : persistentRuntime->get()->exports) {
        if (object->runtimeBirth) {
            pendingBirthGarbage = true;
            prepared.qualified.erase(object->reflection.objectPath); prepared.vm.erase(object->birthVmKey);
        } else prepared.exports.push_back(object);
    }
    prepared.worldActors.assign(persistentWorldActors.begin(),
        persistentWorldActors.begin()+static_cast<std::ptrdiff_t>(persistentAuthoredWorldSlots));
    prepared.worldActors.resize(persistentAuthoredWorldSlots+prepared.births.size(),nullptr);
    std::vector<RuntimeObject*> ordered;
    for (const auto& root : prepared.births) ordered.push_back(root.get());
    std::sort(ordered.begin(),ordered.end(),[](const auto* a,const auto* b) {return a->worldActorIndex<b->worldActorIndex;});
    for (auto* actor : ordered) {
        if (!prepared.qualified.emplace(actor->reflection.objectPath,actor).second ||
            !prepared.vm.emplace(actor->birthVmKey,actor).second)
            throw std::runtime_error("Restored birth identity publication collision");
        prepared.exports.push_back(actor); prepared.worldActors.at(static_cast<std::size_t>(actor->worldActorIndex))=actor;
    }
}

std::vector<QuestVr::ScriptSavedProperty> ClockProperties(
    const QuestVr::ActorAnimationClock& clock) {
    using V = QuestVr::Vm::Value;
    using K = QuestVr::Vm::Kind;
    std::vector<QuestVr::ScriptSavedProperty> values;
    const auto add = [&](const char* name, V value, const std::uint32_t index = 0u) {
        values.push_back({{}, name, index, std::move(value)});
    };
    add("AnimSequence", V::Text(K::Name, clock.pose.main.sequence));
    add("AnimFrame", V::Float(clock.pose.main.normalizedFrame));
    add("AnimRate", V::Float(clock.main.rate)); add("AnimLast", V::Float(clock.main.last));
    add("AnimMinRate", V::Float(clock.main.minRate)); add("TweenRate", V::Float(clock.main.tweenRate));
    add("OldAnimRate", V::Float(clock.main.oldRate)); add("bAnimLoop", V::Bool(clock.main.loop));
    add("bAnimNotify", V::Bool(clock.main.notify)); add("bAnimFinished", V::Bool(clock.main.finished));
    add("Fatness", V::Byte(clock.pose.fatness)); add("RemoteRole", V::Byte(clock.remoteRole));
    for (std::uint32_t slot = 0u; slot < clock.blends.size(); ++slot) {
        const auto& rates = clock.blends[slot];
        add("BlendAnimSequence", V::Text(K::Name, clock.pose.blends[slot].sequence), slot);
        add("BlendAnimFrame", V::Float(clock.pose.blends[slot].normalizedFrame), slot);
        add("BlendAnimRate", V::Float(rates.rate), slot); add("BlendAnimLast", V::Float(rates.last), slot);
        add("BlendAnimMinRate", V::Float(rates.minRate), slot); add("BlendTweenRate", V::Float(rates.tweenRate), slot);
        add("OldBlendAnimRate", V::Float(rates.oldRate), slot);
        add("SimBlendAnim", BlendSimulatedValue(rates.simulated), slot);
    }
    return values;
}

QuestVr::ScriptSavedState CollectScriptSavedState() {
    QuestVr::ScriptSavedState saved;
    saved.mapName = persistentMapPackageName;
    saved.randomSeed = persistentScriptRandomSeed;
    PortableActorVmHost host;
    // Bound the additional snapshot before copying nested live values. The
    // codec independently measures the actual encoded/retained representation.
    QuestVr::ScriptStateLimits limits;
    limits.maxBytes = QuestVr::kMaximumSaveRuntimeBytes;
    QuestVr::ScriptStateDetail::Budget capture{limits};
    if (const auto* graph = persistentRuntime->get()->nativeDecals) {
        capture.Retain(NativeDecalRetainedBytes(*graph));
        if (!graph->model || !graph->level) throw std::runtime_error("Script save native decal binding is missing");
        capture.Retain(saved.mapName.size() + graph->model->reflection.objectPath.size() +
            graph->level->reflection.objectPath.size() + 3u);
        QuestVr::ScriptSavedNativeDecals record;
        record.mapName = saved.mapName;
        record.modelPath = graph->model->reflection.objectPath;
        record.levelPath = graph->level->reflection.objectPath;
        record.surfaceCount = static_cast<std::uint32_t>(graph->state.Usage().surfaces);
        record.nodeCount = static_cast<std::uint32_t>(graph->state.NodeCount());
        record.state = graph->state.ReadOnlySnapshot();
        saved.nativeDecals.emplace(std::move(record));
    }
    const auto measure = [&](const auto& self, const QuestVr::Vm::Value& value, const std::size_t depth) -> void {
        capture.Node(depth); capture.Retain(sizeof(value));
        capture.IntArrayElements(value.intArray.size());
        capture.Retain(QuestVr::DecalStateDetail::Multiply(value.intArray.capacity(), sizeof(std::int32_t)));
        if (value.text.size() > limits.maxStringBytes)
            throw std::runtime_error("Script save live string budget exceeded");
        capture.Retain(value.text.size() + 1u);
        // Count all stored fields, including inactive Value members, before a
        // copy. The codec only emits the payload selected by the concrete kind.
        for (const auto& [name, member] : value.fields) {
            if (name.size() > limits.maxStringBytes)
                throw std::runtime_error("Script save live field budget exceeded");
            capture.Retain(sizeof(std::string) + 5u * sizeof(void*) + name.size() + 1u);
            self(self, member, depth + 1u);
        }
    };
    for (auto* object : persistentRuntime->get()->exports) {
        if (object->nativeAiManager) {
            const auto& state = object->nativeAiManager->state;
            // Measure the complete graph before its first snapshot copy.
            QuestVr::ScriptStateDetail::Writer aiMeasure(limits, nullptr);
            aiMeasure.MeasureAiManager(state);
            capture.Retain(aiMeasure.measuredBudget().retained);
            saved.aiManagers.push_back(state);
        }
        if (!object->classDefaultValues.empty()) {
            if (saved.objects.size() + saved.classDefaults.size() >= limits.maxObjects)
                throw std::runtime_error("Script save default/actor count exceeds budget");
            auto* cls = host.DefaultClass(object);
            if (cls != object) throw std::runtime_error("Script save defaults are not owned by their concrete class");
            QuestVr::ScriptSavedClassDefaults record;
            capture.Retain(sizeof(record) + cls->reflection.objectPath.size() + 1u);
            record.classPath = cls->reflection.objectPath;
            for (const auto& [name, slots] : cls->classDefaultValues) {
                const auto property = host.ClassProperty(cls, name);
                for (const auto& [index, value] : slots) {
                    capture.Properties(1u);
                    capture.Retain(sizeof(QuestVr::ScriptSavedProperty) + property.key.size() + property.name.size() + 2u);
                    measure(measure, value, 0u);
                    record.properties.push_back({property.key, property.name, index, value});
                }
            }
            saved.classDefaults.push_back(std::move(record));
        }
        if (object->runtimeBirth) {
            QuestVr::ScriptSavedBirth birth;
            birth.path = object->reflection.objectPath; birth.classPath = object->cls->reflection.objectPath;
            birth.worldActorIndex = static_cast<std::uint32_t>(object->worldActorIndex);
            capture.Retain(sizeof(birth) + birth.path.size() + birth.classPath.size() + 2u);
            for (const auto& [name, slots] : object->frozenBirthDefaults) {
                const auto property = host.PropertyNamed(object, name);
                for (const auto& [index, value] : slots) {
                    capture.Properties(1u);
                    capture.Retain(sizeof(QuestVr::ScriptSavedProperty) + property.key.size() + property.name.size() + 2u);
                    measure(measure, value, 0u);
                    birth.frozenDefaults.push_back({property.key,property.name,index,value});
                }
            }
            saved.births.push_back(std::move(birth));
        }
        if (!object->committedScriptState) continue;
        if (saved.objects.size() + saved.classDefaults.size() >= limits.maxObjects || object->cls == nullptr)
            throw std::runtime_error("Script save object budget or class is invalid");
        QuestVr::ScriptSavedObject record;
        capture.Retain(sizeof(record) + object->reflection.objectPath.size() + object->cls->reflection.objectPath.size() + 2u);
        record.path = object->reflection.objectPath;
        record.classPath = object->cls->reflection.objectPath;
        record.clock = object->animationClock;
        record.latentTimeLeft = object->latentTimeLeft;
        if (object->committedLifecycle) {
            record.lifecycle.emplace();
            auto& lifecycle = *record.lifecycle;
            lifecycle.worldRemoved = object->worldRemoved;
            lifecycle.touchEventSent = object->touchEventSent;
            capture.Retain(sizeof(lifecycle));
            capture.ActorLinks(object->childActors.size() + object->basedActors.size());
            const auto links = [&](const std::vector<RuntimeObject*>& source, std::vector<std::string>& target) {
                capture.Array(source.size(), sizeof(std::string));
                for (const auto* actor : source) {
                    QuestVr::ScriptStateDetail::Text(actor->reflection.objectPath, limits, true);
                    capture.Retain(actor->reflection.objectPath.size() + 1u);
                    target.push_back(actor->reflection.objectPath);
                }
            };
            links(object->childActors, lifecycle.children); links(object->basedActors, lifecycle.basedActors);
        }
        if (object->stateObject) {
            const auto& state = *object->stateObject;
            capture.Retain(sizeof(state));
            const auto retainIdentity = [&](const std::string& value, const bool empty = false) {
                QuestVr::ScriptStateDetail::Text(value, limits, true, empty);
                capture.Retain(value.size() + 1u);
            };
            if (state.frame) {
                const auto& frame = *state.frame;
                capture.Retain(sizeof(frame));
                retainIdentity(frame.codePath, true); retainIdentity(frame.localsCodePath, true);
                if (frame.control) retainIdentity(frame.control->selectedPath);
                capture.StateLocals(frame.locals.size());
                capture.Array(frame.locals.size(), sizeof(QuestVr::StateLocal));
                for (const auto& local : frame.locals) {
                    retainIdentity(local.key);
                    capture.LocalElements(local.values.size());
                    capture.Array(local.values.size(), sizeof(QuestVr::Vm::Value));
                    for (const auto& value : local.values) measure(measure, value, 0u);
                }
            }
            capture.DisabledStates(state.disabled.size());
            capture.Array(state.disabled.size(), sizeof(decltype(state.disabled)::value_type) + 5u * sizeof(void*));
            for (const auto& [stateName, names] : state.disabled) {
                retainIdentity(stateName);
                capture.DisabledNames(names.size());
                capture.Array(names.size(), sizeof(std::string) + 5u * sizeof(void*));
                for (const auto& name : names) retainIdentity(name);
            }
            record.state = state;
        }
        for (const auto& [name, slots] : object->scriptValues) {
            const auto property = name.find('.')==std::string::npos?host.PropertyNamed(object,name):host.DescribeProperty(name);
            if(!host.OwnsProperty(object->cls,property) || host.InstanceStorageKey(object->cls,property)!=name)
                throw std::runtime_error("Script save instance overlay has an invalid qualified storage identity");
            for (const auto& [index, value] : slots) {
                if (record.properties.size() >= 65'536u)
                    throw std::runtime_error("Script save property budget exceeded");
                capture.Properties(1u);
                capture.Retain(sizeof(QuestVr::ScriptSavedProperty) + property.key.size() + property.name.size() + 2u);
                measure(measure, value, 0u);
                record.properties.push_back({property.key, property.name, index, value});
            }
        }
        // Clock and reflected fields are two views of the same native state.
        // Carry untouched channels too, so restore never silently substitutes
        // current class defaults for a saved clock. Transient tween history is
        // retained solely in the clock, not fabricated as an Unreal property.
        if (record.clock) {
            for (auto value : ClockProperties(*record.clock)) {
                const auto property = host.PropertyNamed(object, value.name);
                value.key = property.key; value.name = property.name;
                const auto found = std::find_if(record.properties.begin(), record.properties.end(),
                    [&](const auto& prior) { return LowerAscii(prior.name) == LowerAscii(value.name) && prior.index == value.index; });
                if (found == record.properties.end()) {
                    capture.Properties(1u);
                    capture.Retain(sizeof(value) + value.key.size() + value.name.size() + 2u);
                    measure(measure, value.value, 0u);
                    record.properties.push_back(std::move(value));
                }
                else if (!QuestVr::Vm::Equal(found->value, value.value))
                    throw std::runtime_error("Script save clock and property disagree");
            }
        }
        saved.objects.push_back(std::move(record));
    }
    return saved;
}

class ScriptSaveSchema {
public:
    explicit ScriptSaveSchema(const std::string& mapName) : mapName_(mapName) {
        SafePackage(mapName_);
        auto* actor = host_.Object("Engine.Actor");
        gameRoot_ = std::filesystem::path(actor->sourcePath).parent_path().parent_path().string();
        const auto path = ResolveRuntimePackagePath(gameRoot_, mapName_);
        if (path.empty() || LowerAscii(std::filesystem::path(path).extension().string()) != ".dx")
            throw std::runtime_error("Script save map is unavailable");
        mapPath_ = path;
        const auto& map = Table(path);
        if (map.exports.size() > 1'000'000u) throw std::runtime_error("Script save map export budget exceeded");
        for (std::size_t i = 0u; i < map.exports.size(); ++i) {
            const auto objectPath = Qualified(map, static_cast<std::int32_t>(i + 1u));
            const auto classPath = Qualified(map, map.exports[i].ObjClass);
            Retain(2u * objectPath.size() + classPath.size() + 3u * sizeof(std::string) + sizeof(std::size_t) + 10u * sizeof(void*));
            if (!mapClasses_.emplace(LowerAscii(objectPath), classPath).second)
                throw std::runtime_error("Script save map contains ambiguous identities");
            mapExports_.emplace(LowerAscii(objectPath), i);
        }
    }
    const std::string& MapSource() const { return mapPath_; }
    static std::string MoverMapName(const std::string& path) {
        const auto separator = path.find('.');
        if (separator == std::string::npos || separator + 1u == path.size())
            throw std::runtime_error("Mover checkpoint owner has no map identity");
        auto name = path.substr(0u, separator);
        SafePackage(name);
        return name;
    }
    void ReadAuthoredMover(const std::string& path, RuntimeObject& actor) {
        const auto& map = Table(mapPath_);
        const auto found = mapExports_.find(LowerAscii(path));
        if (found == mapExports_.end() ||
            path != Qualified(map, static_cast<std::int32_t>(found->second + 1u)))
            throw std::runtime_error("Mover checkpoint owner is not a canonical authored map export");
        if (!nativeActorOrder_) {
            auto order = ReadPortableLevel68ActorOrder(map, SchemaScratchBytes);
            Retain(order.size() * sizeof(std::int32_t));
            for (const auto reference : order)
                if (reference < 0 || static_cast<std::size_t>(reference) > map.exports.size())
                    throw std::runtime_error("Mover checkpoint Level slot is not a local export");
            std::sort(order.begin(), order.end());
            nativeActorOrder_ = std::move(order);
        }
        if (!std::binary_search(nativeActorOrder_->begin(), nativeActorOrder_->end(),
                static_cast<std::int32_t>(found->second + 1u)))
            throw std::runtime_error("Mover checkpoint owner has no native Level binding");
        auto* cls = host_.Object(mapClasses_.at(LowerAscii(path)));
        if (!cls->classDescriptor || cls->reflection.metaClass != "Class" ||
            !IsDerivedFromPath(cls, "Engine.Mover") ||
            LowerAscii(std::filesystem::path(cls->sourcePath).extension().string()) == ".dx")
            throw std::runtime_error("Mover checkpoint owner does not have a loaded script Mover class");
        const auto& entry = map.exports.at(found->second);
        actor.reflection.objectPath = path;
        actor.reflection.metaClass = cls->reflection.objectPath.substr(cls->reflection.objectPath.find_last_of('.') + 1u);
        actor.reflection.flags = static_cast<std::uint32_t>(entry.ObjFlags);
        actor.reflection.serializedSize = entry.ObjSize;
        actor.cls = cls;
        actor.sourcePath = mapPath_;
        actor.exportIndex = found->second;
        auto properties = LoadPortableExportProperties(map, found->second, SchemaScratchBytes);
        actor.instanceProperties = std::move(properties.properties);
        CacheRuntimeNameProperties(&actor, map, actor.instanceProperties);
    }
    const std::string& NativeLevelPath() {
        if (!nativeLevelPath_.empty()) return nativeLevelPath_;
        const auto& map = Table(mapPath_);
        std::string path;
        for (std::size_t i = 0u; i < map.exports.size(); ++i) {
            if (LowerAscii(Qualified(map, map.exports[i].ObjClass)) != "engine.level") continue;
            if (!path.empty()) throw std::runtime_error("Script save map has multiple native Levels");
            path = Qualified(map, static_cast<std::int32_t>(i + 1u));
        }
        if (path.empty()) throw std::runtime_error("Script save map has no serialized native Level");
        Retain(path.size() + 1u);
        nativeLevelPath_ = std::move(path);
        return nativeLevelPath_;
    }
    void ValidateAI(const QuestVr::Ai::State& state) {
        QuestVr::Ai::Validate(state);
        if (state.processDepth != 0u)
            throw std::runtime_error("Script save AI manager is inside an active processing pass");
        if (LowerAscii(state.levelPath) != LowerAscii(NativeLevelPath()))
            throw std::runtime_error("Script save AI manager native Level identity is invalid");
        const auto owner = mapClasses_.find(LowerAscii(state.ownerPath));
        if (owner == mapClasses_.end() || !ClassDerives(owner->second, "Engine.LevelInfo"))
            throw std::runtime_error("Script save AI manager owner is not a map LevelInfo");
        if (!nativeActorOrder_) {
            auto order = ReadPortableLevel68ActorOrder(Table(mapPath_), SchemaScratchBytes);
            Retain(order.size() * sizeof(std::int32_t));
            for (const auto reference : order)
                if (reference < 0 || static_cast<std::size_t>(reference) > Table(mapPath_).exports.size())
                    throw std::runtime_error("Script save AI Level slot is not a local export");
            std::sort(order.begin(), order.end());
            nativeActorOrder_ = std::move(order);
        }
        const auto linked = [&](const std::string& path) {
            if (IsBirth(path)) return true;
            const auto found = mapExports_.find(LowerAscii(path));
            return found != mapExports_.end() && std::binary_search(nativeActorOrder_->begin(),
                nativeActorOrder_->end(), static_cast<std::int32_t>(found->second + 1u));
        };
        if (!linked(state.ownerPath))
            throw std::runtime_error("Script save AI owner has no native Level binding");
        const auto actor = [&](const std::string& path, const bool optional = false) {
            if (optional && path.empty()) return;
            const auto found = mapClasses_.find(LowerAscii(path));
            if (found == mapClasses_.end() || !ClassDerives(found->second, "Engine.Actor") || !linked(path))
                throw std::runtime_error("Script save AI reference is not an actor in its map");
        };
        for (const auto& sender : state.senders) actor(sender.actor);
        for (const auto& receiver : state.receivers) {
            actor(receiver.actor); actor(receiver.previousBestActor, true); actor(receiver.params.bestActor, true);
        }
    }
    void RegisterBirths(const std::vector<QuestVr::ScriptSavedBirth>& births) {
        const auto& map = Table(mapPath_);
        const bool realLevel = std::any_of(map.exports.begin(),map.exports.end(),[&](const auto& entry) {
            return LowerAscii(Qualified(map,entry.ObjClass)) == "engine.level";
        });
        std::size_t baseline{};
        if (realLevel) baseline = ReadPortableLevel68ActorOrder(map, SchemaScratchBytes).size();
        else for (const auto& entry : map.exports) {
            const auto found = persistentVmObjects.find(LowerAscii(Qualified(map,entry.ObjClass)));
            if (found != persistentVmObjects.end() && IsDerivedFromPath(found->second,"Engine.Actor")) ++baseline;
        }
        std::set<std::uint32_t> indices;
        for (const auto& birth : births) {
            auto* cls = DefaultClass(birth.classPath);
            if (!IsDerivedFromPath(cls,"Engine.Actor") || (cls->classDescriptor->classFlags & 1u) ||
                birth.worldActorIndex < baseline || birth.worldActorIndex - baseline >= births.size() ||
                !indices.emplace(birth.worldActorIndex).second)
                throw std::runtime_error("Script save birth class or appended Level slot is invalid");
            const auto key = LowerAscii(birth.path);
            Retain(2u * key.size() + cls->reflection.objectPath.size() +
                3u * sizeof(std::string) + sizeof(RuntimeObject*) + 10u * sizeof(void*));
            if (key.compare(0u,mapName_.size()+1u,LowerAscii(mapName_)+'.') != 0 ||
                !mapClasses_.emplace(key,cls->reflection.objectPath).second)
                throw std::runtime_error("Script save birth collides with authored or other actor identity");
            birthClasses_.emplace(key,cls);
        }
    }
    void ValidateNativeDecals(const QuestVr::ScriptSavedState& saved) {
        if (!saved.nativeDecals) return;
        const auto& graph = *saved.nativeDecals;
        Retain(QuestVr::DecalCheckpointRetainedBytes(graph));
        const auto& map = Table(mapPath_);
        const auto modelIndex = FindPortableRootModel68Export(map, SchemaScratchBytes);
        if (graph.mapName != mapName_ || graph.levelPath != NativeLevelPath() ||
            graph.modelPath != Qualified(map, static_cast<std::int32_t>(modelIndex + 1u)))
            throw std::runtime_error("Script save native decal map/Level/root Model identity is invalid");
        // Property parsing has its own capacity-aware scratch bound. Native
        // geometry additionally retains up to4x decoded records,8x light-list
        // terminator scratch and its body copy.16x the complete export plus
        // fixed handles conservatively bounds this separate decode phase.
        const auto modelBytes = map.exports.at(modelIndex).ObjSize;
        if (modelBytes <= 0) throw std::runtime_error("Script save native decal Model export is empty");
        constexpr std::size_t modelOverhead = sizeof(PortableModelGeometry) + 1024u;
        if (QuestVr::DecalStateDetail::Multiply(static_cast<std::size_t>(modelBytes), 16u) >
                SchemaScratchBytes - modelOverhead)
            throw std::runtime_error("Script save native decal Model scratch budget exceeded");
        const auto model = LoadPortableModel68(map, modelIndex, SchemaScratchBytes);
        if (graph.surfaceCount != model.surfaces.size() || graph.nodeCount != model.nodes.size())
            throw std::runtime_error("Script save native decal Model counts disagree with authored geometry");
        if (!nativeActorOrder_) {
            auto order = ReadPortableLevel68ActorOrder(map, SchemaScratchBytes);
            Retain(order.size() * sizeof(std::int32_t));
            for (const auto ref : order)
                if (ref < 0 || static_cast<std::size_t>(ref) > map.exports.size())
                    throw std::runtime_error("Script save decal native Level slot is not a local export");
            std::sort(order.begin(), order.end()); nativeActorOrder_ = std::move(order);
        }
        // Validate once per symbolic owner; repeated records remain ordered and
        // are not collapsed. Canonical spelling prevents aliases becoming two
        // opaque native owner identities for one actual UObject.
        std::set<std::string> checked;
        std::map<std::string, const QuestVr::ScriptSavedBirth*> births;
        std::map<std::string, const QuestVr::ScriptSavedObject*> actors;
        for (const auto& birth : saved.births) {
            Retain(birth.path.size() + sizeof(std::string) + sizeof(void*) * 6u);
            births.emplace(LowerAscii(birth.path), &birth);
        }
        for (const auto& actor : saved.objects) {
            Retain(actor.path.size() + sizeof(std::string) + sizeof(void*) * 6u);
            actors.emplace(LowerAscii(actor.path), &actor);
        }
        const auto ownerClass = [&](const std::string& path) -> RuntimeObject* {
            const auto key = LowerAscii(path);
            const auto found = mapClasses_.find(key);
            if (found == mapClasses_.end() || !ClassDerives(found->second, "Engine.Decal"))
                throw std::runtime_error("Script save native decal owner is not a Decal in its map");
            const auto born = births.find(key);
            if (born != births.end()) {
                if (born->second->path != path) throw std::runtime_error("Script save decal birth owner is not canonical");
            } else {
                const auto index = mapExports_.at(key);
                if (path != Qualified(map, static_cast<std::int32_t>(index + 1u)) ||
                    !std::binary_search(nativeActorOrder_->begin(), nativeActorOrder_->end(),
                        static_cast<std::int32_t>(index + 1u)))
                    throw std::runtime_error("Script save native decal owner has no canonical native Level binding");
            }
            auto* cls = host_.Object(found->second);
            const auto property = host_.ClassProperty(cls, "SurfList");
            if (LowerAscii(property.key) != "engine.decal.surflist" || property.flags != 0x00400002u ||
                property.arrayDimension != 1u || property.zero.kind != QuestVr::Vm::Kind::IntArray)
                throw std::runtime_error("Script save native decal SurfList schema is not original GOTY");
            return cls;
        };
        for (std::size_t surface = 0u; surface < graph.state.surfaces.size(); ++surface)
            for (const auto& record : graph.state.surfaces[surface]) {
                if (checked.emplace(record.ownerIdentity).second) {
                    Retain(record.ownerIdentity.size() + sizeof(std::string) + sizeof(void*) * 5u);
                    static_cast<void>(ownerClass(record.ownerIdentity));
                }
                for (const auto node : record.nodeMembership)
                    if (node < 0 || static_cast<std::size_t>(node) >= model.nodes.size() ||
                        model.nodes[static_cast<std::size_t>(node)].surface != static_cast<std::int32_t>(surface))
                        throw std::runtime_error("Script save native decal membership node belongs to another surface");
            }
        for (const auto& owner : graph.state.owners) {
            auto* cls = ownerClass(owner.ownerIdentity);
            const auto property = host_.ClassProperty(cls, "SurfList");
            const auto effective = [&]() -> QuestVr::Vm::Value {
                const auto slot = [&](const std::vector<QuestVr::ScriptSavedProperty>& values) -> const QuestVr::ScriptSavedProperty* {
                    for (const auto& value : values) if (LowerAscii(value.name) == "surflist" && value.index == 0u) return &value;
                    return nullptr;
                };
                const auto actor = actors.find(LowerAscii(owner.ownerIdentity));
                if (actor != actors.end())
                    if (const auto* value = slot(actor->second->properties)) return PropertyValue(cls, *value);
                const auto born = births.find(LowerAscii(owner.ownerIdentity));
                if (born != births.end()) {
                    if (const auto* value = slot(born->second->frozenDefaults)) return PropertyValue(cls, *value);
                } else {
                    const auto index = mapExports_.at(LowerAscii(owner.ownerIdentity));
                    const auto tags = LoadPortableExportProperties(map, index, SchemaScratchBytes);
                    for (auto tag = tags.properties.rbegin(); tag != tags.properties.rend(); ++tag)
                        if (tag->name == "SurfList" && tag->arrayIndex == 0u) {
                            if (tag->type != 9u) throw std::runtime_error("Authored decal SurfList tag has the wrong type");
                            return QuestVr::Vm::Value::IntArray(QuestVr::DecodeAuthoredIntArray(tag->value));
                        }
                }
                for (auto* source = cls; source; source = source->base)
                    if (source->classDescriptor)
                        for (auto tag = source->classDescriptor->defaults.rbegin(); tag != source->classDescriptor->defaults.rend(); ++tag)
                            if (tag->name == "SurfList" && tag->arrayIndex == 0u) {
                                if (tag->type != 9u) throw std::runtime_error("Authored decal default SurfList tag has the wrong type");
                                return QuestVr::Vm::Value::IntArray(QuestVr::DecodeAuthoredIntArray(tag->value));
                            }
                return property.zero;
            }();
            if (effective.intArray != owner.surfaces)
                throw std::runtime_error("Script save native owner mirror and reflected SurfList disagree");
        }
    }
    bool IsBirth(const std::string& path) const { return birthClasses_.count(LowerAscii(path)) != 0u; }
    RuntimeObject* SavedClass(const QuestVr::ScriptSavedObject& record) {
        // ObjectProperty values may refer to a class-owned default block, but
        // saved actor records must be members of this map's authored/birth set.
        // Neither Actor ancestry nor an asset/UClass identity creates a slot.
        if (!mapClasses_.count(LowerAscii(record.path)))
            throw std::runtime_error("Script save object record is not an actor in its map");
        const auto actual = ObjectClass(record.path, false);
        if (LowerAscii(actual) != LowerAscii(record.classPath))
            throw std::runtime_error("Script save actor class does not match authored object");
        auto* cls = host_.Object(actual);
        if (!IsDerivedFromPath(cls, "Engine.Actor"))
            throw std::runtime_error("Script save target is not an actor");
        return cls;
    }
    RuntimeObject* DefaultClass(const std::string& classPath) {
        auto* object = host_.Object(classPath);
        if (host_.DefaultClass(object) != object)
            throw std::runtime_error("Script save default target is not an actual script Actor UClass");
        return object;
    }
    void ValidateLifecycle(const QuestVr::ScriptSavedObject& record) {
        if (!record.lifecycle) return;
        if (!mapClasses_.count(LowerAscii(record.path)))
            throw std::runtime_error("Script save lifecycle target is not a map actor");
        const auto link = [&](const std::string& path) {
            const auto found = mapClasses_.find(LowerAscii(path));
            if (found == mapClasses_.end() || !ClassDerives(found->second, "Engine.Actor"))
                throw std::runtime_error("Script save native link is not an actor in its authored map");
        };
        for (const auto& path : record.lifecycle->children) link(path);
        for (const auto& path : record.lifecycle->basedActors) link(path);
        const auto touching = host_.ClassProperty(SavedClass(record), "Touching");
        const auto* metadata = host_.Object(touching.key);
        if (touching.arrayDimension != 4u || touching.zero.kind != QuestVr::Vm::Kind::Object ||
            !metadata->property || metadata->property->type != "ObjectProperty" ||
            LowerAscii(Qualified(Table(metadata->sourcePath), metadata->property->referencedType)) != "engine.actor")
            throw std::runtime_error("Script save native touching schema is not GOTY Actor.Touching[4]");
        // Ordered lists may legitimately contain duplicates after reentrant
        // callbacks. Script may directly overwrite Owner/Base/Touching/delete
        // flags; do not replace the pin with an invented normalized tree.
    }
    QuestVr::Vm::Value PropertyValue(RuntimeObject* cls, const QuestVr::ScriptSavedProperty& saved,const bool instance=false) {
        const auto property = instance?host_.DescribeProperty(saved.key):host_.ClassProperty(cls, saved.name);
        if (LowerAscii(property.key) != LowerAscii(saved.key) ||
            LowerAscii(property.name) != LowerAscii(saved.name) || saved.index >= property.arrayDimension ||
            (instance && !host_.OwnsProperty(cls,property)))
            throw std::runtime_error("Script save property identity or fixed array index is invalid");
        return TypedValue(property, saved.value);
    }
    QuestVr::StateObject StateValue(RuntimeObject* cls, const QuestVr::ScriptSavedObject& saved) {
        const auto& state = *saved.state;
        const auto exported = mapExports_.find(LowerAscii(saved.path));
        if (exported == mapExports_.end() && !IsBirth(saved.path)) throw std::runtime_error("Script save state actor is unavailable in its map");
        const bool authoredHasStack = exported != mapExports_.end() &&
            AnyFlags(Table(mapPath_).exports.at(exported->second).ObjFlags, ObjectFlags::HasStack);
        // HasStack is established by authored load or owned InitExecution. A
        // portable override must not erase an existing object's stack flag.
        if (authoredHasStack && !state.hasStack)
            throw std::runtime_error("Script save state clears an authored HasStack flag");
        if (!state.frameOverride && (state.frame || state.hasStack != authoredHasStack))
            throw std::runtime_error("Script save dormant state context does not match its authored stack");
        QuestVr::StateObject normalized = state;
        normalized.disabled.clear();
        // Eligibility accepts authored Name spellings, whereas the mutable
        // Enable/Disable sets use folded keys. Normalize before EVERY return,
        // including dormant and cleared frames, so restoring mixed-case names
        // cannot leave an event in an unreachable second disabled set.
        for (const auto& [stateName, names] : state.disabled) {
            const auto inserted = normalized.disabled.emplace(QuestVr::ScriptDispatch::FoldName(stateName), std::set<std::string>{});
            if (!inserted.second) throw std::runtime_error("Script save disabled-state identity is duplicated");
            for (const auto& name : names)
                if (!inserted.first->second.emplace(QuestVr::ScriptDispatch::FoldName(name)).second)
                    throw std::runtime_error("Script save disabled-event identity is duplicated");
        }
        if (!state.frame) return normalized;
        const auto& frame = *state.frame;
        const bool modern = frame.control.has_value();
        if (modern && (!state.hasStack || !state.frameOverride || !state.disabled.empty() ||
            frame.latent == QuestVr::StateLatent::Stop))
            throw std::runtime_error("Script save modern control conflicts with legacy frame data");
        if (frame.latent != QuestVr::StateLatent::Continue && frame.latent != QuestVr::StateLatent::Stop &&
            frame.latent != QuestVr::StateLatent::Sleep && frame.latent != QuestVr::StateLatent::WaitForLanding)
            throw std::runtime_error("Script save state latent action is not implemented by this runtime");
        if (frame.latent == QuestVr::StateLatent::WaitForLanding && !IsDerivedFromPath(cls,"Engine.Pawn"))
            throw std::runtime_error("Script save landing receiver is not Engine.Pawn");
        if ((frame.latent == QuestVr::StateLatent::Sleep || frame.latent == QuestVr::StateLatent::WaitForLanding) && !saved.latentTimeLeft)
            throw std::runtime_error("Script save waiting actor has no native latent timer");
        if (!modern && frame.codePath.empty()) {
            if (!frame.localsCodePath.empty() || !frame.locals.empty())
                throw std::runtime_error("Script save cleared state frame retains local storage");
            // Pinned SetState(null) preserves the old PC and latent state.
            return normalized;
        }
        RuntimeObject* code{};
        if (modern) {
            auto* selected = StateNodeDefinition(cls, frame.control->selectedPath);
            code = StateNodeDefinition(cls, frame.codePath);
            normalized.frame->control->selectedPath = selected->reflection.objectPath;
            normalized.frame->codePath = code->reflection.objectPath;
            if (frame.control->codePresent) {
                if (!code->stateDescriptor)
                    throw std::runtime_error("Script save runnable Class code is not implemented");
                const auto ancestry = QuestVr::ScriptDispatch::SelectedNodeAncestry(DispatchGraph(),
                    cls->reflection.objectPath, selected->reflection.objectPath);
                if (std::none_of(ancestry.begin(), ancestry.end(), [&](const auto& path) {
                    return LowerAscii(path) == LowerAscii(code->reflection.objectPath);
                })) throw std::runtime_error("Script save running Node is outside selected-node ancestry");
            }
            if (frame.localsCodePath.empty()) {
                if (!frame.locals.empty())
                    throw std::runtime_error("Script save modern state lacks its retained local schema");
                if (frame.control->codePresent) {
                    const auto program = host_.StateProgram(code);
                    if (!program->variables.empty())
                        throw std::runtime_error("Script save runnable state requires absent persistent-local storage");
                    const auto layout = QuestVr::Vm::AnalyzeProgram(host_, *program);
                    if (frame.statementIndex > layout.statementOffsets.size())
                        throw std::runtime_error("Script save running state PC exceeds the program's terminal ordinal");
                }
                return normalized; // Null Code retains an unconstrained stale ordinal.
            }
        }
        if (!state.hasStack || frame.localsCodePath.empty())
            throw std::runtime_error("Script save executable state has no HasStack flag or local schema");
        if (!modern) code = StateDefinition(cls, frame.codePath);
        auto* localsCode = StateDefinition(cls, frame.localsCodePath);
        const auto leaf = [](const std::string& path) { return LowerAscii(path.substr(path.find_last_of('.') + 1u)); };
        if (!modern && leaf(code->reflection.objectPath) != leaf(localsCode->reflection.objectPath))
            throw std::runtime_error("Script save executable/local state names disagree");
        const auto localProgram = host_.StateProgram(localsCode);
        if ((!modern && frame.latent != QuestVr::StateLatent::Stop) || (modern && frame.control->codePresent)) {
            const auto layout = QuestVr::Vm::AnalyzeProgram(host_, *host_.StateProgram(code));
            if (frame.statementIndex > layout.statementOffsets.size())
                throw std::runtime_error("Script save running state PC exceeds the program's terminal ordinal");
        }
        // A committed Return can leave the next PC at the end ordinal. Retain
        // it exactly; a subsequent resume still diagnoses the missing next
        // statement rather than rewriting the PC or inventing a Stop action.
        // Stop deliberately permits a stale position: missing-label Stop and
        // SetState preserve the PC rather than substituting a fabricated zero.
        if (frame.locals.size() != localProgram->variables.size())
            throw std::runtime_error("Script save state local schema is incomplete");
        std::unordered_map<std::string, const QuestVr::Vm::Property*> properties;
        properties.reserve(localProgram->variables.size());
        for (const auto& property : localProgram->variables) {
            if (!properties.emplace(LowerAscii(property.key), &property).second)
                throw std::runtime_error("Script save state local metadata has duplicate identities");
        }
        std::unordered_set<std::string> seen;
        for (auto& local : normalized.frame->locals) {
            const auto key = LowerAscii(local.key);
            const auto property = properties.find(key);
            if (property == properties.end() || !seen.emplace(key).second ||
                local.values.size() != property->second->arrayDimension)
                throw std::runtime_error("Script save state local identity or fixed-array dimension is invalid");
            local.key = property->second->key;
            for (auto& value : local.values) value = TypedValue(*property->second, value);
        }
        normalized.frame->codePath = code->reflection.objectPath;
        normalized.frame->localsCodePath = localsCode->reflection.objectPath;
        return normalized;
    }
    PortableActorVmHost& Host() { return host_; }
private:
    PortableActorVmHost host_;
    std::string mapName_, gameRoot_, mapPath_;
    std::unordered_map<std::string, PortablePackageTables> tables_;
    std::unordered_map<std::string, std::string> mapClasses_;
    std::unordered_map<std::string, std::size_t> mapExports_;
    std::unordered_map<std::string, RuntimeObject*> birthClasses_;
    std::optional<std::vector<std::int32_t>> nativeActorOrder_;
    std::string nativeLevelPath_;
    static constexpr std::size_t SchemaScratchBytes = 64u * 1024u * 1024u;
    std::size_t retainedMetadata_{};
    void Retain(const std::size_t bytes) {
        constexpr std::size_t cap = 64u * 1024u * 1024u;
        if (bytes > cap || retainedMetadata_ > cap - bytes)
            throw std::runtime_error("Script save schema aggregate metadata budget exceeded");
        retainedMetadata_ += bytes;
    }
    static void SafePackage(const std::string& value) {
        if (value.empty() || value.size() > 128u || !std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        })) throw std::runtime_error("Script save package name is invalid");
    }
    const PortablePackageTables& Table(const std::string& path) {
        auto found = tables_.find(path);
        if (found == tables_.end()) {
            if (tables_.size() >= 64u || std::filesystem::file_size(path) > 512u * 1024u * 1024u)
                throw std::runtime_error("Script save schema package budget exceeded");
            auto table = LoadPortablePackageTables(path);
            Retain(table.names.size() * sizeof(table.names.front()) +
                table.imports.size() * sizeof(table.imports.front()) + table.exports.size() * sizeof(table.exports.front()));
            for (const auto& name : table.names) Retain(name.Name.ToString().size() + 1u);
            found = tables_.emplace(path, std::move(table)).first;
        }
        return found->second;
    }
    static std::string Qualified(const PortablePackageTables& table, const std::int32_t reference) {
        if (reference == 0) return {};
        const auto path = GetPortableObjectPath(table, reference);
        if (path.empty()) throw std::runtime_error("Script save reference is invalid");
        return reference > 0 ? PackageStem(table.sourcePath) + '.' + path : path;
    }
    RuntimeObject* StateDefinition(RuntimeObject* cls, const std::string& path) {
        auto* code = host_.Object(path);
        if (!code->stateDescriptor || code->reflection.metaClass != "State" ||
            LowerAscii(std::filesystem::path(code->sourcePath).extension().string()) == ".dx" ||
            !code->outer || !code->outer->classDescriptor || code->outer->reflection.metaClass != "Class" ||
            !ClassDerives(cls->reflection.objectPath, code->outer->reflection.objectPath))
            throw std::runtime_error("Script save state definition is not owned by the receiver's authored class hierarchy");
        return code;
    }
    RuntimeObject* StateNodeDefinition(RuntimeObject* cls, const std::string& path) {
        auto* node = host_.Object(path);
        if (node->stateDescriptor) return StateDefinition(cls, path);
        if (!node->classDescriptor || node->reflection.metaClass != "Class" ||
            LowerAscii(std::filesystem::path(node->sourcePath).extension().string()) == ".dx" ||
            !ClassDerives(cls->reflection.objectPath, node->reflection.objectPath))
            throw std::runtime_error("Script save StateNode is not a receiver-owned State/Class");
        return node;
    }
    QuestVr::Vm::Value TypedValue(const QuestVr::Vm::Property& property, const QuestVr::Vm::Value& saved) {
        auto* metadata = host_.Object(property.key);
        if (!metadata->property) throw std::runtime_error("Script save property metadata is unavailable");
        const auto& descriptor = *metadata->property;
        auto value = Shape(saved, property.zero);
        if (descriptor.type == "ObjectProperty" || descriptor.type == "ClassProperty") {
            const auto& table = Table(metadata->sourcePath);
            const auto expected = Qualified(table, descriptor.type == "ClassProperty" ?
                descriptor.secondaryType : descriptor.referencedType);
            ObjectValue(value, expected, descriptor.type == "ClassProperty");
        } else if (value.kind == QuestVr::Vm::Kind::Struct) {
            for (const auto& field : host_.StructPropertySchema(property.key)->fields)
                value.fields.at(LowerAscii(field.name)) =
                    TypedValue(host_.DescribeProperty(field.key), value.fields.at(LowerAscii(field.name)));
        }
        return value;
    }
    std::string ObjectClass(const std::string& path, const bool assetAllowed) {
        const auto key = LowerAscii(path);
        if (key.rfind("@classdefault:",0u)==0u) {
            const auto selected=path.substr(std::string_view("@ClassDefault:").size());
            const auto canonical=host_.ClassDefaultObject(selected);
            if (LowerAscii(canonical)!=key)
                throw std::runtime_error("Script save class-default object handle is not canonical");
            return host_.Object(canonical)->defaultOwner->reflection.objectPath;
        }
        const auto mapped = mapClasses_.find(key);
        if (mapped != mapClasses_.end()) return mapped->second;
        const auto found = persistentVmObjects.find(key);
        if (found != persistentVmObjects.end() && found->second &&
            LowerAscii(std::filesystem::path(found->second->sourcePath).extension().string()) != ".dx") {
            auto* object = found->second;
            if (object->cls) return object->cls->reflection.objectPath;
            if (object->reflection.metaClass == "Class") return "Core.Class";
            const auto& table = Table(object->sourcePath);
            // Native asset metaclasses may be imported without a serialized
            // UClass export in Engine.u. The authored export still identifies
            // its exact class; absence of a script wrapper is not Core.Class.
            return Qualified(table, table.exports.at(object->exportIndex).ObjClass);
        }
        if (!assetAllowed) throw std::runtime_error("Script save object is unavailable in its map");
        const auto dot = path.find('.');
        if (dot == std::string::npos) throw std::runtime_error("Script save asset identity is invalid");
        const auto package = path.substr(0u, dot); SafePackage(package);
        const auto source = ResolveRuntimePackagePath(gameRoot_, package);
        // A reference into any other level is not a persistent campaign object.
        if (source.empty() || LowerAscii(std::filesystem::path(source).extension().string()) == ".dx")
            throw std::runtime_error("Script save reference is unavailable or belongs to another map");
        const auto& table = Table(source);
        const auto index = FindPortableExport(table, path.substr(dot + 1u));
        const auto reference = table.exports[index].ObjClass;
        return reference == 0 ? "Core.Class" : Qualified(table, reference);
    }
    bool ClassDerives(std::string actual, const std::string& expected) {
        const auto target = LowerAscii(expected);
        std::unordered_set<std::string> visited;
        for (std::size_t depth = 0u; depth < 128u && !actual.empty(); ++depth) {
            const auto key = LowerAscii(actual);
            if (key == target) return true;
            if (!visited.emplace(key).second) throw std::runtime_error("Script save class hierarchy cycles");
            const auto found = persistentVmObjects.find(key);
            if (found != persistentVmObjects.end() && found->second->reflection.metaClass == "Class") {
                const auto* cls = found->second;
                if (cls->base) actual = cls->base->reflection.objectPath;
                else {
                    const auto& table = Table(cls->sourcePath);
                    actual = Qualified(table, table.exports.at(cls->exportIndex).ObjBase);
                }
            } else {
                // Exact pinned PackageManager native registrations. Package
                // synthesizes these missing UClasses; our disk-only reflection
                // does not. Preserve intermediate Primitive/Bitmap/State/etc.
                // so an absent wrapper neither rejects a valid native asset
                // nor authorizes an unrelated typed property reference.
                static constexpr std::pair<const char*, const char*> parents[] = {
                    {"engine.lodmesh", "Engine.Mesh"}, {"engine.mesh", "Engine.Primitive"},
                    {"engine.model", "Engine.Primitive"}, {"engine.primitive", "Core.Object"},
                    {"engine.texture", "Engine.Bitmap"}, {"engine.bitmap", "Core.Object"},
                    {"core.class", "Core.State"}, {"core.state", "Core.Struct"},
                    {"core.struct", "Core.Field"}, {"core.field", "Core.Object"}
                };
                const auto parent = std::find_if(std::begin(parents), std::end(parents),
                    [&](const auto& item) { return key == item.first; });
                if (parent == std::end(parents)) return false; // Unknown native roots fail closed.
                actual = parent->second;
            }
        }
        return false;
    }
    void ObjectValue(const QuestVr::Vm::Value& value, const std::string& expected, const bool classValue) {
        if (value.text.empty()) return;
        const auto actual = ObjectClass(value.text, true);
        if (classValue) {
            if (LowerAscii(value.text).rfind("@classdefault:",0u)==0u || LowerAscii(actual) != "core.class" ||
                (!expected.empty() && !ClassDerives(value.text, expected)))
                throw std::runtime_error("Script save Class property violates its class constraint");
        } else if (!expected.empty() && !ClassDerives(actual, expected)) {
            throw std::runtime_error("Script save Object property violates its class constraint");
        }
    }
    static QuestVr::Vm::Value Shape(const QuestVr::Vm::Value& value, const QuestVr::Vm::Value& zero) {
        if (value.kind != zero.kind) throw std::runtime_error("Script save value has the wrong concrete property kind");
        auto normalized = value;
        if (value.kind == QuestVr::Vm::Kind::Struct) {
            if (value.fields.size() != zero.fields.size()) throw std::runtime_error("Script save struct shape is invalid");
            normalized.fields.clear();
            for (const auto& [name, member] : value.fields) {
                const auto canonical = LowerAscii(name);
                const auto found = zero.fields.find(canonical);
                if (found == zero.fields.end() || !normalized.fields.emplace(canonical, Shape(member, found->second)).second)
                    throw std::runtime_error("Script save struct member is invalid");
            }
        }
        return normalized;
    }
};

void MeasureLegacySimBlendImports(const QuestVr::ScriptSavedState& saved,
    const QuestVr::ScriptStateLimits& limits, ScriptSaveSchema& schema) {
    const auto needsImport = [](const QuestVr::ScriptSavedObject& object) {
        return object.clock && std::none_of(object.properties.begin(), object.properties.end(),
            [](const auto& property) { return LowerAscii(property.name) == "simblendanim"; });
    };
    if (std::none_of(saved.objects.begin(), saved.objects.end(), needsImport)) return;

    // Deriving the legacy reflected view must obey the SAME cumulative codec
    // limits as records present on disk. Measure without copying the decoded
    // graph; each bounded four-Plane import is compared with its empty wrapper
    // so only its exact property/value/string/encoded-byte delta is charged.
    QuestVr::ScriptStateDetail::Writer original(limits, nullptr);
    original.State(saved);
    auto budget = original.measuredBudget();
    auto encodedBytes = original.size();
    for (const auto& object : saved.objects) {
        if (!needsImport(object)) continue;
        auto* cls = schema.SavedClass(object);
        const auto property = schema.Host().ClassProperty(cls, "SimBlendAnim");
        QuestVr::ScriptSavedState imported; imported.mapName = saved.mapName;
        imported.objects.push_back({object.path, object.classPath, {}, {}, {}, {}});
        QuestVr::ScriptStateDetail::Writer empty(limits, nullptr);
        empty.State(imported);
        for (std::uint32_t slot = 0u; slot < object.clock->blends.size(); ++slot) {
            QuestVr::ScriptSavedProperty value{property.key, property.name, slot,
                BlendSimulatedValue(object.clock->blends[slot].simulated)};
            value.value = schema.PropertyValue(cls, value);
            imported.objects.front().properties.push_back(std::move(value));
        }
        QuestVr::ScriptStateDetail::Writer complete(limits, nullptr);
        complete.State(imported);
        const auto& base = empty.measuredBudget();
        const auto& added = complete.measuredBudget();
        budget.Properties(added.properties - base.properties);
        for (std::size_t node = base.nodes; node < added.nodes; ++node) budget.Node(0u);
        budget.Retain(added.retained - base.retained);
        const auto bytes = complete.size() - empty.size();
        if (bytes > limits.maxBytes || encodedBytes > limits.maxBytes - bytes)
            throw std::runtime_error("Legacy SimBlendAnim import exceeds encoded-state byte budget");
        encodedBytes += bytes;
    }
}

PreparedScriptState PrepareScriptSavedState(
    const QuestVr::ScriptSavedState& saved, const bool apply, const QuestVr::ScriptStateLimits& limits) {
    if (apply && LowerAscii(saved.mapName) != LowerAscii(persistentMapPackageName))
        throw std::runtime_error("Script save can only restore into its authored map");
    ScriptSaveSchema schema(saved.mapName);
    // Register the entire symbolic graph before validating a single property:
    // valid references can point to births not present in the running session.
    schema.RegisterBirths(saved.births);
    MeasureLegacySimBlendImports(saved, limits, schema);
    // Check the complete symbolic graph before allocating staged GC objects.
    for (const auto& state : saved.aiManagers) schema.ValidateAI(state);
    schema.ValidateNativeDecals(saved);
    PreparedScriptState prepared;
    prepared.objects.reserve(saved.objects.size());
    prepared.classDefaults.reserve(saved.classDefaults.size());
    std::unordered_map<std::string, RuntimeObject*> birthTargets;
    for (const auto& birth : saved.births) {
        auto* cls = schema.DefaultClass(birth.classPath);
        decltype(RuntimeObject::frozenBirthDefaults) frozen;
        for (const auto& property : birth.frozenDefaults) {
            auto value = schema.PropertyValue(cls,property);
            if (!frozen[LowerAscii(property.name)].emplace(property.index,std::move(value)).second)
                throw std::runtime_error("Duplicate frozen birth property alias/index");
        }
        if (!apply) continue;
        pendingBirthGarbage = true;
        prepared.births.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{
            birth.path, cls->reflection.objectPath.substr(cls->reflection.objectPath.find_last_of('.') + 1u),
            {}, {}, 0x4000u, 0},nullptr));
        auto* actor = prepared.births.back().get(); actor->runtimeBirth = true;
        actor->birthVmKey = LowerAscii(birth.path); actor->sourcePath = schema.MapSource();
        actor->exportIndex = std::numeric_limits<std::size_t>::max(); actor->cls = cls;
        actor->references.push_back(cls); actor->frozenBirthDefaults.swap(frozen);
        actor->worldActorIndex = static_cast<std::int32_t>(birth.worldActorIndex);
        actor->inventoryTableMetadata = true; actor->inventoryClassReferenceValid = true;
        birthTargets.emplace(actor->birthVmKey,actor);
    }
    const auto target = [&](const std::string& path) {
        const auto born = birthTargets.find(LowerAscii(path));
        return born != birthTargets.end() ? born->second : schema.Host().Object(path);
    };
    std::unordered_set<std::string> defaultClasses;
    for (const auto& record : saved.classDefaults) {
        if (record.properties.empty() || !defaultClasses.emplace(LowerAscii(record.classPath)).second)
            throw std::runtime_error("Empty or duplicate script save class defaults");
        auto* cls = schema.DefaultClass(record.classPath);
        PreparedClassDefaults next;
        if (apply) next.target = cls;
        for (const auto& property : record.properties) {
            auto value = schema.PropertyValue(cls, property);
            if (!next.values[LowerAscii(property.name)].emplace(property.index, std::move(value)).second)
                throw std::runtime_error("Duplicate script save class-default property alias/index");
        }
        prepared.classDefaults.push_back(std::move(next));
    }
    std::unordered_set<std::string> seen;
    for (const auto& record : saved.objects) {
        if (!seen.emplace(LowerAscii(record.path)).second)
            throw std::runtime_error("Duplicate script save actor");
        auto* cls = schema.SavedClass(record);
        schema.ValidateLifecycle(record);
        PreparedScriptObject next;
        if (apply) {
            next.target = target(record.path);
            if (next.target->cls != cls) throw std::runtime_error("Script save live actor class mismatch");
        }
        for (const auto& property : record.properties) {
            auto value = schema.PropertyValue(cls, property,true);
            const auto declared=schema.Host().DescribeProperty(property.key);
            if (!next.values[schema.Host().InstanceStorageKey(cls,declared)].emplace(property.index, std::move(value)).second)
                throw std::runtime_error("Duplicate script save property alias/index");
        }
        if (record.clock) {
            const bool legacyWithoutSim = next.values.count("simblendanim") == 0u;
            for (const auto& property : ClockProperties(*record.clock)) {
                if (legacyWithoutSim && property.name == "SimBlendAnim") {
                    // Old portable clocks already retained these four floats,
                    // but did not duplicate them as reflected Plane records.
                    // Derive this missing view without changing wire codecs.
                    auto compatible = property;
                    compatible.key = schema.Host().ClassProperty(cls, compatible.name).key;
                    next.values["simblendanim"].emplace(property.index, schema.PropertyValue(cls, compatible));
                    continue;
                }
                const auto values = next.values.find(LowerAscii(property.name));
                if (values == next.values.end()) throw std::runtime_error("Script save clock property is missing");
                const auto slot = values->second.find(property.index);
                if (slot == values->second.end() || !QuestVr::Vm::Equal(slot->second, property.value))
                    throw std::runtime_error("Script save clock and reflected property disagree");
            }
        }
        next.clock = record.clock;
        next.latentTimeLeft = record.latentTimeLeft;
        if (record.state) next.state = schema.StateValue(cls, record);
        if (record.lifecycle) {
            next.lifecycle = true; next.removed = record.lifecycle->worldRemoved;
            next.touchSent = record.lifecycle->touchEventSent;
            if (apply) {
                next.children.reserve(record.lifecycle->children.size());
                next.based.reserve(record.lifecycle->basedActors.size());
                for (const auto& path : record.lifecycle->children) next.children.push_back(target(path));
                for (const auto& path : record.lifecycle->basedActors) next.based.push_back(target(path));
            }
        }
        prepared.objects.push_back(std::move(next));
    }
    for (const auto& birth : saved.births) {
        const auto encoded = std::find_if(saved.objects.begin(),saved.objects.end(),[&](const auto& object) {
            return LowerAscii(object.path) == LowerAscii(birth.path);
        });
        if (encoded == saved.objects.end()) throw std::runtime_error("Saved birth has no initialized instance record");
        for (const auto* name : {"Class","Name","ObjectFlags","Outer","XLevel","Level","Tag","bTicked",
                                 "Instigator","Brush","Location","OldLocation","Rotation","Region","Owner"}) {
            const std::string leaf=LowerAscii(name);
            const auto expected=(leaf=="class" || leaf=="name" || leaf=="objectflags" || leaf=="outer"?
                std::string("core.object."):std::string("engine.actor."))+leaf;
            if (std::none_of(encoded->properties.begin(),encoded->properties.end(),[&](const auto& property) {
                return LowerAscii(property.key)==expected && property.index == 0u;
            })) throw std::runtime_error("Saved birth is missing a native initialization field");
        }
        if (apply) {
            auto* actor = target(birth.path);
            const auto record = std::find_if(prepared.objects.begin(),prepared.objects.end(),
                [&](const auto& object) { return object.target == actor; });
            const auto& outer = record->values.at("outer").at(0u);
            actor->outer = outer.text.empty() ? nullptr : target(outer.text);
            actor->reflection.outerPath = outer.text;
            if (actor->outer) actor->references.push_back(actor->outer);
            // Preserve the constructor's actual native Level independently of
            // a later script overlay (which may legitimately contain None).
            actor->nativeXLevel = target(schema.NativeLevelPath());
        }
    }
    for (const auto& state : saved.aiManagers) {
        if (!apply) continue;
        pendingBirthGarbage = true;
        prepared.aiManagers.emplace_back(GC::Alloc<RuntimeAIManager>());
        auto* manager = prepared.aiManagers.back().get();
        manager->owner = target(state.ownerPath); manager->level = target(state.levelPath);
        manager->state = state;
        const auto retain = [&](const std::string& path) {
            if (!path.empty()) manager->actorRoots.push_back(target(path));
        };
        for (const auto& sender : state.senders) retain(sender.actor);
        for (const auto& receiver : state.receivers) {
            retain(receiver.actor); retain(receiver.previousBestActor); retain(receiver.params.bestActor);
        }
    }
    if (apply && saved.nativeDecals) {
        const auto& graph = *saved.nativeDecals;
        QuestVr::ScriptStateDetail::Writer measured(limits, nullptr); measured.State(saved);
        std::size_t rootCount{};
        for (const auto& surface : graph.state.surfaces) rootCount = QuestVr::DecalStateDetail::Add(rootCount, surface.size());
        const auto graphBytes = QuestVr::DecalStateDetail::Add(QuestVr::DecalCheckpointRetainedBytes(graph),
            QuestVr::DecalState::TransientStorageBytes(graph.surfaceCount));
        const auto rootBytes = QuestVr::DecalStateDetail::Multiply(rootCount, sizeof(RuntimeObject*));
        // The decoder, complete staged script/AI records, exact native import
        // candidate/old buffers and roots can coexist. Bound their aggregate
        // before the first graph allocation; schema metadata has its own64MiB
        // cap. This is conservative payload/capacity accounting, not RSS.
        auto staging = QuestVr::DecalStateDetail::Multiply(measured.measuredBudget().retained, 4u);
        staging = QuestVr::DecalStateDetail::Add(staging,
            QuestVr::DecalStateDetail::Add(QuestVr::DecalStateDetail::Multiply(graphBytes, 3u), rootBytes));
        if (const auto* previous = persistentRuntime->get()->nativeDecals)
            staging = QuestVr::DecalStateDetail::Add(staging, NativeDecalRetainedBytes(*previous));
        if (staging > 128u * 1024u * 1024u || graphBytes > limits.maxBytes || rootBytes > limits.maxBytes - graphBytes)
            throw std::runtime_error("Native decal restore aggregate staging/root budget exceeded");
        pendingBirthGarbage = true;
        prepared.nativeDecals.set(GC::Alloc<RuntimeMapDecals>(graph.surfaceCount, graph.nodeCount));
        auto* next = prepared.nativeDecals.get();
        next->actorRoots.reserve(rootCount);
        next->level = target(graph.levelPath); next->model = target(graph.modelPath);
        static_cast<void>(next->state.ImportSnapshot(graph.state));
        const auto retain = [&](const std::string& path) {
            auto* actor = target(path);
            if (!actor || !IsDerivedFromPath(actor->cls, "Engine.Decal") || actor->nativeXLevel != next->level)
                throw std::runtime_error("Staged native decal owner is not bound to its restored native Level");
            next->actorRoots.push_back(actor);
        };
        for (const auto& surface : graph.state.surfaces) for (const auto& record : surface) retain(record.ownerIdentity);
        if (NativeDecalRetainedBytes(*next) > limits.maxBytes)
            throw std::runtime_error("Staged native decal graph exceeds runtime retained byte budget");
    }
    if (LowerAscii(saved.mapName) == LowerAscii(persistentMapPackageName)) {
        std::unordered_map<std::string, std::size_t> replacements;
        for (const auto& object : saved.objects)
            if (object.lifecycle) replacements.emplace(LowerAscii(object.path),
                object.lifecycle->children.size() + object.lifecycle->basedActors.size());
        std::size_t links{};
        for (std::size_t index = persistentScriptExportCount; index < persistentRuntime->get()->exports.size(); ++index) {
            const auto* actor = persistentRuntime->get()->exports[index];
            if (actor->runtimeBirth) continue;
            const auto found = replacements.find(LowerAscii(actor->reflection.objectPath));
            const auto count = found == replacements.end() ? actor->initialBasedActors.size() : found->second;
            if (count > 65'536u || links > 65'536u - count)
                throw std::runtime_error("Restored total native actor link budget exceeded");
            links += count;
        }
        for (const auto& birth : saved.births) {
            const auto found = replacements.find(LowerAscii(birth.path));
            const auto count = found == replacements.end() ? 0u : found->second;
            if (count > 65'536u || links > 65'536u-count) throw std::runtime_error("Restored native birth link budget exceeded");
            links += count;
        }
    }
    if (apply) {
        StageRuntimeBirthReplacement(prepared);
    }
    return prepared;
}

}  // namespace

PortableRuntimeSummary BuildAndVerifyPortableRuntime(
    const PortablePackageTables& package) {
    PortableRuntimeSummary summary;
    const std::size_t baseline = GC::GetStats().numObjects;
    std::size_t destroyedPackage{};
    const PortableReflectionGraph graph = BuildPortableReflectionGraph(package);
    {
        GCRoot<RuntimePackage> runtime(GC::Alloc<RuntimePackage>(&destroyedPackage));
        PopulateRuntime(
            runtime.get(), package, graph, summary, &summary.destroyedObjects);
        GC::Collect();
        if (GC::GetStats().numObjects != baseline + summary.objects + 1) return summary;
    }
    GC::Collect();
    summary.passed = summary.objects == package.exports.size() &&
        summary.classes == graph.classCount &&
        summary.functions == graph.functionCount &&
        summary.states == graph.stateCount &&
        summary.properties == graph.propertyCount &&
        summary.normalizedBytecodeBytes != 0 &&
        summary.destroyedObjects == summary.objects &&
        destroyedPackage == 1 && GC::GetStats().numObjects == baseline;
    return summary;
}

PortableRuntimeSummary InitializePortableRuntime(
    const PortablePackageTables& package, const std::uint32_t initialRandomSeed) {
    ShutdownPortableRuntime();
    if (initialRandomSeed != QuestVr::ScriptRandom::InitialSeed) persistentScriptRandomSeed = initialRandomSeed;
    PortableRuntimeSummary summary;
    const std::size_t baseline = GC::GetStats().numObjects;
    const PortableReflectionGraph graph = BuildPortableReflectionGraph(package);
    persistentRuntime = std::make_unique<GCRoot<RuntimePackage>>(
        GC::Alloc<RuntimePackage>(nullptr));
    PopulateRuntime(persistentRuntime->get(), package, graph, summary, nullptr);
    persistentScriptExportCount = persistentRuntime->get()->exports.size();
    GC::Collect();
    summary.passed = summary.objects == package.exports.size() &&
        summary.classes == graph.classCount &&
        summary.functions == graph.functionCount &&
        summary.states == graph.stateCount &&
        summary.properties == graph.propertyCount &&
        summary.normalizedBytecodeBytes != 0 &&
        GC::GetStats().numObjects == baseline + summary.objects + 1;
    return summary;
}

PortableRuntimeSummary InitializePortableRuntime(
    const std::vector<PortablePackageTables>& packages, const std::uint32_t initialRandomSeed) {
    ShutdownPortableRuntime();
    // Read configuration before allocating/publishing runtime objects. Missing
    // is retained as unavailable; malformed owned input cannot leave a half
    // initialized native window domain behind.
    const auto configuredRootClass=packages.empty()?std::nullopt:QuestVr::EngineConfig::ReadRootClass(
        std::filesystem::path(packages.front().sourcePath).parent_path().parent_path());
    if (initialRandomSeed != QuestVr::ScriptRandom::InitialSeed) persistentScriptRandomSeed = initialRandomSeed;
    PortableRuntimeSummary summary;
    const std::size_t baseline = GC::GetStats().numObjects;
    persistentRuntime = std::make_unique<GCRoot<RuntimePackage>>(
        GC::Alloc<RuntimePackage>(nullptr));

    struct PackageSlice {
        const PortablePackageTables* package{};
        PortableReflectionGraph graph;
        std::string name;
        std::size_t first{};
    };
    std::vector<PackageSlice> slices;
    persistentQualifiedObjects.clear();
    persistentVmObjects.clear();
    for (const PortablePackageTables& package : packages) {
        PackageSlice slice;
        slice.package = &package;
        slice.graph = BuildPortableReflectionGraph(package);
        slice.name = PackageStem(package.sourcePath);
        slice.first = persistentRuntime->get()->exports.size();
        for (PortableReflectionObject& reflection : slice.graph.objects) {
            reflection.objectPath = slice.name + "." + reflection.objectPath;
            RuntimeObject* object = GC::Alloc<RuntimeObject>(reflection, nullptr);
            object->sourcePath = package.sourcePath;
            object->exportIndex =
                persistentRuntime->get()->exports.size() - slice.first;
            persistentRuntime->get()->exports.push_back(object);
            persistentQualifiedObjects[reflection.objectPath] = object;
            IndexRuntimeVmObject(reflection.objectPath, object);
        }
        slices.push_back(std::move(slice));
    }

    const auto resolve = [&](const PackageSlice& slice, std::int32_t reference) {
        if (reference == 0) return static_cast<RuntimeObject*>(nullptr);
        std::string path;
        if (reference > 0) {
            path = slice.name + "." +
                GetPortableObjectPath(*slice.package, reference);
        } else {
            path = GetPortableObjectPath(*slice.package, reference);
        }
        const auto found = persistentQualifiedObjects.find(path);
        if (found != persistentQualifiedObjects.end()) {
            ++summary.resolvedLinks;
            return found->second;
        }
        ++summary.unresolvedExternalLinks;
        return static_cast<RuntimeObject*>(nullptr);
    };

    for (const PackageSlice& slice : slices) {
        for (std::size_t localIndex = 0;
             localIndex < slice.package->exports.size();
             ++localIndex) {
            const std::size_t globalIndex = slice.first + localIndex;
            RuntimeObject* object = persistentRuntime->get()->exports[globalIndex];
            const ExportTableEntry& entry = slice.package->exports[localIndex];
            object->outer = resolve(slice, entry.ObjOuter);
            object->base = resolve(slice, entry.ObjBase);
            object->cls = resolve(slice, entry.ObjClass);
            if (object->outer) object->references.push_back(object->outer);
            if (object->base) object->references.push_back(object->base);
            if (object->cls) object->references.push_back(object->cls);
            if (object->reflection.metaClass == "Function") {
                object->script = std::make_unique<PortableScriptBody>(
                    LoadPortableFunctionScript(*slice.package, localIndex));
                summary.normalizedBytecodeBytes += object->script->bytecode.size();
                ++summary.functions;
            } else if (object->reflection.metaClass == "State") {
                object->stateDescriptor = std::make_unique<PortableStateDescriptor>(
                    LoadPortableStateDescriptor(*slice.package, localIndex));
                ++summary.states;
                summary.normalizedStateBytecodeBytes += object->stateDescriptor->bytecode.size();
                for (const auto reference : {object->stateDescriptor->baseField,
                        object->stateDescriptor->nextField, object->stateDescriptor->scriptText,
                        object->stateDescriptor->children}) {
                    if (auto* target = resolve(slice, reference)) object->references.push_back(target);
                }
            } else if (object->reflection.metaClass == "Class" && entry.ObjSize > 0) {
                object->classDescriptor = std::make_unique<PortableClassDescriptor>(
                    LoadPortableClassDescriptor(*slice.package, localIndex));
                ++summary.serializedClassDefaults;
                summary.classDefaultProperties += object->classDescriptor->defaults.size();
                CacheRuntimeNameProperties(object, *slice.package,
                    object->classDescriptor->defaults);
                for (const PortableTaggedProperty& property :
                     object->classDescriptor->defaults) {
                    if ((property.type != 5u && property.type != 8u) ||
                        property.value.empty()) continue;
                    const std::int32_t reference = DecodePortableObjectReference(property);
                    const std::string path = reference == 0 ? std::string() : reference > 0
                        ? slice.name + "." +
                            GetPortableObjectPath(*slice.package, reference)
                        : GetPortableObjectPath(*slice.package, reference);
                    if (property.type == 5u) QuestVr::SetActorTextureOverride(
                        object->textureOverrides, property.name.ToString(), property.arrayIndex, path);
                    object->objectPropertyPaths[property.name.ToString()] = path;
                    if (reference == 0) continue;
                    if (RuntimeObject* target = resolve(slice, reference)) {
                        object->references.push_back(target);
                    }
                }
            } else if (object->reflection.metaClass.size() >= 8 &&
                object->reflection.metaClass.compare(
                    object->reflection.metaClass.size() - 8, 8, "Property") == 0) {
                object->property = std::make_unique<PortablePropertyDescriptor>(
                    LoadPortablePropertyDescriptor(*slice.package, localIndex));
                for (const std::int32_t reference :
                     {object->property->referencedType,
                      object->property->secondaryType}) {
                    if (RuntimeObject* target = resolve(slice, reference)) {
                        object->references.push_back(target);
                    }
                }
                ++summary.properties;
            }
            CacheInventoryTableMetadata(object, *slice.package, entry);
            if (object->reflection.metaClass == "Class") ++summary.classes;
        }
    }
    // Cache actual class-property membership before the input tables leave
    // scope. Rechecking this on every field read would introduce package I/O
    // into the XR frame. Malformed chains remain explicitly invalid; no valid
    // prefix is published and an orphan export is never treated as a field.
    for (const PackageSlice& slice : slices) {
        for (std::size_t localIndex = 0u; localIndex < slice.package->exports.size(); ++localIndex) {
            auto* owner = persistentRuntime->get()->exports[slice.first+localIndex];
            if (!owner->classDescriptor) continue;
            std::vector<RuntimeObject*> fields;
            std::unordered_set<RuntimeObject*> visited;
            bool valid = true;
            try {
                auto reference = owner->classDescriptor->state.children;
                const PackageSlice* source = &slice;
                while (reference != 0) {
                    auto* field = resolve(*source,reference);
                    if (!field || field->outer != owner || visited.size() >= 8192u ||
                        !visited.insert(field).second) { valid = false; break; }
                    fields.push_back(field);
                    const auto origin = std::find_if(slices.begin(),slices.end(),
                        [&](const auto& value) { return value.package->sourcePath == field->sourcePath; });
                    if (origin == slices.end()) { valid = false; break; }
                    source = &*origin;
                    if (field->property) reference = field->property->nextField;
                    else if (field->script) reference = field->script->nextField;
                    else if (const auto* state = StateMetadata(field)) reference = state->nextField;
                    else {
                        if (!field->commonFieldLinks)
                            field->commonFieldLinks = LoadPortableFieldLinks(*source->package,field->exportIndex);
                        reference = field->commonFieldLinks->nextField;
                    }
                }
            } catch (const std::runtime_error&) { valid = false; }
            owner->classFieldLinksValid = valid;
            if (valid) for (auto* field : fields)
                if (field->property) field->linkedClassOwner = owner;
        }
    }
    for (const PackageSlice& slice : slices) {
        for (std::size_t localIndex = 0;
             localIndex < slice.package->exports.size();
             ++localIndex) {
            RuntimeObject* object =
                persistentRuntime->get()->exports[slice.first + localIndex];
            if (!IsDerivedFromPath(object->cls, "ConSys.ConObject") ||
                slice.package->exports[localIndex].ObjSize <= 0) {
                continue;
            }
            try {
                object->instanceProperties =
                    LoadPortableExportProperties(*slice.package, localIndex).properties;
                CacheRuntimeNameProperties(object, *slice.package, object->instanceProperties);
                ++summary.conversationObjects;
                summary.conversationProperties += object->instanceProperties.size();
                for (const PortableTaggedProperty& property : object->instanceProperties) {
                    if ((property.type != 5u && property.type != 8u) ||
                        property.value.empty()) continue;
                    const std::int32_t reference = DecodePortableObjectReference(property);
                    if (reference == 0) continue;
                    const std::string path = reference > 0
                        ? slice.name + "." +
                            GetPortableObjectPath(*slice.package, reference)
                        : GetPortableObjectPath(*slice.package, reference);
                    object->objectPropertyPaths[property.name.ToString()] = path;
                    if (RuntimeObject* target = resolve(slice, reference)) {
                        object->references.push_back(target);
                    }
                }
            } catch (const std::exception&) {
                ++summary.conversationLoadFailures;
            }
        }
    }
    BuildPersistentDialogueIndex();
    const auto classDefaultObjects=MaterializeClassDefaultObjects(persistentRuntime->get());
    persistentWindowDomain=std::make_unique<GCRoot<RuntimeWindowDomain>>(GC::Alloc<RuntimeWindowDomain>());
    persistentRootWindowClass=configuredRootClass;
    summary.objects = persistentRuntime->get()->exports.size();
    persistentScriptExportCount = summary.objects;
    summary.peakGcObjects = GC::GetStats().numObjects;
    GC::Collect();
    summary.passed = !packages.empty() && summary.objects != 0 &&
        summary.classes != 0 && summary.functions != 0 &&
        summary.normalizedBytecodeBytes != 0 &&
        GC::GetStats().numObjects == baseline + summary.objects + classDefaultObjects + 2;
    return summary;
}

static RuntimeMoverMotion ReadRuntimeMoverMotion(RuntimeObject* actor, const bool legacyActivated = false,
    PortableActorVmHost* sharedHost = nullptr) {
    if (!actor || !IsDerivedFromPath(actor->cls, "Engine.Mover") || actor->worldRemoved)
        throw std::runtime_error("Mover motion requires a live map Mover");
    PortableActorVmHost localHost;
    auto& host = sharedHost ? *sharedHost : localHost;
    // This small interaction layer uses immutable authored keys. Original VM
    // InterpolateTo/state scheduling remains a separate engine integration.
    const auto read = [&](const char* name, const std::uint32_t slot = 0u) {
        return host.Read(actor, host.PropertyNamed(actor, name), slot, false, true);
    };
    RuntimeMoverMotion motion;
    const auto count = QuestVr::Vm::ToInt(read("NumKeys"));
    const auto initialKey = QuestVr::Vm::ToInt(read("KeyNum"));
    if (count < 2 || count > 8 || initialKey < 0 || initialKey >= count)
        throw std::runtime_error("Unsupported authored mover key layout");
    motion.spec.keyCount = static_cast<std::uint8_t>(count);
    motion.spec.moveTime = QuestVr::Vm::ToFloat(read("MoveTime"));
    motion.spec.stayOpenTime = QuestVr::Vm::ToFloat(read("StayOpenTime"));
    const auto glide = QuestVr::Vm::ToInt(read("MoverGlideType"));
    if (glide != 0 && glide != 1) throw std::runtime_error("Unsupported authored mover glide");
    motion.spec.smooth = glide == 1;
    for (std::uint32_t key = 0u; key < motion.spec.keyCount; ++key) {
        const auto position = read("KeyPos", key), rotation = read("KeyRot", key);
        if (position.kind != QuestVr::Vm::Kind::Vector || rotation.kind != QuestVr::Vm::Kind::Rotator)
            throw std::runtime_error("Authored mover key has invalid concrete types");
        motion.spec.keys[key] = {{position.vector[0], position.vector[1], position.vector[2]},
            {rotation.rotation[0], rotation.rotation[1], rotation.rotation[2]}};
    }
    const auto position = read("Location"), rotation = read("Rotation");
    if (position.kind != QuestVr::Vm::Kind::Vector || rotation.kind != QuestVr::Vm::Kind::Rotator)
        throw std::runtime_error("Authored mover base has invalid concrete types");
    const auto& key = motion.spec.keys[static_cast<std::size_t>(initialKey)];
    motion.spec.base = {{position.vector[0] - key.position.x, position.vector[1] - key.position.y,
        position.vector[2] - key.position.z},
        {QuestVr::MoverMotionDetail::Wrap(static_cast<std::int64_t>(rotation.rotation[0]) - key.rotation.pitch),
         QuestVr::MoverMotionDetail::Wrap(static_cast<std::int64_t>(rotation.rotation[1]) - key.rotation.yaw),
         QuestVr::MoverMotionDetail::Wrap(static_cast<std::int64_t>(rotation.rotation[2]) - key.rotation.roll)}};
    const auto state = LowerAscii(ReadInheritedRuntimeName(actor, "InitialState"));
    // Supported demo-layer commands are deliberately narrower than UE1's full
    // Mover state machine (no lifts, pounding, trigger-control or encroachment).
    if (state != "triggertoggle" && state != "triggeropentimed" && state != "bumpopentimed" &&
        state != "standopentimed" && !state.empty() && state != "none")
        throw std::runtime_error("Mover InitialState is outside the supported tutorial motion subset");
    motion.timedReturn = state == "triggeropentimed" || state == "bumpopentimed" || state == "standopentimed";
    motion.state = QuestVr::MakeMoverMotionState(motion.spec,
        legacyActivated ? 1u : static_cast<std::uint8_t>(initialKey));
    return motion;
}

static bool HasRuntimeMoverKey(RuntimeObject* actor) {
    const auto wanted = LowerAscii(ReadInheritedRuntimeName(actor, "KeyIDNeeded"));
    if (wanted.empty() || wanted == "none") return false;
    for (const auto& path : persistentInventory) {
        const auto item = persistentVmObjects.find(LowerAscii(path));
        if (item != persistentVmObjects.end() && item->second &&
            IsDerivedFromPath(item->second->cls, "DeusEx.NanoKey") &&
            LowerAscii(ReadInheritedRuntimeName(item->second, "KeyID")) == wanted) return true;
    }
    return false;
}

static bool RequestRuntimeMover(RuntimeObject* actor, const bool open, const bool directUse,
    const bool undoClosingStep = false) {
    if (directUse && IsDerivedFromPath(actor->cls, "DeusEx.DeusExMover") &&
        !ReadInheritedRuntimeBool(actor, "bFrobbable")) return false;
    if (ReadInheritedRuntimeBool(actor, "bLocked") && !HasRuntimeMoverKey(actor)) return false;
    std::vector<RuntimeObject*> members{actor};
    // Pair only explicit level tags; a class's default Tag must not join every
    // otherwise unrelated door of that class into one giant moving group.
    const bool explicitTag = std::any_of(actor->instanceProperties.begin(), actor->instanceProperties.end(),
        [](const auto& property) { return property.name == "Tag"; }) || FindScriptOverlay(actor, "Tag");
    const auto tag = LowerAscii(ReadInheritedRuntimeName(actor, "Tag"));
    if (explicitTag && !tag.empty() && tag != "none" && tag != "deusexmover") {
        const auto group = persistentMapTagIndex.find(tag);
        if (group != persistentMapTagIndex.end()) for (auto* candidate : group->second)
            if (candidate != actor && candidate && candidate->active && !candidate->worldRemoved &&
                IsDerivedFromPath(candidate->cls, "Engine.Mover")) members.push_back(candidate);
    }
    std::vector<std::pair<RuntimeObject*, RuntimeMoverMotion>> staged;
    staged.reserve(members.size());
    for (auto* member : members) {
        if (ReadInheritedRuntimeBool(member, "bLocked") && !HasRuntimeMoverKey(member)) return false;
        auto motion = member->moverMotion ? *member->moverMotion : ReadRuntimeMoverMotion(member, member->activated);
        if (undoClosingStep && motion.beforeStep && motion.beforeStep->phase == QuestVr::MoverMotionPhase::Moving &&
            motion.beforeStep->targetKey == 0u) motion.state = *motion.beforeStep;
        QuestVr::RequestMoverMotionKey(motion.spec, motion.state, open ? 1u : 0u,
            open && motion.timedReturn ? 0 : -1);
        motion.beforeStep.reset();
        motion.commandPublicationPending = true;
        // The caller republishes a collision-corrected pair immediately. Keep
        // a current sample discoverable even if that correction ends at Idle;
        // never preserve the old closing sample as another blockable step.
        if (undoClosingStep) motion.beforeStep = motion.state;
        staged.emplace_back(member, std::move(motion));
    }
    for (auto& [member, motion] : staged) {
        member->moverMotion = std::move(motion);
        member->activated = open;
    }
    return true;
}

PortableConversationSummary GetPortableConversationSummary() {
    PortableConversationSummary summary;
    if (!persistentRuntime || !persistentRuntime->get()) return summary;
    for (RuntimeObject* object : persistentRuntime->get()->exports) {
        if (object == nullptr || !IsDerivedFromPath(object->cls, "ConSys.ConObject")) continue;
        ++summary.objects;
        if (IsDerivedFromPath(object->cls, "ConSys.Conversation")) ++summary.conversations;
        if (IsDerivedFromPath(object->cls, "ConSys.ConEvent")) ++summary.events;
        if (!IsDerivedFromPath(object->cls, "ConSys.ConSpeech")) continue;
        ++summary.speechObjects;
        for (const PortableTaggedProperty& property : object->instanceProperties) {
            if (property.name == "Speech" && property.type == 13u && !property.value.empty()) {
                const std::string speech = DecodePortableStringProperty(property);
                if (!speech.empty()) {
                    ++summary.speechLines;
                    if (summary.sampleSpeech.empty()) summary.sampleSpeech = speech;
                }
            }
        }
    }
    return summary;
}

PortableDialogueResult GetPortableRuntimeDialogue(
    const std::string& actorPath,
    std::size_t ordinal,
    std::int32_t missionNumber) {
    PortableDialogueResult result;
    result.actorPath = actorPath;
    const auto actorFound = persistentVmObjects.find(LowerAscii(actorPath));
    if (actorFound == persistentVmObjects.end() || actorFound->second == nullptr) {
        return result;
    }
    RuntimeObject* actor = actorFound->second;
    const auto inheritedString = [&](const char* name) {
        for (const PortableTaggedProperty& property : actor->instanceProperties) {
            if (property.name == name && property.type == 13u) {
                return DecodePortableStringProperty(property);
            }
        }
        for (RuntimeObject* cls = actor->cls; cls != nullptr; cls = cls->base) {
            if (!cls->classDescriptor) continue;
            for (const PortableTaggedProperty& property : cls->classDescriptor->defaults) {
                if (property.name == name && property.type == 13u) {
                    return DecodePortableStringProperty(property);
                }
            }
        }
        return std::string();
    };
    const std::string bindName = inheritedString("BindName");
    const std::string barkBindName = inheritedString("BarkBindName");
    result.bindName = !bindName.empty() ? bindName : barkBindName;
    if (result.bindName.empty()) return result;
    const auto appendMissions = [&](const std::string& speaker) {
        const auto found = persistentSpeakerMissions.find(LowerAscii(speaker));
        if (found == persistentSpeakerMissions.end()) return;
        for (std::int32_t mission : found->second) {
            if (!result.missionCandidates.empty()) result.missionCandidates += ",";
            result.missionCandidates += std::to_string(mission);
        }
    };
    appendMissions(bindName);
    if (!barkBindName.empty() && LowerAscii(barkBindName) != LowerAscii(bindName)) {
        appendMissions(barkBindName);
    }
    const std::vector<IndexedDialogueLine>* lines{};
    const auto findLines = [&](const std::string& speaker)
        -> const std::vector<IndexedDialogueLine>* {
        if (speaker.empty()) return static_cast<const std::vector<IndexedDialogueLine>*>(nullptr);
        if (missionNumber != std::numeric_limits<std::int32_t>::min()) {
            const auto found = persistentDialogueIndex.find(DialogueKey(missionNumber, speaker));
            return found == persistentDialogueIndex.end() ? nullptr : &found->second;
        }
        const std::string suffix = "\n" + LowerAscii(speaker);
        for (const auto& entry : persistentDialogueIndex) {
            if (entry.first.size() >= suffix.size() &&
                entry.first.compare(entry.first.size() - suffix.size(), suffix.size(), suffix) == 0) {
                return &entry.second;
            }
        }
        return static_cast<const std::vector<IndexedDialogueLine>*>(nullptr);
    };
    lines = findLines(bindName);
    if (lines == nullptr) lines = findLines(barkBindName);
    if (lines == nullptr || lines->empty()) return result;
    const bool hasFrob = std::any_of(
        lines->begin(), lines->end(), [](const IndexedDialogueLine& line) {
            return line.invokeFrob;
        });
    std::vector<const IndexedDialogueLine*> eligible;
    eligible.reserve(lines->size());
    for (const IndexedDialogueLine& line : *lines) {
        if (!hasFrob || line.invokeFrob) eligible.push_back(&line);
    }
    result.matchingLines = eligible.size();
    if (eligible.empty()) return result;
    const IndexedDialogueLine& selected = *eligible[ordinal % eligible.size()];
    result.found = true;
    result.eventPath = selected.eventPath;
    result.speech = selected.text;
    result.soundId = selected.soundId;
    result.audioPackageName = selected.audioPackageName;
    result.invokeFrob = selected.invokeFrob;
    result.effects = selected.effects;
    result.choices = selected.choices;
    for (PortableDialogueResult::Choice& choice : result.choices) {
        choice.available = true;
        if (!choice.flagName.empty()) {
            const auto flag = persistentConversationFlags.find(LowerAscii(choice.flagName));
            const bool current = flag == persistentConversationFlags.end()
                ? false
                : flag->second;
            choice.available = current == choice.requiredFlagValue;
        }
        if (!choice.skillClassPath.empty() && choice.skillLevelNeeded > 0) {
            choice.available = false;
        }
        const std::string wanted = LowerAscii(choice.label);
        for (std::size_t index = 0u; index < eligible.size(); ++index) {
            if ((!choice.targetEventPath.empty() &&
                 eligible[index]->eventPath == choice.targetEventPath) ||
                (choice.targetEventPath.empty() && !wanted.empty() &&
                 LowerAscii(eligible[index]->entryLabel) == wanted)) {
                choice.targetOrdinal = index;
                break;
            }
        }
    }
    return result;
}

PortableDialogueResult GetPortableRuntimeDataLink(const std::string& name, const std::int32_t missionNumber) {
    const auto found = persistentDataLinkIndex.find(DialogueKey(missionNumber, name));
    return found == persistentDataLinkIndex.end() ? PortableDialogueResult{} : found->second;
}

bool GetPortableRuntimeTouchDataLink(const float x, const float y, const float z,
    const float radius, const float height, PortableDialogueResult& dialogue) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !std::isfinite(radius) || !std::isfinite(height) || radius < 0.0f || height < 0.0f)
        throw std::runtime_error("DataLink contact cylinder is invalid");
    if (persistentMapPackageName.rfind("00_Training", 0u) != 0u) return false;
    for (auto* actor : persistentWorldActors) {
        if (!actor || !actor->active || actor->activated || actor->worldRemoved ||
            !IsDerivedFromPath(actor->cls, "DeusEx.DataLinkTrigger") ||
            !ReadInheritedRuntimeBool(actor, "bCollideActors")) continue;
        const auto flag = LowerAscii(ReadInheritedRuntimeName(actor, "CheckFlag"));
        if (!flag.empty() && flag != "none") continue; // Timer/flag polling not implemented here.
        const auto* location = FindInheritedRuntimeProperty(actor, "Location");
        if (!location || location->type != 10u || location->value.size() != 12u) continue;
        QuestVr::ActorVec3 center;
        std::memcpy(&center, location->value.data(), 12u);
        const auto r = ReadInheritedRuntimeFloat(actor, "CollisionRadius");
        const auto h = ReadInheritedRuntimeFloat(actor, "CollisionHeight");
        if (!QuestVr::IsFiniteActorVector(center) || !std::isfinite(r) || !std::isfinite(h) || r < 0 || h < 0) continue;
        const double dx = static_cast<double>(x) - center.x, dy = static_cast<double>(y) - center.y;
        if (dx*dx + dy*dy > static_cast<double>(radius+r)*(radius+r) ||
            std::fabs(static_cast<double>(z)-center.z) > static_cast<double>(height)+h) continue;
        auto candidate = GetPortableRuntimeDataLink(ReadInheritedRuntimeName(actor, "datalinkTag"), 0);
        if (!candidate.found) continue;
        const auto marker = "DataLink:" + candidate.eventPath;
        if (persistentAppliedDialogueEffects.count(marker)) continue;
        candidate.actorPath = actor->reflection.objectPath;
        // Allocate every returned/persisted string before marking it consumed.
        dialogue = std::move(candidate);
        persistentAppliedDialogueEffects.insert(marker);
        actor->activated = true;
        return true;
    }
    return false;
}

PortableDialogueEffectResult ApplyPortableDialogueEffects(
    const PortableDialogueResult& dialogue) {
    PortableDialogueEffectResult result;
    for (const PortableDialogueResult::Effect& effect : dialogue.effects) {
        if (!persistentAppliedDialogueEffects.insert(effect.eventPath).second) continue;
        switch (effect.type) {
        case PortableDialogueResult::Effect::Type::SetFlag:
            persistentConversationFlags[LowerAscii(effect.key)] = effect.value;
            result.status = effect.value ? "FLAG SET" : "FLAG CLEARED";
            break;
        case PortableDialogueResult::Effect::Type::AddGoal:
            persistentGoals.push_back(effect.text);
            result.status = effect.completed ? "GOAL COMPLETED" : "GOAL ADDED";
            break;
        case PortableDialogueResult::Effect::Type::AddNote:
            persistentNotes.push_back(effect.text);
            result.status = "NOTE ADDED";
            break;
        case PortableDialogueResult::Effect::Type::AddSkillPoints:
            persistentSkillPoints += effect.amount;
            result.status = "+" + std::to_string(effect.amount) + " SKILL POINTS";
            break;
        case PortableDialogueResult::Effect::Type::AddCredits:
            persistentCredits += effect.amount;
            result.status = "+" + std::to_string(effect.amount) + " CREDITS";
            break;
        case PortableDialogueResult::Effect::Type::Trigger: {
            const std::string wanted = LowerAscii(effect.key);
            const auto tagged = persistentMapTagIndex.find(wanted);
            if (tagged != persistentMapTagIndex.end()) {
                for (RuntimeObject* object : tagged->second) {
                    if (IsDerivedFromPath(object->cls, "Engine.Mover")) {
                        try { static_cast<void>(RequestRuntimeMover(object, true, false)); }
                        catch (const std::exception& error) {
                            __android_log_print(ANDROID_LOG_WARN, "quest_main", "Triggered mover rejected: %.512s", error.what());
                        }
                    } else object->activated = true;
                }
            }
            result.status = "TRIGGERED " + effect.key;
            break;
        }
        case PortableDialogueResult::Effect::Type::TransferObject: {
            const auto playerName = [](const std::string& value) {
                const std::string lowered = LowerAscii(value);
                return lowered == "jcdenton" || lowered == "jc denton" ||
                    lowered == "player" || lowered == "playername";
            };
            bool transferred{};
            if (playerName(effect.target)) {
                for (std::int32_t count = 0; count < effect.amount; ++count) {
                    persistentInventory.push_back(
                        effect.key + "@" + effect.eventPath + ":" + std::to_string(count));
                }
                transferred = true;
            } else if (playerName(effect.source)) {
                std::vector<std::size_t> matches;
                for (std::size_t index = 0; index < persistentInventory.size(); ++index) {
                    const std::string& itemPath = persistentInventory[index];
                    const auto object = persistentVmObjects.find(LowerAscii(itemPath));
                    const bool matchesClass = object != persistentVmObjects.end() &&
                        object->second != nullptr &&
                        IsDerivedFromPath(object->second->cls, effect.key);
                    if (matchesClass || itemPath.rfind(effect.key + "@", 0u) == 0u) {
                        matches.push_back(index);
                    }
                }
                if (matches.size() >= static_cast<std::size_t>(effect.amount)) {
                    for (std::int32_t count = effect.amount - 1; count >= 0; --count) {
                        persistentInventory.erase(
                            persistentInventory.begin() +
                            static_cast<std::ptrdiff_t>(matches[static_cast<std::size_t>(count)]));
                    }
                    transferred = true;
                }
            }
            if (!transferred) {
                persistentAppliedDialogueEffects.erase(effect.eventPath);
                result.status = "TRANSFER FAILED";
                continue;
            }
            result.status = playerName(effect.target) ? "ITEM RECEIVED" : "ITEM TRANSFERRED";
            break;
        }
        }
        ++result.applied;
    }
    result.credits = persistentCredits;
    result.skillPoints = persistentSkillPoints;
    result.goals = persistentGoals.size();
    result.notes = persistentNotes.size();
    result.inventoryCount = persistentInventory.size();
    return result;
}

PortableSound LoadPortableRuntimeDialogueSound(const PortableDialogueResult& dialogue) {
    if (!dialogue.found || dialogue.soundId < 0 || dialogue.audioPackageName.empty()) return {};
    const std::string packageName = "DeusExConAudio" + dialogue.audioPackageName;
    for (RuntimeObject* object : persistentRuntime->get()->exports) {
        if (object == nullptr || !IsDerivedFromPath(object->cls, "ConSys.ConAudioList") ||
            PackageStem(object->sourcePath) != packageName) {
            continue;
        }
        const PortablePackageTables package = LoadPortablePackageTables(object->sourcePath);
        const std::vector<std::int32_t> sounds =
            LoadPortableObjectReferenceArrayTail(package, object->exportIndex);
        if (static_cast<std::size_t>(dialogue.soundId) >= sounds.size()) return {};
        const std::int32_t reference = sounds[static_cast<std::size_t>(dialogue.soundId)];
        if (reference <= 0 || static_cast<std::size_t>(reference) > package.exports.size()) return {};
        return LoadPortableSound(package, static_cast<std::size_t>(reference - 1));
    }
    return {};
}

PortableSound LoadPortableRuntimeSound(const std::string& objectPath) {
    if (objectPath.empty() || !persistentRuntime || !persistentRuntime->get()) return {};
    const std::size_t separator = objectPath.find('.');
    if (separator == std::string::npos || separator + 1u >= objectPath.size()) return {};
    const std::string packageName = objectPath.substr(0u, separator);
    const std::string exportPath = objectPath.substr(separator + 1u);
    std::string gameRoot;
    for (std::size_t index = persistentScriptExportCount;
         index < persistentRuntime->get()->exports.size();
         ++index) {
        RuntimeObject* object = persistentRuntime->get()->exports[index];
        if (object == nullptr || object->sourcePath.empty()) continue;
        gameRoot = std::filesystem::path(object->sourcePath).parent_path().parent_path().string();
        break;
    }
    if (gameRoot.empty()) return {};
    std::string packagePath;
    for (const std::string& candidate : {
             gameRoot + "/Sounds/" + packageName + ".uax",
             gameRoot + "/System/" + packageName + ".u"}) {
        if (std::filesystem::is_regular_file(candidate)) {
            packagePath = candidate;
            break;
        }
    }
    if (packagePath.empty()) {
        throw std::runtime_error("Could not resolve ambient sound package " + packageName);
    }
    const PortablePackageTables package = LoadPortablePackageTables(packagePath);
    return LoadPortableSound(package, FindPortableExport(package, exportPath));
}

void ShutdownPortableRuntime() {
    ClearNativePlayerObjects();
    persistentNativeClient.Clear();
    persistentScriptRandomSeed.reset();
    persistentDispatchGraph.reset();
    persistentDispatchSummary = {};
    persistentVmObjects.clear();
    persistentWindowDomain.reset();persistentRootWindowClass.reset();
    persistentPlayerSession.reset();
    persistentRuntime.reset();
    persistentQualifiedObjects.clear();
    persistentMapTagIndex.clear();
    persistentScriptExportCount = 0;
    persistentMapPackageName.clear();
    persistentWorldActors.clear(); persistentAuthoredWorldSlots = 0u; persistentMapSourcePath.clear();
    persistentActorCollision.Clear();
    persistentInventory.clear();
    persistentCredits = 0;
    persistentSkillPoints = 0;
    persistentConversationFlags.clear();
    persistentGoals.clear();
    persistentNotes.clear();
    persistentAppliedDialogueEffects.clear();
    persistentDialogueIndex.clear();
    persistentDataLinkIndex.clear();
    persistentSpeakerMissions.clear();
    persistentPlayerHealth = 100.0f;
    GC::Collect();
}

void BindPortableRuntimeClient(std::shared_ptr<QuestVr::NativeClientLifetime> lifetime,
    const std::uint64_t clientIdentity) {
    if (!persistentRuntime || !persistentRuntime->get())
        throw std::runtime_error("Native client requires an initialized engine runtime");
    persistentNativeClient.Bind(std::move(lifetime), clientIdentity);
}
void RegisterPortableRuntimeViewport(const std::uint64_t clientIdentity,
    const QuestVr::NativeViewport& viewport) {
    if (!persistentRuntime || !persistentRuntime->get())
        throw std::runtime_error("Native viewport requires an initialized engine runtime");
    persistentNativeClient.Register(clientIdentity, viewport);
}
void ClearPortableRuntimeClient() { ClearNativePlayerObjects(); persistentNativeClient.Clear(); }
bool HasPortableRuntimeClient() { return persistentNativeClient.HasClient(); }
std::vector<QuestVr::NativeViewport> ReadPortableRuntimeClientView() {
    const auto targets = persistentNativeClient.ReadOnlyView();
    return {targets.begin(), targets.end()};
}

PortablePlayerSessionSnapshot ReadPortableRuntimePlayerSession(const std::uint64_t viewportIdentity) {
    auto* viewport=NativeSessionViewport(viewportIdentity);
    auto* session=persistentPlayerSession->get();
    PortablePlayerSessionSnapshot result;
    result.clientIdentity=persistentNativeClient.ClientIdentity();result.viewportIdentity=viewportIdentity;
    result.clientPath=session->client->reflection.objectPath;
    result.viewportPath=viewport->reflection.objectPath;result.viewportClassPath=viewport->cls->reflection.objectPath;
    PortableActorVmHost identityHost;
    const auto readIdentity=[&](RuntimeObject* object,const char* field,QuestVr::Vm::Kind kind) {
        const auto property=identityHost.DescribeProperty(field);
        if(property.zero.kind!=kind || property.arrayDimension!=1u || !identityHost.OwnsProperty(object->cls,property))
            throw std::runtime_error("Native session UObject identity field has wrong type/ownership");
        auto value=identityHost.Read(object,property,0u);
        if(value.kind!=kind) throw std::runtime_error("Native session UObject identity field has wrong value type");
        return value;
    };
    result.clientObjectFlags=readIdentity(session->client,"Core.Object.ObjectFlags",QuestVr::Vm::Kind::Int).integer;
    result.viewportObjectFlags=readIdentity(viewport,"Core.Object.ObjectFlags",QuestVr::Vm::Kind::Int).integer;
    result.viewportOuterPath=readIdentity(viewport,"Core.Object.Outer",QuestVr::Vm::Kind::Object).text;
    if(readIdentity(viewport,"Core.Object.Class",QuestVr::Vm::Kind::Object).text!=result.viewportClassPath ||
        result.viewportOuterPath!=result.clientPath ||
        result.clientObjectFlags!=static_cast<std::int32_t>(session->client->reflection.flags) ||
        result.viewportObjectFlags!=static_cast<std::int32_t>(viewport->reflection.flags))
        throw std::runtime_error("Native session UObject fields disagree with actual native identities");
    for(const auto& registered:persistentNativeClient.ReadOnlyView())
        if(registered.identity==viewportIdentity) result.viewport=registered;
    if(auto* actor=viewport->nativeViewportActor) {
        PortableActorVmHost host;
        if(host.Object(actor->reflection.objectPath)!=actor || actor->worldRemoved ||
            !IsDerivedFromPath(actor->cls,"Engine.PlayerPawn") || !actor->nativePlayerBinding || actor->nativePlayer!=viewport)
            throw std::runtime_error("Native viewport/player pairing is stale or inconsistent");
        static_cast<void>(host.RequireNativeLevelInfo(actor));
        result.playerPath=actor->reflection.objectPath;result.bound=true;
    }
    if(result.viewport.identity!=viewportIdentity || result.clientIdentity==0u ||
        NativeSessionViewport(viewportIdentity)!=viewport ||
        result.clientIdentity!=persistentNativeClient.ClientIdentity())
        throw std::runtime_error("Native client/viewport revoked during player-session snapshot");
    return result;
}

PortablePlayerSessionSnapshot PreparePortableRuntimePlayerObjects(const std::uint64_t viewportIdentity) {
    CollectBirthGarbageAtBoundary collect;
    if(!persistentRuntime || !persistentRuntime->get() || !persistentNativeClient.HasClient())
        throw std::runtime_error("Player UObject preparation requires a genuine live native client");
    const auto registered=persistentNativeClient.ReadOnlyView();
    if(std::none_of(registered.begin(),registered.end(),[&](const auto& viewport) {return viewport.identity==viewportIdentity;}))
        throw std::runtime_error("Player UObject preparation requires a registered live viewport");
    if(persistentPlayerSession)
        for(auto* viewport:persistentPlayerSession->get()->viewports)
            if(viewport->nativeViewportIdentity==viewportIdentity) return ReadPortableRuntimePlayerSession(viewportIdentity);
    PortableActorVmHost host;
    auto* playerClass=host.Object("Engine.Player");auto* objectClass=host.Object("Core.Object");
    if(!playerClass->classDescriptor || !objectClass->classDescriptor ||
        !IsDerivedFromPath(playerClass,"Core.Object"))
        throw std::runtime_error("Native viewport requires original Engine.Player/Core.Object UClass metadata");
    using Kind=QuestVr::Vm::Kind;
    for(const auto& [key,kind]:{
            std::pair{"Core.Object.Class",Kind::Object},std::pair{"Core.Object.Name",Kind::Name},
            std::pair{"Core.Object.Outer",Kind::Object},std::pair{"Core.Object.ObjectFlags",Kind::Int}}) {
        const auto field=host.DescribeProperty(key);
        if(LowerAscii(field.key)!=LowerAscii(key) || field.zero.kind!=kind || field.arrayDimension!=1u ||
            !host.OwnsProperty(objectClass,field) || !host.OwnsProperty(playerClass,field))
            throw std::runtime_error("Native player UObject identity schema is malformed: "+std::string(key));
    }
    const auto actorField=host.DescribeProperty("Engine.Player.Actor");
    if(actorField.zero.kind!=QuestVr::Vm::Kind::Object || actorField.arrayDimension!=1u ||
        !host.OwnsProperty(playerClass,actorField))
        throw std::runtime_error("Native viewport requires the actual Engine.Player.Actor declaration");
    pendingBirthGarbage=true;
    GCRoot<RuntimePlayerSession> staged(persistentPlayerSession ? persistentPlayerSession->get() : GC::Alloc<RuntimePlayerSession>());
    auto* session=staged.get();
    auto nextQualified=persistentQualifiedObjects;auto nextVm=persistentVmObjects;
    const auto index=[&](RuntimeObject* object) {
        if(!nextQualified.emplace(object->reflection.objectPath,object).second ||
            !nextVm.emplace(LowerAscii(object->reflection.objectPath),object).second)
            throw std::runtime_error("Native player/session UObject identity collision");
    };
    std::vector<GCRoot<RuntimeObject>> allocations;
    const auto allocate=[&](const std::string& path,const std::string& name,RuntimeObject* cls,RuntimeObject* outer) {
        allocations.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{
            path,cls ? cls->reflection.objectPath.substr(cls->reflection.objectPath.find_last_of('.')+1u) : name,
            outer ? outer->reflection.objectPath : std::string{}, {},0x4000u,0},nullptr));
        auto* object=allocations.back().get();object->cls=cls;object->outer=outer;
        object->sourcePath=cls ? cls->sourcePath : objectClass->sourcePath;
        object->exportIndex=std::numeric_limits<std::size_t>::max();
        if(cls) object->references.push_back(cls);
        if(outer) object->references.push_back(outer);
        return object;
    };
    auto classes=session->classes;auto viewports=session->viewports;
    const auto nativeClass=[&](const char* path,RuntimeObject* base) {
        const auto found=nextVm.find(LowerAscii(path));
        if(found!=nextVm.end()) {
            auto* cls=found->second;
            if(!cls->classDescriptor && !cls->nativeClientClass)
                throw std::runtime_error("Native player class registration collides with a non-UClass");
            if(!IsDerivedFromPath(cls,base->reflection.objectPath))
                throw std::runtime_error("Native player class has the wrong audited base");
            return cls;
        }
        auto* cls=allocate(path,std::string(path).substr(std::string(path).find('.')+1u),nullptr,nullptr);
        cls->reflection.metaClass="Class";cls->nativeClientClass=true;cls->base=base;
        cls->references.push_back(base);classes.push_back(cls);index(cls);return cls;
    };
    auto* clientClass=nativeClass("Engine.Client",objectClass);
    auto* viewportClass=nativeClass("Engine.Viewport",playerClass);
    const auto clientIdentity=persistentNativeClient.ClientIdentity();
    auto* client=session->client;
    const bool newClient=client==nullptr;
    if(!client) {
        const auto name="QuestClient"+std::to_string(clientIdentity);
        client=allocate(name,name,clientClass,nullptr);client->nativeClientIdentity=clientIdentity;index(client);
    }
    const auto name="QuestViewport"+std::to_string(viewportIdentity);
    auto* viewport=allocate(client->reflection.objectPath+'.'+name,name,viewportClass,client);
    viewport->nativeClientIdentity=clientIdentity;viewport->nativeViewportIdentity=viewportIdentity;
    // Concrete native UObject fields are independent of map Actors. Identity
    // fields use their actual loaded declarations, not package table indices.
    for(auto* object:{newClient ? client : nullptr,viewport}) {
        if(!object) continue;
        object->scriptValues["class"][0u]=QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::Object,object->cls->reflection.objectPath);
        object->scriptValues["name"][0u]=QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::Name,
            object->reflection.objectPath.substr(object->reflection.objectPath.find_last_of('.')+1u));
        object->scriptValues["outer"][0u]=QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::Object,
            object->outer ? object->outer->reflection.objectPath : std::string{});
        object->scriptValues["objectflags"][0u]=QuestVr::Vm::Value::Integer(static_cast<std::int32_t>(object->reflection.flags));
    }
    index(viewport);viewports.push_back(viewport);
    auto newOwner=persistentPlayerSession ? std::unique_ptr<GCRoot<RuntimePlayerSession>>{} :
        std::make_unique<GCRoot<RuntimePlayerSession>>(session);
    if(clientIdentity!=persistentNativeClient.ClientIdentity())
        throw std::runtime_error("Native client revoked during player UObject preparation");
    // All allocations/index copies complete before this publication interval.
    persistentQualifiedObjects.swap(nextQualified);persistentVmObjects.swap(nextVm);
    session->classes.swap(classes);session->viewports.swap(viewports);session->client=client;
    if(newOwner) persistentPlayerSession=std::move(newOwner);
    return ReadPortableRuntimePlayerSession(viewportIdentity);
}

QuestVr::Vm::Result SetPortableRuntimePlayer(const std::uint64_t viewportIdentity,
    const std::string& actorPath,const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;
    try {
        PortableActorVmHost host;
        return QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution& execution) {
            return host.SetSessionPlayer(viewportIdentity,actorPath,execution);
        },limits);
    } catch(const std::exception& error) {
        QuestVr::Vm::Result result;result.status=QuestVr::Vm::Status::Unsupported;result.error=error.what();return result;
    }
}

PortableNativeWindowSummary ReadPortableRuntimeNativeWindowSummary() {
    PortableActorVmHost host;
    PortableNativeWindowSummary result;result.configuredRootClass=persistentRootWindowClass;
    if(persistentWindowDomain && persistentWindowDomain->get()) {
        result.objects=persistentWindowDomain->get()->objects.size();
        result.roots=persistentWindowDomain->get()->roots.size();
    }
    if(result.configuredRootClass)
        result.configuredRootNameCounter=host.Object(*result.configuredRootClass)->uniqueObjectCounter;
    const auto found=persistentVmObjects.find("extension.gc");
    if(found!=persistentVmObjects.end()) result.graphicsContextNameCounter=found->second->uniqueObjectCounter;
    return result;
}

PortableRootWindowProbeResult ProbePortableRootWindowInitialization(
    const std::string& playerActorPath,const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;
    PortableRootWindowProbeResult result;
    try {
        PortableActorVmHost host;
        auto* pawn=host.Object(playerActorPath);
        if(!IsDerivedFromPath(pawn->cls,"Extension.PlayerPawnExt") || pawn->nativeRootWindow)
            throw std::runtime_error("Root initialization probe requires an actual uninitialized PlayerPawnExt");
        const auto capture=[&] {
            auto* domain=persistentWindowDomain && persistentWindowDomain->get()?persistentWindowDomain->get():nullptr;
            if(!domain) throw std::runtime_error("Native window probe has no domain");
            // A fixed bounded construction prefix, never an unbounded graph
            // walk after a failed bytecode instruction.
            if(domain->objects.size()>64u || domain->roots.size()>16u)
                throw std::runtime_error("Native window initialization probe exceeds its snapshot bound");
            QuestVr::AuthoredStructLimits bounds;bounds.retainedBytes=1u<<20u;
            QuestVr::AuthoredStructDetail::Budget retained{bounds};
            result.pawnRootPath=pawn->nativeRootWindow?pawn->nativeRootWindow->reflection.objectPath:std::string{};
            for(const auto* root:domain->roots) result.rootedWindows.push_back(root->reflection.objectPath);
            for(auto* object:domain->objects) {
                PortableNativeWindowObjectSnapshot view;
                view.path=object->reflection.objectPath;view.classPath=object->cls->reflection.objectPath;
                view.outerPath=object->outer->reflection.objectPath;
                retained.Retain(sizeof(view)+view.path.capacity()+view.classPath.capacity()+view.outerPath.capacity());
                const auto field=[&](const char* key,const std::uint32_t index=0u) {
                    const auto property=host.DescribeProperty(key);
                    if(index>=property.arrayDimension) throw std::runtime_error("Native window probe array slot exceeds its declaration");
                    auto value=host.Read(object,property,index);
                    retained.Retain(sizeof(PortableNativeWindowFieldSnapshot)+std::strlen(key));
                    QuestVr::AuthoredStructDetail::RetainedValue(value,retained);
                    PortableNativeWindowFieldSnapshot entry{key,std::move(value),{},index};
                    const auto roots=object->nativeWindowPropertyRoots.find(LowerAscii(key));
                    if(roots!=object->nativeWindowPropertyRoots.end()) {
                        const auto slot=roots->second.find(index);
                        if(slot!=roots->second.end()) for(const auto* root:slot->second) {
                            retained.Retain(sizeof(std::string)+root->reflection.objectPath.size());
                            entry.propertyRoots.push_back(root->reflection.objectPath);
                        }
                    }
                    view.fields.push_back(std::move(entry));
                };
                if(object->nativeWindowState) {
                    view.tabOrderDirty=object->nativeWindowState->dirtyTabOrder;
                    for(const auto& [source,target]:{
                        std::pair{&object->nativeWindowState->rowMajor,&view.rowMajorWindows},
                        std::pair{&object->nativeWindowState->colMajor,&view.colMajorWindows},
                        std::pair{&object->nativeWindowState->tabGroups,&view.tabGroupWindows}})
                        for(const auto* window:*source) {
                            retained.Retain(sizeof(std::string)+window->reflection.objectPath.size());
                            target->push_back(window->reflection.objectPath);
                        }
                }
                for(const auto* key:{"Core.Object.Class","Core.Object.Name","Core.Object.Outer","Core.Object.ObjectFlags"}) field(key);
                if(IsDerivedFromPath(object->cls,"Extension.Window")) {
                    for(const auto* name:{"normalFont","boldFont","wGc","gGc","winParent","parentOwner","firstChild","lastChild",
                        "prevSibling","nextSibling","bIsVisible","bIsSensitive","bIsSelectable","bBeingDestroyed","bIsInitialized",
                        "bDrawRawBackground","bStretchBackground","bSmoothBackground","bSpecialText","bTextTranslucent","bConfigured",
                        "bTickEnabled","bLastWidthSpecified","bLastHeightSpecified","bNeedsQuery","bNeedsReconfigure","lockCount",
                        "X","Y","Width","Height","hardcodedWidth","hardcodedHeight","hMargin0","hMargin1","vMargin0","vMargin1",
                        "lastSpecifiedWidth","lastSpecifiedHeight","lastQueryWidth","lastQueryHeight","textVSpacing","rowMajorIndex",
                        "colMajorIndex","att","firstTimer","windowType","winClipRect","focusSound","unfocusSound","visibleSound",
                        "invisibleSound","clientObject","Background","SoundVolume","acceleratorKey","maxClicks","freeTimer",
                        "holdX","holdY","holdWidth","holdHeight","winHAlign","winVAlign","backgroundStyle","tileColor","tilePlane",
                        "defaultCursor","defaultCursorShadow","defaultHotX","defaultHotY","defaultCursorColor"})
                        field((std::string("Extension.Window.")+name).c_str());
                    if(IsDerivedFromPath(object->cls,"Extension.TabGroupWindow"))
                        for(const auto* name:{"bSizeParentToChildren","bSizeChildrenToParent","tabGroupIndex","firstAbsX","firstAbsY"})
                            field((std::string("Extension.TabGroupWindow.")+name).c_str());
                    if(IsDerivedFromPath(object->cls,"Extension.ModalWindow")) {
                        for(const auto* name:{"focusMode","preferredFocus","bDirtyAccelerators"})
                            field((std::string("Extension.ModalWindow.")+name).c_str());
                        for(std::uint32_t slot=0u;slot<255u;++slot) field("Extension.ModalWindow.acceleratorTable",slot);
                    }
                    if(IsDerivedFromPath(object->cls,"Extension.RootWindow"))
                        for(const auto* name:{"parentPawn","nextRootWindow","defaultEditCursor","DefaultMoveCursor",
                            "defaultHorizontalMoveCursor","defaultVerticalMoveCursor","defaultTopLeftMoveCursor","defaultTopRightMoveCursor"})
                            field((std::string("Extension.RootWindow.")+name).c_str());
                    if(IsDerivedFromPath(object->cls,"Extension.RootWindow")) {
                        for(const auto* name:{"initCount","FocusWindow","lastMouseWindow","grabbedWindow","lastButtonWindow",
                            "MouseX","MouseY","prevMouseX","prevMouseY","firstButtonMouseX","firstButtonMouseY",
                            "bMouseMoved","bCursorVisible","bRender","bPositionalSound","bMouseMoveLocked","bMouseButtonLocked",
                            "bClipRender","bStretchRawBackground","bShowStats","bShowFrames","multiClickTimeout","maxMouseDist","rawColor"})
                            field((std::string("Extension.RootWindow.")+name).c_str());
                        for(std::uint32_t slot=0u;slot<255u;++slot) field("Extension.RootWindow.keyDownMap",slot);
                    }
                    if(IsDerivedFromPath(object->cls,"DeusEx.DeusExRootWindow"))
                        for(const auto* name:{"actorDisplay","hud","scopeView","winCount"})
                            field((std::string("DeusEx.DeusExRootWindow.")+name).c_str());
                    if(IsDerivedFromPath(object->cls,"DeusEx.ActorDisplayWindow")) {
                        field("DeusEx.ActorDisplayWindow.maxPoints");
                        for(std::uint32_t slot=0u;slot<16u;++slot) field("DeusEx.ActorDisplayWindow.sinTable",slot);
                    }
                    if(IsDerivedFromPath(object->cls,"DeusEx.DeusExHUD"))
                        for(const auto* name:{"Ammo","hit"}) field((std::string("DeusEx.DeusExHUD.")+name).c_str());
                    if(IsDerivedFromPath(object->cls,"DeusEx.HUDAmmoDisplay"))
                        field("DeusEx.HUDAmmoDisplay.Player");
                    if(IsDerivedFromPath(object->cls,"DeusEx.HUDHitDisplay"))
                        for(const auto* name:{"Player","Head"}) field((std::string("DeusEx.HUDHitDisplay.")+name).c_str());
                    if(IsDerivedFromPath(object->cls,"DeusEx.HUDBaseWindow"))
                        for(const auto* name:{"Player","colBackground","colBorder","colText","colHeaderText"})
                            field((std::string("DeusEx.HUDBaseWindow.")+name).c_str());
                } else if(IsDerivedFromPath(object->cls,"Extension.GC")) {
                    for(const auto* name:{"Canvas","normalFont","boldFont","gcOwner","Style","PolyFlags","textPolyFlags",
                        "bDrawEnabled","bSmoothed","bMasked","bTranslucent","bModulated"})
                        field((std::string("Extension.GC.")+name).c_str());
                }
                result.objects.push_back(std::move(view));
            }
        };
        result.execution=QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution& execution)->QuestVr::Vm::Value {
            const auto init=host.Member(pawn,"Extension.PlayerPawnExt.InitRootWindow");
            try { static_cast<void>(execution.CallFunction(*init,pawn->reflection.objectPath,{})); }
            catch(...) {
                try {capture();} catch(const std::exception& error) {result.inspectionError=error.what();}
                throw; // Keep the original first poisoned callback failure.
            }
            capture();
            throw std::runtime_error("Non-publishing native root initialization probe completed; rollback required");
        },limits);
    } catch(const std::exception& error) {
        result.execution.status=QuestVr::Vm::Status::Unsupported;result.execution.error=error.what();
    }
    return result;
}

PortablePlayerHudSnapshot ReadPortablePlayerHudSnapshot(const std::string& actorPath) {
    PortableActorVmHost host;
    auto* player=host.Object(actorPath);
    if(!player->active || player->worldRemoved || !IsDerivedFromPath(player->cls,"DeusEx.DeusExPlayer"))
        throw std::runtime_error("Player HUD source is not an actual live DeusExPlayer");
    auto* level=host.RequireNativeLevelInfo(player);
    const auto read=[&](RuntimeObject* object,const std::string& field,QuestVr::Vm::Kind kind) {
        const auto property=host.DescribeProperty(field);
        if(property.zero.kind!=kind || property.arrayDimension!=1u || !host.OwnsProperty(object->cls,property))
            throw std::runtime_error("Player HUD declared field has wrong type/ownership: "+field);
        auto value=host.Read(object,property,0u);
        if(value.kind!=kind || (kind==QuestVr::Vm::Kind::Float && !std::isfinite(value.floating)) ||
            value.text.size()>4096u || value.text.find('\0')!=std::string::npos)
            throw std::runtime_error("Player HUD declared field is malformed: "+field);
        return value;
    };
    using Kind=QuestVr::Vm::Kind;
    PortablePlayerHudSnapshot result;result.playerPath=player->reflection.objectPath;
    static constexpr const char* healthFields[]={"HealthHead","HealthTorso","HealthArmLeft","HealthArmRight","HealthLegLeft","HealthLegRight"};
    for(std::size_t index=0u;index<result.limbHealth.size();++index)
        result.limbHealth[index]=read(player,std::string("Engine.Pawn.")+healthFields[index],Kind::Int).integer;
    result.energy=read(player,"DeusEx.DeusExPlayer.Energy",Kind::Float).floating;
    result.energyMax=read(player,"DeusEx.DeusExPlayer.EnergyMax",Kind::Float).floating;
    result.keyRingPath=read(player,"DeusEx.DeusExPlayer.KeyRing",Kind::Object).text;
    if(!result.keyRingPath.empty()) {
        auto* ring=host.Object(result.keyRingPath);
        if(ring->worldRemoved || !IsDerivedFromPath(ring->cls,"DeusEx.NanoKeyRing") ||
            host.RequireNativeLevelInfo(ring)!=level ||
            read(ring,"Engine.Actor.Owner",Kind::Object).text!=result.playerPath)
            throw std::runtime_error("Player HUD KeyRing is not its actual owned same-world ring");
    }
    auto current=read(player,"Engine.Actor.Inventory",Kind::Object).text;
    std::array<RuntimeObject*,1024u> visited{};std::size_t count{};
    while(!current.empty()) {
        auto* item=host.Object(current);
        if(count==visited.size() || std::find(visited.begin(),visited.begin()+count,item)!=visited.begin()+count)
            throw std::runtime_error("Player HUD Inventory chain cycles or exceeds 1024 objects");
        visited[count++]=item;
        if(item->worldRemoved || !IsDerivedFromPath(item->cls,"Engine.Inventory") ||
            host.RequireNativeLevelInfo(item)!=level ||
            read(item,"Engine.Actor.Owner",Kind::Object).text!=result.playerPath)
            throw std::runtime_error("Player HUD Inventory member is not its actual owned same-world item");
        PortablePlayerHudItem row;
        row.actorPath=item->reflection.objectPath;row.classPath=item->cls->reflection.objectPath;
        row.inObjectBelt=read(item,"Engine.Inventory.bInObjectBelt",Kind::Bool).boolean;
        row.beltPosition=read(item,"Engine.Inventory.beltPos",Kind::Int).integer;
        row.iconPath=read(item,"Engine.Inventory.Icon",Kind::Object).text;
        row.beltDescription=read(item,"Engine.Inventory.beltDescription",Kind::String).text;
        if(row.inObjectBelt && (row.beltPosition<0 || row.beltPosition>9))
            throw std::runtime_error("Player HUD active belt position is outside original ten slots");
        result.inventory.push_back(std::move(row));
        current=read(item,"Engine.Actor.Inventory",Kind::Object).text;
    }
    if(!result.keyRingPath.empty() && std::none_of(result.inventory.begin(),result.inventory.end(),[&](const auto& item) {
            return item.actorPath==result.keyRingPath;
        })) throw std::runtime_error("Player HUD KeyRing is absent from its actual Inventory chain");
    result.available=true;return result;
}

PortablePlayerHudSnapshot ReadPortableRuntimePlayerHud(const std::uint64_t viewportIdentity) {
    const auto session=ReadPortableRuntimePlayerSession(viewportIdentity);
    if(!session.bound) return {};
    auto result=ReadPortablePlayerHudSnapshot(session.playerPath);result.bound=true;return result;
}

PortableVmValue ExecutePortableFunction(const std::string& objectPath) {
    if (!persistentRuntime || !persistentRuntime->get()) {
        throw std::runtime_error("Portable VM has no initialized runtime");
    }
    RuntimeObject* function = nullptr;
    for (RuntimeObject* object : persistentRuntime->get()->exports) {
        if ((object->reflection.objectPath == objectPath ||
             (object->reflection.objectPath.size() > objectPath.size() &&
              object->reflection.objectPath.compare(
                  object->reflection.objectPath.size() - objectPath.size(),
                  objectPath.size(), objectPath) == 0 &&
              object->reflection.objectPath[
                  object->reflection.objectPath.size() - objectPath.size() - 1] == '.')) &&
            object->script) {
            function = object;
            break;
        }
    }
    if (function == nullptr) {
        throw std::runtime_error("Portable VM function was not found: " + objectPath);
    }
    RuntimeBytecodeReader reader(function->script->bytecode);
    if (reader.Byte() != 0x04u) {
        throw std::runtime_error("Portable VM function does not begin with Return");
    }
    return EvaluateConstant(reader);
}

QuestVr::Vm::Result ExecutePortableActorFunction(const std::string& actorPath,
    const std::string& functionName, const std::vector<QuestVr::Vm::Evaluation>& arguments,
    const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;
    try {
        PortableActorVmHost host;
        RuntimeObject* actor = host.Object(actorPath);
        if ((!actor->active && !actor->worldRemoved) || !IsDerivedFromPath(actor->cls, "Engine.Actor"))
            throw std::runtime_error("Explicit VM receiver is not a live Engine.Actor");
        const auto function = host.Member(actor, functionName);
        return QuestVr::Vm::Execute(host, *function, actor->reflection.objectPath, arguments, limits);
    } catch (const std::exception& error) {
        QuestVr::Vm::Result result;
        result.status = QuestVr::Vm::Status::Unsupported;
        result.function = functionName; result.error = error.what();
        return result;
    }
}

const char* PortableWorldStartupPhaseName(const PortableWorldStartupPhase phase) {
    switch (phase) {
    case PortableWorldStartupPhase::ValidateWorld: return "ValidateWorld";
    case PortableWorldStartupPhase::SelectGameInfo: return "SelectGameInfo";
    case PortableWorldStartupPhase::ResetTime: return "ResetTime";
    case PortableWorldStartupPhase::RestoreTime: return "RestoreTime";
    case PortableWorldStartupPhase::ClearTouching: return "ClearTouching";
    case PortableWorldStartupPhase::InitializeExecution: return "InitializeExecution";
    case PortableWorldStartupPhase::PublishStartup: return "PublishStartup";
    case PortableWorldStartupPhase::InitGame: return "InitGame";
    case PortableWorldStartupPhase::PreBeginPlay: return "PreBeginPlay";
    case PortableWorldStartupPhase::BeginPlay: return "BeginPlay";
    case PortableWorldStartupPhase::SetActorZone: return "SetActorZone";
    case PortableWorldStartupPhase::LastRenderTime: return "LastRenderTime";
    case PortableWorldStartupPhase::PostBeginPlay: return "PostBeginPlay";
    case PortableWorldStartupPhase::SetInitialState: return "SetInitialState";
    case PortableWorldStartupPhase::WorldBases: return "WorldBases";
    case PortableWorldStartupPhase::ClearStartup: return "ClearStartup";
    case PortableWorldStartupPhase::PostPostBeginPlay: return "PostPostBeginPlay";
    case PortableWorldStartupPhase::CompactActorSlots: return "CompactActorSlots";
    default: return "InvalidPhase";
    }
}

PortableWorldStartupProbeResult ProbePortableWorldStartup(
    const PortableWorldStartupProbeOptions& options, const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;
    PortableWorldStartupProbeResult result;
    try {
        PortableActorVmHost host;
        result.execution = QuestVr::Vm::ExecuteTransaction(host, [&](QuestVr::Vm::Execution& execution) {
            return host.ProbeWorldStartup(options, result, execution);
        }, limits);
    } catch (const std::exception& error) {
        result.execution.status = QuestVr::Vm::Status::Unsupported;
        result.execution.error = error.what();
    }
    return result;
}

QuestVr::Vm::Value ReadPortableActorScriptProperty(const std::string& actorPath,
    const std::string& propertyName, const std::uint32_t arrayIndex) {
    PortableActorVmHost host;
    RuntimeObject* actor = host.Object(actorPath);
    if ((!actor->active && !actor->worldRemoved) || !IsDerivedFromPath(actor->cls, "Engine.Actor"))
        throw std::runtime_error("Explicit VM receiver is not a live Engine.Actor");
    const auto property = host.PropertyNamed(actor, propertyName);
    if (arrayIndex >= property.arrayDimension)
        throw std::runtime_error("VM property fixed-array index is out of range");
    return host.Read(actor, property, arrayIndex);
}

PortableMapNorthSnapshot ReadPortableRuntimeMapNorth() {
    if (persistentMapSourcePath.empty() || persistentWorldActors.empty() ||
        !persistentWorldActors.front() || persistentWorldActors.front()->worldRemoved ||
        !IsDerivedFromPath(persistentWorldActors.front()->cls, "Engine.LevelInfo"))
        throw std::runtime_error("Map north requires an actual loaded Level");
    auto* level = persistentWorldActors.front()->nativeXLevel;
    PortableActorVmHost host;
    if (!level || level->runtimeBirth || level->sourcePath != persistentMapSourcePath ||
        host.RequireNativeLevelInfo(persistentWorldActors.front())!=persistentWorldActors.front())
        throw std::runtime_error("Map north requires the current serialized native Level");
    for (auto* actor : persistentWorldActors) {
        if (!actor || actor->worldRemoved ||
            !IsDerivedFromPath(actor->cls, "DeusEx.DeusExLevelInfo")) continue;
        if (actor->nativeXLevel != level)
            throw std::runtime_error("Map north candidate belongs to a foreign native Level");
        const auto value = ReadPortableActorDeclaredScriptProperty(actor->reflection.objectPath,
            "DeusEx.DeusExLevelInfo.TrueNorth");
        if (value.kind != QuestVr::Vm::Kind::Int)
            throw std::runtime_error("DeusExLevelInfo.TrueNorth is not the declared Int value");
        return {true, actor->reflection.objectPath, value.integer};
    }
    return {};
}

QuestVr::Vm::Value ReadPortableActorDeclaredScriptProperty(const std::string& actorPath,
    const std::string& propertyPath, const std::uint32_t arrayIndex) {
    const auto valid = [](const std::string& identity) {
        return !identity.empty() && identity.size() <= 4096u && identity.find('\0') == std::string::npos;
    };
    if (!valid(actorPath) || !valid(propertyPath))
        throw std::runtime_error("Declared actor property identity is invalid");
    PortableActorVmHost host;
    auto* actor = host.Object(actorPath);
    if (!actor->active || actor->worldRemoved || !IsDerivedFromPath(actor->cls,"Engine.Actor"))
        throw std::runtime_error("Declared property receiver is not an active nonretired Engine.Actor");
    const auto property = host.DescribeProperty(propertyPath);
    if (!host.OwnsProperty(actor->cls,property))
        throw std::runtime_error("Declared property is not owned by its concrete receiver class");
    if (arrayIndex >= property.arrayDimension)
        throw std::runtime_error("Declared property fixed-array index is out of range");
    return host.Read(actor,property,arrayIndex);
}

QuestVr::Vm::Value ReadPortableClassDefault(const std::string& classPath,
    const std::string& propertyName, const std::uint32_t arrayIndex) {
    PortableActorVmHost host;
    auto* object = host.Object(classPath);
    if (host.DefaultClass(object) != object)
        throw std::runtime_error("Explicit default receiver is not a loaded script Actor UClass");
    const auto property = host.ClassProperty(object, propertyName);
    if (arrayIndex >= property.arrayDimension)
        throw std::runtime_error("Class-default fixed-array index is out of range");
    return host.Read(object, property, arrayIndex, true);
}

std::vector<QuestVr::Vm::Value> ReadPortableActorScriptPropertySlots(const std::string& actorPath,
    const std::string& propertyName, const std::uint32_t firstIndex, const std::uint32_t count) {
    if (count == 0u || count > 1024u)
        throw std::runtime_error("VM property fixed-array read count is outside 1..1024");
    PortableActorVmHost host;
    RuntimeObject* actor = host.Object(actorPath);
    if ((!actor->active && !actor->worldRemoved) || !IsDerivedFromPath(actor->cls, "Engine.Actor"))
        throw std::runtime_error("Explicit VM receiver is not a live Engine.Actor");
    const auto property = host.PropertyNamed(actor, propertyName);
    if (firstIndex >= property.arrayDimension || count > property.arrayDimension - firstIndex)
        throw std::runtime_error("VM property fixed-array read range is out of range");
    std::vector<QuestVr::Vm::Value> values;
    values.reserve(count);
    QuestVr::AuthoredStructLimits limits;
    QuestVr::AuthoredStructDetail::Budget retained{limits};
    retained.Retain(values.capacity() * sizeof(QuestVr::Vm::Value));
    for (std::uint32_t offset = 0u; offset < count; ++offset) {
        auto value = host.Read(actor, property, firstIndex + offset);
        QuestVr::AuthoredStructDetail::RetainedValue(value, retained);
        values.push_back(std::move(value));
    }
    return values;
}

QuestVr::Vm::Value CastPortableRuntimeObject(const std::string& declaringObjectPath,
    const std::int32_t targetReference, const QuestVr::Vm::Value& value, const bool meta) {
    PortableActorVmHost host;
    const auto* declaring = host.Object(declaringObjectPath);
    QuestVr::Vm::Function source;
    source.path = declaring->reflection.objectPath;
    source.source = declaring->sourcePath;
    const auto target = host.ResolveCastClass(source, targetReference);
    return host.CastObject(target, value, meta);
}

std::optional<PortableObjectStack> ReadPortableActorSerializedStack(const std::string& actorPath) {
    PortableActorVmHost host;
    const auto* actor = host.Object(actorPath);
    if (!IsDerivedFromPath(actor->cls, "Engine.Actor"))
        throw std::runtime_error("Authored stack receiver is not Engine.Actor");
    return actor->serializedStack;
}

PortableStateDescriptor ReadPortableRuntimeAuthoredStateDescriptor(const std::string& objectPath) {
    PortableActorVmHost host;
    const auto* object = host.Object(objectPath);
    if (object->stateDescriptor) return *object->stateDescriptor;
    if (object->classDescriptor) return object->classDescriptor->state;
    throw std::runtime_error("Runtime object has no serialized State/Class metadata: " + objectPath);
}

PortableScriptDispatchSummary ReadPortableRuntimeDispatchSummary() {
    DispatchGraph(); return persistentDispatchSummary;
}
QuestVr::ScriptDispatch::Class ReadPortableRuntimeDispatchClass(const std::string& classPath) {
    const auto* cls = DispatchGraph().FindClass(classPath);
    if (!cls) throw std::runtime_error("Dispatch metadata class is unavailable: " + classPath);
    return *cls;
}

PortableActorDispatchContext ReadPortableActorDispatchContext(const std::string& actorPath) {
    PortableActorVmHost host;
    auto* actor = host.Object(actorPath);
    if (!IsDerivedFromPath(actor->cls, "Engine.Actor"))
        throw std::runtime_error("Dispatch receiver is not Engine.Actor");
    return CurrentDispatchContext(actor, true); // Detached raw identity inspection, not execution.
}

std::optional<QuestVr::StateObject> ReadPortableActorStateObject(const std::string& actorPath) {
    PortableActorVmHost host;
    auto* actor = host.Object(actorPath);
    if (!IsDerivedFromPath(actor->cls, "Engine.Actor")) throw std::runtime_error("State receiver is not Engine.Actor");
    return actor->stateObject;
}

QuestVr::Vm::Result ResumePortableActorState(const std::string& actorPath, const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;
    try {
        PortableActorVmHost host;
        return QuestVr::Vm::ResumeState(host, actorPath, limits);
    } catch (const std::exception& error) {
        QuestVr::Vm::Result result;
        result.status = QuestVr::Vm::Status::Unsupported; result.function = actorPath; result.error = error.what(); return result;
    }
}

QuestVr::Vm::Result AdvancePortableActorState(const std::string& actorPath, const float elapsed,
    const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;
    try {
        PortableActorVmHost host;
        return QuestVr::Vm::AdvanceState(host, actorPath, elapsed, limits);
    } catch (const std::exception& error) {
        QuestVr::Vm::Result result;
        result.status = QuestVr::Vm::Status::Unsupported; result.function = actorPath; result.error = error.what(); return result;
    }
}

QuestVr::Vm::Result AdvancePortableActorTimer(const std::string& actorPath,const float elapsed,
    const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;
    if (!std::isfinite(elapsed) || elapsed<0.0f) {
        QuestVr::Vm::Result result;
        result.status=QuestVr::Vm::Status::Invalid; result.function=actorPath;
        result.error="Actor timer elapsed time must be finite and nonnegative";
        return result;
    }
    try {
        PortableActorVmHost host;
        auto* actor=host.Object(actorPath);
        return QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution& execution) {
            return QuestVr::Vm::Value::Bool(host.AdvanceTimer(actor,elapsed,execution));
        },limits);
    } catch (const std::exception& error) {
        QuestVr::Vm::Result result;
        result.status=QuestVr::Vm::Status::Unsupported; result.function=actorPath;
        result.error=error.what(); return result;
    }
}

std::optional<float> ReadPortableActorLatentTimeLeft(const std::string& actorPath) {
    PortableActorVmHost host;
    auto* actor=host.Object(actorPath);
    if (!IsDerivedFromPath(actor->cls,"Engine.Actor")) throw std::runtime_error("Latent timer receiver is not Engine.Actor");
    return actor->latentTimeLeft;
}

std::optional<std::string> ResolvePortableActorState(const std::string& actorPath, const std::string& stateName) {
    PortableActorVmHost host;
    auto* actor = host.Object(actorPath);
    if (!IsDerivedFromPath(actor->cls, "Engine.Actor")) throw std::runtime_error("State receiver is not Engine.Actor");
    return QuestVr::ScriptDispatch::ResolveState(DispatchGraph(), actor->cls->reflection.objectPath, stateName);
}

std::optional<std::string> ResolvePortableActorFunction(const std::string& actorPath, const std::string& stateName,
    const std::string& functionName, const QuestVr::ScriptDispatch::LookupKind kind) {
    PortableActorVmHost host;
    auto* actor = host.Object(actorPath);
    if (!IsDerivedFromPath(actor->cls, "Engine.Actor")) throw std::runtime_error("Callback receiver is not Engine.Actor");
    return QuestVr::ScriptDispatch::ResolveFunction(DispatchGraph(), actor->cls->reflection.objectPath, stateName, functionName, kind);
}

QuestVr::Vm::ProgramLayout ReadPortableRuntimeStateProgram(const std::string& objectPath, const QuestVr::Vm::Limits& limits) {
    PortableActorVmHost host;
    auto* object = host.Object(objectPath);
    const auto* metadata = StateMetadata(object);
    if (!metadata) throw std::runtime_error("Program receiver is not a serialized State/Class");
    QuestVr::Vm::Function program;
    program.path = object->reflection.objectPath; program.source = object->sourcePath; program.bytecode = metadata->bytecode;
    return QuestVr::Vm::AnalyzeProgram(host, program, limits);
}

QuestVr::Vm::Result ExecutePortableActorEvent(const std::string& actorPath, const std::string& eventName,
    const bool enumDispatch, const std::vector<QuestVr::Vm::Evaluation>& arguments, const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;
    try {
        PortableActorVmHost host;
        QuestVr::Vm::Result none;
        none.status = QuestVr::Vm::Status::Returned;
        none.function = eventName;
        const auto selected = host.ResolveEvent(actorPath, eventName, enumDispatch);
        if (!selected) return none;
        return QuestVr::Vm::Execute(host, *selected, actorPath, arguments, limits);
    } catch (const std::exception& error) {
        QuestVr::Vm::Result result;
        result.status = QuestVr::Vm::Status::Unsupported;
        result.function = eventName; result.error = error.what(); return result;
    }
}

bool GetPortableRuntimeScriptStatePresent() {
    if (!persistentRuntime || !persistentRuntime->get()) return false;
    if (persistentScriptRandomSeed) return true;
    return GetPortableRuntimeUnsavedScriptState();
}

bool GetPortableRuntimeUnsavedScriptState() {
    if (!persistentRuntime || !persistentRuntime->get()) return false;
    if (persistentRuntime->get()->nativeDecals) return true;
    for (const auto* object : persistentRuntime->get()->exports)
        if (object->committedScriptState || !object->classDefaultValues.empty() || object->nativeAiManager) return true;
    return false;
}

std::uint64_t GetPortableRuntimeWorldRevision() { return persistentWorldRevision; }

static void RelinkAuthoredActorBases(const PortablePackageTables& package, const std::size_t first) {
    PortableActorVmHost host;
    const auto property = [](RuntimeObject* actor, const char* name) -> RuntimeObject* {
        for (auto* cls = actor->cls; cls; cls = cls->base) {
            const auto found = persistentVmObjects.find(LowerAscii(cls->reflection.objectPath + '.' + name));
            if (found != persistentVmObjects.end() && found->second->property) return found->second;
        }
        return nullptr;
    };
    std::vector<RuntimeObject*> actorOrder;
    const bool hasLevel = std::any_of(package.exports.begin(), package.exports.end(), [&](const auto& entry) {
        const auto path = LowerAscii(GetPortableObjectPath(package, entry.ObjClass));
        const auto dot = path.find_last_of('.');
        return path.substr(dot == std::string::npos ? 0u : dot + 1u) == "level";
    });
    if (hasLevel) {
        RuntimeObject* xlevel{};
        for (std::size_t i = 0u; i < package.exports.size(); ++i)
            if (LowerAscii(GetPortableObjectPath(package,package.exports[i].ObjClass)) == "engine.level") {
                if (xlevel) throw std::runtime_error("Map has more than one serialized Level");
                xlevel = persistentRuntime->get()->exports.at(first+i);
            }
        if (!xlevel) throw std::runtime_error("Map has no linked serialized Level");
        const auto order = ReadPortableLevel68ActorOrder(package);
        persistentWorldActors.assign(order.size(), nullptr);
        actorOrder.reserve(order.size());
        for (std::size_t slot = 0u; slot < order.size(); ++slot) {
            const auto reference = order[slot];
            if (reference == 0) continue;
            if (reference < 0 || static_cast<std::size_t>(reference) > package.exports.size())
                throw std::runtime_error("Level actor slot is not a local map export");
            auto* actor = persistentRuntime->get()->exports.at(first + static_cast<std::size_t>(reference - 1));
            if (!IsDerivedFromPath(actor->cls, "Engine.Actor")) {
                // Existing reflection retains unresolved external types. A
                // missing wrapper must not fabricate actor native behavior.
                if (!actor->cls) continue;
                throw std::runtime_error("Level actor slot is not an Actor");
            }
            if (actor->worldActorIndex >= 0) throw std::runtime_error("Level contains a duplicate actor slot");
            actor->worldActorIndex = static_cast<std::int32_t>(slot);
            actor->nativeXLevel = xlevel;
            persistentWorldActors[slot] = actor;
            actorOrder.push_back(actor);
        }
    } else {
        persistentWorldActors.clear();
        // Generated metadata-only maps deliberately omit serialized ULevel.
        // Only that explicit fixture shape uses export-order membership.
        for (std::size_t index = first; index < persistentRuntime->get()->exports.size(); ++index) {
            auto* actor = persistentRuntime->get()->exports[index];
            if (!IsDerivedFromPath(actor->cls, "Engine.Actor")) continue;
            actor->worldActorIndex = static_cast<std::int32_t>(actorOrder.size());
            actorOrder.push_back(actor);
            persistentWorldActors.push_back(actor);
        }
    }
    persistentAuthoredWorldSlots = persistentWorldActors.size();
    QuestVr::ActorCollisionRegistry collision;
    for (auto* actor : actorOrder) host.RegisterCollisionActor(actor,collision);
    persistentActorCollision.Swap(collision);
    std::size_t links{};
    for (auto* actor : actorOrder) {
        if (!IsDerivedFromPath(actor->cls, "Engine.Actor")) continue;
        // Minimal read-only fixtures need not contain the entire Actor schema.
        // Real authored Base bindings are relinked in serialized Level order,
        // as Engine::LinkActorsToLevel does, without replaying Attach callbacks.
        auto* metadata = property(actor, "Base");
        if (!metadata) continue;
        const auto baseValue = host.Read(actor, host.DescribeProperty(metadata->reflection.objectPath), 0u);
        if (baseValue.kind != QuestVr::Vm::Kind::Object) throw std::runtime_error("Authored Base is not an Object");
        if (baseValue.text.empty()) continue;
        auto* base = host.Object(baseValue.text);
        if (!IsDerivedFromPath(base->cls, "Engine.Actor") || base->sourcePath != actor->sourcePath)
            throw std::runtime_error("Authored Base is not an actor in this map");
        auto* levelProperty = property(actor, "Level");
        if (!levelProperty) throw std::runtime_error("Authored based actor has no Level property");
        const auto level = host.Read(actor, host.DescribeProperty(levelProperty->reflection.objectPath), 0u);
        if (LowerAscii(baseValue.text) == LowerAscii(level.text)) continue;
        if (++links > 65'536u) throw std::runtime_error("Authored based actor link budget exceeded");
        base->basedActors.push_back(actor); base->initialBasedActors.push_back(actor);
        auto* count = property(base, "StandingCount");
        if (!count || host.DescribeProperty(count->reflection.objectPath).zero.kind != QuestVr::Vm::Kind::Byte)
            throw std::runtime_error("Authored base has no byte StandingCount");
        base->initialStandingCount = static_cast<std::uint8_t>(std::min<std::size_t>(base->basedActors.size(), 255u));
    }
}

PortableMapRuntimeSummary LoadPortableRuntimeMap(
    const PortablePackageTables& package) {
    if (!persistentRuntime || !persistentRuntime->get()) {
        throw std::runtime_error("Cannot load a map without an initialized runtime");
    }
    PortableMapRuntimeSummary summary;
    if (GetPortableRuntimeUnsavedScriptState()) {
        __android_log_print(ANDROID_LOG_WARN, "quest_main",
            "DeusExQuest: map replacement refused: script state requires a per-map archive");
        return summary;
    }
    summary.replacedExports = UnloadPortableRuntimeMap();
    persistentMapTagIndex.clear();
    const std::string packageName = PackageStem(package.sourcePath);
    const PortableReflectionGraph graph = BuildPortableReflectionGraph(package);
    const std::size_t first = persistentRuntime->get()->exports.size();
    persistentRuntime->get()->exports.reserve(first + graph.objects.size());
    for (PortableReflectionObject reflection : graph.objects) {
        reflection.objectPath = packageName + "." + reflection.objectPath;
        RuntimeObject* object = GC::Alloc<RuntimeObject>(reflection, nullptr);
        object->sourcePath = package.sourcePath;
        object->exportIndex = persistentRuntime->get()->exports.size() - first;
        persistentRuntime->get()->exports.push_back(object);
        persistentQualifiedObjects[reflection.objectPath] = object;
        IndexRuntimeVmObject(reflection.objectPath, object);
    }

    const auto resolve = [&](std::int32_t reference) {
        if (reference == 0) return static_cast<RuntimeObject*>(nullptr);
        std::string path;
        if (reference > 0) {
            path = packageName + "." + GetPortableObjectPath(package, reference);
        } else {
            path = GetPortableObjectPath(package, reference);
        }
        const auto found = persistentQualifiedObjects.find(path);
        return found == persistentQualifiedObjects.end() ? nullptr : found->second;
    };

    for (std::size_t localIndex = 0; localIndex < package.exports.size(); ++localIndex) {
        RuntimeObject* object = persistentRuntime->get()->exports[first + localIndex];
        const ExportTableEntry& entry = package.exports[localIndex];
        object->outer = resolve(entry.ObjOuter);
        object->base = resolve(entry.ObjBase);
        object->cls = resolve(entry.ObjClass);
        if (object->outer) object->references.push_back(object->outer);
        if (object->base) object->references.push_back(object->base);
        if (object->cls) {
            object->references.push_back(object->cls);
            ++summary.resolvedClasses;
        } else if (entry.ObjClass != 0) {
            ++summary.unresolvedClasses;
        }
        if (IsDerivedFromPath(object->cls, "Engine.Actor") && entry.ObjSize > 0) {
            const PortablePropertyStream properties =
                LoadPortableExportProperties(package, localIndex);
            object->instanceProperties = properties.properties;
            object->serializedStack = properties.stack;
            if (properties.stack) {
                object->serializedStateCode = resolve(properties.stack->stateReference);
                object->serializedFunctionCode = resolve(properties.stack->functionReference);
                ++summary.serializedActorStacks;
                for (const auto reference : {properties.stack->functionReference,
                                             properties.stack->stateReference}) {
                    if (auto* target = resolve(reference)) object->references.push_back(target);
                }
            }
            CacheRuntimeNameProperties(object, package, object->instanceProperties);
            for (const PortableTaggedProperty& property : object->instanceProperties) {
                if (property.name == "Tag" && property.type == 6u) {
                    const std::string tag = LowerAscii(
                        DecodePortableNameProperty(package, property));
                    if (!tag.empty() && tag != "none") {
                        persistentMapTagIndex[tag].push_back(object);
                    }
                }
                if ((property.type != 5u && property.type != 8u) ||
                    property.value.empty()) continue;
                const std::int32_t reference = DecodePortableObjectReference(property);
                const std::string path = reference == 0 ? std::string() : reference > 0
                    ? packageName + "." + GetPortableObjectPath(package, reference)
                    : GetPortableObjectPath(package, reference);
                if (property.type == 5u) QuestVr::SetActorTextureOverride(
                    object->textureOverrides, property.name.ToString(), property.arrayIndex, path);
                object->objectPropertyPaths[property.name.ToString()] = path;
                if (reference == 0) continue;
                if (RuntimeObject* target = resolve(reference)) {
                    object->references.push_back(target);
                }
            }
            summary.actorProperties += object->instanceProperties.size();
            ++summary.actors;
        }
        CacheInventoryTableMetadata(object, package, entry);
    }
    summary.exports = graph.objects.size();
    persistentMapPackageName = packageName;
    persistentMapSourcePath = package.sourcePath;
    RelinkAuthoredActorBases(package, first);
    ++persistentWorldRevision;
    GC::Collect();
    summary.passed = summary.exports == package.exports.size() &&
        summary.actors != 0 && summary.actorProperties != 0 &&
        summary.resolvedClasses != 0;
    return summary;
}

std::size_t UnloadPortableRuntimeMap() {
    if (GetPortableRuntimeUnsavedScriptState())
        throw std::runtime_error("Map unload refused: script state requires a per-map archive");
    if (!persistentRuntime || !persistentRuntime->get() ||
        persistentRuntime->get()->exports.size() <= persistentScriptExportCount) {
        if (persistentRuntime && persistentRuntime->get()) persistentRuntime->get()->nativeDecals = nullptr;
        persistentMapPackageName.clear();
        persistentMapTagIndex.clear();
        return 0;
    }
    const std::size_t removed =
        persistentRuntime->get()->exports.size() - persistentScriptExportCount;
    ClearNativePlayerEdges();
    if (!persistentMapPackageName.empty()) {
        const std::string prefix = persistentMapPackageName + ".";
        for (auto it = persistentQualifiedObjects.begin();
             it != persistentQualifiedObjects.end();) {
            if (it->first.compare(0, prefix.size(), prefix) == 0) {
                persistentVmObjects.erase(LowerAscii(it->first));
                it = persistentQualifiedObjects.erase(it);
            } else {
                ++it;
            }
        }
    }
    persistentRuntime->get()->nativeDecals = nullptr;
    persistentRuntime->get()->exports.resize(persistentScriptExportCount);
    ++persistentWorldRevision;
    persistentMapPackageName.clear();
    persistentMapTagIndex.clear();
    persistentWorldActors.clear(); persistentAuthoredWorldSlots = 0u; persistentMapSourcePath.clear();
    persistentActorCollision.Clear();
    GC::Collect();
    return removed;
}

static std::vector<PortableActorSnapshot> ReadPortableRuntimeActors(const bool includeInactive, const bool moversOnly,
    const bool changedMoversOnly = false) {
    std::vector<PortableActorSnapshot> snapshots;
    if (!persistentRuntime || !persistentRuntime->get()) return snapshots;
    // A VR motion frame must not walk the entire loaded script/object export
    // population just to publish two door leaves. The world registry already
    // owns the live map/born actor order and removes retired actors on unload.
    const auto& candidates = moversOnly
        ? persistentWorldActors : persistentRuntime->get()->exports;
    for (std::size_t index = moversOnly ? 0u : persistentScriptExportCount;
         index < candidates.size(); ++index) {
        RuntimeObject* object = candidates[index];
        if (!object) continue;
        if (changedMoversOnly && (!object->moverMotion ||
            (object->moverMotion->state.phase == QuestVr::MoverMotionPhase::Idle &&
             !object->moverMotion->beforeStep))) continue;
        if (object->worldActorIndex < 0 || object->worldRemoved || (!includeInactive && !object->active) ||
            !IsDerivedFromPath(object->cls, "Engine.Actor")) continue;
        if (moversOnly && !IsDerivedFromPath(object->cls, "Engine.Mover")) continue;
        PortableActorSnapshot snapshot;
        snapshot.objectPath = object->reflection.objectPath;
        snapshot.classPath = object->cls ? object->cls->reflection.objectPath : std::string();
        snapshot.pawn = IsDerivedFromPath(object->cls, "Engine.Pawn");
        snapshot.inventory = IsDerivedFromPath(object->cls, "Engine.Inventory");
        snapshot.decoration = IsDerivedFromPath(object->cls, "Engine.Decoration");
        snapshot.mover = IsDerivedFromPath(object->cls, "Engine.Mover");
        snapshot.trigger = IsDerivedFromPath(object->cls, "Engine.Triggers");
        snapshot.travel = IsDerivedFromPath(object->cls, "DeusEx.MapExit") ||
            IsDerivedFromPath(object->cls, "Engine.Teleporter");
        snapshot.light = IsDerivedFromPath(object->cls, "Engine.Light");
        snapshot.activated = object->activated;
        snapshot.collideActors = ReadInheritedRuntimeBool(object, "bCollideActors");
        snapshot.blockPlayers = ReadInheritedRuntimeBool(object, "bBlockPlayers");
        const auto resolveInheritedObjectProperty = [&](const std::string& name) {
            if (const auto* value = FindScriptOverlay(object, name.c_str())) return value->text;
            const auto instance = object->objectPropertyPaths.find(name);
            if (instance != object->objectPropertyPaths.end()) return instance->second;
            for (RuntimeObject* cls = object->cls; cls != nullptr; cls = cls->base) {
                const auto found = cls->objectPropertyPaths.find(name);
                if (found != cls->objectPropertyPaths.end()) return found->second;
            }
            return std::string();
        };
        const auto inheritedProperty = [&](const std::string& name)
            -> const PortableTaggedProperty* {
            return FindInheritedRuntimeProperty(object, name.c_str());
        };
        if (const PortableTaggedProperty* drawScale = inheritedProperty("DrawScale")) {
            if (drawScale->value.size() == 4u) {
                std::memcpy(&snapshot.drawScale, drawScale->value.data(), sizeof(float));
            }
        }
        if (const PortableTaggedProperty* drawScale3D = inheritedProperty("DrawScale3D")) {
            if (drawScale3D->value.size() == 12u) {
                std::memcpy(&snapshot.drawScaleX, drawScale3D->value.data(), sizeof(float));
                std::memcpy(&snapshot.drawScaleY, drawScale3D->value.data() + 4, sizeof(float));
                std::memcpy(&snapshot.drawScaleZ, drawScale3D->value.data() + 8, sizeof(float));
            }
        }
        if (const auto* prePivot = inheritedProperty("PrePivot")) {
            if (prePivot->type == 10u && prePivot->value.size() == 12u) {
                std::memcpy(&snapshot.prePivotX, prePivot->value.data(), sizeof(float));
                std::memcpy(&snapshot.prePivotY, prePivot->value.data() + 4u, sizeof(float));
                std::memcpy(&snapshot.prePivotZ, prePivot->value.data() + 8u, sizeof(float));
            }
        }
        // FScale begins with its three float Scale vector components. Its
        // remaining SheerRate/SheerAxis fields are not applied by pinned BSP
        // brush rendering either; malformed/truncated data keeps identity.
        if (const auto* mainScale = inheritedProperty("MainScale")) {
            if (mainScale->type == 10u && mainScale->value.size() >= 12u) {
                std::memcpy(&snapshot.mainScaleX, mainScale->value.data(), sizeof(float));
                std::memcpy(&snapshot.mainScaleY, mainScale->value.data() + 4u, sizeof(float));
                std::memcpy(&snapshot.mainScaleZ, mainScale->value.data() + 8u, sizeof(float));
            }
        }
        if (const PortableTaggedProperty* rotation = inheritedProperty("Rotation")) {
            if (rotation->value.size() == 12u) {
                std::memcpy(&snapshot.pitch, rotation->value.data(), sizeof(std::int32_t));
                std::memcpy(&snapshot.yaw, rotation->value.data() + 4, sizeof(std::int32_t));
                std::memcpy(&snapshot.roll, rotation->value.data() + 8, sizeof(std::int32_t));
            }
        }
        snapshot.meshPath = resolveInheritedObjectProperty("Mesh");
        const auto mesh = persistentVmObjects.find(LowerAscii(snapshot.meshPath));
        if (mesh != persistentVmObjects.end()) {
            snapshot.meshClassPath = mesh->second->reflection.metaClass;
        }
        snapshot.animByOwner = ReadInheritedRuntimeBool(object, "bAnimByOwner");
        snapshot.ownerPath = resolveInheritedObjectProperty("Owner");
        RuntimeObject* animationSource = object;
        if (snapshot.animByOwner && !snapshot.ownerPath.empty()) {
            const auto owner = persistentVmObjects.find(LowerAscii(snapshot.ownerPath));
            // Pinned VisibleMesh chooses the immediate owner only, not the
            // owner's own animation source. A destroyed/missing owner cannot
            // provide a live animation snapshot.
            if (owner != persistentVmObjects.end() && owner->second != nullptr &&
                owner->second->active && IsDerivedFromPath(owner->second->cls, "Engine.Actor"))
                animationSource = owner->second;
        }
        snapshot.animationSourcePath = animationSource->reflection.objectPath;
        snapshot.animation = ReadRuntimeAnimationSnapshot(animationSource);
        snapshot.materialOverrides = object->textureOverrides;
        for (const auto* name : {"Skin","Texture","MultiSkins"})
            for (std::uint32_t slot=0u;slot<(std::string_view(name)=="MultiSkins" ? 8u : 1u);++slot)
                if (const auto* value=FindScriptOverlay(object,name,slot))
                    QuestVr::SetActorTextureOverride(snapshot.materialOverrides,name,slot,value->text);
        for (RuntimeObject* cls = object->cls; cls != nullptr; cls = cls->base)
            QuestVr::InheritActorTextureOverrides(snapshot.materialOverrides, cls->textureOverrides);
        snapshot.texturePath = snapshot.materialOverrides.texture.path;
        snapshot.brushPath = resolveInheritedObjectProperty("Brush");
        snapshot.ambientSoundPath = resolveInheritedObjectProperty("AmbientSound");
        const auto readInheritedByte = [&](const char* name, std::uint8_t fallback) {
            if (const auto* value = FindScriptOverlay(object, name))
                return static_cast<std::uint8_t>(QuestVr::Vm::ToInt(*value));
            const PortableTaggedProperty* property = inheritedProperty(name);
            return property != nullptr && !property->value.empty()
                ? property->value.front()
                : fallback;
        };
        snapshot.soundRadius = readInheritedByte("SoundRadius", snapshot.soundRadius);
        snapshot.fatness = readInheritedByte("Fatness", snapshot.fatness);
        snapshot.drawType = readInheritedByte("DrawType", snapshot.drawType);
        snapshot.style = readInheritedByte("Style", snapshot.style);
        if (const PortableTaggedProperty* hidden = inheritedProperty("bHidden")) {
            if (hidden->type == 3u) snapshot.hidden = hidden->boolValue;
        }
        const auto readInheritedBool = [&](const char* name) {
            return ReadInheritedRuntimeBool(object, name);
        };
        snapshot.unlit = readInheritedBool("bUnlit");
        snapshot.noSmooth = readInheritedBool("bNoSmooth");
        snapshot.meshEnvironmentMap = readInheritedBool("bMeshEnviroMap");
        snapshot.soundVolume = readInheritedByte("SoundVolume", snapshot.soundVolume);
        snapshot.soundPitch = readInheritedByte("SoundPitch", snapshot.soundPitch);
        snapshot.lightType = readInheritedByte("LightType", snapshot.lightType);
        snapshot.lightEffect = readInheritedByte("LightEffect", snapshot.lightEffect);
        // Engine.Light is a convenient authoring class, not an emission
        // requirement: inherited light properties also make decorations and
        // other actor classes emit, as in pinned LightSystem::BeginFrame.
        snapshot.light = snapshot.light || snapshot.lightType != 0u;
        snapshot.lightBrightness = readInheritedByte(
            "LightBrightness", snapshot.lightBrightness);
        snapshot.lightHue = readInheritedByte("LightHue", snapshot.lightHue);
        snapshot.lightSaturation = readInheritedByte(
            "LightSaturation", snapshot.lightSaturation);
        snapshot.lightRadius = readInheritedByte("LightRadius", snapshot.lightRadius);
        snapshot.lightCone = readInheritedByte("LightCone", snapshot.lightCone);
        snapshot.ambientHue = readInheritedByte("AmbientHue", snapshot.ambientHue);
        snapshot.ambientSaturation = readInheritedByte("AmbientSaturation", snapshot.ambientSaturation);
        snapshot.ambientBrightness = readInheritedByte("AmbientBrightness", snapshot.ambientBrightness);
        const PortableTaggedProperty* destination = inheritedProperty("DestMap");
        if (destination == nullptr) destination = inheritedProperty("URL");
        if (destination != nullptr) {
            if (destination->type == 13u && !destination->value.empty()) {
                snapshot.destinationMap = DecodePortableStringProperty(*destination);
            }
        }
        for (const auto* name : {"DestMap","URL"})
            if (const auto* value=FindScriptOverlay(object,name)) {snapshot.destinationMap=value->text;break;}
        for (const PortableTaggedProperty& property : object->instanceProperties) {
            if (property.name == "Location" && property.type == 10u &&
                property.value.size() == 12u) {
                std::memcpy(&snapshot.x, property.value.data(), sizeof(float));
                std::memcpy(&snapshot.y, property.value.data() + 4, sizeof(float));
                std::memcpy(&snapshot.z, property.value.data() + 8, sizeof(float));
                snapshot.hasLocation = std::isfinite(snapshot.x) &&
                    std::isfinite(snapshot.y) && std::isfinite(snapshot.z);
                break;
            }
        }
        // VM assignments live in an explicit per-actor overlay. Geometry must
        // consume these same values, not continue displaying authored tags.
        if (const auto* value = FindScriptOverlay(object, "DrawScale"))
            snapshot.drawScale = QuestVr::Vm::ToFloat(*value);
        if (const auto* value = FindScriptOverlay(object, "DrawScale3D")) {
            snapshot.drawScaleX = value->vector[0]; snapshot.drawScaleY = value->vector[1];
            snapshot.drawScaleZ = value->vector[2];
        }
        if (const auto* value = FindScriptOverlay(object, "PrePivot")) {
            snapshot.prePivotX = value->vector[0]; snapshot.prePivotY = value->vector[1];
            snapshot.prePivotZ = value->vector[2];
        }
        if (const auto* value = FindScriptOverlay(object,"MainScale")) {
            const auto scale=value->fields.find("scale");
            if (scale!=value->fields.end() && scale->second.kind==QuestVr::Vm::Kind::Vector) {
                snapshot.mainScaleX=scale->second.vector[0];snapshot.mainScaleY=scale->second.vector[1];snapshot.mainScaleZ=scale->second.vector[2];
            }
        }
        if (const auto* value = FindScriptOverlay(object, "Rotation")) {
            snapshot.pitch = value->rotation[0]; snapshot.yaw = value->rotation[1];
            snapshot.roll = value->rotation[2];
        }
        if (const auto* value = FindScriptOverlay(object, "bHidden"))
            snapshot.hidden = QuestVr::Vm::ToBool(*value);
        if (const auto* value = FindScriptOverlay(object, "Location")) {
            snapshot.x = value->vector[0]; snapshot.y = value->vector[1]; snapshot.z = value->vector[2];
            snapshot.hasLocation = std::isfinite(snapshot.x) && std::isfinite(snapshot.y) && std::isfinite(snapshot.z);
        }
        if (snapshot.mover && object->moverMotion) {
            const auto& motion = *object->moverMotion;
            const auto& pose = motion.state.pose;
            snapshot.x = pose.location.x; snapshot.y = pose.location.y; snapshot.z = pose.location.z;
            snapshot.pitch = pose.rotation.pitch; snapshot.yaw = pose.rotation.yaw; snapshot.roll = pose.rotation.roll;
            snapshot.hasLocation = true;
            snapshot.moverMoving = motion.state.phase == QuestVr::MoverMotionPhase::Moving;
            snapshot.moverClosing = (snapshot.moverMoving && motion.state.targetKey == 0u) ||
                (motion.beforeStep && motion.beforeStep->phase == QuestVr::MoverMotionPhase::Moving &&
                 motion.beforeStep->targetKey == 0u);
        }
        snapshots.push_back(std::move(snapshot));
    }
    return snapshots;
}

std::vector<PortableActorSnapshot> GetPortableRuntimeMapActors(bool includeInactive) {
    return ReadPortableRuntimeActors(includeInactive, false);
}

std::vector<PortableActorSnapshot> GetPortableRuntimeMoverActors(const bool changedOnly) {
    return ReadPortableRuntimeActors(false, true, changedOnly);
}

bool AdvancePortableRuntimeMovers(const float elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0f)
        throw std::runtime_error("Mover elapsed time must be finite and nonnegative");
    if (!persistentRuntime || !persistentRuntime->get() || elapsedSeconds == 0.0f) return false;
    // Discard long background/loading stalls, rather than teleporting a door
    // through the player. This demo layer is not a swept encroachment solver.
    const float elapsed = std::min(elapsedSeconds, 0.05f);
    bool changed{};
    for (auto* actor : persistentWorldActors) {
        if (!actor || !actor->active || actor->worldRemoved || !actor->moverMotion) continue;
        auto& motion = *actor->moverMotion;
        motion.beforeStep.reset();
        const bool commandPending = motion.commandPublicationPending;
        if (motion.state.phase == QuestVr::MoverMotionPhase::Idle && !commandPending) continue;
        motion.beforeStep = motion.state;
        const bool moved = motion.state.phase != QuestVr::MoverMotionPhase::Idle &&
            QuestVr::AdvanceMoverMotion(motion.spec, motion.state, elapsed);
        motion.commandPublicationPending = false;
        changed = commandPending || moved || changed;
        actor->activated = motion.state.targetKey != 0u;
    }
    return changed;
}

bool BlockPortableRuntimeMover(const std::string& objectPath) {
    const auto found = persistentVmObjects.find(LowerAscii(objectPath));
    if (found == persistentVmObjects.end() || !found->second || !found->second->moverMotion) return false;
    auto* actor = found->second;
    auto& motion = *actor->moverMotion;
    if (!motion.beforeStep || motion.beforeStep->phase != QuestVr::MoverMotionPhase::Moving ||
        motion.beforeStep->targetKey != 0u) return false;
    // Stage the whole pair at its prior safe sample. A failed key/layout or
    // allocation preflight cannot rewind only one leaf and leak a partial edit.
    return RequestRuntimeMover(actor, true, false, true);
}

namespace {
PortableActorMeshSummary DecodeRuntimeActorAssets(const std::vector<PortableActorSnapshot>& actors) {
    PortableActorMeshSummary summary;
    std::set<std::string> meshPaths;
    std::set<std::string> brushPaths;
    for (const PortableActorSnapshot& actor : actors) {
        if (!actor.meshPath.empty()) meshPaths.insert(actor.meshPath);
        if (actor.mover && !actor.brushPath.empty()) brushPaths.insert(actor.brushPath);
    }
    summary.referencedMeshes = meshPaths.size();
    summary.referencedBrushes = brushPaths.size();
    std::unordered_map<std::string, PortablePackageTables> packages;
    for (const std::string& meshPath : meshPaths) {
        const auto found = persistentVmObjects.find(LowerAscii(meshPath));
        if (found == persistentVmObjects.end()) continue;
        RuntimeObject* meshObject = found->second;
        if (meshObject->lodMesh) {
            summary.triangleVertices+=meshObject->lodMesh->triangles.size();
            ++summary.decodedMeshes; continue;
        }
        auto package = packages.find(meshObject->sourcePath);
        if (package == packages.end()) {
            package = packages.emplace(
                meshObject->sourcePath,
                LoadPortablePackageTables(meshObject->sourcePath)).first;
        }
        meshObject->lodMesh = LoadCompleteRuntimeLodMesh(package->second, meshObject->exportIndex, meshObject->sourcePath);
        summary.triangleVertices += meshObject->lodMesh->triangles.size();
        ++summary.decodedMeshes;
    }
    for (const std::string& brushPath : brushPaths) {
        const auto found = persistentVmObjects.find(LowerAscii(brushPath));
        if (found == persistentVmObjects.end()) continue;
        RuntimeObject* brushObject = found->second;
        if (brushObject->brushMesh) {
            summary.brushTriangleVertices+=brushObject->brushMesh->triangles.size();
            ++summary.decodedBrushes; continue;
        }
        auto package = packages.find(brushObject->sourcePath);
        if (package == packages.end()) {
            package = packages.emplace(
                brushObject->sourcePath,
                LoadPortablePackageTables(brushObject->sourcePath)).first;
        }
        try {
            brushObject->brushMesh = std::make_unique<PortableLodMesh>(
                LoadPortableBrushMesh(package->second, brushObject->exportIndex));
            summary.brushTriangleVertices += brushObject->brushMesh->triangles.size();
            ++summary.decodedBrushes;
        } catch (const std::exception& error) {
            __android_log_print(
                ANDROID_LOG_WARN,
                "quest_main",
                "DeusExQuest: mover brush fallback for %s: %s",
                brushPath.c_str(),
                error.what());
        }
    }
    summary.passed = summary.referencedMeshes != 0 &&
        summary.decodedMeshes == summary.referencedMeshes &&
        summary.triangleVertices != 0;
    return summary;
}
} // namespace

PortableActorMeshSummary DecodePortableRuntimeActorMeshes() {
    return DecodeRuntimeActorAssets(GetPortableRuntimeMapActors(true));
}

const PortableLodMesh& GetPortableRuntimeMeshView(const std::string& meshPath) {
    const auto found = persistentVmObjects.find(LowerAscii(meshPath));
    if (found == persistentVmObjects.end() || !found->second->lodMesh) {
        throw std::runtime_error("Portable actor mesh is not decoded: " + meshPath);
    }
    return *found->second->lodMesh;
}

const PortableLodMesh& GetPortableRuntimeBrushView(const std::string& brushPath) {
    const auto found = persistentVmObjects.find(LowerAscii(brushPath));
    if (found == persistentVmObjects.end() || !found->second->brushMesh) {
        throw std::runtime_error("Portable actor brush is not decoded: " + brushPath);
    }
    return *found->second->brushMesh;
}

PortableLodMesh GetPortableRuntimeMesh(const std::string& meshPath) {
    return GetPortableRuntimeMeshView(meshPath);
}

PortableLodMesh GetPortableRuntimeBrush(const std::string& brushPath) {
    return GetPortableRuntimeBrushView(brushPath);
}

PortableTextureArray BuildPortableRuntimeActorTextureArray(
    std::uint32_t width,
    std::uint32_t height) {
    if (width == 0 || height == 0 || width > 2048 || height > 2048) {
        throw std::runtime_error("Portable actor texture array dimensions are invalid");
    }
    std::set<std::string> paths;
    std::set<std::string> maskedPaths;
    for (const PortableActorSnapshot& actor : GetPortableRuntimeMapActors(true)) {
        const bool sprite = actor.drawType == 1u || actor.drawType == 4u || actor.drawType == 5u || actor.drawType == 7u;
        if (sprite && !actor.texturePath.empty()) paths.insert(actor.texturePath);
        const auto collectMesh = [&](const std::string& path, bool brush) {
            if (path.empty()) return;
            const auto found = persistentVmObjects.find(LowerAscii(path));
            if (found == persistentVmObjects.end()) return;
            const auto* geometry = brush ? found->second->brushMesh.get() : found->second->lodMesh.get();
            if (geometry == nullptr) return;
            // Pack materials actually selected by each original actor, rather
            // than every unused mesh default/override. Inactive actors remain
            // included so pickup/quickload cannot renumber retained materials.
            std::set<std::uint16_t> materials;
            for (const auto& vertex : geometry->triangles) materials.insert(vertex.material);
            for (const auto material : materials) {
                const auto choice = QuestVr::ResolveActorMeshMaterial(actor.materialOverrides,
                    geometry->texturePaths,geometry->materialTextureIndices,material);
                if (!choice.texturePath.empty()) paths.insert(choice.texturePath);
            }
        };
        collectMesh(actor.meshPath, false);
        if (actor.mover) collectMesh(actor.brushPath, true);
        const auto collectMasked = [&](const std::string& path, bool brush) {
            const auto found = persistentVmObjects.find(LowerAscii(path));
            if (found == persistentVmObjects.end()) return;
            const auto* mesh = brush ? found->second->brushMesh.get() : found->second->lodMesh.get();
            if (mesh == nullptr) return;
            for (const auto& vertex : mesh->triangles) {
                if ((vertex.polyFlags & 2u) == 0u && actor.style != 2u) continue;
                const auto choice = QuestVr::ResolveActorMeshMaterial(actor.materialOverrides,
                    mesh->texturePaths,mesh->materialTextureIndices,vertex.material);
                if (!choice.texturePath.empty()) maskedPaths.insert(choice.texturePath);
            }
        };
        collectMasked(actor.meshPath,false);
        if (actor.mover) collectMasked(actor.brushPath,true);
        if (actor.style == 2u && !actor.texturePath.empty()) maskedPaths.insert(actor.texturePath);
    }
    if (paths.size() > 255) {
        throw std::runtime_error("Portable actor texture array exceeds shader layer limit");
    }

    PortableTextureArray result;
    result.width = width;
    result.height = height;
    result.texturePaths.assign(paths.begin(), paths.end());
    result.texturePolyFlags.assign(result.texturePaths.size(), 0u);
    constexpr std::size_t maximumMaterialBytes = 256u*1024u*1024u;
    const auto initialBytes = static_cast<std::size_t>(width)*height*result.texturePaths.size()*4u;
    if (initialBytes > maximumMaterialBytes) throw std::runtime_error("Actor material array exceeds byte budget");
    result.rgba.reserve(initialBytes);
    std::unordered_map<std::string, PortablePackageTables> packages;
    std::string gameRoot;
    if (persistentRuntime && !persistentRuntime->get()->exports.empty()) {
        const std::string& source = persistentRuntime->get()->exports.front()->sourcePath;
        const std::size_t systemSlash = source.find_last_of("/\\");
        const std::size_t rootSlash = systemSlash == std::string::npos
            ? std::string::npos
            : source.find_last_of("/\\", systemSlash - 1);
        if (rootSlash != std::string::npos) gameRoot = source.substr(0, rootSlash);
    }
    const std::size_t pixelsPerLayer = static_cast<std::size_t>(width) * height;
    for (const std::string& qualified : result.texturePaths) {
        try {
            const std::size_t separator = qualified.find('.');
            if (separator == std::string::npos || gameRoot.empty()) {
                throw std::runtime_error("qualified texture path is invalid");
            }
            const std::string packageName = qualified.substr(0, separator);
            const std::string objectPath = qualified.substr(separator + 1);
            std::string packagePath = ResolveRuntimePackagePath(gameRoot, packageName);
            std::size_t textureExport = std::numeric_limits<std::size_t>::max();
            const auto runtimeTexture = persistentVmObjects.find(LowerAscii(qualified));
            if (runtimeTexture != persistentVmObjects.end()) {
                packagePath = runtimeTexture->second->sourcePath;
                textureExport = runtimeTexture->second->exportIndex;
            } else {
                std::FILE* packageProbe = std::fopen(packagePath.c_str(), "rb");
                if (packageProbe == nullptr) {
                    __android_log_print(
                        ANDROID_LOG_WARN,
                        "quest_main",
                        "DeusExQuest: actor texture package missing for %s (%s)",
                        qualified.c_str(),
                        packagePath.c_str());
                    for (std::size_t pixel = 0; pixel < pixelsPerLayer; ++pixel) {
                        const bool dark =
                            ((pixel / width) / 8u + (pixel % width) / 8u) % 2u != 0;
                        result.rgba.push_back(dark ? 70u : 125u);
                        result.rgba.push_back(dark ? 74u : 130u);
                        result.rgba.push_back(dark ? 78u : 135u);
                        result.rgba.push_back(255u);
                    }
                    ++result.failedTextures;
                    continue;
                }
                std::fclose(packageProbe);
            }
            auto package = packages.find(packagePath);
            if (package == packages.end()) {
                package = packages.emplace(
                    packagePath,
                    LoadPortablePackageTables(packagePath)).first;
            }
            if (textureExport == std::numeric_limits<std::size_t>::max()) {
                textureExport = FindPortableTextureExport(package->second, objectPath);
            }
            const std::string textureClass = runtimeTexture == persistentVmObjects.end()
                ? GetPortableObjectPath(
                    package->second,
                    package->second.exports.at(textureExport).ObjClass)
                : runtimeTexture->second->reflection.metaClass;
            const std::size_t classSeparator = textureClass.find_last_of('.');
            const std::string leafClass = classSeparator == std::string::npos
                ? textureClass
                : textureClass.substr(classSeparator + 1);
            if (leafClass != "Texture") {
                __android_log_print(
                    ANDROID_LOG_WARN,
                    "quest_main",
                    "DeusExQuest: actor texture fallback for %s: unsupported class %s",
                    qualified.c_str(),
                    leafClass.c_str());
                for (std::size_t pixel = 0; pixel < pixelsPerLayer; ++pixel) {
                    const bool dark = ((pixel / width) / 8u + (pixel % width) / 8u) % 2u != 0;
                    result.rgba.push_back(dark ? 70u : 125u);
                    result.rgba.push_back(dark ? 74u : 130u);
                    result.rgba.push_back(dark ? 78u : 135u);
                    result.rgba.push_back(255u);
                }
                ++result.failedTextures;
                continue;
            }
            const PortablePropertyStream properties =
                LoadPortableExportProperties(package->second, textureExport);
            for (const auto& property : properties.properties) {
                // Original UTexture's native PolyFlags are script bitfields.
                const std::size_t selectedLayer = static_cast<std::size_t>(&qualified-result.texturePaths.data());
                if (property.name == "bMasked" && property.type == 3u && property.arrayIndex == 0u) {
                    if (property.boolValue) result.texturePolyFlags[selectedLayer] |= 2u;
                    else result.texturePolyFlags[selectedLayer] &= ~2u;
                }
                if (property.name == "PolyFlags" && property.type == 2u &&
                    property.arrayIndex == 0u && property.value.size() == 4u) {
                    const std::size_t layer = static_cast<std::size_t>(
                        &qualified - result.texturePaths.data());
                    std::memcpy(&result.texturePolyFlags[layer], property.value.data(), sizeof(std::uint32_t));
                }
            }
            std::vector<PortableMipmap> mipmaps =
                LoadPortableTextureMipmaps(package->second, textureExport);
            std::vector<std::uint32_t> palette;
            for (const PortableTaggedProperty& property : properties.properties) {
                if (property.name == "Palette") {
                    const std::int32_t paletteReference = DecodePortableObjectReference(property);
                    if (paletteReference <= 0) {
                        throw std::runtime_error("texture palette is not a local export");
                    }
                    palette = LoadPortablePalette(
                        package->second, static_cast<std::size_t>(paletteReference - 1));
                    break;
                }
            }
            if (mipmaps.empty() || palette.empty()) {
                throw std::runtime_error("texture has no indexed mip or palette");
            }
            const PortableMipmap& mip = mipmaps.front();
            if (mip.width == 0 || mip.height == 0 ||
                mip.pixels.size() != static_cast<std::size_t>(mip.width) * mip.height) {
                throw std::runtime_error("texture top mip is malformed");
            }
            if (*std::max_element(mip.pixels.begin(), mip.pixels.end()) >= palette.size())
                throw std::runtime_error("texture top mip refers outside its palette");
            for (std::uint32_t y = 0; y < height; ++y) {
                const std::uint32_t sourceY = y * mip.height / height;
                for (std::uint32_t x = 0; x < width; ++x) {
                    const std::uint32_t sourceX = x * mip.width / width;
                    const std::uint8_t paletteIndex =
                        mip.pixels[static_cast<std::size_t>(sourceY) * mip.width + sourceX];
                    const std::uint32_t color = palette.at(paletteIndex);
                    const auto texel = QuestVr::ActorPaletteTexel(color, paletteIndex);
                    result.rgba.insert(result.rgba.end(), texel.begin(), texel.end());
                }
            }
            ++result.decodedTextures;
        } catch (const std::exception& error) {
            __android_log_print(
                ANDROID_LOG_WARN,
                "quest_main",
                "DeusExQuest: actor texture fallback for %s: %s",
                qualified.c_str(),
                error.what());
            for (std::size_t pixel = 0; pixel < pixelsPerLayer; ++pixel) {
                const bool dark = ((pixel / width) / 8u + (pixel % width) / 8u) % 2u != 0;
                result.rgba.push_back(dark ? 70u : 125u);
                result.rgba.push_back(dark ? 74u : 130u);
                result.rgba.push_back(dark ? 78u : 135u);
                result.rgba.push_back(255u);
            }
            ++result.failedTextures;
        }
    }
    const std::size_t baseLayers = result.texturePaths.size();
    result.maskedTextureLayers.assign(baseLayers,-1);
    for (std::size_t layer = 0u; layer < baseLayers; ++layer) {
        if ((result.texturePolyFlags[layer] & 2u) == 0u && maskedPaths.count(result.texturePaths[layer]) == 0u) continue;
        if (result.texturePaths.size() >= 255u) throw std::runtime_error("Masked actor variants exceed shader layer limit");
        if (pixelsPerLayer*4u > maximumMaterialBytes-result.rgba.size())
            throw std::runtime_error("Masked actor variants exceed byte budget");
        const auto start = result.rgba.begin()+static_cast<std::ptrdiff_t>(layer*pixelsPerLayer*4u);
        std::vector<std::uint8_t> masked(start,start+static_cast<std::ptrdiff_t>(pixelsPerLayer*4u));
        // Pinned GL texture manager keeps opaque and masked P8 uploads apart.
        // Black RGB at the transparent key prevents magenta bilinear fringes.
        for (std::size_t pixel = 0u; pixel < pixelsPerLayer; ++pixel)
            if (masked[pixel*4u+3u] == 0u) masked[pixel*4u] = masked[pixel*4u+1u] = masked[pixel*4u+2u] = 0u;
        result.maskedTextureLayers[layer] = static_cast<std::int32_t>(result.texturePaths.size());
        result.texturePaths.push_back(result.texturePaths[layer]+"#masked");
        result.texturePolyFlags.push_back(result.texturePolyFlags[layer]|2u);
        result.maskedTextureLayers.push_back(-1);
        result.rgba.insert(result.rgba.end(),masked.begin(),masked.end());
        ++result.maskedTextureVariants;
    }
    result.passed = !result.texturePaths.empty() && result.decodedTextures != 0 &&
        result.rgba.size() == pixelsPerLayer * result.texturePaths.size() * 4u;
    return result;
}

PortableInteractionResult InteractPortableRuntimeActor(const std::string& objectPath) {
    PortableInteractionResult result;
    result.objectPath = objectPath;
    const auto found = persistentVmObjects.find(LowerAscii(objectPath));
    if (found == persistentVmObjects.end() || found->second == nullptr) {
        result.action = "missing";
        result.inventoryCount = persistentInventory.size();
        return result;
    }
    RuntimeObject* object = found->second;
    result.classPath = object->cls == nullptr
        ? object->reflection.metaClass
        : object->cls->reflection.objectPath;
    if (!object->active || object->worldRemoved) {
        result.action = "inactive";
    } else if (IsDerivedFromPath(object->cls, "DeusEx.MapExit") ||
               IsDerivedFromPath(object->cls, "Engine.Teleporter")) {
        result.handled = true;
        result.action = "map_exit";
        const auto decodeDestination = [&](const std::vector<PortableTaggedProperty>& properties) {
            for (const PortableTaggedProperty& property : properties) {
                if ((property.name == "DestMap" || property.name == "URL") &&
                    property.type == 13u && !property.value.empty()) {
                    result.destinationMap = DecodePortableStringProperty(property);
                    return true;
                }
            }
            return false;
        };
        const auto* copied=FindScriptOverlay(object,"DestMap");
        if (!copied) copied=FindScriptOverlay(object,"URL");
        if (copied) result.destinationMap=copied->text;
        else if (!decodeDestination(object->instanceProperties)) {
            for (RuntimeObject* cls = object->cls; cls != nullptr; cls = cls->base) {
                if (cls->classDescriptor && decodeDestination(cls->classDescriptor->defaults)) break;
            }
        }
    } else if (IsDerivedFromPath(object->cls, "Engine.Inventory")) {
        object->active = false;
        persistentInventory.push_back(objectPath);
        result.handled = true;
        result.worldChanged = true;
        result.action = "pickup";
    } else if (IsDerivedFromPath(object->cls, "Engine.Mover")) {
        const bool open = object->moverMotion ? object->moverMotion->state.targetKey == 0u : !object->activated;
        if (ReadInheritedRuntimeBool(object, "bLocked") && !HasRuntimeMoverKey(object)) {
            result.handled = true;
            result.action = "mover_locked";
        } else {
            try {
                result.handled = RequestRuntimeMover(object, open, true);
                result.worldChanged = result.handled;
                result.action = result.handled ? (open ? "mover_open" : "mover_close") : "mover_requires_trigger";
            } catch (const std::exception& error) {
                result.action = "mover_unsupported";
                __android_log_print(ANDROID_LOG_WARN, "quest_main", "Mover motion rejected: %.512s", error.what());
            }
        }
    } else if (IsDerivedFromPath(object->cls, "Engine.Triggers")) {
        object->activated = true;
        result.handled = true;
        result.action = "trigger";
        const auto event = LowerAscii(ReadInheritedRuntimeName(object, "Event"));
        const auto targets = persistentMapTagIndex.find(event);
        if (!event.empty() && event != "none" && targets != persistentMapTagIndex.end()) {
            for (auto* target : targets->second) {
                if (!target || !target->active || target->worldRemoved || !IsDerivedFromPath(target->cls, "Engine.Mover")) continue;
                try { result.worldChanged = RequestRuntimeMover(target, true, false) || result.worldChanged; }
                catch (const std::exception& error) {
                    __android_log_print(ANDROID_LOG_WARN, "quest_main", "Triggered mover rejected: %.512s", error.what());
                }
            }
        }
    } else if (IsDerivedFromPath(object->cls, "Engine.Pawn")) {
        object->activated = true;
        result.handled = true;
        result.action = "conversation";
    } else if (IsDerivedFromPath(object->cls, "Engine.Decoration")) {
        object->activated = !object->activated;
        result.handled = true;
        result.action = "decoration";
    } else {
        result.action = "unsupported";
    }
    result.inventoryCount = persistentInventory.size();
    return result;
}

PortableDamageResult DamagePortableRuntimeActor(
    const std::string& objectPath,
    float damage) {
    PortableDamageResult result;
    result.objectPath = objectPath;
    const auto found = persistentVmObjects.find(LowerAscii(objectPath));
    if (found == persistentVmObjects.end() || found->second == nullptr ||
        !std::isfinite(damage) || damage <= 0.0f) {
        return result;
    }
    RuntimeObject* object = found->second;
    if (!object->active || object->worldRemoved || !IsDerivedFromPath(object->cls, "Engine.Pawn")) return result;
    if (!object->healthInitialized) {
        const auto decodeHealth = [&](const std::vector<PortableTaggedProperty>& properties) {
            for (const PortableTaggedProperty& property : properties) {
                if (property.name == "Health" && property.type == 4u &&
                    property.value.size() == sizeof(float)) {
                    float value{};
                    std::memcpy(&value, property.value.data(), sizeof(value));
                    if (std::isfinite(value) && value > 0.0f) object->health = value;
                    return true;
                }
            }
            return false;
        };
        if (const auto* copied=FindScriptOverlay(object,"Health")) object->health=QuestVr::Vm::ToFloat(*copied);
        else if (!decodeHealth(object->instanceProperties)) {
            for (RuntimeObject* cls = object->cls; cls != nullptr; cls = cls->base) {
                if (cls->classDescriptor && decodeHealth(cls->classDescriptor->defaults)) break;
            }
        }
        object->healthInitialized = true;
    }
    object->health = std::max(0.0f, object->health - damage);
    result.handled = true;
    result.remainingHealth = object->health;
    if (object->health <= 0.0f) {
        object->active = false;
        result.killed = true;
        result.worldChanged = true;
    }
    return result;
}

bool VerifyPortableRuntimeDamage() {
    if (!persistentRuntime || !persistentRuntime->get()) return false;
    RuntimeObject* pawn{};
    for (std::size_t index = persistentScriptExportCount;
         index < persistentRuntime->get()->exports.size(); ++index) {
        RuntimeObject* object = persistentRuntime->get()->exports[index];
        if (object->active && IsDerivedFromPath(object->cls, "Engine.Pawn")) {
            pawn = object;
            break;
        }
    }
    if (pawn == nullptr) return false;
    const bool oldInitialized = pawn->healthInitialized;
    const float oldHealth = pawn->health;
    const bool oldActive = pawn->active;
    PortableDamageResult result =
        DamagePortableRuntimeActor(pawn->reflection.objectPath, 1'000'000.0f);
    const bool passed = result.handled && result.killed && result.worldChanged &&
        !pawn->active && result.remainingHealth == 0.0f;
    pawn->healthInitialized = oldInitialized;
    pawn->health = oldHealth;
    pawn->active = oldActive;
    return passed;
}

std::size_t GetPortableRuntimeInventoryCount() {
    return persistentInventory.size();
}

std::vector<std::string> GetPortableRuntimeInventoryItems() {
    return persistentInventory;
}

std::vector<PortableInventoryDescriptor> ReadPortableRuntimeInventoryDescriptors(
    const std::vector<std::string>& actorPaths, const PortableInventoryDescriptorLimits& limits) {
    using Status = PortableInventoryDescriptorStatus;
    using Origin = PortableInventoryValueOrigin;
    using TextStatus = PortableInventoryTextStatus;
    using Kind = QuestVr::Vm::Kind;
    if (limits.count == 0u || limits.count > 1024u || actorPaths.size() > limits.count ||
        limits.retainedBytes == 0u || limits.retainedBytes > 8u * 1024u * 1024u ||
        limits.stringBytes == 0u || limits.stringBytes > 8192u || limits.hierarchy == 0u || limits.hierarchy > 128u)
        throw std::runtime_error("Inventory descriptor request limits are invalid/exceeded");
    if (actorPaths.empty()) return {};
    std::size_t retained{};
    const auto charge = [&](const std::size_t bytes) {
        if (bytes > limits.retainedBytes || retained > limits.retainedBytes - bytes)
            throw std::runtime_error("Inventory descriptor aggregate retained/scratch byte budget exceeded");
        retained += bytes;
    };
    // Bound fixed result capacity and transient lookup/row/hierarchy storage,
    // excluding the already-owned runtime graph and borrowed tagged values.
    charge(2u * actorPaths.size() * sizeof(PortableInventoryDescriptor));
    // Extra bounded scratch covers FString decoding/transport validation and
    // its UTF-16 units while detached optional display strings are copied.
    charge(12u * (limits.stringBytes + 64u) + sizeof(PortableInventoryDescriptor) + 256u * sizeof(RuntimeObject*));
    std::vector<PortableInventoryDescriptor> results; results.reserve(actorPaths.size());
    if (results.capacity() > 2u * actorPaths.size())
        throw std::runtime_error("Inventory descriptor result capacity exceeds charged bound");
    const auto validText = [&](const std::string& text, const bool empty = false) {
        return (empty || !text.empty()) && text.size() <= limits.stringBytes && text.find('\0') == std::string::npos;
    };
    const auto copy = [&](std::string& target, const std::string& source, const bool empty = false) {
        if (!validText(source, empty)) return false;
        // Charge conservatively before allocation, then reject an unusual
        // implementation capacity rather than silently bypassing the bound.
        const auto allowance = 2u * (source.size() + 1u) + 32u;
        charge(allowance); target = source;
        if (target.capacity() + 1u > allowance)
            throw std::runtime_error("Inventory descriptor string capacity exceeds charged bound");
        return true;
    };
    const auto folded = [](const std::string& text) { return QuestVr::ObjectCastDetail::Fold(text); };
    const auto same = [](const std::string& left, const char* right) { return SameInventoryIdentity(left, right); };
    const auto classDerives = [&](RuntimeObject* cls, std::uint8_t native, RuntimeObject* target, const std::uint8_t targetNative) {
        std::array<RuntimeObject*, 128u> seen{}; std::size_t depth{}, indexedDepth{}; std::uint16_t nativeSeen{};
        const std::string_view wanted = target ? std::string_view(target->reflection.objectPath) : InventoryNativeClassPath(targetNative);
        if (wanted.empty() || wanted.size() > limits.stringBytes || wanted.find('\0') != std::string_view::npos) return false;
        while (cls || native != 0u) {
            if (depth++ == limits.hierarchy) return false;
            if (!cls) {
                const auto bit = static_cast<std::uint16_t>(1u << native);
                if ((nativeSeen & bit) != 0u) return false;
                nativeSeen |= bit;
                const auto path = InventoryNativeClassPath(native);
                const auto found = persistentVmObjects.find(folded(path));
                if (found != persistentVmObjects.end()) cls = found->second;
                else {
                    if (SameInventoryIdentity(path, wanted)) return true;
                    native = InventoryNativeClassBase(native); continue;
                }
            }
            if (!cls || !cls->inventoryTableMetadata || !cls->inventorySerializedClass || !cls->inventoryBaseReferenceValid ||
                cls->reflection.metaClass != "Class" || !validText(cls->reflection.objectPath) ||
                std::find(seen.begin(), seen.begin() + indexedDepth, cls) != seen.begin() + indexedDepth) return false;
            seen[indexedDepth++] = cls;
            if (SameInventoryIdentity(cls->reflection.objectPath, wanted)) return true;
            native = cls->base ? 0u : cls->inventoryNativeBase;
            // An actual zero-base non-Object UClass has Core.Object as its
            // pinned implicit base; a nonzero unresolved reference is invalid.
            if (!cls->base && native == 0u && !SameInventoryIdentity(cls->reflection.objectPath, "Core.Object")) native = 1u;
            cls = cls->base;
        }
        return false;
    };
    const auto iconClassValid = [&](const std::string& path, const InventoryIconClassConstraint* constraint) {
        if (!constraint || !constraint->valid || (!constraint->indexedClass && constraint->nativeClass == 0u)) return Status::MalformedProperty;
        if (constraint->indexedClass && (!constraint->indexedClass->inventoryTableMetadata || !constraint->indexedClass->inventorySerializedClass)) return Status::MalformedProperty;
        if (!classDerives(constraint->indexedClass, constraint->nativeClass, nullptr, 3u)) return Status::MalformedProperty;
        if (path.empty()) return Status::Available;
        const auto found = persistentVmObjects.find(folded(path));
        if (found == persistentVmObjects.end() || !found->second) return Status::MissingIconMetadata;
        const auto* asset = found->second;
        if (!asset->inventoryTableMetadata || asset->inventorySerializedClass || !asset->inventoryClassReferenceValid) return Status::MalformedProperty;
        if (!asset->cls && asset->inventoryNativeClass == 0u) return Status::MissingIconMetadata;
        if (!classDerives(asset->cls, asset->inventoryNativeClass, nullptr, 3u) ||
            !classDerives(asset->cls, asset->inventoryNativeClass, constraint->indexedClass, constraint->nativeClass)) return Status::MalformedProperty;
        return Status::Available;
    };
    for (const auto& requested : actorPaths) {
        PortableInventoryDescriptor item;
        if (!validText(requested)) { item.status = Status::InvalidIdentity; results.push_back(std::move(item)); continue; }
        copy(item.requestedPath, requested);
        if (!persistentRuntime || !persistentRuntime->get()) { results.push_back(std::move(item)); continue; }
        const auto found = persistentVmObjects.find(folded(requested));
        if (found == persistentVmObjects.end() || !found->second) {
            item.status = Status::UnindexedIdentity; results.push_back(std::move(item)); continue;
        }
        auto* actor = found->second;
        item.active = actor->active;
        std::array<RuntimeObject*, 128u> ancestry{};
        std::size_t depth{}; bool inventory{}, engineActor{}, malformed{};
        for (auto* cls = actor->cls; cls; cls = cls->base) {
            if (depth == limits.hierarchy || std::find(ancestry.begin(), ancestry.begin() + depth, cls) != ancestry.begin() + depth ||
                cls->reflection.metaClass != "Class" || !cls->inventoryTableMetadata || !cls->inventorySerializedClass ||
                !cls->inventoryBaseReferenceValid || !validText(cls->reflection.objectPath)) { malformed = true; break; }
            ancestry[depth++] = cls;
            inventory = inventory || same(cls->reflection.objectPath, "Engine.Inventory");
            engineActor = engineActor || same(cls->reflection.objectPath, "Engine.Actor");
        }
        if (malformed) { item.status = Status::MalformedProperty; results.push_back(std::move(item)); continue; }
        if (!inventory || !engineActor || actor->inventorySerializedClass || !actor->cls) {
            item.status = Status::NotInventoryActor; results.push_back(std::move(item)); continue;
        }
        if (!actor->inventoryTableMetadata || !actor->inventoryClassReferenceValid) {
            item.status = Status::MalformedProperty; results.push_back(std::move(item)); continue;
        }
        if (!copy(item.actorPath, actor->reflection.objectPath) || !copy(item.classPath, actor->cls->reflection.objectPath) ||
            !copy(item.actorSourcePath, actor->sourcePath) || !copy(item.classSourcePath, actor->cls->sourcePath)) {
            item.status = Status::MalformedProperty; results.push_back(std::move(item)); continue;
        }
        item.status = Status::Available;
        const auto readText = [&](const char* name, std::optional<std::string>& destination, TextStatus& textStatus) {
            RuntimeObject* declaration{};
            for (std::size_t index = 0u; index < depth; ++index) {
                const auto field = persistentVmObjects.find(folded(ancestry[index]->reflection.objectPath + '.' + name));
                if (field != persistentVmObjects.end() && field->second && field->second->property &&
                    field->second->outer == ancestry[index]) { declaration = field->second; break; }
            }
            if (!declaration) { textStatus = TextStatus::MissingPropertyMetadata; return; }
            const auto& metadata = *declaration->property;
            if (metadata.type != "StrProperty" || metadata.arrayDimension != 1) {
                textStatus = TextStatus::MalformedProperty; return;
            }
            const auto* overlay = FindScriptOverlay(actor, name);
            std::string decoded;
            const std::string* source = &decoded;
            if (overlay) {
                if (overlay->kind != Kind::String || !validText(overlay->text, true)) {
                    textStatus = TextStatus::MalformedProperty; return;
                }
                // Check transport without coercing Name/object text or losing
                // original FString units. The aggregate scratch is precharged.
                try { static_cast<void>(QuestVr::UnrealString::Decode(overlay->text)); }
                catch (const std::exception&) { textStatus = TextStatus::MalformedProperty; return; }
                source = &overlay->text;
            } else {
                const PortableTaggedProperty* tag{};
                for (std::size_t index = 0u; index <= depth && !tag; ++index) {
                    auto* owner = index == 0u ? actor : ancestry[index - 1u];
                    if (index != 0u) {
                        // A tag belongs to the nearest declaration in its
                        // source class, not to a descendant's same-name field.
                        // Do not leak a parent's description into a newly
                        // declared string which has only its typed-zero value.
                        RuntimeObject* selected{};
                        for (std::size_t base = index - 1u; base < depth; ++base) {
                            const auto field = persistentVmObjects.find(folded(ancestry[base]->reflection.objectPath + '.' + name));
                            if (field != persistentVmObjects.end() && field->second && field->second->property &&
                                field->second->outer == ancestry[base]) { selected = field->second; break; }
                        }
                        if (selected != declaration) continue;
                    }
                    const auto& properties = index == 0u ? owner->instanceProperties :
                        owner->classDescriptor ? owner->classDescriptor->defaults : owner->instanceProperties;
                    for (auto entry = properties.rbegin(); entry != properties.rend(); ++entry) {
                        if (!same(entry->name.ToString(), name)) continue;
                        if (entry->arrayIndex != 0u) { textStatus = TextStatus::MalformedProperty; return; }
                        if (!tag) tag = &*entry;
                    }
                }
                if (tag) {
                    if (tag->type != 13u) { textStatus = TextStatus::MalformedProperty; return; }
                    // The production decoder validates exact payload length,
                    // terminator and transport budget before allocating units.
                    try { decoded = DecodePortableVmStringProperty(*tag, limits.stringBytes); }
                    catch (const std::exception&) { textStatus = TextStatus::MalformedProperty; return; }
                }
                // No tag with a real declaration is the typed empty FString,
                // not missing metadata and not a guessed item/class label.
            }
            std::string detached;
            // Keep request-wide allocation failures outside the malformed-text
            // catch: aggregate exhaustion still rejects the complete request.
            if (!copy(detached, *source, true)) { textStatus = TextStatus::MalformedProperty; return; }
            destination.emplace(std::move(detached)); textStatus = TextStatus::Available;
        };
        readText("ItemName", item.itemName, item.itemNameStatus);
        readText("Description", item.description, item.descriptionStatus);
        const auto read = [&](const char* name, const Kind kind, std::int32_t* integer, bool* boolean,
            std::string* objectPath, PortableInventoryIconProvenance* provenance) {
            RuntimeObject* declaration{};
            for (std::size_t index = 0u; index < depth; ++index) {
                const auto key = folded(ancestry[index]->reflection.objectPath + '.' + name);
                const auto field = persistentVmObjects.find(key);
                if (field != persistentVmObjects.end() && field->second && field->second->property &&
                    field->second->outer == ancestry[index]) {
                    declaration = field->second; break;
                }
            }
            if (!declaration) { item.status = Status::MissingPropertyMetadata; return false; }
            const auto& metadata = *declaration->property;
            const char* type = kind == Kind::Int ? "IntProperty" : kind == Kind::Bool ? "BoolProperty" : "ObjectProperty";
            if (metadata.type != type || metadata.arrayDimension != 1) { item.status = Status::MalformedProperty; return false; }
            if (provenance && (!copy(provenance->declarationPath, declaration->reflection.objectPath) ||
                !copy(provenance->declarationSourcePath, declaration->sourcePath))) { item.status = Status::MalformedProperty; return false; }
            RuntimeObject* owner{}; const PortableTaggedProperty* tag{};
            const auto* overlay = FindScriptOverlay(actor, name);
            if (overlay) {
                if (overlay->kind != kind) { item.status = Status::MalformedProperty; return false; }
                if (integer) *integer = overlay->integer;
                if (boolean) *boolean = overlay->boolean;
                if (objectPath && !copy(*objectPath, overlay->text, true)) { item.status = Status::MalformedProperty; return false; }
                owner = actor; if (provenance) provenance->origin = Origin::Overlay;
            } else {
                for (std::size_t index = 0u; index <= depth && !tag; ++index) {
                    auto* source = index == 0u ? actor : ancestry[index - 1u];
                    const auto& properties = index == 0u ? source->instanceProperties :
                        source->classDescriptor ? source->classDescriptor->defaults : source->instanceProperties;
                    for (auto entry = properties.rbegin(); entry != properties.rend(); ++entry) {
                        if (!same(entry->name.ToString(), name)) continue;
                        if (entry->arrayIndex != 0u) { item.status = Status::MalformedProperty; return false; }
                        if (!tag) { tag = &*entry; owner = source; }
                    }
                }
                if (tag) {
                    if (integer) {
                        if (tag->type != 2u || tag->value.size() != 4u) { item.status = Status::MalformedProperty; return false; }
                        std::memcpy(integer, tag->value.data(), 4u);
                    } else if (boolean) {
                        if (tag->type != 3u || !tag->value.empty()) { item.status = Status::MalformedProperty; return false; }
                        *boolean = tag->boolValue;
                    } else {
                        if (tag->type != 5u) { item.status = Status::MalformedProperty; return false; }
                        std::int32_t reference{};
                        try { reference = DecodePortableObjectReference(*tag); }
                        catch (const std::exception&) { item.status = Status::MalformedProperty; return false; }
                        if (reference != 0) {
                            const auto cached = owner->objectPropertyPaths.find(tag->name.ToString());
                            if (cached == owner->objectPropertyPaths.end() || !copy(*objectPath, cached->second)) {
                                item.status = Status::MalformedProperty; return false;
                            }
                        }
                    }
                    if (provenance) provenance->origin = owner == actor ? Origin::Instance : Origin::ClassDefault;
                }
            }
            if (objectPath) {
                const auto status = iconClassValid(*objectPath, declaration->inventoryIconClassConstraint.get());
                if (status != Status::Available) { item.status = status; return false; }
            }
            if (provenance && owner && (!copy(provenance->ownerPath, owner->reflection.objectPath) ||
                !copy(provenance->ownerSourcePath, owner->sourcePath))) { item.status = Status::MalformedProperty; return false; }
            return true;
        };
        const auto ints = [&](const char* name, std::int32_t& value) { return read(name, Kind::Int, &value, nullptr, nullptr, nullptr); };
        if (!(ints("invSlotsX", item.invSlotsX) && ints("invSlotsY", item.invSlotsY) && ints("invPosX", item.invPosX) &&
            ints("invPosY", item.invPosY) && read("bDisplayableInv", Kind::Bool, nullptr, &item.bDisplayableInv, nullptr, nullptr) &&
            read("largeIcon", Kind::Object, nullptr, nullptr, &item.largeIconPath, &item.largeIconProvenance) &&
            ints("largeIconWidth", item.largeIconWidth) && ints("largeIconHeight", item.largeIconHeight) &&
            read("Icon", Kind::Object, nullptr, nullptr, &item.fallbackIconPath, &item.fallbackIconProvenance))) {
            results.push_back(std::move(item)); continue;
        }
        if (item.bDisplayableInv) {
            const bool slots = item.invSlotsX > 0 && item.invSlotsX <= 5 && item.invSlotsY > 0 && item.invSlotsY <= 6;
            const bool unassigned = item.invPosX == -1 && item.invPosY == -1;
            item.positionAssigned = !unassigned && slots && item.invPosX >= 0 && item.invPosY >= 0 &&
                item.invPosX <= 5 - item.invSlotsX && item.invPosY <= 6 - item.invSlotsY;
            if (!slots || (!unassigned && !item.positionAssigned)) item.status = Status::InvalidLayout;
        }
        if (!item.largeIconPath.empty()) {
            item.usesLargeIcon = true; copy(item.iconPath, item.largeIconPath);
            item.displayWidth = item.largeIconWidth; item.displayHeight = item.largeIconHeight;
            if (item.displayWidth <= 0 || item.displayWidth > 4096 || item.displayHeight <= 0 || item.displayHeight > 4096)
                item.status = Status::InvalidIconDimensions;
        } else if (!item.fallbackIconPath.empty()) {
            copy(item.iconPath, item.fallbackIconPath); item.displayWidth = 40; item.displayHeight = 35;
        }
        results.push_back(std::move(item));
    }
    return results;
}

PortableHeldItemVisualDescriptor ReadPortableRuntimeHeldItemVisualDescriptor(
    const std::string& collectedActorPath, const PortableHeldItemDescriptorLimits& limits) {
    using Status = PortableHeldItemDescriptorStatus;
    using FieldStatus = PortableHeldItemValueStatus;
    using Origin = PortableHeldItemValueOrigin;
    using Value = QuestVr::Vm::Value;
    using Kind = QuestVr::Vm::Kind;
    if (limits.retainedBytes == 0u || limits.retainedBytes > 1024u*1024u ||
        limits.stringBytes == 0u || limits.stringBytes > 8192u ||
        limits.hierarchy == 0u || limits.hierarchy > 128u ||
        limits.inventoryEntries == 0u || limits.inventoryEntries > 100'000u)
        throw std::runtime_error("Held-item descriptor limits are invalid");
    std::size_t retained{};
    const auto charge = [&](const std::size_t bytes) {
        if (bytes > limits.retainedBytes || retained > limits.retainedBytes-bytes)
            throw std::runtime_error("Held-item descriptor retained/scratch byte budget exceeded");
        retained += bytes;
    };
    charge(sizeof(PortableHeldItemVisualDescriptor)+256u*sizeof(RuntimeObject*)+6u*(limits.stringBytes+64u));
    const auto validText = [&](const std::string& text, const bool empty = false) {
        return (empty || !text.empty()) && text.size() <= limits.stringBytes && text.find('\0') == std::string::npos;
    };
    const auto copy = [&](std::string& target, const std::string& source, const bool empty = false) {
        if (!validText(source,empty)) return false;
        const auto allowance = 2u*(source.size()+1u)+32u;
        charge(allowance); target = source;
        if (target.capacity()+1u > allowance)
            throw std::runtime_error("Held-item descriptor string capacity exceeds charged bound");
        return true;
    };
    const auto folded = [](const std::string_view text) { return QuestVr::ObjectCastDetail::Fold(text); };
    PortableHeldItemVisualDescriptor result;
    if (!validText(collectedActorPath)) { result.status = Status::InvalidIdentity; return result; }
    copy(result.requestedPath,collectedActorPath);
    if (!persistentRuntime || !persistentRuntime->get()) return result;
    const auto found = persistentVmObjects.find(folded(collectedActorPath));
    if (found == persistentVmObjects.end() || !found->second) { result.status = Status::UnindexedIdentity; return result; }
    auto* actor = found->second;
    if (actor->inventorySerializedClass || !actor->cls) { result.status = Status::NotInventoryActor; return result; }
    std::array<RuntimeObject*,128u> ancestry{};
    std::size_t depth{}; bool inventory{}, engineActor{};
    for (auto* cls = actor->cls; cls; cls = cls->base) {
        if (depth == limits.hierarchy || std::find(ancestry.begin(),ancestry.begin()+depth,cls) != ancestry.begin()+depth ||
            cls->reflection.metaClass != "Class" || !cls->inventoryTableMetadata || !cls->inventorySerializedClass ||
            !cls->inventoryBaseReferenceValid || (cls->classDescriptor && !cls->classFieldLinksValid) ||
            !validText(cls->reflection.objectPath)) {
            result.status = Status::MalformedIdentity; return result;
        }
        ancestry[depth++] = cls;
        inventory = inventory || SameInventoryIdentity(cls->reflection.objectPath,"Engine.Inventory");
        engineActor = engineActor || SameInventoryIdentity(cls->reflection.objectPath,"Engine.Actor");
        result.weapon = result.weapon || SameInventoryIdentity(cls->reflection.objectPath,"Engine.Weapon");
    }
    if (!inventory || !engineActor) { result.status = Status::NotInventoryActor; return result; }
    if (!actor->inventoryTableMetadata || !actor->inventoryClassReferenceValid ||
        !copy(result.actorPath,actor->reflection.objectPath) || !copy(result.classPath,actor->cls->reflection.objectPath) ||
        !copy(result.actorSourcePath,actor->sourcePath) || !copy(result.classSourcePath,actor->cls->sourcePath)) {
        result.status = Status::MalformedIdentity; return result;
    }
    if (persistentInventory.size() > limits.inventoryEntries)
        throw std::runtime_error("Held-item descriptor collected identity scan budget exceeded");
    result.collected = std::any_of(persistentInventory.begin(),persistentInventory.end(),[&](const auto& identity) {
        return SameInventoryIdentity(identity,actor->reflection.objectPath);
    });
    if (!result.collected) { result.status = Status::NotCollected; return result; }
    result.active = actor->active;
    result.status = Status::Available;
    const auto classDerives = [&](RuntimeObject* cls, std::uint8_t native, RuntimeObject* target, const std::uint8_t targetNative) {
        std::array<RuntimeObject*,128u> seen{};
        std::size_t total{}, indexed{}; std::uint16_t nativeSeen{};
        const std::string_view wanted = target ? std::string_view(target->reflection.objectPath) : InventoryNativeClassPath(targetNative);
        if (wanted.empty() || wanted.size() > limits.stringBytes) return false;
        while (cls || native != 0u) {
            if (total++ == limits.hierarchy) return false;
            if (!cls) {
                if (native > 13u) return false;
                const auto bit = static_cast<std::uint16_t>(1u<<native);
                if ((nativeSeen&bit) != 0u) return false;
                nativeSeen |= bit;
                const auto path = InventoryNativeClassPath(native);
                const auto resolved = persistentVmObjects.find(folded(path));
                if (resolved != persistentVmObjects.end()) cls = resolved->second;
                else {
                    if (SameInventoryIdentity(path,wanted)) return true;
                    native = InventoryNativeClassBase(native); continue;
                }
            }
            if (!cls || !cls->inventoryTableMetadata || !cls->inventorySerializedClass || !cls->inventoryBaseReferenceValid ||
                cls->reflection.metaClass != "Class" || !validText(cls->reflection.objectPath) ||
                std::find(seen.begin(),seen.begin()+indexed,cls) != seen.begin()+indexed) return false;
            seen[indexed++] = cls;
            if (SameInventoryIdentity(cls->reflection.objectPath,wanted)) return true;
            native = cls->base ? 0u : cls->inventoryNativeBase;
            if (!cls->base && native == 0u && !SameInventoryIdentity(cls->reflection.objectPath,"Core.Object")) native = 1u;
            cls = cls->base;
        }
        return false;
    };
    const auto declarationAt = [&](const std::size_t first, const char* name) -> RuntimeObject* {
        for (std::size_t index = first; index < depth; ++index) {
            const auto field = persistentVmObjects.find(folded(ancestry[index]->reflection.objectPath+'.'+name));
            if (ancestry[index]->classFieldLinksValid && field != persistentVmObjects.end() && field->second &&
                field->second->property && field->second->outer == ancestry[index] && field->second->linkedClassOwner == ancestry[index])
                return field->second;
        }
        return nullptr;
    };
    const auto read = [&](const char* name, const std::uint32_t slot, const std::int32_t dimension,
        const Kind kind, const std::uint8_t expectedNative, PortableHeldItemValueStatus& status,
        PortableHeldItemValueProvenance& provenance) -> std::optional<Value> {
        auto* declaration = declarationAt(0u,name);
        if (!declaration) { status = FieldStatus::MissingPropertyMetadata; return {}; }
        const auto& metadata = *declaration->property;
        const char* type = kind == Kind::Int ? "IntProperty" : kind == Kind::Float ? "FloatProperty" :
            kind == Kind::Bool ? "BoolProperty" : "ObjectProperty";
        if (metadata.type != type || metadata.arrayDimension != dimension || slot >= static_cast<std::uint32_t>(dimension)) {
            status = FieldStatus::MalformedProperty; return {};
        }
        if (!copy(provenance.declarationPath,declaration->reflection.objectPath) ||
            !copy(provenance.declarationSourcePath,declaration->sourcePath)) { status = FieldStatus::MalformedProperty; return {}; }
        const auto* constraint = declaration->inventoryIconClassConstraint.get();
        if (kind == Kind::Object && (!constraint || !constraint->valid ||
            !classDerives(constraint->indexedClass,constraint->nativeClass,nullptr,expectedNative))) {
            status = FieldStatus::MalformedProperty; return {};
        }
        Value value = kind == Kind::Int ? Value::Integer(0) : kind == Kind::Float ? Value::Float(0) :
            kind == Kind::Bool ? Value::Bool(false) : Value::Text(Kind::Object,{});
        RuntimeObject* owner{};
        const Value* overlay{};
        const auto key = folded(name);
        for (const auto* values : {&actor->scriptValues,&actor->frozenBirthDefaults}) {
            const auto property = values->find(key);
            if (property == values->end()) continue;
            if (std::any_of(property->second.begin(),property->second.end(),[&](const auto& entry) {
                return entry.first >= static_cast<std::uint32_t>(dimension);
            })) { status = FieldStatus::MalformedProperty; return {}; }
            const auto entry = property->second.find(slot);
            if (entry == property->second.end()) continue;
            overlay = &entry->second; owner = actor;
            provenance.origin = values == &actor->scriptValues ? Origin::Overlay : Origin::FrozenClassDefault;
            break;
        }
        if (overlay) {
            if (overlay->kind != kind) { status = FieldStatus::MalformedProperty; return {}; }
            if (kind == Kind::Object) {
                if (!copy(value.text,overlay->text,true)) { status = FieldStatus::InvalidValue; return {}; }
            } else if (kind == Kind::Float) value.floating = overlay->floating;
            else if (kind == Kind::Int) value.integer = overlay->integer;
            else value.boolean = overlay->boolean;
        } else {
            const PortableTaggedProperty* tag{};
            for (std::size_t index = 0u; index <= depth && !tag; ++index) {
                if (index != 0u && declarationAt(index-1u,name) != declaration) continue;
                auto* source = index == 0u ? actor : ancestry[index-1u];
                const auto& properties = index == 0u ? source->instanceProperties :
                    source->classDescriptor ? source->classDescriptor->defaults : source->instanceProperties;
                for (auto entry = properties.rbegin(); entry != properties.rend(); ++entry) {
                    if (!SameInventoryIdentity(entry->name.ToString(),name)) continue;
                    if (entry->arrayIndex >= static_cast<std::uint32_t>(dimension)) { status = FieldStatus::MalformedProperty; return {}; }
                    if (!tag && entry->arrayIndex == slot) { tag = &*entry; owner = source; }
                }
            }
            if (tag) {
                provenance.origin = owner == actor ? Origin::Instance : Origin::ClassDefault;
                if (kind == Kind::Int || kind == Kind::Float) {
                    if (tag->type != (kind == Kind::Int ? 2u : 4u) || tag->value.size() != 4u) {
                        status = FieldStatus::MalformedProperty; return {};
                    }
                    if (kind == Kind::Int) std::memcpy(&value.integer,tag->value.data(),4u);
                    else std::memcpy(&value.floating,tag->value.data(),4u);
                } else if (kind == Kind::Bool) {
                    if (tag->type != 3u || !tag->value.empty()) { status = FieldStatus::MalformedProperty; return {}; }
                    value.boolean = tag->boolValue;
                } else {
                    if (tag->type != 5u) { status = FieldStatus::MalformedProperty; return {}; }
                    std::int32_t reference{};
                    try { reference = DecodePortableObjectReference(*tag); }
                    catch (const std::exception&) { status = FieldStatus::MalformedProperty; return {}; }
                    if (reference != 0) {
                        // Scalar cached objectPropertyPaths cannot represent
                        // MultiSkins independently. Its load-time typed texture
                        // override cache retains each actual source array slot.
                        if (SameInventoryIdentity(name,"Skin") || SameInventoryIdentity(name,"MultiSkins")) {
                            const auto& texture = SameInventoryIdentity(name,"Skin") ? owner->textureOverrides.skin : owner->textureOverrides.multiSkins[slot];
                            if (!texture.specified || !copy(value.text,texture.path)) { status = FieldStatus::MalformedProperty; return {}; }
                        } else {
                            const auto path = owner->objectPropertyPaths.find(tag->name.ToString());
                            if (path == owner->objectPropertyPaths.end() || !copy(value.text,path->second)) {
                                status = FieldStatus::MalformedProperty; return {};
                            }
                        }
                    }
                }
            }
        }
        if (kind == Kind::Float && !std::isfinite(value.floating)) { status = FieldStatus::InvalidValue; return {}; }
        if (kind == Kind::Object && !value.text.empty()) {
            const auto asset = persistentVmObjects.find(folded(value.text));
            if (asset == persistentVmObjects.end() || !asset->second) { status = FieldStatus::MissingReferencedObject; return {}; }
            const auto* object = asset->second;
            if (!object->inventoryTableMetadata || object->inventorySerializedClass || !object->inventoryClassReferenceValid ||
                !classDerives(object->cls,object->inventoryNativeClass,nullptr,expectedNative) ||
                !classDerives(object->cls,object->inventoryNativeClass,constraint->indexedClass,constraint->nativeClass)) {
                status = FieldStatus::MalformedProperty; return {};
            }
        }
        if (owner && (!copy(provenance.ownerPath,owner->reflection.objectPath) || !copy(provenance.ownerSourcePath,owner->sourcePath))) {
            status = FieldStatus::MalformedProperty; return {};
        }
        status = FieldStatus::Available; return value;
    };
    const auto object = [&](const char* name, PortableHeldItemValue<std::string>& field,
        const std::uint8_t native, const std::uint32_t slot = 0u, const std::int32_t dimension = 1) {
        if (auto value = read(name,slot,dimension,Kind::Object,native,field.status,field.provenance)) field.value.emplace(std::move(value->text));
    };
    const auto real = [&](const char* name, PortableHeldItemValue<float>& field) {
        if (const auto value = read(name,0u,1,Kind::Float,0u,field.status,field.provenance)) field.value = value->floating;
    };
    const auto integer = [&](const char* name, PortableHeldItemValue<std::int32_t>& field) {
        if (const auto value = read(name,0u,1,Kind::Int,0u,field.status,field.provenance)) field.value = value->integer;
    };
    object("PlayerViewMesh",result.playerViewMesh,11u);
    object("PickupViewMesh",result.pickupViewMesh,11u);
    object("ThirdPersonMesh",result.thirdPersonMesh,11u);
    real("PlayerViewScale",result.playerViewScale);
    real("PickupViewScale",result.pickupViewScale);
    real("ThirdPersonScale",result.thirdPersonScale);
    object("Skin",result.skin,3u);
    for (std::uint32_t slot = 0u; slot < result.multiSkins.size(); ++slot) object("MultiSkins",result.multiSkins[slot],3u,slot,8);
    integer("HitDamage",result.hitDamage);
    integer("AccurateRange",result.accurateRange);
    integer("MaxRange",result.maxRange);
    if (const auto value = read("bHandToHand",0u,1,Kind::Bool,0u,result.handToHand.status,result.handToHand.provenance))
        result.handToHand.value = value->boolean;
    return result;
}

PortablePlayerProgress GetPortableRuntimePlayerProgress() {
    PortablePlayerProgress result;
    result.credits = persistentCredits;
    result.skillPoints = persistentSkillPoints;
    result.goals = persistentGoals;
    result.notes = persistentNotes;
    return result;
}

bool ConsumePortableRuntimeInventoryItem(const std::string& objectPath) {
    const auto found = std::find(
        persistentInventory.begin(), persistentInventory.end(), objectPath);
    if (found == persistentInventory.end()) return false;
    persistentInventory.erase(found);
    return true;
}

float GetPortableRuntimePlayerHealth() {
    return persistentPlayerHealth;
}

float DamagePortableRuntimePlayer(float damage) {
    if (std::isfinite(damage) && damage > 0.0f) {
        persistentPlayerHealth = std::max(0.0f, persistentPlayerHealth - damage);
    }
    return persistentPlayerHealth;
}

float HealPortableRuntimePlayer(float amount) {
    if (std::isfinite(amount) && amount > 0.0f) {
        persistentPlayerHealth = std::min(100.0f, persistentPlayerHealth + amount);
    }
    return persistentPlayerHealth;
}

bool VerifyPortableRuntimeInteraction() {
    if (!persistentRuntime || !persistentRuntime->get()) return false;
    RuntimeObject* inventoryActor{};
    for (std::size_t index = persistentScriptExportCount;
         index < persistentRuntime->get()->exports.size(); ++index) {
        RuntimeObject* object = persistentRuntime->get()->exports[index];
        if (object->active && !object->worldRemoved && IsDerivedFromPath(object->cls, "Engine.Inventory")) {
            inventoryActor = object;
            break;
        }
    }
    if (inventoryActor == nullptr) return false;
    const std::size_t actorsBefore = GetPortableRuntimeMapActors().size();
    const std::size_t inventoryBefore = persistentInventory.size();
    const PortableInteractionResult result =
        InteractPortableRuntimeActor(inventoryActor->reflection.objectPath);
    const bool passed = result.handled && result.worldChanged && result.action == "pickup" &&
        persistentInventory.size() == inventoryBefore + 1u &&
        GetPortableRuntimeMapActors().size() + 1u == actorsBefore;
    inventoryActor->active = true;
    if (persistentInventory.size() > inventoryBefore) persistentInventory.pop_back();
    return passed && GetPortableRuntimeMapActors().size() == actorsBefore &&
        persistentInventory.size() == inventoryBefore;
}

bool SavePortableRuntimeState(const std::string& path) {
    if (!persistentRuntime || !persistentRuntime->get()) return false;
    try {
        // Collect and schema-check all script state before opening the output.
        // A rejected/over-budget capture must not truncate an existing save.
        const auto scriptState = CollectScriptSavedState();
        const bool hasDefaults = !scriptState.classDefaults.empty();
        const bool hasBirths = !scriptState.births.empty();
        const bool hasAiManagers = !scriptState.aiManagers.empty();
        const bool hasRandom = scriptState.randomSeed.has_value();
        const bool hasSleep = std::any_of(scriptState.objects.begin(), scriptState.objects.end(),
            [](const auto& object) { return object.latentTimeLeft.has_value(); });
        const bool hasControl = std::any_of(scriptState.objects.begin(), scriptState.objects.end(),
            [](const auto& object) { return object.state && object.state->frame && object.state->frame->control; });
        const bool hasDecals = scriptState.nativeDecals.has_value() || QuestVr::ScriptStateDetail::HasIntArrays(scriptState);
        const bool hasScript = !scriptState.objects.empty() || hasDefaults || hasBirths || hasAiManagers || hasRandom || hasDecals;
        const bool hasState = std::any_of(scriptState.objects.begin(), scriptState.objects.end(),
            [](const auto& object) { return object.state.has_value(); });
        const bool hasLifecycle = std::any_of(scriptState.objects.begin(), scriptState.objects.end(),
            [](const auto& object) { return object.lifecycle.has_value(); });
        if (hasScript) {
            QuestVr::ScriptStateLimits limits;
            limits.maxBytes = QuestVr::kMaximumSaveRuntimeBytes;
            static_cast<void>(PrepareScriptSavedState(scriptState, false, limits));
        }
        std::vector<std::string> inactive;
        std::vector<std::string> activated;
        std::vector<std::pair<std::string, float>> damaged;
        std::vector<RuntimeObject*> movers;
        for (std::size_t index = persistentScriptExportCount;
             index < persistentRuntime->get()->exports.size(); ++index) {
            RuntimeObject* object = persistentRuntime->get()->exports[index];
            if (!object->active) inactive.push_back(object->reflection.objectPath);
            if (object->activated) activated.push_back(object->reflection.objectPath);
            if (object->healthInitialized) damaged.emplace_back(object->reflection.objectPath, object->health);
            if (object->moverMotion) {
                QuestVr::ValidateMoverMotionState(object->moverMotion->spec, object->moverMotion->state);
                if (object->runtimeBirth || object->worldRemoved || !IsDerivedFromPath(object->cls, "Engine.Mover"))
                    throw std::runtime_error("Mover checkpoint owner is not an authored live map Mover");
                movers.push_back(object);
            }
        }
        std::vector<std::uint8_t> bytes;
        const auto requireBytes = [&](const std::size_t count) {
            if (count > QuestVr::kMaximumSaveRuntimeBytes || bytes.size() > QuestVr::kMaximumSaveRuntimeBytes - count)
                throw std::runtime_error("Runtime checkpoint exceeds whole-file byte budget");
        };
        const auto write32 = [&](const std::uint32_t value) {
            requireBytes(4u);
            for (unsigned i = 0; i < 4u; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8u)));
        };
        const auto writeString = [&](const std::string& value) {
            if (value.size() > 1'048'576u) throw std::runtime_error("Runtime checkpoint string exceeds byte budget");
            write32(static_cast<std::uint32_t>(value.size())); requireBytes(value.size());
            bytes.insert(bytes.end(), value.begin(), value.end());
        };
        const auto writeStrings = [&](const std::vector<std::string>& strings) {
            if (strings.size() > 100'000u) throw std::runtime_error("Runtime checkpoint list exceeds count budget");
            write32(static_cast<std::uint32_t>(strings.size()));
            for (const auto& value : strings) writeString(value);
        };
        const auto writeFloat = [&](const float value) {
            if (!std::isfinite(value)) throw std::runtime_error("Runtime checkpoint contains a non-finite health value");
            std::uint32_t bits{}; std::memcpy(&bits, &value, sizeof(bits)); write32(bits);
        };
        std::vector<std::string> flags;
        flags.reserve(persistentConversationFlags.size());
        for (const auto& entry : persistentConversationFlags)
            flags.push_back(entry.first + (entry.second ? "\n1" : "\n0"));
        const std::vector<std::string> applied(persistentAppliedDialogueEffects.begin(), persistentAppliedDialogueEffects.end());
        if (persistentPlayerHealth < 0.0f || persistentPlayerHealth > 100.0f ||
            persistentCredits < 0 || persistentSkillPoints < 0 || damaged.size() > 100'000u)
            throw std::runtime_error("Runtime checkpoint gameplay values are outside their valid ranges");
        // This is exactly the v3 prefix, including list order and field widths.
        // Only the version word and appended trailer differ for v4-v13. Pure
        // property/clock captures retain the byte-exact original v4 format.
        const auto scriptVersion = hasDecals ? 13u : hasControl ? 12u : hasSleep ? 11u : hasRandom ? 10u : hasAiManagers ? 9u : hasBirths ? 8u : hasLifecycle ? 7u : hasDefaults ? 6u : hasState ? 5u : hasScript ? 4u : 3u;
        write32(0x53515844u); write32(movers.empty() ? scriptVersion : 14u);
        if (!movers.empty()) write32(scriptVersion);
        writeStrings(persistentInventory); writeStrings(inactive); writeStrings(activated); writeFloat(persistentPlayerHealth);
        write32(static_cast<std::uint32_t>(damaged.size()));
        for (const auto& entry : damaged) {
            if (entry.second < 0.0f) throw std::runtime_error("Runtime checkpoint actor health is negative");
            writeString(entry.first); writeFloat(entry.second);
        }
        write32(static_cast<std::uint32_t>(persistentCredits)); write32(static_cast<std::uint32_t>(persistentSkillPoints));
        writeStrings(flags); writeStrings(persistentGoals); writeStrings(persistentNotes); writeStrings(applied);
        if (!movers.empty()) {
            if (movers.size() > 10'000u) throw std::runtime_error("Mover checkpoint count exceeds budget");
            write32(static_cast<std::uint32_t>(movers.size()));
            const auto writePose = [&](const QuestVr::MoverPose& pose) {
                writeFloat(pose.location.x); writeFloat(pose.location.y); writeFloat(pose.location.z);
                write32(static_cast<std::uint32_t>(pose.rotation.pitch));
                write32(static_cast<std::uint32_t>(pose.rotation.yaw));
                write32(static_cast<std::uint32_t>(pose.rotation.roll));
            };
            for (auto* object : movers) {
                writeString(object->reflection.objectPath);
                const auto& state = object->moverMotion->state;
                writePose(state.pose); writePose(state.sourcePose);
                write32(state.targetKey); write32(static_cast<std::uint32_t>(static_cast<std::int32_t>(state.returnKey)));
                write32(static_cast<std::uint32_t>(state.phase));
                writeFloat(state.elapsedSeconds); writeFloat(state.remainingStaySeconds);
            }
        }
        if (hasScript) {
            requireBytes(4u);
            QuestVr::ScriptStateLimits limits;
            limits.maxBytes = QuestVr::kMaximumSaveRuntimeBytes - bytes.size() - 4u;
            const auto blob = QuestVr::EncodeScriptSavedState(scriptState, limits);
            write32(static_cast<std::uint32_t>(blob.size())); requireBytes(blob.size());
            bytes.insert(bytes.end(), blob.begin(), blob.end());
        }
        if (!QuestVr::WriteDurableSaveFile(path, bytes)) {
            __android_log_print(ANDROID_LOG_WARN, "quest_main", "DeusExQuest: runtime checkpoint write failed");
            return false;
        }
        return true;
    } catch (const std::exception& error) {
        __android_log_print(ANDROID_LOG_WARN, "quest_main", "DeusExQuest: runtime checkpoint capture rejected: %.512s", error.what());
    } catch (...) {
        __android_log_print(ANDROID_LOG_WARN, "quest_main", "DeusExQuest: runtime checkpoint capture rejected: unknown failure");
    }
    return false;
}

static bool ReadPortableRuntimeState(const std::string& path, const bool apply, const std::string& expectedMapName) {
    if (!persistentRuntime || !persistentRuntime->get()) return false;
    try {
        std::vector<std::uint8_t> bytes;
        if (!QuestVr::ReadBoundedSaveFile(path, QuestVr::kMaximumSaveRuntimeBytes, bytes)) return false;
        std::size_t cursor{};
        const auto requireBytes = [&](const std::size_t count) {
            if (cursor > bytes.size() || count > bytes.size() - cursor)
                throw std::runtime_error("Runtime checkpoint payload is truncated");
        };
        const auto read32 = [&]() {
            requireBytes(4u); std::uint32_t value{};
            for (unsigned i = 0; i < 4u; ++i) value |= std::uint32_t(bytes[cursor++]) << (i * 8u);
            return value;
        };
        const auto readString = [&]() {
            const auto length = read32();
            if (length > 1'048'576u) throw std::runtime_error("Runtime checkpoint string exceeds byte budget");
            requireBytes(length);
            std::string value(reinterpret_cast<const char*>(bytes.data() + cursor), length); cursor += length;
            return value;
        };
        const auto readStrings = [&]() {
            const auto count = read32();
            if (count > 100'000u || count > (bytes.size() - cursor) / 4u)
                throw std::runtime_error("Runtime checkpoint list exceeds count budget or encoded payload");
            std::vector<std::string> values; values.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i) values.push_back(readString());
            return values;
        };
        const auto readFloat = [&]() {
            const auto bits = read32(); float value{}; std::memcpy(&value, &bits, sizeof(value));
            if (!std::isfinite(value)) throw std::runtime_error("Runtime checkpoint contains a non-finite health value");
            return value;
        };
        if (read32() != 0x53515844u) throw std::runtime_error("Runtime checkpoint magic is invalid");
        const auto formatVersion = read32();
        const auto version = formatVersion == 14u ? read32() : formatVersion;
        if (formatVersion < 1u || formatVersion > 14u || version < 1u || version > 13u ||
            (formatVersion == 14u && version < 3u)) throw std::runtime_error("Runtime checkpoint version is unsupported");
        auto inventory = readStrings(); auto inactive = readStrings(); auto activated = readStrings();
        float playerHealth = 100.0f;
        std::vector<std::pair<std::string, float>> damaged;
        if (version >= 2u) {
            playerHealth = readFloat();
            if (playerHealth < 0.0f || playerHealth > 100.0f)
                throw std::runtime_error("Runtime checkpoint player health is outside 0..100");
            const auto count = read32();
            if (count > 100'000u || count > (bytes.size() - cursor) / 8u)
                throw std::runtime_error("Runtime checkpoint damaged-actor count exceeds budget or encoded payload");
            damaged.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i) {
                auto actor = readString(); const auto health = readFloat();
                if (health < 0.0f) throw std::runtime_error("Runtime checkpoint actor health is negative");
                damaged.emplace_back(std::move(actor), health);
            }
        }
        std::int32_t credits{}, skillPoints{};
        std::vector<std::string> flags, goals, notes, applied;
        if (version >= 3u) {
            const auto savedCredits = read32(), savedSkills = read32();
            if (savedCredits > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
                savedSkills > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
                throw std::runtime_error("Runtime checkpoint credits or skill points are negative/out of range");
            credits = static_cast<std::int32_t>(savedCredits); skillPoints = static_cast<std::int32_t>(savedSkills);
            flags = readStrings(); goals = readStrings(); notes = readStrings(); applied = readStrings();
        }
        std::vector<std::pair<std::string, QuestVr::MoverMotionState>> savedMovers;
        std::string moverMapName;
        if (formatVersion == 14u) {
            const auto count = read32();
            if (!count || count > 10'000u || count > (bytes.size() - cursor) / 72u)
                throw std::runtime_error("Mover checkpoint count exceeds budget or payload");
            savedMovers.reserve(count);
            const auto readInt = [&]() { const auto bits = read32(); std::int32_t value; std::memcpy(&value, &bits, 4u); return value; };
            const auto readPose = [&]() {
                QuestVr::MoverPose pose;
                pose.location = {readFloat(), readFloat(), readFloat()};
                pose.rotation = {readInt(), readInt(), readInt()};
                return pose;
            };
            std::unordered_set<std::string> identities;
            for (std::uint32_t i = 0u; i < count; ++i) {
                auto name = readString();
                if (name.empty() || !identities.insert(LowerAscii(name)).second)
                    throw std::runtime_error("Mover checkpoint owner is empty or duplicated");
                const auto ownerMap = ScriptSaveSchema::MoverMapName(name);
                if (moverMapName.empty()) moverMapName = ownerMap;
                else if (LowerAscii(moverMapName) != LowerAscii(ownerMap))
                    throw std::runtime_error("Mover checkpoint contains actors from multiple maps");
                QuestVr::MoverMotionState state;
                state.pose = readPose(); state.sourcePose = readPose();
                const auto target = read32(); const auto returnKey = readInt(); const auto phase = read32();
                if (target > 7u || returnKey < -1 || returnKey > 7 || phase > 2u)
                    throw std::runtime_error("Mover checkpoint key or phase is invalid");
                state.targetKey = static_cast<std::uint8_t>(target); state.returnKey = static_cast<std::int8_t>(returnKey);
                state.phase = static_cast<QuestVr::MoverMotionPhase>(phase);
                state.elapsedSeconds = readFloat(); state.remainingStaySeconds = readFloat();
                savedMovers.emplace_back(std::move(name), state);
            }
            if (!expectedMapName.empty() && LowerAscii(expectedMapName) != LowerAscii(moverMapName))
                throw std::runtime_error("Mover checkpoint map does not match save metadata");
            if (apply && LowerAscii(moverMapName) != LowerAscii(persistentMapPackageName))
                throw std::runtime_error("Mover checkpoint map is not the loaded map");
        }
        PreparedScriptState prepared;
        std::optional<std::uint32_t> restoredRandomSeed;
        std::unordered_set<std::string> savedBirthKeys;
        if (version >= 4u) {
            const auto prefixBytes = cursor;
            const auto length = read32();
            if (length == 0u || length != bytes.size() - cursor)
                throw std::runtime_error("Runtime checkpoint script trailer length is invalid");
            QuestVr::ScriptStateLimits limits;
            limits.maxBytes = QuestVr::kMaximumSaveRuntimeBytes - prefixBytes - 4u;
            const std::vector<std::uint8_t> blob(bytes.begin() + static_cast<std::ptrdiff_t>(cursor), bytes.end());
            const auto scriptState = QuestVr::DecodeScriptSavedState(blob, limits);
            const bool hasState = std::any_of(scriptState.objects.begin(), scriptState.objects.end(),
                [](const auto& object) { return object.state.has_value(); });
            const bool hasDefaults = !scriptState.classDefaults.empty();
            const bool hasBirths = !scriptState.births.empty();
            const bool hasAiManagers = !scriptState.aiManagers.empty();
            const bool hasRandom = scriptState.randomSeed.has_value();
            const bool hasSleep = std::any_of(scriptState.objects.begin(), scriptState.objects.end(),
                [](const auto& object) { return object.latentTimeLeft.has_value(); });
            const bool hasControl = std::any_of(scriptState.objects.begin(), scriptState.objects.end(),
                [](const auto& object) { return object.state && object.state->frame && object.state->frame->control; });
            const bool hasDecals = scriptState.nativeDecals.has_value() || QuestVr::ScriptStateDetail::HasIntArrays(scriptState, limits);
            restoredRandomSeed = scriptState.randomSeed;
            for (const auto& birth : scriptState.births) savedBirthKeys.emplace(LowerAscii(birth.path));
            const bool hasLifecycle = std::any_of(scriptState.objects.begin(), scriptState.objects.end(),
                [](const auto& object) { return object.lifecycle.has_value(); });
            if ((version == 4u && (blob.at(6u) != QuestVr::ScriptStateDetail::Magic[6u] || hasState)) ||
                (version == 5u && (blob.at(6u) != QuestVr::ScriptStateDetail::StateFrameVersion || !hasState)) ||
                (version == 6u && (blob.at(6u) != QuestVr::ScriptStateDetail::ClassDefaultsVersion || !hasDefaults)) ||
                (version == 7u && (blob.at(6u) != QuestVr::ScriptStateDetail::ActorLifecycleVersion || !hasLifecycle)) ||
                (version == 8u && (blob.at(6u) != QuestVr::ScriptStateDetail::BirthManifestVersion || !hasBirths)) ||
                (version == 9u && (blob.at(6u) != QuestVr::ScriptStateDetail::AiManagerVersion || !hasAiManagers)) ||
                (version == 10u && (blob.at(6u) != QuestVr::ScriptStateDetail::RandomStateVersion || !hasRandom)) ||
                (version == 11u && (blob.at(6u) != QuestVr::ScriptStateDetail::SleepStateVersion || !hasSleep)) ||
                (version == 12u && (blob.at(6u) != QuestVr::ScriptStateDetail::StateControlVersion || !hasControl)) ||
                (version == 13u && (blob.at(6u) != QuestVr::ScriptStateDetail::DecalStateVersion || !hasDecals)))
                throw std::runtime_error("Runtime checkpoint state trailer does not match its envelope version");
            if (!expectedMapName.empty() && LowerAscii(expectedMapName) != LowerAscii(scriptState.mapName))
                throw std::runtime_error("Runtime checkpoint script map does not match save metadata");
            if (!moverMapName.empty() && LowerAscii(moverMapName) != LowerAscii(scriptState.mapName))
                throw std::runtime_error("Runtime checkpoint script and mover maps disagree");
            prepared = PrepareScriptSavedState(scriptState, apply, limits);
            cursor += length;
        }
        if (cursor != bytes.size()) throw std::runtime_error("Runtime checkpoint has trailing bytes");
        if (apply && !prepared.replaceBirths) StageRuntimeBirthReplacement(prepared);
        const auto& restoredObjects = apply ? prepared.vm : persistentVmObjects;
        const auto prefixTarget = [&](const std::string& path) -> RuntimeObject* {
            const auto old = persistentVmObjects.find(LowerAscii(path));
            if (old != persistentVmObjects.end() && old->second->runtimeBirth &&
                !savedBirthKeys.count(old->second->birthVmKey))
                throw std::runtime_error("Checkpoint gameplay prefix references an omitted live birth");
            const auto found = restoredObjects.find(LowerAscii(path));
            return found == restoredObjects.end() ? nullptr : found->second;
        };
        for (const auto& path : inventory) static_cast<void>(prefixTarget(path));
        // Prepare EVERY allocating container and resolve all mutation targets
        // before clearing live script/gameplay state. Legacy paths retain their
        // existing tolerant behavior for actors from previously visited maps.
        std::unordered_map<std::string, bool> restoredFlags;
        for (const auto& flag : flags) {
            const auto separator = flag.rfind('\n');
            if (separator == std::string::npos || separator == 0u || separator + 2u != flag.size() ||
                (flag.back() != '0' && flag.back() != '1') ||
                !restoredFlags.emplace(LowerAscii(flag.substr(0u, separator)), flag.back() == '1').second)
                throw std::runtime_error("Runtime checkpoint conversation flag is invalid or duplicated");
        }
        std::unordered_set<std::string> restoredApplied(applied.begin(), applied.end());
        std::vector<RuntimeObject*> inactiveTargets, activatedTargets;
        std::vector<std::pair<RuntimeObject*, float>> damagedTargets;
        inactiveTargets.reserve(inactive.size()); activatedTargets.reserve(activated.size()); damagedTargets.reserve(damaged.size());
        for (const auto& objectPath : inactive) {
            if (auto* object = prefixTarget(objectPath)) inactiveTargets.push_back(object);
        }
        for (const auto& objectPath : activated) {
            if (auto* object = prefixTarget(objectPath)) activatedTargets.push_back(object);
        }
        for (const auto& entry : damaged) {
            if (auto* object = prefixTarget(entry.first)) damagedTargets.emplace_back(object, entry.second);
        }
        std::vector<std::pair<RuntimeObject*, RuntimeMoverMotion>> restoredMovers;
        restoredMovers.reserve(savedMovers.size());
        std::unique_ptr<ScriptSaveSchema> moverSchema;
        if (!savedMovers.empty() && LowerAscii(moverMapName) != LowerAscii(persistentMapPackageName))
            moverSchema = std::make_unique<ScriptSaveSchema>(moverMapName);
        for (const auto& [path, state] : savedMovers) {
            if (moverSchema) {
                // Read-only preflight must work before a cross-map load worker
                // starts. This temporary receiver is neither GC-allocated nor
                // registered in any live map, tag, collision or VM index.
                struct DetachedMover final : RuntimeObject {
                    DetachedMover() : RuntimeObject({}, nullptr) {}
                    ~DetachedMover() override = default;
                } actor;
                moverSchema->ReadAuthoredMover(path, actor);
                const auto motion = ReadRuntimeMoverMotion(&actor, false, &moverSchema->Host());
                QuestVr::ValidateMoverMotionState(motion.spec, state);
                const bool isActivated = std::any_of(activated.begin(), activated.end(), [&](const auto& value) {
                    return LowerAscii(value) == LowerAscii(path);
                });
                if (isActivated != (state.targetKey != 0u))
                    throw std::runtime_error("Mover checkpoint activation and motion disagree");
                continue;
            }
            auto* actor = prefixTarget(path);
            if (!actor || actor->runtimeBirth || actor->worldActorIndex < 0 || actor->reflection.objectPath != path ||
                !IsDerivedFromPath(actor->cls, "Engine.Mover"))
                throw std::runtime_error("Mover checkpoint has an invalid map actor identity");
            auto motion = ReadRuntimeMoverMotion(actor);
            QuestVr::ValidateMoverMotionState(motion.spec, state);
            const bool activated = std::find(activatedTargets.begin(), activatedTargets.end(), actor) != activatedTargets.end();
            if (activated != (state.targetKey != 0u)) throw std::runtime_error("Mover checkpoint activation and motion disagree");
            motion.state = state;
            restoredMovers.emplace_back(actor, std::move(motion));
        }
        // Old saves contain a boolean open state only. Migrate it to an actual
        // authored endpoint so the brush no longer vanishes after quick-load.
        if (formatVersion < 14u) for (auto* actor : activatedTargets) {
            if (!IsDerivedFromPath(actor->cls, "Engine.Mover")) continue;
            try { restoredMovers.emplace_back(actor, ReadRuntimeMoverMotion(actor, true)); }
            catch (const std::exception&) { /* Unsupported movers stay visible at their authored pose. */ }
        }
        if (!apply) return true;
        // Allocate the legacy/native reset before touching live state. All
        // versions replace native topology; absence is not a merge with the
        // current session. Initial Base relinks are the map-load baseline.
        std::vector<std::pair<RuntimeObject*, std::vector<RuntimeObject*>>> initialBases;
        for (auto* object : persistentRuntime->get()->exports)
            if (!object->runtimeBirth && !object->initialBasedActors.empty()) initialBases.emplace_back(object, object->initialBasedActors);
        // Build the replacement tag index before publishing any registry or
        // field. Omitted overlays use immutable authored names, not live tags.
        PortableActorVmHost collisionHost;
        std::vector<PortableActorSnapshot> visualAssets;
        for (auto* actor : prepared.worldActors) {
            if (!actor) continue;
            const auto saved = std::find_if(prepared.objects.begin(),prepared.objects.end(),[&](const auto& object) {return object.target==actor;});
            if (saved != prepared.objects.end() && saved->removed) continue;
            const auto restoredValue=[&](const QuestVr::Vm::Property& property) {
                if (saved!=prepared.objects.end()) {
                    const auto values=saved->values.find(LowerAscii(property.name));
                    if (values!=saved->values.end() && values->second.count(0u)) return values->second.at(0u);
                }
                return collisionHost.Read(actor,property,0u,false,true);
            };
            collisionHost.RegisterCollisionActor(actor,prepared.collision,restoredValue);
            PortableActorSnapshot assets;
            if (collisionHost.HasPropertyNamed(actor,"Mesh"))
                assets.meshPath=restoredValue(collisionHost.PropertyNamed(actor,"Mesh")).text;
            assets.mover=IsDerivedFromPath(actor->cls,"Engine.Mover");
            if (assets.mover && collisionHost.HasPropertyNamed(actor,"Brush"))
                assets.brushPath=restoredValue(collisionHost.PropertyNamed(actor,"Brush")).text;
            if (!assets.meshPath.empty() || !assets.brushPath.empty()) visualAssets.push_back(std::move(assets));
            std::string tag;
            bool hasTag{};
            if (saved != prepared.objects.end()) {
                const auto values=saved->values.find("tag");
                if (values!=saved->values.end() && values->second.count(0u)) { tag=values->second.at(0u).text; hasTag=true; }
            }
            if (!hasTag) for (auto* source=actor;source;source=source==actor ? actor->cls : source->base) {
                const auto values=source->namePropertyValues.find("tag");
                if (values!=source->namePropertyValues.end() && values->second.count(0u)) {tag=values->second.at(0u);break;}
            }
            tag=LowerAscii(tag);
            if (!tag.empty() && tag!="none") prepared.tags[tag].push_back(actor);
        }
        if (persistentWorldRevision == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Portable world revision budget exceeded");
        // Cold manifests can reference geometry absent from the live world.
        // Decode it while every allocating restore container is still staged.
        static_cast<void>(DecodeRuntimeActorAssets(visualAssets));
        // All versions replace native AI ownership. Missing graph sections in
        // legacy saves mean reset, never merge or re-run initialization.
        if (persistentRuntime->get()->nativeDecals) pendingBirthGarbage = true;
        persistentRuntime->get()->nativeDecals = prepared.nativeDecals.get();
        for (auto* object : persistentRuntime->get()->exports) {
            if (object->nativeAiManager) pendingBirthGarbage = true;
            object->nativeAiManager = nullptr;
        }
        ClearNativePlayerEdges(); // Checkpoints never restore engine-session UPlayer pointers.
        persistentRuntime->get()->exports.swap(prepared.exports);
        persistentWorldActors.swap(prepared.worldActors);
        persistentQualifiedObjects.swap(prepared.qualified); persistentVmObjects.swap(prepared.vm);
        persistentMapTagIndex.swap(prepared.tags);
        persistentActorCollision.Swap(prepared.collision);
        // Publish the staged engine-global stream only after all allocating
        // preflight. Older saves explicitly reset it; validation never does.
        persistentScriptRandomSeed = restoredRandomSeed;
        // Pre-RNG portable formats contain no stream evidence. Their explicit
        // migration baseline is CRT seed1, never the current random history.
        for (RuntimeObject* object : persistentRuntime->get()->exports) {
            object->scriptValues.clear(); object->classDefaultValues.clear();
            object->animationClock.reset(); object->stateObject.reset(); object->committedScriptState = false;
            object->stateTransitionFlags = 0u;
            object->latentTimeLeft.reset();
            object->childActors.clear(); object->basedActors.clear(); object->touchEventSent = {};
            object->committedLifecycle = false; object->worldRemoved = false;
        }
        for (auto& [target, based] : initialBases) target->basedActors.swap(based);
        for (std::size_t index = persistentScriptExportCount; index < persistentRuntime->get()->exports.size(); ++index) {
            RuntimeObject* object = persistentRuntime->get()->exports[index];
            object->active = true; object->activated = false; object->healthInitialized = false; object->health = 100.0f;
            object->moverMotion.reset();
        }
        persistentInventory.swap(inventory); persistentPlayerHealth = playerHealth;
        persistentCredits = credits; persistentSkillPoints = skillPoints;
        persistentConversationFlags.swap(restoredFlags); persistentGoals.swap(goals); persistentNotes.swap(notes);
        persistentAppliedDialogueEffects.swap(restoredApplied);
        for (auto* object : inactiveTargets) object->active = false;
        for (auto* object : activatedTargets) object->activated = true;
        for (auto& [actor, motion] : restoredMovers) actor->moverMotion = std::move(motion);
        for (const auto& entry : damagedTargets) {entry.first->healthInitialized = true; entry.first->health = entry.second;}
        for (auto& cls : prepared.classDefaults) cls.target->classDefaultValues.swap(cls.values);
        for (auto& object : prepared.objects) {
            object.target->scriptValues.swap(object.values); object.target->animationClock.swap(object.clock);
            object.target->stateObject.swap(object.state);
            object.target->latentTimeLeft.swap(object.latentTimeLeft);
            object.target->committedScriptState = true;
            if (object.lifecycle) {
                object.target->childActors.swap(object.children); object.target->basedActors.swap(object.based);
                object.target->touchEventSent = object.touchSent; object.target->worldRemoved = object.removed;
                object.target->committedLifecycle = true;
            }
        }
        for (const auto& root : prepared.aiManagers) root.get()->owner->nativeAiManager = root.get();
        ++persistentWorldRevision;
        return true;
    } catch (const std::exception& error) {
        __android_log_print(ANDROID_LOG_WARN, "quest_main", "DeusExQuest: runtime checkpoint %s rejected: %.512s",
            apply ? "load" : "validation", error.what());
    } catch (...) {
        __android_log_print(ANDROID_LOG_WARN, "quest_main", "DeusExQuest: runtime checkpoint %s rejected: unknown failure",
            apply ? "load" : "validation");
    }
    return false;
}

bool ValidatePortableRuntimeState(const std::string& path, const std::string& expectedMapName) {
    return ReadPortableRuntimeState(path, false, expectedMapName);
}

bool LoadPortableRuntimeState(const std::string& path) {
    CollectBirthGarbageAtBoundary collect;
    return ReadPortableRuntimeState(path, true, {});
}
