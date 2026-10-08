# Deus Ex Quest VR Port

This workspace is an independent ARM64/OpenXR runtime project for running a
user-owned copy of Deus Ex on Meta Quest 3. It does not modify or redistribute
the original game.

## Current milestone

The Android ARM64/OpenXR app has been exercised on Quest 3 with textured
training geometry, VR locomotion/interaction, UI, audio and save/load features.
The latest offline work adds original static BSP shadow lightmaps and fixes
actor skin selection, placement, masking and mover textures, with repeatable
software close-ups. Original vertex-animation data now drives shared initial
pose sampling, including authored corpse frames. A bounded original-bytecode
interpreter now executes isolated animation helpers and stages their native
commands transactionally; automatic startup, state execution and live ticking
are still unfinished. Version-4 runtime saves now preserve these supported
actor properties and complete native animation clocks. These latest GPU changes still
require headset validation.
The full campaign is not yet verified playable: animation, complete gameplay
scripting, visual fidelity and device performance remain in development.
See [current evidence and limitations](docs/STATUS.md).
See [original script execution](docs/PORTABLE-SCRIPT-EXECUTION.md) for the
supported interpreter path and tests, and [script-state saves](docs/SCRIPT_STATE_SAVE.md)
for the scoped persistence boundary and remaining map-archive requirements.
The [installed Quest test build](docs/QUEST-TEST-BUILD.md) remains frozen while
the user tests it. Subsequent offline source work retains
[authored state metadata](docs/AUTHORED-STATE-FOUNDATION.md); it does not enable
automatic AI or replace that installed APK.
The subsequent [script-dispatch foundation](docs/SCRIPT-DISPATCH.md) adds
read-only state/function selection, label analysis and eligibility suppression,
not startup or live state execution.

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
4. All 88 catalog maps decode in desktop world-cache audits; campaign-wide stereo rendering remains to verify on Quest.
5. Training interactions are implemented; original mission/campaign scripting remains incomplete.
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
the pinned SDK checkout before invoking Gradle. It now restores the exact source
dependencies from `third-party-lock.json` on a fresh checkout, preserving any
existing dependency edits. See [build dependencies](docs/BUILD-DEPENDENCIES.md)
for toolchain paths and validation-only checks.

```powershell
.\tools\Build-QuestSmokeTest.ps1
```

## Visual tests without a headset

The desktop capture tool decodes the original packages with the same portable
map-cache code as Quest and renders their BSP and material albedo to BMP files.
Optional `-AuthoredLighting` previews use the shared Quest light evaluator with
real map actors; these are direct vertex-light approximations, not original
lightmaps or shadow reproduction.
It also runs deterministic depth, clipping, transparency, texture-coordinate,
image-baseline, and shared VR-transform regression checks. It does not require
ADB, a connected headset, or redistribution of the game's data.

Quicksaves now use paired, alternating generations with corruption recovery,
and new saves retain map-local position/heading across tracking-origin changes.
See [save recovery](docs/SAVE-RECOVERY.md) for the modeled state and remaining
on-device checks, and [authored lighting](docs/AUTHORED-LIGHTING.md) for fidelity
limits. Neither feature establishes full campaign compatibility.

```powershell
.\tools\Initialize-ThirdParty.ps1
.\tools\Test-DesktopVisuals.ps1 -GameRoot 'D:\Steam\steamapps\common\Deus Ex'
.\tools\Test-DesktopCampaign.ps1 -GameRoot 'D:\Steam\steamapps\common\Deus Ex' -SkipBuild
.\tools\Test-DesktopPersona.ps1 -GameRoot 'D:\Steam\steamapps\common\Deus Ex' -Page Health -SkipBuild
```

World captures do not yet contain actor meshes, authored illumination, skyboxes,
Persona UI, or the live Quest renderer. A separate Persona preview shares the
Quest CPU compositor and renders the original Inventory, Health,
Goals/Notes, and Logs artwork, bitmap fonts, and button chrome with explicit
sample text. It does not verify live interaction. Neither mode verifies gameplay, stereo,
OpenXR tracking, controller input, or Quest performance. Black skies or absent
actors in this limited renderer are not evidence of the same defect in the APK.
See [desktop visual testing](docs/DESKTOP-VISUAL-TESTING.md) for camera controls,
baseline comparisons, reports, and the validation boundary.

## Testing on Quest

With one Quest in developer mode connected and authorized over USB:

```powershell
.\tools\Install-QuestSmokeTest.ps1
```

Quest OS capture can return black for immersive compositor layers. The app has
an independent left-eye framebuffer capture path that includes the world,
actors, controllers, and TinyUI HUD. With the app running, request and pull a
BMP over ADB with:

```powershell
.\tools\Capture-QuestScreenshot.ps1
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
- left Menu button: open or close the paused VR inventory panel;
- B: asynchronously cache and load the next catalog map;
- Y: quick-save world, inventory, position, and facing state;
- X: quick-load and rebuild the live actor scene.

A head-locked HUD displays health, inventory count, and the control summary. The
Menu button opens a head-locked Persona-style screen using the original game's
shipped artwork, default grayscale theme, tab rail, slot grid, and item-data
panes. Original `FontMenuHeaders` and `FontMenuSmall` glyphs are composed into
the panel texture with the shipped navigation and action-button artwork.
Its client/border offsets and 640x480 layout follow the serialized
original UI defaults. It shows the live map, health, and up to thirty items
in the original five-by-six grid. Right-stick up/down
selects inventory items; left/right switches between the
functional Inventory, Health, Goals/Notes, and Logs pages. Health shows live health,
credits, skill points, and inventory count. Goals/Notes reads the same
conversation progress persisted by quick-save/load, while Logs retains recent
NPC lines and JC responses across quick-save/load. A equips weapons or
consumes healing items, and B or Menu returns to play. Movement, turning,
combat, interaction, and map exits pause while this panel is open; quick-save
and quick-load remain available.
The page backgrounds and borders are not recreated substitutes: the app decodes
the shipped Inventory, Health, Goals, Logs, and Conversations border textures
directly from the user's `System/DeusExUI.u` on Quest. Each page uses its original
client/border offsets; Health includes the original neutral body illustration,
Goals/Notes has stacked panes, and Logs has one centered text column. A text-only fallback remains available
if those private game assets cannot be decoded. The original eight-tab rail is
visible, but only Inventory, Health, Goals/Notes, and Logs are functional;
augmentations, skills, conversations, and images are visibly dimmed.
Action captions are adapted to VR bindings, not a claim that every original
desktop action or pointer-clickable control is implemented.
Body-part health and full original menu behavior are still
unfinished; the body illustration does not imply body-part damage simulation.
The new font/chrome integration has desktop CPU visual checks and an Android
build gate; its readability and behavior on Quest still require hardware testing.
See [Quest diagnostics](docs/QUEST-DIAGNOSTICS.md) for safe screenshot/map requests.
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
