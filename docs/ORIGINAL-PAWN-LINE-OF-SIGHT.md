# Original Pawn visibility reached by Training melee

Scope: the internal C++ `Pawn.LineOfSightTo(targetPawn,0,false)` called by
original PickTarget531. Target is an actual Pawn selected from native PawnList;
source bIsPlayer remains a live field, not a PlayerPawn-class assumption.
This does not bind every script514/CanSee/non-Pawn/ignore-distance variant.
Those different calls are not silently accepted by this specialization.

## Static and schema evidence,2026-10-25

Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Export[1273] RVA24E1 jumps to103BEB50..103BF522. Only original package
inspection and static disassembly were used; commercial DLL/EXE code was never
loaded or executed. Source Engine.u declares bIsPlayer Bool20, Visibility
Byte1, Enemy Pawn Object0, BaseEyeHeight Float1, bLOSflag Bool2,
CollisionHeight/Radius Float23, LastSeeingPos/LastSeenPos Vector0. Native
offsets identify these declarations, not portable memory offsets.

Actual constants read as data from the original PE sections:

| Address | Value / exact storage |
| --- | --- |
|104298F0|Float -1, bitsBF800000|
|10431464|Float0.015, bits3C75C28F|
|1042F548 /1043145C|Float5000 /4000|
|10431460 /10431478|Float25,000,000 /16,000,000|
|10430A64 /10431454|Float1,000,000 /250,000|
|10431418|Double0.8, bits3FE999999999999A|
|1042AC10|Double0.5, bits3FE0000000000000|

## Reached ordering and arithmetic

1. Read Enemy at entry even though fixed checkSight0 makes the initial
   comparison redundant. Do not toggle bLOSflag. Compute unspilled Location
   differences and `(Xsquare+Ysquare)+Zsquare`, then a mandatory Float distance²
   store at103BEC02. No sqrt is used.
2. Read live source bIsPlayer then target Visibility. Range stays PC64:
   `min(cap,(scale*min(1,(Visibility+16)*Float0.015))²)`, using5000/25M for
   player and4000/16M otherwise. Float distance²<=retained range passes.
   No SightRadius, PeripheralVision or Stimulus field is reached here.
3. After range acceptance, copy current source Location, reread Enemy, then
   store Eye.Z=Float(source.Z+BaseEyeHeight). Do not read eyeheight early.
4. Enemy target: test centre→Eye, then centre→current source Location. Either
   clear ray writes current source Location to LastSeeingPos FIRST, then the
   CURRENT Enemy.Location to LastSeenPos, and returns true. Both blocked return
   false if distance²>1M; otherwise enter height fallback. No RNG here.
5. Non-enemy distance²>1M: read live bLOSflag/bIsPlayer. If !bLOSflag and
   distance²>retained range*Double0.5, return false. For nonplayers only, draw
   from the original shared stream and reject retained appFrand<0.5. Then one
   centre→Eye ray returns its Boolean directly, with no blocked-ray fallback.
6. Height fallback: copy current target Location; read CollisionHeight; store
   End.Z=Float(target.Z+PC64(height*Double0.8)); query End→Eye. Clear returns true
   WITHOUT enemy-history writes. Blocked can try sides only at distance²<=250k.
7. Read CollisionRadius only now. Construct Float-stored world-space corners
   `(-R,+R),(+R,+R),(-R,-R),(+R,-R)` around current target centre Z. Pruning uses
   ABSOLUTE origin-relative squared sizes, NOT observer-relative distances.
   Corner0 initializes BOTH min/max Float thresholds and indices0. Each later
   retained PC64 square first compares strict>stored Float max; only otherwise
   tests strict<stored Float min. Each accepted threshold stores Float. Ties and
   stored-rounding comparisons must not be replaced with a generic minmax.
8. Reread bLOSflag even though checkSight0 prevents the alternating-side skip.
   Query unpruned indices ascending. Min/max indices can coincide, so up to
   THREE side rays are real (radius0 is a simple case). Maximum full enemy
   route: two centre rays, one height ray and three sides. Side success also
   returns without enemy-history writes.

