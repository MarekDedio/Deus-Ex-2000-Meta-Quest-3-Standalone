# First Training: real weapon-to-crate hit diagnostic

Only Maps/00_Training.dx. This extends the rollback-only genuine original
pickup/equip/Fire probe; it is not a published player or Quest input bridge.
The default command still performs one attack at the actual starting pose.

Latest2026-10-25: portable0x50 now lets two aimed crowbar swings finish.
Actual crate HP10->4->-2, removal, six WoodFragments and a surviving Lockpick
are observed; see the final section below. This supersedes the initial
missing-conversion boundary, not the live-publication limitation.

Subsequent scalar-window integration,2026-10-25: the same complete two-swing
destruction/loot-birth diagnostic now passes original1463 and reaches the next
TileWindow native preferred-size override refusal at original augmentation-
border callbackPC9. Discovery remains48rows/two cycles/two notifies/four traces/
nine births, now227257instructions/24907writes; complete undo/all41 sources
passes. No live publication or automatic falling follows. See
ORIGINAL-WINDOW-PREFERRED-AXIS-QUERY.md for the distinct startup dependency.

## Explicit input, not a forced hit

The optional paired CLI packets `--fire-location X Y Z` and
`--fire-rotation Pitch Yaw Roll` call the selected original Actor.SetLocation267,
then selected Pawn.ClientSetRotation. A false SetLocation stops the diagnostic;
no rotation or Fire is attempted. The actual possibly adjusted Location is
observed. No Location/ViewRotation property, Enemy, FrobTarget, bProjTarget,
hit actor or damage value is patched. All functions share the existing root,
native collision/hash/zone services, write journal and budgets.

`--fire-attacks 1..4` requests complete separate original Fire/NormalFire/Idle
cycles. Each begins only after genuine owned ready/up Idle, and must return a
new authored notify before it can complete. Sequence choice and damage remain
original script decisions. The phase count is per attack; total notify counts
are cumulative across the command. No complete Tick/scheduler is implied.

ClientSetRotation is the actual Engine.Pawn script (flags0xC2/logical45): it
sets ViewRotation from the input before zeroing local Pitch/Roll and invoking
SetRotation299 for the body. SetLocation is native267/flags0x401/logical5 with
required Vector and Bool result. The existing original FarMove/Move providers
preserve real collision/encroachment/hash/zone behavior. Static bytecode log:
artifacts/training-weapon-hit-pose-script-20261025.log. Commercial EXE/DLL code
is never loaded/executed.

Input validation allows finite signed positions and raw signed32 rotation
words without normalization; active nonfinite positions, invalid positive
timestep or counts refuse. It cannot certify a valid placement. One to64
phases per attack and at most four attacks plus two pose calls bound detached
output to262 rows, subject to the unchanged4MiB observation/32MiB state limits.

## Observations and limits

Each row records real before/after player Location, Rotation, ViewRotation and
BaseEyeHeight, both authored crates' actual HitPoints/state/removal/Base and
real post-phase Actor births. After snapshots are valid only when captured or
partialAfterCaptured; poseCaptured alone cannot certify later crate/birth reads.

Native Trace277 records a reply only after the actual Level query returns.
Start/end/extent, actual returned actor and hit location/normal are copied,
with outputsComplete set only after ordered script OUT stores. A failed store
retains its incomplete flag and shared write prefix. The optional sink is
lexical to this one diagnostic, clears on every exit and retains no engine
pointer in detached output. At most64 actual trace replies are retained;
capacity/string charges share the Fire output budget. Normal gameplay and
unrequested native traces do not allocate diagnostic rows.

A returned notify alone is not a hit. A useful witness must show an actual
crate trace, original HP changes from repeated eligible swings, original
removal/fragments/surviving loot, and complete failure/undo controls. Actual
on-device input, automatic physics, committed save/render/audio and accepted
startup remain separate required integration. Test results follow only after
the compiled diagnostic runs against the owned first-map data.

## Initial build/input checks

Host probe and input target build, Android native library link and strict
ARM64 runtime compilation pass. The diagnostic CLI's first strict cross-build
found libc++ lacks floating from_chars; the final CLI uses a bounded classic-
locale noskipws stream instead (integer rotation parsing still uses from_chars).
Final strict ARM64 CLI/input sources pass. Asset-free command controls pass
1692 checks/576 accepted/32 refusals, including signed zero, finite extrema,
signed32 rotations and all count/active-nonfinite refusals. Eight invalid CLI
packets reject before loading owned data, including partial pose packets,
NaN/leading whitespace/trailing bytes, rotation overflow and duplicate counts.
These controls certify input/bounds only, not a hit or legal world placement.

