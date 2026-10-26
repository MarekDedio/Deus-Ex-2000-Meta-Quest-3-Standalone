# First-Training APawn physicsRotation: next verified boundary

Read-only static audit, 2026-10-26. Immediate scope is the first
`Maps/00_Training.dx`, not other campaign maps. This document implements no
rotation code and proves no completed Training frame, crate/loot/gravity
milestone or headset acceptance. After the current falling work the real
Training diagnostic reaches `Original Pawn physicsRotation is not implemented`;
that reached-call observation belongs to the root's separate verification.

## Original source authority

GNU objdump statically read the original PE32 instructions/export/import
tables. The port's own package inspector read original `.u` metadata. No
commercial EXE/DLL was executed, loaded as a module, linked, patched or rewritten.
Rechecked owned `D:/Steam/steamapps/common/Deus Ex/System` SHA256:

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

Export-name index2069 is `?physicsRotation@APawn@@QAEXMVFVector@@@Z`.
Thunk10302626 enters the complete body103C9F30..103CA11B. It is different
from AActor.physicsRotation103CA1E0. APawn.performPhysics directly calls this
Pawn method with whole Float delta and captured entry Velocity by value.
The complete Pawn rotation body **never reads the supplied Velocity argument**;
it only consumes Float delta at frame+8. Do not invent velocity-based turning.

All addresses/offsets below describe the original PE32, not portable object
layouts. The existing original Actor linked-layout proof and exact qualified
field schemas are recorded in ORIGINAL-PHYSICS-ROTATION.md.

## Exact Pawn rotation schedule

1. Raw-capture source Rotation.Pitch, Yaw, Roll in that order at103C9F5A..
   103C9F72. Pitch/Yaw become the initial proposal; the captured Roll word is
   not subsequently used. It is nevertheless an original required read.
2. Walk actual source Class/Super against native PlayerPawn Class1058C160.
   A match goes directly to the final comparison at103CA058, skipping all
   conversions, flag edits, DesiredRotation, Physics and rate reads below.
   The resulting PlayerPawn proposal is still `{capturedPitch,capturedYaw,0}`.
3. For a non-PlayerPawn, unconditionally FILD signed RotationRate.Yaw (+200),
   FMUL memory Float whole delta, then call `_ftol` thunk10413652 (IAT1059A34C).
   Retain its low32 EAX. This conversion happens even if current raw Yaw
   already equals Desired.Yaw. A reached exceptional/wider unauthenticated
   CRT conversion must refuse, not clamp or silently produce0.
4. At103C9FAA load the actual packed native flag word+1EC, clear bit20
   (bFixedRotationDir) and set bit40 (bRotateToDesired), preserving all other
   bits from that captured word. Store unconditionally at103C9FB6, even if
   both Boolean values were already correct. These edits are not restored.
5. Read fresh DesiredRotation.Yaw once. If it differs as raw signed32 from
   captured Yaw, call `AActor.fixedTurn(capturedYaw,thatSameDesiredYaw,deltaYaw)`
   through10303B34 ->103C9DC0. Do not insert Actor's delta0 skip or second
   Desired argument read. Even a delta0 call returns low16(current), potentially
   stripping raw upper bits; fixedTurn itself then skips its flag read.
6. Read fresh DesiredRotation.Pitch for a raw inequality gate against captured
   Pitch. If equal, skip all remaining pitch conversion/mode/rate work.
7. If unequal, FLD whole Float delta and FMUL Float32768 at1042A040 (bytes
   `00000047`), then call the same `_ftol` and retain low32 EAX. This is
   **not** RotationRate.Pitch times delta. The existing exact finite
   `OriginalActorRotationDelta(32768,delta)` has the equivalent integer/power2
   product and authenticated signed64/low32 conversion domain.
8. Read source Physics Byte once at103C9FEB. Physics1 selects target Pitch0.
   Physics2 additionally walks actual Pawn Class1058C660 and selects0 only
   for a real Pawn. All other cases read DesiredRotation.Pitch a second time
   for the fixedTurn target. No bIsPawn shortcut replaces that native cast.
