# Current Quest test-build handoff (2026-10-08)

The asynchronous-startup APK replaces commit `9e4779f`'s launch-blocked build.
It is installed on the user's Quest 3 and launch/render verified, not a finished
full-campaign release. Original commercial game data remains in the existing
private app directory; updating did not uninstall or clear game data or saves.

Package: `dev.deusex.questvr.smoketest` (Android ARM64 debug build).
APK size: 18,384,249 bytes.
SHA-256: `FD7B2FB9CB06E4FF6BA12A0A62E8D6336AB6FE4AD364E12AE5C0F28DB93A237B`.
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
- 26 ordinary host tests pass. Two optional original-data integrations skip
  unless supplied the game root; their separate full original-data checks passed
  before this startup-only change. The new startup ownership path was source
  reviewed, ARM-compiled and exercised on the headset, not host-unit-tested.

## Test controls

| Input | Action |
| --- | --- |
| Left stick | Move |
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

The local `artifacts/release-<commit>/` folder holds the frozen APK, checksum,
device captures and handoff notes. These generated files are intentionally
ignored by Git. Source and build instructions are committed; no new feature
batch should replace this build before the user's headset test.
