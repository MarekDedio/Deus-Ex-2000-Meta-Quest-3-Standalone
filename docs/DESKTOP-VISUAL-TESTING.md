# Visual testing without a Quest

The desktop executable uses the project's actual portable UE1 package decoder
to produce the same `DXQM` v2 world mesh and `DXQA` v1 material array consumed
by the Quest app. A software rasterizer renders that cache into deterministic
BMP images. Original commercial packages stay in the user's game installation;
generated caches, screenshots, and reports stay under ignored `artifacts/`.

This is a world geometry/material inspection tool, not a playable desktop port
or an OpenXR emulator. It does not render actor meshes, authored lighting,
skyboxes, HUD/Persona UI, or controller models. It does not execute campaign
progression or validate audio, saves, level transitions, stereo comfort, native
Quest GPU shaders, or device frame times. Reports explicitly set
`campaignPlayabilityVerified` to `false`.

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

## Hardware validation still required

The latest shared movement fixes cover nonzero tracking origins, off-origin
snap turns, simultaneous movement/turning, collision rollback, path sampling,
tracking loss/resume, and reference-space rebasing. Their arithmetic is tested
using the Meta SDK's vector/quaternion math on desktop. The APK integration still
needs a physical Quest session for tracking transitions, comfort, controller
actions, Persona blending, stereo visuals, and performance. Use the existing
`Capture-QuestScreenshot.ps1` after installing the APK; those captures read the
actual resolved left-eye framebuffer rather than this software approximation.
