# Original ordinary Pawn Tick continuation and short retained tail

Owned GOTY static audit, 2026-10-26. Scope: the first `00_Training.JaimeReyes0`
boundary after genuine MAIN/BLEND. This does not implement or admit APawn
physics, the simulated-Pawn branch, player input, network execution or the
tickType2 perception tail. It does not establish playable Training.

Original `Engine.dll`, `Core.dll`, `DeusEx.dll` and owned Engine/DeusEx packages
were read only. External objdump followed actual instructions, vtable words,
imports and export tables; the existing `script_bytecode_inspect.exe` read the
owned `.u` metadata. No commercial DLL/EXE was executed or loaded. Original
identities and broader context are recorded in
`ORIGINAL-PAWN-TICK-PREPROCESSING.md` and
`ORIGINAL-SCRIPTEDPAWN-TICK-PREREQUISITES.md`.

## Actual first Training route

The current runtime binds this ordinary suffix through
`portable_actor_pawn_tick.inl`, with shared native callback gates in
`portable_actor_native_event.inl`. It runs selected Tick, actual ProcessState,
native timer, LifeSpan and the short retained-Pawn tail on the enclosing Level
Execution. The native timer uses `quest_actor_timer_phase.h`, not the older
generic timer shortcut. Raw ViewRotation copying and actual XLevel weapon
FarMove are bound; equipped-weapon and player-eye/view production success
controls remain deferred, not implicitly covered by the NPC tests.

Current helper verification passes82,749 checks/28 refusals for the suffix
and89,052/32 for native timer, including9,088 independent x87 timer cases.
Generated package production controls reach2,374 cumulative checks/40
refusals in the ScriptedPawn section: selected Tick/state/Timer ordering,
unconditional same-bit timer writes, expiry, actual native723, full work/write
undo and genuine later Pawn-physics/destroy errors. The complete generated
driver exits0 after7,246/136 and its final malformed-schema control.

Actual first Training repeat2 now passes the former continuation gate and
reaches `Original Pawn performPhysics is not implemented`, identically at
206,922 instructions/15,797 writes with full root undo. This is a new reached
boundary, not completed startup or NPC physics. The APawn/falling/landing
differences are recorded in `ORIGINAL-TRAINING-PAWN-PHYSICS-GAP.md`.

The actual reached entry has captured Pawn=source and simulatedPawn=false
(entry Role4), fresh Role4, RemoteRole2, Physics2, tickType0. The captured Pawn
and simulated Boolean do not refresh after animation callbacks.

1. `103A23CE..23D8` reads RemoteRole then Role, both unconditionally.
2. RemoteRole!=3 / Role>=2 goes `103A2524`. The captured Pawn is nonnull, but
   Jaime's actual ancestry JaimeReyes→ScriptedPawn→Pawn→Actor→Object does not
   contain actual APlayerPawn Class1058C160. Therefore no Player pointer or
   local-player input branch is read/executed on this path.
3. `103A2643` tests literal tickType1. Type0 continues to the Tick probe;
   type1 returns1 immediately without state, timer, lifespan, physics or tail.
4. After optional selected Tick, actual ProcessState, timer and lifespan,
   fresh Physics!=0 and fresh Role!=3 select **source virtual** performPhysics.
   Current Physics2 would select APawn's method if earlier callbacks leave it
   unchanged; it cannot be replaced with crate/Actor physics.
5. The short retained-Pawn tail follows physics or a legitimate skipped
   physics branch. Current type0 returns before the tickType2 sight/pain/
   speech/tactics work.

RemoteRole3/Role4 has its own optional PlayerPawn view-copy block, then
ProcessState/timer and the retained tail, with no normal Tick/lifespan/physics.
RemoteRole3/other Role skips directly to that tail. RemoteRole!=3/Role<2 only
dispatches physics for fresh Physics2, regardless of tickType1, then tails.
The real local-player branch joins ProcessState at103A2690, not the ordinary
Tick probe/type1 gate. Its native subclass/flag arm can return1 early; an
opaque service must preserve that reply or explicitly refuse the unimplemented
positive branch, never substitute empty player input.

## Checked callbacks and native ProcessEvent gates

The actual ordinary Tick sequence `103A2653..2669` calls IsProbing with literal
FName336 **before** any function lookup. Only a true result does
`103A266B..268D` perform FindFunctionChecked(Tick) and virtual ProcessEvent with
one raw Float delta. Timer, Expired and UpdateEyeHeight have **no extra outer
probe**: when their source branch is reached, checked lookup precedes the
ProcessEvent gates even if the callback will subsequently be suppressed.

