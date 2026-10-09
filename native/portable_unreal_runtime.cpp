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
struct InventoryIconClassConstraint {
    RuntimeObject* indexedClass{}; // Non-owning immutable metadata, not a new GC root.
    std::uint8_t nativeClass{};
    bool valid{};
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
    RuntimeObject* nativeXLevel{}; // Immutable LinkActorsToLevel baseline, not a script write.
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
    // Nonserialized local-storage identity. References held by an executing
    // expression cannot silently address replacement state locals.
    std::uint64_t stateLocalsRevision{};
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
    bool healthInitialized{};
    float health{100.0f};

protected:
    ~RuntimeObject() override {
        if (destroyed_ != nullptr) ++*destroyed_;
    }

    GCAllocation* Mark(GCAllocation* marklist) override {
        for (RuntimeObject* reference : references) {
            marklist = GC::MarkObject(marklist, reference);
        }
        for (auto* actor : childActors) marklist = GC::MarkObject(marklist, actor);
        for (auto* actor : basedActors) marklist = GC::MarkObject(marklist, actor);
        return marklist;
    }

private:
    std::size_t* destroyed_{};
};

class RuntimePackage final : public GCObject {
public:
    explicit RuntimePackage(std::size_t* destroyedPackage)
        : destroyedPackage_(destroyedPackage) {}

    std::vector<RuntimeObject*> exports;
    std::size_t structDescriptorBytes{};

protected:
    ~RuntimePackage() override {
        if (destroyedPackage_ != nullptr) ++*destroyedPackage_;
    }

    GCAllocation* Mark(GCAllocation* marklist) override {
        for (RuntimeObject* object : exports) {
            marklist = GC::MarkObject(marklist, object);
        }
        return marklist;
    }

private:
    std::size_t* destroyedPackage_{};
};

