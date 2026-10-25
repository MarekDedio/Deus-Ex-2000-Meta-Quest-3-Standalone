# Original MovingBrush production integration

First-Training scope,2026-10-25. Read alongside
`ORIGINAL-MOVING-BRUSH-ACTIVE-CONTRACT.md` and
`ORIGINAL-MOVING-BRUSH-PHYSICS-PREREQUISITES.md` for the independently audited
owned Engine/Core native operations. No original executable or DLL is loaded.

## Implementation boundaries

`quest_actor_moving_brush.h` implements the entire Physics9 outer dispatch:
interleaved raw Location-to-OldLocation stores, one genuine inner call, and
fresh asymmetric PC64/Float displacement/reciprocal-derived Velocity stores.
It does not supply the inner, run rotation/PendingTouch, or schedule a world.

`quest_actor_mover_interpolation.h` implements the original inner gates,
captured local key, retained alpha/remainder/glide arithmetic, native wrapped
rotation and `_ftol` conversion, every position Float spill, actual MoveActor
packet, successful alpha store and selected InterpolateEnd/reentry sequence.
It does not contain direct pose writes or a presentation-door substitute.
Repeated original movement is bounded by its caller's existing work budget,
not an invented retry count. Reached unsupported exceptional arithmetic refuses.

`portable_actor_moving_brush.inl` supplies real native fields, typed exact
declarations, MoveLevelActor and selected InterpolateEnd in the existing host
Execution. Key8 uses explicit native adjacent-field aliases, not an invented
ninth portable array element: KeyRot[0] raw INT words feed KeyPos8 as Float;
BasePos raw Float words feed KeyRot8 as INT. Narrow raw Vector reads preserve
inactive IEEE words without relaxing ordinary VM/checkpoint validation.
Every native scalar/word store participates in the existing outer write and
object journals. Rotation and PendingTouch remain in the original coordinator
after a normal Physics9 return. Other missing active physics modes refuse.

The frame diagnostic records MovingBrush actor/mode/key/class/flag from
already reached provider reads; it does not add native queries or infer the
state from defaults. These detached observations survive whole-root rollback.

## Verification scope

The outer helper tests use an explicitly mocked inner and independently check
hardware x87 arithmetic. They cannot prove actual mover interpolation.
The inner helper tests use callback/movement providers and an independent x87
oracle. They cannot prove production collision or selected-script integration.
The generated production Level fixture runs the actual adapter and journal;
its Engine/Cylinder authority is deliberately absent, so active controls must
reach that real missing service rather than report stubbed movement success.

Actual first-Training startup/update probes and crate/ownership/animation
regressions provide separate integration evidence. Complete Level/Pawn/world
updates, published controller-driven crate destruction, collectible loot,
automatic gravity and headset acceptance are not proved by these helpers.
This backend increment alone does not produce a new APK.

Build/test results are recorded below only after the corresponding jobs finish.

- Android native `deusex_quest` build passed, including both native libraries.
  This is not APK packaging or installation.
- Outer helper:31,057 checks pass; independent hardware-x87 corpus30,180
  packets,19,993 admitted and2,787 asymmetric-spill witnesses. Inner is mocked.
- Inner helper:226,020 checks pass, including5,000 independent x87 random
  packets and the Float-round-to1 retained-alpha discriminators. Movement and
  selected-event providers are test fixtures, not production Level collision.
- Generated actual Level adapter controls log1,617 cumulative checks/43
  refusals, then pass the malformed OldLocation declaration control. Active
  linear/glide keys0/8/255 all reach the genuine absent Engine/Cylinder service;
  the idle cast no longer incorrectly requires collision preparation.
- Full desktop and strict ARM64 syntax checks pass. Final shared CTest:
  244 registered,205 executed,39 explicit owned-data skips,zero failures,
  64.80s. Separate owned crate415/5, deferred429/21 and animation588/24 pass.
- Actual Training update observation repeats twice with28 slots/27 admitted
  calls,4 real Physics9 dispatches,205505 instructions/15651 writes and full
  enclosing undo. Last reached mover is DeusExMover32, actual Mover class,
  key0 and interpolation flag false. Moves/events are0; this does NOT prove
  active production movement success. The next gate is exact Engine.Trigger
  native admission, audited separately, not skipped or newly admitted here.
- Final two-crowbar-Fire repeat2 retains228837 instructions/26047 writes,
  48 observation rows,2 cycles/returned notifies,4 real traces,2 crate changes
  and9 births including surviving Lockpick and wood fragments. Both repeats
  reach the known FExec refusal with complete root undo; this is not published
  controller gameplay or loot collection.
- All45 checked owned script/config/Training-map files retain exact SHA256,
  lengths and UTC modification timestamps. All build/test jobs are terminal.
  No Quest was detected; no APK was packaged or installed.

Next active integration check should initiate the original authored
Engine.Mover.InterpolateTo function inside the existing startup root before
the unfiltered dynamic phase. It must not set fake pose/alpha values, suppress
other actors, use a detached public per-actor transaction, or relabel expected
later refusal as completed startup. Trigger admission/world/Pawn work and
published live crate/usable-loot/gravity acceptance remain unfinished.
