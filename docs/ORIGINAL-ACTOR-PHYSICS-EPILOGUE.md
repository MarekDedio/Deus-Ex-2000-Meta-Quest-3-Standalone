# Original Actor bounce and performPhysics epilogue audit

Read-only owned GOTY static audit, 2026-10-23. This document records prerequisites
for advancing the complete first-Training crate/loot/fragment cohort. It adds no
runtime implementation and does not establish a world scheduler, complete
fragment simulation, committed startup or headset acceptance.

The owned installation is `D:\Steam\steamapps\common\Deus Ex`. Rechecked SHA256:

- `System/Engine.dll`:
  `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
- `System/Engine.u`:
  `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.

No commercial DLL/EXE was loaded or executed. `objdump` reads static bytes;
our `script_bytecode_inspect` reads original package tables and script. Native
addresses and reflected metadata are joint authority, not a guessed C++ ABI or
the pinned engine's different physics implementation. Read alongside
ORIGINAL-ACTOR-FALLING.md, ORIGINAL-TRAINING-CRATE-FALLING.md,
ORIGINAL-DECORATION-LANDING.md and TRAINING-RELEASED-FALLING.md.

## Why the complete cohort matters

The actual released-upper diagnostic first settles the lower crate on world
support and the upper on the lower, then destroys only the lower. Original
DropThings detaches the upper into Falling. The surviving upper subsequently
lands on the genuine newly born **airborne Lockpick0**, not on world geometry.
The exact observed contact is Z-77.487175, Time0.290280402, Normal.Z1 and
BaseLockpick0. That diagnostic has not advanced the Lockpick during those upper
steps. The independent loot diagnostic does advance both actual Lockpicks and
observes their real world landings and later original inventory pickup/merging.
Neither result alone proves final settling of the interleaved whole scene.

The six original WoodFragments and Lockpick born from the lower crate must not
be removed, made nonblocking, moved away or replaced by fabricated loot to make
the upper's contact easier. A subsequent complete phase must advance the
actual cohort with original actor ordering/eligibility and real support/child
carry, while preserving synchronous callbacks, state transitions and births.
Calling the current explicit dry timestep repeatedly on one selected actor is
not that phase. The owned first map remains the integration target; this audit
does not require other-map sweeps.

## Original physFalling bounce branch

The active branch is Engine.dll103D1D81..103D1DF5. It follows the first real
Level.MoveActor, the immediate bDeleteMe check, collision Time<1, and the
existing collision-with-PlayerPawn/Decoration counter handling. It precedes
the ordinary Normal.Z>Double0.7 landing test, so a bouncing actor must not be
sent directly to Landed merely because its hit normal points upward.

1. Read the actual packed actor word at+1EC and test bit0x10, independently
   authenticated as `Engine.Actor.bBounce`; metadata is scalar Core.BoolProperty,
   flags0x21, owned by Engine.Actor. The post-light property chain and original
   Actor copy103244BF..10324569 establish masks1/2/4/8/10/20/40/80/100 at+1EC.
   Original Core Bool.Link packing corroborates that sequence.
2. Copy supplied hit Normal.X/Y/Z raw into a by-value packet, then copy the
   original hit Actor retained in EDX into packet+12
   (103D1D8C..103D1DB2). This is `HitWall(HitNormal, hitActor)`, not BumpWall,
   Touch or an ordinary landing packet.
3. Name lookup loads10583714 at103D1D98. Original export index943 is
   `ENGINE_HitWall`, RVA283714, hence exactly that address. Resolve the selected
   function on the falling actor and call real ProcessEvent synchronously in
   the same root at103D1DB8..103D1DCC. Do not directly call Actor's empty body or
   substitute a hand-coded reflection formula.
4. Reread the **live Physics Byte only** at103D1DCF..103D1DD4. Zero goes to the
   early return103D20D3. It does not execute final Acceleration restoration.
   A nonzero mode other than2 is not rejected or normalized at this site. There
   is no fresh bDeleteMe early return between this callback and this mode test.
5. The native signed local bounce counter starts at0 at103D0E1A and is retained
   across all substeps of this one physFalling call. If it is<2, restore unused
   time as `StoreFloat((Double1-hit.Time)*dt + remaining)`
   (103D1DDA..103D1DEF). Both arithmetic operations retain original PC64
   intermediates until the final Float store. Do not first spill `1-hit.Time`
   or its product, and do not restore unused time on later bounces.
6. Increment that local counter at103D1DF2, then join common tail103D22CE.
   This is not an unbounded immediate bounce loop or a separate transaction.

