# Visual testing without a Quest

The desktop executable uses the project's actual portable UE1 package decoder
to produce the same `DXQM` v2 world mesh and `DXQA` v1 material array consumed
by the Quest app. A software rasterizer renders that cache into deterministic
BMP images. Original commercial packages stay in the user's game installation;
generated caches, screenshots, and reports stay under ignored `artifacts/`.

The world-capture mode is a geometry/material inspection tool, not a playable desktop port
or an OpenXR emulator. Default albedo captures do not render actor meshes, authored lighting,
skyboxes, HUD/Persona UI, or controller models. It does not execute campaign
progression or validate audio, saves, level transitions, stereo comfort, native
Quest GPU shaders, or device frame times. Reports explicitly set
`campaignPlayabilityVerified` to `false`.

`-BakedLighting` (CLI `--baked-lighting`) additionally evaluates original static
light lists, authored per-light shadow bit planes, ZoneInfo/LevelInfo ambient
and `PF_Unlit`, using the same bounded CPU baker as Quest. It requires original
GameRoot/map data and verifies exact world/sidecar triangle and origin matches.
Animated light components remain explicitly omitted; skyboxes, actors, native
GPU precision, display gamma, stereo and gameplay are still unverified. See
[STATIC-LIGHTMAPS.md](STATIC-LIGHTMAPS.md).

`-Actors` / `--actors` additionally loads original mesh actors and textured
movers, with shared Quest CPU placement/material selection. `-IsolatedActor`
/ `--actor-isolate` gives an automatically framed original-object close-up.
These sample inherited authored mesh poses, not a runtime animation clock or
campaign play. Explicit isolated sequence/frame overrides are diagnostic
fixtures; JSON records separate them from authored state.
See [ACTOR-VISUAL-TESTING.md](ACTOR-VISUAL-TESTING.md) for commands and omissions.

A separate `--persona-preview` mode runs the same CPU artwork compositor used
by the APK. It decodes the original page backgrounds, borders, icons, bitmap
fonts, and navigation/action-button artwork. Its sample text and inventory
are fixtures; it does not simulate GL rendering, live state, or input.

On 2026-10-07 the full installed catalog passed decoding and capture: 88 maps,
352 images, and zero map failures. This includes 80 numbered maps plus
multiplayer/utility entries. `DX`, `DXOnly`, and `Entry` produced intentionally
uninformative uniform black frames and are flagged in the report. The first run
failed 15 maps; the audit caught shared texture/palette name collisions and a
case-sensitive export lookup error. Regression tests now cover both bugs.
This evidence is a decoder/capture milestone, not campaign playability.

Recognized procedural textures with no usable stored surface are initialized
from their validated original clamp dimensions and palette. Fire, water, and
other procedural animation is not simulated yet; initialization is logged as
`animation pending`. A static procedural layer is not visual parity with UE1.

## Build and capture

Restore the pinned dependencies first:

```powershell
.\tools\Initialize-ThirdParty.ps1
```

The Windows helper uses MinGW `g++.exe` and `mingw32-make.exe`, plus CMake 3.22.1
or newer. It accepts `-CompilerPath` and `-CMakePath` when they are not on PATH
or in the workstation's known installation locations. It builds with two jobs
and runs CTest before capturing. CTest covers software depth, near clipping,
perspective-correct UVs, alpha cutoff, deterministic output, BMP round trips,
the shared portable GC probe, and VR-world transform regressions.

```powershell
.\tools\Test-DesktopVisuals.ps1 `
    -GameRoot 'D:\Steam\steamapps\common\Deus Ex' `
    -MapName '00_Training'
```

Default outputs are `artifacts/desktop-visuals/view-00.bmp` through
`view-03.bmp`, corresponding to yaw 0, 90, 180, and 270 degrees. Each image has a
JSON report; `summary.json` contains the capture set. Each real map is decoded
once, then the subsequent views consume that exact generated cache. Use a
different `-OutputDirectory` to preserve a capture set for later comparison.
The all-map runner always creates a unique run directory.

With no `-GameRoot` or cache paths, the helper renders a synthetic fixture and
reports **zero real maps tested**. Synthetic success is never substituted for a
missing or unreadable real map.

## Choose a viewpoint

Camera coordinates are meters in the Quest map-cache coordinate system,
relative to the decoder's selected map spawn. The default is `(0, 1.65, 0)`,
looking along negative Z, with 90-degree vertical field of view. Positive yaw
turns the view right. Arbitrary fixed views can be reproduced:

