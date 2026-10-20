# First Training: original downward support search

Verified source checkpoint,2026-10-20. Only `Maps/00_Training.dx` is involved.
Original startup still refuses the next missing conversation-binding native;
there is no completed-startup, playable-physics or new APK claim.

`portable_actor_find_base.inl` connects the audited original FindBase and
SetSupportBase bodies to the existing actual-Level SingleLine service and
the same caller VM root. Exact qualified Location uses current native pose
authority; exact CollisionRadius/Height supply cylinder extents. The downward
end is a stored binary32 result of original PC64 Z minus8 arithmetic. The
query uses flags0x17/extra0 and the original zero packet with Time1/Item-1.

The decision consumes Hit.Actor, not the query Bool or a floor-normal filter.
It rereads live Base after the query and suppresses unchanged Base or a hit
already based on self. The SingleLine mark/arena payload is restored before
IsBasedOn or SupportActor; legitimate enclosing movement marks remain alive.
Expired Next is never traversed and undefined world extent Item is not read.

Nonnull support resolves/authenticates the original Actor.SupportActor and
the actual selected class/state override before the original ProcessEvent
suppression. Dispatch remains synchronous through the same execution's event
path, preserving begun-play/deletion gates; bare CallFunction would bypass
them and is deliberately not used. None support directly calls SetBase(None,
true). Overrides may legitimately leave a different Base.

WorldBases alone clears resulting Base equal to the actual slot0 LevelInfo
with SetBase(None,false). Actor birth has no AttachTag scan and its native
support search now precedes PostPostBeginPlay. The prior overlap/pointer-sort
approximation is removed rather than retained as a second physics authority.

## Retained-workspace correction

The first repeated real startup reached the actual support query but refused
its composed memory admission at LightSwitch0. Both attempts restored full
state and all40 owned sources. The reported extra1,028,720 bytes was the
conservative point-ray workspace for9524 world BSP nodes, even though this
query selects only the smaller extent branch. It was not NativeHitMarks storage
and did not justify increasing the retained-state limit.

The first correction chooses the same already-selected point or extent branch
as the kernel. Validation and traversal do not coexist, and mutually exclusive
point storage is not allocated by an extent query. The existing extent vectors
still required466,804 bytes rather than1,028,720. Strict scratch tests passed
30/7 and ARM64 syntax passed, but the two repeated actual startups STILL
refused at the same LightSwitch0 memory admission. Both fully rolled back with
all40 sources unchanged. Native work151795/262144 was not exhausted. Those
29.6238/31.9751-second host observations are not Quest frame-time measurements.

The next correction actually reduces extent allocations: complete graph
validation uses one packed uint32 DFS frame and one color byte per node;
traversal uses a fixed Visit array rather than growable vectors. After full
acyclic validation, a binary DFS has at most one deferred sibling per current
ancestor plus its current leaf. This bounds the live frontier to N+1, even for
shared DAG children that are still revisited independently. The actual array
capacity is min(originalPendingLimit,N+1), so original tightened-limit refusals
remain. Validation still checks every component, including unreachable nodes,
and preserves the same per-node/per-child work steps and traversal order.

The peak reservation now includes2048 bytes of fixed hull/plane headroom plus
max(5*N,12*(N+1)) on the supported64-bit targets:116,348 bytes for Training's
9524 nodes. The point kernel is unchanged and retains its larger vector-growth
envelope, now with the same explicit fixed headroom. Limits, packets, numeric
math, callback policy and rollback are unchanged; no larger retained-state
limit or skipped structural validation is substituted.

Strict standalone compact-scratch tests pass33 checks/8 refusal controls. Strict NDK27
ARM64 syntax passes for runtime and new test sources. Compact DFS controls add
an8192-deep two-branch frontier with exact validation/query/pending limits,
deep reachable/unreachable cycles, and repeated shared-DAG visits beside the
existing independent scalar collision oracle. Host extent tests pass59987
checks/44 refusal controls, including96/96 successful actual Training sweeps
with96 hits against the independent scalar oracle. The actual root has9524
nodes/3183 distinct hulls and safe independent-oracle depth53. Original map
bytes/mtime remain unchanged. Android native module compilation/link passes.

Repeated actual startup still refuses the COMBINED default/state admission at
LightSwitch0 with extra116348, retained16458283, native work151795/262144,
529945 instructions/39425 writes. Both24.4298/24.6874-second host attempts
fully restore all state and40 source fingerprints. A smaller query alone is
not sufficient and is not described as a completed startup fix.

## Sequential provider lifetime correction

