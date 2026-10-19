# Shared native-work allowance for first Training

2026-10-19. Portable safety policy, not an original Engine.dll counter. This
work targets only `Maps/00_Training.dx`; other-map integration stays deferred.
It does not establish live gravity, destruction, usable loot or begun gameplay.

## Why this is needed

The original mover PostBeginPlay path performs genuine repeated actor scans.
After the original movement and Core Rotator addition implementations, the
first Training startup reaches `Engine.Mover.FindTriggerActor` PC198 in
`00_Training.DeusExMover49`, where the previous aggregate65536 allowance is
exhausted. AllActors has a monotonic cursor and charges one visited numeric
slot, including holes. Different scans authored by the original scripts are
not duplicate accounting that can be removed. Visited live-actor counts are
not necessarily the native slot-array length.

## Policy and ownership

`Vm::Limits.nativeWork` retains default65536. Larger finite roots explicitly
opt in; the portable hard ceiling is16777216. Zero permits roots with no native
work. This ceiling is a safety policy, not a claim that Training needs that
much work, nor an increase of instructions, memory, nodes or structural limits.

The owning `NativeWork::Budget` precharges using subtraction-safe arithmetic,
never overflowing or incrementing beyond its allowance. A refused bulk charge
does not partially consume that charge, so `used` can be below `limit` on
exhaustion. Failure is sticky: later charge/check attempts refuse, including
zero-cost attempts. An invalid reset preserves the preceding ledger. Only the
outer owner resets; callback/iterator/AI views have no reset authority.

All four VM root entries validate the configured ceiling before Host.BeginRoot.
The new limits-aware hook defaults to the existing legacy Begin method. The
actual runtime checks its nested-transaction guard BEFORE resetting the ledger,
then owns one budget across script calls, iterators, geometry, lifecycle and AI
mutation. State poll and resume inside one AdvanceState also share that root.
Separate later roots start fresh; no inner phase/callback/Next resets occur.

Runtime AI helpers borrow this same budget, avoiding their former conflicting
local65536 comparison against the already-spent aggregate counter. Legacy
scalar-counter/codec calls retain their existing default ceiling and error.
State node/string/storage limits and independent structural validation remain
unchanged. The fixed collision-table initialization still has its own49152
draw bound and exact RNG accounting; it is not charged as actor traversal.

Typed exhaustion reports VM Budget, not a guessed message-based classification
or a false/no-hit result. Ordinary nested execution failures still poison the
whole interpreter and preserve the first observed function/PC/opcode/stack.
The runtime also checks sticky exhaustion before Commit publication. Complete
property/native/alias/birth/clock/hash/RNG rollback belongs to the outer owner;
spent ephemeral work is not refunded by an inner rollback or caught exception.

`Vm::Result.nativeWork` is optional final diagnostic accounting: used, limit,
exhausted. It is sampled without allocation only after Commit/Rollback, and only
if BeginRoot successfully returned. Legacy hosts may report no ledger. This
is not a new save format or persistent process history. The startup CLI prints
FINAL NATIVE WORK separately from its deliberately EARLY hash-prefix image.

## Focused checks and real startup observation

The generated ledger/VM and AI suites cover exact limits, independent expected
costs, larger shared allowances, overflow guards, legacy calls, nested calls and
iterator/poll sharing, caught exhaustion and complete caller-owned rollback.
Actual first-map controls use the original native Actor-array length N rather
than a guessed live count: original FindTriggerActor must refuse at N-1 and
complete at N. A separate1+1+2 predicate batch distinguishes allowance3 from4
without a new hash or forced startup. A real positive original-function commit
and its save restore intentionally publish two revisions; neither is hidden.

Useful diagnostic commands after building the desktop tools:

```powershell
artifacts/prerequisites-20261009/build/portable_retail_collision_test.exe --owned-source-root 'D:\Steam\steamapps\common\Deus Ex' --native-work-only
artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe --owned-source-root 'D:\Steam\steamapps\common\Deus Ex' --native-work 262144 --repeat 2
```

The focused mode does not replace the complete owned first-map regression.
The startup observation's exit2 means a verified rollback at an unfinished
boundary, never successful startup.

## Current verified evidence

