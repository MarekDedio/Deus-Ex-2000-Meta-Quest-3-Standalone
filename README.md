# Deus Ex Quest VR Port

This workspace is an independent ARM64/OpenXR runtime project for running a
user-owned copy of Deus Ex on Meta Quest 3. It does not modify or redistribute
the original game.

## Current milestone

The native Android ARM64 app launches on Quest 3, reads user-supplied UE1
packages through a portable SurrealEngine-derived runtime, renders the textured
training BSP and actors at player scale, and submits them stereoscopically
through OpenXR. Locomotion, collision, controller interaction, pickups,
inventory persistence, controller hitscan, pawn health/death, ambient audio,
and quick-save/load are live. Campaign-wide gameplay compatibility remains in
development.

## Data boundary

- Source code, build products, and tests live under `QuestVRPort/`.
- Original maps, textures, music, audio, and compiled scripts remain outside the
  port source tree.
- Release packages must never contain Deus Ex assets.
- A player must provide their own legally obtained installation data.
- The Windows executables and DLLs are not used on Quest.

## Milestones

1. ARM64 Android/OpenXR application launches on Quest 3.
2. Runtime discovers and validates user-supplied game data.
3. UE1 packages, names, imports, exports, and properties can be read.
4. Active-map geometry and textures render stereoscopically from all 88 catalog entries.
5. UnrealScript and Deus Ex native functions support the training campaign.
6. Motion controls, interaction, weapons, inventory, HUD, conversations, and
   saves are usable in VR.
7. All campaign maps pass progression, performance, and comfort testing.

This is a long-term engine port. A smoke-test APK is not presented as a playable
game build.

## Build the current Quest smoke test

The checked-in Android project references the pinned Meta OpenXR SDK checkout in
`third_party/Meta-OpenXR-SDK`. On this workstation, the reproducible toolchain is
installed at `D:\Android\Sdk` with Microsoft OpenJDK 17.
The build helper idempotently applies the checked-in TinyUI font-path patch to
the pinned SDK checkout before invoking Gradle.

```powershell
.\tools\Build-QuestSmokeTest.ps1
```

With one Quest in developer mode connected and authorized over USB:

```powershell
.\tools\Install-QuestSmokeTest.ps1
```

The current APK loads the training map's package tables, BSP, materials, actor
meshes, actor textures, scripts, and ambient sound on-device. Controls are:

- left stick: smooth movement with BSP ground following and wall collision;
- right stick: 30-degree snap turning;
- A: use the pointed actor, including pickups and typed actor interactions;
- right index trigger: controller-aimed pawn damage;
- right grip: cycle the selected inventory item;
- B: asynchronously cache and load the next catalog map;
- Y: quick-save world, inventory, position, and facing state;
- X: quick-load and rebuild the live actor scene.

A head-locked HUD displays health, inventory count, and the control summary.
Real `Engine.Teleporter` and `DeusEx.MapExit` actors decode their URL/DestMap
properties and initiate catalog-validated travel when entered. Quick-saves now
record the active map and can restore across a later level transition.
Runtime-state v2 also persists live player health and partially damaged pawn
health, while continuing to read older v1 runtime saves.
The HUD names the selected inventory item. Trigger attacks require a selected
weapon, use weapon-family damage, and constrain melee weapons to arm's reach.

The training scene has been measured at a steady 72 fps on a physical Quest 3.
Generic visual map-cache generation and runtime/GPU transitions now work and
have been physically verified for Training to TrainingCombat and back. The
remaining major work is progression-driven (rather than debug-button) travel,
broader UnrealScript/native execution, AI/conversations/missions, full UI and
inventory presentation, animation, spatial audio, transition comfort, and
campaign validation.