Independent Core entry evidence:

- IsProbing export-name964, thunk1010258B→10115670: only probe names300..363
  with an actual StateFrame read its live positive 64-bit probe mask. A
  nonprobe name or absent frame returns1. `CurrentDispatchContext(actor,true)`
  plus `ScriptDispatch::IsEnabled` represents this without prematurely
  demanding runnable raw state code. Do not invent an absent-frame false gate.
- FindFunctionChecked export-name705, thunk1010263F→10150BF0: first test
  GIsScriptable101E5540; false returns null without lookup. Otherwise select
  from actual StateNode/Class ancestry and authenticate a real UFunction.
- Actor.ProcessEvent10369100 first tests actual Actor.Level.bBegunPlay, then
  calls Core.ProcessEvent1013F7D0. Core's ordered gates are GIsScriptable,
  selected function's live probe, virtual pending kill, nativeIndex, then the
  flags400 RPC virtual branch. NativeIndex!=0 suppresses execution here;
  an unrepresented RPC branch must remain an explicit refusal.

`OriginalAnimationActorVirtuals`, `OriginalAnimationPendingKill` and the
retained selector/process pair in `portable_actor_animation_phase.inl` are the
existing narrow authority. The ScriptedPawn callback adapter follows the same
pattern; a common native Actor callback can extend its packet to Float/Bool/
none and invoke `execution.CallFunction` only **after** all native gates, on the
retained Function identity. Preserve actual Class/State ownership and same-root
scratch, state and work budgets. A bare CallFunction bypasses these gates;
generic CallEvent does not itself provide the full native GIsScriptable/
nativeIndex/RPC path.

| Exact original base Function | Native / flags | Native packet |
| --- | --- | --- |
| Engine.Actor.Tick | 0 / 0x800 | one FloatProperty DeltaTime, dim1, flags0x80 |
| Engine.Actor.Timer | 0 / 0x800 | none; no parameters/results |
| Engine.Actor.Expired | 0 / 0x800 | none; no parameters/results |
| Engine.Pawn.UpdateEyeHeight | 0 / 0x800 | one FloatProperty DeltaTime, dim1, flags0x80 |

Each listed base has the original two-byte Return/Nothing body. Inputs have
baseField0, no subtype, the exact Core.FloatProperty metaclass and the actual
Function as Outer. Selected overrides may contain locals or different parameter
spelling/Function flags, but must receive exactly the selected native packet:
scalar Float flags exactly0x80, or zero flags&0x80 parameters/results for an
empty packet. Do not require selected ScriptedPawn.Tick's flags to equal800:
its actual class implementation is native0/flags2 with locals and one Float.

LifecycleEvent(Expired) is a useful existing packet/journal pattern, but its
generic CallEvent shortcut is not complete native ProcessEvent equivalence.
There is no active/delete filter inserted between native Tick, ProcessState,
timer, lifespan and physics. Each called service applies its own original gates.

## Actual ScriptedPawn ProcessState alias

The actual ScriptedPawn vtable10027624 cell+14 at10027638 contains1001AF92,
which jumps through IAT10027264, explicitly imported
`Engine.AActor::ProcessState`. Engine's thunk10301F00 reaches103E5B60.
Actual APawn vtable10426994+14 independently contains10301F00. This is a
separate proof from ProcessEvent+10; neither alias proves Pawn physics+8C.

The original state body `103E5B85..5BB8` requires StateFrame and Code, then
Role>=4 or selected StateFlags4, then virtual pending kill. It does **not**
apply Actor.ProcessEvent's begun-play/GIsScriptable event gates. It polls the
native latent slot before the live statement loop, stops on pending deletion,
null Code or nonzero latent action, and ends a slice after the fifth observed
selected-node change. The same-root `execution.RunStatePhase`, host PollState
and existing state-control machinery implement corresponding represented state
operations; unsupported raw continuations/latent handlers remain explicit.
Authenticate the actual nearest-native+14 alias before reusing this service.

## Exact timer source schedule

The ordinary block is `103A269B..2709`; the RemoteRole3 copy has the same
schedule at103A24AA..251C. All three fields are actual Engine.Actor scalars:
TimerRate FloatProperty/1/flags0, TimerCounter FloatProperty/1/flags2,
bTimerLoop BoolProperty/1/flags0, each base0 and no subtype.

