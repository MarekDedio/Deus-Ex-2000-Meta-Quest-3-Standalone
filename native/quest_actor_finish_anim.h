#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace QuestVr {

inline constexpr std::uint32_t OriginalActorFinishAnimNative=261u;
inline constexpr std::uint32_t OriginalActorFinishAnimFlags=0x409u;
inline constexpr std::uint32_t OriginalActorFinishAnimPollNative=385u;

enum class OriginalActorFinishAnimFloat { Frame,Rate,TweenRate,Last };
enum class OriginalActorFinishAnimBool { Loop,Finished };

struct OriginalActorFinishAnimQueries {
    // Providers authenticate a live actual Actor receiver and exact inherited
    // original properties. These are fresh reads, not an animation snapshot.
    std::function<bool(std::uintptr_t,OriginalActorFinishAnimBool)> readBool;
    std::function<float(std::uintptr_t,OriginalActorFinishAnimFloat)> readFloat;
    // This is the actual FName index0 test, not an absent mesh/sequence lookup.
    std::function<bool(std::uintptr_t)> sequenceIsNone;
    // One indivisible provider operation for retail AND flags,~(08|10).
    // Preserve bAnimNotify and every unrelated packed bit. A typed backend
    // may need two journal entries, but cannot dispatch, publish, or expose a
    // half-cleared pair between them; any second-write refusal undoes the
    // complete SAME outer root. Update only matching optional clock mirrors.
    std::function<void(std::uintptr_t)> clearLoopAndFinished;
    // The actual actor-owned native StateFrame identity. This provider is
    // reached ONLY by installation or a finished poll; no frame is allocated.
    // A present portable enum/clock cannot stand in for an authentic frame.
    std::function<std::uintptr_t(std::uintptr_t)> nativeFrame;
    // Write only that retained frame's native UInt32 LatentAction. Never
    // reinterpret portable latent enum ordinals, change Code/PC/locals/nodes,
    // clear a timer, or install a new frame. The action is exactly385 or0.
    std::function<void(std::uintptr_t,std::uintptr_t,std::uint32_t)> writeNativeLatent;
    // Same Execution/root's bounded work accounting. Providers are synchronous
    // native reads/writes, not reentrant script callbacks. Exceptions propagate
    // to that existing root; this pure helper does not own a second journal.
    std::function<void()> work;
};

struct OriginalActorFinishAnimStats {
    std::size_t steps{},boolReads{},floatReads{},sequenceReads{},frameReads{},
        packedFlagClears{},nativeLatentWrites{};
    bool sequenceInactive{},rateInactive{},atOrPastLast{},installed{},released{};
};

namespace OriginalActorFinishAnimDetail {
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original Actor FinishAnim: ")+reason);
}
inline float Finite(float value) {
    if(!std::isfinite(value))Fail("reached nonfinite Float is outside the supported finite comparison subset");
    return value;
}
template<class Provider,class... Arguments>
inline auto Call(const OriginalActorFinishAnimQueries& queries,
    OriginalActorFinishAnimStats& stats,const Provider& provider,Arguments&&... arguments) {
    if(!provider)Fail("reached native provider is missing");
    if(queries.work)queries.work();
    ++stats.steps;
    return provider(std::forward<Arguments>(arguments)...);
}
inline void StoreAction(std::uintptr_t actor,std::uint32_t action,
    const OriginalActorFinishAnimQueries& queries,OriginalActorFinishAnimStats& stats) {
    ++stats.frameReads;
    const auto frame=Call(queries,stats,queries.nativeFrame,actor);
    if(!frame)Fail("reached native StateFrame is absent");
    ++stats.nativeLatentWrites;
    Call(queries,stats,queries.writeNativeLatent,actor,frame,action);
}
} // namespace OriginalActorFinishAnimDetail

// Retail execFinishAnim261, Engine.dll103DFF60..103DFFCD. Native declaration
// flags0x409, zero arguments/OUT/result. The VM/native packet wrapper consumes
// its one EndFunctionParms byte; no expression Step belongs in this helper.
// Owned Engine.dll SHA256:
// 0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4.
// The retail x87 comparisons include unordered cases; this implementation
// deliberately supports finite reached fields only, with lazy explicit refusal
// rather than inventing NaN semantics. Signed Float zero remains exact.
inline OriginalActorFinishAnimStats ExecuteOriginalActorFinishAnim(
    std::uintptr_t actor,const OriginalActorFinishAnimQueries& queries) {
    using namespace OriginalActorFinishAnimDetail;
    using F=OriginalActorFinishAnimFloat;
    using B=OriginalActorFinishAnimBool;
    OriginalActorFinishAnimStats stats;
    ++stats.boolReads;
    if(Call(queries,stats,queries.readBool,actor,B::Loop)) {
        ++stats.packedFlagClears;
        Call(queries,stats,queries.clearLoopAndFinished,actor);
    }
    ++stats.sequenceReads;
    if(Call(queries,stats,queries.sequenceIsNone,actor)) {
        stats.sequenceInactive=true;return stats;
    }
    const auto read=[&](F field) {
        ++stats.floatReads;
        return Finite(Call(queries,stats,queries.readFloat,actor,field));
    };
    //103DFF7E..AF: the opposite rate and AnimLast are not read on this exit.
    const auto frame=read(F::Frame);
    if(read(frame>=0.0f?F::Rate:F::TweenRate)==0.0f) {
        stats.rateInactive=true;return stats;
    }
    //103DFFB1 reloads Frame. Do not substitute the earlier branch snapshot.
    const auto currentFrame=read(F::Frame);
    const auto last=read(F::Last);
    if(!(currentFrame<last)) {stats.atOrPastLast=true;return stats;}
    StoreAction(actor,OriginalActorFinishAnimPollNative,queries,stats);
    stats.installed=true;
    return stats;
}

// Retail execPollFinishAnim385, Engine.dll103E00A0..103E00B2. Its caller must
// select the authentic native385 handler through the real ProcessState route.
// Reads ONLY fresh bAnimFinished before the reached native-frame clear. No
// animation advance, Frame/Rate/Last/sequence/timer/mesh reads or flag reset.
inline OriginalActorFinishAnimStats PollOriginalActorFinishAnim(
    std::uintptr_t actor,const OriginalActorFinishAnimQueries& queries) {
    using namespace OriginalActorFinishAnimDetail;
    OriginalActorFinishAnimStats stats;
    ++stats.boolReads;
    if(!Call(queries,stats,queries.readBool,actor,OriginalActorFinishAnimBool::Finished))return stats;
    StoreAction(actor,0u,queries,stats);
    stats.released=true;
    return stats;
}
} // namespace QuestVr
