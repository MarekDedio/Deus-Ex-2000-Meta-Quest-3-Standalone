#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace QuestVr {

// CPU-host-owned, nonserialized native raw-pose visibility. This is NOT a
// committed Actor movement or hash membership update. A host must authenticate
// the actual actor and its exact declared Location/Rotation before entering.
// Fixed owned slots avoid allocation and dangling links on scope restoration.
class NativeActorPoseStack {
public:
    struct Pose {
        std::array<float,3u> location{};
        std::array<std::int32_t,3u> rotation{};
    };
    static constexpr std::size_t MaximumDepth=64u;
private:
    struct Entry {std::uintptr_t actor{};Pose pose;bool active{};};
    std::array<Entry,MaximumDepth> entries_{};
    std::size_t used_{};
    void Release(std::size_t index) noexcept {
        entries_[index].active=false;
        while(used_&&!entries_[used_-1u].active)--used_;
    }
public:
    NativeActorPoseStack()=default;
    NativeActorPoseStack(const NativeActorPoseStack&)=delete;
    NativeActorPoseStack& operator=(const NativeActorPoseStack&)=delete;
    class Scope {
        NativeActorPoseStack& owner_;
        std::size_t index_;
    public:
        Scope(NativeActorPoseStack& owner,std::uintptr_t actor,const Pose& pose):owner_(owner),index_(owner.used_) {
            if(!actor||owner.used_>=MaximumDepth)
                throw std::runtime_error("Native actor pose identity/depth is outside its fixed scope domain");
            owner.entries_[index_]={actor,pose,true};++owner.used_;
        }
        ~Scope() noexcept {owner_.Release(index_);}
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
        Scope(Scope&&)=delete;
        Scope& operator=(Scope&&)=delete;
    };
    const Pose* Find(std::uintptr_t actor) const noexcept {
        for(auto i=used_;i;--i)if(entries_[i-1u].active&&entries_[i-1u].actor==actor)return &entries_[i-1u].pose;
        return nullptr;
    }
    std::size_t Depth() const noexcept {return used_;}
};
} // namespace QuestVr
