# First Training: ordered crate, loot and fragment physics

This follows b641302's runtime-bound original per-Actor physics. The new
rollback-only diagnostic updates the surviving upper crate and ALL surviving
Actor births from the genuine lower-crate fatal damage together, frame-major.
Only the first original Maps/00_Training.dx is integrated. This is a physics
suffix diagnostic, not completed Actor/Level Tick, live controller combat,
published player startup, a new APK or headset acceptance.

## Runtime and actual native dispatch

PortableWorldStartupProbeOptions.physicsCohort supplies placed survivors, one
captured damage-command index, a positive Float timestep at most0.4 and at most
eight frames. Membership is bounded to eight distinct real current-Level
actors. Placed actors must be authored; births must retain their actual
runtime-birth/class identity from the captured damage observation. Deleted
births are not resurrected; unexpected surviving native implementations
explicitly refuse rather than silently substituting Actor physics.

The original owned metadata RF_Native boundaries and vtable/import thunks are
documented in ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md. The actual first-map native
ancestors are DeusEx.DeusExDecoration (crate), Engine.Projectile (WoodFragment)
and Engine.Inventory (Lockpick). Each resolves the original AActor physics
implementation. Engine.Fragment and DeusEx.DeusExPickup are non-native.
The runtime authenticates the nearest RF_Native boundary, not inherited
ClassFlags or a broad IsA predicate alone.

For each explicit frame the driver reads live ascending native dynamic slots,
including their current array size and identities. Only the admitted cohort
gets this diagnostic's physics suffix. Actual role/remote-role/Physics bytes
select whether performPhysics runs: RemoteRole3 bypasses it; Role below2
admits only Falling2; other roles require nonzero Physics and Role not3.
Physics0 therefore skips the entire common epilogue in future frames. There
is no added per-update bDeleteMe guard, fake pose/velocity/support assignment,
collision suppression, inventory grant or deletion of inconvenient debris.

Every row captures all retained cohort members before/after the source call,
plus the complete existing falling/rotation callback/store/clock observation.
Skipped calls retain their genuine unchanged states and zero call/write counts.
Real based-child movement is observed inside the existing MoveCarryChildren
provider: source/child identities, actual Base, Location before/after, exact
delta/rotation/options0010, return and shared VM write prefix. Recursive calls
retain row indices rather than references across possible vector growth.
Detached outputs use a bounded2MiB shared-host reservation/retention ledger;
same-root movement, callbacks and rollback remain the production services.

This intentionally does not run earlier stasis, owner deferral, animation,
Tick/state/timers/lifespan, parity, pending newborn batches or level time.
No resulting partial startup is published. The ordinary later Possess/HUD
continuation still runs and any genuine failure undoes the entire root.

## Diagnostic and acceptance evidence

```powershell
& 'artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe' --owned-source-root 'D:\Steam\steamapps\common\Deus Ex' --perform-training-cohort --native-work 1048576
```

The CLI first lets both actual crates fall onto genuine world/lower support,
then damages only the lower twice through the original selected TakeDamage.
The cohort is the actual surviving upper and all six WoodFragments plus the
Lockpick born by that fatal event, not a list of guessed generated names.
It requests eight physics-suffix frames of Float0.4 in actual native order.
The observer validates original membership/initial launch/support-loss states,
all-member state continuity, native slot/frame order, independently computed
eligibility, skipped no-effects, actual callbacks/stores and carry packets.
Repeated attempts compare every detached row exactly. Late write/work cuts
must retain an honest incomplete final row and fully undo every earlier
damage, birth, fall, bounce, support, rotation and carry.

An actual carry witness needs BOTH real child movement through the provider
and actual supporting-source translation. A child move can precede a later
parent movement veto; do not infer source success from a child return alone.
No equal delta, monotonic Z or predicted contact is required. If the real
trajectory does not produce carry, record that coverage gap instead of forcing
an intersection. Final support must be interpreted from the real chain, not
assumed world support merely because Physics became0.

## First actual-map observation (2026-10-24)

Desktop focused build and exhaustive unsigned-byte eligibility controls pass
(16777668 checks). Strict ARM64 syntax and Android native compilation/link
pass. These are native libraries, not an APK or on-device gameplay proof.

