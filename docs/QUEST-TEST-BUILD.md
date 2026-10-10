# Current Quest test-build handoff (2026-10-10)

The latest **backend-only, not installed** original-RNG APK is
`artifacts/release-random-state-20261010/DeusExQuestVR-random-state-20261010.apk`:
18,681,297 bytes, SHA-256
`3DA281F523885D6AE25D9BCF14682527B78080EE573EF461E9381816D49293E1`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass; its
16 ZIP entries contain no original commercial game packages. Packaged data-probe:
5,595,032 bytes, SHA-256
`5039FE7EEA0DF45ED8376F816234410E1DC63ECC21566E564CA30AAF00785BDB`;
quest library:5,122,392 bytes, SHA-256
`F15E9D185D91E79C47C81E82031D4E4C69DF709CD85DBAB0239BA221AF43748B`.
It adds original Rand/FRand, actual scripted RandRange/Bird branches and codec7 /
checkpoint10 continuation, including pre-first-draw saves and global map carry.
Explicit StartUp now refuses unsupported Sleep256 at PC6 with full rollback.
This is not automatic startup, live NPC AI or new device acceptance. The installed,
physically accepted seated-player APK remains unchanged. See
[SCRIPT-RANDOM.md](SCRIPT-RANDOM.md).
All 49 ordinary host tests and all three separate original-data integrations pass.
RNG controls pass 58,942 / 37 rejections; codec controls pass 6,337 / 5,634.

The preceding **backend-only, not installed** Switch/Case APK is
`artifacts/release-switch-case-20261010/DeusExQuestVR-switch-case-20261010.apk`:
18,681,209 bytes, SHA-256
`AEB64340ED1C88BFD549D2363615B555D8ACE8B35DC2B385E813799CD0AB1465`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass; its
16 ZIP entries contain no original commercial game packages. The data-probe
library is 5,588,496 bytes, SHA-256
`3F6A3CDC6E8E151D78F71227E2855BA0A5807A92F81FBF19F29683D38AB5E36E`.
It adds original Switch/Case control, live state callback/alias safety and
transactional fallthrough. All 8,208 original functions pass structural
inspection; the original DeusEx head-turn override executes all directions.
In that increment StartUp reached unsupported FRand195 at PC9 and rolled back fully;
the later original-RNG increment above implements it.
This is not automatic world startup, working NPC AI or a new device acceptance.
The installed, physically accepted seated-player APK remains unchanged.
All 48 ordinary host tests and all three separate original-data integrations
pass on the final production source. State controls pass 208 / 32 rejections;
Switch-specific synchronous controls pass 420 / 58.
See [SWITCH-CASE.md](SWITCH-CASE.md) for scoped tests and evidence.

The preceding **backend-only, not installed** blend-animation APK is
`artifacts/release-blend-animation-20261010/DeusExQuestVR-blend-animation-20261010.apk`:
18,681,209 bytes, SHA-256
`E9500D83B97621DF0AE55D1BF02DAD1F3D4DA28FF34EFBFD585E66952FA9E8EA`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass; its
16 ZIP entries contain no original commercial game packages. The data-probe
library is 5,579,312 bytes, SHA-256
`683A63DA9264AF546A912300608EEF998745A385E8EFF4E3F554032C38D59B96`.
It adds original PlayBlendAnim1010/TweenBlendAnim1012 commands, corrected tween
frames/Plane packing, bounded older-clock restoration and complete cold mesh
material caching. Offline native-command Doctor/Jaime captures were inspected
with zero missing material selections; this is not new Quest GPU evidence.
It does not enable live animation ticking or finish original world startup. The installed,
physically accepted seated-player APK remains unchanged; this increment has
no new headset performance/visual acceptance. See [BLEND-ANIMATION.md](BLEND-ANIMATION.md).
All 48 ordinary host tests and all three separate original-data integrations
pass; 200 pure clock controls and the exact legacy import budget boundary pass.

The preceding **backend-only, not installed** native AI event-state APK is
`artifacts/release-ai-event-state-20261010/DeusExQuestVR-ai-event-state-20261010.apk`:
18,681,209 bytes, SHA-256
`2419A01F160BDEBB1A518862FBD3ABED5EF4EC0A61942E26A900E9A72132916B`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass; its
16 ZIP entries contain no original commercial game packages. The packaged
native libraries differ from the lookup build (data-probe 5,569,672 bytes,
SHA-256 `34004A11B3CC0C0D49EA8E207447346B275BBC3B0FCDF0402DC7E38CEF7AFF78`).
It adds manager initialization/registration/emission state, sensory histories,
transactional GC ownership and codec6/checkpoint9 snapshots, not AI processing
or a finished campaign. The 48 ordinary host tests pass (three optional original
data tests are verified separately). This batch is not device-tested or installed
over the accepted seated-player APK. See [AI-EVENT-STATE.md](AI-EVENT-STATE.md).

