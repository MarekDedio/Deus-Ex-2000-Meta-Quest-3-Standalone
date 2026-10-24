#pragma once

#include <cstdint>

namespace QuestVr {

// Owned GOTY Engine.dll AActor::Tick non-Pawn suffix, 103A23CE and
// 103A275C..103A2783. This answers ONLY whether an already-reached physics
// phase dispatches performPhysics. It is not Actor/Level Tick: the caller
// still owns animation, selected callbacks, expiry, owner deferral, stasis
// and tick-type branches, including which of those phases a role path skips.
// Supply the actual unsigned native bytes, with Physics/Role fresh after any
// earlier callbacks. Do not clamp unknown bytes to an enum or add a deletion
// gate. RemoteRole3 bypasses this suffix even for Authority Role4.
[[nodiscard]] constexpr bool OriginalNonPawnPhysicsPhaseEligible(
    std::uint8_t remoteRole,std::uint8_t role,std::uint8_t physics) noexcept {
    if(remoteRole==3u)return false;
    if(role<2u)return physics==2u;
    return physics!=0u&&role!=3u;
}

} // namespace QuestVr
