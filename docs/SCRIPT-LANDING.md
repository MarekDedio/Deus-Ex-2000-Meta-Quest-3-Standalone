# Original Pawn landing wait

This increment adds the original native527 WaitForLanding and its explicit
poll528 behavior to the portable state interpreter. It is a dependency for real
ScriptedPawn.StartUp, not a substitute for collision or falling physics. No
automatic world scheduler, player possession or campaign completion is implied.
This increment's final corrected signed APK is now installed, with an exact
on-device hash check. Training/body/UI captures from the earlier landing APK
are scoped regression evidence, not final-build or complete campaign acceptance;
see [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).

## Original binary contract

The authority is the read-only owned GOTY installation. Engine.dll SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`;
Core.dll SHA256 is
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
The original Engine.Pawn.WaitForLanding declaration has native527, flags0x409
and no arguments. Its exported thunk at10303e2c reaches body103bb6f0:

- It always writes binary32 2.5 to actor-owned native field+e0, shared with Sleep.
- Only Physics2 (Falling) sets the StateFrame latent action to528 (0x210).
- Nonfalling calls leave any existing latent action unchanged, including Sleep.
  They can retain the counter without a portable frame. Falling calls require
  an already established portable frame; guessing raw state continuations is
  still refused transactionally.

The poll thunk10303832 reaches103bb730. If Physics is no longer2, it clears
the action immediately without touching the counter. Otherwise it subtracts
the supplied elapsed float, stores the signed binary32 result with `fst`, and
compares the still-live x87 result to zero. Only a strictly negative remainder
calls LongFall. Exact zero does not call it; a negative counter can call it
again on a later zero-elapsed poll. There is no half-frame tolerance, clamp,
timer reset or automatic release on timeout. The callback's own code decides
what to change. Finite storage overflow and invalid elapsed values refuse in
the portable implementation rather than retaining an unrepresentable timer.

The timeout dynamically finds LongFall with global=0 and dispatches through
APawn vtable+0x10 to AActor.ProcessEvent10369100, then Core.ProcessEvent1013f7d0.
The original begun-play, recognized-probe and pending-kill gates therefore apply.
The original ScriptedPawn override calls SetFall and then selects
FallingState.LongFall; it is not replaced with an invented portable callback.
The pinned reference engine leaves LongFall as TODO, so it is not the oracle
for this timer/event path.

The companion Core audit resolves execEnable117 to1013de80 and execDisable118
to1013dfb0. Both affect only hardcoded FName indices300–363 and an existing
owned StateFrame. All64 original registrations match the portable probe table.
LongFall is not a probe. These natives now leave nonprobe names unchanged,
without manufacturing state storage; legacy disabled-name records for nonprobes
remain byte-preserved but cannot suppress calls. This also corrects the former
arbitrary-function-disable behavior for custom/startup names. Reconstructing
the complete original live FStateFrame ProbeMask and selected StateNode remains
separate work; this change does not claim those storage distinctions complete.

The companion original Serialize/IsProbing audit also exposed an inherited mask
polarity error: package ProbeMask bits are positive enabled bits, not disabled
names. Core Serialize10150210 writes8 bytes directly from frame+1c at10150331;
IsProbing10115670 tests that live positive bit and permits every probe when no
frame exists. Dormant authored dispatch now exposes that exact mask, independent
of contradictory authored class/code masks, and derives negative names only
from clear bits. Raw-frame Enable/Disable overlays retain later positive enables
without reapplying the original package mask. Absent-frame probe calls are
permitted, but Enable/Disable still cannot create a frame. Enable cannot add a
probe excluded by the authored class/state eligibility expression.

This does not complete the transitioned-frame model: selected StateNode and
running Node/nullable Code still need independent identities; per-state negative
records still approximate transitioned live masks. Original callback ordering,
flag-based transition preemption, label search and state-change throttling also
remain prerequisites to real world startup. Existing recognized negative save
records are retained as explicit saved gates; their intent is not guessed from
the old polarity bug. Codec8/envelope11 bytes and versions remain unchanged.

Bounded disassembly, export/vtable resolution, binary hashes and original script
inspection are retained under `artifacts/landing-state-20261010/`. No original
commercial binary or script package is committed.

## Continuation, transaction and persistence

An existing Sleep or landing wait on a non-null-code portable frame returns
committed Waiting from the no-time ResumeState API without polling, preparing
code or executing the next statement. Saved null-code waits are retained but
return without polling, executing or reporting Waiting. A newly reached wait yields only after its complete
top-level state statement. Ordinary nested calls finish and copy OUT values;
Context changes its actual receiver, not the caller's latent status.
Unimplemented latent natives, poll tokens used as script functions and live
iterator continuations still refuse explicitly.

AdvancePortableActorState polls once and runs the live continuation with the
same interpreter, limits and root transaction. LongFall uses that synchronous
callback route, not another public Execute/transaction. It may replace state,
clear a frame, install Sleep, alter Physics or mark the actor pending-kill.
Polling does not overwrite its replacement frame or recheck Physics and release
the wait after the callback. A newly reached wait is not polled again with
leftover elapsed time. Pending-kill after the callback prevents continuation.
Failure or budget exhaustion rolls back the native counter with properties,
frame/locals, callbacks, RNG and native/birth effects.

StateLatent::WaitForLanding remains its existing wire ordinal11. The actor's
existing optional latentTimeLeft uses codec8/runtime envelope11, with no new
schema/version. Typed preflight requires a Pawn receiver and retained native
counter for landing waits, including null-code frames. It does not require
Falling physics: a valid restored nonfalling wait releases on its next eligible
poll. Counters can remain with Stop, Continue, null code or no frame. Legacy
envelopes1–10 reset omitted counters; optional RNG and exact codec1–7 bytes
remain unchanged. Validation does not poll or publish state.

## Remaining scope

This API is still an explicit ProcessState foundation, not Actor.Tick. Real
falling movement, collision/landing events, animation advancement, Tick/Timer
phase ordering, selected-StateNode versus running-code identity, original
state-change throttling and repeated pending-kill checks while executing state
statements remain separate work. A grounded test does not prove physical
landing; a successful backend slice does not prove headset campaign gameplay.

## Verification

The focused asset-free controls exercise declaration/raw/nested/Context waits,
strict timeout/repeated events, callback replacement and pending-kill, shared
budgets/rollback, no elapsed reuse and finite arithmetic boundaries. The codec
test includes an independently assembled ordinal11/2.5 literal, signed extremes,
mixed birth/clock/lifecycle/AI/RNG data and unchanged old wire selection.

Final asset-free verification on the positive-probe source:49 ordinary host
tests pass (three original-data integrations run separately); state execution
passes473 checks/58 rejections, codec controls6477/5731, dispatch63562/20, and
generated actor lifecycle1353/14. Read-only original runtime and class-default
integrations pass, including actual Tick Enable/Disable state-only persistence
and explicit Destroyed Disable/Enable, rather than relying on inverted package
bits. Defaults/lifecycle covers998 checks. The full original actor integration
also passes, including genuinely disabled fully qualified AnimEnd and exact v4
restoration. These controls do not establish campaign startup. Its final log is
`original-actor-probes-verified.log`; the other final logs have `-probes-final`
in `artifacts/landing-state-20261010/`;
earlier fixture failures are retained as diagnostic history, not passing runs.

The original-data actor suite has `--landing-only` coverage using actual
WaitForLanding and LongFall declarations, an actual StartUp Stop boundary,
warm/cold restoration, nonfalling/frameless native calls, shared Sleep storage,
missing timer rejection and explicit event-gate controls. Startup and callback
outcomes are recorded from the actual original code, not assumed complete.

The observed enabled original timeout executes286 instructions and commits
Waiting in ScriptedPawn.FallingState at Sleep PC20, opcode0x61. Its counter is
0.7 and its next statement is PC28. The callback still runs when a legacy
disabled[StartUp].LongFall record is retained: LongFall is not a probe.
After the actual StartUp Sleep on the isolated generated weapon-slot fixture,
WaitForLanding and FollowOrders now reach original Standing Sleep at PC304,
opcode0x61. Its authored expression is FRand()*14+8, and its next statement is
PC319. This is an observed bounded startup/standing slice, not a full actor tick
or campaign startup. The committed two-object weapon/ammo graph is retained.

```powershell
.\artifacts\prerequisites-20261009\build\quest_state_execution_test.exe
.\artifacts\prerequisites-20261009\build\quest_script_state_test.exe
.\artifacts\prerequisites-20261009\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex' --landing-only
```