The collector always allocates its fixed64 output packets before world query.
Its separate growable actor arena is allocated only after that world provider
returns, after its Model workspace is destroyed. Previously the actor arena's
full3*actorCount*packetSize growth envelope was reserved across the inactive
world branch as well. The corrected scope reserves only the fixed collector
across both, and engages the FULL actor envelope immediately before actual
ActorLineCheck. It remains live through nested actor Model queries, arena
processing, mover classification, sort and late admission. Original actual
actor bounds, candidates, hit allocation/order and all limits are unchanged.
No active-link-count shortcut or extra result cap is added.

The optional arena scope is declared after the fixed collector scope and
before callbacks/results. A one-call guard prevents replacement of an engaged
scope. Unwinding therefore restores actor primitives, arena and collector in
LIFO order on normal completion and failure. Strict standalone scratch tests
now pass41 checks/9 refusals, including admitted sequential full peaks and
rejected simultaneous inactive peaks, nested actor primitives and late throws.
Final coherent host build, strict ARM64 checking and Android native module
compilation/link pass. Repeated actual startup now passes ALL1334 WorldBases
visits and clears startup. Both attempts then stop at the next real dependency:
PostPostBeginPlay slot87/Plant2, DeusExDecoration.PostPostBeginPlay PC6,
unsupported native2101 (DeusExDecoration.ConBindEvents). Original script static
inspection confirms that call; the pinned native registration supplies its
name. No successful empty binding substitute is added.

Both attempts report530167 instructions/39430 writes and153783/262144 native
work, not exhausted;86 PostPostBeginPlay visits precede the refusal. Full state/
save/revision/hash/RNG/client/lookup/Actor/GC rollback and all40 source
fingerprints pass. The29.1292/27.4027-second host observations are not Quest
frame timing. Exit2 is a safely rolled-back unsupported startup, NOT completed
world publication, automatic gravity or a playable-physics claim.

## Actual first-map diagnostic coverage

The new --find-base-only mode and full first-map suite call the same runtime
FindBase through a rollback-only diagnostic. Detached snapshots expose paths,
original start/end/extent, selected hit/SupportActor identity, decision flags,
clock bytes and before/after mark depths; no runtime pointer escapes.

The new cases use original unposed lower/upper crates, Doctor1 and LightSwitch0,
with an independent prior SingleLine selection. They distinguish actual
selected-but-suppressed cold SupportActor from an executed callback. Reversible
same-hit and hit-based-on-self relationships test early returns; a real upper
crate attachment followed by its clear downward sweep tests SetBase(None).
Exact N/N-1 shared work, partial writes, late failure and full save/hash/RNG/
lookup/client/GC rollback are checked. The first rebuilt actual focused run
passes168495 checks/25 explicit refusals, including9 successful dormant roots,
3 hit/1 clear witnesses and4 same-hit/hit-based-on-self guard roots. This pass
precedes the subsequent sequential-provider lifetime correction. The final
rebuilt full first-map regression passes2016309 checks/153 explicit refusals,
including the new FindBase controls. It uses1308 actual native-slot actors,
120 collidable/nondeleted actors,478 registration links and214 independently
predicted primitive calls. Full snapshot/save/revision/GC/RNG rollback and
source immutability pass. No other map is loaded, and no dormant callback is
described as gravity.

The first coherent shared CTest run found one obsolete synthetic Spawn
expectation for the removed overlap/pointer-sort support substitute. That
fixture has no actual object lookup/Model providers. The corrected test checks
the exact Unsupported lookup-authority refusal, genuine preceding writes,
immediate GC, save/actor/revision rollback and enclosing Destroy rollback when
its nested physical child cannot find support. It retains unrelated nonphysical
Spawn/lifecycle, collision flags/sizes, AI, region, AttachDecal, late failure,
frozen-default and live-source save/GC checks. No runtime authority is weakened.

The rebuilt generated Spawn test passes9353 checks/164 refusal controls, and
strict ARM64 test syntax passes. Final coherent CTest passes156 executed/
180 listed,24 explicit owned-data skips, zero failures,39.23s. Skipped cases
are not actual-map or headset evidence. The full actual first-Training
collision regression above is a separate completed check, not one of these skips.

Evidence: `artifacts/training-find-base-staged-full-training-20261020.log`,
`training-find-base-staged-startup-20261020.log`,
`training-find-base-final-ctest-20261020.log`, the staged host/ARM/Android build
logs, compact extent actual-map oracle log, and generated Spawn boundary logs.
This remains a source/native-library checkpoint, not a replacement APK.

## Previous verified checkpoint

SetBase checkpoint `5412b18` passes full actual first-Training regression
1896941 checks/143 explicit refusals, focused409929/31, generated lifecycle
1954/25, timer1469/47, and156 executed/180 listed shared CTests with24 explicit
owned-data skips and zero failures. Those results do not automatically verify
this subsequent FindBase integration. See TRAINING-ACTOR-BASE.md.
