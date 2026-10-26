# Original APawn performPhysics outer coordinator

Scope: the actual first-Training NPC virtual physics call. This implements
the original outer coordinator, not completed Pawn falling, Walking, rotation,
playable Training, surviving crate loot or stacked-crate gravity.

## Read-only original authority

The existing port package inspector and GNU objdump statically read owned
`D:/Steam/steamapps/common/Deus Ex/System` files. No commercial EXE/DLL was
executed, loaded as a module, linked, patched or rewritten. Source identity:

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |

Actual ScriptedPawn virtual+8C imports APawn.performPhysics, thunk10302860
to complete body103C9A70..103C9CA8. This is not Actor.performPhysics103C9770.
Addresses and offsets below describe the original PE32 only, not portable
C++ object layouts.

## Exact outer schedule

1. Raw-copy entry Velocity.X/Y/Z at103C9A9A..103C9AB0, before reading Physics.
2. Read Physics Byte and select the actual table103C9CDC. Entries1..10 are
   `103C9AC9,103C9ADB,103C9AFF,103C9AED,103C9B9D,103C9B9D,103C9B9D,103C9B20,
   103C9B9D,103C9B11`. Thus1 Walking,2 Falling,3 Swimming,4 Flying,8 Pathing,
   10 Spider call movement;0/5/6/7/9/11 and all other Byte values do not.
   Walking/Falling/Swimming/Flying/Spider pass whole Float delta and literal
   iterations0. Pathing additionally copies Location to OldLocation and
   derives Velocity from the post-call displacement; a provider must implement
   that whole block rather than only its inner call10301A5A.
3. After any normal movement return, read fresh Physics. Mode10 skips rotation.
   Otherwise perform the actual PlayerPawn Class/Super cast1058C160. A match
   selects rotation without reading Rotators. Nonplayers compare raw signed
   Rotation/DesiredRotation Pitch, Yaw, Roll, with paired reads in that order
   and short circuit at the first mismatch. Only if all three match read
   signed RotationRate.Roll and select rotation if strictly positive.
4. Selected rotation calls APawn.physicsRotation10302626 with whole delta and
   the raw entry Velocity by value. This is not Actor's low16-any-rate gate
   or Actor.physicsRotation. Do not normalize full-width Rotators at this gate.
5. Load Pawn.MoveTimer, subtract whole delta in retained masked x87 PC64.
   Before the Float store, capture source.PendingTouch at103C9C29 and test it.
   Store MoveTimer unconditionally, even if the bits are unchanged.
6. Load fresh Pawn.AvgPhysicsTime, multiply by memory Double0.8. Load whole
   Float delta, multiply by memory Double0.2, add the retained products, then
   store Float unconditionally. Original constants are at10431418 and1042B8E8
   (bytes9A9999999999E93F and9A9999999999C93F). No host float-only rewrite,
   finite-input policy, clamp or fused multiply-add is substituted.
7. If captured PendingTouch is nonnull, FindFunctionChecked(PostTouch) on that
   retained receiver, then native ProcessEvent with Actor Other=source.
   Timer stores precede this callback. After it returns, reread source's live
   PendingTouch. A null fresh pointer legitimately skips advancement at
   103C9C7E..103C9C80, unlike the Actor coordinator's null behavior. Otherwise
   retain that fresh node, read its next pointer, write source=next and then
   clear that node's next pointer. Do not unlink the stale pre-callback receiver,
   loop-drain the list, add a delete gate or lose source/node alias semantics.

## Qualified field and event identities

The complete owned Core property linker/layout method and independent
Actor318/Pawn5D4 native registration anchors are recorded in
ORIGINAL-SCRIPTEDPAWN-TICK-PREREQUISITES.md. A fresh read-only Children/Next
walk from the actual Pawn class confirms the initial34 Bool fields, retaining
the previous property across non-property children. They pack318/31C;
SightCounter320, PainTime324, SpeechTime328 follow. AvgPhysicsTime is export
ref4052, FloatProperty/dim1/flags2/base0/no subtype, next154, at32C. FootRegion
ref154 and HeadRegion ref380 are each the actual Actor.PointRegion Struct
(three DWORDs) at330/33C; MoveTimer ref1030 follows at348, scalar Float/flags0,
base0/no subtype. Runtime uses exact qualified `Engine.Pawn.AvgPhysicsTime`
and `Engine.Pawn.MoveTimer`, never offset-casts portable storage or reads a
similarly named renderer field.