std::unique_ptr<GCRoot<RuntimePackage>> persistentRuntime;
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
    // The exact Texture native registrations/bases from NativeCastClass below;
    // no bare metaclass/suffix or package-name guess supplies a class identity.
    static constexpr const char* paths[] = {"", "Core.Object", "Engine.Bitmap", "Engine.Texture", "Engine.FractalTexture",
        "Engine.FireTexture", "Engine.IceTexture", "Engine.WaterTexture", "Engine.WaveTexture", "Engine.WetTexture", "Engine.ScriptedTexture"};
    return id < std::size(paths) ? paths[id] : "";
}
std::uint8_t InventoryNativeClassId(const std::string_view path) {
    for (std::uint8_t id = 1u; id <= 10u; ++id) if (SameInventoryIdentity(path, InventoryNativeClassPath(id))) return id;
    return 0u;
}
std::uint8_t InventoryNativeClassBase(const std::uint8_t id) {
    static constexpr std::uint8_t bases[] = {0u, 0u, 1u, 2u, 3u, 4u, 4u, 4u, 7u, 7u, 3u};
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
    if (!SameInventoryIdentity(name, "largeIcon") && !SameInventoryIdentity(name, "Icon")) return;
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
std::unordered_map<std::string, std::set<std::int32_t>> persistentSpeakerMissions;

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string DialogueKey(std::int32_t mission, const std::string& speaker) {
    return std::to_string(mission) + "\n" + LowerAscii(speaker);
}

bool IsDerivedFromPath(RuntimeObject* cls, const std::string& path) {
    for (RuntimeObject* current = cls; current != nullptr; current = current->base) {
        if (current->reflection.objectPath == path) return true;
    }
    return false;
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
        const std::int32_t mission = intProperty(list, "missionNumber", -1);
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
                for (auto* member : children(field, field->stateDescriptor->children)) {
                    if (!member->script) continue;
                    const auto& memberPackage = table(member->sourcePath);
                    const auto functionName = memberPackage.names.at(static_cast<std::size_t>(memberPackage.exports[member->exportIndex].ObjName)).Name;
                    state.functions.emplace(functionName.ToString(), member->reflection.objectPath);
                    ++summary.stateFunctions;
                }
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

PortableActorDispatchContext AuthoredDispatchContext(RuntimeObject* actor) {
    if (!actor->cls || !actor->cls->classDescriptor)
        throw std::runtime_error("Authored dispatch receiver has no serialized Class masks");
    PortableActorDispatchContext result;
    result.classProbeMask = actor->cls->classDescriptor->state.probeMask;
    if (!actor->serializedStack) return result;
    const auto& stack = *actor->serializedStack;
    if (stack.functionReference && stack.stateReference) {
        if (!actor->serializedFunctionCode || !(actor->serializedFunctionCode->script ||
            StateMetadata(actor->serializedFunctionCode) || actor->serializedFunctionCode->reflection.metaClass == "Struct"))
            throw std::runtime_error("Serialized frame function is not a UStruct identity");
        if (!stack.logicalOffset || *stack.logicalOffset != -1)
            throw std::runtime_error("Runnable serialized state continuation is not implemented");
        const auto* code = actor->serializedStateCode;
        if (!code || !StateMetadata(code))
            throw std::runtime_error("Serialized stopped frame is not a State/Class identity");
        result.codePath = code->reflection.objectPath;
        const auto* state = StateMetadata(code);
        result.codeMasks = QuestVr::ScriptDispatch::CodeMasks{state->probeMask, state->ignoreMask};
        result.stateName = result.codePath.substr(result.codePath.find_last_of('.') + 1u);
    }
    for (std::uint8_t index = 0; index < 64u; ++index)
        if (stack.probeMask & (std::uint64_t{1} << index))
            result.disabledNames.insert(QuestVr::ScriptDispatch::ProbeEventName(index));
    return result;
}

PortableActorDispatchContext CurrentDispatchContext(RuntimeObject* actor) {
    auto result = AuthoredDispatchContext(actor);
    if (!actor->stateObject) return result;
    const auto& live = *actor->stateObject;
    if (live.frameOverride) {
        result.codePath.clear(); result.stateName = "None"; result.codeMasks.reset();
        if (live.frame && !live.frame->codePath.empty()) {
            const auto found = persistentVmObjects.find(LowerAscii(live.frame->codePath));
            if (found == persistentVmObjects.end() || !found->second->stateDescriptor)
                throw std::runtime_error("Portable frame code is not an authored State");
            const auto* code = found->second;
            result.codePath = code->reflection.objectPath;
            result.stateName = result.codePath.substr(result.codePath.find_last_of('.') + 1u);
            result.codeMasks = QuestVr::ScriptDispatch::CodeMasks{
                code->stateDescriptor->probeMask, code->stateDescriptor->ignoreMask};
        }
    }
    result.disabledNames.clear();
    const auto key = QuestVr::ScriptDispatch::FoldName(result.stateName);
    for (const auto& [state, names] : live.disabled)
        if (QuestVr::ScriptDispatch::FoldName(state) == key) { result.disabledNames = names; break; }
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
        const auto found = persistentVmObjects.find(LowerAscii(path));
        if (found == persistentVmObjects.end() || found->second == nullptr)
            throw std::runtime_error("Portable actor VM object is unavailable: " + path);
        return found->second;
    }
    std::shared_ptr<const Function> Member(RuntimeObject* receiver, const std::string& name) {
        if (name.find('.') != std::string::npos) return FunctionIdentity(Object(name));
        const auto context = CurrentDispatchContext(receiver);
        const auto selected = QuestVr::ScriptDispatch::ResolveFunction(DispatchGraph(),
            receiver->cls->reflection.objectPath, context.stateName, name);
        if (selected) return FunctionIdentity(Object(*selected));
        throw std::runtime_error("Portable actor VM member function unavailable: " + name);
    }
    bool CanCall(const Function& function, const std::string& receiver) override {
        const auto context = CurrentDispatchContext(Object(receiver));
        const auto name = function.path.substr(function.path.find_last_of('.') + 1u);
        return QuestVr::ScriptDispatch::IsEnabled(name, context.classProbeMask, context.codeMasks, context.disabledNames);
    }
    std::shared_ptr<const Function> PrepareFunction(const Function& identity, const std::string&) override {
        return FunctionFor(Object(identity.path));
    }
    std::shared_ptr<const Function> ResolveEvent(const std::string& receiver, const std::string& name,
        const bool enumDispatch) override {
        RuntimeObject* actor = Object(receiver);
        if (!IsDerivedFromPath(actor->cls, "Engine.Actor")) throw std::runtime_error("Event receiver is not Engine.Actor");
        const auto context = CurrentDispatchContext(actor);
        const bool enabled = QuestVr::ScriptDispatch::IsEnabled(name, context.classProbeMask, context.codeMasks, context.disabledNames);
        if (!enabled) return {};
        const auto value = Read(actor, PropertyNamed(actor, "Level"), 0u);
        if (value.kind != Kind::Object || value.text.empty()) throw std::runtime_error("Event receiver has no authored Level binding");
        auto* level = Object(value.text);
        if (!IsDerivedFromPath(level->cls, "Engine.LevelInfo")) throw std::runtime_error("Event receiver Level is not Engine.LevelInfo");
        const auto begun = QuestVr::Vm::ToBool(Read(level, PropertyNamed(level, "bBegunPlay"), 0u));
        if (!QuestVr::ScriptDispatch::MayCallEvent(name, enabled, begun, ReadInheritedRuntimeBool(actor, "bDeleteMe"), enumDispatch)) return {};
        const auto path = QuestVr::ScriptDispatch::ResolveFunction(DispatchGraph(), actor->cls->reflection.objectPath, context.stateName, name);
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
        auto& state = EnsureStateObject(actor);
        if (!state.frameOverride || !state.frame || state.frame->codePath.empty())
            throw std::runtime_error("State label has no running code");
        const auto originalPath = state.frame->codePath;
        const auto stateName = originalPath.substr(originalPath.find_last_of('.') + 1u);
        const auto target = transition && QuestVr::ScriptDispatch::FoldName(label) == "none" ? "Begin" : label;
        const auto position = [&](RuntimeObject* code) {
            const auto program = StateProgram(code);
            const auto layout = QuestVr::Vm::AnalyzeProgram(*this, *program);
            const auto found = std::find_if(layout.labels.begin(), layout.labels.end(), [&](const auto& value) {
                return QuestVr::ScriptDispatch::FoldName(value.name) == QuestVr::ScriptDispatch::FoldName(target);
            });
            if (found == layout.labels.end()) return false;
            const auto statement = std::lower_bound(layout.statementOffsets.begin(), layout.statementOffsets.end(), found->offset);
            state.frame->codePath = code->reflection.objectPath;
            state.frame->statementIndex = static_cast<std::uint32_t>(statement - layout.statementOffsets.begin());
            // Only transition positioning clears a latent action. In-code goto
            // preserves a latent action reached while evaluating its label.
            if (transition) state.frame->latent = QuestVr::StateLatent::Continue;
            return true;
        };
        if (!transition && position(Object(originalPath))) return;
        for (auto* cls = actor->cls; cls; cls = cls->base) {
            const auto* graphClass = DispatchGraph().FindClass(cls->reflection.objectPath);
            const auto found = graphClass->states.find(QuestVr::ScriptDispatch::FoldName(stateName));
            if (found != graphClass->states.end() && position(Object(found->second.path))) return;
        }
        if (!transition) throw std::runtime_error("Could not find state label: " + target);
        state.frame->latent = QuestVr::StateLatent::Stop;
    }
    void Begin() override {
        if (transaction_) throw std::runtime_error("Portable actor VM nested host transaction");
        ValidateStateBudget();
        lifecycleOperations_ = 0u;
        beforeExportCount_ = persistentRuntime->get()->exports.size();
        beforeWorldSlots_ = persistentWorldActors.size();
        transaction_ = true;
    }
    void Commit() override {
        ValidateStateBudget();
        const bool worldChanged = persistentRuntime->get()->exports.size() != beforeExportCount_ ||
            std::any_of(saved_.begin(), saved_.end(), [](const auto& entry) {
            return !entry.first->classDescriptor && IsDerivedFromPath(entry.first->cls, "Engine.Actor");
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
        if (worldChanged) persistentMapTagIndex.swap(tags);
        if (worldChanged) ++persistentWorldRevision;
        beforeCollision_.reset();
        saved_.clear(); transaction_ = false;
    }
    void Rollback() noexcept override {
        if (!transaction_) return;
        if (beforeCollision_) persistentActorCollision.Swap(*beforeCollision_);
        for (auto& entry : saved_) {
            entry.first->scriptValues.swap(entry.second.values);
            entry.first->classDefaultValues.swap(entry.second.defaults);
            entry.first->animationClock.swap(entry.second.clock);
            entry.first->stateObject.swap(entry.second.state);
            entry.first->stateLocalsRevision = entry.second.revision;
            entry.first->committedScriptState = entry.second.committed;
            entry.first->childActors.swap(entry.second.children);
            entry.first->basedActors.swap(entry.second.based);
            entry.first->touchEventSent = entry.second.touchSent;
            entry.first->committedLifecycle = entry.second.lifecycle;
            entry.first->worldRemoved = entry.second.removed;
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
        beforeCollision_.reset(); saved_.clear(); transaction_ = false;
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
        const bool defaults = scope == QuestVr::Vm::Scope::Default;
        auto result = ReferenceFor(object, property, 0u, defaults);
        result->dimension = property.arrayDimension;
        result->element = [this, object, property, defaults](const std::size_t slot) {
            if (slot >= property.arrayDimension || slot > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("VM property fixed-array index is out of range");
            return ReferenceFor(object, property, static_cast<std::uint32_t>(slot), defaults);
        };
        return result;
    }
    Evaluation Native(const std::uint16_t index, const std::string& receiver,
        const std::vector<Evaluation>& arguments, const Function* declaration) override {
        RuntimeObject* object = Object(receiver);
        if (!object->active && !object->worldRemoved) throw std::runtime_error("VM native receiver is inactive");
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
        if (index == 720u) { // Actor.GetPlayerPawn, pinned NActor.cpp current-Level fallback.
            argumentCount(0u, 0u);
            // There is no owned native viewport session yet. In its absence the
            // pin searches actual Level slots, not snapshots, export order,
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
        if (index != 259u && index != 260u && index != 294u && index != 263u && index != 293u && index != 282u)
            throw std::runtime_error("Unsupported runtime native " + std::to_string(index) +
                (declaration ? " (" + declaration->path + ")" : std::string()));
        if (!IsDerivedFromPath(object->cls, "Engine.Actor"))
            throw std::runtime_error("Animation native receiver is not Engine.Actor");
        argumentCount(index == 282u ? 0u : index == 294u ? 2u : 1u,
            index == 260u ? 4u : index == 259u ? 3u : index == 294u ? 2u : index == 282u ? 0u : 1u);
        if (index == 282u) return {Value::Bool(ReadInheritedRuntimeFloat(object, "AnimRate") != 0.0f), {}};
        const auto* mesh = AnimationMesh(object);
        if (index == 263u) return {Value::Bool(QuestVr::ActorClockHasAnim(mesh, sequence())), {}};
        if (index == 293u) return {Value::Text(Kind::Name, QuestVr::ActorClockGetAnimGroup(mesh, sequence())), {}};
        QuestVr::ActorAnimationClock next = object->animationClock ? *object->animationClock : AuthoredClock(object);
        QuestVr::ActorAnimationCommand command;
        command.sequence = sequence();
        command.kind = index == 259u ? QuestVr::ActorAnimationCommandKind::PlayAnim : index == 260u ?
            QuestVr::ActorAnimationCommandKind::LoopAnim : QuestVr::ActorAnimationCommandKind::TweenAnim;
        command.rate = optionalFloat(1u, 1.0f);
        command.tweenTime = optionalFloat(index == 294u ? 1u : 2u, 0.0f);
        command.minRate = optionalFloat(3u, 0.0f);
        const auto applied = QuestVr::ApplyActorAnimationCommand(mesh, next, command);
        if (!applied.error.empty()) throw std::runtime_error(applied.error);
        if (applied.applied) {
            Touch(object); object->animationClock = std::move(next);
            SynchronizeClock(object); object->committedScriptState = true;
        }
        return {};
    }
    std::unique_ptr<QuestVr::Vm::Iterator> CreateIterator(const std::uint16_t index,
        const std::string&, const std::vector<Evaluation>& arguments, const Function*) override {
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
    Evaluation NativeWithExecution(const std::uint16_t index, const std::string& receiver,
        const std::vector<Evaluation>& arguments, const Function* declaration, QuestVr::Vm::Execution& execution) override {
        if (index == 278u) return SpawnNative(receiver, arguments, execution);
        if (index == 272u || index == 298u || index == 279u)
            return LifecycleNative(index, receiver, arguments, execution);
        if (index != 113u && index != 117u && index != 118u) return Native(index, receiver, arguments, declaration);
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
            const auto current = CurrentDispatchContext(actor);
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
        if (arguments.size() > 2u) throw std::runtime_error("GotoState argument count is invalid");
        const auto old = CurrentDispatchContext(actor);
        const auto requested = name(0u, old.stateName);
        const auto label = name(1u, "None");
        const auto selected = QuestVr::ScriptDispatch::ResolveState(DispatchGraph(), actor->cls->reflection.objectPath, requested);
        const std::string selectedPath = selected.value_or(std::string{});
        auto& state = EnsureStateObject(actor);
        if (!old.codePath.empty() && old.codePath != selectedPath)
            static_cast<void>(execution.CallEvent(receiver, "EndState", true, {}));
        if (!state.frameOverride) {
            QuestVr::StateFrame frame;
            // The immutable authored class-backed frame is stopped with PC 0.
            // Its unsupported native class locals are never fabricated; changed
            // state entry below replaces them before executable state code.
            frame.latent = old.codePath.empty() ? QuestVr::StateLatent::Continue : QuestVr::StateLatent::Stop;
            if (!old.codePath.empty() && old.codePath == selectedPath) {
                frame.codePath = old.codePath; frame.localsCodePath = old.codePath;
                for (const auto& property : StateProgram(Object(old.codePath))->variables)
                    frame.locals.push_back({property.key, std::vector<Value>(property.arrayDimension, property.zero)});
            }
            state.frame = std::move(frame); state.frameOverride = true;
        } else if (!state.frame) state.frame.emplace();
        if (old.codePath != selectedPath) {
            if (actor->stateLocalsRevision == std::numeric_limits<std::uint64_t>::max())
                throw std::runtime_error("State local revision budget exceeded");
            ++actor->stateLocalsRevision;
            state.frame->codePath = selectedPath; state.frame->localsCodePath = selectedPath;
            state.frame->locals.clear();
            if (selected) {
                for (const auto& property : StateProgram(Object(*selected))->variables)
                    state.frame->locals.push_back({property.key, std::vector<Value>(property.arrayDimension, property.zero)});
            }
        }
        if (selected) {
            state.hasStack = true;
            GotoStateLabel(receiver, label, true);
        }
        ValidateStateBudget();
        if (selected && old.codePath != selectedPath) static_cast<void>(execution.CallEvent(receiver, "BeginState", true, {}));
        return {};
    }
    Value Read(RuntimeObject* object, const Property& property, const std::uint32_t index,
        const bool defaults = false, const bool immutable = false) {
        const bool classDefaults = defaults || object->classDescriptor != nullptr;
        RuntimeObject* source = classDefaults ? DefaultClass(object) : object;
        if (classDefaults) {
            const auto values = source->classDefaultValues.find(LowerAscii(property.name));
            if (values != source->classDefaultValues.end()) {
                const auto slot = values->second.find(index);
                if (slot != values->second.end()) return slot->second;
            }
        } else {
            if (!immutable) {
                if (const auto* value = FindScriptOverlay(object, property.name.c_str(), index)) return *value;
            } else {
                const auto values=object->frozenBirthDefaults.find(LowerAscii(property.name));
                if (values!=object->frozenBirthDefaults.end()) {
                    const auto slot=values->second.find(index);
                    if (slot!=values->second.end()) return slot->second;
                }
            }
            if (LowerAscii(property.name) == "xlevel" && index == 0u && object->nativeXLevel)
                return Value::Text(Kind::Object, object->nativeXLevel->reflection.objectPath);
            if (LowerAscii(property.name) == "standingcount" && index == 0u && object->initialStandingCount)
                return Value::Byte(*object->initialStandingCount);
            if (LowerAscii(property.name) == "region" && property.zero.kind == Kind::Struct)
                return Region(object, property.zero);
        }
        while (source != nullptr) {
            const auto& properties = source->classDescriptor ? source->classDescriptor->defaults : source->instanceProperties;
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
    Property ClassProperty(RuntimeObject* actorClass, const std::string& name) {
        for (RuntimeObject* cls = actorClass; cls != nullptr; cls = cls->base) {
            const auto found = persistentVmObjects.find(LowerAscii(cls->reflection.objectPath + '.' + name));
            if (found != persistentVmObjects.end() && found->second->property) return Describe(found->second);
        }
        throw std::runtime_error("Runtime property metadata unavailable: " + name);
    }
    Property DescribeProperty(const std::string& key) { return Describe(Object(key)); }
    RuntimeObject* DefaultClass(RuntimeObject* object) {
        auto* cls = object->classDescriptor ? object : object->cls;
        if (!cls || !cls->classDescriptor || cls->reflection.metaClass != "Class" ||
            !IsDerivedFromPath(cls, "Engine.Actor") ||
            LowerAscii(std::filesystem::path(cls->sourcePath).extension().string()) == ".dx" ||
            Table(cls->sourcePath).exports.at(cls->exportIndex).ObjClass != 0)
            throw std::runtime_error("Class-default receiver is not a loaded script Actor UClass");
        return cls;
    }
    bool HasPropertyNamed(RuntimeObject* actor, const char* name) const {
        for (auto* cls=actor->cls;cls;cls=cls->base) {
            const auto found=persistentVmObjects.find(LowerAscii(cls->reflection.objectPath+'.'+name));
            if (found!=persistentVmObjects.end() && found->second->property) return true;
        }
        return false; // Explicit metadata-only fixtures lack native Actor fields.
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
private:
    void SpawnZone(RuntimeObject* actor, QuestVr::Vm::Execution& execution) {
        const auto regionProperty = PropertyNamed(actor, "Region");
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
        const auto attachTag = Read(actor, PropertyNamed(actor, "AttachTag"), 0u).text;
        if (!attachTag.empty() && LowerAscii(attachTag) != "none") {
            // Do not cache tags: earlier callbacks can change them. Iterate
            // actual Level slot order, including newly appended actor slots.
            for (std::size_t i = 0u; i < persistentWorldActors.size(); ++i) {
                LifecycleStep();
                auto* candidate = persistentWorldActors[i];
                if (candidate && !candidate->worldRemoved &&
                    LowerAscii(Read(candidate, PropertyNamed(candidate, "Tag"), 0u).text) == LowerAscii(attachTag))
                    SetBase(candidate, actor, false, execution);
            }
            return;
        }
        const bool physical = IsDerivedFromPath(actor->cls, "Engine.Decoration") ||
            IsDerivedFromPath(actor->cls, "Engine.Inventory") || IsDerivedFromPath(actor->cls, "Engine.Pawn");
        const auto physics = QuestVr::Vm::ToInt(Read(actor, PropertyNamed(actor, "Physics"), 0u));
        if (physical && !ActorLink(actor, "Base") &&
            QuestVr::Vm::ToBool(Read(actor, PropertyNamed(actor, "bCollideWorld"), 0u)) &&
            (physics == 0 || physics == 5)) {
            const auto record=CollisionRecord(actor);
            const auto overlap=persistentActorCollision.Query(record.location,record.height,record.radius);
            std::vector<RuntimeObject*> hits;
            for (const auto identity : overlap.actors) hits.push_back(reinterpret_cast<RuntimeObject*>(identity));
            std::stable_sort(hits.begin(),hits.end(),std::less<RuntimeObject*>{});
            if (!hits.empty()) SetBase(actor,hits.front(),true,execution);
        }
    }
    Evaluation SpawnNative(const std::string& receiver, const std::vector<Evaluation>& arguments,
        QuestVr::Vm::Execution& execution) {
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
        if (!cls->classDescriptor || (cls->classDescriptor->classFlags & 1u)) return none();
        auto* owner = values.size() > 1u && values[1].kind != Kind::Nothing &&
            !values[1].text.empty() ? Object(values[1].text) : nullptr;
        if (values.size() > 1u && values[1].kind != Kind::Nothing && values[1].kind != Kind::Object)
            throw std::runtime_error("Spawn Owner is not an Object");
        if (owner) RequireMapActor(owner);
        auto tag = cls->reflection.objectPath.substr(cls->reflection.objectPath.find_last_of('.') + 1u);
        if (values.size() > 2u && values[2].kind != Kind::Nothing) {
            if (values[2].kind != Kind::Name) throw std::runtime_error("Spawn Tag is not a Name");
            if (!values[2].text.empty() && LowerAscii(values[2].text) != "none") tag = values[2].text;
        }
        auto location = values.size() > 3u && values[3].kind != Kind::Nothing ?
            values[3] : Read(spawner, PropertyNamed(spawner, "Location"), 0u);
        auto rotation = values.size() > 4u && values[4].kind != Kind::Nothing ?
            values[4] : Read(spawner, PropertyNamed(spawner, "Rotation"), 0u);
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
        auto* level = ActorLink(spawner, "Level");
        if (!level || !IsDerivedFromPath(level->cls,"Engine.LevelInfo")) throw std::runtime_error("Spawn has no LevelInfo");
        const auto xlevelValue = Read(spawner, PropertyNamed(spawner,"XLevel"),0u);
        if (xlevelValue.kind != Kind::Object || xlevelValue.text.empty()) throw std::runtime_error("Spawn has no XLevel");
        auto* xlevel = Object(xlevelValue.text);
        if (xlevel->runtimeBirth || xlevel->sourcePath != persistentMapSourcePath ||
            LowerAscii(CastObjectIdentity(xlevelValue.text).classPath) != "engine.level")
            throw std::runtime_error("Spawn XLevel is not the current serialized Level");
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
        set("Tag",Value::Text(Kind::Name,tag)); set("bTicked",Read(spawner,PropertyNamed(spawner,"bTicked"),0u));
        set("Instigator",Read(spawner,PropertyNamed(spawner,"Instigator"),0u));
        set("Brush",Value::Text(Kind::Object,{})); set("Location",location); set("OldLocation",location); set("Rotation",rotation);
        auto region = Read(actor,PropertyNamed(actor,"Region"),0u,true);
        region.fields.at("zone") = Value::Text(Kind::Object,level->reflection.objectPath); set("Region",region);
        TouchCollision(); RegisterCollisionActor(actor,persistentActorCollision);
        SetOwner(actor,owner,execution);
        if (!QuestVr::Vm::ToBool(Read(level,PropertyNamed(level,"bBegunPlay"),0u))) return {Value::Text(Kind::Object,path),{}};
        for (const auto* event : {"Spawned","PreBeginPlay","BeginPlay"})
            static_cast<void>(execution.CallEvent(path,event,true,{}));
        if (QuestVr::Vm::ToBool(Read(actor,PropertyNamed(actor,"bDeleteMe"),0u))) return none();
        SpawnZone(actor,execution);
        for (const auto* event : {"PostBeginPlay","SetInitialState"}) static_cast<void>(execution.CallEvent(path,event,true,{}));
        static_cast<void>(execution.CallEvent(path,"PostPostBeginPlay",false,{}));
        SpawnBase(actor,execution);
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
        if (!actor || !IsDerivedFromPath(actor->cls, "Engine.Actor") ||
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
        std::uint64_t revision{};
        bool committed{};
        std::vector<RuntimeObject*> children, based;
        std::array<bool, 4u> touchSent{};
        bool lifecycle{}, removed{};
    };
    std::unordered_map<std::string, PortablePackageTables> tables_;
    std::unordered_map<std::string, std::shared_ptr<const Function>> functions_;
    std::unordered_map<std::string, std::shared_ptr<const QuestVr::AuthoredStructSchema>> structSchemas_;
    std::set<RuntimeObject*> buildingStructs_;
    std::set<RuntimeObject*> chargedStructDescriptors_;
    std::size_t structFields_{};
    std::size_t structSchemaBytes_{};
    std::unordered_map<RuntimeObject*, Before> saved_;
    std::optional<QuestVr::ActorCollisionRegistry> beforeCollision_;
    std::size_t lifecycleOperations_{};
    std::size_t beforeExportCount_{}, beforeWorldSlots_{};
    bool spawnNotificationLocked_{};
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
        if (type == "StructProperty") {
            return QuestVr::MakeAuthoredStructZero(*StructPropertySchema(property->reflection.objectPath));
        }
        throw std::runtime_error("Unsupported runtime property type " + type);
    }
    Property Describe(RuntimeObject* property) {
        if (!property->property) throw std::runtime_error("VM object has no property descriptor");
        const auto& descriptor = *property->property;
        const auto dot = property->reflection.objectPath.find_last_of('.');
        return {property->reflection.objectPath, property->reflection.objectPath.substr(dot + 1u),
            Zero(property), descriptor.flags, static_cast<std::size_t>(descriptor.arrayDimension)};
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
                if (tag.type == 13u) return Value::Text(Kind::String, DecodePortableStringProperty(tag));
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
            {"Engine.Font", "Core.Object"}
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
        if (object->runtimeBirth) throw std::runtime_error("Runtime birth is not a UClass");
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
        const auto found = persistentVmObjects.find(LowerAscii(path));
        if (found == persistentVmObjects.end()) {
            if (const auto registered = NativeCastClass(path))
                return {registered->path, "Core.Class", true};
            throw std::runtime_error("VM cast object identity unavailable: " + path);
        }
        auto* object = found->second;
        if (object->runtimeBirth) {
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
            if (LowerAscii(actual) != "core.class" || (!expected.empty() && !ClassDerives(value.text, expected)))
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
        auto result = std::make_shared<Reference>(); result->zero = property.zero;
        result->read = [this, object, property, index, defaults]() { return Read(object, property, index, defaults); };
        result->write = [this, object, property, index, defaults](const Value& value) {
            auto* target = defaults || object->classDescriptor ? DefaultClass(object) : object;
            const auto actual = ClassProperty(target->classDescriptor ? target : target->cls, property.name);
            if (LowerAscii(actual.key) != LowerAscii(property.key) || index >= actual.arrayDimension)
                throw std::runtime_error("VM property is not owned by its concrete receiver class");
            auto normalized = TypedLiveValue(property, value);
            Touch(target);
            if (target->classDescriptor) {
                target->classDefaultValues[LowerAscii(property.name)][index] = std::move(normalized);
            } else {
                target->scriptValues[LowerAscii(property.name)][index] = std::move(normalized);
                target->committedScriptState = true;
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
            saved_.emplace(object, Before{object->scriptValues, object->classDefaultValues, object->animationClock, object->stateObject,
                object->stateLocalsRevision, object->committedScriptState, object->childActors, object->basedActors,
                object->touchEventSent, object->committedLifecycle, object->worldRemoved});
        }
    }
    void TouchCollision() {
        if (!transaction_) throw std::runtime_error("Actor collision mutation outside a VM transaction");
        if (!beforeCollision_) beforeCollision_.emplace(persistentActorCollision);
    }
    void ValidateStateBudget() const {
        // Bound persistent state across independent calls, not just one VM's
        // temporary allocations. Measure without copying live values/sets.
        // State/default storage is bounded without copying live values. The
        // save path additionally applies combined gameplay/property/clock/
        // state/default capture and envelope budgets.
        QuestVr::ScriptStateLimits limits;
        limits.maxBytes = QuestVr::kMaximumSaveRuntimeBytes;
        QuestVr::ScriptStateDetail::Writer measured(limits, nullptr);
        QuestVr::ScriptStateDetail::Budget defaults{limits};
        const auto valueBudget = [&](const auto& self, const Value& value, const std::size_t depth) -> void {
            defaults.Node(depth); defaults.Retain(sizeof(value));
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
        std::size_t objects{}, totalNativeLinks{};
        for (const auto* object : persistentRuntime->get()->exports) {
            const auto nativeCount = object->childActors.size() + object->basedActors.size();
            if (nativeCount > limits.maxActorLinks || totalNativeLinks > limits.maxActorLinks - nativeCount)
                throw std::runtime_error("Persistent total native actor link budget exceeded");
            totalNativeLinks += nativeCount;
            if ((object->runtimeBirth || object->committedScriptState || object->stateObject || object->committedLifecycle) &&
                ++objects > limits.maxObjects)
                throw std::runtime_error("Persistent script/lifecycle actor count budget exceeded");
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
            mesh->lodMesh = std::make_unique<PortableLodMesh>(LoadPortableLodMesh(Table(mesh->sourcePath), mesh->exportIndex));
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
    }
    void UpdateClockProperty(RuntimeObject* object, const std::string& name, const std::uint32_t index) {
        if (!object->animationClock) return;
        // Preserve captured tween offsets while explicit script assignments
        // update animation properties. Blend commands are not dispatched yet.
        const auto previous = object->animationClock->pose;
        const auto priorBlends = object->animationClock->blends;
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
            object->animationClock->blends[slot].simulated = priorBlends[slot].simulated;
        }
        (void)name; (void)index;
    }
    Value Region(RuntimeObject* object, const Value& zero, const float verticalOffset = 0.0f) {
        if (object->sourcePath.empty() || PackageStem(object->sourcePath) != persistentMapPackageName)
            throw std::runtime_error("Actor.Region unavailable outside the current authored map");
        if (!rootModel_) rootModel_ = LoadPortableRootModel68(Table(object->sourcePath));
        auto location = Read(object, PropertyNamed(object, "Location"), 0u);
        location.vector[2] += verticalOffset;
        if (location.kind != Kind::Vector || !std::isfinite(location.vector[0]) ||
            !std::isfinite(location.vector[1]) || !std::isfinite(location.vector[2]))
            throw std::runtime_error("Actor.Region has an invalid location");
        std::uint32_t zone{}; std::int32_t leaf{};
        if (!rootModel_->nodes.empty()) {
            std::int32_t current{}; bool terminal{};
            for (std::size_t steps = 0u; steps <= rootModel_->nodes.size(); ++steps) {
                if (current < 0 || static_cast<std::size_t>(current) >= rootModel_->nodes.size())
                    throw std::runtime_error("Actor.Region BSP reference out of range");
                const auto& node = rootModel_->nodes[static_cast<std::size_t>(current)];
                const float side = location.vector[0]*node.planeX + location.vector[1]*node.planeY +
                    location.vector[2]*node.planeZ - node.planeW;
                if (node.front >= 0 && side >= 0.0f) current = node.front;
                else if (node.back >= 0 && side <= 0.0f) current = node.back;
                else {
                    zone = static_cast<std::uint32_t>(side >= 0.0f ? node.zone1 : node.zone0);
                    leaf = side >= 0.0f ? node.leaf0 : node.leaf1; terminal = true; break;
                }
            }
            if (!terminal) throw std::runtime_error("Actor.Region BSP traversal budget exceeded");
        }
        if (zone >= 64u) throw std::runtime_error("Actor.Region zone number out of range");
        RuntimeObject* zoneObject{};
        if (zone < rootModel_->zones.size() && rootModel_->zones[zone].actorReference != 0)
            zoneObject = Object(Qualified(Table(object->sourcePath), rootModel_->zones[zone].actorReference));
        if (zoneObject == nullptr) {
            for (std::size_t index = persistentScriptExportCount; index < persistentRuntime->get()->exports.size(); ++index) {
                auto* candidate = persistentRuntime->get()->exports[index];
                if (IsDerivedFromPath(candidate->cls, "Engine.LevelInfo")) { zoneObject = candidate; break; }
            }
        }
        if (zoneObject == nullptr) throw std::runtime_error("Actor.Region has no LevelInfo fallback");
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
    }
    return values;
}

QuestVr::ScriptSavedState CollectScriptSavedState() {
    QuestVr::ScriptSavedState saved;
    saved.mapName = persistentMapPackageName;
    PortableActorVmHost host;
    // Bound the additional snapshot before copying nested live values. The
    // codec independently measures the actual encoded/retained representation.
    QuestVr::ScriptStateLimits limits;
    limits.maxBytes = QuestVr::kMaximumSaveRuntimeBytes;
    QuestVr::ScriptStateDetail::Budget capture{limits};
    const auto measure = [&](const auto& self, const QuestVr::Vm::Value& value, const std::size_t depth) -> void {
        capture.Node(depth); capture.Retain(sizeof(value));
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
            const auto property = host.PropertyNamed(object, name);
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
    void RegisterBirths(const std::vector<QuestVr::ScriptSavedBirth>& births) {
        const auto& map = Table(mapPath_);
        const bool realLevel = std::any_of(map.exports.begin(),map.exports.end(),[&](const auto& entry) {
            return LowerAscii(Qualified(map,entry.ObjClass)) == "engine.level";
        });
        std::size_t baseline{};
        if (realLevel) baseline = ReadPortableLevel68ActorOrder(map).size();
        else for (const auto& entry : map.exports) {
            const auto found = persistentVmObjects.find(LowerAscii(Qualified(map,entry.ObjClass)));
            if (found != persistentVmObjects.end() && IsDerivedFromPath(found->second,"Engine.Actor")) ++baseline;
        }
        std::set<std::uint32_t> indices;
        for (const auto& birth : births) {
            auto* cls = DefaultClass(birth.classPath);
            if ((cls->classDescriptor->classFlags & 1u) ||
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
    bool IsBirth(const std::string& path) const { return birthClasses_.count(LowerAscii(path)) != 0u; }
    RuntimeObject* SavedClass(const QuestVr::ScriptSavedObject& record) {
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
    QuestVr::Vm::Value PropertyValue(RuntimeObject* cls, const QuestVr::ScriptSavedProperty& saved) {
        const auto property = host_.ClassProperty(cls, saved.name);
        if (LowerAscii(property.key) != LowerAscii(saved.key) ||
            LowerAscii(property.name) != LowerAscii(saved.name) || saved.index >= property.arrayDimension)
            throw std::runtime_error("Script save property identity or fixed array index is invalid");
        return TypedValue(property, saved.value);
    }
    QuestVr::StateObject StateValue(RuntimeObject* cls, const QuestVr::ScriptSavedObject& saved) {
        const auto& state = *saved.state;
        const auto exported = mapExports_.find(LowerAscii(saved.path));
        if (exported == mapExports_.end() && !IsBirth(saved.path)) throw std::runtime_error("Script save state actor is unavailable in its map");
        const bool authoredHasStack = exported != mapExports_.end() &&
            AnyFlags(Table(mapPath_).exports.at(exported->second).ObjFlags, ObjectFlags::HasStack);
        // HasStack is established by authored load or an actual GotoState. A
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
        if (frame.latent != QuestVr::StateLatent::Continue && frame.latent != QuestVr::StateLatent::Stop)
            throw std::runtime_error("Script save state latent action is not implemented by this runtime");
        if (frame.codePath.empty()) {
            if (!frame.localsCodePath.empty() || !frame.locals.empty())
                throw std::runtime_error("Script save cleared state frame retains local storage");
            // Pinned SetState(null) preserves the old PC and latent state.
            return normalized;
        }
        if (!state.hasStack || frame.localsCodePath.empty())
            throw std::runtime_error("Script save executable state has no HasStack flag or local schema");
        auto* code = StateDefinition(cls, frame.codePath);
        auto* localsCode = StateDefinition(cls, frame.localsCodePath);
        const auto leaf = [](const std::string& path) { return LowerAscii(path.substr(path.find_last_of('.') + 1u)); };
        if (leaf(code->reflection.objectPath) != leaf(localsCode->reflection.objectPath))
            throw std::runtime_error("Script save executable/local state names disagree");
        const auto program = host_.StateProgram(code);
        const auto localProgram = host_.StateProgram(localsCode);
        const auto layout = QuestVr::Vm::AnalyzeProgram(host_, *program);
        if (frame.latent == QuestVr::StateLatent::Continue && frame.statementIndex > layout.statementOffsets.size())
            throw std::runtime_error("Script save running state PC exceeds the program's terminal ordinal");
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
            if (LowerAscii(actual) != "core.class" ||
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

PreparedScriptState PrepareScriptSavedState(
    const QuestVr::ScriptSavedState& saved, const bool apply) {
    if (apply && LowerAscii(saved.mapName) != LowerAscii(persistentMapPackageName))
        throw std::runtime_error("Script save can only restore into its authored map");
    ScriptSaveSchema schema(saved.mapName);
    // Register the entire symbolic graph before validating a single property:
    // valid references can point to births not present in the running session.
    schema.RegisterBirths(saved.births);
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
            auto value = schema.PropertyValue(cls, property);
            if (!next.values[LowerAscii(property.name)].emplace(property.index, std::move(value)).second)
                throw std::runtime_error("Duplicate script save property alias/index");
        }
        if (record.clock) {
            for (const auto& property : ClockProperties(*record.clock)) {
                const auto values = next.values.find(LowerAscii(property.name));
                if (values == next.values.end()) throw std::runtime_error("Script save clock property is missing");
                const auto slot = values->second.find(property.index);
                if (slot == values->second.end() || !QuestVr::Vm::Equal(slot->second, property.value))
                    throw std::runtime_error("Script save clock and reflected property disagree");
            }
        }
        next.clock = record.clock;
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
                                 "Instigator","Brush","Location","OldLocation","Rotation","Region","Owner"})
            if (std::none_of(encoded->properties.begin(),encoded->properties.end(),[&](const auto& property) {
                return LowerAscii(property.name) == LowerAscii(name) && property.index == 0u;
            })) throw std::runtime_error("Saved birth is missing a native initialization field");
        if (apply) {
            auto* actor = target(birth.path);
            const auto record = std::find_if(prepared.objects.begin(),prepared.objects.end(),
                [&](const auto& object) { return object.target == actor; });
            const auto& outer = record->values.at("outer").at(0u);
            actor->outer = outer.text.empty() ? nullptr : target(outer.text);
            actor->reflection.outerPath = outer.text;
            if (actor->outer) actor->references.push_back(actor->outer);
            const auto& xlevel = record->values.at("xlevel").at(0u);
            actor->nativeXLevel = xlevel.text.empty() ? nullptr : target(xlevel.text);
        }
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
    const PortablePackageTables& package) {
    ShutdownPortableRuntime();
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
    const std::vector<PortablePackageTables>& packages) {
    ShutdownPortableRuntime();
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
    summary.objects = persistentRuntime->get()->exports.size();
    persistentScriptExportCount = summary.objects;
    summary.peakGcObjects = GC::GetStats().numObjects;
    GC::Collect();
    summary.passed = !packages.empty() && summary.objects != 0 &&
        summary.classes != 0 && summary.functions != 0 &&
        summary.normalizedBytecodeBytes != 0 &&
        GC::GetStats().numObjects == baseline + summary.objects + 1;
    return summary;
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
                for (RuntimeObject* object : tagged->second) object->activated = true;
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
    persistentDispatchGraph.reset();
    persistentDispatchSummary = {};
    persistentVmObjects.clear();
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
    persistentSpeakerMissions.clear();
    persistentPlayerHealth = 100.0f;
    GC::Collect();
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

PortableActorDispatchContext ReadPortableActorDispatchContext(const std::string& actorPath) {
    PortableActorVmHost host;
    auto* actor = host.Object(actorPath);
    if (!IsDerivedFromPath(actor->cls, "Engine.Actor"))
        throw std::runtime_error("Dispatch receiver is not Engine.Actor");
    return CurrentDispatchContext(actor);
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
    for (const auto* object : persistentRuntime->get()->exports)
        if (object->committedScriptState || !object->classDefaultValues.empty()) return true;
    return false;
}

bool GetPortableRuntimeUnsavedScriptState() {
    return GetPortableRuntimeScriptStatePresent();
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
        persistentMapPackageName.clear();
        persistentMapTagIndex.clear();
        return 0;
    }
    const std::size_t removed =
        persistentRuntime->get()->exports.size() - persistentScriptExportCount;
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
    persistentRuntime->get()->exports.resize(persistentScriptExportCount);
    ++persistentWorldRevision;
    persistentMapPackageName.clear();
    persistentMapTagIndex.clear();
    persistentWorldActors.clear(); persistentAuthoredWorldSlots = 0u; persistentMapSourcePath.clear();
    persistentActorCollision.Clear();
    GC::Collect();
    return removed;
}

std::vector<PortableActorSnapshot> GetPortableRuntimeMapActors(bool includeInactive) {
    std::vector<PortableActorSnapshot> snapshots;
    if (!persistentRuntime || !persistentRuntime->get()) return snapshots;
    for (std::size_t index = persistentScriptExportCount;
         index < persistentRuntime->get()->exports.size();
         ++index) {
        RuntimeObject* object = persistentRuntime->get()->exports[index];
        if (object->worldActorIndex < 0 || object->worldRemoved || (!includeInactive && !object->active) ||
            !IsDerivedFromPath(object->cls, "Engine.Actor")) continue;
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
        snapshots.push_back(std::move(snapshot));
    }
    return snapshots;
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
        meshObject->lodMesh = std::make_unique<PortableLodMesh>(
            LoadPortableLodMesh(package->second, meshObject->exportIndex));
        meshObject->lodMesh->texturePaths.reserve(meshObject->lodMesh->textures.size());
        for (const std::int32_t texture : meshObject->lodMesh->textures) {
            std::string texturePath = GetPortableObjectPath(package->second, texture);
            if (texture > 0 && !texturePath.empty()) {
                texturePath = PackageStem(meshObject->sourcePath) + "." + texturePath;
            }
            meshObject->lodMesh->texturePaths.push_back(std::move(texturePath));
        }
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

PortableLodMesh GetPortableRuntimeMesh(const std::string& meshPath) {
    const auto found = persistentVmObjects.find(LowerAscii(meshPath));
    if (found == persistentVmObjects.end() || !found->second->lodMesh) {
        throw std::runtime_error("Portable actor mesh is not decoded: " + meshPath);
    }
    return *found->second->lodMesh;
}

PortableLodMesh GetPortableRuntimeBrush(const std::string& brushPath) {
    const auto found = persistentVmObjects.find(LowerAscii(brushPath));
    if (found == persistentVmObjects.end() || !found->second->brushMesh) {
        throw std::runtime_error("Portable actor brush is not decoded: " + brushPath);
    }
    return *found->second->brushMesh;
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
        object->activated = !object->activated;
        result.handled = true;
        result.worldChanged = true;
        result.action = object->activated ? "mover_open" : "mover_close";
    } else if (IsDerivedFromPath(object->cls, "Engine.Triggers")) {
        object->activated = true;
        result.handled = true;
        result.action = "trigger";
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
    charge(8u * (limits.stringBytes + 64u) + sizeof(PortableInventoryDescriptor) + 256u * sizeof(RuntimeObject*));
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
        const bool hasScript = !scriptState.objects.empty() || hasDefaults || hasBirths;
        const bool hasState = std::any_of(scriptState.objects.begin(), scriptState.objects.end(),
            [](const auto& object) { return object.state.has_value(); });
        const bool hasLifecycle = std::any_of(scriptState.objects.begin(), scriptState.objects.end(),
            [](const auto& object) { return object.lifecycle.has_value(); });
        if (hasScript) static_cast<void>(PrepareScriptSavedState(scriptState, false));
        std::vector<std::string> inactive;
        std::vector<std::string> activated;
        std::vector<std::pair<std::string, float>> damaged;
        for (std::size_t index = persistentScriptExportCount;
             index < persistentRuntime->get()->exports.size(); ++index) {
            RuntimeObject* object = persistentRuntime->get()->exports[index];
            if (!object->active) inactive.push_back(object->reflection.objectPath);
            if (object->activated) activated.push_back(object->reflection.objectPath);
            if (object->healthInitialized) damaged.emplace_back(object->reflection.objectPath, object->health);
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
        // Only the version word and appended trailer differ for v4-v7. Pure
        // property/clock captures retain the byte-exact original v4 format.
        write32(0x53515844u); write32(hasBirths ? 8u : hasLifecycle ? 7u : hasDefaults ? 6u : hasState ? 5u : hasScript ? 4u : 3u);
        writeStrings(persistentInventory); writeStrings(inactive); writeStrings(activated); writeFloat(persistentPlayerHealth);
        write32(static_cast<std::uint32_t>(damaged.size()));
        for (const auto& entry : damaged) {
            if (entry.second < 0.0f) throw std::runtime_error("Runtime checkpoint actor health is negative");
            writeString(entry.first); writeFloat(entry.second);
        }
        write32(static_cast<std::uint32_t>(persistentCredits)); write32(static_cast<std::uint32_t>(persistentSkillPoints));
        writeStrings(flags); writeStrings(persistentGoals); writeStrings(persistentNotes); writeStrings(applied);
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
        const auto version = read32();
        if (version < 1u || version > 8u) throw std::runtime_error("Runtime checkpoint version is unsupported");
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
        PreparedScriptState prepared;
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
            for (const auto& birth : scriptState.births) savedBirthKeys.emplace(LowerAscii(birth.path));
            const bool hasLifecycle = std::any_of(scriptState.objects.begin(), scriptState.objects.end(),
                [](const auto& object) { return object.lifecycle.has_value(); });
            if ((version == 4u && (blob.at(6u) != QuestVr::ScriptStateDetail::Magic[6u] || hasState)) ||
                (version == 5u && (blob.at(6u) != QuestVr::ScriptStateDetail::StateFrameVersion || !hasState)) ||
                (version == 6u && (blob.at(6u) != QuestVr::ScriptStateDetail::ClassDefaultsVersion || !hasDefaults)) ||
                (version == 7u && (blob.at(6u) != QuestVr::ScriptStateDetail::ActorLifecycleVersion || !hasLifecycle)) ||
                (version == 8u && (blob.at(6u) != QuestVr::ScriptStateDetail::BirthManifestVersion || !hasBirths)))
                throw std::runtime_error("Runtime checkpoint state trailer does not match its envelope version");
            if (!expectedMapName.empty() && LowerAscii(expectedMapName) != LowerAscii(scriptState.mapName))
                throw std::runtime_error("Runtime checkpoint script map does not match save metadata");
            prepared = PrepareScriptSavedState(scriptState, apply);
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
        persistentRuntime->get()->exports.swap(prepared.exports);
        persistentWorldActors.swap(prepared.worldActors);
        persistentQualifiedObjects.swap(prepared.qualified); persistentVmObjects.swap(prepared.vm);
        persistentMapTagIndex.swap(prepared.tags);
        persistentActorCollision.Swap(prepared.collision);
        for (RuntimeObject* object : persistentRuntime->get()->exports) {
            object->scriptValues.clear(); object->classDefaultValues.clear();
            object->animationClock.reset(); object->stateObject.reset(); object->committedScriptState = false;
            object->childActors.clear(); object->basedActors.clear(); object->touchEventSent = {};
            object->committedLifecycle = false; object->worldRemoved = false;
        }
        for (auto& [target, based] : initialBases) target->basedActors.swap(based);
        for (std::size_t index = persistentScriptExportCount; index < persistentRuntime->get()->exports.size(); ++index) {
            RuntimeObject* object = persistentRuntime->get()->exports[index];
            object->active = true; object->activated = false; object->healthInitialized = false; object->health = 100.0f;
        }
        persistentInventory.swap(inventory); persistentPlayerHealth = playerHealth;
        persistentCredits = credits; persistentSkillPoints = skillPoints;
        persistentConversationFlags.swap(restoredFlags); persistentGoals.swap(goals); persistentNotes.swap(notes);
        persistentAppliedDialogueEffects.swap(restoredApplied);
        for (auto* object : inactiveTargets) object->active = false;
        for (auto* object : activatedTargets) object->activated = true;
        for (const auto& entry : damagedTargets) {entry.first->healthInitialized = true; entry.first->health = entry.second;}
        for (auto& cls : prepared.classDefaults) cls.target->classDefaultValues.swap(cls.values);
        for (auto& object : prepared.objects) {
            object.target->scriptValues.swap(object.values); object.target->animationClock.swap(object.clock);
            object.target->stateObject.swap(object.state);
            object.target->committedScriptState = true;
            if (object.lifecycle) {
                object.target->childActors.swap(object.children); object.target->basedActors.swap(object.based);
                object.target->touchEventSent = object.touchSent; object.target->worldRemoved = object.removed;
                object.target->committedLifecycle = true;
            }
        }
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
