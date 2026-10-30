# Moving-brush tracker: runtime integration notes (2026-10-30)

Read-only source review for **first `00_Training` only**, alongside the
original contracts in `ORIGINAL-MOVING-BRUSH-TRACKER.md` and
`ORIGINAL-BRUSH-TRANSFORM.md`. This document is the only edit from this
review. No compiler inputs, builds, tests, original binaries, or runtime
state were changed. Line references below identify the inspected source
snapshot and may move as parallel development continues.

The new full FPoly decoder is a necessary source foundation. It is not yet
bound to live RuntimeObject/Model state, a completed tracker, or permission
to remove the explicit post-partition startup guard.

## Concrete current integration sites

| Site | Current behavior | Required tracker integration |
| --- | --- | --- |
| `portable_unreal_runtime.cpp:466` RuntimeObject | nativeLevelModel points to the loader-owned Model; only its64 transient zone timestamp Floats have a typed native payload | Add genuine typed mutable Model/UPolys state and Level-owned tracker, with authenticated identity/lifetime |
| `:1447` RuntimeNativeModelCache | source-file-derived shared **const** Geometry and OriginalImmutableCollisionModel, weak Level/Model identity keys | Keep immutable source asset separate from the authoritative live Model; no const_cast or stale query lease |
| `:5970` NativeSelectedModel | separate immutable per-Brush collision geometry decoded from original Model export | Preserve original brush-local collision source where appropriate; share actual Model/Polys identity, not renderer triangles |
| `:6014` NativeLevelTraceModel | lazily loads original world Model from disk, keyed by generation/Level/Model/source/export only | Query current live topology once tracker construction mutates it; key any derived leases/proofs by exact geometry revision |
| `:6050` NativeWorldCollisionSource | returns immutable proof-backed world source; no call sites in inspected runtime | Do not mistake its existence for current mutable-world integration or a ready original Level collision adapter |
| `:6073` NativeFastLine | queries NativeTraceModel using full kernel validation/work caps | Read current tracker-produced Model and preserve root work accounting |
| `:9195` startup guard | throws after actual SpawnPlayActor loop; caller rolls back | Insert full selected constructor after client/player setup, within same caller journal; retain later-services refusal |
| `:9786/:10951` Before/Touch | snapshots reflected maps, state, selected native fields; no Model/Polys/tracker state | Add dedicated first-write native before-images; Touch(actor) alone cannot undo geometry/links |
| `:11434` ValidateStateBudget | native journals/scopes measured; immutable wrapper overhead only in current Model-cache branch | Admit complete mutable geometry, tracker arrays, UPolys, before-images, roots and scratch peaks |
| `:11979` Region | follows NativeTraceModel then resolves zone actor reference against Level.Model package | Use the same current topology and retain correct original zone/source reference identity |
| `:16172` native Model snapshot | reports immutable cached nodes/proof, no live topology revision | Distinguish source cache from actual live counts/ownership/revision; do not report stale source counts as tracker proof |
| `:16302` BindAuthoredActorSlots | authenticates root Level/Model pair and stages native ownership | Establish genuine baseline payloads while source tables/objects are available; do not publish tracker early |
| `:16601` final loader publication | sets nativeLevelModel/mapGeneration only after loader summary success | Preserve this identity publication order; loader baseline is not successful original LoadMap completion |
| `:14064/:16650` shutdown/unload | clears weak Model caches before GC | Retire live tracker and references before Model/Level/export ownership disappears; do not substitute cache reset for original destructor |

## Immutable-source proof is incompatible with in-place mutation

`quest_immutable_collision_model.h` deliberately creates an **actual const
allocation**, not a const-qualified view of mutable vectors. It exposes no
mutation entrypoint and keeps an atomic, persistent null-hull-validation bit.
`quest_model_extent_trace.h:275` may skip structural validation once that
exact immutable source has been validated; point-overlap's immutable world
overload shares the proof for its nonzero-extent/null-owner branch.

Three tempting substitutions are therefore wrong:

- const_cast and mutate the allocated const Geometry: violates its storage
  contract and makes its retained validation proof stale;
- allocate tracker geometry elsewhere but leave NativeLevelTraceModel on its
  original-file cache: FastTrace/Region would ignore moving BSP changes;
- clear/reload the cache after each update: reloads the old authored file and
  discards the required dynamic surfaces/nodes, rather than retrieving the
  live Model. Owned game files must never be rewritten to simulate live state.

The current identity keys prove which authored Model was decoded, not its
mutable revision. A same-address/same-map-generation test cannot authenticate
updated topology. A safe implementation needs one authoritative live Model
plus versioned/leased query access. Any immutable query snapshot must be an
independently owned exact revision with no mutable aliases; obsolete leases
must not continue to masquerade as the current world. Queries nested during
the original constructor/update must observe the state allowed by the source
pass order, not an unrelated prior committed asset snapshot.

## Native pool Num is not a serialized active graph

The original ctor changes actual Nodes.Num to `min(2*old+512,65536)` and
Verts.Num similarly, zeroing newly added free slots. Ownership arrays decide
which dynamic entries are active. Unused native nodes therefore contain
zero Front/Back/collisionBound values; they are not initialized to -1 merely
to satisfy a portable serialized-data validator.