9. Call fixedTurn(capturedPitch,target,deltaPitch) even if deltaPitch0. Then
   apply the original **signed** clamp, reading fresh RotationRate.Pitch only
   on the selected branch:

   - If proposedPitch <32768, read signed ratePitch. Keep proposedPitch if
     proposedPitch <=ratePitch; otherwise replace it with that raw ratePitch.
   - Otherwise read signed ratePitch and compute wrap32(65536-ratePitch).
     Keep proposedPitch if it is >=that bound; otherwise replace it with the
     bound. Do not saturate, take abs, mask the clamp result or normalize rates.

10. Final proposal is `{proposedPitch,proposedYaw,0}`. At103CA058 compare
    against fresh source Rotation.Pitch, then Yaw, then literal Roll0, with
    short circuit on first difference. If all equal, return without Level or
    collision queries. A PlayerPawn with nonzero Roll can still reach movement.
11. If changed, construct the native initial hit packet (Next/Actor/Primitive,
    Location/Normal zero, Time Float1, Item-1) and call actual XLevel virtual+88
    MoveActor(source,zero translation,proposal,hit,0,0,0,0). The original INT
    result and hit packet are not consumed. The method returns immediately.

There is no selected EndedRotation callback/probe, completion comparison,
pending-delete gate, post-MoveActor work, direct Rotation assignment, Roll
rate calculation or restoration of rotation flags anywhere in this method.
Actor's otherwise reusable outer rotation coordinator is therefore not an
equivalent substitute.

## Exact metadata and reusable services

The owned package inspector freshly confirms these scalar Actor fields:

| Qualified field | Type / dimension / flags / subtype |
| --- | --- |
| Engine.Actor.Rotation | Struct /1/23 /actual Core.Object.Rotator |
| Engine.Actor.DesiredRotation | Struct /1/21 /actual Core.Object.Rotator |
| Engine.Actor.RotationRate | Struct /1/21 /actual Core.Object.Rotator |
| Engine.Actor.bFixedRotationDir | Bool /1/21 /none |
| Engine.Actor.bRotateToDesired | Bool /1/21 /none |

Flags in this table are hexadecimal. BaseField0, genuine Core Property
metaclass/declaring owner and canonical signed32 Pitch/Yaw/Roll members must
also be authenticated. Existing Physics is Actor EPhysics Byte/flags23.
The packed flag positions1EC bit20/40 and Rotator offsets120/1FC/208 are
already independently linked to these names in ORIGINAL-PHYSICS-ROTATION.md.

`OriginalActorFixedTurn` in quest_actor_rotation.h is the exact shared integer
algorithm used by this Pawn method, including wrapping32 add/subtract/negate,
asymmetric half-turn ties and low16 result. Its fresh relevant flag read is
required only when delta is nonzero. The nonplayer flag edits above precede
all fixedTurn calls, so they affect the genuine turn direction. Reuse this
fixedTurn service and exact conversion, not `RotateOriginalActor`.

Existing `PhysicsRotatorField`/`PhysicsRotatorChannel`, qualified Bool metadata
helpers, bounded actual PlayerPawn/Pawn ancestry walks and `MoveLevelActor`
provide the narrow binding building blocks. Mutation must remain in the
enclosing Level Execution/write journal. Preserve other packed flags when
editing the two rotation Bools; there are no actor callbacks between these
native stores, but work/write failure prefixes must still unwind the whole root.

## Meaningful next implementation slice

Implement a separate Pawn rotation coordinator and runtime binding for the
whole bounded body above, including the genuine PlayerPawn negative/positive
cast branches, not a special hardcoded Jaime angle or successful empty service.
Keep MoveActor's actual Engine/Cylinder authority lazy until movement is
selected. Test raw capture/fresh reads, unconditional flag stores, delta0
fixedTurn calls, Pitch's32768 conversion and signed clamps, PlayerPawn Roll0,
the ignored old-Velocity argument, no EndedRotation and false movement results.
Use independent own-code x87 conversion/integer oracles plus actual generated
metadata/script/checkpoint/work/write rollback controls. Then repeat actual
first Training with door and crowbar/crate regressions to identify the next
real update or landing/Walking dependency. This audit alone runs no new build
or test and does not claim any of those acceptance conditions.
