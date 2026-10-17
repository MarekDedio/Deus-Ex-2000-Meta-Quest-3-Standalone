# Selected native Model collision bridge (2026-10-17)

Follow-up: the nonempty owner point branch below is now implemented separately
with its own exact PC64 kernel, flags/packet contract and two-capture owner-hit
normal path. The owned fixture also establishes31 collidable null/null actors,
not227 assumed collidables. See ORIGINAL-MODEL-POINT-OWNER.md. Historical
verification counts and the original extent-only boundary below are retained.

This is collision-provider groundwork for real crate support, not a completed
Move/FindBase/falling/destruction implementation or headset acceptance. It does
not force startup, possession, relevance or actor flags, and does not give
crates generic Pawn health, hide-only destruction or fabricated inventory loot.
Native evidence and arithmetic tests remain in ORIGINAL-NATIVE-PRIMITIVE-DISPATCH,
ORIGINAL-LIVE-ACTOR-PRIMITIVES and the original Model/owner-coordinate reports.

## Data admission is not render admission

`DecodePortableCollisionModel68Body` and `LoadPortableCollisionModel68` share
the original v68/licensee0 serialized record layout with the render decoder.
They preserve Float bits, including inactive signaling NaNs, arbitrary raw
bounding-box Valid bytes and reversed endpoint pairs. `PortableModelBounds`
retains both `rawValid` and the existing nonzero `valid` projection. No renderer
normalization, finite sphere/lightmap policy or render-index cross-validation
is silently imported into a collision query.

The collision decoder still bounds source payload, compact counts, all twelve
decoded array capacities, zone count, actual object-reference ranges and exact
body consumption. These are explicit portable structural/resource admission
policies, not claims that an original empty LineCheck dereferences every array.
Active BSP numeric/index/topology validation belongs to the consuming kernel.
The render decoder and its strict whole-render-Model validation are unchanged.

The live provider uses at most64 MiB property/body scratch and16 MiB decoded
arrays per cold Model load. Those are separate from retained-cache accounting.
No original commercial executable or DLL is loaded or executed.

## Actual selected-object authority

`AActor.GetPrimitive` first resolves the current qualified Brush Object field,
then Mesh only if Brush is None. The earlier exact asset/class/field validation
still runs on every query. A selected Model is not replaced with Level.Model,
the first Model export, an Actor cylinder or renderer geometry.

The provider requires the actor's actual linked native ULevel and its current
non-reused map generation. Selected Models may belong to another loaded source
package; their native class and exact export identity remain mandatory. The
root FastTrace Model cache remains separate and is not populated or redirected
by a Brush query.

## Cache ownership and resource policy

The selected-Model cache has at most4096 entries and64 MiB accounted retained
storage. Its immutable geometry is keyed by actual current ULevel/generation
and selected Model UObject/source/export/path. No cache pointer roots an
UObject. Every hit checks current native identity; unload and Shutdown clear
all entries and byte accounting before map/object GC. Warm checkpoint restore
retains authored native objects and their cache; cold or same-filename map
replacement never reuses the generation.

Accounting includes cache/entry/geometry objects, the owning pointer-vector
capacity, all twelve geometry-vector capacities, and three string capacities
plus terminators. Geometry has unique ownership, so no shared control block is
omitted. This is a bounded retained-storage policy, not a process-RSS or system
allocator-overhead measurement.

Cold decode, copied keys, geometry identity and aggregate admission all finish
before publication. A replacement pointer-vector is allocated first; only
reserved, nonthrowing unique-pointer moves, vector swap and ledger assignment
remain after admission. Failed decode/identity/budget checks publish no entry
or ledger change. A successful immutable decode may remain cached after a
later active-query refusal/VM rollback; it never stores gameplay writes or
an already-validated query capability.

## Bounds and LineCheck

Model bounds capture current native owner ToWorld exactly once, including
for empty BSP, and transform all eight original box endpoint combinations.
Source Valid is ignored; reversed finite pairs are folded in original corner
order, with no Actor-cylinder padding.

