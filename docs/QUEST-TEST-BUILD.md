# Current Quest test-build handoff (2026-10-09)

A newer **backend-only, not installed** class-default build is available at
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
