# First-Training Pawn physics: verified remaining boundary

Read-only static audit, 2026-10-26. Integration remains the first
`Maps/00_Training.dx`, not other maps. This document adds no physics code and
proves neither a committed Training frame nor completed crate destruction,
collectible loot, automatic stacked-crate gravity or headset gameplay.
Current `portable_actor_pawn_tick.inl` calls the separate original APawn outer
coordinator in `portable_pawn_perform_physics.inl`. It now reaches the actual
falling method. Authenticated non-PlayerPawn preprocessing, actual Pawn setup
and retained horizontal cap are now bound. The first real Training frame
passes that dry falling work and reaches the separate Pawn rotation gap.
PlayerPawn preprocessing, swimming and landing/Walking remain unfinished;
the crate helper is not a replacement for these branches.

## Owned-source authority

GNU objdump read the original PE32 bytes, and the port's existing
`script_bytecode_inspect.exe` read original package metadata. No commercial
EXE/DLL was executed, loaded as a module, linked, patched or rewritten.
Rechecked owned `D:/Steam/steamapps/common/Deus Ex/System` SHA256:

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

Addresses below are the original preferred-image PE32 addresses, never
portable object offsets. Existing native virtual identity proof is in
`ORIGINAL-SCRIPTEDPAWN-TICK-PREREQUISITES.md`: ScriptedPawn physics+8C imports
`APawn.performPhysics`, thunk10302860 to body103C9A70. It is different from
`AActor.performPhysics`103C9770. Current actual first-Training repeat2 passes
the guarded selected Tick/state/native-timer/LifeSpan suffix and the separate
APawn outer dispatch and first dry falling work, then reaches
`Original Pawn physicsRotation is not implemented` at206,922 instructions/
15,829 writes, with full undo. The earlier preprocessing gate was at15,797
writes; the32 additional native stores now precede the real rotation service.
This proves the reached methods, not a committed Training frame or completed
Pawn movement/startup.

## The first falling preprocessor is not all Pawn falling work

At103D0A7D..103D0AA9, `AActor.physFalling` retains the actual APawn ancestry
cast against native Class1058C660. For a nonnull Pawn it calls103CC450 with
that retained Pawn, the whole Float delta and iterations. AL nonzero returns
from falling; AL0 continues to the real ZoneNumber test at103D0ACA.

The complete relevant negative branch inside103CC450 is much smaller than
its player-specific positive branch:

- 103CC4A6 reads the receiver Class;103CC4AD compares against **APlayerPawn**
  native Class1058C160, walking Super+28 until match or null.
- A failed PlayerPawn cast takes103CC4C9 directly to103CC9E6. That epilogue
  restores registers, executes `XOR AL,AL` and returns at103CC9F2.
- Thus genuine `JaimeReyes -> ScriptedPawn -> Pawn -> Actor` takes a proven
  false branch here, with no actor stores, trace, script callback or movement.
  This is not permission to return false for PlayerPawn or arbitrary malformed
  ancestry. The bounded metadata/native identity checks must still occur.

The positive player branch contains real traces, callbacks and movement and
remains separate work. More importantly, returning AL0 does **not** bypass
the second Pawn-specific section later in physFalling.

## Remaining actual Pawn falling sections

After ZoneNumber handling, original Acceleration.X/Y/Z is raw-captured at
103D0B9F..103D0BC4. Non-Pawn jumps to103D0E0C; a retained Pawn instead enters
103D0BD0..103D0E0C. This is now implemented in
`quest_pawn_falling_setup.h` and integrated through the captured Pawn hooks in
`quest_actor_falling.h`, not silently omitted.

The native operands include Pawn Float words+390 and+384. The owned metadata
independently confirms real `Engine.Pawn.AirControl` and `AccelRate`, both
scalar FloatProperty/flags21/base0/no subtype. The prior complete linked Pawn
layout methodology and native size5D4 anchors are documented in
`ORIGINAL-SCRIPTEDPAWN-TICK-PREREQUISITES.md`; the new binding must establish
these particular field/offset correspondences with that layout, not only
reuse matching pinned-engine member names. The fresh complete owned prefix
walk now confirms GroundSpeed378/ref302, AccelRate384/ref4030 and
AirControl390/ref3979; see ORIGINAL-PAWN-FALLING-SETUP.md.

At103D0BD9..0BE7 the first word is compared with Float0.15 (bytes9A99193E at
1042ABF4). Only the ordered-greater arm performs the predictive horizontal
movement/extent trace at103D0C63..103D0D08. That actual XLevel virtual+C0 call
uses source, start/end, cylinder extent and flags6; a nonnull hit Actor changes
the local air-control value to Float0.05 at103D0D12. Do not replace this with a
line-of-sight Boolean, hardcoded floor or always-clear result.