All directed Model calls have End=target point, Start=Eye except the second
enemy-centre ray. Original FastLine nonzero means clear. Native BSP queries
do not dispatch script callbacks. The portable reflected journal writes the
two complete history Vector fields in native order; no LastSeenTime is written.

Original appFrand returns an unspilled draw15*Float0x38000100 (`32769*2^-30`).
103BF111 FSTs a separate Float, then compares the still-retained quotient with
Double0.5. Do not substitute the rounded script FRand195 result or create a
separate/randomly seeded generator. Draw only on the reached nonplayer far path.

## Binding and verification

The runtime adapter reuses actual current native Level.Model, shared directed
FastLine budgets and the existing root RNG snapshot. Raw history references
write once through the same Execution/journal, while already guarded531 OUTs
retain their original immediate aim→distance stores. No new renderer target,
Actor hash, public nested execution or partially committed startup is introduced.

Finite active-data/mandatory overflow checks, lexical scratch admission and
shared native-work/query/write limits are portable safety guards, not inferred
retail malformed-data behavior. Negative finite dimensions remain algebraic
inputs; no normalization, guessed axis or collision-height/radius clamp is added.

Verification below separates detached math/protocol/VM controls from the
actual first-Training attack diagnostic. No APK, live crate destruction,
surviving loot, gravity, completed startup or Quest acceptance is implied here.

Detached numeric controls currently pass326589 checks/32825 portable checks,
170820 independent test-owned hardware x87 cases and15133 explicit refusals.
The oracle uses only our local instructions, never the commercial DLL. This
is math evidence, not BSP visibility, runtime enemy writes or target-hit proof.

Pure ordered-provider protocol controls pass70 calls/264 checks/22 refusals,
including inclusive range/head/side boundaries, lazy field failures, fresh
query endpoints and Enemy links, both far RNG gates, strict corner ordering,
and the six-ray degenerate-extrema maximum. Explicit mock-host protocol controls
inside the REAL VM transaction machinery pass193 checks/31 refusals/44 roots:
single borrowed Execution, ordered two-Vector writes, bit-exact partial undo,
optional CRT persistence/undo, caught-failure poisoning and every six-ray work
cutoff. Mock-provider successes are NOT real BSP visibility or actual enemy
state evidence. Final desktop focused builds, Android native link and strict
ARM64 runtime/final tests pass. Two test-only warning-as-error issues were
fixed without changing the compiled runtime.

## Actual Training attack result

Both actual authored weapons now pass the reached internal LOS, return the
original HandToHandAttack notification and complete one original NormalFire
cycle in the genuine same-root pickup/equip/player-Fire diagnostic. Each has
22 detached phase rows, one returned notify and one completed cycle; final
owned weapon returns to original Idle, weaponUp/readyToFire true. Crowbar's
returned notify row13 has14970..14997 writes and final row21 has15007..15014;
knife row12 has14965..14992 and final row21 has15000..15007. This is completed
original attack-control evidence, NOT a controller-aimed hit or crate damage.

Initial pre-refund runs subsequently refuse at HUDActiveAugsBorder.CreateIcons46,
216816 instructions/22174 writes (crowbar),216741/22167 (knife), because the
existing32MiB combined budget is exceeded while the Fire diagnostic still
retains its conservative4MiB observation reservation. Full-root save/state/
revision/hash/RNG/client/lookup/Actor/GC rollback and all41 owned sources pass.
The limit is not raised and no failure becomes partial playable startup.
This diagnostic-memory issue is separate from LOS fidelity; the subsequent
success-only observation accounting fix below addresses its conservative charge.
Logs: training-pawn-los-{crowbar,knife}-discovery-20261025.log.

## Successful diagnostic-output accounting

The Fire helper admits its entire4MiB observation bound before reserving or
copying. Its cumulative Budget charges the complete reserved row capacity
(not only22 used rows), every player inventory capacity, text capacity, byte
grid/clock capacity and authored-notify capacity. Only after the final ready
Idle/ownership/returned-notify checks and all snapshots finish does it subtract
`outputBound-retained.retained` from nativeActorOrderObservationBytes_. Both
`retained<=outputBound` and shared charge>=outputBound are required first.
Nothing allocates or calls a provider after this refund.

