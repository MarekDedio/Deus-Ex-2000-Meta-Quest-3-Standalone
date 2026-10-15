#pragma once

#include "quest_actor_geometry.h"

#include <map>
#include <bit>

namespace QuestVr {

// Renderer-owned original vertex-animation clips. This is a temporary visual
// waiting bootstrap, not a running original AI/state/notify scheduler. Once
// world animation is connected, its current sequence/frame can use this same
// GPU topology/interpolation path instead of rebuilding the actor scene.
struct NpcAnimationClip {
    std::string meshPath, sequence;
    std::uint32_t vertices{}, frames{};
    float normalizedRate{};
    bool loop{};
    std::vector<float> positionsRgba; // [frame][original source vertex][xyzw].
};

struct NpcWaitingInstance {
    std::string actorPath, meshPath, function;
    ActorVec3 prePivot;
    std::shared_ptr<const NpcAnimationClip> clip;
    PortableActorAnimationSnapshot originalAnimation;
    ActorVec3 originalPrePivot;
};

inline bool NpcWaitingMatchesSnapshot(const NpcWaitingInstance& waiting,const PortableActorSnapshot& actor) {
    const auto& a=waiting.originalAnimation;
    const auto& b=actor.animation;
    if (waiting.actorPath!=actor.objectPath || waiting.meshPath!=actor.meshPath || actor.fatness!=128u ||
        actor.animByOwner || !actor.pawn || actor.hidden || actor.drawType!=2u ||
        waiting.originalPrePivot.x!=actor.prePivotX || waiting.originalPrePivot.y!=actor.prePivotY ||
        waiting.originalPrePivot.z!=actor.prePivotZ || a.sequence!=b.sequence || a.frame!=b.frame ||
        a.rate!=b.rate || a.last!=b.last || a.minRate!=b.minRate || a.tweenRate!=b.tweenRate ||
        a.oldRate!=b.oldRate || a.loop!=b.loop || a.notify!=b.notify || a.finished!=b.finished ||
        a.previous.vertexOffset0!=b.previous.vertexOffset0 || a.previous.vertexOffset1!=b.previous.vertexOffset1 ||
        a.previous.fraction!=b.previous.fraction)
        return false;
    for (std::size_t i=0u;i<a.blends.size();++i)
        if (a.blends[i].sequence!=b.blends[i].sequence || a.blends[i].frame!=b.blends[i].frame ||
            a.blends[i].rate!=b.blends[i].rate || a.blends[i].tweenRate!=b.blends[i].tweenRate ||
            a.blends[i].last!=b.blends[i].last || a.blends[i].minRate!=b.blends[i].minRate ||
            a.blends[i].oldRate!=b.blends[i].oldRate || a.blends[i].loop!=b.blends[i].loop ||
            a.blends[i].previous.vertexOffset0!=b.blends[i].previous.vertexOffset0 ||
            a.blends[i].previous.vertexOffset1!=b.blends[i].previous.vertexOffset1 ||
            a.blends[i].previous.fraction!=b.blends[i].previous.fraction)
            return false;
    return true;
}

struct NpcAnimationFrame {
    std::uint32_t first{}, second{};
    float fraction{};
};

inline bool NeedsNpcWaitingBootstrap(const PortableLodMesh& mesh,const PortableActorSnapshot& actor) {
    if (!mesh.animation) return false;
    const auto* authored=FindMeshAnimationSequence(*mesh.animation,actor.animation.sequence,false);
    // Preserve actual map/script poses (including sitting, lying and saved
    // tweens). Only the unresolved/unusable reference-pose fallback needs the
    // temporary waiting bootstrap; it must not stand up an authored sitter.
    return !authored || !HasUsableMeshAnimationSpan(*mesh.animation,*authored);
}

inline NpcAnimationFrame SampleNpcAnimationFrame(const NpcAnimationClip& clip, double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0 || !std::isfinite(clip.normalizedRate) ||
        clip.normalizedRate < 0.0f || clip.frames == 0u || clip.frames > 256u)
        throw std::runtime_error("NPC presentation clock is invalid");
    const double frame = seconds * static_cast<double>(clip.normalizedRate) * clip.frames;
    if (!std::isfinite(frame)) throw std::runtime_error("NPC presentation clock overflowed");
    const double sampled = clip.loop ? std::fmod(frame,static_cast<double>(clip.frames)) :
        std::min(frame,static_cast<double>(clip.frames-1u));
    const auto first = static_cast<std::uint32_t>(sampled);
    return {first,clip.loop ? (first+1u)%clip.frames : std::min(first+1u,clip.frames-1u),
        static_cast<float>(sampled-first)};
}

