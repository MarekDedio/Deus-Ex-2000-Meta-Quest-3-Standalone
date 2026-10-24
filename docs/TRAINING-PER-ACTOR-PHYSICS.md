# First Training: native per-Actor physics, rotation and pending touches

This follows the genuine crate-fragment bounce integration at0ce3c67. The
original `AActor::performPhysics` coordinator is now runtime-bound for its
implemented Falling branch, common rotation and PendingTouch epilogue. This is
not APawn physics, a world scheduler, state Tick or whole-scene interleaving.
Only the first original Training map is used for actual-map integration.

## Original rotation and event delivery

`native/quest_actor_rotation.h` implements the complete bounded original
physicsRotation/fixedTurn audit in ORIGINAL-PHYSICS-ROTATION.md. Rate*time
conversion decodes integer/Float bits, retains the exact product, truncates to
signed64 and consumes low32. It never converts the rate/product through
Float32, clamps to INT32, adds a positive-time gate or consumes exceptional
data on an inactive early-return path. Reached nonfinite/out-of-signed64
conversions explicitly refuse instead of guessing CRT-indefinite results.

The coordinator retains captured versus fresh flag/desired/current reads,
Roll/Yaw/Pitch conversion and Yaw/Pitch/Roll update ordering, original wrapping
fixedTurn behavior, and zero-translation MoveActor with four zero options.
Only real MoveActor may write the pose; its boolean result is ignored. Fresh
actual completion and live probing govern selected EndedRotation. There are
no new deletion/Physics guards, direct pose assignments, rate/flag clearing,
local undo or unconditional empty-base event shortcuts.

`native/portable_actor_perform_physics.inl` authenticates exact original Actor
Rotators/Bool fields and supplies native movement/events in the same caller
VM execution. Existing selected lifecycle-event machinery now authenticates
PostTouch and EndedRotation base packets plus actual class/state overrides
before normal ProcessEvent suppression. Inactive movement/rotation/touch
branches do not require a Level/Model/hash or inactive event declaration.
Other Actor movement blocks6/7/8/9/11 remain explicit missing-provider refusals;
they are not installed as successful empty callbacks. Other no-dispatch modes
still run the common epilogue as in the original Actor implementation.
The public ProbePortableActorNativePhysics entry deliberately invokes the
qualified AActor BASE method, not a native virtual dispatcher. A supplied
Pawn therefore does not gain APawn physics from this diagnostic. The actual
Training cohort admits only Decoration/Inventory/Fragment; a future world
scheduler must select the actual native override or explicitly refuse it.

## PendingTouch ownership and persistence

Object values in scriptValues are symbolic paths, not GC edges. A dedicated
replaceable nativePendingTouch pointer is marked by RuntimeObject::Mark and
initialized from actual declaring metadata/payload at map load and Spawn.
Metadata-only generated maps are initialized explicitly as well; absence of
the declaration is left unbound, never inferred as an owned None pointer.

Both script Let/OUT writes and native dequeue stores synchronize this pointer
with the exact reflected overlay without rebuilding unrelated animation
clocks. Each root's first write retains the receiver and old target with
GCRoots; rollback restores native pointers before retiring births and swaps
the saved overlays back. Native stores use the same VM write journal/budget.
New pointer ownership does not depend on the package rooting every export.

Checkpoint preflight stages every retained map Actor, including actors
omitted from the saved object records and legacy checkpoints. It uses the
exact saved qualified slot when present, otherwise immutable authored/frozen
defaults. Targets resolve through prepared.vm after replacement-birth staging,
not the old live index. The edge table is preadmitted/reserved under the
existing aggregate restore schema budget, including capacity excess and one
bounded temporary. Publication assigns staged pointers nonthrowingly alongside
the ordinary reset/overlay swap. Generic typed Object save encoding remains
unchanged; no process pointer is written to disk.

## Verification and actual-map diagnostic

The independent standalone rotation test passes40981754 checks/220 expected
refusals, including all low16 current angles and key wrap/half-turn/equality
offsets against an instruction/register oracle, an independent bit-column
conversion oracle, exact live/captured reads, inactive gates, move/completion
callbacks and all exercised work/write cutoffs. These are asset-free tests,
not proof of automatic scene physics or Quest performance.

