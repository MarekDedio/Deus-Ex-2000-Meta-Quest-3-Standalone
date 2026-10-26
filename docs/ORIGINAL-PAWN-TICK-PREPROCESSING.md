# Original AActor Tick Pawn context and separate Pawn continuations

Owned GOTY static audit and bounded context-helper foundation, 2026-10-26.
Scope remains first Training. This is not native ScriptedPawn admission,
APawn physics implementation, complete NPC Tick, or headset acceptance.
No original commercial DLL/EXE was loaded or executed. External objdump and
the existing package/bytecode inspector read immutable owned inputs only.

Read alongside ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md,
ORIGINAL-NONPAWN-TICK-TAIL.md, ORIGINAL-ACTOR-BLEND-PHASE.md and
ORIGINAL-SCRIPTEDPAWN-TICK-PREREQUISITES.md. Their original Engine.dll SHA256
is `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`;
Engine.u is `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
Addresses below are preferred-image static evidence, never portable layouts.

## Position within actual Tick

Actual AActor.Tick is export-name/EAT1667, thunk10303DAA to103A1AA0.
Its existing native InStasis/owner/parity prefix remains mandatory. Stasis
can return1 without a parity store; owner deferral can enqueue source and
return0 before its parity/context/animation work. AScriptedPawn's distinct
wrapper must preserve the base return rather than force it to1.

After normal entry,103A1B8B..103A1BAD stores source.bTicked from current
native Level parity while preserving all other packed bits. The new context
is derived from that same retained packed word, not from the superclass name
or a guessed class boolean. No callback occurs between that store and the
following bIsPawn test in the original.

## Exact context103A1BA8..103A1BF0

The local Pawn pointer starts null (103A1B97). Then:

1. Test actual Actor.bIsPawn bit800000 at103A1BA8. If clear, retain null
   Pawn context and false simulated-Pawn flag. Do not read source Class,
   traverse ancestry or read Role on this path.
2. If set, read actual UObject.Class pointer+24 at103A1BB6. Traverse each
   UClass.Super pointer+28 until matching the exact native APawn UClass
   identity1058C660, or encountering null. The comparison is actual metadata
   identity, not a display name, inherited RF_Native bit or a ClassFlags guess.
3. Failed cast leaves the captured Pawn pointer null, even though bIsPawn
   was set. The emitted null-source arm also leaves it null; the portable
   caller already requires a real nonnull retained Actor.
4. Successful cast stores source in the local Pawn pointer at103A1BDA.
   Read actual source Role Byte at103A1BDF. Capture the separate Boolean
   `simulatedPawn = (Role == 2)` at103A1BE2..103A1BED.
5. Enter shared MAIN animation setup103A1BF0, initializing whole-delta
   Remaining and its one shared MAIN/BLEND iteration counter.

Native identity1058C660 is independently corroborated by Engine's exact
PrivateStaticClass@APawn name/EAT1377 RVA28C660. APlayerPawn is a DIFFERENT
native UClass1058C160 (name/EAT1378 RVA28C160). Actual Engine.u Pawn
reference1 has RF_Native04000000 and base Actor reference2; those facts do
not merge APawn and AActor virtual physics implementations.

Consequences that must not be replaced by flag/class OR:

| Actual packet | Captured Pawn | Suppress MAIN AnimEnd |
| --- | --- | --- |
|bIsPawn clear, even actual Pawn ancestry|null|false|
|bIsPawn set, non-Pawn/null Class chain|null|false|
|bIsPawn set, successful native APawn cast, Role2|source|true|
|bIsPawn set, successful native APawn cast, any other Role Byte|source|false|

Both locals are fixed for this Tick invocation. Animation notifies/AnimEnd
can mutate Role, flags, state or world membership; these captured locals do
not silently refresh afterward. MAIN suppresses its AnimEnd at the original
loop/end/tween branches for the captured simulatedPawn flag; mesh notifies
remain eligible. A fresh Role comparison later in a suffix is independent.

## Shared animation exit does not imply a non-Pawn suffix

MAIN ends at103A1FA1. The source rechecks fresh main sequence/frame/rate
or tween, shares Remaining and counter with all four BLEND slots, and loops
through the same main gate after BLEND. The inactive/exhausted main path
eventually reaches103A1FD2. Only there does the captured simulatedPawn flag
select either the specialized branch below or the ordinary role dispatch
at103A23CE. Calling the non-Pawn helper merely because animations returned
would lose the simulated-Pawn branch.

### Captured simulated-Pawn movement103A1FDB..103A21B7

This path is selected by the ENTRY Role2 Boolean, not a fresh Role read.
It does not start by calling APawn.performPhysics.

- Read captured Pawn packed word+318. If bit2 is clear, bit8000 is set,
  or current Actor.Region.Zone+388 bit1 is set, skip the optional gravity
  adjustment and join103A2136. The bit2 association is the actual bIsPlayer
  behavior; other packed-field declaring names are not authenticated here.
- Otherwise seed a fresh hit output and invoke native XLevel virtual+C0
  at103A20B2: source Actor, actual current Location as start, an end with
  the same X/Y and Float(Location.Z-8), extent(CollisionRadius,
  CollisionRadius,CollisionHeight), flags6. This is an actual native trace,
  not a visual-floor or direct-position substitute. Exact extent/endpoint
  operand/spill order and existing LineCheck contract must be preserved.
- If hit.Time compares equal1 (also unordered under the literal C3 test),
  or its Normal.Z is below Double0.7, apply the original gravity adjustment.
  A non-equal Time and Normal.Z>=0.7 skip gravity. The compared Double0.7
  bytes are at1042ADC0; Z-offset Float8 at1042B148.
- Gravity uses fresh Region.Zone gravity at native+31C, multiplied by
  Float0.5 (10426D74) and whole delta. X/Y half-gravity products are Float
  spills; Z's half-gravity remains PC64, then each dt product is added to
  fresh Velocity and Float-stored X/Y/Z. No generic force integration or
  arbitrary timestep clamp may replace that literal order.
- At103A2136 form wholeDelta*Velocity Z/Y/X in PC64, then Float-spill X/Y/Z
  into actual translation arguments. Call original AActor.moveSmooth through
  thunk10301532 at103A2172; Engine name/EAT2051 identifies that native
  method. Its Boolean is ignored here. Real movement/collision callbacks
  share the caller root.
- Then probe actual Tick FName336 at103A2185. If enabled, select/call real
  Tick with the original whole Float delta. Jump directly to103A278D.

This bypasses the ordinary ProcessState, Actor timer, LifeSpan and
performPhysics suffix. It does not bypass the later retained-Pawn tail.
It must not be approximated by hard-coding an idle Pawn or borrowing falling
physics from a crate. Native fields whose declaration/packed association is
not yet authenticated remain explicit prerequisites, not guessed providers.

## Ordinary role dispatch with captured Pawn context

At103A23CE read RemoteRole first, then Role. The actual branches are:

- RemoteRole3: only fresh Role4 runs ProcessState and Actor timer; all such
  paths then join103A278D, bypassing normal LifeSpan/physics. Before that
  work, a retained Pawn that actually casts to APlayerPawn can execute the
  distinct network/view-target copy block103A23E6..103A249F. A normal
  non-player Pawn does not acquire PlayerPawn fields by bIsPlayer alone.
- RemoteRole!=3, Role<2: only fresh Physics2 invokes source virtual
  performPhysics; then join103A278D. Literal tickType1 does not suppress
  this low-role path.
- RemoteRole!=3, Role>=2: a retained Pawn that actually casts to
  APlayerPawn AND has nonnull native Player enters the actual local-player
  input branch103A2535..103A2641. It includes real Player virtual+54 calls,
  selected PlayerInput and PlayerTick, then rejoins ProcessState. Its
  additional native subclass/flag and network branches need their own
  typed authority; this audit does not replace them with empty success.
- Otherwise103A2643 literal tickType1 returns1 immediately. Other types
  run selected Tick, ProcessState, timer and LifeSpan exactly as the shared
  normal suffix. After callbacks, fresh Physics!=0 and fresh Role!=3 select
  **source virtual** performPhysics, not forced AActor.performPhysics.

The APawn vtable10426994+8C contains10302860 ->103C9A70, the genuine
APawn.performPhysics. It is DIFFERENT from Actor10303242 ->103C9770.
ScriptedPawn's actual physics slot also imports APawn's method. Reaching
Pawn Physics0 on one observation cannot justify bypassing later callbacks
that select Pawn physics or admitting the AActor physics implementation.

PlayerInput/PlayerTick static FName identities are1058368C/105836B4:
Engine name/EAT957/958 give RVA28368C/2836B4. The native local-player path
does not become the ordinary selected Tick merely by matching Float packets.

## Retained-Pawn tail103A278D..103A2AB2

This tail is also required after the preceding branches, except earlier
returns such as expiry/tickType1. Entry optionally executes actual network/
PlayerPawn view-copy work according to native XLevel+88, pointed object+38,
fresh Role4, actual PlayerPawn Class, view-target and packed flags. No network
service or field-name meaning is inferred solely from those offsets here.

Then a null captured Pawn returns1 through103A1AED. A real captured Pawn:

1. If Pawn+318 bit2 and fresh source Role>=3, test Pawn+31C bit1. Set calls
   checked selected UpdateEyeHeight with whole Float delta
   (FName105836A0, Engine export-name999); clear raw-copies source Rotation
   Pitch/Yaw/Roll to Pawn+3DC. This packed bit's actual declaring identity
   still requires separate authentication, not a guessed property name.
2. Read Pawn+3D0. Nonnull invokes actual XLevel virtual+8C using that object,
   a raw by-value fresh source Location and two literal zero options at
   103A2929. This is a distinct weapon/far-movement service, not source
   performPhysics+8C; receiver identity and packet must not be conflated.
   The actual Engine.Pawn.Weapon declaration is ObjectProperty/1,
   flags10020, constrained Engine.Weapon; mapping+3D0 still needs native
   field proof when creating its portable provider.
3. Only fresh source Role4 AND tickType2 enter103A2946 onward. Other paths
   return1. This is a literal gate, not permission to infer an ELevelTick
   semantic name from another engine.
4. Float Pawn+320: negative (including original unordered C0 behavior)
   adds exact Double0.2 at1042B8E8 and spills Float, then subtracts whole
   delta and spills Float. If fresh bIsPlayer bit2 is set and Pawn+28 bit2
   is clear, call actual ShowSelf (Engine name/EAT1542 thunk10301014
   ->103C0410). If counter<0 and live probe341 is set, call real
   CheckEnemyVisible (name/EAT830 thunk103014CE ->103C0280), then write
   literal Float0.1 to Pawn+320. These are real perception operations,
   not successful no-op methods.
5. Pawn+324, if ordered >0: subtract whole delta in PC64, FST Float WITHOUT
   pop/reload, compare retained difference against Double0.001 at1042AC20.
   Strictly below (and unordered C0) clears the native Float before selected
   PainTimer. FName105836C4 is Engine export-name955.
6. Pawn+328 similarly subtracts/compares and clears before selected
   SpeechTimer; FName105836C0 is export-name985. Refresh retained Pawn
   local after callbacks as in the native code, not its script-visible class
   relation or entry-captured context.
7. If fresh Pawn+318 bit80000000 is set, call selected UpdateTactics with
   whole Float delta (FName105835C0, export-name1000). Then return1.

The owned Pawn declarations independently identify SightCounter, PainTime
and SpeechTime as FloatProperty/1/flags0, Health as IntProperty/1/flags10021,
ViewRotation as Rotator/1/flags20 and bIsPlayer as BoolProperty/1/flags20.
This audit does not pretend their serialized property offsets are portable
memory layouts or that names alone prove each native association above.

## Implemented bounded foundation and remaining evidence

`quest_actor_pawn_context.h` captures only the exact entry context. Its
providers use actual Class/Super/native UClass identities and the same work
authority as the caller; a flag-clear/null-Class path demands no unnecessary
native metadata/Role provider. Unsupported cyclic/deep ancestry explicitly
refuses within the caller root. The helper mutates no game fields, introduces
no independent transaction/undo, and does not change native Tick admission.

The companion generated provider test covers exact order, all Role bytes,
flag/Class mismatch, null/inactive/missing services, frozen capture after
mutations, bounded Class graphs, every shared-work cutoff and explicit mock
caller-owned undo of provider effects. That isolated helper proof is not
native ScriptedPawn wrapper, movement, physics, AI, full-frame or device proof.
No builds were run by this audit/helper author.

The actual first Training observations must establish each reached native
Pawn path, typed field/virtual identity and complete callback/perception/
physics services before publication. Unknown nearer native Tick/physics
overrides continue to refuse. Skipping NPCs, forcing bIsPawn false or using
the non-Pawn suffix as a blanket replacement would not implement this source.

## Production base-prefix integration in this increment

The root binds this helper inside the genuine base Actor phase, after stasis,
owner deferral and the original parity store. Providers authenticate actual
RuntimeObject Class/Super links and the exact native Engine.Pawn metadata;
they do not replace the native cast with a leaf-name test. The captured
suppressAnimEnd Boolean feeds MAIN, whose existing continuation is shared with
BLEND. No callback refreshes the captured context. A real Pawn continuation
still refuses after animation instead of entering the non-Pawn suffix; the
nearest-native ScriptedPawn/Pawn Tick/physics admission remains unchanged.

The generated production fixture supplies a real native Pawn Class target
without inventing a Pawn instance. It also exercises bIsPawn set on an actual
non-Pawn class; the original cast returns null and ordinary selected Tick is
therefore allowed. This replaces the older overly broad flag-or-class refusal
only where the original source actually supplies a null Pawn context. It does
not authorize a successful empty Pawn continuation.

A separate diagnostic captures the exact actor/Class/nearest-native identity
at a reached closed boundary. For ScriptedPawn only, typed authored Role,
RemoteRole, Physics, bIsPawn, bAlwaysTick, bSimulatedPawn and bStasis values are
also copied inside the same budgeted root. These are observations, not stores
or physical-offset authority. The later independent linker/native-copy audit
maps the custom wrapper's +30C bit to bSimulatedPawn; that proof is separate
from these snapshots. Detached snapshots survive rollback but cannot
publish a frame. Synthetic boundary/error/repeat/write/work/undo checks and a
new actual Training observation are required independently of the helper.

Initial direct helper verification passes1106 checks/15 refusals. The first
full desktop/Android-native/ARM64 build is in progress at this note; final
results are recorded separately. No NPC/native-Pawn success is claimed.

Final verification passes1106 helper checks/15 refusals and generated
production boundary945 cumulative/25, with the complete driver passing after
5817/121 plus its malformed-schema case. The actual non-Pawn flag-mismatch
case and earlier ordinary callbacks run through the real bound context
service. The corrected Physics snapshot charges its own work before reading.
Desktop/Android-native/strict ARM64 builds and final CTest207 executed/
246 registered/39 explicit skips/zero failures176.80s pass. Owned crate415/5,
deferred429/21 and animation588/24 regressions pass. The two-Fire diagnostic
retains228837/26047 and complete undo; all45 checked original files are
unchanged. All build/test/probe/audit jobs are terminal.

Actual Training repeat2 identifies JaimeReyes0 with Role4/RemoteRole2/Physics2,
bIsPawntrue, bAlwaysTickfalse, bSimulatedPawnfalse and bStasisfalse at the still
closed ScriptedPawn gate. Ordinary205999/15731 and paired206335/15799 retain
complete root undo and identical repeated snapshots; both panels still move
56 units and dispatch2 original completion events before that gate. The
remaining custom wrapper/base Pawn continuations/physics are not implemented
or bypassed by this increment. No new APK or device claim follows.
