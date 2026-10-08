#pragma once

#include "quest_mesh_animation.h"

// Native animation-command/clock state, independent of the VM, OpenXR and GL.
// Audited against pinned UActor_Animation.cpp and Native/NActor.cpp. A caller
// must execute actual script commands; this header never selects idle/startup
// sequences, starts AI, or substitutes a guessed UnrealScript state branch.
namespace QuestVr {

struct ActorMainAnimationClock {
    float rate{}, last{}, minRate{}, tweenRate{}, oldRate{};
    bool loop{}, notify{}, finished{}, finishAnimWaiting{};
};

struct ActorBlendAnimationClock {
    float rate{}, last{}, minRate{}, tweenRate{}, oldRate{};
    // SimBlendAnim.{x,y,z,w}, retained for exact command property updates.
    std::array<float, 4> simulated{};
};

struct ActorAnimationClock {
    MeshAnimationState pose;
    ActorMainAnimationClock main;
    std::array<ActorBlendAnimationClock, 4> blends;
    double simulationTime{};
    std::uint8_t remoteRole{};
};

enum class ActorAnimationCommandKind {
    PlayAnim, LoopAnim, TweenAnim, FinishAnim, PlayBlendAnim, TweenBlendAnim
};

struct ActorAnimationCommand {
    ActorAnimationCommandKind kind{ActorAnimationCommandKind::PlayAnim};
    std::string sequence;
    // NActor's native defaults: Rate 1, TweenTime 0, MinRate 0, BlendSlot 0.
    float rate{1.0f}, tweenTime{}, minRate{};
    std::int32_t blendSlot{};
};

struct ActorAnimationCommandResult {
    bool applied{}, fallbackUsed{}, selectedOriginalSpanInvalid{};
    bool capturedOriginalSpanInvalid{}, capturedOriginalHistoryInvalid{};
    bool pinnedTweenBlendPositiveFrame{};
    std::string resolvedSequence, error;
};

enum class ActorAnimationEventKind { Notify, AnimEnd };

struct ActorAnimationEvent {
    ActorAnimationEventKind kind{ActorAnimationEventKind::Notify};
    std::string function, sequence;
    float normalizedFrame{};
    double simulationTime{};
};

struct ActorAnimationClockLimits {
    std::size_t maxEvents{64u}, maxSubsteps{256u}, maxNotifySearches{65'536u};
    std::size_t maxSequences{65'536u}, maxSequenceNameBytes{65'536u};
};

struct ActorAnimationBoundaryResult {
    bool ok{true}, eventReady{}, finishAnimReleased{}, budgetExhausted{};
    bool correctedPinnedPastEndElapsed{};
    float consumedSeconds{}, remainingSeconds{};
    ActorAnimationEvent event;
    std::size_t notifySearches{};
    std::string error;
};

struct ActorBlendClockResult {
    bool ok{true}, correctedPinnedBlendElapsed{true};
    std::array<float, 4> consumedSeconds{}, remainingSeconds{};
    std::string error;
};

struct ActorAnimationTickResult {
    bool ok{true}, budgetExhausted{}, finishAnimReleased{};
    bool correctedPinnedPastEndElapsed{}, correctedPinnedBlendElapsed{};
    float consumedSeconds{}, remainingSeconds{}, pendingBlendSeconds{};
    std::size_t substeps{}, notifySearches{};
    std::vector<ActorAnimationEvent> events;
    std::string error;
};

namespace ActorAnimationClockDetail {

inline void RequireFinite(const float value, const char* label) {
    if (!std::isfinite(value)) throw std::runtime_error(std::string(label)+" is non-finite");
}

inline const PortableMeshAnimationSequence* Sequence(
    const PortableMeshAnimationData* mesh, const std::string& name,
    const bool fallback, const ActorAnimationClockLimits& limits,
    bool* fallbackUsed = nullptr) {
    if (fallbackUsed != nullptr) *fallbackUsed = false;
    if (mesh == nullptr) return nullptr;
    if (mesh->sequences.size() > limits.maxSequences || name.size() > limits.maxSequenceNameBytes)
        throw std::runtime_error("Actor animation sequence lookup budget exceeded");
    const auto* sequence = FindMeshAnimationSequence(*mesh,name,fallback,fallbackUsed);
    if (sequence != nullptr && (sequence->startFrame < 0 || sequence->numFrames <= 0 ||
        !std::isfinite(sequence->rate) || sequence->name.size() > limits.maxSequenceNameBytes))
        throw std::runtime_error("Actor animation selected sequence metadata is invalid");
    return sequence;
}

inline void ValidateClock(const ActorAnimationClock& clock) {
    if (!std::isfinite(clock.simulationTime) || clock.simulationTime < 0.0)
        throw std::runtime_error("Actor animation simulation time is invalid");
    RequireFinite(clock.pose.main.normalizedFrame,"Actor AnimFrame");
    RequireFinite(clock.main.rate,"Actor AnimRate");
    RequireFinite(clock.main.last,"Actor AnimLast");
    RequireFinite(clock.main.minRate,"Actor AnimMinRate");
    RequireFinite(clock.main.tweenRate,"Actor TweenRate");
    RequireFinite(clock.main.oldRate,"Actor OldAnimRate");
    // A notify exactly at 1 may expose that transient frame before the next
    // native boundary wraps/stops it. The mesh sampler still rejects frame1.
    if (clock.pose.main.normalizedFrame > 1.0f || clock.main.last < 0.0f || clock.main.last >= 1.0f)
        throw std::runtime_error("Actor animation normalized clock range is invalid");
}

inline void ValidateElapsed(const float elapsed, const float speed) {
    RequireFinite(elapsed,"Actor animation elapsed time");
    RequireFinite(speed,"Actor animation velocity speed");
    if (elapsed < 0.0f || speed < 0.0f)
        throw std::runtime_error("Actor animation elapsed time or velocity speed is negative");
}

inline MeshTweenHistory CaptureHistory(
    const PortableMeshAnimationData* mesh, const MeshAnimationChannel& channel,
    const bool blend, const ActorAnimationClockLimits& limits,
    bool& danglingSpan, bool& invalidHistory) {
    if (mesh == nullptr || (blend && MeshAnimationNamesEqual(channel.sequence,"None"))) return {};
    const auto* sequence = Sequence(mesh,channel.sequence,true,limits);
    if (sequence == nullptr) return {};
    if (mesh->frameVertices == 0u)
        throw std::runtime_error("Captured animation mesh has no frame vertices");
    danglingSpan = danglingSpan || sequence->invalidOriginalSpan || !HasUsableMeshAnimationSpan(*mesh,*sequence);
    RequireFinite(channel.normalizedFrame,"Captured actor animation frame");
    if (channel.normalizedFrame > 1.0f)
        throw std::runtime_error("Captured actor animation frame exceeds the native clock range");
    // Native SetTweenFrom... merely records offsets. It reads no vertex data:
    // retain dangling authored offsets for the sampler to diagnose on access.
    // In particular a notify at frame1 can synchronously start another command.
    // Pinned history calculates its fraction AFTER modulo, so at frame1 its
    // offsets wrap to frames0/1 but its fraction is NumFrames (not zero).
    // Preserve that metadata and diagnose it; the sampler rejects the invalid
    // fraction when actually drawing the tween rather than blocking a command.
    const float rawFrame = std::max(channel.normalizedFrame,0.0f)*static_cast<float>(sequence->numFrames);
    RequireFinite(rawFrame,"Captured actor animation scaled frame");
    if (static_cast<double>(rawFrame) >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        throw std::runtime_error("Captured actor animation frame exceeds safe native int32 arithmetic");
    const auto originalFrame = static_cast<std::uint32_t>(rawFrame);
    const auto frames = static_cast<std::uint32_t>(sequence->numFrames);
    const auto frame0 = originalFrame%frames;
    const auto frame1 = (originalFrame+1u)%frames;
    const auto offset0 = (static_cast<std::uint64_t>(sequence->startFrame)+frame0)*mesh->frameVertices;
    const auto offset1 = (static_cast<std::uint64_t>(sequence->startFrame)+frame1)*mesh->frameVertices;
    if (offset0 > std::numeric_limits<std::uint32_t>::max() || offset1 > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Captured actor animation offsets exceed retained history storage");
    const float fraction = rawFrame-static_cast<float>(frame0);
    invalidHistory = invalidHistory || fraction < 0.0f || fraction >= 1.0f;
    return {static_cast<std::uint32_t>(offset0),static_cast<std::uint32_t>(offset1),fraction};
}

inline void SetMultiFrameRates(ActorAnimationClock& clock,
    const PortableMeshAnimationSequence& sequence, const ActorAnimationCommand& command,
    const bool loop) {
    const float frames = static_cast<float>(sequence.numFrames);
    clock.main.rate = command.rate*sequence.rate/frames;
    if (loop) clock.main.minRate = command.minRate*sequence.rate/frames;
    clock.main.tweenRate = command.tweenTime > 0.0f ? 1.0f/(command.tweenTime*frames) : 0.0f;
    clock.main.oldRate = clock.main.rate;
}

inline void SetSingleFrame(ActorAnimationClock& clock,const float tweenTime) {
    clock.pose.main.normalizedFrame = -1.0f;
    clock.main.last = clock.main.rate = clock.main.oldRate = clock.main.minRate = 0.0f;
    clock.main.tweenRate = tweenTime > 0.0f ? 1.0f/tweenTime : 10.0f;
    clock.main.notify = false;
}

inline void UpdateSimulatedBlend(ActorBlendAnimationClock& blend,const float frame) {
    const auto previous = blend.simulated;
    blend.simulated = {blend.tweenRate*1000.0f,blend.last*10000.0f,
                       frame*10000.0f,blend.rate*10000.0f};
    if (blend.simulated == previous) blend.simulated[1] += 1.0f;
}

inline float BoundedConsumption(const float requested,const float available) {
    RequireFinite(requested,"Actor animation boundary consumption");
    // Arithmetic rounds at float precision just like UActor's clock, but may
    // never debit negative time or claim more elapsed seconds than supplied.
    return std::clamp(requested,0.0f,available);
}

} // namespace ActorAnimationClockDetail

inline bool ActorClockHasAnim(const PortableMeshAnimationData* mesh,const std::string& name,
                              const ActorAnimationClockLimits& limits = {}) {
    return ActorAnimationClockDetail::Sequence(mesh,name,true,limits) != nullptr;
}

inline std::string ActorClockGetAnimGroup(const PortableMeshAnimationData* mesh,const std::string& name,
                                         const ActorAnimationClockLimits& limits = {}) {
    const auto* sequence = ActorAnimationClockDetail::Sequence(mesh,name,true,limits);
    return sequence != nullptr ? sequence->group : std::string();
}

inline bool ActorClockIsAnimating(const ActorAnimationClock& clock) { return clock.main.rate != 0.0f; }

// Transactional property updates: failed validation leaves the clock unchanged.
// Commands are applied to Self, not Owner. bAnimByOwner is a render-source rule
// owned by the runtime wrapper; placement, PrePivot and Fatness remain on Self.
inline ActorAnimationCommandResult ApplyActorAnimationCommand(
    const PortableMeshAnimationData* mesh, ActorAnimationClock& clock,
    const ActorAnimationCommand& command, const float velocitySpeed = 0.0f,
    const ActorAnimationClockLimits& limits = {}) {
    ActorAnimationCommandResult result;
    try {
        ActorAnimationClockDetail::ValidateElapsed(0.0f,velocitySpeed);
        ActorAnimationClockDetail::RequireFinite(command.rate,"Animation command rate");
        ActorAnimationClockDetail::RequireFinite(command.tweenTime,"Animation command tween time");
        ActorAnimationClockDetail::RequireFinite(command.minRate,"Animation command min rate");
        ActorAnimationClock next = clock;
        if (command.kind == ActorAnimationCommandKind::FinishAnim) {
            if (next.main.loop) { next.main.loop = false; next.main.finished = false; }
            next.main.finishAnimWaiting = true;
            clock = std::move(next); result.applied = true; return result;
        }
        const bool blend = command.kind == ActorAnimationCommandKind::PlayBlendAnim ||
                           command.kind == ActorAnimationCommandKind::TweenBlendAnim;
        if (blend && (command.blendSlot < 0 || command.blendSlot >= 4))
            throw std::runtime_error("Animation native blend slot is outside 0..3");
        const auto* sequence = ActorAnimationClockDetail::Sequence(mesh,command.sequence,
            command.kind != ActorAnimationCommandKind::TweenBlendAnim,limits,&result.fallbackUsed);
        if (sequence == nullptr) return result; // Pinned no-mesh/no-sequence no-op.
        result.resolvedSequence = sequence->name;
        result.selectedOriginalSpanInvalid = sequence->invalidOriginalSpan || !HasUsableMeshAnimationSpan(*mesh,*sequence);
        const float frames = static_cast<float>(sequence->numFrames);
        if (command.kind == ActorAnimationCommandKind::PlayAnim || command.kind == ActorAnimationCommandKind::LoopAnim) {
            const bool loop = command.kind == ActorAnimationCommandKind::LoopAnim;
            const bool repeatLoop = loop && MeshAnimationNamesEqual(next.pose.main.sequence,command.sequence) &&
                ActorClockIsAnimating(next) && next.main.loop;
            if (repeatLoop) {
                if (sequence->numFrames > 1) ActorAnimationClockDetail::SetMultiFrameRates(next,*sequence,command,true);
            } else {
                next.pose.main.previous = ActorAnimationClockDetail::CaptureHistory(mesh,next.pose.main,false,limits,
                    result.capturedOriginalSpanInvalid,result.capturedOriginalHistoryInvalid);
                next.pose.main.sequence = command.sequence;
                if (sequence->numFrames > 1) {
                    next.pose.main.normalizedFrame = command.tweenTime > 0.0f ? -1.0f/frames : 0.0f;
                    next.main.last = 1.0f-1.0f/frames;
                    next.main.notify = !sequence->notifies.empty();
                    ActorAnimationClockDetail::SetMultiFrameRates(next,*sequence,command,loop);
                } else ActorAnimationClockDetail::SetSingleFrame(next,command.tweenTime);
                next.main.loop = loop; next.main.finished = false;
            }
        } else if (command.kind == ActorAnimationCommandKind::TweenAnim) {
            next.pose.main.previous = ActorAnimationClockDetail::CaptureHistory(mesh,next.pose.main,false,limits,
                result.capturedOriginalSpanInvalid,result.capturedOriginalHistoryInvalid);
            next.pose.main.sequence = command.sequence;
            next.pose.main.normalizedFrame = command.tweenTime > 0.0f ? -1.0f/frames : 0.0f;
            next.main.last = next.main.minRate = next.main.rate = next.main.oldRate = 0.0f;
            next.main.tweenRate = command.tweenTime > 0.0f ? 1.0f/(frames*command.tweenTime) : 0.0f;
            next.main.notify = next.main.finished = next.main.loop = false;
        } else if (command.kind == ActorAnimationCommandKind::PlayBlendAnim) {
            auto& channel = next.pose.blends[static_cast<std::size_t>(command.blendSlot)];
            auto& target = next.blends[static_cast<std::size_t>(command.blendSlot)];
            channel.previous = ActorAnimationClockDetail::CaptureHistory(mesh,channel,true,limits,
                result.capturedOriginalSpanInvalid,result.capturedOriginalHistoryInvalid);
            channel.sequence = command.sequence;
            channel.normalizedFrame = -1.0f/frames;
            target.rate = command.rate*sequence->rate/frames;
            target.last = 1.0f-1.0f/frames;
            if (target.last == 0.0f) {
                target.rate = channel.normalizedFrame = 0.0f;
                target.tweenRate = command.tweenTime <= 0.0f ? 10.0f : 1.0f/command.tweenTime;
            } else if (command.tweenTime <= 0.0f) {
                if (command.tweenTime == -1.0f) {
                    channel.normalizedFrame = 0.0f;
                    if (target.minRate <= 0.0f) target.tweenRate = target.minRate == 0.0f ?
                        1.0f/(frames*0.025f) : std::max(velocitySpeed*(-target.minRate),target.rate*0.5f);
                    else target.tweenRate = target.minRate;
                } else { target.tweenRate = 0.0f; channel.normalizedFrame = 0.001f; }
            } else target.tweenRate = 1.0f/(frames*command.tweenTime);
            ActorAnimationClockDetail::UpdateSimulatedBlend(target,channel.normalizedFrame);
            target.oldRate = target.rate;
        } else if (command.kind == ActorAnimationCommandKind::TweenBlendAnim) {
            auto& channel = next.pose.blends[static_cast<std::size_t>(command.blendSlot)];
            auto& target = next.blends[static_cast<std::size_t>(command.blendSlot)];
            channel.sequence = command.sequence;
            target.last = target.minRate = target.rate = target.oldRate = 0.0f;
            target.tweenRate = command.tweenTime <= 0.0f ? 0.0f : 1.0f/(frames*command.tweenTime);
            // Deliberately preserve this pinned command quirk. It neither
            // captures history nor writes a negative tween frame, so time>0
            // does not actually tween. Do not claim original-DLL equivalence.
            channel.normalizedFrame = command.tweenTime <= 0.0f ? 0.0f : 1.0f/frames;
            result.pinnedTweenBlendPositiveFrame = command.tweenTime > 0.0f;
        } else throw std::runtime_error("Unsupported actor animation command kind");
        ActorAnimationClockDetail::ValidateClock(next);
        for (std::size_t slot = 0u; slot < next.blends.size(); ++slot) {
            const auto& target = next.blends[slot];
            ActorAnimationClockDetail::RequireFinite(target.rate,"BlendAnimRate");
            ActorAnimationClockDetail::RequireFinite(target.last,"BlendAnimLast");
            ActorAnimationClockDetail::RequireFinite(target.minRate,"BlendAnimMinRate");
            ActorAnimationClockDetail::RequireFinite(target.tweenRate,"BlendTweenRate");
            ActorAnimationClockDetail::RequireFinite(target.oldRate,"OldBlendAnimRate");
            ActorAnimationClockDetail::RequireFinite(next.pose.blends[slot].normalizedFrame,"BlendAnimFrame");
            for (const auto value : target.simulated) ActorAnimationClockDetail::RequireFinite(value,"SimBlendAnim");
        }
        clock = std::move(next); result.applied = true;
    } catch (const std::exception& error) { result.error = error.what(); }
    return result;
}

// Advance at most one native-clock boundary. The wrapper MUST immediately
// dispatch event.function on Self before calling again: that actual script may
// replace the sequence, rates, mesh or state. Re-read mesh/speed after dispatch.
// hasNotifyFunction is a read-only actual VM lookup; absent handlers are skipped
// as FindEventFunction does, not emitted as fabricated callbacks.
template<typename HasNotifyFunction>
inline ActorAnimationBoundaryResult AdvanceMainAnimationBoundary(
    const PortableMeshAnimationData* mesh, ActorAnimationClock& clock,
    const float elapsed, const float velocitySpeed, HasNotifyFunction hasNotifyFunction,
    const ActorAnimationClockLimits& limits = {}) {
    ActorAnimationBoundaryResult result;
    result.remainingSeconds = elapsed;
    try {
        ActorAnimationClockDetail::ValidateElapsed(elapsed,velocitySpeed);
        ActorAnimationClockDetail::ValidateClock(clock);
        ActorAnimationClock next = clock;
        if (next.main.finishAnimWaiting && (!ActorClockIsAnimating(next) ||
            next.pose.main.normalizedFrame >= next.main.last)) {
            next.main.finishAnimWaiting = false; result.finishAnimReleased = true;
        }
        const auto consume = [&](const float seconds) {
            result.consumedSeconds = ActorAnimationClockDetail::BoundedConsumption(seconds,elapsed);
            result.remainingSeconds = elapsed-result.consumedSeconds;
            next.simulationTime += static_cast<double>(result.consumedSeconds);
            if (!std::isfinite(next.simulationTime)) throw std::runtime_error("Actor animation simulation time overflowed");
        };
        const auto event = [&](const ActorAnimationEventKind kind,const std::string& function) {
            result.eventReady = true;
            result.event = {kind,function,next.pose.main.sequence,next.pose.main.normalizedFrame,next.simulationTime};
        };
        if (elapsed == 0.0f) { clock = std::move(next); return result; }
        const float from = next.pose.main.normalizedFrame;
        if (from >= 0.0f) {
            const float rate = next.main.rate >= 0.0f ? next.main.rate :
                std::max(next.main.minRate,-next.main.rate*velocitySpeed);
            ActorAnimationClockDetail::RequireFinite(rate,"Computed actor animation rate");
            if (rate < 0.0f) throw std::runtime_error("Computed actor animation rate is negative");
            if (rate == 0.0f) { consume(elapsed); clock = std::move(next); return result; }
            const float to = from+rate*elapsed;
            ActorAnimationClockDetail::RequireFinite(to,"Computed actor animation frame");
            const auto* sequence = next.main.notify ?
                ActorAnimationClockDetail::Sequence(mesh,next.pose.main.sequence,true,limits) : nullptr;
            // Notify search intentionally precedes native end/loop clamping.
            if (sequence != nullptr) {
                for (const auto& notify : sequence->notifies) {
                    if (++result.notifySearches > limits.maxNotifySearches) {
                        result.budgetExhausted = true; result.error = "Actor animation notify-search budget exhausted";
                        result.finishAnimReleased = result.eventReady = false;
                        return result; // No state/time was committed.
                    }
                    ActorAnimationClockDetail::RequireFinite(notify.time,"Actor animation notify time");
                    if (notify.time < 0.0f || notify.time > 1.0f)
                        throw std::runtime_error("Actor animation notify time is outside the native normalized range");
                    if (notify.time > from && notify.time <= to && hasNotifyFunction(notify.function)) {
                        next.pose.main.normalizedFrame = notify.time;
                        consume((notify.time-from)/rate);
                        event(ActorAnimationEventKind::Notify,notify.function);
                        clock = std::move(next); return result;
                    }
                }
            }
            if (next.main.loop && next.main.last > from && next.main.last <= to) {
                next.pose.main.normalizedFrame = next.main.last;
                consume((next.main.last-from)/rate);
                if (next.main.finishAnimWaiting) { next.main.finishAnimWaiting = false; result.finishAnimReleased = true; }
                event(ActorAnimationEventKind::AnimEnd,"AnimEnd");
                clock = std::move(next); return result;
            }
            const float end = next.main.loop ? 1.0f : next.main.last;
            if (to >= end) {
                // FinishAnim may stop a loop after AnimLast. Pinned code then
                // debits negative elapsed, creating time and moving its clock
                // backwards. Preserve terminal property updates, debit zero,
                // and expose this correction rather than hiding extra time.
                result.correctedPinnedPastEndElapsed = end < from;
                consume((end-from)/rate);
                next.pose.main.normalizedFrame = end;
                if (next.main.loop) next.pose.main.normalizedFrame = 0.0f;
                else { next.main.rate = 0.0f; next.main.finished = true; }
                if (!next.main.loop && from < end) {
                    if (next.main.finishAnimWaiting) { next.main.finishAnimWaiting = false; result.finishAnimReleased = true; }
                    event(ActorAnimationEventKind::AnimEnd,"AnimEnd");
                }
            } else { next.pose.main.normalizedFrame = to; consume(elapsed); }
        } else {
            if (next.main.tweenRate < 0.0f) throw std::runtime_error("Actor TweenRate is negative");
            if (next.main.tweenRate == 0.0f) { consume(elapsed); clock = std::move(next); return result; }
            const float to = from+next.main.tweenRate*elapsed;
            ActorAnimationClockDetail::RequireFinite(to,"Computed actor tween frame");
            if (to >= 0.0f) {
                consume(-from/next.main.tweenRate);
                next.pose.main.normalizedFrame = 0.0f;
                if (next.main.rate == 0.0f) {
                    next.main.finished = true;
                    if (next.main.finishAnimWaiting) { next.main.finishAnimWaiting = false; result.finishAnimReleased = true; }
                    event(ActorAnimationEventKind::AnimEnd,"AnimEnd");
                }
            } else { next.pose.main.normalizedFrame = to; consume(elapsed); }
        }
        clock = std::move(next);
    } catch (const std::exception& error) {
        // A failed boundary commits neither time nor properties.
        result.ok = false; result.error = error.what();
        result.consumedSeconds = 0.0f; result.remainingSeconds = elapsed;
        result.eventReady = result.finishAnimReleased = false;
        result.correctedPinnedPastEndElapsed = false;
    }
    return result;
}

// Four slots receive the SAME supplied elapsed time independently. This fixes
// the pinned TickBlendAnimation shared-elapsed mutation which zeroes time on a
// finishing slot, starving later slots. Each slot also carries its true tween
// remainder into normal playback; no time is created/dropped across slots.
// The pinned negative-rate cap against BlendAnimLast is retained explicitly.
inline ActorBlendClockResult AdvanceBlendAnimationClock(
    ActorAnimationClock& clock,const float elapsed,const float velocitySpeed) {
    ActorBlendClockResult result;
    result.remainingSeconds.fill(elapsed);
    try {
        ActorAnimationClockDetail::ValidateElapsed(elapsed,velocitySpeed);
        ActorAnimationClock next = clock;
        for (std::size_t slot = 0u; slot < next.blends.size(); ++slot) {
            auto& channel = next.pose.blends[slot];
            auto& target = next.blends[slot];
            if (MeshAnimationNamesEqual(channel.sequence,"None")) {
                result.consumedSeconds[slot] = elapsed; result.remainingSeconds[slot] = 0.0f; continue;
            }
            for (const auto value : {channel.normalizedFrame,target.rate,target.last,target.minRate,target.tweenRate,target.oldRate})
                ActorAnimationClockDetail::RequireFinite(value,"Actor blend clock property");
            if (target.last < 0.0f || target.last >= 1.0f || channel.normalizedFrame > 1.0f)
                throw std::runtime_error("Actor blend normalized clock range is invalid");
            float remaining = elapsed;
            if (channel.normalizedFrame < target.last && channel.normalizedFrame < 0.0f) {
                if (target.tweenRate < 0.0f) throw std::runtime_error("Actor BlendTweenRate is negative");
                if (target.tweenRate > 0.0f) {
                    const float to = channel.normalizedFrame+remaining*target.tweenRate;
                    ActorAnimationClockDetail::RequireFinite(to,"Computed blend tween frame");
                    if (to < 0.0f) { channel.normalizedFrame = to; remaining = 0.0f; }
                    else {
                        remaining -= ActorAnimationClockDetail::BoundedConsumption(-channel.normalizedFrame/target.tweenRate,remaining);
                        channel.normalizedFrame = 0.0f;
                    }
                } else remaining = 0.0f; // Frozen channel, verified stationary.
            }
            if (remaining > 0.0f && channel.normalizedFrame < target.last) {
                const float rate = target.rate < 0.0f ? std::min(-velocitySpeed*target.rate,target.last) : target.rate;
                ActorAnimationClockDetail::RequireFinite(rate,"Computed blend animation rate");
                if (rate < 0.0f) throw std::runtime_error("Computed blend animation rate is negative");
                const float to = channel.normalizedFrame+rate*remaining;
                ActorAnimationClockDetail::RequireFinite(to,"Computed blend animation frame");
                if (to >= target.last) {
                    channel.normalizedFrame = target.last; target.rate = 0.0f;
                    if (next.remoteRole < 2u) {
                        target.simulated[2] = channel.normalizedFrame*10000.0f;
                        target.simulated[3] = std::min(target.rate*5000.0f,32767.0f);
                    }
                } else channel.normalizedFrame = to;
            }
            // Stationary/finished blend states still account for simulation
            // elapsed. Blends have no native notify/AnimEnd callbacks.
            result.consumedSeconds[slot] = elapsed; result.remainingSeconds[slot] = 0.0f;
        }
        clock = std::move(next);
    } catch (const std::exception& error) {
        result.ok = false; result.error = error.what();
        result.consumedSeconds.fill(0.0f); result.remainingSeconds.fill(elapsed);
    }
    return result;
}

// Convenience driver for an unchanged immutable mesh. Actual VM integration
// should use the boundary API so callback-driven mesh changes can be re-read.
// dispatch receives the event after committed native property/time updates;
// it may apply real animation commands synchronously to clock.
template<typename HasNotifyFunction,typename Dispatch>
inline ActorAnimationTickResult AdvanceActorAnimationClock(
    const PortableMeshAnimationData* mesh,ActorAnimationClock& clock,
    const float elapsed,const float velocitySpeed,HasNotifyFunction hasNotifyFunction,Dispatch dispatch,
    const ActorAnimationClockLimits& limits = {}) {
    ActorAnimationTickResult result;
    result.remainingSeconds = elapsed; result.pendingBlendSeconds = elapsed;
    try {
        ActorAnimationClockDetail::ValidateElapsed(elapsed,velocitySpeed);
        while (result.remainingSeconds > 0.0f || result.substeps == 0u) {
            if (result.substeps >= limits.maxSubsteps || result.events.size() >= limits.maxEvents) {
                result.budgetExhausted = true; result.error = "Actor animation event/substep budget exhausted"; break;
            }
            ActorAnimationClockLimits stepLimits = limits;
            stepLimits.maxNotifySearches = limits.maxNotifySearches >= result.notifySearches ?
                limits.maxNotifySearches-result.notifySearches : 0u;
            const auto step = AdvanceMainAnimationBoundary(mesh,clock,result.remainingSeconds,velocitySpeed,hasNotifyFunction,stepLimits);
            ++result.substeps; result.notifySearches += step.notifySearches;
            if (!step.ok || step.budgetExhausted) {
                result.ok = step.ok; result.budgetExhausted = step.budgetExhausted; result.error = step.error; break;
            }
            result.finishAnimReleased = result.finishAnimReleased || step.finishAnimReleased;
            result.correctedPinnedPastEndElapsed = result.correctedPinnedPastEndElapsed || step.correctedPinnedPastEndElapsed;
            result.remainingSeconds = step.remainingSeconds;
            result.consumedSeconds = elapsed-result.remainingSeconds;
            if (step.eventReady) { result.events.push_back(step.event); dispatch(step.event); }
            if (result.remainingSeconds == 0.0f) break;
        }
        if (result.ok && !result.budgetExhausted && result.remainingSeconds == 0.0f) {
            const auto blend = AdvanceBlendAnimationClock(clock,elapsed,velocitySpeed);
            result.ok = blend.ok; result.error = blend.error;
            result.correctedPinnedBlendElapsed = blend.correctedPinnedBlendElapsed;
            if (blend.ok) result.pendingBlendSeconds = 0.0f;
        }
    } catch (const std::exception& error) { result.ok = false; result.error = error.what(); }
    return result;
}

// Save/restore the complete native state above (pose channels AND their prior
// offsets/fractions, all rates/flags/SimBlendAnim, FinishAnim latent wait and
// simulationTime). If a bounded driver yields, persist its remainingSeconds
// and pendingBlendSeconds with the VM/event continuation. Replay the residual
// before accepting new elapsed; do not advance blends twice. Asset identity /
// actor Owner relationship and actual VM state stack remain wrapper-owned.

} // namespace QuestVr