```powershell
.\tools\Test-DesktopVisuals.ps1 `
    -GameRoot 'D:\Steam\steamapps\common\Deus Ex' `
    -MapName '01_NYC_UNATCOIsland' `
    -CameraPosition 0,1.65,0 -YawDegrees 0,90,180,270 `
    -Width 640 -Height 360 -SkipBuild `
    -OutputDirectory '.\artifacts\liberty-inspection'
```

`-PitchDegrees` and `-VerticalFovDegrees` are also available. Existing cache
files can be rendered with `-MeshPath` and `-MaterialArrayPath` instead of
`-GameRoot`. Keep camera, image size, and FOV identical for baseline comparisons.

## Image regression gates

```powershell
.\tools\Test-DesktopVisuals.ps1 `
    -GameRoot 'D:\Steam\steamapps\common\Deus Ex' `
    -MapName '00_Training' -SkipBuild `
    -BaselineDirectory '.\artifacts\training-baseline' `
    -MaxMeanError 0 -OutputDirectory '.\artifacts\training-comparison'
```

Mean absolute image error is normalized to 0..1; zero requires exact pixels.
A failed baseline gate returns a failure rather than approving the changed
image. Baselines must be visually reviewed before adoption: identical images
can preserve an existing bug. Reports also include image hashes, material-layer
and triangle counts, world bounds, coverage, and luminance variance.

`-MinCoverage` can enforce a known viewpoint's minimum visible geometry fraction.
An entire empty capture set fails. Uniform or black frames are warnings in the
campaign report: some utility maps intentionally contain black geometry, and a
camera outside the useful world can produce empty views. Coverage alone cannot
establish correct rendering or playability.

## All-map decoder/capture audit

```powershell
.\tools\Test-DesktopCampaign.ps1 `
    -GameRoot 'D:\Steam\steamapps\common\Deus Ex' -SkipBuild
```

The runner discovers every original `Maps/*.dx` file, captures each at the
configured views, continues after individual failures, and saves a consolidated
`campaign-report.json`. It exits with an error if any selected map fails.
`-MapNames` limits a targeted rerun. Decoded-map counts are distinct from image
counts and from campaign progression; multiplayer and utility maps may also be
present in the installation. A pass means only the stated decoder/capture gates
passed, not that the campaign is fully playable.

## Original Persona artwork previews

```powershell
.\tools\Test-DesktopPersona.ps1 -GameRoot 'D:\Steam\steamapps\common\Deus Ex' -Page Inventory -SkipBuild
.\tools\Test-DesktopPersona.ps1 -GameRoot 'D:\Steam\steamapps\common\Deus Ex' -Page Health -SkipBuild
.\tools\Test-DesktopPersona.ps1 -GameRoot 'D:\Steam\steamapps\common\Deus Ex' -Page GoalsNotes -SkipBuild
.\tools\Test-DesktopPersona.ps1 -GameRoot 'D:\Steam\steamapps\common\Deus Ex' -Page Logs -SkipBuild
```

Omit `-SkipBuild` to build and run the host CTests first. Each preview is
640x480 and places the original masked artwork over a checkerboard to expose
transparent margins. JSON records exact texture provenance, page rectangles,
alpha counts, font atlas/glyph provenance, sample text, and a composed RGBA hash.
Original `FontMenuHeaders` and `FontMenuSmall` use their exact single-byte UE1
glyph rectangles and advances, with masked atlas pixels and window-clipped
wrapping. Inventory contains a clearly identified
icon asset fixture, not a saved player's items; the other pages have no grid.
Health uses its original neutral body and overlay assets, not simulated limb
damage. Logs uses four background tiles and six Conversations border tiles.

`-BaselinePath` and `-MaxMeanError` provide the same image gate as world
captures. Original package, cache, and baseline aliases are rejected before
output writes. On 2026-10-07 all four original page previews were inspected;
Inventory remained pixel-identical after extracting the shared compositor.
CTest covers each page's origin/clipping, two-column Logs tiles, palette-index
zero masks, opaque black pixels, tints, transparent padding, grid selection,
icon aspect ratio, and the Health body's 219x357 crop. Additional tests cover
font masks/tints/advances, word and hard wrapping, malformed glyphs and windows,
original button caps/repeated strips, and current/available/disabled tab tints.
The four unimplemented original tabs are dimmed; drawn buttons are not
pointer-clickable controls. The VR action captions intentionally differ from
the complete original desktop menu.