The first real-map run captures all64 rows:11 eligible calls and53 genuine
skips,48 falling sweeps,13 selected fragment HitWall calls, two ordinary
landings, nine rotation calls/real rotational moves and three child carries.
No PostTouch is reached in this actual sample. The upper's native slot278
precedes genuine birth slots430..436. Frame0 upper settles on Lockpick0 at
Z-77.4741135. In that same frame the actual Lockpick update makes three
options0010 child moves of that already stopped upper, with the source moving
from (995.230774,-282.894928,-103.899864) to
(994.433411,-273.291382,-123.038185). The upper moves from
(1023.12817,-274.078857,-77.4741135) to
(1022.33081,-264.475311,-96.6124344), outside its own physics call.
Both child and source translation are directly witnessed; this is not an
inference from only a child return. The genuine final support chain is
upper -> Lockpick0 -> LevelInfo0, NOT direct upper -> world and not the
destroyed lower. All six real fragments are Physics0/world-supported Dying
by the end of frame1; frames2..7 make no further physics calls/writes.
No objects were erased or manually positioned to obtain this result.

Crate death completes at write15123; cohort updates end at16415. Ordinary later
Possess/HUD still refuses the unchanged fixed Name-array member at
DeusEx.DamageHUDDisplay.InitWindow PC39/opcode1:223050 VM instructions,
25845 writes and519971 native work of1048576. All41 original source
fingerprints and full save/state/revision/hash/RNG/client/lookup/Actor/GC
rollback pass. Discovery intentionally returns2, not playable-startup success.
Evidence: artifacts/training-physics-cohort-discovery-20261024.log.

Both final complete repeats reproduce every detached cohort state/pose,
support, call/store/clock/carry packet and write prefix exactly. VM totals
remain223050 instructions/25845 writes; native work519971 cold/519347 warm.
Evidence: artifacts/training-physics-cohort-repeat-20261024.log.

Repeated write16105 cutoff passes at the third actual child movement:
seven captured source rows plus the incomplete Lockpick update, two completed
carries and a third entered/not-returned carry (writes16101->16105). Both
attempts retain exactly matching detached prefixes,206744 VM instructions/
16105 writes, native work485056 cold/484432 warm. The full original death/
births, all previous falling/bounces/support/rotations, both earlier child
moves and the partial third move roll back; all41 sources remain unchanged.
This is a native C++ failure boundary with empty script Function/PC0/opcode0,
not an invented named script failure. Evidence:
artifacts/training-physics-cohort-carry-cut-20261024.log.

Repeated later write16414 cutoff also passes:14 captured rows and the
incomplete final active fragment update, after all three carries and both
ordinary landings. Both attempts match207564 instructions/16414 writes,
native work488534 cold/487910 warm; earlier complete scene effects plus this
partial update fully undo/all41 sources remain unchanged. Captured-row-only
totals are43 sweeps/11 hits/eight rotations, not a claim that the incomplete
final row's extra operations were independently captured. Evidence:
artifacts/training-physics-cohort-settled-cut-20261024.log.

The coherent full desktop build passes. Shared CTest passes179 executed/
211 registered,32 explicit owned-data skips, zero failures in96.65s. Fresh
post-build original crate controls pass347 checks/two genuine dormant contents
refusals; these are separate from the initialized-world surviving-loot proof.
Evidence: artifacts/training-physics-cohort-full-build-20261024.log,
training-physics-cohort-ctest-20261024.log and
training-physics-cohort-crate-final-20261024.log. Fresh post-build owned
first-Training SetPhysics passes172882 checks/53 explicit refusals, including
19 partial-write cuts and actual Doctor1 full animation-clock preservation.
Normal no-command startup retains218253 instructions/24192 writes,
native work491158, the same genuine HUD refusal, full-root undo/all41 unchanged
sources and no manufactured physics cohort rows. Evidence:
training-physics-cohort-setphysics-final-20261024.log and
training-physics-cohort-normal-regression-20261024.log in artifacts/.

No new APK has been built or installed by this increment. Whole first Training
and eventual full campaign remain unfinished.