Final coherent full desktop/shared gate:190 executed/226 registered,36 explicit
owned-data skips,0 failures in55.89s. Default unposed crowbar repeat2 retains
the exact HUD1463 boundary/222711 instructions/24542 writes,22 rows/one cycle/
one returned notify. Two real traces both hit distant DeusExMover16, so no
crate health change or birth is claimed. Complete undo/all41 sources pass.
Log: training-weapon-hit-default-repeat-20261025.log.

The initially supplied(930,-275,-81.4) location is an honest original
SetLocation false return:one captured placement row,no rotation/Fire/trace,
no crate health change and complete undo/all41 sources. A miss test that
incorrectly expected later1463 was rejected for its mismatched expected error,
not silently accepted. This position is not a certified approach. Original
CDO inspection confirms JC radius20,height47.5,BaseEyeHeight40,Movable true,
Static false,CollideWorld true; placement still needs actual map clearance.

## First actual crate-hit boundary

The explicit east-side input(1100,-275,-65),view(-8192,32768,0) is accepted
by original SetLocation/ClientSetRotation. Real current-Level traces from
JC's eye(1100,-275,-25) return lower CrateBreakableMedGeneral1 at
(1044.4563,-275,-80.5436859),normal(0,0,1). The original attack notification
enters ProcessTraceHit/Active.TakeDamage and HP changes10->4. No direct
damage command, selected target or patched health was supplied.

That same animation phase then refuses in DeusExWeapon.SpawnEffects at
PC131/opcode80:unsupported VectorToRotator conversion. Its authored bytecode
converts HitNormal to orient a real impact spawner. The phase did not return,
so16 rows,zero returned notifies/complete cycles,one HP change and zero births
are reported. No second swing, fatal break, debris or surviving loot is claimed.
Whole save/state/revision/hash/RNG/client/lookup/Actor/GC undo/all41 unchanged
sources pass. Log:training-weapon-hit-east-discovery-20261025.log. SpawnEffects
bytecode:training-hit-spawn-effects-script-20261025.log. The missing conversion
is the next actually reached gameplay dependency, not an unrelated HUD task.

The west-side x930 placement also refuses at Z=-65; detached original BSP
inspection finds solid space there, while east x1100 and north/south approach
points reach floor Z=-127.5. This is setup evidence, not a replacement native
placement/world authority. Its initial Z200 rays start in ceiling geometry and
must not be misreported as floor hits. Logs:training-hit-shape-audit-20261025.log
and training-hit-shape-audit-midroom-20261025.log.

## Repeated hit / wall-facing / partial-output controls

East aimed repeat2 matches206966 instructions/15057 writes,16 rows,two actual
traces and one original HP10->4 change. Both attempts retain the actual trace
endpoints and complete undo/all41 sources. Log:training-weapon-hit-east-repeat-
20261025.log. Calling this an expected-boundary success does not make the
unreturned notify or refused impact effect successful.

At the same accepted location,view(0,0,0) faces away from the crates and hits
world LevelInfo0 at(1159.50122,-275,-25). Crate HP is unchanged. It too needs
the same impact conversion, so the initial expectation of later HUD1463 was
rejected. Corrected repeat2 matches SpawnEffects131/conversion80 with206867
instructions/15052 writes,two real world traces,no crate changes/no births and
complete undo/all41 sources. Log:training-weapon-hit-east-away-corrected-repeat-
20261025.log. This is a world-hit/no-crate-damage control, not a ray miss.

East aimed write-cut repeat2 at15045 stops inside original SkillManager.
GetSkillLevelValue11/opcode15 with206706 instructions,16 rows/two traces,
no HP change or birth. Complete shared state/outputs/RNG/lookup/hash/GC/client/
Actor/save undo and all41 unchanged sources pass. Log:training-weapon-hit-east-
writecut-20261025.log. This cut is BEFORE crate damage, not after the HP store.
Fresh uninstrumented actual Trace277 controls pass43 production calls/37063
checks/23 refusals with all nine native-work cuts; no diagnostic rows leak into
normal calls. Log:training-weapon-hit-owned-trace-20261025.log.

Blocked west-side placement repeat2 matches205372 instructions/14930 writes,
one row and no rotation/attack/trace/HP change/birth. Complete undo/all41 sources
pass. Log:training-weapon-hit-blocked-pose-repeat-20261025.log. No false native
placement result becomes an accepted pose or forced relocation.

Post-damage write-cut repeat2 at15056 stops in SpawnEffects0/opcode15 with
206950 instructions,16 rows/two traces and actual lower-crate HP4. The whole
checkpoint is restored on both attempts, including the already-applied original
damage, with all41 owned sources unchanged. Log:training-weapon-hit-postdamage-
cut-20261025.log. Final dedicated strict ARM64 runtime check also returns0;
all compiler/test jobs are terminal before committing this checkpoint.

