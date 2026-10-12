# Deus Ex Quest VR Port

This workspace is an independent ARM64/OpenXR runtime project for running a
user-owned copy of Deus Ex on Meta Quest 3. It does not modify or redistribute
the original game.

## Current milestone

The immediate focus is a short, recordable Training route: the original opening
instructor transmissions, smoothly moving paired doors, and the desk key/keyed
door. See [Training demo scope and acceptance](docs/TRAINING-DEMO.md). This is a
bounded VR interaction layer, not completion of all three tutorial maps or the
original mission scheduler. Headset acceptance is recorded separately from host
logic tests.

The latest Training work adds the original remote-speech panel and portraits,
original-style interaction brackets, footprint-based stair movement and bounded
live door updates. These fixes still require the physical recording-route
checklist; the original face-to-face dialogue UI remains unfinished.

The Android ARM64/OpenXR app has been exercised on Quest 3 with textured
training geometry, VR locomotion/interaction, UI, audio and save/load features.
The latest offline work adds original static BSP shadow lightmaps and fixes
actor skin selection, placement, masking and mover textures, with repeatable
software close-ups. Original vertex-animation data now drives shared initial
pose sampling, including authored corpse frames. A bounded original-bytecode
interpreter now executes isolated animation helpers and stages their native
commands transactionally. Explicit persistent state slices now support bounded
GotoState, state locals, synchronous entry/exit callbacks and Enable/Disable;
automatic startup, latent/AI behavior and live ticking are still unfinished.
Version-4 runtime saves preserve supported actor properties/native clocks;
version 5 additionally retains the portable state-frame model. These isolated
source checks do not establish live gameplay or device performance.
The full campaign is not yet verified playable: animation, complete gameplay
scripting, visual fidelity and device performance remain in development.
See [current evidence and limitations](docs/STATUS.md).
See [original script execution](docs/PORTABLE-SCRIPT-EXECUTION.md) for the
supported interpreter path and tests, and [script-state saves](docs/SCRIPT_STATE_SAVE.md)
for the scoped persistence boundary and remaining map-archive requirements.
The [runtime actor birth foundation](docs/ACTOR-SPAWN.md) now executes native
Spawn with original callbacks, bounded collision registration and transactional
rollback. Codec v5 / checkpoint v8 cold-restore real born identities and frozen
defaults. The original InitializeInventory helper creates and links a real
weapon/ammo graph in isolated host tests; automatic StartUp and actual player
possession remain unfinished. This is not full campaign startup or newly accepted
Quest gameplay. Runtime actor texture replacement is ARM-compiled but still
requires device performance and visual verification.
The [native AI event-state foundation](docs/AI-EVENT-STATE.md) adds real
manager initialization, callback registrations and sensory history mutations
with rollback and codec6/checkpoint9 graph snapshots. It does not yet implement
AI processing, perception, deferred cleanup or automatic level ticking.
The [original blend-animation commands](docs/BLEND-ANIMATION.md) now execute
PlayBlendAnim1010 and TweenBlendAnim1012 with original signed slots, negative
tween frames and Plane packing. Isolated original head-turn helpers pass;
automatic NPC animation and world startup are still unfinished.
Animation calls no longer cache meshes without texture paths; cold original
regressions and inspected textured Doctor/Jaime captures verify that render fix.
The [original Switch/Case interpreter path](docs/SWITCH-CASE.md) now executes
selector-directed cases, fallthrough and live state callbacks transactionally.
The unchanged DeusEx head-turn override passes all directions and default;
The [original random stream](docs/SCRIPT-RANDOM.md) now implements Rand167 and
FRand195, executes the original scripted RandRange and both Bird idle branches,
and preserves global random continuation through transactional failures and
codec7/checkpoint10 saves, including saving before the first draw.
The [original actor Sleep continuation](docs/SCRIPT-SLEEP.md) now commits actual
Sleep256 waits, polls them explicitly, preserves signed native timers and
StopWaiting behavior, and retains waits in codec8/checkpoint11 saves.
The [original landing wait](docs/SCRIPT-LANDING.md) adds WaitForLanding527,
falling-only poll528 and dynamic LongFall using that same saved native counter.
Explicit StartUp can yield at its original Sleep and continue through landing;
remaining startup dependencies are still refused transactionally.
This is isolated backend progress, not automatic world startup or live NPC AI.
The [original actor timer phase](docs/ACTOR-TIMER-CONTRACT.md) now implements
SetTimer280 and explicit once-per-tick callback coalescing, with reflected
save/restore, state-aware dispatch and atomic callback rollback. Timer-only writes
preserve animation and avoid geometry publication. This source-only checkpoint
does not enable automatic campaign ticking or replace the installed visual-test
APK; original Training startup still reports an exact later class-default gap.
The [original state-dispatch correction](docs/ORIGINAL-STATE-DISPATCH.md) now
uses actual Children/Next Auto order and explicit selected-state SuperField
callback ancestry. Full state identity/control and campaign bootstrap remain
unfinished; recorded headset test receipts identify previously accepted builds.
The [independent actor-state model](docs/STATE-CONTROL-MODEL.md) adds exact
selected/running identities, nullable Code, live masks and codec9/envelope12.
Original transition authoring now uses callback flags, pre-argument capture and
selected-node label ancestry; owned Spawn frame initialization is wired.
Map-wide startup remains required; ambiguous legacy frames are not silently
upgraded. These shared controls do not establish a running original campaign.
Script Spawn now preserves native callback-visible Class.Name and applies the
captured optional tag after return, including notification replacements.
The [original BSP point-trace prerequisite](docs/MODEL-POINT-TRACE.md) and
[decal native audit](docs/DECAL-NATIVE-CONTRACT.md) prepare exact NPC shadows;
Positive CPU AttachDecal now publishes original callback-visible native records
transactionally; rendered shadows and complete original startup remain pending.
The [decal geometry and ordered-state prerequisites](docs/DECAL-GEOMETRY-PLAN.md)
now compose original projection, the full sixteen-sample Shadow grid and node
clipping, with live-texture record ordering and exact duplicate/detach controls.
These are host-tested foundations, not published native shadows or Quest startup.
The [native decal checkpoint integration](docs/DECAL-CHECKPOINT.md) adds bounded
dynamic INT arrays, map-owned GC/journal state, original native Detach and exact
codec10/envelope13 persistence. Rendered native shadows remain pending.
The [native client binding](docs/NATIVE-CLIENT-BINDING.md) now owns Quest's actual
stereo viewport lifetime and original Attach entry guards. It does not invent
player possession or complete original world startup.
The [Quest test build notes](docs/QUEST-TEST-BUILD.md) record an earlier installed
test build, not a permanent deployment freeze. At the user's standing request,
every successful new APK build now automatically installs on the connected,
authorized Quest 3. Offline source work retains
[authored state metadata](docs/AUTHORED-STATE-FOUNDATION.md); this does not enable
automatic AI or establish on-device campaign completion.
The subsequent [script-dispatch foundation](docs/SCRIPT-DISPATCH.md) adds
read-only state/function selection, label analysis and eligibility suppression,
not automatic startup. [Explicit state execution](docs/STATE-EXECUTION.md) adds
transactional state control and persistence, still without a live AI scheduler.
[Original authored struct support](docs/AUTHORED-STRUCTS.md) connects declaration
order and package-local member values to typed runtime writes and saves,
including the eight original InitialInventory slots. Complete authored player
inventory and campaign startup remain unfinished; backend tests do not establish
live headset gameplay.
[Original object/class casts](docs/OBJECT-CASTS.md) now have distinct identity
and declaration-name rules, covered by the final original-data integration.

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
5. Introductory Training interactions are under acceptance; the full tutorial and original mission/campaign scripting remain incomplete.
6. Motion controls, interaction, weapons, inventory, HUD, conversations, and
   saves and spatial audio are usable in VR.
