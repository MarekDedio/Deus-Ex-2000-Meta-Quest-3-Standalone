#pragma once

#include "quest_actor_line_trace.h"

#include <cstdint>
#include <functional>
#include <stdexcept>

namespace QuestVr {
// Original live-field seams only. isBrush is the native nonnull Brush pointer
// AND actual Engine.Brush ancestry predicate, not GetPrimitive or bStatic.
struct OriginalActorOverlapQueries {
    std::function<bool(std::uintptr_t)> isBrush;
    std::function<std::uintptr_t(std::uintptr_t)> level;
    std::function<float(std::uintptr_t,unsigned)> location;
    std::function<float(std::uintptr_t)> radius,height;
    std::function<void()> work;
};
namespace ActorOverlapOriginalDetail {
template<class Provider,class... Arguments>
inline auto Read(const OriginalActorOverlapQueries& queries,const Provider& provider,Arguments... arguments) {
    if(!provider)throw std::invalid_argument("Original actor overlap lacks an active native field provider");
    if(queries.work)queries.work();
    return provider(arguments...);
}
} // namespace ActorOverlapOriginalDetail

// AActor.IsOverlapping10369430..10369521. No pose/flag/hash mutation, event,
// primitive query or Float spill. Positive and NEGATIVE finite dimensions are
// both authentic numeric inputs: native squares their retained signed sum.
// Only active Float reads are finite-checked. Missing inactive providers and
// unused Z/height data remain unrestricted. Caller owns shared work/root
// rollback if a provider or arithmetic-domain refusal throws.
inline bool IsOriginalActorOverlapping(std::uintptr_t source,std::uintptr_t other,
    const OriginalActorOverlapQueries& queries) {
    using namespace ActorLineTraceDetail;
    using ActorOverlapOriginalDetail::Read;
    if(!source)throw std::invalid_argument("Original actor overlap requires a nonnull source");
    if(Read(queries,queries.isBrush,source))return false;
    // Original first source-brush return does not dereference Other at all.
    if(!other)throw std::invalid_argument("Original actor overlap reached a null other actor");
    if(Read(queries,queries.isBrush,other))return false;
    if(other==Read(queries,queries.level,source))return false;
    const auto sourceX=Load(Read(queries,queries.location,source,0u));
    const auto dx=Sub(sourceX,Load(Read(queries,queries.location,other,0u)));
    const auto sourceY=Load(Read(queries,queries.location,source,1u));
    const auto dy=Sub(sourceY,Load(Read(queries,queries.location,other,1u)));
    const auto otherRadius=Load(Read(queries,queries.radius,other));
    const auto radius=Add64(otherRadius,Load(Read(queries,queries.radius,source)));
    const auto dySquared=Multiply64(dy,dy);
    const auto dxSquared=Multiply64(dx,dx);
    const auto xySquared=Add64(dySquared,dxSquared);
    const auto radiusSquared=Multiply64(radius,radius);
    if(Compare64(radiusSquared,xySquared)<=0)return false;
    const auto sourceZ=Load(Read(queries,queries.location,source,2u));
    const auto dz=Sub(sourceZ,Load(Read(queries,queries.location,other,2u)));
    const auto otherHeight=Load(Read(queries,queries.height,other));
    const auto height=Add64(otherHeight,Load(Read(queries,queries.height,source)));
    const auto dzSquared=Multiply64(dz,dz);
    const auto heightSquared=Multiply64(height,height);
    return Compare64(heightSquared,dzSquared)>0;
}
} // namespace QuestVr
