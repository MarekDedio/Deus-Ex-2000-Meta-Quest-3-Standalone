# Original Actor main-animation phase

Static owned-GOTY audit and portable implementation, 2026-10-24. This is the
ordinary main channel inside `AActor::Tick`, not a complete actor/world Tick,
not blend animation, and not yet proof that a collected Training weapon can
attack a crate on Quest. The immediate integration target remains the entire
first `Maps/00_Training.dx`, including original weapon notifies, crates, loot
and gravity. See [the live melee gap](TRAINING-LIVE-MELEE-BRIDGE.md).

## Authority and bounds

Owned `System/Engine.dll` SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Its `AActor::Tick` thunk is10303DAA, body103A1AA0. The main phase starts at
103A1BF0 and exits at103A1FA1. No commercial DLL or EXE is loaded or executed
by this implementation or its tests. Static bytes were inspected with the
installed `objdump`.

`native/quest_actor_animation_phase.h` implements this phase separately from
the earlier `quest_actor_animation_clock.h` pinned/corrected policy. It does
not reuse that helper's handler filtering, corrected past-end time or
configurable catch-up budget. The actual preceding Tick stasis/owner/parity
work and the following blends/state/timer/lifespan/physics phases remain
separate obligations; see [native Tick ordering](ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md).

The adapter must authenticate actual inherited original Actor declarations,
types, dimensions and constraints before supplying these live fields:

| Native source | Reflected meaning |
| --- | --- |
| Actor+54 | AnimSequence FName |
| +58 / +5C / +60 | AnimFrame / AnimRate / TweenRate Float |
| +220 / +224 | AnimLast / AnimMinRate Float |
| +144,+148,+14C | Velocity.X,Y,Z Float |
| +28 bits20,10,08 | bAnimNotify / bAnimLoop / bAnimFinished |
| +174 | actual Mesh reference |
| +22C / +230 | SimAnim.X / SimAnim.Y Float |
| +32 | RemoteRole Byte |
| FStateFrame+24 | native LatentAction integer |

These are associations observed in retail instructions, not portable memory
offsets. A serializer's reflected Offset must not be used as a host layout.
Actual mesh sequence lookup must not guess PlayerViewMesh, choose an idle
sequence or fall back to the first sequence. The source UObject identity must
remain retained through callbacks and deferred deletion.

## Retail operation order

1. At103A1BF9, test current AnimSequence against FName index0. Then, lazily,
   test Frame>=0 ? AnimRate!=0 : TweenRate!=0. Then require the native remaining
   elapsed local>0. These precede the loop counter. Negative finite elapsed
   exits; it is not converted to a positive animation delta.
2. Increment the native iteration count and allow only its first **four**
   iterations (103A1C56..60). Missing notify handlers consume iterations too.
   Do not replace this with an unbounded catch-up loop or arbitrary event cap.
3. Capture old Frame for this iteration. For ordinary playback, nonnegative
   Rate multiplies remaining elapsed. Negative Rate uses
   `max(AnimMinRate,-appSqrt(Velocity.SizeSquared())*AnimRate)` before multiplying
   remaining elapsed. The retail sum order is `(X*X+Y*Y)+Z*Z` despite loading
   the input registers Z,Y,X. It spills that sum to binary64 before appSqrt.
4. Add current Frame; read bAnimNotify **before** storing the tentative result
   as Float (103A1CF7/CFB). If enabled and actual Mesh/sequence exist, search
   all original notify records before comparing AnimLast.
5. A candidate must satisfy `oldFrame < time && storedFrame >= time`. The
   first reached candidate is unconditional; `100000.0f` at103A1D27 is only an
   initial best-distance value, not a time cap. Later candidates compare their
   **PC64** `time-oldFrame` against the prior **Float** best-distance spill.
   Therefore equal times normally keep the first record, but can choose a
   later duplicate when the common distance rounded upward in the Float spill.
6. Commit Frame to the selected notify time at103A1D8A **before** reading its
   function FName and before `FindFunction(name,0)`. Remaining elapsed becomes
   Float `((storedTentative-time)*remaining)/(storedTentative-oldFrame)`.
   A missing function loops without ProcessEvent, but does not undo this
   boundary or restore the old elapsed. A found function invokes real selected
   ProcessEvent synchronously with null parameter/result packets. Then loop
   and reread all live animation/mesh state. No new deletion/world-slot gate
   appears here.
7. If no notify was selected and stored Frame<AnimLast, exit. Otherwise an
   actual loop either sets remaining elapsed0 when Frame<1, or resets Frame0
   and retains Float `(Frame-1)*remaining/(Frame-oldFrame)`. Only when
   `oldFrame<current AnimLast` does it read native FStateFrame.LatentAction:
   native value **385** sets bAnimFinished; then, unless the captured Pawn
   suppression applies, it calls selected AnimEnd. Native385 is FinishAnim,
   not a Class FName index or a portable enum ordinal. The FinishAnim thunk
   103037BA targets103DFF60; its store is103DFFC4. PollFinishAnim at103E00A0
   releases that action when bAnimFinished.
