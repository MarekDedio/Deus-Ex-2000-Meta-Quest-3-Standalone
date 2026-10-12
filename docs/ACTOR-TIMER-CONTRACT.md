# Actor SetTimer / Timer contract audit

Read-only prerequisite audit, 2026-10-12. Native280 remains unsupported; this
document does not implement actor ticking or claim a playable original campaign.
Only this document was written. Original executables/DLLs were not loaded or
executed. Package metadata and bounded static DLL disassembly were read.

## Authority and important reference-engine differences

Owned GOTY root: `D:\Steam\steamapps\common\Deus Ex`.
Engine.dll SHA256, freshly checked:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Independent reference: `third_party/SurrealEngine` at
`677ee14c5b83486e6634687953779aafb7973ad6`. Its source is useful evidence,
but is **not the retail timer contract**:

| Behavior | Owned retail Engine.dll | Pinned independent source |
| --- | --- | --- |
| SetTimer negative rate | Stores the signed argument unchanged | Clamps to zero |
| Expiration comparison | Counter >= Rate | Counter > Rate |
| Several periods elapsed | Removes all whole periods; one Timer callback | One callback for each period in a while loop |
| Ordinary tick order | Tick, ProcessState, Timer, LifeSpan, physics | Tick, state, physics, Timer; LifeSpan afterwards in level scheduler |

Copying the independent loop would change authored gameplay, especially after
long frames. Do not describe per-expiration replay as a more faithful timer.
Static reads are reproducible with the installed NDK27 LLVM18 `llvm-objdump`
using `-d --x86-asm-syntax=intel --start-address=... --stop-address=...` on the
owned Engine.dll: ranges103012d5..103012da, 103e3920..103e399f,
10303daa..10303daf and103a2690..103a278d. The disassembler's nearest-symbol
label is not body identification; named export thunks were followed explicitly.

## Original declarations and native entry

Current `script_bytecode_inspect.exe` inspection of the owned packages proves:

- `Engine.Actor.SetTimer`: native280, flags0x401 (Final|Native), no return;
  `NewTimerRate` is one FloatProperty and `bLoop` one BoolProperty, both flags0x80
  (Parm, not OptionalParm). Require the real declaration and Actor receiver.
- `Engine.Actor.Timer`: no parameters, event flag0x800, empty inherited body.
- The existing original property-chain audit at
  `artifacts/decal-native-20261012-render/actor-property-chain.log` records
  `Actor.TimerRate` FloatProperty flags0, `Actor.TimerCounter` FloatProperty
  flags2 (Const), and `Actor.bTimerLoop` BoolProperty flags0, all dimension1.
  These are reflected actor fields, not latent Sleep/state-local storage.

Retail export `?execSetTimer@AActor@@QAEXAAUFFrame@@QAX@Z` has thunk103012d5,
body103e3920. It evaluates both arguments and consumes the parameter terminator
before writing. At103e398c it zeros actor+4c (TimerCounter), at103e3995 stores
the raw rate into actor+48 (TimerRate), and updates only actor flag0x1000
(bTimerLoop) at103e3983..103e3998. There is no rate clamp, frame requirement,
immediate Timer call or begun-play test in this native body. Repeating SetTimer
with the same rate still resets its counter. Zero/negative rates remain inactive
when Tick tests Rate>0. Native writes to Const TimerCounter must not grant script
assignment permission to that field.

## Original expiration, dispatch and callback mutations

`?Tick@AActor@@UAEHMW4ELevelTick@@@Z` thunk10303daa leads to103a1aa0.
The ordinary path's timer block is103a269b..103a2707; existing bounded evidence
is `artifacts/sleep-state-20261010/original-tick-phase-order-disassembly.log`.
For finite, positive Rate and representable whole-period count:

1. If Rate<=0, leave the counter unchanged. Otherwise add this actor tick's
   elapsed time to Counter.
2. On Counter>=Rate, calculate the whole-period count by dividing by Rate and
   converting through MSVCRT `_ftol` (truncation, not nearest rounding).
3. Store Counter minus count*Rate. For a nonlooping timer, clear Rate **before**
   invoking its callback. The counter retains the fractional remainder even for
   a one-shot; it is not unconditionally cleared on expiration.
