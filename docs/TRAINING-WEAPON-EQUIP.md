# First Training: original weapon pickup and equip

Implementation under verification,2026-10-24. Only the real authored
`00_Training.WeaponCombatKnife0` and `00_Training.WeaponCrowbar1` are admitted.
This is the next missing live-melee prerequisite, not evidence that Quest
trigger attacks, crate damage or player startup have been completed.

The optional `--exercise-training-weapon ACTOR_PATH` mode of
`portable_training_startup_probe` uses the existing genuine first-map startup
Execution after original GameInfo.Login returns JC and before normal
SetSessionPlayer/Possess. It does not construct another player, grant an item,
force an equip state or accept the successful prefix as a playable session.
Default/no-command startup remains unchanged. Other-map content and unrelated
Login/character-creation UI are outside this increment.

The runtime supplies the actual authored target to JC's authenticated member
FrobTarget and calls selected original ParseRightClick. This target injection
is explicit diagnostic input, not proof of controller reach/occlusion. Original
scripts alone decide pickup eligibility, AmmoNone births, ownership and grid
placement. Only after proving real Owner and inventory-chain membership does
the coordinator call selected original PutInHand and UpdateInHand. It never
writes Pawn.Weapon or calls SetInHand directly.

At most64 explicit bounded phases advance the actual weapon's ordinary main
animation, then its same-Execution state phase. FinishAnim261/poll385 retain
their original command/poll order. The diagnostic does not call an overriding
class Fire while Active's real Fire is empty. A final selected UpdateInHand
must clear the original pending transition after original Idle is reached.
Previous-weapon transition handling is refused rather than silently advanced
or overwritten. Normal SetSessionPlayer/Possess always follows a completed
diagnostic, preserving its genuine later failure and whole-root rollback.

Detached observations retain before/after player links, all bounded real
inventory identities, actual grid, weapon Mesh/PlayerViewMesh, ammo, sequence,
animation fields, frame/state PC and write ranges. Serialized raw native latent
UInt32 and modern portable enum are separate domains; Continue does not invent
native0. Output is bounded to8MiB with64KiB scratch and128 inventory links per
snapshot. The CLI independently derives ownership and full equip readiness
from snapshot fields rather than trusting a producer's completed flag.

Example discovery (exit2 means a safely rolled-back unsupported boundary,
not passing startup; use an exact expected error only after observing it):

```powershell
& artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe --owned-source-root 'D:\Steam\steamapps\common\Deus Ex' --native-work 16777216 --exercise-training-weapon '00_Training.WeaponCrowbar1'
```

The desktop probe builds, strict ARM64 runtime/CLI syntax passes and Android
native libraries compile/link. Both actual-map discovery runs now complete
the original pickup/equip sequence before reaching the unchanged later
native1463 HUD boundary. Each captures ten rows: pickup, PutInHand, initial
UpdateInHand, six animation/state slices at0.05s, final UpdateInHand.

The genuine original item retains Owner/Instigator=JC, actual AmmoNone0 and
NanoKeyRing1 in the three-link inventory, Mesh=PlayerViewMesh, physical mode0
and original hidden/carried/owner-only flags. Crowbar occupies actual grid0,0
with2x1 slots; knife occupies0,0 with1x1. Select advances to genuine native385
waiting at state PC7; the sixth slice sets finished, dispatches AnimEnd and
continues original Active to Idle/Stop. Only final original UpdateInHand clears
bInHandTransition. JC remains authentically unpaired in InvalidState, not a
published player. Raw loader native latent0 stays separate from modern wait3.

Crowbar uses221615 instructions/24458 writes, knife221540/24454. Each reaches
the original later Possess/HUD native1463 refusal and verifies full save/state/
revision/hash/RNG/client/lookup/Actor/GC rollback with all41 source fingerprints
unchanged. Discovery exits2 explicitly: this is NOT successful startup.
Evidence: `artifacts/training-weapon-crowbar-discovery-20261024.log` and the
matching `training-weapon-knife-discovery-20261024.log`. Repeat/late-cut/full
shared regression gates follow below.

Two crowbar repeats retain identical ten-row ownership/grid/state/clock/write
observations at221615 instructions/24458 writes. Native work is496023 cold /
495481 warm. Both preserve the later exact1463 boundary and complete-root
rollback/all41 unchanged sources (`training-weapon-crowbar-repeat-20261024.log`).
Two write14929 cutoffs fail at the final UpdateInHand PC492 Bool store, after
completed pickup, equip animation, native385 release and original Idle. Both
retain205331 instructions/14929 writes and fully undo that earlier progress,
including AmmoNone birth and original inventory/grid ownership. Native work
is465447 cold/464905 warm. This cutoff is an explicit Budget refusal (CLI exit2),
not a successful startup or attack (`training-weapon-crowbar-latecut-20261024.log`).
Fresh original crate347/two dormant contents refusals and actual Doctor
animation70/six refusal-prefix regressions pass after this integration. The
coherent full desktop build/shared CTest gate passes182 executed/215 registered,
33 explicit owned-data skips, zero failures in61.24s. Strict ARM64/runtime CLI
syntax and Android native library compile/link also pass. No APK is produced.

This proves original pickup/equip in the staged same-root diagnostic, not
committed ownership, player Fire eligibility, attack notify/trace/damage,
complete Tick, replacement APK or on-device acceptance. All commercial files
are read as data only; no original EXE/DLL is loaded or executed.
