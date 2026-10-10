# Original actor visuals without a headset

The desktop capture can now include original mesh actors and movers with
`--actors`, or frame one original object without the BSP with
`--actor-isolate <full-map-object-path>`. This is a static asset inspection
tool, not a playable desktop game or a substitute for Quest testing. Mesh
captures now sample inherited authored animation properties through the same
CPU pose sampler used by Quest's initial actor geometry. Explicitly invoked
animation natives/helpers can also publish their native clock state; the tool
does not automatically tick that clock or implement campaign startup/idle
selection.

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
- Lazy animation queries/commands and map-wide decoding now use one complete
  mesh loader. Previously, calling an animation native before the first decode
  cached vertices without texture paths, causing otherwise valid textured
  characters to disappear. The new cold-original regression checks both query
  and mutating paths, independent mesh references and selected material layers;
  see [BLEND-ANIMATION.md](BLEND-ANIMATION.md).

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
An isolated-capture failure now names the original actor and reports its
omission reason (including unavailable materials), instead of only a generic
"must name a rendered object" error.
Original files are read-only. Commercial derived textures, meshes, captures
and reports stay under ignored `artifacts/`, never in Git.

For an explicitly selected original sequence, add
`--actor-animation-sequence <name> --actor-animation-frame <fraction>` to an
isolated capture. The fraction must be in `[0,1)`. Use the isolated JSON
record's `availableSequences` list; do not assume every mesh has the same
idle names. These overrides are labelled `poseFixture`, not current campaign
state. `--actor-fatness <0..255>` exercises authored expansion (neutral 128).
The PowerShell wrapper exposes `-ActorAnimationSequence`,
`-ActorAnimationFrame` and `-ActorFatness` with `-IsolatedActor`.

For a native/helper that returns a newly born Actor, the direct executable
accepts typed `--actor-script-object PATH` arguments and
`--actor-script-use-result`. For example:

```powershell
.\artifacts\prerequisites-20261009\build\deusex_desktop_visual.exe --game-root $ownedGame `
    --map 00_Training --actor-isolate 00_Training.Doctor1 `
    --actor-script-function Engine.Actor.Spawn --actor-script-object DeusEx.WeaponPistol `
    --actor-script-object 00_Training.Doctor1 --actor-script-use-result `
    --yaw 45 --pitch -60 --width 960 --height 720 --min-coverage 0.01 `
    --output artifacts\actors\born-pistol.bmp --report artifacts\actors\born-pistol.json
```

The selected class is an actual loaded UClass, not a string placeholder. The
report retains the receiver, typed arguments and returned object separately.
This dormant-map fixture does not initialize the campaign. On 2026-10-09,
initial yaw0/yaw90 captures decoded the real GlockPickup's 83 triangles and
resolved all selected materials, but failed the chosen coverage gate and
showed an incomplete-looking pistol silhouette. Investigation found the flat
pickup was viewed edge-on and isolation silently discarded requested pitch.
The corrected camera orbits with yaw and pitch; 41 synthetic orbit controls
pass. The inspected yaw45/pitch-60 frame shows barrel, trigger and grip and
passes the unchanged 1% gate at 2.0434% coverage, hash `8f382586b76073a`.
Failing edge-on artifacts remain retained. This one CPU fixture is not
original-renderer pixel equivalence, live gameplay or headset GPU evidence.

For a cold animation-to-render material check, invoke the native before the
first decode in a fresh process:

```powershell
.\artifacts\prerequisites-20261009\build\deusex_desktop_visual.exe --game-root $ownedGame `
    --map 00_Training --actor-isolate 00_Training.Doctor1 `
    --actor-script-function Engine.Actor.TweenBlendAnim `
    --actor-script-name Still --actor-script-float 0.3 `
    --yaw 70 --pitch -10 --width 720 --height 720 --min-coverage 0.01 `
    --output artifacts\actors\doctor-tween.bmp --report artifacts\actors\doctor-tween.json
```

On 2026-10-10 the pre-fix capture failed with "No available materials for this
mesh". After complete lazy-cache hydration, inspected Doctor1 and JaimeReyes0
frames each show full textured bodies, 603 triangles, seven overrides and zero
missing material selections. Coverage is 7.8808% / 8.3084% at the unchanged 1%
gate. No elapsed tick occurs; this is cache/render availability evidence, not
an animated original-renderer comparison. Local failure and fixed artifacts
remain under `artifacts/blend-animation-20261010/`.

## Evidence and remaining work

On 2026-10-07 the owned-package transform audit decoded 437 mesh exports,
including 228 nonzero RotOrigin records, with 18,084 differential checks and
17 malformed controls. Material inheritance and inactive-actor retention passed
on Training, Liberty Island, Hong Kong MJ12 lab and Area51: 10,354 actors,
1,229 non-null indexed skin slots and 1,186 selected actor overrides. The
campaign-wide option subsequently passed all 80 numbered maps: 124,199 actors,
10,482 non-null indexed skin slots and 10,292 selected actor overrides. The
78 maps with eligible pickup inventory also passed inactive-asset retention.
Every numbered map's selected material references resolved; procedural
fallbacks and an invisible-only mover brush remain explicitly reported.

Training and Island software scenes loaded 64/280 mesh instances and 25/19
movers respectively with no missing mesh material selections. Jaime and plant
close-ups were visually inspected; the latter revealed and verified the
masked-filtering fix. These counts describe decoded scene submissions, not
visual inspection of every individual actor. Procedural FireTexture layers
still have explicit fallback images. Sprites and cube placeholders are omitted
from desktop previews; Quest retains a physical-asset failure placeholder and
its existing sprite approximation. Editor cameras and zero-scale mesh actors
do not become visible Quest cubes.

The host suite has 23 entries: 22 ordinary tests pass; the original-runtime
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

Shared authored pose sampling, raw-space Fatness and additive channels now pass
the independent 437-mesh original-data audit; see [ANIMATION-POSES.md](ANIMATION-POSES.md).
Live animation (including normal scripted campaign poses), attachment rendering, procedural
texture animation, faithful actor/zone lighting and shadows, sprite behavior,
skyboxes, complete campaign scripting, and on-device performance/stereo
verification remain required for the full playable standalone port.