The preceding **backend-only, not installed** actor-lookup APK is
`artifacts/release-actor-lookup-20261009/DeusExQuestVR-actor-lookup-20261009.apk`:
18,681,209 bytes, SHA-256
`F812793EA19300A794EE8901623068C9BA69370C6FAC1302A7EED1EA92637DD7`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass;
its 16 ZIP entries contain no original commercial game packages. Packaged port
native-library hashes differ from the preceding spawn build. It adds bounded
synchronous foreach/AllActors304, linked-PlayerPawn fallback720, vector221/225
and valid inactive-reference assignments. All 47 ordinary host tests and all
three separate original-data integrations pass. Persistent iterator/AI/player
possession/world-startup work remains unfinished. This backend batch has not
been device-verified or installed over the accepted seated-player APK below.
See [ACTOR-LOOKUP.md](ACTOR-LOOKUP.md).

The preceding **backend-only, not installed** actor-spawn APK is
`artifacts/release-actor-spawn-20261009/DeusExQuestVR-actor-spawn-20261009.apk`:
18,681,209 bytes, SHA-256
`7B24F692B5F8D10FC33D10DF38944898A2D0ADAB0B36FA1CD48701599252ED67`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass;
its 16 ZIP entries contain no original commercial game packages. It adds real
transactional actor births, checkpoint-v8 cold graphs, collision registration
and staged replacement actor materials. The material path and performance
are not yet device-verified; automatic authored player/world startup remains
unfinished. See [ACTOR-SPAWN.md](ACTOR-SPAWN.md). The accepted installed
player-controls APK below remains unchanged.

The preceding **backend-only, not installed** lifecycle APK is
`artifacts/release-actor-lifecycle-20261009/DeusExQuestVR-actor-lifecycle-20261009.apk`:
18,621,265 bytes, SHA-256
`AFA77AAFD9CAA1AE3C24562F2BF95CF48C7C2FDA2FBF5D96BD5A60D323742E4F`.
Android ARM64 compilation and signature scheme v2 verification pass; no original
commercial game packages are embedded. It adds native ownership/attachment/
deletion callbacks, checkpoint-v7 topology and committed geometry/ambient
publication. These backend operations are host/original-data tested, not newly
accepted on-device gameplay. Full Spawn/startup/campaign remain unfinished.
See [ACTOR-LIFECYCLE.md](ACTOR-LIFECYCLE.md). The installed player-controls APK
below remains unchanged and has a second user-confirmed automatic seated-height
acceptance recorded in [PLAYER-VR.md](PLAYER-VR.md).

The preceding **backend-only, not installed** class-default build is available at
`artifacts/release-class-defaults-20261009/DeusExQuestVR-class-defaults-20261009.apk`.
Its SHA-256 is `DE69AC761B7D6A6D01FDEE9C024348E244191D59D5213F1498BCF7BBAE5F9BB2`
and size is 18,600,785 bytes. Android compilation and APK Signature Scheme v2
verification pass; no original commercial packages are embedded. It adds
transactional concrete-class defaults/native283/checkpoint-v6 backend support,
not new visible player controls or complete campaign startup. See
[CLASS-DEFAULTS.md](CLASS-DEFAULTS.md). The installed, physically accepted
player-controls handoff remains the build described below.

This player-controls APK replaces the frozen asynchronous-startup build
`25e38b3`. It is installed on the user's Quest 3, not a finished full-campaign
release. Original commercial game data remains in the existing
private app directory; updating did not uninstall or clear game data or saves.

Package: `dev.deusex.questvr.smoketest` (Android ARM64 debug build).
APK size: 18,600,785 bytes.
SHA-256: `5B1B253765F3F31B80EA5575AD3865472232DBA57AD2837F47F429D0C4AFF403`.
APK Signature Scheme v2 verifies. The APK contains port/SDK libraries and SDK
UI assets, not original `.dx`, `.u`, `.utx`, `.uax` or `.umx` game packages.

## What was verified

- Android/OpenXR initialization returns before expensive runtime preparation.
  Original CPU lighting and staged Training world/actor uploads then complete
  while the process stays running with valid tracking.
