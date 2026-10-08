#pragma once

#include "quest_actor_animation_clock.h"
#include "quest_portable_vm.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>

// A structural, versioned codec, not file I/O or a script-state scheduler.
// The runtime must validate map/class/property identities and typed class
// schemas transactionally before applying decoded state. No packaged game
// assets, VM bytecode, guessed actor startup, or frame-clock debt are stored.
namespace QuestVr {

struct ScriptSavedProperty {
    std::string key, name;
    std::uint32_t index{};
    Vm::Value value;
};
struct ScriptSavedObject {
    std::string path, classPath;
    std::vector<ScriptSavedProperty> properties;
    std::optional<ActorAnimationClock> clock;
};
struct ScriptSavedState {
    std::string mapName;
    std::vector<ScriptSavedObject> objects;
};
struct ScriptStateLimits {
    std::size_t maxBytes{32u << 20u};
    std::size_t maxObjects{4096u}, maxProperties{65'536u};
    std::size_t totalValueNodes{262'144u}, maxStringBytes{8192u}, maxDepth{32u};
};

namespace ScriptStateDetail {
static_assert(sizeof(float)==4u && sizeof(double)==8u &&
    std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,
    "Script state codec requires IEEE binary32/binary64 floats");
inline constexpr std::array<std::uint8_t, 8> Magic{{'D','X','Q','V','M','S',1,0}};
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
    std::size_t retained{}, properties{}, nodes{};
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

class Writer {
public:
    Writer(const ScriptStateLimits& limits, std::vector<std::uint8_t>* output)
        : limits_(limits),budget_{limits},output_(output) {}
    std::size_t size() const { return size_; }
    void State(const ScriptSavedState& state) {
        budget_.Retain(sizeof(ScriptSavedState));
        for (const auto byte : Magic) Byte(byte);
        String(state.mapName,true,false,128u);
        if (state.objects.size()>limits_.maxObjects) Fail("object count exceeds the budget");
        budget_.Array(state.objects.size(),sizeof(ScriptSavedObject)+sizeof(void*));
        Count(state.objects.size());
        for (const auto& object : state.objects) {
            Text(object.path,limits_,true); Text(object.classPath,limits_,true);
        }
        const auto objects=Sorted(state.objects,[](const ScriptSavedObject& value) -> const std::string& { return value.path; });
        for (std::size_t i=0; i<objects.size(); ++i) {
            const auto& object=*objects[i];
            if (i!=0 && Compare(objects[i-1]->path,object.path)==0) Fail("duplicate or case-colliding object path");
            String(object.path,true); String(object.classPath,true);
            budget_.Properties(object.properties.size());
            budget_.Array(object.properties.size(),sizeof(ScriptSavedProperty)+sizeof(void*));
            Count(object.properties.size());
            std::vector<const ScriptSavedProperty*> properties; properties.reserve(object.properties.size());
            for (const auto& property : object.properties) {
                Text(property.key,limits_,true); Text(property.name,limits_,true);
                properties.push_back(&property);
            }
            std::sort(properties.begin(),properties.end(),[](const auto* a,const auto* b) {
                const auto order=Compare(a->key,b->key);
                return order!=0 ? order<0 : a->index<b->index;
            });
            for (std::size_t j=0; j<properties.size(); ++j) {
                const auto& property=*properties[j];
                if (j!=0 && Compare(properties[j-1]->key,property.key)==0) {
                    if (properties[j-1]->key!=property.key || properties[j-1]->index==property.index ||
                        properties[j-1]->name!=property.name)
                        Fail("duplicate or case-colliding property identity");
                }
                String(property.key,true); String(property.name,true); U32(property.index);
                Value(property.value,0u);
            }
            Byte(object.clock ? 1u : 0u);
            if (object.clock) Animation(*object.clock);
        }
    }
private:
    const ScriptStateLimits& limits_;
    Budget budget_;
    std::vector<std::uint8_t>* output_;
    std::size_t size_{};
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
        for (const auto magic : Magic) if (Byte()!=magic) Fail("bad magic or unsupported codec version");
        const auto map=String(true,false,128u); if constexpr(Materialize) state.mapName=map;
        const auto count=U32();
        if (count>limits_.maxObjects || count>Remaining()/13u) Fail("object count exceeds budget or encoded payload");
        budget_.Array(count,sizeof(ScriptSavedObject)+sizeof(void*));
        if constexpr(Materialize) state.objects.reserve(count);
        std::string_view previous;
        for (std::uint32_t i=0; i<count; ++i) {
            ScriptSavedObject object;
            const auto path=String(true),cls=String(true);
            if (i!=0 && Compare(previous,path)>=0) Fail("object paths are not canonical and unique");
            previous=path;
            if constexpr(Materialize) { object.path=path; object.classPath=cls; }
            const auto properties=U32(); budget_.Properties(properties);
            if (properties>Remaining()/13u) Fail("property count exceeds encoded payload");
            budget_.Array(properties,sizeof(ScriptSavedProperty)+sizeof(void*));
            if constexpr(Materialize) object.properties.reserve(properties);
            std::string_view previousKey,previousName; std::uint32_t previousIndex{};
            for (std::uint32_t j=0; j<properties; ++j) {
                ScriptSavedProperty property;
                const auto key=String(true),name=String(true); const auto index=U32();
                if (j!=0) {
                    const auto order=Compare(previousKey,key);
                    if (order>0 || (order==0 && (previousKey!=key || previousIndex>=index || previousName!=name)))
                        Fail("property identities are not canonical and unique");
                }
                previousKey=key; previousIndex=index; previousName=name;
                auto value=Value(0u);
                if constexpr(Materialize) {
                    property.key=key; property.name=name; property.index=index; property.value=std::move(value);
                    object.properties.push_back(std::move(property));
                }
            }
            if (Boolean()) { auto clock=Animation(); if constexpr(Materialize) object.clock=std::move(clock); }
            if constexpr(Materialize) state.objects.push_back(std::move(object));
        }
        if (Remaining()!=0u) Fail("trailing encoded-state bytes");
        return state;
    }
private:
    const std::vector<std::uint8_t>& bytes_;
    const ScriptStateLimits& limits_;
    Budget budget_;
    std::size_t cursor_{};
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
    // First pass is non-materializing: strings are views into the input, and no
    // object/property/field arrays are allocated until aggregate budgets and
    // canonical uniqueness have been checked for the complete payload.
    ScriptStateDetail::Reader<false> verified(bytes,limits); verified.State();
    ScriptStateDetail::Reader<true> materialized(bytes,limits); return materialized.State();
}
} // namespace QuestVr
