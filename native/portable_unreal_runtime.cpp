#include "portable_unreal_runtime.h"

#include "GC/GC.h"

#include "portable_log.h"
#include "portable_model_geometry.h"
#include "quest_actor_animation_clock.h"

#include <memory>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {

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
    std::unique_ptr<PortableScriptBody> script;
    std::unique_ptr<PortablePropertyDescriptor> property;
    std::unique_ptr<PortableClassDescriptor> classDescriptor;
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
    std::optional<QuestVr::ActorAnimationClock> animationClock;
    bool committedScriptState{};
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
std::unordered_map<std::string, std::vector<RuntimeObject*>> persistentMapTagIndex;
std::size_t persistentScriptExportCount{};
std::string persistentMapPackageName;
std::vector<std::string> persistentInventory;
float persistentPlayerHealth{100.0f};
std::int32_t persistentCredits{};
std::int32_t persistentSkillPoints{};
std::unordered_map<std::string, bool> persistentConversationFlags;
std::vector<std::string> persistentGoals;
std::vector<std::string> persistentNotes;
std::unordered_set<std::string> persistentAppliedDialogueEffects;

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
    const auto found = object->scriptValues.find(LowerAscii(name));
    if (found == object->scriptValues.end()) return nullptr;
    const auto slot = found->second.find(index);
    return slot == found->second.end() ? nullptr : &slot->second;
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
        if (object->reflection.metaClass == "Class") ++summary.classes;
    }
    summary.objects = runtime->exports.size();
    summary.peakGcObjects = GC::GetStats().numObjects;
}

