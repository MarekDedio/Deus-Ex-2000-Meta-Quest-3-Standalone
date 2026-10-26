#pragma once

#include "quest_native_scalar.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

struct OriginalActorLastRenderedQueries {
    // Actual retained Actor.XLevel native edge, not named Actor.Level/XLevel
    // script overlays. nativeTime receives this captured ULevel identity.
    std::function<std::uintptr_t(std::uintptr_t)> nativeLevel;
    // Raw actual Actor.LastRenderTime Float, including all NaN payload bits.
    // No CDO/default/renderer-derived fallback when native storage is absent.
    std::function<float(std::uintptr_t)> lastRenderTime;
    std::function<double(std::uintptr_t)> nativeTime;
    std::function<void()> work;
};
struct OriginalActorLastRenderedResult {
    float returned{};
    bool clamped{};
    std::size_t steps{},levelReads{},lastRenderReads{},nativeTimeReads{};
};
namespace ActorLastRenderedDetail {
template<class Provider,class... Args>
inline auto Call(const OriginalActorLastRenderedQueries& q,OriginalActorLastRenderedResult& result,
    const Provider& provider,Args&&... args) {
    if(!provider)throw std::invalid_argument("Original LastRendered lacks a reached provider");
    if(!q.work)throw std::invalid_argument("Original LastRendered lacks caller work authority");
    q.work();++result.steps;
    return provider(std::forward<Args>(args)...);
}
} // namespace ActorLastRenderedDetail

// Engine.Actor.LastRendered native723: thunk103021DA, body1036DE30..1036DE6E.
// The caller authenticates the original no-input/Float-return declaration and
// consumes its EndFunctionParms before entry. This is only the native query,
// not a renderer publication, clock advance or proof of a reached NPC branch.
// All provider effects/errors remain in the SAME caller root; no local undo.
inline OriginalActorLastRenderedResult QueryOriginalActorLastRendered(std::uintptr_t actor,
    const OriginalActorLastRenderedQueries& q) {
    if(!actor)throw std::invalid_argument("Original LastRendered requires a nonnull Actor");
    using namespace NativeScalar;
    using ActorLastRenderedDetail::Call;
    OriginalActorLastRenderedResult result;
    ++result.levelReads;const auto level=Call(q,result,q.nativeLevel,actor);
    ++result.lastRenderReads;const auto last=Load(Call(q,result,q.lastRenderTime,actor));
    // Native loads the edge before FLD LastRenderTime but dereferences ULevel
    // only at FSUBR afterwards. A null edge cannot suppress that prior read.
    if(!level)throw std::invalid_argument("Original LastRendered requires actual native XLevel");
    ++result.nativeTimeReads;const auto now=Memory(Call(q,result,q.nativeTime,level));
    const auto age=Subtract(now,last); // FSUBR m64; no Double/Float intermediate.
    result.clamped=Compare(176u,age,Memory(0.0f)); // Literal C0: negative OR unordered.
    result.returned=result.clamped?0.0f:StoreFloat(age);
    return result;
}
} // namespace QuestVr
