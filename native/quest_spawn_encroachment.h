#pragma once

#include "portable_model_geometry.h"

#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

enum class OriginalSpawnEncroachmentOutcome { Continue,ReturnNone };

struct OriginalSpawnEncroachmentQueries {
    // These are CURRENT native Actor fields after PostBeginPlay, not the
    // original SpawnActor input arguments or an earlier captured actor pose.
    // Channels are Pitch/Yaw/Roll and X/Y/Z respectively. Location is copied
    // without arithmetic; inactive NaN/Infinity/signed-zero words are legal.
    std::function<std::int32_t(std::uintptr_t,unsigned)> rotationChannel;
    std::function<float(std::uintptr_t,unsigned)> locationChannel;
    // Both native calls use the SAME retained SpawnActor Level and Actor.
    // Their final DWORDs are literal0. Providers must join the caller's real
    // Level/hash/Execution root; mock providers do not implement a runtime.
    std::function<std::int32_t(std::uintptr_t,std::uintptr_t,PortableModelVec3,
        std::array<std::int32_t,3u>,std::int32_t)> check;
    std::function<std::int32_t(std::uintptr_t,std::uintptr_t,std::int32_t)> destroy;
    // Optional only for detached controls; a real adapter supplies its one
    // borrowed root ledger. No new allowance/transaction is opened here.
    std::function<void()> work;
};

namespace SpawnEncroachmentDetail {
template<class Function,class... Arguments>
inline auto Call(const OriginalSpawnEncroachmentQueries& queries,const Function& function,
    Arguments... arguments) {
    // Only providers on the reached original path are required or charged.
    if(!function)throw std::invalid_argument("Original Spawn encroachment lacks an active provider");
    if(queries.work)queries.work();
    return function(std::forward<Arguments>(arguments)...);
}
} // namespace SpawnEncroachmentDetail

// Complete original ULevel.SpawnActor10395591..103955EE fragment, AFTER
// PostBeginPlay and BEFORE SetInitialState/FindBase/PostPostBeginPlay. Native
// arg+34 is a full integer: ANY nonzero skips all pose/query/destruction work.
// There is no new bDeleteMe/source-flags filter or native actor-array snapshot.
// Provider failures propagate for the enclosing root to roll back. An ordinary
// true check preserves its callback effects and calls real DestroyActor(Actor,0)
// before returning None REGARDLESS of DestroyActor's native result. The helper
// does not itself create/delete objects or claim a complete SpawnActor port.
inline OriginalSpawnEncroachmentOutcome CheckOriginalSpawnEncroachment(std::uintptr_t level,
    std::uintptr_t actor,std::int32_t nativeSkip,const OriginalSpawnEncroachmentQueries& queries) {
    if(!level||!actor)
        throw std::invalid_argument("Original Spawn encroachment requires its retained nonnull Level and Actor");
    if(nativeSkip)return OriginalSpawnEncroachmentOutcome::Continue;
    using SpawnEncroachmentDetail::Call;
    std::array<std::int32_t,3u> rotation;
    rotation[0u]=Call(queries,queries.rotationChannel,actor,0u);
    rotation[1u]=Call(queries,queries.rotationChannel,actor,1u);
    rotation[2u]=Call(queries,queries.rotationChannel,actor,2u);
    PortableModelVec3 location;
    location.x=Call(queries,queries.locationChannel,actor,0u);
    location.y=Call(queries,queries.locationChannel,actor,1u);
    location.z=Call(queries,queries.locationChannel,actor,2u);
    if(!Call(queries,queries.check,level,actor,location,rotation,std::int32_t{}))
        return OriginalSpawnEncroachmentOutcome::Continue;
    static_cast<void>(Call(queries,queries.destroy,level,actor,std::int32_t{}));
    return OriginalSpawnEncroachmentOutcome::ReturnNone;
}

} // namespace QuestVr
