# First Training: original damage in the genuine initialized world

Scope: ONLY `Maps/00_Training.dx`, specifically the original
`CrateBreakableMedGeneral1` and `CrateBreakableMedGeneral2`. This is the next
direct crate checkpoint, not a reason to complete unrelated HUD features or
other maps first. It is a rollback-only desktop diagnostic, not a playable
headset build or a claim that weapon input/collection/gravity is finished.

## Actual original context, not a begun-play fixture

The optional `--damage-training-crates` command uses the existing first-map
startup root. The original GameInfo starts, original actor initialization and
state selection run, and original Login returns an actual JCDentonMale. Before
SetPlayer/Possess, four commands apply two6-point hits to each original crate.
No Game, begun/startup, state, relevance or instigator field is forced. The
instigator is exclusively that returned Pawn. Normal possession continues
after successful commands, with its later genuine failure and full rollback
unchanged. No client/viewport is removed to bypass the original HUD path.

The native adapter authenticates `Engine.Actor.TakeDamage`: script event
flags0x800, native0, no return, five required scalar parameters in declared
order: Int Damage; Pawn EventInstigator; canonical Vector HitLocation and
Momentum; Name DamageType. The state-aware selected implementation has the
same packet, but legitimately flags0x2, not Event0x800. Dispatch uses the SAME
execution.CallEvent, not a direct Active override, nested execution, manual
Destroyed callback or the presentation-only Pawn float-health accumulator.

## Expected original effects

Both crates have integer HitPoints10, contents Lockpick, numThings1 and no
minimum threshold. Their real active override should produce10->4, then4->-2,
original fragment births, original Containers.Destroyed and native deletion.
Fragment expectations must use post-PreBeginPlay live dimensions, not the
dormant authored collision-height24 (original PreBeginPlay scales it by0.75).

Containers.Destroyed's non-None contents branch assigns RemoteRole1, calls
SetPhysics2, enables world collision, sets VRand()*50 velocity and calls
GotoState('Pickup','dropped'). Engine.Inventory.BecomePickup only replaces
RemoteRole with2 if Physics!=2. The expected surviving Lockpick therefore has
Physics2, RemoteRole1 and selected state `DeusEx.DeusExPickup.Pickup`. The
inherited running state is `Engine.Inventory.Pickup`, label dropped at authored
bytecode offset36; selected/running paths are not interchangeable. The current
birth snapshot records selection, not proof of the running-label PC. No tick
or state slice is invented by this diagnostic.

## Bounds and failure evidence

An empty command list leaves normal startup unchanged. Diagnostics admit at
most eight commands and exactly one real Login viewport. Detached observations
retain a512KiB root-owned budget; transient packet/dispatch scratch is64KiB.
Each event observes only newly appended actual Actor exports, bounded to64.
All share the existing instruction/write/native-work/RNG/undo ledgers. Typed
after-values are evidence only when `captured=true`; no further callbacks or
snapshot work run after a failed event. Optional `--damage-write-limit` is an
explicit shared-root limit, not a fresh budget per hit.

The existing probe verifies complete save/state/revision/native clock/hash/
RNG/client/player/lookup/Actor/GC rollback and all41 owned source fingerprints.
Exit2 remains an observed unsupported/refused startup boundary, not passing
startup acceptance. Surviving loot, if observed and asserted here, still does
not prove collection, physical falling/stacking or paired/equipped VR input.

From the project root after building `portable_training_startup_probe`, run:

```powershell
& .\artifacts\prerequisites-20261009\build\portable_training_startup_probe.exe --owned-source-root 'D:\Steam\steamapps\common\Deus Ex' --repeat 2 --native-work 1048576 --damage-training-crates
& .\artifacts\prerequisites-20261009\build\portable_training_startup_probe.exe --owned-source-root 'D:\Steam\steamapps\common\Deus Ex' --repeat 2 --native-work 1048576 --damage-training-crates --damage-write-limit 14978
```

Both intentionally return exit2 at their observed unsupported/refused boundary.
Require the explicit rollback/source verification and examine every captured
damage row, not just the exit code. Omit both damage switches to observe normal
startup without diagnostic hits; it must not contain damage observations.

## Verification

