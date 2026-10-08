#pragma once

#include "quest_actor_transform.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Serialized animation assets stay in raw mesh space. Sharing these immutable
// arrays avoids copying an entire character's animations for every actor.
struct PortablePackedMeshVertex {
    std::int16_t x{}, y{}, z{};
};
static_assert(sizeof(PortablePackedMeshVertex) == 6u, "Packed animation vertex must be six bytes");

struct PortableMeshAnimationNotify {
    float time{};
    std::string function;
};

struct PortableMeshAnimationSequence {
    std::string name, group;
    std::int32_t startFrame{}, numFrames{};
    float rate{};
    std::vector<PortableMeshAnimationNotify> notifies;
    // Some original assets (e.g. Sword3rd) declare missing animation frames.
    // Preserve their metadata; never invent or clamp those vertex frames.
    bool invalidOriginalSpan{};
};

struct PortableMeshAnimationData {
    std::uint32_t frameVertices{}, animationFrames{};
    std::vector<PortablePackedMeshVertex> frameVerticesPacked;
    // Exact normal-building topology: direct indices, without ReMapAnimVerts.
    std::vector<std::array<std::uint32_t, 3>> normalTopology;
    // One mapped source identity per expanded render/attachment corner.
    std::vector<std::uint32_t> triangleSourceVertexIndices, specialFaceVertexIndices;
    std::vector<PortableMeshAnimationSequence> sequences;
    bool lodMesh{};
    QuestVr::ActorVec3 scale{1.0f, 1.0f, 1.0f}, origin{};
    std::int32_t rotationOriginPitch{}, rotationOriginYaw{}, rotationOriginRoll{};
};

