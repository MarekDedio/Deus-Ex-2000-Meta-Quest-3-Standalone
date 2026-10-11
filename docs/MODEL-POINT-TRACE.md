# Original BSP point-trace prerequisite

`native/quest_model_point_trace.h` implements the recovered GOTY
`UModel::LineCheck` **null-owner, zero-extent, flags6** path used by
`Decal.AttachDecal`. Inputs and outputs remain in authored Unreal units. This
is not Actor.Trace, a mover transform, an extent sweep, physical FindBase, or
native decal attachment. It is not connected to the simplified render collider.

Primary evidence is the read-only Engine.dll audit in
[DECAL-NATIVE-CONTRACT.md](DECAL-NATIVE-CONTRACT.md): UModel wrapper
`0x103F3C20`, point recursion `0x103F3560`, IsCsg `0x103F68B0`. No original
executable/DLL was executed, and no original bytes are redistributed.

## Supported semantics

- Empty Model returns RootOutside without inventing a hit. The value-returning
  portable API supplies fresh default hit fields, not an original mutation of
  caller-provided FCheckResult storage.
- Same-side tests compare float-stored plane distances against double
  `-0.001`/`+0.001`. Front has precedence in the overlapping band. Literal
  zero-vertex nodes and node flag mask `0x27` determine flags6 CSG behavior;
  polygon triangles and surface PolyFlags do not replace that test.
- Crossing visits the near segment before the far segment. Native LastNode is
  unchanged during same-side/near descent and updated only on the far crossing.
  The first solid leaf reports that **node identity**, not its surface index.
- The wrapper projects the raw leaf-entry point onto the full input segment,
  subtracts a half-Unreal-unit backoff, clamps Time, and faces the unnormalized
  authored plane normal against travel. Float stores for distances, split
  directions/offsets, squared length and projected Time are preserved. The
  unrounded clamped Time is retained for offset multiplication as in native
  `fsts`, followed by float-stored offsets/addition.

Exact x87 extended-precision boundary parity is not certified. Native undefined
NaN/overflow/zero-length behavior is not imported: blocked zero-length segments
or nonfinite arithmetic explicitly refuse. Clear zero-length and empty-Model
queries retain their supported behavior.

## Bounds and verification

All front/back components, including unreachable ones, are checked before
querying. Shared children are permitted, cycles/invalid indices are refused.
Validation and querying use heap-backed iterative stacks, not native recursion.
Fixed maximum bounds are one million nodes, eight million validation steps and
sixteen million query steps; callers can only tighten them. Pending/shared
branches consume the same query budget. Coplanar chains are not traversed here;
the future decal projector needs separate chain/clipping limits.

`model_point_trace_test` uses independent, hand-computed small BSPs and an
8,192-node chain. Controls cover all 256 flag bytes, adjacent representable
thresholds, first-hit node/order, LastNode, nonunit normals, half-unit backoff,
empty/start-solid cases, graph/work limits and nonfinite inputs. This validates
the stated primitive, not later projection, native storage, saves or rendering.

The optional `portable_actor_script_test GAME_ROOT --world-startup-only` also
queries the owned Training Model from JaimeReyes0's actual shadow birth point
and verifies repeatability and unchanged full runtime/save/GC state. It still
requires the original startup transaction to fail explicitly at unsupported
`Engine.Decal.AttachDecal`; this prerequisite does not bypass that failure or
publish a partly initialized campaign.

For this owned Training fixture the hit is node1848, surface1837, on the Z=-96
floor. Jaime's shadow birth Z is approximately -95.900185, less than half a
unit above that floor: the wrapper correctly clamps Time to zero. A second
diagnostic ray raised two Unreal units, without moving the actor, checks the
unclamped half-unit backoff. This is an observed original-data regression,
not proof that Attach has selected/clipped a coplanar polygon or drawn a decal.

## Next integration boundary

Actual Attach needs exact coplanar surface selection, original quad/RNG rules,
ordered per-surface native records and duplicated actor SurfList, transactional
detach/destruction/GC, a truthful client viewport binding, renderer publication
and an explicit save/restore policy. Multidecal and mover modes must remain
refused where their original contracts are incomplete. The original audit
records these dependencies rather than substituting an always-false shadow
native to advance startup.