Initial strict ARM64 checks and both desktop/Android native builds pass. The
actual first-map discovery run repeats twice: original first hit completes
10->4, with two root writes and no births. The fatal second hit reaches genuine
WoodFragment0 creation/initial state and refuses `Core.Object.RotRand` native320
at `Engine.Projectile.RandSpin` PC6. Each attempt executes202083 instructions/
14693 writes; native work463089cold/462547warm of1048576 is not exhausted. Both
fully roll back and all41 original source fingerprints remain unchanged. There
is no completed fatal event or surviving Lockpick evidence in that run.

After implementing the audited original native320 (see ORIGINAL-ROT-RAND.md),
both repeated actual Training attempts complete all four damage events. Each
crate goes10->4->-2, is genuinely removed/deleted and produces six surviving
WoodFragments plus exactly one surviving Lockpick. The lockpicks have the
asserted original Physics2, RemoteRole1 and selected DeusExPickup.Pickup state;
their observed Base and Owner areNone. Captures and all birth identities/
locations/velocities match between repeats. This is surviving original contents
in the genuine pre-pairing world, NOT collection or physical settling.

The unchanged subsequent possession reaches DamageHUDDisplay.InitWindow PC39's
fixed Name DamageType[3] gap in both attempts:221246 instructions/24407 writes,
native work502464cold/501840warm of1048576. Full rollback and all41 unchanged
sources pass. With shared write limit14978, both runs instead refuse late in
the second fatal callback at Containers.Destroyed PC918,205756 instructions,
native work473605cold/472981warm. Three earlier events/captures are complete;
the fourth is explicitly uncaptured and is not after-state evidence. Full
save/RNG/hash/player/GC restoration still passes for these partial deaths.

Strict ARM64 runtime/CLI/random-test checks and Android native link pass. The
expanded asset-free production random tests pass86188 checks/67 rejection
controls, including raw/reflected/virtual/nested/native-default/no-result,
mixed stream, malformed metadata, partial-Bool refusal, save continuation and
full Rotator/member/OUT/write-budget rollback. Two test-fixture mistakes were
corrected: a full native Bool zero-fill is4 bytes, not1 (the latter remains an
explicit rejection control); revision comparisons must use the post-save-load
baseline because successful loads themselves publish a revision. No production
VM policy was relaxed to make these controls pass.

The first full shared build was intentionally stopped to fix that baseline;
its latest15 completed COFF objects were checked before incremental resumption.
That resumption exposed an old aggregate caller's missing-field warning for
the new optional command vector; an explicit empty default preserves existing
callers without adding commands. The failed build was stopped, corrected, and
the final coherent full build passed. Shared CTest passes174 executed/206
registered,32 explicit owned-data skips, zero failures in64.23s. The separately
invoked isolated original crate regression passes347 checks/two genuine
contents-refusal controls (its Game=None fixture intentionally deletes loot;
it is not the surviving-contents proof above). The new detached SetPhysics
coordinator passes3112/21 in the shared suite, but is NOT bound to native3970.
The separate owned original-player/HUD6758 controls also pass, retaining the
same explicit fixed-Name-array HUD refusal and complete pairing/checkpoint undo.
No APK was packaged.

Evidence: `artifacts/training-damage-focused-build-20261023.log`,
`training-damage-android-20261023.log`, and
`training-damage-startup-discovery-20261023.log`. An attempted CLI relink during
the discovery executable's run correctly failed with a Windows output-file
lock; retry only after the original process is terminal. That build failure
is not a native compilation failure or a new diagnostic run.

Updated evidence: `training-damage-rotrand-build-20261023.log`,
`training-damage-rotrand-android-20261023.log`,
`training-damage-rotrand-startup-20261023.log`,
`training-damage-late-refusal-20261023.log`, and
`training-damage-rotrand-tests-final-20261023.log`. Final build/regression
evidence: `training-damage-full-build-final-20261023.log`,
`training-damage-android-final-20261023.log`,
`training-damage-full-tests-final-20261023.log`, and
`training-damage-crate-regression-final-20261023.log`, plus
`training-damage-player-hud-final-20261023.log`. Strict success was
silent; there is no claimed strict stdout log.

## Next actual crate work

Live headset weapon damage still uses a separate Pawn-only presentation-health
path; it has not been replaced by this diagnostic or certified. The next small
original physics prerequisite is native3970's full SetPhysics behavior (same
mode early return, real support/detach, post-callback Physics reread and zeroing
Velocity/Acceleration for final modes0/5). Current byte-only adapter does not
provide that contract or simulate falling. The whole first Training map still
needs falling/stack support, support loss after destruction, collectible loot
and paired/equipped input; completing unrelated HUD features is not being used
as a prerequisite to this direct investigation.
