# First Training: movement hit-list lifetime and query memory (2026-10-19)

First `00_Training.dx` remains the only real-map target. This checkpoint
supplies temporary hit-storage authority required to connect complete MoveActor
to the actual runtime. It also corrects simultaneous scratch-memory admission
in already connected Model and MultiLine providers. It is not a new native266
route, world-startup bypass, crate/gravity implementation or playable APK.

## Why movement needs retained lists

The original complete MoveActor and higher CheckEncroachment contracts retain
their original lists until a native memory mark is released. Child movement,
push/recheck/reverse movement, selected callbacks and subsequent touch scans
can run in between. A stack-local query vector or one mutable 'last query'
buffer cannot supply that lifetime. Separate `OriginalLineHitArena` instances
all use local one-based record handles, so their raw Next/head words cannot be
placed into one unqualified namespace.

The original source audits are ORIGINAL-LEVEL-MOVE-ACTOR.md and
ORIGINAL-LEVEL-ENCROACHMENT.md. The new store is a portable host-owned safety
mechanism, not a claim about original unchecked allocation or EH behavior.
No original commercial executable or DLL is loaded/executed.

## Host-owned temporary authority

`native/quest_native_hit_marks.h` implements bounded, noncopyable
`NativeHitMarks`:

- A root stamp binds the supplied native Level, map generation, lookup-control
  identity and lookup generation. The eventual real adapter must authenticate
  those identities against the actual CPU-owned runtime on every use. Nonzero
  words accepted by this helper are not proof of real UObject authority.
- A process-lifetime monotonic root serial is outside gameplay journals.
  Handles encode that root plus an issued record ordinal; mark and record
  words use disjoint odd/even namespaces. No integer becomes a hit pointer.
- Fixed metadata bounds permit64 nested marks and256 retained owning arenas.
  Caller limits may tighten, never enlarge, their storage/token allowances.
  Cumulative per-root record and mark allowances never replenish on Pop.
- Adoption validates every local head/Next, record/count/capacity allowance
  and caller peak-memory preflight **before** altering links or storage. It
  rebases all Next/head words and transfers the vector allocation by swap,
  without copying/reallocating its records. Cycles, original order and
  unreachable records are preserved, not repaired or silently deduplicated.
- Packet/Actor/Next/Time lookups authenticate the live root/range and return
  values, never retained references or pointers. A copied full hit includes
  its rebased global Next, not a stale local index.
- Mark release is strict LIFO. It invalidates suffix ranges before destroying
  their vectors, restores retained-byte accounting and never rewinds issued
  ordinals. Popped/foreign/expired/wrong-kind handles refuse and cannot alias
  later same-slot storage or a new root.
- Pop/Clear are noexcept and allocation-free, with no work hook, script,
  lookup or admission callback. The adapter must treat false cleanup as an
  invariant failure, not successful release. Clear is stronger portable
  root cleanup, not gameplay rollback by itself.
- The admission callback must be pure/non-reentrant. Attempted store mutation
  invalidates adoption even if the callback catches the attempted mutation's
  error. Failed admission leaves both the incoming local links and the current
  retained storage unchanged.

`RetainedBytes()` includes fixed metadata and the sum of live vector capacities,
not just record counts. That charge must be included in the actual host's
shared budget, alongside simultaneous producer/collector/Model workspaces.
No arena or opaque handle belongs in gameplay save data.

## Connected runtime query-memory correction

`portable_retail_multi_line.inl` now retains its collector plus actor-arena
growth envelope in `NativeScratchScope` throughout provider calls. World Model
workspace has a nested scope rather than an independent manual preflight.
The returned64-packet collector is already inside that envelope and is not
charged twice while the reservation remains active.

`NativeActorPrimitiveLineCheck` now reserves the selected Model's validation/
point-or-extent traversal peak through ToWorld and nested admissions.
`NativeActorPrimitivePointCheck` likewise retains its existing occupancy peak
through the whole Model call rather than only preflighting it once. These
reservations use the actual caller ledger and existing state limit; no limit
is raised, no allowance resets during recursion and no query is replaced by
a renderer approximation. Scope release restores only ephemeral accounting.

The query changes apply to actual first-map providers. The new hit store itself
is currently included for target compilation only; it has **not** yet been
bound into the host's movement root or used by native266/299.

## Verification and limits

`desktop/quest_native_hit_marks_test.cpp` covers exact raw packet words,
nontrivial heads/Next/cycles, independent earlier/later arena lifetimes,
wrong-kind/foreign/stale/reused/root handles, strict nested cleanup,
capacity-versus-size accounting, invalid-link and late-admission atomicity,
caught reentrant preflight mutation, fixed depth/cumulative allowances and
allocation-forbidden adoption/access/cleanup. A1000-sibling independent ordinal
model checks that popped handles never become valid while the original parent
list survives.

The test also binds the **real** retained store to the **real** complete
MoveActor coordinator. Mock parent/child queries both start with local1..3;
the parent query survives the child Pop, global Next remains valid in each
live caller packet, child handles retire, and parent/child Bump/touch recipients
and backed-off publication stay distinct. Every shared-work prefix is denied
in turn and all acquired marks/arenas unwind. Lower collision, pose storage,
LevelInfo and events in that composition remain explicit mocks—not an actual
VM/hash transaction or physics proof.

The expanded unit passes5185 checks/1277 explicit refusals. This is a lifetime/
provider-composition result, not thousands of real crate interactions.

## Next actual bridge requirements

The host must lazily create one store in the current Execution, authenticate
its root stamp, charge its retained capacities, and keep query+adoption+late
validation inside the same hash root so refusal poisons consumed tags. Pure
world queries must not create a hash or consume CRT draws merely to own a list.
Commit must refuse live marks/arenas. Rollback must clear ephemeral hit storage
before retiring provisional lookup/birth identities, without undoing ordinary
false/veto effects as if they were errors.

Complete native-qualified pose writes, higher Location-only reverse scopes,
recursive Move/Smooth, authentic event packets and child/zone providers still
must be connected together. Only then can real first-map movement/startup,
crate damage/contents and support/falling acceptance progress. No other map is
integrated or tested by this work.

## Build evidence

The coherent full host build, strict ARM64 runtime/expanded-test syntax and
Android native compilation/link pass. Default CTest passes151 executed/175
registered,24 explicit owned-data skips, zero failures in71.18s. The focused
coordinator/child/pose/hit-store/scratch selection separately passes all five
tests. Owned-data skips are not actual-map or headset evidence.

The separately rebuilt actual first-Training regression passes1090740 checks/77
explicit refusals with38 exact Quest source packages,1308 native-slot actors,
120 collidable/nondeleted actors,478 registration links and94 actual primitive
calls. Model/collector/encroachment/touching/zone controls and full save/revision/
lookup/hash/client/clock/GC/RNG rollback/source immutability pass with the new
connected scratch accounting. This does not run the new store through a real
movement bridge: original startup still reaches DeusExMover16 BeginPlay
PC166/opcode97, unsupported native266, and no playable physics is inferred.

Ignored evidence logs:

- `artifacts/training-hit-marks-unit-20261019.log`
- `artifacts/training-hit-marks-focused-ctest-20261019.log`
- `artifacts/training-hit-marks-host-20261019.log`
- `artifacts/training-hit-marks-arm-runtime-20261019.log`
- `artifacts/training-hit-marks-arm-test-final-20261019.log`
- `artifacts/training-hit-marks-android-20261019.log`
- `artifacts/training-hit-marks-ctest-20261019.log`
- `artifacts/training-hit-marks-owned-20261019.log`
