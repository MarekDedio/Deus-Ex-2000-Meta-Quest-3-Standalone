# First Training: complete movement coordinator (2026-10-19)

The immediate target remains **only `Maps/00_Training.dx`**. This checkpoint
implements the complete ordinary outer MoveActor protocol needed by Training,
not a collision-free transform setter. It does not yet bind native266/299,
publish original world startup or establish playable crate destruction, loot
or gravity. Other-map integration and acceptance remain deferred.

## Original operation and implementation

The complete source audit, source hashes and static disassembly provenance
are in ORIGINAL-LEVEL-MOVE-ACTOR.md. No original commercial DLL/EXE is loaded
or executed. `native/quest_level_move_actor.h` implements the ordinary body
`103990E0..10399F5F`, excluding only profiling counters/timestamps:

- Original Static/Movable/editor eligibility; both lazy small-Delta exits keep
  the caller's whole hit untouched. Rotation-only success discards tiny Delta.
- Complete fresh hit initialization before mark acquisition. One retained
  mark owns the original sweep list through nested moves and all callbacks.
- The verified PC64 numeric geometry/backoff, distinct moving-brush gates,
  live slot0 validation, two radius reads and interleaved endpoint/start reads.
- First blocking hit in original live-list order, with separate ignore-Pawn,
  ignore-Base, reciprocal-Base and blocking-matrix decisions. No re-sort,
  deduplication, self-excluding renderer ray or assumed unobstructed success.
- Complete child-carry and higher encroachment provider calls, by-value parent
  arguments and retained native Level. An ordinary veto keeps earlier child,
  callback and hit effects; it does not roll back the root as if it were an
  unsupported-provider error.
- Live Remove/Location XYZ/Rotation Pitch-Yaw-Roll/Add publication. `bTest`
  still mutates pose/hash, while suppressing carrying, encroachment and the
  subsequent bump/touch work. It is not a read-only preview flag.
- Exact BumpWall(Location,Normal), retained target Bump(source), then source
  Bump with the caller's current hit Actor after that callback.
- Original-list touch scan, live hit Time/Actor/Next and four ascending live
  Touching slots. The touch Time comparison admits below **or unordered**;
  ordered equality/greater ends the entire scan. IgnorePawns is not reapplied.
- Movement-time SetActorZone(test,0), mark release, then the final ordered
  positive hit-Time test. Zone/cleanup effects can change that return value.

The provider boundary deliberately requires **complete** recursive MoveActor,
child carry, higher encroachment and movement-time zone services, all sharing
the same actual VM/hash root. Missing active providers refuse. Inactive
branches are not eagerly demanded. One caller work hook charges every ordinary
provider call; the live-hit cap is cumulative over both passes and refuses
instead of returning truncated success.

Mark cleanup is noexcept, allocation-free and unmetered. Cleanup on veto and
exceptions is an explicitly stronger portable resource guarantee: the retail
veto/EH does not prove a Pop. It does not erase earlier gameplay effects. The
coordinator itself does not open an actor transaction or claim to restore
partial writes; the real enclosing root must poison and roll back on refusal.

## Verification boundary

`desktop/quest_level_move_actor_test.cpp` uses a separate provider interpreter
and hardware-x87 arithmetic oracle, including the saved/restored PC64 control
word, Double sqrt argument and individual Float spill boundaries. Expected
results do not call the production movement numeric/protocol helpers.

It compares raw caller hit words, partial actor state, provider order/work,
all sweep and by-value carry/encroachment packets, live hit records, marks,
zone/carry effects and success/refusal. Controls cover all option masks with
noncanonical signed integers, full-range rotations, mutable callbacks,
moving-brush/class distinctions, partial blockage, NaN/infinite hit Times,
ordinary veto effects, `bTest` mutation, cumulative cyclic-list refusal,
provider/work failure prefixes and nested LIFO cleanup. Stateful getters
distinguish repeated reads from an incorrectly cached snapshot.

`desktop/quest_level_move_actor_composed_test.cpp` additionally connects the
**real** child-carry helper to the **real** outer coordinator with recursive
calls. Its lower world/hash/events/zone providers remain mocks. That suite
can establish the helper binding, not original VM/hash publication or physics.

The coordinator suite passes144728 checks/217 explicit refusals,3828
independent provider/hardware cases,184 provider/work failure prefixes, three
nested controls and seven literal state-changing read-order controls. The
composed helper suite passes265 checks/eight cases/one shared-work refusal.
These are distinct scopes, not numbers to combine into a physics claim.

## Native per-field temporary-pose authority

`native/quest_native_actor_pose.h` now supplies independent nearest-field
Location/Rotation lookup, and the actual runtime `Read()` seam uses those
lookups separately. Existing lower-query full-pose scopes remain read-only.
The new mutable Location-only scope does not invent or freeze Rotation.

Trusted native writes can capture an opaque host/slot/actor/generation token
for the nearest mutable Location layer. A read-only nearest layer refuses;
only genuine absence selects the future durable-write path. Rotation writes
must separately establish that no read-only Rotation layer is active. A
temporary Location write changes only that host-owned layer, never the
reflected map, ColLocation, hash, renderer or animation clock. Scope release
discards it without restoring an unrelated newer durable Rotation.