inline NpcAnimationClip BuildNpcAnimationClip(const PortableLodMesh& mesh,
    const std::string& meshPath, const PortableActorWaitingPresentation& waiting,
    std::uint8_t fatness) {
    if (!waiting.available || !mesh.animation || meshPath != waiting.meshPath || fatness != 128u)
        throw std::runtime_error("NPC waiting presentation lacks an exact neutral-fatness mesh binding");
    const auto& data = *mesh.animation;
    static_cast<void>(ValidateMeshAnimationData(data));
    const auto* sequence = FindMeshAnimationSequence(data,waiting.animation.sequence,false);
    if (!sequence || !HasUsableMeshAnimationSpan(data,*sequence) || sequence->numFrames > 256 ||
        data.frameVertices > 4096u || data.triangleSourceVertexIndices.size() != mesh.triangles.size())
        throw std::runtime_error("NPC waiting presentation has no bounded authored animation span");
    for (const auto& blend : waiting.animation.blends)
        if (!MeshAnimationNamesEqual(blend.sequence,"None"))
            throw std::runtime_error("NPC waiting presentation requires unsupported additive channels");
    if (!IsFiniteActorVector(waiting.prePivot) || !std::isfinite(waiting.animation.rate) ||
        waiting.animation.rate < 0.0f)
        throw std::runtime_error("NPC waiting presentation has invalid transform/rate");
    // Never suppress gameplay notifies under the guise of visual playback.
    // Their event-bearing sequences stay on the ordinary snapshot path until
    // the real scheduler can own and deliver their callbacks.
    if (!sequence->notifies.empty())
        throw std::runtime_error("NPC waiting presentation contains gameplay notifies");
    NpcAnimationClip clip;
    clip.meshPath = meshPath; clip.sequence = sequence->name;
    clip.vertices = data.frameVertices; clip.frames = static_cast<std::uint32_t>(sequence->numFrames);
    clip.normalizedRate = waiting.animation.rate; clip.loop = waiting.animation.loop;
    const std::size_t floats = static_cast<std::size_t>(clip.vertices)*clip.frames*4u;
    if (floats > 4u*1024u*1024u)
        throw std::runtime_error("NPC waiting clip exceeds16MiB");
    clip.positionsRgba.reserve(floats);
    for (std::uint32_t frame=0u; frame<clip.frames; ++frame) {
        MeshAnimationState state;
        state.main.sequence = clip.sequence;
        state.main.normalizedFrame = static_cast<float>(frame)/clip.frames;
        state.fatness = fatness;
        const auto pose = PrepareMeshPose(data,state);
        if (!pose.drawable || pose.fallbackUsed || pose.objectPositions.size() != clip.vertices)
            throw std::runtime_error("Original NPC waiting frame is not drawable: "+pose.error);
        for (const auto& p : pose.objectPositions) {
            if (!IsFiniteActorVector(p)) throw std::runtime_error("NPC waiting frame is non-finite");
            clip.positionsRgba.insert(clip.positionsRgba.end(),{p.x,p.y,p.z,1.0f});
        }
    }
    return clip;
}

// Call only on the exclusive CPU/runtime owner during map preparation, never
// the XR/render loop. Original scripts choose all sequence names and rates.
inline std::map<std::string,NpcWaitingInstance> PrepareNpcWaitingPresentations(
    const std::vector<PortableActorSnapshot>& actors, std::vector<std::string>* errors = nullptr) {
    std::map<std::string,NpcWaitingInstance> result;
    std::map<std::string,std::shared_ptr<const NpcAnimationClip>> cache;
    std::size_t bytes{};
    for (const auto& actor : actors) {
        if (!actor.pawn || actor.meshPath.empty() || actor.hidden || actor.drawType != 2u || actor.animByOwner)
            continue;
        try {
            const auto& mesh=GetPortableRuntimeMeshView(actor.meshPath);
            if (!NeedsNpcWaitingBootstrap(mesh,actor)) continue;
            const auto waiting = PreviewPortableActorWaitingPresentation(actor.objectPath);
            if (!waiting.available) throw std::runtime_error(waiting.error);
            const auto key = waiting.meshPath+'|'+waiting.animation.sequence+'|'+
                std::to_string(std::bit_cast<std::uint32_t>(waiting.animation.rate))+'|'+std::to_string(waiting.animation.loop)+'|'+
                std::to_string(actor.fatness);
            auto found = cache.find(key);
            if (found == cache.end()) {
                auto clip = std::make_shared<const NpcAnimationClip>(BuildNpcAnimationClip(
                    mesh,actor.meshPath,waiting,actor.fatness));
                const auto additional = clip->positionsRgba.size()*sizeof(float);
                if (additional > 32u*1024u*1024u-bytes || cache.size() >= 64u)
                    throw std::runtime_error("NPC waiting catalog exceeds preparation budget");
                bytes += additional; found = cache.emplace(key,std::move(clip)).first;
            }
            if (result.size() >= 256u) throw std::runtime_error("NPC waiting actor count exceeds budget");
            result.emplace(actor.objectPath,NpcWaitingInstance{actor.objectPath,actor.meshPath,
                waiting.function,waiting.prePivot,found->second,actor.animation,
                {actor.prePivotX,actor.prePivotY,actor.prePivotZ}});
        } catch (const std::exception& error) {
            // Unsupported actors keep the explicit original snapshot path.
            // No guessed sequence, cube replacement or persistent mutation.
            if (errors && errors->size()<256u) errors->push_back(actor.objectPath+": "+std::string(error.what()).substr(0u,512u));
        }
    }
    return result;
}

} // namespace QuestVr