`quest_model_point_trace.h` validates **all** nodes, including unreachable
components, then checks front/back cycles. FastLine reuses that admission.
`quest_model_extent_trace.h::ValidateGeometry` likewise visits all nodes,
and reads each non--1 collision bound before its full graph-cycle check.
Blindly appending the original zero pool into today's PortableModelGeometry
and passing it to these raw serialized-model contracts can inspect inactive
reserve entries as if they were authored topology, read nonexistent hull0,
or reject a free-node graph. Merely filtering unreachable nodes changes
their current promised malformed-source admission and is not a justified
global fix.

Keep existing raw/immutable serialized-source contracts intact. A live
tracker needs an explicit authenticated active/ownership-aware query view or
equivalent separate representation with the original index space preserved,
plus validation and work/byte accounting appropriate to mutable runtime BSP.
Do not compact/remap indices, rewrite free-slot bytes, or skip validation
based on an unauthenticated boolean. Source evidence for which consumer
visits which dynamic link is still required; a static triangle mesh is not
that view.

Native FBspNode `+28` is coplanar BSP child, while `+30` is also used by the
tracker as the per-mover root chain. PortableModelNode currently calls the
latter word `renderBound`; runtime code must retain the actual overlay use
and not confuse root-chain traversal with front/back/plane graph traversal.
PortableModelSurface stores authored package-reference ordinals, whereas the
original SetupActor writes a real Mover pointer into Surf.Actor. New dynamic
surfaces require typed runtime reference ownership or an authenticated
identity-sidecar, not casting a host pointer into a signed package ordinal.

## Full Polys identity, GC and source/native split

No `PortableBrushPolys`/`LoadPortableBrushPolys68` call site exists in the
inspected runtime.cpp; the detached codec is not a live UPolys payload.
The source decoder retains all records, source actor/texture references,
source FName index, pans and links. Native SetupActor then rewrites each
live polygon LinkIndex and appends actual world Surf/Vector/Point slots.
Keep serialized immutable provenance separate from those mutable runtime
values while binding both to the actual Model.Polys object.

`RuntimeObject::Mark` at`:677` marks nativeLevelModel and numerous explicit
native owning edges, then immutable source references. It does not mark
future Model.Polys payload references, dynamic Surf mover/texture references,
tracker owners, or journal before-image references. Strong ownership must
follow the actual native lifetime; raw weak cache identity keys are not GC
roots. Provisional tracker work/pending actors and rollback payloads need
temporary roots until retirement. Replaceable runtime edges must be removed
when overwritten/flushed; appending them forever to source `references`
would retain retired movers/materials and conceal stale ownership.

The Level owns its tracker; it is not an invented UObject birth, export,
native package object or actor slot. Constructor publication must occur
only after the real positive operation succeeds. Authenticated current
Level/Model/generation and actual Brush/UPolys object identities remain
mandatory. Use the live post-partition actor ordering, not the original
export list or a cached render actor list.

## Same-caller journal, publication and cleanup

`PortableActorVmHost::BeginRoot` at`:3499` establishes one outer transaction.
Commit`:3538` performs fallible preparation before final nonthrowing
publication; Rollback`:3625` restores native ownership before retiring
provisional object identities. Existing Before/Touch does not cover Model
arrays, UPolys links, Brush root chain, node flags, tracker arrays/cursors,
or tracker publication. Those writes must join a dedicated native journal
before mutation, not a separately committed helper or an unbounded full copy.

At minimum, before-images must restore world Points/Vectors/Surfs/Nodes/Verts
including counts/capacities as required by the admitted ownership contract,
static-node child links/ZoneMasks/prefilter flags, per-mover Brush root links,
actual UPolys LinkIndex writes, bAssimilated, OldPos/OldRot and tracker
free/owner arrays/cursors. Typed reflected cache writes may use the existing
VM reference undo only when they genuinely bind the correct declaring schema;
that does not journal unrelated native geometry.

Rollback must be nonallocating/noexcept and restore those references/state
before export, actor, Model or linker retirement. Track before-image memory,
growth overlap, per-poly scratch, split recursion, FSphere vertices and
GC roots in the **same** bounded caller's ledger/work caps. A diagnostic that
successfully runs the ctor and then reaches the existing later guard must
return the pre-call state, not leave an installed tracker behind.

Current Commit's worldChanged predicate includes actor/registry/order/decals
but no native Model revision. Native topology writes must participate in
publication classification so visual/query consumers cannot retain a stale
world merely because no reflected actor field changed. A new geometry
publication cannot happen earlier than the other effects of the same root.
Rendering/decal code also uses cached rootModel geometry and checks fixed
surface/node counts (`:11147/:11240`); dynamic appends require corresponding
identity/count/revision handling, not silent reuse of source counts.

Original tracker destruction truncates appended world arrays, repairs static
child links, and frees bookkeeping. `UnloadPortableRuntimeMap` currently has
explicit refusals for unsupported deferred/hash/archive ownership; keep the
same honest boundary for unsupported tracker travel/retirement. CPU shutdown
may have a distinct complete teardown path, but clearing weak caches alone
does not reproduce the original live destructor or an atomic startup undo.

## Next first-Training proof, not other-map expansion

Implement authenticated live Model/Polys ownership and a bounded journal first,
then complete the real ctor, transformed polygons and BSP insertion from the
original contracts. Prove a positive actual Training mover cohort, exact
surface/point/vector/node/link effects, correct query visibility and full
failure rollback/unchanged owned files. Keep the later subsystem/detail guard
until those selected services are implemented too. These notes do not call
crate destruction/loot/gravity, full original startup or a new APK finished.