Tokens use monotonically consumed generations, including across reuse of the
same actor and fixed slot. Ordinary writes reject stale/foreign/shadowed
tokens. Delayed VM undo is noexcept/allocation-free and may restore only its
still-active matching generation; after release it cannot mutate a replacement
or resurrect old scope data. A journal's shared write closure must validate
ordinary authority immediately before WriteReference, then use generation-safe
raw undo for later restoration. No closure may capture a stack-local scope
pointer. The owning host must outlive its journal.

The pose tests distinguish mutable-over-read-only, read-only-over-mutable,
nested mutable scopes, other actors, exception unwind, delayed/reused-slot
undo, depth and lifetime-generation exhaustion. They preserve raw signed
zero/subnormal/infinity/quiet/signaling NaN words and prohibit allocation while
valid stores, raw undo and out-of-order cleanup run. Their durable Rotation
fallback is explicitly fixture storage, not a real recursive native299 proof.

This is a real runtime **read-seam** change and the authority prerequisite for
the higher reverse push, not an implemented higher-encroachment bridge. No
existing generic active-hash property guard is weakened; native266/299 and
trusted durable pose publication remain separate required work.

## Remaining actual Training integration

The runtime admits the header for target compilation, but native266/299 remain
unsupported until the complete actual-provider bridge is safe:

- Retain every opaque hit list through its owning mark, including nested
  movement and encroachment requeries, without invalidating earlier handles.
  Current query arenas use **local one-based** record handles; separate arenas
  must not accidentally alias. A host-private root-qualified monotonic handle
  range must rebase first/Next, preserve global Next in copied hit packets and
  reject popped/reused/foreign roots. Mark release must invalidate the suffix
  before allocation-free destruction and never replenish shared work.
- Supply exact native fields, live Level actor slots, class identity and
  current hash Remove/Add, plus trusted qualified pose writes which preserve
  unrelated animation clocks and do not weaken generic active-hash guards.
- Connect recursive Move/Smooth, higher encroachment and real selected event
  packets to the same Execution and shared cumulative work/memory budgets.
- Distinguish lower read-only full-pose query scopes from the higher reverse
  push's mutable **Location-only** authority. A nested committed source
  Rotation must stay visible and survive raw Location restoration. A nested
  source Location write targets the active temporary slot and is discarded
  on restoration, as in the original raw-field protocol; hash effects must
  not be silently repaired.
- Exercise actual first-map success/refusal and full save/state/hash/clock/
  lookup/client/GC/RNG rollback, then advance the authentic startup boundary.

All retained hit-vector capacities must remain charged to the actual host
through nested query callbacks. Collector/producer vectors and Model workspaces
overlap that retained storage: independent preflights alone do not establish
the combined peak. Query plus adoption/late validation must poison the same
hash root on failure, and world-only list ownership must not initialize a hash
or consume CRT draws merely to obtain temporary storage.

The subsequent bounded store and connected query-scratch correction are in
TRAINING-MOVEMENT-HIT-STORAGE.md. Store/coordinator composition now has tests,
but actual host ownership, query adoption and native266 routing remain required;
including the store header is not that binding.

Passing the mock suites does not make first Training startup or crates ready.
No APK is packaged solely for this backend prerequisite. The last installed
build remains unchanged until a playable improvement is verified and packaged.

## Build and regression evidence

The coherent full host build, strict ARM64 runtime/new coordinator/composed/
pose-test syntax checks, and Android native compilation/link pass. Default
CTest passes150 executed/174 registered tests,24 explicit owned-data skips,
zero failures in51.89s. Those skips are not claimed as actual-map validation.

The separately rebuilt actual first-map regression passes1090740 checks/77
explicit refusals using38 exact Quest source packages,1308 actual native-slot
actors,120 collidable/nondeleted actors,478 registration links and94 actual
primitive calls. It verifies the existing real providers, pose-scope behaviour,
save/revision/lookup/hash/client/clock/GC/RNG rollback and source immutability.
It does not call the new detached coordinator through an actual movement
provider. Authentic startup still reaches BeginPlay slot126,
`00_Training.DeusExMover16`, Engine.Mover.BeginPlay PC166/opcode97, unsupported
native266. This is an unchanged integration boundary, not a successful startup.

Ignored evidence logs:

- `artifacts/training-level-move-unit-stateful-20261019.log`
- `artifacts/training-level-move-composed-unit-20261019.log`
- `artifacts/training-level-move-pose-unit-20261019.log`
- `artifacts/training-level-move-focused-ctest-20261019.log`
- `artifacts/training-level-move-host-20261019.log`
- `artifacts/training-level-move-arm-runtime-20261019.log`
- `artifacts/training-level-move-arm-test-20261019.log`
- `artifacts/training-level-move-arm-composed-20261019.log`
- `artifacts/training-level-move-arm-pose-test-20261019.log`
- `artifacts/training-level-move-android-20261019.log`
- `artifacts/training-level-move-ctest-20261019.log`
- `artifacts/training-level-move-owned-20261019.log`