Empty Model LineCheck performs bounded structural admission and returns
RootOutside before reading endpoint/extent values or capturing owner ToWorld.
The complete caller hit packet is unchanged, even if those inactive numeric
fields are invalid or the owner's native ToWorld implementation is unsupported.

Nonempty nonzero-extent queries capture ToWorld once and use the existing
verified owned-Model extent kernel/packet adapter. Misses write only Time2;
accepted hulls publish the original fields while preserving Next and Item.
Nonempty zero-extent Model LineCheck still explicitly refuses pending its
separate owner point-query packet adapter. It is not approximated by an extent,
FastLine or cylinder query. Both-null Brush/Mesh still requires genuine
Engine.Cylinder instance/lifetime support.

FastLine and owned extent work share one outer-host ledger:8,000,000 validation
steps and16,000,000 query steps, independent of query order. Per-call limits
are the smaller of each kernel's fixed maximum and the root's remaining work.
Each kernel fully validates on every warm query. The packet adapter optionally
returns statistics from the same single trace; packet/statistics publish only
on success. Refusals propagate to the same root transaction, never become
clear results or fallback collision.

## Verification

Strict ARM64 syntax checks pass for the runtime, decoder, owned Training test,
generated decoder/provider tests and packet-statistics test. Targeted strict
host builds pass, along with:

- Collision decoder:1767 checks,883 refusals, including every Float channel's
  signed NaN/sNaN/Inf/zero bits, all256 Valid bytes, exact array budgets,
  truncation/count/reference/tail controls and unchanged strict rendering.
- Generated actual native Level/selected-Model provider:700 checks,6 refusals;
  literal bounds and owned-extent packets, empty-field inactivity, cross-package
  selection, failed-decode atomicity, repeated active-kernel refusal, cached
  owner transform changes, warm/cold restore, same-filename replacement,
  unload/shutdown and source/save/revision/GC/clock invariants.
- Owned dormant Training:41435 checks,246 explicit refusals;1308 actors,
  1017 selected Models with bounds/extent adapter comparisons,64 Mesh actors,
  256 Mesh line probes, both actual crates/knife/crowbar and227 unavailable
  Engine fallback branches. All1017 Models retain700781 accounted bytes on the
  host. Full read-only state/source invariants pass. The reused geometry/math
  kernels are explicitly adapter oracles here, not new independent arithmetic
  evidence.
- Existing generated primitive1608/45 and FastTrace2399/47 controls pass.
  Owner extent/packet12213/23 includes3038 independent x87 sweeps and verifies
  successful statistics export and unchanged statistics on late refusal.

Cache aggregate growth/reuse and lifetime are exercised, not actual exhaustion
of4096 entries/64 MiB or a mixed-callback shared-work maximum. The full coherent
host rebuild passes. Fresh CTest:125 passed/143 registered/18 explicit owned-data
skips/zero failures in42.12s. Local ignored logs use
`artifacts/selected-model-*-20261017.log`. No new APK or headset acceptance is
implied by this checkpoint.

## Next gameplay dependency

The next priority is audited native default preparation, then actual CPU-owned
Engine/Transient/Primitive instance lifetime and current ULevel.Engine binding.
The existing native registrations do not supply inherited default buffers,
GameEngine ServerActors/ServerPackages native property declarations or their
config phase; an empty RuntimeObject/header would not be an original engine
allocation. See the additional evidence in ORIGINAL-ENGINE-CYLINDER.md.
Use prepared native defaults and explicit port embedding/construction inputs. This
must be a named collision-service subphase, not a claim of full UEngine.Init,
LoadMap, possession or startup. Original launcher allocation arguments are
still unproven; no guessed retail bootstrap is justified by successful class
registration or a configured class name.

Then integrate the original LoadMap SetActorCollision phase, hash ownership/
shared RNG/CollisionTag/ColLocation and collector under the same VM root.
Do not substitute the pinned overlap registry or omit bCollideActors actors
whose Brush/Mesh are None. Training's227 null/null actors are an all-actor
count, not an audited collidable-fallback count; the next owned fixture should
perform that qualified-field census. The nonempty owner point adapter can
proceed independently for zero-extent weapon/ray queries, but does not replace
the missing Engine fallback or unblock crate nonzero-extent sweeps by itself.