4. Resolve and synchronously ProcessEvent Timer **once**, regardless of count.
   There is no post-callback timer loop and no further timer write in this block.

Evidence: add/compare/store103a26ab..103a26bc; division/conversion and remainder
103a26be..103a26d8; nonloop clear103a26da..103a26e4; lookup and ProcessEvent
103a26eb..103a2704. Conversion thunk10413652 jumps through IAT1059a34c,
the `_ftol` entry in the MSVCRT import table. The timer block uses a live x87 sum
for comparison/division even after storing the binary32 counter. A portable
helper must characterize boundary rounding explicitly, not assume a rounded
float addition or arbitrary epsilon reproduces those operations. Conversion
overflow/nonfinite behavior is not an accepted portable implementation policy.

The exported `eventTimer` thunk10303364/body10323160 uses the same Timer FName
global10583718 and virtual ProcessEvent+10 as the tick block. Lookup must use
the current selected StateNode and explicit inherited dispatch graph, not cache
the class's Timer function. Current live probe-mask Enable/Disable/ignores,
pending-kill and Level.bBegunPlay gates still apply; no frame passes the original
probe test. See [STATE-CONTROL-MODEL.md](STATE-CONTROL-MODEL.md),
[ORIGINAL-STATE-DISPATCH.md](ORIGINAL-STATE-DISPATCH.md), and the original audit
`artifacts/landing-state-20261010/original-state-identity-audit.md`.
Actor ProcessEvent10369100 freshly confirms the begun-play gate.

Suppression of the event does not undo the already reduced counter/cleared
one-shot rate. A callback can SetTimer, assign Rate/Loop, Disable Timer, GotoState,
Destroy, spawn or mutate other actors. Its writes survive this timer block;
SetTimer rearming starts at counter0, with no use of the old elapsed remainder
in a second callback. State transitions cancel latent waits, not these fields.

## Why real authored callers matter

Fresh owned-package inspection establishes these exact bytecode PCs:

- `Engine.PlayerReplicationInfo.PostBeginPlay`: an ordinary synchronous call to
  `Timer` atPC21, then `SetTimer(2.0,true)` atPC27. The explicit call must not be
  mistaken for native SetTimer firing immediately. Its Timer includes RNG and
  player/zone/console dependencies, not merely a counter increment.
- `DeusEx.DataLinkTrigger.EvaluateFlag` atPC129 arms a one-shot while its flag is
  unsatisfied; `DataLinkTrigger.Timer` calls EvaluateFlag again. Rearming from a
  one-shot callback must survive the engine's prior disable. FlagBase native1110
  is a separate remaining dependency.
- `DeusEx.DataLinkPlay.Timer` advances its own `eventTimer` by `blinkRate` once
  per callback, then disables its timer atPC106/209 and transitions/terminates.
  Replaying several Timer callbacks after a long frame accelerates these authored
  UI phases incorrectly.
- `DataLinkPlay.WaitForSpeech` arms native280 atPC107 with ConSys speech length
  (native2056), or atPC154 using text length*perCharDelay. Its **state-specific**
  `WaitForSpeech.Timer` atPC0 GotoStates to `SpeechFinished`; the class Timer is
  not an interchangeable callback. Audio, original UI/conversation bindings and
  other native dependencies remain separate blockers.

## World scheduling boundary

An isolated timer helper is not a full actor scheduler. The original ordinary
path invokes ProcessState at103a2698, then Timer, LifeSpan at103a270b, and physics
dispatch at103a276d/103a2783. Tick entry also has InStasis and owner-defer paths
(103a1ac8..103a1b88); it is not an unordered loop over active render snapshots.
Nonordinary role/tick-mode/player branches, original level pause/players-only,
world slot/owner ordering and births/deletions still need a complete retail
scheduler audit before claiming automatic world simulation.

For comparison only: pinned `Packages/Engine/Resources/Level/ULevel.cpp:58`
ticks owners first, prevents double ticks, selects pause/players-only actors,
iterates a live slot vector and compacts afterwards. Its actor spawn code has a
same-tick bTicked TODO. Pinned `Packages/Engine/Actors/UActor.cpp:169` and
`Native/NActor.cpp:657` show the timer mismatches above. `Engine.cpp:172/1035`
uses game-time dilation and a one-second real-delta clamp; that source's clamp
is not proof of retail timing. VR tracking loss, audio/subtitle presentation
time and map-worker pauses must not silently define original game elapsed time.

