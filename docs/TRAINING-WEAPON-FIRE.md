# First Training original weapon Fire

Scope: first `00_Training.dx`, authored `WeaponCrowbar1` and
`WeaponCombatKnife0`. This diagnostic is not a committed player, live Quest
trigger, successful hit or playable crate sequence.

`--fire-training-weapon ACTOR_PATH` extends the proven pickup/equip diagnostic
with one selected original `DeusExPlayer.Fire(0)` and bounded ordinary main
animation followed by original state continuation. All calls share one Host,
Execution, journal, RNG and budget. Normal startup and equip-only entry points
do not request this extra operation. The separate Fire result interface does
not change normal startup options/results.

The helper requires genuine ownership, inventory links, held Mesh, ready/up
Idle and completed pending transition before Fire. It does not choose the
attack sequence, fabricate readiness/ownership, supply aim/target/damage, call
the class Fire around a state override, or ignore a reached unsupported native.
The original scripts choose Attack/Attack2/Attack3. Metadata records are
explicitly not notify-dispatch or hit proof. Animation counters are retained
only after that entire phase returns; a failing notify cannot invent a returned
counter. Failure snapshots are detached observations before whole-root undo.

## Initial actual-map boundary, 2026-10-24

Both actual weapons select `Attack`, enter original `NormalFire`, progress
through its Sleep continuation, and reach their authored time0.5 notify. The
actual error call stacks contain original `HandToHandAttack` then `TraceFire`:

| Actor | Instructions | Writes | Failing original operation |
| --- | ---: | ---: | --- |
| WeaponCrowbar1 | 205986 | 14972 | TraceFire149, native229 GetAxes |
| WeaponCombatKnife0 | 205911 | 14967 | TraceFire149, native229 GetAxes |

Crowbar records14 phases, knife13. The final animation call throws, so neither
claims a returned notify counter, completed attack cycle, trace or TakeDamage.
Both discovery commands exit2 for the explicit unsupported boundary, not a
passing startup. Whole save/state/revision/hash/RNG/client/lookup/Actor/GC undo
and all41 owned source fingerprints pass. No other map is loaded.

Logs: `artifacts/training-weapon-fire-{crowbar,knife}-discovery-20261024.log`.
Desktop probe, strict ARM64 runtime/CLI syntax and Android native library link
pass for this initial Fire diagnostic. No APK was produced.

```powershell
& artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe --owned-source-root 'D:/Steam/steamapps/common/Deus Ex' --native-work 16777216 --fire-training-weapon '00_Training.WeaponCrowbar1'
```

## Subsequent GetAxes integration

Native229's audited rotation/output semantics are now implemented and tested;
see ORIGINAL-GET-AXES.md. Both weapons pass that actual operation and original
ComputeProjectileStart, next refusing native277 in selected
`Engine.PlayerPawn.AdjustAim` atPC18/opcode97. Crowbar:206046 instructions,
14978 writes (repeat2 exact); knife:205971/14973. Whole-root undo and all41
owned source fingerprints remain unchanged. Logs:
`training-get-axes-crowbar-repeat-20261024.log` and
`training-get-axes-knife-discovery-20261024.log`.

Actual crowbar repeat2 with `--damage-write-limit 14973` stops inside
GetAxes229 after its first output write, at TraceFire149/opcode229:
205986 instructions,14973 writes, genuine Budget/VM write limit. Partial axes,
all earlier inventory/equip/attack/animation/state and native births fully undo;
all41 source fingerprints remain unchanged. The write-limit option is shared
with the earlier damage diagnostic, not a second damage operation. Log:
`training-get-axes-crowbar-axis-writecut-20261024.log`.

No-command startup repeat2 still stops at the exact old HUD1463 location,
218693 instructions/24284 writes. It runs no extra Fire phases and retains
complete original rollback/source checks. This protects the default startup
entry rather than declaring that unsupported startup successful.

## Subsequent original collision-trace integration

The original Actor.Trace277 wrapper now uses the existing real current-Level
query services, preserving ordered OUT writes, live sequential defaults and
copied ordinary inputs. See ORIGINAL-ACTOR-TRACE-WRAPPER.md. Actual crowbar
repeat2 now reaches PickTarget531 at AdjustAim180/opcode98,206070 instructions/
14982 writes; knife reaches the same operation at205995/14977. Full-root undo
and all41 unchanged sources still pass. This is a passed trace-call prefix,
not evidence that an actual melee target was hit or damaged.

Two crowbar runs with `--damage-write-limit 14979` stop inside Trace277 after
its first output store: AdjustAim18/opcode97,206046 instructions/14979 writes,
VM write limit, complete earlier state/output undo and41 unchanged sources.
Equip-only repeat2 retains native1463/221615 instructions/24458 writes.
The final coherent shared gate passes184 executed/219 registered,35 owned
skips, zero failures in87.99s; final strict ARM64 and Android native link pass.

## Subsequent original target-selection prefix

Native531 now enters its actual PawnList/team/scalar-selection prefix with
copied Vector inputs, initialized Float temporaries and guarded live OUT
aliases. Crowbar repeat2 and knife reach its internal C++
Pawn.LineOfSightTo(candidate,0,false) service at the same AdjustAim180,
206070/14982 and205995/14977 respectively. Full-root undo/all41 sources pass;
visibility is not invented or replaced with selected script514. Actual dormant
map controls separately pass181 calls/277720 checks/90 refusals, including
authentic AddPawn/cycle fixtures and original checkpoint restoration; these
are not visible-target/weapon-hit evidence. See ORIGINAL-PAWN-PICK-TARGET.md.

The next required service is the actually reached internal Pawn LOS,
not an account/player-creation UI or a guessed direct crate-health shortcut.
No controller aim, TraceShot, crate TakeDamage, surviving debris/loot physics
or collection has yet been connected to committed gameplay by this diagnostic.

## Subsequent original internal LOS and completed cycle

Both weapons now pass the actual reached internal Pawn LOS and complete one
NormalFire attack, including one returned original HandToHandAttack notify and
return to ready/up Idle. Each has22 captured phase rows. This does not prove a
hit: the genuine JC start is about2434 horizontal UU from the lower crate,
whereas both weapons have80UU melee range. No aim/target/damage was supplied.
The general Actor TraceShot/ProcessTraceHit path already admits crates;
PickTarget531 remains the original Pawn-only auto-aim service.

Crowbar repeat2 matches at the later HUD CreateIcons diagnostic-memory boundary;
both fully undo/all41 sources remain unchanged. Partial-output write-cut repeat2
at14983 stops in TraceFire212/opcode15,206100 instructions,14 rows, no returned
notify or cycle. Normal startup repeat2 remains exact native1463/218693/24284.
Shared189 executed/225 registered,36 owned skips,0 failures; independent math,
protocol, mock-host VM and actual first-map target/trace controls pass. See
ORIGINAL-PAWN-LINE-OF-SIGHT.md for exact results and final memory accounting.

Completed Fire now retains its cumulative actual detached capacities instead
of its whole conservative4MiB admission. Only unused observation allowance is
released, only after final success checks/snapshots. Failure keeps full
admission; shared callback observations and query/undo/RNG charges are not
refunded. The32MiB state limit is unchanged. No live input, publication or APK.