| Source | Literal operation / visible effect |
| --- | --- |
|269B..26A9|FLD Rate(+48), FCOMP Double+0 at10429838; unordered/<=0 exits without Counter, loop or delta arithmetic|
|26AB..26B1|FLD delta, FADD fresh Counter(+4C), FCOM fresh Rate, retaining the PC64 sum|
|26B4..26BC|FST Counter Float **without pop**, then test comparison C0; less/unordered exits after that store|
|26BE..26C8|Duplicate retained sum, FDIV fresh Rate, call10413652→IAT1059A34C MSVCRT `_ftol`, retain EAX32|
|26CB..26D8|Signed DWORD FILD(EAX), multiply fresh Rate, FSUBR from original retained sum, second Float Counter store, discard original sum|
|26DA..26E4|Only if EAX!=0 read packed bTimerLoop(+28 bit1000); only if clear raw-write Rate positive0|
|26EB..2704|Always checked selected Timer lookup and ProcessEvent once, even EAX0; no post-callback timer write/loop|

The comparison happens **before** the first Counter store. Both Counter stores
are unconditional when reached, including unchanged bits. There are four fresh
Rate reads: entry, FCOM, FDIV, FMUL. Counter is read only once. `_ftol` is not
appFloor, nearest rounding or a timer callback loop. Its low signed32 result
word, not full EDX:EAX, supplies FILD and the nonzero/loop condition. Zero or
negative supplied result words retain their literal native behavior. A runtime
provider may explicitly refuse unrepresented exceptional/wider CRT conversion;
that is portable refusal policy, not original validation.

Inactive NaN Rate skips everything lazily. An active unordered sum first stores
its Counter result then exits through C0 without conversion/loop/callback. Active
infinity or conversion-domain exceptions must not be converted to invented
period counts. Finite ordinary arithmetic is PC64/nearest-even with Float spills
at exactly the two Counter stores.

`quest_actor_timer_phase.h` is the narrow native provider coordinator. Its
`convertPeriods(Real)->int32_t` service owns that separate conversion authority.
Every reached provider requires the caller's shared work service; failures
propagate without local rollback. `quest_actor_timer_phase_test.cpp` contains
source-order/lazy/mutation/signed-word/failure-prefix/mock-root-undo controls and
an independent own GNU x87 finite signed32-domain arithmetic oracle. No build or
test result is claimed by the helper's author.

The older `AdvanceTimer` correctly supplies useful arithmetic/journal patterns,
but eagerly reads Counter/loop, suppresses unchanged stores and gates begun/
deletion before generic CallEvent. Do not reuse it unmodified as this literal
native block. Callback suppression must leave already completed timer stores.

## Lifespan and physics boundary

After timer, source `103A270B..2757` freshly reads Actor.LifeSpan
(FloatProperty/1/flags1). Signed zero and unordered NaN skip its arithmetic.
Otherwise subtract delta, FST Float while retaining the PC64 subtraction, and
compare that retained result against Double0.0001 at1042AC40. Reached <= or
unordered result invokes checked Expired then real DestroyActor(force0), without
rechecking callback-mutated lifespan/deletion, and returns1 before the tail.
Existing `ActorTickTailDetail::SubtractLifeSpan` captures the finite spill edge;
unsupported active nonfinite policy must remain explicit.

Normal nonexpired source then rereads Physics; only if nonzero does it reread
Role. Role!=3 calls the actual source physics virtual+8C. ScriptedPawn's slot
imports APawn.performPhysics10302860→103C9A70, **not** Actor103C9770. Physics2
dispatches AActor.physFalling10302FEF→103D0A50, whose actual-Pawn preprocessor
103CC450 runs before zone handling. Existing crate falling explicitly refuses
that Pawn arm. APawn rotation, MoveTimer/avgPhysicsTime and PendingTouch epilogue
also remain distinct native work; a physics refusal must not publish the prefix.

## Short retained-Pawn tail and exact fields

The native network head `103A278D` occurs before the captured-Pawn null return.
Read actual XLevel+88, then pointee+38 only if nonnull, then fresh Role4 only if
both are nonnull. Its real PlayerPawn/view-copy arm is separate unimplemented
network work. Current native `RuntimeObject.nativeStandaloneLevelServices`
explicitly owns absence of NetDriver/DemoRecDriver; it is initialized only for
the actual successfully linked native ULevel in the loader, not from reflected
NetMode or absence of a tag. `ZoneLevel` authenticates current-generation Level
identity. Reuse that capability for the genuine null branch; a missing capability
is not permission to fabricate null drivers.

