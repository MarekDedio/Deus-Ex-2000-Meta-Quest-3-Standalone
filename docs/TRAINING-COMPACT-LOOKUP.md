# Compact weak lookup storage for first Training

2026-10-19. Implemented; verification below. First Training is the only
real-map integration target. No original commercial EXE/DLL is loaded/executed.

## Actual need and representation change

With explicit shared native work, original Training startup reaches the
Mission00 script birth but refuses the unchanged combined persistent-storage
budget. The lookup accounts for12643992 bytes at that frontier; the old64-bit
layout is65592 bytes of Index plus120 bytes per104820 live records. That
diagnostic is Index-only; the public snapshot also charges its runtime
control/ownership allowance. That is
real weak-index storage, not a second set of UObjects or a duplicate charge.

The change retains one owning ordered map and stable public Record addresses,
but stores name/identity chain links and both4096 bucket-head arrays as uint32
object indices, using0 as null. These indices already have an enforced4million
maximum. UObject/Outer identities remain full uintptr_t; neither pointer bits
nor object lifetimes are narrowed or inferred from a stale index alone.

Each chain step resolves the current map node by its numeric index; this adds
an allocation-free O(log N) tree lookup instead of a direct pointer hop. Exact
name/Outer selection must retain publication/rehash bucket order, while Any
selection retains ascending actual index order. Neither order may be replaced
with the other. Copy retains the actual chain indices in its independent map;
move/swap transfer all owned links. Adopt preflights the full input, transfers
preallocated unique nodes, then resolves transferred tail links through the
live map before joining chains. Failed admission must not modify either side.

Remove/Rehash use stored publication keys, not dereferences of weak UObject or
Outer pointers. Record addresses must survive insertion/rehash/adoption, and
stale cleanup must not remove a reused index with another identity. Cleanup,
rehash and adoption remain allocation-free. No map capacity, memory ceiling,
conservative tree-node overhead or original callback is relaxed.

On64-bit hosts the independent layout model predicts8 bytes less per node plus
32768 fewer head-array bytes:871328 bytes for104820 nodes. This is a physical
representation saving, not just a smaller accounting constant. Actual data
counts, retained bytes, successful startup advance and timing must be measured
after builds; this prediction is not acceptance evidence by itself.

## Verification scope

The original lookup tests remain and use the new independent node-layout
model. Additional generated tests use a separate vector-order oracle, sparse/
maximum/reused numeric indices, full-width identities, colliding keys, copy/
move/swap, staged adoption, pointer stability, mixed mutation sequences and
allocation-failure/nonallocation controls. They are not actual-package proof.

The actual first-Training startup tool reports its real initial lookup census/
retained bytes and checks exact restoration of bytes/count/generation after
every rolled-back attempt. Its final work ledger remains separate from the
early hash image. The full first-map regression checks the same retained
storage invariant in addition to complete save/revision/clock/hash/lookup/GC/
RNG/source immutability. A separate genuine Core/Engine lookup integration
suite includes synthetic map lifetime fixtures, not extra campaign integration.

New startup boundaries must come from repeated actual observations, never a
changed success flag or skipped callback. Gravity/support, live held-weapon
crate destruction, surviving usable contents and complete Training still need
end-to-end device acceptance. No replacement APK is justified by lookup-only
tests.

## Current results and next original boundary

The original generated lookup suite passes203655 checks/59 refusals. The new
independent compact suite passes686274 checks,1536 mixed operations,36
allocation-free controls,17 allocation-failure points and36 expected refusals.
Strict NDK27 ARM64 runtime/new-unit syntax and Android native compilation/link
pass. The coherent full host build passes; CTest passes154 executed/178
registered,24 explicit owned-data skips and zero failures in61.51s. Skips are
not actual-map or device evidence. The full actual first-map regression passes
1177670 checks/100 explicit refusals, including exact retained-byte rollback;
it remains first-Training-only, not live crate/gameplay or device acceptance.
The separate genuine Core/Engine lookup integration passes440 checks/32
refusals, including synthetic map lifecycle and late native client ownership.
The existing first-Training `portable_based_actors_test --lookup` passes1560
checks/four refusals: both real crates' isolated original fatal hits create six
WoodFragments, whose actual lookup records survive warm/cold save replacement,
new indices, GC and late-failure rollback. These dormant-world controls do not
establish begun-world Contents callbacks, usable loot or live VR weapon damage.

The actual38-package first-Training observation reports104794 initially indexed
objects and11769824 retained lookup bytes, including the public runtime control
allowance. For this exact census, the independent old-layout counterfactual
is12640944 bytes:871120 bytes more (8 per node plus32768 heads). The old
104820-object *later frontier* prediction of871328 bytes concerns a different
in-root census; do not compare those two snapshots as the same world state.
These are retained representation measurements/models, not OS heap/RSS savings
or a headset frame-time benchmark.

Two actual startup attempts with explicit262144 native work now pass the prior
Mission00 birth/memory refusal, execute its original SpawnScript completion,
and finish all1334 live PostBeginPlay visits. They next refuse at SetInitialState
slot168, `00_Training.NanoKey0`, `Engine.Inventory.BecomePickup` PC22/opcode15:

```text
Unsupported original active-hash collision-shape property mutation contract
```

Both observe513768 instructions/39004 writes, final native work147679 (not
exhausted),167 SetInitialState visits, and the exact original call stack
Actor.SetInitialState -> Pickup.Pickup.BeginState -> Inventory.Pickup.BeginState
-> Inventory.BecomePickup. The two observations take13.723s/13.2691s under
concurrent desktop compilation; these are diagnostic durations, not device or
controlled performance measurements. Full save/state/revision/hash/RNG/client/
viewport/lookup/Actor/GC rollback and all40 owned source files' immutability pass.
The CLI exits2, explicitly unfinished startup. No memory/work ceiling or callback
was skipped to reach this boundary. The next task is the original raw
collision-shape property-store contract, not forcing key pickup or begun play.

Ignored logs beneath `artifacts/`: `training-compact-lookup-original-unit-20261019.log`,
`training-compact-lookup-new-unit-20261019.log`,
`training-compact-lookup-arm-runtime-20261019.log`,
`training-compact-lookup-arm-unit-20261019.log`,
`training-compact-lookup-android-20261019.log` and
`training-compact-lookup-startup-20261019.log`.
The actual lifetime/fragment logs are
`training-compact-lookup-owned-objects-20261019.log` and
`training-compact-lookup-fragment-births-20261019.log` in the same directory.
The complete actual first-map regression log is
`training-compact-lookup-owned-all-20261019.log`.
Coherent build/shared regression logs are
`training-compact-lookup-host-all-20261019.log` and
`training-compact-lookup-ctest-20261019.log`.