- Actual Quest eye captures show lit Training geometry, HUD, and original-art
  Inventory, Health, Goals/Notes and Logs pages with readable bitmap text.
- Idle measurement windows reach 72 fps. Initial staged uploads and diagnostic
  screenshot readbacks still have frame spikes; this is not a general
  performance certification or a full-map visual audit.
- 43 ordinary host tests pass. Two optional original-data integrations skip
  unless supplied the game root; their separate full original-data checks passed
  separately with the final runtime source. The startup ownership path was source
  reviewed, ARM-compiled and exercised on the headset, not host-unit-tested.
- The player increment loads actual JC lower-body and original weapon-viewmodel
  hands, with corrected grip-local orientation and native-size original textures.
  Quest captures from the preceding grip-only build show the textured hands.
  User-confirmed Meta-button recenter works. Captured submitted HUD center is
  exactly head-local `(0,0,-1.05)` with matching head-forward eye projection.
  Latest seated-height acceptance is recorded in [PLAYER-VR.md](PLAYER-VR.md).
- The corrective body/floor APK disables back-face culling only for the wearer's
  lower body, preserves map feet through vertical standing recenter changes,
  and blocks collision probing during partial map upload. Original-texture CPU
  comparisons reproduce/fix the self-view culling defect; 15 transform groups
  and 103 ground-probe controls pass. The user confirmed height/legs correct
  after explicit seated calibration. The final build additionally delays
  automatic calibration until focused tracking/collision/reference state settles
  (1,856 posture controls). Final physical cold start calibrated at 1.650 m
  without a manual command; a downward capture shows both hands/lower body,
  two submitted body surfaces and feet on the actual Training floor. Subsequent
  real head lowering produced 1.501 m eye height, as intended.

## Test controls

| Input | Action |
| --- | --- |
| Left stick | Move |
| Left stick click | Switch seated/standing height; sit upright when calibrating |
| Left grip | Show/hide developer HUD outside Persona |
| Right stick left/right | 30-degree snap turn |
| A | Use pointed actor; equip/use selected item when menu is open |
| Right trigger | Fire/use damage action when selected weapon and ammo permit |
| Right grip | Cycle selected inventory item |
| Left Menu | Open/close Persona inventory |
| Right stick in Persona | Up/down selects item; left/right changes enabled page |
| B in Persona | Close Persona |
| Y / X | Quick-save / quick-load |
| B outside Persona | Developer next-catalog-map request, not authored progression |

Wear the Quest in a lit room. A cold start currently prepares data for roughly
40 seconds before the world appears; a startup message is shown while tracking
is valid. Check movement/turn direction, nearby materials and lighting, menu
pages, pickups and an intentional save/load. Existing saves were preserved;
pressing Y deliberately replaces the quick-save, so avoid overwriting a save
you want to keep. Report the map/actor and whether a bug follows launch, menu
use, pickup, save/load or travel.

Seated mode keeps a 1.65 m calibrated virtual eye height while retaining real
head/controller motion, ground-aligned avatar feet and the same floor in collision
and saved-position mathematics. The mode is remembered separately from saves
and recalibrates after each session's Training load and a short stable-tracking
window. Sit upright while loading; later leaning/crouching remains real motion.
Standing mode keeps physical floor height.
Long-press right Meta for system recenter; the app no longer cancels its forward
reset. The Meta button is not intercepted by game input.

## Known boundaries

Inventory starts empty until pickups or a quick-load. Goals/Notes and Logs
start empty until the corresponding supported runtime actions occur. Health
shows aggregate health; original per-body-part damage is not implemented.
Augs, Skills, Conversations and Images tabs are intentionally disabled, not
working pages. UI interaction, item population and the new v4 script-state
save format were not newly exercised through physical buttons in this handoff.

Full authored script startup, AI/state/latent execution, timers/spawns,
campaign archiving/progression, live GPU animation and remaining dynamic
lighting types/effects are unfinished. Worker teardown joins before global
runtime destruction and can wait if preparation is still running. See
[STATUS.md](STATUS.md) and [SCRIPT_STATE_SAVE.md](SCRIPT_STATE_SAVE.md).

Player visuals are static legs/lower coat and rigid controller-attached hands,
not full-body IK, arm/finger animation, walking/crouching animation or held
weapons. Black/missing-looking Training column sides remain visible in captures;
this player-controls batch does not fix or certify all map rendering.

The local `artifacts/release-player-controls-20261009/` folder holds the frozen APK, checksum,
device captures and handoff notes. These generated files are intentionally
ignored by Git. Source and build instructions are committed; no new feature
batch should replace this build before the user's headset test.
