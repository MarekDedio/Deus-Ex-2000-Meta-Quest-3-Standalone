# Model-owned mutable geometry and sphere prefilter

2026-10-30. First Training integration only. This is a native ownership/mutation
prerequisite, not a completed brush tracker, live BSP insertion, successful
startup, crate/loot/gravity acceptance or a new APK.

## Ownership and journal

`RuntimeObject::nativeMutableModel` owns a typed mutable payload on the actual
original root UModel. Its baseline contains all serialized Model arrays and
records, source references, native map generation and a geometry revision. The
native ULevel.Model pair, indexed original Model export/class and decoded source
identity are authenticated before ownership publication. The immutable authored
Model cache remains independent and is never const-cast, rewritten or reloaded
as a substitute for the mutable current world.

`MutableRootModel` requires the existing outer VM transaction. Initial decoding
and copy-on-write staging are admitted before allocation using the shared native
state ledger; input/property/body scratch and retained decoder arrays cannot each
borrow the full allowance. Source decoding and diagnostic fingerprint loops
also consume the caller's native work. Actual retained capacities are measured
before publication. No byte/work cap is increased.

A first-write journal roots the exact Model, retains its old owning payload and
installs the prepared detached payload with nonthrowing moves. This owner journal
does not serialize a native pointer or create a fake UObject birth. Root rollback
swaps the previous payload back without allocation before provisional identities
retire; successful ordinary publication participates in world-revision change
classification. The observation API always selects root rollback, including on
a successful operation. Its returned packet never exports a mutable reference.

Current mutable records still contain original package-reference ordinals. The
prefilter changes only Flags bytes. It introduces no dynamic surface Actor or
Texture pointer edges, live UPolys links, free pool or tracker owner arrays. Those
require their own typed native storage/GC ownership before insertion. This is not
permission to put a host pointer into a signed source ordinal or append permanent
roots to the source reference list.

## Same-world query selection, not a validation shortcut

`NativeLevelTraceModel` selects the actual current payload when present and
checks its generation/export identity. It cannot silently fall back to the
authored BSP while native flags have changed. `NativeWorldCollisionSource`
explicitly refuses borrowing the immutable source proof for a mutable payload.
Existing raw serialized-model validation contracts are unchanged.

This first payload contains only original source nodes. The callback authenticates
their exact current owner and bounds on every selected read/write. It is not yet
the active/index-preserving capability needed after native pool growth; zeroed
unowned reserve nodes must not be treated as an authored validated graph. No
claim is made that raw collision/render consumers can already accept such pools.

Native mutable geometry makes the runtime unsaved and checkpoint capture refuses
until geometry archive/rebind is implemented. Source-cache timestamps/proofs do
not stand in for saved live geometry. Shutdown owns ordinary payload destruction;
original brush tracker teardown/array truncation is still a separate service.

## Exact prefilter service

`quest_brush_sphere_prefilter.h` implements the recovered root/helper algorithm
from ORIGINAL-BRUSH-SPHERE-PREFILTER.md using existing integer PC64 arithmetic.
Each visited node first clears Flags bitsC0, then compares retained distance to
the retained negative radius before the front test. Equality overlaps; negative
finite radii retain original priority. Overlap recurses Back first, then tails
Front. Root arithmetic remains root arithmetic on root tails; helper tails keep
the helper schedule. Empty Models consume none of the inactive sphere/callbacks.

The borrowed read/write/work callbacks share one caller budget. Every visit and
selected edge, including terminal-1, consumes work. Shared subtrees are revisited,
not deduplicated; cycles exhaust the caller allowance. A bounded recursive depth
may be tightened from256; tail traversal does not consume recursion depth.
Live child links are reacquired after flag writes and Back recursion. Unvisited
siblings and free/unreachable slots are not reset. Nonfinite active data and
invalid selected ownership/indices explicitly refuse after any preceding writes;
the caller journal, not an invented local no-op, restores them.

The runtime bridge applies this operation to the Model-owned arrays and increments
the payload revision only after completion. It verifies current query selection
and immutable-proof refusal. A deliberate test-only failure after a selected
flag write exercises partial-mutation rollback in that same VM root.

Independent generated x87 arithmetic/order tests and the owned Training fixture
are separate: hardware tests are the numeric oracle, while owned tests compose
real mover spheres, source arrays, owner/journal/queries and full checkpoint/GC
checks. Generated controls pass9737 checks/3212 independent comparisons,
35 explicit refusals and3 effective retained/live-order controls. The owned
fixture passes154643 checks/325 identity refusals, including1994 actual
Model-owned flag writes for the25 real non-static Training mover spheres,
25 successful root rollbacks and25 injected partial-write refusals. Its final
checkpoint/revision/slots/time/GC, source-cache identity and17 original source
fingerprints match the pre-test baseline. There is no mutable payload left
published after these observations. The earlier source-transform stage still
works on copied polygons; it is distinct from these actual native Model writes.

Desktop, strict ARM64 and Android-native compilation pass on the corrected
snapshot. Their first discovery runs had a wrong budget-helper identifier;
those are failed gates, and only the specifically verified superseded compiler
leaves were stopped after that concrete diagnostic. Final regression and source
audits are recorded in STATUS.md after completion. No compiler inputs are edited
during active builds/tests. No original commercial DLL/EXE is loaded or executed.

## Remaining implementation

Live UPolys identity/GC, SetupActor LinkIndex/root/OldPos/OldRot writes, dynamic
pool growth/ownership, recursive FilterPoly and AddPolyToBsp, later update/flush/
teardown and active current-revision collision/render integration remain. The
post-partition startup guard stays in place; later selected subsystem/input/
render/detail services and live Training crate tests are not bypassed.
