# First Training: released crate and genuine dropped loot falling

This follows the explicit original dry physFalling integration recorded in
ORIGINAL-ACTOR-FALLING.md. Only the first actual Training map is integrated.
It does not implement the world scheduler or publish incomplete player startup.

## Support loss sequence

`portable_training_startup_probe --owned-source-root OWNED_GAME_ROOT
--native-work 1048576 --fall-after-lower-break` requests:

1. Genuine original timesteps to settle the lower crate on its actual floor.
2. Genuine timesteps to settle the upper crate on the lower.
3. Two original six-point damage events against **only** the lower crate.
4. Genuine timesteps on the surviving upper crate after original DropThings
   detaches it and switches its actual Physics to Falling.

The diagnostic never forces Physics/Base/flags, does not move the upper during
destruction, and does not assume the subsequent hit must be world geometry.
New original fragments and the dropped Lockpick remain present. The observer
requires actual initial support, completed lower death, unchanged upper pose/
velocity/acceleration/OldLocation/stored zone, then real post-death downward
motion and a real non-destroyed collision support through selected Landed.
Collection/sweep combinations are rejected for this separate diagnostic mode.

## Dropped inventory sequence

`portable_training_startup_probe --owned-source-root OWNED_GAME_ROOT
--native-work 1048576 --fall-training-loot` requests original damage against
both crates, then explicit original timesteps against the two actual surviving
Lockpick births. It can be combined with `--collect-training-loot-via-player`
to test original inventory-grid pickup and merging **after** their settling.
This mode is separate from initial crate falling and explicit delta sweeps.

The runtime admits Inventory only when it is an actual runtime birth matched
to a completed surviving original damage-birth observation in this same root.
The actual stored zone/velocity/acceleration and native flags remain the
authority. There is no new grant, force, floor snap or manual event dispatch.
Rows now identify actual class and whether the native Decoration landing
counter exists. Inventory snapshots do not read a nonexistent numLandings.

Dropped Lockpicks select original **state** event
`DeusEx.DeusExPickup.Pickup.Landed`, not a class-level placeholder. Its original
base `Engine.Inventory.Pickup.Landed` clears Pitch, sets Rotation and calls
selected PlayLandingSound. The DeusEx state override can break an actually
breakable pickup at speed>400; sound/AI noise depend on actual LandSound and
Velocity.Z<=-200. These are original callbacks, not cosmetic optional effects.
Positive birth Velocity.Z does not itself prove an apex resplit was selected;
the actual substeps and counters are observed rather than assumed.

The observer ties first pose/velocity/Base to the genuine birth snapshot,
preserves complete vector/support/zone/clock/write continuity, permits actual
3D upward and downward motion, and requires the selected pickup landing plus
real support and stopped Velocity/Acceleration before calling the item settled.
Original player pickup then uses actual settled Physics/Base, not a presumed
still-falling profile. Direct Frob is allowed to retain original prior support
when original SetPhysics0 is an equal-mode early return; no normalization write
is added to make a test pass.

## Completion versus diagnostic prefixes

Once actual execution reaches original DeusExPlayer.Possess, every requested
damage/fall/collection group must be fully captured and complete. A later HUD
refusal cannot conceal a skipped or still-falling diagnostic command. Earlier
genuine native-work/write/callback failure prefixes remain observations, never
successful startup or physics acceptance. Repeated rows compare all detached
fields; whole-root rollback and all41 original source fingerprints are checked.
Normal startup still requests none of these commands and retains its real gap.

## Original-map results

Both repeat2 runs retain identical detached motion/landing/clock/write-prefix
observations, reach the genuine later HUD array boundary, fully roll back and
leave all41 original sources unchanged. They are not accepted startup.

The released upper falls fromZ-55.3848038 through-60.1348038 and-74.3848038,
then lands at-77.487175 on **the actual newly born Lockpick0**, not the world.
Hit.Time0.290280402/Normal.Z1 reaches selected DeusExDecoration.Landed and
real SetPhysics0/BaseLockpick0. All seven rows/twelve native sweeps complete.
Writes14956->14958->15123 destroy only the lower, then15123->15170->15217->15258
advance the upper. This proves real support-loss motion/contact, **NOT** final
world-floor settling: this diagnostic has not advanced the airborne Lockpick.
The full result reaches220942 instructions/24688 writes and501979 cold/
501355 warm native work, then the original later HUD refusal.
Do not suppress that collision, move the item away, make it nonblocking or
report the stack settled on the world. Interleaved original actor physics and
based-child carry remain necessary follow-up evidence.

