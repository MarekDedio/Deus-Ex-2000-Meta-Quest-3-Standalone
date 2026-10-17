#pragma once

#include <cstdint>

namespace QuestVr::ActorSceneSelection {

// These ordinals are pinned by Engine/Actors/UActor.h's EDrawType. The Quest
// renderer also handles DT_RopeSprite/DT_VerticalSprite through its existing
// sprite path; this selector does not claim the original renderer did so.
inline constexpr bool IsSupportedSpriteDrawType(const std::uint8_t drawType) {
    return drawType == 1u || drawType == 4u || drawType == 5u || drawType == 7u;
}

// A scene slot owns geometry and visibility, not permission to interact.
// Preserve the existing gameplay categories even when hidden/DT_None: hidden
// movers can still own collision, and invisible triggers can still own travel.
template<class Snapshot>
constexpr bool HasGameplayRole(const Snapshot& actor) {
    return actor.pawn || actor.inventory || actor.decoration || actor.mover ||
        actor.trigger || actor.travel;
}

// Ordinary Actor subclasses (including script-spawned fragments) can carry
// visible artwork without belonging to any usable category. Asset identity
// only admits a scene candidate: the existing renderer still validates and
// prepares the actual mesh/texture, pose, material and resource budgets.
template<class Snapshot>
constexpr bool HasVisibleAsset(const Snapshot& actor) {
    if (actor.hidden) return false;
    return (actor.drawType == 2u && !actor.meshPath.empty()) ||
        (IsSupportedSpriteDrawType(actor.drawType) && !actor.texturePath.empty());
}

// Camera exclusion and Location admission match the existing Quest policy.
// Callers must independently apply streaming, removal and visibility budgets;
// accepting a render-only actor must never make it usable or highlighted.
template<class Snapshot>
constexpr bool IncludeActorInScene(const Snapshot& actor) {
    return actor.classPath != "Engine.Camera" && actor.hasLocation &&
        (HasGameplayRole(actor) || HasVisibleAsset(actor));
}

enum class ActorSceneGroup : std::uint8_t { Excluded, Mover, Gameplay, Presentation };

// Traverse Mover, Gameplay, then Presentation in that explicit order. New
// render-only births must not consume the existing bounded slots before doors
// or other gameplay actors. Stable source order within each group is retained
// by the caller, as are that caller's streaming/removal rules and capacity.
template<class Snapshot>
constexpr ActorSceneGroup SelectActorSceneGroup(const Snapshot& actor) {
    if (!IncludeActorInScene(actor)) return ActorSceneGroup::Excluded;
    if (actor.mover) return ActorSceneGroup::Mover;
    return HasGameplayRole(actor) ? ActorSceneGroup::Gameplay : ActorSceneGroup::Presentation;
}

} // namespace QuestVr::ActorSceneSelection