## Portable integration, persistence and refusal requirements

- `PortableActorVmHost::Native` has no native280 handler. The unmodified Training
  world probe is pinned by `desktop/portable_actor_script_test.cpp` to
  PreBeginPlay, slot1331 `00_Training.Camera4`, actual function
  `Engine.PlayerReplicationInfo.PostBeginPlay`, PC27/opcode97, with
  `Unsupported runtime native 280` and complete rollback. Adding the entry only
  moves this diagnostic boundary; it does not finish startup.
- `AdvancePortableActorState` explicitly advances ProcessState only: no Timer,
  Tick, physics or startup. Do not repurpose `latentTimeLeft`, which belongs to
  Sleep/WaitForLanding, or add an independent shadow counter disconnected from
  the three reflected timer fields.
- Timer advancement and its callback must share a host execution/root journal:
  counter addition/remainder, one-shot disable, property/state writes, RNG,
  births/deaths, owner/base/touch links, AI/collision/decals and publication all
  roll back on unsupported code, stale receiver or budget failure. Use the
  current execution's event dispatch, not nested public
  `ExecutePortableActorEvent` calls with independent committed transactions.
  Whole-world versus per-actor tick transaction scope still needs an explicit
  design; original engine errors did not supply the port's safety rollback.
- Retain timer fields as validated original property overlays, mark committed
  script state, and include them in dirty/save/restore. Existing CaptureScriptState
  serializes qualified typed property keys; its dedicated latent counter is not
  a SetTimer section. Cold restore must preserve signed Rate, Counter remainder
  and Loop, including stopped/frameless actors. Legacy reset and map replacement
  must not merge stale timers. Do not invent Counter<Rate or Counter>=0 save
  invariants for arbitrary reflected values. Save only at a quiescent transaction
  boundary. Current Variable's explicit Const-write restriction covers native
  INT arrays only; scalar timer write permissions need a deliberate port policy,
  not a claim that retail bytecode dynamically enforces its compiler's Const rule.
- Current host Commit marks **any touched Actor** as a world change, calls
  DecodePortableRuntimeActorMeshes, rebuilds tag indices and increments world
  revision. Per-frame counter persistence needs correct simulation/render
  mutation classification before enabling a campaign tick; omitting persistence
  or skipping nonvisible actors is not a valid performance fix.
- Explicitly reject invalid elapsed time, nonfinite data, unsafe count conversion
  or counter/remainder overflow atomically. Such finite/safety limits are port
  refusal policy, not evidence of retail input validation. Do not replace the
  retail once-per-tick coalescing with guessed callback caps or replay loops.

## Required regression evidence before enabling

1. Actual native declaration/type/receiver validation; positive, zero and negative
   rates; both loop values; same-rate rearm; argument side effects and rollback.
2. Exact equality fires; below threshold does not; multiple whole periods produce
   one callback and the original remainder; looping and nonlooping; zero elapsed
   with an already due counter; selected x87/binary32 boundary fixtures.
3. Callback reads cleared one-shot rate/remainder, rearms, changes rate/loop,
   disables Timer, transitions to a different state Timer, destroys/spawns or
   changes another actor; no second same-tick callback. Probe/begun/deletion
   suppression consumes elapsed periods without running the body.
4. Native280 and callback failure/instruction/depth/storage budgets restore exact
   save bytes, RNG, objects/GC, state, collision/links and world revision. Overflow
   and nonfinite refusals must not leave a half-advanced counter.
5. Cold save/restore equality and future expiration, signed inactive rates,
   one-shot residuals, stopped/frameless actors, legacy reset and map retirement;
   ordinary script cannot write Const Counter while native updates can.
6. Unmodified original PlayerReplicationInfo and DataLink state call paths, then
   complete world scheduler order/eligibility tests and an actual successful
   original startup. Generated fixtures or a handpicked actor list are not proof
   of complete campaign timing. Performance evidence must show timer-only writes
   avoid unnecessary geometry publication without losing genuine callback effects.