The generated work suite passes46682 checks,2605 independent ledger scenarios,
52 VM roots and14027 explicit refusals. The AI suite passes622 checks/305
refusals, including12 literal-cost cross65536 cohorts,69 internal precharge/
caller-snapshot/sticky cuts and45 legacy-default/overflow controls. The general
VM suite remains695763/846. Strict NDK27 ARM64 VM/runtime/new-unit/AI/owned-test/
observation syntax checks and Android native compilation/link pass.

The focused actual38-package first-Training test passes62308 checks/18 refusals.
The genuine source Actor array has1337 slots,1308 occupied, not1308 or1333
total slots. N-1 refuses with complete state/save/revision/hash/RNG/lookup/
client/GC rollback; N completes1312 original script writes with the independently
predicted trigger fields, then exact save restoration and two explicit
publication revisions. The1+1+2 predicate3/4 controls and zero-work roots pass.
The separate complete actual first-map regression passes1177670 checks/100
expected refusals, including movement/clock/hash/query/save and source checks.
The coherent full host build passes, and CTest passes153 executed/177
registered with24 explicit owned-data skips and zero failures in49.78s. Skipped
tests are not actual-map/device evidence; the separate owned run above is the
first-map evidence. No APK is packaged for this prerequisite-only checkpoint.

With explicit262144 work, two real startup observations advance past the former
mover scan to PostBeginPlay slot630, `00_Training.DeusExLevelInfo0`,
`Engine.Actor.SetInitialState` PC25/opcode113. The original stack is
DeusExLevelInfo.PostBeginPlay -> DeusExLevelInfo.SpawnScript -> SetInitialState.
The reported actor/slot is the outer coordinator's actor: SetInitialState is
executing for the genuinely spawned Mission00 script, not for LevelInfo.
Both execute386237 instructions/23833 writes and report107734 final native work
units, limit262144, not exhausted. The next refusal is independent persistent
memory accounting:

```text
Persistent combined default/state byte budget exceeded [native-state retained=16650628, extra=0, retail=489536, output=165013, lookup=12643992]
```

Each observation restores full save/state/revision/hash/RNG/client/viewport/
lookup/Actor/GC and verifies all40 original sources unchanged. The early hash
image still reports2649 work units; it is NOT this final107734 count. Memory
limits are not raised here. Diagnose the retained storage and genuine SpawnScript
state path next, without bypassing original callbacks or inventing begun play.
This is unfinished startup, not automatic gravity or live crate/loot acceptance.

Read-only follow-up audit distinguishes the gate from its trigger. SpawnScript
finds no existing MissionScript and performs the authored Mission00 birth.
That birth has ordinary Actor overlays, InitExecution and lookup/slot ownership;
its frozen defaults copy only mutable class-default overlays, not the whole
immutable CDO. Mission00/MissionScript has no Auto State/locals explosion at
this call: GotoState(Auto) falls back to class/None. The unconditional post-call
budget check reveals accumulated storage, not an unaudited FirstFrame/Timer
execution that can be removed.

The logged16650628 is `defaults.retained`, excluding the separate measured
StateObject/AI-codec contribution. Lookup ownership is charged once, and AI
accounting takes the larger of codec/actual storage rather than adding both.
Do not remove legitimate output/state charges or reduce conservative per-node
allowances merely to pass. A concrete next candidate is physically smaller
lookup chain/head storage using bounded32-bit object indices; current64-bit
lookup storage has two pointer links per node. It must preserve weak identities,
stable records, exact hash/ascending ordering, atomic adoption and allocation-free
cleanup. Its predicted saving is not an implemented or measured improvement in
this checkpoint. Typed Region/frame storage is a further representation audit,
not permission to omit original zone or InitExecution passes.

Ignored evidence beneath `artifacts/`: `training-native-work-unit-20261019.log`,
`training-native-work-ai-20261019.log`,
`training-native-work-vm-regression-20261019.log`,
`training-native-work-owned-focused-20261019.log`,
`training-native-work-owned-all-20261019.log`,
`training-native-work-startup-262144-20261019.log`,
`training-native-work-cli-invalid-20261019.log`,
`training-native-work-host-all-20261019.log`,
`training-native-work-ctest-20261019.log`,
`training-native-work-arm-*-20261019.log` and
`training-native-work-android-20261019.log`.
