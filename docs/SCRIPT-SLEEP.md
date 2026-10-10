# Explicit original actor Sleep continuation

This increment implements native Sleep256 and Pawn.StopWaiting (native0), an
explicit native wait-poll API, and portable saved wait continuation. It is backend
work. It does not install a new headset build, start the original world, tick all
actors, implement movement/AI, or establish a playable campaign.

## Original executable oracle

The read-only owned GOTY installation is the authority for these operations,
not the differing behavior in the pinned reference engine. Original Engine.dll
SHA256 is `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`;
Core.dll is `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

Engine.dll execSleep at `103dfef0` evaluates seconds and its parameter terminator,
sets the receiver actor's StateFrame latent action to384, and stores the raw float
seconds in actor field+e0. It does not clamp negative or zero seconds. This is
actor-owned native storage, not a local in the calling function's frame.

execPollSleep at `103e0020` subtracts the elapsed float, stores the signed binary32
remainder with `fst`, then compares the still-live extended remainder against
elapsed times the double constant0.5. It clears the action only when remaining
time is strictly less than half the elapsed time. Equality keeps waiting. The
portable helper compares the unrounded difference and stores the float result;
it does not clamp the result or carry spare elapsed time into a subsequent Sleep.
Sleep(0) needs a positive poll; a negative counter can wake with zero elapsed.
Nonfinite data, negative elapsed and finite-float storage overflow reject.

Pawn.StopWaiting's body at `103ba590` tests current latent action384. Only then
does it assign float-1 to the same actor field. It does not clear the action or
run script immediately. Calling it on a nonsleeping actor is a no-op.

Original ProcessState at `103e5b60` returns for absent frame/code, pending-kill
actors, or Role below4 with a nonsimulated selected state (flag4). It polls a
current latent action once before running statements. A Sleep newly reached
while running ends that slice without a second poll. Its complete state-change
throttle and selected-StateNode/code-pointer distinctions are not yet modeled by
the portable slice interpreter.

Original Core internal GotoState at `1012e8f0` clears a wait before transition
callbacks; GotoLabel at `1012eb10` clears it before label search. EX_GotoLabel's
wrapper at `1012fd80` calls the same virtual label operation after evaluating its
expression. The portable supported transitions and successful labels now cancel
waits accordingly, including a Sleep in the label expression. The actor-owned
timer survives. Missing-label/null-code representation still follows the earlier
portable approximation: transition misses use Stop, in-code misses refuse. This
does not claim byte-exact original FStateFrame identity in those cases.

The evidence logs under `artifacts/sleep-state-20261010/` retain DLL hashes and
bounded disassembly of Sleep/poll, StopWaiting, ProcessState, internal GotoState/
GotoLabel and the EX_GotoLabel wrapper. No original binaries are copied or added
to the repository.

## Execution and ownership

`ResumePortableActorState` remains a no-time state slice. An existing Sleep
returns committed `Status::Waiting` without polling or executing statements.
New Sleep yields after the complete enclosing top-level statement. Ordinary
nested functions finish and copy OUT parameters back; their transient call
frames are not serialized. A Context receiver sleeping another actor does not
yield the current actor. A live native iterator at yield is still an explicit
rollback failure until persistent iterator storage exists.

`AdvancePortableActorState(actor, elapsed, limits)` polls native state eligibility
and the current wait, then runs the eligible continuation in one interpreter and
root transaction. Timer, latent action, state PC/locals, properties, callbacks,
RNG, births and native links roll back together if the continuation fails or
exceeds a budget. There is no separate committed timer update before script.
Invalid elapsed time is rejected before opening a transaction.

The API is an explicit ProcessState foundation, not Actor.Tick. In particular,
animation advancement, Tick event, timers, physics, actor ordering, original
state-change throttling and repeated pending-kill loop checks remain separate
work. The original ordinary authority Tick orders Tick event, ProcessState,
Timer, then physics; enabling a partial scheduler would not implement that order.
Simulated eligibility currently uses the portable running code descriptor;
selected StateNode identity must be separated before inherited-code scheduling
can be claimed fully original.

Native Sleep requires an already established portable actor frame. Dormant raw
serialized continuations and guessed native class-frame creation remain refused.
No general latent flag exemption was added: other latent declarations/actions
still require their own runtime handlers. WaitForLanding uses this same native
field in the original, but its handler is not implemented in this increment.

## Saved native wait

Script codec8 is selected only when at least one actor has a retained native
`latentTimeLeft`. Each actor gets a presence byte and optional signed finite f32,
after its lifecycle record. All lower sections are present, possibly empty; the
final RNG presence byte is followed by the unchanged algorithm1/uint32 seed only
when a stream is actually retained. No RNG seed or frame is invented for a
timer-only save. Removing all counters recovers exact codec1–7 wire output.

Runtime envelope11 requires codec8 and at least one counter. A sleeping frame
requires its actor counter during typed preflight. A counter can also belong to
a stopped, null-code or frameless actor because it is actor storage, not state
frame storage. Validation does not mutate it. Application publishes staged
state only after complete preflight; envelopes1–10 explicitly reset omitted
counters instead of merging current timers. The existing paired save/recovery
format is unchanged, and map-scoped state still blocks unsupported travel.

## Verification

The asset-free state controls cover strict wake timing, Sleep(0), signed values,
no-time resume, no spare-time reuse, nested calls/OUT, other receivers, the
original latent declaration, unknown waits, instruction/failure rollback and
live-iterator refusal. Codec controls include an independent literal timer-only
wire fixture, negative zero, mixed lower sections/RNG, old codecs, truncation,
bad presence flags, nonfinite values, missing counters and cumulative budgets.

The separate original actor test reads the owned installation, calls the actual
Sleep/StopWaiting declarations, and uses an actual StartUp Stop boundary to
isolate polling. It verifies role/delete/null-code gates, warm and cold restart,
read-only validation, counter retention without a frame, legacy reset and a
sleeping frame missing native timer evidence. Its `--sleep-only` option runs the
focused original coverage without claiming full StartUp completion.

On the isolated generated WeaponPistol inventory fixture, actual StartUp now
commits its real weapon/ammo initialization and Sleep at PC6, opcode0x61.
Waking reaches unsupported WaitForLanding527 at PC18, opcode0x62. The failed
wake preserves the committed inventory, native timer, complete serialized state,
actor counts, GC objects/memory and world revision. This does not imply that
Doctor1's unchanged authored inventory contains that generated weapon slot.

Final source verification passes all49 ordinary host tests and all three
separate original-data integrations. State controls pass296 checks/40 rejections;
save-codec controls6467/5731; existing random controls58942/37. Original actor
coverage includes Training, UNATCO Island and Intro. Android ARM64 build and
APK v2 signature verification pass; the16-entry archive has no commercial
packages. Sizes and hashes are in [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).

Final evidence includes `host-build-final-verified.log`, `host-ctest-final.log`,
`state-controls-final.log`, `codec-final.log`, `random-controls-final.log`,
`original-actor-final.log`, `original-runtime-final.log`,
`original-defaults-first.log`, `android-build-final.log`, `apk-signature.log`,
`apk-hashes.log` and `apk-entries.log`. Earlier logs preserve the corrected
sorted-fixture assertion, pre-Sleep versus post-inventory actor-count assertion,
and a test-executable file lock during a rebuild. These are not final failures.

```powershell
.\artifacts\prerequisites-20261009\build\quest_state_execution_test.exe
.\artifacts\prerequisites-20261009\build\quest_script_state_test.exe
.\artifacts\prerequisites-20261009\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex' --sleep-only
```

The accepted installed seated-player APK remains unchanged. The newly built
backend APK is an archived development artifact, not a headset-tested replacement.