namespace QuestVr {

struct MeshAnimationLimits {
    std::size_t maxRetainedBytes{64u * 1024u * 1024u};
    std::size_t maxSampledBytes{64u * 1024u * 1024u};
    std::size_t maxNormalTriangleSamples{8'000'000u};
    std::size_t maxFrameVertices{262'144u};
    std::size_t maxAnimationFrames{1'000'000u};
    std::size_t maxSequences{65'536u};
    std::size_t maxNotifies{1'000'000u};
    std::size_t maxSampledFrames{16u};
};

struct MeshTweenHistory {
    // UActor stores offsets into the complete vertex array, not frame numbers.
    std::uint32_t vertexOffset0{}, vertexOffset1{};
    float fraction{-1.0f};
};

struct MeshAnimationChannel {
    std::string sequence;
    float normalizedFrame{};
    MeshTweenHistory previous;
};

struct MeshAnimationState {
    MeshAnimationChannel main;
    std::array<MeshAnimationChannel, 4> blends;
    std::uint8_t fatness{128u};
};

struct MeshPose {
    // Full FrameVerts arrays, indexed by triangleSourceVertexIndices. These are
    // mesh-object coordinates, ready for BuildActorToQuest's actor transform.
    std::vector<ActorVec3> objectPositions, objectNormals;
    // Expanded attachment corners: main channel only, no fatness or blends.
    std::vector<ActorVec3> attachmentPositions;
    bool drawable{}, fallbackUsed{}, selectedOriginalSpanInvalid{};
    std::string resolvedSequence, error;
    std::size_t sampledFrames{}, normalTriangleSamples{}, retainedBytes{};
};

inline bool MeshAnimationNamesEqual(const std::string& a, const std::string& b) {
    // NameString canonicalizes both empty strings and any case of None.
    if (a.empty() || b.empty()) {
        if (a.empty() && b.empty()) return true;
        const auto& nonempty = a.empty() ? b : a;
        return nonempty.size() == 4u &&
            (nonempty[0] == 'N' || nonempty[0] == 'n') &&
            (nonempty[1] == 'O' || nonempty[1] == 'o') &&
            (nonempty[2] == 'N' || nonempty[2] == 'n') &&
            (nonempty[3] == 'E' || nonempty[3] == 'e');
    }
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0u; i < a.size(); ++i) {
        const auto fold = [](unsigned char c) {
            return c >= 'a' && c <= 'z' ? static_cast<unsigned char>(c-'a'+'A') : c;
        };
        if (fold(static_cast<unsigned char>(a[i])) != fold(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

inline const PortableMeshAnimationSequence* FindMeshAnimationSequence(
    const PortableMeshAnimationData& data, const std::string& name,
    const bool allowFirstSequenceFallback, bool* fallbackUsed = nullptr) {
    if (fallbackUsed != nullptr) *fallbackUsed = false;
    for (const auto& sequence : data.sequences)
        if (MeshAnimationNamesEqual(sequence.name, name)) return &sequence;
    if (allowFirstSequenceFallback && !data.sequences.empty()) {
        if (fallbackUsed != nullptr) *fallbackUsed = true;
        return &data.sequences.front();
    }
    return nullptr;
}

inline bool HasUsableMeshAnimationSpan(
    const PortableMeshAnimationData& data, const PortableMeshAnimationSequence& sequence) {
    return sequence.startFrame >= 0 && sequence.numFrames > 0 &&
        static_cast<std::uint64_t>(sequence.startFrame)+static_cast<std::uint32_t>(sequence.numFrames) <= data.animationFrames;
}

// Validation also accounts for retained vector/string capacities. Serialized
// rates and notify times must be finite, but their signs/ranges are not guessed.
inline std::size_t ValidateMeshAnimationData(
    const PortableMeshAnimationData& data, const MeshAnimationLimits& limits = {}) {
    if (data.frameVertices == 0u || data.frameVertices > limits.maxFrameVertices ||
        data.animationFrames == 0u || data.animationFrames > limits.maxAnimationFrames ||
        static_cast<std::uint64_t>(data.frameVertices)*data.animationFrames > data.frameVerticesPacked.size())
        throw std::runtime_error("Mesh animation dimensions are invalid or exceed the sampling budget");
    if (data.sequences.size() > limits.maxSequences ||
        data.triangleSourceVertexIndices.size()%3u != 0u || data.specialFaceVertexIndices.size()%3u != 0u ||
        !IsFiniteActorVector(data.scale) || !IsFiniteActorVector(data.origin))
        throw std::runtime_error("Mesh animation metadata is invalid");
    std::uint64_t bytes = sizeof(PortableMeshAnimationData);
    const auto addBytes = [&](const std::uint64_t amount) {
        if (amount > limits.maxRetainedBytes || bytes > limits.maxRetainedBytes-amount)
            throw std::runtime_error("Mesh animation retained-data budget exceeded");
        bytes += amount;
    };
    addBytes(static_cast<std::uint64_t>(data.frameVerticesPacked.capacity())*sizeof(PortablePackedMeshVertex));
    addBytes(static_cast<std::uint64_t>(data.normalTopology.capacity())*sizeof(std::array<std::uint32_t, 3>));
    addBytes(static_cast<std::uint64_t>(data.triangleSourceVertexIndices.capacity())*sizeof(std::uint32_t));
    addBytes(static_cast<std::uint64_t>(data.specialFaceVertexIndices.capacity())*sizeof(std::uint32_t));
    addBytes(static_cast<std::uint64_t>(data.sequences.capacity())*sizeof(PortableMeshAnimationSequence));
    std::uint64_t notifies = 0u;
    const auto addName = [&](const std::string& name) {
        if (name.size() > 65'536u) throw std::runtime_error("Mesh animation name is too long");
        addBytes(static_cast<std::uint64_t>(name.capacity())+1u);
    };
    for (const auto& sequence : data.sequences) {
        if (sequence.startFrame < 0 || sequence.numFrames <= 0 ||
            !std::isfinite(sequence.rate))
            throw std::runtime_error("Mesh animation sequence frame range or rate is invalid");
        addName(sequence.name); addName(sequence.group);
        notifies += sequence.notifies.size();
        if (notifies > limits.maxNotifies) throw std::runtime_error("Mesh animation notify budget exceeded");
        addBytes(static_cast<std::uint64_t>(sequence.notifies.capacity())*sizeof(PortableMeshAnimationNotify));
        for (const auto& notify : sequence.notifies) {
            if (!std::isfinite(notify.time)) throw std::runtime_error("Mesh animation notify time is non-finite");
            addName(notify.function);
        }
    }
    for (const auto& face : data.normalTopology)
        for (const auto index : face)
            if (index >= data.frameVertices) throw std::runtime_error("Mesh animation normal topology is out of bounds");
    for (const auto index : data.triangleSourceVertexIndices)
        if (index >= data.frameVertices) throw std::runtime_error("Mesh animation render source index is out of bounds");
    for (const auto index : data.specialFaceVertexIndices)
        if (index >= data.frameVertices) throw std::runtime_error("Mesh animation attachment source index is out of bounds");
    if (bytes > limits.maxRetainedBytes) throw std::runtime_error("Mesh animation retained-data budget exceeded");
    return static_cast<std::size_t>(bytes);
}

namespace MeshAnimationDetail {

struct FramePlan {
    std::array<std::uint32_t, 3> offsets{};
    float t0{}, t1{}, weight{1.0f};
};

inline ActorVec3 Normalize(const ActorVec3& v) {
    const float length = std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
    if (!std::isfinite(length)) throw std::runtime_error("Mesh animation normal is non-finite");
    // Math/vec.h normalize uses FLT_EPSILON, including zero/near-cancelled sums.
    if (length <= std::numeric_limits<float>::epsilon()) return {};
    return {v.x/length, v.y/length, v.z/length};
}

inline ActorVec3 Mix(const ActorVec3& a, const ActorVec3& b, const float t) {
    // Same evaluation order as the pinned Math/vec.h mix implementation.
    return {a.x*(1.0f-t)+b.x*t, a.y*(1.0f-t)+b.y*t, a.z*(1.0f-t)+b.z*t};
}

inline void ValidateHistory(const PortableMeshAnimationData& data, const MeshTweenHistory& history,
                            const bool allowInitial) {
    if (!std::isfinite(history.fraction)) throw std::runtime_error("Mesh tween history is non-finite");
    const bool initial = allowInitial && history.fraction == -1.0f &&
        history.vertexOffset0 == 0u && history.vertexOffset1 == 0u;
    if (!initial && (history.fraction < 0.0f || history.fraction >= 1.0f))
        throw std::runtime_error("Mesh tween history fraction is invalid");
    for (const auto offset : {history.vertexOffset0, history.vertexOffset1})
        if (offset%data.frameVertices != 0u || offset/data.frameVertices >= data.animationFrames)
            throw std::runtime_error("Mesh tween history frame offset is out of bounds");
}

inline FramePlan PlanChannel(const PortableMeshAnimationData& data,
                             const PortableMeshAnimationSequence& sequence,
                             const MeshAnimationChannel& channel, const bool blend) {
    if (!std::isfinite(channel.normalizedFrame) || channel.normalizedFrame >= 1.0f)
        throw std::runtime_error("Mesh animation channel frame must be finite and below one");
    const float frame = channel.normalizedFrame*static_cast<float>(sequence.numFrames);
    if (!std::isfinite(frame)) throw std::runtime_error("Mesh animation channel frame overflowed");
    const auto offset = [&](const std::uint32_t frameIndex) {
        const std::uint64_t value = static_cast<std::uint64_t>(frameIndex)*data.frameVertices;
        if (value > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("Mesh animation frame offset overflowed");
        return static_cast<std::uint32_t>(value);
    };
    FramePlan plan;
    if (frame >= 0.0f) {
        if (frame >= static_cast<float>(sequence.numFrames))
            throw std::runtime_error("Mesh animation normalized frame rounded beyond its sequence");
        const std::uint32_t frame0 = static_cast<std::uint32_t>(frame);
        const std::uint32_t frame1 = (frame0+1u)%static_cast<std::uint32_t>(sequence.numFrames);
        plan.t0 = frame-static_cast<float>(frame0);
        plan.offsets[0] = offset(static_cast<std::uint32_t>(sequence.startFrame)+frame0);
        plan.offsets[1] = offset(static_cast<std::uint32_t>(sequence.startFrame)+frame1);
    } else {
        plan.t1 = std::clamp(frame+1.0f, 0.0f, 1.0f);
        if (blend && channel.previous.fraction < 0.0f) {
            if (!std::isfinite(channel.previous.fraction))
                throw std::runtime_error("Mesh blend tween history is non-finite");
            plan.offsets.fill(offset(static_cast<std::uint32_t>(sequence.startFrame)));
            plan.weight = plan.t1;
            plan.t1 = 0.0f;
        } else {
            ValidateHistory(data, channel.previous, !blend);
            plan.t0 = channel.previous.fraction;
            plan.offsets[0] = channel.previous.vertexOffset0;
            plan.offsets[1] = channel.previous.vertexOffset1;
            plan.offsets[2] = offset(static_cast<std::uint32_t>(sequence.startFrame));
        }
    }
    return plan;
}

inline void ValidateSampledFrames(const PortableMeshAnimationData& data, const FramePlan& plan) {
    const auto validate = [&](const std::uint32_t offset) {
        if (offset%data.frameVertices != 0u || offset/data.frameVertices >= data.animationFrames ||
            static_cast<std::uint64_t>(offset)+data.frameVertices > data.frameVerticesPacked.size())
            throw std::runtime_error("Selected authored mesh animation accesses a missing original vertex frame");
    };
    // Pinned sampling reads both adjacent frames even when fraction is zero.
    validate(plan.offsets[0]); validate(plan.offsets[1]);
    if (plan.t1 != 0.0f) validate(plan.offsets[2]);
}

} // namespace MeshAnimationDetail

// Capture before replacing a channel's sequence, just like SetTweenFrom... .
inline MeshTweenHistory CaptureMeshTweenHistory(
    const PortableMeshAnimationData& data, const MeshAnimationChannel& channel,
    const bool allowFirstSequenceFallback = true, const MeshAnimationLimits& limits = {}) {
    ValidateMeshAnimationData(data, limits);
    const auto* sequence = FindMeshAnimationSequence(data, channel.sequence, allowFirstSequenceFallback);
    if (sequence == nullptr) return {};
    MeshAnimationChannel positive = channel;
    if (!std::isfinite(positive.normalizedFrame)) throw std::runtime_error("Cannot capture a non-finite mesh tween");
    positive.normalizedFrame = std::max(positive.normalizedFrame, 0.0f);
    const auto plan = MeshAnimationDetail::PlanChannel(data, *sequence, positive, false);
    MeshAnimationDetail::ValidateSampledFrames(data,plan);
    return {plan.offsets[0], plan.offsets[1], plan.t0};
}

inline MeshPose PrepareMeshPose(const PortableMeshAnimationData& data,
                                const MeshAnimationState& state,
                                const MeshAnimationLimits& limits = {}) {
    MeshPose pose;
    try {
        pose.retainedBytes = ValidateMeshAnimationData(data, limits);
        const auto* sequence = FindMeshAnimationSequence(data, state.main.sequence, true, &pose.fallbackUsed);
        if (sequence == nullptr) return pose; // Pinned renderer omits empty sequence tables.
        pose.resolvedSequence = sequence->name;
        // This flag includes all named, resolved channels, including a blend
        // fading in from zero weight. Access validity is checked separately.
        pose.selectedOriginalSpanInvalid = sequence->invalidOriginalSpan ||
            !HasUsableMeshAnimationSpan(data,*sequence);
        const auto main = MeshAnimationDetail::PlanChannel(data, *sequence, state.main, false);
        MeshAnimationDetail::ValidateSampledFrames(data,main);
        std::vector<MeshAnimationDetail::FramePlan> blends;
        for (const auto& channel : state.blends) {
            if (channel.sequence.empty() || MeshAnimationNamesEqual(channel.sequence, "None")) continue;
            const auto* blendSequence = FindMeshAnimationSequence(data, channel.sequence, false);
            if (blendSequence != nullptr) {
                pose.selectedOriginalSpanInvalid = pose.selectedOriginalSpanInvalid ||
                    blendSequence->invalidOriginalSpan || !HasUsableMeshAnimationSpan(data,*blendSequence);
                auto plan = MeshAnimationDetail::PlanChannel(data, *blendSequence, channel, true);
                if (plan.weight > 0.0f) MeshAnimationDetail::ValidateSampledFrames(data,plan);
                blends.push_back(plan);
            }
        }
        std::set<std::uint32_t> frames;
        const auto addFrames = [&](const MeshAnimationDetail::FramePlan& plan) {
            frames.insert(plan.offsets[0]/data.frameVertices);
            frames.insert(plan.offsets[1]/data.frameVertices);
            if (plan.t1 != 0.0f) frames.insert(plan.offsets[2]/data.frameVertices);
        };
        addFrames(main);
        for (const auto& blend : blends) {
            if (blend.weight <= 0.0f) continue;
            addFrames(blend);
            if (data.lodMesh) frames.insert(0u); // Additive rest normals, not the first sequence.
        }
        const std::uint64_t normalSamples = static_cast<std::uint64_t>(frames.size())*data.normalTopology.size();
        const std::uint64_t sampledBytes =
            static_cast<std::uint64_t>(frames.size()+2u)*data.frameVertices*sizeof(ActorVec3)+
            static_cast<std::uint64_t>(data.specialFaceVertexIndices.size())*sizeof(ActorVec3);
        if (frames.size() > limits.maxSampledFrames || normalSamples > limits.maxNormalTriangleSamples ||
            sampledBytes > limits.maxSampledBytes)
            throw std::runtime_error("Mesh animation pose sampling budget exceeded");
        pose.sampledFrames = frames.size();
        pose.normalTriangleSamples = static_cast<std::size_t>(normalSamples);
        const ActorMatrix3 meshToObject = UnrealActorRotation(
            data.rotationOriginPitch, data.rotationOriginYaw, data.rotationOriginRoll)*ActorScaleMatrix(data.scale);
        const ActorMatrix3 meshNormalToObject = meshToObject.NormalMatrix();
        const auto rawPosition = [&](const std::uint32_t offset, const std::uint32_t vertex) {
            const auto& packed = data.frameVerticesPacked[static_cast<std::size_t>(offset)+vertex];
            return ActorVec3{static_cast<float>(packed.x), static_cast<float>(packed.y), static_cast<float>(packed.z)};
        };
        struct FrameNormals { std::uint32_t frame{}; std::vector<ActorVec3> normals; };
        std::vector<FrameNormals> cachedNormals;
        cachedNormals.reserve(frames.size());
        for (const auto frame : frames) {
            FrameNormals cached;
            cached.frame = frame;
            cached.normals.resize(data.frameVertices);
            const auto offset = frame*data.frameVertices;
            for (const auto& face : data.normalTopology) {
                const ActorVec3 a = rawPosition(offset, face[0]), b = rawPosition(offset, face[1]),
                                c = rawPosition(offset, face[2]);
                const ActorVec3 u{b.x-a.x,b.y-a.y,b.z-a.z}, v{c.x-a.x,c.y-a.y,c.z-a.z};
                const auto normal = MeshAnimationDetail::Normalize({u.y*v.z-u.z*v.y, u.z*v.x-u.x*v.z, u.x*v.y-u.y*v.x});
                for (const auto vertex : face) {
                    auto& target = cached.normals[vertex];
                    target.x += normal.x; target.y += normal.y; target.z += normal.z;
                }
            }
            for (auto& normal : cached.normals) normal = MeshAnimationDetail::Normalize(normal);
            cachedNormals.push_back(std::move(cached));
        }
        const auto rawNormal = [&](const std::uint32_t offset, const std::uint32_t vertex) -> const ActorVec3& {
            const auto frame = offset/data.frameVertices;
            for (const auto& cached : cachedNormals)
                if (cached.frame == frame) return cached.normals[vertex];
            throw std::runtime_error("Mesh animation normal frame was not cached");
        };
        const auto sample = [&](const MeshAnimationDetail::FramePlan& plan, const std::uint32_t vertex,
                                const float fatness) {
            const auto inflated = [&](const std::uint32_t offset) {
                ActorVec3 position = rawPosition(offset, vertex);
                if (fatness != 0.0f) {
                    const auto& normal = rawNormal(offset, vertex);
                    position.x += normal.x*fatness; position.y += normal.y*fatness; position.z += normal.z*fatness;
                }
                return position;
            };
            auto position = MeshAnimationDetail::Mix(inflated(plan.offsets[0]), inflated(plan.offsets[1]), plan.t0);
            if (plan.t1 != 0.0f)
                position = MeshAnimationDetail::Mix(position, inflated(plan.offsets[2]), plan.t1);
            return position;
        };
        const auto sampleNormal = [&](const MeshAnimationDetail::FramePlan& plan, const std::uint32_t vertex) {
            auto normal = MeshAnimationDetail::Mix(rawNormal(plan.offsets[0], vertex), rawNormal(plan.offsets[1], vertex), plan.t0);
            if (plan.t1 != 0.0f) normal = MeshAnimationDetail::Mix(normal, rawNormal(plan.offsets[2], vertex), plan.t1);
            return normal;
        };
        const auto objectPosition = [&](const ActorVec3& position) {
            const auto result = meshToObject.Transform({position.x-data.origin.x, position.y-data.origin.y, position.z-data.origin.z});
            if (!IsFiniteActorVector(result)) throw std::runtime_error("Mesh animation sampled position is non-finite");
            return result;
        };
        pose.objectPositions.resize(data.frameVertices);
        pose.objectNormals.resize(data.frameVertices);
        const float fatness = static_cast<float>(state.fatness)/16.0f-8.0f;
        for (std::uint32_t vertex = 0u; vertex < data.frameVertices; ++vertex) {
            auto position = sample(main, vertex, fatness);
            auto normal = sampleNormal(main, vertex);
            const auto restPosition = rawPosition(0u, vertex);
            for (const auto& blend : blends) {
                if (blend.weight <= 0.0f) continue;
                const auto blended = sample(blend, vertex, 0.0f);
                position.x += (blended.x-restPosition.x)*blend.weight;
                position.y += (blended.y-restPosition.y)*blend.weight;
                position.z += (blended.z-restPosition.z)*blend.weight;
                if (data.lodMesh) {
                    const auto blendedNormal = sampleNormal(blend, vertex);
                    const auto& restNormal = rawNormal(0u, vertex);
                    normal.x += (blendedNormal.x-restNormal.x)*blend.weight;
                    normal.y += (blendedNormal.y-restNormal.y)*blend.weight;
                    normal.z += (blendedNormal.z-restNormal.z)*blend.weight;
                }
            }
            pose.objectPositions[vertex] = objectPosition(position);
            pose.objectNormals[vertex] = MeshAnimationDetail::Normalize(meshNormalToObject.Transform(normal));
        }
        pose.attachmentPositions.reserve(data.specialFaceVertexIndices.size());
        for (const auto vertex : data.specialFaceVertexIndices)
            pose.attachmentPositions.push_back(objectPosition(sample(main, vertex, 0.0f)));
        pose.drawable = true;
    } catch (const std::exception& error) {
        pose.drawable = false;
        pose.error = error.what();
        pose.objectPositions.clear(); pose.objectNormals.clear(); pose.attachmentPositions.clear();
    }
    return pose;
}

template<typename Mesh>
inline MeshPose PrepareMeshPose(const Mesh& mesh, const MeshAnimationState& state,
                                const MeshAnimationLimits& limits = {}) {
    if (!mesh.animation) {
        MeshPose pose;
        pose.error = "Mesh has no retained animation data";
        return pose;
    }
    return PrepareMeshPose(*mesh.animation, state, limits);
}

} // namespace QuestVr
