# Authored lighting: current implementation and fidelity work

The Quest renderer and optional desktop `--authored-lighting` preview share
`native/quest_map_lighting.h`. They load actual map actors and inherited class
defaults from the user's installed packages, but currently evaluate an
unoccluded, static, direct-vertex approximation. This is not original UE1
lightmap reproduction, a campaign visual approval, or a Quest performance pass.
The default desktop mode remains world/material albedo only.

## Source-backed corrections

The pinned engine is the `surrealEngineEvaluationFork` revision in
`third-party-lock.json`; references below are relative to its
`SurrealEngine` directory.

- `Packages/Engine/Actors/UActor.h`, `WorldLightRadius`, defines radius as
  `(LightRadius + 1) * 25` Unreal units. The shared port converter now uses that
  expression without an invented minimum radius. Radius byte zero still emits
  within 25 units; `LT_None` and zero brightness disable a light.
- `Light/LightSystem.cpp`, `BeginFrame`, examines every Level actor's light
  properties, not only `Engine.Light` subclasses. Runtime snapshots now retain
  `Engine.Light` classification and also classify any actor with an inherited
  non-`LT_None` type as a potential emitter. This includes authored luminous
  decorations and other actor classes. Location and brightness are still
  validated when constructing the actual light list.
- `Math/hsb.h` uses three linear hue sectors, inverse saturation, and its
  brightness table. The shared conversion follows those semantics, rather than
  conventional six-sector HSV. Tests compare it against the pinned source.
- `Light/LightEffect.cpp`, `SpotlightEffect`, converts the authored cone byte
  to `1 - LightCone/255`, not an angle in degrees. The shared cone boundary
  follows that expression. Its angular fade is still the port approximation.

The host light tests include every radius byte, radius-zero illumination and
boundary rejection, a non-Light-class emitter fixture, inactive/zero-brightness
filtering, emitted type/effect tallies, colored point/spot behavior, and pinned
HSB comparisons. A fixture does not independently prove production actor
classification; real-package captures and runtime checks are needed as well.

## Interpreting desktop reports

The optional preview initializes the same original script-package set as Quest,
loads the selected map through the production portable runtime, and verifies
that its PlayerStart matches the cache decoder's Level-ordered PlayerStart and
serialized location. It rejects a mismatched or unverifiable origin instead of
presenting misplaced lights as valid evidence.

`lighting.lightTypeCounts` and `lighting.lightEffectCounts` tally the original
byte values of emitters accepted by the shared converter. Keys are decimal UE1
enum values. The total for each tally equals `lightCount`. They exclude inactive
types, zero brightness, missing locations and non-finite locations. They do not
claim that pulse, blink, flicker, searchlight, or other effects were simulated.
`emitterTalliesFromAuthoredSnapshots` is false for synthetic fixtures and
albedo-only captures; those modes do not provide authored enum counts.

Vertex-light luminance statistics demonstrate spatial variation in the current
approximation, not original photometric accuracy. Each report explicitly leaves
UE1 lightmaps, BSP shadow occlusion, GPU numerical equivalence, stereo, actors,
live gameplay, and Quest performance unverified.

## Remaining concrete differences

- The global ambient RGB is `0.075`, not the surface's ZoneInfo/LevelInfo ambient.
- Brightness uses the port's `brightness/64` gain; the original HSB brightness
  curve is available for auditing but is not the current renderer's gain model.
- Distance fade is squared linear fade, rather than the pinned effect's
  distance attenuation. Diffuse response retains a small one-sided floor;
  pinned lightmap effects use their own incidence semantics. Spot fade is
  currently linear rather than the pinned squared angular fade.
- Every accepted emitter is currently treated as a static point or spot. Other
  light types/effects, phases, periods, animated palettes, and update cadence
  are not reproduced.
- No per-surface shadow bit planes or authored light lists are evaluated, so
  lights can spill through walls. Large BSP triangles only interpolate their
  corner gains and cannot reproduce localized lightmap patterns.
- Cache v2 does not carry original surface lightmap indices, zones, source
  normals, or `PF_Unlit`. Unlit/emissive surfaces therefore cannot reliably
  bypass this direct-light approximation.
- Native lighting still selects an origin from actor snapshots. Desktop checks
  guard against cache/runtime ordering differences, but the eventual versioned
  cache should carry its exact transform rather than requiring a second guess.

## Next implementation toward original lightmaps

1. Extend the bounded Model decoder in `native/ue1_package_probe.cpp` beyond
   the zones where it currently stops. Preserve each surface's `vNormal`,
   `LightMap`, and relevant flags, plus each BSP node's zone identities.
   `Packages/Engine/Resources/Level/UModel.cpp`, `UModel::Load`, is the pinned
   serialization reference. The remaining tail contains the Polys reference,
   LightMapIndex array, LightBits, bounds/leaves, and actor light references.
2. Decode lightmap entries with strict dimension, offset, allocation, actor
   reference and list-termination bounds. Entries include DataOffset, PanX/Y/Z,
   U/VClamp, U/VScale and LightActors. Resolve actual actors and inherited
   zone/light defaults using the production runtime. Store the exact cache
   origin, axis transform, unit scale, and surface provenance explicitly.
3. Build static lightmap texels off the render thread following
   `Light/LightmapBuilder.cpp` (`Setup`, `CalcWorldLocations`, `AddStaticLights`)
   and `Light/Shadowmap.cpp`. A light's authored bit plane begins at
   `DataOffset + lightIndex * ((width+7)/8) * height`; preserve list order and
   the pinned shadow filtering behavior. Use zone HSB ambient and original
   light colors/effects, not brightness tuning to approximate a reference image.
4. Introduce a versioned mesh/lightmap cache with separate lightmap UVs and
   texture layers. Update the native GLES textured shader and software renderer
   to sample material and lightmap separately. Honor `PF_Unlit`, absent
   lightmaps, side/zone selection, and original addressing/panning semantics.
   Preserve explicit fallback diagnostics when a source cannot be reproduced.
5. Add malformed-payload tests and differential shadow/effect/UV fixtures from
   the pinned source. Inspect original Training, Liberty Island, Hong Kong and
   dark/colored interiors at multiple positions, not just spawn views. Preserve
   albedo-only baselines so a lighting change cannot hide a geometry regression.
6. Implement animated light invalidation and dynamic actor lighting separately,
   then validate original lighting, memory, upload slicing and frame times on
   Quest. Original files and derived commercial lightmap/texture outputs stay
   outside Git; only code, fixtures created by the project, and documentation
   belong in the source repository.

This route addresses localized lighting and authored wall shadows directly.
The current approximation and its tests are useful interim diagnostics, not a
replacement for the requested original-looking, fully playable VR campaign.
