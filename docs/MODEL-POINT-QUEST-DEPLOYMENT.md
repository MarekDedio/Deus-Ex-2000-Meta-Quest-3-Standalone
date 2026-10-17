# Model point bridge Quest deployment (2026-10-17)

## Artifact and installation

- Source commit: `de690dd` (master, pushed to origin).
- Build command: `tools/Build-QuestSmokeTest.ps1 -AndroidSdkPath D:\Android\Sdk -JavaHome "C:\Program Files\Microsoft\jdk-17.0.20.8-hotspot" -DeviceSerial 2G0YC5ZG620985`.
- Gradle `assembleDebug` completed successfully in44s,34 actionable tasks
  (five executed,29 up-to-date). Existing SDK XML/riscv64 metadata warnings
  were nonfatal. Full native module/link and host regression evidence is in
  ORIGINAL-MODEL-POINT-OWNER.md and STATUS.md.
- Archive: `artifacts/model-point-bridge-20261017/DeusExQuestVR-model-point-bridge-20261017.apk`.
- Size:34575870 bytes; SHA256:
  `66A33C43EFF41405EED7F25263C888F106A1F8588FD481D8247B75B6E9910E60`.
- Authorized USB device: Quest3, serial `2G0YC5ZG620985`.
- Package: `dev.deusex.questvr.smoketest`; activity:
  `dev.deusex.questvr.MainActivity`.
- Automatic deployment used `adb install -r`, with no uninstall/data clear.
  Device package `lastUpdateTime=2026-10-17 07:43:05`.
  The installed `base.apk` hash was independently read through ADB and exactly
  matches the archived/local APK above.

## Data preservation and readiness

Before/after installation SHA256 comparisons match exactly for:

- `quest-save-0.meta`, `quest-save-0.runtime`, `quest-save-0.slot0.qsv`;
- `quest-posture.cfg`;
- `System/DeusEx.ini`.

Single launch intent succeeded and confirmed process8948. OpenXR created the
stereo1680x1760 viewport with three images per eye. Logs confirmed persistent
runtime and Training preparation, actual map-replacement collection and
pickup/Pawn-damage validation followed by restoration. These are initialization
diagnostics, not acceptance of live crate gameplay or a bound original player.

Initial OpenXR head pose was unavailable (`xrLocateSpace=0`, flags0). The user
was asked to wear the headset and return to the app. No rendered screenshot or
human visual acceptance of this exact replacement is claimed at this stage.

## Scope

This APK includes the committed fragment render eligibility and selected-Model
collision changes. The accepted existing hands/items/NPC/HUD remain intended
regression coverage. It does not finish live original crowbar crate damage,
contents, Engine/session lifetime, startup/possession, support or falling.
Do not substitute generic health/hide, loot grants, forced startup flags or
visual settling for those missing original systems.

Local evidence logs (ignored build artifacts):

- `artifacts/model-point-bridge-apk-build-install-20261017.log`;
- `artifacts/model-point-bridge-device-data-before-install-20261017.log`;
- `artifacts/model-point-bridge-device-data-after-install-20261017.log`;
- `artifacts/model-point-bridge-device-launch-20261017.log`.