Velocity is Actor.Vector/flags21, Rotation is Rotator23, DesiredRotation and
RotationRate are Rotator21, PendingTouch is Actor Object/flags0. All schemas
are authenticated through existing typed metadata helpers. Native PendingTouch
owns its real pointer edge and journals edge changes alongside script storage.

Actual Engine.Actor.PostTouch is native0/flags800, two-byte Return/Nothing
body, one `Other` ObjectProperty/dim1/flags80 constrained to actual Actor.
Selected overrides may have locals/different function flags but must retain
that exact Actor input packet. The common native callback adapter now handles
this Actor packet in addition to Float/Bool/none, preserving original checked
lookup, selected identity and native ProcessEvent gates before CallFunction.

## Implementation and verification boundaries

`quest_pawn_perform_physics.h` owns the outer schedule. Every reached provider
requires the caller's work authority; provider exceptions propagate with no
private rollback. `portable_pawn_perform_physics.inl` authenticates the actual
native virtual class, binds typed fields/raw entry Velocity, existing same-root
falling and native PendingTouch services. The existing bounded PlayerPawn cast
is shared with the Pawn Tick suffix without changing its original read order.
Active unfinished movement modes and Pawn rotation explicitly refuse.

The pure test checks all256 Byte dispatch values, fresh movement mutations,
raw full-width rotation/rate gates, captured Velocity/touch identity, ordered
timer writes, callback-null/replacement/source aliases, missing services and
every work/write failure prefix with explicit mock-caller rollback. Its
independent oracle executes only its own GNU x87 instructions under masked
PC64/nearest-even, comparing timer/average Float bits across exceptional and
deterministic raw-bit inputs. ARM64 syntax checks do not pretend to execute
this desktop oracle.

The focused helper run passes717,838 checks/46 refusal controls, including
35,512 independent oracle cases (14-cubed exceptional/boundary inputs and
32,768 deterministic raw-bit triples). This is pure outer-coordinator evidence,
not proof that the runtime adapter or first Training completed physics.

Generated production tests use genuine Physics5 (no original movement call),
not a fake falling success provider, to exercise the typed outer timers and a
selected decoded PostTouch script. That script observes the updated timers,
receives real Actor Other and clears the actual native pending edge. Checkpoint
continuation and late callback/write/work rollback pass: the ScriptedPawn
section reaches2,620 cumulative checks/44 refusal controls; the complete
generated driver exits0 after7,492/140 and its final malformed-schema control.
The explicit Physics1 and required Pawn-rotation controls still fail at their
genuine unimplemented services, with full enclosing-world undo.
The real first Training Physics2 path must be observed independently: this
outer method does not remove the remaining falling preprocessing/air-control,
landing/Walking or Pawn rotation gaps. See
ORIGINAL-TRAINING-PAWN-PHYSICS-GAP.md for that next real work.

Final current verification: full desktop build, Android native build and
strict ARM64 helper/runtime syntax all pass. CTest253 registered/214 executed/
39 explicit owned-data skips/zero failures62.79s passes; separate original
first-Training crate415/5, deferred429/21 and animation588/24 pass. Actual
Training repeat2 enters APawn's genuine Physics2 dispatch and reaches the
explicit `Original Pawn physFalling preprocessing is not implemented` boundary
at206,922 instructions/15,797 writes, with full undo. Paired doors repeat
207,258/15,865 and retain two56-unit moves/two completion callbacks before
that same boundary; separate two-Fire repeat2 retains228,837/26,047 and its
real crate/loot/debris observations before full undo at later FExec. None of
these expected-error checks is a committed startup, live controller melee,
surviving usable loot, automatic stacked-crate gravity or headset acceptance.
