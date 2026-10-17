#include "quest_actor_scene_selection.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace Selection = QuestVr::ActorSceneSelection;
using Group = Selection::ActorSceneGroup;

struct Snapshot {
    std::string objectPath{"Generated.Actor"};
    std::string classPath{"Generated.RenderOnlyActor"};
    bool hasLocation{true};
    bool pawn{}, inventory{}, decoration{}, mover{}, trigger{}, travel{};
    bool hidden{};
    std::uint8_t drawType{};
    std::string meshPath, texturePath;
    bool operator==(const Snapshot&) const = default;
};

std::size_t checks{};
void Require(const bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}

void GameplayRoles() {
    using Flag = bool Snapshot::*;
    constexpr std::array<Flag, 6u> flags{&Snapshot::pawn, &Snapshot::inventory,
        &Snapshot::decoration, &Snapshot::mover, &Snapshot::trigger, &Snapshot::travel};
    for (const auto flag : flags) {
        Snapshot actor;
        actor.*flag = true;
        actor.hidden = true;
        Require(Selection::HasGameplayRole(actor), "Existing gameplay category was lost");
        Require(!Selection::HasVisibleAsset(actor), "Hidden DT_None actor gained visible artwork");
        Require(Selection::IncludeActorInScene(actor), "Hidden gameplay actor lost its scene slot");
        Require(Selection::SelectActorSceneGroup(actor) ==
            (flag == &Snapshot::mover ? Group::Mover : Group::Gameplay),
            "Existing gameplay category gained presentation priority");
        actor.hasLocation = false;
        Require(!Selection::IncludeActorInScene(actor), "Gameplay category bypassed Location admission");
        Require(Selection::SelectActorSceneGroup(actor) == Group::Excluded,
            "Locationless gameplay actor entered a priority group");
        actor.hasLocation = true;
        actor.classPath = "Engine.Camera";
        Require(!Selection::IncludeActorInScene(actor), "Editor Camera entered the gameplay scene");
        Require(Selection::SelectActorSceneGroup(actor) == Group::Excluded,
            "Editor Camera entered a priority group");
    }
    Snapshot unknown;
    Require(!Selection::HasGameplayRole(unknown) && !Selection::IncludeActorInScene(unknown),
        "Unknown invisible actor gained a gameplay category or scene slot");
}

void VisibleAssets() {
    for (unsigned value = 0u; value <= 255u; ++value) {
        for (unsigned assets = 0u; assets < 4u; ++assets) {
            for (const bool hidden : {false, true}) {
                Snapshot actor;
                actor.drawType = static_cast<std::uint8_t>(value);
                actor.hidden = hidden;
                if ((assets & 1u) != 0u) actor.meshPath = "Generated.Mesh";
                if ((assets & 2u) != 0u) actor.texturePath = "Generated.Texture";
                bool expected{};
                if (!hidden) {
                    switch (value) {
                    case 2u: expected = (assets & 1u) != 0u; break;
                    case 1u: case 4u: case 5u: case 7u:
                        expected = (assets & 2u) != 0u; break;
                    default: break;
                    }
                }
                const auto before = actor;
                Require(Selection::HasVisibleAsset(actor) == expected,
                    "Render-only draw type/hidden/asset selection differs");
                Require(Selection::IncludeActorInScene(actor) == expected,
                    "Render-only scene admission differs from visible asset admission");
                Require(Selection::SelectActorSceneGroup(actor) ==
                    (expected ? Group::Presentation : Group::Excluded),
                    "Render-only actor received gameplay priority");
                Require(!Selection::HasGameplayRole(actor), "Visible asset granted a gameplay role");
                Require(actor == before, "Scene selection mutated its snapshot");
                actor.hasLocation = false;
                Require(!Selection::IncludeActorInScene(actor), "Visible asset bypassed Location admission");
                Require(Selection::SelectActorSceneGroup(actor) == Group::Excluded,
                    "Locationless render-only actor entered a priority group");
                actor.hasLocation = true;
                actor.classPath = "Engine.Camera";
                Require(!Selection::IncludeActorInScene(actor), "Visible editor Camera entered the scene");
                Require(Selection::SelectActorSceneGroup(actor) == Group::Excluded,
                    "Visible editor Camera entered a priority group");
            }
        }
    }
}

void CombinedRoles() {
    for (unsigned mask = 0u; mask < 64u; ++mask) {
        Snapshot actor;
        actor.pawn = (mask & 1u) != 0u;
        actor.inventory = (mask & 2u) != 0u;
        actor.decoration = (mask & 4u) != 0u;
        actor.mover = (mask & 8u) != 0u;
        actor.trigger = (mask & 16u) != 0u;
        actor.travel = (mask & 32u) != 0u;
        actor.hidden = true;
        actor.drawType = 255u;
        actor.meshPath = "Generated.Mesh";
        actor.texturePath = "Generated.Texture";
        Require(Selection::HasGameplayRole(actor) == (mask != 0u),
            "Combined gameplay category selection differs");
        Require(Selection::IncludeActorInScene(actor) == (mask != 0u),
            "Unsupported hidden artwork changed existing gameplay category admission");
        const auto expected = (mask & 8u) != 0u ? Group::Mover :
            mask != 0u ? Group::Gameplay : Group::Excluded;
        Require(Selection::SelectActorSceneGroup(actor) == expected,
            "Overlapping gameplay roles displaced mover-first priority");
    }
}

void GeneratedFragments() {
    // Commercial-free shape only. Actual original crate bytecode/births belong
    // to the separate owned-data integration test, not this generated fixture.
    std::array<Snapshot, 6u> fragments;
    for (std::size_t index = 0u; index < fragments.size(); ++index) {
        auto& actor = fragments[index];
        actor.objectPath = "Generated.WoodFragment" + std::to_string(index);
        actor.classPath = "DeusEx.WoodFragment";
        actor.drawType = 2u;
        actor.meshPath = "Generated.FragmentMesh";
        Require(Selection::IncludeActorInScene(actor), "Generated fragment lost its scene slot");
        Require(Selection::HasVisibleAsset(actor), "Generated fragment lost its actual mesh identity");
        Require(!Selection::HasGameplayRole(actor), "Generated fragment became usable by its class spelling");
        Require(Selection::SelectActorSceneGroup(actor) == Group::Presentation,
            "Generated fragment displaced gameplay priority");
        actor.classPath = "Generated.ArbitraryActorSubclass";
        Require(Selection::IncludeActorInScene(actor), "Visible Actor admission special-cased fragment names");
    }
    Snapshot actor;
    actor.drawType = 3u; // DT_Brush alone is not a supported render-only mesh.
    actor.meshPath = "Generated.Mesh";
    actor.texturePath = "Generated.Texture";
    Require(!Selection::IncludeActorInScene(actor), "Render-only DT_Brush gained unimplemented brush handling");
    actor.mover = true;
    Require(Selection::IncludeActorInScene(actor), "Gameplay mover lost its independent brush/collision path");
    Require(Selection::SelectActorSceneGroup(actor) == Group::Mover,
        "Gameplay mover lost its first-priority scene group");
}
} // namespace

int main() {
    try {
        GameplayRoles();
        VisibleAssets();
        CombinedRoles();
        GeneratedFragments();
        std::cout << "PASS actor scene selection: " << checks
            << " checks; all gameplay roles, byte draw types, hidden/assets, Camera/Location gates and "
               "generated render-only fragments without interaction roles. Not original crate damage, "
               "startup, physics or GPU evidence.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Actor scene selection failed: " << error.what() << '\n';
        return 1;
    }
}