// Scoped host for the actual normalized UE1 interpreter. It does not dispatch
// startup/state ticks or silently emulate missing natives. A complete nested
// invocation is one transaction, including native animation/tween history.
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
        if (name.find('.') != std::string::npos) return FunctionFor(Object(name));
        for (RuntimeObject* cls = receiver->cls; cls != nullptr; cls = cls->base) {
            const auto found = persistentVmObjects.find(LowerAscii(cls->reflection.objectPath + '.' + name));
            if (found != persistentVmObjects.end() && found->second->script && found->second->outer == cls)
                return FunctionFor(found->second);
        }
        throw std::runtime_error("Portable actor VM member function unavailable: " + name);
    }
    void Begin() override {
        if (transaction_) throw std::runtime_error("Portable actor VM nested host transaction");
        transaction_ = true;
    }
    void Commit() override { saved_.clear(); transaction_ = false; }
    void Rollback() noexcept override {
        for (auto& entry : saved_) {
            entry.first->scriptValues.swap(entry.second.values);
            entry.first->animationClock.swap(entry.second.clock);
            entry.first->committedScriptState = entry.second.committed;
        }
        saved_.clear(); transaction_ = false;
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
    std::shared_ptr<const Function> ResolveFunction(const Function& caller,
        const std::string& receiver, const QuestVr::Vm::Invocation& invocation) override {
        if (invocation.kind == QuestVr::Vm::CallKind::Final)
            return FunctionFor(Object(ResolveObject(caller, invocation.reference)));
        // A current script state has not been started by this host. Do not
        // infer it from Orders/InitialState or redirect a state call silently.
        return Member(Object(receiver), invocation.name);
    }
    std::shared_ptr<Reference> Variable(const std::string& receiver,
        const Property& property, const QuestVr::Vm::Scope scope) override {
        RuntimeObject* object = Object(receiver);
        if (!object->active) throw std::runtime_error("VM receiver is inactive");
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
        if (!object->active) throw std::runtime_error("VM native receiver is inactive");
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
    Value Read(RuntimeObject* object, const Property& property, const std::uint32_t index,
        const bool defaults = false) {
        if (!defaults) {
            if (const auto* value = FindScriptOverlay(object, property.name.c_str(), index)) return *value;
            if (LowerAscii(property.name) == "region" && property.zero.kind == Kind::Struct)
                return Region(object, property.zero);
        }
        RuntimeObject* source = defaults ? object->cls : object;
        while (source != nullptr) {
            const auto& properties = source->classDescriptor ? source->classDescriptor->defaults : source->instanceProperties;
            for (auto tag = properties.rbegin(); tag != properties.rend(); ++tag) {
                if (tag->name != property.name || tag->arrayIndex != index) continue;
                return Decode(*source, *tag, property.zero);
            }
            source = source == object ? object->cls : source->base;
        }
        return property.zero;
    }
    Property PropertyNamed(RuntimeObject* object, const std::string& name) {
        for (RuntimeObject* cls = object->cls; cls != nullptr; cls = cls->base) {
            const auto found = persistentVmObjects.find(LowerAscii(cls->reflection.objectPath + '.' + name));
            if (found != persistentVmObjects.end() && found->second->property) return Describe(found->second);
        }
        throw std::runtime_error("Runtime property metadata unavailable: " + name);
    }
private:
    struct Before {
        decltype(RuntimeObject::scriptValues) values;
        std::optional<QuestVr::ActorAnimationClock> clock;
        bool committed{};
    };
    std::unordered_map<std::string, PortablePackageTables> tables_;
    std::unordered_map<std::string, std::shared_ptr<const Function>> functions_;
    std::unordered_map<RuntimeObject*, Before> saved_;
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
        const auto path = GetPortableObjectPath(table, reference);
        if (path.empty()) throw std::runtime_error("Runtime property object reference unavailable");
        return reference > 0 ? PackageStem(table.sourcePath) + '.' + path : path;
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
            const auto structPath = Qualified(Table(property->sourcePath), descriptor.referencedType);
            if (structPath == "Core.Object.Vector") return Value::Vector({0.0f, 0.0f, 0.0f});
            if (structPath == "Core.Object.Rotator") return Value::Rotator({0, 0, 0});
            if (structPath == "Engine.Actor.PointRegion") {
                Value zero; zero.kind = Kind::Struct;
                zero.fields.emplace("zone", Value::Text(Kind::Object, {}));
                zero.fields.emplace("ileaf", Value::Integer(0));
                zero.fields.emplace("zonenumber", Value::Byte(0u));
                return zero;
            }
            throw std::runtime_error("Unsupported runtime struct type " + structPath);
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
    Value Decode(RuntimeObject& source, const PortableTaggedProperty& tag, const Value& zero) {
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
            case Kind::Vector:
                if (tag.type == 10u && tag.value.size() == 12u) {
                    std::array<float, 3> value{}; std::memcpy(value.data(), tag.value.data(), 12u); return Value::Vector(value);
                } break;
            case Kind::Rotator:
                if (tag.type == 10u && tag.value.size() == 12u) {
                    std::array<std::int32_t, 3> value{}; std::memcpy(value.data(), tag.value.data(), 12u); return Value::Rotator(value);
                } break;
            default: break;
        }
        throw std::runtime_error("Unsupported/malformed authored property " + tag.name.ToString());
    }
    std::shared_ptr<Reference> ReferenceFor(RuntimeObject* object, const Property& property,
        const std::uint32_t index, const bool defaults) {
        auto result = std::make_shared<Reference>(); result->zero = property.zero;
        result->read = [this, object, property, index, defaults]() { return Read(object, property, index, defaults); };
        result->write = [this, object, property, index, defaults](const Value& value) {
            if (defaults) throw std::runtime_error("Class-default mutation is not supported by actor VM host");
            Touch(object);
            object->scriptValues[LowerAscii(property.name)][index] = QuestVr::Vm::Coerce(value, property.zero);
            object->committedScriptState = true;
            UpdateClockProperty(object, property.name, index);
        };
        return result;
    }
    void Touch(RuntimeObject* object) {
        if (!transaction_) throw std::runtime_error("VM mutation outside a transaction");
        if (saved_.find(object) == saved_.end()) {
            if (saved_.size() >= 4096u) throw std::runtime_error("VM touched actor budget exceeded");
            saved_.emplace(object, Before{object->scriptValues, object->animationClock, object->committedScriptState});
        }
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
        const auto updated = AuthoredClock(object);
        object->animationClock->main = updated.main;
        object->animationClock->blends = updated.blends;
        object->animationClock->pose = updated.pose;
        object->animationClock->pose.main.previous = previous.main.previous;
        for (std::size_t slot = 0u; slot < previous.blends.size(); ++slot)
            object->animationClock->pose.blends[slot].previous = previous.blends[slot].previous;
        (void)name; (void)index;
    }
    Value Region(RuntimeObject* object, const Value& zero) {
        if (object->sourcePath.empty() || PackageStem(object->sourcePath) != persistentMapPackageName)
            throw std::runtime_error("Actor.Region unavailable outside the current authored map");
        if (!rootModel_) rootModel_ = LoadPortableRootModel68(Table(object->sourcePath));
        const auto location = Read(object, PropertyNamed(object, "Location"), 0u);
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
    const auto actorFound = persistentQualifiedObjects.find(actorPath);
    if (actorFound == persistentQualifiedObjects.end() || actorFound->second == nullptr) {
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
                    const auto object = persistentQualifiedObjects.find(itemPath);
                    const bool matchesClass = object != persistentQualifiedObjects.end() &&
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
    persistentVmObjects.clear();
    persistentRuntime.reset();
    persistentQualifiedObjects.clear();
    persistentMapTagIndex.clear();
    persistentScriptExportCount = 0;
    persistentMapPackageName.clear();
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
    try {
        PortableActorVmHost host;
        RuntimeObject* actor = host.Object(actorPath);
        if (!actor->active || !IsDerivedFromPath(actor->cls, "Engine.Actor"))
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
    if (!actor->active || !IsDerivedFromPath(actor->cls, "Engine.Actor"))
        throw std::runtime_error("Explicit VM receiver is not a live Engine.Actor");
    const auto property = host.PropertyNamed(actor, propertyName);
    if (arrayIndex >= property.arrayDimension)
        throw std::runtime_error("VM property fixed-array index is out of range");
    return host.Read(actor, property, arrayIndex);
}

bool GetPortableRuntimeUnsavedScriptState() {
    if (!persistentRuntime || !persistentRuntime->get()) return false;
    for (const auto* object : persistentRuntime->get()->exports)
        if (object->committedScriptState) return true;
    return false;
}

PortableMapRuntimeSummary LoadPortableRuntimeMap(
    const PortablePackageTables& package) {
    if (!persistentRuntime || !persistentRuntime->get()) {
        throw std::runtime_error("Cannot load a map without an initialized runtime");
    }
    PortableMapRuntimeSummary summary;
    if (GetPortableRuntimeUnsavedScriptState()) {
        __android_log_print(ANDROID_LOG_WARN, "quest_main",
            "DeusExQuest: map replacement refused: committed script/animation state is not persisted");
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
    }
    summary.exports = graph.objects.size();
    persistentMapPackageName = packageName;
    GC::Collect();
    summary.passed = summary.exports == package.exports.size() &&
        summary.actors != 0 && summary.actorProperties != 0 &&
        summary.resolvedClasses != 0;
    return summary;
}

std::size_t UnloadPortableRuntimeMap() {
    if (GetPortableRuntimeUnsavedScriptState())
        throw std::runtime_error("Map unload refused: committed script/animation state is not persisted");
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
    persistentMapPackageName.clear();
    persistentMapTagIndex.clear();
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
        if ((!includeInactive && !object->active) ||
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
        const auto mesh = persistentQualifiedObjects.find(snapshot.meshPath);
        if (mesh != persistentQualifiedObjects.end()) {
            snapshot.meshClassPath = mesh->second->reflection.metaClass;
        }
        snapshot.animByOwner = ReadInheritedRuntimeBool(object, "bAnimByOwner");
        snapshot.ownerPath = resolveInheritedObjectProperty("Owner");
        RuntimeObject* animationSource = object;
        if (snapshot.animByOwner && !snapshot.ownerPath.empty()) {
            const auto owner = persistentQualifiedObjects.find(snapshot.ownerPath);
            // Pinned VisibleMesh chooses the immediate owner only, not the
            // owner's own animation source. A destroyed/missing owner cannot
            // provide a live animation snapshot.
            if (owner != persistentQualifiedObjects.end() && owner->second != nullptr &&
                owner->second->active && IsDerivedFromPath(owner->second->cls, "Engine.Actor"))
                animationSource = owner->second;
        }
        snapshot.animationSourcePath = animationSource->reflection.objectPath;
        snapshot.animation = ReadRuntimeAnimationSnapshot(animationSource);
        snapshot.materialOverrides = object->textureOverrides;
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

PortableActorMeshSummary DecodePortableRuntimeActorMeshes() {
    PortableActorMeshSummary summary;
    const std::vector<PortableActorSnapshot> actors = GetPortableRuntimeMapActors(true);
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
        const auto found = persistentQualifiedObjects.find(meshPath);
        if (found == persistentQualifiedObjects.end()) continue;
        RuntimeObject* meshObject = found->second;
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
        const auto found = persistentQualifiedObjects.find(brushPath);
        if (found == persistentQualifiedObjects.end()) continue;
        RuntimeObject* brushObject = found->second;
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

PortableLodMesh GetPortableRuntimeMesh(const std::string& meshPath) {
    const auto found = persistentQualifiedObjects.find(meshPath);
    if (found == persistentQualifiedObjects.end() || !found->second->lodMesh) {
        throw std::runtime_error("Portable actor mesh is not decoded: " + meshPath);
    }
    return *found->second->lodMesh;
}

PortableLodMesh GetPortableRuntimeBrush(const std::string& brushPath) {
    const auto found = persistentQualifiedObjects.find(brushPath);
    if (found == persistentQualifiedObjects.end() || !found->second->brushMesh) {
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
            const auto found = persistentQualifiedObjects.find(path);
            if (found == persistentQualifiedObjects.end()) return;
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
            const auto found = persistentQualifiedObjects.find(path);
            if (found == persistentQualifiedObjects.end()) return;
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
            const auto runtimeTexture = persistentQualifiedObjects.find(qualified);
            if (runtimeTexture != persistentQualifiedObjects.end()) {
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
            const std::string textureClass = runtimeTexture == persistentQualifiedObjects.end()
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
    const auto found = persistentQualifiedObjects.find(objectPath);
    if (found == persistentQualifiedObjects.end() || found->second == nullptr) {
        result.action = "missing";
        result.inventoryCount = persistentInventory.size();
        return result;
    }
    RuntimeObject* object = found->second;
    result.classPath = object->cls == nullptr
        ? object->reflection.metaClass
        : object->cls->reflection.objectPath;
    if (!object->active) {
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
        if (!decodeDestination(object->instanceProperties)) {
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
    const auto found = persistentQualifiedObjects.find(objectPath);
    if (found == persistentQualifiedObjects.end() || found->second == nullptr ||
        !std::isfinite(damage) || damage <= 0.0f) {
        return result;
    }
    RuntimeObject* object = found->second;
    if (!object->active || !IsDerivedFromPath(object->cls, "Engine.Pawn")) return result;
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
        if (!decodeHealth(object->instanceProperties)) {
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
        if (object->active && IsDerivedFromPath(object->cls, "Engine.Inventory")) {
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
    // Do not truncate a save that cannot represent committed VM properties or
    // native tween history. v1-v3 persist only the existing gameplay facade.
    if (GetPortableRuntimeUnsavedScriptState()) {
        __android_log_print(ANDROID_LOG_WARN, "quest_main",
            "DeusExQuest: save refused: v3 cannot preserve committed script/animation state");
        return false;
    }
    std::vector<std::string> inactive;
    std::vector<std::string> activated;
    std::vector<std::pair<std::string, float>> damaged;
    for (std::size_t index = persistentScriptExportCount;
         index < persistentRuntime->get()->exports.size(); ++index) {
        RuntimeObject* object = persistentRuntime->get()->exports[index];
        if (!object->active) inactive.push_back(object->reflection.objectPath);
        if (object->activated) activated.push_back(object->reflection.objectPath);
        if (object->healthInitialized) {
            damaged.emplace_back(object->reflection.objectPath, object->health);
        }
    }
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) return false;
    const std::uint32_t magic = 0x53515844u;
    const std::uint32_t version = 3u;
    const auto write32 = [&](std::uint32_t value) {
        return std::fwrite(&value, sizeof(value), 1, file) == 1;
    };
    const auto writeStrings = [&](const std::vector<std::string>& strings) {
        if (!write32(static_cast<std::uint32_t>(strings.size()))) return false;
        for (const std::string& value : strings) {
            if (value.size() > 1'048'576u ||
                !write32(static_cast<std::uint32_t>(value.size())) ||
                std::fwrite(value.data(), 1, value.size(), file) != value.size()) {
                return false;
            }
        }
        return true;
    };
    const auto writeDamaged = [&]() {
        if (!write32(static_cast<std::uint32_t>(damaged.size()))) return false;
        for (const auto& entry : damaged) {
            if (entry.first.size() > 1'048'576u ||
                !write32(static_cast<std::uint32_t>(entry.first.size())) ||
                std::fwrite(entry.first.data(), 1, entry.first.size(), file) != entry.first.size() ||
                std::fwrite(&entry.second, sizeof(entry.second), 1, file) != 1) {
                return false;
            }
        }
        return true;
    };
    std::vector<std::string> flags;
    flags.reserve(persistentConversationFlags.size());
    for (const auto& entry : persistentConversationFlags) {
        flags.push_back(entry.first + (entry.second ? "\n1" : "\n0"));
    }
    const std::vector<std::string> applied(
        persistentAppliedDialogueEffects.begin(), persistentAppliedDialogueEffects.end());
    const bool ok = write32(magic) && write32(version) &&
        writeStrings(persistentInventory) && writeStrings(inactive) &&
        writeStrings(activated) &&
        std::fwrite(&persistentPlayerHealth, sizeof(persistentPlayerHealth), 1, file) == 1 &&
        writeDamaged() &&
        std::fwrite(&persistentCredits, sizeof(persistentCredits), 1, file) == 1 &&
        std::fwrite(&persistentSkillPoints, sizeof(persistentSkillPoints), 1, file) == 1 &&
        writeStrings(flags) && writeStrings(persistentGoals) && writeStrings(persistentNotes) &&
        writeStrings(applied);
    const bool closed = std::fclose(file) == 0;
    return ok && closed;
}

static bool ReadPortableRuntimeState(const std::string& path, const bool apply) {
    if (!persistentRuntime || !persistentRuntime->get()) return false;
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return false;
    const auto read32 = [&](std::uint32_t& value) {
        return std::fread(&value, sizeof(value), 1, file) == 1;
    };
    const auto readStrings = [&](std::vector<std::string>& strings) {
        std::uint32_t count{};
        if (!read32(count) || count > 100'000u) return false;
        strings.clear();
        strings.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            std::uint32_t length{};
            if (!read32(length) || length > 1'048'576u) return false;
            std::string value(length, '\0');
            if (std::fread(value.data(), 1, length, file) != length) return false;
            strings.push_back(std::move(value));
        }
        return true;
    };
    std::uint32_t magic{}, version{};
    std::vector<std::string> inventory;
    std::vector<std::string> inactive;
    std::vector<std::string> activated;
    float playerHealth = 100.0f;
    std::vector<std::pair<std::string, float>> damaged;
    bool ok = read32(magic) && read32(version) && magic == 0x53515844u &&
        (version == 1u || version == 2u || version == 3u) &&
        readStrings(inventory) && readStrings(inactive) && readStrings(activated);
    if (ok && version >= 2u) {
        std::uint32_t damagedCount{};
        ok = std::fread(&playerHealth, sizeof(playerHealth), 1, file) == 1 &&
            std::isfinite(playerHealth) && playerHealth >= 0.0f && playerHealth <= 100.0f &&
            read32(damagedCount) && damagedCount <= 100'000u;
        for (std::uint32_t index = 0; ok && index < damagedCount; ++index) {
            std::uint32_t length{};
            ok = read32(length) && length <= 1'048'576u;
            if (!ok) break;
            std::string path(length, '\0');
            float health{};
            ok = ok && std::fread(path.data(), 1, length, file) == length &&
                std::fread(&health, sizeof(health), 1, file) == 1 &&
                std::isfinite(health) && health >= 0.0f;
            if (ok) damaged.emplace_back(std::move(path), health);
        }
    }
    std::int32_t credits{};
    std::int32_t skillPoints{};
    std::vector<std::string> flags;
    std::vector<std::string> goals;
    std::vector<std::string> notes;
    std::vector<std::string> applied;
    if (ok && version >= 3u) {
        ok = std::fread(&credits, sizeof(credits), 1, file) == 1 &&
            std::fread(&skillPoints, sizeof(skillPoints), 1, file) == 1 &&
            credits >= 0 && skillPoints >= 0 && readStrings(flags) &&
            readStrings(goals) && readStrings(notes) && readStrings(applied);
    }
    const int trailing = ok ? std::fgetc(file) : 0;
    ok = ok && trailing == EOF && std::ferror(file) == 0;
    std::fclose(file);
    if (!ok) return false;
    // Prepare allocating containers before replacing any live state. An
    // allocation failure must not leave half-restored inventory/progress.
    std::unordered_map<std::string, bool> restoredFlags;
    for (const std::string& flag : flags) {
        const std::size_t separator = flag.rfind('\n');
        if (separator == std::string::npos || separator == 0u ||
            separator + 2u != flag.size() || (flag.back() != '0' && flag.back() != '1') ||
            !restoredFlags.emplace(LowerAscii(flag.substr(0u, separator)), flag.back() == '1').second)
            return false;
    }
    std::unordered_set<std::string> restoredApplied(applied.begin(), applied.end());
    if (!apply) return true;
    // A valid legacy checkpoint restores authored actor properties. Clearing
    // happens only after complete validation, never during save inspection.
    for (RuntimeObject* object : persistentRuntime->get()->exports) {
        object->scriptValues.clear(); object->animationClock.reset();
        object->committedScriptState = false;
    }
    for (std::size_t index = persistentScriptExportCount;
         index < persistentRuntime->get()->exports.size(); ++index) {
        RuntimeObject* object = persistentRuntime->get()->exports[index];
        object->active = true;
        object->activated = false;
        object->healthInitialized = false;
        object->health = 100.0f;
    }
    persistentInventory = std::move(inventory);
    persistentPlayerHealth = playerHealth;
    persistentCredits = credits;
    persistentSkillPoints = skillPoints;
    persistentConversationFlags = std::move(restoredFlags);
    persistentGoals = std::move(goals);
    persistentNotes = std::move(notes);
    persistentAppliedDialogueEffects = std::move(restoredApplied);
    for (const std::string& objectPath : inactive) {
        const auto found = persistentQualifiedObjects.find(objectPath);
        if (found != persistentQualifiedObjects.end()) found->second->active = false;
    }
    for (const std::string& objectPath : activated) {
        const auto found = persistentQualifiedObjects.find(objectPath);
        if (found != persistentQualifiedObjects.end()) found->second->activated = true;
    }
    for (const auto& entry : damaged) {
        const auto found = persistentQualifiedObjects.find(entry.first);
        if (found != persistentQualifiedObjects.end()) {
            found->second->healthInitialized = true;
            found->second->health = entry.second;
        }
    }
    return true;
}

bool ValidatePortableRuntimeState(const std::string& path) {
    return ReadPortableRuntimeState(path, false);
}

bool LoadPortableRuntimeState(const std::string& path) {
    return ReadPortableRuntimeState(path, true);
}
