# First-Training crate falling: bounded original-engine audit

Read-only static audit, 2026-10-23. This document identifies the next original
contracts needed after native3970. It does **not** claim a falling phase,
automatic settling, stacking, or on-headset gravity is implemented.
Integration scope is the two original `CrateBreakableMedGeneral` instances in
`Maps/00_Training.dx`, not a sweep of other maps.

## Evidence and reproduction

Owned installation: `D:\Steam\steamapps\common\Deus Ex`.
Original `System\Engine.dll` SHA256, rechecked during this audit:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Corroborating `System\Core.dll` SHA256:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`;
`System\Engine.u` SHA256:
`1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
No commercial DLL/EXE was loaded or executed. `objdump` reads its static bytes;
our script inspector reads original package tables and bytecode.

Read alongside `docs/ORIGINAL-SET-PHYSICS.md`, whose setter, support and raw
Vector-word contracts remain relevant. Useful PowerShell commands are:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& 'C:/ProgramData/mingw64/mingw64/bin/objdump.exe' -p 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& 'C:/ProgramData/mingw64/mingw64/bin/objdump.exe' -d -Mintel --start-address=0x103C9770 --stop-address=0x103C99D0 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& 'C:/ProgramData/mingw64/mingw64/bin/objdump.exe' -d -Mintel --start-address=0x103D0A50 --stop-address=0x103D24D0 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& 'C:/ProgramData/mingw64/mingw64/bin/objdump.exe' -d -Mintel --start-address=0x103CEF60 --stop-address=0x103D04E2 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe 'D:/Steam/steamapps/common/Deus Ex' Engine.Actor.Landed Engine.Decoration.Landed DeusEx.DeusExDecoration.Landed
```

Instruction ranges include exception diagnostics and, for performPhysics,
an inline jump table. These are not additional normal gameplay branches.
Do not start decoding in the middle of an instruction when narrowing a range.

Export-table corroboration:

- `AActor.performPhysics` thunk10303242 -> body103C9770.
- `AActor.physFalling` thunk10302FEF -> body103D0A50.
- `AActor.processLanded` thunk10301B40 -> body103CEF60.
- Native class identities used by the bodies: Pawn1058C660,
  PlayerPawn1058C160, Decoration10578D48, Carcass10573DB8,
  Inventory10576F70. These match the actual `PrivateStaticClass` exports;
  they are not guesses from subclass names.

## Confirmed outer dispatch, not a scheduler

`AActor.performPhysics` normal body is103C9770..103C996F. It reads Physics at
actor+30 and dispatches through table103C99A4. Physics2 selects103C97C4:
call `physFalling(deltaSeconds,0)` at103C97CC, then join common epilogue103C98ED.
There is no fixed0.02-second subdivision loop in this body. The pinned
`UActor::TickPhysics` loop is not authority for such a retail scheduler.

The common epilogue is important even for a falling actor:

- 103C98ED..103C9912 tests the low16 bits of the three words at
  actor+1FC/+200/+204 and conditionally calls exported actor physicsRotation
  thunk103010C8 with deltaSeconds. These match the pinned RotationRate layout,
  but a complete independent native field-offset/rotation algorithm audit is
  still outstanding.
- 103C9917..103C995C reads actor+21C, resolves a selected callback on that
  retained actor, supplies the source Actor, invokes ProcessEvent, then rereads
  source+21C, advances it to that node's+21C and clears the node link. This
  resembles PendingTouch/PostTouch; the exact reflected field/event-name
  association and its native ownership are **not yet established here**.

An explicit same-root falling diagnostic can be a prerequisite without claiming
this complete outer dispatch or a published world Tick. Do not silently omit
these epilogue branches and call the result complete performPhysics.

## Confirmed physFalling structure

Normal code is103D0A50..103D248E, with several internal early returns.
103D20D3 is one early-return epilogue, **not** the end of the whole function.
The final exception-reporting tail begins103D2491.

The following ordering is directly visible in original instructions:

1. Cast actual receiver ancestry to Pawn (103D0A7D..103D0AA9). Pawn-specific
   preprocessing calls103CC450; a crate takes the non-Pawn branch. This audit
   does not establish that preprocessing contract.
2. Test the byte at actor+D0 (103D0ACA). In the established Actor.PointRegion
   layout this is Region.ZoneNumber. Zero takes an out-of-world callback path
   at103D0B6F..103D0B9C and returns; it is not an ordinary gravity sample.
   The selected callback identity/name must be authenticated before binding.
3. Capture original Acceleration raw words at103D0B9F..103D0BC4. Pawn paths can
   temporarily change acceleration/air control. Non-Pawn jumps to103D0E0C.
4. The remaining-time loop at103D0E49 stops when remainingTime<=0 or iterations
   >=8. It increments iterations before the substep. If remainingTime>0.1,
   choose min(0.1f, remainingTime*0.5f); otherwise use all remainingTime
   (103D0E69..103D0EE2). This is not the pinned fixed0.02 subdivision.
5. Copy current Location to OldLocation (103D0EE2..103D0F01); clear packed bit
   0x100 at actor+1EC (103D0F04..103D0F0D); capture old Velocity, then subtract
   the chosen substep from remainingTime. The bit corresponds to the pinned
   bJustTeleported layout; native field registration still needs authentication.
6. Select branches using actual stored Region.Zone, its flags and actor class/
   decoration flags. Do not replace this with a hardcoded Training gravity.
   There are water/buoyancy, bobbing Decoration and Pawn-foot-water paths.
7. Ordinary nonwater/nonbobbing/non-Pawn provisional velocity is visibly built
   at103D1157..103D1255 as oldVelocity + ((Acceleration + zoneVector)*0.5f)*dt,
   with the intermediate binary32 stores shown there. The zone vector is at
   Zone+31C (consistent with ZoneGravity). Exact independent field binding and
   all floating-store rules must be proven before implementing it. Do not copy
   the pin's1.5*Acceleration/gravityScale formulation.
8. 103D13F0..103D1A79 can resplit a substep at a vertical velocity sign crossing
   (magnitude tests against5, both split durations against0.015). This cannot
   be omitted merely because a fresh falling crate initially has zero Z speed;
   loot, impacts or callbacks can provide other velocities.
9. 103D1B39..103D1C48 forms movement from current Velocity and the actual zone
   vector at Zone+328 (consistent with ZoneVelocity), then multiplies by dt.
   A native Actor flag and PlayerPawn class gate affect inclusion of zone drift;
   that flag's independent field identity remains to be established.
10. 103D1C4E..103D1C9C calls actual native Level.MoveActor with current Rotation,
    the swept delta, its hit packet and four zero options. It checks deletion
    immediately afterward (103D1CA2); Pawn transition-to-swimming is separate.
11. On collision, the live actor+1EC bit0x10 selects a bounce callback path
    (103D1D81..103D1DF5; consistent with bBounce). Otherwise hit-normal Z>0.7
    takes landing (103D1DFA..103D1EFE); smaller normals take real hit-wall,
    projected movement, second sweep and two-wall handling. The0.7 threshold
    is original, not the pin's0.7071.
12. On the ordinary continuing/no-hit path,103D22CE..103D23EF recomputes Velocity
    from actual displacement/dt minus the captured zone drift; depending on
    sign comparisons it uses `2*newVelocity-oldVelocity`. Terminal speed is
    examined/clamped afterward at103D23EF..103D2459, using Zone+33C. Do not move
    this clamp ahead of the sweep without original evidence.
13. Loop completion restores the captured Acceleration at103D245E..103D248E.
    Early landing/deletion returns use their own exit and do not execute this
    final restore. A generic RAII restore on every return would change original
    landing, including SetPhysics0's six literal zero stores.

Constants above were checked in original `.rdata`: double0.1 at1042BCB0,
float0.5 at10426D74, float0.1 at10431C54, double0.015 at10431C48,
float5 at1042EAE4, double0.7 at1042ADC0, float0.003 at10431C3C,
and float40000 at10431C40. These do not establish the remaining algorithms.

## processLanded: Decoration is not the empty base event

Normal body103CEF60..103D04DF receives normal, hitActor, remainingTime and
iterations. Before the final event it has native zone/class/fit-recovery logic.

- 103CEF92..103CF11F tests **not actor.bIsPawn**, then the stored
  Region.Zone.bBounceVelocity, then whether its ZoneVelocity differs from the
  zero Vector. If all pass, replace Velocity with ZoneVelocity+(0,0,80) and
  return before all class checks, fit traces, counter writes and Landed.
  The field-layout corroboration and actual untouched Training profile below
  establish these names; they are not assumptions that the branch is false.
- Pawn-specific fit logic is103CF124..103CF592 and is outside the immediate
  crate class path, not proof that all NPC/player landing is implemented.
- Decoration class branch103CF597..103D02AB reads its word at+324 (consistent
  with numLandings), gates at5, and performs real fit/support traces. The first
  trace runs from Location-(0,0,0.8*CollisionHeight) to
  Location-(0,0,CollisionHeight+CollisionRadius+8), flags0x37, zero extent/extra
  (103CF669..103CF7FF). If it finds an Actor it skips the following recovery
  traces. The no-hit four-trace recovery, counter and velocity stores are now
  recovered below. Never substitute a clear trace or force normal landing on
  that branch.
- Carcass-only handling at103D0159..103D0289 is not the crate class path.
  The ordinary Decoration path resets its+324 word at103D0289..103D0299.
- 103D02AB..103D0323 copies the supplied normal into a packet, performs selected
  function lookup and invokes the receiver's real ProcessEvent for Landed.
- AFTER that callback, reread live Physics (103D0326). Only if it is still2:
  Pawn selects SetPhysics1(hitActor); a non-Pawn selects
  SetPhysics0(hitActor), at103D03A9..103D03B5. Reuse the real original setter,
  support callback and journal; do not directly write Base/Physics here.
- The later acceleration normalization/remaining walking call is conditional
  on actual Pawn ancestry AND live Physics1 (103D03BA..103D048D), not crate work.

The original script selection for a DeusExDecoration is material:

- `Engine.Actor.Landed`: flags0x800, one required Core.Object.Vector HitNormal,
  empty two-byte body. It is not the selected crate implementation.
- `Engine.Decoration.Landed`: flags0x2,168-byte body, based on Actor.Landed.
- `DeusEx.DeusExDecoration.Landed`: flags0x2,647-byte body, based on
  Decoration.Landed. It has one required Vector HitNormal plus local Rotator
  Rot and Sound HitSound. A valid override need not retain FUNC_Event0x800.

The DeusEx implementation clears bFixedRotationDir, copies Rotation, zeros
Pitch/Roll and calls native299 SetRotation (PC0..50). At Velocity.Z<=-200 it
chooses a material/mass impact sound, calls original native264 PlaySound when
nonnull, then native713 AISendEvent LoudNoise (PC51..364). It clears
bWasCarried/bBobbing (PC365..380). In standalone mode, a nonexplosive actor
with Z<-500 calls its actual selected TakeDamage with DamageType Fell
(PC526..644). Thus landing can rotate, make noise, damage or destroy the crate.
Keep same-root state-aware CallEvent, before setter zeroing; never call only
the empty base body or fake successful audio publication.

## Completed non-Pawn Decoration landing recovery

This is the complete ordinary Decoration-only path within processLanded,
excluding the separately identified Carcass and Pawn implementations. It can
serve as an actual small native coordinator; it is not falling integration.

The initial zone-bounce shortcut is evaluated before any class cast:

1. Read Actor.bIsPawn (+28 bit0x00800000). True skips the shortcut.
2. Read actual stored Region.Zone (+C8), then ZoneInfo.bBounceVelocity
   (+388 bit0x200). False skips all ZoneVelocity component comparisons.
3. Compare ZoneVelocity.X, then Y, then Z with literal zero; short-circuit on
   the first unequal component (103CEFE7..103CF055). Equality in all three
   skips the shortcut.
4. Read the zone pointer/vector again, form positive-zero+X, positive-zero+Y,
   80+Z with intermediate float stores (103CF062..103CF0F4), then store
   Velocity.X/Y/Z at103CF10B/113/11C and return103D04CF. Preserve exact
   arithmetic/store semantics, including signed zero; it is not a raw copy
   of X/Y. No old Velocity value is an active numeric operand.

For an actual non-Carcass Decoration after that shortcut:

1. Read the signed INT numLandings. If it is>=5 (103CF5FD), write0 at
   103D02A1 and join selected Landed directly; perform no fit queries.
2. Otherwise initialize one real native hit packet: Actor=None, Time=1,
   Item=-1 and zero vectors (103CF60A..103CF644). Read actual height and form
   float `0.8*height`; construct start Location-(0,0,that), then end
   Location-(0,0,height+radius+8). The0.8 multiplication is double constant
   10431418 followed by binary32 store; height+radius+8 stores at103CF715.
   Use the actual Level.SingleLineCheck, source=actor, flags0x37,
   extent=(0,0,0), extra0 (103CF7F9). Its return value is ignored here.
3. Test the resulting **hit.Actor**, not the query's clear return, at103CF7FF.
   A nonnull hit skips all four recovery traces and proceeds to normal landing
   (via the false Carcass cast). Do not require it to equal the supplied floor.
4. If hit.Actor isNone, build and retain extent `(0.5*R,0.5*R,H)`:
   form `(R,R,H)*0.5f`, then double its stored Z (103CF809..103CF880).
   Four swept vertical8 traces follow, each source=actor, flags0x17, this
   retained extent, extra0, and the **same hit packet**. Each trace separately
   rereads radius/location while constructing its start and end; do not replace
   those reads with a guessed sphere or query a render collision cache.

| Order / result | XY offset of start and end | Z offsets | Actual call / retained clear result |
| --- | --- | --- | --- |
| A | (+0.5R,+0.5R) | start0, end-8 | 103CFA3D /103CFA4F |
| B | (-0.5R,+0.5R) | start0, end-8 | 103CFC09 /103CFC1B |
| C | (-0.5R,-0.5R) | start0, end-8 | 103CFDD6 /103CFDE8 |
| D | (+0.5R,-0.5R) | start0, end-8 | 103CFFA2 /103CFFB4 |

These A/B/C/D are the native query's **INT clear returns**, not comparisons
against hit.Actor or Time. The negative half-radius uses original double-0.5
at1042ABD8; positive half-radius uses double0.5 at1042AC10. Preserve the
original intermediate float stores and read order. The four results are
combined only after all four real queries:

`A+B+C+D > 1 && A+C != 0 && B+D != 0`

The literal tests are103CFFBA..103CFFEB; do not turn them into “any support,”
“any miss,” or short-circuit away later queries. If false, proceed to normal
landing. If true:

- Read and increment numLandings, store at103D0006. Original signed32 machine
  addition wraps; a portable implementation must not introduce signed-C++ UB.
- Form float direction `X=A+D-B-C`, `Y=A+B-C-D`, `Z=0.5`
  (103D000C..103D0055). These are two DWORD sums/subtractions then signed-INT
  to float conversions, not a normalized Vector.
- Form `downSpeed = StoreFloat(-1.0f*Velocity.Z)` at103D006A and
  `radiusCap = StoreFloat(30.0f+CollisionRadius)` at103D0082. Literal comparisons
  then choose30 if downSpeed<30; otherwise choose downSpeed if
  downSpeed<radiusCap, else radiusCap. Multiply the selected float by2 and
  store (103D00E0..103D00EC). This finite-input description must not be used to
  replace the original comparison's unordered behavior with a generic clamp.
- Multiply direction by that scalar with the original float stores, write
  Velocity.X, then Y, then Z at103D0140/148/151, and RETURN at103D0154.
  Do not call Landed, set Physics, change Base, normalize direction, restore
  old Velocity or reset the just-incremented counter on this recovery exit.

Normal non-Carcass Decoration landing resets numLandings to0 at103D028F,
then invokes selected Landed at103D02AB..103D0323. A mode-change or destruction
inside that script is preserved; the subsequent SetPhysics condition uses its
fresh live Physics and the originally supplied by-value hitActor.

## Independent field-layout corroboration

The names above are derived jointly from actual Engine.u field chains and
original native copy/registration/replication, not a serialized Offset that
does not exist in ordinary package UProperty records.

Decoration is actual Engine.Actor-derived native class10578D48. Its original
initializer1035FAB0..1035FB01 supplies Actor1057CDF0 and native instance size340
(size push1035FAF0). Following actual Engine.Decoration.Children/Next, ignoring
Functions as data fields, gives this complete data sequence:

| Native position | Actual field / type |
| --- | --- |
| 318 | EffectWhenDestroyed ClassProperty |
| 31C bits1/2/4/8/10 | bPushable /bOnlyTriggerable /bSplash /bBobbing /bWasCarried |
| 320 | PushSound ObjectProperty |
| 324 | numLandings IntProperty, flags2, dimension1, baseField0 |
| 328/32C/330 | contents /content2 /content3 ClassProperty |
| 334 | EndPushSound ObjectProperty |
| 338 bit1 | bPushSoundPlaying BoolProperty |
| 33C | BaseEyeHeight FloatProperty |

Actor's registered size318 and original Core field-link size/alignment rules
are established in ORIGINAL-BRUSH-NATIVE-OFFSETS. The independent exported
Decoration copy constructor, thunk10303A35 ->1033F950..1033FA4C, copies exactly
that pointer/5bit/pointer/INT/3pointer/pointer/bit/float sequence. In particular
numLandings raw DWORD is copied at1033F9E1..1033F9E7. The independently exported
assignment thunk10303094 ->1033FA90 repeats it at1033FB21..1033FB27. This proves
the counter association, rather than merely inferring a name from `>=5`.

Actual ZoneInfo native class1057B018 is registered with size474 by
1035F490..1035F4E5 (size push1035F4D4). Actual native ZoneInfo copy constructor
thunk10301D0C ->10334F30..1033539A copies ZoneGravity+31C and ZoneVelocity+328,
then ground/fluid/terminal fields+334/+338/+33C. Original ZoneInfo optimized
replication thunk10303BB1 ->103657A0 independently compares those first two
vectors and looks up actual ZoneGravity name1047B6D0 at10365843 and
ZoneVelocity name1047B6B0 at103658D0, using actual ZoneInfo UClass1057B018.
This independently associates the named vectors with the arithmetic operands.

The complete actual ZoneInfo data chain places its first ten consecutive
Bool fields after SkyZone+384, in order: bWaterZone,bFogZone,bKillZone,
bNeutralZone,bGravityZone,bPainZone,bDestructive,bNoInventory,
bMoveProjectiles,bBounceVelocity. The copy constructor independently copies
these ten ascending masks at+388, ending in mask200 at103351E9..103351F0,
then starts byte AmbientBrightness+38C. Jointly with original Core Bool.Link
10166060..10166111, this corroborates the
bBounceVelocity mask and bWaterZone bit1; it is not a trailing serialized mask.

Likewise actual Actor's initial Bool chain makes bIsPawn zero-based23, between
bForceStasis and bNetTemporary. Actual native Actor copy's corresponding
masked store is10323C38..10323C47, mask00800000 at+28. Original Core Bool.Link
and the full chain recorded in the existing destroy/movement audits
corroborate this identity. Native adapters must still authenticate these exact
declaring Core.BoolProperty fields and flags; a same-named subclass field or
same-leaf foreign metaclass is not interchangeable.

## Actual untouched first-Training profile (read-only, not acceptance)

An isolated ignored inspector `artifacts/training-landing-profile-oct23.cpp`
was compiled against the already-built libquest_portable_host.a, without any
shared build or tracked compiler-input edit. It calls InitializePortableRuntime,
LoadPortableRuntimeMap for only00_Training, ReadPortableActorScriptProperty and
GetPortableRuntimeMapActors; it executes no functions or lifecycle callbacks.
The final run exited0 and explicitly verified unchanged runtime world revision,
no published script state, and SHA256 equality before/after for its16 original
System packages plus original00_Training.dx. The detached result is
`artifacts/training-landing-profile-oct23.log`.

Both actual instances have classDeusEx.CrateBreakableMedGeneral, Physics2,
BaseNone, Velocity/Acceleration0, numLandings0, radius34,height24,mass50,
RotationRate0, PendingTouchNone, bIsPawn/bBounce/bBobbing/bWasCarried/
bFixedRotationDir/bDeleteMe false, bJustTeleported true, and actor/world
collision enabled. Their authored centers are approximately
(1022.11,-275.103,-103.9) and(1023.12817,-274.078857,-16.8998642); they do not
start with an established support relationship. Do not snap those coordinates
or treat the old authored OldLocation values as valid current displacement.

The public computed Region facade returns zone00_Training.LevelInfo0,
ZoneNumber1, leaf3505 for both. This is **not** a reading of the later native
stored Region after real startup. That actual zone (classEngine.LevelInfo)
has ZoneGravity(0,0,-950), ZoneVelocity0, terminal2500, bBounceVelocityfalse,
bWaterZonefalse,bMoveProjectilestrue, fluid friction1.20000005. Thus the cold
profile naturally avoids zone-bounce/water/bobbing/rotational/PendingTouch
branches; no flags were forced. Future actual started-world diagnostics must
sample the genuine stored fields again rather than assuming these remain true.

## Existing providers and next bounded checkpoint

The following are reusable real same-root services, not permission to invent
an unswept position change:

- `MoveLevelActor` in portable_actor_move.inl: actual sweep/hit packet, hash
  remove/add, based-child carry, encroachment, touch/bump and zone updates.
- `RetailSingleLineLevel`, `FarMoveLevelActor`, native region storage and
  selected movement events: query/placement/event components for recovery.
- `ActorSetPhysicsNative`, `RetailSetSupportBase`, `RetailFindBase`, `SetBase`:
  retain original support relationships and reentrant callback effects.
- Original264 sound,713 AI and selected TakeDamage providers: necessary for
  sufficiently fast original crate landing, not cosmetic optional effects.

The nearest bounded prerequisite is the **original selected Landed packet and
complete non-Pawn Decoration processLanded support/fit path**, with its true
queries and callbacks. Then a real explicit actor physFalling step can consume
the crate's saved Physics2/Velocity and reach that path through MoveActor.
Prove the live original crate/zone fields first; branch assumptions in this
audit are not an admitted runtime profile. A faithful unimplemented branch
must refuse and roll back, not be treated as an empty/no-op result.

First-Training acceptance must show original initialization, lower crate on
world support, upper crate on lower support, genuine lower-crate destruction,
upper crate switching to Falling and settling via swept motion, and genuine
loot falling/collection. Do this in the existing real GameInfo/startup caller
root before later Possess failure, preserving that failure and whole rollback.
It is not a license to publish incomplete startup, force begun flags or
manually invoke Landed at a guessed location. No integration of another actual
map is needed for this checkpoint.

This audit does not recover the world scheduler, physicsRotation,
PendingTouch/PostTouch ownership, every water/Pawn/apex/slide/recovery branch,
or an on-headset physics acceptance result. It adds no runtime code.