At the common tail, reread actual bBounce. If it is still true AND the substep
collision flag is true, jump to the remaining-time/iteration loop103D0E49,
skipping ordinary displacement-derived Velocity correction and terminal clamp.
Otherwise test live bJustTeleported; true also jumps to the loop. If a callback
clears bBounce while leaving Physics nonzero, ordinary tail correction can run.
Do not reuse the pre-callback bBounce word. The eight-iteration bound remains;
ordinary loop completion restores the captured original Acceleration.X/Y/Z,
whereas Physics0/deletion/landing early returns do not. A caller implementing
performPhysics must still run its own epilogue after physFalling returns.

Original Engine.Actor.HitWall declaration is native0, flags0x800, two required
scalar parameters: Core.StructProperty HitNormal flags0x80 referencing actual
Core.Object.Vector, then Core.ObjectProperty HitWall flags0x80 constrained to
Engine.Actor. There is no return value. Its empty two-byte body is only the
base declaration. Valid selected script overrides need not retain FUNC_Event.

### The actual fragment script is significant

There is no `Engine.Fragment.Flying.HitWall` or `DeusEx.WoodFragment.HitWall`
export. `Engine.Fragment.HitWall` is native0, flags0x102,430 logical bytes.
`DeusEx.DeusExFragment.HitWall` overrides it with native0, flags0x102,529 logical
bytes. It has the same required Vector/Actor packet plus local Sound
(Object<Engine.Sound>), Volume Float and Radius Float, all flags0/dimension1.
The actual state-aware selector must determine which inherited function is
eligible; the current genuine WoodFragment births are in Engine.Fragment.Flying.

The DeusEx override preserves several effects a native bounce stub would lose:

- If actual lastHitLoc equals Location, set Velocity to zero; otherwise apply
  the authored elasticity/reflection expression through its original natives.
- Recompute Projectile.speed. On the first sufficiently slow hit, clear
  bFirstHit, enable bRotateToDesired, clear bFixedRotationDir and set desired
  Pitch/Roll0 and randomized Yaw. Scale RotationRate.Yaw/Roll/Pitch by0.75.
- When `(speed<60 && HitNormal.Z>Float0.7) || speed==0`, call original
  SetPhysics0 with the **supplied hit Actor**. Reread Physics; only an actual0
  result clears bBounce and enters Dying. Preserve support callbacks that
  change the requested mode.
- Continue authored random volume/sound/pitch selection and PlaySound264,
  optional AISendEvent713, then save actual Location into lastHitLoc. These
  operations occur even after the script has requested stopped/Dying state;
  a native early exit before the selected script returns would be different.

All random draws, sound/AI effects, support changes and state transitions must
use the existing same-root original services. A legitimate unsupported effect
must propagate its failure and roll back, not be omitted to claim a bounce.

## performPhysics dispatch and unconditional common epilogue

Original exported `AActor.performPhysics` thunk10303242 reaches
103C9770..103C996F. It reads Physics and dispatches through table103C99A4.
Physics2 calls physFalling(deltaSeconds,0) at103C97C4..103C97CC, then jumps to
common epilogue103C98ED. Modes outside the dispatch's2..11 range also join that
epilogue; the selected mode is not a license to skip rotation/touches.

There is no post-dispatch bDeleteMe guard in this body. In particular, a normal
return from a physFalling early-return branch still precedes rotation and
PendingTouch processing. A world scheduler may have its own separate lifetime
or eligibility rules, which this audit does not establish. Do not invent one
inside performPhysics.

### Native field identities

The complete original Actor Children/Next metadata is retained in the ignored
artifact collision-hash-save-audit-20261018/engine-actor-properties.log.
The following consecutive fields match original Actor copy1032456F..103245ED:

| Native position | Actual declaring field and schema |
| --- | --- |
| 1F0 | Actor.DodgeDir ByteProperty flags0, scalar enum |
| 1F4 | Actor.Mass FloatProperty flags1, scalar |
| 1F8 | Actor.Buoyancy FloatProperty flags1, scalar |
| 1FC/200/204 | Actor.RotationRate StructProperty flags0x21, scalar Core.Object.Rotator |
| 208/20C/210 | Actor.DesiredRotation StructProperty flags0x21, scalar Core.Object.Rotator |
| 214 | Actor.PhysAlpha FloatProperty flags0, scalar |
| 218 | Actor.PhysRate FloatProperty flags0, scalar |
| 21C | Actor.PendingTouch ObjectProperty flags0, scalar Engine.Actor constraint |

RotationRate copies at10324599..103245B2; DesiredRotation follows at
103245B5..103245CC; PhysAlpha/PhysRate copy at103245CF..103245E1;
PendingTouch raw pointer copies at103245E7..103245ED. Thus the epilogue's
operands are not inferred only from pinned member names or an unpersisted
serialized UProperty Offset. Future adapters must additionally authenticate
the exact declaring field/metaclass/array dimension/type at runtime, including
the canonical Rotator member schema and Actor object constraint.

