# Original first-door movement observation

First Training only,2026-10-25. This is a non-publishing diagnostic for the
actual authored first-door panels, not the VR presentation-layer door motion.
It is intended to establish whether original script-driven interpolation can
reach real movement/completion inside the new native actor update operation.

## One original script and frame root

The optional `moverCommands` input calls exact original
`Engine.Mover.InterpolateTo(NewKeyNum,Seconds)` through the existing Execution.
The Function, final/script flags3 and two required scalar Byte/Float inputs
are authenticated from the owned Engine.u metadata. There is no direct
assignment to pose, Physics, interpolation flag, key, alpha or rate.

The hook runs after original Login returns its JC and native SetPlayer pairing
exists, immediately before the requested, unfiltered normal dynamic-actor
phase and before ordinary Possess. This timing is an explicit diagnostic
insertion, NOT a claim about the completed native world frame schedule.
It shares the same startup Execution, native collision state, budgets,
callbacks, object/reference journals and final undo.

Commands require one actual client/viewport/Login, real begun GameInfo and
current Level/collision ownership. Only the actual authored dynamic
`00_Training.DeusExMover16` and `00_Training.DeusExMover17` are accepted, with
at most8 calls, key0..7 and finite seconds in(0,60]. No damage, loot, falling,
physics-cohort, weapon or Fire command may share this diagnostic. Ordinary
startup has no mover command and its path is unchanged.

Detached observations include native Location/Rotation/OldLocation/Velocity,
Physics, key, glide, interpolation flag, alpha and rate before/after the
original call and after the reached actor phase. Flags distinguish an entered
or returned call/frame from a captured snapshot. A failed diagnostic snapshot
must not replace the actual script/native/budget exception. Same-root preview
commit always rolls back, even if a later startup path should complete.

The observation executable additionally compares repeat snapshots bitwise,
checks full checkpoint/native hash/RNG/client/object/Actor/GC/revision undo,
and explicitly rereads requested mover fields after rollback. Expected-error
matching remains exact; a matched unsupported boundary is not startup success.

## Reproduction

Use the built `portable_training_startup_probe.exe` with the user's owned
installation. These examples request the first panel's authored key1, then
run the existing1/72-second unfiltered actor phase:

```powershell
& artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe `
  --owned-source-root 'D:\Steam\steamapps\common\Deus Ex' `
  --native-work 16777216 --repeat 2 `
  --interpolate-training-mover 00_Training.DeusExMover16 1 0.25

& artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe `
  --owned-source-root 'D:\Steam\steamapps\common\Deus Ex' `
  --native-work 16777216 --repeat 2 `
  --interpolate-training-mover 00_Training.DeusExMover16 1 0.005
```

The shorter duration requests the original script's minimum movement time,
so a successful physics call may reach captured-alpha completion and the
selected InterpolateEnd event. Results, not the command alone, must establish
whether either movement or completion actually occurred. The diagnostic does
not simulate a controller Trigger/Bump interaction or prove paired-door links,
headset rendering, a committed save or playable crates/loot/gravity.

## Verification results

Results belong here only after the corresponding build/probe jobs finish.

The initial actual-map observations each repeated twice with bit-identical
command/state/write-prefix records, matching final instruction/write counts
and complete native/world rollback. All reached the same later exact gate:
`Original Actor Tick reached an unimplemented native virtual override: DeusEx.DeusExLevelInfo`.
They exit2 as explicitly unsupported observations, not completed startup.

| Requested input | Actual reached behavior | Instructions / writes |
| --- | --- | --- |
| Ordinary actor phase, no mover command |48 slots,47 admitted Actor calls,14 idle Physics9 blocks|205885 /15721|
| Panel16,key1,seconds0.25 |Original call sets rate4/flagtrue/key1; actual MoveActor reaches Y799.500671 from800, alpha0.055555556, Velocity.Y-35.9516602;1 move/no completion event|205990 /15747|
| Panels16+17,key1,seconds0.005 each |Original calls set rate200; actual native moves reach Y744 and919.998535 from800 and863.998535, two opposed56-unit displacements;2 moves/2 completion events, flagsfalse/alpha0, Velocity.Y-4032/+4032|206221 /15789|

The ordinary Engine.Mover.InterpolateEnd script was independently inspected:
it sets PhysAlpha back to0, updates PrevKeyNum/ClientUpdate, and on the terminal
key updates RealPosition/RealRotation. Thus the completion snapshot's alpha0
does not mean the native captured-alpha1 completion store was skipped. Native
event counters and the real script are separate corroboration; no substitute
completion callback was installed. An extra paired-door run with exact
`--expected-error` matching exits0 as a boundary diagnostic only, with the
same206221/15789, two moves/events and full undo.

Seven malformed CLI inputs were rejected before game loading: key8, zero/
nonfinite/above60/whitespace seconds, a different Training mover, and mixing
the command with direct crate damage. Strict ARM64 syntax and Android-native
builds pass. Full desktop build and final shared CTest also pass:244
registered,205 executed,39 explicit owned-data skips,zero failures,53.89s.
Expanded generated Trigger controls pass1268 cumulative checks/33 refusals;
extended Mover/Physics9 controls log2375/59 before the final passing
malformed-OldLocation-schema control.

An additional paired-door control uses `--native-work 473000 --repeat 1`.
Both actual moves and both selected completion events finish before the
same-root work allowance is exhausted at exactly473000. The original error
`Native actor lifecycle work budget exceeded` is preserved (status Budget),
not replaced by an observation error. Both post-frame snapshots still capture
the moved/finished doors; full checkpoint/state/hash/RNG/client/Actor/GC undo
passes. This observation exits2 and is explicitly not successful startup.
Its reached instruction/write prefix is205841/15719, with28 slots/calls and
4 Physics9 blocks. It proves late-budget undo of genuine original movement,
not only a fixture inner or an inactive path.

Separately run original Training regressions pass: crate415 checks/5 genuine
refusals, deferred native ownership429/21, animation588/24. These are not
substitutes for published crate attacks, usable loot, automatic gravity or
headset acceptance.

Two original crowbar-Fire attacks still repeat twice with exact228837
instructions/26047 writes,48 observation rows,2 animation cycles/returned
notifies,4 actual traces,2 crate changes and9 births including surviving
Lockpick/debris. Both roots undo fully at the unchanged FExec boundary.
All45 checked original script/config/Training-map files retain exact SHA256,
lengths and UTC modification timestamps. All jobs are terminal; no Quest is
detected, and no APK was packaged or installed.
