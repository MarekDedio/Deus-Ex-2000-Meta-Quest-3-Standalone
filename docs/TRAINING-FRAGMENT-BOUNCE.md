# First Training: original crate-fragment bouncing

The dry non-Pawn `physFalling` coordinator now implements the original bounce
branch and calls the actual selected `HitWall` script in its caller's VM root.
Only `Maps/00_Training.dx` is used for original-map integration. This is not
complete `performPhysics`, interleaved world physics, live controller combat,
completed player startup or on-headset acceptance. No new APK is implied.

## Runtime behavior

`native/quest_actor_falling.h` retains the original by-value Normal/Actor
packet, fresh Physics-byte zero-only early return, unused-time refund on only
the first two bounces, and fresh bounce/teleport tests at the common tail.
There is no inline replacement reflection formula, added post-callback
deletion guard or acceleration restoration on the Physics0 early return.
The original eight-iteration bound and PC64 intermediates/Float stores remain.
Reached Pawn, water, bobbing and non-bounce wall/slide branches still refuse.

`native/portable_actor_falling.inl` supplies the authenticated selected
`HitWall` event through the existing original movement-event service. Actual
WoodFragments select `DeusEx.DeusExFragment.HitWall`, including authored
reflection, speed, first-hit rotation flags/desired rotation, rate damping,
SetPhysics/support, Dying-state, random, sound and AI-event operations. A
failure in any reached original service propagates; none is skipped to make
a fragment appear to bounce.

The startup diagnostic admits a Fragment only when it is an actual runtime
birth matched to a captured surviving damage birth in the same root. Fragment
snapshots do not read the Decoration-only numLandings counter. Observations
retain real rotation/rates, state, bounce flag, support, stored zone, clocks
and write-prefix continuity, in addition to motion. A callback's selected
identity can be captured before failure, but completed-call stats are copied
only after `physFalling` returns; an uncaptured row's hits=0 does not prove that
HitWall was never entered.

## Bounded original-map diagnostic

```powershell
& 'artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe' --owned-source-root 'D:\Steam\steamapps\common\Deus Ex' --fall-training-fragments --native-work 1048576 --repeat 2 --expected-error 'Unsupported authored struct array/string member DeusEx.DamageHUDDisplay.sIconInfo.DamageType'
```

This applies two original six-point damage calls to the lower crate only,
retains its six genuine WoodFragments and Lockpick, then advances each fragment
with at most eight explicit Float0.1 steps. Commands are actor-major, not
frame-major. The surviving upper crate and the dropped Lockpick are not
advanced by this separate debris diagnostic. No other map is loaded.

Discovery completed all six fragments: 31 captured rows, 63 swept moves,
12 selected HitWall callbacks and six Physics0 stops on LevelInfo0 in original
DeusExFragment.Dying state. The original lower death has writes14764->14914;
fragment steps run14914->16513. The subsequent ordinary Possess/HUD continuation
still reaches its genuine unsupported fixed-Name-array boundary; full-root
rollback and all41 unchanged original sources pass. This expected later error
is not successful startup or a reason to publish the partial world.

Discovery evidence: `artifacts/training-fragment-bounce-discovery-20261023.log`.
Both repeat2 runs reproduce every detached row exactly, including real state,
rotation/rates, support, clock and write prefixes. Final totals are222169 VM
instructions/25943 writes, native work509953 cold/509329 warm of1048576.
See `artifacts/training-fragment-bounce-repeat-20261023.log`.

A separate repeat2 with `--damage-write-limit 16502 --expected-error 'VM write limit'`
stops inside original `Engine.Actor.SupportActor`, PC14/opcode11, during the
sixth fragment's last HitWall/SetPhysics support operation. It retains30
captured rows/five earlier stopped fragments, with the31st row incomplete.
Both attempts reproduce206604 instructions/16502 writes, native work481031
cold/480407 warm, then fully undo the original crate death/births, five
completed fragment stops and sixth partial support callback. All41 original
sources remain unchanged. See `artifacts/training-fragment-bounce-late-20261023.log`.

## Native coordinator and focused controls

`native/quest_actor_perform_physics.h` adds the original per-Actor dispatcher
and unconditional rotation/PendingTouch ordering as a detached coordinator.
It is not yet bound into the runtime. Exact Actor dispatch is Falling2,
Projectile6, Rolling7, Pathing8, MovingBrush9 and Trailer11. Other modes skip
movement, not the common epilogue. Active missing services refuse. Pathing and
MovingBrush providers must implement their entire original wrapper block,
not merely the inner native movement call.

The coordinator tests low16-only Pitch/Yaw/Roll rate gating, whole-delta bits,
fresh callback state and exactly one selected PostTouch/dequeue with fresh
head rereads, alias/reentry and caller-owned rollback. Its standalone test
passes4223 checks/37 expected refusals. It does not implement rotation or
PendingTouch native ownership/binding by itself. See the complete static
rotation audit in `ORIGINAL-PHYSICS-ROTATION.md`.

The falling test passes59951 checks/979 refusal controls, including raw bounce
packet/reentry, nonzero Physics modes, first-two refund accounting, live flags,
callback failures and every exercised work/write cutoff. One initially added
rounding expectation was corrected after the test failed: Float0.09 multiplied
by (1-2^-25) still rounds to0x3db851ec. A genuine subtraction-spill discriminator
uses hitTime0x3e00000b and expects0x3da147ad rather than the premature-spill
0x3da147ac. The separate product-spill discriminator expects0x3e73332f rather
than0x3e73332e. Neither discrepancy was fixed by changing runtime arithmetic.

Strict ARM64 runtime/CLI syntax and Android native compilation/link pass;
final corrected fixture syntax also passes. The coherent full desktop build
passes. Shared CTest executes177 of209 registered tests, with32 explicit
owned-data skips, zero failures in52.36s. Fresh post-build original crate
regression passes347 checks/two genuine dormant contents refusals; fresh
first-Training SetPhysics passes172882/53 controls, including19 partial-write
cuts and complete real Doctor animation-clock preservation. These separately
run controls are not claimed by the skipped owned-data CTests, and the dormant
crate fixture is not the surviving-birth debris test above.

The previous released-upper and falling-loot/player-pickup diagnostics also
pass against this runtime: seven rows/twelve moves for the upper, and nine
rows/sixteen moves plus original transfer/merge for the two Lockpicks. The upper
still lands on the unadvanced airborne Lockpick in that separate sequence;
this is not floor-settled combined-scene acceptance. Normal no-command startup
remains218253 instructions/24192 writes/native work491158 at the same genuine
later HUD boundary, with no falling/damage/collection commands requested.
All41 original sources/full-root rollback pass in each original-map run.

Final ignored artifacts: `training-fragment-bounce-focused-20261023.log`,
`training-fragment-bounce-android-20261023.log`,
`training-fragment-bounce-full-build-final-20261023.log`,
`training-fragment-bounce-full-tests-20261023.log`,
`training-fragment-bounce-crate-final-20261023.log`,
`training-fragment-bounce-physics-final-20261023.log`,
`training-fragment-bounce-upper-regression-20261023.log`,
`training-fragment-bounce-loot-regression-20261023.log` and
`training-fragment-bounce-normal-regression-20261023.log` in artifacts/.
Empty strict syntax output is confirmed by terminal exit0, not by assuming an
empty Tee log exists. The first full build was intentionally stopped after the
new rounding test failed; only the corrected final build/suite is accepted.

The next gameplay integration remains the complete
first-Training crate/loot/fragment cohort with real rotation, pending touches,
actor ordering and based-child carry, followed by normal headset interaction.