This preserves callback-added observations instead of resetting the shared
counter, keeps the separate equip admission and scratch/query/undo/RNG charges,
and does not raise the32MiB combined limit. Failure and exhausted-frame paths
retain their conservative admission until full rollback. It is not a generic
refund on scope exit or an excuse to publish a partially initialized player.

Final accounting host probe, strict ARM64 runtime check and Android native
library link pass. Actual weapon repeat2 now reaches ordinary
DeusExHUD.ConfigurationChanged606/opcode101/native1463 instead of the earlier
diagnostic-memory limit:

| Weapon | Instructions per attempt | Writes | Completed attack |
| --- | ---: | ---: | --- |
| Crowbar1 |222711|24542|22 rows,1 returned notify,1 cycle|
| CombatKnife0 |222636|24535|22 rows,1 returned notify,1 cycle|

Both attempts for each weapon match; all41 owned sources remain unchanged and
full save/state/revision/hash/RNG/client/lookup/Actor/GC undo passes. No-command
startup repeat2 remains exact1463/218693 instructions/24284 writes. Final
crowbar write-cut repeat2 at14983 stops at TraceFire212/opcode15 with206100
instructions,14 rows/no returned notify/no cycle, and the same complete undo.
Logs: training-pawn-los-accounting-{crowbar-repeat,knife-repeat,attack-cut,
default-repeat}-20261025.log. Expected-boundary CLI success means the exact
refusal/undo was verified, NOT successful startup or playable weapon damage.

Final coherent full desktop build/shared CTest:189 executed/225 registered,
36 explicit owned-data skips,0 failures in70.54s. The first shared-build attempt
hit a Windows linker file lock because the old crate-test executable was still
running; after that process exited, the unchanged-source rebuild and complete
suite passed. Logs: training-pawn-los-accounting-full-build-retry-20261025.log
and training-pawn-los-accounting-ctest-20261025.log. This is separate from the
earlier pre-refund81.91s gate and does not count skipped owned tests as passed.

## Fresh first-map mechanics regressions

Initialized original damage/physics-cohort repeat2 retains64 captured rows,
11 eligible/53 skips,48 sweeps,13 hits,2 landings,9 rotation moves and3 actual
child carries. Lower-crate original Active.TakeDamage goes10->4->-2, creates
six fragments and one surviving Lockpick. Upper-crate Base becomes the real
Lockpick, whose Base is LevelInfo, not an invented direct-world support.
Both attempts next refuse normal HUD1463 with223490 instructions/25937 writes
and completely undo/all41 sources unchanged. Log:
training-pawn-los-accounting-cohort-repeat-20261025.log.

This existing explicit physics suffix is not automatic Level Tick, state/
timer/lifespan scheduling or a live weapon hit. The separate dormant
Game=None destruction control correctly deletes contents through original
relevance logic and cannot be used as surviving-loot evidence.

Actual loot falling/player-collection repeat2 also passes:9 captured falling
rows/16 moves/two settled Lockpicks, followed by genuine ParseRightClick
transfer into JC's6x5 grid at(0,0), then merge NumCopies1->2 with original
removal of the second item. Both attempts next reach HUD1463 with224509
instructions/25082 writes, full undo and all41 sources unchanged. Log:
training-pawn-los-accounting-loot-repeat-20261025.log. Supplied member targets
are explicit diagnostic input, not proof of controller reach/occlusion or saved/
visible committed loot. No Game=None relevance flags or FrobTarget cleanup is
fabricated to make these outcomes pass.

After the final shared relink, separate actual-data controls pass again:
dormant crate destruction347 checks/two genuine Game=None contents controls;
PickTarget181 calls/277720 checks/90 refusals/four work cuts; Trace43 calls/
37063 checks/23 refusals/nine work cuts. Existing whole-fixture/save/RNG/GC/
lookup/source preservation checks pass. Logs:
training-pawn-los-accounting-final-{portable_crate_destruction_test,
portable_pawn_pick_target_test,portable_actor_trace_test}-20261025.log.
