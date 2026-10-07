# Original static map lightmaps

World BSP no longer receives the global unoccluded vertex-light approximation.
Quest and desktop `--baked-lighting` consume the original map's static light
lists, per-light shadow bit planes, surface normal/basis, zone ambient and
`PF_Unlit`. The actor renderer retains its interim direct-light approximation.
This is static-lighting progress, not completion of the VR campaign port.

## Source and fidelity

The reference is pinned SurrealEngine revision
`677ee14c5b83486e6634687953779aafb7973ad6`, specifically UModel::Load,
LightmapBuilder, Shadowmap, LightEffect, Math/hsb and GLRenderDevice's complex
surface UV equation. `portable_model_geometry` preserves the entire v68 model
tail: lightmap indexes, ordered actor lists including duplicates/null terminators,
shadow bits, zones, bounds/leaves and flags. Only node-addressed vertex-pool
slots must reference valid points: Training and Intro contain unused free slots
whose serialized point index equals the point count. Live ranges remain strict.

Mask rows are LSB-first and padded to `(width+7)/8`. The light's list ordinal
selects its plane; disabled/unsupported sources never compress this ordering.
The active pinned optimized blur is reproduced, including its edge behavior
and weights summing to two. It is not replaced with a normalized generic blur.
Steady/backdrop lights and pinned static effects use the original HSB table,
radius `(byte+1)*25`, effect attenuation and incidence. Contributions clamp
individually; their sum and inherited zone ambient do not receive an ambient
floor or artificial brightness adjustment.

An analytic basis solve replaces fragile scanline divisions for ordinary
orthogonal texture axes. It retains the pinned builder's asymmetric U sample
half-texel, while rendering UVs independently follow the pinned renderer's
half-texel equation in both axes. These two conventions are tested separately.
Coincident incidence samples are explicitly dark/countable instead of NaN.

Time-dependent types/effects are diagnosed and omitted, not silently frozen
as steady lights. Their per-surface reference counts are not unique actor
counts. Dynamic lights, light switches/invalidation, fog, animated textures,
macro/detail textures, skyboxes and original display/gamma equivalence are
unfinished. There is no original-executable image-equivalence claim.

## Cache, origin and uploads

DXQM v2 is unchanged. A derived DXQS v1 `.mesh.surfaces` stream carries exact
surface/zone records parallel to its vertices. The bounded reader checks chunk
counts, materials, triangle-uniform records and exact EOF. The baker additionally
matches every triangle against the selected original root model's exact BSP
fan, winding, zone and local-position bit patterns, including Unlit/no-lightmap
triangles. A wrong coplanar surface is not accepted merely for lying on a plane.

The origin is the cache decoder's first serialized Level-ordered PlayerStart
with a valid Location, verified against its runtime snapshot. Actors, direct
actor lights and spatial audio now share that origin. The Area51 final map
exposed that export/snapshot order can select a different PlayerStart.

Each surface/ambient tile is packed into 1024x1024 RGBA8 array pages with
duplicated one-pixel gutters. Raw UVs interpolate perspective-correctly; tile
clamping happens per fragment, not at vertices. Power-of-two gain scale keeps
HDR sums through quantization, whose maximum error is reported. Limits are
16 pages/64 MiB, eight million baked pixels, bounded light lists and sample-light
work. Oversized/invalid data fails explicitly rather than allocating unchecked.
Missing lightmaps multiply material by white, and Unlit bypasses world lighting.

Map replacement bakes on its existing worker before saved actor inactivity is
applied. Quest uses a separate world shader/program, leaving actor shading
unchanged. Initial world loading and subsequent transitions use staged GPU
uploads: immutable storage checked against driver limits, at most 128 KiB of
atlas rows per frame, and 4800-vertex geometry batches. Session/transition errors
clean up pending and committed resources. Individual GL allocations/deletions
are still nonpreemptible; timing and shader execution require the headset.

## Validation

Run `tools/Test-DesktopVisuals.ps1 -GameRoot <owned-installation> -MapName
00_Training -BakedLighting`. Original files remain read-only, and all commercial
derived caches/images stay under ignored `artifacts/`, outside Git.

The 2026-10-07 decoder audit passed all 88 installed models, including long
ordered lists beyond 64 lights. Host CTest has 17 tests: 16 pass and the
original-runtime save test explicitly skips without its opt-in game root.
New tests cover malformed/truncated Model sections and DXQS streams, actual
pinned HSB and rotation comparisons, padded-mask blur differentials, effect and
UV math, sampling/clipping/alpha/depth, per-fragment clamp, atlas gutters,
origin ordering, and 26 malformed surface/runtime associations.

Training, Liberty Island, Hong Kong lab and Area51 static captures are local
evidence, not a campaign playthrough. The separate four-view Training albedo
regression remains pixel-exact. The APK compiles; its new shader, stereo output,
memory and frame timing have not been verified on Quest while the headset is
unavailable. Broad `ue1LightmapsVerified`/`bspShadowOcclusionVerified` remain
false; `ue1StaticLightmapsDecoded`/`authoredShadowMasksApplied` distinguish the
specific CPU evidence from unproven complete fidelity.
