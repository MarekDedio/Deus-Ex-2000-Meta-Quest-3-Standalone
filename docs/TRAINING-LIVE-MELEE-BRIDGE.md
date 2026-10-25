# First Training: missing live melee bridge

Read-only audit, 2026-10-24. Scope is the complete first `00_Training.dx`,
specifically its collected knife/crowbar, genuine player and breakable crates.
This is an implementation boundary and next-step recommendation, not evidence
that Quest melee, a complete world Tick or player startup is finished. No
compiler input/build was changed or commercial DLL/EXE executed or loaded.

## Current production disconnect

Latest2026-10-25 LOS checkpoint: both real authored weapons now complete the
original notify/NormalFire/Idle cycle in the rollback-only genuine Login root.
This does not close the production disconnect below. The real starting JC is
about2434 horizontal UU from the lower crate;80UU melee cannot hit it there.
Pawn-only PickTarget531 is auto-aim; actual Pawn.TraceShot277 and subsequent
ProcessTraceHit already admit non-Pawn Actors. A faithful next hit requires
real movement/aim authority, not forcing a selected target or widening531.
See ORIGINAL-PAWN-LINE-OF-SIGHT.md for the cycle and rollback controls.

Latest2026-10-25 follow-on:531 now enters its actual list/team/scalar prefix;
both real weapon probes reach internal Pawn.LineOfSightTo(candidate,0,false)
with whole-root undo. Pure, mixed VM and real dormant first-map prefix/cycle
tests pass. This is still not live player publication, visibility, hit or crate
damage. See ORIGINAL-PAWN-PICK-TARGET.md; the original disconnect remains.

Latest diagnostic checkpoint,2026-10-24: genuine knife/crowbar pickup/equip,
selected player Fire, original Attack notify, GetAxes229 and Actor.Trace277
now run in one rollback-only root. Both weapons next refuse actual
PickTarget531 at PlayerPawn.AdjustAim180. Shared184 executed/219 registered,
35 owned skips,0 failures; strict ARM64/Android native link pass. Crowbar
repeat2 and partial-Trace14979 write-cut repeat2 verify full undo/all41 sources.
See TRAINING-WEAPON-FIRE.md and ORIGINAL-ACTOR-TRACE-WRAPPER.md. None of this
closes the live publication/input gap below or proves a successful crate hit.

- `native/quest_main.cpp:1334`: trigger input reads presentation equipment and
  an audited damage/range profile, then calls `FireTargetedActor` at6185. That
  function point-tests detached interactive targets and calls
  `DamagePortableRuntimeActor` at6222. It does not invoke original weapon Fire.
- `native/portable_unreal_runtime.cpp:15292`: that damage function admits only
  Engine.Pawn and changes presentation `health`/`active`; it does not dispatch
  crate TakeDamage. Widening this health shortcut to Decoration is not the
  required crate integration.
- `InteractPortableRuntimeActor` at15204 still handles Inventory by setting
  `active=false` and appending `persistentInventory`. That presentation pickup
  is not proof of genuine JC Inventory/Weapon ownership/equip state.
- Held preparation in `native/quest_held_item_assets.h:177` samples
  `OriginalStillPose` once. `UpdateHeldItemVisualSelection` in quest_main.cpp
  reconciles equipment/cache selection, not original attack animation time.
- Animation command handling exists at runtime.cpp3350..3403: real commands
  set journaled clocks/properties through `ApplyActorAnimationCommand` and
  `SynchronizeClock`. However, a repository-wide search finds **no production
  call** of `AdvanceMainAnimationBoundary`, `AdvanceActorAnimationClock` or
  `AdvanceBlendAnimationClock`: their only native call sites are inside
  `quest_actor_animation_clock.h` itself. Tests drive those helpers separately.
  Those older corrected-policy helpers are still not the live driver. The new
  original main-animation phase now has a private same-Execution runtime
  binding and rollback-only original Training probe, but no automatic world/
  held-weapon driver. See ORIGINAL-ACTOR-MAIN-ANIMATION.md for its exact scope.

The isolated original startup-damage/cohort path is a different, useful proof:
`portable_startup_damage.inl:125..138` authenticates original TakeDamage and
calls it with the genuine Login player's identity and complete five-argument
packet in the same Execution. It does not connect the Quest trigger/equipment
path to that player and must not be published as a partially successful startup.

## Actual original Fire and notify

Owned `DeusEx.u` SHA256:
`257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480`.
Our existing `script_bytecode_inspect.exe` reads its compiled bytecode only.
PCs below are normalized logical bytecode addresses, not file offsets.

