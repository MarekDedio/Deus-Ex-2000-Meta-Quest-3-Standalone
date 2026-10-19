#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace QuestVr {

// CPU-host-owned, nonserialized native raw-pose visibility. This is NOT a
// committed Actor movement or hash membership update. A host must authenticate
// the actual actor and its exact declared Location/Rotation before entering.
// Fixed owned slots avoid allocation and dangling links on scope restoration.
// Lower source-query scopes override BOTH fields read-only. Higher reverse-
// push scopes override ONLY Location, and writes to that temporary Location
// are discarded on release. Rotation then falls through to the live durable
// field, unless another independent full-pose scope actually overrides it.
class NativeActorPoseStack {
public:
    struct Pose {
        std::array<float,3u> location{};
        std::array<std::int32_t,3u> rotation{};
    };
    static constexpr std::size_t MaximumDepth=64u;
    // A token is authority over one generation of one host-owned Location
    // slot, not an actor pointer or a reusable index. The host/stack must
    // outlive its VM journal; scope objects themselves need not do so.
    class LocationWriteToken {
        friend class NativeActorPoseStack;
        const NativeActorPoseStack* owner_{};
        std::size_t index_{};
        std::uint64_t generation_{};
        std::uintptr_t actor_{};
        LocationWriteToken(const NativeActorPoseStack* owner,std::size_t index,
            std::uint64_t generation,std::uintptr_t actor) noexcept:
            owner_(owner),index_(index),generation_(generation),actor_(actor) {}
    public:
        LocationWriteToken()=default;
        explicit operator bool() const noexcept {return owner_!=nullptr;}
    };
private:
    static constexpr std::uint8_t LocationField=1u,RotationField=2u;
    struct Entry {
        std::uintptr_t actor{};
        Pose pose;
        std::uint64_t generation{};
        std::uint8_t fields{};
        bool active{},mutableLocation{};
    };
    struct Slot {std::size_t index;std::uint64_t generation;};
    std::array<Entry,MaximumDepth> entries_{};
    std::size_t used_{};
    std::uint64_t nextGeneration_{},maximumGeneration_;
    Slot Acquire(std::uintptr_t actor,const Pose& pose,std::uint8_t fields,bool mutableLocation) {
        if(!actor||used_>=MaximumDepth)
            throw std::runtime_error("Native actor pose identity/depth is outside its fixed scope domain");
        if(nextGeneration_>=maximumGeneration_)
            throw std::runtime_error("Native actor pose generation allowance is exhausted");
        const auto index=used_;const auto generation=nextGeneration_+1u;
        auto& entry=entries_[index];entry.actor=actor;
        // Copy object representations, not arithmetic Float loads/stores.
        // Inactive NaNs (including signaling payloads) remain raw native data.
        std::memcpy(&entry.pose,&pose,sizeof(pose));
        entry.generation=generation;entry.fields=fields;
        entry.mutableLocation=mutableLocation;entry.active=true;
        nextGeneration_=generation;++used_;
        return {index,generation};
    }
    void Release(Slot slot) noexcept {
        if(slot.index>=MaximumDepth||entries_[slot.index].generation!=slot.generation)return;
        const auto index=slot.index;
        entries_[index].active=false;
        while(used_&&!entries_[used_-1u].active)--used_;
    }
    const Entry* FindField(std::uintptr_t actor,std::uint8_t field) const noexcept {
        for(auto i=used_;i;--i) {
            const auto& entry=entries_[i-1u];
            if(entry.active&&entry.actor==actor&&(entry.fields&field))return &entry;
        }
        return nullptr;
    }
    Entry* TokenEntry(LocationWriteToken token) noexcept {
        if(token.owner_!=this||token.index_>=MaximumDepth)return nullptr;
        auto& entry=entries_[token.index_];
        return entry.active&&entry.actor==token.actor_&&entry.generation==token.generation_&&
            (entry.fields&LocationField)&&entry.mutableLocation?&entry:nullptr;
    }
    const Entry* TokenEntry(LocationWriteToken token) const noexcept {
        if(token.owner_!=this||token.index_>=MaximumDepth)return nullptr;
        const auto& entry=entries_[token.index_];
        return entry.active&&entry.actor==token.actor_&&entry.generation==token.generation_&&
            (entry.fields&LocationField)&&entry.mutableLocation?&entry:nullptr;
    }
public:
    // A caller may tighten this monotonic lifetime allowance for bounded
    // diagnostics. Releasing a slot NEVER restores/refreshes generations.
    explicit NativeActorPoseStack(std::uint64_t maximumGeneration=std::numeric_limits<std::uint64_t>::max()) noexcept:
        maximumGeneration_(maximumGeneration) {}
    NativeActorPoseStack(const NativeActorPoseStack&)=delete;
    NativeActorPoseStack& operator=(const NativeActorPoseStack&)=delete;
    class Scope {
        NativeActorPoseStack& owner_;
        Slot slot_;
    public:
        Scope(NativeActorPoseStack& owner,std::uintptr_t actor,const Pose& pose):owner_(owner),
            slot_(owner.Acquire(actor,pose,LocationField|RotationField,false)) {}
        ~Scope() noexcept {owner_.Release(slot_);}
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
        Scope(Scope&&)=delete;
        Scope& operator=(Scope&&)=delete;
    };
    class LocationScope {
        NativeActorPoseStack& owner_;
        Slot slot_;
        static Pose RawLocation(const std::array<float,3u>& location) noexcept {
            Pose pose;std::memcpy(pose.location.data(),location.data(),sizeof(location));return pose;
        }
    public:
        LocationScope(NativeActorPoseStack& owner,std::uintptr_t actor,const std::array<float,3u>& location):owner_(owner),
            slot_(owner.Acquire(actor,RawLocation(location),LocationField,true)) {}
        ~LocationScope() noexcept {owner_.Release(slot_);}
        LocationScope(const LocationScope&)=delete;
        LocationScope& operator=(const LocationScope&)=delete;
        LocationScope(LocationScope&&)=delete;
        LocationScope& operator=(LocationScope&&)=delete;
    };
    const std::array<float,3u>* FindLocation(std::uintptr_t actor) const noexcept {
        const auto* entry=FindField(actor,LocationField);return entry?&entry->pose.location:nullptr;
    }
    const std::array<std::int32_t,3u>* FindRotation(std::uintptr_t actor) const noexcept {
        const auto* entry=FindField(actor,RotationField);return entry?&entry->pose.rotation:nullptr;
    }
    // Legacy whole-pose lookup is safe ONLY for one coherent nearest full
    // query entry. A partial Location plus another/durable Rotation has no
    // truthful Pose pointer; callers must use the independent field lookups.
    const Pose* Find(std::uintptr_t actor) const noexcept {
        const auto* location=FindField(actor,LocationField);
        return location&&location==FindField(actor,RotationField)?&location->pose:nullptr;
    }
    LocationWriteToken CaptureLocationWrite(std::uintptr_t actor) const {
        if(!actor)throw std::invalid_argument("Native actor pose write requires a nonnull actual identity");
        const auto* entry=FindField(actor,LocationField);
        if(!entry)return {}; // ONLY absence permits the durable write path.
        if(!entry->mutableLocation)
            throw std::runtime_error("Native source-query Location is read-only to committed writes");
        const auto index=static_cast<std::size_t>(entry-entries_.data());
        return {this,index,entry->generation,actor};
    }
    // No current Rotation scope is writable. Never bypass a read-only query
    // view merely because a lower/durable Rotation destination also exists.
    void RequireDurableRotationWrite(std::uintptr_t actor) const {
        if(!actor)throw std::invalid_argument("Native actor pose write requires a nonnull actual identity");
        if(FindField(actor,RotationField))
            throw std::runtime_error("Native source-query Rotation is read-only to committed writes");
    }
    void ValidateLocationWrite(LocationWriteToken token) const {
        const auto* entry=TokenEntry(token);
        if(!entry||entry!=FindField(token.actor_,LocationField))
            throw std::runtime_error("Native actor pose write authority is expired, foreign or shadowed");
    }
    void WriteLocation(LocationWriteToken token,unsigned channel,float value) {
        if(channel>=3u)throw std::invalid_argument("Native actor pose Location channel is outside its raw Vector");
        ValidateLocationWrite(token);
        auto* entry=TokenEntry(token);std::memcpy(&entry->pose.location[channel],&value,sizeof(value));
    }
    // Delayed VM undo can outlive/release/reuse its original scope. It must
    // never throw, allocate, touch durable script storage or mutate an ABA
    // replacement. A still-active matching lower scope may be restored while
    // hidden: this is RAW UNDO, not a new committed native-write capability.
    // A host using this in a journal's shared write closure MUST separately
    // ValidateLocationWrite immediately before the ordinary WriteReference.
    bool UndoLocation(LocationWriteToken token,unsigned channel,float value) noexcept {
        auto* entry=TokenEntry(token);if(!entry||channel>=3u)return false;
        std::memcpy(&entry->pose.location[channel],&value,sizeof(value));return true;
    }
    std::size_t Depth() const noexcept {return used_;}
};
} // namespace QuestVr