At103A288A/288D, null captured Pawn returns1. Otherwise:

1. Read Pawn.bIsPlayer; only if true read fresh source Role>=3, then
   Pawn.bViewTarget. True calls checked UpdateEyeHeight(Float delta); false
   raw-copies source Rotation Pitch/Yaw/Roll into captured Pawn.ViewRotation.
2. Read captured Pawn.Weapon. If nonnull, call the **actual XLevel** virtual+8C
   FarMoveActor with Weapon, raw by-value fresh source Location, test0/noCheck0.
   Ignore its Boolean. This Level+8C is not Actor performPhysics+8C.
3. Read fresh source Role; only Role4 tests tickType==2. Current type0 returns1
   without sight/pain/speech/tactics. An unimplemented reached type2 tail must
   refuse rather than pretend success.

| Exact declaring field | Native layout identity | Actual type / dim / flags / subtype |
| --- | --- | --- |
| Engine.Pawn.bIsPlayer | +318 bit2 | BoolProperty /1/0x20/none |
| Engine.Pawn.bViewTarget | +31C bit1 | BoolProperty /1/0/none |
| Engine.Pawn.Weapon | +3D0 | ObjectProperty /1/0x10020/Engine.Weapon |
| Engine.Pawn.ViewRotation | +3DC | StructProperty /1/0x20/Core.Object.Rotator |
| Engine.Actor.Rotation | +120 | StructProperty /1/0x23/Core.Object.Rotator |
| Engine.Actor.Location | +114 | StructProperty /1/0x23/Core.Object.Vector |
| Engine.PlayerPawn.Player | +5D4 | ObjectProperty /1/0x2/Engine.Player |

All have baseField0, actual listed declaring Outer and exact Core Property
metaclass. The independently proven Core Bool linker/Pawn base318 maps the
initial child chain: bBehindView bit1, bIsPlayer bit2, through bAdvancedTactics
bit80000000, then bViewTarget at31C bit1 and bCanGlide bit2, before SightCounter.
Thus the eye-height flag is **bViewTarget**, not a guessed bUpdateEyeHeight.
Weapon and ViewRotation derive from the full linked Pawn layout, not same-name
renderer state. The native Player mapping is additionally evidenced by original
receiver-local GetPlayerPawn's +5D4 read and actual PlayerPawn.Player export69;
Pawn's independently registered/linked final size is5D4. Fresh raw Engine.u
Children/Next traversal finds actual PlayerPawn Class export5, RF040F0004,
base1(Pawn), Children3301; its **first property** is export69 Player,
Outer5/dimension1/flags2/base0/next1729. Original ObjectProperty size/alignment4
therefore assigns this first own field at5D4 without a guessed serialized offset.

`OriginalAnimationField`, `RetailPredicatePointerField`, canonical Rotator
schema helpers, `MoveLocation` and `FarMoveLevelActor(level,weapon,location,0,0,
execution)` are reusable narrow binding patterns. ViewRotation raw stores must
retain signed32 channel bits, without angle normalization/clock reconstruction.
Use actual qualified declaration storage and the same root write journal.

The Player pointer is read only after actual APlayerPawn cast succeeds, not
from bIsPlayer. Authenticate PlayerPawn.Player against Engine.Player. If
`nativePlayerBinding` is true, read **raw nativePlayer**; public Read can mask a
revoked client binding and is not the original null test. Otherwise use the
actual typed Object field. Return an opaque uintptr_t identity, not a truncated
PE32 word or a guessed viewport/player. Real positive player work stays explicit.

## Next actual script boundary, not an assumed success

The newly enabled ordinary Tick must capture actual selected StateNode/Tick
identity and live probe. ScriptedPawn.Standing.Tick is a real27-byte override
that increments animTimer[1] then Global Tick; the class Tick is560 bytes and
includes GetPlayerPawn720, conditionally LastRendered723, standing/conversation/
enemy/beam/carcass work and UpdatePoison. LastRendered723 is now bound with
authenticated authored declaration and numeric-native/lazy-identity call
forms; this does not prove that a conditional723 call ran in the observed
first-Training path. Native2105 is actual ScriptedPawn.IsValidEnemy
(Pawn input flags80, optional Bool flags90, Bool return580), not an unknown
arithmetic opcode. CycleActors1002 and AICanSee705 are other real conditional
dependencies. The first **actually reached** missing service must be observed
with complete caller-root rollback; do not preemptively skip that script or
implement unrelated maps/AI paths merely because its bytecode mentions them.