`DeusEx.DeusExWeapon.Fire` has1053 logical bytes, flags0x2. Its hand-to-hand
branch PC379..535 clears bReadyToFire, enters NormalFire, sets bPointing,
calls the PlayerPawn owner's selected PlayFiring, then selected
PlaySelectiveFiring and PlayFiringSound. It does **not** call TraceFire there.
Original `PlaySelectiveFiring` (386 bytes, flags0x102) chooses Attack when
FRand<0.33, Attack2 when the same draw<0.66, otherwise Attack3. On its standalone
branch, nonautomatic weapons issue PlayAnim(selected, default rate,0.1).

The class defaults select `DeusExItems.CombatKnife` and `DeusExItems.Crowbar`
as PlayerViewMesh; their pickup meshes are different. Owned
`DeusExItems.u` SHA256:
`F7114C5228248FCBE9E89E9C6408ABCCE1E41566207A5ABB7C7E58CC4D9BEAAA`.
Every attack sequence below has exactly one **HandToHandAttack** notify at
normalized time **0.5**, raw Float bits **0x3F000000**:

| Mesh / original export | Sequence | Start / frames | Rate | Notify time file offset |
| --- | --- | --- | --- | --- |
| CombatKnife /338 | Attack | 4 /8 | 22 | 0035C8FC |
| CombatKnife /338 | Attack2 | 12 /12 | 22 | 0035C910 |
| CombatKnife /338 | Attack3 | 24 /8 | 20 | 0035C924 |
| Crowbar /345 | Attack | 6 /12 | 28 | 00387047 |
| Crowbar /345 | Attack2 | 6 /12 | 28 | 0038705B |
| Crowbar /345 | Attack3 | 18 /14 | 28 | 0038706F |

CombatKnifePickup export342 and CrowbarPickup export349 have only All/Still,
one frame each and no notifies. Advancing the pickup mesh cannot produce the
original attack notification. Genuine equipped weapon Mesh selection must
follow the real pickup/equip/SetHand path, not be assumed from PlayerViewMesh.

`DeusEx.DeusExWeapon.HandToHandAttack` is282 bytes, native0, flags0x102:
PC0..10 returns if bOwnerWillNotify. Its nonstandalone owner/Role/server gates
remain real. PC130..165 optionally calls a ScriptedPawn owner's SetAttackAngle.
PC166..185 tests bInstantHit then calls selected TraceFire(0.0), otherwise
ProjectileFire. The later thrown-weapon destroy/finish branch is also original
script, not a reason to retire knife/crowbar directly.

Selected TraceFire is1299 bytes, flags0x102. It uses the Pawn owner's
ViewRotation/GetAxes at149, selected ComputeProjectileStart at191/513, owner
AdjustAim at232 and owner TraceShot at696. Its hit calls selected ProcessTraceHit
at1086/1250. ProcessTraceHit is712 bytes, flags0x102: selected WeaponDamageType
at141, then eligible original Other.TakeDamage at282 or422, including
HitDamage*Mult, Pawn(Owner), actual hit position,1000*X momentum and DamageType.
It also reaches skill/augmentation and effects services. These reached services
must be implemented or explicitly refused, not bypassed by a fixed damage
number or detached sphere hit. VR controller aim still needs an explicit
genuine player/weapon input contract; this audit does not equate the existing
renderer pointer ray to original TraceShot.

## Retail notify ordering differs from the current pure helper

Owned Engine.dll hash and full Tick bounds are recorded in
[the original Tick audit](ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md). Static Tick
103A1D30..103A1D74 selects the nearest reached mesh notify **before** looking
for a handler. At103A1D8A it stores Actor.AnimFrame to that notify time and
updates remaining elapsed before103A1D98 calls UObject.FindFunction(FName,0).
IAT10599ED4 imports
`?FindFunction@UObject@@QAEPAVUFunction@@VFName@@H@Z`. A missing function loops
at103A1BF9 without ProcessEvent; a present function invokes source virtual+10
ProcessEvent with null parameter/result pointers at103A1DA6..103A1DAF, then
loops and rereads live state.

The older pure clock helper filters candidates with `hasNotifyFunction` before
committing a notify boundary, and documents other pinned-clock corrections.
It is therefore not already an exact retail Tick implementation. A genuine
wrapper needs an explicit retail arithmetic/order audit, callback-aware
fresh mesh/rate/state reads and the same original ProcessEvent gates. It must
commit clock/property changes through the existing journal, then synchronously
dispatch the selected original function in the **same Execution/root**, before
continuing remaining time. Calling public ExecutePortableActorEvent for each
boundary would construct another host/root rather than supply this contract.
The new retail phase implements this separate boundary/provider contract;
its runtime observation currently proves a dormant original Doctor lookup and
stores, not active weapon callbacks. NormalFire's Sleep/FinishAnim/state
continuation and selected AnimEnd remain
part of completing a usable original attack cycle, not merely the first hit.

