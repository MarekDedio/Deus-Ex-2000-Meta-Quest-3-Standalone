# Deus Ex Quest VR Port

This workspace is an independent ARM64/OpenXR runtime project for running a
user-owned copy of Deus Ex on Meta Quest 3. It does not modify or redistribute
the original game.

## Current milestone

The native Android ARM64 app launches on Quest 3, reads user-supplied UE1
packages through a portable SurrealEngine-derived runtime, renders the textured
training BSP and actors at player scale with per-map colored point and spotlight
illumination, and submits them stereoscopically
through OpenXR. Locomotion, collision, controller interaction, pickups,
inventory persistence, controller hitscan, pawn health/death, spatial ambient audio,
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
   saves and spatial audio are usable in VR.
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
- A: use the pointed actor, or consume the selected healing item when no actor
  is targeted;
- right index trigger: controller-aimed pawn damage when the selected firearm
  has a compatible ammo pickup (melee weapons need no ammo);
- right grip: cycle the selected inventory item;
- B: asynchronously cache and load the next catalog map;
- Y: quick-save world, inventory, position, and facing state;
- X: quick-load and rebuild the live actor scene.

A head-locked HUD displays health, inventory count, and the control summary.
Pointing at a pawn and pressing A resolves its real `BindName` against the
active mission's serialized conversation events and displays the shipped
subtitle while decoding and mixing its referenced MP3 speech over ambient audio;
Training uses the game's mission `-1` conversation bucket.
Real `Engine.Teleporter` and `DeusEx.MapExit` actors decode their URL/DestMap
properties and initiate catalog-validated travel when entered. Quick-saves now
record the active map and can restore across a later level transition.
Runtime-state v2 also persists live player health and partially damaged pawn
health, while continuing to read older v1 runtime saves.
The HUD names the selected inventory item. Trigger attacks require a selected
weapon, use weapon-family damage, and constrain melee weapons to arm's reach.

The training scene has been measured at a steady 72 fps on a physical Quest 3.
Serialized ambient emitters now follow their real map positions, radius,
volume, and pitch with head-relative stereo panning and distance attenuation;
they are replaced in the background with each map transition.
NPC dialogue is likewise positioned at its serialized speaker while JC's
spoken conversation choices remain head-centered for comfort and clarity.
The BSP shader now bakes each active map's serialized light actors into vertex
illumination, including brightness, radius, color, and spotlight cones. UE1
lightmap textures and shadow/occlusion fidelity are still under development.
The first campaign map, `01_NYC_UNATCOIsland`, also stabilizes at 72 fps with
proximity-streamed actors, incremental BSP/texture uploads, and an eight-meter
collision grid. Its measured worst transition frame is 41.66 ms, down from the
original 1.22 seconds; steady-state worst frame time is 13.89 ms.
Generic visual map-cache generation and runtime/GPU transitions now work and
have been physically verified for Training to TrainingCombat and back. The
runtime also follows decoded teleporter/map-exit destinations. The remaining
major work is broader UnrealScript/native execution, AI/conversations/missions,
full UI and inventory presentation, animation, weapon and interaction audio, further transition
comfort, and end-to-end campaign validation.