### Rotation gate and partial algorithm audit

At103C98ED..103C990A test the **low16 bits**, in order, of RotationRate.Pitch,
then Yaw, then Roll. Any nonzero result calls physicsRotation with the original
whole deltaSeconds; all zero skips it. Nonzero upper16 bits alone do not select
rotation. The original export `AActor.physicsRotation` index2068/thunk103010C8
reaches body103CA1E0. This is a native method, not a script event to synthesize.

Confirmed initial ordering inside that body:

1. Capture bRotateToDesired (+1EC bit0x40); if false, test
   bFixedRotationDir (+1EC bit0x20). Both false returns at103CA44A.
2. If initially bRotateToDesired, compare current Rotation with actual
   DesiredRotation as raw INT Pitch, Yaw, Roll; exact equality in all three
   returns before time/rate conversions (103CA222..103CA254).
3. Read RotationRate.Roll and multiply its signed-INT value by deltaSeconds;
   call conversion helper10413652. Then do Yaw, then Pitch in that order
   (103CA25A..103CA29C). Current Rotation is also captured while these values
   are constructed. This is not three direct authored-property additions.
4. Conditional axis updates use original fixedTurn thunk10303B34. The later
   proposed rotation is applied through genuine Level.MoveActor with zero
   translation and real collision/event/support semantics, not a direct
   Rotation store.

This is a **partial** rotation audit. The complete conversion helper, fixedTurn
edge/wrap behavior, all dynamic flag rereads, rotational sweep/event outcomes
and completion callback need their own bounded original audit and tests before
binding the method. No rate-times-delta approximation or pinned substitute is
authorized by the evidence above. Reached unimplemented rotation must refuse
and roll back, not silently leave rotating fragments frozen.

### PendingTouch/PostTouch: exactly one node, after callback reread

The native body103C9917..103C995C is fully bounded:

1. Read `retained = source.PendingTouch`. None skips this operation.
2. Resolve selected **PostTouch on retained**, not on source. Packet is one
   Actor argument `Other=source`. The name load105835D0 at103C9921 matches
   exported ENGINE_PostTouch index968/RVA2835D0 exactly. Real ProcessEvent is
  103C992F..103C9940.
3. After that callback, **reread** `node = source.PendingTouch`
   (103C9943), then read `next = node.PendingTouch` (103C9949).
4. Write source.PendingTouch=next at103C994F, then write
   node.PendingTouch=None at103C9955. Return from the phase. There is no loop
   draining the rest of the list.

The called retained actor and the subsequently unlinked node can differ after
callback reentry. Do not clear the node before PostTouch, cache its old Next,
unlink retained unconditionally or drain multiple entries. Original code would
dereference a null freshly reread node; an explicit portable refusal for that
invalid callback result is preferable to inventing a successful empty dequeue.
Deletion is not a new early-exit rule at this site. Both final pointer writes
need the actual caller's journal plus native Actor GC/archive ownership.

Original Engine.Actor.PostTouch is native0, flags0x800, logical size2, required
Core.ObjectProperty Other flags0x80/dimension1 constrained to Engine.Actor,
with no return value. Its base script is empty, but a selected override or
reentrant callback is not optional. The exact metadata and event identity are
established independently of the pinned PendingTouch implementation.

## Next bounded implementation and verification

At this audit's initial checkpoint, native/quest_actor_falling.h refused
reached bounce collisions. The subsequent integration is recorded in
TRAINING-FRAGMENT-BOUNCE.md: it uses original HitWall, Physics, random, sound,
state and support services, but is not by itself complete performPhysics.
The complete static rotation audit is now in ORIGINAL-PHYSICS-ROTATION.md;
runtime rotation remains unimplemented. The detached performPhysics
coordinator has reentry tests, but PendingTouch still needs native ownership
and event/store binding. The
automatic ordered cohort frame phase and broader world scheduler remain
unimplemented by these explicit diagnostics.

Useful required controls include selected override packet/provenance;
callback Physics0 versus other nonzero modes; bounceCount0/1/2 unused-time
restoration; callback changes to bBounce/bJustTeleported; exact normal Float
and unused-time rounding; all late-store/callback/budget failures with
whole-root rollback; actual fragment random/audio/state effects; low16-only
rotation gating; and PendingTouch callback replacement/self-link/one-node
dequeue with complete pointer undo. An actual first-Training cohort run must
retain original births and show the upper's support following the advancing
loot, not merely repeat the earlier airborne-support contact.

These are prerequisites toward playable first-Training destruction, loot and
gravity. They do not justify publishing partial startup, suppressing the later
real Possess/HUD failure, claiming a finished APK or claiming campaign-wide
physics acceptance.
