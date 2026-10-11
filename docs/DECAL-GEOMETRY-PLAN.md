# Original decal geometry and ordered state prerequisites

Added 2026-10-12. The user's original GOTY DLLs/packages were read, never
executed or redistributed. The authority, addresses, defaults and gaps are in
[DECAL-NATIVE-CONTRACT.md](DECAL-NATIVE-CONTRACT.md).

## Implemented scope

These portable, bounded headers work in authored Unreal coordinates:

| Header | Contract |
| --- | --- |
| `quest_model_point_trace.h` | Original null-owner, flags6, zero-extent BSP point trace; Item is a node, with native wrapper backoff. |
| `quest_decal_surface_select.h` | Actual polygon edge signs, immediate-previous-distance comparison, iPlane-only fallback, primary/secondary center distinctions, fresh surface-node cache and whole-quad node membership clipping. |
| `quest_decal_primary_quad.h` | Original random/supplied/base-fallback orientation, USize-only radius, ordered world/relative corners and pretrace random consumption. |
| `quest_decal_secondary_quad.h` | Upper-only multilevel clamp, original ordered grid, signed-angle rejection and two-line corner correction. |
| `quest_decal_projection.h` | Composes primary trace/selection/gates, quad, secondary grid/reprojection and membership into an ordered geometry-only plan. |
| `quest_decal_state.h` | Surface-owned records and owner SurfList mirrors: first live-Texture insertion, primary duplicates, reverse detach/first owner match, copy/swap rollback. |

The original Shadow inherits MultiDecalLevel4 and DrawScale1. Its actual
`DeusExItems.Skins.FlatFXTex40` texture is 32x32. The plan therefore executes
sixteen secondary samples, not a primary-only substitute. Primary ray
normalization retains its Z/Y/X squared-sum order; common quad/grid
normalizations retain X/Y/Z order. The primary residual compares against
FLOAT0.001, whereas BSP recursion uses DOUBLE0.001.

Surface selection follows the original adjacent sign comparisons, including
zero-sign bridging, not a fixed winding or a nearest triangle. Fresh node
cache order is ascending authored node-table index, including empty nodes;
membership skips empty polygons and uses fixed original clip winding. It
retains boundaries and inserts strict crossings before pruning negatives,
with the original intermediate polygon <24 bound. The entire quad remains
stored; clipped polygons are temporary. Empty membership means unrestricted
rendering, not an invisible attachment.

The primary path permits repeated surface records. Secondary samples skip
every surface already present in the actor's SurfList, including previous
calls and the newly added primary/secondary surfaces. Secondary selection
intersects only the initial hit surface before polygon selection; it does not
perform the primary path's second selected-plane intersection. Opposite
normals can pass the absolute cosine gate, and the signed FLOAT cosine drives
the original correction. Replacing that correction with simple projection is
not equivalent.

Random zero direction is prepared before tracing. Ordinary misses/flag
rejections retain successful draws; malformed input or budget exceptions do
not publish a partial candidate seed. Geometry-query work is limited before
entering helpers, with the remaining aggregate allowance partitioned between
their validation/query counters. Tight custom limits may refuse conservatively
when one partition exhausts. Nested limit schemas are validated even on misses.
Exact x87 extended-precision boundary parity remains uncertified.

The state container has deterministic structural/payload byte accounting and
conservative candidate/temporary charges. This is not a process-RSS or allocator
capacity measurement. Resolver lookup work, runtime object roots, journal
snapshots and GPU staging must also be bounded by the integrating host.

## Tests

Generated host tests cover actual small BSPs and hand-computed geometry,
orientation branches, RNG consumption, nonfinite/index/cycle/byte/work refusals,
live Texture changes, duplicate records, mirrored missing/foreign records,
detach order and rollback. The composed split-floor fixture exercises two
surface records, sixteen samples and secondary deduplication, then publishes
and detaches repeated records using the standalone state container.