8. A nonloop end stores Rate0, Frame=AnimLast, bAnimFinished=true and Float
   `(tentative-AnimLast)*remaining/(tentative-oldFrame)` before selected
   AnimEnd. This also runs when the original Frame was already past AnimLast;
   no corrected-time/skip policy is substituted. After AnimEnd, reread actual
   RemoteRole. If below2 and not an actual Weapon descendant, store fresh
   Frame*10000 to SimAnim.X and fresh Rate*5000 to SimAnim.Y. The latter Float
   store precedes comparison of its retained PC64 product to32767 and a
   possible second SimAnim.Y=32767 store. No lower clamp is present.
9. For negative-frame tweening, retain the PC64 tentative `Frame+remaining *
   TweenRate` through the zero comparison and residual division, even though
   it has already been stored to Frame as Float. A negative PC64 value which
   spills to negative zero still exits as an unfinished tween. At a crossing,
   store Frame0 and Float `tentative*remaining/(tentative-oldFrame)`. If live
   AnimRate==0, mark finished before optional selected AnimEnd. This tween
   branch does not acquire the nonloop RemoteRole/SimAnim tail.

The captured suppression flag comes from the preceding Tick context: actual
bIsPawn, a real Pawn ancestry check, and Role==2 (103A1BAD..1BF0). It suppresses
AnimEnd only, not mesh notifies. It is not refreshed after a callback. The
helper does not pretend to implement that preceding Pawn-specific phase.

## Arithmetic and event contracts

All arithmetic uses deterministic integer-dyadic x87 PC64 nearest-even
operations with the original individual Float spills. The speed square root
uses the existing independently tested binary64-argument/PC64 FSQRT primitive.
Engine's IAT10599CA4 has Hint/Name RVA0029BAB4, importing
`?appSqrt@@YANN@Z` from Core. Core10123290 is exactly `fld QWORD [esp+4];
fsqrt; ret`; the diagnostic Double store103A1CCC does not pop/narrow its live
PC64 result. This is not host `sqrtf` or an assumed binary64 return.

Ordinary finite active storage/arithmetic is supported. Active nonfinite
fields, nonrepresentable stores, undefined boundary division and exceeded
name/notify budgets explicitly throw into the caller's existing root. Lazy
inactive fields are not eagerly validated. Notify records are not normalized
to[0,1] or silently sorted/filtered. Metadata providers must reject invalid
table storage/lifetime rather than manufacturing records.

Every write provider must update only that live field through the existing
journal and Execution work/write accounting. There is no captured clock to
reinstall after ProcessEvent. Every reached metadata/field/event provider is
work-accounted; unrelated inactive providers can be absent. A failure after
early Frame/notify/callback stores is a whole-root refusal, not successful
partial publication.

`FindFunction(name,0)` is nullable and remains unchecked. The checked AnimEnd
provider must reproduce actual `FindFunctionChecked(AnimEnd,0)`, including
its global `GIsScriptable` behavior: Core10150BF0 loads that global101E5540;
false returns null. Retail nevertheless passes this null declaration into
Actor.ProcessEvent. The wrapper's begun-play gate and Core's scriptable gate
then prevent a dereference. Accordingly the helper forwards the retained
checked result even when null; it neither invents an event nor adds an early
refusal. Scriptable-enabled missing checked lookup must still fail in the
provider. Actual ProcessEvent owns its native scriptable/begun/probe/delete
gates and runs any eligible selected declaration in the **same Execution**.
The helper's event counters count attempted native dispatches, not proof that
those gates admitted script execution.

`remainingSeconds` in the returned statistics is the raw native loop local,
**not resumable owed elapsed**. A normal sub-end advance has consumed the
frame's animation time while leaving that local equal to its input. Do not
feed that number back into another animation call just because it is nonzero.
At the literal iteration cap, use original per-Tick behavior; do not invent a
portable residual catch-up policy.

## Tests and unproven integration

`desktop/quest_actor_animation_phase_test.cpp` covers lazy gates and signed
zero, unsorted and duplicate notifies, the rounded Float best-distance case,
first candidates beyond100000, missing handlers consuming four iterations,
notify-before-end ordering, tween negative-zero spills and residuals, loop
FinishAnim385/AnimEnd suppression, original past-end completion, synchronous
replacement of mesh/rate/sequence, fresh post-AnimEnd SimAnim fields, retained
deleted source identity, null checked results passed to ProcessEvent, authentic
checked failures, and all reached work/write-prefix cutoffs in a test journal.

