# Fresh original brush tracker implementation (2026-10-31)

This increment implements the real fresh-constructor mutation path selected
after first Training player setup, under a diagnostic root which always rolls
back. It is not a completed original LoadMap, noninitial moving-object update,
live collision/render adapter, travel teardown, save/rebind or playable APK.
The production post-partition startup guard remains unchanged.

## Implemented selected graph

`quest_brush_tracker_pools.h` preserves actual world Model Num/index space and
zero free-record contents, with typed owner/parent pairs, vertex owners and
active-Mover versus free-point-next bookkeeping. Constructor order is Vectors
shrink, Surfs shrink, Nodes grow/zero/shrink, Verts grow/zero/shrink, zero owner
arrays. Counts are captured before Setup; post-Setup Points count is distinct.
Replacement arrays have exact capacity under the pinned library contract,
with overlapping allocation admitted before creation, not a shrink_to_fit hint.

Node allocation retains original wraparound cursor behavior. Vertex allocation
retains the strict cursor+NumVertices<end bounds on both passes and marks the
successful run backwards. The unusual selected count0 behavior is preserved,
not corrected into an empty allocation. Point allocation asserts count/owner
correspondence, pops the actual absolute free index without zeroing its XYZ,
or appends Point then bookkeeping; freeing replaces owner with the next link.
64-bit UObject identities are never cast into package references or int32.

`quest_brush_poly_filter.h` implements actual FilterPoly traversal: half split
at14 vertices, copied-output-first recursion, prefilter FRONT priority, strict
CSG/outside rules, retained coplanar orientation comparison, front-first split
recursion/insertion, live parent reacquisition and back tail traversal. Tails
do not consume recursive depth. The caller supplies shared work and cumulative
scratch admission; native stack allocations remain charged until the mover's
marker, including allocations in returned recursive calls. Portable recursive
frame/value scratch is conservatively charged as well.

`quest_brush_bsp_insert.h` selects original versus coplanar attachment parents,
copies the chosen parent's old mask, preserves original-parent zone/leaf and
mask targets, fills exact plane/flags/count fields, allocates owned vertices
and points, and only then attaches actual BSP and separate mover-root links.
A typed root destination uses Model-root or node index, never a pointer into
a reallocated vector. No-node and no-vertex branches retain original partial
write behavior. Budget exceptions unwind the caller; they do not manufacture
the point helper's unobserved -1 result. See ORIGINAL-BRUSH-ADD-POLY-DETAILS.md.

## Runtime candidate, ownership and rollback

The actual Level can own a typed native tracker with its exact Level/Model,
generation, original counts, arrays/cursors and initial-completion state.
It is not an invented UObject, actor birth, package or export. Runtime GC
visits typed owner edges. Constructor publication occurs only after complete
mandatory Update and topology checks, with an owning beforeimage rooted on the
actual Level; rollback swaps that owner before Model/Polys/actor retirement.

The current probe requires an absent prior tracker and performs real Setup,
pool construction and Update(nullptr). The pending list is built from current
ascending Actor slots and processed in reverse. It sets assimilation and pose
caches before the dependency/flush passes, takes their genuine fresh -1-root
short paths, then clears assimilation, builds original model coordinates,
transforms full source polygons, builds their sphere, mutates prefilter flags,
fills world surface vectors/points, filters/inserts and clears final root-chain
node bit0x20. A noninitial root explicitly refuses its missing provider; the
fresh short branch is not an empty implementation for later updates.

The same constructor is connected after the genuine live-viewport player loop
in original startup. Inside the actual Client branch it authenticates the
current Level's real outer Package and FName, compares that name with Entry,
and constructs only for the selected non-Entry branch. Probe-only forced GC
and preview disposition are separate wrappers, not invented original native
side effects. A detached startup observation records actual completion/owner/
topology/count results; the Training CLI requires those results before accepting
the retained later startup boundary. This code connection alone is not proof
that the real startup test has passed; outcomes belong in STATUS.md.

Prepared tracker arrays, pending roots, transformed polygons/vertices, filter
scratch and insertion growth overlap join the same shared caller byte/work
ledgers. Native Model/Polys and reflected field beforeimages remain in that
outer root. A forced refusal after actual BSP/root attachment must restore
the complete pre-call save/state/revision/native-geometry/GC baseline.

The first startup-context run exposed a real stage-overlap failure before
Setup writes: `UE1 model v68: decoded arrays exceed working-byte budget`
after493.737s at266870 instructions/37932 writes, native work13954092 below
the unchanged16777216 opt-in ceiling. That failed expected-boundary gate is
retained, not counted as startup/rollback/source acceptance. A dormant-map
constructor pass did not cover this large live startup-journal allocation peak.

The correction clones an already-authenticated immutable cached world Model
VALUE into a separate private owning payload, after admission of all array/
path bytes and identity workspace. The current query resolver authenticates
exact Level/Model/source/export/generation and actual const allocation first.
Source arrays remain const/nonaliased and their validation proof is never
borrowed by the mutable copy. With no existing cache, the original collision-
only cold decoder and its bounded scratch semantics are unchanged. Prepared
Model copies and old beforeimage edge roots remain in the shared scratch
ledger through nested identity admission and nonfallible ownership transfer.
No byte/work cap was raised. The owned fixture exercises both cold and cached
constructor paths; corrected final gate outcomes belong in STATUS.md.

Corrected real first-Training startup verification reaches the retained later
guard in353.268s with actual25 Movers/150 polygons/150 owned nodes/600 vertices/
600 points recorded in that SAME caller root. Native work14169034 remains under
16777216; full rollback/45 unchanged sources and cold source-cache replacement
verify. This is expected-boundary evidence, not published startup. Final owned
Setup/tracker1791 checks/9 policy-or-missing-source refusals also pass, including
the read-only FastTrace cache primer and cached-copy agreement/checkpoint tests.
Final regression scope and remaining services are recorded in STATUS.md.

## Active index-preserving read boundary

The construction helper authenticates exact current Level/Model/payload and
generation. Original indices remain unchanged. Dynamic reads require a real
owner pair, matching typed surface owner, a real owned vertex run and matching
dynamic point owners. Selecting a free or foreign dynamic entry refuses.
All selected child links are checked after construction. Free pool bytes are
not patched, compacted or silently pruned to satisfy source validators.

This private construction read path is not yet the LineCheck/FastTrace/Region
or renderer capability. Raw serialized and immutable-source kernels are
unchanged. No old authored-source proof can authenticate the mutated payload,
and no public alreadyValidated switch or stale immutable snapshot is added.

## Verification and remaining scope

Generated controls exercise flags/outside/coplanar decisions, recursive order,
live callback mutation, half splits, limits and selected-read refusal; exhaustive
small-pool cursor/owner controls check allocation, strict wrap/fit and point
reuse; insertion controls check parent/mask/plane/owner stores and partial
failure. The extended owned first-Training fixture compares actual constructor
counts, reversed mover order, native active-owner totals and root chains, and
complete success/failure rollback against original checkpoints. Test code is
not itself passing evidence; final outcomes are recorded in STATUS.md.

Original binaries remain read-only file data. No original DLL/EXE is executed.
Other-map integration, VR layouts/controls and existing safety caps are not
changed. Complete noninitial dependency propagation, flush/free/destructor
semantics, authenticated live query/render access, geometry archive/rebind,
later reset/render/detail startup, original gameplay Tick, crate/loot/gravity
and headset acceptance remain required before claiming a new playable build.
The bounded static dependency/flush follow-up is recorded separately in
ORIGINAL-BRUSH-DEPENDENCY-FLUSH.md; it is not a later-update implementation.