The owned-data actor suite additionally reads real Shadow class defaults and
the texture's authored USize/VSize, queries JaimeReyes0's actual Training floor,
and checks trace/selected identities, exact floor center, full multilevel
geometry, membership, repeatability, ordered standalone state and detach.
It verifies byte-identical checkpoint, actor/revision and GC observations.
This read-only test does not spawn a live Shadow or run its native attachment.

Jaime's authored feet are at `(361.278564453,1206.00805664,-95.90018463)`.
The BSP trace reports node1848/surface1837, but that polygon does **not** contain
the exact floor center. The coplanar chain proceeds through1849/1850/1851 to
node1852/surface1861, whose four independently evaluated planar signed edge
areas are all positive (minimum7042.5584), well away from a rounding boundary.
Selected center Z is exactly-96. With original32x32/scale1/multi4 settings and
seed1, the plan stores one primary surface record with one membership node,
draws six random values, and visits sixteen duplicate secondary samples.
The query accounts2,385,413 work steps. Equating hit-node surface with final
selected surface would attach to the wrong polygon; the owned-data regression
pins both identities separately. Ignored evidence is
`artifacts/decal-geometry-20261012/inspect-real-plan.log`.

```powershell
& 'D:\Android\Sdk\cmake\3.22.1\bin\cmake.exe' --build artifacts/prerequisites-20261009/build -j4
& 'D:\Android\Sdk\cmake\3.22.1\bin\ctest.exe' --test-dir artifacts/prerequisites-20261009/build --output-on-failure
& artifacts/prerequisites-20261009/build/portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex' --world-startup-only
```

## Required native integration, not completed here

1. Bind the real native client viewport and live Texture, preserving guard order.
   Geometry-only success is not native Attach success. Do not turn unsupported
   native modes into false/success to bypass startup.
2. Put one map-native decal GC object under RuntimePackage; retain actual native
   Level/model and record-owner roots. Per-call Host.rootModel_ is not persistent
   attachment ownership. Strings alone do not retain actors.
3. Add journalled native dynamic INT SurfList storage/bridging. Current VM
   ArrayProperty decoding is unsupported; fixed arrayDimension is not a dynamic
   array length. Preserve legitimate mirror/native mismatches and original Const
   metadata instead of inventing a fixed array.
4. Add TouchDecals snapshots/provisional roots, aggregate retained/staging
   accounting and native dirty-world revision publication. Rollback must restore
   the graph/roots before provisional birth identities disappear, without scripts
   or allocations.
5. Bind exact original native-zero Attach/Detach via NativeWithExecution.
   Primary mover attachments need stored mover rotation and the original
   `SetBase(self.Owner,true)` callback path. Secondary records have no matching
   original mover/SetBase block. Do not bypass overridden Destroyed events.
6. Add a versioned explicit port-native geometry save section, exact symbolic
   preflight and rooted staged restore, with ordered corners/membership/records,
   owner lists and RNG. Do not replay Attach/random draws on load. Legacy saves
   need an explicit empty/reset policy. Original disk archives do not serialize
   the native geometry, so this is a deliberate port checkpoint policy.
7. Integrate map replacement/unload/shutdown and unsaved-state detection; retire
   attachments only with their map and refuse accidental unarchived state loss.
8. Publish live Texture/material revisions and render ordered surface records,
   node filters, visible-polygon clipping, UVs, gray/modulated color, 0.75UE normal
   bias, mover deltas and LastRenderedTime. Validate on Quest before claiming
   rendered original shadows or full Attach fidelity.

The rollback-only Training startup still stops at unsupported
`Engine.Decal.AttachDecal`; no original world/player possession, campaign Tick,
or full standalone campaign acceptance is established by these prerequisites.
No APK is created merely for host-only headers; the last installed Quest renderer
and corrected hand/body geometry are unchanged. Every actual new APK continues
to use the build helper's default data-preserving connected-Quest install.