On x86 GCC it also runs a separate host x87 PC64 oracle written from the retail
instruction order, using volatile extended intermediates, actual FSQRT and
binary64 argument spills. Its unchanged-state/no-op-handler matrix and seeded
random cases compare all final raw Float fields, events, iterations, cap and
native remaining-local bits. It does not call the portable dyadic arithmetic
or execute the commercial DLL. ARM64 still runs the deterministic/provider
fixtures; its lack of an x87 oracle is reported explicitly.

The focused host helper passes225385 checks/102 refusal controls, including
7680 independent x87 oracle cases. Its first compile found an ADL ambiguity
between timer-specific and general PC64 division; explicitly qualifying the
general signed division resolved it without changing arithmetic. Evidence:
artifacts/training-main-animation-pure-final-20261024.log. A pure helper
test, including its local journal, does not prove production Execution
rollback, actual selected-script events, full native Actor Tick, live held
weapon animation, HandToHandAttack/TraceFire, crate destruction/loot ownership,
or Quest rendering/performance. Those need separate original-script runtime
and on-device acceptance; the [melee bridge witness](TRAINING-LIVE-MELEE-BRIDGE.md)
remains required.

## Runtime adapter under verification

`portable_actor_animation_phase.inl` binds the phase inside PortableActorVmHost
to the same supplied Execution. Actual per-field References update only the
matching optional clock member, not a stale full clock. Lazy mesh providers
retain real source identities; the loader now preserves each notify's original
serializedIndex alongside the existing presentation time sort. The adapter
reconstructs and validates the complete original ordinal permutation before
search, retaining all duplicate/unsorted source records.

Original Core export `?GIsScriptable@@3HA` names101E5540. Static original
Setup.exe imports it at10468D30 and writes1 at1040DEFE before its appInit call;
Editor.dll writes0 through1037089C at10219BCD..BD2. The portable launcher's
explicit game-process mode enables this service only for a successful runtime
initialization and disables it on shutdown. This does not claim reconstruction
of the packed DeusEx.exe launcher or permit fabricated begun/player flags.

The exact nearest native classes admitted by this adapter have independently
audited ProcessEvent+10 and IsPendingKill+2C virtuals. Engine Actor/Decoration/
Inventory/Projectile/Pawn/Weapon select10302D74->10369100 and
10302167->1030DF80. DeusExDecoration/DeusExPlayer/ScriptedPawn import the same
Actor methods via1001AF98/1001AF7A. Real serialized knife/crowbar Class chains
select native Engine.Weapon; Doctor selects native DeusEx.ScriptedPawn. Other
native boundaries remain explicit refusals, not inheritance assumptions.
The begun-play field has original property flags0, not Const2. Event order
remains retained lookup, actual Actor begun gate, Core scriptable gate, selected
probe, native pending kill, native-index suppression and then script execution.
Reached nativeIndex0/flags400 remote dispatch remains explicitly unimplemented.

`ProbePortableActorMainAnimation` is rollback-only; it captures the original
outer Pawn/Role2 suppression without executing or claiming that surrounding
Tick. An owned Training Doctor notify test exercises genuine metadata, real
PlayAnim and per-store phase writes with actual dormant begun suppression.
The actual owned Training Doctor1 Walk/PlayFootStep boundary0.1 passes70 checks/
six refusal prefixes, including every three-store write cutoff, three native
work cutoffs, full checkpoint/revision/GC undo and a repeated identical phase.
It performs three native stores and searches four original notify records.
The first fixture run correctly refused an undecoded mesh query; the final
fixture reads the original source mesh directly, then lets actual PlayAnim
decode its runtime asset. The root probe's Role declaration was corrected
from RemoteRole's flags21 to actual Role flags20 before this successful run.
Evidence: artifacts/training-main-animation-owned-final-20261024.log.
No active script notify or live weapon hit is implied by a found handler whose
ProcessEvent is suppressed. Modern unknown native latent provenance remains a
lazy refusal on the reached loop-ending branch, not a guessed Continue=0.
Final Android native compilation/link pass (training-main-animation-android-final
log). No APK or headset deployment is implied; the Quest is currently absent
from adb. Final strict ARM64 runtime/test syntax passes. Full coherent desktop
build/shared CTest pass180 executed/213 registered,33 explicit owned-data skips,
zero failures in134.67s. Fresh original crate controls pass347 checks/two
dormant contents refusals separately. Both normal no-command Training startup
repeats retain exact native1463 at218693 instructions/24284 writes, native
work492818 cold/492276 warm; full-root undo/all41 unchanged source fingerprints
pass. Logs: training-main-animation-full-build/ctest/crate/normal-startup and
runtime-syntax-final with date20261024 in artifacts/.

Reproduce static evidence without executing commercial code:

```powershell
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A1BF0 --stop-address=0x103A1FA1 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -s --start-address=0x10599CA4 --stop-address=0x10599CA8 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10123290 --stop-address=0x10123297 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10150BF0 --stop-address=0x10150C20 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
```