The generated production test passes4358 combined coordinate/SetPhysics/
PendingTouch checks with179 refusal controls. The new cases exercise actual
selected PostTouch with Other=source, exactly one dequeue, callback replacement
and fresh next reads, node/source self-aliases, a fresh-None refusal, selected
callback failure, all23 partial write cutoffs, native-work cutoffs and a later
Actor Physics6 failure after an earlier epilogue completes. Script Let plus
late failure and omitted/explicit checkpoint pointer reset are tested through
production APIs. This fixture does not yet independently prove replacement-
birth PendingTouch identity/GC after restore; the staging code uses the real
replacement index, but that specific witness remains required.

```powershell
& 'artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe' --owned-source-root 'D:\Steam\steamapps\common\Deus Ex' --perform-training-fragments --native-work 1048576 --repeat 2 --expected-error 'Unsupported authored struct array/string member DeusEx.DamageHUDDisplay.sIconInfo.DamageType'
```

This destroys the original lower crate and explicitly updates each of its six
genuine WoodFragment births eight times with Float0.1. Unlike the separate
fall-only diagnostic, stopped Physics0 fragments continue through the common
rotation/touch epilogue. It is still actor-major, not a native world frame.

Discovery captures48 rows,63 falling sweeps,12 original HitWall callbacks,
six Physics0 world-supported Dying stops,48 rotation calls and38 actual
rotation MoveActor calls. No EndedRotation/PostTouch callback is reached in
this bounded actual-map sample; zero callbacks must not be cited as original-
map event acceptance. Original callback behavior is tested separately above.
Fragment updates run writes14914->16627; the ordinary later Possess/HUD path
then reaches its unchanged fixed-Name-array boundary at222169 VM instructions/
26057 writes, native work511417 of1048576. The full root rolls back and all41
original sources remain unchanged. This is not successful startup.

Evidence: `artifacts/training-perform-physics-discovery-20261024.log`,
`training-perform-physics-generated-20261024.log` and
`training-perform-physics-rotation-final-20261024.log` in artifacts/.
The final budget-fix focused rebuild and generated production test pass;
strict ARM64 and Android native compilation/link also pass. Both final
repeat2 original-map attempts reproduce all48 detached rows exactly, including
native rotations/rates, states, support, clocks and write prefixes. VM totals
remain222169 instructions/26057 writes, native work511417 cold/510793 warm.

Repeated cutoff16626 interrupts the last fragment's last rotational MoveActor
after six original stops and47 earlier completed epilogues. Both runs retain
47 captured rows and one incomplete48th,206683 VM instructions/16626 writes,
native work482532 cold/481908 warm. The full root undoes original death/births,
all earlier falling/bounces/support/rotations and this partial pose store.
All41 original sources remain unchanged. See
`training-perform-physics-repeat-20261024.log` and
`training-perform-physics-late-20261024.log` in artifacts/.

The coherent full desktop build passes. Shared CTest passes178 executed/
210 registered,32 explicit owned-data skips, zero failures in56.82s. The
fresh post-build owned crate regression passes347 checks/two genuine dormant
contents refusals; this does not replace the initialized-world loot witnesses.
The fresh first-Training SetPhysics regression also passes172882 checks/53
explicit refusals, including19 partial-write cuts and real full animation-clock
preservation (training-perform-physics-setphysics-final-20261024.log).

Separate original-map regressions retain the existing released-upper result
(seven rows/twelve moves, landing on the unstepped airborne Lockpick), both
loot world landings plus original player transfer/merge, and normal no-command
startup. Their respective VM instruction/write/native-work totals are
220942/24688/501997,224069/24990/516485 and218253/24192/491158. Each retains
the exact later HUD refusal, full-root undo and all41 unchanged sources.
Evidence: training-perform-physics-full-build-20261024.log,
training-perform-physics-ctest-20261024.log,
training-perform-physics-crate-final-20261024.log and the upper/loot/normal
regression logs with the same prefix/date in artifacts/.
No APK was made and no headset acceptance or automatic full-cohort settling
is claimed.

The next gameplay step remains ordered, frame-major advancement of the
surviving upper crate plus genuine loot and all debris, using real support/
child carry and the complete per-Actor path. Whole first Training, normal
controller weapon input and eventual full campaign remain the actual goals.