The independent two-loot run completes nine rows/sixteen native sweeps. Both
actual Lockpicks move with original3D velocities and land on LevelInfo0 via
`DeusExPickup.Pickup.Landed`, with no Decoration counter/fit work. Lockpick0
settles at(994.406433,-272.968445,-123.764198), hit.Time0.634739757; Lockpick1
settles at(1047.87146,-276.450775,-122.382515), hit.Time0.400524557. Both normals
haveZ1, Physics0/Baseworld and zero V/A. No apex resplit is selected; this is
an observation, not a claim that positive launch velocity requires one.

After those real landings, original ParseRightClick transfers the first into
actual6x5 inventory grid cell(0,0), BaseJC and Pickup animation, then merges
the second into NumCopies2 while preserving the grid/full player clock.
Fall writes15068->15115->15162->15191->15238->15285->15332->15379->15426->15455,
pickup15455->15518 and merge15518->15558 are shared-root actual prefixes.
The complete combined run reaches224069 instructions/24990 writes and the
same HUD refusal. Native work is516471 cold/515844 warm of1048576.

Evidence in ignored artifacts/: training-released-upper-repeat-20261023.log
and training-released-loot-player-repeat-20261023.log. Strict ARM64 syntax,
focused host compile/link and Android native compile/link pass. Late-write
controls below pass; full regression results follow their terminal gates.

Repeated cutoff15248 interrupts selected DeusExDecoration.BaseChange PC0/
opcode20 during the released upper's final support operation. Six prior rows
are captured; the seventh is entered but not returned/captured. Both runs fully
undo the completed initial stack, lower death/births, released motion and partial
support. Metrics:205408 instructions/15248 writes,473067 cold/472443 warm work.

Repeated cutoff15445 interrupts Engine.Inventory.Pickup.Landed PC0/opcode15
during the second item's floor landing. Eight prior rows, including the first
item's completed floor support, are captured; the ninth is not. Both runs undo
all prior destruction, births, settled support and partial landing; neither
starts a collection command. Metrics:206818 instructions/15445 writes,
479786 cold/479162 warm native work. All41 sources remain unchanged on both
cutoffs. Default after-values in uncaptured rows are not observations.
See training-released-upper-late-20261023.log and
training-released-loot-late-20261023.log.

Three incompatible-mode combinations are rejected before data loading.
Normal no-command startup remains at218253 instructions/24192 writes/
491158 native work and the same original HUD boundary. It fully undoes its
root/all41 originals; no falling/damage/collection rows are manufactured.
The separate direct-Frob-after-settling run also completes actual transfer and
merging. As predicted by original equal-mode SetPhysics, its first item's
Base **remains LevelInfo0** rather than JC (the latter is player-ParseRightClick
behavior), and its player-only grid position remains(-1,-1). This distinction
is preserved, not normalized. Direct writes15455->15480->15510; all-root undo/
all41 unchanged sources pass. See training-released-loot-direct-20261023.log
and training-released-falling-normal-20261023.log.

Final coherent full desktop build passes. Shared CTest executes176 of208
registered tests, with32 explicit owned-data skips, zero failures in42.22s.
Fresh owned crate regression passes347 checks/two genuine contents refusals;
this dormant fixture does not prove surviving loot, which is instead observed
in the real initialized startup-root runs above. Logs:
training-released-falling-full-build-20261023.log,
training-released-falling-full-tests-20261023.log and
training-released-falling-crate-final-20261023.log.
The fresh post-build owned SetPhysics regression also passes172882 checks/
53 explicit refusals, including19 partial-write cuts and full real Doctor
animation-clock preservation: training-released-falling-physics-final-20261023.log.

The next required original bounce/rotation/PendingTouch contracts and real
based-child carry constraints are recorded in ORIGINAL-ACTOR-PHYSICS-EPILOGUE.md.
That document is static audit evidence only, not an implementation or a reason
to claim the interleaved scene already works.

## Remaining gameplay work

Explicit same-root steps do not constitute automatic world dispatch, rotation/
PendingTouch, all reached bounce/water/wall/Pawn paths, live equipped-weapon
controller input, committed player/startup, paired sound, rendering or headset
acceptance. No new APK is implied by a native-only build. The whole first map
and eventual full campaign remain unfinished.
