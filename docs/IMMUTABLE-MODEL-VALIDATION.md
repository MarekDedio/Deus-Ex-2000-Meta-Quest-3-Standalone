# Lazy immutable world-Model validation

Implementation, 2026-10-26. Actual content integration remains the first
`Maps/00_Training.dx`; this is not an assertion of completed startup, gravity,
collectible loot, an APK or headset acceptance.

## Measured motivation

The prior real Training update stopped during validation of null-owner
`00_Training.Model36`: aggregate validation7,937,992 of8,000,000, aggregate
query8,344 of16,000,000, then the next validation exhausted its remaining
62,008 steps. This was not traversal or shared native-work exhaustion.
See [the measured failure](ORIGINAL-TRAINING-COLLISION-WORK-GAP.md).

## Immutable authority, not an unchecked flag

`OriginalImmutableCollisionModel` privately owns a genuine const geometry
allocation. Fresh `LoadWorld`/`LoadCollision` factories construct directly
from loader results; `CopyOf` deep-copies mutable input. There is no public
constructor taking a pointer, moved mutable geometry, or a validation bit.
Callers cannot retain legal mutable aliases to these const allocations.
`GeometryLease` retains const geometry lifetime without lending proof authority
to the raw-geometry API.

A private atomic proof is published only after full successful null-owner
hull validation. It is not set on failed or incomplete validation. A later
query failure can leave completed immutable validation warm: this is asset
cache state, not a committed gameplay mutation or a fresh work allowance.
Concurrent cold calls may each do the full validation; none can observe an
unfinished proof as complete.

The runtime retains that source alongside its existing geometry lease, still
authenticating the actual map generation, Level/Model identity, source path,
export and object path on every use. Source/lease identity is checked too.
The cache clears before map GC/unload/replacement. Newly retained wrapper and
control-block overhead participates in the live state budget. No new copy of
the existing runtime geometry is retained by this integration.

## Exact reuse domain

World extent LineCheck and world box PointCheck already use the same full
`ModelExtentTraceDetail::Validate(..., owned=false)` contract. Their typed
source overloads share this one successful proof, lazily at the original
validation point. Cold work is fully charged; warm validation costs zero.
Current array/cardinality and positive query/pending caps still apply, even
when warm. A zero validation allowance is legal only for a warm proof;
uncached validation with zero allowance refuses. Root validation/query caps
are not increased or reset.

Raw mutable-geometry APIs keep full validation on every call. Owned queries
and zero/empty occupancy keep their raw contracts, and do not create or borrow
the null-owner hull proof. In particular, owned hull bounds may legitimately
contain inactive NaN/Inf/reversed values that null-owner validation rejects.
FastLine and point-line queries remain unchanged/full-validating. No hull
validation is made eager during cache decode, so inactive bad hulls cannot
newly reject those branches.

The numeric extent traversal and occupancy bodies are shared unchanged by
raw and typed entrypoints. Hit packets remain atomically published only on
success, retaining all original caller-seeded fields and defined writes.
Query work, clipping, transforms, hull/bevel operations and malformed-data
checks are not replaced by a cached collision result or a clear-trace stub.

## Verification scope

The dedicated helper driver checks both extent-to-box and box-to-extent warmup
orders; exact packets and query counters; cold validation and warm query budget
cuts; mutable original/vector-alias isolation; raw/owned/zero separation;
malformed/pruned/cyclic data; independent same-filename source replacement;
and const-lease lifetime. Optional owned-data mode exercises both fresh loader
factories on the actual first Training model.

The first-Training startup probe records proof state before/after rollback,
and verifies actual unload plus same-filename generation replacement starts
cold. Its existing complete save/revision/hash/RNG/client/lookup/Actor/GC undo
checks still apply. This is diagnostic evidence, not playable publication.

Actual first-Training repeat2 now passes the former Model36 validation cap:
237,922 instructions /19,289 writes,121 slots /120 calls,13 stasis returns /
12 deferred entries. Cold proof0->1 and warm1->1 reach the same later
Engine.GameInfo native-class gate, with complete gameplay undo. Same-filename
reload changes generation1->2 and starts with no cached geometry/proof.
See [the next GameInfo gate](ORIGINAL-TRAINING-GAMEINFO-TICK-GAP.md); the world
frame itself still does not complete.

Dedicated generated controls pass74 checks /36 refusals; optional actual
Training world/collision-loader mode passes77 /36. Existing extent kernel
passes60,055 /64 including96 successful real-map contacts and the independent
scalar oracle. Separate actual crate415 /5, deferred429 /21, animation588 /24
and original FastTrace252 /0 controls pass. FastTrace retains16 clear /32
blocked supplied Training rays. These are backend tests, not Quest rendering
or frame-rate validation.

Paired-door repeat2 keeps both56-unit movements/two completion callbacks and
now reaches GameInfo at238,258 instructions /19,357 writes. Two original
crowbar Fire cycles repeat2 retain228,837 /26,047,48 observations/two cycles/
two notifies/four actual traces/two crate changes/nine births before later
FExec. Both verify whole gameplay undo and source/proof lifetime behavior.

Final verification: full desktop rebuild, Android-native link, strict GNU
helper and ARM64 helper/runtime syntax pass. CTest217 executed /256 registered /
39 explicit owned-data skips /zero failures75.39s. Production Actor-update
driver8,825/163 before final malformed-schema control exits0. All45 original
baseline files and11 frozen compiler inputs retain exact size/timestamp/SHA256;
all build/test/probe jobs are terminal. No new APK was packaged or installed.