The shared Pawn section at103D0D1F multiplies retained control by the second
Float word; it writes Acceleration.Z raw+0 at103D0D31. Horizontal velocity
X/Y squares are added in retained PC64, spilled to Double and passed to the
actual imported sqrt. The result is retained and compared with Double10
(10430AB0); the low-speed arm forms `(Float10-speed)/wholeDelta + limit`.
It compares that limit squared with the current acceleration squared and,
when selected, uses actual SafeNormal and Float stores to bound acceleration
before joining103D0E0C. Preserve instruction comparison masks, operand reloads,
sqrt result retention, Float/Double spills and zero/nonfinite behavior; a
host float-only rewrite or pinned generic integration is not authority.

Much of the dry substep loop is already useful: original adaptive subdivision,
OldLocation stores, teleport flag, swept `MoveLevelActor`, callbacks and zone
drift. The existing drift selection already distinguishes bIsPawn,
PlayerPawn ancestry and ZoneVelocity squared>40000. Other currently refused
arms remain real: water, PlayerPawn foot water and nonbounce wall/slide.
Reaching one must fail honestly in the same caller root until implemented.

Landing is an additional mandatory gap. At103D20CE falling calls the actual
`AActor.processLanded` thunk10301B40, not a fabricated Pawn event. Its
Pawn ancestry branch103CF124..103CF184 is distinct from ordinary Decoration.
The existing `ProcessOriginalDecorationLanded` explicitly refuses Pawn.
Do not merely remove that guard or discard the supplied remaining time and
iterations. The original Pawn landing branch and any consequent Walking work
must be audited/bound before a successful NPC floor landing can be claimed.

## APawn outer dispatch and epilogue

The complete ordinary body is103C9A70..103C9CA8. The following differs from
the existing Actor outer helper:

1. Capture raw Velocity.X/Y/Z before reading Physics, at103C9A9A..9AB0.
2. Physics2 table entry103C9ADB supplies `(wholeDelta, iterations0)` to
   `AActor.physFalling`10302FEF. Other dispatch entries must not inherit Actor's
   different table. A normal falling early return still reaches this epilogue.
3. Read **fresh** Physics. Mode10 skips rotation. Otherwise actual PlayerPawn
   ancestry, any raw Rotation/DesiredRotation mismatch (Pitch, Yaw, Roll in
   that order), or signed RotationRate.Roll>0 selects the Pawn rotation call.
   It is not Actor's low16-any-RotationRate test.
4. Call actual `APawn.physicsRotation`10302626 ->103C9F30 with wholeDelta and
   the captured pre-movement Velocity by value. This is not
   `RetailPhysicsRotation`, which implements the different Actor method.
5. At103C9C20, load MoveTimer and subtract wholeDelta in PC64. **Before its
   Float store**, retain PendingTouch in EDI at103C9C29. Then store MoveTimer.
6. Store Float `avgPhysicsTime*Double0.8 + wholeDelta*Double0.2`, retaining
   the original intermediate products and addition. The actual constant
   bytes are9A9999999999E93F at10431418 and9A9999999999C93F at1042B8E8.
7. If the retained PendingTouch is nonnull, resolve checked PostTouch on that
   retained actor and ProcessEvent with Other=source. Then reread source
   PendingTouch. **If this fresh pointer is null, Pawn skips advancement**
   (103C9C7E..9C80); otherwise read that fresh node's next link, assign source
   to next and clear that node's link. The existing Actor coordinator instead
   refuses a null fresh pointer, so its complete epilogue is not interchangeable.

Owned metadata confirms MoveTimer Float/flags0 and avgPhysicsTime Float/flags2,
both dimension1/base0/no subtype. Body operands are+348 and+32C; confirm their
qualified linked-layout identities when binding. All arithmetic/writes and
PostTouch effects belong to the same Level Execution, write journal and work
budget. There is no inner commit/undo or synthesized null PendingTouch.

## Actionable implementation slice

The separate original Pawn outer coordinator is now implemented, preserving
entry Velocity capture, actual Physics2 service, rotation gate, MoveTimer/
average stores and the distinct callback-mutated-null touch path. Focused
helper and generated production callback/checkpoint/undo controls pass; see
ORIGINAL-PAWN-PERFORM-PHYSICS.md. Active missing movement/rotation services stay
explicit, so this is not finished NPC physics. Authenticated non-PlayerPawn
preprocessing, actual air-control setup, retained cap and the fresh Physics3
swimming handoff/refusal are now integrated. Next implement actual Pawn
rotation, which the first Training NPC now reaches, then the real Pawn
processLanded/Walking and other services as reached. Keep unvisited modes
closed, not no-op. See ORIGINAL-PAWN-FALLING-SETUP.md and
ORIGINAL-PAWN-ROTATION-GAP.md.

Reusable existing pieces are `NativeScalar` PC64 arithmetic/sqrt/Float spills,
raw `WriteFallingVectorWord`/`WriteFallingScalar`, `OriginalAnimationField`
qualified declarations, canonical Rotator readers, actual XLevel
`MoveLevelActor` and `LevelSingleLineCheck` providers, and
`PendingTouchField`/`WriteNativePendingTouch` ownership/journaling. Reuse
services, not the wrong Actor outer algorithm. Validate authentic repeated
first-Training frames plus paired-door and crowbar/crate/loot rollback
regressions; do not skip NPC slots to produce a passing crate-only world.