7. All campaign maps pass progression, performance, and comfort testing.

This is a long-term engine port. A smoke-test APK is not presented as a playable
game build.

## Build the current Quest smoke test

The checked-in Android project references the pinned Meta OpenXR SDK checkout in
`third_party/Meta-OpenXR-SDK`. On this workstation, the reproducible toolchain is
installed at `D:\Android\Sdk` with Microsoft OpenJDK 17.
The build helper idempotently applies the checked-in TinyUI font-path and
OpenXR/Android lifecycle-order patches to
the pinned SDK checkout before invoking Gradle. It now restores the exact source
dependencies from `third-party-lock.json` on a fresh checkout, preserving any
existing dependency edits. See [build dependencies](docs/BUILD-DEPENDENCIES.md)
for toolchain paths and validation-only checks.

```powershell
.\tools\Build-QuestSmokeTest.ps1
```

After a successful build, the helper checks for authorized Quest 3
`2G0YC5ZG620985` and replace-installs the new APK without clearing saves or
automatically launching the app. It never guesses another phone/headset.
Use `-DeviceSerial <serial>` for another verified Quest 3, or `-SkipInstall`
for an explicit offline/CI build. Missing ADB or an absent/unauthorized headset
prints a skipped-install result and retains the built APK; an attempted install
failure is reported as an error. `-ValidateOnly` never contacts a device.
Installation is not proof of rendering, tracking or campaign playability.

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

The manual helper verifies the selected device is a Quest 3, installs with `-r`,
and launches by default. Pass `-InstallOnly` to omit explicit force-stop/launch
commands. Installing an update can itself stop the app; reopen it afterward.
With multiple devices, explicitly provide `-DeviceSerial <serial>`.

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
Training uses the original Mission00 conversation bucket (`0`). Its opening
datalinks resolve by exact authored name rather than the generic NPC-line cursor.
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
