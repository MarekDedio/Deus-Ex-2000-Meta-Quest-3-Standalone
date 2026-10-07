# Original actor visuals without a headset

The desktop capture can now include original mesh actors and movers with
`--actors`, or frame one original object without the BSP with
`--actor-isolate <full-map-object-path>`. This is a static asset inspection
tool, not a playable desktop game or a substitute for Quest testing.

## Shared fixes

- Indexed, inherited `MultiSkins`, `Skin`, mesh defaults and `Texture` now
  follow the pinned renderer's actual selection order. Explicit `None` blocks
  inheritance at that array element without suppressing later render fallbacks.
- Vertex meshes preserve serialized `RotOrigin`, material polygon flags, and
  smoothed normals by serialized vertex identity. Actor pitch/roll signs,
  mesh/brush PrePivot differences, MainScale, nonuniform normal transforms and
  reflected triangle winding use shared CPU helpers in Quest and desktop.
- Movers decode bounded authored `UPolys` rather than gray, untextured BSP
  approximations. They retain original polygon texture references, UV bases,
  texture dimensions/DrawScale, pan values and normals.
- Texture `bMasked` is the original native bitfield, not a `PolyFlags` integer
  tag. Opaque P8 layers preserve index-zero RGB; masked variants zero that RGB
  before filtering. The latter fixes the magenta fringes found in actual plant
  close-ups. No border-color chroma-key guess is used for actor textures.
- The actor bank keeps separate layer indices from world materials. Only
  selected original material sources are packed, including inactive actors
  needed by quickload. Limits remain 255 total actor layers and 256 MiB.
- Actor opaque/masked/two-sided/unlit and translucent/modulated modes have
  explicit renderer handling. Desktop sorts blended triangles; Quest sorts
  bounded draw chunks after opaque BSP. Neither is exact visibility sorting
  for intersecting translucent polygons. NoSmooth and environment mapping
  remain incomplete, and actor lighting is still the direct approximation.

## Commands

Build the host tools with `tools/Test-DesktopVisuals.ps1` or CMake, then:

```powershell
$ownedGame = 'D:\Steam\steamapps\common\Deus Ex'
.\tools\Test-DesktopVisuals.ps1 -GameRoot $ownedGame -MapName 00_Training `
    -Actors -BakedLighting -Width 640 -Height 360 -SkipBuild

.\desktop\build\deusex_desktop_visual.exe --game-root $ownedGame `
    --map 00_Training --actor-isolate 00_Training.JaimeReyes0 --yaw 90 `
    --width 640 --height 640 --output artifacts\actors\jaime.bmp

.\desktop\build\deusex_desktop_visual.exe --game-root $ownedGame `
    --map 00_Training --actor-isolate 00_Training.Plant6 `
    --width 640 --height 640 --output artifacts\actors\plant.bmp

.\desktop\build\quest_actor_materials_test.exe --game-root $ownedGame
.\desktop\build\quest_actor_materials_test.exe --game-root $ownedGame --all-campaign-maps
.\desktop\build\quest_actor_transform_test.exe --game-root $ownedGame
```

Isolation automatically frames original actor bounds; yaw chooses the viewing
side. JSON reports retain full original object paths, bounds, source asset,
material overrides, omitted sprites, fallback textures and errors. A successful
capture does not mean every actor is supported: inspect its reported omissions.
Original files are read-only. Commercial derived textures, meshes, captures
and reports stay under ignored `artifacts/`, never in Git.

## Evidence and remaining work

On 2026-10-07 the owned-package transform audit decoded 437 mesh exports,
including 228 nonzero RotOrigin records, with 18,084 differential checks and
17 malformed controls. Material inheritance and inactive-actor retention passed
on Training, Liberty Island, Hong Kong MJ12 lab and Area51: 10,354 actors,
1,229 non-null indexed skin slots and 1,186 selected actor overrides. The
campaign-wide option expands this audit beyond those four maps; its current
result is recorded in STATUS rather than inferred from the four-map check.

Training and Island software scenes loaded 64/280 mesh instances and 25/19
movers respectively with no missing mesh material selections. Jaime and plant
close-ups were visually inspected; the latter revealed and verified the
masked-filtering fix. These counts describe decoded scene submissions, not
visual inspection of every individual actor. Procedural FireTexture layers
still have explicit fallback images. Sprites and cube placeholders are omitted
from desktop previews; Quest retains a physical-asset failure placeholder and
its existing sprite approximation. Editor cameras and zero-scale mesh actors
do not become visible Quest cubes.

The host suite has 21 entries: 20 ordinary tests pass; the original-runtime
state test skips unless explicitly opted in. Synthetic tests cover material
precedence/inheritance, texture flags/palette semantics, separate banks,
near clipping, perspective sampling, alpha/depth/blending, shared geometry,
normals/winding and malformed streams. Existing world-albedo and Persona
fixtures remain pixel-exact. The APK builds, but these new GPU paths have not
been run on Quest while the headset is unavailable.

The separately opted-in original-runtime state test passed with 101,375 runtime
objects and 1,308 Training actors, including Training/Combat rollback, paired
save recovery, and retained HazMatSuit mesh/materials after pickup and restore.
Its conversation/progression inputs are explicit fixtures, not a campaign run.

Animation (including normal campaign poses), Fatness, attachments, procedural
texture animation, faithful actor/zone lighting and shadows, sprite behavior,
skyboxes, complete campaign scripting, and on-device performance/stereo
verification remain required for the full playable standalone port.