New reports set `fontsAndTextVerified` to `true` with an explicit
`fontsAndTextVerificationScope` of original atlas decoding and shared CPU
fixture composition only. `glRenderingVerified`, `openXrVerified`,
`liveRuntimeStateVerified`, and `controllerInteractionVerified` remain false.
Only actual Quest eye-buffer captures can validate the new font texture's
stereo readability, blending, and occlusion against world geometry.
Unicode/localized text rendering is not verified by these single-byte fonts.

## Frame budgets and map-state recovery

Host tests cover the cooperative actor-work budget and the map-replacement
transaction's checkpoint/rollback/fail-closed decisions. A cooperative budget
cannot preempt a slow mesh copy, allocation, or driver call. Desktop timings
are not Quest performance evidence.

The real-data state test is opt-in; its default CTest entry reports skipped,
not passed, without a game installation. After building, run:

```powershell
.\desktop\build\portable_runtime_state_test.exe --game-root 'D:\Steam\steamapps\common\Deus Ex'
```

This loads real Training and TrainingCombat map packages and tests isolated
temporary save/load checkpoints. Seeded progress effects are explicit test
inputs, not a campaign/script playthrough. Original packages and user saves
are not modified. This verifies portable runtime recovery primitives, not
the complete asynchronous Quest/GL transition or on-device failure recovery.

## Shared dialogue audio tests

`portable_mp3_audio` tests the same byte-only MP3 decoding and stereo resampling
helper used by Quest. Generated source fixtures exercise malformed/truncated
data, size/rate limits, forged Xing sample counts, and deterministic mono/stereo
conversion. Allocation budgets are 64 MiB compressed input, 32 MiB decoded PCM,
and 32 MiB output PCM. The helper does not access UE1 global name storage or
the live game runtime from an audio worker.

To test an original owned voice export, opt in explicitly:

```powershell
.\desktop\build\portable_mp3_audio_test.exe --package 'D:\Steam\steamapps\common\Deus Ex\System\DeusExConAudioMission01.u'
```

The test selects the first original MP3 Sound export and compares shared
resampling sample-for-sample with the previous algorithm at five output rates.
Optional `--mp3 <generated-fixture.mp3>` can test nonzero channel fixtures.
Neither mode validates AAudio output, spatial mixing, audible playback, or Quest
device behavior. No original audio is embedded in the repository.

## Optional original-map lighting previews

The default albedo path is unchanged. To inspect actual authored light actors
with the same CPU evaluator used for Quest vertex colors:

```powershell
.\tools\Test-DesktopVisuals.ps1 -GameRoot 'D:\Steam\steamapps\common\Deus Ex' -MapName 00_Training -AuthoredLighting -YawDegrees @(0) -Width 960 -Height 540 -SkipBuild -OutputDirectory artifacts\training-lighting
```

The opt-in mode reads the same 38 production System packages and validates that
the map-cache and actor light positions use the same authored PlayerStart.
Unsupported bounds-fallback origins fail closed. JSON records package paths,
light counts, source types/effects, and vertex-light ranges. Without GameRoot,
the switch uses explicitly labeled synthetic lights, not campaign evidence.
External caches alone cannot prove original light positions and are rejected.
Each real-map view currently reinitializes the runtime; use one yaw for a quick
check. This is slower than the albedo-only cached multi-view workflow.

The software renderer interpolates RGB light gains perspective-correctly, as
the native shader does semantically, while retaining masks, clipping and depth.
This is not a GPU numerical-equivalence check. UE1 baked lightmaps, per-surface
shadow bits, zones, dynamic light effects and actor rendering remain excluded.
See [authored-lighting limits](AUTHORED-LIGHTING.md).

The save metadata, paired-bundle and asynchronous epoch CTests exercise shared
production helpers using isolated files/fixtures. The opt-in real-data runtime
test also verifies paired Training/Combat generation identity and fallback from
corrupt or semantically invalid runtime bytes. They do not exercise the whole
controller-to-GL asynchronous restore; see [save recovery](SAVE-RECOVERY.md).

## Hardware validation still required

The latest shared movement fixes cover nonzero tracking origins, off-origin
snap turns, simultaneous movement/turning, collision rollback, path sampling,
tracking loss/resume, and reference-space rebasing. Their arithmetic is tested
using the Meta SDK's vector/quaternion math on desktop. The APK integration still
needs a physical Quest session for tracking transitions, comfort, controller
actions, Persona blending, stereo visuals, and performance. Use the existing
`Capture-QuestScreenshot.ps1` after installing the APK; those captures read the
actual resolved left-eye framebuffer rather than this software approximation.