## Prioritized implementation and acceptance

Immediate nonpublishing crate-hit test after the completed attack checkpoint:
extend only the explicit Fire diagnostic with supplied pose input. Use original
Actor.SetLocation267 (require true and observe the actual adjusted Location),
then selected Pawn.ClientSetRotation (sets ViewRotation before its body-only
pitch/roll cleanup and SetRotation299), then unchanged original Fire. Determine
a free approach from actual Model floor and genuine JC/crate collision bounds;
do not patch Location, ViewRotation, FrobTarget, Enemy or bProjTarget. Record
actual trace results/range and crate HitPoints before/after in bounded detached
observations. The crate identity can be a test assertion, never Fire/Trace input.
This can test real weapon-to-crate damage before the separate HUD1463 service
is implemented, while still continuing ordinary possession and undoing the
entire root at its genuine boundary. It is not live controller input or a
permission to publish an unpaired InvalidState player. ClientSetRotation is
original script flags0xC2/logical45; SetLocation267 and SetRotation299 are actual
native0x401/logical5 declarations with required Core Vector/Rotator inputs and
Bool0x580 results. Existing FarMove/Move adapters preserve real collision/hash/
zone/encroachment authority. A controller-hand trace origin is a distinct VR
contract; original instant-hit Fire begins at the genuine player's eye.

1. Complete genuine startup/player pairing and real Training pickup/equip
   ownership. Do not publish the diagnostic Login prefix or fabricate a JC,
   Inventory grant, Weapon pointer or successful unsupported startup.
2. Bridge trigger/release input to the genuine currently equipped weapon's
   selected Fire/state path. Keep the existing presentation path separate
   until that bridge succeeds; do not also apply its instant damage.
3. Connect the newly implemented original main-animation notify phase to real
   equip/state progression and reached native services. The original FinishAnim
   command261/poll385 and same-Execution state phase now pass focused native,
   VM and generated-production tests (ORIGINAL-FINISHANIM-LATENT.md). Actual
   actual-map pickup/equip now completes for both authored knife/crowbar in
   the rollback-only Login diagnostic: actual ownership/grid/AmmoNone, six
   animation/state slices, original Idle and final transition clear. See
   TRAINING-WEAPON-EQUIP.md; repeat2, final-transition write14929 cutoff2 and
   shared182/215(33 skips,0 failures) gates pass.
   Committed input and genuine player Fire eligibility remain unproven.
   One real attack must select one authored sequence,
   cross its0.5 boundary once, call original HandToHandAttack once, trace the
   actual target and reach original crate TakeDamage. An interrupt/replacement
   before the boundary must not leave a fabricated pending hit.
4. Advance all genuine crate/loot/fragment physics in native order, then publish
   detached completed actor/equipment/pose snapshots from the committed world
   revision. The current cohort diagnostic is physics-phase-only, not full Tick.

Acceptance witness: collect an authored Training crowbar, equip it through
real JC ownership, hit the actual lower breakable crate with repeated eligible
original attacks, capture each notify/trace/TakeDamage chain, retain all real
fragments and dropped Lockpick, verify the upper support/base response and
collect the genuine loot. Also test out-of-range/occluded/interrupted swings,
NormalFire completion/repeat input, same-root refusal rollback and committed
revision/render publication. This document records no such completed live or
headset test.

Reproduce the script audit without commercial code execution:

```powershell
& artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe 'D:\Steam\steamapps\common\Deus Ex' 'DeusEx.DeusExWeapon.Fire' 'DeusEx.DeusExWeapon.PlaySelectiveFiring' 'DeusEx.DeusExWeapon.HandToHandAttack' 'DeusEx.DeusExWeapon.TraceFire' 'DeusEx.DeusExWeapon.ProcessTraceHit' 'DeusEx.DeusExWeapon.NormalFire'
& objdump -d -Mintel --start-address=0x103A1D09 --stop-address=0x103A1DB7 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
```

Mesh evidence was independently read from immutable package bytes using the
same v68 table layout and lazy-array boundary checks as
`LoadPortableLodMesh` (package_tables.cpp1649..1781): after the empty tagged
property prefix and41-byte primitive header, validate vertices/triangles lazy
ends, then read sequence name/group, Int32 start/count, compact notify count,
Float time/compact function name and Float rate. The table records absolute
file offsets of those Float times, not inferred durations or source comments.
