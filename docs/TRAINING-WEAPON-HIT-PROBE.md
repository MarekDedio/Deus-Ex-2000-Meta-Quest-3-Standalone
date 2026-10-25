# First Training: real weapon-to-crate hit diagnostic

Only Maps/00_Training.dx. This extends the rollback-only genuine original
pickup/equip/Fire probe; it is not a published player or Quest input bridge.
The default command still performs one attack at the actual starting pose.

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