## Completed original damaging cycles,2026-10-25

After general portable VectorToRotator0x50 implementation, the SAME east-side
pose/aim and two original selected player Fire calls complete two genuine
weapon attack cycles. The original script chooses Attack then Attack3, returns
both HandToHandAttack notifies and restores the weapon's ready/up Idle state.
All four actual trace replies hit the lower authored crate; no direct damage,
forced hit actor, state patch or supplied health value is involved.

The lower crate changes10->4->-2 and becomes removed/deleteMe. The untouched
upper crate remains HP10. Real births are six WoodFragments in Flying/Physics2,
Lockpick0 in DeusExPickup.Pickup/Physics2 with no owner and removed/deleteMe=false,
and two tracehithandnonpenspawner Actors oriented(16383,0,0). The Lockpick spawns
at(1000.01221,-286.9953,-103.899864). It survives the destruction callback;
falling/settling/collection are NOT run by this Fire diagnostic.

Discovery:48 observation rows,two complete cycles/two returned notifies,four
actual traces,two crate-health changes,nine births. Later ordinary client setup
still refuses native1463 at DeusExHUD.ConfigurationChanged606, after227245
instructions/24892 writes. Whole-root state/save/revision/hash/RNG/client/
lookup/Actor/GC undo and all41 unchanged owned sources pass. Discovery exits2
because original startup is still unsupported, not because Fire failed.
Log:vector-to-rotator-crate-discovery-20261025.log.

This is an actual original weapon-to-crate-to-surviving-loot diagnostic, not
live controller authority, automatic world physics, committed loot/save/render/
audio or on-device acceptance. Other map integration remains deferred.

Repeat2 matches the complete discovery exactly:227245 instructions/24892
writes and48 rows/two cycles/two returned notifies/four traces/two HP changes/
nine births. Detached source/pose/trace/birth/weapon snapshots agree; full root
undo/all41 sources pass. Log:vector-to-rotator-crate-repeat-20261025.log.

A forced shared write cut at15343 occurs AFTER the second damaging notify
returns and all nine births exist, on its next animation phase. Repeat2 matches
210822 instructions/15343 writes,40 rows/one completed cycle/two returned
notifies/four traces/two HP changes/nine births. The lower crate is genuinely
removed with HP-2 before the failure. Whole-root undo restores it, removes the
new loot/fragments/effects and restores all original state/source fingerprints.
This is an explicit failed cycle, not falsely reported as two complete cycles.
Log:vector-to-rotator-fatal-writecut-20261025.log.

Away/world-hit discovery now passes0x50 and creates an actual impact spawner,
then reaches original GetWallMaterial42,iterator native1000/TraceTexture.
It records207017 instructions/15061 writes,16 rows/no completed cycles/no
returned notify,two actual LevelInfo0 traces,zero crate changes/one birth.
Complete undo/all41 sources passes. That reached surface-material dependency
is still unsupported; no wall-hit sound/effect completion is claimed. Log:
vector-to-rotator-away-discovery-20261025.log.

Away repeat2 matches that exact iterator1000 boundary and every detached
observation; crate HP remains unchanged and whole undo/all41 sources pass.
Log:vector-to-rotator-away-repeat-20261025.log. Fresh original bytecode confirms
native1000 is Engine.Actor.TraceTexture,flags0x405/logical50: required Class,
OUT Actor/Name/Name/Int/Vector/Vector,required End Vector,optional Start/Extent
Vectors. GetWallMaterial41/42 invokes it with eight expressions (implicit
receiver Start and zero Extent); its body seeks LevelInfo/Mover texture group.
This declaration evidence does not establish original iterator implementation;
the pinned TraceTextureIterator is not silently substituted for retail logic.
Static package logs:training-hit-{wall-material-script,trace-texture-declaration}-
20261025.log. No commercial DLL/EXE is loaded or executed.

Unposed default Fire regression still completes one cycle/notify without
crate damage:22 rows/two distant mover traces/no births, later HUD1463 at
222711 instructions/24542 writes, complete undo/all41 sources. Log:
vector-to-rotator-default-regression-20261025.log. Fresh actual uninstrumented
Trace277 controls pass43 calls/37063 checks/23 refusals/nine work cuts;
original native279 crate controls pass347/two explicit dormant-contents
refusals. Logs:vector-to-rotator-owned-{trace,crate}-20261025.log. These separate
controls do not claim live startup or surviving loot in a dormant fixture.

Final coherent full desktop build/shared CTest:192 executed/228 registered,
36 explicit owned-data skips,zero failures in57.80s. Android native library
link and strict ARM64 VM/new-test compilation pass. All build/test processes
are terminal before commit; no new APK or installed-headset change is made.
