#pragma once

#include "quest_actor_animation_clock.h"
#include "quest_portable_vm.h"
#include "quest_state_frame.h"
#include "quest_ai_event_state.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>

// A structural, versioned codec, not file I/O or a script-state scheduler.
// The runtime must validate map/class/property identities and typed class
// schemas and native actor-link identities transactionally before applying
// decoded state. No packaged game assets, VM bytecode, guessed actor startup,
// or frame-clock debt are stored.
namespace QuestVr {

struct ScriptSavedProperty {
    std::string key, name;
    std::uint32_t index{};
    Vm::Value value;
};
struct ScriptSavedActorLifecycle {
    // Native lists are ordered, and reentrant callbacks may leave repeated
    // entries. Do not sort, deduplicate, or infer them from reflected links.
    std::vector<std::string> children;
    std::vector<std::string> basedActors;
    std::array<bool,4> touchEventSent{};
    bool worldRemoved{};
};
struct ScriptSavedObject {
    std::string path, classPath;
    std::vector<ScriptSavedProperty> properties;
    std::optional<ActorAnimationClock> clock;
    std::optional<StateObject> state{};
    std::optional<ScriptSavedActorLifecycle> lifecycle{};
};
struct ScriptSavedClassDefaults {
    std::string classPath;
    std::vector<ScriptSavedProperty> properties;
};
struct ScriptSavedBirth {
    std::string path, classPath;
    std::uint32_t worldActorIndex{};
    // Freeze the concrete CDO's mutable patch block at birth. Unchanged slots
    // retain the immutable authored baseline, never later mutable CDO patches.
    // Empty snapshots are valid; instance overlays are separate copied values.
    std::vector<ScriptSavedProperty> frozenDefaults;
};
struct ScriptSavedState {
    std::string mapName;
    std::vector<ScriptSavedObject> objects;
    std::vector<ScriptSavedClassDefaults> classDefaults{};
    std::vector<ScriptSavedBirth> births{};
    // Native level-owned graphs are neither reflected actor properties nor
    // spawned UObjects. An empty graph still records manager presence.
    std::vector<Ai::State> aiManagers{};
};
struct ScriptStateLimits {
    std::size_t maxBytes{32u << 20u};
    // Births and their optional overlays share one actor identity. Concrete
    // mutable class-default records consume additional identities as before.
    std::size_t maxObjects{4096u}, maxProperties{65'536u};
    std::size_t totalValueNodes{262'144u}, maxStringBytes{8192u}, maxDepth{32u};
    std::size_t maxStateLocals{65'536u}, maxLocalElements{262'144u};
    std::size_t maxDisabledStates{65'536u}, maxDisabledNames{262'144u};
    std::size_t maxActorLinks{65'536u};
    std::size_t maxAiManagers{4096u}, maxAiEventTypes{Ai::MaxEventTypes};
    std::size_t maxAiNodes{Ai::MaxNodes}, maxAiLinks{65'536u};
};

namespace ScriptStateDetail {
static_assert(sizeof(float)==4u && sizeof(double)==8u &&
    std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,
    "Script state codec requires IEEE binary32/binary64 floats");
inline constexpr std::array<std::uint8_t, 8> Magic{{'D','X','Q','V','M','S',1,0}};
inline constexpr std::uint8_t StateFrameVersion=2u;
inline constexpr std::uint8_t ClassDefaultsVersion=3u;
inline constexpr std::uint8_t ActorLifecycleVersion=4u;
inline constexpr std::uint8_t BirthManifestVersion=5u;
inline constexpr std::uint8_t AiManagerVersion=6u;
inline constexpr std::uint32_t MaximumWorldActorSlots=1'000'000u;
// Stable serialized tags deliberately do not depend on Vm::Kind ordinals.
enum class Tag : std::uint8_t {
    Nothing=0, Byte=1, Int=2, Bool=3, Float=4, Name=5, Object=6,
    String=7, Vector=8, Rotator=9, Struct=10
};

[[noreturn]] inline void Fail(const char* message) {
    throw std::runtime_error(std::string("Script saved state: ")+message);
}
inline unsigned char Fold(const unsigned char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c-'A'+'a') : c;
}
inline int Compare(const std::string_view a, const std::string_view b) {
    for (std::size_t i=0; i<std::min(a.size(),b.size()); ++i) {
        const auto x=Fold(static_cast<unsigned char>(a[i]));
        const auto y=Fold(static_cast<unsigned char>(b[i]));
        if (x!=y) return x<y ? -1 : 1;
    }
    return a.size()==b.size() ? 0 : a.size()<b.size() ? -1 : 1;
}
inline void Text(const std::string_view value, const ScriptStateLimits& limits,
                 const bool identity, const bool empty=false, const std::size_t cap=0) {
    if (value.size()>limits.maxStringBytes || (cap!=0 && value.size()>cap) || (!empty && value.empty()))
        Fail("string length is invalid or exceeds the budget");
    for (const unsigned char c : value) {
        if (c==0 || (identity && (c<32u || c>126u)))
            Fail("identity is not printable ASCII or text contains NUL");
    }
    // Value strings are bounded byte strings. Original UE1 strings may contain
    // code-page text rather than UTF-8; this codec preserves those bytes exactly.
}
inline void BirthPath(const std::string_view path,const std::string_view map) {
    if (path.size()<=map.size()+1u || path[map.size()]!='.' || Compare(path.substr(0u,map.size()),map)!=0)
        Fail("birth identity is not qualified by its saved map");
    bool component{};
    for (const unsigned char c : path.substr(map.size()+1u)) {
        if (c=='.') {
            if (!component) Fail("birth identity contains an empty component");
            component=false;
        } else {
            if (c<=32u || c>126u || c=='/' || c=='\\') Fail("birth identity component is invalid");
            component=true;
        }
    }
    if (!component) Fail("birth identity contains an empty component");
}
inline void BirthIndices(std::vector<std::uint32_t>& indices) {
    for (const auto index : indices) if (index>=MaximumWorldActorSlots) Fail("birth world actor index exceeds the fixed slot cap");
    std::sort(indices.begin(),indices.end());
    if (std::adjacent_find(indices.begin(),indices.end())!=indices.end()) Fail("duplicate birth world actor index");
}
struct SavedObjectIdentity { std::string_view path, classPath; };
template<typename Object,typename Path,typename Class>
inline bool BirthOverlay(const std::string_view path,const std::string_view cls,
    const std::vector<Object>& objects,Path objectPath,Class objectClass) {
    const auto found=std::lower_bound(objects.begin(),objects.end(),path,[&](const auto& object,const auto& value) {
        return Compare(objectPath(object),value)<0;
    });
    if (found==objects.end() || Compare(objectPath(*found),path)!=0) return false;
    if (Compare(objectClass(*found),cls)!=0) Fail("birth and actor overlay class identities disagree");
    return true;
}
template<typename Class,typename Path>
inline void BirthClassAlias(const std::string_view path,const std::vector<Class>& classes,Path classPath) {
    const auto found=std::lower_bound(classes.begin(),classes.end(),path,[&](const auto& cls,const auto& value) {
        return Compare(classPath(cls),value)<0;
    });
    if (found!=classes.end() && Compare(classPath(*found),path)==0)
        Fail("birth identity aliases a class-default identity");
}
inline void Finite(const float value) { if (!std::isfinite(value)) Fail("non-finite float"); }
inline void Clock(const ActorAnimationClock& clock, const ScriptStateLimits& limits) {
    if (!std::isfinite(clock.simulationTime) || clock.simulationTime<0.0)
        Fail("invalid animation simulation time");
    const auto channel=[&](const MeshAnimationChannel& value) {
        Text(value.sequence,limits,true,true);
        Finite(value.normalizedFrame);
        if (value.normalizedFrame>1.0f) Fail("animation frame exceeds the native clock range");
        Finite(value.previous.fraction);
        // History offsets need the actual mesh to validate. In particular the
        // pinned native's notify-at1 command stores fraction=NumFrames. Retain
        // this finite metadata verbatim; the sampler diagnoses it on access.
    };
    const auto rates=[&](float rate,float last,float min,float tween,float old) {
        for (const auto value : {rate,last,min,tween,old}) Finite(value);
        if (last<0.0f || last>=1.0f || tween<0.0f) Fail("invalid animation last frame or tween rate");
    };
    channel(clock.pose.main);
    rates(clock.main.rate,clock.main.last,clock.main.minRate,clock.main.tweenRate,clock.main.oldRate);
    for (std::size_t i=0; i<clock.blends.size(); ++i) {
        channel(clock.pose.blends[i]);
        const auto& blend=clock.blends[i];
        rates(blend.rate,blend.last,blend.minRate,blend.tweenRate,blend.oldRate);
        for (const auto value : blend.simulated) Finite(value);
    }
}

struct Budget {
    const ScriptStateLimits& limits;
    std::size_t retained{}, properties{}, nodes{}, stateLocals{}, localElements{}, disabledStates{}, disabledNames{}, actorLinks{};
    std::size_t aiManagers{}, aiEventTypes{}, aiNodes{}, aiLinks{};
    void Retain(std::size_t bytes) {
        if (bytes>limits.maxBytes || retained>limits.maxBytes-bytes)
            Fail("aggregate retained-state budget exceeded");
        retained+=bytes;
    }
    void Array(std::size_t count, std::size_t size) {
        if (size!=0 && count>limits.maxBytes/size) Fail("retained array budget exceeded");
        Retain(count*size);
    }
    void Properties(std::size_t count) {
        if (count>limits.maxProperties || properties>limits.maxProperties-count)
            Fail("aggregate property count exceeds the budget");
        properties+=count;
    }
    void Node(std::size_t depth) {
        if (depth>=limits.maxDepth || nodes>=limits.totalValueNodes)
            Fail("value depth or aggregate node budget exceeded");
        ++nodes;
    }
    void StateLocals(std::size_t count) { Aggregate(count,limits.maxStateLocals,stateLocals,"aggregate state-local count exceeds the budget"); }
    void LocalElements(std::size_t count) { Aggregate(count,limits.maxLocalElements,localElements,"aggregate state-local element count exceeds the budget"); }
    void DisabledStates(std::size_t count) { Aggregate(count,limits.maxDisabledStates,disabledStates,"aggregate disabled-state count exceeds the budget"); }
    void DisabledNames(std::size_t count) { Aggregate(count,limits.maxDisabledNames,disabledNames,"aggregate disabled-name count exceeds the budget"); }
    void ActorLinks(std::size_t count) { Aggregate(count,limits.maxActorLinks,actorLinks,"aggregate native actor-link count exceeds the budget"); }
    void AiManagers(std::size_t count) { Aggregate(count,limits.maxAiManagers,aiManagers,"aggregate AI manager count exceeds the budget"); }
    void AiEventTypes(std::size_t count) { Aggregate(count,limits.maxAiEventTypes,aiEventTypes,"aggregate AI event-type count exceeds the budget"); }
    void AiNodes(std::size_t count) { Aggregate(count,limits.maxAiNodes,aiNodes,"aggregate AI node count exceeds the budget"); }
    void AiLinks(std::size_t count) { Aggregate(count,limits.maxAiLinks,aiLinks,"aggregate AI link count exceeds the budget"); }
private:
    static void Aggregate(std::size_t count,std::size_t cap,std::size_t& current,const char* message) {
        if (count>cap || current>cap-count) Fail(message);
        current+=count;
    }
};

template<typename Item,typename Name>
inline std::vector<const Item*> Sorted(const std::vector<Item>& items, Name name) {
    std::vector<const Item*> sorted; sorted.reserve(items.size());
    for (const auto& item : items) sorted.push_back(&item);
    std::sort(sorted.begin(),sorted.end(),[&](const Item* a,const Item* b) {
        return Compare(name(*a),name(*b))<0;
    });
    return sorted;
}

using PropertyAlias=std::pair<std::string_view,std::uint32_t>;
inline void ValidatePropertyAliases(std::vector<PropertyAlias>& aliases) {
    std::sort(aliases.begin(),aliases.end(),[](const auto& a,const auto& b) {
        const auto order=Compare(a.first,b.first);
        return order!=0 ? order<0 : a.second<b.second;
    });
    for (std::size_t i=1u; i<aliases.size(); ++i)
        if (Compare(aliases[i-1u].first,aliases[i].first)==0 && aliases[i-1u].second==aliases[i].second)
            Fail("duplicate or case-colliding class-default property alias/index");
}

class Writer {
public:
    Writer(const ScriptStateLimits& limits, std::vector<std::uint8_t>* output)
        : limits_(limits),budget_{limits},output_(output) {}
    std::size_t size() const { return size_; }
    const Budget& measuredBudget() const { return budget_; }
    // Cumulative, non-copy validation of standalone live StateObjects. Runtime
    // transactions can preflight persistent state with the exact codec rules
    // before committing, without creating a saved-object snapshot or blob.
    // This method is deliberately unavailable on an emitting writer.
    void MeasureStateObject(const StateObject& state) {
        if (output_) Fail("state-only measurement requires a non-emitting writer");
        budget_.Retain(sizeof(StateObject));
        ObjectState(state);
    }
    void MeasureActorLifecycle(const ScriptSavedActorLifecycle& lifecycle) {
        if (output_) Fail("actor-lifecycle measurement requires a non-emitting writer");
        budget_.Retain(sizeof(ScriptSavedActorLifecycle));
        ActorLifecycle(lifecycle);
    }
    void MeasureAiManager(const Ai::State& state) {
        if (output_) Fail("AI-manager measurement requires a non-emitting writer");
        budget_.AiManagers(1u);
        AiManager(state);
    }
    void State(const ScriptSavedState& state) {
        budget_.Retain(sizeof(ScriptSavedState));
        const bool withFrames=std::any_of(state.objects.begin(),state.objects.end(),[](const auto& object) { return object.state.has_value(); });
        const bool withDefaults=!state.classDefaults.empty();
        const bool withLifecycle=std::any_of(state.objects.begin(),state.objects.end(),[](const auto& object) { return object.lifecycle.has_value(); });
        const bool withBirths=!state.births.empty();
        const bool withAi=!state.aiManagers.empty();
        const auto version=withAi ? AiManagerVersion : withBirths ? BirthManifestVersion : withLifecycle ? ActorLifecycleVersion : withDefaults ? ClassDefaultsVersion : withFrames ? StateFrameVersion : Magic[6u];
        for (std::size_t i=0; i<Magic.size(); ++i) Byte(i==6u ? version : Magic[i]);
        String(state.mapName,true,false,128u);
        if (state.objects.size()>limits_.maxObjects || state.classDefaults.size()>limits_.maxObjects-state.objects.size() ||
            state.births.size()>limits_.maxObjects)
            Fail("aggregate object/class-default count exceeds the budget");
        budget_.Array(state.objects.size(),sizeof(ScriptSavedObject)+sizeof(void*));
        if (withBirths || withAi) budget_.Array(state.objects.size(),sizeof(SavedObjectIdentity));
        Count(state.objects.size());
        for (const auto& object : state.objects) {
            Text(object.path,limits_,true); Text(object.classPath,limits_,true);
        }
        const auto objects=Sorted(state.objects,[](const ScriptSavedObject& value) -> const std::string& { return value.path; });
        for (std::size_t i=0; i<objects.size(); ++i) {
            const auto& object=*objects[i];
            if (i!=0 && Compare(objects[i-1]->path,object.path)==0) Fail("duplicate or case-colliding object path");
            String(object.path,true); String(object.classPath,true);
            Properties(object.properties,false);
            Byte(object.clock ? 1u : 0u);
            if (object.clock) Animation(*object.clock);
            if (withFrames || withDefaults || withLifecycle || withBirths || withAi) {
                Byte(object.state ? 1u : 0u);
                if (object.state) ObjectState(*object.state);
            }
            if (withLifecycle || withBirths || withAi) {
                Byte(object.lifecycle ? 1u : 0u);
                if (object.lifecycle) ActorLifecycle(*object.lifecycle);
            }
        }
        budget_.Array(state.classDefaults.size(),sizeof(ScriptSavedClassDefaults)+sizeof(void*));
        if (withBirths || withAi) budget_.Array(state.classDefaults.size(),sizeof(std::string_view));
        const auto classes=Sorted(state.classDefaults,[](const auto& defaults) -> const std::string& { return defaults.classPath; });
        if (withDefaults || withLifecycle || withBirths || withAi) {
            Count(state.classDefaults.size());
            for (const auto& defaults : state.classDefaults) Text(defaults.classPath,limits_,true);
            for (std::size_t i=0; i<classes.size(); ++i) {
                const auto& defaults=*classes[i];
                if (i!=0 && Compare(classes[i-1u]->classPath,defaults.classPath)==0)
                    Fail("duplicate or case-colliding class-default identity");
                if (defaults.properties.empty()) Fail("empty class-default record");
                String(defaults.classPath,true); Properties(defaults.properties,true);
            }
        }
        if (withBirths || withAi) {
            budget_.Array(state.births.size(),sizeof(ScriptSavedBirth)+sizeof(void*));
            budget_.Array(state.births.size(),sizeof(std::uint32_t));
            std::vector<std::uint32_t> indices; indices.reserve(state.births.size());
            Count(state.births.size());
            for (const auto& birth : state.births) {
                Text(birth.path,limits_,true); Text(birth.classPath,limits_,true); BirthPath(birth.path,state.mapName);
                indices.push_back(birth.worldActorIndex);
            }
            BirthIndices(indices);
            const auto births=Sorted(state.births,[](const auto& birth) -> const std::string& { return birth.path; });
            auto identities=state.objects.size()+state.classDefaults.size();
            for (std::size_t i=0u; i<births.size(); ++i) {
                const auto& birth=*births[i];
                if (i!=0u && Compare(births[i-1u]->path,birth.path)==0)
                    Fail("duplicate or case-colliding birth identity");
                BirthClassAlias(birth.path,classes,[](const auto* cls) -> const std::string& { return cls->classPath; });
                if (!BirthOverlay(birth.path,birth.classPath,objects,
                    [](const auto* object) -> const std::string& { return object->path; },
                    [](const auto* object) -> const std::string& { return object->classPath; }) && ++identities>limits_.maxObjects)
                    Fail("aggregate authored actor/birth/class-default identity budget exceeded");
                String(birth.path,true); String(birth.classPath,true); U32(birth.worldActorIndex); Properties(birth.frozenDefaults,true);
            }
        }
        if (withAi) {
            budget_.AiManagers(state.aiManagers.size());
            budget_.Array(state.aiManagers.size(),sizeof(void*));
            Count(state.aiManagers.size());
            const auto managers=Sorted(state.aiManagers,[](const auto& manager) -> const std::string& { return manager.ownerPath; });
            for (std::size_t i=0u; i<managers.size(); ++i) {
                if (i!=0u && Compare(managers[i-1u]->ownerPath,managers[i]->ownerPath)==0)
                    Fail("duplicate or case-colliding AI manager owner");
                AiManager(*managers[i]);
            }
        }
    }
private:
    const ScriptStateLimits& limits_;
    Budget budget_;
    std::vector<std::uint8_t>* output_;
    std::size_t size_{};
    std::size_t aiWork_{};
    void Byte(std::uint8_t value) {
        if (size_>=limits_.maxBytes) Fail("encoded-state byte budget exceeded");
        ++size_; if (output_) output_->push_back(value);
    }
    void U32(std::uint32_t value) { for (unsigned i=0; i<4u; ++i) Byte(static_cast<std::uint8_t>(value>>(8u*i))); }
    void U64(std::uint64_t value) { for (unsigned i=0; i<8u; ++i) Byte(static_cast<std::uint8_t>(value>>(8u*i))); }
    void F32(float value) { Finite(value); std::uint32_t bits; std::memcpy(&bits,&value,4u); U32(bits); }
    void Count(std::size_t count) {
        if (count>std::numeric_limits<std::uint32_t>::max()) Fail("serialized count exceeds uint32");
        U32(static_cast<std::uint32_t>(count));
    }
    void String(const std::string& value,bool identity,bool empty=false,std::size_t cap=0) {
        Text(value,limits_,identity,empty,cap); budget_.Retain(value.size()+1u);
        Count(value.size()); for (const unsigned char c : value) Byte(c);
    }
    void AiChannels(const Ai::Channels& channels) {
        F32(channels.visibility); F32(channels.volume); F32(channels.radius); F32(channels.smell);
    }
    void AiManager(const Ai::State& state) {
        budget_.AiEventTypes(state.eventTypes.size());
        budget_.AiNodes(state.senders.size()); budget_.AiNodes(state.receivers.size());
        budget_.AiLinks(2u+(state.receiverHead ? 1u : 0u));
        for (const auto& event : state.eventTypes) {
            budget_.AiLinks(event.senderIds.size()); budget_.AiLinks(event.receiverIds.size());
        }
        for (const auto& sender : state.senders) {
            static_cast<void>(sender); budget_.AiLinks(2u);
        }
        for (const auto& receiver : state.receivers)
            budget_.AiLinks(4u+(!receiver.previousBestActor.empty() ? 1u : 0u)+(!receiver.params.bestActor.empty() ? 1u : 0u));
        Ai::Validate(state,aiWork_);
        budget_.Retain(sizeof(Ai::State));
        String(state.ownerPath,true); String(state.levelPath,true);
        U32(state.processDepth); U32(state.pendingDeleteCount); Byte(state.historyCursor); U32(state.receiverHead);
        budget_.Array(state.eventTypes.size(),sizeof(Ai::EventType));
        Count(state.eventTypes.size());
        const auto ids=[&](const std::vector<Ai::Id>& values) {
            budget_.Array(values.size(),sizeof(Ai::Id));
            Count(values.size()); for (const auto id : values) U32(id);
        };
        for (const auto& event : state.eventTypes) {
            String(event.name,true); ids(event.senderIds); ids(event.receiverIds);
        }
        budget_.Array(state.senders.size(),sizeof(Ai::Sender));
        Count(state.senders.size());
        for (const auto& sender : state.senders) {
            String(sender.actor,true); U32(sender.eventType); Byte(sender.deleted ? 1u : 0u); F32(sender.score);
            for (const auto& channels : sender.history) AiChannels(channels);
            AiChannels(sender.current);
        }
        budget_.Array(state.receivers.size(),sizeof(Ai::Receiver));
        Count(state.receivers.size());
        for (const auto& receiver : state.receivers) {
            String(receiver.actor,true); U32(receiver.eventType); Byte(receiver.deleted ? 1u : 0u);
            String(receiver.callback,true,true); String(receiver.scoreCallback,true,true);
            Byte(receiver.flags.checkVisibility ? 1u : 0u); Byte(receiver.flags.checkDirection ? 1u : 0u);
            Byte(receiver.flags.checkCylinder ? 1u : 0u); Byte(receiver.flags.checkLineOfSight ? 1u : 0u);
            Byte(receiver.callbackPending ? 1u : 0u); Byte(receiver.eventState); Byte(receiver.detected ? 1u : 0u);
            F32(receiver.previousScore); String(receiver.previousBestActor,true,true); Byte(receiver.historyCursor);
            String(receiver.params.bestActor,true,true); F32(receiver.params.score); F32(receiver.params.visibility);
            F32(receiver.params.volume); F32(receiver.params.smell); U32(receiver.ringNext); U32(receiver.ringPrev);
        }
    }
    void ActorLifecycle(const ScriptSavedActorLifecycle& lifecycle) {
        Byte(lifecycle.worldRemoved ? 1u : 0u);
        for (const bool sent : lifecycle.touchEventSent) Byte(sent ? 1u : 0u);
        const auto links=[&](const std::vector<std::string>& values) {
            budget_.ActorLinks(values.size());
            budget_.Array(values.size(),sizeof(std::string));
            Count(values.size());
            for (const auto& path : values) String(path,true);
        };
        links(lifecycle.children); links(lifecycle.basedActors);
    }
    void Properties(const std::vector<ScriptSavedProperty>& values,const bool defaults) {
        budget_.Properties(values.size());
        budget_.Array(values.size(),sizeof(ScriptSavedProperty)+sizeof(void*));
        Count(values.size());
        std::vector<const ScriptSavedProperty*> properties; properties.reserve(values.size());
        std::vector<PropertyAlias> aliases;
        if (defaults) { budget_.Array(values.size(),sizeof(PropertyAlias)); aliases.reserve(values.size()); }
        for (const auto& property : values) {
            Text(property.key,limits_,true); Text(property.name,limits_,true);
            properties.push_back(&property);
            if (defaults) aliases.emplace_back(property.name,property.index);
        }
        if (defaults) ValidatePropertyAliases(aliases);
        std::sort(properties.begin(),properties.end(),[](const auto* a,const auto* b) {
            const auto order=Compare(a->key,b->key);
            return order!=0 ? order<0 : a->index<b->index;
        });
        for (std::size_t j=0; j<properties.size(); ++j) {
            const auto& property=*properties[j];
            if (j!=0 && Compare(properties[j-1u]->key,property.key)==0 &&
                (properties[j-1u]->key!=property.key || properties[j-1u]->index==property.index ||
                 properties[j-1u]->name!=property.name))
                Fail("duplicate or case-colliding property identity");
            String(property.key,true); String(property.name,true); U32(property.index); Value(property.value,0u);
        }
    }
    void Value(const Vm::Value& value,std::size_t depth) {
        budget_.Node(depth);
        switch (value.kind) {
        case Vm::Kind::Nothing: Byte(static_cast<std::uint8_t>(Tag::Nothing)); break;
        case Vm::Kind::Byte:
            if (value.integer<0 || value.integer>255) Fail("byte value is outside 0..255");
            Byte(static_cast<std::uint8_t>(Tag::Byte)); Byte(static_cast<std::uint8_t>(value.integer)); break;
        case Vm::Kind::Int: Byte(static_cast<std::uint8_t>(Tag::Int)); U32(static_cast<std::uint32_t>(value.integer)); break;
        case Vm::Kind::Bool: Byte(static_cast<std::uint8_t>(Tag::Bool)); Byte(value.boolean ? 1u : 0u); break;
        case Vm::Kind::Float: Byte(static_cast<std::uint8_t>(Tag::Float)); F32(value.floating); break;
        case Vm::Kind::Name: Byte(static_cast<std::uint8_t>(Tag::Name)); String(value.text,true,true); break;
        case Vm::Kind::Object: Byte(static_cast<std::uint8_t>(Tag::Object)); String(value.text,true,true); break;
        case Vm::Kind::String: Byte(static_cast<std::uint8_t>(Tag::String)); String(value.text,false,true); break;
        case Vm::Kind::Vector:
            Byte(static_cast<std::uint8_t>(Tag::Vector)); for (const auto component : value.vector) F32(component); break;
        case Vm::Kind::Rotator:
            Byte(static_cast<std::uint8_t>(Tag::Rotator));
            for (const auto component : value.rotation) U32(static_cast<std::uint32_t>(component));
            break;
        case Vm::Kind::Struct: {
            Byte(static_cast<std::uint8_t>(Tag::Struct));
            if (value.fields.size()>limits_.totalValueNodes-budget_.nodes) Fail("struct field count exceeds node budget");
            // Account map-node overhead plus sorting pointers before allocation.
            budget_.Array(value.fields.size(),sizeof(Vm::Value)+sizeof(std::string)+5u*sizeof(void*));
            Count(value.fields.size());
            using Field=std::pair<const std::string,Vm::Value>;
            std::vector<const Field*> fields; fields.reserve(value.fields.size());
            for (const auto& field : value.fields) {
                Text(field.first,limits_,true); fields.push_back(&field);
            }
            std::sort(fields.begin(),fields.end(),[](const auto* a,const auto* b) { return Compare(a->first,b->first)<0; });
            for (std::size_t i=0; i<fields.size(); ++i) {
                if (i!=0 && Compare(fields[i-1]->first,fields[i]->first)==0) Fail("duplicate or case-colliding struct field");
                String(fields[i]->first,true); Value(fields[i]->second,depth+1u);
            }
            break;
        }
        default: Fail("unrecognized in-memory value kind");
        }
    }
    void Channel(const MeshAnimationChannel& channel) {
        String(channel.sequence,true,true); F32(channel.normalizedFrame);
        U32(channel.previous.vertexOffset0); U32(channel.previous.vertexOffset1); F32(channel.previous.fraction);
    }
    void Rates(float rate,float last,float min,float tween,float old) {
        for (const auto value : {rate,last,min,tween,old}) F32(value);
    }
    void Animation(const ActorAnimationClock& clock) {
        Clock(clock,limits_); Channel(clock.pose.main);
        Rates(clock.main.rate,clock.main.last,clock.main.minRate,clock.main.tweenRate,clock.main.oldRate);
        Byte(clock.main.loop); Byte(clock.main.notify); Byte(clock.main.finished); Byte(clock.main.finishAnimWaiting);
        for (std::size_t i=0; i<clock.blends.size(); ++i) {
            Channel(clock.pose.blends[i]); const auto& blend=clock.blends[i];
            Rates(blend.rate,blend.last,blend.minRate,blend.tweenRate,blend.oldRate);
            for (const auto value : blend.simulated) F32(value);
        }
        std::uint64_t bits; std::memcpy(&bits,&clock.simulationTime,8u); U64(bits);
        Byte(clock.remoteRole); Byte(clock.pose.fatness);
    }
    void ObjectState(const StateObject& state) {
        if (state.frame && !state.frameOverride) Fail("state frame requires a portable frame override");
        Byte(state.hasStack ? 1u : 0u); Byte(state.frameOverride ? 1u : 0u); Byte(state.frame ? 1u : 0u);
        if (state.frame) {
            const auto& frame=*state.frame;
            String(frame.codePath,true,true); String(frame.localsCodePath,true,true);
            U32(frame.statementIndex);
            if (static_cast<std::uint8_t>(frame.latent)>static_cast<std::uint8_t>(StateLatent::WaitForLanding))
                Fail("unrecognized state latent action");
            Byte(static_cast<std::uint8_t>(frame.latent));
            budget_.StateLocals(frame.locals.size());
            budget_.Array(frame.locals.size(),sizeof(StateLocal)+sizeof(void*));
            Count(frame.locals.size());
            for (const auto& local : frame.locals) Text(local.key,limits_,true);
            const auto locals=Sorted(frame.locals,[](const StateLocal& local) -> const std::string& { return local.key; });
            for (std::size_t i=0; i<locals.size(); ++i) {
                const auto& local=*locals[i];
                if (i!=0 && Compare(locals[i-1]->key,local.key)==0) Fail("duplicate or case-colliding state-local identity");
                String(local.key,true);
                budget_.LocalElements(local.values.size());
                budget_.Array(local.values.size(),sizeof(Vm::Value));
                Count(local.values.size());
                for (const auto& value : local.values) Value(value,0u);
            }
        }
        budget_.DisabledStates(state.disabled.size());
        using Disabled=std::pair<const std::string,std::set<std::string>>;
        budget_.Array(state.disabled.size(),sizeof(Disabled)+5u*sizeof(void*));
        Count(state.disabled.size());
        std::vector<const Disabled*> states; states.reserve(state.disabled.size());
        for (const auto& item : state.disabled) { Text(item.first,limits_,true); states.push_back(&item); }
        std::sort(states.begin(),states.end(),[](const auto* a,const auto* b) { return Compare(a->first,b->first)<0; });
        for (std::size_t i=0; i<states.size(); ++i) {
            if (i!=0 && Compare(states[i-1]->first,states[i]->first)==0) Fail("duplicate or case-colliding disabled-state identity");
            String(states[i]->first,true); const auto& names=states[i]->second;
            budget_.DisabledNames(names.size());
            budget_.Array(names.size(),sizeof(std::string)+5u*sizeof(void*));
            Count(names.size());
            std::vector<const std::string*> sorted; sorted.reserve(names.size());
            for (const auto& name : names) { Text(name,limits_,true); sorted.push_back(&name); }
            std::sort(sorted.begin(),sorted.end(),[](const auto* a,const auto* b) { return Compare(*a,*b)<0; });
            for (std::size_t j=0; j<sorted.size(); ++j) {
                if (j!=0 && Compare(*sorted[j-1],*sorted[j])==0) Fail("duplicate or case-colliding disabled-name identity");
                String(*sorted[j],true);
            }
        }
    }
};

template<bool Materialize>
class Reader {
public:
    Reader(const std::vector<std::uint8_t>& bytes,const ScriptStateLimits& limits)
        : bytes_(bytes),limits_(limits),budget_{limits} {
        if (bytes.size()>limits.maxBytes) Fail("encoded-state byte budget exceeded");
    }
    ScriptSavedState State() {
        ScriptSavedState state; budget_.Retain(sizeof(ScriptSavedState));
        std::uint8_t version{};
        for (std::size_t i=0; i<Magic.size(); ++i) {
            const auto byte=Byte();
            if (i==6u) {
                version=byte;
                if (version!=Magic[i] && version!=StateFrameVersion && version!=ClassDefaultsVersion &&
                    version!=ActorLifecycleVersion && version!=BirthManifestVersion && version!=AiManagerVersion)
                    Fail("bad magic or unsupported codec version");
            } else if (byte!=Magic[i]) Fail("bad magic or unsupported codec version");
        }
        const auto map=String(true,false,128u); if constexpr(Materialize) state.mapName=map;
        const auto count=U32();
        if (count>limits_.maxObjects || count>Remaining()/13u) Fail("object count exceeds budget or encoded payload");
        budget_.Array(count,sizeof(ScriptSavedObject)+sizeof(void*));
        if constexpr(Materialize) state.objects.reserve(count);
        std::vector<SavedObjectIdentity> objectIdentities;
        std::vector<std::string_view> classIdentities;
        if (version>=BirthManifestVersion) {
            budget_.Array(count,sizeof(SavedObjectIdentity)); objectIdentities.reserve(count);
        }
        std::string_view previous;
        bool hasLifecycle{};
        for (std::uint32_t i=0; i<count; ++i) {
            ScriptSavedObject object;
            const auto path=String(true),cls=String(true);
            if (i!=0 && Compare(previous,path)>=0) Fail("object paths are not canonical and unique");
            previous=path;
            if (version>=BirthManifestVersion) objectIdentities.push_back({path,cls});
            if constexpr(Materialize) { object.path=path; object.classPath=cls; }
            Properties(object.properties,false);
            if (Boolean()) { auto clock=Animation(); if constexpr(Materialize) object.clock=std::move(clock); }
            if (version>=StateFrameVersion && Boolean()) {
                auto saved=ObjectState(); if constexpr(Materialize) object.state=std::move(saved);
            }
            if (version>=ActorLifecycleVersion && Boolean()) {
                hasLifecycle=true;
                auto saved=ActorLifecycle(); if constexpr(Materialize) object.lifecycle=std::move(saved);
            }
            if constexpr(Materialize) state.objects.push_back(std::move(object));
        }
        if (version==ActorLifecycleVersion && !hasLifecycle)
            Fail("actor-lifecycle codec has no native lifecycle record");
        std::uint32_t classes{};
        if (version>=ClassDefaultsVersion) {
            classes=U32();
            if ((version==ClassDefaultsVersion && classes==0u) || classes>limits_.maxObjects-count || classes>Remaining()/9u)
                Fail("class-default count is empty or exceeds aggregate budget/payload");
            budget_.Array(classes,sizeof(ScriptSavedClassDefaults)+sizeof(void*));
            if constexpr(Materialize) state.classDefaults.reserve(classes);
            if (version>=BirthManifestVersion) {
                budget_.Array(classes,sizeof(std::string_view)); classIdentities.reserve(classes);
            }
            previous={};
            for (std::uint32_t i=0; i<classes; ++i) {
                ScriptSavedClassDefaults defaults; const auto cls=String(true);
                if (i!=0 && Compare(previous,cls)>=0) Fail("class-default identities are not canonical and unique");
                previous=cls;
                if (version>=BirthManifestVersion) classIdentities.push_back(cls);
                if constexpr(Materialize) defaults.classPath=cls;
                Properties(defaults.properties,true);
                if constexpr(Materialize) state.classDefaults.push_back(std::move(defaults));
            }
        }
        if (version>=BirthManifestVersion) {
            const auto births=U32();
            if ((version==BirthManifestVersion && births==0u) || births>limits_.maxObjects || births>Remaining()/18u)
                Fail("birth count is empty or exceeds budget/payload");
            budget_.Array(births,sizeof(ScriptSavedBirth)+sizeof(void*));
            budget_.Array(births,sizeof(std::uint32_t));
            std::vector<std::uint32_t> indices; indices.reserve(births);
            if constexpr(Materialize) state.births.reserve(births);
            previous={}; std::size_t identities=static_cast<std::size_t>(count)+classes;
            for (std::uint32_t i=0u; i<births; ++i) {
                ScriptSavedBirth birth; const auto path=String(true),cls=String(true); BirthPath(path,map);
                const auto worldActorIndex=U32(); indices.push_back(worldActorIndex);
                if (i!=0u && Compare(previous,path)>=0) Fail("birth identities are not canonical and unique");
                previous=path;
                BirthClassAlias(path,classIdentities,[](const auto value) { return value; });
                if (!BirthOverlay(path,cls,objectIdentities,
                    [](const auto& object) { return object.path; },[](const auto& object) { return object.classPath; }) &&
                    ++identities>limits_.maxObjects)
                    Fail("aggregate authored actor/birth/class-default identity budget exceeded");
                if constexpr(Materialize) { birth.path=path; birth.classPath=cls; birth.worldActorIndex=worldActorIndex; }
                Properties(birth.frozenDefaults,true,true);
                if constexpr(Materialize) state.births.push_back(std::move(birth));
            }
            BirthIndices(indices);
        }
        if (version>=AiManagerVersion) {
            const auto managers=U32(); budget_.AiManagers(managers);
            if (managers==0u || managers>Remaining()/35u)
                Fail("AI manager count is empty or exceeds encoded payload");
            budget_.Array(managers,sizeof(void*));
            if constexpr(Materialize) state.aiManagers.reserve(managers);
            previous={};
            for (std::uint32_t i=0u; i<managers; ++i) {
                // One bounded native graph is materialized in the verifier so
                // the shared ring/link validator runs in BOTH passes. It is
                // discarded immediately, without copying actor/value trees.
                auto manager=AiManager();
                if (i!=0u && Compare(previous,manager.ownerPath)>=0)
                    Fail("AI manager owners are not canonical and unique");
                // Keep the previous identity as a view of the encoded bytes,
                // not a reference to this temporary manager's owned string.
                previous=lastAiOwner_;
                if constexpr(Materialize) state.aiManagers.push_back(std::move(manager));
            }
        }
        if (Remaining()!=0u) Fail("trailing encoded-state bytes");
        return state;
    }
private:
    const std::vector<std::uint8_t>& bytes_;
    const ScriptStateLimits& limits_;
    Budget budget_;
    std::size_t cursor_{};
    std::size_t aiWork_{};
    std::string_view lastAiOwner_;
    std::size_t Remaining() const { return bytes_.size()-cursor_; }
    std::uint8_t Byte() { if (Remaining()==0u) Fail("truncated encoded-state payload"); return bytes_[cursor_++]; }
    std::uint32_t U32() { std::uint32_t value{}; for (unsigned i=0; i<4u; ++i) value|=std::uint32_t(Byte())<<(8u*i); return value; }
    std::uint64_t U64() { std::uint64_t value{}; for (unsigned i=0; i<8u; ++i) value|=std::uint64_t(Byte())<<(8u*i); return value; }
    bool Boolean() { const auto value=Byte(); if (value>1u) Fail("invalid boolean encoding"); return value!=0u; }
    float F32() { const auto bits=U32(); float value; std::memcpy(&value,&bits,4u); Finite(value); return value; }
    std::int32_t I32() { const auto bits=U32(); std::int32_t value; std::memcpy(&value,&bits,4u); return value; }
    std::string_view String(bool identity,bool empty=false,std::size_t cap=0) {
        const auto size=U32(); if (size>Remaining()) Fail("truncated encoded-state string");
        const std::string_view value(reinterpret_cast<const char*>(bytes_.data()+cursor_),size);
        Text(value,limits_,identity,empty,cap); budget_.Retain(static_cast<std::size_t>(size)+1u);
        cursor_+=size; return value;
    }
    Ai::Channels AiChannels() {
        return {F32(),F32(),F32(),F32()};
    }
    Ai::State AiManager() {
        const auto retainedStart=budget_.retained;
        const auto bounded=[&]() {
            if (budget_.retained-retainedStart>Ai::MaxStateBytes)
                Fail("AI manager retained-state budget exceeded");
        };
        const auto text=[&](const bool empty=false) {
            const auto value=String(true,empty,Ai::MaxStringBytes); bounded(); return value;
        };
        const auto array=[&](const std::size_t count,const std::size_t size) {
            budget_.Array(count,size); bounded();
        };
        Ai::State state; budget_.Retain(sizeof(Ai::State));
        const auto owner=text(),level=text();
        lastAiOwner_=owner; state.ownerPath=owner; state.levelPath=level;
        state.processDepth=U32(); state.pendingDeleteCount=U32();
        state.historyCursor=Byte(); state.receiverHead=U32();
        budget_.AiLinks(2u+(state.receiverHead ? 1u : 0u));
        const auto events=U32(); budget_.AiEventTypes(events);
        if (events>Ai::MaxEventTypes || events>Remaining()/13u) Fail("AI event-type count exceeds native cap or encoded payload");
        array(events,sizeof(Ai::EventType)); state.eventTypes.reserve(events);
        const auto ids=[&]() {
            const auto count=U32(); budget_.AiLinks(count);
            if (count>Ai::MaxNodes || count>Remaining()/4u) Fail("AI node-link count exceeds native cap or encoded payload");
            array(count,sizeof(Ai::Id)); std::vector<Ai::Id> values; values.reserve(count);
            for (std::uint32_t i=0u; i<count; ++i) values.push_back(U32());
            return values;
        };
        for (std::uint32_t i=0u; i<events; ++i) {
            Ai::EventType event; event.name=text();
            event.senderIds=ids(); event.receiverIds=ids(); state.eventTypes.push_back(std::move(event));
        }
        const auto senders=U32(); budget_.AiNodes(senders);
        if (senders>Ai::MaxNodes || senders>Remaining()/286u) Fail("AI sender count exceeds native cap or encoded payload");
        array(senders,sizeof(Ai::Sender)); state.senders.reserve(senders);
        for (std::uint32_t i=0u; i<senders; ++i) {
            budget_.AiLinks(2u); Ai::Sender sender;
            sender.actor=text(); sender.eventType=U32(); sender.deleted=Boolean(); sender.score=F32();
            for (auto& channels : sender.history) channels=AiChannels();
            sender.current=AiChannels(); state.senders.push_back(std::move(sender));
        }
        const auto receivers=U32(); budget_.AiNodes(receivers);
        if (receivers>Ai::MaxNodes-senders || receivers>Remaining()/62u) Fail("AI receiver count exceeds native cap or encoded payload");
        array(receivers,sizeof(Ai::Receiver)); state.receivers.reserve(receivers);
        for (std::uint32_t i=0u; i<receivers; ++i) {
            Ai::Receiver receiver; receiver.actor=text(); receiver.eventType=U32(); receiver.deleted=Boolean();
            receiver.callback=text(true); receiver.scoreCallback=text(true);
            receiver.flags={Boolean(),Boolean(),Boolean(),Boolean()};
            receiver.callbackPending=Boolean(); receiver.eventState=Byte(); receiver.detected=Boolean();
            receiver.previousScore=F32(); receiver.previousBestActor=text(true); receiver.historyCursor=Byte();
            receiver.params.bestActor=text(true); receiver.params.score=F32(); receiver.params.visibility=F32();
            receiver.params.volume=F32(); receiver.params.smell=F32(); receiver.ringNext=U32(); receiver.ringPrev=U32();
            budget_.AiLinks(4u+(!receiver.previousBestActor.empty() ? 1u : 0u)+(!receiver.params.bestActor.empty() ? 1u : 0u));
            state.receivers.push_back(std::move(receiver));
        }
        Ai::Validate(state,aiWork_);
        return state;
    }
    ScriptSavedActorLifecycle ActorLifecycle() {
        ScriptSavedActorLifecycle lifecycle;
        lifecycle.worldRemoved=Boolean();
        for (auto& sent : lifecycle.touchEventSent) sent=Boolean();
        const auto links=[&](std::vector<std::string>& values) {
            const auto count=U32(); budget_.ActorLinks(count);
            if (count>Remaining()/5u) Fail("native actor-link count exceeds encoded payload");
            budget_.Array(count,sizeof(std::string));
            if constexpr(Materialize) values.reserve(count);
            for (std::uint32_t i=0u; i<count; ++i) {
                const auto path=String(true);
                if constexpr(Materialize) values.emplace_back(path);
            }
        };
        links(lifecycle.children); links(lifecycle.basedActors);
        return lifecycle;
    }
    void Properties(std::vector<ScriptSavedProperty>& values,const bool defaults,const bool allowEmpty=false) {
        const auto count=U32(); budget_.Properties(count);
        if (defaults && !allowEmpty && count==0u) Fail("empty class-default record");
        if (count>Remaining()/13u) Fail("property count exceeds encoded payload");
        budget_.Array(count,sizeof(ScriptSavedProperty)+sizeof(void*));
        if constexpr(Materialize) values.reserve(count);
        std::vector<PropertyAlias> aliases;
        if (defaults) { budget_.Array(count,sizeof(PropertyAlias)); aliases.reserve(count); }
        std::string_view previousKey,previousName; std::uint32_t previousIndex{};
        for (std::uint32_t j=0; j<count; ++j) {
            ScriptSavedProperty property;
            const auto key=String(true),name=String(true); const auto index=U32();
            if (j!=0) {
                const auto order=Compare(previousKey,key);
                if (order>0 || (order==0 && (previousKey!=key || previousIndex>=index || previousName!=name)))
                    Fail("property identities are not canonical and unique");
            }
            previousKey=key; previousIndex=index; previousName=name;
            if (defaults) aliases.emplace_back(name,index);
            auto value=Value(0u);
            if constexpr(Materialize) {
                property.key=key; property.name=name; property.index=index; property.value=std::move(value);
                values.push_back(std::move(property));
            }
        }
        if (defaults) ValidatePropertyAliases(aliases);
    }
    Vm::Value Value(std::size_t depth) {
        budget_.Node(depth); Vm::Value value;
        const auto tag=static_cast<Tag>(Byte());
        switch (tag) {
        case Tag::Nothing: value.kind=Vm::Kind::Nothing; break;
        case Tag::Byte: value.kind=Vm::Kind::Byte; value.integer=Byte(); break;
        case Tag::Int: value.kind=Vm::Kind::Int; value.integer=I32(); break;
        case Tag::Bool: value.kind=Vm::Kind::Bool; value.boolean=Boolean(); break;
        case Tag::Float: value.kind=Vm::Kind::Float; value.floating=F32(); break;
        case Tag::Name: case Tag::Object: case Tag::String: {
            value.kind=tag==Tag::Name ? Vm::Kind::Name : tag==Tag::Object ? Vm::Kind::Object : Vm::Kind::String;
            const auto text=String(tag!=Tag::String,true); if constexpr(Materialize) value.text=text;
            break;
        }
        case Tag::Vector: value.kind=Vm::Kind::Vector; for (auto& v : value.vector) v=F32(); break;
        case Tag::Rotator: value.kind=Vm::Kind::Rotator; for (auto& v : value.rotation) v=I32(); break;
        case Tag::Struct: {
            value.kind=Vm::Kind::Struct; const auto count=U32();
            if (count>limits_.totalValueNodes-budget_.nodes || count>Remaining()/6u)
                Fail("struct field count exceeds budget or encoded payload");
            budget_.Array(count,sizeof(Vm::Value)+sizeof(std::string)+5u*sizeof(void*));
            std::string_view previous;
            for (std::uint32_t i=0; i<count; ++i) {
                const auto field=String(true);
                if (i!=0 && Compare(previous,field)>=0) Fail("struct fields are not canonical and unique");
                previous=field; auto member=Value(depth+1u);
                if constexpr(Materialize) value.fields.emplace(std::string(field),std::move(member));
            }
            break;
        }
        default: Fail("unrecognized serialized value tag");
        }
        return value;
    }
    MeshAnimationChannel Channel() {
        MeshAnimationChannel channel; const auto sequence=String(true,true);
        if constexpr(Materialize) channel.sequence=sequence;
        channel.normalizedFrame=F32(); channel.previous.vertexOffset0=U32(); channel.previous.vertexOffset1=U32();
        channel.previous.fraction=F32(); return channel;
    }
    void Rates(float& rate,float& last,float& min,float& tween,float& old) {
        rate=F32(); last=F32(); min=F32(); tween=F32(); old=F32();
    }
    ActorAnimationClock Animation() {
        ActorAnimationClock clock; clock.pose.main=Channel();
        Rates(clock.main.rate,clock.main.last,clock.main.minRate,clock.main.tweenRate,clock.main.oldRate);
        clock.main.loop=Boolean(); clock.main.notify=Boolean(); clock.main.finished=Boolean(); clock.main.finishAnimWaiting=Boolean();
        for (std::size_t i=0; i<clock.blends.size(); ++i) {
            clock.pose.blends[i]=Channel(); auto& blend=clock.blends[i];
            Rates(blend.rate,blend.last,blend.minRate,blend.tweenRate,blend.oldRate);
            for (auto& value : blend.simulated) value=F32();
        }
        const auto bits=U64(); std::memcpy(&clock.simulationTime,&bits,8u);
        clock.remoteRole=Byte(); clock.pose.fatness=Byte(); Clock(clock,limits_); return clock;
    }
    StateObject ObjectState() {
        StateObject state; state.hasStack=Boolean();
        state.frameOverride=Boolean();
        if (Boolean()) {
            if (!state.frameOverride) Fail("state frame requires a portable frame override");
            StateFrame frame;
            const auto code=String(true,true),localsCode=String(true,true);
            if constexpr(Materialize) { frame.codePath=code; frame.localsCodePath=localsCode; }
            frame.statementIndex=U32(); const auto latent=Byte();
            if (latent>static_cast<std::uint8_t>(StateLatent::WaitForLanding)) Fail("unrecognized state latent action");
            frame.latent=static_cast<StateLatent>(latent);
            const auto count=U32(); budget_.StateLocals(count);
            if (count>Remaining()/9u) Fail("state-local count exceeds encoded payload");
            budget_.Array(count,sizeof(StateLocal)+sizeof(void*));
            if constexpr(Materialize) frame.locals.reserve(count);
            std::string_view previous;
            for (std::uint32_t i=0; i<count; ++i) {
                StateLocal local; const auto key=String(true);
                if (i!=0 && Compare(previous,key)>=0) Fail("state-local identities are not canonical and unique");
                previous=key; const auto elements=U32(); budget_.LocalElements(elements);
                if (elements>Remaining()) Fail("state-local element count exceeds encoded payload");
                budget_.Array(elements,sizeof(Vm::Value));
                if constexpr(Materialize) { local.key=key; local.values.reserve(elements); }
                for (std::uint32_t j=0; j<elements; ++j) {
                    auto value=Value(0u); if constexpr(Materialize) local.values.push_back(std::move(value));
                }
                if constexpr(Materialize) frame.locals.push_back(std::move(local));
            }
            if constexpr(Materialize) state.frame=std::move(frame);
        }
        const auto count=U32(); budget_.DisabledStates(count);
        if (count>Remaining()/9u) Fail("disabled-state count exceeds encoded payload");
        using Disabled=std::pair<const std::string,std::set<std::string>>;
        budget_.Array(count,sizeof(Disabled)+5u*sizeof(void*));
        std::string_view previous;
        for (std::uint32_t i=0; i<count; ++i) {
            const auto name=String(true);
            if (i!=0 && Compare(previous,name)>=0) Fail("disabled-state identities are not canonical and unique");
            previous=name; const auto names=U32(); budget_.DisabledNames(names);
            if (names>Remaining()/5u) Fail("disabled-name count exceeds encoded payload");
            budget_.Array(names,sizeof(std::string)+5u*sizeof(void*));
            std::set<std::string> disabled; std::string_view previousName;
            for (std::uint32_t j=0; j<names; ++j) {
                const auto item=String(true);
                if (j!=0 && Compare(previousName,item)>=0) Fail("disabled-name identities are not canonical and unique");
                previousName=item;
                if constexpr(Materialize) disabled.emplace(item);
            }
            if constexpr(Materialize) state.disabled.emplace(std::string(name),std::move(disabled));
        }
        return state;
    }
};
} // namespace ScriptStateDetail

inline std::vector<std::uint8_t> EncodeScriptSavedState(
    const ScriptSavedState& state,const ScriptStateLimits& limits={}) {
    // Validate/count the whole tree BEFORE reserving output. Sorting uses only
    // bounded pointer arrays, not copies of nested values or game resources.
    ScriptStateDetail::Writer measured(limits,nullptr); measured.State(state);
    std::vector<std::uint8_t> bytes; bytes.reserve(measured.size());
    ScriptStateDetail::Writer emitted(limits,&bytes); emitted.State(state); return bytes;
}
inline ScriptSavedState DecodeScriptSavedState(
    const std::vector<std::uint8_t>& bytes,const ScriptStateLimits& limits={}) {
    // First pass does not materialize actor/property/field trees. CDO/birth
    // aliases use budgeted input views/slot arrays. Native AI consistency needs
    // the shared graph validator, so one fully budgeted manager at a time is
    // materialized and discarded during verification before full publication.
    ScriptStateDetail::Reader<false> verified(bytes,limits); verified.State();
    ScriptStateDetail::Reader<true> materialized(bytes,limits); return materialized.State();
}
} // namespace QuestVr
